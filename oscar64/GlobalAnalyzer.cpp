#include "GlobalAnalyzer.h"

GlobalAnalyzer::GlobalAnalyzer(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mCalledFunctions(nullptr), mCallingFunctions(nullptr), mVariableFunctions(nullptr), mFunctions(nullptr), mGlobalVariables(nullptr), mTopoFunctions(nullptr), mCompilerOptions(COPT_DEFAULT)
{

}

GlobalAnalyzer::~GlobalAnalyzer(void)
{

}

void GlobalAnalyzer::DumpCallGraph(void)
{
	printf("------------------------------\n");

	for (int i = 0; i < mFunctions.Size(); i++)
	{
		GrowingArray<Declaration*>	decs(nullptr);
		GrowingArray<int>			calls(0);

		Declaration* from = mFunctions[i];
		for (int j = 0; j < from->mCalled.Size(); j++)
		{
			Declaration* to = from->mCalled[j];

			int k = decs.IndexOf(to);
			if (k == -1)
			{
				decs.Push(to);
				calls.Push(1);
			}
			else
				calls[k]++;
		}

		if (decs.Size() > 0)
		{
			for (int j = 0; j < decs.Size(); j++)
			{
				if (decs[j]->mType == DT_CONST_FUNCTION)
					printf("CALL %s[%d, %08llx] -> %d -> %s[%d, %08llx]\n", from->mQualIdent->mString, from->mComplexity, from->mFlags, calls[j], decs[j]->mQualIdent->mString, decs[j]->mComplexity, decs[j]->mFlags);
				else
					printf("CALL %s[%d, %08llx] -> %d\n", from->mQualIdent->mString, from->mComplexity, from->mFlags, calls[j]);
			}
		}
		else
		{
			printf("LEAF %d -> %s[%d, %08llx]\n", from->mCallers.Size(), from->mQualIdent->mString, from->mComplexity, from->mFlags );
		}
	}

	for (int i = 0; i < mGlobalVariables.Size(); i++)
	{
		Declaration* var = mGlobalVariables[i];
		printf("VAR %s[%d, %08llx, %d]\n", var->mQualIdent->mString, var->mSize, var->mFlags, var->mUseCount);
	}
}

static int VarUseCountScale(Declaration* type)
{
	if (type->mType == DT_TYPE_BOOL || type->mType == DT_TYPE_INTEGER || type->mType == DT_TYPE_FLOAT || type->mType == DT_TYPE_ENUM)
		return 0x100 / type->mSize;
	else if (type->mType == DT_TYPE_POINTER)
		return 0x800;
	else if (type->mType == DT_TYPE_ARRAY)
	{
		if (type->mSize > 0)
			return VarUseCountScale(type->mBase) / type->mSize;
		else
			return 0;
	}
	else if (type->mSize == DT_TYPE_STRUCT)
	{
		int size = 0;
		Declaration* e = type->mParams;
		while (e)
		{
			int t = VarUseCountScale(e->mBase);
			if (t == 0)
				return 0;
			size += t;
			e = e->mNext;
		}
		return size / (type->mSize * type->mSize);
	}
	else
		return 0;
}

void GlobalAnalyzer::AutoZeroPage(LinkerSection* lszp, int zpsize)
{
	if (mCompilerOptions & COPT_OPTIMIZE_AUTO_ZEROPAGE)
	{
		GrowingArray<Declaration*>	vars(nullptr);

		for (int i = 0; i < mGlobalVariables.Size(); i++)
		{
			Declaration* var = mGlobalVariables[i];
			if (var->mFlags & DTF_ANALYZED)
			{
				if (var->mFlags & DTF_ZEROPAGE)
					zpsize -= var->mSize;
				else if (var->mValue)
					;
				else
				{
					var->mUseCount *= VarUseCountScale(var->mBase);
					if (var->mUseCount)
					{
						int j = 0;
						while (j < vars.Size() && vars[j]->mUseCount > var->mUseCount)
							j++;
						vars.Insert(j, var);
					}
				}
			}
		}

		int i = 0;
		while (i < vars.Size() && zpsize > 0)
		{
			if (vars[i]->mSize <= zpsize && !vars[i]->mLinkerObject)
			{
				vars[i]->mSection = lszp;
				vars[i]->mFlags |= DTF_ZEROPAGE;
				zpsize -= vars[i]->mSize;
			}
			i++;
		}
	}
}

void GlobalAnalyzer::TopoSort(Declaration* procDec)
{
	if (!(procDec->mFlags & DTF_FUNC_ANALYZING))
	{
		procDec->mFlags |= DTF_FUNC_ANALYZING;
		if (!mTopoFunctions.Contains(procDec))
		{
			for (int i = 0; i < procDec->mCalled.Size(); i++)
				TopoSort(procDec->mCalled[i]);
			mTopoFunctions.Push(procDec);
		}
		procDec->mFlags &= ~DTF_FUNC_ANALYZING;
	}
}

int GlobalAnalyzer::CallerInvokes(Declaration* called)
{
	int n = 0;
	for (int i = 0; i < called->mCallers.Size(); i++)
	{
		Declaration* f = called->mCallers[i];
		n += CallerInvokes(f, called);
	}
	return n;
}

int GlobalAnalyzer::CallerInvokes(Declaration* caller, Declaration* called)
{
	int n = 1;
	if (caller->mType == DT_CONST_FUNCTION && (caller->mFlags & (DTF_INLINE | DTF_FORCE_INLINE | DTF_REQUEST_INLINE)) && !(caller->mFlags & DTF_PREVENT_INLINE) && !(caller->mFlags & DTF_FUNC_RECURSIVE) && !(caller->mFlags & DTF_FUNC_VARIABLE) && !(caller->mFlags & DTF_EXPORT))
		n = CallerInvokes(caller);
	return n > 1 ? n : 1;
}

void GlobalAnalyzer::AutoInline(void)
{
	for (int i = 0; i < mFunctions.Size(); i++)
		TopoSort(mFunctions[i]);
	
	bool	changed = false;
	do
	{
		changed = false;

		// Reverse order, to check inline from bottom to top of call graph
		for (int i = 0; i< mTopoFunctions.Size(); i++)
		{
			Declaration* f = mTopoFunctions[i];

			if (f->mType == DT_CONST_FUNCTION &&
				!(f->mFlags & DTF_INLINE) && 
				!(f->mFlags & DTF_EXPORT) && 
				!(f->mFlags & DTF_PREVENT_INLINE) &&
				!(f->mBase->mFlags & DTF_VARIADIC) &&
				!(f->mFlags & DTF_FUNC_VARIABLE) && 
				!((f->mFlags & DTF_FUNC_ASSEMBLER) && !(f->mFlags & DTF_REQUEST_INLINE)) && 
				!(f->mFlags & DTF_INTRINSIC) && 
				!(f->mFlags & DTF_FUNC_RECURSIVE) &&
				!(f->mFlags & DTF_FUNC_NO_RETURN))
			{
				int		nparams = 0;
				Declaration* dec = f->mBase->mParams;
				while (dec)
				{
					nparams += dec->mSize;
					dec = dec->mNext;
				}

				int invokes = CallerInvokes(f);
				int	cost = (f->mComplexity - 20 * nparams - 10);

//				printf("CHECK INLINING %s (%d) %d * (%d - 1)\n", f->mIdent->mString, f->mComplexity, cost, invokes);

				bool	doinline = false;
				if ((f->mCompilerOptions & COPT_OPTIMIZE_INLINE) && (f->mFlags & DTF_REQUEST_INLINE) || (f->mFlags & DTF_FORCE_INLINE))
					doinline = true;
				if (f->mLocalSize < 100)
				{
					if ((f->mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE) && ((cost - 20) * (invokes - 1) <= 20))
					{
						if (f->mCompilerOptions & COPT_OPTIMIZE_CODE_SIZE)
						{
							if (invokes == 1 && f->mSection == f->mCallers[0]->mSection || cost < 0)
								doinline = true;
						}
						else if (invokes == 1 && f->mComplexity > 100)
						{
//							printf("CHECK INLINING2 %s <- %s %d\n", f->mIdent->mString, f->mCallers[0]->mIdent->mString, f->mCallers[0]->mCalled.Size());
							if (cost < 0 || f->mCallers[0]->mComplexity + cost  < 1000 || f->mCallers[0]->mCalled.Size() == 1)
								doinline = true;
						}
						else 
							doinline = true;
					}
					if ((f->mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE_ALL) && (cost * (invokes - 1) <= 10000))
						doinline = true;
				}

				if (doinline)
				{
//					printf("INLINING %s %d * (%d - 1)\n", f->mIdent->mString, cost, invokes);

					f->mFlags |= DTF_INLINE;
					for (int j = 0; j < f->mCallers.Size(); j++)
					{
						Declaration* cf = f->mCallers[j];

						int sk = 0, dk = 0;
						while (sk < cf->mCalled.Size())
						{
							if (cf->mCalled[sk] == f && ((cf->mFlags & DTF_NATIVE) || !(f->mFlags & DTF_NATIVE)))
							{
								cf->mComplexity += cost;
								for (int m = 0; m < f->mCalled.Size(); m++)
								{
									cf->mCalled.Push(f->mCalled[m]);
									f->mCalled[m]->mCallers.Push(cf);
								}
							}
							else
								cf->mCalled[dk++] = cf->mCalled[sk];
							sk++;
						}
						cf->mCalled.SetSize(dk);
					}

					changed = true;
				}
			}
		}

	} while (changed);

	for (int i = 0; i < mFunctions.Size(); i++)
	{
		CheckFastcall(mFunctions[i], true);
	}

	for (int i = 0; i < mFunctions.Size(); i++)
	{
		Declaration* dec = mFunctions[i];
		Declaration* pdec = dec->mBase->mParams;
		while (pdec)
		{
			if (pdec->mFlags & DTF_FPARAM_CONST)
			{
				pdec->mFlags |= DTF_FPARAM_UNUSED;

				pdec->mVarIndex = dec->mNumVars++;

				Expression* aexp = new Expression(pdec->mLocation, EX_INITIALIZATION);
				Expression* pexp = new Expression(pdec->mLocation, EX_VARIABLE);
				Expression* lexp = new Expression(dec->mLocation, EX_SEQUENCE);

				pexp->mDecType = pdec->mBase;
				pexp->mDecValue = pdec;

				aexp->mDecType = pdec->mBase;
				aexp->mToken = TK_ASSIGN;
				aexp->mLeft = pexp;
				aexp->mRight = pdec->mValue;

				lexp->mLeft = aexp;
				lexp->mRight = dec->mValue;
				dec->mValue = lexp;
			}

			pdec = pdec->mNext;
		}
	}

}

bool GlobalAnalyzer::MarkCycle(Declaration* rootDec, Declaration* procDec)
{
	if (rootDec == procDec)
		return true;

	if (!(procDec->mFlags & DTF_FUNC_ANALYZING))
	{
		procDec->mFlags |= DTF_FUNC_ANALYZING;

		bool	cycle = false;
		for (int i = 0; i < procDec->mCalled.Size(); i++)
		{
			if (MarkCycle(rootDec, procDec->mCalled[i]))
				cycle = true;
		}
		if (cycle)
			procDec->mFlags |= DTF_FUNC_RECURSIVE;

		procDec->mFlags &= ~DTF_FUNC_ANALYZING;

		return cycle;
	}

	return false;
}

void GlobalAnalyzer::MarkRecursions(void)
{
	for (int i = 0; i < mFunctions.Size(); i++)
	{
		Declaration* cf = mFunctions[i];
		for (int j = 0; j < cf->mCalled.Size(); j++)
		{
			if (MarkCycle(cf, cf->mCalled[j]))
				cf->mFlags |= DTF_FUNC_RECURSIVE;
		}
	}
}

void GlobalAnalyzer::CheckFastcall(Declaration* procDec, bool head)
{
	if (!(procDec->mBase->mFlags & DTF_FASTCALL) && (procDec->mType == DT_CONST_FUNCTION) && !(procDec->mFlags & DTF_FUNC_ANALYZING))
	{
//		printf("CheckFastcall1 %s %08llx %08llx\n", procDec->mQualIdent->mString, procDec->mFlags, procDec->mBase->mFlags);

		procDec->mFlags |= DTF_FUNC_ANALYZING;
		int	nbase = 0;
		for (int i = 0; i < procDec->mCalled.Size(); i++)
		{
			Declaration* cf = procDec->mCalled[i];

			if (cf->mType == DT_TYPE_FUNCTION)
			{
				for (int i = 0; i < mVariableFunctions.Size(); i++)
				{
					Declaration* vf = mVariableFunctions[i];

					if (vf->mBase->IsSame(cf))
					{
						CheckFastcall(vf, false);

						int n = vf->mBase->mFastCallSize;
						if (n > nbase)
							nbase = n;
					}

				}
				//				procDec->mFlags |= DTF_DYNSTACK;
			}
			else
				CheckFastcall(cf, false);

			if (cf->mFlags & DTF_DYNSTACK)
				procDec->mFlags |= DTF_DYNSTACK;

			//			if (!(cf->mBase->mFlags & DTF_FASTCALL))
			//				procDec->mBase->mFlags |= DTF_STACKCALL;

			cf = cf->mBase;
			int n = cf->mFastCallSize;
			if (n > nbase)
				nbase = n;
		}

//		printf("CheckFastcall2 %s %08llx %08llx\n", procDec->mQualIdent->mString, procDec->mFlags, procDec->mBase->mFlags);

		if (procDec->mValue && procDec->mValue->mType == EX_DISPATCH)
		{
			Declaration* maxf = nullptr, * reff = nullptr;

			bool	stackCall = false;

			for (int i = 0; i < procDec->mCalled.Size(); i++)
			{
				Declaration* cf = procDec->mCalled[i];

				if (cf->mBase->mFlags & DTF_STACKCALL)
				{
					stackCall = true;
					reff = cf;
				}

				if (!maxf)
					maxf = cf;
				else if (cf->mBase->mFastCallBase > maxf->mBase->mFastCallBase)
					maxf = cf;
			}

			if (!reff)
				reff = maxf;

			for (int i = 0; i < procDec->mCalled.Size(); i++)
			{
				Declaration* cf = procDec->mCalled[i];

				if (stackCall)
				{
					cf->mBase->mFlags &= ~DTF_FASTCALL;
					cf->mBase->mFlags |= DTF_STACKCALL;
				}

				if (cf != reff)
				{
					Declaration* fp = cf->mBase->mParams, * mp = reff->mBase->mParams;
					while (fp)
					{
						fp->mVarIndex = mp->mVarIndex;
						fp = fp->mNext;
						mp = mp->mNext;
					}

					assert(!mp);
					cf->mBase->mFastCallSize = cf->mFastCallSize = maxf->mBase->mFastCallSize;
				}

				if (cf != maxf)
					cf->mBase->mFastCallBase = cf->mFastCallBase = maxf->mBase->mFastCallBase;
			}

			procDec->mFastCallBase = procDec->mBase->mFastCallBase;
			procDec->mFastCallSize = procDec->mBase->mFastCallSize;
			procDec->mFlags &= ~DTF_FUNC_ANALYZING;

			return;
		}

		procDec->mFastCallBase = nbase;
		procDec->mFastCallSize = nbase;
		procDec->mBase->mFastCallBase = nbase;
		procDec->mBase->mFastCallSize = nbase;

		procDec->mFlags &= ~DTF_FUNC_ANALYZING;

//		printf("CheckFastcall3 %s %08llx %08llx\n", procDec->mQualIdent->mString, procDec->mFlags, procDec->mBase->mFlags);

		if (procDec->mBase->mFlags & DTF_STACKCALL)
		{
			procDec->mBase->mFlags |= DTF_STACKCALL;
		}
		else if (procDec->mFlags & DTF_FUNC_RECURSIVE)
		{
//			if (head)
				procDec->mBase->mFlags |= DTF_STACKCALL;
		}
		else if (!(procDec->mBase->mFlags & DTF_VARIADIC) && !(procDec->mFlags & DTF_FUNC_VARIABLE) && !(procDec->mFlags & DTF_DYNSTACK))
		{
			int		nparams = nbase, npalign = 0;
			int		numfpzero = BC_REG_FPARAMS_END - BC_REG_FPARAMS;
			int		fplimit = numfpzero;

			if ((procDec->mFlags & DTF_NATIVE) || (mCompilerOptions & COPT_NATIVE))
			{
				if (!(procDec->mFlags & DTF_FUNC_INTRCALLED))
					fplimit += 256;
			}

			if (procDec->mBase->mBase->mType == DT_TYPE_STRUCT)
			{
				if (nbase < numfpzero && nbase + 2 > numfpzero)
					nbase = numfpzero;
				nparams += 2;
			}

			int		cnparams = nparams;
			Declaration* dec = procDec->mBase->mParams;
			while (dec)
			{
				if (!(dec->mFlags & DTF_FPARAM_UNUSED))
				{
					// Check for parameter crossing boundary
					if (cnparams < numfpzero && cnparams + dec->mBase->mSize > numfpzero)
					{
						npalign = numfpzero - nparams;
						cnparams += npalign;
					}
					cnparams += dec->mBase->mSize;
				}
				dec = dec->mNext;
			}

			if (cnparams <= fplimit)
			{
				npalign = 0;
				dec = procDec->mBase->mParams;
				while (dec)
				{
					if (dec->mFlags & DTF_FPARAM_UNUSED)
					{

					}
					else if (dec->mForwardParam && (dec->mForwardCall->mBase->mFlags & DTF_FASTCALL) && !(dec->mForwardCall->mFlags & DTF_INLINE))
					{
						dec->mVarIndex = dec->mForwardParam->mVarIndex;
					}
					else
					{
						dec->mForwardParam = nullptr;
						// Check for parameter crossing boundary
						if (nparams < numfpzero && nparams + dec->mBase->mSize > numfpzero)
						{
							npalign = numfpzero - nparams;
							nparams += npalign;
						}
						dec->mVarIndex = nparams;
						nparams += dec->mBase->mSize;
					}
					dec = dec->mNext;
				}

				procDec->mFastCallBase = nbase;
				procDec->mFastCallSize = nparams;
				procDec->mBase->mFastCallBase = nbase;
				procDec->mBase->mFastCallSize = nparams;

				procDec->mBase->mFlags |= DTF_FASTCALL;
//				printf("FASTCALL %s\n", procDec->mQualIdent->mString);
			}
			else
			{
//				printf("STACKCALL %s, %d %d\n", procDec->mQualIdent->mString, cnparams, fplimit);
				procDec->mBase->mFlags |= DTF_STACKCALL;
			}
		}
		else
		{
//			printf("STACKCALL %s, F%d\n", procDec->mQualIdent->mString, !!(procDec->mFlags& DTF_FUNC_VARIABLE));
			procDec->mBase->mFlags |= DTF_STACKCALL;
		}
	}
}

void GlobalAnalyzer::CheckInterrupt(void)
{
	bool	changed = false;
	do
	{
		changed = false;

		for (int i = 0; i < mFunctions.Size(); i++)
		{
			Declaration* f = mFunctions[i];
			if (f->mFlags & DTF_FUNC_INTRCALLED)
			{
				for (int j = 0; j < f->mCalled.Size(); j++)
				{
					Declaration* cf = f->mCalled[j];
					if (!(cf->mFlags & DTF_FUNC_INTRCALLED))
					{
						cf->mFlags |= DTF_FUNC_INTRCALLED;
						changed = true;
					}
				}
			}
		}

	} while (changed);
}

bool GlobalAnalyzer::IsStackParam(const Declaration* pdec) const
{
	if (pdec->mType == DT_TYPE_STRUCT)
	{
		if (pdec->mSize > 4)
			return true;
		if (pdec->mCopyConstructor)
		{
			if (!((mCompilerOptions & COPT_OPTIMIZE_INLINE) && (pdec->mCopyConstructor->mFlags & DTF_REQUEST_INLINE)))
				return true;
		}
		if (pdec->mDestructor)
		{
			if (!((mCompilerOptions & COPT_OPTIMIZE_INLINE) && (pdec->mDestructor->mFlags & DTF_REQUEST_INLINE)))
				return true;
		}
	}
	
	return false;
}

void GlobalAnalyzer::UndoParamReference(Expression* exp, Declaration * param)
{
	if (exp)
	{
		if (exp->mType == EX_VARIABLE)
		{
			if (exp->mDecValue == param)
				exp->mDecType = param->mBase;
		}

		UndoParamReference(exp->mLeft, param);
		UndoParamReference(exp->mRight, param);
	}
}

void GlobalAnalyzer::AnalyzeProcedure(Expression* cexp, Expression* exp, Declaration* dec)
{
	dec->mUseCount++;

	if (dec->mFlags & DTF_FUNC_ANALYZING)
	{
		dec->mFlags |= DTF_FUNC_RECURSIVE;
		dec->mFlags &= ~DTF_FUNC_CONSTEXPR;
	}

	if (!(dec->mFlags & DTF_ANALYZED))
	{
		dec->mFlags |= DTF_FUNC_ANALYZING;

		if (dec->mFlags & DTF_DEPRECATED)
		{
			mErrors->Error(dec->mLocation, EWARN_DEFAULT_COPY_DEPRECATED, "Using deprecated function", dec->mQualIdent->mString);
			if (cexp)
				mErrors->Error(cexp->mLocation, EINFO_CALLED_FROM, "Called from here");
		}

		mFunctions.Push(dec);

		Declaration* pdec = dec->mBase->mParams;
		while (pdec)
		{
			if (IsStackParam(pdec->mBase))
				dec->mBase->mFlags |= DTF_STACKCALL;
			pdec = pdec->mNext;
		}

		dec->mFlags |= DTF_ANALYZED;
		dec->mFlags |= DTF_FUNC_INTRSAVE;
		if (dec->mBase->mBase && dec->mBase->mBase->mType != DT_TYPE_VOID)
			dec->mFlags |= DTF_FUNC_NO_RETURN;

		if (dec->mFlags & DTF_INTERRUPT)
			dec->mFlags |= DTF_FUNC_INTRCALLED;

		if ((dec->mFlags & DTF_INTRINSIC) && !dec->mValue)
			dec->mFlags |= DTF_FUNC_CONSTEXPR;
		else if (dec->mFlags & DTF_DEFINED)
		{
			if (mCompilerOptions & COPT_OPTIMIZE_CONST_EXPRESSIONS)
				dec->mFlags |= DTF_FUNC_CONSTEXPR;
			dec->mFlags |= DTF_FUNC_PURE;
			Analyze(exp, dec, 0);

			Declaration* pdec = dec->mBase->mParams;
			int vi = 0;
			while (pdec)
			{
				pdec->mVarIndex += vi;
				if (pdec->mBase->mType == DT_TYPE_REFERENCE && pdec->mBase->mBase->IsSimpleType() && !(pdec->mFlags & DTF_VAR_ADDRESS) && (pdec->mBase->mBase->mFlags & DTF_CONST))
				{
					pdec->mBase = pdec->mBase->mBase;
					pdec->mSize = pdec->mBase->mSize;
					vi += pdec->mSize - 2;

					UndoParamReference(exp, pdec);
				}
				pdec = pdec->mNext;
			}
		}
		else
		{
			mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mQualIdent);
			if (cexp)
				mErrors->Error(cexp->mLocation, EINFO_CALLED_FROM, "Called from here");

		}

		dec->mFlags &= ~DTF_FUNC_ANALYZING;
	}
}


void GlobalAnalyzer::AnalyzeAssembler(Expression* exp, Declaration* procDec)
{
	while (exp)
	{
		if (procDec)
			procDec->mComplexity += 2;

		if (exp->mLeft && exp->mLeft->mDecValue)
		{
			Declaration* adec = exp->mLeft->mDecValue;
			if (adec->mType == DT_LABEL_REF)
			{

			}
			else if (adec->mType == DT_VARIABLE_REF)
			{
				if (adec->mBase->mFlags & DTF_GLOBAL)
				{
					AnalyzeGlobalVariable(adec->mBase);
					adec->mBase->mFlags |= DTF_VAR_ALIASING;
				}
			}
			else if (adec->mType == DT_LABEL)
			{

			}
			else if (adec->mType == DT_VARIABLE)
			{
				if (adec->mFlags & DTF_GLOBAL)
				{
					AnalyzeGlobalVariable(adec);
					adec->mFlags |= DTF_VAR_ALIASING;
				}
			}
			else if (adec->mType == DT_FUNCTION_REF)
			{
				AnalyzeProcedure(exp, adec->mBase->mValue, adec->mBase);
				RegisterProc(adec->mBase);
			}
			else if (adec->mType == DT_CONST_FUNCTION)
			{
				AnalyzeProcedure(exp, adec->mValue, adec);
				RegisterCall(procDec, adec);
			}
		}

		exp = exp->mRight;
	}
}

void GlobalAnalyzer::AnalyzeGlobalVariable(Declaration* dec)
{
	while (dec->mType == DT_VARIABLE_REF)
		dec = dec->mBase;

	dec->mUseCount++;

	if (!(dec->mFlags & DTF_ANALYZED))
	{
		dec->mFlags |= DTF_ANALYZED;

		mGlobalVariables.Push(dec);

		if (dec->mValue)
		{
			Analyze(dec->mValue, dec, 0);
		}
	}
}

void GlobalAnalyzer::AnalyzeInit(Declaration* mdec)
{
	while (mdec)
	{
		if (mdec->mValue)
			RegisterProc(Analyze(mdec->mValue, mdec, 0));
		else if (mdec->mParams)
			AnalyzeInit(mdec->mParams);
		mdec = mdec->mNext;
	}
}

Declaration * GlobalAnalyzer::Analyze(Expression* exp, Declaration* procDec, uint32 flags)
{
	Declaration* ldec, * rdec;

	exp->mFlags = flags;

	switch (exp->mType)
	{
	case EX_ERROR:
	case EX_VOID:
		break;
	case EX_CONSTANT:
		if (mCompilerOptions & COPT_DEBUGINFO)
			exp->mDecValue->mReferences.Push(exp);

		if (exp->mDecValue->mType == DT_CONST_FUNCTION)
			AnalyzeProcedure(exp, exp->mDecValue->mValue, exp->mDecValue);
		else if (exp->mDecValue->mType == DT_CONST_STRUCT)
		{
			AnalyzeInit(exp->mDecValue->mParams);
		}
		else if (exp->mDecValue->mType == DT_CONST_POINTER)
		{
			ldec = Analyze(exp->mDecValue->mValue, procDec, ANAFL_LHS | ANAFL_ALIAS);
			if (ldec->mType == DT_VARIABLE)
				ldec->mFlags |= DTF_VAR_ALIASING;
			RegisterProc(ldec);
		}
		else if (exp->mDecValue->mType == DT_CONST_ADDRESS)
		{
			procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
		}
		else if (exp->mDecValue->mType == DT_CONST_ASSEMBLER)
		{
			AnalyzeAssembler(exp->mDecValue->mValue, procDec);
		}

		return exp->mDecValue;
	case EX_VARIABLE:
		if (exp->mDecType->IsSimpleType())
			procDec->mComplexity += 5 * exp->mDecType->mSize;
		else
			procDec->mComplexity += 10;

		if (mCompilerOptions & COPT_DEBUGINFO)
			exp->mDecValue->mReferences.Push(exp);

		if (flags & ANAFL_ALIAS)
		{
			Declaration* dec = exp->mDecValue;
			while (dec->mType == DT_VARIABLE_REF)
				dec = dec->mBase;
			dec->mFlags |= DTF_VAR_ALIASING;
		}

		if ((exp->mDecValue->mFlags & DTF_STATIC) || (exp->mDecValue->mFlags & DTF_GLOBAL))
		{
			Declaration* type = exp->mDecValue->mBase;
			while (type->mType == DT_TYPE_ARRAY)
				type = type->mBase;

			if (!(type->mFlags & DTF_CONST))
				procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

			if (flags & ANAFL_LHS)
				procDec->mFlags &= ~DTF_FUNC_PURE;

			AnalyzeGlobalVariable(exp->mDecValue);
		}
		else
		{
			if (flags & ANAFL_LHS)
				exp->mDecValue->mFlags |= DTF_VAR_ADDRESS;

			if (!(exp->mDecValue->mFlags & DTF_ANALYZED))
			{
				procDec->mLocalSize += exp->mDecValue->mSize;
				exp->mDecValue->mFlags |= DTF_ANALYZED;
			}
		}
		return exp->mDecValue;
	case EX_INITIALIZATION:
	case EX_ASSIGNMENT:
		procDec->mComplexity += 5 * exp->mLeft->mDecType->mSize;

		ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, ANAFL_RHS);
		if (exp->mLeft->mType == EX_VARIABLE && exp->mRight->mType == EX_CALL && exp->mLeft->mDecType->mType == DT_TYPE_STRUCT)
			exp->mLeft->mDecValue->mFlags |= DTF_VAR_ALIASING;
		RegisterProc(rdec);
		return ldec;

	case EX_BINARY:
		procDec->mComplexity += 10 * exp->mDecType->mSize;

		ldec = Analyze(exp->mLeft, procDec, flags & ~ANAFL_ALIAS);
		rdec = Analyze(exp->mRight, procDec, flags & ~ANAFL_ALIAS);
		return ldec;

	case EX_RELATIONAL:
		procDec->mComplexity += 10 * exp->mLeft->mDecType->mSize;

		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, ANAFL_RHS);
		return TheBoolTypeDeclaration;

	case EX_PREINCDEC:
		procDec->mComplexity += 10 * exp->mLeft->mDecType->mSize;

		return Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS);
	case EX_PREFIX:
		if (exp->mToken == TK_BINARY_AND)
		{
			ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_ALIAS);
			if (ldec->mType == DT_VARIABLE)
				ldec->mFlags |= DTF_VAR_ALIASING;
		} 
		else if (exp->mToken == TK_MUL)
		{
			ldec = Analyze(exp->mLeft, procDec, 0);
			procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
			if (flags & ANAFL_LHS)
				procDec->mFlags &= ~DTF_FUNC_PURE;

			return exp->mDecType;
		}
		else if (exp->mToken == TK_BANKOF)
		{
			return TheUnsignedCharTypeDeclaration;
		}
		else if (exp->mToken == TK_SIZEOF)
		{
			return TheUnsignedIntTypeDeclaration;
		}
		else
		{
			procDec->mComplexity += 10 * exp->mLeft->mDecType->mSize;
			return Analyze(exp->mLeft, procDec, 0);
		}
		break;
	case EX_POSTFIX:
		procDec->mComplexity += 10 * exp->mLeft->mDecType->mSize;
		break;
	case EX_POSTINCDEC:
		procDec->mComplexity += 10 * exp->mLeft->mDecType->mSize;

		return Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS);
	case EX_INDEX:
		procDec->mComplexity += 10 * exp->mRight->mDecType->mSize;

		ldec = Analyze(exp->mLeft, procDec, flags & ~ANAFL_ALIAS);
		if (ldec->mType == DT_VARIABLE || ldec->mType == DT_ARGUMENT)
		{
			ldec = ldec->mBase;
			if (ldec->mType == DT_TYPE_POINTER)
			{
				if (flags & ANAFL_LHS)
					procDec->mFlags &= ~DTF_FUNC_PURE;
				procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
			}
		}
		rdec = Analyze(exp->mRight, procDec, 0);
		if (ldec->mBase)
			return ldec->mBase;
		break;
	case EX_QUALIFY:
		Analyze(exp->mLeft, procDec, flags);
		return exp->mDecValue->mBase;
	case EX_DISPATCH:
		procDec->mFlags |= DTF_PREVENT_INLINE;
		Analyze(exp->mLeft, procDec, flags & ~ANAFL_ALIAS);
//		RegisterCall(procDec, exp->mLeft->mDecType);
		break;
	case EX_VCALL:
		exp->mType = EX_CALL;
		exp->mLeft->mDecValue = exp->mLeft->mDecValue->mVTable;
		// intentional fall through
	case EX_CALL:
	case EX_INLINE:
		procDec->mComplexity += 10;

		ldec = Analyze(exp->mLeft, procDec, 0);
		if ((ldec->mFlags & DTF_INTRINSIC) && !ldec->mValue)
		{

		}
		else
		{
			if (exp->mType == EX_INLINE)
			{
				for (int i = 0; i < ldec->mCalled.Size(); i++)
					RegisterCall(procDec, ldec->mCalled[i]);
			}
			else
				RegisterCall(procDec, ldec);
			if (!(GetProcFlags(ldec) & (DTF_FUNC_INTRSAVE | DTF_INTERRUPT)))
			{
				procDec->mFlags &= ~DTF_FUNC_INTRSAVE;
				if (procDec->mFlags & DTF_INTERRUPT)
					mErrors->Error(exp->mLocation, EWARN_NOT_INTERRUPT_SAFE, "Calling non interrupt safe function", ldec->mQualIdent);
			}
			if (!(GetProcFlags(ldec) & DTF_FUNC_PURE))
				procDec->mFlags &= ~DTF_FUNC_PURE;
		}

		if (exp->mRight)
		{
			// Check for struct to struct forwarding
			Expression* rex = exp->mRight;
			Declaration* pdec = ldec->mBase->mParams;
			while (rex)
			{
				Expression* pex = rex->mType == EX_LIST ? rex->mLeft : rex;

				procDec->mComplexity += 5 * pex->mDecType->mSize;

				if (pdec && !(ldec->mBase->mFlags & DTF_VARIADIC) && !(ldec->mFlags & (DTF_INTRINSIC | DTF_FUNC_ASSEMBLER)))
				{
#if 1
					if (mCompilerOptions & COPT_OPTIMIZE_CONST_PARAMS)
					{
						if (!(pdec->mFlags & DTF_FPARAM_NOCONST))
						{
							if (pex->mType == EX_CONSTANT)
							{
								if (pdec->mFlags & DTF_FPARAM_CONST)
								{
									if (!pex->mDecValue->IsSameValue(pdec->mValue->mDecValue))
									{
										pdec->mFlags |= DTF_FPARAM_NOCONST;
										pdec->mFlags &= ~DTF_FPARAM_CONST;
									}
								}
								else
								{
									pdec->mValue = pex;
									pdec->mFlags |= DTF_FPARAM_CONST;
								}
							}
							else
							{
								pdec->mFlags |= DTF_FPARAM_NOCONST;
								pdec->mFlags &= ~DTF_FPARAM_CONST;
							}
						}
					}
					else
					{
						pdec->mFlags |= DTF_FPARAM_NOCONST;
						pdec->mFlags &= ~DTF_FPARAM_CONST;
					}
#endif
				}

				if (pdec && pdec->mBase->mType == DT_TYPE_STRUCT && pdec->mBase->mCopyConstructor)
				{
					if (pdec->mBase->mMoveConstructor)
					{
						AnalyzeProcedure(exp, pdec->mBase->mMoveConstructor->mValue, pdec->mBase->mMoveConstructor);
						RegisterCall(procDec, pdec->mBase->mMoveConstructor);
					}
					AnalyzeProcedure(exp, pdec->mBase->mCopyConstructor->mValue, pdec->mBase->mCopyConstructor);
					RegisterCall(procDec, pdec->mBase->mCopyConstructor);
				}
				
				if (pex->mType == EX_CALL && IsStackParam(pex->mDecType) && !(pdec && (pdec->mBase->mType == DT_TYPE_REFERENCE || pdec->mBase->mType == DT_TYPE_RVALUEREF)))
					ldec->mBase->mFlags |= DTF_STACKCALL;

				RegisterProc(Analyze(pex, procDec, (pdec && pdec->mBase->IsReference()) ? ANAFL_LHS : 0));

				if (pdec)
					pdec = pdec->mNext;

				if (rex->mType == EX_LIST)
					rex = rex->mRight;
				else
					rex = nullptr;
			}
		}
		break;
	case EX_LIST:
	case EX_COMMA:
		RegisterProc(Analyze(exp->mLeft, procDec, 0));
		return Analyze(exp->mRight, procDec, 0);
	case EX_RETURN:
		if (exp->mLeft)
		{
			RegisterProc(Analyze(exp->mLeft, procDec, procDec->mBase->mBase->IsReference() ? ANAFL_LHS : 0));
			if (procDec->mBase->mBase && procDec->mBase->mBase->mType == DT_TYPE_STRUCT && procDec->mBase->mBase->mCopyConstructor)
			{
				if (procDec->mBase->mBase->mMoveConstructor)
				{
					AnalyzeProcedure(exp, procDec->mBase->mBase->mMoveConstructor->mValue, procDec->mBase->mBase->mMoveConstructor);
					RegisterCall(procDec, procDec->mBase->mBase->mMoveConstructor);
				}
				AnalyzeProcedure(exp, procDec->mBase->mBase->mCopyConstructor->mValue, procDec->mBase->mBase->mCopyConstructor);
				RegisterCall(procDec, procDec->mBase->mBase->mCopyConstructor);
			}

			procDec->mFlags &= ~DTF_FUNC_NO_RETURN;
		}
		break;
	case EX_SEQUENCE:
		do
		{
			if (exp->mType == EX_SEQUENCE)
			{
				if (exp->mLeft)
					ldec = Analyze(exp->mLeft, procDec, 0);
				exp = exp->mRight;
			}
			else
				return Analyze(exp, procDec, 0);

		} while (exp);
		break;

	case EX_SCOPE:
		Analyze(exp->mLeft, procDec, 0);
		break;

	case EX_CONSTRUCT:
		if (exp->mLeft->mLeft)
			Analyze(exp->mLeft->mLeft, procDec, 0);
		if (exp->mLeft->mRight)
			Analyze(exp->mLeft->mRight, procDec, 0);
		if (exp->mRight)
			return Analyze(exp->mRight, procDec, 0);
		break;

	case EX_CLEANUP:
		Analyze(exp->mRight, procDec, 0);
		return Analyze(exp->mLeft, procDec, flags & ~ANAFL_ALIAS);
		
	case EX_WHILE:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		procDec->mComplexity += 20;

		ldec = Analyze(exp->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_IF:
		procDec->mComplexity += 20;

		ldec = Analyze(exp->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight->mLeft, procDec, 0);
		if (exp->mRight->mRight)
			rdec = Analyze(exp->mRight->mRight, procDec, 0);
		break;
	case EX_ELSE:
		break;
	case EX_FOR:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		procDec->mComplexity += 30;

		if (exp->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mRight, procDec, 0);
		if (exp->mLeft->mLeft->mLeft)
			ldec = Analyze(exp->mLeft->mLeft->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight, procDec, 0);
		if (exp->mLeft->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mLeft->mRight, procDec, 0);
		break;
	case EX_FORBODY:
		ldec = Analyze(exp->mLeft, procDec, 0);
		if (exp->mRight)
			Analyze(exp->mRight, procDec, 0);
		break;
	case EX_DO:
		procDec->mComplexity += 20;

		ldec = Analyze(exp->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_BREAK:
	case EX_CONTINUE:
	case EX_ASSUME:
		break;
	case EX_TYPE:
		break;
	case EX_TYPECAST:
		return Analyze(exp->mLeft, procDec, 0);
		break;
	case EX_LOGICAL_AND:
		ldec = Analyze(exp->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_LOGICAL_OR:
		ldec = Analyze(exp->mLeft, procDec, 0);
		rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_LOGICAL_NOT:
		ldec = Analyze(exp->mLeft, procDec, 0);
		break;
	case EX_ASSEMBLER:
		procDec->mFlags |= DTF_FUNC_ASSEMBLER;
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
		procDec->mFlags &= ~DTF_FUNC_PURE;
		AnalyzeAssembler(exp, procDec);
		break;
	case EX_UNDEFINED:
		break;
	case EX_SWITCH:
		ldec = Analyze(exp->mLeft, procDec, 0);
		exp = exp->mRight;
		while (exp)
		{
			procDec->mComplexity += 10;

			if (exp->mLeft->mRight)
				rdec = Analyze(exp->mLeft->mRight, procDec, 0);
			exp = exp->mRight;
		}
		break;
	case EX_CASE:
		break;
	case EX_DEFAULT:
		break;
	case EX_CONDITIONAL:
		procDec->mComplexity += exp->mDecType->mSize * 10;

		ldec = Analyze(exp->mLeft, procDec, 0);
		RegisterProc(Analyze(exp->mRight->mLeft, procDec, flags));
		RegisterProc(Analyze(exp->mRight->mRight, procDec, flags));
		break;
	}

	return TheVoidTypeDeclaration;
}

uint64 GlobalAnalyzer::GetProcFlags(Declaration* to) const
{
	if (to->mType == DT_CONST_FUNCTION)
		return to->mFlags;
	else if (to->mType == DT_TYPE_FUNCTION)
		return to->mFlags;
	else if (to->mType == DT_TYPE_POINTER && to->mBase->mType == DT_TYPE_FUNCTION)
		return GetProcFlags(to->mBase);
	else if (to->mType == DT_VARIABLE || to->mType == DT_ARGUMENT)
		return GetProcFlags(to->mBase);
	else
		return 0;
}

void GlobalAnalyzer::RegisterCall(Declaration* from, Declaration* to)
{
	if (from)
	{
		if (to->mType == DT_VARIABLE || to->mType == DT_ARGUMENT)
			to = to->mBase;

		if (to->mType == DT_CONST_FUNCTION)
		{
			if (to->mFlags & DTF_DYNSTACK)
				from->mFlags |= DTF_DYNSTACK;

			if (!(to->mFlags & DTF_FUNC_CONSTEXPR))
				from->mFlags &= ~DTF_FUNC_CONSTEXPR;

			if (to->mCallers.Size() == 0)
				mCalledFunctions.Push(to);
			to->mCallers.Push(from);
			if (from->mCalled.Size() == 0)
				mCallingFunctions.Push(from);
			from->mCalled.Push(to);
		}
		else if (to->mType == DT_TYPE_FUNCTION)
		{
			from->mFlags &= ~DTF_FUNC_CONSTEXPR;
			if (from->mCalled.Size() == 0)
				mCallingFunctions.Push(from);
			from->mCalled.Push(to);
		}
		else if (to->mType == DT_TYPE_POINTER && to->mBase->mType == DT_TYPE_FUNCTION)
		{
			from->mFlags &= ~DTF_FUNC_CONSTEXPR;
			if (from->mCalled.Size() == 0)
				mCallingFunctions.Push(from);
			from->mCalled.Push(to->mBase);
		}
	}
}

void GlobalAnalyzer::RegisterProc(Declaration* to)
{
	if (to->mType == DT_CONST_FUNCTION)
	{
#if 1
		if (to->mBase->mFlags & DTF_VIRTUAL)
		{

		}
		else 
#endif		
		if (!(to->mFlags & DTF_FUNC_VARIABLE))
		{
			to->mFlags |= DTF_FUNC_VARIABLE;
			mVariableFunctions.Push(to);
			Declaration* pdec = to->mParams;
			while (pdec)
			{
				pdec->mFlags |= DTF_FPARAM_NOCONST;
				pdec->mFlags &= ~DTF_FPARAM_CONST;
				pdec = pdec->mNext;
			}
		}
	}
}

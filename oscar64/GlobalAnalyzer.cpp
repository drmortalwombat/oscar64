#include "GlobalAnalyzer.h"

GlobalAnalyzer::GlobalAnalyzer(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mCalledFunctions(nullptr), mCallingFunctions(nullptr), mVariableFunctions(nullptr), mFunctions(nullptr), mGlobalVariables(nullptr), mCompilerOptions(COPT_DEFAULT)
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

void GlobalAnalyzer::AutoInline(void)
{
	bool	changed = false;
	do
	{
		changed = false;

		for (int i = 0; i < mFunctions.Size(); i++)
		{
			Declaration* f = mFunctions[i];
			if (!(f->mFlags & DTF_INLINE) && 
				!(f->mFlags & DTF_EXPORT) && 
				!(f->mFlags & DTF_PREVENT_INLINE) &&
				!(f->mBase->mFlags & DTF_VARIADIC) &&
				!(f->mFlags & DTF_FUNC_VARIABLE) && 
				!((f->mFlags & DTF_FUNC_ASSEMBLER) && !(f->mFlags & DTF_REQUEST_INLINE)) && 
				!(f->mFlags & DTF_INTRINSIC) && 
				!(f->mFlags & DTF_FUNC_RECURSIVE) && f->mLocalSize < 100)
			{
				int		nparams = 0;
				Declaration* dec = f->mBase->mParams;
				while (dec)
				{
					nparams++;
					dec = dec->mNext;
				}

				int	cost = (f->mComplexity - 20 * nparams);

				bool	doinline = false;
				if ((f->mCompilerOptions & COPT_OPTIMIZE_INLINE) && (f->mFlags & DTF_REQUEST_INLINE))
					doinline = true;
				if ((f->mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE) && (cost * (f->mCallers.Size() - 1) <= 0))
					doinline = true;
				if ((f->mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE_ALL) && (cost * (f->mCallers.Size() - 1) <= 10000))
					doinline = true;

				if (doinline)
					{
#if 0
					printf("INLINING %s %d * (%d - 1)\n", f->mIdent->mString, cost, f->mCallers.Size());
#endif
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
				pdec->mVarIndex = dec->mNumVars++;

				Expression* aexp = new Expression(pdec->mLocation, EX_ASSIGNMENT);
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
	if (!(procDec->mBase->mFlags & DTF_FASTCALL) && !(procDec->mBase->mFlags & DTF_STACKCALL) && (procDec->mType == DT_CONST_FUNCTION) && !(procDec->mFlags & DTF_FUNC_ANALYZING))
	{
		procDec->mFlags |= DTF_FUNC_ANALYZING;
		int	nbase = 0;
		for (int i = 0; i < procDec->mCalled.Size(); i++)
		{
			Declaration* cf = procDec->mCalled[i];

			if (cf->mType == DT_TYPE_FUNCTION)
				procDec->mFlags |= DTF_DYNSTACK;

			CheckFastcall(cf, false);

//			if (!(cf->mBase->mFlags & DTF_FASTCALL))
//				procDec->mBase->mFlags |= DTF_STACKCALL;

			cf = cf->mBase;
			int n = cf->mFastCallBase + cf->mFastCallSize;
			if (n > nbase)
				nbase = n;
		}

		procDec->mFastCallBase = nbase;
		procDec->mFastCallSize = 0;
		procDec->mBase->mFastCallBase = nbase;
		procDec->mBase->mFastCallSize = 0;

		procDec->mFlags &= ~DTF_FUNC_ANALYZING;

		if (procDec->mBase->mFlags & DTF_STACKCALL)
		{
			procDec->mBase->mFlags |= DTF_STACKCALL;
		}
		else if (procDec->mFlags & DTF_FUNC_RECURSIVE)
		{
			if (head)
				procDec->mBase->mFlags |= DTF_STACKCALL;
		}
		else if (!(procDec->mBase->mFlags & DTF_VARIADIC) && !(procDec->mFlags & DTF_FUNC_VARIABLE) && !(procDec->mFlags & DTF_DYNSTACK))
		{
			int		nparams = 0, npalign = 0;
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

			Declaration* dec = procDec->mBase->mParams;
			while (dec)
			{
				// Check for parameter crossing boundary
				if (nbase + nparams < numfpzero && nbase + nparams + dec->mBase->mSize > numfpzero)
				{
					npalign = numfpzero - (nbase + nparams);
					nparams += npalign;
				}
				nparams += dec->mBase->mSize;
				dec = dec->mNext;
			}

			if (nbase + nparams <= fplimit)
			{
				procDec->mFastCallBase = nbase;
				procDec->mFastCallSize = nparams;
				procDec->mBase->mFastCallBase = nbase;
				procDec->mBase->mFastCallSize = nparams;

				// Align fast call parameters to avoid crossing the zero page boundary
				if (npalign)
				{
					Declaration* dec = procDec->mBase->mParams;
					while (dec)
					{	
						if (nbase + dec->mVarIndex + dec->mBase->mSize > numfpzero)
							dec->mVarIndex += npalign;
						dec = dec->mNext;
					}
				}

				procDec->mBase->mFlags |= DTF_FASTCALL;
#if 0
				printf("FASTCALL %s\n", f->mIdent->mString);
#endif
			}
			else
				procDec->mBase->mFlags |= DTF_STACKCALL;
		}
		else
			procDec->mBase->mFlags |= DTF_STACKCALL;
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

void GlobalAnalyzer::AnalyzeProcedure(Expression* exp, Declaration* dec)
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

		mFunctions.Push(dec);

		dec->mFlags |= DTF_ANALYZED;
		dec->mFlags |= DTF_FUNC_INTRSAVE;

		if (dec->mFlags & DTF_INTERRUPT)
			dec->mFlags |= DTF_FUNC_INTRCALLED;

		if ((dec->mFlags & DTF_INTRINSIC) && !dec->mValue)
			dec->mFlags |= DTF_FUNC_CONSTEXPR;
		else if (dec->mFlags & DTF_DEFINED)
		{
			if (mCompilerOptions & COPT_OPTIMIZE_CONST_EXPRESSIONS)
				dec->mFlags |= DTF_FUNC_CONSTEXPR;
			dec->mFlags |= DTF_FUNC_PURE;
			Analyze(exp, dec, false);
		}
		else
			mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mQualIdent);

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
					AnalyzeGlobalVariable(adec->mBase);
			}
			else if (adec->mType == DT_LABEL)
			{

			}
			else if (adec->mType == DT_VARIABLE)
			{
				if (adec->mFlags & DTF_GLOBAL)
					AnalyzeGlobalVariable(adec);
			}
			else if (adec->mType == DT_FUNCTION_REF)
			{
				AnalyzeProcedure(adec->mBase->mValue, adec->mBase);
				RegisterProc(adec->mBase);
			}
			else if (adec->mType == DT_CONST_FUNCTION)
			{
				AnalyzeProcedure(adec->mValue, adec);
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
			Analyze(dec->mValue, dec, false);
		}
	}
}

void GlobalAnalyzer::AnalyzeInit(Declaration* mdec)
{
	while (mdec)
	{
		if (mdec->mValue)
			RegisterProc(Analyze(mdec->mValue, mdec, false));
		else if (mdec->mParams)
			AnalyzeInit(mdec->mParams);
		mdec = mdec->mNext;
	}
}

Declaration * GlobalAnalyzer::Analyze(Expression* exp, Declaration* procDec, bool lhs)
{
	Declaration* ldec, * rdec;

	procDec->mComplexity += 10;

	switch (exp->mType)
	{
	case EX_ERROR:
	case EX_VOID:
		break;
	case EX_CONSTANT:
		if (exp->mDecValue->mType == DT_CONST_FUNCTION)
			AnalyzeProcedure(exp->mDecValue->mValue, exp->mDecValue);
		else if (exp->mDecValue->mType == DT_CONST_STRUCT)
		{
			AnalyzeInit(exp->mDecValue->mParams);
		}
		else if (exp->mDecValue->mType == DT_CONST_POINTER)
		{
			ldec = Analyze(exp->mDecValue->mValue, procDec, true);
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
		if ((exp->mDecValue->mFlags & DTF_STATIC) || (exp->mDecValue->mFlags & DTF_GLOBAL))
		{
			Declaration* type = exp->mDecValue->mBase;
			while (type->mType == DT_TYPE_ARRAY)
				type = type->mBase;

			if (!(type->mFlags & DTF_CONST))
				procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

			if (lhs)
				procDec->mFlags &= ~DTF_FUNC_PURE;

			AnalyzeGlobalVariable(exp->mDecValue);
		}
		else
		{
			if (!(exp->mDecValue->mFlags & DTF_ANALYZED))
			{
				procDec->mLocalSize += exp->mDecValue->mSize;
				exp->mDecValue->mFlags |= DTF_ANALYZED;
			}
		}
		return exp->mDecValue;
	case EX_INITIALIZATION:
	case EX_ASSIGNMENT:
		ldec = Analyze(exp->mLeft, procDec, true);
		rdec = Analyze(exp->mRight, procDec, false);
		RegisterProc(rdec);
		return ldec;

	case EX_BINARY:
		ldec = Analyze(exp->mLeft, procDec, lhs);
		rdec = Analyze(exp->mRight, procDec, lhs);
		return ldec;

	case EX_RELATIONAL:
		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		return TheBoolTypeDeclaration;

	case EX_PREINCDEC:
		return Analyze(exp->mLeft, procDec, true);
	case EX_PREFIX:
		if (exp->mToken == TK_BINARY_AND)
		{
			ldec = Analyze(exp->mLeft, procDec, true);
			if (ldec->mType == DT_VARIABLE)
				ldec->mFlags |= DTF_VAR_ALIASING;
		} 
		else if (exp->mToken == TK_MUL)
		{
			ldec = Analyze(exp->mLeft, procDec, false);
			procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
			if (lhs)
				procDec->mFlags &= ~DTF_FUNC_PURE;

			return exp->mDecType;
		}
		else
			return Analyze(exp->mLeft, procDec, false);
		break;
	case EX_POSTFIX:
		break;
	case EX_POSTINCDEC:
		return Analyze(exp->mLeft, procDec, true);
	case EX_INDEX:
		ldec = Analyze(exp->mLeft, procDec, lhs);
		if (ldec->mType == DT_VARIABLE || ldec->mType == DT_ARGUMENT)
		{
			ldec = ldec->mBase;
			if (ldec->mType == DT_TYPE_POINTER)
			{
				if (lhs)
					procDec->mFlags &= ~DTF_FUNC_PURE;
				procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
			}
		}
		rdec = Analyze(exp->mRight, procDec, false);
		if (ldec->mBase)
			return ldec->mBase;
		break;
	case EX_QUALIFY:
		Analyze(exp->mLeft, procDec, lhs);
		return exp->mDecValue->mBase;
	case EX_CALL:
	case EX_INLINE:
		ldec = Analyze(exp->mLeft, procDec, false);
		if ((ldec->mFlags & DTF_INTRINSIC) && !ldec->mValue)
		{

		}
		else
		{
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

				if (pdec && !(ldec->mBase->mFlags & DTF_VARIADIC) && !(ldec->mFlags & (DTF_INTRINSIC | DTF_FUNC_ASSEMBLER)))
				{
#if 1
					if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
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
				
				if (pex->mType == EX_CALL && pex->mDecType->mType == DT_TYPE_STRUCT)
					ldec->mBase->mFlags |= DTF_STACKCALL;

				if (pdec)
					pdec = pdec->mNext;

				if (rex->mType == EX_LIST)
					rex = rex->mRight;
				else
					rex = nullptr;
			}
			RegisterProc(Analyze(exp->mRight, procDec, false));
		}
		break;
	case EX_LIST:
		RegisterProc(Analyze(exp->mLeft, procDec, false));
		return Analyze(exp->mRight, procDec, false);
	case EX_RETURN:
		if (exp->mLeft)
			RegisterProc(Analyze(exp->mLeft, procDec, false));
		break;
	case EX_SEQUENCE:
		do
		{
			if (exp->mLeft)
				ldec = Analyze(exp->mLeft, procDec, false);
			exp = exp->mRight;
		} while (exp);
		break;
	case EX_WHILE:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		break;
	case EX_IF:
		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight->mLeft, procDec, false);
		if (exp->mRight->mRight)
			rdec = Analyze(exp->mRight->mRight, procDec, false);
		break;
	case EX_ELSE:
		break;
	case EX_FOR:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		if (exp->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mRight, procDec, false);
		if (exp->mLeft->mLeft->mLeft)
			ldec = Analyze(exp->mLeft->mLeft->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		if (exp->mLeft->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mLeft->mRight, procDec, false);
		break;
	case EX_DO:
		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		break;
	case EX_BREAK:
	case EX_CONTINUE:
	case EX_ASSUME:
		break;
	case EX_TYPE:
		break;
	case EX_TYPECAST:
		return Analyze(exp->mRight, procDec, false);
		break;
	case EX_LOGICAL_AND:
		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		break;
	case EX_LOGICAL_OR:
		ldec = Analyze(exp->mLeft, procDec, false);
		rdec = Analyze(exp->mRight, procDec, false);
		break;
	case EX_LOGICAL_NOT:
		ldec = Analyze(exp->mLeft, procDec, false);
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
		ldec = Analyze(exp->mLeft, procDec, false);
		exp = exp->mRight;
		while (exp)
		{
			if (exp->mLeft->mRight)
				rdec = Analyze(exp->mLeft->mRight, procDec, false);
			exp = exp->mRight;
		}
		break;
	case EX_CASE:
		break;
	case EX_DEFAULT:
		break;
	case EX_CONDITIONAL:
		ldec = Analyze(exp->mLeft, procDec, false);
		RegisterProc(Analyze(exp->mRight->mLeft, procDec, false));
		RegisterProc(Analyze(exp->mRight->mRight, procDec, false));
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

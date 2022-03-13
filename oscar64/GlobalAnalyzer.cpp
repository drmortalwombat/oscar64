#include "GlobalAnalyzer.h"

GlobalAnalyzer::GlobalAnalyzer(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mCalledFunctions(nullptr), mCallingFunctions(nullptr), mVariableFunctions(nullptr), mFunctions(nullptr), mCompilerOptions(COPT_DEFAULT)
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
					printf("CALL %s[%d, %08llx] -> %d -> %s[%d, %08llx]\n", from->mIdent->mString, from->mComplexity, from->mFlags, calls[j], decs[j]->mIdent->mString, decs[j]->mComplexity, decs[j]->mFlags);
				else
					printf("CALL %s[%d, %08llx] -> %d\n", from->mIdent->mString, from->mComplexity, from->mFlags, calls[j]);
			}
		}
		else
		{
			printf("LEAF %d -> %s[%d, %08llx]\n", from->mCallers.Size(), from->mIdent->mString, from->mComplexity, from->mFlags );
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
			if (!(f->mFlags & DTF_INLINE) && !(f->mBase->mFlags & DTF_VARIADIC) && !(f->mFlags & DTF_FUNC_VARIABLE) && !((f->mFlags & DTF_FUNC_ASSEMBLER) && !(f->mFlags & DTF_REQUEST_INLINE)) && !(f->mFlags & DTF_INTRINSIC) && !(f->mFlags & DTF_FUNC_RECURSIVE) && f->mLocalSize < 100)
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
				if ((mCompilerOptions & COPT_OPTIMIZE_INLINE) && (f->mFlags & DTF_REQUEST_INLINE))
					doinline = true;
				if ((mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE) && (cost * (f->mCallers.Size() - 1) <= 0))
					doinline = true;
				if ((mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE_ALL) && (cost * (f->mCallers.Size() - 1) <= 10000))
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
		CheckFastcall(mFunctions[i]);
	}
}

void GlobalAnalyzer::CheckFastcall(Declaration* procDec)
{
	if (!(procDec->mBase->mFlags & DTF_FASTCALL) && !(procDec->mBase->mFlags & DTF_STACKCALL) && (procDec->mType == DT_CONST_FUNCTION))
	{
		if (!(procDec->mBase->mFlags & DTF_VARIADIC) && !(procDec->mFlags & DTF_FUNC_VARIABLE) && !(procDec->mFlags & DTF_FUNC_RECURSIVE))
		{
			int	nbase = 0;
			for (int i = 0; i < procDec->mCalled.Size(); i++)
			{
				Declaration* cf = procDec->mCalled[i];

				CheckFastcall(cf);

				cf = cf->mBase;
				if (cf->mFlags & DTF_FASTCALL)
				{
					int n = cf->mFastCallBase + cf->mFastCallSize;
					if (n > nbase)
						nbase = n;
				}
				else
					nbase = 1000;
			}

			int		nparams = 0;

			if (procDec->mBase->mBase->mType == DT_TYPE_STRUCT)
				nparams += 2;

			Declaration* dec = procDec->mBase->mParams;
			while (dec)
			{
				nparams += dec->mBase->mSize;
				dec = dec->mNext;
			}

			if (nbase + nparams <= BC_REG_FPARAMS_END - BC_REG_FPARAMS)
			{
				procDec->mFastCallBase = nbase;
				procDec->mFastCallSize = nparams;
				procDec->mBase->mFastCallBase = nbase;
				procDec->mBase->mFastCallSize = nparams;

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

void GlobalAnalyzer::AnalyzeProcedure(Expression* exp, Declaration* dec)
{
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

		if ((dec->mFlags & DTF_INTRINSIC) && !dec->mValue)
			dec->mFlags |= DTF_FUNC_CONSTEXPR;
		else if (dec->mFlags & DTF_DEFINED)
		{
			if (mCompilerOptions & COPT_OPTIMIZE_CONST_EXPRESSIONS)
				dec->mFlags |= DTF_FUNC_CONSTEXPR;
			Analyze(exp, dec);
		}
		else
			mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mIdent->mString);

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
	if (!(dec->mFlags & DTF_ANALYZED))
	{
		dec->mFlags |= DTF_ANALYZED;

		if (dec->mValue)
		{
			Analyze(dec->mValue, dec);
		}
	}
}

Declaration * GlobalAnalyzer::Analyze(Expression* exp, Declaration* procDec)
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
			Declaration* mdec = exp->mDecValue->mParams;
			while (mdec)
			{
				if (mdec->mValue)
					RegisterProc(Analyze(mdec->mValue, mdec));
				mdec = mdec->mNext;
			}
		}
		else if (exp->mDecValue->mType == DT_CONST_POINTER)
		{
			RegisterProc(Analyze(exp->mDecValue->mValue, procDec));
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
			procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
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
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		RegisterProc(rdec);
		break;
	case EX_BINARY:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		return ldec;

	case EX_RELATIONAL:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		return TheBoolTypeDeclaration;

	case EX_PREINCDEC:
		return Analyze(exp->mLeft, procDec);
	case EX_PREFIX:
		ldec = Analyze(exp->mLeft, procDec);
		if (exp->mToken == TK_BINARY_AND)
		{
			if (ldec->mType == DT_VARIABLE)
				ldec->mFlags |= DTF_VAR_ALIASING;
		} 
		else if (exp->mToken == TK_MUL)
		{
			procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
			return exp->mDecType;
		}
		break;
	case EX_POSTFIX:
		break;
	case EX_POSTINCDEC:
		return Analyze(exp->mLeft, procDec);
	case EX_INDEX:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		return ldec->mBase;
	case EX_QUALIFY:
		Analyze(exp->mLeft, procDec);
		return exp->mDecValue->mBase;
	case EX_CALL:
		ldec = Analyze(exp->mLeft, procDec);
		RegisterCall(procDec, ldec);
		if (!(GetProcFlags(ldec) & (DTF_FUNC_INTRSAVE | DTF_INTERRUPT)))
		{
			procDec->mFlags &= ~DTF_FUNC_INTRSAVE;
			if (procDec->mFlags & DTF_INTERRUPT)
				mErrors->Error(exp->mLocation, EWARN_NOT_INTERRUPT_SAFE, "Calling non interrupt safe function", ldec->mIdent);
		}

		if (exp->mRight)
			RegisterProc(Analyze(exp->mRight, procDec));
		break;
	case EX_LIST:
		RegisterProc(Analyze(exp->mLeft, procDec));
		return Analyze(exp->mRight, procDec);
	case EX_RETURN:
		if (exp->mLeft)
			RegisterProc(Analyze(exp->mLeft, procDec));
		break;
	case EX_SEQUENCE:
		do
		{
			if (exp->mLeft)
				ldec = Analyze(exp->mLeft, procDec);
			exp = exp->mRight;
		} while (exp);
		break;
	case EX_WHILE:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		break;
	case EX_IF:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight->mLeft, procDec);
		if (exp->mRight->mRight)
			rdec = Analyze(exp->mRight->mRight, procDec);
		break;
	case EX_ELSE:
		break;
	case EX_FOR:
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;

		if (exp->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mRight, procDec);
		if (exp->mLeft->mLeft->mLeft)
			ldec = Analyze(exp->mLeft->mLeft->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		if (exp->mLeft->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mLeft->mRight, procDec);
		break;
	case EX_DO:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		break;
	case EX_BREAK:
	case EX_CONTINUE:
	case EX_ASSUME:
		break;
	case EX_TYPE:
		break;
	case EX_TYPECAST:
		rdec = Analyze(exp->mRight, procDec);
		break;
	case EX_LOGICAL_AND:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		break;
	case EX_LOGICAL_OR:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		break;
	case EX_LOGICAL_NOT:
		ldec = Analyze(exp->mLeft, procDec);
		break;
	case EX_ASSEMBLER:
		procDec->mFlags |= DTF_FUNC_ASSEMBLER;
		procDec->mFlags &= ~DTF_FUNC_CONSTEXPR;
		AnalyzeAssembler(exp, procDec);
		break;
	case EX_UNDEFINED:
		break;
	case EX_SWITCH:
		ldec = Analyze(exp->mLeft, procDec);
		exp = exp->mRight;
		while (exp)
		{
			if (exp->mLeft->mRight)
				rdec = Analyze(exp->mLeft->mRight, procDec);
			exp = exp->mRight;
		}
		break;
	case EX_CASE:
		break;
	case EX_DEFAULT:
		break;
	case EX_CONDITIONAL:
		ldec = Analyze(exp->mLeft, procDec);
		RegisterProc(Analyze(exp->mRight->mLeft, procDec));
		RegisterProc(Analyze(exp->mRight->mRight, procDec));
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
		to->mFlags |= DTF_FUNC_VARIABLE;
		mVariableFunctions.Push(to);
	}
}

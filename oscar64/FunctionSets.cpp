#include "FunctionSets.h"

FunctionSets::FunctionSets(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker)
{

}

FunctionSets::~FunctionSets(void)
{

}	

void FunctionSets::CompleteAnalyze(void)
{
	for (int i = 0; i < mFunctions.Size(); i++)
	{
		Declaration*	fdec = mFunctions[i];
		if (fdec->mUnion)
		{
			Declaration* udec = Find(fdec);

//			printf("Union %s -> %s\n", fdec->mQualIdent->mString, udec->mQualIdent->mString);
		}
	}

	for (int i = 0; i < mFunctions.Size(); i++)
		mFunctions[i]->mFlags &= ~(DTF_FUNC_ANALYZING | DTF_ANALYZED);
	for (int i = 0; i < mVariables.Size(); i++)
		mVariables[i]->mFlags &= ~(DTF_FUNC_ANALYZING | DTF_ANALYZED);

	for (int i = 0; i < mUnions.Size(); i++)
	{
		Declaration* vdec = mUnions[i];
		Declaration* udec = Find(vdec);

//		if (udec->mQualIdent && vdec->mQualIdent)
//			printf("Type %s -> %s\n", vdec->mQualIdent->mString, udec->mQualIdent->mString);

		if (vdec->mType == DT_TYPE_POINTER)
		{
			vdec->mBase = udec->mBase;
		}
		else if (vdec->mType == DT_RETURN_VALUE)
		{
			vdec->mBase->mBase = udec->mBase;
		}
		else
		{
			if (vdec->mBase->mType == DT_TYPE_ARRAY)
				vdec = vdec->mBase;

			if (udec->mBase->mType == DT_TYPE_POINTER)
				vdec->mBase = udec->mBase;
			else
				vdec->mBase = udec->mBase->BuildPointer(vdec->mLocation);
		}
	}
}


void FunctionSets::AnalyzeProcedure(Expression* exp, Declaration* procDec)
{
	if (procDec->mFlags & DTF_FUNC_ANALYZING)
		;
	else if (procDec->mFlags & DTF_ANALYZED)
		;
	else if ((procDec->mFlags & DTF_INTRINSIC) && !procDec->mValue)
		;
	else
	{
		procDec->mFlags |= DTF_FUNC_ANALYZING;
		mFunctions.Push(procDec);

		Analyze(exp, procDec);

		procDec->mFlags |= DTF_ANALYZED;
		procDec->mFlags &= ~DTF_FUNC_ANALYZING;
	}
}

void FunctionSets::AnalyzeAssembler(Expression* exp, Declaration* procDec)
{
	while (exp)
	{
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
				AnalyzeProcedure(adec->mBase->mValue, adec->mBase);
			}
			else if (adec->mType == DT_CONST_FUNCTION)
			{
				AnalyzeProcedure(adec->mValue, adec);
			}
		}

		exp = exp->mRight;
	}
}

void FunctionSets::AnalyzeGlobalVariable(Declaration* dec)
{
	while (dec->mType == DT_VARIABLE_REF)
		dec = dec->mBase;

	if (!(dec->mFlags & DTF_ANALYZED))
	{
		dec->mFlags |= DTF_ANALYZED;
		mVariables.Push(dec);

		if (dec->mValue)
		{
			Analyze(dec->mValue, dec);
		}
	}
}

Declaration* FunctionSets::Find(Declaration* ldec)
{
	if (ldec->mUnion)
	{
		ldec->mUnion = Find(ldec->mUnion);
		return ldec->mUnion;
	}
	return ldec;
}

static bool IsFuncType(Declaration* dec)
{
	if (dec->mType == DT_TYPE_POINTER)
		return dec->mBase->mType == DT_TYPE_FUNCTION;
	else
		return dec->mType == DT_TYPE_FUNCTION;
}

Declaration * FunctionSets::Unify(Declaration* ldec, Declaration* rdec)
{
	if (ldec && ldec->mType == DT_VARIABLE_REF)
		ldec = ldec->mBase;
	if (rdec && rdec->mType == DT_VARIABLE_REF)
		rdec = rdec->mBase;

	if (ldec && !ldec->mUnion)
	{
		if (ldec->mType == DT_VARIABLE || ldec->mType == DT_ARGUMENT || ldec->mType == DT_ELEMENT || ldec->mType == DT_RETURN_VALUE)
		{
			if (ldec->mBase->IsFunctionType() || ldec->mBase->mType == DT_TYPE_ARRAY && ldec->mBase->mBase->IsFunctionType())
				if (!mUnions.Contains(ldec)) mUnions.Push(ldec);
		}
		else if (ldec->IsFunctionType())
		{
			if (!mUnions.Contains(ldec)) mUnions.Push(ldec);
		}
	}

	if (rdec && !rdec->mUnion)
	{
		if (rdec->mType == DT_VARIABLE || rdec->mType == DT_ARGUMENT || rdec->mType == DT_ELEMENT || rdec->mType == DT_RETURN_VALUE)
		{
			if (rdec->mBase->IsFunctionType() || rdec->mBase->mType == DT_TYPE_ARRAY && rdec->mBase->mBase->IsFunctionType())
				if (!mUnions.Contains(rdec)) mUnions.Push(rdec);
		}
		else if (rdec->IsFunctionType())
		{
			if (!mUnions.Contains(rdec)) mUnions.Push(rdec);
		}
	}

	if (ldec && ldec->mType == DT_CONST_FUNCTION && !ldec->mBase->mUnion)
		ldec->mBase->mUnion = ldec;
	if (rdec && rdec->mType == DT_CONST_FUNCTION && !rdec->mBase->mUnion)
		rdec->mBase->mUnion = rdec;

	if (!ldec)
		return rdec;
	else if (rdec)
	{
		ldec = Find(ldec);
		rdec = Find(rdec);

		if (ldec != rdec)
		{
			if (ldec->mType == DT_CONST_FUNCTION)
			{
				rdec->mUnion = ldec;
				return ldec;
			}
			else
			{
				ldec->mUnion = rdec;
				return rdec;
			}
		}
	}

	return ldec;
}

Declaration * FunctionSets::AnalyzeInit(Declaration* mdec)
{
	Declaration	*	pdec = mdec->mParams;
	Declaration* ptype = mdec->mBase;

	if (!ptype)
		return nullptr;

	if (ptype->mType == DT_TYPE_ARRAY)
	{
		while (pdec)
		{
			Declaration* ldec = AnalyzeInit(pdec);
			if (ldec && ptype->mBase->IsFunctionType())
			{
				ptype->mBase = CanonicalType(ptype->mBase);
				Unify(ptype->mBase, ldec);
			}
			pdec = pdec->mNext;
		}
	}
	else if (ptype->mType == DT_TYPE_STRUCT)
	{
		while (pdec)
		{
			Declaration * ldec = AnalyzeInit(pdec);
			if (ldec)
			{
				Declaration* vdec = ptype->mParams;
				while (vdec && vdec->mOffset != pdec->mOffset)
					vdec = vdec->mNext;
				if (vdec)
					Unify(vdec, ldec);
			}
			pdec = pdec->mNext;
		}

	}
	else if (mdec->mValue)
	{
		return Analyze(mdec->mValue, mdec);
	}

	return nullptr;
}


Declaration* FunctionSets::Analyze(Expression* exp, Declaration* procDec)
{
	Declaration* ldec, * rdec;

	switch (exp->mType)
	{
	case EX_ERROR:
	case EX_VOID:
		break;
	case EX_CONSTANT:
		if (exp->mDecValue->mType == DT_CONST_FUNCTION)
		{
			AnalyzeProcedure(exp->mDecValue->mValue, exp->mDecValue);
		}
		else if (exp->mDecValue->mType == DT_CONST_STRUCT)
		{
			AnalyzeInit(exp->mDecValue);
		}
		else if (exp->mDecValue->mType == DT_CONST_POINTER)
		{
		}
		else if (exp->mDecValue->mType == DT_CONST_ADDRESS)
		{
		}
		else if (exp->mDecValue->mType == DT_CONST_ASSEMBLER)
		{
			AnalyzeAssembler(exp->mDecValue->mValue, procDec);
		}
		return exp->mDecValue;

	case EX_VARIABLE:
	{
		Declaration* vdec = exp->mDecValue;
		while (vdec->mType == DT_VARIABLE_REF)
			vdec = vdec->mBase;

		if ((vdec->mFlags & DTF_STATIC) || (vdec->mFlags & DTF_GLOBAL))
		{
			AnalyzeGlobalVariable(vdec);
		}

		return vdec;
	}
	case EX_INITIALIZATION:
	case EX_ASSIGNMENT:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		return Unify(ldec, rdec);

	case EX_BINARY:
		Analyze(exp->mLeft, procDec);
		Analyze(exp->mRight, procDec);
		break;

	case EX_RELATIONAL:
		Analyze(exp->mLeft, procDec);
		Analyze(exp->mRight, procDec);
		break;

	case EX_PREINCDEC:
		Analyze(exp->mLeft, procDec);
		break;

	case EX_PREFIX:
		Analyze(exp->mLeft, procDec);
		break;

	case EX_POSTFIX:
		break;
	case EX_POSTINCDEC:
		Analyze(exp->mLeft, procDec);
		break;

	case EX_INDEX:
		ldec = Analyze(exp->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		return ldec;

	case EX_QUALIFY:
		ldec = Analyze(exp->mLeft, procDec);
		return exp->mDecValue;

	case EX_DISPATCH:
		Analyze(exp->mLeft, procDec);
		break;

	case EX_VCALL:
		exp->mType = EX_CALL;
		exp->mLeft->mDecValue = exp->mLeft->mDecValue->mVTable;
		// intentional fall through
	case EX_CALL:
	case EX_INLINE:

		ldec = Analyze(exp->mLeft, procDec);
		if (exp->mRight)
		{
			// Check for struct to struct forwarding
			Expression* rex = exp->mRight;
			Declaration* pdec = ldec->mBase->mParams;
			while (rex)
			{
				Expression* pex = rex->mType == EX_LIST ? rex->mLeft : rex;
				Unify(pdec, Analyze(pex, procDec));

				if (pdec)
					pdec = pdec->mNext;

				if (rex->mType == EX_LIST)
					rex = rex->mRight;
				else
					rex = nullptr;
			}
		}
		if (ldec->mReturnValue)
			return ldec->mReturnValue;
		break;

	case EX_LIST:
	case EX_COMMA:
		Analyze(exp->mLeft, procDec);
		return Analyze(exp->mRight, procDec);
	case EX_RETURN:
		if (exp->mLeft)
		{
			ldec = Analyze(exp->mLeft, procDec);
			if (procDec->mBase->mBase->IsFunctionType())
			{
				if (!procDec->mReturnValue)
				{
					Declaration* rtype = procDec->mBase->mBase->Clone();
					procDec->mBase->mBase = rtype;

					procDec->mReturnValue = new Declaration(exp->mLocation, DT_RETURN_VALUE);
					procDec->mReturnValue->mIdent = procDec->mReturnValue->mQualIdent = procDec->mQualIdent->Mangle("@return");
					procDec->mReturnValue->mBase = rtype;
				}
				procDec->mReturnValue = Unify(procDec->mReturnValue, ldec);
			}
		}
		break;
	case EX_SEQUENCE:
		do
		{
			if (exp->mType == EX_SEQUENCE)
			{
				if (exp->mLeft)
					ldec = Analyze(exp->mLeft, procDec);
				exp = exp->mRight;
			}
			else
				return Analyze(exp, procDec);

		} while (exp);
		break;

	case EX_SCOPE:
		Analyze(exp->mLeft, procDec);
		break;

	case EX_CONSTRUCT:
		if (exp->mLeft->mLeft)
			Analyze(exp->mLeft->mLeft, procDec);
		if (exp->mLeft->mRight)
			Analyze(exp->mLeft->mRight, procDec);
		if (exp->mRight)
			return Analyze(exp->mRight, procDec);
		break;

	case EX_CLEANUP:
		Analyze(exp->mRight, procDec);
		return Analyze(exp->mLeft, procDec);

	case EX_WHILE:
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
		if (exp->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mRight, procDec);
		if (exp->mLeft->mLeft->mLeft)
			ldec = Analyze(exp->mLeft->mLeft->mLeft, procDec);
		rdec = Analyze(exp->mRight, procDec);
		if (exp->mLeft->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mLeft->mRight, procDec);
		break;
	case EX_FORBODY:
		ldec = Analyze(exp->mLeft, procDec);
		if (exp->mRight)
			Analyze(exp->mRight, procDec);
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
		ldec = Analyze(exp->mLeft, procDec);
		if (exp->mDecType->mType == DT_TYPE_POINTER && exp->mDecType->mBase->mType == DT_TYPE_FUNCTION)
		{
			exp->mDecType = CanonicalType(exp->mDecType);
			Unify(exp->mDecType, ldec);
		}
		return ldec;

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
		Analyze(exp->mLeft, procDec);
		ldec = Analyze(exp->mRight->mLeft, procDec);
		rdec = Analyze(exp->mRight->mRight, procDec);
		return Unify(ldec, rdec);
	}

	return nullptr;
}

Declaration* FunctionSets::CanonicalType(Declaration* tdec)
{
	for (int i = 0; i < mTypes.Size(); i++)
		if (mTypes[i]->IsSame(tdec))
			return mTypes[i];
	tdec = tdec->Clone();
	mTypes.Push(tdec);
	return tdec;
}

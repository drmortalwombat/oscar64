#include "InterCodeGenerator.h"

InterCodeGenerator::InterCodeGenerator(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mCompilerOptions(COPT_DEFAULT)
{

}

InterCodeGenerator::~InterCodeGenerator(void)
{

}

static inline InterType InterTypeOf(const Declaration* dec)
{
	switch (dec->mType)
	{
	default:
	case DT_TYPE_VOID:
		return IT_NONE;
	case DT_TYPE_INTEGER:
	case DT_TYPE_ENUM:
		if (dec->mSize == 1)
			return IT_INT8;
		else if (dec->mSize == 2)
			return IT_INT16;
		else
			return IT_INT32;
	case DT_TYPE_BOOL:
		return IT_BOOL;
	case DT_TYPE_FLOAT:
		return IT_FLOAT;
	case DT_TYPE_POINTER:
	case DT_TYPE_FUNCTION:
	case DT_TYPE_ARRAY:
		return IT_POINTER;
	}
}

InterCodeGenerator::ExValue InterCodeGenerator::Dereference(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, ExValue v, int level)
{
	while (v.mReference > level)
	{
		InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_LOAD);
		ins->mSrc[0].mMemory = IM_INDIRECT;
		ins->mSrc[0].mType = IT_POINTER;
		ins->mSrc[0].mTemp = v.mTemp;
		ins->mDst.mType = v.mReference == 1 ? InterTypeOf(v.mType) : IT_POINTER;
		ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
		ins->mSrc[0].mOperandSize = v.mReference == 1 ? v.mType->mSize : 2;
		ins->mSrc[0].mStride = v.mReference == 1 ? v.mType->mStripe : 1;

		if (v.mReference == 1 && v.mType->mType == DT_TYPE_ENUM)
		{
			ins->mDst.mRange.LimitMin(v.mType->mMinValue);
			ins->mDst.mRange.LimitMax(v.mType->mMaxValue);
		}
		if (v.mType->mFlags & DTF_VOLATILE)
			ins->mVolatile = true;
		block->Append(ins);

		v = ExValue(v.mType, ins->mDst.mTemp, v.mReference - 1);
	}

	return v;
}

InterCodeGenerator::ExValue InterCodeGenerator::CoerceType(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, ExValue v, Declaration* type)
{
	int		stemp = v.mTemp;

	if (v.mType->IsIntegerType() && type->mType == DT_TYPE_FLOAT)
	{
		if (v.mType->mSize == 1)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction	*	xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO16S;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT16;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
			else
			{
				InterInstruction	*	xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO16U;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT16;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
		}

		InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
		cins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_INT2FLOAT : IA_UINT2FLOAT;
		cins->mSrc[0].mType = IT_INT16;
		cins->mSrc[0].mTemp = stemp;
		cins->mDst.mType = IT_FLOAT;
		cins->mDst.mTemp = proc->AddTemporary(IT_FLOAT);
		block->Append(cins);

		v.mTemp = cins->mDst.mTemp;
		v.mType = type;
	}
	else if (v.mType->mType == DT_TYPE_FLOAT && type->IsIntegerType())
	{
		InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
		cins->mOperator = IA_FLOAT2INT;
		cins->mSrc[0].mType = IT_FLOAT;
		cins->mSrc[0].mTemp = v.mTemp;
		cins->mDst.mType = IT_INT16;
		cins->mDst.mTemp = proc->AddTemporary(IT_INT16);
		block->Append(cins);
		v.mTemp = cins->mDst.mTemp;
		v.mType = type;
	}
	else if (v.mType->mSize < type->mSize)
	{
		if (v.mType->mSize == 1 && type->mSize == 2)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction	*	xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO16S;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT16;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
			else
			{
				InterInstruction	*	xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO16U;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT16;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
		}
		else if (v.mType->mSize == 2 && type->mSize == 4)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT16TO32S;
				xins->mSrc[0].mType = IT_INT16;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT32;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT32);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
			else
			{
				InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT16TO32U;
				xins->mSrc[0].mType = IT_INT16;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT32;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT32);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
		}
		else if (v.mType->mSize == 1 && type->mSize == 4)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO32S;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT32;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT32);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
			else
			{
				InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO32U;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT32;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT32);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
		}

		v.mTemp = stemp;
		v.mType = type;
	}
	else
	{
		// ignore size reduction

		v.mType = type;
	}

	return v;
}

static inline bool InterTypeTypeInteger(InterType t)
{
	return t == IT_INT8 || t == IT_INT16 || t == IT_INT32 || t == IT_BOOL;
}

static inline InterType InterTypeOfArithmetic(InterType t1, InterType t2)
{
	if (t1 == IT_FLOAT || t2 == IT_FLOAT)
		return IT_FLOAT;
	else if (InterTypeTypeInteger(t1) && InterTypeTypeInteger(t2))
		return t1 > t2 ? t1 : t2;
	else if (t1 == IT_POINTER && t2 == IT_POINTER)
		return IT_INT16;
	else if (t1 == IT_POINTER || t2 == IT_POINTER)
		return IT_POINTER;
	else
		return IT_INT16;
}

void InterCodeGenerator::InitLocalVariable(InterCodeProcedure* proc, Declaration* dec, int index)
{
	if (!proc->mLocalVars[index])
	{
		proc->mLocalVars[index] = new InterVariable();
		proc->mLocalVars[index]->mIdent = dec->mIdent;
	}
}

void InterCodeGenerator::InitGlobalVariable(InterCodeModule * mod, Declaration* dec)
{
	if (!dec->mLinkerObject)
	{
		InterVariable	*	var = new InterVariable();
		var->mOffset = 0;
		var->mSize = dec->mSize;
		var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA, dec->mAlignment);
		var->mIdent = dec->mIdent;

		Declaration* type = dec->mBase;
		while (type->mType == DT_TYPE_ARRAY)
			type = type->mBase;
		if (type->mFlags & DTF_CONST)
			var->mLinkerObject->mFlags |= LOBJF_CONST;
		if (dec->mFlags & DTF_ZEROPAGE)
			var->mLinkerObject->mFlags |= LOBJF_ZEROPAGE;

		var->mIndex = mod->mGlobalVars.Size();
		mod->mGlobalVars.Push(var);

		dec->mVarIndex = var->mIndex;
		dec->mLinkerObject = var->mLinkerObject;

		if (dec->mFlags & DTF_SECTION_START)
			dec->mLinkerObject->mType = LOT_SECTION_START;
		else if (dec->mFlags & DTF_SECTION_END)
			dec->mLinkerObject->mType = LOT_SECTION_END;

		uint8* d = var->mLinkerObject->AddSpace(var->mSize);
		if (dec->mValue)
		{
			if (dec->mValue->mType == EX_CONSTANT)
			{				
				BuildInitializer(mod, d, 0, dec->mValue->mDecValue, var);
			}
			else
				mErrors->Error(dec->mLocation, EERR_CONSTANT_INITIALIZER, "Non constant initializer");
		}
	}
}

void InterCodeGenerator::TranslateAssembler(InterCodeModule* mod, Expression* exp, GrowingArray<Declaration*> * refvars)
{
	int	offset = 0, osize = 0;
	Expression* cexp = exp;
	while (cexp)
	{
		osize += AsmInsSize(cexp->mAsmInsType, cexp->mAsmInsMode);
		cexp = cexp->mRight;
	}

	Declaration* dec = exp->mDecValue;

	dec->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_NATIVE_CODE);

	uint8* d = dec->mLinkerObject->AddSpace(osize);

	GrowingArray<Declaration*>		refVars(nullptr);

	cexp = exp;
	while (cexp)
	{
		if (cexp->mAsmInsType != ASMIT_BYTE)
		{
			int	opcode = AsmInsOpcodes[cexp->mAsmInsType][cexp->mAsmInsMode];
			d[offset++] = opcode;
		}

		Declaration* aexp = nullptr;
		if (cexp->mLeft)
			aexp = cexp->mLeft->mDecValue;

		switch (cexp->mAsmInsMode)
		{
		case ASMIM_IMPLIED:
			break;
		case ASMIM_IMMEDIATE:
			if (!aexp)
				mErrors->Error(cexp->mLocation, EERR_ASM_INVALD_OPERAND, "Missing assembler operand");
			else if (aexp->mType == DT_CONST_INTEGER)
				d[offset] = cexp->mLeft->mDecValue->mInteger & 255;
			else if (aexp->mType == DT_LABEL_REF)
			{
				if (aexp->mBase->mBase)
				{
					if (!aexp->mBase->mBase->mLinkerObject)
						TranslateAssembler(mod, aexp->mBase->mBase->mValue, nullptr);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					if (aexp->mFlags & DTF_UPPER_BYTE)
						ref.mFlags = LREF_HIGHBYTE;
					else
						ref.mFlags = LREF_LOWBYTE;
					ref.mRefObject = aexp->mBase->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mOffset + aexp->mBase->mInteger;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined immediate operand", aexp->mBase->mIdent);
			}
			else if (aexp->mType == DT_VARIABLE_REF)
			{
				if (aexp->mBase->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp->mBase);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					if (aexp->mFlags & DTF_UPPER_BYTE)
						ref.mFlags = LREF_HIGHBYTE;
					else
						ref.mFlags = LREF_LOWBYTE;
					ref.mRefObject = aexp->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mOffset;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Invalid immediate operand");
			}
			else if (aexp->mType == DT_FUNCTION_REF)
			{
				if (!aexp->mBase->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mBase->mValue, aexp->mBase);
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				if (aexp->mFlags & DTF_UPPER_BYTE)
					ref.mFlags = LREF_HIGHBYTE;
				else
					ref.mFlags = LREF_LOWBYTE;
				ref.mRefObject = aexp->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset;
				ref.mRefObject->mFlags |= LOBJF_RELEVANT;
				dec->mLinkerObject->AddReference(ref);
			}
			else
				mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Invalid immediate operand");

			offset += 1;
			break;
		case ASMIM_ZERO_PAGE:
		case ASMIM_ZERO_PAGE_X:
		case ASMIM_INDIRECT_X:
		case ASMIM_INDIRECT_Y:
			if (!aexp)
				mErrors->Error(cexp->mLocation, EERR_ASM_INVALD_OPERAND, "Missing assembler operand");
			else if (aexp->mType == DT_VARIABLE_REF)
			{
				if (refvars)
				{
					int j = 0;
					while (j < refvars->Size() && (*refvars)[j] != aexp->mBase)
						j++;
					if (j == refvars->Size())
						refvars->Push(aexp->mBase);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_TEMPORARY;
					ref.mRefObject = dec->mLinkerObject;
					ref.mRefOffset = j;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);

					d[offset] = aexp->mOffset;
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Local variable outside scope");
			}
			else if (aexp->mType == DT_ARGUMENT || aexp->mType == DT_VARIABLE)
			{
				if (refvars)
				{
					int j = 0;
					while (j < refvars->Size() && (*refvars)[j] != aexp)
						j++;
					if (j == refvars->Size())
						refvars->Push(aexp);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_TEMPORARY;
					ref.mRefObject = dec->mLinkerObject;
					ref.mRefOffset = j;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);

					d[offset] = 0;
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Local variable outside scope");
			}
			else
				d[offset] = aexp->mInteger;
			offset += 1;
			break;
		case ASMIM_ABSOLUTE:
		case ASMIM_INDIRECT:
		case ASMIM_ABSOLUTE_X:
		case ASMIM_ABSOLUTE_Y:
			if (!aexp)
				mErrors->Error(cexp->mLocation, EERR_ASM_INVALD_OPERAND, "Missing assembler operand");
			else if (aexp->mType == DT_CONST_INTEGER)
			{
				d[offset + 0] = cexp->mLeft->mDecValue->mInteger & 255;
				d[offset + 1] = cexp->mLeft->mDecValue->mInteger >> 8;
			}
			else if (aexp->mType == DT_LABEL)
			{
				if (aexp->mBase)
				{
					if (!aexp->mBase->mLinkerObject)
						TranslateAssembler(mod, aexp->mBase->mValue, nullptr);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
					ref.mRefObject = aexp->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mInteger;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined label");
			}
			else if (aexp->mType == DT_LABEL_REF)
			{
				if (aexp->mBase->mBase)
				{
					if (!aexp->mBase->mBase->mLinkerObject)
						TranslateAssembler(mod, aexp->mBase->mBase->mValue, nullptr);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
					ref.mRefObject = aexp->mBase->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mOffset + aexp->mBase->mInteger;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined label");
			}
			else if (aexp->mType == DT_CONST_ASSEMBLER)
			{
				if (!aexp->mLinkerObject)
					TranslateAssembler(mod, aexp->mValue, nullptr);

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
				ref.mRefObject = aexp->mLinkerObject;
				ref.mRefOffset = 0;
				ref.mRefObject->mFlags |= LOBJF_RELEVANT;
				dec->mLinkerObject->AddReference(ref);
			}
			else if (aexp->mType == DT_VARIABLE)
			{
				if (aexp->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
					ref.mRefObject = aexp->mLinkerObject;
					ref.mRefOffset = 0;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
			}
			else if (aexp->mType == DT_VARIABLE_REF)
			{
				if (aexp->mBase->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp->mBase);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
					ref.mRefObject = aexp->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mOffset;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
			}
			else if (aexp->mType == DT_CONST_FUNCTION)
			{
				if (!aexp->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mValue, aexp);
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
				ref.mRefObject = aexp->mLinkerObject;
				ref.mRefOffset = 0;
				ref.mRefObject->mFlags |= LOBJF_RELEVANT;
				dec->mLinkerObject->AddReference(ref);
			}
			else if (aexp->mType == DT_FUNCTION_REF)
			{
				if (!aexp->mBase->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mBase->mValue, aexp->mBase);
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
				ref.mRefObject = aexp->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset;
				ref.mRefObject->mFlags |= LOBJF_RELEVANT;
				dec->mLinkerObject->AddReference(ref);
			}

			offset += 2;
			break;
		case ASMIM_RELATIVE:
			if (!aexp)
				mErrors->Error(cexp->mLocation, EERR_ASM_INVALD_OPERAND, "Missing assembler operand");
			else
			{
				if (aexp->mType == DT_LABEL_REF)
				{
					if (aexp->mBase->mBase)
						d[offset] = aexp->mOffset + aexp->mBase->mInteger - offset - 1;
					else
						mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined immediate operand", aexp->mBase->mIdent);
				}
				else
					d[offset] = aexp->mInteger - offset - 1;
			}
			offset++;
			break;
		}

		cexp = cexp->mRight;
	}

	assert(offset == osize);
}

void InterCodeGenerator::BuildSwitchTree(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock* block, ExValue v, const SwitchNodeArray& nodes, int left, int right, InterCodeBasicBlock* dblock)
{
	if (right - left < 5)
	{
		for (int i = left; i < right; i++)
		{
			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			proc->Append(cblock);

			InterInstruction* vins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			vins->mConst.mType = IT_INT16;
			vins->mConst.mIntConst = nodes[i].mValue;
			vins->mDst.mType = IT_INT16;
			vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
			block->Append(vins);

			InterInstruction* cins = new InterInstruction(exp->mLocation, IC_RELATIONAL_OPERATOR);
			cins->mOperator = IA_CMPEQ;
			cins->mSrc[0].mType = vins->mDst.mType;
			cins->mSrc[0].mTemp = vins->mDst.mTemp;
			cins->mSrc[1].mType = vins->mDst.mType;
			cins->mSrc[1].mTemp = v.mTemp;
			cins->mDst.mType = IT_BOOL;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

			block->Append(cins);

			InterInstruction* bins = new InterInstruction(exp->mLocation, IC_BRANCH);
			bins->mSrc[0].mType = IT_BOOL;
			bins->mSrc[0].mTemp = cins->mDst.mTemp;
			block->Append(bins);

			block->Close(nodes[i].mBlock, cblock);

			block = cblock;
		}

		InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);

		block->Append(jins);
		block->Close(dblock, nullptr);
	}
	else
	{
		int	center = (left + right + 1) >> 1;

		InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
		proc->Append(cblock);

		InterCodeBasicBlock* rblock = new InterCodeBasicBlock();
		proc->Append(rblock);

		InterCodeBasicBlock* lblock = new InterCodeBasicBlock();
		proc->Append(lblock);

		InterInstruction* vins = new InterInstruction(exp->mLocation, IC_CONSTANT);
		vins->mConst.mType = IT_INT16;
		vins->mConst.mIntConst = nodes[center].mValue;
		vins->mDst.mType = IT_INT16;
		vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
		block->Append(vins);

		InterInstruction* cins = new InterInstruction(exp->mLocation, IC_RELATIONAL_OPERATOR);
		cins->mOperator = IA_CMPEQ;
		cins->mSrc[0].mType = vins->mDst.mType;
		cins->mSrc[0].mTemp = vins->mDst.mTemp;
		cins->mSrc[1].mType = vins->mDst.mType;
		cins->mSrc[1].mTemp = v.mTemp;
		cins->mDst.mType = IT_BOOL;
		cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

		block->Append(cins);

		InterInstruction* bins = new InterInstruction(exp->mLocation, IC_BRANCH);
		bins->mSrc[0].mType = IT_BOOL;
		bins->mSrc[0].mTemp = cins->mDst.mTemp;
		block->Append(bins);

		block->Close(nodes[center].mBlock, cblock);

		InterInstruction* rins = new InterInstruction(exp->mLocation, IC_RELATIONAL_OPERATOR);
		rins->mOperator = IA_CMPLS;
		rins->mSrc[0].mType = vins->mDst.mType;
		rins->mSrc[0].mTemp = vins->mDst.mTemp;
		rins->mSrc[1].mType = vins->mDst.mType;
		rins->mSrc[1].mTemp = v.mTemp;
		rins->mDst.mType = IT_BOOL;
		rins->mDst.mTemp = proc->AddTemporary(rins->mDst.mType);

		cblock->Append(rins);

		InterInstruction* rbins = new InterInstruction(exp->mLocation, IC_BRANCH);
		rbins->mSrc[0].mType = IT_BOOL;
		rbins->mSrc[0].mTemp = rins->mDst.mTemp;
		cblock->Append(rbins);

		cblock->Close(lblock, rblock);

		BuildSwitchTree(proc, exp, lblock, v, nodes, left, center, dblock);
		BuildSwitchTree(proc, exp, rblock, v, nodes, center + 1, right, dblock);
	}
}

InterCodeGenerator::ExValue InterCodeGenerator::TranslateInline(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, Expression* exp, InterCodeBasicBlock* breakBlock, InterCodeBasicBlock* continueBlock, InlineMapper* inlineMapper, bool inlineConstexpr)
{
	Declaration* dec;
	ExValue	vl, vr;

	Declaration* fdec = exp->mLeft->mDecValue;
	Expression* fexp = fdec->mValue;
	Declaration* ftype = fdec->mBase;

	InlineMapper	nmapper;
	nmapper.mReturn = new InterCodeBasicBlock();
	nmapper.mVarIndex = proc->mNumLocals;
	nmapper.mConstExpr = inlineConstexpr;
	proc->mNumLocals += fdec->mNumVars;
	if (inlineMapper)
		nmapper.mDepth = inlineMapper->mDepth + 1;
	proc->Append(nmapper.mReturn);

	Declaration* pdec = ftype->mParams;
	Expression* pex = exp->mRight;
	while (pex)
	{
		int	nindex = proc->mNumLocals++;
		Declaration* vdec = new Declaration(pex->mLocation, DT_VARIABLE);

		InterInstruction* ains = new InterInstruction(pex->mLocation, IC_CONSTANT);
		ains->mDst.mType = IT_POINTER;
		ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
		ains->mConst.mMemory = IM_LOCAL;
		ains->mConst.mVarIndex = nindex;

		if (pdec)
		{
			nmapper.mParams[pdec->mVarIndex] = nindex;

			vdec->mVarIndex = nindex;
			vdec->mBase = pdec->mBase;
			if (pdec->mBase->mType == DT_TYPE_ARRAY)
				ains->mConst.mOperandSize = 2;
			else
				ains->mConst.mOperandSize = pdec->mSize;
			vdec->mSize = ains->mConst.mOperandSize;
			vdec->mIdent = pdec->mIdent;
		}
		else
		{
			mErrors->Error(pex->mLocation, EERR_WRONG_PARAMETER, "Too many arguments for function call");
		}

		block->Append(ains);

		Expression* texp;

		if (pex->mType == EX_LIST)
		{
			texp = pex->mLeft;
			pex = pex->mRight;
		}
		else
		{
			texp = pex;
			pex = nullptr;
		}

		ExValue	vp(pdec ? pdec->mBase : TheSignedIntTypeDeclaration, ains->mDst.mTemp, 1);

		vr = TranslateExpression(procType, proc, block, texp, breakBlock, continueBlock, inlineMapper, &vp);

		if (vr.mType->mType == DT_TYPE_STRUCT || vr.mType->mType == DT_TYPE_UNION)
		{
			if (pdec && !pdec->mBase->CanAssign(vr.mType))
				mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

			vr = Dereference(proc, texp, block, vr, 1);

			if (vr.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an adressable expression");

			if (vp.mTemp != vr.mTemp)
			{
				InterInstruction* cins = new InterInstruction(exp->mLocation, IC_COPY);

				cins->mSrc[0].mType = IT_POINTER;
				cins->mSrc[0].mTemp = vr.mTemp;
				cins->mSrc[0].mMemory = IM_INDIRECT;
				cins->mSrc[0].mOperandSize = vr.mType->mSize;
				cins->mSrc[0].mStride = vr.mType->mStripe;

				cins->mSrc[1].mType = IT_POINTER;
				cins->mSrc[1].mTemp = ains->mDst.mTemp;
				cins->mSrc[1].mMemory = IM_INDIRECT;
				cins->mSrc[1].mOperandSize = vr.mType->mSize;

				cins->mConst.mOperandSize = vr.mType->mSize;
				block->Append(cins);
			}
		}
		else
		{
			if (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION)
				vr = Dereference(proc, texp, block, vr, 1);
			else
				vr = Dereference(proc, texp, block, vr);

			if (pdec)
			{
				if (!pdec->mBase->CanAssign(vr.mType))
					mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
				vr = CoerceType(proc, texp, block, vr, pdec->mBase);
			}
			else if (vr.mType->IsIntegerType() && vr.mType->mSize < 2)
			{
				vr = CoerceType(proc, texp, block, vr, TheSignedIntTypeDeclaration);
			}

			InterInstruction* wins = new InterInstruction(texp->mLocation, IC_STORE);
			wins->mSrc[1].mMemory = IM_INDIRECT;
			wins->mSrc[0].mType = InterTypeOf(vr.mType);;
			wins->mSrc[0].mTemp = vr.mTemp;
			wins->mSrc[1].mType = IT_POINTER;
			wins->mSrc[1].mTemp = ains->mDst.mTemp;
			if (pdec)
			{
				if (pdec->mBase->mType == DT_TYPE_ARRAY)
					wins->mSrc[1].mOperandSize = 2;
				else
					wins->mSrc[1].mOperandSize = pdec->mSize;
			}
			else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
				wins->mSrc[1].mOperandSize = vr.mType->mSize;
			else
				wins->mSrc[1].mOperandSize = 2;
			block->Append(wins);
		}

		if (pdec)
			pdec = pdec->mNext;
	}

	if (pdec)
		mErrors->Error(exp->mLocation, EERR_WRONG_PARAMETER, "Not enough arguments for function call");

	Declaration* rdec = nullptr;
	if (ftype->mBase->mType != DT_TYPE_VOID)
	{
		int	nindex = proc->mNumLocals++;
		nmapper.mResult = nindex;

		rdec = new Declaration(ftype->mLocation, DT_VARIABLE);
		rdec->mVarIndex = nindex;
		rdec->mBase = ftype->mBase;
		rdec->mSize = rdec->mBase->mSize;
	}

	vl = TranslateExpression(ftype, proc, block, fexp, nullptr, nullptr, &nmapper);

	InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);
	block->Append(jins);

	block->Close(nmapper.mReturn, nullptr);
	block = nmapper.mReturn;

	if (rdec)
	{
		InterInstruction* ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
		ins->mDst.mType = IT_POINTER;
		ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
		ins->mConst.mOperandSize = rdec->mSize;
		ins->mConst.mIntConst = rdec->mOffset;
		ins->mConst.mVarIndex = rdec->mVarIndex;
		ins->mConst.mMemory = IM_LOCAL;

		block->Append(ins);

		return ExValue(rdec->mBase, ins->mDst.mTemp, 1);
	}
	else
		return ExValue(TheVoidTypeDeclaration);
}

InterCodeGenerator::ExValue InterCodeGenerator::TranslateExpression(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, Expression* exp, InterCodeBasicBlock* breakBlock, InterCodeBasicBlock* continueBlock, InlineMapper* inlineMapper, ExValue* lrexp)
{
	Declaration* dec;
	ExValue	vl, vr;

	for (;;)
	{
		switch (exp->mType)
		{
		case EX_VOID:
			return ExValue(TheVoidTypeDeclaration);

		case EX_SEQUENCE:
		case EX_LIST:
			vr = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			exp = exp->mRight;
			if (!exp)
				return ExValue(TheVoidTypeDeclaration);
			break;
		case EX_CONSTANT:
			dec = exp->mDecValue;
			switch (dec->mType)
			{
			case DT_CONST_INTEGER:
				{
				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				if (ins->mDst.mType == IT_INT8)
				{
					if (dec->mBase->mFlags & DTF_SIGNED)
					{
						if (dec->mInteger < -128 || dec->mInteger > 127)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
						ins->mConst.mIntConst = int8(dec->mInteger);
					}
					else
					{
						if (dec->mInteger < 0 || dec->mInteger > 255)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Unsigned integer constant truncated");
						ins->mConst.mIntConst = uint8(dec->mInteger);
					}
				}
				else if (ins->mDst.mType == IT_INT16)
				{
					if (dec->mBase->mFlags & DTF_SIGNED)
					{
						if (dec->mInteger < -32768 || dec->mInteger > 32767)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
						ins->mConst.mIntConst = int16(dec->mInteger);
					}
					else
					{
						if (dec->mInteger < 0 || dec->mInteger > 65535)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Unsigned integer constant truncated");
						ins->mConst.mIntConst = uint16(dec->mInteger);
					}
				}
				else if (ins->mDst.mType == IT_INT32)
				{
					if (dec->mBase->mFlags & DTF_SIGNED)
					{
						if (dec->mInteger < -2147483648LL || dec->mInteger > 2147483647LL)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
						ins->mConst.mIntConst = int32(dec->mInteger);
					}
					else
					{
						if (dec->mInteger < 0 || dec->mInteger > 4294967296LL)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Unsigned integer constant truncated");
						ins->mConst.mIntConst = uint32(dec->mInteger);
					}
				}
				else
				{
					ins->mConst.mIntConst = dec->mInteger;
				}
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp);

			} break;

			case DT_CONST_FLOAT:
				{
				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mFloatConst = dec->mNumber;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp);

			} break;

			case DT_CONST_ADDRESS:
			{
				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mIntConst = dec->mInteger;
				ins->mConst.mMemory = IM_ABSOLUTE;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp);

			} break;

			case DT_CONST_FUNCTION:
			{
				if (!dec->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(proc->mModule, dec->mValue, dec);
				}

				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mVarIndex = dec->mVarIndex;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mMemory = IM_PROCEDURE;
				ins->mConst.mIntConst = 0;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp);

			} break;

			case DT_CONST_ASSEMBLER:
			{
				if (!dec->mLinkerObject)
					TranslateAssembler(proc->mModule, dec->mValue, nullptr);

				InterInstruction* ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mVarIndex = dec->mVarIndex;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mMemory = IM_PROCEDURE;
				ins->mConst.mIntConst = 0;
				block->Append(ins);
				return ExValue(TheVoidPointerTypeDeclaration, ins->mDst.mTemp);
			}

			case DT_CONST_POINTER:
			{
				vl = TranslateExpression(procType, proc, block, dec->mValue, breakBlock, continueBlock, inlineMapper);
				vl.mReference--;
				vl.mType = exp->mDecType;
				return vl;
			}
			case DT_CONST_DATA:
			{
				if (!dec->mLinkerObject)
				{
					dec->mVarIndex = proc->mModule->mGlobalVars.Size();
					InterVariable* var = new InterVariable();
					var->mIndex = dec->mVarIndex;
					var->mIdent = dec->mIdent;
					var->mOffset = 0;
					var->mSize = dec->mSize;
					if ((dec->mFlags & DTF_VAR_ALIASING) || dec->mBase->mType == DT_TYPE_ARRAY)
						var->mAliased = true;
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA, dec->mAlignment);
					dec->mLinkerObject = var->mLinkerObject;
					var->mLinkerObject->AddData(dec->mData, dec->mSize);
					proc->mModule->mGlobalVars.Push(var);
				}

				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mIntConst = 0;
				ins->mConst.mVarIndex = dec->mVarIndex;
				assert(dec->mVarIndex >= 0);
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mMemory = IM_GLOBAL;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp, 1);
			}

			case DT_CONST_STRUCT:
			{
				if (!dec->mLinkerObject)
				{
					InterVariable	* var = new InterVariable();
					var->mOffset = 0;
					var->mSize = dec->mSize;
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA, dec->mAlignment);
					dec->mLinkerObject = var->mLinkerObject;
					var->mIdent = dec->mIdent;
					dec->mVarIndex = proc->mModule->mGlobalVars.Size();
					var->mIndex = dec->mVarIndex;
					proc->mModule->mGlobalVars.Push(var);

					uint8* d = var->mLinkerObject->AddSpace(dec->mSize);;

					BuildInitializer(proc->mModule, d, 0, dec, var);
				}

				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mIntConst = 0;
				ins->mConst.mVarIndex = dec->mVarIndex;
				assert(dec->mVarIndex >= 0);
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mMemory = IM_GLOBAL;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp, 1);
			}

			default:
				mErrors->Error(dec->mLocation, EERR_CONSTANT_TYPE, "Unimplemented constant type");
			}

			return ExValue(TheVoidTypeDeclaration);

		case EX_VARIABLE:
		{
			dec = exp->mDecValue;
			
			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			ins->mDst.mType = IT_POINTER;
			ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
			ins->mConst.mOperandSize = dec->mSize;
			ins->mConst.mIntConst = dec->mOffset;

			if (dec->mType == DT_VARIABLE_REF)
				dec = dec->mBase;

			ins->mConst.mVarIndex = dec->mVarIndex;

			int ref = 1;
			if (dec->mType == DT_ARGUMENT)
			{
				if (inlineMapper)
				{
					ins->mConst.mMemory = IM_LOCAL;
					ins->mConst.mVarIndex = inlineMapper->mParams[dec->mVarIndex];
				}
				else if (procType->mFlags & DTF_FASTCALL)
				{
					ins->mConst.mMemory = IM_FPARAM;
					ins->mConst.mVarIndex += procType->mFastCallBase;
				}
				else
					ins->mConst.mMemory = IM_PARAM;
				if (dec->mBase->mType == DT_TYPE_ARRAY)
				{
					ref = 2;
					ins->mConst.mOperandSize = 2;
				}
			}
			else if (dec->mFlags & DTF_GLOBAL)
			{
				InitGlobalVariable(proc->mModule, dec);
				ins->mConst.mMemory = IM_GLOBAL;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mVarIndex = dec->mVarIndex;
				assert(dec->mVarIndex >= 0);
			}
			else
			{
				if (inlineMapper)
					ins->mConst.mVarIndex += inlineMapper->mVarIndex;

				InitLocalVariable(proc, dec, ins->mConst.mVarIndex);

				ins->mConst.mMemory = IM_LOCAL;
			}

			block->Append(ins);

			if (!(dec->mBase->mFlags & DTF_DEFINED))
				mErrors->Error(dec->mLocation, EERR_VARIABLE_TYPE, "Undefined variable type");

			return ExValue(exp->mDecType, ins->mDst.mTemp, ref);
		}


		case EX_ASSIGNMENT:
		case EX_INITIALIZATION:
			{
			if (exp->mLeft->mDecType && exp->mLeft->mDecType->mType == DT_TYPE_STRUCT)
			{
				vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper, &vl);
			}
			else
			{
				vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
				vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			}

			if (exp->mToken == TK_ASSIGN || !(vl.mType->mType == DT_TYPE_POINTER && vr.mType->IsIntegerType() && (exp->mToken == TK_ASSIGN_ADD || exp->mToken == TK_ASSIGN_SUB)))
			{
				if (!vl.mType->CanAssign(vr.mType))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
			} 

			if (exp->mType != EX_INITIALIZATION && (vl.mType->mFlags & DTF_CONST))
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot assign to const type");

			if (vl.mType->mType == DT_TYPE_STRUCT || vl.mType->mType == DT_TYPE_ARRAY || vl.mType->mType == DT_TYPE_UNION)
			{
				vr = Dereference(proc, exp, block, vr, 1);
				vl = Dereference(proc, exp, block, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");
				if (vr.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an adressable expression");

				if (vr.mTemp != vl.mTemp)
				{
					InterInstruction* ins = new InterInstruction(exp->mLocation, IC_COPY);
					ins->mSrc[0].mType = IT_POINTER;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mSrc[0].mMemory = IM_INDIRECT;
					ins->mSrc[0].mOperandSize = vl.mType->mSize;
					ins->mSrc[0].mStride = vr.mType->mStripe;

					ins->mSrc[1].mType = IT_POINTER;
					ins->mSrc[1].mTemp = vl.mTemp;
					ins->mSrc[1].mMemory = IM_INDIRECT;
					ins->mSrc[1].mOperandSize = vl.mType->mSize;
					ins->mSrc[1].mStride = vl.mType->mStripe;
					ins->mConst.mOperandSize = vl.mType->mSize;
					block->Append(ins);
				}
			}
			else
			{
				if (vl.mType->mType == DT_TYPE_POINTER && (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION))
				{
					vr = Dereference(proc, exp, block, vr, 1);
					vr.mReference = 0;
					vr.mType = vl.mType;
				}
				else
					vr = Dereference(proc, exp, block, vr);

				vl = Dereference(proc, exp, block, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

				InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_STORE);

				if (exp->mToken != TK_ASSIGN)
				{
					ExValue	vll = Dereference(proc, exp, block, vl);

					if (vl.mType->mType == DT_TYPE_POINTER)
					{
						InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT);

						if (exp->mToken == TK_ASSIGN_ADD)
						{
							cins->mConst.mIntConst = vl.mType->Stride();
						}
						else if (exp->mToken == TK_ASSIGN_SUB)
						{
							cins->mConst.mIntConst = -vl.mType->Stride();
						}
						else
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer assignment");

						if (!vr.mType->IsIntegerType())
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Invalid argument for pointer inc/dec");

						cins->mDst.mType = IT_INT16;
						cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
						block->Append(cins);

						vr = CoerceType(proc, exp, block, vr, TheSignedIntTypeDeclaration);

						InterInstruction	*	mins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
						mins->mOperator = IA_MUL;
						mins->mSrc[0].mType = IT_INT16;
						mins->mSrc[0].mTemp = vr.mTemp;
						mins->mSrc[1].mType = IT_INT16;
						mins->mSrc[1].mTemp = cins->mDst.mTemp;
						mins->mDst.mType = IT_INT16;
						mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
						block->Append(mins);

						InterInstruction	*	ains = new InterInstruction(exp->mLocation, IC_LEA);
						ains->mSrc[1].mMemory = IM_INDIRECT;
						ains->mSrc[0].mType = IT_INT16;
						ains->mSrc[0].mTemp = mins->mDst.mTemp;
						ains->mSrc[1].mType = IT_POINTER;
						ains->mSrc[1].mTemp = vll.mTemp;
						ains->mDst.mType = IT_POINTER;
						ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
						block->Append(ains);

						vr.mTemp = ains->mDst.mTemp;
						vr.mType = vll.mType;
					}
					else
					{
						Declaration	*	otype = vll.mType;
#if 1
						if ((exp->mRight->mType != EX_CONSTANT || (exp->mToken != TK_ASSIGN_ADD && exp->mToken != TK_ASSIGN_SUB && exp->mToken != TK_ASSIGN_AND && exp->mToken != TK_ASSIGN_OR)) && otype->mSize < 2)
#else
						if (otype->mSize < 2)
#endif
						{
							if ((vll.mType->mFlags | vr.mType->mFlags) & DTF_SIGNED)
								otype = TheSignedIntTypeDeclaration;
							else
								otype = TheUnsignedIntTypeDeclaration;
							vll = CoerceType(proc, exp, block, vll, otype);
						}

						vr = CoerceType(proc, exp, block, vr, otype);

						InterInstruction	*	oins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
						oins->mSrc[0].mType = InterTypeOf(otype);
						oins->mSrc[0].mTemp = vr.mTemp;
						oins->mSrc[1].mType = InterTypeOf(otype);
						oins->mSrc[1].mTemp = vll.mTemp;
						oins->mDst.mType = InterTypeOf(otype);
						oins->mDst.mTemp = proc->AddTemporary(oins->mDst.mType);

						if (!vll.mType->IsNumericType())
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Left hand element type is not numeric");
						if (!vr.mType->IsNumericType())
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Right hand element type is not numeric");

						switch (exp->mToken)
						{
						case TK_ASSIGN_ADD:
							oins->mOperator = IA_ADD;
							break;
						case TK_ASSIGN_SUB:
							oins->mOperator = IA_SUB;
							break;
						case TK_ASSIGN_MUL:
							oins->mOperator = IA_MUL;
							break;
						case TK_ASSIGN_DIV:
							if (vll.mType->mFlags & DTF_SIGNED)
								oins->mOperator = IA_DIVS;
							else
								oins->mOperator = IA_DIVU;
							break;
						case TK_ASSIGN_MOD:
							if (vll.mType->mFlags & DTF_SIGNED)
								oins->mOperator = IA_MODS;
							else
								oins->mOperator = IA_MODU;
							break;
						case TK_ASSIGN_SHL:
							oins->mOperator = IA_SHL;
							break;
						case TK_ASSIGN_SHR:
							if (vll.mType->mFlags & DTF_SIGNED)
								oins->mOperator = IA_SAR;
							else
								oins->mOperator = IA_SHR;
							break;
						case TK_ASSIGN_AND:
							oins->mOperator = IA_AND;
							break;
						case TK_ASSIGN_XOR:
							oins->mOperator = IA_XOR;
							break;
						case TK_ASSIGN_OR:
							oins->mOperator = IA_OR;
							break;
						}

						vr.mTemp = oins->mDst.mTemp;
						vr.mType = otype;

						block->Append(oins);

						vr = CoerceType(proc, exp, block, vr, vl.mType);
					}
				}
				else
				{
					vr = CoerceType(proc, exp, block, vr, vl.mType);
				}

				ins->mCode = IC_STORE;
				ins->mSrc[1].mMemory = IM_INDIRECT;
				ins->mSrc[0].mType = InterTypeOf(vr.mType);
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mSrc[1].mType = IT_POINTER;
				ins->mSrc[1].mTemp = vl.mTemp;
				ins->mSrc[1].mOperandSize = vl.mType->mSize;
				ins->mSrc[1].mStride = vl.mType->mStripe;
				ins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
				block->Append(ins);
			}
			}

			return vr;

		case EX_INDEX:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);

			int stride = vl.mType->Stride();

			if (vl.mType->mType == DT_TYPE_ARRAY && exp->mRight->mType == EX_CONSTANT && exp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				if (vl.mType->mFlags & DTF_DEFINED)
				{
					int64	index = exp->mRight->mDecValue->mInteger;
					if (index < 0 || index * stride >= vl.mType->mSize)
						mErrors->Error(exp->mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Constant array index out of bounds");
				}
			}

			vl = Dereference(proc, exp, block, vl, vl.mType->mType == DT_TYPE_POINTER ? 0 : 1);
			vr = Dereference(proc, exp, block, vr);

			if (vl.mType->mType != DT_TYPE_ARRAY && vl.mType->mType != DT_TYPE_POINTER)
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Invalid type for indexing");

			if (!vr.mType->IsIntegerType())
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Index operand is not integral number");

			vr = CoerceType(proc, exp, block, vr, TheSignedIntTypeDeclaration);

			InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			cins->mConst.mIntConst = stride;
			cins->mDst.mType = IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			block->Append(cins);

			InterInstruction	*	mins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
			mins->mOperator = IA_MUL;
			mins->mSrc[0].mType = IT_INT16;
			mins->mSrc[0].mTemp = vr.mTemp;
			mins->mSrc[1].mType = IT_INT16;
			mins->mSrc[1].mTemp = cins->mDst.mTemp;
			mins->mDst.mType = IT_INT16;
			mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
			block->Append(mins);

			InterInstruction	*	ains = new InterInstruction(exp->mLocation, IC_LEA);
			ains->mSrc[1].mMemory = IM_INDIRECT;
			ains->mSrc[0].mType = IT_INT16;
			ains->mSrc[0].mTemp = mins->mDst.mTemp;
			ains->mSrc[1].mType = IT_POINTER;
			ains->mSrc[1].mTemp = vl.mTemp;
			ains->mDst.mType = IT_POINTER;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			block->Append(ains);

			return ExValue(vl.mType->mBase, ains->mDst.mTemp, 1);
		}

		case EX_QUALIFY:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

			InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			cins->mConst.mIntConst = exp->mDecValue->mOffset;
			cins->mDst.mType = IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			block->Append(cins);

			InterInstruction	*	ains = new InterInstruction(exp->mLocation, IC_LEA);
			ains->mSrc[1].mMemory = IM_INDIRECT;
			ains->mSrc[0].mType = IT_INT16;
			ains->mSrc[0].mTemp = cins->mDst.mTemp;
			ains->mSrc[1].mType = IT_POINTER;
			ains->mSrc[1].mTemp = vl.mTemp;
			ains->mDst.mType = IT_POINTER;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			block->Append(ains);

			return ExValue(exp->mDecValue->mBase, ains->mDst.mTemp, 1);
		}

		case EX_BINARY:
			{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
			vr = Dereference(proc, exp, block, vr);

			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);

			if (vl.mType->mType == DT_TYPE_POINTER || vl.mType->mType == DT_TYPE_ARRAY)
			{
				if (vl.mType->mType == DT_TYPE_POINTER)
					vl = Dereference(proc, exp, block, vl);
				else
				{
					vl = Dereference(proc, exp, block, vl, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vl.mType->mBase;
					ptype->mSize = 2;
					ptype->mStride = vl.mType->mStride;
					vl.mType = ptype;
				}

				if (vr.mType->IsIntegerType())
				{
					InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT);

					if (exp->mToken == TK_ADD)
					{
						cins->mConst.mIntConst = vl.mType->Stride();
					}
					else if (exp->mToken == TK_SUB)
					{
						cins->mConst.mIntConst = -vl.mType->Stride();
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");

					cins->mDst.mType = IT_INT16;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
					block->Append(cins);

					vr = CoerceType(proc, exp, block, vr, TheSignedIntTypeDeclaration);

					InterInstruction	*	mins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
					mins->mCode = IC_BINARY_OPERATOR;
					mins->mOperator = IA_MUL;
					mins->mSrc[0].mType = IT_INT16;
					mins->mSrc[0].mTemp = vr.mTemp;
					mins->mSrc[1].mType = IT_INT16;
					mins->mSrc[1].mTemp = cins->mDst.mTemp;
					mins->mDst.mType = IT_INT16;
					mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
					block->Append(mins);

					ins->mCode = IC_LEA;
					ins->mSrc[1].mMemory = IM_INDIRECT;
					ins->mSrc[0].mType = IT_INT16;
					ins->mSrc[0].mTemp = mins->mDst.mTemp;
					ins->mSrc[1].mType = IT_POINTER;
					ins->mSrc[1].mTemp = vl.mTemp;
					ins->mDst.mType = IT_POINTER;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);
				}
				else if (vr.mType->IsSame(vl.mType))
				{
					if (exp->mToken == TK_SUB)
					{
						InterInstruction	*	clins = new InterInstruction(exp->mLocation, IC_TYPECAST), *	crins = new InterInstruction(exp->mLocation, IC_TYPECAST);
						clins->mSrc[0].mTemp = vl.mTemp;
						clins->mSrc[0].mType = IT_POINTER;
						clins->mDst.mType = IT_INT16;
						clins->mDst.mTemp = proc->AddTemporary(clins->mDst.mType);
						block->Append(clins);

						crins->mSrc[0].mTemp = vr.mTemp;
						crins->mSrc[0].mType = IT_POINTER;
						crins->mDst.mType = IT_INT16;
						crins->mDst.mTemp = proc->AddTemporary(crins->mDst.mType);
						block->Append(crins);

						InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT);
						cins->mConst.mIntConst = vl.mType->Stride();
						cins->mDst.mType = IT_INT16;
						cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
						block->Append(cins);

						InterInstruction	*	sins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR), *	dins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
						sins->mOperator = IA_SUB;
						sins->mSrc[0].mType = IT_INT16;
						sins->mSrc[0].mTemp = crins->mDst.mTemp;
						sins->mSrc[1].mType = IT_INT16;
						sins->mSrc[1].mTemp = clins->mDst.mTemp;
						sins->mDst.mType = IT_INT16;
						sins->mDst.mTemp = proc->AddTemporary(sins->mDst.mType);
						block->Append(sins);

						dins->mOperator = IA_DIVS;
						dins->mSrc[0].mType = IT_INT16;
						dins->mSrc[0].mTemp = cins->mDst.mTemp;
						dins->mSrc[1].mType = IT_INT16;
						dins->mSrc[1].mTemp = sins->mDst.mTemp;
						dins->mDst.mType = IT_INT16;
						dins->mDst.mTemp = proc->AddTemporary(dins->mDst.mType);
						block->Append(dins);

						return ExValue(TheSignedIntTypeDeclaration, dins->mDst.mTemp);
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");
				}
				else
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");
			}
			else if (vr.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_ARRAY)
			{
				if (vr.mType->mType == DT_TYPE_POINTER)
					vr = Dereference(proc, exp, block, vr);
				else
				{
					vr = Dereference(proc, exp, block, vr, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vr.mType->mBase;
					ptype->mSize = 2;
					vr.mType = ptype;
				}

				if (vl.mType->IsIntegerType())
				{
					InterInstruction* cins = new InterInstruction(exp->mLocation, IC_CONSTANT);

					if (exp->mToken == TK_ADD)
					{
						cins->mConst.mIntConst = vr.mType->Stride();
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");

					cins->mDst.mType = IT_INT16;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
					block->Append(cins);

					vl = CoerceType(proc, exp, block, vl, TheSignedIntTypeDeclaration);

					InterInstruction* mins = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR);
					mins->mOperator = IA_MUL;
					mins->mSrc[0].mType = IT_INT16;
					mins->mSrc[0].mTemp = vl.mTemp;
					mins->mSrc[1].mType = IT_INT16;
					mins->mSrc[1].mTemp = cins->mDst.mTemp;
					mins->mDst.mType = IT_INT16;
					mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
					block->Append(mins);

					ins->mCode = IC_LEA;
					ins->mSrc[1].mMemory = IM_INDIRECT;
					ins->mSrc[0].mType = IT_INT16;
					ins->mSrc[0].mTemp = mins->mDst.mTemp;
					ins->mSrc[1].mType = IT_POINTER;
					ins->mSrc[1].mTemp = vr.mTemp;
					ins->mDst.mType = IT_POINTER;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);

					vl = vr;
				}
				else
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");
			}
			else
			{
				vl = Dereference(proc, exp, block, vl);

				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Left hand operand type is not numeric");
				if (!vr.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Right hand operand type is not numeric");

				Declaration* dtype;
				if (vr.mType->mType == DT_TYPE_FLOAT || vl.mType->mType == DT_TYPE_FLOAT)
					dtype = TheFloatTypeDeclaration;
				else if ((exp->mToken == TK_BINARY_AND || exp->mToken == TK_BINARY_OR || exp->mToken == TK_BINARY_XOR) && vl.mType->mSize == 1 && vr.mType->mSize == 1)
				{
					if (vl.mType->mFlags & DTF_SIGNED)
						dtype = vl.mType;
					else
						dtype = vr.mType;
				}
				else if (exp->mToken == TK_LEFT_SHIFT || exp->mToken == TK_RIGHT_SHIFT)
				{
					if (vl.mType->mFlags & DTF_SIGNED)
					{
						if (vl.mType->mSize == 4)
							dtype = TheSignedLongTypeDeclaration;
						else
							dtype = TheSignedIntTypeDeclaration;
					}
					else
					{
						if (vl.mType->mSize == 4 || vr.mType->mSize == 4)
							dtype = TheUnsignedLongTypeDeclaration;
						else
							dtype = TheUnsignedIntTypeDeclaration;
					}
				}
				else if (vr.mType->mSize < vl.mType->mSize && (vl.mType->mFlags & DTF_SIGNED))
				{
					if (vl.mType->mSize == 4)
						dtype = TheSignedLongTypeDeclaration;
					else
						dtype = TheSignedIntTypeDeclaration;
				}
				else if (vl.mType->mSize < vr.mType->mSize && (vr.mType->mFlags & DTF_SIGNED))
				{
					if (vr.mType->mSize == 4)
						dtype = TheSignedLongTypeDeclaration;
					else
						dtype = TheSignedIntTypeDeclaration;
				}
				else if ((vr.mType->mFlags & DTF_SIGNED) && (vl.mType->mFlags & DTF_SIGNED))
				{
					if (vl.mType->mSize == 4)
						dtype = TheSignedLongTypeDeclaration;
					else
						dtype = TheSignedIntTypeDeclaration;
				}
				else
				{
					if (vl.mType->mSize == 4 || vr.mType->mSize == 4)
						dtype = TheUnsignedLongTypeDeclaration;
					else
						dtype = TheUnsignedIntTypeDeclaration;
				}

				// Convert bool to char on binary operation
				if (dtype->mType == DT_TYPE_BOOL)
				{
					if (exp->mToken == TK_BINARY_AND || exp->mToken == TK_BINARY_OR)
						mErrors->Error(exp->mLocation, EWARN_BOOL_SHORTCUT, "Binary and/or operator used for boolean values, use logical operator ('&&' '||').");
					dtype = TheUnsignedCharTypeDeclaration;
				}

				vl = CoerceType(proc, exp, block, vl, dtype);
				vr = CoerceType(proc, exp, block, vr, dtype);

				bool	signedOP = dtype->mFlags & DTF_SIGNED;

				ins->mCode = IC_BINARY_OPERATOR;
				switch (exp->mToken)
				{
				case TK_ADD:
					ins->mOperator = IA_ADD;
					break;
				case TK_SUB:
					ins->mOperator = IA_SUB;
					break;
				case TK_MUL:
					ins->mOperator = IA_MUL;
					break;
				case TK_DIV:
					ins->mOperator = signedOP ? IA_DIVS : IA_DIVU;
					break;
				case TK_MOD:
					ins->mOperator = signedOP ? IA_MODS : IA_MODU;
					break;
				case TK_LEFT_SHIFT:
					ins->mOperator = IA_SHL;
					break;
				case TK_RIGHT_SHIFT:
					ins->mOperator = (vl.mType->mFlags & DTF_SIGNED) ? IA_SAR : IA_SHR;
					break;
				case TK_BINARY_AND:
					ins->mOperator = IA_AND;
					break;
				case TK_BINARY_OR:
					ins->mOperator = IA_OR;
					break;
				case TK_BINARY_XOR:
					ins->mOperator = IA_XOR;
					break;
				}

				ins->mSrc[0].mType = InterTypeOf(vr.mType);;
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mSrc[1].mType = InterTypeOf(vl.mType);;
				ins->mSrc[1].mTemp = vl.mTemp;
				ins->mDst.mType = InterTypeOfArithmetic(ins->mSrc[0].mType, ins->mSrc[1].mType);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);
			}

			return ExValue(vl.mType, ins->mDst.mTemp);
		}


		case EX_PREINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT), *	ains = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR), *	sins = new InterInstruction(exp->mLocation, IC_STORE);

			ExValue	vdl = Dereference(proc, exp, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mDst.mType = ftype ? IT_FLOAT : IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mConst.mIntConst = exp->mToken == TK_INC ? vdl.mType->Stride() : -(vdl.mType->Stride());
			else if (vdl.mType->IsNumericType())
				cins->mConst.mIntConst = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric value or pointer");

			block->Append(cins);

			InterType	ttype = InterTypeOf(vdl.mType);
			if (ttype == IT_POINTER)
			{
				ains->mCode = IC_LEA;
				ains->mSrc[1].mMemory = IM_INDIRECT;
			}
			else
			{
				ains->mCode = IC_BINARY_OPERATOR;
				ains->mOperator = IA_ADD;
			}
			ains->mSrc[0].mType = cins->mDst.mType;
			ains->mSrc[0].mTemp = cins->mDst.mTemp;
			ains->mSrc[1].mType = ttype;
			ains->mSrc[1].mTemp = vdl.mTemp;
			ains->mDst.mType = ttype;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			block->Append(ains);

			sins->mSrc[1].mMemory = IM_INDIRECT;
			sins->mSrc[0].mType = ains->mDst.mType;
			sins->mSrc[0].mTemp = ains->mDst.mTemp;
			sins->mSrc[1].mType = IT_POINTER;
			sins->mSrc[1].mTemp = vl.mTemp;
			sins->mSrc[1].mOperandSize = vl.mType->mSize;
			sins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
			block->Append(sins);

			// Return reference to value
			return ExValue(vl.mType, vl.mTemp, 1);
		}
		break;

		case EX_POSTINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CONSTANT), *	ains = new InterInstruction(exp->mLocation, IC_BINARY_OPERATOR), *	sins = new InterInstruction(exp->mLocation, IC_STORE);

			ExValue	vdl = Dereference(proc, exp, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mDst.mType = ftype ? IT_FLOAT : IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mConst.mIntConst = exp->mToken == TK_INC ? vdl.mType->Stride() : -(vdl.mType->Stride());
			else if (vdl.mType->IsNumericType())
				cins->mConst.mIntConst = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric value or pointer");
			block->Append(cins);

			InterType	ttype = InterTypeOf(vdl.mType);
			if (ttype == IT_POINTER)
			{
				ains->mCode = IC_LEA;
				ains->mSrc[1].mMemory = IM_INDIRECT;
			}
			else
			{
				ains->mCode = IC_BINARY_OPERATOR;
				ains->mOperator = IA_ADD;
			}
			ains->mSrc[0].mType = cins->mDst.mType;
			ains->mSrc[0].mTemp = cins->mDst.mTemp;
			ains->mSrc[1].mType = ttype;
			ains->mSrc[1].mTemp = vdl.mTemp;
			ains->mDst.mType = ttype;
			ains->mDst.mTemp = proc->AddTemporary(ttype);
			block->Append(ains);

			sins->mSrc[1].mMemory = IM_INDIRECT;
			sins->mSrc[0].mType = ains->mDst.mType;
			sins->mSrc[0].mTemp = ains->mDst.mTemp;
			sins->mSrc[1].mType = IT_POINTER;
			sins->mSrc[1].mTemp = vl.mTemp;
			sins->mSrc[1].mOperandSize = vl.mType->mSize;
			sins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
			block->Append(sins);

			return ExValue(vdl.mType, vdl.mTemp);
		}
		break;

		case EX_PREFIX:
		{	
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);

			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_UNARY_OPERATOR);

			switch (exp->mToken)
			{
			case TK_ADD:
				vl = Dereference(proc, exp, block, vl);
				ins->mOperator = IA_NONE;
				break;
			case TK_SUB:
				vl = Dereference(proc, exp, block, vl);
				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric type");
				else if (vl.mType->mType == DT_TYPE_INTEGER && vl.mType->mSize < 2)
					vl = CoerceType(proc, exp, block, vl, TheSignedIntTypeDeclaration);
				ins->mOperator = IA_NEG;
				break;
			case TK_BINARY_NOT:
				vl = Dereference(proc, exp, block, vl);
				if (!(vl.mType->mType == DT_TYPE_POINTER || vl.mType->IsNumericType()))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric or pointer type");
				else if (vl.mType->mType == DT_TYPE_INTEGER && vl.mType->mSize < 2)
					vl = CoerceType(proc, exp, block, vl, TheUnsignedIntTypeDeclaration);
				ins->mOperator = IA_NOT;
				break;
			case TK_MUL:
				if (vl.mType->mType == DT_TYPE_ARRAY)
					vl = Dereference(proc, exp, block, vl, 1);
				else if (vl.mType->mType != DT_TYPE_POINTER)
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a pointer type");
				else if (vl.mType->mStride != 1)
					vl = Dereference(proc, exp, block, vl, 0);
				return ExValue(vl.mType->mBase, vl.mTemp, vl.mReference + 1);
			case TK_BINARY_AND:
			{
				if (vl.mReference < 1)
					mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable value");

				Declaration* dec = new Declaration(exp->mLocation, DT_TYPE_POINTER);
				dec->mBase = vl.mType;
				dec->mSize = 2;
				dec->mFlags = DTF_DEFINED;
				return ExValue(dec, vl.mTemp, vl.mReference - 1);
			}
			}
			
			ins->mSrc[0].mType = InterTypeOf(vl.mType);
			ins->mSrc[0].mTemp = vl.mTemp;
			ins->mDst.mType = ins->mSrc[0].mType;
			ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);

			block->Append(ins);

			return ExValue(vl.mType, ins->mDst.mTemp);
		}

		case EX_RELATIONAL:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl, vl.mType->mType == DT_TYPE_ARRAY ? 1 : 0);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
			vr = Dereference(proc, exp, block, vr, vr.mType->mType == DT_TYPE_ARRAY ? 1 : 0);

			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_RELATIONAL_OPERATOR);

			Declaration* dtype = TheSignedIntTypeDeclaration;

			if (vl.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_POINTER)
			{
				dtype = vl.mType;
				if (vl.mType->IsIntegerType() || vr.mType->IsIntegerType())
					;
				else if ((vl.mType->mType == DT_TYPE_POINTER || vl.mType->mType == DT_TYPE_ARRAY) && (vr.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_ARRAY))
				{
					if (vl.mType->mBase->mType == DT_TYPE_VOID || vr.mType->mBase->mType == DT_TYPE_VOID)
						;
					else if (!vl.mType->mBase->IsConstSame(vr.mType->mBase))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible pointer types");
				}
				else
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible pointer types");
			}
			else if (!vl.mType->IsNumericType() || !vr.mType->IsNumericType())
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric or pointer type");
			else if (vr.mType->mType == DT_TYPE_FLOAT || vl.mType->mType == DT_TYPE_FLOAT)
				dtype = TheFloatTypeDeclaration;
			else if (vr.mType->mSize < vl.mType->mSize && (vl.mType->mFlags & DTF_SIGNED))
			{
				if (vl.mType->mSize == 4)
					dtype = TheSignedLongTypeDeclaration;
				else
					dtype = TheSignedIntTypeDeclaration;
			}
			else if (vl.mType->mSize < vr.mType->mSize && (vr.mType->mFlags & DTF_SIGNED))
			{
				if (vr.mType->mSize == 4)
					dtype = TheSignedLongTypeDeclaration;
				else
					dtype = TheSignedIntTypeDeclaration;
			}
			else if ((vr.mType->mFlags & DTF_SIGNED) && (vl.mType->mFlags & DTF_SIGNED))
			{
				if (vl.mType->mSize == 4)
					dtype = TheSignedLongTypeDeclaration;
				else
					dtype = TheSignedIntTypeDeclaration;
			}
			else
			{
				if (vl.mType->mSize == 4 || vr.mType->mSize == 4)
					dtype = TheUnsignedLongTypeDeclaration;
				else
					dtype = TheUnsignedIntTypeDeclaration;
			}

			vl = CoerceType(proc, exp, block, vl, dtype);
			vr = CoerceType(proc, exp, block, vr, dtype);

			bool	signedCompare = dtype->mFlags & DTF_SIGNED;

			switch (exp->mToken)
			{
			case TK_EQUAL:
				ins->mOperator = IA_CMPEQ;
				break;
			case TK_NOT_EQUAL:
				ins->mOperator = IA_CMPNE;
				break;
			case TK_GREATER_THAN:
				ins->mOperator = signedCompare ? IA_CMPGS : IA_CMPGU;
				break;
			case TK_GREATER_EQUAL:
				ins->mOperator = signedCompare ? IA_CMPGES : IA_CMPGEU;
				break;
			case TK_LESS_THAN:
				ins->mOperator = signedCompare ? IA_CMPLS : IA_CMPLU;
				break;
			case TK_LESS_EQUAL:
				ins->mOperator = signedCompare ? IA_CMPLES : IA_CMPLEU;
				break;
			}
			ins->mSrc[0].mType = InterTypeOf(vr.mType);;
			ins->mSrc[0].mTemp = vr.mTemp;
			ins->mSrc[1].mType = InterTypeOf(vl.mType);;
			ins->mSrc[1].mTemp = vl.mTemp;
			ins->mDst.mType = IT_BOOL;
			ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);

			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins->mDst.mTemp);
		}

		case EX_CALL:
		{
			if (exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION && (exp->mLeft->mDecValue->mFlags & DTF_INTRINSIC))
			{
				Declaration* decf = exp->mLeft->mDecValue;
				const Ident	*	iname = decf->mIdent;

				if (!strcmp(iname->mString, "fabs"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_UNARY_OPERATOR);
					ins->mOperator = IA_ABS;
					ins->mSrc[0].mType = IT_FLOAT;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mDst.mType = IT_FLOAT;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mDst.mTemp);
				}
				else if (!strcmp(iname->mString, "floor"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_UNARY_OPERATOR);
					ins->mOperator = IA_FLOOR;
					ins->mSrc[0].mType = IT_FLOAT;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mDst.mType = IT_FLOAT;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mDst.mTemp);
				}
				else if (!strcmp(iname->mString, "ceil"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_UNARY_OPERATOR);
					ins->mOperator = IA_CEIL;
					ins->mSrc[0].mType = IT_FLOAT;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mDst.mType = IT_FLOAT;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mDst.mTemp);
				}
				else if (!strcmp(iname->mString, "strcpy"))
				{
					if (exp->mRight->mType == EX_LIST)
					{
						Expression* tex = exp->mRight->mLeft, * sex = exp->mRight->mRight;
						if ((tex->mDecType->mType == DT_TYPE_ARRAY && tex->mDecType->mSize <= 256) ||
							(sex->mDecType->mType == DT_TYPE_ARRAY && sex->mDecType->mSize <= 256))
						{
							vl = TranslateExpression(procType, proc, block, tex, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, vl, 1);
							else
								vl = Dereference(proc, exp, block, vl);

							vr = TranslateExpression(procType, proc, block, sex, breakBlock, continueBlock, inlineMapper);
							if (vr.mType->mType == DT_TYPE_ARRAY)
								vr = Dereference(proc, exp, block, vr, 1);
							else
								vr = Dereference(proc, exp, block, vr);

							if (!TheCharPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							if (!TheConstCharPointerTypeDeclaration->CanAssign(vr.mType))
								mErrors->Error(sex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction* ins = new InterInstruction(exp->mLocation, IC_STRCPY);
							ins->mSrc[0].mType = IT_POINTER;
							ins->mSrc[0].mMemory = IM_INDIRECT;
							ins->mSrc[0].mTemp = vr.mTemp;
							ins->mSrc[1].mType = IT_POINTER;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mTemp = vl.mTemp;
							block->Append(ins);

							return vl;
						}
					}
				}
				else if (!strcmp(iname->mString, "memcpy"))
				{
					if (exp->mRight->mType == EX_LIST)
					{
						Expression* tex = exp->mRight->mLeft, * sex = exp->mRight->mRight->mLeft, * nex = exp->mRight->mRight->mRight;
						if (nex && nex->mType == EX_CONSTANT && nex->mDecValue->mType == DT_CONST_INTEGER && nex->mDecValue->mInteger < 128)
						{
							vl = TranslateExpression(procType, proc, block, tex, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, vl, 1);
							else
								vl = Dereference(proc, exp, block, vl);

							vr = TranslateExpression(procType, proc, block, sex, breakBlock, continueBlock, inlineMapper);
							if (vr.mType->mType == DT_TYPE_ARRAY)
								vr = Dereference(proc, exp, block, vr, 1);
							else
								vr = Dereference(proc, exp, block, vr);

							if (!TheCharPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							if (!TheConstCharPointerTypeDeclaration->CanAssign(vr.mType))
								mErrors->Error(sex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction* ins = new InterInstruction(exp->mLocation, IC_COPY);
							ins->mSrc[0].mType = IT_POINTER;
							ins->mSrc[0].mMemory = IM_INDIRECT;
							ins->mSrc[0].mTemp = vr.mTemp;
							ins->mSrc[1].mType = IT_POINTER;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mTemp = vl.mTemp;
							ins->mSrc[0].mOperandSize = nex->mDecValue->mInteger;
							ins->mSrc[1].mOperandSize = nex->mDecValue->mInteger;
							ins->mConst.mOperandSize = nex->mDecValue->mInteger;
							block->Append(ins);

							return vl;
						}
					}
				}
				else
				{
					mErrors->Error(exp->mLeft->mDecValue->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown intrinsic function", iname);
					return ExValue(TheVoidTypeDeclaration);
				}
			}

			bool	canInline = exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION && !(inlineMapper && inlineMapper->mDepth > 10);
			bool	doInline = false, inlineConstexpr = false;

			if (canInline)
			{
				if (inlineMapper && inlineMapper->mConstExpr)
					inlineConstexpr = true;
				else if (exp->mLeft->mDecValue->mFlags & DTF_FUNC_CONSTEXPR)
				{
					Expression* pex = exp->mRight;
					inlineConstexpr = true;
					while (inlineConstexpr && pex)
					{
						if (pex->mType == EX_LIST)
						{
							if (pex->mLeft->mType != EX_CONSTANT)
								inlineConstexpr = false;
							pex = pex->mRight;
						}
						else
						{
							if (pex->mType != EX_CONSTANT)
								inlineConstexpr = false;
							pex = nullptr;
						}
					}
				}

				if (inlineConstexpr)
					doInline = true;
				else if (exp->mLeft->mDecValue->mFlags & DTF_INLINE)
				{
					if (proc->mNativeProcedure || !(exp->mLeft->mDecValue->mFlags & DTF_NATIVE))
						doInline = true;
				}
			}

			if (doInline)
			{
				return TranslateInline(procType, proc, block, exp, breakBlock, continueBlock, inlineMapper, inlineConstexpr);
			}
			else
			{
				vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
				vl = Dereference(proc, exp, block, vl);

				int	atotal = 0;

				InterCodeBasicBlock* fblock = block;

				Declaration* ftype = vl.mType;
				if (ftype->mType == DT_TYPE_POINTER)
					ftype = ftype->mBase;

				InterInstruction* fins = nullptr;

				if (ftype->mFlags & DTF_FASTCALL)
				{

				}
				else
				{
					fins = new InterInstruction(exp->mLocation, IC_PUSH_FRAME);
					fins->mConst.mIntConst = atotal;
					block->Append(fins);
				}

				Declaration	* decResult = nullptr;

				if (ftype->mBase->mType == DT_TYPE_STRUCT)
				{
					int	ttemp;
					if (lrexp)
					{
						ttemp = lrexp->mTemp;

						decResult = lrexp->mType;
					}
					else
					{
						int	nindex = proc->mNumLocals++;

						Declaration* vdec = new Declaration(exp->mLocation, DT_VARIABLE);

						vdec->mVarIndex = nindex;
						vdec->mBase = ftype->mBase;
						vdec->mSize = ftype->mBase->mSize;

						decResult = vdec;

						InterInstruction* vins = new InterInstruction(exp->mLocation, IC_CONSTANT);
						vins->mDst.mType = IT_POINTER;
						vins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						vins->mConst.mMemory = IM_LOCAL;
						vins->mConst.mVarIndex = nindex;
						vins->mConst.mOperandSize = ftype->mBase->mSize;
						block->Append(vins);

						ttemp = vins->mDst.mTemp;
					}

					InterInstruction* ains = new InterInstruction(exp->mLocation, IC_CONSTANT);
					ains->mDst.mType = IT_POINTER;
					ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
					ains->mConst.mVarIndex = 0;
					ains->mConst.mIntConst = 0;
					ains->mConst.mOperandSize = 2;
					if (ftype->mFlags & DTF_FASTCALL) 
					{
						ains->mConst.mMemory = IM_FFRAME;
						ains->mConst.mVarIndex += ftype->mFastCallBase;
					}
					else
						ains->mConst.mMemory = IM_FRAME;
					block->Append(ains);

					InterInstruction* wins = new InterInstruction(exp->mLocation, IC_STORE);
					wins->mSrc[1].mMemory = IM_INDIRECT;
					wins->mSrc[0].mType = IT_POINTER;
					wins->mSrc[0].mTemp = ttemp;
					wins->mSrc[1].mType = IT_POINTER;
					wins->mSrc[1].mTemp = ains->mDst.mTemp;
					wins->mSrc[1].mOperandSize = 2;
					block->Append(wins);

					atotal = 2;
				}

				if (exp->mLeft->mDecValue && exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION)
					proc->AddCalledFunction(proc->mModule->mProcedures[exp->mLeft->mDecValue->mVarIndex]);
				else
					proc->CallsFunctionPointer();

				GrowingArray<InterInstruction*>	defins(nullptr);

				Declaration* pdec = ftype->mParams;
				Expression* pex = exp->mRight;
				while (pex)
				{
					InterInstruction	*	ains = new InterInstruction(exp->mLocation, IC_CONSTANT);
					ains->mDst.mType = IT_POINTER;
					ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
					if (pdec)
					{
						ains->mConst.mVarIndex = pdec->mVarIndex;
						ains->mConst.mIntConst = pdec->mOffset;
						if (pdec->mBase->mType == DT_TYPE_ARRAY)
							ains->mConst.mOperandSize = 2;
						else
							ains->mConst.mOperandSize = pdec->mSize;
					}
					else if (ftype->mFlags & DTF_VARIADIC)
					{
						ains->mConst.mVarIndex = atotal;
						ains->mConst.mIntConst = 0;
						ains->mConst.mOperandSize = 2;
					}
					else
					{
						mErrors->Error(pex->mLocation, EERR_WRONG_PARAMETER, "Too many arguments for function call");
					}

					if (ftype->mFlags & DTF_FASTCALL)
					{
						ains->mConst.mMemory = IM_FFRAME;
						ains->mConst.mIntConst = 0;
						ains->mConst.mVarIndex += ftype->mFastCallBase;
					}
					else
						ains->mConst.mMemory = IM_FRAME;

					block->Append(ains);

					Expression* texp;

					if (pex->mType == EX_LIST)
					{
						texp = pex->mLeft;
						pex = pex->mRight;
					}
					else
					{
						texp = pex;
						pex = nullptr;
					}

					ExValue	vp(pdec ? pdec->mBase : TheSignedIntTypeDeclaration, ains->mDst.mTemp, 1);

					vr = TranslateExpression(procType, proc, block, texp, breakBlock, continueBlock, inlineMapper, &vp);

					if (vr.mType->mType == DT_TYPE_STRUCT || vr.mType->mType == DT_TYPE_UNION)
					{
						if (pdec && !pdec->mBase->CanAssign(vr.mType))
							mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

						vr = Dereference(proc, texp, block, vr, 1);

						if (vr.mReference != 1)
							mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an adressable expression");

						if (vp.mTemp != vr.mTemp)
						{
							InterInstruction* cins = new InterInstruction(texp->mLocation, IC_COPY);
							cins->mSrc[0].mType = IT_POINTER;
							cins->mSrc[0].mTemp = vr.mTemp;
							cins->mSrc[0].mMemory = IM_INDIRECT;
							cins->mSrc[0].mOperandSize = vr.mType->mSize;
							cins->mSrc[0].mStride = vr.mType->mStripe;

							cins->mSrc[1].mOperandSize = vr.mType->mSize;
							cins->mSrc[1].mType = IT_POINTER;
							cins->mSrc[1].mTemp = ains->mDst.mTemp;
							cins->mSrc[1].mMemory = IM_INDIRECT;

							cins->mConst.mOperandSize = vr.mType->mSize;
							block->Append(cins);
						}

						atotal += vr.mType->mSize;
					}
					else
					{
						if (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION)
							vr = Dereference(proc, texp, block, vr, 1);
						else if (pdec && pdec->mBase->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_INTEGER && texp->mType == EX_CONSTANT && texp->mDecValue->mType == DT_CONST_INTEGER && texp->mDecValue->mInteger == 0)
							vr = CoerceType(proc, texp, block, vr, pdec->mBase);
						else
							vr = Dereference(proc, texp, block, vr);

						if (pdec)
						{
							if (!pdec->mBase->CanAssign(vr.mType))
							{
								pdec->mBase->CanAssign(vr.mType);
								mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							}
							vr = CoerceType(proc, texp, block, vr, pdec->mBase);
						}
						else if (vr.mType->IsIntegerType() && vr.mType->mSize < 2)
						{
							vr = CoerceType(proc, texp, block, vr, TheSignedIntTypeDeclaration);
						}


						InterInstruction* wins = new InterInstruction(texp->mLocation, IC_STORE);
						wins->mSrc[1].mMemory = IM_INDIRECT;
						wins->mSrc[0].mType = InterTypeOf(vr.mType);;
						wins->mSrc[0].mTemp = vr.mTemp;
						wins->mSrc[1].mType = IT_POINTER;
						wins->mSrc[1].mTemp = ains->mDst.mTemp;
						if (pdec)
						{
							if (pdec->mBase->mType == DT_TYPE_ARRAY)
								wins->mSrc[1].mOperandSize = 2;
							else
								wins->mSrc[1].mOperandSize = pdec->mSize;
						}
						else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
							wins->mSrc[1].mOperandSize = vr.mType->mSize;
						else
							wins->mSrc[1].mOperandSize = 2;

						if (ftype->mFlags & DTF_FASTCALL)
							defins.Push(wins);
						else
							block->Append(wins);

						atotal += wins->mSrc[1].mOperandSize;
					}

					if (pdec)
						pdec = pdec->mNext;
				}

				if (pdec)
					mErrors->Error(exp->mLocation, EERR_WRONG_PARAMETER, "Not enough arguments for function call");

				for (int i = 0; i < defins.Size(); i++)
					block->Append(defins[i]);

				InterInstruction	*	cins = new InterInstruction(exp->mLocation, IC_CALL);
				if (exp->mLeft->mDecValue && exp->mLeft->mDecValue->mFlags & DTF_NATIVE)
					cins->mCode = IC_CALL_NATIVE;
				else
					cins->mCode = IC_CALL;

				if (exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mFlags & DTF_FUNC_CONSTEXPR)
					cins->mConstExpr = true;

				cins->mSrc[0].mType = IT_POINTER;
				cins->mSrc[0].mTemp = vl.mTemp;
				if (ftype->mBase->mType != DT_TYPE_VOID && ftype->mBase->mType != DT_TYPE_STRUCT)
				{
					cins->mDst.mType = InterTypeOf(ftype->mBase);
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
				}

				block->Append(cins);


				if (ftype->mFlags & DTF_FASTCALL)
				{
					
				}
				else
				{
					fins->mConst.mIntConst = atotal;

					InterInstruction* xins = new InterInstruction(exp->mLocation, IC_POP_FRAME);
					xins->mConst.mIntConst = atotal;
					block->Append(xins);
				}

				if (decResult)
				{
					if (lrexp)
						return *lrexp;

					InterInstruction* vins = new InterInstruction(exp->mLocation, IC_CONSTANT);
					vins->mDst.mType = IT_POINTER;
					vins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
					vins->mConst.mMemory = IM_LOCAL;
					vins->mConst.mVarIndex = decResult->mVarIndex;
					block->Append(vins);

					return ExValue(ftype->mBase, vins->mDst.mTemp, 1);
				}
				else
					return ExValue(ftype->mBase, cins->mDst.mTemp);
			}

		}

		case EX_ASSEMBLER:
		{
			GrowingArray<Declaration*>	 refvars(nullptr);

			TranslateAssembler(proc->mModule, exp, &refvars);

			Declaration* dec = exp->mDecValue;

			if (block)
			{
				dec->mLinkerObject->mFlags |= LOBJF_INLINE;

				InterInstruction* ins = new InterInstruction(exp->mLocation, IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mOperandSize = dec->mSize;
				ins->mConst.mIntConst = 0;
				ins->mConst.mMemory = IM_GLOBAL;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mVarIndex = dec->mVarIndex;
				block->Append(ins);

				InterInstruction	*	jins = new InterInstruction(exp->mLocation, IC_ASSEMBLER);
				jins->mSrc[0].mType = IT_POINTER;
				jins->mSrc[0].mTemp = ins->mDst.mTemp;
				jins->mNumOperands = 1;

				for (int i = 0; i < refvars.Size(); i++)
				{
					InterInstruction* vins = new InterInstruction(exp->mLocation, IC_CONSTANT);
					vins->mDst.mType = IT_POINTER;
					vins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);

					Declaration* vdec = refvars[i];
					if (vdec->mType == DT_ARGUMENT)
					{
						vins->mConst.mVarIndex = vdec->mVarIndex;
						if (inlineMapper)
						{
							vins->mConst.mMemory = IM_LOCAL;
							vins->mConst.mVarIndex = inlineMapper->mParams[vdec->mVarIndex];
						}
						else if (procType->mFlags & DTF_FASTCALL)
						{
							vins->mConst.mMemory = IM_FPARAM;
							vins->mConst.mVarIndex += procType->mFastCallBase;
						}
						else
							vins->mConst.mMemory = IM_PARAM;
						vins->mConst.mOperandSize = vdec->mSize;
						vins->mConst.mIntConst = vdec->mOffset;
					}
					else if (vdec->mType == DT_VARIABLE)
					{
						vins->mConst.mMemory = IM_LOCAL;
						vins->mConst.mVarIndex = vdec->mVarIndex;
						vins->mConst.mOperandSize = vdec->mSize;
						vins->mConst.mIntConst = vdec->mOffset;
					}

					block->Append(vins);

					InterInstruction* lins = new InterInstruction(exp->mLocation, IC_LOAD);
					lins->mSrc[0].mMemory = IM_INDIRECT;
					lins->mSrc[0].mType = IT_POINTER;
					lins->mSrc[0].mTemp = vins->mDst.mTemp;
					lins->mDst.mType = InterTypeOf(vdec->mBase);
					lins->mDst.mTemp = proc->AddTemporary(lins->mDst.mType);
					lins->mSrc[0].mOperandSize = vdec->mSize;
					block->Append(lins);

					jins->mSrc[jins->mNumOperands].mType = InterTypeOf(vdec->mBase);
					jins->mSrc[jins->mNumOperands].mTemp = lins->mDst.mTemp;
					jins->mNumOperands++;
				}

				block->Append(jins);
			}

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_RETURN:
		{
			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_RETURN);
			if (exp->mLeft)
			{
				vr = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);

				if (procType->mBase->mType == DT_TYPE_STRUCT)
				{
					vr = Dereference(proc, exp, block, vr, 1);

					if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has void return type");
					else if (!procType->mBase->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Cannot return incompatible type");
					else if (vr.mReference != 1)
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Non addressable object");

					InterInstruction* ains = new InterInstruction(exp->mLocation, IC_CONSTANT);

					if (inlineMapper)
					{
						ains->mCode = IC_CONSTANT;
						ains->mDst.mType = IT_POINTER;
						ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						ains->mConst.mOperandSize = procType->mBase->mSize;
						ains->mConst.mIntConst = 0;
						ains->mConst.mVarIndex = inlineMapper->mResult;
						ains->mConst.mMemory = IM_LOCAL;
						block->Append(ains);

						ins->mCode = IC_NONE;
					}
					else
					{
						InterInstruction* pins = new InterInstruction(exp->mLocation, IC_CONSTANT);
						pins->mDst.mType = IT_POINTER;
						pins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						pins->mConst.mVarIndex = 0;
						pins->mConst.mIntConst = 0;
						pins->mConst.mOperandSize = 2;
						if (procType->mFlags & DTF_FASTCALL)
						{
							pins->mConst.mMemory = IM_FPARAM;
							pins->mConst.mVarIndex += procType->mFastCallBase;
						}
						else
							pins->mConst.mMemory = IM_PARAM;
						block->Append(pins);

						ains->mCode = IC_LOAD;
						ains->mSrc[0].mMemory = IM_INDIRECT;
						ains->mSrc[0].mType = IT_POINTER;
						ains->mSrc[0].mTemp = pins->mDst.mTemp;
						ains->mDst.mType = IT_POINTER;
						ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						block->Append(ains);

						ins->mCode = IC_RETURN;
					}

					InterInstruction* cins = new InterInstruction(exp->mLocation, IC_COPY);
					cins->mSrc[0].mType = IT_POINTER;
					cins->mSrc[0].mTemp = vr.mTemp;
					cins->mSrc[0].mMemory = IM_INDIRECT;
					cins->mSrc[0].mOperandSize = vr.mType->mSize;
					cins->mSrc[0].mStride = vr.mType->mStripe;

					cins->mSrc[1].mOperandSize = vr.mType->mSize;
					cins->mSrc[1].mType = IT_POINTER;
					cins->mSrc[1].mTemp = ains->mDst.mTemp;
					cins->mSrc[1].mMemory = IM_INDIRECT;

					cins->mConst.mOperandSize = vr.mType->mSize;
					block->Append(cins);
				}
				else
				{
					vr = Dereference(proc, exp, block, vr);

					if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has void return type");
					else if (!procType->mBase->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Cannot return incompatible type");

					vr = CoerceType(proc, exp, block, vr, procType->mBase);

					ins->mSrc[0].mType = InterTypeOf(vr.mType);
					ins->mSrc[0].mTemp = vr.mTemp;

					if (inlineMapper)
					{
						InterInstruction* ains = new InterInstruction(exp->mLocation, IC_CONSTANT);
						ains->mDst.mType = IT_POINTER;
						ains->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
						ains->mConst.mOperandSize = procType->mBase->mSize;
						ains->mConst.mIntConst = 0;
						ains->mConst.mVarIndex = inlineMapper->mResult;;
						ains->mConst.mMemory = IM_LOCAL;
						block->Append(ains);

						ins->mSrc[1].mType = ains->mDst.mType;
						ins->mSrc[1].mTemp = ains->mDst.mTemp;
						ins->mSrc[1].mMemory = IM_INDIRECT;
						ins->mCode = IC_STORE;
						ins->mSrc[1].mOperandSize = ains->mConst.mOperandSize;
					}
					else
						ins->mCode = IC_RETURN_VALUE;
				}
			}
			else
			{
				if (procType->mBase && procType->mBase->mType != DT_TYPE_VOID)
					mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has non void return type");

				if (inlineMapper)
					ins->mCode = IC_NONE;
				else
					ins->mCode = IC_RETURN;
			}

			if (ins->mCode != IC_NONE)
				block->Append(ins);

			if (inlineMapper)
			{
				InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);
				block->Append(jins);
				block->Close(inlineMapper->mReturn, nullptr);
			}
			else
				block->Close(nullptr, nullptr);
			block = new InterCodeBasicBlock();
			proc->Append(block);

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_BREAK:
		{
			if (breakBlock)
			{
				InterInstruction	*	jins = new InterInstruction(exp->mLocation, IC_JUMP);
				block->Append(jins);

				block->Close(breakBlock, nullptr);
				block = new InterCodeBasicBlock();
				proc->Append(block);
			}
			else
				mErrors->Error(exp->mLocation, EERR_INVALID_BREAK, "No break target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_CONTINUE:
		{
			if (continueBlock)
			{
				InterInstruction	*	jins = new InterInstruction(exp->mLocation, IC_JUMP);
				block->Append(jins);

				block->Close(continueBlock, nullptr);
				block = new InterCodeBasicBlock();
				proc->Append(block);
			}
			else
				mErrors->Error(exp->mLocation, EERR_INVALID_CONTINUE, "No continue target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_ASSUME:
		{
#if 1
			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, inlineMapper);

			InterInstruction* ins = new InterInstruction(exp->mLocation, IC_UNREACHABLE);
			fblock->Append(ins);
			fblock->Close(nullptr, nullptr);

			block = tblock;
#endif
			return ExValue(TheVoidTypeDeclaration);
		}
		case EX_LOGICAL_NOT:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl);

			InterInstruction	*	zins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			zins->mDst.mType = InterTypeOf(vl.mType);
			zins->mDst.mTemp = proc->AddTemporary(zins->mDst.mType);
			zins->mConst.mIntConst = 0;
			block->Append(zins);

			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_RELATIONAL_OPERATOR);
			ins->mOperator = IA_CMPEQ;
			ins->mSrc[0].mType = InterTypeOf(vl.mType);
			ins->mSrc[0].mTemp = zins->mDst.mTemp;
			ins->mSrc[1].mType = InterTypeOf(vl.mType);
			ins->mSrc[1].mTemp = vl.mTemp;
			ins->mDst.mType = IT_BOOL;
			ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins->mDst.mTemp);
		}

		case EX_CONDITIONAL:
		{			
#if 1
			if (!exp->mRight->mLeft->HasSideEffects() && !exp->mRight->mRight->HasSideEffects())
			{
				ExValue	vc = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);

				vl = TranslateExpression(procType, proc, block, exp->mRight->mLeft, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, block, exp->mRight->mRight, breakBlock, continueBlock, inlineMapper);

				vc = Dereference(proc, exp, block, vc);

				int			ttemp;
				InterType	ttype, stypel, styper;

				stypel = InterTypeOf(vl.mType);
				styper = InterTypeOf(vr.mType);

				Declaration* dtype;
				if (stypel == IT_POINTER || styper == IT_POINTER)
				{
					if (vl.mType->mType == DT_TYPE_ARRAY)
						vl = Dereference(proc, exp, block, vl, 1);
					else
						vl = Dereference(proc, exp, block, vl);

					if (vr.mType->mType == DT_TYPE_ARRAY)
						vr = Dereference(proc, exp, block, vr, 1);
					else
						vr = Dereference(proc, exp, block, vr);

					if (vl.mType->mBase->IsSubType(vr.mType->mBase))
						dtype = vr.mType;
					else if (vr.mType->mBase->IsSubType(vl.mType->mBase))
						dtype = vl.mType;
					else
					{
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible conditional types");
						dtype = vl.mType;
					}

					Declaration* ntype = new Declaration(dtype->mLocation, DT_TYPE_POINTER);
					ntype->mBase = dtype->mBase;
					dtype = ntype;

					ttype = IT_POINTER;
				}
				else
				{
					vl = Dereference(proc, exp, block, vl);
					vr = Dereference(proc, exp, block, vr);

					if (stypel == styper)
					{
						ttype = stypel;
						dtype = vl.mType;
					}
					else if (stypel > styper)
					{
						ttype = stypel;
						dtype = vl.mType;

						vr = CoerceType(proc, exp, block, vr, dtype);
					}
					else
					{
						ttype = styper;
						dtype = vr.mType;

						vl = CoerceType(proc, exp, block, vl, dtype);
					}
				}

				vc = CoerceType(proc, exp, block, vc, TheBoolTypeDeclaration);

				ttemp = proc->AddTemporary(ttype);

				InterInstruction* sins = new InterInstruction(exp->mLocation, IC_SELECT);
				sins->mSrc[2].mType = InterTypeOf(vc.mType);
				sins->mSrc[2].mTemp = vc.mTemp;
				sins->mSrc[1].mType = ttype;
				sins->mSrc[1].mTemp = vl.mTemp;
				sins->mSrc[0].mType = ttype;
				sins->mSrc[0].mTemp = vr.mTemp;
				sins->mDst.mType = ttype;
				sins->mDst.mTemp = ttemp;
				block->Append(sins);

				return ExValue(dtype, ttemp);
			}
			else
#endif
			{
				InterInstruction* jins0 = new InterInstruction(exp->mLocation, IC_JUMP);
				InterInstruction* jins1 = new InterInstruction(exp->mLocation, IC_JUMP);

				InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
				proc->Append(tblock);
				InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
				proc->Append(fblock);
				InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
				proc->Append(eblock);

				TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, inlineMapper);

				vl = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, breakBlock, continueBlock, inlineMapper);

				int			ttemp;
				InterType	ttype, stypel, styper;

				stypel = InterTypeOf(vl.mType);
				styper = InterTypeOf(vr.mType);

				Declaration* dtype;
				if (stypel == IT_POINTER || styper == IT_POINTER)
				{
					if (vl.mType->mType == DT_TYPE_ARRAY)
						vl = Dereference(proc, exp, tblock, vl, 1);
					else
						vl = Dereference(proc, exp, tblock, vl);

					if (vr.mType->mType == DT_TYPE_ARRAY)
						vr = Dereference(proc, exp, fblock, vr, 1);
					else
						vr = Dereference(proc, exp, fblock, vr);

					if (vl.mType->mBase->IsSubType(vr.mType->mBase))
						dtype = vr.mType;
					else if (vr.mType->mBase->IsSubType(vl.mType->mBase))
						dtype = vl.mType;
					else
					{
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible conditional types");
						dtype = vl.mType;
					}

					Declaration* ntype = new Declaration(dtype->mLocation, DT_TYPE_POINTER);
					ntype->mBase = dtype->mBase;
					dtype = ntype;

					ttype = IT_POINTER;
				}
				else
				{
					vl = Dereference(proc, exp, tblock, vl);
					vr = Dereference(proc, exp, fblock, vr);

					if (stypel == styper)
					{
						ttype = stypel;
						dtype = vl.mType;
					}
					else if (stypel > styper)
					{
						ttype = stypel;
						dtype = vl.mType;

						vr = CoerceType(proc, exp, fblock, vr, dtype);
					}
					else
					{
						ttype = styper;
						dtype = vr.mType;

						vl = CoerceType(proc, exp, tblock, vl, dtype);
					}
				}

				ttemp = proc->AddTemporary(ttype);

				InterInstruction* rins = new InterInstruction(exp->mLocation, IC_LOAD_TEMPORARY);
				rins->mSrc[0].mType = ttype;
				rins->mSrc[0].mTemp = vr.mTemp;
				rins->mDst.mType = ttype;
				rins->mDst.mTemp = ttemp;
				fblock->Append(rins);

				InterInstruction* lins = new InterInstruction(exp->mLocation, IC_LOAD_TEMPORARY);
				lins->mSrc[0].mType = ttype;
				lins->mSrc[0].mTemp = vl.mTemp;
				lins->mDst.mType = ttype;
				lins->mDst.mTemp = ttemp;
				tblock->Append(lins);

				tblock->Append(jins0);
				tblock->Close(eblock, nullptr);

				fblock->Append(jins1);
				fblock->Close(eblock, nullptr);

				block = eblock;

				return ExValue(dtype, ttemp);
			}

			break;
		}

		case EX_TYPECAST:
		{
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock, inlineMapper);

			InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);

			if (exp->mLeft->mDecType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, exp, block, vr);

				int stemp = vr.mTemp;

				if (vr.mType->mSize == 1)
				{
					if (vr.mType->mFlags & DTF_SIGNED)
					{
						InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
						xins->mOperator = IA_EXT8TO16S;
						xins->mSrc[0].mType = IT_INT8;
						xins->mSrc[0].mTemp = stemp;
						xins->mDst.mType = IT_INT16;
						xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
						block->Append(xins);
						stemp = xins->mDst.mTemp;
					}
					else
					{
						InterInstruction* xins = new InterInstruction(exp->mLocation, IC_CONVERSION_OPERATOR);
						xins->mOperator = IA_EXT8TO16U;
						xins->mSrc[0].mType = IT_INT8;
						xins->mSrc[0].mTemp = stemp;
						xins->mDst.mType = IT_INT16;
						xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
						block->Append(xins);
						stemp = xins->mDst.mTemp;
					}
				}

				ins->mOperator = (vr.mType->mFlags & DTF_SIGNED) ? IA_INT2FLOAT : IA_UINT2FLOAT;
				ins->mSrc[0].mType = IT_INT16;
				ins->mSrc[0].mTemp = stemp;
				ins->mDst.mType = InterTypeOf(exp->mLeft->mDecType);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);
			}
			else if (exp->mLeft->mDecType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
			{
				vr = Dereference(proc, exp, block, vr);
				ins->mOperator = (exp->mLeft->mDecType->mFlags & DTF_SIGNED) ? IA_FLOAT2INT : IA_FLOAT2UINT;
				ins->mSrc[0].mType = InterTypeOf(vr.mType);
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mDst.mType = IT_INT16;
				ins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(ins);

				if (exp->mLeft->mDecType->mSize == 1)
				{
					InterInstruction* xins = new InterInstruction(exp->mLocation, IC_TYPECAST);
					xins->mSrc[0].mType = IT_INT16;
					xins->mSrc[0].mTemp = ins->mDst.mTemp;
					xins->mDst.mType = InterTypeOf(exp->mLeft->mDecType);
					xins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(xins);
					ins = xins;
				}
			}
			else if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_POINTER)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference);
			}
			else if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_ARRAY)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference - 1);
			}
			else if (exp->mLeft->mDecType->mType != DT_TYPE_VOID && vr.mType->mType == DT_TYPE_VOID)
			{
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Cannot cast void object to non void object");
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference);
			}
			else if (exp->mLeft->mDecType->IsIntegerType() && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, exp, block, vr);
				return CoerceType(proc, exp, block, vr, exp->mLeft->mDecType);
			}
			else
			{
				vr = Dereference(proc, exp, block, vr);
				ins->mCode = IC_TYPECAST;
				ins->mSrc[0].mType = InterTypeOf(vr.mType);
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mDst.mType = InterTypeOf(exp->mLeft->mDecType);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);
			}

			return ExValue(exp->mLeft->mDecType, ins->mDst.mTemp);
		}
			break;

		case EX_LOGICAL_AND:
		case EX_LOGICAL_OR:
		{
			InterInstruction* jins0 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(exp->mLocation, IC_JUMP);

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp, inlineMapper);

			int	ttemp = proc->AddTemporary(IT_BOOL);

			InterInstruction* tins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			tins->mConst.mIntConst = 1;
			tins->mDst.mType = IT_BOOL;
			tins->mDst.mTemp = ttemp;
			tblock->Append(tins);

			InterInstruction* fins = new InterInstruction(exp->mLocation, IC_CONSTANT);
			fins->mConst.mIntConst = 0;
			fins->mDst.mType = IT_BOOL;
			fins->mDst.mTemp = ttemp;
			fblock->Append(fins);

			tblock->Append(jins0);
			tblock->Close(eblock, nullptr);

			fblock->Append(jins1);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(TheBoolTypeDeclaration, ttemp);

		} break;

		case EX_WHILE:
		{
			InterInstruction	*	jins0 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(exp->mLocation, IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock();
			proc->Append(bblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);
			
			block->Append(jins0);
			block->Close(cblock, nullptr);

			TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft, inlineMapper);

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, lblock, inlineMapper);
			bblock->Append(jins1);
			bblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_IF:
		{
			InterInstruction	*	jins0 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(exp->mLocation, IC_JUMP);

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, inlineMapper);

			vr = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, breakBlock, continueBlock, inlineMapper);
			tblock->Append(jins0);
			tblock->Close(eblock, nullptr);

			if (exp->mRight->mRight)
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, breakBlock, continueBlock, inlineMapper);
			fblock->Append(jins1);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_FOR:
		{
			// assignment
			if (exp->mLeft->mRight)
				TranslateExpression(procType, proc, block, exp->mLeft->mRight, breakBlock, continueBlock, inlineMapper);

			InterInstruction* jins0 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins2 = new InterInstruction(exp->mLocation, IC_JUMP);
			InterInstruction* jins3 = new InterInstruction(exp->mLocation, IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock();
			proc->Append(bblock);
			InterCodeBasicBlock* iblock = new InterCodeBasicBlock();
			proc->Append(iblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			block->Append(jins0);
			block->Close(cblock, nullptr);

			// condition
			if (exp->mLeft->mLeft->mLeft)
				TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft->mLeft->mLeft, inlineMapper);
			else
			{
				cblock->Append(jins1);
				cblock->Close(bblock, nullptr);
			}

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, iblock, inlineMapper);

			bblock->Append(jins2);
			bblock->Close(iblock, nullptr);

			// increment
			if (exp->mLeft->mLeft->mRight)
				TranslateExpression(procType, proc, iblock, exp->mLeft->mLeft->mRight, breakBlock, continueBlock, inlineMapper);

			iblock->Append(jins3);
			iblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_DO:
		{
			InterInstruction	*	jins = new InterInstruction(exp->mLocation, IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			block->Append(jins);
			block->Close(cblock, nullptr);

			vr = TranslateExpression(procType, proc, cblock, exp->mRight, eblock, cblock, inlineMapper);

			TranslateLogic(procType, proc, cblock, lblock, eblock, exp->mLeft, inlineMapper);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_SWITCH:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, vl);
			vl = CoerceType(proc, exp, block, vl, TheSignedIntTypeDeclaration);

			InterCodeBasicBlock	* dblock = nullptr;
			InterCodeBasicBlock* sblock = block;

			block = nullptr;
			
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			SwitchNodeArray		switchNodes({ 0 });

			Expression* sexp = exp->mRight;
			while (sexp)
			{
				Expression* cexp = sexp->mLeft;
				if (cexp->mType == EX_CASE)
				{
					InterCodeBasicBlock* nblock = new InterCodeBasicBlock();
					proc->Append(nblock);

					SwitchNode	snode;
					snode.mBlock = nblock;
					snode.mValue = 0;

					if (cexp->mLeft->mType == EX_CONSTANT && cexp->mLeft->mDecValue->mType == DT_CONST_INTEGER)
						snode.mValue = cexp->mLeft->mDecValue->mInteger;
					else
						mErrors->Error(cexp->mLeft->mLocation, ERRR_INVALID_CASE, "Integral constant expected");

					int i = 0;
					while (i < switchNodes.Size() && switchNodes[i].mValue < snode.mValue)
						i++;
					if (i < switchNodes.Size() && switchNodes[i].mValue == snode.mValue)
						mErrors->Error(exp->mLeft->mLocation, ERRR_INVALID_CASE, "Duplicate case constant");
					else
						switchNodes.Insert(i, snode);

					if (block)
					{
						InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);

						block->Append(jins);
						block->Close(nblock, nullptr);
					}

					block = nblock;
				}
				else if (cexp->mType == EX_DEFAULT)
				{
					if (!dblock)
					{
						dblock = new InterCodeBasicBlock();
						proc->Append(dblock);

						if (block)
						{
							InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);

							block->Append(jins);
							block->Close(dblock, nullptr);
						}

						block = dblock;
					}
					else
						mErrors->Error(cexp->mLocation, EERR_DUPLICATE_DEFAULT, "Duplicate default");
				}

				if (cexp->mRight)
					TranslateExpression(procType, proc, block, cexp->mRight, eblock, continueBlock, inlineMapper);

				sexp = sexp->mRight;
			}

			BuildSwitchTree(proc, exp, sblock, vl, switchNodes, 0, switchNodes.Size(), dblock ? dblock : eblock);

			if (block)
			{
				InterInstruction* jins = new InterInstruction(exp->mLocation, IC_JUMP);

				block->Append(jins);
				block->Close(eblock, nullptr);
			}

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}
		default:
			mErrors->Error(exp->mLocation, EERR_UNIMPLEMENTED, "Unimplemented expression");
			return ExValue(TheVoidTypeDeclaration);
		}
	}
}

void InterCodeGenerator::BuildInitializer(InterCodeModule * mod, uint8* dp, int offset, Declaration* data, InterVariable * variable)
{
	if (data->mType == DT_CONST_DATA)
	{
		memcpy(dp + offset, data->mData, data->mBase->mSize);
	}
	else if (data->mType == DT_CONST_STRUCT)
	{
		Declaration* mdec = data->mParams;
		while (mdec)
		{
			BuildInitializer(mod, dp, offset + mdec->mOffset, mdec, variable);
			mdec = mdec->mNext;
		}
	}
	else if (data->mType == DT_CONST_INTEGER)
	{
		int64	t = data->mInteger;
		for (int i = 0; i < data->mBase->mSize; i++)
		{
			dp[offset + i * data->mBase->mStripe] = uint8(t & 0xff);
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_ADDRESS)
	{
		int64	t = data->mInteger;
		dp[offset + 0] = uint8(t & 0xff);
		dp[offset + data->mBase->mStripe] = t >> 8;
	}
	else if (data->mType == DT_CONST_FLOAT)
	{
		union { float f; uint32 i; } cast;
		cast.f = data->mNumber;
		int64	t = cast.i;
		for (int i = 0; i < 4; i++)
		{
			dp[offset + i * data->mBase->mStripe] = t & 0xff;
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_ASSEMBLER)
	{
		if (!data->mLinkerObject)
			TranslateAssembler(mod, data->mValue, nullptr);

		LinkerReference	ref;
		ref.mObject = variable->mLinkerObject;
		ref.mOffset = offset;
		ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
		ref.mRefObject = data->mLinkerObject;
		ref.mRefOffset = 0;
		variable->mLinkerObject->AddReference(ref);
	}
	else if (data->mType == DT_CONST_FUNCTION)
	{
		assert(false);
	}
	else if (data->mType == DT_CONST_POINTER)
	{
		Expression* exp = data->mValue;
		Declaration* dec = exp->mDecValue;

		LinkerReference	ref;
		ref.mObject = variable->mLinkerObject;
		ref.mOffset = offset;
		ref.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;

		switch (dec->mType)
		{
		case DT_CONST_DATA:
		{
			if (!dec->mLinkerObject)
			{
				dec->mVarIndex = mod->mGlobalVars.Size();
				InterVariable* var = new InterVariable();
				var->mIndex = dec->mVarIndex;
				var->mOffset = 0;
				var->mSize = dec->mSize;
				var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA, dec->mAlignment);
				dec->mLinkerObject = var->mLinkerObject;
				var->mLinkerObject->AddData(dec->mData, dec->mSize);
				mod->mGlobalVars.Push(var);
			}

			ref.mRefObject = dec->mLinkerObject;
			ref.mRefOffset = 0;
			break;
		}
		case DT_CONST_FUNCTION:
			if (!dec->mLinkerObject)
			{
				InterCodeProcedure* cproc = this->TranslateProcedure(mod, dec->mValue, dec);
			}

			ref.mRefObject = dec->mLinkerObject;
			ref.mRefOffset = 0;
			break;

		case DT_VARIABLE:
		{
			if (!dec->mLinkerObject)
			{
				InitGlobalVariable(mod, dec);
			}

			ref.mRefObject = dec->mLinkerObject;
			ref.mRefOffset = 0;
		}	break;

		case DT_VARIABLE_REF:
		{
			if (!dec->mBase->mLinkerObject)
			{
				InitGlobalVariable(mod, dec->mBase);
			}

			ref.mRefObject = dec->mBase->mLinkerObject;
			ref.mRefOffset = dec->mOffset;
		}	break;

		}

		if (data->mBase->mStripe == 1)
			variable->mLinkerObject->AddReference(ref);
		else
		{
			ref.mFlags = LREF_LOWBYTE;
			variable->mLinkerObject->AddReference(ref);
			ref.mFlags = LREF_HIGHBYTE;
			ref.mOffset += data->mBase->mStripe;
			variable->mLinkerObject->AddReference(ref);
		}
	}
}

void InterCodeGenerator::TranslateLogic(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock* block, InterCodeBasicBlock* tblock, InterCodeBasicBlock* fblock, Expression* exp, InlineMapper* inlineMapper)
{
	switch (exp->mType)
	{
	case EX_LOGICAL_NOT:
		TranslateLogic(procType, proc, block, fblock, tblock, exp->mLeft, inlineMapper);
		break;
	case EX_LOGICAL_AND:
	{
		InterCodeBasicBlock* ablock = new InterCodeBasicBlock();
		proc->Append(ablock);
		TranslateLogic(procType, proc, block, ablock, fblock, exp->mLeft, inlineMapper);
		TranslateLogic(procType, proc, ablock, tblock, fblock, exp->mRight, inlineMapper);
		break;
	}
	case EX_LOGICAL_OR:
	{
		InterCodeBasicBlock* oblock = new InterCodeBasicBlock();
		proc->Append(oblock);
		TranslateLogic(procType, proc, block, tblock, oblock, exp->mLeft, inlineMapper);
		TranslateLogic(procType, proc, oblock, tblock, fblock, exp->mRight, inlineMapper);
		break;
	}
	default:
	{
		ExValue	vr = TranslateExpression(procType, proc, block, exp, nullptr, nullptr, inlineMapper);
		vr = Dereference(proc, exp, block, vr);

		InterInstruction	*	ins = new InterInstruction(exp->mLocation, IC_BRANCH);
		ins->mSrc[0].mType = InterTypeOf(vr.mType);
		ins->mSrc[0].mTemp = vr.mTemp;
		block->Append(ins);

		block->Close(tblock, fblock);
	}

	}
}

InterCodeProcedure* InterCodeGenerator::TranslateProcedure(InterCodeModule * mod, Expression* exp, Declaration * dec)
{
	InterCodeProcedure* proc = new InterCodeProcedure(mod, dec->mLocation, dec->mIdent, mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_BYTE_CODE));

	dec->mVarIndex = proc->mID;
	dec->mLinkerObject = proc->mLinkerObject;
	proc->mNumLocals = dec->mNumVars;

	if (dec->mFlags & DTF_NATIVE)
		proc->mNativeProcedure = true;

	if (dec->mFlags & DTF_INTERRUPT)
		proc->mInterrupt = true;

	if (dec->mFlags & DTF_HWINTERRUPT)
		proc->mHardwareInterrupt = true;

	if (dec->mFlags & DTF_FUNC_INTRCALLED)
		proc->mInterruptCalled = true;

	if (dec->mBase->mFlags & DTF_FASTCALL)
	{
		proc->mFastCallProcedure = true;

		dec->mLinkerObject->mNumTemporaries = 1;
		dec->mLinkerObject->mTemporaries[0] = BC_REG_FPARAMS;
		dec->mLinkerObject->mTempSizes[0] = BC_REG_FPARAMS_END - BC_REG_FPARAMS;
	}

	if (dec->mBase->mBase->mType != DT_TYPE_VOID && dec->mBase->mBase->mType != DT_TYPE_STRUCT)
		proc->mValueReturn = true;

	InterCodeBasicBlock* entryBlock = new InterCodeBasicBlock();

	proc->Append(entryBlock);

	InterCodeBasicBlock* exitBlock = entryBlock;

	if (dec->mFlags & DTF_DEFINED)
	{
		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Generate intermediate code <%s>\n", proc->mIdent->mString);

		TranslateExpression(dec->mBase, proc, exitBlock, exp, nullptr, nullptr, nullptr);
	}
	else
		mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mIdent->mString);

	InterInstruction	*	ins = new InterInstruction(exp ? exp->mLocation : dec->mLocation, IC_RETURN);
	exitBlock->Append(ins);
	exitBlock->Close(nullptr, nullptr);

	if (mErrors->mErrorCount == 0)
	{
		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Optimize intermediate code <%s>\n", proc->mIdent->mString);

		proc->Close();
	}

	return proc;
}

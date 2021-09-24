#include "InterCodeGenerator.h"

InterCodeGenerator::InterCodeGenerator(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mForceNativeCode(false)
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
		if (dec->mSize == 1)
			return IT_INT8;
		else if (dec->mSize == 2)
			return IT_INT16;
		else
			return IT_INT32;
	case DT_TYPE_ENUM:
		return IT_INT8;
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

InterCodeGenerator::ExValue InterCodeGenerator::Dereference(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, int level)
{
	while (v.mReference > level)
	{
		InterInstruction	*	ins = new InterInstruction();
		ins->mCode = IC_LOAD;
		ins->mMemory = IM_INDIRECT;
		ins->mSType[0] = IT_POINTER;
		ins->mSTemp[0] = v.mTemp;
		ins->mTType = v.mReference == 1 ? InterTypeOf(v.mType) : IT_POINTER;
		ins->mTTemp = proc->AddTemporary(ins->mTType);
		ins->mOperandSize = v.mReference == 1 ? v.mType->mSize : 2;
		block->Append(ins);

		v = ExValue(v.mType, ins->mTTemp, v.mReference - 1);
	}

	return v;
}

InterCodeGenerator::ExValue InterCodeGenerator::CoerceType(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, Declaration* type)
{
	int		stemp = v.mTemp;

	if (v.mType->IsIntegerType() && type->mType == DT_TYPE_FLOAT)
	{
		if (v.mType->mSize == 1)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction	*	xins = new InterInstruction();
				xins->mCode = IC_CONVERSION_OPERATOR;
				xins->mOperator = IA_EXT8TO16S;
				xins->mSType[0] = IT_INT8;
				xins->mSTemp[0] = stemp;
				xins->mTType = IT_INT16;
				xins->mTTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mTTemp;
			}
			else
			{
				InterInstruction	*	xins = new InterInstruction();
				xins->mCode = IC_CONVERSION_OPERATOR;
				xins->mOperator = IA_EXT8TO16U;
				xins->mSType[0] = IT_INT8;
				xins->mSTemp[0] = stemp;
				xins->mTType = IT_INT16;
				xins->mTTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mTTemp;
			}
		}

		InterInstruction	*	cins = new InterInstruction();
		cins->mCode = IC_CONVERSION_OPERATOR;
		cins->mOperator = IA_INT2FLOAT;
		cins->mSType[0] = IT_INT16;
		cins->mSTemp[0] = stemp;
		cins->mTType = IT_FLOAT;
		cins->mTTemp = proc->AddTemporary(IT_FLOAT);
		block->Append(cins);

		v.mTemp = cins->mTTemp;
		v.mType = type;
	}
	else if (v.mType->mType == DT_TYPE_FLOAT && type->IsIntegerType())
	{
		InterInstruction	*	cins = new InterInstruction();
		cins->mCode = IC_CONVERSION_OPERATOR;
		cins->mOperator = IA_FLOAT2INT;
		cins->mSType[0] = IT_FLOAT;
		cins->mSTemp[0] = v.mTemp;
		cins->mTType = IT_INT16;
		cins->mTTemp = proc->AddTemporary(IT_INT16);
		block->Append(cins);
		v.mTemp = cins->mTTemp;
		v.mType = type;
	}
	else if (v.mType->mSize < type->mSize)
	{
		if (v.mType->mSize == 1 && type->mSize == 2)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction	*	xins = new InterInstruction();
				xins->mCode = IC_CONVERSION_OPERATOR;
				xins->mOperator = IA_EXT8TO16S;
				xins->mSType[0] = IT_INT8;
				xins->mSTemp[0] = stemp;
				xins->mTType = IT_INT16;
				xins->mTTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mTTemp;
			}
			else
			{
				InterInstruction	*	xins = new InterInstruction();
				xins->mCode = IC_CONVERSION_OPERATOR;
				xins->mOperator = IA_EXT8TO16U;
				xins->mSType[0] = IT_INT8;
				xins->mSTemp[0] = stemp;
				xins->mTType = IT_INT16;
				xins->mTTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mTTemp;
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

void InterCodeGenerator::InitGlobalVariable(InterCodeModule * mod, Declaration* dec)
{
	if (!dec->mLinkerObject)
	{
		InterVariable	*	var = new InterVariable();
		var->mOffset = 0;
		var->mSize = dec->mSize;
		var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA);
		var->mIdent = dec->mIdent;

		var->mIndex = mod->mGlobalVars.Size();
		mod->mGlobalVars.Push(var);

		dec->mVarIndex = var->mIndex;
		dec->mLinkerObject = var->mLinkerObject;

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

void InterCodeGenerator::TranslateAssembler(InterCodeModule* mod, Expression * exp)
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
			if (aexp->mType == DT_CONST_INTEGER)
				d[offset++] = cexp->mLeft->mDecValue->mInteger & 255;
			else if (aexp->mType == DT_LABEL_REF)
			{
				if (!aexp->mBase->mBase->mLinkerObject)
					TranslateAssembler(mod, aexp->mBase->mBase->mValue);

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = aexp->mFlags & DTF_UPPER_BYTE;
				ref.mLowByte = !(aexp->mFlags & DTF_UPPER_BYTE);
				ref.mRefObject = aexp->mBase->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset + aexp->mBase->mInteger;
				mLinker->AddReference(ref);

				offset += 1;
			}
			else if (aexp->mType == DT_FUNCTION_REF)
			{
				if (!aexp->mBase->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mBase->mValue, aexp->mBase);
					cproc->ReduceTemporaries();
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = aexp->mFlags & DTF_UPPER_BYTE;
				ref.mLowByte = !(aexp->mFlags & DTF_UPPER_BYTE);
				ref.mRefObject = aexp->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset;
				mLinker->AddReference(ref);

				offset += 1;
			}
			break;
		case ASMIM_ZERO_PAGE:
		case ASMIM_ZERO_PAGE_X:
		case ASMIM_INDIRECT_X:
		case ASMIM_INDIRECT_Y:
			d[offset++] = aexp->mInteger;
			break;
		case ASMIM_ABSOLUTE:
		case ASMIM_INDIRECT:
		case ASMIM_ABSOLUTE_X:
		case ASMIM_ABSOLUTE_Y:
			if (aexp->mType == DT_CONST_INTEGER)
			{
				d[offset++] = cexp->mLeft->mDecValue->mInteger & 255;
				d[offset++] = cexp->mLeft->mDecValue->mInteger >> 8;
			}
			else if (aexp->mType == DT_LABEL)
			{
				if (!aexp->mBase->mLinkerObject)
					TranslateAssembler(mod, aexp->mBase->mValue);

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = true;
				ref.mLowByte = true;
				ref.mRefObject = aexp->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mInteger;
				mLinker->AddReference(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_LABEL_REF)
			{
				if (!aexp->mBase->mBase->mLinkerObject)
					TranslateAssembler(mod, aexp->mBase->mBase->mValue);

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = true;
				ref.mLowByte = true;
				ref.mRefObject = aexp->mBase->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset + aexp->mBase->mInteger;
				mLinker->AddReference(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_CONST_ASSEMBLER)
			{
				if (!aexp->mLinkerObject)
					TranslateAssembler(mod, aexp->mValue);

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = true;
				ref.mLowByte = true;
				ref.mRefObject = aexp->mLinkerObject;
				ref.mRefOffset = 0;
				mLinker->AddReference(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_VARIABLE)
			{
				if (aexp->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mHighByte = true;
					ref.mLowByte = true;
					ref.mRefObject = aexp->mLinkerObject;
					ref.mRefOffset = 0;
					mLinker->AddReference(ref);

					offset += 2;
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
					ref.mHighByte = true;
					ref.mLowByte = true;
					ref.mRefObject = aexp->mBase->mLinkerObject;
					ref.mRefOffset = aexp->mOffset;
					mLinker->AddReference(ref);

					offset += 2;
				}
			}
			else if (aexp->mType == DT_CONST_FUNCTION)
			{
				if (!aexp->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mValue, aexp);
					cproc->ReduceTemporaries();
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = true;
				ref.mLowByte = true;
				ref.mRefObject = aexp->mLinkerObject;
				ref.mRefOffset = 0;
				mLinker->AddReference(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_FUNCTION_REF)
			{
				if (!aexp->mBase->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(mod, aexp->mBase->mValue, aexp->mBase);
					cproc->ReduceTemporaries();
				}

				LinkerReference	ref;
				ref.mObject = dec->mLinkerObject;
				ref.mOffset = offset;
				ref.mHighByte = true;
				ref.mLowByte = true;
				ref.mRefObject = aexp->mBase->mLinkerObject;
				ref.mRefOffset = aexp->mOffset;
				mLinker->AddReference(ref);

				offset += 2;
			}
			break;
		case ASMIM_RELATIVE:
			d[offset] = aexp->mInteger - offset - 1;
			offset++;
			break;
		}

		cexp = cexp->mRight;
	}

	assert(offset == osize);
}

InterCodeGenerator::ExValue InterCodeGenerator::TranslateExpression(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, Expression* exp, InterCodeBasicBlock* breakBlock, InterCodeBasicBlock* continueBlock)
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
			vr = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
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
				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = InterTypeOf(dec->mBase);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				if (ins->mTType == IT_INT8)
				{
					if (dec->mFlags & DTF_SIGNED)
					{
						if (dec->mInteger < -128 || dec->mInteger > 127)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
					}
					else
					{
						if (dec->mInteger < 0 || dec->mInteger > 255)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Unsigned integer constant truncated");
					}
					ins->mIntValue = char(dec->mInteger);
				}
				else if (ins->mTType == IT_INT8)
				{
					if (dec->mFlags & DTF_SIGNED)
					{
						if (dec->mInteger < -32768 || dec->mInteger > 32767)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
					}
					else
					{
						if (dec->mInteger < 0 || dec->mInteger > 65535)
							mErrors->Error(dec->mLocation, EWARN_CONSTANT_TRUNCATED, "Unsigned integer constant truncated");
					}
					ins->mIntValue = short(dec->mInteger);
				}
				else
				{
					ins->mIntValue = dec->mInteger;
				}
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp);

			} break;

			case DT_CONST_FLOAT:
				{
				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = InterTypeOf(dec->mBase);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				ins->mFloatValue = dec->mNumber;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp);

			} break;

			case DT_CONST_ADDRESS:
			{
				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mTTemp = proc->AddTemporary(IT_POINTER);
				ins->mIntValue = dec->mInteger;
				ins->mMemory = IM_ABSOLUTE;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp);

			} break;

			case DT_CONST_FUNCTION:
			{
				if (!dec->mLinkerObject)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(proc->mModule, dec->mValue, dec);
					cproc->ReduceTemporaries();
				}

				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = InterTypeOf(dec->mBase);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				ins->mVarIndex = dec->mVarIndex;
				ins->mLinkerObject = dec->mLinkerObject;
				ins->mMemory = IM_PROCEDURE;
				ins->mIntValue = 0;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp);

			} break;

			case DT_CONST_POINTER:
			{
				vl = TranslateExpression(procType, proc, block, dec->mValue, breakBlock, continueBlock);
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
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA);
					dec->mLinkerObject = var->mLinkerObject;
					var->mLinkerObject->AddData(dec->mData, dec->mSize);
					proc->mModule->mGlobalVars.Push(var);
				}

				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mTTemp = proc->AddTemporary(IT_POINTER);
				ins->mIntValue = 0;
				ins->mVarIndex = dec->mVarIndex;
				ins->mLinkerObject = dec->mLinkerObject;
				ins->mMemory = IM_GLOBAL;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp, 1);
			}

			case DT_CONST_STRUCT:
			{
				if (!dec->mLinkerObject)
				{
					InterVariable	* var = new InterVariable();
					var->mOffset = 0;
					var->mSize = dec->mSize;
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA);
					dec->mLinkerObject = var->mLinkerObject;
					var->mIdent = dec->mIdent;
					dec->mVarIndex = proc->mModule->mGlobalVars.Size();
					var->mIndex = dec->mVarIndex;
					proc->mModule->mGlobalVars.Push(var);

					uint8* d = var->mLinkerObject->AddSpace(dec->mSize);;

					BuildInitializer(proc->mModule, d, 0, dec, var);
				}

				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mTTemp = proc->AddTemporary(IT_POINTER);
				ins->mIntValue = 0;
				ins->mVarIndex = dec->mVarIndex;
				ins->mLinkerObject = dec->mLinkerObject;
				ins->mMemory = IM_GLOBAL;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mTTemp, 1);
			}

			default:
				mErrors->Error(dec->mLocation, EERR_CONSTANT_TYPE, "Unimplemented constant type");
			}

			return ExValue(TheVoidTypeDeclaration);

		case EX_VARIABLE:
		{
			dec = exp->mDecValue;
			
			InterInstruction	*	ins = new InterInstruction();
			ins->mCode = IC_CONSTANT;
			ins->mTType = IT_POINTER;
			ins->mTTemp = proc->AddTemporary(ins->mTType);
			ins->mOperandSize = dec->mSize;
			ins->mIntValue = dec->mOffset;
			if (dec->mType == DT_ARGUMENT)
				ins->mMemory = IM_PARAM;
			else if (dec->mFlags & DTF_GLOBAL)
			{
				InitGlobalVariable(proc->mModule, dec);
				ins->mMemory = IM_GLOBAL;
				ins->mLinkerObject = dec->mLinkerObject;
			}
			else
				ins->mMemory = IM_LOCAL;
			ins->mVarIndex = dec->mVarIndex;

			block->Append(ins);

			if (!(dec->mBase->mFlags & DTF_DEFINED))
				mErrors->Error(dec->mLocation, EERR_VARIABLE_TYPE, "Undefined variable type");

			return ExValue(dec->mBase, ins->mTTemp, 1);
		}


		case EX_ASSIGNMENT:
			{
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);

			if (exp->mToken == TK_ASSIGN || !(vl.mType->mType == DT_TYPE_POINTER && vr.mType->IsIntegerType() && (exp->mToken == TK_ASSIGN_ADD || exp->mToken == TK_ASSIGN_SUB)))
			{
				if (!vl.mType->CanAssign(vr.mType))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
			} 

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot assign to const type");

			if (vl.mType->mType == DT_TYPE_STRUCT || vl.mType->mType == DT_TYPE_ARRAY || vl.mType->mType == DT_TYPE_UNION)
			{
				vr = Dereference(proc, block, vr, 1);
				vl = Dereference(proc, block, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");
				if (vr.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an adressable expression");

				
				InterInstruction	*	ins = new InterInstruction();
				ins->mCode = IC_COPY;
				ins->mMemory = IM_INDIRECT;
				ins->mSType[0] = IT_POINTER;
				ins->mSTemp[0] = vr.mTemp;
				ins->mSType[1] = IT_POINTER;
				ins->mSTemp[1] = vl.mTemp;
				ins->mOperandSize = vl.mType->mSize;
				block->Append(ins);
			}
			else
			{
				if (vl.mType->mType == DT_TYPE_POINTER && (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION))
					vr = Dereference(proc, block, vr, 1);
				else
					vr = Dereference(proc, block, vr);

				vl = Dereference(proc, block, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

				InterInstruction	*	ins = new InterInstruction();

				if (exp->mToken != TK_ASSIGN)
				{
					ExValue	vll = Dereference(proc, block, vl);

					if (vl.mType->mType == DT_TYPE_POINTER)
					{
						InterInstruction	*	cins = new InterInstruction();
						cins->mCode = IC_CONSTANT;

						if (exp->mToken == TK_ASSIGN_ADD)
						{
							cins->mIntValue = vl.mType->mBase->mSize;
						}
						else if (exp->mToken == TK_ASSIGN_SUB)
						{
							cins->mIntValue = -vl.mType->mBase->mSize;
						}
						else
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer assignment");

						if (!vr.mType->IsIntegerType())
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Invalid argument for pointer inc/dec");

						cins->mTType = IT_INT16;
						cins->mTTemp = proc->AddTemporary(cins->mTType);
						block->Append(cins);

						vr = CoerceType(proc, block, vr, TheSignedIntTypeDeclaration);

						InterInstruction	*	mins = new InterInstruction();
						mins->mCode = IC_BINARY_OPERATOR;
						mins->mOperator = IA_MUL;
						mins->mSType[0] = IT_INT16;
						mins->mSTemp[0] = vr.mTemp;
						mins->mSType[1] = IT_INT16;
						mins->mSTemp[1] = cins->mTTemp;
						mins->mTType = IT_INT16;
						mins->mTTemp = proc->AddTemporary(mins->mTType);
						block->Append(mins);

						InterInstruction	*	ains = new InterInstruction();
						ains->mCode = IC_LEA;
						ains->mMemory = IM_INDIRECT;
						ains->mSType[0] = IT_INT16;
						ains->mSTemp[0] = mins->mTTemp;
						ains->mSType[1] = IT_POINTER;
						ains->mSTemp[1] = vll.mTemp;
						ains->mTType = IT_POINTER;
						ains->mTTemp = proc->AddTemporary(ains->mTType);
						block->Append(ains);

						vr.mTemp = ains->mTTemp;
						vr.mType = vll.mType;
					}
					else
					{
						vr = CoerceType(proc, block, vr, vll.mType);

						InterInstruction	*	oins = new InterInstruction();
						oins->mCode = IC_BINARY_OPERATOR;
						oins->mSType[0] = InterTypeOf(vr.mType);
						oins->mSTemp[0] = vr.mTemp;
						oins->mSType[1] = InterTypeOf(vll.mType);
						oins->mSTemp[1] = vll.mTemp;
						oins->mTType = InterTypeOf(vll.mType);
						oins->mTTemp = proc->AddTemporary(oins->mTType);

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

						vr.mTemp = oins->mTTemp;
						vr.mType = vll.mType;

						block->Append(oins);
					}
				}
				else
				{
					vr = CoerceType(proc, block, vr, vl.mType);
				}

				ins->mCode = IC_STORE;
				ins->mMemory = IM_INDIRECT;
				ins->mSType[0] = InterTypeOf(vr.mType);
				ins->mSTemp[0] = vr.mTemp;
				ins->mSType[1] = IT_POINTER;
				ins->mSTemp[1] = vl.mTemp;
				ins->mOperandSize = vl.mType->mSize;
				block->Append(ins);
			}
			}

			return vr;

		case EX_INDEX:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);

			vl = Dereference(proc, block, vl, vl.mType->mType == DT_TYPE_POINTER ? 0 : 1);
			vr = Dereference(proc, block, vr);

			if (vl.mType->mType != DT_TYPE_ARRAY && vl.mType->mType != DT_TYPE_POINTER)
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Invalid type for indexing");

			if (!vr.mType->IsIntegerType())
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Index operand is not integral number");

			vr = CoerceType(proc, block, vr, TheSignedIntTypeDeclaration);

			InterInstruction	*	cins = new InterInstruction();
			cins->mCode = IC_CONSTANT;
			cins->mIntValue = vl.mType->mBase->mSize;
			cins->mTType = IT_INT16;
			cins->mTTemp = proc->AddTemporary(cins->mTType);
			block->Append(cins);

			InterInstruction	*	mins = new InterInstruction();
			mins->mCode = IC_BINARY_OPERATOR;
			mins->mOperator = IA_MUL;
			mins->mSType[0] = IT_INT16;
			mins->mSTemp[0] = vr.mTemp;
			mins->mSType[1] = IT_INT16;
			mins->mSTemp[1] = cins->mTTemp;
			mins->mTType = IT_INT16;
			mins->mTTemp = proc->AddTemporary(mins->mTType);
			block->Append(mins);

			InterInstruction	*	ains = new InterInstruction();
			ains->mCode = IC_LEA;
			ains->mMemory = IM_INDIRECT;
			ains->mSType[0] = IT_INT16;
			ains->mSTemp[0] = mins->mTTemp;
			ains->mSType[1] = IT_POINTER;
			ains->mSTemp[1] = vl.mTemp;
			ains->mTType = IT_POINTER;
			ains->mTTemp = proc->AddTemporary(ains->mTType);
			block->Append(ains);

			return ExValue(vl.mType->mBase, ains->mTTemp, 1);
		}

		case EX_QUALIFY:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

			InterInstruction	*	cins = new InterInstruction();
			cins->mCode = IC_CONSTANT;
			cins->mIntValue = exp->mDecValue->mOffset;
			cins->mTType = IT_INT16;
			cins->mTTemp = proc->AddTemporary(cins->mTType);
			block->Append(cins);

			InterInstruction	*	ains = new InterInstruction();
			ains->mCode = IC_LEA;
			ains->mMemory = IM_INDIRECT;
			ains->mSType[0] = IT_INT16;
			ains->mSTemp[0] = cins->mTTemp;
			ains->mSType[1] = IT_POINTER;
			ains->mSTemp[1] = vl.mTemp;
			ains->mTType = IT_POINTER;
			ains->mTTemp = proc->AddTemporary(ains->mTType);
			block->Append(ains);

			return ExValue(exp->mDecValue->mBase, ains->mTTemp, 1);
		}

		case EX_BINARY:
			{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vr = Dereference(proc, block, vr);

			InterInstruction	*	ins = new InterInstruction();

			if (vl.mType->mType == DT_TYPE_POINTER || vl.mType->mType == DT_TYPE_ARRAY)
			{
				if (vl.mType->mType == DT_TYPE_POINTER)
					vl = Dereference(proc, block, vl);
				else
				{
					vl = Dereference(proc, block, vl, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vl.mType->mBase;
					ptype->mSize = 2;
					vl.mType = ptype;
				}

				if (vr.mType->IsIntegerType())
				{
					InterInstruction	*	cins = new InterInstruction();
					cins->mCode = IC_CONSTANT;

					if (exp->mToken == TK_ADD)
					{
						cins->mIntValue = vl.mType->mBase->mSize;
					}
					else if (exp->mToken == TK_SUB)
					{
						cins->mIntValue = -vl.mType->mBase->mSize;
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");

					cins->mTType = IT_INT16;
					cins->mTTemp = proc->AddTemporary(cins->mTType);
					block->Append(cins);

					InterInstruction	*	mins = new InterInstruction();
					mins->mCode = IC_BINARY_OPERATOR;
					mins->mOperator = IA_MUL;
					mins->mSType[0] = IT_INT16;
					mins->mSTemp[0] = vr.mTemp;
					mins->mSType[1] = IT_INT16;
					mins->mSTemp[1] = cins->mTTemp;
					mins->mTType = IT_INT16;
					mins->mTTemp = proc->AddTemporary(mins->mTType);
					block->Append(mins);

					ins->mCode = IC_LEA;
					ins->mMemory = IM_INDIRECT;
					ins->mSType[0] = IT_INT16;
					ins->mSTemp[0] = mins->mTTemp;
					ins->mSType[1] = IT_POINTER;
					ins->mSTemp[1] = vl.mTemp;
					ins->mTType = IT_POINTER;
					ins->mTTemp = proc->AddTemporary(ins->mTType);
					block->Append(ins);
				}
				else if (vr.mType->IsSame(vl.mType))
				{
					if (exp->mToken == TK_SUB)
					{
						InterInstruction	*	clins = new InterInstruction(), *	crins = new InterInstruction();
						clins->mCode = IC_TYPECAST;
						clins->mSTemp[0] = vl.mTemp;
						clins->mSType[0] = IT_POINTER;
						clins->mTType = IT_INT16;
						clins->mTTemp = proc->AddTemporary(clins->mTType);
						block->Append(clins);

						crins->mCode = IC_TYPECAST;
						crins->mSTemp[0] = vr.mTemp;
						crins->mSType[0] = IT_POINTER;
						crins->mTType = IT_INT16;
						crins->mTTemp = proc->AddTemporary(crins->mTType);
						block->Append(crins);

						InterInstruction	*	cins = new InterInstruction();
						cins->mCode = IC_CONSTANT;
						cins->mIntValue = vl.mType->mBase->mSize;
						cins->mTType = IT_INT16;
						cins->mTTemp = proc->AddTemporary(cins->mTType);
						block->Append(cins);

						InterInstruction	*	sins = new InterInstruction(), *	dins = new InterInstruction();
						sins->mCode = IC_BINARY_OPERATOR;
						sins->mOperator = IA_SUB;
						sins->mSType[0] = IT_INT16;
						sins->mSTemp[0] = crins->mTTemp;
						sins->mSType[1] = IT_INT16;
						sins->mSTemp[1] = clins->mTTemp;
						sins->mTType = IT_INT16;
						sins->mTTemp = proc->AddTemporary(sins->mTType);
						block->Append(sins);

						dins->mCode = IC_BINARY_OPERATOR;
						dins->mOperator = IA_DIVS;
						dins->mSType[0] = IT_INT16;
						dins->mSTemp[0] = cins->mTTemp;
						dins->mSType[1] = IT_INT16;
						dins->mSTemp[1] = sins->mTTemp;
						dins->mTType = IT_INT16;
						dins->mTTemp = proc->AddTemporary(dins->mTType);
						block->Append(dins);

						return ExValue(TheSignedIntTypeDeclaration, dins->mTTemp);
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");
				}
			}
			else
			{
				vl = Dereference(proc, block, vl);

				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Left hand operand type is not numeric");
				if (!vr.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Right hand operand type is not numeric");

				Declaration* dtype;
				if (vr.mType->mType == DT_TYPE_FLOAT || vl.mType->mType == DT_TYPE_FLOAT)
					dtype = TheFloatTypeDeclaration;
				else if (vr.mType->mSize < vl.mType->mSize && (vl.mType->mFlags & DTF_SIGNED))
					dtype = TheSignedIntTypeDeclaration;
				else if (vl.mType->mSize < vr.mType->mSize && (vr.mType->mFlags & DTF_SIGNED))
					dtype = TheSignedIntTypeDeclaration;
				else if ((vr.mType->mFlags & DTF_SIGNED) && (vl.mType->mFlags & DTF_SIGNED))
					dtype = TheSignedIntTypeDeclaration;
				else
					dtype = TheUnsignedIntTypeDeclaration;

				vl = CoerceType(proc, block, vl, dtype);
				vr = CoerceType(proc, block, vr, dtype);

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

				ins->mSType[0] = InterTypeOf(vr.mType);;
				ins->mSTemp[0] = vr.mTemp;
				ins->mSType[1] = InterTypeOf(vl.mType);;
				ins->mSTemp[1] = vl.mTemp;
				ins->mTType = InterTypeOfArithmetic(ins->mSType[0], ins->mSType[1]);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				block->Append(ins);
			}

			return ExValue(vl.mType, ins->mTTemp);
		}


		case EX_PREINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction	*	cins = new InterInstruction(), *	ains = new InterInstruction(), *	rins = new InterInstruction(), *	sins = new InterInstruction();

			ExValue	vdl = Dereference(proc, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mCode = IC_CONSTANT;
			cins->mTType = ftype ? IT_FLOAT : IT_INT16;
			cins->mTTemp = proc->AddTemporary(cins->mTType);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mIntValue = exp->mToken == TK_INC ? vdl.mType->mBase->mSize : -(vdl.mType->mBase->mSize);
			else if (vdl.mType->IsNumericType())
				cins->mIntValue = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric value or pointer");

			block->Append(cins);

			ains->mCode = IC_BINARY_OPERATOR;
			ains->mOperator = IA_ADD;
			ains->mSType[0] = cins->mTType;
			ains->mSTemp[0] = cins->mTTemp;
			ains->mSType[1] = InterTypeOf(vdl.mType);
			ains->mSTemp[1] = vdl.mTemp;
			ains->mTType = ains->mSType[1];
			ains->mTTemp = proc->AddTemporary(ains->mTType);
			block->Append(ains);

			sins->mCode = IC_STORE;
			sins->mMemory = IM_INDIRECT;
			sins->mSType[0] = ains->mTType;
			sins->mSTemp[0] = ains->mTTemp;
			sins->mSType[1] = IT_POINTER;
			sins->mSTemp[1] = vl.mTemp;
			sins->mOperandSize = vl.mType->mSize;
			block->Append(sins);

			return ExValue(vdl.mType, ains->mTTemp);
		}
		break;

		case EX_POSTINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction	*	cins = new InterInstruction(), *	ains = new InterInstruction(), *	rins = new InterInstruction(), *	sins = new InterInstruction();

			ExValue	vdl = Dereference(proc, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mCode = IC_CONSTANT;
			cins->mTType = ftype ? IT_FLOAT : IT_INT16;
			cins->mTTemp = proc->AddTemporary(cins->mTType);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mIntValue = exp->mToken == TK_INC ? vdl.mType->mBase->mSize : -(vdl.mType->mBase->mSize);
			else if (vdl.mType->IsNumericType())
				cins->mIntValue = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric value or pointer");
			block->Append(cins);

			ains->mCode = IC_BINARY_OPERATOR;
			ains->mOperator = IA_ADD;
			ains->mSType[0] = cins->mTType;
			ains->mSTemp[0] = cins->mTTemp;
			ains->mSType[1] = InterTypeOf(vdl.mType);
			ains->mSTemp[1] = vdl.mTemp;
			ains->mTType = ains->mSType[1];
			ains->mTTemp = proc->AddTemporary(ains->mTType);
			block->Append(ains);

			sins->mCode = IC_STORE;
			sins->mMemory = IM_INDIRECT;
			sins->mSType[0] = ains->mTType;
			sins->mSTemp[0] = ains->mTTemp;
			sins->mSType[1] = IT_POINTER;
			sins->mSTemp[1] = vl.mTemp;
			sins->mOperandSize = vl.mType->mSize;
			block->Append(sins);

			return ExValue(vdl.mType, vdl.mTemp);
		}
		break;

		case EX_PREFIX:
		{	
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);

			InterInstruction	*	ins = new InterInstruction();

			ins->mCode = IC_UNARY_OPERATOR;

			switch (exp->mToken)
			{
			case TK_ADD:
				vl = Dereference(proc, block, vl);
				ins->mOperator = IA_NONE;
				break;
			case TK_SUB:
				vl = Dereference(proc, block, vl);
				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric type");
				ins->mOperator = IA_NEG;
				break;
			case TK_BINARY_NOT:
				vl = Dereference(proc, block, vl);
				if (!(vl.mType->mType == DT_TYPE_POINTER || vl.mType->IsNumericType()))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric or pointer type");
				ins->mOperator = IA_NOT;
				break;
			case TK_MUL:
				if (vl.mType->mType != DT_TYPE_POINTER)
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a pointer type");
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
			
			ins->mSType[0] = InterTypeOf(vl.mType);
			ins->mSTemp[0] = vl.mTemp;
			ins->mTType = ins->mSType[0];
			ins->mTTemp = proc->AddTemporary(ins->mTType);

			block->Append(ins);

			return ExValue(vl.mType, ins->mTTemp);
		}

		case EX_RELATIONAL:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vr = Dereference(proc, block, vr);

			InterInstruction	*	ins = new InterInstruction();

			Declaration* dtype = TheSignedIntTypeDeclaration;

			if (vl.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_POINTER)
			{
				dtype = vl.mType;
				if (!vl.mType->IsSame(vr.mType))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible pointer types");
			}
			else if (!vl.mType->IsNumericType() || !vr.mType->IsNumericType())
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric or pointer type");
			else if (vr.mType->mType == DT_TYPE_FLOAT || vl.mType->mType == DT_TYPE_FLOAT)
				dtype = TheFloatTypeDeclaration;
			else if (vr.mType->mSize < vl.mType->mSize && (vl.mType->mFlags & DTF_SIGNED))
				dtype = TheSignedIntTypeDeclaration;
			else if (vl.mType->mSize < vr.mType->mSize && (vr.mType->mFlags & DTF_SIGNED))
				dtype = TheSignedIntTypeDeclaration;
			else if ((vr.mType->mFlags & DTF_SIGNED) && (vl.mType->mFlags & DTF_SIGNED))
				dtype = TheSignedIntTypeDeclaration;
			else
				dtype = TheUnsignedIntTypeDeclaration;

			vl = CoerceType(proc, block, vl, dtype);
			vr = CoerceType(proc, block, vr, dtype);

			bool	signedCompare = dtype->mFlags & DTF_SIGNED;

			ins->mCode = IC_RELATIONAL_OPERATOR;
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
			ins->mSType[0] = InterTypeOf(vr.mType);;
			ins->mSTemp[0] = vr.mTemp;
			ins->mSType[1] = InterTypeOf(vl.mType);;
			ins->mSTemp[1] = vl.mTemp;
			ins->mTType = IT_BOOL;
			ins->mTTemp = proc->AddTemporary(ins->mTType);

			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins->mTTemp);
		}

		case EX_CALL:
		{
			if (exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION && (exp->mLeft->mDecValue->mFlags & DTF_INTRINSIC))
			{
				Declaration* decf = exp->mLeft->mDecValue;
				const Ident	*	iname = decf->mIdent;

				if (!strcmp(iname->mString, "fabs"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
					vr = Dereference(proc, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction();
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_ABS;
					ins->mSType[0] = IT_FLOAT;
					ins->mSTemp[0] = vr.mTemp;
					ins->mTType = IT_FLOAT;
					ins->mTTemp = proc->AddTemporary(ins->mTType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mTTemp);
				}
				else if (!strcmp(iname->mString, "floor"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
					vr = Dereference(proc, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction();
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_FLOOR;
					ins->mSType[0] = IT_FLOAT;
					ins->mSTemp[0] = vr.mTemp;
					ins->mTType = IT_FLOAT;
					ins->mTTemp = proc->AddTemporary(ins->mTType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mTTemp);
				}
				else if (!strcmp(iname->mString, "ceil"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
					vr = Dereference(proc, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction();
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_CEIL;
					ins->mSType[0] = IT_FLOAT;
					ins->mSTemp[0] = vr.mTemp;
					ins->mTType = IT_FLOAT;
					ins->mTTemp = proc->AddTemporary(ins->mTType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mTTemp);
				}
				else
					mErrors->Error(exp->mLeft->mDecValue->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown intrinsic function", iname->mString);

				return ExValue(TheVoidTypeDeclaration);
			}
			else
			{
				vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
				vl = Dereference(proc, block, vl);

				int	atotal = 0;

				int	findex = block->mInstructions.Size();
				InterCodeBasicBlock* fblock = block;

				InterInstruction	*	fins = new InterInstruction();
				fins->mCode = IC_PUSH_FRAME;
				fins->mIntValue = atotal;
				block->Append(fins);

				Declaration	* ftype = vl.mType;
				if (ftype->mType == DT_TYPE_POINTER)
					ftype = ftype->mBase;

				Declaration* pdec = ftype->mParams;
				Expression* pex = exp->mRight;
				while (pex)
				{
					InterInstruction	*	ains = new InterInstruction();
					ains->mCode = IC_CONSTANT;
					ains->mTType = IT_POINTER;
					ains->mTTemp = proc->AddTemporary(ains->mTType);
					ains->mMemory = IM_FRAME;
					if (pdec)
					{
						ains->mVarIndex = pdec->mVarIndex;
						ains->mIntValue = pdec->mOffset;
						ains->mOperandSize = pdec->mSize;
					}
					else if (ftype->mFlags & DTF_VARIADIC)
					{
						ains->mVarIndex = atotal;
						ains->mIntValue = 0;
						ains->mOperandSize = 2;
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

					vr = TranslateExpression(procType, proc, block, texp, breakBlock, continueBlock);

					if (vr.mType->mType == DT_TYPE_STRUCT || vr.mType->mType == DT_TYPE_UNION)
					{
						if (pdec && !pdec->mBase->CanAssign(vr.mType))
							mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

						vr = Dereference(proc, block, vr, 1);

						if (vr.mReference != 1)
							mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an adressable expression");

						InterInstruction* cins = new InterInstruction();
						cins->mCode = IC_COPY;
						cins->mMemory = IM_INDIRECT;
						cins->mSType[0] = IT_POINTER;
						cins->mSTemp[0] = vr.mTemp;
						cins->mSType[1] = IT_POINTER;
						cins->mSTemp[1] = ains->mTTemp;
						cins->mOperandSize = vr.mType->mSize;
						block->Append(cins);

						atotal += cins->mOperandSize;
					}
					else
					{
						if (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION)
							vr = Dereference(proc, block, vr, 1);
						else
							vr = Dereference(proc, block, vr);

						if (pdec)
						{
							if (!pdec->mBase->CanAssign(vr.mType))
								mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							vr = CoerceType(proc, block, vr, pdec->mBase);
						}
						else if (vr.mType->IsIntegerType() && vr.mType->mSize < 2)
						{
							vr = CoerceType(proc, block, vr, TheSignedIntTypeDeclaration);
						}


						InterInstruction* wins = new InterInstruction();
						wins->mCode = IC_STORE;
						wins->mMemory = IM_INDIRECT;
						wins->mSType[0] = InterTypeOf(vr.mType);;
						wins->mSTemp[0] = vr.mTemp;
						wins->mSType[1] = IT_POINTER;
						wins->mSTemp[1] = ains->mTTemp;
						if (pdec)
							wins->mOperandSize = pdec->mSize;
						else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
							wins->mOperandSize = vr.mType->mSize;
						else
							wins->mOperandSize = 2;

						block->Append(wins);

						atotal += wins->mOperandSize;
					}

					if (pdec)
						pdec = pdec->mNext;
				}

				if (pdec)
					mErrors->Error(exp->mLocation, EERR_WRONG_PARAMETER, "Not enough arguments for function call");

				InterInstruction	*	cins = new InterInstruction();
				if (exp->mLeft->mDecValue && exp->mLeft->mDecValue->mFlags & DTF_NATIVE)
					cins->mCode = IC_JSR;
				else
					cins->mCode = IC_CALL;
				cins->mSType[0] = IT_POINTER;
				cins->mSTemp[0] = vl.mTemp;
				if (ftype->mBase->mType != DT_TYPE_VOID)
				{
					cins->mTType = InterTypeOf(ftype->mBase);
					cins->mTTemp = proc->AddTemporary(cins->mTType);
				}

				block->Append(cins);

				fblock->mInstructions[findex]->mIntValue = atotal;

				InterInstruction	*	xins = new InterInstruction();
				xins->mCode = IC_POP_FRAME;
				xins->mIntValue = atotal;
				block->Append(xins);

				return ExValue(ftype->mBase, cins->mTTemp);
			}

		}

		case EX_ASSEMBLER:
		{
			TranslateAssembler(proc->mModule, exp);

			Declaration* dec = exp->mDecValue;

			if (block)
			{
				InterInstruction* ins = new InterInstruction();
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				ins->mOperandSize = dec->mSize;
				ins->mIntValue = 0;
				ins->mMemory = IM_GLOBAL;
				ins->mLinkerObject = dec->mLinkerObject;
				ins->mVarIndex = dec->mVarIndex;
				block->Append(ins);

				InterInstruction	*	jins = new InterInstruction();
				jins->mCode = IC_JSR;
				jins->mSType[0] = IT_POINTER;
				jins->mSTemp[0] = ins->mTTemp;

				block->Append(jins);
			}

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_RETURN:
		{
			InterInstruction	*	ins = new InterInstruction();
			if (exp->mLeft)
			{
				vr = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
				vr = Dereference(proc, block, vr);

				if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
					mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has void return type");
				else if (!procType->mBase->CanAssign(vr.mType))
					mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Cannot return incompatible type");

				vr = CoerceType(proc, block, vr, procType->mBase);

				ins->mSType[0] = InterTypeOf(vr.mType);
				ins->mSTemp[0] = vr.mTemp;
				ins->mCode = IC_RETURN_VALUE;
			}
			else
			{
				if (procType->mBase && procType->mBase->mType != DT_TYPE_VOID)
					mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has non void return type");

				ins->mCode = IC_RETURN;
			}

			block->Append(ins);
			block->Close(nullptr, nullptr);
			block = new InterCodeBasicBlock();
			proc->Append(block);

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_BREAK:
		{
			if (breakBlock)
			{
				InterInstruction	*	jins = new InterInstruction();
				jins->mCode = IC_JUMP;
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
				InterInstruction	*	jins = new InterInstruction();
				jins->mCode = IC_JUMP;
				block->Append(jins);

				block->Close(continueBlock, nullptr);
				block = new InterCodeBasicBlock();
				proc->Append(block);
			}
			else
				mErrors->Error(exp->mLocation, EERR_INVALID_CONTINUE, "No continue target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_LOGICAL_NOT:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);

			InterInstruction	*	zins = new InterInstruction();
			zins->mCode = IC_CONSTANT;
			zins->mTType = InterTypeOf(vr.mType);
			zins->mTTemp = proc->AddTemporary(zins->mTType);
			zins->mIntValue = 0;
			block->Append(zins);

			InterInstruction	*	ins = new InterInstruction();
			ins->mCode = IC_RELATIONAL_OPERATOR;
			ins->mOperator = IA_CMPNE;
			ins->mSType[0] = InterTypeOf(vr.mType);
			ins->mSTemp[0] = zins->mTTemp;
			ins->mSType[1] = InterTypeOf(vr.mType);
			ins->mSTemp[1] = vl.mTemp;
			ins->mTType = IT_BOOL;
			ins->mTTemp = proc->AddTemporary(ins->mTType);
			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins->mTTemp);
		}

		case EX_CONDITIONAL:
		{
			InterInstruction	*	jins0 = new InterInstruction();
			jins0->mCode = IC_JUMP;
			InterInstruction* jins1 = new InterInstruction();
			jins1->mCode = IC_JUMP;

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft);

			vl = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, tblock, vl);

			vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, breakBlock, continueBlock);
			vr = Dereference(proc, fblock, vr);

			int			ttemp;
			InterType	ttype, stypel, styper;

			stypel = InterTypeOf(vl.mType);
			styper = InterTypeOf(vr.mType);

			Declaration* dtype;
			if (stypel == IT_POINTER || styper == IT_POINTER)
			{
				if (!vl.mType->IsSame(vr.mType))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible conditional types");
				
				ttype = IT_POINTER;
				dtype = vl.mType;
			}
			else if (stypel == styper)
			{
				ttype = stypel;
				dtype = vl.mType;
			}
			else if (stypel > styper)
			{
				ttype = stypel;
				dtype = vl.mType;

				vr = CoerceType(proc, fblock, vr, dtype);
			}
			else
			{
				ttype = styper;
				dtype = vr.mType;

				vl = CoerceType(proc, tblock, vl, dtype);
			}

			ttemp = proc->AddTemporary(ttype);

			InterInstruction	*	rins = new InterInstruction();
			rins->mCode = IC_LOAD_TEMPORARY;
			rins->mSType[0] = InterTypeOf(vr.mType);
			rins->mSTemp[0] = vr.mTemp;
			rins->mTType = ttype;
			rins->mTTemp = ttemp;
			fblock->Append(rins);

			InterInstruction	*	lins = new InterInstruction();
			lins->mCode = IC_LOAD_TEMPORARY;
			lins->mSType[0] = InterTypeOf(vl.mType);
			lins->mSTemp[0] = vl.mTemp;
			lins->mTType = ttype;
			lins->mTTemp = ttemp;
			tblock->Append(lins);

			tblock->Append(jins0);
			tblock->Close(eblock, nullptr);

			fblock->Append(jins1);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(dtype, ttemp);

			break;
		}

		case EX_TYPECAST:
		{
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);

			InterInstruction	*	ins = new InterInstruction();

			if (exp->mLeft->mDecType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, block, vr);
				ins->mCode = IC_CONVERSION_OPERATOR;
				ins->mOperator = IA_INT2FLOAT;
				ins->mSType[0] = InterTypeOf(vr.mType);
				ins->mSTemp[0] = vr.mTemp;
				ins->mTType = InterTypeOf(exp->mLeft->mDecType);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				block->Append(ins);
			}
			else if (exp->mLeft->mDecType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
			{
				vr = Dereference(proc, block, vr);
				ins->mCode = IC_CONVERSION_OPERATOR;
				ins->mOperator = IA_FLOAT2INT;
				ins->mSType[0] = InterTypeOf(vr.mType);
				ins->mSTemp[0] = vr.mTemp;
				ins->mTType = InterTypeOf(exp->mLeft->mDecType);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				block->Append(ins);
			}
			else if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_POINTER)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference);
			}
			else if (exp->mLeft->mDecType->mType != DT_TYPE_VOID && vr.mType->mType == DT_TYPE_VOID)
			{
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Cannot cast void object to non void object");
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference);
			}
			else
			{
				vr = Dereference(proc, block, vr);
				ins->mCode = IC_TYPECAST;
				ins->mSType[0] = InterTypeOf(vr.mType);
				ins->mSTemp[0] = vr.mTemp;
				ins->mTType = InterTypeOf(exp->mLeft->mDecType);
				ins->mTTemp = proc->AddTemporary(ins->mTType);
				block->Append(ins);
			}

			return ExValue(exp->mLeft->mDecType, ins->mTTemp);
		}
			break;

		case EX_LOGICAL_AND:
		{

		}
		case EX_WHILE:
		{
			InterInstruction	*	jins0 = new InterInstruction();
			jins0->mCode = IC_JUMP;
			InterInstruction* jins1 = new InterInstruction();
			jins1->mCode = IC_JUMP;

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock();
			proc->Append(bblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);
			
			block->Append(jins0);
			block->Close(cblock, nullptr);

			TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft);

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, lblock);
			bblock->Append(jins1);
			bblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_IF:
		{
			InterInstruction	*	jins0 = new InterInstruction();
			jins0->mCode = IC_JUMP;
			InterInstruction* jins1 = new InterInstruction();
			jins1->mCode = IC_JUMP;

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft);

			vr = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, breakBlock, continueBlock);
			tblock->Append(jins0);
			tblock->Close(eblock, nullptr);

			if (exp->mRight->mRight)
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, breakBlock, continueBlock);
			fblock->Append(jins1);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_FOR:
		{
			// assignment
			if (exp->mLeft->mRight)
				TranslateExpression(procType, proc, block, exp->mLeft->mRight, breakBlock, continueBlock);

			InterInstruction	*	jins0 = new InterInstruction();
			jins0->mCode = IC_JUMP;
			InterInstruction* jins1 = new InterInstruction();
			jins1->mCode = IC_JUMP;
			InterInstruction* jins2 = new InterInstruction();
			jins2->mCode = IC_JUMP;
			InterInstruction* jins3 = new InterInstruction();
			jins3->mCode = IC_JUMP;

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
				TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft->mLeft->mLeft);
			else
			{
				cblock->Append(jins1);
				cblock->Close(bblock, nullptr);
			}

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, iblock);

			bblock->Append(jins2);
			bblock->Close(iblock, nullptr);

			// increment
			if (exp->mLeft->mLeft->mRight)
				TranslateExpression(procType, proc, iblock, exp->mLeft->mLeft->mRight, breakBlock, continueBlock);

			iblock->Append(jins3);
			iblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_DO:
		{
			InterInstruction	*	jins = new InterInstruction();
			jins->mCode = IC_JUMP;

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			block->Append(jins);
			block->Close(cblock, nullptr);

			vr = TranslateExpression(procType, proc, cblock, exp->mRight, eblock, cblock);

			TranslateLogic(procType, proc, cblock, lblock, eblock, exp->mLeft);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_SWITCH:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);
			vl = CoerceType(proc, block, vl, TheSignedIntTypeDeclaration);

			InterCodeBasicBlock	* dblock = nullptr;
			InterCodeBasicBlock* sblock = block;

			block = nullptr;
			
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			Expression* sexp = exp->mRight;
			while (sexp)
			{
				Expression* cexp = sexp->mLeft;
				if (cexp->mType == EX_CASE)
				{
					InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
					proc->Append(cblock);

					InterCodeBasicBlock* nblock = new InterCodeBasicBlock();
					proc->Append(nblock);

					vr = TranslateExpression(procType, proc, sblock, cexp->mLeft, breakBlock, continueBlock);
					vr = Dereference(proc, sblock, vr);
					vr = CoerceType(proc, sblock, vr, TheSignedIntTypeDeclaration);

					InterInstruction	*	cins = new InterInstruction();
					cins->mCode = IC_RELATIONAL_OPERATOR;
					cins->mOperator = IA_CMPEQ;
					cins->mSType[0] = InterTypeOf(vr.mType);;
					cins->mSTemp[0] = vr.mTemp;
					cins->mSType[1] = InterTypeOf(vl.mType);;
					cins->mSTemp[1] = vl.mTemp;
					cins->mTType = IT_BOOL;
					cins->mTTemp = proc->AddTemporary(cins->mTType);

					sblock->Append(cins);

					InterInstruction	*	bins = new InterInstruction();
					bins->mCode = IC_BRANCH;
					bins->mSType[0] = IT_BOOL;
					bins->mSTemp[0] = cins->mTTemp;
					sblock->Append(bins);

					sblock->Close(nblock, cblock);

					if (block)
					{
						InterInstruction* jins = new InterInstruction();
						jins->mCode = IC_JUMP;

						block->Append(jins);
						block->Close(nblock, nullptr);
					}

					sblock = cblock;
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
							InterInstruction* jins = new InterInstruction();
							jins->mCode = IC_JUMP;

							block->Append(jins);
							block->Close(dblock, nullptr);
						}

						block = dblock;
					}
					else
						mErrors->Error(cexp->mLocation, EERR_DUPLICATE_DEFAULT, "Duplicate default");
				}

				if (cexp->mRight)
					TranslateExpression(procType, proc, block, cexp->mRight, eblock, continueBlock);

				sexp = sexp->mRight;
			}

			InterInstruction* jins = new InterInstruction();
			jins->mCode = IC_JUMP;

			sblock->Append(jins);
			if (dblock)
				sblock->Close(dblock, nullptr);
			else
				sblock->Close(eblock, nullptr);

			if (block)
			{
				InterInstruction* jins = new InterInstruction();
				jins->mCode = IC_JUMP;

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
			dp[offset + i] = uint8(t & 0xff);
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_ADDRESS)
	{
		int64	t = data->mInteger;
		dp[offset + 0] = uint8(t & 0xff);
		dp[offset + 1] = t >> 8;
	}
	else if (data->mType == DT_CONST_FLOAT)
	{
		union { float f; uint32 i; } cast;
		cast.f = data->mNumber;
		int64	t = cast.i;
		for (int i = 0; i < 4; i++)
		{
			dp[offset + i] = t & 0xff;
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_ASSEMBLER)
	{
		if (!data->mLinkerObject)
			TranslateAssembler(mod, data->mValue);

		LinkerReference	ref;
		ref.mObject = variable->mLinkerObject;
		ref.mOffset = offset;
		ref.mHighByte = true;
		ref.mLowByte = true;
		ref.mRefObject = data->mLinkerObject;
		ref.mRefOffset = 0;
		mLinker->AddReference(ref);
	}
	else if (data->mType == DT_CONST_FUNCTION)
	{
		if (!data->mLinkerObject)
		{
			InterCodeProcedure* cproc = this->TranslateProcedure(mod, data->mValue, data);
			cproc->ReduceTemporaries();
		}

		LinkerReference	ref;
		ref.mObject = variable->mLinkerObject;
		ref.mOffset = offset;
		ref.mHighByte = true;
		ref.mLowByte = true;
		ref.mRefObject = data->mLinkerObject;
		ref.mRefOffset = 0;
		mLinker->AddReference(ref);
	}
	else if (data->mType == DT_CONST_POINTER)
	{
		Expression* exp = data->mValue;
		Declaration* dec = exp->mDecValue;

		LinkerReference	ref;
		ref.mObject = variable->mLinkerObject;
		ref.mOffset = offset;
		ref.mHighByte = true;
		ref.mLowByte = true;

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
				var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mIdent, dec->mSection, LOT_DATA);
				dec->mLinkerObject = var->mLinkerObject;
				var->mLinkerObject->AddData(dec->mData, dec->mSize);
				mod->mGlobalVars.Push(var);
			}

			ref.mRefObject = dec->mLinkerObject;
			ref.mRefOffset = 0;
			mLinker->AddReference(ref);
			break;
		}
		}
	}
}

void InterCodeGenerator::TranslateLogic(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock* block, InterCodeBasicBlock* tblock, InterCodeBasicBlock* fblock, Expression* exp)
{
	switch (exp->mType)
	{
	case EX_LOGICAL_NOT:
		TranslateLogic(procType, proc, block, fblock, tblock, exp->mLeft);
		break;
	case EX_LOGICAL_AND:
	{
		InterCodeBasicBlock* ablock = new InterCodeBasicBlock();
		proc->Append(ablock);
		TranslateLogic(procType, proc, block, ablock, fblock, exp->mLeft);
		TranslateLogic(procType, proc, ablock, tblock, fblock, exp->mRight);
		break;
	}
	case EX_LOGICAL_OR:
	{
		InterCodeBasicBlock* oblock = new InterCodeBasicBlock();
		proc->Append(oblock);
		TranslateLogic(procType, proc, block, tblock, oblock, exp->mLeft);
		TranslateLogic(procType, proc, oblock, tblock, fblock, exp->mRight);
		break;
	}
	default:
	{
		ExValue	vr = TranslateExpression(procType, proc, block, exp, nullptr, nullptr);
		vr = Dereference(proc, block, vr);

		InterInstruction	*	ins = new InterInstruction();
		ins->mCode = IC_BRANCH;
		ins->mSType[0] = InterTypeOf(vr.mType);
		ins->mSTemp[0] = vr.mTemp;
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

	if (mForceNativeCode)
		dec->mFlags |= DTF_NATIVE;

	if (dec->mFlags & DTF_NATIVE)
		proc->mNativeProcedure = true;

	InterCodeBasicBlock* entryBlock = new InterCodeBasicBlock();

	proc->Append(entryBlock);

	InterCodeBasicBlock* exitBlock = entryBlock;

	if (dec->mFlags & DTF_DEFINED)
		TranslateExpression(dec->mBase, proc, exitBlock, exp, nullptr, nullptr);
	else
		mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mIdent->mString);

	InterInstruction	*	ins = new InterInstruction();
	ins->mCode = IC_RETURN;
	exitBlock->Append(ins);
	exitBlock->Close(nullptr, nullptr);

	proc->Close();

	return proc;
}

#include "InterCodeGenerator.h"
#include <crtdbg.h>

InterCodeGenerator::InterCodeGenerator(Errors* errors)
	: mErrors(errors)
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
		return (dec->mFlags & DTF_SIGNED) ? IT_SIGNED : IT_UNSIGNED;
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
		InterInstruction	ins;
		ins.code = IC_LOAD;
		ins.mem = IM_INDIRECT;
		ins.stype[0] = IT_POINTER;
		ins.stemp[0] = v.mTemp;
		ins.ttype = v.mReference == 1 ? InterTypeOf(v.mType) : IT_POINTER;
		ins.ttemp = proc->AddTemporary(ins.ttype);
		ins.opsize = v.mReference == 1 ? v.mType->mSize : 2;
		block->Append(ins);

		v = ExValue(v.mType, ins.ttemp, v.mReference - 1);
	}

	return v;
}

InterCodeGenerator::ExValue InterCodeGenerator::CoerceType(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, Declaration* type)
{
	if (v.mType->IsIntegerType() && type->mType == DT_TYPE_FLOAT)
	{
		InterInstruction	cins;
		cins.code = IC_CONVERSION_OPERATOR;
		cins.oper = IA_INT2FLOAT;
		cins.stype[0] = InterTypeOf(v.mType);
		cins.stemp[0] = v.mTemp;
		cins.ttype = IT_FLOAT;
		cins.ttemp = proc->AddTemporary(cins.ttype);
		v.mTemp = cins.ttemp;
		v.mType = type;
		block->Append(cins);
	}
	else if (v.mType->mType == DT_TYPE_FLOAT && type->IsIntegerType())
	{
		InterInstruction	cins;
		cins.code = IC_CONVERSION_OPERATOR;
		cins.oper = IA_FLOAT2INT;
		cins.stype[0] = InterTypeOf(v.mType);
		cins.stemp[0] = v.mTemp;
		cins.ttype = InterTypeOf(type);
		cins.ttemp = proc->AddTemporary(cins.ttype);
		v.mTemp = cins.ttemp;
		v.mType = type;
		block->Append(cins);
	}

	return v;
}

static inline bool InterTypeTypeInteger(InterType t)
{
	return t == IT_UNSIGNED || t == IT_SIGNED || t == IT_BOOL;
}

static inline InterType InterTypeOfArithmetic(InterType t1, InterType t2)
{
	if (t1 == IT_FLOAT || t2 == IT_FLOAT)
		return IT_FLOAT;
	else if (t1 == IT_SIGNED && t2 == IT_SIGNED)
		return IT_SIGNED;
	else if (t1 == IT_POINTER && t2 == IT_POINTER)
		return IT_SIGNED;
	else if (t1 == IT_POINTER || t2 == IT_POINTER)
		return IT_POINTER;
	else
		return IT_UNSIGNED;
}

void InterCodeGenerator::InitGlobalVariable(InterCodeModule * mod, Declaration* dec)
{
	if (dec->mVarIndex < 0)
	{
		InterVariable	var;
		var.mOffset = 0;
		var.mSize = dec->mSize;
		var.mData = nullptr;
		var.mIdent = dec->mIdent;
		if (dec->mValue)
		{
			if (dec->mValue->mType == EX_CONSTANT)
			{
				uint8* d = new uint8[dec->mSize];
				memset(d, 0, dec->mSize);
				var.mData = d;

				GrowingArray<InterVariable::Reference>	references({ 0 });
				BuildInitializer(mod, d, 0, dec->mValue->mDecValue, references);
				var.mNumReferences = references.Size();
				if (var.mNumReferences)
				{
					var.mReferences = new InterVariable::Reference[var.mNumReferences];
					for (int i = 0; i < var.mNumReferences; i++)
						var.mReferences[i] = references[i];
				}
			}
			else
				mErrors->Error(dec->mLocation, "Non constant initializer");
		}
		dec->mVarIndex = mod->mGlobalVars.Size();
		var.mIndex = dec->mVarIndex;
		mod->mGlobalVars.Push(var);
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

	InterVariable	var;
	var.mOffset = 0;
	var.mSize = osize;
	uint8* d = new uint8[osize];
	var.mData = d;
	var.mAssembler = true;

	var.mIndex = mod->mGlobalVars.Size();
	if (exp->mDecValue)
	{
		exp->mDecValue->mVarIndex = var.mIndex;
		var.mIdent = exp->mDecValue->mIdent;
	}
	mod->mGlobalVars.Push(var);

	GrowingArray<InterVariable::Reference>	references({ 0 });

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
				if (aexp->mBase->mVarIndex < 0)
					TranslateAssembler(mod, aexp->mBase->mValue);

				InterVariable::Reference	ref;
				ref.mFunction = false;
				ref.mUpper = true;
				ref.mLower = true;
				ref.mAddr = offset;
				ref.mIndex = aexp->mBase->mVarIndex;
				ref.mOffset = aexp->mInteger;

				references.Push(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_LABEL_REF)
			{
				if (aexp->mBase->mBase->mVarIndex < 0)
					TranslateAssembler(mod, aexp->mBase->mBase->mValue);

				InterVariable::Reference	ref;
				ref.mFunction = false;
				ref.mUpper = true;
				ref.mLower = true;
				ref.mAddr = offset;
				ref.mIndex = aexp->mBase->mBase->mVarIndex;
				ref.mOffset = aexp->mOffset + aexp->mBase->mInteger;

				references.Push(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_CONST_ASSEMBLER)
			{
				if (aexp->mVarIndex < 0)
					TranslateAssembler(mod, aexp->mValue);

				InterVariable::Reference	ref;
				ref.mFunction = false;
				ref.mUpper = true;
				ref.mLower = true;
				ref.mAddr = offset;
				ref.mIndex = aexp->mVarIndex;
				ref.mOffset = 0;

				references.Push(ref);

				offset += 2;
			}
			else if (aexp->mType == DT_VARIABLE)
			{
				if (aexp->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp);

					InterVariable::Reference	ref;
					ref.mFunction = false;
					ref.mUpper = true;
					ref.mLower = true;
					ref.mAddr = offset;
					ref.mIndex = aexp->mVarIndex;
					ref.mOffset = 0;

					references.Push(ref);

					offset += 2;
				}
			}
			else if (aexp->mType == DT_VARIABLE_REF)
			{
				if (aexp->mBase->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp->mBase);

					InterVariable::Reference	ref;
					ref.mFunction = false;
					ref.mUpper = true;
					ref.mLower = true;
					ref.mAddr = offset;
					ref.mIndex = aexp->mBase->mVarIndex;
					ref.mOffset = aexp->mOffset;

					references.Push(ref);

					offset += 2;
				}
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

	InterVariable& ivar(mod->mGlobalVars[var.mIndex]);

	ivar.mNumReferences = references.Size();
	if (ivar.mNumReferences)
	{
		ivar.mReferences = new InterVariable::Reference[ivar.mNumReferences];
		for (int i = 0; i < ivar.mNumReferences; i++)
			ivar.mReferences[i] = references[i];
	}
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
				InterInstruction	ins;
				ins.code = IC_CONSTANT;
				ins.ttype = InterTypeOf(dec->mBase);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				if (ins.ttype == IT_SIGNED)
					ins.ivalue = short(dec->mInteger);
				else
					ins.ivalue = unsigned short(dec->mInteger);
				block->Append(ins);
				return ExValue(dec->mBase, ins.ttemp);

			} break;

			case DT_CONST_FLOAT:
				{
				InterInstruction	ins;
				ins.code = IC_CONSTANT;
				ins.ttype = InterTypeOf(dec->mBase);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				ins.fvalue = dec->mNumber;
				block->Append(ins);
				return ExValue(dec->mBase, ins.ttemp);

			} break;

			case DT_CONST_ADDRESS:
			{
				InterInstruction	ins;
				ins.code = IC_CONSTANT;
				ins.ttype = IT_POINTER;
				ins.ttemp = proc->AddTemporary(IT_POINTER);
				ins.ivalue = dec->mInteger;
				ins.mem = IM_ABSOLUTE;
				block->Append(ins);
				return ExValue(dec->mBase, ins.ttemp);

			} break;

			case DT_CONST_FUNCTION:
			{
				if (dec->mVarIndex < 0)
				{
					InterCodeProcedure* cproc = this->TranslateProcedure(proc->mModule, dec->mValue, dec);
					cproc->ReduceTemporaries();
				}

				InterInstruction	ins;
				ins.code = IC_CONSTANT;
				ins.ttype = InterTypeOf(dec->mBase);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				ins.vindex = dec->mVarIndex;
				ins.mem = IM_PROCEDURE;
				ins.ivalue = 0;
				block->Append(ins);
				return ExValue(dec->mBase, ins.ttemp);

			} break;

			case DT_CONST_POINTER:
			{
				vl = TranslateExpression(procType, proc, block, dec->mValue, breakBlock, continueBlock);
				return vl;
			}
			case DT_CONST_DATA:
			{
				if (dec->mVarIndex < 0)
				{
					dec->mVarIndex = proc->mModule->mGlobalVars.Size();
					InterVariable	var;
					var.mIndex = dec->mVarIndex;
					var.mIdent = dec->mIdent;
					var.mOffset = 0;
					var.mSize = dec->mSize;
					var.mData = dec->mData;
					proc->mModule->mGlobalVars.Push(var);
				}

				InterInstruction	ins;
				ins.code = IC_CONSTANT;
				ins.ttype = IT_POINTER;
				ins.ttemp = proc->AddTemporary(IT_POINTER);
				ins.ivalue = 0;
				ins.vindex = dec->mVarIndex;
				ins.mem = IM_GLOBAL;
				block->Append(ins);
				return ExValue(dec->mBase, ins.ttemp);
			}

			default:
				mErrors->Error(dec->mLocation, "Unimplemented constant type");
			}

			return ExValue(TheVoidTypeDeclaration);

		case EX_VARIABLE:
		{
			dec = exp->mDecValue;
			
			InterInstruction	ins;
			ins.code = IC_CONSTANT;
			ins.ttype = IT_POINTER;
			ins.ttemp = proc->AddTemporary(ins.ttype);
			ins.opsize = dec->mSize;
			ins.ivalue = dec->mOffset;
			if (dec->mType == DT_ARGUMENT)
				ins.mem = IM_PARAM;
			else if (dec->mFlags & DTF_GLOBAL)
			{
				InitGlobalVariable(proc->mModule, dec);
				ins.mem = IM_GLOBAL;
			}
			else
				ins.mem = IM_LOCAL;
			ins.vindex = dec->mVarIndex;

			block->Append(ins);

			if (!(dec->mBase->mFlags & DTF_DEFINED))
				mErrors->Error(dec->mLocation, "Undefined variable type");

			return ExValue(dec->mBase, ins.ttemp, 1);
		}


		case EX_ASSIGNMENT:
			{
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);

			if (!vl.mType->CanAssign(vr.mType))
				mErrors->Error(exp->mLocation, "Cannot assign incompatible types");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, "Cannot assign to const type");

			if (vl.mType->mType == DT_TYPE_STRUCT || vl.mType->mType == DT_TYPE_ARRAY)
			{
				vr = Dereference(proc, block, vr, 1);
				vl = Dereference(proc, block, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, "Not a left hand expression");
				if (vr.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, "Not an adressable expression");

				
				InterInstruction	ins;
				ins.code = IC_COPY;
				ins.mem = IM_INDIRECT;
				ins.stype[0] = IT_POINTER;
				ins.stemp[0] = vr.mTemp;
				ins.stype[1] = IT_POINTER;
				ins.stemp[1] = vl.mTemp;
				ins.opsize = vl.mType->mSize;
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
					mErrors->Error(exp->mLeft->mLocation, "Not a left hand expression");

				InterInstruction	ins;

				if (exp->mToken != TK_ASSIGN)
				{
					ExValue	vll = Dereference(proc, block, vl);

					if (vl.mType->mType == DT_TYPE_POINTER)
					{
						InterInstruction	cins;
						cins.code = IC_CONSTANT;

						if (exp->mToken == TK_ASSIGN_ADD)
						{
							cins.ivalue = vl.mType->mBase->mSize;
						}
						else if (exp->mToken == TK_ASSIGN_SUB)
						{
							cins.ivalue = -vl.mType->mBase->mSize;
						}
						else
							mErrors->Error(exp->mLocation, "Invalid pointer assignment");

						if (!vr.mType->IsIntegerType())
							mErrors->Error(exp->mLocation, "Invalid argument for pointer inc/dec");

						cins.ttype = IT_SIGNED;
						cins.ttemp = proc->AddTemporary(cins.ttype);
						block->Append(cins);

						InterInstruction	mins;
						mins.code = IC_BINARY_OPERATOR;
						mins.oper = IA_MUL;
						mins.stype[0] = IT_SIGNED;
						mins.stemp[0] = vr.mTemp;
						mins.stype[1] = IT_SIGNED;
						mins.stemp[1] = cins.ttemp;
						mins.ttype = IT_SIGNED;
						mins.ttemp = proc->AddTemporary(mins.ttype);
						block->Append(mins);

						InterInstruction	ains;
						ains.code = IC_LEA;
						ains.mem = IM_INDIRECT;
						ains.stype[0] = IT_SIGNED;
						ains.stemp[0] = mins.ttemp;
						ains.stype[1] = IT_POINTER;
						ains.stemp[1] = vl.mTemp;
						ains.ttype = IT_POINTER;
						ains.ttemp = proc->AddTemporary(ains.ttype);
						block->Append(ains);
					}
					else
					{
						if (vll.mType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
						{
							InterInstruction	cins;
							cins.code = IC_CONVERSION_OPERATOR;
							cins.oper = IA_FLOAT2INT;
							cins.stype[0] = InterTypeOf(vr.mType);
							cins.stemp[0] = vr.mTemp;
							cins.ttype = InterTypeOf(vll.mType);
							cins.ttemp = proc->AddTemporary(cins.ttype);
							vr.mTemp = cins.ttemp;
							vr.mType = TheSignedIntTypeDeclaration;
							block->Append(cins);
						}
						else if (vll.mType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
						{
							InterInstruction	cins;
							cins.code = IC_CONVERSION_OPERATOR;
							cins.oper = IA_INT2FLOAT;
							cins.stype[0] = InterTypeOf(vr.mType);
							cins.stemp[0] = vr.mTemp;
							cins.ttype = InterTypeOf(vll.mType);;
							cins.ttemp = proc->AddTemporary(cins.ttype);
							vr.mTemp = cins.ttemp;
							vr.mType = TheFloatTypeDeclaration;
							block->Append(cins);
						}

						InterInstruction	oins;
						oins.code = IC_BINARY_OPERATOR;
						oins.stype[0] = InterTypeOf(vr.mType);
						oins.stemp[0] = vr.mTemp;
						oins.stype[1] = InterTypeOf(vll.mType);
						oins.stemp[1] = vll.mTemp;
						oins.ttype = InterTypeOf(vll.mType);
						oins.ttemp = proc->AddTemporary(oins.ttype);

						if (!vll.mType->IsNumericType())
							mErrors->Error(exp->mLocation, "Left hand element type is not numeric");
						if (!vr.mType->IsNumericType())
							mErrors->Error(exp->mLocation, "Right hand element type is not numeric");

						switch (exp->mToken)
						{
						case TK_ASSIGN_ADD:
							oins.oper = IA_ADD;
							break;
						case TK_ASSIGN_SUB:
							oins.oper = IA_SUB;
							break;
						case TK_ASSIGN_MUL:
							oins.oper = IA_MUL;
							break;
						case TK_ASSIGN_DIV:
							oins.oper = IA_DIVS;
							break;
						case TK_ASSIGN_MOD:
							oins.oper = IA_MODS;
							break;
						case TK_ASSIGN_SHL:
							oins.oper = IA_SHL;
							break;
						case TK_ASSIGN_SHR:
							oins.oper = IA_SHR;
							break;
						case TK_ASSIGN_AND:
							oins.oper = IA_AND;
							break;
						case TK_ASSIGN_XOR:
							oins.oper = IA_XOR;
							break;
						case TK_ASSIGN_OR:
							oins.oper = IA_OR;
							break;
						}

						vr.mTemp = oins.ttemp;
						vr.mType = vll.mType;

						block->Append(oins);
					}
				}
				else
				{
					if (vl.mType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
					{
						InterInstruction	cins;
						cins.code = IC_CONVERSION_OPERATOR;
						cins.oper = IA_FLOAT2INT;
						cins.stype[0] = InterTypeOf(vr.mType);
						cins.stemp[0] = vr.mTemp;
						cins.ttype = InterTypeOf(vl.mType);
						cins.ttemp = proc->AddTemporary(cins.ttype);
						vr.mTemp = cins.ttemp;
						vr.mType = TheSignedIntTypeDeclaration;
						block->Append(cins);
					}
					else if (vl.mType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
					{
						InterInstruction	cins;
						cins.code = IC_CONVERSION_OPERATOR;
						cins.oper = IA_INT2FLOAT;
						cins.stype[0] = InterTypeOf(vr.mType);
						cins.stemp[0] = vr.mTemp;
						cins.ttype = InterTypeOf(vl.mType);;
						cins.ttemp = proc->AddTemporary(cins.ttype);
						vr.mTemp = cins.ttemp;
						vr.mType = TheFloatTypeDeclaration;
						block->Append(cins);
					}
				}

				ins.code = IC_STORE;
				ins.mem = IM_INDIRECT;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.stype[1] = IT_POINTER;
				ins.stemp[1] = vl.mTemp;
				ins.opsize = vl.mType->mSize;
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
				mErrors->Error(exp->mLocation, "Invalid type for indexing");

			if (!vr.mType->IsIntegerType())
				mErrors->Error(exp->mLocation, "Index operand is not integral number");

			InterInstruction	cins;
			cins.code = IC_CONSTANT;
			cins.ivalue = vl.mType->mBase->mSize;
			cins.ttype = IT_SIGNED;
			cins.ttemp = proc->AddTemporary(cins.ttype);
			block->Append(cins);

			InterInstruction	mins;
			mins.code = IC_BINARY_OPERATOR;
			mins.oper = IA_MUL;
			mins.stype[0] = IT_SIGNED;
			mins.stemp[0] = vr.mTemp;
			mins.stype[1] = IT_SIGNED;
			mins.stemp[1] = cins.ttemp;
			mins.ttype = IT_SIGNED;			
			mins.ttemp = proc->AddTemporary(mins.ttype);
			block->Append(mins);

			InterInstruction	ains;
			ains.code = IC_LEA;
			ains.mem = IM_INDIRECT;
			ains.stype[0] = IT_SIGNED;
			ains.stemp[0] = mins.ttemp;
			ains.stype[1] = IT_POINTER;
			ains.stemp[1] = vl.mTemp;
			ains.ttype = IT_POINTER;
			ains.ttemp = proc->AddTemporary(ains.ttype);
			block->Append(ains);

			return ExValue(vl.mType->mBase, ains.ttemp, 1);
		}

		case EX_QUALIFY:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLocation, "Not an addressable expression");

			InterInstruction	cins;
			cins.code = IC_CONSTANT;
			cins.ivalue = exp->mDecValue->mOffset;
			cins.ttype = IT_SIGNED;
			cins.ttemp = proc->AddTemporary(cins.ttype);
			block->Append(cins);

			InterInstruction	ains;
			ains.code = IC_LEA;
			ains.mem = IM_INDIRECT;
			ains.stype[0] = IT_SIGNED;
			ains.stemp[0] = cins.ttemp;
			ains.stype[1] = IT_POINTER;
			ains.stemp[1] = vl.mTemp;
			ains.ttype = IT_POINTER;
			ains.ttemp = proc->AddTemporary(ains.ttype);
			block->Append(ains);

			return ExValue(exp->mDecValue->mBase, ains.ttemp, 1);
		}

		case EX_BINARY:
			{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vr = Dereference(proc, block, vr);

			InterInstruction	ins;

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
					InterInstruction	cins;
					cins.code = IC_CONSTANT;

					if (exp->mToken == TK_ADD)
					{
						cins.ivalue = vl.mType->mBase->mSize;
					}
					else if (exp->mToken == TK_SUB)
					{
						cins.ivalue = -vl.mType->mBase->mSize;
					}
					else
						mErrors->Error(exp->mLocation, "Invalid pointer operation");

					cins.ttype = IT_SIGNED;
					cins.ttemp = proc->AddTemporary(cins.ttype);
					block->Append(cins);

					InterInstruction	mins;
					mins.code = IC_BINARY_OPERATOR;
					mins.oper = IA_MUL;
					mins.stype[0] = IT_SIGNED;
					mins.stemp[0] = vr.mTemp;
					mins.stype[1] = IT_SIGNED;
					mins.stemp[1] = cins.ttemp;
					mins.ttype = IT_SIGNED;
					mins.ttemp = proc->AddTemporary(mins.ttype);
					block->Append(mins);

					ins.code = IC_LEA;
					ins.mem = IM_INDIRECT;
					ins.stype[0] = IT_SIGNED;
					ins.stemp[0] = mins.ttemp;
					ins.stype[1] = IT_POINTER;
					ins.stemp[1] = vl.mTemp;
					ins.ttype = IT_POINTER;
					ins.ttemp = proc->AddTemporary(ins.ttype);
					block->Append(ins);
				}
				else if (vr.mType->IsSame(vl.mType))
				{
					if (exp->mToken == TK_SUB)
					{
						InterInstruction	clins, crins;
						clins.code = IC_TYPECAST;
						clins.stemp[0] = vl.mTemp;
						clins.stype[0] = IT_POINTER;
						clins.ttype = IT_SIGNED;
						clins.ttemp = proc->AddTemporary(clins.ttype);
						block->Append(clins);

						crins.code = IC_TYPECAST;
						crins.stemp[0] = vr.mTemp;
						crins.stype[0] = IT_POINTER;
						crins.ttype = IT_SIGNED;
						crins.ttemp = proc->AddTemporary(crins.ttype);
						block->Append(crins);

						InterInstruction	cins;
						cins.code = IC_CONSTANT;
						cins.ivalue = vl.mType->mBase->mSize;
						cins.ttype = IT_SIGNED;
						cins.ttemp = proc->AddTemporary(cins.ttype);
						block->Append(cins);

						InterInstruction	sins, dins;
						sins.code = IC_BINARY_OPERATOR;
						sins.oper = IA_SUB;
						sins.stype[0] = IT_SIGNED;
						sins.stemp[0] = crins.ttemp;
						sins.stype[1] = IT_SIGNED;
						sins.stemp[1] = clins.ttemp;
						sins.ttype = IT_SIGNED;
						sins.ttemp = proc->AddTemporary(sins.ttype);
						block->Append(sins);

						dins.code = IC_BINARY_OPERATOR;
						dins.oper = IA_DIVS;
						dins.stype[0] = IT_SIGNED;
						dins.stemp[0] = cins.ttemp;
						dins.stype[1] = IT_SIGNED;
						dins.stemp[1] = sins.ttemp;
						dins.ttype = IT_SIGNED;
						dins.ttemp = proc->AddTemporary(dins.ttype);
						block->Append(dins);

						return ExValue(TheSignedIntTypeDeclaration, dins.ttemp);
					}
					else
						mErrors->Error(exp->mLocation, "Invalid pointer operation");
				}
			}
			else
			{
				vl = Dereference(proc, block, vl);

				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, "Left hand operand type is not numeric");
				if (!vr.mType->IsNumericType())
					mErrors->Error(exp->mLocation, "Right hand operand type is not numeric");

				if (vl.mType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
				{
					InterInstruction	cins;
					cins.code = IC_CONVERSION_OPERATOR;
					cins.oper = IA_INT2FLOAT;
					cins.stype[0] = InterTypeOf(vl.mType);
					cins.stemp[0] = vl.mTemp;
					cins.ttype = IT_FLOAT;
					cins.ttemp = proc->AddTemporary(cins.ttype);
					vl.mTemp = cins.ttemp;
					vl.mType = TheFloatTypeDeclaration;
					block->Append(cins);
				}
				else if (vl.mType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
				{
					InterInstruction	cins;
					cins.code = IC_CONVERSION_OPERATOR;
					cins.oper = IA_INT2FLOAT;
					cins.stype[0] = InterTypeOf(vr.mType);
					cins.stemp[0] = vr.mTemp;
					cins.ttype = IT_FLOAT;
					cins.ttemp = proc->AddTemporary(cins.ttype);
					vr.mTemp = cins.ttemp;
					vr.mType = TheFloatTypeDeclaration;
					block->Append(cins);
				}

				bool	signedOP = (vl.mType->mFlags & DTF_SIGNED) && (vr.mType->mFlags & DTF_SIGNED);

				ins.code = IC_BINARY_OPERATOR;
				switch (exp->mToken)
				{
				case TK_ADD:
					ins.oper = IA_ADD;
					break;
				case TK_SUB:
					ins.oper = IA_SUB;
					break;
				case TK_MUL:
					ins.oper = IA_MUL;
					break;
				case TK_DIV:
					ins.oper = signedOP ? IA_DIVS : IA_DIVU;
					break;
				case TK_MOD:
					ins.oper = signedOP ? IA_MODS : IA_MODU;
					break;
				case TK_LEFT_SHIFT:
					ins.oper = IA_SHL;
					break;
				case TK_RIGHT_SHIFT:
					ins.oper = (vl.mType->mFlags & DTF_SIGNED) ? IA_SAR : IA_SHR;
					break;
				case TK_BINARY_AND:
					ins.oper = IA_AND;
					break;
				case TK_BINARY_OR:
					ins.oper = IA_OR;
					break;
				case TK_BINARY_XOR:
					ins.oper = IA_XOR;
					break;
				}

				ins.stype[0] = InterTypeOf(vr.mType);;
				ins.stemp[0] = vr.mTemp;
				ins.stype[1] = InterTypeOf(vl.mType);;
				ins.stemp[1] = vl.mTemp;
				ins.ttype = InterTypeOfArithmetic(ins.stype[0], ins.stype[1]);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				block->Append(ins);
			}

			return ExValue(vl.mType, ins.ttemp);
		}


		case EX_PREINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, "Cannot change const value");

			InterInstruction	cins, ains, rins, sins;

			ExValue	vdl = Dereference(proc, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins.code = IC_CONSTANT;
			cins.ttype = ftype ? IT_FLOAT : IT_SIGNED;
			cins.ttemp = proc->AddTemporary(cins.ttype);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins.ivalue = exp->mToken == TK_INC ? vdl.mType->mBase->mSize : -(vdl.mType->mBase->mSize);
			else if (vdl.mType->IsNumericType())
				cins.ivalue = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, "Not a numeric value or pointer");

			block->Append(cins);

			ains.code = IC_BINARY_OPERATOR;
			ains.oper = IA_ADD;
			ains.stype[0] = cins.ttype;
			ains.stemp[0] = cins.ttemp;
			ains.stype[1] = InterTypeOf(vdl.mType);
			ains.stemp[1] = vdl.mTemp;
			ains.ttype = ains.stype[1];
			ains.ttemp = proc->AddTemporary(ains.ttype);
			block->Append(ains);

			sins.code = IC_STORE;
			sins.mem = IM_INDIRECT;
			sins.stype[0] = ains.ttype;
			sins.stemp[0] = ains.ttemp;
			sins.stype[1] = IT_POINTER;
			sins.stemp[1] = vl.mTemp;
			sins.opsize = vl.mType->mSize;
			block->Append(sins);

			return ExValue(vdl.mType, ains.ttemp);
		}
		break;

		case EX_POSTINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, "Cannot change const value");

			InterInstruction	cins, ains, rins, sins;

			ExValue	vdl = Dereference(proc, block, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins.code = IC_CONSTANT;
			cins.ttype = ftype ? IT_FLOAT : IT_SIGNED;
			cins.ttemp = proc->AddTemporary(cins.ttype);
			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins.ivalue = exp->mToken == TK_INC ? vdl.mType->mBase->mSize : -(vdl.mType->mBase->mSize);
			else if (vdl.mType->IsNumericType())
				cins.ivalue = exp->mToken == TK_INC ? 1 : -1;
			else
				mErrors->Error(exp->mLocation, "Not a numeric value or pointer");
			block->Append(cins);

			ains.code = IC_BINARY_OPERATOR;
			ains.oper = IA_ADD;
			ains.stype[0] = cins.ttype;
			ains.stemp[0] = cins.ttemp;
			ains.stype[1] = InterTypeOf(vdl.mType);
			ains.stemp[1] = vdl.mTemp;
			ains.ttype = ains.stype[1];
			ains.ttemp = proc->AddTemporary(ains.ttype);
			block->Append(ains);

			sins.code = IC_STORE;
			sins.mem = IM_INDIRECT;
			sins.stype[0] = ains.ttype;
			sins.stemp[0] = ains.ttemp;
			sins.stype[1] = IT_POINTER;
			sins.stemp[1] = vl.mTemp;
			sins.opsize = vl.mType->mSize;
			block->Append(sins);

			return ExValue(vdl.mType, vdl.mTemp);
		}
		break;

		case EX_PREFIX:
		{	
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);

			InterInstruction	ins;

			ins.code = IC_UNARY_OPERATOR;

			switch (exp->mToken)
			{
			case TK_ADD:
				vl = Dereference(proc, block, vl);
				ins.oper = IA_NONE;
				break;
			case TK_SUB:
				vl = Dereference(proc, block, vl);
				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, "Not a numeric type");
				ins.oper = IA_NEG;
				break;
			case TK_BINARY_NOT:
				vl = Dereference(proc, block, vl);
				if (!(vl.mType->mType == DT_TYPE_POINTER || vl.mType->IsNumericType()))
					mErrors->Error(exp->mLocation, "Not a numeric or pointer type");
				ins.oper = IA_NOT;
				break;
			case TK_MUL:
				if (vl.mType->mType != DT_TYPE_POINTER)
					mErrors->Error(exp->mLocation, "Not a pointer type");
				return ExValue(vl.mType->mBase, vl.mTemp, vl.mReference + 1);
			case TK_BINARY_AND:
			{
				if (vl.mReference < 1)
					mErrors->Error(exp->mLocation, "Not an addressable value");

				Declaration* dec = new Declaration(exp->mLocation, DT_TYPE_POINTER);
				dec->mBase = vl.mType;
				dec->mSize = 2;
				dec->mFlags = DTF_DEFINED;
				return ExValue(dec, vl.mTemp, vl.mReference - 1);
			}
			}
			
			ins.stype[0] = InterTypeOf(vl.mType);
			ins.stemp[0] = vl.mTemp;
			ins.ttype = ins.stype[0];
			ins.ttemp = proc->AddTemporary(ins.ttype);

			block->Append(ins);

			return ExValue(vl.mType, ins.ttemp);
		}

		case EX_RELATIONAL:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
			vr = Dereference(proc, block, vr);

			InterInstruction	ins;

			bool	signedCompare = (vl.mType->mFlags & DTF_SIGNED) && (vr.mType->mFlags & DTF_SIGNED);

			if (!(vl.mType->mType == DT_TYPE_POINTER || vl.mType->IsNumericType()))
				mErrors->Error(exp->mLocation, "Not a numeric or pointer type");

			if (vl.mType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
			{
				InterInstruction	cins;
				cins.code = IC_CONVERSION_OPERATOR;
				cins.oper = IA_INT2FLOAT;
				cins.stype[0] = InterTypeOf(vl.mType);
				cins.stemp[0] = vl.mTemp;
				cins.ttype = IT_FLOAT;
				cins.ttemp = proc->AddTemporary(cins.ttype);
				vl.mTemp = cins.ttemp;
				vl.mType = TheFloatTypeDeclaration;
				block->Append(cins);
			}
			else if (vl.mType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
			{
				InterInstruction	cins;
				cins.code = IC_CONVERSION_OPERATOR;
				cins.oper = IA_INT2FLOAT;
				cins.stype[0] = InterTypeOf(vr.mType);
				cins.stemp[0] = vr.mTemp;
				cins.ttype = IT_FLOAT;
				cins.ttemp = proc->AddTemporary(cins.ttype);
				vr.mTemp = cins.ttemp;
				vr.mType = TheFloatTypeDeclaration;
				block->Append(cins);
			}

			ins.code = IC_RELATIONAL_OPERATOR;
			switch (exp->mToken)
			{
			case TK_EQUAL:
				ins.oper = IA_CMPEQ;
				break;
			case TK_NOT_EQUAL:
				ins.oper = IA_CMPNE;
				break;
			case TK_GREATER_THAN:
				ins.oper = signedCompare ? IA_CMPGS : IA_CMPGU;
				break;
			case TK_GREATER_EQUAL:
				ins.oper = signedCompare ? IA_CMPGES : IA_CMPGEU;
				break;
			case TK_LESS_THAN:
				ins.oper = signedCompare ? IA_CMPLS : IA_CMPLU;
				break;
			case TK_LESS_EQUAL:
				ins.oper = signedCompare ? IA_CMPLES : IA_CMPLEU;
				break;
			}
			ins.stype[0] = InterTypeOf(vr.mType);;
			ins.stemp[0] = vr.mTemp;
			ins.stype[1] = InterTypeOf(vl.mType);;
			ins.stemp[1] = vl.mTemp;
			ins.ttype = IT_BOOL;
			ins.ttemp = proc->AddTemporary(ins.ttype);

			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins.ttemp);
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
						mErrors->Error(exp->mLocation, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	ins;
					ins.code = IC_UNARY_OPERATOR;
					ins.oper = IA_ABS;
					ins.stype[0] = IT_FLOAT;
					ins.stemp[0] = vr.mTemp;
					ins.ttype = IT_FLOAT;
					ins.ttemp = proc->AddTemporary(ins.ttype);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins.ttemp);
				}
				else if (!strcmp(iname->mString, "floor"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
					vr = Dereference(proc, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	ins;
					ins.code = IC_UNARY_OPERATOR;
					ins.oper = IA_FLOOR;
					ins.stype[0] = IT_FLOAT;
					ins.stemp[0] = vr.mTemp;
					ins.ttype = IT_FLOAT;
					ins.ttemp = proc->AddTemporary(ins.ttype);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins.ttemp);
				}
				else if (!strcmp(iname->mString, "ceil"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);
					vr = Dereference(proc, block, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, "Cannot assign incompatible types");
					vr = CoerceType(proc, block, vr, decf->mBase->mParams);

					InterInstruction	ins;
					ins.code = IC_UNARY_OPERATOR;
					ins.oper = IA_CEIL;
					ins.stype[0] = IT_FLOAT;
					ins.stemp[0] = vr.mTemp;
					ins.ttype = IT_FLOAT;
					ins.ttemp = proc->AddTemporary(ins.ttype);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins.ttemp);
				}
				else
					mErrors->Error(exp->mLeft->mDecValue->mLocation, "Unknown intrinsic function", iname->mString);

				return ExValue(TheVoidTypeDeclaration);
			}
			else
			{
				vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
				vl = Dereference(proc, block, vl);

				int	atotal = 0;

				int	findex = block->code.Size();
				InterCodeBasicBlock* fblock = block;

				InterInstruction	fins;
				fins.code = IC_PUSH_FRAME;
				fins.ivalue = atotal;
				block->Append(fins);

				Declaration	* ftype = vl.mType;
				if (ftype->mType == DT_TYPE_POINTER)
					ftype = ftype->mBase;

				Declaration* pdec = ftype->mParams;
				Expression* pex = exp->mRight;
				while (pex)
				{
					InterInstruction	ains;
					ains.code = IC_CONSTANT;
					ains.ttype = IT_POINTER;
					ains.ttemp = proc->AddTemporary(ains.ttype);
					ains.mem = IM_FRAME;
					if (pdec)
					{
						ains.vindex = pdec->mVarIndex;
						ains.ivalue = pdec->mOffset;
						ains.opsize = pdec->mSize;
					}
					else if (ftype->mFlags & DTF_VARIADIC)
					{
						ains.vindex = atotal;
						ains.ivalue = 0;
						ains.opsize = 2;
					}
					else
					{
						mErrors->Error(pex->mLocation, "Too many arguments for function call");
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

					if (vr.mType->mType != DT_TYPE_ARRAY)
						vr = Dereference(proc, block, vr);

					if (pdec)
					{
						if (!pdec->mBase->CanAssign(vr.mType))
							mErrors->Error(texp->mLocation, "Cannot assign incompatible types");
						vr = CoerceType(proc, block, vr, pdec->mBase);
					}

					InterInstruction	wins;
					wins.code = IC_STORE;
					wins.mem = IM_INDIRECT;
					wins.stype[0] = InterTypeOf(vr.mType);;
					wins.stemp[0] = vr.mTemp;
					wins.stype[1] = IT_POINTER;
					wins.stemp[1] = ains.ttemp;
					if (pdec)
						wins.opsize = pdec->mSize;
					else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
						wins.opsize = vr.mType->mSize;
					else
						wins.opsize = 2;

					block->Append(wins);

					atotal += wins.opsize;
					if (pdec)
						pdec = pdec->mNext;
				}

				if (pdec)
					mErrors->Error(exp->mLocation, "Not enough arguments for function call");

				InterInstruction	cins;
				cins.code = IC_CALL;
				cins.stype[0] = IT_POINTER;
				cins.stemp[0] = vl.mTemp;
				if (ftype->mBase->mType != DT_TYPE_VOID)
				{
					cins.ttype = InterTypeOf(ftype->mBase);
					cins.ttemp = proc->AddTemporary(cins.ttype);
				}

				block->Append(cins);

				fblock->code[findex].ivalue = atotal;

				InterInstruction	xins;
				xins.code = IC_POP_FRAME;
				xins.ivalue = atotal;
				block->Append(xins);

				return ExValue(vl.mType->mBase, cins.ttemp);
			}

		}

		case EX_ASSEMBLER:
		{
			int	offset = 0, osize = 0;
			Expression* cexp = exp;
			while (cexp)
			{
				osize += AsmInsSize(cexp->mAsmInsType, cexp->mAsmInsMode);
				cexp = cexp->mRight;
			}

			InterInstruction	ins;
			ins.code = IC_CONSTANT;
			ins.ttype = IT_POINTER;
			ins.ttemp = proc->AddTemporary(ins.ttype);
			ins.opsize = osize;
			ins.ivalue = 0;

			InterVariable	var;
			var.mOffset = 0;
			var.mSize = osize;
			uint8* d = new uint8[osize];
			var.mData = d;
			var.mAssembler = true;

			
			var.mIndex = proc->mModule->mGlobalVars.Size();
			if (exp->mDecValue)
			{
				exp->mDecValue->mVarIndex = var.mIndex;
				var.mIdent = exp->mDecValue->mIdent;
			}
			proc->mModule->mGlobalVars.Push(var);

			GrowingArray<InterVariable::Reference>	references({ 0 });

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
						if (aexp->mBase->mVarIndex < 0)
						{
							InterCodeBasicBlock* bblock = nullptr;
							TranslateExpression(procType, proc, bblock, aexp->mBase->mValue, breakBlock, continueBlock);
						}

						InterVariable::Reference	ref;
						ref.mFunction = false;
						ref.mUpper = true;
						ref.mLower = true;
						ref.mAddr = offset;
						ref.mIndex = aexp->mBase->mVarIndex;
						ref.mOffset = aexp->mInteger;

						references.Push(ref);

						offset += 2;
					}
					else if (aexp->mType == DT_LABEL_REF)
					{
						if (aexp->mBase->mBase->mVarIndex < 0)
						{
							InterCodeBasicBlock* bblock = nullptr;
							TranslateExpression(procType, proc, bblock, aexp->mBase->mBase->mValue, breakBlock, continueBlock);
						}

						InterVariable::Reference	ref;
						ref.mFunction = false;
						ref.mUpper = true;
						ref.mLower = true;
						ref.mAddr = offset;
						ref.mIndex = aexp->mBase->mBase->mVarIndex;
						ref.mOffset = aexp->mOffset + aexp->mBase->mInteger;

						references.Push(ref);

						offset += 2;
					}
					else if (aexp->mType == DT_CONST_ASSEMBLER)
					{
						if (aexp->mVarIndex < 0)
						{
							InterCodeBasicBlock* bblock = nullptr;
							TranslateExpression(procType, proc, bblock, aexp->mValue, breakBlock, continueBlock);
						}

						InterVariable::Reference	ref;
						ref.mFunction = false;
						ref.mUpper = true;
						ref.mLower = true;
						ref.mAddr = offset;
						ref.mIndex = aexp->mVarIndex;
						ref.mOffset = 0;

						references.Push(ref);

						offset += 2;
					}
					else if (aexp->mType == DT_VARIABLE)
					{
						if (aexp->mFlags & DTF_GLOBAL)
						{
							InitGlobalVariable(proc->mModule, aexp);

							InterVariable::Reference	ref;
							ref.mFunction = false;
							ref.mUpper = true;
							ref.mLower = true;
							ref.mAddr = offset;
							ref.mIndex = aexp->mVarIndex;
							ref.mOffset = 0;

							references.Push(ref);

							offset += 2;
						}
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

			InterVariable& ivar(proc->mModule->mGlobalVars[var.mIndex]);

			ivar.mNumReferences = references.Size();
			if (ivar.mNumReferences)
			{
				ivar.mReferences = new InterVariable::Reference[ivar.mNumReferences];
				for (int i = 0; i < ivar.mNumReferences; i++)
					ivar.mReferences[i] = references[i];
			}

			if (block)
			{
				ins.mem = IM_GLOBAL;
				ins.vindex = var.mIndex;
				block->Append(ins);

				InterInstruction	jins;
				jins.code = IC_JSR;
				jins.stype[0] = IT_POINTER;
				jins.stemp[0] = ins.ttemp;

				block->Append(jins);
			}

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_RETURN:
		{
			InterInstruction	ins;
			if (exp->mLeft)
			{
				vr = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
				vr = Dereference(proc, block, vr);

				if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
					mErrors->Error(exp->mLocation, "Function has void return type");
				else if (!procType->mBase->CanAssign(vr.mType))
					mErrors->Error(exp->mLocation, "Cannot return incompatible type");

				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.code = IC_RETURN_VALUE;
			}
			else
			{
				if (procType->mBase && procType->mBase->mType != DT_TYPE_VOID)
					mErrors->Error(exp->mLocation, "Function has non void return type");

				ins.code = IC_RETURN;
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
				InterInstruction	jins;
				jins.code = IC_JUMP;
				block->Append(jins);

				block->Close(breakBlock, nullptr);
				block = new InterCodeBasicBlock();
				proc->Append(block);
			}
			else
				mErrors->Error(exp->mLocation, "No break target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_CONTINUE:
		{
			if (continueBlock)
			{
				InterInstruction	jins;
				jins.code = IC_JUMP;
				block->Append(jins);

				block->Close(continueBlock, nullptr);
				block = new InterCodeBasicBlock();
				proc->Append(block);
			}
			else
				mErrors->Error(exp->mLocation, "No continue target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_LOGICAL_NOT:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);

			InterInstruction	zins;
			zins.code = IC_CONSTANT;
			zins.ttype = InterTypeOf(vr.mType);
			zins.ttemp = proc->AddTemporary(zins.ttype);
			zins.ivalue = 0;
			block->Append(zins);

			InterInstruction	ins;
			ins.code = IC_RELATIONAL_OPERATOR;
			ins.oper = IA_CMPNE;
			ins.stype[0] = InterTypeOf(vr.mType);
			ins.stemp[0] = zins.ttemp;
			ins.stype[1] = InterTypeOf(vr.mType);
			ins.stemp[1] = vl.mTemp;
			ins.ttype = IT_BOOL;
			ins.ttemp = proc->AddTemporary(ins.ttype);
			block->Append(ins);

			return ExValue(TheBoolTypeDeclaration, ins.ttemp);
		}

		case EX_CONDITIONAL:
		{
			InterInstruction	jins;
			jins.code = IC_JUMP;

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
			InterType	ttype;
			Declaration* dtype;
			if (vl.mType->mType == DT_TYPE_FLOAT || vr.mType->mType == DT_TYPE_FLOAT)
			{
				ttype = IT_FLOAT;
				dtype = TheFloatTypeDeclaration;
			}
			else if (vl.mType->IsIntegerType() && vr.mType->IsIntegerType())
			{
				ttype = ((vl.mType->mFlags & DTF_SIGNED) && (vr.mType->mFlags & DTF_SIGNED)) ? IT_SIGNED : IT_UNSIGNED;
				dtype = vl.mType;
			}
			else if (vl.mType->mType == DT_TYPE_POINTER && vl.mType->IsSame(vr.mType))
			{
				ttype = IT_POINTER;
				dtype = vl.mType;
			}
			else
			{
				mErrors->Error(exp->mLocation, "Incompatible conditional types");
				ttype = IT_SIGNED;
				dtype = TheVoidTypeDeclaration;
			}
			ttemp = proc->AddTemporary(ttype);

			if (ttype == IT_FLOAT && vr.mType->IsIntegerType())
			{
				InterInstruction	ins;
				ins.code = IC_CONVERSION_OPERATOR;
				ins.oper = IA_INT2FLOAT;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.ttype = ttype;
				ins.ttemp = ttemp;
				fblock->Append(ins);
			}
			else
			{
				InterInstruction	ins;
				ins.code = IC_LOAD_TEMPORARY;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.ttype = ttype;
				ins.ttemp = ttemp;
				fblock->Append(ins);
			}

			if (ttype == IT_FLOAT && vl.mType->IsIntegerType())
			{
				InterInstruction	ins;
				ins.code = IC_CONVERSION_OPERATOR;
				ins.oper = IA_INT2FLOAT;
				ins.stype[0] = InterTypeOf(vl.mType);
				ins.stemp[0] = vl.mTemp;
				ins.ttype = ttype;
				ins.ttemp = ttemp;
				tblock->Append(ins);
			}
			else
			{
				InterInstruction	ins;
				ins.code = IC_LOAD_TEMPORARY;
				ins.stype[0] = InterTypeOf(vl.mType);
				ins.stemp[0] = vl.mTemp;
				ins.ttype = ttype;
				ins.ttemp = ttemp;
				tblock->Append(ins);
			}

			tblock->Append(jins);
			tblock->Close(eblock, nullptr);

			fblock->Append(jins);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(dtype, ttemp);

			break;
		}

		case EX_TYPECAST:
		{
			vr = TranslateExpression(procType, proc, block, exp->mRight, breakBlock, continueBlock);

			InterInstruction	ins;

			if (exp->mLeft->mDecType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, block, vr);
				ins.code = IC_CONVERSION_OPERATOR;
				ins.oper = IA_INT2FLOAT;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.ttype = InterTypeOf(exp->mLeft->mDecType);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				block->Append(ins);
			}
			else if (exp->mLeft->mDecType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
			{
				vr = Dereference(proc, block, vr);
				ins.code = IC_CONVERSION_OPERATOR;
				ins.oper = IA_FLOAT2INT;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.ttype = InterTypeOf(exp->mLeft->mDecType);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				block->Append(ins);
			}
			else if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_POINTER)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mLeft->mDecType, vr.mTemp, vr.mReference);
			}
			else
			{
				vr = Dereference(proc, block, vr);
				ins.code = IC_TYPECAST;
				ins.stype[0] = InterTypeOf(vr.mType);
				ins.stemp[0] = vr.mTemp;
				ins.ttype = InterTypeOf(exp->mLeft->mDecType);
				ins.ttemp = proc->AddTemporary(ins.ttype);
				block->Append(ins);
			}

			return ExValue(exp->mLeft->mDecType, ins.ttemp);
		}
			break;

		case EX_LOGICAL_AND:
		{

		}
		case EX_WHILE:
		{
			InterInstruction	jins;
			jins.code = IC_JUMP;

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock();
			proc->Append(bblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);
			
			block->Append(jins);
			block->Close(cblock, nullptr);

			TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft);

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, lblock);
			bblock->Append(jins);
			bblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_IF:
		{
			InterInstruction	jins;
			jins.code = IC_JUMP;

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft);

			vr = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, breakBlock, continueBlock);
			tblock->Append(jins);
			tblock->Close(eblock, nullptr);

			if (exp->mRight->mRight)
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, breakBlock, continueBlock);
			fblock->Append(jins);
			fblock->Close(eblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_FOR:
		{
			// assignment
			if (exp->mLeft->mRight)
				TranslateExpression(procType, proc, block, exp->mLeft->mRight, breakBlock, continueBlock);

			InterInstruction	jins;
			jins.code = IC_JUMP;

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock();
			InterCodeBasicBlock* lblock = cblock;
			proc->Append(cblock);
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock();
			proc->Append(bblock);
			InterCodeBasicBlock* iblock = new InterCodeBasicBlock();
			proc->Append(iblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			block->Append(jins);
			block->Close(cblock, nullptr);

			// condition
			if (exp->mLeft->mLeft->mLeft)
				TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft->mLeft->mLeft);
			else
			{
				cblock->Append(jins);
				cblock->Close(bblock, nullptr);
			}

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, eblock, iblock);

			bblock->Append(jins);
			bblock->Close(iblock, nullptr);

			// increment
			if (exp->mLeft->mLeft->mRight)
				TranslateExpression(procType, proc, iblock, exp->mLeft->mLeft->mRight, breakBlock, continueBlock);

			iblock->Append(jins);
			iblock->Close(lblock, nullptr);

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_DO:
		{
			InterInstruction	jins;
			jins.code = IC_JUMP;

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
			InterInstruction	jins;
			jins.code = IC_JUMP;

			vl = TranslateExpression(procType, proc, block, exp->mLeft, breakBlock, continueBlock);
			vl = Dereference(proc, block, vl);

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

					InterInstruction	cins;
					cins.code = IC_RELATIONAL_OPERATOR;
					cins.oper = IA_CMPEQ;
					cins.stype[0] = InterTypeOf(vr.mType);;
					cins.stemp[0] = vr.mTemp;
					cins.stype[1] = InterTypeOf(vl.mType);;
					cins.stemp[1] = vl.mTemp;
					cins.ttype = IT_BOOL;
					cins.ttemp = proc->AddTemporary(cins.ttype);

					sblock->Append(cins);

					InterInstruction	bins;
					bins.code = IC_BRANCH;
					bins.stype[0] = IT_BOOL;
					bins.stemp[0] = cins.ttemp;
					sblock->Append(bins);

					sblock->Close(nblock, cblock);

					if (block)
					{
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
							block->Append(jins);
							block->Close(dblock, nullptr);
						}

						block = dblock;
					}
					else
						mErrors->Error(cexp->mLocation, "Duplicate default");
				}

				if (cexp->mRight)
					TranslateExpression(procType, proc, block, cexp->mRight, eblock, continueBlock);

				sexp = sexp->mRight;
			}

			sblock->Append(jins);
			if (dblock)
				sblock->Close(dblock, nullptr);
			else
				sblock->Close(eblock, nullptr);

			if (block)
			{
				block->Append(jins);
				block->Close(eblock, nullptr);
			}

			block = eblock;

			return ExValue(TheVoidTypeDeclaration);
		}
		default:
			mErrors->Error(exp->mLocation, "Unimplemented expression");
			return ExValue(TheVoidTypeDeclaration);
		}
	}
}

void InterCodeGenerator::BuildInitializer(InterCodeModule * mod, uint8* dp, int offset, Declaration* data, GrowingArray<InterVariable::Reference>& references)
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
			BuildInitializer(mod, dp, offset + mdec->mOffset, mdec, references);
			mdec = mdec->mNext;
		}
	}
	else if (data->mType == DT_CONST_INTEGER)
	{
		__int64	t = data->mInteger;
		for (int i = 0; i < data->mBase->mSize; i++)
		{
			dp[offset + i] = uint8(t & 0xff);
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_ADDRESS)
	{
		__int64	t = data->mInteger;
		dp[offset + 0] = uint8(t & 0xff);
		dp[offset + 1] = t >> 8;
	}
	else if (data->mType == DT_CONST_FLOAT)
	{
		union { float f; uint32 i; } cast;
		cast.f = data->mNumber;
		__int64	t = cast.i;
		for (int i = 0; i < 4; i++)
		{
			dp[i] = t & 0xff;
			t >>= 8;
		}
	}
	else if (data->mType == DT_CONST_POINTER)
	{
		Expression* exp = data->mValue;
		Declaration* dec = exp->mDecValue;

		InterVariable::Reference	ref;
		ref.mAddr = offset;
		ref.mLower = true;
		ref.mUpper = true;
		switch (dec->mType)
		{
		case DT_CONST_DATA:
		{
			if (dec->mVarIndex < 0)
			{
				dec->mVarIndex = mod->mGlobalVars.Size();
				InterVariable	var;
				var.mIndex = dec->mVarIndex;
				var.mOffset = 0;
				var.mSize = dec->mSize;
				var.mData = dec->mData;
				var.mIdent = dec->mIdent;
				mod->mGlobalVars.Push(var);
			}
			ref.mFunction = false;
			ref.mIndex = dec->mVarIndex;
			ref.mOffset = 0;
			references.Push(ref);
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

		InterInstruction	ins;
		ins.code = IC_BRANCH;
		ins.stype[0] = IT_BOOL;
		ins.stemp[0] = vr.mTemp;
		block->Append(ins);

		block->Close(tblock, fblock);
	}

	}
}

InterCodeProcedure* InterCodeGenerator::TranslateProcedure(InterCodeModule * mod, Expression* exp, Declaration * dec)
{
	InterCodeProcedure* proc = new InterCodeProcedure(mod, dec->mLocation, dec->mIdent);
	dec->mVarIndex = proc->mID;

	InterCodeBasicBlock* entryBlock = new InterCodeBasicBlock();

	proc->Append(entryBlock);

	InterCodeBasicBlock* exitBlock = entryBlock;

	if (dec->mFlags & DTF_DEFINED)
		TranslateExpression(dec->mBase, proc, exitBlock, exp, nullptr, nullptr);
	else
		mErrors->Error(dec->mLocation, "Calling undefined function", dec->mIdent->mString);

	InterInstruction	ins;
	ins.code = IC_RETURN;
	exitBlock->Append(ins);
	exitBlock->Close(nullptr, nullptr);

	_CrtCheckMemory();

	proc->Close();

	return proc;
}

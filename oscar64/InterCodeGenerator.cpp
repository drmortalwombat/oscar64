#include "InterCodeGenerator.h"
#include "Constexpr.h"

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
	case DT_TYPE_REFERENCE:
	case DT_TYPE_RVALUEREF:
		return IT_POINTER;
	}
}

InterCodeGenerator::ExValue InterCodeGenerator::ToValue(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, InlineMapper* inlineMapper, ExValue v)
{
	if (v.mType && v.mType->IsReference())
	{
		v.mType = v.mType->mBase;
		v.mReference++;
	}

	return v;
}

InterCodeGenerator::ExValue InterCodeGenerator::Dereference(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, InlineMapper* inlineMapper, ExValue v, int level)
{
	while (v.mReference > level)
	{
		InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD);
		ins->mSrc[0].mMemory = IM_INDIRECT;
		ins->mSrc[0].mType = IT_POINTER;
		ins->mSrc[0].mTemp = v.mTemp;
		ins->mDst.mType = v.mReference == 1 ? InterTypeOf(v.mType) : IT_POINTER;

		if (ins->mDst.mType == IT_NONE)
		{
			mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Not a simple type");
			return v;
		}

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

		if (v.mReference == 1 && v.mBits)
		{
			if (v.mBits + v.mShift <= 8)
				ins->mDst.mType = IT_INT8;
			else if (v.mBits + v.mShift <= 16)
				ins->mDst.mType = IT_INT16;
			else
				ins->mDst.mType = IT_INT32;

			ins->mSrc[0].mOperandSize = InterTypeSize[ins->mDst.mType];
			int nbits = ins->mSrc[0].mOperandSize * 8;

			InterInstruction* clsins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			clsins->mDst.mType = IT_INT8;
			clsins->mDst.mTemp = proc->AddTemporary(clsins->mDst.mType);
			clsins->mConst.mType = IT_INT8;
			clsins->mConst.mIntConst = nbits - v.mShift - v.mBits;
			clsins->mNumOperands = 0;
			block->Append(clsins);

			InterInstruction* crsins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			crsins->mDst.mType = IT_INT8;
			crsins->mDst.mTemp = proc->AddTemporary(crsins->mDst.mType);
			crsins->mConst.mType = IT_INT8;
			crsins->mConst.mIntConst = nbits - v.mBits;
			crsins->mNumOperands = 0;
			block->Append(crsins);

			InterInstruction* slins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
			slins->mOperator = IA_SHL;
			slins->mDst.mType = ins->mDst.mType;
			slins->mDst.mTemp = proc->AddTemporary(slins->mDst.mType);
			slins->mSrc[1] = ins->mDst;
			slins->mSrc[0] = clsins->mDst;
			slins->mNumOperands = 2;
			block->Append(slins);

			InterInstruction* srins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
			srins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_SAR : IA_SHR;
			srins->mDst.mType = ins->mDst.mType;
			srins->mDst.mTemp = proc->AddTemporary(slins->mDst.mType);
			srins->mSrc[1] = slins->mDst;
			srins->mSrc[0] = crsins->mDst;
			srins->mNumOperands = 2;
			block->Append(srins);
			
			// Promote unsigned bitfields that fit into a signed int to signed int
			Declaration* vtype = v.mType;
			if (vtype->mSize == 2 && v.mBits < 16 && !(vtype->mFlags & DTF_SIGNED))
				vtype = TheSignedIntTypeDeclaration;

			if (InterTypeSize[ins->mDst.mType] < v.mType->mSize)
			{
				InterInstruction* crins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
				crins->mDst.mType = InterTypeOf(v.mType);
				crins->mDst.mTemp = proc->AddTemporary(crins->mDst.mType);
				crins->mSrc[0] = srins->mDst;
				crins->mNumOperands = 1;
				if (ins->mDst.mType == IT_INT16)
					crins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_EXT16TO32S : IA_EXT16TO32U;
				else if (v.mType->mSize == 2)
					crins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_EXT8TO16S : IA_EXT8TO16U;
				else
					crins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_EXT8TO32S : IA_EXT8TO32U;
				block->Append(crins);
				v = ExValue(vtype, crins->mDst.mTemp, v.mReference - 1);
			}
			else
				v = ExValue(vtype, srins->mDst.mTemp, v.mReference - 1);
		}
		else
			v = ExValue(v.mType, ins->mDst.mTemp, v.mReference - 1, v.mBits, v.mShift);
	}

	return v;
}

void InterCodeGenerator::StoreValue(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, InlineMapper* inlineMapper, ExValue vl, ExValue vr)
{
	// Bitfield assignment
	if (vl.mBits)
	{
		InterType	itype;

		if (vl.mBits + vl.mShift <= 8)
			itype = IT_INT8;
		else if (vl.mBits + vl.mShift <= 16)
			itype = IT_INT16;
		else
			itype = IT_INT32;

		int nbits = InterTypeSize[itype] * 8;

		InterInstruction* lins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD);
		lins->mDst.mType = itype;
		lins->mDst.mTemp = proc->AddTemporary(lins->mDst.mType);
		lins->mSrc[0].mMemory = IM_INDIRECT;
		lins->mSrc[0].mType = IT_POINTER;
		lins->mSrc[0].mTemp = vl.mTemp;
		lins->mSrc[0].mOperandSize = InterTypeSize[itype];
		lins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
		lins->mNumOperands = 1;
		block->Append(lins);

		InterInstruction* csins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
		csins->mDst.mType = IT_INT8;
		csins->mDst.mTemp = proc->AddTemporary(csins->mDst.mType);
		csins->mConst.mType = IT_INT8;
		csins->mConst.mIntConst = vl.mShift;
		csins->mNumOperands = 0;
		block->Append(csins);

		InterInstruction* cmsins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
		cmsins->mDst.mType = itype;
		cmsins->mDst.mTemp = proc->AddTemporary(cmsins->mDst.mType);
		cmsins->mConst.mType = itype;
		cmsins->mConst.mIntConst = ((1 << vl.mBits) - 1) << vl.mShift;
		cmsins->mNumOperands = 0;
		block->Append(cmsins);

		InterInstruction* cmlins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
		cmlins->mDst.mType = itype;
		cmlins->mDst.mTemp = proc->AddTemporary(cmlins->mDst.mType);
		cmlins->mConst.mType = itype;
		cmlins->mConst.mIntConst = ~cmsins->mConst.mIntConst;
		cmlins->mNumOperands = 0;
		block->Append(cmlins);

		int	rtemp = vr.mTemp;

		if (InterTypeSize[itype] > vr.mType->mSize)
		{
			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);

			if (InterTypeSize[itype] == 2)
				cins->mOperator = IA_EXT8TO16U;
			else if (vr.mType->mSize == 1)
				cins->mOperator = IA_EXT8TO32U;
			else
				cins->mOperator = IA_EXT8TO16U;

			cins->mDst.mType = itype;
			cins->mDst.mTemp = proc->AddTemporary(itype);
			cins->mSrc[0].mTemp = rtemp;
			cins->mSrc[0].mType = InterTypeOf(vr.mType);
			block->Append(cins);

			rtemp = cins->mDst.mTemp;
		}

		InterInstruction* sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
		sins->mOperator = IA_SHL;
		sins->mDst.mType = itype;
		sins->mDst.mTemp = proc->AddTemporary(sins->mDst.mType);
		sins->mSrc[1].mTemp = rtemp;
		sins->mSrc[1].mType = itype;
		sins->mSrc[0] = csins->mDst;
		sins->mNumOperands = 2;
		block->Append(sins);

		InterInstruction* msins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
		msins->mOperator = IA_AND;
		msins->mDst.mType = itype;
		msins->mDst.mTemp = proc->AddTemporary(msins->mDst.mType);
		msins->mSrc[0] = sins->mDst;
		msins->mSrc[1] = cmsins->mDst;
		msins->mNumOperands = 2;
		block->Append(msins);

		InterInstruction* mlins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
		mlins->mOperator = IA_AND;
		mlins->mDst.mType = itype;
		mlins->mDst.mTemp = proc->AddTemporary(mlins->mDst.mType);
		mlins->mSrc[0] = lins->mDst;
		mlins->mSrc[1] = cmlins->mDst;
		mlins->mNumOperands = 2;
		block->Append(mlins);

		InterInstruction* oins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
		oins->mOperator = IA_OR;
		oins->mDst.mType = itype;
		oins->mDst.mTemp = proc->AddTemporary(oins->mDst.mType);
		oins->mSrc[0] = msins->mDst;
		oins->mSrc[1] = mlins->mDst;
		oins->mNumOperands = 2;
		block->Append(oins);

		InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
		ins->mSrc[1].mMemory = IM_INDIRECT;
		ins->mSrc[0] = oins->mDst;
		ins->mSrc[1].mType = IT_POINTER;
		ins->mSrc[1].mTemp = vl.mTemp;
		ins->mSrc[1].mOperandSize = InterTypeSize[itype];
		ins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
		ins->mNumOperands = 2;
		block->Append(ins);
	}
	else
	{
		InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);

		ins->mSrc[1].mMemory = IM_INDIRECT;
		ins->mSrc[0].mType = InterTypeOf(vr.mType);
		ins->mSrc[0].mTemp = vr.mTemp;
		ins->mSrc[1].mType = IT_POINTER;
		ins->mSrc[1].mTemp = vl.mTemp;
		ins->mSrc[1].mOperandSize = vl.mType->mSize;
		ins->mSrc[1].mStride = vl.mType->mStripe;
		ins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
		ins->mNumOperands = 2;
		block->Append(ins);
	}
}

InterCodeGenerator::ExValue InterCodeGenerator::CoerceType(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, InlineMapper* inlineMapper, ExValue v, Declaration* type, bool checkTrunc)
{
	int		stemp = v.mTemp;

	if (type->IsReference())
	{
		if (v.mType->IsReference())
		{
			if (!type->mBase->IsSubType(v.mType->mBase))
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Incompatible type for reference");
		}
		else if (!type->mBase->IsSubType(v.mType))
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Incompatible type for reference");

		return v;
	}
	else if (type->mType == DT_TYPE_STRUCT && type->mBase && type->mBase->IsSubType(v.mType))
	{
		v.mType = type;
		return v;
	}

	if (v.mType->IsReference() && type->IsSimpleType())
	{
		v.mReference++;
		v.mType = v.mType->mBase;
		v = Dereference(proc, exp, block, inlineMapper, v);
	}

	if (v.mType->IsIntegerType() && type->mType == DT_TYPE_FLOAT)
	{
		if (v.mType->mSize == 1)
		{
			if (v.mType->mFlags & DTF_SIGNED)
			{
				InterInstruction	*	xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction	*	xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
				xins->mOperator = IA_EXT8TO16U;
				xins->mSrc[0].mType = IT_INT8;
				xins->mSrc[0].mTemp = stemp;
				xins->mDst.mType = IT_INT16;
				xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
				block->Append(xins);
				stemp = xins->mDst.mTemp;
			}
		}

		InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
		if (v.mType->mSize <= 2)
		{
			cins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_INT2FLOAT : IA_UINT2FLOAT;
			cins->mSrc[0].mType = IT_INT16;
		}
		else
		{
			cins->mOperator = (v.mType->mFlags & DTF_SIGNED) ? IA_LINT2FLOAT : IA_LUINT2FLOAT;
			cins->mSrc[0].mType = IT_INT32;
		}
		cins->mSrc[0].mTemp = stemp;
		cins->mDst.mType = IT_FLOAT;
		cins->mDst.mTemp = proc->AddTemporary(IT_FLOAT);
		block->Append(cins);

		v.mTemp = cins->mDst.mTemp;
		v.mType = type;
	}
	else if (v.mType->mType == DT_TYPE_FLOAT && type->IsIntegerType())
	{
		mErrors->Error(exp->mLocation, EWARN_FLOAT_TO_INT, "Float to int conversion, potential loss of precision");

		InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);

		if (type->mSize <= 2)
		{
			cins->mDst.mType = IT_INT16;
			if (type->mFlags & DTF_SIGNED)
				cins->mOperator = IA_FLOAT2INT;
			else
				cins->mOperator = IA_FLOAT2UINT;
		}
		else
		{
			cins->mDst.mType = IT_INT32;
			if (type->mFlags & DTF_SIGNED)
				cins->mOperator = IA_FLOAT2LINT;
			else
				cins->mOperator = IA_FLOAT2LUINT;
		}

		cins->mSrc[0].mType = IT_FLOAT;
		cins->mSrc[0].mTemp = v.mTemp;

		cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
		block->Append(cins);
		v.mTemp = cins->mDst.mTemp;
		v.mType = type;
	}
	else if (type->mType == DT_TYPE_POINTER && v.mType->mType == DT_TYPE_ARRAY)
	{
		v.mType = type;
		return v;
	}
	else if (type->mType == DT_TYPE_POINTER && v.mType->IsNumericType() && (mCompilerOptions & COPT_CPLUSPLUS))
	{
		mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign numeric value to pointer");
	}
	else if( type->mType == DT_TYPE_BOOL && v.mType->mType == DT_TYPE_POINTER)
	{
		InterInstruction* zins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
		zins->mDst.mType = IT_POINTER;
		zins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
		zins->mConst.mType = IT_POINTER;
		zins->mConst.mMemory = IM_ABSOLUTE;
		zins->mConst.mIntConst = 0;
		block->Append(zins);

		InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
		cins->mOperator = IA_CMPNE;
		cins->mSrc[0].mType = IT_POINTER;
		cins->mSrc[0].mTemp = v.mTemp;
		cins->mSrc[1].mType = IT_POINTER;
		cins->mSrc[1].mTemp = zins->mDst.mTemp;
		cins->mDst.mType = IT_BOOL;
		cins->mDst.mTemp = proc->AddTemporary(IT_BOOL);
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
				InterInstruction	*	xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction	*	xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
				InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
		if (checkTrunc && v.mType->mSize > type->mSize)
		{
			const InterInstruction* ins = block->FindByDst(stemp);
			if (ins && ins->mCode == IC_CONSTANT)
			{
				int64 min = 0, max = 0;

				if (type->mFlags & DTF_SIGNED)
				{
					switch (type->mSize)
					{
					case 1:
						min = -128; max = 127;
						break;
					case 2:
						min = -32768; max = 32767;
						break;
					case 4:
						min = -2147483648LL; max = 2147483647LL;
						break;
					}
				}
				else
				{
					switch (type->mSize)
					{
					case 1:
						max = 255;
						break;
					case 2:
						max = 65535;
						break;
					case 4:
						max = 429467295LL;
						break;
					}
				}

				if (ins->mConst.mIntConst < min || ins->mConst.mIntConst > max)
				{
					mErrors->Error(exp->mLocation, EWARN_CONSTANT_TRUNCATED, "Integer constant truncated");
				}
			}
		}
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

void InterCodeGenerator::InitParameter(InterCodeProcedure* proc, Declaration* dec, int index)
{
	if (!proc->mParamVars[index])
	{
		proc->mParamVars[index] = new InterVariable();
		proc->mParamVars[index]->mIdent = dec->mIdent;
		proc->mParamVars[index]->mDeclaration = dec;
	}
}

void InterCodeGenerator::InitLocalVariable(InterCodeProcedure* proc, Declaration* dec, int index)
{
	if (!proc->mLocalVars[index])
	{
		proc->mLocalVars[index] = new InterVariable();
		proc->mLocalVars[index]->mIdent = dec->mIdent;
		proc->mLocalVars[index]->mDeclaration = dec;
	}
	else
	{
		assert(proc->mLocalVars[index]->mIdent == dec->mIdent);
		assert(proc->mLocalVars[index]->mDeclaration == dec);
	}
}

static const Ident* StructIdent(const Ident* base, const Ident* item)
{
	if (base)
	{
		char	buffer[200];
		strcpy_s(buffer, base->mString);
		strcat_s(buffer, ".");
		strcat_s(buffer, item->mString);
		return Ident::Unique(buffer);
	}
	else
		return item;
}

static void AddGlobalVariableRanges(LinkerObject* lo, Declaration* dec, int offset, const Ident* ident)
{
	switch (dec->mType)
	{
	case DT_TYPE_STRUCT:
	{
		Declaration* deci = dec->mParams;
		while (deci)
		{
			AddGlobalVariableRanges(lo, deci->mBase, offset + deci->mOffset, StructIdent(ident, deci->mIdent));
			deci = deci->mNext;
		}
	} break;
	case DT_TYPE_INTEGER:
	case DT_TYPE_FLOAT:
	case DT_TYPE_POINTER:
	case DT_TYPE_BOOL:
	case DT_TYPE_ENUM:
	{
		LinkerObjectRange	range;
		range.mOffset = offset;
		range.mSize = dec->mStripe;
		switch (dec->mSize)
		{
		case 1:
			range.mIdent = ident;
			lo->mRanges.Push(range);
			break;
		case 2:
			range.mIdent = StructIdent(ident, Ident::Unique("lo"));
			lo->mRanges.Push(range);
			range.mOffset += dec->mStripe;
			range.mIdent = StructIdent(ident, Ident::Unique("hi"));
			lo->mRanges.Push(range);
			break;
		case 4:
			range.mIdent = StructIdent(ident, Ident::Unique("b0"));
			lo->mRanges.Push(range);
			range.mOffset += dec->mStripe;
			range.mIdent = StructIdent(ident, Ident::Unique("b1"));
			lo->mRanges.Push(range);
			range.mOffset += dec->mStripe;
			range.mIdent = StructIdent(ident, Ident::Unique("b2"));
			lo->mRanges.Push(range);
			range.mOffset += dec->mStripe;
			range.mIdent = StructIdent(ident, Ident::Unique("b3"));
			lo->mRanges.Push(range);
			break;
		}
	}	break;

	default:
		if (ident)
		{
			LinkerObjectRange	range;
			range.mIdent = ident;
			range.mOffset = offset;
			range.mSize = dec->mSize * dec->mStripe;
			lo->mRanges.Push(range);
		}
	}
}

void InterCodeGenerator::InitGlobalVariable(InterCodeModule * mod, Declaration* dec)
{
	if (!dec->mLinkerObject)
	{
		InterVariable	*	var = new InterVariable();
		var->mOffset = 0;
		var->mSize = dec->mSize;
		var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_DATA, dec->mAlignment);
		var->mLinkerObject->mVariable = var;
		var->mIdent = dec->mQualIdent;

		Declaration* decb = dec->mBase;
		while (decb && decb->mType == DT_TYPE_ARRAY)
			decb = decb->mBase;

		if (decb && decb->mStripe != 1)
			AddGlobalVariableRanges(var->mLinkerObject, decb, 0, nullptr);

		Declaration* type = dec->mBase;
		while (type->mType == DT_TYPE_ARRAY)
			type = type->mBase;
		if (type->mFlags & DTF_CONST)
			var->mLinkerObject->mFlags |= LOBJF_CONST;
		if (dec->mFlags & DTF_ZEROPAGE)
			var->mLinkerObject->mFlags |= LOBJF_ZEROPAGE;

		var->mIndex = mod->mGlobalVars.Size();
		var->mDeclaration = dec;
		mod->mGlobalVars.Push(var);

		if ((dec->mFlags & DTF_VAR_ALIASING) || (dec->mBase->mType == DT_TYPE_ARRAY && !(type->mFlags & DTF_CONST)))
			var->mAliased = true;

		dec->mVarIndex = var->mIndex;
		dec->mLinkerObject = var->mLinkerObject;

		if (dec->mFlags & DTF_SECTION_START)
			dec->mLinkerObject->mType = LOT_SECTION_START;
		else if (dec->mFlags & DTF_SECTION_END)
			dec->mLinkerObject->mType = LOT_SECTION_END;
		else if (dec->mFlags & DTF_BANK_INLAY)
		{
			dec->mLinkerObject->mType = LOT_INLAY;
			dec->mInlayRegion->mInlayObject = dec->mLinkerObject;
		}

		uint8* d = var->mLinkerObject->AddSpace(var->mSize);
		if (dec->mValue)
		{
			if (dec->mValue->mType == EX_CONSTANT)
			{				
				if (dec->mBase->CanAssign(dec->mValue->mDecType))
					BuildInitializer(mod, d, 0, dec->mValue->mDecValue, var);
				else
					mErrors->Error(dec->mLocation, EERR_INCOMPATIBLE_TYPES, "Incompatible constant initializer");
			}
			else if (dec->mValue->mType == EX_CONSTRUCT)
			{
				DestructStack* destack = nullptr;
				GotoNode* gotos = nullptr;
				dec->mValue->mRight->mDecValue = dec;
				dec->mLinkerObject->mFlags &= ~LOBJF_CONST;
				TranslateExpression(nullptr, mMainInitProc, mMainInitBlock, dec->mValue, destack, gotos, BranchTarget(), BranchTarget(), nullptr);
			}
			else if (dec->mValue->mType == EX_VARIABLE && dec->mValue->mDecType->mType == DT_TYPE_ARRAY && dec->mBase->mType == DT_TYPE_POINTER && dec->mBase->CanAssign(dec->mValue->mDecType))
			{
				Declaration* ndec = new Declaration(dec->mValue->mLocation, DT_CONST_POINTER);
				ndec->mValue = dec->mValue;
				ndec->mBase = dec->mValue->mDecType;

				BuildInitializer(mod, d, 0, ndec, var);
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

	dec->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_NATIVE_CODE, dec->mAlignment);

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
					ref.mRefOffset = aexp->mOffset + int(aexp->mBase->mInteger);
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined immediate operand", aexp->mBase->mQualIdent);
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
			else if (aexp->mType == DT_ARGUMENT || aexp->mType == DT_VARIABLE)
			{
				if (aexp->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE;
					ref.mRefObject = aexp->mLinkerObject;
					ref.mRefOffset = 0;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else if (refvars)
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
				if (aexp->mFlags & DTF_GLOBAL)
				{
					InitGlobalVariable(mod, aexp);

					LinkerReference	ref;
					ref.mObject = dec->mLinkerObject;
					ref.mOffset = offset;
					ref.mFlags = LREF_LOWBYTE;
					ref.mRefObject = aexp->mLinkerObject;
					ref.mRefOffset = 0;
					ref.mRefObject->mFlags |= LOBJF_RELEVANT;
					dec->mLinkerObject->AddReference(ref);
				}
				else if (refvars)
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
				d[offset] = uint8(aexp->mInteger);
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
				d[offset + 0] = uint8(cexp->mLeft->mDecValue->mInteger & 255);
				d[offset + 1] = uint8(cexp->mLeft->mDecValue->mInteger >> 8);
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
					ref.mRefOffset = int(aexp->mInteger);
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
					ref.mRefOffset = aexp->mOffset + int(aexp->mBase->mInteger);
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
				int64 disp = 0;
				if (aexp->mType == DT_LABEL_REF)
				{
					if (aexp->mBase->mBase)
						disp = aexp->mOffset + aexp->mBase->mInteger - offset - 1;
					else
						mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined label", aexp->mBase->mQualIdent);
				}
				else if (aexp->mType == DT_LABEL)
				{
					if (aexp->mBase)
						disp = aexp->mInteger - offset - 1;
					else
						mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Undefined label", aexp->mQualIdent);
				}

				if (disp < -128 || disp > 127)
					mErrors->Error(aexp->mLocation, EERR_ASM_INVALD_OPERAND, "Branch target out of range");

				d[offset] = uint8(disp);
			}
			offset++;
			break;
		}

		cexp = cexp->mRight;
	}

	assert(offset == osize);
}

void InterCodeGenerator::BuildSwitchTree(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock* block, InlineMapper * inlineMapper, ExValue v, const SwitchNodeArray& nodes, int left, int right, int vleft, int vright, InterCodeBasicBlock* dblock)
{
	if (right - left < 3)
	{
		for (int i = left; i < right; i++)
		{
			if (vleft == vright)
			{
				InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
				block->Append(jins);
				block->Close(nodes[i].mBlock, nullptr);
				return;
			}
			else
			{
				InterCodeBasicBlock* cblock = new InterCodeBasicBlock(proc);

				InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				vins->mConst.mType = IT_INT16;
				vins->mConst.mIntConst = nodes[i].mLower;
				vins->mDst.mType = IT_INT16;
				vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
				block->Append(vins);

				if (nodes[i].mLower == nodes[i].mUpper)
				{
					InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
					cins->mOperator = IA_CMPEQ;
					cins->mSrc[0].mType = vins->mDst.mType;
					cins->mSrc[0].mTemp = vins->mDst.mTemp;
					cins->mSrc[1].mType = vins->mDst.mType;
					cins->mSrc[1].mTemp = v.mTemp;
					cins->mDst.mType = IT_BOOL;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

					block->Append(cins);

					InterInstruction* bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
					bins->mSrc[0].mType = IT_BOOL;
					bins->mSrc[0].mTemp = cins->mDst.mTemp;
					block->Append(bins);
				}
				else
				{
					InterCodeBasicBlock* tblock = new InterCodeBasicBlock(proc);

					InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
					cins->mOperator = IA_CMPLS;
					cins->mSrc[0].mType = vins->mDst.mType;
					cins->mSrc[0].mTemp = vins->mDst.mTemp;
					cins->mSrc[1].mType = vins->mDst.mType;
					cins->mSrc[1].mTemp = v.mTemp;
					cins->mDst.mType = IT_BOOL;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

					block->Append(cins);

					InterInstruction* bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
					bins->mSrc[0].mType = IT_BOOL;
					bins->mSrc[0].mTemp = cins->mDst.mTemp;
					block->Append(bins);
					block->Close(cblock, tblock);

					block = tblock;

					InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					vins->mConst.mType = IT_INT16;
					vins->mConst.mIntConst = nodes[i].mUpper;
					vins->mDst.mType = IT_INT16;
					vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
					block->Append(vins);
					
					cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
					cins->mOperator = IA_CMPLES;
					cins->mSrc[0].mType = vins->mDst.mType;
					cins->mSrc[0].mTemp = vins->mDst.mTemp;
					cins->mSrc[1].mType = vins->mDst.mType;
					cins->mSrc[1].mTemp = v.mTemp;
					cins->mDst.mType = IT_BOOL;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
					block->Append(cins);

					bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
					bins->mSrc[0].mType = IT_BOOL;
					bins->mSrc[0].mTemp = cins->mDst.mTemp;
					block->Append(bins);
				}

				block->Close(nodes[i].mBlock, cblock);

				block = cblock;

				if (vleft == nodes[i].mLower)
					vleft = nodes[i].mLower + 1;
			}
		}

		InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

		block->Append(jins);
		block->Close(dblock, nullptr);
	}
	else
	{
		int	center = (left + right) >> 1;

		InterCodeBasicBlock* cblock = new InterCodeBasicBlock(proc);
		InterCodeBasicBlock* rblock = new InterCodeBasicBlock(proc);
		InterCodeBasicBlock* lblock = new InterCodeBasicBlock(proc);

		if (nodes[center].mLower == nodes[center].mUpper)
		{
			int vcenter = nodes[center].mLower;

			InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			vins->mConst.mType = IT_INT16;
			vins->mConst.mIntConst = vcenter;
			vins->mDst.mType = IT_INT16;
			vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
			block->Append(vins);

			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
			cins->mOperator = IA_CMPEQ;
			cins->mSrc[0].mType = vins->mDst.mType;
			cins->mSrc[0].mTemp = vins->mDst.mTemp;
			cins->mSrc[1].mType = vins->mDst.mType;
			cins->mSrc[1].mTemp = v.mTemp;
			cins->mDst.mType = IT_BOOL;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

			block->Append(cins);

			InterInstruction* bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
			bins->mSrc[0].mType = IT_BOOL;
			bins->mSrc[0].mTemp = cins->mDst.mTemp;
			block->Append(bins);

			block->Close(nodes[center].mBlock, cblock);

			InterInstruction* rins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
			rins->mOperator = IA_CMPLS;
			rins->mSrc[0].mType = vins->mDst.mType;
			rins->mSrc[0].mTemp = vins->mDst.mTemp;
			rins->mSrc[1].mType = vins->mDst.mType;
			rins->mSrc[1].mTemp = v.mTemp;
			rins->mDst.mType = IT_BOOL;
			rins->mDst.mTemp = proc->AddTemporary(rins->mDst.mType);

			cblock->Append(rins);

			InterInstruction* rbins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
			rbins->mSrc[0].mType = IT_BOOL;
			rbins->mSrc[0].mTemp = rins->mDst.mTemp;
			cblock->Append(rbins);

			cblock->Close(lblock, rblock);

			BuildSwitchTree(proc, exp, lblock, inlineMapper, v, nodes, left, center, vleft, vcenter - 1, dblock);
			BuildSwitchTree(proc, exp, rblock, inlineMapper, v, nodes, center + 1, right, vcenter + 1, vright, dblock);
		}
		else
		{
			int vlower = nodes[center].mLower, vupper = nodes[center].mUpper;
			
			InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			vins->mConst.mType = IT_INT16;
			vins->mConst.mIntConst = vlower;
			vins->mDst.mType = IT_INT16;
			vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
			block->Append(vins);

			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
			cins->mOperator = IA_CMPLS;
			cins->mSrc[0].mType = vins->mDst.mType;
			cins->mSrc[0].mTemp = vins->mDst.mTemp;
			cins->mSrc[1].mType = vins->mDst.mType;
			cins->mSrc[1].mTemp = v.mTemp;
			cins->mDst.mType = IT_BOOL;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);

			block->Append(cins);

			InterInstruction* bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
			bins->mSrc[0].mType = IT_BOOL;
			bins->mSrc[0].mTemp = cins->mDst.mTemp;
			block->Append(bins);
			block->Close(lblock, cblock);

			vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			vins->mConst.mType = IT_INT16;
			vins->mConst.mIntConst = vupper;
			vins->mDst.mType = IT_INT16;
			vins->mDst.mTemp = proc->AddTemporary(vins->mDst.mType);
			cblock->Append(vins);

			cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
			cins->mOperator = IA_CMPLES;
			cins->mSrc[0].mType = vins->mDst.mType;
			cins->mSrc[0].mTemp = vins->mDst.mTemp;
			cins->mSrc[1].mType = vins->mDst.mType;
			cins->mSrc[1].mTemp = v.mTemp;
			cins->mDst.mType = IT_BOOL;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			cblock->Append(cins);

			bins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
			bins->mSrc[0].mType = IT_BOOL;
			bins->mSrc[0].mTemp = cins->mDst.mTemp;
			cblock->Append(bins);

			cblock->Close(nodes[center].mBlock, rblock);

			BuildSwitchTree(proc, exp, lblock, inlineMapper, v, nodes, left, center, vleft, vlower - 1, dblock);
			BuildSwitchTree(proc, exp, rblock, inlineMapper, v, nodes, center + 1, right, vupper + 1, vright, dblock);
		}

	}
}

void InterCodeGenerator::UnwindDestructStack(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, DestructStack* stack, DestructStack* bottom, InlineMapper* inlineMapper)
{
	while (stack && stack != bottom)
	{
		if (stack->mDestruct)
		{
			DestructStack* destack = nullptr;
			GotoNode* gotos = nullptr;
			TranslateExpression(procType, proc, block, stack->mDestruct, destack, gotos, BranchTarget(), BranchTarget(), inlineMapper);
		}

		stack = stack->mNext;
	}

	if (stack != bottom)
		mErrors->Error(proc->mLocation, EWARN_DESTRUCTOR_MISMATCH, "Destructor sequence mismatch");
}

void InterCodeGenerator::ConnectGotos(Declaration* procType, InterCodeProcedure* proc, GotoNode* gotos, InlineMapper* inlineMapper)
{
	GotoNode* gl = gotos;
	while (gl)
	{
		GotoNode* g = gotos;
		while (g && !(g != gl && g->mExpr->mType == EX_LABEL && g->mExpr->mDecValue->mIdent == gl->mExpr->mDecValue->mIdent))
			g = g->mNext;

		if (gl->mExpr->mType == EX_LABEL)
		{
			if (g)
				mErrors->Error(gl->mExpr->mLocation, EERR_DUPLICATE_DEFINITION, "Label defined twice", gl->mExpr->mDecValue->mIdent);
		}
		else
		{
			if (g)
			{
				if (gl->mDestruct)
				{
					DestructStack* s = gl->mDestruct;
					while (s && s != g->mDestruct)
						s = s->mNext;
					if (s == g->mDestruct)
						UnwindDestructStack(procType, proc, gl->mBlock, gl->mDestruct, s, inlineMapper);
					else
						mErrors->Error(gl->mExpr->mLocation, ERRR_INVALID_GOTO, "Invalid got bypass constructor");
				}

				gl->mBlock->Close(g->mBlock, nullptr);
			}
			else
				mErrors->Error(gl->mExpr->mLocation, EERR_UNDEFINED_OBJECT, "Undefined label", gl->mExpr->mDecValue->mIdent);
		}
		gl = gl->mNext;
	}

}

Location InterCodeGenerator::MapLocation(Expression* exp, InlineMapper* inlineMapper)
{
	if (inlineMapper)
		return Location(exp->mLocation, inlineMapper->mLocation);
	else
		return exp->mLocation;
}

InterCodeGenerator::ExValue InterCodeGenerator::TranslateInline(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, Expression* exp, const BranchTarget& breakBlock, const BranchTarget& continueBlock, InlineMapper* inlineMapper, bool inlineConstexpr, ExValue* lrexp)
{
	DestructStack* destack = nullptr;
	GotoNode* gotos = nullptr;

	ExValue	vl, vr;

	Declaration* fdec = exp->mLeft->mDecValue;
	Expression* fexp = fdec->mValue;
	Declaration* ftype = fdec->mBase;

	proc->mCheckUnreachable = false;

	InlineMapper	nmapper;
	nmapper.mReturn = new InterCodeBasicBlock(proc);
	nmapper.mVarIndex = proc->mNumLocals;
	nmapper.mConstExpr = inlineConstexpr;
	nmapper.mLocation = new Location(MapLocation(exp, inlineMapper));
	proc->mNumLocals += fdec->mNumVars;
	if (inlineMapper)
		nmapper.mDepth = inlineMapper->mDepth + 1;

	Declaration* pdec = ftype->mParams;
	Expression* pex = exp->mRight;
	while (pex)
	{
		int	nindex = proc->mNumLocals++;
		Declaration* vdec = new Declaration(pex->mLocation, DT_VARIABLE);

		InterInstruction* ains = new InterInstruction(MapLocation(pex, inlineMapper), IC_CONSTANT);
		ains->mDst.mType = IT_POINTER;
		ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
		ains->mConst.mType = IT_POINTER;
		ains->mConst.mMemory = IM_LOCAL;
		ains->mConst.mVarIndex = nindex;

		if (pdec)
		{
			if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				nmapper.mParams[pdec->mVarIndex] = nindex;

			vdec->mVarIndex = nindex;
			vdec->mBase = pdec->mBase;
			if (pdec->mBase->mType == DT_TYPE_ARRAY || pdec->mBase->mType == DT_TYPE_REFERENCE || pdec->mBase->mType == DT_TYPE_RVALUEREF)
				ains->mConst.mOperandSize = 2;
			else
				ains->mConst.mOperandSize = pdec->mSize;
			vdec->mSize = ains->mConst.mOperandSize;
			vdec->mIdent = pdec->mIdent;
			vdec->mQualIdent = pdec->mQualIdent;
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

		if (pdec && (pdec->mBase->mType == DT_TYPE_STRUCT || pdec->mBase->mType == DT_TYPE_UNION))
			vr = TranslateExpression(procType, proc, block, texp, destack, gotos, breakBlock, continueBlock, inlineMapper, &vp);
		else
			vr = TranslateExpression(procType, proc, block, texp, destack, gotos, breakBlock, continueBlock, inlineMapper, nullptr);

		if (!(pdec && pdec->mBase->IsReference()) && (vr.mType->mType == DT_TYPE_STRUCT || vr.mType->mType == DT_TYPE_UNION))
		{
			if (pdec && !pdec->mBase->CanAssign(vr.mType))
				mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types", pdec->mBase->MangleIdent(), vr.mType->MangleIdent());

			vr = Dereference(proc, texp, block, inlineMapper, vr, 1);

			if (vr.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

			if (vp.mTemp != vr.mTemp)
				CopyStructSimple(proc, exp, block, inlineMapper, vp, vr);
		}
		else
		{
			if (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION)
				vr = Dereference(proc, texp, block, inlineMapper, vr, 1);
			else if (pdec && (pdec->mBase->IsReference() && !vr.mType->IsReference()))
				vr = Dereference(proc, texp, block, inlineMapper, vr, 1);
			else if (vr.mType->IsReference() && !(pdec && pdec->mBase->IsReference()))
			{
				vr.mReference++;
				vr = Dereference(proc, texp, block, inlineMapper, vr);
			}
			else
				vr = Dereference(proc, texp, block, inlineMapper, vr);

			if (pdec)
			{
				if (pdec->mBase->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_INTEGER)
				{
					Expression* er = texp;
					while (er && er->mType == EX_COMMA)
						er = er->mRight;
					if (er && er->mType == EX_CONSTANT && er->mDecValue->mType == DT_CONST_INTEGER && er->mDecValue->mInteger == 0)
					{
						mErrors->Error(exp->mLocation, EWARN_NUMERIC_0_USED_AS_NULLPTR, "Numeric 0 used for nullptr");
						vr = CoerceType(proc, texp, block, inlineMapper, vr, pdec->mBase);
					}
				}

				if (!pdec->mBase->CanAssign(vr.mType))
				{
					pdec->mBase->CanAssign(vr.mType);
					mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types", pdec->mBase->MangleIdent(), vr.mType->MangleIdent());
				}
				vr = CoerceType(proc, texp, block, inlineMapper, vr, pdec->mBase);
			}
			else if (vr.mType->IsIntegerType() && vr.mType->mSize < 2)
			{
				vr = CoerceType(proc, texp, block, inlineMapper, vr, TheSignedIntTypeDeclaration);
			}

			InterInstruction* wins = new InterInstruction(MapLocation(texp, inlineMapper), IC_STORE);
			wins->mSrc[1].mMemory = IM_INDIRECT;
			wins->mSrc[0].mType = vr.mReference > 0 ? IT_POINTER : InterTypeOf(vr.mType);
			wins->mSrc[0].mTemp = vr.mTemp;
//			assert(wins->mSrc[0].mType != IT_NONE);
			wins->mSrc[1].mType = IT_POINTER;
			wins->mSrc[1].mTemp = ains->mDst.mTemp;
			if (pdec)
			{
				if (pdec->mBase->mType == DT_TYPE_ARRAY || pdec->mBase->mType == DT_TYPE_REFERENCE || pdec->mBase->mType == DT_TYPE_RVALUEREF)
					wins->mSrc[1].mOperandSize = 2;
				else
					wins->mSrc[1].mOperandSize = pdec->mSize;
			}
			else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
				wins->mSrc[1].mOperandSize = vr.mType->mSize;
			else
				wins->mSrc[1].mOperandSize = 2;

			if (!pdec || !(pdec->mFlags & DTF_FPARAM_UNUSED))
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
		if (lrexp)
		{
			nmapper.mResultExp = lrexp;
		}
		else
		{
			int	nindex = proc->mNumLocals++;
			nmapper.mResult = nindex;

			rdec = new Declaration(ftype->mLocation, DT_VARIABLE);
			rdec->mVarIndex = nindex;
			rdec->mBase = ftype->mBase;
			rdec->mSize = rdec->mBase->mSize;
		}
	}

	DestructStack* idestack = nullptr;
	GotoNode* igotos = nullptr;

	vl = TranslateExpression(ftype, proc, block, fexp, idestack, igotos, BranchTarget(), BranchTarget(), &nmapper);

	InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
	block->Append(jins);

	block->Close(nmapper.mReturn, nullptr);
	block = nmapper.mReturn;

	ConnectGotos(ftype, proc, igotos, &nmapper);

	// Unwind inner destruct stack
	UnwindDestructStack(ftype, proc, block, idestack, nullptr, &nmapper);

	// Uwind parameter passing stack
	UnwindDestructStack(ftype, proc, block, destack, nullptr, inlineMapper);

	if (rdec)
	{
		InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
		ins->mDst.mType = IT_POINTER;
		ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
		ins->mConst.mType = IT_POINTER;
		ins->mConst.mOperandSize = rdec->mSize;
		ins->mConst.mIntConst = rdec->mOffset;
		ins->mConst.mVarIndex = rdec->mVarIndex;
		ins->mConst.mMemory = IM_LOCAL;

		block->Append(ins);

		ExValue rv(rdec->mBase, ins->mDst.mTemp, 1);
		if (!rdec->mBase->IsReference() && rdec->mBase->mType != DT_TYPE_STRUCT)
			rv = Dereference(proc, exp, block, inlineMapper, rv);
		return rv;
	}
	else if (lrexp)
	{
		return *lrexp;
	}
	else
		return ExValue(TheVoidTypeDeclaration);
}

void InterCodeGenerator::CopyStructSimple(InterCodeProcedure* proc, Expression * exp, InterCodeBasicBlock* block, InlineMapper * inlineMapper, ExValue vl, ExValue vr)
{
	int		ne = 0;
	Declaration* mdec = nullptr;
	if (vl.mType->mType == DT_TYPE_STRUCT)
	{
		Declaration* dec = vl.mType->mParams;
		while (dec)
		{
			if (dec->mType == DT_ELEMENT && !(dec->mFlags & DTF_STATIC) && dec->mBase->IsSimpleType())
			{
				mdec = dec->mBase;
				ne++;
			}
			dec = dec->mNext;
		}
	}

	bool	cvol = false;
	if ((vl.mType->mFlags & DTF_VOLATILE) || (vr.mType->mFlags & DTF_VOLATILE))
		cvol = true;
	else
	{
		Declaration* dec = vl.mType->mParams;
		while (!cvol && dec)
		{
			if (dec->mType == DT_ELEMENT && (dec->mBase->mFlags & DTF_VOLATILE))
				cvol = true;
			dec = dec->mNext;
		}
		dec = vr.mType->mParams;
		while (!cvol && dec)
		{
			if (dec->mType == DT_ELEMENT && (dec->mBase->mFlags & DTF_VOLATILE))
				cvol = true;
			dec = dec->mNext;
		}
	}

	// Single element structs are copied as individual value
	if (ne == 1 && mdec->mSize == vl.mType->mSize)
	{
		InterInstruction* lins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD);
		lins->mNumOperands = 1;

		lins->mSrc[0].mType = IT_POINTER;
		lins->mSrc[0].mTemp = vr.mTemp;
		lins->mSrc[0].mMemory = IM_INDIRECT;
		lins->mSrc[0].mOperandSize = mdec->mSize;
		lins->mSrc[0].mStride = mdec->mStripe;

		lins->mDst.mType = InterTypeOf(mdec);
		lins->mDst.mTemp = proc->AddTemporary(lins->mDst.mType);
		block->Append(lins);

		InterInstruction* sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
		sins->mNumOperands = 2;

		sins->mSrc[1].mType = IT_POINTER;
		sins->mSrc[1].mTemp = vl.mTemp;
		sins->mSrc[1].mMemory = IM_INDIRECT;
		sins->mSrc[1].mOperandSize = mdec->mSize;
		sins->mSrc[1].mStride = mdec->mStripe;

		sins->mSrc[0] = lins->mDst;
		block->Append(sins);
	}
	else
	{
		InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_COPY);
		cins->mNumOperands = 2;

		cins->mSrc[0].mType = IT_POINTER;
		cins->mSrc[0].mTemp = vr.mTemp;
		cins->mSrc[0].mMemory = IM_INDIRECT;
		cins->mSrc[0].mOperandSize = vr.mType->mSize;
		cins->mSrc[0].mStride = vr.mType->mStripe;

		cins->mSrc[1].mOperandSize = vl.mType->mSize;
		cins->mSrc[1].mType = IT_POINTER;
		cins->mSrc[1].mTemp = vl.mTemp;
		cins->mSrc[1].mMemory = IM_INDIRECT;
		cins->mSrc[1].mStride = vl.mType->mStripe;

		cins->mConst.mOperandSize = vl.mType->mSize;
		cins->mVolatile = cvol;
		block->Append(cins);
	}
}

void InterCodeGenerator::CopyStruct(InterCodeProcedure* proc, Expression* exp, InterCodeBasicBlock*& block, ExValue vl, ExValue vr, InlineMapper* inlineMapper, bool moving)
{
	if (vr.mTemp == vl.mTemp)
		return;

	if (vl.mType->mCopyConstructor || moving && vl.mType->mMoveConstructor)
	{
		Declaration* ccdec = vl.mType->mCopyConstructor;
		if (moving && vl.mType->mMoveConstructor)
			ccdec = vl.mType->mMoveConstructor;

		if (!ccdec->mLinkerObject)
			this->TranslateProcedure(proc->mModule, ccdec->mValue, ccdec);

		bool	canInline = (mCompilerOptions & COPT_OPTIMIZE_INLINE) && !(inlineMapper && inlineMapper->mDepth > 10);
		bool	doInline = false;

		if (canInline)
		{
			if (ccdec->mFlags & DTF_INLINE)
			{
				if ((ccdec->mFlags & DTF_REQUEST_INLINE) || (mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE))
				{
					if (proc->mNativeProcedure || !(ccdec->mFlags & DTF_NATIVE))
						doInline = true;
				}
			}
		}

		if (doInline)
		{
			DestructStack* destack = nullptr;
			GotoNode* gotos = nullptr;

			Expression* fexp = ccdec->mValue;
			Declaration* ftype = ccdec->mBase;

			InlineMapper	nmapper;
			nmapper.mReturn = new InterCodeBasicBlock(proc);
			nmapper.mVarIndex = proc->mNumLocals;
			nmapper.mConstExpr = false;
			proc->mNumLocals += ccdec->mNumVars;
			if (inlineMapper)
				nmapper.mDepth = inlineMapper->mDepth + 1;

			Declaration* pdec = ftype->mParams;
			int	nindex = proc->mNumLocals++;
			Declaration* vdec = new Declaration(exp->mLocation, DT_VARIABLE);

			InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			ains->mDst.mType = IT_POINTER;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			ains->mConst.mType = IT_POINTER;
			ains->mConst.mMemory = IM_LOCAL;
			ains->mConst.mVarIndex = nindex;

			if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				nmapper.mParams[pdec->mVarIndex] = nindex;

			vdec->mVarIndex = nindex;
			vdec->mBase = pdec->mBase;
			ains->mConst.mOperandSize = 2;
			vdec->mSize = ains->mConst.mOperandSize;
			vdec->mIdent = pdec->mIdent;
			vdec->mQualIdent = pdec->mQualIdent;
			block->Append(ains);

			InterInstruction* wins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
			wins->mNumOperands = 2;
			wins->mSrc[1].mMemory = IM_INDIRECT;
			wins->mSrc[0].mType = vl.mReference > 0 ? IT_POINTER : InterTypeOf(vl.mType);
			wins->mSrc[0].mTemp = vl.mTemp;
			wins->mSrc[1].mType = IT_POINTER;
			wins->mSrc[1].mTemp = ains->mDst.mTemp;
			wins->mSrc[1].mOperandSize = 2;
			if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				block->Append(wins);

			pdec = pdec->mNext;

			nindex = proc->mNumLocals++;
			vdec = new Declaration(exp->mLocation, DT_VARIABLE);

			ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			ains->mNumOperands = 0;
			ains->mDst.mType = IT_POINTER;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			ains->mConst.mType = IT_POINTER;
			ains->mConst.mMemory = IM_LOCAL;
			ains->mConst.mVarIndex = nindex;

			if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				nmapper.mParams[pdec->mVarIndex] = nindex;

			vdec->mVarIndex = nindex;
			vdec->mBase = pdec->mBase;
			ains->mConst.mOperandSize = 2;
			vdec->mSize = ains->mConst.mOperandSize;
			vdec->mIdent = pdec->mIdent;
			vdec->mQualIdent = pdec->mQualIdent;
			block->Append(ains);

			wins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
			wins->mNumOperands = 2;
			wins->mSrc[1].mMemory = IM_INDIRECT;
			wins->mSrc[0].mType = vr.mReference > 0 ? IT_POINTER : InterTypeOf(vr.mType);;
			wins->mSrc[0].mTemp = vr.mTemp;
			wins->mSrc[1].mType = IT_POINTER;
			wins->mSrc[1].mTemp = ains->mDst.mTemp;
			wins->mSrc[1].mOperandSize = 2;
			if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				block->Append(wins);

			if (!fexp)
				mErrors->Error(exp->mLocation, EERR_CALL_OF_DELETED_FUNCTION, "Call of deleted copy constructor");
			else
				TranslateExpression(ftype, proc, block, fexp, destack, gotos, BranchTarget(), BranchTarget(), &nmapper);

			InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			block->Append(jins);

			block->Close(nmapper.mReturn, nullptr);
			block = nmapper.mReturn;
		}
		else if (ccdec->mBase->mFlags & DTF_FASTCALL)
		{
			InterInstruction* psins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			psins->mDst.mType = IT_POINTER;
			psins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
			psins->mDst.mOperandSize = 2;
			psins->mConst.mType = IT_POINTER;
			psins->mConst.mVarIndex = 0;
			psins->mConst.mIntConst = 0;
			psins->mConst.mOperandSize = 2;
			if (ccdec->mBase->mFlags & DTF_FASTCALL)
			{
				psins->mConst.mMemory = IM_FFRAME;
				psins->mConst.mVarIndex += ccdec->mBase->mFastCallBase;
			}
			else
				psins->mConst.mMemory = IM_FRAME;
			block->Append(psins);

			InterInstruction* ssins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
			ssins->mSrc[0].mType = IT_POINTER;
			ssins->mSrc[0].mTemp = vl.mTemp;
			ssins->mSrc[0].mMemory = IM_INDIRECT;
			ssins->mSrc[0].mOperandSize = 2;
			ssins->mSrc[1] = psins->mDst;
			block->Append(ssins);

			InterInstruction* plins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			plins->mDst.mType = IT_POINTER;
			plins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
			plins->mDst.mOperandSize = 2;
			plins->mConst.mType = IT_POINTER;
			plins->mConst.mVarIndex = 2;
			plins->mConst.mIntConst = 0;
			plins->mConst.mOperandSize = 2;
			if (ccdec->mBase->mFlags & DTF_FASTCALL)
			{
				plins->mConst.mMemory = IM_FFRAME;
				plins->mConst.mVarIndex += ccdec->mBase->mFastCallBase;
			}
			else
				plins->mConst.mMemory = IM_FRAME;
			block->Append(plins);

			InterInstruction* slins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
			slins->mSrc[0].mType = IT_POINTER;
			slins->mSrc[0].mTemp = vr.mTemp;
			slins->mSrc[0].mMemory = IM_INDIRECT;
			slins->mSrc[0].mOperandSize = 2;
			slins->mSrc[1] = plins->mDst;
			block->Append(slins);

			proc->AddCalledFunction(proc->mModule->mProcedures[ccdec->mVarIndex]);

			InterInstruction* pcins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			pcins->mDst.mType = IT_POINTER;
			pcins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
			pcins->mConst.mType = IT_POINTER;
			pcins->mConst.mVarIndex = ccdec->mVarIndex;
			pcins->mConst.mIntConst = 0;
			pcins->mConst.mOperandSize = 2;
			pcins->mConst.mMemory = IM_PROCEDURE;
			pcins->mConst.mLinkerObject = ccdec->mLinkerObject;
			block->Append(pcins);

			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CALL);
			if (ccdec->mFlags & DTF_NATIVE)
				cins->mCode = IC_CALL_NATIVE;
			else
				cins->mCode = IC_CALL;
			cins->mSrc[0] = pcins->mDst;
			cins->mNumOperands = 1;

			block->Append(cins);
		}
	}
	else
		CopyStructSimple(proc, exp, block, inlineMapper, vl, vr);
}

InterCodeGenerator::ExValue InterCodeGenerator::TranslateExpression(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock*& block, Expression* exp, DestructStack*& destack, GotoNode*& gotos, const BranchTarget& breakBlock, const BranchTarget& continueBlock, InlineMapper* inlineMapper, ExValue* lrexp)
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
		case EX_COMMA:
			if (exp->mLeft)
				vr = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			exp = exp->mRight;
			if (!exp)
				return ExValue(TheVoidTypeDeclaration);
			break;

		case EX_CONSTRUCT:
		{
			if (exp->mLeft->mLeft)
				TranslateExpression(procType, proc, block, exp->mLeft->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper, lrexp);

			if (exp->mLeft->mRight)
			{
				DestructStack* de = new DestructStack();
				de->mNext = destack;
				de->mDestruct = exp->mLeft->mRight;
				destack = de;
			}

			exp = exp->mRight;
			if (!exp)
				return ExValue(TheVoidTypeDeclaration);
		}
			break;

		case EX_PACK:
		case EX_PACK_TYPE:
			mErrors->Error(exp->mLocation, EERR_INVALID_PACK_USAGE, "Invalid pack usage");
			return ExValue(TheVoidTypeDeclaration);

		case EX_RESULT:
		{
			if (lrexp)
				return *lrexp;

			InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);

			if (inlineMapper)
			{
				if (inlineMapper->mResultExp)
				{
					ains->mDst.mTemp = inlineMapper->mResultExp->mTemp;
				}
				else
				{
					ains->mCode = IC_CONSTANT;
					ains->mDst.mType = IT_POINTER;
					ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
					ains->mConst.mType = IT_POINTER;
					ains->mConst.mOperandSize = procType->mBase->mSize;
					ains->mConst.mIntConst = 0;
					ains->mConst.mVarIndex = inlineMapper->mResult;
					ains->mConst.mMemory = IM_LOCAL;
					block->Append(ains);
				}
			}
			else
			{
				InterInstruction* pins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				pins->mDst.mType = IT_POINTER;
				pins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				pins->mConst.mType = IT_POINTER;
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
				ains->mNumOperands = 1;
				block->Append(ains);
			}

			return ExValue(exp->mDecType, ains->mDst.mTemp, 1);
		}
		case EX_CLEANUP:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
			return vl;
		}

		case EX_CONSTANT:
			dec = exp->mDecValue;
			switch (dec->mType)
			{
			case DT_CONST_INTEGER:
				{
				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mType = ins->mDst.mType;

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
				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mType = ins->mDst.mType;
				ins->mConst.mFloatConst = dec->mNumber;
				block->Append(ins);
				return ExValue(dec->mBase, ins->mDst.mTemp);

			} break;

			case DT_CONST_ADDRESS:
			{
				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mType = IT_POINTER;
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

				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = InterTypeOf(dec->mBase);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mType = ins->mDst.mType;
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

				InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mType = IT_POINTER;
				ins->mConst.mVarIndex = dec->mVarIndex;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mMemory = IM_PROCEDURE;
				ins->mConst.mIntConst = 0;
				block->Append(ins);
				return ExValue(TheVoidPointerTypeDeclaration, ins->mDst.mTemp);
			}

			case DT_CONST_POINTER:
			{
				vl = TranslateExpression(procType, proc, block, dec->mValue, destack, gotos, breakBlock, continueBlock, inlineMapper);
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
					var->mIdent = dec->mQualIdent;
					var->mOffset = 0;
					var->mSize = dec->mSize;
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_DATA, dec->mAlignment);
					var->mLinkerObject->mVariable = var;

					if (dec->mBase->mFlags & DTF_CONST)
						var->mLinkerObject->mFlags |= LOBJF_CONST;
					else if ((dec->mFlags & DTF_VAR_ALIASING) || dec->mBase->mType == DT_TYPE_ARRAY)
						var->mAliased = true;
					var->mLinkerObject->AddData(dec->mData, dec->mSize);

					LinkerObject* aobj = mLinker->FindSame(var->mLinkerObject);
					if (aobj)
						var->mLinkerObject = aobj;

					dec->mLinkerObject = var->mLinkerObject;
					proc->mModule->mGlobalVars.Push(var);
				}

				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mType = IT_POINTER;
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
					if (dec->mFlags & DTF_VAR_ALIASING)
						var->mAliased = true;
					var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_DATA, dec->mAlignment);
					var->mLinkerObject->mVariable = var;
					dec->mLinkerObject = var->mLinkerObject;
					var->mIdent = dec->mQualIdent;
					var->mDeclaration = dec;
					dec->mVarIndex = proc->mModule->mGlobalVars.Size();
					var->mIndex = dec->mVarIndex;
					proc->mModule->mGlobalVars.Push(var);

					uint8* d = var->mLinkerObject->AddSpace(dec->mSize);;

					BuildInitializer(proc->mModule, d, 0, dec, var);
				}

				InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
				ins->mConst.mType = IT_POINTER;
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
			
			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			ins->mDst.mType = IT_POINTER;
			ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
			ins->mConst.mType = IT_POINTER;
			ins->mConst.mOperandSize = dec->mSize;
			ins->mConst.mIntConst = dec->mOffset;

			int	lbits = dec->mBits, lshift = dec->mShift;

			if (dec->mType == DT_VARIABLE_REF)
				dec = dec->mBase;

			ins->mConst.mVarIndex = dec->mVarIndex;

			int ref = 1;
			if (dec->mType == DT_ARGUMENT)
			{
				if (dec->mFlags & DTF_FPARAM_UNUSED)
				{
					ins->mConst.mMemory = IM_LOCAL;
					if (inlineMapper)
						ins->mConst.mVarIndex += inlineMapper->mVarIndex;
					InitLocalVariable(proc, dec, ins->mConst.mVarIndex);
					proc->mCheckUnreachable = false;
				}
				else if (inlineMapper)
				{
					ins->mConst.mMemory = IM_LOCAL;
					ins->mConst.mVarIndex = inlineMapper->mParams[dec->mVarIndex];
					InitLocalVariable(proc, dec, ins->mConst.mVarIndex);
				}
				else if (procType->mFlags & DTF_FASTCALL)
				{
					ins->mConst.mMemory = IM_FPARAM;
//					ins->mConst.mVarIndex += procType->mFastCallBase;
					InitParameter(proc, dec, ins->mConst.mVarIndex);
				}
				else
				{
					ins->mConst.mMemory = IM_PARAM;
					InitParameter(proc, dec, ins->mConst.mVarIndex);
				}

				if (dec->mBase->mType == DT_TYPE_ARRAY)
				{
					ref = 2;
					ins->mConst.mOperandSize = 2;
				}
			}
			else if (dec->mType == DT_CONST_STRUCT || (dec->mFlags & DTF_GLOBAL))
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
				mErrors->Error(dec->mLocation, EERR_VARIABLE_TYPE, "Undefined variable type for", dec->mQualIdent);

			if (exp->mDecType->IsReference())
				return ExValue(exp->mDecType->mBase, ins->mDst.mTemp, ref + 1);
			else
				return ExValue(exp->mDecType, ins->mDst.mTemp, ref, lbits, lshift);
		}


		case EX_ASSIGNMENT:
		case EX_INITIALIZATION:
		{
			if (exp->mLeft->mDecType && exp->mLeft->mDecType->mType == DT_TYPE_STRUCT)
			{
				vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper, &vl);
			}
			else if (exp->mType == EX_INITIALIZATION && exp->mLeft->mDecType && exp->mLeft->mDecType->IsReference())
			{
				vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vl.mType = exp->mLeft->mDecType;
			}
			else
			{
				vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			}

			if (exp->mType == EX_ASSIGNMENT)
			{
				vl = ToValue(proc, exp, block, inlineMapper, vl);
				vr = ToValue(proc, exp, block, inlineMapper, vr);
			}

			if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_INTEGER)
			{
				Expression* er = exp->mRight;
				while (er && er->mType == EX_COMMA)
					er = er->mRight;
				if (er && er->mType == EX_CONSTANT && er->mDecValue->mType == DT_CONST_INTEGER && er->mDecValue->mInteger == 0)
				{
					mErrors->Error(exp->mLocation, EWARN_NUMERIC_0_USED_AS_NULLPTR, "Numeric 0 used for nullptr");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, exp->mLeft->mDecType);
				}
			}

			if (exp->mToken == TK_ASSIGN || !(vl.mType->mType == DT_TYPE_POINTER && vr.mType->IsIntegerType() && (exp->mToken == TK_ASSIGN_ADD || exp->mToken == TK_ASSIGN_SUB)))
			{
				if (!vl.mType->CanAssign(vr.mType))
				{
					vl.mType->CanAssign(vr.mType);
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types", vl.mType->MangleIdent(), vr.mType->MangleIdent());
				}
			} 

			if (exp->mType != EX_INITIALIZATION && (vl.mType->mFlags & DTF_CONST))
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot assign to const type");

			if (exp->mType == EX_INITIALIZATION && exp->mRight->mType == EX_CONSTANT && exp->mRight->mDecValue && exp->mRight->mDecValue->mLinkerObject)
			{
				if (!(exp->mRight->mDecValue->mFlags & DTF_VAR_ALIASING))
					exp->mRight->mDecValue->mLinkerObject->mFlags |= LOBJF_CONST;
			}


			if (vl.mType->IsReference())
			{
				if (vr.mType->IsReference())
					vr = Dereference(proc, exp, block, inlineMapper, vr, 0);
				else
				{
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
					if (vr.mReference != 1)
						mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");
				}

				vl = Dereference(proc, exp, block, inlineMapper, vl, 2);

				if (vl.mReference != 2)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand reference expression");

				if (vr.mTemp != vl.mTemp)
				{
					InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
					ins->mSrc[0].mType = IT_POINTER;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mSrc[0].mMemory = IM_INDIRECT;
					ins->mSrc[0].mOperandSize = 2;
					ins->mSrc[0].mStride = vr.mType->mStripe;

					ins->mSrc[1].mType = IT_POINTER;
					ins->mSrc[1].mTemp = vl.mTemp;
					ins->mSrc[1].mMemory = IM_INDIRECT;
					ins->mSrc[1].mOperandSize = 2;
					ins->mSrc[1].mStride = vl.mType->mStripe;
					ins->mNumOperands = 2;

					block->Append(ins);
				}
			}
			else if (vl.mType->mType == DT_TYPE_STRUCT || vl.mType->mType == DT_TYPE_ARRAY || vl.mType->mType == DT_TYPE_UNION)
			{
				vr = ToValue(proc, exp, block, inlineMapper, vr);

				vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
				vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");
				if (vr.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

				if (vr.mTemp != vl.mTemp)
					CopyStructSimple(proc, exp, block, inlineMapper, vl, vr);
			}
			else
			{
				vr = ToValue(proc, exp, block, inlineMapper, vr);

				if (vl.mType->mType == DT_TYPE_POINTER && (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION))
				{
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
					vr.mReference = 0;
					vr.mType = vl.mType;
				}
				else
					vr = Dereference(proc, exp, block, inlineMapper, vr);

				vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

				if (vl.mReference != 1)
					mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

				if (exp->mToken != TK_ASSIGN)
				{
					ExValue	vll = Dereference(proc, exp, block, inlineMapper, vl);

					if (vl.mType->mType == DT_TYPE_POINTER)
					{
						InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
						cins->mConst.mType = IT_INT16;

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
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Invalid argument for pointer inc/dec", vr.mType->MangleIdent());

						cins->mDst.mType = IT_INT16;
						cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
						block->Append(cins);

						vr = CoerceType(proc, exp, block, inlineMapper, vr, TheSignedIntTypeDeclaration);

						InterInstruction	*	mins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
						mins->mOperator = IA_MUL;
						mins->mSrc[0].mType = IT_INT16;
						mins->mSrc[0].mTemp = vr.mTemp;
						mins->mSrc[1].mType = IT_INT16;
						mins->mSrc[1].mTemp = cins->mDst.mTemp;
						mins->mDst.mType = IT_INT16;
						mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
						block->Append(mins);

						InterInstruction	*	ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_LEA);
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
							vll = CoerceType(proc, exp, block, inlineMapper, vll, otype);
						}

						vr = CoerceType(proc, exp, block, inlineMapper, vr, otype, exp->mToken != TK_ASSIGN_AND);

						InterInstruction	*	oins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
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

						vr = CoerceType(proc, exp, block, inlineMapper, vr, vl.mType);
					}
				}
				else
				{
					vr = CoerceType(proc, exp, block, inlineMapper, vr, vl.mType);
				}

				StoreValue(proc, exp, block, inlineMapper, vl, vr);
			}
			}

			return vl;

		case EX_INDEX:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

			vl = ToValue(proc, exp, block, inlineMapper, vl);
			vr = ToValue(proc, exp, block, inlineMapper, vr);

			int stride = vl.mType->Stride();

			if (vl.mType->mType == DT_TYPE_ARRAY && exp->mRight->mType == EX_CONSTANT && exp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				if (vl.mType->mFlags & DTF_DEFINED)
				{
					int64	index = exp->mRight->mDecValue->mInteger;
					if (vl.mType->mSize > 0 && (index < 0 || index * stride >= vl.mType->mSize * vl.mType->mStripe))
						mErrors->Error(exp->mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Constant array index out of bounds");
				}
			}

			vl = Dereference(proc, exp, block, inlineMapper, vl, vl.mType->mType == DT_TYPE_POINTER ? 0 : 1);
			vr = Dereference(proc, exp, block, inlineMapper, vr);

			if (vl.mType->mType != DT_TYPE_ARRAY && vl.mType->mType != DT_TYPE_POINTER)
			{
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Invalid type for indexing");
				return ExValue(TheConstVoidTypeDeclaration, -1);
			}

			if (!vr.mType->IsIntegerType())
				mErrors->Error(exp->mLocation, EERR_INVALID_INDEX, "Index operand is not integral number");

			vr = CoerceType(proc, exp, block, inlineMapper, vr, TheSignedIntTypeDeclaration);

			InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			cins->mConst.mType = IT_INT16;
			cins->mConst.mIntConst = stride;
			cins->mDst.mType = IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			block->Append(cins);

			InterInstruction	*	mins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
			mins->mOperator = IA_MUL;
			mins->mSrc[0].mType = IT_INT16;
			mins->mSrc[0].mTemp = vr.mTemp;
			mins->mSrc[1].mType = IT_INT16;
			mins->mSrc[1].mTemp = cins->mDst.mTemp;
			mins->mDst.mType = IT_INT16;
			mins->mDst.mTemp = proc->AddTemporary(mins->mDst.mType);
			block->Append(mins);

			InterInstruction	*	ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_LEA);
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
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

			vl = ToValue(proc, exp, block, inlineMapper, vl);

			vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

			InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			cins->mConst.mType = IT_INT16;
			cins->mConst.mIntConst = exp->mDecValue->mOffset;
			cins->mDst.mType = IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			block->Append(cins);

			InterInstruction	*	ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_LEA);
			ains->mSrc[1].mMemory = IM_INDIRECT;
			ains->mSrc[0].mType = IT_INT16;
			ains->mSrc[0].mTemp = cins->mDst.mTemp;
			ains->mSrc[1].mType = IT_POINTER;
			ains->mSrc[1].mTemp = vl.mTemp;
			ains->mDst.mType = IT_POINTER;
			ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
			block->Append(ains);

			if (exp->mDecType->IsReference())
				return ExValue(exp->mDecType->mBase, ains->mDst.mTemp, 2);
			else
				return ExValue(exp->mDecType, ains->mDst.mTemp, 1, exp->mDecValue->mBits, exp->mDecValue->mShift);
		}

		case EX_BINARY:
			{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

			vl = ToValue(proc, exp, block, inlineMapper, vl);
			vr = ToValue(proc, exp, block, inlineMapper, vr);

			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);

			if (vl.mType->mType == DT_TYPE_POINTER || vl.mType->mType == DT_TYPE_ARRAY)
			{
				if (vl.mType->mType == DT_TYPE_POINTER)
					vl = Dereference(proc, exp, block, inlineMapper, vl);
				else
				{
					vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vl.mType->mBase;
					ptype->mSize = 2;
					ptype->mStride = vl.mType->mStride;
					vl.mReference = 0;
					vl.mType = exp->mDecType; // ptype;
				}

				if (vr.mType->mType == DT_TYPE_POINTER)
					vr = Dereference(proc, exp, block, inlineMapper, vr);
				else if (vr.mType->mType == DT_TYPE_ARRAY)
				{
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vr.mType->mBase;
					ptype->mSize = 2;
					ptype->mStride = vr.mType->mStride;
					vr.mReference = 0;
					vr.mType = ptype;
				}
				else
					vr = Dereference(proc, exp, block, inlineMapper, vr);

				if (vr.mType->IsIntegerType())
				{
					InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					cins->mConst.mType = IT_INT16;

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

					vr = CoerceType(proc, exp, block, inlineMapper, vr, TheSignedIntTypeDeclaration);

					InterInstruction	*	mins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
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
				else if (vr.mType->mType == DT_TYPE_POINTER && vr.mType->mBase->IsConstSame(vl.mType->mBase))
				{
					if (exp->mToken == TK_SUB)
					{
						InterInstruction	*	clins = new InterInstruction(MapLocation(exp, inlineMapper), IC_TYPECAST), *	crins = new InterInstruction(MapLocation(exp, inlineMapper), IC_TYPECAST);
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

						int		s = vl.mType->Stride();
						bool	binary = !(s & (s - 1));
						if (binary)
						{
							int n = 0;
							while (s > 1)
							{
								s >>= 1;
								n++;
							}
							s = n;
						}

						InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
						cins->mConst.mType = IT_INT16;
						cins->mConst.mIntConst = s;
						cins->mDst.mType = IT_INT16;
						cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
						block->Append(cins);

						InterInstruction	*	sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR), *	dins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
						sins->mOperator = IA_SUB;
						sins->mSrc[0].mType = IT_INT16;
						sins->mSrc[0].mTemp = crins->mDst.mTemp;
						sins->mSrc[1].mType = IT_INT16;
						sins->mSrc[1].mTemp = clins->mDst.mTemp;
						sins->mDst.mType = IT_INT16;
						sins->mDst.mTemp = proc->AddTemporary(sins->mDst.mType);
						block->Append(sins);

						dins->mOperator = binary ? IA_SAR : IA_DIVS;
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
					vr = Dereference(proc, exp, block, inlineMapper, vr);
				else
				{
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);

					Declaration* ptype = new Declaration(exp->mLocation, DT_TYPE_POINTER);
					ptype->mBase = vr.mType->mBase;
					ptype->mSize = 2;
					vr.mType = ptype;
				}

				if (vl.mType->IsIntegerType())
				{
					InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					cins->mConst.mType = IT_INT16;

					if (exp->mToken == TK_ADD)
					{
						cins->mConst.mIntConst = vr.mType->Stride();
					}
					else
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid pointer operation");

					cins->mDst.mType = IT_INT16;
					cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
					block->Append(cins);

					vl = CoerceType(proc, exp, block, inlineMapper, vl, TheSignedIntTypeDeclaration);

					InterInstruction* mins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);
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
				vl = Dereference(proc, exp, block, inlineMapper, vl);
				vr = Dereference(proc, exp, block, inlineMapper, vr);

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

				vl = CoerceType(proc, exp, block, inlineMapper, vl, dtype);
				vr = CoerceType(proc, exp, block, inlineMapper, vr, dtype);

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
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = ToValue(proc, exp, block, inlineMapper, vl);
			vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT), * ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);// , * sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);

			ExValue	vdl = Dereference(proc, exp, block, inlineMapper, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mDst.mType = ftype ? IT_FLOAT : IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			cins->mConst.mType = cins->mDst.mType;

			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mConst.mIntConst = exp->mToken == TK_INC ? vdl.mType->Stride() : -(vdl.mType->Stride());
			else if (vdl.mType->IsNumericType())
			{
				cins->mConst.mIntConst = exp->mToken == TK_INC ? 1 : -1;
				cins->mConst.mFloatConst = double(cins->mConst.mIntConst);
			}
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

			StoreValue(proc, exp, block, inlineMapper, vl, ExValue(vl.mType, ains->mDst.mTemp));
#if 0
			sins->mSrc[1].mMemory = IM_INDIRECT;
			sins->mSrc[0].mType = ains->mDst.mType;
			sins->mSrc[0].mTemp = ains->mDst.mTemp;
			sins->mSrc[1].mType = IT_POINTER;
			sins->mSrc[1].mTemp = vl.mTemp;
			sins->mSrc[1].mOperandSize = vl.mType->mSize;
			sins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
			block->Append(sins);
#endif

			// Return reference to value
			return ExValue(vl.mType, vl.mTemp, 1);
		}
		break;

		case EX_POSTINCDEC:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = ToValue(proc, exp, block, inlineMapper, vl);
			vl = Dereference(proc, exp, block, inlineMapper, vl, 1);

			if (vl.mReference != 1)
				mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not a left hand expression");

			if (vl.mType->mFlags & DTF_CONST)
				mErrors->Error(exp->mLocation, EERR_CONST_ASSIGN, "Cannot change const value");

			InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT), * ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_BINARY_OPERATOR);// , * sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);

			ExValue	vdl = Dereference(proc, exp, block, inlineMapper, vl);

			bool	ftype = vl.mType->mType == DT_TYPE_FLOAT;

			cins->mDst.mType = ftype ? IT_FLOAT : IT_INT16;
			cins->mDst.mTemp = proc->AddTemporary(cins->mDst.mType);
			cins->mConst.mType = cins->mDst.mType;

			if (vdl.mType->mType == DT_TYPE_POINTER)
				cins->mConst.mIntConst = exp->mToken == TK_INC ? vdl.mType->Stride() : -(vdl.mType->Stride());
			else if (vdl.mType->IsNumericType())
			{
				cins->mConst.mIntConst = exp->mToken == TK_INC ? 1 : -1;
				cins->mConst.mFloatConst = double(cins->mConst.mIntConst);
			}
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

			StoreValue(proc, exp, block, inlineMapper, vl, ExValue(vl.mType, ains->mDst.mTemp));
#if 0
			sins->mSrc[1].mMemory = IM_INDIRECT;
			sins->mSrc[0].mType = ains->mDst.mType;
			sins->mSrc[0].mTemp = ains->mDst.mTemp;
			sins->mSrc[1].mType = IT_POINTER;
			sins->mSrc[1].mTemp = vl.mTemp;
			sins->mSrc[1].mOperandSize = vl.mType->mSize;
			sins->mVolatile = vl.mType->mFlags & DTF_VOLATILE;
			block->Append(sins);
#endif
			return ExValue(vdl.mType, vdl.mTemp);
		}
		break;

		case EX_PREFIX:
		{	
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = ToValue(proc, exp, block, inlineMapper, vl);

			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_UNARY_OPERATOR);

			switch (exp->mToken)
			{
			case TK_ADD:
				vl = Dereference(proc, exp, block, inlineMapper, vl);
				ins->mOperator = IA_NONE;
				break;
			case TK_SUB:
				vl = Dereference(proc, exp, block, inlineMapper, vl);
				if (!vl.mType->IsNumericType())
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric type");
				else if (vl.mType->mType == DT_TYPE_INTEGER && vl.mType->mSize < 2)
					vl = CoerceType(proc, exp, block, inlineMapper, vl, TheSignedIntTypeDeclaration);
				ins->mOperator = IA_NEG;
				break;
			case TK_BINARY_NOT:
				vl = Dereference(proc, exp, block, inlineMapper, vl);
				if (!(vl.mType->mType == DT_TYPE_POINTER || vl.mType->IsNumericType()))
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a numeric or pointer type");
				else if (vl.mType->mType == DT_TYPE_INTEGER && vl.mType->mSize < 2)
					vl = CoerceType(proc, exp, block, inlineMapper, vl, TheSignedIntTypeDeclaration);
				ins->mOperator = IA_NOT;
				break;
			case TK_MUL:
				if (vl.mType->mType == DT_TYPE_ARRAY)
					vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
				else if (vl.mType->mType != DT_TYPE_POINTER)
					mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a pointer type");
				else if (vl.mType->mStride != 1)
					vl = Dereference(proc, exp, block, inlineMapper, vl, 0);
				else
					return ExValue(vl.mType->mBase, vl.mTemp, vl.mReference + 1);
				return ExValue(vl.mType->mBase, vl.mTemp, 1);
			case TK_BINARY_AND:
			{
				if (vl.mReference < 1 || vl.mBits)
					mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable value");

				Declaration* dec = new Declaration(exp->mLocation, DT_TYPE_POINTER);
				dec->mBase = vl.mType;
				dec->mSize = 2;
				dec->mFlags = DTF_DEFINED;
				return ExValue(exp->mDecType, vl.mTemp, vl.mReference - 1);
			}
			case TK_BANKOF:
			{
				LinkerRegion* rgn;
				if (exp->mLeft->mDecValue->mSection && (rgn = mLinker->FindRegionOfSection(exp->mLeft->mDecValue->mSection)))
				{
					uint64	i = 0;
					while (i < 64 && rgn->mCartridgeBanks != (1ULL << i))
						i++;
					if (i < 64)
					{
						ins->mCode = IC_CONSTANT;
						ins->mNumOperands = 0;
						ins->mConst.mType = IT_INT8;
						ins->mConst.mIntConst = i;
						ins->mDst.mType = IT_INT8;
						ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
						block->Append(ins);
						return ExValue(TheUnsignedCharTypeDeclaration, ins->mDst.mTemp, vl.mReference - 1);
					}
				}
				mErrors->Error(exp->mLocation, ERRR_CANNOT_FIND_BANK_OF_EXPRESSION, "Cannot find bank of expressiohn");
			}	break;

			case TK_NEW:
			{
				ins->mCode = IC_MALLOC;
				ins->mSrc[0].mType = InterTypeOf(vl.mType);
				ins->mSrc[0].mTemp = vl.mTemp;
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mDst.mRestricted = proc->AddRestricted();
				block->Append(ins);
				return ExValue(exp->mDecType, ins->mDst.mTemp, 0);

			} break;

			case TK_DELETE:
			{
				vl = Dereference(proc, exp, block, inlineMapper, vl, 0);

				ins->mCode = IC_FREE;
				ins->mSrc[0].mType = IT_POINTER;
				ins->mSrc[0].mTemp = vl.mTemp;
				block->Append(ins);
				return ExValue(TheConstVoidTypeDeclaration, -1);

			} break;
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
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = ToValue(proc, exp, block, inlineMapper, vl);
			vl = Dereference(proc, exp, block, inlineMapper, vl, vl.mType->mType == DT_TYPE_ARRAY ? 1 : 0);
			vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vr = ToValue(proc, exp, block, inlineMapper, vr);
			vr = Dereference(proc, exp, block, inlineMapper, vr, vr.mType->mType == DT_TYPE_ARRAY ? 1 : 0);


			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);

			Declaration* dtype = TheSignedIntTypeDeclaration;

			if (vl.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_POINTER)
			{
				dtype = vl.mType;
				if (vl.mType->IsIntegerType())
				{
					if ((mCompilerOptions & COPT_CPLUSPLUS) || exp->mLeft->mType != EX_CONSTANT || exp->mLeft->mDecValue->mInteger != 0)
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot compare integer and pointer");
				}
				else if (vr.mType->IsIntegerType())
				{
					if ((mCompilerOptions & COPT_CPLUSPLUS) || exp->mRight->mType != EX_CONSTANT || exp->mRight->mDecValue->mInteger != 0)
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot compare integer and pointer");
				}
				else if ((vl.mType->mType == DT_TYPE_POINTER || vl.mType->mType == DT_TYPE_ARRAY) && (vr.mType->mType == DT_TYPE_POINTER || vr.mType->mType == DT_TYPE_ARRAY))
				{
					if (vl.mType->mBase->mType == DT_TYPE_VOID || vr.mType->mBase->mType == DT_TYPE_VOID)
						;
					else if ((mCompilerOptions & COPT_CPLUSPLUS) && (vl.mType->mBase->IsSubType(vr.mType->mBase) || vr.mType->mBase->IsSubType(vl.mType->mBase)))
						;
					else if (!vl.mType->mBase->IsConstRefSame(vr.mType->mBase))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible pointer types", vl.mType->mBase->MangleIdent(), vr.mType->mBase->MangleIdent());
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

			vl = CoerceType(proc, exp, block, inlineMapper, vl, dtype);
			vr = CoerceType(proc, exp, block, inlineMapper, vr, dtype);

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

		case EX_VCALL:
		case EX_CALL:
		case EX_INLINE:
		{
			if (exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION && (exp->mLeft->mDecValue->mFlags & DTF_INTRINSIC))
			{
				Declaration* decf = exp->mLeft->mDecValue;
				const Ident	*	iname = decf->mQualIdent;

				if (!strcmp(iname->mString, "fabs"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_UNARY_OPERATOR);
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
					vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_UNARY_OPERATOR);
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
					vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, decf->mBase->mParams);

					InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_UNARY_OPERATOR);
					ins->mOperator = IA_CEIL;
					ins->mSrc[0].mType = IT_FLOAT;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mDst.mType = IT_FLOAT;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(ins);

					return ExValue(TheFloatTypeDeclaration, ins->mDst.mTemp);
				}
				else if (!strcmp(iname->mString, "sin"))
				{
				}
				else if (!strcmp(iname->mString, "cos"))
				{
				}
				else if (!strcmp(iname->mString, "tan"))
				{
				}
				else if (!strcmp(iname->mString, "log"))
				{
				}
				else if (!strcmp(iname->mString, "exp"))
				{
				}
				else if (!strcmp(iname->mString, "breakpoint"))
				{
					InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BREAKPOINT);
					ins->mNumOperands = 0;
					block->Append(ins);

					return ExValue(TheVoidTypeDeclaration, 0);
				}
				else if (!strcmp(iname->mString, "malloc"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, decf->mBase->mParams);

					InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_MALLOC);
					ins->mSrc[0].mType = IT_INT16;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mNumOperands = 1;
					ins->mDst.mType = IT_POINTER;
					ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					ins->mDst.mRestricted = proc->AddRestricted();
					block->Append(ins);

					return ExValue(TheVoidPointerTypeDeclaration, ins->mDst.mTemp);
				}
				else if (!strcmp(iname->mString, "free"))
				{
					vr = TranslateExpression(procType, proc, block, exp->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (decf->mBase->mParams->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
					vr = CoerceType(proc, exp, block, inlineMapper, vr, decf->mBase->mParams);

					InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_FREE);
					ins->mSrc[0].mType = IT_POINTER;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mNumOperands = 1;
					block->Append(ins);

					return ExValue(TheVoidTypeDeclaration, 0);
				}
				else if (!strcmp(iname->mString, "strcpy"))
				{
					if (exp->mRight->mType == EX_LIST)
					{
						Expression* tex = exp->mRight->mLeft, * sex = exp->mRight->mRight;
						if ((tex->mDecType->mType == DT_TYPE_ARRAY && tex->mDecType->mSize <= 256) ||
							(sex->mDecType->mType == DT_TYPE_ARRAY && sex->mDecType->mSize <= 256))
						{
							vl = TranslateExpression(procType, proc, block, tex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
							else
								vl = Dereference(proc, exp, block, inlineMapper, vl);

							vr = TranslateExpression(procType, proc, block, sex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vr.mType->mType == DT_TYPE_ARRAY)
								vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
							else
								vr = Dereference(proc, exp, block, inlineMapper, vr);

							if (!TheCharPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							if (!TheConstCharPointerTypeDeclaration->CanAssign(vr.mType))
								mErrors->Error(sex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STRCPY);
							ins->mNumOperands = 2;

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
						if (nex && nex->mType == EX_CONSTANT && nex->mDecValue->mType == DT_CONST_INTEGER && nex->mDecValue->mInteger < 512)
						{
							vl = TranslateExpression(procType, proc, block, tex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
							else
								vl = Dereference(proc, exp, block, inlineMapper, vl);

							vr = TranslateExpression(procType, proc, block, sex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vr.mType->mType == DT_TYPE_ARRAY)
								vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
							else
								vr = Dereference(proc, exp, block, inlineMapper, vr);

							if (!TheVoidPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							if (!TheConstVoidPointerTypeDeclaration->CanAssign(vr.mType))
								mErrors->Error(sex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_COPY);
							ins->mNumOperands = 2;

							ins->mSrc[0].mType = IT_POINTER;
							ins->mSrc[0].mMemory = IM_INDIRECT;
							ins->mSrc[0].mTemp = vr.mTemp;
							ins->mSrc[1].mType = IT_POINTER;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mTemp = vl.mTemp;
							ins->mSrc[0].mOperandSize = int(nex->mDecValue->mInteger);
							ins->mSrc[1].mOperandSize = int(nex->mDecValue->mInteger);
							ins->mConst.mOperandSize = int(nex->mDecValue->mInteger);
							if ((vl.mType->mBase->mFlags & DTF_VOLATILE) || (vr.mType->mBase->mFlags & DTF_VOLATILE))
								ins->mVolatile = true;
							block->Append(ins);

							return vl;
						}
					}
				}
				else if (!strcmp(iname->mString, "memset"))
				{
					if (exp->mRight->mType == EX_LIST)
					{
						Expression* tex = exp->mRight->mLeft, * sex = exp->mRight->mRight->mLeft, * nex = exp->mRight->mRight->mRight;
						if (nex && nex->mType == EX_CONSTANT && nex->mDecValue->mType == DT_CONST_INTEGER && nex->mDecValue->mInteger <= 1024)
						{
							vl = TranslateExpression(procType, proc, block, tex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
							else
								vl = Dereference(proc, exp, block, inlineMapper, vl);

							vr = TranslateExpression(procType, proc, block, sex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							vr = Dereference(proc, exp, block, inlineMapper, vr);

							if (!TheVoidPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");
							if (!TheConstCharTypeDeclaration->CanAssign(vr.mType))
								mErrors->Error(sex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_FILL);
							ins->mNumOperands = 2;

							ins->mSrc[0].mType = IT_INT8;
							ins->mSrc[0].mTemp = vr.mTemp;
							ins->mSrc[1].mType = IT_POINTER;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mTemp = vl.mTemp;
							ins->mSrc[0].mOperandSize = 1;
							ins->mSrc[1].mOperandSize = int(nex->mDecValue->mInteger);
							ins->mConst.mOperandSize = int(nex->mDecValue->mInteger);
							block->Append(ins);

							return vl;
						}
					}
				}
				else if (!strcmp(iname->mString, "memclr"))
				{
					if (exp->mRight->mType == EX_LIST)
					{
						Expression* tex = exp->mRight->mLeft, * nex = exp->mRight->mRight;
						if (nex && nex->mType == EX_CONSTANT && nex->mDecValue->mType == DT_CONST_INTEGER && nex->mDecValue->mInteger <= 1024)
						{
							vl = TranslateExpression(procType, proc, block, tex, destack, gotos, breakBlock, continueBlock, inlineMapper);
							if (vl.mType->mType == DT_TYPE_ARRAY)
								vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
							else
								vl = Dereference(proc, exp, block, inlineMapper, vl);

							if (!TheVoidPointerTypeDeclaration->CanAssign(vl.mType))
								mErrors->Error(tex->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types");

							InterInstruction * zins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
							zins->mDst.mType = IT_INT8;
							zins->mDst.mTemp = proc->AddTemporary(zins->mDst.mType);
							zins->mConst.mType = IT_INT8;
							zins->mConst.mIntConst = 0;
							block->Append(zins);

							InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_FILL);
							ins->mNumOperands = 2;

							ins->mSrc[0].mType = IT_INT8;
							ins->mSrc[0].mTemp = zins->mDst.mTemp;
							ins->mSrc[1].mType = IT_POINTER;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mTemp = vl.mTemp;
							ins->mSrc[0].mOperandSize = 1;
							ins->mSrc[1].mOperandSize = int(nex->mDecValue->mInteger);
							ins->mConst.mOperandSize = int(nex->mDecValue->mInteger);
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

			bool	canInline = exp->mLeft->mType == EX_CONSTANT && 
								exp->mLeft->mDecValue->mType == DT_CONST_FUNCTION && 
								(mCompilerOptions & COPT_OPTIMIZE_INLINE) &&
								!(inlineMapper && inlineMapper->mDepth > 10) &&
								exp->mType != EX_VCALL;
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

				if (exp->mType == EX_INLINE)
					doInline = true;
				else if (inlineConstexpr)
					doInline = true;
				else if (exp->mLeft->mDecValue->mFlags & DTF_INLINE)
				{
					if ((exp->mLeft->mDecValue->mFlags & DTF_REQUEST_INLINE) || (mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE))
					{
						if (proc->mNativeProcedure || !(exp->mLeft->mDecValue->mFlags & DTF_NATIVE))
							doInline = true;
					}
				}
			}

			if (doInline)
			{
				return TranslateInline(procType, proc, block, exp, breakBlock, continueBlock, inlineMapper, inlineConstexpr, lrexp);
			}
			else
			{
				Expression	*	funcexp = exp->mLeft;
			
				vl = TranslateExpression(procType, proc, block, funcexp, destack, gotos, breakBlock, continueBlock, inlineMapper);

				vl = Dereference(proc, exp, block, inlineMapper, vl);

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
					fins = new InterInstruction(MapLocation(exp, inlineMapper), IC_PUSH_FRAME);
					fins->mNumOperands = 0;
					fins->mConst.mIntConst = atotal;
					block->Append(fins);
				}

				Declaration	* decResult = nullptr;
				GrowingArray<InterInstruction*>	defins(nullptr);

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

						InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
						vins->mDst.mType = IT_POINTER;
						vins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						vins->mConst.mType = IT_POINTER;
						vins->mConst.mMemory = IM_LOCAL;
						vins->mConst.mVarIndex = nindex;
						vins->mConst.mOperandSize = ftype->mBase->mSize;
						block->Append(vins);

						ttemp = vins->mDst.mTemp;
					}

					InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					ains->mDst.mType = IT_POINTER;
					ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
					ains->mConst.mType = IT_POINTER;
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

					InterInstruction* wins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
					wins->mSrc[1].mMemory = IM_INDIRECT;
					wins->mSrc[0].mType = IT_POINTER;
					wins->mSrc[0].mTemp = ttemp;
					wins->mSrc[1].mType = IT_POINTER;
					wins->mSrc[1].mTemp = ains->mDst.mTemp;
					wins->mSrc[1].mOperandSize = 2;
					
					if (ftype->mFlags & DTF_FASTCALL)
						defins.Push(wins);
					else
						block->Append(wins);

					atotal = 2;
				}

				if (funcexp->mDecValue && funcexp->mDecValue->mType == DT_CONST_FUNCTION)
					proc->AddCalledFunction(proc->mModule->mProcedures[funcexp->mDecValue->mVarIndex]);
				else
					proc->CallsFunctionPointer();

				Declaration* pdec = ftype->mParams;
				Expression* pex = exp->mRight;
				while (pex)
				{
					InterInstruction	*	ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					ains->mConst.mType = IT_POINTER;

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
//						ains->mConst.mVarIndex += ftype->mFastCallBase;
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

					Declaration* ptype = pdec ? pdec->mBase : texp->mDecType;

					ExValue	vp(ptype ? ptype : TheSignedIntTypeDeclaration, ains->mDst.mTemp, 1);

					if (pdec && pdec->mBase->IsReference() && texp->mType == EX_CALL && !texp->mDecType->IsReference())
					{
						mErrors->Error(texp->mLocation, EERR_MISSING_TEMP, "Missing temporary variable");
#if 0
						int	nindex = proc->mNumLocals++;

						Declaration* vdec = new Declaration(exp->mLocation, DT_VARIABLE);

						vdec->mVarIndex = nindex;
						vdec->mBase = pdec->mBase->mBase;
						vdec->mSize = pdec->mBase->mBase->mSize;

						InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
						vins->mDst.mType = IT_POINTER;
						vins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						vins->mConst.mMemory = IM_LOCAL;
						vins->mConst.mVarIndex = nindex;
						vins->mConst.mOperandSize = vdec->mSize;
						block->Append(vins);

						vp.mType = pdec->mBase->mBase;
						vp.mTemp = vins->mDst.mTemp;
#endif
					}

					if (ptype && (ptype->mType == DT_TYPE_STRUCT || ptype->mType == DT_TYPE_UNION))
						vr = TranslateExpression(procType, proc, block, texp, destack, gotos, breakBlock, continueBlock, inlineMapper, &vp);
					else
						vr = TranslateExpression(procType, proc, block, texp, destack, gotos, breakBlock, continueBlock, inlineMapper, nullptr);

					if (!(pdec && pdec->mBase->IsReference()) && (vr.mType->mType == DT_TYPE_STRUCT || vr.mType->mType == DT_TYPE_UNION))
					{
						if (pdec && !pdec->mBase->CanAssign(vr.mType))
							mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types", pdec->mBase->MangleIdent(), vr.mType->MangleIdent());

						vr = Dereference(proc, texp, block, inlineMapper, vr, 1);

						if (vr.mReference != 1)
							mErrors->Error(exp->mLeft->mLocation, EERR_NOT_AN_LVALUE, "Not an addressable expression");

						if (vp.mTemp != vr.mTemp)
						{
							CopyStruct(proc, exp, block, vp, vr, inlineMapper, false);
#if 0
							InterInstruction* cins = new InterInstruction(texp->mLocation, IC_COPY);
							cins->mSrc[0].mType = IT_POINTER;
							cins->mSrc[0].mTemp = vr.mTemp;
							cins->mSrc[0].mMemory = IM_INDIRECT;
							cins->mSrc[0].mOperandSize = vp.mType->mSize;
							cins->mSrc[0].mStride = vr.mType->mStripe;

							cins->mSrc[1].mOperandSize = vp.mType->mSize;
							cins->mSrc[1].mType = IT_POINTER;
							cins->mSrc[1].mTemp = ains->mDst.mTemp;
							cins->mSrc[1].mMemory = IM_INDIRECT;

							cins->mConst.mOperandSize = vp.mType->mSize;
							block->Append(cins);
#endif
						}

						atotal += vr.mType->mSize;
					}
					else
					{
						if (vr.mType->mType == DT_TYPE_ARRAY)// || vr.mType->mType == DT_TYPE_FUNCTION)
							vr = Dereference(proc, texp, block, inlineMapper, vr, 1);
						else if (pdec && pdec->mBase->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_INTEGER && texp->mType == EX_CONSTANT && texp->mDecValue->mType == DT_CONST_INTEGER && texp->mDecValue->mInteger == 0)
						{
							mErrors->Error(texp->mLocation, EWARN_NUMERIC_0_USED_AS_NULLPTR, "Numeric 0 used for nullptr");
							vr = CoerceType(proc, texp, block, inlineMapper, vr, pdec->mBase);
						}
						else if (pdec && (pdec->mBase->IsReference() && !vr.mType->IsReference()))
							vr = Dereference(proc, texp, block, inlineMapper, vr, 1);
						else if (vr.mType->IsReference() && !(pdec && pdec->mBase->IsReference()))
						{
							vr.mReference++;
							vr.mType = vr.mType->mBase;
							vr = Dereference(proc, texp, block, inlineMapper, vr);
						}
						else
							vr = Dereference(proc, texp, block, inlineMapper, vr);

						if (pdec)
						{
							if (!(pdec->mFlags & DTF_FPARAM_UNUSED) && !pdec->mBase->CanAssign(vr.mType))
							{
								pdec->mBase->CanAssign(vr.mType);
								mErrors->Error(texp->mLocation, EERR_INCOMPATIBLE_TYPES, "Cannot assign incompatible types", pdec->mBase->MangleIdent(), vr.mType->MangleIdent());
							}
							vr = CoerceType(proc, texp, block, inlineMapper, vr, pdec->mBase);
						}
						else if (vr.mType->IsIntegerType() && vr.mType->mSize < 2)
						{
							vr = CoerceType(proc, texp, block, inlineMapper, vr, TheSignedIntTypeDeclaration);
						}


						InterInstruction* wins = new InterInstruction(texp->mLocation, IC_STORE);
						wins->mSrc[1].mMemory = IM_INDIRECT;
						wins->mSrc[0].mType = InterTypeOf(vr.mType);;
						wins->mSrc[0].mTemp = vr.mTemp;
						wins->mSrc[1].mType = IT_POINTER;
						wins->mSrc[1].mTemp = ains->mDst.mTemp;
						if (pdec)
						{
							if (pdec->mBase->mType == DT_TYPE_ARRAY || pdec->mBase->mType == DT_TYPE_REFERENCE || pdec->mBase->mType == DT_TYPE_RVALUEREF || pdec->mBase->mType == DT_TYPE_FUNCTION)
							{
								wins->mSrc[1].mOperandSize = 2;
								wins->mSrc[0].mType = IT_POINTER;
							}
							else
								wins->mSrc[1].mOperandSize = pdec->mSize;
						}
						else if (vr.mType->mSize > 2 && vr.mType->mType != DT_TYPE_ARRAY)
							wins->mSrc[1].mOperandSize = vr.mType->mSize;
						else
							wins->mSrc[1].mOperandSize = 2;

						if (!pdec || !(pdec->mFlags & DTF_FPARAM_UNUSED))
						{
							if (ftype->mFlags & DTF_FASTCALL)
								defins.Push(wins);
							else
								block->Append(wins);
						}

						atotal += wins->mSrc[1].mOperandSize;
					}

					if (pdec)
						pdec = pdec->mNext;
				}

				if (pdec)
					mErrors->Error(exp->mLocation, EERR_WRONG_PARAMETER, "Not enough arguments for function call");

				for (int i = 0; i < defins.Size(); i++)
					block->Append(defins[i]);

				InterInstruction	*	cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CALL);
				cins->mNumOperands = 1;
				if (funcexp->mDecValue && (funcexp->mDecValue->mFlags & DTF_NATIVE) || (mCompilerOptions & COPT_NATIVE))
					cins->mCode = IC_CALL_NATIVE;
				else
					cins->mCode = IC_CALL;

				if (funcexp->mDecValue && (funcexp->mDecValue->mFlags & DTF_FUNC_PURE))
					cins->mNoSideEffects = true;

				if (funcexp->mType == EX_CONSTANT && (funcexp->mDecValue->mFlags & DTF_FUNC_CONSTEXPR) && funcexp->mDecType->mBase->mType != DT_TYPE_STRUCT)
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

					InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_POP_FRAME);
					xins->mNumOperands = 0;
					xins->mConst.mIntConst = atotal;
					block->Append(xins);
				}

				if (decResult)
				{
					if (lrexp)
						return *lrexp;

					InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					vins->mDst.mType = IT_POINTER;
					vins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
					vins->mConst.mType = IT_POINTER;
					vins->mConst.mMemory = IM_LOCAL;
					vins->mConst.mVarIndex = decResult->mVarIndex;
					block->Append(vins);

					return ExValue(ftype->mBase, vins->mDst.mTemp, 1);
				}
				else if (ftype->mBase->IsReference())
					return ExValue(ftype->mBase->mBase, cins->mDst.mTemp, 1);
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

				InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
				ins->mDst.mType = IT_POINTER;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				ins->mConst.mType = IT_POINTER;
				ins->mConst.mOperandSize = dec->mSize;
				ins->mConst.mIntConst = 0;
				ins->mConst.mMemory = IM_GLOBAL;
				ins->mConst.mLinkerObject = dec->mLinkerObject;
				ins->mConst.mVarIndex = dec->mVarIndex;
				block->Append(ins);

				InterInstruction	*	jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_ASSEMBLER);
				jins->mDst.mTemp = proc->AddTemporary(IT_INT32);
				jins->mDst.mType = IT_INT32;

				jins->mSrc[0].mType = IT_POINTER;
				jins->mSrc[0].mTemp = ins->mDst.mTemp;
				jins->mNumOperands = 1;

				for (int i = 0; i < refvars.Size(); i++)
				{
					InterInstruction* vins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
					vins->mConst.mType = IT_POINTER;

					vins->mDst.mType = IT_POINTER;
					vins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);

					Declaration* vdec = refvars[i];
					if (vdec->mType == DT_ARGUMENT)
					{
						vins->mConst.mVarIndex = vdec->mVarIndex;
						if (vdec->mFlags & DTF_FPARAM_UNUSED)
						{
							vins->mConst.mMemory = IM_LOCAL;
							if (inlineMapper)
								vins->mConst.mVarIndex += inlineMapper->mVarIndex;
						}
						else if (inlineMapper)
						{
							vins->mConst.mMemory = IM_LOCAL;
							vins->mConst.mVarIndex = inlineMapper->mParams[vdec->mVarIndex];
						}
						else if (procType->mFlags & DTF_FASTCALL)
						{
							vins->mConst.mMemory = IM_FPARAM;
//							vins->mConst.mVarIndex += procType->mFastCallBase;
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
						if (inlineMapper)
							vins->mConst.mVarIndex += inlineMapper->mVarIndex;
					}

					block->Append(vins);

					InterInstruction* lins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD);
					lins->mSrc[0].mMemory = IM_INDIRECT;
					lins->mSrc[0].mType = IT_POINTER;
					lins->mSrc[0].mTemp = vins->mDst.mTemp;
					lins->mDst.mType = InterTypeOf(vdec->mBase);
					lins->mDst.mTemp = proc->AddTemporary(lins->mDst.mType);
					lins->mSrc[0].mOperandSize = vdec->mSize;
					block->Append(lins);

					if (jins->mNumOperands >= 12)
						mErrors->Error(exp->mLocation, EERR_ASSEMBLER_LIMIT, "Maximum number of variables in assembler block exceeded", vdec->mIdent);
					else
					{
						jins->mSrc[jins->mNumOperands].mType = InterTypeOf(vdec->mBase);
						jins->mSrc[jins->mNumOperands].mTemp = lins->mDst.mTemp;
						jins->mNumOperands++;
					}
				}

				block->Append(jins);

				return ExValue(exp->mDecType, jins->mDst.mTemp);
			}

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_RETURN:
		{
			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RETURN);
			if (exp->mLeft)
			{
				if (procType->mBase->IsReference())
				{
					vr = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

					if (vr.mType->IsReference())
					{
						vr.mReference++;
						vr.mType = vr.mType->mBase;
					}

					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);

					vr = CoerceType(proc, exp, block, inlineMapper, vr, procType->mBase);

					if (vr.mReference == 0)
						mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Returning value as reference");

					ins->mSrc[0].mType = IT_POINTER;
					ins->mSrc[0].mTemp = vr.mTemp;
					ins->mSrc[0].mMemory = IM_INDIRECT;

					if (inlineMapper)
					{
						if (inlineMapper->mResultExp)
						{
							if (inlineMapper->mResultExp->mType->IsReference())
							{
								ins->mSrc[1].mType = IT_POINTER;
								ins->mSrc[1].mTemp = inlineMapper->mResultExp->mTemp;
								ins->mSrc[1].mMemory = IM_INDIRECT;
								ins->mCode = IC_STORE;
								ins->mSrc[1].mOperandSize = 2;
								ins->mNumOperands = 2;
							}
							else
							{
								//bool moving = exp->mLeft->IsRValue() || exp->mLeft->mType == EX_VARIABLE && !(exp->mLeft->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)) && exp->mLeft->mDecType->mType != DT_TYPE_REFERENCE;

								CopyStruct(proc, exp, block, *(inlineMapper->mResultExp), vr, inlineMapper, false);
								ins->mCode = IC_NONE;
							}
						}
						else
						{
							InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
							ains->mDst.mType = IT_POINTER;
							ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
							ains->mConst.mType = IT_POINTER;
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
							ins->mNumOperands = 2;
						}
					}
					else
					{
						ins->mCode = IC_RETURN_VALUE;
						ins->mNumOperands = 1;
					}
				}
				else if (procType->mBase->mType == DT_TYPE_STRUCT)
				{

					InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);

					if (inlineMapper)
					{
						if (inlineMapper->mResultExp)
						{
							ains->mDst.mTemp = inlineMapper->mResultExp->mTemp;
						}
						else
						{
							ains->mCode = IC_CONSTANT;
							ains->mDst.mType = IT_POINTER;
							ains->mDst.mTemp = proc->AddTemporary(IT_POINTER);
							ains->mConst.mType = IT_POINTER;
							ains->mConst.mOperandSize = procType->mBase->mSize;
							ains->mConst.mIntConst = 0;
							ains->mConst.mVarIndex = inlineMapper->mResult;
							ains->mConst.mMemory = IM_LOCAL;
							block->Append(ains);
						}

						ins->mCode = IC_NONE;
					}
					else
					{
						InterInstruction* pins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
						pins->mDst.mType = IT_POINTER;
						pins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
						pins->mConst.mType = IT_POINTER;
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
						ains->mNumOperands = 1;
						block->Append(ains);

						ins->mCode = IC_RETURN;
					}

					ExValue	rvr(procType->mBase, ains->mDst.mTemp, 1);

					vr = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper, &rvr);

					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);

					if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has void return type");
					else if (!procType->mBase->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Cannot return incompatible type");
					else if (vr.mReference != 1)
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Non addressable object");

					bool moving = exp->mLeft->IsRValue() || exp->mLeft->mType == EX_VARIABLE && !(exp->mLeft->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)) && exp->mLeft->mDecType->mType != DT_TYPE_REFERENCE;

					CopyStruct(proc, exp, block, rvr, vr, inlineMapper, moving);
#if 0
					if (procType->mBase->mCopyConstructor)
					{
						Declaration* ccdec = procType->mBase->mCopyConstructor;
						if (ccdec->mBase->mFlags & DTF_FASTCALL)
						{
							if (!ccdec->mLinkerObject)
								this->TranslateProcedure(proc->mModule, ccdec->mValue, ccdec);

							InterInstruction* psins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
							psins->mDst.mType = IT_POINTER;
							psins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
							psins->mConst.mVarIndex = 0;
							psins->mConst.mIntConst = 0;
							psins->mConst.mOperandSize = 2;
							if (procType->mFlags & DTF_FASTCALL)
							{
								psins->mConst.mMemory = IM_FPARAM;
								psins->mConst.mVarIndex += ccdec->mBase->mFastCallBase;
							}
							else
								psins->mConst.mMemory = IM_PARAM;
							block->Append(psins);

							InterInstruction* ssins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
							ssins->mSrc[0] = ains->mDst;
							ssins->mSrc[1] = psins->mDst;
							block->Append(ssins);

							InterInstruction* plins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
							plins->mDst.mType = IT_POINTER;
							plins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
							plins->mConst.mVarIndex = 2;
							plins->mConst.mIntConst = 0;
							plins->mConst.mOperandSize = 2;
							if (procType->mFlags & DTF_FASTCALL)
							{
								plins->mConst.mMemory = IM_FPARAM;
								plins->mConst.mVarIndex += ccdec->mBase->mFastCallBase;
							}
							else
								plins->mConst.mMemory = IM_PARAM;
							block->Append(plins);

							InterInstruction* slins = new InterInstruction(MapLocation(exp, inlineMapper), IC_STORE);
							slins->mSrc[0].mType = IT_POINTER;
							slins->mSrc[0].mTemp = vr.mTemp;
							slins->mSrc[0].mMemory = IM_INDIRECT;
							slins->mSrc[0].mOperandSize = procType->mBase->mSize;
							slins->mSrc[0].mStride = vr.mType->mStripe;
							slins->mSrc[1] = plins->mDst;
							block->Append(slins);

							proc->AddCalledFunction(proc->mModule->mProcedures[ccdec->mVarIndex]);

							InterInstruction* pcins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
							pcins->mDst.mType = IT_POINTER;
							pcins->mDst.mTemp = proc->AddTemporary(IT_POINTER);
							pcins->mConst.mType = IT_POINTER;
							pcins->mConst.mVarIndex = ccdec->mVarIndex;
							pcins->mConst.mIntConst = 0;
							pcins->mConst.mOperandSize = 2;
							pcins->mConst.mMemory = IM_GLOBAL;
							pcins->mConst.mLinkerObject = ccdec->mLinkerObject;
							block->Append(pcins);

							InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CALL);
							if (ccdec->mFlags & DTF_NATIVE)
								cins->mCode = IC_CALL_NATIVE;
							else
								cins->mCode = IC_CALL;
							cins->mSrc[0] = pcins->mDst;
							cins->mNumOperands = 1;

							block->Append(cins);
						}
					}
					else
					{
						InterInstruction* cins = new InterInstruction(MapLocation(exp, inlineMapper), IC_COPY);
						cins->mSrc[0].mType = IT_POINTER;
						cins->mSrc[0].mTemp = vr.mTemp;
						cins->mSrc[0].mMemory = IM_INDIRECT;
						cins->mSrc[0].mOperandSize = procType->mBase->mSize;
						cins->mSrc[0].mStride = vr.mType->mStripe;

						cins->mSrc[1].mOperandSize = procType->mBase->mSize;
						cins->mSrc[1].mType = IT_POINTER;
						cins->mSrc[1].mTemp = ains->mDst.mTemp;
						cins->mSrc[1].mMemory = IM_INDIRECT;

						cins->mConst.mOperandSize = procType->mBase->mSize;
						block->Append(cins);
					}
#endif
				}
				else
				{
					vr = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

					if (procType->mBase->mType == DT_TYPE_POINTER && (vr.mType->mType == DT_TYPE_ARRAY || vr.mType->mType == DT_TYPE_FUNCTION))
					{
						vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
						vr.mReference = 0;
						vr.mType = procType->mBase;
					}
					else
						vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (!procType->mBase || procType->mBase->mType == DT_TYPE_VOID)
					{
						if (vr.mType->mType != DT_TYPE_VOID)
							mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Function has void return type");
					}
					else if (procType->mBase->mType == DT_TYPE_BOOL && (vr.mType->IsIntegerType() || vr.mType->mType == DT_TYPE_POINTER))
						;
					else if (procType->mBase->mType == DT_TYPE_POINTER && exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecValue->mType == DT_CONST_INTEGER && exp->mLeft->mDecValue->mInteger == 0)
						mErrors->Error(exp->mLocation, EWARN_NUMERIC_0_USED_AS_NULLPTR, "Numeric 0 used for nullptr");
					else if (!procType->mBase->CanAssign(vr.mType))
						mErrors->Error(exp->mLocation, EERR_INVALID_RETURN, "Cannot return incompatible type");

					if (vr.mType->mType == DT_TYPE_VOID)
					{
						if (inlineMapper)
							ins->mCode = IC_NONE;
						else
							ins->mCode = IC_RETURN;
					}
					else
					{
						vr = CoerceType(proc, exp, block, inlineMapper, vr, procType->mBase);

						ins->mSrc[0].mType = InterTypeOf(vr.mType);
						ins->mSrc[0].mTemp = vr.mTemp;

						if (inlineMapper)
						{
							if (inlineMapper->mResultExp)
							{
								ins->mSrc[1].mType = IT_POINTER;
								ins->mSrc[1].mTemp = inlineMapper->mResultExp->mTemp;
								ins->mSrc[1].mMemory = IM_INDIRECT;
								ins->mCode = IC_STORE;
								ins->mSrc[1].mOperandSize = procType->mBase->mSize;
								ins->mNumOperands = 2;
							}
							else
							{
								InterInstruction* ains = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
								ains->mDst.mType = IT_POINTER;
								ains->mDst.mTemp = proc->AddTemporary(ains->mDst.mType);
								ains->mConst.mType = IT_POINTER;
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
								ins->mNumOperands = 2;
							}
						}
						else
						{
							ins->mCode = IC_RETURN_VALUE;
							ins->mNumOperands = 1;
						}
					}
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

			UnwindDestructStack(procType, proc, block, destack, nullptr, inlineMapper);

			if (ins->mCode != IC_NONE)
				block->Append(ins);

			if (inlineMapper)
			{
				InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
				block->Append(jins);
				block->Close(inlineMapper->mReturn, nullptr);
			}
			else
				block->Close(nullptr, nullptr);
			block = new InterCodeBasicBlock(proc);

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_BREAK:
		{
			if (breakBlock.mBlock)
			{
				UnwindDestructStack(procType, proc, block, destack, breakBlock.mStack, inlineMapper);
				InterInstruction	*	jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
				block->Append(jins);

				block->Close(breakBlock.mBlock, nullptr);
				block = new InterCodeBasicBlock(proc);
			}
			else
				mErrors->Error(exp->mLocation, EERR_INVALID_BREAK, "No break target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_CONTINUE:
		{
			if (continueBlock.mBlock)
			{
				UnwindDestructStack(procType, proc, block, destack, continueBlock.mStack, inlineMapper);
				InterInstruction	*	jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
				block->Append(jins);

				block->Close(continueBlock.mBlock, nullptr);
				block = new InterCodeBasicBlock(proc);
			}
			else
				mErrors->Error(exp->mLocation, EERR_INVALID_CONTINUE, "No continue target");

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_ASSUME:
		{
#if 1
			InterCodeBasicBlock* tblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock(proc);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, destack, gotos, inlineMapper);

			InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_UNREACHABLE);
			ins->mNumOperands = 0;
			fblock->Append(ins);
			fblock->Close(nullptr, nullptr);

			block = tblock;
#endif
			return ExValue(TheVoidTypeDeclaration);
		}
		case EX_LOGICAL_NOT:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, inlineMapper, vl);

			InterInstruction	*	zins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			zins->mDst.mType = InterTypeOf(vl.mType);
			zins->mDst.mTemp = proc->AddTemporary(zins->mDst.mType);
			zins->mConst.mType = zins->mDst.mType;
			zins->mConst.mIntConst = 0;
			block->Append(zins);

			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_RELATIONAL_OPERATOR);
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
				ExValue	vc = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

				vl = TranslateExpression(procType, proc, block, exp->mRight->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, block, exp->mRight->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

				vc = Dereference(proc, exp, block, inlineMapper, vc);

				int			ttemp, tref = 0;
				InterType	ttype, stypel, styper;

				stypel = InterTypeOf(vl.mType);
				styper = InterTypeOf(vr.mType);

				Declaration* dtype = exp->mDecType;

				if (dtype->IsReference())
				{
					vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
					tref = 1;
					
					dtype = dtype->mBase;
					ttype = IT_POINTER;
				}
				else if (stypel == IT_POINTER || styper == IT_POINTER)
				{
					if (vl.mType->mType == DT_TYPE_ARRAY)
						vl = Dereference(proc, exp, block, inlineMapper, vl, 1);
					else
						vl = Dereference(proc, exp, block, inlineMapper, vl);

					if (vr.mType->mType == DT_TYPE_ARRAY)
						vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
					else
						vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (vl.mType->mBase && vr.mType->mBase)
					{
						if (vl.mType->mBase->IsSubType(vr.mType->mBase))
							dtype = vr.mType;
						else if (vr.mType->mBase->IsSubType(vl.mType->mBase))
							dtype = vl.mType;
						else
						{
							mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible conditional types");
							dtype = vl.mType;
						}
					}
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
					vl = Dereference(proc, exp, block, inlineMapper, vl);
					vr = Dereference(proc, exp, block, inlineMapper, vr);

					if (stypel == styper)
					{
						ttype = stypel;
						dtype = vl.mType;
					}
					else if (stypel > styper)
					{
						ttype = stypel;
						dtype = vl.mType;

						vr = CoerceType(proc, exp, block, inlineMapper, vr, dtype);
					}
					else
					{
						ttype = styper;
						dtype = vr.mType;

						vl = CoerceType(proc, exp, block, inlineMapper, vl, dtype);
					}
				}

				vc = CoerceType(proc, exp, block, inlineMapper, vc, TheBoolTypeDeclaration);

				ttemp = proc->AddTemporary(ttype);

				InterInstruction* sins = new InterInstruction(MapLocation(exp, inlineMapper), IC_SELECT);
				sins->mSrc[2].mType = InterTypeOf(vc.mType);
				sins->mSrc[2].mTemp = vc.mTemp;
				sins->mSrc[1].mType = ttype;
				sins->mSrc[1].mTemp = vl.mTemp;
				sins->mSrc[0].mType = ttype;
				sins->mSrc[0].mTemp = vr.mTemp;
				sins->mDst.mType = ttype;
				sins->mDst.mTemp = ttemp;
				block->Append(sins);

				return ExValue(dtype, ttemp, tref);
			}
			else
#endif
			{
				InterInstruction* jins0 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
				InterInstruction* jins1 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

				InterCodeBasicBlock* tblock = new InterCodeBasicBlock(proc);
				InterCodeBasicBlock* fblock = new InterCodeBasicBlock(proc);
				InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);

				TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, destack, gotos, inlineMapper);

				vl = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

				int			ttemp;
				InterType	ttype, stypel, styper;

				stypel = InterTypeOf(vl.mType);
				styper = InterTypeOf(vr.mType);

				Declaration* dtype;
				if (stypel == IT_POINTER || styper == IT_POINTER)
				{
					if (vl.mType->mType == DT_TYPE_ARRAY)
						vl = Dereference(proc, exp, tblock, inlineMapper, vl, 1);
					else
						vl = Dereference(proc, exp, tblock, inlineMapper, vl);

					if (vr.mType->mType == DT_TYPE_ARRAY)
						vr = Dereference(proc, exp, fblock, inlineMapper, vr, 1);
					else
						vr = Dereference(proc, exp, fblock, inlineMapper, vr);

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
					vl = Dereference(proc, exp, tblock, inlineMapper, vl);
					vr = Dereference(proc, exp, fblock, inlineMapper, vr);

					if (stypel == styper)
					{
						ttype = stypel;
						dtype = vl.mType;
					}
					else if (stypel > styper)
					{
						ttype = stypel;
						dtype = vl.mType;

						vr = CoerceType(proc, exp, fblock, inlineMapper, vr, dtype);
					}
					else
					{
						ttype = styper;
						dtype = vr.mType;

						vl = CoerceType(proc, exp, tblock, inlineMapper, vl, dtype);
					}
				}

				if (ttype != IT_NONE)
				{
					ttemp = proc->AddTemporary(ttype);

					InterInstruction* rins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD_TEMPORARY);
					rins->mSrc[0].mType = ttype;
					rins->mSrc[0].mTemp = vr.mTemp;
					rins->mDst.mType = ttype;
					rins->mDst.mTemp = ttemp;
					fblock->Append(rins);

					InterInstruction* lins = new InterInstruction(MapLocation(exp, inlineMapper), IC_LOAD_TEMPORARY);
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
				else
				{
					tblock->Append(jins0);
					tblock->Close(eblock, nullptr);

					fblock->Append(jins1);
					fblock->Close(eblock, nullptr);

					block = eblock;

					return ExValue();
				}
			}

			break;
		}

		case EX_TYPECAST:
		{
			vr = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

			InterInstruction	*	ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);

			if (exp->mDecType->mType == DT_TYPE_FLOAT && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, exp, block, inlineMapper, vr);

				int stemp = vr.mTemp;

				if (vr.mType->mSize == 1)
				{
					if (vr.mType->mFlags & DTF_SIGNED)
					{
						InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
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
						InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONVERSION_OPERATOR);
						xins->mOperator = IA_EXT8TO16U;
						xins->mSrc[0].mType = IT_INT8;
						xins->mSrc[0].mTemp = stemp;
						xins->mDst.mType = IT_INT16;
						xins->mDst.mTemp = proc->AddTemporary(IT_INT16);
						block->Append(xins);
						stemp = xins->mDst.mTemp;
					}
				}

				if (vr.mType->mSize <= 2)
				{
					ins->mOperator = (vr.mType->mFlags & DTF_SIGNED) ? IA_INT2FLOAT : IA_UINT2FLOAT;
					ins->mSrc[0].mType = IT_INT16;
				}
				else
				{
					ins->mOperator = (vr.mType->mFlags & DTF_SIGNED) ? IA_LINT2FLOAT : IA_LUINT2FLOAT;
					ins->mSrc[0].mType = IT_INT32;
				}

				ins->mSrc[0].mTemp = stemp;
				ins->mDst.mType = InterTypeOf(exp->mDecType);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);
			}
			else if (exp->mDecType->IsIntegerType() && vr.mType->mType == DT_TYPE_FLOAT)
			{
				vr = Dereference(proc, exp, block, inlineMapper, vr);
				if (exp->mDecType->mSize <= 2)
				{
					ins->mOperator = (exp->mDecType->mFlags & DTF_SIGNED) ? IA_FLOAT2INT : IA_FLOAT2UINT;
					ins->mDst.mType = IT_INT16;
				}
				else
				{
					ins->mOperator = (exp->mDecType->mFlags & DTF_SIGNED) ? IA_FLOAT2LINT : IA_FLOAT2LUINT;
					ins->mDst.mType = IT_INT32;
				}

				ins->mSrc[0].mType = InterTypeOf(vr.mType);
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);

				if (exp->mDecType->mSize == 1)
				{
					InterInstruction* xins = new InterInstruction(MapLocation(exp, inlineMapper), IC_TYPECAST);
					xins->mSrc[0].mType = IT_INT16;
					xins->mSrc[0].mTemp = ins->mDst.mTemp;
					xins->mDst.mType = InterTypeOf(exp->mDecType);
					xins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
					block->Append(xins);
					ins = xins;
				}
			}
			else if (exp->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_POINTER)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mDecType, vr.mTemp, vr.mReference);
			}
			else if (exp->mDecType->mType == DT_TYPE_POINTER && vr.mType->mType == DT_TYPE_ARRAY)
			{
				// no need for actual operation when casting pointer to pointer
				return ExValue(exp->mDecType, vr.mTemp, vr.mReference - 1);
			}
			else if (exp->mDecType->mType != DT_TYPE_VOID && vr.mType->mType == DT_TYPE_VOID)
			{
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Cannot cast void object to non void object");
				return ExValue(exp->mDecType, vr.mTemp, vr.mReference);
			}
			else if (exp->mDecType->IsIntegerType() && vr.mType->IsIntegerType())
			{
				vr = Dereference(proc, exp, block, inlineMapper, vr);
				return CoerceType(proc, exp, block, inlineMapper, vr, exp->mDecType);
			}
			else if (exp->mDecType->mType == DT_TYPE_VOID)
			{
			}
			else if (exp->mDecType->IsReference() && exp->mDecType->mBase->IsConstSame(vr.mType))
			{
				if (vr.mReference == 0)
					mErrors->Error(exp->mLocation, EERR_NOT_AN_LVALUE, "Not an L value");

				vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
				return ExValue(exp->mDecType, vr.mTemp);
			}
			else
			{
				if (vr.mType->mType == DT_TYPE_ARRAY)
					vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
				else
					vr = Dereference(proc, exp, block, inlineMapper, vr);
				ins->mCode = IC_TYPECAST;
				ins->mSrc[0].mType = vr.mReference > 0 ? IT_POINTER : InterTypeOf(vr.mType);
				ins->mSrc[0].mTemp = vr.mTemp;
				ins->mDst.mType = InterTypeOf(exp->mDecType);
				ins->mDst.mTemp = proc->AddTemporary(ins->mDst.mType);
				block->Append(ins);
			}

			return ExValue(exp->mDecType, ins->mDst.mTemp);
		}
			break;

		case EX_LOGICAL_AND:
		case EX_LOGICAL_OR:
		{
			InterInstruction* jins0 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);
			
			TranslateLogic(procType, proc, block, tblock, fblock, exp, destack, gotos, inlineMapper);

			int	ttemp = proc->AddTemporary(IT_BOOL);

			InterInstruction* tins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			tins->mConst.mType = IT_BOOL;
			tins->mConst.mIntConst = 1;
			tins->mDst.mType = IT_BOOL;
			tins->mDst.mTemp = ttemp;
			tblock->Append(tins);

			InterInstruction* fins = new InterInstruction(MapLocation(exp, inlineMapper), IC_CONSTANT);
			fins->mConst.mType = IT_BOOL;
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

		case EX_SCOPE:
		{
			DestructStack* odestack = destack;

			TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);

			UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);
			destack = odestack;

			return ExValue(TheVoidTypeDeclaration);

		} break;

		case EX_DISPATCH:
		{
			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, inlineMapper, vl);

			InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_DISPATCH);
			ins->mSrc[0].mType = IT_POINTER;
			ins->mSrc[0].mTemp = vl.mTemp;

			block->Append(ins);

			Declaration* dinit = exp->mLeft->mLeft->mDecValue->mValue->mDecValue->mParams;
			while (dinit)
			{
				proc->mModule->mProcedures[dinit->mValue->mDecValue->mVarIndex]->mDispatchedCall = true;
				proc->AddCalledFunction(proc->mModule->mProcedures[dinit->mValue->mDecValue->mVarIndex]);
				dinit = dinit->mNext;
			}


//			proc->CallsFunctionPointer();

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_WHILE:
		{
			DestructStack* odestack = destack;

			InterInstruction	*	jins0 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* lblock = cblock;
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);
			
			block->Append(jins0);
			block->Close(cblock, nullptr);

			TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft, destack, gotos, inlineMapper);

			DestructStack* idestack = destack;

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, destack, gotos, BranchTarget(eblock, odestack), BranchTarget(lblock, idestack), inlineMapper);

			UnwindDestructStack(procType, proc, bblock, destack, idestack, inlineMapper);
			destack = idestack;

			bblock->Append(jins1);
			bblock->Close(lblock, nullptr);

			block = eblock;

			UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);
			destack = odestack;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_IF:
		{
			DestructStack* odestack = destack;

			InterInstruction	*	jins0 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);

			TranslateLogic(procType, proc, block, tblock, fblock, exp->mLeft, destack, gotos, inlineMapper);

			DestructStack* itdestack = destack;
			vr = TranslateExpression(procType, proc, tblock, exp->mRight->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			UnwindDestructStack(procType, proc, tblock, destack, itdestack, inlineMapper);
			destack = itdestack;

			tblock->Append(jins0);
			tblock->Close(eblock, nullptr);

			if (exp->mRight->mRight)
			{
				DestructStack* ifdestack = destack;
				vr = TranslateExpression(procType, proc, fblock, exp->mRight->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);
				UnwindDestructStack(procType, proc, fblock, destack, ifdestack, inlineMapper);
				destack = ifdestack;
			}
			fblock->Append(jins1);
			fblock->Close(eblock, nullptr);

			block = eblock;

			UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);
			destack = odestack;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_FOR:
		{
			DestructStack* odestack = destack;

			// assignment
			if (exp->mLeft->mRight)
				TranslateExpression(procType, proc, block, exp->mLeft->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

			InterInstruction* jins0 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins1 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins2 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			InterInstruction* jins3 = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* lblock = cblock;
			InterCodeBasicBlock* bblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* iblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);

			block->Append(jins0);
			block->Close(cblock, nullptr);

			// condition
			if (exp->mLeft->mLeft->mLeft)
				TranslateLogic(procType, proc, cblock, bblock, eblock, exp->mLeft->mLeft->mLeft, destack, gotos, inlineMapper);
			else
			{
				cblock->Append(jins1);
				cblock->Close(bblock, nullptr);
			}

			// Body

			DestructStack* idestack = destack;

			vr = TranslateExpression(procType, proc, bblock, exp->mRight, destack, gotos, BranchTarget(eblock, odestack), BranchTarget(iblock, idestack), inlineMapper);

			UnwindDestructStack(procType, proc, bblock, destack, idestack, inlineMapper);
			destack = idestack;

			bblock->Append(jins2);
			bblock->Close(iblock, nullptr);

			// increment
			if (exp->mLeft->mLeft->mRight)
				TranslateExpression(procType, proc, iblock, exp->mLeft->mLeft->mRight, destack, gotos, breakBlock, continueBlock, inlineMapper);

			iblock->Append(jins3);
			iblock->Close(lblock, nullptr);

			block = eblock;

			UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);
			destack = odestack;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_DO:
		{
			DestructStack* odestack = destack;

			InterInstruction	*	jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

			InterCodeBasicBlock* cblock = new InterCodeBasicBlock(proc);
			InterCodeBasicBlock* lblock = cblock;
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);

			block->Append(jins);
			block->Close(cblock, nullptr);

			DestructStack* idestack = destack;

			vr = TranslateExpression(procType, proc, cblock, exp->mRight, destack, gotos, BranchTarget(eblock, odestack), BranchTarget(cblock, idestack), inlineMapper);

			UnwindDestructStack(procType, proc, cblock, destack, idestack, inlineMapper);
			destack = idestack;

			TranslateLogic(procType, proc, cblock, lblock, eblock, exp->mLeft, destack, gotos, inlineMapper);

			block = eblock;

			UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);
			destack = odestack;

			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_LABEL:
		{
			GotoNode* g = new GotoNode();
			g->mNext = gotos;
			g->mExpr = exp;
			g->mBlock = new InterCodeBasicBlock(proc);
			g->mDestruct = destack;
			InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			block->Append(jins);
			block->Close(g->mBlock, nullptr);
			block = g->mBlock;
			gotos = g;
			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_GOTO:
		{
			GotoNode* g = new GotoNode();
			g->mNext = gotos;
			g->mExpr = exp;
			g->mBlock = block;
			g->mDestruct = destack;
			InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);
			block->Append(jins);

			block = new InterCodeBasicBlock(proc);
			gotos = g;
			return ExValue(TheVoidTypeDeclaration);
		}

		case EX_SWITCH:
		{
			DestructStack* odestack = destack;

			vl = TranslateExpression(procType, proc, block, exp->mLeft, destack, gotos, breakBlock, continueBlock, inlineMapper);
			vl = Dereference(proc, exp, block, inlineMapper, vl);

			int vleft = 0, vright = 65535;

			if (vl.mType->mSize == 1)
			{
				if (vl.mType->mFlags & DTF_SIGNED)
				{
					vleft = -128;
					vright = 127;
				}
				else
					vright = 255;
			}
			else if (vl.mType->mFlags & DTF_SIGNED)
			{
				vleft = -32768;
				vright = 32767;
			}

			vl = CoerceType(proc, exp, block, inlineMapper, vl, TheSignedIntTypeDeclaration);

			InterCodeBasicBlock	* dblock = nullptr;
			InterCodeBasicBlock* sblock = block;

			block = nullptr;
			
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(proc);

			SwitchNodeArray		switchNodes({ 0 });

			Expression* sexp = exp->mRight;
			while (sexp)
			{
				Expression* cexp = sexp->mLeft;
				if (cexp->mType == EX_CASE)
				{
					SwitchNode	snode;
					snode.mLower = snode.mUpper = 0;
					snode.mBlock = nullptr;

					if (cexp->mLeft->mType == EX_CONSTANT && cexp->mLeft->mDecValue->mType == DT_CONST_INTEGER)
						snode.mLower = snode.mUpper = int(cexp->mLeft->mDecValue->mInteger);
					else if (cexp->mLeft->mType == EX_LIST)
					{
						Expression* rexp = cexp->mLeft;

						if (rexp->mLeft->mType == EX_CONSTANT && rexp->mLeft->mDecValue->mType == DT_CONST_INTEGER &&
							rexp->mRight->mType == EX_CONSTANT && rexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
						{
							snode.mLower = int(rexp->mLeft->mDecValue->mInteger);
							snode.mUpper = int(rexp->mRight->mDecValue->mInteger);
						}
						else
							mErrors->Error(cexp->mLeft->mLocation, ERRR_INVALID_CASE, "Integral constant range expected");
					}
					else
						mErrors->Error(cexp->mLeft->mLocation, ERRR_INVALID_CASE, "Integral constant expected");

					int i = 0;
					while (i < switchNodes.Size() && snode.mLower > switchNodes[i].mLower)
						i++;
					if (i < switchNodes.Size() && snode.mUpper >= switchNodes[i].mLower)
						mErrors->Error(cexp->mLeft->mLocation, ERRR_INVALID_CASE, "Duplicate case constant");
					else
					{
						if (block && block->mInstructions.Size() == 0)
							snode.mBlock = block;
						else
						{
							InterCodeBasicBlock* nblock = new InterCodeBasicBlock(proc);
							snode.mBlock = nblock;

							if (block)
							{
								InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

								block->Append(jins);
								block->Close(nblock, nullptr);
							}

							block = nblock;
						}
						switchNodes.Insert(i, snode);
					}
				}
				else if (cexp->mType == EX_DEFAULT)
				{
					if (!dblock)
					{
						dblock = new InterCodeBasicBlock(proc);

						if (block)
						{
							InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

							block->Append(jins);
							block->Close(dblock, nullptr);
						}

						block = dblock;
					}
					else
						mErrors->Error(cexp->mLocation, EERR_DUPLICATE_DEFAULT, "Duplicate default");
				}

				if (cexp->mRight)
					TranslateExpression(procType, proc, block, cexp->mRight, destack, gotos, BranchTarget(eblock, odestack), continueBlock, inlineMapper);

				sexp = sexp->mRight;
			}

			if (switchNodes.Size() > 0)
			{
				int i = 1, j = 1;
				while (i < switchNodes.Size())
				{
					if (switchNodes[i].mBlock == switchNodes[j - 1].mBlock && switchNodes[i].mLower == switchNodes[j - 1].mUpper + 1)
						switchNodes[j - 1].mUpper = switchNodes[i].mUpper;
					else
						switchNodes[j++] = switchNodes[i];
					i++;
				}
				switchNodes.SetSize(j);
			}

			BuildSwitchTree(proc, exp, sblock, inlineMapper, vl, switchNodes, 0, switchNodes.Size(), vleft, vright,  dblock ? dblock : eblock);

			if (block)
			{
				UnwindDestructStack(procType, proc, block, destack, odestack, inlineMapper);

				InterInstruction* jins = new InterInstruction(MapLocation(exp, inlineMapper), IC_JUMP);

				block->Append(jins);
				block->Close(eblock, nullptr);
			}

			block = eblock;
			
			destack = odestack;

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
		if (data->mBits)
		{
			if (data->mBits + data->mShift <= 8)
			{
				uint8	mask = uint8(((1 << data->mBits) - 1) << data->mShift);
				dp[offset] = (dp[offset] & ~mask) | (uint8(t << data->mShift) & mask);
			}
			else if (data->mBits + data->mShift <= 16)
			{
				uint16	mask = uint16(((1 << data->mBits) - 1) << data->mShift);

				dp[offset + 0] = (dp[offset + 0] & ~(mask & 0xff)) | (uint8(t << data->mShift) & (mask & 0xff));
				dp[offset + 1] = (dp[offset + 1] & ~(mask >> 8)) | (uint8(t >> (8 - data->mShift)) & (mask >> 8));
			}
			else
			{
				uint32	mask = uint32(((1 << data->mBits) - 1) << data->mShift);
				t = (t << data->mShift) & mask;

				dp[offset + 0] = (dp[offset + 0] & ~(mask & 0xff)) | (uint8(t      ));
				dp[offset + 1] = (dp[offset + 1] & ~(mask >> 8))   | (uint8(t >>  8));
				dp[offset + 2] = (dp[offset + 2] & ~(mask >> 16))  | (uint8(t >> 16));
				dp[offset + 3] = (dp[offset + 3] & ~(mask >> 24))  | (uint8(t >> 24));
			}
		}
		else
		{
			for (int i = 0; i < data->mBase->mSize; i++)
			{
				dp[offset + i * data->mBase->mStripe] = uint8(t & 0xff);
				t >>= 8;
			}
		}
	}
	else if (data->mType == DT_CONST_ADDRESS)
	{
		int64	t = data->mInteger;
		dp[offset + 0] = uint8(t & 0xff);
		dp[offset + data->mBase->mStripe] = uint8(t >> 8);
	}
	else if (data->mType == DT_CONST_FLOAT)
	{
		union { float f; uint32 i; } cast;
		cast.f = float(data->mNumber);
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
	else if (data->mType == DT_CONST_CONSTRUCTOR)
	{
		Declaration* dec = new Declaration(data->mLocation, DT_VARIABLE_REF);
		dec->mBase = variable->mDeclaration;
		dec->mOffset = offset;
		DestructStack* destack = nullptr;
		GotoNode* gotos = nullptr;
		data->mValue->mRight->mDecValue = dec;
		dec->mBase->mFlags |= DTF_VAR_ALIASING;
		TranslateExpression(nullptr, mMainInitProc, mMainInitBlock, data->mValue, destack, gotos, BranchTarget(), BranchTarget(), nullptr);
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
				if (dec->mFlags & DTF_VAR_ALIASING)
					var->mAliased = true;
				var->mLinkerObject = mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_DATA, dec->mAlignment);
				var->mLinkerObject->mVariable = var;
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

void InterCodeGenerator::TranslateLogic(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock* block, InterCodeBasicBlock* tblock, InterCodeBasicBlock* fblock, Expression* exp, DestructStack*& destack, GotoNode*& gotos, InlineMapper* inlineMapper)
{
	switch (exp->mType)
	{
	case EX_LOGICAL_NOT:
		TranslateLogic(procType, proc, block, fblock, tblock, exp->mLeft, destack, gotos, inlineMapper);
		break;
	case EX_LOGICAL_AND:
	{
		InterCodeBasicBlock* ablock = new InterCodeBasicBlock(proc);
		TranslateLogic(procType, proc, block, ablock, fblock, exp->mLeft, destack, gotos, inlineMapper);
		TranslateLogic(procType, proc, ablock, tblock, fblock, exp->mRight, destack, gotos, inlineMapper);
		break;
	}
	case EX_LOGICAL_OR:
	{
		InterCodeBasicBlock* oblock = new InterCodeBasicBlock(proc);
		TranslateLogic(procType, proc, block, tblock, oblock, exp->mLeft, destack, gotos, inlineMapper);
		TranslateLogic(procType, proc, oblock, tblock, fblock, exp->mRight, destack, gotos, inlineMapper);
		break;
	}
	default:
	{
		ExValue	vr = TranslateExpression(procType, proc, block, exp, destack, gotos, BranchTarget(), BranchTarget(), inlineMapper);

		if (vr.mType->mType == DT_TYPE_ARRAY)
			vr = Dereference(proc, exp, block, inlineMapper, vr, 1);
		else
		{
			vr = Dereference(proc, exp, block, inlineMapper, vr);
			if (!vr.mType->IsSimpleType())
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_TYPES, "Not a valid condition value");
		}
				
		InterInstruction* ins = new InterInstruction(MapLocation(exp, inlineMapper), IC_BRANCH);
		ins->mSrc[0].mType = InterTypeOf(vr.mType);
		ins->mSrc[0].mTemp = vr.mTemp;
		block->Append(ins);

		block->Close(tblock, fblock);
	}

	}
}

static InterCodeGenerator::DestructStack* FindBase(InterCodeGenerator::DestructStack* sfrom, InterCodeGenerator::DestructStack* sto)
{
	while (sfrom)
	{
		InterCodeGenerator::DestructStack* s = sto;
		while (s)
		{
			if (s == sfrom)
				return s;
			s = s->mNext;
		}

		sfrom = sfrom->mNext;
	}

	return sfrom;
}

InterCodeProcedure* InterCodeGenerator::TranslateProcedure(InterCodeModule * mod, Expression* exp, Declaration * dec)
{
	InterCodeProcedure* proc = new InterCodeProcedure(mod, dec->mLocation, dec->mQualIdent, mLinker->AddObject(dec->mLocation, dec->mQualIdent, dec->mSection, LOT_BYTE_CODE, dec->mAlignment));
	proc->mLinkerObject->mFullIdent = dec->FullIdent();

#if 0
	if (proc->mIdent && !strcmp(proc->mIdent->mString, "main"))
		exp->Dump(0);
#endif
#if 0
	if (proc->mIdent)
	{
		printf("TRANS %s\n", proc->mIdent->mString);
		exp->Dump(0);
	}
#endif

	uint64	outerCompilerOptions = mCompilerOptions;
	mCompilerOptions = dec->mCompilerOptions;

	proc->mCompilerOptions = mCompilerOptions;


	dec->mVarIndex = proc->mID;
	dec->mLinkerObject = proc->mLinkerObject;
	proc->mNumLocals = dec->mNumVars;
	proc->mNumParams = dec->mBase->mSize;
	proc->mDeclaration = dec;

	if (dec->mFlags & DTF_NATIVE)
		proc->mNativeProcedure = true;

	if (dec->mFlags & DTF_INTERRUPT)
		proc->mInterrupt = true;

	if (dec->mFlags & DTF_HWINTERRUPT)
		proc->mHardwareInterrupt = true;

	if (dec->mFlags & DTF_FUNC_INTRCALLED)
		proc->mInterruptCalled = true;
	
	if (dec->mFlags & DTF_DYNSTACK)
		proc->mDynamicStack = true;

	if ((dec->mFlags & DTF_PREVENT_INLINE) || (dec->mBase->mFlags & DTF_VARIADIC))
		proc->mNoInline = true;

	if (dec->mBase->mFlags & DTF_FASTCALL)
	{
		proc->mFastCallProcedure = true;

		if (dec->mFastCallSize > 0)
		{
			proc->mFastCallBase = dec->mFastCallBase;
			
			if (dec->mFastCallBase < BC_REG_FPARAMS_END - BC_REG_FPARAMS && dec->mFastCallSize > dec->mFastCallBase)
			{
				dec->mLinkerObject->mNumTemporaries = 1;
				dec->mLinkerObject->mTemporaries[0] = BC_REG_FPARAMS + dec->mFastCallBase;
				if (dec->mFastCallSize < BC_REG_FPARAMS_END - BC_REG_FPARAMS)
					dec->mLinkerObject->mTempSizes[0] = dec->mFastCallSize - dec->mFastCallBase;
				else
					dec->mLinkerObject->mTempSizes[0] = BC_REG_FPARAMS_END - dec->mFastCallBase;
			}

			Declaration* pdec = dec->mBase->mParams;
			while (pdec)
			{
				if (!(pdec->mFlags & DTF_FPARAM_UNUSED))
				{
					int	start = pdec->mVarIndex + BC_REG_FPARAMS, end = start + pdec->mSize;
					if (start < BC_REG_FPARAMS_END)
					{
						if (end > BC_REG_FPARAMS_END)
							end = BC_REG_FPARAMS_END;

						int i = 0;
						while (i < dec->mLinkerObject->mNumTemporaries && (dec->mLinkerObject->mTemporaries[i] > end || dec->mLinkerObject->mTemporaries[i] + dec->mLinkerObject->mTempSizes[i] < start))
							i++;
						if (i < dec->mLinkerObject->mNumTemporaries)
						{
							if (dec->mLinkerObject->mTemporaries[i] > start)
							{
								dec->mLinkerObject->mTempSizes[i] += dec->mLinkerObject->mTemporaries[i] - start;
								dec->mLinkerObject->mTemporaries[i] = start;
							}
							if (dec->mLinkerObject->mTemporaries[i] + dec->mLinkerObject->mTempSizes[i] < end)
								dec->mLinkerObject->mTempSizes[i] = end - dec->mLinkerObject->mTemporaries[i];
						}
						else
						{
							dec->mLinkerObject->mTemporaries[i] = start;
							dec->mLinkerObject->mTempSizes[i] = end - start;
							dec->mLinkerObject->mNumTemporaries++;
						}
					}
				}
				pdec = pdec->mNext;
			}
		}
		else
			proc->mFastCallBase = BC_REG_FPARAMS_END - BC_REG_FPARAMS;
	}
	else
		proc->mFastCallBase = BC_REG_FPARAMS_END - BC_REG_FPARAMS;

	if (dec->mBase->mBase->mType != DT_TYPE_VOID && dec->mBase->mBase->mType != DT_TYPE_STRUCT)
	{
		proc->mValueReturn = true;
		proc->mReturnType = InterTypeOf(dec->mBase->mBase);
	}

	InterCodeBasicBlock* entryBlock = new InterCodeBasicBlock(proc);

	if (!strcmp(proc->mIdent->mString, "main"))
	{
		mMainInitBlock = entryBlock;
		mMainInitProc = proc;
		entryBlock = new InterCodeBasicBlock(proc);
	}

	InterCodeBasicBlock* exitBlock = entryBlock;

	if (dec->mFlags & DTF_DEFINED)
	{
		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Generate intermediate code <%s>\n", proc->mIdent->mString);
#if 0
		Declaration* pdec = dec->mBase->mParams;
		while (pdec)
		{
			if (pdec->mFlags & DTF_FPARAM_CONST)
			{
				pdec->mVarIndex = proc->mNumLocals;
				proc->mNumLocals ++;
				dec->mNumVars++;

				InitParameter(proc, pdec, pdec->mVarIndex + proc->mFastCallBase);
				Expression* aexp = new Expression(pdec->mLocation, EX_ASSIGNMENT);
				Expression* pexp = new Expression(pdec->mLocation, EX_VARIABLE);

				pexp->mDecType = pdec->mBase;
				pexp->mDecValue = pdec;

				aexp->mDecType = pdec->mBase;
				aexp->mToken = TK_ASSIGN;
				aexp->mLeft = pexp;
				aexp->mRight = pdec->mValue;

				TranslateExpression(dec->mBase, proc, exitBlock, aexp, nullptr, nullptr, nullptr);
			}
			pdec = pdec->mNext;
		}
#endif

		DestructStack* destack = nullptr;
		GotoNode* gotos = nullptr;

		TranslateExpression(dec->mBase, proc, exitBlock, exp, destack, gotos, BranchTarget(), BranchTarget(), nullptr);

		ConnectGotos(dec->mBase, proc, gotos, nullptr);

		UnwindDestructStack(dec->mBase, proc, exitBlock, destack, nullptr, nullptr);

		if (!strcmp(proc->mIdent->mString, "main"))
		{
			InterInstruction* zins = new InterInstruction(dec->mLocation, IC_CONSTANT);
			zins->mDst.mType = IT_INT16;
			zins->mDst.mTemp = proc->AddTemporary(IT_INT16);
			zins->mConst.mType = IT_INT16;
			zins->mConst.mIntConst = 0;
			exitBlock->Append(zins);

			InterInstruction* rins = new InterInstruction(dec->mLocation, IC_RETURN_VALUE);
			rins->mSrc[0].mType = IT_INT16;
			rins->mSrc[0].mTemp = zins->mDst.mTemp;
			exitBlock->Append(rins);

			mMainStartupBlock = entryBlock;
		}
	}
	else
		mErrors->Error(dec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", dec->mQualIdent->mString);

	if (strcmp(proc->mIdent->mString, "main"))
	{
		InterInstruction* ins = new InterInstruction(exp ? exp->mEndLocation : dec->mLocation, IC_RETURN);
		exitBlock->Append(ins);
	}

	exitBlock->Close(nullptr, nullptr);

	if (mErrors->mErrorCount == 0 && proc != mMainInitProc)
	{
		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Optimize intermediate code <%s>\n", proc->mIdent->mString);

		proc->Close();
	}

	mCompilerOptions = outerCompilerOptions;

	return proc;
}

void InterCodeGenerator::CompleteMainInit(void)
{
	if (mErrors->mErrorCount == 0)
	{
		InterInstruction* ins = new InterInstruction(mMainInitProc->mLocation, IC_JUMP);
		mMainInitBlock->Append(ins);
		mMainInitBlock->Close(mMainStartupBlock, nullptr);

		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Optimize intermediate code <%s>\n", mMainInitProc->mIdent->mString);

		mMainInitProc->Close();
	}
}

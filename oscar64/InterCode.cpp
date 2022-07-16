#include "InterCode.h"
#include "CompilerTypes.h"

#include <stdio.h>
#include <math.h>

int InterTypeSize[] = {
	0,
	1,
	1,
	2,
	4,
	4,
	2
};

static bool IsCommutative(InterOperator op)
{
	return op == IA_ADD || op == IA_MUL || op == IA_AND || op == IA_OR || op == IA_XOR;
}

static bool IsIntegerType(InterType type)
{
	return type >= IT_INT8 && type <= IT_INT32;
}

IntegerValueRange::IntegerValueRange(void)
	: mMinState(S_UNKNOWN), mMaxState(S_UNKNOWN)
{}

IntegerValueRange::~IntegerValueRange(void)
{}

void IntegerValueRange::Reset(void)
{
	mMinState = S_UNKNOWN;
	mMaxState = S_UNKNOWN;
}


bool IntegerValueRange::Same(const IntegerValueRange& range) const
{
	if (mMinState == range.mMinState && mMaxState == range.mMaxState)
	{
		if ((mMinState == S_BOUND || mMinState == S_WEAK) && mMinValue != range.mMinValue)
			return false;
		if ((mMaxState == S_BOUND || mMaxState == S_WEAK) && mMaxValue != range.mMaxValue)
			return false;

		return true;
	}

	return false;
}

void IntegerValueRange::LimitMin(int64 value)
{
	if (mMinState != S_BOUND || mMinValue < value)
	{
		mMinState = S_BOUND;
		mMinValue = value;
	}
}

void IntegerValueRange::LimitMax(int64 value)
{
	if (mMaxState != S_BOUND || mMaxValue > value)
	{
		mMaxState = S_BOUND;
		mMaxValue = value;
	}
}

void IntegerValueRange::LimitMinWeak(int64 value)
{
	if (mMinState == S_UNBOUND || mMinValue < value)
	{
		mMinState = S_BOUND;
		mMinValue = value;
	}

}

void IntegerValueRange::LimitMaxWeak(int64 value)
{
	if (mMaxState == S_UNBOUND || mMaxValue > value)
	{
		mMaxState = S_BOUND;
		mMaxValue = value;
	}
}

bool IntegerValueRange::IsConstant(void) const
{
	return mMinState == S_BOUND && mMaxState == S_BOUND && mMinValue == mMaxValue;
}

void IntegerValueRange::Limit(const IntegerValueRange& range)
{
	if (range.mMinState == S_BOUND)
		LimitMin(range.mMinValue);
	if (range.mMaxState == S_BOUND)
		LimitMax(range.mMaxValue);
}

bool IntegerValueRange::Merge(const IntegerValueRange& range, bool head, bool initial)
{
	bool	changed = false;

	if (mMinState != S_UNBOUND)
	{
		if (range.mMinState == S_UNKNOWN)
		{
			if (head)
			{
				if (mMinState == S_BOUND)
					mMinState = S_WEAK;
			}
			else if (mMinState != S_UNKNOWN)
			{
				mMinState = S_UNKNOWN;
				changed = true;
			}
		}
		else if (range.mMinState == S_UNBOUND)
		{
			mMinState = S_UNBOUND;
			changed = true;
		}
		else if (mMinState == S_UNKNOWN)
		{
			if (head)
			{
				mMinState = S_WEAK;
				mMinValue = range.mMinValue;
				changed = true;
			}
		}
		else if (range.mMinValue < mMinValue)
		{
			if (range.mMinState == S_WEAK && (initial || !head))
				mMinState = S_WEAK;
			mMinValue = range.mMinValue;
			changed = true;
		}
		else if (mMinState == S_BOUND && range.mMinState == S_WEAK && (initial || !head))
		{
			mMinState = S_WEAK;
			changed = true;
		}
	}

	if (mMaxState != S_UNBOUND)
	{
		if (range.mMaxState == S_UNKNOWN)
		{
			if (head)
			{
				if (mMaxState == S_BOUND)
					mMaxState = S_WEAK;
			}
			else if (mMaxState != S_UNKNOWN)
			{
				mMaxState = S_UNKNOWN;
				changed = true;
			}
		}
		else if (range.mMaxState == S_UNBOUND)
		{
			mMaxState = S_UNBOUND;
			changed = true;
		}
		else if (mMaxState == S_UNKNOWN)
		{
			if (head)
			{
				mMaxState = S_WEAK;
				mMaxValue = range.mMaxValue;
				changed = true;
			}
		}
		else if (range.mMaxValue > mMaxValue)
		{
			if (range.mMaxState == S_WEAK && (initial || !head))
				mMaxState = S_WEAK;
			mMaxValue = range.mMaxValue;
			changed = true;
		}
		else if (mMaxState == S_BOUND && range.mMaxState == S_WEAK && (initial || !head))
		{
			mMaxState = S_WEAK;
			changed = true;
		}
	}

	return changed;
}


ValueSet::ValueSet(void)
{
	mSize = 32;
	mNum = 0;
	mInstructions = new InterInstructionPtr[mSize];
}

ValueSet::ValueSet(const ValueSet& values)
{
	int	i;

	mSize = values.mSize;
	mNum = values.mNum;
	mInstructions = new InterInstructionPtr[mSize];

	for (i = 0; i < mNum; i++)
		mInstructions[i] = values.mInstructions[i];
}

ValueSet& ValueSet::operator=(const ValueSet& values)
{
	int	i;

	mNum = values.mNum;
	if (mSize != values.mSize)
	{
		delete[] mInstructions;
		mSize = values.mSize;
		mInstructions = new InterInstructionPtr[mSize];
	}

	for (i = 0; i < mNum; i++)
		mInstructions[i] = values.mInstructions[i];

	return *this;
}

ValueSet::~ValueSet(void)
{
	delete[] mInstructions;
}

void ValueSet::FlushAll(void)
{
	mNum = 0;
}

void ValueSet::FlushFrameAliases(void)
{
	int	i;

	i = 0;

	while (i < mNum)
	{
		if (mInstructions[i]->mCode == IC_CONSTANT && mInstructions[i]->mDst.mType == IT_POINTER && mInstructions[i]->mConst.mMemory == IM_FRAME)
		{
			//
			// Address in frame space
			//
			mNum--;
			if (i < mNum)
			{
				mInstructions[i] = mInstructions[mNum];
			}
		}
		else
			i++;
	}
}


static bool MemPtrRange(const InterInstruction* ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset)
{
	while (ins && ins->mCode == IC_LEA && ins->mSrc[1].mMemory == IM_INDIRECT)
		ins = tvalue[ins->mSrc[1].mTemp];

	if (ins)
	{
		if (ins->mCode == IC_CONSTANT)
		{
			mem = ins->mConst.mMemory;
			vindex = ins->mConst.mVarIndex;
			offset = ins->mConst.mIntConst;

			return true;
		}
		else if (ins->mCode == IC_LEA)
		{
			mem = ins->mSrc[1].mMemory;
			vindex = ins->mSrc[1].mVarIndex;
			offset = ins->mSrc[1].mIntConst;

			return true;
		}
	}

	return false;
}


static bool MemRange(const InterInstruction* ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset, int& size)
{
	if (ins->mCode == IC_LOAD && ins->mSrc[0].mMemory == IM_INDIRECT)
	{
		size = ins->mSrc[0].mOperandSize;
		return MemPtrRange(tvalue[ins->mSrc[0].mTemp], tvalue, mem, vindex, offset);
	}
	else if (ins->mSrc[1].mMemory == IM_INDIRECT)
	{
		if (ins->mCode == IC_COPY)
			size = ins->mConst.mOperandSize;
		else
			size = ins->mSrc[1].mOperandSize;
		return MemPtrRange(tvalue[ins->mSrc[1].mTemp], tvalue, mem, vindex, offset);
	}

	if (ins)
	{
		if (ins->mCode == IC_LOAD)
		{
			mem = ins->mSrc[0].mMemory;
			vindex = ins->mSrc[0].mVarIndex;
			offset = ins->mSrc[0].mIntConst;
			size = ins->mSrc[0].mOperandSize;
		}
		else
		{
			mem = ins->mSrc[1].mMemory;
			vindex = ins->mSrc[1].mVarIndex;
			offset = ins->mSrc[1].mIntConst;
			size = ins->mSrc[1].mOperandSize;
		}

		return true;
	}

	return false;
}

void ValueSet::FlushCallAliases(const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams)
{
	int	i;

	InterMemory	mem;
	int			vindex;
	int			offset;
	int			size;

	i = 0;
	while (i < mNum)
	{
		if (mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE)
		{
			if (MemRange(mInstructions[i], tvalue, mem, vindex, offset, size) && 
				((mem == IM_PARAM && !aliasedParams[vindex]) ||
				 (mem == IM_LOCAL && !aliasedLocals[vindex])))
				i++;
			else
			{
				//
				// potential alias load
				//
				mNum--;
				if (i < mNum)
				{
					mInstructions[i] = mInstructions[mNum];
				}
			}
		}
		else
			i++;
	}
}

static bool CollidingMem(const InterOperand& op1, const InterOperand& op2, const GrowingVariableArray& staticVars)
{
	if (op1.mMemory != op2.mMemory)
	{
		if (op1.mMemory == IM_INDIRECT)
		{
			if (op2.mMemory == IM_GLOBAL)
				return staticVars[op2.mVarIndex]->mAliased;
			else if (op2.mMemory == IM_FPARAM)
				return false;
			else
				return true;
		}
		else if (op2.mMemory == IM_INDIRECT)
		{
			if (op1.mMemory == IM_GLOBAL)
				return staticVars[op1.mVarIndex]->mAliased;
			else if (op1.mMemory == IM_FPARAM)
				return false;
			else
				return true;
		}
		else
			return false;
	}

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_FPARAM:
	case IM_PARAM:
		return op1.mVarIndex == op2.mVarIndex && op1.mIntConst < op2.mIntConst + op2.mOperandSize && op2.mIntConst < op1.mIntConst + op1.mOperandSize;
	case IM_ABSOLUTE:
		return op1.mIntConst < op2.mIntConst + op2.mOperandSize && op2.mIntConst < op1.mIntConst + op1.mOperandSize;
	case IM_GLOBAL:
		if (op1.mLinkerObject == op2.mLinkerObject)
			return op1.mIntConst < op2.mIntConst + op2.mOperandSize && op2.mIntConst < op1.mIntConst + op1.mOperandSize;
		else
			return false;
	case IM_INDIRECT:
		if (op1.mTemp == op2.mTemp)
			return op1.mIntConst < op2.mIntConst + op2.mOperandSize && op2.mIntConst < op1.mIntConst + op1.mOperandSize;
		else if (op1.mLinkerObject && op2.mLinkerObject && op1.mLinkerObject != op2.mLinkerObject)
			return false;
		else
			return true;
	default:
		return false;
	}
}

static bool CollidingMem(const InterOperand& op, const InterInstruction* ins, const GrowingVariableArray& staticVars)
{
	if (ins->mCode == IC_LOAD)
		return CollidingMem(op, ins->mSrc[0], staticVars);
	else if (ins->mCode == IC_STORE)
		return CollidingMem(op, ins->mSrc[1], staticVars);
	else
		return false;
}

static bool CollidingMem(const InterInstruction* ins1, const InterInstruction* ins2, const GrowingVariableArray& staticVars)
{
	if (ins1->mCode == IC_LOAD)
		return CollidingMem(ins1->mSrc[0], ins2, staticVars);
	else if (ins1->mCode == IC_STORE)
		return CollidingMem(ins1->mSrc[1], ins2, staticVars);
	else
		return false;
}

static bool SameMem(const InterOperand& op1, const InterOperand& op2)
{
	if (op1.mMemory != op2.mMemory || op1.mType != op2.mType || op1.mIntConst != op2.mIntConst)
		return false;

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_FPARAM:
	case IM_PARAM:
		return op1.mVarIndex == op2.mVarIndex;
	case IM_ABSOLUTE:
		return true;
	case IM_GLOBAL:
		return op1.mLinkerObject == op2.mLinkerObject;
	case IM_INDIRECT:
		return op1.mTemp == op2.mTemp;
	default:
		return false;
	}
}

static bool SameMemRegion(const InterOperand& op1, const InterOperand& op2)
{
	if (op1.mMemory != op2.mMemory)
		return false;

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_FPARAM:
	case IM_PARAM:
	case IM_FRAME:
	case IM_FFRAME:
		return true;
	case IM_ABSOLUTE:
		return true;
	case IM_GLOBAL:
		return op1.mLinkerObject == op2.mLinkerObject;
	case IM_INDIRECT:
		return op1.mTemp == op2.mTemp;
	default:
		return false;
	}
}

// returns true if op2 is part of op1
static bool SameMemSegment(const InterOperand& op1, const InterOperand& op2)
{
	if (op1.mMemory != op2.mMemory || op1.mIntConst > op2.mIntConst || op1.mIntConst + op1.mOperandSize < op2.mIntConst + op2.mOperandSize)
		return false;

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_FPARAM:
	case IM_PARAM:
		return op1.mVarIndex == op2.mVarIndex;
	case IM_ABSOLUTE:
		return true;
	case IM_GLOBAL:
		return op1.mLinkerObject == op2.mLinkerObject;
	case IM_INDIRECT:
		return op1.mTemp == op2.mTemp;
	default:
		return false;
	}
}

static bool SameMemAndSize(const InterOperand& op1, const InterOperand& op2)
{
	if (op1.mMemory != op2.mMemory || op1.mType != op2.mType || op1.mIntConst != op2.mIntConst || op1.mOperandSize != op2.mOperandSize)
		return false;

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_FPARAM:
	case IM_PARAM:
		return op1.mVarIndex == op2.mVarIndex;
	case IM_ABSOLUTE:
		return true;
	case IM_GLOBAL:
		return op1.mLinkerObject == op2.mLinkerObject;
	case IM_INDIRECT:
		return op1.mTemp == op2.mTemp;
	default:
		return false;
	}
}

static bool SameMem(const InterOperand& op, const InterInstruction* ins)
{
	if (ins->mCode == IC_LOAD)
		return SameMem(op, ins->mSrc[0]);
	else if (ins->mCode == IC_STORE)
		return SameMem(op, ins->mSrc[1]);
	else
		return false;
}

static bool SameInstruction(const InterInstruction* ins1, const InterInstruction* ins2)
{
	if (ins1->mCode == ins2->mCode && ins1->mNumOperands == ins2->mNumOperands)
	{
		if ((ins1->mCode == IC_BINARY_OPERATOR || ins1->mCode == IC_UNARY_OPERATOR || ins1->mCode == IC_RELATIONAL_OPERATOR || ins1->mCode == IC_CONVERSION_OPERATOR) && ins1->mOperator != ins2->mOperator)
			return false;

		if (ins1->mCode == IC_BINARY_OPERATOR && IsCommutative(ins1->mOperator))
		{
			return
				ins1->mSrc[0].IsEqual(ins2->mSrc[0]) && ins1->mSrc[1].IsEqual(ins2->mSrc[1]) ||
				ins1->mSrc[0].IsEqual(ins2->mSrc[1]) && ins1->mSrc[1].IsEqual(ins2->mSrc[0]);
		}
		else
		{
			for (int i = 0; i < ins1->mNumOperands; i++)
				if (!ins1->mSrc[i].IsEqual(ins2->mSrc[i]))
					return false;
			return true;
		}
	}

	return false;
}


static int64 ConstantFolding(InterOperator oper, InterType type, int64 val1, int64 val2 = 0)
{
	switch (oper)
	{
	case IA_ADD:
		return val1 + val2;
		break;
	case IA_SUB:
		return val1 - val2;
		break;
	case IA_MUL:
		return val1 * val2;
		break;
	case IA_DIVU:
		return (uint64)val1 / (uint64)val2;
		break;
	case IA_DIVS:
		return val1 / val2;
		break;
	case IA_MODU:
		return (uint64)val1 % (uint64)val2;
		break;
	case IA_MODS:
		return val1 % val2;
		break;
	case IA_OR:
		return val1 | val2;
		break;
	case IA_AND:
		return val1 & val2;
		break;
	case IA_XOR:
		return val1 ^ val2;
		break;
	case IA_NEG:
		return -val1;
		break;
	case IA_NOT:
		return ~val1;
		break;
	case IA_SHL:
		return val1 << val2;
		break;
	case IA_SHR:
		return (uint64)val1 >> (uint64)val2;
		break;
	case IA_SAR:

		switch (type)
		{
		case IT_INT8:
			return int8(val1) >> val2;
		case IT_INT16:
			return int16(val1) >> val2;
		case IT_INT32:
			return int32(val1) >> val2;
		default:
			return val1 >> val2;
		}
		break;
	case IA_CMPEQ:
		return val1 == val2 ? 1 : 0;
		break;
	case IA_CMPNE:
		return val1 != val2 ? 1 : 0;
		break;
	case IA_CMPGES:
		return val1 >= val2 ? 1 : 0;
		break;
	case IA_CMPLES:
		return val1 <= val2 ? 1 : 0;
		break;
	case IA_CMPGS:
		return val1 > val2 ? 1 : 0;
		break;
	case IA_CMPLS:
		return val1 < val2 ? 1 : 0;
		break;
	case IA_CMPGEU:
		return (uint64)val1 >= (uint64)val2 ? 1 : 0;
		break;
	case IA_CMPLEU:
		return (uint64)val1 <= (uint64)val2 ? 1 : 0;
		break;
	case IA_CMPGU:
		return (uint64)val1 > (uint64)val2 ? 1 : 0;
		break;
	case IA_CMPLU:
		return (uint64)val1 < (uint64)val2 ? 1 : 0;
		break;
	default:
		return 0;
	}
}

static int64 ConstantRelationalFolding(InterOperator oper, double val1, double val2)
{
	switch (oper)
	{
	case IA_CMPEQ:
		return val1 == val2 ? 1 : 0;
		break;
	case IA_CMPNE:
		return val1 != val2 ? 1 : 0;
		break;
	case IA_CMPGES:
	case IA_CMPGEU:
		return val1 >= val2 ? 1 : 0;
		break;
	case IA_CMPLES:
	case IA_CMPLEU:
		return val1 <= val2 ? 1 : 0;
		break;
	case IA_CMPGS:
	case IA_CMPGU:
		return val1 > val2 ? 1 : 0;
		break;
	case IA_CMPLS:
	case IA_CMPLU:
		return val1 < val2 ? 1 : 0;
		break;
	default:
		return 0;
	}
}

static double ConstantFolding(InterOperator oper, double val1, double val2 = 0.0)
{
	switch (oper)
	{
	case IA_ADD:
		return val1 + val2;
		break;
	case IA_SUB:
		return val1 - val2;
		break;
	case IA_MUL:
		return val1 * val2;
		break;
	case IA_DIVU:
	case IA_DIVS:
		return val1 / val2;
		break;
	case IA_NEG:
		return -val1;
		break;
	case IA_ABS:
		return fabs(val1);
		break;
	case IA_FLOOR:
		return floor(val1);
		break;
	case IA_CEIL:
		return ceil(val1);
		break;

	default:
		return 0;
	}
}

static void ConversionConstantFold(InterInstruction * ins, const InterOperand & cop)
{
	switch (ins->mOperator)
	{
	case IA_INT2FLOAT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mFloatConst = (double)(cop.mIntConst);
		ins->mConst.mType = IT_FLOAT;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_FLOAT2INT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int)(cop.mFloatConst);
		ins->mConst.mType = IT_INT16;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_EXT8TO16S:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int8)(cop.mIntConst);
		ins->mConst.mType = IT_INT16;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_EXT8TO16U:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (uint8)(cop.mIntConst);
		ins->mConst.mType = IT_INT16;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_EXT16TO32S:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int16)(cop.mIntConst);
		ins->mConst.mType = IT_INT32;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_EXT16TO32U:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (uint16)(cop.mIntConst);
		ins->mConst.mType = IT_INT32;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	}
}

static void LoadConstantFold(InterInstruction* ins, InterInstruction* ains, const GrowingVariableArray& staticVars)
{
	const uint8* data;

	LinkerObject	*	lobj;
	int					offset;

	if (ains)
	{
		lobj = ains->mConst.mLinkerObject;
		offset = ains->mConst.mIntConst;
	}
	else
	{
		lobj = ins->mSrc[0].mLinkerObject;
		offset = ins->mSrc[0].mIntConst;
	}
	
	data = lobj->mData + offset;

	switch (ins->mDst.mType)
	{
	case IT_BOOL:
		ins->mConst.mIntConst = data[0] ? 1 : 0;
	case IT_INT8:
		ins->mConst.mIntConst = data[0];
		break;
	case IT_INT16:
		ins->mConst.mIntConst = (int)data[0] | ((int)data[1] << 8);
		break;
	case IT_POINTER:
	{
		int i = 0;
		while (i < lobj->mReferences.Size() && lobj->mReferences[i]->mOffset != offset)
			i++;
		if (i < lobj->mReferences.Size())
		{
			int j = 0;
			while (j < staticVars.Size() && !(staticVars[j] && staticVars[j]->mLinkerObject == lobj->mReferences[i]->mRefObject))
				j++;

			ins->mConst.mLinkerObject = lobj->mReferences[i]->mRefObject;
			ins->mConst.mIntConst = lobj->mReferences[i]->mRefOffset;
			ins->mConst.mMemory = IM_GLOBAL;
			ins->mConst.mOperandSize = ins->mConst.mLinkerObject->mSize;
			if (j < staticVars.Size())
				ins->mConst.mVarIndex = staticVars[j]->mIndex;
			else
				ins->mConst.mVarIndex = -1;
		}
		else
		{
			ins->mConst.mIntConst = (int)data[0] | ((int)data[1] << 8);
			ins->mConst.mMemory = IM_ABSOLUTE;
		}

	} break;
	case IT_INT32:
		ins->mConst.mIntConst = (int)data[0] | ((int)data[1] << 8) | ((int)data[2] << 16) | ((int)data[3] << 24);
		break;
	case IT_FLOAT:
	{
		union { float f; unsigned int v; } cc;
		cc.v = (int)data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		ins->mConst.mFloatConst = cc.v;
	} break;
	}

	ins->mCode = IC_CONSTANT;
	ins->mConst.mType = ins->mDst.mType;
	ins->mSrc[0].mTemp = -1;
}

void ValueSet::InsertValue(InterInstruction * ins)
{
	InterInstructionPtr* nins;
	int								i;

	if (mNum == mSize)
	{
		mSize *= 2;
		nins = new InterInstructionPtr[mSize];
		for (i = 0; i < mNum; i++)
			nins[i] = mInstructions[i];
		delete[] mInstructions;
		mInstructions = nins;
	}

	mInstructions[mNum++] = ins;
}

static bool HasSideEffect(InterCode code)
{
	return code == IC_CALL || code == IC_CALL_NATIVE || code == IC_ASSEMBLER;
}

static bool IsMoveable(InterCode code)
{
	if (HasSideEffect(code) || code == IC_COPY || code == IC_STRCPY || code == IC_STORE || code == IC_BRANCH || code == IC_POP_FRAME || code == IC_PUSH_FRAME)
		return false;
	if (code == IC_RETURN || code == IC_RETURN_STRUCT || code == IC_RETURN_VALUE)
		return false;

	return true;
}


static bool CanBypassLoad(const InterInstruction* lins, const InterInstruction* bins)
{
	// Check ambiguity
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY)
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;

	// True data dependency
	if (bins->UsesTemp(lins->mDst.mTemp))
		return false;

	if (bins->mCode == IC_STORE)
	{
		if (lins->mVolatile)
			return false;
		else if (lins->mSrc[0].mMemory == IM_INDIRECT && bins->mSrc[1].mMemory == IM_INDIRECT)
		{
			return lins->mSrc[0].mLinkerObject && bins->mSrc[1].mLinkerObject && lins->mSrc[0].mLinkerObject != bins->mSrc[1].mLinkerObject;
		}
		else if (lins->mSrc[0].mTemp >= 0 || bins->mSrc[1].mTemp >= 0)
			return false;
		else if (lins->mSrc[0].mMemory != bins->mSrc[1].mMemory)
			return true;
		else if (lins->mSrc[0].mMemory == IM_GLOBAL)
		{
			return lins->mSrc[0].mLinkerObject != bins->mSrc[1].mLinkerObject ||
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (lins->mSrc[0].mMemory == IM_ABSOLUTE)
		{
			return
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (lins->mSrc[0].mMemory == IM_LOCAL)
		{
			return lins->mSrc[0].mVarIndex != bins->mSrc[1].mVarIndex ||
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else
			return false;
	}

	// False data dependency
	if (lins->mSrc[0].mTemp >= 0 && lins->mSrc[0].mTemp == bins->mDst.mTemp)
		return false;

	return true;
}

static bool CanBypass(const InterInstruction* lins, const InterInstruction* bins)
{
	if (HasSideEffect(lins->mCode) && HasSideEffect(bins->mCode))
		return false;

	if (lins->mDst.mTemp >= 0)
	{
		if (lins->mDst.mTemp == bins->mDst.mTemp)
			return false;

		for (int i = 0; i < bins->mNumOperands; i++)
			if (lins->mDst.mTemp == bins->mSrc[i].mTemp)
				return false;
	}
	if (bins->mDst.mTemp >= 0)
	{
		for (int i = 0; i < lins->mNumOperands; i++)
			if (bins->mDst.mTemp == lins->mSrc[i].mTemp)
				return false;
	}
	if (bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_POP_FRAME)
	{
		if (lins->mCode == IC_CONSTANT && lins->mDst.mType == IT_POINTER && lins->mConst.mMemory == IM_FRAME)
			return false;
	}

	return true;
}

static bool CanBypassUp(const InterInstruction* lins, const InterInstruction* bins)
{
	if (HasSideEffect(lins->mCode) && HasSideEffect(bins->mCode))
		return false;

	if (lins->mDst.mTemp >= 0)
	{
		if (lins->mDst.mTemp == bins->mDst.mTemp)
			return false;

		for (int i = 0; i < bins->mNumOperands; i++)
			if (lins->mDst.mTemp == bins->mSrc[i].mTemp)
				return false;
	}
	if (bins->mDst.mTemp >= 0)
	{
		for (int i = 0; i < lins->mNumOperands; i++)
			if (bins->mDst.mTemp == lins->mSrc[i].mTemp)
				return false;
	}
	if (bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_POP_FRAME)
	{
		if (lins->mCode == IC_CONSTANT && lins->mDst.mType == IT_POINTER && lins->mConst.mMemory == IM_FRAME)
			return false;
	}

	return true;
}

static bool CanBypassLoadUp(const InterInstruction* lins, const InterInstruction* bins)
{
	// Check ambiguity
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY)
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;

	// False data dependency
	if (bins->UsesTemp(lins->mDst.mTemp) || bins->mDst.mTemp == lins->mDst.mTemp)
		return false;

	// True data dependency
	if (lins->mSrc[0].mTemp >= 0 && lins->mSrc[0].mTemp == bins->mDst.mTemp)
		return false;

	if (bins->mCode == IC_STORE)
	{
		if (lins->mVolatile)
			return false;
		else if (bins->mSrc[1].mMemory == IM_FRAME || bins->mSrc[1].mMemory == IM_FFRAME)
			return true;
		else if (lins->mSrc[0].mMemory == IM_INDIRECT && bins->mSrc[1].mMemory == IM_INDIRECT)
		{
			return lins->mSrc[0].mLinkerObject && bins->mSrc[1].mLinkerObject && lins->mSrc[0].mLinkerObject != bins->mSrc[1].mLinkerObject;
		}
		else if (lins->mSrc[0].mTemp >= 0 || bins->mSrc[1].mTemp >= 0)
			return false;
		else if (lins->mSrc[0].mMemory != bins->mSrc[1].mMemory)
			return true;
		else if (lins->mSrc[0].mMemory == IM_GLOBAL)
		{
			return lins->mSrc[0].mLinkerObject != bins->mSrc[1].mLinkerObject ||
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (lins->mSrc[0].mMemory == IM_ABSOLUTE)
		{
			return
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (lins->mSrc[0].mMemory == IM_LOCAL)
		{
			return lins->mSrc[0].mVarIndex != bins->mSrc[1].mVarIndex ||
				lins->mSrc[0].mIntConst + lins->mSrc[0].mOperandSize <= bins->mSrc[1].mIntConst ||
				lins->mSrc[0].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else
			return false;
	}

	return true;
}


static bool IsChained(const InterInstruction* ins, const InterInstruction* nins)
{
	if (ins->mDst.mTemp >= 0)
	{
		for (int i = 0; i < nins->mNumOperands; i++)
			if (ins->mDst.mTemp == nins->mSrc[i].mTemp)
				return true;
	}

	return false;
}

static bool CanBypassStore(const InterInstruction* sins, const InterInstruction* bins)
{
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY || bins->mCode == IC_PUSH_FRAME)
		return false;

	// True data dependency
	if (bins->mDst.mTemp >= 0)
	{
		for (int i = 0; i < sins->mNumOperands; i++)
			if (bins->mDst.mTemp == sins->mSrc[i].mTemp)
				return false;
	}

	InterMemory	sm = IM_NONE, bm = IM_NONE;
	int			bi = -1, si = -1, bt = -1, st = -1, bo = 0, so = 0, bz = 1, sz = 1;
	LinkerObject* slo = nullptr, * blo = nullptr;

	if (sins->mCode == IC_LOAD)
	{
		sm = sins->mSrc[0].mMemory;
		si = sins->mSrc[0].mVarIndex;
		st = sins->mSrc[0].mTemp;
		so = sins->mSrc[0].mIntConst;
		slo = sins->mSrc[0].mLinkerObject;
		sz = InterTypeSize[sins->mDst.mType];
	}
	else if (sins->mCode == IC_LEA || sins->mCode == IC_STORE)
	{
		sm = sins->mSrc[1].mMemory;
		si = sins->mSrc[1].mVarIndex;
		st = sins->mSrc[1].mTemp;
		so = sins->mSrc[1].mIntConst;
		slo = sins->mSrc[1].mLinkerObject;
		sz = InterTypeSize[sins->mSrc[0].mType];
	}

	if (bins->mCode == IC_LOAD)
	{
		bm = bins->mSrc[0].mMemory;
		bi = bins->mSrc[0].mVarIndex;
		bt = bins->mSrc[0].mTemp;
		bo = bins->mSrc[0].mIntConst;
		blo = bins->mSrc[0].mLinkerObject;
		bz = InterTypeSize[bins->mDst.mType];
	}
	else if (bins->mCode == IC_LEA || bins->mCode == IC_STORE)
	{
		bm = bins->mSrc[1].mMemory;
		bi = bins->mSrc[1].mVarIndex;
		bt = bins->mSrc[1].mTemp;
		bo = bins->mSrc[1].mIntConst;
		blo = bins->mSrc[1].mLinkerObject;
		bz = InterTypeSize[bins->mSrc[0].mType];
	}

	// Check ambiguity
	if (bins->mCode == IC_STORE || bins->mCode == IC_LOAD)
	{
		if (sm == IM_LOCAL)
		{
			if (bm == IM_PARAM || bm == IM_GLOBAL || bm == IM_FPARAM)
				;
			else if (bm == IM_LOCAL)
			{
				if (bi == si)
					return false;
			}
			else
				return false;
		}
		else if (sm == IM_FRAME || sm == IM_FFRAME)
			;
		else if (sm == IM_FPARAM)
		{
			if (bi == si)
				return false;
		}
		else if (sm == IM_INDIRECT && bm == IM_INDIRECT)
		{
			if (st == bt)
			{
				if (so + sz > bo && bo + bz > so)
					return false;
			}
			else
				return slo && blo && slo != blo;
		}
		else
			return false;
	}

	if (sm == IM_FRAME && (bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_POP_FRAME))
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;


	return true;
}

static bool StoreAliasing(const InterInstruction * lins, const InterInstruction* sins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, const GrowingVariableArray& staticVars)
{
	InterMemory	lmem, smem;
	int			lvindex, svindex;
	int			loffset, soffset;
	int			lsize, ssize;

	if (MemRange(lins, tvalue, lmem, lvindex, loffset, lsize))
	{
		if (MemRange(sins, tvalue, smem, svindex, soffset, ssize))
		{
			if (smem == lmem && svindex == lvindex)
			{
				if (soffset + ssize >= loffset && loffset + lsize >= soffset)
					return true;
			}

			return false;
		}

		if (lmem == IM_LOCAL)
			return aliasedLocals[lvindex];
		else if (lmem == IM_PARAM || lmem == IM_FPARAM)
			return aliasedParams[lvindex];
		else if (lmem == IM_GLOBAL)
			return staticVars[lvindex]->mAliased;
	}
	else if (MemRange(sins, tvalue, smem, svindex, soffset, ssize))
	{
		if (smem == IM_LOCAL)
			return aliasedLocals[svindex];
		else if (smem == IM_PARAM || smem == IM_FPARAM)
			return aliasedParams[svindex];
		else if (smem == IM_GLOBAL)
			return staticVars[svindex]->mAliased;
	}

	return true;
}

void ValueSet::Intersect(ValueSet& set)
{
	int k = 0;
	for(int i=0; i<mNum; i++)
	{
		int j = 0;
		while (j < set.mNum && mInstructions[i] != set.mInstructions[j])
			j++;
		if (j < set.mNum)
		{
			mInstructions[k] = mInstructions[i];
			k++;
		}
	}
	mNum = k;
}

TempForwardingTable::TempForwardingTable(void) : mAssoc(Assoc(-1, -1, -1))
{
}

TempForwardingTable::TempForwardingTable(const TempForwardingTable& table) : mAssoc(Assoc(-1, -1, -1))
{
	for (int i = 0; i < table.mAssoc.Size(); i++)
	{
		mAssoc[i].mAssoc = table.mAssoc[i].mAssoc;
		mAssoc[i].mSucc = table.mAssoc[i].mSucc;
		mAssoc[i].mPred = table.mAssoc[i].mPred;
	}
}

TempForwardingTable& TempForwardingTable::operator=(const TempForwardingTable& table)
{
	mAssoc.SetSize(table.mAssoc.Size());
	for (int i = 0; i < table.mAssoc.Size(); i++)
	{
		mAssoc[i].mAssoc = table.mAssoc[i].mAssoc;
		mAssoc[i].mSucc = table.mAssoc[i].mSucc;
		mAssoc[i].mPred = table.mAssoc[i].mPred;
	}

	return *this;
}

void TempForwardingTable::Intersect(const TempForwardingTable& table)
{
	for (int i = 0; i < table.mAssoc.Size(); i++)
	{
		if (mAssoc[i].mAssoc != table.mAssoc[i].mAssoc)
			this->Destroy(i);
	}
}

int TempForwardingTable::Size(void) const
{
	return mAssoc.Size();
}

void TempForwardingTable::SetSize(int size)
{
	int i;
	mAssoc.SetSize(size);

	for (i = 0; i < size; i++)
		mAssoc[i] = Assoc(i, i, i);
}

void TempForwardingTable::Reset(void)
{
	int i;

	for (i = 0; i < mAssoc.Size(); i++)
		mAssoc[i] = Assoc(i, i, i);
}

int TempForwardingTable::operator[](int n)
{
	return mAssoc[n].mAssoc;
}

void TempForwardingTable::Destroy(int n)
{
	int i, j;

	if (mAssoc[n].mAssoc == n)
	{
		i = mAssoc[n].mSucc;
		while (i != n)
		{
			j = mAssoc[i].mSucc;
			mAssoc[i] = Assoc(i, i, i);
			i = j;
		}
	}
	else
	{
		mAssoc[mAssoc[n].mPred].mSucc = mAssoc[n].mSucc;
		mAssoc[mAssoc[n].mSucc].mPred = mAssoc[n].mPred;
	}

	mAssoc[n] = Assoc(n, n, n);
}

void TempForwardingTable::Build(int from, int to)
{
	int i;

	from = mAssoc[from].mAssoc;
	to = mAssoc[to].mAssoc;

	if (from != to)
	{
		i = mAssoc[from].mSucc;
		while (i != from)
		{
			mAssoc[i].mAssoc = to;
			i = mAssoc[i].mSucc;
		}
		mAssoc[from].mAssoc = to;

		mAssoc[mAssoc[to].mSucc].mPred = mAssoc[from].mPred;
		mAssoc[mAssoc[from].mPred].mSucc = mAssoc[to].mSucc;
		mAssoc[to].mSucc = from;
		mAssoc[from].mPred = to;
	}
}



bool InterInstruction::ReferencesTemp(int temp) const
{
	if (temp < 0)
		return false;

	if (temp == mDst.mTemp)
		return true;
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp == temp)
			return true;
	return false;
}

InterInstruction* InterInstruction::Clone(void) const
{
	InterInstruction* ins = new InterInstruction();
	*ins = *this;
	return ins;
}

bool InterInstruction::IsEqual(const InterInstruction* ins) const
{
	if (mCode != ins->mCode)
		return false;

	if (mCode == IC_BINARY_OPERATOR || mCode == IC_UNARY_OPERATOR || mCode == IC_RELATIONAL_OPERATOR || mCode == IC_CONVERSION_OPERATOR)
	{
		if (mOperator != ins->mOperator)
			return false;
	}

	if (!mDst.IsEqual(ins->mDst))
		return false;

	for (int i = 0; i < mNumOperands; i++)
		if (!mSrc[i].IsEqual(ins->mSrc[i]))
			return false;

	if (mCode == IC_CONSTANT && !mConst.IsEqual(ins->mConst))
		return false;

	return true;
}

bool InterInstruction::IsEqualSource(const InterInstruction* ins) const
{
	if (mCode != ins->mCode)
		return false;

	if (mCode == IC_BINARY_OPERATOR || mCode == IC_UNARY_OPERATOR || mCode == IC_RELATIONAL_OPERATOR || mCode == IC_CONVERSION_OPERATOR)
	{
		if (mOperator != ins->mOperator)
			return false;
	}

	for (int i = 0; i < mNumOperands; i++)
		if (!mSrc[i].IsEqual(ins->mSrc[i]))
			return false;

	if (mCode == IC_CONSTANT && !mConst.IsEqual(ins->mConst))
		return false;

	return true;
}



void ValueSet::UpdateValue(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, const GrowingVariableArray& staticVars)
{
	int	i, value, temp;

	temp = ins->mDst.mTemp;

	if (temp >= 0)
	{
		i = 0;
		while (i < mNum)
		{
			if (mInstructions[i]->ReferencesTemp(temp))
			{
				mNum--;
				if (i < mNum)
					mInstructions[i] = mInstructions[mNum];
			}
			else
				i++;
		}
	}

	for (i = 0; i < ins->mNumOperands; i++)
	{
		temp = ins->mSrc[i].mTemp;
		if (temp >= 0 && tvalue[temp])
		{
			ins->mSrc[i].mTemp = tvalue[temp]->mDst.mTemp;
		}
	}

	switch (ins->mCode)
	{
	case IC_LOAD:
		i = 0;
		while (i < mNum &&
			(mInstructions[i]->mCode != IC_LOAD ||
				mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp ||
				mInstructions[i]->mSrc[0].mOperandSize != ins->mSrc[0].mOperandSize))
		{
			i++;
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
			ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
			ins->mNumOperands = 1;
			assert(ins->mSrc[0].mTemp >= 0);
		}
		else
		{
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_STORE ||
					mInstructions[i]->mSrc[1].mTemp != ins->mSrc[0].mTemp ||
					mInstructions[i]->mSrc[1].mOperandSize != ins->mSrc[0].mOperandSize))
			{
				i++;
			}

			if (i < mNum)
			{
				if (mInstructions[i]->mSrc[0].mTemp < 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[0].mType = mInstructions[i]->mSrc[0].mType;
					ins->mConst.mIntConst = mInstructions[i]->mSrc[0].mIntConst;
				}
				else
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mSrc[0].mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mSrc[0].mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT && tvalue[ins->mSrc[0].mTemp]->mConst.mMemory == IM_GLOBAL && (tvalue[ins->mSrc[0].mTemp]->mConst.mLinkerObject->mFlags & LOBJF_CONST))
			{
				LoadConstantFold(ins, tvalue[ins->mSrc[0].mTemp], staticVars);
				InsertValue(ins);
			}
			else
			{
				if (!ins->mVolatile)
					InsertValue(ins);
			}
		}

		break;
	case IC_STORE:
		i = 0;
		while (i < mNum)
		{
			if ((mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE) && StoreAliasing(mInstructions[i], ins, tvalue, aliasedLocals, aliasedParams, staticVars))
			{
				mNum--;
				if (mNum > 0)
					mInstructions[i] = mInstructions[mNum];
			}
			else
				i++;
		}

		if (!ins->mVolatile)
			InsertValue(ins);
		break;
	case IC_COPY:
	case IC_STRCPY:
		i = 0;
		while (i < mNum)
		{
			if ((mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE) && StoreAliasing(mInstructions[i], ins, tvalue, aliasedLocals, aliasedParams, staticVars))
			{
				mNum--;
				if (mNum > 0)
					mInstructions[i] = mInstructions[mNum];
			}
			else
				i++;
		}

		break;

	case IC_CONSTANT:
		switch (ins->mDst.mType)
		{
		case IT_FLOAT:
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mDst.mType != ins->mDst.mType ||
					mInstructions[i]->mConst.mFloatConst != ins->mConst.mFloatConst))
			{
				i++;
			}
			break;
		case IT_POINTER:
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mDst.mType != ins->mDst.mType ||
					mInstructions[i]->mConst.mIntConst != ins->mConst.mIntConst ||
					mInstructions[i]->mConst.mMemory != ins->mConst.mMemory ||
					mInstructions[i]->mConst.mVarIndex != ins->mConst.mVarIndex ||
					mInstructions[i]->mConst.mLinkerObject != ins->mConst.mLinkerObject))
			{
				i++;
			}
			break;
		default:

			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mDst.mType != ins->mDst.mType ||
					mInstructions[i]->mConst.mIntConst != ins->mConst.mIntConst))
			{
				i++;
			}
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
			ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
			ins->mNumOperands = 1;
			assert(ins->mSrc[0].mTemp >= 0);
		}
		else
		{
			InsertValue(ins);
		}
		break;

	case IC_LEA:
		i = 0;
		while (i < mNum &&
			(mInstructions[i]->mCode != IC_LEA ||
				mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp ||
				mInstructions[i]->mSrc[0].mIntConst != ins->mSrc[0].mIntConst ||
				mInstructions[i]->mSrc[1].mTemp != ins->mSrc[1].mTemp))
		{
			i++;
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
			ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
			ins->mSrc[1].mTemp = -1;
			ins->mNumOperands = 1;
			assert(ins->mSrc[0].mTemp >= 0);
		}
		else
		{
			InsertValue(ins);
		}
		break;

	case IC_BINARY_OPERATOR:
		switch (ins->mSrc[0].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mFloatConst = ConstantFolding(ins->mOperator, tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mDst.mType != ins->mDst.mType ||
						mInstructions[i]->mConst.mFloatConst != ins->mConst.mFloatConst))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_BINARY_OPERATOR ||
						mInstructions[i]->mOperator != ins->mOperator ||
						mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp ||
						mInstructions[i]->mSrc[1].mTemp != ins->mSrc[1].mTemp))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			assert(ins->mSrc[1].mType == IT_INT8 || ins->mSrc[1].mType == IT_INT16 || ins->mSrc[1].mType == IT_INT32);

			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

				return;
			}

			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				if ((ins->mOperator == IA_ADD || ins->mOperator == IA_SUB ||
					ins->mOperator == IA_OR || ins->mOperator == IA_XOR ||
					ins->mOperator == IA_SHL || ins->mOperator == IA_SHR || ins->mOperator == IA_SAR) && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == 0 ||
					(ins->mOperator == IA_MUL || ins->mOperator == IA_DIVU || ins->mOperator == IA_DIVS) && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == 1 ||
					(ins->mOperator == IA_AND) && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
					ins->mSrc[0].mType = ins->mSrc[1].mType;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND) && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if (ins->mOperator == IA_MUL && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[0] = ins->mSrc[1];
					ins->mSrc[1].mTemp = -1;
					ins->mSrc[1].mType = IT_NONE;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
			}
			else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
			{
				if ((ins->mOperator == IA_ADD || ins->mOperator == IA_OR || ins->mOperator == IA_XOR) && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 0 ||
					(ins->mOperator == IA_MUL) && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 1 ||
					(ins->mOperator == IA_AND) && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND ||
					ins->mOperator == IA_SHL || ins->mOperator == IA_SHR || ins->mOperator == IA_SAR) && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if (ins->mOperator == IA_MUL && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[1].mTemp = -1;
					ins->mSrc[1].mType = IT_NONE;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if (ins->mOperator == IA_SUB && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[1].mTemp = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
			}
			else if (ins->mSrc[0].mTemp == ins->mSrc[1].mTemp)
			{
				if (ins->mOperator == IA_SUB || ins->mOperator == IA_XOR)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
				else if (ins->mOperator == IA_AND || ins->mOperator == IA_OR)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);

					return;
				}
			}
			
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_BINARY_OPERATOR ||
					mInstructions[i]->mOperator != ins->mOperator ||
					mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp ||
					mInstructions[i]->mSrc[1].mTemp != ins->mSrc[1].mTemp))
			{
				i++;
			}

			if (i < mNum)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
				ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 1;
				assert(ins->mSrc[0].mTemp >= 0);
			}
			else
			{
				InsertValue(ins);
			}
			break;
		}
		break;

	case IC_CONVERSION_OPERATOR:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ConversionConstantFold(ins, tvalue[ins->mSrc[0].mTemp]->mConst);
			if (ins->mDst.mType == IT_FLOAT)
			{
				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mDst.mType != ins->mDst.mType ||
						mInstructions[i]->mConst.mFloatConst != ins->mConst.mFloatConst))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				InsertValue(ins);
			}
		}
		else
		{
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONVERSION_OPERATOR ||
					mInstructions[i]->mOperator != ins->mOperator ||
					mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp))
			{
				i++;
			}

			if (i < mNum)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
				ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 1;
				assert(ins->mSrc[0].mTemp >= 0);
			}
			else
			{
				InsertValue(ins);
			}
		}
		break;

	case IC_UNARY_OPERATOR:
		switch (ins->mSrc[0].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mFloatConst = ConstantFolding(ins->mOperator, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mSrc[0].mTemp = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mDst.mType != ins->mDst.mType ||
						mInstructions[i]->mConst.mFloatConst != ins->mConst.mFloatConst))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_UNARY_OPERATOR ||
						mInstructions[i]->mOperator != ins->mOperator ||
						mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
				ins->mSrc[0].mTemp = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mDst.mType != ins->mDst.mType ||
						mInstructions[i]->mConst.mIntConst != ins->mConst.mIntConst))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_UNARY_OPERATOR ||
						mInstructions[i]->mOperator != ins->mOperator ||
						mInstructions[i]->mSrc[0].mTemp != ins->mSrc[0].mTemp))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = mInstructions[i]->mDst.mTemp;
					ins->mSrc[0].mType = mInstructions[i]->mDst.mType;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		}
		break;

	case IC_RELATIONAL_OPERATOR:
		switch (ins->mSrc[1].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantRelationalFolding(ins->mOperator, tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONVERSION_OPERATOR && 
					 tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_INT2FLOAT &&
					 tvalue[ins->mSrc[0].mTemp]->mOperator == tvalue[ins->mSrc[1].mTemp]->mOperator)
			{
				ins->mSrc[0].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
				ins->mSrc[0].mTemp = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mTemp;
				ins->mSrc[1].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
				ins->mSrc[1].mTemp = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mTemp;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT &&
					tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_INT2FLOAT)
			{
				bool	toconst = false;
				int		cvalue = 0;

				if (tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO16S || tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO32S)
				{
					int	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					if (ivalue < -128)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 1;
							break;
						}
					}
					else if (ivalue > 127)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 1;
							break;
						}
					}					
				}
				else if (tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO16U || tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO32U)
				{
					int	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					if (ivalue < 0)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 1;
							break;
						}
					}
					else if (ivalue > 255)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 1;
							break;
						}
					}
					else
					{
						switch (ins->mOperator)
						{
						case IA_CMPGES:
							ins->mOperator = IA_CMPGEU;
							break;
						case IA_CMPLES:
							ins->mOperator = IA_CMPLEU;
							break;
						case IA_CMPGS:
							ins->mOperator = IA_CMPGU;
							break;
						case IA_CMPLS:
							ins->mOperator = IA_CMPLU;
							break;
						}
					}
				}

				if (toconst)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = cvalue;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
				}
				else
				{
					ins->mSrc[0].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
					ins->mSrc[1].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
					ins->mSrc[1].mTemp = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mTemp;
				}

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
					tvalue[ins->mSrc[0].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[0].mTemp]->mOperator != IA_INT2FLOAT)
			{
				bool	toconst = false;
				int		cvalue = 0;

				if (tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO16S || tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO32S)
				{
					int	ivalue = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
					if (ivalue > 127)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 1;
							break;
						}
					}
					else if (ivalue < -128)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 1;
							break;
						}
					}					
				}
				else if (tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO16U || tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO32U)
				{
					int	ivalue = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
					if (ivalue > 255)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 1;
							break;
						}
					}
					else if (ivalue < 0)
					{
						toconst = true;
						switch (ins->mOperator)
						{
						case IA_CMPEQ:
						case IA_CMPGES:
						case IA_CMPGS:
						case IA_CMPGEU:
						case IA_CMPGU:
							cvalue = 0;
							break;
						case IA_CMPNE:
						case IA_CMPLES:
						case IA_CMPLS:
						case IA_CMPLEU:
						case IA_CMPLU:
							cvalue = 1;
							break;
						}
					}
					else
					{
						switch (ins->mOperator)
						{
						case IA_CMPGES:
							ins->mOperator = IA_CMPGEU;
							break;
						case IA_CMPLES:
							ins->mOperator = IA_CMPLEU;
							break;
						case IA_CMPGS:
							ins->mOperator = IA_CMPGU;
							break;
						case IA_CMPLS:
							ins->mOperator = IA_CMPLU;
							break;
						}
					}
				}

				if (toconst)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = cvalue;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
				}
				else
				{
					ins->mSrc[1].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
					ins->mSrc[0].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
					ins->mSrc[0].mTemp = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mTemp;
				}

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			else if (ins->mSrc[1].mTemp == ins->mSrc[0].mTemp)
			{
				ins->mCode = IC_CONSTANT;

				switch (ins->mOperator)
				{
				case IA_CMPEQ:
				case IA_CMPGES:
				case IA_CMPLES:
				case IA_CMPGEU:
				case IA_CMPLEU:
					ins->mConst.mIntConst = 1;
					break;
				case IA_CMPNE:
				case IA_CMPGS:
				case IA_CMPLS:
				case IA_CMPGU:
				case IA_CMPLU:
					ins->mConst.mIntConst = 0;
					break;
				}
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams, staticVars);
			}
			break;
		}
		break;
	case IC_BRANCH:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			if (tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst)
				ins->mCode = IC_JUMP;
			else
				ins->mCode = IC_JUMPF;
			ins->mSrc[0].mTemp = -1;
		}
		break;
	case IC_PUSH_FRAME:
	case IC_POP_FRAME:
		FlushFrameAliases();
		break;
	case IC_CALL:
	case IC_CALL_NATIVE:
		FlushCallAliases(tvalue, aliasedLocals, aliasedParams);
		break;

	}
}


InterOperand::InterOperand(void)
	: mTemp(INVALID_TEMPORARY), mType(IT_NONE), mFinal(false), mIntConst(0), mFloatConst(0), mVarIndex(-1), mOperandSize(0), mLinkerObject(nullptr), mMemory(IM_NONE)
{}

bool InterOperand::IsUByte(void) const
{
	return
		mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue >= 0 &&
		mRange.mMaxState == IntegerValueRange::S_BOUND && mRange.mMaxValue < 256;
}

bool InterOperand::IsSByte(void) const
{
	return
		mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue >= -128 &&
		mRange.mMaxState == IntegerValueRange::S_BOUND && mRange.mMaxValue < 128;
}

bool InterOperand::IsUnsigned(void) const
{
	if (mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue >= 0 && mRange.mMaxState == IntegerValueRange::S_BOUND)
	{
		switch (mType)
		{
		case IT_INT8:
			return mRange.mMaxValue < 128;
		case IT_INT16:
			return mRange.mMaxValue < 32768;
		case IT_INT32:
			return true;
		}
	}

	return false;
}


void InterOperand::ForwardMem(const InterOperand& op)
{
	mIntConst = op.mIntConst;
	mFloatConst = op.mFloatConst;
	mVarIndex = op.mVarIndex;
	mOperandSize = op.mOperandSize;
	mLinkerObject = op.mLinkerObject;
	mMemory = op.mMemory;
	mTemp = op.mTemp;
	mType = op.mType;
	mRange = op.mRange;
	mFinal = false;
}

void InterOperand::Forward(const InterOperand& op)
{
	mTemp = op.mTemp;
	mType = op.mType;
	mRange = op.mRange;
	mFinal = false;
}

bool InterOperand::IsEqual(const InterOperand& op) const
{
	if (mType != op.mType || mTemp != op.mTemp)
		return false;

	if (mMemory != op.mMemory)
		return false;

	if (mIntConst != op.mIntConst || mFloatConst != op.mFloatConst)
		return false;

	if (mMemory != IM_NONE && mMemory != IM_INDIRECT)
	{
		if (mVarIndex != op.mVarIndex || mLinkerObject != op.mLinkerObject)
			return false;
	}

	return true;
}

InterInstruction::InterInstruction(void)
{
	mCode = IC_NONE;
	mOperator = IA_NONE;

	mNumOperands = 3;

	mInUse = false;
	mVolatile = false;
	mInvariant = false;
	mSingleAssignment = false;
}

void InterInstruction::SetCode(const Location& loc, InterCode code)
{
	this->mCode = code;
	this->mLocation = loc;
}

static bool TypeInteger(InterType t)
{
	return t == IT_INT8 || t == IT_INT16 || t == IT_INT32 || t == IT_BOOL || t == IT_POINTER;
}

static bool TypeCompatible(InterType t1, InterType t2)
{
	return t1 == t2 || TypeInteger(t1) && TypeInteger(t2);
}

static bool TypeArithmetic(InterType t)
{
	return t == IT_INT8 || t == IT_INT16 || t == IT_INT32 || t == IT_BOOL || t == IT_FLOAT;
}

static void FilterTempUseUsage(NumberSet& requiredTemps, NumberSet& providedTemps, int temp)
{
	if (temp >= 0)
	{
		if (!providedTemps[temp]) requiredTemps += temp;
	}
}

static void FilterTempDefineUsage(NumberSet& requiredTemps, NumberSet& providedTemps, int temp)
{
	if (temp >= 0)
	{
		providedTemps += temp;
	}
}

void InterInstruction::CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable)
{
	if (mCode == IC_CONSTANT)
	{
		if (mDst.mType == IT_POINTER && mConst.mMemory == IM_LOCAL)
			localTable[mDst.mTemp] = mConst.mVarIndex;
		else if (mDst.mType == IT_POINTER && (mConst.mMemory == IM_PARAM || mConst.mMemory == IM_FPARAM))
			paramTable[mDst.mTemp] = mConst.mVarIndex;
	}
	else if (mCode == IC_LEA)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
			localTable[mDst.mTemp] = localTable[mSrc[1].mTemp];
		else if (mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM)
			paramTable[mDst.mTemp] = paramTable[mSrc[1].mTemp];
	}
	else if (mCode == IC_LOAD_TEMPORARY)
	{
		localTable[mDst.mTemp] = localTable[mSrc[0].mTemp];
		paramTable[mDst.mTemp] = paramTable[mSrc[0].mTemp];
	}
}

void InterInstruction::MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams)
{
	if (mCode == IC_STORE && mSrc[0].mTemp >= 0)
	{
		int	l = localTable[mSrc[0].mTemp];
		if (l >= 0)
			aliasedLocals += l;
		l = paramTable[mSrc[0].mTemp];
		if (l >= 0)
			aliasedParams += l;
	}
}

void InterInstruction::FilterTempUsage(NumberSet& requiredTemps, NumberSet& providedTemps)
{
	if (mCode != IC_NONE)
	{
		for (int i = 0; i < mNumOperands; i++)
			FilterTempUseUsage(requiredTemps, providedTemps, mSrc[i].mTemp);
		FilterTempDefineUsage(requiredTemps, providedTemps, mDst.mTemp);
	}
}

void InterInstruction::FilterStaticVarsUsage(const GrowingVariableArray& staticVars, NumberSet& requiredVars, NumberSet& providedVars)
{
	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_INDIRECT)
		{
			for (int i = 0; i < staticVars.Size(); i++)
			{
				if (staticVars[i]->mAliased && !providedVars[i])
					requiredVars += i;
			}
		}
		else if (mSrc[0].mMemory == IM_GLOBAL)
		{
			if (!providedVars[mSrc[0].mVarIndex])
				requiredVars += mSrc[0].mVarIndex;
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_INDIRECT)
		{
			for (int i = 0; i < staticVars.Size(); i++)
			{
				if (staticVars[i]->mAliased && !providedVars[i])
					requiredVars += i;
			}
		}
		else if (mSrc[1].mMemory == IM_GLOBAL)
		{
			if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == staticVars[mSrc[1].mVarIndex]->mSize)
				providedVars += mSrc[1].mVarIndex;
			else if (!providedVars[mSrc[1].mVarIndex])
				requiredVars += mSrc[1].mVarIndex;
		}
	}
	else if (mCode == IC_COPY || mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE || mCode == IC_STRCPY)
	{
		requiredVars.OrNot(providedVars);
	}
}

void InterInstruction::FilterVarsUsage(const GrowingVariableArray& localVars, NumberSet& requiredVars, NumberSet& providedVars, const GrowingVariableArray& params, NumberSet& requiredParams, NumberSet& providedParams, InterMemory paramMemory)
{
	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_LOCAL)
		{
			assert(mSrc[0].mTemp < 0);
			if (!providedVars[mSrc[0].mVarIndex])
				requiredVars += mSrc[0].mVarIndex;
		}
		else if (mSrc[0].mMemory == paramMemory)
		{
			assert(mSrc[0].mTemp < 0);
			if (!providedParams[mSrc[0].mVarIndex])
				requiredParams += mSrc[0].mVarIndex;
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedVars[mSrc[1].mVarIndex] && (mSrc[1].mIntConst != 0 || mSrc[1].mOperandSize != localVars[mSrc[1].mVarIndex]->mSize))
				requiredVars += mSrc[1].mVarIndex;
			providedVars += mSrc[1].mVarIndex;
		}
		else if (mSrc[1].mMemory == paramMemory)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedParams[mSrc[1].mVarIndex] && (mSrc[1].mIntConst != 0 || mSrc[1].mOperandSize != params[mSrc[1].mVarIndex]->mSize))
				requiredParams += mSrc[1].mVarIndex;
			providedParams += mSrc[1].mVarIndex;
		}
	}
	else if (mCode == IC_COPY || mCode == IC_STRCPY)
	{
		if (mSrc[0].mMemory == IM_LOCAL)
		{
			assert(mSrc[0].mTemp < 0);
			if (!providedVars[mSrc[0].mVarIndex])
				requiredVars += mSrc[0].mVarIndex;
		}
		else if (mSrc[0].mMemory == paramMemory)
		{
			assert(mSrc[0].mTemp < 0);
			if (!providedParams[mSrc[0].mVarIndex])
				requiredParams += mSrc[0].mVarIndex;
		}

		if (mSrc[1].mMemory == IM_LOCAL)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedVars[mSrc[1].mVarIndex] && (mSrc[1].mIntConst != 0 || mSrc[1].mOperandSize != localVars[mSrc[1].mVarIndex]->mSize))
				requiredVars += mSrc[1].mVarIndex;
			providedVars += mSrc[1].mVarIndex;
		}
		else if (mSrc[1].mMemory == paramMemory)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedParams[mSrc[1].mVarIndex] && (mSrc[1].mIntConst != 0 || mSrc[1].mOperandSize != params[mSrc[1].mVarIndex]->mSize))
				requiredParams += mSrc[1].mVarIndex;
			providedParams += mSrc[1].mVarIndex;
		}
	}
	else if (mCode == IC_ASSEMBLER)
	{
		for (int i = 1; i < mNumOperands; i++)
		{
			if (mSrc[i].mMemory == IM_LOCAL)
			{
				if (!providedVars[mSrc[i].mVarIndex])
					requiredVars += mSrc[i].mVarIndex;
			}
			else if (mSrc[i].mMemory == paramMemory)
			{
				if (!providedParams[mSrc[i].mVarIndex])
					requiredParams += mSrc[i].mVarIndex;
			}
		}
	}
}

static void PerformTempUseForwarding(int& temp, TempForwardingTable& forwardingTable)
{
	if (temp >= 0)
		temp = forwardingTable[temp];
}

static void PerformTempDefineForwarding(int temp, TempForwardingTable& forwardingTable)
{
	if (temp >= 0)
	{
		forwardingTable.Destroy(temp);
	}
}

bool InterInstruction::PropagateConstTemps(const GrowingInstructionPtrArray& ctemps)
{
	switch (mCode)
	{
	case IC_LOAD:
	case IC_CALL:
	case IC_CALL_NATIVE:
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mSrc[0].mIntConst += ains->mConst.mIntConst;
			mSrc[0].mLinkerObject = ains->mConst.mLinkerObject;
			mSrc[0].mVarIndex = ains->mConst.mVarIndex;
			mSrc[0].mMemory = ains->mConst.mMemory;
			mSrc[0].mTemp = -1;
			return true;
		}
		break;
	case IC_STORE:
		if (mSrc[1].mTemp >= 0 && ctemps[mSrc[1].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[1].mTemp];
			mSrc[1].mIntConst += ains->mConst.mIntConst;
			mSrc[1].mLinkerObject = ains->mConst.mLinkerObject;
			mSrc[1].mVarIndex = ains->mConst.mVarIndex;
			mSrc[1].mMemory = ains->mConst.mMemory;
			mSrc[1].mTemp = -1;
			return true;
		}
		break;
	case IC_LOAD_TEMPORARY:
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mCode = IC_CONSTANT;
			mConst.mIntConst = ains->mConst.mIntConst;
			mConst.mFloatConst = ains->mConst.mFloatConst;
			mConst.mLinkerObject = ains->mConst.mLinkerObject;
			mConst.mVarIndex = ains->mConst.mVarIndex;
			mConst.mMemory = ains->mConst.mMemory;
			mSrc[0].mTemp = -1;
			return true;
		}
		break;
	case IC_BINARY_OPERATOR:
	{
		bool	changed = false;

		for (int i = 0; i < 2; i++)
		{
			if (mSrc[i].mTemp >= 0 && ctemps[mSrc[i].mTemp])
			{
				InterInstruction* ains = ctemps[mSrc[i].mTemp];
				mSrc[i] = ains->mConst;
				mSrc[i].mType = ains->mDst.mType;
				changed = true;
			}
		}

		if (changed)
		{
			this->ConstantFolding();
			return true;
		}
	} break;

	case IC_CONVERSION_OPERATOR:
	case IC_UNARY_OPERATOR:
	{
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mSrc[0] = ains->mConst;
			mSrc[0].mType = ains->mDst.mType;
			this->ConstantFolding();
			return true;
		}

	} break;

	case IC_LEA:
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mSrc[0] = ains->mConst;
			mSrc[0].mType = ains->mDst.mType;

			this->ConstantFolding();
			return true;
		}
		else if (mSrc[0].mTemp < 0 && mSrc[1].mTemp >= 0 && ctemps[mSrc[1].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[1].mTemp];
			mSrc[1] = ains->mConst;
			mSrc[1].mType = IT_POINTER;

			this->ConstantFolding();
			return true;
		}
		break;

	}

	return false;
}

void InterInstruction::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	if (mCode != IC_NONE)
	{
		for (int i = 0; i < mNumOperands; i++)
			PerformTempUseForwarding(mSrc[i].mTemp, forwardingTable);
		PerformTempDefineForwarding(mDst.mTemp, forwardingTable);
	}
	if (mCode == IC_LOAD_TEMPORARY && mDst.mTemp != mSrc[0].mTemp)
	{
		forwardingTable.Build(mDst.mTemp, mSrc[0].mTemp);
	}
}

bool InterInstruction::RemoveUnusedResultInstructions(InterInstruction* pre, NumberSet& requiredTemps)
{
	bool	changed = false;

	if (pre && mCode == IC_LOAD_TEMPORARY && pre->mDst.mTemp == mSrc[0].mTemp && !requiredTemps[mSrc[0].mTemp] && pre->mDst.mTemp >= 0)
	{
		// previous instruction produced result, but it is not needed here
		pre->mDst.mTemp = mDst.mTemp;
		pre->mSingleAssignment = mSingleAssignment;

		mCode = IC_NONE;
		mDst.mTemp = -1;
		for (int i = 0; i < mNumOperands; i++)
			mSrc[i].mTemp = -1;
		mNumOperands = 0;

		changed = true;
	}
	else if (mCode == IC_LOAD_TEMPORARY && mDst.mTemp == mSrc[0].mTemp)
	{
		mCode = IC_NONE;
		mDst.mTemp = -1;
		for (int i = 0; i < mNumOperands; i++)
			mSrc[i].mTemp = -1;
		mNumOperands = 0;

		changed = true;
	}
	else if (mDst.mTemp != -1)
	{
		if (!requiredTemps[mDst.mTemp] && mDst.mTemp >= 0)
		{
			if (mCode == IC_LOAD && mVolatile)
			{
			}
			else if (!HasSideEffect(mCode))
			{
				mCode = IC_NONE;
				mDst.mTemp = -1;
				for (int i = 0; i < mNumOperands; i++)
					mSrc[i].mTemp = -1;

				changed = true;
			}
			else
			{
				mDst.mTemp = -1;

				changed = true;
			}
		}
		else
			requiredTemps -= mDst.mTemp;
	}

	for (int i = 0; i < mNumOperands; i++)
	{
		if (mSrc[i].mTemp >= 0) mSrc[i].mFinal = !requiredTemps[mSrc[i].mTemp] && mSrc[i].mTemp >= 0;
	}

	for (int i = 0; i < mNumOperands; i++)
	{
		if (mSrc[i].mTemp >= 0) requiredTemps += mSrc[i].mTemp;
	}

	return changed;
}

void InterInstruction::BuildCallerSaveTempSet(NumberSet& requiredTemps, NumberSet& callerSaveTemps)
{
	if (mDst.mTemp >= 0)
		requiredTemps -= mDst.mTemp;

	if (mCode == IC_CALL || mCode == IC_CALL_NATIVE)
		callerSaveTemps |= requiredTemps;

	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) requiredTemps += mSrc[i].mTemp;
}

bool InterInstruction::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, NumberSet& requiredVars, const GrowingVariableArray& params, NumberSet& requiredParams, InterMemory paramMemory)
{
	bool	changed = false;

	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_LOCAL)
		{
			requiredVars += mSrc[0].mVarIndex;
		}
		else if (mSrc[0].mMemory == paramMemory)
		{
			requiredParams += mSrc[0].mVarIndex;
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			if (localVars[mSrc[1].mVarIndex]->mAliased)
				;
			else if (requiredVars[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == localVars[mSrc[1].mVarIndex]->mSize)
					requiredVars -= mSrc[1].mVarIndex;
			}
			else
			{
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
		else if (mSrc[1].mMemory == paramMemory)
		{
			if (params[mSrc[1].mVarIndex]->mAliased)
				;
			else if (requiredParams[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == params[mSrc[1].mVarIndex]->mSize)
					requiredParams -= mSrc[1].mVarIndex;
			}
			else
			{
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
	}
	else if (mCode == IC_COPY)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			if (localVars[mSrc[1].mVarIndex]->mAliased)
				;
			else if (requiredVars[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == localVars[mSrc[1].mVarIndex]->mSize)
					requiredVars -= mSrc[1].mVarIndex;
			}
			else
			{
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
		else if (mSrc[1].mMemory == paramMemory)
		{
			if (params[mSrc[1].mVarIndex]->mAliased)
				;
			else if (requiredParams[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == params[mSrc[1].mVarIndex]->mSize)
					requiredParams -= mSrc[1].mVarIndex;
			}
			else
			{
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}

		if (mSrc[0].mMemory == IM_LOCAL)
		{
			requiredVars += mSrc[0].mVarIndex;
		}
		else if (mSrc[0].mMemory == paramMemory)
		{
			requiredParams += mSrc[0].mVarIndex;
		}
	}

	return changed;
}

bool InterInstruction::RemoveUnusedStaticStoreInstructions(const GrowingVariableArray& staticVars, NumberSet& requiredVars)
{
	bool	changed = false;

	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_INDIRECT)
		{
			for (int i = 0; i < staticVars.Size(); i++)
			{
				if (staticVars[i]->mAliased)
					requiredVars += i;
			}
		}
		else if (mSrc[0].mMemory == IM_GLOBAL)
		{
			requiredVars += mSrc[0].mVarIndex;
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_GLOBAL)
		{
			if (requiredVars[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == staticVars[mSrc[1].mVarIndex]->mSize)
					requiredVars -= mSrc[1].mVarIndex;
			}
			else if (!mVolatile)
			{
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
	}
	else if (mCode == IC_COPY || mCode == IC_STRCPY)
	{
		requiredVars.Fill();
	}
	else if (mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE)
	{
		requiredVars.Fill();
	}

	return changed;
}

bool InterInstruction::UsesTemp(int temp) const
{
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp == temp)
			return true;
	return false;
}

static void DestroySourceValues(int temp, GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
{
	int i, j;
	const	InterInstruction* ins;

	if (temp >= 0)
	{
		i = 0;
		while (i < tvalid.Num())
		{
			j = tvalid.Element(i);

			ins = tvalue[j];

			if (ins->UsesTemp(temp))
			{
				tvalue[j] = NULL;
				tvalid -= j;
			}
			else
				i++;
		}
	}
}

void InterInstruction::PerformValueForwarding(GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
{
	DestroySourceValues(mDst.mTemp, tvalue, tvalid);

	if (mCode == IC_LOAD_TEMPORARY)
	{
		if (tvalue[mSrc[0].mTemp])
		{
			tvalue[mDst.mTemp] = tvalue[mSrc[0].mTemp];
			tvalid += mDst.mTemp;
		}
	}
	else
	{
		if (mDst.mTemp >= 0)
		{
			tvalue[mDst.mTemp] = this;
			tvalid += mDst.mTemp;
		}
	}
}

void InterInstruction::LocalRenameRegister(GrowingIntArray& renameTable, int& num)
{
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) mSrc[i].mTemp = renameTable[mSrc[i].mTemp];

	if (mDst.mTemp >= 0)
	{
		renameTable[mDst.mTemp] = num;
		mDst.mTemp = num++;
	}
#if 0
	if (mCode == IC_LOAD_TEMPORARY && mSrc[0].mTemp < 0)
		mCode = IC_CONSTANT;
#endif
}

void InterInstruction::GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) mSrc[i].mTemp = renameTable[mSrc[i].mTemp];

	if (mDst.mTemp >= 0)
	{
		mDst.mTemp = renameTable[mDst.mTemp];
		if (InterTypeSize[mDst.mType] > InterTypeSize[temporaries[mDst.mTemp]])
			temporaries[mDst.mTemp] = mDst.mType;
	}
#if 0
	if (mCode == IC_LOAD_TEMPORARY && mSrc[0].mTemp < 0)
		mCode = IC_CONSTANT;
#endif
}

static void UpdateCollisionSet(NumberSet& liveTemps, NumberSet* collisionSets, int temp)
{
	int i;

	if (temp >= 0 && !liveTemps[temp])
	{
		for (i = 0; i < liveTemps.Size(); i++)
		{
			if (liveTemps[i])
			{
				collisionSets[i] += temp;
				collisionSets[temp] += i;
			}
		}

		liveTemps += temp;
	}
}

void InterInstruction::BuildCollisionTable(NumberSet& liveTemps, NumberSet* collisionSets)
{
	if (mDst.mTemp >= 0)
	{
		//		if (!liveTemps[ttemp]) __asm int 3
		liveTemps -= mDst.mTemp;
	}

	for (int i = 0; i < mNumOperands; i++)
		UpdateCollisionSet(liveTemps, collisionSets, mSrc[i].mTemp);
}

void InterInstruction::ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) mSrc[i].mTemp = renameTable[mSrc[i].mTemp];

	if (mDst.mTemp >= 0)
	{
		mDst.mTemp = renameTable[mDst.mTemp];
		temporaries[mDst.mTemp] = mDst.mType;
	}
}


void InterInstruction::CollectActiveTemporaries(FastNumberSet& set)
{
	if (mDst.mTemp >= 0) set += mDst.mTemp;
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) set += mSrc[i].mTemp;
}

void InterInstruction::ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries)
{
	if (mDst.mTemp >= 0)
	{
		mDst.mTemp = set.Index(mDst.mTemp);
		temporaries[mDst.mTemp] = mDst.mType;
	}
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0) mSrc[i].mTemp = set.Index(mSrc[i].mTemp);
}

void InterInstruction::CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes)
{
	switch (mCode)
	{
	case IC_LOAD:
		if (mSrc[0].mMemory == IM_LOCAL && mSrc[0].mTemp < 0)
		{
			if ((localTypes[mSrc[0].mVarIndex] == IT_NONE || localTypes[mSrc[0].mVarIndex] == mDst.mType) && mSrc[0].mIntConst == 0)
			{
				localTypes[mSrc[0].mVarIndex] = mDst.mType;
				simpleLocals += mSrc[0].mVarIndex;
			}
			else
				complexLocals += mSrc[0].mVarIndex;
		}
		else if ((mSrc[0].mMemory == IM_PARAM || mSrc[0].mMemory == IM_FPARAM) && mSrc[0].mTemp < 0)
		{
			if ((paramTypes[mSrc[0].mVarIndex] == IT_NONE || paramTypes[mSrc[0].mVarIndex] == mDst.mType) && mSrc[0].mIntConst == 0)
			{
				paramTypes[mSrc[0].mVarIndex] = mDst.mType;
				simpleParams += mSrc[0].mVarIndex;
			}
			else
				complexParams += mSrc[0].mVarIndex;
		}
		break;
	case IC_STORE:
		if (mSrc[1].mMemory == IM_LOCAL && mSrc[1].mTemp < 0)
		{
			if ((localTypes[mSrc[1].mVarIndex] == IT_NONE || localTypes[mSrc[1].mVarIndex] == mSrc[0].mType) && mSrc[1].mIntConst == 0)
			{
				localTypes[mSrc[1].mVarIndex] = mSrc[0].mType;
				simpleLocals += mSrc[1].mVarIndex;
			}
			else
				complexLocals += mSrc[1].mVarIndex;
		}
		else if ((mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM) && mSrc[1].mTemp < 0)
		{
			if ((paramTypes[mSrc[1].mVarIndex] == IT_NONE || paramTypes[mSrc[1].mVarIndex] == mSrc[0].mType) && mSrc[1].mIntConst == 0)
			{
				paramTypes[mSrc[1].mVarIndex] = mSrc[0].mType;
				simpleParams += mSrc[1].mVarIndex;
			}
			else
				complexParams += mSrc[1].mVarIndex;
		}
		break;
	case IC_LEA:
		if (mSrc[1].mMemory == IM_LOCAL && mSrc[1].mTemp < 0)
			complexLocals += mSrc[1].mVarIndex;
		else if ((mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM) && mSrc[1].mTemp < 0)
			complexParams += mSrc[1].mVarIndex;
		break;
	case IC_COPY:
		if (mSrc[1].mMemory == IM_LOCAL && mSrc[1].mTemp < 0)
			complexLocals += mSrc[1].mVarIndex;
		else if ((mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM) && mSrc[1].mTemp < 0)
			complexParams += mSrc[1].mVarIndex;
		if (mSrc[0].mMemory == IM_LOCAL && mSrc[0].mTemp < 0)
			complexLocals += mSrc[0].mVarIndex;
		else if ((mSrc[0].mMemory == IM_PARAM || mSrc[0].mMemory == IM_FPARAM) && mSrc[0].mTemp < 0)
			complexParams += mSrc[0].mVarIndex;
		break;
	case IC_CONSTANT:
		if (mDst.mType == IT_POINTER && mConst.mMemory == IM_LOCAL)
			complexLocals += mConst.mVarIndex;
		else if (mDst.mType == IT_POINTER && (mConst.mMemory == IM_PARAM || mConst.mMemory == IM_FPARAM))
			complexParams += mConst.mVarIndex;
		break;
	}
}

void InterInstruction::SimpleLocalToTemp(int vindex, int temp)
{
	switch (mCode)
	{
	case IC_LOAD:
		if (mSrc[0].mMemory == IM_LOCAL && mSrc[0].mTemp < 0 && vindex == mSrc[0].mVarIndex)
		{
			mCode = IC_LOAD_TEMPORARY;
			mSrc[0].mTemp = temp;
			mSrc[0].mType = mDst.mType;
			mNumOperands = 1;

			assert(mSrc[0].mTemp >= 0);

		}
		break;
	case IC_STORE:
		if (mSrc[1].mMemory == IM_LOCAL && mSrc[1].mTemp < 0 && vindex == mSrc[1].mVarIndex)
		{
			if (mSrc[0].mTemp < 0)
			{
				mCode = IC_CONSTANT;
				mConst.mIntConst = mSrc[0].mIntConst;
				mConst.mFloatConst = mSrc[0].mFloatConst;
			}
			else
			{
				mCode = IC_LOAD_TEMPORARY;
				mNumOperands = 1;
				assert(mSrc[0].mTemp >= 0);
			}

			mDst.mTemp = temp;
			mDst.mType = mSrc[0].mType;
		}
		break;
	}
}

bool InterInstruction::ConstantFolding(void)
{
	switch (mCode)
	{
	case IC_RELATIONAL_OPERATOR:
		if (mSrc[0].mTemp < 0 && mSrc[1].mTemp < 0)
		{
			mCode = IC_CONSTANT;
			if (IsIntegerType(mSrc[0].mType))
				mConst.mIntConst = ::ConstantFolding(mOperator, mSrc[0].mType, mSrc[1].mIntConst, mSrc[0].mIntConst);
			else
				mConst.mIntConst = ConstantRelationalFolding(mOperator, mSrc[1].mFloatConst, mSrc[0].mFloatConst);
			mConst.mType = IT_BOOL;
			mNumOperands = 0;
			return true;
		}
		else if (mSrc[0].mTemp == mSrc[1].mTemp)
		{
			mCode = IC_CONSTANT;
			mConst.mIntConst = ::ConstantFolding(mOperator, mSrc[0].mType, 0, 0);
			mConst.mType = IT_BOOL;
			mSrc[0].mTemp = -1;
			mSrc[1].mTemp = -1;
			mNumOperands = 0;
			return true;
		}
		break;
	case IC_BINARY_OPERATOR:
		if (mSrc[0].mTemp < 0 && mSrc[1].mTemp < 0)
		{
			mCode = IC_CONSTANT;
			if (mDst.mType == IT_FLOAT)
				mConst.mFloatConst = ::ConstantFolding(mOperator, mSrc[1].mFloatConst, mSrc[0].mFloatConst);
			else
				mConst.mIntConst = ::ConstantFolding(mOperator, mDst.mType, mSrc[1].mIntConst, mSrc[0].mIntConst);
			mNumOperands = 0;
			return true;
		}
#if 1
		else if (mSrc[0].mTemp < 0)
		{
			if (mDst.mType == IT_FLOAT)
			{

			}
			else if (IsIntegerType(mDst.mType))
			{
				if ((mOperator == IA_ADD || mOperator == IA_SUB || mOperator == IA_OR || mOperator == IA_XOR || mOperator == IA_SHL || mOperator == IA_SHR || mOperator == IA_SAR) && mSrc[0].mIntConst == 0 ||
					(mOperator == IA_MUL || mOperator == IA_DIVS || mOperator == IA_DIVU) && mSrc[0].mIntConst == 1)
				{
					mCode = IC_LOAD_TEMPORARY;
					mSrc[0] = mSrc[1];
					mSrc[1].mTemp = -1;
					mNumOperands = 1;
					assert(mSrc[0].mTemp >= 0);
					return true;
				}
				else if (mOperator == IA_MODU && (mSrc[0].mIntConst & (mSrc[0].mIntConst - 1)) == 0)
				{
					mOperator = IA_AND;
					mSrc[0].mIntConst--;
					return true;
				}
				else if (mOperator == IA_DIVU && (mSrc[0].mIntConst & (mSrc[0].mIntConst - 1)) == 0)
				{
					int	n = 0;
					while (mSrc[0].mIntConst > 1)
					{
						n++;
						mSrc[0].mIntConst >>= 1;
					}
					mOperator = IA_SHR;
					mSrc[0].mIntConst = n;
					return true;
				}
			}
		}
		else if (mSrc[1].mTemp < 0)
		{
			if (mDst.mType == IT_FLOAT)
			{

			}
			else if (IsIntegerType(mDst.mType))
			{
				if ((mOperator == IA_ADD || mOperator == IA_OR || mOperator == IA_XOR) && mSrc[1].mIntConst == 0 || (mOperator == IA_MUL) && mSrc[1].mIntConst == 1)
				{
					mCode = IC_LOAD_TEMPORARY;
					mSrc[1].mTemp = -1;
					mNumOperands = 1;
					assert(mSrc[0].mTemp >= 0);
					return true;
				}
				else if ((mOperator == IA_AND || mOperator == IA_MUL || mOperator == IA_SHL || mOperator == IA_SHR || mOperator == IA_SAR) && mSrc[1].mIntConst == 0)
				{
					mCode = IC_CONSTANT;
					mConst.mIntConst = 0;
					mNumOperands = 0;
					return true;
				}
			}
		}
#endif
		break;
	case IC_UNARY_OPERATOR:
		if (mSrc[0].mTemp < 0)
		{
			mCode = IC_CONSTANT;
			if (mDst.mType == IT_FLOAT)
				mConst.mFloatConst = ::ConstantFolding(mOperator, mSrc[0].mFloatConst);
			else
				mConst.mIntConst = ::ConstantFolding(mOperator, mSrc[0].mIntConst);
			mNumOperands = 0;
			return true;
		}
		break;
	case IC_CONVERSION_OPERATOR:
		if (mSrc[0].mTemp < 0)
		{
			ConversionConstantFold(this, mSrc[0]);
			return true;
		}
		break;
	case IC_LOAD_TEMPORARY:
		if (mDst.mTemp == mSrc[0].mTemp)
		{
			mCode = IC_NONE;
			mDst.mTemp = -1;
			for (int i = 0; i < mNumOperands; i++)
				mSrc[i].mTemp = -1;
			return true;
		}
		break;
	case IC_LEA:
		if (mSrc[0].mTemp < 0 && mSrc[1].mTemp < 0)
		{
			mCode = IC_CONSTANT;
			mConst = mSrc[1];
			mConst.mIntConst += mSrc[0].mIntConst;
			mConst.mRange.Reset();
			mNumOperands = 0;
			return true;
		}
		else if (mSrc[0].mTemp < 0 && mSrc[0].mIntConst == 0 && mSrc[1].mIntConst == 0)
		{
			mCode = IC_LOAD_TEMPORARY;
			mSrc[0] = mSrc[1];
			mSrc[1].mTemp = -1;
			mNumOperands = 1;
			assert(mSrc[0].mTemp >= 0);
			return true;
		}
		break;
	}

	return false;
}


void InterOperand::Disassemble(FILE* file)
{
	static char typechars[] = "NBCILFP";

	if (mTemp >= 0) 
	{
		if (mFinal)
			fprintf(file, "R%d(%cF)", mTemp, typechars[mType]);
		else
			fprintf(file, "R%d(%c)", mTemp, typechars[mType]);

		if (mType == IT_POINTER && mMemory == IM_INDIRECT)
			fprintf(file, "+%d", int(mIntConst));

		if (mRange.mMinState >= IntegerValueRange::S_WEAK || mRange.mMaxState >= IntegerValueRange::S_WEAK)
		{
			fprintf(file, "[");
			if (mRange.mMinState == IntegerValueRange::S_WEAK)
				fprintf(file, "~%d", int(mRange.mMinValue));
			else if (mRange.mMinState == IntegerValueRange::S_BOUND)
				fprintf(file, "%d", int(mRange.mMinValue));
			else if (mRange.mMinState == IntegerValueRange::S_UNKNOWN)
				fprintf(file, "?");
			fprintf(file, "..");
			if (mRange.mMaxState == IntegerValueRange::S_WEAK)
				fprintf(file, "~%d", int(mRange.mMaxValue));
			else if (mRange.mMaxState == IntegerValueRange::S_BOUND)
				fprintf(file, "%d", int(mRange.mMaxValue));
			else if (mRange.mMaxState == IntegerValueRange::S_UNKNOWN)
				fprintf(file, "?");
			fprintf(file, "]");
		}
	}
	else if (mType == IT_POINTER)
	{
		fprintf(file, "V%d+%d", mVarIndex, int(mIntConst));
	}
	else if (IsIntegerType(mType) || mType == IT_BOOL)
	{
		fprintf(file, "C%c:%d", typechars[mType], int(mIntConst));
	}
	else if (mType == IT_FLOAT)
	{
		fprintf(file, "C%c:%f", typechars[mType], mFloatConst);
	}
}

void InterInstruction::Disassemble(FILE* file)
{
	if (this->mCode != IC_NONE)
	{
		static char memchars[] = "NPLGFPITAZZ";

		fprintf(file, "\t");
		switch (this->mCode)
		{
		case IC_LOAD_TEMPORARY:
		case IC_STORE_TEMPORARY:
			fprintf(file, "MOVE");
			break;
		case IC_BINARY_OPERATOR:
			fprintf(file, "BINOP%d", mOperator);
			break;
		case IC_UNARY_OPERATOR:
			fprintf(file, "UNOP%d", mOperator);
			break;
		case IC_RELATIONAL_OPERATOR:
			fprintf(file, "RELOP%d", mOperator);
			break;
		case IC_CONVERSION_OPERATOR:
			fprintf(file, "CONV%d", mOperator);
			break;
		case IC_STORE:
			fprintf(file, "STORE%c%d", memchars[mSrc[1].mMemory], mSrc[1].mOperandSize);
			break;
		case IC_LOAD:
			fprintf(file, "LOAD%c%d", memchars[mSrc[0].mMemory], mSrc[0].mOperandSize);
			break;
		case IC_COPY:
			fprintf(file, "COPY%c%c", memchars[mSrc[0].mMemory], memchars[mSrc[1].mMemory]);
			break;
		case IC_STRCPY:
			fprintf(file, "STRCPY%c%c", memchars[mSrc[0].mMemory], memchars[mSrc[1].mMemory]);
			break;
		case IC_LEA:
			fprintf(file, "LEA%c", memchars[mSrc[1].mMemory]);
			break;
		case IC_TYPECAST:
			fprintf(file, "CAST");
			break;
		case IC_SELECT:
			fprintf(file, "SELECT");
			break;
		case IC_CONSTANT:
			fprintf(file, "CONST");
			break;
		case IC_BRANCH:
			fprintf(file, "BRANCH");
			break;
		case IC_JUMP:
			fprintf(file, "JUMP");
			break;
		case IC_JUMPF:
			fprintf(file, "JUMPF");
			break;
		case IC_PUSH_FRAME:
			fprintf(file, "PUSHF\t%d", int(mConst.mIntConst));
			break;
		case IC_POP_FRAME:
			fprintf(file, "POPF\t%d", int(mConst.mIntConst));
			break;
		case IC_CALL:
			fprintf(file, "CALL");
			break;
		case IC_CALL_NATIVE:
			fprintf(file, "CALLN");
			break;
		case IC_ASSEMBLER:
			fprintf(file, "JSR");
			break;
		case IC_RETURN_VALUE:
			fprintf(file, "RETV");
			break;
		case IC_RETURN_STRUCT:
			fprintf(file, "RETS");
			break;
		case IC_RETURN:
			fprintf(file, "RET");
			break;
		case IC_UNREACHABLE:
			fprintf(file, "UNREACHABLE");
			break;
		}
		static char typechars[] = "NBCILFP";

		fprintf(file, "\t");
		if (mDst.mTemp >= 0)
			mDst.Disassemble(file);
		fprintf(file, "\t<-\t");


		if (this->mCode == IC_CONSTANT)
		{
			if (mDst.mType == IT_POINTER)
			{
				fprintf(file, "C%c%d(%d:%d)", memchars[mConst.mMemory], mConst.mOperandSize, mConst.mVarIndex, int(mConst.mIntConst));
			}
			else if (mDst.mType == IT_FLOAT)
				fprintf(file, "CF:%f", mConst.mFloatConst);
			else
			{
#ifdef _WIN32
				fprintf(file, "CI:%I64d", mConst.mIntConst);
#else
				fprintf(file, "CI:%lld", mConst.mIntConst);
#endif
			}
		}
		else
		{
			bool	first = true;
			for (int i = 0; i < mNumOperands; i++)
			{
				int j = mNumOperands - i - 1;
				if (!first)
					fprintf(file, ", ");
				if (mSrc[j].mType != IT_NONE)
				{
					mSrc[j].Disassemble(file);
					first = false;
				}
			}
		}

		fprintf(file, "\t{");
		if (mInvariant)
			fprintf(file, "I");
		if (mVolatile)
			fprintf(file, "V");
		if (mSingleAssignment)
			fprintf(file, "S");
		fprintf(file, "}\n");
	}
}

InterCodeBasicBlock::InterCodeBasicBlock(void)
	: mInstructions(nullptr), mEntryRenameTable(-1), mExitRenameTable(-1), mMergeTValues(nullptr), mMergeAValues(nullptr), mTrueJump(nullptr), mFalseJump(nullptr), mLoopPrefix(nullptr), mDominator(nullptr),
	mEntryValueRange(IntegerValueRange()), mTrueValueRange(IntegerValueRange()), mFalseValueRange(IntegerValueRange()), mLocalValueRange(IntegerValueRange()), 
	mReverseValueRange(IntegerValueRange()), mEntryBlocks(nullptr), mLoadStoreInstructions(nullptr), mLoopPathBlocks(nullptr), mMemoryValueSize(0), mEntryMemoryValueSize(0)
{
	mVisited = false;
	mInPath = false;
	mLoopHead = false;
	mChecked = false;
	mTraceIndex = -1;
	mUnreachable = false;
}

InterCodeBasicBlock::~InterCodeBasicBlock(void)
{
}


void InterCodeBasicBlock::Append(InterInstruction * code)
{
	if (code->mCode == IC_BINARY_OPERATOR)
	{
		assert(code->mSrc[1].mType != IT_POINTER);
	}

	assert(!(code->mInUse));
	code->mInUse = true;
	this->mInstructions.Push(code);
}

void InterCodeBasicBlock::Close(InterCodeBasicBlock* trueJump, InterCodeBasicBlock* falseJump)
{
	this->mTrueJump = trueJump;
	this->mFalseJump = falseJump;
	this->mNumEntries = 0;
}


void InterCodeBasicBlock::CollectEntryBlocks(InterCodeBasicBlock* from)
{
	if (from)
	{
		int i = 0;
		while (i < mEntryBlocks.Size() && mEntryBlocks[i] != from)
			i++;
		if (i == mEntryBlocks.Size())
			mEntryBlocks.Push(from);
	}

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump) mTrueJump->CollectEntryBlocks(this);
		if (mFalseJump) mFalseJump->CollectEntryBlocks(this);
	}
}

void InterCodeBasicBlock::BuildDominatorTree(InterCodeBasicBlock* from)
{
	if (from == this)
		return;
	else if (!mDominator)
	{
		assert(!from || mIndex != 0);
		mDominator = from;
	}
	else if (from == mDominator)
		return; 
	else
	{
		assert(mIndex != 0);

		GrowingInterCodeBasicBlockPtrArray	d1(nullptr), d2(nullptr);

		InterCodeBasicBlock* b = mDominator;
		while (b)
		{
			d1.Push(b);
			b = b->mDominator;
		}
		b = from;
		while (b)
		{
			d2.Push(b);
			b = b->mDominator;
		}

		b = nullptr;
		while (d1.Size() > 0 && d2.Size() > 0 && d1.Last() == d2.Last())
		{
			b = d1.Pop(); d2.Pop();
		}

		if (mDominator == b)
			return;

		mDominator = b;
	}

	if (mTrueJump)
		mTrueJump->BuildDominatorTree(this);
	if (mFalseJump)
		mFalseJump->BuildDominatorTree(this);
}

void InterCodeBasicBlock::CollectEntries(void)
{
	mNumEntries++;
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump) mTrueJump->CollectEntries();
		if (mFalseJump) mFalseJump->CollectEntries();
	}
}

static bool IsInfiniteLoop(InterCodeBasicBlock* head, InterCodeBasicBlock* block)
{
	if (!block->mChecked)
	{
		if (block->mTrueJump && !block->mFalseJump)
		{
			if (block->mTrueJump == head)
				return true;

			block->mChecked = true;
			bool loop = IsInfiniteLoop(head, block->mTrueJump);
			block->mChecked = false;

			return loop;
		}
	}
	
	return false;
}

void InterCodeBasicBlock::GenerateTraces(bool expand, bool compact)
{
	int i;

	if (mInPath)
		mLoopHead = true;

	if (!mVisited)
	{
		mVisited = true;
		mInPath = true;

		for (;;)
		{
			if (mTrueJump && mTrueJump->mInstructions.Size() == 1 && mTrueJump->mInstructions[0]->mCode == IC_JUMP && !mTrueJump->mLoopHead && mTrueJump->mTraceIndex != mIndex)
			{
				mTrueJump->mTraceIndex = mIndex;
				mTrueJump->mNumEntries--;
				mTrueJump = mTrueJump->mTrueJump;
				if (mTrueJump)
					mTrueJump->mNumEntries++;
			}
			else if (mFalseJump && mFalseJump->mInstructions.Size() == 1 && mFalseJump->mInstructions[0]->mCode == IC_JUMP && !mFalseJump->mLoopHead && mFalseJump->mTraceIndex != mIndex)
			{
				mFalseJump->mTraceIndex = mIndex;
				mFalseJump->mNumEntries--;
				mFalseJump = mFalseJump->mTrueJump;
				if (mFalseJump)
					mFalseJump->mNumEntries++;
			}
			else if (mTrueJump && !mFalseJump && ((expand && mTrueJump->mInstructions.Size() < 10 && mTrueJump->mInstructions.Size() > 1 && !mLoopHead) || mTrueJump->mNumEntries == 1) && !mTrueJump->mLoopHead && !IsInfiniteLoop(mTrueJump, mTrueJump))
			{
				mTrueJump->mNumEntries--;
				int	n = mTrueJump->mNumEntries;

				mInstructions.Pop();
				for (i = 0; i < mTrueJump->mInstructions.Size(); i++)
					mInstructions.Push(new InterInstruction(* (mTrueJump->mInstructions[i]) ));

				mFalseJump = mTrueJump->mFalseJump;
				mTrueJump = mTrueJump->mTrueJump;

				if (n > 0)
				{
					if (mTrueJump)
						mTrueJump->mNumEntries++;
					if (mFalseJump)
						mFalseJump->mNumEntries++;
				}
			}
#if 1
			else if (compact && mTrueJump && !mFalseJump && mTrueJump->mInstructions.Size() == 1 && mTrueJump->mInstructions[0]->mCode == IC_BRANCH && mTrueJump->mFalseJump)
			{
				InterCodeBasicBlock* tj = mTrueJump;

				int	ns = mInstructions.Size();

				tj->mNumEntries--;
				tj->mTrueJump->mNumEntries++;
				tj->mFalseJump->mNumEntries++;
				
				mInstructions[ns - 1]->mCode = IC_BRANCH;
				mInstructions[ns - 1]->mOperator = tj->mInstructions[0]->mOperator;
				mInstructions[ns - 1]->mSrc[0].Forward(tj->mInstructions[0]->mSrc[0]);
				mInstructions[ns - 1]->mNumOperands = 1;
				
				mTrueJump = tj->mTrueJump;
				mFalseJump = tj->mFalseJump;
			}
#endif
			else
				break;
		}

		if (mTrueJump) mTrueJump->GenerateTraces(expand, compact);
		if (mFalseJump) mFalseJump->GenerateTraces(expand, compact);

		mInPath = false;
	}
}

static bool IsSimpleAddressMultiply(int val)
{
	switch (val)
	{
	case 1:		//	SHR	3
	case 2:		// SHR	2
	case 4:		// SHR	1
	case 8:
	case 16:		// * 2
	case 32:		// * 4
	case 64:		// * 8
	case 128:	// LEA	r * 2, * 8
	case 192:	// LEA	r, r * 2, * 8
	case 256:	// LEA	r * 4, * 8
	case 512:	// LEA	r * 8, * 8
		return true;
	}

	return false;
}

static void OptimizeAddress(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, int offset)
{
	ins->mSrc[offset].mIntConst = 0;

	while (ins->mSrc[offset].mTemp >= 0 && tvalue[ins->mSrc[offset].mTemp])
	{
		InterInstruction* ains = tvalue[ins->mSrc[offset].mTemp];

		if (ains->mCode == IC_CONSTANT)
		{
			ins->mSrc[offset].mIntConst += ains->mConst.mIntConst;
			ins->mSrc[offset].mLinkerObject = ains->mConst.mLinkerObject;
			ins->mSrc[offset].mVarIndex = ains->mConst.mVarIndex;
			ins->mSrc[offset].mMemory = ains->mConst.mMemory;
			ins->mSrc[offset].mTemp = -1;
		}
		else if (ains->mCode == IC_LEA && ains->mSrc[0].mTemp < 0 && ains->mSrc[1].mTemp >= 0 && tvalue[ains->mSrc[1].mTemp] && ains->mSrc[0].mIntConst >= 0)
		{
			ins->mSrc[offset].mIntConst += ains->mSrc[0].mIntConst;
			ins->mSrc[offset].mTemp = ains->mSrc[1].mTemp;
		}
		else if (ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_ADD && ains->mSrc[0].mTemp < 0 && ains->mSrc[1].mTemp >= 0 && tvalue[ains->mSrc[1].mTemp] && ains->mSrc[0].mIntConst >= 0)
		{
			assert(false);
			ins->mSrc[offset].mIntConst = ains->mSrc[0].mIntConst;
			ins->mSrc[offset].mTemp = ains->mSrc[1].mTemp;
		}
		else if (ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_ADD && ains->mSrc[1].mTemp < 0 && ains->mSrc[0].mTemp >= 0 && tvalue[ains->mSrc[0].mTemp] && ains->mSrc[1].mIntConst >= 0)
		{
			assert(false);
			ins->mSrc[offset].mIntConst = ains->mSrc[1].mIntConst;
			ins->mSrc[offset].mTemp = ains->mSrc[0].mTemp;
		}
		else
			break;
	}
}


void InterCodeBasicBlock::CheckValueUsage(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const GrowingVariableArray& staticVars)
{
	switch (ins->mCode)
	{
	case IC_CALL:
	case IC_CALL_NATIVE:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ins->mSrc[0].mMemory = tvalue[ins->mSrc[0].mTemp]->mConst.mMemory;
			ins->mSrc[0].mLinkerObject = tvalue[ins->mSrc[0].mTemp]->mConst.mLinkerObject;
			ins->mSrc[0].mVarIndex = tvalue[ins->mSrc[0].mTemp]->mConst.mVarIndex;
			ins->mSrc[0].mOperandSize = tvalue[ins->mSrc[0].mTemp]->mConst.mOperandSize;
			ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
			ins->mSrc[0].mTemp = -1;
		}

		break;
	case IC_ASSEMBLER:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ins->mSrc[0].mMemory = tvalue[ins->mSrc[0].mTemp]->mConst.mMemory;
			ins->mSrc[0].mLinkerObject = tvalue[ins->mSrc[0].mTemp]->mConst.mLinkerObject;
			ins->mSrc[0].mVarIndex = tvalue[ins->mSrc[0].mTemp]->mConst.mVarIndex;
			ins->mSrc[0].mOperandSize = tvalue[ins->mSrc[0].mTemp]->mConst.mOperandSize;
			ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
			ins->mSrc[0].mTemp = -1;
		}
		for (int i = 1; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp >= 0 && tvalue[ins->mSrc[i].mTemp])
			{
				InterInstruction* lins = tvalue[ins->mSrc[i].mTemp];
				if (lins->mCode == IC_LOAD && lins->mSrc[0].mTemp < 0 && lins->mSrc[0].mMemory == IM_FPARAM)
				{
					ins->mSrc[i].mType = IT_POINTER;
					ins->mSrc[i].mMemory = IM_FPARAM;
					ins->mSrc[i].mVarIndex = lins->mSrc[0].mVarIndex;
					ins->mSrc[i].mIntConst = lins->mSrc[0].mIntConst;
					ins->mSrc[i].mOperandSize = lins->mSrc[0].mOperandSize;
					ins->mSrc[i].mTemp = -1;
				}
			}
		}

		break;
	case IC_LOAD_TEMPORARY:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			switch (ins->mSrc[0].mType)
			{
			case IT_FLOAT:
				ins->mCode = IC_CONSTANT;
				ins->mConst.mFloatConst = tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst;
				ins->mSrc[0].mTemp = -1;
				break;
			case IT_POINTER:
				ins->mCode = IC_CONSTANT;
				ins->mConst.mMemory = tvalue[ins->mSrc[0].mTemp]->mConst.mMemory;
				ins->mConst.mLinkerObject = tvalue[ins->mSrc[0].mTemp]->mConst.mLinkerObject;
				ins->mConst.mVarIndex = tvalue[ins->mSrc[0].mTemp]->mConst.mVarIndex;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mConst.mOperandSize = tvalue[ins->mSrc[0].mTemp]->mConst.mOperandSize;
				ins->mSrc[0].mTemp = -1;
				break;
			default:
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
				break;
			}
		}
		break;

	case IC_LOAD:
		OptimizeAddress(ins, tvalue, 0);

		if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL && (ins->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
			LoadConstantFold(ins, nullptr, staticVars);

		break;
	case IC_STORE:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			switch (ins->mSrc[0].mType)
			{
			case IT_FLOAT:
				ins->mSrc[0].mFloatConst = tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst;
				ins->mSrc[0].mTemp = -1;
				break;
			case IT_POINTER:
				break;
			default:
				ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
				break;
			}
		}
		OptimizeAddress(ins, tvalue, 1);
		break;
	case IC_COPY:
		OptimizeAddress(ins, tvalue, 0);
		OptimizeAddress(ins, tvalue, 1);
		break;
	case IC_LEA:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mDst.mType = IT_POINTER;
				ins->mConst.mMemory = tvalue[ins->mSrc[1].mTemp]->mConst.mMemory;
				ins->mConst.mLinkerObject = tvalue[ins->mSrc[1].mTemp]->mConst.mLinkerObject;
				ins->mConst.mVarIndex = tvalue[ins->mSrc[1].mTemp]->mConst.mVarIndex;
				ins->mConst.mIntConst = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst + tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mConst.mOperandSize = tvalue[ins->mSrc[1].mTemp]->mConst.mOperandSize;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
			}
			else if (tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == 0)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSrc[0].mType = ins->mSrc[1].mType;
				ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 1;
				assert(ins->mSrc[0].mTemp >= 0);
			}
			else
			{
				ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
			}
		}
		else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT && (tvalue[ins->mSrc[1].mTemp]->mConst.mMemory == IM_GLOBAL || tvalue[ins->mSrc[1].mTemp]->mConst.mMemory == IM_ABSOLUTE))
		{
			ins->mSrc[1].mMemory = tvalue[ins->mSrc[1].mTemp]->mConst.mMemory;
			ins->mSrc[1].mLinkerObject = tvalue[ins->mSrc[1].mTemp]->mConst.mLinkerObject;
			ins->mSrc[1].mVarIndex = tvalue[ins->mSrc[1].mTemp]->mConst.mVarIndex;
			ins->mSrc[1].mIntConst = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
			ins->mSrc[1].mOperandSize = tvalue[ins->mSrc[1].mTemp]->mConst.mOperandSize;
			ins->mSrc[1].mTemp = -1;

			while (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_BINARY_OPERATOR)
			{
				InterInstruction* iins = tvalue[ins->mSrc[0].mTemp];
				if (iins->mOperator == IA_ADD)
				{
					if (iins->mSrc[0].mTemp >= 0 && iins->mSrc[1].mTemp < 0)
					{
						ins->mSrc[0].mTemp = iins->mSrc[0].mTemp;
						ins->mSrc[1].mIntConst += iins->mSrc[1].mIntConst;
					}
					else if (iins->mSrc[0].mTemp < 0 && iins->mSrc[1].mTemp >= 0)
					{
						ins->mSrc[0].mTemp = iins->mSrc[1].mTemp;
						ins->mSrc[1].mIntConst += iins->mSrc[0].mIntConst;
					}
					else
						break;
				}
				else if (iins->mOperator == IA_SUB)
				{
					if (iins->mSrc[0].mTemp < 0 && iins->mSrc[1].mTemp >= 0)
					{
						ins->mSrc[0].mTemp = iins->mSrc[1].mTemp;
						ins->mSrc[1].mIntConst -= iins->mSrc[0].mIntConst;
					}
					else
						break;
				}
				else
					break;
			}
		}
		break;
	case IC_TYPECAST:
		if (ins->mSrc[0].mType == ins->mDst.mType)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mNumOperands = 1;
			assert(ins->mSrc[0].mTemp >= 0);
		}
		else if (TypeInteger(ins->mSrc[0].mType) && ins->mDst.mType == IT_POINTER)
		{
			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mDst.mType = IT_POINTER;
				ins->mConst.mMemory = IM_ABSOLUTE;
				ins->mConst.mVarIndex = 0;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
			}
		}
		else if (ins->mDst.mType == IT_INT16 && ins->mSrc[0].mType == IT_POINTER)
		{
			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp])
			{
				InterInstruction* cins = tvalue[ins->mSrc[0].mTemp];
				if (cins->mCode == IC_CONSTANT && cins->mConst.mMemory == IM_ABSOLUTE)
				{
					ins->mCode = IC_CONSTANT;
					ins->mDst.mType = IT_INT16;
					ins->mConst.mIntConst = cins->mConst.mIntConst;
					ins->mSrc[0].mTemp = -1;
				}
			}
		}
		break;

	case IC_CONVERSION_OPERATOR:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ConversionConstantFold(ins, tvalue[ins->mSrc[0].mTemp]->mConst);
		}
		break;

	case IC_SELECT:
		for(int i=0; i<3; i++)
		{
			if (ins->mSrc[i].mTemp >= 0 && tvalue[ins->mSrc[i].mTemp] && tvalue[ins->mSrc[i].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mSrc[i] = tvalue[ins->mSrc[i].mTemp]->mConst;
				ins->mSrc[i].mTemp = -1;
			}
		}
		break;
	case IC_RETURN_VALUE:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			switch (ins->mSrc[0].mType)
			{
			case IT_FLOAT:
				break;
			case IT_POINTER:
				break;
			default:
				ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
				break;
			}
		}
		break;
	case IC_BINARY_OPERATOR:
		switch (ins->mSrc[0].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
			{
				if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mFloatConst = ConstantFolding(ins->mOperator, tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
				}
				else
				{
					ins->mSrc[1].mFloatConst = tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst;
					ins->mSrc[1].mTemp = -1;

					if (ins->mOperator == IA_ADD && ins->mSrc[1].mFloatConst == 0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mNumOperands = 1;
						assert(ins->mSrc[0].mTemp >= 0);
					}
					else if (ins->mOperator == IA_MUL)
					{
						if (ins->mSrc[1].mFloatConst == 1.0)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							ins->mNumOperands = 1;
							assert(ins->mSrc[0].mTemp >= 0);
						}
						else if (ins->mSrc[1].mFloatConst == 0.0)
						{
							ins->mCode = IC_CONSTANT;
							ins->mConst.mFloatConst = 0.0;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[1].mTemp = -1;
						}
						else if (ins->mSrc[1].mFloatConst == 2.0)
						{
							ins->mOperator = IA_ADD;
							ins->mSrc[1].mTemp = ins->mSrc[0].mTemp;
							assert(ins->mSrc[0].mTemp >= 0);
						}
					}
				}
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mSrc[0].mFloatConst = tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst;
				ins->mSrc[0].mTemp = -1;

				if (ins->mOperator == IA_ADD && ins->mSrc[0].mFloatConst == 0)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else if (ins->mOperator == IA_MUL)
				{
					if (ins->mSrc[0].mFloatConst == 1.0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
						ins->mSrc[1].mTemp = -1;
						ins->mNumOperands = 1;
						assert(ins->mSrc[0].mTemp >= 0);
					}
					else if (ins->mSrc[0].mFloatConst == 0.0)
					{
						ins->mCode = IC_CONSTANT;
						ins->mConst.mFloatConst = 0.0;
						ins->mSrc[0].mTemp = -1;
						ins->mSrc[1].mTemp = -1;
					}
					else if (ins->mSrc[0].mFloatConst == 2.0)
					{
						ins->mOperator = IA_ADD;
						ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
						assert(ins->mSrc[0].mTemp >= 0);
					}
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
			{
				if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
				}
				else
				{
					ins->mSrc[1].mIntConst = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
					ins->mSrc[1].mTemp = -1;
#if 1
					if (ins->mOperator == IA_ADD && ins->mSrc[1].mIntConst == 0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mNumOperands = 1;
						assert(ins->mSrc[0].mTemp >= 0);
					}
					else if (ins->mOperator == IA_MUL)
					{
						if (ins->mSrc[1].mIntConst == 1)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							ins->mNumOperands = 1;
							assert(ins->mSrc[0].mTemp >= 0);
						}
						else if (ins->mSrc[1].mIntConst == 2)
						{
							ins->mOperator = IA_SHL;
							ins->mSrc[1].mTemp = ins->mSrc[0].mTemp;
							ins->mSrc[1].mType = ins->mSrc[0].mType;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[0].mIntConst = 1;
						}
						else if (ins->mSrc[1].mIntConst == 4)
						{
							ins->mOperator = IA_SHL;
							ins->mSrc[1].mTemp = ins->mSrc[0].mTemp;
							ins->mSrc[1].mType = ins->mSrc[0].mType;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[0].mIntConst = 2;
						}
						else if (ins->mSrc[1].mIntConst == 8)
						{
							ins->mOperator = IA_SHL;
							ins->mSrc[1].mTemp = ins->mSrc[0].mTemp;
							ins->mSrc[1].mType = ins->mSrc[0].mType;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[0].mIntConst = 3;
						}

					}
#endif
				}
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;

				if (ins->mOperator == IA_ADD && ins->mSrc[0].mIntConst == 0)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);
				}
				else if (ins->mOperator == IA_MUL)
				{
					if (ins->mSrc[0].mIntConst == 1)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
						ins->mSrc[1].mTemp = -1;
						ins->mNumOperands = 1;
						assert(ins->mSrc[0].mTemp >= 0);
					}
					else if (ins->mSrc[0].mIntConst == 2)
					{
						ins->mOperator = IA_SHL;
						ins->mSrc[0].mIntConst = 1;
					}
					else if (ins->mSrc[0].mIntConst == 4)
					{
						ins->mOperator = IA_SHL;
						ins->mSrc[0].mIntConst = 2;
					}
					else if (ins->mSrc[0].mIntConst == 8)
					{
						ins->mOperator = IA_SHL;
						ins->mSrc[0].mIntConst = 3;
					}
				}
				else if (ins->mOperator == IA_MODU && (ins->mSrc[0].mIntConst & (ins->mSrc[0].mIntConst - 1)) == 0)
				{
					ins->mOperator = IA_AND;
					ins->mSrc[0].mIntConst--;
				}
			}

			if (ins->mSrc[0].mTemp > 0 && ins->mSrc[1].mTemp > 0 && ins->mSrc[0].mTemp == ins->mSrc[1].mTemp)
			{
				if (ins->mOperator == IA_ADD)
				{
					ins->mOperator = IA_SHL;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[0].mIntConst = 1;
				}
				else if (ins->mOperator == IA_SUB)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mType = ins->mDst.mType;
					ins->mConst.mIntConst = 0;
				}
			}

			if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins->mSrc[0].mTemp];
				if (ins->mOperator == pins->mOperator && (ins->mOperator == IA_ADD || ins->mOperator == IA_MUL || ins->mOperator == IA_AND || ins->mOperator == IA_OR))
				{
					if (pins->mSrc[1].mTemp < 0)
					{
						ins->mSrc[1].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[1].mIntConst, pins->mSrc[1].mIntConst);
						ins->mSrc[0].mTemp = pins->mSrc[0].mTemp;
					}
					else if (pins->mSrc[0].mTemp < 0)
					{
						ins->mSrc[1].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[1].mIntConst, pins->mSrc[0].mIntConst);
						ins->mSrc[0].mTemp = pins->mSrc[1].mTemp;
					}
				}
			}
			else if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins->mSrc[1].mTemp];
				if (ins->mOperator == pins->mOperator && (ins->mOperator == IA_ADD || ins->mOperator == IA_MUL || ins->mOperator == IA_AND || ins->mOperator == IA_OR))
				{
					if (pins->mSrc[1].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[1].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[0].mTemp;
					}
					else if (pins->mSrc[0].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[0].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[1].mTemp;
					}
				}
				else if (ins->mOperator == IA_SUB && pins->mOperator == IA_SUB)
				{
					if (pins->mSrc[0].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(IA_ADD, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[0].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[1].mTemp;
					}
				}
				else if (ins->mOperator == IA_ADD && pins->mOperator == IA_SUB)
				{
					if (pins->mSrc[0].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(IA_SUB, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[0].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[1].mTemp;
					}
				}
				else if (ins->mOperator == IA_SUB && pins->mOperator == IA_ADD)
				{
					if (pins->mSrc[1].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[1].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[0].mTemp;
					}
					else if (pins->mSrc[0].mTemp < 0)
					{
						ins->mSrc[0].mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, ins->mSrc[0].mIntConst, pins->mSrc[0].mIntConst);
						ins->mSrc[1].mTemp = pins->mSrc[1].mTemp;
					}
				}
				else if (ins->mOperator == IA_SHL && (pins->mOperator == IA_SHR || pins->mOperator == IA_SAR) && pins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == pins->mSrc[0].mIntConst)
				{
					ins->mOperator = IA_AND;
					ins->mSrc[0].mIntConst = -1LL << ins->mSrc[0].mIntConst;
					ins->mSrc[1].mTemp = pins->mSrc[1].mTemp;
				}
			}

			break;
		}
		break;
	case IC_UNARY_OPERATOR:
		switch (ins->mSrc[0].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mFloatConst = ConstantFolding(ins->mOperator, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mSrc[0].mTemp = -1;
			}
			break;
		case IT_POINTER:
			break;
		default:
			break;
		}
		break;
	case IC_RELATIONAL_OPERATOR:
		switch (ins->mSrc[1].mType)
		{
		case IT_FLOAT:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantRelationalFolding(ins->mOperator, tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mDst.mType = IT_BOOL;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
			}
			else
			{
				if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[1].mFloatConst = tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst;
					ins->mSrc[1].mTemp = -1;
				}
				else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[0].mFloatConst = tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst;
					ins->mSrc[0].mTemp = -1;
				}
			}
			break;
		case IT_POINTER:
#if 0
			if (ins->mOperator == IA_CMPEQ || ins->mOperator == IA_CMPNE)
			{
				if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mOperandSize = tvalue[ins->mSrc[0].mTemp]->mOperandSize;
					ins->mSrc[0].mTemp = -1;
				}
				else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mOperandSize = tvalue[ins->mSrc[1].mTemp]->mOperandSize;
					ins->mSrc[1].mTemp = -1;
				}
			}
#endif
			break;
		default:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
			}
			else
			{
				if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[1].mIntConst = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
					ins->mSrc[1].mTemp = -1;
				}
				else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					ins->mSrc[0].mTemp = -1;
				}
			}
			break;
		}
		break;
	case IC_BRANCH:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
			ins->mSrc[0].mTemp = -1;
		}
		break;
	}
}


void InterCodeBasicBlock::CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
			mInstructions[i]->CollectLocalAddressTemps(localTable, paramTable);

		if (mTrueJump) mTrueJump->CollectLocalAddressTemps(localTable, paramTable);
		if (mFalseJump) mFalseJump->CollectLocalAddressTemps(localTable, paramTable);
	}
}

void InterCodeBasicBlock::MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
			mInstructions[i]->MarkAliasedLocalTemps(localTable, aliasedLocals, paramTable, aliasedParams);

		if (mTrueJump) mTrueJump->MarkAliasedLocalTemps(localTable, aliasedLocals, paramTable, aliasedParams);
		if (mFalseJump) mFalseJump->MarkAliasedLocalTemps(localTable, aliasedLocals, paramTable, aliasedParams);
	}
}

bool InterCodeBasicBlock::PropagateNonLocalUsedConstTemps(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int i = 0;
		while (i < mInstructions.Size())
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_CONSTANT && ins->mSingleAssignment)
			{
				int	ttemp = ins->mDst.mTemp;
				InterCodeBasicBlock* target = this;
				while (target && !target->mLocalUsedTemps[ttemp])
				{
					InterCodeBasicBlock* ttarget = nullptr;

					if (!target->mFalseJump)
						ttarget = target->mTrueJump;
					else if (!target->mFalseJump->mFalseJump && target->mFalseJump->mTrueJump == target->mTrueJump && !target->mFalseJump->mLocalUsedTemps[ttemp])
						ttarget = target->mTrueJump;
					else if (!target->mTrueJump->mFalseJump && target->mTrueJump->mTrueJump == target->mFalseJump && !target->mTrueJump->mLocalUsedTemps[ttemp])
						ttarget = target->mFalseJump;

					while (ttarget && ttarget->mLoopHead)
					{
						if (ttarget->mFalseJump == ttarget && !ttarget->mLocalUsedTemps[ttemp])
							ttarget = ttarget->mTrueJump;
						else if (ttarget->mTrueJump == ttarget && !ttarget->mLocalUsedTemps[ttemp])
							ttarget = ttarget->mFalseJump;
						else
							ttarget = nullptr;
					}

					target = ttarget;
				}

				if (target && this != target)
				{
					target->mInstructions.Insert(0, ins);
					mInstructions.Remove(i);
					changed = true;
				}
				else
					i++;
			}
			else
				i++;
		}

		if (mTrueJump && mTrueJump->PropagateNonLocalUsedConstTemps())
			changed = true;
		if (mFalseJump && mFalseJump->PropagateNonLocalUsedConstTemps())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::CollectAllUsedDefinedTemps(NumberSet& defined, NumberSet& used) 
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mDst.mTemp >= 0)
				defined += ins->mDst.mTemp;
			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0)
					used += ins->mSrc[j].mTemp;
			}
		}

		if (mTrueJump) mTrueJump->CollectAllUsedDefinedTemps(defined, used);
		if (mFalseJump) mFalseJump->CollectAllUsedDefinedTemps(defined, used);
	}
}

void InterCodeBasicBlock::CollectLocalUsedTemps(int numTemps)
{
	if (!mVisited)
	{
		mVisited = true;

		mLocalUsedTemps.Reset(numTemps);
		mLocalModifiedTemps.Reset(numTemps);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mDst.mTemp >= 0)
				mLocalModifiedTemps += ins->mDst.mTemp;
			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0)
					mLocalUsedTemps += ins->mSrc[j].mTemp;
			}
		}

		if (mTrueJump) mTrueJump->CollectLocalUsedTemps(numTemps);
		if (mFalseJump) mFalseJump->CollectLocalUsedTemps(numTemps);
	}
}

void  InterCodeBasicBlock::CollectConstTemps(GrowingInstructionPtrArray& ctemps, NumberSet& assignedTemps)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	ltemps(nullptr);

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->ConstantFolding();
			mInstructions[i]->PropagateConstTemps(ltemps);

			int		ttemp = mInstructions[i]->mDst.mTemp;
			if (ttemp >= 0)
			{
				if (mInstructions[i]->mCode == IC_CONSTANT)
					ltemps[ttemp] = mInstructions[i];

				if (assignedTemps[ttemp])
					ctemps[ttemp] = nullptr;
				else
				{
					assignedTemps += ttemp;
					if (mInstructions[i]->mCode == IC_CONSTANT)
						ctemps[ttemp] = mInstructions[i];
				}
			}
		}

		if (mTrueJump) mTrueJump->CollectConstTemps(ctemps, assignedTemps);
		if (mFalseJump) mFalseJump->CollectConstTemps(ctemps, assignedTemps);
	}
}

bool InterCodeBasicBlock::PropagateVariableCopy(const GrowingInstructionPtrArray& ctemps, const GrowingVariableArray& staticVars)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	ltemps(nullptr);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			int	j;

			if (ins->mDst.mTemp >= 0)
			{
				j = 0;
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (ltemps[k]->mSrc[0].mTemp != ins->mDst.mTemp && ltemps[k]->mSrc[1].mTemp != ins->mDst.mTemp)
					{
						ltemps[j++] = ltemps[k];
					}
				}
				ltemps.SetSize(j);
			}

			switch (ins->mCode)
			{
			case IC_LOAD:
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (SameMemSegment(ltemps[k]->mSrc[1], ins->mSrc[0]))
					{
						ins->mSrc[0].mMemory = ltemps[k]->mSrc[0].mMemory;
						ins->mSrc[0].mTemp = ltemps[k]->mSrc[0].mTemp;
						ins->mSrc[0].mVarIndex = ltemps[k]->mSrc[0].mVarIndex;
						ins->mSrc[0].mIntConst += ltemps[k]->mSrc[0].mIntConst;
						ins->mSrc[0].mLinkerObject = ltemps[k]->mSrc[0].mLinkerObject;
						changed = true;
					}
				}

				break;

			case IC_STORE:

				j = 0;
				for(int k=0; k<ltemps.Size(); k++)
				{
					if (!CollidingMem(ltemps[k]->mSrc[0], ins->mSrc[1], staticVars) &&
						!CollidingMem(ltemps[k]->mSrc[1], ins->mSrc[1], staticVars))
					{
						ltemps[j++] = ltemps[k];
					}
				}
				ltemps.SetSize(j);
				break;

			case IC_COPY:
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (SameMemAndSize(ltemps[k]->mSrc[1], ins->mSrc[0]))
					{
						ins->mSrc[0] = ltemps[k]->mSrc[0];
						changed = true;
					}
				}

				j = 0;
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (!CollidingMem(ltemps[k]->mSrc[0], ins->mSrc[1], staticVars) &&
						!CollidingMem(ltemps[k]->mSrc[1], ins->mSrc[1], staticVars))
					{
						ltemps[j++] = ltemps[k];
					}
				}
				ltemps.SetSize(j);
				ltemps.Push(ins);
				break;
			}

		}

		if (mTrueJump && mTrueJump->PropagateVariableCopy(ltemps, staticVars))
			changed = true;
		if (mFalseJump && mFalseJump->PropagateVariableCopy(ltemps, staticVars))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::PropagateConstTemps(const GrowingInstructionPtrArray& ctemps)
{
	bool	changed = false;

	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->PropagateConstTemps(ctemps))
				changed = true;
		}

		if (mTrueJump && mTrueJump->PropagateConstTemps(ctemps))
			changed = true;
		if (mFalseJump && mFalseJump->PropagateConstTemps(ctemps))
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::SimplifyIntegerRangeRelops(void)
{
	if (!mVisited)
	{
		mVisited = true;

#if 1
		int sz = mInstructions.Size();
		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp)
		{
			InterInstruction* cins = mInstructions[sz - 2];

			bool	constFalse = false, constTrue = false;

			if ((cins->mSrc[1].mTemp < 0 || (cins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND && cins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND)) &&
				(cins->mSrc[0].mTemp < 0 || (cins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && cins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)))
			{
				switch (cins->mOperator)
				{
				case IA_CMPEQ:
					if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue || cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constFalse = true;
					break;
				case IA_CMPNE:
					if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue || cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constTrue = true;
					break;
				case IA_CMPLS:
					if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
						constTrue = true;
					else if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
						constFalse = true;
					break;
				case IA_CMPLU:
					if (cins->mSrc[1].IsUnsigned() && cins->mSrc[0].IsUnsigned())
					{
						if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPLES:
					if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
						constTrue = true;
					else if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constFalse = true;
					break;
				case IA_CMPLEU:
					if (cins->mSrc[1].IsUnsigned() && cins->mSrc[0].IsUnsigned())
					{
						if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPGS:
					if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constTrue = true;
					else if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
						constFalse = true;
					break;
				case IA_CMPGU:
					if (cins->mSrc[1].IsUnsigned() && cins->mSrc[0].IsUnsigned())
					{
						if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constFalse = true;
					}
					break;
				case IA_CMPGES:
					if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
						constTrue = true;
					else if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
						constFalse = true;
					break;
				case IA_CMPGEU:
					if (cins->mSrc[1].IsUnsigned() && cins->mSrc[0].IsUnsigned())
					{
						if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
							constFalse = true;
					}
					break;
				}
			}


			if (constTrue || constFalse)
			{
				cins->mCode = IC_CONSTANT;
				cins->mConst.mType = IT_BOOL;
				cins->mConst.mIntConst = constTrue ? 1 : 0;
				cins->mNumOperands = 0;
			}
			else
			{
				switch (cins->mOperator)
				{
				case IA_CMPLS:
					if (cins->mSrc[0].IsUnsigned() && cins->mSrc[1].IsUnsigned())
						cins->mOperator = IA_CMPLU;
					break;
				case IA_CMPLES:
					if (cins->mSrc[0].IsUnsigned() && cins->mSrc[1].IsUnsigned())
						cins->mOperator = IA_CMPLEU;
					break;
				case IA_CMPGS:
					if (cins->mSrc[0].IsUnsigned() && cins->mSrc[1].IsUnsigned())
						cins->mOperator = IA_CMPGU;
					break;
				case IA_CMPGES:
					if (cins->mSrc[0].IsUnsigned() && cins->mSrc[1].IsUnsigned())
						cins->mOperator = IA_CMPGEU;
					break;
				}
			}
		}
#endif

		if (sz >= 1 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 1]->mSrc[0].mTemp < 0)
		{
			InterInstruction* bins = mInstructions[sz - 1];

			if (bins->mSrc[0].mIntConst)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
			else
			{
				mTrueJump->mNumEntries--;
				mTrueJump = mFalseJump;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
		}

		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_CONSTANT && mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp)
		{
			InterInstruction* bins = mInstructions[sz - 1];

			if (mInstructions[sz - 2]->mConst.mIntConst)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
			else
			{
				mTrueJump->mNumEntries--;
				mTrueJump = mFalseJump;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
		}
		

#if 1
		for (int i = 0; i < sz; i++)
		{
			if (i + 1 < sz)
			{
				if (
					mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&
					mInstructions[i + 1]->mCode == IC_LEA &&
					mInstructions[i + 0]->mSrc[1].mTemp != mInstructions[i + 0]->mDst.mTemp &&
					mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
					mInstructions[i + 0]->mSrc[0].IsUByte() && mInstructions[i + 1]->mSrc[0].IsUByte() &&
					mInstructions[i + 0]->mSrc[0].mRange.mMaxValue + mInstructions[i + 1]->mSrc[0].mRange.mMaxValue < 256)
				{
					mInstructions[i + 0]->mCode = IC_BINARY_OPERATOR;
					mInstructions[i + 0]->mOperator = IA_ADD;

					mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
					mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
					mInstructions[i + 1]->mSrc[0].mTemp = mInstructions[i + 0]->mDst.mTemp;
					mInstructions[i + 0]->mDst.mType = IT_INT16;
					mInstructions[i + 1]->mSrc[0].mType = IT_INT16;
					mInstructions[i + 1]->mSrc[0].mRange.mMaxValue += mInstructions[i + 0]->mSrc[0].mRange.mMaxValue;
					mInstructions[i + 0]->mDst.mRange = mInstructions[i + 1]->mSrc[0].mRange;
				}
			}
			if (i + 2 < sz)
			{
				if (
					mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&
					mInstructions[i + 2]->mCode == IC_LEA &&
					mInstructions[i + 0]->mSrc[1].mTemp != mInstructions[i + 0]->mDst.mTemp &&
					mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&
					mInstructions[i + 0]->mSrc[0].IsUByte() && mInstructions[i + 2]->mSrc[0].IsUByte() &&
					mInstructions[i + 0]->mSrc[0].mRange.mMaxValue + mInstructions[i + 2]->mSrc[0].mRange.mMaxValue < 256 &&

					mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
					mInstructions[i + 1]->mDst.mTemp != mInstructions[0]->mSrc[0].mTemp &&
					mInstructions[i + 1]->mDst.mTemp != mInstructions[0]->mSrc[1].mTemp&&
					mInstructions[i + 1]->mDst.mTemp != mInstructions[0]->mDst.mTemp)
				{
					InterInstruction* ins = mInstructions[i + 1];
					mInstructions[i + 1] = mInstructions[i + 0];
					mInstructions[i + 0] = ins;
				}
			}
#if 1
			if (mInstructions[i]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i]->mOperator == IA_EXT8TO16S &&
				mInstructions[i]->mSrc[0].IsUByte() && mInstructions[i]->mSrc[0].mRange.mMaxValue < 128)
			{
				mInstructions[i]->mOperator = IA_EXT8TO16U;
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mOperator == IA_SAR && mInstructions[i]->mSrc[0].IsUnsigned() && mInstructions[i]->mSrc[1].IsUnsigned())
			{
				mInstructions[i]->mOperator = IA_SHR;
				mInstructions[i]->ConstantFolding();
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mOperator == IA_DIVS && mInstructions[i]->mSrc[0].IsUnsigned() && mInstructions[i]->mSrc[1].IsUnsigned())
			{
				mInstructions[i]->mOperator = IA_DIVU;
				mInstructions[i]->ConstantFolding();
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mOperator == IA_MODS && mInstructions[i]->mSrc[0].IsUnsigned() && mInstructions[i]->mSrc[1].IsUnsigned())
			{
				mInstructions[i]->mOperator = IA_MODU;
				mInstructions[i]->ConstantFolding();
			}

#endif
		}
#endif
		if (mTrueJump)
			mTrueJump->SimplifyIntegerRangeRelops();
		if (mFalseJump)
			mFalseJump->SimplifyIntegerRangeRelops();
	}
}


bool InterCodeBasicBlock::BuildGlobalIntegerRangeSets(bool initial, const GrowingVariableArray& localVars)
{
	bool	changed = false;

	mNumEntered++;
	if (!mLoopHead && mNumEntered < mEntryBlocks.Size())
		return false;

	mLocalValueRange.Clear();

	assert(mLocalValueRange.Size() == mExitRequiredTemps.Size());

	for (int j = 0; j < mEntryBlocks.Size(); j++)
	{
		InterCodeBasicBlock* from = mEntryBlocks[j];
		GrowingIntegerValueRangeArray& range(this == from->mTrueJump ? from->mTrueValueRange : from->mFalseValueRange);
		if (j == 0)
			mLocalValueRange = range;
		else
		{
			for (int i = 0; i < mLocalValueRange.Size(); i++)
				mLocalValueRange[i].Merge(range[i], mLoopHead, initial);
		}
		assert(mLocalValueRange.Size() == mExitRequiredTemps.Size());
	}

	for (int i = 0; i < mLocalValueRange.Size(); i++)
		if (!mLocalValueRange[i].Same(mEntryValueRange[i]))
			changed = true;

	if (mVisited && mNumEntered >= 2 * mEntryBlocks.Size())
		return changed;

	if (mTrueJump && !mFalseJump)
	{
		for (int i = 0; i < mEntryMemoryValueSize.Size(); i++)
		{
			if (mEntryMemoryValueSize[i] != mTrueJump->mMemoryValueSize[i])
				changed = true;
		}
	}

	if (!mVisited || changed)
	{
		mVisited = true;

		if (changed)
		{
			mEntryValueRange = mLocalValueRange;

			UpdateLocalIntegerRangeSets(localVars);

		}

		if (mTrueJump && mTrueJump->BuildGlobalIntegerRangeSets(initial, localVars))
			changed = true;
		if (mFalseJump && mFalseJump->BuildGlobalIntegerRangeSets(initial, localVars))
			changed = true;
	}

	return changed;
}

static int64 SignedTypeMin(InterType type)
{
	switch (type)
	{
	case IT_INT8:
		return -128;
	case IT_INT16:
		return -32768;
	default:
		return -0x80000000LL;
	}
}

static int64 SignedTypeMax(InterType type)
{
	switch (type)
	{
	case IT_INT8:
		return 127;
	case IT_INT16:
		return 32767;
	default:
		return 0x7fffffff;
	}
}

static int64 BuildLowerBitsMask(int64 v)
{
	v |= v >> 32;
	v |= v >> 16;
	v |= v >> 8;
	v |= v >> 4;
	v |= v >> 2;
	v |= v >> 1;
	return v;
}

void InterCodeBasicBlock::UpdateLocalIntegerRangeSets(const GrowingVariableArray& localVars)
{
	mLocalValueRange = mEntryValueRange;

	int sz = mInstructions.Size();

	assert(mLocalValueRange.Size() == mExitRequiredTemps.Size());

	for (int i = 0; i < sz; i++)
	{
		InterInstruction* ins(mInstructions[i]);

		for (int i = 0; i < ins->mNumOperands; i++)
		{
			if (IsIntegerType(ins->mSrc[i].mType) || ins->mSrc[i].mType == IT_BOOL)
			{
				if (ins->mSrc[i].mTemp >= 0)
				{
					ins->mSrc[i].mRange = mLocalValueRange[ins->mSrc[i].mTemp];
#if 1
					if (ins->mSrc[i].mRange.mMinState == IntegerValueRange::S_BOUND && ins->mSrc[i].mRange.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[i].mRange.mMinValue == ins->mSrc[i].mRange.mMaxValue)
					{
						ins->mSrc[i].mTemp = -1;
						ins->mSrc[i].mIntConst = ins->mSrc[i].mRange.mMinValue;
					}
#endif
				}
				else
				{
					ins->mSrc[i].mRange.mMaxState = ins->mSrc[i].mRange.mMinState = IntegerValueRange::S_BOUND;
					ins->mSrc[i].mRange.mMinValue = ins->mSrc[i].mRange.mMaxValue = ins->mSrc[i].mIntConst;
				}
			}
		}

		ins->ConstantFolding();

		if (ins->mDst.mTemp >= 0 && IsIntegerType(ins->mDst.mType))
		{
			IntegerValueRange& vr(mLocalValueRange[ins->mDst.mTemp]);

			switch (ins->mCode)
			{
			case IC_LOAD:
				vr = ins->mDst.mRange;

				if (i > 0 &&
					mInstructions[i - 1]->mCode == IC_LEA && mInstructions[i - 1]->mDst.mTemp == ins->mSrc[0].mTemp &&
					mInstructions[i - 1]->mSrc[1].mTemp < 0 && mInstructions[i - 1]->mSrc[1].mMemory == IM_GLOBAL && (mInstructions[i - 1]->mSrc[1].mLinkerObject->mFlags & LOBJF_CONST))
				{
					if (ins->mDst.mType == IT_INT8)
					{
						LinkerObject* lo = mInstructions[i - 1]->mSrc[1].mLinkerObject;
						int	mi = 0, ma = 0;

						if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMaxState == IntegerValueRange::S_BOUND &&
							vr.mMinValue >= -128 && vr.mMaxValue <= 127)
						{
							for (int j = 0; j < lo->mSize; j++)
							{
								int v = (int8)(lo->mData[j]);
								if (v < mi)
									mi = v;
								if (v > ma)
									ma = v;
							}
						}
						else
						{
							for (int j = 0; j < lo->mSize; j++)
							{
								int v = lo->mData[j];
								if (v & 0x80)
									mi = -128;
								if (v > ma)
									ma = v;
							}
						}

						vr.LimitMax(ma);
						vr.LimitMin(mi);
					}
				}

				break;
			case IC_CONSTANT:
				vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
				vr.mMinValue = vr.mMaxValue = ins->mConst.mIntConst;
				break;
			case IC_LOAD_TEMPORARY:
				vr = ins->mSrc[0].mRange;
				break;
			case IC_SELECT:
				vr = ins->mSrc[1].mRange;
				vr.Merge(ins->mSrc[0].mRange, false, false);
				break;
			case IC_UNARY_OPERATOR:
			{
				switch (ins->mOperator)
				{
				case IA_NEG:
				{
					IntegerValueRange	tr;
					IntegerValueRange& sr(mLocalValueRange[ins->mSrc[0].mTemp]);

					tr.mMinState = vr.mMaxState;
					tr.mMinValue = -vr.mMaxValue;
					tr.mMaxState = vr.mMinState;
					tr.mMaxValue = -vr.mMinValue;
					vr = tr;
				}
				break;
				default:
					vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
				}
				break;
			}
			case IC_CONVERSION_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_EXT8TO16S:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < -128 || vr.mMaxValue > 127)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 127;
					}
					if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < -128 || vr.mMinValue > 127)
					{
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = -128;
					}
					break;

				case IA_EXT8TO16U:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < 0 || vr.mMaxValue > 255 || vr.mMinValue < 0)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 255;
					}
					if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < 0 || vr.mMinValue > 255)
					{
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = 0;
					}
					break;

				default:
					vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
				}
				break;

			case IC_BINARY_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_ADD:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst > 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (ins->mSrc[0].mIntConst < 0 && vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						vr.mMaxValue += ins->mSrc[0].mIntConst;
						vr.mMinValue += ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[0].mTemp];
						if (ins->mSrc[1].mIntConst > 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (ins->mSrc[1].mIntConst < 0 && vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						vr.mMaxValue += ins->mSrc[1].mIntConst;
						vr.mMinValue += ins->mSrc[1].mIntConst;
					}
					else
					{
						if (ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND)
						{
							vr.mMaxState = IntegerValueRange::S_BOUND;
							vr.mMaxValue = ins->mSrc[0].mRange.mMaxValue + ins->mSrc[1].mRange.mMaxValue;
						}
						else
							vr.mMaxState = IntegerValueRange::S_UNBOUND;

						if (ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND)
						{
							vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMinValue = ins->mSrc[0].mRange.mMinValue + ins->mSrc[1].mRange.mMinValue;
						}
						else
							vr.mMinState = IntegerValueRange::S_UNBOUND;
					}

					break;
				case IA_SUB:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst < 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (ins->mSrc[0].mIntConst > 0 && vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						vr.mMaxValue -= ins->mSrc[0].mIntConst;
						vr.mMinValue -= ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[0].mTemp];
						int	maxv = vr.mMaxValue, minv = vr.mMinValue;

						if (vr.mMaxState == IntegerValueRange::S_WEAK)
						{
							if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						}
						if (vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;

						vr.mMaxValue = ins->mSrc[1].mIntConst - minv;
						vr.mMinValue = ins->mSrc[1].mIntConst - maxv;
					}
					else
					{
						if (ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND)
						{
							vr.mMaxState = IntegerValueRange::S_BOUND;
							vr.mMaxValue = ins->mSrc[1].mRange.mMaxValue - ins->mSrc[0].mRange.mMinValue;
						}
						else
							vr.mMaxState = IntegerValueRange::S_UNBOUND;

						if (ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND)
						{
							vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMinValue = ins->mSrc[1].mRange.mMinValue - ins->mSrc[0].mRange.mMaxValue;
						}
						else
							vr.mMinState = IntegerValueRange::S_UNBOUND;
					}

					break;
				case IA_MUL:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst > 0)
						{
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							else if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMinState = IntegerValueRange::S_UNBOUND;

							vr.mMaxValue *= ins->mSrc[0].mIntConst;
							vr.mMinValue *= ins->mSrc[0].mIntConst;
						}
						else
						{
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
							{
								if (vr.mMinState == IntegerValueRange::S_WEAK)
									vr.mMaxState = IntegerValueRange::S_UNBOUND;
								vr.mMinState = IntegerValueRange::S_UNBOUND;
							}
							if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;

							int	maxv = vr.mMaxValue, minv = vr.mMinValue;
							vr.mMaxValue = ins->mSrc[0].mIntConst * minv;
							vr.mMinValue = ins->mSrc[0].mIntConst * maxv;
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[0].mTemp];
						if (ins->mSrc[1].mIntConst > 0)
						{
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							else if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMinState = IntegerValueRange::S_UNBOUND;

							vr.mMaxValue *= ins->mSrc[1].mIntConst;
							vr.mMinValue *= ins->mSrc[1].mIntConst;
						}
						else
						{
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
							{
								if (vr.mMinState == IntegerValueRange::S_WEAK)
									vr.mMaxState = IntegerValueRange::S_UNBOUND;
								vr.mMinState = IntegerValueRange::S_UNBOUND;
							}
							if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;

							int	maxv = vr.mMaxValue, minv = vr.mMinValue;
							vr.mMaxValue = ins->mSrc[1].mIntConst * minv;
							vr.mMinValue = ins->mSrc[1].mIntConst * maxv;
						}
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;

					break;
				case IA_SHL:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;

						vr.mMaxValue <<= ins->mSrc[0].mIntConst;
						vr.mMinValue <<= ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[0].IsUByte() && ins->mSrc[0].mRange.mMaxValue < 16)
					{
						if (ins->mSrc[1].mTemp < 0)
						{
							vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMaxState = IntegerValueRange::S_BOUND;
							
							if (ins->mSrc[1].mIntConst < 0)
							{
								vr.mMinValue = ins->mSrc[1].mIntConst << ins->mSrc[0].mRange.mMaxValue;
								vr.mMaxValue = 0;
							}
							else
							{
								vr.mMinValue = 0;
								vr.mMaxValue = ins->mSrc[1].mIntConst << ins->mSrc[0].mRange.mMaxValue;
							}
						}
						else
						{
							vr = mLocalValueRange[ins->mSrc[1].mTemp];
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							else if (vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMinState = IntegerValueRange::S_UNBOUND;

							vr.mMaxValue <<= ins->mSrc[0].mRange.mMaxValue;
							vr.mMinValue <<= ins->mSrc[0].mRange.mMaxValue;
						}
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
				case IA_SHR:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];

						if (ins->mSrc[0].mIntConst > 0)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinState >= 0)
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = (unsigned short)(int64min(65535, vr.mMaxValue)) >> ins->mSrc[0].mIntConst;
									vr.mMinValue = (unsigned short)(int64max(0, vr.mMinValue)) >> ins->mSrc[0].mIntConst;
									break;
								case IT_INT8:
									vr.mMaxValue = (unsigned char)(int64min(255, vr.mMaxValue)) >> ins->mSrc[0].mIntConst;
									vr.mMinValue = (unsigned char)(int64max(0, vr.mMinValue)) >> ins->mSrc[0].mIntConst;
									break;
								case IT_INT32:
									vr.mMaxValue = (unsigned)(vr.mMaxValue) >> ins->mSrc[0].mIntConst;
									vr.mMinValue = (unsigned)(vr.mMinValue) >> ins->mSrc[0].mIntConst;
									break;
								}
							}
							else
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = 65535 >> ins->mSrc[0].mIntConst;
									vr.mMinValue = 0;
									break;
								case IT_INT8:
									vr.mMaxValue = 255 >> ins->mSrc[0].mIntConst;
									vr.mMinValue = 0;
									break;
								case IT_INT32:
									vr.mMaxValue = 0x100000000ULL >> ins->mSrc[0].mIntConst;
									vr.mMinValue = 0;
									break;
								}
							}
						}
					}
					else if (ins->mSrc[1].mTemp >= 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMinValue >= 0)
							vr.mMinValue = 0;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
				case IA_SAR:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];

						if (ins->mSrc[0].mIntConst > 0)
						{
							switch (ins->mSrc[1].mType)
							{
							case IT_INT16:
								vr.mMaxValue = (short)(int64min( 32767, vr.mMaxValue)) >> ins->mSrc[0].mIntConst;
								vr.mMinValue = (short)(int64max(-32768, vr.mMinValue)) >> ins->mSrc[0].mIntConst;
								break;
							case IT_INT8:
								vr.mMaxValue = (char)(int64min( 127, vr.mMaxValue)) >> ins->mSrc[0].mIntConst;
								vr.mMinValue = (char)(int64max(-128, vr.mMinValue)) >> ins->mSrc[0].mIntConst;
								break;
							case IT_INT32:
								vr.mMaxValue = (int)(vr.mMaxValue) >> ins->mSrc[0].mIntConst;
								vr.mMinValue = (int)(vr.mMinValue) >> ins->mSrc[0].mIntConst;
								break;
							}
						}
					}
					else if (ins->mSrc[1].mTemp >= 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMinValue >= 0)
							vr.mMinValue = 0;
						else if (vr.mMaxValue < 0)
							vr.mMaxValue = -1;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
				case IA_AND:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst >= 0)
						{
							vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMaxValue = ins->mSrc[0].mIntConst;
							vr.mMinValue = 0;
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[0].mTemp];
						if (ins->mSrc[1].mIntConst >= 0)
						{
							vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMaxValue = ins->mSrc[1].mIntConst;
							vr.mMinValue = 0;
						}
					}
					else if (ins->mSrc[0].IsUnsigned() && ins->mSrc[1].IsUnsigned())
					{
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
						int64 v0 = (ins->mSrc[0].mRange.mMaxValue & BuildLowerBitsMask(ins->mSrc[1].mRange.mMaxValue));
						int64 v1 = (ins->mSrc[1].mRange.mMaxValue & BuildLowerBitsMask(ins->mSrc[0].mRange.mMaxValue));
						vr.mMaxValue = (v0 > v1) ? v0 : v1;
						vr.mMinValue = 0;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;

				case IA_OR:
				case IA_XOR:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[1].mTemp];
						int64	v = vr.mMaxValue;
						v |= v >> 16;
						v |= v >> 8;
						v |= v >> 4;
						v |= v >> 2;
						v |= v >> 1;

						if (vr.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[0].mIntConst >= 0)
							vr.mMaxValue = BuildLowerBitsMask(vr.mMaxValue) | ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mLocalValueRange[ins->mSrc[0].mTemp];

						if (vr.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[0].mIntConst >= 0)
							vr.mMaxValue = BuildLowerBitsMask(vr.mMaxValue) | ins->mSrc[0].mIntConst;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;

				default:
					vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
				}
				break;

			default:
				vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
			}

			ins->mDst.mRange = vr;
#if 1
			if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMinState == IntegerValueRange::S_BOUND && vr.mMaxValue == vr.mMinValue)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mType = ins->mDst.mType;
				ins->mConst.mIntConst = vr.mMaxValue;
				ins->mNumOperands = 0;
			}
#endif
		}

		assert(mLocalValueRange.Size() == mExitRequiredTemps.Size());
	}

#if 1
	mReverseValueRange.SetSize(mLocalValueRange.Size());

	for (int i = 0; i < mReverseValueRange.Size(); i++)
		mReverseValueRange[i].Reset();

	if (mTrueJump && !mFalseJump)
	{
		for (int i = 0; i < mMemoryValueSize.Size(); i++)
		{
			mEntryMemoryValueSize[i] = mMemoryValueSize[i] = mTrueJump->mMemoryValueSize[i];
		}
	}

	for (int i = sz - 1; i >= 0; i--)
	{
		InterInstruction* ins(mInstructions[i]);
		if (ins->mCode == IC_LOAD && ins->mSrc[0].mMemory == IM_INDIRECT && ins->mSrc[0].mTemp >= 0)
			mMemoryValueSize[ins->mSrc[0].mTemp] = int64max(mMemoryValueSize[ins->mSrc[0].mTemp], ins->mSrc[0].mIntConst + InterTypeSize[ins->mDst.mType]);
		else if (ins->mCode == IC_STORE && ins->mSrc[1].mMemory == IM_INDIRECT && ins->mSrc[1].mTemp >= 0)
			mMemoryValueSize[ins->mSrc[1].mTemp] = int64max(mMemoryValueSize[ins->mSrc[1].mTemp], ins->mSrc[1].mIntConst + InterTypeSize[ins->mSrc[0].mType]);
		else if (ins->mCode == IC_COPY)
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT && ins->mSrc[0].mTemp >= 0)
				mMemoryValueSize[ins->mSrc[0].mTemp] = ins->mConst.mOperandSize;
			if (ins->mSrc[1].mMemory == IM_INDIRECT && ins->mSrc[1].mTemp >= 0)
				mMemoryValueSize[ins->mSrc[1].mTemp] = ins->mConst.mOperandSize;
		}
		else if (ins->mCode == IC_LEA && ins->mSrc[1].mMemory != IM_INDIRECT && ins->mSrc[0].mTemp >= 0 && mMemoryValueSize[ins->mDst.mTemp] > 0)
		{
			int	asize = 0;
			if (ins->mSrc[1].mMemory == IM_GLOBAL)
				asize = ins->mSrc[1].mLinkerObject->mSize;
			else if (ins->mSrc[1].mMemory == IM_LOCAL)
				asize = localVars[ins->mSrc[1].mVarIndex]->mSize;

			if (asize > 0)
			{
				mReverseValueRange[ins->mSrc[0].mTemp].LimitMin(0);
				mReverseValueRange[ins->mSrc[0].mTemp].LimitMax(asize - mMemoryValueSize[ins->mDst.mTemp]);
			}
		}

		if (ins->mDst.mTemp >= 0)
		{
			ins->mDst.mRange.Limit(mReverseValueRange[ins->mDst.mTemp]);
			mReverseValueRange[ins->mDst.mTemp].Reset();
			IntegerValueRange& vr(ins->mDst.mRange);

			switch (ins->mCode)
			{
			case IC_CONVERSION_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_EXT8TO16U:
					if (ins->mSrc[0].mTemp >= 0 && (vr.mMaxValue != 255 || vr.mMinValue != 0))
						mReverseValueRange[ins->mSrc[0].mTemp].Limit(vr);
					break;
				case IA_EXT8TO16S:
					if (ins->mSrc[0].mTemp >= 0 && (vr.mMaxValue != 127 || vr.mMinValue != -128))
						mReverseValueRange[ins->mSrc[0].mTemp].Limit(vr);
					break;
				}
				break;
			case IC_BINARY_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_SHL:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue >> ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue >> ins->mSrc[0].mIntConst);
						mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					break;
				case IA_SUB:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue + ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue + ins->mSrc[0].mIntConst);
						mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					break;
				case IA_ADD:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue - ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue - ins->mSrc[0].mIntConst);
						mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMin(vr.mMinValue - ins->mSrc[1].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMax(vr.mMaxValue - ins->mSrc[1].mIntConst);
						mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
					}
					else if (ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND && ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue - ins->mSrc[0].mRange.mMaxValue);
						if (vr.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue - ins->mSrc[0].mRange.mMinValue);

						if (vr.mMinState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMin(vr.mMinValue - ins->mSrc[1].mRange.mMaxValue);
						if (vr.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMax(vr.mMaxValue - ins->mSrc[1].mRange.mMinValue);

						mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
						mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					break;
				case IA_MUL:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mIntConst > 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue / ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue / ins->mSrc[0].mIntConst);
						mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mIntConst > 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMin(vr.mMinValue / ins->mSrc[1].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMax(vr.mMaxValue / ins->mSrc[1].mIntConst);
						mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
					}
					break;
				}
				break;

			case IC_LEA:
				if (ins->mSrc[1].mMemory == IM_INDIRECT && mMemoryValueSize[ins->mDst.mTemp] > 0)
				{
					if (ins->mSrc[0].mTemp < 0)
					{
						mMemoryValueSize[ins->mSrc[1].mTemp] = mMemoryValueSize[ins->mDst.mTemp] - ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)
					{
						mMemoryValueSize[ins->mSrc[1].mTemp] = mMemoryValueSize[ins->mDst.mTemp] - ins->mSrc[0].mRange.mMinValue;
					}
				}
				break;
			}

		}

		for (int i = 0; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp >= 0)
				ins->mSrc[i].mRange.Limit(mReverseValueRange[ins->mSrc[i].mTemp]);
		}

		if (ins->mDst.mTemp >= 0)
			mMemoryValueSize[ins->mDst.mTemp] = 0;
	}
#endif

	mTrueValueRange = mLocalValueRange;
	mFalseValueRange = mLocalValueRange;

	if (sz >= 1)
	{
		if (mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 1]->mSrc[0].mTemp >= 0 && mInstructions[sz - 1]->mSrc[0].mType == IT_BOOL)
		{
			int s = mInstructions[sz - 1]->mSrc[0].mTemp;

			mTrueValueRange[s].mMinState = IntegerValueRange::S_BOUND;
			mTrueValueRange[s].mMinValue = 1;
			mTrueValueRange[s].mMaxState = IntegerValueRange::S_BOUND;
			mTrueValueRange[s].mMaxValue = 1;

			mFalseValueRange[s].mMinState = IntegerValueRange::S_BOUND;
			mFalseValueRange[s].mMinValue = 0;
			mFalseValueRange[s].mMaxState = IntegerValueRange::S_BOUND;
			mFalseValueRange[s].mMaxValue = 0;
		}
	}

	if (sz >= 2)
	{
		if (mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp && IsIntegerType(mInstructions[sz - 2]->mSrc[0].mType))
		{
			int	s1 = mInstructions[sz - 2]->mSrc[1].mTemp, s0 = mInstructions[sz - 2]->mSrc[0].mTemp;

			switch (mInstructions[sz - 2]->mOperator)
			{
#if 1
			case IA_CMPEQ:
				if (s0 < 0)
				{
					mTrueValueRange[s1].mMinState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s1].mMinValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
					mTrueValueRange[s1].mMaxState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s1].mMaxValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
				}
				else if (s1 < 0)
				{
					mTrueValueRange[s0].mMinState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s0].mMinValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
					mTrueValueRange[s0].mMaxState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s0].mMaxValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
				}
				break;

			case IA_CMPNE:
				if (s0 < 0)
				{
					mFalseValueRange[s1].mMinState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s1].mMinValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
					mFalseValueRange[s1].mMaxState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s1].mMaxValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
				}
				else if (s1 < 0)
				{
					mFalseValueRange[s0].mMinState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s0].mMinValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
					mFalseValueRange[s0].mMaxState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s0].mMaxValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
				}
				break;
#endif
			case IA_CMPLS:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst - 1);
					mTrueValueRange[s1].LimitMinWeak(SignedTypeMin(mInstructions[sz - 2]->mSrc[1].mType));

					mFalseValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMaxWeak(SignedTypeMax(mInstructions[sz - 2]->mSrc[1].mType));
				}
				else if (s1 < 0)
				{
					mTrueValueRange[s0].LimitMin(mInstructions[sz - 2]->mSrc[1].mIntConst + 1);
					mTrueValueRange[s0].LimitMaxWeak(SignedTypeMax(mInstructions[sz - 2]->mSrc[0].mType));

					mFalseValueRange[s0].LimitMax(mInstructions[sz - 2]->mSrc[1].mIntConst);
					mFalseValueRange[s0].LimitMinWeak(SignedTypeMin(mInstructions[sz - 2]->mSrc[0].mType));
				}
				else
				{
					if (mLocalValueRange[s0].mMaxState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s1].LimitMax(mLocalValueRange[s0].mMaxValue - 1);
					if (mLocalValueRange[s0].mMinState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s1].LimitMin(mLocalValueRange[s0].mMinValue);
				}
				break;
			case IA_CMPLES:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst + 1);
				}
				break;
			case IA_CMPGS:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst + 1);
					mFalseValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst);
				}
				break;
			case IA_CMPGES:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst - 1);
				}
				break;

			case  IA_CMPLU:
				if (s1 >= 0)
				{
					if (s0 < 0)
					{
						mTrueValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst - 1);
						mTrueValueRange[s1].LimitMin(0);

						if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && mFalseValueRange[s1].mMinValue >= 0)
						{
							mFalseValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst);
						}
					}
					else if (mInstructions[sz - 2]->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND)
					{
						mTrueValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mRange.mMaxValue - 1);
						mTrueValueRange[s1].LimitMin(0);

						if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && mFalseValueRange[s1].mMinValue >= 0)
						{
							mFalseValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mRange.mMaxValue);
						}
					}
				}
				break;
			case IA_CMPLEU:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst);
					mTrueValueRange[s1].LimitMin(0);

					if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && mFalseValueRange[s1].mMinValue >= 0)
					{
						mFalseValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst + 1);
					}
				}
				break;
			case IA_CMPGU:
				if (s1 >= 0)
				{
					mTrueValueRange[s1].LimitMin(1);
					if (s0 < 0)
					{
						mTrueValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst + 1);
						mFalseValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst);
						mFalseValueRange[s1].LimitMin(0);
					}
				}
				break;
			case IA_CMPGEU:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMin(mInstructions[sz - 2]->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMax(mInstructions[sz - 2]->mSrc[0].mIntConst - 1);
					mFalseValueRange[s1].LimitMin(0);
				}
				break;
			}

			if (s1 >= 0 && sz > 2 && mInstructions[sz - 3]->mCode == IC_LOAD_TEMPORARY && mInstructions[sz - 3]->mSrc[0].mTemp == s1)
			{
				mTrueValueRange[mInstructions[sz - 3]->mDst.mTemp] = mTrueValueRange[s1];
				mFalseValueRange[mInstructions[sz - 3]->mDst.mTemp] = mFalseValueRange[s1];
			}
		}
	}

	for (int i = 0; i < mLocalValueRange.Size(); i++)
	{
		if (!mExitRequiredTemps[i])
		{
			mLocalValueRange[i].mMinState = mLocalValueRange[i].mMaxState = IntegerValueRange::S_UNKNOWN;
			mTrueValueRange[i] = mFalseValueRange[i] = mLocalValueRange[i];
		}
	}
}



void InterCodeBasicBlock::RestartLocalIntegerRangeSets(const GrowingVariableArray& localVars)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mEntryValueRange.Size(); i++)
		{
			IntegerValueRange& vr(mEntryValueRange[i]);
			if (vr.mMinState == IntegerValueRange::S_UNBOUND)
				vr.mMinState = IntegerValueRange::S_UNKNOWN;
			if (vr.mMaxState == IntegerValueRange::S_UNBOUND)
				vr.mMaxState = IntegerValueRange::S_UNKNOWN;
		}

		UpdateLocalIntegerRangeSets(localVars);

		if (mTrueJump) mTrueJump->RestartLocalIntegerRangeSets(localVars);
		if (mFalseJump) mFalseJump->RestartLocalIntegerRangeSets(localVars);
	}
}

void InterCodeBasicBlock::BuildLocalIntegerRangeSets(int num, const GrowingVariableArray& localVars)
{
	if (!mVisited)
	{
		mVisited = true;

		mEntryValueRange.SetSize(num, true);
		mTrueValueRange.SetSize(num, true);
		mFalseValueRange.SetSize(num, true);
		mLocalValueRange.SetSize(num, true);
		mMemoryValueSize.SetSize(num, true);
		mEntryMemoryValueSize.SetSize(num, true);

		UpdateLocalIntegerRangeSets(localVars);

		if (mTrueJump) mTrueJump->BuildLocalIntegerRangeSets(num, localVars);
		if (mFalseJump) mFalseJump->BuildLocalIntegerRangeSets(num, localVars);
	}
}

void InterCodeBasicBlock::BuildConstTempSets(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mEntryConstTemp = NumberSet(mEntryRequiredTemps.Size());
		mExitConstTemp = NumberSet(mEntryRequiredTemps.Size());

		for (i = 0; i < mInstructions.Size(); i++)
		{
			const InterInstruction* ins = mInstructions[i];

			if (ins->mDst.mTemp >= 0)
			{
				if (ins->mCode == IC_CONSTANT)
					mExitConstTemp += ins->mDst.mTemp;
				else
					mExitConstTemp -= ins->mDst.mTemp;
			}
		}

		if (mTrueJump) mTrueJump->BuildConstTempSets();
		if (mFalseJump) mFalseJump->BuildConstTempSets();
	}
}

bool InterCodeBasicBlock::PropagateConstOperationsUp(void)
{
//	return false;

	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mTrueJump->PropagateConstOperationsUp())
			changed = true;

		if (mFalseJump && mFalseJump->PropagateConstOperationsUp())
			changed = true;

		if (mEntryBlocks.Size())
		{
			mEntryConstTemp = mEntryBlocks[0]->mExitConstTemp;

			for (int i = 1; i < mEntryBlocks.Size(); i++)
				mEntryConstTemp &= mEntryBlocks[i]->mExitConstTemp;

			int i = 0;
			while (i + 1 < mInstructions.Size())
			{
				const InterInstruction* ins = mInstructions[i];
			
				if (!HasSideEffect(ins->mCode) && ins->mCode != IC_CONSTANT && ins->mCode != IC_STORE && ins->mCode != IC_COPY)
				{
					bool	isProvided = false;
					if (ins->mDst.mTemp >= 0)
					{
						for (int j = 0; j < mEntryBlocks.Size(); j++)
							if (mEntryBlocks[j]->mExitRequiredTemps[ins->mDst.mTemp])
								isProvided = true;
					}
						

					bool	hasop = false;
					int		j = 0;
					while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || mEntryConstTemp[ins->mSrc[j].mTemp]))
					{
						if (ins->mSrc[j].mTemp >= 0)
							hasop = true;
						j++;
					}

					if (j == ins->mNumOperands && hasop && !isProvided && CanMoveInstructionBeforeBlock(i))
					{
						for (int j = 0; j < mEntryBlocks.Size(); j++)
						{
							InterInstruction* nins = ins->Clone();
							InterCodeBasicBlock* eb = mEntryBlocks[j];

							int di = eb->mInstructions.Size() - 1;
							if (eb->mInstructions[di]->mCode == IC_BRANCH && di > 0 && eb->mInstructions[di - 1]->mDst.mTemp == eb->mInstructions[di]->mSrc[0].mTemp &&
								CanBypassUp(ins, eb->mInstructions[di - 1]))
							{
								di--;
							}

							eb->mInstructions.Insert(di, nins);
						}
						mInstructions.Remove(i);
						changed = true;
					}
					else
						i++;
				}
				else
					i++;

				if (ins->mDst.mTemp >= 0)
					mEntryConstTemp -= ins->mDst.mTemp;
			}			
		}
	}

	return changed;
}




void InterCodeBasicBlock::BuildLocalTempSets(int num)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mLocalRequiredTemps = NumberSet(num);
		mLocalProvidedTemps = NumberSet(num);

		mEntryRequiredTemps = NumberSet(num);
		mEntryProvidedTemps = NumberSet(num);
		mExitRequiredTemps = NumberSet(num);
		mExitProvidedTemps = NumberSet(num);

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->FilterTempUsage(mLocalRequiredTemps, mLocalProvidedTemps);
		}

		mEntryRequiredTemps = mLocalRequiredTemps;
		mExitProvidedTemps = mLocalProvidedTemps;

		if (mTrueJump) mTrueJump->BuildLocalTempSets(num);
		if (mFalseJump) mFalseJump->BuildLocalTempSets(num);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedTempSet(NumberSet fromProvidedTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mEntryProvidedTemps = fromProvidedTemps;
		changed = true;
	}
	else if (!(mEntryProvidedTemps <= fromProvidedTemps))
	{
		mEntryProvidedTemps &= fromProvidedTemps;
		changed = true;
	}

	if (changed)
	{
		mExitProvidedTemps = mLocalProvidedTemps;
		mExitProvidedTemps |= mEntryProvidedTemps;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedTempSet(mExitProvidedTemps);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedTempSet(mExitProvidedTemps);
	}
}

static bool SameSingleAssignment(const GrowingInstructionPtrArray& tunified, const InterInstruction* ins, const InterInstruction* cins)
{
	if (ins->mCode == cins->mCode && ins->mOperator == cins->mOperator && ins->mNumOperands == cins->mNumOperands)
	{
		if (ins->mCode == IC_CONSTANT)
			return ins->mConst.IsEqual(cins->mConst);

		for (int i = 0; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp < 0)
			{
				if (cins->mSrc[i].mTemp < 0)
				{
					if (!ins->mSrc[i].IsEqual(cins->mSrc[i]))
						return false;
				}
				else
					return false;
			}
			else if (cins->mSrc[i].mTemp < 0)
			{
				return false;
			}
			else if (tunified[ins->mSrc[i].mTemp] != tunified[cins->mSrc[i].mTemp])
				return false;
		}

		return true;
	}
	else
		return false;
}

bool InterCodeBasicBlock::SingleAssignmentTempForwarding(const GrowingInstructionPtrArray& tunified,  const GrowingInstructionPtrArray& tvalues)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	ntunified(tunified), ntvalues(tvalues);

		NumberSet providedTemps(mEntryProvidedTemps);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mSingleAssignment)
			{
				int	j = 0;
				while (j < ntvalues.Size() && !(providedTemps[ntvalues[j]->mDst.mTemp] && SameSingleAssignment(ntunified, ins, ntvalues[j])))
					j++;
				if (j < ntvalues.Size())
				{
					if (ins->mCode != IC_CONSTANT && !(ins->mCode == IC_LOAD && ins->mSrc[0].mMemory == IM_FPARAM))
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSrc[0].mTemp = ntvalues[j]->mDst.mTemp;
						ins->mSrc[0].mType = ntvalues[j]->mDst.mType;
						ins->mNumOperands = 1;
						assert(ins->mSrc[0].mTemp >= 0);
					}
					changed = true;
					ntunified[ins->mDst.mTemp] = ntvalues[j];
				}
				else
				{
					ntvalues.Push(ins);
					ntunified[ins->mDst.mTemp] = ins;
				}
			}
			if (ins->mDst.mTemp >= 0)
				providedTemps += ins->mDst.mTemp;
		}

		if (mTrueJump && mTrueJump->SingleAssignmentTempForwarding(ntunified, ntvalues))
			changed = true;
		if (mFalseJump && mFalseJump->SingleAssignmentTempForwarding(ntunified, ntvalues))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::CalculateSingleAssignmentTemps(FastNumberSet& tassigned, GrowingInstructionPtrArray& tvalue, NumberSet& modifiedParams, InterMemory paramMemory)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			int	t = ins->mDst.mTemp;
			if (t >= 0)
			{
				if (!tassigned[t] || tvalue[t] == ins)
				{
					if (!tassigned[t])
					{
						changed = true;
						tassigned += t;
					}

					int j = 0;
					while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || tvalue[ins->mSrc[j].mTemp] != nullptr))
						j++;

					bool	valid = j == ins->mNumOperands;
					if (valid)
					{
						if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE || ins->mCode == IC_ASSEMBLER)
							valid = false;
						else if (ins->mCode == IC_LOAD)
						{
							if (ins->mSrc[0].mMemory == paramMemory)
							{
								if (modifiedParams[ins->mSrc[0].mVarIndex])
									valid = false;
							}
							else if (ins->mSrc[0].mMemory == IM_GLOBAL)
							{
								if (!(ins->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
									valid = false;
							}
							else
								valid = false;
						}
						else if (ins->mCode == IC_LEA)
						{
							if (ins->mSrc[1].mMemory == paramMemory)
							{
								if (!modifiedParams[ins->mSrc[1].mVarIndex])
								{
									modifiedParams += ins->mSrc[1].mVarIndex;
									changed = true;
								}
							}
						}
					}

					if (valid)
					{
						if (!tvalue[t])
						{
							tvalue[t] = ins;
							changed = true;
						}
					}
					else if (tvalue[t])
					{
						tvalue[t] = nullptr;
						changed = true;
					}
				}
				else if (tvalue[t])
				{
					tvalue[t] = nullptr;
					changed = true;
				}
			}
			else if (ins->mCode == IC_STORE)
			{
				if (ins->mSrc[1].mMemory == paramMemory)
				{
					if (!modifiedParams[ins->mSrc[1].mVarIndex])
					{
						modifiedParams += ins->mSrc[1].mVarIndex;
						changed = true;
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->CalculateSingleAssignmentTemps(tassigned, tvalue, modifiedParams, paramMemory))
			changed = true;
		if (mFalseJump && mFalseJump->CalculateSingleAssignmentTemps(tassigned, tvalue, modifiedParams, paramMemory))
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	int i;

	if (!mVisited)
	{
		TempForwardingTable	localForwardingTable(forwardingTable);

		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this) && mLocalModifiedTemps.Size())
			{
				assert(localForwardingTable.Size() == mLocalModifiedTemps.Size());

				for (int i = 0; i < mLocalModifiedTemps.Size(); i++)
				{
					if (mLocalModifiedTemps[i])
						localForwardingTable.Destroy(i);
				}
			}
			else
				localForwardingTable.Reset();
		}
#if 0
		else if (mNumEntries > 1)
		{
			lvalues.FlushAll();
			ltvalue.Clear();
		}
#endif
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				localForwardingTable.Intersect(mMergeForwardingTable);
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeForwardingTable = localForwardingTable;
				return;
			}
		}

		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->PerformTempForwarding(localForwardingTable);
		}

		if (mTrueJump) mTrueJump->PerformTempForwarding(localForwardingTable);
		if (mFalseJump) mFalseJump->PerformTempForwarding(localForwardingTable);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredTempSet(NumberSet& fromRequiredTemps)
{
	bool revisit = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	newRequiredTemps(mExitRequiredTemps);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredTempSet(newRequiredTemps)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredTempSet(newRequiredTemps)) revisit = true;

		if (!(newRequiredTemps <= mExitRequiredTemps))
		{
			revisit = true;

			mExitRequiredTemps = newRequiredTemps;
			newRequiredTemps -= mLocalProvidedTemps;
			mEntryRequiredTemps |= newRequiredTemps;
		}

	}

	fromRequiredTemps |= mEntryRequiredTemps;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedResultInstructions(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredTemps(mExitRequiredTemps);
		int i;

		if (mInstructions.Size() > 0)
		{
			for (i = mInstructions.Size() - 1; i > 0; i--)
			{
				if (mInstructions[i]->RemoveUnusedResultInstructions(mInstructions[i - 1], requiredTemps))
					changed = true;
			}
			if (mInstructions[0]->RemoveUnusedResultInstructions(NULL, requiredTemps))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedResultInstructions())
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedResultInstructions())
				changed = true;
		}
	}

	return changed;
}

void InterCodeBasicBlock::BuildCallerSaveTempSet(NumberSet& callerSaveTemps)
{
	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredTemps(mExitRequiredTemps);
		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
			mInstructions[i]->BuildCallerSaveTempSet(requiredTemps, callerSaveTemps);
	
		if (mTrueJump)
			mTrueJump->BuildCallerSaveTempSet(callerSaveTemps);
		if (mFalseJump)
			mFalseJump->BuildCallerSaveTempSet(callerSaveTemps);
	}
}


void InterCodeBasicBlock::BuildStaticVariableSet(const GrowingVariableArray& staticVars)
{
	if (!mVisited)
	{
		mVisited = true;

		mLocalRequiredStatics = NumberSet(staticVars.Size());
		mLocalProvidedStatics = NumberSet(staticVars.Size());

		mEntryRequiredStatics = NumberSet(staticVars.Size());
		mEntryProvidedStatics = NumberSet(staticVars.Size());
		mExitRequiredStatics = NumberSet(staticVars.Size());
		mExitProvidedStatics = NumberSet(staticVars.Size());

		for (int i = 0; i < mInstructions.Size(); i++)
			mInstructions[i]->FilterStaticVarsUsage(staticVars, mLocalRequiredStatics, mLocalProvidedStatics);

		mEntryRequiredStatics = mLocalRequiredStatics;
		mExitProvidedStatics = mLocalProvidedStatics;

		if (mTrueJump) mTrueJump->BuildStaticVariableSet(staticVars);
		if (mFalseJump) mFalseJump->BuildStaticVariableSet(staticVars);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet fromProvidedVars)
{
	if (!mVisited || !(fromProvidedVars <= mEntryProvidedStatics))
	{
		mEntryProvidedStatics |= fromProvidedVars;
		fromProvidedVars |= mExitProvidedStatics;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedStaticVariableSet(staticVars, fromProvidedVars);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedStaticVariableSet(staticVars, fromProvidedVars);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet& fromRequiredVars)
{
	bool revisit = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	newRequiredVars(mExitRequiredStatics);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredStaticVariableSet(staticVars, newRequiredVars)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredStaticVariableSet(staticVars, newRequiredVars)) revisit = true;

		if (!(newRequiredVars <= mExitRequiredStatics))
		{
			revisit = true;

			mExitRequiredStatics = newRequiredVars;
			newRequiredVars -= mLocalProvidedStatics;
			mEntryRequiredStatics |= newRequiredVars;
		}

	}

	fromRequiredVars |= mEntryRequiredStatics;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedStaticStoreInstructions(const GrowingVariableArray& staticVars)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredVars(mExitRequiredStatics);

		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			if (mInstructions[i]->RemoveUnusedStaticStoreInstructions(staticVars, requiredVars))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedStaticStoreInstructions(staticVars))
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedStaticStoreInstructions(staticVars))
				changed = true;
		}
	}

	return changed;
}

void InterCodeBasicBlock::BuildLocalVariableSets(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mLocalRequiredVars = NumberSet(localVars.Size());
		mLocalProvidedVars = NumberSet(localVars.Size());

		mEntryRequiredVars = NumberSet(localVars.Size());
		mEntryProvidedVars = NumberSet(localVars.Size());
		mExitRequiredVars = NumberSet(localVars.Size());
		mExitProvidedVars = NumberSet(localVars.Size());

		mLocalRequiredParams = NumberSet(params.Size());
		mLocalProvidedParams = NumberSet(params.Size());

		mEntryRequiredParams = NumberSet(params.Size());
		mEntryProvidedParams = NumberSet(params.Size());
		mExitRequiredParams = NumberSet(params.Size());
		mExitProvidedParams = NumberSet(params.Size());

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->FilterVarsUsage(localVars, mLocalRequiredVars, mLocalProvidedVars, params, mLocalRequiredParams, mLocalProvidedParams, paramMemory);
		}

		mEntryRequiredVars = mLocalRequiredVars;
		mExitProvidedVars = mLocalProvidedVars;

		mEntryRequiredParams = mLocalRequiredParams;
		mExitProvidedParams = mLocalProvidedParams;

		if (mTrueJump) mTrueJump->BuildLocalVariableSets(localVars, params, paramMemory);
		if (mFalseJump) mFalseJump->BuildLocalVariableSets(localVars, params, paramMemory);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedVariableSet(const GrowingVariableArray& localVars, NumberSet fromProvidedVars, const GrowingVariableArray& params, NumberSet fromProvidedParams, InterMemory paramMemory)
{
	if (!mVisited || !(fromProvidedVars <= mEntryProvidedVars) || !(fromProvidedParams <= mEntryProvidedParams))
	{
		mEntryProvidedVars |= fromProvidedVars;
		fromProvidedVars |= mExitProvidedVars;

		mEntryProvidedParams |= fromProvidedParams;
		fromProvidedParams |= mExitProvidedParams;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars, params, fromProvidedParams, paramMemory);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars, params, fromProvidedParams, paramMemory);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredVariableSet(const GrowingVariableArray& localVars, NumberSet& fromRequiredVars, const GrowingVariableArray& params, NumberSet& fromRequiredParams, InterMemory paramMemory)
{
	bool revisit = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	newRequiredVars(mExitRequiredVars);
		NumberSet	newRequiredParams(mExitRequiredParams);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars, params, newRequiredParams, paramMemory)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars, params, newRequiredParams, paramMemory)) revisit = true;

		if (!(newRequiredVars <= mExitRequiredVars) || !(newRequiredParams <= mExitRequiredParams))
		{
			revisit = true;

			mExitRequiredVars = newRequiredVars;
			newRequiredVars -= mLocalProvidedVars;
			mEntryRequiredVars |= newRequiredVars;

			mExitRequiredParams = newRequiredParams;
			newRequiredParams -= mLocalProvidedParams;
			mEntryRequiredParams |= newRequiredParams;
		}

	}

	fromRequiredVars |= mEntryRequiredVars;
	fromRequiredParams |= mEntryRequiredParams;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredVars(mExitRequiredVars);
		NumberSet		requiredParams(mExitRequiredParams);

		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			if (mInstructions[i]->RemoveUnusedStoreInstructions(localVars, requiredVars, params, requiredParams, paramMemory))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedStoreInstructions(localVars, params, paramMemory))
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedStoreInstructions(localVars, params, paramMemory))
				changed = true;
		}
	}

	return changed;

}

bool InterCodeBasicBlock::EliminateAliasValues(const GrowingInstructionPtrArray& tvalue, const GrowingInstructionPtrArray& avalue)
{
	bool	changed = false;

	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);
		GrowingInstructionPtrArray	lavalue(avalue);

		if (mLoopHead)
		{
			ltvalue.Clear();
			lavalue.Clear();
		}
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (mMergeTValues[i] != ltvalue[i])
						ltvalue[i] = nullptr;
					if (mMergeAValues[i] != lavalue[i])
						lavalue[i] = nullptr;
				}

				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (lavalue[i] && !ltvalue[lavalue[i]->mSrc[0].mTemp])
						lavalue[i] = nullptr;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTValues = ltvalue;
				mMergeAValues = lavalue;
				return false;
			}
		}

		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp > 0 && lavalue[ins->mSrc[j].mTemp])
				{
					InterInstruction* mins = lavalue[ins->mSrc[j].mTemp];

					if (mExitRequiredTemps[mins->mDst.mTemp] && !mExitRequiredTemps[mins->mSrc[0].mTemp])
					{
						ins->mSrc[j].Forward(mins->mDst);
						changed = true;
					}
				}
			}

			if (ins->mDst.mTemp >= 0)
			{
				if (ltvalue[ins->mDst.mTemp] && ltvalue[ins->mDst.mTemp]->mCode == IC_LOAD_TEMPORARY)
					lavalue[ltvalue[ins->mDst.mTemp]->mSrc[0].mTemp] = nullptr;

				ltvalue[ins->mDst.mTemp] = ins;
				lavalue[ins->mDst.mTemp] = nullptr;
				if (ins->mCode == IC_LOAD_TEMPORARY)
					lavalue[ins->mSrc[0].mTemp] = ins;
			}
		}


		if (mTrueJump && mTrueJump->EliminateAliasValues(ltvalue, lavalue))
			changed = true;

		if (mFalseJump && mFalseJump->EliminateAliasValues(ltvalue, lavalue))
			changed = true;
	}

	return changed;
}

bool  InterCodeBasicBlock::MergeIndexedLoadStore(const GrowingInstructionPtrArray& tvalue)
{
	bool	changed = false;

	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);

		if (mNumEntries > 1)
			ltvalue.Clear();

		mVisited = true;

		// Move lea to front

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			if (ins->mCode == IC_LEA)
			{
				int j = i;
				while (j > 0 && CanBypassUp(ins, mInstructions[j - 1]))
				{
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				mInstructions[j] = ins;
			}
		}
#if 1
		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			if (ins->mCode == IC_STORE || ins->mCode == IC_LOAD)
			{
				int pi = ins->mCode == IC_LOAD ? 0 : 1;

				if (ins->mSrc[pi].mTemp >= 0 && ltvalue[ins->mSrc[pi].mTemp])
				{
					InterInstruction* lins = ltvalue[ins->mSrc[pi].mTemp];

					if (lins->mSrc[0].mTemp >= 0)
					{
						if (lins->mSrc[1].mMemory != IM_ABSOLUTE || (lins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && lins->mSrc[0].mRange.mMaxValue >= 256))
						{
							InterInstruction* bins = lins;
							for (int j = 0; j < ltvalue.Size(); j++)
							{
								InterInstruction* cins = ltvalue[j];
								if (cins &&
									cins->mSrc[0].mTemp == bins->mSrc[0].mTemp &&
									cins->mSrc[1].mTemp < 0 && bins->mSrc[1].mTemp < 0 &&
									cins->mSrc[1].mMemory == bins->mSrc[1].mMemory &&
									cins->mSrc[1].mVarIndex == bins->mSrc[1].mVarIndex &&
									cins->mSrc[1].mIntConst < bins->mSrc[1].mIntConst)
								{
									bins = cins;
								}
							}

							if (bins != lins && ins->mSrc[pi].mIntConst + lins->mSrc[1].mIntConst - bins->mSrc[1].mIntConst < 252 && ins->mSrc[pi].mIntConst + lins->mSrc[1].mIntConst - bins->mSrc[1].mIntConst >= 0)
							{
								ins->mSrc[pi].mTemp = bins->mDst.mTemp;
								ins->mSrc[pi].mIntConst += lins->mSrc[1].mIntConst - bins->mSrc[1].mIntConst;
								changed = true;
							}
						}
					}
				}
			}

			int	dtemp = ins->mDst.mTemp;

			if (dtemp >= 0)
			{

				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (ltvalue[i] && ltvalue[i]->ReferencesTemp(dtemp))
						ltvalue[i] = nullptr;
				}

				if (!ins->UsesTemp(dtemp))
					ltvalue[dtemp] = ins;
			}
		}
#endif

		if (mTrueJump && mTrueJump->MergeIndexedLoadStore(ltvalue))
			changed = true;

		if (mFalseJump && mFalseJump->MergeIndexedLoadStore(ltvalue))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::SimplifyIntegerNumeric(const GrowingInstructionPtrArray& tvalue, int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);

		if (mLoopHead)
		{
			ltvalue.Clear();
		}
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (mMergeTValues[i] != ltvalue[i])
						ltvalue[i] = nullptr;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTValues = ltvalue;
				return false;
			}
		}

		mVisited = true;


		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			switch (ins->mCode)
			{
			case IC_BINARY_OPERATOR:
			{
				switch (ins->mOperator)
				{
				case IA_SHL:
#if 1
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp] && ins->mDst.mType == IT_INT16)
					{
						InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];
						if (pins->mCode == IC_CONVERSION_OPERATOR && pins->mOperator == IA_EXT8TO16U && pins->mSrc[0].IsUByte() && pins->mSrc[0].mTemp >= 0 && ltvalue[pins->mSrc[0].mTemp])
						{
							InterInstruction* ains = ltvalue[pins->mSrc[0].mTemp];

							if (ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_ADD && ains->mSrc[0].mTemp < 0)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* nins = new InterInstruction();
								nins->mCode = IC_BINARY_OPERATOR;
								nins->mOperator = IA_SHL;
								nins->mSrc[0] = ins->mSrc[0];
								nins->mSrc[1] = ains->mSrc[1];
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_INT16;
								nins->mDst.mRange = ins->mDst.mRange;
								mInstructions.Insert(i, nins);

								ins->mOperator = IA_ADD;
								ins->mSrc[0] = ains->mSrc[0];
								ins->mSrc[0].mIntConst <<= nins->mSrc[0].mIntConst;
								ins->mSrc[1] = nins->mDst;

								changed = true;
								break;
							}
						}
					}
#endif
#if 1
					if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp] && ins->mDst.mType == IT_INT16)
					{
						InterInstruction* pins = ltvalue[ins->mSrc[0].mTemp];
						if (pins->mCode == IC_CONVERSION_OPERATOR && pins->mOperator == IA_EXT8TO16U && pins->mSrc[0].IsUByte() && pins->mSrc[0].mRange.mMaxValue < 16 && pins->mSrc[0].mTemp >= 0 && ltvalue[pins->mSrc[0].mTemp])
						{
							InterInstruction* ains = ltvalue[pins->mSrc[0].mTemp];

							if (ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_ADD && ains->mSrc[0].mTemp < 0 && ains->mSrc[1].IsUByte())
							{
								ins->mSrc[0] = ains->mSrc[1];
								ins->mSrc[1].mIntConst <<= ains->mSrc[0].mIntConst;

								changed = true;
								break;
							}
						}
					}
#endif
					break;
#if 1
				case IA_ADD:
					if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp] && ins->mSrc[0].mFinal)
					{
						InterInstruction* pins = ltvalue[ins->mSrc[0].mTemp];

						if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD)
						{
							if (pins->mSrc[0].mTemp < 0)
							{
								ins->mSrc[0].Forward(pins->mSrc[1]);
								pins->mSrc[1].mFinal = false;
								ins->mSrc[1].mIntConst += pins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (pins->mSrc[1].mTemp < 0)
							{
								ins->mSrc[0].Forward(pins->mSrc[0]);
								pins->mSrc[0].mFinal = false;
								ins->mSrc[1].mIntConst += pins->mSrc[1].mIntConst;
								changed = true;
							}
						}
					}
					else if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp] && ins->mSrc[1].mFinal)
					{
						InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

						if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD)
						{
							if (pins->mSrc[0].mTemp < 0)
							{
								ins->mSrc[1].Forward(pins->mSrc[1]);
								pins->mSrc[1].mFinal = false;
								ins->mSrc[0].mIntConst += pins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (pins->mSrc[1].mTemp < 0)
							{
								ins->mSrc[1].Forward(pins->mSrc[0]);
								pins->mSrc[0].mFinal = false;
								ins->mSrc[0].mIntConst += pins->mSrc[1].mIntConst;
								changed = true;
							}
						}
					}

					break;
#endif
				}
			}	break;

			case IC_LEA:
				if (ins->mSrc[1].mMemory == IM_INDIRECT && ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp])
				{
					InterInstruction* pins = tvalue[ins->mSrc[1].mTemp];
					if (pins->mCode == IC_LEA)
						ins->mSrc[1].mLinkerObject = pins->mSrc[1].mLinkerObject;
				}

				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp])
				{
					InterInstruction* pins = ltvalue[ins->mSrc[0].mTemp];

					if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD && pins->mSrc[0].mTemp < 0 && pins->mDst.mType == IT_INT16)
					{
						ins->mSrc[0] = pins->mSrc[1];
						ins->mSrc[1].mIntConst += pins->mSrc[0].mIntConst;
						changed = true;
					}
#if 1
					else if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD && pins->mSrc[1].mTemp < 0 && pins->mDst.mType == IT_INT16)
					{
						ins->mSrc[0] = pins->mSrc[0];
						ins->mSrc[1].mIntConst += pins->mSrc[1].mIntConst;
						changed = true;
					}
#endif
#if 1
					else if (pins->mCode == IC_CONVERSION_OPERATOR && pins->mOperator == IA_EXT8TO16U && pins->mSrc[0].IsUByte() && pins->mSrc[0].mTemp >= 0 && ltvalue[pins->mSrc[0].mTemp])
					{
						InterInstruction* ains = ltvalue[pins->mSrc[0].mTemp];

						if (ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_ADD && ains->mSrc[0].mTemp < 0)
						{
							if (ains->mSrc[1].mType == IT_INT16)
							{
								ins->mSrc[0] = ains->mSrc[1];
								ins->mSrc[1].mIntConst += ains->mSrc[0].mIntConst;
								changed = true;
							}
							else if (ains->mSrc[1].mType == IT_INT8)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* nins = new InterInstruction();
								nins->mCode = IC_CONVERSION_OPERATOR;
								nins->mOperator = IA_EXT8TO16U;
								nins->mSrc[0] = ains->mSrc[1];
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_INT16;
								nins->mDst.mRange = pins->mDst.mRange;
								mInstructions.Insert(i, nins);

								ins->mSrc[0] = nins->mDst;
								ins->mSrc[1].mIntConst += ains->mSrc[0].mIntConst;
								changed = true;
							}
						}
					}
#endif
				}
				else if (ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp] && ltvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
				{
					InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

					ins->mSrc[1].ForwardMem(pins->mConst);
					ins->mSrc[1].mType = IT_POINTER;
					changed = true;
				}
				else if (ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mTemp < 0 && ltvalue[ins->mSrc[1].mTemp])
				{
					InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

					if (pins->mCode == IC_LEA && pins->mSrc[1].mTemp < 0)
					{
						ins->mSrc[1].ForwardMem(pins->mSrc[1]);
						ins->mSrc[1].mIntConst += ins->mSrc[0].mIntConst;
						ins->mSrc[0].Forward(pins->mSrc[0]);
						changed = true;
					}
				}
				break;
#if 1
			case IC_CONVERSION_OPERATOR:
				if (ins->mOperator == IA_EXT8TO16U)
				{
					if (ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp] && ltvalue[ins->mSrc[0].mTemp]->mDst.mType == IT_INT16 && ins->mSrc[0].IsUByte())
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSrc[0].mType = IT_INT16;
						assert(ins->mSrc[0].mTemp >= 0);
						changed = true;
					}
				}
				else if (ins->mOperator == IA_EXT8TO16S)
				{
					if (ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp] && ltvalue[ins->mSrc[0].mTemp]->mDst.mType == IT_INT16 && ins->mSrc[0].IsSByte())
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSrc[0].mType = IT_INT16;
						assert(ins->mSrc[0].mTemp >= 0);
						changed = true;
					}
				}

				break;
#endif
#if 1
			case IC_STORE:
				if (ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp])
				{
					InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

					if (ins->mSrc[1].mMemory == IM_INDIRECT && pins->mCode == IC_LEA)
						ins->mSrc[1].mLinkerObject = pins->mSrc[1].mLinkerObject;

					if (pins->mCode == IC_LEA && pins->mSrc[0].mTemp < 0 && ins->mSrc[1].mIntConst + pins->mSrc[0].mIntConst >= 0)
					{
						ins->mSrc[1].Forward(pins->mSrc[1]);
						pins->mSrc[1].mFinal = false;
						ins->mSrc[1].mIntConst += pins->mSrc[0].mIntConst;
						changed = true;
					}
				}
				break;
#endif
#if 1
			case IC_LOAD:
				if (ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp])
				{
					InterInstruction* pins = ltvalue[ins->mSrc[0].mTemp];

					if (ins->mSrc[0].mMemory == IM_INDIRECT  && pins->mCode == IC_LEA)
						ins->mSrc[0].mLinkerObject = pins->mSrc[1].mLinkerObject;

					if (pins->mCode == IC_LEA && pins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst + pins->mSrc[0].mIntConst >= 0)
					{
						ins->mSrc[0].Forward(pins->mSrc[1]);
						pins->mSrc[1].mFinal = false;
						ins->mSrc[0].mIntConst += pins->mSrc[0].mIntConst;
						changed = true;
					}
				}
				break;
#endif
			}


			// Now kill all instructions that referenced the current destination as source, they are
			// not valid anymore

			int	dtemp = ins->mDst.mTemp;

			if (dtemp >= 0)
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (ltvalue[i] && ltvalue[i]->ReferencesTemp(dtemp))
						ltvalue[i] = nullptr;
				}

				if (!ins->UsesTemp(dtemp))
					ltvalue[dtemp] = ins;
			}
		}

#if _DEBUG
		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			assert(ins->mCode != IC_LOAD_TEMPORARY || ins->mSrc[0].mTemp >= 0);		
		}
#endif

		if (mTrueJump && mTrueJump->SimplifyIntegerNumeric(ltvalue, spareTemps))
			changed = true;

		if (mFalseJump && mFalseJump->SimplifyIntegerNumeric(ltvalue, spareTemps))
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, int& spareTemps, const GrowingVariableArray& staticVars)
{
	int i;

	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);
		ValueSet					lvalues(values);

		if (mLoopHead)
		{
			lvalues.FlushAll();
			ltvalue.Clear();
		}
#if 0
		else if (mNumEntries > 1)
		{
			lvalues.FlushAll();
			ltvalue.Clear();
		}
#endif
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				lvalues.Intersect(mMergeValues);
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (mMergeTValues[i] != ltvalue[i])
						ltvalue[i] = nullptr;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTValues = ltvalue;
				mMergeValues = lvalues;
				return;
			}
		}

		mVisited = true;

		tvalid.Clear();
		for (i = 0; i < ltvalue.Size(); i++)
		{
			if (ltvalue[i])
				tvalid += i;
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			// Normalize kommutative operands if one is constant

#if 1
			if (ins->mCode == IC_BINARY_OPERATOR &&
				(ins->mOperator == IA_MUL || ins->mOperator == IA_ADD || ins->mOperator == IA_AND || ins->mOperator == IA_OR || ins->mOperator == IA_XOR) &&
				ltvalue[ins->mSrc[1].mTemp] && ltvalue[ins->mSrc[0].mTemp] &&
				ltvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT && ltvalue[ins->mSrc[0].mTemp]->mCode != IC_CONSTANT)
			{
				InterOperand	op = ins->mSrc[0];
				ins->mSrc[0] = ins->mSrc[1];
				ins->mSrc[1] = op;
			}
#endif

#if 1
			if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && ins->mDst.mType == IT_INT16 && spareTemps + 1 < tvalid.Size())
			{
				InterInstruction* mi0 = ltvalue[ins->mSrc[0].mTemp], * mi1 = ltvalue[ins->mSrc[1].mTemp];

				if (mi0 && mi1 && mi1->mCode == IC_CONSTANT && mi0->mCode == IC_BINARY_OPERATOR && mi0->mOperator == IA_ADD)
				{
					InterInstruction* ai0 = ltvalue[mi0->mSrc[0].mTemp], * ai1 = ltvalue[mi0->mSrc[1].mTemp];
					if (ai0 && ai0->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction();
						nai->mCode = IC_BINARY_OPERATOR;
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi0->mSrc[1].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[1].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction();
						cai->mCode = IC_CONSTANT;
						cai->mDst.mTemp = spareTemps++;
						cai->mDst.mType = IT_INT16;
						cai->mConst.mIntConst = ai0->mConst.mIntConst * mi1->mConst.mIntConst;
						mInstructions.Insert(i, cai);

						ltvalue[cai->mDst.mTemp] = nullptr;

						ins->mOperator = IA_ADD;
						ins->mSrc[1].mTemp = nai->mDst.mTemp;
						ins->mSrc[0].mTemp = cai->mDst.mTemp;
					}
					else if (ai1 && ai1->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction();
						nai->mCode = IC_BINARY_OPERATOR;
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi0->mSrc[0].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[1].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction();
						cai->mCode = IC_CONSTANT;
						cai->mDst.mTemp = spareTemps++;
						cai->mDst.mType = IT_INT16;
						cai->mConst.mIntConst = ai1->mConst.mIntConst * mi1->mConst.mIntConst;
						mInstructions.Insert(i, cai);

						ltvalue[cai->mDst.mTemp] = nullptr;

						ins->mOperator = IA_ADD;
						ins->mSrc[1].mTemp = nai->mDst.mTemp;
						ins->mSrc[0].mTemp = cai->mDst.mTemp;
					}
				}
				else if (mi0 && mi1 && mi0->mCode == IC_CONSTANT && mi1->mCode == IC_BINARY_OPERATOR && mi1->mOperator == IA_ADD)
				{
					InterInstruction* ai0 = ltvalue[mi1->mSrc[0].mTemp], * ai1 = ltvalue[mi1->mSrc[1].mTemp];
					if (ai0 && ai0->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction();
						nai->mCode = IC_BINARY_OPERATOR;
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi1->mSrc[1].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[0].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction();
						cai->mCode = IC_CONSTANT;
						cai->mDst.mTemp = spareTemps++;
						cai->mDst.mType = IT_INT16;
						cai->mConst.mIntConst = ai0->mConst.mIntConst * mi0->mConst.mIntConst;
						mInstructions.Insert(i, cai);

						ltvalue[cai->mDst.mTemp] = nullptr;

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mTemp = nai->mDst.mTemp;
						ins->mSrc[1].mTemp = cai->mDst.mTemp;
					}
					else if (ai1 && ai1->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction();
						nai->mCode = IC_BINARY_OPERATOR;
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi1->mSrc[0].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[0].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction();
						cai->mCode = IC_CONSTANT;
						cai->mDst.mTemp = spareTemps++;
						cai->mDst.mType = IT_INT16;
						cai->mConst.mIntConst = ai1->mConst.mIntConst * mi0->mConst.mIntConst;
						mInstructions.Insert(i, cai);

						ltvalue[cai->mDst.mTemp] = nullptr;

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mTemp = nai->mDst.mTemp;
						ins->mSrc[1].mTemp = cai->mDst.mTemp;
					}
				}
			}
#endif
#if 1
			if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && ins->mDst.mType == IT_INT16 && spareTemps < tvalid.Size())
			{
				InterInstruction* mi0 = ltvalue[ins->mSrc[0].mTemp], * mi1 = ltvalue[ins->mSrc[1].mTemp];

				if (mi0 && mi1)
				{
					if (mi1->mCode == IC_CONSTANT && mi0->mCode == IC_BINARY_OPERATOR && mi0->mOperator == IA_ADD)
					{
						InterInstruction* ai0 = ltvalue[mi0->mSrc[0].mTemp], * ai1 = ltvalue[mi0->mSrc[1].mTemp];
						if (ai0 && ai1)
						{
							if (ai0 && ai0->mCode == IC_CONSTANT)
							{
								InterInstruction* cai = new InterInstruction();
								cai->mCode = IC_CONSTANT;
								cai->mDst.mTemp = spareTemps++;
								cai->mDst.mType = IT_INT16;
								cai->mConst.mIntConst = ai0->mConst.mIntConst + mi1->mConst.mIntConst;
								mInstructions.Insert(i, cai);

								ltvalue[cai->mDst.mTemp] = nullptr;

								ins->mSrc[1].mTemp = mi0->mSrc[1].mTemp;
								ins->mSrc[0].mTemp = cai->mDst.mTemp;
							}
							else if (ai1 && ai1->mCode == IC_CONSTANT)
							{
								InterInstruction* cai = new InterInstruction();
								cai->mCode = IC_CONSTANT;
								cai->mDst.mTemp = spareTemps++;
								cai->mDst.mType = IT_INT16;
								cai->mConst.mIntConst = ai1->mConst.mIntConst + mi1->mConst.mIntConst;
								mInstructions.Insert(i, cai);

								ltvalue[cai->mDst.mTemp] = nullptr;

								ins->mSrc[1].mTemp = mi0->mSrc[0].mTemp;
								ins->mSrc[0].mTemp = cai->mDst.mTemp;
							}
						}
					}
					else if (mi0->mCode == IC_CONSTANT && mi1->mCode == IC_BINARY_OPERATOR && mi1->mOperator == IA_ADD)
					{
						InterInstruction* ai0 = ltvalue[mi1->mSrc[0].mTemp], * ai1 = ltvalue[mi1->mSrc[1].mTemp];
						if (ai0 && ai1)
						{
							if (ai0 && ai0->mCode == IC_CONSTANT)
							{
								InterInstruction* cai = new InterInstruction();
								cai->mCode = IC_CONSTANT;
								cai->mDst.mTemp = spareTemps++;
								cai->mDst.mType = IT_INT16;
								cai->mConst.mIntConst = ai0->mConst.mIntConst + mi0->mConst.mIntConst;
								mInstructions.Insert(i, cai);

								ltvalue[cai->mDst.mTemp] = nullptr;

								ins->mSrc[1].mTemp = mi1->mSrc[1].mTemp;
								ins->mSrc[0].mTemp = cai->mDst.mTemp;
							}
							else if (ai1 && ai1->mCode == IC_CONSTANT)
							{
								InterInstruction* cai = new InterInstruction();
								cai->mCode = IC_CONSTANT;
								cai->mDst.mTemp = spareTemps++;
								cai->mDst.mType = IT_INT16;
								cai->mConst.mIntConst = ai1->mConst.mIntConst + mi0->mConst.mIntConst;
								mInstructions.Insert(i, cai);

								ltvalue[cai->mDst.mTemp] = nullptr;

								ins->mSrc[1].mTemp = mi1->mSrc[0].mTemp;
								ins->mSrc[0].mTemp = cai->mDst.mTemp;
							}
						}
					}
				}
			}

			if (ins->mCode == IC_LEA && spareTemps < tvalid.Size())
			{
				InterInstruction* li0 = ltvalue[ins->mSrc[0].mTemp], * li1 = ltvalue[ins->mSrc[1].mTemp];

				if (li0 && li1)
				{
					if (li1->mCode != IC_CONSTANT && li0->mCode == IC_BINARY_OPERATOR && li0->mOperator == IA_ADD)
					{
						InterInstruction* ai0 = ltvalue[li0->mSrc[0].mTemp], * ai1 = ltvalue[li0->mSrc[1].mTemp];
						if (ai0 && ai1 && ai0->mCode == IC_CONSTANT && ai0->mConst.mIntConst >= 0)
						{
							InterInstruction* nai = new InterInstruction();
							nai->mCode = IC_LEA;
							nai->mSrc[1].mMemory = IM_INDIRECT;
							nai->mSrc[0].mTemp = li0->mSrc[1].mTemp;
							nai->mSrc[0].mType = IT_INT16;
							nai->mSrc[1].mTemp = ins->mSrc[1].mTemp;
							nai->mSrc[1].mType = IT_POINTER;
							nai->mDst.mTemp = spareTemps++;
							nai->mDst.mType = IT_POINTER;
							mInstructions.Insert(i, nai);

							ltvalue[nai->mDst.mTemp] = nullptr;

							ins->mSrc[1].mTemp = nai->mDst.mTemp;
							ins->mSrc[0].mTemp = li0->mSrc[0].mTemp;
						}
						else if (ai1 && ai1->mCode == IC_CONSTANT)
						{
						}
					}
					else if (li0->mCode == IC_CONSTANT && li1->mCode == IC_LEA)
					{
						InterInstruction* ai0 = ltvalue[li1->mSrc[0].mTemp], * ai1 = ltvalue[li1->mSrc[1].mTemp];
						if (ai0 && ai1 && ai0->mCode == IC_CONSTANT && ai0->mConst.mIntConst >= 0)
						{
							InterInstruction* cai = new InterInstruction();
							cai->mCode = IC_CONSTANT;
							cai->mDst.mTemp = spareTemps++;
							cai->mDst.mType = IT_INT16;
							cai->mConst.mIntConst = ai0->mConst.mIntConst + li0->mConst.mIntConst;
							mInstructions.Insert(i, cai);

							ins->mSrc[0].mTemp = cai->mDst.mTemp;
							ins->mSrc[1].mTemp = li1->mSrc[1].mTemp;

							ltvalue[cai->mDst.mTemp] = nullptr;
						}
					}
				}
			}

#endif
			lvalues.UpdateValue(mInstructions[i], ltvalue, aliasedLocals, aliasedParams, staticVars);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams, spareTemps, staticVars);
		if (mFalseJump) mFalseJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams, spareTemps, staticVars);
	}
}

void InterCodeBasicBlock::PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid, const GrowingVariableArray& staticVars)
{
	int i;

	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);

		if (mLoopHead)
		{
			ltvalue.Clear();
		}
#if 0
		else if (mNumEntries > 1)
		{
			lvalues.FlushAll();
			ltvalue.Clear();
		}
#endif
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (mMergeTValues[i] != ltvalue[i])
						ltvalue[i] = nullptr;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTValues = ltvalue;
				return;
			}
		}

		mVisited = true;

		tvalid.Clear();
		for (i = 0; i < ltvalue.Size(); i++)
		{
			if (ltvalue[i])
				tvalid += i;
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			CheckValueUsage(mInstructions[i], ltvalue, staticVars);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid, staticVars);
		if (mFalseJump) mFalseJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid, staticVars);
	}
}


bool InterCodeBasicBlock::EliminateDeadBranches(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mInstructions.Size() > 0)
		{
			if (mInstructions[mInstructions.Size() - 1]->mCode == IC_JUMP && mFalseJump)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = nullptr;
				changed = true;
			}
			else if (mInstructions[mInstructions.Size() - 1]->mCode == IC_JUMPF)
			{
				mInstructions[mInstructions.Size() - 1]->mCode = IC_JUMP;
				mTrueJump->mNumEntries--;
				mTrueJump = mFalseJump;
				mFalseJump = nullptr;
				changed = true;
			}
		}

		if (mTrueJump && mTrueJump->EliminateDeadBranches()) changed = true;
		if (mFalseJump && mFalseJump->EliminateDeadBranches()) changed = true;
	}

	return changed;
}

static void Union(GrowingIntArray& table, int i, int j)
{
	int k, l;

	k = table[j];
	while (j != k)
	{
		l = table[k];
		table[j] = l;
		j = k; k = l;
	}

	table[j] = table[i];
}

static int Find(GrowingIntArray& table, int i)
{
	int j, k, l;

	j = i;
	k = table[j];
	while (j != k)
	{
		l = table[k];
		table[j] = l;
		j = k; k = l;
	}

	return j;
}

bool InterCodeBasicBlock::LoadStoreForwarding(const GrowingInstructionPtrArray& tvalue, const GrowingVariableArray& staticVars)
{
	bool	changed = false;

	if (!mVisited)
	{
		if (!mLoopHead)
		{
			if (mNumEntries > 0)
			{
				if (mNumEntered == 0)
					mLoadStoreInstructions = tvalue;
				else
				{
					int i = 0;
					while (i < mLoadStoreInstructions.Size())
					{
						InterInstruction* ins(mLoadStoreInstructions[i]);
						InterInstruction* nins = nullptr;

						int j = tvalue.IndexOf(ins);
						if (j != -1)
							nins = ins;
						else
						{
							if (ins->mCode == IC_LOAD)
							{
								j = 0;
								while (j < tvalue.Size() && !SameMem(ins->mSrc[0], tvalue[j]))
									j++;
								if (j < tvalue.Size())
								{
									if (tvalue[j]->mCode == IC_LOAD && tvalue[j]->mDst.IsEqual(ins->mDst))
										nins = ins;
									else if (tvalue[j]->mCode == IC_STORE && tvalue[j]->mSrc[0].IsEqual(ins->mDst))
										nins = ins;
								}
							}
							else if (ins->mCode == IC_STORE)
							{
								j = 0;
								while (j < tvalue.Size() && !SameMem(ins->mSrc[1], tvalue[j]))
									j++;
								if (j < tvalue.Size())
								{
									if (tvalue[j]->mCode == IC_LOAD && tvalue[j]->mDst.IsEqual(ins->mSrc[0]))
										nins = tvalue[j];
									else if (tvalue[j]->mCode == IC_STORE && tvalue[j]->mSrc[0].IsEqual(ins->mSrc[0]))
										nins = ins;
								}
							}
						}

						if (nins)
							mLoadStoreInstructions[i++] = nins;
						else
							mLoadStoreInstructions.Remove(i);
					}
				}

				mNumEntered++;

				if (mNumEntered < mNumEntries)
				{
					return false;
				}
			}
		}
#if 1
		else if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
		{
			mLoadStoreInstructions = tvalue;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins(mInstructions[i]);
				if (ins->mDst.mTemp >= 0)
				{
					int j = 0;
					while (j < mLoadStoreInstructions.Size())
					{
						if (mLoadStoreInstructions[j]->ReferencesTemp(ins->mDst.mTemp) || CollidingMem(ins, mLoadStoreInstructions[j], staticVars))
							mLoadStoreInstructions.Remove(j);
						else
							j++;
					}
				}
			}
		}
#endif
		else
			mLoadStoreInstructions.SetSize(0);

		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			InterInstruction	*	nins = nullptr;
			bool					flushMem = false;

			if (ins->mCode == IC_LOAD)
			{
				if (!ins->mVolatile)
				{
					int	j = 0;
					while (j < mLoadStoreInstructions.Size() && !SameMem(ins->mSrc[0], mLoadStoreInstructions[j]))
						j++;
					if (j < mLoadStoreInstructions.Size())
					{
						InterInstruction* lins = mLoadStoreInstructions[j];
						if (lins->mCode == IC_LOAD)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							ins->mSrc[0] = lins->mDst;
							ins->mNumOperands = 1;
							assert(ins->mSrc[0].mTemp >= 0);
							changed = true;
						}
						else if (lins->mCode == IC_STORE)
						{
							if (lins->mSrc[0].mTemp < 0)
							{
								ins->mCode = IC_CONSTANT;
								ins->mConst = lins->mSrc[0];
								changed = true;
							}
							else
							{
								ins->mCode = IC_LOAD_TEMPORARY;
								ins->mSrc[0] = lins->mSrc[0];
								ins->mNumOperands = 1;
								assert(ins->mSrc[0].mTemp >= 0);
								changed = true;
							}
						}
					}
					else
						nins = ins;
				}
			}
			else if (ins->mCode == IC_STORE)
			{
				int	j = 0, k = 0;

				while (j < mLoadStoreInstructions.Size() && !SameMem(ins->mSrc[1], mLoadStoreInstructions[j]))
					j++;

				if (!ins->mVolatile && j < mLoadStoreInstructions.Size() && mLoadStoreInstructions[j]->mCode == IC_LOAD && ins->mSrc[0].mTemp == mLoadStoreInstructions[j]->mDst.mTemp)
				{
					ins->mCode = IC_NONE;
					ins->mNumOperands = 0;
					changed = true;
				}
				else
				{
					j = 0;
					while (j < mLoadStoreInstructions.Size())
					{
						if (!CollidingMem(ins->mSrc[1], mLoadStoreInstructions[j], staticVars))
							mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
						j++;
					}
					mLoadStoreInstructions.SetSize(k);
				}

				if (!ins->mVolatile)
					nins = ins;
			}
			else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
				flushMem = true;
			else if (ins->mCode == IC_LEA || ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR)
			{
				int	j = 0;
				while (j < mLoadStoreInstructions.Size() && !SameInstruction(ins, mLoadStoreInstructions[j]))
					j++;
				if (j < mLoadStoreInstructions.Size())
				{
					InterInstruction* lins = mLoadStoreInstructions[j];
					assert(lins->mDst.mTemp >= 0);
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0] = lins->mDst;
					ins->mNumOperands = 1;
					changed = true;
				}
				else
					nins = ins;
			}
			else if (HasSideEffect(ins->mCode))
				flushMem = true;			

			{
				int	j = 0, k = 0, t = ins->mDst.mTemp;

				while (j < mLoadStoreInstructions.Size())
				{
					if (flushMem && (mLoadStoreInstructions[j]->mCode == IC_LOAD || mLoadStoreInstructions[j]->mCode == IC_STORE))
						;
					else if (t < 0)
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					else if (t != mLoadStoreInstructions[j]->mDst.mTemp)
					{
						int l = 0;
						while (l < mLoadStoreInstructions[j]->mNumOperands && t != mLoadStoreInstructions[j]->mSrc[l].mTemp)
							l++;
						if (l == mLoadStoreInstructions[j]->mNumOperands)
							mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					}
					j++;
				}
				mLoadStoreInstructions.SetSize(k);
			}

			if (nins)
				mLoadStoreInstructions.Push(nins);
		}

		if (mTrueJump && mTrueJump->LoadStoreForwarding(mLoadStoreInstructions, staticVars))
			changed = true;
		if (mFalseJump && mFalseJump->LoadStoreForwarding(mLoadStoreInstructions, staticVars))
			changed = true;
	}

	return changed;
}


void InterCodeBasicBlock::LocalRenameRegister(const GrowingIntArray& renameTable, int& num)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mEntryRenameTable.SetSize(renameTable.Size());
		mExitRenameTable.SetSize(renameTable.Size());

		for (i = 0; i < renameTable.Size(); i++)
		{
			if (mEntryRequiredTemps[i])
			{
				if (renameTable[i] < 0)
				{
					mExitRenameTable[i] = mEntryRenameTable[i] = num++;
				}
				else
				{
					mEntryRenameTable[i] = renameTable[i];
					mExitRenameTable[i] = renameTable[i];
				}
			}
			else
			{
				mEntryRenameTable[i] = -1;
				mExitRenameTable[i] = -1;
			}
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->LocalRenameRegister(mExitRenameTable, num);
		}

		if (mTrueJump) mTrueJump->LocalRenameRegister(mExitRenameTable, num);
		if (mFalseJump) mFalseJump->LocalRenameRegister(mExitRenameTable, num);
	}
}

void InterCodeBasicBlock::BuildGlobalRenameRegisterTable(const GrowingIntArray& renameTable, GrowingIntArray& globalRenameTable)
{
	int i;

	for (i = 0; i < renameTable.Size(); i++)
	{
		if (renameTable[i] >= 0 && mEntryRenameTable[i] >= 0 && renameTable[i] != mEntryRenameTable[i])
		{
			Union(globalRenameTable, renameTable[i], mEntryRenameTable[i]);
		}
	}

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalRenameRegisterTable(mExitRenameTable, globalRenameTable);
		if (mFalseJump) mFalseJump->BuildGlobalRenameRegisterTable(mExitRenameTable, globalRenameTable);
	}
}

void InterCodeBasicBlock::GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->GlobalRenameRegister(renameTable, temporaries);
		}

		if (mTrueJump) mTrueJump->GlobalRenameRegister(renameTable, temporaries);
		if (mFalseJump) mFalseJump->GlobalRenameRegister(renameTable, temporaries);
	}
}

void InterCodeBasicBlock::BuildCollisionTable(NumberSet* collisionSets)
{
	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredTemps(mExitRequiredTemps);
		int i, j;

		for (i = 0; i < mExitRequiredTemps.Size(); i++)
		{
			if (mExitRequiredTemps[i])
			{
				for (j = 0; j < mExitRequiredTemps.Size(); j++)
				{
					if (mExitRequiredTemps[j])
					{
						collisionSets[i] += j;
					}
				}
			}
		}

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			mInstructions[i]->BuildCollisionTable(requiredTemps, collisionSets);
		}

		if (mTrueJump) mTrueJump->BuildCollisionTable(collisionSets);
		if (mFalseJump) mFalseJump->BuildCollisionTable(collisionSets);
	}
}

void InterCodeBasicBlock::ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->ReduceTemporaries(renameTable, temporaries);
		}

		if (mTrueJump) mTrueJump->ReduceTemporaries(renameTable, temporaries);
		if (mFalseJump) mFalseJump->ReduceTemporaries(renameTable, temporaries);
	}
}

void InterCodeBasicBlock::MapVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			bool	found = false;

			switch (mInstructions[i]->mCode)
			{
			case IC_CONSTANT:
				if (mInstructions[i]->mDst.mType == IT_POINTER)
				{
					if (mInstructions[i]->mConst.mMemory == IM_LOCAL)
					{
						localVars[mInstructions[i]->mConst.mVarIndex]->mUsed = true;
					}
				}
				break;

			case IC_STORE:
				if (mInstructions[i]->mSrc[1].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[1].mVarIndex]->mUsed = true;
				}
				break;

			case IC_LOAD:
			case IC_CALL_NATIVE:
				if (mInstructions[i]->mSrc[0].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[0].mVarIndex]->mUsed = true;
				}
				break;

			case IC_COPY:
			case IC_STRCPY:
				if (mInstructions[i]->mSrc[0].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[0].mVarIndex]->mUsed = true;
				}
				if (mInstructions[i]->mSrc[1].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[1].mVarIndex]->mUsed = true;
				}
				break;

			}
		}

		if (mTrueJump) mTrueJump->MapVariables(globalVars, localVars);
		if (mFalseJump) mFalseJump->MapVariables(globalVars, localVars);
	}
}

void InterCodeBasicBlock::MarkRelevantStatics(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			const InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_LOAD)
			{
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL)
					ins->mSrc[0].mLinkerObject->mFlags |= LOBJF_RELEVANT;
			}
			else if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL)
					ins->mSrc[1].mLinkerObject->mFlags |= LOBJF_RELEVANT;
			}
			else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER)
			{
				if (ins->mConst.mMemory == IM_GLOBAL)
					ins->mConst.mLinkerObject->mFlags |= LOBJF_RELEVANT;
			}
			else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
			{
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL)
					ins->mSrc[0].mLinkerObject->mFlags |= LOBJF_RELEVANT;
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL)
					ins->mSrc[1].mLinkerObject->mFlags |= LOBJF_RELEVANT;
			}
		}

		if (mTrueJump) mTrueJump->MarkRelevantStatics();
		if (mFalseJump) mFalseJump->MarkRelevantStatics();
	}
}

bool InterCodeBasicBlock::CanMoveInstructionDown(int si, int ti) const
{
	InterInstruction* ins = mInstructions[si];

	if (ins->mCode == IC_LOAD)
	{
		for (int i = si + 1; i < ti; i++)
			if (!CanBypassLoad(ins, mInstructions[i]))
				return false;
	}
	else if (ins->mCode == IC_STORE)
	{
		return false;
	}
	else if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE || ins->mCode == IC_COPY || ins->mCode == IC_PUSH_FRAME || ins->mCode == IC_POP_FRAME ||
		ins->mCode == IC_RETURN || ins->mCode == IC_RETURN_STRUCT || ins->mCode == IC_RETURN_VALUE)
		return false;
	else
	{
		for (int i = si + 1; i < ti; i++)
			if (!CanBypass(ins, mInstructions[i]))
				return false;
	}

	return true;
}

bool InterCodeBasicBlock::CanMoveInstructionBehindBlock(int ii) const
{
	return CanMoveInstructionDown(ii, mInstructions.Size());
}

bool InterCodeBasicBlock::CanMoveInstructionBeforeBlock(int ii, const InterInstruction* ins) const
{
	if (ins->mCode == IC_LOAD)
	{
		for (int i = 0; i < ii; i++)
			if (!CanBypassLoadUp(ins, mInstructions[i]))
				return false;
	}
	else if (ins->mCode == IC_STORE)
	{
		for (int i = 0; i < ii; i++)
			if (!CanBypassStore(ins, mInstructions[i]))
				return false;
	}
	else if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE || ins->mCode == IC_COPY || ins->mCode == IC_PUSH_FRAME || ins->mCode == IC_POP_FRAME ||
		ins->mCode == IC_RETURN || ins->mCode == IC_RETURN_STRUCT || ins->mCode == IC_RETURN_VALUE)
		return false;
	else
	{
		for (int i = 0; i < ii; i++)
			if (!CanBypassUp(ins, mInstructions[i]))
				return false;
	}

	return true;
}

bool InterCodeBasicBlock::CanMoveInstructionBeforeBlock(int ii) const
{
	return CanMoveInstructionBeforeBlock(ii, mInstructions[ii]);
}

bool InterCodeBasicBlock::MergeCommonPathInstructions(void)
{
	bool changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump && mTrueJump->mNumEntries == 1 && mFalseJump->mNumEntries == 1 && mTrueJump->mInstructions.Size() && mFalseJump->mInstructions.Size())
		{
			int	ti = 0;
			while (ti < mTrueJump->mInstructions.Size() && !changed)
			{
				InterInstruction* tins = mTrueJump->mInstructions[ti];
				InterInstruction* nins = (ti + 1 < mTrueJump->mInstructions.Size()) ? mTrueJump->mInstructions[ti + 1] : nullptr;

				if (tins->mCode != IC_BRANCH && tins->mCode != IC_JUMP && !(nins && nins->mCode == IC_BRANCH && tins->mDst.mTemp == nins->mSrc[0].mTemp))
				{
					int	fi = 0;
					while (fi < mFalseJump->mInstructions.Size() && !tins->IsEqualSource(mFalseJump->mInstructions[fi]))
						fi++;

					if (fi < mFalseJump->mInstructions.Size())
					{
						InterInstruction* fins = mFalseJump->mInstructions[fi];

						if ((tins->mDst.mTemp == -1 || !mFalseJump->mEntryRequiredTemps[tins->mDst.mTemp]) &&
							(fins->mDst.mTemp == -1 || !mTrueJump->mEntryRequiredTemps[fins->mDst.mTemp]))
						{
							if (mTrueJump->CanMoveInstructionBeforeBlock(ti) && mFalseJump->CanMoveInstructionBeforeBlock(fi))
							{
								int	tindex = mInstructions.Size() - 1;
								if (mInstructions.Size() >= 2 && mInstructions[tindex - 1]->mDst.mTemp == mInstructions[tindex]->mSrc[0].mTemp && CanBypassUp(tins, mInstructions[tindex - 1]))
									tindex--;

								mInstructions.Insert(tindex, tins);
								tindex++;
								if (tins->mDst.mTemp != -1)
								{
									if (fins->mDst.mTemp != tins->mDst.mTemp)
									{
										InterInstruction* nins = new InterInstruction();
										nins->mCode = IC_LOAD_TEMPORARY;
										nins->mDst.mTemp = fins->mDst.mTemp;
										nins->mDst.mType = fins->mDst.mType;
										nins->mSrc[0].mTemp = tins->mDst.mTemp;
										nins->mSrc[0].mType = tins->mDst.mType;
										assert(nins->mSrc[0].mTemp >= 0);
										mInstructions.Insert(tindex, nins);
									}
								}
								mTrueJump->mInstructions.Remove(ti);
								mFalseJump->mInstructions.Remove(fi);
								changed = true;
							}
						}
					}
				}

				ti++;
			}
		}

		if (mTrueJump && mTrueJump->MergeCommonPathInstructions())
			changed = true;
		if (mFalseJump && mFalseJump->MergeCommonPathInstructions())
			changed = true;
	}

	return changed;
}

static void CollectDominatorPath(InterCodeBasicBlock* block, InterCodeBasicBlock* dom, GrowingInterCodeBasicBlockPtrArray& blocks)
{
	if (blocks.IndexOf(block) != -1)
		return;
	if (block != dom)
	{
		blocks.Push(block);
		for (int i = 0; i < block->mEntryBlocks.Size(); i++)
			CollectDominatorPath(block->mEntryBlocks[i], dom, blocks);
	}
}

static bool CanMoveInstructionBeforePath(const GrowingInterCodeBasicBlockPtrArray& blocks, const InterInstruction* ins)
{
	for (int i = 0; i < blocks.Size(); i++)
		if (!blocks[i]->CanMoveInstructionBeforeBlock(blocks[i]->mInstructions.Size(), ins))
			return false;
	return true;
}

bool InterCodeBasicBlock::MoveTrainCrossBlock(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mDominator)
		{
			GrowingInterCodeBasicBlockPtrArray	path(nullptr);

			CollectDominatorPath(this, mDominator, path);

			if ((!mDominator->mTrueJump || path.IndexOf(mDominator->mTrueJump) != -1) &&
				(!mDominator->mFalseJump || path.IndexOf(mDominator->mFalseJump) != -1) &&
				(!mTrueJump || path.IndexOf(mTrueJump) == -1) &&
				(!mFalseJump || path.IndexOf(mFalseJump) == -1))
			{
				int i = 1;
				while (i < path.Size() &&
					(!path[i]->mTrueJump || path.IndexOf(path[i]->mTrueJump) != -1) &&
					(!path[i]->mFalseJump || path.IndexOf(path[i]->mFalseJump) != -1))
					i++;
				if (i == path.Size())
				{
					path.Remove(0);

					int	i = 0;
					while (i < mInstructions.Size())
					{
						FastNumberSet	nset(mEntryRequiredTemps.Size());

						InterInstruction* ins(mInstructions[i]);
						if (ins->mCode == IC_STORE)
						{
							for (int k = 0; k < ins->mNumOperands; k++)
							{
								if (ins->mSrc[k].mTemp >= 0)
									nset += ins->mSrc[k].mTemp;
							}

							int j = i;
							while (j > 0 && mInstructions[j - 1]->mDst.mTemp >= 0 && nset[mInstructions[j - 1]->mDst.mTemp])
							{
								j--;
								InterInstruction* nins(mInstructions[j]);

								for (int k = 0; k < nins->mNumOperands; k++)
								{
									if (nins->mSrc[k].mTemp >= 0)
										nset += nins->mSrc[k].mTemp;
								}
							}

							int k = j;
							while (k <= i && CanMoveInstructionBeforeBlock(j, mInstructions[k]) && CanMoveInstructionBeforePath(path, mInstructions[k]))
								k++;

							if (k > i)
							{
								for (int k = j; k <= i; k++)
									mDominator->mInstructions.Insert(mDominator->mInstructions.Size() - 1, mInstructions[k]);
								mInstructions.Remove(j, i - j + 1);
								i = j - 1;
								changed = true;
							}
						}

						i++;
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->MoveTrainCrossBlock())
			changed = true;
		if (mFalseJump && mFalseJump->MoveTrainCrossBlock())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::ForwardDiamondMovedTemp(void)
{
	bool changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mTrueJump == mFalseJump)
		{
			mFalseJump = nullptr;
			mInstructions[mInstructions.Size() - 1]->mCode = IC_JUMP;
			mInstructions[mInstructions.Size() - 1]->mNumOperands = 0;
			mTrueJump->mNumEntries--;
			changed = true;
		}

		if (mTrueJump && mFalseJump)
		{
			InterCodeBasicBlock* tblock = nullptr, * fblock = nullptr;

			if (mTrueJump != mFalseJump && mTrueJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump && mTrueJump->mFalseJump == mFalseJump->mFalseJump)
			{
				if (mTrueJump->mInstructions.Size() == 1)
				{
					tblock = mTrueJump;
					fblock = mFalseJump;
				}
				else if (mFalseJump->mInstructions.Size() == 1)
				{
					fblock = mTrueJump;
					tblock = mFalseJump;
				}

				if (fblock && tblock)
				{
					if (tblock->mInstructions[0]->IsEqual(fblock->mInstructions.Last()) && !fblock->mLocalModifiedTemps[tblock->mInstructions[0]->mSrc[0].mTemp])
					{
						fblock->mTrueJump = tblock;
						fblock->mFalseJump = nullptr;
						fblock->mInstructions[fblock->mInstructions.Size() - 1]->mCode = IC_JUMP;
						fblock->mInstructions[fblock->mInstructions.Size() - 1]->mNumOperands = 0;
						tblock->mNumEntries++;
						tblock->mFalseJump->mNumEntries--;
						tblock->mTrueJump->mNumEntries--;
						changed = true;
					}
				}

				fblock = nullptr;
				tblock = nullptr;
			}

			if (!mTrueJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump)
			{
				tblock = mTrueJump;
				fblock = mFalseJump;
			}
			else if (!mFalseJump->mFalseJump && mFalseJump->mTrueJump == mTrueJump)
			{
				fblock = mTrueJump;
				tblock = mFalseJump;
			}

			if (fblock && tblock)
			{
				if (tblock->mNumEntries == 1 && fblock->mNumEntries == 2)
				{
					if (tblock->mInstructions.Size() == 2 && tblock->mInstructions[0]->mCode == IC_LEA && tblock->mInstructions[0]->mSrc[0].mTemp < 0 && tblock->mInstructions[0]->mSrc[1].mTemp >= 0)
					{
						InterInstruction* lins = tblock->mInstructions[0];

						// Single "lea temp, imm" in diamond
						int i = mInstructions.Size();
						while (i > 0 && mInstructions[i - 1]->mDst.mTemp != lins->mSrc[1].mTemp)
							i--;

						if (i > 0)
						{
							i--;
							InterInstruction* tins = mInstructions[i];

							if (mInstructions[i]->mCode == IC_LOAD_TEMPORARY)
							{
								int		offset = 0;
								bool	fail = false;
								for (int j = i + 1; j < mInstructions.Size(); j++)
								{
									if (mInstructions[j]->mDst.mTemp == tins->mSrc[0].mTemp)
									{
										if (mInstructions[j]->mCode == IC_LEA && mInstructions[j]->mSrc[1].mTemp == tins->mSrc[0].mTemp && mInstructions[j]->mSrc[0].mTemp < 0)
										{
											offset += mInstructions[j]->mSrc[0].mIntConst;
										}
										else
											fail = true;
									}
								}

								if (!fail)
								{
									mExitRequiredTemps += tins->mSrc[0].mTemp;
									tblock->mEntryRequiredTemps += tins->mSrc[0].mTemp;

									lins->mSrc[1].mTemp = tins->mSrc[0].mTemp;
									lins->mSrc[0].mIntConst -= offset;
									changed = true;
								}
							}

						}
					}

					for (int i = mInstructions.Size() - 1; i >= 0; i--)
					{
						InterInstruction* mins = mInstructions[i];

						if (mins->mCode == IC_LOAD_TEMPORARY)
						{
							int	ttemp = mins->mDst.mTemp;
							int stemp = mins->mSrc[0].mTemp;

							if (!IsTempModifiedOnPath(ttemp, i + 1) && !IsTempModifiedOnPath(stemp, i + 1) && !tblock->mExitRequiredTemps[stemp])
							{
								int	j = 0;
								while (j < tblock->mInstructions.Size() &&
									tblock->mInstructions[j]->mDst.mTemp != ttemp &&
									tblock->mInstructions[j]->mDst.mTemp != stemp)
								{
									j++;
								}

								if (j < tblock->mInstructions.Size() && tblock->mInstructions[j]->mDst.mTemp == ttemp)
								{
									if (!tblock->IsTempModifiedOnPath(stemp, j + 1))
									{
										tblock->mInstructions[j]->mDst.mTemp = stemp;

										InterInstruction* nins = new InterInstruction();
										nins->mCode = IC_LOAD_TEMPORARY;
										nins->mDst.mTemp = ttemp;
										nins->mDst.mType = mins->mDst.mType;
										nins->mSrc[0].mTemp = stemp;
										nins->mSrc[0].mType = mins->mDst.mType;
										assert(nins->mSrc[0].mTemp >= 0);
										fblock->mInstructions.Insert(0, nins);

										tblock->mExitRequiredTemps += stemp;

										changed = true;
									}
								}
							}
						}
#if 1
						else if ((mins->mCode == IC_BINARY_OPERATOR || mins->mCode == IC_LEA) && (mins->mSrc[1].mTemp < 0 || mins->mSrc[0].mTemp < 0) || mins->mCode == IC_UNARY_OPERATOR || mins->mCode == IC_CONVERSION_OPERATOR)
						{
							int	ttemp = mins->mDst.mTemp;
							int stemp = ((mins->mCode == IC_BINARY_OPERATOR || mins->mCode == IC_LEA) && mins->mSrc[0].mTemp < 0) ? mins->mSrc[1].mTemp : mins->mSrc[0].mTemp;

							if (!tblock->mLocalRequiredTemps[ttemp] && !tblock->mLocalModifiedTemps[stemp] && !tblock->mLocalModifiedTemps[ttemp] && !IsTempReferencedOnPath(ttemp, i + 1) && !IsTempModifiedOnPath(stemp, i + 1))
							{
								tblock->mEntryRequiredTemps += stemp;
								tblock->mExitRequiredTemps += stemp;

								fblock->mEntryRequiredTemps += stemp;
								mExitRequiredTemps += stemp;

								fblock->mInstructions.Insert(0, mins);
								mInstructions.Remove(i);

								changed = true;
							}
						}
#endif
						else if (mins->mCode == IC_LOAD)
						{
							int	ttemp = mins->mDst.mTemp;
							int stemp = mins->mSrc[0].mTemp;

							if (!tblock->mLocalRequiredTemps[ttemp] && (stemp < 0 || !tblock->mLocalModifiedTemps[stemp]) && 
								!tblock->mLocalModifiedTemps[ttemp] && !IsTempReferencedOnPath(ttemp, i + 1) && (stemp < 0 || !IsTempModifiedOnPath(stemp, i + 1)) &&
								CanMoveInstructionBehindBlock(i))
							{
								if (stemp >= 0)
								{
									tblock->mEntryRequiredTemps += stemp;
									tblock->mExitRequiredTemps += stemp;

									fblock->mEntryRequiredTemps += stemp;
									mExitRequiredTemps += stemp;
								}

								fblock->mInstructions.Insert(0, mins);
								mInstructions.Remove(i);

								changed = true;
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->ForwardDiamondMovedTemp())
			changed = true;
		if (mFalseJump && mFalseJump->ForwardDiamondMovedTemp())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::IsTempModifiedOnPath(int temp, int at) const
{
	while (at < mInstructions.Size())
	{
		if (mInstructions[at]->mDst.mTemp == temp)
			return true;
		at++;
	}

	return false;
}

bool InterCodeBasicBlock::IsTempReferencedOnPath(int temp, int at) const
{
	while (at < mInstructions.Size())
	{
		const InterInstruction	* ins = mInstructions[at];

		if (ins->mDst.mTemp == temp)
			return true;
		for (int i = 0; i < ins->mNumOperands; i++)
			if (ins->mSrc[i].mTemp == temp)
				return true;

		at++;
	}

	return false;
}

bool InterCodeBasicBlock::PushSinglePathResultInstructions(void)
{
	int i;

	bool changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump)
		{
			InterCodeBasicBlock* joinedBlock = nullptr;


			NumberSet	trueExitRequiredTemps(mTrueJump->mEntryRequiredTemps), falseExitRequiredTems(mFalseJump->mEntryRequiredTemps);
			NumberSet	providedTemps(mExitRequiredTemps.Size()), requiredTemps(mExitRequiredTemps.Size());

			if (mTrueJump->mTrueJump && mFalseJump->mTrueJump && !mTrueJump->mFalseJump && !mFalseJump->mFalseJump &&
				mTrueJump->mNumEntries == 1 && mFalseJump->mNumEntries == 1 &&
				mTrueJump->mTrueJump == mFalseJump->mTrueJump && mTrueJump->mTrueJump->mNumEntries == 2)
			{
				joinedBlock = mTrueJump->mTrueJump;
			}

			bool	hadStore = false;

			int i = mInstructions.Size();
			while (i > 0)
			{
				i--;
				InterInstruction* ins(mInstructions[i]);

				int		dtemp = ins->mDst.mTemp;
				bool	moved = false;

				if (dtemp >= 0 && !providedTemps[dtemp] && !requiredTemps[dtemp])
				{
					int j = 0;
					while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || !(providedTemps[ins->mSrc[j].mTemp] || IsTempModifiedOnPath(ins->mSrc[j].mTemp, i + 1))))
						j++;

					if (j == ins->mNumOperands && IsMoveable(ins->mCode) && CanMoveInstructionBehindBlock(i))
					{
						if (mTrueJump->mNumEntries == 1 && trueExitRequiredTemps[dtemp] && !falseExitRequiredTems[dtemp])
						{
							for (int j = 0; j < ins->mNumOperands; j++)
							{
								if (ins->mSrc[j].mTemp >= 0)
									trueExitRequiredTemps += ins->mSrc[j].mTemp;
							}
							mTrueJump->mInstructions.Insert(0, ins);
							mInstructions.Remove(i);
							moved = true;
							changed = true;
						}
						else if (mFalseJump->mNumEntries == 1 && !trueExitRequiredTemps[dtemp] && falseExitRequiredTems[dtemp])
						{
							for (int j = 0; j < ins->mNumOperands; j++)
							{
								if (ins->mSrc[j].mTemp >= 0)
									falseExitRequiredTems += ins->mSrc[j].mTemp;
							}
							mFalseJump->mInstructions.Insert(0, ins);
							mInstructions.Remove(i);
							moved = true;
							changed = true;
						}
#if 1
						else if (joinedBlock && !HasSideEffect(ins->mCode) &&
							!mFalseJump->mLocalUsedTemps[dtemp] && !mFalseJump->mLocalModifiedTemps[dtemp] &&
							!mTrueJump->mLocalUsedTemps[dtemp] && !mTrueJump->mLocalModifiedTemps[dtemp])
						{
							int j = 0;
							while (j < ins->mNumOperands && !(ins->mSrc[j].mTemp >= 0 && (mFalseJump->mLocalModifiedTemps[dtemp] || mTrueJump->mLocalModifiedTemps[dtemp])))
								j++;

							if (j == ins->mNumOperands)
							{
								if (ins->mCode == IC_LOAD)
								{
									j = 0;
									while (j < mTrueJump->mInstructions.Size() && CanBypassLoad(ins, mTrueJump->mInstructions[j]))
										j++;
								}
								if (ins->mCode != IC_LOAD || j == mTrueJump->mInstructions.Size())
								{
									if (ins->mCode == IC_LOAD)
									{
										j = 0;
										while (j < mFalseJump->mInstructions.Size() && CanBypassLoad(ins, mFalseJump->mInstructions[j]))
											j++;
									}

									if (ins->mCode != IC_LOAD || j == mFalseJump->mInstructions.Size())
									{
										for (int j = 0; j < ins->mNumOperands; j++)
										{
											if (ins->mSrc[j].mTemp >= 0)
											{
												trueExitRequiredTemps += ins->mSrc[j].mTemp;
												falseExitRequiredTems += ins->mSrc[j].mTemp;
												joinedBlock->mEntryRequiredTemps += ins->mSrc[j].mTemp;
											}
										}

										joinedBlock->mInstructions.Insert(0, ins);
										mInstructions.Remove(i);
										moved = true;
										changed = true;
									}
								}
							}
						}
#endif
					}

					providedTemps += ins->mDst.mTemp;
				}

				if (!moved)
				{
					for (int j = 0; j < ins->mNumOperands; j++)
					{
						if (ins->mSrc[j].mTemp >= 0)
							requiredTemps += ins->mSrc[j].mTemp;
					}
				}

				if (HasSideEffect(ins->mCode))
					hadStore = true;

			}
		}

		if (mTrueJump && mTrueJump->PushSinglePathResultInstructions())
			changed = true;
		if (mFalseJump && mFalseJump->PushSinglePathResultInstructions())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::RemoveNonRelevantStatics(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_STORE || ins->mCode == IC_COPY)
			{
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL && !ins->mVolatile)
				{
					if (!(ins->mSrc[1].mLinkerObject->mFlags & LOBJF_RELEVANT) && (ins->mSrc[1].mLinkerObject->mType == LOT_BSS || ins->mSrc[1].mLinkerObject->mType == LOT_DATA))
					{
						ins->mSrc[0].mTemp = -1;
						ins->mCode = IC_NONE;
					}
				}
			}
		}

		if (mTrueJump) mTrueJump->RemoveNonRelevantStatics();
		if (mFalseJump) mFalseJump->RemoveNonRelevantStatics();
	}
}

void InterCodeBasicBlock::CollectOuterFrame(int level, int& size, bool &inner, bool &inlineAssembler, bool &byteCodeCall)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode == IC_PUSH_FRAME)
			{
				level++;
				if (level == 1)
				{
					if (mInstructions[i]->mConst.mIntConst > size)
						size = mInstructions[i]->mConst.mIntConst;
					mInstructions[i]->mCode = IC_NONE;
				}
				else
					inner = true;
			}
			else if (mInstructions[i]->mCode == IC_POP_FRAME)
			{
				if (level == 1)
				{
					mInstructions[i]->mCode = IC_NONE;
				}
				level--;
			}
			else if (mInstructions[i]->mCode == IC_ASSEMBLER)
				inlineAssembler = true;
			else if (mInstructions[i]->mCode == IC_CALL)
				byteCodeCall = true;
		}

		if (mTrueJump) mTrueJump->CollectOuterFrame(level, size, inner, inlineAssembler, byteCodeCall);
		if (mFalseJump) mFalseJump->CollectOuterFrame(level, size, inner, inlineAssembler, byteCodeCall);
	}
}

bool InterCodeBasicBlock::IsLeafProcedure(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
			if (mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE)
				return false;

		if (mTrueJump && !mTrueJump->IsLeafProcedure())
			return false;
		if (mFalseJump && !mFalseJump->IsLeafProcedure())
			return false;
	}

	return true;
}


void InterCodeBasicBlock::ExpandSelect(InterCodeProcedure* proc)
{
	if (!mVisited)
	{
		mVisited = true;

		int i = 0;
		while (i < mInstructions.Size() && mInstructions[i]->mCode != IC_SELECT)
			i++;

		if (i < mInstructions.Size())
		{
			InterInstruction* sins = mInstructions[i];

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock();
			proc->Append(tblock);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock();
			proc->Append(fblock);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock();
			proc->Append(eblock);

			for (int j = i + 1; j < mInstructions.Size(); j++)
				eblock->mInstructions.Push(mInstructions[j]);
			eblock->Close(mTrueJump, mFalseJump);

			mInstructions.SetSize(i);

			InterInstruction* bins = new InterInstruction();
			bins->mCode = IC_BRANCH;
			bins->mSrc[0] = sins->mSrc[2];
			mInstructions.Push(bins);

			InterInstruction* tins = new InterInstruction();
			if (sins->mSrc[1].mTemp < 0)
			{
				tins->mCode = IC_CONSTANT;
				tins->mConst = sins->mSrc[1];
			}
			else
			{
				tins->mCode = IC_LOAD_TEMPORARY;
				tins->mSrc[0] = sins->mSrc[1];
			}

			tins->mDst = sins->mDst;

			InterInstruction* fins = new InterInstruction();
			if (sins->mSrc[0].mTemp < 0)
			{
				fins->mCode = IC_CONSTANT;
				fins->mConst = sins->mSrc[0];
			}
			else
			{
				fins->mCode = IC_LOAD_TEMPORARY;
				fins->mSrc[0] = sins->mSrc[0];
			}

			fins->mDst = sins->mDst;

			tblock->mInstructions.Push(tins);
			InterInstruction* jins = new InterInstruction();
			jins->mCode = IC_JUMP;
			tblock->mInstructions.Push(jins);

			tblock->Close(eblock, nullptr);

			fblock->mInstructions.Push(fins);
			jins = new InterInstruction();
			jins->mCode = IC_JUMP;
			fblock->mInstructions.Push(jins);

			fblock->Close(eblock, nullptr);

			mTrueJump = tblock;
			mFalseJump = fblock;
		}

		if (mTrueJump)
			mTrueJump->ExpandSelect(proc);
		if (mFalseJump)
			mFalseJump->ExpandSelect(proc);
	}
}

void InterCodeBasicBlock::SplitBranches(InterCodeProcedure* proc)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump && (mInstructions.Size() > 2 || mInstructions.Size() == 2 && mInstructions[0]->mCode != IC_RELATIONAL_OPERATOR))
		{
			InterCodeBasicBlock* block = new InterCodeBasicBlock();
			proc->Append(block);
			if (mInstructions[mInstructions.Size() - 2]->mCode == IC_RELATIONAL_OPERATOR)
			{
				block->mInstructions.Push(mInstructions[mInstructions.Size() - 2]);
				block->mInstructions.Push(mInstructions[mInstructions.Size() - 1]);
				mInstructions.SetSize(mInstructions.Size() - 2);
			}
			else
			{
				block->mInstructions.Push(mInstructions.Pop());
			}

			InterInstruction* jins = new InterInstruction();
			jins->mCode = IC_JUMP;
			mInstructions.Push(jins);
			block->Close(mTrueJump, mFalseJump);
			mTrueJump = block;
			mFalseJump = nullptr;
			block->mNumEntries = 1;

			block->SplitBranches(proc);
		}
		else
		{
			if (mTrueJump)
				mTrueJump->SplitBranches(proc);
			if (mFalseJump)
				mFalseJump->SplitBranches(proc);
		}
	}
}

bool InterCodeBasicBlock::IsEqual(const InterCodeBasicBlock* block) const
{
	if (mTrueJump == block->mTrueJump && mFalseJump == block->mFalseJump && mInstructions.Size() == block->mInstructions.Size())
	{
		for (int i = 0; i < mInstructions.Size(); i++)
			if (!mInstructions[i]->IsEqual(block->mInstructions[i]))
				return false;
		return true;
	}

	return false;
}

bool InterCodeBasicBlock::CheckStaticStack(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE)
			{
				if (mInstructions[i]->mSrc[0].mTemp >= 0 || !mInstructions[i]->mSrc[0].mLinkerObject)
					return false;
				else if (!(mInstructions[i]->mSrc[0].mLinkerObject->mFlags & LOBJF_STATIC_STACK))
					return false;
			}
		}

		if (mTrueJump && !mTrueJump->CheckStaticStack())
			return false;
		if (mFalseJump && !mFalseJump->CheckStaticStack())
			return false;
	}

	return true;
}

void ApplyStaticStack(InterOperand & iop, const GrowingVariableArray& localVars)
{
	if (iop.mMemory == IM_LOCAL)
	{
		iop.mMemory = IM_GLOBAL;
		iop.mLinkerObject = localVars[iop.mVarIndex]->mLinkerObject;
		iop.mVarIndex = localVars[iop.mVarIndex]->mIndex;
	}
}

void InterCodeBasicBlock::CollectStaticStack(LinkerObject* lobj, const GrowingVariableArray& localVars)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE)
				lobj->mStackSection->mSections.Push(mInstructions[i]->mSrc[0].mLinkerObject->mStackSection);

			if (mInstructions[i]->mCode == IC_LOAD)
				ApplyStaticStack(mInstructions[i]->mSrc[0], localVars);
			else if (mInstructions[i]->mCode == IC_STORE || mInstructions[i]->mCode == IC_LEA)
				ApplyStaticStack(mInstructions[i]->mSrc[1], localVars);
			else if (mInstructions[i]->mCode == IC_CONSTANT && mInstructions[i]->mDst.mType == IT_POINTER)
				ApplyStaticStack(mInstructions[i]->mConst, localVars);
			else if (mInstructions[i]->mCode == IC_COPY)
			{
				ApplyStaticStack(mInstructions[i]->mSrc[0], localVars);
				ApplyStaticStack(mInstructions[i]->mSrc[1], localVars);
			}
		}

		if (mTrueJump) mTrueJump->CollectStaticStack(lobj, localVars);
		if (mFalseJump) mFalseJump->CollectStaticStack(lobj, localVars);
	}
}

bool InterCodeBasicBlock::DropUnreachable(void)
{
	if (!mVisited)
	{
		mVisited = true;

		int i = 0;
		while (i < mInstructions.Size() && mInstructions[i]->mCode != IC_UNREACHABLE)
			i++;

		if (i < mInstructions.Size())
		{
			// kill all instructions after this
			mInstructions.SetSize(i + 1);
			mFalseJump = nullptr;
			mTrueJump = nullptr;

			if (mInstructions.Size() == 1)
				mUnreachable = true;
		}
		else
		{
			if (mFalseJump)
			{
				if (mFalseJump->DropUnreachable())
				{
					mInstructions.Last()->mCode = IC_JUMP;
					mInstructions.Last()->mNumOperands = 0;
					mFalseJump = nullptr;

					if (mTrueJump->DropUnreachable())
					{
						mTrueJump = nullptr;
						mInstructions.SetSize(mInstructions.Size() - 1);
						if (mInstructions.Size() == 0)
							mUnreachable = true;
					}
				}
				else if (mTrueJump->DropUnreachable())
				{
					mInstructions.Last()->mCode = IC_JUMP;
					mInstructions.Last()->mNumOperands = 0;
					mTrueJump = mFalseJump;
					mFalseJump = nullptr;
				}
			}
			else if (mTrueJump && mTrueJump->DropUnreachable())
			{
				mTrueJump = nullptr;
				mInstructions.SetSize(mInstructions.Size() - 1);
				if (mInstructions.Size() == 0)
					mUnreachable = true;
			}
		}
	}

	return mUnreachable;
}


bool InterCodeBasicBlock::OptimizeIntervalCompare(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int	sz = mInstructions.Size();
		if (sz >= 2 && mTrueJump && mFalseJump && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && 
			mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp && !mExitRequiredTemps[mInstructions[sz - 1]->mSrc[0].mTemp] && 
			IsIntegerType(mInstructions[sz - 2]->mSrc[0].mType))
		{
			if (mInstructions[sz - 2]->mOperator == IA_CMPGES && mInstructions[sz - 2]->mSrc[0].mTemp == -1)
			{
				if (mTrueJump->mInstructions.Size() == 2 && mTrueJump->mInstructions[1]->mCode == IC_BRANCH && mTrueJump->mFalseJump == mFalseJump &&
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc->mTemp)
				{
					if (mTrueJump->mInstructions[0]->mSrc[0].mTemp == -1 && mTrueJump->mInstructions[0]->mSrc[1].mTemp == mInstructions[sz - 2]->mSrc[1].mTemp)
					{
						if (mTrueJump->mInstructions[0]->mOperator == IA_CMPLS && mInstructions[sz - 2]->mSrc[0].mIntConst == 0 && mTrueJump->mInstructions[0]->mSrc[0].mIntConst > 0)
						{
							mInstructions[sz - 2]->mOperator = IA_CMPLU;
							mInstructions[sz - 2]->mSrc[0].mIntConst = mTrueJump->mInstructions[0]->mSrc[0].mIntConst;
							mTrueJump->mNumEntries--;
							mTrueJump = mTrueJump->mTrueJump;
							mTrueJump->mNumEntries++;
							changed = true;
						}
						else if (mTrueJump->mInstructions[0]->mOperator == IA_CMPLES && mInstructions[sz - 2]->mSrc[0].mIntConst == 0 && mTrueJump->mInstructions[0]->mSrc[0].mIntConst > 0)
						{
							mInstructions[sz - 2]->mOperator = IA_CMPLEU;
							mInstructions[sz - 2]->mSrc[0].mIntConst = mTrueJump->mInstructions[0]->mSrc[0].mIntConst;
							mTrueJump->mNumEntries--;
							mTrueJump = mTrueJump->mTrueJump;
							mTrueJump->mNumEntries++;
							changed = true;
						}
					}
				}				
			}
			else if (mInstructions[sz - 2]->mOperator == IA_CMPLS && mInstructions[sz - 2]->mSrc[0].mTemp == -1)
			{
				if (mTrueJump->mInstructions.Size() == 2 && mTrueJump->mInstructions[1]->mCode == IC_BRANCH && mTrueJump->mFalseJump == mFalseJump &&
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc->mTemp)
				{
					if (mTrueJump->mInstructions[0]->mSrc[0].mTemp == -1 && mTrueJump->mInstructions[0]->mSrc[1].mTemp == mInstructions[sz - 2]->mSrc[1].mTemp)
					{
						if (mTrueJump->mInstructions[0]->mOperator == IA_CMPGES && mInstructions[sz - 2]->mSrc[0].mIntConst > 0 && mTrueJump->mInstructions[0]->mSrc[0].mIntConst == 0)
						{
							mInstructions[sz - 2]->mOperator = IA_CMPLU;
							mTrueJump->mNumEntries--;
							mTrueJump = mTrueJump->mTrueJump;
							mTrueJump->mNumEntries++;
							changed = true;
						}
					}
				}
			}
			else if (mInstructions[sz - 2]->mOperator == IA_CMPLES && mInstructions[sz - 2]->mSrc[0].mTemp == -1)
			{
				if (mTrueJump->mInstructions.Size() == 2 && mTrueJump->mInstructions[1]->mCode == IC_BRANCH && mTrueJump->mFalseJump == mFalseJump &&
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc->mTemp)
				{
					if (mTrueJump->mInstructions[0]->mSrc[0].mTemp == -1 && mTrueJump->mInstructions[0]->mSrc[1].mTemp == mInstructions[sz - 2]->mSrc[1].mTemp)
					{
						if (mTrueJump->mInstructions[0]->mOperator == IA_CMPGES && mInstructions[sz - 2]->mSrc[0].mIntConst > 0 && mTrueJump->mInstructions[0]->mSrc[0].mIntConst == 0)
						{
							mInstructions[sz - 2]->mOperator = IA_CMPLEU;
							mTrueJump->mNumEntries--;
							mTrueJump = mTrueJump->mTrueJump;
							mTrueJump->mNumEntries++;
							changed = true;
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->OptimizeIntervalCompare())
			changed = true;
		if (mFalseJump && mFalseJump->OptimizeIntervalCompare())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::FollowJumps(void)
{
	if (!mVisited)
	{
		mVisited = true;

		while (mInstructions.Size() > 0 && mInstructions[mInstructions.Size() - 1]->mCode == IC_JUMP && mTrueJump->mNumEntries == 1)
		{
			InterCodeBasicBlock* block = mTrueJump;
			mInstructions.SetSize(mInstructions.Size() - 1);
			for (int i = 0; i < block->mInstructions.Size(); i++)
				mInstructions.Push(block->mInstructions[i]);

			block->mNumEntries = 0;
			block->mInstructions.Clear();
			mTrueJump = block->mTrueJump;
			mFalseJump = block->mFalseJump;
		}

		if (mTrueJump)
			mTrueJump->FollowJumps();
		if (mFalseJump)
			mFalseJump->FollowJumps();
	}
}

void InterCodeBasicBlock::BuildLoopSuffix(InterCodeProcedure* proc)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump)
		{
			if (mTrueJump == this && mFalseJump != this)
			{
				if (mFalseJump->mNumEntries > 1)
				{
					InterCodeBasicBlock* suffix = new InterCodeBasicBlock();
					proc->Append(suffix);
					InterInstruction* jins = new InterInstruction();
					jins->mCode = IC_JUMP;
					suffix->Append(jins);
					suffix->Close(mFalseJump, nullptr);
					mFalseJump = suffix;
					suffix->mNumEntries = 1;
				}
			}
			else if (mFalseJump == this && mTrueJump != this)
			{
				if (mTrueJump->mNumEntries > 1)
				{
					InterCodeBasicBlock* suffix = new InterCodeBasicBlock();
					proc->Append(suffix);
					InterInstruction* jins = new InterInstruction();
					jins->mCode = IC_JUMP;
					suffix->Append(jins);
					suffix->Close(mTrueJump, nullptr);
					mTrueJump = suffix;
					suffix->mNumEntries = 1;
				}
			}
		}

		if (mTrueJump)
			mTrueJump->BuildLoopSuffix(proc);
		if (mFalseJump)
			mFalseJump->BuildLoopSuffix(proc);
	}
}

InterCodeBasicBlock* InterCodeBasicBlock::BuildLoopPrefix(InterCodeProcedure* proc)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump)
			mTrueJump = mTrueJump->BuildLoopPrefix(proc);
		if (mFalseJump)
			mFalseJump = mFalseJump->BuildLoopPrefix(proc);

		if (mLoopHead)
		{
			mLoopPrefix = new InterCodeBasicBlock();
			proc->Append(mLoopPrefix);
			InterInstruction* jins = new InterInstruction();
			jins->mCode = IC_JUMP;
			mLoopPrefix->Append(jins);
			mLoopPrefix->Close(this, nullptr);
		}
	}

	return mLoopPrefix ? mLoopPrefix : this;
}

bool InterCodeBasicBlock::CollectLoopBody(InterCodeBasicBlock* head, GrowingArray<InterCodeBasicBlock*> & body)
{
	if (mLoopHead)
		return this == head;

	if (body.IndexOf(this) != -1)
			return true;
	body.Push(this);

	for (int i = 0; i < mEntryBlocks.Size(); i++)
		if (!mEntryBlocks[i]->CollectLoopBody(head, body))
			return false;

	return true;
}

void InterCodeBasicBlock::CollectLoopPath(const GrowingArray<InterCodeBasicBlock*>& body, GrowingArray<InterCodeBasicBlock*>& path)
{
	if (body.IndexOf(this) >= 0)
	{
		if (!mLoopPath)
		{
			if (mTrueJump && !mTrueJump->mLoopHead)
			{
				mTrueJump->CollectLoopPath(body, mLoopPathBlocks);
				if (mFalseJump)
				{
					GrowingArray<InterCodeBasicBlock*>	fpath(nullptr);

					if (!mFalseJump->mLoopHead)
						mFalseJump->CollectLoopPath(body, fpath);

					int i = 0;
					while (i < mLoopPathBlocks.Size())
					{
						if (fpath.IndexOf(mLoopPathBlocks[i]) >= 0)
							i++;
						else
							mLoopPathBlocks.Remove(i);
					}
				}
			}

			mLoopPathBlocks.Insert(0, this);
			mLoopPath = true;
		}

		path = mLoopPathBlocks;
	}
}

void InterCodeBasicBlock::InnerLoopOptimization(const NumberSet& aliasedParams)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead)
		{
			GrowingArray<InterCodeBasicBlock*> body(nullptr), path(nullptr);
			body.Push(this);
			bool	innerLoop = true;
			
			for (int i = 0; i < mEntryBlocks.Size(); i++)
			{
				if (mEntryBlocks[i] != mLoopPrefix)
				{
					if (!mEntryBlocks[i]->CollectLoopBody(this, body))
						innerLoop = false;
				}
			}

			if (innerLoop)
			{
				for (int i = 0; i < body.Size(); i++)
				{
					body[i]->mLoopPath = false;
					body[i]->mLoopPathBlocks.SetSize(0);
				}

				this->CollectLoopPath(body, path);

#if 0
				printf("InnerLoop %d\n", mIndex);
				for (int i = 0; i < body.Size(); i++)
					printf("body %d\n", body[i]->mIndex);
				for (int i = 0; i < path.Size(); i++)
					printf("path %d\n", path[i]->mIndex);
#endif
				bool	hasCall = false, hasFrame = false;
				for (int bi = 0; bi < body.Size(); bi++)
				{
					InterCodeBasicBlock* block = body[bi];

					for (int i = 0; i < block->mInstructions.Size(); i++)
					{
						InterInstruction* ins = block->mInstructions[i];
						ins->mInvariant = false;
						ins->mExpensive = false;
						if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
							hasCall = true;
						else if (ins->mCode == IC_PUSH_FRAME)
							hasFrame = true;
					}
				}

				for (int bi = 0; bi < path.Size(); bi++)
				{
					InterCodeBasicBlock* block = path[bi];

					for (int i = 0; i < block->mInstructions.Size(); i++)
					{
						InterInstruction* ins = block->mInstructions[i];
						ins->mInvariant = true;

						if (!IsMoveable(ins->mCode))
							ins->mInvariant = false;
						else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER && ins->mConst.mMemory == IM_FRAME && hasFrame)
							ins->mInvariant = false;
						else if (ins->mCode == IC_LEA && ins->mSrc[1].mMemory == IM_FRAME && hasFrame)
							ins->mInvariant = false;
						else if (ins->mCode == IC_LOAD)
						{
							if (ins->mSrc[0].mTemp >= 0 || ins->mVolatile)
							{
								ins->mInvariant = false;
							}
							else if (ins->mSrc[0].mMemory == IM_GLOBAL && hasCall)
							{
								ins->mInvariant = false;
							}
							else if (ins->mSrc[0].mMemory == IM_LOCAL && hasCall)
							{
								ins->mInvariant = false;
							}
							else
							{
								for (int bj = 0; bj < body.Size(); bj++)
								{
									InterCodeBasicBlock* blockj = body[bj];

									for (int j = 0; j < blockj->mInstructions.Size(); j++)
									{
										InterInstruction* sins = blockj->mInstructions[j];
										if (sins->mCode == IC_STORE)
										{
											if (sins->mSrc[1].mTemp >= 0)
											{
												if ((ins->mSrc[0].mMemory != IM_PARAM && ins->mSrc[0].mMemory != IM_FPARAM) || aliasedParams[ins->mSrc[0].mVarIndex])
													ins->mInvariant = false;
											}
											else if (ins->mSrc[0].mMemory == sins->mSrc[1].mMemory && ins->mSrc[0].mVarIndex == sins->mSrc[1].mVarIndex && ins->mSrc[0].mLinkerObject == sins->mSrc[1].mLinkerObject)
											{
												ins->mInvariant = false;
											}
										}
										else if (sins->mCode == IC_COPY)
										{
											ins->mInvariant = false;
										}
									}
								}
							}
						}
					}
				}

				enum Dependency
				{
					DEP_UNKNOWN,
					DEP_DEFINED,
					DEP_ITERATED,
					DEP_VARIABLE
				};

				GrowingArray<Dependency>			dep(DEP_UNKNOWN);
				GrowingArray<InterInstructionPtr>	tvalues(nullptr);

				for (int bi = 0; bi < body.Size(); bi++)
				{
					InterCodeBasicBlock* block = body[bi];

					for (int i = 0; i < block->mInstructions.Size(); i++)
					{
						InterInstruction* ins = block->mInstructions[i];
						int t = ins->mDst.mTemp;
						if (t >= 0)
						{
							if (HasSideEffect(ins->mCode) || !ins->mInvariant)
								dep[t] = DEP_VARIABLE;
							else if (dep[t] == DEP_UNKNOWN)
								dep[t] = DEP_DEFINED;
							else if (dep[t] == DEP_DEFINED)
							{
								dep[t] = DEP_VARIABLE;
								ins->mInvariant = false;
							}
						}

						if (ins->mInvariant)
						{
							switch (ins->mCode)
							{
							case IC_BINARY_OPERATOR:
								ins->mExpensive = true;
								break;
							case IC_UNARY_OPERATOR:
								ins->mExpensive = true;
								break;
							case IC_LEA:
								if (ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0)
									ins->mExpensive = true;
								break;
							case IC_LOAD:
							case IC_STORE:
								ins->mExpensive = true;
								break;
							}
						}
					}
				}

				for (int i = 0; i < dep.Size(); i++)
				{
					if (dep[i] == DEP_DEFINED)
						dep[i] = DEP_ITERATED;
				}

				bool	changed;
				do
				{
					changed = false;

					for (int bi = 0; bi < path.Size(); bi++)
					{
						InterCodeBasicBlock* block = path[bi];

						for (int i = 0; i < block->mInstructions.Size(); i++)
						{
							InterInstruction* ins = block->mInstructions[i];
							int t = ins->mDst.mTemp;
							if (t >= 0)
							{
								if (dep[t] < DEP_VARIABLE)
								{
									int j = 0;
									while (j < ins->mNumOperands && !(ins->mSrc[j].mTemp >= 0 && dep[ins->mSrc[j].mTemp] >= DEP_ITERATED))
										j++;
									if (j < ins->mNumOperands)
									{
										dep[t] = DEP_VARIABLE;
										ins->mInvariant = false;
										changed = true;
									}
									else
									{
										dep[t] = DEP_DEFINED;
									}
								}
								else
									ins->mInvariant = false;
							}
						}
					}

				} while (changed);

#if 1
				NumberSet		required(dep.Size());

				do
				{
					changed = false;

					for (int bi = 0; bi < path.Size(); bi++)
					{
						InterCodeBasicBlock* block = path[bi];

						for (int i = 0; i < block->mInstructions.Size(); i++)
						{
							InterInstruction* ins = block->mInstructions[i];

							if (ins->mInvariant && !ins->mExpensive && ins->mDst.mTemp >= 0 && required[ins->mDst.mTemp])
								ins->mExpensive = true;

							if (ins->mInvariant && ins->mExpensive)
							{
								for (int i = 0; i < ins->mNumOperands; i++)
								{
									if (ins->mSrc[i].mTemp >= 0 && !required[ins->mSrc[i].mTemp])
									{
										required += ins->mSrc[i].mTemp;
										changed = true;
									}
								}
							}
						}
					}

				} while(changed);


				for (int bi = 0; bi < path.Size(); bi++)
				{
					InterCodeBasicBlock* block = path[bi];

					int	j = 0;
					for (int i = 0; i < block->mInstructions.Size(); i++)
					{
						InterInstruction* ins = block->mInstructions[i];
						if (ins->mInvariant && ins->mExpensive)
						{
							mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
						}
						else
						{
							block->mInstructions[j++] = ins;
						}
					}
#ifdef _DEBUG
					if (j != block->mInstructions.Size())
						printf("Moved %d %d\n", mIndex, block->mInstructions.Size() - j);
#endif
					block->mInstructions.SetSize(j);
				}
#endif
			}
		}


		if (mTrueJump)
			mTrueJump->InnerLoopOptimization(aliasedParams);
		if (mFalseJump)
			mFalseJump->InnerLoopOptimization(aliasedParams);
	}
}

void InterCodeBasicBlock::SingleBlockLoopUnrolling(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mTrueJump == this)
		{
			int	nins = mInstructions.Size();

			if (nins > 3 && nins < 20)
			{
				if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
					mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[nins - 2]->mOperator == IA_CMPLU && mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
					mInstructions[nins - 2]->mSrc[0].mTemp < 0 &&
					mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_ADD && mInstructions[nins - 3]->mDst.mTemp == mInstructions[nins - 2]->mSrc[1].mTemp)
				{
					int	ireg = mInstructions[nins - 3]->mDst.mTemp;

					if (ireg == mInstructions[nins - 3]->mSrc[0].mTemp && mInstructions[nins - 3]->mSrc[1].mTemp < 0 ||
						ireg == mInstructions[nins - 3]->mSrc[1].mTemp && mInstructions[nins - 3]->mSrc[0].mTemp < 0)
					{

						int	i = 0;
						while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ireg)
							i++;
						if (i == nins - 3)
						{
							if (mDominator->mTrueValueRange[ireg].IsConstant())
							{
								int	start = mDominator->mTrueValueRange[ireg].mMinValue;
								int	end = mInstructions[nins - 2]->mSrc[0].mIntConst;
								int	step = mInstructions[nins - 3]->mSrc[0].mTemp < 0 ? mInstructions[nins - 3]->mSrc[0].mIntConst : mInstructions[nins - 3]->mSrc[1].mIntConst;
								int	count = (end - start) / step;

								if (count < 5 && (nins - 3) * count < 20)
								{
									mInstructions.SetSize(nins - 2);
									nins -= 2;
									for (int i = 1; i < count; i++)
									{
										for (int j = 0; j < nins; j++)
										{
											mInstructions.Push(mInstructions[j]->Clone());
										}
									}

									mNumEntries--;
									mLoopHead = false;
									mTrueJump = mFalseJump;
									mFalseJump = nullptr;

									InterInstruction* jins = new InterInstruction();
									jins->mCode = IC_JUMP;
									mInstructions.Push(jins);
								}
							}
						}
					}
				}					
			}
		}

		if (mTrueJump)
			mTrueJump->SingleBlockLoopUnrolling();
		if (mFalseJump)
			mFalseJump->SingleBlockLoopUnrolling();
	}
}


void InterCodeBasicBlock::SingleBlockLoopOptimisation(const NumberSet& aliasedParams, const GrowingVariableArray& staticVars)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this))
		{
			bool	hasCall = false;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
					hasCall = true;
			}

#if 1
			InterCodeBasicBlock* tailBlock = this == mTrueJump ? mFalseJump : mTrueJump;
			assert(tailBlock->mNumEntries == 1);

			if (!hasCall)
			{
				// Check forwarding globals

				int i = 0;
				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins = mInstructions[i];

					// A global load
					if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp < 0 && (ins->mSrc[0].mMemory == IM_GLOBAL || ins->mSrc[0].mMemory == IM_FPARAM))
					{
						// Find the last store that overlaps the load
						int	j = mInstructions.Size() - 1;
						while (j > i && !(mInstructions[j]->mCode == IC_STORE && CollidingMem(ins->mSrc[0], mInstructions[j]->mSrc[1], staticVars)))
							j--;

						if (j > i)
						{
							InterInstruction* sins = mInstructions[j];

							// Does a full store
							if (SameMem(ins->mSrc[0], sins->mSrc[1]))
							{
								if (sins->mSrc[0].mTemp >= 0)
								{
									// Check temp not used before load
									int k = 0;
									while (k < i && !mInstructions[k]->UsesTemp(sins->mSrc[0].mTemp))
										k++;
									if (k == i)
									{
										// Check temp not modified after load
										k = j + 1;
										while (k < mInstructions.Size() && mInstructions[k]->mDst.mTemp != sins->mSrc[0].mTemp)
											k++;
										if (k == mInstructions.Size())
										{
											assert(!mEntryRequiredTemps[sins->mSrc[0].mTemp]);

											// Move load before loop
											mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
											InterInstruction* nins = new InterInstruction();
											mInstructions[i] = nins;
											nins->mCode = IC_LOAD_TEMPORARY;
											nins->mDst.Forward(ins->mDst);
											nins->mSrc[0].Forward(sins->mSrc[0]);
											ins->mDst.Forward(sins->mSrc[0]);
											sins->mSrc[0].mFinal = false;
											assert(nins->mSrc[0].mTemp >= 0);

											// Move store behind loop
											tailBlock->mInstructions.Insert(0, sins);
											mInstructions.Remove(j);
										}
									}
								}
							}
						}
					}
				}
			}
#endif

			GrowingArray<InterInstructionPtr>	tvalues(nullptr);
			GrowingArray<int>					nassigns(0);

			int	frameLevel = 0;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				ins->mInvariant = true;

				if (ins->mCode == IC_PUSH_FRAME)
					frameLevel++;
				else if (ins->mCode == IC_POP_FRAME)
					frameLevel--;

				if (!IsMoveable(ins->mCode))
					ins->mInvariant = false;
				else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER && ins->mConst.mMemory == IM_FRAME && frameLevel != 0)
					ins->mInvariant = false;
				else if (ins->mCode == IC_LEA && ins->mSrc[1].mMemory == IM_FRAME && frameLevel != 0)
					ins->mInvariant = false;
				else if (ins->mCode == IC_LOAD)
				{
					if (ins->mSrc[0].mTemp >= 0 || ins->mVolatile)
					{
						ins->mInvariant = false;
					}
					else if (ins->mSrc[0].mMemory == IM_GLOBAL && hasCall)
					{
						ins->mInvariant = false;
					}
					else if (ins->mSrc[0].mMemory == IM_LOCAL && hasCall)
					{
						ins->mInvariant = false;
					}
					else
					{
						for (int j = 0; j < mInstructions.Size(); j++)
						{
							InterInstruction* sins = mInstructions[j];
							if (sins->mCode == IC_STORE && CollidingMem(ins->mSrc[0], sins->mSrc[1], staticVars))
							{
								if (sins->mSrc[1].mTemp >= 0)
								{
									if ((ins->mSrc[0].mMemory != IM_PARAM && ins->mSrc[0].mMemory != IM_FPARAM) || aliasedParams[ins->mSrc[0].mVarIndex])
									{
										int k = j - 1;
										while (k >= 0 && mInstructions[k]->mDst.mTemp != sins->mSrc[1].mTemp)
											k--;
										if (k >= 0)
										{
											InterInstruction* lins = mInstructions[k];
											if (lins->mCode == IC_LEA && lins->mSrc[1].mTemp < 0)
											{
												if (ins->mSrc[0].mMemory == lins->mSrc[1].mMemory && ins->mSrc[0].mVarIndex == lins->mSrc[1].mVarIndex && ins->mSrc[0].mLinkerObject == lins->mSrc[1].mLinkerObject)
													ins->mInvariant = false;
											}
											else
												ins->mInvariant = false;
										}
										else
											ins->mInvariant = false;
									}
								}
								else if (CollidingMem(ins->mSrc[0], sins->mSrc[1], staticVars))
								{
									ins->mInvariant = false;
								}
							}
							else if (sins->mCode == IC_COPY)
							{
								ins->mInvariant = false;
							}
						}
					}
				}
			}

			enum Dependency
			{
				DEP_UNKNOWN,
				DEP_DEFINED,

				DEP_INDEX,
				DEP_INDEX_DERIVED,
				DEP_INDEX_EXTENDED,

				DEP_ITERATED,
				DEP_VARIABLE
			};

			GrowingArray<Dependency>			dep(DEP_UNKNOWN);
			GrowingArray<int64>					indexStep(0), indexBase(0);
			tvalues.SetSize(0);

			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				int t = ins->mDst.mTemp;
				if (t >= 0)
				{
					if (HasSideEffect(ins->mCode) || !ins->mInvariant)
						dep[t] = DEP_VARIABLE;
					else if (dep[t] == DEP_UNKNOWN)
					{
#if 1
						if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp == t)// && ins->mSrc[0].mIntConst > 0)
						{
							indexStep[t] = ins->mSrc[0].mIntConst;
							indexBase[t] = 0;
							dep[t] = DEP_INDEX;
							ins->mInvariant = false;
						}
						else
#endif
							dep[t] = DEP_DEFINED;
					}
					else if (dep[t] == DEP_DEFINED || dep[t] == DEP_INDEX)
					{
						dep[t] = DEP_VARIABLE;
						ins->mInvariant = false;
					}
				}
			}

			for (int i = 0; i < dep.Size(); i++)
			{
				if (dep[i] == DEP_DEFINED)
					dep[i] = DEP_ITERATED;
			}

			indexStep.SetSize(dep.Size());
			indexBase.SetSize(dep.Size());

			bool	changed;
			do
			{
				changed = false;

				for (int i = 0; i < mInstructions.Size(); i++)
				{
					if (i + 1 < mInstructions.Size())
					{
						InterInstruction* ins0 = mInstructions[i + 0];
						InterInstruction* ins1 = mInstructions[i + 1];
						if (ins0->mCode == IC_BINARY_OPERATOR && ins0->mOperator == IA_ADD && ins1->mCode == IC_BINARY_OPERATOR && ins1->mOperator == IA_ADD)
						{
							if (ins0->mDst.mTemp == ins1->mSrc[1].mTemp && IsIntegerType(ins1->mDst.mType) && ins1->mSrc[1].mFinal)
							{
								if (ins0->mSrc[0].mTemp >= 0 && ins0->mSrc[1].mTemp >= 0 && ins1->mSrc[0].mTemp >= 0)
								{
									if ((dep[ins0->mSrc[1].mTemp] == DEP_INDEX || dep[ins0->mSrc[1].mTemp] == DEP_INDEX_EXTENDED) &&
										(dep[ins0->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins0->mSrc[0].mTemp] == DEP_VARIABLE) &&
										(dep[ins1->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins1->mSrc[0].mTemp] == DEP_VARIABLE))
									{
										InterOperand	op = ins0->mSrc[1];
										ins0->mSrc[1] = ins1->mSrc[0];
										ins1->mSrc[0] = op;
										if (dep[ins0->mSrc[0].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[1].mTemp] == DEP_UNKNOWN)
											ins0->mInvariant = true;
										dep[ins0->mDst.mTemp] = DEP_UNKNOWN;
										dep[ins1->mDst.mTemp] = DEP_UNKNOWN;
										changed = true;
									}
								}
							}
						}

						if (ins0->mCode == IC_BINARY_OPERATOR && ins0->mOperator == IA_ADD && ins1->mCode == IC_LEA && ins1->mSrc[1].mTemp >= 0)
						{
							if (ins1->mSrc[0].mTemp == ins0->mDst.mTemp && ins1->mSrc[0].mFinal && ins0->mSrc[0].mTemp >= 0 && ins0->mSrc[1].mTemp)
							{
								if (dep[ins1->mSrc[1].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[0].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[1].mTemp] == DEP_INDEX_EXTENDED)
								{
									InterOperand	iop = ins0->mSrc[1];
									InterOperand	pop = ins1->mSrc[1];

									ins0->mCode = IC_LEA;
									ins0->mDst.mType = IT_POINTER;

									ins0->mSrc[1] = pop;

									ins1->mSrc[1].mTemp = ins0->mDst.mTemp;
									ins1->mSrc[0] = iop;

									if (dep[ins0->mSrc[0].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[1].mTemp] == DEP_UNKNOWN)
										ins0->mInvariant = true;
									dep[ins0->mDst.mTemp] = DEP_UNKNOWN;
									dep[ins1->mDst.mTemp] = DEP_UNKNOWN;
								}
							}
						}

					}

					InterInstruction* ins = mInstructions[i];
					int t = ins->mDst.mTemp;
					if (t >= 0)
					{
						if (dep[t] < DEP_VARIABLE && dep[t] != DEP_INDEX)
						{
							int j = 0;
							while (j < ins->mNumOperands && !(ins->mSrc[j].mTemp >= 0 && dep[ins->mSrc[j].mTemp] >= DEP_INDEX))
								j++;
							if (j < ins->mNumOperands)
							{
								if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && IsIntegerType(ins->mDst.mType) && ins->mSrc[1].mTemp < 0 && (dep[ins->mSrc[0].mTemp] == DEP_INDEX || dep[ins->mSrc[0].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED) ||
									ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED) ||
									ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SHL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED) ||
									ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && (ins->mSrc[0].mTemp < 0 || dep[ins->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[0].mTemp] == DEP_DEFINED) && dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED ||
									ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED ||
									ins->mCode == IC_LEA && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED )
								{
									if (dep[ins->mDst.mTemp] != DEP_INDEX_DERIVED)
									{
										dep[ins->mDst.mTemp] = DEP_INDEX_DERIVED;
										ins->mInvariant = false;
										changed = true;
									}
								}
								else if (ins->mCode == IC_CONVERSION_OPERATOR && (ins->mOperator == IA_EXT8TO16S || ins->mOperator == IA_EXT8TO16U) && dep[ins->mSrc[0].mTemp] == DEP_INDEX)
								{
									if (dep[ins->mDst.mTemp] != DEP_INDEX_EXTENDED)
									{
										dep[ins->mDst.mTemp] = DEP_INDEX_EXTENDED;
										ins->mInvariant = false;
										changed = true;
									}
								}
								else
								{
									dep[t] = DEP_VARIABLE;
									ins->mInvariant = false;
									changed = true;
								}
							}
							else
							{
								dep[t] = DEP_DEFINED;
							}
						}
					}
				}

			} while (changed);

			GrowingArray<InterInstructionPtr>	indexins(nullptr);
			GrowingArray<InterInstructionPtr>	pindexins(nullptr);

			int	j = 0;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				if (ins->mInvariant)
				{
					mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
				}
				else if (ins->mDst.mTemp >= 0 && dep[ins->mDst.mTemp] == DEP_INDEX)
				{
					indexBase[ins->mDst.mTemp] = indexStep[ins->mDst.mTemp];
					mInstructions[j++] = ins;
				}
				else if (ins->mDst.mTemp >= 0 && dep[ins->mDst.mTemp] == DEP_INDEX_DERIVED)
				{
					if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && IsIntegerType(ins->mDst.mType) && ins->mSrc[1].mTemp < 0 && (dep[ins->mSrc[0].mTemp] == DEP_INDEX || dep[ins->mSrc[0].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = ins->mSrc[1].mIntConst * indexStep[ins->mSrc[0].mTemp];
						indexBase[ins->mDst.mTemp] = ins->mSrc[1].mIntConst * indexBase[ins->mSrc[0].mTemp];

						InterInstruction* bins = new InterInstruction();
						bins->mCode = IC_BINARY_OPERATOR;
						bins->mOperator = IA_MUL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction();
						ains->mCode = IC_BINARY_OPERATOR;
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[1] = ins->mSrc[1]; ains->mSrc[1].mIntConst = ins->mSrc[1].mIntConst * indexBase[ins->mSrc[0].mTemp];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[1].mIntConst = ins->mSrc[1].mIntConst * indexStep[ins->mSrc[0].mTemp];
						ins->mSrc[0] = ins->mDst;

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = ins->mSrc[0].mIntConst * indexStep[ins->mSrc[1].mTemp];
						indexBase[ins->mDst.mTemp] = ins->mSrc[0].mIntConst * indexBase[ins->mSrc[1].mTemp];

						InterInstruction* bins = new InterInstruction();
						bins->mCode = IC_BINARY_OPERATOR;
						bins->mOperator = IA_MUL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction();
						ains->mCode = IC_BINARY_OPERATOR;
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mSrc[0]; ains->mSrc[0].mIntConst = ins->mSrc[0].mIntConst * indexBase[ins->mSrc[1].mTemp];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mIntConst = ins->mSrc[0].mIntConst * indexStep[ins->mSrc[1].mTemp];
						ins->mSrc[1] = ins->mDst;

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SHL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						indexBase[ins->mDst.mTemp] = indexBase[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;

						InterInstruction* bins = new InterInstruction();
						bins->mCode = IC_BINARY_OPERATOR;
						bins->mOperator = IA_SHL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction();
						ains->mCode = IC_BINARY_OPERATOR;
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mSrc[0]; ains->mSrc[0].mIntConst = indexBase[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mIntConst = indexStep[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						ins->mSrc[1] = ins->mDst;

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && (ins->mSrc[0].mTemp < 0 || dep[ins->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[0].mTemp] == DEP_DEFINED) && dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED)
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[1].mTemp];
						indexBase[ins->mDst.mTemp] = 0;

						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);

						InterInstruction* ains = new InterInstruction();
						ains->mCode = IC_BINARY_OPERATOR;
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[1].mTemp = -1;
						ains->mSrc[1].mIntConst = indexStep[ins->mSrc[1].mTemp];

						indexins.Push(ains);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED)
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[0].mTemp];
						indexBase[ins->mDst.mTemp] = 0;

						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);

						InterInstruction* ains = new InterInstruction();
						ains->mCode = IC_BINARY_OPERATOR;
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[0].mTemp = -1;
						ains->mSrc[0].mIntConst = indexStep[ins->mSrc[0].mTemp];

						indexins.Push(ains);
					}
					else if (ins->mCode == IC_LEA && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED)
					{
#if 1
						int k = 0;
						while (k < pindexins.Size() && !(
							ins->mSrc[0].mTemp == pindexins[k]->mSrc[0].mTemp &&
							ins->mSrc[1].mTemp == pindexins[k]->mSrc[1].mTemp &&
							ins->mSrc[1].mMemory == pindexins[k]->mSrc[1].mMemory &&
							ins->mSrc[1].mVarIndex == pindexins[k]->mSrc[1].mVarIndex))
							k++;

						if (k < pindexins.Size() && ins->mSrc[1].mIntConst >= pindexins[k]->mSrc[1].mIntConst)
						{
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[0].mIntConst = ins->mSrc[1].mIntConst - pindexins[k]->mSrc[1].mIntConst;
							ins->mSrc[1].mTemp = pindexins[k]->mDst.mTemp;
							ins->mSrc[1].mMemory = IM_INDIRECT;
							ins->mSrc[1].mIntConst = 0;
							mInstructions[j++] = ins;
						}
						else
#endif
						{
							indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[0].mTemp];
							indexBase[ins->mDst.mTemp] = 0;

							mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);

							InterInstruction* ains = new InterInstruction();
							ains->mCode = IC_LEA;
							ains->mDst = ins->mDst;
							ains->mSrc[1] = ins->mDst;
							ains->mSrc[1].mMemory = IM_INDIRECT;
							ains->mSrc[1].mIntConst = 0;
							ains->mSrc[0].mType = IT_INT16;
							ains->mSrc[0].mTemp = -1;
							ains->mSrc[0].mIntConst = indexStep[ins->mSrc[0].mTemp];

							if (tailBlock->mEntryRequiredTemps[ains->mDst.mTemp])
							{
								InterInstruction* dins = new InterInstruction();
								dins->mCode = IC_LEA;
								dins->mDst = ins->mDst;
								dins->mSrc[1] = ins->mDst;
								dins->mSrc[1].mMemory = IM_INDIRECT;
								dins->mSrc[1].mIntConst = 0;
								dins->mSrc[0].mType = IT_INT16;
								dins->mSrc[0].mTemp = -1;
								dins->mSrc[0].mIntConst = -indexStep[ins->mSrc[0].mTemp];
								tailBlock->mInstructions.Insert(0, dins);
							}
							indexins.Push(ains);

							if (indexStep[ins->mSrc[0].mTemp] > 1)
								pindexins.Push(ins);
						}
					}
				}
				else if (ins->mDst.mTemp >= 0 && dep[ins->mDst.mTemp] == DEP_INDEX_EXTENDED)
				{
					indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[0].mTemp];
					indexBase[ins->mDst.mTemp] = indexBase[ins->mSrc[0].mTemp];

					mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);

					InterInstruction* ains = new InterInstruction();
					ains->mCode = ins->mCode;
					ains->mOperator = ins->mOperator;
					ains->mSrc[0] = ins->mSrc[0];
					ains->mDst = ins->mDst;

					mInstructions[j++] = ains;
				}
				else
				{
					mInstructions[j++] = ins;
				}
			}

			mInstructions.SetSize(j);

			NumberSet		requiredTemps(tailBlock->mEntryRequiredTemps);

			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				for(int j=0; j<ins->mNumOperands; j++)
					if (ins->mSrc[j].mTemp >= 0 && ins->mDst.mTemp != ins->mSrc[j].mTemp)
						requiredTemps += ins->mSrc[j].mTemp;
			}

			int	di = mInstructions.Size() - 1;
			if (di > 0 && mInstructions[di - 1]->mCode == IC_RELATIONAL_OPERATOR)
			{
				di--;
				if (di > 0 && mInstructions[di - 1]->mDst.mTemp == mInstructions[di]->mSrc[0].mTemp || mInstructions[di - 1]->mDst.mTemp == mInstructions[di]->mSrc[1].mTemp)
					di--;
			}

			for (int i = 0; i < indexins.Size(); i++)
			{
				InterInstruction* ins = indexins[i];
				if (requiredTemps[ins->mDst.mTemp])
				{
					mInstructions.Insert(di, ins);
					di++;
				}
			}

			int	i = 0;
			while (i < mInstructions.Size())
			{
				InterInstruction* ins = mInstructions[i];
				if (!HasSideEffect(ins->mCode) && !ins->mVolatile && ins->mDst.mTemp >= 0 && !requiredTemps[ins->mDst.mTemp])
					mInstructions.Remove(i);
				else
					i++;
			}

			// move temp moves into tail if not used in loop

			i = mInstructions.Size() - 1;
			while (i >= 0)
			{
				InterInstruction* ins = mInstructions[i];
				if (ins->mCode == IC_LOAD_TEMPORARY && !mEntryRequiredTemps[ins->mDst.mTemp])
				{
					int	dt = ins->mDst.mTemp, st = ins->mSrc[0].mTemp;

					int	toffset = 0;
					int	j = i + 1;
					while (j < mInstructions.Size())
					{
						InterInstruction* cins = mInstructions[j];

						if (cins->mDst.mTemp == dt)
							break;
						else if (cins->mCode == IC_LOAD_TEMPORARY && cins->mSrc[0].mTemp == st && cins->mSrc[0].mFinal)
							st = cins->mDst.mTemp;
						else if (cins->mDst.mTemp == st)
						{
							if (cins->mCode == IC_LEA && cins->mSrc[1].mTemp == st && cins->mSrc[0].mTemp < 0)
								toffset += cins->mSrc[0].mIntConst;
							else
								break;
						}

						j++;
					}

					if (j == mInstructions.Size())
					{
						ins->mSrc[0].mTemp = st;
						if (toffset != 0)
						{
							if (ins->mDst.mType == IT_POINTER)
							{
								ins->mCode = IC_LEA;
								ins->mNumOperands = 2;
								ins->mSrc[1] = ins->mSrc[0];
								ins->mSrc[0].mTemp = -1;
								ins->mSrc[0].mType = IT_INT16;
								ins->mSrc[0].mIntConst = -toffset;
							}
						}
						tailBlock->mInstructions.Insert(0, ins);
						mInstructions.Remove(i);
					}
				}
				i--;
			}


		}

		if (mTrueJump)
			mTrueJump->SingleBlockLoopOptimisation(aliasedParams, staticVars);
		if (mFalseJump)
			mFalseJump->SingleBlockLoopOptimisation(aliasedParams, staticVars);
	}
}

void InterCodeBasicBlock::CompactInstructions(void)
{
	if (!mVisited)
	{
		mVisited = true;

		int	j = 0;
		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode != IC_NONE)
			{
				mInstructions[j++] = mInstructions[i];
			}
		}
		mInstructions.SetSize(j);

		if (mTrueJump)
			mTrueJump->CompactInstructions();
		if (mFalseJump)
			mFalseJump->CompactInstructions();
	}
}

static void SwapInstructions(InterInstruction* it, InterInstruction* ib)
{
	for (int i = 0; i < ib->mNumOperands; i++)
	{
		if (ib->mSrc[i].mTemp >= 0 && ib->mSrc[i].mFinal)
		{
			for (int j = 0; j < it->mNumOperands; j++)
			{
				if (it->mSrc[j].mTemp == ib->mSrc[i].mTemp)
				{
					it->mSrc[j].mFinal = true;
					ib->mSrc[i].mFinal = false;
				}
			}
		}
	}
}

void InterCodeBasicBlock::CheckFinalLocal(void) 
{
#if _DEBUG
	NumberSet	required(mExitRequiredTemps);

	for (int i = mInstructions.Size() - 1; i >= 0; i--)
	{
		const InterInstruction* ins(mInstructions[i]);
		if (ins->mDst.mTemp >= 0)
			required -= ins->mDst.mTemp;
		for (int j = 0; j < ins->mNumOperands; j++)
		{
			if (ins->mSrc[j].mTemp >= 0 && ins->mSrc[j].mFinal)
				assert(!required[ins->mSrc[j].mTemp]);
		}

		for (int j = 0; j < ins->mNumOperands; j++)
			if (ins->mSrc[j].mTemp >= 0)
				required += ins->mSrc[j].mTemp;
	}

	NumberSet	provided(mEntryProvidedTemps);

	for (int i = 0; i< mInstructions.Size(); i++)
	{
		const InterInstruction* ins(mInstructions[i]);
		for (int j = 0; j < ins->mNumOperands; j++)
		{
			if (ins->mSrc[j].mTemp >= 0)
				assert(provided[ins->mSrc[j].mTemp]);
		}

		if (ins->mDst.mTemp >= 0)
			provided += ins->mDst.mTemp;
	}
#endif
}

void InterCodeBasicBlock::CheckFinal(void)
{
#if _DEBUG
	if (!mVisited)
	{
		mVisited = true;

		CheckFinalLocal();

		if (mTrueJump) mTrueJump->CheckFinal();
		if (mFalseJump) mFalseJump->CheckFinal();
	}
#endif
}

void InterCodeBasicBlock::PeepholeOptimization(const GrowingVariableArray& staticVars)
{
	int		i;
	
	if (!mVisited)
	{
		mVisited = true;

		CheckFinalLocal();

		// Remove none instructions

		int	j = 0;
		for (i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode != IC_NONE)
			{
				mInstructions[j++] = mInstructions[i];
			}
		}
		mInstructions.SetSize(j);


		// shorten lifespan

		int	loopTmp = -1;

		int	limit = mInstructions.Size() - 1;
		if (limit >= 0 && mInstructions[limit]->mCode == IC_BRANCH)
		{
			limit--;
#if 1
			// try to move conditional source down
			int i = limit;
			while (i >= 0 && mInstructions[i]->mDst.mTemp != mInstructions[limit + 1]->mSrc[0].mTemp)
				i--;
			
			if (i >= 0 && i != limit)
			{
				InterInstruction* ins(mInstructions[i]);

				if (ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR)
				{
					if (ins->mSrc[0].mTemp < 0)
					{
						int k = i;
						while (k < limit && CanBypass(ins, mInstructions[k + 1]))
							k++;

						if (k == limit)
						{
							for (int l = i; l < limit; l++)
							{
								SwapInstructions(ins, mInstructions[l + 1]);
								mInstructions[l] = mInstructions[l + 1];
							}
							mInstructions[limit] = ins;

//							mInstructions.Remove(i);
//							mInstructions.Insert(limit, ins);
						}
					}
				}
			}
#endif
			if (limit > 0 && mInstructions[limit]->mCode == IC_RELATIONAL_OPERATOR)
			{
				if (mInstructions[limit]->mSrc[1].mTemp)
					loopTmp = mInstructions[limit]->mSrc[1].mTemp;
				else if (mInstructions[limit]->mSrc[0].mTemp)
					loopTmp = mInstructions[limit]->mSrc[0].mTemp;
				limit--;

				if (loopTmp >= 0)
				{
					int i = limit;
					while (i >= 0 && !(mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mDst.mTemp == loopTmp))
						i--;
					if (i >= 0 && i < limit)
					{
						InterInstruction* ins(mInstructions[i]);
						int j = i;
						while (j < limit && CanBypass(ins, mInstructions[j + 1]))
						{
							SwapInstructions(ins, mInstructions[j + 1]);
							mInstructions[j] = mInstructions[j + 1];
							j++;
						}
						if (i != j)
							mInstructions[j] = ins;
					}

					if (limit > 0 && mInstructions[limit]->mCode == IC_BINARY_OPERATOR && (mInstructions[limit]->mDst.mTemp == loopTmp))
						limit--;
				}
			}
			else if (limit > 0 && mInstructions[limit]->mDst.mTemp == mInstructions[limit + 1]->mSrc[0].mTemp)
				limit--;
		}
		else if (limit >= 0 && mInstructions[limit]->mCode == IC_JUMP)
			limit --;

		CheckFinalLocal();

		int i = limit;
#if 1
		while (i >= 0)
		{
			// move non indirect loads down
			if (mInstructions[i]->mCode == IC_LOAD && (mInstructions[i]->mSrc[0].mMemory != IM_INDIRECT || mInstructions[i]->mDst.mType != IT_INT8 || !mInstructions[i]->mSrc[0].mFinal))
			{
				InterInstruction	*	ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanBypassLoad(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR || mInstructions[i]->mCode == IC_UNARY_OPERATOR || mInstructions[i]->mCode == IC_CONVERSION_OPERATOR || mInstructions[i]->mCode == IC_CONSTANT)
			{
				InterInstruction* ins(mInstructions[i]);

				int k = i;
				while (k < limit && CanBypass(ins, mInstructions[k + 1]))
					k++;
				if (k < limit)
				{
					while (k > i && IsChained(mInstructions[k], mInstructions[k + 1]))
						k--;
				}

				int j = i;
				while (j < k)
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}

				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_LEA && (mInstructions[i]->mSrc[0].mTemp < 0 || mInstructions[i]->mSrc[1].mTemp < 0))
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanBypass(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}

			i--;
		}

		CheckFinalLocal();
#endif
#if 1
		// move indirect load/store pairs up
		i = 0;
		while (i + 1 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_LOAD && mInstructions[i + 1]->mCode == IC_STORE && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal)
			{
				if (mInstructions[i + 0]->mSrc[0].mMemory == IM_INDIRECT)
				{
					InterInstruction* lins(mInstructions[i + 0]);
					InterInstruction* sins(mInstructions[i + 1]);

					int j = i;
					while (j > 0 && 
						CanBypassLoadUp(lins, mInstructions[j - 1]) &&
						CanBypassStore(sins, mInstructions[j - 1]))
					{
						SwapInstructions(mInstructions[j - 1], lins);
						SwapInstructions(mInstructions[j - 1], sins);

						mInstructions[j + 1] = mInstructions[j - 1];
						j--;
					}

					if (i != j)
					{
						mInstructions[j + 0] = lins;
						mInstructions[j + 1] = sins;
					}
				}
			}

			i++;
		}

		CheckFinalLocal();
#endif
#if 1
		i = 0;
		while (i < mInstructions.Size())
		{
			// move stores up
			if (mInstructions[i]->mCode == IC_STORE)
			{
				InterInstruction	*	ins(mInstructions[i]);
				int j = i;
				while (j > 0 && CanBypassStore(ins, mInstructions[j - 1]))
				{
					SwapInstructions(mInstructions[j - 1], ins);
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mSrc[0].mTemp >= 0 && mInstructions[i]->mSrc[0].mFinal && mInstructions[i]->mSrc[1].mTemp >= 0 && mInstructions[i]->mSrc[1].mFinal)
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j > 0 && CanBypassUp(ins, mInstructions[j - 1]))
				{
					SwapInstructions(mInstructions[j - 1], ins);
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_LOAD && mInstructions[i]->mSrc[0].mMemory == IM_INDIRECT && mInstructions[i]->mSrc[0].mFinal && mInstructions[i]->mDst.mType == IT_INT8)
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j > 0 && CanBypassLoadUp(ins, mInstructions[j - 1]))
				{
					SwapInstructions(mInstructions[j - 1], ins);
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				if (i != j)
					mInstructions[j] = ins;
			}

			i++;
		}

		CheckFinalLocal();
#endif

#if 1
		i = limit;
		while (i >= 0)
		{
			// move non indirect loads down
			if (mInstructions[i]->mCode == IC_LOAD && (mInstructions[i]->mSrc[0].mMemory != IM_INDIRECT || mInstructions[i]->mDst.mType != IT_INT8 || !mInstructions[i]->mSrc[0].mFinal))
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanBypassLoad(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_CONSTANT || (mInstructions[i]->mCode == IC_LEA && mInstructions[i]->mSrc[0].mTemp == -1))
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanBypass(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}

			i--;
		}
#endif

		CheckFinalLocal();

		bool	changed = false;
		do
		{
			int	j = 0;
			for (i = 0; i < mInstructions.Size(); i++)
			{
				if (mInstructions[i]->mCode != IC_NONE)
				{
					mInstructions[j++] = mInstructions[i];
				}
			}
			mInstructions.SetSize(j);

			changed = false;

			for (i = 0; i < mInstructions.Size(); i++)
			{
				if (mInstructions[i]->mCode == IC_LOAD_TEMPORARY && mInstructions[i]->mDst.mTemp == mInstructions[i]->mSrc->mTemp)
				{
					mInstructions[i]->mCode = IC_NONE;
					mInstructions[i]->mNumOperands = 0;
					changed = true;
				}
				if (mInstructions[i]->mCode == IC_LOAD && mInstructions[i]->mSrc[0].mMemory == IM_GLOBAL && (mInstructions[i]->mSrc->mLinkerObject->mFlags & LOBJF_CONST))
				{
					LoadConstantFold(mInstructions[i], nullptr, staticVars);
					changed = true;
				}

				if (i + 2 < mInstructions.Size())
				{
					if (mInstructions[i + 0]->mCode == IC_LOAD &&
						mInstructions[i + 1]->mCode == IC_LOAD &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mSrc[0].mTemp &&
						mInstructions[i + 0]->mSrc[0].mIntConst > mInstructions[i + 1]->mSrc[0].mIntConst)
					{
						SwapInstructions(mInstructions[i + 0], mInstructions[i + 1]);
						InterInstruction* ins(mInstructions[i + 0]);
						mInstructions[i + 0] = mInstructions[i + 1];
						mInstructions[i + 1] = ins;
						changed = true;
					}
					else if (mInstructions[i + 0]->mDst.mTemp >= 0 &&
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i]->mDst.mTemp &&
						(mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR || mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR) && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal)
					{
#if _DEBUG
						for (int j = i + 3; j < mInstructions.Size(); j++)
							assert(!mInstructions[j]->ReferencesTemp(mInstructions[i]->mDst.mTemp));
#endif

						int	t = mInstructions[i + 0]->mDst.mTemp;
						mInstructions[i + 0]->mDst.mTemp = mInstructions[i + 1]->mDst.mTemp;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 2]->mSrc[0].mTemp = mInstructions[i + 1]->mDst.mTemp;
						mInstructions[i + 2]->mSrc[0].mFinal = false;
						if (mInstructions[i + 2]->mSrc[1].mTemp == t)
						{
							mInstructions[i + 2]->mSrc[1].mTemp = mInstructions[i + 1]->mDst.mTemp;
							mInstructions[i + 2]->mSrc[1].mFinal = false;
						}
						changed = true;
					}
					else if (mInstructions[i + 0]->mDst.mTemp >= 0 &&
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i]->mDst.mTemp &&
						(mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR || mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR) && mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal)
					{
#if _DEBUG
						for (int j = i + 3; j < mInstructions.Size(); j++)
							assert(!mInstructions[j]->ReferencesTemp(mInstructions[i]->mDst.mTemp));
#endif

						mInstructions[i + 0]->mDst.mTemp = mInstructions[i + 1]->mDst.mTemp;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 2]->mSrc[1].mTemp = mInstructions[i + 1]->mDst.mTemp;
						mInstructions[i + 2]->mSrc[1].mFinal = false;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						(mInstructions[i + 1]->mSrc[0].mIntConst & (1LL << mInstructions[i + 0]->mSrc[0].mIntConst)) == 0)
					{
						int	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
						mInstructions[i + 1]->mSrc[0].mIntConst >>= shift;
						mInstructions[i + 0]->mOperator = IA_AND;
						mInstructions[i + 0]->mSrc[0].mIntConst = ~((1LL << shift) - 1);
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[1].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						(mInstructions[i + 1]->mSrc[1].mIntConst & (1LL << mInstructions[i + 0]->mSrc[0].mIntConst)) == 0)
					{
						int	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
						mInstructions[i + 1]->mSrc[1].mIntConst >>= shift;
						mInstructions[i + 0]->mOperator = IA_AND;
						mInstructions[i + 0]->mSrc[0].mIntConst = ~((1LL << shift) - 1);
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHL && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						(mInstructions[i + 1]->mSrc[0].mIntConst << mInstructions[i + 0]->mSrc[0].mIntConst) < 65536)
					{
						mInstructions[i + 1]->mSrc[0].mIntConst <<= mInstructions[i + 0]->mSrc[0].mIntConst;;
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 0]->mCode = IC_NONE;
						mInstructions[i + 0]->mNumOperands = 0;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_MUL && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						(mInstructions[i + 0]->mSrc[0].mIntConst << mInstructions[i + 1]->mSrc[0].mIntConst) < 65536)
					{
						mInstructions[i + 1]->mSrc[0].mIntConst = mInstructions[i + 0]->mSrc[0].mIntConst << mInstructions[i + 1]->mSrc[0].mIntConst;
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 1]->mOperator = IA_MUL;
						mInstructions[i + 0]->mCode = IC_NONE;
						mInstructions[i + 0]->mNumOperands = 0;
						changed = true;
					}
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_OR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_AND && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
					{
						int64	ior = mInstructions[i + 0]->mSrc[0].mIntConst & mInstructions[i + 1]->mSrc[0].mIntConst;
						int64	iand = mInstructions[i + 1]->mSrc[0].mIntConst;

						mInstructions[i + 0]->mOperator = IA_AND;
						mInstructions[i + 0]->mSrc[0].mIntConst = iand;
						mInstructions[i + 1]->mOperator = IA_OR;
						mInstructions[i + 1]->mSrc[0].mIntConst = ior;
						changed = true;
					}
#endif
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_AND && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_AND && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
					{
						mInstructions[i + 0]->mSrc[0].mIntConst &= mInstructions[i + 1]->mSrc[0].mIntConst;
						mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_OR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_OR && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
					{
						mInstructions[i + 0]->mSrc[0].mIntConst |= mInstructions[i + 1]->mSrc[0].mIntConst;
						mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						changed = true;
					}
#endif
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
						mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_MUL && mInstructions[i + 2]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&
						(mInstructions[i + 2]->mSrc[0].mIntConst & 1) == 0)
					{

						int	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
						int	mshift = 1;
						while (!(mInstructions[i + 2]->mSrc[0].mIntConst & (1ULL << mshift)))
							mshift++;

						mInstructions[i + 1]->mCode = IC_BINARY_OPERATOR;
						mInstructions[i + 1]->mOperator = IA_AND;
						mInstructions[i + 1]->mSrc[0].mType = IT_INT16;
						mInstructions[i + 1]->mSrc[1].mType = IT_INT16;
						mInstructions[i + 1]->mSrc[1].mTemp = -1;
						mInstructions[i + 1]->mSrc[1].mIntConst = 255;

						if (mshift < shift)
						{
							mInstructions[i + 0]->mSrc[0].mIntConst = shift - mshift;
							mInstructions[i + 1]->mSrc[1].mIntConst = 255ULL >> shift << mshift;
							mInstructions[i + 2]->mSrc[0].mIntConst >>= mshift;
						}
						else if (mshift >= shift)
						{
							mInstructions[i + 0]->mCode = IC_LOAD_TEMPORARY;
							mInstructions[i + 0]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
							mInstructions[i + 0]->mSrc[1].mTemp = -1;
							assert(mInstructions[i + 0]->mSrc[0].mTemp >= 0);

							mInstructions[i + 1]->mSrc[1].mIntConst = 255ULL >> shift << shift;
							mInstructions[i + 2]->mSrc[0].mIntConst >>= shift;
						}

						changed = true;
					}
#endif
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal
						)
					{	
						mInstructions[i + 1]->mSrc[0].Forward(mInstructions[i + 0]->mSrc[0]);
						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						changed = true;
					}
#endif
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
						mInstructions[i + 1]->mCode == IC_LOAD && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal
						)
					{	
						mInstructions[i + 1]->mSrc[0].Forward(mInstructions[i + 0]->mSrc[0]);
						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						changed = true;
					}
#endif
#if 1
					else if (
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mExitRequiredTemps[mInstructions[i + 1]->mDst.mTemp] &&
						(!mExitRequiredTemps[mInstructions[i + 1]->mSrc[0].mTemp] ||
							(mEntryRequiredTemps[mInstructions[i + 1]->mDst.mTemp] && !mEntryRequiredTemps[mInstructions[i + 1]->mSrc[0].mTemp])) &&
						mInstructions[i + 0]->mDst.mTemp == mInstructions[i + 1]->mSrc[0].mTemp)
					{
						mInstructions[i + 0]->mDst.mTemp = mInstructions[i + 1]->mDst.mTemp;
						mInstructions[i + 1]->mDst.mTemp = mInstructions[i + 1]->mSrc[0].mTemp;
						mInstructions[i + 1]->mSrc[0].mTemp = mInstructions[i + 0]->mDst.mTemp;
						mInstructions[i + 1]->mSrc[0].mFinal = false;
						mInstructions[i + 0]->mSingleAssignment = mInstructions[i + 1]->mSingleAssignment;
						changed = true;
					}
#endif
					else if (
						mInstructions[i + 0]->mDst.mTemp >= 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && IsCommutative(mInstructions[i + 1]->mOperator) && mInstructions[i + 0]->mDst.mTemp == mInstructions[i + 1]->mSrc[0].mTemp && mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 1]->mSrc[1].mTemp)
					{
						InterOperand	io = mInstructions[i + 1]->mSrc[1];
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
						mInstructions[i + 1]->mSrc[0] = io;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 0]->mSrc[1].IsUByte() && !mInstructions[i + 0]->mSrc[0].IsUByte())
					{
						mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[1];

						mInstructions[i + 0]->mCode = IC_LEA;
						mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[1];
						mInstructions[i + 0]->mDst.mType = IT_POINTER;
						mInstructions[i + 0]->mDst.mRange.Reset();

						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mDst;
						mInstructions[i + 1]->mSrc[1].mMemory = IM_INDIRECT;

						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 0]->mSrc[0].IsUByte() && !mInstructions[i + 0]->mSrc[1].IsUByte())
					{
						mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[0];

						mInstructions[i + 0]->mCode = IC_LEA;
						mInstructions[i + 0]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[1];
						mInstructions[i + 0]->mDst.mType = IT_POINTER;
						mInstructions[i + 0]->mDst.mRange.Reset();

						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mDst;
						mInstructions[i + 1]->mSrc[1].mMemory = IM_INDIRECT;
						changed = true;
					}
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mIntConst >= 0 && mInstructions[i + 0]->mSrc[0].mIntConst <= 16 &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal)
					{
						mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[0];

						mInstructions[i + 0]->mCode = IC_LEA;
						mInstructions[i + 0]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[1];
						mInstructions[i + 0]->mDst.mType = IT_POINTER;
						mInstructions[i + 0]->mDst.mRange.Reset();

						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mDst;
						mInstructions[i + 1]->mSrc[1].mMemory = IM_INDIRECT;
						changed = true;
					}
#endif
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[1].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mType == IT_INT16 &&
						mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 1]->mSrc[0].IsUByte() &&
						mInstructions[i + 2]->mCode == IC_LEA && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
						mInstructions[i + 2]->mSrc[1].mTemp < 0)
					{
						mInstructions[i + 2]->mSrc[0] = mInstructions[i + 0]->mSrc[0];
						mInstructions[i + 2]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[1].mIntConst;

						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mType == IT_INT16 &&
						mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 1]->mSrc[0].IsUByte() &&
						mInstructions[i + 2]->mCode == IC_LEA && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
						mInstructions[i + 2]->mSrc[1].mTemp < 0)
					{
						mInstructions[i + 2]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 2]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;

						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SUB && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mType == IT_INT16 &&
						mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
						mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
						mInstructions[i + 1]->mSrc[0].IsUByte() &&
						mInstructions[i + 2]->mCode == IC_LEA && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
						mInstructions[i + 2]->mSrc[1].mTemp < 0)
					{
						mInstructions[i + 2]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 2]->mSrc[1].mIntConst -= mInstructions[i + 0]->mSrc[0].mIntConst;

						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mMemory == IM_GLOBAL &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						mInstructions[i + 0]->mSrc[0].IsUByte() && mInstructions[i + 1]->mSrc[0].IsUByte() && mInstructions[i + 0]->mSrc[0].mRange.mMaxValue + mInstructions[i + 1]->mSrc[0].mRange.mMaxValue < 252)
					{
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];

						mInstructions[i + 0]->mCode = IC_BINARY_OPERATOR;
						mInstructions[i + 0]->mOperator = IA_ADD;
						mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
						mInstructions[i + 0]->mDst.mType = IT_INT16;
						mInstructions[i + 0]->mDst.mRange.mMaxState = IntegerValueRange::S_BOUND;
						mInstructions[i + 0]->mDst.mRange.mMaxValue = mInstructions[i + 0]->mSrc[1].mRange.mMaxValue + mInstructions[i + 0]->mSrc[0].mRange.mMaxValue;
						mInstructions[i + 0]->mDst.mRange.mMinState = IntegerValueRange::S_BOUND;
						mInstructions[i + 0]->mDst.mRange.mMinValue = 0;

						mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mDst;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_STORE && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						mInstructions[i + 1]->mSrc[1].mIntConst != 0)
					{
						mInstructions[i + 0]->mSrc[1].mIntConst += mInstructions[i + 1]->mSrc[1].mIntConst;
						mInstructions[i + 1]->mSrc[1].mIntConst = 0;
						changed = true;
					}
#if 1
					else if (
						mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mMemory == IM_GLOBAL &&
						mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						mInstructions[i + 1]->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && !mInstructions[i + 1]->mSrc[0].IsUByte() &&
						mInstructions[i + 0]->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && !mInstructions[i + 0]->mSrc[0].IsUByte())
					{
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];

						mInstructions[i + 0]->mCode = IC_BINARY_OPERATOR;
						mInstructions[i + 0]->mOperator = IA_ADD;
						mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
						mInstructions[i + 0]->mDst.mType = IT_INT16;
						mInstructions[i + 0]->mDst.mRange.mMaxState = IntegerValueRange::S_BOUND;
						mInstructions[i + 0]->mDst.mRange.mMaxValue = mInstructions[i + 0]->mSrc[1].mRange.mMaxValue + mInstructions[i + 0]->mSrc[0].mRange.mMaxValue;
						mInstructions[i + 0]->mDst.mRange.mMinState = IntegerValueRange::S_BOUND;
						mInstructions[i + 0]->mDst.mRange.mMinValue = 0;

						mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mDst;
						changed = true;
					}
#endif
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
						mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHL &&
						mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
						mInstructions[i + 2]->mCode == IC_LEA &&
						mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
						mInstructions[i + 2]->mSrc[1].mTemp < 0)
					{
						mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
						mInstructions[i + 2]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst << mInstructions[i + 1]->mSrc[0].mIntConst;

						mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
						changed = true;
					}

#if 1
					// Postincrement artifact
					if (mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
						mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
						mInstructions[i + 0]->mSrc[0].mTemp == mInstructions[i + 1]->mSrc[1].mTemp &&
						mInstructions[i + 0]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp)
					{
						InterInstruction	*	ins = mInstructions[i + 1];
						int		ttemp = mInstructions[i + 1]->mDst.mTemp;
						int	k = i + 1;
						while (k + 2 < mInstructions.Size() &&
							mInstructions[k + 1]->mCode != IC_RELATIONAL_OPERATOR &&
							!mInstructions[k + 1]->ReferencesTemp(ttemp))							
						{
							mInstructions[k] = mInstructions[k + 1];
							k++;
						}
						if (k > i + 1)
						{
							mInstructions[k] = ins;
							changed = true;
						}
					}

					CheckFinalLocal();
#endif
				}



#if 1
				if (i + 1 < mInstructions.Size())
				{
					if (
						mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
						mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mSrc[0].mTemp && mInstructions[i + 1]->mSrc[0].mFinal
						)
					{
						mInstructions[i + 1]->mSrc[0].mTemp = mInstructions[i + 0]->mDst.mTemp;
						mInstructions[i + 1]->mSrc[0].mFinal = false;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
						mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mSrc[0].mTemp && mInstructions[i + 1]->mSrc[1].mFinal
						)
					{
						mInstructions[i + 1]->mSrc[1].mTemp = mInstructions[i + 0]->mDst.mTemp;
						mInstructions[i + 1]->mSrc[1].mFinal = false;
						changed = true;
					}
				}
#endif
			}

		} while (changed);

		// build trains

		for(int i = mInstructions.Size() - 1; i > 0; i--)
		{
			InterInstruction* tins = mInstructions[i];

			j = i - 1;
			while (j >= 0 && !tins->ReferencesTemp(mInstructions[j]->mDst.mTemp))
				j--;
			if (j >= 0 && j < i - 1)
			{
				if (CanMoveInstructionDown(j, i)) 
				{
					InterInstruction* jins = mInstructions[j];
					for (int k = j; k < i - 1; k++)
					{
						SwapInstructions(jins, mInstructions[k + 1]);
						mInstructions[k] = mInstructions[k + 1];
					}
					mInstructions[i - 1] = jins;

//					mInstructions.Insert(i, mInstructions[j]);
//					mInstructions.Remove(j);
				}
			}
		}

		CheckFinalLocal();

		// sort stores up

		do
		{
			changed = false;

			for (int i = 0; i + 1 < mInstructions.Size(); i++)
			{
				if (mInstructions[i + 0]->mCode == IC_STORE && mInstructions[i + 1]->mCode == IC_STORE && 
					!mInstructions[i + 0]->mVolatile && !mInstructions[i + 1]->mVolatile &&
//					!CollidingMem(mInstructions[i + 0]->mSrc[1], mInstructions[i + 1]->mSrc[1]) &&
					SameMemRegion(mInstructions[i + 0]->mSrc[1], mInstructions[i + 1]->mSrc[1]) &&

					(mInstructions[i + 0]->mSrc[1].mVarIndex > mInstructions[i + 1]->mSrc[1].mVarIndex ||
						mInstructions[i + 0]->mSrc[1].mVarIndex == mInstructions[i + 1]->mSrc[1].mVarIndex &&
						mInstructions[i + 0]->mSrc[1].mIntConst > mInstructions[i + 1]->mSrc[1].mIntConst))
				{
					SwapInstructions(mInstructions[i + 0], mInstructions[i + 1]);
					InterInstruction* ins = mInstructions[i + 1];
					mInstructions[i + 1] = mInstructions[i + 0];
					mInstructions[i + 0] = ins;
					changed = true;
				}
			}

		} while (changed);

		CheckFinalLocal();

		if (mTrueJump) mTrueJump->PeepholeOptimization(staticVars);
		if (mFalseJump) mFalseJump->PeepholeOptimization(staticVars);
	}
}


void InterCodeBasicBlock::CollectVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars, GrowingVariableArray& paramVars, InterMemory	paramMemory)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			bool	found = false;

			InterInstruction* ins = mInstructions[i];

			switch (ins->mCode)
			{
			case IC_CONSTANT:
				if (ins->mConst.mMemory == IM_LOCAL)
				{
					int varIndex = ins->mConst.mVarIndex;
					if (!localVars[varIndex])
						localVars[varIndex] = new InterVariable;

					int	size = ins->mConst.mOperandSize + ins->mConst.mIntConst;
					if (size > localVars[varIndex]->mSize)
						localVars[varIndex]->mSize = size;
					localVars[varIndex]->mAliased = true;
				}
				else if (ins->mConst.mMemory == paramMemory)
				{
					int varIndex = ins->mConst.mVarIndex;
					if (!paramVars[varIndex])
						paramVars[varIndex] = new InterVariable;

					int	size = ins->mConst.mOperandSize + ins->mConst.mIntConst;
					if (size > paramVars[varIndex]->mSize)
						paramVars[varIndex]->mSize = size;
					paramVars[varIndex]->mAliased = true;
				}
				break;

			case IC_STORE:
			case IC_LOAD:		
			case IC_COPY:
			case IC_STRCPY:
			case IC_CALL_NATIVE:
			case IC_ASSEMBLER:

				for(int j=0; j<ins->mNumOperands; j++)
				{
					if (ins->mSrc[j].mMemory == IM_LOCAL)
					{
						int varIndex = ins->mSrc[j].mVarIndex;
						if (!localVars[varIndex])
							localVars[varIndex] = new InterVariable;

						int	size = ins->mSrc[j].mOperandSize + ins->mSrc[j].mIntConst;
						if (size > localVars[varIndex]->mSize)
							localVars[varIndex]->mSize = size;
					}
					else if (ins->mSrc[j].mMemory == IM_FPARAM || ins->mSrc[j].mMemory == IM_PARAM)
					{
						int varIndex = ins->mSrc[j].mVarIndex;
						if (!paramVars[varIndex])
							paramVars[varIndex] = new InterVariable;

						int	size = ins->mSrc[j].mOperandSize + ins->mSrc[j].mIntConst;
						if (size > paramVars[varIndex]->mSize)
							paramVars[varIndex]->mSize = size;
					}
				}
				break;
			}
		}

		if (mTrueJump) mTrueJump->CollectVariables(globalVars, localVars, paramVars, paramMemory);
		if (mFalseJump) mFalseJump->CollectVariables(globalVars, localVars, paramVars, paramMemory);
	}
}

void InterCodeBasicBlock::CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->CollectSimpleLocals(complexLocals, simpleLocals, localTypes, complexParams, simpleParams, paramTypes);
		}

		if (mTrueJump) mTrueJump->CollectSimpleLocals(complexLocals, simpleLocals, localTypes, complexParams, simpleParams, paramTypes);
		if (mFalseJump) mFalseJump->CollectSimpleLocals(complexLocals, simpleLocals, localTypes, complexParams, simpleParams, paramTypes);
	}
}

void InterCodeBasicBlock::SimpleLocalToTemp(int vindex, int temp)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->SimpleLocalToTemp(vindex, temp);
		}

		if (mTrueJump) mTrueJump->SimpleLocalToTemp(vindex, temp);
		if (mFalseJump) mFalseJump->SimpleLocalToTemp(vindex, temp);
	}

}

void InterCodeBasicBlock::CollectActiveTemporaries(FastNumberSet& set)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->CollectActiveTemporaries(set);
		}

		if (mTrueJump) mTrueJump->CollectActiveTemporaries(set);
		if (mFalseJump) mFalseJump->CollectActiveTemporaries(set);
	}
}

void InterCodeBasicBlock::RemapActiveTemporaries(const FastNumberSet& set)
{
	if (!mVisited)
	{
		mVisited = true;

		GrowingIntegerValueRangeArray	entryValueRange(mEntryValueRange);
		GrowingIntegerValueRangeArray	trueValueRange(mTrueValueRange);
		GrowingIntegerValueRangeArray	falseValueRange(mFalseValueRange);
		GrowingIntegerValueRangeArray	localValueRange(mLocalValueRange);
		GrowingIntegerValueRangeArray	reverseValueRange(mReverseValueRange);

		mEntryValueRange.SetSize(set.Num(), true);
		mTrueValueRange.SetSize(set.Num(), true);
		mFalseValueRange.SetSize(set.Num(), true);
		mLocalValueRange.SetSize(set.Num(), true);
		mReverseValueRange.SetSize(set.Num(), true);

		for (int i = 0; i < set.Num(); i++)
		{
			int j = set.Element(i);
			mEntryValueRange[i] = entryValueRange[j];
			mTrueValueRange[i] = trueValueRange[j];
			mFalseValueRange[i] = falseValueRange[j];
			mLocalValueRange[i] = localValueRange[j];
			mReverseValueRange[i] = reverseValueRange[j];
		}

		if (mTrueJump) mTrueJump->RemapActiveTemporaries(set);
		if (mFalseJump) mFalseJump->RemapActiveTemporaries(set);
	}
}

void InterCodeBasicBlock::ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->ShrinkActiveTemporaries(set, temporaries);
		}

		if (mTrueJump) mTrueJump->ShrinkActiveTemporaries(set, temporaries);
		if (mFalseJump) mFalseJump->ShrinkActiveTemporaries(set, temporaries);
	}
}

void InterCodeBasicBlock::Disassemble(FILE* file, bool dumpSets)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		const char* s = mLoopHead ? "Head" : "";
		fprintf(file, "L%d: <= D%d: (%d) %s \n", mIndex, (mDominator ? mDominator->mIndex : -1), mNumEntries, s);

		if (dumpSets)
		{
			fprintf(file, "Entry required temps : ");
			for (i = 0; i < mEntryRequiredTemps.Size(); i++)
			{
				if (mEntryRequiredTemps[i])
					fprintf(file, "#");
				else
					fprintf(file, "-");
			}
			fprintf(file, "\n\n");
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode != IC_NONE)
			{
				fprintf(file, "%04x\t", i);
				mInstructions[i]->Disassemble(file);
			}
		}

		if (mTrueJump) fprintf(file, "\t\t==> %d\n", mTrueJump->mIndex);
		if (mFalseJump) fprintf(file, "\t\t==> %d\n", mFalseJump->mIndex);

		if (mTrueJump) mTrueJump->Disassemble(file, dumpSets);
		if (mFalseJump) mFalseJump->Disassemble(file, dumpSets);
	}
}

InterCodeProcedure::InterCodeProcedure(InterCodeModule * mod, const Location & location, const Ident* ident, LinkerObject * linkerObject)
	: mTemporaries(IT_NONE), mBlocks(nullptr), mLocation(location), mTempOffset(-1), mTempSizes(0), 
	mRenameTable(-1), mRenameUnionTable(-1), mGlobalRenameTable(-1),
	mValueForwardingTable(nullptr), mLocalVars(nullptr), mParamVars(nullptr), mModule(mod),
	mIdent(ident), mLinkerObject(linkerObject),
	mNativeProcedure(false), mLeafProcedure(false), mCallsFunctionPointer(false), mCalledFunctions(nullptr), mFastCallProcedure(false), 
	mInterrupt(false), mHardwareInterrupt(false), mCompiled(false), mInterruptCalled(false), 
	mSaveTempsLinkerObject(nullptr)
{
	mID = mModule->mProcedures.Size();
	mModule->mProcedures.Push(this);
	mLinkerObject->mProc = this;
	mCallerSavedTemps = 16;
}

InterCodeProcedure::~InterCodeProcedure(void)
{
}

void InterCodeProcedure::ResetEntryBlocks(void)
{
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mEntryBlocks.SetSize(0);
}

void InterCodeProcedure::ResetVisited(void)
{
	int i;

	for (i = 0; i < mBlocks.Size(); i++)
	{
#if _DEBUG
		for (int j = 0; j < mBlocks[i]->mInstructions.Size(); j++)
		{
			InterInstruction* ins = mBlocks[i]->mInstructions[j];
			assert(!ins || ins->mCode != IC_LOAD_TEMPORARY || ins->mSrc[0].mTemp >= 0);
		}
#endif


#if 0
		if (mBlocks[i]->mInstructions.Size() > 0)
		{
			const InterInstruction* ins(mBlocks[i]->mInstructions.Last());
			if (ins)
			{
				if (ins->mCode == IC_BRANCH)
				{
					assert(mBlocks[i]->mTrueJump && mBlocks[i]->mFalseJump && ins->mNumOperands >= 1 && ins->mSrc[0].mTemp >= 0);
				}
				else if (ins->mCode == IC_JUMP)
				{
					assert(mBlocks[i]->mTrueJump);
				}
			}
		}
#endif

		mBlocks[i]->mVisited = false;
		mBlocks[i]->mNumEntered = 0;
	}
}

void InterCodeProcedure::Append(InterCodeBasicBlock* block)
{
	block->mIndex = mBlocks.Size();
	mBlocks.Push(block);
}

int InterCodeProcedure::AddTemporary(InterType type)
{
	int	temp = mTemporaries.Size();
	mTemporaries.Push(type);
	return temp;
}

void InterCodeProcedure::CheckFinal(void)
{
	ResetVisited();
	mEntryBlock->CheckFinal();
}

void InterCodeProcedure::DisassembleDebug(const char* name)
{
	Disassemble(name);
}

void InterCodeProcedure::BuildTraces(bool expand, bool dominators, bool compact)
{
	// Count number of entries
	//
	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
	{
		mBlocks[i]->mNumEntries = 0;
		mBlocks[i]->mLoopHead = false;
	}
	mEntryBlock->CollectEntries();

	//
	// Build traces
	//
	ResetVisited();
	mEntryBlock->GenerateTraces(expand, compact);

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mDominator = nullptr;
	if (dominators)
		mEntryBlock->BuildDominatorTree(nullptr);
}

void InterCodeProcedure::BuildDataFlowSets(void)
{
	int	numTemps = mTemporaries.Size();

	//
	//	Build set with local provided/required temporaries
	//
	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numTemps);

	//
	// Build set of globaly provided temporaries
	//
	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps));

	//
	// Build set of globaly required temporaries, might need
	// multiple iterations until it stabilizes
	//
	NumberSet	totalRequired(numTemps);

	do {
		ResetVisited();
	} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired));

	ResetVisited();
	mEntryBlock->CollectLocalUsedTemps(numTemps);
}

void InterCodeProcedure::RenameTemporaries(void)
{
	int	numTemps = mTemporaries.Size();

	//
	// Now we can rename temporaries to remove false dependencies
	//
	mRenameTable.SetSize(numTemps, true);

	int		i, j, numRename;

	numRename = 0;

	//
	// First localy rename all temporaries
	//
	ResetVisited();
	mEntryBlock->LocalRenameRegister(mRenameTable, numRename);

	DisassembleDebug("local renamed temps");

	//
	// Build a union find data structure for rename merging, this
	// merges renames temporaries back, that have been renamed differently
	// on separate paths.
	//
	mRenameUnionTable.SetSize(numRename);
	for (i = 0; i < numRename; i++)
		mRenameUnionTable[i] = i;

	//
	// Build global rename table using a union/find algorithm
	//
	mRenameTable.SetSize(numTemps, true);

	ResetVisited();
	mEntryBlock->BuildGlobalRenameRegisterTable(mRenameTable, mRenameUnionTable);

	//
	// Now calculate the global temporary IDs for all local ids
	//
	int		numRenamedTemps;

	mGlobalRenameTable.SetSize(numRename, true);

	numRenamedTemps = 0;

	for (i = 0; i < numRename; i++)
	{
		j = Find(mRenameUnionTable, i);

		if (mGlobalRenameTable[j] < 0)
			mGlobalRenameTable[j] = numRenamedTemps++;

		mGlobalRenameTable[i] = mGlobalRenameTable[j];
	}

	mTemporaries.SetSize(numRenamedTemps, true);

	//
	// Set global temporary IDs
	//
	ResetVisited();
	mEntryBlock->GlobalRenameRegister(mGlobalRenameTable, mTemporaries);

	numTemps = numRenamedTemps;

	DisassembleDebug("global renamed temps");
}

void InterCodeProcedure::SingleAssignmentForwarding(void)
{
	int	numTemps = mTemporaries.Size();

	InterMemory	paramMemory = mFastCallProcedure ? IM_FPARAM : IM_PARAM;

	FastNumberSet					tassigned(numTemps);
	GrowingInstructionPtrArray		tunified(nullptr), tvalues(nullptr);
	NumberSet						modifiedParams(mParamAliasedSet.Size());

	tunified.SetSize(numTemps);

	bool	changed;
	do
	{
		ResetVisited();
		changed = mEntryBlock->CalculateSingleAssignmentTemps(tassigned, tunified, modifiedParams, paramMemory);
	} while (changed);

	for (int i = 0; i < numTemps; i++)
	{
		if (tunified[i])
			tunified[i]->mSingleAssignment = true;
	}

	tunified.Clear();
	ResetVisited();
	mEntryBlock->SingleAssignmentTempForwarding(tunified, tvalues);

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("single assignment forwarding");

}

void InterCodeProcedure::PeepholeOptimization(void)
{
	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	ResetVisited();
	mEntryBlock->PeepholeOptimization(mModule->mGlobalVars);
}


void InterCodeProcedure::CheckUsedDefinedTemps(void)
{
#if _DEBUG
	int	numTemps = mTemporaries.Size();

	NumberSet	defined(numTemps), used(numTemps);

	ResetVisited();
	mEntryBlock->CollectAllUsedDefinedTemps(defined, used);

	for (int i = 0; i < numTemps; i++)
	{
		assert(!used[i] || defined[i]);
	}

#endif
}

void InterCodeProcedure::TempForwarding(void)
{
	int	numTemps = mTemporaries.Size();

	CheckUsedDefinedTemps();

	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps);

	//
	// Now remove needless temporary moves, that apear due to
	// stack evaluation
	//
	mTempForwardingTable.SetSize(numTemps);

	mTempForwardingTable.Reset();
	ResetVisited();
	mEntryBlock->PerformTempForwarding(mTempForwardingTable);

	DisassembleDebug("temp forwarding");
}

void InterCodeProcedure::RemoveUnusedInstructions(void)
{
	int	numTemps = mTemporaries.Size();

	do {
		ResetVisited();
		mEntryBlock->BuildLocalTempSets(numTemps);

		ResetVisited();
		mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (mEntryBlock->RemoveUnusedResultInstructions());
}

void InterCodeProcedure::RemoveUnusedStoreInstructions(InterMemory	paramMemory)
{
	if (mLocalVars.Size() > 0 || mParamVars.Size() > 0)
	{
		for (int i = 0; i < mLocalVars.Size(); i++)
		{
			if (mLocalAliasedSet[i])
				mLocalVars[i]->mAliased = true;
		}
		for (int i = 0; i < mParamVars.Size(); i++)
		{
			if (mParamAliasedSet[i])
				mParamVars[i]->mAliased = true;
		}

		//
		// Now remove unused stores
		//

		do {
			ResetVisited();
			mEntryBlock->BuildLocalVariableSets(mLocalVars, mParamVars, paramMemory);

			ResetVisited();
			mEntryBlock->BuildGlobalProvidedVariableSet(mLocalVars, NumberSet(mLocalVars.Size()), mParamVars, NumberSet(mParamVars.Size()), paramMemory);

			NumberSet	totalRequired2(mLocalVars.Size());
			NumberSet	totalRequiredParams(mParamVars.Size());

			do {
				ResetVisited();
			} while (mEntryBlock->BuildGlobalRequiredVariableSet(mLocalVars, totalRequired2, mParamVars, totalRequiredParams, paramMemory));

			ResetVisited();
		} while (mEntryBlock->RemoveUnusedStoreInstructions(mLocalVars, mParamVars, paramMemory));

		DisassembleDebug("removed unused local stores");
	}

	// Remove unused global stores

	if (mModule->mGlobalVars.Size())
	{
		do {
			ResetVisited();
			mEntryBlock->BuildStaticVariableSet(mModule->mGlobalVars);

			ResetVisited();
			mEntryBlock->BuildGlobalProvidedStaticVariableSet(mModule->mGlobalVars, NumberSet(mModule->mGlobalVars.Size()));

			NumberSet	totalRequired2(mModule->mGlobalVars.Size());

			do {
				ResetVisited();
			} while (mEntryBlock->BuildGlobalRequiredStaticVariableSet(mModule->mGlobalVars, totalRequired2));

			ResetVisited();
		} while (mEntryBlock->RemoveUnusedStaticStoreInstructions(mModule->mGlobalVars));

		DisassembleDebug("removed unused static stores");
	}
}

void InterCodeProcedure::MergeCommonPathInstructions(void)
{
	bool	changed;
	do
	{
		ResetVisited();
		mEntryBlock->CompactInstructions();

		BuildDataFlowSets();

		ResetVisited();
		changed = mEntryBlock->MergeCommonPathInstructions();

		DisassembleDebug("Merged common path part");

		if (changed)
		{
			TempForwarding();
			RemoveUnusedInstructions();

		}

	} while (changed);

	DisassembleDebug("Merged common path instructions");
}

void InterCodeProcedure::PushSinglePathResultInstructions(void)
{
	bool	changed;
	do
	{
		BuildDataFlowSets();

		ResetVisited();
		changed = mEntryBlock->PushSinglePathResultInstructions();

		DisassembleDebug("Pushed single path result");

	} while (changed);
}

void InterCodeProcedure::PromoteSimpleLocalsToTemp(InterMemory paramMemory, int nlocals, int nparams)
{
	ResetVisited();
	mEntryBlock->CollectVariables(mModule->mGlobalVars, mLocalVars, mParamVars, paramMemory);

	RemoveUnusedStoreInstructions(paramMemory);

	for (int j = 0; j < 2; j++)
	{
		//
		// Promote local variables to temporaries
		//

		FastNumberSet	simpleLocals(nlocals), complexLocals(nlocals);
		GrowingTypeArray	localTypes(IT_NONE);

		FastNumberSet	simpleParams(nparams), complexParams(nparams);
		GrowingTypeArray	paramTypes(IT_NONE);

		ResetVisited();
		mEntryBlock->CollectSimpleLocals(complexLocals, simpleLocals, localTypes, complexParams, simpleParams, paramTypes);

		for (int i = 0; i < simpleLocals.Num(); i++)
		{
			int vi = simpleLocals.Element(i);
			if (!complexLocals[vi])
			{
				mLocalVars[vi]->mTemp = true;
				ResetVisited();
				mEntryBlock->SimpleLocalToTemp(vi, AddTemporary(localTypes[vi]));
			}
		}

		DisassembleDebug("local variables to temps");

		BuildTraces(false);

		BuildDataFlowSets();

		RenameTemporaries();

		do {
			BuildDataFlowSets();

			TempForwarding();
		} while (GlobalConstantPropagation());

		//
		// Now remove unused instructions
		//

		RemoveUnusedInstructions();

		DisassembleDebug("removed unused instructions 2");

		TempForwarding();
	}


	ResetVisited();
	mEntryBlock->CompactInstructions();
}

void InterCodeProcedure::MergeIndexedLoadStore(void)
{
	GrowingInstructionPtrArray	silvalues(nullptr);

	do
	{
		BuildDataFlowSets();

		TempForwarding();
		RemoveUnusedInstructions();

		silvalues.SetSize(mTemporaries.Size(), true);

		ResetVisited();
	} while (mEntryBlock->MergeIndexedLoadStore(silvalues));

	BuildDataFlowSets();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("MergeIndexedLoadStore");
}

void InterCodeProcedure::SimplifyIntegerNumeric(FastNumberSet& activeSet)
{
	GrowingInstructionPtrArray	silvalues(nullptr);
	int							silvused = mTemporaries.Size();

	do
	{
		mTemporaries.SetSize(silvused, true);

		BuildDataFlowSets();

		TempForwarding();
		RemoveUnusedInstructions();

		activeSet.Clear();

		ResetVisited();
		mEntryBlock->CollectActiveTemporaries(activeSet);

		silvused = activeSet.Num();
		silvalues.SetSize(silvused + 16, true);

		mTemporaries.SetSize(activeSet.Num(), true);

		ResetVisited();
		mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

		ResetVisited();
		mEntryBlock->RemapActiveTemporaries(activeSet);

		ResetVisited();
	} while (mEntryBlock->SimplifyIntegerNumeric(silvalues, silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("SimplifyIntegerNumeric");
}

void InterCodeProcedure::ExpandSelect(void)
{
#if 1
	ResetVisited();
	mEntryBlock->ExpandSelect(this);

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	DisassembleDebug("ExpandSelect");
#endif
}

void InterCodeProcedure::EliminateAliasValues()
{
	GrowingInstructionPtrArray	eivalues(nullptr);
	do {
		BuildDataFlowSets();

		assert(mTemporaries.Size() == mEntryBlock->mLocalValueRange.Size());

		eivalues.SetSize(mTemporaries.Size(), true);

		ResetVisited();
	} while (mEntryBlock->EliminateAliasValues(eivalues, eivalues));

	DisassembleDebug("EliminateAliasValues");
}

void InterCodeProcedure::LoadStoreForwarding(InterMemory paramMemory)
{
	bool changed;
	do {
		GrowingInstructionPtrArray	gipa(nullptr);
		ResetVisited();
		changed = mEntryBlock->LoadStoreForwarding(gipa, mModule->mGlobalVars);

		DisassembleDebug("Load/Store forwardingX");

		RemoveUnusedStoreInstructions(paramMemory);

		TempForwarding();
		RemoveUnusedInstructions();

		DisassembleDebug("Load/Store forwarding");
	} while (changed);
}

void InterCodeProcedure::Close(void)
{
	int				i, j, k, start;
	GrowingTypeArray	tstack(IT_NONE);

	mEntryBlock = mBlocks[0];

	DisassembleDebug("start");

	BuildTraces(true);

	ResetVisited();
	mLeafProcedure = mEntryBlock->IsLeafProcedure();

	mHasDynamicStack = false;
	mHasInlineAssembler = false;
	mCallsByteCode = false;
	if (!mLeafProcedure)
	{
		int		size = 0;

		ResetVisited();
		mEntryBlock->CollectOuterFrame(0, size, mHasDynamicStack, mHasInlineAssembler, mCallsByteCode);

		if (mModule->mCompilerOptions & COPT_NATIVE)
			mCallsByteCode = false;
		mCommonFrameSize = size;
	}
	else
		mCommonFrameSize = 0;

	BuildDataFlowSets();

	RenameTemporaries();

	BuildDataFlowSets();

	TempForwarding();

	int	numTemps = mTemporaries.Size();

	//
	// Find all local variables that are never aliased
	//
	GrowingIntArray		localTable(-1), paramTable(-1);
	ResetVisited();
	mEntryBlock->CollectLocalAddressTemps(localTable, paramTable);

	int			nlocals = 0, nparams = 0;
	for (int i = 0; i < localTable.Size(); i++)
		if (localTable[i] + 1 > nlocals)
			nlocals = localTable[i] + 1;
	for (int i = 0; i < paramTable.Size(); i++)
		if (paramTable[i] + 1 > nparams)
			nparams = paramTable[i] + 1;

	mLocalAliasedSet.Reset(nlocals);
	mParamAliasedSet.Reset(nparams);
	ResetVisited();
	mEntryBlock->MarkAliasedLocalTemps(localTable, mLocalAliasedSet, paramTable, mParamAliasedSet);

	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps + 32);


	bool	eliminated;
	int		retries = 2;
	//
	//	Now forward constant values
	//
	do {
		valueSet.FlushAll();
		mValueForwardingTable.SetSize(numTemps, true);
		tvalidSet.Reset(numTemps + 32);

		ResetVisited();
		mEntryBlock->PerformValueForwarding(mValueForwardingTable, valueSet, tvalidSet, mLocalAliasedSet, mParamAliasedSet, numTemps, mModule->mGlobalVars);

		ResetVisited();
		eliminated = mEntryBlock->EliminateDeadBranches();
		if (eliminated)
		{
			BuildTraces(false);
			/*
			ResetVisited();
			for (int i = 0; i < mBlocks.Size(); i++)
				mBlocks[i]->mNumEntries = 0;
			mEntryBlock->CollectEntries();
			*/
		}

		mTemporaries.SetSize(numTemps, true);

		BuildDataFlowSets();

		TempForwarding();
		retries--;

	} while (eliminated || retries > 0);


	DisassembleDebug("value forwarding");

	mValueForwardingTable.SetSize(numTemps, true);
	mTemporaries.SetSize(numTemps, true);

	ResetVisited();
	mEntryBlock->PerformMachineSpecificValueUsageCheck(mValueForwardingTable, tvalidSet, mModule->mGlobalVars);

	GlobalConstantPropagation();

	DisassembleDebug("machine value forwarding");

	//
	// Now remove needless temporary moves, that apear due to
	// stack evaluation
	//
	mTempForwardingTable.Reset();
	mTempForwardingTable.SetSize(numTemps);

	ResetVisited();
	mEntryBlock->PerformTempForwarding(mTempForwardingTable);

	DisassembleDebug("temp forwarding 2");


	//
	// Now remove unused instructions
	//

	RemoveUnusedInstructions();

	DisassembleDebug("removed unused instructions");

	InterMemory	paramMemory = mFastCallProcedure ? IM_FPARAM : IM_PARAM;

	PromoteSimpleLocalsToTemp(paramMemory, nlocals, nparams);

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->OptimizeIntervalCompare();

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	DisassembleDebug("interval compare");

	MergeIndexedLoadStore();

	PeepholeOptimization();

	DisassembleDebug("Peephole optimized");

	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	TempForwarding();
	RemoveUnusedInstructions();

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

	DisassembleDebug("single block loop opt");

	BuildDataFlowSets();

	RenameTemporaries();

	BuildDataFlowSets();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

#if 1
	ResetVisited();
	mEntryBlock->InnerLoopOptimization(mParamAliasedSet);

	DisassembleDebug("inner loop opt");

	BuildDataFlowSets();
#endif
	CheckUsedDefinedTemps();

	ExpandSelect();

	BuildDataFlowSets();

	CheckUsedDefinedTemps();

	SingleAssignmentForwarding();

	CheckUsedDefinedTemps();

	PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Peephole optimized");


	bool changed = false;

	PushSinglePathResultInstructions();

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Moved single path instructions");

	PropagateNonLocalUsedTemps();

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("propagate non local used temps");

	MergeCommonPathInstructions();

	do
	{
		GrowingInstructionPtrArray	cipa(nullptr);
		ResetVisited();
		changed = mEntryBlock->PropagateVariableCopy(cipa, mModule->mGlobalVars);

		RemoveUnusedStoreInstructions(paramMemory);
	} while (changed);

	DisassembleDebug("Copy forwarding");

	LoadStoreForwarding(paramMemory);

	FastNumberSet	activeSet(numTemps);

	//
	// And remove unused temporaries
	//

	ResetVisited();
	mEntryBlock->CollectActiveTemporaries(activeSet);

	mTemporaries.SetSize(activeSet.Num(), true);

	ResetVisited();
	mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();

#if 1
	ResetVisited();
	mEntryBlock->BuildLocalIntegerRangeSets(mTemporaries.Size(), mLocalVars);

	do {
		DisassembleDebug("tt");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(true, mLocalVars));

	do {
		DisassembleDebug("tq");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(false, mLocalVars));


	DisassembleDebug("Estimated value range");
#if 1
	ResetVisited();
	mEntryBlock->RestartLocalIntegerRangeSets(mLocalVars);

	do {
		DisassembleDebug("tr");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(true, mLocalVars));

	do {
		DisassembleDebug("tr");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(false, mLocalVars));

	DisassembleDebug("Estimated value range 2");
#endif
	ResetVisited();
	mEntryBlock->SimplifyIntegerRangeRelops();

	DisassembleDebug("Simplified range limited relational ops");
#endif

	BuildTraces(false);
	DisassembleDebug("Rebuilt traces");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

#if 1
	SimplifyIntegerNumeric(activeSet);

#endif

	EliminateAliasValues();

	MergeIndexedLoadStore();

#if 1
	DisassembleDebug("PreMoveTrainCrossBlockA");

	PeepholeOptimization();

#if 1
	DisassembleDebug("PreMoveTrainCrossBlockB");

	ResetVisited();
	mEntryBlock->MoveTrainCrossBlock();
#endif
	PeepholeOptimization();

	DisassembleDebug("MoveTrainCrossBlock");

#endif


#if 1
	ResetVisited();
	mEntryBlock->RestartLocalIntegerRangeSets(mLocalVars);

	do {
		DisassembleDebug("tr");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(true, mLocalVars));

	do {
		DisassembleDebug("tr");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(false, mLocalVars));

	DisassembleDebug("Estimated value range 2");
#endif

#if 1
	if (mModule->mCompilerOptions & COPT_OPTIMIZE_AUTO_UNROLL)
	{
		ResetVisited();
		mEntryBlock->SingleBlockLoopUnrolling();

		DisassembleDebug("Single Block loop unrolling");
	}
#endif

#if 1
	ResetVisited();
	mEntryBlock->DropUnreachable();

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->ForwardDiamondMovedTemp();
	DisassembleDebug("Diamond move forwarding");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();
	DisassembleDebug("Removed unreachable branches");

	BuildTraces(false);
	DisassembleDebug("Rebuilt traces");

	do {
		TempForwarding();
	} while (GlobalConstantPropagation());

#endif

#if 1
	PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Peephole optimized");

#endif

	LoadStoreForwarding(paramMemory);

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

	DisassembleDebug("single block loop opt 2");

	BuildDataFlowSets();

	BuildTraces(false);
	DisassembleDebug("Rebuilt traces");
#endif

#if 1
	PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Peephole optimized");

#endif

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

	DisassembleDebug("single block loop opt 3");

	BuildDataFlowSets();

	BuildTraces(false);
	DisassembleDebug("Rebuilt traces");
#endif

#if 1
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	do {
		changed = false;

		ResetVisited();
		mEntryBlock->BuildConstTempSets();

		ResetVisited();
		if (mEntryBlock->PropagateConstOperationsUp())
		{
			BuildDataFlowSets();

			GlobalConstantPropagation();

			TempForwarding();

			RemoveUnusedInstructions();

			changed = true;

			DisassembleDebug("prop const op up");
		}
	} while (changed);
#endif

#if 1
	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->ForwardDiamondMovedTemp();
	DisassembleDebug("Diamond move forwarding 2");

	TempForwarding();

	RemoveUnusedInstructions();
#endif

#if 1
	ResetVisited();
	if (!mInterruptCalled && mEntryBlock->CheckStaticStack())
	{
		mLinkerObject->mFlags |= LOBJF_STATIC_STACK;
		mLinkerObject->mStackSection = mModule->mLinker->AddSection(mIdent, LST_STATIC_STACK);

		for (int i = 0; i < mLocalVars.Size(); i++)
		{
			InterVariable* var(mLocalVars[i]);
			if (var && !var->mTemp && !var->mLinkerObject)
			{
				var->mLinkerObject = mModule->mLinker->AddObject(mLocation, var->mIdent, mLinkerObject->mStackSection, LOT_BSS);
				var->mLinkerObject->AddSpace(var->mSize);
				var->mIndex = mModule->mGlobalVars.Size();
				mModule->mGlobalVars.Push(var);
			}
		}

		mSaveTempsLinkerObject = mModule->mLinker->AddObject(mLocation, mIdent, mLinkerObject->mStackSection, LOT_BSS);

		ResetVisited();
		mEntryBlock->CollectStaticStack(mLinkerObject, mLocalVars);
	}
#endif

#if 1
	do {
		TempForwarding();
	} while (GlobalConstantPropagation());

	PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Global Constant Prop 1");

#endif

#if 1
	SimplifyIntegerNumeric(activeSet);

#endif

#if 1
	for (int i = 0; i < 4; i++)
	{
		PeepholeOptimization();

		DisassembleDebug("Peephole Temp Check");

		ReduceTemporaries();

		MergeBasicBlocks();

		BuildDataFlowSets();

		TempForwarding();

		BuildLoopPrefix();

		BuildDataFlowSets();

		DisassembleDebug("Checking Unused");

		RemoveUnusedInstructions();

		DisassembleDebug("Checked Unused");

		BuildDataFlowSets();

		RenameTemporaries();

		BuildDataFlowSets();

		TempForwarding();
#if 1

		BuildDataFlowSets();
		do {
			TempForwarding();
		} while (GlobalConstantPropagation());

		LoadStoreForwarding(paramMemory);

		MergeCommonPathInstructions();
#if 1
		PushSinglePathResultInstructions();
#endif

		TempForwarding();
		RemoveUnusedInstructions();
#endif

		DisassembleDebug("Global Constant Prop 2");
	}
#endif

	MapVariables();

	DisassembleDebug("mapped variabled");

	ReduceTemporaries();

	DisassembleDebug("Reduced Temporaries");

	// Optimize for size

	MergeBasicBlocks();
	BuildTraces(false, false, true);
	DisassembleDebug("Final Merged basic blocks");

	BuildDataFlowSets();

	MapCallerSavedTemps();

	if (mSaveTempsLinkerObject && mTempSize > 16)
		mSaveTempsLinkerObject->AddSpace(mTempSize - 16);
}

void InterCodeProcedure::AddCalledFunction(InterCodeProcedure* proc)
{
	mCalledFunctions.Push(proc);
}

void InterCodeProcedure::CallsFunctionPointer(void)
{
	mCallsFunctionPointer = true;
}

void InterCodeProcedure::MarkRelevantStatics(void)
{
	ResetVisited();
	mEntryBlock->MarkRelevantStatics();

}

void InterCodeProcedure::RemoveNonRelevantStatics(void)
{
	ResetVisited();
	mEntryBlock->RemoveNonRelevantStatics();
	RemoveUnusedInstructions();
}

void InterCodeProcedure::MapVariables(void)
{
	ResetVisited();
	mEntryBlock->MapVariables(mModule->mGlobalVars, mLocalVars);
	mLocalSize = 0;
	for (int i = 0; i < mLocalVars.Size(); i++)
	{
		if (mLocalVars[i] && mLocalVars[i]->mUsed && !mLocalVars[i]->mLinkerObject)
		{
			mLocalVars[i]->mOffset = mLocalSize;
			mLocalSize += mLocalVars[i]->mSize;
		}
	}
}

bool InterCodeBasicBlock::SameExitCode(const InterCodeBasicBlock* block) const
{
	if (mInstructions.Size() > 1 && block->mInstructions.Size() > 1)
	{
		InterInstruction* ins0 = mInstructions[mInstructions.Size() - 2];
		InterInstruction* ins1 = block->mInstructions[block->mInstructions.Size() - 2];

		if (ins0->IsEqual(ins1))
		{
			if (ins0->mCode == IC_STORE && ins0->mSrc[1].mTemp >= 0)
			{
				int	j0 = mInstructions.Size() - 2;
				while (j0 >= 0 && mInstructions[j0]->mDst.mTemp != ins0->mSrc[1].mTemp)
					j0--;
				int	j1 = block->mInstructions.Size() - 2;
				while (j1 >= 0 && block->mInstructions[j1]->mDst.mTemp != ins0->mSrc[1].mTemp)
					j1--;

				if (j0 >= 0 && j1 >= 0)
				{
					if (!(mInstructions[j0]->IsEqual(block->mInstructions[j1])))
					{
						if (mInstructions[j0]->mCode == IC_LEA && mInstructions[j0]->mSrc[1].mTemp < 0)
							return false;
						if (block->mInstructions[j1]->mCode == IC_LEA && mInstructions[j1]->mSrc[1].mTemp < 0)
							return false;
					}
				}
			}

			return true;
		}
	}

	return false;
}

bool PartitionSameExitCode(GrowingArray<InterCodeBasicBlock* > & eblocks, GrowingArray<InterCodeBasicBlock* > & mblocks)
{
	int i = 0;

	mblocks.SetSize(0, true);

	while (i + 1 < eblocks.Size())
	{
		int j = i + 1;
		while (j < eblocks.Size())
		{
			if (eblocks[i]->SameExitCode(eblocks[j]))
			{
				mblocks.Push(eblocks[j]);
				eblocks.Remove(j);
			}
			else
				j++;
		}

		if (mblocks.Size())
		{
			mblocks.Push(eblocks[i]);
			eblocks.Remove(i);
			return true;
		}

		i++;
	}

	return false;
}

void InterCodeProcedure::MergeBasicBlocks(void)
{
	ResetVisited();
	mEntryBlock->FollowJumps();

	ResetVisited();
	mEntryBlock->SplitBranches(this);

	DisassembleDebug("PostSplit");

	bool	changed;
	do
	{
		changed = false;

		GrowingArray<InterCodeBasicBlock* >	blockMap(nullptr);

		for (int i = 0; i < mBlocks.Size(); i++)
		{
			InterCodeBasicBlock* block = mBlocks[i];
			if (block->mNumEntries)
			{
				int j = 0;
				while (j < i && !(mBlocks[j]->mNumEntries && mBlocks[j]->IsEqual(block)))
					j++;
				blockMap[i] = mBlocks[j];
			}
		}


		if (mEntryBlock != blockMap[mEntryBlock->mIndex])
		{
			mEntryBlock = blockMap[mEntryBlock->mIndex];
			changed = true;
		}

		for (int i = 0; i < mBlocks.Size(); i++)
		{
			InterCodeBasicBlock* block = mBlocks[i];
			if (block->mNumEntries)
			{	
				if (block->mTrueJump && block->mTrueJump != blockMap[block->mTrueJump->mIndex])
				{
					block->mTrueJump = blockMap[block->mTrueJump->mIndex];
					changed = true;
				}
				if (block->mFalseJump && block->mFalseJump != blockMap[block->mFalseJump->mIndex])
				{
					block->mFalseJump = blockMap[block->mFalseJump->mIndex];
					changed = true;
				}
			}
		}

		if (changed)
		{
			ResetVisited();
			for (int i = 0; i < mBlocks.Size(); i++)
				mBlocks[i]->mNumEntries = 0;
			mEntryBlock->CollectEntries();
		}

		for (int i = 0; i < mBlocks.Size(); i++)
		{
			InterCodeBasicBlock* block = mBlocks[i];
#if 0
			// too eager will need some rework
			if (block->mTrueJump && block->mFalseJump && block->mTrueJump->mNumEntries == 1 && block->mFalseJump->mNumEntries == 1)
			{
				while (block->mTrueJump->mInstructions.Size() && block->mFalseJump->mInstructions.Size() &&
					block->mTrueJump->mInstructions[0]->IsEqual(block->mFalseJump->mInstructions[0]) &&
					block->mTrueJump->mInstructions[0]->mCode != IC_BRANCH && 
					block->mTrueJump->mInstructions[0]->mCode != IC_JUMP &&
					block->mTrueJump->mInstructions[0]->mCode != IC_RELATIONAL_OPERATOR &&
					block->mTrueJump->mInstructions[0]->mDst.mTemp != block->mInstructions.Last()->mSrc[0].mTemp)
				{
					if (block->mInstructions.Size() >= 2 && CanBypass(block->mTrueJump->mInstructions[0], block->mInstructions[block->mInstructions.Size() - 2]))
						block->mInstructions.Insert(block->mInstructions.Size() - 2, block->mTrueJump->mInstructions[0]);
					else
						block->mInstructions.Insert(block->mInstructions.Size() - 1, block->mTrueJump->mInstructions[0]);
					block->mTrueJump->mInstructions.Remove(0);
					block->mFalseJump->mInstructions.Remove(0);
					changed = true;
				}
			}
#endif
			if (block->mNumEntries >= 2)
			{
				GrowingArray<InterCodeBasicBlock* >	eblocks(nullptr);

				for (int j = 0; j < mBlocks.Size(); j++)
				{
					InterCodeBasicBlock* eblock = mBlocks[j];
					if (eblock->mNumEntries > 0 && eblock->mTrueJump == block && !eblock->mFalseJump)
						eblocks.Push(eblock);
				}

				if (eblocks.Size() == block->mNumEntries)
				{
					GrowingArray<InterCodeBasicBlock* >	mblocks(nullptr);

					while (PartitionSameExitCode(eblocks, mblocks))
					{
						InterCodeBasicBlock* nblock;

						if (eblocks.Size() || mblocks.IndexOf(block) != -1)
						{
//							break;

							nblock = new InterCodeBasicBlock();
							this->Append(nblock);

							for (int i = 0; i < mblocks.Size(); i++)
								mblocks[i]->mTrueJump = nblock;
							block->mNumEntries -= mblocks.Size();

							InterInstruction* jins = new InterInstruction();
							jins->mCode = IC_JUMP;
							nblock->mInstructions.Push(jins);
							nblock->Close(block, nullptr);

							nblock->mNumEntries = mblocks.Size();
							block->mNumEntries++;

							eblocks.Push(nblock);
						}
						else
							nblock = block;

						InterInstruction* ins = mblocks[0]->mInstructions[mblocks[0]->mInstructions.Size() - 2];

						nblock->mInstructions.Insert(0, ins);
						for (int j = 0; j < mblocks.Size(); j++)
						{
							assert(mblocks[j]->mInstructions[mblocks[j]->mInstructions.Size() - 1]->mCode == IC_JUMP);
							assert(mblocks[j]->mInstructions[mblocks[j]->mInstructions.Size() - 2]->IsEqual(ins));

							mblocks[j]->mInstructions.Remove(mblocks[j]->mInstructions.Size() - 2);
						}
						changed = true;
					}
				}
			}
		}

	} while (changed);

	ResetVisited();
	mEntryBlock->FollowJumps();

}

void InterCodeProcedure::BuildLoopPrefix(void)
{
	ResetVisited();

	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mLoopPrefix = nullptr;

	mEntryBlock = mEntryBlock->BuildLoopPrefix(this);

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	ResetVisited();
	mEntryBlock->BuildLoopSuffix(this);
}

bool InterCodeProcedure::PropagateNonLocalUsedTemps(void)
{
	ResetVisited();
	mEntryBlock->CollectLocalUsedTemps(mTemporaries.Size());

	ResetVisited();
	return mEntryBlock->PropagateNonLocalUsedConstTemps();
}

bool InterCodeProcedure::GlobalConstantPropagation(void)
{
	NumberSet					assignedTemps(mTemporaries.Size());
	GrowingInstructionPtrArray	ctemps(nullptr);

	ResetVisited();
	mEntryBlock->CollectConstTemps(ctemps, assignedTemps);

	ResetVisited();
	return mEntryBlock->PropagateConstTemps(ctemps);
}

void InterCodeProcedure::ReduceTemporaries(void)
{
	NumberSet* collisionSet;
	int i, j, numRenamedTemps;
	int numTemps = mTemporaries.Size();

	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numTemps);

	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps));

	NumberSet	totalRequired2(numTemps);

	do {
		ResetVisited();
	} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

	collisionSet = new NumberSet[numTemps];

	for (i = 0; i < numTemps; i++)
		collisionSet[i].Reset(numTemps);

	ResetVisited();
	mEntryBlock->BuildCollisionTable(collisionSet);

	mRenameTable.SetSize(numTemps, true);

	numRenamedTemps = 0;

	NumberSet	usedTemps(numTemps);

	for (i = 0; i < numTemps; i++)
	{
		usedTemps.Clear();

		for (j = 0; j < numTemps; j++)
		{
			if (mRenameTable[j] >= 0 && (collisionSet[i][j] || InterTypeSize[mTemporaries[j]] != InterTypeSize[mTemporaries[i]]))
			{
				usedTemps += mRenameTable[j];
			}
		}

		j = 0;
		while (usedTemps[j])
			j++;

		mRenameTable[i] = j;
		if (j >= numRenamedTemps) numRenamedTemps = j + 1;
	}

	mTemporaries.SetSize(numRenamedTemps, true);

	ResetVisited();
	mEntryBlock->GlobalRenameRegister(mRenameTable, mTemporaries);

	delete[] collisionSet;

	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numRenamedTemps);

	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numRenamedTemps));

	NumberSet	totalRequired3(numRenamedTemps);

	do {
		ResetVisited();
	} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired3));
}

void InterCodeProcedure::MapCallerSavedTemps(void)
{
	NumberSet	callerSaved(mTemporaries.Size());
	ResetVisited();
	mEntryBlock->BuildCallerSaveTempSet(callerSaved);

	int		callerSavedTemps = 0, calleeSavedTemps = 16, freeCallerSavedTemps = 0, freeTemps = 0;

	if (mCallsFunctionPointer)
		freeCallerSavedTemps = 16;
	else
	{
		for (int i = 0; i < mCalledFunctions.Size(); i++)
		{
			InterCodeProcedure* proc = mCalledFunctions[i];
			if (proc->mCallerSavedTemps > freeCallerSavedTemps)
				freeCallerSavedTemps = proc->mCallerSavedTemps;
		}
	}

	callerSavedTemps = freeCallerSavedTemps;
	mFreeCallerSavedTemps = freeCallerSavedTemps;

	int maxCallerSavedTemps = mCallerSavedTemps;

	assert(freeCallerSavedTemps <= mCallerSavedTemps);

	mTempOffset.SetSize(mTemporaries.Size(), true);
	mTempSizes.SetSize(mTemporaries.Size(), true);

	for (int i = 0; i < mTemporaries.Size(); i++)
	{
		if (!callerSaved[i])
		{
			int size = InterTypeSize[mTemporaries[i]];

			if (freeTemps + size <= freeCallerSavedTemps)
			{
				mTempOffset[i] = freeTemps;
				mTempSizes[i] = size;
				freeTemps += size;
			}
		}
	}

//	if (freeTemps > callerSavedTemps)
//		callerSavedTemps = freeTemps;

	for (int i = 0; i < mTemporaries.Size(); i++)
	{
		if (!mTempSizes[i])
		{
			int size = InterTypeSize[mTemporaries[i]];

			if (callerSavedTemps + size <= maxCallerSavedTemps)
			{
				mTempOffset[i] = callerSavedTemps;
				mTempSizes[i] = size;
				callerSavedTemps += size;
			}
			else
			{
				mTempOffset[i] = calleeSavedTemps;
				mTempSizes[i] = size;
				calleeSavedTemps += size;
			}
		}
	}

	mTempSize = calleeSavedTemps;
	mCallerSavedTemps = callerSavedTemps;

#if 0
	printf("Map %s, %d, %d, %d, %d\n", mIdent->mString, freeTemps, callerSavedTemps, calleeSavedTemps, freeCallerSavedTemps);
	for (int i = 0; i < mTempOffset.Size(); i++)
		printf("T%02d : %d, %d\n", i, mTempOffset[i], mTempSizes[i]);
#endif

	if (mSaveTempsLinkerObject && mTempSize > 16)
		mSaveTempsLinkerObject->AddSpace(mTempSize - 16);

//	printf("Map %s, %d, %d, %d, %d\n", mIdent->mString, freeTemps, callerSavedTemps, calleeSavedTemps, freeCallerSavedTemps);
}

void InterCodeProcedure::Disassemble(FILE* file)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	fprintf(file, "%s: %s:%d\n", mIdent->mString, mLocation.mFileName, mLocation.mLine);

	static char typechars[] = "NBCILFP";
	for (int i = 0; i < mTemporaries.Size(); i++)
	{
		fprintf(file, "$%02x R%d(%c), ", mTempOffset[i], i, typechars[mTemporaries[i]]);
	}

	fprintf(file, "\n");

	ResetVisited();
	mEntryBlock->Disassemble(file, false);
}

void InterCodeProcedure::Disassemble(const char* name, bool dumpSets)
{
#if 0
#ifdef _WIN32
	FILE* file;
	static bool	initial = true;

	if (!initial)
	{
		fopen_s(&file, "r:\\cldiss.txt", "a");
	}
	else
	{
		fopen_s(&file, "r:\\cldiss.txt", "w");
		initial = false;
	}

	if (file)
	{
		fprintf(file, "--------------------------------------------------------------------\n");
		fprintf(file, "%s : %s:%d\n", name, mLocation.mFileName, mLocation.mLine);

		if (mTempOffset.Size())
		{
			static char typechars[] = "NBCILFP";
			for (int i = 0; i < mTemporaries.Size(); i++)
			{
				fprintf(file, "$%02x T%d(%c), ", mTempOffset[i], i, typechars[mTemporaries[i]]);
			}
		}
		fprintf(file, "\n");

		ResetVisited();
		mEntryBlock->Disassemble(file, dumpSets);

		fclose(file);
	}
#endif
#endif
}

InterCodeModule::InterCodeModule(Linker * linker)
	: mLinker(linker), mGlobalVars(nullptr), mProcedures(nullptr), mCompilerOptions(0)
{
}

InterCodeModule::~InterCodeModule(void)
{

}

bool InterCodeModule::Disassemble(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		for (int i = 0; i < mProcedures.Size(); i++)
		{
			InterCodeProcedure* proc = mProcedures[i];

			proc->Disassemble(file);
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

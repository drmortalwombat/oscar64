#include "InterCode.h"
#include "CompilerTypes.h"

#include <stdio.h>
#include <math.h>
#include <algorithm>

#define DISASSEMBLE_OPT	0

static bool CheckFunc;
static bool CheckCase;

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

static bool IsScalarType(InterType type)
{
	return type >= IT_INT8 && type <= IT_INT32 || type == IT_BOOL;
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

static int64 UnsignedTypeMax(InterType type)
{
	switch (type)
	{
	case IT_INT8:
		return 255;
	case IT_INT16:
		return 65535;
	default:
		return 0xffffffff;
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

static int64 BinMask(int64 n)
{
	n |= n >> 32;
	n |= n >> 16;
	n |= n >> 8;
	n |= n >> 4;
	n |= n >> 2;
	n |= n >> 1;
	return n;
}

static int64 LimitIntConstValue(InterType type, int64 v)
{
	switch (type)
	{
	case IT_INT8:
		if (v >= -128 && v < 256)
			return v;
		else
			return v & 255;
		break;
	case IT_INT16:
		if (v >= -32768 && v < 65536)
			return v;
		else
			return v & 65535;
		break;
	default:
	case IT_INT32:
		return v;
	}
}


static int64 ToTypedSigned(int64 val, InterType type)
{
	switch (InterTypeSize[type])
	{
	case 1:
		return int64(int8(val));
	case 4:
		return int64(int32(val));
	default:
		return int64(int16(val));
	}
}

static int64 ToTypedUnsigned(int64 val, InterType type)
{
	switch (InterTypeSize[type])
	{
	case 1:
		return int64(uint8(val));
	case 4:
		return int64(uint32(val));
	default:
		return int64(uint16(val));
	}
}

static int64 TypeShiftMask(InterType type, int64 val)
{
	switch (InterTypeSize[type])
	{
	case 1:
		return val & 7;
	default:
	case 2:
		return val & 15;
	case 4:
		return val & 31;
	}
}


IntegerValueRange::IntegerValueRange(void)
	: mMinState(S_UNKNOWN), mMaxState(S_UNKNOWN)
{
	mMinExpanded = 0;
	mMaxExpanded = 0;
}

IntegerValueRange::~IntegerValueRange(void)
{}

void IntegerValueRange::Reset(void)
{
	mMinState = S_UNKNOWN;
	mMaxState = S_UNKNOWN;
	mMinExpanded = 0;
	mMaxExpanded = 0;
}

void IntegerValueRange::Restart(void)
{
	if (mMinState == IntegerValueRange::S_UNBOUND)
		mMinState = IntegerValueRange::S_UNKNOWN;
	if (mMaxState == IntegerValueRange::S_UNBOUND)
		mMaxState = IntegerValueRange::S_UNKNOWN;
}


bool IntegerValueRange::Weaker(const IntegerValueRange& range) const
{
	bool	minWeak = false, maxWeak = false;

	if (range.mMinState == S_UNKNOWN)
		minWeak = false;
	else if (mMinState == S_UNKNOWN)
		minWeak = true;
	else if (mMinState == S_BOUND)
	{
		if (range.mMinState >= S_WEAK && mMinValue < range.mMinValue)
			minWeak = true;
	}
	else if (mMinState == S_WEAK)
	{
		if (range.mMinState == S_BOUND || range.mMinState == S_UNBOUND)
			minWeak = true;
		if (range.mMinState == S_WEAK && mMinValue != range.mMinValue)
			minWeak = true;
	}
	else if (mMinState == S_UNBOUND)
	{
		if (mMinExpanded >= 32 && range.mMinState == S_WEAK)
			;
		else if (range.mMinState != S_UNBOUND)
			minWeak = true;
	}

	if (range.mMaxState == S_UNKNOWN)
		maxWeak = false;
	else if (mMaxState == S_UNKNOWN)
		maxWeak = true;
	else if (mMaxState == S_BOUND)
	{
		if (range.mMaxState >= S_WEAK && mMaxValue > range.mMaxValue)
			maxWeak = true;
	}
	else if (mMaxState == S_WEAK)
	{
		if (range.mMaxState == S_BOUND || range.mMaxState == S_UNBOUND)
			maxWeak = true;
		if (range.mMaxState == S_WEAK && mMaxValue != range.mMaxValue)
			maxWeak = true;
	}
	else if (mMaxState == S_UNBOUND)
	{
		if (mMaxExpanded >= 32 && range.mMaxState == S_WEAK)
			;
		else if (range.mMaxState != S_UNBOUND)
			maxWeak = true;
	}

	return minWeak || maxWeak;
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

void IntegerValueRange::LimitMinBound(int64 value)
{
	if (mMinState == S_BOUND && mMinValue < value)
		mMinValue = value;
}

void IntegerValueRange::LimitMaxBound(int64 value)
{
	if (mMaxState == S_BOUND && mMaxValue > value)
		mMaxValue = value;
}

void IntegerValueRange::LimitMinWeak(int64 value)
{
	if (mMinState == S_UNBOUND || mMinState != S_UNKNOWN && mMinValue < value)
	{
		mMinState = S_BOUND;
		mMinValue = value;
	}

}

void IntegerValueRange::AddConstValue(InterType type, int64 value)
{
	if (value > 0 && IntegerValueRange::S_WEAK)
		mMaxState = S_UNBOUND;
	else if (value < 0 && mMinState == S_WEAK)
		mMinState = S_UNBOUND;

	if (type == IT_INT8 && value >= 128 && mMaxState != S_BOUND)
		mMinState = S_UNBOUND;

	mMinValue += value;
	mMaxValue += value;

	if (type == IT_INT8)
	{
		if (mMinState == S_BOUND && mMinValue < -255 || mMaxState == S_BOUND && mMaxValue > 255)
		{
			LimitMax(255);
			LimitMin(-128);
		}
	}
}


void IntegerValueRange::LimitMaxWeak(int64 value)
{
	if (mMaxState == S_UNBOUND || mMaxState != S_UNKNOWN && mMaxValue > value)
	{
		mMaxState = S_BOUND;
		mMaxValue = value;
	}
}

bool IntegerValueRange::IsInvalid(void) const
{
	return mMinState == S_BOUND && mMaxState == S_BOUND && mMinValue > mMaxValue;
}

bool IntegerValueRange::IsBound(void) const
{
	return mMinState == S_BOUND && mMaxState == S_BOUND && mMinValue <= mMaxValue;
}

bool IntegerValueRange::IsConstant(void) const
{
	return mMinState == S_BOUND && mMaxState == S_BOUND && mMinValue == mMaxValue;
}

void IntegerValueRange::MergeUnknown(const IntegerValueRange& range)
{
	if (mMinState != S_BOUND)
	{
		mMinState = range.mMinState;
		mMinValue = range.mMinValue;
	}
	else if (range.mMinState == S_BOUND && mMinValue < range.mMinValue)
		mMinValue = range.mMinValue;

	if (mMaxState != S_BOUND)
	{
		mMaxState = range.mMaxState;
		mMaxValue = range.mMaxValue;
	}
	else if (range.mMaxState == S_BOUND && mMaxValue > range.mMaxValue)
		mMaxValue = range.mMaxValue;
}


void IntegerValueRange::LimitWeak(const IntegerValueRange& range)
{
	if (range.mMinState == S_BOUND)
		LimitMinWeak(range.mMinValue);
	if (range.mMaxState == S_BOUND)
		LimitMaxWeak(range.mMaxValue);
}

void IntegerValueRange::Limit(const IntegerValueRange& range)
{
	if (range.mMinState == S_BOUND)
		LimitMin(range.mMinValue);
	if (range.mMaxState == S_BOUND)
		LimitMax(range.mMaxValue);
}


void IntegerValueRange::SetConstant(int64 value)
{
	mMinState = S_BOUND;
	mMaxState = S_BOUND;
	mMinValue = value;
	mMaxValue = value;
}

void IntegerValueRange::SetLimit(int64 minValue, int64 maxValue)
{
	mMinState = S_BOUND;
	mMinValue = minValue;
	mMaxState = S_BOUND;
	mMaxValue = maxValue;
}

void IntegerValueRange::SetBounds(State minState, int64 minValue, State maxState, int64 maxValue)
{
	mMinState = minState;
	mMinValue = minValue;
	mMaxState = maxState;
	mMaxValue = maxValue;
}


void IntegerValueRange::Expand(const IntegerValueRange& range)
{
	if (mMinState == S_BOUND)
	{
		if (range.mMinState == S_BOUND)
		{
			if (range.mMinValue > mMinValue)
				mMinValue = range.mMinValue;
		}
		else if (range.mMinState == S_WEAK)
		{
			if (range.mMinValue > mMinValue)
			{
				mMinValue = range.mMinValue;
				mMinState = S_WEAK;
			}
		}
	}
	else if (mMinState == S_WEAK)
	{
		if (range.mMinState == S_BOUND)
		{
			mMinState = range.mMinState;
			mMinValue = range.mMinValue;
		}
		else if (range.mMinState == S_WEAK)
		{
			if (range.mMinValue != mMinValue)
			{
				mMinExpanded++;
				mMinValue = range.mMinValue;
				if (mMinExpanded >= 32)
					mMinState = S_UNBOUND;
			}
		}
		else if (range.mMinState == S_UNBOUND)
			mMinState = S_UNBOUND;
	}
	else if (mMinState == S_UNKNOWN || range.mMinState != S_UNKNOWN)
	{
		mMinState = range.mMinState;
		mMinValue = range.mMinValue;
	}

	if (mMaxState == S_BOUND)
	{
		if (range.mMaxState == S_BOUND)
		{
			if (range.mMaxValue < mMaxValue)
				mMaxValue = range.mMaxValue;
		}
		else if (range.mMaxState == S_WEAK)
		{
			if (range.mMaxValue < mMaxValue)
			{
				mMaxValue = range.mMaxValue;
				mMaxState = S_WEAK;
			}
		}
	}
	else if (mMaxState == S_WEAK)
	{
		if (range.mMaxState == S_BOUND)
		{
			mMaxValue = range.mMaxValue;
			mMaxState = S_BOUND;
		}
		else if (range.mMaxState == S_WEAK)
		{
			if (range.mMaxValue != mMaxValue)
			{
				mMaxExpanded++;
				mMaxValue = range.mMaxValue;
				if (mMaxExpanded >= 32)
					mMaxState = S_UNBOUND;
			}
		}
		else if (range.mMaxState == S_UNBOUND)
			mMaxState = S_UNBOUND;
	}
	else if (mMaxState == S_UNKNOWN || range.mMaxState != S_UNKNOWN)
	{
		mMaxState = range.mMaxState;
		mMaxValue = range.mMaxValue;
	}

#if 0
	if (range.mMinState == S_BOUND && mMinState == S_BOUND && range.mMinValue < mMinValue)
	{
		mMinValue = range.mMinValue;
		if (mMinExpanded >= 32)
			mMinState = S_UNBOUND;
		else
			mMinExpanded++;
	}
	else
	{
		mMinState = range.mMinState;
		mMinValue = range.mMinValue;
	}
	if (range.mMaxState == S_BOUND && mMaxState == S_BOUND && range.mMaxValue > mMaxValue)
	{
		mMaxValue = range.mMaxValue;
		if (mMaxExpanded >= 32)
			mMaxState = S_UNBOUND;
		else
			mMaxExpanded++;
	}
	else
	{
		mMaxState = range.mMaxState;
		mMaxValue = range.mMaxValue;
	}
#endif
}

void IntegerValueRange::Union(const IntegerValueRange& range)
{
	if (range.mMinState == S_UNBOUND || mMinState == S_UNBOUND)
		mMinState = S_UNBOUND;
	else if (range.mMinState == S_UNKNOWN || mMinState == S_UNKNOWN)
		mMinState = S_UNKNOWN;
	else
	{
		mMinValue = int64min(mMinValue, range.mMinValue);
		if (range.mMinState == S_WEAK)
			mMinState = S_WEAK;
	}
	if (range.mMaxState == S_UNBOUND || mMaxState == S_UNBOUND)
		mMaxState = S_UNBOUND;
	else if (range.mMaxState == S_UNKNOWN || mMaxState == S_UNKNOWN)
		mMaxState = S_UNKNOWN;
	else
	{
		mMaxValue = int64max(mMaxValue, range.mMaxValue);
		if (range.mMaxState == S_WEAK)
			mMaxState = S_WEAK;
	}
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
			offset = int(ins->mConst.mIntConst);

			return true;
		}
		else if (ins->mCode == IC_LEA)
		{
			mem = ins->mSrc[1].mMemory;
			vindex = ins->mSrc[1].mVarIndex;
			offset = int(ins->mSrc[1].mIntConst);

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
		if (ins->mCode == IC_COPY ||ins->mCode == IC_FILL)
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
			offset = int(ins->mSrc[0].mIntConst);
			size = ins->mSrc[0].mOperandSize;
		}
		else
		{
			mem = ins->mSrc[1].mMemory;
			vindex = ins->mSrc[1].mVarIndex;
			offset = int(ins->mSrc[1].mIntConst);
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

static bool CollidingMemType(InterType type1, InterType type2)
{
	if (type1 == IT_NONE || type2 == IT_NONE)
		return true;
	else// if (type1 == IT_POINTER || type1 == IT_FLOAT || type2 == IT_POINTER || type2 == IT_FLOAT)
		return type1 == type2;
//	else
//		return false;

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
	if (op1.mMemory != op2.mMemory || op1.mStride != op2.mStride)
		return false;

	if (op1.mStride == 1)
	{
		if (op1.mIntConst > op2.mIntConst || op1.mIntConst + op1.mOperandSize < op2.mIntConst + op2.mOperandSize)
			return false;
	}
	else
	{
		if (op1.mIntConst % op1.mStride != op2.mIntConst % op2.mStride)
			return false;
		if (op1.mIntConst > op2.mIntConst || op1.mIntConst + op1.mOperandSize * op1.mStride < op2.mIntConst + op2.mOperandSize * op2.mStride)
			return false;
	}

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
	if (op1.mMemory != op2.mMemory || op1.mType != op2.mType || op1.mIntConst != op2.mIntConst || op1.mOperandSize != op2.mOperandSize || op1.mStride != op2.mStride)
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

static bool SameMemAndSize(const InterOperand& op, const InterInstruction* ins)
{
	if (ins->mCode == IC_LOAD)
		return SameMemAndSize(op, ins->mSrc[0]);
	else if (ins->mCode == IC_STORE)
		return SameMemAndSize(op, ins->mSrc[1]);
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


bool InterCodeBasicBlock::CollidingMem(const InterOperand& op1, InterType type1, const InterOperand& op2, InterType type2) const
{
	if (op1.mMemory != op2.mMemory)
	{
		if (op1.mMemory == IM_INDIRECT)
		{
			if (op1.mRestricted)
				return false;
			else if (op2.mMemory == IM_GLOBAL)
			{
				if (op1.mMemoryBase == IM_GLOBAL)
					return op1.mVarIndex == op2.mVarIndex && (
						mProc->mModule->mGlobalVars[op2.mVarIndex]->mSize == InterTypeSize[type1] ||
						mProc->mModule->mGlobalVars[op2.mVarIndex]->mSize == InterTypeSize[type2] ||
						CollidingMemType(type1, type2));
				else
					return mProc->mModule->mGlobalVars[op2.mVarIndex]->mAliased;
			}
			else if (op2.mMemory == IM_FPARAM || op2.mMemory == IM_FFRAME)
				return false;
			else if (op2.mMemory == IM_LOCAL)
			{
				if (op1.mMemoryBase == IM_LOCAL)
					return op1.mVarIndex == op2.mVarIndex && (
						mProc->mLocalVars[op2.mVarIndex]->mSize == InterTypeSize[type1] || 
						mProc->mLocalVars[op2.mVarIndex]->mSize == InterTypeSize[type2] ||
						CollidingMemType(type1, type2));
				else
					return mProc->mLocalVars[op2.mVarIndex]->mAliased && CollidingMemType(type1, type2);
			}
			else if (op2.mMemory == IM_INDIRECT && (op1.mMemoryBase != IM_NONE && op2.mMemoryBase != IM_NONE && op1.mMemoryBase != IM_INDIRECT && op2.mMemoryBase != IM_INDIRECT))
			{
				if (op1.mMemoryBase == op2.mMemoryBase)
				{
					if (op1.mMemoryBase == IM_LOCAL || op1.mMemoryBase == IM_GLOBAL)
						return op1.mVarIndex == op2.mVarIndex && CollidingMemType(type1, type2);
					else
						return CollidingMemType(type1, type2);
				}
				else
					return false;
			}
			else
				return CollidingMemType(type1, type2);
		}
		else if (op2.mMemory == IM_INDIRECT)
		{
			if (op2.mRestricted)
				return false;
			else if (op1.mMemory == IM_GLOBAL)
			{
				if (op2.mMemoryBase == IM_GLOBAL)
					return op1.mVarIndex == op2.mVarIndex && (
						mProc->mModule->mGlobalVars[op2.mVarIndex]->mSize == InterTypeSize[type1] ||
						mProc->mModule->mGlobalVars[op2.mVarIndex]->mSize == InterTypeSize[type2] ||
						CollidingMemType(type1, type2));
				else
					return mProc->mModule->mGlobalVars[op1.mVarIndex]->mAliased;
			}
			else if (op1.mMemory == IM_FPARAM || op1.mMemory == IM_FFRAME)
				return false;
			else if (op1.mMemory == IM_LOCAL)
			{
				if (op2.mMemoryBase == IM_LOCAL)
					return op1.mVarIndex == op2.mVarIndex && (
						mProc->mLocalVars[op1.mVarIndex]->mSize == InterTypeSize[type1] ||
						mProc->mLocalVars[op1.mVarIndex]->mSize == InterTypeSize[type2] ||
						CollidingMemType(type1, type2));
				else
					return mProc->mLocalVars[op1.mVarIndex]->mAliased && CollidingMemType(type1, type2);
			}
			else
				return CollidingMemType(type1, type2);
		}
		else
			return false;
	}

	switch (op1.mMemory)
	{
	case IM_LOCAL:
	case IM_PARAM:
	case IM_FRAME:
		return op1.mVarIndex == op2.mVarIndex && op1.mIntConst < op2.mIntConst + op2.mOperandSize && op2.mIntConst < op1.mIntConst + op1.mOperandSize;
	case IM_FPARAM:
	case IM_FFRAME:
		return op1.mVarIndex + op1.mIntConst < op2.mVarIndex + op2.mIntConst + op2.mOperandSize && op2.mVarIndex + op2.mIntConst < op1.mVarIndex + op1.mIntConst + op1.mOperandSize;
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
		else if (op1.mRestricted && op2.mRestricted && op1.mRestricted != op2.mRestricted)
			return false;
		else
			return CollidingMemType(type1, type2);
	default:
		return false;
	}
}

bool InterCodeBasicBlock::CollidingMem(const InterOperand& op, InterType type, const InterInstruction* ins) const
{
	if (ins->mCode == IC_LOAD)
		return CollidingMem(op, type, ins->mSrc[0], ins->mDst.mType);
	else if (ins->mCode == IC_STORE)
		return CollidingMem(op, type, ins->mSrc[1], ins->mSrc[0].mType);
	else if (ins->mCode == IC_FILL)
		return CollidingMem(op, type, ins->mSrc[1], IT_NONE);
	else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
		return CollidingMem(op, type, ins->mSrc[0], IT_NONE) || CollidingMem(op, type, ins->mSrc[1], IT_NONE);
	else
		return false;
}

bool InterCodeBasicBlock::CollidingMem(const InterInstruction* ins1, const InterInstruction* ins2) const
{
	if (ins1->mCode == IC_LOAD)
		return CollidingMem(ins1->mSrc[0], ins1->mDst.mType, ins2);
	else if (ins1->mCode == IC_STORE)
		return CollidingMem(ins1->mSrc[1], ins1->mSrc[0].mType, ins2);
	else if (ins1->mCode == IC_FILL)
		return CollidingMem(ins1->mSrc[1], IT_NONE, ins2);
	else if (ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY)
		return CollidingMem(ins1->mSrc[0], IT_NONE, ins2) || CollidingMem(ins1->mSrc[1], IT_NONE, ins2);
	else
		return false;
}


bool InterCodeBasicBlock::AliasingMem(const InterInstruction* ins1, const InterInstruction* ins2) const
{
	if (ins1->mCode == IC_LOAD)
		return AliasingMem(ins1->mSrc[0], ins1->mDst.mType, ins2);
	else if (ins1->mCode == IC_STORE)
		return AliasingMem(ins1->mSrc[1], ins1->mSrc[0].mType, ins2);
	else if (ins1->mCode == IC_FILL)
		return AliasingMem(ins1->mSrc[1], IT_NONE, ins2);
	else if (ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY)
		return AliasingMem(ins1->mSrc[0], IT_NONE, ins2) || AliasingMem(ins1->mSrc[1], IT_NONE, ins2);
	else
		return false;
}

bool InterCodeBasicBlock::AliasingMem(const InterOperand& op, InterType type, const InterInstruction* ins) const
{
	if (ins->mCode == IC_LOAD)
		return AliasingMem(op, type, ins->mSrc[0], ins->mDst.mType);
	else if (ins->mCode == IC_STORE)
		return AliasingMem(op, type, ins->mSrc[1], ins->mSrc[0].mType);
	else if (ins->mCode == IC_FILL)
		return AliasingMem(op, type, ins->mSrc[1], IT_NONE);
	else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
		return AliasingMem(op, type, ins->mSrc[0], IT_NONE) || AliasingMem(op, type, ins->mSrc[1], IT_NONE);
	else
		return false;
}

bool InterCodeBasicBlock::AliasingMem(const InterOperand& op1, InterType type1, const InterOperand& op2, InterType type2) const
{
	if (type1 != IT_NONE && type1 == type2 && SameMem(op1, op2))
		return false;
	else
		return CollidingMem(op1, type1, op2, type2);
}

bool InterCodeBasicBlock::AliasingMem(InterCodeBasicBlock* block, InterInstruction* lins, int from, int to) const
{
	if (to > block->mInstructions.Size())
		to = block->mInstructions.Size();
	for (int i = from; i < to; i++)
		if (AliasingMem(block->mInstructions[i], lins))
			return true;
	return false;
}


bool InterCodeBasicBlock::DestroyingMem(const InterInstruction* lins, const InterInstruction* sins) const
{
	if (sins->mCode == IC_LOAD)
		return false;
	else if (sins->mCode == IC_STORE)
		return CollidingMem(sins->mSrc[1], sins->mSrc[0].mType, lins);
	else if (sins->mCode == IC_FILL)
		return CollidingMem(sins->mSrc[1], IT_NONE, lins);
	else if (sins->mCode == IC_COPY || sins->mCode == IC_STRCPY)
		return CollidingMem(sins->mSrc[1], IT_NONE, lins);
	else if (sins->mCode == IC_FREE)
	{
		int	opmask = 0;
		if (lins->mCode == IC_LOAD)
			opmask = 1;
		else if (lins->mCode == IC_STORE)
			opmask = 2;
		else if (lins->mCode == IC_FILL)
			opmask = 2;
		else if (lins->mCode == IC_COPY)
			opmask = 3;

		for (int k = 0; k < lins->mNumOperands; k++)
		{
			if ((1 << k) & opmask)
			{
				const InterOperand& op(lins->mSrc[k]);
				if (op.mMemory == IM_INDIRECT)
					return true;
			}
		}

		return false;
	}
	else if (sins->mCode == IC_CALL || sins->mCode == IC_CALL_NATIVE)
	{
		if (sins->mSrc[0].mTemp < 0 && sins->mSrc[0].mLinkerObject)
		{
			InterCodeProcedure* proc = sins->mSrc[0].mLinkerObject->mProc;
			if (proc)
			{
				int	opmask = 0;
				if (lins->mCode == IC_LOAD)
					opmask = 1;
				else if (lins->mCode == IC_STORE)
					opmask = 2;
				else if (lins->mCode == IC_FILL)
					opmask = 2;
				else if (lins->mCode == IC_COPY)
					opmask = 3;

				for (int k = 0; k < lins->mNumOperands; k++)
				{
					if ((1 << k) & opmask)
					{
						const InterOperand& op(lins->mSrc[k]);

						if (op.mTemp >= 0)
						{
							if (proc->mStoresIndirect)
								return true;
						}
						else if (op.mMemory == IM_FFRAME || op.mMemory == IM_FRAME)
							return true;
						else if (op.mMemory == IM_GLOBAL)
						{
							if (proc->ModifiesGlobal(op.mVarIndex))
								return true;
						}
						else if (op.mMemory == IM_LOCAL && !mProc->mLocalVars[op.mVarIndex]->mAliased)
							;
						else if ((op.mMemory == IM_PARAM || op.mMemory == IM_FPARAM) && !mProc->mParamVars[op.mVarIndex]->mAliased)
							;
						else
							return true;
					}
				}

				return false;
			}
		}

		return true;
	}
	else
		return false;
}

bool InterCodeBasicBlock::DestroyingMem(InterCodeBasicBlock* block, InterInstruction* lins, int from, int to) const
{
	for (int i = from; i < to; i++)
	{
		InterInstruction* ins = block->mInstructions[i];
		if (DestroyingMem(lins, ins))
			return true;
	}

	return false;
}

bool InterCodeBasicBlock::CanSwapInstructions(const InterInstruction* ins0, const InterInstruction* ins1) const
{
	// Cannot swap branches
	if (ins1->mCode == IC_JUMP || ins1->mCode == IC_BRANCH || ins1->mCode == IC_DISPATCH || ins1->mCode == IC_RETURN || ins1->mCode == IC_RETURN_STRUCT)
		return false;

	// Check function call
	if (ins1->mCode == IC_CALL || ins1->mCode == IC_CALL_NATIVE || ins1->mCode == IC_ASSEMBLER)
	{
		if (ins0->mCode == IC_CALL || ins0->mCode == IC_CALL_NATIVE || ins0->mCode == IC_ASSEMBLER ||
			ins0->mCode == IC_RETURN || ins0->mCode == IC_RETURN_STRUCT || ins0->mCode == IC_RETURN_VALUE ||
			ins0->mCode == IC_PUSH_FRAME || ins0->mCode == IC_POP_FRAME || ins0->mCode == IC_MALLOC || ins0->mCode == IC_FREE || ins0->mCode == IC_BREAKPOINT)
			return false;

		if (ins0->mCode == IC_LOAD)
		{
			if (ins0->mSrc[0].mTemp >= 0)
				return false;
			if (ins0->mSrc[0].mMemory == IM_PARAM || ins0->mSrc[0].mMemory == IM_FPARAM)
			{
				if (mProc->mParamAliasedSet[ins0->mSrc[0].mVarIndex])
					return false;
			}
			else if (!ins1->mNoSideEffects)
				return false;
		}
		else if (ins0->mCode == IC_STORE || ins0->mCode == IC_COPY || ins0->mCode == IC_STRCPY || ins0->mCode == IC_FILL)
		{
			if (!ins1->mNoSideEffects || !ins1->mConstExpr)
				return false;
		}
	}
	if (ins0->mCode == IC_CALL || ins0->mCode == IC_CALL_NATIVE || ins0->mCode == IC_ASSEMBLER)
	{
		if (ins1->mCode == IC_RETURN || ins1->mCode == IC_RETURN_STRUCT || ins1->mCode == IC_RETURN_VALUE ||
			ins1->mCode == IC_PUSH_FRAME || ins1->mCode == IC_POP_FRAME || ins1->mCode == IC_MALLOC || ins1->mCode == IC_FREE || ins1->mCode == IC_BREAKPOINT)
			return false;

		if (ins0->mSrc[0].mMemory == IM_PROCEDURE && ins0->mSrc[0].mLinkerObject && ins0->mSrc[0].mLinkerObject->mProc && 
			ins0->mSrc[0].mLinkerObject->mProc->mLeafProcedure &&
			ins0->mSrc[0].mLinkerObject->mProc->mParamVars.Size() == 0)
		{
			if (ins1->mCode == IC_STORE)
			{
				if (ins1->mSrc[1].mMemory != IM_FRAME && ins1->mSrc[1].mMemory != IM_FFRAME)
					return false;
			}
			else if (ins1->mCode == IC_LOAD || ins1->mCode == IC_STORE || ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY || ins1->mCode == IC_FILL)
				return false;
		}
		else if (ins1->mCode == IC_LOAD || ins1->mCode == IC_STORE || ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY || ins1->mCode == IC_FILL)
			return false;
	}

	if (ins0->mCode == IC_BREAKPOINT && ins1->mCode == IC_STORE)
		return false;
	if (ins1->mCode == IC_BREAKPOINT && ins0->mCode == IC_STORE)
		return false;

	if (ins0->mCode == IC_MALLOC || ins0->mCode == IC_FREE)
	{
		if (ins1->mCode == IC_MALLOC || ins1->mCode == IC_FREE)
			return false;
	}

	if (ins0->mCode == IC_FREE)
	{
		if (ins1->mCode == IC_LOAD || ins1->mCode == IC_STORE || ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY || ins1->mCode == IC_FILL)
			return false;
	}
	if (ins1->mCode == IC_FREE)
	{
		if (ins0->mCode == IC_LOAD || ins0->mCode == IC_STORE || ins0->mCode == IC_COPY || ins0->mCode == IC_STRCPY || ins0->mCode == IC_FILL)
			return false;
	}

	// Check frame pointer
	if (ins0->mCode == IC_PUSH_FRAME || ins0->mCode == IC_POP_FRAME)
	{
		if (ins1->mCode == IC_PUSH_FRAME || ins1->mCode == IC_POP_FRAME)
			return false;
		if (ins1->mCode == IC_CONSTANT && ins1->mDst.mType == IT_POINTER && ins1->mConst.mMemory == IM_FRAME)
			return false;
		if (ins1->mCode == IC_STORE && ins1->mSrc[1].mMemory == IM_FRAME)
			return false;
	}
	if (ins1->mCode == IC_PUSH_FRAME || ins1->mCode == IC_POP_FRAME)
	{
		if (ins0->mCode == IC_CONSTANT && ins0->mDst.mType == IT_POINTER && ins0->mConst.mMemory == IM_FRAME)
			return false;
		if (ins0->mCode == IC_STORE && ins0->mSrc[1].mMemory == IM_FRAME)
			return false;
	}

	// False data dependency
	if (ins1->mDst.mTemp >= 0)
	{
		if (ins1->mDst.mTemp == ins0->mDst.mTemp)
			return false;

		for (int i = 0; i < ins0->mNumOperands; i++)
			if (ins1->mDst.mTemp == ins0->mSrc[i].mTemp)
				return false;
	}

	// True data dependency
	if (ins0->mDst.mTemp >= 0)
	{
		for (int i = 0; i < ins1->mNumOperands; i++)
			if (ins0->mDst.mTemp == ins1->mSrc[i].mTemp)
				return false;
	}

	if ((ins0->mCode == IC_LOAD || ins0->mCode == IC_STORE || ins0->mCode == IC_COPY || ins0->mCode == IC_STRCPY || ins0->mCode == IC_FILL) &&
		(ins1->mCode == IC_LOAD || ins1->mCode == IC_STORE || ins1->mCode == IC_COPY || ins1->mCode == IC_STRCPY || ins1->mCode == IC_FILL))
	{
		if (ins0->mMemmap || ins1->mMemmap)
			return false;

		if (ins0->mVolatile && ins1->mVolatile)
			return false;

		if (ins1->mVolatile && ins0->mCode == IC_LOAD)
		{
			if (ins1->mCode == IC_LOAD)
				;
			else if (ins1->mCode == IC_STORE)
			{
				if (ins1->mSrc[1].mMemory == IM_ABSOLUTE && (ins0->mSrc[0].mMemory != IM_ABSOLUTE && ins0->mSrc[0].mMemory != IM_INDIRECT))
					;
				else
					return false;
			}
			else
				return false;
		}

		if (ins0->mCode == IC_LOAD)
		{
			if (DestroyingMem(ins0, ins1))
				return false;
		}
		else if (ins1->mCode == IC_LOAD)
		{
			if (DestroyingMem(ins1, ins0))
				return false;
		}
		else if (ins0->mCode == IC_STORE || ins0->mCode == IC_COPY || ins0->mCode == IC_STRCPY || ins0->mCode == IC_FILL)
		{
			if (CollidingMem(ins0, ins1))
				return false;
		}
	}

	return true;
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
		if (type == IT_INT32 && val1 >= 0 && val2 >= 0)
			return val1 * val2 & 0xffffffff;
		else
			return val1 * val2;

	case IA_DIVU:
		if (val2)
		{
			if (type == IT_INT32)
				return (uint32)val1 / (uint32)val2;
			else if (type == IT_INT16)
				return (uint16)val1 / (uint16)val2;
			else
				return (uint8)val1 / (uint8)val2;
		}
		else
			return 0;
		break;
	case IA_DIVS:
		if (val2)
			return val1 / val2;
		else
			return 0;
		break;
	case IA_MODU:
		if (val2)
		{
			if (type == IT_INT32)
				return (uint32)val1 % (uint32)val2;
			else if (type == IT_INT16)
				return (uint16)val1 % (uint16)val2;
			else
				return (uint8)val1 % (uint8)val2;
		}
		else
			return 0;
		break;
	case IA_MODS:
		if (val2)
			return val1 % val2;
		else
			return 0;
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
		switch (type)
		{
		case IT_INT8:
			return uint8(~val1);
		case IT_INT16:
			return uint16(~val1);
		case IT_INT32:
			return uint32(~val1);
		default:
			return ~val1;
		}
		break;
	case IA_SHL:
	{
		int64 shift = TypeShiftMask(type, val2);
		if (val1 < 0 && shift < 16 && val1 << shift >= SignedTypeMin(type))
			return ToTypedSigned(val1 << shift, type);
		else
			return ToTypedUnsigned(val1 << shift, type);
	}
	case IA_SHR:
		return ToTypedUnsigned(val1, type) >> TypeShiftMask(type, val2);
	case IA_SAR:
		return ToTypedSigned(val1, type) >> TypeShiftMask(type, val2);
	case IA_CMPEQ:
		return ToTypedUnsigned(val1, type) == ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPNE:
		return ToTypedUnsigned(val1, type) != ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPGES:
		return ToTypedSigned(val1, type) >= ToTypedSigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPLES:
		return ToTypedSigned(val1, type) <= ToTypedSigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPGS:
		return ToTypedSigned(val1, type) > ToTypedSigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPLS:
		return ToTypedSigned(val1, type) < ToTypedSigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPGEU:
		return ToTypedUnsigned(val1, type) >= ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPLEU:
		return ToTypedUnsigned(val1, type) <= ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPGU:
		return ToTypedUnsigned(val1, type) > ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	case IA_CMPLU:
		return ToTypedUnsigned(val1, type) < ToTypedUnsigned(val2, type) ? 1 : 0;
		break;
	default:
		return 0;
	}
}

static int64 ConstantRelationalPointerFolding(InterOperator oper, const InterOperand& op1, const InterOperand& op2)
{
	if (op1.mMemory == op2.mMemory)
	{
		if (op1.mMemory == IM_ABSOLUTE)
			return ConstantFolding(oper, IT_INT16, op1.mIntConst, op2.mIntConst);
		else if (op1.mMemory != IM_INDIRECT && op1.mVarIndex == op2.mVarIndex)
			return ConstantFolding(oper, IT_INT16, op1.mIntConst, op2.mIntConst);
	}

	if (oper == IA_CMPNE)
		return 1;
	else
		return 0;
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

InterOperator InvertRelational(InterOperator oper)
{
	switch (oper)
	{
	case IA_CMPGES:
		return IA_CMPLS;
	case IA_CMPLES:
		return IA_CMPGS;
	case IA_CMPGS:
		return IA_CMPLES;
	case IA_CMPLS:
		return IA_CMPGES;
	case IA_CMPGEU:
		return IA_CMPLU;
	case IA_CMPLEU:
		return IA_CMPGU;
	case IA_CMPGU:
		return IA_CMPLEU;
	case IA_CMPLU:
		return IA_CMPGEU;
	case IA_CMPEQ:
		return IA_CMPNE;
	case IA_CMPNE:
		return IA_CMPEQ;
	default:
		return oper;
	}

}

static bool IsStrictUnsignedRelational(InterOperator oper)
{
	return
		oper == IA_CMPLEU ||
		oper == IA_CMPGEU ||
		oper == IA_CMPLU ||
		oper == IA_CMPGU;
}

static bool IsSignedRelational(InterOperator oper)
{
	return
		oper == IA_CMPEQ ||
		oper == IA_CMPNE || 
		oper == IA_CMPLES ||
		oper == IA_CMPGES ||
		oper == IA_CMPLS ||
		oper == IA_CMPGS;
}

InterOperator MirrorRelational(InterOperator oper)
{
	switch (oper)
	{
	case IA_CMPGES:
		return IA_CMPLES;
	case IA_CMPLES:
		return IA_CMPGES;
	case IA_CMPGS:
		return IA_CMPLS;
	case IA_CMPLS:
		return IA_CMPGS;
	case IA_CMPGEU:
		return IA_CMPLEU;
	case IA_CMPLEU:
		return IA_CMPGEU;
	case IA_CMPGU:
		return IA_CMPLU;
	case IA_CMPLU:
		return IA_CMPGU;
	default:
		return oper;
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
	case IA_UINT2FLOAT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mFloatConst = (double)((uint16)cop.mIntConst);
		ins->mConst.mType = IT_FLOAT;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_FLOAT2UINT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int)(cop.mFloatConst);
		ins->mConst.mType = IT_INT16;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_LINT2FLOAT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mFloatConst = (double)(cop.mIntConst);
		ins->mConst.mType = IT_FLOAT;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_FLOAT2LINT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int)(cop.mFloatConst);
		ins->mConst.mType = IT_INT32;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_LUINT2FLOAT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mFloatConst = (double)((uint32)cop.mIntConst);
		ins->mConst.mType = IT_FLOAT;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_FLOAT2LUINT:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (uint32)(cop.mFloatConst);
		ins->mConst.mType = IT_INT32;
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
	case IA_EXT8TO32S:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (int8)(cop.mIntConst);
		ins->mConst.mType = IT_INT32;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	case IA_EXT8TO32U:
		ins->mCode = IC_CONSTANT;
		ins->mConst.mIntConst = (uint8)(cop.mIntConst);
		ins->mConst.mType = IT_INT32;
		ins->mSrc[0].mTemp = -1;
		ins->mNumOperands = 0;
		break;
	}
}

static InterOperand OperandConstantFolding(InterOperator oper, InterOperand op1, InterOperand op2)
{
	InterOperand	dop;

	switch (oper)
	{
	case IA_INT2FLOAT:
		dop.mFloatConst = (double)(op1.mIntConst);
		dop.mType = IT_FLOAT;
		break;
	case IA_FLOAT2INT:
		dop.mIntConst = (int)(op1.mFloatConst);
		dop.mType = IT_INT16;
		break;
	case IA_UINT2FLOAT:
		dop.mFloatConst = (double)((uint16)op1.mIntConst);
		dop.mType = IT_FLOAT;
		break;
	case IA_FLOAT2UINT:
		dop.mIntConst = (int)(op1.mFloatConst);
		dop.mType = IT_INT16;
		break;
	case IA_LINT2FLOAT:
		dop.mFloatConst = (double)(op1.mIntConst);
		dop.mType = IT_FLOAT;
		break;
	case IA_FLOAT2LINT:
		dop.mIntConst = (int)(op1.mFloatConst);
		dop.mType = IT_INT16;
		break;
	case IA_LUINT2FLOAT:
		dop.mFloatConst = (double)((uint32)op1.mIntConst);
		dop.mType = IT_FLOAT;
		break;
	case IA_FLOAT2LUINT:
		dop.mIntConst = (uint32)(op1.mFloatConst);
		dop.mType = IT_INT16;
		break;
	case IA_EXT8TO16S:
		dop.mIntConst = (int8)(op1.mIntConst);
		dop.mType = IT_INT16;
		break;
	case IA_EXT8TO16U:
		dop.mIntConst = (uint8)(op1.mIntConst);
		dop.mType = IT_INT16;
		break;
	case IA_EXT16TO32S:
		dop.mIntConst = (int16)(op1.mIntConst);
		dop.mType = IT_INT32;
		break;
	case IA_EXT16TO32U:
		dop.mIntConst = (uint16)(op1.mIntConst);
		dop.mType = IT_INT32;
		break;
	case IA_EXT8TO32S:
		dop.mIntConst = (int8)(op1.mIntConst);
		dop.mType = IT_INT32;
		break;
	case IA_EXT8TO32U:
		dop.mIntConst = (uint8)(op1.mIntConst);
		dop.mType = IT_INT32;
		break;
	case IA_CMPEQ:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst == op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mIntConst == op2.mIntConst && op1.mLinkerObject == op2.mLinkerObject;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) == ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPNE:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst != op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory != op2.mMemory || op1.mVarIndex != op2.mVarIndex || op1.mIntConst != op2.mIntConst || op1.mLinkerObject != op2.mLinkerObject;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) != ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPGES:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst >= op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst >= op2.mIntConst;
		else
			dop.mIntConst = ToTypedSigned(op1.mIntConst, op1.mType) >= ToTypedSigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPGEU:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst >= op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst >= op2.mIntConst;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) >= ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPLES:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst <= op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst <= op2.mIntConst;
		else
			dop.mIntConst = ToTypedSigned(op1.mIntConst, op1.mType) <= ToTypedSigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPLEU:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst <= op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst <= op2.mIntConst;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) <= ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPGS:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst > op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst > op2.mIntConst;
		else
			dop.mIntConst = ToTypedSigned(op1.mIntConst, op1.mType) > ToTypedSigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPGU:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst > op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst > op2.mIntConst;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) > ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPLS:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst < op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst < op2.mIntConst;
		else
			dop.mIntConst = ToTypedSigned(op1.mIntConst, op1.mType) < ToTypedSigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;
	case IA_CMPLU:
		if (op1.mType == IT_FLOAT)
			dop.mIntConst = op1.mFloatConst < op2.mFloatConst ? 1 : 0;
		else if (op1.mType == IT_POINTER)
			dop.mIntConst = op1.mMemory == op2.mMemory && op1.mVarIndex == op2.mVarIndex && op1.mLinkerObject == op2.mLinkerObject && op1.mIntConst < op2.mIntConst;
		else
			dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) < ToTypedUnsigned(op2.mIntConst, op2.mType) ? 1 : 0;
		dop.mType = IT_BOOL;
		break;

	case IA_ADD:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = op1.mFloatConst + op2.mFloatConst;
		else
			dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst + op2.mIntConst);
		break;
	case IA_SUB:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = op1.mFloatConst - op2.mFloatConst;
		else
			dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst - op2.mIntConst);
		break;
	case IA_MUL:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = op1.mFloatConst * op2.mFloatConst;
		else
			dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst * op2.mIntConst);
		break;
	case IA_DIVU:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = op1.mFloatConst / op2.mFloatConst;
		else
			dop.mIntConst = LimitIntConstValue(dop.mType, ToTypedUnsigned(op1.mIntConst, op1.mType) / ToTypedUnsigned(op2.mIntConst, op1.mType));
		break;
	case IA_DIVS:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = op1.mFloatConst / op2.mFloatConst;
		else
			dop.mIntConst = LimitIntConstValue(dop.mType, ToTypedSigned(op1.mIntConst, op1.mType) / ToTypedSigned(op2.mIntConst, op1.mType));
		break;
	case IA_MODU:
		dop.mType = op1.mType;
		dop.mIntConst = LimitIntConstValue(dop.mType, ToTypedUnsigned(op1.mIntConst, op1.mType) % ToTypedUnsigned(op2.mIntConst, op1.mType));
		break;
	case IA_MODS:
		dop.mType = op1.mType;
		dop.mIntConst = LimitIntConstValue(dop.mType, ToTypedSigned(op1.mIntConst, op1.mType) % ToTypedSigned(op2.mIntConst, op1.mType));
		break;
	case IA_OR:
		dop.mType = op1.mType;
		dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst | op2.mIntConst);
		break;
	case IA_AND:
		dop.mType = op1.mType;
		dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst & op2.mIntConst);
		break;
	case IA_XOR:
		dop.mType = op1.mType;
		dop.mIntConst = LimitIntConstValue(dop.mType, op1.mIntConst ^ op2.mIntConst);
		break;
	case IA_NEG:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = -op1.mFloatConst;
		else
			dop.mIntConst = -op1.mIntConst;
		break;
	case IA_ABS:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
		{
			if (op1.mFloatConst < 0)
				dop.mFloatConst = -op1.mFloatConst;
			else
				dop.mFloatConst = op1.mFloatConst;
		}
		else
		{
			if (op1.mIntConst < 0)
				dop.mIntConst = -op1.mIntConst;
			else
				dop.mIntConst = op1.mIntConst;
		}
		break;
		break;
	case IA_FLOOR:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = floor(op1.mFloatConst);
		else
			dop.mIntConst = op1.mIntConst;
		break;
	case IA_CEIL:
		dop.mType = op1.mType;
		if (op1.mType == IT_FLOAT)
			dop.mFloatConst = ceil(op1.mFloatConst);
		else
			dop.mIntConst = op1.mIntConst;
		break;
	case IA_NOT:
		dop.mType = op1.mType;
		switch (op1.mType)
		{
		case IT_INT8:
			dop.mIntConst = uint8(~op1.mIntConst);
		case IT_INT16:
			dop.mIntConst = uint16(~op1.mIntConst);
		case IT_INT32:
			dop.mIntConst = uint32(~op1.mIntConst);
		default:
			dop.mIntConst = ~op1.mIntConst;
		}
		break;
	case IA_SHL:
		dop.mType = op1.mType;
		dop.mIntConst = ToTypedUnsigned(op1.mIntConst << TypeShiftMask(op1.mType, op2.mIntConst), op1.mType);
		break;
	case IA_SHR:
		dop.mType = op1.mType;
		dop.mIntConst = ToTypedUnsigned(op1.mIntConst, op1.mType) >> TypeShiftMask(op1.mType, op2.mIntConst);
		break;
	case IA_SAR:
		dop.mType = op1.mType;
		dop.mIntConst = ToTypedSigned(op1.mIntConst, op1.mType) >> TypeShiftMask(op1.mType, op2.mIntConst);
		break;
	}

	assert(dop.mType != IT_NONE);

	return dop;
}

InterOperand InterCodeBasicBlock::LoadConstantOperand(const InterInstruction* ins, const InterOperand& op, InterType type, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs)
{
	InterOperand cop;

	const uint8* data;

	LinkerObject* lobj;
	int					offset, stride;

	lobj = op.mLinkerObject;
	offset = int(op.mIntConst);
	stride = op.mStride;


	if (offset >= 0 && offset + stride * (InterTypeSize[type] - 1) < lobj->mSize)
	{
		data = lobj->mData + offset;

		switch (type)
		{
		case IT_BOOL:
			cop.mIntConst = data[0] ? 1 : 0;
		case IT_INT8:
			cop.mIntConst = data[0];
			break;
		case IT_INT16:
			cop.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8);
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
				if (j < staticVars.Size())
				{
					cop.mMemory = IM_GLOBAL;
					cop.mVarIndex = staticVars[j]->mIndex;
				}
				else
				{
					j = 0;
					while (j < staticProcs.Size() && !(staticProcs[j] && staticProcs[j]->mLinkerObject == lobj->mReferences[i]->mRefObject))
						j++;

					if (j < staticProcs.Size())
					{
						cop.mMemory = IM_PROCEDURE;
						cop.mVarIndex = staticProcs[j]->mID;
					}
					else
					{
						cop.mMemory = IM_GLOBAL;
						cop.mVarIndex = -1;
					}
				}

				cop.mLinkerObject = lobj->mReferences[i]->mRefObject;
				cop.mIntConst = lobj->mReferences[i]->mRefOffset;
				cop.mOperandSize = ins->mConst.mLinkerObject->mSize;
			}
			else
			{
				cop.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8);
				cop.mMemory = IM_ABSOLUTE;
			}

		} break;
		case IT_INT32:
			cop.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8) | ((int)data[2 * stride] << 16) | ((int)data[3 * stride] << 24);
			break;
		case IT_FLOAT:
		{
			union { float f; unsigned int v; } cc;
			cc.v = (int)data[0 * stride] | (data[1 * stride] << 8) | (data[2 * stride] << 16) | (data[3 * stride] << 24);
			cop.mFloatConst = cc.f;
		} break;
		}
	}
	else
	{
		cop.mIntConst = 0;
		cop.mFloatConst = 0;
		cop.mMemory = IM_ABSOLUTE;
		cop.mLinkerObject = nullptr;

		mProc->mModule->mErrors->Error(ins->mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Constant index out of bounds");
	}

	cop.mType = type;
	return cop;
}

void InterCodeBasicBlock::LoadConstantFold(InterInstruction* ins, InterInstruction* ains, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray&staticProcs)
{
	const uint8* data;

	LinkerObject	*	lobj;
	int					offset, stride;

	if (ains)
	{
		lobj = ains->mConst.mLinkerObject;
		offset = int(ains->mConst.mIntConst);
		stride = ains->mConst.mStride;
	}
	else
	{
		lobj = ins->mSrc[0].mLinkerObject;
		offset = int(ins->mSrc[0].mIntConst);
		stride = ins->mSrc[0].mStride;
	}

	
	if (offset >= 0 && offset + stride * (InterTypeSize[ins->mDst.mType] - 1) < lobj->mSize)
	{
		data = lobj->mData + offset;

		switch (ins->mDst.mType)
		{
		case IT_BOOL:
			ins->mConst.mIntConst = data[0] ? 1 : 0;
		case IT_INT8:
			ins->mConst.mIntConst = data[0];
			break;
		case IT_INT16:
			ins->mConst.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8);
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
				if (j < staticVars.Size())
				{
					ins->mConst.mMemory = IM_GLOBAL;
					ins->mConst.mVarIndex = staticVars[j]->mIndex;
				}
				else
				{
					j = 0;
					while (j < staticProcs.Size() && !(staticProcs[j] && staticProcs[j]->mLinkerObject == lobj->mReferences[i]->mRefObject))
						j++;

					if (j < staticProcs.Size())
					{
						ins->mConst.mMemory = IM_PROCEDURE;
						ins->mConst.mVarIndex = staticProcs[j]->mID;
					}
					else
					{
						ins->mConst.mMemory = IM_GLOBAL;
						ins->mConst.mVarIndex = -1;
					}
				}

				ins->mConst.mLinkerObject = lobj->mReferences[i]->mRefObject;
				ins->mConst.mIntConst = lobj->mReferences[i]->mRefOffset;
				ins->mConst.mOperandSize = ins->mConst.mLinkerObject->mSize;
			}
			else
			{
				ins->mConst.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8);
				ins->mConst.mMemory = IM_ABSOLUTE;
			}

		} break;
		case IT_INT32:
			ins->mConst.mIntConst = (int)data[0 * stride] | ((int)data[1 * stride] << 8) | ((int)data[2 * stride] << 16) | ((int)data[3 * stride] << 24);
			break;
		case IT_FLOAT:
		{
			union { float f; unsigned int v; } cc;
			cc.v = (int)data[0 * stride] | (data[1 * stride] << 8) | (data[2 * stride] << 16) | (data[3 * stride] << 24);
			ins->mConst.mFloatConst = cc.f;
		} break;
		}
	}
	else
	{
		ins->mConst.mIntConst = 0;
		ins->mConst.mFloatConst = 0;
		ins->mConst.mMemory = IM_ABSOLUTE;
		ins->mConst.mLinkerObject = nullptr;

		mProc->mModule->mErrors->Error(ins->mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Constant index out of bounds");
	}

	ins->mCode = IC_CONSTANT;
	ins->mConst.mType = ins->mDst.mType;
	ins->mDst.mMemory = ins->mConst.mMemory;
	ins->mSrc[0].mTemp = -1;
	ins->mNumOperands = 0;
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
	return code == IC_CALL || code == IC_CALL_NATIVE || code == IC_ASSEMBLER || code == IC_DISPATCH || code == IC_BREAKPOINT;/* || code == IC_MALLOC || code == IC_FREE */;
}

static bool IsObservable(InterCode code)
{
	return code == IC_CALL || code == IC_CALL_NATIVE || code == IC_ASSEMBLER || code == IC_DISPATCH || code == IC_STORE || code == IC_COPY || code == IC_STRCPY || code == IC_FILL || code == IC_MALLOC || code == IC_FREE || code == IC_BREAKPOINT;
}

static bool IsMoveable(InterCode code)
{
	if (HasSideEffect(code) || code == IC_COPY || code == IC_STRCPY || code == IC_STORE || code == IC_FILL || code == IC_BRANCH || code == IC_POP_FRAME || code == IC_PUSH_FRAME || code == IC_MALLOC || code == IC_FREE)
		return false;
	if (code == IC_RETURN || code == IC_RETURN_STRUCT || code == IC_RETURN_VALUE || code == IC_DISPATCH)
		return false;

	return true;
}


static bool CanBypassLoad(const InterInstruction* lins, const InterInstruction* bins)
{
	// Check ambiguity
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY || bins->mCode == IC_FILL)
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

static bool CanBypassStoreDown(const InterInstruction* sins, const InterInstruction* bins)
{
	// Check ambiguity
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY || bins->mCode == IC_FILL)
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;

	// True data dependency
	if (bins->mDst.mTemp >= 0 && sins->UsesTemp(bins->mDst.mTemp))
		return false;

	if (bins->mCode == IC_STORE)
	{
		if (sins->mVolatile || sins->mMemmap)
			return false;
		else if (sins->mSrc[1].mMemory == IM_INDIRECT && bins->mSrc[1].mMemory == IM_INDIRECT)
		{
			return sins->mSrc[1].mLinkerObject && bins->mSrc[1].mLinkerObject && sins->mSrc[1].mLinkerObject != bins->mSrc[1].mLinkerObject;
		}
		else if (sins->mSrc[1].mTemp >= 0 || bins->mSrc[1].mTemp >= 0)
			return false;
		else if (sins->mSrc[1].mMemory != bins->mSrc[1].mMemory)
			return true;
		else if (sins->mSrc[1].mMemory == IM_GLOBAL)
		{
			return sins->mSrc[1].mLinkerObject != bins->mSrc[1].mLinkerObject ||
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[1].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (sins->mSrc[1].mMemory == IM_ABSOLUTE)
		{
			return
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[1].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else if (sins->mSrc[1].mMemory == IM_LOCAL)
		{
			return sins->mSrc[1].mVarIndex != bins->mSrc[1].mVarIndex ||
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[1].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[1].mIntConst + bins->mSrc[1].mOperandSize;
		}
		else
			return false;
	}

	if (bins->mCode == IC_LOAD)
	{
		if (sins->mVolatile || sins->mMemmap)
			return false;
		else if (sins->mSrc[1].mMemory == IM_INDIRECT && bins->mSrc[0].mMemory == IM_INDIRECT)
		{
			return sins->mSrc[1].mLinkerObject && bins->mSrc[0].mLinkerObject && sins->mSrc[1].mLinkerObject != bins->mSrc[0].mLinkerObject;
		}
		else if (sins->mSrc[1].mTemp >= 0 || bins->mSrc[0].mTemp >= 0)
			return false;
		else if (sins->mSrc[1].mMemory != bins->mSrc[0].mMemory)
			return true;
		else if (sins->mSrc[1].mMemory == IM_GLOBAL)
		{
			return sins->mSrc[1].mLinkerObject != bins->mSrc[0].mLinkerObject ||
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[0].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[0].mIntConst + bins->mSrc[0].mOperandSize;
		}
		else if (sins->mSrc[1].mMemory == IM_ABSOLUTE)
		{
			return
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[0].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[0].mIntConst + bins->mSrc[0].mOperandSize;
		}
		else if (sins->mSrc[1].mMemory == IM_LOCAL)
		{
			return sins->mSrc[1].mVarIndex != bins->mSrc[0].mVarIndex ||
				sins->mSrc[1].mIntConst + sins->mSrc[1].mOperandSize <= bins->mSrc[0].mIntConst ||
				sins->mSrc[1].mIntConst >= bins->mSrc[0].mIntConst + bins->mSrc[0].mOperandSize;
		}
		else
			return false;
	}

	return true;
}

static bool CanBypass(const InterInstruction* lins, const InterInstruction* bins)
{
	if (HasSideEffect(lins->mCode) && HasSideEffect(bins->mCode))
		return false;

	if (lins->mCode == IC_CALL || lins->mCode == IC_CALL_NATIVE)
	{
		if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE ||
			bins->mCode == IC_RETURN || bins->mCode == IC_RETURN_STRUCT || bins->mCode == IC_RETURN_VALUE ||
			bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_POP_FRAME)
			return false;
		if (bins->mCode == IC_LOAD || bins->mCode == IC_STORE || bins->mCode == IC_COPY || bins->mCode == IC_FILL)
			return false;
	}

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
	if (lins->mCode == IC_STORE || lins->mCode == IC_COPY || lins->mCode == IC_FILL)
	{
		if (bins->mCode == IC_STORE || bins->mCode == IC_LOAD || bins->mCode == IC_COPY || bins->mCode == IC_FILL || bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE)
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
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY || bins->mCode == IC_FILL)
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
	if (bins->mCode == IC_COPY || bins->mCode == IC_STRCPY || bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_FILL)
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
		so = int(sins->mSrc[0].mIntConst);
		slo = sins->mSrc[0].mLinkerObject;
		sz = InterTypeSize[sins->mDst.mType];
	}
	else if (sins->mCode == IC_LEA || sins->mCode == IC_STORE)
	{
		sm = sins->mSrc[1].mMemory;
		si = sins->mSrc[1].mVarIndex;
		st = sins->mSrc[1].mTemp;
		so = int(sins->mSrc[1].mIntConst);
		slo = sins->mSrc[1].mLinkerObject;
		sz = InterTypeSize[sins->mSrc[0].mType];
	}

	if (bins->mCode == IC_LOAD)
	{
		bm = bins->mSrc[0].mMemory;
		bi = bins->mSrc[0].mVarIndex;
		bt = bins->mSrc[0].mTemp;
		bo = int(bins->mSrc[0].mIntConst);
		blo = bins->mSrc[0].mLinkerObject;
		bz = InterTypeSize[bins->mDst.mType];
	}
	else if (bins->mCode == IC_LEA || bins->mCode == IC_STORE)
	{
		bm = bins->mSrc[1].mMemory;
		bi = bins->mSrc[1].mVarIndex;
		bt = bins->mSrc[1].mTemp;
		bo = int(bins->mSrc[1].mIntConst);
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
			if (bm == IM_FPARAM && bi + bz > si && si + bz > bi)
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

TempForwardingTable::TempForwardingTable(const TempForwardingTable& table) : mAssoc(table.mAssoc)
{
#if 0
	mAssoc.Reserve(table.mAssoc.Size());
	for (int i = 0; i < table.mAssoc.Size(); i++)
	{
		mAssoc[i].mAssoc = table.mAssoc[i].mAssoc;
		mAssoc[i].mSucc = table.mAssoc[i].mSucc;
		mAssoc[i].mPred = table.mAssoc[i].mPred;
	}
#endif
}

TempForwardingTable& TempForwardingTable::operator=(const TempForwardingTable& table)
{
	mAssoc = table.mAssoc;
#if 0
	mAssoc.SetSize(table.mAssoc.Size());
	for (int i = 0; i < table.mAssoc.Size(); i++)
	{
		mAssoc[i].mAssoc = table.mAssoc[i].mAssoc;
		mAssoc[i].mSucc = table.mAssoc[i].mSucc;
		mAssoc[i].mPred = table.mAssoc[i].mPred;
	}
#endif
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

void TempForwardingTable::Shrink(void)
{
	mAssoc.shrink();
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

void InterInstruction::ReplaceTemp(int from, int to)
{
	if (mDst.mTemp == from) mDst.mTemp = to;
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp == from)
			mSrc[i].mTemp = to;
}

InterInstruction* InterInstruction::Clone(void) const
{
	InterInstruction* ins = new InterInstruction(mLocation, mCode);
	ins->mDst = mDst;
	ins->mConst = mConst;
	ins->mOperator = mOperator;
	ins->mNumOperands = mNumOperands;
	for (int i = 0; i < mNumOperands; i++)
		ins->mSrc[i] = mSrc[i];
	ins->mInUse = mInUse;
	ins->mInvariant = mInvariant;
	ins->mVolatile = mVolatile;
	ins->mMemmap = mMemmap;
	ins->mExpensive = mExpensive;
	ins->mSingleAssignment = mSingleAssignment;
	ins->mNoSideEffects = mNoSideEffects;
	ins->mConstExpr = mConstExpr;
	ins->mRemove = false;
	ins->mAliasing = mAliasing;

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

void ValueSet::UpdateValue(InterCodeBasicBlock * block, InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs)
{
	int	i, temp;

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
					ins->mConst.mType = mInstructions[i]->mSrc[0].mType;
					ins->mConst.mIntConst = LimitIntConstValue(mInstructions[i]->mDst.mType, mInstructions[i]->mSrc[0].mIntConst);
					ins->mNumOperands = 0;
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
				block->LoadConstantFold(ins, tvalue[ins->mSrc[0].mTemp], staticVars, staticProcs);
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
	case IC_FILL:
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
				ins->mConst.mType = IT_FLOAT;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;

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
				ins->mConst.mIntConst = LimitIntConstValue(ins->mDst.mType, ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst));
				ins->mConst.mType = ins->mDst.mType;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

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

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND) && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mConst.mType = ins->mDst.mType;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if (ins->mOperator == IA_MUL && tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[0] = ins->mSrc[1];
					ins->mSrc[1].mTemp = -1;
					ins->mSrc[1].mType = IT_NONE;
					ins->mNumOperands = 1;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

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

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND ||
					ins->mOperator == IA_SHL || ins->mOperator == IA_SHR || ins->mOperator == IA_SAR) && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mConst.mType = ins->mDst.mType;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if (ins->mOperator == IA_MUL && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == -1)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[1].mTemp = -1;
					ins->mSrc[1].mType = IT_NONE;
					ins->mNumOperands = 1;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if (ins->mOperator == IA_SUB && tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst == 0)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
			}
			else if (ins->mSrc[0].mTemp == ins->mSrc[1].mTemp)
			{
				if (ins->mOperator == IA_SUB || ins->mOperator == IA_XOR)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mIntConst = 0;
					ins->mConst.mType = ins->mDst.mType;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

					return;
				}
				else if (ins->mOperator == IA_AND || ins->mOperator == IA_OR)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
					assert(ins->mSrc[0].mTemp >= 0);

					UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);

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
				ins->mConst.mType = IT_FLOAT;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;

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
				ins->mConst.mIntConst = LimitIntConstValue(ins->mDst.mType, ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst));
				ins->mConst.mType = ins->mDst.mType;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;

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
				ins->mConst.mType = IT_BOOL;
				ins->mConst.mIntConst = ConstantRelationalFolding(ins->mOperator, tvalue[ins->mSrc[1].mTemp]->mConst.mFloatConst, tvalue[ins->mSrc[0].mTemp]->mConst.mFloatConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
				ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mType = IT_BOOL;
				ins->mConst.mIntConst = ConstantFolding(ins->mOperator, ins->mSrc[0].mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONVERSION_OPERATOR && 
					 tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_INT2FLOAT &&
					 tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2UINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_UINT2FLOAT &&
					 tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2LINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_LINT2FLOAT &&
					 tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2LUINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_LUINT2FLOAT &&
					 tvalue[ins->mSrc[0].mTemp]->mOperator == tvalue[ins->mSrc[1].mTemp]->mOperator)
			{
				ins->mSrc[0].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
				ins->mSrc[0].mTemp = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mTemp;
				ins->mSrc[1].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
				ins->mSrc[1].mTemp = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mTemp;

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			else if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT &&
					tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_INT2FLOAT &&
					tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2UINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_UINT2FLOAT &&
					tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2LINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_LINT2FLOAT &&
					tvalue[ins->mSrc[1].mTemp]->mOperator != IA_FLOAT2LUINT && tvalue[ins->mSrc[1].mTemp]->mOperator != IA_LUINT2FLOAT)
			{
				bool	toconst = false;
				int		cvalue = 0;

				if (tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO16S || tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT8TO32S)
				{
					int64	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					if (ins->mSrc[1].mType == IT_INT16 && (ivalue < -32768 || ivalue > 32767))
					{

					}
					else if (ivalue < -128)
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
					int64	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
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
				else if (tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT16TO32U)
				{
					int64	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
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
					else if (ivalue > 65535)
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
				else if (tvalue[ins->mSrc[1].mTemp]->mOperator == IA_EXT16TO32S)
				{
					int64	ivalue = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					if (ivalue < -32768)
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
					else if (ivalue > 32767)
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

				if (toconst)
				{
					ins->mCode = IC_CONSTANT;
					ins->mConst.mType = IT_BOOL;
					ins->mConst.mIntConst = cvalue;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;
				}
				else
				{
					ins->mSrc[0].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
					ins->mSrc[1].mType = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mType;
					ins->mSrc[1].mTemp = tvalue[ins->mSrc[1].mTemp]->mSrc[0].mTemp;
				}

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONVERSION_OPERATOR &&
			 	     ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT &&
					tvalue[ins->mSrc[0].mTemp]->mOperator != IA_FLOAT2INT && tvalue[ins->mSrc[0].mTemp]->mOperator != IA_INT2FLOAT &&
					tvalue[ins->mSrc[0].mTemp]->mOperator != IA_FLOAT2UINT && tvalue[ins->mSrc[0].mTemp]->mOperator != IA_UINT2FLOAT &&
					tvalue[ins->mSrc[0].mTemp]->mOperator != IA_FLOAT2LINT && tvalue[ins->mSrc[0].mTemp]->mOperator != IA_LINT2FLOAT &&
					tvalue[ins->mSrc[0].mTemp]->mOperator != IA_FLOAT2LUINT && tvalue[ins->mSrc[0].mTemp]->mOperator != IA_LUINT2FLOAT)
					{
				bool	toconst = false;
				int		cvalue = 0;

				if (tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO16S || tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT8TO32S)
				{
					int64	ivalue = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
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
					int64	ivalue = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
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
				else if (tvalue[ins->mSrc[0].mTemp]->mOperator == IA_EXT16TO32U)
				{
					int64	ivalue = tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst;
					if (ivalue > 65535)
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
					ins->mConst.mType = IT_BOOL;
					ins->mConst.mIntConst = cvalue;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;
				}
				else
				{
					ins->mSrc[1].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
					ins->mSrc[0].mType = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mType;
					ins->mSrc[0].mTemp = tvalue[ins->mSrc[0].mTemp]->mSrc[0].mTemp;
				}

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			else if (ins->mSrc[1].mTemp == ins->mSrc[0].mTemp)
			{
				ins->mCode = IC_CONSTANT;
				ins->mConst.mType = IT_BOOL;

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
				ins->mNumOperands = 0;

				UpdateValue(block, ins, tvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			}
			break;
		}
		break;
	case IC_BRANCH:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			InterInstruction* tins = tvalue[ins->mSrc[0].mTemp];
			if (IsIntegerType(tins->mConst.mType) || tins->mConst.mType == IT_BOOL)
			{
				if (tins->mConst.mIntConst)
					ins->mCode = IC_JUMP;
				else
					ins->mCode = IC_JUMPF;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
			}
			else if (tins->mConst.mType == IT_POINTER)
			{
				if (tins->mConst.mMemory == IM_ABSOLUTE)
				{
					if (tins->mConst.mIntConst)
						ins->mCode = IC_JUMP;
					else
						ins->mCode = IC_JUMPF;
					ins->mSrc[0].mTemp = -1;
					ins->mNumOperands = 0;
				}
				else if (tins->mConst.mMemory == IM_GLOBAL || tins->mConst.mMemory == IM_LOCAL || tins->mConst.mMemory == IM_PARAM || tins->mConst.mMemory == IM_FPARAM)
				{
					ins->mCode = IC_JUMP;
					ins->mSrc[0].mTemp = -1;
					ins->mNumOperands = 0;
				}
			}
			else if (tins->mConst.mType == IT_FLOAT)
			{
				if (tins->mConst.mFloatConst)
					ins->mCode = IC_JUMP;
				else
					ins->mCode = IC_JUMPF;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
			}
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
	: mTemp(INVALID_TEMPORARY), mType(IT_NONE), mFinal(false), mIntConst(0), mFloatConst(0), mVarIndex(-1), mOperandSize(0), mLinkerObject(nullptr), mMemory(IM_NONE), mStride(1), mRestricted(0), mMemoryBase(IM_NONE)
{}

bool InterOperand::IsNotUByte(void) const
{
	return
		mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue < 0 ||
		mRange.mMaxState == IntegerValueRange::S_BOUND && mRange.mMaxValue >= 256;
}

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

bool InterOperand::IsPositive(void) const
{
	return mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue >= 0;
}

bool InterOperand::IsInRange(int lower, int upper) const
{
	return
		mRange.mMinState == IntegerValueRange::S_BOUND && mRange.mMinValue >= lower &&
		mRange.mMaxState == IntegerValueRange::S_BOUND && mRange.mMaxValue <= upper;
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
			return mRange.mMaxValue < 0x80000000ll;
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
	mMemoryBase = op.mMemoryBase;
	mTemp = op.mTemp;
	mType = op.mType;
	mRange = op.mRange;
	mStride = op.mStride;
	mRestricted = op.mRestricted;
	mFinal = false;
}

void InterOperand::Forward(const InterOperand& op)
{
	mTemp = op.mTemp;
	if (mType != IT_INT8 || op.mType != IT_INT16 && op.mType != IT_INT32)
		mType = op.mType;
	mRange = op.mRange;
	mIntConst = op.mIntConst;
	mFloatConst = op.mFloatConst;
	mFinal = false;
}

void InterOperand::ForwardTemp(const InterOperand& op)
{
	mTemp = op.mTemp;
	if (mType != IT_INT8 || op.mType != IT_INT16 && op.mType != IT_INT32)
		mType = op.mType;
	mRange = op.mRange;
	mFinal = false;
}

bool InterOperand::IsEqual(const InterOperand& op) const
{
	if (mType != op.mType || mTemp != op.mTemp)
		return false;

	if (mTemp < 0 && mMemory != op.mMemory)
		return false;

	if (mIntConst != op.mIntConst || mFloatConst != op.mFloatConst)
		return false;

	if (mStride != op.mStride)
		return false;

	if (mMemory != IM_NONE && mMemory != IM_INDIRECT)
	{
		if (mVarIndex != op.mVarIndex || mLinkerObject != op.mLinkerObject)
			return false;
	}

	return true;
}

InterInstruction::InterInstruction(const Location& loc, InterCode code)
	: mLocation(loc), mCode(code), mSrc(mOps)
{
	mOperator = IA_NONE;

	switch (code)
	{
	case IC_LOAD_TEMPORARY:
	case IC_LOAD:
	case IC_UNARY_OPERATOR:
	case IC_BRANCH:
	case IC_TYPECAST:
	case IC_RETURN_VALUE:
	case IC_RETURN_STRUCT:
	case IC_CONVERSION_OPERATOR:
	case IC_DISPATCH:
		mNumOperands = 1;
		break;

	case IC_BINARY_OPERATOR:
	case IC_RELATIONAL_OPERATOR:
	case IC_STORE:
	case IC_LEA:
		mNumOperands = 2;
		break;

	case IC_CONSTANT:
	case IC_JUMP:
	case IC_JUMPF:
	case IC_RETURN:
		mNumOperands = 0;
		break;
	case IC_ASSEMBLER:
		mSrc = new InterOperand[32];
		mNumOperands = 1;
		break;
	default:
		mNumOperands = 3;
		break;
	}

	mInUse = false;
	mVolatile = false;
	mInvariant = false;
	mSingleAssignment = false;
	mNoSideEffects = false;
	mConstExpr = false;
	mAliasing = false;
	mMemmap = false;
	mLockOrder = false;
}

void InterInstruction::Reset(void)
{
	mCode = IC_NONE;
	mNumOperands = 0;
	mDst.mTemp = -1;
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

void InterInstruction::CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable, int& nlocals, int& nparams)
{
	if (mCode == IC_CONSTANT)
	{
		if (mDst.mType == IT_POINTER && mConst.mMemory == IM_LOCAL)
		{
			localTable[mDst.mTemp] = mConst.mVarIndex;
			if (mConst.mVarIndex >= nlocals)
				nlocals = mConst.mVarIndex + 1;
		}
		else if (mDst.mType == IT_POINTER && (mConst.mMemory == IM_PARAM || mConst.mMemory == IM_FPARAM))
		{
			paramTable[mDst.mTemp] = mConst.mVarIndex;
			if (mConst.mVarIndex >= nparams)
				nparams = mConst.mVarIndex + 1;
		}
	}
	else if (mCode == IC_LEA)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			if (mSrc[1].mTemp >= 0)
				localTable[mDst.mTemp] = localTable[mSrc[1].mTemp];
			else
			{
				localTable[mDst.mTemp] = mSrc[1].mVarIndex;
				if (mSrc[1].mVarIndex >= nlocals)
					nlocals = mSrc[1].mVarIndex + 1;
			}
		}
		else if (mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM)
		{
			if (mSrc[1].mTemp >= 0)
				paramTable[mDst.mTemp] = paramTable[mSrc[1].mTemp];
			else
			{
				paramTable[mDst.mTemp] = mSrc[1].mVarIndex;
				if (mSrc[1].mVarIndex >= nparams)
					nparams = mSrc[1].mVarIndex + 1;
			}
		}
		else if (mSrc[1].mTemp >= 0)
		{
			localTable[mDst.mTemp] = localTable[mSrc[1].mTemp];
			paramTable[mDst.mTemp] = paramTable[mSrc[1].mTemp];
		}
	}
	else if (mCode == IC_LOAD_TEMPORARY)
	{
		localTable[mDst.mTemp] = localTable[mSrc[0].mTemp];
		paramTable[mDst.mTemp] = paramTable[mSrc[0].mTemp];
	}
	else if (mCode == IC_STORE && mSrc[1].mTemp < 0)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			if (mSrc[1].mVarIndex >= nlocals)
				nlocals = mSrc[1].mVarIndex + 1;
		}
		else if (mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM)
		{
			if (mSrc[1].mVarIndex >= nparams)
				nparams = mSrc[1].mVarIndex + 1;
		}
	}
	else if (mCode == IC_LOAD && mSrc[0].mTemp < 0)
	{
		if (mSrc[0].mMemory == IM_LOCAL)
		{
			if (mSrc[0].mVarIndex >= nlocals)
				nlocals = mSrc[0].mVarIndex + 1;
		}
		else if (mSrc[0].mMemory == IM_PARAM || mSrc[0].mMemory == IM_FPARAM)
		{
			if (mSrc[0].mVarIndex >= nparams)
				nparams = mSrc[0].mVarIndex + 1;
		}
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
			if (mSrc[0].mVarIndex >= 0 && !providedVars[mSrc[0].mVarIndex])
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
			if (mSrc[1].mVarIndex >= 0)
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == staticVars[mSrc[1].mVarIndex]->mSize)
					providedVars += mSrc[1].mVarIndex;
				else if (!providedVars[mSrc[1].mVarIndex])
					requiredVars += mSrc[1].mVarIndex;
			}
		}
	}
	else if (mCode == IC_COPY || mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE || mCode == IC_STRCPY || mCode == IC_DISPATCH || mCode == IC_FILL)
	{
		requiredVars.OrNot(providedVars);
	}
}

void InterInstruction::FilterStaticVarsByteUsage(const GrowingVariableArray& staticVars, NumberSet& requiredVars, NumberSet& providedVars, Errors* errors)
{
	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_INDIRECT)
		{
			if (!mSrc[0].mRestricted)
			{
				for (int i = 0; i < staticVars.Size(); i++)
				{
					if (staticVars[i]->mAliased && !providedVars.RangeFilled(staticVars[i]->mByteIndex, staticVars[i]->mSize))
						requiredVars.AddRange(staticVars[i]->mByteIndex, staticVars[i]->mSize);
				}
			}
		}
		else if (mSrc[0].mMemory == IM_GLOBAL)
		{
			if (mSrc[0].mVarIndex >= 0)
			{
				if (int(mSrc[0].mIntConst) < 0 || int(mSrc[0].mIntConst) + InterTypeSize[mDst.mType] > staticVars[mSrc[0].mVarIndex]->mSize)
					errors->Error(mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Index out of bounds");
				else if (!providedVars.RangeFilled(staticVars[mSrc[0].mVarIndex]->mByteIndex + int(mSrc[0].mIntConst), InterTypeSize[mDst.mType]))
					requiredVars.AddRange(staticVars[mSrc[0].mVarIndex]->mByteIndex + int(mSrc[0].mIntConst), InterTypeSize[mDst.mType]);
			}
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_INDIRECT)
		{
		}
		else if (mSrc[1].mMemory == IM_GLOBAL)
		{
			if (mSrc[1].mVarIndex >= 0)
			{
				if (int(mSrc[1].mIntConst) < 0 || int(mSrc[1].mIntConst) + InterTypeSize[mSrc[0].mType] > staticVars[mSrc[1].mVarIndex]->mSize)
					errors->Error(mLocation, EWARN_INDEX_OUT_OF_BOUNDS, "Index out of bounds");
				else 
					providedVars.AddRange(staticVars[mSrc[1].mVarIndex]->mByteIndex + int(mSrc[1].mIntConst), InterTypeSize[mSrc[0].mType]);
			}
		}
	}
	else if (mCode == IC_COPY || mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_ASSEMBLER || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE || mCode == IC_STRCPY || mCode == IC_DISPATCH || mCode == IC_FILL)
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
#if 0
	else if (mCode == IC_LEA)
	{
		if (mSrc[1].mMemory == IM_LOCAL)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedVars[mSrc[1].mVarIndex])
				requiredVars += mSrc[1].mVarIndex;
		}
		else if (mSrc[1].mMemory == paramMemory)
		{
			assert(mSrc[1].mTemp < 0);
			if (!providedParams[mSrc[1].mVarIndex])
				requiredParams += mSrc[1].mVarIndex;
		}
	}
	else if (mCode == IC_CONSTANT)
	{
		if (mConst.mMemory == IM_LOCAL)
		{
			if (!providedVars[mConst.mVarIndex])
				requiredVars += mConst.mVarIndex;
		}
		else if (mConst.mMemory == paramMemory)
		{
			assert(mConst.mTemp < 0);
			if (!providedParams[mConst.mVarIndex])
				requiredParams += mConst.mVarIndex;
		}
	}
#endif
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
	else if (mCode == IC_FILL)
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
			mSrc[0].mIntConst = ToTypedUnsigned(mSrc[0].mIntConst + ains->mConst.mIntConst, IT_INT16);
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
			mSrc[1].mIntConst = ToTypedUnsigned(mSrc[1].mIntConst + ains->mConst.mIntConst, IT_INT16);
			mSrc[1].mLinkerObject = ains->mConst.mLinkerObject;
			mSrc[1].mVarIndex = ains->mConst.mVarIndex;
			mSrc[1].mMemory = ains->mConst.mMemory;
			mSrc[1].mTemp = -1;
			return true;
		}
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp] && IsIntegerType(mSrc[0].mType))
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mSrc[0].mIntConst = LimitIntConstValue(mSrc[0].mType, ains->mConst.mIntConst);
			mSrc[0].mTemp = -1;
			return true;
		}
		break;
	case IC_LOAD_TEMPORARY:
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mCode = IC_CONSTANT;
			mConst = ains->mConst;
			mSrc[0].mTemp = -1;
			mNumOperands = 0;
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
				mSrc[i].mType = mDst.mType;
				if (IsIntegerType(mSrc[i].mType))
					mSrc[i].mIntConst = LimitIntConstValue(mSrc[i].mType, mSrc[i].mIntConst);
				changed = true;
			}
		}

		if (changed)
		{
			this->ConstantFolding();
			return true;
		}
	} break;

	case IC_RELATIONAL_OPERATOR:
	{
		bool	changed = false;

		for (int i = 0; i < 2; i++)
		{
			if (mSrc[i].mTemp >= 0 && ctemps[mSrc[i].mTemp])
			{
				InterType	t = mSrc[i].mType;
				InterInstruction* ains = ctemps[mSrc[i].mTemp];

				if (t != IT_POINTER || ains->mConst.mMemory == IM_ABSOLUTE || ains->mConst.mMemory == IM_GLOBAL)
				{
					mSrc[i] = ains->mConst;
					mSrc[i].mType = t;
					changed = true;
				}
				else if (t == IT_POINTER && ains->mConst.mType == IT_INT16)
				{
					mSrc[i] = ains->mConst;
					mSrc[i].mMemory = IM_ABSOLUTE;
					mSrc[i].mType = IT_POINTER;
					changed = true;
				}
			}
		}

		if (!changed)
		{
			if (mSrc[0].mType == IT_POINTER && mSrc[1].mType == IT_POINTER &&
				mSrc[0].mTemp >= 0 && mSrc[1].mTemp >= 0 &&
				ctemps[mSrc[0].mTemp] && ctemps[mSrc[1].mTemp])
			{
				InterInstruction* si0 = ctemps[mSrc[0].mTemp];
				InterInstruction* si1 = ctemps[mSrc[1].mTemp];

				if (si0->mConst.mMemory != IM_INDIRECT && si1->mConst.mMemory != IM_INDIRECT)
				{
					mCode = IC_CONSTANT;
					mConst.mIntConst = ::ConstantRelationalPointerFolding(mOperator, si1->mConst, si0->mConst);
					mConst.mType = IT_BOOL;
					mNumOperands = 0;
					return true;
				}
			}
		}

		if (changed)
		{
			this->ConstantFolding();
			return true;
		}

	} break;

	case IC_FREE:
	{
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];

			if (ains->mConst.mMemory == IM_ABSOLUTE && ains->mConst.mIntConst == 0)
			{
				mCode = IC_NONE;
				mDst.mTemp = -1;
				mNumOperands = 0;
				return true;
			}
		}

	}	break;

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
		else if (mSrc[1].mTemp >= 0 && ctemps[mSrc[1].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[1].mTemp];
			mSrc[1] = ains->mConst;
			mSrc[1].mType = IT_POINTER;

			this->ConstantFolding();
			return true;
		}
		break;

	case IC_BRANCH:
		if (mSrc[0].mTemp >= 0 && ctemps[mSrc[0].mTemp])
		{
			InterInstruction* ains = ctemps[mSrc[0].mTemp];
			mSrc[0] = ains->mConst;

			this->ConstantFolding();
			return true;
		}
		break;
	}


	return false;
}

void InterInstruction::PerformTempForwarding(TempForwardingTable& forwardingTable, bool reverse)
{
	if (mCode != IC_NONE)
	{
		for (int i = 0; i < mNumOperands; i++)
			PerformTempUseForwarding(mSrc[i].mTemp, forwardingTable);
		PerformTempDefineForwarding(mDst.mTemp, forwardingTable);
	}
	if (mCode == IC_LOAD_TEMPORARY && mDst.mTemp != mSrc[0].mTemp)
	{
		if (reverse)
			forwardingTable.Build(mSrc[0].mTemp, mDst.mTemp);
		else
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
	else if (mCode == IC_COPY && !mVolatile && mSrc[0].mTemp < 0 && mSrc[1].mTemp < 0 && mSrc[0].mMemory == mSrc[1].mMemory &&
		mSrc[0].mVarIndex == mSrc[1].mVarIndex && mSrc[0].mLinkerObject == mSrc[1].mLinkerObject && mSrc[0].mIntConst == mSrc[1].mIntConst)
	{
		mNumOperands = 0;
		mDst.mTemp = -1;
		mCode = IC_NONE;
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
				mNumOperands = 0;

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

	if (mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_DISPATCH)
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
				mNumOperands = 0;
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
				mNumOperands = 0;
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
	}
	else if (mCode == IC_SELECT)
	{
		if (mDst.mType == IT_POINTER)
		{
			if (mSrc[1].mTemp < 0)
			{
				if (mSrc[1].mMemory == IM_LOCAL)
				{
					requiredVars += mSrc[1].mVarIndex;
				}
				else if (mSrc[1].mMemory == paramMemory)
				{
					requiredParams += mSrc[1].mVarIndex;
				}
			}
			if (mSrc[2].mTemp < 0)
			{
				if (mSrc[2].mMemory == IM_LOCAL)
				{
					requiredVars += mSrc[2].mVarIndex;
				}
				else if (mSrc[2].mMemory == paramMemory)
				{
					requiredParams += mSrc[2].mVarIndex;
				}
			}
		}
	}
	else if (mCode == IC_CONSTANT)
	{
		if (mConst.mType == IT_POINTER)
		{
			if (mConst.mMemory == IM_LOCAL)
			{
				requiredVars += mConst.mVarIndex;
			}
			else if (mConst.mMemory == paramMemory)
			{
				requiredParams += mConst.mVarIndex;
			}
		}
	}
	else if (mCode == IC_COPY)
	{
		if (!mVolatile && mSrc[0].mTemp < 0 && mSrc[1].mTemp < 0 && mSrc[0].mMemory == mSrc[1].mMemory &&
			mSrc[0].mVarIndex == mSrc[1].mVarIndex && mSrc[0].mLinkerObject == mSrc[1].mLinkerObject && mSrc[0].mIntConst == mSrc[1].mIntConst)
		{
			mNumOperands = 0;
			mSrc[0].mTemp = -1;
			mCode = IC_NONE;
			changed = true;
		}
		else if (mSrc[1].mMemory == IM_LOCAL)
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
				mNumOperands = 0;
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
				mNumOperands = 0;
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

bool InterInstruction::RemoveUnusedStaticStoreInstructions(InterCodeBasicBlock* block, const GrowingVariableArray& staticVars, NumberSet& requiredVars, GrowingInstructionPtrArray& storeIns)
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
			if (mSrc[0].mVarIndex >= 0)
				requiredVars += mSrc[0].mVarIndex;
		}

		int k = 0;
		for (int i = 0; i < storeIns.Size(); i++)
		{
			if (!block->CollidingMem(this, storeIns[i]))
				storeIns[k++] = storeIns[i];
		}
		storeIns.SetSize(k);
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_GLOBAL && mSrc[1].mVarIndex >= 0)
		{
			if (requiredVars[mSrc[1].mVarIndex])
			{
				if (mSrc[1].mIntConst == 0 && mSrc[1].mOperandSize == staticVars[mSrc[1].mVarIndex]->mSize)
					requiredVars -= mSrc[1].mVarIndex;
			}
			else if (!mVolatile)
			{
				mNumOperands = 0;
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
		else
		{
			int i = 0;
			while (i < storeIns.Size() && !SameMem(mSrc[1], storeIns[i]))
				i++;
			if (!mVolatile && i < storeIns.Size())
			{
				mCode = IC_NONE;
				mNumOperands = 0;
				changed = true;
			}
			else
			{
				int k = 0;
				for (int i = 0; i < storeIns.Size(); i++)
				{
					if (!block->CollidingMem(this, storeIns[i]))
						storeIns[k++] = storeIns[i];
				}
				storeIns.SetSize(k);
			}
		}
	}
	else if (mCode == IC_SELECT)
	{
		if (mDst.mType == IT_POINTER)
		{
			if (mSrc[1].mTemp < 0)
			{
				if (mSrc[1].mMemory == IM_GLOBAL && mSrc[1].mVarIndex >= 0)
					requiredVars += mSrc[1].mVarIndex;
			}
			if (mSrc[2].mTemp < 0)
			{
				if (mSrc[2].mMemory == IM_GLOBAL && mSrc[2].mVarIndex >= 0)
					requiredVars += mSrc[2].mVarIndex;
			}
		}
	}
	else if (mCode == IC_COPY || mCode == IC_STRCPY || mCode == IC_FILL)
	{
		requiredVars.Fill();
		storeIns.SetSize(0);
	}
	else if (mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_ASSEMBLER || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE || mCode == IC_DISPATCH)
	{
		requiredVars.Fill();
		storeIns.SetSize(0);
	}

	if (mCode == IC_STORE)
		storeIns.Push(this);

	if (mDst.mTemp >= 0)
	{
		int k = 0;
		for (int i = 0; i < storeIns.Size(); i++)
		{
			if (storeIns[i]->mSrc[1].mTemp != mDst.mTemp)
				storeIns[k++] = storeIns[i];
		}
		storeIns.SetSize(k);
	}

	return changed;
}

bool InterInstruction::RemoveUnusedStaticStoreByteInstructions(InterCodeBasicBlock* block, const GrowingVariableArray& staticVars, NumberSet& requiredVars)
{
	bool	changed = false;

	if (mCode == IC_LOAD)
	{
		if (mSrc[0].mMemory == IM_INDIRECT)
		{
			if (!mSrc[0].mRestricted)
			{
				for (int i = 0; i < staticVars.Size(); i++)
				{
					if (staticVars[i]->mAliased)
						requiredVars.AddRange(staticVars[i]->mByteIndex, staticVars[i]->mSize);
				}
			}
		}
		else if (mSrc[0].mMemory == IM_GLOBAL)
		{
			if (mSrc[0].mVarIndex >= 0)
				requiredVars.AddRange(staticVars[mSrc[0].mVarIndex]->mByteIndex + int(mSrc[0].mIntConst), InterTypeSize[mDst.mType]);
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mSrc[1].mMemory == IM_GLOBAL && mSrc[1].mVarIndex >= 0)
		{
			if (!requiredVars.RangeClear(staticVars[mSrc[1].mVarIndex]->mByteIndex + int(mSrc[1].mIntConst), InterTypeSize[mSrc[0].mType]))
			{
				requiredVars.SubRange(staticVars[mSrc[1].mVarIndex]->mByteIndex + int(mSrc[1].mIntConst), InterTypeSize[mSrc[0].mType]);
			}
			else if (!mVolatile)
			{
				mNumOperands = 0;
				mSrc[0].mTemp = -1;
				mCode = IC_NONE;
				changed = true;
			}
		}
	}
	else if (mCode == IC_COPY || mCode == IC_STRCPY || mCode == IC_FILL)
	{
		requiredVars.Fill();
	}
	else if (mCode == IC_CALL || mCode == IC_CALL_NATIVE || mCode == IC_ASSEMBLER || mCode == IC_RETURN || mCode == IC_RETURN_STRUCT || mCode == IC_RETURN_VALUE || mCode == IC_DISPATCH)
	{
		requiredVars.Fill();
	}

	return changed;
}

int InterInstruction::NumUsedTemps(void) const
{
	int n = 0;
	for (int i = 0; i < mNumOperands; i++)
		if (mSrc[i].mTemp >= 0)
			n++;
	return n;
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

			ins = tvalue.getAt(j);

			if (ins->UsesTemp(temp))
			{
				tvalue.destroyAt(j);
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
		// Ensure collision with unused destination register
		UpdateCollisionSet(liveTemps, collisionSets, mDst.mTemp);
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
	case IC_FILL:
		if (mSrc[1].mMemory == IM_LOCAL && mSrc[1].mTemp < 0)
			complexLocals += mSrc[1].mVarIndex;
		else if ((mSrc[1].mMemory == IM_PARAM || mSrc[1].mMemory == IM_FPARAM) && mSrc[1].mTemp < 0)
			complexParams += mSrc[1].mVarIndex;
		break;
	case IC_CONSTANT:
		if (mDst.mType == IT_POINTER && mConst.mMemory == IM_LOCAL)
			complexLocals += mConst.mVarIndex;
		else if (mDst.mType == IT_POINTER && (mConst.mMemory == IM_PARAM || mConst.mMemory == IM_FPARAM))
			complexParams += mConst.mVarIndex;
		break;
	}
}

void InterInstruction::UnionRanges(InterInstruction* ins)
{
	mDst.mRange.Union(ins->mDst.mRange);
	for(int i=0; i<mNumOperands; i++)
		mSrc[i].mRange.Union(ins->mSrc[i].mRange);
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
				mConst.mType = mSrc[0].mType;
				mConst.mIntConst = mSrc[0].mIntConst;
				mConst.mFloatConst = mSrc[0].mFloatConst;
				mNumOperands = 0;
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
			if (mSrc[0].mType == IT_POINTER)
				mConst.mIntConst = ::ConstantRelationalPointerFolding(mOperator, mSrc[1], mSrc[0]);
			else if (IsIntegerType(mSrc[0].mType) || mSrc[0].mType == IT_BOOL)
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
			mConst.mType = mDst.mType;
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
					mConst.mType = mDst.mType;
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
				mConst.mIntConst = ::ConstantFolding(mOperator, mDst.mType, mSrc[0].mIntConst);
			mConst.mType = mDst.mType;
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
	case IC_TYPECAST:
		if (mSrc[0].mTemp < 0)
		{
			if (mDst.mType == IT_POINTER && IsIntegerType(mSrc[0].mType))
			{
				mCode = IC_CONSTANT;
				mConst.mType = IT_POINTER;
				mConst.mMemory = IM_ABSOLUTE;
				mConst.mIntConst = mSrc[0].mIntConst;
				mConst.mLinkerObject = nullptr;
				mConst.mOperandSize = 2;
				mNumOperands = 0;
				return true;
			}
		}
		break;
	case IC_LOAD_TEMPORARY:
		if (mDst.mTemp == mSrc[0].mTemp)
		{
			mCode = IC_NONE;
			mDst.mTemp = -1;
			for (int i = 0; i < mNumOperands; i++)
				mSrc[i].mTemp = -1;
			mNumOperands = 0;
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
	case IC_BRANCH:
		if (mSrc[0].mTemp < 0)
		{
			if (IsIntegerType(mSrc[0].mType))
				mSrc[0].mIntConst = mSrc[0].mIntConst != 0;
			else if (mSrc[0].mType == IT_FLOAT)
				mSrc[0].mIntConst = mSrc[0].mFloatConst != 0;
			else if (mSrc[0].mType == IT_POINTER)
			{
				if (mSrc[0].mMemory == IM_ABSOLUTE)
					mSrc[0].mIntConst = mSrc[0].mIntConst != 0;
				else
					mSrc[0].mIntConst = 1;
			}
			mSrc[0].mType = IT_BOOL;
			return true;
		}
		break;
	case IC_SELECT:
		if (mSrc[2].mTemp < 0)
		{
			bool	cond = mSrc[2].mIntConst != 0;

			if (IsIntegerType(mSrc[2].mType))
				cond = mSrc[2].mIntConst != 0;
			else if (mSrc[2].mType == IT_FLOAT)
				cond = mSrc[2].mFloatConst != 0;
			else if (mSrc[2].mType == IT_POINTER)
			{
				if (mSrc[2].mMemory == IM_ABSOLUTE)
					cond = mSrc[2].mIntConst != 0;
				else
					cond = true;
			}

			int ci = cond ? 1 : 0;
			if (mSrc[ci].mTemp < 0)
			{
				mCode = IC_CONSTANT;
				mConst = mSrc[ci];
			}
			else
			{
				mCode = IC_LOAD_TEMPORARY;
				mSrc[0] = mSrc[ci];
			}
			mNumOperands = 1;
			return true;
		}
		break;
	}

	return false;
}


void InterOperand::Disassemble(FILE* file, InterCodeProcedure* proc)
{
	static char typechars[] = "NBCILFP";

	if (mTemp >= 0) 
	{
		if (mFinal)
			fprintf(file, "R%d(%cF)", mTemp, typechars[mType]);
		else
			fprintf(file, "R%d(%c)", mTemp, typechars[mType]);

		if (mType == IT_POINTER && mMemory == IM_INDIRECT)
		{
			fprintf(file, "+%d", int(mIntConst));
		}

		if (mRestricted)
			fprintf(file, "{R%d}", mRestricted);
		else if (mMemory == IM_INDIRECT && mMemoryBase != IM_INDIRECT && mMemoryBase != IM_NONE)
		{
			const char* vname = "";
			bool	aliased = false;

			if (mMemoryBase == IM_LOCAL)
			{
				if (!proc->mLocalVars[mVarIndex])
					vname = "null";
				else if (!proc->mLocalVars[mVarIndex]->mIdent)
				{
					vname = "";
					aliased = proc->mLocalVars[mVarIndex]->mAliased;
				}
				else
				{
					vname = proc->mLocalVars[mVarIndex]->mIdent->mString;
					aliased = proc->mLocalVars[mVarIndex]->mAliased;
				}
			}
			else if (mMemoryBase == IM_PROCEDURE)
			{
				if (mVarIndex >= 0 && proc->mModule->mProcedures[mVarIndex])
					vname = proc->mModule->mProcedures[mVarIndex]->mIdent->mString;
				else if (mLinkerObject && mLinkerObject->mIdent)
					vname = mLinkerObject->mIdent->mString;
			}
			else if (mMemoryBase == IM_GLOBAL)
			{
				if (mVarIndex < 0)
				{
					if (mLinkerObject && mLinkerObject->mIdent)
						vname = mLinkerObject->mIdent->mString;
					else
						vname = "";
				}
				else if (!proc->mModule->mGlobalVars[mVarIndex])
					vname = "null";
				else if (!proc->mModule->mGlobalVars[mVarIndex]->mIdent)
				{
					vname = "";
					aliased = proc->mModule->mGlobalVars[mVarIndex]->mAliased;
				}
				else
				{
					vname = proc->mModule->mGlobalVars[mVarIndex]->mIdent->mString;
					aliased = proc->mModule->mGlobalVars[mVarIndex]->mAliased;
				}
			}
			else if (mMemoryBase == IM_ABSOLUTE)
			{
				vname = "ABS";
			}

			if (aliased)
				fprintf(file, " {V(%d '%s' A)} ", mVarIndex, vname);
			else
				fprintf(file, " {V(%d '%s')} ", mVarIndex, vname);
		}

		if (mRange.mMinState >= IntegerValueRange::S_WEAK || mRange.mMaxState >= IntegerValueRange::S_WEAK)
		{
			fprintf(file, "[");
			if (mRange.mMinState == IntegerValueRange::S_WEAK)
				fprintf(file, "~%lld", mRange.mMinValue);
			else if (mRange.mMinState == IntegerValueRange::S_BOUND)
				fprintf(file, "%lld", mRange.mMinValue);
			else if (mRange.mMinState == IntegerValueRange::S_UNKNOWN)
				fprintf(file, "?");
			fprintf(file, "..");
			if (mRange.mMaxState == IntegerValueRange::S_WEAK)
				fprintf(file, "~%lld", mRange.mMaxValue);
			else if (mRange.mMaxState == IntegerValueRange::S_BOUND)
				fprintf(file, "%lld", mRange.mMaxValue);
			else if (mRange.mMaxState == IntegerValueRange::S_UNKNOWN)
				fprintf(file, "?");
			fprintf(file, "]");
		}
	}
	else if (mType == IT_POINTER)
	{
		const char* vname = "";
		bool	aliased = false;

		if (mMemory == IM_LOCAL)
		{
			if (!proc->mLocalVars[mVarIndex])
				vname = "null";
			else if (!proc->mLocalVars[mVarIndex]->mIdent)
			{
				vname = "";
				aliased = proc->mLocalVars[mVarIndex]->mAliased;
			}
			else
			{
				vname = proc->mLocalVars[mVarIndex]->mIdent->mString;
				aliased = proc->mLocalVars[mVarIndex]->mAliased;
			}
		}
		else if (mMemory == IM_PROCEDURE)
		{
			if (mVarIndex >= 0 && proc->mModule->mProcedures[mVarIndex])
				vname = proc->mModule->mProcedures[mVarIndex]->mIdent->mString;
			else if (mLinkerObject && mLinkerObject->mIdent)
				vname = mLinkerObject->mIdent->mString;
		}
		else if (mMemory == IM_GLOBAL)
		{
			if (mVarIndex < 0)
			{
				if (mLinkerObject && mLinkerObject->mIdent)
					vname = mLinkerObject->mIdent->mString;
				else
					vname = "";
			}
			else if (!proc->mModule->mGlobalVars[mVarIndex])
				vname = "null";
			else if (!proc->mModule->mGlobalVars[mVarIndex]->mIdent)
			{
				vname = "";
				aliased = proc->mModule->mGlobalVars[mVarIndex]->mAliased;
			}
			else
			{
				vname = proc->mModule->mGlobalVars[mVarIndex]->mIdent->mString;
				aliased = proc->mModule->mGlobalVars[mVarIndex]->mAliased;
			}
		}
		
		if (aliased)
			fprintf(file, "V(%d '%s' A)+%d ", mVarIndex, vname, int(mIntConst));
		else
			fprintf(file, "V(%d '%s')+%d ", mVarIndex, vname, int(mIntConst));
	}
	else if (IsIntegerType(mType) || mType == IT_BOOL)
	{
		fprintf(file, "C%c:%lld", typechars[mType], mIntConst);
	}
	else if (mType == IT_FLOAT)
	{
		union { float f; uint32 u; } u;
		u.f = (float)mFloatConst;
		fprintf(file, "C%c:%f (%08x)", typechars[mType], mFloatConst, u.u);
	}
}

void InterInstruction::Disassemble(FILE* file, InterCodeProcedure* proc)
{
	if (this->mCode != IC_NONE)
	{
		static char memchars[] = "NPLGFPITAZC";

		fprintf(file, "\t");
		switch (this->mCode)
		{
		case IC_LOAD_TEMPORARY:
			assert(mNumOperands == 1);
			fprintf(file, "MOVE");
			break;
		case IC_BINARY_OPERATOR:
			assert(mNumOperands == 2);
			fprintf(file, "BINOP%d", mOperator);
			break;
		case IC_UNARY_OPERATOR:
			assert(mNumOperands == 1);
			fprintf(file, "UNOP%d", mOperator);
			break;
		case IC_RELATIONAL_OPERATOR:
			assert(mNumOperands == 2);
			fprintf(file, "RELOP%d", mOperator);
			break;
		case IC_CONVERSION_OPERATOR:
			assert(mNumOperands == 1);
			fprintf(file, "CONV%d", mOperator);
			break;
		case IC_STORE:
			assert(mNumOperands == 2);
			if (mSrc[1].mStride != 1)
				fprintf(file, "STORE%c%d:%d", memchars[mSrc[1].mMemory], mSrc[1].mOperandSize, mSrc[1].mStride);
			else
				fprintf(file, "STORE%c%d", memchars[mSrc[1].mMemory], mSrc[1].mOperandSize);
			break;
		case IC_LOAD:
			assert(mNumOperands == 1);
			if (mSrc[0].mStride != 1)
				fprintf(file, "LOAD%c%d:%d", memchars[mSrc[0].mMemory], mSrc[0].mOperandSize, mSrc[0].mStride);
			else
				fprintf(file, "LOAD%c%d", memchars[mSrc[0].mMemory], mSrc[0].mOperandSize);
			break;
		case IC_COPY:
			if (mSrc[1].mStride != 1)
			{
				if (mSrc[0].mStride != 1)
					fprintf(file, "COPY%d:%c%d%c%d", mConst.mOperandSize, memchars[mSrc[0].mMemory], mSrc[0].mStride, memchars[mSrc[1].mMemory], mSrc[1].mStride);
				else
					fprintf(file, "COPY%d:%c%c%d", mConst.mOperandSize, memchars[mSrc[0].mMemory], memchars[mSrc[1].mMemory], mSrc[1].mStride);
			}
			else if (mSrc[0].mStride != 1)
				fprintf(file, "COPY%d:%c%d%c", mConst.mOperandSize, memchars[mSrc[0].mMemory], mSrc[0].mStride, memchars[mSrc[1].mMemory]);
			else
				fprintf(file, "COPY%d:%c%c", mConst.mOperandSize, memchars[mSrc[0].mMemory], memchars[mSrc[1].mMemory]);
			break;
		case IC_FILL:
			assert(mNumOperands == 2);
			if (mSrc[1].mStride != 1)
				fprintf(file, "FILL%c%d:%d", memchars[mSrc[1].mMemory], mConst.mOperandSize, mSrc[1].mStride);
			else
				fprintf(file, "FILL%c%d", memchars[mSrc[1].mMemory], mConst.mOperandSize);
			break;

		case IC_MALLOC:
			assert(mNumOperands == 1);
			fprintf(file, "MALLOC");
			break;
		case IC_FREE:
			assert(mNumOperands == 1);
			fprintf(file, "FREE");
			break;

		case IC_STRCPY:
			fprintf(file, "STRCPY%c%c", memchars[mSrc[0].mMemory], memchars[mSrc[1].mMemory]);
			break;
		case IC_LEA:
			assert(mNumOperands == 2);
			fprintf(file, "LEA%c", memchars[mSrc[1].mMemory]);
			break;
		case IC_TYPECAST:
			assert(mNumOperands == 1);
			fprintf(file, "CAST");
			break;
		case IC_SELECT:
			assert(mNumOperands == 3);
			fprintf(file, "SELECT");
			break;
		case IC_CONSTANT:
			assert(mNumOperands == 0);
			fprintf(file, "CONST");
			break;
		case IC_BRANCH:
			assert(mNumOperands == 1);
			fprintf(file, "BRANCH");
			break;
		case IC_JUMP:
			assert(mNumOperands == 0);
			fprintf(file, "JUMP");
			break;
		case IC_JUMPF:
			assert(mNumOperands == 0);
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
		case IC_DISPATCH:
			fprintf(file, "DISPATCH");
			break;
		case IC_RETURN_VALUE:
			assert(mNumOperands == 1);
			fprintf(file, "RETV");
			break;
		case IC_RETURN_STRUCT:
			assert(mNumOperands == 1);
			fprintf(file, "RETS");
			break;
		case IC_RETURN:
			fprintf(file, "RET");
			break;
		case IC_UNREACHABLE:
			fprintf(file, "UNREACHABLE");
			break;
		case IC_BREAKPOINT:
			fprintf(file, "BREAKPOINT");
			break;
		}
		static char typechars[] = "NBCILFP";

		fprintf(file, "\t");
		if (mDst.mTemp >= 0)
			mDst.Disassemble(file, proc);
		fprintf(file, "\t<-\t");


		if (this->mCode == IC_CONSTANT)
		{
			if (mConst.mType == IT_NONE)
				fprintf(file, "?");

			if (mDst.mType == IT_POINTER)
			{	
				const char* vname = "";
				bool		aliased = false;

				if (mConst.mMemory == IM_LOCAL || mConst.mMemoryBase == IM_LOCAL)
				{
					if (mConst.mVarIndex < 0 || !proc->mLocalVars[mConst.mVarIndex])
						vname = "null";
					else if (!proc->mLocalVars[mConst.mVarIndex]->mIdent)
						vname = "";
					else
						vname = proc->mLocalVars[mConst.mVarIndex]->mIdent->mString;
				}
				else if (mConst.mMemory == IM_PROCEDURE)
				{
					if (mConst.mVarIndex >= 0 && proc->mModule->mProcedures[mConst.mVarIndex])
						vname = proc->mModule->mProcedures[mConst.mVarIndex]->mIdent->mString;
					else if (mConst.mLinkerObject && mConst.mLinkerObject->mIdent)
						vname = mConst.mLinkerObject->mIdent->mString;
				}
				else if (mConst.mMemory == IM_GLOBAL)
				{
					if (mConst.mVarIndex < 0)
					{
						if (mConst.mLinkerObject && mConst.mLinkerObject->mIdent)
							vname = mConst.mLinkerObject->mIdent->mString;
						else
							vname = "";
					}
					else if (!proc->mModule->mGlobalVars[mConst.mVarIndex])
						vname = "null";
					else if (!proc->mModule->mGlobalVars[mConst.mVarIndex]->mIdent)
					{
						vname = "";
						aliased = proc->mModule->mGlobalVars[mConst.mVarIndex]->mAliased;
					}
					else
					{
						vname = proc->mModule->mGlobalVars[mConst.mVarIndex]->mIdent->mString;
						aliased = proc->mModule->mGlobalVars[mConst.mVarIndex]->mAliased;
					}
				}

				if (aliased)
					fprintf(file, "C%c%d(%d:%d '%s' A)", memchars[mConst.mMemory], mConst.mOperandSize, mConst.mVarIndex, int(mConst.mIntConst), vname);
				else
					fprintf(file, "C%c%d(%d:%d '%s')", memchars[mConst.mMemory], mConst.mOperandSize, mConst.mVarIndex, int(mConst.mIntConst), vname);
			}
			else if (mDst.mType == IT_FLOAT)
				fprintf(file, "CF:%f", mConst.mFloatConst);
			else
			{
				fprintf(file, "CI:%lld", mConst.mIntConst);
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
					mSrc[j].Disassemble(file, proc);
					first = false;
				}
			}
		}

		fprintf(file, "\t{");
		if (mInvariant)
			fprintf(file, "I");
		if (mVolatile)
			fprintf(file, "V");
		if (mMemmap)
			fprintf(file, "M");
		if (mNoSideEffects)
			fprintf(file, "E");
		if (mConstExpr)
			fprintf(file, "C");
		if (mSingleAssignment)
			fprintf(file, "S");
		if (mAliasing)
			fprintf(file, "A");
		fprintf(file, "}\n");
	}
}

InterCodeBasicBlock::InterCodeBasicBlock(InterCodeProcedure * proc)
	: mProc(proc),
	mInstructions(nullptr), mEntryRenameTable(-1), mExitRenameTable(-1), mMergeTValues(nullptr), mMergeAValues(nullptr), mTrueJump(nullptr), mFalseJump(nullptr), mLoopPrefix(nullptr), mDominator(nullptr),
	mLoadStoreInstructions(nullptr), mMemoryValueSize(0), mEntryMemoryValueSize(0)
{
	mVisited = false;
	mInPath = false;
	mLoopHead = false;
	mChecked = false;
	mTraceIndex = -1;
	mUnreachable = false;
	mValueRangeValid = false;

	mIndex = proc->mNumBlocks++;
	proc->mBlocks.Push(this);
}

InterCodeBasicBlock::~InterCodeBasicBlock(void)
{
}

InterCodeBasicBlock* InterCodeBasicBlock::Clone(void)
{
	InterCodeBasicBlock* nblock = new InterCodeBasicBlock(mProc);
	for (int i = 0; i < mInstructions.Size(); i++)
		nblock->mInstructions.Push(mInstructions[i]->Clone());
	return nblock;
}


void InterCodeBasicBlock::Append(InterInstruction * code)
{
#if _DEBUG
	if (code->mCode == IC_BINARY_OPERATOR)
	{
		assert(code->mSrc[1].mType != IT_POINTER);
	}
	if (code->mCode == IC_CONSTANT)
	{
		assert(code->mDst.mType == code->mConst.mType);
		if (code->mDst.mType == IT_INT8) assert(code->mConst.mIntConst >= -128 && code->mConst.mIntConst <= 255);
		if (code->mDst.mType == IT_INT16) assert(code->mConst.mIntConst >= -32768 && code->mConst.mIntConst <= 65535);
	}
	if (code->mCode == IC_CONSTANT && code->mConst.mType == IT_POINTER && code->mConst.mMemory == IM_GLOBAL && code->mConst.mVarIndex >= 0)
	{
		assert(code->mConst.mVarIndex < mProc->mModule->mGlobalVars.Size());
		assert(mProc->mModule->mGlobalVars[code->mConst.mVarIndex]);
	}
	if (code->mCode == IC_STORE)
	{
		assert(code->mSrc[1].mOperandSize > 0);
	}
	if (code->mCode == IC_LOAD_TEMPORARY)
	{
		assert(code->mSrc[0].mTemp >= 0);
	}
	if (code->mDst.mTemp >= 0)
		assert(code->mDst.mType != IT_NONE);
	for (int i = 0; i < code->mNumOperands; i++)
		assert(code->mSrc[i].mType != IT_NONE);

	assert(!(code->mInUse));
#endif
	code->mInUse = true;
	this->mInstructions.Push(code);
}


void InterCodeBasicBlock::AppendBeforeBranch(InterInstruction* code, bool loopindex)
{
	int ti = mInstructions.Size() - 1;
	if (mInstructions[ti]->mCode == IC_BRANCH)
	{
		if (ti > 0 && mInstructions[ti - 1]->mDst.mTemp == mInstructions[ti]->mSrc[0].mTemp && loopindex || CanBypassUp(code, mInstructions[ti - 1]))
		{
			ti--;
			if (ti > 0 && mInstructions[ti]->UsesTemp(mInstructions[ti - 1]->mDst.mTemp) && CanBypassUp(code, mInstructions[ti - 1]))
				ti--;
		}
	}

	mInstructions.Insert(ti, code);
}

const InterInstruction* InterCodeBasicBlock::FindByDst(int dst, int depth) const
{
	int n = mInstructions.Size() - 1;
	while (n >= 0 && mInstructions[n]->mDst.mTemp != dst)
		n--;
	if (n >= 0)
		return mInstructions[n];
	else if (depth < 10 && mEntryBlocks.Size() == 1)
		return mEntryBlocks[0]->FindByDst(dst, depth + 1);
	else
		return nullptr;
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

bool InterCodeBasicBlock::IsDominator(InterCodeBasicBlock* block)
{
	while (block)
	{
		if (block == this)
			return true;
		block = block->mDominator;
	}
	return false;
}

void InterCodeBasicBlock::CollectEntries(void)
{
	if (mInPath)
	{
		mLoopDebug = true;
		mLoopHead = true;
	}
	mNumEntries++;
	if (!mVisited)
	{
		mVisited = true;

		mInPath = true;
		if (mTrueJump) mTrueJump->CollectEntries();
		if (mFalseJump) mFalseJump->CollectEntries();
		mInPath = false;
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

bool InterCodeBasicBlock::StripLoopHead(int size)
{
	bool	changed = false;

	if (!mVisited)
	{
		if (mLoopHead && mFalseJump && mTrueJump != this && mFalseJump != this && mLoopPrefix && mInstructions.Size() < size)
		{
//			printf("StripA %s %d\n", mProc->mIdent->mString, mIndex);

			ExpandingArray<InterCodeBasicBlock*>	lblocks;
			if (CollectSingleEntryGenericLoop(lblocks))
			{
				if (!lblocks.Contains(mTrueJump) || !lblocks.Contains(mFalseJump))
				{
					//				printf("StripB %s %d\n", mProc->mIdent->mString, mIndex);

					mLoopPrefix->mInstructions.SetSize(0);
					for (int i = 0; i < mInstructions.Size(); i++)
						mLoopPrefix->mInstructions.Push(mInstructions[i]->Clone());

					mLoopPrefix->mFalseJump = mFalseJump;
					mLoopPrefix->mTrueJump = mTrueJump;

					mEntryBlocks.RemoveAll(mLoopPrefix);
					mNumEntries--;
					mLoopHead = false;

					mTrueJump->mEntryBlocks.Push(mLoopPrefix);
					mTrueJump->mNumEntries++;
					mFalseJump->mEntryBlocks.Push(mLoopPrefix);
					mFalseJump->mNumEntries++;

					if (!lblocks.Contains(mTrueJump))
					{
						mFalseJump->mLoopHead = true;
					}
					else if (!lblocks.Contains(mFalseJump))
					{
						mTrueJump->mLoopHead = true;
					}

					changed = true;
				}
			}
		}

		mVisited = true;

		if (mTrueJump && mTrueJump->StripLoopHead(size)) 
			changed = true;
		if (mFalseJump && mFalseJump->StripLoopHead(size))
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::GenerateTraces(int expand, bool compact)
{
	if (mInPath)
		mLoopHead = true;

	assert(mIndex != 0 || !mLoopHead);

	if (!mVisited)
	{
		mVisited = true;
		mInPath = true;

		// Limit number of contractions
		for (int i = 0; i < 100; i++)
		{
			int sz = mInstructions.Size();

			if (mFalseJump && sz > 0 && mInstructions[sz - 1]->mCode == IC_BRANCH && (mInstructions[sz - 1]->mSrc[0].mType == IT_BOOL || IsIntegerType(mInstructions[sz - 1]->mSrc[0].mType)) && mInstructions[sz - 1]->mSrc[0].mTemp < 0)
			{
				mInstructions[sz - 1]->mCode = IC_JUMP;
				mInstructions[sz - 1]->mNumOperands = 0;
				if (!mInstructions[sz - 1]->mSrc[0].mIntConst)
				{
					mTrueJump->mNumEntries--;
					mTrueJump = mFalseJump;
				}
				else
					mFalseJump->mNumEntries--;
				mFalseJump = nullptr;
			}
#if 1
			if (mFalseJump && sz > 0 && mInstructions[sz - 1]->mCode == IC_BRANCH && mTrueJump && mTrueJump == mFalseJump)
			{
				mInstructions[sz - 1]->mCode = IC_JUMP;
				mInstructions[sz - 1]->mNumOperands = 0;
				mFalseJump->mNumEntries--;
				mFalseJump = nullptr;
			}
#endif
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
			else if (
				mTrueJump &&
				mInstructions.Size() > 0 &&
				mInstructions.Last()->mCode == IC_BRANCH &&
				mTrueJump->mInstructions.Size() == 1 &&
				mTrueJump->mInstructions[0]->mCode == IC_BRANCH &&
				mTrueJump->mInstructions[0]->mSrc[0].mTemp == mInstructions.Last()->mSrc[0].mTemp)
			{
				mTrueJump->mNumEntries--;
				mTrueJump = mTrueJump->mTrueJump;
				if (mTrueJump)
					mTrueJump->mNumEntries++;
			}
			else if (
				mFalseJump &&
				mInstructions.Size() > 0 &&
				mInstructions.Last()->mCode == IC_BRANCH &&
				mFalseJump->mInstructions.Size() == 1 &&
				mFalseJump->mInstructions[0]->mCode == IC_BRANCH &&
				mFalseJump->mInstructions[0]->mSrc[0].mTemp == mInstructions.Last()->mSrc[0].mTemp)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = mFalseJump->mFalseJump;
				if (mFalseJump)
					mFalseJump->mNumEntries++;
			}
			else if (
				compact && 
				mFalseJump &&
				mInstructions.Size() > 0 &&
				mInstructions.Last()->mCode == IC_BRANCH &&
				mInstructions.Last()->mSrc[0].mTemp < 0)
			{
				int	ns = mInstructions.Size();

				if (mInstructions.Last()->mSrc[0].mIntConst)
					mFalseJump->mNumEntries--;
				else
				{
					mTrueJump->mNumEntries--;
					mTrueJump = mFalseJump;
				}

				mFalseJump = nullptr;
				mInstructions[ns - 1]->mCode = IC_JUMP;
				mInstructions[ns - 1]->mNumOperands = 0;
			}
			else if (mTrueJump && !mFalseJump && ((mTrueJump->mInstructions.Size() < expand && mTrueJump->mInstructions.Size() > 1 && !mLoopHead) || mTrueJump->mNumEntries == 1) && !mTrueJump->mLoopHead && !IsInfiniteLoop(mTrueJump, mTrueJump))
			{
//				if (mLoopDebug)
//					printf("StripC %s %d %d\n", mProc->mIdent->mString, mTrueJump->mIndex, mTrueJump->mInstructions.Size());

				mTrueJump->mNumEntries--;
				int	n = mTrueJump->mNumEntries;

				mInstructions.Pop();
				for (i = 0; i < mTrueJump->mInstructions.Size(); i++)
					mInstructions.Push(mTrueJump->mInstructions[i]->Clone());

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

bool InterCodeBasicBlock::MergeSameConditionTraces(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump && !mTrueJump->mFalseJump && !mFalseJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump && mTrueJump->mNumEntries == 1 && mFalseJump->mNumEntries == 1)
		{
			// we have a diamond situation
			InterCodeBasicBlock* mb0 = mTrueJump->mTrueJump;
			if (mb0 && mb0->mNumEntries == 2)
			{
				if (mb0->mTrueJump && mb0->mFalseJump && !mb0->mTrueJump->mFalseJump && !mb0->mFalseJump->mFalseJump && mb0->mTrueJump->mTrueJump == mb0->mFalseJump->mTrueJump && mb0->mTrueJump->mNumEntries == 1 && mb0->mFalseJump->mNumEntries == 1)
				{
					// we have a dual diamond
					InterCodeBasicBlock* mb1 = mb0->mTrueJump->mTrueJump;
					if (mb1 && mb1->mNumEntries == 2 && mb0 != mb1)
					{
						int	tc = mInstructions.Last()->mSrc[0].mTemp;
						if (tc >= 0 && tc == mb0->mInstructions.Last()->mSrc[0].mTemp)
						{
							if (!mTrueJump->mLocalModifiedTemps[tc] && !mFalseJump->mLocalModifiedTemps[tc] && !mb0->mLocalModifiedTemps[tc])
							{
								// Same conditions in both diamonds
								if (mb0->mInstructions.Size() < 8)
								{
									// Join blocks

									mTrueJump->mInstructions.Remove(mTrueJump->mInstructions.Size() - 1);
									mFalseJump->mInstructions.Remove(mFalseJump->mInstructions.Size() - 1);

									for (int i = 0; i + 1 < mb0->mInstructions.Size(); i++)
									{
										mTrueJump->mInstructions.Push(mb0->mInstructions[i]->Clone());
										mFalseJump->mInstructions.Push(mb0->mInstructions[i]->Clone());
									}

									for(int i=0; i<mb0->mTrueJump->mInstructions.Size(); i++)
										mTrueJump->mInstructions.Push(mb0->mTrueJump->mInstructions[i]->Clone());
									for (int i = 0; i < mb0->mFalseJump->mInstructions.Size(); i++)
										mFalseJump->mInstructions.Push(mb0->mFalseJump->mInstructions[i]->Clone());

									mTrueJump->mLocalModifiedTemps |= mb0->mLocalModifiedTemps;
									mFalseJump->mLocalModifiedTemps |= mb0->mLocalModifiedTemps;
									mTrueJump->mLocalModifiedTemps |= mb0->mTrueJump->mLocalModifiedTemps;
									mFalseJump->mLocalModifiedTemps |= mb0->mFalseJump->mLocalModifiedTemps;

									mTrueJump->mTrueJump = mb1;
									mFalseJump->mTrueJump = mb1;

									changed = true;
								}
							}
						}
					}

				}
			}
		}

		if (mTrueJump && mTrueJump->MergeSameConditionTraces())
			changed = true;
		if (mFalseJump && mFalseJump->MergeSameConditionTraces())
			changed = true;
	}

	return changed;
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
			ins->mSrc[offset].mFinal = false;
			ains->mSrc[1].mFinal = false;
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


static bool ispow2(int64 v)
{
	if (v > 0)
	{
		while (!(v & 1))
			v >>= 1;
		return v == 1;
	}

	return 0;
}

static int binlog(int64 n)
{
	int	k = -1;

	while (n)
	{
		n >>= 1;
		k++;
	}

	return k;
}


void InterCodeBasicBlock::CheckValueUsage(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs, FastNumberSet& fsingle)
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
				ins->mNumOperands = 0;
				break;
			case IT_POINTER:
				ins->mCode = IC_CONSTANT;
				ins->mConst.mMemory = tvalue[ins->mSrc[0].mTemp]->mConst.mMemory;
				ins->mConst.mLinkerObject = tvalue[ins->mSrc[0].mTemp]->mConst.mLinkerObject;
				ins->mConst.mVarIndex = tvalue[ins->mSrc[0].mTemp]->mConst.mVarIndex;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mConst.mOperandSize = tvalue[ins->mSrc[0].mTemp]->mConst.mOperandSize;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
				break;
			default:
				ins->mCode = IC_CONSTANT;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
				break;
			}
			ins->mConst.mType = ins->mDst.mType;
		}

		break;

	case IC_LOAD:
		OptimizeAddress(ins, tvalue, 0);

		if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL && (ins->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
			LoadConstantFold(ins, nullptr, staticVars, staticProcs);

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
	case IC_FILL:
		if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
		{
			ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
			ins->mSrc[0].mTemp = -1;
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
				ins->mConst.mType = IT_POINTER;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;
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
#if 1
			else if (ins->mSrc[1].mTemp >= 0 && fsingle[ins->mSrc[1].mTemp])
			{
				InterInstruction* lins = tvalue[ins->mSrc[1].mTemp];
				while (lins && lins->mCode == IC_LEA && lins->mSrc[1].mTemp >= 0 && fsingle[lins->mSrc[1].mTemp])
					lins = tvalue[lins->mSrc[1].mTemp];
				if (lins && lins->mSrc[1].mTemp < 0 && (lins->mSrc[1].mMemory == IM_ABSOLUTE || lins->mSrc[1].mMemory == IM_GLOBAL))
				{
					lins->mSrc[1].mIntConst += tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0].mType = ins->mSrc[1].mType;
					ins->mSrc[0].mTemp = ins->mSrc[1].mTemp;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 1;
				}
				else
				{
					ins->mSrc[0].mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
					ins->mSrc[0].mTemp = -1;
				}
			}
#endif
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
						ins->mSrc[0].mRange.AddConstValue(IT_INT16, -iins->mSrc[1].mIntConst);
						ins->mSrc[1].mIntConst += iins->mSrc[1].mIntConst;
					}
					else if (iins->mSrc[0].mTemp < 0 && iins->mSrc[1].mTemp >= 0)
					{
						ins->mSrc[0].mTemp = iins->mSrc[1].mTemp;
						ins->mSrc[0].mRange.AddConstValue(IT_INT16, -iins->mSrc[0].mIntConst);
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
						ins->mSrc[0].mRange.AddConstValue(IT_INT16, iins->mSrc[0].mIntConst);
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
				ins->mConst.mType = IT_POINTER;
				ins->mConst.mMemory = IM_ABSOLUTE;
				ins->mConst.mVarIndex = 0;
				ins->mConst.mIntConst = tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
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
					ins->mConst.mType = IT_INT16;
					ins->mConst.mIntConst = cins->mConst.mIntConst;
					ins->mSrc[0].mTemp = -1;
					ins->mNumOperands = 0;
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
		for (int i = 0; i < 3; i++)
		{
			if (ins->mSrc[i].mTemp >= 0 && tvalue[ins->mSrc[i].mTemp] && tvalue[ins->mSrc[i].mTemp]->mCode == IC_CONSTANT && ins->mSrc[i].mType != IT_POINTER)
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
					ins->mConst.mType = IT_FLOAT;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;
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
							ins->mConst.mType = IT_FLOAT;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[1].mTemp = -1;
							ins->mNumOperands = 0;
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
						ins->mConst.mType = IT_FLOAT;
						ins->mSrc[0].mTemp = -1;
						ins->mSrc[1].mTemp = -1;
						ins->mNumOperands = 0;
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
					ins->mConst.mIntConst = LimitIntConstValue(ins->mDst.mType, ConstantFolding(ins->mOperator, ins->mDst.mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst));
					ins->mConst.mType = ins->mDst.mType;
					ins->mSrc[0].mTemp = -1;
					ins->mSrc[1].mTemp = -1;
					ins->mNumOperands = 0;
				}
				else
				{
					ins->mSrc[1].mIntConst = LimitIntConstValue(ins->mSrc[1].mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst);
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
						else if (ins->mSrc[0].mType == IT_INT32 && ispow2(ins->mSrc[1].mIntConst))
						{
							int64 s = ins->mSrc[1].mIntConst;
							ins->mOperator = IA_SHL;
							ins->mSrc[1].mTemp = ins->mSrc[0].mTemp;
							ins->mSrc[1].mType = ins->mSrc[0].mType;
							ins->mSrc[0].mTemp = -1;
							ins->mSrc[0].mIntConst = 0;
							while (s > 1)
							{
								ins->mSrc[0].mIntConst++;
								s >>= 1;
							}							
						}

					}
#endif
				}
			}
			else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
			{
				ins->mSrc[0].mIntConst = LimitIntConstValue(ins->mSrc[0].mType, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
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
					else if (ins->mSrc[1].mType == IT_INT32 && ispow2(ins->mSrc[0].mIntConst))
					{
						int64 s = ins->mSrc[0].mIntConst;
						ins->mOperator = IA_SHL;
						ins->mSrc[0].mIntConst = 0;
						while (s > 1)
						{
							ins->mSrc[0].mIntConst++;
							s >>= 1;
						}
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
					ins->mNumOperands = 0;
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
				ins->mConst.mType = IT_FLOAT;
				ins->mSrc[0].mTemp = -1;
				ins->mNumOperands = 0;
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
				ins->mConst.mType = IT_BOOL;
				ins->mDst.mType = IT_BOOL;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;
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
				ins->mConst.mType = ins->mDst.mType;
				ins->mSrc[0].mTemp = -1;
				ins->mSrc[1].mTemp = -1;
				ins->mNumOperands = 0;
			}
			else
			{
				if (ins->mSrc[1].mTemp >= 0 && tvalue[ins->mSrc[1].mTemp] && tvalue[ins->mSrc[1].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[1].mIntConst = LimitIntConstValue(ins->mSrc[1].mType, tvalue[ins->mSrc[1].mTemp]->mConst.mIntConst);
					ins->mSrc[1].mTemp = -1;
				}
				else if (ins->mSrc[0].mTemp >= 0 && tvalue[ins->mSrc[0].mTemp] && tvalue[ins->mSrc[0].mTemp]->mCode == IC_CONSTANT)
				{
					ins->mSrc[0].mIntConst = LimitIntConstValue(ins->mSrc[0].mType, tvalue[ins->mSrc[0].mTemp]->mConst.mIntConst);
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
			ins->mSrc[0].mMemory = tvalue[ins->mSrc[0].mTemp]->mConst.mMemory;
			ins->mSrc[0].mTemp = -1;
		}
		break;
	}
}


void InterCodeBasicBlock::CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable, int& nlocals, int& nparams)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
			mInstructions[i]->CollectLocalAddressTemps(localTable, paramTable, nlocals, nparams);

		if (mTrueJump) mTrueJump->CollectLocalAddressTemps(localTable, paramTable, nlocals, nparams);
		if (mFalseJump) mFalseJump->CollectLocalAddressTemps(localTable, paramTable, nlocals, nparams);
	}
}

void InterCodeBasicBlock::RecheckLocalAliased(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterVariable* v = nullptr;

			InterInstruction* ins = mInstructions[i];
			if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mTemp < 0)
				{
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						v = mProc->mLocalVars[ins->mSrc[1].mVarIndex];
					else if (ins->mSrc[1].mMemory == IM_PARAM || ins->mSrc[1].mMemory == IM_FPARAM)
						v = mProc->mParamVars[ins->mSrc[1].mVarIndex];
				}
			}
			else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER)
			{
				if (ins->mConst.mMemory == IM_LOCAL)
					v = mProc->mLocalVars[ins->mConst.mVarIndex];
				else if (ins->mConst.mMemory == IM_PARAM || ins->mConst.mMemory == IM_FPARAM)
					v = mProc->mParamVars[ins->mConst.mVarIndex];
			}

			if (v)
			{
				if (!v->mNotAliased)
					v->mAliased = true;
			}
		}

		if (mTrueJump) mTrueJump->RecheckLocalAliased();
		if (mFalseJump) mFalseJump->RecheckLocalAliased();
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
				else
					ltemps[ttemp] = nullptr;

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

bool InterCodeBasicBlock::PropagateVariableCopy(const GrowingInstructionPtrArray& ctemps, const GrowingVariableArray& staticVars, const NumberSet& aliasedLocals, const NumberSet& aliasedParams)
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
						if (ltemps[k]->mSrc[0].mStride != ltemps[k]->mSrc[1].mStride)
							ins->mSrc[0].mIntConst = (ins->mSrc[0].mIntConst - ltemps[k]->mSrc[1].mIntConst) / ltemps[k]->mSrc[1].mStride * ltemps[k]->mSrc[0].mStride + ltemps[k]->mSrc[0].mIntConst;
						else 
							ins->mSrc[0].mIntConst = ins->mSrc[0].mIntConst - ltemps[k]->mSrc[1].mIntConst + ltemps[k]->mSrc[0].mIntConst;
						ins->mSrc[0].mLinkerObject = ltemps[k]->mSrc[0].mLinkerObject;
						ins->mSrc[0].mStride = ltemps[k]->mSrc[0].mStride;
						changed = true;
					}
				}

				break;

			case IC_STORE:

				j = 0;
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (!CollidingMem(ltemps[k], ins))
					{
						ltemps[j++] = ltemps[k];
					}
				}
				ltemps.SetSize(j);
				break;

			case IC_FILL:
				j = 0;
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (!CollidingMem(ltemps[k], ins))
					{
						ltemps[j++] = ltemps[k];
					}
				}
				ltemps.SetSize(j);
				break;


			case IC_COPY:
				if (!ins->mVolatile)
				{
					for (int k = 0; k < ltemps.Size(); k++)
					{
						if (SameMemAndSize(ltemps[k]->mSrc[1], ins->mSrc[0]))
						{
							ins->mSrc[0] = ltemps[k]->mSrc[0];
							changed = true;
						}
					}
				}

				j = 0;
				for (int k = 0; k < ltemps.Size(); k++)
				{
					if (!CollidingMem(ltemps[k], ins))
					{
						ltemps[j++] = ltemps[k];
					}
				}
				if (!ins->mVolatile)
				{
					ltemps.SetSize(j);
					ltemps.Push(ins);
				}
				break;

			case IC_CALL:
			case IC_CALL_NATIVE:
				if (!ins->mNoSideEffects)
				{
					j = 0;
					for (int k = 0; k < ltemps.Size(); k++)
					{
						if (ltemps[k]->mSrc[0].mTemp < 0 && ltemps[k]->mSrc[1].mTemp < 0 &&
							(ltemps[k]->mSrc[0].mMemory == IM_LOCAL && !aliasedLocals[ltemps[k]->mSrc[0].mVarIndex] ||
							 (ltemps[k]->mSrc[0].mMemory == IM_PARAM || ltemps[k]->mSrc[0].mMemory == IM_FPARAM) && !aliasedParams[ltemps[k]->mSrc[0].mVarIndex]) &&
							(ltemps[k]->mSrc[1].mMemory == IM_LOCAL && !aliasedLocals[ltemps[k]->mSrc[1].mVarIndex] ||
							 (ltemps[k]->mSrc[1].mMemory == IM_PARAM || ltemps[k]->mSrc[1].mMemory == IM_FPARAM) && !aliasedParams[ltemps[k]->mSrc[1].mVarIndex]))
						{
							ltemps[j++] = ltemps[k];
						}
					}
					ltemps.SetSize(j);
				}
				break;
			}

		}

		if (mTrueJump && mTrueJump->PropagateVariableCopy(ltemps, staticVars, aliasedLocals, aliasedParams))
			changed = true;
		if (mFalseJump && mFalseJump->PropagateVariableCopy(ltemps, staticVars, aliasedLocals, aliasedParams))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::EarlyBranchElimination(const GrowingInstructionPtrArray& ctemps)
{
	bool	changed = false;

	int i;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	temps(ctemps);
		if (mNumEntries > 1)
			temps.Clear();

		int sz = mInstructions.Size();
		for (i = 0; i < sz; i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mDst.mTemp >= 0)
			{
				if (ins->mCode == IC_UNARY_OPERATOR && temps[ins->mSrc[0].mTemp])
				{
					ins->mConst = OperandConstantFolding(ins->mOperator, temps[ins->mSrc[0].mTemp]->mConst, temps[ins->mSrc[0].mTemp]->mConst);
					ins->mCode = IC_CONSTANT;
					ins->mNumOperands = 0;
					ins->mSrc[0].mTemp = -1;
				}
				else if (ins->mCode == IC_BINARY_OPERATOR && temps[ins->mSrc[0].mTemp] && temps[ins->mSrc[1].mTemp])
				{
					ins->mConst = OperandConstantFolding(ins->mOperator, temps[ins->mSrc[1].mTemp]->mConst, temps[ins->mSrc[0].mTemp]->mConst);
					ins->mCode = IC_CONSTANT;
					ins->mNumOperands = 0;
					ins->mSrc[0].mTemp = -1;
				}
				else if (ins->mCode == IC_RELATIONAL_OPERATOR && temps[ins->mSrc[0].mTemp] && temps[ins->mSrc[1].mTemp])
				{
					ins->mConst = OperandConstantFolding(ins->mOperator, temps[ins->mSrc[1].mTemp]->mConst, temps[ins->mSrc[0].mTemp]->mConst);
					ins->mCode = IC_CONSTANT;
					ins->mNumOperands = 0;
					ins->mSrc[0].mTemp = -1;
				}

				if (ins->mCode == IC_CONSTANT)
					temps[mInstructions[i]->mDst.mTemp] = ins;
				else
					temps[mInstructions[i]->mDst.mTemp] = nullptr;
			}
		}

		if (sz > 0 && mInstructions[sz - 1]->mCode == IC_BRANCH && temps[mInstructions[sz - 1]->mSrc[0].mTemp])
		{
			InterInstruction* cins = temps[mInstructions[sz - 1]->mSrc[0].mTemp];

			if (cins->mConst.mType == IT_BOOL)
			{
				mInstructions[sz - 1]->mCode = IC_JUMP;
				mInstructions[sz - 1]->mNumOperands = 0;

				if (cins->mConst.mIntConst)
					mFalseJump->mNumEntries--;
				else
				{
					mTrueJump->mNumEntries--;
					mTrueJump = mFalseJump;
				}
				mFalseJump = nullptr;
				changed = true;
			}
		}

		if (mTrueJump && mTrueJump->EarlyBranchElimination(temps))
			changed = true;
		if (mFalseJump && mFalseJump->EarlyBranchElimination(temps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::PropagateNonNullPointers(const NumberSet& vtemps)
{
	bool	changed = false;

	int i;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	ltemps(vtemps);

		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
			{
				for (i = 0; i < mInstructions.Size(); i++)
					if (mInstructions[i]->mDst.mTemp >= 0)
						ltemps -= mInstructions[i]->mDst.mTemp;
			}
			else
				ltemps.Clear();
		}
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				for (int i = 0; i < ltemps.Size(); i++)
				{
					if (!mMergeTemps[i])
						ltemps -= i;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTemps = ltemps;
				return false;
			}
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mDst.mTemp >= 0)
			{
				if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER)
				{
					if (ins->mConst.mMemory != IM_ABSOLUTE || ins->mConst.mIntConst != 0)
						ltemps += ins->mDst.mTemp;
					else
						ltemps -= ins->mDst.mTemp;
				}
				else
				{
					ltemps -= ins->mDst.mTemp;
					if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp >= 0)
						ltemps += ins->mSrc[0].mTemp;
					else if (ins->mCode == IC_RELATIONAL_OPERATOR)
					{
						if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mType == IT_POINTER && ins->mSrc[0].mMemory == IM_ABSOLUTE && ins->mSrc[0].mIntConst == 0 && ins->mSrc[1].mTemp >= 0 && ltemps[ins->mSrc[1].mTemp] ||
							ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mType == IT_POINTER && ins->mSrc[1].mMemory == IM_ABSOLUTE && ins->mSrc[1].mIntConst == 0 && ins->mSrc[0].mTemp >= 0 && ltemps[ins->mSrc[0].mTemp])
						{
							if (ins->mOperator == IA_CMPEQ)
							{
								ins->mCode = IC_CONSTANT;
								ins->mConst.mType = IT_BOOL;
								ins->mConst.mIntConst = 0;
								ins->mNumOperands = 0;
								changed = true;
							}
							else if (ins->mOperator == IA_CMPNE)
							{
								ins->mCode = IC_CONSTANT;
								ins->mConst.mType = IT_BOOL;
								ins->mConst.mIntConst = 1;
								ins->mNumOperands = 0;
								changed = true;
							}
						}
					}
				}
			}
			else if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp >= 0)
				ltemps += ins->mSrc[1].mTemp;
		}

		if (mTrueJump && mTrueJump->PropagateNonNullPointers(ltemps))
			changed = true;
		if (mFalseJump && mFalseJump->PropagateNonNullPointers(ltemps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::PropagateConstCompareResults(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int sz = mInstructions.Size();
		if (sz >= 2 && mFalseJump &&
			mInstructions[sz - 1]->mCode == IC_BRANCH &&
			mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && (mInstructions[sz - 2]->mOperator == IA_CMPEQ || mInstructions[sz - 2]->mOperator == IA_CMPNE) &&
			mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp)
		{
			InterCodeBasicBlock* cblock = mTrueJump;
			if (mInstructions[sz - 2]->mOperator == IA_CMPNE)
				cblock = mFalseJump;

			if (cblock->mNumEntries == 1)
			{
				if (mInstructions[sz - 2]->mSrc[1].mTemp < 0 && mInstructions[sz - 2]->mSrc[0].mTemp >= 0 && cblock->mEntryRequiredTemps[mInstructions[sz - 2]->mSrc[0].mTemp] && mInstructions[sz - 2]->mSrc[0].mTemp != mInstructions[sz - 2]->mDst.mTemp)
				{
					InterInstruction* cins = new InterInstruction(mInstructions[sz - 2]->mLocation, IC_CONSTANT);
					cins->mDst = mInstructions[sz - 2]->mSrc[0];
					cins->mConst = mInstructions[sz - 2]->mSrc[1];
					cblock->mInstructions.Insert(0, cins);
					changed = true;						
				}
				else if (mInstructions[sz - 2]->mSrc[0].mTemp < 0 && mInstructions[sz - 2]->mSrc[1].mTemp >= 0 && cblock->mEntryRequiredTemps[mInstructions[sz - 2]->mSrc[1].mTemp] && mInstructions[sz - 2]->mSrc[1].mTemp != mInstructions[sz - 2]->mDst.mTemp)
				{
					InterInstruction* cins = new InterInstruction(mInstructions[sz - 2]->mLocation, IC_CONSTANT);
					cins->mDst = mInstructions[sz - 2]->mSrc[1];
					cins->mConst = mInstructions[sz - 2]->mSrc[0];
					cblock->mInstructions.Insert(0, cins);
					changed = true;
				}
			}
		}

		if (mTrueJump && mTrueJump->PropagateConstCompareResults())
			changed = true;
		if (mFalseJump && mFalseJump->PropagateConstCompareResults())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::ForwardConstTemps(const GrowingInstructionPtrArray& ctemps)
{
	bool	changed = false;

	int i;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	ltemps(ctemps);

		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
			{
				for (i = 0; i < mInstructions.Size(); i++)
					if (mInstructions[i]->mDst.mTemp >= 0)
						ltemps[mInstructions[i]->mDst.mTemp] = nullptr;
			}
			else
				ltemps.Clear();
		}
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				for (int i = 0; i < ltemps.Size(); i++)
				{
					if (mMergeTValues[i] != ltemps[i])
						ltemps[i] = nullptr;
				}
			}

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mMergeTValues = ltemps;
				return false;
			}
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->PropagateConstTemps(ltemps))
				changed = true;
			if (mInstructions[i]->mDst.mTemp >= 0)
			{
				if (mInstructions[i]->mCode == IC_CONSTANT)
					ltemps[mInstructions[i]->mDst.mTemp] = mInstructions[i];
				else
					ltemps[mInstructions[i]->mDst.mTemp] = nullptr;
			}
		}

		if (mTrueJump && mTrueJump->ForwardConstTemps(ltemps))
			changed = true;
		if (mFalseJump && mFalseJump->ForwardConstTemps(ltemps))
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

bool InterCodeBasicBlock::CombineIndirectAddressing(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	tvalue(nullptr);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* lins = mInstructions[i];
			InterInstruction* tins = nullptr;

			if (lins->mCode == IC_LEA && lins->mSrc[0].mTemp >= 0 && lins->mSrc[1].mTemp < 0 &&
				!lins->mSrc[0].IsUByte() &&
				(lins->mSrc[1].mMemory == IM_ABSOLUTE || lins->mSrc[1].mMemory == IM_GLOBAL || lins->mSrc[1].mMemory == IM_LOCAL))
			{
				int j = 0;
				while (j < tvalue.Size() &&
					!(tvalue[j]->mSrc[0].mTemp == lins->mSrc[0].mTemp &&
						tvalue[j]->mSrc[1].mTemp < 0 &&
						tvalue[j]->mSrc[1].mMemory == lins->mSrc[1].mMemory &&
						tvalue[j]->mSrc[1].mVarIndex == lins->mSrc[1].mVarIndex &&
						tvalue[j]->mSrc[1].mIntConst <= lins->mSrc[1].mIntConst &&
						tvalue[j]->mSrc[1].mIntConst + 256 > lins->mSrc[1].mIntConst))
					j++;

				if (j < tvalue.Size())
				{
					int64	offset = lins->mSrc[1].mIntConst - tvalue[j]->mSrc[1].mIntConst;
					lins->mSrc[1] = tvalue[j]->mDst;
					lins->mSrc[0].mTemp = -1;
					lins->mSrc[0].mIntConst = offset;
					changed = true;
				}
				else
					tins = lins;
			}

			if (HasSideEffect(lins->mCode))
				tvalue.SetSize(0);
			else if (lins->mDst.mTemp >= 0)
			{
				int j = 0;
				while (j < tvalue.Size())
				{
					if (tvalue[j]->ReferencesTemp(lins->mDst.mTemp))
						tvalue.Remove(j);
					else
						j++;
				}
			}

			if (tins)
				tvalue.Push(tins);
		}

		if (mTrueJump && mTrueJump->CombineIndirectAddressing())
			changed = true;
		if (mFalseJump && mFalseJump->CombineIndirectAddressing())
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
		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && 
			mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp &&
			IsScalarType(mInstructions[sz - 2]->mSrc[0].mType) && IsScalarType(mInstructions[sz - 2]->mSrc[1].mType))
		{
			InterInstruction* cins = mInstructions[sz - 2];

			bool	constFalse = false, constTrue = false;

			if ((cins->mSrc[1].mTemp < 0 || (cins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND && cins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND)) &&
				(cins->mSrc[0].mTemp < 0 || (cins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && cins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)))
			{
				bool	signedvalid = cins->mSrc[0].mRange.mMaxValue <= SignedTypeMax(cins->mSrc[0].mType) && 
									  cins->mSrc[1].mRange.mMaxValue <= SignedTypeMax(cins->mSrc[1].mType);

				switch (cins->mOperator)
				{
				case IA_CMPEQ:
					if (cins->mSrc[0].mType == IT_INT8 && cins->mSrc[1].mType == IT_INT8 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 256 + cins->mSrc[0].mRange.mMinValue ||
 						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 256 + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[0].mType == IT_INT16 && cins->mSrc[1].mType == IT_INT16 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 65536 + cins->mSrc[0].mRange.mMinValue ||
						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 65536 + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[0].mType == IT_INT32 && cins->mSrc[1].mType == IT_INT32 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 0x100000000ll + cins->mSrc[0].mRange.mMinValue ||
						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 0x100000000ll + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue || cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constFalse = true;
					break;
				case IA_CMPNE:
					if (cins->mSrc[0].mType == IT_INT8 && cins->mSrc[1].mType == IT_INT8 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 256 + cins->mSrc[0].mRange.mMinValue ||
						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 256 + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[0].mType == IT_INT16 && cins->mSrc[1].mType == IT_INT16 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 65536 + cins->mSrc[0].mRange.mMinValue ||
						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 65536 + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[0].mType == IT_INT32 && cins->mSrc[1].mType == IT_INT32 &&
						(cins->mSrc[0].mRange.mMinValue < 0 && cins->mSrc[1].mRange.mMaxValue >= 0x100000000ll + cins->mSrc[0].mRange.mMinValue ||
						 cins->mSrc[1].mRange.mMinValue < 0 && cins->mSrc[0].mRange.mMaxValue >= 0x100000000ll + cins->mSrc[1].mRange.mMinValue))
						;
					else if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue || cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
						constTrue = true;
					break;
				case IA_CMPLS:
					if (signedvalid)
					{
						if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPLU:
					if (cins->mSrc[0].mTemp < 0 && cins->mSrc[0].mIntConst == 0)
					{
						constFalse = true;
					}
					else if (cins->mSrc[1].IsPositive() && cins->mSrc[0].IsPositive())
					{
						if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPLES:
					if (signedvalid)
					{
						if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPLEU:
					if (cins->mSrc[1].IsPositive() && cins->mSrc[0].IsPositive())
					{
						if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constFalse = true;
					}
					break;
				case IA_CMPGS:
					if (signedvalid)
					{
						if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constFalse = true;
					}
					break;
				case IA_CMPGU:
					if (cins->mSrc[1].mTemp < 0 && cins->mSrc[1].mIntConst == 0)
					{
						constFalse = true;
					}
					else if (cins->mSrc[1].IsPositive() && cins->mSrc[1].mRange.mMaxValue == 0)
					{
						constFalse = true;
					}
					else if (cins->mSrc[1].IsPositive() && cins->mSrc[0].IsPositive())
					{
						if (cins->mSrc[1].mRange.mMinValue > cins->mSrc[0].mRange.mMaxValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMaxValue <= cins->mSrc[0].mRange.mMinValue)
							constFalse = true;
					}
					break;
				case IA_CMPGES:
					if (signedvalid)
					{
						if (cins->mSrc[1].mRange.mMinValue >= cins->mSrc[0].mRange.mMaxValue)
							constTrue = true;
						else if (cins->mSrc[1].mRange.mMaxValue < cins->mSrc[0].mRange.mMinValue)
							constFalse = true;
					}
					break;
				case IA_CMPGEU:
					if (cins->mSrc[1].IsPositive() && cins->mSrc[0].IsPositive())
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
				mFalseJump->WarnUnreachable();
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
			else
			{
				mTrueJump->mNumEntries--;
				mTrueJump->WarnUnreachable();
				mTrueJump = mFalseJump;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
		}

		if (sz >= 1 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 1]->mSrc[0].mRange.IsBound() && mInstructions[sz - 1]->mSrc[0].mRange.mMinValue > 0 && mInstructions[sz - 1]->mSrc[0].mRange.mMaxValue < 255)
		{
			InterInstruction* bins = mInstructions[sz - 1];

			mFalseJump->mNumEntries--;
			mFalseJump->WarnUnreachable();
			mFalseJump = nullptr;
			bins->mCode = IC_JUMP;
			bins->mSrc[0].mTemp = -1;
			bins->mNumOperands = 0;
		}

		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_CONSTANT && mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp)
		{
			InterInstruction* bins = mInstructions[sz - 1];

			if (mInstructions[sz - 2]->mConst.mIntConst)
			{
				mFalseJump->mNumEntries--;
				mFalseJump->WarnUnreachable();
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
			else
			{
				mTrueJump->mNumEntries--;
				mTrueJump->WarnUnreachable();
				mTrueJump = mFalseJump;
				mFalseJump = nullptr;
				bins->mCode = IC_JUMP;
				bins->mSrc[0].mTemp = -1;
				bins->mNumOperands = 0;
			}
		}
		
#if 1
		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[sz - 2]->mDst.mTemp == mInstructions[sz - 1]->mSrc[0].mTemp &&
			mInstructions[sz - 2]->mSrc[0].mTemp >= 0 && mInstructions[sz - 2]->mSrc[1].mTemp >= 0)
		{
			if (mFalseJump->mNumEntries == 1 && mFalseJump->mInstructions.Size() == 2 && mFalseJump->mInstructions[1]->mCode == IC_BRANCH &&
				mFalseJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mFalseJump->mInstructions[0]->mDst.mTemp == mFalseJump->mInstructions[1]->mSrc[0].mTemp)
			{
				if (mInstructions[sz - 2]->mOperator == IA_CMPLS && mFalseJump->mInstructions[0]->mOperator == IA_CMPGS &&
					mInstructions[sz - 2]->mSrc[0].mTemp == mFalseJump->mInstructions[0]->mSrc[0].mTemp &&
					mInstructions[sz - 2]->mSrc[1].mTemp == mFalseJump->mInstructions[0]->mSrc[1].mTemp)
				{
					mFalseJump->mInstructions[0]->mOperator = IA_CMPNE;
				}
				else if (mInstructions[sz - 2]->mOperator == IA_CMPGS && mFalseJump->mInstructions[0]->mOperator == IA_CMPLS &&
					mInstructions[sz - 2]->mSrc[0].mTemp == mFalseJump->mInstructions[0]->mSrc[0].mTemp &&
					mInstructions[sz - 2]->mSrc[1].mTemp == mFalseJump->mInstructions[0]->mSrc[1].mTemp)
				{
					mFalseJump->mInstructions[0]->mOperator = IA_CMPNE;
				}
				else if (mInstructions[sz - 2]->mOperator == IA_CMPLS && mFalseJump->mInstructions[0]->mOperator == IA_CMPLS &&
					mInstructions[sz - 2]->mSrc[0].mTemp == mFalseJump->mInstructions[0]->mSrc[1].mTemp &&
					mInstructions[sz - 2]->mSrc[1].mTemp == mFalseJump->mInstructions[0]->mSrc[0].mTemp)
				{
					mFalseJump->mInstructions[0]->mOperator = IA_CMPNE;
				}
				else if (mInstructions[sz - 2]->mOperator == IA_CMPGS && mFalseJump->mInstructions[0]->mOperator == IA_CMPGS &&
					mInstructions[sz - 2]->mSrc[0].mTemp == mFalseJump->mInstructions[0]->mSrc[1].mTemp &&
					mInstructions[sz - 2]->mSrc[1].mTemp == mFalseJump->mInstructions[0]->mSrc[0].mTemp)
				{
					mFalseJump->mInstructions[0]->mOperator = IA_CMPNE;
				}
			}
		}
#endif

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
#if 1
			// Check shortcut double jump
			if (mTrueJump && mTrueJump->mInstructions.Size() == 1 && mTrueJump->mInstructions[0]->mCode == IC_BRANCH && 
				mTrueJump->mInstructions[0]->mSrc[0].mTemp >= 0 && 
				mTrueJump->mInstructions[0]->mSrc[0].mTemp < mTrueValueRange.Size() &&
				IsScalarType(mTrueJump->mInstructions[0]->mSrc[0].mType) &&
				mTrueValueRange[mTrueJump->mInstructions[0]->mSrc[0].mTemp].IsConstant())
			{
				mTrueJump->mNumEntries--;
				mTrueJump->mEntryBlocks.RemoveAll(this);
				if (mTrueValueRange[mTrueJump->mInstructions[0]->mSrc[0].mTemp].mMaxValue == 0)
					mTrueJump = mTrueJump->mFalseJump;
				else
					mTrueJump = mTrueJump->mTrueJump;
				mTrueJump->mNumEntries++;
				mTrueJump->mEntryBlocks.Push(this);
			}

			if (mFalseJump && mFalseJump->mInstructions.Size() == 1 && mFalseJump->mInstructions[0]->mCode == IC_BRANCH && 
				mFalseJump->mInstructions[0]->mSrc[0].mTemp >= 0 && 
				mFalseJump->mInstructions[0]->mSrc[0].mTemp < mFalseValueRange.Size() &&
				IsScalarType(mFalseJump->mInstructions[0]->mSrc[0].mType) &&
				mFalseValueRange[mFalseJump->mInstructions[0]->mSrc[0].mTemp].IsConstant())
			{
				mFalseJump->mNumEntries--;
				mFalseJump->mEntryBlocks.RemoveAll(this);
				if (mFalseValueRange[mFalseJump->mInstructions[0]->mSrc[0].mTemp].mMaxValue == 0)
					mFalseJump = mFalseJump->mFalseJump;
				else
					mFalseJump = mFalseJump->mTrueJump;
				mFalseJump->mNumEntries++;
				mFalseJump->mEntryBlocks.Push(this);
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


bool InterCodeBasicBlock::BuildGlobalIntegerRangeSets(bool initial)
{
	bool	changed = false;

	mNumEntered++;
	if (!mLoopHead && mNumEntered < mEntryBlocks.Size())
		return false;

	assert(mProc->mLocalValueRange.Size() == mExitRequiredTemps.Size());
	assert(mLocalParamValueRange.Size() == mProc->mParamVars.Size());

	bool	firstEntry = true;

	for (int j = 0; j < mEntryBlocks.Size(); j++)
	{
		InterCodeBasicBlock* from = mEntryBlocks[j];
		GrowingIntegerValueRangeArray& range(this == from->mTrueJump ? from->mTrueValueRange : from->mFalseValueRange);
		GrowingIntegerValueRangeArray& prange(this == from->mTrueJump ? from->mTrueParamValueRange : from->mFalseParamValueRange);

		if (range.Size())
		{
			if (firstEntry)
			{
				firstEntry = false;
				mProc->mLocalValueRange = range;
				mLocalParamValueRange = prange;
			}
			else
			{
				for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
				{
					if (this != from || IsTempModified(i))
						mProc->mLocalValueRange[i].Merge(range[i], mLoopHead, initial);
				}
				for (int i = 0; i < mLocalParamValueRange.Size(); i++)
					mLocalParamValueRange[i].Merge(prange[i], mLoopHead, initial);
			}
		}
	}

	if (firstEntry)
	{
		mProc->mLocalValueRange.Clear();
		mLocalParamValueRange.Clear();
	}

	assert(mProc->mLocalValueRange.Size() == mExitRequiredTemps.Size());
	assert(mLocalParamValueRange.Size() == mProc->mParamVars.Size());

	for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
		if (mEntryValueRange[i].Weaker(mProc->mLocalValueRange[i]))
			changed = true;

	for (int i = 0; i < mLocalParamValueRange.Size(); i++)
		if (mEntryParamValueRange[i].Weaker(mLocalParamValueRange[i]))
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
			for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
				mEntryValueRange[i].Expand(mProc->mLocalValueRange[i]);
			for (int i = 0; i < mLocalParamValueRange.Size(); i++)
				mEntryParamValueRange[i].Expand(mLocalParamValueRange[i]);

//			mEntryValueRange = mLocalValueRange;
//			mEntryParamValueRange = mLocalParamValueRange;

			UpdateLocalIntegerRangeSets();

		}

		if (mTrueJump && mTrueJump->BuildGlobalIntegerRangeSets(initial))
			changed = true;
		if (mFalseJump && mFalseJump->BuildGlobalIntegerRangeSets(initial))
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::UnionIntegerRanges(const InterCodeBasicBlock* block)
{
	if (mEntryValueRange.Size() > 0)
	{
		if (block->mEntryValueRange.Size())
		{
			assert(mEntryValueRange.Size() == block->mEntryValueRange.Size());

			for (int i = 0; i < mEntryValueRange.Size(); i++)
				mEntryValueRange[i].Union(block->mEntryValueRange[i]);
		}
		else
			mEntryValueRange.SetSize(0);
	}
		
	for (int i = 0; i < mInstructions.Size(); i++)
	{
		assert(mInstructions[i]->IsEqual(block->mInstructions[i]));
		mInstructions[i]->UnionRanges(block->mInstructions[i]);
	}

}

void InterCodeBasicBlock::MarkIntegerRangeBoundUp(int temp, int64 value, GrowingIntegerValueRangeArray& range)
{
	range[temp].SetLimit(value, value);

	for (int i = mInstructions.Size() - 1; i >= 0; i--)
	{
		InterInstruction* ins(mInstructions[i]);

		if (ins->mDst.mTemp == temp)
		{
			if (ins->mCode == IC_BINARY_OPERATOR && ins->mSrc[1].mTemp == temp && ins->mSrc[0].mTemp < 0)
			{
				switch (ins->mOperator)
				{
				case IA_ADD:
					value -= ins->mSrc[0].mIntConst;
					break;
				case IA_SUB:
					value += ins->mSrc[0].mIntConst;
					break;
				default:
					return;
				}
			}
			else if (ins->mCode == IC_LOAD_TEMPORARY)
			{
				if (!IsTempModifiedInRange(i + 1, mInstructions.Size(), ins->mSrc[0].mTemp))
					range[ins->mSrc[0].mTemp].SetLimit(value, value);
				temp = ins->mSrc[0].mTemp;
			}
			else
				return;
		}
		else if (ins->mCode == IC_LOAD_TEMPORARY && ins->mSrc[0].mTemp == temp)
		{
			if (!IsTempModifiedInRange(i + 1, mInstructions.Size(), ins->mDst.mTemp))
				range[ins->mDst.mTemp].SetLimit(value, value);
		}
	}
}

void InterCodeBasicBlock::UpdateLocalIntegerRangeSetsForward(void)
{
	int sz = mInstructions.Size();

	for (int i = 0; i < sz; i++)
	{
		InterInstruction* ins(mInstructions[i]);

		for (int i = 0; i < ins->mNumOperands; i++)
		{
			if (IsIntegerType(ins->mSrc[i].mType) || ins->mSrc[i].mType == IT_BOOL || ins->mSrc[i].mType == IT_POINTER)
			{
				if (ins->mSrc[i].mTemp >= 0)
				{
					ins->mSrc[i].mRange.MergeUnknown(mProc->mLocalValueRange[ins->mSrc[i].mTemp]);
#if 1
					if (ins->mCode != IC_ASSEMBLER && ins->mSrc[i].mType != IT_POINTER &&
						ins->mSrc[i].mRange.mMinState == IntegerValueRange::S_BOUND && 
						ins->mSrc[i].mRange.mMaxState == IntegerValueRange::S_BOUND && 
						ins->mSrc[i].mRange.mMinValue == ins->mSrc[i].mRange.mMaxValue)
					{
						if (ins->mCode == IC_LOAD_TEMPORARY)
						{
							ins->mCode = IC_CONSTANT;
							ins->mConst.mType = ins->mSrc[0].mType;
							ins->mConst.mIntConst = ins->mSrc[0].mRange.mMinValue;
							ins->mNumOperands = 0;
						}
						else
						{
							ins->mSrc[i].mTemp = -1;
							ins->mSrc[i].mIntConst = ins->mSrc[i].mRange.mMinValue;
						}
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

		if (ins->mDst.mTemp >= 0 && (IsIntegerType(ins->mDst.mType) || ins->mDst.mType == IT_BOOL))
		{
			IntegerValueRange& vr(mProc->mLocalValueRange[ins->mDst.mTemp]);

			switch (ins->mCode)
			{
			case IC_LOAD:
				vr = ins->mDst.mRange;

				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_FPARAM && ins->mSrc[0].mIntConst == 0)
					vr.Limit(mLocalParamValueRange[ins->mSrc[0].mVarIndex]);
#if 1
				if (ins->mDst.mType == IT_INT8)
				{
					vr.LimitMin(-128);
					vr.LimitMax(255);
				}
				else if (ins->mDst.mType == IT_INT16)
				{
					vr.LimitMin(-32768);
					vr.LimitMax(65535);
				}
				else if (ins->mDst.mType == IT_BOOL)
				{
					vr.LimitMin(0);
					vr.LimitMax(1);
				}
#endif
				{
					LinkerObject* lo = mInstructions[i]->mSrc[0].mLinkerObject;

					if (i > 0 &&
						mInstructions[i - 1]->mCode == IC_LEA && mInstructions[i - 1]->mDst.mTemp == ins->mSrc[0].mTemp &&
						mInstructions[i - 1]->mSrc[1].mTemp < 0 && mInstructions[i - 1]->mSrc[1].mMemory == IM_GLOBAL)
						lo = mInstructions[i - 1]->mSrc[1].mLinkerObject;

					if (lo && lo->mFlags & LOBJF_CONST)
					{
						if (ins->mDst.mType == IT_INT8)
						{
							bool	isUnsigned = false, isSigned = false;
							if (i + 1 < mInstructions.Size() && mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal)
							{
								if (mInstructions[i + 1]->mOperator == IA_EXT8TO16U)
									isUnsigned = true;
								else if (mInstructions[i + 1]->mOperator == IA_EXT8TO16S)
									isSigned = true;
							}

							int	start = 0, end = lo->mSize;
#if 1
							if (ins->mSrc[0].mTemp < 0)
							{
								start = int(ins->mSrc[0].mIntConst);
								end = start + 1;
							}
							else
							{
								if (ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)
								{
									start = int(ins->mSrc[0].mRange.mMinValue + ins->mSrc[0].mIntConst);
								}
								if (ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND)
								{
									end = int(ins->mSrc[0].mRange.mMaxValue + ins->mSrc[0].mIntConst + 1);
								}
							}

							if (start < 0)
								start = 0;
							if (end > lo->mSize)
								end = lo->mSize;
#endif

							int	mi = 255, ma = 0;

							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMaxState == IntegerValueRange::S_BOUND &&
								vr.mMinValue >= -128 && vr.mMaxValue <= 127)
							{
								for (int j = start; j < end; j++)
								{
									int v = isUnsigned ? lo->mData[j] : (int8)(lo->mData[j]);
									if (v < mi)
										mi = v;
									if (v > ma)
										ma = v;
								}
							}
							else
							{
								for (int j = start; j < end; j++)
								{
									int v = lo->mData[j];
									if (isUnsigned)
										;
									else if (isSigned)
										v = (int8)v;
									else if (v & 0x80)
										mi = -128;

									if (v < mi)
										mi = v;
									if (v > ma)
										ma = v;
								}
							}

//							printf("LCheck %s:%d %d..%d (0..%d + %d) -> %d..%d\n", ins->mLocation.mFileName, ins->mLocation.mLine, start, end, lo->mSize, (int)(ins->mSrc[0].mIntConst), mi, ma);

							vr.LimitMax(ma);
							vr.LimitMin(mi);
						}
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
					IntegerValueRange& sr(mProc->mLocalValueRange[ins->mSrc[0].mTemp]);

					tr.mMinState = sr.mMaxState;
					tr.mMinValue = -sr.mMaxValue;
					tr.mMaxState = sr.mMinState;
					tr.mMaxValue = -sr.mMinValue;
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
				case IA_EXT8TO32S:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < -128 || vr.mMaxValue > 127)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 127;
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = -128;
					}
					else if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < -128 || vr.mMinValue > 127)
					{
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = -128;
					}
					break;

				case IA_EXT16TO32S:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < -65536 || vr.mMaxValue > 65535)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 65535;
					}
					if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < -65536 || vr.mMinValue > 65535)
					{
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = -65536;
					}
					break;

				case IA_EXT8TO16U:
				case IA_EXT8TO32U:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND && vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue > 0)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 255;
					}
					else
					{
						if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < 0 || vr.mMaxValue > 255 || vr.mMinValue < 0 ||
							vr.mMinState != IntegerValueRange::S_BOUND)
						{
							vr.mMaxState = IntegerValueRange::S_BOUND;
							vr.mMaxValue = 255;
							vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMinValue = 0;
						}
						if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < 0 || vr.mMinValue > 255)
						{
							vr.mMinState = IntegerValueRange::S_BOUND;
							vr.mMinValue = 0;
						}
					}
					break;
				case IA_EXT16TO32U:
					vr = ins->mSrc[0].mRange;
					if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMaxValue < 0 || vr.mMaxValue > 65535 || vr.mMinValue < 0 ||
						vr.mMinState != IntegerValueRange::S_BOUND)
					{
						vr.mMaxState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = 65535;
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = 0;
					}
					if (vr.mMinState != IntegerValueRange::S_BOUND || vr.mMinValue < 0 || vr.mMinValue > 65535)
					{
						vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMinValue = 0;
					}
					break;

				default:
					vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
				}
				break;

			case IC_RELATIONAL_OPERATOR:
				vr.SetLimit(0, 1);
				break;
			case IC_BINARY_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_ADD:
					if (ins->mSrc[0].mTemp < 0)
					{
#if 0
						if (/*ins->mSrc[1].mTemp == ins->mDst.mTemp &&*/ dependTemps[ins->mDst.mTemp] && i + 3 != sz)
						{
							int64 start = pblock->mTrueValueRange[ins->mDst.mTemp].mMinValue;
							vr.SetLimit(start + ins->mSrc[0].mIntConst, start + nloop * ins->mSrc[0].mIntConst);
						}
						else
#endif
						{
							vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
							if (ins->mSrc[0].mIntConst > 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							else if (ins->mSrc[0].mIntConst < 0 && vr.mMinState == IntegerValueRange::S_WEAK)
								vr.mMinState = IntegerValueRange::S_UNBOUND;
							if (ins->mDst.mType == IT_INT8 && (
								ins->mSrc[0].mIntConst >= 128 && vr.mMaxState != IntegerValueRange::S_BOUND ||
								vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue + ins->mSrc[0].mIntConst >= 256))
								vr.mMinState = IntegerValueRange::S_UNBOUND;

							vr.mMaxValue += ins->mSrc[0].mIntConst;
							vr.mMinValue += ins->mSrc[0].mIntConst;

							if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) ||
								vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
							{
								vr.mMinState = IntegerValueRange::S_UNBOUND;
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
							}
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];
						if (ins->mSrc[1].mIntConst > 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (ins->mSrc[1].mIntConst < 0 && vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						vr.mMaxValue += ins->mSrc[1].mIntConst;
						vr.mMinValue += ins->mSrc[1].mIntConst;

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) ||
							vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
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

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) ||
							vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
					}

#if 1
					if (ins->mDst.mType == IT_INT8)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < -255 || vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > 255)
						{
							vr.LimitMax(255);
							vr.LimitMin(-128);
						}
					}
#endif
					break;
				case IA_SUB:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst < 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (ins->mSrc[0].mIntConst > 0 && vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;
						vr.mMaxValue -= ins->mSrc[0].mIntConst;
						vr.mMinValue -= ins->mSrc[0].mIntConst;

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) ||
							vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];

						IntegerValueRange::State	s = vr.mMinState;
						vr.mMinState = vr.mMaxState;
						vr.mMaxState = s;

						int64	maxv = vr.mMaxValue, minv = vr.mMinValue;

						if (vr.mMinState == IntegerValueRange::S_WEAK)
						{
							if (vr.mMaxState == IntegerValueRange::S_WEAK)
								vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
						if (vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;

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
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
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

							int64	maxv = vr.mMaxValue, minv = vr.mMinValue;
							vr.mMaxValue = ins->mSrc[0].mIntConst * minv;
							vr.mMinValue = ins->mSrc[0].mIntConst * maxv;
						}

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) || 
							vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}

					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];
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

							int64	maxv = vr.mMaxValue, minv = vr.mMinValue;
							vr.mMaxValue = ins->mSrc[1].mIntConst * minv;
							vr.mMinValue = ins->mSrc[1].mIntConst * maxv;
						}

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType) ||
							vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue < SignedTypeMin(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;

					break;
				case IA_SHL:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMaxState == IntegerValueRange::S_WEAK)
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						else if (vr.mMinState == IntegerValueRange::S_WEAK)
							vr.mMinState = IntegerValueRange::S_UNBOUND;

						vr.mMaxValue <<= TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
						vr.mMinValue <<= TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);

						if (vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue > UnsignedTypeMax(ins->mDst.mType))
						{
							vr.mMinState = IntegerValueRange::S_UNBOUND;
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}
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
							vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
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
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];

						if (ins->mSrc[0].mIntConst > 0)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= 0)
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = (unsigned short)(int64min(65535, vr.mMaxValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (unsigned short)(int64max(0, vr.mMinValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT8:
									vr.mMaxValue = (unsigned char)(int64min(255, vr.mMaxValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (unsigned char)(int64max(0, vr.mMinValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT32:
									vr.mMaxValue = (unsigned)(vr.mMaxValue) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (unsigned)(vr.mMinValue) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								}
							}
							else
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = 65535 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = 0;
									break;
								case IT_INT8:
									vr.mMaxValue = 255 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = 0;
									break;
								case IT_INT32:
									vr.mMaxValue = 0x100000000ULL >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = 0;
									break;
								}
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMaxState = IntegerValueRange::S_BOUND;
							}
						}
					}
					else if (ins->mSrc[1].mTemp >= 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMinValue >= 0)
							vr.mMinValue = 0;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
				case IA_SAR:
					if (ins->mSrc[0].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];

						if (ins->mSrc[0].mIntConst > 0)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMaxState == IntegerValueRange::S_BOUND)
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = (int16)(int64min(32767, vr.mMaxValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int16)(int64max(-32768, vr.mMinValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT8:
									vr.mMaxValue = (int8)(int64min(127, vr.mMaxValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int8)(int64max(-128, vr.mMinValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT32:
									vr.mMaxValue = (int32)(int64min(2147483647, vr.mMaxValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int32)(int64max(-2147483648, vr.mMinValue)) >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								}
							}
							else
							{
								switch (ins->mSrc[1].mType)
								{
								case IT_INT16:
									vr.mMaxValue = (int16) 32767 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int16)-32768 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT8:
									vr.mMaxValue = (int8) 127 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int8)-128 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								case IT_INT32:
									vr.mMaxValue = (int32) 2147483647 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									vr.mMinValue = (int32)-2147483648 >> TypeShiftMask(ins->mDst.mType, ins->mSrc[0].mIntConst);
									break;
								}
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMaxState = IntegerValueRange::S_BOUND;
							}
						}
					}
					else if (ins->mSrc[1].mTemp >= 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMinValue >= 0)
							vr.mMinValue = 0;
						else if (vr.mMaxValue < 0)
							vr.mMaxValue = -1;
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
				case IA_AND:
					if (ins->mSrc[0].IsUnsigned() && ins->mSrc[1].IsUnsigned())
					{
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
						int64 v0 = (ins->mSrc[0].mRange.mMaxValue & BuildLowerBitsMask(ins->mSrc[1].mRange.mMaxValue));
						int64 v1 = (ins->mSrc[1].mRange.mMaxValue & BuildLowerBitsMask(ins->mSrc[0].mRange.mMaxValue));
						vr.mMaxValue = (v0 > v1) ? v0 : v1;
						vr.mMinValue = 0;
					}
					else if (ins->mSrc[0].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (ins->mSrc[0].mIntConst >= 0)
						{
							if (ins->mSrc[1].IsUnsigned())
							{
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMinValue = 0;
								vr.LimitMax(ins->mSrc[0].mIntConst & BuildLowerBitsMask(ins->mSrc[1].mRange.mMaxValue));
							}
							else
							{
								vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMaxValue = ins->mSrc[0].mIntConst;
								vr.mMinValue = 0;
							}
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];
						if (ins->mSrc[1].mIntConst >= 0)
						{
							if (ins->mSrc[0].IsUnsigned())
							{
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMinValue = 0;
								vr.LimitMax(ins->mSrc[1].mIntConst & BuildLowerBitsMask(ins->mSrc[0].mRange.mMaxValue));
							}
							else
							{
								vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMaxValue = ins->mSrc[1].mIntConst;
								vr.mMinValue = 0;
							}
						}
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;

					if (vr.mMaxState == IntegerValueRange::S_BOUND)
					{
						int64 lowmask = -1;
						if (ins->mSrc[0].mTemp < 0)
							lowmask &= ins->mSrc[0].mIntConst;
						if (ins->mSrc[1].mTemp < 0)
							lowmask &= ins->mSrc[1].mIntConst;
						for (int i = 0; i < 32 && !(lowmask & (1LL << i)); i++)
							vr.mMaxValue &= ~(1LL << i);
					}
					break;

				case IA_OR:
				case IA_XOR:
					if (ins->mSrc[0].IsUnsigned() && ins->mSrc[1].IsUnsigned())
					{
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
						vr.mMaxValue = BuildLowerBitsMask(ins->mSrc[1].mRange.mMaxValue) | BuildLowerBitsMask(ins->mSrc[0].mRange.mMaxValue);
						vr.mMinValue = 0;
					}
					else if (ins->mSrc[0].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];

						if (ins->mSrc[0].mIntConst >= 0)
							vr.mMaxValue = BuildLowerBitsMask(vr.mMaxValue) | ins->mSrc[0].mIntConst;
						else if (ins->mSrc[0].mIntConst < 0)
						{
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
							if (vr.mMinState == IntegerValueRange::S_BOUND)
								vr.mMinValue = int64min(vr.mMinValue, ins->mSrc[0].mIntConst);
							else
							{
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMinValue = ins->mSrc[0].mIntConst;
							}
						}
					}
					else if (ins->mSrc[1].mTemp < 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];

						if (ins->mSrc[1].mIntConst >= 0)
							vr.mMaxValue = BuildLowerBitsMask(vr.mMaxValue) | ins->mSrc[1].mIntConst;
						else if (ins->mSrc[1].mIntConst < 0)
						{
							vr.mMaxState = IntegerValueRange::S_UNBOUND;
							if (vr.mMinState == IntegerValueRange::S_BOUND)
								vr.mMinValue = int64min(vr.mMinValue, ins->mSrc[1].mIntConst);
							else
							{
								vr.mMinState = IntegerValueRange::S_BOUND;
								vr.mMinValue = ins->mSrc[1].mIntConst;
							}
						}
					}
					else
						vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
					break;
#if 1
				case IA_DIVU:

					if (ins->mSrc[1].mTemp >= 0)
					{
						vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
						if (vr.mMaxState != IntegerValueRange::S_BOUND || vr.mMinState < IntegerValueRange::S_WEAK || vr.mMinValue < 0)
						{
							vr.mMaxValue = IntegerValueRange::S_BOUND;
							vr.mMaxValue = UnsignedTypeMax(ins->mSrc[1].mType);
						}
					}
					else
						vr.LimitMax(ins->mSrc[1].mIntConst);

					vr.LimitMin(0);
					vr.mMinValue = 0;

					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst > 1)
						vr.mMaxValue /= ins->mSrc[0].mIntConst;
					break;

				case IA_MODU:
					vr.LimitMin(0);
					if (ins->mSrc[0].mTemp < 0)
						vr.LimitMax(ins->mSrc[0].mIntConst - 1);
					else if (ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND)
						vr.LimitMax(ins->mSrc[0].mRange.mMaxValue - 1);
					else if (ins->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND)
						vr.LimitMax(ins->mSrc[1].mRange.mMaxValue);
					else
						vr.mMaxState = IntegerValueRange::S_UNBOUND;
					if (vr.mMaxValue < 0)
						vr.mMaxState = IntegerValueRange::S_UNBOUND;
					break;
#endif
				default:
					vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
				}
				break;

			default:
				vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
			}

#if 1
			if (ins->mDst.mType == IT_INT8)
			{
				vr.LimitMinBound(-128);
				vr.LimitMaxBound(255);
			}
			else if (ins->mDst.mType == IT_INT16)
			{
				vr.LimitMinBound(-32768);
				vr.LimitMaxBound(65535);
			}
#endif
			ins->mDst.mRange.MergeUnknown(vr);
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
		else if (ins->mDst.mTemp >= 0 && ins->mDst.mType == IT_POINTER)
		{
			IntegerValueRange& vr(mProc->mLocalValueRange[ins->mDst.mTemp]);

			switch (ins->mCode)
			{
			case IC_CONSTANT:
				vr.mMaxState = vr.mMinState = IntegerValueRange::S_BOUND;
				vr.mMinValue = vr.mMaxValue = ins->mConst.mIntConst;
				break;
			case IC_LOAD_TEMPORARY:
				vr = ins->mSrc[0].mRange;
				break;
			case IC_LEA:
				if (ins->mSrc[0].mTemp < 0)
				{
					vr = mProc->mLocalValueRange[ins->mSrc[1].mTemp];
					if (ins->mSrc[0].mIntConst > 0 && vr.mMaxState == IntegerValueRange::S_WEAK)
						vr.mMaxState = IntegerValueRange::S_UNBOUND;
					else if (ins->mSrc[0].mIntConst < 0 && vr.mMinState == IntegerValueRange::S_WEAK)
						vr.mMinState = IntegerValueRange::S_UNBOUND;

					vr.mMaxValue += ins->mSrc[0].mIntConst;
					vr.mMinValue += ins->mSrc[0].mIntConst;
				}
				else if (ins->mSrc[1].mTemp < 0)
				{
					vr = mProc->mLocalValueRange[ins->mSrc[0].mTemp];
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

			default:
				vr.mMaxState = vr.mMinState = IntegerValueRange::S_UNBOUND;
			}

			ins->mDst.mRange.MergeUnknown(vr);
		}
		else if (ins->mCode == IC_STORE)
		{
			if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_FPARAM && ins->mSrc[0].mIntConst == 0)
				mLocalParamValueRange[ins->mSrc[1].mVarIndex] = ins->mSrc[0].mRange;
		}

		assert(mProc->mLocalValueRange.Size() == mExitRequiredTemps.Size());
	}
}

void InterCodeBasicBlock::UpdateLocalIntegerRangeSetsBackward(void) 
{
	mProc->mReverseValueRange.SetSize(mProc->mLocalValueRange.Size());

	for (int i = 0; i < mProc->mReverseValueRange.Size(); i++)
		mProc->mReverseValueRange[i].Reset();

	if (mTrueJump && !mFalseJump)
	{
		for (int i = 0; i < mMemoryValueSize.Size(); i++)
		{
			mEntryMemoryValueSize[i] = mMemoryValueSize[i] = mTrueJump->mMemoryValueSize[i];
		}
	}

	NumberSet	requiredTemps(mExitRequiredTemps);

	int sz = mInstructions.Size();
	for (int i = sz - 1; i >= 0; i--)
	{
		InterInstruction* ins(mInstructions[i]);
		if (ins->mCode == IC_LOAD && ins->mSrc[0].mMemory == IM_INDIRECT && ins->mSrc[0].mTemp >= 0 && requiredTemps[ins->mDst.mTemp])
			mMemoryValueSize[ins->mSrc[0].mTemp] = int64max(mMemoryValueSize[ins->mSrc[0].mTemp], ins->mSrc[0].mIntConst + (InterTypeSize[ins->mDst.mType] - 1) * ins->mSrc[0].mStride + 1);
		else if (ins->mCode == IC_STORE && ins->mSrc[1].mMemory == IM_INDIRECT && ins->mSrc[1].mTemp >= 0)
			mMemoryValueSize[ins->mSrc[1].mTemp] = int64max(mMemoryValueSize[ins->mSrc[1].mTemp], ins->mSrc[1].mIntConst + (InterTypeSize[ins->mSrc[0].mType] - 1) * ins->mSrc[1].mStride + 1);
		else if (ins->mCode == IC_FILL)
		{
			if (ins->mSrc[1].mMemory == IM_INDIRECT && ins->mSrc[1].mTemp >= 0)
				mMemoryValueSize[ins->mSrc[1].mTemp] = ins->mConst.mOperandSize;
		}
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
				asize = mProc->mLocalVars[ins->mSrc[1].mVarIndex]->mSize;

			if (asize > 0)
			{
				mProc->mReverseValueRange[ins->mSrc[0].mTemp].LimitMin(-ins->mSrc[1].mIntConst);
				mProc->mReverseValueRange[ins->mSrc[0].mTemp].LimitMax(asize - ins->mSrc[1].mIntConst - mMemoryValueSize[ins->mDst.mTemp]);
			}
		}

		if (ins->mDst.mTemp < 0 || requiredTemps[ins->mDst.mTemp] || IsObservable(ins->mCode))
		{
			if (ins->mDst.mTemp >= 0)
				requiredTemps -= ins->mDst.mTemp;

			if (ins->mCode == IC_SELECT)
			{
				if (ins->mSrc[0].mTemp >= 0)
					requiredTemps += ins->mSrc[0].mTemp;
			}
			else
			{
				for (int i = 0; i < ins->mNumOperands; i++)
					if (ins->mSrc[i].mTemp >= 0)
						requiredTemps += ins->mSrc[i].mTemp;
			}
		}

		if (ins->mDst.mTemp >= 0)
		{
			ins->mDst.mRange.Limit(mProc->mReverseValueRange[ins->mDst.mTemp]);
			mProc->mReverseValueRange[ins->mDst.mTemp].Reset();
			IntegerValueRange& vr(ins->mDst.mRange);

			switch (ins->mCode)
			{
			case IC_CONVERSION_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_EXT8TO16U:
					if (ins->mSrc[0].mTemp >= 0 && (vr.mMaxValue != 255 || vr.mMinValue != 0))
						mProc->mReverseValueRange[ins->mSrc[0].mTemp].Limit(vr);
					break;
				case IA_EXT8TO16S:
					if (ins->mSrc[0].mTemp >= 0 && (vr.mMaxValue != 127 || vr.mMinValue != -128))
						mProc->mReverseValueRange[ins->mSrc[0].mTemp].Limit(vr);
					break;
				}
				break;
			case IC_BINARY_OPERATOR:
				switch (ins->mOperator)
				{
				case IA_SHL:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (ins->mDst.mType == IT_INT16)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= -0x4000 &&
								vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue < 0x4000)
							{
								ins->mSrc[1].mRange.LimitMin(vr.mMinValue >> ins->mSrc[0].mIntConst);
								ins->mSrc[1].mRange.LimitMax(vr.mMaxValue >> ins->mSrc[0].mIntConst);
								mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
							}
						}
						else if (ins->mDst.mType == IT_INT8)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= -0x40 &&
								vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue < 0x40)
							{
								ins->mSrc[1].mRange.LimitMin(vr.mMinValue >> ins->mSrc[0].mIntConst);
								ins->mSrc[1].mRange.LimitMax(vr.mMaxValue >> ins->mSrc[0].mIntConst);
								mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
							}
						}
						else if (ins->mDst.mType == IT_INT32)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= -0x40000000 &&
								vr.mMaxState == IntegerValueRange::S_BOUND && vr.mMaxValue < 0x40000000)
							{
								ins->mSrc[1].mRange.LimitMin(vr.mMinValue >> ins->mSrc[0].mIntConst);
								ins->mSrc[1].mRange.LimitMax(vr.mMaxValue >> ins->mSrc[0].mIntConst);
								mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
							}
						}
					}
					break;
				case IA_SUB:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue + ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue + ins->mSrc[0].mIntConst);

						mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					break;
				case IA_ADD:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0)
					{
						if (ins->mDst.mType == IT_INT8)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= 0 && vr.mMinValue - ins->mSrc[0].mIntConst < 0)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}

						if (ins->mDst.mType != IT_INT8 || vr.mMinValue > -128 || vr.mMaxValue < 255)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND)
								ins->mSrc[1].mRange.LimitMin(vr.mMinValue - ins->mSrc[0].mIntConst);
							if (vr.mMaxState == IntegerValueRange::S_BOUND)
								ins->mSrc[1].mRange.LimitMax(vr.mMaxValue - ins->mSrc[0].mIntConst);
						}
						
						mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0)
					{
						if (ins->mDst.mType == IT_INT8)
						{
							if (vr.mMinState == IntegerValueRange::S_BOUND && vr.mMinValue >= 0 && vr.mMinValue - ins->mSrc[1].mIntConst < 0)
								vr.mMaxState = IntegerValueRange::S_UNBOUND;
						}

						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMin(vr.mMinValue - ins->mSrc[1].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMax(vr.mMaxValue - ins->mSrc[1].mIntConst);
						mProc->mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
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

						mProc->mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
						mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					break;
				case IA_MUL:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mIntConst > 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMin(vr.mMinValue / ins->mSrc[0].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[1].mRange.LimitMax(vr.mMaxValue / ins->mSrc[0].mIntConst);
						mProc->mReverseValueRange[ins->mSrc[1].mTemp].Limit(ins->mSrc[1].mRange);
					}
					else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mIntConst > 0)
					{
						if (vr.mMinState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMin(vr.mMinValue / ins->mSrc[1].mIntConst);
						if (vr.mMaxState == IntegerValueRange::S_BOUND)
							ins->mSrc[0].mRange.LimitMax(vr.mMaxValue / ins->mSrc[1].mIntConst);
						mProc->mReverseValueRange[ins->mSrc[0].mTemp].Limit(ins->mSrc[0].mRange);
					}
					break;
				}
				break;

			case IC_LEA:
				if (ins->mSrc[1].mMemory == IM_INDIRECT && mMemoryValueSize[ins->mDst.mTemp] > 0)
				{
					if (ins->mSrc[0].mTemp < 0)
					{
						if (ins->mSrc[0].mIntConst >= 0)
							mMemoryValueSize[ins->mSrc[1].mTemp] = mMemoryValueSize[ins->mDst.mTemp] - ins->mSrc[0].mIntConst;
					}
					else if (ins->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND)
					{
						if (ins->mSrc[0].mRange.mMinValue >= 0)
							mMemoryValueSize[ins->mSrc[1].mTemp] = mMemoryValueSize[ins->mDst.mTemp] - ins->mSrc[0].mRange.mMinValue;
					}
				}
				break;
			}

		}

		for (int i = 0; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp >= 0)
				ins->mSrc[i].mRange.Limit(mProc->mReverseValueRange[ins->mSrc[i].mTemp]);
		}

		if (ins->mDst.mTemp >= 0)
			mMemoryValueSize[ins->mDst.mTemp] = 0;
	}
}

bool InterCodeBasicBlock::TempIsUnsigned(int temp)
{
	for (int i = 0; i < mInstructions.Size(); i++)
	{
		InterInstruction* ins = mInstructions[i];
		if (ins->UsesTemp(temp))
		{
			if (ins->mCode == IC_CONVERSION_OPERATOR && (ins->mOperator == IA_EXT8TO16U || ins->mOperator == IA_EXT8TO32U))
				return true;
		}
	}

	return false;
}

void InterCodeBasicBlock::UpdateLocalIntegerRangeSets(void)
{
	mProc->mLocalValueRange = mEntryValueRange;
	mLocalParamValueRange = mEntryParamValueRange;

	int sz = mInstructions.Size();

	assert(mProc->mLocalValueRange.Size() == mExitRequiredTemps.Size());

	InterCodeBasicBlock	*	pblock;
	int64					nloop;
	bool					nfixed;

	bool singleLoop = CheckSingleBlockLimitedLoop(pblock, nloop, nfixed);

#if 0
	FastNumberSet		dependTemps(mExitRequiredTemps.Size());
#endif

	struct TempChain
	{
		int		mBaseTemp;
		bool	mConstant;
		int64	mOffset;
	};

	ExpandingArray<TempChain>		tempChain;

	if (singleLoop)
	{
		if (nfixed && nloop == 1)
		{
			InterInstruction* jins = mInstructions.Last();
			jins->mCode = IC_JUMP;
			jins->mNumOperands = 0;
			if (mTrueJump == this)
				mTrueJump = mFalseJump;

			mNumEntries--;
			mEntryBlocks.RemoveAll(this);
			mFalseJump = nullptr;
		}
		else
		{
			tempChain.SetSize(mExitRequiredTemps.Size());

			for (int i = 0; i < mExitRequiredTemps.Size(); i++)
			{
				tempChain[i].mBaseTemp = i;
				tempChain[i].mOffset = 0;
				tempChain[i].mConstant = true;
			}

			for (int i = 0; i < sz; i++)
			{
				InterInstruction* ins(mInstructions[i]);
				if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
					ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst > 0 &&
					tempChain[ins->mSrc[1].mTemp].mBaseTemp >= 0)
				{
					tempChain[ins->mDst.mTemp].mBaseTemp = tempChain[ins->mSrc[1].mTemp].mBaseTemp;
					tempChain[ins->mDst.mTemp].mOffset = tempChain[ins->mSrc[1].mTemp].mOffset + ins->mSrc[0].mIntConst;
					tempChain[ins->mDst.mTemp].mConstant = tempChain[ins->mSrc[1].mTemp].mConstant;
				}
				else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
					ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst > 0 &&
					tempChain[ins->mSrc[0].mTemp].mBaseTemp >= 0)
				{
					tempChain[ins->mDst.mTemp].mBaseTemp = tempChain[ins->mSrc[0].mTemp].mBaseTemp;
					tempChain[ins->mDst.mTemp].mOffset = tempChain[ins->mSrc[0].mTemp].mOffset + ins->mSrc[1].mIntConst;
					tempChain[ins->mDst.mTemp].mConstant = tempChain[ins->mSrc[0].mTemp].mConstant;
				}
				else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
					ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mTemp >= 0 && ins->mSrc[0].mRange.IsBound() && ins->mSrc[0].IsUnsigned() &&
					tempChain[ins->mSrc[1].mTemp].mBaseTemp >= 0)
				{
					tempChain[ins->mDst.mTemp].mBaseTemp = tempChain[ins->mSrc[1].mTemp].mBaseTemp;
					tempChain[ins->mDst.mTemp].mOffset = tempChain[ins->mSrc[1].mTemp].mOffset + ins->mSrc[0].mRange.mMaxValue;
					tempChain[ins->mDst.mTemp].mConstant = false;
				}
				else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
					ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0 && ins->mSrc[1].mRange.IsBound() && ins->mSrc[1].IsUnsigned() &&
					tempChain[ins->mSrc[0].mTemp].mBaseTemp >= 0)
				{
					tempChain[ins->mDst.mTemp].mBaseTemp = tempChain[ins->mSrc[0].mTemp].mBaseTemp;
					tempChain[ins->mDst.mTemp].mOffset = tempChain[ins->mSrc[0].mTemp].mOffset + ins->mSrc[1].mRange.mMaxValue;
					tempChain[ins->mDst.mTemp].mConstant = false;
				}
				else if (ins->mCode == IC_CONVERSION_OPERATOR && ins->mOperator == IA_EXT8TO16U && ins->mSrc[0].mTemp >= 0)
				{
					tempChain[ins->mDst.mTemp] = tempChain[ins->mSrc[0].mTemp];
				}
				else if (ins->mDst.mTemp >= 0)
				{
					tempChain[ins->mDst.mTemp].mBaseTemp = -1;
				}
			}

			for (int i = 0; i < tempChain.Size(); i++)
			{
				if (tempChain[i].mBaseTemp == i)
				{
					IntegerValueRange& r(pblock->mTrueValueRange[i]);
					if (r.IsConstant())
					{
						mProc->mLocalValueRange[i].LimitMax(r.mMinValue + (nloop - 1) * tempChain[i].mOffset);
					}
				}
			}
		}
	}

	UpdateLocalIntegerRangeSetsForward();
#if 1

	UpdateLocalIntegerRangeSetsBackward();
#if 1
	mProc->mLocalValueRange = mEntryValueRange;
	for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
		mProc->mLocalValueRange[i].LimitWeak(mProc->mReverseValueRange[i]);
	mLocalParamValueRange = mEntryParamValueRange;

	if (singleLoop)
	{
		for (int i = 0; i < tempChain.Size(); i++)
		{
			if (tempChain[i].mBaseTemp == i)
			{
				IntegerValueRange& r(pblock->mTrueValueRange[i]);
				if (r.IsConstant())
				{
					mProc->mLocalValueRange[i].LimitMax(r.mMinValue + (nloop - 1) * tempChain[i].mOffset);
				}
			}
		}
	}

	UpdateLocalIntegerRangeSetsForward();
	UpdateLocalIntegerRangeSetsBackward();
#endif

#endif

	mTrueValueRange = mProc->mLocalValueRange;
	mFalseValueRange = mProc->mLocalValueRange;
	mTrueParamValueRange = mLocalParamValueRange;
	mFalseParamValueRange = mLocalParamValueRange;

	if (singleLoop && nfixed)
	{
		for (int i = 0; i < tempChain.Size(); i++)
		{
			if (tempChain[i].mBaseTemp == i && tempChain[i].mConstant)
			{
				IntegerValueRange& r(pblock->mTrueValueRange[i]);
				if (r.IsConstant())
				{
					if (mTrueJump == this)
						mFalseValueRange[i].SetConstant(r.mMinValue + nloop * tempChain[i].mOffset);
					else
						mTrueValueRange[i].SetConstant(r.mMinValue + nloop * tempChain[i].mOffset);
				}
			}
		}
	}

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
		else if (mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 1]->mSrc[0].mTemp >= 0 && IsIntegerType(mInstructions[sz - 1]->mSrc[0].mType))
		{
			int s = mInstructions[sz - 1]->mSrc[0].mTemp;

			mFalseValueRange[s].mMinState = IntegerValueRange::S_BOUND;
			mFalseValueRange[s].mMinValue = 0;
			mFalseValueRange[s].mMaxState = IntegerValueRange::S_BOUND;
			mFalseValueRange[s].mMaxValue = 0;
		}
	}

	if (sz >= 2)
	{
		InterInstruction* cins = mInstructions[sz - 2];

		if (mInstructions[sz - 1]->mCode == IC_BRANCH && cins->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[sz - 1]->mSrc[0].mTemp == cins->mDst.mTemp && IsIntegerType(cins->mSrc[0].mType))
		{
			int	s1 = cins->mSrc[1].mTemp, s0 = cins->mSrc[0].mTemp;
			int s1c = -1, s0c = -1;
			InterOperator	s1o, s0o;

#if 1
			for (int si = 0; si < mExitConversionInstructions.Size(); si++)
			{
				if (mExitConversionInstructions[si]->mCode == IC_CONVERSION_OPERATOR && (mExitConversionInstructions[si]->mOperator == IA_EXT8TO16S || mExitConversionInstructions[si]->mOperator == IA_EXT8TO16U))
				{
					if (s1 == mExitConversionInstructions[si]->mSrc[0].mTemp)
					{
						s1c = mExitConversionInstructions[si]->mDst.mTemp;
						s1o = mExitConversionInstructions[si]->mOperator;
					}
					if (s0 == mExitConversionInstructions[si]->mSrc[0].mTemp)
					{
						s0c = mExitConversionInstructions[si]->mDst.mTemp;
						s0o = mExitConversionInstructions[si]->mOperator;
					}
				}
			}
#else
			for(int si=0; si<sz-2; si++)
			{
				if (mInstructions[si]->mDst.mTemp == s0 || mInstructions[si]->mDst.mTemp == s0c)
					s0c = -1;
				else if (mInstructions[si]->mDst.mTemp == s1 || mInstructions[si]->mDst.mTemp == s1c)
					s1c = -1;
				if (mInstructions[si]->mCode == IC_CONVERSION_OPERATOR && (mInstructions[si]->mOperator == IA_EXT8TO16S || mInstructions[si]->mOperator == IA_EXT8TO16U))
				{
					if (s1 == mInstructions[si]->mSrc[0].mTemp)
					{
						s1c = mInstructions[si]->mDst.mTemp;
						s1o = mInstructions[si]->mOperator;
					}
					if (s0 == mInstructions[si]->mSrc[0].mTemp)
					{
						s0c = mInstructions[si]->mDst.mTemp;
						s0o = mInstructions[si]->mOperator;
					}
				}
			}
#endif
			switch (cins->mOperator)
			{
#if 1
			case IA_CMPEQ:
				if (s0 < 0)
				{
					MarkIntegerRangeBoundUp(s1, cins->mSrc[0].mIntConst, mTrueValueRange);
#if 0
					mTrueValueRange[s1].mMinState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s1].mMinValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
					mTrueValueRange[s1].mMaxState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s1].mMaxValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
#endif
				}
				else if (s1 < 0)
				{
					MarkIntegerRangeBoundUp(s0, cins->mSrc[1].mIntConst, mTrueValueRange);
#if 0
					mTrueValueRange[s0].mMinState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s0].mMinValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
					mTrueValueRange[s0].mMaxState = IntegerValueRange::S_BOUND;
					mTrueValueRange[s0].mMaxValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
#endif
				}
				break;

			case IA_CMPNE:
				if (s0 < 0)
				{
					MarkIntegerRangeBoundUp(s1, cins->mSrc[0].mIntConst, mFalseValueRange);
#if 0
					mFalseValueRange[s1].mMinState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s1].mMinValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
					mFalseValueRange[s1].mMaxState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s1].mMaxValue = mInstructions[sz - 2]->mSrc[0].mIntConst;
#endif
				}
				else if (s1 < 0)
				{
					MarkIntegerRangeBoundUp(s0, cins->mSrc[1].mIntConst, mFalseValueRange);
#if 0
					mFalseValueRange[s0].mMinState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s0].mMinValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
					mFalseValueRange[s0].mMaxState = IntegerValueRange::S_BOUND;
					mFalseValueRange[s0].mMaxValue = mInstructions[sz - 2]->mSrc[1].mIntConst;
#endif
				}
				break;
#endif
			case IA_CMPLS:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(cins->mSrc[0].mIntConst - 1);
					mTrueValueRange[s1].LimitMinWeak(SignedTypeMin(cins->mSrc[1].mType));

					mFalseValueRange[s1].LimitMin(cins->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMaxWeak(SignedTypeMax(cins->mSrc[1].mType));

					if (s1c >= 0 && s1o == IA_EXT8TO16S)
					{
						mTrueValueRange[s1c].LimitMax(cins->mSrc[0].mIntConst - 1);
						mTrueValueRange[s1c].LimitMinWeak(SignedTypeMin(cins->mSrc[1].mType));

						mFalseValueRange[s1c].LimitMin(cins->mSrc[0].mIntConst);
						mFalseValueRange[s1c].LimitMaxWeak(SignedTypeMax(cins->mSrc[1].mType));
					}
				}
				else if (s1 < 0)
				{
					mTrueValueRange[s0].LimitMin(cins->mSrc[1].mIntConst + 1);
					mTrueValueRange[s0].LimitMaxWeak(SignedTypeMax(cins->mSrc[0].mType));

					mFalseValueRange[s0].LimitMax(cins->mSrc[1].mIntConst);
					mFalseValueRange[s0].LimitMinWeak(SignedTypeMin(cins->mSrc[0].mType));

					if (s0c >= 0 && s0o == IA_EXT8TO16S)
					{
						mTrueValueRange[s0c].LimitMin(cins->mSrc[1].mIntConst + 1);
						mTrueValueRange[s0c].LimitMaxWeak(SignedTypeMax(cins->mSrc[0].mType));

						mFalseValueRange[s0c].LimitMax(cins->mSrc[1].mIntConst);
						mFalseValueRange[s0c].LimitMinWeak(SignedTypeMin(cins->mSrc[0].mType));
					}
				}
				else
				{
					if (mProc->mLocalValueRange[s0].mMaxState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s1].LimitMaxWeak(mProc->mLocalValueRange[s0].mMaxValue - 1);
					if (mProc->mLocalValueRange[s0].mMinState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s1].LimitMinWeak(mProc->mLocalValueRange[s0].mMinValue);
				}
				break;
			case IA_CMPLES:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(cins->mSrc[0].mIntConst);
					mTrueValueRange[s1].LimitMinWeak(SignedTypeMin(cins->mSrc[1].mType));
					mFalseValueRange[s1].LimitMin(cins->mSrc[0].mIntConst + 1);
					mFalseValueRange[s1].LimitMaxWeak(SignedTypeMax(cins->mSrc[1].mType));
				}
				else if (s1 < 0)
				{
					mTrueValueRange[s0].LimitMin(cins->mSrc[1].mIntConst);
					mTrueValueRange[s0].LimitMaxWeak(SignedTypeMax(cins->mSrc[0].mType));
					mFalseValueRange[s0].LimitMax(cins->mSrc[1].mIntConst - 1);
					mFalseValueRange[s0].LimitMinWeak(SignedTypeMin(cins->mSrc[0].mType));
				}
				else
				{
					if (mProc->mLocalValueRange[s0].mMaxState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s1].LimitMin(mProc->mLocalValueRange[s0].mMinValue + 1);
					if (mProc->mLocalValueRange[s0].mMinState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s1].LimitMax(mProc->mLocalValueRange[s0].mMaxValue);

					if (mProc->mLocalValueRange[s1].mMaxState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s0].LimitMax(mProc->mLocalValueRange[s1].mMaxValue - 1);
					if (mProc->mLocalValueRange[s1].mMinState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s0].LimitMin(mProc->mLocalValueRange[s1].mMinValue);
				}
				break;
			case IA_CMPGS:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMin(cins->mSrc[0].mIntConst + 1);
					mTrueValueRange[s1].LimitMaxWeak(SignedTypeMax(cins->mSrc[1].mType));
					mFalseValueRange[s1].LimitMax(cins->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMinWeak(SignedTypeMin(cins->mSrc[1].mType));
				}
				else if (s1 < 0)
				{
					mTrueValueRange[s0].LimitMax(cins->mSrc[1].mIntConst - 1);
					mTrueValueRange[s0].LimitMinWeak(SignedTypeMin(cins->mSrc[0].mType));
					mFalseValueRange[s0].LimitMin(cins->mSrc[1].mIntConst);
					mFalseValueRange[s0].LimitMaxWeak(SignedTypeMax(cins->mSrc[0].mType));
				}
				else
				{
					if (mProc->mLocalValueRange[s0].mMaxState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s1].LimitMin(mProc->mLocalValueRange[s0].mMinValue + 1);
					if (mProc->mLocalValueRange[s0].mMinState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s1].LimitMax(mProc->mLocalValueRange[s0].mMaxValue);

					if (mProc->mLocalValueRange[s1].mMaxState == IntegerValueRange::S_BOUND)
						mTrueValueRange[s0].LimitMax(mProc->mLocalValueRange[s1].mMaxValue - 1);
					if (mProc->mLocalValueRange[s1].mMinState == IntegerValueRange::S_BOUND)
						mFalseValueRange[s0].LimitMin(mProc->mLocalValueRange[s1].mMinValue);
				}
				break;
			case IA_CMPGES:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMin(cins->mSrc[0].mIntConst);
					mTrueValueRange[s1].LimitMaxWeak(SignedTypeMax(cins->mSrc[1].mType));
					mFalseValueRange[s1].LimitMax(cins->mSrc[0].mIntConst - 1);
					mFalseValueRange[s1].LimitMinWeak(SignedTypeMin(cins->mSrc[1].mType));
				}
				break;

			case  IA_CMPLU:
				if (s1 >= 0)
				{
					if (s0 < 0)
					{
						if (cins->mSrc[0].mIntConst > 0)
						{
							mTrueValueRange[s1].LimitMax(cins->mSrc[0].mIntConst - 1);

							if (s1c >= 0 && s1o == IA_EXT8TO16U)
							{
								mTrueValueRange[s1c].LimitMax(cins->mSrc[0].mIntConst - 1);
								mFalseValueRange[s1c].LimitMin(cins->mSrc[0].mIntConst);
							}

							if (cins->mSrc[0].mIntConst > 0 && cins->mSrc[0].mIntConst < SignedTypeMax(cins->mSrc[1].mType))
								mTrueValueRange[s1].LimitMin(0);
							else
								mTrueValueRange[s1].LimitMinWeak(0);

/*							if (cins->mSrc[1].mType == IT_INT8)
							{
								mFalseValueRange[s1].LimitMin(cins->mSrc[0].mIntConst);
								mFalseValueRange[s1].LimitMax(255);
							}
							else*/ if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && mFalseValueRange[s1].mMinValue >= 0)
							{
								mFalseValueRange[s1].LimitMin(cins->mSrc[0].mIntConst);
							}
						}
					}
					else if (cins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND)
					{
						mTrueValueRange[s1].LimitMax(cins->mSrc[0].mRange.mMaxValue - 1);
						mTrueValueRange[s1].LimitMinWeak(0);

						if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && mFalseValueRange[s1].mMinValue >= 0)
						{
							mFalseValueRange[s1].LimitMin(cins->mSrc[0].mRange.mMinValue);
						}
					}
				}
				else if (s0 >= 0)
				{
					mTrueValueRange[s0].LimitMin(cins->mSrc[1].mIntConst + 1);
					mFalseValueRange[s0].LimitMax(cins->mSrc[1].mIntConst);
				}
				break;
			case IA_CMPLEU:
				if (s0 < 0)
				{
					mTrueValueRange[s1].LimitMax(cins->mSrc[0].mIntConst);
					mTrueValueRange[s1].LimitMin(0);
					if (s1c >= 0)
					{
						mTrueValueRange[s1c].LimitMax(cins->mSrc[0].mIntConst);
						mTrueValueRange[s1c].LimitMin(0);
					}

					if (mFalseValueRange[s1].mMinState == IntegerValueRange::S_BOUND && 
						mFalseValueRange[s1].mMinValue >= 0 || mFalseJump->TempIsUnsigned(s1))
					{
						mFalseValueRange[s1].LimitMin(cins->mSrc[0].mIntConst + 1);
					}
					if (s1c >= 0 && s1o == IA_EXT8TO16U)
						mFalseValueRange[s1c].LimitMin(cins->mSrc[0].mIntConst + 1);
				}
				break;
			case IA_CMPGU:
				if (s1 >= 0)
				{
					if (cins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND && cins->mSrc[1].mRange.mMinValue < 0)
						mTrueValueRange[s1].mMaxState = IntegerValueRange::S_UNBOUND;

					mTrueValueRange[s1].LimitMin(1);
					if (s0 < 0)
					{
						if (cins->mSrc[0].mIntConst >= 0)
						{
							mTrueValueRange[s1].LimitMin(cins->mSrc[0].mIntConst + 1);
							mFalseValueRange[s1].LimitMax(cins->mSrc[0].mIntConst);
							mFalseValueRange[s1].LimitMin(0);
						}
					}
				}
				break;
			case IA_CMPGEU:
				if (s0 < 0)
				{
					if (cins->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND && cins->mSrc[1].mRange.mMinValue < 0)
						mTrueValueRange[s1].mMaxState = IntegerValueRange::S_UNBOUND;

					mTrueValueRange[s1].LimitMin(cins->mSrc[0].mIntConst);
					mFalseValueRange[s1].LimitMax(cins->mSrc[0].mIntConst - 1);
					mFalseValueRange[s1].LimitMin(0);
				}
				else
					mFalseValueRange[s0].LimitMin(1);
				break;
			}

			if (s1 >= 0 && sz > 2 && mInstructions[sz - 3]->mCode == IC_LOAD_TEMPORARY && mInstructions[sz - 3]->mSrc[0].mTemp == s1)
			{
				mTrueValueRange[mInstructions[sz - 3]->mDst.mTemp] = mTrueValueRange[s1];
				mFalseValueRange[mInstructions[sz - 3]->mDst.mTemp] = mFalseValueRange[s1];
			}

			if (sz >= 3 && mInstructions[sz - 3]->mCode == IC_LOAD)
			{
				InterInstruction* ins = mInstructions[sz - 3];

				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_FPARAM)
				{
					mTrueParamValueRange[ins->mSrc[0].mVarIndex].Limit(mTrueValueRange[ins->mDst.mTemp]);
					mFalseParamValueRange[ins->mSrc[0].mVarIndex].Limit(mFalseValueRange[ins->mDst.mTemp]);
				}
			}
		}
		else if (mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp && 
			cins->mSrc[0].mType == IT_POINTER && cins->mSrc[1].mType == IT_POINTER)
		{


			int	s1 = cins->mSrc[1].mTemp, s0 = cins->mSrc[0].mTemp;

		}

	}

	for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
	{
		if (!mExitRequiredTemps[i])
		{
			mProc->mLocalValueRange[i].mMinState = mProc->mLocalValueRange[i].mMaxState = IntegerValueRange::S_UNKNOWN;
			mTrueValueRange[i] = mFalseValueRange[i] = mProc->mLocalValueRange[i];
		}
	}
}



void InterCodeBasicBlock::PruneUnusedIntegerRangeSets(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mEntryValueRange.Size() > 0 && mEntryRequiredTemps.Size())
		{
			for (int i = 0; i < mEntryValueRange.Size(); i++)
			{
				if (!mEntryRequiredTemps[i])
					mEntryValueRange[i].Reset();
			}
		}

		if (mTrueJump) mTrueJump->PruneUnusedIntegerRangeSets();
		if (mFalseJump) mFalseJump->PruneUnusedIntegerRangeSets();
	}
}

void InterCodeBasicBlock::RestartLocalIntegerRangeSets(int num)
{
	if (!mVisited)
	{
		mVisited = true;

		mValueRangeValid = false;

		mEntryValueRange.SetSize(num, false);
		mTrueValueRange.SetSize(num, false);
		mFalseValueRange.SetSize(num, false);
//		mLocalValueRange.SetSize(num, false);
		mMemoryValueSize.SetSize(num, false);
		mEntryMemoryValueSize.SetSize(num, false);
#if 0
		if (mTrueJump && !mTrueJump->mVisited)
			mTrueJump->mMemoryValueSize.SetSize(num, true);
		if (mFalseJump && !mFalseJump->mVisited)
			mFalseJump->mMemoryValueSize.SetSize(num, true);
#endif
		mEntryParamValueRange.SetSize(mProc->mParamVars.Size(), false);
		mTrueParamValueRange.SetSize(mProc->mParamVars.Size(), false);
		mFalseParamValueRange.SetSize(mProc->mParamVars.Size(), false);
		mLocalParamValueRange.SetSize(mProc->mParamVars.Size(), false);

		for (int i = 0; i < mEntryValueRange.Size(); i++)
			mEntryValueRange[i].Restart();

		for (int i = 0; i < mEntryParamValueRange.Size(); i++)
			mEntryParamValueRange[i].Restart();

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->mDst.mRange.Restart();
			for (int j = 0; j < mInstructions[i]->mNumOperands; j++)
				mInstructions[i]->mSrc[j].mRange.Restart();
		}

		UpdateLocalIntegerRangeSets();

		if (mTrueJump) mTrueJump->RestartLocalIntegerRangeSets(num);
		if (mFalseJump) mFalseJump->RestartLocalIntegerRangeSets(num);
	}
}

void InterCodeBasicBlock::BuildLocalIntegerRangeSets(int num)
{
	if (!mVisited)
	{
		mVisited = true;

		mEntryValueRange.SetSize(num, true);
		mTrueValueRange.SetSize(num, true);
		mFalseValueRange.SetSize(num, true);
//		mLocalValueRange.SetSize(num, true);
		mMemoryValueSize.SetSize(num, true);
		mEntryMemoryValueSize.SetSize(num, true);

		mEntryParamValueRange.SetSize(mProc->mParamVars.Size(), true);
		mTrueParamValueRange.SetSize(mProc->mParamVars.Size(), true);
		mFalseParamValueRange.SetSize(mProc->mParamVars.Size(), true);
		mLocalParamValueRange.SetSize(mProc->mParamVars.Size(), true);

		UpdateLocalIntegerRangeSets();

		if (mTrueJump) mTrueJump->BuildLocalIntegerRangeSets(num);
		if (mFalseJump) mFalseJump->BuildLocalIntegerRangeSets(num);
	}
}

void InterCodeBasicBlock::PropagateValueRangeSetConversions(const ExpandingInstructionPtrArray& tvalue)
{
	if (!mVisited)
	{
		if (mLoopHead)
		{
			mExitConversionInstructions.SetSize(0);
		}
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
			{
				int i = 0, j = 0;
				while (i < mExitConversionInstructions.Size())
				{
					if (tvalue.Contains(mExitConversionInstructions[i]))
						mExitConversionInstructions[j++] = mExitConversionInstructions[i++];
					else
						i++;
				}
				mExitConversionInstructions.SetSize(j);
			}
			else
				mExitConversionInstructions = tvalue;

			mNumEntered++;

			if (mNumEntered < mNumEntries)
				return;
		}

		for (int k = 0; k < mInstructions.Size(); k++)
		{
			InterInstruction* ins = mInstructions[k];

			int reg = ins->mDst.mTemp;
			if (reg >= 0)
			{
				int i = 0, j = 0;
				while (i < mExitConversionInstructions.Size())
				{
					if (mExitConversionInstructions[i]->ReferencesTemp(reg))
						i++;
					else
						mExitConversionInstructions[j++] = mExitConversionInstructions[i++];
				}
				mExitConversionInstructions.SetSize(j);
				if (ins->mCode == IC_CONVERSION_OPERATOR || ins->mCode == IC_LOAD_TEMPORARY)
					mExitConversionInstructions.Push(ins);
			}
		}

		mVisited = true;

		if (mTrueJump) mTrueJump->PropagateValueRangeSetConversions(mExitConversionInstructions);
		if (mFalseJump) mFalseJump->PropagateValueRangeSetConversions(mExitConversionInstructions);
	}
}

void InterCodeBasicBlock::BuildConstTempSets(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mEntryConstTemp.Reset(mEntryRequiredTemps.Size());
		mExitConstTemp.Reset(mEntryRequiredTemps.Size());

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
			
				if (!HasSideEffect(ins->mCode) && ins->mCode != IC_CONSTANT && ins->mCode != IC_STORE && ins->mCode != IC_COPY && ins->mCode != IC_FILL)
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
								CanSwapInstructions(eb->mInstructions[di - 1], ins))
							{
								di--;
							}

							eb->mInstructions.Insert(di, nins);
							if (ins->mDst.mTemp >= 0)
								eb->mExitRequiredTemps += ins->mDst.mTemp;
						}

						if (ins->mDst.mTemp >= 0)
							mEntryRequiredTemps += ins->mDst.mTemp;

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

		mLocalRequiredTemps.Reset(num);
		mLocalProvidedTemps.Reset(num);

		mEntryRequiredTemps.Reset(num);
		mEntryProvidedTemps.Reset(num);
		mEntryPotentialTemps.Reset(num);
		mExitRequiredTemps.Reset(num);
		mExitProvidedTemps.Reset(num);
		mExitPotentialTemps.Reset(num);

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->FilterTempUsage(mLocalRequiredTemps, mLocalProvidedTemps);
		}

		mEntryRequiredTemps = mLocalRequiredTemps;
		mExitProvidedTemps = mLocalProvidedTemps;
		mExitPotentialTemps = mLocalProvidedTemps;

		if (mTrueJump) mTrueJump->BuildLocalTempSets(num);
		if (mFalseJump) mFalseJump->BuildLocalTempSets(num);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedTempSet(const NumberSet & fromProvidedTemps, const NumberSet& potentialProvidedTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mEntryProvidedTemps = fromProvidedTemps;
		mEntryPotentialTemps = potentialProvidedTemps;
		changed = true;
	}
	else
	{
		if (!(mEntryProvidedTemps <= fromProvidedTemps))
		{
			mEntryProvidedTemps &= fromProvidedTemps;
			changed = true;
		}
		if (!(potentialProvidedTemps <= mEntryPotentialTemps))
		{
			mEntryPotentialTemps |= potentialProvidedTemps;
			changed = true;
		}
	}

	if (changed)
	{
		mExitProvidedTemps = mLocalProvidedTemps;
		mExitProvidedTemps |= mEntryProvidedTemps;

		mExitPotentialTemps = mLocalProvidedTemps;
		mExitPotentialTemps |= mEntryPotentialTemps;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedTempSet(mExitProvidedTemps, mExitPotentialTemps);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedTempSet(mExitProvidedTemps, mExitPotentialTemps);
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
			else if (!tunified[ins->mSrc[i].mTemp])
				return false;
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

void InterCodeBasicBlock::CalculateSingleUsedTemps(FastNumberSet& fused, FastNumberSet& fsingle)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0)
				{
					if (fused[ins->mSrc[j].mTemp])
						fsingle -= ins->mSrc[j].mTemp;
					else
					{
						fused += ins->mSrc[j].mTemp;
						fsingle += ins->mSrc[j].mTemp;
					}
				}
			}
		}

		if (mTrueJump)
			mTrueJump->CalculateSingleUsedTemps(fused, fsingle);
		if (mFalseJump)
			mFalseJump->CalculateSingleUsedTemps(fused, fsingle);
	}
}

void InterCodeBasicBlock::CheckSingleAssignmentBools(GrowingInstructionPtrArray& tvalue)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			for (int k = 0; k < ins->mNumOperands; k++)
			{
				if (ins->mCode != IC_BRANCH && ins->mSrc[k].mTemp >= 0 && tvalue[ins->mSrc[k].mTemp])
					tvalue[ins->mSrc[k].mTemp] = nullptr;
			}
		}

		if (mTrueJump) mTrueJump->CheckSingleAssignmentBools(tvalue);
		if (mFalseJump) mFalseJump->CheckSingleAssignmentBools(tvalue);
	}
}

void InterCodeBasicBlock::MapSingleAssignmentBools(const GrowingInstructionPtrArray& tvalue)
{
	if (!mVisited)
	{
		mVisited = true;

		int sz = mInstructions.Size();
		if (sz > 0 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 1]->mSrc[0].mTemp >= 0 && tvalue[mInstructions[sz - 1]->mSrc[0].mTemp])
		{
			if (tvalue[mInstructions[sz - 1]->mSrc[0].mTemp]->mOperator == IA_CMPEQ)
			{
				InterCodeBasicBlock* tb = mTrueJump; mTrueJump = mFalseJump; mFalseJump = tb;
			}

			mInstructions[sz - 1]->mSrc[0] = tvalue[mInstructions[sz - 1]->mSrc[0].mTemp]->mDst;
		}

		if (mTrueJump) mTrueJump->MapSingleAssignmentBools(tvalue);
		if (mFalseJump) mFalseJump->MapSingleAssignmentBools(tvalue);
	}
}

void InterCodeBasicBlock::CollectSingleAssignmentBools(FastNumberSet& tassigned, GrowingInstructionPtrArray& tvalue)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			int	t = ins->mDst.mTemp;
			if (t >= 0)
			{
				if (!tassigned[t])
				{
					tassigned += t;

					if (ins->mCode == IC_RELATIONAL_OPERATOR && 
						(ins->mOperator == IA_CMPEQ || ins->mOperator == IA_CMPNE) &&
						(ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == 0 && ins->mSrc[1].IsUByte() || ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst == 0 && ins->mSrc[0].IsUByte()))
					{
						if (i + 2 == mInstructions.Size() && mInstructions[i + 1]->mCode == IC_BRANCH && mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp)
							;
						else
							tvalue[t] = ins;
					}
				}
				else
					tvalue[t] = nullptr;
			}
		}

		if (mTrueJump) mTrueJump->CollectSingleAssignmentBools(tassigned, tvalue);
		if (mFalseJump) mFalseJump->CollectSingleAssignmentBools(tassigned, tvalue);
	}
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
						if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE || ins->mCode == IC_ASSEMBLER || ins->mCode == IC_MALLOC || ins->mCode == IC_FREE)
							valid = false;
						else if (ins->mCode == IC_LOAD)
						{
							if (ins->mVolatile)
								valid = false;
							else if (ins->mSrc[0].mMemory == paramMemory)
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
						else if (ins->mCode == IC_CONSTANT)
						{
							if (ins->mConst.mType == IT_POINTER && ins->mConst.mMemory == paramMemory)
							{
								if (!modifiedParams[ins->mConst.mVarIndex])
								{
									modifiedParams += ins->mConst.mVarIndex;
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

void InterCodeBasicBlock::PerformTempForwarding(const TempForwardingTable& forwardingTable, bool reverse, bool checkloops)
{
	int i;

	if (!mVisited)
	{
		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this) && mLocalModifiedTemps.Size())
			{
				mMergeForwardingTable = forwardingTable;
				assert(mMergeForwardingTable.Size() == mLocalModifiedTemps.Size());

				for (int i = 0; i < mLocalModifiedTemps.Size(); i++)
				{
					if (mLocalModifiedTemps[i])
						mMergeForwardingTable.Destroy(i);
				}
			}
			else if (mLoopPrefix && checkloops)
			{
				ExpandingArray<InterCodeBasicBlock*> body;
				body.Push(this);
				bool	innerLoop = true;

				for (int i = 0; i < mEntryBlocks.Size(); i++)
				{
					if (mEntryBlocks[i] != mLoopPrefix)
					{
						if (!mEntryBlocks[i]->CollectLoopBodyRecursive(this, body))
							innerLoop = false;
					}
				}

				if (innerLoop)
				{
					mMergeForwardingTable = forwardingTable;
					assert(mMergeForwardingTable.Size() == mLocalModifiedTemps.Size());

					for (int j = 0; j < body.Size(); j++)
					{
						for (int i = 0; i < mLocalModifiedTemps.Size(); i++)
						{
							if (body[j]->mLocalModifiedTemps[i])
								mMergeForwardingTable.Destroy(i);
						}
					}
				}
				else
					mMergeForwardingTable.SetSize(forwardingTable.Size());
			}
			else
				mMergeForwardingTable.SetSize(forwardingTable.Size());
		}
		else
		{
			if (mNumEntered == 0)
				mMergeForwardingTable = forwardingTable;
			else
				mMergeForwardingTable.Intersect(forwardingTable);

			mNumEntered++;

			if (mNumEntered < mNumEntries)
				return;
		}

		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->PerformTempForwarding(mMergeForwardingTable, reverse);
		}

		if (mTrueJump) mTrueJump->PerformTempForwarding(mMergeForwardingTable, reverse, checkloops);
		if (mFalseJump) mFalseJump->PerformTempForwarding(mMergeForwardingTable, reverse, checkloops);

		mMergeForwardingTable.Shrink();
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredTempSet(NumberSet& fromRequiredTemps)
{
	bool revisit = false;

	if (!mVisited)
	{
		mVisited = true;

		mNewRequiredTemps = mExitRequiredTemps;

//		NumberSet	newRequiredTemps(mExitRequiredTemps);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredTempSet(mNewRequiredTemps)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredTempSet(mNewRequiredTemps)) revisit = true;

		if (!(mNewRequiredTemps <= mExitRequiredTemps))
		{
			revisit = true;

			mExitRequiredTemps = mNewRequiredTemps;
			mNewRequiredTemps -= mLocalProvidedTemps;
			mEntryRequiredTemps |= mNewRequiredTemps;
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

bool InterCodeBasicBlock::RemoveUnusedLocalStoreInstructions(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;
		if (mInstructions.Size() > 0 && mInstructions.Last()->mCode == IC_RETURN)
		{
			int i = mInstructions.Size();
			while (i > 0)
			{
				i--;
				InterInstruction* ins = mInstructions[i];
				if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_LOCAL)
				{
					ins->mCode = IC_NONE;
					ins->mNumOperands = 0;
					changed = true;
				}
				else if (ins->mCode == IC_LOAD)
					break;
				else if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
					break;
				else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY || ins->mCode == IC_FILL)
					break;
			}
		}

		if (mTrueJump && mTrueJump->RemoveUnusedLocalStoreInstructions())
			changed = true;
		if (mFalseJump && mFalseJump->RemoveUnusedLocalStoreInstructions())
			changed = true;
	}

	return false;
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

		mLocalRequiredStatics.Reset(staticVars.Size());
		mLocalProvidedStatics.Reset(staticVars.Size());

		mEntryRequiredStatics.Reset(staticVars.Size());
		mEntryProvidedStatics.Reset(staticVars.Size());
		mExitRequiredStatics.Reset(staticVars.Size());
		mExitProvidedStatics.Reset(staticVars.Size());

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

		NumberSet						requiredVars(mExitRequiredStatics);
		GrowingInstructionPtrArray		storeIns(nullptr);

		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			if (mInstructions[i]->RemoveUnusedStaticStoreInstructions(this, staticVars, requiredVars, storeIns))
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


void InterCodeBasicBlock::BuildStaticVariableByteSet(const GrowingVariableArray& staticVars, int bsize)
{
	if (!mVisited)
	{
		mVisited = true;

		mLocalRequiredStatics.Reset(bsize);
		mLocalProvidedStatics.Reset(bsize);

		mEntryRequiredStatics.Reset(bsize);
		mEntryProvidedStatics.Reset(bsize);
		mExitRequiredStatics.Reset(bsize);
		mExitProvidedStatics.Reset(bsize);

		for (int i = 0; i < mInstructions.Size(); i++)
			mInstructions[i]->FilterStaticVarsByteUsage(staticVars, mLocalRequiredStatics, mLocalProvidedStatics, mProc->mModule->mErrors);

		mEntryRequiredStatics = mLocalRequiredStatics;
		mExitProvidedStatics = mLocalProvidedStatics;

		if (mTrueJump) mTrueJump->BuildStaticVariableByteSet(staticVars, bsize);
		if (mFalseJump) mFalseJump->BuildStaticVariableByteSet(staticVars, bsize);
	}
}

bool InterCodeBasicBlock::RemoveUnusedStaticStoreByteInstructions(const GrowingVariableArray& staticVars, int bsize)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet						requiredVars(mExitRequiredStatics);

		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			if (mInstructions[i]->RemoveUnusedStaticStoreByteInstructions(this, staticVars, requiredVars))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedStaticStoreByteInstructions(staticVars, bsize))
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedStaticStoreByteInstructions(staticVars, bsize))
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

		mLocalRequiredVars.Reset(localVars.Size());
		mLocalProvidedVars.Reset(localVars.Size());

		mEntryRequiredVars.Reset(localVars.Size());
		mEntryProvidedVars.Reset(localVars.Size());
		mExitRequiredVars.Reset(localVars.Size());
		mExitProvidedVars.Reset(localVars.Size());

		mLocalRequiredParams.Reset(params.Size());
		mLocalProvidedParams.Reset(params.Size());

		mEntryRequiredParams.Reset(params.Size());
		mEntryProvidedParams.Reset(params.Size());
		mExitRequiredParams.Reset(params.Size());
		mExitProvidedParams.Reset(params.Size());

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

		NumberSet						requiredVars(mExitRequiredVars);
		NumberSet						requiredParams(mExitRequiredParams);	

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

void InterCodeBasicBlock::CollectUnusedRestricted(NumberSet& restrictSet)
{
	if (!mVisited)
	{
		mVisited = true;

		NumberSet	activeSet(restrictSet.Size());

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			const InterInstruction* ins(mInstructions[i]);

			switch (ins->mCode)
			{
			case IC_MALLOC:
				activeSet += ins->mDst.mRestricted;
				break;
			case IC_STORE:
				activeSet -= ins->mSrc[0].mRestricted;
				break;
			case IC_LEA:
				activeSet -= ins->mSrc[0].mRestricted;
				break;
			case IC_LOAD:
				activeSet -= ins->mSrc[0].mRestricted;
				break;
			case IC_COPY:
			case IC_STRCPY:
				activeSet -= ins->mSrc[0].mRestricted;
				break;
			case IC_FREE:
				if (activeSet[ins->mSrc[0].mRestricted])
					restrictSet += ins->mSrc[0].mRestricted;
				break;
			}
		}

		if (mTrueJump) mTrueJump->CollectUnusedRestricted(restrictSet);
		if (mFalseJump) mFalseJump->CollectUnusedRestricted(restrictSet);
	}
}

bool InterCodeBasicBlock::RemoveUnusedRestricted(const NumberSet& restrictSet)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			switch (ins->mCode)
			{
			case IC_STORE:
				if (ins->mSrc[0].mRestricted && restrictSet[ins->mSrc[0].mRestricted] ||
					ins->mSrc[1].mRestricted && restrictSet[ins->mSrc[1].mRestricted])
				{
					ins->mCode = IC_NONE;
					ins->mNumOperands = 0;
					changed = true;
				}
				break;
			case IC_COPY:
			case IC_STRCPY:
				if (ins->mSrc[1].mRestricted && restrictSet[ins->mSrc[1].mRestricted])
				{
					ins->mCode = IC_NONE;
					ins->mNumOperands = 0;
					changed = true;
				}
				break;
			case IC_FREE:
				if (ins->mSrc[0].mRestricted && restrictSet[ins->mSrc[0].mRestricted])
				{
					ins->mCode = IC_NONE;
					ins->mNumOperands = 0;
					changed = true;
				}
				break;
			case IC_MALLOC:
				if (ins->mDst.mRestricted && restrictSet[ins->mDst.mRestricted])
				{
					ins->mCode = IC_NONE;
					ins->mDst.mTemp = -1;
					ins->mNumOperands = 0;
					changed = true;
				}
				break;
			}
		}

		if (mTrueJump && mTrueJump->RemoveUnusedRestricted(restrictSet))
			changed = true;
		if (mFalseJump && mFalseJump->RemoveUnusedRestricted(restrictSet))
			changed = true;
	}

	return changed;

}

bool InterCodeBasicBlock::RemoveUnusedArgumentStoreInstructions(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		mEntryRequiredArgs.Reset(64, true);

		if (mTrueJump && mTrueJump == this)
		{
			if (mFalseJump)
			{
				if (mFalseJump->RemoveUnusedArgumentStoreInstructions())
					changed = true;
				mEntryRequiredArgs = mFalseJump->mEntryRequiredArgs;
			}
			else
				mEntryRequiredArgs.Reset(64, false);
		}
		else if (mFalseJump && mFalseJump == this)
		{
			if (mTrueJump->RemoveUnusedArgumentStoreInstructions())
				changed = true;
			mEntryRequiredArgs = mTrueJump->mEntryRequiredArgs;
		}
		else
		{
			if (mTrueJump && mTrueJump->RemoveUnusedArgumentStoreInstructions())
				changed = true;
			if (mFalseJump && mFalseJump->RemoveUnusedArgumentStoreInstructions())
				changed = true;

			if (mTrueJump)
			{
				mEntryRequiredArgs = mTrueJump->mEntryRequiredArgs;
				if (mFalseJump)
					mEntryRequiredArgs |= mTrueJump->mEntryRequiredArgs;
			}
			else
				mEntryRequiredArgs.Reset(64, false);
		}

		int i = mInstructions.Size() - 1;
		while (i >= 0)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
			{
				mEntryRequiredArgs.Fill();
			}
			else if (ins->mCode == IC_STORE && ins->mSrc[1].mMemory == IM_FFRAME && ins->mSrc[1].mVarIndex + int(ins->mSrc[1].mIntConst) + InterTypeSize[ins->mSrc[0].mType] < 64)
			{
				if (mEntryRequiredArgs.RangeClear(ins->mSrc[1].mVarIndex + int(ins->mSrc[1].mIntConst), InterTypeSize[ins->mSrc[0].mType]))
				{
					mInstructions.Remove(i);
					changed = true;
				}
				else
					mEntryRequiredArgs.SubRange(ins->mSrc[1].mVarIndex + int(ins->mSrc[1].mIntConst), InterTypeSize[ins->mSrc[0].mType]);
			}
			i--;
		}
	}

	return changed;
}

bool InterCodeBasicBlock::RemoveUnusedIndirectStoreInstructions(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		ExpandingArray<InterInstructionPtr>		stores;

		for (int i = mInstructions.Size() - 1; i >= 0; i--)
		{
			InterInstruction* ins = mInstructions[i];

			if (ins->mCode == IC_STORE)
			{
				int j = 0;
				while (j < stores.Size() && !SameMemAndSize(ins->mSrc[1], stores[j]->mSrc[1]))
					j++;

				if (j < stores.Size())
				{
					if (ins->mVolatile)
						stores[j] = ins;
					else
					{
						ins->mCode = IC_NONE;
						ins->mNumOperands = 0;
					}
				}
				else
				{
					j = 0;
					while (j < stores.Size())
					{
						if (CollidingMem(ins, stores[j]))
							stores.Remove(j);
						else
							j++;
					}
					stores.Push(ins);
				}
			}
			else if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
			{
				stores.SetSize(0);
			}
			else
			{
				int j = 0;
				while (j < stores.Size())
				{
					if (CollidingMem(ins, stores[j]) || ins->mDst.mTemp >= 0 && ins->mDst.mTemp == stores[j]->mSrc[1].mTemp)
						stores.Remove(j);
					else
						j++;
				}
			}
		}

		if (mTrueJump && mTrueJump->RemoveUnusedIndirectStoreInstructions())
			changed = true;
		if (mFalseJump && mFalseJump->RemoveUnusedIndirectStoreInstructions())
			changed = true;
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
				if (ins->mSrc[j].mTemp >= 0 && lavalue[ins->mSrc[j].mTemp])
				{
					InterInstruction* mins = lavalue[ins->mSrc[j].mTemp];

					if (mExitRequiredTemps[mins->mDst.mTemp] && !mExitRequiredTemps[mins->mSrc[0].mTemp])
					{
						ins->mSrc[j].ForwardTemp(mins->mDst);
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

bool InterCodeBasicBlock::EliminateIntegerSumAliasTemps(const GrowingInstructionPtrArray& tvalue)
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

			int dtemp = ins->mDst.mTemp;
			int stemp = -1;

			if (ins->mCode == IC_BINARY_OPERATOR &&
				ins->mOperator == IA_ADD &&
				ins->mSrc[0].mTemp < 0 &&
				ins->mSrc[1].mTemp >= 0)
			{
				stemp = ins->mSrc[1].mTemp;

				if (ins->mSrc[1].mFinal && ltvalue[stemp])
				{
					InterInstruction* sins = ltvalue[stemp];
					int64 diff = ins->mSrc[0].mIntConst - sins->mSrc[0].mIntConst;
					if (diff >= -128 && diff < 256 || ins->mSrc[0].mIntConst < -128 || ins->mSrc[0].mIntConst >= 256)
					{
						ins->mSrc[1] = sins->mDst;
						ins->mSrc[0].mIntConst = diff;
						changed = true;
					}

					stemp = -1;
				}
				else if (ins->mSrc[1].mFinal)
					stemp = -1;					
			}

			if (dtemp >= 0)
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (ltvalue[i] && ltvalue[i]->ReferencesTemp(dtemp))
						ltvalue[i] = nullptr;
				}
			}
			if (stemp >= 0)
				ltvalue[stemp] = ins;
		}


		if (mTrueJump && mTrueJump->EliminateIntegerSumAliasTemps(ltvalue))
			changed = true;

		if (mFalseJump && mFalseJump->EliminateIntegerSumAliasTemps(ltvalue))
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
				while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
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
								int64 diff = lins->mSrc[1].mIntConst - bins->mSrc[1].mIntConst;

								ins->mSrc[pi].mTemp = bins->mDst.mTemp;
								ins->mSrc[pi].mIntConst += diff;
								ins->mSrc[pi].mRange.AddConstValue(IT_INT16, -diff);
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

static bool CheckSimplifyPointerOffsets(const InterInstruction* ins, int temp, int& mino, int& maxo)
{
	if (ins->mDst.mTemp == temp)
		return false;

	if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp == temp)
	{
		if (ins->mSrc[0].mIntConst < mino)
			mino = int(ins->mSrc[0].mIntConst);
		if (ins->mSrc[0].mIntConst > maxo)
			maxo = int(ins->mSrc[0].mIntConst);

		return true;
	}

	if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp == temp)
	{
		if (ins->mSrc[0].mTemp == temp)
			return false;

		if (ins->mSrc[1].mIntConst < mino)
			mino = int(ins->mSrc[1].mIntConst);
		if (ins->mSrc[1].mIntConst > maxo)
			maxo = int(ins->mSrc[1].mIntConst);

		return true;
	}

	for (int i = 0; i < ins->mNumOperands; i++)
		if (ins->mSrc[i].mTemp == temp)
			return false;

	return true;
}

bool InterCodeBasicBlock::SimplifyPointerOffsets(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			
			if (ins->mCode == IC_LEA && (ins->mSrc[0].mTemp < 0 || ins->mSrc[1].mTemp < 0) && !mExitRequiredTemps[ins->mDst.mTemp])
			{
				int minoffset = 65535, maxoffset = -65535;

				int j = i + 1;
				while (j < mInstructions.Size() && CheckSimplifyPointerOffsets(mInstructions[j], ins->mDst.mTemp, minoffset, maxoffset))
					j++;

				if (j == mInstructions.Size() && (minoffset < 0 || maxoffset > 255) && maxoffset - minoffset < 256)
				{
					if (ins->mSrc[0].mTemp < 0)
						ins->mSrc[0].mIntConst += minoffset;
					else
						ins->mSrc[1].mIntConst += minoffset;

					changed = true;

					for (int j = i + 1; j < mInstructions.Size(); j++)
					{
						InterInstruction* tins = mInstructions[j];
						if (tins->mCode == IC_LOAD && tins->mSrc[0].mTemp == ins->mDst.mTemp)
							tins->mSrc[0].mIntConst -= minoffset;
						else if (tins->mCode == IC_STORE && tins->mSrc[1].mTemp == ins->mDst.mTemp)
							tins->mSrc[1].mIntConst -= minoffset;
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->SimplifyPointerOffsets())
			changed = true;
		if (mFalseJump && mFalseJump->SimplifyPointerOffsets())
			changed = true;
	}

	return true;
}

static bool IsValidSignedIntRange(InterType t, int64 value)
{
	switch (t)
	{
	case IT_INT8:
		return value >= -128 && value <= 127;
	case IT_INT16:
		return value >= -32768 && value <= 32767;
	case IT_INT32:
		return true;
	default:
		return false;
	}
}

bool InterCodeBasicBlock::ForwardShortLoadStoreOffsets(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* lins = mInstructions[i];
			if (lins->mCode == IC_LEA && lins->mSrc[1].mTemp >= 0 && lins->mSrc[0].mTemp < 0 && lins->mSrc[0].IsUByte())
			{
				for (int j = i + 1; j < mInstructions.Size(); j++)
				{
					InterInstruction* lins2 = mInstructions[j];
					if (lins2->mCode == IC_LEA && lins2->mSrc[1].mTemp == lins->mDst.mTemp && lins2->mSrc[0].mTemp >= 0)
					{
						int k = j + 1;
						while (k < mInstructions.Size() && !mInstructions[k]->ReferencesTemp(lins2->mDst.mTemp) && !mInstructions[k]->ReferencesTemp(lins->mSrc[1].mTemp))
							k++;
						if (k < mInstructions.Size())
						{
							InterInstruction* mins = mInstructions[k];
							if (mins->mCode == IC_LOAD && mins->mSrc[0].mTemp == lins2->mDst.mTemp && mins->mSrc[0].mFinal)
							{
								lins2->mSrc[1].mTemp = lins->mSrc[1].mTemp;
								lins->mSrc[1].mFinal = false;
								mins->mSrc[0].mIntConst += lins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (mins->mCode == IC_STORE && mins->mSrc[1].mTemp == lins2->mDst.mTemp && mins->mSrc[1].mFinal)
							{
								lins2->mSrc[1].mTemp = lins->mSrc[1].mTemp;
								lins->mSrc[1].mFinal = false;
								mins->mSrc[1].mIntConst += lins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (mins->mCode == IC_COPY && mins->mSrc[0].mTemp == lins2->mDst.mTemp && mins->mSrc[0].mFinal)
							{
								lins2->mSrc[1].mTemp = lins->mSrc[1].mTemp;
								lins->mSrc[1].mFinal = false;
								mins->mSrc[0].mIntConst += lins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (mins->mCode == IC_COPY && mins->mSrc[1].mTemp == lins2->mDst.mTemp && mins->mSrc[1].mFinal)
							{
								lins2->mSrc[1].mTemp = lins->mSrc[1].mTemp;
								lins->mSrc[1].mFinal = false;
								mins->mSrc[1].mIntConst += lins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (mins->mCode == IC_FILL && mins->mSrc[1].mTemp == lins2->mDst.mTemp && mins->mSrc[1].mFinal)
							{
								lins2->mSrc[1].mTemp = lins->mSrc[1].mTemp;
								lins->mSrc[1].mFinal = false;
								mins->mSrc[1].mIntConst += lins->mSrc[0].mIntConst;
								changed = true;
							}
						}
					}

					if (lins2->mDst.mTemp == lins->mDst.mTemp)
						break;
				}
			}
		}

		if (mTrueJump && mTrueJump->ForwardShortLoadStoreOffsets())
			changed = true;
		if (mFalseJump && mFalseJump->ForwardShortLoadStoreOffsets())
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

							if (ains->mCode == IC_BINARY_OPERATOR && (ains->mOperator == IA_ADD || ains->mOperator == IA_SUB) && ains->mSrc[0].mTemp < 0)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* cins = new InterInstruction(ins->mLocation, IC_CONVERSION_OPERATOR);
								cins->mOperator = IA_EXT8TO16U;
								cins->mSrc[0] = ains->mSrc[1];
								cins->mDst.mTemp = spareTemps++;
								cins->mDst.mType = IT_INT16;
								cins->mDst.mRange = ains->mSrc[1].mRange;
								cins->mDst.mRange.LimitMin(0);
								mInstructions.Insert(i, cins);

								InterInstruction* nins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
								nins->mOperator = IA_SHL;
								nins->mSrc[0] = ins->mSrc[0];
								nins->mSrc[1] = cins->mDst;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_INT16;
								if (cins->mDst.mRange.mMinState == IntegerValueRange::S_BOUND)
								{
									nins->mDst.mRange.mMinState = IntegerValueRange::S_BOUND;
									nins->mDst.mRange.mMinValue = cins->mDst.mRange.mMinValue << nins->mSrc[0].mIntConst;
								}
								if (cins->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND)
								{
									nins->mDst.mRange.mMaxState = IntegerValueRange::S_BOUND;
									nins->mDst.mRange.mMaxValue = cins->mDst.mRange.mMaxValue << nins->mSrc[0].mIntConst;
								}
								mInstructions.Insert(i + 1, nins);

								ins->mOperator = ains->mOperator;
								ins->mSrc[0] = ains->mSrc[0];
								ins->mSrc[0].mIntConst <<= nins->mSrc[0].mIntConst;
								ins->mSrc[0].mType = IT_INT16;
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
					else if (ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0 && 
						ltvalue[ins->mSrc[0].mTemp] && ltvalue[ins->mSrc[1].mTemp] &&
						ins->mSrc[0].mFinal && ins->mSrc[1].mFinal)
					{
						InterInstruction* ai0 = ltvalue[ins->mSrc[0].mTemp], * ai1 = ltvalue[ins->mSrc[1].mTemp];
						if (ai0->mCode == IC_BINARY_OPERATOR && ai0->mOperator == IA_SUB && ai0->mSrc[0].mTemp < 0 && ai0->mSrc[1].mFinal &&
							ai1->mCode == IC_BINARY_OPERATOR && ai1->mOperator == IA_SUB && ai1->mSrc[0].mTemp < 0 && ai1->mSrc[1].mFinal)
						{
							if (spareTemps + 2 >= ltvalue.Size())
								return true;

							InterInstruction* sins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							sins->mOperator = IA_SUB;
							sins->mDst = ins->mDst;
							sins->mSrc[1].mType = IT_INT16;
							sins->mSrc[1].mTemp = spareTemps++;
							sins->mSrc[1].mFinal = true;
							sins->mSrc[0] = ai0->mSrc[0];
							sins->mSrc[0].mIntConst += ai1->mSrc[0].mIntConst;
							sins->mSrc[0].mRange.AddConstValue(IT_INT16, -ai1->mSrc[0].mIntConst);
							mInstructions.Insert(i + 1, sins);

							ins->mDst = sins->mSrc[1];

							ai0->mCode = IC_LOAD_TEMPORARY;
							ai0->mSrc[0] = ai0->mSrc[1];
							ai0->mDst.mRange = ai0->mSrc[0].mRange;
							ai0->mNumOperands = 1;

							ai1->mCode = IC_LOAD_TEMPORARY;
							ai1->mSrc[0] = ai1->mSrc[1];
							ai1->mDst.mRange = ai1->mSrc[0].mRange;
							ai1->mNumOperands = 1;

							ins->mSrc[0].mRange = ai0->mDst.mRange;
							ins->mSrc[1].mRange = ai1->mDst.mRange;

							changed = true;
						}
					}


					break;
#endif
#if 1
				case IA_SUB:
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp] && ins->mSrc[1].mFinal)
					{
						InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

						if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD)
						{
							if (pins->mSrc[0].mTemp < 0)
							{
								ins->mOperator = IA_ADD;
								ins->mSrc[1].Forward(pins->mSrc[1]);
								pins->mSrc[1].mFinal = false;
								ins->mSrc[0].mIntConst = pins->mSrc[0].mIntConst - ins->mSrc[0].mIntConst;
								changed = true;
							}
							else if (pins->mSrc[1].mTemp < 0)
							{
								ins->mOperator = IA_ADD;
								ins->mSrc[1].Forward(pins->mSrc[0]);
								pins->mSrc[0].mFinal = false;
								ins->mSrc[0].mIntConst = pins->mSrc[1].mIntConst - ins->mSrc[0].mIntConst;
								changed = true;
							}
						}
					}

					break;
#endif
				}
			}	break;

			case IC_RELATIONAL_OPERATOR:
				if (ins->mOperator == IA_CMPLS || ins->mOperator == IA_CMPLES || ins->mOperator == IA_CMPGS || ins->mOperator == IA_CMPGES)
				{
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp])
					{
						InterInstruction* pins = ltvalue[ins->mSrc[1].mTemp];

						if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD)
						{
							if (pins->mSrc[0].mTemp < 0)
							{
								if (IsValidSignedIntRange(ins->mSrc[0].mType, ins->mSrc[0].mIntConst - pins->mSrc[0].mIntConst))
								{
									ins->mSrc[1].Forward(pins->mSrc[1]);
									pins->mSrc[1].mFinal = false;
									ins->mSrc[0].mIntConst -= pins->mSrc[0].mIntConst;
									changed = true;
								}
							}
							else if (pins->mSrc[1].mTemp < 0)
							{
								if (IsValidSignedIntRange(ins->mSrc[0].mType, ins->mSrc[0].mIntConst - pins->mSrc[1].mIntConst))
								{
									ins->mSrc[1].Forward(pins->mSrc[0]);
									pins->mSrc[0].mFinal = false;
									ins->mSrc[0].mIntConst -= pins->mSrc[1].mIntConst;
									changed = true;
								}
							}
						}
					}
				}
				break;

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
						ins->mSrc[1].mRange.AddConstValue(IT_INT16, -pins->mSrc[0].mIntConst);
						changed = true;
					}
#if 1
					else if (pins->mCode == IC_BINARY_OPERATOR && pins->mOperator == IA_ADD && pins->mSrc[1].mTemp < 0 && pins->mDst.mType == IT_INT16)
					{
						ins->mSrc[0] = pins->mSrc[0];
						ins->mSrc[1].mIntConst += pins->mSrc[1].mIntConst;
						ins->mSrc[1].mRange.AddConstValue(IT_INT16, -pins->mSrc[1].mIntConst);
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
								ins->mSrc[1].mRange.AddConstValue(IT_INT16, -ains->mSrc[0].mIntConst);
								changed = true;
							}
							else if (ains->mSrc[1].mType == IT_INT8)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* nins = new InterInstruction(ins->mLocation, IC_CONVERSION_OPERATOR);
								nins->mOperator = IA_EXT8TO16U;
								nins->mSrc[0] = ains->mSrc[1];
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_INT16;
								nins->mDst.mRange = ains->mSrc[1].mRange;
								mInstructions.Insert(i, nins);

								ins->mSrc[0] = nins->mDst;
								ins->mSrc[1].mIntConst += ains->mSrc[0].mIntConst;
								ins->mSrc[1].mRange.AddConstValue(IT_INT16, -ains->mSrc[0].mIntConst);
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
						ins->mSrc[1].mRange.AddConstValue(IT_INT16, -ins->mSrc[0].mIntConst);
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

					if (pins->mCode == IC_LEA)
					{
						if (ins->mSrc[1].mMemory == IM_INDIRECT)
						{
							ins->mSrc[1].mLinkerObject = pins->mSrc[1].mLinkerObject;
							ins->mSrc[1].mVarIndex = pins->mSrc[1].mVarIndex;
						}

						if (pins->mSrc[0].mTemp < 0 && ins->mSrc[1].mIntConst + pins->mSrc[0].mIntConst >= 0)
						{
							ins->mSrc[1].mMemory = pins->mSrc[1].mMemory;
							ins->mSrc[1].ForwardTemp(pins->mSrc[1]);
							pins->mSrc[1].mFinal = false;

							int64 diff = pins->mSrc[0].mIntConst + pins->mSrc[1].mIntConst;
							ins->mSrc[1].mIntConst += diff;
							ins->mSrc[1].mRange = pins->mSrc[1].mRange;
							changed = true;
						}
#if 1
						else if (pins->mSrc[1].mTemp < 0 && pins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mIntConst && (ins->mSrc[1].mIntConst >= 256 || pins->mSrc[0].IsUByte()))
						{
							int k = mInstructions.IndexOf(pins);
							if (k >= 0)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* nins = new InterInstruction(ins->mLocation, IC_LEA);
								nins->mSrc[0].Forward(pins->mSrc[0]);
								nins->mSrc[1].ForwardMem(pins->mSrc[1]);
								nins->mSrc[1].mIntConst += ins->mSrc[1].mIntConst;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_POINTER;
								nins->mDst.mRange = ins->mDst.mRange;

								ins->mSrc[1].mRange.Reset();
								ins->mSrc[1].mIntConst = 0;
								ins->mSrc[1].mTemp = nins->mDst.mTemp;

								mInstructions.Insert(k + 1, nins);
								changed = true;
							}
						}
#endif
					}
				}
				break;
#endif
#if 1
			case IC_LOAD:
				if (ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp])
				{
					InterInstruction* pins = ltvalue[ins->mSrc[0].mTemp];

					if (pins->mCode == IC_LEA)
					{
						if (ins->mSrc[0].mMemory == IM_INDIRECT)
						{
							ins->mSrc[0].mLinkerObject = pins->mSrc[1].mLinkerObject;
							ins->mSrc[0].mVarIndex = pins->mSrc[1].mVarIndex;
						}

						if (pins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst + pins->mSrc[0].mIntConst >= 0)
						{
							int64 offset = ins->mSrc[0].mIntConst + pins->mSrc[0].mIntConst;
							int osize = ins->mSrc[0].mOperandSize;
							ins->mSrc[0].ForwardMem(pins->mSrc[1]);
							ins->mSrc[0].mOperandSize = osize;
							pins->mSrc[1].mFinal = false;
							ins->mSrc[0].mIntConst += offset;
							ins->mSrc[0].mRange = pins->mSrc[1].mRange;
							changed = true;
						}
						else if (pins->mSrc[1].mTemp < 0 && pins->mSrc[0].mTemp >= 0 && ins->mSrc[0].mIntConst && (ins->mSrc[0].mIntConst >= 256 || pins->mSrc[0].IsUByte()))
						{
							int k = mInstructions.IndexOf(pins);
							if (k >= 0)
							{
								if (spareTemps + 2 >= ltvalue.Size())
									return true;

								InterInstruction* nins = new InterInstruction(ins->mLocation, IC_LEA);
								nins->mSrc[0].Forward(pins->mSrc[0]);
								nins->mSrc[1].ForwardMem(pins->mSrc[1]);
								nins->mSrc[1].mIntConst += ins->mSrc[0].mIntConst;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_POINTER;
								nins->mDst.mRange.Reset();
								ins->mSrc[0].mRange.Reset();
								ins->mSrc[0].mIntConst = 0;
								ins->mSrc[0].mTemp = nins->mDst.mTemp;

								mInstructions.Insert(k + 1, nins);
								changed = true;
							}
						}
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

void InterCodeBasicBlock::PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, int& spareTemps, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs)
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
				InterInstruction* mci0 = nullptr;
				InterInstruction* mci1 = nullptr;
				if (mi0 && mi0->mCode == IC_CONVERSION_OPERATOR)
					mci0 = ltvalue[mi0->mSrc[0].mTemp];
				if (mi1 && mi1->mCode == IC_CONVERSION_OPERATOR)
					mci1 = ltvalue[mi1->mSrc[0].mTemp];

				if (mi0 && mi1 && mi1->mCode == IC_CONSTANT && mi0->mCode == IC_BINARY_OPERATOR && mi0->mOperator == IA_ADD)
				{
					InterInstruction* ai0 = ltvalue[mi0->mSrc[0].mTemp], * ai1 = ltvalue[mi0->mSrc[1].mTemp];
					if (ai0 && ai0->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi0->mSrc[1].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[1].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
						InterInstruction* nai = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi0->mSrc[0].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[1].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
						InterInstruction* nai = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi1->mSrc[1].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[0].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
						InterInstruction* nai = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi1->mSrc[0].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[0].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
#if 1
				else if (mi0 && mi1 && mi0->mCode == IC_CONSTANT && mi1->mCode == IC_BINARY_OPERATOR && mi1->mOperator == IA_SUB)
				{
					InterInstruction* ai0 = ltvalue[mi1->mSrc[0].mTemp], * ai1 = ltvalue[mi1->mSrc[1].mTemp];
					if (ai0 && ai0->mCode == IC_CONSTANT)
					{
						InterInstruction* nai = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						nai->mOperator = IA_MUL;
						nai->mSrc[0].mTemp = mi1->mSrc[1].mTemp;
						nai->mSrc[0].mType = IT_INT16;
						nai->mSrc[1].mTemp = ins->mSrc[0].mTemp;
						nai->mSrc[1].mType = IT_INT16;
						nai->mDst.mTemp = spareTemps++;
						nai->mDst.mType = IT_INT16;
						mInstructions.Insert(i, nai);

						ltvalue[nai->mDst.mTemp] = nullptr;

						InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
						cai->mDst.mTemp = spareTemps++;
						cai->mDst.mType = IT_INT16;
						cai->mConst.mIntConst = ai0->mConst.mIntConst * mi0->mConst.mIntConst;
						mInstructions.Insert(i, cai);

						ltvalue[cai->mDst.mTemp] = nullptr;

						ins->mOperator = IA_SUB;
						ins->mSrc[1].mTemp = nai->mDst.mTemp;
						ins->mSrc[0].mTemp = cai->mDst.mTemp;
					}
				}
#endif
#if 1
				else if (mi0 && mci1 && mci1->mCode == IC_RELATIONAL_OPERATOR)
				{
					InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
					cai->mDst.mTemp = spareTemps++;
					cai->mDst.mType = ins->mDst.mType;
					cai->mConst.mIntConst = 0;
					cai->mConst.mFloatConst = 0;
					mInstructions.Insert(i, cai);

					ins->mCode = IC_SELECT;
					ins->mNumOperands = 3;
					ins->mSrc[2] = mci1->mDst;
					ins->mSrc[1] = ins->mSrc[0];
					ins->mSrc[0].mTemp = cai->mDst.mTemp;

					ltvalue[cai->mDst.mTemp] = nullptr;
				}
				else if (mci0 && mi1 && mci0->mCode == IC_RELATIONAL_OPERATOR)
				{
					InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
					cai->mDst.mTemp = spareTemps++;
					cai->mDst.mType = ins->mDst.mType;
					cai->mConst.mIntConst = 0;
					cai->mConst.mFloatConst = 0;
					mInstructions.Insert(i, cai);

					ins->mCode = IC_SELECT;
					ins->mNumOperands = 3;
					ins->mSrc[2] = mci0->mDst;
					ins->mSrc[0].mTemp = cai->mDst.mTemp;

					ltvalue[cai->mDst.mTemp] = nullptr;
				}
#endif
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
								InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
								InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
								InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
								InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
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
							InterInstruction* nai = new InterInstruction(ins->mLocation, IC_LEA);
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
						if (ai0 && ai1 && ai0->mCode == IC_CONSTANT)// && ai0->mConst.mIntConst >= 0)
						{
							InterInstruction* cai = new InterInstruction(ins->mLocation, IC_CONSTANT);
							cai->mDst.mTemp = spareTemps++;
							cai->mDst.mType = IT_INT16;
							cai->mConst.mIntConst = ai0->mConst.mIntConst + li0->mConst.mIntConst;
							cai->mConst.mType = IT_INT16;
							mInstructions.Insert(i, cai);

							ins->mSrc[0].mTemp = cai->mDst.mTemp;
							ins->mSrc[1].mTemp = li1->mSrc[1].mTemp;

							ltvalue[cai->mDst.mTemp] = nullptr;
						}
					}
				}
			}

#endif
			lvalues.UpdateValue(this, mInstructions[i], ltvalue, aliasedLocals, aliasedParams, staticVars, staticProcs);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams, spareTemps, staticVars, staticProcs);
		if (mFalseJump) mFalseJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams, spareTemps, staticVars, staticProcs);
	}
}

void InterCodeBasicBlock::PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs, FastNumberSet &fsingle)
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
			CheckValueUsage(mInstructions[i], ltvalue, staticVars, staticProcs, fsingle);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid, staticVars, staticProcs, fsingle);
		if (mFalseJump) mFalseJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid, staticVars, staticProcs, fsingle);
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

void InterCodeBasicBlock::LinkerObjectForwarding(const GrowingInstructionPtrArray& tvalue)
{
	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);

		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
			{
				mLoadStoreInstructions = tvalue;
				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins(mInstructions[i]);
					if (ins->mDst.mTemp >= 0)
						ltvalue[ins->mDst.mTemp] = nullptr;
				}
			}
			else
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
				return;
			}
		}

		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction	* ins(mInstructions[i]);
			InterInstruction* lins = nullptr;

			if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp])
					ins->mSrc[1].mLinkerObject = ltvalue[ins->mSrc[1].mTemp]->mSrc[1].mLinkerObject;

				if (ins->mSrc[1].mLinkerObject)
					lins = ins;				
			}
			else if (ins->mCode == IC_LOAD)
			{
				if (ins->mSrc[0].mTemp >= 0 && ltvalue[ins->mSrc[0].mTemp])
					ins->mSrc[0].mLinkerObject = ltvalue[ins->mSrc[0].mTemp]->mSrc[1].mLinkerObject;
			}
			else if (ins->mCode == IC_STORE)
			{
				if (ins->mSrc[1].mTemp >= 0 && ltvalue[ins->mSrc[1].mTemp])
					ins->mSrc[1].mLinkerObject = ltvalue[ins->mSrc[1].mTemp]->mSrc[1].mLinkerObject;
			}

			if (lins)
				ltvalue[lins->mDst.mTemp] = lins;
			else if (ins->mDst.mTemp >= 0)
				ltvalue[ins->mDst.mTemp] = nullptr;
		}

		if (mTrueJump) mTrueJump->LinkerObjectForwarding(ltvalue);
		if (mFalseJump) mFalseJump->LinkerObjectForwarding(ltvalue);
	}
}

void InterCodeBasicBlock::ReduceRecursionTempSpilling(InterMemory paramMemory, const GrowingInstructionPtrArray& tvalue)
{
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

						if (nins)
							mLoadStoreInstructions[i++] = nins;
						else
							mLoadStoreInstructions.Remove(i);
					}
				}

				mNumEntered++;

				if (mNumEntered < mNumEntries)
					return;
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
						if (mLoadStoreInstructions[j]->ReferencesTemp(ins->mDst.mTemp) || CollidingMem(ins, mLoadStoreInstructions[j]))
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

		NumberSet	rtemps(mEntryRequiredTemps);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			InterInstruction* lins = nullptr;
			bool			flushMem = false;

			if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
			{
				if (ins->mSrc[0].mLinkerObject == mProc->mLinkerObject)
				{
					for (int j = 0; j < mLoadStoreInstructions.Size(); j++)
					{
						if (rtemps[mLoadStoreInstructions[j]->mDst.mTemp])
							mInstructions.Insert(i + 1, mLoadStoreInstructions[j]->Clone());
					}
				}
			}
			else if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == paramMemory)
			{
				if (InterTypeSize[ins->mDst.mType] == ins->mSrc[0].mOperandSize)
					lins = ins;
			}

			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0 && ins->mSrc[j].mFinal)
					rtemps -= ins->mSrc[j].mTemp;
			}

			int	j = 0, k = 0, t = ins->mDst.mTemp;
			if (t >= 0 || IsObservable(ins->mCode))
			{
				while (j < mLoadStoreInstructions.Size())
				{
					if (DestroyingMem(mLoadStoreInstructions[j], ins))
						;
					else if (t != mLoadStoreInstructions[j]->mDst.mTemp)
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];

					j++;
				}
				mLoadStoreInstructions.SetSize(k);
			}

			if (lins)
				mLoadStoreInstructions.Push(lins);
		}

		if (mTrueJump) mTrueJump->ReduceRecursionTempSpilling(paramMemory, mLoadStoreInstructions);
		if (mFalseJump) mFalseJump->ReduceRecursionTempSpilling(paramMemory, mLoadStoreInstructions);
	}
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
								while (j < tvalue.Size() && !SameMemAndSize(ins->mSrc[0], tvalue[j]))
									j++;
								if (j < tvalue.Size())
								{
									InterInstruction* tins = tvalue[j];

									if (tvalue[j]->mCode == IC_LOAD && tins->mDst.IsEqual(ins->mDst))
										nins = ins;
									else if (tvalue[j]->mCode == IC_STORE && tins->mSrc[0].IsEqual(ins->mDst))
										nins = ins;
								}
							}
							else if (ins->mCode == IC_STORE)
							{
								j = 0;
								while (j < tvalue.Size() && !SameMemAndSize(ins->mSrc[1], tvalue[j]))
									j++;
								if (j < tvalue.Size())
								{
									InterInstruction* tins = tvalue[j];

									if (tvalue[j]->mCode == IC_LOAD && tvalue[j]->mDst.IsEqual(ins->mSrc[0]))
										nins = tvalue[j];
									else if (tvalue[j]->mCode == IC_STORE && tvalue[j]->mSrc[0].IsEqual(ins->mSrc[0]))
									{
										nins = ins;
										if (ins->mSrc[1].mTemp >= 0 && tins->mSrc[1].mTemp >= 0)
										{
											if (ins->mSrc[1].mMemoryBase != tins->mSrc[1].mMemoryBase ||
												ins->mSrc[1].mMemoryBase == IM_GLOBAL && ins->mSrc[1].mVarIndex != tins->mSrc[1].mVarIndex)
											{
												nins = ins->Clone();
												nins->mSrc[1].mMemoryBase = IM_INDIRECT;
											}
										}
									}
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
				int j = 0;
				while (j < mLoadStoreInstructions.Size())
				{
					if (InvalidatedBy(mLoadStoreInstructions[j], ins))
						mLoadStoreInstructions.Remove(j);
					else
						j++;
				}
			}
		} 
#if 1
		else if (tvalue.Size() > 0)
		{
			ExpandingArray<InterCodeBasicBlock*> body;
			body.Push(this);
			bool	innerLoop = true;
			int		n = 1;

			for (int i = 0; i < mEntryBlocks.Size(); i++)
			{
				if (IsDominator( mEntryBlocks[i] ))
				{
					n++;
					if (!mEntryBlocks[i]->CollectLoopBodyRecursive(this, body))
						innerLoop = false;
				}
			}

			if (innerLoop && n == mEntryBlocks.Size() )
			{
				mLoadStoreInstructions = tvalue;
				for (int j = 0; j < body.Size(); j++)
				{
					for (int i = 0; i < body[j]->mInstructions.Size(); i++)
					{
						InterInstruction* ins(body[j]->mInstructions[i]);
						int j = 0;
						while (j < mLoadStoreInstructions.Size())
						{
							if (InvalidatedBy(mLoadStoreInstructions[j], ins))
								mLoadStoreInstructions.Remove(j);
							else
								j++;
						}
					}
				}
			}
			else
				mLoadStoreInstructions.SetSize(0);
		}
#endif
#endif
		else
			mLoadStoreInstructions.SetSize(0);

		mVisited = true;

#if 0
		for (int i = 0; i < mLoadStoreInstructions.Size(); i++)
		{
			if (mLoadStoreInstructions[i]->mCode == IC_STORE && 
				mLoadStoreInstructions[i]->mSrc[1].mTemp < 0 && 
				mLoadStoreInstructions[i]->mSrc[1].mMemory == IM_GLOBAL &&
				mLoadStoreInstructions[i]->mSrc[1].mVarIndex == 68)
				printf("I:%d\n", mIndex);
		}
#endif

#if 1
		// move loads up as far as possible to avoid false aliasing
		for (int i = 1; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_LOAD)
			{
				int j = i;
				while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
				{
					SwapInstructions(mInstructions[j - 1], ins);
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
		}
#endif

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
					while (j < mLoadStoreInstructions.Size() && !SameMemAndSize(ins->mSrc[0], mLoadStoreInstructions[j]))
						j++;
					if (j < mLoadStoreInstructions.Size())
					{
						InterInstruction* lins = mLoadStoreInstructions[j];
						if (lins->mCode == IC_LOAD)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							ins->mSrc[0] = lins->mDst;
							ins->mSrc[0].mRestricted = ins->mDst.mRestricted = lins->mDst.mRestricted;							
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
								ins->mNumOperands = 0;
								changed = true;
							}
							else
							{
								ins->mCode = IC_LOAD_TEMPORARY;
								ins->mSrc[0] = lins->mSrc[0];
								ins->mSrc[0].mRestricted = ins->mDst.mRestricted = lins->mSrc[0].mRestricted;
								ins->mDst.mRange.Limit(ins->mSrc[0].mRange);
								ins->mNumOperands = 1;
								assert(ins->mSrc[0].mTemp >= 0);
								changed = true;
							}
						}
					}
					else
					{
						j = 0;
						while (j < mLoadStoreInstructions.Size() && !(
							mLoadStoreInstructions[j]->mCode == IC_COPY && SameMemSegment(mLoadStoreInstructions[j]->mSrc[1], ins->mSrc[0]) ||
							mLoadStoreInstructions[j]->mCode == IC_FILL && SameMemSegment(mLoadStoreInstructions[j]->mSrc[1], ins->mSrc[0])))
							j++;
						if (j < mLoadStoreInstructions.Size())
						{
							InterInstruction* cins = mLoadStoreInstructions[j];

							if (cins->mCode == IC_FILL)
							{
								int64	v = 0;
								for (int j = 0; j < InterTypeSize[ins->mDst.mType]; j++)
									v = (v << 8) | (cins->mSrc[0].mIntConst & 0xff);

								ins->mCode = IC_CONSTANT;
								ins->mConst.mType = ins->mDst.mType;
								ins->mConst.mIntConst = v;
								ins->mNumOperands = 0;
								changed = true;
							}
							else
							{
								int64	offset = ins->mSrc[0].mIntConst - cins->mSrc[1].mIntConst;
								ins->mSrc[0] = cins->mSrc[0];
								ins->mSrc[0].mOperandSize = InterTypeSize[ins->mDst.mType];
								ins->mSrc[0].mIntConst += offset;
								changed = true;
							}
						}
						else
							nins = ins;
					}
				}
			}
			else if (ins->mCode == IC_STORE)
			{
				int	j = 0, k = 0;

				while (j < mLoadStoreInstructions.Size() && !SameMemAndSize(ins->mSrc[1], mLoadStoreInstructions[j]))
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
						if (!DestroyingMem(mLoadStoreInstructions[j], ins))
							mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
						j++;
					}
					mLoadStoreInstructions.SetSize(k);
				}

				if (!ins->mVolatile)
					nins = ins;
			}
			else if (ins->mCode == IC_COPY)
			{
				int	j = 0, k = 0;
				while (j < mLoadStoreInstructions.Size())
				{
					if (!DestroyingMem(mLoadStoreInstructions[j], ins))
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					j++;
				}
				mLoadStoreInstructions.SetSize(k);
#if 1
				uint64	fillmask = 0;
				if (ins->mSrc[0].mOperandSize < 64 && ins->mSrc[0].mStride == 1 && ins->mSrc[1].mStride == 1)
				{
					for (int j = 0; j < mLoadStoreInstructions.Size(); j++)
					{
						InterInstruction* sins = mLoadStoreInstructions[j];
						if (sins->mCode == IC_STORE && SameMemSegment(ins->mSrc[0], sins->mSrc[1]))
						{
							int64	offset = sins->mSrc[1].mIntConst - ins->mSrc[0].mIntConst;
							if (offset >= 0 && offset < ins->mSrc[0].mOperandSize)
							{
								for (int k = 0; k < InterTypeSize[sins->mSrc[0].mType]; k++)
									fillmask |= 1ULL << (k + offset);
							}
						}
					}

					if (fillmask + 1 == (1ULL << ins->mSrc[0].mOperandSize))
					{
						int n = 0;
						for (int j = 0; j < mLoadStoreInstructions.Size(); j++)
						{
							InterInstruction* sins = mLoadStoreInstructions[j];
							if (sins->mCode == IC_STORE && SameMemSegment(ins->mSrc[0], sins->mSrc[1]))
							{
								int64	offset = sins->mSrc[1].mIntConst - ins->mSrc[0].mIntConst;
								if (offset >= 0 && offset < ins->mSrc[0].mOperandSize)
								{
									InterInstruction* tins = sins->Clone();
									tins->mSrc[1] = ins->mSrc[1];
									tins->mSrc[1].mOperandSize = sins->mSrc[1].mOperandSize;
									tins->mSrc[1].mIntConst = sins->mSrc[1].mIntConst - ins->mSrc[0].mIntConst + ins->mSrc[1].mIntConst;
									n++;
									mInstructions.Insert(i + n, tins);
								}
							}
						}
						ins->mCode = IC_NONE;
						ins->mNumOperands = 0;
					}
					else if (!ins->mVolatile && ins->mSrc[0].mStride == 1 && ins->mSrc[1].mStride == 1)
						nins = ins;
				}
				else if (!ins->mVolatile && ins->mSrc[0].mStride == 1 && ins->mSrc[1].mStride == 1)
					nins = ins;
#endif
			}
			else if (ins->mCode == IC_FILL)
			{
				int	j = 0, k = 0;
				while (j < mLoadStoreInstructions.Size())
				{
					if (!DestroyingMem(mLoadStoreInstructions[j], ins))
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					j++;
				}
				mLoadStoreInstructions.SetSize(k);

				if (!ins->mVolatile && ins->mSrc[1].mStride == 1 && ins->mSrc[0].mTemp < 0)
					nins = ins;
			}
			else if (ins->mCode == IC_STRCPY)
			{
				int	j = 0, k = 0;
				while (j < mLoadStoreInstructions.Size())
				{
					if (!DestroyingMem(mLoadStoreInstructions[j], ins))
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					j++;
				}
				mLoadStoreInstructions.SetSize(k);
			}
			else if (ins->mCode == IC_LEA || ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR || ins->mCode == IC_CONVERSION_OPERATOR)
			{
			//
				int	j = 0;
				while (j < mLoadStoreInstructions.Size() && !SameInstruction(ins, mLoadStoreInstructions[j]))
					j++;
				if (j < mLoadStoreInstructions.Size())
				{
					InterInstruction* lins = mLoadStoreInstructions[j];
					assert(lins->mDst.mTemp >= 0);
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSrc[0] = lins->mDst;
					ins->mSrc[0].mRestricted = ins->mDst.mRestricted = lins->mDst.mRestricted;
					ins->mDst.mRange.Limit(ins->mSrc[0].mRange);
					ins->mNumOperands = 1;
					changed = true;
				}
				else if (ins->mCode == IC_LEA && ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_ABSOLUTE && ins->mSrc[0].mTemp >= 0)
				{
					int	offset = int(ins->mSrc[1].mIntConst);

					j = 0;
					while (j < mLoadStoreInstructions.Size() && !(
						mLoadStoreInstructions[j]->mCode == IC_LEA && mLoadStoreInstructions[j]->mSrc[1].mTemp < 0 && mLoadStoreInstructions[j]->mSrc[1].mMemory == IM_ABSOLUTE &&
						mLoadStoreInstructions[j]->mSrc[0].mTemp == ins->mSrc[0].mTemp &&
						(((mLoadStoreInstructions[j]->mSrc[0].mIntConst + mLoadStoreInstructions[j]->mSrc[1].mIntConst - offset) & 255) == 0)))
						j++;
					if (j < mLoadStoreInstructions.Size())
					{
						ins->mSrc[1] = mLoadStoreInstructions[j]->mDst;
						ins->mSrc[1].mMemory = IM_INDIRECT;
						ins->mSrc[1].mIntConst = 0;

						ins->mSrc[0].mTemp = -1;
						ins->mSrc[0].mIntConst = offset - mLoadStoreInstructions[j]->mSrc[1].mIntConst;
						changed = true;
					}
					else
					{
						if (i + 1 < mInstructions.Size() &&
							(mInstructions[i + 1]->mCode == IC_STORE && mInstructions[i + 1]->mSrc[1].mTemp == ins->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal ||
								mInstructions[i + 1]->mCode == IC_LOAD && mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal))
						{
							int64 loffset = (mInstructions[i + 1]->mCode == IC_STORE ? mInstructions[i + 1]->mSrc[1].mIntConst : mInstructions[i + 1]->mSrc[0].mIntConst) + offset;

							j = 0;
							while (j < mLoadStoreInstructions.Size() && !(
								mLoadStoreInstructions[j]->mCode == IC_LEA && mLoadStoreInstructions[j]->mSrc[1].mTemp < 0 && mLoadStoreInstructions[j]->mSrc[1].mMemory == IM_ABSOLUTE &&
								mLoadStoreInstructions[j]->mSrc[0].mTemp == ins->mSrc[0].mTemp &&
								loffset - mLoadStoreInstructions[j]->mSrc[0].mIntConst - mLoadStoreInstructions[j]->mSrc[1].mIntConst >= 0 &&
								loffset - mLoadStoreInstructions[j]->mSrc[0].mIntConst - mLoadStoreInstructions[j]->mSrc[1].mIntConst < 255))
								j++;

							if (j < mLoadStoreInstructions.Size())
							{
								InterInstruction* lins = mLoadStoreInstructions[j];
								assert(lins->mDst.mTemp >= 0);
								ins->mCode = IC_LOAD_TEMPORARY;
								ins->mSrc[0] = lins->mDst;
								ins->mSrc[0].mRestricted = ins->mDst.mRestricted = lins->mDst.mRestricted;
								ins->mDst.mRange = lins->mDst.mRange;
								ins->mNumOperands = 1;

								if (mInstructions[i + 1]->mCode == IC_STORE)
								{
									mInstructions[i + 1]->mSrc[1].mRange = ins->mDst.mRange;
									mInstructions[i + 1]->mSrc[1].mIntConst = loffset - mLoadStoreInstructions[j]->mSrc[0].mIntConst - mLoadStoreInstructions[j]->mSrc[1].mIntConst;
								}
								else
								{
									mInstructions[i + 1]->mSrc[0].mRange = ins->mDst.mRange;
									mInstructions[i + 1]->mSrc[0].mIntConst = loffset - mLoadStoreInstructions[j]->mSrc[0].mIntConst - mLoadStoreInstructions[j]->mSrc[1].mIntConst;
								}

								changed = true;
							}
							else
								nins = ins;
						}
						else
							nins = ins;
					}
				}
				else
					nins = ins;
			}
			else if (ins->mCode == IC_CALL_NATIVE && ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mLinkerObject && ins->mSrc[0].mLinkerObject->mProc && ins->mSrc[0].mLinkerObject->mProc->mGlobalsChecked)
			{
				InterCodeProcedure* proc = ins->mSrc[0].mLinkerObject->mProc;

				int	j = 0, k = 0;
				while (j < mLoadStoreInstructions.Size())
				{

					int		opmask = 0;

					InterInstruction* lins = mLoadStoreInstructions[j];

					if (lins->mCode == IC_LOAD)
						opmask = 1;
					else if (lins->mCode == IC_STORE || lins->mCode == IC_FILL)
						opmask = 2;
					else if (lins->mCode == IC_COPY)
						opmask = 3;

					bool	flush = false;
					for(int k=0; k<lins->mNumOperands; k++)
					{
						if ((1 << k) & opmask)
						{
							InterOperand& op(lins->mSrc[k]);

							if (op.mTemp >= 0)
							{
								if (op.mMemoryBase == IM_GLOBAL)
									flush = proc->ModifiesGlobal(op.mVarIndex);
								else if (op.mMemoryBase == IM_LOCAL && !mProc->mLocalVars[op.mVarIndex]->mAliased)
									flush = false;
								else if ((op.mMemoryBase == IM_PARAM || op.mMemoryBase == IM_FPARAM) && !mProc->mParamVars[op.mVarIndex]->mAliased)
									flush = false;
								else
									flush = proc->mStoresIndirect;
							}
							else if (op.mMemory == IM_FFRAME || op.mMemory == IM_FRAME)
								flush = true;
							else if (op.mMemory == IM_GLOBAL)
								flush = proc->ModifiesGlobal(op.mVarIndex);
							else if (op.mMemory == IM_LOCAL && !mProc->mLocalVars[op.mVarIndex]->mAliased)
								flush = false;
							else if ((op.mMemory == IM_PARAM || op.mMemory == IM_FPARAM) && !mProc->mParamVars[op.mVarIndex]->mAliased)
								flush = false;
							else
								flush = true;
						}
					}

					if (!flush)
						mLoadStoreInstructions[k++] = mLoadStoreInstructions[j];
					j++;
				}
				mLoadStoreInstructions.SetSize(k);
			}
			else if (HasSideEffect(ins->mCode))
				flushMem = true;			

			{
				int	j = 0, k = 0, t = ins->mDst.mTemp;

				while (j < mLoadStoreInstructions.Size())
				{
					if (flushMem && (mLoadStoreInstructions[j]->mCode == IC_LOAD || mLoadStoreInstructions[j]->mCode == IC_STORE || mLoadStoreInstructions[j]->mCode == IC_COPY || mLoadStoreInstructions[j]->mCode == IC_FILL))
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

				if (nins && t >= 0)
				{
					// Check self destruction of source operand
					int l = 0;
					while (l < nins->mNumOperands && t != nins->mSrc[l].mTemp)
						l++;
					if (l != nins->mNumOperands)
						nins = nullptr;
				}
			}

			if (nins)
				mLoadStoreInstructions.Push(nins);
		}

#if 1
		int sz = mInstructions.Size() - 1;
		if (sz > 2 && mInstructions[sz]->mCode == IC_BRANCH && mInstructions[sz - 1]->mCode == IC_RELATIONAL_OPERATOR)
			sz--;

		// move loads down as far as possible to avoid false aliasing
		for (int i = mInstructions.Size() - 2; i >= 0; i--)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_LOAD)
			{
				int j = i;
				while (j + 1 < sz && CanSwapInstructions(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
		}
#endif
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

		RenameValueRanges(mEntryRenameTable, num);

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

		RenameValueRanges(renameTable, temporaries.Size());

		if (mTrueJump) mTrueJump->GlobalRenameRegister(renameTable, temporaries);
		if (mFalseJump) mFalseJump->GlobalRenameRegister(renameTable, temporaries);
	}
}

void InterCodeBasicBlock::RenameValueRanges(const GrowingIntArray& renameTable, int numTemps)
{
	if (mEntryValueRange.Size() > 0)
	{
		mProc->mLocalValueRange = mEntryValueRange;
		GrowingArray<int64>			memoryValueSize(mMemoryValueSize);

		mEntryValueRange.SetSize(numTemps, true);
		mMemoryValueSize.SetSize(numTemps, true);
		for (int i = 0; i < mProc->mLocalValueRange.Size(); i++)
		{
			if (renameTable[i] >= 0)
			{
				assert(mProc->mLocalValueRange[i].mMinState == IntegerValueRange::S_UNKNOWN || mEntryValueRange[renameTable[i]].mMinState == IntegerValueRange::S_UNKNOWN);
				assert(mProc->mLocalValueRange[i].mMaxState == IntegerValueRange::S_UNKNOWN || mEntryValueRange[renameTable[i]].mMaxState == IntegerValueRange::S_UNKNOWN);

				mEntryValueRange[renameTable[i]].Limit(mProc->mLocalValueRange[i]);
				mMemoryValueSize[renameTable[i]] = int64min(mMemoryValueSize[renameTable[i]], memoryValueSize[i]);
			}				
		}
	}
}

void InterCodeBasicBlock::BuildFriendsTable(GrowingIntArray& friendsMap)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			int di = ins->mDst.mTemp, si = -1;

			if (di >= 0)
			{
				if (ins->mCode == IC_LOAD_TEMPORARY)
					si = ins->mSrc[0].mTemp;
				else if (ins->mCode == IC_LEA && ins->mSrc[1].mFinal)
					si = ins->mSrc[1].mTemp;
				else if (ins->mCode == IC_UNARY_OPERATOR && ins->mSrc[0].mFinal)
					si = ins->mSrc[0].mTemp;
				else if (ins->mCode == IC_BINARY_OPERATOR)
				{
					if (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mFinal)
						si = ins->mSrc[1].mTemp;
					else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mFinal)
						si = ins->mSrc[0].mTemp;
				}

				if (si != di)
				{
					if (si >= 0 && friendsMap[di] == -1)
						friendsMap[di] = si;
					else
						friendsMap[di] = -2;
				}
			}
		}

		if (mTrueJump) mTrueJump->BuildFriendsTable(friendsMap);
		if (mFalseJump) mFalseJump->BuildFriendsTable(friendsMap);
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
			case IC_LEA:
			case IC_FILL:
				if (mInstructions[i]->mSrc[1].mTemp < 0 && mInstructions[i]->mSrc[1].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[1].mVarIndex]->mUsed = true;
				}
				break;

			case IC_LOAD:
			case IC_CALL_NATIVE:
				if (mInstructions[i]->mSrc[0].mTemp < 0 && mInstructions[i]->mSrc[0].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[0].mVarIndex]->mUsed = true;
				}
				break;

			case IC_COPY:
			case IC_STRCPY:
				if (mInstructions[i]->mSrc[0].mTemp < 0 && mInstructions[i]->mSrc[0].mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mSrc[0].mVarIndex]->mUsed = true;
				}
				if (mInstructions[i]->mSrc[1].mTemp < 0 && mInstructions[i]->mSrc[1].mMemory == IM_LOCAL)
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
					ins->mSrc[0].mLinkerObject->MarkRelevant();
			}
			else if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL)
					ins->mSrc[1].mLinkerObject->MarkRelevant();
			}
			else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER)
			{
				if (ins->mConst.mMemory == IM_GLOBAL)
					ins->mConst.mLinkerObject->MarkRelevant();
			}
			else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
			{
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL)
					ins->mSrc[0].mLinkerObject->MarkRelevant();
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL)
					ins->mSrc[1].mLinkerObject->MarkRelevant();
			}
		}

		if (mTrueJump) mTrueJump->MarkRelevantStatics();
		if (mFalseJump) mFalseJump->MarkRelevantStatics();
	}
}

bool InterCodeBasicBlock::IsInsModified(const InterInstruction* ins)
{
	return IsInsModifiedInRange(0, mInstructions.Size(), ins);
}

bool InterCodeBasicBlock::IsInsModifiedInRange(int from, int to, const InterInstruction* ins)
{
	if (ins->mDst.mTemp >= 0 && IsTempModifiedInRange(from, to, ins->mDst.mTemp))
		return true;

	for (int i = 0; i < ins->mNumOperands; i++)
	{
		if (ins->mSrc[i].mTemp >= 0 && IsTempModifiedInRange(from, to, ins->mSrc[i].mTemp))
			return true;
	}

	return false;
}

bool InterCodeBasicBlock::IsTempModified(int temp)
{
	return IsTempModifiedInRange(0, mInstructions.Size(), temp);
}

bool InterCodeBasicBlock::IsTempModifiedInRange(int from, int to, int temp)
{
	for (int i = from; i < to; i++)
		if (mInstructions[i]->mDst.mTemp == temp)
			return true;
	return false;
}

bool InterCodeBasicBlock::IsTempUsedInRange(int from, int to, int temp)
{
	for (int i = from; i < to; i++)
	{
		InterInstruction* ins = mInstructions[i];
		for (int j = 0; j < ins->mNumOperands; j++)
			if (ins->mSrc[j].mTemp == temp)
				return true;
	}
	return false;
}

bool InterCodeBasicBlock::IsTempReferenced(int temp)
{
	return IsTempReferencedInRange(0, mInstructions.Size(), temp);
}

bool InterCodeBasicBlock::IsTempReferencedInRange(int from, int to, int temp)
{
	for (int i = from; i < to; i++)
	{
		InterInstruction* ins = mInstructions[i];
		if (ins->mDst.mTemp == temp)
			return true;
		for (int j = 0; j < ins->mNumOperands; j++)
			if (ins->mSrc[j].mTemp == temp)
				return true;
	}
	return false;
}

InterInstruction* InterCodeBasicBlock::FindTempOrigin(int temp) const
{
	for (int i = mInstructions.Size() - 1; i >= 0; i--)
	{
		if (mInstructions[i]->mDst.mTemp == temp)
		{
			mMark = i;
			return mInstructions[i];
		}
	}
	return nullptr;	
}

InterInstruction* InterCodeBasicBlock::FindTempOriginSinglePath(int temp) const
{
	for (int i = mInstructions.Size() - 1; i >= 0; i--)
	{
		if (mInstructions[i]->mDst.mTemp == temp)
		{
			mMark = i;
			return mInstructions[i];
		}
	}
	if (mEntryBlocks.Size() == 1)
		return mEntryBlocks[0]->FindTempOriginSinglePath(temp);
	return nullptr;
}

bool InterCodeBasicBlock::CanMoveInstructionDown(int si, int ti) const
{
	InterInstruction* ins = mInstructions[si];

#if 1
	if (ins->mCode == IC_COPY || ins->mCode == IC_PUSH_FRAME || ins->mCode == IC_POP_FRAME || ins->mCode == IC_FILL ||
		ins->mCode == IC_RETURN || ins->mCode == IC_RETURN_STRUCT || ins->mCode == IC_RETURN_VALUE || ins->mCode == IC_DISPATCH ||
		ins->mCode == IC_BREAKPOINT)
		return false;

	for (int i = si + 1; i < ti; i++)
		if (!CanSwapInstructions(ins, mInstructions[i]))
			return false;
	return true;

#else

	if (ins->mCode == IC_LOAD)
	{
		for (int i = si + 1; i < ti; i++)
			if (!CanBypassLoad(ins, mInstructions[i]))
				return false;
	}
	else if (ins->mCode == IC_STORE)
	{
		for (int i = si + 1; i < ti; i++)
			if (!CanBypassStore(ins, mInstructions[i]))
				return false;
	}
	else if (ins->mCode == IC_COPY || ins->mCode == IC_PUSH_FRAME || ins->mCode == IC_POP_FRAME ||
		ins->mCode == IC_RETURN || ins->mCode == IC_RETURN_STRUCT || ins->mCode == IC_RETURN_VALUE)
		return false;
	else
	{
		for (int i = si + 1; i < ti; i++)
			if (!CanBypass(ins, mInstructions[i]))
				return false;
	}

	return true;
#endif
}

int InterCodeBasicBlock::FindSameInstruction(const InterInstruction* ins) const
{
	int	i = mInstructions.Size() - 1;
	while (i >= 0 && !mInstructions[i]->IsEqual(ins))
		i--;
	return i;
}

bool InterCodeBasicBlock::CanMoveInstructionBehindBlock(int ii) const
{
	if (CanMoveInstructionDown(ii, mInstructions.Size() - 1))
	{
		InterInstruction* ins = mInstructions.Last();
		if (ins->mCode == IC_BRANCH && mInstructions[ii]->mDst.mTemp == ins->mSrc[0].mTemp)
			return false;
		return true;
	}
	else
		return false;
}

bool InterCodeBasicBlock::CanMoveInstructionBeforeBlock(int ii, const InterInstruction* ins) const
{

#if 1
	if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE || ins->mCode == IC_COPY || ins->mCode == IC_PUSH_FRAME || ins->mCode == IC_POP_FRAME || ins->mCode == IC_FILL ||
		ins->mCode == IC_RETURN || ins->mCode == IC_RETURN_STRUCT || ins->mCode == IC_RETURN_VALUE || ins->mCode == IC_DISPATCH || ins->mCode == IC_BREAKPOINT)
		return false;

	for (int i = 0; i < ii; i++)
		if (!CanSwapInstructions(mInstructions[i], ins))
			return false;
	return true;
#else
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
#endif
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
					while (fi < mFalseJump->mInstructions.Size() && !(tins->mCode == mFalseJump->mInstructions[fi]->mCode && tins->mDst.mType == mFalseJump->mInstructions[fi]->mDst.mType && tins->IsEqualSource(mFalseJump->mInstructions[fi])))
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
								if (mInstructions[tindex]->mCode != IC_BRANCH || tins->mDst.mTemp != mInstructions[tindex]->mSrc[0].mTemp)
								{
									if (mInstructions.Size() >= 2 && mInstructions[tindex - 1]->mDst.mTemp == mInstructions[tindex]->mSrc[0].mTemp &&
										CanSwapInstructions(mInstructions[tindex - 1], tins))
										//									CanBypassUp(tins, mInstructions[tindex - 1]))
										tindex--;

									mInstructions.Insert(tindex, tins);
									tindex++;
									if (tins->mDst.mTemp != -1)
									{
										for (int i = 0; i < tins->mNumOperands; i++)
											tins->mSrc[i].mRange.Union(fins->mSrc[i].mRange);
										tins->mDst.mRange.Union(fins->mDst.mRange);

										if (fins->mDst.mTemp != tins->mDst.mTemp)
										{
											InterInstruction* nins = new InterInstruction(tins->mLocation, IC_LOAD_TEMPORARY);
											nins->mDst.mTemp = fins->mDst.mTemp;
											nins->mDst.mType = fins->mDst.mType;
											nins->mSrc[0].mTemp = tins->mDst.mTemp;
											nins->mSrc[0].mType = tins->mDst.mType;
											assert(nins->mSrc[0].mTemp >= 0);
											mFalseJump->mInstructions.Insert(0, nins);
											fi++;
										}
									}
									mTrueJump->mInstructions.Remove(ti);
									mFalseJump->mInstructions.Remove(fi);
									changed = true;
								}
							}
						}
					}
				}

				ti++;
			}
		}
#if 1
		if (mNumEntries > 1)
		{
			int i = 0;
			while (i < mEntryBlocks.Size() && mEntryBlocks[i]->mInstructions.Size() > 1 && !mEntryBlocks[i]->mFalseJump)
				i++;
			if (i == mEntryBlocks.Size())
			{
				InterCodeBasicBlock* eb = mEntryBlocks[0];

				int	ebi = eb->mInstructions.Size() - 2;
				while (ebi >= 0)
				{
					InterInstruction* ins = eb->mInstructions[ebi];

					if (ins && eb->CanMoveInstructionBehindBlock(ebi))
					{
						int j = 1, eji = -1;
						while (j < mEntryBlocks.Size() && (eji = mEntryBlocks[j]->FindSameInstruction(ins)) >= 0 && mEntryBlocks[j]->CanMoveInstructionBehindBlock(eji))
							j++;

						if (j == mEntryBlocks.Size())
						{
							mInstructions.Insert(0, ins);
							for (int j = 0; j < mEntryBlocks.Size(); j++)
								mEntryBlocks[j]->mInstructions.Remove(mEntryBlocks[j]->FindSameInstruction(ins));
							changed = true;
						}
					}

					ebi--;
				}
			}
		}
#endif
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

bool InterCodeBasicBlock::IsDirectLoopPathBlock(InterCodeBasicBlock* block)
{
	if (this == block)
		return true;

	if (block->mLoopHead)
		return false;

	if (block->mEntryBlocks.Size() == 0)
		return false;

	for (int i = 0; i < block->mEntryBlocks.Size(); i++)
		if (IsDirectLoopPathBlock(block->mEntryBlocks[i]))
			return true;

	return false;
}

bool InterCodeBasicBlock::IsDirectDominatorBlock(InterCodeBasicBlock* block)
{
	if (this == block)
		return true;

	if (block->mLoopHead)
		return false;

	if (block->mEntryBlocks.Size() == 0)
		return false;
	
	for (int i = 0; i < block->mEntryBlocks.Size(); i++)
		if (!IsDirectDominatorBlock(block->mEntryBlocks[i]))
			return false;

	return true;
}

bool InterCodeBasicBlock::CollectBlocksToDominator(InterCodeBasicBlock* dblock, ExpandingArray<InterCodeBasicBlock*>& body)
{
	if (this == dblock)
		return true;

	if (mLoopHead)
		return false;

	if (mEntryBlocks.Size() == 0)
		return false;

	body.Push(this);

	for (int i = 0; i < mEntryBlocks.Size(); i++)
		if (!mEntryBlocks[i]->CollectBlocksToDominator(dblock, body))
			return false;

	return true;
}

bool InterCodeBasicBlock::HoistCommonConditionalPath(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mFalseJump)
		{
			InterCodeBasicBlock* cblock = nullptr, * eblock = nullptr;

			if (!mTrueJump->mFalseJump && mTrueJump->mTrueJump && IsDirectDominatorBlock(mTrueJump->mTrueJump))
			{
				cblock = mTrueJump;
				eblock = mTrueJump->mTrueJump;
			}
			else if (!mFalseJump->mFalseJump && mFalseJump->mTrueJump && IsDirectDominatorBlock(mFalseJump->mTrueJump))
			{
				cblock = mFalseJump;
				eblock = mFalseJump->mTrueJump;
			}

			if (cblock && cblock->mNumEntries == 1)
			{
				ExpandingArray<InterCodeBasicBlock*>	pblocks;
				eblock->CollectBlocksToDominator(this, pblocks);
				pblocks.RemoveAll(cblock);
				pblocks.RemoveAll(eblock);

				for (int i = 0; i < cblock->mInstructions.Size(); i++)
				{
					InterInstruction* ins = cblock->mInstructions[i];

					if (cblock->CanMoveInstructionBeforeBlock(i) && !HasSideEffect(ins->mCode) && ins->mDst.mTemp >= 0 && 
						!cblock->IsTempModifiedInRange(i + 1, cblock->mInstructions.Size(), ins->mDst.mTemp) &&
						!cblock->mExitRequiredTemps[ins->mDst.mTemp])
					{
						int j = 0;
						while (j < eblock->mInstructions.Size() && !eblock->mInstructions[j]->IsEqualSource(ins))
							j++;

						if (j < eblock->mInstructions.Size() && !eblock->IsTempModifiedInRange(0, j, ins->mDst.mTemp) && eblock->CanMoveInstructionBeforeBlock(j) && cblock->CanMoveInstructionBeforeBlock(cblock->mInstructions.Size(), eblock->mInstructions[j]))
						{
							int k = 0;
							while (k < pblocks.Size() && !pblocks[k]->IsTempModified(ins->mDst.mTemp))
								k++;
							if (k == pblocks.Size())
							{
								eblock->mInstructions[j]->mCode = IC_LOAD_TEMPORARY;
								eblock->mInstructions[j]->mSrc[0] = ins->mDst;
								eblock->mInstructions[j]->mNumOperands = 1;

								mInstructions.Insert(mInstructions.Size() - 1, ins);
								cblock->mInstructions.Remove(i);

								mExitRequiredTemps += ins->mDst.mTemp;
								cblock->mEntryRequiredTemps += ins->mDst.mTemp;
								cblock->mExitRequiredTemps += ins->mDst.mTemp;
								eblock->mEntryRequiredTemps += ins->mDst.mTemp;
								i--;
								changed = true;
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->HoistCommonConditionalPath())
			changed = true;
		if (mFalseJump && mFalseJump->HoistCommonConditionalPath())
			changed = true;
	}

	return changed;
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
						if (ins->mCode == IC_STORE && ins->mSrc[0].mFinal)
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

bool InterCodeBasicBlock::ForwardLoopMovedTemp(void)
{
	bool changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && !mFalseJump && mTrueJump->mLoopHead && mTrueJump->mNumEntries == 2)
		{
			InterCodeBasicBlock* eblock = nullptr;
			if (mTrueJump->mTrueJump == mTrueJump)
				eblock = mTrueJump->mFalseJump;
			else if (mTrueJump->mFalseJump == mTrueJump)
				eblock = mTrueJump->mTrueJump;

			if (eblock && eblock->mNumEntries == 1)
			{
				int i = mInstructions.Size() - 1;
				while (i >= 0)
				{
					if (mInstructions[i]->mCode == IC_LOAD_TEMPORARY && CanMoveInstructionBehindBlock(i) &&
						!mTrueJump->mLocalUsedTemps[mInstructions[i]->mDst.mTemp] &&
						!mTrueJump->mLocalModifiedTemps[mInstructions[i]->mSrc[0].mTemp])
					{
						eblock->mInstructions.Insert(0, mInstructions[i]);
						mInstructions.Remove(i);
						changed = true;
					}
					else
						i--;
				}
			}
		}

		if (mTrueJump && mTrueJump->ForwardLoopMovedTemp())
			changed = true;
		if (mFalseJump && mFalseJump->ForwardLoopMovedTemp())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::UnionEntryValueRange(const GrowingIntegerValueRangeArray&range, const GrowingIntegerValueRangeArray& paramRange)
{
	int ni = intmin(mEntryValueRange.Size(), range.Size());
	for (int i = 0; i < ni; i++)
		mEntryValueRange[i].Union(range[i]);
	ni = intmin(mEntryParamValueRange.Size(), paramRange.Size());
	for (int i = 0; i < ni; i++)
		mEntryParamValueRange[i].Union(paramRange[i]);
}

bool InterCodeBasicBlock::ForwardRealDiamondMovedTemp(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump && mTrueJump->mTrueJump &&
			mTrueJump->mNumEntries == 1 && !mTrueJump->mFalseJump &&
			mFalseJump->mNumEntries == 1 && !mFalseJump->mFalseJump &&
			mTrueJump->mTrueJump == mFalseJump->mTrueJump && mTrueJump->mTrueJump->mNumEntries == 2)
		{
			InterCodeBasicBlock* eblock = mTrueJump->mTrueJump;

			for (int i = mInstructions.Size() - 1; i >= 0; i--)
			{
				InterInstruction* mins = mInstructions[i];

				if ((mins->mCode == IC_BINARY_OPERATOR || mins->mCode == IC_LEA) && (mins->mSrc[1].mTemp < 0 || mins->mSrc[0].mTemp < 0) || mins->mCode == IC_UNARY_OPERATOR || mins->mCode == IC_CONVERSION_OPERATOR || mins->mCode == IC_LOAD_TEMPORARY)
				{
					int	ttemp = mins->mDst.mTemp;
					int stemp = ((mins->mCode == IC_BINARY_OPERATOR || mins->mCode == IC_LEA) && mins->mSrc[0].mTemp < 0) ? mins->mSrc[1].mTemp : mins->mSrc[0].mTemp;

					if (!mTrueJump->mLocalRequiredTemps[ttemp] && !mTrueJump->mLocalModifiedTemps[stemp] && !mTrueJump->mLocalModifiedTemps[ttemp] &&
						!mFalseJump->mLocalRequiredTemps[ttemp] && !mFalseJump->mLocalModifiedTemps[stemp] && !mFalseJump->mLocalModifiedTemps[ttemp] &&
						!IsTempReferencedOnPath(ttemp, i + 1) && !IsTempModifiedOnPath(stemp, i + 1))
					{
						mTrueJump->mEntryRequiredTemps += stemp;
						mTrueJump->mExitRequiredTemps += stemp;

						mFalseJump->mEntryRequiredTemps += stemp;
						mFalseJump->mExitRequiredTemps += stemp;

						eblock->mEntryRequiredTemps += stemp;
						mExitRequiredTemps += stemp;

						eblock->mInstructions.Insert(0, mins);
						mInstructions.Remove(i);

						changed = true;
					}
				}
				else if (mins->mCode == IC_LOAD)
				{
					int	ttemp = mins->mDst.mTemp;
					int stemp = mins->mSrc[0].mTemp;

					if (!mTrueJump->mLocalRequiredTemps[ttemp] && (stemp < 0 || !mTrueJump->mLocalModifiedTemps[stemp]) && !mTrueJump->mLocalModifiedTemps[ttemp] &&
						!mFalseJump->mLocalRequiredTemps[ttemp] && (stemp < 0 || !mFalseJump->mLocalModifiedTemps[stemp]) && !mFalseJump->mLocalModifiedTemps[ttemp] &&
						!IsTempReferencedOnPath(ttemp, i + 1) && (stemp < 0 || !IsTempModifiedOnPath(stemp, i + 1)) &&
						CanMoveInstructionBehindBlock(i) &&
						!DestroyingMem(mTrueJump, mins, 0, mTrueJump->mInstructions.Size()) && !DestroyingMem(mFalseJump, mins, 0, mFalseJump->mInstructions.Size()))
					{
						if (stemp >= 0)
						{
							mTrueJump->mEntryRequiredTemps += stemp;
							mTrueJump->mExitRequiredTemps += stemp;

							mFalseJump->mEntryRequiredTemps += stemp;
							mFalseJump->mExitRequiredTemps += stemp;

							eblock->mEntryRequiredTemps += stemp;
							mExitRequiredTemps += stemp;
						}

						eblock->mInstructions.Insert(0, mins);
						mInstructions.Remove(i);

						changed = true;
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->ForwardRealDiamondMovedTemp())
			changed = true;
		if (mFalseJump && mFalseJump->ForwardRealDiamondMovedTemp())
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
			InterCodeBasicBlock* tblock = nullptr, * fblock = nullptr, * sblock = nullptr;

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
					if (tblock->mInstructions[0]->IsEqual(fblock->mInstructions.Last()) && (tblock->mInstructions[0]->mSrc[0].mTemp < 0 || !fblock->mLocalModifiedTemps[tblock->mInstructions[0]->mSrc[0].mTemp]))
					{
						tblock->UnionEntryValueRange(fblock->mTrueValueRange, fblock->mTrueParamValueRange);
						tblock->UnionEntryValueRange(fblock->mFalseValueRange, fblock->mFalseParamValueRange);

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
											offset += int(mInstructions[j]->mSrc[0].mIntConst);
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

										InterInstruction* nins = new InterInstruction(mins->mLocation, IC_LOAD_TEMPORARY);
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
								!DestroyingMem(tblock, mins, 0, tblock->mInstructions.Size()) &&
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
				bool	moved = false, pair = false;

				if (i > 0 && ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_DIVU && i > 0 &&
					mInstructions[i - 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i - 1]->mOperator == IA_MODU &&
					ins->mSrc[0].IsEqual(mInstructions[i - 1]->mSrc[0]) &&
					ins->mSrc[1].IsEqual(mInstructions[i - 1]->mSrc[1]))
					pair = true;
				else if (i > 0 && ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MODU && i > 0 &&
					mInstructions[i - 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i - 1]->mOperator == IA_DIVU &&
					ins->mSrc[0].IsEqual(mInstructions[i - 1]->mSrc[0]) &&
					ins->mSrc[1].IsEqual(mInstructions[i - 1]->mSrc[1]))
					pair = true;
				else if (i + 1 < mInstructions.Size() && ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_DIVU &&
					mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MODU &&
					ins->mSrc[0].IsEqual(mInstructions[i + 1]->mSrc[0]) &&
					ins->mSrc[1].IsEqual(mInstructions[i + 1]->mSrc[1]))
					pair = true;
				else if (i + 1 < mInstructions.Size() && ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MODU &&
					mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_DIVU &&
					ins->mSrc[0].IsEqual(mInstructions[i + 1]->mSrc[0]) &&
					ins->mSrc[1].IsEqual(mInstructions[i + 1]->mSrc[1]))
					pair = true;

				if (!pair && dtemp >= 0 && !providedTemps[dtemp] && !requiredTemps[dtemp])
				{
					int j = 0;
					while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || !(providedTemps[ins->mSrc[j].mTemp] || IsTempModifiedOnPath(ins->mSrc[j].mTemp, i + 1))))
						j++;

					if (j == ins->mNumOperands && IsMoveable(ins->mCode) && CanMoveInstructionBehindBlock(i))
					{
						if (mTrueJump->mNumEntries == 1 && trueExitRequiredTemps[dtemp] && !falseExitRequiredTems[dtemp])
						{
							if (ins->mDst.mTemp >= 0 && mTrueJump->mEntryValueRange.Size() > ins->mDst.mTemp)
								mTrueJump->mEntryValueRange[ins->mDst.mTemp].Reset();

							for (int j = 0; j < ins->mNumOperands; j++)
							{
								if (ins->mSrc[j].mTemp >= 0)
								{
									trueExitRequiredTemps += ins->mSrc[j].mTemp;
									mTrueJump->mEntryRequiredTemps += ins->mSrc[j].mTemp;
									mTrueJump->mLocalUsedTemps += ins->mSrc[j].mTemp;
								}
							}
							mTrueJump->mLocalModifiedTemps += dtemp;
							mTrueJump->mInstructions.Insert(0, ins);
							mInstructions.Remove(i);
							moved = true;
							changed = true;
						}
						else if (mFalseJump->mNumEntries == 1 && !trueExitRequiredTemps[dtemp] && falseExitRequiredTems[dtemp])
						{
							if (ins->mDst.mTemp >= 0 && mFalseJump->mEntryValueRange.Size() > ins->mDst.mTemp)
								mFalseJump->mEntryValueRange[ins->mDst.mTemp].Reset();

							for (int j = 0; j < ins->mNumOperands; j++)
							{
								if (ins->mSrc[j].mTemp >= 0)
								{
									falseExitRequiredTems += ins->mSrc[j].mTemp;
									mFalseJump->mEntryRequiredTemps += ins->mSrc[j].mTemp;
									mFalseJump->mLocalUsedTemps += ins->mSrc[j].mTemp;
								}
							}
							mFalseJump->mLocalModifiedTemps += dtemp;
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
							while (j < ins->mNumOperands && !(ins->mSrc[j].mTemp >= 0 && (mFalseJump->mLocalModifiedTemps[ins->mSrc[j].mTemp] || mTrueJump->mLocalModifiedTemps[ins->mSrc[j].mTemp])))
								j++;

							if (j == ins->mNumOperands)
							{
								if (ins->mCode == IC_LOAD)
								{
									j = 0;
									while (j < mTrueJump->mInstructions.Size() && CanSwapInstructions(ins, mTrueJump->mInstructions[j]))
										j++;
								}
								if (ins->mCode != IC_LOAD || j == mTrueJump->mInstructions.Size())
								{
									if (ins->mCode == IC_LOAD)
									{
										j = 0;
										while (j < mFalseJump->mInstructions.Size() && CanSwapInstructions(ins, mFalseJump->mInstructions[j]))
											j++;
									}

									if (ins->mCode != IC_LOAD || j == mFalseJump->mInstructions.Size())
									{
										if (ins->mDst.mTemp >= 0 && joinedBlock->mEntryValueRange.Size() > ins->mDst.mTemp)
											joinedBlock->mEntryValueRange[ins->mDst.mTemp].Reset();

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
			if (ins->mCode == IC_STORE || ins->mCode == IC_COPY || ins->mCode == IC_FILL)
			{
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL && !ins->mVolatile)
				{
					if (!(ins->mSrc[1].mLinkerObject->mFlags & LOBJF_RELEVANT) && (ins->mSrc[1].mLinkerObject->mType == LOT_BSS || ins->mSrc[1].mLinkerObject->mType == LOT_DATA))
					{
						ins->mSrc[0].mTemp = -1;
						ins->mNumOperands = 0;
						ins->mCode = IC_NONE;
					}
				}
			}
		}

		if (mTrueJump) mTrueJump->RemoveNonRelevantStatics();
		if (mFalseJump) mFalseJump->RemoveNonRelevantStatics();
	}
}

bool InterCodeBasicBlock::RecheckOuterFrame(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_LOAD)
			{
				if (ins->mSrc[0].mMemory == IM_FRAME)
					return true;
			}
			else if (ins->mCode == IC_STORE || ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mMemory == IM_FRAME)
					return true;
			}
			else if (ins->mCode == IC_COPY)
			{
				if (ins->mSrc[0].mMemory == IM_FRAME)
					return true;
				if (ins->mSrc[1].mMemory == IM_FRAME)
					return true;
			}
			else if (ins->mCode == IC_CONSTANT)
			{
				if (ins->mConst.mType == IT_POINTER && ins->mConst.mMemory == IM_FRAME)
					return true;
			}
			else if (ins->mCode == IC_PUSH_FRAME)
				return true;
		}

		if (mTrueJump && mTrueJump->RecheckOuterFrame())
			return true;
		if (mFalseJump && mFalseJump->RecheckOuterFrame())
			return true;
	}

	return false;
}

void InterCodeBasicBlock::CollectParamVarsSize(int& size)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_LOAD)
			{
				if (ins->mSrc[0].mMemory == IM_PARAM)
				{
					if (ins->mSrc[0].mVarIndex + InterTypeSize[ins->mDst.mType] > size)
						size = ins->mSrc[0].mVarIndex + InterTypeSize[ins->mDst.mType];
				}
			}
			else if (ins->mCode == IC_STORE)
			{
				if (ins->mSrc[1].mMemory == IM_PARAM)
				{
					if (ins->mSrc[1].mVarIndex + InterTypeSize[ins->mSrc[0].mType] > size)
						size = ins->mSrc[1].mVarIndex + InterTypeSize[ins->mSrc[0].mType];
				}
			}
			else if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mMemory == IM_PARAM)
				{
					if (ins->mSrc[1].mVarIndex + mProc->mParamVars[ins->mSrc[1].mVarIndex]->mSize > size)
						size = ins->mSrc[1].mVarIndex + mProc->mParamVars[ins->mSrc[1].mVarIndex]->mSize;
				}
			}
			else if (ins->mCode == IC_COPY)
			{
				if (ins->mSrc[0].mMemory == IM_PARAM)
				{
					if (ins->mSrc[0].mVarIndex + mProc->mParamVars[ins->mSrc[0].mVarIndex]->mSize > size)
						size = ins->mSrc[0].mVarIndex + mProc->mParamVars[ins->mSrc[0].mVarIndex]->mSize;
				}
				if (ins->mSrc[1].mMemory == IM_PARAM)
				{
					if (ins->mSrc[1].mVarIndex + mProc->mParamVars[ins->mSrc[1].mVarIndex]->mSize > size)
						size = ins->mSrc[1].mVarIndex + mProc->mParamVars[ins->mSrc[1].mVarIndex]->mSize;
				}
			}
			else if (ins->mCode == IC_CONSTANT)
			{
				if (ins->mConst.mType == IT_POINTER && ins->mConst.mMemory == IM_PARAM)
				{
					if (ins->mConst.mVarIndex + mProc->mParamVars[ins->mConst.mVarIndex]->mSize > size)
						size = ins->mConst.mVarIndex + mProc->mParamVars[ins->mConst.mVarIndex]->mSize;

				}
			}
		}

		if (mTrueJump) mTrueJump->CollectParamVarsSize(size);
		if (mFalseJump) mFalseJump->CollectParamVarsSize(size);
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
						size = int(mInstructions[i]->mConst.mIntConst);
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
			if (mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE || mInstructions[i]->mCode == IC_DISPATCH)
				return false;

		if (mTrueJump && !mTrueJump->IsLeafProcedure())
			return false;
		if (mFalseJump && !mFalseJump->IsLeafProcedure())
			return false;
	}

	return true;
}


void InterCodeBasicBlock::ExpandSelect(void)
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

			InterCodeBasicBlock* tblock = new InterCodeBasicBlock(mProc);
			InterCodeBasicBlock* fblock = new InterCodeBasicBlock(mProc);
			InterCodeBasicBlock* eblock = new InterCodeBasicBlock(mProc);

			for (int j = i + 1; j < mInstructions.Size(); j++)
				eblock->mInstructions.Push(mInstructions[j]);
			eblock->Close(mTrueJump, mFalseJump);

			mInstructions.SetSize(i);

			InterInstruction* bins = new InterInstruction(sins->mLocation, IC_BRANCH);
			bins->mSrc[0] = sins->mSrc[2];
			mInstructions.Push(bins);

			InterInstruction* tins;
			if (sins->mSrc[1].mTemp < 0)
			{
				tins = new InterInstruction(sins->mLocation, IC_CONSTANT);
				tins->mConst = sins->mSrc[1];
			}
			else
			{
				tins = new InterInstruction(sins->mLocation, IC_LOAD_TEMPORARY);
				tins->mSrc[0] = sins->mSrc[1];
			}

			tins->mDst = sins->mDst;

			InterInstruction* fins;
			if (sins->mSrc[0].mTemp < 0)
			{
				fins = new InterInstruction(sins->mLocation, IC_CONSTANT);
				fins->mConst = sins->mSrc[0];
			}
			else
			{
				fins = new InterInstruction(sins->mLocation, IC_LOAD_TEMPORARY);
				fins->mSrc[0] = sins->mSrc[0];
			}

			fins->mDst = sins->mDst;

			tblock->mInstructions.Push(tins);
			InterInstruction* jins = new InterInstruction(sins->mLocation, IC_JUMP);
			tblock->mInstructions.Push(jins);

			tblock->Close(eblock, nullptr);

			fblock->mInstructions.Push(fins);
			jins = new InterInstruction(sins->mLocation, IC_JUMP);
			fblock->mInstructions.Push(jins);

			fblock->Close(eblock, nullptr);

			mTrueJump = tblock;
			mFalseJump = fblock;
		}

		if (mTrueJump)
			mTrueJump->ExpandSelect();
		if (mFalseJump)
			mFalseJump->ExpandSelect();
	}
}

void InterCodeBasicBlock::SplitBranches(void)
{
	if (!mVisited)
	{
		mVisited = true;

		int sz = mInstructions.Size();

		int nsplit = 0;

		if (mTrueJump && mFalseJump && sz > 1)
		{
			if (mLoopHead && mNumEntries == 2 && (mTrueJump == this || mFalseJump == this) && sz < 5)
				nsplit = 0;
			else if (sz >= 2 && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR)
				nsplit = 2;
			else
				nsplit = 1;
		}

		if (nsplit > 0 && nsplit < sz)
		{
			InterCodeBasicBlock* block = new InterCodeBasicBlock(mProc);
			InterInstruction* ins = mInstructions.Last();

			for(int i=0; i<nsplit; i++)
				block->mInstructions.Push(mInstructions[sz - nsplit + i]);
			mInstructions.SetSize(sz - nsplit);

			InterInstruction* jins = new InterInstruction(ins->mLocation, IC_JUMP);
			mInstructions.Push(jins);
			block->Close(mTrueJump, mFalseJump);
			mTrueJump = block;
			mFalseJump = nullptr;
			block->mNumEntries = 1;

			block->SplitBranches();
		}
		else
		{
			if (mTrueJump)
				mTrueJump->SplitBranches();
			if (mFalseJump)
				mFalseJump->SplitBranches();
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

bool InterCodeBasicBlock::PreventsCallerStaticStack(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
			{
				if (ins->mSrc[0].mTemp >= 0 || !ins->mSrc[0].mLinkerObject)
					return false;
				else if (ins->mSrc[0].mLinkerObject == mProc->mLinkerObject)
					; // Simple recursion
				else if (!(ins->mSrc[0].mLinkerObject->mFlags & LOBJF_STATIC_STACK))
					return false;
			}
			else if (ins->mCode == IC_DISPATCH)
			{
				for (int j = 0; j < mProc->mCalledFunctions.Size(); j++)
				{
					if (!(mProc->mCalledFunctions[j]->mLinkerObject && (mProc->mCalledFunctions[j]->mLinkerObject->mFlags & LOBJF_STATIC_STACK)))
						return false;
				}
			}
		}

		if (mTrueJump && mTrueJump->PreventsCallerStaticStack())
			return true;
		if (mFalseJump && mFalseJump->PreventsCallerStaticStack())
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
			else if (mInstructions[i]->mCode == IC_DISPATCH)
			{
				for (int j = 0; j < mProc->mCalledFunctions.Size(); j++)
				{
					if (!(mProc->mCalledFunctions[j]->mLinkerObject && (mProc->mCalledFunctions[j]->mLinkerObject->mFlags & LOBJF_STATIC_STACK)))
						return false;
				}
			}
		}

		if (mTrueJump && !mTrueJump->CheckStaticStack())
			return false;
		if (mFalseJump && !mFalseJump->CheckStaticStack())
			return false;
	}

	return true;
}

void InterCodeBasicBlock::ApplyStaticStack(InterOperand & iop, const GrowingVariableArray& localVars)
{
	if (iop.mMemory == IM_LOCAL)
	{
		assert(localVars[iop.mVarIndex]->mIndex < mProc->mModule->mGlobalVars.Size());

		iop.mMemory = iop.mMemoryBase = IM_GLOBAL;
		iop.mLinkerObject = localVars[iop.mVarIndex]->mLinkerObject;
		iop.mVarIndex = localVars[iop.mVarIndex]->mIndex;
	}	
}

void InterCodeBasicBlock::CollectStaticStackDependencies(LinkerObject* lobj)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if ((mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE) && mInstructions[i]->mSrc[0].mLinkerObject && mInstructions[i]->mSrc[0].mLinkerObject->mStackSection)
				lobj->mStackSection->mSections.Push(mInstructions[i]->mSrc[0].mLinkerObject->mStackSection);
		}

		if (mTrueJump) mTrueJump->CollectStaticStackDependencies(lobj);
		if (mFalseJump) mFalseJump->CollectStaticStackDependencies(lobj);
	}
}

void InterCodeBasicBlock::CollectStaticStack(LinkerObject* lobj, const GrowingVariableArray& localVars)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if ((mInstructions[i]->mCode == IC_CALL || mInstructions[i]->mCode == IC_CALL_NATIVE) && mInstructions[i]->mSrc[0].mLinkerObject->mStackSection)
				lobj->mStackSection->mSections.Push(mInstructions[i]->mSrc[0].mLinkerObject->mStackSection);

			if (mInstructions[i]->mCode == IC_LOAD)
				ApplyStaticStack(mInstructions[i]->mSrc[0],localVars);
			else if (mInstructions[i]->mCode == IC_STORE || mInstructions[i]->mCode == IC_LEA || mInstructions[i]->mCode == IC_FILL)
				ApplyStaticStack(mInstructions[i]->mSrc[1], localVars);
			else if (mInstructions[i]->mCode == IC_CONSTANT && mInstructions[i]->mDst.mType == IT_POINTER)
			{
				ApplyStaticStack(mInstructions[i]->mConst, localVars);
				mInstructions[i]->mDst.mMemory = mInstructions[i]->mDst.mMemoryBase = mInstructions[i]->mConst.mMemory;
			}
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

void PromoteStaticStackParam(InterOperand& iop, LinkerObject* paramlobj)
{
	if (iop.mMemory == IM_FFRAME || iop.mMemory == IM_FPARAM)
	{
		if (iop.mVarIndex >= BC_REG_FPARAMS_END - BC_REG_FPARAMS)
		{
			int	offset = iop.mVarIndex - (BC_REG_FPARAMS_END - BC_REG_FPARAMS);
			iop.mMemory = IM_GLOBAL;
			iop.mIntConst += offset;
			iop.mLinkerObject = paramlobj;
			iop.mVarIndex = -1;
			paramlobj->EnsureSpace(int(iop.mIntConst), iop.mOperandSize);
		}
	}
}

void InterCodeBasicBlock::PromoteStaticStackParams(LinkerObject* paramlobj)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if (mInstructions[i]->mCode == IC_LOAD)
				PromoteStaticStackParam(mInstructions[i]->mSrc[0], paramlobj);
			else if (mInstructions[i]->mCode == IC_STORE || mInstructions[i]->mCode == IC_LEA || mInstructions[i]->mCode == IC_FILL)
				PromoteStaticStackParam(mInstructions[i]->mSrc[1], paramlobj);
			else if (mInstructions[i]->mCode == IC_CONSTANT && mInstructions[i]->mDst.mType == IT_POINTER)
				PromoteStaticStackParam(mInstructions[i]->mConst, paramlobj);
			else if (mInstructions[i]->mCode == IC_COPY)
			{
				PromoteStaticStackParam(mInstructions[i]->mSrc[0], paramlobj);
				PromoteStaticStackParam(mInstructions[i]->mSrc[1], paramlobj);
			}
		}

		if (mTrueJump) mTrueJump->PromoteStaticStackParams(paramlobj);
		if (mFalseJump) mFalseJump->PromoteStaticStackParams(paramlobj);
	}
}


void InterCodeBasicBlock::CollectByteIndexPointers(NumberSet& invtemps, NumberSet& inctemps, GrowingIntArray& vartemps)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			const InterInstruction* ins = mInstructions[i];
			if (ins->mDst.mTemp >= 0 && !invtemps[ins->mDst.mTemp])
			{
				if (ins->mDst.mType == IT_POINTER && ins->mDst.mMemoryBase == IM_GLOBAL && ins->mDst.mVarIndex >= 0 && mProc->mModule->mGlobalVars[ins->mDst.mVarIndex]->mSize < 256)
				{
					if (vartemps[ins->mDst.mTemp] == -1 || vartemps[ins->mDst.mTemp] == ins->mDst.mVarIndex)
					{
						vartemps[ins->mDst.mTemp] = ins->mDst.mVarIndex;
						if (ins->mCode == IC_LEA)
						{
							if (ins->mSrc[1].mTemp == ins->mDst.mTemp)
								inctemps += ins->mDst.mTemp;
							else
								invtemps += ins->mDst.mTemp;
						}
						else if (ins->mCode == IC_CONSTANT)
						{

						}
						else
							invtemps += ins->mDst.mTemp;
					}
					else
						invtemps += ins->mDst.mTemp;
				}
				else
					invtemps += ins->mDst.mTemp;
			}

			if (ins->mCode == IC_STORE)
			{
				if (ins->mSrc[0].mTemp >= 0)
					invtemps += ins->mSrc[0].mTemp;
			}
			else if (ins->mCode == IC_LEA)
			{
				if (ins->mSrc[1].mTemp >= 0 && ins->mSrc[1].mTemp != ins->mDst.mTemp)
					invtemps += ins->mSrc[1].mTemp;
				if (ins->mSrc[0].mTemp >= 0)
					invtemps += ins->mSrc[0].mTemp;
			}
			else if (ins->mCode == IC_LOAD)
			{
			}
			else if (ins->mCode == IC_CONSTANT)
			{
			}
			else if (ins->mCode == IC_RELATIONAL_OPERATOR && (ins->mSrc[0].mType == IT_POINTER || ins->mSrc[1].mType == IT_POINTER))
			{
				int ic = 0, ir = 1;
				if (ins->mSrc[0].mTemp > 0)
				{
					ic = 1; ir = 0;
				}

				if (ins->mSrc[ic].mTemp < 0 && ins->mSrc[ir].mTemp >= 0 && !invtemps[ins->mSrc[ir].mTemp])
				{
					int treg = ins->mSrc[ir].mTemp;
					int vidx = ins->mSrc[ic].mVarIndex;

					if (ins->mSrc[ic].mMemory == IM_GLOBAL && vidx >= 0 && mProc->mModule->mGlobalVars[vidx]->mSize < 256)
					{
						if (vartemps[treg] == -1 || vartemps[treg] == vidx)
						{
							vartemps[treg] = vidx;
						}
						else
							invtemps += treg;
					}
					else
						invtemps += treg;
				}
				else
				{
					if (ins->mSrc[1].mTemp >= 0)
						invtemps += ins->mSrc[1].mTemp;
					if (ins->mSrc[0].mTemp >= 0)
						invtemps += ins->mSrc[0].mTemp;
				}
			}
			else
			{
				for (int i = 0; i < ins->mNumOperands; i++)
					if (ins->mSrc[i].mTemp >= 0)
						invtemps += ins->mSrc[i].mTemp;
			}
		}

		if (mTrueJump) mTrueJump->CollectByteIndexPointers(invtemps, inctemps, vartemps);
		if (mFalseJump) mFalseJump->CollectByteIndexPointers(invtemps, inctemps, vartemps);
	}
}

bool InterCodeBasicBlock::ReplaceByteIndexPointers(const NumberSet& inctemps, const GrowingIntArray& vartemps, int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		InterType	itype = mProc->mNativeProcedure ? IT_INT8 : IT_INT16;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			switch (ins->mCode)
			{
			case IC_CONSTANT:
				if (inctemps[ins->mDst.mTemp])
				{
					ins->mDst.mType = itype;
				}
				break;
			case IC_LOAD:
			case IC_STORE:
			{
				int pi = ins->mCode == IC_STORE ? 1 : 0;
				if (ins->mSrc[pi].mTemp >= 0 && inctemps[ins->mSrc[pi].mTemp])
				{
					int ireg = ins->mSrc[pi].mTemp;

					InterInstruction* nins = new InterInstruction(ins->mLocation, IC_LEA);
					nins->mNumOperands = 2;
					nins->mSrc[0].mType = itype;
					nins->mSrc[0].mTemp = ireg;
					nins->mSrc[1].mType = IT_POINTER;
					nins->mSrc[1].mTemp = -1;
					nins->mSrc[1].mMemory = IM_GLOBAL;
					nins->mSrc[1].mVarIndex = vartemps[ireg];
					nins->mSrc[1].mLinkerObject = mProc->mModule->mGlobalVars[vartemps[ireg]]->mLinkerObject;
					mInstructions.Insert(i, nins);
					nins->mDst = ins->mSrc[pi];
					ins->mSrc[pi].mTemp = nins->mDst.mTemp = spareTemps++;
					ins->mSrc[pi].mFinal = true;
					i++;
					changed = true;
				}
			}	break;
			case IC_RELATIONAL_OPERATOR:
				if (ins->mSrc[0].mTemp >= 0 && inctemps[ins->mSrc[0].mTemp] || ins->mSrc[1].mTemp >= 0 && inctemps[ins->mSrc[1].mTemp])
				{
					ins->mSrc[0].mType = itype;
					ins->mSrc[1].mType = itype;
					changed = true;
				}
				break;
			case IC_LEA:
				if (inctemps[ins->mDst.mTemp])
				{
					ins->mCode = IC_BINARY_OPERATOR;
					ins->mOperator = IA_ADD;
					ins->mDst.mType = itype;
					ins->mSrc[1].mType = itype;
					changed = true;
				}
				break;
			}

		}

		if (mTrueJump && mTrueJump->ReplaceByteIndexPointers(inctemps, vartemps, spareTemps)) changed = true;
		if (mFalseJump && mFalseJump->ReplaceByteIndexPointers(inctemps, vartemps, spareTemps)) changed = true;
	}

	return changed;
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
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc[0].mTemp &&
					mTrueJump->mInstructions[1]->mSrc[0].mFinal)
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
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc[0].mTemp &&
					mTrueJump->mInstructions[1]->mSrc[0].mFinal)
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
					mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc[0].mTemp &&
					mTrueJump->mInstructions[1]->mSrc[0].mFinal)
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

static bool BlockSameCondition(InterCodeBasicBlock* b, InterCodeBasicBlock* s)
{
	int nb = b->mInstructions.Size();
	if (s->mInstructions.Size() == 2 && 
		s->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR &&
		s->mInstructions[0]->mOperator == b->mInstructions[nb - 2]->mOperator &&
		s->mInstructions[1]->mCode == IC_BRANCH &&
		s->mInstructions[1]->mSrc[0].mTemp == s->mInstructions[0]->mDst.mTemp &&
		s->mInstructions[1]->mSrc[0].mFinal &&
		s->mInstructions[0]->IsEqualSource(b->mInstructions[nb - 2]))
	{
		return true;
	}
	
	return false;
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

		int sz = mInstructions.Size();
		if (sz > 1 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && 
			mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp &&
			mInstructions[sz - 2]->mSrc[0].mTemp != mInstructions[sz - 2]->mDst.mTemp &&
			mInstructions[sz - 2]->mSrc[1].mTemp != mInstructions[sz - 2]->mDst.mTemp)
		{
			if (mTrueJump && BlockSameCondition(this, mTrueJump))
			{
				InterCodeBasicBlock* target = mTrueJump->mTrueJump;
				mTrueJump->mNumEntries--;
				mTrueJump->mEntryBlocks.RemoveAll(this);
				target->mNumEntries++;
				target->mEntryBlocks.Push(this);
				mTrueJump = target;
			}
			else if (mFalseJump && BlockSameCondition(this, mFalseJump))
			{
				InterCodeBasicBlock* target = mFalseJump->mFalseJump;
				mFalseJump->mNumEntries--;
				mFalseJump->mEntryBlocks.RemoveAll(this);
				target->mNumEntries++;
				target->mEntryBlocks.Push(this);
				mFalseJump = target;
			}
#if 1
			else if (mTrueJump && mTrueJump->mInstructions.Size() == 2 &&
				mTrueJump->mInstructions[0]->mDst.mTemp != mInstructions[sz - 2]->mDst.mTemp &&
				mTrueJump->mInstructions[1]->mCode == IC_BRANCH && mTrueJump->mInstructions[1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp)
			{
				InterCodeBasicBlock* block = mTrueJump->Clone();
				block->mInstructions[1]->mCode = IC_JUMP;
				block->mInstructions[1]->mNumOperands = 0;
				block->Close(mTrueJump->mTrueJump, nullptr);

				block->mTrueJump->mNumEntries++;
				block->mTrueJump->mEntryBlocks.Push(block);

				mTrueJump->mNumEntries--;
				mTrueJump->mEntryBlocks.RemoveAll(this);
				block->mNumEntries++;
				block->mEntryBlocks.Push(this);
				mTrueJump = block;
			}
			else if (mFalseJump && mFalseJump->mInstructions.Size() == 2 &&
				mFalseJump->mInstructions[0]->mDst.mTemp != mInstructions[sz - 2]->mDst.mTemp &&
				mFalseJump->mInstructions[1]->mCode == IC_BRANCH && mFalseJump->mInstructions[1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp)
			{
				InterCodeBasicBlock* block = mFalseJump->Clone();
				block->mInstructions[1]->mCode = IC_JUMP;
				block->mInstructions[1]->mNumOperands = 0;
				block->Close(mFalseJump->mFalseJump, nullptr);

				block->mTrueJump->mNumEntries++;
				block->mTrueJump->mEntryBlocks.Push(block);

				mFalseJump->mNumEntries--;
				mFalseJump->mEntryBlocks.RemoveAll(this);
				block->mNumEntries++;
				block->mEntryBlocks.Push(this);
				mFalseJump = block;
			}
#endif
		}
		else if (sz > 1 && mInstructions[sz - 1]->mCode == IC_JUMP && mInstructions[sz - 2]->mCode == IC_CONSTANT)
		{
			if (mTrueJump->mInstructions.Size() == 2 && !mTrueJump->mLoopHead &&
				mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR &&
				mTrueJump->mInstructions[1]->mCode == IC_BRANCH && 
				mTrueJump->mInstructions[1]->mSrc[0].mTemp == mTrueJump->mInstructions[0]->mDst.mTemp &&
				mTrueJump->mInstructions[1]->mSrc[0].mFinal)
			{
				InterCodeBasicBlock* target = nullptr;

				if (mTrueJump->mInstructions[0]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp &&
					mTrueJump->mInstructions[0]->mSrc[1].mTemp < 0)
				{
					InterOperand op = OperandConstantFolding(mTrueJump->mInstructions[0]->mOperator, mTrueJump->mInstructions[0]->mSrc[1], mInstructions[sz - 2]->mConst);
					target = op.mIntConst ? mTrueJump->mTrueJump : mTrueJump->mFalseJump;
				}
				else if (mTrueJump->mInstructions[0]->mSrc[1].mTemp == mInstructions[sz - 2]->mDst.mTemp &&
					mTrueJump->mInstructions[0]->mSrc[0].mTemp < 0)
				{
					InterOperand op = OperandConstantFolding(mTrueJump->mInstructions[0]->mOperator, mInstructions[sz - 2]->mConst, mTrueJump->mInstructions[0]->mSrc[0]);
					target = op.mIntConst ? mTrueJump->mTrueJump : mTrueJump->mFalseJump;
				}

				if (target)
				{
					mTrueJump->mNumEntries--;
					mTrueJump->mEntryBlocks.RemoveAll(this);
					target->mNumEntries++;
					target->mEntryBlocks.Push(this);
					mTrueJump = target;
				}
			}
		}

		if (mTrueJump)
			mTrueJump->FollowJumps();
		if (mFalseJump)
			mFalseJump->FollowJumps();
	}
}

void InterCodeBasicBlock::BuildLoopSuffix(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mFalseJump)
		{
			if (mTrueJump->mLoopHead && mTrueJump->mNumEntries == 2 && mFalseJump->mNumEntries > 1)
			{
				InterCodeBasicBlock* suffix = new InterCodeBasicBlock(mProc);

				suffix->mEntryRequiredTemps = mFalseJump->mEntryRequiredTemps;
				suffix->mExitRequiredTemps = mFalseJump->mEntryRequiredTemps;
				suffix->mLocalModifiedTemps.Reset(mExitRequiredTemps.Size());

				InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
				suffix->Append(jins);
				suffix->Close(mFalseJump, nullptr);
				mFalseJump = suffix;
				suffix->mNumEntries = 1;
			}
			else if (mFalseJump->mLoopHead && mFalseJump->mNumEntries == 2 && mTrueJump->mNumEntries > 1)
			{
				InterCodeBasicBlock* suffix = new InterCodeBasicBlock(mProc);
				suffix->mEntryRequiredTemps = mTrueJump->mEntryRequiredTemps;
				suffix->mExitRequiredTemps = mTrueJump->mEntryRequiredTemps;
				suffix->mLocalModifiedTemps.Reset(mExitRequiredTemps.Size());

				InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
				suffix->Append(jins);
				suffix->Close(mTrueJump, nullptr);
				mTrueJump = suffix;
				suffix->mNumEntries = 1;
			}
		}
#if 0
		if (mLoopHead && mNumEntries == 2 && mFalseJump)
		{
			if (mTrueJump == this && mFalseJump != this)
			{
				if (mFalseJump->mNumEntries > 1)
				{
					InterCodeBasicBlock* suffix = new InterCodeBasicBlock(mProc);
					suffix->mEntryRequiredTemps = mFalseJump->mEntryRequiredTemps;
					suffix->mExitRequiredTemps = mFalseJump->mEntryRequiredTemps;
					suffix->mLocalModifiedTemps.Reset(mExitRequiredTemps.Size());

					InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
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
					InterCodeBasicBlock* suffix = new InterCodeBasicBlock(mProc);
					suffix->mEntryRequiredTemps = mTrueJump->mEntryRequiredTemps;
					suffix->mExitRequiredTemps = mTrueJump->mEntryRequiredTemps;
					suffix->mLocalModifiedTemps.Reset(mExitRequiredTemps.Size());

					InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
					suffix->Append(jins);
					suffix->Close(mTrueJump, nullptr);
					mTrueJump = suffix;
					suffix->mNumEntries = 1;
				}
			}
		}
#endif

		if (mTrueJump)
			mTrueJump->BuildLoopSuffix();
		if (mFalseJump)
			mFalseJump->BuildLoopSuffix();
	}
}

InterCodeBasicBlock* InterCodeBasicBlock::BuildLoopPrefix(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump)
			mTrueJump = mTrueJump->BuildLoopPrefix();
		if (mFalseJump)
			mFalseJump = mFalseJump->BuildLoopPrefix();

		if (mLoopHead)
		{
			mLoopPrefix = new InterCodeBasicBlock(mProc);
			mLoopPrefix->mEntryRequiredTemps = mEntryRequiredTemps;
			mLoopPrefix->mExitRequiredTemps = mEntryRequiredTemps;
			mLoopPrefix->mLocalModifiedTemps.Reset(mEntryRequiredTemps.Size());

			InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
			mLoopPrefix->Append(jins);
			mLoopPrefix->Close(this, nullptr);
		}
	}

	return mLoopPrefix ? mLoopPrefix : this;
}

bool InterCodeBasicBlock::CollectLoopBody(InterCodeBasicBlock* head, ExpandingArray<InterCodeBasicBlock*> & body)
{
	if (mLoopHead)
	{
#if 0
		return this == head;
#else
		if (this == head)
			return true;
		else if ((mTrueJump == this || mFalseJump == this) && mEntryBlocks.Size() == 2)
		{
			int j = 0;
			while (j < mInstructions.Size() && (mInstructions[j]->mDst.mTemp < 0 || !mExitRequiredTemps[mInstructions[j]->mDst.mTemp]))
				j++;
			if (j != mInstructions.Size())
				return false;
		}
		else
			return false;
#endif
	}

	if (body.IndexOf(this) != -1)
			return true;
	body.Push(this);

	for (int i = 0; i < mEntryBlocks.Size(); i++)
		if (!mEntryBlocks[i]->CollectLoopBody(head, body))
			return false;

	return true;
}

bool InterCodeBasicBlock::CollectLoopBodyRecursive(InterCodeBasicBlock* head, ExpandingArray<InterCodeBasicBlock*>& body)
{
	if (this == head)
		return true;

	if (body.IndexOf(this) != -1)
		return true;
	body.Push(this);

	for (int i = 0; i < mEntryBlocks.Size(); i++)
		if (!mEntryBlocks[i]->CollectLoopBodyRecursive(head, body))
			return false;

	return true;
}

void InterCodeBasicBlock::CollectLoopPath(const ExpandingArray<InterCodeBasicBlock*>& body, ExpandingArray<InterCodeBasicBlock*>& path)
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
					ExpandingArray<InterCodeBasicBlock*>	fpath;

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

static bool IsMatchingStore(const InterInstruction* lins, const InterInstruction* sins)
{
	if (sins->mCode == IC_STORE && 
		sins->mSrc[1].mTemp < 0 && lins->mSrc[0].mMemory == sins->mSrc[1].mMemory && lins->mSrc[0].mIntConst == sins->mSrc[1].mIntConst &&
		lins->mDst.mType == sins->mSrc[0].mType)
	{
		switch (lins->mSrc[0].mMemory)
		{
		case IM_FPARAM:
			return lins->mSrc[0].mVarIndex == sins->mSrc[1].mVarIndex;
		}
	}

	return false;
}

bool InterCodeBasicBlock::CollidingMem(InterCodeBasicBlock* block, InterInstruction * lins, int from, int to) const
{
	for (int i = from; i < to; i++)
	{
		InterInstruction* ins = block->mInstructions[i];
		if (CollidingMem(lins, ins))
			return true;
	}

	return false;
}



bool InterCodeBasicBlock::InvalidatedBy(const InterInstruction* ins, const InterInstruction* by) const
{
	if (by->mDst.mTemp >= 0 && ins->ReferencesTemp(by->mDst.mTemp))
		return true;
	else if (ins->mCode == IC_STORE || ins->mCode == IC_LOAD)
		return DestroyingMem(ins, by);
	else
		return CollidingMem(by, ins);
}

void InterCodeBasicBlock::CollectReachable(ExpandingArray<InterCodeBasicBlock*>& lblock)
{
	if (!mVisited && !mPatched)
	{
		lblock.Push(this);
		mPatched = true;

		if (mTrueJump) mTrueJump->CollectReachable(lblock);
		if (mFalseJump) mFalseJump->CollectReachable(lblock);
	}
}

bool InterCodeBasicBlock::CollectGenericLoop(ExpandingArray<InterCodeBasicBlock*>& lblocks)
{
	ExpandingArray<InterCodeBasicBlock*>	 rblocks;

	mProc->ResetPatched();
	CollectReachable(rblocks);

	mProc->ResetPatched();

	bool	changed;
	do
	{
		changed = false;

		for (int i = 0; i < rblocks.Size(); i++)
		{
			InterCodeBasicBlock* block(rblocks[i]);

			if (!block->mPatched &&
				(block->mTrueJump && (block->mTrueJump->mPatched || block->mTrueJump == this) ||
					block->mFalseJump && (block->mFalseJump->mPatched || block->mFalseJump == this)))
			{
				lblocks.Push(block);
				block->mPatched = true;
				changed = true;
			}
		}

	} while (changed);

	return lblocks.Size() > 0;
}

bool InterCodeBasicBlock::CollectSingleEntryGenericLoop(ExpandingArray<InterCodeBasicBlock*>& lblocks)
{
	if (CollectGenericLoop(lblocks))
	{
		for (int i = 0; i < lblocks.Size(); i++)
		{
			InterCodeBasicBlock* block = lblocks[i];

			if (block != this)
			{
				for (int j = 0; j < block->mEntryBlocks.Size(); j++)
					if (!lblocks.Contains(block->mEntryBlocks[j]))
						return false;
			}
		}

		return true;
	}
	else
		return false;
}



bool InterCodeBasicBlock::CollectSingleHeadLoopBody(InterCodeBasicBlock* head, InterCodeBasicBlock* tail, ExpandingArray<InterCodeBasicBlock*>& body)
{
	int i = 0;
	body.Push(head);

	if (head == tail)
		return true;

	while (i < body.Size())
	{
		InterCodeBasicBlock* block = body[i++];
		if (block->mTrueJump)
		{
			if (block->mTrueJump == head)
				return false;
			if (block->mTrueJump != tail && !body.Contains(block->mTrueJump))
				body.Push(block->mTrueJump);

			if (block->mFalseJump)
			{
				if (block->mFalseJump == head)
					return false;
				if (block->mFalseJump != tail && !body.Contains(block->mFalseJump))
					body.Push(block->mFalseJump);
			}
		}
		else
			return false;
	}

	body.Push(tail);

	return true;
}

static InterInstruction * FindSourceInstruction(InterCodeBasicBlock* block, int temp)
{
	for (;;)
	{
		int i = block->mInstructions.Size() - 1;
		while (i >= 0)
		{
			if (block->mInstructions[i]->mDst.mTemp == temp)
				return block->mInstructions[i];
			i--;
		}

		if (block->mEntryBlocks.Size() != 1)
			return nullptr;

		block = block->mEntryBlocks[0];
	}
}

bool InterCodeBasicBlock::MoveLoopHeadCheckToTail(void)
{
	bool	modified = false;
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2 && mInstructions.Size() == 2)
		{
			if (mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[1]->mCode == IC_BRANCH &&
				mInstructions[1]->mSrc[0].mTemp == mInstructions[0]->mDst.mTemp)
			{
				if (mFalseJump != this && mTrueJump->mTrueJump == this && !mTrueJump->mFalseJump)
				{
					mTrueJump->mInstructions.SetSize(mTrueJump->mInstructions.Size() - 1);
					mTrueJump->mInstructions.Push(mInstructions[0]->Clone());
					mTrueJump->mInstructions.Push(mInstructions[1]->Clone());
					mTrueJump->mTrueJump = mTrueJump;
					mTrueJump->mFalseJump = mFalseJump;
					mTrueJump->mEntryBlocks.Push(mTrueJump);
					mTrueJump->mNumEntries++;
					mFalseJump->mEntryBlocks.Push(mTrueJump);
					mFalseJump->mNumEntries++;
					mNumEntries--;
					mEntryBlocks.Remove(mEntryBlocks.IndexOf(mTrueJump));
					modified = true;
				}
			}
		}

		if (mTrueJump && mTrueJump->MoveLoopHeadCheckToTail())
			modified = true;
		if (mFalseJump && mFalseJump->MoveLoopHeadCheckToTail())
			modified = true;
	}

	return modified;
}

bool SameExitCondition(InterCodeBasicBlock* b1, InterCodeBasicBlock* b2)
{
	if (b1->mTrueJump == b2->mTrueJump && b1->mFalseJump == b2->mFalseJump)
	{
		int n1 = b1->mInstructions.Size(), n2 = b2->mInstructions.Size();
		if (n1 > 1 && n2 > 1)
		{
			if (b1->mInstructions[n1 - 1]->mCode == IC_BRANCH && b2->mInstructions[n2 - 1]->mCode == IC_BRANCH && 
				b1->mInstructions[n1 - 2]->mCode == IC_RELATIONAL_OPERATOR && 
				b1->mInstructions[n1 - 1]->mSrc[0].mTemp == b1->mInstructions[n1 - 2]->mDst.mTemp &&
				b1->mInstructions[n1 - 1]->mSrc[0].mFinal &&
				b2->mInstructions[n2 - 2]->mCode == IC_RELATIONAL_OPERATOR && 
				b2->mInstructions[n2 - 1]->mSrc[0].mTemp == b2->mInstructions[n2 - 2]->mDst.mTemp &&
				b2->mInstructions[n2 - 1]->mSrc[0].mFinal &&
				b1->mInstructions[n1 - 2]->IsEqualSource(b2->mInstructions[n2 - 2]))
			{
				return true;
			}
		}
	}
	
	return false;
}

InterCodeBasicBlock* InterCodeBasicBlock::CheckIsSimpleIntRangeBranch(const GrowingIntegerValueRangeArray& irange)
{
	int sz = mInstructions.Size();
	if (irange.Size() > 0 &&
		mTrueJump && mFalseJump && mInstructions.Size() == 2 &&
		mInstructions[sz - 1]->mCode == IC_BRANCH && 
		mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && 
		IsScalarType(mInstructions[sz - 2]->mSrc[0].mType) &&
		IsScalarType(mInstructions[sz - 2]->mSrc[1].mType) &&
		mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp && mInstructions[sz - 1]->mSrc[0].mFinal)
	{
		InterType	it = mInstructions[sz - 2]->mSrc[0].mType;

		const InterInstruction* cins = mInstructions[sz - 2];
		if ((cins->mSrc[0].mTemp < 0 || irange[cins->mSrc[0].mTemp].IsBound()) &&
			(cins->mSrc[1].mTemp < 0 || irange[cins->mSrc[1].mTemp].IsBound()))
		{
			IntegerValueRange	r0, r1;
			if (cins->mSrc[0].mTemp < 0)
				r0.SetConstant(cins->mSrc[0].mIntConst);
			else
				r0 = irange[cins->mSrc[0].mTemp];
			if (cins->mSrc[1].mTemp < 0)
				r1.SetConstant(cins->mSrc[1].mIntConst);
			else
				r1 = irange[cins->mSrc[1].mTemp];

			if (r0.mMaxValue <= SignedTypeMax(it) && r1.mMaxValue <= SignedTypeMax(it) || r0.mMinValue >= 0 && r1.mMinValue >= 0)
			{
				switch (mInstructions[sz - 2]->mOperator)
				{
				case IA_CMPEQ:
					if (r0.mMinValue > r1.mMaxValue || r0.mMaxValue < r1.mMinValue)
						return mFalseJump;
					break;
				case IA_CMPNE:
					if (r0.mMinValue > r1.mMaxValue || r0.mMaxValue < r1.mMinValue)
						return mTrueJump;
					break;
				}
			}
		}
	}

	return nullptr;
}

InterCodeBasicBlock* InterCodeBasicBlock::CheckIsConstBranch(const GrowingInstructionPtrArray& cins)
{
	if (!mFalseJump || mInstructions.Size() < 1 || mInstructions[mInstructions.Size() - 1]->mCode != IC_BRANCH)
		return nullptr;

	GrowingInstructionPtrArray	tins(cins);
	for (int i = 0; i < mInstructions.Size() - 1; i++)
	{
		InterInstruction* ins(mInstructions[i]);
		InterInstruction* nins = nullptr;
		if (IsObservable(ins->mCode))
			return nullptr;
		if (ins->mDst.mTemp >= 0)
		{
			if (ins->mCode == IC_CONSTANT)
				nins = ins;
			else if (ins->mCode == IC_LOAD && !ins->mVolatile)
			{
				int k = 0;
				while (k < tins.Size() && !(tins[k]->mCode == IC_STORE && SameMemAndSize(ins->mSrc[0], tins[k]->mSrc[1])))
					k++;
				if (k < tins.Size())
				{
					nins = new InterInstruction(ins->mLocation, IC_CONSTANT);
					nins->mDst = ins->mDst;
					nins->mConst = tins[k]->mSrc[0];
				}
			}
			else if (ins->mCode == IC_RELATIONAL_OPERATOR && IsIntegerType(ins->mSrc[0].mType))
			{
				IntegerValueRange	v0, v1;

				if (ins->mSrc[0].mTemp < 0)
					v0.SetLimit(ins->mSrc[0].mIntConst, ins->mSrc[0].mIntConst);
				else
				{
					int k = 0;
					while (k < tins.Size() && tins[k]->mDst.mTemp != ins->mSrc[0].mTemp)
						k++;
					if (k < tins.Size())
						v0 = tins[k]->mDst.mRange;
				}

				if (ins->mSrc[1].mTemp < 0)
					v1.SetLimit(ins->mSrc[1].mIntConst, ins->mSrc[1].mIntConst);
				else
				{
					int k = 0;
					while (k < tins.Size() && tins[k]->mDst.mTemp != ins->mSrc[1].mTemp)
						k++;
					if (k < tins.Size())
						v1 = tins[k]->mDst.mRange;
				}

				if (v0.IsBound() && v1.IsBound())
				{
					if (ins->mOperator == IA_CMPEQ)
					{
						if (v0.IsConstant() && v1.IsConstant() && v1.mMinValue == v0.mMinValue)
						{
							nins = new InterInstruction(ins->mLocation, IC_CONSTANT);
							nins->mDst = ins->mDst;
							nins->mConst.mType = IT_BOOL;
							nins->mConst.mIntConst = 1;
						}
						else if ((v0.mMaxValue <= SignedTypeMax(ins->mSrc[0].mType) && v1.mMaxValue <= SignedTypeMax(ins->mSrc[0].mType) || v0.mMinValue >= 0 && v1.mMinValue >= 0) && (v0.mMinValue > v1.mMaxValue || v1.mMinValue > v0.mMaxValue))
						{
							nins = new InterInstruction(ins->mLocation, IC_CONSTANT);
							nins->mDst = ins->mDst;
							nins->mConst.mType = IT_BOOL;
							nins->mConst.mIntConst = 0;
						}
					}
				}
			}
			else if (ins->mDst.mTemp >= 0 && ins->mDst.mRange.IsBound())
				nins = ins;

			if (ins->mDst.mTemp >= 0)
			{
				int k = 0;
				while (k < tins.Size() && tins[k]->mDst.mTemp != ins->mDst.mTemp)
					k++;
				if (k < tins.Size())
					tins.Remove(k);
			}

			if (nins)
				tins.Push(nins);
		}
	}

	InterInstruction* bins = mInstructions[mInstructions.Size() - 1];

	int k = 0;
	while (k < tins.Size() && tins[k]->mDst.mTemp != bins->mSrc[0].mTemp)
		k++;

	if (k < tins.Size() && tins[k]->mCode == IC_CONSTANT)
	{
		InterInstruction* cins = tins[k];

		bool	istrue = false;
		InterType	it = cins->mDst.mType;

		if (it == IT_FLOAT)
			istrue = cins->mConst.mFloatConst != 0;
		else if (it == IT_POINTER)
			istrue = cins->mConst.mMemory != IM_ABSOLUTE || cins->mConst.mIntConst != 0;
		else
			istrue = cins->mConst.mIntConst != 0;

		InterCodeBasicBlock* tblock = istrue ? mTrueJump : mFalseJump;

		InterCodeBasicBlock* xblock = this->Clone();
		InterInstruction* bins = xblock->mInstructions.Pop();
		InterInstruction* jins = new InterInstruction(bins->mLocation, IC_JUMP);
		xblock->mInstructions.Push(jins);
		xblock->mTrueJump = tblock;
		tblock->mEntryBlocks.Push(xblock);
		tblock->mNumEntries++;
		return xblock;
	}

	return nullptr;
}

bool InterCodeBasicBlock::ChangeTrueJump(InterCodeBasicBlock* block)
{
	if (block != mTrueJump)
	{
		mTrueJump->mEntryBlocks.RemoveAll(this);
		mTrueJump->mNumEntries--;
		block->mEntryBlocks.Push(this);
		block->mNumEntries++;
		mTrueJump = block;
		return true;
	}

	return false;
}

bool InterCodeBasicBlock::ChangeFalseJump(InterCodeBasicBlock* block)
{
	if (block != mFalseJump)
	{
		mFalseJump->mEntryBlocks.RemoveAll(this);
		mFalseJump->mNumEntries--;
		block->mEntryBlocks.Push(this);
		block->mNumEntries++;
		mFalseJump = block;
		return true;
	}

	return false;
}

bool InterCodeBasicBlock::ShortcutDuplicateBranches(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int sz = mInstructions.Size();
		if (sz >= 2 && mInstructions[sz - 1]->mCode == IC_BRANCH && mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[sz - 1]->mSrc[0].mTemp == mInstructions[sz - 2]->mDst.mTemp)
		{
			InterInstruction* cins = mInstructions[sz - 2];
			InterInstruction* bins = mInstructions[sz - 1];

			if (mTrueJump->mInstructions.Size() == 2 && mTrueJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mTrueJump->mInstructions[1]->mCode == IC_BRANCH && mTrueJump->mInstructions[0]->mDst.mTemp == mTrueJump->mInstructions[1]->mSrc[0].mTemp)
			{
				InterInstruction* tcins = mTrueJump->mInstructions[0];
				InterInstruction* tbins = mTrueJump->mInstructions[1];

				if (cins->mSrc[0].IsEqual(tcins->mSrc[0]) && cins->mSrc[1].IsEqual(tcins->mSrc[1]))
				{
					if (tbins->mSrc[0].mFinal)
					{
						if (cins->mOperator == tcins->mOperator)
						{
							if (ChangeTrueJump(mTrueJump->mTrueJump))
								changed = true;
						}
						else if (cins->mOperator == InvertRelational(tcins->mOperator))
						{
							if (ChangeTrueJump(mTrueJump->mFalseJump))
								changed = true;
						}
					}
					else if (bins->mSrc[0].mFinal)
					{
						if (cins->mOperator == tcins->mOperator)
						{
							if (ChangeTrueJump(mTrueJump->mTrueJump))
							{
								bins->mSrc[0].mTemp = cins->mDst.mTemp = tcins->mDst.mTemp;
								bins->mSrc[0].mFinal = false;
								mExitRequiredTemps += bins->mSrc[0].mTemp;
								changed = true;
							}
						}
						else if (cins->mOperator == InvertRelational(tcins->mOperator))
						{
							if (ChangeTrueJump(mTrueJump->mFalseJump))
							{
								bins->mSrc[0].mTemp = cins->mDst.mTemp = tcins->mDst.mTemp;
								bins->mSrc[0].mFinal = false;
								cins->mOperator = tcins->mOperator;
								mExitRequiredTemps += bins->mSrc[0].mTemp;
								InterCodeBasicBlock* t = mTrueJump; mTrueJump = mFalseJump; mFalseJump = t;
								changed = true;
							}
						}
					}
				}
			}
			if (mFalseJump->mInstructions.Size() == 2 && mFalseJump->mInstructions[0]->mCode == IC_RELATIONAL_OPERATOR && mFalseJump->mInstructions[1]->mCode == IC_BRANCH && mFalseJump->mInstructions[0]->mDst.mTemp == mFalseJump->mInstructions[1]->mSrc[0].mTemp)
			{
				InterInstruction* tcins = mFalseJump->mInstructions[0];
				InterInstruction* tbins = mFalseJump->mInstructions[1];

				if (cins->mSrc[0].IsEqual(tcins->mSrc[0]) && cins->mSrc[1].IsEqual(tcins->mSrc[1]))
				{
					if (tbins->mSrc[0].mFinal)
					{
						if (cins->mOperator == tcins->mOperator)
						{
							if (ChangeFalseJump(mFalseJump->mFalseJump))
								changed = true;
						}
						else if (cins->mOperator == InvertRelational(tcins->mOperator))
						{
							if (ChangeFalseJump(mFalseJump->mTrueJump))
								changed = true;
						}
					}
					else if (bins->mSrc[0].mFinal)
					{
						if (cins->mOperator == tcins->mOperator)
						{
							if (ChangeFalseJump(mFalseJump->mFalseJump))
							{
								bins->mSrc[0].mTemp = cins->mDst.mTemp = tcins->mDst.mTemp;
								bins->mSrc[0].mFinal = false;
								mExitRequiredTemps += bins->mSrc[0].mTemp;
								changed = true;
							}
						}
						else if (cins->mOperator == InvertRelational(tcins->mOperator))
						{
							if (ChangeFalseJump(mFalseJump->mTrueJump))
							{
								bins->mSrc[0].mTemp = cins->mDst.mTemp = tcins->mDst.mTemp;
								bins->mSrc[0].mFinal = false;
								cins->mOperator = tcins->mOperator;
								mExitRequiredTemps += bins->mSrc[0].mTemp;
								InterCodeBasicBlock* t = mTrueJump; mTrueJump = mFalseJump; mFalseJump = t;
								changed = true;
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->ShortcutDuplicateBranches())
			changed = true;
		if (mFalseJump && mFalseJump->ShortcutDuplicateBranches())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::ShortcutConstBranches(const GrowingInstructionPtrArray& cins)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		GrowingInstructionPtrArray	tins(cins);
		if (mNumEntries > 1)
			tins.SetSize(0);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			InterInstruction* nins = nullptr;

			if (ins->mCode == IC_CONSTANT)
				nins = ins;
			else
			{
				if (IsObservable(ins->mCode))
				{
					int k = 0;
					while (k < tins.Size())
					{
						if (tins[k]->mCode == IC_STORE && DestroyingMem(tins[k], ins))
							tins.Remove(k);
						else
							k++;
					}
				}
				else if (ins->mDst.mTemp >= 0 && ins->mDst.mRange.IsBound())
					nins = ins;

				if (ins->mCode == IC_STORE && !ins->mVolatile && ins->mSrc[0].mTemp < 0)
					nins = ins;
			}

			if (ins->mDst.mTemp >= 0)
			{
				int k = 0;
				while (k < tins.Size() && tins[k]->mDst.mTemp != ins->mDst.mTemp)
					k++;
				if (k < tins.Size())
					tins.Remove(k);
			}

			if (nins)
				tins.Push(nins);
		}

		
		if (mTrueJump)
		{
			InterCodeBasicBlock* tblock = mTrueJump->CheckIsConstBranch(tins);
			if (!tblock) tblock = mTrueJump->CheckIsSimpleIntRangeBranch(mTrueValueRange);

			if (tblock)
			{
				mTrueJump->mEntryBlocks.RemoveAll(this);
				mTrueJump->mNumEntries--;
				tblock->mEntryBlocks.Push(this);
				tblock->mNumEntries++;
				mTrueJump = tblock;
			}
			if (mTrueJump->ShortcutConstBranches(tins))
				changed = true;
		}
		if (mFalseJump)
		{
			InterCodeBasicBlock* tblock = mFalseJump->CheckIsConstBranch(tins);
			if (!tblock) tblock = mFalseJump->CheckIsSimpleIntRangeBranch(mFalseValueRange);

			if (tblock)
			{
				mFalseJump->mEntryBlocks.RemoveAll(this);
				mFalseJump->mNumEntries--;
				tblock->mEntryBlocks.Push(this);
				tblock->mNumEntries++;
				mFalseJump = tblock;
			}
			if (mFalseJump->ShortcutConstBranches(tins))
				changed = true;
		}
	}

	return changed;
}


void InterCodeBasicBlock::ReplaceCopyFill(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2 && mTrueJump == this)
		{
			if (mInstructions.Size() == 4 &&
				mInstructions[0]->mCode == IC_STORE &&
					mInstructions[0]->mSrc[0].mTemp < 0 && 
					mInstructions[0]->mSrc[0].mType == IT_INT8 && 
					mInstructions[0]->mSrc[1].mTemp >= 0 &&
				mInstructions[1]->mCode == IC_LEA &&
					mInstructions[1]->mDst.mTemp == mInstructions[0]->mSrc[1].mTemp &&
					mInstructions[1]->mSrc[1].mTemp == mInstructions[0]->mSrc[1].mTemp &&
					mInstructions[1]->mSrc[0].mTemp < 0 &&
					mInstructions[1]->mSrc[0].mIntConst == 1 &&
				mInstructions[2]->mCode == IC_RELATIONAL_OPERATOR &&
					mInstructions[2]->mSrc[1].mTemp == mInstructions[1]->mDst.mTemp &&
					mInstructions[2]->mSrc[0].mTemp < 0 &&
				mInstructions[3]->mCode == IC_BRANCH &&
					mInstructions[3]->mSrc[0].mTemp == mInstructions[2]->mDst.mTemp
				)
			{
				int reg = mInstructions[0]->mSrc[1].mTemp;

				InterCodeBasicBlock* pblock = mEntryBlocks[0];
				if (pblock == this)
					pblock = mEntryBlocks[1];

				const InterInstruction* cins = mInstructions[2];
				const InterInstruction* bins = pblock->FindByDst(reg);

				if (bins && bins->mCode == IC_CONSTANT)
				{
					if (bins->mConst.mMemory == cins->mSrc[0].mMemory)
					{
						int64	size = cins->mSrc[0].mIntConst - bins->mConst.mIntConst;
						if (size > 0)
						{
							mInstructions[0]->mCode = IC_FILL;
							mInstructions[0]->mConst.mOperandSize = int(size);
							mInstructions[0]->mSrc[1] = bins->mConst;
							mInstructions[1]->mSrc[0].mIntConst = size;
							mInstructions[2]->mCode = IC_NONE; 
							mInstructions[2]->mDst.mTemp = -1;
							mInstructions[2]->mNumOperands = 0;
							mInstructions[3]->mCode = IC_JUMP;
							mInstructions[3]->mDst.mTemp = -1;
							mInstructions[3]->mNumOperands = 0;

							mNumEntries--;
							mEntryBlocks.RemoveAll(this);
							mTrueJump = mFalseJump;
							mFalseJump = nullptr;
						}
					}
				}
			}
		}

		if (mTrueJump) mTrueJump->ReplaceCopyFill();
		if (mFalseJump) mFalseJump->ReplaceCopyFill();
	}
}

bool InterCodeBasicBlock::MergeLoopTails(void)
{
	bool	modified = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() > 2)
		{
			int fi = 0;
			while (fi < mEntryBlocks.Size() && mEntryBlocks[fi] == mLoopPrefix)
				fi++;

			int i = fi + 1;
			while (i < mEntryBlocks.Size() && (mEntryBlocks[i] == mLoopPrefix || SameExitCondition(mEntryBlocks[i], mEntryBlocks[fi])))
				i++;
			if (i == mEntryBlocks.Size())
			{
				int n = 2;
				bool match = true;
				while (i == mEntryBlocks.Size() && n < mEntryBlocks[fi]->mInstructions.Size())
				{
					i = fi + 1;
					while (i < mEntryBlocks.Size() && (mEntryBlocks[i] == mLoopPrefix || 
						n < mEntryBlocks[i]->mInstructions.Size() &&
						mEntryBlocks[i]->mInstructions[mEntryBlocks[i]->mInstructions.Size() - n - 1]->IsEqual(mEntryBlocks[fi]->mInstructions[mEntryBlocks[fi]->mInstructions.Size() - n - 1])))
						i++;

					if (i == mEntryBlocks.Size())
						n++;
				}

				InterCodeBasicBlock* block = new InterCodeBasicBlock(mProc);
				block->mTrueJump = mEntryBlocks[fi]->mTrueJump;
				block->mFalseJump = mEntryBlocks[fi]->mFalseJump;
				for (int i = 0; i < n; i++)
					block->mInstructions.Push(mEntryBlocks[fi]->mInstructions[mEntryBlocks[fi]->mInstructions.Size() - n + i]);
				
				InterInstruction* bins = mEntryBlocks[fi]->mInstructions.Last();

				i = 0;
				while (i < mEntryBlocks.Size())
				{
					if (mEntryBlocks[i] == mLoopPrefix)
						i++;
					else
					{
						mEntryBlocks[i]->mInstructions.SetSize(mEntryBlocks[i]->mInstructions.Size() - n);
						mEntryBlocks[i]->mFalseJump = nullptr;
						mEntryBlocks[i]->mTrueJump = block;
						InterInstruction* jins = new InterInstruction(bins->mLocation, IC_JUMP);
						mEntryBlocks[i]->mInstructions.Push(jins);
						block->mEntryBlocks.Push(mEntryBlocks[i]);
						block->mNumEntries++;
						mEntryBlocks.Remove(i);
					}
				}

				mNumEntries = 2;
				mEntryBlocks.Push(block);

				modified = true;
			}
		}

		if (mTrueJump && mTrueJump->MergeLoopTails())
			modified = true;
		if (mFalseJump && mFalseJump->MergeLoopTails())
			modified = true;
	}

	return modified;
}

bool IsSingleLoopAssign(int at, InterCodeBasicBlock* block, const ExpandingArray<InterCodeBasicBlock*>& body)
{
	InterInstruction* ai = block->mInstructions[at];
	if (ai->mDst.mTemp < 0)
		return true;
	if (block->IsTempReferencedInRange(0, at, ai->mDst.mTemp))
		return false;
	if (block->IsTempModifiedInRange(at + 1, block->mInstructions.Size(), ai->mDst.mTemp))
		return false;
	for (int i = 1; i < body.Size(); i++)
		if (body[i]->IsTempModified(ai->mDst.mTemp))
			return false;
	return true;
}

bool IsLoopInvariantTemp(int tmp, const ExpandingArray<InterCodeBasicBlock*>& body)
{
	if (tmp < 0)
		return true;

	for (int i = 0; i < body.Size(); i++)
		if (body[i]->IsTempModified(tmp))
			return false;
	return true;
}

static bool IsSimpleFactor(int64 val)
{
	return (val == 1 || val == 2 || val == 4 || val == 8);
}


void InterCodeBasicBlock::LimitLoopIndexIntegerRangeSets(void)
{
	if (!mVisited)
	{
		if (mLoopHead && mEntryBlocks.Size() == 2)
		{
			InterCodeBasicBlock* tail;

			if (mEntryBlocks[0] == mLoopPrefix)
				tail = mEntryBlocks[1];
			else
				tail = mEntryBlocks[0];

			// Inner exit loop
			ExpandingArray<InterCodeBasicBlock*> body;

			if (CollectSingleEntryGenericLoop(body))
			{
				ExpandingArray<InterCodeBasicBlock*> exits;
				for (int i = 0; i < body.Size(); i++)
				{
					if (body[i]->mTrueJump && !body.Contains(body[i]->mTrueJump) || body[i]->mFalseJump && !(body.Contains(body[i]->mFalseJump)))
						exits.Push(body[i]);
				}

				if (exits.Size() == 1)
				{
					InterCodeBasicBlock* eblock = exits[0];

					int esz = eblock->mInstructions.Size();
					if (esz >= 2)
					{
						InterInstruction* ci = eblock->mInstructions[esz - 2];
						InterInstruction* bi = eblock->mInstructions[esz - 1];

						if (ci->mCode == IC_RELATIONAL_OPERATOR && (
							ci->mSrc[0].mTemp < 0 && ci->mSrc[1].mTemp >= 0 && IsIntegerType(ci->mSrc[1].mType) ||
							ci->mSrc[1].mTemp < 0 && ci->mSrc[0].mTemp >= 0 && IsIntegerType(ci->mSrc[0].mType)) &&
							bi->mCode == IC_BRANCH && bi->mSrc[0].mTemp == ci->mDst.mTemp)
						{
							// single compare
							int icsrc = ci->mSrc[0].mTemp >= 0 ? 0 : 1;							
							int itemp = ci->mSrc[icsrc].mTemp;

							if (mDominator->mTrueValueRange[itemp].IsConstant())
							{
								// Find increment in tail
								int i = 0;
								while (i < tail->mInstructions.Size() && tail->mInstructions[i]->mDst.mTemp != itemp)
									i++;
								if (i < tail->mInstructions.Size())
								{
									InterInstruction* ai = tail->mInstructions[i];
									if ((ai->mOperator == IA_ADD || ai->mOperator == IA_SUB) && (ai->mSrc[0].mTemp == itemp && ai->mSrc[1].mTemp < 0 || ai->mSrc[1].mTemp == itemp && ai->mSrc[0].mTemp < 0))
									{
										int j = 0;
										while (j < body.Size() && (body[j] == tail || !body[j]->IsTempModified(itemp)))
											j++;
										if (j == body.Size() && !tail->IsTempModifiedInRange(0, i, itemp) && !tail->IsTempModifiedInRange(i + 1, tail->mInstructions.Size(), itemp))
										{
											int iasrc = ai->mSrc[0].mTemp >= 0 ? 0 : 1;

											int64 istart = mDominator->mTrueValueRange[itemp].mMaxValue;
											int64 iinc, iend;
											if (ai->mOperator == IA_ADD)
												iinc = ai->mSrc[1 - iasrc].mIntConst;
											else
												iinc = -ai->mSrc[1 - iasrc].mIntConst;

											InterOperator	op;
											if (ci->mSrc[0].mTemp < 0)
											{
												iend = ci->mSrc[0].mIntConst;
												op = ci->mOperator;
											}
											else
											{
												iend = ci->mSrc[1].mIntConst;
												op = MirrorRelational(ci->mOperator);
											}

											if (body.Contains(eblock->mFalseJump))
												op = InvertRelational(op);

//											printf("Collected loop %s:%d, %d, %d, %d, [%d %d+%d!=%d] %d (%d..%d)\n", mProc->mIdent->mString, mIndex, body.Size(), eblock->mIndex, tail->mIndex, itemp, int(istart), int(iinc), int(iend), op, int(ai->mSrc[iasrc].mRange.mMinValue), int(ai->mSrc[iasrc].mRange.mMaxValue));

											if (istart < iend && iinc > 0) // counting up
											{
												if (op == IA_CMPNE)
												{
													if ((iend - istart) % iinc == 0)
													{
														ai->mSrc[iasrc].mRange.LimitMin(istart);
														ai->mSrc[iasrc].mRange.LimitMax(iend - iinc);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

		}

		mVisited = true;

		if (mTrueJump) mTrueJump->LimitLoopIndexIntegerRangeSets();
		if (mFalseJump) mFalseJump->LimitLoopIndexIntegerRangeSets();
	}
}

void InterCodeBasicBlock::LimitLoopIndexRanges(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2)
		{
			InterCodeBasicBlock* tail, * post;

			if (mEntryBlocks[0] == mLoopPrefix)
				tail = mEntryBlocks[1];
			else
				tail = mEntryBlocks[0];

			if (tail->mTrueJump == this)
				post = tail->mFalseJump;
			else
				post = tail->mTrueJump;

			if (post && post->mNumEntries == 1)
			{
				ExpandingArray<InterCodeBasicBlock*> body;

				if (tail->CollectSingleHeadLoopBody(this, tail, body))
				{
					int tz = tail->mInstructions.Size();
					if (tz > 2)
					{
						InterInstruction* ai = tail->mInstructions[tz - 3];
						InterInstruction* ci = tail->mInstructions[tz - 2];
						InterInstruction* bi = tail->mInstructions[tz - 1];

						if (ai->mCode == IC_BINARY_OPERATOR && ai->mOperator == IA_ADD && ai->mSrc[0].mTemp < 0 && ai->mDst.mTemp == ai->mSrc[1].mTemp && ai->mSrc[0].mIntConst > 1 && IsIntegerType(ai->mDst.mType) &&
							ci->mCode == IC_RELATIONAL_OPERATOR && ci->mOperator == IA_CMPLU && ci->mSrc[0].mTemp < 0 && ci->mSrc[1].mTemp == ai->mDst.mTemp &&
							bi->mCode == IC_BRANCH && bi->mSrc[0].mTemp == ci->mDst.mTemp && !post->mEntryRequiredTemps[ai->mDst.mTemp] &&
							!tail->IsTempModifiedInRange(0, tz - 3, ai->mDst.mTemp))
						{
							int i = 0;
							while (i + 1 < body.Size() && !body[i]->IsTempModifiedInRange(0, body[i]->mInstructions.Size(), ai->mDst.mTemp))
								i++;
							if (i + 1 == body.Size())
							{
								InterInstruction* si = FindSourceInstruction(mLoopPrefix, ai->mDst.mTemp);
								if (si && si->mCode == IC_CONSTANT && si->mConst.mIntConst < ci->mSrc[0].mIntConst)
								{
									int64	climit = ((ci->mSrc[0].mIntConst - si->mConst.mIntConst + ai->mSrc[0].mIntConst - 1) / ai->mSrc[0].mIntConst - 1) * ai->mSrc[0].mIntConst + si->mConst.mIntConst;
									if (climit + 1 != ci->mSrc[0].mIntConst)
									{
//										printf("LimitLoopIndex %s:%d, %d..%d by %d to %d\n", mProc->mIdent->mString, mIndex, int(si->mConst.mIntConst), int(ci->mSrc[0].mIntConst), int(ai->mSrc[0].mIntConst), int(climit + 1));
										ci->mSrc[0].mIntConst = climit + 1;
									}
								}
							}
						}
					}
				}
			}

		}

		if (mTrueJump) mTrueJump->LimitLoopIndexRanges();
		if (mFalseJump) mFalseJump->LimitLoopIndexRanges();
	}

}

bool InterCodeBasicBlock::SingleTailLoopOptimization(const NumberSet& aliasedParams, const GrowingVariableArray& staticVars)
{
	bool	modified = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2)
		{
			InterCodeBasicBlock* tail, *post;

			if (mEntryBlocks[0] == mLoopPrefix)
				tail = mEntryBlocks[1];
			else
				tail = mEntryBlocks[0];

			if (tail->mTrueJump == this)
				post = tail->mFalseJump;
			else
				post = tail->mTrueJump;

			if (post && post->mNumEntries == 1)
			{
				ExpandingArray<InterCodeBasicBlock*> body;

				if (tail->CollectSingleHeadLoopBody(this, tail, body))
				{
					int tz = tail->mInstructions.Size();

#if 1
					if (tz > 2)
					{
						InterInstruction* ai = tail->mInstructions[tz - 3];
						InterInstruction* ci = tail->mInstructions[tz - 2];
						InterInstruction* bi = tail->mInstructions[tz - 1];

						if (ai->mCode == IC_BINARY_OPERATOR && ai->mOperator == IA_ADD && ai->mSrc[0].mTemp < 0 && ai->mDst.mTemp == ai->mSrc[1].mTemp && ai->mSrc[0].mIntConst > 0 && IsIntegerType(ai->mDst.mType) &&
							ci->mCode == IC_RELATIONAL_OPERATOR && ci->mOperator == IA_CMPLU && ci->mSrc[0].mTemp < 0 && ci->mSrc[1].mTemp == ai->mDst.mTemp &&
							bi->mCode == IC_BRANCH && bi->mSrc[0].mTemp == ci->mDst.mTemp && !post->mEntryRequiredTemps[ai->mDst.mTemp] &&
							!tail->IsTempReferencedInRange(0, tz - 3, ai->mDst.mTemp))
						{
							int i = 0;
							while (i + 1 < body.Size() && !body[i]->IsTempReferencedInRange(0, body[i]->mInstructions.Size(), ai->mDst.mTemp))
								i++;
							if (i + 1 == body.Size())
							{
								InterInstruction* si = FindSourceInstruction(mLoopPrefix, ai->mDst.mTemp);
								if (si && si->mCode == IC_CONSTANT)
								{
									int64	num = (ci->mSrc[0].mIntConst - si->mConst.mIntConst + ai->mSrc[0].mIntConst - 1) / ai->mSrc[0].mIntConst;
									if (num > 255 && num < 32768)
									{
										ai->mSrc[0].mIntConst = 1;

										ci->mOperator = IA_CMPNE;
										ci->mSrc[0].mIntConst = 0;
										ci->mSrc[0].mRange.SetLimit(0, 0);

										InterInstruction* mins = new InterInstruction(si->mLocation, IC_CONSTANT);
										mins->mConst.mType = ai->mDst.mType;
										mins->mConst.mIntConst = -num;
										mins->mDst = ai->mDst;
										mins->mDst.mRange.SetLimit(-num, -num);
										mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, mins);

										for (int k = 0; k < body.Size(); k++)
										{
											if (body[k]->mEntryValueRange.Size())
												body[k]->mEntryValueRange[ai->mSrc[1].mTemp].SetLimit(-num, -1);
										}

										ai->mSrc[1].mRange.SetLimit(-num, -1);
										ai->mDst.mRange.SetLimit(-num + 1, 0);
										ci->mSrc[1].mRange.SetLimit(-num + 1, 0);
									}
									else if (num > 0)
									{
										ai->mOperator = IA_SUB;
										ai->mSrc[0].mIntConst = 1;
										ci->mOperator = IA_CMPGU;
										ci->mSrc[0].mIntConst = 0;
										ci->mSrc[0].mRange.SetLimit(0, 0);

										InterInstruction* mins = new InterInstruction(si->mLocation, IC_CONSTANT);
										mins->mConst.mType = ai->mDst.mType;
										mins->mConst.mIntConst = num;
										mins->mDst = ai->mDst;
										mins->mDst.mRange.SetLimit(num, num);
										mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, mins);

										for (int k = 0; k < body.Size(); k++)
										{
											if (body[k]->mEntryValueRange.Size())
												body[k]->mEntryValueRange[ai->mSrc[1].mTemp].SetLimit(1, num);
										}

										if (mEntryValueRange.Size())
											mEntryValueRange[ai->mSrc[1].mTemp].SetLimit(1, num);

										ai->mSrc[1].mRange.SetLimit(1, num);
										ai->mDst.mRange.SetLimit(0, num - 1);
										ci->mSrc[1].mRange.SetLimit(0, num - 1);
									}

									modified = true;
								}
							}
						}
						else if (ai->mCode == IC_BINARY_OPERATOR && ai->mOperator == IA_ADD && ai->mSrc[0].mTemp < 0 && ai->mDst.mTemp == ai->mSrc[1].mTemp && ai->mSrc[0].mIntConst == 1 && IsIntegerType(ai->mDst.mType) &&
							ci->mCode == IC_RELATIONAL_OPERATOR && ci->mOperator == IA_CMPLU && ci->mSrc[0].mTemp >= 0 && ci->mSrc[0].IsPositive() && ci->mSrc[1].mTemp == ai->mDst.mTemp &&
							bi->mCode == IC_BRANCH && bi->mSrc[0].mTemp == ci->mDst.mTemp && !post->mEntryRequiredTemps[ai->mDst.mTemp] &&
							!tail->IsTempReferencedInRange(0, tz - 3, ai->mDst.mTemp) && !tail->IsTempModifiedInRange(0, tz - 3, ci->mSrc[0].mTemp))
						{
							int i = 0;
							while (i + 1 < body.Size() &&
								!body[i]->IsTempReferenced(ai->mDst.mTemp) &&
								!body[i]->IsTempModified(ci->mSrc[0].mTemp))
								i++;
							if (i + 1 == body.Size())
							{
								InterInstruction* si = FindSourceInstruction(mLoopPrefix, ai->mDst.mTemp);
								if (si && si->mCode == IC_CONSTANT && si->mConst.mIntConst >= 0)
								{
									int64 num = ci->mSrc[0].mRange.mMaxValue - si->mConst.mIntConst;
									IntegerValueRange::State	bound = ci->mSrc[0].mRange.mMaxState;

									InterInstruction* mins = new InterInstruction(si->mLocation, IC_BINARY_OPERATOR);
									mins->mOperator = IA_SUB;
									mins->mSrc[0] = si->mConst;
									mins->mSrc[1] = ci->mSrc[0];
									mins->mDst = ai->mDst;
									mins->mDst.mRange.SetBounds(IntegerValueRange::S_BOUND, 1, bound, num);

									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, mins);

									ai->mOperator = IA_SUB;
									ai->mSrc[0].mIntConst = 1;
									ci->mOperator = IA_CMPGU;
									ci->mSrc[0].mTemp = -1;
									ci->mSrc[0].mIntConst = 0;
									ci->mSrc[0].mRange.SetLimit(0, 0);

									for (int k = 0; k < body.Size(); k++)
									{
										if (body[k]->mEntryValueRange.Size())
											body[k]->mEntryValueRange[ai->mSrc[1].mTemp].SetBounds(IntegerValueRange::S_BOUND, 1, bound, num);
									}

									if (mEntryValueRange.Size())
										mEntryValueRange[ai->mSrc[1].mTemp].SetBounds(IntegerValueRange::S_BOUND, 1, bound, num);

									ai->mSrc[1].mRange.SetBounds(IntegerValueRange::S_BOUND, 1, bound, num);
									ai->mDst.mRange.SetBounds(IntegerValueRange::S_BOUND, 0, bound, num - 1);
									ci->mSrc[1].mRange.SetBounds(IntegerValueRange::S_BOUND, 0, bound, num - 1);

									modified = true;
								}
							}
						}
					}
#endif
#if 1
					GrowingIntArray		indexScale(0);

					if (!modified)
					{


						int tz = tail->mInstructions.Size();
						for (int i = 0; i < tz; i++)
						{
							InterInstruction* ai = tail->mInstructions[i];
							if (ai->mCode == IC_BINARY_OPERATOR && ai->mOperator == IA_ADD && ai->mSrc[0].mTemp < 0 && ai->mDst.mTemp == ai->mSrc[1].mTemp && ai->mSrc[0].mIntConst != 0 && IsIntegerType(ai->mDst.mType) &&
								!tail->IsTempModifiedInRange(i + 1, tz, ai->mDst.mTemp) &&
								!tail->IsTempModifiedInRange(0, i - 1, ai->mDst.mTemp))
							{
								int j = 0;
								while (j + 1 < body.Size() && !body[j]->IsTempModified(ai->mDst.mTemp))
									j++;
								if (j + 1 == body.Size())
								{
									indexScale[ai->mDst.mTemp] = (int)ai->mSrc[0].mIntConst;
								}
							}
						}
					}

					bool	hasStore = false;
					for (int j = 0; j < body.Size(); j++)
					{
						int sz = body[j]->mInstructions.Size();
						for (int i = 0; i < sz; i++)
						{
							InterInstruction* ins = body[j]->mInstructions[i];
							if (IsObservable(ins->mCode))
								hasStore = true;
						}
					}

					int i = 0;
					while (i < mInstructions.Size())
					{
						InterInstruction* lins = mInstructions[i];

						if (lins->mCode == IC_BINARY_OPERATOR)
						{
							if (lins->mOperator == IA_MUL && lins->mSrc[0].mTemp < 0 && (lins->mDst.IsNotUByte() || !IsSimpleFactor(lins->mSrc[0].mIntConst)) && lins->mSrc[1].mTemp >= 0 && indexScale[lins->mSrc[1].mTemp] != 0 && IsSingleLoopAssign(i, this, body))
							{
								mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
								mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
								mEntryRequiredTemps += lins->mDst.mTemp;
								tail->mExitRequiredTemps += lins->mDst.mTemp;
								tail->mEntryRequiredTemps += lins->mDst.mTemp;
								mInstructions.Remove(i);

								InterInstruction* ains = new InterInstruction(lins->mLocation, IC_BINARY_OPERATOR);
								ains->mOperator = IA_ADD;
								ains->mDst = lins->mDst;
								ains->mSrc[1] = lins->mDst;
								ains->mSrc[0] = lins->mSrc[0];
								ains->mSrc[0].mIntConst *= indexScale[lins->mSrc[1].mTemp];
								ains->mDst.mRange.AddConstValue(ains->mDst.mType, ains->mSrc[0].mIntConst);
								tail->AppendBeforeBranch(ains);

								indexScale[ains->mDst.mTemp] = (int)ains->mSrc[0].mIntConst;

								modified = true;
								continue;
							}
							else if (lins->mOperator == IA_ADD && lins->mSrc[0].mTemp >= 0 && indexScale[lins->mSrc[0].mTemp] != 0 && IsSingleLoopAssign(i, this, body))
							{
								if (i + 1 < mInstructions.Size() && mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == lins->mDst.mTemp)
									;
								else if (lins->mSrc[1].mTemp < 0 || IsLoopInvariantTemp(lins->mSrc[1].mTemp, body))
								{
									assert(IsIntegerType(lins->mDst.mType));

									int s = indexScale[lins->mSrc[0].mTemp];

									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
									mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
									mEntryRequiredTemps += lins->mDst.mTemp;
									tail->mExitRequiredTemps += lins->mDst.mTemp;
									tail->mEntryRequiredTemps += lins->mDst.mTemp;
									mInstructions.Remove(i);

									InterInstruction* ains = new InterInstruction(lins->mLocation, IC_BINARY_OPERATOR);
									ains->mOperator = IA_ADD;
									ains->mDst = lins->mDst;
									ains->mSrc[1] = lins->mDst;
									ains->mSrc[0].mType = lins->mDst.mType;
									ains->mSrc[0].mTemp = -1;
									ains->mSrc[0].mIntConst = s;
									tail->AppendBeforeBranch(ains);

									if (s > 0)
										ains->mDst.mRange.mMaxValue += s;
									else
										ains->mDst.mRange.mMinValue += s;

									indexScale[ains->mDst.mTemp] = s;

									modified = true;
									continue;
								}
							}
							else if ((lins->mOperator == IA_ADD || lins->mOperator == IA_SUB) && lins->mSrc[1].mTemp >= 0 && lins->mSrc[1].mTemp != lins->mDst.mTemp && 
								      indexScale[lins->mSrc[1].mTemp] != 0 && IsSingleLoopAssign(i, this, body))
							{
								if (i + 1 < mInstructions.Size() && mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == lins->mDst.mTemp)
									;
								else if (lins->mSrc[0].mTemp < 0 || IsLoopInvariantTemp(lins->mSrc[0].mTemp, body))
								{
									assert(IsIntegerType(lins->mDst.mType));

									int s = indexScale[lins->mSrc[1].mTemp];

									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
									mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
									mEntryRequiredTemps += lins->mDst.mTemp;
									tail->mExitRequiredTemps += lins->mDst.mTemp;
									tail->mEntryRequiredTemps += lins->mDst.mTemp;
									mInstructions.Remove(i);

									InterInstruction* ains = new InterInstruction(lins->mLocation, IC_BINARY_OPERATOR);
									ains->mOperator = IA_ADD;
									ains->mDst = lins->mDst;
									ains->mSrc[1] = lins->mDst;
									ains->mSrc[0].mType = lins->mDst.mType;
									ains->mSrc[0].mTemp = -1;
									ains->mSrc[0].mIntConst = s;
									tail->AppendBeforeBranch(ains);
									ains->mDst.mRange.AddConstValue(ains->mDst.mType, s);

									indexScale[ains->mDst.mTemp] = s;

									modified = true;
									continue;
								}
							}
						}
						else if (lins->mCode == IC_CONVERSION_OPERATOR && lins->mOperator == IA_EXT8TO16U && i + 1 < mInstructions.Size() && indexScale[lins->mSrc[0].mTemp] != 0 && IsSingleLoopAssign(i, this, body))
						{
							InterInstruction* nins = mInstructions[i + 1];

							if (nins->mCode == IC_BINARY_OPERATOR)
							{
								if (nins->mOperator == IA_MUL && nins->mSrc[0].mTemp < 0 && (nins->mDst.IsNotUByte() || !IsSimpleFactor(nins->mSrc[0].mIntConst)) && nins->mSrc[1].mTemp == lins->mDst.mTemp && nins->mSrc[1].mFinal && IsSingleLoopAssign(i + 1, this, body))
								{
									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, nins);
									mLoopPrefix->mExitRequiredTemps += nins->mDst.mTemp;
									mEntryRequiredTemps += nins->mDst.mTemp;
									tail->mExitRequiredTemps += nins->mDst.mTemp;
									tail->mEntryRequiredTemps += nins->mDst.mTemp;
									mInstructions.Remove(i);
									mInstructions.Remove(i);

									InterInstruction* ains = new InterInstruction(nins->mLocation, IC_BINARY_OPERATOR);
									ains->mOperator = IA_ADD;
									ains->mDst = nins->mDst;
									ains->mSrc[1] = nins->mDst;
									ains->mSrc[0] = nins->mSrc[0];
									ains->mSrc[0].mIntConst *= indexScale[lins->mSrc[0].mTemp];
									tail->AppendBeforeBranch(ains);
									ains->mDst.mRange.AddConstValue(ains->mDst.mType, ains->mSrc[0].mIntConst);

									indexScale[ains->mDst.mTemp] = (int)ains->mSrc[0].mIntConst;

									modified = true;
									continue;
								}
							}
#if 1
							if (nins->mCode == IC_LEA && nins->mSrc[0].mTemp == lins->mDst.mTemp)
							{

							}
							else if (nins->mCode == IC_BINARY_OPERATOR && nins->mOperator == IA_ADD && nins->mSrc[0].mTemp == lins->mDst.mTemp && !nins->mDst.IsNotUByte())
							{
							}
							else if (lins->mSrc[0].IsUByte()) // ensure no overflow
							{
								int s = indexScale[lins->mSrc[0].mTemp];

								mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
								mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
								mEntryRequiredTemps += lins->mDst.mTemp;
								tail->mExitRequiredTemps += lins->mDst.mTemp;
								tail->mEntryRequiredTemps += lins->mDst.mTemp;
								mInstructions.Remove(i);

								InterInstruction* ains = new InterInstruction(lins->mLocation, IC_BINARY_OPERATOR);
								ains->mOperator = IA_ADD;
								ains->mDst = lins->mDst;
								ains->mSrc[1] = lins->mDst;
								ains->mSrc[0].mType = lins->mDst.mType;
								ains->mSrc[0].mTemp = -1;
								ains->mSrc[0].mIntConst = s;
								tail->AppendBeforeBranch(ains, true);
								ains->mDst.mRange.AddConstValue(ains->mDst.mType, s);

								indexScale[ains->mDst.mTemp] = s;

								modified = true;
								continue;
							}
#endif
						}
						else if (lins->mCode == IC_LEA)
						{
							if (lins->mSrc[0].mTemp >= 0 && !lins->mSrc[0].IsUByte() && indexScale[lins->mSrc[0].mTemp] != 0 && IsSingleLoopAssign(i, this, body) && IsLoopInvariantTemp(lins->mSrc[1].mTemp, body))
							{
								mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
								mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
								mEntryRequiredTemps += lins->mDst.mTemp;
								tail->mExitRequiredTemps += lins->mDst.mTemp;
								tail->mEntryRequiredTemps += lins->mDst.mTemp;
								mInstructions.Remove(i);

								InterInstruction* ains = new InterInstruction(lins->mLocation, IC_LEA);
								ains->mDst = lins->mDst;
								ains->mSrc[1] = lins->mDst;
								ains->mSrc[0].mTemp = -1;
								ains->mSrc[0].mType = IT_INT16;
								ains->mSrc[0].mIntConst = indexScale[lins->mSrc[0].mTemp];

								if (IsTempModifiedInRange(0, i, lins->mSrc[0].mTemp))
									mInstructions.Insert(i, ains);
								else
									tail->AppendBeforeBranch(ains);

								modified = true;
								continue;
							}
						}

						if (lins->mCode == IC_BINARY_OPERATOR || lins->mCode == IC_CONSTANT || lins->mCode == IC_UNARY_OPERATOR ||
							lins->mCode == IC_CONVERSION_OPERATOR || lins->mCode == IC_SELECT ||
							lins->mCode == IC_LEA ||
							lins->mCode == IC_RELATIONAL_OPERATOR || (lins->mCode == IC_LOAD && !hasStore && !lins->mVolatile))
						{
#if 1
							if (CanMoveInstructionBeforeBlock(i) && !IsInsModifiedInRange(i + 1, mInstructions.Size(), lins) && !tail->IsInsModified(lins) && !lins->UsesTemp(lins->mDst.mTemp))
							{
								int j = 1;
								while (j < body.Size() && !body[j]->IsInsModified(lins))
									j++;
								if (j == body.Size())
								{
									mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
									mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
									mEntryRequiredTemps += lins->mDst.mTemp;
									mInstructions.Remove(i);
									i--;

									modified = true;
								}
							}
#endif
						}
						else if (lins->mCode == IC_LOAD && !lins->mVolatile && lins->mSrc[0].mTemp < 0)
						{
							if (CanMoveInstructionBeforeBlock(i))
							{
								int j = tail->mInstructions.Size() - 1;
								while (j >= 0 && !IsMatchingStore(lins, tail->mInstructions[j]))
									j--;

								if (j >= 0 && !tail->mExitRequiredTemps[lins->mDst.mTemp])
								{
									InterInstruction* sins = tail->mInstructions[j];

									if (tail->CanMoveInstructionBehindBlock(j))
									{
										if (!CollidingMem(this, lins, i + 1, mInstructions.Size()) &&
											!CollidingMem(tail, lins, 0, j))
										{
#if 1
											int k = 1;
											while (k + 1 < body.Size() && !CollidingMem(body[k], lins, 0, body[k]->mInstructions.Size()))
												k++;

											if (k + 1 == body.Size())
											{
												if (sins->mSrc[0].mTemp >= 0)
													tail->mExitRequiredTemps += sins->mSrc[0].mTemp;

												post->mInstructions.Insert(0, sins);
												tail->mInstructions.Remove(j);

												if (sins->mSrc[0].mTemp != lins->mDst.mTemp)
												{
													InterInstruction* mins;

													if (sins->mSrc[0].mTemp >= 0)
													{
														mins = new InterInstruction(sins->mLocation, IC_LOAD_TEMPORARY);
														mins->mSrc[0] = sins->mSrc[0];
													}
													else
													{
														mins = new InterInstruction(sins->mLocation, IC_CONSTANT);
														mins->mConst = sins->mSrc[0];
													}

													mins->mDst = lins->mDst;
													tail->mExitRequiredTemps += mins->mDst.mTemp;
													tail->mInstructions.Insert(tail->mInstructions.Size() - 1, mins);
												}

												mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
												mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
												mEntryRequiredTemps += lins->mDst.mTemp;
												mInstructions.Remove(i);
												i--;

												modified = true;
											}
#endif
										}
									}
								}
								else
								{
									int k = 0;
									while (k < body.Size() && !DestroyingMem(body[k], lins, 0, body[k]->mInstructions.Size()))
										k++;
									if (k == body.Size())
									{
#if 1
										if (!IsInsModifiedInRange(i + 1, mInstructions.Size(), lins) && !tail->IsInsModified(lins))
										{
											int j = 1;
											while (j < body.Size() && !body[j]->IsInsModified(lins))
												j++;
											if (j == body.Size())
											{
												mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, lins);
												mLoopPrefix->mExitRequiredTemps += lins->mDst.mTemp;
												mEntryRequiredTemps += lins->mDst.mTemp;
												mInstructions.Remove(i);
												i--;

												modified = true;
											}
										}
#endif
									}
								}
							}
						}

						i++;
					}
#if 1
					if (modified)
					{
						for (int j = 0; j < indexScale.Size(); j++)
						{
							if (indexScale[j] != 0 && !post->mEntryRequiredTemps[j])
							{
								int k = 0;
								int tz = tail->mInstructions.Size();
								while (k < tz && tail->mInstructions[k]->mDst.mTemp != j)
									k++;

								if (k < tz && !tail->IsTempReferencedInRange(0, k, j) && !tail->IsTempReferencedInRange(k + 1, tz, j))
								{
									int bi = 0;
									while (bi + 1 < body.Size() && !body[bi]->IsTempReferenced(j))
										bi++;
									if (bi + 1 == body.Size())
									{
										tail->mInstructions.Remove(k);
									}
								}
							}
						}
					}
#endif
#endif
				}
			}
		}

		if (mTrueJump && mTrueJump->SingleTailLoopOptimization(aliasedParams, staticVars))
			modified = true;
		if (mFalseJump && mFalseJump->SingleTailLoopOptimization(aliasedParams, staticVars))
			modified = true;
	}

	return modified;
}

bool InterCodeBasicBlock::EmptyLoopOptimization(void)
{
	bool	modified = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
		{
			if (mInstructions.Size() == 3)
			{
				InterInstruction* ai = mInstructions[0];
				InterInstruction* ci = mInstructions[1];
				InterInstruction* bi = mInstructions[2];

				if (ai->mCode == IC_BINARY_OPERATOR && ai->mOperator == IA_ADD && ai->mSrc[0].mTemp < 0 && ai->mDst.mTemp == ai->mSrc[1].mTemp && ai->mSrc[0].mIntConst == 1 && IsIntegerType(ai->mDst.mType) &&
					ci->mCode == IC_RELATIONAL_OPERATOR && ci->mOperator == IA_CMPLU && ci->mSrc[1].mTemp == ai->mDst.mTemp &&
					bi->mCode == IC_BRANCH && bi->mSrc[0].mTemp == ci->mDst.mTemp)
				{
					InterInstruction* mi = nullptr;

					InterInstruction* si = FindSourceInstruction(mLoopPrefix, ai->mSrc[1].mTemp);
					if (si && si->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND &&
						ci->mSrc[0].mRange.mMinState == IntegerValueRange::S_BOUND &&
						si->mDst.mRange.mMaxValue < ci->mSrc[0].mRange.mMinValue)
					{
						if (ci->mSrc[0].mTemp >= 0)
						{
							mi = new InterInstruction(ai->mLocation, IC_LOAD_TEMPORARY);
							mi->mDst = ai->mDst;
							mi->mSrc[0] = ci->mSrc[0];
							mi->mDst.mRange = mi->mSrc[0].mRange;
						}
					}

					if (mi)
					{
						mInstructions.SetSize(0);
						mInstructions.Push(mi);

						if (mFalseJump != this)
							mTrueJump = mFalseJump;
						mFalseJump = nullptr;
						mLoopHead = false;
						mNumEntries = 1;
						mEntryBlocks.SetSize(0);
						mEntryBlocks.Push(mLoopPrefix);

						InterInstruction* ins = new InterInstruction(mi->mLocation, IC_JUMP);
						mInstructions.Push(ins);

						modified = true;
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->EmptyLoopOptimization())
			modified = true;
		if (mFalseJump && mFalseJump->EmptyLoopOptimization())
			modified = true;
	}

	return modified;
}

void InterCodeBasicBlock::ConstInnerLoopOptimization(void)
{
	if (!mVisited)
	{
		if (mLoopHead && mEntryBlocks.Size() == 2)
		{
			InterCodeBasicBlock* tail;

			if (mEntryBlocks[0] == mLoopPrefix)
				tail = mEntryBlocks[1];
			else
				tail = mEntryBlocks[0];

			// Inner exit loop
			ExpandingArray<InterCodeBasicBlock*> body;

			if (CollectSingleEntryGenericLoop(body))
			{
				ExpandingArray<InterCodeBasicBlock*> exits;
				for (int i = 0; i < body.Size(); i++)
				{
					if (body[i]->mTrueJump && !body.Contains(body[i]->mTrueJump) || body[i]->mFalseJump && !(body.Contains(body[i]->mFalseJump)))
						exits.Push(body[i]);
				}

				if (exits.Size() == 1)
				{
					InterCodeBasicBlock* eblock = exits[0];
					NumberSet	cset(mEntryRequiredTemps.Size()), mset(mEntryRequiredTemps.Size());

					InterCodeBasicBlock* pblock = mLoopPrefix;
					while (pblock)
					{
						for (int i = pblock->mInstructions.Size() - 1; i >= 0; i--)
						{
							const InterInstruction* ins(pblock->mInstructions[i]);
							if (ins->mDst.mTemp >= 0)
							{
								if (ins->mCode == IC_CONSTANT && !mset[ins->mDst.mTemp])
									cset += ins->mDst.mTemp;
								mset += ins->mDst.mTemp;
							}
						}
						if (pblock->mEntryBlocks.Size() == 1)
							pblock = pblock->mEntryBlocks[0];
						else
							pblock = nullptr;
					}

					bool	isconst = true;
					for (int j = 0; isconst && j < body.Size(); j++)
					{
						InterCodeBasicBlock* block = body[j];

						for (int i = 0; isconst && i < block->mInstructions.Size(); i++)
						{
							const InterInstruction* ins(block->mInstructions[i]);

							if (ins->mCode == IC_CONSTANT || ins->mCode == IC_BRANCH ||
								ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR || ins->mCode == IC_LEA ||
								ins->mCode == IC_CONVERSION_OPERATOR)
							{
								int j = 0;
								while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || cset[ins->mSrc[j].mTemp]))
									j++;
								if (j == ins->mNumOperands)
								{
									if (ins->mDst.mTemp >= 0)
										cset += ins->mDst.mTemp;
								}
								else
									isconst = false;
							}
							else if (ins->mCode == IC_LOAD && (ins->mSrc[0].mTemp < 0 || cset[ins->mSrc[0].mTemp]) &&
								ins->mSrc[0].mMemoryBase == IM_GLOBAL && ins->mSrc[0].mLinkerObject && (ins->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
							{
								cset += ins->mDst.mTemp;
							}
							else if (ins->mCode == IC_JUMP)
								;
							else
								isconst = false;
						}
					}

					if (isconst)
					{
						ExpandingArray<InterOperand>	vars;
						vars.SetSize(cset.Size());
						mset.Clear();

						InterCodeBasicBlock* pblock = mLoopPrefix;
						while (pblock)
						{
							for (int i = pblock->mInstructions.Size() - 1; i >= 0; i--)
							{
								const InterInstruction* ins(pblock->mInstructions[i]);
								if (ins->mDst.mTemp >= 0)
								{
									if (ins->mCode == IC_CONSTANT && !mset[ins->mDst.mTemp])
									{
										assert(ins->mConst.mType != IT_NONE);
										vars[ins->mDst.mTemp] = ins->mConst;
									}
									mset += ins->mDst.mTemp;
								}
							}
							if (pblock->mEntryBlocks.Size() == 1)
								pblock = pblock->mEntryBlocks[0];
							else
								pblock = nullptr;
						}

						bool	done = false;
						int n = 0;

						mset.Clear();
						int ni = 0;
						InterCodeBasicBlock* block = this;

						while (n < 20000 && !done)
						{
							n++;
							const InterInstruction* ins(block->mInstructions[ni++]);

							switch (ins->mCode)
							{
							case IC_CONSTANT:
								assert(ins->mConst.mType != IT_NONE);
								vars[ins->mDst.mTemp] = ins->mConst;
								mset += ins->mDst.mTemp;
								break;
							case IC_BINARY_OPERATOR:
							case IC_RELATIONAL_OPERATOR:
								vars[ins->mDst.mTemp] =
									OperandConstantFolding(ins->mOperator,
										ins->mSrc[1].mTemp < 0 ? ins->mSrc[1] : vars[ins->mSrc[1].mTemp],
										ins->mSrc[0].mTemp < 0 ? ins->mSrc[0] : vars[ins->mSrc[0].mTemp]);
								mset += ins->mDst.mTemp;
								break;
							case IC_UNARY_OPERATOR:
							case IC_CONVERSION_OPERATOR:
								vars[ins->mDst.mTemp] =
									OperandConstantFolding(ins->mOperator,
										ins->mSrc[0].mTemp < 0 ? ins->mSrc[0] : vars[ins->mSrc[0].mTemp],
										ins->mSrc[0].mTemp < 0 ? ins->mSrc[0] : vars[ins->mSrc[0].mTemp]);
								mset += ins->mDst.mTemp;
								break;

							case IC_LEA:
							{
								InterOperand	op;
								int64			index = ins->mSrc[1].mIntConst;
								if (ins->mSrc[0].mTemp < 0)
									index += ins->mSrc[0].mIntConst;
								else
									index += vars[ins->mSrc[0].mTemp].mIntConst;

								if (ins->mSrc[1].mTemp < 0)
									op = ins->mSrc[1];
								else
								{
									op = vars[ins->mSrc[1].mTemp];
									index += op.mIntConst;
								}

								op.mIntConst = index;
								vars[ins->mDst.mTemp] = op;
								mset += ins->mDst.mTemp;
							}	break;
							case IC_LOAD:
							{
								InterOperand	op;
								if (ins->mSrc[0].mTemp < 0)
									op = ins->mSrc[0];
								else
								{
									op = vars[ins->mSrc[0].mTemp];
									op.mIntConst += ins->mSrc[0].mIntConst;
								}

								vars[ins->mDst.mTemp] = LoadConstantOperand(ins, op, ins->mDst.mType, mProc->mModule->mGlobalVars, mProc->mModule->mProcedures);
								mset += ins->mDst.mTemp;

							}	break;

							case IC_JUMP:
								block = block->mTrueJump;								
								ni = 0;
								break;

							case IC_BRANCH:
								block = vars[ins->mSrc[0].mTemp].mIntConst ? block->mTrueJump : block->mFalseJump;
								ni = 0;
								if (!body.Contains(block))
									done = true;
								break;
							}
						}

						if (done)
						{
							block->mEntryBlocks.RemoveAll(eblock);
							block->mEntryBlocks.Push(this);
							mTrueJump = block;
							mFalseJump = nullptr;
							mLoopHead = false;
							mNumEntries = 1;
							mEntryBlocks.SetSize(0);
							mEntryBlocks.Push(mLoopPrefix);

							InterInstruction* last = mInstructions.Last();
							mInstructions.SetSize(0);
							for (int i = 0; i < mset.Size(); i++)
							{
								if (mset[i])
								{
									InterInstruction* ins = new InterInstruction(last->mLocation, IC_CONSTANT);
									ins->mCode = IC_CONSTANT;
									ins->mDst.mTemp = i;
									ins->mConst = vars[i];
									ins->mDst.mType = ins->mConst.mType;
									mInstructions.Push(ins);
								}
							}

							InterInstruction* ins = new InterInstruction(last->mLocation, IC_JUMP);
							mInstructions.Push(ins);
						}
					}
				}
			}
		}

		mVisited = true;

		if (mTrueJump)
			mTrueJump->ConstInnerLoopOptimization();
		if (mFalseJump)
			mFalseJump->ConstInnerLoopOptimization();
	}

}

void InterCodeBasicBlock::ConstSingleLoopOptimization(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
		{
			NumberSet	cset(mEntryRequiredTemps.Size()), mset(mEntryRequiredTemps.Size());
			
			InterCodeBasicBlock* pblock = mLoopPrefix;
			while (pblock)
			{
				for (int i = pblock->mInstructions.Size() - 1; i >= 0; i--)
				{
					const InterInstruction* ins(pblock->mInstructions[i]);
					if (ins->mDst.mTemp >= 0)
					{
						if (ins->mCode == IC_CONSTANT && !mset[ins->mDst.mTemp])
							cset += ins->mDst.mTemp;
						mset += ins->mDst.mTemp;
					}
				}
				if (pblock->mEntryBlocks.Size() == 1)
					pblock = pblock->mEntryBlocks[0];
				else
					pblock = nullptr;
			}

			bool	isconst = true;
			for (int i = 0; isconst && i < mInstructions.Size(); i++)
			{
				const InterInstruction* ins(mInstructions[i]);

				if (ins->mCode == IC_CONSTANT || ins->mCode == IC_BRANCH ||
					ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR || ins->mCode == IC_LEA)
				{
					int j = 0;
					while (j < ins->mNumOperands && (ins->mSrc[j].mTemp < 0 || cset[ins->mSrc[j].mTemp]))
						j++;
					if (j == ins->mNumOperands)
					{
						if (ins->mDst.mTemp >= 0)
							cset += ins->mDst.mTemp;
					}
					else
						isconst = false;
				}
				else if (ins->mCode == IC_LOAD && (ins->mSrc[0].mTemp < 0 || cset[ins->mSrc[0].mTemp]) &&
					ins->mSrc[0].mMemoryBase == IM_GLOBAL && ins->mSrc[0].mLinkerObject && (ins->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
				{
					cset += ins->mDst.mTemp;
				}
				else
					isconst = false;
			}

			if (isconst)
			{
				ExpandingArray<InterOperand>	vars;
				vars.SetSize(cset.Size());
				mset.Clear();

				InterCodeBasicBlock* pblock = mLoopPrefix;
				while (pblock)
				{
					for (int i = pblock->mInstructions.Size() - 1; i >= 0; i--)
					{
						const InterInstruction* ins(pblock->mInstructions[i]);
						if (ins->mDst.mTemp >= 0)
						{
							if (ins->mCode == IC_CONSTANT && !mset[ins->mDst.mTemp])
							{
								assert(ins->mConst.mType != IT_NONE);
								vars[ins->mDst.mTemp] = ins->mConst;
							}
							mset += ins->mDst.mTemp;
						}
					}
					if (pblock->mEntryBlocks.Size() == 1)
						pblock = pblock->mEntryBlocks[0];
					else
						pblock = nullptr;
				}

				bool	done = false;
				int n = 0;

				mset.Clear();
				while (n < 20000 && !done)
				{
					for (int i = 0; i < mInstructions.Size(); i++)
					{
						n++;

						const InterInstruction* ins(mInstructions[i]);
						switch (ins->mCode)
						{
						case IC_CONSTANT:
							assert(ins->mConst.mType != IT_NONE);
							vars[ins->mDst.mTemp] = ins->mConst;
							mset += ins->mDst.mTemp;
							break;
						case IC_BINARY_OPERATOR:
						case IC_UNARY_OPERATOR:
						case IC_RELATIONAL_OPERATOR:
							vars[ins->mDst.mTemp] =
								OperandConstantFolding(ins->mOperator,
									ins->mSrc[1].mTemp < 0 ? ins->mSrc[1] : vars[ins->mSrc[1].mTemp],
									ins->mSrc[0].mTemp < 0 ? ins->mSrc[0] : vars[ins->mSrc[0].mTemp]);
							mset += ins->mDst.mTemp;
							break;
						case IC_LEA:
						{
							InterOperand	op;
							int64			index = ins->mSrc[1].mIntConst;
							if (ins->mSrc[0].mTemp < 0)
								index += ins->mSrc[0].mIntConst;
							else
								index += vars[ins->mSrc[0].mTemp].mIntConst;

							if (ins->mSrc[1].mTemp < 0)
								op = ins->mSrc[1];
							else
							{
								op = vars[ins->mSrc[1].mTemp];
								index += op.mIntConst;
							}

							op.mIntConst = index;
							vars[ins->mDst.mTemp] = op;
							mset += ins->mDst.mTemp;
						}	break;
						case IC_LOAD:
						{
							InterOperand	op;
							if (ins->mSrc[0].mTemp < 0)
								op = ins->mSrc[0];
							else
							{
								op = vars[ins->mSrc[0].mTemp];
								op.mIntConst += ins->mSrc[0].mIntConst;
							}

							vars[ins->mDst.mTemp] = LoadConstantOperand(ins, op, ins->mDst.mType, mProc->mModule->mGlobalVars, mProc->mModule->mProcedures);
							mset += ins->mDst.mTemp;

						}	break;

						case IC_BRANCH:
							if ((vars[ins->mSrc[0].mTemp].mIntConst ? mTrueJump : mFalseJump) != this)
								done = true;
							break;
						}
					}
				}

				if (done)
				{
					InterInstruction* last = mInstructions.Last();
					mInstructions.SetSize(0);
					if (mFalseJump != this)
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					mLoopHead = false;
					mNumEntries = 1;
					mEntryBlocks.SetSize(0);
					mEntryBlocks.Push(mLoopPrefix);

					for (int i = 0; i < mset.Size(); i++)
					{
						if (mset[i])
						{
							InterInstruction* ins = new InterInstruction(last->mLocation, IC_CONSTANT);
							ins->mCode = IC_CONSTANT;
							ins->mDst.mTemp = i;
							ins->mConst = vars[i];
							ins->mDst.mType = ins->mConst.mType;
							mInstructions.Push(ins);
						}
					}

					InterInstruction* ins = new InterInstruction(last->mLocation, IC_JUMP);
					mInstructions.Push(ins);
				}
			}
		}

		if (mTrueJump)
			mTrueJump->ConstSingleLoopOptimization();
		if (mFalseJump)
			mFalseJump->ConstSingleLoopOptimization();
	}
}

void InterCodeBasicBlock::EliminateDoubleLoopCounter(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2 && mLoopPrefix->mEntryBlocks.Size() == 1)
		{
			ExpandingArray<InterCodeBasicBlock*> body, path;
			body.Push(this);
			bool	innerLoop = true;

			for (int i = 0; i < mEntryBlocks.Size(); i++)
			{
				if (mEntryBlocks[i] != mLoopPrefix)
				{
					if (!mEntryBlocks[i]->CollectLoopBodyRecursive(this, body))
						innerLoop = false;
				}
			}

			if (innerLoop)
			{
				InterCodeBasicBlock* eblock;
				if (mEntryBlocks[0] == mLoopPrefix)
					eblock = mEntryBlocks[1];
				else
					eblock = mEntryBlocks[0];

				struct LoopCounter
				{
					InterInstruction	*	mInit, * mInc, * mCmp;
					int64					mStart, mEnd, mStep;
					bool					mReferenced;
				};

				ExpandingArray<LoopCounter>	lcs;

				for (int i = 0; i < eblock->mInstructions.Size(); i++)
				{
					InterInstruction* ins(eblock->mInstructions[i]);

					LoopCounter	lc;
					lc.mInc = nullptr;
					lc.mInit = nullptr;
					lc.mCmp = nullptr;
					lc.mReferenced = false;

					if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType))
					{
						if (ins->mDst.mTemp == ins->mSrc[0].mTemp && ins->mSrc[1].mTemp < 0 ||
							ins->mDst.mTemp == ins->mSrc[1].mTemp && ins->mSrc[0].mTemp < 0)
						{
							lc.mInc = ins;
						}
					}
#if 1
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SUB && IsIntegerType(ins->mDst.mType))
					{
						if (ins->mDst.mTemp == ins->mSrc[1].mTemp && ins->mSrc[0].mTemp < 0)
						{
							lc.mInc = ins;
						}
					}
#endif
					else if (ins->mCode == IC_LEA && ins->mDst.mTemp == ins->mSrc[1].mTemp && ins->mSrc[0].mTemp < 0)
					{
						lc.mInc = ins;
					}

					if (lc.mInc)
					{
						int temp = lc.mInc->mDst.mTemp;

						if (!eblock->IsTempModifiedInRange(0, i, temp))
						{
							int sz = eblock->mInstructions.Size();
							int rz = sz - 1;
							if (eblock->mInstructions[sz - 1]->mCode == IC_BRANCH &&
								eblock->mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR && eblock->mInstructions[sz - 1]->mSrc[0].mTemp == eblock->mInstructions[sz - 2]->mDst.mTemp &&
								((eblock->mInstructions[sz - 2]->mSrc[0].mTemp == temp && eblock->mInstructions[sz - 2]->mSrc[1].mTemp < 0) ||
								 (eblock->mInstructions[sz - 2]->mSrc[1].mTemp == temp && eblock->mInstructions[sz - 2]->mSrc[0].mTemp < 0)))
							{
								InterInstruction* ci = eblock->mInstructions[sz - 2];

								if (ci->mOperator == IA_CMPEQ && eblock->mFalseJump == this ||
									ci->mOperator == IA_CMPNE && eblock->mTrueJump == this ||
									ci->mOperator == IA_CMPGU && eblock->mTrueJump == this && ci->mSrc[0].mTemp < 0 && ci->mSrc[0].mIntConst == 0 ||
									ci->mOperator == IA_CMPLU && eblock->mTrueJump == this && ci->mSrc[0].mTemp < 0 ||
									ci->mOperator == IA_CMPLEU && eblock->mTrueJump == this && ci->mSrc[0].mTemp < 0)
								{
									if (ci->mSrc[0].mTemp < 0)
										lc.mEnd = ci->mSrc[0].mIntConst;
									else
										lc.mEnd = ci->mSrc[1].mIntConst;
									lc.mCmp = eblock->mInstructions[sz - 2];
									rz--;
								}
							}

							if (!eblock->IsTempModifiedInRange(i + 1, sz, temp))
							{
								if (eblock->IsTempReferencedInRange(0, i, temp) || eblock->IsTempReferencedInRange(i + 1, rz, temp))
									lc.mReferenced = true;

								for (int k = 0; k < body.Size(); k++)
								{
									if (body[k] != eblock && body[k]->IsTempReferenced(temp))
										lc.mReferenced = true;
								}

								int k = 0;
								while (k < body.Size() && (body[k] == eblock || !body[k]->IsTempModified(lc.mInc->mDst.mTemp)))
									k++;

								if (k == body.Size())
								{
									lc.mInit = mLoopPrefix->mEntryBlocks[0]->FindTempOrigin(lc.mInc->mDst.mTemp);
									if (lc.mInit && lc.mInit->mCode == IC_CONSTANT)
									{
										lc.mStart = lc.mInit->mConst.mIntConst;
										if (lc.mInc->mSrc[0].mTemp < 0)
										{
											if (lc.mInc->mOperator == IA_SUB)
												lc.mStep = -lc.mInc->mSrc[0].mIntConst;
											else
												lc.mStep = lc.mInc->mSrc[0].mIntConst;
										}
										else
											lc.mStep = lc.mInc->mSrc[1].mIntConst;
										lcs.Push(lc);
									}
								}
							}
						}
					}
				}

//				printf("EDLC %d, %d\n", mIndex, lcs.Size());


				if (lcs.Size() >= 2)
				{
					int i = 0;
					while (i < lcs.Size())
					{
						if (lcs[i].mReferenced)
						{
							int j = i + 1;
							while (j < lcs.Size() && !(lcs[j].mReferenced && 
								lcs[i].mStart == lcs[j].mStart && 
								lcs[i].mStep == lcs[j].mStep && 
								lcs[i].mInc->mCode == lcs[j].mInc->mCode &&
								(lcs[i].mInc->mCode != IC_LEA || SameMem(lcs[i].mInc->mDst, lcs[j].mInc->mDst))))
								j++;
							if (j < lcs.Size())
							{
//								printf("Found Same %s : %d-%d\n", mProc->mIdent->mString, i, j);
								int ki = mInstructions.IndexOf(lcs[i].mInc);
								int kj = mInstructions.IndexOf(lcs[j].mInc);
								int tmpi = lcs[i].mInc->mDst.mTemp;
								int tmpj = lcs[j].mInc->mDst.mTemp;

								if (ki < kj && !IsTempReferencedInRange(ki + 1, kj, tmpi))
								{
									lcs[i].mInc->mCode = IC_NONE; lcs[i].mInc->mNumOperands = 0; lcs[i].mInc->mDst.mTemp = -1;
								}
								else if (kj < ki && !IsTempReferencedInRange(kj + 1, ki, tmpj))
								{
									lcs[j].mInc->mCode = IC_NONE; lcs[j].mInc->mNumOperands = 0; lcs[j].mInc->mDst.mTemp = -1;
								}
								else
								{
									i++;
									continue;
								}

								if (lcs[i].mCmp)
								{
									for (int k = 0; k < mInstructions.Size(); k++)
										mInstructions[k]->ReplaceTemp(tmpj, tmpi);
									lcs[j].mReferenced = false;
								}
								else
								{
									for (int k = 0; k < mInstructions.Size(); k++)
										mInstructions[k]->ReplaceTemp(tmpi, tmpj);
									lcs[i].mReferenced = false;
								}
							}
							else
								i++;
						}
						else
							i++;
					}
				}
				if (lcs.Size() >= 2)
				{
					int64	loop = -1;
					int k = 0;
					while (k < lcs.Size() && !lcs[k].mCmp)
						k++;
					if (k < lcs.Size())
					{
						int64	start = lcs[k].mStart;
						int64	end = lcs[k].mEnd;
						int64	step = lcs[k].mStep;

						if (lcs[k].mCmp->mOperator == IA_CMPLEU)
							end += step;

						if (step > 0 && end > start || step < 0 && end < start)
							loop = (end - start) / step;
					}

#if 0
					for (int i = 0; i < lcs.Size(); i++)
					{
						printf("LCS[%d] %lld + %lld -> %lld {%d, %d, %d}\n", i, lcs[i].mStart, lcs[i].mStep, lcs[i].mEnd, lcs[i].mInc->mDst.mTemp, lcs[i].mInc->mCode, lcs[i].mReferenced);
					}
#endif

					if (loop > 0)
					{
						if (!lcs[k].mReferenced)
						{
							int j = 0;
							while (j < lcs.Size() && !(lcs[j].mReferenced && lcs[j].mInc->mCode == IC_BINARY_OPERATOR && lcs[j].mStart + lcs[j].mStep * loop < 65536))
								j++;

							// Pointer compare with constants only in native code path
							if (j == lcs.Size() && mProc->mNativeProcedure)
							{
								j = 0;
								while (j < lcs.Size() && !(lcs[j].mReferenced && lcs[j].mInc->mCode == IC_LEA && (lcs[j].mInit->mConst.mMemory == IM_GLOBAL || lcs[j].mInit->mConst.mMemory == IM_ABSOLUTE) && 
									lcs[j].mStart + lcs[j].mStep * loop <= 65536))
									j++;
							}

//							printf("Found %d, %d\n", j, k);

							if (j < lcs.Size())
							{
								if (lcs[j].mInc->mCode == IC_LEA)
								{
									lcs[j].mInc->mDst.mRange.LimitMax(lcs[j].mStart + lcs[j].mStep * loop);
									lcs[j].mInc->mSrc[1].mRange.LimitMax(lcs[j].mStart + lcs[j].mStep * (loop - 1));
								}

								int ci = 0, ti = 1;
								if (lcs[k].mCmp->mSrc[1].mTemp < 0)
								{
									ci = 1;
									ti = 0;
								}

								lcs[k].mCmp->mSrc[ti] = lcs[j].mInc->mDst;
								lcs[k].mCmp->mSrc[ci] = lcs[j].mInit->mConst;
								lcs[k].mCmp->mSrc[ci].mIntConst += loop * lcs[j].mStep;
								if (lcs[k].mCmp->mOperator == IA_CMPGU || lcs[k].mCmp->mOperator == IA_CMPLEU)
									lcs[k].mCmp->mOperator = IA_CMPNE;

								InterInstruction* iins = lcs[k].mInit->Clone();
								iins->mConst.mIntConst += loop * lcs[k].mStep;

								mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, iins);							

								lcs[k].mInc->mCode = IC_NONE;
								lcs[k].mInc->mDst.mTemp = -1;
								lcs[k].mInc->mNumOperands = 0;
							}
						}
					}
				}
			}
		}
		
		if (mTrueJump)
			mTrueJump->EliminateDoubleLoopCounter();
		if (mFalseJump)
			mFalseJump->EliminateDoubleLoopCounter();
	}
}
void InterCodeBasicBlock::InnerLoopOptimization(const NumberSet& aliasedParams)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead)
		{
			ExpandingArray<InterCodeBasicBlock*> body, path;
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
				bool	hasCall = false, hasFrame = false, hasStore = false;
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
						else if (ins->mCode == IC_STORE)
						{
							if (ins->mSrc[1].mTemp >= 0)
								hasStore = true;
							else if ((ins->mSrc[1].mMemory == IM_PARAM || ins->mSrc[1].mMemory == IM_FPARAM) && !aliasedParams[ins->mSrc[1].mVarIndex])
							;
							else
								hasStore = true;
						}
						else if (ins->mCode == IC_COPY || ins->mCode == IC_STRCPY || ins->mCode == IC_FILL)
							hasStore = true;
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
							if (ins->mVolatile)
							{
								ins->mInvariant = false;
							}
							else if (ins->mSrc[0].mTemp >= 0 && (hasStore || hasCall))
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
							else if (ins->mSrc[0].mMemory == IM_PARAM && hasCall && aliasedParams[ins->mSrc[0].mVarIndex])
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
										else if (sins->mCode == IC_COPY || sins->mCode == IC_FILL)
										{
											ins->mInvariant = false;
										}
									}
								}
							}
						}
					}
				}

				for (int bi = 0; bi < body.Size(); bi++)
				{
					InterCodeBasicBlock* block = body[bi];

					for (int i = 0; i < block->mInstructions.Size(); i++)
					{
						InterInstruction* ins = block->mInstructions[i];

						if (ins->mCode == IC_LEA && ins->mSrc[1].mMemory != IM_FRAME || ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_BINARY_OPERATOR || ins->mCode == IC_RELATIONAL_OPERATOR)
						{
							if (!block->mEntryRequiredTemps[ins->mDst.mTemp] && !block->mExitRequiredTemps[ins->mDst.mTemp])
							{
								ins->mInvariant = true;
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
						InterInstruction* nins = nullptr;
						if (i + 1 < block->mInstructions.Size())
							nins = block->mInstructions[i + 1];

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
							{
								int offset = 0;
								if (ins->mSrc[0].mTemp < 0)
									offset = int(ins->mSrc[0].mIntConst);

								if (ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0)
									ins->mExpensive = true;
								else if (ins->mSrc[0].mTemp >= 0 && ins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND && ins->mSrc[0].mRange.mMaxValue >= 256)
									ins->mExpensive = true;
								else if (nins && nins->mCode == IC_LEA && nins->mSrc[0].mTemp >= 0 && (nins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_UNBOUND || nins->mSrc[0].mRange.mMaxValue + offset >= 255))
									ins->mExpensive = true;
							}	break;
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

					for (int bi = 0; bi < body.Size(); bi++)
					{
						InterCodeBasicBlock* block = body[bi];

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

					for (int bi = 0; bi < body.Size(); bi++)
					{
						InterCodeBasicBlock* block = body[bi];

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


				for (int bi = 0; bi < body.Size(); bi++)
				{
					InterCodeBasicBlock* block = body[bi];

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

int InterCodeBasicBlock::NumUnrollInstructions(int ireg)
{
	FastNumberSet	ffree(mLocalUsedTemps.Size());
	ffree += ireg;

	int	nins = 0;
	for (int i = 0; i + 3 < mInstructions.Size(); i++)
	{
		const InterInstruction* ins(mInstructions[i]);
		switch (ins->mCode)
		{
		case IC_BINARY_OPERATOR:
		case IC_RELATIONAL_OPERATOR:
		case IC_LEA:
			if (ins->mSrc[0].mTemp >= 0 && ffree[ins->mSrc[0].mTemp] && ins->mSrc[1].mTemp < 0 || 
				ins->mSrc[1].mTemp >= 0 && ffree[ins->mSrc[1].mTemp] && ins->mSrc[0].mTemp < 0)
				ffree += ins->mDst.mTemp;
			else
				nins++;
			break;
		case IC_UNARY_OPERATOR:
			if (ffree[ins->mSrc[0].mTemp])
				ffree += ins->mDst.mTemp;
			else
				nins++;
			break;
		case IC_CONSTANT:
			ffree += ins->mDst.mTemp;
			break;
		case IC_CONVERSION_OPERATOR:
			if (ffree[ins->mSrc[0].mTemp])
				ffree += ins->mDst.mTemp;
			else if (ins->mOperator == IA_EXT8TO16U || ins->mOperator == IA_EXT8TO32U || ins->mOperator == IA_EXT16TO32U || ins->mOperator == IA_EXT8TO16S)
				;
			else
				nins++;
			break;
		default:
			nins++;
		}
	}

	return nins;
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
					mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && 
						(mInstructions[nins - 2]->mOperator == IA_CMPLS || mInstructions[nins - 2]->mOperator == IA_CMPLES || 
						 mInstructions[nins - 2]->mOperator == IA_CMPGS || mInstructions[nins - 2]->mOperator == IA_CMPGES ||
						 mInstructions[nins - 2]->mOperator == IA_CMPLU || mInstructions[nins - 2]->mOperator == IA_CMPLEU  || 
						 mInstructions[nins - 2]->mOperator == IA_CMPGU || mInstructions[nins - 2]->mOperator == IA_CMPGEU ||
						 mInstructions[nins - 2]->mOperator == IA_CMPNE) &&
						mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
					mInstructions[nins - 2]->mSrc[0].mTemp < 0 &&
					mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && (mInstructions[nins - 3]->mOperator == IA_ADD || mInstructions[nins - 3]->mOperator == IA_SUB) && mInstructions[nins - 3]->mDst.mTemp == mInstructions[nins - 2]->mSrc[1].mTemp)
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
								int64	start = mDominator->mTrueValueRange[ireg].mMinValue;
								int64	end = mInstructions[nins - 2]->mSrc[0].mIntConst;
								if (mInstructions[nins - 2]->mOperator == IA_CMPLEU || mInstructions[nins - 2]->mOperator == IA_CMPLES)
									end++;
								else if (mInstructions[nins - 2]->mOperator == IA_CMPGEU || mInstructions[nins - 2]->mOperator == IA_CMPGES)
									end--;

								int64	step = mInstructions[nins - 3]->mSrc[0].mTemp < 0 ? mInstructions[nins - 3]->mSrc[0].mIntConst : mInstructions[nins - 3]->mSrc[1].mIntConst;
								if (mInstructions[nins - 3]->mOperator == IA_SUB)
									step = -step;

								int	count = step > 0 ? int((end - start + step - 1) / step) : int((start - end - step - 1) / -step);

//								if (CheckFunc && mIndex == 10) printf("Unroll %lld %lld %lld %d\n", start, end, step, count);

								if (count > 0 && 
									!( mInstructions[nins - 2]->mOperator == IA_CMPNE && end != start + count * step) &&
									!((mInstructions[nins - 2]->mOperator == IA_CMPGU || mInstructions[nins - 2]->mOperator == IA_CMPGEU) && (end < 0 || start < 0)))
								{
									int cins = NumUnrollInstructions(ireg);

									if (count < 5 && (cins - 3) * count < 20)
									{
										//										printf("Unrolling %s,%d\n", mProc->mIdent->mString, mIndex);

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

										InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
										mInstructions.Push(jins);
									}
								}
							}
						}
					}
				}					
				else if (
					mInstructions[nins - 1]->mCode == IC_BRANCH &&
					mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[nins - 2]->mOperator == IA_CMPNE && mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
					mInstructions[nins - 3]->mCode == IC_LEA &&
					mInstructions[nins - 3]->mDst.mTemp == mInstructions[nins - 2]->mSrc[1].mTemp &&
					mInstructions[nins - 3]->mDst.mTemp == mInstructions[nins - 3]->mSrc[1].mTemp &&
					mInstructions[nins - 3]->mSrc[0].mTemp < 0)
				{
					InterCodeBasicBlock* prefix = this == mEntryBlocks[0] ? mEntryBlocks[1] : mEntryBlocks[0];

					int64	step = mInstructions[nins - 3]->mSrc[0].mIntConst;

					if (step > 0)
					{
						InterInstruction* i1 = FindSourceInstruction(prefix, mInstructions[nins - 2]->mSrc[0].mTemp);
						InterInstruction* i2 = FindSourceInstruction(prefix, mInstructions[nins - 2]->mSrc[1].mTemp);

						if (i1 && i2 && i1->mCode == IC_CONSTANT && i2->mCode == IC_CONSTANT && i1->mConst.mMemory == i2->mConst.mMemory)
						{
							if (i1->mConst.mMemory == IM_GLOBAL && i1->mConst.mLinkerObject == i2->mConst.mLinkerObject)
							{
								int64 count = (i1->mConst.mIntConst - i2->mConst.mIntConst) / step;
								if (count * step + i2->mConst.mIntConst == i1->mConst.mIntConst)
								{
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

										InterInstruction* jins = new InterInstruction(mInstructions[0]->mLocation, IC_JUMP);
										mInstructions.Push(jins);
									}
								}
							}
						}
					}

				}

			}
		}

#if 1
		if (mLoopHead && mNumEntries == 2 && mTrueJump == this && mFalseJump && mFalseJump->mNumEntries == 1)
		{
			int	nins = mInstructions.Size();

			if (nins > 3)
			{
				if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
					mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && 
					(mInstructions[nins - 2]->mOperator == IA_CMPLU || mInstructions[nins - 2]->mOperator == IA_CMPLEU || mInstructions[nins - 2]->mOperator == IA_CMPLS || mInstructions[nins - 2]->mOperator == IA_CMPLES || mInstructions[nins - 2]->mOperator == IA_CMPNE) &&
					mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
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
								int64	start = mDominator->mTrueValueRange[ireg].mMinValue;
								int64	end = mInstructions[nins - 2]->mSrc[0].mIntConst;
								if (mInstructions[nins - 2]->mOperator == IA_CMPLEU || mInstructions[nins - 2]->mOperator == IA_CMPLES)
									end++;

								int64	step = mInstructions[nins - 3]->mSrc[0].mTemp < 0 ? mInstructions[nins - 3]->mSrc[0].mIntConst : mInstructions[nins - 3]->mSrc[1].mIntConst;
								int	count = int((end - start + step - 1) / step);

								if (mInstructions[nins - 2]->mOperator != IA_CMPNE || end == start + count * step)
								{
									int i = 0;
									while (i < mInstructions.Size())
									{
										InterInstruction* ins = mInstructions[i];

										if (ins->mCode == IC_BINARY_OPERATOR &&
											(ins->mOperator == IA_ADD || ins->mOperator == IA_SUB) &&
											(ins->mDst.mTemp == ins->mSrc[0].mTemp && ins->mSrc[1].mTemp < 0 || ins->mDst.mTemp == ins->mSrc[1].mTemp && ins->mSrc[0].mTemp < 0) &&
											!IsTempReferencedInRange(0, i, ins->mDst.mTemp) && !IsTempReferencedInRange(i + 1, mInstructions.Size(), ins->mDst.mTemp))
										{
//											printf("Extract %s : %s,%d (%d) : %d, %d\n", mProc->mIdent->mString, ins->mLocation.mFileName, ins->mLocation.mLine, mIndex, ins->mSrc[0].mTemp, ins->mSrc[1].mTemp);

											if (mTrueValueRange.Size())
											{
												mTrueValueRange[ins->mDst.mTemp].Reset();
												mFalseValueRange[ins->mDst.mTemp].Reset();
												mFalseJump->mEntryValueRange[ins->mDst.mTemp].Reset();
											}

											if (ins->mSrc[0].mTemp < 0)
												ins->mSrc[0].mIntConst *= count;
											else
												ins->mSrc[1].mIntConst *= count;
											mFalseJump->mInstructions.Insert(0, ins);
											mInstructions.Remove(i);
										}
										else
											i++;
									}
								}
							}
						}
					}
				}
			}
		}
#endif

		if (mTrueJump)
			mTrueJump->SingleBlockLoopUnrolling();
		if (mFalseJump)
			mFalseJump->SingleBlockLoopUnrolling();
	}
}

static int FindStore(InterCodeBasicBlock* block, int pos, const InterOperand& op)
{
	while (pos > 0)
	{
		pos--;
		InterInstruction* ins(block->mInstructions[pos]);
		if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp < 0)
		{
			if ((op.mMemory == IM_PARAM && ins->mSrc[1].mMemory == IM_FRAME ||
				op.mMemory == IM_FPARAM && ins->mSrc[1].mMemory == IM_FFRAME) &&
				op.mVarIndex == ins->mSrc[1].mVarIndex)
				return pos;
		}
		if (ins->mCode == IC_POP_FRAME && op.mMemory == IM_PARAM)
			return -1;
	}

	return -1;
}

bool InterCodeBasicBlock::StructReturnPropagation(void)
{
	if (mInstructions.Size() >= 6)
	{
		int sz = mInstructions.Size();
		if (mInstructions[sz - 1]->mCode == IC_RETURN &&
			mInstructions[sz - 2]->mCode == IC_COPY && mInstructions[sz - 2]->mSrc[0].mTemp < 0 && mInstructions[sz - 2]->mSrc[1].mTemp == mInstructions[sz - 3]->mDst.mTemp &&
			mInstructions[sz - 2]->mSrc[0].mMemory == IM_GLOBAL && mInstructions[sz - 2]->mSrc[0].mLinkerObject && (mInstructions[sz - 2]->mSrc[0].mLinkerObject->mFlags & LOBJF_LOCAL_VAR) &&
			mInstructions[sz - 3]->mCode == IC_LOAD && mInstructions[sz - 3]->mSrc[0].mTemp < 0 && (mInstructions[sz - 3]->mSrc[0].mMemory == IM_FPARAM || mInstructions[sz - 3]->mSrc[0].mMemory == IM_PARAM))
		{
			LinkerObject* lo = mInstructions[sz - 2]->mSrc[0].mLinkerObject;

			int match = -1;
			for (int i = 0; i < sz - 3; i++)
			{
				InterInstruction* ins = mInstructions[i];
				if (ins->mCode == IC_STORE || ins->mCode == IC_COPY || ins->mCode == IC_STRCPY || ins->mCode == IC_FILL || ins->mCode == IC_LEA)
				{
					if (ins->mSrc[1].mLinkerObject == lo)
						return false;
				}
				else if (ins->mCode == IC_LOAD || ins->mCode == IC_COPY || ins->mCode == IC_STRCPY)
				{
					if (ins->mSrc[0].mLinkerObject == lo)
						return false;
				}
				else if (ins->mCode == IC_CONSTANT && ins->mDst.mType == IT_POINTER && ins->mConst.mLinkerObject == lo)
				{
					if (match == -1)
						match = i;
					else
						return false;
				}
			}

			if (match >= 0)
			{
				if (mInstructions[match + 1]->mCode == IC_STORE && mInstructions[match + 1]->mSrc[0].mTemp == mInstructions[match + 0]->mDst.mTemp && mInstructions[match + 1]->mSrc[0].mFinal)
				{
					mInstructions[match]->mCode = IC_LOAD;
					mInstructions[match]->mSrc[0] = mInstructions[sz - 3]->mSrc[0];
					mInstructions[match]->mNumOperands = 1;

					mInstructions[sz - 3]->mCode = IC_NONE; mInstructions[sz - 3]->mNumOperands = 0; mInstructions[sz - 3]->mDst.mTemp = -1;
					mInstructions[sz - 2]->mCode = IC_NONE; mInstructions[sz - 2]->mNumOperands = 0; mInstructions[sz - 2]->mDst.mTemp = -1;

					return true;
				}
			}
			else
				return false;
		}
	}

	return false;
}

bool InterCodeBasicBlock::CollapseDispatch()
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i + 2 < mInstructions.Size(); i++)
		{
			if (mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_LOAD && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mStride == 2 &&
				mInstructions[i + 2]->mCode == IC_DISPATCH)
			{
				LinkerObject* lo = mInstructions[i + 0]->mSrc[1].mLinkerObject;
				if (lo && lo->mReferences.Size() > 0)
				{
					int j = 1;
					while (2 * j < lo->mReferences.Size() && lo->mReferences[0]->mRefObject == lo->mReferences[2 * j]->mRefObject)
						j++;
					if (2 * j == lo->mReferences.Size())
					{
						mInstructions[i + 1]->mCode = IC_CONSTANT;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mConst.mType = IT_POINTER;
						mInstructions[i + 1]->mConst.mMemory = IM_GLOBAL;
						mInstructions[i + 1]->mConst.mLinkerObject = lo->mReferences[0]->mRefObject;
						changed = true;
						printf("dispatch");
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->CollapseDispatch())
			changed = true;
		if (mFalseJump && mFalseJump->CollapseDispatch())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::CheapInlining(int & numTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mCode == IC_CALL_NATIVE && ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mLinkerObject && ins->mSrc[0].mLinkerObject->mProc && ins->mSrc[0].mLinkerObject->mProc->mCheapInline)
			{
				InterCodeProcedure* proc = ins->mSrc[0].mLinkerObject->mProc;
				InterCodeBasicBlock* block = proc->mEntryBlock;

				int	ntemps = numTemps;
				GrowingArray<int>	tmap(-1);

				bool	fail = false;
				for (int j = 0; j < block->mInstructions.Size(); j++)
				{
					InterInstruction* nins(block->mInstructions[j]);
					if (nins->mCode == IC_LOAD && nins->mSrc[0].mTemp < 0 && FindStore(this, i, nins->mSrc[0]) < 0)
						fail = true;
				}

				if (!fail)
				{
//					printf("Cheap Inline %s into %s\n", proc->mIdent->mString, mProc->mIdent->mString);

					for (int j = 0; j < i; j++)
						mInstructions[j]->mRemove = false;

					mInstructions.Remove(i);
					changed = true;

					if (proc->mCommonFrameSize)
					{
						InterInstruction* fins = new InterInstruction(ins->mLocation, IC_PUSH_FRAME);
						fins->mNumOperands = 0;
						fins->mConst.mIntConst = proc->mCommonFrameSize;
						mInstructions.Insert(i, fins);
						i++;
					}

					for (int j = 0; j < block->mInstructions.Size(); j++)
					{
						InterInstruction* nins(block->mInstructions[j]);
						switch (nins->mCode)
						{
						case IC_LOAD:
						{
							int k = FindStore(this, i, nins->mSrc[0]);
							if (k >= 0)
							{
								InterInstruction* pins = mInstructions[k]->Clone();
								pins->mLocation.mFrom = &(ins->mLocation);
								mInstructions[k]->mRemove = true;

								if (pins->mSrc[0].mTemp < 0)
								{
									pins->mCode = IC_CONSTANT;
									pins->mConst = pins->mSrc[0];
									pins->mNumOperands = 0;
								}
								else
								{
									pins->mCode = IC_LOAD_TEMPORARY;
									pins->mNumOperands = 1;
								}

								pins->mDst = nins->mDst;
								pins->mDst.mTemp = ntemps;
								mInstructions.Insert(k + 1, pins);
								i++;
							}
							else
							{
								InterInstruction* pins = nins->Clone();
								pins->mLocation.mFrom = &(ins->mLocation);
								if (pins->mSrc[0].mTemp >= 0)
									pins->mSrc[0].mTemp = tmap[pins->mSrc[0].mTemp];
								if (pins->mDst.mTemp >= 0)
									pins->mDst.mTemp = ntemps;
								mInstructions.Insert(i, pins);
								i++;
							}
						}	break;
						case IC_STORE:
						{
							InterInstruction* pins = nins->Clone();
							pins->mLocation.mFrom = &(ins->mLocation);
							if (pins->mSrc[0].mTemp >= 0)
								pins->mSrc[0].mTemp = tmap[pins->mSrc[0].mTemp];
							if (pins->mSrc[1].mTemp >= 0)
								pins->mSrc[1].mTemp = tmap[pins->mSrc[1].mTemp];
							mInstructions.Insert(i, pins);
							i++;
						}	break;
						case IC_CALL:
						case IC_CALL_NATIVE:
						{
							InterInstruction* pins = nins->Clone();
							pins->mLocation.mFrom = &(ins->mLocation);
							if (pins->mDst.mTemp >= 0)
								pins->mDst.mTemp = ntemps;
							mInstructions.Insert(i, pins);
							i++;
						} break;
						case IC_RETURN:
							break;
						case IC_RETURN_VALUE:
						{
							if (ins->mDst.mTemp >= 0)
							{
								InterInstruction* pins = nins->Clone();
								pins->mLocation.mFrom = &(ins->mLocation);
								if (pins->mSrc[0].mTemp < 0)
								{
									pins->mCode = IC_CONSTANT;
									pins->mConst = pins->mSrc[0];
									pins->mNumOperands = 0;
								}
								else
								{
									pins->mCode = IC_LOAD_TEMPORARY;
									pins->mNumOperands = 1;
								}

								pins->mDst = ins->mDst;
								if (pins->mSrc[0].mTemp >= 0)
									pins->mSrc[0].mTemp = tmap[pins->mSrc[0].mTemp];
								mInstructions.Insert(i, pins);
								i++;
							}
						} break;
						default:
						{
							InterInstruction* pins = nins->Clone();
							pins->mLocation.mFrom = &(ins->mLocation);
							for (int k = 0; k < pins->mNumOperands; k++)
								if (pins->mSrc[k].mTemp >= 0)
									pins->mSrc[k].mTemp = tmap[pins->mSrc[k].mTemp];
							if (pins->mDst.mTemp >= 0)
								pins->mDst.mTemp = ntemps;
							mInstructions.Insert(i, pins);
							i++;
						} break;
						}

						if (nins->mDst.mTemp >= 0)
							tmap[nins->mDst.mTemp] = ntemps++;
					}

					numTemps = ntemps;

					if (proc->mCommonFrameSize)
					{
						InterInstruction* fins = new InterInstruction(ins->mLocation, IC_POP_FRAME);
						fins->mNumOperands = 0;
						fins->mConst.mIntConst = proc->mCommonFrameSize;
						mInstructions.Insert(i, fins);
						i++;
					}

					for (int j = 0; j < i; j++)
					{
						if (mInstructions[j]->mRemove)
						{
							mInstructions[j]->mCode = IC_NONE;
							mInstructions[j]->mDst.mTemp = -1;
							mInstructions[j]->mNumOperands = 0;
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->CheapInlining(numTemps))
			changed = true;
		if (mFalseJump && mFalseJump->CheapInlining(numTemps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::MapLateIntrinsics(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mCode == IC_CALL_NATIVE && ins->mSrc[0].mLinkerObject && 
				ins->mSrc[0].mLinkerObject->mProc &&
				ins->mSrc[0].mLinkerObject->mProc->mIntrinsicFunction &&
				!strcmp(ins->mSrc[0].mLinkerObject->mIdent->mString, "memcpy"))
			{
				int		ops[3];
				bool	fail = false;

				for (int vi = 0; vi < 3; vi++)
				{
					InterOperand	op;
					op.mType = vi < 2 ? IT_POINTER : IT_INT16;
					op.mTemp = -1;
					op.mMemory = IM_FPARAM;
					op.mVarIndex = 2 * vi;

					if ((ops[vi] = FindStore(this, i, op)) < 0)
					{
						fail = true;
						break;
					}
				}

				if (!fail && mInstructions[ops[2]]->mSrc[0].mTemp < 0 && mInstructions[ops[2]]->mSrc[0].mIntConst < 256)
				{
					ins->mCode = IC_COPY;
					ins->mSrc[0] = mInstructions[ops[1]]->mSrc[0]; mInstructions[ops[1]]->mSrc[0].mFinal = false;
					ins->mSrc[1] = mInstructions[ops[0]]->mSrc[0]; mInstructions[ops[0]]->mSrc[0].mFinal = false;
					ins->mSrc[0].mOperandSize = ins->mSrc[1].mOperandSize = ins->mConst.mOperandSize = int(mInstructions[ops[2]]->mSrc[0].mIntConst);
					ins->mNumOperands = 2;

					mInstructions[ops[0]]->mCode = IC_NONE; mInstructions[ops[0]]->mNumOperands = 0; mInstructions[ops[0]]->mDst.mTemp = -1;
					mInstructions[ops[1]]->mCode = IC_NONE; mInstructions[ops[1]]->mNumOperands = 0; mInstructions[ops[1]]->mDst.mTemp = -1;
					mInstructions[ops[2]]->mCode = IC_NONE; mInstructions[ops[2]]->mNumOperands = 0; mInstructions[ops[2]]->mDst.mTemp = -1;

					changed = true;
				}
			}
		}

		if (mTrueJump && mTrueJump->MapLateIntrinsics())
			changed = true;
		if (mFalseJump && mFalseJump->MapLateIntrinsics())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::PullStoreUpToConstAddress(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i], * cins;
			if (ins->mCode == IC_STORE && ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp >= 0 && CanMoveInstructionBeforeBlock(i))
			{
				int j = 0;
				while (j < mEntryBlocks.Size() && (cins = mEntryBlocks[j]->FindTempOriginSinglePath(ins->mSrc[1].mTemp)) && cins->mCode == IC_CONSTANT)
					j++;

				if (j == mEntryBlocks.Size())
				{
					for (int j = 0; j < mEntryBlocks.Size(); j++)
						mEntryBlocks[j]->AppendBeforeBranch(ins->Clone());
					changed = true;
					mInstructions.Remove(i);
					i--;
				}
			}
		}

		if (mTrueJump && mTrueJump->PullStoreUpToConstAddress())
			changed = true;
		if (mFalseJump && mFalseJump->PullStoreUpToConstAddress())
			changed = true;
	}

	return false;
}

void InterCodeBasicBlock::ReduceTempLivetimes(void)
{
	if (!mVisited)
	{
		mVisited = true;

		bool	changed;
		do {
			changed = false;

			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];

				if (ins->mCode == IC_BINARY_OPERATOR && ins->mSrc[0].mTemp >= 0 && ins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mFinal && ins->mSrc[1].mFinal)
				{
					bool	calls = false;
					int j = i;
					while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
					{
						if (mInstructions[j - 1]->mCode == IC_CALL || mInstructions[j - 1]->mCode == IC_CALL_NATIVE)
							calls = true;
						j--;
					}
					
					if (calls && j > 0 && j < i)
					{
						while (i > j)
						{
							SwapInstructions(mInstructions[i - 1], ins);
							mInstructions[i] = mInstructions[i - 1];
							i--;
						}
						mInstructions[i] = ins;
						changed = true;
					}
				}
			}

		} while (changed);

		if (mTrueJump) mTrueJump->ReduceTempLivetimes();
		if (mFalseJump) mFalseJump->ReduceTempLivetimes();
	}
}

void InterCodeBasicBlock::RemoveUnusedMallocs(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* mins = mInstructions[i];
			if (mins->mCode == IC_MALLOC)
			{
				int	mtemp = mins->mDst.mTemp;
				bool	used = false;
				int j = i + 1;
				while (j < mInstructions.Size() && !used)
				{
					InterInstruction* fins = mInstructions[j];
					if (fins->mCode == IC_FREE && fins->mSrc[0].mTemp == mtemp)
						break;
					if (fins->ReferencesTemp(mtemp))
					{
						if (fins->mCode != IC_STORE || fins->mSrc[1].mTemp != mtemp)
							used = true;
					}
					j++;
				}

				if (j < mInstructions.Size() && !used)
				{
					mins->mCode = IC_NONE; mins->mNumOperands = 0;
					for (int k = i + 1; k <= j; k++)
					{
						InterInstruction* lins = mInstructions[k];
						if (lins->UsesTemp(mtemp))
						{
							lins->mCode = IC_NONE;
							lins->mDst.mTemp = -1;
							lins->mNumOperands = 0;
						}
					}
				}
			}		
		}

		if (mTrueJump) mTrueJump->RemoveUnusedMallocs();
		if (mFalseJump) mFalseJump->RemoveUnusedMallocs();
	}
}

bool InterCodeBasicBlock::UntangleLoadStoreSequence(void)
{
	bool changed = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			if (i + 2 < mInstructions.Size())
			{
				if (mInstructions[i + 0]->mCode == IC_LOAD &&
					mInstructions[i + 1]->mCode == IC_STORE &&
					mInstructions[i + 2]->mCode == IC_STORE &&
					mInstructions[i + 1]->mSrc[0].mTemp != mInstructions[i + 0]->mDst.mTemp &&
					mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
					CanSwapInstructions(mInstructions[i + 1], mInstructions[i + 2]))
				{
					SwapInstructions(mInstructions[i + 1], mInstructions[i + 2]);
					InterInstruction* ins = mInstructions[i + 1];
					mInstructions[i + 1] = mInstructions[i + 2];
					mInstructions[i + 2] = ins;
					changed = true;
				}
			}
			if (i + 3 < mInstructions.Size())
			{
				if (mInstructions[i + 0]->mCode == IC_LOAD &&
					mInstructions[i + 1]->mCode == IC_STORE &&
					mInstructions[i + 1]->mSrc[0].mTemp != mInstructions[i + 0]->mDst.mTemp &&
					mInstructions[i + 2]->mCode == IC_LOAD &&
					mInstructions[i + 3]->mCode == IC_STORE &&
					mInstructions[i + 3]->mSrc[0].mTemp == mInstructions[i + 2]->mDst.mTemp && mInstructions[i + 3]->mSrc[0].mFinal &&
					CanSwapInstructions(mInstructions[i + 1], mInstructions[i + 2]) &&
					CanSwapInstructions(mInstructions[i + 1], mInstructions[i + 3]) &&
					CanSwapInstructions(mInstructions[i + 0], mInstructions[i + 2]) &&
					CanSwapInstructions(mInstructions[i + 0], mInstructions[i + 3]))
				{
					SwapInstructions(mInstructions[i + 1], mInstructions[i + 2]);
					SwapInstructions(mInstructions[i + 1], mInstructions[i + 3]);
					SwapInstructions(mInstructions[i + 0], mInstructions[i + 2]);
					SwapInstructions(mInstructions[i + 0], mInstructions[i + 3]);

					InterInstruction* ins = mInstructions[i + 0];
					mInstructions[i + 0] = mInstructions[i + 2];
					mInstructions[i + 2] = ins;
					ins = mInstructions[i + 1];
					mInstructions[i + 1] = mInstructions[i + 3];
					mInstructions[i + 3] = ins;

					changed = true;
				}
			}
		}
	
		if (mTrueJump && mTrueJump->UntangleLoadStoreSequence())
			changed = true;
		if (mFalseJump && mFalseJump->UntangleLoadStoreSequence())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::PropagateMemoryAliasingInfo(const GrowingInstructionPtrArray& tvalue, bool loops)
{
	if (!mVisited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);

		if (mLoopHead)
		{
			if (mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
			{
				for (int i = 0; i < ltvalue.Size(); i++)
				{
					if (ltvalue[i])
					{
						for (int j = 0; j < mInstructions.Size(); j++)
						{
							if (mInstructions[j]->mDst.mTemp == i)
							{
								if (mInstructions[j]->mCode == IC_LEA && mInstructions[j]->mSrc[1].mTemp == i)
									;
								else
								{
									ltvalue[i] = nullptr;
									break;
								}
							}
						}
					}
				}
			}
#if 1
			else if (loops)
			{
				ExpandingArray<InterCodeBasicBlock*> lblocks;

				if (CollectSingleEntryGenericLoop(lblocks))
				{
					for (int i = 0; i < ltvalue.Size(); i++)
					{
						if (ltvalue[i])
						{
							bool	fail = false;

							for (int k = 0; k < lblocks.Size() && !fail; k++)
							{
								InterCodeBasicBlock* b = lblocks[k];
								for (int j = 0; j < b->mInstructions.Size() && !fail; j++)
								{
									InterInstruction* ins = b->mInstructions[j];
									if (ins->mDst.mTemp == i)
									{
										if (ins->mCode == IC_LEA && ins->mSrc[1].mTemp == i)
											;
										else
											fail = true;
									}
								}
							}

							if (fail)
								ltvalue[i] = nullptr;
						}
					}
				}
				else
					ltvalue.Clear();
			}
#else
			else if (loops && mNumEntries == 2)
			{
				InterCodeBasicBlock* tail, * post;

				if (mEntryBlocks[0] == mLoopPrefix)
					tail = mEntryBlocks[1];
				else
					tail = mEntryBlocks[0];

				if (tail->mTrueJump == this)
					post = tail->mFalseJump;
				else
					post = tail->mTrueJump;

				if (post && post->mNumEntries == 1)
				{
					ExpandingArray<InterCodeBasicBlock*> body;

					if (tail->CollectSingleHeadLoopBody(this, tail, body))
					{
						for (int i = 0; i < ltvalue.Size(); i++)
						{
							if (ltvalue[i])
							{
								bool	fail = false;

								for (int k = 0; k < body.Size() && !fail; k++)
								{
									InterCodeBasicBlock* b = body[k];
									for (int j = 0; j < b->mInstructions.Size() && !fail; j++)
									{
										InterInstruction* ins = b->mInstructions[j];
										if (ins->mDst.mTemp == i)
										{
											if (ins->mCode == IC_LEA && ins->mSrc[1].mTemp == i)
												;
											else
												fail = true;
										}
									}
								}

								if (fail)
								{
									ltvalue[i] = nullptr;
								}
							}
						}
					}
				}
			}
#endif
			else
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
				return;
			}
		}

		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0 && ltvalue[ins->mSrc[j].mTemp] && ins->mSrc[j].mType == IT_POINTER)
				{
					ins->mSrc[j].mRestricted = ltvalue[ins->mSrc[j].mTemp]->mDst.mRestricted;
					ins->mSrc[j].mMemoryBase = ltvalue[ins->mSrc[j].mTemp]->mDst.mMemoryBase;
					ins->mSrc[j].mVarIndex = ltvalue[ins->mSrc[j].mTemp]->mDst.mVarIndex;
					ins->mSrc[j].mLinkerObject = ltvalue[ins->mSrc[j].mTemp]->mDst.mLinkerObject;
					if (ins->mSrc[j].mMemory == IM_NONE && ins->mSrc[j].mMemoryBase != IM_NONE)
						ins->mSrc[j].mMemory = IM_INDIRECT;

					assert(ins->mSrc[j].mMemoryBase != IM_LOCAL || ins->mSrc[j].mVarIndex >= 0);
				}
				else if (ins->mSrc[j].mTemp < 0 && ins->mSrc[j].mType == IT_POINTER)
					ins->mSrc[j].mMemoryBase = ins->mSrc[j].mMemory;
			}

			if (ins->mCode == IC_LEA)
			{
				ins->mDst.mMemory = ins->mSrc[1].mMemory;
				ins->mDst.mRestricted = ins->mSrc[1].mRestricted;
				if (ins->mSrc[1].mMemory != IM_INDIRECT)
					ins->mSrc[1].mMemoryBase = ins->mSrc[1].mMemory;
				ins->mDst.mMemoryBase = ins->mSrc[1].mMemoryBase;
				ins->mDst.mVarIndex = ins->mSrc[1].mVarIndex;
				ins->mDst.mLinkerObject = ins->mSrc[1].mLinkerObject;
			}
			else if (ins->mCode == IC_LOAD_TEMPORARY)
			{
				ins->mDst.mMemory = ins->mSrc[0].mMemory;
				ins->mDst.mRestricted = ins->mSrc[0].mRestricted;
				ins->mDst.mMemoryBase = ins->mSrc[0].mMemoryBase;
				ins->mDst.mVarIndex = ins->mSrc[0].mVarIndex;
				ins->mDst.mLinkerObject = ins->mSrc[0].mLinkerObject;
			}
			else if (ins->mCode == IC_CONSTANT)
			{
				ins->mDst.mMemory = ins->mConst.mMemory;
				ins->mDst.mRestricted = ins->mConst.mRestricted;
				ins->mDst.mMemoryBase = ins->mConst.mMemory;
				ins->mDst.mVarIndex = ins->mConst.mVarIndex;
				ins->mDst.mLinkerObject = ins->mConst.mLinkerObject;
			}
			else if (ins->mCode == IC_MALLOC)
			{
				ins->mDst.mMemoryBase = IM_INDIRECT;
			}

			if (ins->mDst.mTemp >= 0)
			{
				if (ins->mDst.mRestricted || ins->mDst.mMemoryBase != IM_NONE)
					ltvalue[ins->mDst.mTemp] = ins;
				else
					ltvalue[ins->mDst.mTemp] = nullptr;
			}
		}


		if (mTrueJump) mTrueJump->PropagateMemoryAliasingInfo(ltvalue, loops);
		if (mFalseJump) mFalseJump->PropagateMemoryAliasingInfo(ltvalue, loops);
	}
}

static bool IsTempModifiedInBlocks(const ExpandingArray<InterCodeBasicBlock*>& body, int temp)
{
	for (int j = 0; j < body.Size(); j++)
		if (body[j]->IsTempModified(temp))
			return true;
	return false;
}

static bool IsInsSrcModifiedInBlocks(const ExpandingArray<InterCodeBasicBlock*>& body, const InterInstruction * ins)
{
	for (int i = 0; i < ins->mNumOperands; i++)
	{
		if (ins->mSrc[i].mTemp >= 0 && IsTempModifiedInBlocks(body, ins->mSrc[i].mTemp))
			return true;
	}
	return false;
}

void InterCodeBasicBlock::InnerLoopCountZeroCheck(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2)
		{
			ExpandingArray<InterCodeBasicBlock*> body, path;
			body.Push(this);
			bool	innerLoop = true;

			InterCodeBasicBlock* lblock = nullptr;
			InterCodeBasicBlock* pblock = mLoopPrefix;

			while (pblock->mInstructions.Size() == 1 && pblock->mInstructions[0]->mCode == IC_JUMP && pblock->mEntryBlocks.Size() == 1)
				pblock = pblock->mEntryBlocks[0];

			for (int i = 0; i < mEntryBlocks.Size(); i++)
			{
				if (mEntryBlocks[i] != mLoopPrefix)
				{
					lblock = mEntryBlocks[i];
					if (!mEntryBlocks[i]->CollectLoopBody(this, body))
						innerLoop = false;
				}
			}

			if (pblock && this != lblock && innerLoop && lblock && lblock->mInstructions.Size() > 2)
			{
				int sz = lblock->mInstructions.Size();

				if (lblock->mInstructions[sz - 1]->mCode == IC_BRANCH &&
					lblock->mInstructions[sz - 2]->mCode == IC_RELATIONAL_OPERATOR &&
					lblock->mInstructions[sz - 3]->mCode == IC_BINARY_OPERATOR && lblock->mInstructions[sz - 3]->mOperator == IA_ADD)
				{
					InterInstruction* ains = lblock->mInstructions[sz - 3];
					InterInstruction* cins = lblock->mInstructions[sz - 2];
					InterInstruction* bins = lblock->mInstructions[sz - 1];

					if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
						cins->mSrc[1].mTemp == ains->mDst.mTemp &&
						cins->mSrc[0].mTemp < 0 &&
						ains->mSrc[1].mTemp == ains->mDst.mTemp &&
						ains->mSrc[0].mTemp < 0 &&
						cins->mOperator == IA_CMPLU &&
						ains->mSrc[0].mIntConst >= 1 &&
						cins->mSrc[0].mIntConst > 0)
					{
						int ctemp = ains->mDst.mTemp;

						bool fail = false;
						for (int i = 0; i < body.Size(); i++)
						{
							int sz = body[i]->mInstructions.Size();
							if (body[i] == lblock)
								sz -= 3;
							if (body[i]->IsTempReferencedInRange(0, sz, ains->mDst.mTemp))
							{
								fail = true;
								break;
							}
							else if (
								body[i]->mTrueJump && !body.Contains(body[i]->mTrueJump) && body[i]->mTrueJump->mEntryRequiredTemps[ctemp] ||
								body[i]->mFalseJump && !body.Contains(body[i]->mFalseJump) && body[i]->mFalseJump->mEntryRequiredTemps[ctemp])
							{
								fail = true;
								break;
							}
						}

						if (!fail)
						{
							int pi = pblock->mInstructions.Size() - 1;
							while (pi >= 0 && pblock->mInstructions[pi]->mDst.mTemp != ctemp)
								pi--;
							if (pi >= 0 && pblock->mInstructions[pi]->mCode == IC_CONSTANT)
							{
								int64	istart = pblock->mInstructions[pi]->mConst.mIntConst;
								if (istart >= 0)
								{
									int64	icount = (cins->mSrc[0].mIntConst - istart + ains->mSrc[0].mIntConst - 1) / ains->mSrc[0].mIntConst;

									if (icount > 0 && icount < 256)
									{
										ains->mSrc[0].mIntConst = -1;
										cins->mSrc[0].mIntConst = 0;
										pblock->mInstructions[pi]->mConst.mIntConst = icount;										

										ains->mSrc[1].mRange.SetLimit(1, icount);
										ains->mDst.mRange.SetLimit(0, icount - 1);
										cins->mSrc[1].mRange.SetLimit(0, icount - 1);
										cins->mOperator = IA_CMPNE;

										for (int i = 0; i < body.Size(); i++)
										{
											if (body[i]->mEntryValueRange.Size() > 0)
												body[i]->mEntryValueRange[ains->mSrc[1].mTemp] = ains->mSrc[1].mRange;
										}
									}
								}
							}
						}
					}
				}

			}
		}

		if (mTrueJump) mTrueJump->InnerLoopCountZeroCheck();
		if (mFalseJump) mFalseJump->InnerLoopCountZeroCheck();
	}
}

void InterCodeBasicBlock::SingleLoopCountZeroCheck(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mEntryBlocks.Size() == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
		{
			int	nins = mInstructions.Size();

			InterCodeBasicBlock * pblock = mEntryBlocks[0];
			if (pblock == this)
				pblock = mEntryBlocks[1];
			while (pblock->mInstructions.Size() == 1 && pblock->mInstructions[0]->mCode == IC_JUMP && pblock->mEntryBlocks.Size() == 1)
				pblock = pblock->mEntryBlocks[0];

			if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
				mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_ADD)
			{
				InterInstruction* ains = mInstructions[nins - 3];
				InterInstruction* cins = mInstructions[nins - 2];
				InterInstruction* bins = mInstructions[nins - 1];

				if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
					cins->mSrc[1].mTemp == ains->mDst.mTemp &&
					cins->mSrc[0].mTemp < 0 &&
					ains->mSrc[1].mTemp == ains->mDst.mTemp &&
					ains->mSrc[0].mTemp < 0 &&
					cins->mOperator == IA_CMPGS &&
					ains->mSrc[0].mIntConst < -1 &&
					cins->mSrc[0].mIntConst > 0 &&
					cins->mSrc[0].mIntConst < - ains->mSrc[0].mIntConst &&
					!IsTempModifiedInRange(0, nins - 3, ains->mDst.mTemp))
				{
					int pi = pblock->mInstructions.Size() - 1;
					while (pi >= 0 && pblock->mInstructions[pi]->mDst.mTemp != ains->mDst.mTemp)
						pi--;
					if (pi >= 0 && pblock->mInstructions[pi]->mCode == IC_CONSTANT)
					{
						int64	istart = pblock->mInstructions[pi]->mConst.mIntConst;
						if (istart > 0)
						{
							int64	iend = istart % -ains->mSrc[0].mIntConst;

							if (cins->mSrc[0].mIntConst < iend)
							{
								cins->mSrc[0].mIntConst = 0;
								cins->mOperator = IA_CMPGES;
							}
						}
					}
				}
			}
		}

		if (mTrueJump) mTrueJump->SingleLoopCountZeroCheck();
		if (mFalseJump) mFalseJump->SingleLoopCountZeroCheck();
	}
}

bool InterCodeBasicBlock::MoveConditionOutOfLoop(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead)
		{
			ExpandingArray<InterCodeBasicBlock*> body, path;
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
				int nscale = 4, nlimit = 4, nmaxlimit = 8;
				if (mProc->mCompilerOptions & COPT_OPTIMIZE_AUTO_UNROLL)
				{
					nscale = 1;
					nlimit = 10;
					nmaxlimit = 16;
				}

				// Find all conditions based on invariants
				for (int i = 0; i < body.Size(); i++)
				{
					InterCodeBasicBlock* block = body[i];
					int nins = block->mInstructions.Size();
					if (block->mFalseJump && block->mInstructions[nins-1]->mCode == IC_BRANCH && body.Contains(block->mFalseJump) && body.Contains(block->mTrueJump))
					{
						int	ncins = 0;
						if (!IsInsSrcModifiedInBlocks(body, block->mInstructions[nins - 1]))
							ncins = 1;
						else if (nins > 1 && block->mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
							block->mInstructions[nins - 1]->mSrc[0].mTemp == block->mInstructions[nins - 2]->mDst.mTemp && block->mInstructions[nins - 1]->mSrc[0].mFinal &&
							!IsInsSrcModifiedInBlocks(body, block->mInstructions[nins - 2]))
							ncins = 2;

						if (ncins > 0)
						{
							// The condition is not modified on the path
							// Now check the number of instructions in the conditional section
							
							int ninside = 0, noutside = 0;
							for (int i = 0; i < body.Size(); i++)
							{
								bool	tdom = block->mTrueJump->IsDirectLoopPathBlock(body[i]);
								bool	fdom = block->mFalseJump->IsDirectLoopPathBlock(body[i]);
								if (tdom != fdom)
									ninside += body[i]->mInstructions.Size() + 1;
								else
									noutside += body[i]->mInstructions.Size() + 1;
							}

							// Less than four instructions outside of condition, or twice as many
							// inside as outside is the trigger
							if (noutside - ncins < nmaxlimit && (noutside - ncins < nlimit || 2 * ninside > nscale * (noutside - ncins)))
							{
								// Now clone the loop into a true and a false branch

								GrowingArray<InterCodeBasicBlock*>	copies(nullptr);
								for (int i = 0; i < body.Size(); i++)
								{
									InterCodeBasicBlock* nblock = body[i]->Clone();
									copies[body[i]->mIndex] = nblock;
								}

								for (int i = 0; i < body.Size(); i++)
								{
									InterCodeBasicBlock* rblock = body[i];
									InterCodeBasicBlock* nblock = copies[rblock->mIndex];
									if (rblock->mTrueJump)
									{
										InterCodeBasicBlock* tblock = copies[rblock->mTrueJump->mIndex];
										if (tblock)
											nblock->mTrueJump = tblock;
										else
											nblock->mTrueJump = rblock->mTrueJump;
									}
									if (rblock->mFalseJump)
									{
										InterCodeBasicBlock* tblock = copies[rblock->mFalseJump->mIndex];
										if (tblock)
											nblock->mFalseJump = tblock;
										else
											nblock->mFalseJump = rblock->mFalseJump;
									}
								}
								mLoopPrefix->mInstructions.Pop();
								for (int i = 0; i < ncins; i++)
									mLoopPrefix->mInstructions.Push(block->mInstructions[nins - ncins + i]->Clone());

								block->mInstructions[nins - 1]->mSrc[0].mTemp = -1;
								block->mInstructions[nins - 1]->mSrc[0].mIntConst = 1;

								mLoopPrefix->mFalseJump = copies[mLoopPrefix->mTrueJump->mIndex];

								InterCodeBasicBlock* nblock = copies[block->mIndex];
								nblock->mInstructions[nins - 1]->mSrc[0].mTemp = -1;
								nblock->mInstructions[nins - 1]->mSrc[0].mIntConst = 0;

								return true;
							}
						}
					}
				}
			}
			else if (mEntryBlocks.Size() == 2 && mInstructions.Size() == 1 && mInstructions[0]->mCode == IC_BRANCH && mInstructions[0]->mSrc[0].mTemp >= 0)
			{
				InterCodeBasicBlock* tail, * post;

				if (mEntryBlocks[0] == mLoopPrefix)
					tail = mEntryBlocks[1];
				else
					tail = mEntryBlocks[0];

				if (mTrueJump == tail || mFalseJump == tail)
				{
					if (tail->mTrueJump == this)
						post = tail->mFalseJump;
					else
						post = tail->mTrueJump;

					if (post && post->mNumEntries == 1)
					{
						ExpandingArray<InterCodeBasicBlock*> lbody;

						if (tail->CollectSingleHeadLoopBody(this, tail, lbody))
						{
							int tz = tail->mInstructions.Size();
							int ct = mInstructions[0]->mSrc[0].mTemp;

							int i = 0;
							while (i < lbody.Size() && !lbody[i]->IsTempModified(ct))
								i++;

							if (i == lbody.Size())
							{
								i = 0;
								while (i < tz && !IsObservable(tail->mInstructions[i]->mCode) && (tail->mInstructions[i]->mDst.mTemp < 0 || !post->mEntryRequiredTemps[tail->mInstructions[i]->mDst.mTemp]))
									i++;

								if (i == tz)
								{
									InterInstruction * ins = mLoopPrefix->mInstructions.Pop();
									mLoopPrefix->mInstructions.Push(mInstructions.Pop());
									mInstructions.Push(ins);

									tail->mEntryBlocks.RemoveAll(this);
									tail->mNumEntries--;

									post->mEntryBlocks.Push(mLoopPrefix);
									post->mNumEntries++;
									
									if (mTrueJump == tail)
									{
										mTrueJump = mFalseJump;
										mLoopPrefix->mTrueJump = post;
										mLoopPrefix->mFalseJump = this;
									}
									else
										mLoopPrefix->mFalseJump = post;
									mFalseJump = nullptr;

									return true;
								}
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->MoveConditionOutOfLoop())
			return true;
		if (mFalseJump && mFalseJump->MoveConditionOutOfLoop())
			return true;
	}

	return false;
}



void InterCodeBasicBlock::PushMoveOutOfLoop(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && mFalseJump)
		{
			InterCodeBasicBlock* eblock = nullptr, * lblock = nullptr;

			if (mTrueJump->mLoopHead)
			{
				lblock = mTrueJump;
				eblock = mFalseJump;
			}
			else if (mFalseJump->mLoopHead)
			{
				lblock = mFalseJump;
				eblock = mTrueJump;
			}				

			if (eblock)
			{
				int i = 0;
				while (i < mInstructions.Size())
				{
					InterInstruction* mins = mInstructions[i];
					if (mins->mCode == IC_LOAD_TEMPORARY && !mins->mSrc[0].mFinal)
					{
						if (!lblock->mEntryRequiredTemps[mins->mDst.mTemp] && eblock->mEntryRequiredTemps[mins->mDst.mTemp] && !eblock->mExitRequiredTemps[mins->mDst.mTemp])
						{
							int	offset = 0;
							int j = i + 1;
							bool	fail = false;

							while (j < mInstructions.Size() && !fail)
							{
								InterInstruction* cins = mInstructions[j];
								if (cins->ReferencesTemp(mins->mDst.mTemp))
									fail = true;
								else if (cins->mDst.mTemp == mins->mSrc[0].mTemp)
								{
									if (cins->mCode == IC_LEA && cins->mSrc[1].mTemp == mins->mSrc[0].mTemp && cins->mSrc[0].mTemp < 0)
										offset += int(cins->mSrc[0].mIntConst);
									else
										fail = true;
								}
								j++;
							}

							if (!fail)
							{
								int j = 0;
								while (j < eblock->mInstructions.Size() && !fail)
								{
									InterInstruction* cins = eblock->mInstructions[j];

									if (cins->ReferencesTemp(mins->mDst.mTemp))
									{
										if (cins->mCode == IC_LEA && cins->mSrc[1].mTemp == mins->mDst.mTemp && cins->mSrc[0].mTemp < 0)
										{
											if (cins->mSrc[1].mFinal)
												break;
										}
										else
											fail = true;
									}

									if (cins->mDst.mTemp == mins->mSrc[0].mTemp)
										fail = true;

									j++;
								}

								if (!fail)
								{
									eblock->mEntryRequiredTemps += mins->mSrc[0].mTemp;

									j = 0;
									while (j < eblock->mInstructions.Size())
									{
										InterInstruction* cins = eblock->mInstructions[j];
										if (cins->ReferencesTemp(mins->mDst.mTemp))
										{
											if (cins->mCode == IC_LEA && cins->mSrc[1].mTemp == mins->mDst.mTemp && cins->mSrc[0].mTemp < 0)
											{
												cins->mSrc[1].mTemp = mins->mSrc[0].mTemp;
												cins->mSrc[0].mIntConst -= offset;

												if (cins->mSrc[1].mFinal)
													break;
											}
										}
										j++;
									}
								}
							}
						}
					}

					i++;
				}
			}
		}

		if (mTrueJump)
			mTrueJump->PushMoveOutOfLoop();
		if (mFalseJump)
			mFalseJump->PushMoveOutOfLoop();
	}
}

bool  InterCodeBasicBlock::CheckSingleBlockLimitedLoop(InterCodeBasicBlock*& pblock, int64& nloop, bool& nfixed)
{
	if (mLoopHead && mEntryBlocks.Size() == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
	{
		int	nins = mInstructions.Size();

		pblock = mEntryBlocks[0];
		if (pblock == this)
			pblock = mEntryBlocks[1];

		if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
			mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_ADD)
		{
			InterInstruction* ains = mInstructions[nins - 3];
			InterInstruction* cins = mInstructions[nins - 2];
			InterInstruction* bins = mInstructions[nins - 1];

			if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
				cins->mSrc[1].mTemp == ains->mDst.mTemp &&
				cins->mSrc[0].mTemp < 0 &&
				ains->mSrc[1].mTemp == ains->mDst.mTemp &&
				ains->mSrc[0].mTemp < 0 &&
				(cins->mOperator == IA_CMPLU || cins->mOperator == IA_CMPLEU) &&
				cins->mSrc[0].mIntConst < 255 &&
				ains->mSrc[0].mIntConst > 0)
			{
				InterInstruction* iins = pblock->FindTempOriginSinglePath(ains->mDst.mTemp);

				if (iins && iins->mCode == IC_CONSTANT)
				{
					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 3)
					{
						nloop = cins->mSrc[0].mIntConst - iins->mConst.mIntConst;
						if (cins->mOperator == IA_CMPLEU)
							nloop++;
						nloop = (nloop + ains->mSrc[0].mIntConst - 1) / ains->mSrc[0].mIntConst;
						nfixed = true;

						return true;
					}
				}
			}
			else if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
				cins->mSrc[1].mTemp == ains->mDst.mTemp &&
				cins->mSrc[0].mRange.mMaxState == IntegerValueRange::S_BOUND &&
				ains->mSrc[1].mTemp == ains->mDst.mTemp &&
				ains->mSrc[0].mTemp < 0 &&
				(cins->mOperator == IA_CMPLU || cins->mOperator == IA_CMPLEU) &&
				cins->mSrc[0].mRange.mMaxValue < 255 &&
				ains->mSrc[0].mRange.mMaxValue > 0)
			{
				InterInstruction* iins = pblock->FindTempOriginSinglePath(ains->mDst.mTemp);

				if (iins && iins->mCode == IC_CONSTANT)
				{
					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 3)
					{
						nloop = cins->mSrc[0].mRange.mMaxValue - iins->mConst.mIntConst;
						if (cins->mOperator == IA_CMPLEU)
							nloop++;
						nloop = (nloop + ains->mSrc[0].mIntConst - 1) / ains->mSrc[0].mIntConst;
						nfixed = false;

						return true;
					}
				}
			}
		}
		else if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
			mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_SUB)
		{
			InterInstruction* ains = mInstructions[nins - 3];
			InterInstruction* cins = mInstructions[nins - 2];
			InterInstruction* bins = mInstructions[nins - 1];

			if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
				cins->mSrc[1].mTemp == ains->mDst.mTemp &&
				cins->mSrc[0].mTemp < 0 &&
				ains->mSrc[1].mTemp == ains->mDst.mTemp &&
				ains->mSrc[0].mTemp < 0 &&
				(cins->mOperator == IA_CMPGU || cins->mOperator == IA_CMPGEU) &&
				ains->mSrc[0].mIntConst > 0)
			{
				InterInstruction* iins = pblock->FindTempOriginSinglePath(ains->mDst.mTemp);

				if (iins && iins->mCode == IC_CONSTANT)
				{
					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 3)
					{
						nloop = iins->mConst.mIntConst - cins->mSrc[0].mIntConst;
						if (cins->mOperator == IA_CMPGEU)
							nloop++;
						nloop = (nloop + ains->mSrc[0].mIntConst - 1) / ains->mSrc[0].mIntConst;
						nfixed = true;

						return true;
					}
				}
			}
		}
		else if (
			mInstructions[nins - 1]->mCode == IC_BRANCH &&
			mInstructions[nins - 2]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 2]->mOperator == IA_ADD)
		{
			InterInstruction* ains = mInstructions[nins - 2];
			InterInstruction* bins = mInstructions[nins - 1];

			if (bins->mSrc[0].mTemp == ains->mDst.mTemp &&
				ains->mSrc[1].mTemp == ains->mDst.mTemp &&
				ains->mSrc[0].mTemp < 0 &&
				ains->mSrc[0].mIntConst == -1)
			{
				InterInstruction* iins = pblock->FindTempOriginSinglePath(ains->mDst.mTemp);

				if (iins && iins->mCode == IC_CONSTANT)
				{
					int i = 0;
					while (i < nins - 2 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 2)
					{
						nloop = iins->mConst.mIntConst;

						mProc->mLocalValueRange[ains->mDst.mTemp].LimitMin(1);
						mProc->mLocalValueRange[ains->mDst.mTemp].LimitMax(iins->mConst.mIntConst);

						nfixed = false;
						return true;
					}
				}
			}
		}
		else if (
			mInstructions[nins - 1]->mCode == IC_BRANCH &&
			mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
			mInstructions[nins - 3]->mCode == IC_LEA)
		{
			InterInstruction* ains = mInstructions[nins - 3];
			InterInstruction* cins = mInstructions[nins - 2];
			InterInstruction* bins = mInstructions[nins - 1];

			if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
				(cins->mSrc[1].mTemp == ains->mDst.mTemp && cins->mSrc[0].mTemp < 0 || cins->mSrc[0].mTemp == ains->mDst.mTemp && cins->mSrc[1].mTemp < 0) &&
				ains->mSrc[1].mTemp == ains->mDst.mTemp &&
				ains->mSrc[0].mTemp < 0 &&
				cins->mOperator == IA_CMPNE &&
				ains->mSrc[0].mIntConst != 0)
			{
				int ci = cins->mSrc[0].mTemp < 0 ? 0 : 1;

				InterInstruction* iins = pblock->FindTempOriginSinglePath(ains->mDst.mTemp);

				if (iins && iins->mCode == IC_CONSTANT)
				{
					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;

					if (i == nins - 3 && SameMemRegion(iins->mConst, cins->mSrc[ci]))
					{
						int64 ndiv = cins->mSrc[ci].mIntConst - iins->mConst.mIntConst;
						nloop = ndiv / ains->mSrc[0].mIntConst;
						if (ndiv == nloop * ains->mSrc[0].mIntConst)
						{
							nfixed = true;
							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}


bool InterCodeBasicBlock::SingleBlockLoopSinking(int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
		{
			InterCodeBasicBlock* pblock = mEntryBlocks[0], * eblock = mFalseJump;
			if (pblock == this)
				pblock = mEntryBlocks[1];
			if (eblock == this)
				eblock = mTrueJump;

			if (eblock->mNumEntries == 1)
			{
				int j = mInstructions.Size() - 1;
				while (j >= 0)
				{
					InterInstruction* ins = mInstructions[j];

					if (ins->mCode != IC_LOAD_TEMPORARY && !HasSideEffect(ins->mCode) && ins->mDst.mTemp >= 0 && !IsTempUsedInRange(0, mInstructions.Size(), ins->mDst.mTemp) && !ins->mVolatile)
					{
						if (spareTemps + ins->mNumOperands >= mEntryRequiredTemps.Size() + 16)
							return true;

						mInstructions.Remove(j);
						eblock->mInstructions.Insert(0, ins);

						for (int i = 0; i < ins->mNumOperands; i++)
						{
							if (ins->mSrc[i].mTemp >= 0 && IsTempModifiedInRange(j, mInstructions.Size(), ins->mSrc[i].mTemp))
							{
								InterInstruction* tins = new InterInstruction(ins->mLocation, IC_LOAD_TEMPORARY);
								tins->mSrc[0] = ins->mSrc[i];
								tins->mDst = tins->mSrc[0];
								tins->mDst.mTemp = spareTemps++;
								ins->mSrc[i] = tins->mDst;
								mInstructions.Insert(j, tins);
							}
						}

						changed = true;
					}

					j--;
				}
			}
		}

		if (mTrueJump && mTrueJump->SingleBlockLoopSinking(spareTemps))
			changed = true;
		if (mFalseJump && mFalseJump->SingleBlockLoopSinking(spareTemps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::SingleBlockLoopPointerToByte(int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
		{
			int	nins = mInstructions.Size();

			InterCodeBasicBlock* pblock = mEntryBlocks[0], * eblock = mFalseJump;
			if (pblock == this)
				pblock = mEntryBlocks[1];
			if (eblock == this)
				eblock = mTrueJump;

			if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
				mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_ADD)
			{
				InterInstruction* ains = mInstructions[nins - 3];
				InterInstruction* cins = mInstructions[nins - 2];
				InterInstruction* bins = mInstructions[nins - 1];

				if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
					cins->mSrc[1].mTemp == ains->mDst.mTemp &&
					cins->mSrc[0].mTemp < 0 &&
					ains->mSrc[1].mTemp == ains->mDst.mTemp &&
					ains->mSrc[0].mTemp < 0 &&
					(cins->mOperator == IA_CMPLU || cins->mOperator == IA_CMPLEU) &&
					cins->mSrc[0].mIntConst < 255 &&
					ains->mSrc[0].mIntConst > 0)
				{
					GrowingArray<InterInstructionPtr>	tvalues(nullptr);
					tvalues.SetSize(mEntryRequiredTemps.Size() + 16);

					GrowingArray<int>					mtemps(-1);

					int pi = pblock->mInstructions.Size() - 1;
					while (pi >= 0 && pblock->mInstructions[pi]->mDst.mTemp != ains->mDst.mTemp)
						pi--;

					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 3)
					{					
						int nloop = int(cins->mSrc[0].mIntConst);
						if (cins->mOperator == IA_CMPLEU)
							nloop++;
						nloop /= int(ains->mSrc[0].mIntConst);

						for (int i = 0; i < mInstructions.Size() - 3; i++)
						{
							InterInstruction* lins = mInstructions[i];
							if (lins->mCode == IC_LEA && lins->mDst.mTemp == lins->mSrc[1].mTemp && lins->mSrc[0].mTemp < 0 && lins->mSrc[0].mIntConst > 0 && lins->mSrc[0].mIntConst * nloop < 256 &&
								!IsTempReferencedInRange(i + 1, mInstructions.Size(), lins->mDst.mTemp) && !IsTempModifiedInRange(0, i, lins->mDst.mTemp) &&
								!eblock->mEntryRequiredTemps[lins->mDst.mTemp])
							{
								bool	isglobal = false;
								InterInstruction* slins = pblock->FindTempOrigin(lins->mSrc[1].mTemp);
								if (slins)
								{
									if (slins->mCode == IC_CONSTANT)
									{
										if (slins->mConst.mMemory == IM_ABSOLUTE || slins->mConst.mMemory == IM_GLOBAL)
											isglobal = true;
									}
									else if (slins->mCode == IC_LEA)
									{
										if (slins->mSrc[0].mTemp < 0)
										{
											if (slins->mSrc[1].mMemory == IM_ABSOLUTE || slins->mSrc[1].mMemory == IM_GLOBAL)
												isglobal = true;
										}
									}
								}

								bool	failed = false;
								for (int j = 0; j < i; j++)
								{
									InterInstruction* jins = mInstructions[j];

									if (jins->ReferencesTemp(lins->mDst.mTemp))
									{
										if (jins->mCode == IC_LOAD)
										{
											if (!isglobal && (jins->mSrc[0].mIntConst < 0 || jins->mSrc[0].mIntConst > 2))
												failed = true;
										}
										else if (jins->mCode == IC_STORE)
										{
											if (!isglobal && (jins->mSrc[1].mIntConst < 0 || jins->mSrc[1].mIntConst > 2))
												failed = true;
										}
										else
											failed = true;
									}
								}

								if (!failed)
								{
									if (spareTemps + 2 >= mEntryRequiredTemps.Size() + 16)
										return true;
									
									int inc = int(lins->mSrc[0].mIntConst);

									int ireg = mtemps[inc];

									if (ireg < 0)
									{
										ireg = spareTemps++;

										InterInstruction* cins = new InterInstruction(lins->mLocation, IC_CONSTANT);
										cins->mDst.mTemp = ireg;
										cins->mDst.mType = IT_INT16;
										cins->mConst.mType = IT_INT16;
										cins->mConst.mIntConst = 0;
										mtemps[inc] = cins->mDst.mTemp;

										pblock->mInstructions.Insert(pblock->mInstructions.Size() - 1, cins);

										InterInstruction* iins = new InterInstruction(lins->mLocation, IC_BINARY_OPERATOR);
										iins->mNumOperands = 2;
										iins->mOperator = IA_ADD;
										iins->mDst = cins->mDst;
										iins->mSrc[1] = cins->mDst;
										iins->mSrc[0].mTemp = -1;
										iins->mSrc[0].mType = IT_INT16;
										iins->mSrc[0].mIntConst = lins->mSrc[0].mIntConst;
										iins->mSrc[1].mRange.SetLimit(0, inc * (nloop - 1));
										iins->mDst.mRange.SetLimit(lins->mSrc[0].mIntConst, inc * nloop);
										mInstructions.Insert(i, iins);
									}

									InterInstruction* nins = new InterInstruction(lins->mLocation, IC_LEA);
									nins->mNumOperands = 2;
									nins->mDst.mTemp = spareTemps++;
									nins->mDst.mType = IT_POINTER;
									nins->mSrc[1] = lins->mSrc[1];
									nins->mSrc[1].mFinal = false;
									nins->mSrc[0].mType = IT_INT16;
									nins->mSrc[0].mTemp = ireg;
									nins->mSrc[0].mRange.SetLimit(0, inc * (nloop - 1));

									for (int j = 0; j < i; j++)
									{
										InterInstruction* jins = mInstructions[j];
										for (int k = 0; k < jins->mNumOperands; k++)
											if (jins->mSrc[k].mTemp == lins->mDst.mTemp)
												jins->mSrc[k].mTemp = nins->mDst.mTemp;
									}

									mInstructions.Insert(0, nins);

									lins->mCode = IC_NONE;
									lins->mNumOperands = 0;
									lins->mDst.mTemp = -1;

									changed = true;
								}
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->SingleBlockLoopPointerToByte(spareTemps))
			changed = true;
		if (mFalseJump && mFalseJump->SingleBlockLoopPointerToByte(spareTemps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::SingleBlockLoopPointerSplit(int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
		{
			int	nins = mInstructions.Size();

			InterCodeBasicBlock* pblock = mEntryBlocks[0], *eblock = mFalseJump;
			if (pblock == this)
				pblock = mEntryBlocks[1];
			if (eblock == this)
				eblock = mTrueJump;

			if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
				mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[nins - 3]->mCode == IC_BINARY_OPERATOR && mInstructions[nins - 3]->mOperator == IA_ADD)
			{
				InterInstruction* ains = mInstructions[nins - 3];
				InterInstruction* cins = mInstructions[nins - 2];
				InterInstruction* bins = mInstructions[nins - 1];

				if (bins->mSrc[0].mTemp == cins->mDst.mTemp &&
					cins->mSrc[1].mTemp == ains->mDst.mTemp &&
					cins->mSrc[0].mTemp < 0 &&
					ains->mSrc[1].mTemp == ains->mDst.mTemp &&
					ains->mSrc[0].mTemp < 0)
				{
					GrowingArray<InterInstructionPtr>	tvalues(nullptr);
					tvalues.SetSize(mEntryRequiredTemps.Size() + 16);

					int pi = pblock->mInstructions.Size() - 1;
					while (pi >= 0 && pblock->mInstructions[pi]->mDst.mTemp != ains->mDst.mTemp)
						pi--;

					int i = 0;
					while (i < nins - 3 && mInstructions[i]->mDst.mTemp != ains->mDst.mTemp)
						i++;
					if (i == nins - 3)
					{
						InterInstruction* xins = nullptr;
						for (int i = 0; i < mInstructions.Size() - 3; i++)
						{
							InterInstruction* lins = mInstructions[i];
							
							if (xins && lins->mDst.mTemp >= 0 && lins->mDst.mTemp == xins->mDst.mTemp)
								xins = nullptr;

							if (lins->mCode == IC_LEA && lins->mSrc[0].mTemp == ains->mDst.mTemp && lins->mSrc[0].IsUByte() && lins->mSrc[1].mTemp >= 0 && !mLocalModifiedTemps[lins->mSrc[1].mTemp])
							{
								tvalues[lins->mDst.mTemp] = lins;
							}
							else if (lins->mCode == IC_LEA && xins && lins->mSrc[0].mTemp == xins->mDst.mTemp && lins->mSrc[0].IsUByte() && lins->mSrc[1].mTemp >= 0 && !mLocalModifiedTemps[lins->mSrc[1].mTemp])
							{
								tvalues[lins->mDst.mTemp] = lins;
							}
							else if (lins->mCode == IC_CONVERSION_OPERATOR && lins->mOperator == IA_EXT8TO16U && lins->mSrc[0].mTemp == ains->mDst.mTemp && lins->mSrc[0].IsUByte())
							{
								xins = lins;
							}
							else if (lins->mCode == IC_LEA && lins->mSrc[0].mTemp < 0 && lins->mSrc[0].mIntConst == ains->mSrc[0].mIntConst && lins->mSrc[1].mTemp == lins->mDst.mTemp && 
								pi >= 0 && pblock->mInstructions[pi]->mCode == IC_CONSTANT && ains->mSrc[1].IsUByte() && pblock->mInstructions[pi]->mConst.mIntConst == 0 &&
								!IsTempReferencedInRange(i + 1, mInstructions.Size(), lins->mDst.mTemp) && !IsTempModifiedInRange(0, i, lins->mDst.mTemp) &&
								!eblock->mEntryRequiredTemps[lins->mDst.mTemp])
							{
								if (spareTemps + 2 >= mEntryRequiredTemps.Size() + 16)
									return true;


								InterInstruction* nins = new InterInstruction(lins->mLocation, IC_LEA);
								InterInstruction* cins = nullptr;
								nins->mSrc[1] = lins->mSrc[1];

								if (ains->mDst.mType == IT_INT16)
									nins->mSrc[0] = ains->mSrc[1];
								else
								{
									cins = new InterInstruction(lins->mLocation, IC_CONVERSION_OPERATOR);
									cins->mOperator = IA_EXT8TO16U;
									cins->mSrc[0] = ains->mSrc[1];
									cins->mDst.mMemory = IM_INDIRECT;
									cins->mDst.mTemp = spareTemps++;
									cins->mDst.mType = IT_INT16;
									nins->mSrc[0] = cins->mDst;
								}

								nins->mDst.mMemory = IM_INDIRECT;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_POINTER;

								for (int j = 0; j < i; j++)
								{
									InterInstruction* tins = mInstructions[j];
									for (int k = 0; k < tins->mNumOperands; k++)
									{
										if (tins->mSrc[k].mTemp == lins->mDst.mTemp)
											tins->mSrc[k].mTemp = nins->mDst.mTemp;
									}
								}

								mInstructions.Remove(i);
								mInstructions.Insert(0, nins);
								if (cins)
									mInstructions.Insert(0, cins);

								changed = true;
							}
							else if (lins->mCode == IC_STORE && lins->mSrc[1].mTemp >= 0 && lins->mSrc[1].mIntConst >= 32 && tvalues[lins->mSrc[1].mTemp])
							{
								if (spareTemps + 2 >= mEntryRequiredTemps.Size() + 16)
									return true;

								InterInstruction* pins = tvalues[lins->mSrc[1].mTemp];
								InterInstruction* nins = new InterInstruction(lins->mLocation, IC_LEA);
								nins->mSrc[1] = pins->mSrc[1];
								nins->mSrc[0].mTemp = -1;
								nins->mSrc[0].mType = IT_INT16;
								nins->mSrc[0].mIntConst = lins->mSrc[1].mIntConst;
								nins->mDst.mMemory = IM_INDIRECT;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_POINTER;

								pblock->mInstructions.Insert(pblock->mInstructions.Size() - 1, nins);

								InterInstruction* mins = pins->Clone();
								mins->mDst.mTemp = spareTemps++;
								mins->mDst.mMemory = IM_INDIRECT;								
								mins->mDst.mRange.Reset();
								mins->mSrc[1] = nins->mDst;
								mInstructions.Insert(i, mins);

								
								lins->mSrc[1].mTemp = mins->mDst.mTemp;
								lins->mSrc[1].mIntConst = 0;
								lins->mSrc[1].mRange.Reset();

								changed = true;
							}
							else if (lins->mCode == IC_LOAD && lins->mSrc[0].mTemp >= 0 && lins->mSrc[0].mIntConst >= 16 && tvalues[lins->mSrc[0].mTemp])
							{
								if (spareTemps + 2 >= mEntryRequiredTemps.Size() + 16)
									return true;

								InterInstruction* pins = tvalues[lins->mSrc[0].mTemp];
								InterInstruction* nins = new InterInstruction(lins->mLocation, IC_LEA);
								nins->mSrc[1] = pins->mSrc[1];
								nins->mSrc[0].mTemp = -1;
								nins->mSrc[0].mType = IT_INT16;
								nins->mSrc[0].mIntConst = lins->mSrc[0].mIntConst;
								nins->mDst.mMemory = IM_INDIRECT;
								nins->mDst.mTemp = spareTemps++;
								nins->mDst.mType = IT_POINTER;

								pblock->mInstructions.Insert(pblock->mInstructions.Size() - 1, nins);

								InterInstruction* mins = pins->Clone();
								mins->mDst.mTemp = spareTemps++;
								mins->mDst.mMemory = IM_INDIRECT;
								mins->mDst.mRange.Reset();
								mins->mSrc[1] = nins->mDst;
								mInstructions.Insert(i, mins);


								lins->mSrc[0].mTemp = mins->mDst.mTemp;
								lins->mSrc[0].mIntConst = 0;
								lins->mSrc[0].mRange.Reset();

								changed = true;
							}
							else if (lins->mDst.mTemp >= 0)
								tvalues[lins->mDst.mTemp] = nullptr;
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->SingleBlockLoopPointerSplit(spareTemps))
			changed = true;
		if (mFalseJump && mFalseJump->SingleBlockLoopPointerSplit(spareTemps))
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::Flatten2DLoop(void)
{
	return false;

	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && !mFalseJump && mTrueJump && mTrueJump->mNumEntries == 1 && !mTrueJump->mFalseJump && mTrueJump->mInstructions.Size() == 1)
		{
			InterCodeBasicBlock* lblock = mTrueJump->mTrueJump;

			if (lblock->mLoopHead && lblock->mNumEntries == 2 && lblock->mTrueJump == lblock)
			{
				InterCodeBasicBlock* eblock = lblock->mFalseJump;

				if (eblock && eblock->mNumEntries == 1 && eblock->mTrueJump == this)
				{
					int esz = eblock->mInstructions.Size();
					if (esz >= 3 &&
						eblock->mInstructions[esz - 1]->mCode == IC_BRANCH &&
						eblock->mInstructions[esz - 2]->mCode == IC_RELATIONAL_OPERATOR && eblock->mInstructions[esz - 2]->mDst.mTemp == eblock->mInstructions[esz - 1]->mSrc[0].mTemp &&
						(eblock->mInstructions[esz - 2]->mOperator == IA_CMPLU || eblock->mInstructions[esz - 2]->mOperator == IA_CMPNE) &&
						eblock->mInstructions[esz - 2]->mSrc[0].mTemp < 0 && eblock->mInstructions[esz - 2]->mSrc[1].IsUnsigned() && eblock->mInstructions[esz - 2]->mSrc[1].mRange.mMaxValue <= eblock->mInstructions[esz - 2]->mSrc[0].mIntConst &&
						eblock->mInstructions[esz - 3]->mCode == IC_BINARY_OPERATOR && eblock->mInstructions[esz - 3]->mDst.mTemp == eblock->mInstructions[esz - 2]->mSrc[1].mTemp &&
						eblock->mInstructions[esz - 3]->mDst.mTemp == eblock->mInstructions[esz - 3]->mSrc[1].mTemp &&
						eblock->mInstructions[esz - 3]->mSrc[0].mTemp < 0 && eblock->mInstructions[esz - 3]->mSrc[0].mIntConst == 1)
					{
						int otemp = eblock->mInstructions[esz - 3]->mDst.mTemp;
						int sz = mInstructions.Size();
						if (sz == 3 &&
							mInstructions[0]->mCode == IC_BINARY_OPERATOR && (mInstructions[0]->mOperator == IA_SHL || mInstructions[0]->mOperator == IA_MUL) && 
							mInstructions[0]->mSrc[0].mTemp < 0 && mInstructions[0]->mSrc[1].mTemp == otemp)
						{
							int64 scale = mInstructions[0]->mSrc[0].mIntConst;
							if (mInstructions[0]->mOperator == IA_SHL)
								scale = 1LL << scale;

							if (mInstructions[1]->mCode == IC_CONSTANT && mInstructions[1]->mConst.mIntConst == 0)
							{
								int ilz = lblock->mInstructions.Size();
								if (ilz >= 3 &&
									lblock->mInstructions[ilz - 1]->mCode == IC_BRANCH &&
									lblock->mInstructions[ilz - 2]->mCode == IC_RELATIONAL_OPERATOR && lblock->mInstructions[ilz - 2]->mDst.mTemp == lblock->mInstructions[ilz - 1]->mSrc[0].mTemp &&
									(lblock->mInstructions[ilz - 2]->mOperator == IA_CMPLU || lblock->mInstructions[ilz - 2]->mOperator == IA_CMPNE) &&
									lblock->mInstructions[ilz - 2]->mSrc[0].mTemp < 0 && lblock->mInstructions[ilz - 2]->mSrc[1].IsUnsigned() && lblock->mInstructions[ilz - 2]->mSrc[1].mRange.mMaxValue <= lblock->mInstructions[ilz - 2]->mSrc[0].mIntConst &&
									lblock->mInstructions[ilz - 3]->mCode == IC_BINARY_OPERATOR && lblock->mInstructions[ilz - 3]->mDst.mTemp == lblock->mInstructions[ilz - 2]->mSrc[1].mTemp &&
									lblock->mInstructions[ilz - 3]->mDst.mTemp == lblock->mInstructions[ilz - 3]->mSrc[1].mTemp &&
									lblock->mInstructions[ilz - 3]->mSrc[0].mTemp < 0 && lblock->mInstructions[ilz - 3]->mSrc[0].mIntConst == 1)
								{
									int itemp = lblock->mInstructions[ilz - 3]->mDst.mTemp;
									int64 icount = lblock->mInstructions[ilz - 2]->mSrc[0].mIntConst;

									if (itemp == mInstructions[1]->mDst.mTemp && icount == scale &&
										!eblock->mEntryRequiredTemps[itemp] &&
										!eblock->mFalseJump->mEntryRequiredTemps[otemp])
									{
										if (lblock->mInstructions[0]->mCode == IC_BINARY_OPERATOR && lblock->mInstructions[0]->mOperator == IA_ADD &&
											((lblock->mInstructions[0]->mSrc[0].mTemp == itemp &&	lblock->mInstructions[0]->mSrc[1].mTemp == mInstructions[0]->mDst.mTemp) ||
											 (lblock->mInstructions[0]->mSrc[1].mTemp == itemp && lblock->mInstructions[0]->mSrc[0].mTemp == mInstructions[0]->mDst.mTemp)) &&
											!IsTempReferencedInRange(2, sz, itemp) &&
											!IsTempReferencedInRange(2, sz, otemp) &&
											!lblock->IsTempReferencedInRange(1, ilz - 3, itemp) &&
											!lblock->IsTempReferencedInRange(1, ilz - 3, otemp) &&
											!eblock->IsTempReferencedInRange(0, esz - 3, otemp))
										{
											// Extend loop range
											int64 xcount = eblock->mInstructions[esz - 2]->mSrc->mIntConst * scale;

											eblock->mInstructions[esz - 2]->mSrc[0].mIntConst = xcount;
											eblock->mInstructions[esz - 2]->mSrc[1].mRange.mMaxValue = xcount;
											eblock->mInstructions[esz - 3]->mSrc[1].mRange.mMaxValue = xcount - 1;
											eblock->mInstructions[esz - 3]->mDst.mRange.mMaxValue = xcount;

											lblock->mInstructions[ilz - 1]->mCode = IC_JUMP; lblock->mInstructions[ilz - 1]->mNumOperands = 0;
											lblock->mTrueJump = eblock;
											lblock->mFalseJump = nullptr;
											lblock->mLoopHead = false;
											lblock->mNumEntries = 1;

											lblock->mInstructions[0]->mCode = IC_LOAD_TEMPORARY;
											lblock->mInstructions[0]->mNumOperands = 1;
											lblock->mInstructions[0]->mSrc[0] = eblock->mInstructions[esz - 3]->mSrc[1];

											mEntryValueRange[otemp].mMaxValue = xcount - 1;
											lblock->mEntryValueRange[otemp].mMaxValue = xcount - 1;
											eblock->mEntryValueRange[otemp].mMaxValue = xcount - 1;

											changed = true;
											printf("oopsie");
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->Flatten2DLoop())
			changed = true;
		if (mFalseJump && mFalseJump->Flatten2DLoop())
			changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::PostDecLoopOptimization(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && mFalseJump && (mTrueJump == this || mFalseJump == this) && mInstructions.Size() > 3)
		{
			int	nins = mInstructions.Size();

			InterCodeBasicBlock* pblock = mEntryBlocks[0], * eblock = mFalseJump;
			if (pblock == this)
				pblock = mEntryBlocks[1];
			if (eblock == this)
				eblock = mTrueJump;

			InterInstruction* movins = nullptr, * decins = nullptr;
			int		ctemp = -1;

			if (mInstructions[nins - 1]->mCode == IC_BRANCH && !pblock->mFalseJump)
			{
				ctemp = mInstructions[nins - 1]->mSrc->mTemp;
				if (mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[nins - 2]->mDst.mTemp == ctemp)
				{
					if (mInstructions[nins - 2]->mSrc[0].mTemp < 0)
						ctemp = mInstructions[nins - 2]->mSrc[1].mTemp;
					else
						ctemp = mInstructions[nins - 2]->mSrc[0].mTemp;
				}

				if (ctemp >= 0)
				{
					int movi = nins - 1;
					while (movi >= 0 && mInstructions[movi]->mDst.mTemp != ctemp)
						movi--;

					if (movi >= 0)
					{
						int ltemp = mInstructions[movi]->mSrc->mTemp;
						int inci = movi;
						while (inci < nins && mInstructions[inci]->mDst.mTemp != ltemp)
							inci++;
						if (inci < nins)
						{
							if (mInstructions[inci]->mCode == IC_BINARY_OPERATOR && mInstructions[inci]->mOperator == IA_ADD)
							{
								int inco = -1;
								if (mInstructions[inci]->mSrc[0].mTemp == ltemp && mInstructions[inci]->mSrc[1].mTemp < 0)
									inco = 1;
								else if (mInstructions[inci]->mSrc[1].mTemp == ltemp && mInstructions[inci]->mSrc[0].mTemp < 0)
									inco = 0;

								if (inco >= 0 && !IsTempReferencedInRange(0, movi, ltemp) && !IsTempReferencedInRange(movi + 1, inci, ltemp) && !IsTempReferencedInRange(inci + 1, nins, ltemp))
								{
									if (!eblock->mEntryRequiredTemps[ltemp])
									{
										InterInstruction* ins = mInstructions[inci];

										int64 v = ins->mSrc[inco].mIntConst;

										ins->mDst.mRange.AddConstValue(ins->mDst.mType, -v);
										ins->mSrc[1 - inco].mRange.AddConstValue(ins->mDst.mType, -v);
										mEntryValueRange[ltemp].AddConstValue(ins->mDst.mType, -v);

										mInstructions.Remove(inci);
										mInstructions.Insert(movi, ins);
										InterInstruction* pins = ins->Clone();
										pins->mSrc[inco].mIntConst = -v;

										pins->mDst.mRange.AddConstValue(ins->mDst.mType, -v);
										ins->mSrc[1 - inco].mRange.AddConstValue(ins->mDst.mType, -v);

										pblock->mInstructions.Insert(pblock->mInstructions.Size() - 1, pins);
										changed = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->PostDecLoopOptimization())
			changed = true;
		if (mFalseJump && mFalseJump->PostDecLoopOptimization())
			changed = true;
	}

	return changed;
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
				if ((ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE) && !ins->mConstExpr)
					hasCall = true;
			}

#if 1
			InterCodeBasicBlock* tailBlock = this == mTrueJump ? mFalseJump : mTrueJump;
			assert(tailBlock->mNumEntries == 1);

			// remove counting loop without body
			if (mInstructions.Size() == 3)
			{
				InterInstruction* ains = mInstructions[0];
				InterInstruction* cins = mInstructions[1];
				InterInstruction* bins = mInstructions[2];

				if (ains->mCode == IC_BINARY_OPERATOR && cins->mCode == IC_RELATIONAL_OPERATOR && bins->mCode == IC_BRANCH)
				{
					if (ains->mSrc[1].mTemp == ains->mDst.mTemp && ains->mSrc[0].mTemp < 0 &&
						cins->mSrc[1].mTemp == ains->mDst.mTemp && ains->mSrc[0].mTemp < 0 &&
						bins->mSrc[0].mTemp == cins->mDst.mTemp && bins->mSrc[0].mFinal)
					{
						if (ains->mOperator == IA_ADD && ains->mSrc[0].mIntConst == 1 &&
							cins->mOperator == IA_CMPLU && mTrueJump == this && !tailBlock->mEntryRequiredTemps[ains->mDst.mTemp])
						{
							cins->mCode = IC_CONSTANT;
							cins->mConst.mType = IT_BOOL;
							cins->mConst.mIntConst = 0;
							cins->mNumOperands = 0;
						}
					}
				}
				else if (ains->mCode == IC_LEA && cins->mCode == IC_RELATIONAL_OPERATOR && bins->mCode == IC_BRANCH)
				{
					if (ains->mSrc[1].mTemp == ains->mDst.mTemp && ains->mSrc[0].mTemp < 0 &&
						(cins->mSrc[1].mTemp == ains->mDst.mTemp || cins->mSrc[0].mTemp == ains->mDst.mTemp) &&
						bins->mSrc[0].mTemp == cins->mDst.mTemp && bins->mSrc[0].mFinal)
					{
						if (cins->mOperator == IA_CMPNE && mTrueJump == this && !tailBlock->mEntryRequiredTemps[ains->mDst.mTemp])
						{
							cins->mCode = IC_CONSTANT;
							cins->mConst.mType = IT_BOOL;
							cins->mConst.mIntConst = 0;
							cins->mNumOperands = 0;
						}
					}
				}
			}
#endif
#if 1
			if (!hasCall && (mProc->mCompilerOptions & COPT_OPTIMIZE_BASIC))
			{
				// Check forwarding globals

				int i = 0;
				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins = mInstructions[i];

					// A global load
					if (ins->mCode == IC_LOAD && !ins->mVolatile && ins->mSrc[0].mTemp < 0 && (ins->mSrc[0].mMemory == IM_GLOBAL || ins->mSrc[0].mMemory == IM_FPARAM))
					{
						// Find the last store that overlaps the load
						int	j = mInstructions.Size() - 1;
						while (j > i && !(mInstructions[j]->mCode == IC_STORE && CollidingMem(ins, mInstructions[j])))
							j--;

						if (j > i)
						{
							InterInstruction* sins = mInstructions[j];

							// Does a full store
							if (SameMem(ins->mSrc[0], sins->mSrc[1]) && !AliasingMem(this, ins, 0, mInstructions.Size()) && !DestroyingMem(this, ins, 0, j))
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
											InterInstruction* nins = new InterInstruction(sins->mLocation, IC_LOAD_TEMPORARY);
											mInstructions[i] = nins;
											nins->mDst.Forward(ins->mDst);
											nins->mSrc[0].Forward(sins->mSrc[0]);
											ins->mDst.Forward(sins->mSrc[0]);
											sins->mSrc[0].mFinal = false;
											assert(nins->mSrc[0].mTemp >= 0);

											// Propagate all loads to move temps

											for (int t = i + 1; t < mInstructions.Size(); t++)
											{
												InterInstruction* ti = mInstructions[t];
												if (ti->mCode == IC_LOAD && SameMem(ti->mSrc[0], ins->mSrc[0]))
												{
													ti->mCode = IC_LOAD_TEMPORARY;
													ti->mSrc[0] = ins->mDst;
												}
											}

											// Move store behind loop
											if (sins->mSrc[0].mTemp >= 0)
												tailBlock->mEntryRequiredTemps += sins->mSrc[0].mTemp;
											tailBlock->mInstructions.Insert(0, sins);
											mInstructions.Remove(j);
										}
									}
								}
							}
						}
					}
					else if (ins->mCode == IC_STORE && !ins->mVolatile && ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp < 0 && (ins->mSrc[1].mMemory == IM_GLOBAL || ins->mSrc[1].mMemory == IM_FPARAM))
					{
						int j = 0;
						while (j < mInstructions.Size() && (j == i || !CollidingMem(ins, mInstructions[j])))
							j++;

						if (j == mInstructions.Size())
						{
							tailBlock->mInstructions.Insert(0, ins);
							mInstructions.Remove(i);
							i--;
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
					if ((ins->mSrc[0].mTemp >= 0 && mLocalModifiedTemps[ins->mSrc[0].mTemp]) || ins->mVolatile || hasCall)
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
							if (sins->mCode == IC_STORE && CollidingMem(ins, sins))
							{
								if (sins->mSrc[1].mTemp >= 0 && ins->mSrc[0].mTemp < 0)
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
								else
								{
									ins->mInvariant = false;
								}
							}
							else if ((sins->mCode == IC_COPY || sins->mCode == IC_FILL) && DestroyingMem(ins, sins))
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
						if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp == t)// && ins->mSrc[0].mIntConst > 0)
						{
							indexStep[t] = ins->mSrc[0].mIntConst;
							indexBase[t] = 0;
							dep[t] = DEP_INDEX;
							ins->mInvariant = false;
						}
						else
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
							if (ins1->mSrc[0].mTemp == ins0->mDst.mTemp && ins1->mSrc[0].mFinal && ins0->mSrc[0].mTemp >= 0 && ins0->mSrc[1].mTemp >= 0)
							{
								if (dep[ins1->mSrc[1].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[0].mTemp] == DEP_UNKNOWN && dep[ins0->mSrc[1].mTemp] == DEP_INDEX_EXTENDED)
								{
									InterOperand	iop = ins0->mSrc[1];
									InterOperand	pop = ins1->mSrc[1];

									ins0->mCode = IC_LEA;
									ins0->mDst.mType = IT_POINTER;
									ins0->mDst.mMemory = ins1->mSrc[1].mMemory;

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
#if 0
							if (tailBlock->mEntryRequiredTemps[ins->mDst.mTemp])
							{
								dep[t] = DEP_VARIABLE;
								ins->mInvariant = false;
								changed = true;
							}
							else
#endif
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
										ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SUB && IsIntegerType(ins->mDst.mType) && (ins->mSrc[0].mTemp < 0 || dep[ins->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[0].mTemp] == DEP_DEFINED) && dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED ||
										ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SUB && IsIntegerType(ins->mDst.mType) && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && (dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED || dep[ins->mSrc[0].mTemp] == DEP_INDEX) ||
										ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
										IsIntegerType(ins->mDst.mType) &&
										(ins->mSrc[0].mTemp >= 0 && ins->mSrc[0].IsNotUByte() && (dep[ins->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[0].mTemp] == DEP_DEFINED)) &&
										(dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED) ||
										ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD &&
										IsIntegerType(ins->mDst.mType) &&
										(ins->mSrc[1].mTemp >= 0 && ins->mSrc[1].IsNotUByte() && (dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED)) &&
										(dep[ins->mSrc[0].mTemp] == DEP_INDEX || dep[ins->mSrc[0].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED) ||
										ins->mCode == IC_LEA && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED)
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
				}

			} while (changed);

			GrowingArray<InterInstructionPtr>	indexins(nullptr);
			GrowingArray<InterInstructionPtr>	pindexins(nullptr);


			InterInstruction* cins = nullptr;
			if (mInstructions.Size() > 2 && mInstructions[mInstructions.Size() - 2]->mCode == IC_RELATIONAL_OPERATOR)
				cins = mInstructions[mInstructions.Size() - 2];

			int	j = 0;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];

				if (ins->mInvariant)
				{
					if (ins->mDst.mTemp >= 0 && IsTempReferencedInRange(0, i, ins->mDst.mTemp))
						ins->mInvariant = false;
					else
					{
						for (int j = 0; j < ins->mNumOperands; j++)
							if (ins->mSrc[j].mTemp >= 0 && dep[ins->mSrc[j].mTemp] == DEP_VARIABLE)
								ins->mInvariant = false;
					}
				}
				else
				{
					if (ins->mCode == IC_BINARY_OPERATOR && (ins->mOperator == IA_AND || ins->mOperator == IA_OR) && !IsTempReferencedInRange(0, i, ins->mDst.mTemp))
					{
						if (ins->mDst.mTemp == ins->mSrc[0].mTemp && ins->mSrc[1].mTemp < 0)
						{
							if (!IsTempModifiedInRange(i + 1, mInstructions.Size(), ins->mDst.mTemp))
								ins->mInvariant = true;
						}
						else if (ins->mDst.mTemp == ins->mSrc[1].mTemp && ins->mSrc[0].mTemp < 0)
						{
							if (!IsTempModifiedInRange(i + 1, mInstructions.Size(), ins->mDst.mTemp))
								ins->mInvariant = true;
						}
					}
				}

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

						InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						bins->mOperator = IA_MUL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[1] = ins->mSrc[1]; ains->mSrc[1].mIntConst = ins->mSrc[1].mIntConst * indexBase[ins->mSrc[0].mTemp];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[1].mIntConst = ins->mSrc[1].mIntConst * indexStep[ins->mSrc[0].mTemp];
						ins->mSrc[0] = ins->mDst;

						if (tailBlock->mEntryRequiredTemps[ins->mDst.mTemp] && !(cins && (cins->mSrc[0].mTemp == ins->mDst.mTemp || cins->mSrc[1].mTemp == ins->mDst.mTemp)))
						{
							InterInstruction* rins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							rins->mOperator = IA_SUB;
							rins->mDst = ins->mDst;
							rins->mSrc[1] = ins->mDst;
							rins->mSrc[0] = ins->mSrc[1];
							tailBlock->mInstructions.Insert(0, rins);
						}

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MUL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = ins->mSrc[0].mIntConst * indexStep[ins->mSrc[1].mTemp];
						indexBase[ins->mDst.mTemp] = ins->mSrc[0].mIntConst * indexBase[ins->mSrc[1].mTemp];

						InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						bins->mOperator = IA_MUL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mSrc[0]; ains->mSrc[0].mIntConst = ins->mSrc[0].mIntConst * indexBase[ins->mSrc[1].mTemp];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mIntConst = ins->mSrc[0].mIntConst * indexStep[ins->mSrc[1].mTemp];
						ins->mSrc[1] = ins->mDst;

						if (tailBlock->mEntryRequiredTemps[ins->mDst.mTemp] && !(cins && (cins->mSrc[0].mTemp == ins->mDst.mTemp || cins->mSrc[1].mTemp == ins->mDst.mTemp)))
						{
							InterInstruction* rins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							rins->mOperator = IA_SUB;
							rins->mDst = ins->mDst;
							rins->mSrc[1] = ins->mDst;
							rins->mSrc[0] = ins->mSrc[0];
							tailBlock->mInstructions.Insert(0, rins);
						}

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SHL && IsIntegerType(ins->mDst.mType) && ins->mSrc[0].mTemp < 0 && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						indexBase[ins->mDst.mTemp] = indexBase[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;

						InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						bins->mOperator = IA_SHL;
						bins->mDst = ins->mDst;
						bins->mSrc[0] = ins->mSrc[0];
						bins->mSrc[1] = ins->mSrc[1];
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mSrc[0]; ains->mSrc[0].mIntConst = indexBase[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ains);

						ins->mOperator = IA_ADD;
						ins->mSrc[0].mIntConst = indexStep[ins->mSrc[1].mTemp] << ins->mSrc[0].mIntConst;
						ins->mSrc[1] = ins->mDst;
						if (ins->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND)
							ins->mDst.mRange.mMaxValue += ins->mSrc[0].mIntConst;

						if (tailBlock->mEntryRequiredTemps[ins->mDst.mTemp] && !(cins && (cins->mSrc[0].mTemp == ins->mDst.mTemp || cins->mSrc[1].mTemp == ins->mDst.mTemp)))
						{
							InterInstruction* rins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							rins->mOperator = IA_SUB;
							rins->mDst = ins->mDst;
							rins->mSrc[1] = ins->mDst;
							rins->mSrc[0] = ins->mSrc[0];
							tailBlock->mInstructions.Insert(0, rins);
						}

						indexins.Push(ins);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && (ins->mOperator == IA_ADD || ins->mOperator == IA_SUB) && IsIntegerType(ins->mDst.mType) && (ins->mSrc[0].mTemp < 0 || dep[ins->mSrc[0].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[0].mTemp] == DEP_DEFINED) && (dep[ins->mSrc[1].mTemp] == DEP_INDEX || dep[ins->mSrc[1].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[1].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[1].mTemp];
						indexBase[ins->mDst.mTemp] = 0;

						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
						if (indexBase[ins->mSrc[1].mTemp])
						{
							InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							bins->mOperator = IA_ADD;
							bins->mDst = ins->mDst;
							bins->mSrc[0] = ins->mDst;
							bins->mSrc[1].mType = ins->mDst.mType;
							bins->mSrc[1].mTemp = -1;
							bins->mSrc[1].mIntConst = indexBase[ins->mSrc[1].mTemp];
							mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);
						}

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[1].mTemp = -1;
						ains->mSrc[1].mIntConst = indexStep[ins->mSrc[1].mTemp];
						if (ains->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND)
							ains->mDst.mRange.mMaxValue += ains->mSrc[1].mIntConst;

						indexins.Push(ains);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && IsIntegerType(ins->mDst.mType) && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && (dep[ins->mSrc[0].mTemp] == DEP_INDEX || dep[ins->mSrc[0].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = indexStep[ins->mSrc[0].mTemp];
						indexBase[ins->mDst.mTemp] = 0;

						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
						if (indexBase[ins->mSrc[0].mTemp])
						{
							InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							bins->mOperator = IA_ADD;
							bins->mDst = ins->mDst;
							bins->mSrc[0] = ins->mDst;
							bins->mSrc[1].mType = ins->mDst.mType;
							bins->mSrc[1].mTemp = -1;
							bins->mSrc[1].mIntConst = indexBase[ins->mSrc[0].mTemp];
							mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);
						}

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_ADD;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[0].mTemp = -1;
						ains->mSrc[0].mIntConst = indexStep[ins->mSrc[0].mTemp];

						if (ains->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND)
							ains->mDst.mRange.mMaxValue += ains->mSrc[0].mIntConst;
						indexins.Push(ains);
					}
					else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_SUB && IsIntegerType(ins->mDst.mType) && (ins->mSrc[1].mTemp < 0 || dep[ins->mSrc[1].mTemp] == DEP_UNKNOWN || dep[ins->mSrc[1].mTemp] == DEP_DEFINED) && (dep[ins->mSrc[0].mTemp] == DEP_INDEX || dep[ins->mSrc[0].mTemp] == DEP_INDEX_EXTENDED || dep[ins->mSrc[0].mTemp] == DEP_INDEX_DERIVED))
					{
						indexStep[ins->mDst.mTemp] = -indexStep[ins->mSrc[0].mTemp];
						indexBase[ins->mDst.mTemp] = 0;

						mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, ins);
						if (indexBase[ins->mSrc[0].mTemp])
						{
							InterInstruction* bins = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
							bins->mOperator = IA_SUB;
							bins->mDst = ins->mDst;
							bins->mSrc[1] = ins->mDst;
							bins->mSrc[0].mType = ins->mDst.mType;
							bins->mSrc[0].mTemp = -1;
							bins->mSrc[0].mIntConst = indexBase[ins->mSrc[0].mTemp];
							mLoopPrefix->mInstructions.Insert(mLoopPrefix->mInstructions.Size() - 1, bins);
						}

						InterInstruction* ains = new InterInstruction(ins->mLocation, IC_BINARY_OPERATOR);
						ains->mOperator = IA_SUB;
						ains->mDst = ins->mDst;
						ains->mSrc[1] = ins->mDst;
						ains->mSrc[0] = ins->mDst;
						ains->mSrc[0].mTemp = -1;
						ains->mSrc[0].mIntConst = indexStep[ins->mSrc[0].mTemp];

						if (ains->mDst.mRange.mMinState == IntegerValueRange::S_BOUND)
							ains->mDst.mRange.mMinValue -= ains->mSrc[0].mIntConst;
						indexins.Push(ains);
					}
					else if (ins->mCode == IC_LEA && ins->mSrc[1].mTemp < 0 && ins->mSrc[0].IsUByte() && (ins->mSrc[1].mMemory == IM_ABSOLUTE || ins->mSrc[1].mMemory == IM_GLOBAL))
					{
						mInstructions[j++] = ins;
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
							ins->mSrc[1] = pindexins[k]->mDst;
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

							InterInstruction* ains = new InterInstruction(ins->mLocation, IC_LEA);
							ains->mDst = ins->mDst;
							ains->mSrc[1] = ins->mDst;
							ains->mSrc[1].mMemory = IM_INDIRECT;
							ains->mSrc[1].mIntConst = 0;
							ains->mSrc[0].mType = IT_INT16;
							ains->mSrc[0].mTemp = -1;
							ains->mSrc[0].mIntConst = indexStep[ins->mSrc[0].mTemp];
							if (ains->mDst.mRange.mMaxState == IntegerValueRange::S_BOUND)
								ains->mDst.mRange.mMaxValue += ains->mSrc[0].mIntConst;

							if (tailBlock->mEntryRequiredTemps[ains->mDst.mTemp])
							{
								InterInstruction* dins = new InterInstruction(ins->mLocation, IC_LEA);
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

					InterInstruction* ains = new InterInstruction(ins->mLocation, ins->mCode);
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
								toffset += int(cins->mSrc[0].mIntConst);
							else if (cins->mCode == IC_BINARY_OPERATOR && cins->mOperator == IA_ADD && cins->mSrc[1].mTemp == st && cins->mSrc[0].mTemp < 0)
								toffset += int(cins->mSrc[0].mIntConst);
							else
								break;						
						}
						else
						{
							int k = 0;
							while (k < cins->mNumOperands && cins->mSrc[k].mTemp != dt)
								k++;

							if (k != cins->mNumOperands)
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
								ins->mSrc[1].mMemory = IM_INDIRECT;
								ins->mDst.mMemory = IM_INDIRECT;
								ins->mSrc[0].mTemp = -1;
								ins->mSrc[0].mType = IT_INT16;
								ins->mSrc[0].mIntConst = -toffset;
							}
							else
							{
								ins->mCode = IC_BINARY_OPERATOR;
								ins->mOperator = IA_ADD;
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

static bool IsSimilarLea(InterInstruction* ins0, InterInstruction* ins1)
{
	if (ins0->mCode == IC_LEA && ins1->mCode == IC_LEA &&
		ins0->mSrc[0].mTemp == ins1->mSrc[0].mTemp &&
		ins0->mSrc[1].mTemp == ins1->mSrc[1].mTemp &&
		ins0->mSrc[0].IsUByte() && ins1->mSrc[0].IsUByte())
	{
		if (ins0->mSrc[1].mTemp >= 0 && ins0->mSrc[1].mFinal && ins1->mSrc[1].mFinal ||
			ins0->mSrc[1].mMemory == ins1->mSrc[1].mMemory &&
			ins0->mSrc[1].mVarIndex == ins1->mSrc[1].mVarIndex &&
			ins0->mSrc[1].mLinkerObject == ins1->mSrc[1].mLinkerObject)
		{
			if (ins0->mSrc[1].mIntConst < ins1->mSrc[1].mIntConst)
			{
				if (ins1->mSrc[0].mRange.mMaxValue + ins1->mSrc[1].mIntConst - ins0->mSrc[1].mIntConst < 128)
					return true;
			}
			else if (ins0->mSrc[1].mIntConst > ins1->mSrc[1].mIntConst)
			{
				if (ins0->mSrc[0].mRange.mMaxValue + ins0->mSrc[1].mIntConst - ins1->mSrc[1].mIntConst < 128)
					return true;
			}
			else
				return true;
		}
	}

	return false;
}

bool InterCodeBasicBlock::ShortLeaMerge(int& spareTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (!mLoopHead && mEntryBlocks.Size() > 1)
		{
			int k = 0;
			while (k < mEntryBlocks.Size() && !mEntryBlocks[k]->mFalseJump)
				k++;

			if (k == mEntryBlocks.Size())
			{
				GrowingInstructionArray	iins(nullptr);

				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins = mInstructions[i];

					int	ttemp = -1;
					if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp >= 0)
						ttemp = ins->mSrc[1].mTemp;
					else if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp >= 0)
						ttemp = ins->mSrc[0].mTemp;

					if (ttemp >= 0 && !IsTempModifiedInRange(0, i, ttemp))
					{
						bool	found = true;

						iins.SetSize(0);
						for (int k = 0; k < mEntryBlocks.Size(); k++)
						{
							InterInstruction* sins = mEntryBlocks[k]->FindTempOrigin(ttemp);
							if (sins && sins->mCode == IC_LEA && mEntryBlocks[k]->CanMoveInstructionBehindBlock(mEntryBlocks[k]->mMark))
								iins.Push(sins);
							else
							{
								found = false;
								break;
							}
						}

						if (found)
						{
							int64	minint = iins[0]->mSrc[1].mIntConst;

							for (int k = 1; k < mEntryBlocks.Size(); k++)
							{
								if (IsSimilarLea(iins[0], iins[k]))
								{
									if (iins[k]->mSrc[1].mIntConst < minint)
										minint = iins[k]->mSrc[1].mIntConst;
								}
								else
								{
									found = false;
									break;
								}
							}

							if (found)
							{
								InterInstruction* lins = iins[0]->Clone();
								lins->mSrc[0].mRange.Reset();
								lins->mSrc[1].mIntConst = minint;
								mInstructions.Insert(0, lins);

								if (iins[0]->mSrc[0].mTemp < 0)
								{
									if (spareTemps + 2 >= mEntryRequiredTemps.Size() + 16)
										return true;
									int ttemp = spareTemps++;
									mInstructions[0]->mSrc[0].mTemp = ttemp;
									mInstructions[0]->mSrc[0].mType = IT_INT16;

									for (int k = 0; k < mEntryBlocks.Size(); k++)
									{
										InterInstruction* sins = iins[k];
										sins->mCode = IC_CONSTANT;
										sins->mDst = mInstructions[0]->mSrc[0];
										sins->mConst = sins->mSrc[0];
										sins->mConst.mIntConst -= minint;
										sins->mNumOperands = 0;
									}
								}
								else
								{
									for (int k = 0; k < mEntryBlocks.Size(); k++)
									{
										InterInstruction* sins = iins[k];
										sins->mCode = IC_BINARY_OPERATOR;
										sins->mOperator = IA_ADD;
										sins->mDst = sins->mSrc[0];
										sins->mSrc[1].mTemp = -1;
										sins->mSrc[1].mType = sins->mSrc[0].mType;
										sins->mSrc[1].mIntConst -= minint;
										sins->mDst.mRange.mMaxValue += sins->mSrc[1].mIntConst;
										mEntryBlocks[k]->mExitRequiredTemps += sins->mDst.mTemp;
									}
								}

								changed = true;
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->ShortLeaMerge(spareTemps)) changed = true;
		if (mFalseJump && mFalseJump->ShortLeaMerge(spareTemps)) changed = true;
	}

	return changed;
}

bool InterCodeBasicBlock::ShortLeaCleanup(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (!mLoopHead && mEntryBlocks.Size() > 1)
		{
			int k = 0;
			while (k < mEntryBlocks.Size() && !mEntryBlocks[k]->mFalseJump)
				k++;

			if (k == mEntryBlocks.Size())
			{
				GrowingInstructionArray	iins(nullptr);

				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins = mInstructions[i];

					int	ttemp = -1;
					if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp >= 0)
						ttemp = ins->mSrc[1].mTemp;
					else if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp >= 0)
						ttemp = ins->mSrc[0].mTemp;

					if (ttemp >= 0 && !IsTempModifiedInRange(0, i, ttemp))
					{
						bool	found = true;
						bool	shortlea = false, noshortlea = false;

						iins.SetSize(0);
						for (int k = 0; k < mEntryBlocks.Size(); k++)
						{
							InterInstruction* sins = mEntryBlocks[k]->FindTempOrigin(ttemp);
							if (ins->mCode == IC_STORE && sins && sins->mCode == IC_LEA && sins->mSrc[1].mTemp < 0 && sins->mSrc[0].IsUByte() && CanMoveInstructionBeforeBlock(i) && !mEntryBlocks[k]->mFalseJump)
								shortlea = true;
							else
								noshortlea = true;
						}

						if (shortlea && !noshortlea)
						{
							for (int k = 0; k < mEntryBlocks.Size(); k++)
							{
								mEntryBlocks[k]->AppendBeforeBranch(ins->Clone());
								if (ins->mDst.mTemp >= 0)
									mEntryBlocks[k]->mExitRequiredTemps += ins->mDst.mTemp;
							}
							if (ins->mDst.mTemp >= 0)
								mEntryRequiredTemps += ins->mDst.mTemp;
							ins->mCode = IC_NONE; ins->mNumOperands = 0; ins->mDst.mTemp = -1;
							changed = true;
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->ShortLeaCleanup()) changed = true;
		if (mFalseJump && mFalseJump->ShortLeaCleanup()) changed = true;
	}

	return changed;
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

void InterCodeBasicBlock::CheckFinalLocal(void) 
{
#if _DEBUG
	NumberSet	required(mExitRequiredTemps);

	for (int i = mInstructions.Size() - 1; i >= 0; i--)
	{
		const InterInstruction* ins(mInstructions[i]);
		if (ins->mCode == IC_NONE)
			assert(ins->mDst.mTemp < 0 && ins->mNumOperands == 0);

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

	for (int i = 0; i < mInstructions.Size(); i++)
	{
		const InterInstruction* ins(mInstructions[i]);
		for (int j = 0; j < ins->mNumOperands; j++)
		{
			if (ins->mSrc[j].mTemp >= 0 && !provided[ins->mSrc[j].mTemp])
			{
				//				printf("Use of potentially undefined temp %d\n", ins->mSrc[j].mTemp);
			}
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

void InterCodeBasicBlock::CheckBlocks(void)
{
#if _DEBUG
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
			assert(mInstructions[i] != nullptr);

		assert(!mTrueJump || mTrueJump->mIndex > 0);
		assert(!mFalseJump || mFalseJump->mIndex > 0);

		if (mTrueJump) mTrueJump->CheckBlocks();
		if (mFalseJump) mFalseJump->CheckBlocks();
	}
#endif
}

bool InterCodeBasicBlock::PeepholeReplaceOptimization(const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs)
{
	int	j = 0;
	for (int i = 0; i < mInstructions.Size(); i++)
	{
		if (mInstructions[i]->mCode != IC_NONE)
		{
			mInstructions[j++] = mInstructions[i];
		}
	}
	mInstructions.SetSize(j);

	bool changed = false;

	for (int i = 0; i < mInstructions.Size(); i++)
	{
		if (mInstructions[i]->mCode == IC_LOAD_TEMPORARY && mInstructions[i]->mDst.mTemp == mInstructions[i]->mSrc[0].mTemp)
		{
			mInstructions[i]->mCode = IC_NONE;
			mInstructions[i]->mNumOperands = 0;
			mInstructions[i]->mDst.mTemp = -1;
			changed = true;
		}
		if (mInstructions[i]->mCode == IC_LOAD && mInstructions[i]->mSrc[0].mMemory == IM_GLOBAL && (mInstructions[i]->mSrc[0].mLinkerObject->mFlags & LOBJF_CONST))
		{
			LoadConstantFold(mInstructions[i], nullptr, staticVars, staticProcs);
			changed = true;
		}

		if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mOperator == IA_AND && mInstructions[i]->mSrc[1].mTemp < 0 && mInstructions[i]->mSrc[0].IsUnsigned())
		{
			mInstructions[i]->mSrc[1].mIntConst &= BinMask(mInstructions[i]->mSrc[0].mRange.mMaxValue);
			if (mInstructions[i]->mSrc[1].mIntConst == BinMask(mInstructions[i]->mSrc[1].mIntConst) && mInstructions[i]->mSrc[0].mRange.mMaxValue <= mInstructions[i]->mSrc[1].mIntConst)
			{
				mInstructions[i]->mCode = IC_LOAD_TEMPORARY;
				mInstructions[i]->mNumOperands = 1;
				changed = true;
			}
				
		}
		if (mInstructions[i]->mCode == IC_BINARY_OPERATOR && mInstructions[i]->mOperator == IA_AND && mInstructions[i]->mSrc[0].mTemp < 0 && mInstructions[i]->mSrc[1].IsUnsigned())
		{
			mInstructions[i]->mSrc[0].mIntConst &= BinMask(mInstructions[i]->mSrc[1].mRange.mMaxValue);
			if (mInstructions[i]->mSrc[0].mIntConst == BinMask(mInstructions[i]->mSrc[0].mIntConst) && mInstructions[i]->mSrc[1].mRange.mMaxValue <= mInstructions[i]->mSrc[0].mIntConst)
			{
				mInstructions[i]->mCode = IC_LOAD_TEMPORARY;
				mInstructions[i]->mSrc[0] = mInstructions[i]->mSrc[1];
				mInstructions[i]->mNumOperands = 1;
				changed = true;
			}
		}

		if (i + 1 < mInstructions.Size())
		{
#if 1
			if (mInstructions[i + 0]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[i + 1]->mCode == IC_BRANCH && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				(mInstructions[i + 0]->mOperator == IA_CMPEQ || mInstructions[i + 0]->mOperator == IA_CMPNE) &&
				(mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mType == IT_INT8 && mInstructions[i + 0]->mSrc[0].mIntConst == 0 ||
					mInstructions[i + 0]->mSrc[1].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mType == IT_INT8 && mInstructions[i + 0]->mSrc[1].mIntConst == 0))
			{
				if (mInstructions[i + 0]->mSrc[0].mTemp < 0)
					mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
				else
					mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[0];
				if (mInstructions[i + 0]->mOperator == IA_CMPEQ)
				{
					InterCodeBasicBlock* b = mTrueJump; mTrueJump = mFalseJump; mFalseJump = b;
				}
				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
#endif

			if (mInstructions[i + 0]->mCode == IC_LOAD && mInstructions[i + 0]->mSrc[0].mTemp >= 0 && mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[0].mTemp &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mDst.mTemp == mInstructions[i + 0]->mSrc[0].mTemp && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mSrc[0].mTemp &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mIntConst == mInstructions[i + 1]->mSrc[0].mIntConst)
			{
				InterInstruction* ins(mInstructions[i + 0]);
				mInstructions[i + 0] = mInstructions[i + 1];
				mInstructions[i + 1] = ins;
				mInstructions[i + 1]->mSrc[0].mIntConst = 0;
				changed = true;
			}

			if (mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mTemp >= 0 && mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[1].mTemp &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mSrc[1].mTemp && mInstructions[i + 1]->mDst.mTemp != mInstructions[i + 1]->mSrc[1].mTemp &&
				mInstructions[i + 0]->mSrc[0].mIntConst > mInstructions[i + 1]->mSrc[0].mIntConst &&
				!mInstructions[i + 2]->ReferencesTemp(mInstructions[i + 1]->mDst.mTemp))
			{
				SwapInstructions(mInstructions[i + 0], mInstructions[i + 1]);
				InterInstruction* ins(mInstructions[i + 0]);
				mInstructions[i + 0] = mInstructions[i + 1];
				mInstructions[i + 1] = ins;
			}
		}

		if (mInstructions[i]->mCode == IC_LEA && mInstructions[i]->mSrc[1].mFinal && mInstructions[i]->mSrc[0].mTemp < 0)
		{
			int k = i;
			while (k + 1 < mInstructions.Size() && 
				mInstructions[k + 1]->mCode == IC_STORE && mInstructions[k + 1]->mSrc[1].mTemp == mInstructions[i]->mDst.mTemp && mInstructions[k + 1]->mSrc[0].mTemp != mInstructions[i]->mDst.mTemp &&
				mInstructions[k + 1]->mSrc[1].mIntConst + mInstructions[i]->mSrc[0].mIntConst < 128)
				k++;
			if (k > i && mInstructions[k]->mSrc[1].mFinal)
			{
				while (k > i)
				{
					mInstructions[k]->mSrc[1].mRange = mInstructions[i]->mSrc[1].mRange;
					mInstructions[k]->mSrc[1].mTemp = mInstructions[i]->mSrc[1].mTemp;
					mInstructions[k]->mSrc[1].mIntConst += mInstructions[i]->mSrc[0].mIntConst;
					k--;
				}
				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}

		}

		if (i + 2 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_NONE)
			{
				// just skip it
			}
			else if (mInstructions[i + 0]->mCode == IC_LOAD &&
				mInstructions[i + 1]->mCode == IC_LOAD &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mSrc[0].mTemp &&
				mInstructions[i + 0]->mSrc[0].mIntConst > mInstructions[i + 1]->mSrc[0].mIntConst &&
				mInstructions[i + 1]->mSrc[0].mTemp != mInstructions[i + 0]->mDst.mTemp)
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
				mInstructions[i + 2]->mSrc[0].mTemp = mInstructions[i + 1]->mDst.mTemp;
				mInstructions[i + 2]->mSrc[0].mFinal = false;
				if (mInstructions[i + 2]->mSrc[1].mTemp == t)
				{
					mInstructions[i + 2]->mSrc[1].mTemp = mInstructions[i + 1]->mDst.mTemp;
					mInstructions[i + 2]->mSrc[1].mFinal = false;
				}
				mInstructions[i + 1]->mCode = IC_NONE;
				mInstructions[i + 1]->mDst.mTemp = -1;
				mInstructions[i + 1]->mNumOperands = 0;
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
				mInstructions[i + 2]->mSrc[1].mTemp = mInstructions[i + 1]->mDst.mTemp;
				mInstructions[i + 2]->mSrc[1].mFinal = false;
				mInstructions[i + 1]->mCode = IC_NONE;
				mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && 
				mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mIntConst >= 0 &&
				mInstructions[i + 1]->mCode == IC_TYPECAST && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				mInstructions[i + 1]->mDst.mType == IT_POINTER && mInstructions[i + 1]->mSrc[0].mType == IT_INT16)
			{
				int64	addr = mInstructions[i + 0]->mSrc[0].mIntConst;

				mInstructions[i + 0]->mCode = IC_LEA;
				mInstructions[i + 0]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 0]->mSrc[1].mTemp = -1;
				mInstructions[i + 0]->mSrc[1].mType = IT_POINTER;
				mInstructions[i + 0]->mSrc[1].mMemory = IM_ABSOLUTE;
				mInstructions[i + 0]->mSrc[1].mIntConst = addr;
				mInstructions[i + 0]->mSrc[1].mLinkerObject = nullptr;
				mInstructions[i + 0]->mSrc[1].mVarIndex = -1;
				mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
		
				mInstructions[i + 1]->mCode = IC_NONE;
				mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				(mInstructions[i + 1]->mSrc[0].mIntConst & ((1LL << mInstructions[i + 0]->mSrc[0].mIntConst) - 1)) == 0)
			{
				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[0].mIntConst >>= shift;
				mInstructions[i + 0]->mOperator = IA_AND;
				mInstructions[i + 0]->mSrc[0].mIntConst = ~((1LL << shift) - 1);
				mInstructions[i + 0]->mDst.mRange.Reset();
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				(mInstructions[i + 1]->mSrc[1].mIntConst & ((1LL << mInstructions[i + 0]->mSrc[0].mIntConst) - 1)) == 0)
			{
				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[1].mIntConst >>= shift;
				mInstructions[i + 0]->mOperator = IA_AND;
				mInstructions[i + 0]->mSrc[0].mIntConst = ~((1LL << shift) - 1);
				mInstructions[i + 0]->mDst.mRange.Reset();
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
				mInstructions[i + 0]->mDst.mTemp = -1;
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
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHL && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_ADD && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 0]->mSrc[1].IsUByte() && !mInstructions[i + 1]->mSrc[1].IsUByte() &&
				mInstructions[i + 0]->mSrc[1].mRange.mMaxValue + (mInstructions[i + 1]->mSrc[0].mIntConst >> mInstructions[i + 0]->mSrc[0].mIntConst) < 256 &&
				(mInstructions[i + 1]->mSrc[0].mIntConst & ((1 << mInstructions[i + 0]->mSrc[0].mIntConst) - 1)) == 0)
			{
				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst, add = mInstructions[i + 1]->mSrc[0].mIntConst;

				mInstructions[i + 0]->mOperator = IA_ADD; mInstructions[i + 0]->mSrc[0].mIntConst = add >> shift;
				mInstructions[i + 1]->mOperator = IA_SHL; mInstructions[i + 1]->mSrc[0].mIntConst = shift;
				mInstructions[i + 0]->mDst.mRange = mInstructions[i + 0]->mSrc[1].mRange;
				mInstructions[i + 0]->mDst.mRange.AddConstValue(mInstructions[i + 0]->mDst.mType, add >> shift);
				mInstructions[i + 1]->mSrc[1].mRange = mInstructions[i + 0]->mDst.mRange;
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
				mInstructions[i + 1]->mDst.mTemp = -1;
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
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 0]->mOperator == IA_SUB || mInstructions[i + 0]->mOperator == IA_XOR) &&
				mInstructions[i + 0]->mSrc[0].mTemp >= 0 && mInstructions[i + 0]->mSrc[0].mTemp == mInstructions[i + 0]->mSrc[1].mTemp)
			{
				mInstructions[i + 0]->mCode = IC_CONSTANT;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mConst.mType = mInstructions[i + 0]->mDst.mType;
				mInstructions[i + 0]->mConst.mIntConst = 0;
				mInstructions[i + 0]->mConst.mFloatConst = 0;
				changed = true;
			}
#endif
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_AND && mInstructions[i + 0]->mSrc[0].mTemp < 0 && ispow2(mInstructions[i + 0]->mSrc[0].mIntConst) &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && (mInstructions[i + 1]->mOperator == IA_CMPEQ || mInstructions[i + 1]->mOperator == IA_CMPNE) && mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[0].mIntConst == mInstructions[i + 1]->mSrc[0].mIntConst &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp)
			{
				mInstructions[i + 1]->mSrc[0].mIntConst = 0;
				mInstructions[i + 1]->mOperator = InvertRelational(mInstructions[i + 1]->mOperator);
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_AND && mInstructions[i + 0]->mSrc[1].mTemp < 0 && ispow2(mInstructions[i + 0]->mSrc[1].mIntConst) &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && (mInstructions[i + 1]->mOperator == IA_CMPEQ || mInstructions[i + 1]->mOperator == IA_CMPNE) && mInstructions[i + 1]->mSrc[1].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mIntConst == mInstructions[i + 1]->mSrc[1].mIntConst &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp)
			{
				mInstructions[i + 1]->mSrc[1].mIntConst = 0;
				mInstructions[i + 1]->mOperator = InvertRelational(mInstructions[i + 1]->mOperator);
				changed = true;
			}

#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_MUL && mInstructions[i + 2]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&
				(mInstructions[i + 2]->mSrc[0].mIntConst & 1) == 0)
			{

				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				int64	mshift = 1;
				while (!(mInstructions[i + 2]->mSrc[0].mIntConst & (1ULL << mshift)))
					mshift++;

				mInstructions[i + 1]->mCode = IC_BINARY_OPERATOR;
				mInstructions[i + 1]->mOperator = IA_AND;
				mInstructions[i + 1]->mNumOperands = 2;
				mInstructions[i + 1]->mSrc[0].mType = IT_INT16;
				mInstructions[i + 1]->mSrc[1].mType = IT_INT16;
				mInstructions[i + 1]->mSrc[1].mTemp = -1;
				mInstructions[i + 1]->mSrc[1].mIntConst = 255;

				if (mshift < shift)
				{
					mInstructions[i + 0]->mDst.mRange.mMaxValue <<= mshift;
					mInstructions[i + 0]->mSrc[0].mIntConst = shift - mshift;

					mInstructions[i + 1]->mSrc[0].mRange.mMaxValue = mInstructions[i + 0]->mDst.mRange.mMaxValue;
					mInstructions[i + 1]->mDst.mRange.mMaxValue = mInstructions[i + 0]->mDst.mRange.mMaxValue;
					mInstructions[i + 1]->mSrc[1].mIntConst = 255ULL >> shift << mshift;
					mInstructions[i + 2]->mSrc[1].mRange.mMaxValue = mInstructions[i + 0]->mDst.mRange.mMaxValue;
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
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
			{

				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				if (shift & 7)
				{
					int64	mshift = mInstructions[i + 1]->mSrc[0].mIntConst;

					mInstructions[i + 0]->mOperator = IA_AND;
					mInstructions[i + 0]->mSrc[0].mType = mInstructions[i + 0]->mSrc[1].mType;

					mInstructions[i + 0]->mSrc[0].mIntConst = (UnsignedTypeMax(mInstructions[i + 0]->mSrc[1].mType) >> shift) << shift;
					mInstructions[i + 0]->mDst.mRange = mInstructions[i + 0]->mSrc[1].mRange;
					mInstructions[i + 0]->mDst.mRange.LimitMax(mInstructions[i + 0]->mSrc[0].mIntConst);
					mInstructions[i + 0]->mDst.mRange.mMinState = IntegerValueRange::S_BOUND;
					mInstructions[i + 0]->mDst.mRange.mMinValue = 0;
					mInstructions[i + 1]->mSrc[1].mRange = mInstructions[i + 0]->mDst.mRange;

					if (shift > mshift && mInstructions[i + 0]->mDst.mType > mInstructions[i + 1]->mSrc[1].mType)
					{
						mInstructions[i + 1]->mSrc[1].mType = mInstructions[i + 0]->mDst.mType;
						mInstructions[i + 1]->mDst.mType = mInstructions[i + 0]->mDst.mType;
					}

					if (shift > mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHR;
						mInstructions[i + 1]->mSrc[0].mIntConst = shift - mshift;
					}
					else if (shift < mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHL;
						mInstructions[i + 1]->mSrc[0].mIntConst = mshift - shift;
					}
					else
					{
						mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mDst.mTemp = -1;
					}

					changed = true;
				}
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[0].mTemp < 0 && ispow2(mInstructions[i + 1]->mSrc[0].mIntConst) &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
			{

				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				if (shift & 7)
				{
					int64	mshift = binlog(mInstructions[i + 1]->mSrc[0].mIntConst);

					mInstructions[i + 0]->mOperator = IA_AND;
					mInstructions[i + 0]->mSrc[0].mType = mInstructions[i + 0]->mSrc[1].mType;

					mInstructions[i + 0]->mSrc[0].mIntConst = (UnsignedTypeMax(mInstructions[i + 0]->mSrc[1].mType) >> shift) << shift;
					mInstructions[i + 0]->mDst.mRange = mInstructions[i + 0]->mSrc[1].mRange;
					mInstructions[i + 0]->mDst.mRange.LimitMax(mInstructions[i + 0]->mSrc[0].mIntConst);
					mInstructions[i + 0]->mDst.mRange.mMinState = IntegerValueRange::S_BOUND;
					mInstructions[i + 0]->mDst.mRange.mMinValue = 0;
					mInstructions[i + 1]->mSrc[1].mRange = mInstructions[i + 0]->mDst.mRange;

					if (shift > mshift && mInstructions[i + 0]->mDst.mType > mInstructions[i + 1]->mSrc[1].mType)
					{
						mInstructions[i + 1]->mSrc[1].mType = mInstructions[i + 0]->mDst.mType;
						mInstructions[i + 1]->mDst.mType = mInstructions[i + 0]->mDst.mType;
					}

					if (shift > mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHR;
						mInstructions[i + 1]->mSrc[0].mIntConst = shift - mshift;
					}
					else if (shift < mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHL;
						mInstructions[i + 1]->mSrc[0].mIntConst = mshift - shift;
					}
					else
					{
						mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mDst.mTemp = -1;
					}

					changed = true;
				}
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHL && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHR && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
			{
				int64	shift = mInstructions[i + 0]->mSrc[0].mIntConst;
				if (shift & 7)
				{
					int64	mshift = mInstructions[i + 1]->mSrc[0].mIntConst;

					mInstructions[i + 0]->mOperator = IA_AND;
					mInstructions[i + 0]->mSrc[0].mType = mInstructions[i + 1]->mSrc[0].mType;

					switch (mInstructions[i + 0]->mSrc[1].mType)
					{
					case IT_INT8:
						mInstructions[i + 0]->mSrc[0].mIntConst = 0xffu >> shift;
						break;
					case IT_INT16:
						mInstructions[i + 0]->mSrc[0].mIntConst = 0xffffu >> shift;
						break;
					case IT_INT32:
						mInstructions[i + 0]->mSrc[0].mIntConst = 0xffffffffu >> shift;
						break;
					}

					if (shift > mshift && mInstructions[i + 0]->mDst.mType > mInstructions[i + 1]->mSrc[1].mType)
					{
						mInstructions[i + 1]->mSrc[1].mType = mInstructions[i + 0]->mDst.mType;
						mInstructions[i + 1]->mDst.mType = mInstructions[i + 0]->mDst.mType;
					}

					if (shift > mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHL;
						mInstructions[i + 1]->mSrc[0].mIntConst = shift - mshift;
					}
					else if (shift < mshift)
					{
						mInstructions[i + 1]->mOperator = IA_SHR;
						mInstructions[i + 1]->mSrc[0].mIntConst = mshift - shift;
					}
					else
					{
						mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mDst.mTemp = -1;
					}

					changed = true;
				}
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal
				)
			{
				mInstructions[i + 1]->mSrc[0].ForwardTemp(mInstructions[i + 0]->mSrc[0]);
				mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
				mInstructions[i + 1]->mCode == IC_LOAD && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal
				)
			{
				mInstructions[i + 1]->mSrc[0].ForwardTemp(mInstructions[i + 0]->mSrc[0]);
				mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR &&
				(mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal && mInstructions[i + 1]->mSrc[1].mTemp < 0 ||
					mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal && mInstructions[i + 1]->mSrc[0].mTemp < 0)
				)
			{
				int64 v = mInstructions[i + 1]->mSrc[0].mIntConst;
				InterOperator	op = mInstructions[i + 1]->mOperator;
				if (mInstructions[i + 1]->mSrc[0].mTemp >= 0)
				{
					v = mInstructions[i + 1]->mSrc[1].mIntConst;
					op = MirrorRelational(op);
				}

				bool	flip = false, istrue = false, isfalse = true;

				switch (op)
				{
				case IA_CMPEQ:
					flip = v == 0;
					istrue = false;
					isfalse = (v != 0 && v != 1);
					break;
				case IA_CMPNE:
					flip = v != 0;
					isfalse = false;
					istrue = (v != 0 && v != 1);
					break;
				case IA_CMPGEU:
				case IA_CMPGES:
					istrue = v <= 0;
					isfalse = v > 1;
					break;
				case IA_CMPGU:
				case IA_CMPGS:
					istrue = v < 0;
					isfalse = v >= 1;
					break;
				case IA_CMPLEU:
				case IA_CMPLES:
					flip = true;
					isfalse = v < 0;
					istrue = v >= 1;
					break;
				case IA_CMPLU:
				case IA_CMPLS:
					flip = true;
					isfalse = v <= 0;
					istrue = v > 1;
					break;
				}

				if (istrue)
				{
					mInstructions[i + 1]->mCode = IC_CONSTANT;
					mInstructions[i + 1]->mConst.mType = IT_BOOL;
					mInstructions[i + 1]->mConst.mIntConst = 1;
					mInstructions[i + 1]->mSrc[0].mTemp = -1;
					mInstructions[i + 1]->mSrc[0].mType = IT_NONE;
					mInstructions[i + 1]->mSrc[1].mTemp = -1;
					mInstructions[i + 1]->mSrc[1].mType = IT_NONE;
					mInstructions[i + 1]->mNumOperands = 0;
				}
				else if (isfalse)
				{
					mInstructions[i + 1]->mCode = IC_CONSTANT;
					mInstructions[i + 1]->mConst.mType = IT_BOOL;
					mInstructions[i + 1]->mConst.mIntConst = 0;
					mInstructions[i + 1]->mSrc[0].mTemp = -1;
					mInstructions[i + 1]->mSrc[0].mType = IT_NONE;
					mInstructions[i + 1]->mSrc[1].mTemp = -1;
					mInstructions[i + 1]->mSrc[1].mType = IT_NONE;
					mInstructions[i + 1]->mNumOperands = 0;
				}
				else
				{
					mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
					mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
					mInstructions[i + 1]->mDst.mTemp = -1;
					if (flip)
						mInstructions[i + 0]->mOperator = InvertRelational(mInstructions[i + 0]->mOperator);
				}
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
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && IsCommutative(mInstructions[i + 1]->mOperator) && 
				mInstructions[i + 0]->mDst.mTemp == mInstructions[i + 1]->mSrc[0].mTemp && 
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 1]->mSrc[1].mTemp)
			{
				InterOperand	io = mInstructions[i + 1]->mSrc[1];
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
				mInstructions[i + 1]->mSrc[0] = io;
				changed = true;
			}
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				mInstructions[i + 0]->mSrc[1].IsUByte() && !mInstructions[i + 0]->mSrc[0].IsUByte() &&
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[1].mTemp)
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
				mInstructions[i + 0]->mSrc[0].IsUByte() && !mInstructions[i + 0]->mSrc[1].IsUByte() &&
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[0].mTemp)
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
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SUB && mInstructions[i + 0]->mSrc[0].mTemp < 0 && 
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[1].mTemp < 0 && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal)
			{
				mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 1]->mSrc[1].mIntConst -= mInstructions[i + 0]->mSrc[0].mIntConst;

				mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal)
			{
				mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;
				mInstructions[i + 0]->mSrc[0].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;

				mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}
#endif
#if 1
			else if (
				mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[0].mTemp >= 0 &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[0].mTemp >= 0 && 
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && 
				mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 0]->mSrc[0].IsUByte() && mInstructions[i + 1]->mSrc[0].IsUByte() && mInstructions[i + 0]->mSrc[0].mRange.mMaxValue + mInstructions[i + 1]->mSrc[0].mRange.mMaxValue < 256)
			{
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];

				mInstructions[i + 0]->mCode = IC_BINARY_OPERATOR;
				mInstructions[i + 0]->mOperator = IA_ADD;
				mInstructions[i + 0]->mDst.mType = IT_INT16;
				mInstructions[i + 0]->mSrc[1] = mInstructions[i + 1]->mSrc[0];
				mInstructions[i + 0]->mDst.mRange = mInstructions[i + 0]->mSrc[0].mRange;
				mInstructions[i + 0]->mDst.mRange.mMaxValue += mInstructions[i + 0]->mSrc[1].mRange.mMaxValue;

				mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mDst;
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
				mInstructions[i + 0]->mDst.mTemp = -1;
				mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
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
				mInstructions[i + 0]->mDst.mTemp = -1;
				mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
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
				mInstructions[i + 0]->mDst.mTemp = -1;
				mInstructions[i + 1]->mCode = IC_NONE; mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_ADD &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mTemp >= 0 &&
				mInstructions[i + 2]->mCode == IC_LEA && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
				mInstructions[i + 2]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[1].mTemp)
			{
				mInstructions[i + 2]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 1]->mDst.mRange.Reset();
				mInstructions[i + 0]->mSrc[1].mFinal = false;
				mInstructions[i + 1]->mSrc[0].mFinal = false;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_ADD &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 1]->mSrc[1].mTemp >= 0 &&

				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_ADD &&
				mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp &&

				//						!mInstructions[i + 1]->ReferencesTemp(mInstructions[i + 0]->mDst.mTemp) &&
				(mInstructions[i + 2]->mSrc[0].mFinal || mInstructions[i + 2]->mSrc[1].mFinal))
			{
				if (mInstructions[i + 2]->mSrc[1].mFinal)
				{
					mInstructions[i + 0]->mSrc[0].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;
					mInstructions[i + 1]->mSrc[1].mFinal = false;
					mInstructions[i + 2]->mSrc[0] = mInstructions[i + 1]->mSrc[1];
					mInstructions[i + 0]->mDst.mRange.Reset();
					mInstructions[i + 2]->mDst.mRange.Reset();
				}
				else
				{
					mInstructions[i + 1]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;
					mInstructions[i + 0]->mSrc[1].mFinal = false;
					mInstructions[i + 2]->mSrc[0] = mInstructions[i + 0]->mSrc[1];

					mInstructions[i + 1]->mDst.mRange.Reset();
					mInstructions[i + 2]->mDst.mRange.Reset();
				}

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
				mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&
				mInstructions[i + 1]->mCode == IC_LOAD && mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				mInstructions[i + 1]->mSrc[0].mIntConst + mInstructions[i + 0]->mSrc[0].mIntConst < 256)
			{
				mInstructions[i + 1]->mSrc[0].mTemp = mInstructions[i + 0]->mSrc[1].mTemp;
				mInstructions[i + 1]->mSrc[0].mRange = mInstructions[i + 0]->mSrc[1].mRange;
				mInstructions[i + 1]->mSrc[0].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[0].mFinal = mInstructions[i + 0]->mSrc[1].mFinal;
				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
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
				mInstructions[i + 2]->mSrc[0].mRange.AddConstValue(IT_INT16, - (mInstructions[i + 0]->mSrc[0].mIntConst << mInstructions[i + 1]->mSrc[0].mIntConst));

				mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mDst.mType == IT_INT16 &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 0]->mDst.mIntConst >= 0 &&
				mInstructions[i + 0]->mSrc[1].mRange.mMaxState == IntegerValueRange::S_BOUND && mInstructions[i + 0]->mSrc[1].mRange.mMaxValue + mInstructions[i + 0]->mSrc[0].mIntConst < 32767 &&
				(IsSignedRelational(mInstructions[i + 1]->mOperator) || mInstructions[i + 0]->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND && mInstructions[i + 0]->mSrc[1].mRange.mMinValue >= 0) &&
				mInstructions[i + 1]->mSrc[0].mIntConst - mInstructions[i + 0]->mSrc[0].mIntConst >= (IsSignedRelational(mInstructions[i + 1]->mOperator) ? 0 : -32768) &&
				mInstructions[i + 1]->mSrc[0].mIntConst - mInstructions[i + 0]->mSrc[0].mIntConst <= 32767)
			{
				mInstructions[i + 1]->mSrc[0].mIntConst -= mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 0]->mSrc[1].mFinal = false;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SUB && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mDst.mType == IT_INT16 &&
				mInstructions[i + 0]->mDst.mIntConst >= 0 &&
				mInstructions[i + 0]->mSrc[1].mRange.mMinState == IntegerValueRange::S_BOUND && mInstructions[i + 0]->mSrc[1].mRange.mMinValue - mInstructions[i + 0]->mSrc[0].mIntConst >= (IsSignedRelational(mInstructions[i + 1]->mOperator) ? -32768 : 0) &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[0].mIntConst + mInstructions[i + 0]->mSrc[0].mIntConst >= (IsSignedRelational(mInstructions[i + 1]->mOperator) ? -32768 : 0) &&
				mInstructions[i + 1]->mSrc[0].mIntConst + mInstructions[i + 0]->mSrc[0].mIntConst <= 32767)
			{
				mInstructions[i + 1]->mSrc[0].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 0]->mSrc[1].mFinal = false;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_LEA &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && 
				mInstructions[i + 2]->mCode == IC_LOAD &&
				mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
				CanSwapInstructions(mInstructions[i + 1], mInstructions[i + 2]))
			{
				InterInstruction* ins = mInstructions[i + 2];
				mInstructions[i + 2] = mInstructions[i + 1];
				mInstructions[i + 1] = ins;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_STORE && mInstructions[i + 1]->mCode == IC_STORE &&
				SameMemSegment(mInstructions[i + 1]->mSrc[1], mInstructions[i + 0]->mSrc[1]) &&
				!mInstructions[i + 0]->mVolatile && !mInstructions[i + 1]->mVolatile)
			{
				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
			else if (
				mInstructions[i + 0]->mCode == IC_LEA && 
				mInstructions[i + 0]->mSrc[0].mTemp >= 0 && mInstructions[i + 0]->mSrc[1].mTemp >= 0 &&

				mInstructions[i + 1]->mCode == IC_LOAD && 
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&

				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_ADD &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 2]->mDst.mTemp && 
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 0]->mSrc[0].mTemp &&
				mInstructions[i + 2]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 2]->mSrc[0].mIntConst == mInstructions[i + 1]->mSrc[0].mIntConst &&
				mInstructions[i + 2]->mSrc[1].mTemp != mInstructions[i + 1]->mDst.mTemp)
			{
				InterInstruction* iadd = mInstructions[i + 2];
				mInstructions[i + 2] = mInstructions[i + 1];
				mInstructions[i + 1] = mInstructions[i + 0];
				mInstructions[i + 0] = iadd;
				mInstructions[i + 2]->mSrc[0].mIntConst = 0;
				changed = true;
			}
#if 1
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_AND &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHR &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&
				mInstructions[i + 2]->mSrc[0].mTemp < 0)
			{
				mInstructions[i + 0]->mSrc[0].mIntConst &= ~((1 << mInstructions[i + 1]->mSrc[0].mIntConst) - 1);
				mInstructions[i + 2]->mSrc[0].mIntConst <<= mInstructions[i + 1]->mSrc[0].mIntConst;
				mInstructions[i + 2]->mSrc[1] = mInstructions[i + 0]->mDst;
				mInstructions[i + 1]->mCode = IC_NONE;
				mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;
				changed = true;
			}

			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0)
			{
				mInstructions[i + 1]->mSrc[0].mIntConst <<= mInstructions[i + 0]->mSrc[0].mIntConst;

				mInstructions[i + 0]->mOperator = IA_AND;
				mInstructions[i + 0]->mSrc[0].mIntConst = ~((1 << mInstructions[i + 0]->mSrc[0].mIntConst) - 1);
				mInstructions[i + 0]->mDst.mRange.Reset();
				mInstructions[i + 1]->mSrc[1].mRange.Reset();
				changed = true;
			}

			if (i + 3 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 0]->mDst.IsUByte() && mInstructions[i + 0]->mSrc[1].IsUByte() &&
				mInstructions[i + 1]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 1]->mOperator == IA_EXT8TO16U &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal &&
				mInstructions[i + 2]->mCode == IC_LEA && mInstructions[i + 2]->mSrc[1].mTemp >= 0 &&
				mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
				mInstructions[i + 3]->mCode == IC_STORE &&
				mInstructions[i + 3]->mSrc[1].mTemp == mInstructions[i + 2]->mDst.mTemp && mInstructions[i + 3]->mSrc[1].mFinal)
			{
				mInstructions[i + 3]->mSrc[1].mIntConst += mInstructions[i + 0]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 1]->mDst.mRange = mInstructions[i + 0]->mSrc[1].mRange;
				mInstructions[i + 2]->mSrc[0].mRange = mInstructions[i + 0]->mSrc[1].mRange;
				changed = true;
			}


			if (i + 3 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SHR &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 1]->mOperator == IA_ADD || mInstructions[i + 1]->mOperator == IA_SUB) &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&

				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_SHL &&
				mInstructions[i + 2]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&

				mInstructions[i + 3]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 3]->mOperator == IA_AND &&
				mInstructions[i + 3]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 3]->mSrc[1].mTemp == mInstructions[i + 2]->mDst.mTemp && mInstructions[i + 3]->mSrc[1].mFinal &&

				mInstructions[i + 0]->mSrc[0].mIntConst == mInstructions[i + 2]->mSrc[0].mIntConst)
			{
				mInstructions[i + 3]->mSrc[0].mIntConst &= ~((1 << mInstructions[i + 0]->mSrc[0].mIntConst) - 1);
				mInstructions[i + 1]->mSrc[0].mIntConst <<= mInstructions[i + 0]->mSrc[0].mIntConst;

				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 1]->mDst = mInstructions[i + 2]->mDst;

				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				mInstructions[i + 2]->mCode = IC_NONE;
				mInstructions[i + 2]->mNumOperands = 0;
				mInstructions[i + 2]->mDst.mTemp = -1;
				changed = true;
			}
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_AND &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 1]->mOperator == IA_ADD || mInstructions[i + 1]->mOperator == IA_SUB) &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&

				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_AND &&
				mInstructions[i + 2]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&

				(mInstructions[i + 0]->mSrc[0].mIntConst & (mInstructions[i + 0]->mSrc[0].mIntConst + 1)) == 0 && // Is power of two - 1
				(~mInstructions[i + 0]->mSrc[0].mIntConst & mInstructions[i + 1]->mSrc[0].mIntConst) == 0 && // add is part of initial mask
				(~mInstructions[i + 0]->mSrc[0].mIntConst & mInstructions[i + 2]->mSrc[0].mIntConst) == 0) // final mask is part of initial mask
			{
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];

				mInstructions[i + 0]->mCode = IC_NONE;
				mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
#endif

#if 1
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 0]->mOperator == IA_EXT8TO16U &&
				mInstructions[i + 0]->mSrc[0].mTemp >= 0 &&
				mInstructions[i + 1]->mDst.mTemp != mInstructions[i + 0]->mSrc[0].mTemp &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 1]->mOperator == IA_ADD || mInstructions[i + 1]->mOperator == IA_SUB) &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp &&

				mInstructions[i + 2]->mCode == IC_STORE && 
				mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
				mInstructions[i + 2]->mSrc[0].mType == IT_INT8)
			{
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[0];
				mInstructions[i + 0]->mSrc[0].mFinal = false;
				mInstructions[i + 1]->mDst.mType = IT_INT8;

				changed = true;
			}
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_CONVERSION_OPERATOR && mInstructions[i + 0]->mOperator == IA_EXT8TO16U &&
				mInstructions[i + 0]->mSrc[0].mTemp >= 0 &&
				mInstructions[i + 1]->mDst.mTemp != mInstructions[i + 0]->mSrc[0].mTemp &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 1]->mOperator == IA_ADD || mInstructions[i + 1]->mOperator == IA_SUB) &&
				mInstructions[i + 1]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp &&

				mInstructions[i + 2]->mCode == IC_STORE &&
				mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
				mInstructions[i + 2]->mSrc[0].mType == IT_INT8)
			{
				mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mSrc[0];
				mInstructions[i + 0]->mSrc[0].mFinal = false;
				mInstructions[i + 1]->mDst.mType = IT_INT8;

				changed = true;
			}
#endif

#if 0
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mDst.mType == IT_FLOAT &&

				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && (mInstructions[i + 1]->mOperator == IA_ADD || mInstructions[i + 1]->mOperator == IA_MUL) &&
				mInstructions[i + 1]->mSrc[0].mTemp != mInstructions[i + 0]->mDst.mTemp &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp)
			{
				InterOperand	op = mInstructions[i + 1]->mSrc[0];
				mInstructions[i + 1]->mSrc[0] = mInstructions[i + 1]->mSrc[1];
				mInstructions[i + 1]->mSrc[1] = op;

				changed = true;
			}
#endif
#if 1
			if (i + 2 < mInstructions.Size() &&
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && 
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 1]->mSrc[0].mTemp &&
				mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 1]->mSrc[1].mTemp &&
				mInstructions[i + 2]->mSrc[0].mTemp >= 0 && mInstructions[i + 2]->mSrc[1].mTemp >= 0 &&
				mInstructions[i + 2]->mSrc[0].mFinal && mInstructions[i + 2]->mSrc[1].mFinal &&
				mInstructions[i + 2]->mDst.mType == IT_INT16)
			{
				bool	fail = false;
				int		s0, s1, c0, c1, s2 = i + 2;

				if (mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp)
				{
					s0 = i + 0; s1 = i + 1;
				}
				else if (mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp)
				{
					s0 = i + 1; s1 = i + 0;
				}
				else
					fail = true;

				if (!fail)
				{
					if (mInstructions[s0]->mSrc[0].mTemp < 0)
						c0 = 0;
					else if (mInstructions[s0]->mSrc[1].mTemp < 0)
						c0 = 1;
					else
						fail = true;

					if (mInstructions[s1]->mSrc[0].mTemp < 0)
						c1 = 0;
					else if (mInstructions[s1]->mSrc[1].mTemp < 0)
						c1 = 1;
					else
						fail = true;

					if (!fail)
					{
						InterOperator	o0 = mInstructions[s0]->mOperator;
						InterOperator	o1 = mInstructions[s1]->mOperator;
						InterOperator	o2 = mInstructions[s2]->mOperator;

						if (o2 == IA_SUB)
						{
							if ((o0 == IA_ADD || o0 == IA_SUB && c0 == 0) && (o1 == IA_ADD || o1 == IA_SUB && c1 == 0))
							{
								int64 iconst =
									(o1 == IA_ADD ? mInstructions[s1]->mSrc[c1].mIntConst : -mInstructions[s1]->mSrc[c1].mIntConst) -
									(o0 == IA_ADD ? mInstructions[s0]->mSrc[c0].mIntConst : -mInstructions[s0]->mSrc[c0].mIntConst);

								mInstructions[s0]->mSrc[0] = mInstructions[s0]->mSrc[1 - c0];
								mInstructions[s0]->mSrc[1] = mInstructions[s1]->mSrc[1 - c1];
								mInstructions[s0]->mDst.mRange.Reset();
								mInstructions[s0]->mOperator = IA_SUB;
								mInstructions[s2]->mOperator = IA_ADD;
								mInstructions[s2]->mSrc[0].mRange = mInstructions[s0]->mDst.mRange;
								mInstructions[s2]->mSrc[1].mTemp = -1;
								mInstructions[s2]->mSrc[1].mIntConst = iconst;
								changed = true;
							}
						}
						else if (o2 == IA_ADD)
						{
							if ((o0 == IA_ADD || o0 == IA_SUB && c0 == 0) && (o1 == IA_ADD || o1 == IA_SUB && c1 == 0))
							{
								int64 iconst =
									(o1 == IA_ADD ? mInstructions[s1]->mSrc[c1].mIntConst : -mInstructions[s1]->mSrc[c1].mIntConst) +
									(o0 == IA_ADD ? mInstructions[s0]->mSrc[c0].mIntConst : -mInstructions[s0]->mSrc[c0].mIntConst);

								mInstructions[s0]->mSrc[0] = mInstructions[s0]->mSrc[1 - c0];
								mInstructions[s0]->mSrc[1] = mInstructions[s1]->mSrc[1 - c1];
								mInstructions[s0]->mDst.mRange.Reset();
								mInstructions[s0]->mOperator = IA_ADD;
								mInstructions[s2]->mOperator = IA_ADD;
								mInstructions[s2]->mSrc[0].mRange = mInstructions[s0]->mDst.mRange;
								mInstructions[s2]->mSrc[1].mTemp = -1;
								mInstructions[s2]->mSrc[1].mIntConst = iconst;
								changed = true;
							}
						}
					}
				}
			}
#endif

#if 1
			// Postincrement artifact
			if (mInstructions[i]->mCode == IC_LOAD_TEMPORARY)
			{
				InterInstruction* mins = mInstructions[i];

				int j = i + 1;
				while (j + 1 < mInstructions.Size() &&
					!mInstructions[j]->ReferencesTemp(mins->mDst.mTemp) &&
					!mInstructions[j]->ReferencesTemp(mins->mSrc[0].mTemp))
					j++;

				if (j + 1 < mInstructions.Size())
				{
					InterInstruction* bins = mInstructions[j];
					if (bins->mCode == IC_BINARY_OPERATOR &&
						bins->mSrc[0].mTemp < 0 &&
						mins->mSrc[0].mTemp == bins->mSrc[1].mTemp &&
						mins->mSrc[0].mTemp == bins->mDst.mTemp)
					{
						int		ttemp = bins->mDst.mTemp;
						int	k = j;
						while (k + 2 < mInstructions.Size() &&
							mInstructions[k + 1]->mCode != IC_RELATIONAL_OPERATOR &&
							!mInstructions[k + 1]->ReferencesTemp(ttemp))
						{
							mInstructions[k] = mInstructions[k + 1];
							k++;
						}
						if (k > j)
						{
							mInstructions[k] = bins;
							changed = true;
						}

						j = i;
						while (j + 1 < mInstructions.Size() &&
							!mInstructions[j + 1]->ReferencesTemp(mins->mDst.mTemp) &&
							!mInstructions[j + 1]->ReferencesTemp(mins->mSrc[0].mTemp))
						{
							mInstructions[j] = mInstructions[j + 1];
							j++;
						}
						if (j > i)
						{
							mInstructions[j] = mins;
							changed = true;
						}
					}
				}
			}

			CheckFinalLocal();
#endif
		}
#if 1
		if (i + 2 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY &&
				IsIntegerType(mInstructions[i + 0]->mDst.mType) &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 0]->mSrc[0].mTemp == mInstructions[i + 1]->mSrc[1].mTemp &&
				mInstructions[i + 0]->mSrc[0].mTemp == mInstructions[i + 1]->mDst.mTemp &&
				mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp &&
				mInstructions[i + 2]->mSrc[0].mTemp < 0)
			{
				if (
					mInstructions[i + 1]->mOperator == IA_ADD &&
					(mInstructions[i + 2]->mOperator == IA_CMPLES || mInstructions[i + 2]->mOperator == IA_CMPLS) &&
					mInstructions[i + 1]->mSrc[0].mIntConst > 0 &&
					mInstructions[i + 2]->mSrc[0].mIntConst + mInstructions[i + 1]->mSrc[0].mIntConst < SignedTypeMax(mInstructions[i + 2]->mSrc[0].mType))
				{
					mInstructions[i + 2]->mSrc[1] = mInstructions[i + 1]->mDst;
					mInstructions[i + 2]->mSrc[1].mFinal = false;
					mInstructions[i + 2]->mSrc[0].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;
					changed = true;
				}
			}
		}
#endif
		if (i + 3 < mInstructions.Size())
		{
			if (
				mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD && mInstructions[i + 0]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 2]->mOperator == IA_ADD &&
				mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i + 1]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal &&
				mInstructions[i + 3]->mCode == IC_LEA && mInstructions[i + 3]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 3]->mSrc[0].mTemp == mInstructions[i + 2]->mDst.mTemp && mInstructions[i + 3]->mSrc[0].mFinal)
			{
				int64	d = mInstructions[i + 0]->mSrc[0].mIntConst * mInstructions[i + 1]->mSrc[0].mIntConst;
				mInstructions[i + 3]->mSrc[1].mIntConst += d;
				mInstructions[i + 1]->mSrc[1] = mInstructions[i + 0]->mSrc[1];
				mInstructions[i + 1]->mDst.mRange.mMinValue -= d; mInstructions[i + 1]->mDst.mRange.mMaxValue -= d;
				mInstructions[i + 2]->mSrc[1].mRange.mMinValue -= d; mInstructions[i + 2]->mSrc[1].mRange.mMaxValue -= d;
				mInstructions[i + 2]->mDst.mRange.mMinValue -= d; mInstructions[i + 2]->mDst.mRange.mMaxValue -= d;
				mInstructions[i + 3]->mSrc[0].mRange.mMinValue -= d; mInstructions[i + 3]->mSrc[0].mRange.mMaxValue -= d;
				mInstructions[i + 0]->mCode = IC_NONE; mInstructions[i + 0]->mNumOperands = 0;
				mInstructions[i + 0]->mDst.mTemp = -1;
				changed = true;
			}
		}


#if 1
		if (i + 1 < mInstructions.Size())
		{
			if (
				mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp < 0 && mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[0].mTemp &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 1]->mSrc[0].mIntConst < 0)
			{
				InterOperand	t0 = mInstructions[i + 0]->mDst;
				InterOperand	t1 = mInstructions[i + 1]->mDst;

				mInstructions[i + 0]->mDst = t1;
				mInstructions[i + 1]->mDst = t0;
				mInstructions[i + 1]->mSrc[1] = t1;
				mInstructions[i + 0]->mSrc[1].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[0].mIntConst = -mInstructions[i + 1]->mSrc[0].mIntConst;
				changed = true;
			}
			if (
				mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 0]->mDst.mTemp != mInstructions[i + 0]->mSrc[1].mTemp &&
				mInstructions[i + 1]->mCode == IC_LEA && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mTemp < 0 && mInstructions[i + 1]->mSrc[0].mIntConst < 0)
			{
				InterOperand	t0 = mInstructions[i + 0]->mDst;
				InterOperand	t1 = mInstructions[i + 1]->mDst;

				mInstructions[i + 0]->mDst = t1;
				mInstructions[i + 1]->mDst = t0;
				mInstructions[i + 1]->mSrc[1] = t1;
				mInstructions[i + 0]->mSrc[0].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;
				mInstructions[i + 1]->mSrc[0].mIntConst = -mInstructions[i + 1]->mSrc[0].mIntConst;
				changed = true;
			}
		}

#endif
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
#if 1
		if (i + 1 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_LEA && mInstructions[i + 0]->mSrc[1].mTemp < 0 &&
				mInstructions[i + 1]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				mInstructions[i + 1]->mSrc[0].mMemory == IM_ABSOLUTE && mInstructions[i + 1]->mSrc[0].mIntConst == 0)
			{
				if (mInstructions[i + 0]->mSrc[1].mMemory != IM_ABSOLUTE)
				{
					if (mInstructions[i + 1]->mOperator == IA_CMPEQ)
					{
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mCode = IC_CONSTANT;
						mInstructions[i + 1]->mConst.mType = IT_BOOL;
						mInstructions[i + 1]->mConst.mIntConst = 0;
						changed = true;
					}
					else if (mInstructions[i + 1]->mOperator == IA_CMPNE)
					{
						mInstructions[i + 1]->mNumOperands = 0;
						mInstructions[i + 1]->mCode = IC_CONSTANT;
						mInstructions[i + 1]->mConst.mType = IT_BOOL;
						mInstructions[i + 1]->mConst.mIntConst = 1;
						changed = true;
					}
				}
			}
		}
#endif
#if 1
		if (i + 1 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_ADD &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_ADD &&
				mInstructions[i + 1]->mSrc[1].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[1].mFinal &&
				mInstructions[i + 0]->mSrc[0].mTemp < 0 && mInstructions[i + 1]->mSrc[0].mTemp < 0 &&
				IsIntegerType(mInstructions[i + 0]->mDst.mType))
			{
				mInstructions[i + 0]->mSrc[0].mIntConst += mInstructions[i + 1]->mSrc[0].mIntConst;
				mInstructions[i + 0]->mDst = mInstructions[i + 1]->mDst;

				mInstructions[i + 1]->mCode = IC_NONE;
				mInstructions[i + 1]->mNumOperands = 0;
				mInstructions[i + 1]->mDst.mTemp = -1;

				changed = true;
			}
		}
#endif
#if 1
		if (i + 1 < mInstructions.Size())
		{
			if (mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SUB &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHL &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal && mInstructions[i + 1]->mSrc[0].IsUByte() &&
				mInstructions[i + 1]->mSrc[1].mTemp < 0 && ispow2(mInstructions[i + 1]->mSrc[1].mIntConst))
			{
				int64 k = binlog(mInstructions[i + 1]->mSrc[1].mIntConst);
				
				if (mInstructions[i + 0]->mSrc[0].mTemp >= 0 && mInstructions[i + 0]->mSrc[1].mTemp < 0)
				{
					int64 s = mInstructions[i + 0]->mSrc[1].mIntConst;
					mInstructions[i + 0]->mSrc[1] = mInstructions[i + 0]->mSrc[0];
					mInstructions[i + 0]->mSrc[0].mTemp = -1;
					mInstructions[i + 0]->mSrc[0].mIntConst = mInstructions[i + 0]->mSrc[1].mRange.mMinValue;
					mInstructions[i + 0]->mDst.mRange.SetLimit(0, mInstructions[i + 0]->mSrc[1].mRange.mMaxValue - mInstructions[i + 0]->mSrc[0].mIntConst);
					mInstructions[i + 1]->mOperator = IA_SHR;
					mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mDst;
					int64 d = s - mInstructions[i + 0]->mSrc[0].mIntConst;
					if (d < 0)
						mInstructions[i + 1]->mSrc[1].mIntConst >>= -d;
					else
						mInstructions[i + 1]->mSrc[1].mIntConst <<= d;
					changed = true;
				}
			}
			if (mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SUB &&
				mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_SHR &&
				mInstructions[i + 1]->mSrc[0].mTemp == mInstructions[i + 0]->mDst.mTemp && mInstructions[i + 1]->mSrc[0].mFinal && mInstructions[i + 1]->mSrc[0].IsUByte() &&
				mInstructions[i + 1]->mSrc[1].mTemp < 0 && ispow2(mInstructions[i + 1]->mSrc[1].mIntConst))
			{
				int64 k = binlog(mInstructions[i + 1]->mSrc[1].mIntConst);

				if (mInstructions[i + 0]->mSrc[0].mTemp >= 0 && mInstructions[i + 0]->mSrc[1].mTemp < 0)
				{
					int64 s = mInstructions[i + 0]->mSrc[1].mIntConst;
					mInstructions[i + 0]->mSrc[1] = mInstructions[i + 0]->mSrc[0];
					mInstructions[i + 0]->mSrc[0].mTemp = -1;
					mInstructions[i + 0]->mSrc[0].mIntConst = mInstructions[i + 0]->mSrc[1].mRange.mMinValue;
					mInstructions[i + 0]->mDst.mRange.SetLimit(0, mInstructions[i + 0]->mSrc[1].mRange.mMaxValue - mInstructions[i + 0]->mSrc[0].mIntConst);
					mInstructions[i + 1]->mOperator = IA_SHL;
					mInstructions[i + 1]->mSrc[0] = mInstructions[i + 0]->mDst;
					int64 d = s - mInstructions[i + 0]->mSrc[0].mIntConst;
					if (d < 0)
						mInstructions[i + 1]->mSrc[1].mIntConst <<= -d;
					else
						mInstructions[i + 1]->mSrc[1].mIntConst >>= d;
					changed = true;
				}
			}
		}


#endif
	}

	return changed;
}

static int TempUseDelta(const InterInstruction* ins)
{
	int d = 0;
	if (ins->mDst.mTemp >= 0)
		d++;
	for (int i = 0; i < ins->mNumOperands; i++)
		if (ins->mSrc[i].mTemp >= 0 && ins->mSrc[i].mFinal)
			d--;
	return d;
}

bool InterCodeBasicBlock::IsConstExitTemp(int temp) const
{
	int n = mInstructions.Size() - 1;
	while (n >= 0 && mInstructions[n]->mDst.mTemp != temp)
		n--;
	return n >= 0 && mInstructions[n]->mCode == IC_CONSTANT;
}

bool InterCodeBasicBlock::SplitSingleBranchUseConst(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (!mLoopHead && mFalseJump)
		{
			for (int i = 0; i + 1 < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];

				if (ins->mCode == IC_CONSTANT)
				{
					if (CanMoveInstructionBehindBlock(i))
					{
						InterCodeBasicBlock* tblock = nullptr;

						if (mTrueJump->mEntryRequiredTemps[ins->mDst.mTemp] && !mFalseJump->mEntryRequiredTemps[ins->mDst.mTemp])
							tblock = mTrueJump;
						else if (!mTrueJump->mEntryRequiredTemps[ins->mDst.mTemp] && mFalseJump->mEntryRequiredTemps[ins->mDst.mTemp])
							tblock = mFalseJump;

						if (tblock)
						{
							if (tblock->mNumEntries > 1)
							{
								InterCodeBasicBlock* nblock = new InterCodeBasicBlock(mProc);

								nblock->mEntryRequiredTemps = tblock->mEntryRequiredTemps;
								nblock->mExitRequiredTemps = tblock->mEntryRequiredTemps;

								mExitRequiredTemps -= ins->mDst.mTemp;
								nblock->mEntryRequiredTemps -= ins->mDst.mTemp;

								tblock->mEntryBlocks.RemoveAll(this);
								tblock->mEntryBlocks.Push(nblock);

								if (tblock == mTrueJump)
									mTrueJump = nblock;
								else
									mFalseJump = nblock;

								InterInstruction* jins = new InterInstruction(mInstructions.Last()->mLocation, IC_JUMP);
								nblock->mInstructions.Push(jins);
								nblock->Close(tblock, nullptr);

								nblock->mEntryBlocks.Push(this);
								nblock->mNumEntries = 1;

								tblock = nblock;
							}

							tblock->mInstructions.Insert(0, ins);
							mInstructions.Remove(i);
							i--;
							changed = true;
						}						
					}
				}
			}
		}
	
	
		if (mTrueJump && mTrueJump->SplitSingleBranchUseConst())
			changed = true;
		if (mFalseJump && mFalseJump->SplitSingleBranchUseConst())
			changed = true;
	}

	return changed;
}


bool InterCodeBasicBlock::CommonTailCodeMerge(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (!mLoopHead)
		{
			for (int i = 0; i + 1 < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];

				if (CanMoveInstructionBeforeBlock(i))
				{
					bool	aconst = true, aany = false;
					for (int j = 0; j < ins->mNumOperands; j++)
					{
						if (ins->mSrc[j].mTemp >= 0)
						{
							aany = true;
							int k = 0;
							while (k < mEntryBlocks.Size() && mEntryBlocks[k]->IsConstExitTemp(ins->mSrc[j].mTemp) && !mEntryBlocks[k]->mFalseJump)
								k++;
							if (k < mEntryBlocks.Size())
							{
								aconst = false;
								break;
							}
						}
					}

					if (aconst && aany)
					{
						for (int j = 0; j < mEntryBlocks.Size(); j++)
						{
							InterCodeBasicBlock* eblock = mEntryBlocks[j];
							eblock->mInstructions.Insert(eblock->mInstructions.Size() - 1, ins->Clone());
							if (ins->mDst.mTemp >= 0)
								eblock->mExitRequiredTemps += ins->mDst.mTemp;							
						}
						if (ins->mDst.mTemp >= 0)
							mEntryRequiredTemps += ins->mDst.mTemp;
						ins->mCode = IC_NONE;
						ins->mNumOperands = 0;
						ins->mDst.mTemp = -1;
						changed = true;
					}

				}
			}
		}

		if (mTrueJump && mTrueJump->CommonTailCodeMerge())
			changed = true;
		if (mFalseJump && mFalseJump->CommonTailCodeMerge())
			changed = true;
	}

	return changed;
}

void InterCodeBasicBlock::PeepholeOptimization(const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs)
{
	int		i;
	
	if (!mVisited)
	{
		mVisited = true;

		CheckFinalLocal();
		if (mTrueJump) mTrueJump->CheckFinalLocal();
		if (mFalseJump) mFalseJump->CheckFinalLocal();

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
						while (k < limit && CanSwapInstructions(ins, mInstructions[k + 1]))
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
						while (j < limit && CanSwapInstructions(ins, mInstructions[j + 1]))
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
				while (j < limit && CanSwapInstructions(ins, mInstructions[j + 1]))
				{
					SwapInstructions(ins, mInstructions[j + 1]);
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}
			else if (mInstructions[i]->mCode == IC_BINARY_OPERATOR || mInstructions[i]->mCode == IC_UNARY_OPERATOR || mInstructions[i]->mCode == IC_CONVERSION_OPERATOR || mInstructions[i]->mCode == IC_CONSTANT || mInstructions[i]->mCode == IC_LOAD_TEMPORARY && !mInstructions[i]->mSrc[0].mFinal)
			{
				InterInstruction* ins(mInstructions[i]);

				int k = i;
				while (k < limit && CanSwapInstructions(ins, mInstructions[k + 1]))
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
				while (j < limit && CanSwapInstructions(ins, mInstructions[j + 1]))
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
		i = 0;
		while (i <= limit)
		{
			InterInstruction* ins(mInstructions[i]);

			if (ins->mDst.mTemp >= 0 && (ins->mDst.mType == IT_FLOAT /* || ins->mDst.mType == IT_INT32*/))
			{
				if ((ins->mCode == IC_UNARY_OPERATOR || ins->mCode == IC_CONVERSION_OPERATOR) && ins->mSrc[0].mFinal ||
					(ins->mCode == IC_BINARY_OPERATOR && (ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mFinal || ins->mSrc[1].mTemp < 0 && ins->mSrc[0].mFinal)))
				{
					int reg = ins->mSrc[0].mTemp;
					if (ins->mCode == IC_BINARY_OPERATOR && reg < 0)
						reg = ins->mSrc[1].mTemp;

					int j = i - 1;
					while (j >= 0 && CanSwapInstructions(mInstructions[j], ins))
						j--;
					if (j >= 0 && j < i - 1 && mInstructions[j]->mDst.mTemp == reg)
					{
						int k = i - 1;
						while (k > j)
						{
							SwapInstructions(mInstructions[k], ins);
							mInstructions[k + 1] = mInstructions[k];
							k--;
						}
						mInstructions[k + 1] = ins;
					}
				}
			}
			i++;
		}
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
						CanSwapInstructions(mInstructions[j - 1], lins) &&
						CanSwapInstructions(mInstructions[j - 1], sins))
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
				while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
				{
					if (mInstructions[j - 1]->mCode == IC_STORE && mInstructions[j - 1]->mSrc[1].mMemory == IM_INDIRECT)
					{
						CanSwapInstructions(mInstructions[j - 1], ins);
					}
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
				while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
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
				while (j > 0 && CanSwapInstructions(mInstructions[j - 1], ins))
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

		// Bubble up unrelated instructions
		i = mInstructions.Size() - 2;
		while (i >= 0)
		{
			if (mInstructions[i]->mCode == IC_LOAD ||
				mInstructions[i]->mCode == IC_UNARY_OPERATOR ||
				mInstructions[i]->mCode == IC_CONVERSION_OPERATOR ||
				mInstructions[i]->mCode == IC_BINARY_OPERATOR && (mInstructions[i]->mSrc[0].mTemp < 0 || mInstructions[i]->mSrc[1].mTemp < 0))
			{
				InterInstruction* ins(mInstructions[i]);

				if (mInstructions[i + 1]->mCode == IC_CONSTANT && CanSwapInstructions(ins, mInstructions[i + 1]))
				{
					if (mInstructions[i + 2]->mNumOperands >= 1 && mInstructions[i + 2]->mSrc[0].mTemp == mInstructions[i]->mDst.mTemp && mInstructions[i + 2]->mSrc[0].mFinal ||
						mInstructions[i + 2]->mNumOperands == 2 && mInstructions[i + 2]->mSrc[1].mTemp == mInstructions[i]->mDst.mTemp && mInstructions[i + 2]->mSrc[1].mFinal)
					{ 
						SwapInstructions(ins, mInstructions[i + 1]);
						mInstructions[i] = mInstructions[i + 1];
						mInstructions[i + 1] = ins;
					}
				}
			}
			i--;
		}

#if 1
		do {} while (PeepholeReplaceOptimization(staticVars, staticProcs));
#endif

		limit = mInstructions.Size() - 1;
		if (limit >= 0 && mInstructions[limit]->mCode == IC_BRANCH)
		{
			limit--;
			if (limit > 0 && mInstructions[limit]->mCode == IC_RELATIONAL_OPERATOR)
				limit--;
		}

#if 1
		i = limit;
		while (i >= 0)
		{
			// move non indirect loads down
			if (mInstructions[i]->mCode == IC_LOAD && (mInstructions[i]->mSrc[0].mMemory != IM_INDIRECT || mInstructions[i]->mDst.mType != IT_INT8 || !mInstructions[i]->mSrc[0].mFinal))
			{
				InterInstruction* ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanSwapInstructions(ins, mInstructions[j + 1]))
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
				while (j < limit && CanSwapInstructions(ins, mInstructions[j + 1]))
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

		do	{} while (PeepholeReplaceOptimization(staticVars, staticProcs));

		// build trains
#if 1
		for(int i = mInstructions.Size() - 1; i > 0; i--)
		{
			InterInstruction* tins = mInstructions[i];

			int	ti = i;

			j = i - 1;
			while (j >= 0 && !tins->ReferencesTemp(mInstructions[j]->mDst.mTemp))
				j--;
			while (j >= 0)
			{
				InterInstruction* jins = mInstructions[j];

				if (j < ti - 1)
				{
					if (CanMoveInstructionDown(j, ti))
					{
						for (int k = j; k < ti - 1; k++)
						{
							SwapInstructions(jins, mInstructions[k + 1]);
							mInstructions[k] = mInstructions[k + 1];
						}
						mInstructions[ti - 1] = jins;

						if (jins->NumUsedTemps() <= 1 && !(jins->mCode == IC_CALL || jins->mCode == IC_CALL_NATIVE))
							ti--;
						//					mInstructions.Insert(i, mInstructions[j]);
						//					mInstructions.Remove(j);
					}
				}
				else if (jins->NumUsedTemps() <= 1 && !(jins->mCode == IC_CALL || jins->mCode == IC_CALL_NATIVE))
					ti--;

				j--;
				while (j >= 0 && !tins->ReferencesTemp(mInstructions[j]->mDst.mTemp))
					j--;
			}
		}
#endif
		CheckFinalLocal();

		// check move calls our of range to save register spilling
		i = 0;
		while (i < mInstructions.Size())
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_CALL || ins->mCode == IC_CALL_NATIVE)
			{
				int j = i;
				while (j > 0 && CanSwapInstructions(ins, mInstructions[j - 1]) && TempUseDelta(mInstructions[j - 1]) >= 0)
					j--;
				while (j < i && TempUseDelta(mInstructions[j]) == 0)
					j++;

				if (j < i)
				{
					while (i > j)
					{
						i--;
						SwapInstructions(mInstructions[i], ins);
						mInstructions[i + 1] = mInstructions[i];
					}
					mInstructions[j] = ins;
				}
			}
			i++;
		}

		// sort stores up

		bool	changed;

		do
		{
			changed = false;

			for (int i = 0; i + 1 < mInstructions.Size(); i++)
			{
				if (mInstructions[i + 0]->mCode == IC_STORE && mInstructions[i + 1]->mCode == IC_STORE && 
					!mInstructions[i + 0]->mVolatile && !mInstructions[i + 1]->mVolatile &&
					!CollidingMem(mInstructions[i + 0], mInstructions[i + 1]) &&
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
#if 1
				else if (i + 2 < mInstructions.Size() && mInstructions[i + 0]->mCode == IC_STORE && mInstructions[i + 2]->mCode == IC_STORE &&
					mInstructions[i + 1]->mCode == IC_LOAD &&
					CanSwapInstructions(mInstructions[i + 0], mInstructions[i + 1]) &&
					mInstructions[i + 1]->mDst.mTemp == mInstructions[i + 2]->mSrc[0].mTemp && mInstructions[i + 2]->mSrc[0].mFinal &&
					!mInstructions[i + 0]->mVolatile && !mInstructions[i + 2]->mVolatile &&
					SameMemRegion(mInstructions[i + 0]->mSrc[1], mInstructions[i + 2]->mSrc[1]) &&

					(mInstructions[i + 0]->mSrc[1].mVarIndex > mInstructions[i + 2]->mSrc[1].mVarIndex ||
						mInstructions[i + 0]->mSrc[1].mVarIndex == mInstructions[i + 2]->mSrc[1].mVarIndex &&
						mInstructions[i + 0]->mSrc[1].mIntConst > mInstructions[i + 2]->mSrc[1].mIntConst))
				{
					InterInstruction* ins = mInstructions[i + 0];
					SwapInstructions(ins, mInstructions[i + 1]);
					mInstructions[i + 0] = mInstructions[i + 1];
					SwapInstructions(ins, mInstructions[i + 2]);
					mInstructions[i + 1] = mInstructions[i + 2];
					mInstructions[i + 2] = ins;

					changed = true;
				}
#endif
			}

		} while (changed);

		// Move conversion from float to int upwards
		for (int i = 1; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins(mInstructions[i]);
			if (ins->mCode == IC_CONVERSION_OPERATOR && (ins->mOperator == IA_FLOAT2INT || ins->mOperator == IA_FLOAT2UINT || ins->mOperator == IA_FLOAT2LINT || ins->mOperator == IA_FLOAT2LUINT)  && ins->mSrc[0].mFinal)
			{
				int j = i - 1;
				while (j > 0 && CanSwapInstructions(ins, mInstructions[j]))
				{
					SwapInstructions(mInstructions[j], ins);
					mInstructions[j + 1] = mInstructions[j];
					j--;
				}
				mInstructions[j + 1] = ins;
			}
		}

		// move div up to mod
		int imod = -1, idiv = -1;
		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_MODU)
			{
				imod = -1;
				if (idiv >= 0 && ins->mSrc[0].IsEqual(mInstructions[idiv]->mSrc[0]) && ins->mSrc[1].IsEqual(mInstructions[idiv]->mSrc[1]))
				{
					int j = i - 1;
					while (j > idiv && CanSwapInstructions(mInstructions[j], ins))
					{
						SwapInstructions(mInstructions[j], ins);
						mInstructions[j + 1] = mInstructions[j];
						j--;
					}
					mInstructions[j + 1] = ins;
				}
				else
					imod = i;
			}
			else if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_DIVU)
			{
				idiv = -1;

				if (imod >= 0 && ins->mSrc[0].IsEqual(mInstructions[imod]->mSrc[0]) && ins->mSrc[1].IsEqual(mInstructions[imod]->mSrc[1]))
				{
					int j = i - 1;
					while (j > imod && CanSwapInstructions(mInstructions[j], ins))
					{
						SwapInstructions(mInstructions[j + 0], ins);
						mInstructions[j + 1] = mInstructions[j];
						j--;
					}
					mInstructions[j + 1] = ins;
				}
				else
					idiv = i;
			}
		}
#if 1
		// move lea to load/store down
		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mCode == IC_LEA && ((ins->mSrc[0].mTemp < 0 || !ins->mSrc[0].mFinal) || (ins->mSrc[1].mTemp < 0 || !ins->mSrc[1].mFinal)))
			{
				int j = i + 1;
				while (j < mInstructions.Size() && CanSwapInstructions(ins, mInstructions[j]))
					j++;
				if (j > i + 1 && j < mInstructions.Size() &&
					(mInstructions[j]->mCode == IC_LOAD && mInstructions[j]->mSrc[0].mTemp == ins->mDst.mTemp ||
						mInstructions[j]->mCode == IC_STORE && mInstructions[j]->mSrc[1].mTemp == ins->mDst.mTemp))
				{
					for (int k = i; k < j - 1; k++)
					{
						SwapInstructions(ins, mInstructions[k + 1]);
						mInstructions[k] = mInstructions[k + 1];
					}
					mInstructions[j - 1] = ins;
				}
			}

		}
#endif
		// Check complex comparison, that may be simpler when using operands
		if (mFalseJump && mInstructions.Size() >= 3)
		{
			int nins = mInstructions.Size();
			if (mInstructions[nins - 1]->mCode == IC_BRANCH && mInstructions[nins - 1]->mSrc[0].mFinal &&
				mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
				(mInstructions[nins - 2]->mOperator == IA_CMPEQ || mInstructions[nins - 2]->mOperator == IA_CMPNE))
			{
				InterInstruction* rins = mInstructions[nins - 2];
				InterInstruction* ains = mInstructions[nins - 3];

				if (rins->mSrc[0].mTemp < 0 && ains->mDst.mTemp == rins->mSrc[1].mTemp)
				{
					if (rins->mSrc[0].mType == IT_POINTER && rins->mSrc[1].mType == IT_POINTER &&
						rins->mSrc[0].mMemory == rins->mSrc[1].mMemoryBase &&
						ains->mCode == IC_LEA &&
						ains->mSrc[1].mTemp < 0 && SameMem(ains->mSrc[1], rins->mSrc[0]))
					{
						rins->mSrc[1] = ains->mSrc[1];
						ains->mSrc[1].mFinal = false;
						rins->mSrc[0].mType = IT_INT16;
						rins->mSrc[0].mIntConst = 0;
					}
				}
				else if (rins->mSrc[1].mTemp < 0 && ains->mDst.mTemp == rins->mSrc[0].mTemp)
				{
					if (rins->mSrc[0].mType == IT_POINTER && rins->mSrc[1].mType == IT_POINTER &&
						rins->mSrc[1].mMemory == rins->mSrc[0].mMemoryBase &&
						ains->mCode == IC_LEA &&
						ains->mSrc[1].mTemp < 0 && SameMem(ains->mSrc[1], rins->mSrc[1]))
					{
						rins->mSrc[0] = ains->mSrc[0];
						ains->mSrc[0].mFinal = false;
						rins->mSrc[1].mType = IT_INT16;
						rins->mSrc[1].mIntConst = 0;
					}
					else if (rins->mSrc[0].mType == IT_INT16 &&
						rins->mSrc[0].IsInRange(0, 65534) &&
						ains->mCode == IC_BINARY_OPERATOR && ains->mOperator == IA_MUL)
					{
						if (ains->mSrc[0].mTemp < 0 && rins->mSrc[1].mIntConst % ains->mSrc[0].mIntConst == 0)
						{
							rins->mSrc[0] = ains->mSrc[1];
							ains->mSrc[1].mFinal = false;
							rins->mSrc[1].mIntConst /= ains->mSrc[0].mIntConst;
						}
						else if (ains->mSrc[1].mTemp < 0 && rins->mSrc[1].mIntConst % ains->mSrc[1].mIntConst == 0)
						{
							rins->mSrc[0] = ains->mSrc[0];
							ains->mSrc[0].mFinal = false;
							rins->mSrc[1].mIntConst /= ains->mSrc[1].mIntConst;
						}
					}
				}
			}
		}

		// Check case of cmp signed immediate
		if (mFalseJump && mInstructions.Size() > 3)
		{
			int nins = mInstructions.Size();
			if (mInstructions[nins - 1]->mCode == IC_BRANCH &&
				mInstructions[nins - 2]->mCode == IC_RELATIONAL_OPERATOR && mInstructions[nins - 2]->mDst.mTemp == mInstructions[nins - 1]->mSrc[0].mTemp &&
				mInstructions[nins - 2]->mOperator == IA_CMPLS && mInstructions[nins - 2]->mSrc[0].mTemp < 0)
			{
				int j = nins - 2;
				while (j >= 0 && mInstructions[j]->mDst.mTemp != mInstructions[nins - 2]->mSrc[1].mTemp)
					j--;
				if (j >= 0 && mInstructions[j]->mCode == IC_LOAD_TEMPORARY)
				{
					int si = mInstructions[j]->mSrc[0].mTemp, di = mInstructions[j]->mDst.mTemp, ioffset = 0;

					InterInstruction* ains = nullptr;

					int k = j + 1;
					while (k < nins - 2)
					{
						InterInstruction* ins = mInstructions[k];
						if (ins->mDst.mTemp == si)
						{
							if (ins->mCode == IC_BINARY_OPERATOR && ins->mOperator == IA_ADD && ins->mSrc[0].mTemp < 0 && ins->mSrc[1].mTemp == si)
							{
								ioffset += int(ins->mSrc[0].mIntConst);
								ains = ins;
							}
							else
								break;
						}

						k++;
					}

					if (k == nins - 2)
					{
						if (ains)
						{
							mInstructions[nins - 2]->mSrc[1] = ains->mDst;
							mInstructions[nins - 2]->mSrc[0].mIntConst += ioffset;
						}
					}
				}
			}
		}

#if 1
		// Check orientation of float loop around
		if (mLoopHead && (mTrueJump == this || mFalseJump == this))
		{
			int fi = 0;
			while (fi < mInstructions.Size() && !(mInstructions[fi]->mDst.mTemp >= 0 && mInstructions[fi]->mDst.mType == IT_FLOAT))
				fi++;
			if (fi < mInstructions.Size() && mInstructions[fi]->mCode == IC_BINARY_OPERATOR &&
				(mInstructions[fi]->mOperator == IA_ADD || mInstructions[fi]->mOperator == IA_MUL) &&
				mInstructions[fi]->mSrc[0].mTemp >= 0 && mInstructions[fi]->mSrc[1].mTemp >= 0)
			{
				int fj = mInstructions.Size() - 1;
				while (fj >= fi && !(mInstructions[fj]->mDst.mTemp >= 0 && mInstructions[fj]->mDst.mType == IT_FLOAT))
					fj--;

				if (fj >= fi)
				{
					if (mInstructions[fi]->mSrc[1].mTemp == mInstructions[fj]->mDst.mTemp)
					{
						mInstructions[fi]->mLockOrder = true;
					}
					else if (mInstructions[fi]->mSrc[0].mTemp == mInstructions[fj]->mDst.mTemp)
					{
						mInstructions[fi]->mLockOrder = true;
						InterOperand	op = mInstructions[fi]->mSrc[0];
						mInstructions[fi]->mSrc[0] = mInstructions[fi]->mSrc[1];
						mInstructions[fi]->mSrc[1] = op;
					}
					else
						mInstructions[fi]->mLockOrder = false;
				}
			}
		}
#endif
		CheckFinalLocal();

		do {} while (PeepholeReplaceOptimization(staticVars, staticProcs));

		CheckFinalLocal();

		if (mTrueJump) mTrueJump->PeepholeOptimization(staticVars, staticProcs);
		if (mFalseJump) mFalseJump->PeepholeOptimization(staticVars, staticProcs);
	}
}

void InterCodeBasicBlock::CheckNullptrDereference(void) 
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mCode == IC_LOAD && ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_ABSOLUTE && ins->mSrc[0].mIntConst == 0 && !ins->mVolatile)
				mProc->mModule->mErrors->Error(ins->mLocation, EWARN_NULL_POINTER_DEREFERENCED, "nullptr dereferenced");
			else if (ins->mCode == IC_STORE && ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_ABSOLUTE && ins->mSrc[1].mIntConst == 0 && !ins->mVolatile)
				mProc->mModule->mErrors->Error(ins->mLocation, EWARN_NULL_POINTER_DEREFERENCED, "nullptr dereferenced");
		}

		if (mTrueJump) mTrueJump->CheckNullptrDereference();
		if (mFalseJump) mFalseJump->CheckNullptrDereference();
	}
}

void InterCodeBasicBlock::CheckValueReturn(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
			if (ins->mCode == IC_ASSEMBLER || ins->mCode == IC_DISPATCH)
				return;
			else if (ins->mCode == IC_RETURN)
				mProc->mModule->mErrors->Error(ins->mLocation, EWARN_MISSING_RETURN_STATEMENT, "Missing return statement");
		}

		if (mTrueJump) mTrueJump->CheckValueReturn();
		if (mFalseJump) mFalseJump->CheckValueReturn();
	}
}

void InterCodeBasicBlock::MarkAliasing(const NumberSet& aliasedParams)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			if (ins->mCode == IC_LOAD || ins->mCode == IC_COPY)
			{
				if ((ins->mSrc[0].mMemory == IM_PARAM || ins->mSrc[0].mMemory == IM_FPARAM) && aliasedParams[ins->mSrc[0].mVarIndex])
					ins->mAliasing = true;
			}

			if (ins->mCode == IC_STORE || ins->mCode == IC_FILL || ins->mCode == IC_COPY)
			{
				if ((ins->mSrc[1].mMemory == IM_PARAM || ins->mSrc[1].mMemory == IM_FPARAM) && aliasedParams[ins->mSrc[1].mVarIndex])
					ins->mAliasing = true;
			}
		}

		if (mTrueJump) mTrueJump->MarkAliasing(aliasedParams);
		if (mFalseJump) mFalseJump->MarkAliasing(aliasedParams);
	}
}

void InterCodeBasicBlock::CollectGlobalReferences(NumberSet& referencedGlobals, NumberSet& modifiedGlobals, bool& storesIndirect, bool& loadsIndirect, bool& globalsChecked)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			switch (ins->mCode)
			{
			case IC_LOAD:
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL && ins->mSrc[0].mVarIndex >= 0)
					referencedGlobals += ins->mSrc[0].mVarIndex;
				else if (ins->mSrc[0].mMemoryBase == IM_GLOBAL && ins->mSrc[0].mVarIndex >= 0)
					referencedGlobals += ins->mSrc[0].mVarIndex;
				else if (ins->mSrc[0].mTemp >= 0 && (ins->mSrc[0].mMemoryBase == IM_NONE || ins->mSrc[0].mMemoryBase == IM_INDIRECT))
					loadsIndirect = true;
				break;
			case IC_STORE:
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mMemoryBase == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mTemp >= 0 && (ins->mSrc[1].mMemoryBase == IM_NONE || ins->mSrc[1].mMemoryBase == IM_INDIRECT))
					storesIndirect = true;
				break;
			case IC_FILL:
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mMemoryBase == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mTemp >= 0 && (ins->mSrc[1].mMemoryBase == IM_NONE || ins->mSrc[1].mMemoryBase == IM_INDIRECT))
					storesIndirect = true;
				break;
			case IC_COPY:
			case IC_STRCPY:
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mMemory == IM_GLOBAL && ins->mSrc[0].mVarIndex >= 0)
					referencedGlobals += ins->mSrc[0].mVarIndex;
				else if (ins->mSrc[0].mMemoryBase == IM_GLOBAL && ins->mSrc[0].mVarIndex >= 0)
					referencedGlobals += ins->mSrc[0].mVarIndex;
				else if (ins->mSrc[0].mTemp >= 0 && (ins->mSrc[0].mMemoryBase == IM_NONE || ins->mSrc[0].mMemoryBase == IM_INDIRECT))
					loadsIndirect = true;
				if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mMemory == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mMemoryBase == IM_GLOBAL && ins->mSrc[1].mVarIndex >= 0)
				{
					referencedGlobals += ins->mSrc[1].mVarIndex;
					modifiedGlobals += ins->mSrc[1].mVarIndex;
				}
				else if (ins->mSrc[1].mTemp >= 0 && (ins->mSrc[1].mMemoryBase == IM_NONE || ins->mSrc[1].mMemoryBase == IM_INDIRECT))
					storesIndirect = true;
				break;
			case IC_ASSEMBLER:
				for (int i = 1; i < ins->mNumOperands; i++)
				{
					if (ins->mSrc[i].mType == IT_POINTER && ins->mSrc[i].mTemp < 0)
					{
						storesIndirect = true;
						loadsIndirect = true;
					}
				}
				break;
			case IC_CALL:
			case IC_CALL_NATIVE:
				if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mLinkerObject && ins->mSrc[0].mLinkerObject->mProc)
				{
					InterCodeProcedure* proc = ins->mSrc[0].mLinkerObject->mProc;
					if (proc->mGlobalsChecked)
					{
						if (proc->mStoresIndirect)
							storesIndirect = true;
						if (proc->mLoadsIndirect)
							loadsIndirect = true;
						referencedGlobals |= proc->mReferencedGlobals;
						modifiedGlobals |= proc->mModifiedGlobals;
					}
					else
						globalsChecked = false;
				}
				else
					globalsChecked = false;
				break;
			case IC_DISPATCH:
				{
					for (int j = 0; j < mProc->mCalledFunctions.Size(); j++)
					{
						InterCodeProcedure* proc = mProc->mCalledFunctions[j];

						if (proc->mGlobalsChecked)
						{
							if (proc->mStoresIndirect)
								storesIndirect = true;
							if (proc->mLoadsIndirect)
								loadsIndirect = true;
							referencedGlobals |= proc->mReferencedGlobals;
							modifiedGlobals |= proc->mModifiedGlobals;
						}
						else
							globalsChecked = false;
					}
				}
				break;
			}
		}

		if (mTrueJump) mTrueJump->CollectGlobalReferences(referencedGlobals, modifiedGlobals, storesIndirect, loadsIndirect, globalsChecked);
		if (mFalseJump) mFalseJump->CollectGlobalReferences(referencedGlobals, modifiedGlobals, storesIndirect, loadsIndirect, globalsChecked);
	}
}

void InterCodeBasicBlock::WarnInvalidValueRanges(void)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];

			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0 && ins->mSrc[j].mRange.IsInvalid())
				{
					mProc->mModule->mErrors->Error(ins->mLocation, EWARN_INVALID_VALUE_RANGE, "Invalid value range");
				}
			}
		}

		if (mTrueJump) mTrueJump->WarnInvalidValueRanges();
		if (mFalseJump) mFalseJump->WarnInvalidValueRanges();
	}
}


void InterCodeBasicBlock::WarnUsedUndefinedVariables(void)
{
	if (!mVisited)
	{
		mVisited = true;

		NumberSet	providedTemps(mEntryProvidedTemps), potentialTemps(mEntryPotentialTemps);

		for (int i = 0; i < mInstructions.Size(); i++)
		{
			InterInstruction* ins = mInstructions[i];
	
			for (int j = 0; j < ins->mNumOperands; j++)
			{
				if (ins->mSrc[j].mTemp >= 0 && !providedTemps[ins->mSrc[j].mTemp])
				{
					int t = ins->mSrc[j].mTemp;

					int k = 0;
					while (k < mProc->mLocalVars.Size() && !(mProc->mLocalVars[k] && mProc->mLocalVars[k]->mTempIndex == t))
						k++;

					if (potentialTemps[t])
					{
						if (k < mProc->mLocalVars.Size() && mProc->mLocalVars[k]->mIdent)
							mProc->mModule->mErrors->Error(ins->mLocation, EWARN_USE_OF_UNINITIALIZED_VARIABLE, "Use of potentially uninitialized variable", mProc->mLocalVars[k]->mIdent);
						else
							mProc->mModule->mErrors->Error(ins->mLocation, EWARN_USE_OF_UNINITIALIZED_VARIABLE, "Use of potentially uninitialized expression");
					}
					else
					{
						if (k < mProc->mLocalVars.Size() && mProc->mLocalVars[k]->mIdent)
							mProc->mModule->mErrors->Error(ins->mLocation, EWARN_USE_OF_UNINITIALIZED_VARIABLE, "Use of uninitialized variable", mProc->mLocalVars[k]->mIdent);
						else
							mProc->mModule->mErrors->Error(ins->mLocation, EWARN_USE_OF_UNINITIALIZED_VARIABLE, "Use of uninitialized expression");

						if (ins->mCode == IC_LOAD_TEMPORARY)
						{
							ins->mCode = IC_CONSTANT;
							ins->mConst = ins->mSrc[j];
							ins->mConst.mTemp = -1;
							ins->mConst.mIntConst = 0;
							ins->mConst.mLinkerObject = nullptr;
							ins->mConst.mVarIndex = -1;
							ins->mConst.mMemory = IM_ABSOLUTE;
							ins->mNumOperands = 0;
						}
						else
						{
							ins->mSrc[j].mTemp = -1;
							ins->mSrc[j].mIntConst = 0;
						}
					}
				}
			}

			if (ins->mDst.mTemp >= 0)
				providedTemps += ins->mDst.mTemp;
		}

		if (mTrueJump) mTrueJump->WarnUsedUndefinedVariables();
		if (mFalseJump) mFalseJump->WarnUsedUndefinedVariables();
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

					int	size = int(ins->mConst.mOperandSize + ins->mConst.mIntConst);
					if (size > localVars[varIndex]->mSize)
						localVars[varIndex]->mSize = size;
					localVars[varIndex]->mAliased = true;
				}
				else if (ins->mConst.mMemory == paramMemory)
				{
					int varIndex = ins->mConst.mVarIndex;
					if (!paramVars[varIndex])
						paramVars[varIndex] = new InterVariable;

					int	size = int(ins->mConst.mOperandSize + ins->mConst.mIntConst);
					if (size > paramVars[varIndex]->mSize)
						paramVars[varIndex]->mSize = size;
					paramVars[varIndex]->mAliased = true;
				}
				break;

			case IC_LEA:
				if (ins->mSrc[1].mMemory == IM_LOCAL)
				{
					int varIndex = ins->mSrc[1].mVarIndex;
					if (!localVars[varIndex])
						localVars[varIndex] = new InterVariable;

					int	size = int(ins->mSrc[1].mOperandSize + ins->mSrc[1].mIntConst);
					if (size > localVars[varIndex]->mSize)
						localVars[varIndex]->mSize = size;
					localVars[varIndex]->mAliased = true;
				}
				else if (ins->mSrc[1].mMemory == paramMemory)
				{
					int varIndex = ins->mSrc[1].mVarIndex;
					if (!paramVars[varIndex])
						paramVars[varIndex] = new InterVariable;

					int	size = int(ins->mSrc[1].mOperandSize + ins->mSrc[1].mIntConst);
					if (size > paramVars[varIndex]->mSize)
						paramVars[varIndex]->mSize = size;
					paramVars[varIndex]->mAliased = true;
				}
				break;

			case IC_STORE:
			case IC_LOAD:		
			case IC_COPY:
			case IC_FILL:
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

						int	size = int(ins->mSrc[j].mOperandSize + ins->mSrc[j].mIntConst);
						if (size > localVars[varIndex]->mSize)
							localVars[varIndex]->mSize = size;
					}
					else if (ins->mSrc[j].mMemory == IM_FPARAM || ins->mSrc[j].mMemory == IM_PARAM)
					{
						int varIndex = ins->mSrc[j].mVarIndex;
						if (!paramVars[varIndex])
							paramVars[varIndex] = new InterVariable;

						int	size = int(ins->mSrc[j].mOperandSize + ins->mSrc[j].mIntConst);
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
//		GrowingIntegerValueRangeArray	localValueRange(mLocalValueRange);
//		GrowingIntegerValueRangeArray	reverseValueRange(mReverseValueRange);
		GrowingArray<int64>				memoryValueSize(mMemoryValueSize);

		mEntryValueRange.SetSize(set.Num(), true);
		mTrueValueRange.SetSize(set.Num(), true);
		mFalseValueRange.SetSize(set.Num(), true);
//		mLocalValueRange.SetSize(set.Num(), true);
//		mReverseValueRange.SetSize(set.Num(), true);
		mMemoryValueSize.SetSize(set.Num(), true);

		for (int i = 0; i < set.Num(); i++)
		{
			int j = set.Element(i);
			if (j < entryValueRange.Size())
			{
				mEntryValueRange[i] = entryValueRange[j];
				mTrueValueRange[i] = trueValueRange[j];
				mFalseValueRange[i] = falseValueRange[j];
				//			mLocalValueRange[i] = localValueRange[j];
				//			mReverseValueRange[i] = reverseValueRange[j];
				mMemoryValueSize[i] = memoryValueSize[j];
			}
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
		
		fprintf(file, "L%d: <= D%d: (%d) %s P%d", mIndex, (mDominator ? mDominator->mIndex : -1), mNumEntries, s, (mLoopPrefix ? mLoopPrefix->mIndex : -1));
		if (mInstructions.Size())
			fprintf(file, " %s\n", mInstructions[0]->mLocation.mFileName);
		else
			fprintf(file, "\n");

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
			fprintf(file, "Exit  required temps : ");
			for (i = 0; i < mExitRequiredTemps.Size(); i++)
			{
				if (mExitRequiredTemps[i])
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
				fprintf(file, "%04x (%4d)\t", i, mInstructions[i]->mLocation.mLine);
				mInstructions[i]->Disassemble(file, mProc);
			}
		}

		if (mTrueJump) fprintf(file, "\t\t==> %d\n", mTrueJump->mIndex);
		if (mFalseJump) fprintf(file, "\t\t==> %d\n", mFalseJump->mIndex);

		if (mTrueJump) mTrueJump->Disassemble(file, dumpSets);
		if (mFalseJump) mFalseJump->Disassemble(file, dumpSets);
	}
}


void InterCodeBasicBlock::WarnUnreachable(void)
{
	if (mNumEntries == 0 && mProc->mCheckUnreachable)
	{
		int i = 0;
		while (i < mInstructions.Size() && !IsObservable(mInstructions[i]->mCode))
			i++;
		if (i < mInstructions.Size())
			mProc->mModule->mErrors->Error(mInstructions[i]->mLocation, EWARN_UNREACHABLE_CODE, "Unreachable code");
	}
}



InterCodeProcedure::InterCodeProcedure(InterCodeModule * mod, const Location & location, const Ident* ident, LinkerObject * linkerObject)
	: mTemporaries(IT_NONE), mBlocks(nullptr), mLocation(location), mTempOffset(-1), mTempSizes(0), mNumBlocks(0),
	mRenameTable(-1), mRenameUnionTable(-1), mGlobalRenameTable(-1),
	mValueForwardingTable(nullptr), mLocalVars(nullptr), mParamVars(nullptr), mModule(mod),
	mIdent(ident), mLinkerObject(linkerObject),
	mNativeProcedure(false), mLeafProcedure(false), mCallsFunctionPointer(false), mCalledFunctions(nullptr), mFastCallProcedure(false), 
	mInterrupt(false), mHardwareInterrupt(false), mCompiled(false), mInterruptCalled(false), mDynamicStack(false), mAssembled(false),
	mSaveTempsLinkerObject(nullptr), mValueReturn(false), mFramePointer(false),
	mCheckUnreachable(true), mReturnType(IT_NONE), mCheapInline(false), mNoInline(false),
	mDeclaration(nullptr), mGlobalsChecked(false), mDispatchedCall(false),
	mIntrinsicFunction(false),
	mNumRestricted(1)
{
	mID = mModule->mProcedures.Size();
	mModule->mProcedures.Push(this);
	mLinkerObject->mProc = this;
	mLinkerObject->mFlags |= LOBJF_CONST;
	mCallerSavedTemps = BC_REG_TMP_SAVED - BC_REG_TMP;
}

InterCodeProcedure::~InterCodeProcedure(void)
{
}

void InterCodeProcedure::ResetEntryBlocks(void)
{
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mEntryBlocks.SetSize(0);
}

void InterCodeProcedure::ResetPatched(void)
{
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mPatched = false;
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

int InterCodeProcedure::AddTemporary(InterType type)
{
	assert(type != IT_NONE);

	int	temp = mTemporaries.Size();
	mTemporaries.Push(type);
	return temp;
}

int InterCodeProcedure::AddRestricted(void)
{
	return mNumRestricted++;
}

void InterCodeProcedure::CheckBlocks(void)
{
	ResetVisited();
	mEntryBlock->CheckBlocks();
}

bool InterCodeProcedure::ReplaceByteIndexPointers(FastNumberSet& activeSet)
{
	activeSet.Clear();

	ResetVisited();
	mEntryBlock->CollectActiveTemporaries(activeSet);

	int		silvused = activeSet.Num();
	if (silvused != mTemporaries.Size())
	{
		mTemporaries.SetSize(silvused, true);

		ResetVisited();
		mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

		mLocalValueRange.SetSize(silvused, true);

		ResetVisited();
		mEntryBlock->RemapActiveTemporaries(activeSet);
	}

	NumberSet			invtemps(silvused), inctemps(silvused);
	GrowingIntArray		vartemps(-1);

	ResetVisited();
	mEntryBlock->CollectByteIndexPointers(invtemps, inctemps, vartemps);
	inctemps -= invtemps;
#if 0
	for (int i = 0; i < silvused; i++)
	{
		if (inctemps[i])
		{
			if (mModule->mGlobalVars[vartemps[i]]->mIdent)
				printf("Inctemp %d, %s[%d]\n", i, mModule->mGlobalVars[vartemps[i]]->mIdent->mString, mModule->mGlobalVars[vartemps[i]]->mSize);
			else
				printf("Inctemp %d, V%d[%d]\n", i, vartemps[i], mModule->mGlobalVars[vartemps[i]]->mSize);
		}
	}
#endif
	ResetVisited();
	if (mEntryBlock->ReplaceByteIndexPointers(inctemps, vartemps, silvused))
	{
		activeSet.Clear();

		ResetVisited();
		mEntryBlock->CollectActiveTemporaries(activeSet);

		silvused = activeSet.Num();
		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			mLocalValueRange.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);
		}
	}

	return false;
}

void InterCodeProcedure::ForwardSingleAssignmentBools(void)
{
	int	numTemps = mTemporaries.Size();

	FastNumberSet					tassigned(numTemps);
	GrowingInstructionPtrArray		tvalues(nullptr);

	tvalues.SetSize(numTemps);

	ResetVisited();
	mEntryBlock->CollectSingleAssignmentBools(tassigned, tvalues);

	ResetVisited();
	mEntryBlock->CheckSingleAssignmentBools(tvalues);

	for (int i = 0; i < tassigned.Num(); i++)
	{
		int t = tassigned.Element(i);
		if (tvalues[t])
		{
			tvalues[t]->mCode = IC_LOAD_TEMPORARY;
			tvalues[t]->mNumOperands = 1;
			if (tvalues[t]->mSrc[0].mTemp >= 0)
				tvalues[t]->mDst = tvalues[t]->mSrc[0];
			else
			{
				tvalues[t]->mDst = tvalues[t]->mSrc[1];
				tvalues[t]->mSrc[0] = tvalues[t]->mSrc[1];
			}
			tvalues[t]->mDst.mTemp = t;
		}
	}

	ResetVisited();
	mEntryBlock->MapSingleAssignmentBools(tvalues);

	DisassembleDebug("ForwardSingleAssignmentBools");

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();
}

void InterCodeProcedure::StructReturnPropagation(void)
{
	if (!mEntryBlock->mTrueJump)
	{
		mEntryBlock->StructReturnPropagation();
	}
}

void InterCodeProcedure::CollapseDispatch(void)
{
	ResetVisited();
	mEntryBlock->CollapseDispatch();
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

void InterCodeProcedure::RebuildIntegerRangeSet(void)
{
	mLocalValueRange.SetSize(mTemporaries.Size(), false);

	ExpandingInstructionPtrArray	tarr;
	ResetVisited();
	mEntryBlock->PropagateValueRangeSetConversions(tarr);

	DisassembleDebug("BeforeRebuildIntegerRangeSet");

	ResetVisited();
	mEntryBlock->RestartLocalIntegerRangeSets(mTemporaries.Size());

	// No need to re-init the loop specific parts, we are restarting.
	// Would lead to infinite pumping weak - bound in some cases
#if 1
	int limit = 10;
	do {
		DisassembleDebug("tr0");

		limit--;
		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(true) && limit > 0);
#endif
	do {
		DisassembleDebug("tr1");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(false));

	assert(mTemporaries.Size() == mLocalValueRange.Size());

	DisassembleDebug("Estimated value range 2");
}

void InterCodeProcedure::EarlyBranchElimination(void)
{
	GrowingInstructionPtrArray	 ctemps(nullptr);

	ResetVisited();
	while (mEntryBlock->EarlyBranchElimination(ctemps))
	{
		BuildTraces(10);
		TrimBlocks();
	}
}

void InterCodeProcedure::BuildTraces(int expand, bool dominators, bool compact)
{
	// Count number of entries
	//
	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
	{
		mBlocks[i]->mNumEntries = 0;
		mBlocks[i]->mLoopHead = false;
		mBlocks[i]->mLoopDebug = false;
		mBlocks[i]->mTraceIndex = -1;
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

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mDominator = nullptr;
	if (dominators)
		mEntryBlock->BuildDominatorTree(nullptr);
}

void InterCodeProcedure::TrimBlocks(void)
{
	int j = 1;
	for (int i = 1; i < mBlocks.Size(); i++)
	{
		InterCodeBasicBlock* block = mBlocks[i];
		if (block->mNumEntries > 0)
		{
			if (block->mLoopPrefix && block->mLoopPrefix->mNumEntries == 0)
				block->mLoopPrefix = nullptr;
			if (block->mDominator && block->mDominator->mNumEntries == 0)
				block->mDominator = nullptr;

			block->mIndex = j;
			mBlocks[j++] = block;
		}
		else
			delete block;
	}
	mBlocks.SetSize(j, false);
	mNumBlocks = j;
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
	// Build set of globally provided temporaries
	//
	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps), NumberSet(numTemps));

	//
	// Build set of globally required temporaries, might need
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
	// First locally rename all temporaries
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

	DisassembleDebug("single assignment forwarding");

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

}

void InterCodeProcedure::PeepholeOptimization(void)
{
	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	Disassemble("Precheck Final");
	CheckFinal();

	ResetVisited();
	mEntryBlock->PeepholeOptimization(mModule->mGlobalVars, mModule->mProcedures);

	Disassemble("PeepholeOptimization");
	CheckFinal();
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

void InterCodeProcedure::ShortcutConstBranches(void)
{
	GrowingInstructionPtrArray	cins(nullptr);

	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	do {
		Disassemble("PreShortcutConstBranches");
		ResetVisited();
	} while (mEntryBlock->ShortcutConstBranches(cins));

	Disassemble("ShortcutConstBranches");
}

void InterCodeProcedure::ShortcutDuplicateBranches(void)
{
	BuildDataFlowSets();
	TempForwarding();
	RemoveUnusedInstructions();

	do {
		ResetVisited();
	} while (mEntryBlock->ShortcutDuplicateBranches());

	Disassemble("ShortcutDuplicateBranches");
}


void InterCodeProcedure::MoveConditionsOutOfLoop(void)
{
	BuildTraces(0);
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	Disassemble("PreMoveConditionOutOfLoop");

	ResetVisited();
	while (mEntryBlock->MoveConditionOutOfLoop())
	{
		Disassemble("MoveConditionOutOfLoop");

		BuildTraces(0);

		BuildDataFlowSets();
		TempForwarding();
		RemoveUnusedInstructions();

		BuildTraces(0);
		BuildLoopPrefix();
		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);

		ResetVisited();
		mEntryBlock->InnerLoopOptimization(mParamAliasedSet);

		Disassemble("PostMoveConditionOutOfLoop");

		ResetVisited();
	}
}

void InterCodeProcedure::EliminateDoubleLoopCounter(void)
{
	BuildTraces(0);
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	mEntryBlock->EliminateDoubleLoopCounter();
}

void InterCodeProcedure::LimitLoopIndexIntegerRangeSets(void)
{
	BuildTraces(0);
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	mEntryBlock->LimitLoopIndexIntegerRangeSets();
}

void InterCodeProcedure::PropagateMemoryAliasingInfo(bool loops)
{
	GrowingInstructionPtrArray	tvalue(nullptr);

	if (loops)
	{
		BuildTraces(0);
		BuildLoopPrefix();
		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);
	}

	ResetVisited();
	mEntryBlock->PropagateMemoryAliasingInfo(tvalue, loops);

	Disassemble("PropagateMemoryAliasingInfo");
}


void InterCodeProcedure::WarnUsedUndefinedVariables(void)
{
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	mEntryBlock->WarnUsedUndefinedVariables();
}

void InterCodeProcedure::WarnInvalidValueRanges(void)
{
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	mEntryBlock->WarnInvalidValueRanges();
}


void InterCodeProcedure::TempForwarding(bool reverse, bool checkloops)
{
	int	numTemps = mTemporaries.Size();

	CheckUsedDefinedTemps();

	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps);

	if (checkloops)
	{
		BuildLoopPrefix();

		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);
	}

	DisassembleDebug("pre temp forwarding");

	//
	// Now remove needless temporary moves, that appear due to
	// stack evaluation
	//
	mTempForwardingTable.SetSize(numTemps);

	mTempForwardingTable.Reset();
	ResetVisited();
	mEntryBlock->PerformTempForwarding(mTempForwardingTable, reverse, checkloops);
	
	if (checkloops)
		DisassembleDebug("loop temp forwarding");
	else
		DisassembleDebug("temp forwarding");
}


void InterCodeProcedure::RemoveUnusedInstructions(void)
{
	int	numTemps = mTemporaries.Size();

	do {
		ResetVisited();
		mEntryBlock->BuildLocalTempSets(numTemps);

		ResetVisited();
		mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps), NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (mEntryBlock->RemoveUnusedResultInstructions());
}

void InterCodeProcedure::RemoveUnusedLocalStoreInstructions(void)
{
	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		ResetVisited();
		mEntryBlock->RemoveUnusedLocalStoreInstructions();
	}
}

void InterCodeProcedure::RemoveUnusedPartialStoreInstructions(void)
{
	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		if (mModule->mGlobalVars.Size())
		{
			int	byteIndex = 0;
			for (int i = 0; i < mModule->mGlobalVars.Size(); i++)
			{
				if (mModule->mGlobalVars[i])
				{
					mModule->mGlobalVars[i]->mByteIndex = byteIndex;
					byteIndex += mModule->mGlobalVars[i]->mSize;
				}
			}

			do {
				ResetVisited();
				mEntryBlock->BuildStaticVariableByteSet(mModule->mGlobalVars, byteIndex);

				ResetVisited();
				mEntryBlock->BuildGlobalProvidedStaticVariableSet(mModule->mGlobalVars, NumberSet(byteIndex));

				NumberSet	totalRequired2(byteIndex);

				do {
					ResetVisited();
				} while (mEntryBlock->BuildGlobalRequiredStaticVariableSet(mModule->mGlobalVars, totalRequired2));

				ResetVisited();
			} while (mEntryBlock->RemoveUnusedStaticStoreByteInstructions(mModule->mGlobalVars, byteIndex));

			DisassembleDebug("removed unused static byte stores");
		}
	}
}

void InterCodeProcedure::RemoveUnusedStoreInstructions(InterMemory	paramMemory)
{
	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		if (mLocalVars.Size() > 0 || mParamVars.Size() > 0)
		{
#if 0
			for (int i = 0; i < mLocalAliasedSet.Size(); i++)
			{
				if (mLocalVars[i])
					mLocalVars[i]->mAliased = mLocalAliasedSet[i];
			}
			for (int i = 0; i < mParamAliasedSet.Size(); i++)
			{
				if (mParamVars[i])
					mParamVars[i]->mAliased = mParamAliasedSet[i];
			}
#endif
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

		// Remove unused indirect stores

		ResetVisited();
		mEntryBlock->RemoveUnusedIndirectStoreInstructions();

		DisassembleDebug("RemoveUnusedIndirectStoreInstructions");
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

void InterCodeProcedure::CollectVariables(InterMemory paramMemory)
{
	for (int i = 0; i < mLocalVars.Size(); i++)
		if (mLocalVars[i])
			mLocalVars[i]->mAliased = false;
	for (int i = 0; i < mParamVars.Size(); i++)
		if (mParamVars[i])
			mParamVars[i]->mAliased = false;

	ResetVisited();
	mEntryBlock->CollectVariables(mModule->mGlobalVars, mLocalVars, mParamVars, paramMemory);
}

void InterCodeProcedure::PromoteSimpleLocalsToTemp(InterMemory paramMemory, int nlocals, int nparams)
{
	for (int j = 0; j < 2; j++)
	{
		CollectVariables(paramMemory);

		RemoveUnusedStoreInstructions(paramMemory);

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
				mLocalVars[vi]->mTempIndex = AddTemporary(localTypes[vi]);
				ResetVisited();
				mEntryBlock->SimpleLocalToTemp(vi, mLocalVars[vi]->mTempIndex);
			}
		}

		DisassembleDebug("local variables to temps");

		BuildTraces(0);

		BuildDataFlowSets();

		WarnUsedUndefinedVariables();

		RenameTemporaries();

		DisassembleDebug("PreGlobalConstantPropagation");

		do {
			BuildDataFlowSets();

			WarnUsedUndefinedVariables();

			DisassembleDebug("WarnUsedUndefinedVariables");

			TempForwarding();
		} while (GlobalConstantPropagation());

		DisassembleDebug("GlobalConstantPropagation");

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

	ResetVisited();
	mEntryBlock->SimplifyPointerOffsets();

	DisassembleDebug("SimplifyPointerOffsets");
}

void InterCodeProcedure::SingleBlockLoopPointerToByte(FastNumberSet& activeSet)
{
	int							silvused = mTemporaries.Size();

	do
	{
		mTemporaries.SetSize(silvused, true);

		DisassembleDebug("SingleBlockLoopPointerToByteA");

		BuildDataFlowSets();

		DisassembleDebug("SingleBlockLoopPointerToByteB");

		TempForwarding();
		RemoveUnusedInstructions();

		DisassembleDebug("SingleBlockLoopPointerToByteC");

		activeSet.Clear();

		ResetVisited();
		mEntryBlock->CollectActiveTemporaries(activeSet);

		silvused = activeSet.Num();
		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			mLocalValueRange.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);
		}

		ResetVisited();
	} while (mEntryBlock->SingleBlockLoopPointerToByte(silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("SingleBlockLoopPointerToByte");


}


void InterCodeProcedure::SingleBlockLoopPointerSplit(FastNumberSet& activeSet)
{
	int							silvused = mTemporaries.Size();

	do
	{
		mTemporaries.SetSize(silvused, true);

		DisassembleDebug("SingleBlockLoopPointerSplitA");

		BuildDataFlowSets();

		DisassembleDebug("SingleBlockLoopPointerSplitB");

		TempForwarding();
		RemoveUnusedInstructions();

		DisassembleDebug("SingleBlockLoopPointerSplitC");

		activeSet.Clear();

		ResetVisited();
		mEntryBlock->CollectActiveTemporaries(activeSet);

		silvused = activeSet.Num();
		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);
		}

		ResetVisited();
	} while (mEntryBlock->SingleBlockLoopPointerSplit(silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("SingleBlockLoopPointerSplit");

}


void InterCodeProcedure::SingleBlockLoopSinking(FastNumberSet& activeSet)
{
	int							silvused = mTemporaries.Size();

	ResetVisited();
	mEntryBlock->BuildLoopSuffix();

	do
	{
		mTemporaries.SetSize(silvused, true);

		DisassembleDebug("SingleBlockLoopSinkingA");

		BuildDataFlowSets();

		DisassembleDebug("SingleBlockLoopSinkingB");

		TempForwarding();
		RemoveUnusedInstructions();

		DisassembleDebug("SingleBlockLoopSinkingC");

		activeSet.Clear();

		ResetVisited();
		mEntryBlock->CollectActiveTemporaries(activeSet);

		silvused = activeSet.Num();
		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);
		}

		ResetVisited();
	} while (mEntryBlock->SingleBlockLoopSinking(silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("SingleBlockLoopSinking");

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

		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);
		}

		DisassembleDebug("PreSimplifyIntegerNumeric");

		ResetVisited();

	} while (mEntryBlock->SimplifyIntegerNumeric(silvalues, silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("SimplifyIntegerNumeric");
}

void InterCodeProcedure::ExpandSelect(void)
{
#if 1
	ResetVisited();
	mEntryBlock->ExpandSelect();

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

void InterCodeProcedure::EliminateIntegerSumAliasTemps(void)
{
	GrowingInstructionPtrArray	eivalues(nullptr);
	do {
		RemoveUnusedInstructions();

		eivalues.SetSize(mTemporaries.Size(), true);

		ResetVisited();
	} while (mEntryBlock->EliminateIntegerSumAliasTemps(eivalues));

	DisassembleDebug("EliminateIntegerSumAliasTemps");
}

void InterCodeProcedure::EliminateAliasValues()
{
	assert(mTemporaries.Size() == mLocalValueRange.Size());

	GrowingInstructionPtrArray	eivalues(nullptr);
	do {
		BuildDataFlowSets();

		assert(mTemporaries.Size() == mLocalValueRange.Size());

		eivalues.SetSize(mTemporaries.Size(), true);

		ResetVisited();
	} while (mEntryBlock->EliminateAliasValues(eivalues, eivalues));

	DisassembleDebug("EliminateAliasValues");
}

void InterCodeProcedure::ReduceRecursionTempSpilling(InterMemory paramMemory)
{
	GrowingInstructionPtrArray	gipa(nullptr);
	ResetVisited();
	mEntryBlock->ReduceRecursionTempSpilling(paramMemory, gipa);

	DisassembleDebug("ReduceRecursionTempSpilling");
}

void InterCodeProcedure::LoadStoreForwarding(InterMemory paramMemory)
{
	BuildTraces(0);

	DisassembleDebug("Load/Store forwardingY");

	bool changed;
	do {
		PropagateMemoryAliasingInfo(false);

		GrowingInstructionPtrArray	gipa(nullptr);
		ResetVisited();
		changed = mEntryBlock->LoadStoreForwarding(gipa, mModule->mGlobalVars);

		DisassembleDebug("Load/Store forwardingX");

		RemoveUnusedStoreInstructions(paramMemory);

		GlobalConstantPropagation();
		RemoveUnusedMallocs();

		TempForwarding();
		RemoveUnusedInstructions();

		DisassembleDebug("Load/Store forwarding");
	} while (changed);

}

void InterCodeProcedure::CombineIndirectAddressing(void)
{
	ResetVisited();

	mEntryBlock->CombineIndirectAddressing();
	BuildDataFlowSets();
}

void InterCodeProcedure::PropagateConstOperationsUp(void)
{
#if 1
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();

	bool	changed;
	do {
		changed = false;

		ResetVisited();
		if (mEntryBlock->SplitSingleBranchUseConst())
			changed = true;

		DisassembleDebug("SplitSingleBranchUseConst");

		ResetVisited();
		if (mEntryBlock->CommonTailCodeMerge())
		{
			changed = true;
			BuildDataFlowSets();
		}

		DisassembleDebug("CommonTailCodeMerge");

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
}

void InterCodeProcedure::BuildLocalAliasTable(void)
{
	//
	// Find all local variables that are never aliased
	//
	GrowingIntArray		localTable(-1), paramTable(-1);
	int					nlocals = 0, nparams = 0;

	localTable.SetSize(mTemporaries.Size());
	paramTable.SetSize(mTemporaries.Size());

	ResetVisited();
	mEntryBlock->CollectLocalAddressTemps(localTable, paramTable, nlocals, nparams);

	mLocalAliasedSet.Reset(nlocals);
	mParamAliasedSet.Reset(nparams);
	ResetVisited();
	mEntryBlock->MarkAliasedLocalTemps(localTable, mLocalAliasedSet, paramTable, mParamAliasedSet);

	Disassemble("Built alias temps");
}

void InterCodeProcedure::Close(void)
{
	GrowingTypeArray	tstack(IT_NONE);
	
	CheckFunc = !strcmp(mIdent->mString, "cia_init");
	CheckCase = false;

	mEntryBlock = mBlocks[0];

	DisassembleDebug("start");

	BuildTraces(10);
	DisassembleDebug("traces");

	EarlyBranchElimination();
	BuildTraces(0);

	do {
		BuildLoopPrefix();
		ResetVisited();
	} while (mEntryBlock->StripLoopHead(10));
	DisassembleDebug("Loop Head");
	BuildTraces(0);


	DisassembleDebug("branch elimination");
#if 0
	do {
		BuildLoopPrefix();
		ResetVisited();
	} while (mEntryBlock->StripLoopHead());
	DisassembleDebug("Loop Head");
	BuildTraces(0);
#endif

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

	BuildLocalAliasTable();

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
		tvalidSet.Reset(numTemps + 64);

		ResetVisited();
		mEntryBlock->PerformValueForwarding(mValueForwardingTable, valueSet, tvalidSet, mLocalAliasedSet, mParamAliasedSet, numTemps, mModule->mGlobalVars, mModule->mProcedures);

		assert(numTemps <= tvalidSet.Size());

		DisassembleDebug("PerformValueForwarding");

		ResetVisited();
		eliminated = mEntryBlock->EliminateDeadBranches();
		if (eliminated)
		{
			BuildTraces(0);
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

	RemoveUnusedInstructions();

	FastNumberSet	fusedSet(numTemps), fsingleSet(numTemps);
	ResetVisited();
	mEntryBlock->CalculateSingleUsedTemps(fusedSet, fsingleSet);

	ResetVisited();
	mEntryBlock->PerformMachineSpecificValueUsageCheck(mValueForwardingTable, tvalidSet, mModule->mGlobalVars, mModule->mProcedures, fsingleSet);

	DisassembleDebug("machine value forwarding");

	GlobalConstantPropagation();

	DisassembleDebug("Global Constant Propagation");

	// Check for cheap inlining
	// 
#if 1
	if (mCompilerOptions & COPT_OPTIMIZE_INLINE)
	{
		ResetVisited();
		if (mEntryBlock->CheapInlining(numTemps))
		{
			mValueForwardingTable.SetSize(numTemps, true);
			mTemporaries.SetSize(numTemps, true);

			DisassembleDebug("Cheap Inlining");

			BuildDataFlowSets();

			if (mCommonFrameSize == 0)
			{
				int		size = 0;

				ResetVisited();
				mEntryBlock->CollectOuterFrame(0, size, mHasDynamicStack, mHasInlineAssembler, mCallsByteCode);
				mCommonFrameSize = size;
			}
		}
	}
#endif
	ResetVisited();
	mEntryBlock->RemoveUnusedArgumentStoreInstructions();

	DisassembleDebug("RemoveUnusedArgumentStoreInstructions");

	// 
	//
	// Now remove needless temporary moves, that appear due to
	// stack evaluation
	//
	mTempForwardingTable.Reset();
	mTempForwardingTable.SetSize(numTemps);

	ResetVisited();
	mEntryBlock->PerformTempForwarding(mTempForwardingTable, false, false);

	DisassembleDebug("temp forwarding 2");

	//
	// Now remove unused instructions
	//

	RemoveUnusedInstructions();

	DisassembleDebug("removed unused instructions");

	InterMemory	paramMemory = mFastCallProcedure ? IM_FPARAM : IM_PARAM;

	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
		PromoteSimpleLocalsToTemp(paramMemory, mLocalAliasedSet.Size(), mParamAliasedSet.Size());
	else
		CollectVariables(paramMemory);

	for (int i = 0; i < mLocalVars.Size(); i++)
		if (i < mLocalAliasedSet.Size() && mLocalAliasedSet[i])
			mLocalVars[i]->mAliased = true;

	RecheckLocalAliased();

	BuildDataFlowSets();

	LoadStoreForwarding(paramMemory);

	RecheckLocalAliased();

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

	DisassembleDebug("pre single block loop opt");

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

	do {
		BuildLoopPrefix();
		ResetVisited();
	} while (mEntryBlock->StripLoopHead(4));
	DisassembleDebug("Loop Head 2");
	BuildTraces(0);

	TrimBlocks();

	BuildDataFlowSets();
	CheckUsedDefinedTemps();

#if 0
	ExpandSelect();

	BuildDataFlowSets();

	CheckUsedDefinedTemps();
#endif
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
		changed = mEntryBlock->PropagateVariableCopy(cipa, mModule->mGlobalVars, mLocalAliasedSet, mParamAliasedSet);

		RemoveUnusedStoreInstructions(paramMemory);
	} while (changed);

	DisassembleDebug("Copy forwarding");

	LoadStoreForwarding(paramMemory);

	RecheckLocalAliased();

	ResetVisited();
	mEntryBlock->FollowJumps();

	DisassembleDebug("Followed Jumps");

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

#if 1
	BuildDataFlowSets();

	do {
		Disassemble("gcp+");
		TempForwarding();
	} while (GlobalConstantPropagation());
	Disassemble("gcp-");
#endif

	BuildTraces(0);
	TrimBlocks();

	DisassembleDebug("Rebuilt traces");


	BuildDataFlowSets();

	TempForwarding();
	RemoveUnusedInstructions();

	LimitLoopIndexRanges();
	DisassembleDebug("LimitLoopIndexRanges");

#if 1
	ExpandingInstructionPtrArray	tarr;
	ResetVisited();
	mEntryBlock->PropagateValueRangeSetConversions(tarr);

	ResetVisited();
	mEntryBlock->BuildLocalIntegerRangeSets(mTemporaries.Size());

	do {
		DisassembleDebug("tt");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(true));

	TempForwarding();
	RemoveUnusedInstructions();

	do {
		DisassembleDebug("tq");

		ResetVisited();
	} while (mEntryBlock->BuildGlobalIntegerRangeSets(false));


	DisassembleDebug("Estimated value range");


	GrowingInstructionPtrArray	pipa(nullptr);
	ResetVisited();
	mEntryBlock->LinkerObjectForwarding(pipa);

	RebuildIntegerRangeSet();

	ResetVisited();
	mEntryBlock->SimplifyIntegerRangeRelops();

	DisassembleDebug("Simplified range limited relational ops 1");
#endif

	BuildTraces(0);
	TrimBlocks();
	DisassembleDebug("Rebuilt traces");

#if 1
	SimplifyIntegerNumeric(activeSet);

#endif

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->FollowJumps();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);
	BuildDataFlowSets();

	DisassembleDebug("Followed Jumps 2");

	RebuildIntegerRangeSet();

	EliminateAliasValues();

	SingleBlockLoopPointerSplit(activeSet);

	MergeIndexedLoadStore();
	
	SingleBlockLoopPointerToByte(activeSet);

	SingleBlockLoopSinking(activeSet);

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

	DisassembleDebug("PreHoistCommonConditionalPath");
	HoistCommonConditionalPath();
	DisassembleDebug("HoistCommonConditionalPath");

#if 1
	ResetVisited();
	mEntryBlock->MoveLoopHeadCheckToTail();

#endif
	DisassembleDebug("MoveLoopHeadCheckToTail");

#if 1
	LoadStoreForwarding(paramMemory);

	RebuildIntegerRangeSet();
#endif

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->Flatten2DLoop();

	DisassembleDebug("Flatten2DLoop");

#endif

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->PostDecLoopOptimization();

	DisassembleDebug("PostDecLoopOptimization");

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

	DisassembleDebug("single block loop opt X");

	BuildDataFlowSets();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);
#endif

	BuildTraces(0);
	TrimBlocks();

#if 1
	SingleBlockLoopPointerSplit(activeSet);

	MergeIndexedLoadStore();
#endif

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->InnerLoopOptimization(mParamAliasedSet);

	DisassembleDebug("inner loop opt 2");

	BuildDataFlowSets();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildTraces(0);
	TrimBlocks();
#endif

	SingleTailLoopOptimization(paramMemory);
	BuildDataFlowSets();

	PropagateMemoryAliasingInfo(true);

#if 1
	ExpandSelect();

	BuildDataFlowSets();

	CheckUsedDefinedTemps();
#endif

#if 1
	BuildTraces(0);
	TrimBlocks();

	PushSinglePathResultInstructions();

	ResetVisited();
	if (mEntryBlock->MergeSameConditionTraces())
	{
		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);
	}

	BuildDataFlowSets();

	CheckUsedDefinedTemps();
#endif

	PeepholeOptimization();

#if 1
	BuildDataFlowSets();

	do {
		Disassemble("gcp+");
		TempForwarding();
	} while (GlobalConstantPropagation());
	Disassemble("gcp-");
#endif

	LoadStoreForwarding(paramMemory);

#if 1
	RebuildIntegerRangeSet();
#endif

#if 1
	ResetVisited();
	mEntryBlock->SimplifyIntegerRangeRelops();

	DisassembleDebug("Simplified range limited relational ops 2");
#endif
	
	LimitLoopIndexIntegerRangeSets();
	DisassembleDebug("LimitLoopIndexIntegerRangeSets");

#if 1
	if (mCompilerOptions & COPT_OPTIMIZE_AUTO_UNROLL)
	{
		ResetVisited();
		mEntryBlock->SingleBlockLoopUnrolling();

		DisassembleDebug("Single Block loop unrolling");

		RemoveUnusedInstructions();

		BuildDataFlowSets();

		RenameTemporaries();
	}
#endif

#if 1
	ResetVisited();
	mEntryBlock->DropUnreachable();

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->ForwardDiamondMovedTemp();
	DisassembleDebug("Diamond move forwarding");

	ResetVisited();
	mEntryBlock->ForwardRealDiamondMovedTemp();
	DisassembleDebug("Real Diamond move forwarding");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();
	DisassembleDebug("Removed unreachable branches");

	BuildTraces(0);
	TrimBlocks();
	DisassembleDebug("Rebuilt traces");

	BuildDataFlowSets();

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

	RebuildIntegerRangeSet();

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

	DisassembleDebug("single block loop opt 2");

	BuildDataFlowSets();

	BuildTraces(0);
	TrimBlocks();
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

	BuildTraces(0);
	TrimBlocks();
	DisassembleDebug("Rebuilt traces");
#endif

#if 1
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->InnerLoopOptimization(mParamAliasedSet);

	DisassembleDebug("inner loop opt 3");

	BuildDataFlowSets();

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	BuildTraces(0);
	BuildDataFlowSets();
	BuildTraces(0);
#endif

	PropagateConstOperationsUp();

#if 1
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	DisassembleDebug("Pre MergeLoopTails");

	ResetVisited();
	if (mEntryBlock->MergeLoopTails())
	{
		BuildTraces(0);
		BuildDataFlowSets();
	}

	DisassembleDebug("Post MergeLoopTails");

	SingleTailLoopOptimization(paramMemory);
	BuildDataFlowSets();

#endif

	PropagateMemoryAliasingInfo(true);

	ResetVisited();
	mEntryBlock->SimplifyIntegerRangeRelops();

	DisassembleDebug("Simplified range limited relational ops 3");

#if 1
	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->ForwardDiamondMovedTemp();
	DisassembleDebug("Diamond move forwarding 2");

	ResetVisited();
	mEntryBlock->ForwardRealDiamondMovedTemp();
	DisassembleDebug("Real Diamond move forwarding 2");

	TempForwarding();

	RemoveUnusedInstructions();
#endif

	RemoveUnusedLocalStoreInstructions();

	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		MoveConditionsOutOfLoop();
	}

#if 1
	ResetVisited();

	if (!mInterruptCalled && !mDynamicStack && mNativeProcedure && mEntryBlock->CheckStaticStack())
	{
		mLinkerObject->mFlags |= LOBJF_STATIC_STACK;
		mLinkerObject->mStackSection = mModule->mLinker->AddSection(mIdent->Mangle("@stack"), LST_STATIC_STACK);
		mLinkerObject->mStackSection->mSections.Push(mModule->mParamLinkerSection);

		for (int i = 0; i < mLocalVars.Size(); i++)
		{
			InterVariable* var(mLocalVars[i]);
			if (var && !var->mTemp && !var->mLinkerObject)
			{
				var->mLinkerObject = mModule->mLinker->AddObject(mLocation, var->mIdent, mLinkerObject->mStackSection, LOT_BSS, var->mAlignment);
				var->mLinkerObject->mOwnerProc = this;
				var->mLinkerObject->mVariable = var;
				var->mLinkerObject->mFlags |= LOBJF_LOCAL_VAR;
				var->mLinkerObject->AddSpace(var->mSize);
				var->mIndex = mModule->mGlobalVars.Size();
				mModule->mGlobalVars.Push(var);
			}
		}

		mSaveTempsLinkerObject = mModule->mLinker->AddObject(mLocation, mIdent->Mangle("@stack"), mLinkerObject->mStackSection, LOT_BSS);
		mSaveTempsLinkerObject->mOwnerProc = this;

		ResetVisited();
		mEntryBlock->CollectStaticStack(mLinkerObject, mLocalVars);

		GrowingInstructionPtrArray	pipa(nullptr);
		ResetVisited();
		mEntryBlock->LinkerObjectForwarding(pipa);

#if 1
		BuildLoopPrefix();
		DisassembleDebug("added dominators");

		BuildDataFlowSets();

		ResetVisited();
		mEntryBlock->SingleBlockLoopOptimisation(mParamAliasedSet, mModule->mGlobalVars);

		DisassembleDebug("single block loop opt 4");

		BuildDataFlowSets();

		BuildTraces(0);
		DisassembleDebug("Rebuilt traces");
#endif
	}
	else if (!mInterruptCalled && mNativeProcedure)
	{
//		mLinkerObject->mFlags |= LOBJF_STATIC_STACK;
		mLinkerObject->mStackSection = mModule->mLinker->AddSection(mIdent->Mangle("@stack"), LST_STATIC_STACK);
		mLinkerObject->mStackSection->mSections.Push(mModule->mParamLinkerSection);
		ResetVisited();
		mEntryBlock->CollectStaticStackDependencies(mLinkerObject);
	}

#endif

	DisassembleDebug("PreLoopTemp");
	BuildDataFlowSets();
	ResetVisited();
	mEntryBlock->ForwardLoopMovedTemp();
	DisassembleDebug("PostLoopTemp");

	BuildDataFlowSets();
	RemoveUnusedInstructions();

	CheckFinal();
	DisassembleDebug("PreConstP");
#if 1
	do {
		DisassembleDebug("InConstP");
		TempForwarding();
	} while (GlobalConstantPropagation());

	BuildTraces(0);
	DisassembleDebug("Rebuilt traces");


	PeepholeOptimization();
	TempForwarding();
	RemoveUnusedInstructions();

	ShortcutConstBranches();

	ShortcutDuplicateBranches();

	DisassembleDebug("Global Constant Prop 1");

	BuildDataFlowSets();
	ResetVisited();
	mEntryBlock->PushMoveOutOfLoop();
	BuildDataFlowSets();

	DisassembleDebug("PushMoveOutOfLoop");

	TempForwarding(false, true);

#endif

#if 1
	SimplifyIntegerNumeric(activeSet);

#endif

	PropagateConstOperationsUp();

	CombineIndirectAddressing();

	BuildDataFlowSets();
	ResetVisited();
	mEntryBlock->ForwardShortLoadStoreOffsets();
	DisassembleDebug("ForwardShortLoadStoreOffsets");

	PeepholeOptimization();

	ConstSingleLoopOptimization();

	BuildTraces(0);

	ResetVisited();
	mEntryBlock->EmptyLoopOptimization();

#if 1
	BuildDataFlowSets();
	do {
		TempForwarding();
	} while (GlobalConstantPropagation());
#endif
#if 1
	ResetVisited();
	if (mEntryBlock->MapLateIntrinsics())
	{
		DisassembleDebug("MapLateIntrinsics");

		ResetVisited();
		mEntryBlock->RemoveUnusedArgumentStoreInstructions();

		DisassembleDebug("RemoveUnusedArgumentStoreInstructions");

		do
		{
			GrowingInstructionPtrArray	cipa(nullptr);
			ResetVisited();
			changed = mEntryBlock->PropagateVariableCopy(cipa, mModule->mGlobalVars, mLocalAliasedSet, mParamAliasedSet);

			RemoveUnusedStoreInstructions(paramMemory);
		} while (changed);

		DisassembleDebug("Copy forwarding 2");

	}

	PropagateMemoryAliasingInfo(true);

	ReplaceByteIndexPointers(activeSet);

	DisassembleDebug("ReplaceByteIndexPointers");

#endif
	ForwardSingleAssignmentBools();
#if 1
	{
		NumberSet	restrictSet(mNumRestricted);

		ResetVisited();
		mEntryBlock->CollectUnusedRestricted(restrictSet);

		ResetVisited();
		mEntryBlock->RemoveUnusedRestricted(restrictSet);

		DisassembleDebug("Remove unused restricted");
	}
#endif

//	CollapseDispatch();
//	DisassembleDebug("CollapseDispatch");


#if 1
	for (int i = 0; i < 8; i++)
	{
		BuildTraces(0);
		TrimBlocks();

		LoadStoreForwarding(paramMemory);

		RebuildIntegerRangeSet();

		ResetVisited();
		mEntryBlock->SimplifyIntegerRangeRelops();

		PeepholeOptimization();

		DisassembleDebug("Peephole Temp Check");
		
		RemoveUnusedInstructions();

		ReduceTemporaries();

		CheckBlocks();

		MergeBasicBlocks(activeSet);

		CheckBlocks();

		DisassembleDebug("TempForward Rev 1");

		BuildDataFlowSets();

		TempForwarding(true);

		RemoveUnusedInstructions();

		CheckBlocks();

		BuildDataFlowSets();

		DisassembleDebug("TempForward Rev 2");

		TempForwarding();

		DisassembleDebug("TempForward Rev 3");

		CheckBlocks();

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
		CheckBlocks();

		BuildDataFlowSets();
		do {
			TempForwarding();
		} while (GlobalConstantPropagation());

		DisassembleDebug("GlobalConstantPropagation");

		LoadStoreForwarding(paramMemory);

		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);
		MergeCommonPathInstructions();

		CheckBlocks();
#if 1	
		PushSinglePathResultInstructions();
#endif

		TempForwarding();
		RemoveUnusedInstructions();
#endif

		DisassembleDebug("Global Constant Prop 2");
	}
#endif

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->PropagateConstCompareResults();
	DisassembleDebug("PropagateConstCompareResults2");

	ResetVisited();
	mEntryBlock->PropagateNonNullPointers(NumberSet(mTemporaries.Size()));
	DisassembleDebug("PropagateNonNullPointers");

	GrowingInstructionPtrArray	ptemps(nullptr);
	ptemps.SetSize(mTemporaries.Size());
	ResetVisited();
	mEntryBlock->ForwardConstTemps(ptemps);

	ResetVisited();
	mEntryBlock->PullStoreUpToConstAddress();
	DisassembleDebug("PullStoreUpToConstAddress");

	LoadStoreForwarding(paramMemory);

	ConstSingleLoopOptimization();

	BuildDataFlowSets();
	TempForwarding(false, true);

	EliminateDoubleLoopCounter();
	DisassembleDebug("EliminateDoubleLoopCounter");

	BuildDataFlowSets();

	PeepholeOptimization();

	ResetVisited();
	mEntryBlock->ReplaceCopyFill();

	ResetVisited();
	mEntryBlock->PropagateConstCompareResults();

	ptemps.SetSize(mTemporaries.Size(), true);
	ResetVisited();
	mEntryBlock->ForwardConstTemps(ptemps);

	ResetVisited();
	mEntryBlock->SingleLoopCountZeroCheck();

	ResetVisited();
	mEntryBlock->InnerLoopCountZeroCheck();

	RemoveUnusedPartialStoreInstructions();

	PeepholeOptimization();

	EliminateIntegerSumAliasTemps();

	ResetVisited();
	if (mEntryBlock->Flatten2DLoop())
	{
		DisassembleDebug("Flatten2DLoop");

		ConstSingleLoopOptimization();

		BuildDataFlowSets();
		TempForwarding(false, true);

		PeepholeOptimization();
	}

	UntangleLoadStoreSequence();

	ConstInnerLoopOptimization();

	BuildTraces(0);

	BuildDataFlowSets();
	TempForwarding(false, true);

	ResetVisited();
	mEntryBlock->EmptyLoopOptimization();

#if 1
	BuildDataFlowSets();
	do {
		TempForwarding();
	} while (GlobalConstantPropagation());
#endif

	ResetVisited();
	mEntryBlock->ReduceTempLivetimes();
	DisassembleDebug("reduce temp livetimes");

	MapVariables();

	DisassembleDebug("mapped variabled");

	ReduceTemporaries(true);

	DisassembleDebug("Reduced Temporaries");

	if (!mFastCallProcedure)
		ReduceRecursionTempSpilling(paramMemory);

	// Optimize for size

	MergeBasicBlocks(activeSet);
	BuildTraces(0, false, true);
	TrimBlocks();

	DisassembleDebug("Final Merged basic blocks");

	WarnInvalidValueRanges();

	BuildDataFlowSets();

	MapCallerSavedTemps();

	ResetVisited();
	mEntryBlock->PromoteStaticStackParams(mModule->mParamLinkerObject);

	if (mValueReturn)
	{
		ResetVisited();
		mEntryBlock->CheckValueReturn();
	}

	ResetVisited();
	mEntryBlock->CheckNullptrDereference();

	StructReturnPropagation();

	if (mSaveTempsLinkerObject && mTempSize > BC_REG_TMP_SAVED - BC_REG_TMP)
		mSaveTempsLinkerObject->AddSpace(mTempSize - (BC_REG_TMP_SAVED - BC_REG_TMP));

	for (int i = 0; i < mParamVars.Size(); i++)
	{
		InterVariable* v(mParamVars[i]);
		if (v && v->mIdent)
		{
			if (v->mLinkerObject)
			{
			}
			else
			{
				LinkerObjectRange	range;
				range.mIdent = v->mIdent;
				range.mOffset = i + BC_REG_FPARAMS;
				range.mSize = v->mSize;
				mLinkerObject->mZeroPageRanges.Push(range);
			}
		}
	}

	if (mTempSize <= BC_REG_TMP_SAVED - BC_REG_TMP && !(mLinkerObject->mFlags & LOBJF_STATIC_STACK) && mLeafProcedure)
	{
		bool	hasLocals = false;

		for (int i = 0; i < mLocalVars.Size(); i++)
			if (mLocalVars[i] && mLocalVars[i]->mUsed)
				hasLocals = true;

		if (!hasLocals)
			mLinkerObject->mFlags |= LOBJF_STATIC_STACK;
	}

	if (!(mLinkerObject->mFlags & LOBJF_STATIC_STACK))
	{
		ResetVisited();
		if (!mEntryBlock->PreventsCallerStaticStack())
			mLinkerObject->mFlags |= LOBJF_STATIC_STACK;
	}

	if (!mNoInline && !mEntryBlock->mTrueJump)
	{
		NumberSet	tlocals(mTemporaries.Size());

		int	nconst = 0, nvariables = 0, nparams = 0, ncalls = 0, nret = 0, nother = 0, nops = 0;
		for (int i = 0; i < mEntryBlock->mInstructions.Size(); i++)
		{
			InterInstruction* ins = mEntryBlock->mInstructions[i];
			switch (ins->mCode)
			{
			case IC_LOAD:
				if (ins->mSrc[0].mTemp < 0)
				{
					if (ins->mSrc[0].mMemory == IM_FPARAM || ins->mSrc[0].mMemory == IM_PARAM)
					{
						nparams++;
						tlocals += ins->mDst.mTemp;
					}
					else if (ins->mSrc[0].mMemory == IM_GLOBAL || ins->mSrc[0].mMemory == IM_ABSOLUTE)
					{
						nvariables++;
						tlocals += ins->mDst.mTemp;
					}
					else
						nother++;
				}
				else if (tlocals[ins->mSrc[0].mTemp])
					nops++;
				else
					nother++;
				break;
			case IC_STORE:
				if (ins->mSrc[1].mTemp >= 0 || (ins->mSrc[1].mMemory != IM_FFRAME && ins->mSrc[1].mMemory != IM_FRAME))
					nops++;
				if (ins->mSrc[0].mTemp < 0)
					nconst++;
				break;

#if 1
			case IC_CONSTANT:
				if (ins->mDst.mType == IT_POINTER && (ins->mConst.mMemory == IM_FPARAM || ins->mConst.mMemory == IM_PARAM || ins->mConst.mMemory == IM_LOCAL))
					nother++;
				else
					nconst++;
				break;
#endif
#if 1
			case IC_LEA:
				if (ins->mSrc[1].mTemp >= 0 || (ins->mSrc[1].mMemory != IM_FPARAM && ins->mSrc[1].mMemory != IM_PARAM && ins->mSrc[1].mMemory != IM_LOCAL))
					nops++;
				else
					nother++;
				break;
#endif
#if 1
			case IC_BINARY_OPERATOR:
			case IC_UNARY_OPERATOR:
			case IC_RELATIONAL_OPERATOR:
				nops++;
				break;
#endif
			case IC_CALL:
			case IC_CALL_NATIVE:
				if (ins->mSrc[0].mTemp < 0)
					ncalls++;
				else
					nother++;
				break;
			case IC_FREE:
				ncalls++;
				break;
			case IC_RETURN:
			case IC_RETURN_VALUE:
				nret++;
				break;
			default:
				nother++;
			}
		}

		if (nother == 0 && ncalls <= 1 && nret == 1 && nconst <= 1 + nparams && nops <= 1 + nparams)
			mCheapInline = true;
	}

	mGlobalsChecked = true;
	mStoresIndirect = false;
	mLoadsIndirect = false;
	mReferencedGlobals.Reset(mModule->mGlobalVars.Size());
	mModifiedGlobals.Reset(mModule->mGlobalVars.Size());
#if 1
	if (!mLeafProcedure && mCommonFrameSize > 0)
	{
		ResetVisited();
		if (!mEntryBlock->RecheckOuterFrame())
			mCommonFrameSize = 0;
	}

	mParamVarsSize = 0;
	ResetVisited();
	mEntryBlock->CollectParamVarsSize(mParamVarsSize);

#endif
	ResetVisited();
	mEntryBlock->CollectGlobalReferences(mReferencedGlobals, mModifiedGlobals, mStoresIndirect, mLoadsIndirect, mGlobalsChecked);

	ResetVisited();
	mEntryBlock->MarkAliasing(mParamAliasedSet);

	DisassembleDebug("Marked Aliasing");
}

void InterCodeProcedure::AddCalledFunction(InterCodeProcedure* proc)
{
	assert(proc != nullptr);
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
	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		ResetVisited();
		mEntryBlock->RemoveNonRelevantStatics();
		RemoveUnusedInstructions();
	}
}


void InterCodeProcedure::LimitLoopIndexRanges(void)
{
	BuildLoopPrefix();
	DisassembleDebug("added dominators");

	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	ResetVisited();
	mEntryBlock->LimitLoopIndexRanges();
}

void InterCodeProcedure::SingleTailLoopOptimization(InterMemory paramMemory)
{
	bool	changed;

	do {
		changed = false;

		BuildLoopPrefix();
		DisassembleDebug("added dominators");

		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);

		BuildDataFlowSets();

		ResetVisited();
		changed = mEntryBlock->SingleTailLoopOptimization(mParamAliasedSet, mModule->mGlobalVars);
		DisassembleDebug("SingleTailLoopOptimization");


		if (changed)
		{
			TempForwarding();

			RemoveUnusedInstructions();

			RemoveUnusedStoreInstructions(paramMemory);
		}

		BuildTraces(0);
		DisassembleDebug("Rebuilt traces");

	} while (changed);
}

void InterCodeProcedure::MapVariables(void)
{
	ResetVisited();
	mEntryBlock->MapVariables(mModule->mGlobalVars, mLocalVars);
	mLocalSize = 0;
	for (int i = 0; i < mLocalVars.Size(); i++)
	{
#if 0
		if (mLocalVars[i])
		{
			printf("MapVars %s, %d: %s %d %d\n",
				mIdent->mString, i, mLocalVars[i]->mIdent ? mLocalVars[i]->mIdent->mString : "?",
				mLocalVars[i]->mUsed, mLocalVars[i]->mSize);
		}
#endif
		if (mLocalVars[i] && mLocalVars[i]->mUsed && !mLocalVars[i]->mLinkerObject)
		{
			mLocalVars[i]->mOffset = mLocalSize;
			mLocalSize += mLocalVars[i]->mSize;
		}
	}
}

static bool IsReadModifyWrite(const InterCodeBasicBlock* block, int at)
{
	if (block->mInstructions[at]->mCode == IC_LOAD &&
		block->mInstructions[at + 2]->mCode == IC_STORE &&
		block->mInstructions[at + 1]->mDst.mTemp == block->mInstructions[at + 2]->mSrc[0].mTemp &&
		block->mInstructions[at]->mSrc[0].IsEqual(block->mInstructions[at + 2]->mSrc[1]))
	{
		if (block->mInstructions[at + 1]->mCode == IC_BINARY_OPERATOR)
		{
			return
				block->mInstructions[at + 1]->mSrc[0].mTemp == block->mInstructions[at]->mDst.mTemp ||
				block->mInstructions[at + 1]->mSrc[1].mTemp == block->mInstructions[at]->mDst.mTemp;
		}
		else if (block->mInstructions[at + 1]->mCode == IC_UNARY_OPERATOR)
		{
			return
				block->mInstructions[at + 1]->mSrc[0].mTemp == block->mInstructions[at]->mDst.mTemp;
		}
	}
	
	return false;
}

bool InterCodeBasicBlock::SameExitCode(const InterCodeBasicBlock* block) const
{
	int sz0 = mInstructions.Size();
	int	sz1 = block->mInstructions.Size();

	if (sz0 > 1 && sz1 > 1)
	{
		InterInstruction* ins0 = mInstructions[sz0 - 2];
		InterInstruction* ins1 = block->mInstructions[sz1 - 2];

		if (ins0->IsEqual(ins1))
		{
			if (ins0->mCode == IC_STORE && ins0->mSrc[1].mTemp >= 0)
			{
				int	j0 = sz0 - 3;
				while (j0 >= 0 && mInstructions[j0]->mDst.mTemp != ins0->mSrc[1].mTemp)
					j0--;
				int	j1 = sz1 - 3;
				while (j1 >= 0 && block->mInstructions[j1]->mDst.mTemp != ins0->mSrc[1].mTemp)
					j1--;

				if (j0 >= 0 && j1 >= 0)
				{
					if (!(mInstructions[j0]->IsEqual(block->mInstructions[j1])))
					{
						if (mInstructions[j0]->mCode == IC_LEA && mInstructions[j0]->mSrc[1].mTemp < 0 ||
							block->mInstructions[j1]->mCode == IC_LEA && block->mInstructions[j1]->mSrc[1].mTemp < 0)
						{
							if (!IsSimilarLea(mInstructions[j0], block->mInstructions[j1]))
								return false;
						}
					}
				}
				else if (j0 >= 0)
				{
					if (mInstructions[j0]->mCode == IC_LEA && mInstructions[j0]->mSrc[1].mTemp < 0 && mInstructions[j0]->mSrc[0].IsUByte())
						return false;
				}
				else if (j1 >= 0)
				{
					if (block->mInstructions[j1]->mCode == IC_LEA && block->mInstructions[j1]->mSrc[1].mTemp < 0 && block->mInstructions[j1]->mSrc[0].IsUByte())
						return false;
				}

				if (InterTypeSize[ins0->mSrc[0].mType] == 4)
				{
					bool	rm0 = sz0 >= 4 && IsReadModifyWrite(this, sz0 - 4);
					bool	rm1 = sz1 >= 4 && IsReadModifyWrite(block, sz1 - 4);

					if (rm0 && rm1)
					{
						if (!(mInstructions[sz0 - 3]->IsEqual(block->mInstructions[sz1 - 3])))
							return false;
					}
					else if (rm0 || rm1)
						return false;
				}
			}
			else if (ins0->mCode == IC_LOAD && ins0->mSrc[0].mTemp >= 0)
			{
				int	j0 = mInstructions.Size() - 3;
				while (j0 >= 0 && mInstructions[j0]->mDst.mTemp != ins0->mSrc[0].mTemp)
					j0--;
				int	j1 = block->mInstructions.Size() - 3;
				while (j1 >= 0 && block->mInstructions[j1]->mDst.mTemp != ins0->mSrc[0].mTemp)
					j1--;

				if (j0 >= 0 && j1 >= 0)
				{
					if (!(mInstructions[j0]->IsEqual(block->mInstructions[j1])))
					{
						if (mInstructions[j0]->mCode == IC_LEA && mInstructions[j0]->mSrc[1].mTemp < 0 ||
							block->mInstructions[j1]->mCode == IC_LEA && block->mInstructions[j1]->mSrc[1].mTemp < 0)
						{
							if (!IsSimilarLea(mInstructions[j0], block->mInstructions[j1]))
								return false;
						}
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

bool InterCodeProcedure::ShortLeaMerge(FastNumberSet& activeSet)
{
	DisassembleDebug("PreShortLeaMerge");

	int		silvused = mTemporaries.Size();

	int		n = 0;
	do
	{
		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);

		n++;
		if (silvused != mTemporaries.Size())
		{
			mTemporaries.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

			mLocalValueRange.SetSize(activeSet.Num(), true);

			ResetVisited();
			mEntryBlock->RemapActiveTemporaries(activeSet);

			silvused = mTemporaries.Size();
		}

		ResetVisited();
	} while (mEntryBlock->ShortLeaMerge(silvused));

	assert(silvused == mTemporaries.Size());

	DisassembleDebug("ShortLeaMerge");

	ResetVisited();
	mEntryBlock->ShortLeaCleanup();

	DisassembleDebug("ShortLeaCleanup");

	return n > 1;
}

void InterCodeProcedure::MergeBasicBlocks(FastNumberSet& activeSet)
{
	ResetVisited();
	mEntryBlock->FollowJumps();

	ResetVisited();
	mEntryBlock->SplitBranches();

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
				while (j < i && !(mBlocks[j]->mNumEntries && mBlocks[j]->IsEqual(block) && mBlocks[j]->mIndex != 0))
					j++;
				blockMap[i] = mBlocks[j];
				if (i != j)
					mBlocks[j]->UnionIntegerRanges(block);
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

				bool	allBlocks = eblocks.Size() == block->mNumEntries;
//				if ()
				{
					GrowingArray<InterCodeBasicBlock* >	mblocks(nullptr);

					while (PartitionSameExitCode(eblocks, mblocks))
					{
						InterCodeBasicBlock* nblock;

						if (!allBlocks || eblocks.Size() || mblocks.IndexOf(block) != -1)
						{
							nblock = new InterCodeBasicBlock(this);

							for (int i = 0; i < mblocks.Size(); i++)
								mblocks[i]->mTrueJump = nblock;
							block->mNumEntries -= mblocks.Size();

							InterInstruction* jins = new InterInstruction(mblocks[0]->mInstructions.Last()->mLocation, IC_JUMP);
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
							
							ins->UnionRanges(mblocks[j]->mInstructions[mblocks[j]->mInstructions.Size() - 2]);

							mblocks[j]->mInstructions.Remove(mblocks[j]->mInstructions.Size() - 2);
						}

						if (nblock->mEntryValueRange.Size())
						{
							for (int j = 0; j < ins->mNumOperands; j++)
							{
								if (ins->mSrc[j].mTemp >= 0)
									nblock->mEntryValueRange[ins->mSrc[j].mTemp] = ins->mSrc[j].mRange;
							}
						}

						changed = true;
					}
				}
			}
		}

		if (!changed)
		{
			ResetVisited();
			mEntryBlock->FollowJumps();

			BuildDataFlowSets();

			changed = ShortLeaMerge(activeSet);
		}

	} while (changed);


}

bool InterCodeProcedure::ReferencesGlobal(int varindex)
{
	if (mGlobalsChecked)
	{
		if (varindex >= 0)
		{
			if (mModule->mGlobalVars[varindex]->mAliased)
				return mLoadsIndirect || mStoresIndirect;
			else if (varindex < mReferencedGlobals.Size())
				return mReferencedGlobals[varindex];
			else
				return false;
		}
		else
			return false;
	}
	else
		return true;
}

bool InterCodeProcedure::ModifiesGlobal(int varindex)
{
	if (mGlobalsChecked)
	{
		if (varindex >= 0)
		{
			if (mModule->mGlobalVars[varindex]->mAliased && mStoresIndirect)
				return true;

			if (varindex < mModifiedGlobals.Size())
				return mModifiedGlobals[varindex];
			else
				return false;
		}
		else
			return false;
	}
	else
		return true;
}

void InterCodeProcedure::BuildLoopPrefix(void)
{
	ResetVisited();

	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mLoopPrefix = nullptr;

	mEntryBlock = mEntryBlock->BuildLoopPrefix();

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	ResetVisited();
	mEntryBlock->BuildLoopSuffix();
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

void InterCodeProcedure::RemoveUnusedMallocs(void)
{
	ResetVisited();
	mEntryBlock->RemoveUnusedMallocs();
}

void InterCodeProcedure::RecheckLocalAliased(void)
{
	for (int i = 0; i < mLocalVars.Size(); i++)
	{
		InterVariable* v = mLocalVars[i];
		if (v)
		{
			if (v->mAliased)
				v->mAliased = false;
			else
				v->mNotAliased = true;
		}
	}
	for (int i = 0; i < mParamVars.Size(); i++)
	{
		InterVariable* v = mParamVars[i];
		if (v)
		{
			if (v->mAliased)
				v->mAliased = false;
			else
				v->mNotAliased = true;
		}
	}
	ResetVisited();
	mEntryBlock->RecheckLocalAliased();

	DisassembleDebug("RecheckLocalAliased");
}

void InterCodeProcedure::UntangleLoadStoreSequence(void)
{
	do {
		ResetVisited();
	} while (mEntryBlock->UntangleLoadStoreSequence());

	DisassembleDebug("UntangleLoadStoreSequence");
}

void InterCodeProcedure::ConstSingleLoopOptimization(void)
{
	BuildTraces(0);
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	GrowingInstructionPtrArray	ptemps(nullptr);
	ptemps.SetSize(mTemporaries.Size());

	ResetVisited();
	mEntryBlock->ForwardConstTemps(ptemps);

	Disassemble("PreConstSingleLoopOptimization");
	ResetVisited();
	mEntryBlock->ConstSingleLoopOptimization();
	Disassemble("PostConstSingleLoopOptimization");
}

void InterCodeProcedure::ConstInnerLoopOptimization(void)
{
	BuildTraces(0);
	BuildLoopPrefix();
	ResetEntryBlocks();
	ResetVisited();
	mEntryBlock->CollectEntryBlocks(nullptr);

	GrowingInstructionPtrArray	ptemps(nullptr);
	ptemps.SetSize(mTemporaries.Size());

	ResetVisited();
	mEntryBlock->ForwardConstTemps(ptemps);

	Disassemble("PreConstInnerLoopOptimization");
	ResetVisited();
	mEntryBlock->ConstInnerLoopOptimization();
	Disassemble("PostConstInnerLoopOptimization");
}

void InterCodeProcedure::HoistCommonConditionalPath(void)
{
	for(;;)
	{ 
		ResetVisited();
		if (!mEntryBlock->HoistCommonConditionalPath())
			return;
		Disassemble("InnerHoist");
		TempForwarding();
		RemoveUnusedInstructions();
	}
}


void InterCodeProcedure::ReduceTemporaries(bool final)
{
	NumberSet* collisionSet;
	int i, j, numRenamedTemps;
	int numTemps = mTemporaries.Size();

	GrowingIntArray	friendsMap(-1);

	NumberSet	callerSaved(numTemps);

	if (final)
	{
		ResetVisited();
		mEntryBlock->BuildCallerSaveTempSet(callerSaved);
	}

	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numTemps);

	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps), NumberSet(numTemps));

	NumberSet	totalRequired2(numTemps);

	do {
		ResetVisited();
	} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

	ResetVisited();
	mEntryBlock->PruneUnusedIntegerRangeSets();

	collisionSet = new NumberSet[numTemps]; 

	for (i = 0; i < numTemps; i++)
		collisionSet[i].Reset(numTemps);

	Disassemble("ReduceTemps", true);

	ResetVisited();
	mEntryBlock->BuildCollisionTable(collisionSet);

	friendsMap.SetSize(numTemps, true);

	ResetVisited();
	mEntryBlock->BuildFriendsTable(friendsMap);

	mRenameTable.SetSize(numTemps, true);

	numRenamedTemps = 0;

	NumberSet	usedTemps(numTemps);

	if (final)
	{
		for (int sz = 4; sz > 0; sz >>= 1)
		{
			for (i = 0; i < numTemps; i++)
			{
				if (InterTypeSize[mTemporaries[i]] == sz && !callerSaved[i])
				{
					usedTemps.Clear();

					for (j = 0; j < numTemps; j++)
					{
						if (mRenameTable[j] >= 0 && collisionSet[i][j])
							usedTemps += mRenameTable[j];
					}
					
					if (friendsMap[i] >= 0 && mRenameTable[friendsMap[i]] >= 0 && !usedTemps[mRenameTable[friendsMap[i]]])
						j = mRenameTable[friendsMap[i]];
					else
					{
						j = 0;
						while (usedTemps[j])
							j++;
					}

					mRenameTable[i] = j;
					if (j >= numRenamedTemps) numRenamedTemps = j + 1;
				}
			}
			for (i = 0; i < numTemps; i++)
			{
				if (InterTypeSize[mTemporaries[i]] == sz && callerSaved[i])
				{
					usedTemps.Clear();

					for (j = 0; j < numTemps; j++)
					{
						if (mRenameTable[j] >= 0 && collisionSet[i][j])
							usedTemps += mRenameTable[j];
					}

					if (friendsMap[i] >= 0 && mRenameTable[friendsMap[i]] >= 0 && !usedTemps[mRenameTable[friendsMap[i]]])
						j = mRenameTable[friendsMap[i]];
					else
					{
						j = 0;
						while (usedTemps[j])
							j++;
					}

					mRenameTable[i] = j;
					if (j >= numRenamedTemps) numRenamedTemps = j + 1;
				}
			}
		}
	}
	else
	{
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
	}

	mTemporaries.SetSize(numRenamedTemps, true);

	ResetVisited();
	mEntryBlock->GlobalRenameRegister(mRenameTable, mTemporaries);

	delete[] collisionSet;

	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numRenamedTemps);

	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numRenamedTemps), NumberSet(numRenamedTemps));

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

	int		callerSavedTemps = 0, calleeSavedTemps = BC_REG_TMP_SAVED - BC_REG_TMP, freeCallerSavedTemps = 0, freeTemps = 0;

	if (mCallsFunctionPointer || mDynamicStack)
		freeCallerSavedTemps = BC_REG_TMP_SAVED - BC_REG_TMP;
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

	if (mSaveTempsLinkerObject && mTempSize > BC_REG_TMP_SAVED - BC_REG_TMP)
		mSaveTempsLinkerObject->AddSpace(mTempSize - (BC_REG_TMP_SAVED - BC_REG_TMP));

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
#if DISASSEMBLE_OPT
#ifdef _WIN32
	FILE* file;
	static bool	initial = true;

	if (!CheckFunc)
		return;

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

InterCodeModule::InterCodeModule(Errors* errors, Linker * linker)
	: mErrors(errors), mLinker(linker), mGlobalVars(nullptr), mProcedures(nullptr), mCompilerOptions(0), mParamLinkerObject(nullptr), mParamLinkerSection(nullptr)
{
}

InterCodeModule::~InterCodeModule(void)
{

}


void InterCodeModule::InitParamStack(LinkerSection* stackSection)
{
	mParamLinkerSection = mLinker->AddSection(Ident::Unique("sstack"), LST_STATIC_STACK);
	stackSection->mSections.Push(mParamLinkerSection);

	mParamLinkerObject = mLinker->AddObject(Location(), Ident::Unique("sstack"), mParamLinkerSection, LOT_STACK);
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

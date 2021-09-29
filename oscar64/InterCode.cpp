#include "InterCode.h"

#include "InterCode.h"
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

void ValueSet::FlushCallAliases(void)
{
	int	i;

	i = 0;

	while (i < mNum)
	{
		if ((mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE) && mInstructions[i]->mMemory != IM_PARAM && mInstructions[i]->mMemory != IM_LOCAL)
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
		else
			i++;
	}
}

static int64 ConstantFolding(InterOperator oper, int64 val1, int64 val2)
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
		return val1 >> val2;
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

static void ConversionConstantFold(InterInstruction * ins, InterInstruction * cins)
{
	switch (ins->mOperator)
	{
	case IA_INT2FLOAT:
		ins->mCode = IC_CONSTANT;
		ins->mFloatValue = (double)(cins->mIntValue);
		ins->mSTemp[0] = -1;
		break;
	case IA_FLOAT2INT:
		ins->mCode = IC_CONSTANT;
		ins->mIntValue = (int)(cins->mFloatValue);
		ins->mSTemp[0] = -1;
		break;
	case IA_EXT8TO16S:
		ins->mCode = IC_CONSTANT;
		ins->mIntValue = (char)(cins->mIntValue);
		ins->mSTemp[0] = -1;
		break;
	case IA_EXT8TO16U:
		ins->mCode = IC_CONSTANT;
		ins->mIntValue = (unsigned char)(cins->mIntValue);
		ins->mSTemp[0] = -1;
		break;
	}
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

static bool MemPtrRange(const InterInstruction* ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset)
{
	while (ins && ins->mMemory == IM_INDIRECT && ins->mCode == IC_LEA)
		ins = tvalue[ins->mSTemp[1]];

	if (ins && (ins->mCode == IC_CONSTANT || ins->mCode == IC_LEA))
	{
		mem = ins->mMemory;
		vindex = ins->mVarIndex;
		offset = ins->mIntValue;

		return true;
	}
	else
		return false;
}


static bool MemRange(const InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset)
{
	if (ins->mMemory == IM_INDIRECT)
	{
		if (ins->mCode == IC_LOAD)
			return MemPtrRange(tvalue[ins->mSTemp[0]], tvalue, mem, vindex, offset);
		else
			return MemPtrRange(tvalue[ins->mSTemp[1]], tvalue, mem, vindex, offset);
	}
	if (ins)
	{
		mem = ins->mMemory;
		vindex = ins->mVarIndex;
		offset = ins->mIntValue;

		return true;
	}
	else
		return false;
}

static bool StoreAliasing(const InterInstruction * lins, const InterInstruction* sins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams)
{
	InterMemory	lmem, smem;
	int			lvindex, svindex;
	int			loffset, soffset;

	if (MemRange(lins, tvalue, lmem, lvindex, loffset))
	{
		if (MemRange(sins, tvalue, smem, svindex, soffset))
		{
			if (smem == lmem && svindex == lvindex)
			{
				if (soffset + sins->mOperandSize >= loffset && loffset + lins->mOperandSize >= soffset)
					return true;
			}

			return false;
		}

		if (lmem == IM_LOCAL)
			return aliasedLocals[lvindex];
		else if (lmem == IM_PARAM)
			return aliasedParams[lvindex];
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

void ValueSet::UpdateValue(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams)
{
	int	i, value, temp;

	temp = ins->mTTemp;

	if (temp >= 0)
	{
		i = 0;
		while (i < mNum)
		{
			if (temp == mInstructions[i]->mTTemp ||
				temp == mInstructions[i]->mSTemp[0] ||
				temp == mInstructions[i]->mSTemp[1] ||
				temp == mInstructions[i]->mSTemp[2])
			{
				mNum--;
				if (i < mNum)
					mInstructions[i] = mInstructions[mNum];
			}
			else
				i++;
		}
	}

	for (i = 0; i < 3; i++)
	{
		temp = ins->mSTemp[i];
		if (temp >= 0 && tvalue[temp])
		{
			ins->mSTemp[i] = tvalue[temp]->mTTemp;
		}
	}

	switch (ins->mCode)
	{
	case IC_LOAD:
		i = 0;
		while (i < mNum &&
			(mInstructions[i]->mCode != IC_LOAD ||
				mInstructions[i]->mSTemp[0] != ins->mSTemp[0] ||
				mInstructions[i]->mOperandSize != ins->mOperandSize))
		{
			i++;
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSTemp[0] = mInstructions[i]->mTTemp;
			ins->mSType[0] = mInstructions[i]->mTType;
			assert(ins->mSTemp[0] >= 0);
		}
		else
		{
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_STORE ||
					mInstructions[i]->mSTemp[1] != ins->mSTemp[0] ||
					mInstructions[i]->mOperandSize != ins->mOperandSize))
			{
				i++;
			}

			if (i < mNum)
			{
				if (mInstructions[i]->mSTemp[0] < 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mSTemp[0] = -1;
					ins->mSType[0] = mInstructions[i]->mSType[0];
					ins->mIntValue = mInstructions[i]->mSIntConst[0];
				}
				else
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mSTemp[0];
					ins->mSType[0] = mInstructions[i]->mSType[0];
					assert(ins->mSTemp[0] >= 0);
				}
			}
			else
			{
				InsertValue(ins);
			}
		}

		break;
	case IC_STORE:
		i = 0;
		while (i < mNum)
		{
			if ((mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE) && StoreAliasing(mInstructions[i], ins, tvalue, aliasedLocals, aliasedParams))
			{
				mNum--;
				if (mNum > 0)
					mInstructions[i] = mInstructions[mNum];
			}
			else
				i++;
		}

		InsertValue(ins);
		break;
	case IC_COPY:
		i = 0;
		while (i < mNum)
		{
			if ((mInstructions[i]->mCode == IC_LOAD || mInstructions[i]->mCode == IC_STORE) && StoreAliasing(mInstructions[i], ins, tvalue, aliasedLocals, aliasedParams))
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
		switch (ins->mTType)
		{
		case IT_FLOAT:
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mTType != ins->mTType ||
					mInstructions[i]->mFloatValue != ins->mFloatValue))
			{
				i++;
			}
			break;
		case IT_POINTER:
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mTType != ins->mTType ||
					mInstructions[i]->mIntValue != ins->mIntValue ||
					mInstructions[i]->mMemory != ins->mMemory ||
					mInstructions[i]->mVarIndex != ins->mVarIndex ||
					mInstructions[i]->mLinkerObject != ins->mLinkerObject))
			{
				i++;
			}
			break;
		default:

			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_CONSTANT ||
					mInstructions[i]->mTType != ins->mTType ||
					mInstructions[i]->mIntValue != ins->mIntValue))
			{
				i++;
			}
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSTemp[0] = mInstructions[i]->mTTemp;
			ins->mSType[0] = mInstructions[i]->mTType;
			assert(ins->mSTemp[0] >= 0);
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
				mInstructions[i]->mSTemp[0] != ins->mSTemp[0] ||
				mInstructions[i]->mSIntConst[0] != ins->mSIntConst[0] ||
				mInstructions[i]->mSTemp[1] != ins->mSTemp[1]))
		{
			i++;
		}

		if (i < mNum)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			ins->mSTemp[0] = mInstructions[i]->mTTemp;
			ins->mSType[0] = mInstructions[i]->mTType;
			ins->mSTemp[1] = -1;
			assert(ins->mSTemp[0] >= 0);
		}
		else
		{
			InsertValue(ins);
		}
		break;

	case IC_BINARY_OPERATOR:
		switch (ins->mSType[0])
		{
		case IT_FLOAT:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mFloatValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mFloatValue, tvalue[ins->mSTemp[0]]->mFloatValue);
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mTType != ins->mTType ||
						mInstructions[i]->mFloatValue != ins->mFloatValue))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					assert(ins->mSTemp[0] >= 0);
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
						mInstructions[i]->mSTemp[0] != ins->mSTemp[0] ||
						mInstructions[i]->mSTemp[1] != ins->mSTemp[1]))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);
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
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mIntValue, tvalue[ins->mSTemp[0]]->mIntValue);
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

				return;
			}

			if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				if ((ins->mOperator == IA_ADD || ins->mOperator == IA_SUB ||
					ins->mOperator == IA_OR || ins->mOperator == IA_XOR ||
					ins->mOperator == IA_SHL || ins->mOperator == IA_SHR || ins->mOperator == IA_SAR) && tvalue[ins->mSTemp[0]]->mIntValue == 0 ||
					(ins->mOperator == IA_MUL || ins->mOperator == IA_DIVU || ins->mOperator == IA_DIVS) && tvalue[ins->mSTemp[0]]->mIntValue == 1 ||
					(ins->mOperator == IA_AND) && tvalue[ins->mSTemp[0]]->mIntValue == -1)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = ins->mSTemp[1];
					ins->mSType[0] = ins->mSType[1];
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND) && tvalue[ins->mSTemp[0]]->mIntValue == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mIntValue = 0;
					ins->mSTemp[0] = -1;
					ins->mSTemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
			}
			else if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
			{
				if ((ins->mOperator == IA_ADD || ins->mOperator == IA_OR || ins->mOperator == IA_XOR) && tvalue[ins->mSTemp[1]]->mIntValue == 0 ||
					(ins->mOperator == IA_MUL) && tvalue[ins->mSTemp[1]]->mIntValue == 1 ||
					(ins->mOperator == IA_AND) && tvalue[ins->mSTemp[1]]->mIntValue == -1)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
				else if ((ins->mOperator == IA_MUL || ins->mOperator == IA_AND ||
					ins->mOperator == IA_SHL || ins->mOperator == IA_SHR || ins->mOperator == IA_SAR) && tvalue[ins->mSTemp[1]]->mIntValue == 0)
				{
					ins->mCode = IC_CONSTANT;
					ins->mIntValue = 0;
					ins->mSTemp[0] = -1;
					ins->mSTemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
				else if (ins->mOperator == IA_SUB && tvalue[ins->mSTemp[1]]->mIntValue == 0)
				{
					ins->mCode = IC_UNARY_OPERATOR;
					ins->mOperator = IA_NEG;
					ins->mSTemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
			}
			else if (ins->mSTemp[0] == ins->mSTemp[1])
			{
				if (ins->mOperator == IA_SUB || ins->mOperator == IA_XOR)
				{
					ins->mCode = IC_CONSTANT;
					ins->mIntValue = 0;
					ins->mSTemp[0] = -1;
					ins->mSTemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
				else if (ins->mOperator == IA_AND || ins->mOperator == IA_OR)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);

					return;
				}
			}
			
			i = 0;
			while (i < mNum &&
				(mInstructions[i]->mCode != IC_BINARY_OPERATOR ||
					mInstructions[i]->mOperator != ins->mOperator ||
					mInstructions[i]->mSTemp[0] != ins->mSTemp[0] ||
					mInstructions[i]->mSTemp[1] != ins->mSTemp[1]))
			{
				i++;
			}

			if (i < mNum)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSTemp[0] = mInstructions[i]->mTTemp;
				ins->mSType[0] = mInstructions[i]->mTType;
				ins->mSTemp[1] = -1;
				assert(ins->mSTemp[0] >= 0);
			}
			else
			{
				InsertValue(ins);
			}
			break;
		}
		break;

	case IC_CONVERSION_OPERATOR:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			ConversionConstantFold(ins, tvalue[ins->mSTemp[0]]);
			if (ins->mTType == IT_FLOAT)
			{
				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mTType != ins->mTType ||
						mInstructions[i]->mFloatValue != ins->mFloatValue))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					assert(ins->mSTemp[0] >= 0);
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
					mInstructions[i]->mSTemp[0] != ins->mSTemp[0]))
			{
				i++;
			}

			if (i < mNum)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSTemp[0] = mInstructions[i]->mTTemp;
				ins->mSType[0] = mInstructions[i]->mTType;
				ins->mSTemp[1] = -1;
				assert(ins->mSTemp[0] >= 0);
			}
			else
			{
				InsertValue(ins);
			}
		}
		break;

	case IC_UNARY_OPERATOR:
		switch (ins->mSType[0])
		{
		case IT_FLOAT:
			if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mFloatValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[0]]->mFloatValue);
				ins->mSTemp[0] = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mTType != ins->mTType ||
						mInstructions[i]->mFloatValue != ins->mFloatValue))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					assert(ins->mSTemp[0] >= 0);
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
						mInstructions[i]->mSTemp[0] != ins->mSTemp[0]))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);
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
			if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[0]]->mIntValue);
				ins->mSTemp[0] = -1;

				i = 0;
				while (i < mNum &&
					(mInstructions[i]->mCode != IC_CONSTANT ||
						mInstructions[i]->mTType != ins->mTType ||
						mInstructions[i]->mIntValue != ins->mIntValue))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					assert(ins->mSTemp[0] >= 0);
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
						mInstructions[i]->mSTemp[0] != ins->mSTemp[0]))
				{
					i++;
				}

				if (i < mNum)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = mInstructions[i]->mTTemp;
					ins->mSType[0] = mInstructions[i]->mTType;
					assert(ins->mSTemp[0] >= 0);
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
		switch (ins->mSType[1])
		{
		case IT_FLOAT:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantRelationalFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mFloatValue, tvalue[ins->mSTemp[0]]->mFloatValue);
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mIntValue, tvalue[ins->mSTemp[0]]->mIntValue);
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);
			}
			else if (ins->mSTemp[1] == ins->mSTemp[0])
			{
				ins->mCode = IC_CONSTANT;

				switch (ins->mOperator)
				{
				case IA_CMPEQ:
				case IA_CMPGES:
				case IA_CMPLES:
				case IA_CMPGEU:
				case IA_CMPLEU:
					ins->mIntValue = 1;
					break;
				case IA_CMPNE:
				case IA_CMPGS:
				case IA_CMPLS:
				case IA_CMPGU:
				case IA_CMPLU:
					ins->mIntValue = 0;
					break;
				}
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals, aliasedParams);
			}
			break;
		}
		break;
	case IC_CALL:
	case IC_CALL_NATIVE:
		FlushCallAliases();
		break;

	}
}

InterInstruction::InterInstruction(void)
{
	mCode = IC_NONE;

	mTType = IT_NONE;
	mSType[0] = IT_NONE;
	mSType[1] = IT_NONE;
	mSType[2] = IT_NONE;

	mSIntConst[0] = 0;
	mSIntConst[1] = 0;
	mSIntConst[2] = 0;

	mSFloatConst[0] = 0;
	mSFloatConst[1] = 0;
	mSFloatConst[2] = 0;
	mMemory = IM_NONE;
	mOperandSize = 0;
	mVarIndex = -1;
	mIntValue = 0;
	mFloatValue = 0;
	mLinkerObject = nullptr;

	mTTemp = INVALID_TEMPORARY;
	mSTemp[0] = INVALID_TEMPORARY;
	mSTemp[1] = INVALID_TEMPORARY;
	mSTemp[2] = INVALID_TEMPORARY;

	mSFinal[0] = mSFinal[1] = mSFinal[2] = false;
	mInUse = false;
	mVolatile = false;
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

static InterType TypeCheckArithmecitResult(InterType t1, InterType t2)
{
	if (t1 == IT_FLOAT && t2 == IT_FLOAT)
		return IT_FLOAT;
	else if (TypeInteger(t1) && TypeInteger(t2))
		return t1 > t2 ? t1 : t2;
	else
		throw InterCodeTypeMismatchException();
}

static void TypeCheckAssign(InterType& t, InterType s)
{
	if (s == IT_NONE)
		throw InterCodeUninitializedException();
	else if (t == IT_NONE)
		t = s;
	else if (!TypeCompatible(t, s))
		throw InterCodeTypeMismatchException();
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
		if (mTType == IT_POINTER && mMemory == IM_LOCAL)
			localTable[mTTemp] = mVarIndex;
		else if (mTType == IT_POINTER && mMemory == IM_PARAM)
			paramTable[mTTemp] = mVarIndex;
	}
	else if (mCode == IC_LEA)
	{
		if (mMemory == IM_LOCAL)
			localTable[mTTemp] = localTable[mSTemp[1]];
		else if (mMemory == IM_PARAM)
			paramTable[mTTemp] = paramTable[mSTemp[1]];
	}
	else if (mCode == IC_LOAD_TEMPORARY)
	{
		localTable[mTTemp] = localTable[mSTemp[0]];
		paramTable[mTTemp] = paramTable[mSTemp[0]];
	}
}

void InterInstruction::MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams)
{
	if (mCode == IC_STORE && mSTemp[0] >= 0)
	{
		int	l = localTable[mSTemp[0]];
		if (l >= 0)
			aliasedLocals += l;
		l = paramTable[mSTemp[0]];
		if (l >= 0)
			aliasedParams += l;
	}
}

void InterInstruction::FilterTempUsage(NumberSet& requiredTemps, NumberSet& providedTemps)
{
	FilterTempUseUsage(requiredTemps, providedTemps, mSTemp[0]);
	FilterTempUseUsage(requiredTemps, providedTemps, mSTemp[1]);
	FilterTempUseUsage(requiredTemps, providedTemps, mSTemp[2]);
	FilterTempDefineUsage(requiredTemps, providedTemps, mTTemp);
}

void InterInstruction::FilterVarsUsage(const GrowingVariableArray& localVars, NumberSet& requiredVars, NumberSet& providedVars)
{
	if (mCode == IC_LOAD && mMemory == IM_LOCAL)
	{
		assert(mSTemp[0] < 0);
		if (!providedVars[mVarIndex])
			requiredVars += mVarIndex;
	}
	else if (mCode == IC_STORE && mMemory == IM_LOCAL)
	{
		assert(mSTemp[1] < 0);
		if (!providedVars[mVarIndex] && (mSIntConst[1] != 0 || mOperandSize != localVars[mVarIndex]->mSize))
			requiredVars += mVarIndex;
		providedVars += mVarIndex;
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
		if (mSTemp[0] >= 0 && ctemps[mSTemp[0]])
		{
			InterInstruction* ains = ctemps[mSTemp[0]];
			mSIntConst[0] = ains->mIntValue;
			mLinkerObject = ains->mLinkerObject;
			mVarIndex = ains->mVarIndex;
			mMemory = ains->mMemory;
			mSTemp[0] = -1;
			return true;
		}
		break;
	case IC_STORE:
		if (mSTemp[1] >= 0 && ctemps[mSTemp[1]])
		{
			InterInstruction* ains = ctemps[mSTemp[1]];
			mSIntConst[1] = ains->mIntValue;
			mLinkerObject = ains->mLinkerObject;
			mVarIndex = ains->mVarIndex;
			mMemory = ains->mMemory;
			mSTemp[1] = -1;
			return true;
		}
		break;
	case IC_LOAD_TEMPORARY:
		if (mSTemp[0] >= 0 && ctemps[mSTemp[0]])
		{
			InterInstruction* ains = ctemps[mSTemp[0]];
			mCode = IC_CONSTANT;
			mIntValue = ains->mIntValue;
			mFloatValue = ains->mFloatValue;
			mLinkerObject = ains->mLinkerObject;
			mVarIndex = ains->mVarIndex;
			mMemory = ains->mMemory;
			mSTemp[0] = -1;
			return true;
		}
	}

	return false;
}

void InterInstruction::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	PerformTempUseForwarding(mSTemp[0], forwardingTable);
	PerformTempUseForwarding(mSTemp[1], forwardingTable);
	PerformTempUseForwarding(mSTemp[2], forwardingTable);
	PerformTempDefineForwarding(mTTemp, forwardingTable);
	if (mCode == IC_LOAD_TEMPORARY && mTTemp != mSTemp[0])
	{
		forwardingTable.Build(mTTemp, mSTemp[0]);
	}
}

bool HasSideEffect(InterCode code)
{
	return code == IC_CALL || code == IC_CALL_NATIVE || code == IC_ASSEMBLER;
}

bool InterInstruction::RemoveUnusedResultInstructions(InterInstruction* pre, NumberSet& requiredTemps, int numStaticTemps)
{
	bool	changed = false;

	if (pre && mCode == IC_LOAD_TEMPORARY && pre->mTTemp == mSTemp[0] && !requiredTemps[mSTemp[0]] && pre->mTTemp >= numStaticTemps)
	{
		// previous instruction produced result, but it is not needed here
		pre->mTTemp = mTTemp;

		mCode = IC_NONE;
		mTTemp = -1;
		mSTemp[0] = -1;
		mSTemp[1] = -1;
		mSTemp[2] = -1;

		changed = true;
	}
	else if (mTTemp != -1)
	{
		if (!requiredTemps[mTTemp] && mTTemp >= numStaticTemps)
		{
			if (!HasSideEffect(mCode))
			{
				mCode = IC_NONE;
				mTTemp = -1;
				mSTemp[0] = -1;
				mSTemp[1] = -1;
				mSTemp[2] = -1;

				changed = true;
			}
			else
			{
				mTTemp = -1;

				changed = true;
			}
		}
		else
			requiredTemps -= mTTemp;
	}

	if (mSTemp[0] >= 0) mSFinal[0] = !requiredTemps[mSTemp[0]] && mSTemp[0] >= numStaticTemps;
	if (mSTemp[1] >= 0) mSFinal[1] = !requiredTemps[mSTemp[1]] && mSTemp[1] >= numStaticTemps;
	if (mSTemp[2] >= 0) mSFinal[2] = !requiredTemps[mSTemp[2]] && mSTemp[2] >= numStaticTemps;

	if (mSTemp[0] >= 0) requiredTemps += mSTemp[0];
	if (mSTemp[1] >= 0) requiredTemps += mSTemp[1];
	if (mSTemp[2] >= 0) requiredTemps += mSTemp[2];

	return changed;
}

void InterInstruction::BuildCallerSaveTempSet(NumberSet& requiredTemps, NumberSet& callerSaveTemps)
{
	if (mTTemp >= 0)
		requiredTemps -= mTTemp;

	if (mCode == IC_CALL || mCode == IC_CALL_NATIVE)
		callerSaveTemps |= requiredTemps;

	if (mSTemp[0] >= 0) requiredTemps += mSTemp[0];
	if (mSTemp[1] >= 0) requiredTemps += mSTemp[1];
	if (mSTemp[2] >= 0) requiredTemps += mSTemp[2];
}

bool InterInstruction::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, NumberSet& requiredTemps)
{
	bool	changed = false;

	if (mCode == IC_LOAD)
	{
		if (mMemory == IM_LOCAL)
		{
			requiredTemps += mVarIndex;
		}
	}
	else if (mCode == IC_STORE)
	{
		if (mMemory == IM_LOCAL)
		{
			if (localVars[mVarIndex]->mAliased)
				;
			else if (requiredTemps[mVarIndex])
			{
				if (mSIntConst[1] == 0 && mOperandSize == localVars[mVarIndex]->mSize)
					requiredTemps -= mVarIndex;
			}
			else
			{
				mCode = IC_NONE;
				changed = true;
			}
		}
	}

	return changed;
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

			if (ins->mSTemp[0] == temp || ins->mSTemp[1] == temp || ins->mSTemp[2] == temp)
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
	DestroySourceValues(mTTemp, tvalue, tvalid);

	if (mCode == IC_LOAD_TEMPORARY)
	{
		if (tvalue[mSTemp[0]])
		{
			tvalue[mTTemp] = tvalue[mSTemp[0]];
			tvalid += mTTemp;
		}
	}
	else
	{
		if (mTTemp >= 0)
		{
			tvalue[mTTemp] = this;
			tvalid += mTTemp;
		}
	}
}

void InterInstruction::LocalRenameRegister(GrowingIntArray& renameTable, int& num, int fixed)
{
	if (mSTemp[0] >= 0) mSTemp[0] = renameTable[mSTemp[0]];
	if (mSTemp[1] >= 0) mSTemp[1] = renameTable[mSTemp[1]];
	if (mSTemp[2] >= 0) mSTemp[2] = renameTable[mSTemp[2]];

	if (mTTemp >= fixed)
	{
		renameTable[mTTemp] = num;
		mTTemp = num++;
	}
}

void InterInstruction::GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	if (mSTemp[0] >= 0) mSTemp[0] = renameTable[mSTemp[0]];
	if (mSTemp[1] >= 0) mSTemp[1] = renameTable[mSTemp[1]];
	if (mSTemp[2] >= 0) mSTemp[2] = renameTable[mSTemp[2]];

	if (mTTemp >= 0)
	{
		mTTemp = renameTable[mTTemp];
		if (InterTypeSize[mTType] > InterTypeSize[temporaries[mTTemp]])
			temporaries[mTTemp] = mTType;
	}
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
	if (mTTemp >= 0)
	{
		//		if (!liveTemps[ttemp]) __asm int 3
		liveTemps -= mTTemp;
	}

	UpdateCollisionSet(liveTemps, collisionSets, mSTemp[0]);
	UpdateCollisionSet(liveTemps, collisionSets, mSTemp[1]);
	UpdateCollisionSet(liveTemps, collisionSets, mSTemp[2]);
}

void InterInstruction::ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	if (mSTemp[0] >= 0) mSTemp[0] = renameTable[mSTemp[0]];
	if (mSTemp[1] >= 0) mSTemp[1] = renameTable[mSTemp[1]];
	if (mSTemp[2] >= 0) mSTemp[2] = renameTable[mSTemp[2]];

	if (mTTemp >= 0)
	{
		mTTemp = renameTable[mTTemp];
		temporaries[mTTemp] = mTType;
	}
}


void InterInstruction::CollectActiveTemporaries(FastNumberSet& set)
{
	if (mTTemp >= 0) set += mTTemp;
	if (mSTemp[0] >= 0) set += mSTemp[0];
	if (mSTemp[1] >= 0) set += mSTemp[1];
	if (mSTemp[2] >= 0) set += mSTemp[2];
}

void InterInstruction::ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries)
{
	if (mTTemp >= 0)
	{
		mTTemp = set.Index(mTTemp);
		temporaries[mTTemp] = mTType;
	}
	if (mSTemp[0] >= 0) mSTemp[0] = set.Index(mSTemp[0]);
	if (mSTemp[1] >= 0) mSTemp[1] = set.Index(mSTemp[1]);
	if (mSTemp[2] >= 0) mSTemp[2] = set.Index(mSTemp[2]);
}

void InterInstruction::CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes)
{
	switch (mCode)
	{
	case IC_LOAD:
		if (mMemory == IM_LOCAL && mSTemp[0] < 0)
		{
			if (localTypes[mVarIndex] == IT_NONE || localTypes[mVarIndex] == mTType)
			{
				localTypes[mVarIndex] = mTType;
				simpleLocals += mVarIndex;
			}
			else
				complexLocals += mVarIndex;
		}
		else if (mMemory == IM_PARAM && mSTemp[0] < 0)
		{
			if (paramTypes[mVarIndex] == IT_NONE || paramTypes[mVarIndex] == mTType)
			{
				paramTypes[mVarIndex] = mTType;
				simpleParams += mVarIndex;
			}
			else
				complexParams += mVarIndex;
		}
		break;
	case IC_STORE:
		if (mMemory == IM_LOCAL && mSTemp[1] < 0)
		{
			if (localTypes[mVarIndex] == IT_NONE || localTypes[mVarIndex] == mSType[0])
			{
				localTypes[mVarIndex] = mSType[0];
				simpleLocals += mVarIndex;
			}
			else
				complexLocals += mVarIndex;
		}
		else if (mMemory == IM_PARAM && mSTemp[1] < 0)
		{
			if (paramTypes[mVarIndex] == IT_NONE || paramTypes[mVarIndex] == mSType[0])
			{
				paramTypes[mVarIndex] = mSType[0];
				simpleParams += mVarIndex;
			}
			else
				complexParams += mVarIndex;
		}
		break;
	case IC_LEA:
		if (mMemory == IM_LOCAL && mSTemp[1] < 0)
			complexLocals += mVarIndex;
		else if (mMemory == IM_PARAM && mSTemp[1] < 0)
			complexParams += mVarIndex;
		break;
	case IC_CONSTANT:
		if (mTType == IT_POINTER && mMemory == IM_LOCAL)
			complexLocals += mVarIndex;
		else if (mTType == IT_POINTER && mMemory == IM_PARAM)
			complexParams += mVarIndex;
		break;
	}
}

void InterInstruction::SimpleLocalToTemp(int vindex, int temp)
{
	switch (mCode)
	{
	case IC_LOAD:
		if (mMemory == IM_LOCAL && mSTemp[0] < 0 && vindex == this->mVarIndex)
		{
			mCode = IC_LOAD_TEMPORARY;
			mSTemp[0] = temp;
			mSType[0] = mTType;

			assert(mSTemp[0] >= 0);

		}
		break;
	case IC_STORE:
		if (mMemory == IM_LOCAL && mSTemp[1] < 0 && vindex == this->mVarIndex)
		{
			if (mSTemp[0] < 0)
			{
				mCode = IC_CONSTANT;
				mIntValue = mSIntConst[0];
				mFloatValue = mSFloatConst[0];
			}
			else
			{
				mCode = IC_LOAD_TEMPORARY;
				assert(mSTemp[0] >= 0);
			}

			mTTemp = temp;
			mTType = mSType[0];
		}
		break;
	}
}

void InterInstruction::Disassemble(FILE* file)
{
	if (this->mCode != IC_NONE)
	{
		static char memchars[] = "NPLGFPITA";

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
			fprintf(file, "UNOP");
			break;
		case IC_RELATIONAL_OPERATOR:
			fprintf(file, "RELOP");
			break;
		case IC_CONVERSION_OPERATOR:
			fprintf(file, "CONV");
			break;
		case IC_STORE:
			fprintf(file, "STORE%c%d", memchars[mMemory], mOperandSize);
			break;
		case IC_LOAD:
			fprintf(file, "LOAD%c%d", memchars[mMemory], mOperandSize);
			break;
		case IC_COPY:
			fprintf(file, "COPY%c", memchars[mMemory]);
			break;
		case IC_LEA:
			fprintf(file, "LEA%c", memchars[mMemory]);
			break;
		case IC_TYPECAST:
			fprintf(file, "CAST");
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
		case IC_PUSH_FRAME:
			fprintf(file, "PUSHF\t%d", int(mIntValue));
			break;
		case IC_POP_FRAME:
			fprintf(file, "POPF\t%d", int(mIntValue));
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
		}
		static char typechars[] = "NBCILFP";

		fprintf(file, "\t");
		if (mTTemp >= 0) fprintf(file, "R%d(%c)", mTTemp, typechars[mTType]);
		fprintf(file, "\t<-\t");
		if (mSTemp[2] >= 0) fprintf(file, "R%d(%c%c), ", mSTemp[2], typechars[mSType[2]], mSFinal[2] ? 'F' : '-');
		if (mSTemp[1] >= 0) 
			fprintf(file, "R%d(%c%c), ", mSTemp[1], typechars[mSType[1]], mSFinal[1] ? 'F' : '-');
		else if (this->mCode == IC_STORE)
			fprintf(file, "V%d+%d, ", mVarIndex, int(mSIntConst[1]));
		if (mSTemp[0] >= 0)
			fprintf(file, "R%d(%c%c)", mSTemp[0], typechars[mSType[0]], mSFinal[0] ? 'F' : '-');
		else if (this->mCode == IC_LOAD)
			fprintf(file, "V%d+%d", mVarIndex, int(mSIntConst[0]));
		if (this->mCode == IC_CONSTANT)
		{
			if (mTType == IT_POINTER)
			{
				fprintf(file, "C%c%d(%d:%d)", memchars[mMemory], mOperandSize, mVarIndex, int(mIntValue));
			}
			else if (mTType == IT_FLOAT)
				fprintf(file, "C%f", mFloatValue);
			else
			{
#ifdef _WIN32
				fprintf(file, "C%I64d", mIntValue);
#else
				fprintf(file, "C%lld", mIntValue);
#endif
			}
		}

		fprintf(file, "\n");
	}
}

InterCodeBasicBlock::InterCodeBasicBlock(void)
	: mInstructions(nullptr), mEntryRenameTable(-1), mExitRenameTable(-1), mMergeTValues(nullptr), mTrueJump(nullptr), mFalseJump(nullptr), mDominator(nullptr)
{
	mInPath = false;
	mLoopHead = false;
}

InterCodeBasicBlock::~InterCodeBasicBlock(void)
{
}


void InterCodeBasicBlock::Append(InterInstruction * code)
{
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

void InterCodeBasicBlock::GenerateTraces(void)
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
			if (mTrueJump && mTrueJump->mInstructions.Size() == 1 && mTrueJump->mInstructions[0]->mCode == IC_JUMP)
			{
				mTrueJump->mNumEntries--;
				mTrueJump = mTrueJump->mTrueJump;
				if (mTrueJump)
					mTrueJump->mNumEntries++;
			}
			else if (mFalseJump && mFalseJump->mInstructions.Size() == 1 && mFalseJump->mInstructions[0]->mCode == IC_JUMP)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = mFalseJump->mTrueJump;
				if (mFalseJump)
					mFalseJump->mNumEntries++;
			}
			else if (mTrueJump && !mFalseJump && ((mTrueJump->mInstructions.Size() < 10 && mTrueJump->mInstructions.Size() > 1) || mTrueJump->mNumEntries == 1))
			{
				mTrueJump->mNumEntries--;

				mInstructions.Pop();
				for (i = 0; i < mTrueJump->mInstructions.Size(); i++)
					mInstructions.Push(new InterInstruction(* (mTrueJump->mInstructions[i]) ));

				mFalseJump = mTrueJump->mFalseJump;
				mTrueJump = mTrueJump->mTrueJump;

				if (mTrueJump)
					mTrueJump->mNumEntries++;
				if (mFalseJump)
					mFalseJump->mNumEntries++;
			}
			else
				break;
		}

		if (mTrueJump) mTrueJump->GenerateTraces();
		if (mFalseJump) mFalseJump->GenerateTraces();

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
	ins->mSIntConst[offset] = 0;

	if (ins->mSTemp[offset] >= 0 && tvalue[ins->mSTemp[offset]])
	{
		InterInstruction* ains = tvalue[ins->mSTemp[offset]];

		if (ains->mCode == IC_CONSTANT)
		{
			ins->mSIntConst[offset] = ains->mIntValue;
			ins->mLinkerObject = ains->mLinkerObject;
			ins->mVarIndex = ains->mVarIndex;
			ins->mMemory = ains->mMemory;
			ins->mSTemp[offset] = -1;
		}
		else if (ains->mCode == IC_LEA && ains->mSTemp[0] < 0 && ains->mSTemp[1] >= 0 && tvalue[ains->mSTemp[1]])
		{
			ins->mSIntConst[offset] = ains->mSIntConst[0];
			ins->mSTemp[offset] = ains->mSTemp[1];
		}
	}
}


void InterCodeBasicBlock::CheckValueUsage(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue)
{
	switch (ins->mCode)
	{
	case IC_CALL:
	case IC_CALL_NATIVE:
	case IC_ASSEMBLER:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			ins->mMemory = tvalue[ins->mSTemp[0]]->mMemory;
			ins->mLinkerObject = tvalue[ins->mSTemp[0]]->mLinkerObject;
			ins->mVarIndex = tvalue[ins->mSTemp[0]]->mVarIndex;
			ins->mOperandSize = tvalue[ins->mSTemp[0]]->mOperandSize;
			ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
			ins->mSTemp[0] = -1;
		}

		break;
	case IC_LOAD_TEMPORARY:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			switch (ins->mSType[0])
			{
			case IT_FLOAT:
				ins->mCode = IC_CONSTANT;
				ins->mFloatValue = tvalue[ins->mSTemp[0]]->mFloatValue;
				ins->mSTemp[0] = -1;
				break;
			case IT_POINTER:
				ins->mCode = IC_CONSTANT;
				ins->mMemory = tvalue[ins->mSTemp[0]]->mMemory;
				ins->mLinkerObject = tvalue[ins->mSTemp[0]]->mLinkerObject;
				ins->mVarIndex = tvalue[ins->mSTemp[0]]->mVarIndex;
				ins->mIntValue = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mOperandSize = tvalue[ins->mSTemp[0]]->mOperandSize;
				ins->mSTemp[0] = -1;
				break;
			default:
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;
				break;
			}
		}
		break;

	case IC_LOAD:
		OptimizeAddress(ins, tvalue, 0);
		break;
	case IC_STORE:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			switch (ins->mSType[0])
			{
			case IT_FLOAT:
				ins->mSFloatConst[0] = tvalue[ins->mSTemp[0]]->mFloatValue;
				ins->mSTemp[0] = -1;
				break;
			case IT_POINTER:
				break;
			default:
				ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;
				break;
			}
		}
		OptimizeAddress(ins, tvalue, 1);
		break;
	case IC_LEA:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mMemory = tvalue[ins->mSTemp[1]]->mMemory;
				ins->mLinkerObject = tvalue[ins->mSTemp[1]]->mLinkerObject;
				ins->mVarIndex = tvalue[ins->mSTemp[1]]->mVarIndex;
				ins->mIntValue = tvalue[ins->mSTemp[1]]->mIntValue + tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mOperandSize = tvalue[ins->mSTemp[1]]->mOperandSize;
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;
			}
			else if (tvalue[ins->mSTemp[0]]->mIntValue == 0)
			{
				ins->mCode = IC_LOAD_TEMPORARY;
				ins->mSType[0] = ins->mSType[1];
				ins->mSTemp[0] = ins->mSTemp[1];
				ins->mSTemp[1] = -1;
				assert(ins->mSTemp[0] >= 0);
			}
			else
			{
				ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;
			}
		}
		break;
	case IC_TYPECAST:
		if (ins->mSType[0] == ins->mTType)
		{
			ins->mCode = IC_LOAD_TEMPORARY;
			assert(ins->mSTemp[0] >= 0);
		}
		else if (TypeInteger(ins->mSType[0]) && ins->mTType == IT_POINTER)
		{
			if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mTType = IT_POINTER;
				ins->mMemory = IM_ABSOLUTE;
				ins->mVarIndex = 0;
				ins->mIntValue = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;
			}
		}
		break;
	case IC_RETURN_VALUE:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			switch (ins->mSType[0])
			{
			case IT_FLOAT:
				break;
			case IT_POINTER:
				break;
			default:
				ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;
				break;
			}
		}
		break;
	case IC_BINARY_OPERATOR:
		switch (ins->mSType[0])
		{
		case IT_FLOAT:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
			{
				if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
				{
					ins->mCode = IC_CONSTANT;
					ins->mFloatValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mFloatValue, tvalue[ins->mSTemp[0]]->mFloatValue);
					ins->mSTemp[0] = -1;
					ins->mSTemp[1] = -1;
				}
				else
				{
					ins->mSFloatConst[1] = tvalue[ins->mSTemp[1]]->mFloatValue;
					ins->mSTemp[1] = -1;

					if (ins->mOperator == IA_ADD && ins->mSFloatConst[1] == 0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						assert(ins->mSTemp[0] >= 0);
					}
					else if (ins->mOperator == IA_MUL)
					{
						if (ins->mSFloatConst[1] == 1.0)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							assert(ins->mSTemp[0] >= 0);
						}
						else if (ins->mSFloatConst[1] == 0.0)
						{
							ins->mCode = IC_CONSTANT;
							ins->mFloatValue = 0.0;
							ins->mSTemp[0] = -1;
							ins->mSTemp[1] = -1;
						}
						else if (ins->mSFloatConst[1] == 2.0)
						{
							ins->mOperator = IA_ADD;
							ins->mSTemp[1] = ins->mSTemp[0];
							assert(ins->mSTemp[0] >= 0);
						}
					}
				}
			}
			else if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mSFloatConst[0] = tvalue[ins->mSTemp[0]]->mFloatValue;
				ins->mSTemp[0] = -1;

				if (ins->mOperator == IA_ADD && ins->mSFloatConst[0] == 0)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = ins->mSTemp[1];
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);
				}
				else if (ins->mOperator == IA_MUL)
				{
					if (ins->mSFloatConst[0] == 1.0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSTemp[0] = ins->mSTemp[1];
						ins->mSTemp[1] = -1;
						assert(ins->mSTemp[0] >= 0);
					}
					else if (ins->mSFloatConst[0] == 0.0)
					{
						ins->mCode = IC_CONSTANT;
						ins->mFloatValue = 0.0;
						ins->mSTemp[0] = -1;
						ins->mSTemp[1] = -1;
					}
					else if (ins->mSFloatConst[0] == 2.0)
					{
						ins->mOperator = IA_ADD;
						ins->mSTemp[0] = ins->mSTemp[1];
						assert(ins->mSTemp[0] >= 0);
					}
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
			{
				if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
				{
					ins->mCode = IC_CONSTANT;
					ins->mIntValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mIntValue, tvalue[ins->mSTemp[0]]->mIntValue);
					ins->mSTemp[0] = -1;
					ins->mSTemp[1] = -1;
				}
				else
				{
					ins->mSIntConst[1] = tvalue[ins->mSTemp[1]]->mIntValue;
					ins->mSTemp[1] = -1;
#if 1
					if (ins->mOperator == IA_ADD && ins->mSIntConst[1] == 0)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						assert(ins->mSTemp[0] >= 0);
					}
					else if (ins->mOperator == IA_MUL)
					{
						if (ins->mSIntConst[1] == 1)
						{
							ins->mCode = IC_LOAD_TEMPORARY;
							assert(ins->mSTemp[0] >= 0);
						}
						else if (ins->mSIntConst[1] == 2)
						{
							ins->mOperator = IA_SHL;
							ins->mSTemp[1] = ins->mSTemp[0];
							ins->mSType[1] = ins->mSType[0];
							ins->mSTemp[0] = -1;
							ins->mSIntConst[0] = 1;
						}
						else if (ins->mSIntConst[1] == 4)
						{
							ins->mOperator = IA_SHL;
							ins->mSTemp[1] = ins->mSTemp[0];
							ins->mSType[1] = ins->mSType[0];
							ins->mSTemp[0] = -1;
							ins->mSIntConst[0] = 2;
						}
						else if (ins->mSIntConst[1] == 8)
						{
							ins->mOperator = IA_SHL;
							ins->mSTemp[1] = ins->mSTemp[0];
							ins->mSType[1] = ins->mSType[0];
							ins->mSTemp[0] = -1;
							ins->mSIntConst[0] = 3;
						}

					}
#endif
				}
			}
			else if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
				ins->mSTemp[0] = -1;

				if (ins->mOperator == IA_ADD && ins->mSIntConst[0] == 0)
				{
					ins->mCode = IC_LOAD_TEMPORARY;
					ins->mSTemp[0] = ins->mSTemp[1];
					ins->mSTemp[1] = -1;
					assert(ins->mSTemp[0] >= 0);
				}
				else if (ins->mOperator == IA_MUL)
				{
					if (ins->mSIntConst[0] == 1)
					{
						ins->mCode = IC_LOAD_TEMPORARY;
						ins->mSTemp[0] = ins->mSTemp[1];
						ins->mSTemp[1] = -1;
						assert(ins->mSTemp[0] >= 0);
					}
					else if (ins->mSIntConst[0] == 2)
					{
						ins->mOperator = IA_SHL;
						ins->mSIntConst[0] = 1;
					}
					else if (ins->mSIntConst[0] == 4)
					{
						ins->mOperator = IA_SHL;
						ins->mSIntConst[0] = 2;
					}
					else if (ins->mSIntConst[0] == 8)
					{
						ins->mOperator = IA_SHL;
						ins->mSIntConst[0] = 3;
					}
				}
			}

			if (ins->mSTemp[1] < 0 && ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins->mSTemp[0]];
				if (ins->mOperator == pins->mOperator && (ins->mOperator == IA_ADD || ins->mOperator == IA_MUL || ins->mOperator == IA_AND || ins->mOperator == IA_OR))
				{
					if (pins->mSTemp[1] < 0)
					{
						ins->mSIntConst[1] = ConstantFolding(ins->mOperator, ins->mSIntConst[1], pins->mSIntConst[1]);
						ins->mSTemp[0] = pins->mSTemp[0];
					}
					else if (pins->mSTemp[0] < 0)
					{
						ins->mSIntConst[1] = ConstantFolding(ins->mOperator, ins->mSIntConst[1], pins->mSIntConst[0]);
						ins->mSTemp[0] = pins->mSTemp[1];
					}
				}
			}
			else if (ins->mSTemp[0] < 0 && ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins->mSTemp[1]];
				if (ins->mOperator == pins->mOperator && (ins->mOperator == IA_ADD || ins->mOperator == IA_MUL || ins->mOperator == IA_AND || ins->mOperator == IA_OR))
				{
					if (pins->mSTemp[1] < 0)
					{
						ins->mSIntConst[0] = ConstantFolding(ins->mOperator, ins->mSIntConst[0], pins->mSIntConst[1]);
						ins->mSTemp[1] = pins->mSTemp[0];
					}
					else if (pins->mSTemp[0] < 0)
					{
						ins->mSIntConst[0] = ConstantFolding(ins->mOperator, ins->mSIntConst[0], pins->mSIntConst[0]);
						ins->mSTemp[1] = pins->mSTemp[1];
					}
				}
				else if (ins->mOperator == IA_SHL && (pins->mOperator == IA_SHR || pins->mOperator == IA_SAR) && pins->mSTemp[0] < 0 && ins->mSIntConst[0] == pins->mSIntConst[0])
				{
					ins->mOperator = IA_AND;
					ins->mSIntConst[0] = -1LL << ins->mSIntConst[0];
					ins->mSTemp[1] = pins->mSTemp[1];
				}
			}

			break;
		}
		break;
	case IC_UNARY_OPERATOR:
		switch (ins->mSType[0])
		{
		case IT_FLOAT:
			if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mFloatValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[0]]->mFloatValue);
				ins->mSTemp[0] = -1;
			}
			break;
		case IT_POINTER:
			break;
		default:
			break;
		}
		break;
	case IC_RELATIONAL_OPERATOR:
		switch (ins->mSType[1])
		{
		case IT_FLOAT:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantRelationalFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mFloatValue, tvalue[ins->mSTemp[0]]->mFloatValue);
				ins->mTType = IT_BOOL;
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;
			}
			else
			{
				if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
				{
					ins->mSFloatConst[1] = tvalue[ins->mSTemp[1]]->mFloatValue;
					ins->mSTemp[1] = -1;
				}
				else if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
				{
					ins->mSFloatConst[0] = tvalue[ins->mSTemp[0]]->mFloatValue;
					ins->mSTemp[0] = -1;
				}
			}
			break;
		case IT_POINTER:
			if (ins->mOperator == IA_CMPEQ || ins->mOperator == IA_CMPNE)
			{
				if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
				{
					ins->mOperandSize = tvalue[ins->mSTemp[0]]->mOperandSize;
					ins->mSTemp[0] = -1;
				}
				else if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
				{
					ins->mOperandSize = tvalue[ins->mSTemp[1]]->mOperandSize;
					ins->mSTemp[1] = -1;
				}
			}
			break;
		default:
			if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT &&
				ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
			{
				ins->mCode = IC_CONSTANT;
				ins->mIntValue = ConstantFolding(ins->mOperator, tvalue[ins->mSTemp[1]]->mIntValue, tvalue[ins->mSTemp[0]]->mIntValue);
				ins->mSTemp[0] = -1;
				ins->mSTemp[1] = -1;
			}
			else
			{
				if (ins->mSTemp[1] >= 0 && tvalue[ins->mSTemp[1]] && tvalue[ins->mSTemp[1]]->mCode == IC_CONSTANT)
				{
					ins->mSIntConst[1] = tvalue[ins->mSTemp[1]]->mIntValue;
					ins->mSTemp[1] = -1;
				}
				else if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
				{
					ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
					ins->mSTemp[0] = -1;
				}
			}
			break;
		}
		break;
	case IC_BRANCH:
		if (ins->mSTemp[0] >= 0 && tvalue[ins->mSTemp[0]] && tvalue[ins->mSTemp[0]]->mCode == IC_CONSTANT)
		{
			ins->mSIntConst[0] = tvalue[ins->mSTemp[0]]->mIntValue;
			ins->mSTemp[0] = -1;
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

void  InterCodeBasicBlock::CollectConstTemps(GrowingInstructionPtrArray& ctemps, NumberSet& assignedTemps)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		for (i = 0; i < mInstructions.Size(); i++)
		{
			int		ttemp = mInstructions[i]->mTTemp;
			if (ttemp >= 0)
			{
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

void InterCodeBasicBlock::BuildLocalTempSets(int num, int numFixed)
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
		exitProvidedTemps = NumberSet(num);

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->FilterTempUsage(mLocalRequiredTemps, mLocalProvidedTemps);
		}

		mEntryRequiredTemps = mLocalRequiredTemps;
		exitProvidedTemps = mLocalProvidedTemps;

		for (i = 0; i < numFixed; i++)
		{
			mEntryRequiredTemps += i;
			exitProvidedTemps += i;
		}

		if (mTrueJump) mTrueJump->BuildLocalTempSets(num, numFixed);
		if (mFalseJump) mFalseJump->BuildLocalTempSets(num, numFixed);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedTempSet(NumberSet fromProvidedTemps)
{
	if (!mVisited || !(fromProvidedTemps <= mEntryProvidedTemps))
	{
		mEntryProvidedTemps |= fromProvidedTemps;
		fromProvidedTemps |= exitProvidedTemps;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedTempSet(fromProvidedTemps);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedTempSet(fromProvidedTemps);
	}
}

void InterCodeBasicBlock::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	int i;

	if (!mVisited)
	{
		TempForwardingTable	localForwardingTable(forwardingTable);

		if (mLoopHead)
		{
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
	int	i;

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

bool InterCodeBasicBlock::RemoveUnusedResultInstructions(int numStaticTemps)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredTemps(mExitRequiredTemps);
		int i;

		for (i = 0; i < numStaticTemps; i++)
			requiredTemps += i;

		if (mInstructions.Size() > 0)
		{
			for (i = mInstructions.Size() - 1; i > 0; i--)
			{
				if (mInstructions[i]->RemoveUnusedResultInstructions(mInstructions[i - 1], requiredTemps, numStaticTemps))
					changed = true;
			}
			if (mInstructions[0]->RemoveUnusedResultInstructions(NULL, requiredTemps, numStaticTemps))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedResultInstructions(numStaticTemps))
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedResultInstructions(numStaticTemps))
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


void InterCodeBasicBlock::BuildLocalVariableSets(const GrowingVariableArray& localVars)
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

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->FilterVarsUsage(localVars, mLocalRequiredVars, mLocalProvidedVars);
		}

		mEntryRequiredVars = mLocalRequiredVars;
		mExitProvidedVars = mLocalProvidedVars;

		if (mTrueJump) mTrueJump->BuildLocalVariableSets(localVars);
		if (mFalseJump) mFalseJump->BuildLocalVariableSets(localVars);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedVariableSet(const GrowingVariableArray& localVars, NumberSet fromProvidedVars)
{
	if (!mVisited || !(fromProvidedVars <= mEntryProvidedVars))
	{
		mEntryProvidedVars |= fromProvidedVars;
		fromProvidedVars |= mExitProvidedVars;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredVariableSet(const GrowingVariableArray& localVars, NumberSet& fromRequiredVars)
{
	bool revisit = false;
	int	i;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	newRequiredVars(mExitRequiredVars);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars)) revisit = true;

		if (!(newRequiredVars <= mExitRequiredVars))
		{
			revisit = true;

			mExitRequiredVars = newRequiredVars;
			newRequiredVars -= mLocalProvidedVars;
			mEntryRequiredVars |= newRequiredVars;
		}

	}

	fromRequiredVars |= mEntryRequiredVars;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredVars(mExitRequiredVars);
		int i;

		for (i = mInstructions.Size() - 1; i >= 0; i--)
		{
			if (mInstructions[i]->RemoveUnusedStoreInstructions(localVars, requiredVars))
				changed = true;
		}

		if (mTrueJump)
		{
			if (mTrueJump->RemoveUnusedStoreInstructions(localVars))
				changed = true;
		}
		if (mFalseJump)
		{
			if (mFalseJump->RemoveUnusedStoreInstructions(localVars))
				changed = true;
		}
	}

	return changed;

}

void InterCodeBasicBlock::PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals, const NumberSet& aliasedParams)
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
			lvalues.UpdateValue(mInstructions[i], ltvalue, aliasedLocals, aliasedParams);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams);
		if (mFalseJump) mFalseJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals, aliasedParams);
	}
}

void InterCodeBasicBlock::PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
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
			CheckValueUsage(mInstructions[i], ltvalue);
			mInstructions[i]->PerformValueForwarding(ltvalue, tvalid);
		}

		if (mTrueJump) mTrueJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid);
		if (mFalseJump) mFalseJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid);
	}
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


void InterCodeBasicBlock::LocalRenameRegister(const GrowingIntArray& renameTable, int& num, int fixed)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mEntryRenameTable.SetSize(renameTable.Size());
		mExitRenameTable.SetSize(renameTable.Size());

		for (i = 0; i < renameTable.Size(); i++)
		{
			if (i < fixed)
			{
				mEntryRenameTable[i] = i;
				mExitRenameTable[i] = i;
			}
			else if (mEntryRequiredTemps[i])
			{
				mEntryRenameTable[i] = renameTable[i];
				mExitRenameTable[i] = renameTable[i];
			}
			else
			{
				mEntryRenameTable[i] = -1;
				mExitRenameTable[i] = -1;
			}
		}

		for (i = 0; i < mInstructions.Size(); i++)
		{
			mInstructions[i]->LocalRenameRegister(mExitRenameTable, num, fixed);
		}

		if (mTrueJump) mTrueJump->LocalRenameRegister(mExitRenameTable, num, fixed);
		if (mFalseJump) mFalseJump->LocalRenameRegister(mExitRenameTable, num, fixed);
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
				if (mInstructions[i]->mTType != IT_POINTER)
					break;

			case IC_STORE:
			case IC_LOAD:
			case IC_CALL_NATIVE:
				if (mInstructions[i]->mMemory == IM_LOCAL)
				{
					localVars[mInstructions[i]->mVarIndex]->mUsed = true;
				}
				break;
			}
		}

		if (mTrueJump) mTrueJump->MapVariables(globalVars, localVars);
		if (mFalseJump) mFalseJump->MapVariables(globalVars, localVars);
	}
}

void InterCodeBasicBlock::CollectOuterFrame(int level, int& size)
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
					if (mInstructions[i]->mIntValue > size)
						size = mInstructions[i]->mIntValue;
					mInstructions[i]->mCode = IC_NONE;
				}
			}
			else if (mInstructions[i]->mCode == IC_POP_FRAME)
			{
				if (level == 1)
				{
					mInstructions[i]->mCode = IC_NONE;
				}
				level--;
			}
		}

		if (mTrueJump) mTrueJump->CollectOuterFrame(level, size);
		if (mFalseJump) mFalseJump->CollectOuterFrame(level, size);
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

static bool CanBypassLoad(const InterInstruction * lins, const InterInstruction * bins)
{
	// Check ambiguity
	if (bins->mCode == IC_STORE || bins->mCode == IC_COPY)
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;

	// True data dependency
	if (lins->mTTemp == bins->mSTemp[0] || lins->mTTemp == bins->mSTemp[1] || lins->mTTemp == bins->mSTemp[2])
		return false;

	// False data dependency
	if (lins->mSTemp[0] >= 0 && lins->mSTemp[0] == bins->mTTemp)
		return false;

	return true;
}

static bool CanBypassStore(const InterInstruction * sins, const InterInstruction * bins)
{
	if (bins->mCode == IC_COPY || bins->mCode == IC_PUSH_FRAME)
		return false;

	// Check ambiguity
	if (bins->mCode == IC_STORE || bins->mCode == IC_LOAD)
	{
		if (sins->mMemory == IM_LOCAL)
		{
			if (bins->mMemory == IM_PARAM || bins->mMemory == IM_GLOBAL)
				;
			else if (bins->mMemory == IM_LOCAL)
			{
				if (bins->mVarIndex == sins->mVarIndex)
					return false;
			}
			else
				return false;
		}
		else
			return false;
	}

	if (sins->mMemory == IM_FRAME && (bins->mCode == IC_PUSH_FRAME || bins->mCode == IC_POP_FRAME))
		return false;

	// Side effects
	if (bins->mCode == IC_CALL || bins->mCode == IC_CALL_NATIVE || bins->mCode == IC_ASSEMBLER)
		return false;

	// True data dependency
	if (bins->mTTemp >= 0 && (bins->mTTemp == sins->mSTemp[0] || bins->mTTemp == sins->mSTemp[1]))
		return false;

	return true;
}

InterCodeBasicBlock* InterCodeBasicBlock::PropagateDominator(InterCodeProcedure* proc)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump)
			mTrueJump = mTrueJump->PropagateDominator(proc);
		if (mFalseJump)
			mFalseJump = mFalseJump->PropagateDominator(proc);

		if (mLoopHead)
		{
			mDominator = new InterCodeBasicBlock();
			proc->Append(mDominator);
			mDominator->Close(this, nullptr);
		}
	}

	return mDominator ? mDominator : this;
}

bool IsMoveable(InterCode code)
{
	if (HasSideEffect(code) || code == IC_COPY || code == IC_STORE || code == IC_BRANCH || code == IC_POP_FRAME || code == IC_PUSH_FRAME)
		return false;
	if (code == IC_RETURN || code == IC_RETURN_STRUCT || code == IC_RETURN_VALUE)
		return false;

	return true;
}

void InterCodeBasicBlock::SingleBlockLoopOptimisation(void)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mLoopHead && mNumEntries == 2 && (mTrueJump == this || mFalseJump == this))
		{
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				ins->mInvariant = true;

				if (!IsMoveable(ins->mCode))
					ins->mInvariant = false;
				else if (ins->mCode == IC_LOAD)
				{
					if (ins->mSTemp[0] >= 0 || ins->mVolatile)
					{
						ins->mInvariant = false;
					}
					else
					{
						for (int j = 0; j < mInstructions.Size(); j++)
						{
							InterInstruction* sins = mInstructions[j];
							if (sins->mCode == IC_STORE)
							{
								if (sins->mSTemp[1] >= 0)
								{
									ins->mInvariant = false;
								}
								else if (ins->mMemory == sins->mMemory && ins->mVarIndex == sins->mVarIndex && ins->mLinkerObject == sins->mLinkerObject)
								{
									ins->mInvariant = false;
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

			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				int t = ins->mTTemp;
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

				for (int i = 0; i < mInstructions.Size(); i++)
				{
					InterInstruction* ins = mInstructions[i];
					int t = ins->mTTemp;
					if (t >= 0)
					{
						if (dep[t] < DEP_VARIABLE)
						{
							if (ins->mSTemp[0] >= 0 && dep[ins->mSTemp[0]] >= DEP_ITERATED ||
								ins->mSTemp[1] >= 0 && dep[ins->mSTemp[1]] >= DEP_ITERATED ||
								ins->mSTemp[2] >= 0 && dep[ins->mSTemp[2]] >= DEP_ITERATED)
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
					}
				}

			} while (changed);

#if 1
			int	j = 0;
			for (int i = 0; i < mInstructions.Size(); i++)
			{
				InterInstruction* ins = mInstructions[i];
				if (ins->mInvariant)
				{
					mDominator->mInstructions.Push(ins);
				}
				else
				{
					mInstructions[j++] = ins;
				}
			}
#ifdef _DEBUG
			if (j != mInstructions.Size())
				printf("Moved %d %d\n", mIndex, mInstructions.Size() - j);
#endif
			mInstructions.SetSize(j);
#endif
		}

		if (mTrueJump)
			mTrueJump->SingleBlockLoopOptimisation();
		if (mFalseJump)
			mFalseJump->SingleBlockLoopOptimisation();
	}
}

void InterCodeBasicBlock::PeepholeOptimization(void)
{
	int		i;
	
	if (!mVisited)
	{
		mVisited = true;

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

		int	limit = mInstructions.Size() - 1;
		if (limit >= 2 && mInstructions[limit]->mCode == IC_BRANCH)
			limit -= 2;

		int i = limit;

		while (i >= 0)
		{
			// move loads down
			if (mInstructions[i]->mCode == IC_LOAD)
			{
				InterInstruction	*	ins(mInstructions[i]);
				int j = i;
				while (j < limit && CanBypassLoad(ins, mInstructions[j + 1]))
				{
					mInstructions[j] = mInstructions[j + 1];
					j++;
				}
				if (i != j)
					mInstructions[j] = ins;
			}

			i--;
		}

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
					mInstructions[j] = mInstructions[j - 1];
					j--;
				}
				if (i != j)
					mInstructions[j] = ins;
			}

			i++;
		}

		bool	changed;
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
				if (i + 2 < mInstructions.Size())
				{
					if (mInstructions[i + 0]->mTTemp >= 0 &&
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mSTemp[0] == mInstructions[i]->mTTemp &&
						(mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR || mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR) && mInstructions[i + 2]->mSTemp[0] == mInstructions[i]->mTTemp && mInstructions[i + 2]->mSFinal[0])
					{
						mInstructions[i + 0]->mTTemp = mInstructions[i + 1]->mTTemp;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 2]->mSTemp[0] = mInstructions[i + 1]->mTTemp;
						mInstructions[i + 2]->mSFinal[0] = false;
						changed = true;
					}
					else if (mInstructions[i + 0]->mTTemp >= 0 &&
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mSTemp[0] == mInstructions[i]->mTTemp &&
						(mInstructions[i + 2]->mCode == IC_RELATIONAL_OPERATOR || mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR) && mInstructions[i + 2]->mSTemp[1] == mInstructions[i]->mTTemp && mInstructions[i + 2]->mSFinal[1])
					{
						mInstructions[i + 0]->mTTemp = mInstructions[i + 1]->mTTemp;
						mInstructions[i + 1]->mCode = IC_NONE;
						mInstructions[i + 2]->mSTemp[1] = mInstructions[i + 1]->mTTemp;
						mInstructions[i + 2]->mSFinal[1] = false;
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSTemp[0] < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSTemp[0] < 0 &&
						mInstructions[i + 1]->mSTemp[1] == mInstructions[i + 0]->mTTemp && mInstructions[i + 1]->mSFinal[1] &&
						(mInstructions[i + 1]->mSIntConst[0] & (1LL << mInstructions[i + 0]->mSIntConst[0])) == 0)
					{
						int	shift = mInstructions[i + 0]->mSIntConst[0];
						mInstructions[i + 1]->mSIntConst[0] >>= shift;
						mInstructions[i + 0]->mOperator = IA_AND;
						mInstructions[i + 0]->mSIntConst[0] = ~((1LL << shift) - 1);
						changed = true;
					}
					else if (
						mInstructions[i + 0]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 0]->mOperator == IA_SAR && mInstructions[i + 0]->mSTemp[0] < 0 &&
						mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR && mInstructions[i + 1]->mOperator == IA_MUL && mInstructions[i + 1]->mSTemp[1] < 0 &&
						mInstructions[i + 1]->mSTemp[0] == mInstructions[i + 0]->mTTemp && mInstructions[i + 1]->mSFinal[0] &&
						(mInstructions[i + 1]->mSIntConst[1] & (1LL << mInstructions[i + 0]->mSIntConst[0])) == 0)
					{
						int	shift = mInstructions[i + 0]->mSIntConst[0];
						mInstructions[i + 1]->mSIntConst[1] >>= shift;
						mInstructions[i + 0]->mOperator = IA_AND;
						mInstructions[i + 0]->mSIntConst[0] = ~((1LL << shift) - 1);
						changed = true;
					}
					else if (
						mInstructions[i + 1]->mCode == IC_LOAD_TEMPORARY && mExitRequiredTemps[mInstructions[i + 1]->mTTemp] && !mExitRequiredTemps[mInstructions[i + 1]->mSTemp[0]] &&
						mInstructions[i + 0]->mTTemp == mInstructions[i + 1]->mSTemp[0])
					{
						mInstructions[i + 0]->mTTemp = mInstructions[i + 1]->mTTemp;
						mInstructions[i + 1]->mTTemp = mInstructions[i + 1]->mSTemp[0];
						mInstructions[i + 1]->mSTemp[0] = mInstructions[i + 0]->mTTemp;
						changed = true;
					}

					// Postincrement artifact
					if (mInstructions[i + 0]->mCode == IC_LOAD_TEMPORARY && mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
						mInstructions[i + 1]->mSTemp[0] < 0 &&
						mInstructions[i + 0]->mSTemp[0] == mInstructions[i + 1]->mSTemp[1] &&
						mInstructions[i + 0]->mSTemp[0] == mInstructions[i + 1]->mTTemp)
					{
						InterInstruction	*	ins = mInstructions[i + 1];
						int		ttemp = mInstructions[i + 1]->mTTemp;
						int	k = i + 1;
						while (k + 2 < mInstructions.Size() &&
							mInstructions[k + 1]->mCode != IC_RELATIONAL_OPERATOR &&
							mInstructions[k + 1]->mSTemp[0] != ttemp &&
							mInstructions[k + 1]->mSTemp[1] != ttemp &&
							mInstructions[k + 1]->mSTemp[2] != ttemp &&
							mInstructions[k + 1]->mTTemp != ttemp)
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
				}


			}

		} while (changed);

		if (mTrueJump) mTrueJump->PeepholeOptimization();
		if (mFalseJump) mFalseJump->PeepholeOptimization();
	}
}


void InterCodeBasicBlock::CollectVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars)
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
			case IC_STORE:
			case IC_LOAD:							
			case IC_CONSTANT:
			case IC_CALL_NATIVE:
			case IC_ASSEMBLER:
				if (mInstructions[i]->mMemory == IM_LOCAL)
				{
					int varIndex = mInstructions[i]->mVarIndex;
					if (!localVars[varIndex])
						localVars[varIndex] = new InterVariable;

					int	size = mInstructions[i]->mOperandSize + mInstructions[i]->mIntValue;
					if (size > localVars[varIndex]->mSize)
						localVars[varIndex]->mSize = size;
					if (mInstructions[i]->mCode == IC_CONSTANT)
						localVars[varIndex]->mAliased = true;
				}
				break;
			}
		}

		if (mTrueJump) mTrueJump->CollectVariables(globalVars, localVars);
		if (mFalseJump) mFalseJump->CollectVariables(globalVars, localVars);
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
		fprintf(file, "L%d: (%d) %s\n", mIndex, mNumEntries, s);

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
	mValueForwardingTable(nullptr), mLocalVars(nullptr), mModule(mod),
	mIdent(ident), mLinkerObject(linkerObject),
	mNativeProcedure(false), mLeafProcedure(false), mCallsFunctionPointer(false), mCalledFunctions(nullptr)
{
	mID = mModule->mProcedures.Size();
	mModule->mProcedures.Push(this);
	mLinkerObject->mProc = this;
	mCallerSavedTemps = 16;
}

InterCodeProcedure::~InterCodeProcedure(void)
{
}

void InterCodeProcedure::ResetVisited(void)
{
	int i;

	for (i = 0; i < mBlocks.Size(); i++)
	{
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

void InterCodeProcedure::DisassembleDebug(const char* name)
{
	Disassemble(name);
}

void InterCodeProcedure::BuildTraces(void)
{
	// Count number of entries
//
	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	//
	// Build traces
	//
	ResetVisited();
	mEntryBlock->GenerateTraces();

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();

	DisassembleDebug("BuildTraces");
}

void InterCodeProcedure::BuildDataFlowSets(void)
{
	int	numTemps = mTemporaries.Size();

	//
	//	Build set with local provided/required temporaries
	//
	ResetVisited();
	mEntryBlock->BuildLocalTempSets(numTemps, numFixedTemporaries);

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
}

void InterCodeProcedure::RenameTemporaries(void)
{
	int	numTemps = mTemporaries.Size();

	//
	// Now we can rename temporaries to remove false dependencies
	//
	mRenameTable.SetSize(numTemps, true);

	int		i, j, numRename;

	numRename = numFixedTemporaries;
	for (i = 0; i < numRename; i++)
		mRenameTable[i] = i;

	//
	// First localy rename all temporaries
	//
	ResetVisited();
	mEntryBlock->LocalRenameRegister(mRenameTable, numRename, numFixedTemporaries);

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

	for (i = 0; i < numFixedTemporaries; i++)
		mGlobalRenameTable[i] = i;

	numRenamedTemps = numFixedTemporaries;

	for (i = numFixedTemporaries; i < numRename; i++)
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

void InterCodeProcedure::TempForwarding(void)
{
	int	numTemps = mTemporaries.Size();

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
		mEntryBlock->BuildLocalTempSets(numTemps, numFixedTemporaries);

		ResetVisited();
		mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (mEntryBlock->RemoveUnusedResultInstructions(numFixedTemporaries));
}

void InterCodeProcedure::Close(void)
{
	int				i, j, k, start;
	GrowingTypeArray	tstack(IT_NONE);

	numFixedTemporaries = 0;
	mEntryBlock = mBlocks[0];

	DisassembleDebug("start");

	BuildTraces();

	ResetVisited();
	mLeafProcedure = mEntryBlock->IsLeafProcedure();

	if (!mLeafProcedure)
	{
		int		size = 0;

		ResetVisited();
		mEntryBlock->CollectOuterFrame(0, size);
		mCommonFrameSize = size;
	}
	else
		mCommonFrameSize = 0;

	BuildDataFlowSets();

	RenameTemporaries();

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

	//
	//	Now forward constant values
	//
	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps);

	mValueForwardingTable.SetSize(numTemps, true);

	ResetVisited();
	mEntryBlock->PerformValueForwarding(mValueForwardingTable, valueSet, tvalidSet, mLocalAliasedSet, mParamAliasedSet);

	DisassembleDebug("value forwarding");

	mValueForwardingTable.Clear();

	ResetVisited();
	mEntryBlock->PerformMachineSpecificValueUsageCheck(mValueForwardingTable, tvalidSet);

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

	do {
		ResetVisited();
		mEntryBlock->BuildLocalTempSets(numTemps, numFixedTemporaries);

		ResetVisited();
		mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (mEntryBlock->RemoveUnusedResultInstructions(numFixedTemporaries));

	DisassembleDebug("removed unused instructions");

	ResetVisited();
	mEntryBlock->CollectVariables(mModule->mGlobalVars, mLocalVars);


	if (mLocalVars.Size() > 0)
	{
		for (int i = 0; i < mLocalVars.Size(); i++)
		{
			if (mLocalAliasedSet[i])
				mLocalVars[i]->mAliased = true;
		}

		//
		// Now remove unused stores
		//

		do {
			ResetVisited();
			mEntryBlock->BuildLocalVariableSets(mLocalVars);

			ResetVisited();
			mEntryBlock->BuildGlobalProvidedVariableSet(mLocalVars, NumberSet(mLocalVars.Size()));

			NumberSet	totalRequired2(mLocalVars.Size());

			do {
				ResetVisited();
			} while (mEntryBlock->BuildGlobalRequiredVariableSet(mLocalVars, totalRequired2));

			ResetVisited();
		} while (mEntryBlock->RemoveUnusedStoreInstructions(mLocalVars));

		DisassembleDebug("removed unused local stores");
	}

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
			ResetVisited();
			mEntryBlock->SimpleLocalToTemp(vi, AddTemporary(localTypes[vi]));
		}
	}

	DisassembleDebug("local variables to temps");

	BuildTraces();

	BuildDataFlowSets();

	RenameTemporaries();

	do {
		TempForwarding();
	} while (GlobalConstantPropagation());

	//
	// Now remove unused instructions
	//

	RemoveUnusedInstructions();

	DisassembleDebug("removed unused instructions 2");

	TempForwarding();

	BuildDominators();
	DisassembleDebug("added dominators");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Peephole optimized");

	ResetVisited();
	mEntryBlock->SingleBlockLoopOptimisation();

	DisassembleDebug("single block loop opt");

	BuildDataFlowSets();

	ResetVisited();
	mEntryBlock->PeepholeOptimization();

	TempForwarding();
	RemoveUnusedInstructions();

	DisassembleDebug("Peephole optimized");

	FastNumberSet	activeSet(numTemps);

	//
	// And remove unused temporaries
	//
	for (i = 0; i < numFixedTemporaries; i++)
		activeSet += i;

	ResetVisited();
	mEntryBlock->CollectActiveTemporaries(activeSet);


	mTemporaries.SetSize(activeSet.Num(), true);



	ResetVisited();
	mEntryBlock->ShrinkActiveTemporaries(activeSet, mTemporaries);

	MapVariables();

	DisassembleDebug("mapped variabled");

	ReduceTemporaries();

	DisassembleDebug("Reduced Temporaries");
}

void InterCodeProcedure::AddCalledFunction(InterCodeProcedure* proc)
{
	mCalledFunctions.Push(proc);
}

void InterCodeProcedure::CallsFunctionPointer(void)
{
	mCallsFunctionPointer = true;
}

void InterCodeProcedure::MapVariables(void)
{
	ResetVisited();
	mEntryBlock->MapVariables(mModule->mGlobalVars, mLocalVars);
	mLocalSize = 0;
	for (int i = 0; i < mLocalVars.Size(); i++)
	{
		if (mLocalVars[i] && mLocalVars[i]->mUsed)
		{
			mLocalVars[i]->mOffset = mLocalSize;
			mLocalSize += mLocalVars[i]->mSize;
		}
	}
}

void InterCodeProcedure::BuildDominators(void)
{
	ResetVisited();
	mEntryBlock = mEntryBlock->PropagateDominator(this);

	ResetVisited();
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mNumEntries = 0;
	mEntryBlock->CollectEntries();
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
	mEntryBlock->BuildLocalTempSets(numTemps, numFixedTemporaries);

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

	for (i = 0; i < numFixedTemporaries; i++)
		mRenameTable[i] = i;

	numRenamedTemps = numFixedTemporaries;

	NumberSet	usedTemps(numTemps);

	for (i = numFixedTemporaries; i < numTemps; i++)
	{
		usedTemps.Clear();

		for (j = numFixedTemporaries; j < numTemps; j++)
		{
			if (mRenameTable[j] >= 0 && (collisionSet[i][j] || InterTypeSize[mTemporaries[j]] != InterTypeSize[mTemporaries[i]]))
			{
				usedTemps += mRenameTable[j];
			}
		}

		j = numFixedTemporaries;
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
	mEntryBlock->BuildLocalTempSets(numRenamedTemps, numFixedTemporaries);

	ResetVisited();
	mEntryBlock->BuildGlobalProvidedTempSet(NumberSet(numRenamedTemps));

	NumberSet	totalRequired3(numRenamedTemps);

	do {
		ResetVisited();
	} while (mEntryBlock->BuildGlobalRequiredTempSet(totalRequired3));


	NumberSet	callerSaved(numRenamedTemps);
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

	mTempOffset.SetSize(0);

	for (int i = 0; i < mTemporaries.Size(); i++)
	{
		int size = InterTypeSize[mTemporaries[i]];

		if (freeTemps + size <= freeCallerSavedTemps && !callerSaved[i])
		{
			mTempOffset.Push(freeTemps);
			mTempSizes.Push(size);
			freeTemps += size;
		}
		else if (callerSavedTemps + size <= 16)
		{
			mTempOffset.Push(callerSavedTemps);
			mTempSizes.Push(size);
			callerSavedTemps += size;
		}
		else
		{
			mTempOffset.Push(calleeSavedTemps);
			mTempSizes.Push(size);
			calleeSavedTemps += size;
		}
	}
	mTempSize = calleeSavedTemps;
	mCallerSavedTemps = callerSavedTemps;
}

void InterCodeProcedure::Disassemble(const char* name, bool dumpSets)
{
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
}

InterCodeModule::InterCodeModule(void)
	: mGlobalVars(nullptr), mProcedures(nullptr)
{
}

InterCodeModule::~InterCodeModule(void)
{

}

#include "NativeCodeGenerator.h"

NativeCodeInstruction::NativeCodeInstruction(AsmInsType type, AsmInsMode mode, int address, int varIndex, bool lower, bool upper)
	: mType(type), mMode(mode), mAddress(address), mVarIndex(varIndex), mLower(lower), mUpper(upper), mRuntime(nullptr)
{}

NativeCodeInstruction::NativeCodeInstruction(const char* runtime)
	: mType(ASMIT_JSR), mMode(ASMIM_ABSOLUTE), mAddress(0), mVarIndex(0), mLower(true), mUpper(true), mRuntime(runtime)
{}


void NativeCodeInstruction::Assemble(NativeCodeBasicBlock* block)
{
	block->PutByte(AsmInsOpcodes[mType][mMode]);

	switch (mMode)
	{
	case ASMIM_IMPLIED:
		break;
	case ASMIM_ZERO_PAGE:
	case ASMIM_ZERO_PAGE_X:
	case ASMIM_ZERO_PAGE_Y:
	case ASMIM_INDIRECT_X:
	case ASMIM_INDIRECT_Y:
		block->PutByte(uint8(mAddress));
		break;
	case ASMIM_IMMEDIATE:
		if (mVarIndex != -1)
		{
			ByteCodeRelocation		rl;
			rl.mAddr = block->mCode.Size();
			rl.mFunction = false;
			rl.mLower = mLower;
			rl.mUpper = mUpper;
			rl.mIndex = mVarIndex;
			rl.mOffset = mAddress;
			rl.mRuntime = nullptr;
			block->mRelocations.Push(rl);
			block->PutByte(0);
		}
		else
		{
			block->PutByte(uint16(mAddress));
		}
		break;
	case ASMIM_ABSOLUTE:
	case ASMIM_INDIRECT:
	case ASMIM_ABSOLUTE_X:
	case ASMIM_ABSOLUTE_Y:
		if (mRuntime)
		{
			ByteCodeRelocation		rl;
			rl.mAddr = block->mCode.Size();
			rl.mFunction = false;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = -1;
			rl.mOffset = 0;
			rl.mRuntime = mRuntime;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else if (mVarIndex != - 1)
		{
			ByteCodeRelocation		rl;
			rl.mAddr = block->mCode.Size();
			rl.mFunction = false;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = mVarIndex;
			rl.mOffset = mAddress;
			rl.mRuntime = nullptr;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else
		{
			block->PutWord(uint16(mAddress));
		}
		break;
	case ASMIM_RELATIVE:
		block->PutByte(uint8(mAddress));
		break;
	}
}


void NativeCodeBasicBlock::PutByte(uint8 code)
{
	this->mCode.Insert(code);
}

void NativeCodeBasicBlock::PutWord(uint16 code)
{
	this->mCode.Insert((uint8)(code & 0xff));
	this->mCode.Insert((uint8)(code >> 8));
}

static AsmInsType InvertBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case ASMIT_BEQ: return ASMIT_BNE;
	case ASMIT_BNE: return ASMIT_BEQ;
	case ASMIT_BPL: return ASMIT_BMI;
	case ASMIT_BMI: return ASMIT_BPL;
	case ASMIT_BCS: return ASMIT_BCC;
	case ASMIT_BCC: return ASMIT_BCS;
	default:
		return code;
	}
}

static AsmInsType TransposeBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case ASMIT_BEQ: return ASMIT_BEQ;
	case ASMIT_BNE: return ASMIT_BNE;
	case ASMIT_BPL: return ASMIT_BMI;
	case ASMIT_BMI: return ASMIT_BPL;
	case ASMIT_BCS: return ASMIT_BCC;
	case ASMIT_BCC: return ASMIT_BCS;
	default:
		return code;
	}
}


int NativeCodeBasicBlock::PutJump(NativeCodeProcedure* proc, int offset)
{
	PutByte(0x4c);

	ByteCodeRelocation		rl;
	rl.mAddr = mCode.Size();
	rl.mFunction = true;
	rl.mLower = true;
	rl.mUpper = true;
	rl.mIndex = proc->mIndex;
	rl.mOffset = mOffset + mCode.Size() + offset - 1;
	rl.mRuntime = nullptr;
	mRelocations.Push(rl);

	PutWord(0);
	return 3;
}

int NativeCodeBasicBlock::PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset)
{
	if (offset >= -126 && offset <= 129)
	{
		PutByte(AsmInsOpcodes[code][ASMIM_RELATIVE]);
		PutByte(offset - 2);
		return 2;
	}
	else
	{
		PutByte(AsmInsOpcodes[InvertBranchCondition(code)][ASMIM_RELATIVE]);
		PutByte(3);
		PutByte(0x4c);

		ByteCodeRelocation	rl;
		rl.mAddr = mCode.Size();
		rl.mFunction = true;
		rl.mLower = true;
		rl.mUpper = true;
		rl.mIndex = proc->mIndex;
		rl.mOffset = mOffset + mCode.Size() + offset - 3;
		rl.mRuntime = nullptr;
		mRelocations.Push(rl);
		
		PutWord(0);
		return 5;
	}
}

void NativeCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.mTType == IT_FLOAT)
	{
	}
	else if (ins.mTType == IT_POINTER)
	{
		if (ins.mMemory == IM_GLOBAL)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue, ins.mVarIndex, true, false));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue, ins.mVarIndex, false, true));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_ABSOLUTE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mIntValue >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
		{
			int	index = ins.mIntValue;
			if (ins.mMemory == IM_LOCAL)
				index += proc->mLocalVars[ins.mVarIndex].mOffset;
			else
				index += ins.mVarIndex + proc->mLocalSize + 2;

			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, (mNoFrame ? BC_REG_STACK : BC_REG_LOCALS) + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
		}
		else if (ins.mMemory == IM_PROCEDURE)
		{
		}
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mIntValue & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mIntValue >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
	}

}

void NativeCodeBasicBlock::StoreValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.mSType[0] == IT_FLOAT)
	{
	}
	else if (ins.mSType[0] == IT_POINTER)
	{
		if (ins.mSTemp[1] < 0)
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[1];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (ins.mMemory == IM_FRAME)
				{
				}
			}
			else
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[1];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (ins.mMemory == IM_FRAME)
				{
				}
			}
		}
		else
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				}
			}
			else
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				}
			}
		}
	}
	else
	{
		if (ins.mSTemp[1] < 0)
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mOperandSize == 1)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
				else if (ins.mOperandSize == 2)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
			}
			else
			{
				if (ins.mOperandSize == 1)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
				else if (ins.mOperandSize == 2)
				{
					if (ins.mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1], ins.mVarIndex));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1, ins.mVarIndex));
					}
					else if (ins.mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins.mSIntConst[1] + 1));
					}
					else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
					{
						int	index = ins.mSIntConst[1];
						if (ins.mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins.mVarIndex].mOffset;
						else
							index += ins.mVarIndex + proc->mLocalSize + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					}
					else if (ins.mMemory == IM_FRAME)
					{
					}
				}
			}
		}
		else
		{
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));

					if (ins.mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					}
				}
			}
			else
			{
				if (ins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));

					if (ins.mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					}
				}
			}
		}
	}

}

void NativeCodeBasicBlock::LoadStoreValue(InterCodeProcedure* proc, const InterInstruction& rins, const InterInstruction& wins)
{
	if (rins.mTType == IT_FLOAT)
	{

	}
	else if (rins.mTType == IT_POINTER)
	{

	}
	else
	{
		if (wins.mOperandSize == 1)
		{
			if (rins.mSTemp[0] < 0)
			{
				if (rins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins.mSIntConst[0], rins.mVarIndex));
				}
				else if (rins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins.mSIntConst[0]));
				}
				else if (rins.mMemory == IM_LOCAL || rins.mMemory == IM_PARAM)
				{
					int	index = rins.mSIntConst[0];
					if (rins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[rins.mVarIndex].mOffset;
					else
						index += rins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
			}
			else
			{
				if (rins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins.mSTemp[0]]));
				}
			}

			if (wins.mSTemp[1] < 0)
			{
				if (wins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins.mSIntConst[1], wins.mVarIndex));
				}
				else if (wins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins.mSIntConst[1]));
				}
				else if (wins.mMemory == IM_LOCAL || wins.mMemory == IM_PARAM)
				{
					int	index = wins.mSIntConst[1];
					if (wins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[wins.mVarIndex].mOffset;
					else
						index += wins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}
				else if (wins.mMemory == IM_FRAME)
				{
				}
			}
			else
			{
				if (wins.mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins.mSTemp[1]]));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValueToReg(InterCodeProcedure* proc, const InterInstruction& ins, int reg, const NativeCodeInstruction* ainsl, const NativeCodeInstruction* ainsh)
{
	if (ins.mTType == IT_FLOAT)
	{
	}
	else if (ins.mTType == IT_POINTER)
	{
		if (ins.mSTemp[0] < 0)
		{
			if (ins.mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
				if (ainsl) mIns.Push(*ainsl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1, ins.mVarIndex));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins.mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0]));
				if (ainsl) mIns.Push(*ainsl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
			{
				int	index = ins.mSIntConst[0];
				if (ins.mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins.mVarIndex].mOffset;
				else
					index += ins.mVarIndex + proc->mLocalSize + 2;

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				if (ainsl) mIns.Push(*ainsl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
		}
		else
		{
			if (ins.mMemory == IM_INDIRECT)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
				if (ainsl) mIns.Push(*ainsl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
		}
	}
	else
	{
		if (ins.mSTemp[0] < 0)
		{
			if (ins.mOperandSize == 1)
			{
				if (ins.mTType == IT_SIGNED)
					mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, 0));

				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0]));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[0];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
				}

				if (ainsl) mIns.Push(*ainsl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				if (ins.mTType == IT_SIGNED)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, 1));
					mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				}
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins.mOperandSize == 2)
			{
				if (ins.mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0], ins.mVarIndex));
					if (ainsl) mIns.Push(*ainsl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1, ins.mVarIndex));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins.mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0]));
					if (ainsl) mIns.Push(*ainsl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins.mSIntConst[0] + 1));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins.mMemory == IM_LOCAL || ins.mMemory == IM_PARAM)
				{
					int	index = ins.mSIntConst[0];
					if (ins.mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins.mVarIndex].mOffset;
					else
						index += ins.mVarIndex + proc->mLocalSize + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					if (ainsl) mIns.Push(*ainsl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, mNoFrame ? BC_REG_STACK : BC_REG_LOCALS));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
		}
		else
		{
			if (ins.mMemory == IM_INDIRECT)
			{
				if (ins.mOperandSize == 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					if (ainsl) mIns.Push(*ainsl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					if (ins.mTType == IT_SIGNED)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, 1));
						mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					}
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins.mOperandSize == 2)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					if (ainsl) mIns.Push(*ainsl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	LoadValueToReg(proc, ins, BC_REG_TMP + proc->mTempOffset[ins.mTTemp], nullptr, nullptr);
}

void NativeCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, const InterInstruction& ins, const InterInstruction * sins1, const InterInstruction * sins0)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins.mTTemp];

	switch (ins.mOperator)
	{
	case IA_ADD:
	case IA_OR:
	case IA_AND:
	case IA_XOR:
	{
		if (ins.mOperator == IA_ADD && (
				ins.mSTemp[0] < 0 && ins.mSIntConst[0] == 1 && !sins1 && ins.mSTemp[1] == ins.mTTemp ||
				ins.mSTemp[1] < 0 && ins.mSIntConst[1] == 1 && !sins0 && ins.mSTemp[0] == ins.mTTemp))
		{
			mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, 2));
			mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg + 1));
		}
		else
		{
			NativeCodeInstruction	insl, insh;

			AsmInsType	atype;
			switch (ins.mOperator)
			{
			case IA_ADD:
				atype = ASMIT_ADC;
				mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
				break;
			case IA_OR:
				atype = ASMIT_ORA;
				break;
			case IA_AND:
				atype = ASMIT_AND;
				break;
			case IA_XOR:
				atype = ASMIT_EOR;
				break;
			}

			if (ins.mSTemp[1] < 0)
			{
				insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff);
				insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff);
				if (sins0)
					LoadValueToReg(proc, *sins0, treg, &insl, &insh);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(insh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else if (ins.mSTemp[0] < 0)
			{
				insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff);
				insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff);
				if (sins1)
					LoadValueToReg(proc, *sins1, treg, &insl, &insh);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(insh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else
			{
				if (sins1 && sins0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, treg);
					insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, treg + 1);

					LoadValueToReg(proc, *sins1, treg, nullptr, nullptr);
					LoadValueToReg(proc, *sins0, treg, &insl, &insh);
				}
				else if (sins1)
				{
					insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]);
					insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1);

					LoadValueToReg(proc, *sins1, treg, &insl, &insh);
				}
				else if (sins0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]);
					insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1);

					LoadValueToReg(proc, *sins0, treg, &insl, &insh);
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
		}
	} break;
	case IA_SUB:
	{
		NativeCodeInstruction	insl, insh;

		mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		if (ins.mSTemp[0] < 0)
		{
			insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff);
			insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff);
			if (sins1)
				LoadValueToReg(proc, *sins1, treg, &insl, &insh);
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				mIns.Push(insl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
				mIns.Push(insh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		}
		else if (ins.mSTemp[1] < 0)
		{
			if (sins0)
			{
				insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg);
				insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg + 1);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));

				LoadValueToReg(proc, *sins0, treg, &insl, &insh);
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		}
		else
		{
			if (sins0)
			{
				insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg);
				insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg + 1);

				LoadValueToReg(proc, *sins0, treg, nullptr, nullptr);
			}
			else
			{
				insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]);
				insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1);
			}

			if (sins1)
			{
				LoadValueToReg(proc, *sins1, treg, &insl, &insh);
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				mIns.Push(insl);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
				mIns.Push(insh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		}
	} break;
	case IA_MUL:
	case IA_DIVS:
	case IA_MODS:
	case IA_DIVU:
	case IA_MODU:
	{
		if (sins1)
			LoadValueToReg(proc, *sins1, BC_REG_ACCU, nullptr, nullptr);
		else if (ins.mSTemp[1] < 0)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}

		if (sins0)
			LoadValueToReg(proc, *sins0, BC_REG_WORK, nullptr, nullptr);
		else if (ins.mSTemp[0] < 0)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
		}

		int	reg = BC_REG_ACCU;
		switch (ins.mOperator)
		{
		case IA_MUL:
			mIns.Push(NativeCodeInstruction("mul16"));
			reg = BC_REG_WORK + 2;
			break;
		case IA_DIVS:
			mIns.Push(NativeCodeInstruction("divs16"));
			break;
		case IA_MODS:
			mIns.Push(NativeCodeInstruction("mods16"));
			reg = BC_REG_WORK + 2;
			break;
		case IA_DIVU:
			mIns.Push(NativeCodeInstruction("divu16"));
			break;
		case IA_MODU:
			mIns.Push(NativeCodeInstruction("modu16"));
			reg = BC_REG_WORK + 2;
			break;
		}

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
	} break;
	case IA_SHL:
	{
		if (ins.mSTemp[0] < 0)
		{
			int	shift = ins.mSIntConst[0] & 15;
			if (shift == 0)
			{
				if (ins.mSTemp[1] != ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else if (shift == 1)
			{
				if (ins.mSTemp[1] != ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else
			{
				if (ins.mSTemp[1] != ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
				}
				mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
				for (int i = 1; i < shift; i++)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		}
	} break;
	case IA_SAR:
	{
		if (sins1) LoadValueToReg(proc, *sins1, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]], nullptr, nullptr);
		if (sins0) LoadValueToReg(proc, *sins0, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]], nullptr, nullptr);

		if (ins.mSTemp[0] < 0)
		{
			int	shift = ins.mSIntConst[0] & 15;
			if (shift == 0)
			{
				if (ins.mSTemp[1] != ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else
			{
				if (ins.mSTemp[1] != ins.mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
				}

				for (int i = 0; i < shift; i++)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
					mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		}
		else if (ins.mSTemp[1] < 0 && ins.mSIntConst[1] < 0x100)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
			mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));

			mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
			mIns.Push(NativeCodeInstruction(ASMIT_BEQ, ASMIM_RELATIVE, 4));

			mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -4));

			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, treg + 1));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
			mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

			if (ins.mSTemp[1] < 0)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
			}
			else if (ins.mSTemp[1] != ins.mTTemp)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
			}

			mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
			mIns.Push(NativeCodeInstruction(ASMIT_BEQ, ASMIM_RELATIVE, 2 + 1 + 2 + 1 + 2));

			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
			mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -(2 + 1 + 2 + 1 + 2)));

			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
		}
	} break;

	}
}

void NativeCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins.mTTemp];

	if (ins.mTType == IT_FLOAT)
	{
	}
	else
	{
		switch (ins.mOperator)
		{
		case IA_NEG:
			mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			break;

		case IA_NOT:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			break;
		}
	}
}

void NativeCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction& ins, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump)
{
	InterOperator	op = ins.mOperator;

	NativeCodeBasicBlock* eblock = new NativeCodeBasicBlock();
	eblock->mNoFrame = mNoFrame;
	eblock->mIndex = 1000;

	NativeCodeBasicBlock* nblock = new NativeCodeBasicBlock();
	nblock->mNoFrame = mNoFrame;
	nblock->mIndex = 1000;

	int	li = 1, ri = 0;
	if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
	{
		li = 0; ri = 1;
	}

	if (op >= IA_CMPGES && ins.mOperator <= IA_CMPLS)
	{
		if (ins.mSTemp[ri] >= 0)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[ri]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK));
		}

		if (ins.mSTemp[li] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ((ins.mSIntConst[li] >> 8) & 0xff) ^ 0x80));
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[li]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
		}

		if (ins.mSTemp[ri] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ((ins.mSIntConst[ri] >> 8) & 0xff) ^ 0x80));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_WORK));
	}
	else
	{
		if (ins.mSTemp[li] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[li] >> 8) & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[li]] + 1));
		if (ins.mSTemp[ri] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins.mSIntConst[ri] >> 8) & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[ri]] + 1));
	}

	this->Close(eblock, nblock, ASMIT_BEQ);

	if (ins.mSTemp[li] < 0)
		eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[li] & 0xff));
	else
		eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[li]]));
	if (ins.mSTemp[ri] < 0)
		eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ins.mSIntConst[ri] & 0xff));
	else
		eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[ri]]));

	switch (op)
	{
	case IA_CMPEQ:
		nblock->Close(falseJump, nullptr, ASMIT_JMP);
		eblock->Close(trueJump, falseJump, ASMIT_BEQ);
		break;
	case IA_CMPNE:
		nblock->Close(trueJump, nullptr, ASMIT_JMP);
		eblock->Close(falseJump, trueJump, ASMIT_BEQ);
		break;
	case IA_CMPLU:
	case IA_CMPLS:
	case IA_CMPGU:
	case IA_CMPGS:
		eblock->Close(nblock, nullptr, ASMIT_JMP);
		nblock->Close(trueJump, falseJump, ASMIT_BCC);
		break;
	case IA_CMPLEU:
	case IA_CMPLES:
	case IA_CMPGEU:
	case IA_CMPGES:
		eblock->Close(nblock, nullptr, ASMIT_JMP);
		nblock->Close(falseJump, trueJump, ASMIT_BCC);
		break;

	}
}

void NativeCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction& ins)
{
	mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
	if (ins.mSTemp[1] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins.mSIntConst[1] & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]]));
	if (ins.mSTemp[0] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ins.mSIntConst[0] & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]]));
	mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp]));
	if (ins.mSTemp[1] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins.mSIntConst[1] >> 8) & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[1]] + 1));
	if (ins.mSTemp[0] < 0)
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins.mSIntConst[0] >> 8) & 0xff));
	else
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mSTemp[0]] + 1));
	mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins.mTTemp] + 1));
}

void NativeCodeBasicBlock::Assemble(void)
{
	if (!mAssembled)
	{
		mAssembled = true;

		//PeepHoleOptimizer();

		for (int i = 0; i < mIns.Size(); i++)
			mIns[i].Assemble(this);

		if (this->mTrueJump)
			this->mTrueJump->Assemble();
		if (this->mFalseJump)
			this->mFalseJump->Assemble();
	}
}

void NativeCodeBasicBlock::Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch)
{
	this->mTrueJump = trueJump;
	this->mFalseJump = falseJump;
	this->mBranch = branch;
}


static int BranchByteSize(int from, int to)
{
	if (to - from >= -126 && to - from <= 129)
		return 2;
	else
		return 5;
}

static int JumpByteSize(int from, int to)
{
	return 3;
}

NativeCodeBasicBlock* NativeCodeBasicBlock::BypassEmptyBlocks(void)
{
	if (mBypassed)
		return this;
	else if (!mFalseJump && mCode.Size() == 0)
		return mTrueJump->BypassEmptyBlocks();
	else
	{
		mBypassed = true;

		if (mFalseJump)
			mFalseJump = mFalseJump->BypassEmptyBlocks();
		if (mTrueJump)
			mTrueJump = mTrueJump->BypassEmptyBlocks();

		return this;
	}
}

void NativeCodeBasicBlock::CopyCode(NativeCodeProcedure * proc, uint8* target)
{
	int i;
	int next;
	int pos, at;
	uint8 b;

	if (!mCopied)
	{
		mCopied = true;


		next = mOffset + mCode.Size();

		if (mFalseJump)
		{
			if (mFalseJump->mOffset <= mOffset)
			{
				if (mTrueJump->mOffset <= mOffset)
				{
					next += PutBranch(proc, mBranch, mTrueJump->mOffset - next);
					next += PutJump(proc, mFalseJump->mOffset - next);

				}
				else
				{
					next += PutBranch(proc, InvertBranchCondition(mBranch), mFalseJump->mOffset - next);
				}
			}
			else
			{
				next += PutBranch(proc, mBranch, mTrueJump->mOffset - next);
			}
		}
		else if (mTrueJump)
		{
			if (mTrueJump->mOffset != next)
			{
				next += PutJump(proc, mTrueJump->mOffset - next);
			}
		}

		assert(next - mOffset == mSize);

		for (i = 0; i < mCode.Size(); i++)
		{
			mCode.Lookup(i, target[i + mOffset]);
		}

		for (int i = 0; i < mRelocations.Size(); i++)
		{
			ByteCodeRelocation& rl(mRelocations[i]);
			rl.mAddr += mOffset;
			proc->mRelocations.Push(rl);
		}

		if (mTrueJump) mTrueJump->CopyCode(proc, target);
		if (mFalseJump) mFalseJump->CopyCode(proc, target);
	}
}

void NativeCodeBasicBlock::CalculateOffset(int& total)
{
	int next;

	if (mOffset > total)
	{
		mOffset = total;
		next = total + mCode.Size();

		if (mFalseJump)
		{
			if (mFalseJump->mOffset <= total)
			{
				// falseJump has been placed

				if (mTrueJump->mOffset <= total)
				{
					// trueJump and falseJump have been placed, not much to do

					next = next + BranchByteSize(next, mTrueJump->mOffset);
					total = next + JumpByteSize(next, mFalseJump->mOffset);
					mSize = total - mOffset;
				}
				else
				{
					// trueJump has not been placed, but falseJump has

					total = next + BranchByteSize(next, mFalseJump->mOffset);
					mSize = total - mOffset;
					mTrueJump->CalculateOffset(total);
				}
			}
			else if (mTrueJump->mOffset <= total)
			{
				// falseJump has not been placed, but trueJump has

				total = next + BranchByteSize(next, mTrueJump->mOffset);
				mSize = total - mOffset;
				mFalseJump->CalculateOffset(total);
			}
			else if (mKnownShortBranch)
			{
				// neither falseJump nor trueJump have been placed,
				// but we know from previous iteration that we can do
				// a short branch

				total = next + 2;
				mSize = total - mOffset;

				mFalseJump->CalculateOffset(total);
				if (mTrueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now
					mTrueJump->CalculateOffset(total);
				}
			}
			else
			{
				// neither falseJump nor trueJump have been placed
				// this may lead to some undo operation...
				// first assume a full size branch:

				total = next + 5;
				mSize = total - mOffset;

				mFalseJump->CalculateOffset(total);
				if (mTrueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now

					mTrueJump->CalculateOffset(total);
				}

				if (BranchByteSize(next, mTrueJump->mOffset) < 3)
				{
					// oh, we can replace by a short branch

					mKnownShortBranch = true;

					total = next + 2;
					mSize = total - mOffset;

					mFalseJump->CalculateOffset(total);
					if (mTrueJump->mOffset > total)
					{
						// trueJump was not placed in the process, so lets place it now

						mTrueJump->CalculateOffset(total);
					}
				}
			}
		}
		else if (mTrueJump)
		{
			if (mTrueJump->mOffset <= total)
			{
				// trueJump has been placed, so append the branch size

				total = next + JumpByteSize(next, mTrueJump->mOffset);
				mSize = total - mOffset;
			}
			else
			{
				// we have to place trueJump, so just put it right behind us

				total = next;
				mSize = total - mOffset;

				mTrueJump->CalculateOffset(total);
			}
		}
		else
		{
			// no exit from block

			total += mCode.Size();
			mSize = total - mOffset;
		}
	}
}

NativeCodeBasicBlock::NativeCodeBasicBlock(void)
	: mIns(NativeCodeInstruction(ASMIT_INV, ASMIM_IMPLIED)), mRelocations({ 0 })
{
	mTrueJump = mFalseJump = NULL;
	mOffset = 0x7fffffff;
	mCopied = false;
	mKnownShortBranch = false;
	mBypassed = false;
}

NativeCodeBasicBlock::~NativeCodeBasicBlock(void)
{

}

NativeCodeProcedure::NativeCodeProcedure(void)
	: mRelocations({ 0 })
{
	mTempBlocks = 1000;
}

NativeCodeProcedure::~NativeCodeProcedure(void)
{

}

void NativeCodeProcedure::Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc)
{
	int	nblocks = proc->mBlocks.Size();
	tblocks = new NativeCodeBasicBlock * [nblocks];
	for (int i = 0; i < nblocks; i++)
		tblocks[i] = nullptr;

	mIndex = proc->mID;

	int		tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;
	int		stackExpand = tempSave + proc->mLocalSize + 2;

	mNoFrame = proc->mLocalSize == 0 && tempSave == 0;

	entryBlock = new NativeCodeBasicBlock();
	entryBlock->mNoFrame = mNoFrame;
	entryBlock->mIndex = 0;

	if (!mNoFrame)
	{
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, stackExpand & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (stackExpand >> 8) & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));

		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, tempSave + 2));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));

		if (tempSave)
		{
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED - 1));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, - 6));
		}

	}

	tblocks[0] = entryBlock;

	exitBlock = new NativeCodeBasicBlock();
	exitBlock->mNoFrame = mNoFrame;

	if (!mNoFrame)
	{
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));

		if (tempSave)
		{
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));

			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -6));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
		}

		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, stackExpand & 0xff));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (stackExpand >> 8) & 0xff));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));

	}

	exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_RTS, ASMIM_IMPLIED));

	CompileInterBlock(proc, proc->mBlocks[0], entryBlock);
	entryBlock->Assemble();

	int	total, base;

	NativeCodeBasicBlock* lentryBlock = entryBlock->BypassEmptyBlocks();

	total = 0;
	base = generator->mProgEnd;

	lentryBlock->CalculateOffset(total);	

	generator->AddAddress(proc->mID, true, base, total, proc->mIdent, true);

	lentryBlock->CopyCode(this, generator->mMemory + base);
	
	generator->mProgEnd += total;

	for (int i = 0; i < mRelocations.Size(); i++)
	{
		ByteCodeRelocation& rl(mRelocations[i]);
		rl.mAddr += base;
		generator->mRelocations.Push(rl);
	}
}

NativeCodeBasicBlock* NativeCodeProcedure::AllocateBlock(void)
{
	NativeCodeBasicBlock* block = new NativeCodeBasicBlock();
	block->mNoFrame = mNoFrame;
	block->mIndex = mTempBlocks++;

	return block;
}

NativeCodeBasicBlock* NativeCodeProcedure::CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* sblock)
{
	if (tblocks[sblock->mIndex])
		return tblocks[sblock->mIndex];

	NativeCodeBasicBlock* block = new NativeCodeBasicBlock();
	block->mNoFrame = mNoFrame;

	tblocks[sblock->mIndex] = block;
	block->mIndex = sblock->mIndex;

	CompileInterBlock(iproc, sblock, block);

	return block;
}

void NativeCodeProcedure::CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock* block)
{
	int	i = 0;
	while (i < iblock->mInstructions.Size())
	{
		const InterInstruction& ins = iblock->mInstructions[i];

		switch (ins.mCode)
		{
		case IC_STORE:
			block->StoreValue(iproc, ins);
			break;
		case IC_LOAD:
			if (i + 1 < iblock->mInstructions.Size() &&
				iblock->mInstructions[i + 1].mCode == IC_STORE &&
				iblock->mInstructions[i + 1].mSTemp[0] == ins.mTTemp &&
				iblock->mInstructions[i + 1].mSFinal[0] &&
				iblock->mInstructions[i + 1].mOperandSize == 1)
			{
				block->LoadStoreValue(iproc, ins, iblock->mInstructions[i + 1]);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				ins.mOperandSize == 2 &&
				iblock->mInstructions[i + 1].mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1].mSTemp[0] == ins.mTTemp && iblock->mInstructions[i + 1].mSFinal[0])
			{
				block->BinaryOperator(iproc, iblock->mInstructions[i + 1], nullptr, &ins);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				ins.mOperandSize == 2 &&
				iblock->mInstructions[i + 1].mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1].mSTemp[1] == ins.mTTemp && iblock->mInstructions[i + 1].mSFinal[1])
			{
				block->BinaryOperator(iproc, iblock->mInstructions[i + 1], &ins, nullptr);
				i++;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				ins.mOperandSize == 2 &&
				iblock->mInstructions[i + 1].mCode == IC_LOAD && iblock->mInstructions[i + 1].mOperandSize == 2 &&
				iblock->mInstructions[i + 2].mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2].mSTemp[0] == iblock->mInstructions[i + 1].mTTemp && iblock->mInstructions[i + 2].mSFinal[0] &&
				iblock->mInstructions[i + 2].mSTemp[1] == ins.mTTemp && iblock->mInstructions[i + 2].mSFinal[1])
			{
				block->BinaryOperator(iproc, iblock->mInstructions[i + 2], &ins, &(iblock->mInstructions[i + 1]));
				i += 2;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				ins.mOperandSize == 2 &&
				iblock->mInstructions[i + 1].mCode == IC_LOAD && iblock->mInstructions[i + 1].mOperandSize == 2 &&
				iblock->mInstructions[i + 2].mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2].mSTemp[1] == iblock->mInstructions[i + 1].mTTemp && iblock->mInstructions[i + 2].mSFinal[1] &&
				iblock->mInstructions[i + 2].mSTemp[0] == ins.mTTemp && iblock->mInstructions[i + 2].mSFinal[0])
			{
				block->BinaryOperator(iproc, iblock->mInstructions[i + 2], &(iblock->mInstructions[i + 1]), &ins);
				i += 2;
			}
			else
				block->LoadValue(iproc, ins);
			break;
		case IC_COPY:
			//		CopyValue(iproc, ins);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins.mSTemp[0] != ins.mTTemp)
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp] + 1));
			}
		}	break;
		case IC_BINARY_OPERATOR:
			block->BinaryOperator(iproc, ins, nullptr, nullptr);
			break;
		case IC_UNARY_OPERATOR:
			block->UnaryOperator(iproc, ins);
			break;
		case IC_CONVERSION_OPERATOR:
			//			NumericConversion(iproc, ins);
			break;
		case IC_LEA:
			block->LoadEffectiveAddress(iproc, ins);
			break;
		case IC_CONSTANT:
			block->LoadConstant(iproc, ins);
			break;
		case IC_CALL:
			//			CallFunction(iproc, ins);
			break;
		case IC_JSR:
			//			CallAssembler(iproc, ins);
			break;
		case IC_PUSH_FRAME:
		{
		}	break;
		case IC_POP_FRAME:
		{
		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (iblock->mInstructions[i + 1].mCode == IC_BRANCH)
			{
				block->RelationalOperator(iproc, ins, CompileBlock(iproc, iblock->mTrueJump), CompileBlock(iproc, iblock->mFalseJump));
				return;
			}
			else
			{
				NativeCodeBasicBlock* tblock, * fblock, * rblock;

				tblock = AllocateBlock();
				fblock = AllocateBlock();
				rblock = AllocateBlock();

				block->RelationalOperator(iproc, ins, tblock, fblock);

				tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 1));
				tblock->Close(rblock, nullptr, ASMIT_JMP);
				fblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				fblock->Close(rblock, nullptr, ASMIT_JMP);

				rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp]));
				rblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp] + 1));

				block = rblock;
			}
			break;

		case IC_RETURN_VALUE:
		{
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

			block->Close(exitBlock, nullptr, ASMIT_JMP);
			return;
		}

		case IC_RETURN:
			block->Close(exitBlock, nullptr, ASMIT_JMP);
			return;

		case IC_TYPECAST:
			if (ins.mSTemp[0] != ins.mTTemp)
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mTTemp] + 1));
			}
			break;

		case IC_BRANCH:
			if (ins.mSTemp[0] < 0)
			{
				if (ins.mSIntConst[0] == 0)
					block->Close(CompileBlock(iproc, iblock->mFalseJump), nullptr, ASMIT_JMP);
				else
					block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
			}
			else
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins.mSTemp[0]] + 1));

				block->Close(CompileBlock(iproc, iblock->mTrueJump), CompileBlock(iproc, iblock->mFalseJump), ASMIT_BNE);
			}
			return;

		}

		i++;
	}

	block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
}


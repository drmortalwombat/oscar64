#include "NativeCodeGenerator.h"

NativeCodeInstruction::NativeCodeInstruction(AsmInsType type, AsmInsMode mode)
	: mType(type), mMode(mode), mGlobal(false), mAddress(0), mVarIndex(-1)
{}


void NativeCodeInstruction::Assemble(ByteCodeGenerator* generator, NativeCodeBasicBlock* block)
{
	switch (mMode)
	{
		block->PutByte(AsmInsOpcodes[mType][mMode]);

		switch (mMode)
		{
		case ASMIM_IMPLIED:
			break;
		case ASMIM_IMMEDIATE:
		case ASMIM_ZERO_PAGE:
		case ASMIM_ZERO_PAGE_X:
		case ASMIM_INDIRECT_X:
		case ASMIM_INDIRECT_Y:
			block->PutByte(uint8(mAddress));
			break;
		case ASMIM_ABSOLUTE:
		case ASMIM_INDIRECT:
		case ASMIM_ABSOLUTE_X:
		case ASMIM_ABSOLUTE_Y:
			if (mGlobal)
			{
				ByteCodeRelocation	rl;
				rl.mAddr = block->mCode.Size();
				rl.mFunction = false;
				rl.mLower = true;
				rl.mUpper = true;
				rl.mIndex = mVarIndex;
				rl.mOffset = mAddress;
				block->mRelocations.Push(rl);
				block->PutWord(0);
			}
			else
			{
				block->PutWord(uint16(mAddress));
			}
			break;
		case ASMIM_RELATIVE:
			block->PutByte(uint8(mAddress - block->mCode.Size() - 1));
			break;
		}
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
#if 0
static AsmInsType InvertBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case BC_BRANCHS_EQ: return BC_BRANCHS_NE;
	case BC_BRANCHS_NE: return BC_BRANCHS_EQ;
	case BC_BRANCHS_GT: return BC_BRANCHS_LE;
	case BC_BRANCHS_GE: return BC_BRANCHS_LT;
	case BC_BRANCHS_LT: return BC_BRANCHS_GE;
	case BC_BRANCHS_LE: return BC_BRANCHS_GT;
	default:
		return code;
	}
}

static AsmInsType TransposeBranchCondition(AsmInsType code)
{
	switch (code)
	{
	case BC_BRANCHS_EQ: return BC_BRANCHS_EQ;
	case BC_BRANCHS_NE: return BC_BRANCHS_NE;
	case BC_BRANCHS_GT: return BC_BRANCHS_LT;
	case BC_BRANCHS_GE: return BC_BRANCHS_LE;
	case BC_BRANCHS_LT: return BC_BRANCHS_GT;
	case BC_BRANCHS_LE: return BC_BRANCHS_GE;
	default:
		return code;
	}
}


int NativeCodeBasicBlock::PutBranch(ByteCodeGenerator* generator, AsmInsType code, int offset)
{
	if (offset >= -126 && offset <= 129)
	{
		PutByte(AsmInsOpcodes[code][ASMIM_RELATIVE]);
		PutByte(offset - 2);
		return 2;
	}
	else
	{
		PutByte(AsmInsOpcodes[code][ASMIM_RELATIVE]);

		PutCode(generator, ByteCode(code + (BC_JUMPF - BC_JUMPS)));
		PutWord(offset - 3);
		return 3;
	}
}
#endif

#include "ByteCodeGenerator.h"
#include "Assembler.h"

static ByteCode		StoreTypedTmpCodes[] = {
	BC_NOP,
	BC_STORE_REG_8,
	BC_STORE_REG_8,
	BC_STORE_REG_16,
	BC_STORE_REG_32,
	BC_STORE_REG_32,
	BC_STORE_REG_16
};


static ByteCode InvertBranchCondition(ByteCode code)
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

static ByteCode TransposeBranchCondition(ByteCode code)
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

ByteCodeInstruction::ByteCodeInstruction(ByteCode code)
	: mCode(code), mRelocate(false), mRegisterFinal(false), mLinkerObject(nullptr), mValue(0), mRegister(0)
{
}


bool ByteCodeInstruction::IsStore(void) const
{
	return
		mCode == BC_STORE_ABS_8 ||
		mCode == BC_STORE_ABS_16 ||
		mCode == BC_STORE_ABS_32 ||
		mCode == BC_STORE_LOCAL_8 ||
		mCode == BC_STORE_LOCAL_16 ||
		mCode == BC_STORE_LOCAL_32 ||
		mCode == BC_STORE_FRAME_8 ||
		mCode == BC_STORE_FRAME_16 ||
		mCode == BC_STORE_FRAME_32 ||
		mCode == BC_STORE_ADDR_8 ||
		mCode == BC_STORE_ADDR_16 ||
		mCode == BC_STORE_ADDR_32;
}

bool ByteCodeInstruction::ChangesAccu(void) const
{
	return ChangesRegister(BC_REG_ACCU);
}

bool ByteCodeInstruction::ChangesAddr(void) const
{
	return ChangesRegister(BC_REG_ADDR);
}

bool ByteCodeInstruction::LoadsRegister(uint32 reg) const
{
	if (mRegister == reg)
	{
		if (mCode >= BC_LOAD_ABS_8 && mCode <= BC_LOAD_ABS_32)
			return true;
		if (mCode >= BC_LOAD_LOCAL_8 && mCode <= BC_LOAD_LOCAL_32)
			return true;
		if (mCode >= BC_LOAD_ADDR_8 && mCode <= BC_LOAD_ADDR_32)
			return true;
		if (mCode >= BC_CONST_8 && mCode <= BC_CONST_32)
			return true;
		if (mCode == BC_LEA_ABS || mCode == BC_LEA_LOCAL || mCode == BC_LEA_FRAME)
			return true;
	}

	return false;
}

bool ByteCodeInstruction::IsCommutative(void) const
{
	if (mCode == BC_BINOP_ADDR_16 || mCode == BC_BINOP_ANDR_16 || mCode == BC_BINOP_ORR_16 || mCode == BC_BINOP_XORR_16 || mCode == BC_BINOP_MULR_16)
		return true;

	if (mCode == BC_BINOP_ADD_F32 || mCode == BC_BINOP_MUL_F32)
		return true;

	return false;
}

bool ByteCodeInstruction::StoresRegister(uint32 reg) const
{
	if (mRegister == reg)
	{
		if (mCode >= BC_STORE_ABS_8 && mCode <= BC_STORE_ABS_32)
			return true;
		if (mCode >= BC_STORE_LOCAL_8 && mCode <= BC_STORE_LOCAL_32)
			return true;
		if (mCode >= BC_STORE_FRAME_8 && mCode <= BC_STORE_FRAME_32)
			return true;
		if (mCode >= BC_STORE_ADDR_8 && mCode <= BC_STORE_ADDR_32)
			return true;
	}

	return false;

}

bool ByteCodeInstruction::ChangesRegister(uint32 reg) const
{
	if (mRegister == reg)
	{
		if (mCode == BC_STORE_REG_8 || mCode == BC_STORE_REG_16 || mCode == BC_STORE_REG_32)
			return true;
		if (mCode >= BC_LOAD_ABS_8 && mCode <= BC_LOAD_ABS_32)
			return true;
		if (mCode >= BC_LOAD_LOCAL_8 && mCode <= BC_LOAD_LOCAL_32)
			return true;
		if (mCode >= BC_LOAD_ADDR_8 && mCode <= BC_LOAD_ADDR_32)
			return true;
		if (mCode >= BC_CONST_8 && mCode <= BC_CONST_32)
			return true;
		if (mCode == BC_LEA_ABS || mCode == BC_LEA_LOCAL || mCode == BC_LEA_FRAME)
			return true;
		if (mCode >= BC_BINOP_ADDI_16 && mCode <= BC_BINOP_ORI_8)
			return true;
		if (mCode == BC_CALL)
			return true;
	}

	if (reg == BC_REG_ACCU)
	{
		if (mCode == BC_LOAD_REG_8 || mCode == BC_LOAD_REG_16 || mCode == BC_LOAD_REG_32)
			return true;
		if (mCode >= BC_BINOP_ADDR_16 && mCode <= BC_BINOP_SHRR_I16)
			return true;
		if (mCode >= BC_BINOP_CMPUR_16 && mCode <= BC_BINOP_CMPSI_16)
			return true;
		if (mCode >= BC_OP_NEGATE_16 && mCode <= BC_OP_INVERT_16)
			return true;
		if (mCode >= BC_BINOP_ADD_F32 && mCode <= BC_OP_CEIL_F32)
			return true;
		if (mCode >= BC_CONV_U16_F32 && mCode <= BC_CONV_F32_I16)
			return true;
		if (mCode >= BC_BINOP_SHLI_16 && mCode <= BC_BINOP_SHRI_U16)
			return true;
		if (mCode >= BC_SET_EQ && mCode <= BC_SET_LE)
			return true;
		if (mCode == BC_JSR || mCode == BC_CALL)
			return true;
	}

	if (reg == BC_REG_ADDR)
	{
		if (mCode == BC_ADDR_REG)
			return true;
		if (mCode >= BC_LOAD_ABS_8 && mCode <= BC_STORE_ABS_32)
			return true;
		if (mCode == BC_JSR || mCode == BC_CALL)
			return true;
	}

	return false;
}

void ByteCodeInstruction::Assemble(ByteCodeGenerator* generator, ByteCodeBasicBlock* block)
{
	switch (mCode)
	{
	case BC_NOP:
		break;
	case BC_EXIT:
		block->PutCode(generator, mCode);
		break;

	case BC_CONST_8:
		block->PutCode(generator, BC_CONST_8); block->PutByte(mRegister); block->PutByte(uint8(mValue));
		break;
	case BC_CONST_P8:
	case BC_CONST_N8:
	case BC_CONST_16:
		if (mRelocate)
		{
			block->PutCode(generator, mCode);
			block->PutByte(mRegister);

			LinkerReference	rl;
			rl.mOffset = block->mCode.Size();
			rl.mLowByte = true;
			rl.mHighByte = true;
			rl.mRefObject = mLinkerObject;
			rl.mRefOffset = mValue;
			block->mRelocations.Push(rl);

			block->PutWord(0);
		}
		else if (mValue >= 0 && mValue < 255)
		{
			block->PutCode(generator, BC_CONST_P8); block->PutByte(mRegister); block->PutByte(uint8(mValue));
		}
		else if (mValue < 0 && mValue >= -256)
		{
			block->PutCode(generator, BC_CONST_N8); block->PutByte(mRegister); block->PutByte(uint8(mValue));
		}
		else
		{
			block->PutCode(generator, BC_CONST_16); block->PutByte(mRegister); block->PutWord(uint16(mValue));
		}
		break;
	case BC_CONST_32:
		block->PutCode(generator, BC_CONST_32); block->PutByte(mRegister); block->PutDWord(uint32(mValue));
		break;

	case BC_LOAD_REG_8:
	case BC_STORE_REG_8:
	case BC_LOAD_REG_16:
	case BC_STORE_REG_16:
	case BC_LOAD_REG_32:
	case BC_STORE_REG_32:
	case BC_ADDR_REG:
		block->PutCode(generator, mCode); 
		block->PutByte(mRegister);
		break;

	case BC_LOAD_ABS_8:
	case BC_LOAD_ABS_U8:
	case BC_LOAD_ABS_I8:
	case BC_LOAD_ABS_16:
	case BC_LOAD_ABS_32:
	case BC_STORE_ABS_8:
	case BC_STORE_ABS_16:
	case BC_STORE_ABS_32:
		block->PutCode(generator, mCode);
		if (mRelocate)
		{
			LinkerReference	rl;
			rl.mOffset = block->mCode.Size();
			rl.mLowByte = true;
			rl.mHighByte = true;
			rl.mRefObject = mLinkerObject;
			rl.mRefOffset = mValue;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else
			block->PutWord(mValue);

		block->PutByte(mRegister);
		break;

	case BC_LEA_ABS:
		block->PutCode(generator, mCode); block->PutByte(mRegister);
		if (mRelocate)
		{
			LinkerReference	rl;
			rl.mOffset = block->mCode.Size();
			rl.mLowByte = true;
			rl.mHighByte = true;
			rl.mRefObject = mLinkerObject;
			rl.mRefOffset = mValue;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else
			block->PutWord(uint16(mValue));
		break;

	case BC_LOAD_LOCAL_8:
	case BC_LOAD_LOCAL_U8:
	case BC_LOAD_LOCAL_I8:
	case BC_LOAD_LOCAL_16:
	case BC_LOAD_LOCAL_32:
	case BC_STORE_LOCAL_8:
	case BC_STORE_LOCAL_16:
	case BC_STORE_LOCAL_32:
	case BC_STORE_FRAME_8:
	case BC_STORE_FRAME_16:
	case BC_STORE_FRAME_32:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister); 
		block->PutByte(uint8(mValue));
		break;

	case BC_LEA_LOCAL:
	case BC_LEA_FRAME:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister); 
		block->PutWord(uint16(mValue));
		break;

	case BC_BINOP_ADDR_16:
	case BC_BINOP_SUBR_16:
	case BC_BINOP_ANDR_16:
	case BC_BINOP_ORR_16:
	case BC_BINOP_XORR_16:
	case BC_BINOP_MULR_16:
	case BC_BINOP_DIVR_U16:
	case BC_BINOP_MODR_U16:
	case BC_BINOP_DIVR_I16:
	case BC_BINOP_MODR_I16:
	case BC_BINOP_SHLR_16:
	case BC_BINOP_SHRR_U16:
	case BC_BINOP_SHRR_I16:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		break;

	case BC_BINOP_ADDI_16:
	case BC_BINOP_SUBI_16:
	case BC_BINOP_ANDI_16:
	case BC_BINOP_ORI_16:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister); 
		block->PutWord(uint16(mValue));
		break;
	case BC_BINOP_MULI8_16:
	case BC_BINOP_ADDI_8:
	case BC_BINOP_ANDI_8:
	case BC_BINOP_ORI_8:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		block->PutByte(mValue);
		break;
	case BC_BINOP_SHLI_16:
	case BC_BINOP_SHRI_U16:
	case BC_BINOP_SHRI_I16:
		block->PutCode(generator, mCode);
		block->PutByte(uint8(mValue));
		break;

	case BC_BINOP_CMPUR_16:
	case BC_BINOP_CMPSR_16:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		break;

	case BC_BINOP_CMPUI_16:
	case BC_BINOP_CMPSI_16:
		block->PutCode(generator, mCode);
		block->PutWord(uint16(mValue));
		break;

	case BC_OP_NEGATE_16:
	case BC_OP_INVERT_16:
		block->PutCode(generator, mCode);
		break;

	case BC_BINOP_ADD_F32:
	case BC_BINOP_SUB_F32:
	case BC_BINOP_MUL_F32:
	case BC_BINOP_DIV_F32:
	case BC_BINOP_CMP_F32:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		break;

	case BC_OP_NEGATE_F32:
	case BC_OP_ABS_F32:
	case BC_OP_FLOOR_F32:
	case BC_OP_CEIL_F32:
	case BC_CONV_U16_F32:
	case BC_CONV_I16_F32:
	case BC_CONV_F32_U16:
	case BC_CONV_F32_I16:
		block->PutCode(generator, mCode);
		break;

	case BC_CONV_I8_I16:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		break;

	case BC_JUMPS:
	case BC_BRANCHS_EQ:
	case BC_BRANCHS_NE:
	case BC_BRANCHS_GT:
	case BC_BRANCHS_GE:
	case BC_BRANCHS_LT:
	case BC_BRANCHS_LE:
		assert(false);
		break;

	case BC_JUMPF:
	case BC_BRANCHF_EQ:
	case BC_BRANCHF_NE:
	case BC_BRANCHF_GT:
	case BC_BRANCHF_GE:
	case BC_BRANCHF_LT:
	case BC_BRANCHF_LE:
		assert(false);
		break;

	case BC_SET_EQ:
	case BC_SET_NE:
	case BC_SET_GT:
	case BC_SET_GE:
	case BC_SET_LT:
	case BC_SET_LE:
		block->PutCode(generator, mCode);
		break;

	case BC_ENTER:
	case BC_RETURN:
		assert(false);
		break;

	case BC_CALL:
		block->PutCode(generator, mCode);
		break;

	case BC_PUSH_FRAME:
	case BC_POP_FRAME:
		block->PutCode(generator, mCode);
		block->PutWord(uint16(mValue + 2));
		break;

	case BC_JSR:
	{
		block->PutCode(generator, mCode);

		LinkerReference	rl;
		rl.mOffset = block->mCode.Size();
		rl.mLowByte = true;
		rl.mHighByte = true;
		rl.mRefObject = mLinkerObject;
		rl.mRefOffset = 0;
		block->mRelocations.Push(rl);

		block->PutWord(0);
	}	break;

	case BC_LOAD_ADDR_8:
	case BC_LOAD_ADDR_U8:
	case BC_LOAD_ADDR_I8:
	case BC_LOAD_ADDR_16:
	case BC_LOAD_ADDR_32:
	case BC_STORE_ADDR_8:
	case BC_STORE_ADDR_16:
	case BC_STORE_ADDR_32:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		block->PutByte(mValue);
		break;

	case BC_COPY:
	case BC_COPY_LONG:
		if (mValue < 256)
		{
			block->PutCode(generator, BC_COPY);
			block->PutByte(uint8(mValue));
		}
		else
		{
			block->PutCode(generator, BC_COPY_LONG);
			block->PutByte(uint8(mValue));
		}
		break;

	default:
		assert(false);
	}
}


void ByteCodeBasicBlock::PutByte(uint8 code)
{
	this->mCode.Insert(code);
}

void ByteCodeBasicBlock::PutWord(uint16 code)
{
	this->mCode.Insert((uint8)(code & 0xff));
	this->mCode.Insert((uint8)(code >> 8));
}

void ByteCodeBasicBlock::PutDWord(uint32 code)
{
	this->mCode.Insert((uint8)(code & 0xff));
	this->mCode.Insert((uint8)((code >> 8) & 0xff));
	this->mCode.Insert((uint8)((code >> 16) & 0xff));
	this->mCode.Insert((uint8)((code >> 24) & 0xff));
}

void ByteCodeBasicBlock::PutBytes(const uint8* code, int num)
{
	while (num--)
	{
		this->mCode.Insert(*code++);
	}
}

void ByteCodeBasicBlock::PutCode(ByteCodeGenerator* generator, ByteCode code)
{
	PutByte(uint8(code) * 2);
	generator->mByteCodeUsed[code] = true;
}

int ByteCodeBasicBlock::PutBranch(ByteCodeGenerator* generator, ByteCode code, int offset)
{
	if (offset >= -126 && offset <= 129)
	{
		PutCode(generator, code);
		PutByte(offset - 2);
		return 2;
	}
	else
	{
		PutCode(generator, ByteCode(code + (BC_JUMPF - BC_JUMPS)));
		PutWord(offset - 3);
		return 3;
	}
}

ByteCodeBasicBlock::ByteCodeBasicBlock(void)
	: mRelocations({ 0 }), mIns(ByteCodeInstruction(BC_NOP))
{
	mTrueJump = mFalseJump = NULL;
	mTrueLink = mFalseLink = NULL;
	mOffset = 0x7fffffff;
	mCopied = false;
	mAssembled = false;
	mKnownShortBranch = false;
	mBypassed = false;
}

void ByteCodeBasicBlock::IntConstToAccu(int64 val)
{
	ByteCodeInstruction	ins(BC_CONST_16);
	ins.mRegister = BC_REG_ACCU;
	ins.mValue = int(val);
	mIns.Push(ins);
}

void ByteCodeBasicBlock::FloatConstToAccu(double val)
{
	union { float f; int v; } cc;
	cc.f = val;
	ByteCodeInstruction	bins(BC_CONST_32);
	bins.mRegister = BC_REG_ACCU;
	bins.mValue = cc.v;
	mIns.Push(bins);
}

void ByteCodeBasicBlock::FloatConstToWork(double val)
{
	union { float f; int v; } cc;
	cc.f = val;
	ByteCodeInstruction	bins(BC_CONST_32);
	bins.mRegister = BC_REG_WORK;
	bins.mValue = cc.v;
	mIns.Push(bins);
}


void ByteCodeBasicBlock::IntConstToAddr(int64 val)
{
	ByteCodeInstruction	ins(BC_CONST_16);
	ins.mRegister = BC_REG_ADDR;
	ins.mValue = int(val);
	mIns.Push(ins);
}

void ByteCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mTType == IT_FLOAT)
	{
		union { float f; int v; } cc;
		cc.f = ins->mFloatValue;
		ByteCodeInstruction	bins(BC_CONST_32);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		bins.mValue = cc.v;
		mIns.Push(bins);
	}
	else if (ins->mTType == IT_POINTER)
	{
		if (ins->mMemory == IM_GLOBAL)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mLinkerObject = ins->mLinkerObject;
			bins.mValue = ins->mIntValue;
			bins.mRelocate = true;
			mIns.Push(bins);
		}
		else if (ins->mMemory == IM_ABSOLUTE)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mValue = ins->mIntValue;
			mIns.Push(bins);
		}
		else if (ins->mMemory == IM_LOCAL)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mValue = ins->mIntValue + proc->mLocalVars[ins->mVarIndex]->mOffset;
			mIns.Push(bins);
		}
		else if (ins->mMemory == IM_PARAM)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mValue = ins->mIntValue + ins->mVarIndex + proc->mLocalSize + 2;
			mIns.Push(bins);
		}
		else if (ins->mMemory == IM_FRAME)
		{
			ByteCodeInstruction	bins(BC_LEA_FRAME);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
			mIns.Push(bins);
		}
		else if (ins->mMemory == IM_PROCEDURE)
		{
			ByteCodeInstruction	bins(BC_CONST_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mLinkerObject = ins->mLinkerObject;
			bins.mValue = 0;
			bins.mRelocate = true;
			mIns.Push(bins);
		}
	}
	else if (ins->mTType == IT_BOOL || ins->mTType == IT_INT8)
	{
		ByteCodeInstruction	bins(BC_CONST_8);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		bins.mValue = ins->mIntValue;
		mIns.Push(bins);
	}
	else
	{
		ByteCodeInstruction	bins(BC_CONST_16);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		bins.mValue = ins->mIntValue;
		mIns.Push(bins);
	}

}

void ByteCodeBasicBlock::CopyValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	ByteCodeInstruction	sins(BC_ADDR_REG);
	sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
	sins.mRegisterFinal = ins->mSFinal[1];
	mIns.Push(sins);
	ByteCodeInstruction	dins(BC_LOAD_REG_16);
	dins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
	dins.mRegisterFinal = ins->mSFinal[0];
	mIns.Push(dins);
	ByteCodeInstruction	cins(BC_COPY);
	cins.mValue = ins->mOperandSize;
	mIns.Push(cins);
}

void ByteCodeBasicBlock::StoreDirectValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSType[0] == IT_FLOAT)
	{
		if (ins->mSTemp[1] < 0)
		{
			if (ins->mSTemp[0] < 0)
			{
				FloatConstToAccu(ins->mSFloatConst[0]);

				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
						bins.mRegister = BC_REG_ACCU;
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_STORE_ADDR_32);
						bins.mRegister = BC_REG_ACCU;
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_ACCU;
					bins.mRegisterFinal = ins->mSFinal[0];
					bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_STORE_ADDR_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				FloatConstToAccu(ins->mSFloatConst[0]);

				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
				}
			}
		}
	}
	else if (ins->mSType[0] == IT_POINTER)
	{
		if (ins->mSTemp[1] < 0)
		{
			if (ins->mSTemp[0] < 0)
			{
				IntConstToAccu(ins->mSIntConst[0]);

				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins->mSIntConst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				IntConstToAccu(ins->mSIntConst[0]);

				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
				}
			}
		}
	}
	else
	{
		if (ins->mSTemp[1] < 0)
		{
			if (ins->mSTemp[0] < 0)
			{
				IntConstToAccu(ins->mSIntConst[0]);

				if (ins->mOperandSize == 1)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mLinkerObject;
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;

						if (index <= 255)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_8);
							bins.mRegister = BC_REG_ACCU;
							bins.mValue = index;
							mIns.Push(bins);
						}
						else
						{
							ByteCodeInstruction	lins(BC_LEA_LOCAL);
							lins.mRegister = BC_REG_ADDR;
							lins.mValue = index;
							mIns.Push(lins);
							ByteCodeInstruction	bins(BC_STORE_ADDR_8);
							bins.mRegister = BC_REG_ACCU;
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
						mIns.Push(bins);
					}
				}
				else if (ins->mOperandSize == 2)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mLinkerObject;
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;

						if (index <= 254)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
							bins.mRegister = BC_REG_ACCU;
							bins.mValue = index;
							mIns.Push(bins);
						}
						else
						{
							ByteCodeInstruction	lins(BC_LEA_LOCAL);
							lins.mRegister = BC_REG_ADDR;
							lins.mValue = index;
							mIns.Push(lins);
							ByteCodeInstruction	bins(BC_STORE_ADDR_16);
							bins.mRegister = BC_REG_ACCU;
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (ins->mOperandSize == 1)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mLinkerObject;
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;

						if (index <= 255)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_8);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
							bins.mRegisterFinal = ins->mSFinal[0];
							bins.mValue = index;
							mIns.Push(bins);
						}
						else
						{
							ByteCodeInstruction	lins(BC_LEA_LOCAL);
							lins.mRegister = BC_REG_ADDR;
							lins.mValue = index;
							mIns.Push(lins);
							ByteCodeInstruction	bins(BC_STORE_ADDR_8);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
							bins.mRegisterFinal = ins->mSFinal[0];
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
						mIns.Push(bins);
					}
				}
				else if (ins->mOperandSize == 2)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mLinkerObject;
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins->mSIntConst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						mIns.Push(bins);
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;

						if (index <= 254)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
							bins.mRegisterFinal = ins->mSFinal[0];
							bins.mValue = index;
							mIns.Push(bins);
						}
						else
						{
							ByteCodeInstruction	lins(BC_LEA_LOCAL);
							lins.mRegister = BC_REG_ADDR;
							lins.mValue = index;
							mIns.Push(lins);
							ByteCodeInstruction	bins(BC_STORE_ADDR_16);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
							bins.mRegisterFinal = ins->mSFinal[0];
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = ins->mVarIndex + ins->mSIntConst[1] + 2;
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				IntConstToAccu(ins->mSIntConst[0]);

				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					if (ins->mOperandSize == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mSIntConst[1];
						mIns.Push(bins);
					}
					else if (ins->mOperandSize == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mSIntConst[1];
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);
					if (ins->mOperandSize == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = ins->mSIntConst[1];
						mIns.Push(bins);
					}
					else if (ins->mOperandSize == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = ins->mSIntConst[1];
						mIns.Push(bins);
					}
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadDirectValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mTType == IT_FLOAT)
	{
		if (ins->mSTemp[0] < 0)
		{
			if (ins->mMemory == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mRelocate = true;
				bins.mLinkerObject = ins->mLinkerObject;
				bins.mValue = ins->mSIntConst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				mIns.Push(bins);
			}
			else if (ins->mMemory == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mValue = ins->mSIntConst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				mIns.Push(bins);
			}
			else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
			{
				int	index = ins->mSIntConst[0];
				if (ins->mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mVarIndex]->mOffset;
				else
					index += ins->mVarIndex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = index;
					mIns.Push(bins);
				}
				else
				{
					ByteCodeInstruction	lins(BC_LEA_LOCAL);
					lins.mRegister = BC_REG_ADDR;
					lins.mValue = index;
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = 0;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				bins.mValue = ins->mSIntConst[0];
				mIns.Push(bins);
			}
		}
	}
	else if (ins->mTType == IT_POINTER)
	{
		if (ins->mSTemp[0] < 0)
		{
			if (ins->mMemory == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mRelocate = true;
				bins.mLinkerObject = ins->mLinkerObject;
				bins.mValue = ins->mSIntConst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				mIns.Push(bins);
			}
			else if (ins->mMemory == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mValue = ins->mSIntConst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				mIns.Push(bins);
			}
			else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
			{
				int	index = ins->mSIntConst[0];
				if (ins->mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mVarIndex]->mOffset;
				else
					index += ins->mVarIndex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = index;
					mIns.Push(bins);
				}
				else
				{
					ByteCodeInstruction	lins(BC_LEA_LOCAL);
					lins.mRegister = BC_REG_ADDR;
					lins.mValue = index;
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = 0;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
				bins.mValue = ins->mSIntConst[0];
				mIns.Push(bins);
			}
		}
	}
	else
	{
		if (ins->mSTemp[0] < 0)
		{ 
			if (ins->mOperandSize == 1)
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins((ins->mTType == IT_BOOL || ins->mTType == IT_INT8) ? BC_LOAD_ABS_8 : BC_LOAD_ABS_U8);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins((ins->mTType == IT_BOOL || ins->mTType == IT_INT8) ? BC_LOAD_ABS_8 : BC_LOAD_ABS_U8);
					bins.mValue = ins->mSIntConst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[0];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 255)
					{
						ByteCodeInstruction	bins((ins->mTType == IT_BOOL || ins->mTType == IT_INT8) ? BC_LOAD_LOCAL_8 : BC_LOAD_LOCAL_U8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins((ins->mTType == IT_BOOL || ins->mTType == IT_INT8) ? BC_LOAD_ADDR_8 : BC_LOAD_ADDR_U8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
			}
			else if (ins->mOperandSize == 2)
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mLinkerObject;
					bins.mValue = ins->mSIntConst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mValue = ins->mSIntConst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					mIns.Push(bins);
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[0];
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);

				if (ins->mOperandSize == 1)
				{
					ByteCodeInstruction	bins((ins->mTType == IT_BOOL || ins->mTType == IT_INT8) ? BC_LOAD_ADDR_8 : BC_LOAD_ADDR_U8);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = ins->mSIntConst[0];
					mIns.Push(bins);
				}
				else if (ins->mOperandSize == 2)
				{
					ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = ins->mSIntConst[0];
					mIns.Push(bins);
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSTemp[0] < 0)
	{
		if (ins->mSTemp[1] == ins->mTTemp)
		{
			ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			bins.mValue = ins->mSIntConst[0];
			mIns.Push(bins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
			lins.mRegisterFinal = ins->mSFinal[1];
			mIns.Push(lins);
			ByteCodeInstruction	ains(BC_BINOP_ADDI_16);
			ains.mRegister = BC_REG_ACCU;
			ains.mValue = ins->mSIntConst[0];
			mIns.Push(ains);
			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			mIns.Push(sins);
		}
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
		lins.mRegisterFinal = ins->mSFinal[1];
		mIns.Push(lins);
		ByteCodeInstruction	ains(BC_BINOP_ADDR_16);
		ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		ains.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(ains);
		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);
	}
}
void ByteCodeBasicBlock::CallFunction(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSTemp[0] < 0)
	{
		ByteCodeInstruction	bins(BC_LEA_ABS);
		bins.mRelocate = true;
		bins.mLinkerObject = ins->mLinkerObject;
		bins.mValue = 0;
		bins.mRegister = BC_REG_ADDR;
		mIns.Push(bins);
	}
	else
	{
		ByteCodeInstruction	bins(BC_ADDR_REG);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		bins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(bins);
	}

	ByteCodeInstruction	cins(BC_CALL);
	mIns.Push(cins);

	if (ins->mTTemp >= 0)
	{
		ByteCodeInstruction	bins(StoreTypedTmpCodes[ins->mTType]);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(bins);
	}
}

void ByteCodeBasicBlock::CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSTemp[0] < 0)
	{
		ByteCodeInstruction	bins(BC_JSR);
		bins.mRelocate = true;
		bins.mLinkerObject = ins->mLinkerObject;
		bins.mValue = ins->mSIntConst[0];
		mIns.Push(bins);
	}

	if (ins->mTTemp >= 0)
	{
		ByteCodeInstruction	bins(StoreTypedTmpCodes[ins->mTType]);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(bins);
	}
}

ByteCode ByteCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins)
{
	ByteCode	code;
	bool		csigned = false;

	switch (ins->mOperator)
	{
	default:
	case IA_CMPEQ:
		code = BC_BRANCHS_EQ;
		break;
	case IA_CMPNE:
		code = BC_BRANCHS_NE;
		break;

	case IA_CMPGES:
		csigned = true;
		code = BC_BRANCHS_GE;
		break;
	case IA_CMPGEU:
		code = BC_BRANCHS_GE;
		break;
	case IA_CMPGS:
		csigned = true;
		code = BC_BRANCHS_GT;
		break;
	case IA_CMPGU:
		code = BC_BRANCHS_GT;
		break;
	case IA_CMPLES:
		csigned = true;
		code = BC_BRANCHS_LE;
		break;
	case IA_CMPLEU:
		code = BC_BRANCHS_LE;
		break;
	case IA_CMPLS:
		csigned = true;
		code = BC_BRANCHS_LT;
		break;
	case IA_CMPLU:
		code = BC_BRANCHS_LT;
		break;
	}

	if (ins->mSType[0] == IT_FLOAT)
	{
		if (ins->mSTemp[0] < 0)
		{
			FloatConstToAccu(ins->mSFloatConst[0]);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
			lins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(lins);
		}

		ByteCodeInstruction	cins(BC_BINOP_CMP_F32);

		if (ins->mSTemp[1] < 0)
		{
			FloatConstToWork(ins->mSFloatConst[1]);
			cins.mRegister = BC_REG_WORK;
		}
		else
		{
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
		}

		cins.mRegisterFinal = ins->mSFinal[1];
		mIns.Push(cins);
	}
	else
	{
		if (ins->mSTemp[1] < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
			lins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
				cins.mValue = ins->mSIntConst[1];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
				cins.mValue = ins->mSIntConst[1];
				mIns.Push(cins);
			}
		}
		else if (ins->mSTemp[0] < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
			lins.mRegisterFinal = ins->mSFinal[1];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
				cins.mValue = ins->mSIntConst[0];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
				cins.mValue = ins->mSIntConst[0];
				mIns.Push(cins);
			}
			code = TransposeBranchCondition(code);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
			lins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				cins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				cins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(cins);
			}
		}
	}

	return code;
}

static ByteCode ByteCodeBinRegOperator(const InterInstruction * ins)
{
	if (ins->mTType == IT_FLOAT)
	{
		switch (ins->mOperator)
		{
		case IA_ADD: return BC_BINOP_ADD_F32;
		case IA_SUB: return BC_BINOP_SUB_F32;
		case IA_MUL: return BC_BINOP_MUL_F32;
		case IA_DIVU: return BC_BINOP_DIV_F32;
		case IA_DIVS: return BC_BINOP_DIV_F32;

		default:
			return BC_EXIT;
		}
	}
	else
	{
		switch (ins->mOperator)
		{
		case IA_ADD: return BC_BINOP_ADDR_16;
		case IA_SUB: return BC_BINOP_SUBR_16;
		case IA_MUL: return BC_BINOP_MULR_16;
		case IA_DIVU: return BC_BINOP_DIVR_U16;
		case IA_MODU: return BC_BINOP_MODR_U16;
		case IA_DIVS: return BC_BINOP_DIVR_I16;
		case IA_MODS: return BC_BINOP_MODR_I16;
		case IA_AND: return BC_BINOP_ANDR_16;
		case IA_OR: return BC_BINOP_ORR_16;
		case IA_XOR: return BC_BINOP_XORR_16;

		case IA_SHL: return BC_BINOP_SHLR_16;
		case IA_SHR: return BC_BINOP_SHRR_U16;
		case IA_SAR: return BC_BINOP_SHRR_I16;

		default:
			return BC_EXIT;
		}
	}
}

static ByteCode ByteCodeBinImmOperator(const InterInstruction * ins)
{
	switch (ins->mOperator)
	{
	case IA_ADD: return BC_BINOP_ADDI_16;
	case IA_SUB: return BC_BINOP_SUBI_16;
	case IA_AND: return BC_BINOP_ANDI_16;
	case IA_OR:  return BC_BINOP_ORI_16;
	case IA_SHL: return BC_BINOP_SHLI_16;
	case IA_SHR: return BC_BINOP_SHRI_U16;
	case IA_SAR: return BC_BINOP_SHRI_I16;
	case IA_MUL: return BC_BINOP_MULI8_16;

	default:
		return BC_EXIT;
	}
}

static ByteCode ByteCodeBinSizeImmOperator(const InterInstruction* ins)
{
	switch (ins->mOperator)
	{
	case IA_ADD: return InterTypeSize[ins->mTType] == 1 ? BC_BINOP_ADDI_8 : BC_BINOP_ADDI_16;
	case IA_SUB: return BC_BINOP_SUBI_16;
	case IA_AND: return InterTypeSize[ins->mTType] == 1 ? BC_BINOP_ANDI_8 : BC_BINOP_ANDI_16;
	case IA_OR:  return InterTypeSize[ins->mTType] == 1 ? BC_BINOP_ORI_8 : BC_BINOP_ORI_16;
	case IA_SHL: return BC_BINOP_SHLI_16;
	case IA_SHR: return BC_BINOP_SHRI_U16;
	case IA_SAR: return BC_BINOP_SHRI_I16;
	case IA_MUL: return BC_BINOP_MULI8_16;

	default:
		return BC_EXIT;
	}
}

void ByteCodeBasicBlock::NumericConversion(InterCodeProcedure* proc, const InterInstruction * ins)
{
	switch (ins->mOperator)
	{
	case IA_FLOAT2INT:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		lins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(lins);

		ByteCodeInstruction	bins(BC_CONV_F32_I16);
//		ByteCodeInstruction	bins(ins->mTType == IT_SIGNED ? BC_CONV_F32_I16 : BC_CONV_F32_U16);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);

	}	break;
	case IA_INT2FLOAT:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		lins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(lins);

		ByteCodeInstruction	bins(BC_CONV_I16_F32);
//		ByteCodeInstruction	bins(ins.mSType[0] == IT_SIGNED ? BC_CONV_I16_F32 : BC_CONV_U16_F32);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);

	} break;
	case IA_EXT8TO16S:
	{
		if (ins->mSTemp[0] == ins->mTTemp)
		{
			ByteCodeInstruction	cins(BC_CONV_I8_I16);
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			cins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(cins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
			lins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(lins);

			ByteCodeInstruction	cins(BC_CONV_I8_I16);
			cins.mRegister = BC_REG_ACCU;
			mIns.Push(cins);

			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			mIns.Push(sins);
		}
	}	break;
	case IA_EXT8TO16U:
	{
		if (ins->mSTemp[0] == ins->mTTemp)
		{
			ByteCodeInstruction	cins(BC_BINOP_ANDI_16);
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			cins.mRegisterFinal = ins->mSFinal[0];
			cins.mValue = 0x00ff;
			mIns.Push(cins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
			lins.mRegisterFinal = ins->mSFinal[0];
			mIns.Push(lins);

			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
			mIns.Push(sins);
		}
	}	break;

	}
}

void ByteCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mTType == IT_FLOAT)
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		lins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(lins);

		switch (ins->mOperator)
		{
		case IA_NEG:
		{
			ByteCodeInstruction	oins(BC_OP_NEGATE_F32);
			mIns.Push(oins);
		}	break;
		case IA_ABS:
		{
			ByteCodeInstruction	oins(BC_OP_ABS_F32);
			mIns.Push(oins);
		}	break;
		case IA_FLOOR:
		{
			ByteCodeInstruction	oins(BC_OP_FLOOR_F32);
			mIns.Push(oins);
		}	break;
		case IA_CEIL:
		{
			ByteCodeInstruction	oins(BC_OP_CEIL_F32);
			mIns.Push(oins);
		}	break;
		}

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
		lins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(lins);

		switch (ins->mOperator)
		{
		case IA_NEG:
		{
			ByteCodeInstruction	oins(BC_OP_NEGATE_16);
			mIns.Push(oins);
		}	break;

		case IA_NOT:
		{
			ByteCodeInstruction	oins(BC_OP_INVERT_16);
			mIns.Push(oins);
		}	break;
		}

		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mTType == IT_FLOAT)
	{
		ByteCode	bc = ByteCodeBinRegOperator(ins);

		if (ins->mSTemp[1] < 0)
		{
			FloatConstToAccu(ins->mSFloatConst[1]);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
			lins.mRegisterFinal = ins->mSFinal[1];
			mIns.Push(lins);
		}

		ByteCodeInstruction	bins(bc);

		if (ins->mSTemp[0] < 0)
		{
			FloatConstToWork(ins->mSFloatConst[0]);
			bins.mRegister = BC_REG_WORK;
		}
		else if (ins->mSTemp[1] == ins->mSTemp[0])
			bins.mRegister = BC_REG_ACCU;
		else
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];

		bins.mRegisterFinal = ins->mSFinal[0];
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);
	}
	else
	{
		switch (ins->mOperator)
		{
		case IA_ADD:
		case IA_OR:
		case IA_AND:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			ByteCode	bci = ByteCodeBinImmOperator(ins);
			ByteCode	bcis = ByteCodeBinSizeImmOperator(ins);

			if (ins->mSTemp[1] < 0)
			{
				if (ins->mSTemp[0] == ins->mTTemp)
				{
					ByteCodeInstruction	bins(bcis);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(InterTypeSize[ins->mSType[0]] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSIntConst[1];
				mIns.Push(bins);
			}
			else if (ins->mSTemp[0] < 0)
			{
				if (ins->mSTemp[1] == ins->mTTemp)
				{
					ByteCodeInstruction	bins(bcis);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = ins->mSIntConst[0];
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(InterTypeSize[ins->mSType[1]] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSIntConst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
		}
			break;
		case IA_SUB:
			if (ins->mSTemp[1] < 0)
			{
				if (ins->mSTemp[0] == ins->mTTemp)
				{
					ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSIntConst[1];
				mIns.Push(bins);
			}
			else if (ins->mSTemp[0] < 0)
			{
				if (ins->mSTemp[1] == ins->mTTemp)
				{
					ByteCodeInstruction	bins(InterTypeSize[ins->mSType[0]] == 1 ? BC_BINOP_ADDI_8 : BC_BINOP_ADDI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
					bins.mValue = - ins->mSIntConst[0];
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = - ins->mSIntConst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
			break;
		case IA_MUL:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			ByteCode	bci = ByteCodeBinImmOperator(ins);
			if (ins->mSTemp[1] < 0)
			{
				if (ins->mSIntConst[1] >= 0 && ins->mSIntConst[1] <= 255)
				{
					if (ins->mSTemp[0] == ins->mTTemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
						bins.mRegisterFinal = ins->mSFinal[0];
						bins.mValue = ins->mSIntConst[1];
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					lins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSIntConst[1];
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins->mSIntConst[1]);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
					bins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(bins);
				}
			}
			else if (ins->mSTemp[0] < 0)
			{
				if (ins->mSIntConst[0] >= 0 && ins->mSIntConst[0] <= 255)
				{
					if (ins->mSTemp[1] == ins->mTTemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
						bins.mRegisterFinal = ins->mSFinal[1];
						bins.mValue = ins->mSIntConst[0];
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					lins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSIntConst[0];
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins->mSIntConst[0]);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
					bins.mRegisterFinal = ins->mSFinal[1];
					mIns.Push(bins);
				}
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
		} break;
		case IA_XOR:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins->mSTemp[1] < 0)
			{
				IntConstToAccu(ins->mSIntConst[1]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
			else if (ins->mSTemp[0] < 0)
			{
				IntConstToAccu(ins->mSIntConst[0]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				bins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
		} break;
		case IA_DIVS:
		case IA_MODS:
		case IA_DIVU:
		case IA_MODU:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins->mSTemp[1] < 0)
			{
				IntConstToAccu(ins->mSIntConst[1]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
			else if (ins->mSTemp[0] < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				IntConstToAddr(ins->mSIntConst[0]);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_ADDR;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
		} break;

		case IA_SHL:
		case IA_SHR:
		case IA_SAR:
		{
			ByteCode	rbc = ByteCodeBinRegOperator(ins);

			ByteCode	ibc = ByteCodeBinImmOperator(ins);
			if (ins->mSTemp[1] < 0)
			{
				IntConstToAccu(ins->mSIntConst[1]);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}
			else if (ins->mSTemp[0] < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(ibc);
				bins.mValue = ins->mSIntConst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];
				lins.mRegisterFinal = ins->mSFinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				bins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(bins);
			}

		} break;
		}

		ByteCodeInstruction	sins(InterTypeSize[ins->mSType[1]] == 1 ? BC_STORE_REG_8 : BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::Compile(InterCodeProcedure* iproc, ByteCodeProcedure* proc, InterCodeBasicBlock* sblock)
{
	mIndex = sblock->mIndex;

	int	i = 0;
	while (i < sblock->mInstructions.Size())
	{
		const InterInstruction	* ins = sblock->mInstructions[i];

		switch (ins->mCode)
		{
		case IC_STORE:
			StoreDirectValue(iproc, ins);
			break;
		case IC_LOAD:
			LoadDirectValue(iproc, ins);
			break;
		case IC_COPY:
			CopyValue(iproc, ins);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins->mSTemp[0] != ins->mTTemp)
			{
				switch (ins->mTType)
				{
				case IT_BOOL:
				case IT_INT8:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_8);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
					lins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_8);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mTTemp];
					mIns.Push(sins);
				} break;
				case IT_INT16:
				case IT_POINTER:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
					lins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_16);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mTTemp];
					mIns.Push(sins);
				} break;
				case IT_INT32:
				case IT_FLOAT:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_32);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
					lins.mRegisterFinal = ins->mSFinal[0];
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_32);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mTTemp];
					mIns.Push(sins);
				} break;

				}
			}
		}	break;
		case IC_BINARY_OPERATOR:
			BinaryOperator(iproc, ins);
			break;
		case IC_UNARY_OPERATOR:
			UnaryOperator(iproc, ins);
			break;
		case IC_CONVERSION_OPERATOR:
			NumericConversion(iproc, ins);
			break;
		case IC_LEA:
			LoadEffectiveAddress(iproc, ins);
			break;
		case IC_CONSTANT:
			LoadConstant(iproc, ins);
			break;
		case IC_CALL:
			CallFunction(iproc, ins);
			break;
		case IC_JSR:
			CallAssembler(iproc, ins);
			break;
		case IC_PUSH_FRAME: 
		{
			ByteCodeInstruction	bins(BC_PUSH_FRAME);
			bins.mValue = ins->mIntValue + 2;
			mIns.Push(bins);

		}	break;
		case IC_POP_FRAME:
		{
			ByteCodeInstruction	bins(BC_POP_FRAME);
			bins.mValue = ins->mIntValue + 2;
			mIns.Push(bins);

		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (sblock->mInstructions[i + 1]->mCode == IC_BRANCH && sblock->mInstructions[i + 1]->mSFinal[0])
			{
				ByteCode code = RelationalOperator(iproc, ins);
				this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump), code);
				i++;
				return;
			}
			else
			{
				ByteCode code = RelationalOperator(iproc, ins);
				ByteCodeInstruction	bins(ByteCode(code - BC_BRANCHS_EQ + BC_SET_EQ));
				mIns.Push(bins);
				ByteCodeInstruction	sins(StoreTypedTmpCodes[ins->mTType]);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mTTemp];
				mIns.Push(sins);
			}
			break;

		case IC_RETURN_VALUE:
			if (ins->mSTemp[0] < 0)
				IntConstToAccu(ins->mSIntConst[0]);
			else if (ins->mSType[0] == IT_FLOAT)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_32);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSType[0]] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);
			}

			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_RETURN:
			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_TYPECAST:
			if (ins->mSTemp[0] >= 0 && ins->mTTemp != ins->mSTemp[0])
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	sins(BC_STORE_REG_16);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mTTemp];
				mIns.Push(sins);
			}
			break;

		case IC_BRANCH:
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mSIntConst[0] == 0)
					this->Close(proc->CompileBlock(iproc, sblock->mFalseJump), nullptr, BC_JUMPS);
				else
					this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, BC_JUMPS);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSType[0]] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]];
				lins.mRegisterFinal = ins->mSFinal[0];
				mIns.Push(lins);

				this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump), BC_BRANCHS_NE);
			}
			return;

		}

		i++;
	}

	this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, BC_JUMPS);
}


bool ByteCodeBasicBlock::PeepHoleOptimizer(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int	accuReg = -1;

		bool	progress = false;
		do {
			progress = false;

			int i = 0;
			int j = 0;
			while (i < mIns.Size())
			{
				if (mIns[i].mCode == BC_NOP)
					;
				else
				{
					if (i != j)
						mIns[j] = mIns[i];
					j++;
				}
				i++;
			}
			mIns.SetSize(j);

			int	accuTemp = -1, addrTemp = -1;

			for (int i = 0; i < mIns.Size(); i++)
			{
				if (i + 2 < mIns.Size())
				{
					if (mIns[i].mCode == BC_LOAD_LOCAL_16 &&
						mIns[i + 1].mCode == BC_LOAD_LOCAL_16 && mIns[i + 1].mRegister != BC_REG_ACCU && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i + 2].mRegister == mIns[i].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						if (mIns[i + 2].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_32 && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						if (mIns[i + 2].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].IsStore() && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i].mCode = BC_NOP;
						mIns[i + 2].mRegister = BC_REG_ACCU;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAddr() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_ADDR_REG && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i].mCode = BC_ADDR_REG;
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 2].mCode = BC_NOP;
					}
					else if (
						mIns[i + 2].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 0].LoadsRegister(mIns[i + 2].mRegister) && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = mIns[i + 2].mCode;
						mIns[i + 2].mCode = BC_NOP;
					}
					else if (
						mIns[i + 2].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 0].mCode == BC_STORE_REG_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;;
						mIns[i + 1].mCode = mIns[i + 2].mCode;
						mIns[i + 2].mCode = BC_NOP;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 &&
						mIns[i + 1].mCode == BC_LOAD_REG_32 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 2].IsCommutative() && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						if (mIns[i + 2].mRegisterFinal)
							mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mRegister = mIns[i + 1].mRegister;
						mIns[i + 2].mRegisterFinal = mIns[i + 1].mRegisterFinal;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 2].IsCommutative() && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						if (mIns[i + 2].mRegisterFinal)
							mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mRegister = mIns[i + 1].mRegister;
						mIns[i + 2].mRegisterFinal = mIns[i + 1].mRegisterFinal;
					}
					else if (mIns[i + 0].mCode == BC_STORE_REG_32 &&
						mIns[i + 2].mCode == BC_BINOP_CMP_F32 && mIns[i + 2].mRegister == mIns[i + 0].mRegister && mIns[i + 2].mRegisterFinal &&
						mIns[i + 1].LoadsRegister(BC_REG_ACCU) && i + 3 == mIns.Size())
					{
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						mIns[i + 0].mCode = BC_NOP;
						mBranch = TransposeBranchCondition(mBranch);
					}
					else if (mIns[i + 0].mCode == BC_LOAD_REG_16 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
					}
					else if (mIns[i + 0].mCode == BC_CONST_16 && mIns[i + 2].mCode == BC_CONST_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && mIns[i + 0].mValue == mIns[i + 2].mValue && !mIns[i + 1].ChangesRegister(mIns[i + 0].mRegister))
					{
						mIns[i + 2].mCode = BC_NOP;
					}
					else if (mIns[i + 0].mCode >= BC_SET_EQ && mIns[i + 0].mCode <= BC_SET_LE &&
						mIns[i + 1].mCode == BC_STORE_REG_8 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
					}
				}
				if (i + 1 < mIns.Size())
				{
					if (mIns[i].mCode == BC_STORE_REG_16 && mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 1].mCode = BC_NOP;
						if (mIns[i + 1].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_REG_16 && mIns[i + 1].mCode == BC_STORE_REG_16 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 && mIns[i + 1].mCode == BC_LOAD_REG_32 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 1].mCode = BC_NOP;
						if (mIns[i + 1].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_REG_32 && mIns[i + 1].mCode == BC_STORE_REG_32 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 && mIns[i + 1].mCode == BC_ADDR_REG && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mCode = BC_ADDR_REG;
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i].LoadsRegister(mIns[i + 1].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 1].mCode == BC_LOAD_REG_32 && mIns[i].LoadsRegister(mIns[i + 1].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 && mIns[i + 1].StoresRegister(mIns[i].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 && mIns[i + 1].StoresRegister(mIns[i].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 1].mCode == BC_LOAD_REG_32 && mIns[i].LoadsRegister(mIns[i + 1].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_LOCAL_16 && mIns[i + 1].mCode == BC_ADDR_REG && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ADDR;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_ABS_U8 && mIns[i + 1].mCode == BC_CONV_I8_I16 && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mCode = BC_LOAD_ABS_I8;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_LOCAL_U8 && mIns[i + 1].mCode == BC_CONV_I8_I16 && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mCode = BC_LOAD_LOCAL_I8;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_ADDR_U8 && mIns[i + 1].mCode == BC_CONV_I8_I16 && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mCode = BC_LOAD_ADDR_I8;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
				}

				if ((mIns[i].mCode == BC_LOAD_REG_16 || mIns[i].mCode == BC_STORE_REG_16 || mIns[i].mCode == BC_LOAD_REG_32 || mIns[i].mCode == BC_STORE_REG_32) && accuTemp == mIns[i].mRegister)
					mIns[i].mCode = BC_NOP;
				if (mIns[i].mCode == BC_ADDR_REG && mIns[i].mRegister == addrTemp)
					mIns[i].mCode = BC_NOP;

				if (mIns[i].ChangesAccu())
					accuTemp = -1;
				if (mIns[i].ChangesAddr())
					addrTemp = -1;
				if (accuTemp != -1 && mIns[i].ChangesRegister(accuTemp))
					accuTemp = -1;
				if (addrTemp != -1 && mIns[i].ChangesRegister(addrTemp))
					addrTemp = -1;

				if (mIns[i].mCode == BC_LOAD_REG_16 || mIns[i].mCode == BC_STORE_REG_16 || mIns[i].mCode == BC_LOAD_REG_32 || mIns[i].mCode == BC_STORE_REG_32)
					accuTemp = mIns[i].mRegister;
				if (mIns[i].mCode == BC_ADDR_REG && mIns[i].mRegister != BC_REG_ACCU)
					addrTemp = mIns[i].mRegister;
			}

			if (progress)
				changed = true;
		} while (progress);

		if (mTrueJump && mTrueJump->PeepHoleOptimizer())
			changed = true;
		if (mFalseJump && mFalseJump->PeepHoleOptimizer())
			changed = true;
	}

	return changed;
}

void ByteCodeBasicBlock::Assemble(ByteCodeGenerator* generator)
{
	if (!mAssembled)
	{
		mAssembled = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			mIns[i].Assemble(generator, this);
		}

		if (this->mTrueJump)
			this->mTrueJump->Assemble(generator);
		if (this->mFalseJump)
			this->mFalseJump->Assemble(generator);
	}
}

void ByteCodeBasicBlock::Close(ByteCodeBasicBlock* trueJump, ByteCodeBasicBlock* falseJump, ByteCode branch)
{
	this->mTrueJump = this->mTrueLink = trueJump;
	this->mFalseJump = this->mFalseLink = falseJump;
	this->mBranch = branch;
}

static int BranchByteSize(int from, int to)
{
	if (to - from >= -126 && to - from <= 129)
		return 2;
	else
		return 3;
}

static int JumpByteSize(int from, int to)
{
	if (to - from >= -126 && to - from <= 129)
		return 2;
	else
		return 3;
}

ByteCodeBasicBlock* ByteCodeBasicBlock::BypassEmptyBlocks(void)
{
	if (mBypassed)
		return this;
	else if (!mFalseJump && mCode.Size() == 0 && this != mTrueJump)
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

void ByteCodeBasicBlock::CopyCode(ByteCodeGenerator* generator, LinkerObject* linkerObject, uint8 * target)
{
	int i;
	int next, end;
	int pos, at;
	uint8 b;

	if (!mCopied)
	{
		mCopied = true;

		for (int i = 0; i < mRelocations.Size(); i++)
		{
			LinkerReference	rl = mRelocations[i];
			rl.mObject = linkerObject;
			rl.mOffset += mOffset;
			generator->mLinker->AddReference(rl);
		}

		end = mOffset + mCode.Size();
		next = mOffset + mSize;

		if (mFalseJump)
		{
			if (mFalseJump->mOffset <= mOffset)
			{
				if (mTrueJump->mOffset <= mOffset)
				{
					end += PutBranch(generator, mBranch, mTrueJump->mOffset - end);
					end += PutBranch(generator, BC_JUMPS, mFalseJump->mOffset - end);
					
				}
				else
				{
					end += PutBranch(generator, InvertBranchCondition(mBranch), mFalseJump->mOffset - end);
				}
			}
			else
			{
				end += PutBranch(generator, mBranch, mTrueJump->mOffset - end);
			}
		}
		else if (mTrueJump)
		{
			if (mTrueJump->mOffset != next)
			{
				end += PutBranch(generator, BC_JUMPS, mTrueJump->mOffset - end);
			}
		}

		assert(end == next);

		for (i = 0; i < mCode.Size(); i++)
		{
			mCode.Lookup(i, target[i + mOffset]);
		}

		if (mTrueJump) mTrueJump->CopyCode(generator, linkerObject, target);
		if (mFalseJump) mFalseJump->CopyCode(generator, linkerObject, target);
	}
}

void ByteCodeBasicBlock::CalculateOffset(int& total)
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

				total = next + 3;
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

ByteCodeProcedure::ByteCodeProcedure(void)
	: mBlocks(nullptr)
{
}

ByteCodeProcedure::~ByteCodeProcedure(void)
{
}



void ByteCodeProcedure::Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc)
{
	mID = proc->mID;

	mNumBlocks = proc->mBlocks.Size();

	tblocks = new ByteCodeBasicBlock * [mNumBlocks];
	for (int i = 0; i < mNumBlocks; i++)
		tblocks[i] = nullptr;

	int		tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;

	entryBlock = new ByteCodeBasicBlock();
	mBlocks.Push(entryBlock);
	entryBlock->PutCode(generator, BC_ENTER); entryBlock->PutWord(proc->mLocalSize + 2 + tempSave); entryBlock->PutByte(tempSave);

	if (!proc->mLeafProcedure)
	{
		entryBlock->PutCode(generator, BC_PUSH_FRAME);
		entryBlock->PutWord(uint16(proc->mCommonFrameSize + 2));
	}

	tblocks[0] = entryBlock;

	exitBlock = new ByteCodeBasicBlock();
	mBlocks.Push(exitBlock);

	if (!proc->mLeafProcedure)
	{
		exitBlock->PutCode(generator, BC_POP_FRAME);
		exitBlock->PutWord(uint16(proc->mCommonFrameSize + 2));
	}
	exitBlock->PutCode(generator, BC_RETURN); exitBlock->PutByte(tempSave); exitBlock->PutWord(proc->mLocalSize + 2 + tempSave);

	entryBlock->Compile(proc, this, proc->mBlocks[0]);

	bool	progress = false;
	ResetVisited();
	progress = entryBlock->PeepHoleOptimizer();

	entryBlock->Assemble(generator);

	int	total;

	ByteCodeBasicBlock* lentryBlock = entryBlock->BypassEmptyBlocks();

	total = 0;

	lentryBlock->CalculateOffset(total);

	uint8	*	data = proc->mLinkerObject->AddSpace(total);

	lentryBlock->CopyCode(generator, proc->mLinkerObject, data);
	mProgSize = total; 
}

ByteCodeBasicBlock* ByteCodeProcedure::CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* sblock)
{
	if (tblocks[sblock->mIndex])
		return tblocks[sblock->mIndex];

	ByteCodeBasicBlock	*	block = new ByteCodeBasicBlock();
	mBlocks.Push(block);
	tblocks[sblock->mIndex] = block;
	block->Compile(iproc, this, sblock);
	
	return block;
}

void ByteCodeProcedure::ResetVisited(void)
{
	for (int i = 0; i < mBlocks.Size(); i++)
	{
		mBlocks[i]->mVisited = false;
	}
}

ByteCodeGenerator::ByteCodeGenerator(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker)
{
	for (int i = 0; i < 128; i++)
		mByteCodeUsed[i] = false;

	mByteCodeUsed[BC_LEA_ABS] = true;
	mByteCodeUsed[BC_CALL] = true;
	mByteCodeUsed[BC_EXIT] = true;
	mByteCodeUsed[BC_NATIVE] = true;
}

ByteCodeGenerator::~ByteCodeGenerator(void)
{
}

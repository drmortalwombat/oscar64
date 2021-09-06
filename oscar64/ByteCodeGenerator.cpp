#include "ByteCodeGenerator.h"
#include "Assembler.h"

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
	: mCode(code), mRelocate(false), mFunction(false), mRegisterFinal(false), mVIndex(-1), mValue(0), mRegister(0)
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
	if (mCode == BC_LOAD_REG_16 || mCode == BC_LOAD_REG_32)
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

	return ChangesRegister(BC_REG_ACCU);

}

bool ByteCodeInstruction::ChangesAddr(void) const
{
	if (mCode == BC_ADDR_REG)
		return true;
	if (mCode >= BC_LOAD_ABS_U8 && mCode <= BC_STORE_ABS_32)
		return true;
	if (mCode == BC_JSR || mCode == BC_CALL)
		return true;

	return ChangesRegister(BC_REG_ADDR);
}

bool ByteCodeInstruction::LoadsRegister(uint32 reg) const
{
	if (mRegister == reg)
	{
		if (mCode >= BC_LOAD_ABS_U8 && mCode <= BC_LOAD_ABS_32)
			return true;
		if (mCode >= BC_LOAD_LOCAL_U8 && mCode <= BC_LOAD_LOCAL_32)
			return true;
		if (mCode >= BC_LOAD_ADDR_U8 && mCode <= BC_LOAD_ADDR_32)
			return true;
		if (mCode >= BC_CONST_P8 && mCode <= BC_CONST_32)
			return true;
		if (mCode == BC_LEA_ABS || mCode == BC_LEA_LOCAL)
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
		if (mCode == BC_STORE_REG_16 || mCode == BC_STORE_REG_32)
			return true;
		if (mCode >= BC_LOAD_ABS_U8 && mCode <= BC_LOAD_ABS_32)
			return true;
		if (mCode >= BC_LOAD_LOCAL_U8 && mCode <= BC_LOAD_LOCAL_32)
			return true;
		if (mCode >= BC_LOAD_ADDR_U8 && mCode <= BC_LOAD_ADDR_32)
			return true;
		if (mCode >= BC_CONST_P8 && mCode <= BC_CONST_32)
			return true;
		if (mCode == BC_LEA_ABS || mCode == BC_LEA_LOCAL)
			return true;
		if (mCode >= BC_BINOP_ADDI_16 && mCode <= BC_BINOP_MULI8_16)
			return true;
		if (mCode == BC_CALL)
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

	case BC_CONST_P8:
	case BC_CONST_N8:
	case BC_CONST_16:
		if (mRelocate)
		{
			block->PutCode(generator, mCode);
			block->PutByte(mRegister);
			ByteCodeRelocation	rl;
			rl.mAddr = block->code.Size();
			rl.mFunction = mFunction;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = mVIndex;
			rl.mOffset = mValue;
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


	case BC_LOAD_REG_16:
	case BC_STORE_REG_16:
	case BC_LOAD_REG_32:
	case BC_STORE_REG_32:
	case BC_ADDR_REG:
		block->PutCode(generator, mCode); block->PutByte(mRegister);
		break;

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
			ByteCodeRelocation	rl;
			rl.mAddr = block->code.Size();
			rl.mFunction = mFunction;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = mVIndex;
			rl.mOffset = mValue;
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
			ByteCodeRelocation	rl;
			rl.mAddr = block->code.Size();
			rl.mFunction = mFunction;
			rl.mLower = true;
			rl.mUpper = true;
			rl.mIndex = mVIndex;
			rl.mOffset = mValue;
			block->mRelocations.Push(rl);
			block->PutWord(0);
		}
		else
			block->PutWord(uint16(mValue));
		break;

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

		ByteCodeRelocation	rl;
		rl.mAddr = block->code.Size();
		rl.mFunction = false;
		rl.mLower = true;
		rl.mUpper = true;
		rl.mIndex = mVIndex;
		rl.mOffset = 0;
		block->mRelocations.Push(rl);

		block->PutWord(0);
	}	break;

	case BC_LOAD_ADDR_U8:
	case BC_LOAD_ADDR_I8:
	case BC_LOAD_ADDR_16:
	case BC_LOAD_ADDR_32:
	case BC_STORE_ADDR_8:
	case BC_STORE_ADDR_16:
	case BC_STORE_ADDR_32:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
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
	this->code.Insert(code);
}

void ByteCodeBasicBlock::PutWord(uint16 code)
{
	this->code.Insert((uint8)(code & 0xff));
	this->code.Insert((uint8)(code >> 8));
}

void ByteCodeBasicBlock::PutDWord(uint32 code)
{
	this->code.Insert((uint8)(code & 0xff));
	this->code.Insert((uint8)((code >> 8) & 0xff));
	this->code.Insert((uint8)((code >> 16) & 0xff));
	this->code.Insert((uint8)((code >> 24) & 0xff));
}

void ByteCodeBasicBlock::PutBytes(const uint8* code, int num)
{
	while (num--)
	{
		this->code.Insert(*code++);
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
	trueJump = falseJump = NULL;
	trueLink = falseLink = NULL;
	mOffset = 0x7fffffff;
	copied = false;
	knownShortBranch = false;
	killed = false;
	bypassed = false;
}

void ByteCodeBasicBlock::IntConstToAccu(__int64 val)
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


void ByteCodeBasicBlock::IntConstToAddr(__int64 val)
{
	ByteCodeInstruction	ins(BC_CONST_16);
	ins.mRegister = BC_REG_ADDR;
	ins.mValue = int(val);
	mIns.Push(ins);
}

void ByteCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.ttype == IT_FLOAT)
	{
		union { float f; int v; } cc;
		cc.f = ins.fvalue;
		ByteCodeInstruction	bins(BC_CONST_32);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		bins.mValue = cc.v;
		mIns.Push(bins);
	}
	else if (ins.ttype == IT_POINTER)
	{
		if (ins.mem == IM_GLOBAL)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mVIndex = ins.vindex;
			bins.mValue = ins.ivalue;
			bins.mRelocate = true;
			bins.mFunction = false;
			mIns.Push(bins);
		}
		else if (ins.mem == IM_ABSOLUTE)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mValue = ins.ivalue;
			mIns.Push(bins);
		}
		else if (ins.mem == IM_LOCAL)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mValue = ins.ivalue + proc->mLocalVars[ins.vindex].mOffset;
			mIns.Push(bins);
		}
		else if (ins.mem == IM_PARAM)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mValue = ins.ivalue + ins.vindex + proc->mLocalSize + 2;
			mIns.Push(bins);
		}
		else if (ins.mem == IM_PROCEDURE)
		{
			ByteCodeInstruction	bins(BC_CONST_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mVIndex = ins.vindex;
			bins.mValue = 0;
			bins.mRelocate = true;
			bins.mFunction = true;
			mIns.Push(bins);
		}
	}
	else
	{
		ByteCodeInstruction	bins(BC_CONST_16);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		bins.mValue = ins.ivalue;
		mIns.Push(bins);
	}

}

void ByteCodeBasicBlock::CopyValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	ByteCodeInstruction	sins(BC_ADDR_REG);
	sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
	sins.mRegisterFinal = ins.sfinal[1];
	mIns.Push(sins);
	ByteCodeInstruction	dins(BC_LOAD_REG_16);
	dins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
	dins.mRegisterFinal = ins.sfinal[0];
	mIns.Push(dins);
	ByteCodeInstruction	cins(BC_COPY);
	cins.mValue = ins.opsize;
	mIns.Push(cins);
}

void ByteCodeBasicBlock::StoreDirectValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.stype[0] == IT_FLOAT)
	{
		if (ins.stemp[1] < 0)
		{
			if (ins.stemp[0] < 0)
			{
				FloatConstToAccu(ins.sfconst[0]);

				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[1];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
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
						ByteCodeInstruction	bins(BC_STORE_ADDR_32);
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
				}
				else if (ins.mem == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins.vindex + ins.siconst[1] + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[1];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
				}
				else if (ins.mem == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					bins.mValue = ins.vindex + ins.siconst[1] + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);

				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					mIns.Push(bins);
				}
			}
		}
	}
	else if (ins.stype[0] == IT_POINTER)
	{
		if (ins.stemp[1] < 0)
		{
			if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);

				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[1];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

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
						mIns.Push(bins);
					}
				}
				else if (ins.mem == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins.vindex + ins.siconst[1] + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins.siconst[1];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[1];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
				}
				else if (ins.mem == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					bins.mValue = ins.vindex + ins.siconst[1] + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);

				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					mIns.Push(bins);
				}
			}
		}
	}
	else
	{
		if (ins.stemp[1] < 0)
		{
			if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);

				if (ins.opsize == 1)
				{
					if (ins.mem == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mVIndex = ins.vindex;
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins.mem == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
					{
						int	index = ins.siconst[1];
						if (ins.mem == IM_LOCAL)
							index += proc->mLocalVars[ins.vindex].mOffset;
						else
							index += ins.vindex + proc->mLocalSize + 2;

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
							mIns.Push(bins);
						}
					}
					else if (ins.mem == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins.vindex + ins.siconst[1] + 2;
						mIns.Push(bins);
					}
				}
				else if (ins.opsize == 2)
				{
					if (ins.mem == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mVIndex = ins.vindex;
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins.mem == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
					{
						int	index = ins.siconst[1];
						if (ins.mem == IM_LOCAL)
							index += proc->mLocalVars[ins.vindex].mOffset;
						else
							index += ins.vindex + proc->mLocalSize + 2;

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
							mIns.Push(bins);
						}
					}
					else if (ins.mem == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins.vindex + ins.siconst[1] + 2;
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (ins.opsize == 1)
				{
					if (ins.mem == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mVIndex = ins.vindex;
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
					else if (ins.mem == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
					else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
					{
						int	index = ins.siconst[1];
						if (ins.mem == IM_LOCAL)
							index += proc->mLocalVars[ins.vindex].mOffset;
						else
							index += ins.vindex + proc->mLocalSize + 2;

						if (index <= 255)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_8);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
							bins.mRegisterFinal = ins.sfinal[0];
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
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
							bins.mRegisterFinal = ins.sfinal[0];
							mIns.Push(bins);
						}
					}
					else if (ins.mem == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						bins.mValue = ins.vindex + ins.siconst[1] + 2;
						mIns.Push(bins);
					}
				}
				else if (ins.opsize == 2)
				{
					if (ins.mem == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mVIndex = ins.vindex;
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
					else if (ins.mem == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins.siconst[1];
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
					else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
					{
						int	index = ins.siconst[1];
						if (ins.mem == IM_LOCAL)
							index += proc->mLocalVars[ins.vindex].mOffset;
						else
							index += ins.vindex + proc->mLocalSize + 2;

						if (index <= 254)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
							bins.mRegisterFinal = ins.sfinal[0];
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
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
							bins.mRegisterFinal = ins.sfinal[0];
							mIns.Push(bins);
						}
					}
					else if (ins.mem == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						bins.mValue = ins.vindex + ins.siconst[1] + 2;
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);

				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					if (ins.opsize == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins.opsize == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (ins.mem == IM_INDIRECT)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);
					if (ins.opsize == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
					else if (ins.opsize == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						mIns.Push(bins);
					}
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadDirectValue(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.ttype == IT_FLOAT)
	{
		if (ins.stemp[0] < 0)
		{
			if (ins.mem == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mRelocate = true;
				bins.mVIndex = ins.vindex;
				bins.mValue = ins.siconst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
			else if (ins.mem == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mValue = ins.siconst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
			else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
			{
				int	index = ins.siconst[0];
				if (ins.mem == IM_LOCAL)
					index += proc->mLocalVars[ins.vindex].mOffset;
				else
					index += ins.vindex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
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
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins.mem == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
		}
	}
	else if (ins.ttype == IT_POINTER)
	{
		if (ins.stemp[0] < 0)
		{
			if (ins.mem == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mRelocate = true;
				bins.mVIndex = ins.vindex;
				bins.mValue = ins.siconst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
			else if (ins.mem == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mValue = ins.siconst[0];
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
			else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
			{
				int	index = ins.siconst[0];
				if (ins.mem == IM_LOCAL)
					index += proc->mLocalVars[ins.vindex].mOffset;
				else
					index += ins.vindex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
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
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins.mem == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
				mIns.Push(bins);
			}
		}
	}
	else
	{
		if (ins.stemp[0] < 0)
		{ 
			if (ins.opsize == 1)
			{
				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(ins.ttype == IT_SIGNED ? BC_LOAD_ABS_I8 : BC_LOAD_ABS_U8);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(ins.ttype == IT_SIGNED ? BC_LOAD_ABS_I8 : BC_LOAD_ABS_U8);
					bins.mValue = ins.siconst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[0];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

					if (index <= 255)
					{
						ByteCodeInstruction	bins(ins.ttype == IT_SIGNED ? BC_LOAD_LOCAL_I8 : BC_LOAD_LOCAL_U8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(ins.ttype == IT_SIGNED ? BC_LOAD_ADDR_I8 : BC_LOAD_ADDR_U8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
						mIns.Push(bins);
					}
				}
			}
			else if (ins.opsize == 2)
			{
				if (ins.mem == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mRelocate = true;
					bins.mVIndex = ins.vindex;
					bins.mValue = ins.siconst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mValue = ins.siconst[0];
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
				else if (ins.mem == IM_LOCAL || ins.mem == IM_PARAM)
				{
					int	index = ins.siconst[0];
					if (ins.mem == IM_LOCAL)
						index += proc->mLocalVars[ins.vindex].mOffset;
					else
						index += ins.vindex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins.mem == IM_INDIRECT)
			{
				ByteCodeInstruction	lins(BC_ADDR_REG);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);

				if (ins.opsize == 1)
				{
					if (ins.ttype == IT_SIGNED)
					{
						ByteCodeInstruction	bins(BC_LOAD_ADDR_I8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	bins(BC_LOAD_ADDR_U8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
						mIns.Push(bins);
					}
				}
				else if (ins.opsize == 2)
				{
					ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					mIns.Push(bins);
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.stemp[0] < 0)
	{
		if (ins.stemp[1] == ins.ttemp)
		{
			ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			bins.mValue = ins.siconst[0];
			mIns.Push(bins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
			lins.mRegisterFinal = ins.sfinal[1];
			mIns.Push(lins);
			ByteCodeInstruction	ains(BC_BINOP_ADDI_16);
			ains.mRegister = BC_REG_ACCU;
			ains.mValue = ins.siconst[0];
			mIns.Push(ains);
			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			mIns.Push(sins);
		}
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
		lins.mRegisterFinal = ins.sfinal[1];
		mIns.Push(lins);
		ByteCodeInstruction	ains(BC_BINOP_ADDR_16);
		ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		ains.mRegisterFinal = ins.sfinal[0];
		mIns.Push(ains);
		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::CallFunction(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.stemp[0] < 0)
	{
		ByteCodeInstruction	bins(BC_LEA_ABS);
		bins.mRelocate = true;
		bins.mFunction = true;
		bins.mVIndex = ins.vindex;
		bins.mValue = 0;
		bins.mRegister = BC_REG_ADDR;
		mIns.Push(bins);
	}
	else
	{
		ByteCodeInstruction	bins(BC_ADDR_REG);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		bins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(bins);
	}

	ByteCodeInstruction	cins(BC_CALL);
	mIns.Push(cins);

	if (ins.ttemp >= 0)
	{
		if (ins.ttype == IT_FLOAT)
		{
			ByteCodeInstruction	bins(BC_STORE_REG_32);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			mIns.Push(bins);
		}
		else
		{
			ByteCodeInstruction	bins(BC_STORE_REG_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
			mIns.Push(bins);
		}
	}
}

void ByteCodeBasicBlock::CallAssembler(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.stemp[0] < 0)
	{
		ByteCodeInstruction	bins(BC_JSR);
		bins.mRelocate = true;
		bins.mVIndex = ins.vindex;
		mIns.Push(bins);
	}
}

ByteCode ByteCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{
	ByteCode	code;
	bool		csigned = false;

	switch (ins.oper)
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

	if (ins.stype[0] == IT_FLOAT)
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		lins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(lins);
		ByteCodeInstruction	cins(BC_BINOP_CMP_F32);
		cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
		cins.mRegisterFinal = ins.sfinal[1];
		mIns.Push(cins);
	}
	else
	{
		if (ins.stemp[1] < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
			lins.mRegisterFinal = ins.sfinal[0];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
				cins.mValue = ins.siconst[1];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
				cins.mValue = ins.siconst[1];
				mIns.Push(cins);
			}
		}
		else if (ins.stemp[0] < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
			lins.mRegisterFinal = ins.sfinal[1];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
				cins.mValue = ins.siconst[0];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
				cins.mValue = ins.siconst[0];
				mIns.Push(cins);
			}
			code = TransposeBranchCondition(code);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
			lins.mRegisterFinal = ins.sfinal[0];
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				cins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				cins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(cins);
			}
		}
	}

	return code;
}

static ByteCode ByteCodeBinRegOperator(const InterInstruction& ins)
{
	if (ins.ttype == IT_FLOAT)
	{
		switch (ins.oper)
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
		switch (ins.oper)
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

static ByteCode ByteCodeBinImmOperator(const InterInstruction& ins)
{
	switch (ins.oper)
	{
	case IA_ADD: return BC_BINOP_ADDI_16;
	case IA_SUB: return BC_BINOP_SUBI_16;
	case IA_AND: return BC_BINOP_ANDI_16;
	case IA_OR: return BC_BINOP_ORI_16;
	case IA_SHL: return BC_BINOP_SHLI_16;
	case IA_SHR: return BC_BINOP_SHRI_U16;
	case IA_SAR: return BC_BINOP_SHRI_I16;
	case IA_MUL: return BC_BINOP_MULI8_16;

	default:
		return BC_EXIT;
	}
}

void ByteCodeBasicBlock::NumericConversion(InterCodeProcedure* proc, const InterInstruction& ins)
{
	switch (ins.oper)
	{
	case IA_FLOAT2INT:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		lins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(lins);

		ByteCodeInstruction	bins(ins.ttype == IT_SIGNED ? BC_CONV_F32_I16 : BC_CONV_F32_U16);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);

	}	break;
	case IA_INT2FLOAT:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		lins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(lins);

		ByteCodeInstruction	bins(ins.stype[0] == IT_SIGNED ? BC_CONV_I16_F32 : BC_CONV_U16_F32);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);

	} break;
	}
}

void ByteCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.ttype == IT_FLOAT)
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		lins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(lins);

		switch (ins.oper)
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
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		lins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(lins);

		switch (ins.oper)
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
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, const InterInstruction& ins)
{
	if (ins.ttype == IT_FLOAT)
	{
		ByteCode	bc = ByteCodeBinRegOperator(ins);

		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
		lins.mRegisterFinal = ins.sfinal[1];
		mIns.Push(lins);

		ByteCodeInstruction	bins(bc);
		if (ins.stemp[1] == ins.stemp[0])
			bins.mRegister = BC_REG_ACCU;
		else
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
		bins.mRegisterFinal = ins.sfinal[0];
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);
	}
	else
	{
		switch (ins.oper)
		{
		case IA_ADD:
		case IA_OR:
		case IA_AND:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			ByteCode	bci = ByteCodeBinImmOperator(ins);

			if (ins.stemp[1] < 0)
			{
				if (ins.stemp[0] == ins.ttemp)
				{
					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					bins.mValue = ins.siconst[1];
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins.siconst[1];
				mIns.Push(bins);
			}
			else if (ins.stemp[0] < 0)
			{
				if (ins.stemp[1] == ins.ttemp)
				{
					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					bins.mValue = ins.siconst[0];
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins.siconst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
		}
			break;
		case IA_SUB:
			if (ins.stemp[1] < 0)
			{
				if (ins.stemp[0] == ins.ttemp)
				{
					ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					bins.mValue = ins.siconst[1];
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins.siconst[1];
				mIns.Push(bins);
			}
			else if (ins.stemp[0] < 0)
			{
				if (ins.stemp[1] == ins.ttemp)
				{
					ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
					bins.mValue = - ins.siconst[0];
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = - ins.siconst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
			break;
		case IA_MUL:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			ByteCode	bci = ByteCodeBinImmOperator(ins);
			if (ins.stemp[1] < 0)
			{
				if (ins.siconst[1] >= 0 && ins.siconst[1] <= 255)
				{
					if (ins.stemp[0] == ins.ttemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
						bins.mRegisterFinal = ins.sfinal[0];
						bins.mValue = ins.siconst[1];
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					lins.mRegisterFinal = ins.sfinal[0];
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins.siconst[1];
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins.siconst[1]);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
					bins.mRegisterFinal = ins.sfinal[0];
					mIns.Push(bins);
				}
			}
			else if (ins.stemp[0] < 0)
			{
				if (ins.siconst[0] >= 0 && ins.siconst[0] <= 255)
				{
					if (ins.stemp[1] == ins.ttemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
						bins.mRegisterFinal = ins.sfinal[1];
						bins.mValue = ins.siconst[0];
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					lins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins.siconst[0];
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins.siconst[0]);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
					bins.mRegisterFinal = ins.sfinal[1];
					mIns.Push(bins);
				}
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
		} break;
		case IA_XOR:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins.stemp[1] < 0)
			{
				IntConstToAccu(ins.siconst[1]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
			else if (ins.stemp[0] < 0)
			{
				IntConstToAccu(ins.siconst[0]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				bins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
		} break;
		case IA_DIVS:
		case IA_MODS:
		case IA_DIVU:
		case IA_MODU:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins.stemp[1] < 0)
			{
				IntConstToAccu(ins.siconst[1]);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
			else if (ins.stemp[0] < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				IntConstToAddr(ins.siconst[0]);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_ADDR;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
		} break;

		case IA_SHL:
		case IA_SHR:
		case IA_SAR:
		{
			ByteCode	rbc = ByteCodeBinRegOperator(ins);

			ByteCode	ibc = ByteCodeBinImmOperator(ins);
			if (ins.stemp[1] < 0)
			{
				IntConstToAccu(ins.siconst[1]);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}
			else if (ins.stemp[0] < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(ibc);
				bins.mValue = ins.siconst[0];
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[1]];
				lins.mRegisterFinal = ins.sfinal[1];
				mIns.Push(lins);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.stemp[0]];
				bins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(bins);
			}

		} break;
		}

		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins.ttemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::Compile(InterCodeProcedure* iproc, ByteCodeProcedure* proc, InterCodeBasicBlock* sblock)
{
	num = sblock->num;

	int	i = 0;
	while (i < sblock->code.Size())
	{
		const InterInstruction	& ins = sblock->code[i];

		switch (ins.code)
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
			if (ins.stemp[0] != ins.ttemp)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	sins(BC_STORE_REG_16);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.ttemp];
				mIns.Push(sins);
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
			bins.mValue = ins.ivalue + 2;
			mIns.Push(bins);

		}	break;
		case IC_POP_FRAME:
		{
			ByteCodeInstruction	bins(BC_POP_FRAME);
			bins.mValue = ins.ivalue + 2;
			mIns.Push(bins);

		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (sblock->code[i + 1].code == IC_BRANCH)
			{
				ByteCode code = RelationalOperator(iproc, ins);
				this->Close(proc->CompileBlock(iproc, sblock->trueJump), proc->CompileBlock(iproc, sblock->falseJump), code);
				i++;
				return;
			}
			else
			{
				ByteCode code = RelationalOperator(iproc, ins);
				ByteCodeInstruction	bins(ByteCode(code - BC_BRANCHS_EQ + BC_SET_EQ));
				mIns.Push(bins);
				ByteCodeInstruction	sins(BC_STORE_REG_16);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.ttemp];
				mIns.Push(sins);
			}
			break;

		case IC_RETURN_VALUE:
			if (ins.stemp[0] < 0)
				IntConstToAccu(ins.siconst[0]);
			else if (ins.stype[0] == IT_FLOAT)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_32);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
			}

			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_RETURN:
			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_TYPECAST:
			if (ins.stemp[0] >= 0 && ins.ttemp != ins.stemp[0])
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);
				ByteCodeInstruction	sins(BC_STORE_REG_16);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.ttemp];
				mIns.Push(sins);
			}
			break;

		case IC_BRANCH:
			if (ins.stemp[0] < 0)
			{
				if (ins.siconst[0] == 0)
					this->Close(proc->CompileBlock(iproc, sblock->falseJump), nullptr, BC_JUMPS);
				else
					this->Close(proc->CompileBlock(iproc, sblock->trueJump), nullptr, BC_JUMPS);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins.stemp[0]];
				lins.mRegisterFinal = ins.sfinal[0];
				mIns.Push(lins);

				this->Close(proc->CompileBlock(iproc, sblock->trueJump), proc->CompileBlock(iproc, sblock->falseJump), BC_BRANCHS_NE);
			}
			return;

		}

		i++;
	}

	this->Close(proc->CompileBlock(iproc, sblock->trueJump), nullptr, BC_JUMPS);
}

void ByteCodeBasicBlock::PeepHoleOptimizer(void)
{
	int	accuReg = -1;

	bool	progress = false;
	do	{
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
					branch = TransposeBranchCondition(branch);
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
	} while (progress);
}

void ByteCodeBasicBlock::Assemble(ByteCodeGenerator* generator)
{
	if (!mAssembled)
	{
		mAssembled = true;

		PeepHoleOptimizer();

		for (int i = 0; i < mIns.Size(); i++)
			mIns[i].Assemble(generator, this);

		if (this->trueJump)
			this->trueJump->Assemble(generator);
		if (this->falseJump)
			this->falseJump->Assemble(generator);
	}
}

void ByteCodeBasicBlock::Close(ByteCodeBasicBlock* trueJump, ByteCodeBasicBlock* falseJump, ByteCode branch)
{
	this->trueJump = this->trueLink = trueJump;
	this->falseJump = this->falseLink = falseJump;
	this->branch = branch;
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
	if (bypassed)
		return this;
	else if (!falseJump && code.Size() == 0)
		return trueJump->BypassEmptyBlocks();
	else
	{
		bypassed = true;

		if (falseJump)
			falseJump = falseJump->BypassEmptyBlocks();
		if (trueJump)
			trueJump = trueJump->BypassEmptyBlocks();

		return this;
	}
}

void ByteCodeBasicBlock::CopyCode(ByteCodeGenerator* generator, uint8 * target)
{
	int i;
	int next;
	int pos, at;
	uint8 b;

	if (!copied)
	{
		copied = true;

		for (int i = 0; i < mRelocations.Size(); i++)
		{
			ByteCodeRelocation	rl = mRelocations[i];
			rl.mAddr += target - generator->mMemory + mOffset;
			generator->mRelocations.Push(rl);
		}

		next = mOffset + code.Size();

		if (falseJump)
		{
			if (falseJump->mOffset <= mOffset)
			{
				if (trueJump->mOffset <= mOffset)
				{
					next += PutBranch(generator, branch, trueJump->mOffset - next);
					next += PutBranch(generator, BC_JUMPS, falseJump->mOffset - next);
					
				}
				else
				{
					next += PutBranch(generator, InvertBranchCondition(branch), falseJump->mOffset - next);
				}
			}
			else
			{
				next += PutBranch(generator, branch, trueJump->mOffset - next);
			}
		}
		else if (trueJump)
		{
			if (trueJump->mOffset != next)
			{
				next += PutBranch(generator, BC_JUMPS, trueJump->mOffset - next);
			}
		}

		assert(next - mOffset == mSize);

		for (i = 0; i < code.Size(); i++)
		{
			code.Lookup(i, target[i + mOffset]);
		}

		if (trueJump) trueJump->CopyCode(generator, target);
		if (falseJump) falseJump->CopyCode(generator, target);
	}
}

void ByteCodeBasicBlock::CalculateOffset(int& total)
{
	int next;

	if (mOffset > total)
	{
		mOffset = total;
		next = total + code.Size();

		if (falseJump)
		{
			if (falseJump->mOffset <= total)
			{
				// falseJump has been placed

				if (trueJump->mOffset <= total)
				{
					// trueJump and falseJump have been placed, not much to do

					next = next + BranchByteSize(next, trueJump->mOffset);
					total = next + JumpByteSize(next, falseJump->mOffset);
					mSize = total - mOffset;
				}
				else
				{
					// trueJump has not been placed, but falseJump has

					total = next + BranchByteSize(next, falseJump->mOffset);
					mSize = total - mOffset;
					trueJump->CalculateOffset(total);
				}
			}
			else if (trueJump->mOffset <= total)
			{
				// falseJump has not been placed, but trueJump has

				total = next + BranchByteSize(next, trueJump->mOffset);
				mSize = total - mOffset;
				falseJump->CalculateOffset(total);
			}
			else if (knownShortBranch)
			{
				// neither falseJump nor trueJump have been placed,
				// but we know from previous iteration that we can do
				// a short branch

				total = next + 2;
				mSize = total - mOffset;

				falseJump->CalculateOffset(total);
				if (trueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now
					trueJump->CalculateOffset(total);
				}
			}
			else
			{
				// neither falseJump nor trueJump have been placed
				// this may lead to some undo operation...
				// first assume a full size branch:

				total = next + 3;
				mSize = total - mOffset;

				falseJump->CalculateOffset(total);
				if (trueJump->mOffset > total)
				{
					// trueJump was not placed in the process, so lets place it now

					trueJump->CalculateOffset(total);
				}

				if (BranchByteSize(next, trueJump->mOffset) < 3)
				{
					// oh, we can replace by a short branch

					knownShortBranch = true;

					total = next + 2;
					mSize = total - mOffset;

					falseJump->CalculateOffset(total);
					if (trueJump->mOffset > total)
					{
						// trueJump was not placed in the process, so lets place it now

						trueJump->CalculateOffset(total);
					}
				}
			}
		}
		else if (trueJump)
		{
			if (trueJump->mOffset <= total)
			{
				// trueJump has been placed, so append the branch size

				total = next + JumpByteSize(next, trueJump->mOffset);
				mSize = total - mOffset;
			}
			else
			{
				// we have to place trueJump, so just put it right behind us

				total = next;
				mSize = total - mOffset;

				trueJump->CalculateOffset(total);
			}
		}
		else
		{
			// no exit from block

			total += code.Size();
			mSize = total - mOffset;
		}
	}
}

ByteCodeProcedure::ByteCodeProcedure(void)
{
}

ByteCodeProcedure::~ByteCodeProcedure(void)
{
}



void ByteCodeProcedure::Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc)
{
	tblocks = new ByteCodeBasicBlock * [proc->blocks.Size()];
	for (int i = 0; i < proc->blocks.Size(); i++)
		tblocks[i] = nullptr;

	int		tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;

	entryBlock = new ByteCodeBasicBlock();
	entryBlock->PutCode(generator, BC_ENTER); entryBlock->PutWord(proc->mLocalSize + 2 + tempSave); entryBlock->PutByte(tempSave);

	if (!proc->mLeafProcedure)
	{
		entryBlock->PutCode(generator, BC_PUSH_FRAME);
		entryBlock->PutWord(uint16(proc->mCommonFrameSize + 2));
	}

	tblocks[0] = entryBlock;

	exitBlock = new ByteCodeBasicBlock();

	if (!proc->mLeafProcedure)
	{
		exitBlock->PutCode(generator, BC_POP_FRAME);
		exitBlock->PutWord(uint16(proc->mCommonFrameSize + 2));
	}
	exitBlock->PutCode(generator, BC_RETURN); exitBlock->PutByte(tempSave); exitBlock->PutWord(proc->mLocalSize + 2 + tempSave);

	entryBlock->Compile(proc, this, proc->blocks[0]);
	entryBlock->Assemble(generator);

	int	total;

	ByteCodeBasicBlock* lentryBlock = entryBlock->BypassEmptyBlocks();

	total = 0;

	lentryBlock->CalculateOffset(total);

	generator->AddAddress(proc->mID, true, generator->mProgEnd, total, proc->mIdent, false);

	mProgStart = generator->mProgEnd;
	lentryBlock->CopyCode(generator, generator->mMemory + generator->mProgEnd);
	mProgSize = total; 

	generator->mProgEnd += total;
}

ByteCodeBasicBlock* ByteCodeProcedure::CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* sblock)
{
	if (tblocks[sblock->num])
		return tblocks[sblock->num];

	ByteCodeBasicBlock	*	block = new ByteCodeBasicBlock();
	tblocks[sblock->num] = block;
	block->Compile(iproc, this, sblock);
	
	return block;
}

const char* ByteCodeProcedure::TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc)
{
	if (tmp == BC_REG_ADDR)
		return "ADDR";
	else if (tmp == BC_REG_ACCU)
		return "ACCU";
	else if (tmp >= BC_REG_TMP && tmp < BC_REG_TMP + proc->mTempSize)
	{
		int	i = 0;
		while (i + 1 < proc->mTempOffset.Size() && proc->mTempOffset[i + 1] <= tmp - BC_REG_TMP)
			i++;
		sprintf_s(buffer, 10, "T%d", i);
		return buffer;
	}
	else
	{
		sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}
}


void ByteCodeProcedure::Disassemble(FILE * file, ByteCodeGenerator * generator, InterCodeProcedure * proc)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc->mIdent)
		fprintf(file, "%s:\n", proc->mIdent->mString);

	char	tbuffer[10];

	int	i = 0;
	while (i < mProgSize)
	{
		ByteCode	bc = ByteCode(generator->mMemory[mProgStart + i] / 2);

		fprintf(file, "%04x:\t", mProgStart + i);
		i++;

		switch (bc)
		{
		case BC_NOP:
			fprintf(file, "NOP");
			break;
		case BC_EXIT:
			fprintf(file, "EXIT");
			break;

		case BC_CONST_P8:
			fprintf(file, "MOV\t%s, #%d", TempName(generator->mMemory[mProgStart + i], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;
		case BC_CONST_N8:
			fprintf(file, "MOV\t%s, #%d", TempName(generator->mMemory[mProgStart + i], tbuffer, proc), int(generator->mMemory[mProgStart + i + 1]) - 0x100);
			i += 2;
			break;
		case BC_CONST_16:
			fprintf(file, "MOV\t%s, #$%04x", TempName(generator->mMemory[mProgStart + i], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_CONST_32:
			fprintf(file, "MOVD\t%s, #$%08x", TempName(generator->mMemory[mProgStart + i], tbuffer, proc), uint32(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2] + 0x10000 * generator->mMemory[mProgStart + i + 3] + 0x1000000 * generator->mMemory[mProgStart + i + 4]));
			i += 5;
			break;

		case BC_LOAD_REG_16:
			fprintf(file, "MOV\tACCU, %s", TempName(generator->mMemory[mProgStart + i], tbuffer, proc));
			i += 1;
			break;
		case BC_STORE_REG_16:
			fprintf(file, "MOV\t%s, ACCU", TempName(generator->mMemory[mProgStart + i], tbuffer, proc));
			i += 1;
			break;
		case BC_LOAD_REG_32:
			fprintf(file, "MOVD\tACCU, %s", TempName(generator->mMemory[mProgStart + i], tbuffer, proc));
			i += 1;
			break;
		case BC_STORE_REG_32:
			fprintf(file, "MOVD\t%s, ACCU", TempName(generator->mMemory[mProgStart + i], tbuffer, proc));
			i += 1;
			break;
		case BC_ADDR_REG:
			fprintf(file, "MOV\tADDR, %s", TempName(generator->mMemory[mProgStart + i], tbuffer, proc));
			i += 1;
			break;

		case BC_LOAD_ABS_U8:
			fprintf(file, "MOVUB\t%s, $%04x", TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 3;
			break;
		case BC_LOAD_ABS_I8:
			fprintf(file, "MOVSB\t%s, $%04x", TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 3;
			break;
		case BC_LOAD_ABS_16:
			fprintf(file, "MOV\t%s, $%04x", TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 3;
			break;
		case BC_LOAD_ABS_32:
			fprintf(file, "MOVD\t%s, $%04x", TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 3;
			break;

		case BC_LEA_ABS:
			fprintf(file, "LEA\t%s, $%04x", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;

		case BC_STORE_ABS_8:
			fprintf(file, "MOVB\t$%04x, %s", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]), TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc));
			i += 3;
			break;
		case BC_STORE_ABS_16:
			fprintf(file, "MOV\t$%04x, %s", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]), TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc));
			i += 3;
			break;
		case BC_STORE_ABS_32:
			fprintf(file, "MOVD\t$%04x, %s", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]), TempName(generator->mMemory[mProgStart + i + 2], tbuffer, proc));
			i += 3;
			break;

		case BC_LOAD_LOCAL_U8:
			fprintf(file, "MOVUB\t%s, %d(FP)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_I8:
			fprintf(file, "MOVSB\t%s, %d(FP)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_16:
			fprintf(file, "MOV\t%s, %d(FP)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_32:
			fprintf(file, "MOVD\t%s, %d(FP)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;

		case BC_STORE_LOCAL_8:
			fprintf(file, "MOVB\t%d(FP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_LOCAL_16:
			fprintf(file, "MOV\t%d(FP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_LOCAL_32:
			fprintf(file, "MOVD\t%d(FP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;

		case BC_LEA_LOCAL:
			fprintf(file, "LEA\t%s, %d(FP)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;

		case BC_STORE_FRAME_8:
			fprintf(file, "MOVB\t%d(SP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_FRAME_16:
			fprintf(file, "MOV\t%d(SP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_FRAME_32:
			fprintf(file, "MOVD\t%d(SP), %s", generator->mMemory[mProgStart + i + 1], TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 2;
			break;

		case BC_BINOP_ADDR_16:
			fprintf(file, "ADD\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SUBR_16:
			fprintf(file, "SUB\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MULR_16:
			fprintf(file, "MUL\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIVR_U16:
			fprintf(file, "DIVU\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MODR_U16:
			fprintf(file, "MODU\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIVR_I16:
			fprintf(file, "DIVS\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MODR_I16:
			fprintf(file, "MODS\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_ANDR_16:
			fprintf(file, "AND\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_ORR_16:
			fprintf(file, "OR\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_XORR_16:
			fprintf(file, "XOR\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHLR_16:
			fprintf(file, "SHL\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHRR_I16:
			fprintf(file, "SHRU\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHRR_U16:
			fprintf(file, "SHRI\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_BINOP_ADDI_16:
			fprintf(file, "ADD\t%s, #$%04X", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_BINOP_SUBI_16:
			fprintf(file, "SUBR\t%s, #$%04X", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ANDI_16:
			fprintf(file, "AND\t%s, #$%04X", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ORI_16:
			fprintf(file, "OR\t%s, #$%04X", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_BINOP_MULI8_16:
			fprintf(file, "MUL\t%s, #%d", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc), generator->mMemory[mProgStart + i + 1]);
			i += 2;
			break;

		case BC_BINOP_SHLI_16:
			fprintf(file, "SHL\tACCU, #%d", uint8(generator->mMemory[mProgStart + i + 0]));
			i += 1;
			break;
		case BC_BINOP_SHRI_U16:
			fprintf(file, "SHRU\tACCU, #%d", uint8(generator->mMemory[mProgStart + i + 0]));
			i += 1;
			break;
		case BC_BINOP_SHRI_I16:
			fprintf(file, "SHRS\tACCU, #%d", uint8(generator->mMemory[mProgStart + i + 0]));
			i += 1;
			break;

		case BC_BINOP_CMPUR_16:
			fprintf(file, "CMPU\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_CMPSR_16:
			fprintf(file, "CMPS\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_BINOP_CMPUI_16:
			fprintf(file, "CMPU\tACCU, #$%04X", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BINOP_CMPSI_16:
			fprintf(file, "CMPS\tACCU, #$%04X", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_BINOP_ADD_F32:
			fprintf(file, "ADDF\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SUB_F32:
			fprintf(file, "SUBF\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MUL_F32:
			fprintf(file, "MULF\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIV_F32:
			fprintf(file, "DIVF\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_CMP_F32:
			fprintf(file, "CMPF\tACCU, %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_COPY:
			fprintf(file, "COPY\t#%d", generator->mMemory[mProgStart + i + 0]);
			i++;
			break;

		case BC_COPY_LONG:
			fprintf(file, "COPYL\t#%d", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_OP_NEGATE_16:
			fprintf(file, "NEG\tACCU");
			break;
		case BC_OP_INVERT_16:
			fprintf(file, "NOT\tACCU");
			break;

		case BC_OP_NEGATE_F32:
			fprintf(file, "NEGF\tACCU");
			break;
		case BC_OP_ABS_F32:
			fprintf(file, "ABSF\tACCU");
			break;
		case BC_OP_FLOOR_F32:
			fprintf(file, "FLOORF\tACCU");
			break;
		case BC_OP_CEIL_F32:
			fprintf(file, "CEILF\tACCU");
			break;


		case BC_CONV_U16_F32:
			fprintf(file, "CNVUF\tACCU");
			break;
		case BC_CONV_I16_F32:
			fprintf(file, "CNVSF\tACCU");
			break;
		case BC_CONV_F32_U16:
			fprintf(file, "CNVFU\tACCU");
			break;
		case BC_CONV_F32_I16:
			fprintf(file, "CNVFS\tACCU");
			break;

		case BC_JUMPS:
			fprintf(file, "JUMP\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_EQ:
			fprintf(file, "BEQ\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_NE:
			fprintf(file, "BNE\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_GT:
			fprintf(file, "BGT\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_GE:
			fprintf(file, "BGE\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_LT:
			fprintf(file, "BLT\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;
		case BC_BRANCHS_LE:
			fprintf(file, "BLE\t$%04X", mProgStart + i + 1 + int8(generator->mMemory[mProgStart + i + 0]));
			i++;
			break;

		case BC_JUMPF:
			fprintf(file, "JUMPF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_EQ:
			fprintf(file, "BEQF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_NE:
			fprintf(file, "BNEF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_GT:
			fprintf(file, "BGTF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_GE:
			fprintf(file, "BGEF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_LT:
			fprintf(file, "BLTF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_LE:
			fprintf(file, "BLEF\t$%04X", mProgStart + i + 2 + int16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_SET_EQ:
			fprintf(file, "SEQ");
			break;
		case BC_SET_NE:
			fprintf(file, "SNE");
			break;
		case BC_SET_GT:
			fprintf(file, "SGT");
			break;
		case BC_SET_GE:
			fprintf(file, "SGE");
			break;
		case BC_SET_LT:
			fprintf(file, "SLT");
			break;
		case BC_SET_LE:
			fprintf(file, "SLE");
			break;

		case BC_ENTER:
			fprintf(file, "ENTER\t%d, %d", generator->mMemory[mProgStart + i + 2], uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 3;
			break;
		case BC_RETURN:
			fprintf(file, "RETURN\t%d, %d", generator->mMemory[mProgStart + i], uint16(generator->mMemory[mProgStart + i + 1] + 256 * generator->mMemory[mProgStart + i + 2]));
			i += 3;
			break;
		case BC_CALL:
			fprintf(file, "CALL");
			break;
		case BC_JSR:
			fprintf(file, "JSR\t$%04x", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_PUSH_FRAME:
			fprintf(file, "PUSH\t#$%04X", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_POP_FRAME:
			fprintf(file, "POP\t#$%04X", uint16(generator->mMemory[mProgStart + i + 0] + 256 * generator->mMemory[mProgStart + i + 1]));
			i += 2;
			break;

		case BC_LOAD_ADDR_U8:
			fprintf(file, "MOVUB\t%s, (ADDR)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;
		case BC_LOAD_ADDR_I8:
			fprintf(file, "MOVSB\t%s, (ADDR)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;
		case BC_LOAD_ADDR_16:
			fprintf(file, "MOV\t%s, (ADDR)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;
		case BC_LOAD_ADDR_32:
			fprintf(file, "MOVD\t%s, (ADDR)", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;

		case BC_STORE_ADDR_8:
			fprintf(file, "MOVB\t(ADDR), %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;
		case BC_STORE_ADDR_16:
			fprintf(file, "MOV\t(ADDR), %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;
		case BC_STORE_ADDR_32:
			fprintf(file, "MOV\t(ADDR), %s", TempName(generator->mMemory[mProgStart + i + 0], tbuffer, proc));
			i++;
			break;

		}

		fprintf(file, "\n");
	}
}

ByteCodeGenerator::ByteCodeGenerator(void)
	: mProcedureAddr({ 0 }), mGlobalAddr({ 0 }), mRelocations({ 0 })
{
	mProgStart = mProgEnd = 0x0801;
	for (int i = 0; i < 128; i++)
		mByteCodeUsed[i] = false;
}

ByteCodeGenerator::~ByteCodeGenerator(void)
{
}

int ByteCodeGenerator::AddGlobal(int index, const Ident* ident, int size, const uint8* data, bool assembler)
{
	int	addr = mProgEnd;

	AddAddress(index, false, addr, size, ident, assembler);

	if (data)
	{
		for (int i = 0; i < size; i++)
			mMemory[mProgEnd + i] = data[i];
	}
	else
	{
		for (int i = 0; i < size; i++)
			mMemory[mProgEnd + i] = 0;
	}
		
	mProgEnd += size;

	return addr;
}

void ByteCodeGenerator::AddAddress(int index, bool function, int address, int size, const Ident* ident, bool assembler)
{
	Address	addr;
	addr.mIndex = index;
	addr.mFunction = function;
	addr.mAddress = address;
	addr.mSize = size;
	addr.mIdent = ident;
	addr.mAssembler = assembler;
	if (function)
		mProcedureAddr[index] = addr;
	else
		mGlobalAddr[index] = addr;
}


void ByteCodeGenerator::WriteBasicHeader(void)
{
	mMemory[mProgEnd++] = 0x0b;
	mMemory[mProgEnd++] = 0x08;
	mMemory[mProgEnd++] = 0x0a;
	mMemory[mProgEnd++] = 0x00;
	mMemory[mProgEnd++] = 0x9e;
	mMemory[mProgEnd++] = 0x32;
	mMemory[mProgEnd++] = 0x30;
	mMemory[mProgEnd++] = 0x36;
	mMemory[mProgEnd++] = 0x31;
	mMemory[mProgEnd++] = 0x00;
	mMemory[mProgEnd++] = 0x00;
	mMemory[mProgEnd++] = 0x00;
}

void ByteCodeGenerator::SetBasicEntry(int index)
{
	mProgEntry = index;
}

void ByteCodeGenerator::WriteByteCodeHeader(void)
{
	int	n = mProgEnd + 6;

	mMemory[mProgEnd++] = BC_LEA_ABS * 2;
	mMemory[mProgEnd++] = BC_REG_ADDR;
	mMemory[mProgEnd++] = n & 255;
	mMemory[mProgEnd++] = n >> 8;
	mMemory[mProgEnd++] = BC_CALL * 2;
	mMemory[mProgEnd++] = BC_EXIT * 2;

	mByteCodeUsed[BC_LEA_ABS] = true;
	mByteCodeUsed[BC_CALL] = true;
	mByteCodeUsed[BC_EXIT] = true;
}

void ByteCodeGenerator::ResolveRelocations(void)
{
	for (int i = 0; i < mRelocations.Size(); i++)
	{
		int	dp = mRelocations[i].mAddr;
		int	sp;
		if (mRelocations[i].mFunction)
			sp = mProcedureAddr[mRelocations[i].mIndex].mAddress + mRelocations[i].mOffset;
		else
			sp = mGlobalAddr[mRelocations[i].mIndex].mAddress + mRelocations[i].mOffset;
		if (mRelocations[i].mLower)
			mMemory[dp++] = sp & 255;
		if (mRelocations[i].mUpper)
			mMemory[dp++] = sp >> 8;
	}

	int	entry = mGlobalAddr[mProgEntry].mAddress;

	mMemory[mProgStart + 5] = (entry / 1000) % 10 + '0';
	mMemory[mProgStart + 6] = (entry / 100) % 10 + '0';
	mMemory[mProgStart + 7] = (entry / 10) % 10 + '0';
	mMemory[mProgStart + 8] = entry % 10 + '0';
}

bool ByteCodeGenerator::WritePRGFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		mMemory[mProgStart - 2] = mProgStart & 0xff;
		mMemory[mProgStart - 1] = mProgStart >> 8;

		int	done = fwrite(mMemory  + mProgStart - 2, 1, mProgEnd - mProgStart + 2, file);
		fclose(file);
		return done == mProgEnd - mProgStart + 2;
	}
	else
		return false;
}

void ByteCodeGenerator::WriteAsmFile(FILE* file)
{
	for (int i = 0; i < mGlobalAddr.Size(); i++)
	{
		if (mGlobalAddr[i].mAssembler)
		{
			fprintf(file, "--------------------------------------------------------------------\n");
			if (mGlobalAddr[i].mIdent)
				fprintf(file, "%s:\n", mGlobalAddr[i].mIdent->mString);

			int		ip = mGlobalAddr[i].mAddress;
			while (ip < mGlobalAddr[i].mAddress + mGlobalAddr[i].mSize)
			{
				int			iip = ip;
				uint8	opcode = mMemory[ip++];
				AsmInsData	d = DecInsData[opcode];
				int	addr = 0;

				switch (d.mMode)
				{
				case ASMIM_IMPLIED:
					fprintf(file, "%04x : %04x %02x __ __ %s\n", iip, ip, mMemory[ip], AsmInstructionNames[d.mType]);
					break;
				case ASMIM_IMMEDIATE:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s #$%02x\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_ZERO_PAGE:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s $%02x\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_ZERO_PAGE_X:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s $%02x,x\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_ZERO_PAGE_Y:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s $%02x,y\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_ABSOLUTE:
					addr = mMemory[ip] + 256 * mMemory[ip + 1];
					fprintf(file, "%04x : %04x %02x %02x %02x %s $%04x\n", iip, ip, mMemory[ip], mMemory[ip + 1], mMemory[ip + 2], AsmInstructionNames[d.mType], addr);
					ip += 2;
					break;
				case ASMIM_ABSOLUTE_X:
					addr = mMemory[ip] + 256 * mMemory[ip + 1];
					fprintf(file, "%04x : %04x %02x %02x %02x %s $%04x,x\n", iip, ip, mMemory[ip], mMemory[ip + 1], mMemory[ip + 2], AsmInstructionNames[d.mType], addr);
					ip += 2;
					break;
				case ASMIM_ABSOLUTE_Y:
					addr = mMemory[ip] + 256 * mMemory[ip + 1];
					fprintf(file, "%04x : %04x %02x %02x %02x %s $%04x,y\n", iip, ip, mMemory[ip], mMemory[ip + 1], mMemory[ip + 2], AsmInstructionNames[d.mType], addr);
					ip += 2;
					break;
				case ASMIM_INDIRECT:
					addr = mMemory[ip] + 256 * mMemory[ip + 1];
					ip += 2;
					fprintf(file, "%04x : %04x %02x %02x %02x %s ($%04x)\n", iip, ip, mMemory[ip], mMemory[ip + 1], mMemory[ip + 2], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_INDIRECT_X:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s ($%02x,x)\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_INDIRECT_Y:
					addr = mMemory[ip++];
					fprintf(file, "%04x : %04x %02x %02x __ %s ($%02x),y\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				case ASMIM_RELATIVE:
					addr = mMemory[ip++];
					if (addr & 0x80)
						addr = addr + ip - 256;
					else
						addr = addr + ip;
					fprintf(file, "%04x : %04x %02x %02x __ %s $%02x\n", iip, ip, mMemory[ip], mMemory[ip + 1], AsmInstructionNames[d.mType], addr);
					break;
				}
			}
		}
	}
}

bool ByteCodeGenerator::WriteMapFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		for (int i = 0; i < mProcedureAddr.Size(); i++)
		{
			if (mProcedureAddr[i].mIdent)
				fprintf(file, "%04x - %04x : F %s\n", mProcedureAddr[i].mAddress, mProcedureAddr[i].mAddress + mProcedureAddr[i].mSize, mProcedureAddr[i].mIdent->mString);
			else
				fprintf(file, "%04x - %04x : F\n", mProcedureAddr[i].mAddress, mProcedureAddr[i].mAddress + mProcedureAddr[i].mSize);
		}

		for (int i = 0; i < mGlobalAddr.Size(); i++)
		{
			if (mGlobalAddr[i].mAssembler)
			{
				if (mGlobalAddr[i].mIdent)
					fprintf(file, "%04x - %04x : A %s\n", mGlobalAddr[i].mAddress, mGlobalAddr[i].mAddress + mGlobalAddr[i].mSize, mGlobalAddr[i].mIdent->mString);
				else
					fprintf(file, "%04x - %04x : A\n", mGlobalAddr[i].mAddress, mGlobalAddr[i].mAddress + mGlobalAddr[i].mSize);
			}
			else
			{
				if (mGlobalAddr[i].mIdent)
					fprintf(file, "%04x - %04x : V %s\n", mGlobalAddr[i].mAddress, mGlobalAddr[i].mAddress + mGlobalAddr[i].mSize, mGlobalAddr[i].mIdent->mString);
				else
					fprintf(file, "%04x - %04x : V\n", mGlobalAddr[i].mAddress, mGlobalAddr[i].mAddress + mGlobalAddr[i].mSize);
			}
		}

		return true;
	}
	else
		return false;
}


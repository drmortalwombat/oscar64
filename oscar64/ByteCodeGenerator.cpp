#include "ByteCodeGenerator.h"
#include "Assembler.h"

static const uint32 LIVE_ACCU = 0x00000001;


static const char* ByteCodeNames[] = {
	"NOP",
	"EXIT",

	"CONST_8",
	"CONST_P8",
	"CONST_N8",
	"CONST_16",
	"CONST_32",

	"LOAD_REG_8",
	"STORE_REG_8",
	"LOAD_REG_16",
	"STORE_REG_16",
	"ADDR_REG",
	"LOAD_REG_32",
	"STORE_REG_32",

	"LOAD_ABS_8",
	"LOAD_ABS_U8",
	"LOAD_ABS_16",
	"LOAD_ABS_32",

	"STORE_ABS_8",
	"STORE_ABS_16",
	"STORE_ABS_32",

	"LEA_ABS",
	"LEA_ABS_INDEX",
	"LEA_ABS_INDEX_U8",
	"LEA_ACCU_INDEX",

	"LOAD_LOCAL_8",
	"LOAD_LOCAL_U8",
	"LOAD_LOCAL_16",
	"LOAD_LOCAL_32",

	"STORE_LOCAL_8",
	"STORE_LOCAL_16",
	"STORE_LOCAL_32",

	"LEA_LOCAL",

	"STORE_FRAME_8",
	"STORE_FRAME_16",
	"STORE_FRAME_32",

	"LEA_FRAME",

	"LOAD_ADDR_8",
	"LOAD_ADDR_U8",
	"LOAD_ADDR_16",
	"LOAD_ADDR_32",

	"STORE_ADDR_8",
	"STORE_ADDR_16",
	"STORE_ADDR_32",

	"BINOP_ADDR_16",
	"BINOP_SUBR_16",
	"BINOP_ANDR_16",
	"BINOP_ORR_16",
	"BINOP_XORR_16",
	"BINOP_MULR_16",
	"BINOP_DIVR_U16",
	"BINOP_MODR_U16",
	"BINOP_DIVR_I16",
	"BINOP_MODR_I16",
	"BINOP_SHLR_16",
	"BINOP_SHRR_U16",
	"BINOP_SHRR_I16",

	"BINOP_ADDA_16",

	"BINOP_ADDI_16",
	"BINOP_SUBI_16",
	"BINOP_ANDI_16",
	"BINOP_ORI_16",
	"BINOP_MULI8_16",

	"BINOP_ADDI_8",
	"BINOP_ANDI_8",
	"BINOP_ORI_8",

	"BINOP_SHLI_16",
	"BINOP_SHRI_U16",
	"BINOP_SHRI_I16",

	"BINOP_CMPUR_16",
	"BINOP_CMPSR_16",

	"BINOP_CMPUI_16",
	"BINOP_CMPSI_16",

	"BINOP_CMPUR_8",
	"BINOP_CMPSR_8",

	"BINOP_CMPUI_8",
	"BINOP_CMPSI_8",

	"OP_NEGATE_16",
	"OP_INVERT_16",

	"BINOP_ADD_F32",
	"BINOP_SUB_F32",
	"BINOP_MUL_F32",
	"BINOP_DIV_F32",
	"BINOP_CMP_F32",
	"OP_NEGATE_F32",
	"OP_ABS_F32",
	"OP_FLOOR_F32",
	"OP_CEIL_F32",

	"CONV_U16_F32",
	"CONV_I16_F32",
	"CONV_F32_U16",
	"CONV_F32_I16",

	"CONV_I8_I16",

	"JUMPS",
	"BRANCHS_EQ",
	"BRANCHS_NE",
	"BRANCHS_GT",
	"BRANCHS_GE",
	"BRANCHS_LT",
	"BRANCHS_LE",

	"JUMPF",
	"BRANCHF_EQ",
	"BRANCHF_NE",
	"BRANCHF_GT",
	"BRANCHF_GE",
	"BRANCHF_LT",
	"BRANCHF_LE",

	"SET_EQ",
	"SET_NE",
	"SET_GT",
	"SET_GE",
	"SET_LT",
	"SET_LE",

	"JSR",		//113

	nullptr,
	nullptr,
	nullptr,

	"NATIVE",	//117

	"ENTER",
	"RETURN",
	"CALL_ADDR",
	"CALL_ABS",
	"PUSH_FRAME",
	"POP_FRAME",

	"COPY",
	"COPY_LONG",
	"STRCPY",

	"EXTRT"
};


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

bool ByteCodeInstruction::UsesAccu(void) const
{
	return UsesRegister(BC_REG_ACCU);
}

bool ByteCodeInstruction::ChangesAddr(void) const
{
	return ChangesRegister(BC_REG_ADDR);
}

bool ByteCodeInstruction::UsesAddr(void) const
{
	return UsesRegister(BC_REG_ADDR);
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

	if (mCode == BC_BINOP_ADD_L32 || mCode == BC_BINOP_AND_L32 || mCode == BC_BINOP_OR_L32 || mCode == BC_BINOP_XOR_L32 || mCode == BC_BINOP_MUL_L32)
		return true;

	return false;
}

bool ByteCodeInstruction::IsLocalStore(void) const
{
	return mCode >= BC_STORE_LOCAL_8 && mCode <= BC_STORE_LOCAL_32;
}

bool ByteCodeInstruction::IsLocalLoad(void) const
{
	return mCode >= BC_LOAD_LOCAL_8 && mCode <= BC_LOAD_LOCAL_32 || mCode == BC_COPY || mCode == BC_STRCPY;
}

bool ByteCodeInstruction::IsLocalAccess(void) const
{
	return IsLocalStore() || IsLocalLoad();
}

bool ByteCodeInstruction::IsShiftByRegister(void) const
{
	return
		mCode == BC_BINOP_SHLR_16 || mCode == BC_BINOP_SHRR_I16 || mCode == BC_BINOP_SHRR_U16 ||
		mCode == BC_BINOP_SHL_L32 || mCode == BC_BINOP_SHR_U32 || mCode == BC_BINOP_SHR_I32;
}


bool ByteCodeInstruction::IsIntegerConst(void) const
{
	return mCode >= BC_CONST_8 && mCode <= BC_CONST_32;
}

bool ByteCodeInstruction::CheckAccuSize(uint32 & used)
{
	bool	changed = false;

	switch (mCode)
	{
	case BC_CONST_8:
	case BC_CONST_P8:
	case BC_CONST_N8:
	case BC_CONST_16:
	case BC_CONST_32:
		if (mRegister == BC_REG_ACCU)
			used = 0;
		break;

	case BC_LOAD_REG_8:
		if (mRegister != BC_REG_ACCU)
			used = 0;
		else if (!(used & 0xffffff00))
		{
			mCode = BC_NOP;
			changed = true;
		}
		else
			used = 0x000000ff;
		break;

	case BC_LOAD_REG_16:
		if (!(used & 0xffffff00))
		{
			mCode = BC_LOAD_REG_8;
			changed = true;
		}
		used = 0;
		break;

	case BC_LOAD_REG_32:
		if (!(used & 0xffffff00))
		{
			mCode = BC_LOAD_REG_8;
			changed = true;
		}
		else if (!(used & 0xffff0000))
		{
			mCode = BC_LOAD_REG_16;
			changed = true;
		}
		used = 0;
		break;

	case BC_STORE_REG_8:
		used |= 0x000000ff;
		break;

	case BC_STORE_ABS_8:
	case BC_STORE_LOCAL_8:
	case BC_STORE_FRAME_8:
	case BC_STORE_ADDR_8:
		if (mRegister == BC_REG_ACCU)
			used |= 0x000000ff;
		break;

	case BC_STORE_REG_16:
		used |= 0x0000ffff;
		break;

	case BC_STORE_ABS_16:
	case BC_STORE_LOCAL_16:
	case BC_STORE_FRAME_16:
	case BC_STORE_ADDR_16:
		if (mRegister == BC_REG_ACCU)
			used |= 0x0000ffff;
		break;

	case BC_STORE_REG_32:
		used = 0xffffffff;
		break;

	case BC_STORE_ABS_32:
	case BC_STORE_LOCAL_32:
	case BC_STORE_FRAME_32:
	case BC_STORE_ADDR_32:
		if (mRegister == BC_REG_ACCU)
			used = 0xffffffff;
		break;

	case BC_LOAD_ABS_8:
	case BC_LOAD_ABS_U8:
		if (mRegister == BC_REG_ACCU)
			used = 0;
		break;
	case BC_LOAD_ABS_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_ABS_8;
				changed = true;
			}
			used = 0;
		}
		break;
	case BC_LOAD_ABS_32:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_ABS_8;
				changed = true;
			}
			else if (!(used & 0xffff0000))
			{
				mCode = BC_LOAD_ABS_16;
				changed = true;
			}
			used = 0;
		}
		break;

	case BC_LEA_ABS:
	case BC_LEA_LOCAL:
	case BC_LEA_FRAME:
		if (mRegister == BC_REG_ACCU)
			used = 0;
		break;

	case BC_LEA_ABS_INDEX:
	case BC_LEA_ABS_INDEX_U8:
	case BC_ADDR_REG:
		if (mRegister == BC_REG_ACCU)
			used |= 0x0000ffff;
		break;

	case BC_LEA_ACCU_INDEX:
		used |= 0x0000ffff;
		break;

	case BC_LOAD_LOCAL_8:
	case BC_LOAD_LOCAL_U8:
		if (mRegister == BC_REG_ACCU)
			used = 0;
		break;
	case BC_LOAD_LOCAL_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_LOCAL_8;
				changed = true;
			}
			used = 0;
		}
		break;
	case BC_LOAD_LOCAL_32:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_LOCAL_8;
				changed = true;
			}
			else if (!(used & 0xffff0000))
			{
				mCode = BC_LOAD_LOCAL_16;
				changed = true;
			}
			used = 0;
		}
		break;

	case BC_LOAD_ADDR_8:
	case BC_LOAD_ADDR_U8:
		if (mRegister == BC_REG_ACCU)
			used = 0;
		break;
	case BC_LOAD_ADDR_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_ADDR_8;
				changed = true;
			}
			used = 0;
		}
		break;
	case BC_LOAD_ADDR_32:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_LOAD_ADDR_8;
				changed = true;
			}
			else if (!(used & 0xffff0000))
			{
				mCode = BC_LOAD_ADDR_16;
				changed = true;
			}
			used = 0;
		}
		break;

	case BC_BINOP_ADDR_16:
	case BC_BINOP_SUBR_16:
	case BC_BINOP_ANDR_16:
	case BC_BINOP_ORR_16:
	case BC_BINOP_XORR_16:
	case BC_BINOP_MULR_16:
	case BC_BINOP_SUBI_16:
	case BC_BINOP_MULI8_16:
	case BC_BINOP_SHLR_16:
	case BC_BINOP_ADDI_8:
	case BC_BINOP_ANDI_8:
	case BC_BINOP_ORI_8:
	case BC_BINOP_SHLI_16:
	case BC_BINOP_CMPUR_16:
	case BC_BINOP_CMPSR_16:
	case BC_BINOP_CMPUI_16:
	case BC_BINOP_CMPSI_16:
	case BC_OP_NEGATE_16:
	case BC_OP_INVERT_16:
		break;

	case BC_BINOP_DIVR_U16:
	case BC_BINOP_MODR_U16:
	case BC_BINOP_DIVR_I16:
	case BC_BINOP_MODR_I16:
	case BC_BINOP_SHRR_U16:
	case BC_BINOP_SHRR_I16:
	case BC_BINOP_SHRI_U16:
	case BC_BINOP_SHRI_I16:
		used |= 0x0000ffff;
		break;

	case BC_BINOP_ADDA_16:
		used |= 0x0000ffff;
		break;

	case BC_BINOP_ADDI_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_BINOP_ADDI_8;
				changed = true;
			}
		}
		break;
	case BC_BINOP_ANDI_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_BINOP_ANDI_8;
				changed = true;
			}
		}
		break;
	case BC_BINOP_ORI_16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_BINOP_ORI_8;
				changed = true;
			}
		}
		break;

	case BC_BINOP_CMPUR_8:
	case BC_BINOP_CMPSR_8:
	case BC_BINOP_CMPUI_8:
	case BC_BINOP_CMPSI_8:
		used = 0x000000ff;
		break;

	case BC_BINOP_ADD_F32:
	case BC_BINOP_SUB_F32:
	case BC_BINOP_MUL_F32:
	case BC_BINOP_DIV_F32:
	case BC_BINOP_CMP_F32:
	case BC_OP_NEGATE_F32:
	case BC_OP_ABS_F32:
	case BC_OP_FLOOR_F32:
	case BC_OP_CEIL_F32:
	case BC_CONV_F32_U16:
	case BC_CONV_F32_I16:
		used = 0xffffffff;
		break;

	case BC_CONV_U16_F32:
	case BC_CONV_I16_F32:
	case BC_BRANCHS_EQ:
	case BC_BRANCHS_NE:
	case BC_BRANCHS_GT:
	case BC_BRANCHS_GE:
	case BC_BRANCHS_LT:
	case BC_BRANCHS_LE:
	case BC_BRANCHF_EQ:
	case BC_BRANCHF_NE:
	case BC_BRANCHF_GT:
	case BC_BRANCHF_GE:
	case BC_BRANCHF_LT:
	case BC_BRANCHF_LE:
	case BC_SET_EQ:
	case BC_SET_NE:
	case BC_SET_GT:
	case BC_SET_GE:
	case BC_SET_LT:
	case BC_SET_LE:
	case BC_CONV_I16_I32:
	case BC_CONV_U16_U32:
		used = 0x0000ffff;
		break;

	case BC_CONV_I8_I16:
		if (mRegister == BC_REG_ACCU)
		{
			if (!(used & 0xffffff00))
			{
				mCode = BC_NOP;
				changed = true;
			}
			else
				used = 0x000000ff;
		}
		break;

	case BC_BINOP_ADD_L32:
	case BC_BINOP_SUB_L32:
	case BC_BINOP_AND_L32:
	case BC_BINOP_OR_L32:
	case BC_BINOP_XOR_L32:
	case BC_BINOP_MUL_L32:
	case BC_BINOP_DIV_U32:
	case BC_BINOP_MOD_U32:
	case BC_BINOP_DIV_I32:
	case BC_BINOP_MOD_I32:
	case BC_BINOP_SHL_L32:
	case BC_BINOP_SHR_U32:
	case BC_BINOP_SHR_I32:
	case BC_BINOP_CMP_U32:
	case BC_BINOP_CMP_S32:
		used = 0xffffffff;
		break;
		
	case BC_RETURN:
		used = 0xffffffff;
		break;

	case BC_COPY:
	case BC_COPY_LONG:
	case BC_STRCPY:
		used = 0x0000ffff;
		break;

	default:
		break;
	}

	return changed;
}


bool ByteCodeInstruction::IsSame(const ByteCodeInstruction& ins) const
{
	if (mCode == ins.mCode && mValue == ins.mValue && mRegister == ins.mRegister && mLinkerObject == ins.mLinkerObject)
	{
		return true;
	}

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

bool ByteCodeInstruction::UsesRegister(uint32 reg) const
{
	if (StoresRegister(reg))
		return true;

	if (mRegister == reg)
	{
		if (mCode == BC_LOAD_REG_8 || mCode == BC_LOAD_REG_16 || mCode == BC_LOAD_REG_32 || mCode == BC_ADDR_REG || mCode == BC_LEA_ABS_INDEX || mCode == BC_LEA_ABS_INDEX_U8 || mCode == BC_LEA_ACCU_INDEX)
			return true;

		if (mCode >= BC_BINOP_ADDI_16 && mCode <= BC_BINOP_ORI_8)
			return true;

		if (mCode >= BC_BINOP_ADDR_16 && mCode <= BC_BINOP_SHRR_I16)
			return true;

		if (mCode == BC_BINOP_CMPUR_16 || mCode == BC_BINOP_CMPSR_16)
			return true;

		if (mCode == BC_BINOP_CMPUR_8 || mCode == BC_BINOP_CMPSR_8)
			return true;

		if (mCode >= BC_BINOP_ADD_F32 && mCode <= BC_BINOP_CMP_F32)
			return true;

		if (mCode >= BC_BINOP_ADD_L32 && mCode <= BC_BINOP_CMP_S32)
			return true;

		if (mCode == BC_BINOP_ADDA_16)
			return true;
	}

	if (reg == BC_REG_ACCU)
	{
		if (mCode == BC_STORE_REG_8 || mCode == BC_STORE_REG_16 || mCode == BC_STORE_REG_32)
			return true;

		if (mCode >= BC_BINOP_ADDR_16 && mCode <= BC_BINOP_SHRR_I16)
			return true;
		if (mCode >= BC_BINOP_CMPUR_16 && mCode <= BC_BINOP_CMPSI_16)
			return true;
		if (mCode >= BC_BINOP_CMPUR_8 && mCode <= BC_BINOP_CMPSI_8)
			return true;
		if (mCode >= BC_OP_NEGATE_16 && mCode <= BC_OP_INVERT_16)
			return true;
		if (mCode >= BC_BINOP_ADD_F32 && mCode <= BC_OP_CEIL_F32)
			return true;
		if (mCode >= BC_CONV_U16_F32 && mCode <= BC_CONV_F32_I16)
			return true;
		if (mCode >= BC_BINOP_SHLI_16 && mCode <= BC_BINOP_SHRI_I16)
			return true;
		if (mCode >= BC_SET_EQ && mCode <= BC_SET_LE)
			return true;
		if (mCode >= BC_CONV_I16_I32 && mCode <= BC_BINOP_CMP_S32)
			return true;
		if (mCode == BC_LEA_ACCU_INDEX)
			return true;
		if (mCode == BC_COPY || mCode == BC_STRCPY)
			return true;
		if (mCode == BC_BINOP_ADDA_16)
			return true;
	}

	if (reg == BC_REG_ADDR)
	{
		if (mCode >= BC_LOAD_ADDR_8 && mCode <= BC_STORE_ADDR_32)
			return true;

		if (mCode == BC_COPY || mCode == BC_STRCPY)
			return true;

		if (mCode == BC_JSR || mCode == BC_CALL_ADDR || mCode == BC_CALL_ABS)
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
		if (mCode == BC_BINOP_ADDA_16)
			return true;
	}

	if (reg >= BC_REG_WORK && reg < BC_REG_FPARAMS_END || reg >= BC_REG_TMP && reg < BC_REG_TMP_SAVED)
	{
		if (mCode == BC_JSR || mCode == BC_CALL_ADDR || mCode == BC_CALL_ABS)
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
		if (mCode >= BC_BINOP_CMPUR_8 && mCode <= BC_BINOP_CMPSI_8)
			return true;
		if (mCode >= BC_OP_NEGATE_16 && mCode <= BC_OP_INVERT_16)
			return true;
		if (mCode >= BC_BINOP_ADD_F32 && mCode <= BC_OP_CEIL_F32)
			return true;
		if (mCode >= BC_CONV_U16_F32 && mCode <= BC_CONV_F32_I16)
			return true;
		if (mCode >= BC_BINOP_SHLI_16 && mCode <= BC_BINOP_SHRI_I16)
			return true;
		if (mCode >= BC_SET_EQ && mCode <= BC_SET_LE)
			return true;
		if (mCode == BC_JSR || mCode == BC_CALL_ADDR || mCode == BC_CALL_ABS)
			return true;
		if (mCode >= BC_CONV_I16_I32 && mCode <= BC_BINOP_CMP_S32)
			return true;
	}

	if (reg == BC_REG_ADDR)
	{
		if (mCode == BC_ADDR_REG)
			return true;
		if (mCode >= BC_LOAD_ABS_8 && mCode <= BC_STORE_ABS_32)
			return true;
		if (mCode == BC_JSR || mCode == BC_CALL_ADDR || mCode == BC_CALL_ABS)
			return true;
		if (mCode == BC_LEA_ABS_INDEX || mCode == BC_LEA_ABS_INDEX_U8 || mCode == BC_LEA_ACCU_INDEX)
			return true;
	}

	if (reg == BC_REG_WORK)
	{
		if (mCode == BC_JSR || mCode == BC_CALL_ADDR || mCode == BC_CALL_ABS)
			return true;

		if (mCode == BC_BINOP_DIVR_I16 || mCode == BC_BINOP_DIVR_U16 || mCode == BC_BINOP_MODR_I16 || mCode == BC_BINOP_MODR_U16 ||
			mCode == BC_BINOP_DIV_I32 || mCode == BC_BINOP_DIV_U32 || mCode == BC_BINOP_MOD_I32 || mCode == BC_BINOP_MOD_U32 ||
			mCode == BC_BINOP_MULR_16 || mCode == BC_BINOP_MULI8_16 || mCode == BC_BINOP_MUL_L32)
			return true;

		if (mCode >= BC_BINOP_ADD_F32 && mCode <= BC_OP_CEIL_F32)
			return true;
		if (mCode >= BC_CONV_U16_F32 && mCode <= BC_CONV_F32_I16)
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
			rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
			rl.mRefObject = mLinkerObject;
			rl.mRefOffset = mValue;
			block->mRelocations.Push(rl);

			block->PutWord(0);
		}
		else if (mValue >= 0 && mValue < 256)
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
			rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
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
	case BC_LEA_ABS_INDEX:
	case BC_LEA_ABS_INDEX_U8:
		block->PutCode(generator, mCode); block->PutByte(mRegister);
		if (mRelocate)
		{
			LinkerReference	rl;
			rl.mOffset = block->mCode.Size();
			rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
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

	case BC_LEA_ACCU_INDEX:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
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
	case BC_BINOP_ADDA_16:
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
	case BC_BINOP_CMPUR_8:
	case BC_BINOP_CMPSR_8:
		block->PutCode(generator, mCode);
		block->PutByte(mRegister);
		break;

	case BC_BINOP_CMPUI_16:
	case BC_BINOP_CMPSI_16:
		block->PutCode(generator, mCode);
		block->PutWord(uint16(mValue));
		break;

	case BC_BINOP_CMPUI_8:
	case BC_BINOP_CMPSI_8:
		block->PutCode(generator, mCode);
		block->PutByte(uint8(mValue));
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

	case BC_CALL_ADDR:
		block->PutCode(generator, mCode);
		break;

	case BC_CALL_ABS:
	{
		block->PutCode(generator, mCode);
		
		LinkerReference	rl;
		rl.mOffset = block->mCode.Size();
		rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
		rl.mRefObject = mLinkerObject;
		rl.mRefOffset = 0;
		block->mRelocations.Push(rl);

		block->PutWord(0);
	}	break;

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
		rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
		rl.mRefObject = mLinkerObject;
		rl.mRefOffset = 0;
		block->mRelocations.Push(rl);

		block->PutWord(0);
	}	break;

	case BC_LOAD_ADDR_8:
	case BC_LOAD_ADDR_U8:
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

	case BC_STRCPY:
		block->PutCode(generator, BC_STRCPY);
		break;

	case BC_CONV_I16_I32:
	case BC_CONV_U16_U32:
	case BC_OP_NEGATE_32:
	case BC_OP_INVERT_32:
	case BC_BINOP_ADD_L32:
	case BC_BINOP_SUB_L32:
	case BC_BINOP_AND_L32:
	case BC_BINOP_OR_L32:
	case BC_BINOP_XOR_L32:
	case BC_BINOP_MUL_L32:
	case BC_BINOP_DIV_U32:
	case BC_BINOP_MOD_U32:
	case BC_BINOP_DIV_I32:
	case BC_BINOP_MOD_I32:
	case BC_BINOP_SHL_L32:
	case BC_BINOP_SHR_U32:
	case BC_BINOP_SHR_I32:
	case BC_BINOP_CMP_U32:
	case BC_BINOP_CMP_S32:
		{
		block->PutCode(generator, BC_EXTRT);

		LinkerReference	rl;
		rl.mOffset = block->mCode.Size();
		rl.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
		rl.mRefObject = generator->mExtByteCodes[mCode - 128];
		rl.mRefOffset = 0;
		block->mRelocations.Push(rl);
		block->PutWord(0);
		block->PutByte(mRegister);
		break;
	}

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
	generator->mByteCodeUsed[code]++;
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
	: mRelocations({ 0 }), mIns(ByteCodeInstruction(BC_NOP)), mEntryBlocks(nullptr)
{
	mTrueJump = mFalseJump = NULL;
	mTrueLink = mFalseLink = NULL;
	mOffset = 0x7fffffff;
	mCopied = false;
	mAssembled = false;
	mKnownShortBranch = false;
	mBypassed = false;
	mExitLive = 0;
}

void ByteCodeBasicBlock::IntConstToAccu(int64 val)
{
	ByteCodeInstruction	ins(BC_CONST_16);
	ins.mRegister = BC_REG_ACCU;
	ins.mValue = int(val);
	mIns.Push(ins);
}

void ByteCodeBasicBlock::LongConstToAccu(int64 val)
{
	ByteCodeInstruction	bins(BC_CONST_32);
	bins.mRegister = BC_REG_ACCU;
	bins.mValue = int(val);
	mIns.Push(bins);
}

void ByteCodeBasicBlock::LongConstToWork(int64 val)
{
	ByteCodeInstruction	bins(BC_CONST_32);
	bins.mRegister = BC_REG_WORK;
	bins.mValue = int(val);
	mIns.Push(bins);
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
	if (ins->mDst.mType == IT_FLOAT)
	{
		union { float f; int v; } cc;
		cc.f = ins->mConst.mFloatConst;
		ByteCodeInstruction	bins(BC_CONST_32);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		bins.mValue = cc.v;
		mIns.Push(bins);
	}
	else if (ins->mDst.mType == IT_POINTER)
	{
		if (ins->mConst.mMemory == IM_GLOBAL)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mLinkerObject = ins->mConst.mLinkerObject;
			bins.mValue = ins->mConst.mIntConst;
			bins.mRelocate = true;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_ABSOLUTE)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = ins->mConst.mIntConst;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_LOCAL)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = ins->mConst.mIntConst + proc->mLocalVars[ins->mConst.mVarIndex]->mOffset;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_PARAM)
		{
			ByteCodeInstruction	bins(BC_LEA_LOCAL);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = ins->mConst.mIntConst + ins->mConst.mVarIndex + proc->mLocalSize + 2;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_FPARAM)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = BC_REG_FPARAMS + ins->mConst.mIntConst + ins->mConst.mVarIndex;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_FRAME)
		{
			ByteCodeInstruction	bins(BC_LEA_FRAME);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = ins->mConst.mVarIndex + ins->mSrc[1].mIntConst + 2;
			mIns.Push(bins);
		}
		else if (ins->mConst.mMemory == IM_PROCEDURE)
		{
			ByteCodeInstruction	bins(BC_CONST_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mLinkerObject = ins->mConst.mLinkerObject;
			bins.mValue = 0;
			bins.mRelocate = true;
			mIns.Push(bins);
		}
	}
	else if (ins->mDst.mType == IT_BOOL || ins->mDst.mType == IT_INT8)
	{
		ByteCodeInstruction	bins(BC_CONST_8);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		bins.mValue = ins->mConst.mIntConst;
		mIns.Push(bins);
	}
	else if (ins->mDst.mType == IT_INT32)
	{
		ByteCodeInstruction	bins(BC_CONST_32);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		bins.mValue = ins->mConst.mIntConst;
		mIns.Push(bins);
	}
	else
	{
		ByteCodeInstruction	bins(BC_CONST_16);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		bins.mValue = ins->mConst.mIntConst;
		mIns.Push(bins);
	}

}

void ByteCodeBasicBlock::CopyValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	ByteCodeInstruction	sins(BC_ADDR_REG);
	sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
	sins.mRegisterFinal = ins->mSrc[1].mFinal;
	mIns.Push(sins);
	ByteCodeInstruction	dins(BC_LOAD_REG_16);
	dins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
	dins.mRegisterFinal = ins->mSrc[0].mFinal;
	mIns.Push(dins);
	ByteCodeInstruction	cins(BC_COPY);
	cins.mValue = ins->mConst.mOperandSize;
	mIns.Push(cins);
}

void ByteCodeBasicBlock::StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins)
{
	ByteCodeInstruction	sins(BC_ADDR_REG);
	sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
	sins.mRegisterFinal = ins->mSrc[1].mFinal;
	mIns.Push(sins);
	ByteCodeInstruction	dins(BC_LOAD_REG_16);
	dins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
	dins.mRegisterFinal = ins->mSrc[0].mFinal;
	mIns.Push(dins);
	ByteCodeInstruction	cins(BC_STRCPY);
	mIns.Push(cins);
}

void ByteCodeBasicBlock::StoreDirectValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSrc[0].mType == IT_FLOAT)
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				FloatConstToAccu(ins->mSrc[0].mFloatConst);

				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	bins(BC_STORE_REG_32);
					bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
						bins.mRegister = BC_REG_ACCU;
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_ACCU;
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_32);
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	ains(BC_LOAD_REG_32);
					ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					mIns.Push(ains);

					ByteCodeInstruction	bins(BC_STORE_REG_32);
					bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				FloatConstToAccu(ins->mSrc[0].mFloatConst);

				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + 4 <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = index;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + 4 <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					ByteCodeInstruction	bins(BC_STORE_ADDR_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					bins.mValue = index;
					mIns.Push(bins);
				}
			}
		}
	}
	else if (ins->mSrc[0].mType == IT_POINTER)
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[0].mIntConst);

				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_ACCU;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	bins(BC_STORE_REG_16);
					bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

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
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_STORE_ABS_16);
					bins.mValue = ins->mSrc[1].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	ains(BC_LOAD_REG_16);
					ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					ains.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(ains);

					ByteCodeInstruction	bins(BC_STORE_REG_16);
					bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
					mIns.Push(bins);
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					ByteCodeInstruction	bins(BC_STORE_FRAME_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[0].mIntConst);

				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + 2 <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = index;
					mIns.Push(bins);
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + 2 <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					ByteCodeInstruction	bins(BC_STORE_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					bins.mValue = index;
					mIns.Push(bins);
				}
			}
		}
	}
	else
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (InterTypeSize[ins->mSrc[0].mType] <= 2)
					IntConstToAccu(ins->mSrc[0].mIntConst);
				else
					LongConstToAccu(ins->mSrc[0].mIntConst);

				if (InterTypeSize[ins->mSrc[0].mType] == 1)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	bins(BC_STORE_REG_8);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

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
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 2)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	bins(BC_STORE_REG_16);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

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
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 4)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_32);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_32);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_ACCU;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	bins(BC_STORE_REG_32);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

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
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_32);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (InterTypeSize[ins->mSrc[0].mType] == 1)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_8);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	ains(BC_LOAD_REG_8);
						ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						ains.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(ains);

						ByteCodeInstruction	bins(BC_STORE_REG_8);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

						if (index <= 255)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_8);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 2)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_16);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	ains(BC_LOAD_REG_16);
						ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						ains.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(ains);

						ByteCodeInstruction	bins(BC_STORE_REG_16);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

						if (index <= 254)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_16);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 4)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_32);
						bins.mRelocate = true;
						bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						ByteCodeInstruction	bins(BC_STORE_ABS_32);
						bins.mValue = ins->mSrc[1].mIntConst;
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						ByteCodeInstruction	ains(BC_LOAD_REG_32);
						ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						ains.mRegisterFinal = ins->mSrc[0].mFinal;
						mIns.Push(ains);

						ByteCodeInstruction	bins(BC_STORE_REG_32);
						bins.mRegister = BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst;
						mIns.Push(bins);
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;

						if (index <= 252)
						{
							ByteCodeInstruction	bins(BC_STORE_LOCAL_32);
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
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
							bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
							bins.mRegisterFinal = ins->mSrc[0].mFinal;
							bins.mValue = 0;
							mIns.Push(bins);
						}
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						ByteCodeInstruction	bins(BC_STORE_FRAME_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (InterTypeSize[ins->mSrc[0].mType] <= 2)
					IntConstToAccu(ins->mSrc[0].mIntConst);
				else
					LongConstToAccu(ins->mSrc[0].mIntConst);

				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + InterTypeSize[ins->mSrc[0].mType] <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					if (InterTypeSize[ins->mSrc[0].mType] == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = index;
						mIns.Push(bins);
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = index;
						mIns.Push(bins);
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 4)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_32);
						bins.mRegister = BC_REG_ACCU;
						bins.mValue = index;
						mIns.Push(bins);
					}
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	index = ins->mSrc[1].mIntConst;
					if (index >= 0 && index + InterTypeSize[ins->mSrc[0].mType] <= 256)
					{
						ByteCodeInstruction	lins(BC_ADDR_REG);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						mIns.Push(lins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
						lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						lins.mRegisterFinal = ins->mSrc[1].mFinal;
						lins.mValue = index;
						index = 0;
						mIns.Push(lins);
					}

					if (InterTypeSize[ins->mSrc[0].mType] == 1)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = index;
						mIns.Push(bins);
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 2)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = index;
						mIns.Push(bins);
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 4)
					{
						ByteCodeInstruction	bins(BC_STORE_ADDR_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = index;
						mIns.Push(bins);
					}
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadDirectValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mDst.mType == IT_FLOAT)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mMemory == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mRelocate = true;
				bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
				bins.mValue = ins->mSrc[0].mIntConst;
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_32);
				bins.mValue = ins->mSrc[0].mIntConst;
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_FPARAM)
			{
				ByteCodeInstruction	ains(BC_LOAD_REG_32);
				ains.mRegister = BC_REG_FPARAMS + ins->mSrc[0].mIntConst + ins->mSrc[0].mVarIndex;
				mIns.Push(ains);

				ByteCodeInstruction	bins(BC_STORE_REG_32);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
			{
				int	index = ins->mSrc[0].mIntConst;
				if (ins->mSrc[0].mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
				else
					index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = 0;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	index = ins->mSrc[0].mIntConst;
				if (index >= 0 && index + 4 <= 256)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
				}
				else
				{
					ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					lins.mValue = index;
					index = 0;
					mIns.Push(lins);
				}

				ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				bins.mValue = index;
				mIns.Push(bins);
			}
		}
	}
	else if (ins->mDst.mType == IT_POINTER)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mMemory == IM_GLOBAL)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mRelocate = true;
				bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
				bins.mValue = ins->mSrc[0].mIntConst;
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
			{
				ByteCodeInstruction	bins(BC_LOAD_ABS_16);
				bins.mValue = ins->mSrc[0].mIntConst;
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_FPARAM)
			{
				ByteCodeInstruction	ains(BC_LOAD_REG_16);
				ains.mRegister = BC_REG_FPARAMS + ins->mSrc[0].mIntConst + ins->mSrc[0].mVarIndex;
				mIns.Push(ains);

				ByteCodeInstruction	bins(BC_STORE_REG_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
			{
				int	index = ins->mSrc[0].mIntConst;
				if (ins->mSrc[0].mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
				else
					index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;

				if (index <= 254)
				{
					ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = 0;
					mIns.Push(bins);
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	index = ins->mSrc[0].mIntConst;
				if (index >= 0 && index + 2 <= 256)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
				}
				else
				{
					ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					lins.mValue = index;
					index = 0;
					mIns.Push(lins);
				}

				ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
				bins.mValue = index;
				mIns.Push(bins);
			}
		}
	}
	else
	{
		if (ins->mSrc[0].mTemp < 0)
		{ 
			if (InterTypeSize[ins->mDst.mType] == 1)
			{
				if (ins->mSrc[0].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_8);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_8);
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	ains(BC_LOAD_REG_8);
					ains.mRegister = BC_REG_FPARAMS + ins->mSrc[0].mIntConst + ins->mSrc[0].mVarIndex;
					mIns.Push(ains);

					ByteCodeInstruction	bins(BC_STORE_REG_8);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;

					if (index <= 255)
					{
						ByteCodeInstruction	bins(BC_LOAD_LOCAL_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
						bins.mValue = index;
						mIns.Push(bins);
					}
					else
					{
						ByteCodeInstruction	lins(BC_LEA_LOCAL);
						lins.mRegister = BC_REG_ADDR;
						lins.mValue = index;
						mIns.Push(lins);
						ByteCodeInstruction	bins(BC_LOAD_ADDR_8);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
			}
			else if (InterTypeSize[ins->mDst.mType] == 2)
			{
				if (ins->mSrc[0].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_16);
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	ains(BC_LOAD_REG_16);
					ains.mRegister = BC_REG_FPARAMS + ins->mSrc[0].mIntConst + ins->mSrc[0].mVarIndex;
					mIns.Push(ains);

					ByteCodeInstruction	bins(BC_STORE_REG_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;

					if (index <= 254)
					{
						ByteCodeInstruction	bins(BC_LOAD_LOCAL_16);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
			}
			else if (InterTypeSize[ins->mDst.mType] == 4)
			{
				if (ins->mSrc[0].mMemory == IM_GLOBAL)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_32);
					bins.mRelocate = true;
					bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					ByteCodeInstruction	bins(BC_LOAD_ABS_32);
					bins.mValue = ins->mSrc[0].mIntConst;
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					ByteCodeInstruction	ains(BC_LOAD_REG_32);
					ains.mRegister = BC_REG_FPARAMS + ins->mSrc[0].mIntConst + ins->mSrc[0].mVarIndex;
					mIns.Push(ains);

					ByteCodeInstruction	bins(BC_STORE_REG_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(bins);
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;

					if (index <= 252)
					{
						ByteCodeInstruction	bins(BC_LOAD_LOCAL_32);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
						bins.mValue = 0;
						mIns.Push(bins);
					}
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	index = ins->mSrc[0].mIntConst;
				if (index >= 0 && index + InterTypeSize[ins->mDst.mType] <= 256)
				{
					ByteCodeInstruction	lins(BC_ADDR_REG);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
				}
				else
				{
					ByteCodeInstruction	lins(BC_LEA_ABS_INDEX);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					lins.mValue = index;
					index = 0;
					mIns.Push(lins);
				}

				if (InterTypeSize[ins->mDst.mType] == 1)
				{
					ByteCodeInstruction	bins(BC_LOAD_ADDR_8);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = index;
					mIns.Push(bins);
				}
				else if (InterTypeSize[ins->mDst.mType] == 2)
				{
					ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = index;
					mIns.Push(bins);
				}
				else if (InterTypeSize[ins->mDst.mType] == 4)
				{
					ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = index;
					mIns.Push(bins);
				}
			}
		}
	}
}

void ByteCodeBasicBlock::LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins)
{
	int	ri = rins->mSrc[0].mIntConst, wi = wins->mSrc[1].mIntConst;

	if (ri < 252 && wi < 252 && rins->mSrc[0].mTemp == wins->mSrc[1].mTemp && rins->mSrc[0].mMemory == IM_INDIRECT && wins->mSrc[1].mMemory == IM_INDIRECT)
	{
		ByteCodeInstruction	lins(BC_ADDR_REG);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp];
		lins.mRegisterFinal = wins->mSrc[1].mFinal;
		mIns.Push(lins);

		if (InterTypeSize[rins->mDst.mType] == 1)
		{
			ByteCodeInstruction	bins(BC_LOAD_ADDR_8);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[rins->mDst.mTemp];
			bins.mValue = ri;
			mIns.Push(bins);
		}
		else if (InterTypeSize[rins->mDst.mType] == 2)
		{
			ByteCodeInstruction	bins(BC_LOAD_ADDR_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[rins->mDst.mTemp];
			bins.mValue = ri;
			mIns.Push(bins);
		}
		else if (InterTypeSize[rins->mDst.mType] == 4)
		{
			ByteCodeInstruction	bins(BC_LOAD_ADDR_32);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[rins->mDst.mTemp];
			bins.mValue = ri;
			mIns.Push(bins);
		}

		if (InterTypeSize[wins->mSrc[0].mType] == 1)
		{
			ByteCodeInstruction	bins(BC_STORE_ADDR_8);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp];
			bins.mValue = wi;
			mIns.Push(bins);
		}
		else if (InterTypeSize[wins->mSrc[0].mType] == 2)
		{
			ByteCodeInstruction	bins(BC_STORE_ADDR_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp];
			bins.mValue = wi;
			mIns.Push(bins);
		}
		else if (InterTypeSize[wins->mSrc[0].mType] == 4)
		{
			ByteCodeInstruction	bins(BC_STORE_ADDR_32);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp];
			bins.mValue = wi;
			mIns.Push(bins);
		}
	}
	else
	{
		LoadDirectValue(proc, rins);
		StoreDirectValue(proc, wins);
	}
}

void ByteCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSrc[0].mTemp < 0)
	{
		if (ins->mSrc[1].mTemp == ins->mDst.mTemp)
		{
			ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			bins.mValue = ins->mSrc[0].mIntConst;
			mIns.Push(bins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
			lins.mRegisterFinal = ins->mSrc[1].mFinal;
			mIns.Push(lins);
			ByteCodeInstruction	ains(BC_BINOP_ADDI_16);
			ains.mRegister = BC_REG_ACCU;
			ains.mValue = ins->mSrc[0].mIntConst;
			mIns.Push(ains);
			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			mIns.Push(sins);
		}
	}
	else if (ins->mSrc[1].mTemp < 0)
	{
		if (ins->mSrc[1].mMemory == IM_GLOBAL)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_ACCU;
			bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
			bins.mValue = ins->mSrc[1].mIntConst;
			bins.mRelocate = true;
			mIns.Push(bins);
		}
		else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
		{
			ByteCodeInstruction	bins(BC_LEA_ABS);
			bins.mRegister = BC_REG_ACCU;
			bins.mValue = ins->mSrc[1].mIntConst;
			mIns.Push(bins);
		}
		else if (ins->mSrc[1].mMemory == IM_FPARAM)
		{
			ByteCodeInstruction	bins(BC_CONST_16);
			bins.mRegister = BC_REG_ACCU;
			bins.mValue = BC_REG_FPARAMS + ins->mSrc[1].mIntConst + ins->mSrc[1].mVarIndex;
			mIns.Push(bins);
		}
		else if (ins->mSrc[1].mMemory == IM_PROCEDURE)
		{
			ByteCodeInstruction	bins(BC_CONST_16);
			bins.mRegister = BC_REG_ACCU;
			bins.mLinkerObject = ins->mSrc[1].mLinkerObject;
			bins.mValue = 0;
			bins.mRelocate = true;
			mIns.Push(bins);
		}

		ByteCodeInstruction	ains(BC_BINOP_ADDR_16);
		ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		ains.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(ains);
		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
		lins.mRegisterFinal = ins->mSrc[1].mFinal;
		mIns.Push(lins);
		ByteCodeInstruction	ains(BC_BINOP_ADDR_16);
		ains.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		ains.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(ains);
		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
}
void ByteCodeBasicBlock::CallFunction(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSrc[0].mTemp < 0)
	{
		ByteCodeInstruction	bins(BC_CALL_ABS);
		bins.mRelocate = true;
		bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
		bins.mValue = 0;
		mIns.Push(bins);
	}
	else
	{
		ByteCodeInstruction	bins(BC_ADDR_REG);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		bins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(bins);
		ByteCodeInstruction	cins(BC_CALL_ADDR);
		mIns.Push(cins);
	}

	if (ins->mDst.mTemp >= 0)
	{
		ByteCodeInstruction	bins(StoreTypedTmpCodes[ins->mDst.mType]);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(bins);
	}
}

void ByteCodeBasicBlock::CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSrc[0].mTemp < 0)
	{
		ByteCodeInstruction	bins(BC_JSR);
		bins.mRelocate = true;
		bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
		bins.mValue = ins->mSrc[0].mIntConst;
		for (int i = 1; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp < 0)
			{
				if (ins->mSrc[i].mMemory == IM_FPARAM)
					ins->mSrc[0].mLinkerObject->mTemporaries[i - 1] = BC_REG_FPARAMS + ins->mSrc[i].mVarIndex + ins->mSrc[i].mIntConst;
			}
			else
				ins->mSrc[0].mLinkerObject->mTemporaries[i - 1] = BC_REG_TMP + proc->mTempOffset[ins->mSrc[i].mTemp];
		}
		mIns.Push(bins);
	}

	if (ins->mDst.mTemp >= 0)
	{
		ByteCodeInstruction	bins(StoreTypedTmpCodes[ins->mDst.mType]);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(bins);
	}
}

void ByteCodeBasicBlock::CallNative(InterCodeProcedure* proc, const InterInstruction* ins)
{
	if (ins->mSrc[0].mTemp < 0)
	{
		ByteCodeInstruction	bins(BC_JSR);
		bins.mRelocate = true;
		bins.mLinkerObject = ins->mSrc[0].mLinkerObject;
		bins.mValue = ins->mSrc[0].mIntConst;
		mIns.Push(bins);
	}

	if (ins->mDst.mTemp >= 0)
	{
		ByteCodeInstruction	bins(StoreTypedTmpCodes[ins->mDst.mType]);
		bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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

	if (ins->mSrc[0].mType == IT_FLOAT)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			FloatConstToAccu(ins->mSrc[0].mFloatConst);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
		}

		ByteCodeInstruction	cins(BC_BINOP_CMP_F32);

		if (ins->mSrc[1].mTemp < 0)
		{
			FloatConstToWork(ins->mSrc[1].mFloatConst);
			cins.mRegister = BC_REG_WORK;
		}
		else
		{
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
		}

		cins.mRegisterFinal = ins->mSrc[1].mFinal;
		mIns.Push(cins);
	}
	else if (ins->mSrc[0].mType == IT_INT32)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			LongConstToAccu(ins->mSrc[0].mIntConst);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
		}


		ByteCodeInstruction	cins(BC_BINOP_CMP_U32);
		if (csigned)
			cins.mCode = BC_BINOP_CMP_S32;

		if (ins->mSrc[1].mTemp < 0)
		{
			LongConstToWork(ins->mSrc[1].mIntConst);
			cins.mRegister = BC_REG_WORK;
		}
		else
		{
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
		}

		cins.mRegisterFinal = ins->mSrc[1].mFinal;
		mIns.Push(cins);
	}
	else if (ins->mSrc[0].mType == IT_INT8)
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_8);
				cins.mValue = ins->mSrc[1].mIntConst;
				mIns.Push(cins);
			}
			else
			{
				if (ins->mSrc[1].mIntConst == 0)
				{
					switch (ins->mOperator)
					{
					case IA_CMPEQ:
					case IA_CMPGEU:
						return BC_BRANCHS_EQ;
					case IA_CMPNE:
					case IA_CMPLU:
						return BC_BRANCHS_NE;
					case IA_CMPLEU:
						return BC_JUMPS;
					case IA_CMPGU:
						return BC_NOP;
					}
				}
				else
				{
					ByteCodeInstruction	cins(BC_BINOP_CMPUI_8);
					cins.mValue = ins->mSrc[1].mIntConst;
					mIns.Push(cins);
				}
			}
		}
		else if (ins->mSrc[0].mTemp < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
			lins.mRegisterFinal = ins->mSrc[1].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				if (ins->mSrc[0].mIntConst == 0)
				{
					ByteCodeInstruction	cins(BC_CONV_I8_I16);
					cins.mRegister = BC_REG_ACCU;
					mIns.Push(cins);

					switch (ins->mOperator)
					{
					case IA_CMPEQ:
						return BC_BRANCHS_EQ;
					case IA_CMPNE:
						return BC_BRANCHS_NE;
					case IA_CMPLES:
						return BC_BRANCHS_LE;
					case IA_CMPGS:
						return BC_BRANCHS_GT;
					case IA_CMPGES:
						return BC_BRANCHS_GE;
					case IA_CMPLS:
						return BC_BRANCHS_LT;
					}
				}
				else
				{
					ByteCodeInstruction	cins(BC_BINOP_CMPSI_8);
					cins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(cins);
				}
			}
			else
			{
				if (ins->mSrc[0].mIntConst == 0)
				{
					switch (ins->mOperator)
					{
					case IA_CMPEQ:
					case IA_CMPLEU:
						return BC_BRANCHS_EQ;
					case IA_CMPNE:
					case IA_CMPGU:
						return BC_BRANCHS_NE;
					case IA_CMPGEU:
						return BC_JUMPS;
					case IA_CMPLU:
						return BC_NOP;
					}
				}
				else
				{
					ByteCodeInstruction	cins(BC_BINOP_CMPUI_8);
					cins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(cins);
				}
			}
			code = TransposeBranchCondition(code);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSR_8);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				cins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUR_8);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				cins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(cins);
			}
		}
	}
	else
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
				cins.mValue = ins->mSrc[1].mIntConst;
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
				cins.mValue = ins->mSrc[1].mIntConst;
				mIns.Push(cins);
			}
		}
		else if (ins->mSrc[0].mTemp < 0)
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
			lins.mRegisterFinal = ins->mSrc[1].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				if (ins->mSrc[0].mIntConst == 0)
				{
					switch (ins->mOperator)
					{
					case IA_CMPEQ:
						return BC_BRANCHS_EQ;
					case IA_CMPNE:
						return BC_BRANCHS_NE;
					case IA_CMPLES:
						return BC_BRANCHS_LE;
					case IA_CMPGS:
						return BC_BRANCHS_GT;
					case IA_CMPGES:
						return BC_BRANCHS_GE;
					case IA_CMPLS:
						return BC_BRANCHS_LT;
					}
				}
				else
				{
					ByteCodeInstruction	cins(BC_BINOP_CMPSI_16);
					cins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(cins);
				}
			}
			else
			{
				if (ins->mSrc[0].mIntConst == 0)
				{
					switch (ins->mOperator)
					{
					case IA_CMPEQ:
					case IA_CMPLEU:
						return BC_BRANCHS_EQ;
					case IA_CMPNE:
					case IA_CMPGU:
						return BC_BRANCHS_NE;
					case IA_CMPGEU:
						return BC_JUMPS;
					case IA_CMPLU:
						return BC_NOP;
					}
				}
				else
				{
					ByteCodeInstruction	cins(BC_BINOP_CMPUI_16);
					cins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(cins);
				}
			}
			code = TransposeBranchCondition(code);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_16);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);
			if (csigned)
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPSR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				cins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(cins);
			}
			else
			{
				ByteCodeInstruction	cins(BC_BINOP_CMPUR_16);
				cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				cins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(cins);
			}
		}
	}

	return code;
}

static ByteCode ByteCodeBinRegOperator(const InterInstruction * ins)
{
	if (ins->mDst.mType == IT_FLOAT)
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
	else if (ins->mDst.mType == IT_INT32)
	{
		switch (ins->mOperator)
		{
		case IA_ADD: return BC_BINOP_ADD_L32;
		case IA_SUB: return BC_BINOP_SUB_L32;
		case IA_MUL: return BC_BINOP_MUL_L32;
		case IA_DIVU: return BC_BINOP_DIV_U32;
		case IA_MODU: return BC_BINOP_MOD_U32;
		case IA_DIVS: return BC_BINOP_DIV_I32;
		case IA_MODS: return BC_BINOP_MOD_I32;
		case IA_AND: return BC_BINOP_AND_L32;
		case IA_OR: return BC_BINOP_OR_L32;
		case IA_XOR: return BC_BINOP_XOR_L32;

		case IA_SHL: return BC_BINOP_SHL_L32;
		case IA_SHR: return BC_BINOP_SHR_U32;
		case IA_SAR: return BC_BINOP_SHR_I32;

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
	case IA_ADD: return InterTypeSize[ins->mDst.mType] == 1 ? BC_BINOP_ADDI_8 : BC_BINOP_ADDI_16;
	case IA_SUB: return BC_BINOP_SUBI_16;
	case IA_AND: return InterTypeSize[ins->mDst.mType] == 1 ? BC_BINOP_ANDI_8 : BC_BINOP_ANDI_16;
	case IA_OR:  return InterTypeSize[ins->mDst.mType] == 1 ? BC_BINOP_ORI_8 : BC_BINOP_ORI_16;
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
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(lins);

		ByteCodeInstruction	bins(BC_CONV_F32_I16);
//		ByteCodeInstruction	bins(ins->mDst.mType == IT_SIGNED ? BC_CONV_F32_I16 : BC_CONV_F32_U16);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);

	}	break;
	case IA_INT2FLOAT:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(lins);

		ByteCodeInstruction	bins(BC_CONV_I16_F32);
//		ByteCodeInstruction	bins(ins.mSType[0] == IT_SIGNED ? BC_CONV_I16_F32 : BC_CONV_U16_F32);
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);

	} break;
	case IA_EXT8TO16S:
	{
		if (ins->mSrc[0].mTemp == ins->mDst.mTemp)
		{
			ByteCodeInstruction	cins(BC_CONV_I8_I16);
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			cins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(cins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);

			ByteCodeInstruction	cins(BC_CONV_I8_I16);
			cins.mRegister = BC_REG_ACCU;
			mIns.Push(cins);

			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			mIns.Push(sins);
		}
	}	break;
	case IA_EXT8TO16U:
	{
		if (ins->mSrc[0].mTemp == ins->mDst.mTemp)
		{
			ByteCodeInstruction	cins(BC_BINOP_ANDI_16);
			cins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			cins.mRegisterFinal = ins->mSrc[0].mFinal;
			cins.mValue = 0x00ff;
			mIns.Push(cins);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_8);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
			lins.mRegisterFinal = ins->mSrc[0].mFinal;
			mIns.Push(lins);

			ByteCodeInstruction	sins(BC_STORE_REG_16);
			sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
			mIns.Push(sins);
		}
	}	break;
	case IA_EXT16TO32S:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(lins);

		ByteCodeInstruction	cins(BC_CONV_I16_I32);
		cins.mRegister = 0;
		mIns.Push(cins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}	break;

	case IA_EXT16TO32U:
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(lins);

		ByteCodeInstruction	cins(BC_CONV_U16_U32);
		cins.mRuntime = "lextu16";
		cins.mRegister = 0;
		mIns.Push(cins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}	break;
	}
}

void ByteCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mDst.mType == IT_FLOAT)
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
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
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
	else if (ins->mDst.mType == IT_INT32)
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_32);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(lins);

		switch (ins->mOperator)
		{
		case IA_NEG:
		{
			ByteCodeInstruction	oins(BC_OP_NEGATE_32);
			mIns.Push(oins);
		}	break;
		case IA_NOT:
		{
			ByteCodeInstruction	oins(BC_OP_INVERT_32);
			mIns.Push(oins);
		}	break;
		}

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
	else
	{
		ByteCodeInstruction	lins(BC_LOAD_REG_16);
		lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
		lins.mRegisterFinal = ins->mSrc[0].mFinal;
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
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
}

void ByteCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mDst.mType == IT_FLOAT)
	{
		ByteCode	bc = ByteCodeBinRegOperator(ins);

		if (ins->mSrc[1].mTemp < 0)
		{
			FloatConstToAccu(ins->mSrc[1].mFloatConst);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
			lins.mRegisterFinal = ins->mSrc[1].mFinal;
			mIns.Push(lins);
		}

		ByteCodeInstruction	bins(bc);

		if (ins->mSrc[0].mTemp < 0)
		{
			FloatConstToWork(ins->mSrc[0].mFloatConst);
			bins.mRegister = BC_REG_WORK;
		}
		else if (ins->mSrc[1].mTemp == ins->mSrc[0].mTemp)
			bins.mRegister = BC_REG_ACCU;
		else
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];

		bins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
		mIns.Push(sins);
	}
	else if (ins->mDst.mType == IT_INT32)
	{
		ByteCode	bc = ByteCodeBinRegOperator(ins);

		if (ins->mSrc[1].mTemp < 0)
		{
			LongConstToAccu(ins->mSrc[1].mIntConst);
		}
		else
		{
			ByteCodeInstruction	lins(BC_LOAD_REG_32);
			lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
			lins.mRegisterFinal = ins->mSrc[1].mFinal;
			mIns.Push(lins);
		}

		ByteCodeInstruction	bins(bc);

		if (ins->mSrc[0].mTemp < 0)
		{
			LongConstToWork(ins->mSrc[0].mIntConst);
			bins.mRegister = BC_REG_WORK;
		}
		else if (ins->mSrc[1].mTemp == ins->mSrc[0].mTemp)
			bins.mRegister = BC_REG_ACCU;
		else
			bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];

		bins.mRegisterFinal = ins->mSrc[0].mFinal;
		mIns.Push(bins);

		ByteCodeInstruction	sins(BC_STORE_REG_32);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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

			if (ins->mSrc[1].mTemp < 0)
			{
				if (ins->mSrc[0].mTemp == ins->mDst.mTemp)
				{
					ByteCodeInstruction	bins(bcis);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = ins->mSrc[1].mIntConst;
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[0].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSrc[1].mIntConst;
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[1].mTemp == ins->mDst.mTemp)
				{
					ByteCodeInstruction	bins(bcis);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(bins);
					return;
				}

				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[1].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(lins);

				ByteCodeInstruction	bins(bci);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSrc[0].mIntConst;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[1].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
		}
			break;
		case IA_SUB:
			if (ins->mSrc[1].mTemp < 0)
			{
				if (ins->mSrc[0].mTemp == ins->mDst.mTemp)
				{
					ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = ins->mSrc[1].mIntConst;
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[1].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = ins->mSrc[1].mIntConst;
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[1].mTemp == ins->mDst.mTemp)
				{
					ByteCodeInstruction	bins(InterTypeSize[ins->mSrc[0].mType] == 1 ? BC_BINOP_ADDI_8 : BC_BINOP_ADDI_16);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
					bins.mValue = - ins->mSrc[0].mIntConst;
					mIns.Push(bins);
					return;
				}
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[0].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_ADDI_16);
				bins.mRegister = BC_REG_ACCU;
				bins.mValue = - ins->mSrc[0].mIntConst;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[1].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);;
				mIns.Push(lins);

				ByteCodeInstruction	bins(BC_BINOP_SUBR_16);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
			break;
		case IA_MUL:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			ByteCode	bci = ByteCodeBinImmOperator(ins);
			if (ins->mSrc[1].mTemp < 0)
			{
				if (ins->mSrc[1].mIntConst >= 0 && ins->mSrc[1].mIntConst <= 255)
				{
					if (ins->mSrc[0].mTemp == ins->mDst.mTemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
						bins.mRegisterFinal = ins->mSrc[0].mFinal;
						bins.mValue = ins->mSrc[1].mIntConst;
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSrc[1].mIntConst;
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins->mSrc[1].mIntConst);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
					bins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(bins);
				}
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[0].mIntConst >= 0 && ins->mSrc[0].mIntConst <= 255)
				{
					if (ins->mSrc[1].mTemp == ins->mDst.mTemp)
					{
						ByteCodeInstruction	bins(bci);
						bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						bins.mRegisterFinal = ins->mSrc[1].mFinal;
						bins.mValue = ins->mSrc[0].mIntConst;
						mIns.Push(bins);
						return;
					}

					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					lins.mRegisterFinal = ins->mSrc[1].mFinal;
					mIns.Push(lins);

					ByteCodeInstruction	bins(bci);
					bins.mRegister = BC_REG_ACCU;
					bins.mValue = ins->mSrc[0].mIntConst;
					mIns.Push(bins);
				}
				else
				{
					IntConstToAccu(ins->mSrc[0].mIntConst);
					ByteCodeInstruction	bins(bc);
					bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					bins.mRegisterFinal = ins->mSrc[1].mFinal;
					mIns.Push(bins);
				}
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);;
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
		} break;
		case IA_XOR:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins->mSrc[1].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[1].mIntConst);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[0].mIntConst);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				bins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);;
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
		} break;
		case IA_DIVS:
		case IA_MODS:
		case IA_DIVU:
		case IA_MODU:
		{
			ByteCode	bc = ByteCodeBinRegOperator(ins);
			if (ins->mSrc[1].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[1].mIntConst);
				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(lins);

				IntConstToAddr(ins->mSrc[0].mIntConst);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_ADDR;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);;
				mIns.Push(lins);

				ByteCodeInstruction	bins(bc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
		} break;

		case IA_SHL:
		case IA_SHR:
		case IA_SAR:
		{
			ByteCode	rbc = ByteCodeBinRegOperator(ins);

			ByteCode	ibc = ByteCodeBinImmOperator(ins);
			if (ins->mSrc[1].mTemp < 0)
			{
				IntConstToAccu(ins->mSrc[1].mIntConst);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal;
				mIns.Push(lins);

				ByteCodeInstruction	bins(ibc);
				bins.mValue = ins->mSrc[0].mIntConst;
				mIns.Push(bins);
			}
			else
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
				lins.mRegisterFinal = ins->mSrc[1].mFinal && (ins->mSrc[1].mTemp != ins->mSrc[0].mTemp);;
				mIns.Push(lins);

				ByteCodeInstruction	bins(rbc);
				bins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				bins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(bins);
			}

		} break;
		}

		ByteCodeInstruction	sins(InterTypeSize[ins->mSrc[1].mType] == 1 ? BC_STORE_REG_8 : BC_STORE_REG_16);
		sins.mRegister = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];
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
			if (i + 1 < sblock->mInstructions.Size() && sblock->mInstructions[i + 1]->mCode == IC_STORE && ins->mDst.mTemp == sblock->mInstructions[i + 1]->mSrc[0].mTemp)
			{
				LoadStoreIndirectValue(iproc, ins, sblock->mInstructions[i + 1]);
				i++;
			}
			else
				LoadDirectValue(iproc, ins);
			break;
		case IC_COPY:
			CopyValue(iproc, ins);
			break;
		case IC_STRCPY:
			StrcpyValue(iproc, ins);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			{
				switch (ins->mDst.mType)
				{
				case IT_BOOL:
				case IT_INT8:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_8);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_8);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(sins);
				} break;
				case IT_INT16:
				case IT_POINTER:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_16);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_16);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp];
					mIns.Push(sins);
				} break;
				case IT_INT32:
				case IT_FLOAT:
				{
					ByteCodeInstruction	lins(BC_LOAD_REG_32);
					lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
					lins.mRegisterFinal = ins->mSrc[0].mFinal;
					mIns.Push(lins);
					ByteCodeInstruction	sins(BC_STORE_REG_32);
					sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp];
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
		case IC_CALL_NATIVE:
			CallNative(iproc, ins);
			break;
		case IC_ASSEMBLER:
			CallAssembler(iproc, ins);
			break;
		case IC_PUSH_FRAME: 
		{
			ByteCodeInstruction	bins(BC_PUSH_FRAME);
			bins.mValue = ins->mConst.mIntConst + 2;
			mIns.Push(bins);

		}	break;
		case IC_POP_FRAME:
		{
			ByteCodeInstruction	bins(BC_POP_FRAME);
			bins.mValue = ins->mConst.mIntConst + 2;
			mIns.Push(bins);

		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (i + 1 < sblock->mInstructions.Size() && sblock->mInstructions[i + 1]->mCode == IC_BRANCH && sblock->mInstructions[i + 1]->mSrc[0].mFinal)
			{
				ByteCode code = RelationalOperator(iproc, ins);
				this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump), code);
				i++;
				return;
			}
			else
			{
				ByteCode code = RelationalOperator(iproc, ins);
				if (code == BC_JUMPS)
				{
					IntConstToAccu(1);
				}
				else if (code == BC_NOP)
				{
					IntConstToAccu(0);
				}
				else
				{
					ByteCodeInstruction	bins(ByteCode(code - BC_BRANCHS_EQ + BC_SET_EQ));
					mIns.Push(bins);
				}
				ByteCodeInstruction	sins(StoreTypedTmpCodes[ins->mDst.mType]);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(sins);
			}
			break;

		case IC_RETURN_VALUE:
			if (ins->mSrc[0].mTemp < 0)
				IntConstToAccu(ins->mSrc[0].mIntConst);
			else if (ins->mSrc[0].mType == IT_FLOAT || ins->mSrc[0].mType == IT_INT32)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_32);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[0].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);
			}

			mExitLive = LIVE_ACCU;

			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_RETURN:
			this->Close(proc->exitBlock, nullptr, BC_JUMPS);
			return;

		case IC_TYPECAST:
			if (ins->mSrc[0].mTemp >= 0 && ins->mDst.mTemp != ins->mSrc[0].mTemp)
			{
				ByteCodeInstruction	lins(BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);
				ByteCodeInstruction	sins(InterTypeSize[ins->mDst.mType] == 1 ? BC_STORE_REG_8 : BC_STORE_REG_16);
				sins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp];
				mIns.Push(sins);
			}
			break;

		case IC_BRANCH:
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[0].mIntConst == 0)
					this->Close(proc->CompileBlock(iproc, sblock->mFalseJump), nullptr, BC_JUMPS);
				else
					this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, BC_JUMPS);
			}
			else
			{
				ByteCodeInstruction	lins(InterTypeSize[ins->mSrc[0].mType] == 1 ? BC_LOAD_REG_8 : BC_LOAD_REG_16);
				lins.mRegister = BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp];
				lins.mRegisterFinal = ins->mSrc[0].mFinal;
				mIns.Push(lins);

				this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), proc->CompileBlock(iproc, sblock->mFalseJump), BC_BRANCHS_NE);
			}
			return;

		}

		i++;
	}

	this->Close(proc->CompileBlock(iproc, sblock->mTrueJump), nullptr, BC_JUMPS);
}

void ByteCodeBasicBlock::CollectEntryBlocks(ByteCodeBasicBlock* block)
{
	if (block)
		mEntryBlocks.Push(block);

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump)
			mTrueJump->CollectEntryBlocks(this);
		if (mFalseJump)
			mFalseJump->CollectEntryBlocks(this);
	}
}

bool ByteCodeBasicBlock::SameTail(ByteCodeInstruction& ins)
{
	if (mIns.Size() > 0)
		return mIns[mIns.Size() - 1].IsSame(ins);
	else
		return false;
}

bool ByteCodeBasicBlock::JoinTailCodeSequences(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mEntryBlocks.Size() > 1)
		{
			int i = 0;
			while (i < mEntryBlocks.Size() && mEntryBlocks[i]->mBranch == BC_JUMPS)
				i++;
			if (i == mEntryBlocks.Size())
			{
				ByteCodeBasicBlock* eb = mEntryBlocks[0];

				while (eb->mIns.Size() > 0)
				{
					ByteCodeInstruction& ins(eb->mIns[eb->mIns.Size() - 1]);
					i = 1;
					while (i < mEntryBlocks.Size() && mEntryBlocks[i]->SameTail(ins))
						i++;
					if (i == mEntryBlocks.Size())
					{
						mIns.Insert(0, ins);
						for (int i = 0; i < mEntryBlocks.Size(); i++)
						{
							ByteCodeBasicBlock* b = mEntryBlocks[i];
							b->mIns.SetSize(b->mIns.Size() - 1);
							if (mIns.Size() == 1)
								mExitLive |= b->mExitLive;
							b->mExitLive = LIVE_ACCU;
						}
						changed = true;
					}
					else
						break;
				}
			}
		}

		if (mTrueJump && mFalseJump && mTrueJump->mEntryBlocks.Size() == 1 && mFalseJump->mEntryBlocks.Size() == 1)
		{
			while (mTrueJump->mIns.Size() > 0 && mFalseJump->mIns.Size() > 0 && !mTrueJump->mIns[0].ChangesAccu() && mTrueJump->mIns[0].IsSame(mFalseJump->mIns[0]))
			{
				mIns.Push(mTrueJump->mIns[0]);
				mTrueJump->mIns.Remove(0);
				mFalseJump->mIns.Remove(0);
			}
		}

		if (mTrueJump && mTrueJump->JoinTailCodeSequences())
			changed = true;
		if (mFalseJump && mFalseJump->JoinTailCodeSequences())
			changed = true;
	}

	return changed;
}

bool ByteCodeBasicBlock::PropagateAccuCrossBorder(int accu, int addr)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;
		if (mEntryBlocks.Size() != 1)
			accu = addr = -1;

		int j = 0;
		for (int i = 0; i < mIns.Size(); i++)
		{
			if (mIns[i].mCode == BC_ADDR_REG && addr == mIns[i].mRegister ||
				mIns[i].mCode == BC_LOAD_REG_16 && accu == mIns[i].mRegister ||
				mIns[i].mCode == BC_STORE_REG_16 && accu == mIns[i].mRegister)
			{
				changed = true;
			}
			else
			{
				if (mIns[i].mCode == BC_ADDR_REG)
					addr = mIns[i].mRegister;
				else if (mIns[i].mCode == BC_LOAD_REG_16)
					accu = mIns[i].mRegister;
				else if (mIns[i].mCode == BC_STORE_REG_16)
				{
					accu = mIns[i].mRegister;
					if (addr == mIns[i].mRegister)
						addr = -1;
				}
				else
				{
					if (accu >= 0 && mIns[i].ChangesRegister(accu))
						accu = -1;
					if (addr >= 0 && mIns[i].ChangesRegister(addr))
						addr = -1;

					if (accu >= 0 && mIns[i].ChangesAccu())
						accu = -1;
					if (addr >= 0 && mIns[i].ChangesAddr())
						addr = -1;
				}

				if (i != j)
				{
					mIns[j] = mIns[i];
				}
				j++;
			}
		}
		mIns.SetSize(j);

		if (mTrueJump && mTrueJump->PropagateAccuCrossBorder(accu, addr))
			changed = true;
		if (mFalseJump && mFalseJump->PropagateAccuCrossBorder(accu, addr))
			changed = true;
	}

	return changed;
}


bool ByteCodeBasicBlock::PeepHoleOptimizer(int phase)
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

			// check reg addr up
			// 
#if 1
			for (int i = 2; i < mIns.Size(); i++)
			{
				if (mIns[i].mCode == BC_ADDR_REG && mIns[i].mRegister != BC_REG_ACCU && mIns[i].mRegisterFinal)
				{
					int j = i;
					while (j > 0 && !mIns[j - 1].ChangesAddr() && !mIns[j - 1].UsesAddr() && !mIns[j - 1].ChangesRegister(mIns[j].mRegister) && !mIns[j - 1].UsesRegister(mIns[j].mRegister))
					{
						ByteCodeInstruction	bins = mIns[j - 1];
						mIns[j - 1] = mIns[j];
						mIns[j] = bins;
						j--;
					}
				}
			}
#endif
#if 1
			if (phase == 2)
			{
				for (int i = 2; i < mIns.Size(); i++)
				{
					if (mIns[i].mCode >= BC_LOAD_ADDR_8 && mIns[i].mCode <= BC_STORE_ADDR_32)
					{
						int j = i;
						while (j > 0 && !mIns[j - 1].ChangesAddr() && !mIns[j - 1].ChangesRegister(mIns[j].mRegister) && !mIns[j - 1].UsesRegister(mIns[j].mRegister))
						{
							ByteCodeInstruction	bins = mIns[j - 1];
							mIns[j - 1] = mIns[j];
							mIns[j] = bins;
							j--;
						}
					}
				}
			}
#endif

			// mark accu live

			uint32	live = mExitLive;
			uint32	aused = mExitLive ? 0xffffffff : 0x00000000;

			if (mBranch != BC_JUMPS && mBranch != BC_NOP)
			{
				live |= LIVE_ACCU;
				aused = 0x0000ffff;
			}

			for (int i = mIns.Size() - 1; i >= 0; i--)
			{
				mIns[i].mLive = live;

				if (mIns[i].ChangesAccu())
					live &= ~LIVE_ACCU;
				if (mIns[i].UsesAccu())
					live |= LIVE_ACCU;
			}

//			assert(!(live & LIVE_ACCU));

			int	accuTemp = -1, addrTemp = -1, accuVal = 0, accuTempByte = -1;
			bool	accuConst = false;

			for (int i = 0; i < mIns.Size(); i++)
			{
				if (i + 4 < mIns.Size())
				{
#if 1
					if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 &&
						(mIns[i + 3].mCode == BC_BINOP_ADDR_16 || mIns[i + 3].mCode == BC_BINOP_SUBR_16) && mIns[i + 3].mRegister == mIns[i + 1].mRegister && mIns[i + 3].mRegisterFinal &&
						mIns[i + 4].mCode == BC_STORE_REG_8 && !(mIns[i + 4].mLive & LIVE_ACCU))
					{
						mIns[i + 3].mRegister = mIns[i + 0].mRegister;
						mIns[i + 3].mRegisterFinal = mIns[i + 0].mRegisterFinal;
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						!mIns[i + 2].ChangesAccu() && !mIns[i + 2].UsesAccu() && !mIns[i + 2].UsesRegister(mIns[i + 1].mRegister) && !mIns[i + 2].ChangesRegister(mIns[i + 0].mRegister) && !mIns[i + 2].ChangesRegister(mIns[i + 1].mRegister) &&
						!mIns[i + 3].ChangesAccu() && !mIns[i + 3].UsesAccu() && !mIns[i + 3].UsesRegister(mIns[i + 1].mRegister) && !mIns[i + 3].ChangesRegister(mIns[i + 0].mRegister) && !mIns[i + 3].ChangesRegister(mIns[i + 1].mRegister) &&
						mIns[i + 4].mCode == BC_ADDR_REG && mIns[i + 4].mRegister == mIns[i + 1].mRegister && mIns[i + 4].mRegisterFinal)
					{
						mIns[i + 4].mCode = BC_LEA_ACCU_INDEX;
						mIns[i + 4].mRegister = mIns[i + 0].mRegister;
						mIns[i + 4].mRegisterFinal = mIns[i + 0].mRegisterFinal;
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mLive |= LIVE_ACCU;
						mIns[i + 3].mLive |= LIVE_ACCU;
						progress = true;
					}
#endif
				}

#if 1
				if (i + 3 < mIns.Size())
				{
					if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].IsIntegerConst() && mIns[i + 2].mRegister == BC_REG_ACCU &&
						mIns[i + 3].IsShiftByRegister() && mIns[i + 3].mRegister == mIns[i + 1].mRegister && mIns[i + 3].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 3].mRegister = mIns[i + 0].mRegister;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_LEA_ABS && mIns[i + 1].mRegister != BC_REG_ACCU &&
						mIns[i + 2].mCode == BC_BINOP_ADDR_16 && mIns[i + 2].mRegister == mIns[i + 1].mRegister && mIns[i + 2].mRegisterFinal &&
						mIns[i + 3].mCode == BC_ADDR_REG && mIns[i + 3].mRegister == BC_REG_ACCU && !(mIns[i + 3].mLive & LIVE_ACCU))
					{
						mIns[i + 1].mCode = BC_LEA_ABS_INDEX_U8;
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						mIns[i + 1].mRegisterFinal = mIns[i + 0].mRegisterFinal;
						mIns[i + 1].mLive &= ~LIVE_ACCU;

						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_NOP;
						mIns[i + 3].mCode = BC_NOP;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAccu() && !mIns[i + 1].ChangesRegister(mIns[i + 0].mRegister) &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 &&
						mIns[i + 3].IsCommutative() && mIns[i + 3].mRegister == mIns[i + 0].mRegister)
					{
						mIns[i + 2].mCode = mIns[i + 3].mCode;
						mIns[i + 3].mCode = BC_NOP;
						if (mIns[i + 3].mRegisterFinal && !mIns[i + 1].UsesRegister(mIns[i + 0].mRegister))
							mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_BINOP_MULI8_16 && mIns[i + 1].mRegister == mIns[i + 0].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i + 2].mRegister != mIns[i + 0].mRegister &&
						mIns[i + 3].IsCommutative() && mIns[i + 3].mRegister == mIns[i + 0].mRegister && mIns[i + 3].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i + 1].mLive |= LIVE_ACCU;
						mIns[i + 2].mCode = BC_NOP;
						mIns[i + 3].mRegister = mIns[i + 2].mRegister;
						mIns[i + 3].mRegisterFinal = mIns[i + 2].mRegisterFinal;
						progress = true;
					}
#endif
				}
#endif
#if 1
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
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_8 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mRegister = BC_REG_ACCU;
						if (mIns[i + 2].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						(mIns[i + 1].mCode == BC_BINOP_ADDI_16 || mIns[i + 1].mCode == BC_BINOP_MULI8_16 || mIns[i + 1].mCode == BC_BINOP_ANDI_16 || mIns[i + 1].mCode == BC_BINOP_ORI_16) && mIns[i + 1].mRegister == mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = BC_REG_ACCU;
						if (mIns[i + 2].mRegisterFinal)
							mIns[i + 2].mCode = BC_NOP;
						else
							mIns[i + 2].mCode = BC_STORE_REG_16;

						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_32 && mIns[i].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						if (mIns[i + 2].mRegisterFinal)
							mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAccu() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].IsStore() && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i].mCode = BC_NOP;
						mIns[i + 2].mRegister = BC_REG_ACCU;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_16 &&
						!mIns[i + 1].ChangesAddr() && !mIns[i + 1].UsesAddr() && mIns[i + 1].mRegister != mIns[i].mRegister &&
						mIns[i + 2].mCode == BC_ADDR_REG && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i].mCode = BC_ADDR_REG;
						mIns[i].mRegister = BC_REG_ACCU;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 2].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 0].LoadsRegister(mIns[i + 2].mRegister) && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = mIns[i + 2].mCode;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 2].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister != mIns[i + 2].mRegister &&
						mIns[i + 0].mCode == BC_STORE_REG_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;;
						mIns[i + 1].mCode = mIns[i + 2].mCode;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
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
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_32 &&
						mIns[i + 1].LoadsRegister(BC_REG_ACCU) &&
						mIns[i + 2].IsCommutative() && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = mIns[i + 2].mRegister;
						progress = true;
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
						progress = true;
					}
					else if (mIns[i + 0].mCode == BC_STORE_REG_32 &&
						mIns[i + 2].mCode == BC_BINOP_CMP_F32 && mIns[i + 2].mRegister == mIns[i + 0].mRegister && mIns[i + 2].mRegisterFinal &&
						mIns[i + 1].LoadsRegister(BC_REG_ACCU) && i + 3 == mIns.Size())
					{
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						mIns[i + 0].mCode = BC_NOP;
						mBranch = TransposeBranchCondition(mBranch);
						progress = true;
					}
					else if (mIns[i + 0].mCode == BC_LOAD_REG_16 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_LOAD_REG_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_8 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 0].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 0].mCode == BC_CONST_16 && mIns[i + 2].mCode == BC_CONST_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && mIns[i + 0].mValue == mIns[i + 2].mValue && !mIns[i + 1].ChangesRegister(mIns[i + 0].mRegister))
					{
						if (mIns[i + 0].mRegister == BC_REG_ACCU)
							mIns[i + 1].mLive |= LIVE_ACCU;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 0].mCode >= BC_SET_EQ && mIns[i + 0].mCode <= BC_SET_LE &&
						mIns[i + 1].mCode == BC_STORE_REG_8 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == mIns[i + 2].mRegister)
					{
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 0].IsLocalStore() && mIns[i + 2].IsSame(mIns[i + 0]) && !mIns[i + 1].IsLocalAccess() && mIns[i + 1].mCode != BC_JSR)
					{
						mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_LOAD_LOCAL_16 && mIns[i + 1].mRegister == BC_REG_ACCU &&
						mIns[i + 2].IsCommutative() && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_8 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_8 &&
						mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 0].mRegister == mIns[i + 1].mRegister &&
						mIns[i + 2].mCode == BC_STORE_REG_8)
					{
						if (mIns[i + 1].mRegisterFinal)
							mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 0].mRegister == mIns[i + 1].mRegister &&
						mIns[i + 2].mCode == BC_STORE_REG_8)
					{
						if (mIns[i + 1].mRegisterFinal)
							mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].LoadsRegister(BC_REG_ACCU) &&
						mIns[i + 2].IsCommutative() && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 &&
						mIns[i + 2].mCode == BC_BINOP_SUBR_16 && mIns[i].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_OP_NEGATE_16;
						mIns[i + 1].mCode = BC_BINOP_ADDR_16;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mCode == BC_LEA_ABS && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 2].mCode == BC_ADDR_REG && mIns[i + 2].mRegister == BC_REG_ACCU && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_LEA_ABS_INDEX;
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 0].mRegisterFinal = mIns[i + 1].mRegisterFinal;
						mIns[i + 0].mLive &= ~LIVE_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_LEA_ABS_INDEX && mIns[i + 2].mRegister == mIns[i + 1].mRegister && !(mIns[i + 2].mLive & LIVE_ACCU) && mIns[i + 2].mRegisterFinal)
					 {
						 mIns[i + 2].mCode = BC_LEA_ABS_INDEX_U8;
						 mIns[i + 2].mRegister = mIns[i + 0].mRegister;
						 mIns[i + 2].mRegisterFinal = mIns[i + 0].mRegisterFinal;
						 mIns[i + 0].mCode = BC_NOP;
						 mIns[i + 1].mCode = BC_NOP;
						 progress = true;
					 }

					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						(mIns[i + 1].mCode == BC_BINOP_MULI8_16 || mIns[i + 1].mCode == BC_BINOP_ADDI_16) && mIns[i + 1].mRegister == mIns[i + 0].mRegister &&
						(mIns[i + 2].mCode == BC_STORE_ABS_16 || mIns[i + 2].mCode == BC_STORE_LOCAL_16 || mIns[i + 2].mCode == BC_STORE_FRAME_16 || mIns[i + 2].mCode == BC_STORE_ADDR_16) && mIns[i + 2].mRegister == mIns[i + 0].mRegister &&
						mIns[i + 2].mRegisterFinal && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i + 2].mRegister = BC_REG_ACCU;
						mIns[i + 1].mLive |= LIVE_ACCU;
						progress = true;
					}

					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_BINOP_ANDI_16 && mIns[i + 2].mRegister == mIns[i + 1].mRegister && mIns[i + 2].mValue == 0x00ff)
					{
						mIns[i + 2].mCode = BC_STORE_REG_16;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 0].mLive |= LIVE_ACCU;
						progress = true;
					}

					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_BINOP_ADDI_16 && mIns[i + 0].mRegister == mIns[i + 1].mRegister &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 2].mRegister == mIns[i + 0].mRegister && mIns[i + 2].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_LOAD_REG_8;
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 0].mLive |= LIVE_ACCU;

						mIns[i + 1].mCode = BC_BINOP_ADDI_8;
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i + 1].mLive |= LIVE_ACCU;

						mIns[i + 2].mCode = BC_NOP;

						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 && 
						mIns[i + 1].mCode == BC_BINOP_SHRI_U16 &&
						mIns[i + 2].mCode == BC_LOAD_REG_8 && mIns[i + 2].mRegister == BC_REG_ACCU)
					{
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LEA_ABS &&
						mIns[i + 1].mCode == BC_BINOP_ADDR_16 && mIns[i + 0].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal &&
						mIns[i + 2].mCode == BC_ADDR_REG && mIns[i + 2].mRegister == BC_REG_ACCU && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_LEA_ABS_INDEX;
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LEA_ABS && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_BINOP_ADDA_16 &&
						mIns[i + 2].mCode == BC_ADDR_REG && mIns[i + 1].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_LEA_ABS_INDEX;
						mIns[i + 0].mRegister = mIns[i + 2].mRegister;
						mIns[i + 0].mRegisterFinal = true;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_16 &&
						mIns[i + 1].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 2].mCode == BC_STORE_REG_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 0].mRegisterFinal = mIns[i + 1].mRegisterFinal;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_BINOP_ADDA_16;
						progress = true;
					}
					else if (
						(mIns[i + 0].mCode == BC_LOAD_LOCAL_16 || mIns[i + 0].mCode == BC_LOAD_ABS_16 || mIns[i + 0].mCode == BC_LOAD_ADDR_16 ||
						 mIns[i + 0].mCode == BC_LOAD_LOCAL_U8 || mIns[i + 0].mCode == BC_LOAD_ABS_U8 || mIns[i + 0].mCode == BC_LOAD_ADDR_U8) && 
						mIns[i + 1].mCode == BC_BINOP_ADDR_16 && mIns[i + 0].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal &&
						mIns[i + 2].mCode == BC_STORE_REG_16 && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mRegister = mIns[i + 2].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mCode = BC_BINOP_ADDA_16;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 2].mCode == BC_LEA_ABS_INDEX && mIns[i + 0].mRegister == mIns[i + 2].mRegister && mIns[i + 2].mRegisterFinal &&
						(mIns[i + 1].mCode == BC_BINOP_ADDI_16 || mIns[i + 1].mCode == BC_BINOP_ANDI_16 || mIns[i + 1].mCode == BC_BINOP_ORI_16 || mIns[i + 1].mCode == BC_BINOP_MULI8_16) &&
						mIns[i + 1].mRegister == mIns[i + 0].mRegister && !(mIns[i + 2].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i + 2].mRegister = BC_REG_ACCU;
						mIns[i + 1].mLive |= LIVE_ACCU;
						progress = true;
					}
#if 1
					else if (
						i + 3 == mIns.Size() && mFalseJump &&
						mIns[i + 0].mCode == BC_STORE_REG_8 &&
						mIns[i + 1].mCode == BC_LOAD_REG_8 &&
						mIns[i + 2].mCode == BC_BINOP_CMPUR_8 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && !(mExitLive & LIVE_ACCU) && mIns[i + 2].mRegisterFinal
						)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mRegister = mIns[i + 1].mRegister;
						mIns[i + 2].mRegisterFinal = mIns[i + 1].mRegisterFinal;
						mBranch = TransposeBranchCondition(mBranch);
						progress = true;
					}
					else if (
						i + 3 == mIns.Size() && mFalseJump &&
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 &&
						mIns[i + 2].mCode == BC_BINOP_CMPUR_16 && mIns[i + 0].mRegister == mIns[i + 2].mRegister && !(mExitLive & LIVE_ACCU) && mIns[i + 2].mRegisterFinal
						)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mCode = BC_NOP;
						mIns[i + 2].mRegister = mIns[i + 1].mRegister;
						mIns[i + 2].mRegisterFinal = mIns[i + 1].mRegisterFinal;
						mBranch = TransposeBranchCondition(mBranch);
						progress = true;
					}

#endif
					
				}
#endif
#if 1
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
					else if (mIns[i + 0].mCode == BC_LOAD_REG_16 && mIns[i + 1].mCode == BC_ADDR_REG && mIns[i + 1].mRegister == BC_REG_ACCU && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_ADDR_REG;
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
					else if (
						(mIns[i + 0].mCode == BC_LEA_FRAME || mIns[i + 0].mCode == BC_LEA_LOCAL || mIns[i + 0].mCode == BC_LEA_ABS) && 
						mIns[i + 1].mCode == BC_ADDR_REG && mIns[i + 0].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 0].mRegister = BC_REG_ADDR;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_BINOP_ADDI_16 && mIns[i + 1].mCode == BC_BINOP_ADDI_16 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 1].mValue += mIns[i].mValue;
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_BINOP_ADDI_16 && mIns[i].mValue == 0)
					{
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
#if 1
					else if (mIns[i].mCode == BC_STORE_REG_8 && mIns[i + 1].StoresRegister(mIns[i].mRegister) && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_STORE_REG_8 && mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_LOAD_REG_16 && mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == BC_REG_ACCU)
					{
						mIns[i].mCode = BC_LOAD_REG_8;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if ((mIns[i].mCode == BC_CONST_16 || mIns[i].mCode == BC_CONST_P8 || mIns[i].mCode == BC_CONST_N8) &&
							(mIns[i + 1].mCode == BC_CONST_16 || mIns[i + 1].mCode == BC_CONST_P8 || mIns[i + 1].mCode == BC_CONST_N8 || mIns[i + 1].mCode == BC_CONST_32) && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_CONST_32 && mIns[i + 1].mCode == BC_CONST_32 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i].mCode == BC_CONST_8 && mIns[i + 1].mCode == BC_CONST_8 && mIns[i].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i].mCode = BC_NOP;
						progress = true;
					}
#endif
#if 1
					else if (mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 0].mCode == BC_LOAD_ADDR_8 && mIns[i + 1].mRegisterFinal && mIns[i + 0].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 0].mCode = BC_LOAD_ADDR_U8;
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 0].mCode == BC_LOAD_ABS_8 && mIns[i + 1].mRegisterFinal && mIns[i + 0].mRegister == mIns[i + 1].mRegister)
					{
						mIns[i + 0].mCode = BC_LOAD_ABS_U8;
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_BINOP_ANDI_16 && mIns[i + 0].mValue == 0x00ff &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 1].mRegister == mIns[i + 0].mRegister)
					{
						mIns[i + 0].mCode = BC_LOAD_REG_8;
						mIns[i + 0].mLive |= LIVE_ACCU;

						if (!mIns[i + 1].mRegisterFinal)
							mIns[i + 1].mCode = BC_STORE_REG_16;
						else
							mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 && 
						mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == BC_REG_ACCU)
					{
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 && 
						mIns[i + 1].mCode == BC_BINOP_SHRI_I16)
					{
						mIns[i + 1].mCode = BC_BINOP_SHRI_U16;
						progress = true;
					}
					else if (
						(mIns[i + 0].mCode == BC_STORE_REG_16 || mIns[i + 0].mCode == BC_STORE_REG_8) && 
						mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == mIns[i + 0].mRegister)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						if (mIns[i + 1].mRegisterFinal)
							mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_STORE_REG_16 &&
						(mIns[i + 1].mCode == BC_STORE_LOCAL_16 || mIns[i + 1].mCode == BC_STORE_ABS_16 || mIns[i + 1].mCode == BC_STORE_ADDR_16 || mIns[i + 1].mCode == BC_STORE_FRAME_16) && 
						mIns[i + 1].mRegister == mIns[i + 0].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mRegister = BC_REG_ACCU;
						mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}

					if ((mIns[i].mCode == BC_LOAD_ABS_U8 || mIns[i].mCode == BC_LOAD_ADDR_U8 || mIns[i].mCode == BC_LOAD_ABS_16 || mIns[i].mCode == BC_LOAD_ADDR_16) && mIns[i].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_STORE_REG_16 && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#endif
					else if (
						mIns[i + 1].mCode == BC_STORE_REG_8 && !(mIns[i + 1].mLive & LIVE_ACCU) && mIns[i + 1].mRegister != BC_REG_ADDR &&
						(mIns[i + 0].mCode == BC_LOAD_ADDR_8 || mIns[i + 0].mCode == BC_LOAD_ADDR_U8 || mIns[i + 0].mCode == BC_LOAD_ADDR_16 || mIns[i + 0].mCode == BC_LOAD_ADDR_32) && 
						mIns[i + 0].mRegister == BC_REG_ACCU)	
					{
						mIns[i + 0].mCode = BC_LOAD_ADDR_8;
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 1].mCode == BC_STORE_REG_8 && !(mIns[i + 1].mLive & LIVE_ACCU) && mIns[i + 1].mRegister != BC_REG_ADDR &&
						(mIns[i + 0].mCode == BC_LOAD_ABS_8 || mIns[i + 0].mCode == BC_LOAD_ABS_U8 || mIns[i + 0].mCode == BC_LOAD_ABS_16 || mIns[i + 0].mCode == BC_LOAD_ABS_32) &&
						mIns[i + 0].mRegister == BC_REG_ACCU)
					{
						mIns[i + 0].mCode = BC_LOAD_ABS_8;
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 1].mCode == BC_STORE_REG_8 && !(mIns[i + 1].mLive & LIVE_ACCU) && mIns[i + 1].mRegister != BC_REG_ADDR &&
						(mIns[i + 0].mCode == BC_LOAD_LOCAL_8 || mIns[i + 0].mCode == BC_LOAD_LOCAL_U8 || mIns[i + 0].mCode == BC_LOAD_LOCAL_16 || mIns[i + 0].mCode == BC_LOAD_LOCAL_32) &&
						mIns[i + 0].mRegister == BC_REG_ACCU)
					{
						mIns[i + 0].mCode = BC_LOAD_LOCAL_8;
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_REG_8 && !(mIns[i + 1].mLive & LIVE_ACCU) && 
						(mIns[i + 1].mCode == BC_STORE_ABS_8 || mIns[i + 1].mCode == BC_STORE_LOCAL_8 || mIns[i + 1].mCode == BC_STORE_ADDR_8 || mIns[i + 1].mCode == BC_STORE_FRAME_8) &&
						mIns[i + 1].mRegister == BC_REG_ACCU)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						mIns[i + 1].mRegisterFinal = mIns[i + 0].mRegisterFinal;
						progress = true;
					}

#if 1
					else if (
						mIns[i + 0].mCode == BC_CONST_16 && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_STORE_REG_8 && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_CONST_8;
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mCode == BC_CONST_16 && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_STORE_REG_16 && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						(mIns[i + 0].mCode == BC_LEA_LOCAL || mIns[i + 0].mCode == BC_LEA_ABS || mIns[i + 0].mCode == BC_LEA_FRAME) && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_STORE_REG_16 && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_ADDR_REG && mIns[i + 1].mRegister == BC_REG_ACCU && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mCode = BC_LEA_ACCU_INDEX;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mCode == BC_LOAD_LOCAL_8 &&
						mIns[i + 1].mCode == BC_LOAD_REG_8 && mIns[i + 1].mRegister == mIns[i + 0].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 0].mCode = BC_LOAD_LOCAL_U8;
						mIns[i + 0].mRegister = BC_REG_ACCU;
						mIns[i + 0].mLive |= LIVE_ACCU;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_LOAD_LOCAL_U8 && mIns[i + 0].mRegister == BC_REG_ACCU &&
						mIns[i + 1].mCode == BC_STORE_REG_16 && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 0].mRegister = mIns[i + 1].mRegister;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mCode == BC_BINOP_ADDR_16 &&
						mIns[i + 1].mCode == BC_STORE_REG_16 && mIns[i + 0].mRegister == mIns[i + 1].mRegister && !(mIns[i + 1].mLive & LIVE_ACCU))
					{
						mIns[i + 1].mCode = BC_BINOP_ADDA_16;
						mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mCode == BC_BINOP_ADDA_16 &&
						mIns[i + 1].mCode == BC_LOAD_REG_16 && mIns[i + 0].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i + 1].mCode = BC_BINOP_ADDR_16;
						mIns[i + 0].mCode = BC_NOP;
						progress = true;
					}
#endif
#if 1
					else if (
						i + 2 == mIns.Size() && mFalseJump &&
						mIns[i + 0].mCode == BC_LOAD_REG_8 &&
						mIns[i + 1].mCode == BC_BINOP_CMPUR_8 && accuTempByte == mIns[i + 1].mRegister && !(mExitLive & LIVE_ACCU)
						)
					{
						mIns[i + 0].mCode = BC_NOP;
						mIns[i + 1].mRegister = mIns[i + 0].mRegister;
						mBranch = TransposeBranchCondition(mBranch);
						progress = true;
					}

#endif

#if 0
					else if ((mIns[i].mCode == BC_LOAD_LOCAL_16 || mIns[i].mCode == BC_LOAD_ABS_16) && mIns[i + 1].mCode == BC_ADDR_REG && mIns[i].mRegister == mIns[i + 1].mRegister && mIns[i + 1].mRegisterFinal)
					{
						mIns[i].mRegister = BC_REG_ADDR;
						mIns[i + 1].mCode = BC_NOP;
						progress = true;
					}
#endif
				}

				if (mIns[i].mCode == BC_BINOP_ANDI_16 && mIns[i].mRegister == BC_REG_ACCU && mIns[i].mValue == 0x00ff)
				{
					mIns[i].mCode = BC_LOAD_REG_8;
					progress = true;
				}
#endif
#if 1
				if ((mIns[i].mCode == BC_LOAD_REG_16 || mIns[i].mCode == BC_STORE_REG_16 || mIns[i].mCode == BC_LOAD_REG_32 || mIns[i].mCode == BC_STORE_REG_32) && accuTemp == mIns[i].mRegister)
				{
					mIns[i].mCode = BC_NOP;
					progress = true;
				}
				else if ((mIns[i].mCode == BC_LOAD_REG_8 || mIns[i].mCode == BC_STORE_REG_8) && accuTempByte == mIns[i].mRegister)
				{
					mIns[i].mCode = BC_NOP;
					progress = true;
				}
#endif
				if (mIns[i].mCode == BC_ADDR_REG && mIns[i].mRegister == addrTemp)
				{
					if (mIns[i].mRegisterFinal)
					{
						int	j = i;
						while (j > 0 && !mIns[j - 1].UsesRegister(mIns[i].mRegister) && !mIns[j - 1].ChangesRegister(mIns[i].mRegister))
							j--;
						if (j > 0 && mIns[j - 1].UsesRegister(mIns[i].mRegister))
							mIns[j - 1].mRegisterFinal = true;
					}
					mIns[i].mCode = BC_NOP;
					progress = true;
				}

				if (accuConst && !mIns[i].mRelocate && mIns[i].mRegister == BC_REG_ACCU)
				{
					switch (mIns[i].mCode)
					{
					case BC_CONST_16:
						if ((mIns[i].mValue & 0xffff) == (accuVal & 0xffff))
						{
							mIns[i].mCode = BC_NOP;
							progress = true;
						}
						break;
					case BC_CONST_32:
						if (mIns[i].mValue == accuVal)
						{
							mIns[i].mCode = BC_NOP;
							progress = true;
						}
						break;
					case BC_CONST_8:
						if ((mIns[i].mValue & 0xff) == (accuVal & 0xffff))
						{
							mIns[i].mCode = BC_NOP;
							progress = true;
						}
						break;
					}
				}

				if (mIns[i].ChangesAccu())
				{
					accuTemp = -1;
					accuTempByte = -1;
					accuConst = false;
				}
				if (mIns[i].ChangesAddr())
					addrTemp = -1;
				if (accuTemp != -1 && mIns[i].ChangesRegister(accuTemp))
					accuTemp = -1;
				if (accuTempByte != -1 && mIns[i].ChangesRegister(accuTempByte))
					accuTempByte = -1;
				if (addrTemp != -1 && mIns[i].ChangesRegister(addrTemp))
					addrTemp = -1;

				if (mIns[i].mCode == BC_LOAD_REG_16 || mIns[i].mCode == BC_STORE_REG_16 || mIns[i].mCode == BC_LOAD_REG_32 || mIns[i].mCode == BC_STORE_REG_32)
					accuTemp = mIns[i].mRegister;
				else if (mIns[i].mCode == BC_LOAD_REG_8)
					accuTempByte = mIns[i].mRegister;
				else if (mIns[i].mCode == BC_ADDR_REG && mIns[i].mRegister != BC_REG_ACCU)
					addrTemp = mIns[i].mRegister;

				if (mIns[i].mRegister == BC_REG_ACCU && !mIns[i].mRelocate)
				{
					switch (mIns[i].mCode)
					{
					case BC_CONST_16:
						accuVal = mIns[i].mValue & 0xffff;
						accuConst = true;
						break;
					case BC_CONST_32:
						accuVal = mIns[i].mValue;
						accuConst = true;
						break;
					case BC_CONST_8:
						accuVal = mIns[i].mValue & 0xff;
						accuConst = true;
						break;
					}
				}
			}

			if (phase >= 2)
			{
				for (int i = mIns.Size() - 1; i >= 0; i--)
				{
					if (mIns[i].CheckAccuSize(aused))
						progress = true;
				}
			}

			if (progress)
				changed = true;
		} while (progress);

		if (mTrueJump && mTrueJump->PeepHoleOptimizer(phase))
			changed = true;
		if (mFalseJump && mFalseJump->PeepHoleOptimizer(phase))
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
	if (branch == BC_NOP)
	{
		this->mTrueJump = this->mTrueLink = falseJump;
		this->mFalseJump = this->mFalseLink = nullptr;
		this->mBranch = BC_JUMPS;
	}
	else if (branch == BC_JUMPS)
	{
		this->mTrueJump = this->mTrueLink = trueJump;
		this->mFalseJump = this->mFalseLink = nullptr;
		this->mBranch = BC_JUMPS;
	}
	else
	{
		this->mTrueJump = this->mTrueLink = trueJump;
		this->mFalseJump = this->mFalseLink = falseJump;
		this->mBranch = branch;
	}
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
			linkerObject->AddReference(rl);
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
#if 1
			else if (!mTrueJump->mFalseJump && !mFalseJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump && 
				mTrueJump->mCode.Size() < 120 && mFalseJump->mCode.Size() < 120 && mTrueJump->mTrueJump->mOffset > mOffset)
			{
				// Small diamond so place true then false directly behind each other
				// with short branches

				mSize = mCode.Size() + 2;

				mFalseJump->mOffset = next + 2;
				mFalseJump->mSize = mFalseJump->mCode.Size() + 2;

				total = mFalseJump->mOffset + mFalseJump->mSize;
				mTrueJump->CalculateOffset(total);
			}
#endif
			else
			{
				// neither falseJump nor trueJump have been placed
				// 

				if (mTrueJump->mFalseJump == mFalseJump || mTrueJump->mTrueJump == mFalseJump)
				{
					ByteCodeBasicBlock* block = mFalseJump;
					mFalseJump = mTrueJump;
					mTrueJump = block;
					mBranch = InvertBranchCondition(mBranch);
				}

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

	entryBlock->Compile(proc, this, proc->mBlocks[0]);

#if 1
	bool	progress = false;

	int	phase = 0;

	do {

		progress = false;

		ResetVisited();
		progress = entryBlock->PeepHoleOptimizer(phase);

		ResetVisited();
		for (int i = 0; i < mBlocks.Size(); i++)
			mBlocks[i]->mEntryBlocks.SetSize(0);
		entryBlock->CollectEntryBlocks(nullptr);

		ResetVisited();
		if (entryBlock->JoinTailCodeSequences())
			progress = true;

		if (!progress && phase < 4)
		{
			phase++;
			progress = true;
		}

	} while (progress);

	ResetVisited();
	entryBlock->PropagateAccuCrossBorder(-1, -1);

#endif

	entryBlock->Assemble(generator);

	if (!proc->mLeafProcedure)
	{
		exitBlock->PutCode(generator, BC_POP_FRAME);
		exitBlock->PutWord(uint16(proc->mCommonFrameSize + 2));
	}
	exitBlock->PutCode(generator, BC_RETURN); exitBlock->PutByte(tempSave); exitBlock->PutWord(proc->mLocalSize + 2 + tempSave);


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
	{
		mByteCodeUsed[i] = 0;
		mExtByteCodes[i] = nullptr;
	}

	mByteCodeUsed[BC_CALL_ABS] = 1;
	mByteCodeUsed[BC_EXIT] = 1;
	mByteCodeUsed[BC_NATIVE] = 1;

	assert(sizeof(ByteCodeNames) == 128 * sizeof(char*));
}

ByteCodeGenerator::~ByteCodeGenerator(void)
{
}

bool ByteCodeGenerator::WriteByteCodeStats(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "w");
	if (file)
	{
		for (int i = 0; i < 128; i++)
		{
			if (mByteCodeUsed[i] > 0)
				fprintf(file, "BC %s : %d\n", ByteCodeNames[i], mByteCodeUsed[i]);
		}
		fclose(file);

		return true;
	}

	return false;
}

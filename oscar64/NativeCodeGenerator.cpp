#include "NativeCodeGenerator.h"
#include "CompilerTypes.h"

static const int CPU_REG_A = 256;
static const int CPU_REG_X = 257;
static const int CPU_REG_Y = 258;
static const int CPU_REG_C = 259;
static const int CPU_REG_Z = 260;

static const int NUM_REGS = 261;

static const uint32 LIVE_CPU_REG_A = 0x00000001;
static const uint32 LIVE_CPU_REG_X = 0x00000002;
static const uint32 LIVE_CPU_REG_Y = 0x00000004;
static const uint32 LIVE_CPU_REG_C = 0x00000008;
static const uint32 LIVE_CPU_REG_Z = 0x00000010;
static const uint32 LIVE_MEM	   = 0x00000020;

static int GlobalValueNumber = 0;

NativeRegisterData::NativeRegisterData(void)
	: mMode(NRDM_UNKNOWN), mValue(GlobalValueNumber++)
{

}

void NativeRegisterData::Reset(void)
{
	mMode = NRDM_UNKNOWN;
	mValue = GlobalValueNumber++;
}

bool NativeRegisterData::SameData(const NativeRegisterData& d) const
{
	if (mMode != d.mMode)
		return false;

	switch (mMode)
	{
	case NRDM_IMMEDIATE:
	case NRDM_ZERO_PAGE:
		return mValue == d.mValue;
	case NRDM_IMMEDIATE_ADDRESS:
	case NRDM_ABSOLUTE:
		return mValue == d.mValue && mLinkerObject == d.mLinkerObject && mFlags == d.mFlags;
	default:
		return false;
	}
}

void NativeRegisterDataSet::Reset(void)
{
	for (int i = 0; i < NUM_REGS; i++)
		mRegs[i].Reset();
}

void NativeRegisterDataSet::ResetZeroPage(int addr)
{
	mRegs[addr].Reset();
	for (int i = 0; i < NUM_REGS; i++)
	{
		if (mRegs[i].mMode == NRDM_ZERO_PAGE && mRegs[i].mValue == addr)
			mRegs[i].Reset();
	}
}

void NativeRegisterDataSet::ResetAbsolute(LinkerObject* linkerObject, int addr)
{
	for (int i = 0; i < NUM_REGS; i++)
	{
		if (mRegs[i].mMode == NRDM_ABSOLUTE && mRegs[i].mLinkerObject == linkerObject && mRegs[i].mValue == addr)
			mRegs[i].Reset();
	}
}

void NativeRegisterDataSet::ResetIndirect(void)
{
	for (int i = 0; i < NUM_REGS; i++)
	{
		if (mRegs[i].mMode == NRDM_ABSOLUTE)
			mRegs[i].Reset();
	}
}


void NativeRegisterDataSet::Intersect(const NativeRegisterDataSet& set)
{

	for (int i = 0; i < NUM_REGS; i++)
	{
		if (mRegs[i].mMode == NRDM_UNKNOWN)
		{
			if (set.mRegs[i].mMode != NRDM_UNKNOWN || mRegs[i].mValue != set.mRegs[i].mValue)
				mRegs[i].Reset();
		}
		else if (mRegs[i].mMode == NRDM_IMMEDIATE)
		{
			if (set.mRegs[i].mMode != NRDM_IMMEDIATE || mRegs[i].mValue != set.mRegs[i].mValue)
				mRegs[i].Reset();
		}
		else if (mRegs[i].mMode == NRDM_IMMEDIATE_ADDRESS)
		{
			if (set.mRegs[i].mMode != NRDM_IMMEDIATE_ADDRESS || mRegs[i].mValue != set.mRegs[i].mValue || mRegs[i].mLinkerObject != set.mRegs[i].mLinkerObject || mRegs[i].mFlags != set.mRegs[i].mFlags)
				mRegs[i].Reset();
		}
	}

	bool	changed;
	do
	{
		changed = false;

		for (int i = 0; i < NUM_REGS; i++)
		{
			if (mRegs[i].mMode == NRDM_ZERO_PAGE)
			{
				if (set.mRegs[i].mMode != NRDM_ZERO_PAGE || mRegs[i].mValue != set.mRegs[i].mValue)
				{
					mRegs[i].Reset();
					changed = true;
				}
				else if (mRegs[mRegs[i].mValue].mValue != set.mRegs[set.mRegs[i].mValue].mValue)
				{
					mRegs[i].Reset();
					changed = true;
				}
			}
			else if (mRegs[i].mMode == NRDM_ABSOLUTE)
			{
				if (set.mRegs[i].mMode != NRDM_ABSOLUTE || mRegs[i].mValue != set.mRegs[i].mValue)
				{
					mRegs[i].Reset();
					changed = true;
				}
			}
		}

	} while (changed);
}

NativeCodeInstruction::NativeCodeInstruction(AsmInsType type, AsmInsMode mode, int address, LinkerObject* linkerObject, uint32 flags, int param)
	: mType(type), mMode(mode), mAddress(address), mLinkerObject(linkerObject), mFlags(flags), mParam(param)
{
	if (mode == ASMIM_IMMEDIATE_ADDRESS)
	{
		assert((mFlags & (NCIF_LOWER | NCIF_UPPER)) != (NCIF_LOWER | NCIF_UPPER));
		assert(HasAsmInstructionMode(mType, ASMIM_IMMEDIATE));
	}
}

bool NativeCodeInstruction::IsUsedResultInstructions(NumberSet& requiredTemps)
{
	bool	used = false;

	mLive = 0;
	if (requiredTemps[CPU_REG_A])
		mLive |= LIVE_CPU_REG_A;
	if (requiredTemps[CPU_REG_X])
		mLive |= LIVE_CPU_REG_X;
	if (requiredTemps[CPU_REG_Y])
		mLive |= LIVE_CPU_REG_Y;
	if (requiredTemps[CPU_REG_Z])
		mLive |= LIVE_CPU_REG_Z;
	if (requiredTemps[CPU_REG_C])
		mLive |= LIVE_CPU_REG_C;
	if (mMode == ASMIM_ZERO_PAGE && requiredTemps[mAddress])
		mLive |= LIVE_MEM;
	if (mMode == ASMIM_INDIRECT_Y && (requiredTemps[mAddress] || requiredTemps[mAddress + 1]))
		mLive |= LIVE_MEM;

	if (mType == ASMIT_JSR)
	{
		requiredTemps -= CPU_REG_C;
		requiredTemps -= CPU_REG_Z;
		requiredTemps -= CPU_REG_A;
		requiredTemps -= CPU_REG_X;
		requiredTemps -= CPU_REG_Y;

		if (mFlags & NCIF_RUNTIME)
		{
			for (int i = 0; i < 4; i++)
			{
				requiredTemps += BC_REG_ACCU + i;
				requiredTemps += BC_REG_WORK + i;
			}

			if (mFlags & NCIF_USE_ZP_32_X)
			{
				for (int i = 0; i < 4; i++)
					requiredTemps += mParam + i;
			}

			if (mFlags & NCIF_USE_CPU_REG_A)
				requiredTemps += CPU_REG_A;

			if (mFlags & NCIF_FEXEC)
			{
				requiredTemps += BC_REG_LOCALS;
				requiredTemps += BC_REG_LOCALS + 1;
				for(int i= BC_REG_FPARAMS; i< BC_REG_FPARAMS_END; i++)
					requiredTemps += i;
			}
		}
		else
		{
			for (int i = 0; i < 4; i++)
			{
				requiredTemps -= BC_REG_ACCU + i;
				requiredTemps -= BC_REG_WORK + i;
			}

			requiredTemps += BC_REG_LOCALS;
			requiredTemps += BC_REG_LOCALS + 1;
			if (mLinkerObject)
			{
				for (int i = 0; i < mLinkerObject->mNumTemporaries; i++)
				{
					for (int j = 0; j < mLinkerObject->mTempSizes[i]; j++)
						requiredTemps += mLinkerObject->mTemporaries[i] + j;
				}
			}
		}

		return true;
	}

	if (mType == ASMIT_RTS)
	{
#if 1
		if (mFlags & NCIF_LOWER)
		{
			requiredTemps += BC_REG_ACCU;
			if (mFlags & NCIF_UPPER)
			{
				requiredTemps += BC_REG_ACCU + 1;

				if (mFlags & NCIF_LONG)
				{
					requiredTemps += BC_REG_ACCU + 2;
					requiredTemps += BC_REG_ACCU + 3;
				}
			}
		}
#endif
#if 0
		for (int i = 0; i < 4; i++)
		{
			requiredTemps += BC_REG_ACCU + i;
		}
#endif

		requiredTemps += BC_REG_STACK;
		requiredTemps += BC_REG_STACK + 1;
		requiredTemps += BC_REG_LOCALS;
		requiredTemps += BC_REG_LOCALS + 1;

		return true;
	}

	if (mType == ASMIT_BYTE)
		return true;

	// check side effects

	switch (mType)
	{
	case ASMIT_STA:
	case ASMIT_STX:
	case ASMIT_STY:
	case ASMIT_INC:
	case ASMIT_DEC:
	case ASMIT_ASL:
	case ASMIT_LSR:
	case ASMIT_ROL:
	case ASMIT_ROR:
		if (mMode != ASMIM_IMPLIED && mMode != ASMIM_ZERO_PAGE)
			used = true;
		break;
	case ASMIT_JSR:
	case ASMIT_JMP:
	case ASMIT_BEQ:
	case ASMIT_BNE:
	case ASMIT_BPL:
	case ASMIT_BMI:
	case ASMIT_BCC:
	case ASMIT_BCS:
		used = true;
		break;
	}

	if (requiredTemps[CPU_REG_C])
	{
		switch (mType)
		{
		case ASMIT_CLC:
		case ASMIT_SEC:
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_CMP:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_CPX:
		case ASMIT_CPY:
			used = true;
			break;
		}
	}

	if (requiredTemps[CPU_REG_Z])
	{
		switch (mType)
		{
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_INC:
		case ASMIT_DEC:
		case ASMIT_CMP:
		case ASMIT_CPX:
		case ASMIT_CPY:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_AND:
		case ASMIT_LDA:
		case ASMIT_LDX:
		case ASMIT_LDY:
		case ASMIT_BIT:
		case ASMIT_INX:
		case ASMIT_DEX:
		case ASMIT_INY:
		case ASMIT_DEY:
		case ASMIT_TYA:
		case ASMIT_TXA:
		case ASMIT_TAY:
		case ASMIT_TAX:
			used = true;
			break;
		}
	}

	if (requiredTemps[CPU_REG_A])
	{
		switch (mType)
		{
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_ASL:
		case ASMIT_LSR:
			if (mMode == ASMIM_IMPLIED)
				used = true;
			break;
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_AND:
		case ASMIT_LDA:
		case ASMIT_TXA:
		case ASMIT_TYA:
			used = true;
			break;
		}
	}

	if (requiredTemps[CPU_REG_X])
	{
		switch (mType)
		{
		case ASMIT_LDX:
		case ASMIT_INX:
		case ASMIT_DEX:
		case ASMIT_TAX:
			used = true;
			break;
		}
	}

	if (requiredTemps[CPU_REG_Y])
	{
		switch (mType)
		{
		case ASMIT_LDY:
		case ASMIT_INY:
		case ASMIT_DEY:
		case ASMIT_TAY:
			used = true;
			break;
		}
	}

	if (mMode == ASMIM_ZERO_PAGE)
	{
		switch (mType)
		{
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_INC:
		case ASMIT_DEC:
		case ASMIT_STA:
		case ASMIT_STX:
		case ASMIT_STY:
			if (requiredTemps[mAddress])
				used = true;
			break;
		}
	}

	if (used)
	{
		switch (mMode)
		{
		case ASMIM_ZERO_PAGE_X:
		case ASMIM_INDIRECT_X:
		case ASMIM_ABSOLUTE_X:
			requiredTemps += CPU_REG_X;
			break;

		case ASMIM_ZERO_PAGE_Y:
		case ASMIM_ABSOLUTE_Y:
			requiredTemps += CPU_REG_Y;
			break;

		case ASMIM_INDIRECT_Y:
			requiredTemps += CPU_REG_Y;
			requiredTemps += mAddress;
			requiredTemps += mAddress + 1;
			break;
		}

		// check carry flags

		switch (mType)
		{
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ROL:
		case ASMIT_ROR:
			requiredTemps += CPU_REG_C;
			break;
		case ASMIT_CMP:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_CPX:
		case ASMIT_CPY:
		case ASMIT_CLC:
		case ASMIT_SEC:
			requiredTemps -= CPU_REG_C;
			break;
		case ASMIT_BCC:
		case ASMIT_BCS:
			requiredTemps += CPU_REG_C;
			break;
		case ASMIT_BEQ:
		case ASMIT_BNE:
		case ASMIT_BPL:
		case ASMIT_BMI:
			requiredTemps += CPU_REG_Z;
			break;
		}

		// check zero flags

		switch (mType)
		{
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_CMP:
		case ASMIT_CPX:
		case ASMIT_CPY:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_INC:
		case ASMIT_DEC:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_AND:
		case ASMIT_LDA:
		case ASMIT_LDX:
		case ASMIT_LDY:
		case ASMIT_BIT:
		case ASMIT_TAY:
		case ASMIT_TYA:
		case ASMIT_TAX:
		case ASMIT_TXA:
		case ASMIT_INX:
		case ASMIT_DEX:
		case ASMIT_INY:
		case ASMIT_DEY:
			requiredTemps -= CPU_REG_Z;
			break;
		}

		// check CPU register

		switch (mType)
		{
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_ASL:
		case ASMIT_LSR:
			if (mMode == ASMIM_IMPLIED)
				requiredTemps += CPU_REG_A;
			break;

		case ASMIT_LDA:
			requiredTemps -= CPU_REG_A;
			break;

		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_AND:
			requiredTemps += CPU_REG_A;
			break;
		case ASMIT_LDX:
			requiredTemps -= CPU_REG_X;
			break;
		case ASMIT_INX:
		case ASMIT_DEX:
			requiredTemps += CPU_REG_X;
			break;
		case ASMIT_LDY:
			requiredTemps -= CPU_REG_Y;
			break;
		case ASMIT_INY:
		case ASMIT_DEY:
			requiredTemps += CPU_REG_Y;
			break;

		case ASMIT_CMP:
		case ASMIT_STA:
			requiredTemps += CPU_REG_A;
			break;
		case ASMIT_CPX:
		case ASMIT_STX:
			requiredTemps += CPU_REG_X;
			break;
		case ASMIT_CPY:
		case ASMIT_STY:
			requiredTemps += CPU_REG_Y;
			break;

		case ASMIT_TXA:
			requiredTemps += CPU_REG_X;
			requiredTemps -= CPU_REG_A;
			break;
		case ASMIT_TYA:
			requiredTemps += CPU_REG_Y;
			requiredTemps -= CPU_REG_A;
			break;
		case ASMIT_TAX:
			requiredTemps += CPU_REG_A;
			requiredTemps -= CPU_REG_X;
			break;
		case ASMIT_TAY:
			requiredTemps += CPU_REG_A;
			requiredTemps -= CPU_REG_Y;
			break;
		}

		if (mMode == ASMIM_ZERO_PAGE)
		{
			switch (mType)
			{
			case ASMIT_STA:
			case ASMIT_STX:
			case ASMIT_STY:
				requiredTemps -= mAddress;
				break;
			default:
				requiredTemps += mAddress;
			}
		}

		return true;
	}

	return false;
}

bool NativeCodeInstruction::LoadsAccu(void) const
{
	return mType == ASMIT_LDA || mType == ASMIT_TXA || mType == ASMIT_TYA || mType == ASMIT_JSR;
}

bool NativeCodeInstruction::ChangesAccuAndFlag(void) const
{
	if (mType == ASMIT_LDA || mType == ASMIT_TXA || mType == ASMIT_TYA ||
		mType == ASMIT_ORA || mType == ASMIT_AND || mType == ASMIT_EOR ||
		mType == ASMIT_SBC || mType == ASMIT_ADC || mType == ASMIT_JSR)
		return true;
	else if (mType == ASMIT_LSR || mType == ASMIT_ASL || mType == ASMIT_ROR || mType == ASMIT_ROL)
		return mMode == ASMIM_IMPLIED;
	else
		return false;
}

bool NativeCodeInstruction::RequiresYReg(void) const
{
	if (mMode == ASMIM_ABSOLUTE_Y || mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_ZERO_PAGE_Y)
		return true;
	if (mType == ASMIT_TYA || mType == ASMIT_STY || mType == ASMIT_CPY || mType == ASMIT_INY || mType == ASMIT_DEY)
		return true;

	return false;
}

bool NativeCodeInstruction::RequiresXReg(void) const
{
	if (mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_INDIRECT_X || mMode == ASMIM_ZERO_PAGE_X)
		return true;
	if (mType == ASMIT_TXA || mType == ASMIT_STX || mType == ASMIT_CPX || mType == ASMIT_INX || mType == ASMIT_DEX)
		return true;

	return false;
}


void NativeCodeInstruction::ReplaceYRegWithXReg(void)
{
	switch (mType)
	{
	case ASMIT_LDY:
		mType = ASMIT_LDX;
		break;
	case ASMIT_STY:
		mType = ASMIT_STX;
		break;
	case ASMIT_CPY:
		mType = ASMIT_CPX;
		break;
	case ASMIT_TYA:
		mType = ASMIT_TXA;
		break;
	case ASMIT_TAY:
		mType = ASMIT_TAX;
		break;
	}


	if (mMode == ASMIM_ABSOLUTE_Y)
	{
		assert(HasAsmInstructionMode(mType, ASMIM_ABSOLUTE_X));
		mMode = ASMIM_ABSOLUTE_X;
	}
}

bool NativeCodeInstruction::ChangesYReg(void) const
{
	return mType == ASMIT_TAY || mType == ASMIT_LDY || mType == ASMIT_INY || mType == ASMIT_DEY || mType == ASMIT_JSR;
}

bool NativeCodeInstruction::ChangesXReg(void) const
{
	return mType == ASMIT_TAX || mType == ASMIT_LDX || mType == ASMIT_INX || mType == ASMIT_DEX || mType == ASMIT_JSR;
}

bool NativeCodeInstruction::ChangesZeroPage(int address) const
{
	if (mMode == ASMIM_ZERO_PAGE && mAddress == address)
		return mType == ASMIT_INC || mType == ASMIT_DEC || mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR || mType == ASMIT_STA || mType == ASMIT_STX || mType == ASMIT_STY;
	else
		return false;
}

bool NativeCodeInstruction::UsesZeroPage(int address) const
{
	if (mMode == ASMIM_ZERO_PAGE && mAddress == address)
		return true;
	else if (mMode == ASMIM_INDIRECT_Y && (mAddress == address || mAddress == address + 1))
		return true;
	else
		return false;
}


bool NativeCodeInstruction::ChangesGlobalMemory(void) const
{
	if (mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_ABSOLUTE || mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_ABSOLUTE_Y)
		return mType == ASMIT_INC || mType == ASMIT_DEC || mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR || mType == ASMIT_STA || mType == ASMIT_STX || mType == ASMIT_STY || mType == ASMIT_JSR;
	else
		return false;
}

bool NativeCodeInstruction::RequiresCarry(void) const
{
	return
		mType == ASMIT_ADC || mType == ASMIT_SBC ||
		mType == ASMIT_ROL || mType == ASMIT_ROR;
}

bool NativeCodeInstruction::ChangesZFlag(void) const
{
	return 
		mType == ASMIT_ADC || mType == ASMIT_SBC ||
		mType == ASMIT_LSR || mType == ASMIT_ASL || mType == ASMIT_ROL || mType == ASMIT_ROR ||
		mType == ASMIT_INC || mType == ASMIT_DEC ||
		mType == ASMIT_INY || mType == ASMIT_DEY ||
		mType == ASMIT_INX || mType == ASMIT_DEX ||
		mType == ASMIT_CMP || mType == ASMIT_CPX || mType == ASMIT_CPY ||
		mType == ASMIT_LDA || mType == ASMIT_LDX || mType == ASMIT_LDY ||
		mType == ASMIT_JSR;
}

bool NativeCodeInstruction::ChangesCarry(void) const
{
	return
		mType == ASMIT_CLC || mType == ASMIT_SEC ||
		mType == ASMIT_ADC || mType == ASMIT_SBC ||
		mType == ASMIT_LSR || mType == ASMIT_ASL || mType == ASMIT_ROL || mType == ASMIT_ROR ||
		mType == ASMIT_CMP || mType == ASMIT_CPX || mType == ASMIT_CPY ||
		mType == ASMIT_JSR;
}

bool NativeCodeInstruction::RequiresAccu(void) const
{
	if (mMode == ASMIM_IMPLIED)
	{
		return
			mType == ASMIT_TAX || mType == ASMIT_TAY ||
			mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR;
	}
	else
	{
		return
			mType == ASMIT_STA ||
			mType == ASMIT_ORA || mType == ASMIT_AND || mType == ASMIT_EOR ||
			mType == ASMIT_SBC || mType == ASMIT_ADC || mType == ASMIT_CMP;
	}
}

bool NativeCodeInstruction::UsesAccu(void) const
{
	if (ChangesAccu())
		return true;

	return mType == ASMIT_STA || mType == ASMIT_CMP || mType == ASMIT_TAX || mType == ASMIT_TAY;
}

bool NativeCodeInstruction::ChangesAccu(void) const
{
	if (mMode == ASMIM_IMPLIED)
	{
		return
			mType == ASMIT_TXA || mType == ASMIT_TYA ||
			mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR;
	}
	else
	{
		return
			mType == ASMIT_JSR ||
			mType == ASMIT_LDA || 
			mType == ASMIT_ORA || mType == ASMIT_AND || mType == ASMIT_EOR ||
			mType == ASMIT_SBC || mType == ASMIT_ADC;
	}
}



bool NativeCodeInstruction::ChangesAddress(void) const
{
	if (mMode != ASMIM_IMPLIED)
		return mType == ASMIT_INC || mType == ASMIT_DEC || mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR || mType == ASMIT_STA || mType == ASMIT_STX || mType == ASMIT_STY;
	else
		return false;
}

bool NativeCodeInstruction::IsShift(void) const
{
	return mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR;
}

bool NativeCodeInstruction::IsCommutative(void) const
{
	return mType == ASMIT_ADC || mType == ASMIT_AND || mType == ASMIT_ORA || mType == ASMIT_EOR;
}


bool NativeCodeInstruction::IsSame(const NativeCodeInstruction& ins) const
{
	if (mType == ins.mType && mMode == ins.mMode && mParam == ins.mParam)
	{
		switch (mMode)
		{
		case ASMIM_IMPLIED:
			return true;
		case ASMIM_IMMEDIATE:
		case ASMIM_ZERO_PAGE:
		case ASMIM_ZERO_PAGE_X:
		case ASMIM_ZERO_PAGE_Y:
		case ASMIM_INDIRECT_X:
		case ASMIM_INDIRECT_Y:
			return ins.mAddress == mAddress;
		case ASMIM_IMMEDIATE_ADDRESS:
			return (ins.mLinkerObject == mLinkerObject && ins.mAddress == mAddress && ins.mFlags == mFlags);
		case ASMIM_ABSOLUTE:
		case ASMIM_ABSOLUTE_X:
		case ASMIM_ABSOLUTE_Y:
			return (ins.mLinkerObject == mLinkerObject && ins.mAddress == mAddress);
		default:
			return false;
		}
	}
	else
		return false;
}

bool NativeCodeInstruction::MayBeChangedOnAddress(const NativeCodeInstruction& ins) const
{
	if (!ins.ChangesAddress())
		return false;

	if (ins.mMode == ASMIM_ZERO_PAGE)
	{
		if (mMode == ASMIM_ZERO_PAGE)
			return mAddress == ins.mAddress;
		else if (mMode == ASMIM_INDIRECT_X || mMode == ASMIM_INDIRECT_Y)
			return mAddress == ins.mAddress || mAddress + 1 == ins.mAddress;
		else
			return mMode == ASMIM_ZERO_PAGE_X || mMode == ASMIM_ZERO_PAGE_Y;
	}
	else if (ins.mMode == ASMIM_ZERO_PAGE_X || ins.mMode == ASMIM_ZERO_PAGE_Y)
	{
		return mMode == ASMIM_ZERO_PAGE || mMode == ASMIM_INDIRECT_X || mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_ZERO_PAGE_X || mMode == ASMIM_ZERO_PAGE_Y;
	}
	else if (ins.mMode == ASMIM_ABSOLUTE)
	{
		if (mMode == ASMIM_ABSOLUTE)
			return mLinkerObject == ins.mLinkerObject && mAddress == ins.mAddress;
		else if (mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_ABSOLUTE_Y)
			return mLinkerObject == ins.mLinkerObject;
		else
			return mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_INDIRECT_X;
	}
	else if (ins.mMode == ASMIM_ABSOLUTE_X || ins.mMode == ASMIM_ABSOLUTE_Y)
	{
		if (mMode == ASMIM_ABSOLUTE || mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_ABSOLUTE_Y)
			return mLinkerObject == ins.mLinkerObject;
		else
			return mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_INDIRECT_X;
	}
	else if (ins.mMode == ASMIM_INDIRECT_Y || ins.mMode == ASMIM_INDIRECT_X)
		return mMode == ASMIM_ABSOLUTE || mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_ABSOLUTE_Y || mMode == ASMIM_INDIRECT_Y || mMode == ASMIM_INDIRECT_X;
	else
		return false;
}


bool NativeCodeInstruction::SameEffectiveAddress(const NativeCodeInstruction& ins) const
{
	if (mMode != ins.mMode)
		return false;

	switch (mMode)
	{
	case ASMIM_ZERO_PAGE:
	case ASMIM_ZERO_PAGE_X:
	case ASMIM_ZERO_PAGE_Y:
	case ASMIM_INDIRECT_X:
	case ASMIM_INDIRECT_Y:
		return ins.mAddress == mAddress;
	case ASMIM_ABSOLUTE:
	case ASMIM_ABSOLUTE_X:
	case ASMIM_ABSOLUTE_Y:
		return (ins.mLinkerObject == mLinkerObject && ins.mAddress == mAddress);
	default:
		return false;
	}
}

bool NativeCodeInstruction::ApplySimulation(const NativeRegisterDataSet& data)
{
	switch (mType)
	{
	case ASMIT_LDA:
	case ASMIT_LDX:
	case ASMIT_LDY:
	case ASMIT_CMP:
	case ASMIT_CPX:
	case ASMIT_CPY:
	case ASMIT_ADC:
	case ASMIT_SBC:
	case ASMIT_AND:
	case ASMIT_ORA:
	case ASMIT_EOR:
		if (mMode == ASMIM_ZERO_PAGE && data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
		{
			mMode = ASMIM_IMMEDIATE;
			mAddress = data.mRegs[mAddress].mValue;
			return true;
		}
		else if (mMode == ASMIM_ZERO_PAGE && data.mRegs[mAddress].mMode == NRDM_IMMEDIATE_ADDRESS)
		{
			mMode = ASMIM_IMMEDIATE_ADDRESS;
			mLinkerObject = data.mRegs[mAddress].mLinkerObject;
			mFlags = data.mRegs[mAddress].mFlags;
			mAddress = data.mRegs[mAddress].mValue;
			assert((mFlags & (NCIF_LOWER | NCIF_UPPER)) != (NCIF_LOWER | NCIF_UPPER));
			return true;
		}
		break;
	}

	if (mMode == ASMIM_INDIRECT_Y && data.mRegs[mAddress].mMode == NRDM_IMMEDIATE && data.mRegs[mAddress + 1].mMode == NRDM_IMMEDIATE)
	{
		mMode = ASMIM_ABSOLUTE_Y;
		mAddress = data.mRegs[mAddress].mValue + 256 * data.mRegs[mAddress + 1].mValue;
		mLinkerObject = nullptr;
	}
	else if (mMode == ASMIM_INDIRECT_Y && data.mRegs[mAddress].mMode == NRDM_IMMEDIATE_ADDRESS && data.mRegs[mAddress + 1].mMode == NRDM_IMMEDIATE_ADDRESS && data.mRegs[mAddress].mLinkerObject == data.mRegs[mAddress + 1].mLinkerObject)
	{
		mMode = ASMIM_ABSOLUTE_Y;
		mLinkerObject = data.mRegs[mAddress].mLinkerObject;
		mAddress = data.mRegs[mAddress].mValue;
	}

	return false;
}

void NativeCodeInstruction::Simulate(NativeRegisterDataSet& data)
{
	int	reg = -1;
	if (mMode == ASMIM_ZERO_PAGE)
		reg = mAddress;
	else if (mMode == ASMIM_IMPLIED)
		reg = CPU_REG_A;

	switch (mType)
	{
	case ASMIT_JSR:
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_X].Reset();
		data.mRegs[CPU_REG_Y].Reset();

		for (int i = 0; i < 4; i++)
		{
			data.mRegs[BC_REG_ACCU + i].Reset();
			data.mRegs[BC_REG_WORK + i].Reset();
			data.mRegs[BC_REG_ADDR + i].Reset();
		}
		data.mRegs[BC_REG_WORK_Y].Reset();

		if (mFlags & NCIF_FEXEC)
		{
			for (int i = BC_REG_TMP; i < BC_REG_TMP_SAVED; i++)
				data.mRegs[i].Reset();
		}
		else if (!(mFlags & NCIF_RUNTIME))
		{
			if (mLinkerObject && mLinkerObject->mProc)
			{
				for (int i = BC_REG_TMP; i < BC_REG_TMP + mLinkerObject->mProc->mCallerSavedTemps; i++)
					data.mRegs[i].Reset();
			}
			else
			{
				for (int i = BC_REG_TMP; i < BC_REG_TMP_SAVED; i++)
					data.mRegs[i].Reset();
			}
		}
		break;

	case ASMIT_ROL:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue << 1) | data.mRegs[CPU_REG_C].mValue;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[reg].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_ROR:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue >> 1) | (data.mRegs[CPU_REG_C].mValue << 7);
				data.mRegs[CPU_REG_C].mValue = data.mRegs[reg].mValue & 1;
				data.mRegs[reg].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_ASL:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue << 1);
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[reg].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_LSR:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue >> 1);
				data.mRegs[CPU_REG_C].mValue = data.mRegs[reg].mValue & 1;
				data.mRegs[reg].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_INC:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[reg].mValue = (data.mRegs[reg].mValue + 1) & 255;
				data.mRegs[CPU_REG_Z].mValue = data.mRegs[reg].mValue;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_DEC:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[reg].mValue = (data.mRegs[reg].mValue + 1) & 255;
				data.mRegs[CPU_REG_Z].mValue = data.mRegs[reg].mValue;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_ADC:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue + data.mRegs[CPU_REG_A].mValue + data.mRegs[CPU_REG_C].mValue;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[CPU_REG_A].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_SBC:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue ^ 0xff) + data.mRegs[CPU_REG_A].mValue + data.mRegs[CPU_REG_C].mValue;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[CPU_REG_A].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_AND:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue & data.mRegs[CPU_REG_A].mValue;
				data.mRegs[CPU_REG_A].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else if ((data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[reg].mValue == 0) || (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == 0))
			{
				data.mRegs[CPU_REG_A].mValue = 0;
				data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = 0;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_ORA:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue | data.mRegs[CPU_REG_A].mValue;
				data.mRegs[CPU_REG_A].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else if ((data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[reg].mValue == 0xff) || (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == 0xff))
			{
				data.mRegs[CPU_REG_A].mValue = 0xff;
				data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = 0xff;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_EOR:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue | data.mRegs[CPU_REG_A].mValue;
				data.mRegs[CPU_REG_A].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_INX:
		if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_X].mValue = (data.mRegs[CPU_REG_X].mValue + 1) & 255;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_X].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_DEX:
		if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_X].mValue = (data.mRegs[CPU_REG_X].mValue - 1) & 255;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_X].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_INY:
		if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Y].mValue = (data.mRegs[CPU_REG_Y].mValue + 1) & 255;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_Y].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_DEY:
		if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Y].mValue = (data.mRegs[CPU_REG_Y].mValue - 1) & 255;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_Y].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_TXA:
		if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_A].mValue = data.mRegs[CPU_REG_X].mValue;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_X].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_TYA:
		if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_A].mValue = data.mRegs[CPU_REG_Y].mValue;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_Y].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_TAX:
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_X].mValue = data.mRegs[CPU_REG_A].mValue;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_X].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_TAY:
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Y].mValue = data.mRegs[CPU_REG_A].mValue;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_Y].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_CMP:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue ^ 0xff) + data.mRegs[CPU_REG_A].mValue + 1;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_CPX:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue ^ 0xff) + data.mRegs[CPU_REG_X].mValue + 1;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_CPY:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
			{
				int	t = (data.mRegs[reg].mValue ^ 0xff) + data.mRegs[CPU_REG_Y].mValue + 1;
				data.mRegs[CPU_REG_C].mValue = t >= 256;
				data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t & 255;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_LDA:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue;
				data.mRegs[CPU_REG_A].mValue = t;
				data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else if (mMode == ASMIM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_A].mValue = mAddress;
			data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else if (mMode == ASMIM_IMMEDIATE_ADDRESS)
		{
			data.mRegs[CPU_REG_A].mValue = mAddress;
			data.mRegs[CPU_REG_A].mLinkerObject = mLinkerObject;
			data.mRegs[CPU_REG_A].mFlags = mFlags;
			data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE_ADDRESS;
			data.mRegs[CPU_REG_Z].Reset();
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_LDX:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue;
				data.mRegs[CPU_REG_X].mValue = t;
				data.mRegs[CPU_REG_X].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_X].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else if (mMode == ASMIM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_X].mValue = mAddress;
			data.mRegs[CPU_REG_X].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_X].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_LDY:
		if (reg >= 0)
		{
			if (data.mRegs[reg].mMode == NRDM_IMMEDIATE)
			{
				int	t = data.mRegs[reg].mValue;
				data.mRegs[CPU_REG_Y].mValue = t;
				data.mRegs[CPU_REG_Y].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = t;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[CPU_REG_Y].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else if (mMode == ASMIM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Y].mValue = mAddress;
			data.mRegs[CPU_REG_Y].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
		}
		else
		{
			data.mRegs[CPU_REG_Y].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_STA:
		if (reg >= 0)
		{
			if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE || data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE_ADDRESS)
			{
				data.mRegs[reg] = data.mRegs[CPU_REG_A];
			}
			else
			{
				data.mRegs[reg].Reset();
			}
		}
		break;

	case ASMIT_STX:
		if (reg >= 0)
		{
			if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[reg].mValue = data.mRegs[CPU_REG_X].mValue;
				data.mRegs[reg].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
			}
		}
		break;

	case ASMIT_STY:
		if (reg >= 0)
		{
			if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[reg].mValue = data.mRegs[CPU_REG_Y].mValue;
				data.mRegs[reg].mMode = NRDM_IMMEDIATE;
			}
			else
			{
				data.mRegs[reg].Reset();
			}
		}
		break;
	}
}

bool NativeCodeInstruction::ValueForwarding(NativeRegisterDataSet& data, AsmInsType& carryop)
{
	bool	changed = false;

	carryop = ASMIT_NOP;

	mFlags &= ~NCIF_YZERO;

	if ((data.mRegs[CPU_REG_Y].mMode & NRDM_IMMEDIATE) && (data.mRegs[CPU_REG_Y].mValue == 0))
		mFlags |= NCIF_YZERO;

	if (mType == ASMIT_JSR)
	{
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_X].Reset();
		data.mRegs[CPU_REG_Y].Reset();

		for (int i = 0; i < 4; i++)
		{
			data.ResetZeroPage(BC_REG_ACCU + i);
			data.ResetZeroPage(BC_REG_WORK + i);
			data.ResetZeroPage(BC_REG_ADDR + i);
		}
		data.ResetZeroPage(BC_REG_WORK_Y);

		if (!(mFlags & NCIF_RUNTIME) || (mFlags & NCIF_FEXEC))
		{
			for (int i = BC_REG_TMP; i < BC_REG_TMP_SAVED; i++)
				data.ResetZeroPage(i);
		}

		return false;
	}

	if (data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mValue == 0)
	{
		switch (mType)
		{
		case ASMIT_ROL:
			mType = ASMIT_ASL;
			changed = true;
			break;
		case ASMIT_ROR:
			mType = ASMIT_LSR;
			changed = true;
			break;
		}
	}

	switch (mType)
	{
	case ASMIT_CLC:
		data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
		data.mRegs[CPU_REG_C].mValue = 0;
		break;
	case ASMIT_SEC:
		data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
		data.mRegs[CPU_REG_C].mValue = 1;
		break;

	case ASMIT_ROL:
	case ASMIT_ROR:
		if (mMode == ASMIM_IMPLIED)
			data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_ASL:
	case ASMIT_LSR:
		if (mMode == ASMIM_IMPLIED)
		{
			if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == 0 && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_CLC;				
				data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_C].mValue = 0;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_C].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_INC:
	case ASMIT_DEC:
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_LDA:
		if (mMode == ASMIM_IMMEDIATE)
		{
			if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				mMode = ASMIM_IMPLIED;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_A].mValue = mAddress;
			}

			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
		}
		else if (mMode == ASMIM_IMMEDIATE_ADDRESS)
		{
			data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE_ADDRESS;
			data.mRegs[CPU_REG_A].mValue = mAddress;
			data.mRegs[CPU_REG_A].mLinkerObject = mLinkerObject;
			data.mRegs[CPU_REG_A].mFlags = mFlags;
			data.mRegs[CPU_REG_Z].Reset();
		}
		else
		{
			if (mMode != ASMIM_ZERO_PAGE && mMode != ASMIM_ABSOLUTE)
				data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_STA:
		if (mMode == ASMIM_ZERO_PAGE && data.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE && mAddress == data.mRegs[CPU_REG_A].mValue)
		{
			mType = ASMIT_NOP;
			mMode = ASMIM_IMPLIED;
			changed = true;
		}
		break;
	case ASMIT_STX:
		if (mMode == ASMIM_ZERO_PAGE && data.mRegs[CPU_REG_X].mMode == NRDM_ZERO_PAGE && mAddress == data.mRegs[CPU_REG_X].mValue)
		{
			mType = ASMIT_NOP;
			mMode = ASMIM_IMPLIED;
			changed = true;
		}
		break;
	case ASMIT_STY:
		if (mMode == ASMIM_ZERO_PAGE && data.mRegs[CPU_REG_Y].mMode == NRDM_ZERO_PAGE && mAddress == data.mRegs[CPU_REG_Y].mValue)
		{
			mType = ASMIT_NOP;
			mMode = ASMIM_IMPLIED;
			changed = true;
		}
		break;

	case ASMIT_ADC:
		if (data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_C].mValue == 0)
		{
			if (mMode == ASMIM_IMMEDIATE && mAddress == 0)
			{
				mType = ASMIT_ORA;
				changed = true;
			}
			else if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == 0)
			{
				mType = ASMIT_LDA;
				changed = true;
			}
		}

		data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_SBC:
		if (data.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
		{
			if (mMode == ASMIM_IMMEDIATE && mAddress == 0 && data.mRegs[CPU_REG_C].mValue == 1)
			{
				mType = ASMIT_ORA;
				changed = true;
			}
			else if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
			{
				int	t = (mAddress ^ 0xff) + data.mRegs[CPU_REG_A].mValue + data.mRegs[CPU_REG_C].mValue;

				mType = ASMIT_LDA;
				mAddress = t & 0xff;

				int c = t >= 256;

				if (t && !data.mRegs[CPU_REG_C].mValue)
					carryop = ASMIT_SEC;
				else if (!t && data.mRegs[CPU_REG_C].mValue)
					carryop = ASMIT_CLC;

				changed = true;
			}
			else if (mMode == ASMIM_IMMEDIATE_ADDRESS && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE_ADDRESS && mLinkerObject == data.mRegs[CPU_REG_A].mLinkerObject)
			{
				int	t;
				if (mFlags & NCIF_LOWER)
					t = (mAddress ^ 0xffff) + data.mRegs[CPU_REG_A].mValue + data.mRegs[CPU_REG_C].mValue;
				else
					t = ((mAddress ^ 0xffff) >> 8) + data.mRegs[CPU_REG_A].mValue + data.mRegs[CPU_REG_C].mValue;

				mType = ASMIT_LDA;
				mMode = ASMIM_IMMEDIATE;
				mAddress = t & 0xff;

				int c = t >= 256;

				if (t && !data.mRegs[CPU_REG_C].mValue)
					carryop = ASMIT_SEC;
				else if (!t && data.mRegs[CPU_REG_C].mValue)
					carryop = ASMIT_CLC;

				changed = true;
			}
		}

		data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_CMP:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue - mAddress;
			data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_C].mValue = data.mRegs[CPU_REG_A].mValue >= mAddress;
		}
		else if (mMode == ASMIM_IMMEDIATE && mAddress == 0)
		{
			data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_C].mValue = 1;			
			data.mRegs[CPU_REG_Z].Reset();
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_CPX:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_X].mValue - mAddress;
			data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_C].mValue = data.mRegs[CPU_REG_X].mValue >= mAddress;
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_CPY:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_Y].mValue - mAddress;
			data.mRegs[CPU_REG_C].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_C].mValue = data.mRegs[CPU_REG_Y].mValue >= mAddress;
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_ORA:
	case ASMIT_EOR:
	case ASMIT_AND:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			if (mType == ASMIT_ORA)
				mAddress |= data.mRegs[CPU_REG_A].mValue;
			else if (mType == ASMIT_AND)
				mAddress &= data.mRegs[CPU_REG_A].mValue;
			else if (mType == ASMIT_EOR)
				mAddress ^= data.mRegs[CPU_REG_A].mValue;
			mType = ASMIT_LDA;
			data.mRegs[CPU_REG_A].mValue = mAddress;
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
			changed = true;
		}
		else if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_A].mValue == 0)
		{
			if (mType == ASMIT_ORA || mType == ASMIT_EOR)
			{
				mType = ASMIT_LDA;
				data.mRegs[CPU_REG_A].Reset();
				data.mRegs[CPU_REG_Z].Reset();
			}
			else
			{
				mType = ASMIT_LDA;
				mMode = ASMIM_IMMEDIATE;
				mAddress = 0;

				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = 0;
			}
			changed = true;
		}
		else
		{
			data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_LDX:
		if (mMode == ASMIM_IMMEDIATE)
		{
			if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_X].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				mMode = ASMIM_IMPLIED;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_X].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_X].mValue = mAddress;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = mAddress;
			}
		}
		else
		{
			if (mMode != ASMIM_ZERO_PAGE)
				data.mRegs[CPU_REG_X].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_INX:
	case ASMIT_DEX:
		data.mRegs[CPU_REG_X].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_LDY:
		if (mMode == ASMIM_IMMEDIATE)
		{
			if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE && data.mRegs[CPU_REG_Y].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				mMode = ASMIM_IMPLIED;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_Y].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Y].mValue = mAddress;
				data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
				data.mRegs[CPU_REG_Z].mValue = mAddress;
			}
		}
		else
		{
			if (mMode != ASMIM_ZERO_PAGE)
				data.mRegs[CPU_REG_Y].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_INY:
	case ASMIT_DEY:
		data.mRegs[CPU_REG_Y].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_TXA:
		data.mRegs[CPU_REG_A] = data.mRegs[CPU_REG_X];
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TYA:
		data.mRegs[CPU_REG_A] = data.mRegs[CPU_REG_Y];
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TAX:
		data.mRegs[CPU_REG_X] = data.mRegs[CPU_REG_A];
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TAY:
		data.mRegs[CPU_REG_Y] = data.mRegs[CPU_REG_A];
		if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE)
		{
			data.mRegs[CPU_REG_Z].mMode = NRDM_IMMEDIATE;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	}

	if (mMode == ASMIM_ABSOLUTE_X && data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
	{
		mMode = ASMIM_ABSOLUTE;
		mAddress += data.mRegs[CPU_REG_X].mValue;
		changed = true;
	}
	else if (mMode == ASMIM_ABSOLUTE_Y && data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
	{
		mMode = ASMIM_ABSOLUTE;
		mAddress += data.mRegs[CPU_REG_Y].mValue;
		changed = true;
	}

	if (mMode == ASMIM_ABSOLUTE_X && data.mRegs[CPU_REG_X].SameData(data.mRegs[CPU_REG_Y]) && HasAsmInstructionMode(mType, ASMIM_ABSOLUTE_Y))
	{
		mMode = ASMIM_ABSOLUTE_Y;
		changed = true;
	}

	if (mMode == ASMIM_ZERO_PAGE)
	{
		switch (mType)
		{
		case ASMIT_LDA:
			if (data.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_A].mValue == mAddress)
			{
				if (mLive & LIVE_CPU_REG_Z)
				{
					mType = ASMIT_ORA;
					mMode = ASMIM_IMMEDIATE;
					mAddress = 0;
				}
				else
				{
					mType = ASMIT_NOP;
					mMode = ASMIM_IMPLIED;
				}
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[CPU_REG_A] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_A].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE_ADDRESS)
			{
				data.mRegs[CPU_REG_A] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_A].mValue;
				mLinkerObject = data.mRegs[CPU_REG_A].mLinkerObject;
				mFlags = (mFlags & ~(NCIF_LOWER | NCIF_UPPER)) | (data.mRegs[CPU_REG_A].mFlags & (NCIF_LOWER | NCIF_UPPER));
				mMode = ASMIM_IMMEDIATE_ADDRESS;
				changed = true;
			}
			else if (data.mRegs[mAddress].SameData(data.mRegs[CPU_REG_A]))
			{
				if (mLive & LIVE_CPU_REG_Z)
				{
					mType = ASMIT_ORA;
					mMode = ASMIM_IMMEDIATE;
					mAddress = 0;
				}
				else
				{
					mType = ASMIT_NOP;
					mMode = ASMIM_IMPLIED;
				}
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_ZERO_PAGE)
			{
				data.mRegs[CPU_REG_A] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_A].mValue;
				changed = true;
			}
			else if (data.mRegs[CPU_REG_X].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_X].mValue == mAddress)
			{
				mType = ASMIT_TXA;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_A] = data.mRegs[CPU_REG_X];
				changed = true;
			}
#if 1
			else if (data.mRegs[CPU_REG_Y].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_Y].mValue == mAddress)
			{	
				mType = ASMIT_TYA;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_A] = data.mRegs[CPU_REG_Y];
				changed = true;
			}
#endif
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
				{
					data.mRegs[CPU_REG_A].mMode = NRDM_IMMEDIATE;
					data.mRegs[CPU_REG_A].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_A].mMode = NRDM_ZERO_PAGE;
					data.mRegs[CPU_REG_A].mValue = mAddress;
				}
			}
			break;

		case ASMIT_LDX:
			if (data.mRegs[CPU_REG_X].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_X].mValue == mAddress)
			{
				mType = ASMIT_NOP;
				mMode = ASMIM_IMPLIED;
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[CPU_REG_X] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_X].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_A].mValue == mAddress)
			{
				mType = ASMIT_TAX;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_X] = data.mRegs[CPU_REG_A];
				changed = true;
			}
			else if (data.mRegs[mAddress].SameData(data.mRegs[CPU_REG_A]))
			{
				mType = ASMIT_TAX;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_X] = data.mRegs[CPU_REG_A];
				changed = true;
			}
#if 1
			else if (data.mRegs[mAddress].mMode == NRDM_ZERO_PAGE)
			{
				data.mRegs[CPU_REG_X] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_X].mValue;
				changed = true;
			}
#endif
			else
			{
				data.mRegs[CPU_REG_X].Reset();
				if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
				{
					data.mRegs[CPU_REG_X].mMode = NRDM_IMMEDIATE;
					data.mRegs[CPU_REG_X].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_X].mMode = NRDM_ZERO_PAGE;
					data.mRegs[CPU_REG_X].mValue = mAddress;
				}
			}
			break;

		case ASMIT_LDY:
			if (data.mRegs[CPU_REG_Y].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_Y].mValue == mAddress)
			{
				mType = ASMIT_NOP;
				mMode = ASMIM_IMPLIED;
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[CPU_REG_Y] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_Y].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE && data.mRegs[CPU_REG_A].mValue == mAddress)
			{
				mType = ASMIT_TAY;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_Y] = data.mRegs[CPU_REG_A];
				changed = true;
			}
			else if (data.mRegs[mAddress].SameData(data.mRegs[CPU_REG_A]))
			{
				mType = ASMIT_TAY;
				mMode = ASMIM_IMPLIED;
				data.mRegs[CPU_REG_Y] = data.mRegs[CPU_REG_A];
				changed = true;
			}
#if 1
			else if (data.mRegs[mAddress].mMode == NRDM_ZERO_PAGE)
			{
				data.mRegs[CPU_REG_Y] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_Y].mValue;
				changed = true;
			}
#endif
			else
			{
				data.mRegs[CPU_REG_Y].Reset();
				if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
				{
					data.mRegs[CPU_REG_Y].mMode = NRDM_IMMEDIATE;
					data.mRegs[CPU_REG_Y].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_Y].mMode = NRDM_ZERO_PAGE;
					data.mRegs[CPU_REG_Y].mValue = mAddress;
				}
			}
			break;

		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_AND:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_CMP:
		case ASMIT_CPX:
		case ASMIT_CPY:
			if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE)
			{
				mAddress = data.mRegs[mAddress].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_ZERO_PAGE)
			{
				mAddress = data.mRegs[mAddress].mValue;
				changed = true;
			}
			else if (data.mRegs[mAddress].mMode == NRDM_ABSOLUTE)
			{
				mMode = ASMIM_ABSOLUTE;
				mLinkerObject = data.mRegs[mAddress].mLinkerObject;
				mAddress = data.mRegs[mAddress].mValue;
				changed = true;
			}
			break;

		case ASMIT_STA:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE || data.mRegs[CPU_REG_A].mMode == NRDM_IMMEDIATE_ADDRESS)
			{
				data.mRegs[mAddress] = data.mRegs[CPU_REG_A];
			}
			else if (data.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE)
			{
				data.mRegs[mAddress].mMode = NRDM_ZERO_PAGE;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_A].mValue;
			}
			else if (data.mRegs[CPU_REG_A].mMode == NRDM_ABSOLUTE)
			{
				data.mRegs[mAddress] = data.mRegs[CPU_REG_A];
			}
			else
			{
				data.mRegs[CPU_REG_A].mMode = NRDM_ZERO_PAGE;
				data.mRegs[CPU_REG_A].mValue = mAddress;
			}
			break;
		case ASMIT_STX:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_X].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[mAddress].mMode = NRDM_IMMEDIATE;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_X].mValue;
			}
			else
			{
				data.mRegs[CPU_REG_X].mMode = NRDM_ZERO_PAGE;
				data.mRegs[CPU_REG_X].mValue = mAddress;
			}
			break;
		case ASMIT_STY:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_Y].mMode == NRDM_IMMEDIATE)
			{
				data.mRegs[mAddress].mMode = NRDM_IMMEDIATE;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_Y].mValue;
			}
			else
			{
				data.mRegs[CPU_REG_Y].mMode = NRDM_ZERO_PAGE;
				data.mRegs[CPU_REG_Y].mValue = mAddress;
			}
			break;
		case ASMIT_INC:
		case ASMIT_DEC:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_ROL:
		case ASMIT_ROR:
			data.ResetZeroPage(mAddress);			
			break;
		}
	}
	else if (mMode == ASMIM_INDIRECT_Y)
	{
		if (data.mRegs[mAddress].mMode == NRDM_ZERO_PAGE && data.mRegs[mAddress + 1].mMode == NRDM_ZERO_PAGE && data.mRegs[mAddress].mValue + 1 == data.mRegs[mAddress + 1].mValue)
		{
			mAddress = data.mRegs[mAddress].mValue;
		}
		else if (data.mRegs[mAddress].mMode == NRDM_IMMEDIATE_ADDRESS && data.mRegs[mAddress + 1].mMode == NRDM_IMMEDIATE_ADDRESS && data.mRegs[mAddress].mLinkerObject == data.mRegs[mAddress + 1].mLinkerObject)
		{
			mMode = ASMIM_ABSOLUTE_Y;
			mLinkerObject = data.mRegs[mAddress].mLinkerObject;
			mAddress = data.mRegs[mAddress + 1].mValue;
		}

		if (ChangesAddress())
			data.ResetIndirect();
	}
	else if (mMode == ASMIM_ABSOLUTE)
	{
		switch (mType)
		{
		case ASMIT_LDA:
			if (!(mFlags & NCIF_VOLATILE))
			{
				if (data.mRegs[CPU_REG_A].mMode == NRDM_ABSOLUTE && data.mRegs[CPU_REG_A].mLinkerObject == mLinkerObject && data.mRegs[CPU_REG_A].mValue == mAddress)
				{
					if (mLive & LIVE_CPU_REG_Z)
					{
						mType = ASMIT_ORA;
						mMode = ASMIM_IMMEDIATE;
						mAddress = 0;
					}
					else
					{
						mType = ASMIT_NOP;
						mMode = ASMIM_IMPLIED;
					}
					changed = true;
				}
				else
				{
					data.mRegs[CPU_REG_A].mMode = NRDM_ABSOLUTE;
					data.mRegs[CPU_REG_A].mLinkerObject = mLinkerObject;
					data.mRegs[CPU_REG_A].mValue = mAddress;
					data.mRegs[CPU_REG_A].mFlags = mFlags;
				}
			}
			else
				data.mRegs[CPU_REG_A].Reset();
			break;
		default:
			if (ChangesAddress())
				data.ResetAbsolute(mLinkerObject, mAddress);
		}
	}
	else if (mMode == ASMIM_ABSOLUTE_X || mMode == ASMIM_ABSOLUTE_Y)
	{
		if (ChangesAddress())
			data.ResetAbsolute(mLinkerObject, mAddress);
	}

	return changed;
}

void NativeCodeInstruction::FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps)
{
	// check runtime calls

	if (mType == ASMIT_JSR)
	{
#if 1
		if (mFlags & NCIF_RUNTIME)
		{
			for (int i = 0; i < 4; i++)
			{
				if (!providedTemps[BC_REG_ACCU + i])
					requiredTemps += BC_REG_ACCU + i;
				if (!providedTemps[BC_REG_WORK + i])
					requiredTemps += BC_REG_WORK + i;
				if (!providedTemps[BC_REG_ADDR + i])
					requiredTemps += BC_REG_ADDR + i;
			}
			if (mFlags & NCIF_USE_ZP_32_X)
			{
				for (int i = 0; i < 4; i++)
				{
					if (!providedTemps[mParam + i])
						requiredTemps += mParam + i;
				}
			}
			if (mFlags & NCIF_USE_CPU_REG_A)
			{
				if (!providedTemps[CPU_REG_A])
					requiredTemps += CPU_REG_A;
			}

			if (mFlags & NCIF_FEXEC)
			{
				for (int i = BC_REG_FPARAMS; i < BC_REG_FPARAMS_END; i++)
					if (!providedTemps[i])
						requiredTemps += i;
			}
		}
		else
		{
			if (mLinkerObject)
			{
				for (int i = 0; i < mLinkerObject->mNumTemporaries; i++)
				{
					for (int j = 0; j < mLinkerObject->mTempSizes[i]; j++)
					{
						if (!providedTemps[mLinkerObject->mTemporaries[i] + j])
							requiredTemps += mLinkerObject->mTemporaries[i] + j;
					}
				}
			}
		}
#endif
		for (int i = 0; i < 4; i++)
		{
			providedTemps += BC_REG_ACCU + i;
			providedTemps += BC_REG_WORK + i;
			providedTemps += BC_REG_ADDR + i;
		}

		providedTemps += CPU_REG_A;
		providedTemps += CPU_REG_X;
		providedTemps += CPU_REG_Y;
		providedTemps += CPU_REG_C;
		providedTemps += CPU_REG_Z;
		return;
	}

	if (mType == ASMIT_RTS)
	{
#if 1
		if (mFlags & NCIF_LOWER)
		{
			if (!providedTemps[BC_REG_ACCU + 0]) requiredTemps += BC_REG_ACCU + 0;

			if (mFlags & NCIF_UPPER)
			{
				if (!providedTemps[BC_REG_ACCU + 1]) requiredTemps += BC_REG_ACCU + 1;

				if (mFlags & NCIF_LONG)
				{
					if (!providedTemps[BC_REG_ACCU + 2]) requiredTemps += BC_REG_ACCU + 2;
					if (!providedTemps[BC_REG_ACCU + 3]) requiredTemps += BC_REG_ACCU + 3;
				}
			}
		}
#endif
#if 0
		for (int i = 0; i < 4; i++)
		{
			if (!providedTemps[BC_REG_ACCU + i])
				requiredTemps += BC_REG_ACCU + i;
		}
#endif
		if (!providedTemps[BC_REG_STACK])
			requiredTemps += BC_REG_STACK;
		if (!providedTemps[BC_REG_STACK + 1])
			requiredTemps += BC_REG_STACK + 1;
		if (!providedTemps[BC_REG_LOCALS])
			requiredTemps += BC_REG_LOCALS;
		if (!providedTemps[BC_REG_LOCALS + 1])
			requiredTemps += BC_REG_LOCALS + 1;

		return;
	}

	// check index

	switch (mMode)
	{
	case ASMIM_ZERO_PAGE_X:
	case ASMIM_INDIRECT_X:
	case ASMIM_ABSOLUTE_X:
		if (!providedTemps[CPU_REG_X])
			requiredTemps += CPU_REG_X;
		break;

	case ASMIM_ZERO_PAGE_Y:
	case ASMIM_ABSOLUTE_Y:
		if (!providedTemps[CPU_REG_Y])
			requiredTemps += CPU_REG_Y;
		break;

	case ASMIM_INDIRECT_Y:
		if (!providedTemps[CPU_REG_Y])
			requiredTemps += CPU_REG_Y;
		if (!providedTemps[mAddress])
			requiredTemps += mAddress;
		if (!providedTemps[mAddress + 1])
			requiredTemps += mAddress + 1;
		break;
	}

	// check carry flags

	switch (mType)
	{
	case ASMIT_ADC:
	case ASMIT_SBC:
	case ASMIT_ROL:
	case ASMIT_ROR:
		if (!providedTemps[CPU_REG_C])
			requiredTemps += CPU_REG_C;
		providedTemps += CPU_REG_C;
		break;
	case ASMIT_CMP:
	case ASMIT_ASL:
	case ASMIT_LSR:
	case ASMIT_CPX:
	case ASMIT_CPY:
	case ASMIT_CLC:
	case ASMIT_SEC:
		providedTemps += CPU_REG_C;
		break;
	case ASMIT_BCC:
	case ASMIT_BCS:
		if (!providedTemps[CPU_REG_C])
			requiredTemps += CPU_REG_C;
		break;
	case ASMIT_BEQ:
	case ASMIT_BNE:
	case ASMIT_BPL:
	case ASMIT_BMI:
		if (!providedTemps[CPU_REG_Z])
			requiredTemps += CPU_REG_Z;
		break;
	}

	// check zero flag

	switch (mType)
	{
	case ASMIT_ADC:
	case ASMIT_SBC:
	case ASMIT_ROL:
	case ASMIT_ROR:
	case ASMIT_INC:
	case ASMIT_DEC:
	case ASMIT_CMP:
	case ASMIT_CPX:
	case ASMIT_CPY:
	case ASMIT_ASL:
	case ASMIT_LSR:
	case ASMIT_ORA:
	case ASMIT_EOR:
	case ASMIT_AND:
	case ASMIT_LDA:
	case ASMIT_LDX:
	case ASMIT_LDY:
	case ASMIT_BIT:
		providedTemps += CPU_REG_Z;
		break;
	}

	// check CPU register

	switch (mType)
	{
	case ASMIT_ROL:
	case ASMIT_ROR:
	case ASMIT_ASL:
	case ASMIT_LSR:
		if (mMode == ASMIM_IMPLIED)
		{
			if (!providedTemps[CPU_REG_A])
				requiredTemps += CPU_REG_A;
			providedTemps += CPU_REG_A;
		}
		break;

	case ASMIT_LDA:
		providedTemps += CPU_REG_A;
		break;

	case ASMIT_CMP:
	case ASMIT_STA:
		if (!providedTemps[CPU_REG_A])
			requiredTemps += CPU_REG_A;
		break;
	case ASMIT_CPX:
	case ASMIT_STX:
		if (!providedTemps[CPU_REG_X])
			requiredTemps += CPU_REG_X;
		break;
	case ASMIT_CPY:
	case ASMIT_STY:
		if (!providedTemps[CPU_REG_Y])
			requiredTemps += CPU_REG_Y;
		break;

	case ASMIT_ADC:
	case ASMIT_SBC:
	case ASMIT_ORA:
	case ASMIT_EOR:
	case ASMIT_AND:
		if (!providedTemps[CPU_REG_A])
			requiredTemps += CPU_REG_A;
		providedTemps += CPU_REG_A;
		break;
	case ASMIT_LDX:
		providedTemps += CPU_REG_X;
		break;
	case ASMIT_INX:
	case ASMIT_DEX:
		if (!providedTemps[CPU_REG_X])
			requiredTemps += CPU_REG_X;
		providedTemps += CPU_REG_X;
		break;
	case ASMIT_LDY:
		providedTemps += CPU_REG_Y;
		break;
	case ASMIT_INY:
	case ASMIT_DEY:
		if (!providedTemps[CPU_REG_Y])
			requiredTemps += CPU_REG_Y;
		providedTemps += CPU_REG_Y;
		break;

	case ASMIT_TAX:
		if (!providedTemps[CPU_REG_A])
			requiredTemps += CPU_REG_A;
		providedTemps += CPU_REG_X;
		break;
	case ASMIT_TAY:
		if (!providedTemps[CPU_REG_A])
			requiredTemps += CPU_REG_A;
		providedTemps += CPU_REG_Y;
		break;
	case ASMIT_TXA:
		if (!providedTemps[CPU_REG_X])
			requiredTemps += CPU_REG_X;
		providedTemps += CPU_REG_A;
		break;
	case ASMIT_TYA:
		if (!providedTemps[CPU_REG_Y])
			requiredTemps += CPU_REG_Y;
		providedTemps += CPU_REG_A;
		break;
	}

	if (mMode == ASMIM_ZERO_PAGE)
	{
		switch (mType)
		{
		case ASMIT_STA:
		case ASMIT_STX:
		case ASMIT_STY:
			providedTemps += mAddress;
			break;
		case ASMIT_ROL:
		case ASMIT_ROR:
		case ASMIT_ASL:
		case ASMIT_LSR:
		case ASMIT_INC:
		case ASMIT_DEC:
			if (!providedTemps[mAddress])
				requiredTemps += mAddress;
			providedTemps += mAddress;
			break;
		default:
			if (!providedTemps[mAddress])
				requiredTemps += mAddress;
		}
	}
}

void NativeCodeInstruction::CopyMode(const NativeCodeInstruction& ins)
{
	mMode = ins.mMode;
	mAddress = ins.mAddress;
	mLinkerObject = ins.mLinkerObject;
	mFlags = (mFlags & ~(NCIF_LOWER | NCIF_UPPER)) | (ins.mFlags & (NCIF_LOWER | NCIF_UPPER));
}

void NativeCodeInstruction::Assemble(NativeCodeBasicBlock* block)
{
	if (mType == ASMIT_BYTE)
		block->PutByte(mAddress);
	else if (mType == ASMIT_JSR && mLinkerObject && (mLinkerObject->mFlags & LOBJF_INLINE))
	{
		int	pos = block->mCode.Size();
		int size = mLinkerObject->mSize;

		// skip RTS on embedding
		if (mLinkerObject->mData[size - 1] == 0x60)
			size--;

		for (int i = 0; i < size; i++)
			block->PutByte(mLinkerObject->mData[i]);
		for (int i = 0; i < mLinkerObject->mReferences.Size(); i++)
		{
			LinkerReference	rl = *(mLinkerObject->mReferences[i]);
			if (rl.mFlags & LREF_TEMPORARY)
			{
				block->mCode[pos + rl.mOffset] += mLinkerObject->mTemporaries[rl.mRefOffset];
			}
			else
			{
				rl.mOffset += pos;
				if (rl.mRefObject == rl.mObject)
				{
					rl.mRefObject = nullptr;
					rl.mRefOffset += pos;
					rl.mFlags |= LREF_INBLOCK;
				}

				block->mRelocations.Push(rl);
			}
		}
	}
	else
	{
		if (mType == ASMIT_JSR && (mFlags & NCIF_USE_ZP_32_X))
		{
			block->PutByte(AsmInsOpcodes[ASMIT_LDX][ASMIM_IMMEDIATE]);
			block->PutByte(mParam);
		}

		AsmInsMode	mode = mMode;

		if (mode == ASMIM_ABSOLUTE && !mLinkerObject && mAddress < 256 && HasAsmInstructionMode(mType, ASMIM_ZERO_PAGE))
			mode = ASMIM_ZERO_PAGE;
		else if (mode == ASMIM_ABSOLUTE_X && !mLinkerObject && mAddress < 256 && HasAsmInstructionMode(mType, ASMIM_ZERO_PAGE_X))
			mode = ASMIM_ZERO_PAGE_X;

		if (mode == ASMIM_IMMEDIATE_ADDRESS)
		{
			assert((mFlags & (NCIF_LOWER | NCIF_UPPER)) != (NCIF_LOWER | NCIF_UPPER));
			assert(HasAsmInstructionMode(mType, ASMIM_IMMEDIATE));
			block->PutByte(AsmInsOpcodes[mType][ASMIM_IMMEDIATE]);
		}
		else
		{
			assert(HasAsmInstructionMode(mType, mode));
			block->PutByte(AsmInsOpcodes[mType][mode]);
		}

		switch (mode)
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
			block->PutByte(uint16(mAddress));
			break;
		case ASMIM_IMMEDIATE_ADDRESS:
			if (mLinkerObject)
			{
				LinkerReference		rl;
				rl.mOffset = block->mCode.Size();
				rl.mFlags = 0;
				if (mFlags & NCIF_LOWER)
					rl.mFlags |= LREF_LOWBYTE;
				if (mFlags & NCIF_UPPER)
					rl.mFlags |= LREF_HIGHBYTE;
				rl.mRefObject = mLinkerObject;

				rl.mRefObject = mLinkerObject;
				rl.mRefOffset = mAddress;

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
			if (mLinkerObject)
			{
				LinkerReference		rl;
				rl.mOffset = block->mCode.Size();

				rl.mRefObject = mLinkerObject;
				rl.mRefOffset = mAddress;
				if (mFlags & NCIF_LOWER)
				{
					rl.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
				}
				else
				{
					rl.mFlags = LREF_HIGHBYTE;
					rl.mOffset++;
				}

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
}


void NativeCodeBasicBlock::PutByte(uint8 code)
{
	this->mCode.Push(code);
}

void NativeCodeBasicBlock::PutWord(uint16 code)
{
	this->mCode.Push((uint8)(code & 0xff));
	this->mCode.Push((uint8)(code >> 8));
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


int NativeCodeBasicBlock::PutJump(NativeCodeProcedure* proc, NativeCodeBasicBlock* target)
{
	if (target->mIns.Size() == 1 && target->mIns[0].mType == ASMIT_RTS)
	{
		PutByte(0x60);
		return 1;
	}
	else
	{
		PutByte(0x4c);

		LinkerReference		rl;
		rl.mObject = nullptr;
		rl.mOffset = mCode.Size();
		rl.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
		rl.mRefObject = nullptr;
		rl.mRefOffset = target->mOffset;
		mRelocations.Push(rl);

		PutWord(0);
		return 3;
	}
}

int NativeCodeBasicBlock::JumpByteSize(NativeCodeBasicBlock* target)
{
	if (target->mIns.Size() == 1 && target->mIns[0].mType == ASMIT_RTS)
		return 1;
	else
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

		LinkerReference		rl;
		rl.mObject = nullptr;
		rl.mOffset = mCode.Size();
		rl.mFlags = LREF_LOWBYTE | LREF_HIGHBYTE;
		rl.mRefObject = nullptr;
		rl.mRefOffset = mOffset + mCode.Size() + offset - 3;
		mRelocations.Push(rl);
		
		PutWord(0);
		return 5;
	}
}

void NativeCodeBasicBlock::LoadConstantToReg(InterCodeProcedure * proc, const InterInstruction * ins, InterType type, int reg)
{
	if (type == IT_FLOAT)
	{
		union { float f; unsigned int v; } cc;
		cc.f = ins->mConst.mFloatConst;

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
	}
	else if (type == IT_POINTER)
	{
		if (ins->mConst.mMemory == IM_GLOBAL)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mConst.mIntConst, ins->mConst.mLinkerObject, NCIF_LOWER));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mConst.mIntConst, ins->mConst.mLinkerObject, NCIF_UPPER));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mConst.mMemory == IM_ABSOLUTE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mConst.mIntConst & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mConst.mIntConst >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mConst.mMemory == IM_FPARAM)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, BC_REG_FPARAMS + ins->mConst.mVarIndex + ins->mConst.mIntConst));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mConst.mMemory == IM_FRAME)
		{
			int	index = ins->mConst.mVarIndex + ins->mConst.mIntConst + 2;

			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mConst.mMemory == IM_LOCAL || ins->mConst.mMemory == IM_PARAM)
		{
			int	index = ins->mConst.mIntConst;
			int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
			if (ins->mConst.mMemory == IM_LOCAL)
				index += proc->mLocalVars[ins->mConst.mVarIndex]->mOffset;
			else
				index += ins->mConst.mVarIndex + proc->mLocalSize + 2;
			index += mFrameOffset;
			CheckFrameIndex(areg, index, 2);

			if (index != 0)
				mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, areg));
			if (index != 0)
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, areg + 1));
			if (index != 0)
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mConst.mMemory == IM_PROCEDURE)
		{
			NativeCodeInstruction	lins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mConst.mLinkerObject, NCIF_LOWER);
			NativeCodeInstruction	hins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mConst.mLinkerObject, NCIF_UPPER);

			mIns.Push(lins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(hins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
	}
	else if (type == IT_INT32)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mConst.mIntConst & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mConst.mIntConst >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mConst.mIntConst >> 16) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mConst.mIntConst >> 24) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mConst.mIntConst & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
		if (InterTypeSize[ins->mDst.mType] > 1)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mConst.mIntConst >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
	}

}

void NativeCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins)
{
	LoadConstantToReg(proc, ins, ins->mDst.mType, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]);
}

void NativeCodeBasicBlock::CheckFrameIndex(int& reg, int& index, int size, int treg)
{
	if (index < 0 || index + size > 256)
	{
		if (treg == 0)
			treg = BC_REG_ADDR;
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
		index = 0;
		reg = treg;
	}
}

void NativeCodeBasicBlock::StoreValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	uint32	flags = NCIF_LOWER | NCIF_UPPER;
	if (ins->mVolatile)
		flags |= NCIF_VOLATILE;

	if (ins->mSrc[0].mType == IT_FLOAT)
	{
		if (ins->mSrc[1].mTemp < 0)
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				union { float f; unsigned int v; } cc;
				cc.f = ins->mSrc[0].mFloatConst;

				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, ins->mSrc[1].mLinkerObject, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, nullptr, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 3));
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y,BC_REG_STACK));
				}
			}
			else
			{
				int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];

				if (ins->mSrc[0].mFinal && CheckPredAccuStore(sreg))
				{
					// cull previous store from accu to temp using direcrt forwarding from accu
					mIns.SetSize(mIns.Size() - 8);
					sreg = BC_REG_ACCU;
				}

				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, ins->mSrc[1].mLinkerObject, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, nullptr, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 3));
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					union { float f; unsigned int v; } cc;
					cc.f = ins->mSrc[0].mFloatConst;

					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
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
				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
				}
				else if (ins->mSrc[1].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
				}
				else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[1].mIntConst;
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
					else
						index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mSrc[1].mMemory == IM_FRAME)
				{
					int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
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
				if (InterTypeSize[ins->mSrc[0].mType] == 1)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 1);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 2)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 2);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 4)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 3));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 4);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
			}
			else
			{
				if (InterTypeSize[ins->mSrc[0].mType] == 1)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 1);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 2)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 2);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (InterTypeSize[ins->mSrc[0].mType] == 4)
				{
					if (ins->mSrc[1].mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, ins->mSrc[1].mLinkerObject, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, ins->mSrc[1].mLinkerObject, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 1, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 2, nullptr, flags));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSrc[1].mIntConst + 3, nullptr, flags));
					}
					else if (ins->mSrc[1].mMemory == IM_FPARAM)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 3));
					}
					else if (ins->mSrc[1].mMemory == IM_LOCAL || ins->mSrc[1].mMemory == IM_PARAM)
					{
						int	index = ins->mSrc[1].mIntConst;
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

						if (ins->mSrc[1].mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mSrc[1].mVarIndex]->mOffset;
						else
							index += ins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
						index += mFrameOffset;
						CheckFrameIndex(reg, index, 4);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mSrc[1].mMemory == IM_FRAME)
					{
						int	index = ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, InterTypeSize[ins->mSrc[0].mType]);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));

					if (InterTypeSize[ins->mSrc[0].mType] == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 4)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
				}
			}
			else
			{
				if (ins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	reg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
					int index = ins->mSrc[1].mIntConst;

					CheckFrameIndex(reg, index, InterTypeSize[ins->mSrc[0].mType]);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));

					if (InterTypeSize[ins->mSrc[0].mType] == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (InterTypeSize[ins->mSrc[0].mType] == 4)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
				}
			}
		}
	}

}

void NativeCodeBasicBlock::LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins)
{
	int size = InterTypeSize[wins->mSrc[0].mType];

	AsmInsMode	rmode = ASMIM_INDIRECT_Y;
	int	rindex = rins->mSrc[0].mIntConst;
	int rareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
	LinkerObject* rlobject = nullptr;

	uint32	rflags = NCIF_LOWER | NCIF_UPPER;
	if (rins->mVolatile)
		rflags |= NCIF_VOLATILE;

	switch (rins->mSrc[0].mMemory)
	{
	case IM_PARAM:
		rindex += rins->mSrc[0].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_LOCAL:
		rindex += proc->mLocalVars[rins->mSrc[0].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PROCEDURE:
	case IM_GLOBAL:
		rmode = ASMIM_ABSOLUTE;
		rlobject = rins->mSrc[0].mLinkerObject;
		rindex = rins->mSrc[0].mIntConst;
		break;
	case IM_FRAME:
		rindex = rins->mSrc[0].mVarIndex + rins->mSrc[0].mIntConst + 2;
		rareg = BC_REG_STACK;
		break;
	case IM_INDIRECT:
		rareg = BC_REG_TMP + proc->mTempOffset[rins->mSrc[0].mTemp];
		break;
	case IM_ABSOLUTE:
		rmode = ASMIM_ABSOLUTE;
		rindex = rins->mSrc[0].mIntConst;
		break;
	case IM_FPARAM:
		rmode = ASMIM_ZERO_PAGE;
		rareg = BC_REG_FPARAMS + rins->mSrc[0].mVarIndex + rins->mSrc[0].mIntConst;
		break;
	}

	AsmInsMode	wmode = ASMIM_INDIRECT_Y;
	int	windex = wins->mSrc[1].mIntConst;
	int wareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
	LinkerObject* wlobject = nullptr;

	uint32	wflags = NCIF_LOWER | NCIF_UPPER;
	if (wins->mVolatile)
		wflags |= NCIF_VOLATILE;

	switch (wins->mSrc[1].mMemory)
	{
	case IM_PARAM:
		windex += wins->mSrc[1].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_LOCAL:
		windex += proc->mLocalVars[wins->mSrc[1].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PROCEDURE:
	case IM_GLOBAL:
		wmode = ASMIM_ABSOLUTE;
		wlobject = wins->mSrc[1].mLinkerObject;
		windex = wins->mSrc[1].mIntConst;
		break;
	case IM_FRAME:
		windex = wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst + 2;
		wareg = BC_REG_STACK;
		break;
	case IM_INDIRECT:
		wareg = BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp];
		break;
	case IM_ABSOLUTE:
		wmode = ASMIM_ABSOLUTE;
		windex = wins->mSrc[1].mIntConst;
		break;
	case IM_FPARAM:
		wmode = ASMIM_ZERO_PAGE;
		wareg = BC_REG_FPARAMS + wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst;
		break;
	}

	if (rmode == ASMIM_INDIRECT_Y)
		CheckFrameIndex(rareg, rindex, size, BC_REG_ADDR);

	if (wmode == ASMIM_INDIRECT_Y)
		CheckFrameIndex(wareg, windex, size, BC_REG_ACCU);

	for (int i = 0; i < size; i++)
	{
		if (rmode == ASMIM_INDIRECT_Y)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rindex + i));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, rareg));
		}
		else if (rmode == ASMIM_ZERO_PAGE)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, rareg + i));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rindex + i, rlobject, rflags));

		if (wmode == ASMIM_INDIRECT_Y)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, windex + i));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, wareg));
		}
		else if (wmode == ASMIM_ZERO_PAGE)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, wareg + i));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, windex + i, wlobject, wflags));
	}
}

void NativeCodeBasicBlock::LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins)
{
	uint32	rflags = NCIF_LOWER | NCIF_UPPER;
	if (rins->mVolatile)
		rflags |= NCIF_VOLATILE;

	uint32	wflags = NCIF_LOWER | NCIF_UPPER;
	if (wins->mVolatile)
		wflags |= NCIF_VOLATILE;


	if (rins->mDst.mType == IT_FLOAT)
	{

	}
	else if (rins->mDst.mType == IT_POINTER)
	{

	}
	else
	{

		if (InterTypeSize[wins->mSrc[0].mType] == 1)
		{
			if (rins->mSrc[0].mTemp < 0)
			{
				if (rins->mSrc[0].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins->mSrc[0].mIntConst, rins->mSrc[0].mLinkerObject, rflags));
				}
				else if (rins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins->mSrc[0].mIntConst, nullptr, rflags));
				}
				else if (rins->mSrc[0].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + rins->mSrc[0].mVarIndex + rins->mSrc[0].mIntConst));
				}
				else if (rins->mSrc[0].mMemory == IM_LOCAL || rins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = rins->mSrc[0].mIntConst;
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (rins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[rins->mSrc[0].mVarIndex]->mOffset;
					else
						index += rins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				}
			}
			else
			{
				if (rins->mSrc[0].mMemory == IM_INDIRECT)
				{
					int	areg = BC_REG_TMP + proc->mTempOffset[rins->mSrc[0].mTemp];
					int index = rins->mSrc[0].mIntConst;

					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				}
			}

			if (wins->mSrc[1].mTemp < 0)
			{
				if (wins->mSrc[1].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins->mSrc[1].mIntConst, wins->mSrc[1].mLinkerObject, wflags));
				}
				else if (wins->mSrc[1].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins->mSrc[1].mIntConst, nullptr, wflags));
				}
				else if (wins->mSrc[1].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst));
				}
				else if (wins->mSrc[1].mMemory == IM_LOCAL || wins->mSrc[1].mMemory == IM_PARAM)
				{
					int	index = wins->mSrc[1].mIntConst;
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (wins->mSrc[1].mMemory == IM_LOCAL)
						index += proc->mLocalVars[wins->mSrc[1].mVarIndex]->mOffset;
					else
						index += wins->mSrc[1].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, areg));
				}
				else if (wins->mSrc[1].mMemory == IM_FRAME)
				{
					int	index = wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
			else
			{
				if (wins->mSrc[1].mMemory == IM_INDIRECT)
				{
					int	areg = BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp];
					int index = wins->mSrc[1].mIntConst;

					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, areg));
				}
			}
		}
	}
}

bool NativeCodeBasicBlock::LoadLoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins1, const InterInstruction* rins0, const InterInstruction* oins, const InterInstruction* wins)
{
	if (rins1->mSrc[0].mMemory == IM_INDIRECT && rins0->mSrc[0].mMemory == IM_INDIRECT && wins->mSrc[1].mMemory == IM_INDIRECT)
	{
		int size = InterTypeSize[wins->mSrc[0].mType];

		if (wins->mSrc[0].mFinal) 
		{
			if (wins->mSrc[0].mTemp == rins1->mSrc[0].mTemp || wins->mSrc[0].mTemp == rins0->mSrc[0].mTemp)
				return false;
		}

		switch (oins->mOperator)
		{
		case IA_ADD:
			mIns.Push(NativeCodeInstruction(ASMIT_CLC));
			break;
		default:
			return false;
		}


		for (int i = 0; i < size; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rins1->mSrc[0].mIntConst + i));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[rins1->mSrc[0].mTemp]));

			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rins0->mSrc[0].mIntConst + i));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[rins0->mSrc[0].mTemp]));

			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, wins->mSrc[1].mIntConst + i));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp]));

			if (!wins->mSrc[0].mFinal)
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp] + i));
		}

		return true;
	}
	else
		return false;
}

bool NativeCodeBasicBlock::LoadUnopStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, const InterInstruction* wins)
{
	int size = InterTypeSize[wins->mSrc[0].mType];

	AsmInsMode  ram = ASMIM_INDIRECT_Y, wam = ASMIM_INDIRECT_Y;
	bool		sfinal = wins->mSrc[0].mFinal;
	int			imm;
	AsmInsType	at;

	switch (oins->mOperator)
	{
	case IA_NEG:
		mIns.Push(NativeCodeInstruction(ASMIT_SEC));
		imm = 0x00;
		at = ASMIT_SBC;
		break;
	case IA_NOT:
		imm = 0xff;
		at = ASMIT_EOR;
		break;
	default:
		return false;
	}

	int	rindex = rins->mSrc[0].mIntConst;
	int rareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

	switch (rins->mSrc[0].mMemory)
	{
	case IM_INDIRECT:
		rareg = BC_REG_TMP + proc->mTempOffset[rins->mSrc[0].mTemp];
		break;
	case IM_LOCAL:
		rindex += proc->mLocalVars[rins->mSrc[0].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PARAM:
		rindex += rins->mSrc[0].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_FPARAM:
		ram = ASMIM_ZERO_PAGE;
		rareg = BC_REG_FPARAMS + rins->mSrc[0].mVarIndex + rins->mSrc[0].mIntConst;
		break;
	default:
		return false;
	}

	int	windex = wins->mSrc[1].mIntConst;
	int wareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

	switch (wins->mSrc[1].mMemory)
	{
	case IM_INDIRECT:
		wareg = BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp];
		break;
	case IM_LOCAL:
		windex += proc->mLocalVars[wins->mSrc[1].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PARAM:
		windex += wins->mSrc[1].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_FPARAM:
		wam = ASMIM_ZERO_PAGE;
		wareg = BC_REG_FPARAMS + +wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst;
		break;
	default:
		return false;
	}

	uint32	rflags = NCIF_LOWER | NCIF_UPPER;
	if (rins->mVolatile)
		rflags |= NCIF_VOLATILE;

	uint32	wflags = NCIF_LOWER | NCIF_UPPER;
	if (wins->mVolatile)
		wflags |= NCIF_VOLATILE;

	if (ram == ASMIM_INDIRECT_Y)
		CheckFrameIndex(rareg, rindex, size, BC_REG_ADDR);
	if (wam == ASMIM_INDIRECT_Y)
		CheckFrameIndex(wareg, windex, size, BC_REG_ACCU);


	for (int i = 0; i < size; i++)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, imm));

		if (ram == ASMIM_INDIRECT_Y)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rindex + i));
			mIns.Push(NativeCodeInstruction(at, ram, rareg));
		}
		else
			mIns.Push(NativeCodeInstruction(at, ram, rareg + i));

		if (!sfinal)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp] + i));

		if (wam == ASMIM_INDIRECT_Y)
		{
			if (ram != ASMIM_INDIRECT_Y || rindex != windex)
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, windex + i));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, wam, wareg));
		}
		else
			mIns.Push(NativeCodeInstruction(ASMIT_STA, wam, wareg + i));
	}

	return true;

}

bool NativeCodeBasicBlock::LoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, int oindex, const InterInstruction* wins)
{
	int size = InterTypeSize[wins->mSrc[0].mType];

	AsmInsType	at = ASMIT_ADC, an = ASMIT_ADC;
	AsmInsMode  am = oins->mSrc[oindex].mTemp < 0 ? ASMIM_IMMEDIATE : ASMIM_ZERO_PAGE, ram = ASMIM_INDIRECT_Y, wam = ASMIM_INDIRECT_Y;
	bool		reverse = false, sfinal = wins->mSrc[0].mFinal;

	switch (oins->mOperator)
	{
	case IA_ADD:
		mIns.Push(NativeCodeInstruction(ASMIT_CLC));
		at = an = ASMIT_ADC;
		break;
	case IA_SUB:
		mIns.Push(NativeCodeInstruction(ASMIT_SEC));
		at = an = ASMIT_SBC;
		if (oindex == 1)
			reverse = true;
		break;
	case IA_AND:
		at = an = ASMIT_AND;
		break;
	case IA_OR:
		at = an = ASMIT_ORA;
		break;
	case IA_XOR:
		at = an = ASMIT_EOR;
		break;
	case IA_SHL:
		if (oindex == 0 && oins->mSrc[oindex].mTemp < 0 && oins->mSrc[oindex].mIntConst == 1)
		{
			at = ASMIT_ASL;
			an = ASMIT_ROL;
			am = ASMIM_IMPLIED;
		}
		else
			return false;
		break;
	default:
		return false;
	}

	int	rindex = rins->mSrc[0].mIntConst;
	int rareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

	switch (rins->mSrc[0].mMemory)
	{
	case IM_INDIRECT:
		rareg = BC_REG_TMP + proc->mTempOffset[rins->mSrc[0].mTemp];
		break;
	case IM_LOCAL:
		rindex += proc->mLocalVars[rins->mSrc[0].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PARAM:
		rindex += rins->mSrc[0].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_FPARAM:
		ram = ASMIM_ZERO_PAGE;
		rareg = BC_REG_FPARAMS + rins->mSrc[0].mVarIndex + rins->mSrc[0].mIntConst;
		break;
	default:
		return false;
	}

	int	windex = wins->mSrc[1].mIntConst;
	int wareg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

	switch (wins->mSrc[1].mMemory)
	{
	case IM_INDIRECT:
		wareg = BC_REG_TMP + proc->mTempOffset[wins->mSrc[1].mTemp];
		break;
	case IM_LOCAL:
		windex += proc->mLocalVars[wins->mSrc[1].mVarIndex]->mOffset + mFrameOffset;
		break;
	case IM_PARAM:
		windex += wins->mSrc[1].mVarIndex + proc->mLocalSize + 2 + mFrameOffset;
		break;
	case IM_FPARAM:
		wam = ASMIM_ZERO_PAGE;
		wareg = BC_REG_FPARAMS + +wins->mSrc[1].mVarIndex + wins->mSrc[1].mIntConst;
		break;
	default:
		return false;
	}

	uint32	rflags = NCIF_LOWER | NCIF_UPPER;
	if (rins->mVolatile)
		rflags |= NCIF_VOLATILE;

	uint32	wflags = NCIF_LOWER | NCIF_UPPER;
	if (wins->mVolatile)
		wflags |= NCIF_VOLATILE;

	if (ram == ASMIM_INDIRECT_Y)
		CheckFrameIndex(rareg, rindex, size, BC_REG_ADDR);
	if (wam == ASMIM_INDIRECT_Y)
		CheckFrameIndex(wareg, windex, size, BC_REG_ACCU);

	for (int i = 0; i < size; i++)
	{
		if (reverse)
		{
			if (am == ASMIM_IMPLIED)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMPLIED));
			else if (am == ASMIM_IMMEDIATE)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (oins->mSrc[oindex].mIntConst >> (8 * i)) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[oins->mSrc[oindex].mTemp] + i));

			if (ram == ASMIM_INDIRECT_Y)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rindex + i));
				mIns.Push(NativeCodeInstruction(at, ram, rareg));
			}
			else
				mIns.Push(NativeCodeInstruction(at, ram, rareg + i));
		}
		else
		{
			if (ram == ASMIM_INDIRECT_Y)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rindex + i));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ram, rareg));
			}
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ram, rareg + i));

			if (am == ASMIM_IMPLIED)
				mIns.Push(NativeCodeInstruction(at, ASMIM_IMPLIED));
			else if (am == ASMIM_IMMEDIATE)
				mIns.Push(NativeCodeInstruction(at, ASMIM_IMMEDIATE, (oins->mSrc[oindex].mIntConst >> (8 * i)) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(at, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[oins->mSrc[oindex].mTemp] + i));
		}

		if (!sfinal)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[wins->mSrc[0].mTemp] + i));

		at = an;
		if (wam == ASMIM_INDIRECT_Y)
		{
			if (ram != ASMIM_INDIRECT_Y || rindex != windex)
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, windex + i));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, wam, wareg));
		}
		else
			mIns.Push(NativeCodeInstruction(ASMIT_STA, wam, wareg + i));
	}

	return true;
}

void NativeCodeBasicBlock::LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction* ainsl, const NativeCodeInstruction* ainsh)
{
	uint32	flags = NCIF_LOWER | NCIF_UPPER;
	if (ins->mVolatile)
		flags |= NCIF_VOLATILE;

	if (ins->mDst.mType == IT_FLOAT)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, ins->mSrc[0].mLinkerObject, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 2, ins->mSrc[0].mLinkerObject, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 3, ins->mSrc[0].mLinkerObject, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
			else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, nullptr, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, nullptr, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 2, nullptr, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 3, nullptr, flags));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
			else if (ins->mSrc[0].mMemory == IM_FPARAM)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
			else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
			{
				int	index = ins->mSrc[0].mIntConst;
				int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
				if (ins->mSrc[0].mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
				else
					index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
				index += mFrameOffset;
				CheckFrameIndex(areg, index, 4);

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	areg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				int index = ins->mSrc[0].mIntConst;

				CheckFrameIndex(areg, index, 4);

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
		}
	}
	else if (ins->mDst.mType == IT_POINTER)
	{
		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, flags));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, ins->mSrc[0].mLinkerObject, flags));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, nullptr, flags));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, nullptr, flags));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins->mSrc[0].mMemory == IM_FPARAM)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 1));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
			{
				int	index = ins->mSrc[0].mIntConst;
				int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
				if (ins->mSrc[0].mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
				else
					index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
				index += mFrameOffset;
				CheckFrameIndex(areg, index, 2);

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	areg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				int index = ins->mSrc[0].mIntConst;

				CheckFrameIndex(areg, index, 2);

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				if (reg == areg)
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
				else
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				if (reg == areg)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				}
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
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, flags));
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, nullptr, flags));
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst));
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				}

				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));

				if (InterTypeSize[ins->mDst.mType] > 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
			else if (InterTypeSize[ins->mDst.mType] == 2)
			{
				if (ins->mSrc[0].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, flags));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, ins->mSrc[0].mLinkerObject, flags));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, nullptr, flags));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, nullptr, flags));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 1));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(areg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
			else if (InterTypeSize[ins->mDst.mType] == 4)
			{
				if (ins->mSrc[0].mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, ins->mSrc[0].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 2, ins->mSrc[0].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 3, ins->mSrc[0].mLinkerObject, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
				}
				else if (ins->mSrc[0].mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 1, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 2, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst + 3, nullptr, flags));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
				}
				else if (ins->mSrc[0].mMemory == IM_FPARAM)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_FPARAMS + ins->mSrc[0].mVarIndex + ins->mSrc[0].mIntConst + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
				}
				else if (ins->mSrc[0].mMemory == IM_LOCAL || ins->mSrc[0].mMemory == IM_PARAM)
				{
					int	index = ins->mSrc[0].mIntConst;
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mSrc[0].mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mSrc[0].mVarIndex]->mOffset;
					else
						index += ins->mSrc[0].mVarIndex + proc->mLocalSize + 2;
					index += mFrameOffset;
					CheckFrameIndex(areg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
				}
			}
		}
		else
		{
			if (ins->mSrc[0].mMemory == IM_INDIRECT)
			{
				int	areg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp];
				int index = ins->mSrc[0].mIntConst;

				CheckFrameIndex(areg, index, InterTypeSize[ins->mDst.mType]);

				if (InterTypeSize[ins->mDst.mType] == 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						if (ainsh) mIns.Push(*ainsh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					}
				}
				else if (InterTypeSize[ins->mDst.mType] == 2)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}

					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						if (reg == areg)
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
						else
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
						if (ainsh) mIns.Push(*ainsh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
						if (reg == areg)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
						}
					}
					else
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				}
				else if (InterTypeSize[ins->mDst.mType] == 4)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	LoadValueToReg(proc, ins, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp], nullptr, nullptr);
}

NativeCodeBasicBlock * NativeCodeBasicBlock::CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc)
{
	int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], dreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

	int	size = ins->mConst.mOperandSize;
	if (size < 4)
	{
		for (int i = 0; i < size; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, i));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
		}

		return this;
	}
	else
	{
		NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
		NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

		if (size < 128)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, size - 1));
			this->Close(lblock, nullptr, ASMIT_JMP);
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			lblock->Close(lblock, eblock, ASMIT_BPL);
		}
		else if (size <= 256)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, size - 1));
			this->Close(lblock, nullptr, ASMIT_JMP);
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
			lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			lblock->Close(lblock, eblock, ASMIT_BNE);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
		}

		return eblock;
	}
}

NativeCodeBasicBlock* NativeCodeBasicBlock::StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc)
{
	int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], dreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

	NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
	NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

	mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0xff));
	this->Close(lblock, nullptr, ASMIT_JMP);
	lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
	lblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
	lblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
	lblock->Close(lblock, eblock, ASMIT_BNE);

	return eblock;
}

bool NativeCodeBasicBlock::CheckPredAccuStore(int reg)
{
	if (mIns.Size() < 8)
		return false;

	int	p = mIns.Size() - 8;

	for (int i = 0; i < 4; i++)
	{
		if (mIns[p + 0].mType != ASMIT_LDA || mIns[p + 0].mMode != ASMIM_ZERO_PAGE || mIns[p + 0].mAddress != BC_REG_ACCU + i)
			return false;
		if (mIns[p + 1].mType != ASMIT_STA || mIns[p + 1].mMode != ASMIM_ZERO_PAGE || mIns[p + 1].mAddress != reg + i)
			return false;

		p += 2;
	}

	return true;
}

void NativeCodeBasicBlock::ShiftRegisterLeft(InterCodeProcedure* proc, int reg, int shift)
{
	if (shift == 0)
	{

	}
	else if (shift == 1)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, reg + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, reg + 1));
	}
	else if (shift >= 5)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
		for (int i = shift; i < 8; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, reg + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, (0xff << shift) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
		for (int i = 0; i < shift; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
	}
}

void NativeCodeBasicBlock::ShiftRegisterLeftByte(InterCodeProcedure* proc, int reg, int shift)
{
	if (shift == 0)
	{

	}
	else if (shift == 1)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, reg));
	}
	else if (shift >= 6)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
		for (int i = shift; i < 8; i++)
			mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, (0xff << shift) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg));
		for (int i = 0; i < shift; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
	}
}

int NativeCodeBasicBlock::ShortMultiply(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins, int index, int mul)
{
	mul &= 0xffff;

	int	lshift = 0, lmul = mul;
	while (!(lmul & 1))
	{
		lmul >>= 1;
		lshift++;
	}

	if (mul > 1 && (lshift > 3 || lmul != 1) && ins->mSrc[index].IsUByte() && ins->mSrc[index].mRange.mMaxValue < 16)
	{
		int	dreg = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];

		if (sins)
		{
			LoadValueToReg(proc, sins, dreg, nullptr, nullptr);
			mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, dreg));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[index].mTemp]));
		}

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, 0, nproc->mGenerator->AllocateShortMulTable(mul, ins->mSrc[index].mRange.mMaxValue + 1, false)));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, dreg));
		if (ins->mDst.IsUByte())
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, dreg + 1));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, 0, nproc->mGenerator->AllocateShortMulTable(mul, ins->mSrc[index].mRange.mMaxValue + 1, true)));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, dreg + 1));
		}

		return dreg;
	}
	
	if (lmul == 1 && !sins && ins->mSrc[index].mTemp == ins->mDst.mTemp)
	{
		// shift in place

		int	dreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[index].mTemp];

		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, dreg, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, dreg + 1));
		}
		else
			ShiftRegisterLeft(proc, dreg, lshift);
		return dreg;
	}

	if (sins)
		LoadValueToReg(proc, sins, BC_REG_ACCU, nullptr, nullptr);
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[index].mTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[index].mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
	}

	switch (lmul)
	{
#if 1
	case 1:
		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, BC_REG_ACCU, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
			ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 3:
		if (ins->mSrc[index].IsUByte() && ins->mSrc[index].mRange.mMaxValue <= 85)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, BC_REG_ACCU, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
			ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 5:
		if (ins->mSrc[index].IsUByte() && ins->mSrc[index].mRange.mMaxValue <= 51)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
			mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, BC_REG_ACCU, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
			ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 7:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
		mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
		mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, BC_REG_ACCU, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
			ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 9:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 4));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 5));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		if (ins->mDst.IsUByte())
		{
			ShiftRegisterLeftByte(proc, BC_REG_ACCU, lshift);
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
			ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
#if 1
	case 25:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TAY, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
#endif
#endif
	default:
		if (mul & 0xff00)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, mul & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, mul >> 8));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));

			NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, mul));
//			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));

			NativeCodeGenerator::Runtime& rt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16by8")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, rt.mOffset, rt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_USE_CPU_REG_A));
		}

		return BC_REG_WORK + 2;
	}
}

static bool IsPowerOf2(unsigned n)
{
	return (n & (n - 1)) == 0;
}

static int Binlog(unsigned n)
{
	int	k = -1;

	while (n)
	{
		n >>= 1;
		k++;
	}

	return k;
}
NativeCodeBasicBlock* NativeCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction * sins1, const InterInstruction * sins0)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];

	if (ins->mDst.mType == IT_FLOAT)
	{
		int		sop0 = 0, sop1 = 1;
		bool	flipop = false;
		bool	changedSign = false;

		if (ins->mOperator == IA_ADD || ins->mOperator == IA_MUL || ins->mOperator == IA_SUB)
		{
			if (!sins0 && ins->mSrc[sop0].mTemp >= 0 && CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop0].mTemp]))
			{
				flipop = true;
				sop0 = 1; sop1 = 0;
				const InterInstruction* sins = sins0; sins0 = sins1; sins1 = sins;
			}
			else if (!sins1 && !sins0 && ins->mSrc[sop0].mTemp < 0 && ins->mSrc[sop1].mTemp >= 0 && !CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp]))
			{
				flipop = true;
				sop0 = 1; sop1 = 0;
				const InterInstruction* sins = sins0; sins0 = sins1; sins1 = sins;
			}
		}

		int	sreg0 = ins->mSrc[sop0].mTemp < 0 ? -1 : BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop0].mTemp];

		if (ins->mSrc[sop1].mTemp < 0)
		{
			union { float f; unsigned int v; } cc;

			if (ins->mOperator == IA_SUB && flipop)
			{
				changedSign = true;
				cc.f = -ins->mSrc[sop1].mFloatConst;
			}
			else
				cc.f = ins->mSrc[sop1].mFloatConst;

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}
		else if (sins1)
		{
			LoadValueToReg(proc, sins1, BC_REG_ACCU, nullptr, nullptr);
		}
		else if (CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp]))
		{
			if (ins->mSrc[sop1].mFinal)
			{
				// cull previous store from accu to temp using direcrt forwarding
				mIns.SetSize(mIns.Size() - 8);
			}
			if (sreg0 == BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp])
				sreg0 = BC_REG_ACCU;
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[sop1].mTemp] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}

		if (ins->mSrc[sop0].mTemp < 0)
		{
			union { float f; unsigned int v; } cc;

			if (ins->mOperator == IA_SUB && !flipop)
			{
				changedSign = true;
				cc.f = -ins->mSrc[sop0].mFloatConst;
			}
			else
				cc.f = ins->mSrc[sop0].mFloatConst;

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));

			NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fsplitt")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}
		else if (sins0)
		{
			LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
			NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fsplitt")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}
		else
		{
			NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fsplitx")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_USE_ZP_32_X, sreg0));
#if 0
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
#endif
		}


		switch (ins->mOperator)
		{
		case IA_ADD:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("faddsub")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}	break;
		case IA_SUB:
		{
			if (!changedSign)
			{
				if (flipop)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
				}
			}

			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("faddsub")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}	break;
		case IA_MUL:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("fmul")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}	break;
		case IA_DIVS:
		case IA_DIVU:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("fdiv")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
		}	break;
		}

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
	}
	else if (ins->mDst.mType == IT_INT32)
	{
		switch (ins->mOperator)
		{
		case IA_ADD:
		case IA_SUB:
		case IA_OR:
		case IA_AND:
		case IA_XOR:
		{
			if (sins1)	LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0)	LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			AsmInsType	atype;
			switch (ins->mOperator)
			{
			case IA_ADD:
				mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
				atype = ASMIT_ADC;
				break;
			case IA_SUB:
				mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
				atype = ASMIT_SBC;
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

			for (int i = 0; i < 4; i++)
			{
				if (ins->mSrc[1].mTemp < 0)
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> (8 * i)) & 0xff));
				else
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + i));
				if (ins->mSrc[0].mTemp < 0)
					mIns.Push(NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> (8 * i)) & 0xff));
				else
					mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + i));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + i));
			}
		} break;

		case IA_MUL:
		case IA_DIVS:
		case IA_MODS:
		case IA_DIVU:
		case IA_MODU:
		{
			if (sins1)
				LoadValueToReg(proc, sins1, BC_REG_ACCU, nullptr, nullptr);
			else if (ins->mSrc[1].mTemp < 0)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 16) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 24) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			}

			if (sins0)
				LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
			else if (ins->mSrc[0].mTemp < 0)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
			}
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
			}

			int	reg = BC_REG_ACCU;

			switch (ins->mOperator)
			{
			case IA_MUL:
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul32")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
				reg = BC_REG_WORK + 4;
			}	break;
			case IA_DIVS:
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divs32")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
			}	break;
			case IA_MODS:
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mods32")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
				reg = BC_REG_WORK + 4;
			}	break;
			case IA_DIVU:
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divu32")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
			}	break;
			case IA_MODU:
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("modu32")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
				reg = BC_REG_WORK + 4;
			}	break;
			}

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
		} break;
		case IA_SHL:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int	shift = ins->mSrc[0].mIntConst & 31;

				int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

				if (shift >= 24)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					sreg = treg;
					shift -= 24;
				}
				else if (shift >= 16)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					sreg = treg;
					shift -= 16;
				}
				else if (shift >= 8)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					sreg = treg;
					shift -= 8;
				}

				if (shift == 0)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					}
				}
				else if (shift == 1)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 3));
					}
				}
				else
				{
					NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
					NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					}
					else
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));

					mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, shift));
					this->Close(lblock, nullptr, ASMIT_JMP);

					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg + 0));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 2));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					lblock->Close(lblock, eblock, ASMIT_BNE);

					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					return eblock;
				}
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x1f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 24) & 0xff));
				}
				else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 3));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				this->Close(lblock, eblock, ASMIT_BNE);

				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg + 0));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 2));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				lblock->Close(lblock, eblock, ASMIT_BNE);

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
				return eblock;
			}
		} break;

		case IA_SHR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int	shift = ins->mSrc[0].mIntConst & 31;

				int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

				if (shift >= 24)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 24;
				}
				else if (shift >= 16)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 16;
				}
				else if (shift >= 8)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 8;
				}

				if (shift == 0)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					}
				}
				else if (shift == 1)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_ZERO_PAGE, treg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
					}
				}
				else
				{
					NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
					NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					}
					else
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));

					mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, shift));
					this->Close(lblock, nullptr, ASMIT_JMP);

					lblock->mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					lblock->Close(lblock, eblock, ASMIT_BNE);

					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					return eblock;
				}
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x1f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 24) & 0xff));
				}
				else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 3));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				this->Close(lblock, eblock, ASMIT_BNE);

				lblock->mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				lblock->Close(lblock, eblock, ASMIT_BNE);

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
				return eblock;
			}
		} break;

		case IA_SAR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int	shift = ins->mSrc[0].mIntConst & 31;

				int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

				if (shift >= 24)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 24;
				}
				else if (shift >= 16)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 16;
				}
				else if (shift >= 8)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					sreg = treg;
					shift -= 8;
				}

				if (shift == 0)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					}
				}
				else if (shift == 1)
				{
					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 3));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
					}
				}
				else
				{
					NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
					NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

					if (sreg != treg)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					}
					else
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));

					mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, shift));
					this->Close(lblock, nullptr, ASMIT_JMP);

					lblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					lblock->Close(lblock, eblock, ASMIT_BNE);

					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
					return eblock;
				}
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x1f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 24) & 0xff));
				}
				else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 3));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 3));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				this->Close(lblock, eblock, ASMIT_BNE);

				lblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 2));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 1));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				lblock->Close(lblock, eblock, ASMIT_BNE);

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
				return eblock;
			}
		} break;
		}
	}
	else
	{
		switch (ins->mOperator)
		{
		case IA_ADD:
		case IA_OR:
		case IA_AND:
		case IA_XOR:
		{
			if (ins->mOperator == IA_ADD && (
				ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == 1 && !sins1 && ins->mSrc[1].mTemp == ins->mDst.mTemp ||
				ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst == 1 && !sins0 && ins->mSrc[0].mTemp == ins->mDst.mTemp))
			{
				mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg));
				if (InterTypeSize[ins->mDst.mType] > 1)
				{
					if (ins->mDst.IsUByte())
					{
						if (ins->mSrc[0].mTemp >= 0 && !ins->mSrc[0].IsUByte() || ins->mSrc[1].mTemp >= 0 && !ins->mSrc[1].IsUByte())
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else
					{
						NativeCodeBasicBlock* iblock = nproc->AllocateBlock();
						NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

						this->Close(eblock, iblock, ASMIT_BNE);

						iblock->mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg + 1));

						iblock->Close(eblock, nullptr, ASMIT_JMP);
						return eblock;
					}
				}
			}
			else if (ins->mOperator == IA_ADD && InterTypeSize[ins->mDst.mType] == 1 && (
				ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == -1 && !sins1 && ins->mSrc[1].mTemp == ins->mDst.mTemp ||
				ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst == -1 && !sins0 && ins->mSrc[0].mTemp == ins->mDst.mTemp))
			{
				mIns.Push(NativeCodeInstruction(ASMIT_DEC, ASMIM_ZERO_PAGE, treg));
			}
			else
			{
				NativeCodeInstruction	insl, insh;

				AsmInsType	atype;
				switch (ins->mOperator)
				{
				case IA_ADD:
					atype = ASMIT_ADC;
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

				if (ins->mSrc[1].mTemp < 0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff);
					insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff);
					if (sins0)
					{
						if (ins->mDst.IsUByte())
							insh = NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0);

						LoadValueToReg(proc, sins0, treg, &insl, &insh);
					}
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(insl);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						if (InterTypeSize[ins->mDst.mType] > 1)
						{
							if (ins->mDst.IsUByte())
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
							else
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
								mIns.Push(insh);
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
						}
					}
				}
				else if (ins->mSrc[0].mTemp < 0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff);
					insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff);
					if (sins1)
					{
						if (ins->mDst.IsUByte())
							insh = NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0);

						LoadValueToReg(proc, sins1, treg, &insl, &insh);
					}
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(insl);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						if (InterTypeSize[ins->mDst.mType] > 1)
						{
							if (ins->mDst.IsUByte())
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
							else
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
								mIns.Push(insh);
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
						}
					}
				}
				else
				{
					if (sins1 && sins0)
					{
						if (sins0->mSrc[0].mMemory == IM_INDIRECT && sins1->mSrc[0].mMemory == IM_INDIRECT && sins0->mSrc[0].mIntConst < 255 && sins1->mSrc[0].mIntConst < 255)
						{
							if (ins->mOperator == IA_ADD)
								mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, sins0->mSrc[0].mIntConst + 0));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[sins0->mSrc[0].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, sins1->mSrc[0].mIntConst + 0));
							mIns.Push(NativeCodeInstruction(atype, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[sins1->mSrc[0].mTemp]));
							if (InterTypeSize[ins->mDst.mType] > 1)
							{
								if (ins->mDst.mTemp == sins0->mSrc[0].mTemp || ins->mDst.mTemp == sins1->mSrc[0].mTemp)
									mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
								else
									mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
								mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, sins0->mSrc[0].mIntConst + 1));
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[sins0->mSrc[0].mTemp]));
								mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, sins1->mSrc[0].mIntConst + 1));
								mIns.Push(NativeCodeInstruction(atype, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[sins1->mSrc[0].mTemp]));
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
								if (ins->mDst.mTemp == sins0->mSrc[0].mTemp || ins->mDst.mTemp == sins1->mSrc[0].mTemp)
								{
									mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK_Y));
									mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
								}
							}
							else
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						}
						else
						{
							insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, treg);
							insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, treg + 1);

							if (sins1->mDst.mTemp == ins->mDst.mTemp)
							{
								LoadValueToReg(proc, sins1, treg, nullptr, nullptr);
								LoadValueToReg(proc, sins0, treg, &insl, &insh);
							}
							else
							{
								LoadValueToReg(proc, sins0, treg, nullptr, nullptr);
								LoadValueToReg(proc, sins1, treg, &insl, &insh);
							}
						}
					}
					else if (sins1)
					{
						insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]);
						insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1);

						LoadValueToReg(proc, sins1, treg, &insl, &insh);
					}
					else if (sins0)
					{
						insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]);
						insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1);

						LoadValueToReg(proc, sins0, treg, &insl, &insh);
					}
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						if (InterTypeSize[ins->mDst.mType] > 1)
						{
#if 1
							if (ins->mDst.IsUByte())
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
							else
#endif
							{
								mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
								mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
								mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							}
						}
					}
				}
			}
		} break;
		case IA_SUB:
		{
			NativeCodeInstruction	insl, insh;

			if (InterTypeSize[ins->mDst.mType] == 1 &&
				ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst == 1 && !sins0 && ins->mSrc[0].mTemp == ins->mDst.mTemp)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_DEC, ASMIM_ZERO_PAGE, treg));
			}
			else if (ins->mSrc[0].mTemp < 0)
			{
				insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff);
				if (ins->mDst.IsUByte())
					insh = NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0);
				else
					insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff);

				if (sins1)
					LoadValueToReg(proc, sins1, treg, &insl, &insh);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						mIns.Push(insh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
			else if (ins->mSrc[1].mTemp < 0)
			{
				if (sins0)
				{
					LoadValueToReg(proc, sins0, treg, nullptr, nullptr);

					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
			else
			{
				if (sins0)
				{
					insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_WORK + 0);
					insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_WORK + 1);

					LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
				}
				else
				{
					insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]);
					insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1);
				}

				if (sins1)
				{
					LoadValueToReg(proc, sins1, treg, &insl, &insh);
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					if (InterTypeSize[ins->mDst.mType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						mIns.Push(insh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
		} break;
		case IA_MUL:
		case IA_DIVS:
		case IA_MODS:
		case IA_DIVU:
		case IA_MODU:
		{
			int	reg = BC_REG_ACCU;

			if (ins->mOperator == IA_MUL && ins->mSrc[1].mTemp < 0)
			{
				reg = ShortMultiply(proc, nproc, ins, sins0, 0, ins->mSrc[1].mIntConst);
			}
			else if (ins->mOperator == IA_MUL && ins->mSrc[0].mTemp < 0)
			{
				reg = ShortMultiply(proc, nproc, ins, sins1, 1, ins->mSrc[0].mIntConst);
			}
			else if (ins->mOperator == IA_MUL && ins->mSrc[0].IsUByte())
			{
				if (sins1)
					LoadValueToReg(proc, sins1, BC_REG_ACCU, nullptr, nullptr);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}

				if (sins0)
				{
					LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
				}
				else
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));

				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16by8")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_USE_CPU_REG_A));
				reg = BC_REG_WORK + 2;
			}
			else if (ins->mOperator == IA_MUL && ins->mSrc[1].IsUByte())
			{
				if (sins0)
					LoadValueToReg(proc, sins0, BC_REG_ACCU, nullptr, nullptr);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}

				if (sins1)
				{
					LoadValueToReg(proc, sins1, BC_REG_WORK, nullptr, nullptr);
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
				}
				else
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));

				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16by8")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_USE_CPU_REG_A));
				reg = BC_REG_WORK + 2;
			}
			else
			{
				if (sins1)
					LoadValueToReg(proc, sins1, BC_REG_ACCU, nullptr, nullptr);
				else if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}

				if (sins0)
					LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
				else if (ins->mSrc[0].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				}

				switch (ins->mOperator)
				{
				case IA_MUL:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
					reg = BC_REG_WORK + 2;
				}	break;
				case IA_DIVS:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divs16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
				}	break;
				case IA_MODS:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mods16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
					reg = BC_REG_WORK + 2;
				}	break;
				case IA_DIVU:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divu16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
				}	break;
				case IA_MODU:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("modu16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
					reg = BC_REG_WORK + 2;
				}	break;
				}
			}

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		} break;
		case IA_SHL:
		{
			if (ins->mSrc[0].mTemp < 0 && (ins->mSrc[0].mIntConst & 15) == 1 && sins1)
			{
				NativeCodeInstruction	insl(ASMIT_ASL, ASMIM_IMPLIED);
				NativeCodeInstruction	insh(ASMIT_ROL, ASMIM_IMPLIED);
				LoadValueToReg(proc, sins1, treg, &insl, &insh);
				return this;
			}
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

				int	shift = ins->mSrc[0].mIntConst & 15;
				if (ins->mDst.IsUByte())
				{
					if (shift == 0)
					{
						if (sreg != treg)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						}
					}
					else if (shift == 1)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
						}
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else if (shift > 5)
					{

						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						for(int i=shift; i<8; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0xff << shift));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg ));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
						for (int i = 0; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					}

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
				else
				{
					if (shift == 0)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 1)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 7)
					{
						int sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];

						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						for (int i = 8; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
						}

						int	check = 0xffff;
						if (ins->mSrc[1].IsUByte())
							check = ins->mSrc[1].mRange.mMaxValue;

						check <<= 1;
						if (check >= 0x100)
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						for (int i = 1; i < shift; i++)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
							check <<= 1;
							if (check >= 0x100)
								mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						}

						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
			else if (ins->mSrc[1].mTemp < 0 && IsPowerOf2(ins->mSrc[1].mIntConst & 0xffff))
			{
				int	l = Binlog(ins->mSrc[1].mIntConst & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bitshift")));

				if (l < 8)
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 8 + l, frt.mLinkerObject));
				else
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + l, frt.mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
				}
				else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				this->Close(lblock, eblock, ASMIT_BNE);

				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				lblock->Close(lblock, eblock, ASMIT_BNE);

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				return eblock;
			}
		} break;
		case IA_SHR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int	shift = ins->mSrc[0].mIntConst & 15;
#if 1
				if (ins->mSrc[1].IsUByte())
				{
					if (shift == 0)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 7)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else if (shift == 6)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						for (int i = 0; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else
#endif
				{
					if (shift == 0)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 1)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
						}
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						for (int i = 8; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else if (shift >= 5)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, treg));
						}
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						for (int i = shift; i < 8; i++)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						}
						mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0xff >> shift));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_ZERO_PAGE, treg + 1));
						}
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						for (int i = 1; i < shift; i++)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						}
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					}
				}
			}
			else if (ins->mSrc[1].mTemp < 0 && IsPowerOf2(ins->mSrc[1].mIntConst & 0xffff))
			{
				int	l = Binlog(ins->mSrc[1].mIntConst & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bitshift")));

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 39 - l, frt.mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 47 - l, frt.mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].mTemp < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
				}
				else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				this->Close(lblock, eblock, ASMIT_BNE);

				lblock->mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
				lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				lblock->Close(lblock, eblock, ASMIT_BNE);

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				return eblock;
			}



		} break;
		case IA_SAR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp], nullptr, nullptr);

			if (ins->mSrc[0].mTemp < 0)
			{
				int	shift = ins->mSrc[0].mIntConst & 15;

				if (ins->mSrc[1].IsUByte())
				{
					if (shift == 0)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 7)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
					}
					else if (shift == 6)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 3));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						for (int i = 0; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 0));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else
				{
					if (shift == 0)
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift == 7)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
						mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else if (shift == 6)
					{
						int sreg = BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp];
						if (sreg != treg)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));

							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));

							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
							mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
							mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));

							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
							mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
							mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
							mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, treg + 1));
							mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						}
					}
					else if (shift >= 8)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						for (int i = 8; i < shift; i++)
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80 >> (shift - 8)));
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, 0x80 >> (shift - 8)));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
					else
					{
						if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
						}
						else
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
						}

						for (int i = 0; i < shift; i++)
						{
							mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
							mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
						}

						mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80 >> shift));
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, 0x80 >> shift));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
			else if (ins->mSrc[1].mTemp < 0 && IsPowerOf2(ins->mSrc[1].mIntConst & 0xffff))
			{
				int	l = Binlog(ins->mSrc[1].mIntConst & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bitshift")));

				if (l == 15)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ABSOLUTE_X, frt.mOffset + 39 - l, frt.mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 47 - l, frt.mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 39 - l, frt.mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_X, frt.mOffset + 47 - l, frt.mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else
			{
				NativeCodeBasicBlock* lblock = nproc->AllocateBlock();
				NativeCodeBasicBlock* eblock = nproc->AllocateBlock();

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
				if (!ins->mSrc[0].IsUByte() || ins->mSrc[0].mRange.mMaxValue > 15)
					mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSrc[1].IsUByte())
				{
					if (ins->mSrc[1].mTemp < 0)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
					}
					else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg));
					}

					mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
					this->Close(lblock, eblock, ASMIT_BNE);

					lblock->mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					lblock->Close(lblock, eblock, ASMIT_BNE);

					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
				else
				{
					if (ins->mSrc[1].mTemp < 0)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
					}
					else if (ins->mSrc[1].mTemp != ins->mDst.mTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
					}

					mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
					this->Close(lblock, eblock, ASMIT_BNE);

					lblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 0x80));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg + 0));
					lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
					lblock->Close(lblock, eblock, ASMIT_BNE);

					eblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
				return eblock;
			}
		} break;

		}
	}

	return this;
}

void NativeCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp];

	if (ins->mDst.mType == IT_FLOAT)
	{
		switch (ins->mOperator)
		{
		case IA_NEG:
		case IA_ABS:
			if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
			}
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));

			if (ins->mOperator == IA_NEG)
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x7f));

			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
			break;
		case IA_FLOOR:
		case IA_CEIL:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

			NativeCodeGenerator::Runtime& frx(nproc->mGenerator->ResolveRuntime(Ident::Unique("fsplita")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frx.mOffset, frx.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));

			if (ins->mOperator == IA_FLOOR)
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ffloor")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
			}
			else
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fceil")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));
			}

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
			break;
		}
	}
	else
	{
		switch (ins->mOperator)
		{
		case IA_NEG:
			mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			if (ins->mDst.mType == IT_INT32)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
			}
			break;

		case IA_NOT:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			if (ins->mDst.mType == IT_INT32)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 3));
			}
			break;
		}
	}
}

void NativeCodeBasicBlock::NumericConversion(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins)
{
	switch (ins->mOperator)
	{
	case IA_FLOAT2INT:
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ftoi")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));

	}	break;
	case IA_INT2FLOAT:
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ffromi")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));

	} break;
	case IA_EXT8TO16S:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
		if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		break;
	case IA_EXT8TO16U:
		if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		break;
	case IA_EXT16TO32S:
		if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
		if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0x00));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
		break;
	case IA_EXT16TO32U:
		if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
		break;
	}
}

void NativeCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump)
{
	InterOperator	op = ins->mOperator;

	if (ins->mSrc[0].mType == IT_FLOAT)
	{
		int	li = 0, ri = 1;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 1; ri = 0;
		}

		if (ins->mSrc[li].mTemp < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSrc[li].mFloatConst;

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}
		else if (ins->mSrc[li].mFinal && CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp]))
		{
			// cull previous store from accu to temp using direcrt forwarding
			mIns.SetSize(mIns.Size() - 8);
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}

		if (ins->mSrc[ri].mTemp < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSrc[ri].mFloatConst;

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
		}

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fcmp")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER));

		switch (op)
		{
		case IA_CMPEQ:
			Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		case IA_CMPNE:
			Close(falseJump, trueJump, ASMIT_BEQ);
			break;
		case IA_CMPLU:
		case IA_CMPLS:
		case IA_CMPGU:
		case IA_CMPGS:
			Close(trueJump, falseJump, ASMIT_BMI);
			break;
		case IA_CMPLEU:
		case IA_CMPLES:
		case IA_CMPGEU:
		case IA_CMPGES:
			Close(falseJump, trueJump, ASMIT_BMI);
			break;
		}
	}
	else if (ins->mSrc[0].mType == IT_INT32)
	{
		int	li = 1, ri = 0;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 0; ri = 1;
		}

		if (op >= IA_CMPGES && ins->mOperator <= IA_CMPLS)
		{
			if (ins->mSrc[ri].mTemp >= 0)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK));
			}

			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ((ins->mSrc[li].mIntConst >> 24) & 0xff) ^ 0x80));
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			}

			if (ins->mSrc[ri].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ((ins->mSrc[ri].mIntConst >> 24) & 0xff) ^ 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_WORK));
		}
		else
		{
			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[li].mIntConst >> 24) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 3));
			if (ins->mSrc[ri].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins->mSrc[ri].mIntConst >> 24) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 3));
		}

		NativeCodeBasicBlock* eblock3 = nproc->AllocateBlock();
		NativeCodeBasicBlock* eblock2 = nproc->AllocateBlock();
		NativeCodeBasicBlock* eblock1 = nproc->AllocateBlock();
		NativeCodeBasicBlock* nblock = nproc->AllocateBlock();

		this->Close(nblock, eblock3, ASMIT_BNE);

		if (ins->mSrc[li].mTemp < 0)
			eblock3->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[li].mIntConst >> 16) & 0xff));
		else
			eblock3->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 2));
		if (ins->mSrc[ri].mTemp < 0)
			eblock3->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins->mSrc[ri].mIntConst >> 16) & 0xff));
		else
			eblock3->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 2));

		eblock3->Close(nblock, eblock2, ASMIT_BNE);

		if (ins->mSrc[li].mTemp < 0)
			eblock2->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[li].mIntConst >> 8) & 0xff));
		else
			eblock2->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 1));
		if (ins->mSrc[ri].mTemp < 0)
			eblock2->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins->mSrc[ri].mIntConst >> 8) & 0xff));
		else
			eblock2->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 1));

		eblock2->Close(nblock, eblock1, ASMIT_BNE);

		if (ins->mSrc[li].mTemp < 0)
			eblock1->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[li].mIntConst & 0xff));
		else
			eblock1->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp]));
		if (ins->mSrc[ri].mTemp < 0)
			eblock1->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ins->mSrc[ri].mIntConst & 0xff));
		else
			eblock1->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp]));

		switch (op)
		{
		case IA_CMPEQ:
			nblock->Close(falseJump, nullptr, ASMIT_JMP);
			eblock1->Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		case IA_CMPNE:
			nblock->Close(trueJump, nullptr, ASMIT_JMP);
			eblock1->Close(falseJump, trueJump, ASMIT_BEQ);
			break;
		case IA_CMPLU:
		case IA_CMPLS:
		case IA_CMPGU:
		case IA_CMPGS:
			eblock1->Close(nblock, nullptr, ASMIT_JMP);
			nblock->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		case IA_CMPLEU:
		case IA_CMPLES:
		case IA_CMPGEU:
		case IA_CMPGES:
			eblock1->Close(nblock, nullptr, ASMIT_JMP);
			nblock->Close(falseJump, trueJump, ASMIT_BCC);
			break;

		}
	}
	else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst == 0 || ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == 0)
	{
		int	rt = ins->mSrc[1].mTemp;
		if (rt < 0)
		{
			rt = ins->mSrc[0].mTemp;
			switch (op)
			{
			case IA_CMPLEU:
				op = IA_CMPGEU;
				break;
			case IA_CMPGEU:
				op = IA_CMPLEU;
				break;
			case IA_CMPLU:
				op = IA_CMPGU;
				break;
			case IA_CMPGU:
				op = IA_CMPLU;
				break;
			case IA_CMPLES:
				op = IA_CMPGES;
				break;
			case IA_CMPGES:
				op = IA_CMPLES;
				break;
			case IA_CMPLS:
				op = IA_CMPGS;
				break;
			case IA_CMPGS:
				op = IA_CMPLS;
				break;
			}
		}

		switch (op)
		{
		case IA_CMPEQ:
		case IA_CMPLEU:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			if (InterTypeSize[ins->mSrc[0].mType] > 1)
				mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		case IA_CMPNE:
		case IA_CMPGU:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			if (InterTypeSize[ins->mSrc[0].mType] > 1)
				mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(trueJump, falseJump, ASMIT_BNE);
			break;
		case IA_CMPGEU:
			this->Close(trueJump, nullptr, ASMIT_JMP);
			break;
		case IA_CMPLU:
			this->Close(falseJump, nullptr, ASMIT_JMP);
			break;
		case IA_CMPGES:
			if (InterTypeSize[ins->mSrc[0].mType] == 1)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt]));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(trueJump, falseJump, ASMIT_BPL);
			break;
		case IA_CMPLS:
			if (InterTypeSize[ins->mSrc[0].mType] == 1)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt]));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(trueJump, falseJump, ASMIT_BMI);
			break;
		case IA_CMPGS:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			if (InterTypeSize[ins->mSrc[0].mType] == 1)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt]));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, falseJump, ASMIT_BPL);
			if (InterTypeSize[ins->mSrc[0].mType] != 1)
				eblock->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(trueJump, falseJump, ASMIT_BNE);
			break;
		}
		case IA_CMPLES:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			if (InterTypeSize[ins->mSrc[0].mType] == 1)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt]));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, trueJump, ASMIT_BPL);
			if (InterTypeSize[ins->mSrc[0].mType] != 1)
				eblock->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		}

		}
	}
	else if (InterTypeSize[ins->mSrc[0].mType] == 1)
	{
		NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
		NativeCodeBasicBlock* nblock = nproc->AllocateBlock();

		int	li = 1, ri = 0;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 0; ri = 1;
		}

		int		iconst = 0;
		bool	rconst = false;

		if (ins->mSrc[li].mTemp < 0 && (op == IA_CMPGES || op == IA_CMPLS) && int16(ins->mSrc[li].mIntConst) > - 128)
		{
			iconst = ins->mSrc[li].mIntConst - 1;
			rconst = true;
			li = ri; ri = 1 - li;

			NativeCodeBasicBlock* t = trueJump; trueJump = falseJump; falseJump = t;
		}
		else if (ins->mSrc[li].mTemp < 0 && (op == IA_CMPLES || op == IA_CMPGS) && int16(ins->mSrc[li].mIntConst) < 127)
		{
			iconst = ins->mSrc[li].mIntConst + 1;
			rconst = true;
			li = ri; ri = 1 - li;

			NativeCodeBasicBlock* t = trueJump; trueJump = falseJump; falseJump = t;
		}
		else if (ins->mSrc[ri].mTemp < 0)
		{
			iconst = ins->mSrc[ri].mIntConst;
			rconst = true;
		}

		if (op >= IA_CMPGES && ins->mOperator <= IA_CMPLS)
		{
			if (!rconst)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK));
			}

			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[li].mIntConst & 0xff) ^ 0x80));
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp]));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			}

			if (rconst)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (iconst & 0xff) ^ 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_WORK));
		}
		else
		{
			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[li].mIntConst & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp]));
			if (rconst)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, iconst & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp]));
		}

		switch (op)
		{
		case IA_CMPEQ:
			this->Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		case IA_CMPNE:
			this->Close(falseJump, trueJump, ASMIT_BEQ);
			break;
		case IA_CMPLU:
		case IA_CMPLS:
		case IA_CMPGU:
		case IA_CMPGS:
			this->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		case IA_CMPLEU:
		case IA_CMPLES:
		case IA_CMPGEU:
		case IA_CMPGES:
			this->Close(falseJump, trueJump, ASMIT_BCC);
			break;

		}

	}
#if 1
	else if (ins->mSrc[1].mTemp < 0 && ins->mSrc[1].mIntConst < 256 && ins->mSrc[1].mIntConst > 0 || ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst < 256 && ins->mSrc[0].mIntConst > 0)
	{
		int	rt = ins->mSrc[1].mTemp;
		int ival = ins->mSrc[0].mIntConst;
		if (rt < 0)
		{
			rt = ins->mSrc[0].mTemp;
			ival = ins->mSrc[1].mIntConst;
			switch (op)
			{
			case IA_CMPLEU:
				op = IA_CMPGEU;
				break;
			case IA_CMPGEU:
				op = IA_CMPLEU;
				break;
			case IA_CMPLU:
				op = IA_CMPGU;
				break;
			case IA_CMPGU:
				op = IA_CMPLU;
				break;
			case IA_CMPLES:
				op = IA_CMPGES;
				break;
			case IA_CMPGES:
				op = IA_CMPLES;
				break;
			case IA_CMPLS:
				op = IA_CMPGS;
				break;
			case IA_CMPGS:
				op = IA_CMPLS;
				break;
			}
		}

		switch (op)
		{
		case IA_CMPEQ:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, falseJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(trueJump, falseJump, ASMIT_BEQ);
			break;
		}
		case IA_CMPNE:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, trueJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(falseJump, trueJump, ASMIT_BEQ);
			break;
		}

		case IA_CMPLEU:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, falseJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ival));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(falseJump, trueJump, ASMIT_BCC);
			break;
		}

		case IA_CMPGEU:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, trueJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(falseJump, trueJump, ASMIT_BCC);
			break;
		}

		case IA_CMPLU:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, falseJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		}

		case IA_CMPGU:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(eblock, trueJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ival));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		}

		case IA_CMPLES:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			NativeCodeBasicBlock* sblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(sblock, trueJump, ASMIT_BPL);
			sblock->Close(eblock, falseJump, ASMIT_BEQ);

			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ival));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(falseJump, trueJump, ASMIT_BCC);
			break;
		}

		case IA_CMPGES:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			NativeCodeBasicBlock* sblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(sblock, falseJump, ASMIT_BPL);
			sblock->Close(eblock, trueJump, ASMIT_BEQ);

			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(falseJump, trueJump, ASMIT_BCC);
			break;
		}

		case IA_CMPLS:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			NativeCodeBasicBlock* sblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(sblock, trueJump, ASMIT_BPL);
			sblock->Close(eblock, falseJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ival));
			eblock->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		}

		case IA_CMPGS:
		{
			NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
			NativeCodeBasicBlock* sblock = nproc->AllocateBlock();
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 1));
			this->Close(sblock, falseJump, ASMIT_BPL);
			sblock->Close(eblock, trueJump, ASMIT_BEQ);
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ival));
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[rt] + 0));
			eblock->Close(trueJump, falseJump, ASMIT_BCC);
			break;
		}

		}
	}
#endif
	else
	{
		NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
		NativeCodeBasicBlock* nblock = nproc->AllocateBlock();

		int	li = 1, ri = 0;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 0; ri = 1;
		}

		int		iconst = 0;
		bool	rconst = false;

		if (ins->mSrc[li].mTemp < 0 && (op == IA_CMPGES || op == IA_CMPLS) && int16(ins->mSrc[li].mIntConst) > - 32768)
		{
			iconst = ins->mSrc[li].mIntConst - 1;
			rconst = true;
			li = ri; ri = 1 - li;

			NativeCodeBasicBlock* t = trueJump; trueJump = falseJump; falseJump = t;
		}
		else if (ins->mSrc[li].mTemp < 0 && (op == IA_CMPLES || op == IA_CMPGS) && int16(ins->mSrc[li].mIntConst) < 32767)
		{
			iconst = ins->mSrc[li].mIntConst + 1;
			rconst = true;
			li = ri; ri = 1 - li;

			NativeCodeBasicBlock* t = trueJump; trueJump = falseJump; falseJump = t;
		}
		else if (ins->mSrc[ri].mTemp < 0)
		{
			iconst = ins->mSrc[ri].mIntConst;
			rconst = true;
		}

		if (op >= IA_CMPGES && ins->mOperator <= IA_CMPLS)
		{
			if (!rconst)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK));
			}

			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ((ins->mSrc[li].mIntConst >> 8) & 0xff) ^ 0x80));
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			}

			if (rconst)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ((iconst >> 8) & 0xff) ^ 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_WORK));
		}
		else
		{
			if (ins->mSrc[li].mTemp < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[li].mIntConst >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp] + 1));
			if (rconst)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (iconst >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp] + 1));
		}

		this->Close(nblock, eblock, ASMIT_BNE);

		if (ins->mSrc[li].mTemp < 0)
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[li].mIntConst & 0xff));
		else
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[li].mTemp]));
		if (rconst)
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, iconst & 0xff));
		else
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[ri].mTemp]));

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
}

void NativeCodeBasicBlock::LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0)
{
	bool		isub = false;
	int			ireg = ins->mSrc[0].mTemp;
	AsmInsType	iop = ASMIT_ADC;

	if (sins0)
	{
		isub = true;
		ireg = sins0->mSrc[0].mTemp;
		iop = ASMIT_SBC;
	}

	if (sins1)
	{
		if (ins->mSrc[0].mTemp < 0 && ins->mSrc[0].mIntConst == 0)
			LoadValueToReg(proc, sins1, ins->mDst.mTemp, nullptr, nullptr);
		else
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				NativeCodeInstruction	ainsl(ASMIT_ADC, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff);
				NativeCodeInstruction	ainsh(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff);

				LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp], &ainsl, &ainsh);
			}
			else
			{
				NativeCodeInstruction	ainsl(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]);
				NativeCodeInstruction	ainsh(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1);

				LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp], &ainsl, &ainsh);
			}
		}
	}
	else if (ins->mSrc[1].mTemp < 0)
	{

		mIns.Push(NativeCodeInstruction(isub ? ASMIT_SEC : ASMIT_CLC, ASMIM_IMPLIED));
		if (ins->mSrc[1].mMemory == IM_GLOBAL)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, NCIF_LOWER));
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			// if the global variable is smaller than 256 bytes, we can safely ignore the upper byte?
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[1].mIntConst, ins->mSrc[1].mLinkerObject, NCIF_UPPER));
#if 1
			if (ins->mSrc[1].mLinkerObject->mSize < 256 || ins->mSrc[0].IsUByte())
				mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, 0));
			else
#endif
				mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		}
		else if (ins->mSrc[1].mMemory == IM_ABSOLUTE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[1].mIntConst & 0xff));
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[1].mIntConst >> 8) & 0xff));
#if 1
			if (ins->mSrc[0].IsUByte())
				mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, 0));
			else
#endif
				mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		}
		else if (ins->mSrc[1].mMemory == IM_PROCEDURE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mSrc[1].mLinkerObject, NCIF_LOWER));
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mSrc[1].mLinkerObject, NCIF_UPPER));
#if 1
			if (ins->mSrc[0].IsUByte())
				mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, 0));
			else
#endif
				mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		}
		else if (ins->mSrc[1].mMemory == IM_FPARAM)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, BC_REG_FPARAMS + ins->mSrc[1].mVarIndex + ins->mSrc[1].mIntConst));
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			if (ins->mSrc[0].IsUByte())
				mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, 0));
			else
				mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
		}
	}
	else
	{
		if (ins->mSrc[0].mTemp >= 0 || ins->mSrc[0].mIntConst != 0)
			mIns.Push(NativeCodeInstruction(isub ? ASMIT_SEC : ASMIT_CLC, ASMIM_IMPLIED));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp]));

		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mIntConst)
				mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
		}
		else
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg]));

		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp]));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[1].mTemp] + 1));

		if (ins->mSrc[0].mTemp < 0)
		{
			if (ins->mSrc[0].mIntConst)
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
		}
		else if (ins->mSrc[0].IsUByte())
			mIns.Push(NativeCodeInstruction(iop, ASMIM_IMMEDIATE, 0));
		else
			mIns.Push(NativeCodeInstruction(iop, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ireg] + 1));

		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
	}
}

void NativeCodeBasicBlock::CallFunction(InterCodeProcedure* proc, NativeCodeProcedure * nproc, const InterInstruction * ins)
{
	if (ins->mSrc[0].mTemp < 0)
	{
		if (ins->mSrc[0].mLinkerObject)
		{
			NativeCodeInstruction	lins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, NCIF_LOWER);
			NativeCodeInstruction	hins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject, NCIF_UPPER);

			mIns.Push(lins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
			mIns.Push(hins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
		else
		{
			NativeCodeInstruction	lins(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff);
			NativeCodeInstruction	hins(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff);

			mIns.Push(lins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
			mIns.Push(hins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		}
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
	}

	NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bcexec")));
	mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_FEXEC));

	if (ins->mDst.mTemp >= 0)
	{
		if (ins->mDst.mType == IT_FLOAT)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
			if (InterTypeSize[ins->mDst.mType] > 1)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
			}
			if (InterTypeSize[ins->mDst.mType] > 2)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
			}
		}
	}
}

void NativeCodeBasicBlock::CallAssembler(InterCodeProcedure* proc, NativeCodeProcedure * nproc, const InterInstruction* ins)
{
	if (ins->mCode == IC_ASSEMBLER)
	{
		for (int i = 1; i < ins->mNumOperands; i++)
		{
			if (ins->mSrc[i].mTemp < 0)
			{
				if (ins->mSrc[i].mMemory == IM_FPARAM)
					ins->mSrc[0].mLinkerObject->mTemporaries[i - 1] = BC_REG_FPARAMS + ins->mSrc[i].mVarIndex + ins->mSrc[i].mIntConst;
			}
			else
				ins->mSrc[0].mLinkerObject->mTemporaries[i - 1] = BC_REG_TMP + proc->mTempOffset[ins->mSrc[i].mTemp];
			ins->mSrc[0].mLinkerObject->mTempSizes[i - 1] = InterTypeSize[ins->mSrc[i].mType];
		}
		ins->mSrc[0].mLinkerObject->mNumTemporaries = ins->mNumOperands - 1;
	}

	if (ins->mSrc[0].mTemp < 0)
	{
		assert(ins->mSrc[0].mLinkerObject);
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, ins->mSrc[0].mIntConst, ins->mSrc[0].mLinkerObject));
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSrc[0].mTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bcexec")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME | NCIF_LOWER | NCIF_UPPER | NCIF_FEXEC));
	}

	if (ins->mDst.mTemp >= 0)
	{
		if (ins->mDst.mType == IT_FLOAT)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 0));
			if (InterTypeSize[ins->mDst.mType] > 1)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 1));
			}
			if (InterTypeSize[ins->mDst.mType] > 2)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mDst.mTemp] + 3));
			}
		}
	}
}

void NativeCodeBasicBlock::BuildLocalRegSets(void)
{
	int i;

	if (!mVisited)
	{
		mVisited = true;

		mLocalRequiredRegs = NumberSet(NUM_REGS);
		mLocalProvidedRegs = NumberSet(NUM_REGS);

		mEntryRequiredRegs = NumberSet(NUM_REGS);
		mEntryProvidedRegs = NumberSet(NUM_REGS);
		mExitRequiredRegs = NumberSet(NUM_REGS);
		mExitProvidedRegs = NumberSet(NUM_REGS);

		for (i = 0; i < mIns.Size(); i++)
		{
			mIns[i].FilterRegUsage(mLocalRequiredRegs, mLocalProvidedRegs);
		}

		switch (mBranch)
		{
		case ASMIT_BCC:
		case ASMIT_BCS:
			if (!mLocalProvidedRegs[CPU_REG_C])
				mLocalRequiredRegs += CPU_REG_C;
			break;
		case ASMIT_BEQ:
		case ASMIT_BNE:
		case ASMIT_BMI:
		case ASMIT_BPL:
			if (!mLocalProvidedRegs[CPU_REG_Z])
				mLocalRequiredRegs += CPU_REG_Z;
			break;
		}

		mEntryRequiredRegs = mLocalRequiredRegs;
		mExitProvidedRegs = mLocalProvidedRegs;

		if (mTrueJump) mTrueJump->BuildLocalRegSets();
		if (mFalseJump) mFalseJump->BuildLocalRegSets();
	}

}

void NativeCodeBasicBlock::BuildGlobalProvidedRegSet(NumberSet fromProvidedRegs)
{
	if (!mVisited || !(fromProvidedRegs <= mEntryProvidedRegs))
	{
		mEntryProvidedRegs |= fromProvidedRegs;
		fromProvidedRegs |= mExitProvidedRegs;

		mVisited = true;

		if (mTrueJump) mTrueJump->BuildGlobalProvidedRegSet(fromProvidedRegs);
		if (mFalseJump) mFalseJump->BuildGlobalProvidedRegSet(fromProvidedRegs);
	}

}

bool NativeCodeBasicBlock::BuildGlobalRequiredRegSet(NumberSet& fromRequiredRegs)
{
	bool revisit = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet	newRequiredRegs(mExitRequiredRegs);

		if (mTrueJump && mTrueJump->BuildGlobalRequiredRegSet(newRequiredRegs)) revisit = true;
		if (mFalseJump && mFalseJump->BuildGlobalRequiredRegSet(newRequiredRegs)) revisit = true;

		if (!(newRequiredRegs <= mExitRequiredRegs))
		{
			revisit = true;

			mExitRequiredRegs = newRequiredRegs;
			newRequiredRegs -= mLocalProvidedRegs;
			mEntryRequiredRegs |= newRequiredRegs;
		}

	}

	fromRequiredRegs |= mEntryRequiredRegs;

	return revisit;

}

bool NativeCodeBasicBlock::RemoveUnusedResultInstructions(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		NumberSet		requiredRegs(mExitRequiredRegs);
		int i;

		switch (mBranch)
		{
		case ASMIT_BCC:
		case ASMIT_BCS:
			requiredRegs += CPU_REG_C;
			break;
		case ASMIT_BEQ:
		case ASMIT_BNE:
		case ASMIT_BMI:
		case ASMIT_BPL:
			requiredRegs += CPU_REG_Z;
			break;
		}

		for (i = mIns.Size() - 1; i >= 0; i--)
		{
			if (mIns[i].mType != ASMIT_NOP && !mIns[i].IsUsedResultInstructions(requiredRegs))
			{
				if (i > 0 && mIns[i - 1].mMode == ASMIM_RELATIVE && mIns[i - 1].mAddress > 0)
				{
					mIns[i - 1].mType = ASMIT_NOP;
					mIns[i - 1].mMode = ASMIM_IMPLIED;
				}
				mIns[i].mType = ASMIT_NOP;
				mIns[i].mMode = ASMIM_IMPLIED;
				changed = true;
			}
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

void NativeCodeBasicBlock::CountEntries(NativeCodeBasicBlock * fromJump)
{
	if (mVisiting)
		mLoopHead = true;

	if (mNumEntries == 0)
		mFromJump = fromJump;
	else
		mFromJump = nullptr;
	mNumEntries++;

	if (!mVisited)
	{
		mVisited = true;
		mVisiting = true;

		if (mTrueJump)
			mTrueJump->CountEntries(this);
		if (mFalseJump)
			mFalseJump->CountEntries(this);

		mVisiting = false;
	}
}

bool NativeCodeBasicBlock::MergeBasicBlocks(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

		mVisited = true;

		if (!mLocked)
		{
			while (mTrueJump && !mFalseJump && mTrueJump->mNumEntries == 1 && mTrueJump != this && !mTrueJump->mLocked)
			{
				for (int i = 0; i < mTrueJump->mIns.Size(); i++)
					mIns.Push(mTrueJump->mIns[i]);
				mBranch = mTrueJump->mBranch;
				mFalseJump = mTrueJump->mFalseJump;
				mTrueJump = mTrueJump->mTrueJump;
				changed = true;
			}

			while (mTrueJump && mTrueJump->mIns.Size() == 0 && !mTrueJump->mFalseJump && !mTrueJump->mLocked && mTrueJump != this && mTrueJump->mTrueJump != mTrueJump)
			{
				mTrueJump->mNumEntries--;
				mTrueJump = mTrueJump->mTrueJump;
				mTrueJump->mNumEntries++;
				changed = true;
			}

			while (mFalseJump && mFalseJump->mIns.Size() == 0 && !mFalseJump->mFalseJump && !mFalseJump->mLocked && mFalseJump != this && mFalseJump->mTrueJump != mFalseJump)
			{
				mFalseJump->mNumEntries--;
				mFalseJump = mFalseJump->mTrueJump;
				mFalseJump->mNumEntries++;
				changed = true;
			}

			if (mTrueJump && mTrueJump == mFalseJump)
			{
				mBranch = ASMIT_JMP;
				mFalseJump = nullptr;
				changed = true;
			}
		}

		if (mTrueJump)
			mTrueJump->MergeBasicBlocks();
		if (mFalseJump)
			mFalseJump->MergeBasicBlocks();

		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
	}
	return changed;
}

void NativeCodeBasicBlock::CollectEntryBlocks(NativeCodeBasicBlock* block)
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

void NativeCodeBasicBlock::BuildEntryDataSet(const NativeRegisterDataSet& set)
{
	if (!mVisited)
		mEntryRegisterDataSet = set;
	else
	{
		bool	changed = false;
		for (int i = 0; i < NUM_REGS; i++)
		{
			if (set.mRegs[i].mMode == NRDM_IMMEDIATE)
			{
				if (mEntryRegisterDataSet.mRegs[i].mMode == NRDM_IMMEDIATE && set.mRegs[i].mValue == mEntryRegisterDataSet.mRegs[i].mValue)
				{
				}
				else if (mEntryRegisterDataSet.mRegs[i].mMode != NRDM_UNKNOWN)
				{
					mEntryRegisterDataSet.mRegs[i].Reset();
					mVisited = false;
				}
			}
			else if (set.mRegs[i].mMode == NRDM_IMMEDIATE_ADDRESS)
			{
				if (mEntryRegisterDataSet.mRegs[i].mMode == NRDM_IMMEDIATE_ADDRESS &&
					set.mRegs[i].mValue == mEntryRegisterDataSet.mRegs[i].mValue &&
					set.mRegs[i].mLinkerObject == mEntryRegisterDataSet.mRegs[i].mLinkerObject &&
					set.mRegs[i].mFlags == mEntryRegisterDataSet.mRegs[i].mFlags)
				{
				}
				else if (mEntryRegisterDataSet.mRegs[i].mMode != NRDM_UNKNOWN)
				{
					mEntryRegisterDataSet.mRegs[i].Reset();
					mVisited = false;
				}
			}
			else if (mEntryRegisterDataSet.mRegs[i].mMode != NRDM_UNKNOWN)
			{
				mEntryRegisterDataSet.mRegs[i].Reset();
				mVisited = false;
			}
		}
	}

	if (!mVisited)
	{
		mVisited = true;

		mNDataSet = mEntryRegisterDataSet;

		for (int i = 0; i < mIns.Size(); i++)
			mIns[i].Simulate(mNDataSet);

		if (mTrueJump)
			mTrueJump->BuildEntryDataSet(mNDataSet);
		if (mFalseJump)
			mFalseJump->BuildEntryDataSet(mNDataSet);
	}
}

bool NativeCodeBasicBlock::ApplyEntryDataSet(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		mNDataSet = mEntryRegisterDataSet;

		for (int i = 0; i < mIns.Size(); i++)
		{
			if (mIns[i].ApplySimulation(mNDataSet))
				changed = true;
			mIns[i].Simulate(mNDataSet);
		}

		if (mTrueJump && mTrueJump->ApplyEntryDataSet())
			changed = true;
		if (mFalseJump && mFalseJump->ApplyEntryDataSet())
			changed = true;
	}

	return changed;
}

void NativeCodeBasicBlock::FindZeroPageAlias(const NumberSet& statics, NumberSet& invalid, uint8* alias, int accu)
{
	if (!mVisited)
	{
		mVisited = true;

		if (mNumEntries > 1)
			accu = -1;

		for (int i = 0; i < mIns.Size(); i++)
		{
			if (mIns[i].mMode == ASMIM_ZERO_PAGE)
			{
				if (mIns[i].mType == ASMIT_LDA)
					accu = mIns[i].mAddress;
				else if (mIns[i].mType == ASMIT_STA)
				{
					if (accu < 0 || !statics[accu])
						invalid += mIns[i].mAddress;
					else if (alias[mIns[i].mAddress])
					{
						if (alias[mIns[i].mAddress] != accu)
							invalid += mIns[i].mAddress;
					}
					else
					{
						alias[mIns[i].mAddress] = accu;
					}
				}
				else if (mIns[i].ChangesAccu())
					accu = -1;
				else if (mIns[i].ChangesAddress())
					invalid += mIns[i].mAddress;
			}
			else if (mIns[i].ChangesAccu())
				accu = -1;
		}

		if (mTrueJump)
			mTrueJump->FindZeroPageAlias(statics, invalid, alias, accu);
		if (mFalseJump)
			mFalseJump->FindZeroPageAlias(statics, invalid, alias, accu);
	}
}

void NativeCodeBasicBlock::CollectZeroPageSet(ZeroPageSet& locals, ZeroPageSet& global)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			switch (mIns[i].mMode)
			{
			case ASMIM_ZERO_PAGE:
				if (mIns[i].ChangesAddress())
					locals += mIns[i].mAddress;
				break;
			case ASMIM_ABSOLUTE:
				if (mIns[i].mType == ASMIT_JSR)
				{
					if ((mIns[i].mFlags & NCIF_RUNTIME) && !(mIns[i].mFlags & NCIF_FEXEC))
					{
						for (int j = 0; j < 4; j++)
						{
							locals += BC_REG_ACCU + j;
							locals += BC_REG_WORK + j;
						}
					}

					if (mIns[i].mLinkerObject)
					{
						LinkerObject* lo = mIns[i].mLinkerObject;

						global |= lo->mZeroPageSet;
					}
				}
				break;
			}
		}

		if (mTrueJump)
			mTrueJump->CollectZeroPageSet(locals, global);
		if (mFalseJump)
			mFalseJump->CollectZeroPageSet(locals, global);
	}
}

void NativeCodeBasicBlock::CollectZeroPageUsage(NumberSet& used, NumberSet &modified, NumberSet& pairs)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			switch (mIns[i].mMode)
			{
			case ASMIM_ZERO_PAGE:
				used += mIns[i].mAddress;
				if (mIns[i].ChangesAddress())
					modified += mIns[i].mAddress;
				break;
			case ASMIM_INDIRECT_Y:
				used += mIns[i].mAddress + 0;
				used += mIns[i].mAddress + 1;
				pairs += mIns[i].mAddress;
				break;
			case ASMIM_ABSOLUTE:
				if (mIns[i].mType == ASMIT_JSR)
				{
					if (mIns[i].mFlags & NCIF_RUNTIME)
					{
						for (int j = 0; j < 4; j++)
						{
							used += BC_REG_ACCU + j;
							used += BC_REG_WORK + j;
							modified += BC_REG_ACCU + j;
							modified += BC_REG_WORK + j;
						}
					}

					if (mIns[i].mLinkerObject)
					{
						LinkerObject* lo = mIns[i].mLinkerObject;

						for (int i = 0; i < lo->mNumTemporaries; i++)
						{
							for (int j = 0; j < lo->mTempSizes[i]; j++)
								used += lo->mTemporaries[i] + j;
						}
					}
				}
				break;
			}
		}

		if (mTrueJump)
			mTrueJump->CollectZeroPageUsage(used, modified, pairs);
		if (mFalseJump)
			mFalseJump->CollectZeroPageUsage(used, modified, pairs);
	}
}

void NativeCodeBasicBlock::GlobalRegisterXMap(int reg)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			NativeCodeInstruction& ins(mIns[i]);
			if (ins.mMode == ASMIM_ZERO_PAGE && ins.mAddress == reg)
			{
				switch (ins.mType)
				{
				case ASMIT_STA:
					ins.mType = ASMIT_TAX;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_LDA:
					ins.mType = ASMIT_TXA;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_INC:
					ins.mType = ASMIT_INX;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_DEC:
					ins.mType = ASMIT_DEX;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_LDX:
					assert(ins.mAddress == reg);
					ins.mType = ASMIT_NOP;
					ins.mMode = ASMIM_IMPLIED;
					break;
				}
			}
		}

		if (mTrueJump)
			mTrueJump->GlobalRegisterXMap(reg);
		if (mFalseJump)
			mFalseJump->GlobalRegisterXMap(reg);
	}
}

void NativeCodeBasicBlock::GlobalRegisterYMap(int reg)
{
	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			NativeCodeInstruction& ins(mIns[i]);
			if (ins.mMode == ASMIM_ZERO_PAGE && ins.mAddress == reg)
			{
				switch (ins.mType)
				{
				case ASMIT_STA:
					ins.mType = ASMIT_TAY;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_LDA:
					ins.mType = ASMIT_TYA;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_INC:
					ins.mType = ASMIT_INY;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_DEC:
					ins.mType = ASMIT_DEY;
					ins.mMode = ASMIM_IMPLIED;
					break;
				case ASMIT_LDY:
					assert(ins.mAddress == reg);
					ins.mType = ASMIT_NOP;
					ins.mMode = ASMIM_IMPLIED;
					break;
				}
			}
		}

		if (mTrueJump)
			mTrueJump->GlobalRegisterYMap(reg);
		if (mFalseJump)
			mFalseJump->GlobalRegisterYMap(reg);
	}
}

bool NativeCodeBasicBlock::ReduceLocalYPressure(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int	loadYRegs[256];

		int start = 0;

		while (start < mIns.Size())
		{
			while (start < mIns.Size() && (mIns[start].mLive & LIVE_CPU_REG_X))
				start++;

			if (start < mIns.Size())
			{
				int end = start + 1;

				for (int i = 0; i < 256; i++)
					loadYRegs[i] = 0;

				int	yreg = -1, areg = -1;
				while (end < mIns.Size() && !mIns[end].ChangesXReg())
				{
					const NativeCodeInstruction& ins(mIns[end]);

					if (ins.mType == ASMIT_LDY && ins.mMode == ASMIM_ZERO_PAGE)
					{
						yreg = ins.mAddress;
						if (loadYRegs[yreg] >= 0)
							loadYRegs[yreg]++;
					}
					else if (ins.mType == ASMIT_TAY)
					{
						yreg = areg;
						if (yreg > 0 && loadYRegs[yreg] >= 0)
							loadYRegs[yreg]++;
					}
					else if (ins.mType == ASMIT_STA && ins.mMode == ASMIM_ZERO_PAGE)
					{
						areg = ins.mAddress;
					}
					else if (ins.mMode == ASMIM_INDIRECT_Y)
					{
						if (yreg != -1)
							loadYRegs[yreg] = -1;
					}
					else if (ins.ChangesYReg())
						yreg = -1;
					else if (ins.ChangesAccu())
						areg = -1;

					end++;
				}

				if (end > start)
				{
					int	maxy = 0, maxr = 0;
					for (int i = 1; i < 256; i++)
					{
						if (loadYRegs[i] > maxy)
						{
							maxy = loadYRegs[i];
							maxr = i;
						}
					}

					if (maxy > 1)
					{
						bool	active = false;
						int		aactive = false;
						for (int i = start; i < end; i++)
						{
							NativeCodeInstruction& ins(mIns[i]);
							if (ins.mType == ASMIT_LDY && ins.mMode == ASMIM_ZERO_PAGE && ins.mAddress == maxr)
							{
								ins.mType = ASMIT_LDX;
								active = true;
								aactive = false;
							}
							else if ((ins.mType == ASMIT_LDA || ins.mType == ASMIT_STA) && ins.mMode == ASMIM_ZERO_PAGE && ins.mAddress == maxr)
							{
								aactive = true;
							}
							else if (active)
							{
								aactive = false;
								if (ins.mType == ASMIT_INY)
									ins.mType = ASMIT_INX;
								else if (ins.mType == ASMIT_DEY)
									ins.mType = ASMIT_DEX;
								else if (ins.mType == ASMIT_CPY)
									ins.mType = ASMIT_CPX;
								else if (ins.mType == ASMIT_TYA)
									ins.mType = ASMIT_TXA;
								else if (ins.mType == ASMIT_STY)
									ins.mType = ASMIT_STX;
								else if (ins.mMode == ASMIM_ABSOLUTE_Y)
								{
									assert(HasAsmInstructionMode(ins.mType, ASMIM_ABSOLUTE_X));
									ins.mMode = ASMIM_ABSOLUTE_X;
								}
								else if (ins.mType == ASMIT_LDY || ins.mType == ASMIT_TAY)
									active = false;
							}
							else if (aactive)
							{
								if (ins.mType == ASMIT_TAY)
								{
									ins.mType = ASMIT_TAX;
									active = true;
								}
								else if (ins.ChangesAccu())
									aactive = false;
							}
						}

						changed = true;
					}
				}
		
				start = end;
			}
		}

		if (mTrueJump && mTrueJump->ReduceLocalYPressure())
			changed = true;

		if (mFalseJump && mFalseJump->ReduceLocalYPressure())
			changed = true;
	}

	return changed;
}

bool NativeCodeBasicBlock::LocalRegisterXYMap(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		int		xregs[256], yregs[256];
		for (int i = 0; i < 256; i++)
			xregs[i] = yregs[i] = -1;

		for (int i = 0; i < mIns.Size(); i++)
		{
			if (i + 1 < mIns.Size() && mIns[i].mType == ASMIT_LDA && (mIns[i].mMode != ASMIM_ZERO_PAGE || (xregs[mIns[i].mAddress] < 0 && xregs[mIns[i].mAddress] < 0) || (mIns[i + 1].mLive & LIVE_MEM)))
			{
				if (mIns[i + 1].IsCommutative() && mIns[i + 1].mMode == ASMIM_ZERO_PAGE)
				{
					if (xregs[mIns[i + 1].mAddress] >= 0 && !(mIns[i + 1].mLive & LIVE_MEM))
					{
						int		addr = mIns[i + 1].mAddress;
						mIns[i + 1].CopyMode(mIns[i]);
						mIns[i].mMode = ASMIM_ZERO_PAGE;
						mIns[i].mAddress = addr;
					}
					else if (yregs[mIns[i + 1].mAddress] >= 0 && !(mIns[i + 1].mLive & LIVE_MEM))
					{
						int		addr = mIns[i + 1].mAddress;
						mIns[i + 1].CopyMode(mIns[i]);
						mIns[i].mMode = ASMIM_ZERO_PAGE;
						mIns[i].mAddress = addr;
					}
				}
			}

			const NativeCodeInstruction& ins(mIns[i]);

			if (ins.ChangesXReg())
			{
				for (int i = 0; i < 256; i++)
					xregs[i] = -1;
			}
			if (ins.ChangesYReg())
			{
				for (int i = 0; i < 256; i++)
					yregs[i] = -1;
			}

			if (ins.mMode == ASMIM_ZERO_PAGE)
			{
				switch (ins.mType)
				{
				case ASMIT_STA:
					if (ins.mAddress >= BC_REG_ACCU && ins.mAddress < BC_REG_ACCU + 4 ||
						ins.mAddress >= BC_REG_WORK && ins.mAddress < BC_REG_WORK + 4)
					{
					}
					else
					{
						if (!(ins.mLive & LIVE_CPU_REG_X))
						{
							if (xregs[ins.mAddress] < 0)
								xregs[ins.mAddress] = i;
						}
						if (!(ins.mLive & LIVE_CPU_REG_Y))
						{
							if (yregs[ins.mAddress] < 0)
								yregs[ins.mAddress] = i;
						}
					}
					break;
				case ASMIT_LDA:
					if (xregs[ins.mAddress] >= 0 && !(ins.mLive & LIVE_MEM))
					{
						changed = true;

						for (int j = xregs[ins.mAddress]; j <= i; j++)
						{
							NativeCodeInstruction& rins(mIns[j]);
							if (rins.mMode == ASMIM_ZERO_PAGE && rins.mAddress == ins.mAddress)
							{
								switch (rins.mType)
								{
								case ASMIT_STA:
									rins.mType = ASMIT_TAX;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_LDA:
									rins.mType = ASMIT_TXA;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_INC:
									rins.mType = ASMIT_INX;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_DEC:
									rins.mType = ASMIT_DEX;
									rins.mMode = ASMIM_IMPLIED;
									break;
								}
							}
							rins.mLive |= LIVE_CPU_REG_X;
						}
						for (int i = 0; i < 256; i++)
							xregs[i] = -1;
					}
					else if (yregs[ins.mAddress] >= 0 && !(ins.mLive & LIVE_MEM))
					{
						changed = true;

						for (int j = yregs[ins.mAddress]; j <= i; j++)
						{
							NativeCodeInstruction& rins(mIns[j]);
							if (rins.mMode == ASMIM_ZERO_PAGE && rins.mAddress == ins.mAddress)
							{
								switch (rins.mType)
								{
								case ASMIT_STA:
									rins.mType = ASMIT_TAY;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_LDA:
									rins.mType = ASMIT_TYA;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_INC:
									rins.mType = ASMIT_INY;
									rins.mMode = ASMIM_IMPLIED;
									break;
								case ASMIT_DEC:
									rins.mType = ASMIT_DEY;
									rins.mMode = ASMIM_IMPLIED;
									break;
								}
							}
							rins.mLive |= LIVE_CPU_REG_Y;
						}
						for (int i = 0; i < 256; i++)
							yregs[i] = -1;
					}
					break;
				case ASMIT_INC:
				case ASMIT_DEC:
					break;
				default:
					xregs[ins.mAddress + 0] = -1;
					yregs[ins.mAddress + 0] = -1;
					break;
				}
			}
			else if (ins.mMode == ASMIM_INDIRECT_Y)
			{
				xregs[ins.mAddress + 0] = -1;
				xregs[ins.mAddress + 1] = -1;
				yregs[ins.mAddress + 0] = -1;
				yregs[ins.mAddress + 1] = -1;
			}
		}

		if (mTrueJump && mTrueJump->LocalRegisterXYMap())
			changed = true;

		if (mFalseJump && mFalseJump->LocalRegisterXYMap())
			changed = true;
	}

	return changed;
}

void NativeCodeBasicBlock::GlobalRegisterXYCheck(int* xregs, int* yregs)
{
	if (!mVisited)
	{
		mVisited = true;

		int	yreg = -1;
		int xreg = -1;

		for (int i = 0; i < mIns.Size(); i++)
		{
			const NativeCodeInstruction& ins(mIns[i]);
			if (ins.mMode == ASMIM_ZERO_PAGE)
			{
				switch (ins.mType)
				{
				case ASMIT_LDA:
					if (yregs[ins.mAddress] >= 0)
						yregs[ins.mAddress]++;
					if (xregs[ins.mAddress] >= 0)
						xregs[ins.mAddress]++;
					break;
				case ASMIT_LDY:
					if (yregs[ins.mAddress] >= 0)
					{
						yregs[ins.mAddress] += 2;
						yreg = ins.mAddress;
					}
					for (int i = 1; i < 256; i++)
						if (i != ins.mAddress)
							yregs[i] = -1;

					xregs[ins.mAddress] = -1;
					break;
				case ASMIT_LDX:
					if (xregs[ins.mAddress] >= 0)
					{
						xregs[ins.mAddress] += 2;
						xreg = ins.mAddress;
					}
					for (int i = 1; i < 256; i++)
						if (i != ins.mAddress)
							xregs[i] = -1;

					yregs[ins.mAddress] = -1;
					break;
				case ASMIT_STA:
					if (yreg == ins.mAddress)
						yreg = -1;
					if (xreg == ins.mAddress)
						xreg = -1;

					if (ins.mLive & LIVE_CPU_REG_Z)
					{
						xregs[ins.mAddress + 0] = -1;
						yregs[ins.mAddress + 0] = -1;
					}
					else
					{
						if (yregs[ins.mAddress] >= 0)
							yregs[ins.mAddress]++;
						if (xregs[ins.mAddress] >= 0)
							xregs[ins.mAddress]++;
					}
					break;
				case ASMIT_INC:
				case ASMIT_DEC:
					if (yreg == ins.mAddress)
						yreg = -1;
					if (xreg == ins.mAddress)
						xreg = -1;

					if (yregs[ins.mAddress] >= 0)
						yregs[ins.mAddress] += 3;
					if (xregs[ins.mAddress] >= 0)
						xregs[ins.mAddress] += 3;
					break;
				default:
					xregs[ins.mAddress + 0] = -1;
					yregs[ins.mAddress + 0] = -1;
					break;
				}
			}
			else if (ins.mMode == ASMIM_INDIRECT_Y)
			{
				for (int i = 1; i < 256; i++)
					if (i != yreg)
						yregs[i] = -1;

				xregs[ins.mAddress + 0] = -1;
				xregs[ins.mAddress + 1] = -1;
				yregs[ins.mAddress + 0] = -1;
				yregs[ins.mAddress + 1] = -1;
			}
			else if (ins.mMode == ASMIM_ZERO_PAGE_X || ins.mMode == ASMIM_ZERO_PAGE_Y)
			{

			}
			else
			{
				if (ins.RequiresXReg())
				{
					for (int i = 1; i < 256; i++)
						if (i != xreg)
							xregs[i] = -1;
				}
				if (ins.RequiresYReg())
				{
					for (int i = 1; i < 256; i++)
						if (i != yreg)
							yregs[i] = -1;
				}
				if (ins.ChangesXReg())
					xregs[0] = -1;
				if (ins.ChangesYReg())
					yregs[0] = -1;
			}
		}

		if (xregs[0] >= 0 || yregs[0] >= 0)
		{
			if (mTrueJump)
				mTrueJump->GlobalRegisterXYCheck(xregs, yregs);
			if (mFalseJump)
				mFalseJump->GlobalRegisterXYCheck(xregs, yregs);
		}
	}
}

bool NativeCodeBasicBlock::RemapZeroPage(const uint8* remap)
{
	bool	modified = false;

	if (!mVisited)
	{
		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			int	addr;

			switch (mIns[i].mMode)
			{
			case ASMIM_ZERO_PAGE:
			case ASMIM_INDIRECT_Y:
				addr = remap[mIns[i].mAddress];
				if (addr != mIns[i].mAddress)
				{
					mIns[i].mAddress = addr;
					modified = true;
				}
				break;
			case ASMIM_ABSOLUTE:
				if (mIns[i].mType == ASMIT_JSR && mIns[i].mLinkerObject)
				{
					LinkerObject* lo = mIns[i].mLinkerObject;

					for (int j = 0; j < lo->mNumTemporaries; j++)
					{
						addr = remap[lo->mTemporaries[j]];
						if (addr != lo->mTemporaries[j])
						{
							lo->mTemporaries[j] = addr;
							modified = true;
						}
					}

					if (mIns[i].mFlags & NCIF_USE_ZP_32_X)
						mIns[i].mParam = remap[mIns[i].mParam];
				}
				break;
			}
		}

		if (mTrueJump && mTrueJump->RemapZeroPage(remap))
			modified = true;

		if (mFalseJump && mFalseJump->RemapZeroPage(remap))
			modified = true;
	}

	return modified;
}

bool NativeCodeBasicBlock::SameTail(const NativeCodeInstruction& ins) const
{
	if (mIns.Size() > 0)
		return mIns[mIns.Size() - 1].IsSame(ins);
	else
		return false;
}

void NativeCodeBasicBlock::AddEntryBlock(NativeCodeBasicBlock* block)
{
	int i = 0;
	while (i < mEntryBlocks.Size() && mEntryBlocks[i] != block)
		i++;
	if (i == mEntryBlocks.Size())
		mEntryBlocks.Push(block);
}

void NativeCodeBasicBlock::RemEntryBlock(NativeCodeBasicBlock* block)
{
	int i = 0;
	while (i < mEntryBlocks.Size() && mEntryBlocks[i] != block)
		i++;
	if (i < mEntryBlocks.Size())
		mEntryBlocks.Remove(i);
}

NativeCodeBasicBlock * NativeCodeBasicBlock::SplitMatchingTails(NativeCodeProcedure* proc)
{
	NativeCodeBasicBlock* nblock = nullptr;

	for (int i = 0; i < mEntryBlocks.Size() - 1; i++)
	{
		NativeCodeBasicBlock* bi(mEntryBlocks[i]);

		if (bi->mBranch == ASMIT_JMP && bi->mIns.Size() > 1)
		{
			for (int j = i + 1; j < mEntryBlocks.Size(); j++)
			{
				NativeCodeBasicBlock* bj(mEntryBlocks[j]);

				if (bj->mBranch == ASMIT_JMP && bj->mIns.Size() > 1)
				{
					if (bi->mIns[bi->mIns.Size() - 1].IsSame(bj->mIns[bj->mIns.Size() - 1]) &&
						bi->mIns[bi->mIns.Size() - 2].IsSame(bj->mIns[bj->mIns.Size() - 2]))
					{
						if (!nblock)
						{
							nblock = proc->AllocateBlock();
							nblock->mBranch = ASMIT_JMP;
							nblock->mVisited = false;
							nblock->mTrueJump = this;

							nblock->mEntryBlocks.Push(bi);
							bi->mTrueJump = nblock;
							mEntryBlocks[i] = nullptr;
						}

						nblock->mEntryBlocks.Push(bj);
						bj->mTrueJump = nblock;
						mEntryBlocks[j] = nullptr;
					}
				}
			}

			if (nblock)
			{
				int	i = 0;
				while (i < mEntryBlocks.Size())
				{
					if (mEntryBlocks[i])
						i++;
					else
						mEntryBlocks.Remove(i);
				}

				return nblock;
			}
		}
	}

	return nullptr;
}

bool NativeCodeBasicBlock::JoinTailCodeSequences(NativeCodeProcedure* proc)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;
#if 1
		if (mEntryBlocks.Size() > 1)
		{
			int i = 0;
			while (i < mEntryBlocks.Size() && mEntryBlocks[i]->mBranch == ASMIT_JMP)
				i++;
			if (i == mEntryBlocks.Size())
			{
				NativeCodeBasicBlock* eb = mEntryBlocks[0];

				while (eb->mIns.Size() > 0)
				{
					NativeCodeInstruction& ins(eb->mIns[eb->mIns.Size() - 1]);
					i = 1;
					while (i < mEntryBlocks.Size() && mEntryBlocks[i]->SameTail(ins))
						i++;
					if (i == mEntryBlocks.Size())
					{
						mIns.Insert(0, ins);
						for (int i = 0; i < mEntryBlocks.Size(); i++)
						{
							NativeCodeBasicBlock* b = mEntryBlocks[i];
							b->mIns.SetSize(b->mIns.Size() - 1);
						}
						changed = true;
					}
					else
						break;
				}
			}
			
			if (mEntryBlocks.Size() > 2)
			{
				NativeCodeBasicBlock* nblock = SplitMatchingTails(proc);
				if (nblock)
				{
					if (nblock->JoinTailCodeSequences(proc))
						changed = true;
				}
			}
		}
#endif
#if 1
		if (mTrueJump && mFalseJump && !mTrueJump->mFalseJump && !mFalseJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump)
		{
			if (mTrueJump->mIns.Size() > mFalseJump->mIns.Size())
			{
				if (mTrueJump->mTrueJump != mFalseJump)
				{
					int	i = 0, offset = mTrueJump->mIns.Size() - mFalseJump->mIns.Size();
					while (i < mFalseJump->mIns.Size() && mFalseJump->mIns[i].IsSame(mTrueJump->mIns[i + offset]))
						i++;
					if (i == mFalseJump->mIns.Size())
					{
						if (mTrueJump->mTrueJump)
							mTrueJump->mTrueJump->RemEntryBlock(mTrueJump);
						mTrueJump->mTrueJump = mFalseJump;
						if (mTrueJump->mTrueJump)
							mTrueJump->mTrueJump->AddEntryBlock(mTrueJump);
						mTrueJump->mIns.SetSize(offset);

						changed = true;
					}
				}
			}
			else
			{
				if (mFalseJump->mTrueJump != mTrueJump)
				{
					int	i = 0, offset = mFalseJump->mIns.Size() - mTrueJump->mIns.Size();
					while (i < mTrueJump->mIns.Size() && mTrueJump->mIns[i].IsSame(mFalseJump->mIns[i + offset]))
						i++;
					if (i == mTrueJump->mIns.Size())
					{
						if (mFalseJump->mTrueJump)
							mFalseJump->mTrueJump->RemEntryBlock(mFalseJump);
						mFalseJump->mTrueJump = mTrueJump;
						if (mFalseJump->mTrueJump)
							mFalseJump->mTrueJump->AddEntryBlock(mFalseJump);
						mFalseJump->mIns.SetSize(offset);
						changed = true;
					}
				}
			}
		}
#endif
#if 1
		if (mTrueJump && mTrueJump->mNumEntries == 1 && mFalseJump && mFalseJump->mNumEntries == 1)
		{
			int s = mIns.Size();
			if (s > 0 && mIns[s - 1].mType == ASMIT_CMP && mIns[s - 1].mMode == ASMIM_IMMEDIATE && !(mIns[s - 1].mLive & LIVE_CPU_REG_X))
			{
				while (mTrueJump->mIns.Size() > 1 && mFalseJump->mIns.Size() > 1 &&

					((mTrueJump->mIns[0].mType == ASMIT_LDA && mTrueJump->mIns[1].mType == ASMIT_STA && !(mTrueJump->mIns[1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z))) ||
					 (mTrueJump->mIns[0].mType == ASMIT_LDX && mTrueJump->mIns[1].mType == ASMIT_STX && !(mTrueJump->mIns[1].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))) &&
					((mFalseJump->mIns[0].mType == ASMIT_LDA && mFalseJump->mIns[1].mType == ASMIT_STA && !(mFalseJump->mIns[1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z))) ||
					 (mFalseJump->mIns[0].mType == ASMIT_LDX && mFalseJump->mIns[1].mType == ASMIT_STX && !(mFalseJump->mIns[1].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))) &&
					mTrueJump->mIns[0].SameEffectiveAddress(mFalseJump->mIns[0]) && mTrueJump->mIns[1].SameEffectiveAddress(mFalseJump->mIns[1]) &&
					HasAsmInstructionMode(ASMIT_LDX, mTrueJump->mIns[0].mMode) && HasAsmInstructionMode(ASMIT_STX, mTrueJump->mIns[1].mMode))
				{
					mTrueJump->mIns[0].mType = ASMIT_LDX;
					mTrueJump->mIns[0].mLive |= LIVE_CPU_REG_X;
					mTrueJump->mIns[1].mType = ASMIT_STX;

					mIns.Insert(s - 1, mTrueJump->mIns[0]);
					mIns.Insert(s, mTrueJump->mIns[1]);
					s += 2;
					mTrueJump->mIns.Remove(0); mTrueJump->mIns.Remove(0);
					mFalseJump->mIns.Remove(0); mFalseJump->mIns.Remove(0);

					changed = true;
				}
			}
			else if (s > 0 && mIns[s - 1].mType == ASMIT_LDA && !(mIns[s - 1].mLive & LIVE_CPU_REG_X))
			{
				while (mTrueJump->mIns.Size() > 1 && mFalseJump->mIns.Size() > 1 &&

					((mTrueJump->mIns[0].mType == ASMIT_LDA && mTrueJump->mIns[1].mType == ASMIT_STA && !(mTrueJump->mIns[1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z))) ||
						(mTrueJump->mIns[0].mType == ASMIT_LDX && mTrueJump->mIns[1].mType == ASMIT_STX && !(mTrueJump->mIns[1].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))) &&
					((mFalseJump->mIns[0].mType == ASMIT_LDA && mFalseJump->mIns[1].mType == ASMIT_STA && !(mFalseJump->mIns[1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z))) ||
						(mFalseJump->mIns[0].mType == ASMIT_LDX && mFalseJump->mIns[1].mType == ASMIT_STX && !(mFalseJump->mIns[1].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))) &&
					mTrueJump->mIns[0].SameEffectiveAddress(mFalseJump->mIns[0]) && mTrueJump->mIns[1].SameEffectiveAddress(mFalseJump->mIns[1]) &&
					HasAsmInstructionMode(ASMIT_LDX, mTrueJump->mIns[0].mMode) && HasAsmInstructionMode(ASMIT_STX, mTrueJump->mIns[1].mMode) &&
					!mIns[s - 1].MayBeChangedOnAddress(mTrueJump->mIns[1]))
				{
					mTrueJump->mIns[0].mType = ASMIT_LDX;
					mTrueJump->mIns[0].mLive |= LIVE_CPU_REG_X;
					mTrueJump->mIns[1].mType = ASMIT_STX;

					mIns.Insert(s - 1, mTrueJump->mIns[0]);
					mIns.Insert(s, mTrueJump->mIns[1]);
					s += 2;
					mTrueJump->mIns.Remove(0); mTrueJump->mIns.Remove(0);
					mFalseJump->mIns.Remove(0); mFalseJump->mIns.Remove(0);

					changed = true;
				}
			}
		}
#endif
		if (mTrueJump && mTrueJump->JoinTailCodeSequences(proc))
			changed = true;
		if (mFalseJump && mFalseJump->JoinTailCodeSequences(proc))
			changed = true;
	}

	return changed;
}

bool NativeCodeBasicBlock::FindPageStartAddress(int at, int reg, int& addr)
{
	int	j = at - 2;
	while (j >= 0)
	{
		if (mIns[j + 0].mType == ASMIT_LDA && mIns[j + 0].mMode == ASMIM_IMMEDIATE && 
			mIns[j + 1].mType == ASMIT_STA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE && mIns[j + 1].mAddress == reg + 1)
		{
			addr = mIns[j + 0].mAddress << 8;
			return true;
		}
		if (mIns[j + 1].mMode == ASMIM_ZERO_PAGE && (mIns[j + 1].mAddress == reg || mIns[j + 1].mAddress == reg + 1) && mIns[j + 1].ChangesAddress())
			return false;

		j--;
	}

	if (mFromJump)
		return mFromJump->FindPageStartAddress(mFromJump->mIns.Size(), reg, addr);
	else
		return false;
}

bool NativeCodeBasicBlock::FindGlobalAddress(int at, int reg, int& apos)
{
	int j = at - 4;
	while (j >= 0)
	{
		if (mIns[j + 0].mType == ASMIT_LDA && mIns[j + 0].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 0].mFlags & NCIF_LOWER) && mIns[j + 0].mLinkerObject &&
			mIns[j + 1].mType == ASMIT_STA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE && mIns[j + 1].mAddress == reg &&
			mIns[j + 2].mType == ASMIT_LDA && mIns[j + 2].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 2].mFlags & NCIF_UPPER) && mIns[j + 2].mLinkerObject == mIns[j + 0].mLinkerObject &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg + 1)
		{
			apos = j + 0;
			return true;
		}
		if (mIns[j + 3].mMode == ASMIM_ZERO_PAGE && (mIns[j + 3].mAddress == reg || mIns[j + 3].mAddress == reg + 1) && mIns[j + 3].ChangesAddress())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::FindImmediateStore(int at, int reg, const NativeCodeInstruction*& ains)
{
	int	j = at - 1;
	while (j >= 0)
	{
		if (mIns[j + 0].mType == ASMIT_LDA && (mIns[j + 0].mMode == ASMIM_IMMEDIATE || mIns[j + 0].mMode == ASMIM_IMMEDIATE_ADDRESS) &&
			mIns[j + 1].mType == ASMIT_STA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE && mIns[j + 1].mAddress == reg)
		{
			ains = &(mIns[j + 0]);
			return true;
		}
		else if (mIns[j + 1].ChangesZeroPage(reg))
			return false;
		j--;
	}

	return false;
}


bool NativeCodeBasicBlock::FindGlobalAddressSumY(int at, int reg, bool direct, int& apos, const NativeCodeInstruction*& ains, const NativeCodeInstruction*& iins, uint32& flags, int& addr)
{
	flags = 0;

	int j = at - 7;
	while (j >= 0)
	{
		if (mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 1].mFlags & NCIF_LOWER) && mIns[j + 1].mLinkerObject &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 4].mFlags & NCIF_UPPER) && mIns[j + 4].mLinkerObject == mIns[j + 1].mLinkerObject &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 1]);
			iins = &(mIns[j + 2]);
			apos = j + 0;

			int ireg = iins->mAddress;
			if (reg == ireg)
			{
				if (!direct)
					return false;

				flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].ChangesYReg())
						flags &= ~LIVE_CPU_REG_Y;
					if (mIns[k].ChangesXReg())
						flags &= ~LIVE_CPU_REG_X;
					k++;
				}

				return flags != 0;
			}
			else
			{
				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}

				return true;
			}
		}
		else if (mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 2].mFlags & NCIF_LOWER) && mIns[j + 2].mLinkerObject &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 4].mFlags & NCIF_UPPER) && mIns[j + 4].mLinkerObject == mIns[j + 2].mLinkerObject &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 2]);
			iins = &(mIns[j + 1]);
			apos = j + 0;

			int ireg = iins->mAddress;
			if (reg == ireg)
			{
				if (!direct)
					return false;

				flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].ChangesYReg())
						flags &= ~LIVE_CPU_REG_Y;
					if (mIns[k].ChangesXReg())
						flags &= ~LIVE_CPU_REG_X;
					k++;
				}

				return flags != 0;
			}
			else
			{
				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}

				return true;
			}
		}
		else if (
			mIns[j + 0].mType == ASMIT_STA && mIns[j + 0].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 1].mType == ASMIT_CLC &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 2].mFlags & NCIF_LOWER) && mIns[j + 2].mLinkerObject &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 4].mFlags & NCIF_UPPER) && mIns[j + 4].mLinkerObject == mIns[j + 2].mLinkerObject &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 2]);
			iins = &(mIns[j + 0]);
			apos = j + 1;

			int ireg = iins->mAddress;
			if (reg == ireg)
			{
				if (!direct)
					return false;

				flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].ChangesYReg())
						flags &= ~LIVE_CPU_REG_Y;
					if (mIns[k].ChangesXReg())
						flags &= ~LIVE_CPU_REG_X;
					k++;
				}

				return flags != 0;
			}
			else
			{
				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}

				return true;
			}
		}
		else if (mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 1].mFlags & NCIF_LOWER) && mIns[j + 1].mLinkerObject &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ABSOLUTE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 4].mFlags & NCIF_UPPER) && mIns[j + 4].mLinkerObject == mIns[j + 1].mLinkerObject &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 1]);
			iins = &(mIns[j + 2]);
			apos = j + 0;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].ChangesGlobalMemory())
					return false;
				k++;
			}

			return true;
		}
#if 1
		else if (
			mIns[j + 1].mType == ASMIT_CLC &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 2].mFlags & NCIF_LOWER) && mIns[j + 2].mLinkerObject &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[j + 4].mFlags & NCIF_UPPER) && mIns[j + 4].mLinkerObject == mIns[j + 2].mLinkerObject &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 2]);
			iins = nullptr;
			apos = j + 1;

			if (!direct)
				return false;

			flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

			if (mIns[j + 0].mType == ASMIT_TAX)
				flags |= LIVE_CPU_REG_X;
			if (mIns[j + 0].mType == ASMIT_TAY)
				flags |= LIVE_CPU_REG_Y;

			int	k = j + 7;
			while (k < at)
			{
				assert(!(flags & LIVE_CPU_REG_Y) || mIns[k].mType != ASMIT_TYA);
				if (mIns[k].ChangesYReg())
					flags &= ~LIVE_CPU_REG_Y;
				if (mIns[k].ChangesXReg())
					flags &= ~LIVE_CPU_REG_X;
				k++;
			}

			return flags != 0;
		}
		else if (mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_IMMEDIATE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE && 
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 2]);
			iins = &(mIns[j + 1]);
			apos = j + 0;

			addr = mIns[j + 2].mAddress + 256 * (mIns[j + 4].mAddress + mIns[j + 5].mAddress);

			int ireg = iins->mAddress;
			if (reg == ireg)
			{
				if (!direct)
					return false;

				flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].ChangesYReg())
						flags &= ~LIVE_CPU_REG_Y;
					if (mIns[k].ChangesXReg())
						flags &= ~LIVE_CPU_REG_X;
					k++;
				}

				return flags != 0;
			}
			else
			{
				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}

				return true;
			}
		}
		else if (mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_IMMEDIATE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 1]);
			iins = &(mIns[j + 2]);
			apos = j + 0;

			addr = mIns[j + 1].mAddress + 256 * (mIns[j + 4].mAddress + mIns[j + 5].mAddress);

			int ireg = iins->mAddress;
			if (reg == ireg)
			{
				if (!direct)
					return false;

				flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].ChangesYReg())
						flags &= ~LIVE_CPU_REG_Y;
					if (mIns[k].ChangesXReg())
						flags &= ~LIVE_CPU_REG_X;
					k++;
				}

				return flags != 0;
			}
			else
			{
				int	k = j + 7;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}

				return true;
			}
		}
		else if (
			mIns[j + 1].mType == ASMIT_CLC &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_IMMEDIATE && 
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_IMMEDIATE && 
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && 
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			ains = &(mIns[j + 2]);
			iins = nullptr;
			apos = j + 1;

			addr = mIns[j + 2].mAddress + 256 * (mIns[j + 4].mAddress + mIns[j + 5].mAddress);

			if (!direct)
				return false;

			flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[j + 1].mLive;

			if (mIns[j + 0].mType == ASMIT_TAX)
				flags |= LIVE_CPU_REG_X;
			if (mIns[j + 0].mType == ASMIT_TAY)
				flags |= LIVE_CPU_REG_Y;

			int	k = j + 7;
			while (k < at)
			{
				assert(!(flags & LIVE_CPU_REG_Y) || mIns[k].mType != ASMIT_TYA);
				if (mIns[k].ChangesYReg())
					flags &= ~LIVE_CPU_REG_Y;
				if (mIns[k].ChangesXReg())
					flags &= ~LIVE_CPU_REG_X;
				k++;
			}

			return flags != 0;
		}
#endif

		if (mIns[j + 6].mMode == ASMIM_ZERO_PAGE && (mIns[j + 6].mAddress == reg || mIns[j + 6].mAddress == reg + 1) && mIns[j + 6].ChangesAddress())
			return false;

		j--;
	}

	if (at >= 6 &&
		mIns[0].mType == ASMIT_CLC &&
		mIns[1].mType == ASMIT_ADC && mIns[1].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[1].mFlags & NCIF_LOWER) && mIns[1].mLinkerObject &&
		mIns[2].mType == ASMIT_STA && mIns[2].mMode == ASMIM_ZERO_PAGE && mIns[2].mAddress == reg &&
		mIns[3].mType == ASMIT_LDA && mIns[3].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[3].mFlags & NCIF_UPPER) && mIns[3].mLinkerObject == mIns[1].mLinkerObject &&
		mIns[4].mType == ASMIT_ADC && mIns[4].mMode == ASMIM_IMMEDIATE && mIns[4].mAddress == 0 &&
		mIns[5].mType == ASMIT_STA && mIns[5].mMode == ASMIM_ZERO_PAGE && mIns[5].mAddress == reg + 1)
	{
		ains = &(mIns[1]);
		iins = nullptr;
		apos = 0;

		if (!direct)
			return false;

		flags = (LIVE_CPU_REG_X | LIVE_CPU_REG_Y) & ~mIns[0].mLive;

		int	k = 6;
		while (k < at)
		{
			assert(!(flags & LIVE_CPU_REG_Y) || mIns[k].mType != ASMIT_TYA);
			if (mIns[k].ChangesYReg())
				flags &= ~LIVE_CPU_REG_Y;
			if (mIns[k].ChangesXReg())
				flags &= ~LIVE_CPU_REG_X;
			k++;
		}

		return flags != 0;
	}

	if (mFromJump)
	{
		while (j >= -6)
		{
			if (mIns[j + 6].mMode == ASMIM_ZERO_PAGE && (mIns[j + 6].mAddress == reg || mIns[j + 6].mAddress == reg + 1) && mIns[j + 6].ChangesAddress())
				return false;
			j--;
		}
		if (mFromJump->FindGlobalAddressSumY(mFromJump->mIns.Size(), reg, false, apos, ains, iins, flags, addr))
		{
			if (iins->mMode == ASMIM_ZERO_PAGE)
			{
				int ireg = iins->mAddress;
				int	k = 0;
				while (k < at)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg && mIns[k].ChangesAddress())
						return false;
					k++;
				}
			}
			else
			{
				int	k = 0;
				while (k < at)
				{
					if (mIns[k].ChangesGlobalMemory())
						return false;
					k++;
				}
			}

			return true;
		}
	}

	return false;
}

bool NativeCodeBasicBlock::JoinTAXARange(int from, int to)
{
	int	start = from;
	if (from >= 2)
	{
		start = from - 2;
		if (mIns[start].mType == ASMIT_LDA && mIns[start].mMode == ASMIM_ZERO_PAGE && mIns[start + 1].mType == ASMIT_AND && mIns[start + 1].mMode == ASMIM_IMMEDIATE)
		{

			for (int i = from + 1; i < to; i++)
			{
				if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[start].mAddress && mIns[i].ChangesAddress())
					return false;
			}
		}
		else
			return false;
	}
	else
		return false;

	mIns.Remove(to);
	for (int i = start; i < from; i++)
	{
		mIns.Insert(to, mIns[start]);
		mIns.Remove(start);
	}
	mIns.Remove(start);

	return true;
}

bool NativeCodeBasicBlock::JoinTAYARange(int from, int to)
{
	return false;
}

bool NativeCodeBasicBlock::PatchGlobalAdressSumYByX(int at, int reg, const NativeCodeInstruction& ains, int addr)
{
	int	yindex = 0;
	int last = at;

	while (last < mIns.Size())
	{
		if (mIns[last].mType == ASMIT_LDY && mIns[last].mMode == ASMIM_IMMEDIATE)
			yindex = mIns[last].mAddress;
		else if (mIns[last].ChangesYReg())
			return false;
		else if (mIns[last].mMode == ASMIM_ZERO_PAGE && (mIns[last].mAddress == reg || mIns[last].mAddress == reg + 1) && mIns[last].ChangesAddress())
			return false;
		else if (mIns[last].mMode == ASMIM_INDIRECT_Y && mIns[last].mAddress == reg)
		{
			if (!(mIns[last].mLive & LIVE_MEM))
				break;
		}
		else if (mIns[last].RequiresXReg())
			return false;
		last++;
	}

	if (last == mIns.Size())
		return false;

	yindex = 0;
	for (int i = at; i <= last; i++)
	{
		mIns[i].mLive |= LIVE_CPU_REG_X;

		if (mIns[i].mType == ASMIT_LDY && mIns[i].mMode == ASMIM_IMMEDIATE)
			yindex = mIns[i].mAddress;
		else if (mIns[i].mMode == ASMIM_INDIRECT_Y && mIns[i].mAddress == reg)
		{
			mIns[i].mMode = ASMIM_ABSOLUTE_X;
			if (ains.mMode == ASMIM_IMMEDIATE)
			{
				mIns[i].mLinkerObject = nullptr;
				mIns[i].mAddress = addr + yindex;
			}
			else
			{
				mIns[i].mLinkerObject = ains.mLinkerObject;
				mIns[i].mAddress = ains.mAddress + yindex;
			}
		}
	}

	return true;
}


bool NativeCodeBasicBlock::PatchDirectAddressSumY(int at, int reg, int apos, int breg)
{
	int	yindex = 0;
	int last = at;

	while (last < mIns.Size())
	{
		if (mIns[last].mType == ASMIT_LDY && mIns[last].mMode == ASMIM_IMMEDIATE && (mIns[last].mAddress == yindex || mIns[last].mAddress == yindex + 1))
			yindex = mIns[last].mAddress;
		else if (mIns[last].ChangesYReg())
			return false;
		else if (mIns[last].mMode == ASMIM_ZERO_PAGE && (mIns[last].mAddress == breg || mIns[last].mAddress == breg + 1 || mIns[last].mAddress == reg || mIns[last].mAddress == reg + 1) && mIns[last].ChangesAddress())
			return false;
		else if (mIns[last].mMode == ASMIM_INDIRECT_Y && mIns[last].mAddress == reg)
		{
			if (!(mIns[last].mLive & LIVE_MEM))
				break;
		}
		else if (mIns[last].RequiresYReg())
			return false;
		last++;
	}

	if (last == mIns.Size())
		return false;

	if (mIns[last].mLive & LIVE_CPU_REG_Y)
	{
		mIns.Insert(last + 1, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, yindex));
		mIns[last + 1].mLive |= CPU_REG_Y;
	}

	mIns[apos].mType = ASMIT_TAY;
	for (int i = 0; i < 5; i++)
	{
		mIns[apos + i + 1].mType = ASMIT_NOP;
		mIns[apos + i + 1].mMode = ASMIM_IMPLIED;
	}
	yindex = 0;

	for (int i = apos; i <= last; i++)
	{
		mIns[i].mLive |= LIVE_CPU_REG_Y;

		if (mIns[i].mType == ASMIT_LDY && mIns[i].mMode == ASMIM_IMMEDIATE)
		{
			if (mIns[i].mAddress == yindex)
			{
				mIns[i].mType = ASMIT_NOP;
				mIns[i].mMode = ASMIM_IMPLIED;
			}
			else
			{
				mIns[i].mType = ASMIT_INY;
				mIns[i].mMode = ASMIM_IMPLIED;
				yindex++;
			}
		}
		else if (mIns[i].mMode == ASMIM_INDIRECT_Y && mIns[i].mAddress == reg)
		{
			mIns[i].mAddress = breg;
		}
	}

	return true;
}

bool NativeCodeBasicBlock::PatchAddressSumY(int at, int reg, int apos, int breg, int ireg)
{
	int	yindex = 0;
	int last = apos + 7;

	while (last < mIns.Size())
	{
		if (mIns[last].mType == ASMIT_LDY && mIns[last].mMode == ASMIM_IMMEDIATE && (mIns[last].mAddress == yindex || mIns[last].mAddress == yindex + 1 || mIns[last].mAddress + 1 == yindex))
			yindex = mIns[last].mAddress;
		else if (mIns[last].ChangesYReg())
			return false;
		else if (mIns[last].mMode == ASMIM_ZERO_PAGE && (mIns[last].mAddress == breg || mIns[last].mAddress == breg + 1 || mIns[last].mAddress == reg || mIns[last].mAddress == reg + 1) && mIns[last].ChangesAddress())
			return false;
		else if (mIns[last].mMode == ASMIM_INDIRECT_Y && mIns[last].mAddress == reg)
		{
			if (!(mIns[last].mLive & LIVE_MEM))
				break;
		}
		else if (mIns[last].RequiresYReg())
			return false;
		last++;
	}

	if (last == mIns.Size())
		return false;

	if (mIns[last].mLive & LIVE_CPU_REG_Y)
	{
		mIns.Insert(last + 1, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, yindex));
		mIns[last + 1].mLive |= CPU_REG_Y;
	}

	for (int i = 0; i < 5; i++)
	{
		mIns[apos + i + 2].mType = ASMIT_NOP;
		mIns[apos + i + 2].mMode = ASMIM_IMPLIED;
	}
	mIns[apos + 1].mType = ASMIT_LDY;
	mIns[apos + 1].mMode = ASMIM_ZERO_PAGE;
	mIns[apos + 1].mAddress = ireg;
	mIns[apos + 1].mLive |= LIVE_MEM | LIVE_CPU_REG_Y;

	yindex = 0;

	for (int i = apos + 7; i <= last; i++)
	{
		mIns[i].mLive |= LIVE_CPU_REG_Y;

		if (mIns[i].mType == ASMIT_LDY && mIns[i].mMode == ASMIM_IMMEDIATE)
		{
			if (mIns[i].mAddress == yindex)
			{
				mIns[i].mType = ASMIT_NOP;
				mIns[i].mMode = ASMIM_IMPLIED;
			}
			else if (mIns[i].mAddress == yindex + 1)
			{
				mIns[i].mType = ASMIT_INY;
				mIns[i].mMode = ASMIM_IMPLIED;
				yindex++;
			}
			else
			{
				mIns[i].mType = ASMIT_DEY;
				mIns[i].mMode = ASMIM_IMPLIED;
				yindex--;
			}
		}
		else if (mIns[i].mMode == ASMIM_INDIRECT_Y && mIns[i].mAddress == reg)
		{
			mIns[i].mFlags &= ~NCIF_YZERO;
			mIns[i].mAddress = breg;
		}
	}

	return true;
}

bool NativeCodeBasicBlock::FindDirectAddressSumY(int at, int reg, int& apos, int& breg)
{
	int j = at - 6;
	while (j >= 0)
	{
		if (
			mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_ADC && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_STA && mIns[j + 2].mMode == ASMIM_ZERO_PAGE && mIns[j + 2].mAddress == reg &&
			mIns[j + 3].mType == ASMIT_LDA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == mIns[j + 1].mAddress + 1 &&
			mIns[j + 4].mType == ASMIT_ADC && mIns[j + 4].mMode == ASMIM_IMMEDIATE && mIns[j + 4].mAddress == 0 &&
			mIns[j + 5].mType == ASMIT_STA && mIns[j + 5].mMode == ASMIM_ZERO_PAGE && mIns[j + 5].mAddress == reg + 1)
		{
			breg = mIns[j + 1].mAddress;
			int	k = j + 6;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1) && mIns[k].ChangesAddress())
					return false;

				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1))
					return false;
				if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == breg)
					return false;

				k++;
			}

			apos = j;

			return true;
		}

		if (mIns[j + 5].mMode == ASMIM_ZERO_PAGE && (mIns[j + 5].mAddress == reg || mIns[j + 5].mAddress == reg + 1) && mIns[j + 5].ChangesAddress() || mIns[j + 5].ChangesYReg() || mIns[j + 5].RequiresYReg())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::FindExternAddressSumY(int at, int reg, int& breg, int& ireg)
{
	int j = at - 7;
	while (j >= 0)
	{
		if (
			mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 1].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 1].mAddress;
			ireg = mIns[j + 2].mAddress;

			if (ireg == breg || reg == breg || ireg == reg)
				return false;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				k++;
			}

			return true;
		}
		else if (
			mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 2].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 2].mAddress;
			ireg = mIns[j + 1].mAddress;

			if (ireg == breg || reg == breg || ireg == reg)
				return false;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				k++;
			}

			return true;
		}
		else if (
			mIns[j + 0].mType == ASMIT_STA && mIns[j + 0].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 1].mType == ASMIT_CLC &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 2].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 2].mAddress;
			ireg = mIns[j + 0].mAddress;

			if (ireg == breg || reg == breg || ireg == reg)
				return false;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				k++;
			}

			return true;
		}

		if (mIns[j + 6].mMode == ASMIM_ZERO_PAGE && (mIns[j + 6].mAddress == reg || mIns[j + 6].mAddress == reg + 1) && mIns[j + 6].ChangesAddress())
			return false;

		j--;
	}

	if (mFromJump)
	{
		while (j >= -6)
		{
			if (mIns[j + 6].mMode == ASMIM_ZERO_PAGE && (mIns[j + 6].mAddress == reg || mIns[j + 6].mAddress == reg + 1) && mIns[j + 6].ChangesAddress())
				return false;
			j--;
		}

		if (mFromJump->FindExternAddressSumY(mFromJump->mIns.Size(), reg, breg, ireg))
		{
			int	k = 0;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				k++;
			}

			return true;
		}
	}


	return false;

}

bool NativeCodeBasicBlock::FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg)
{
	int j = at - 7;
	while (j >= 0)
	{
		if (
#if 1
			mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 1].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 1].mAddress;
			ireg = mIns[j + 2].mAddress;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				if (breg == reg || ireg == breg)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1))
						return false;
					if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == breg)
						return false;
				}
				else if (ireg == reg)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg)
						return false;
					if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == ireg)
						return false;
				}
				k++;
			}

			apos = j;

			return true;
		}
		else if (
#endif
			mIns[j + 0].mType == ASMIT_CLC &&
			mIns[j + 1].mType == ASMIT_LDA && mIns[j + 1].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 2].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 2].mAddress;
			ireg = mIns[j + 1].mAddress;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				if (breg == reg || ireg == breg)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1))
						return false;
					if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == breg)
						return false;
				}
				else if (ireg == reg)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && mIns[k].mAddress == ireg)
						return false;
					if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == ireg)
						return false;
				}
				k++;
			}

			apos = j;

			return true;
		}
		else if (
			mIns[j + 0].mType == ASMIT_STA && mIns[j + 0].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 1].mType == ASMIT_CLC &&
			mIns[j + 2].mType == ASMIT_ADC && mIns[j + 2].mMode == ASMIM_ZERO_PAGE &&
			mIns[j + 3].mType == ASMIT_STA && mIns[j + 3].mMode == ASMIM_ZERO_PAGE && mIns[j + 3].mAddress == reg &&
			mIns[j + 4].mType == ASMIT_LDA && mIns[j + 4].mMode == ASMIM_ZERO_PAGE && mIns[j + 4].mAddress == mIns[j + 2].mAddress + 1 &&
			mIns[j + 5].mType == ASMIT_ADC && mIns[j + 5].mMode == ASMIM_IMMEDIATE && mIns[j + 5].mAddress == 0 &&
			mIns[j + 6].mType == ASMIT_STA && mIns[j + 6].mMode == ASMIM_ZERO_PAGE && mIns[j + 6].mAddress == reg + 1)
		{
			breg = mIns[j + 2].mAddress;
			ireg = mIns[j + 0].mAddress;

			int	k = j + 7;
			while (k < at)
			{
				if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1 || mIns[k].mAddress == ireg) && mIns[k].ChangesAddress())
					return false;
				if (breg == reg || ireg == breg)
				{
					if (mIns[k].mMode == ASMIM_ZERO_PAGE && (mIns[k].mAddress == breg || mIns[k].mAddress == breg + 1))
						return false;
					if (mIns[k].mMode == ASMIM_INDIRECT_Y && mIns[k].mAddress == breg)
						return false;
				}
				k++;
			}

			apos = j;

			return true;
		}

		if (mIns[j + 6].mMode == ASMIM_ZERO_PAGE && (mIns[j + 6].mAddress == reg || mIns[j + 6].mAddress == reg + 1) && mIns[j + 6].ChangesAddress())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveIndirectLoadStoreDown(int at)
{
	int j = at + 2;

	while (j < mIns.Size())
	{
		if (mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at + 1].mAddress)
		{
			if (!(mIns[j].mLive & LIVE_MEM) && HasAsmInstructionMode(mIns[j].mType, ASMIM_INDIRECT_Y))
			{
				mIns[j].mMode = ASMIM_INDIRECT_Y;
				mIns[j].mAddress = mIns[at].mAddress;
				mIns[j].mLive |= LIVE_MEM;
				mIns[at + 0].mType = ASMIT_NOP; mIns[at + 0].mMode = ASMIM_IMPLIED;
				mIns[at + 1].mType = ASMIT_NOP; mIns[at + 1].mMode = ASMIM_IMPLIED;

				for (int k = at; k < j; k++)
					mIns[k].mLive |= LIVE_CPU_REG_Y;

				return true;
			}

			return false;
		}

		if (mIns[j].ChangesYReg())
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at].mAddress) || mIns[j].ChangesZeroPage(mIns[at].mAddress + 1))
			return false;
		if (mIns[j].ChangesGlobalMemory())
			return false;

		j++;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveIndirectLoadStoreUp(int at)
{
	int	j = at - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at].mAddress)
		{
			mIns.Insert(j + 1, mIns[at + 1]);
			mIns.Insert(j + 2, mIns[at + 3]);
			mIns[at + 4].mType = ASMIT_NOP;
			mIns[at + 4].mMode = ASMIM_IMPLIED;
			return true;
		}

		if (mIns[j].mLive & LIVE_CPU_REG_Y)
			return false;
		if (mIns[j].ChangesYReg())
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at].mAddress))
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at + 2].mAddress))
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at + 2].mAddress + 1))
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadStoreOutOfXYRangeUp(int at)
{
	int	j = at - 1;
	while (j >= 0)
	{
		if (mIns[j].MayBeChangedOnAddress(mIns[at + 2]))
			return false;
		if (mIns[j].ChangesAddress() && mIns[j].SameEffectiveAddress(mIns[at + 1]))
			return false;

		if (mIns[j].mType == ASMIT_LDA)
		{
			if (j > 0 && (mIns[j - 1].mType == ASMIT_CLC || mIns[j - 1].mType == ASMIT_SEC))
				j--;
			mIns.Insert(j, mIns[at + 2]);
			mIns.Insert(j, mIns[at + 2]);
			if (j > 0)
			{
				mIns[j].mLive |= mIns[j - 1].mLive;
				mIns[j + 1].mLive |= mIns[j - 1].mLive;
			}
			mIns.Remove(at + 3);
			mIns.Remove(at + 3);

			return true;
		}

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveAbsoluteLoadStoreUp(int at)
{
	int	j = at - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at].mAddress)
		{
			mIns[j].mLive |= LIVE_CPU_REG_A;
			mIns.Insert(j + 1, mIns[at + 1]);
			mIns[j + 1].mLive |= LIVE_CPU_REG_A;
			mIns[at + 2].mType = ASMIT_NOP;
			mIns[at + 2].mMode = ASMIM_IMPLIED;
			return true;
		}
		if (mIns[j].ChangesZeroPage(mIns[at].mAddress))
			return false;
		if (mIns[j].ChangesGlobalMemory())
			return false;
		if (mIns[j].SameEffectiveAddress(mIns[at + 1]))
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::ReplaceZeroPageUp(int at)
{
	int i = at - 1;
	while (i >= 0)
	{
		if ((mIns[i].mType == ASMIT_STA || mIns[i].mType == ASMIT_STX || mIns[i].mType == ASMIT_STY) && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[at].mAddress)
		{
			while (i < at)
			{
				if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[at].mAddress)
					mIns[i].mAddress = mIns[at + 1].mAddress;
				i++;
			}

			mIns[at + 0].mType = ASMIT_NOP; mIns[at + 0].mMode = ASMIM_IMPLIED;
			mIns[at + 1].mType = ASMIT_NOP; mIns[at + 1].mMode = ASMIM_IMPLIED;

			return true;
		}

		if (mIns[i].mType == ASMIT_JSR)
			return false;

		if (mIns[i].ChangesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[i].UsesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[i].mMode == ASMIM_INDIRECT_Y && (mIns[i].mAddress == mIns[at + 1].mAddress || mIns[i].mAddress + 1 == mIns[at + 1].mAddress))
			return false;

		i--;
	}

	return false;
}


bool NativeCodeBasicBlock::MoveLoadXUp(int at)
{
	int	i = at - 1;
	while (i >= 0)
	{
		if (mIns[i].mType == ASMIT_STA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[at].mAddress)
		{
			mIns[i].mType = ASMIT_TAX;
			mIns[i].mMode = ASMIM_IMPLIED;
			mIns[at].mType = ASMIT_NOP;
			mIns[at].mMode = ASMIM_IMPLIED;
			while (i < at)
			{
				mIns[i].mLive |= LIVE_CPU_REG_X;
				i++;
			}

			return true;
		}

		if (mIns[i].ChangesXReg() || (mIns[i].mLive & LIVE_CPU_REG_X) || mIns[i].UsesZeroPage(mIns[at].mAddress))
			return false;

		i--;
	}

	return false;
}


bool NativeCodeBasicBlock::MoveStoreXUp(int at)
{
	bool	done = false;

	while (at > 0)
	{
		if (mIns[at - 1].ChangesXReg() || mIns[at - 1].mType == ASMIT_STX)
			return done;
		if (mIns[at].mMode == ASMIM_ZERO_PAGE)
		{
			if ((mIns[at - 1].mMode == ASMIM_ZERO_PAGE || mIns[at - 1].mMode == ASMIM_INDIRECT_Y) && mIns[at - 1].mAddress == mIns[at].mAddress)
				return done;
			if (mIns[at - 1].mMode == ASMIM_INDIRECT_Y && mIns[at - 1].mAddress == mIns[at].mAddress + 1)
				return done;
		}
		else
		{
			if (mIns[at - 1].mMode == ASMIM_ABSOLUTE && mIns[at - 1].mLinkerObject == mIns[at].mLinkerObject && mIns[at - 1].mAddress == mIns[at].mAddress)
				return done;
			else if ((mIns[at - 1].mMode == ASMIM_ABSOLUTE_X || mIns[at - 1].mMode == ASMIM_ABSOLUTE_Y) && mIns[at - 1].mLinkerObject == mIns[at].mLinkerObject)
				return done;
		}

		mIns[at].mLive |= mIns[at - 1].mLive;

		NativeCodeInstruction	ins = mIns[at - 1];
		mIns[at - 1] = mIns[at];
		mIns[at] = ins; 
		at--;
		done = true;
	}

	return done;
}

bool NativeCodeBasicBlock::MoveStoreHighByteDown(int at)
{
	int	i = at + 4;
	while (i + 1 < mIns.Size())
	{
		if (mIns[i].mLive & LIVE_CPU_REG_Y)
			return false;
		if (mIns[i].ChangesZeroPage(mIns[at + 2].mAddress) || mIns[i].ChangesZeroPage(mIns[at + 2].mAddress + 1) || mIns[i].ChangesZeroPage(mIns[at + 3].mAddress))
			return false;
		if (mIns[i].UsesZeroPage(mIns[at + 3].mAddress))
			return false;
		if (mIns[i].ChangesGlobalMemory())
			return false;

		if (!(mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
		{
			mIns.Insert(i + 1, mIns[at + 3]);	
			mIns.Insert(i + 1, mIns[at + 2]);
			mIns.Insert(i + 1, mIns[at + 1]);

			mIns[at + 1].mType = ASMIT_NOP; mIns[at + 1].mMode = ASMIM_IMPLIED; // LDY
			mIns[at + 2].mType = ASMIT_NOP; mIns[at + 2].mMode = ASMIM_IMPLIED; // LDA (x), y
			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED; // STA T

			return true;
		}

		i++;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveAddHighByteDown(int at)
{
	int	i = at + 4;
	while (i + 1 < mIns.Size())
	{
		if (mIns[i].mLive & LIVE_CPU_REG_C)
			return false;
		if (mIns[i].ChangesZeroPage(mIns[at + 1].mAddress) || mIns[i].ChangesZeroPage(mIns[at + 3].mAddress))
			return false;
		if (mIns[i].UsesZeroPage(mIns[at + 3].mAddress))
			return false;

		if (!(mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
		{
			mIns.Insert(i + 1, mIns[at + 3]);
			mIns.Insert(i + 1, mIns[at + 2]);
			mIns.Insert(i + 1, mIns[at + 1]);

			mIns[at + 1].mType = ASMIT_NOP; mIns[at + 1].mMode = ASMIM_IMPLIED; // LDA U
			mIns[at + 2].mType = ASMIT_NOP; mIns[at + 2].mMode = ASMIM_IMPLIED; // ADC #
			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED; // STA T

			return true;
		}

		i++;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadImmStoreAbsoluteUp(int at)
{
	int	j = at;
	while (j > 0)
	{
		if (mIns[j - 1].mType == ASMIT_LDA && (mIns[j - 1].mMode == ASMIM_IMMEDIATE || mIns[j - 1].mMode == ASMIM_ZERO_PAGE))
		{
			if (mIns[j - 1].mMode == mIns[at + 0].mMode && mIns[j - 1].mAddress == mIns[at + 0].mAddress)
			{
				while (j < at && mIns[j].mType == ASMIT_STA)
					j++;

				NativeCodeInstruction	sins = mIns[at + 1];
				mIns.Remove(at + 1);
				if (!(sins.mLive & LIVE_CPU_REG_A))
					mIns.Remove(at);

				mIns.Insert(j, sins);
				return true;
			}
			j--;
		}
		else if (mIns[j - 1].mType == ASMIT_STA && mIns[j - 1].mMode == mIns[at + 1].mMode && mIns[j - 1].mLinkerObject == mIns[at + 1].mLinkerObject && mIns[j - 1].mAddress != mIns[at + 1].mAddress)
			j--;
		else
			return false;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadStoreUp(int at)
{
	int	j = at;
	while (j > 0 && !((mIns[j - 1].mType == ASMIT_STA || mIns[j - 1].mType == ASMIT_LDA) && mIns[j - 1].mMode == ASMIM_ZERO_PAGE && mIns[j - 1].mAddress == mIns[at].mAddress))
	{
		j--;
		if ((mIns[j].mMode == ASMIM_ZERO_PAGE || mIns[j].mMode == ASMIM_INDIRECT_Y) && mIns[j].mAddress == mIns[at + 1].mAddress)
			return false;
		if (mIns[j].mMode == ASMIM_INDIRECT_Y && mIns[j].mAddress + 1 == mIns[at + 1].mAddress)
			return false;
		if (mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at].mAddress && mIns[j].ChangesAddress())
			return false;
		if (mIns[j].mMode == ASMIM_ABSOLUTE_Y && mIns[j].mAddress <= mIns[at + 1].mAddress && mIns[j].mType == ASMIT_LDA && !mIns[j].mLinkerObject)
			return false;
		if (mIns[j].mType == ASMIT_JSR)
			return false;
	}

	if (j > 0 && j < at)
	{
		for (int i = at; i > j; i--)
		{
			mIns[i] = mIns[i - 1];
		}
		mIns[j - 1].mLive |= LIVE_CPU_REG_A;
		mIns[j] = mIns[at + 1];
		mIns[at + 1].mType = ASMIT_NOP;	mIns[at + 1].mMode = ASMIM_IMPLIED;

		return true;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadStoreXUp(int at)
{
	int	j = at;
	while (j > 0 && !((mIns[j - 1].mType == ASMIT_STA || mIns[j - 1].mType == ASMIT_LDA) && mIns[j - 1].mMode == ASMIM_ZERO_PAGE && mIns[j - 1].mAddress == mIns[at].mAddress))
	{
		j--;
		if ((mIns[j].mMode == ASMIM_ZERO_PAGE || mIns[j].mMode == ASMIM_INDIRECT_Y) && mIns[j].mAddress == mIns[at + 1].mAddress)
			return false;
		if (mIns[j].mMode == ASMIM_INDIRECT_Y && mIns[j].mAddress + 1 == mIns[at + 1].mAddress)
			return false;
		if (mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at].mAddress && mIns[j].ChangesAddress())
			return false;
		if (mIns[j].mMode == ASMIM_ABSOLUTE_Y && mIns[j].mAddress <= mIns[at + 1].mAddress && mIns[j].mType == ASMIT_LDA && !mIns[j].mLinkerObject)
			return false;
		if (mIns[j].mType == ASMIT_JSR)
			return false;
	}

	if (j > 0 && j < at)
	{
		for (int i = at; i > j; i--)
		{
			mIns[i] = mIns[i - 1];
		}
		mIns[j - 1].mLive |= LIVE_CPU_REG_A;
		mIns[j] = mIns[at + 1];
		mIns[j].mType = ASMIT_STA;
		mIns[at + 1].mType = ASMIT_NOP;	mIns[at + 1].mMode = ASMIM_IMPLIED;

		return true;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadAddImmStoreUp(int at)
{
	int	j = at - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at + 1].mAddress)
		{
			if (mIns[j].mLive & LIVE_CPU_REG_A)
				return false;

			mIns.Insert(j + 1, mIns[at + 3]);	// STA
			mIns.Insert(j + 1, mIns[at + 3]);	// ADC
			mIns.Insert(j + 1, mIns[at + 2]);	// CLC

			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED;
			mIns[at + 4].mType = ASMIT_NOP; mIns[at + 4].mMode = ASMIM_IMPLIED;
			mIns[at + 5].mType = ASMIT_NOP; mIns[at + 5].mMode = ASMIM_IMPLIED;
			mIns[at + 6].mType = ASMIT_NOP; mIns[at + 6].mMode = ASMIM_IMPLIED;
			return true;
		}

		if (mIns[j].ChangesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[j].UsesZeroPage(mIns[at + 3].mAddress))
			return false;
		if (mIns[j].ChangesCarry())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveCLCLoadAddZPStoreUp(int at)
{
	int	j = at - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at + 1].mAddress)
		{
			if (mIns[j].mLive & LIVE_CPU_REG_A)
				return false;

			mIns.Insert(j + 1, mIns[at + 3]);	// STA
			mIns.Insert(j + 1, mIns[at + 3]);	// ADC
			mIns.Insert(j + 1, mIns[at + 2]);	// CLC

			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED;
			mIns[at + 4].mType = ASMIT_NOP; mIns[at + 4].mMode = ASMIM_IMPLIED;
			mIns[at + 5].mType = ASMIT_NOP; mIns[at + 5].mMode = ASMIM_IMPLIED;
			mIns[at + 6].mType = ASMIT_NOP; mIns[at + 6].mMode = ASMIM_IMPLIED;
			return true;
		}

		if (mIns[j].ChangesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at + 2].mAddress))
			return false;
		if (mIns[j].UsesZeroPage(mIns[at + 3].mAddress))
			return false;
		if (mIns[j].ChangesCarry())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::ReverseLoadCommutativeOpUp(int aload, int aop)
{
	int	j = aload - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[aop].mAddress)
		{
			AsmInsType	type = mIns[aop].mType;
			mIns[aop] = mIns[aload];
			mIns[aop].mType = type;
			mIns[aload].mType = ASMIT_NOP;
			mIns[aload].mMode = ASMIM_IMPLIED;

			while (j < aop)
			{
				mIns[j].mLive |= LIVE_CPU_REG_A;
				j++;
			}

			j = aload;
			while (j < aop)
			{
				mIns[j].mLive |= mIns[aload - 1].mLive;
				j++;
			}

			return true;
		}

		if (mIns[j].ChangesAccu())
			return false;
		if (mIns[j].ChangesZeroPage(mIns[aop].mAddress))
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveLoadAddZPStoreUp(int at)
{
	int	j = at - 1;
	while (j > 0)
	{
		if (mIns[j].mType == ASMIT_STA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at + 0].mAddress)
		{
			mIns.Insert(j + 1, mIns[at + 2]);	// STA
			mIns.Insert(j + 1, mIns[at + 2]);	// ADC

			mIns[at + 2].mType = ASMIT_NOP; mIns[at + 2].mMode = ASMIM_IMPLIED;
			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED;
			mIns[at + 4].mType = ASMIT_NOP; mIns[at + 4].mMode = ASMIM_IMPLIED;
			return true;
		}

		if (mIns[j].ChangesZeroPage(mIns[at + 0].mAddress))
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[j].UsesZeroPage(mIns[at + 2].mAddress))
			return false;
		if (mIns[j].ChangesCarry())
			return false;

		j--;
	}

	return false;
}

bool NativeCodeBasicBlock::MoveCLCLoadAddZPStoreDown(int at)
{
	int	j = at + 4;
	while (j < mIns.Size())
	{
		if (mIns[j].mType == ASMIT_LDA && mIns[j].mMode == ASMIM_ZERO_PAGE && mIns[j].mAddress == mIns[at + 3].mAddress)
		{
			if (j == at + 4)
				return false;
			if (mIns[j].mLive & LIVE_CPU_REG_C)
				return false;

			mIns.Insert(j, mIns[at + 3]);	// STA
			mIns.Insert(j, mIns[at + 2]);	// ADC
			mIns.Insert(j, mIns[at + 1]);	// LDA
			mIns.Insert(j, mIns[at + 0]);	// CLC

			mIns[at + 0].mType = ASMIT_NOP; mIns[at + 0].mMode = ASMIM_IMPLIED;
			mIns[at + 1].mType = ASMIT_NOP; mIns[at + 1].mMode = ASMIM_IMPLIED;
			mIns[at + 2].mType = ASMIT_NOP; mIns[at + 2].mMode = ASMIM_IMPLIED;
			mIns[at + 3].mType = ASMIT_NOP; mIns[at + 3].mMode = ASMIM_IMPLIED;
			return true;
		}

		if (mIns[j].ChangesZeroPage(mIns[at + 1].mAddress))
			return false;
		if (mIns[j].ChangesZeroPage(mIns[at + 2].mAddress))
			return false;
		if (mIns[j].UsesZeroPage(mIns[at + 3].mAddress))
			return false;

		j++;
	}

	return false;
}

bool NativeCodeBasicBlock::ValueForwarding(const NativeRegisterDataSet& data, bool global)
{
	bool	changed = false;

	if (!mVisited)
	{
		mNDataSet = data;

		if (mLoopHead || !global)
		{
			mNDataSet.Reset();
		}
#if 0
		else if (mNumEntries != 1)
		{
			ndata.Reset();
		}
#endif
		else if (mNumEntries > 0)
		{
			if (mNumEntered > 0)
				mNDataSet.Intersect(mDataSet);

			mNumEntered++;

			if (mNumEntered < mNumEntries)
			{
				mDataSet = mNDataSet;
				return false;
			}
		}

		mVisited = true;

		for (int i = 0; i < mIns.Size(); i++)
		{
			AsmInsType	carryop;

			// Check load and commutative with current accu value
#if 1
			if (i + 1 < mIns.Size() && mIns[i].mType == ASMIT_LDA && mIns[i + 1].IsCommutative() && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mNDataSet.mRegs[CPU_REG_A].mMode == NRDM_ZERO_PAGE && mNDataSet.mRegs[CPU_REG_A].mValue == mIns[i + 1].mAddress)
			{
				mIns[i].mType = mIns[i + 1].mType;
				mIns[i + 1].mType = ASMIT_NOP;	mIns[i + 1].mMode = ASMIM_IMPLIED;
				changed = true;
			}
#endif
			if (mIns[i].ValueForwarding(mNDataSet, carryop))
				changed = true;
			if (carryop != ASMIT_NOP)
				mIns.Insert(i + 1, NativeCodeInstruction(carryop));
		}

#if 1
		NativeCodeBasicBlock* fork = this;
		if (!mFalseJump && mTrueJump && mTrueJump->mIns.Size() == 0)
			fork = mTrueJump;

		if (fork->mFalseJump)
		{
			switch (fork->mBranch)
			{
			case ASMIT_BCS:
				if (mNDataSet.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if (!mNDataSet.mRegs[CPU_REG_C].mValue)
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BCC:
				if (mNDataSet.mRegs[CPU_REG_C].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if (mNDataSet.mRegs[CPU_REG_C].mValue)
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BNE:
				if (mNDataSet.mRegs[CPU_REG_Z].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if (!mNDataSet.mRegs[CPU_REG_Z].mValue)
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BEQ:
				if (mNDataSet.mRegs[CPU_REG_Z].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if (mNDataSet.mRegs[CPU_REG_Z].mValue)
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BPL:
				if (mNDataSet.mRegs[CPU_REG_Z].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if ((mNDataSet.mRegs[CPU_REG_Z].mValue & 0x80))
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BMI:
				if (mNDataSet.mRegs[CPU_REG_Z].mMode == NRDM_IMMEDIATE)
				{
					mBranch = ASMIT_JMP;
					if (!(mNDataSet.mRegs[CPU_REG_Z].mValue & 0x80))
						mTrueJump = fork->mFalseJump;
					else
						mTrueJump = fork->mTrueJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			}
		}
#endif
		if (this->mTrueJump && this->mTrueJump->ValueForwarding(mNDataSet, global))
			changed = true;
		if (this->mFalseJump && this->mFalseJump->ValueForwarding(mNDataSet, global))
			changed = true;
	}

	return changed;
}

bool NativeCodeBasicBlock::OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc)
{
	NativeCodeBasicBlock* lblock = proc->AllocateBlock();
	NativeCodeBasicBlock* eblock = proc->AllocateBlock();

	eblock->mBranch = ASMIT_JMP;
	eblock->mTrueJump = mFalseJump;
	eblock->mFalseJump = nullptr;

	lblock->mBranch = mBranch;
	lblock->mTrueJump = lblock;
	lblock->mFalseJump = eblock;

	for (int i = 0; i < mIns.Size(); i++)
		lblock->mIns.Push(mIns[i]);

	mIns.SetSize(0);

	mBranch = ASMIT_JMP;
	mTrueJump = lblock;
	mFalseJump = nullptr;

	return lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);
}

bool NativeCodeBasicBlock::OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, NativeCodeBasicBlock* prevBlock, NativeCodeBasicBlock* exitBlock)
{
	bool changed = false;

	int	sz = mIns.Size();

	if (sz == 2 && (mBranch == ASMIT_BEQ || mBranch == ASMIT_BNE) && mIns[0].mType == ASMIT_LDA && mIns[1].mType == ASMIT_CMP && !(mIns[1].mFlags & NCIF_VOLATILE) && !(mIns[1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
	{
		if (!prevBlock)
			return OptimizeSimpleLoopInvariant(proc);
		mIns[1].mType = ASMIT_LDA; mIns[1].mLive |= LIVE_CPU_REG_A;
		mIns[0].mType = ASMIT_CMP; mIns[0].mLive |= LIVE_CPU_REG_Z;
		prevBlock->mIns.Push(mIns[1]);
		mIns.Remove(1);
		return true;
	}

	if (sz >= 3 && mIns[0].mType == ASMIT_LDA && mIns[sz - 2].mType == ASMIT_LDA && mIns[0].SameEffectiveAddress(mIns[sz - 2]) && mIns[sz - 1].mType == ASMIT_CMP)
	{
		if (!prevBlock)
			return OptimizeSimpleLoopInvariant(proc);

		prevBlock->mIns.Push(mIns[0]);
		mIns.Remove(0);
		return true;
	}

	if (sz >= 3 && mIns[0].mType == ASMIT_LDY && mIns[sz - 2].mType == ASMIT_LDA && mIns[0].SameEffectiveAddress(mIns[sz - 2]) &&
		mIns[sz - 1].mType == ASMIT_CMP && HasAsmInstructionMode(ASMIT_CPY, mIns[sz - 1].mMode) && !(mIns[sz - 1].mLive & LIVE_CPU_REG_A))
	{
		if (!prevBlock)
			return OptimizeSimpleLoopInvariant(proc);

		mIns[sz - 2].mType = ASMIT_LDY;
		mIns[sz - 1].mType = ASMIT_CPY;

		prevBlock->mIns.Push(mIns[0]);
		mIns.Remove(0);
		return true;
	}

	if (sz >= 3 && mIns[0].mType == ASMIT_LDX && mIns[sz - 2].mType == ASMIT_LDA && mIns[0].SameEffectiveAddress(mIns[sz - 2]) &&
		mIns[sz - 1].mType == ASMIT_CMP && HasAsmInstructionMode(ASMIT_CPX, mIns[sz - 1].mMode) && !(mIns[sz - 1].mLive & LIVE_CPU_REG_A))
	{
		if (!prevBlock)
			return OptimizeSimpleLoopInvariant(proc);

		mIns[sz - 2].mType = ASMIT_LDX;
		mIns[sz - 1].mType = ASMIT_CPX;

		prevBlock->mIns.Push(mIns[0]);
		mIns.Remove(0);
		return true;
	}

	if (sz >= 2 && mIns[0].mType == ASMIT_LDY && mIns[0].mMode == ASMIM_ZERO_PAGE)
	{
		int	i = mIns.Size() - 1;
		while (i > 0 && !mIns[i].ChangesYReg() && !mIns[i].ChangesZeroPage(mIns[0].mAddress))
			i--;

		if (i > 0 &&
			(mIns[i].mType == ASMIT_LDY && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[0].mAddress) ||
			(mIns[i].mType == ASMIT_TAY && (mIns[i - 1].mType == ASMIT_LDA || mIns[i - 1].mType == ASMIT_STA) && mIns[i - 1].mMode == ASMIM_ZERO_PAGE && mIns[i - 1].mAddress == mIns[0].mAddress))
		{
			if (!prevBlock)
				return OptimizeSimpleLoopInvariant(proc);
			while (i < mIns.Size())
			{
				mIns[i].mLive |= LIVE_CPU_REG_Y;
				i++;
			}

			prevBlock->mIns.Push(mIns[0]);
			mIns.Remove(0);
			return true;
		}
	}

	if (sz >= 2 && mIns[0].mType == ASMIT_LDX && mIns[0].mMode == ASMIM_ZERO_PAGE)
	{
		int	i = mIns.Size() - 1;
		while (i > 0 && !mIns[i].ChangesXReg() && !mIns[i].ChangesZeroPage(mIns[0].mAddress))
			i--;

		if (i > 0 &&
			(mIns[i].mType == ASMIT_LDX && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[0].mAddress) ||
			(mIns[i].mType == ASMIT_TAX && (mIns[i - 1].mType == ASMIT_LDA || mIns[i - 1].mType == ASMIT_STA) && mIns[i - 1].mMode == ASMIM_ZERO_PAGE && mIns[i - 1].mAddress == mIns[0].mAddress))
		{
			if (!prevBlock)
				return OptimizeSimpleLoopInvariant(proc);
			while (i < mIns.Size())
			{
				mIns[i].mLive |= LIVE_CPU_REG_X;
				i++;
			}

			prevBlock->mIns.Push(mIns[0]);
			mIns.Remove(0);
			return true;
		}
	}


	int	ai = 0;
	while (ai < mIns.Size() && !mIns[ai].ChangesAccu())
		ai++;

	if (ai < mIns.Size())
	{
		if (mIns[ai].mType == ASMIT_LDA && mIns[ai].mMode == ASMIM_IMMEDIATE)
		{
			int i = ai + 1;
			while (i < mIns.Size() && !mIns[i].ChangesAccu())
				i++;
			if (i == mIns.Size())
			{
				if (!prevBlock)
					return OptimizeSimpleLoopInvariant(proc);

				prevBlock->mIns.Push(mIns[ai]);
				mIns.Remove(ai);

				changed = true;
			}
		}
		else if (mIns[ai].mType == ASMIT_LDA && mIns[ai].mMode == ASMIM_ZERO_PAGE)
		{
			int i = ai + 1;
			while (i < mIns.Size() && !mIns[i].ChangesAccu() && !mIns[i].ChangesZeroPage(mIns[ai].mAddress))
				i++;
			if (i == mIns.Size())
			{
				if (!prevBlock)
					return OptimizeSimpleLoopInvariant(proc);

				prevBlock->mIns.Push(mIns[ai]);
				mIns.Remove(ai);

				changed = true;
			}

			i = mIns.Size() - 1;
			while (i >= 0 && !mIns[i].ChangesAccu() && mIns[i].mType != ASMIT_STA)
				i--;
			if (i >= 0 && mIns[i].mType == ASMIT_STA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[ai].mAddress)
			{
				if (!prevBlock)
					return OptimizeSimpleLoopInvariant(proc);

				prevBlock->mIns.Push(mIns[ai]);
				mIns.Remove(ai);

				changed = true;
			}
		}
	}

	ai = 0;
	while (ai < mIns.Size() && !mIns[ai].ChangesYReg())
		ai++;

	if (ai < mIns.Size())
	{
		if (mIns[ai].mType == ASMIT_LDY && mIns[ai].mMode == ASMIM_IMMEDIATE)
		{
			int i = ai + 1;
			while (i < mIns.Size() && !mIns[i].ChangesYReg())
				i++;
			if (i == mIns.Size())
			{
				if (!prevBlock)
					return OptimizeSimpleLoopInvariant(proc);

				prevBlock->mIns.Push(mIns[ai]);
				mIns.Remove(ai);

				changed = true;
			}
		}
		else if (mIns[ai].mType == ASMIT_LDY && mIns[ai].mMode == ASMIM_ZERO_PAGE)
		{
			int i = 0;
			while (i < mIns.Size() && (i == ai || !mIns[i].ChangesYReg()))
				i++;
			if (i == mIns.Size())
			{
				int addr = mIns[ai].mAddress;
				i = 0;
				while (i < mIns.Size() &&
					(mIns[i].mMode != ASMIM_ZERO_PAGE || mIns[i].mAddress != addr ||
						mIns[i].mType == ASMIT_LDA || mIns[i].mType == ASMIT_STA || mIns[i].mType == ASMIT_INC || mIns[i].mType == ASMIT_DEC || mIns[i].mType == ASMIT_LDY))
					i++;
				if (i == mIns.Size())
				{
					if (!prevBlock)
						return OptimizeSimpleLoopInvariant(proc);

					changed = true;

					prevBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, addr));
					exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STY, ASMIM_ZERO_PAGE, addr));
					for (int i = 0; i < mIns.Size(); i++)
					{
						if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == addr)
						{
							if (mIns[i].mType == ASMIT_LDA)
							{
								mIns[i].mType = ASMIT_TYA; mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (mIns[i].mType == ASMIT_STA)
							{
								mIns[i].mType = ASMIT_TAY; mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (mIns[i].mType == ASMIT_LDY)
							{
								mIns[i].mType = ASMIT_NOP; mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (mIns[i].mType == ASMIT_INC)
							{
								mIns[i].mType = ASMIT_INY; mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (mIns[i].mType == ASMIT_DEC)
							{
								mIns[i].mType = ASMIT_DEY; mIns[i].mMode = ASMIM_IMPLIED;
							}
						}
					}
				}
			}
		}
	}

	return changed;
}

bool NativeCodeBasicBlock::SimpleLoopReversal(NativeCodeProcedure* proc)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump && !mFalseJump && mTrueJump->mTrueJump == mTrueJump && mIns.Size() > 0 && mTrueJump->mIns.Size() > 1 && mTrueJump->mBranch == ASMIT_BCC)
		{
			NativeCodeBasicBlock* lb = mTrueJump;
			int	lbs = lb->mIns.Size();

			if (lb->mIns[lbs-1].mType == ASMIT_CPX)
			{
				int li = mIns.Size() - 1;
				while (li >= 0 && !mIns[li].ChangesXReg())
					li--;

				if (li >= 0 && lb->mIns[lbs-2].mType == ASMIT_INX && mIns[li].mType == ASMIT_LDX && mIns[li].mMode == ASMIM_IMMEDIATE)
				{
					if (lb->mIns[lbs - 1].mMode == ASMIM_ZERO_PAGE && mIns[li].mAddress == 0)
					{
						int	a = lb->mIns[lbs - 1].mAddress;

						int	i = 0;
						while (i + 2 < lbs && !(lb->mIns[i].RequiresXReg() || lb->mIns[i].ChangesZeroPage(a)))
							i++;
						if (i + 2 == lbs)
						{
							mIns[li].mMode = ASMIM_ZERO_PAGE;
							mIns[li].mAddress = a;
							lb->mIns[lbs - 2].mType = ASMIT_DEX;
							lb->mIns[lbs - 1].mType = ASMIT_NOP; lb->mIns[lbs - 1].mMode = ASMIM_IMPLIED;
							lb->mBranch = ASMIT_BNE;
						}
					}
					else if (lb->mIns[lbs - 1].mMode == ASMIM_IMMEDIATE)
					{
						int	a = lb->mIns[lbs - 1].mAddress - mIns[li].mAddress;

						int	i = 0;
						while (i + 2 < lbs && !lb->mIns[i].RequiresXReg())
							i++;
						if (i + 2 == lbs)
						{
							mIns[li].mAddress = a;
							lb->mIns[lbs - 2].mType = ASMIT_DEX;
							lb->mIns[lbs - 1].mType = ASMIT_NOP; lb->mIns[lbs - 1].mMode = ASMIM_IMPLIED;
							lb->mBranch = ASMIT_BNE;
						}
					}
				}

			}
			else if (lb->mIns[lbs - 1].mType == ASMIT_CPY)
			{
				NativeCodeBasicBlock* lb = mTrueJump;
				int	lbs = lb->mIns.Size();

				if (lb->mIns[lbs - 1].mType == ASMIT_CPY)
				{
					if (lb->mIns[lbs - 2].mType == ASMIT_INY && mIns.Last().mType == ASMIT_LDY && mIns.Last().mMode == ASMIM_IMMEDIATE && mIns.Last().mAddress == 0)
					{
						if (lb->mIns[lbs - 1].mMode == ASMIM_ZERO_PAGE)
						{
							int	a = lb->mIns[lbs - 1].mAddress;

							int	i = 0;
							while (i + 2 < lbs && !(lb->mIns[i].RequiresYReg() || lb->mIns[i].ChangesZeroPage(a)))
								i++;
							if (i + 2 == lbs)
							{
								mIns[mIns.Size() - 1].mMode = ASMIM_ZERO_PAGE;
								mIns[mIns.Size() - 1].mAddress = a;
								lb->mIns[lbs - 2].mType = ASMIT_DEY;
								lb->mIns[lbs - 1].mType = ASMIT_NOP; lb->mIns[lbs - 1].mMode = ASMIM_IMPLIED;
								lb->mBranch = ASMIT_BNE;
							}
						}
					}
				}
			}
		}

		if (mTrueJump && mTrueJump->SimpleLoopReversal(proc))
			changed = true;
		if (mFalseJump && mFalseJump->SimpleLoopReversal(proc))
			changed = true;
	}

	return changed;
}

bool NativeCodeBasicBlock::OptimizeSimpleLoop(NativeCodeProcedure * proc)
{
	if (!mVisited)
	{
		mVisited = true;

		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

		bool	changed = false;
		int	sz = mIns.Size();

#if 1
		if (sz > 3 &&
			mIns[sz - 2].mType == ASMIT_LDA && mIns[sz - 2].mMode == ASMIM_IMMEDIATE &&
			mIns[sz - 1].mType == ASMIT_CMP && mIns[sz - 1].mMode == ASMIM_ZERO_PAGE && !(mIns[sz - 1].mLive & LIVE_CPU_REG_A))
		{
			if (mBranch == ASMIT_BCS && mIns[sz - 2].mAddress < 0xff)
			{
				int	val = mIns[sz - 2].mAddress + 1;
				mBranch = ASMIT_BCC;
				mIns[sz - 2].mMode = ASMIM_ZERO_PAGE; mIns[sz - 2].mAddress = mIns[sz - 1].mAddress;
				mIns[sz - 1].mMode = ASMIM_IMMEDIATE; mIns[sz - 1].mAddress = val;
			}
			else if (mBranch == ASMIT_BCC && mIns[sz - 2].mAddress < 0xff)
			{
				int	val = mIns[sz - 2].mAddress + 1;
				mBranch = ASMIT_BCS;
				mIns[sz - 2].mMode = ASMIM_ZERO_PAGE; mIns[sz - 2].mAddress = mIns[sz - 1].mAddress;
				mIns[sz - 1].mMode = ASMIM_IMMEDIATE; mIns[sz - 1].mAddress = val;
			}
			else if ((mBranch == ASMIT_BEQ || mBranch == ASMIT_BNE) && !(mIns[sz - 1].mLive & LIVE_CPU_REG_C))
			{
				int	val = mIns[sz - 2].mAddress;
				mIns[sz - 2].mMode = ASMIM_ZERO_PAGE; mIns[sz - 2].mAddress = mIns[sz - 1].mAddress;
				mIns[sz - 1].mMode = ASMIM_IMMEDIATE; mIns[sz - 1].mAddress = val;
			}
		}
#endif
		if (mFalseJump == this)
		{
			mBranch = InvertBranchCondition(mBranch);
			mFalseJump = mTrueJump;
			mTrueJump = this;
		}

		if (sz == 2 && mTrueJump == this)
		{
			changed = OptimizeSimpleLoopInvariant(proc, nullptr, nullptr);
		}
		else if (sz > 3 && sz < 200 && mNumEntries == 2 && mTrueJump == this)
		{
			bool		simple = true;

			for(int i=0; i<mIns.Size(); i++)				
			{ 
				if (mIns[i].mType == ASMIT_JSR)
					simple = false;
			}

			if (simple)
			{
				if ((mIns[sz - 3].mType == ASMIT_INC || mIns[sz - 3].mType == ASMIT_DEC) && mIns[sz - 3].mMode == ASMIM_ZERO_PAGE &&
					mIns[sz - 2].mType == ASMIT_LDA && mIns[sz - 2].mMode == ASMIM_ZERO_PAGE && mIns[sz - 3].mAddress == mIns[sz - 2].mAddress &&
					mIns[sz - 1].mType == ASMIT_CMP && mIns[sz - 1].mMode == ASMIM_IMMEDIATE && !(mIns[sz - 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X | LIVE_CPU_REG_Y)) &&
					(mBranch == ASMIT_BCC || mBranch == ASMIT_BCS || mBranch == ASMIT_BNE))
				{
					// check for usage of Y register

					bool	yother = false, yindex = false, xother = false, xindex = false;
					int		zreg = mIns[sz - 3].mAddress;
					int		limit = mIns[sz - 1].mAddress;
					int		yinc = 0, xinc = 0;

					for (int i = 0; i < sz - 3; i++)
					{
						if (mIns[i].mType == ASMIT_TAY)
							yother = true;
						else if (mIns[i].mType == ASMIT_INY)
							yinc++;
						else if (mIns[i].mType == ASMIT_DEY)
							yinc--;
						else if (mIns[i].mType == ASMIT_LDY)
						{
							if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg && yinc >= -1 && yinc <= 1)
							{
								yinc = 0;
								yindex = true;
							}
							else
								yother = true;
						}
						else if (!yindex && (mIns[i].mType == ASMIT_STY || mIns[i].mType == ASMIT_TYA || mIns[i].mMode == ASMIM_ABSOLUTE_Y || mIns[i].mMode == ASMIM_INDIRECT_Y))
							yother = true;
						else if (mIns[i].mType != ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							yother = true;

						if (mIns[i].mType == ASMIT_TAX)
							xother = true;
						else if (mIns[i].mType == ASMIT_INX)
							xinc++;
						else if (mIns[i].mType == ASMIT_DEX)
							xinc--;
						else if (mIns[i].mType == ASMIT_LDX)
						{
							if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg && xinc >= -1 && xinc <= 1)
								xindex = true;
							else
								xother = true;
						}
						else if (!xindex && (mIns[i].mType == ASMIT_STX || mIns[i].mType == ASMIT_TXA || mIns[i].mMode == ASMIM_ABSOLUTE_X || mIns[i].mMode == ASMIM_INDIRECT_X))
							xother = true;
						else if (mIns[i].mType != ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							xother = true;
					}

					if (!yother)
					{
						int	linc = yinc;
						if (mIns[sz - 3].mType == ASMIT_INC)
							linc--;
						else
							linc++;

						NativeCodeBasicBlock* lblock = proc->AllocateBlock();
						NativeCodeBasicBlock* eblock = proc->AllocateBlock();

						yinc = 0;
						for (int i = 0; i + 3 < sz; i++)
						{
							if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
							else if (mIns[i].mType == ASMIT_LDY)
							{
								if (yinc > 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY));
								else if (yinc < 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY));
								yinc = 0;
							}
							else
							{
								lblock->mIns.Push(mIns[i]);
								if (mIns[i].mType == ASMIT_INY)
									yinc++;
								else if (mIns[i].mType == ASMIT_DEY)
									yinc--;
							}
						}

						while (linc < 0)
						{
							lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
							linc++;
						}
						while (linc > 0)
						{
							lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
							linc--;
						}

						lblock->mIns.Push(NativeCodeInstruction(ASMIT_CPY, ASMIM_IMMEDIATE, limit));
						lblock->mBranch = mBranch;
						lblock->mTrueJump = lblock;
						lblock->mFalseJump = eblock;

						eblock->mIns.Push(NativeCodeInstruction(ASMIT_STY, ASMIM_ZERO_PAGE, zreg));
						eblock->mBranch = ASMIT_JMP;
						eblock->mTrueJump = mFalseJump;
						eblock->mFalseJump = nullptr;


						mIns.SetSize(0);
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, zreg));
						mBranch = ASMIT_JMP;
						mTrueJump = lblock;
						mFalseJump = nullptr;

						lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);

						changed = true;

						assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
					}
					else if (!xother)
					{
						int	linc = xinc;
						if (mIns[sz - 3].mType == ASMIT_INC)
							linc--;
						else
							linc++;

						NativeCodeBasicBlock* lblock = proc->AllocateBlock();
						NativeCodeBasicBlock* eblock = proc->AllocateBlock();

						xinc = 0;
						for (int i = 0; i + 3 < sz; i++)
						{
							if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
							else if (mIns[i].mType == ASMIT_LDX)
							{
								if (xinc > 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX));
								else if (xinc < 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_INX));
								xinc = 0;
							}
							else
							{
								lblock->mIns.Push(mIns[i]);
								if (mIns[i].mType == ASMIT_INX)
									xinc++;
								else if (mIns[i].mType == ASMIT_DEX)
									xinc--;
							}
						}

						while (linc < 0)
						{
							lblock->mIns.Push(NativeCodeInstruction(ASMIT_INX, ASMIM_IMPLIED));
							linc++;
						}
						while (linc > 0)
						{
							lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
							linc--;
						}

						lblock->mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, limit));
						lblock->mBranch = mBranch;
						lblock->mTrueJump = lblock;
						lblock->mFalseJump = eblock;

						eblock->mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, zreg));
						eblock->mBranch = ASMIT_JMP;
						eblock->mTrueJump = mFalseJump;
						eblock->mFalseJump = nullptr;

						mIns.SetSize(0);
						mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, zreg));
						mBranch = ASMIT_JMP;
						mTrueJump = lblock;
						mFalseJump = nullptr;

						lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);

						changed = true;

						assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
					}
				}
				else if (mIns[sz - 1].mType == ASMIT_DEC && mIns[sz - 1].mMode == ASMIM_ZERO_PAGE && mBranch == ASMIT_BNE)
				{
					// check for usage of Y register

					bool	yother = false, yindex = false;
					int		zreg = mIns[sz - 1].mAddress;
					int		yinc = 0, xinc = 0;

					for (int i = 0; i < sz - 1; i++)
					{
						if (mIns[i].mType == ASMIT_TAY)
							yother = true;
						else if (mIns[i].mType == ASMIT_INY)
							yinc++;
						else if (mIns[i].mType == ASMIT_DEY)
							yinc--;
						else if (mIns[i].mType == ASMIT_LDY)
						{
							if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg && yinc >= -1 && yinc <= 1)
							{
								yinc = 0;
								yindex = true;
							}
							else
								yother = true;
						}
						else if (!yindex && (mIns[i].mType == ASMIT_STY || mIns[i].mType == ASMIT_TYA || mIns[i].mMode == ASMIM_ABSOLUTE_Y || mIns[i].mMode == ASMIM_INDIRECT_Y))
							yother = true;
						else if (mIns[i].mType != ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							yother = true;
					}

					if (!yother)
					{
						int	linc = yinc + 1;

						NativeCodeBasicBlock* lblock = proc->AllocateBlock();
						NativeCodeBasicBlock* eblock = proc->AllocateBlock();

						yinc = 0;
						for (int i = 0; i + 1 < sz; i++)
						{
							if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
							else if (mIns[i].mType == ASMIT_LDY)
							{
								if (yinc > 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY));
								else if (yinc < 0)
									lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY));
								yinc = 0;
							}
							else
							{
								lblock->mIns.Push(mIns[i]);
								if (mIns[i].mType == ASMIT_INY)
									yinc++;
								else if (mIns[i].mType == ASMIT_DEY)
									yinc--;
							}
						}

						if (linc == 0)
						{
							lblock->mIns.Push(NativeCodeInstruction(ASMIT_CPY, ASMIM_IMMEDIATE, 0));
						}
						else						
						{
							while (linc < 0)
							{
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
								linc++;
							}
							while (linc > 0)
							{
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
								linc--;
							}
						}

						lblock->mBranch = mBranch;
						lblock->mTrueJump = lblock;
						lblock->mFalseJump = eblock;

						eblock->mIns.Push(NativeCodeInstruction(ASMIT_STY, ASMIM_ZERO_PAGE, zreg));
						eblock->mBranch = ASMIT_JMP;
						eblock->mTrueJump = mFalseJump;
						eblock->mFalseJump = nullptr;

						mIns.SetSize(0);
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, zreg));
						mBranch = ASMIT_JMP;
						mTrueJump = lblock;
						mFalseJump = nullptr;

						lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);

						changed = true;

						assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
					}
				}
				else if (mIns[sz - 3].mType == ASMIT_INC && mIns[sz - 3].mMode == ASMIM_ZERO_PAGE &&
					mIns[sz - 2].mType == ASMIT_LDA && mIns[sz - 2].mMode == ASMIM_ZERO_PAGE && mIns[sz - 3].mAddress == mIns[sz - 2].mAddress &&
					mIns[sz - 1].mType == ASMIT_CMP && mIns[sz - 1].mMode == ASMIM_ZERO_PAGE && !(mIns[sz - 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X | LIVE_CPU_REG_Y)) &&
					mBranch == ASMIT_BCC)
				{
					// check for usage of Y register

					bool	yother = false, yindex = false, lchanged = false, xother = false, xindex = false;
					int		lreg = mIns[sz - 1].mAddress;
					int		zreg = mIns[sz - 3].mAddress;

					for (int i = 0; i < sz - 3; i++)
					{
						if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == lreg && mIns[i].ChangesAddress())
							lchanged = true;

						if (mIns[i].mType == ASMIT_INY || mIns[i].mType == ASMIT_DEY || mIns[i].mType == ASMIT_TAY)
							yother = true;
						else if (mIns[i].mType == ASMIT_LDY)
						{
							if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								yindex = true;
							else
								yother = true;
						}
						else if (!yindex && (mIns[i].mType == ASMIT_STY || mIns[i].mType == ASMIT_TYA || mIns[i].mMode == ASMIM_ABSOLUTE_Y || mIns[i].mMode == ASMIM_INDIRECT_Y))
							yother = true;
						else if (mIns[i].mType != ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							yother = true;

						if (mIns[i].mType == ASMIT_INX || mIns[i].mType == ASMIT_DEX || mIns[i].mType == ASMIT_TAX)
							xother = true;
						else if (mIns[i].mType == ASMIT_LDX)
						{
							if (mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								xindex = true;
							else
								xother = true;
						}
						else if (mIns[i].mType == ASMIT_LDY && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							xother = true;
						else if (!xindex && (mIns[i].mType == ASMIT_STX || mIns[i].mType == ASMIT_TXA || mIns[i].mMode == ASMIM_ABSOLUTE_X || mIns[i].mMode == ASMIM_INDIRECT_X))
							xother = true;
						else if (mIns[i].mType != ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
							xother = true;
					}

					if (!yother && !lchanged)
					{
						NativeCodeBasicBlock* lblock = proc->AllocateBlock();
						NativeCodeBasicBlock* eblock = proc->AllocateBlock();
						for (int i = 0; i + 3 < sz; i++)
						{
							if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
							else if (mIns[i].mType != ASMIT_LDY)
								lblock->mIns.Push(mIns[i]);
						}
						lblock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						lblock->mIns.Push(NativeCodeInstruction(ASMIT_CPY, ASMIM_ZERO_PAGE, lreg));
						lblock->mBranch = mBranch;
						lblock->mTrueJump = lblock;
						lblock->mFalseJump = eblock;

						eblock->mIns.Push(NativeCodeInstruction(ASMIT_STY, ASMIM_ZERO_PAGE, zreg));
						eblock->mBranch = ASMIT_JMP;
						eblock->mTrueJump = mFalseJump;
						eblock->mFalseJump = nullptr;

						mIns.SetSize(0);
						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, zreg));
						mBranch = ASMIT_JMP;
						mTrueJump = lblock;
						mFalseJump = nullptr;

						lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);

						changed = true;

						assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
					}
					else if (!xother && !lchanged)
					{
						NativeCodeBasicBlock* lblock = proc->AllocateBlock();
						NativeCodeBasicBlock* eblock = proc->AllocateBlock();
						for (int i = 0; i + 3 < sz; i++)
						{
							if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
							else if (mIns[i].mType != ASMIT_LDX)
								lblock->mIns.Push(mIns[i]);
						}
						lblock->mIns.Push(NativeCodeInstruction(ASMIT_INX, ASMIM_IMPLIED));
						lblock->mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_ZERO_PAGE, lreg));
						lblock->mBranch = mBranch;
						lblock->mTrueJump = lblock;
						lblock->mFalseJump = eblock;

						eblock->mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, zreg));
						eblock->mBranch = ASMIT_JMP;
						eblock->mTrueJump = mFalseJump;
						eblock->mFalseJump = nullptr;

						mIns.SetSize(0);
						mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, zreg));
						mBranch = ASMIT_JMP;
						mTrueJump = lblock;
						mFalseJump = nullptr;

						lblock->OptimizeSimpleLoopInvariant(proc, this, eblock);

						changed = true;

						assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);
					}
				}
#if 1
				if (!changed)
					changed = OptimizeSimpleLoopInvariant(proc, nullptr, nullptr);
#endif
			}
		}

		if (mTrueJump && mTrueJump->OptimizeSimpleLoop(proc))
			changed = true;
		if (mFalseJump && mFalseJump->OptimizeSimpleLoop(proc))
			changed = true;

		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

		return changed;
	}

	return false;
}

bool NativeCodeBasicBlock::OptimizeInnerLoop(NativeCodeProcedure* proc, NativeCodeBasicBlock* head, NativeCodeBasicBlock* tail, GrowingArray<NativeCodeBasicBlock*>& lblocks)
{
	bool		simple = true;

	for (int j = 0; j < lblocks.Size(); j++)
	{
		NativeCodeBasicBlock* block = lblocks[j];
		for (int i = 0; i < block->mIns.Size(); i++)
		{
			if (block->mIns[i].mType == ASMIT_JSR)
				simple = false;
		}
	}

	int sz = tail->mIns.Size();
	if (simple && sz >= 3)
	{
		if (tail->mIns[sz - 3].mType == ASMIT_INC && tail->mIns[sz - 3].mMode == ASMIM_ZERO_PAGE &&
			tail->mIns[sz - 2].mType == ASMIT_LDA && tail->mIns[sz - 2].mMode == ASMIM_ZERO_PAGE && tail->mIns[sz - 3].mAddress == tail->mIns[sz - 2].mAddress &&
			tail->mIns[sz - 1].mType == ASMIT_CMP && tail->mIns[sz - 1].mMode == ASMIM_IMMEDIATE && !(tail->mIns[sz - 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X | LIVE_CPU_REG_Y)) &&
			tail->mBranch == ASMIT_BCC && tail->mTrueJump == head)
		{
			// check for usage of Y register

			bool	yother = false, yindex = false, xother = false, xindex = false;
			int		zreg = tail->mIns[sz - 3].mAddress;
			int		limit = tail->mIns[sz - 1].mAddress;

			for (int j = 0; j < lblocks.Size(); j++)
			{
				NativeCodeBasicBlock* block = lblocks[j];

				int bz = block == tail ? block->mIns.Size() - 3 : block->mIns.Size();

				for (int i = 0; i < bz; i++)
				{
					if (block->mIns[i].mType == ASMIT_INY || block->mIns[i].mType == ASMIT_DEY || block->mIns[i].mType == ASMIT_TAY)
						yother = true;
					else if (block->mIns[i].mType == ASMIT_LDY)
					{
						if (block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
							yindex = true;
						else
							yother = true;
					}
					else if (!yindex && (block->mIns[i].mType == ASMIT_STY || block->mIns[i].mType == ASMIT_TYA || block->mIns[i].mMode == ASMIM_ABSOLUTE_Y || block->mIns[i].mMode == ASMIM_INDIRECT_Y))
						yother = true;
					else if (block->mIns[i].mType != ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
						yother = true;

					if (block->mIns[i].mType == ASMIT_INX || block->mIns[i].mType == ASMIT_DEX || block->mIns[i].mType == ASMIT_TAX)
						xother = true;
					else if (block->mIns[i].mType == ASMIT_LDX)
					{
						if (block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
							xindex = true;
						else
							xother = true;
					}
					else if (!xindex && (block->mIns[i].mType == ASMIT_STX || block->mIns[i].mType == ASMIT_TXA || block->mIns[i].mMode == ASMIM_ABSOLUTE_X || block->mIns[i].mMode == ASMIM_INDIRECT_X))
						xother = true;
					else if (block->mIns[i].mType != ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
						xother = true;

				}
			}
#if 1
			if (!yother)
			{
				NativeCodeBasicBlock* lblock = proc->AllocateBlock();
				NativeCodeBasicBlock* eblock = proc->AllocateBlock();

				tail->mIns.Remove(sz - 3);
				tail->mIns.Remove(sz - 3);
				tail->mIns.Remove(sz - 3);

				for (int j = 0; j < lblocks.Size(); j++)
				{
					NativeCodeBasicBlock* block = lblocks[j];

					int bz = block->mIns.Size();

					if (block == head)
					{
						for (int i = 0; i < bz; i++)
						{
							if (block->mIns[i].mType == ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
							else if (block->mIns[i].mType != ASMIT_LDY)
								lblock->mIns.Push(block->mIns[i]);
						}
					}
					else
					{
						for (int i = 0; i < bz; i++)
						{
							if (block->mIns[i].mType == ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
							{
								block->mIns[i].mType = ASMIT_TYA;
								block->mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (block->mIns[i].mType == ASMIT_LDY)
							{
								block->mIns[i].mType = ASMIT_NOP; block->mIns[i].mMode = ASMIM_IMPLIED;
							}
						}
					}
				}

				tail->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				tail->mIns.Push(NativeCodeInstruction(ASMIT_CPY, ASMIM_IMMEDIATE, limit));

				lblock->mBranch = head->mBranch;
				lblock->mTrueJump = head->mTrueJump;
				lblock->mFalseJump = head->mFalseJump;

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STY, ASMIM_ZERO_PAGE, zreg));
				eblock->mBranch = ASMIT_JMP;
				eblock->mTrueJump = tail->mFalseJump;
				eblock->mFalseJump = nullptr;

				tail->mTrueJump = lblock;
				tail->mFalseJump = eblock;

				head->mIns.SetSize(0);
				head->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, zreg));
				head->mBranch = ASMIT_JMP;
				head->mTrueJump = lblock;
				head->mFalseJump = nullptr;

				return true;
			}
			else 
#endif				
			if (!xother)
			{
				NativeCodeBasicBlock* lblock = proc->AllocateBlock();
				NativeCodeBasicBlock* eblock = proc->AllocateBlock();

				tail->mIns.Remove(sz - 3);
				tail->mIns.Remove(sz - 3);
				tail->mIns.Remove(sz - 3);

				for (int j = 0; j < lblocks.Size(); j++)
				{
					NativeCodeBasicBlock* block = lblocks[j];

					int bz = block->mIns.Size();

					if (block == head)
					{
						for (int i = 0; i < bz; i++)
						{
							if (block->mIns[i].mType == ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
								lblock->mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
							else if (block->mIns[i].mType != ASMIT_LDX)
								lblock->mIns.Push(block->mIns[i]);
						}
					}
					else
					{
						for (int i = 0; i < bz; i++)
						{
							if (block->mIns[i].mType == ASMIT_LDA && block->mIns[i].mMode == ASMIM_ZERO_PAGE && block->mIns[i].mAddress == zreg)
							{
								block->mIns[i].mType = ASMIT_TXA;
								block->mIns[i].mMode = ASMIM_IMPLIED;
							}
							else if (block->mIns[i].mType == ASMIT_LDX)
							{
								block->mIns[i].mType = ASMIT_NOP; block->mIns[i].mMode = ASMIM_IMPLIED;
							}
						}
					}
				}

				tail->mIns.Push(NativeCodeInstruction(ASMIT_INX, ASMIM_IMPLIED));
				tail->mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, limit));

				lblock->mBranch = head->mBranch;
				lblock->mTrueJump = head->mTrueJump;
				lblock->mFalseJump = head->mFalseJump;

				eblock->mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, zreg));
				eblock->mBranch = ASMIT_JMP;
				eblock->mTrueJump = tail->mFalseJump;
				eblock->mFalseJump = nullptr;

				tail->mTrueJump = lblock;
				tail->mFalseJump = eblock;

				head->mIns.SetSize(0);
				head->mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, zreg));
				head->mBranch = ASMIT_JMP;
				head->mTrueJump = lblock;
				head->mFalseJump = nullptr;

				return true;
			}
		}

	}

	return false;
}

void NativeCodeBasicBlock::CollectInnerLoop(NativeCodeBasicBlock* head, GrowingArray<NativeCodeBasicBlock*>& lblocks)
{
	if (mLoopHeadBlock != head)
	{
		mLoopHeadBlock = head;
		lblocks.Push(this);

		if (mTrueJump != head && mFalseJump != head)
		{
			if (mTrueJump)
				mTrueJump->CollectInnerLoop(head, lblocks);
			if (mFalseJump)
				mFalseJump->CollectInnerLoop(head, lblocks);
		}
	}
}

NativeCodeBasicBlock* NativeCodeBasicBlock::FindTailBlock(NativeCodeBasicBlock* head)
{
	if (mVisiting || mVisited)
		return nullptr;
	else if (mTrueJump == head || mFalseJump == head)
		return this;
	else
	{
		mVisiting = true;

		NativeCodeBasicBlock* tail = nullptr;
		if (mTrueJump)
		{
			tail = mTrueJump->FindTailBlock(head);
			if (tail && mFalseJump && mFalseJump->FindTailBlock(head) != tail)
				tail = nullptr;
		}
		else if (mFalseJump)
			tail = mFalseJump->FindTailBlock(head);

		mVisiting = false;

		return tail;
	}	
}

bool NativeCodeBasicBlock::OptimizeInnerLoops(NativeCodeProcedure* proc)
{
	bool changed = false;

	if (!mVisited)
	{
		if (mLoopHead)
		{
			NativeCodeBasicBlock* tail = FindTailBlock(this);

			if (tail)
			{
				GrowingArray<NativeCodeBasicBlock*>	 lblocks(nullptr);

				if (this == tail)
					lblocks.Push(this);
				else
					CollectInnerLoop(this, lblocks);

				changed = OptimizeInnerLoop(proc, this, tail, lblocks);
			}
		}

		mVisited = true;

		if (mTrueJump && mTrueJump->OptimizeInnerLoops(proc))
			changed = true;
		if (mFalseJump && mFalseJump->OptimizeInnerLoops(proc))
			changed = true;
	}

	return changed;
}


bool NativeCodeBasicBlock::OptimizeSelect(NativeCodeProcedure* proc)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		if (mFalseJump && mIns.Size() > 0 && mIns.Last().ChangesAccuAndFlag() &&
			mTrueJump->mIns.Size() == 1 && mFalseJump->mIns.Size() == 1 &&
			!mTrueJump->mFalseJump && !mFalseJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump &&
			mTrueJump->mIns[0].mType == ASMIT_LDA && mTrueJump->mIns[0].mMode == ASMIM_IMMEDIATE &&
			mFalseJump->mIns[0].mType == ASMIT_LDA && mFalseJump->mIns[0].mMode == ASMIM_IMMEDIATE)
		{
			if (mBranch == ASMIT_BNE || mBranch == ASMIT_BEQ)
			{
				char	vt = mTrueJump->mIns[0].mAddress, vf = mFalseJump->mIns[0].mAddress;
				mTrueJump = mTrueJump->mTrueJump;
				mFalseJump = nullptr;

				if (mBranch == ASMIT_BEQ)
				{
					char t = vt; vt = vf; vf = t;
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0xff));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, vt ^ vf));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, vt));
				changed = true;
			}
		}

		if (mTrueJump && mTrueJump->OptimizeSelect(proc))
			changed = true;
		if (mFalseJump && mFalseJump->OptimizeSelect(proc))
			changed = true;
	}

	return changed;
}

// Size reduction violating various assumptions such as no branches in basic blocks
// must be last step before actual assembly

void NativeCodeBasicBlock::BlockSizeReduction(void)
{
	if (!mVisited)
	{
		mVisited = true;

		int i = 0;
		int j = 0;

		if (mEntryBlocks.Size() == 1 && 
			mEntryBlocks[0]->mIns.Size() > 0 && mIns.Size() > 0 && 
			mIns[0].mType == ASMIT_CMP && mIns[0].mMode == ASMIM_IMMEDIATE &&
			mEntryBlocks[0]->mIns.Last().mType == ASMIT_CMP && mEntryBlocks[0]->mIns.Last().mMode == ASMIM_IMMEDIATE &&
			mIns[0].mAddress == mEntryBlocks[0]->mIns.Last().mAddress)
		{
			// Skip initial compare if same as last of entry block
			i++;
		}

		while (i < mIns.Size())
		{
			if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0 &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ZERO_PAGE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_INC; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 2;
				mIns[j + 2].mType = ASMIT_INC; mIns[j + 2].mMode = ASMIM_ZERO_PAGE; mIns[j + 2].mAddress = mIns[i + 4].mAddress;
				j += 3;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0xff &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ZERO_PAGE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_LDA; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 2;
				mIns[j + 2].mType = ASMIT_DEC; mIns[j + 2].mMode = ASMIM_ZERO_PAGE; mIns[j + 2].mAddress = mIns[i + 4].mAddress;
				mIns[j + 3].mType = ASMIT_DEC; mIns[j + 3].mMode = ASMIM_ZERO_PAGE; mIns[j + 3].mAddress = mIns[j + 0].mAddress;
				j += 4;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_TXA &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
				mIns[i + 3].mType == ASMIT_TAX &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0xff &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ZERO_PAGE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_TXA; mIns[j + 0].mMode = ASMIM_IMPLIED;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 2;
				mIns[j + 2].mType = ASMIT_DEC; mIns[j + 2].mMode = ASMIM_ZERO_PAGE; mIns[j + 2].mAddress = mIns[i + 4].mAddress;
				mIns[j + 3].mType = ASMIT_DEX; mIns[j + 3].mMode = ASMIM_IMPLIED;
				j += 4;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ABSOLUTE && mIns[i + 3].mAddress == mIns[i + 1].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0 &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ABSOLUTE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_INC; mIns[j + 0].mMode = ASMIM_ABSOLUTE; mIns[j + 0].mAddress = mIns[i + 1].mAddress; mIns[j + 0].mLinkerObject = mIns[i + 1].mLinkerObject;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 3;
				mIns[j + 2].mType = ASMIT_INC; mIns[j + 2].mMode = ASMIM_ABSOLUTE; mIns[j + 2].mAddress = mIns[i + 4].mAddress; mIns[j + 2].mLinkerObject = mIns[i + 4].mLinkerObject;
				j += 3;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ABSOLUTE && mIns[i + 3].mAddress == mIns[i + 1].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0xff &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ABSOLUTE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_LDA; mIns[j + 0].mMode = ASMIM_ABSOLUTE; mIns[j + 0].mAddress = mIns[i + 1].mAddress; mIns[j + 0].mLinkerObject = mIns[i + 1].mLinkerObject;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE; mIns[j + 1].mAddress = 3;
				mIns[j + 2].mType = ASMIT_DEC; mIns[j + 2].mMode = ASMIM_ABSOLUTE; mIns[j + 2].mAddress = mIns[i + 4].mAddress; mIns[j + 2].mLinkerObject = mIns[i + 4].mLinkerObject;
				mIns[j + 3].mType = ASMIT_DEC; mIns[j + 3].mMode = ASMIM_ABSOLUTE; mIns[j + 3].mAddress = mIns[j + 0].mAddress; mIns[j + 3].mLinkerObject = mIns[j + 0].mLinkerObject;
				j += 4;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 1].mType == ASMIT_CLC &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ABSOLUTE && mIns[i + 3].mAddress == mIns[i + 0].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0 &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ABSOLUTE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_INC; mIns[j + 0].mMode = ASMIM_ABSOLUTE; mIns[j + 0].mAddress = mIns[i + 0].mAddress; mIns[j + 0].mLinkerObject = mIns[i + 0].mLinkerObject;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE; mIns[j + 1].mAddress = 3;
				mIns[j + 2].mType = ASMIT_INC; mIns[j + 2].mMode = ASMIM_ABSOLUTE; mIns[j + 2].mAddress = mIns[i + 4].mAddress; mIns[j + 2].mLinkerObject = mIns[i + 4].mLinkerObject;
				j += 3;
				i += 7;
			}
			else if (i + 6 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 1].mType == ASMIT_CLC &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ABSOLUTE && mIns[i + 3].mAddress == mIns[i + 0].mAddress &&
				mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE && mIns[i + 5].mAddress == 0xff &&
				mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ABSOLUTE && mIns[i + 6].mAddress == mIns[i + 4].mAddress &&
				!(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_LDA; mIns[j + 0].mMode = ASMIM_ABSOLUTE; mIns[j + 0].mAddress = mIns[i + 0].mAddress; mIns[j + 0].mLinkerObject = mIns[i + 0].mLinkerObject;
				mIns[j + 1].mType = ASMIT_BNE; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 3;
				mIns[j + 2].mType = ASMIT_DEC; mIns[j + 2].mMode = ASMIM_ABSOLUTE; mIns[j + 2].mAddress = mIns[i + 4].mAddress; mIns[j + 2].mLinkerObject = mIns[i + 4].mLinkerObject;
				mIns[j + 3].mType = ASMIT_DEC; mIns[j + 3].mMode = ASMIM_ABSOLUTE; mIns[j + 3].mAddress = mIns[j + 0].mAddress; mIns[j + 2].mLinkerObject = mIns[j + 0].mLinkerObject;
				j += 4;
				i += 7;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCC; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 2;
				mIns[j + 1].mType = ASMIT_INC; mIns[j + 1].mMode = ASMIM_ZERO_PAGE; mIns[j + 1].mAddress = mIns[i + 2].mAddress;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ABSOLUTE && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCC; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 3;
				mIns[j + 1] = mIns[i + 2];
				mIns[j + 1].mType = ASMIT_INC;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_TXA &&
				mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_TAX &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCC; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 1;
				mIns[j + 1].mType = ASMIT_INX; mIns[j + 1].mMode = ASMIM_IMPLIED;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 1].mType == ASMIT_SBC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCS; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 2;
				mIns[j + 1].mType = ASMIT_DEC; mIns[j + 1].mMode = ASMIM_ZERO_PAGE; mIns[j + 1].mAddress = mIns[i + 2].mAddress;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE &&
				mIns[i + 1].mType == ASMIT_SBC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ABSOLUTE && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCS; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 3;
				mIns[j + 1] = mIns[i + 2];
				mIns[j + 1].mType = ASMIT_DEC;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_TXA &&
				mIns[i + 1].mType == ASMIT_SBC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_TAX &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_BCS; mIns[j + 0].mMode = ASMIM_RELATIVE; mIns[j + 0].mAddress = 1;
				mIns[j + 1].mType = ASMIT_DEX; mIns[j + 1].mMode = ASMIM_IMPLIED;
				j += 2;
				i += 3;
			}
#if 1
			else if (i + 3 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_ASL && mIns[i + 0].mMode == ASMIM_IMPLIED &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
				mIns[i + 3].mType == ASMIT_EOR && mIns[i + 3].mMode == ASMIM_IMMEDIATE && mIns[i + 3].mAddress == 0xff &&
				!(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_AND; mIns[j + 0].mMode = ASMIM_IMMEDIATE; mIns[j + 0].mAddress = 0x80;
				mIns[j + 1].mType = ASMIT_BPL; mIns[j + 1].mMode = ASMIM_RELATIVE;  mIns[j + 1].mAddress = 2;   
				mIns[j + 2].mType = ASMIT_LDA; mIns[j + 2].mMode = ASMIM_IMMEDIATE; mIns[j + 2].mAddress = 0xff;
				j += 3;
				i += 4;
			}
#endif
			else if (i + 3 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE &&
				!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z | LIVE_CPU_REG_X)))
			{
				mIns[j + 0].mType = ASMIT_LDX; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_INX; mIns[j + 1].mMode = ASMIM_IMPLIED;
				mIns[j + 2].mType = ASMIT_STX; mIns[j + 2].mMode = ASMIM_ZERO_PAGE; mIns[j + 2].mAddress = mIns[i + 3].mAddress;
				j += 3;
				i += 4;
			}
			else if (i + 3 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_SEC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE &&
				!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z | LIVE_CPU_REG_X)))
			{
				mIns[j + 0].mType = ASMIT_LDX; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_DEX; mIns[j + 1].mMode = ASMIM_IMPLIED;
				mIns[j + 2].mType = ASMIT_STX; mIns[j + 2].mMode = ASMIM_ZERO_PAGE; mIns[j + 2].mAddress = mIns[i + 3].mAddress;
				j += 3;
				i += 4;
			}
			else if (i + 3 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 2 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 3].mAddress &&
				!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_INC; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_INC; mIns[j + 1].mMode = ASMIM_ZERO_PAGE; mIns[j + 1].mAddress = mIns[i + 3].mAddress;
				j += 2;
				i += 4;
			}
			else if (i + 3 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_SEC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 2 &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 3].mAddress &&
				!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
			{
				mIns[j + 0].mType = ASMIT_DEC; mIns[j + 0].mMode = ASMIM_ZERO_PAGE; mIns[j + 0].mAddress = mIns[i + 1].mAddress;
				mIns[j + 1].mType = ASMIT_DEC; mIns[j + 1].mMode = ASMIM_ZERO_PAGE; mIns[j + 1].mAddress = mIns[i + 3].mAddress;
				j += 2;
				i += 4;
			}
			else if (i + 1 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE_X &&
				mIns[i + 1].mType == ASMIT_TAY && !(mIns[i + 1].mLive & LIVE_CPU_REG_A))
			{
				mIns[j] = mIns[i];
				mIns[j].mType = ASMIT_LDY;
				mIns[j].mLive |= LIVE_CPU_REG_Y;
				j++;
				i += 2;
			}
			else if (i + 1 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ABSOLUTE_Y &&
				mIns[i + 1].mType == ASMIT_TAX && !(mIns[i + 1].mLive & LIVE_CPU_REG_A))
			{
				mIns[j] = mIns[i];
				mIns[j].mType = ASMIT_LDX;
				mIns[j].mLive |= LIVE_CPU_REG_Y;
				j++;
				i += 2;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDY && !(mIns[i + 0].mLive & LIVE_CPU_REG_X) && (mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE) &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ABSOLUTE_Y &&
				mIns[i + 2].mType == ASMIT_TAY && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
			{
				mIns[j + 0] = mIns[i + 0];
				mIns[j + 1] = mIns[i + 1];

				mIns[j + 0].mType = ASMIT_LDX;
				mIns[j + 0].mLive |= LIVE_CPU_REG_X;
				mIns[j + 1].mType = ASMIT_LDY;
				mIns[j + 1].mMode = ASMIM_ABSOLUTE_X;
				mIns[j + 1].mLive |= LIVE_CPU_REG_Y;
				j += 2;
				i += 3;
			}
			else if (i + 2 < mIns.Size() &&
				mIns[i + 0].mType == ASMIT_LDX && !(mIns[i + 0].mLive & LIVE_CPU_REG_Y) && (mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE) &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ABSOLUTE_X &&
				mIns[i + 2].mType == ASMIT_TAX && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
			{
				mIns[j + 0] = mIns[i + 0];
				mIns[j + 1] = mIns[i + 1];

				mIns[j + 0].mType = ASMIT_LDY;
				mIns[j + 0].mLive |= LIVE_CPU_REG_Y;
				mIns[j + 1].mType = ASMIT_LDX;
				mIns[j + 1].mMode = ASMIM_ABSOLUTE_Y;
				mIns[j + 1].mLive |= LIVE_CPU_REG_X;
				j += 2;
				i += 3;
			}
			else if (i + 5 < mIns.Size() &&
				mIns[i + 0].ChangesAccuAndFlag() &&
				mIns[i + 1].mType == ASMIT_CMP && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0x01 &&
				mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0x00 &&
				mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_IMMEDIATE && mIns[i + 3].mAddress == 0xff &&
				mIns[i + 4].mType == ASMIT_AND && mIns[i + 4].mMode == ASMIM_IMMEDIATE &&
				mIns[i + 5].mType == ASMIT_EOR && mIns[i + 5].mMode == ASMIM_IMMEDIATE)
			{
				char veq = mIns[i + 4].mAddress ^ mIns[i + 5].mAddress, vne = mIns[i + 5].mAddress;

				mIns[j + 0] = mIns[i + 0];
				mIns[j + 1].mType = ASMIT_BEQ; mIns[j + 1].mMode = ASMIM_RELATIVE; mIns[j + 1].mAddress = veq != 0 ? 4 : 2;
				mIns[j + 2].mType = ASMIT_LDA; mIns[j + 2].mMode = ASMIM_IMMEDIATE; mIns[j + 2].mAddress = vne; mIns[j + 2].mFlags = 0;
				j += 3;
				if (veq != 0)
				{
					if (vne)
						mIns[j + 0].mType = ASMIT_BNE;
					else
						mIns[j + 0].mType = ASMIT_BEQ;
					mIns[j + 0].mMode = ASMIM_RELATIVE;
					mIns[j + 0].mAddress = 2;
					mIns[j + 1].mType = ASMIT_LDA; mIns[j + 1].mMode = ASMIM_IMMEDIATE; mIns[j + 1].mAddress = veq; mIns[j + 1].mFlags = 0;
					j += 2;
				}
				i += 6;
			}
			else if (i + 1 < mIns.Size() &&
				mIns[i + 0].ChangesZFlag() && mIns[i + 1].mType == ASMIT_LDA && mIns[i + 0].SameEffectiveAddress(mIns[i + 1]) && !(mIns[i + 1].mLive & LIVE_CPU_REG_A))
			{
				mIns[j++] = mIns[i++];
				i++;
			}
			else
				mIns[j++] = mIns[i++];
		}
		mIns.SetSize(j);
#if 1
		bool	yimm = false, ximm = false;
		int		yval = 0, xval = 0;
		for(int i = 0; i < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDY)
			{
				if (mIns[i].mMode == ASMIM_IMMEDIATE)
				{
					if (yimm && mIns[i].mAddress == ((yval + 1) & 0xff))
					{
						yval = mIns[i].mAddress;
						mIns[i].mType = ASMIT_INY;
						mIns[i].mMode = ASMIM_IMPLIED;
					}
					else if (yimm && mIns[i].mAddress == ((yval - 1) & 0xff))
					{
						yval = mIns[i].mAddress;
						mIns[i].mType = ASMIT_DEY;
						mIns[i].mMode = ASMIM_IMPLIED;
					}
					else
					{
						yimm = true;
						yval = mIns[i].mAddress;
					}

				}
				else
					yimm = false;
			}
			else if (yimm && mIns[i].mType == ASMIT_INY)
			{
				yval = (yval + 1) & 0xff;
			}
			else if (yimm && mIns[i].mType == ASMIT_DEY)
			{
				yval = (yval - 1) & 0xff;
			}
			else if (mIns[i].mType == ASMIT_TAY)
				yimm = false;
			else if (mIns[i].mType == ASMIT_LDX)
			{
				if (mIns[i].mMode == ASMIM_IMMEDIATE)
				{
					if (ximm && mIns[i].mAddress == ((xval + 1) & 0xff))
					{
						xval = mIns[i].mAddress;
						mIns[i].mType = ASMIT_INX;
						mIns[i].mMode = ASMIM_IMPLIED;
					}
					else if (ximm && mIns[i].mAddress == ((xval - 1) & 0xff))
					{
						xval = mIns[i].mAddress;
						mIns[i].mType = ASMIT_DEX;
						mIns[i].mMode = ASMIM_IMPLIED;
					}
					else
					{
						ximm = true;
						xval = mIns[i].mAddress;
					}

				}
				else
					ximm = false;
			}
			else if (ximm && mIns[i].mType == ASMIT_INX)
			{
				xval = (xval + 1) & 0xff;
			}
			else if (ximm && mIns[i].mType == ASMIT_DEX)
			{
				xval = (xval - 1) & 0xff;
			}
			else if (mIns[i].mType == ASMIT_TAX)
				ximm = false;
			else if (mIns[i].mType == ASMIT_JSR)
				yimm = ximm = false;
		}
#endif

		bool	carrySet = true, carryClear = true;

		for (int i = 0; i < mEntryBlocks.Size(); i++)
		{
			if (mEntryBlocks[i]->mBranch == ASMIT_BCC)
			{
				if (mEntryBlocks[i]->mTrueJump == this)
					carrySet = false;
				else
					carryClear = false;
			}
			else if (mEntryBlocks[i]->mBranch == ASMIT_BCS)
			{
				if (mEntryBlocks[i]->mTrueJump == this)
					carryClear = false;
				else
					carrySet = false;
			}
			else
				carryClear = carrySet = false;
		}

#if 1
		i = 0;
		j = 0;
		int	accuVal = 0, accuMask = 0;
		while (i < mIns.Size())
		{
			bool	skip = false;
			switch (mIns[i].mType)
			{
			case ASMIT_CLC:
				if (carryClear)
					skip = true;
				else
				{
					carryClear = true;
					carrySet = false;
				}
				break;
			case ASMIT_SEC:
				if (carrySet)
					skip = true;
				else
				{
					carryClear = false;
					carrySet = true;
				}
				break;
			case ASMIT_ADC:
			case ASMIT_SBC:
				accuMask = 0;
			case ASMIT_CMP:
				carryClear = false;
				carrySet = false;
				break;
			case ASMIT_AND:
				if (mIns[i].mMode == ASMIM_IMMEDIATE)
				{
					int	clear = mIns[i].mAddress ^ 0xff;
					if (!(mIns[i].mLive & CPU_REG_Z) && !(clear & (accuMask ^ 0xff)) && !(accuVal & clear))
						skip = true;
					accuVal = 0;
					accuMask = clear;
				}
				else
					accuMask = 0;
				break;
			case ASMIT_ORA:
				if (mIns[i].mMode == ASMIM_IMMEDIATE)
				{
					accuVal = 0xff;
					accuMask = mIns[i].mAddress;
				}
				else
					accuMask = 0;
				break;
			case ASMIT_LDA:
				if (mIns[i].mMode == ASMIM_IMMEDIATE)
				{
					accuVal = mIns[i].mAddress;
					accuMask = 0xff;
				}
				else
					accuMask = 0x00;
				break;
			case ASMIT_ASL:
				if (mIns[i].mMode == ASMIM_IMPLIED && (accuMask & 0x80))
				{
					accuVal <<= 1;
					accuMask = ((accuMask << 1) | 1) & 0xff;

					if (accuVal & 0x100)
					{
						carryClear = false;
						carrySet = true;
					}
					else
					{
						carryClear = true;
						carrySet = false;
					}
					accuVal &= 0xff;
				}
				else
				{
					carryClear = false;
					carrySet = false;
				}
				break;
			case ASMIT_LSR:
				if (mIns[i].mMode == ASMIM_IMPLIED && (accuMask & 0x01))
				{
					accuMask = (accuMask >> 1) | 0x80;

					if (accuVal & 0x01)
					{
						carryClear = false;
						carrySet = true;
					}
					else
					{
						carryClear = true;
						carrySet = false;
					}

					accuVal >>= 1;
				}
				else
				{
					carryClear = false;
					carrySet = false;
				}
				break;
			case ASMIT_ROL:
				if (mIns[i].mMode == ASMIM_IMPLIED && (accuMask & 0x80))
				{
					accuVal <<= 1;
					accuMask = (accuMask << 1) & 0xff;

					if (accuVal & 0x100)
					{
						carryClear = false;
						carrySet = true;
					}
					else
					{
						carryClear = true;
						carrySet = false;
					}
					accuVal &= 0xff;
				}
				else
				{
					carryClear = false;
					carrySet = false;
				}
				break;
			case ASMIT_ROR:
				if (mIns[i].mMode == ASMIM_IMPLIED && (accuMask & 0x01))
				{
					accuMask = (accuMask >> 1);

					if (accuVal & 0x01)
					{
						carryClear = false;
						carrySet = true;
					}
					else
					{
						carryClear = true;
						carrySet = false;
					}

					accuVal >>= 1;
				}
				else
				{
					carryClear = false;
					carrySet = false;
				}
				break;
			default:
				if (mIns[i].ChangesCarry())
				{
					carryClear = false;
					carrySet = false;
				}

				if (mIns[i].ChangesAccu())
					accuMask = 0;
			}

			if (!skip)
			{
				if (i != j)
					mIns[j] = mIns[i];
				j++;
			}
			i++;
		}
		mIns.SetSize(j);

#endif
		if (mTrueJump)
			mTrueJump->BlockSizeReduction();
		if (mFalseJump)
			mFalseJump->BlockSizeReduction();
	}
}


bool NativeCodeBasicBlock::RemoveNops(void)
{
	bool changed = false;

	assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

	int i = 0;
	int j = 0;
	while (i < mIns.Size())
	{
		if (mIns[i].mType == ASMIT_NOP)
			;
		else
		{
			if (i != j)
				mIns[j] = mIns[i];
			j++;
		}
		i++;
	}
	if (j != i)
		changed = true;
	mIns.SetSize(j);
	mIns.Reserve(2 * j);

	return changed;
}

bool NativeCodeBasicBlock::PeepHoleOptimizer(int pass)
{
	if (!mVisited)
	{
		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

		bool	changed = RemoveNops();

		mVisited = true;

#if 1
		// move load store pairs up to initial store

		for (int i = 2; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && (mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveLoadStoreUp(i))
					changed = true;
			}
		}
#endif

#if 1
		// replace zero page up

		for (int i = 1; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) && !(mIns[i + 0].mLive & LIVE_MEM))
			{
				if (ReplaceZeroPageUp(i))
					changed = true;
			}
		}
#endif


#if 1
		// move load store pairs up to initial store

		for (int i = 2; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDX && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_STX && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && (mIns[i + 1].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveLoadStoreXUp(i))
					changed = true;
			}
		}
#endif


#if 1
		// move load - store (),y up to initial store
		// 

		for (int i = 2; i + 2 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y)
			{
				if (MoveIndirectLoadStoreUp(i))
					changed = true;
			}
		}
#endif

#if 1
		// move load (),y store zp down to potential user
		for (int i = 2; i + 2 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_INDIRECT_Y && mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
			{
				if (MoveIndirectLoadStoreDown(i))
					changed = true;
			}
		}

#endif

#if 1
		// move load - store abs up to initial store
		// 

		for (int i = 2; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ABSOLUTE)
			{
				if (MoveAbsoluteLoadStoreUp(i))
					changed = true;
			}
		}
#endif


#if 1
		// move load - add # - store up to initial store
		// 

		for (int i = 2; i + 3 < mIns.Size(); i++)
		{
			if (
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && (mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveLoadAddImmStoreUp(i))
					changed = true;
			}
		}
#endif


#if 1
		// move load - add ZP - store up to initial store
		// 

		for (int i = 2; i + 3 < mIns.Size(); i++)
		{
			if (
				mIns[i + 0].mType == ASMIT_CLC &&
				mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && (mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveCLCLoadAddZPStoreUp(i))
					changed = true;
			}
			else if (
				mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && (mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveLoadAddZPStoreUp(i))
					changed = true;
			}
		}
#endif

#if 1
		// move simple add down to consumer

		if (!changed)
		{
			for (int i = 0; i + 4 < mIns.Size(); i++)
			{
				if (
					mIns[i + 0].mType == ASMIT_CLC &&
					mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
					mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
					mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && (mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_C)) == 0)
				{
					if (MoveCLCLoadAddZPStoreDown(i))
						changed = true;
				}
			}
		}
#endif

		// 
		// shorten x/y register livetime

#if 1
		//
		// move ldx/y down

		for (int i = 0; i + 2 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDY)
			{
				if (!mIns[i + 1].RequiresYReg() && !mIns[i + 1].ChangesYReg() && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
				{
					if (mIns[i].mMode != ASMIM_ABSOLUTE_X || !mIns[i + 1].ChangesXReg())
					{
						if (!mIns[i].MayBeChangedOnAddress(mIns[i + 1]))
						{
							if (mIns[i + 1].SameEffectiveAddress(mIns[i]))
								mIns[i + 1].mLive |= LIVE_MEM;

							NativeCodeInstruction	ins = mIns[i];
							mIns[i] = mIns[i + 1];
							mIns[i + 1] = ins;
							mIns[i + 1].mLive |= mIns[i].mLive;
						}
					}
				}
			}
			else if (mIns[i].mType == ASMIT_LDX)
			{
				if (!mIns[i + 1].RequiresXReg() && !mIns[i + 1].ChangesXReg() && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
				{
					if (mIns[i].mMode != ASMIM_ABSOLUTE_Y || !mIns[i + 1].ChangesYReg())
					{
						if (!mIns[i].MayBeChangedOnAddress(mIns[i + 1]))
						{
							if (mIns[i + 1].SameEffectiveAddress(mIns[i]))
								mIns[i + 1].mLive |= LIVE_MEM;

							NativeCodeInstruction	ins = mIns[i];
							mIns[i] = mIns[i + 1];
							mIns[i + 1] = ins;
							mIns[i + 1].mLive |= mIns[i].mLive;
						}
					}
				}
			}
		}
#endif

#if 1
		// move stx up

		for (int i = 1; i < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_STX && (mIns[i].mMode == ASMIM_ZERO_PAGE || mIns[i].mMode == ASMIM_ABSOLUTE))
			{
				if (MoveStoreXUp(i))
					changed = true;
			}
			else if (mIns[i].mType == ASMIT_LDX && mIns[i].mMode == ASMIM_ZERO_PAGE && !(mIns[i].mLive & LIVE_MEM))
			{
				if (MoveLoadXUp(i))
					changed = true;
			}
		}
#endif

#if 1
		// move clc and sec down
		for (int i = 0; i + 1 < mIns.Size(); i++)
		{
			if ((mIns[i].mType == ASMIT_CLC || mIns[i].mType == ASMIT_SEC) && !mIns[i + 1].RequiresCarry() && !mIns[i + 1].ChangesCarry())
			{
				if (i + 2 < mIns.Size() && mIns[i + 1].mType == ASMIT_LDA && (mIns[i + 1].mMode == ASMIM_IMMEDIATE || mIns[i + 1].mMode == ASMIM_IMMEDIATE_ADDRESS || mIns[i + 1].mMode == ASMIM_ZERO_PAGE) && mIns[i + 2].RequiresCarry())
					;
				else if (i + 2 < mIns.Size() && mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mMode == ASMIM_INDIRECT_Y)
					;
				else
				{
					NativeCodeInstruction	pins = mIns[i];
					mIns[i] = mIns[i + 1];
					mIns[i + 1] = pins;
//					changed = true;
				}
			}
		}

#endif

#if 1
		// move iny/dey/inx/dex down

		for (int i = 0; i + 1 < mIns.Size(); i++)
		{
			if ((mIns[i].mType == ASMIT_INY || mIns[i].mType == ASMIT_DEY) && !mIns[i + 1].ChangesYReg() && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
			{
				if (!mIns[i + 1].RequiresYReg())
				{
					NativeCodeInstruction	pins = mIns[i];
					mIns[i] = mIns[i + 1];
					mIns[i + 1] = pins;
				}
				else if (mIns[i + 1].mMode == ASMIM_ABSOLUTE_Y)
				{
					if (mIns[i].mType == ASMIT_INY)
						mIns[i + 1].mAddress++;
					else
						mIns[i + 1].mAddress--;
					NativeCodeInstruction	pins = mIns[i];
					mIns[i] = mIns[i + 1];
					mIns[i + 1] = pins;
				}
			}
			else if ((mIns[i].mType == ASMIT_INX || mIns[i].mType == ASMIT_DEX) && !mIns[i + 1].ChangesXReg() && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
			{
				if (!mIns[i + 1].RequiresXReg())
				{
					NativeCodeInstruction	pins = mIns[i];
					mIns[i] = mIns[i + 1];
					mIns[i + 1] = pins;
				}
				else if (mIns[i + 1].mMode == ASMIM_ABSOLUTE_X)
				{
					if (mIns[i].mType == ASMIT_INX)
						mIns[i + 1].mAddress++;
					else
						mIns[i + 1].mAddress--;
					NativeCodeInstruction	pins = mIns[i];
					mIns[i] = mIns[i + 1];
					mIns[i + 1] = pins;
				}
			}
		}
#endif
#if 1
		// move tya/clc/adc/tay down
		for (int i = 0; i + 5 < mIns.Size(); i++)
		{
			if (mIns[i + 0].mType == ASMIT_TYA && mIns[i + 1].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 3].mType == ASMIT_TAY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
			{
				if (mIns[i + 4].mType == ASMIT_LDA && (mIns[i + 4].mMode == ASMIM_IMMEDIATE || mIns[i + 4].mMode == ASMIM_IMMEDIATE_ADDRESS || mIns[i + 4].mMode == ASMIM_ZERO_PAGE) &&
					mIns[i + 5].mType == ASMIT_STA && !(mIns[i + 5].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
				{
					mIns[i + 4].mLive |= LIVE_CPU_REG_Y;
					mIns[i + 5].mLive |= LIVE_CPU_REG_Y;
					if (mIns[i + 5].mMode == ASMIM_ABSOLUTE_Y)
						mIns[i + 5].mAddress += mIns[i + 2].mAddress;

					mIns.Insert(i + 0, mIns[i + 4]); mIns.Remove(i + 5);
					mIns.Insert(i + 1, mIns[i + 5]); mIns.Remove(i + 6);
				}
#if 1
				else if (i + 6 < mIns.Size() &&
					mIns[i + 4].mType == ASMIT_LDA && (mIns[i + 4].mMode == ASMIM_IMMEDIATE || mIns[i + 4].mMode == ASMIM_IMMEDIATE_ADDRESS || mIns[i + 4].mMode == ASMIM_ZERO_PAGE) &&
					mIns[i + 5].mType == ASMIT_STA && 
					mIns[i + 6].mType == ASMIT_STA && !(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
				{
					mIns[i + 4].mLive |= LIVE_CPU_REG_Y;
					mIns[i + 5].mLive |= LIVE_CPU_REG_Y;
					mIns[i + 6].mLive |= LIVE_CPU_REG_Y;
					if (mIns[i + 5].mMode == ASMIM_ABSOLUTE_Y)
						mIns[i + 5].mAddress += mIns[i + 2].mAddress;
					if (mIns[i + 6].mMode == ASMIM_ABSOLUTE_Y)
						mIns[i + 6].mAddress += mIns[i + 2].mAddress;

					mIns.Insert(i + 0, mIns[i + 4]); mIns.Remove(i + 5);
					mIns.Insert(i + 1, mIns[i + 5]); mIns.Remove(i + 6);
					mIns.Insert(i + 2, mIns[i + 6]); mIns.Remove(i + 7);
				}
#endif
			}
		}

#endif

#if 1

		// reverse "sta t,lda abs,clc,adc t" to "sta t,clc,adc abs,nop"

		for (int i = 1; i + 2 < mIns.Size(); i++)
		{
			if (mIns[i + 2].IsCommutative() && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && (mIns[i + 1].mType == ASMIT_CLC || mIns[i + 1].mType == ASMIT_SEC) && mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode != ASMIM_ZERO_PAGE)
			{
				if (ReverseLoadCommutativeOpUp(i, i + 2))
					changed = true;
			}
			else if (mIns[i + 1].IsCommutative() && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode != ASMIM_ZERO_PAGE)
			{
				if (ReverseLoadCommutativeOpUp(i, i + 1))
					changed = true;
			}
		}

		// shortcut index

		for (int i = 0; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_TXA && mIns[i + 1].mType == ASMIT_TAY)
			{
				int j = i + 2;
				while (j < mIns.Size() && !mIns[j].ChangesXReg() && !mIns[j].ChangesYReg())
				{
					if (mIns[j].mMode == ASMIM_ABSOLUTE_Y)
					{
						assert(HasAsmInstructionMode(mIns[j].mType, ASMIM_ABSOLUTE_X));
						mIns[j].mMode = ASMIM_ABSOLUTE_X;
						changed = true;
					}
					j++;
				}
			}
			else if (mIns[i].mType == ASMIT_TYA && mIns[i + 1].mType == ASMIT_TAX)
			{
				int j = i + 2;
				while (j < mIns.Size() && !mIns[j].ChangesXReg() && !mIns[j].ChangesYReg())
				{
					if (mIns[j].mMode == ASMIM_ABSOLUTE_X)
					{
						assert(HasAsmInstructionMode(mIns[j].mType, ASMIM_ABSOLUTE_Y));
						mIns[j].mMode = ASMIM_ABSOLUTE_Y;
						changed = true;
					}
					j++;
				}
			}
		}

#endif

#if 1
		for (int i = 0; i + 2 < mIns.Size(); i++)
		{
			if ((mIns[i + 0].mType == ASMIT_TAX || mIns[i + 0].mType == ASMIT_TAY) &&
				mIns[i + 1].mType == ASMIT_LDA && (mIns[i + 1].mMode == ASMIM_ABSOLUTE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE) &&
				mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 2].mLive & (LIVE_CPU_REG_Z | LIVE_CPU_REG_A)))
			{
				if (MoveLoadStoreOutOfXYRangeUp(i))
					changed = true;
			}
		}

#endif

		int		taxPos = -1, tayPos = -1;
		for (int i = 0; i < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_TAX)
			{
				if (!(mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_C)))
					taxPos = i;
				else
					taxPos = -1;
			}
			else if (mIns[i].mType == ASMIT_TAY)
			{
				if (!(mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_C)))
					tayPos = i;
				else
					tayPos = -1;
			}
			else if (mIns[i].ChangesXReg())
				taxPos = -1;
			else if (mIns[i].ChangesYReg())
				tayPos = -1;
			else if (mIns[i].mType == ASMIT_TXA)
			{
				if (!(mIns[i].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))
				{
					if (JoinTAXARange(taxPos, i))
						changed = true;
					taxPos = -1; tayPos = -1;
				}
				else
					taxPos = -1;
			}
			else if (mIns[i].mType == ASMIT_TYA)
			{
				if (!(mIns[i].mLive & (LIVE_CPU_REG_Y | LIVE_CPU_REG_Z)))
				{
					if (JoinTAYARange(tayPos, i))
						changed = true;
					taxPos = -1; tayPos = -1;
				}
				else
					taxPos = -1;
			}
			else if (mIns[i].RequiresXReg())
				taxPos = -1;
			else if (mIns[i].RequiresYReg())
				tayPos = -1;
		}

#if 1
		if (pass > 1)
		{
			// move high byte load down, if low byte is immediatedly needed afterwards

			for (int i = 0; i + 4 < mIns.Size(); i++)
			{
				if (mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
					mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
					mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y && mIns[i + 2].mAddress != mIns[i + 3].mAddress && mIns[i + 2].mAddress + 1 != mIns[i + 3].mAddress &&
					mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress != mIns[i + 0].mAddress &&
					mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ZERO_PAGE && mIns[i + 4].mAddress == mIns[i + 0].mAddress && !(mIns[i + 4].mLive & LIVE_CPU_REG_Z))
				{
					if (MoveStoreHighByteDown(i))
						changed = true;
				}
			}

			for (int i = 0; i + 4 < mIns.Size(); i++)
			{
				if (mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
					mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress != mIns[i + 0].mAddress &&
					mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE &&
					mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress != mIns[i + 0].mAddress &&
					mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_ZERO_PAGE && mIns[i + 4].mAddress == mIns[i + 0].mAddress && !(mIns[i + 4].mLive & LIVE_CPU_REG_Z))
				{
					if (MoveAddHighByteDown(i))
						changed = true;
				}
			}
		}
#endif

#if 1
		for (int i = 2; i + 1 < mIns.Size(); i++)
		{
			if (mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
				mIns[i + 1].mType == ASMIT_STA && (mIns[i + 1].mMode == ASMIM_ABSOLUTE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE))
			{
				if (MoveLoadImmStoreAbsoluteUp(i + 0))
					changed = true;
			}
		}
#endif

		bool	progress = false;
		do {
			progress = false;

			mIns.Reserve(mIns.Size() * 2 + 32);

			if (RemoveNops())
				changed = true;

			// Replace (a & 0x80) != 0 with bpl/bmi
			int	sz = mIns.Size();
#if 1
			if (sz > 1 &&
				mIns[sz - 2].ChangesAccuAndFlag() &&
				mIns[sz - 1].mType == ASMIT_AND && mIns[sz - 1].mMode == ASMIM_IMMEDIATE && mIns[sz - 1].mAddress == 0x80 && !(mIns[sz - 1].mLive & LIVE_CPU_REG_A))
			{
				if (mBranch == ASMIT_BEQ)
				{
					mBranch = ASMIT_BPL;
					mIns[sz - 2].mLive |= LIVE_CPU_REG_Z;
					mIns[sz - 1].mType = ASMIT_NOP;	mIns[sz - 1].mMode = ASMIM_IMPLIED;
					changed = true;
				}
				else if (mBranch == ASMIT_BNE)
				{
					mBranch = ASMIT_BMI;
					mIns[sz - 2].mLive |= LIVE_CPU_REG_Z;
					mIns[sz - 1].mType = ASMIT_NOP;	mIns[sz - 1].mMode = ASMIM_IMPLIED;
					changed = true;
				}
			}
#endif
			if (sz > 4 &&
				mIns[sz - 4].mType == ASMIT_ASL && mIns[sz - 4].mMode == ASMIM_IMPLIED &&
				mIns[sz - 3].mType == ASMIT_LDA && mIns[sz - 3].mMode == ASMIM_IMMEDIATE && mIns[sz - 3].mAddress == 0 &&
				mIns[sz - 2].mType == ASMIT_ADC && mIns[sz - 2].mMode == ASMIM_IMMEDIATE && mIns[sz - 2].mAddress == 0xff &&
				mIns[sz - 1].mType == ASMIT_EOR && mIns[sz - 1].mMode == ASMIM_IMMEDIATE && mIns[sz - 1].mAddress == 0xff && !(mIns[sz - 1].mLive & LIVE_CPU_REG_A))
			{
				if (mBranch == ASMIT_BEQ)
				{
					mBranch = ASMIT_BPL;
					mIns[sz - 4].mType = ASMIT_NOP; mIns[sz - 4].mMode = ASMIM_IMPLIED;
					mIns[sz - 3].mType = ASMIT_NOP;	mIns[sz - 3].mMode = ASMIM_IMPLIED;
					mIns[sz - 2].mType = ASMIT_NOP;	mIns[sz - 2].mMode = ASMIM_IMPLIED;
					mIns[sz - 1].mType = ASMIT_ORA; mIns[sz - 1].mAddress = 0;
					changed = true;
				}
				else if (mBranch == ASMIT_BNE)
				{
					mBranch = ASMIT_BMI;
					mIns[sz - 4].mType = ASMIT_NOP; mIns[sz - 4].mMode = ASMIM_IMPLIED;
					mIns[sz - 3].mType = ASMIT_NOP; mIns[sz - 3].mMode = ASMIM_IMPLIED;
					mIns[sz - 2].mType = ASMIT_NOP; mIns[sz - 2].mMode = ASMIM_IMPLIED;
					mIns[sz - 1].mType = ASMIT_ORA; mIns[sz - 1].mAddress = 0;
					changed = true;
				}
			}

			for (int i = 0; i < mIns.Size(); i++)
			{
#if 1
#if 1
				if (mIns[i].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0)
				{
					mIns[i].mType = ASMIT_LDA;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0xff && (mIns[i].mLive & LIVE_CPU_REG_Z) == 0)
				{
					mIns[i].mType = ASMIT_NOP; mIns[i].mMode = ASMIM_IMPLIED;;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0xff)
				{
					mIns[i].mType = ASMIT_LDA;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0x00 && (mIns[i].mLive & LIVE_CPU_REG_Z) == 0)
				{
					mIns[i].mType = ASMIT_NOP; mIns[i].mMode = ASMIM_IMPLIED;;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ROR && mIns[i].mMode == ASMIM_IMPLIED && (mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
				{
					mIns[i].mType = ASMIT_LSR;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ROL && mIns[i].mMode == ASMIM_IMPLIED && (mIns[i].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
				{
					mIns[i].mType = ASMIT_ASL;
					progress = true;
				}
#endif
#if 1
				int	apos;
				if (mIns[i].mMode == ASMIM_INDIRECT_Y && FindGlobalAddress(i, mIns[i].mAddress, apos))
				{
					mIns[i].mMode = ASMIM_ABSOLUTE_Y;
					mIns[i].mAddress = mIns[apos].mAddress;
					mIns[i].mLinkerObject = mIns[apos].mLinkerObject;
					progress = true;
				}
#endif

#if 1
				if (mIns[i + 0].mMode == ASMIM_INDIRECT_Y && (mIns[i + 0].mFlags & NCIF_YZERO))
				{
					const NativeCodeInstruction* ains, * iins;

					int	sreg = mIns[i + 0].mAddress;

					int	apos, breg, ireg, addr;
					uint32	flags;

					if (FindAddressSumY(i, sreg, apos, breg, ireg))
					{
#if 1
						if (!(breg == sreg || ireg == sreg) || !(mIns[i + 0].mLive & LIVE_MEM))
						{
							if (breg == sreg || ireg == sreg)
							{
								mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
								mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
							}
							if (mIns[i + 0].mLive & LIVE_CPU_REG_Y)
							{
								mIns.Insert(i + 1, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
								mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
							}
							mIns.Insert(i + 0, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, ireg));
							mIns[i + 0].mLive |= LIVE_CPU_REG_Y | LIVE_MEM;

							mIns[i + 1].mAddress = breg;
							mIns[i + 1].mFlags &= ~NCIF_YZERO;
							progress = true;
						}
#endif

					}
#if 1
					else if (FindGlobalAddressSumY(i, sreg, true, apos, ains, iins, flags, addr))
					{
						if (iins || (flags & LIVE_CPU_REG_Y) || (flags & LIVE_CPU_REG_X)) //!(mIns[i + 1].mLive & LIVE_CPU_REG_X))
						{
							if (mIns[i + 0].mLive & LIVE_CPU_REG_Y)
							{
								mIns.Insert(i + 1, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
								mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
								if (mIns[i + 0].mLive & LIVE_CPU_REG_Z)
								{
									mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_ORA, ASMIM_IMMEDIATE, 0));
									mIns[i + 2].mLive |= LIVE_CPU_REG_Y | LIVE_CPU_REG_Z;
								}
							}

							if (flags & LIVE_CPU_REG_Y)
							{
								mIns[i + 0].mMode = ASMIM_ABSOLUTE_Y;
							}
							else if (flags & LIVE_CPU_REG_X)
							{
								mIns[i + 0].mMode = ASMIM_ABSOLUTE_X;
							}
							else
							{
								mIns[i + 0].mMode = ASMIM_ABSOLUTE_Y;
							}


							if (ains->mMode == ASMIM_IMMEDIATE)
							{
								mIns[i + 0].mLinkerObject = nullptr;
								mIns[i + 0].mAddress = addr;
							}
							else
							{
								mIns[i + 0].mLinkerObject = ains->mLinkerObject;
								mIns[i + 0].mAddress = ains->mAddress;
							}

							mIns[i + 0].mFlags &= ~NCIF_YZERO;

							if (!iins)
							{
								if (flags & LIVE_CPU_REG_Y)
								{
									mIns.Insert(apos, NativeCodeInstruction(ASMIT_TAY, ASMIM_IMPLIED));
									mIns[apos].mLive = LIVE_CPU_REG_Y | LIVE_CPU_REG_A;
									for (int j = apos; j < i + 1; j++)
										mIns[j].mLive |= LIVE_CPU_REG_Y;
								}
								else
								{
									PatchGlobalAdressSumYByX(i + 1, sreg, *ains, addr);
									mIns.Insert(apos, NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
									mIns[apos].mLive = LIVE_CPU_REG_X | LIVE_CPU_REG_A;
									for (int j = apos; j < i + 2; j++)
										mIns[j].mLive |= LIVE_CPU_REG_X;
								}
							}
							else
							{
								if (iins->mMode != ASMIM_ZERO_PAGE)
									mIns.Insert(i + 0, NativeCodeInstruction(ASMIT_LDY, iins->mMode, iins->mAddress, iins->mLinkerObject, iins->mFlags));
								else if (iins->mAddress == sreg)
								{
									if (flags & LIVE_CPU_REG_Y)
									{
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, iins->mAddress));
										mIns[apos].mLive = LIVE_CPU_REG_Y | LIVE_CPU_REG_A | LIVE_MEM;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_Y;
									}
									else
									{
										PatchGlobalAdressSumYByX(i + 1, sreg, *ains, addr);
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, iins->mAddress));
										mIns[apos].mLive = LIVE_CPU_REG_X | LIVE_CPU_REG_A | LIVE_MEM;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_X;
									}
									i++;
								}
								else
									mIns.Insert(i + 0, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, iins->mAddress));
								if (i > 0)
									mIns[i + 0].mLive = mIns[i - 1].mLive | LIVE_CPU_REG_Y | LIVE_MEM;
							}

							progress = true;
						}
					}
#endif
					else if (FindExternAddressSumY(i, sreg, breg, ireg))
					{
#if 1
						if (mIns[i + 0].mLive & LIVE_CPU_REG_Y)
						{
							mIns.Insert(i + 1, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
							mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						}
						mIns.Insert(i + 0, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, ireg));
						mIns[i + 0].mLive |= LIVE_CPU_REG_Y | LIVE_MEM;

						mIns[i + 1].mAddress = breg;
						mIns[i + 1].mFlags &= ~NCIF_YZERO;
						progress = true;
#endif

					}

					if (mIns[i + 0].mMode == ASMIM_INDIRECT_Y && (mIns[i + 0].mFlags & NCIF_YZERO) && !(mIns[i + 0].mLive & LIVE_CPU_REG_X))
					{
						const NativeCodeInstruction* ains;
						if (FindImmediateStore(i, mIns[i].mAddress + 1, ains))
						{
							mIns.Insert(i, NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, mIns[i].mAddress));
							mIns[i + 0].mLive = mIns[i + 1].mLive | LIVE_CPU_REG_X;
							mIns[i + 1].mMode = ASMIM_ABSOLUTE_X;

							if (ains->mMode == ASMIM_IMMEDIATE)
								mIns[i + 1].mAddress = ains->mAddress << 8;
							else
							{
								mIns[i + 1].mLinkerObject = ains->mLinkerObject;
								mIns[i + 1].mAddress = ains->mAddress;
								mIns[i + 1].mFlags |= NCIF_UPPER;
								mIns[i + 1].mFlags &= ~NCIF_LOWER;
							}
							progress = true;
						}
					}


				}
#endif
#if 1
				if (i + 1 < mIns.Size())
				{
					if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_LDA)
					{
						mIns[i].mType = ASMIT_NOP; mIns[i].mMode = ASMIM_IMPLIED;;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_STA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[i + 1].mAddress)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_STA && mIns[i + 1].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[i + 1].mAddress && (mIns[i + 1].mLive & LIVE_CPU_REG_Z) == 0)
					{
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_STA && mIns[i + 1].mType == ASMIT_LDA && mIns[i].SameEffectiveAddress(mIns[i + 1]) && !(mIns[i + 1].mFlags & NCIF_VOLATILE) && (mIns[i + 1].mLive & LIVE_CPU_REG_Z) == 0)
					{
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_AND && mIns[i + 1].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress &= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_ORA && mIns[i + 1].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress |= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_EOR && mIns[i + 1].mType == ASMIT_EOR && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress ^= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_ORA && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_EOR && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_AND && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0xff)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_CLC && mIns[i + 1].mType == ASMIT_ROR)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_LSR;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0 && mIns[i + 1].mType == ASMIT_LSR && mIns[i + 1].mMode == ASMIM_IMPLIED)
					{
						mIns[i + 1].mType = ASMIT_CLC;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_CLC && mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_SEC && mIns[i + 1].mType == ASMIT_SBC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 && !(mIns[i + 1].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].SameEffectiveAddress(mIns[i + 0]))
					{
						mIns[i + 1].mType = ASMIT_ROL;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i + 1].mType == ASMIT_ORA && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 && mIns[i].ChangesAccuAndFlag())
					{
						mIns[i + 0].mLive |= (mIns[i + 1].mLive & LIVE_CPU_REG_Z);
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i + 1].mType == ASMIT_CMP && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 && mIns[i].ChangesAccuAndFlag() && !(mIns[i + 1].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 0].mLive |= (mIns[i + 1].mLive & LIVE_CPU_REG_Z);
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mType == ASMIT_ASL && mIns[i + 1].mMode == ASMIM_IMPLIED)
					{
						int	aval = mIns[i + 0].mAddress << 1;
						mIns[i + 0].mAddress = aval & 0xff;
						if (aval & 0x100)
							mIns[i + 1].mType = ASMIT_SEC;
						else
							mIns[i + 1].mType = ASMIT_CLC;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[i + 1].mAddress &&
						(mIns[i + 1].mType == ASMIT_LSR || mIns[i + 1].mType == ASMIT_ASL || mIns[i + 1].mType == ASMIT_ROL || mIns[i + 1].mType == ASMIT_ROR) && !(mIns[i + 0].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = mIns[i + 1].mType;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_STA;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_ASL && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 1].mAddress && !(mIns[i + 1].mLive & (LIVE_MEM | LIVE_CPU_REG_A)))
					{
						mIns[i + 0].mType = ASMIT_LDA;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_ASL;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_TAY, ASMIM_IMPLIED));
						mIns[i + 2].mLive = mIns[i + 1].mLive;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA &&
						mIns[i + 1].mType == ASMIT_LDY && mIns[i + 0].SameEffectiveAddress(mIns[i + 1]))
					{
						mIns[i + 1].mType = ASMIT_TAY;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_STA &&
						mIns[i + 1].mType == ASMIT_LDX && mIns[i + 0].SameEffectiveAddress(mIns[i + 1]))
					{
						mIns[i + 1].mType = ASMIT_TAX;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						mIns[i + 1].mType == ASMIT_STA && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_STX;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_STA && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_STY;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						mIns[i + 1].mType == ASMIT_STX && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_STA;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						mIns[i + 1].mType == ASMIT_STY && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_STA;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						mIns[i + 1].mType == ASMIT_TAX)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_TAY)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						mIns[i + 1].mType == ASMIT_TXA)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						mIns[i + 1].mType == ASMIT_TYA)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ABSOLUTE_X) &&
						mIns[i + 1].mType == ASMIT_TAY && !(mIns[i + 1].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = ASMIT_LDY; mIns[i + 0].mLive |= mIns[i + 1].mLive;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ABSOLUTE_Y) &&
						mIns[i + 1].mType == ASMIT_TAX && !(mIns[i + 1].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = ASMIT_LDX; mIns[i + 0].mLive |= mIns[i + 1].mLive;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDY && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ABSOLUTE_X) &&
						mIns[i + 1].mType == ASMIT_TYA && !(mIns[i + 1].mLive & LIVE_CPU_REG_Y))
					{
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= mIns[i + 1].mLive;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDX && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ABSOLUTE_Y) &&
						mIns[i + 1].mType == ASMIT_TXA && !(mIns[i + 1].mLive & LIVE_CPU_REG_X))
					{
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= mIns[i + 1].mLive;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						mIns[i + 1].mType == ASMIT_CMP && (mIns[i + 1].mMode == ASMIM_IMMEDIATE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_CPX;
						mIns[i + 0].mLive |= LIVE_CPU_REG_X;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_CMP && (mIns[i + 1].mMode == ASMIM_IMMEDIATE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE))
					{
						mIns[i + 1].mType = ASMIT_CPY;
						mIns[i + 0].mLive |= LIVE_CPU_REG_Y;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDX &&
						mIns[i + 1].mType == ASMIT_STX && !(mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X)))
					{
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_STA;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDY &&
						mIns[i + 1].mType == ASMIT_STY && !(mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y)))
					{
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_STA;
						progress = true;
					}
					else if (
						(mIns[i + 0].mType == ASMIT_LDX || mIns[i + 0].mType == ASMIT_LDY) && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && !mIns[i + 1].SameEffectiveAddress(mIns[i + 0]))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = ins;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INY &&
						mIns[i + 1].mMode == ASMIM_ABSOLUTE_Y && !(mIns[i + 1].mLive & LIVE_CPU_REG_Y))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mAddress++;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INX &&
						mIns[i + 1].mMode == ASMIM_ABSOLUTE_X && !(mIns[i + 1].mLive & LIVE_CPU_REG_X))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mAddress++;
						progress = true;
					}
#endif

#if 1
					else if (
						(mIns[i + 0].mType == ASMIT_ASL || mIns[i + 0].mType == ASMIT_LSR || mIns[i + 0].mType == ASMIT_ROL || mIns[i + 0].mType == ASMIT_ROR) && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 0].mAddress && !(mIns[i + 1].mLive & LIVE_MEM))
					{
						mIns[i + 1].mType = mIns[i + 0].mType;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 0].mType = ASMIT_LDA;
						progress = true;
					}
#endif

#if 1
					else if (mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_INDIRECT_Y)
					{
						const NativeCodeInstruction* ains, *iins;

						int sreg = mIns[i + 1].mAddress;
						int	apos, addr;
						uint32	flags;

						if (FindGlobalAddressSumY(i, sreg, true, apos, ains, iins, flags, addr))
						{
							if (iins || (flags & LIVE_CPU_REG_Y) || (flags & LIVE_CPU_REG_X)) //!(mIns[i + 1].mLive & LIVE_CPU_REG_X))
							{
								if (mIns[i + 1].mLive & LIVE_CPU_REG_Y)
								{
									mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, mIns[i + 0].mAddress));
									mIns[i + 2].mLive |= LIVE_CPU_REG_Y;
									if (mIns[i + 1].mLive & LIVE_CPU_REG_Z)
									{
										mIns.Insert(i + 3, NativeCodeInstruction(ASMIT_ORA, ASMIM_IMMEDIATE, 0));
										mIns[i + 3].mLive |= LIVE_CPU_REG_Y | LIVE_CPU_REG_Z;
									}
								}

								if (flags & LIVE_CPU_REG_Y)
								{
									mIns[i + 1].mMode = ASMIM_ABSOLUTE_Y;
								}
								else if (flags & LIVE_CPU_REG_X)
								{
									mIns[i + 1].mMode = ASMIM_ABSOLUTE_X;
								}
								else
								{
									mIns[i + 1].mMode = ASMIM_ABSOLUTE_Y;
								}

								if (ains->mMode == ASMIM_IMMEDIATE)
								{
									mIns[i + 1].mLinkerObject = 0;
									mIns[i + 1].mAddress = addr;
								}
								else
								{
									mIns[i + 1].mLinkerObject = ains->mLinkerObject;
									mIns[i + 1].mAddress = ains->mAddress + mIns[i + 0].mAddress;
								}

								if (!iins)
								{
									mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
									if (flags & LIVE_CPU_REG_Y)
									{
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_TAY, ASMIM_IMPLIED));
										mIns[apos].mLive = LIVE_CPU_REG_Y | LIVE_CPU_REG_A;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_Y;
									}
									else
									{
										PatchGlobalAdressSumYByX(i + 1, sreg, *ains, addr);
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
										mIns[apos].mLive = LIVE_CPU_REG_X | LIVE_CPU_REG_A;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_X;
									}
								}
								else if (iins->mMode != ASMIM_ZERO_PAGE)
								{
									mIns[i + 0].mMode = iins->mMode;
									mIns[i + 0].mAddress = iins->mAddress;
									mIns[i + 0].mLinkerObject = iins->mLinkerObject;
									mIns[i + 0].mFlags = iins->mFlags;

									if (!(flags & LIVE_CPU_REG_Y) && (flags & LIVE_CPU_REG_X))
										mIns[i + 0].mType == ASMIT_LDX;
								}
								else if (iins->mAddress == sreg)
								{
									mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
									if (flags & LIVE_CPU_REG_Y)
									{
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, iins->mAddress));
										mIns[apos].mLive = mIns[apos + 1].mLive | LIVE_CPU_REG_Y | LIVE_CPU_REG_A | LIVE_MEM;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_Y;
									}
									else
									{
										PatchGlobalAdressSumYByX(i + 1, sreg, *ains, addr);
										mIns.Insert(apos, NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, iins->mAddress));
										mIns[apos].mLive = mIns[apos + 1].mLive | LIVE_CPU_REG_X | LIVE_CPU_REG_A | LIVE_MEM;
										for (int j = apos; j < i + 2; j++)
											mIns[j].mLive |= LIVE_CPU_REG_X;
									}
								}
								else
								{
									mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
									mIns[i + 0].mAddress = iins->mAddress;

									if (!(flags & LIVE_CPU_REG_Y) && (flags & LIVE_CPU_REG_X))
										mIns[i + 0].mType == ASMIT_LDX;
								}

								progress = true;
							}
						}
					}
#endif
#if 1
					if (mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mMode == ASMIM_INDIRECT_Y)
					{
						int	addr;
						if (FindPageStartAddress(i, mIns[i + 1].mAddress, addr))
						{
							if (mIns[i + 1].mLive & LIVE_CPU_REG_Y)
								mIns.Insert(i + 2, mIns[i + 0]);

							int	absaddr = addr + mIns[i + 0].mAddress;

							mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
							mIns[i + 0].mLive |= LIVE_MEM;
							mIns[i + 0].mAddress = mIns[i + 1].mAddress;

							mIns[i + 1].mMode = ASMIM_ABSOLUTE_Y;
							mIns[i + 1].mAddress = absaddr;
							mIns[i + 1].mLinkerObject = nullptr;

							progress = true;
						}
					}
#endif

#if 1
					if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mMode == ASMIM_INDIRECT_Y)
					{
						int	sreg = mIns[i + 1].mAddress;

						int	apos, breg, ireg;

						if (FindAddressSumY(i, sreg, apos, breg, ireg))
						{
							if (PatchAddressSumY(i, sreg, apos, breg, ireg))
								progress = true;
						}
					}
#endif

#if 1
					if (
						mIns[i + 0].mMode == ASMIM_INDIRECT_Y && (mIns[i + 0].mFlags & NCIF_YZERO) &&
						mIns[i + 1].mMode == ASMIM_INDIRECT_Y && mIns[i + 0].mAddress == mIns[i + 1].mAddress)
					{
						const NativeCodeInstruction* ains, * iins;

						int	sreg = mIns[i + 0].mAddress;

						int	apos, breg, ireg;
						uint32	flags;

						if (FindAddressSumY(i, sreg, apos, breg, ireg))
						{
							if (!(breg == sreg || ireg == sreg) || !(mIns[i + 1].mLive & LIVE_MEM))
							{
								if (breg == sreg || ireg == sreg)
								{
									mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
									mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
								}
								if (mIns[i + 1].mLive & LIVE_CPU_REG_Y)
								{
									mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
									mIns[i + 2].mLive |= LIVE_CPU_REG_Y;
								}
								mIns.Insert(i + 0, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, ireg));
								mIns[i + 0].mLive |= LIVE_CPU_REG_Y | LIVE_MEM;
								mIns[i + 1].mAddress = breg; mIns[i + 1].mFlags &= ~NCIF_YZERO;
								mIns[i + 2].mAddress = breg; mIns[i + 2].mFlags &= ~NCIF_YZERO;
								progress = true;
							}
						}
					}
#endif

#if 1
					if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress <= 3 &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_INDIRECT_Y)
					{
						int	apos, breg, ireg;
						if (FindAddressSumY(i, mIns[i + 1].mAddress, apos, breg, ireg))
						{
							if (breg != mIns[i + 1].mAddress && ireg != mIns[i + 1].mAddress)// || !(mIns[i + 1].mLive & LIVE_MEM))
							{
								int yoffset = mIns[i + 0].mAddress;

								if (breg == mIns[i + 1].mAddress)
								{
									mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
									mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
								}
								if (mIns[i + 1].mLive & LIVE_CPU_REG_Y)
								{
									mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, yoffset));
									mIns[i + 2].mLive |= LIVE_CPU_REG_Y;
								}

								mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
								mIns[i + 0].mAddress = ireg;
								mIns[i + 0].mLive |= LIVE_MEM;
								mIns[i + 1].mAddress = breg;

								for(int j=0; j<yoffset; j++)
								{
									mIns.Insert(i + 1, NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
									mIns[i + 1].mLive = mIns[i + 0].mLive;
								}

								progress = true;
							}
						}
					}

#endif

				}

#endif
#if 1
				if (i + 2 < mIns.Size())
				{
					if (mIns[i].mType == ASMIT_LDA && mIns[i + 2].mType == ASMIT_LDA && (mIns[i + 1].mType == ASMIT_CLC || mIns[i + 1].mType == ASMIT_SEC))
					{
						mIns[i].mType = ASMIT_NOP; mIns[i].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 2].mAddress &&
						mIns[i + 1].mType == ASMIT_INC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 1].mAddress && (mIns[i + 2].mLive & LIVE_CPU_REG_C) == 0)
					{
						mIns[i + 0].mType = ASMIT_CLC;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_ADC;
						mIns[i + 1].mMode = ASMIM_IMMEDIATE;
						mIns[i + 1].mAddress = 1;
						mIns[i + 2].mType = ASMIT_STA;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode != ASMIM_ZERO_PAGE &&
						mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 2].mAddress &&
						mIns[i + 2].IsCommutative() && HasAsmInstructionMode(mIns[i + 2].mType, mIns[i + 1].mMode) &&
						(mIns[i + 2].mLive & LIVE_MEM) == 0)
					{
						mIns[i + 1].mType = mIns[i + 2].mType;
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].ChangesAccuAndFlag() &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 2].mType == ASMIT_LDA &&
						mIns[i + 1].SameEffectiveAddress(mIns[i + 2]))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_LDY && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
						!mIns[i + 1].SameEffectiveAddress(mIns[i + 0]) && !mIns[i + 1].ChangesAccu())
					{
						mIns[i + 2].mType = ASMIT_TAY;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_TAY && !(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) &&
						!mIns[i + 1].ChangesAccu() && !mIns[i + 1].RequiresAccu() && !(mIns[i + 1].mLive & LIVE_CPU_REG_Y))
					{
						mIns[i + 0].mType = ASMIT_LDY;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
						mIns[i + 1].mType == ASMIT_DEC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 0].mAddress &&
						!(mIns[i + 2].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 0].mType = ASMIT_SEC; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_SBC; mIns[i + 1].mMode = ASMIM_IMMEDIATE; mIns[i + 1].mAddress = 1;
						mIns[i + 2].mType = ASMIT_STA;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mType == ASMIT_CMP && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_ROR && mIns[i + 2].mMode == ASMIM_IMPLIED)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_CLC; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].ChangesAccuAndFlag() &&
						mIns[i + 1].mType == ASMIT_STA &&
						mIns[i + 2].mType == ASMIT_ORA && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0)
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mLive |= mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z);
						progress = true;
					}
					else if (
						mIns[i + 0].ChangesAccuAndFlag() &&
						mIns[i + 1].mType == ASMIT_STA &&
						mIns[i + 2].mType == ASMIT_CMP && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0 && !(mIns[i + 2].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z);
						mIns[i + 1].mLive |= mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z);
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 1].IsShift() &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 2].mType = mIns[i + 1].mType;
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].UsesZeroPage(mIns[i + 0].mAddress) && !mIns[i + 1].UsesAccu() &&
						mIns[i + 2].IsShift() && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mType = ASMIT_STA;
						mIns[i + 2].mLive |= mIns[i + 1].mLive & LIVE_CPU_REG_C;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].IsShift() && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress)
					{
						mIns[i + 0] = mIns[i + 2];
						mIns[i + 2] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 0];
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_STA;
						mIns[i + 2].mLive |= mIns[i + 1].mLive & LIVE_CPU_REG_C;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mMode != ASMIM_RELATIVE &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 2].mLive & LIVE_CPU_REG_A) &&
						mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 2].mAddress &&
						(mIns[i + 1].mType == ASMIT_DEC || mIns[i + 1].mType == ASMIT_INC))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mLive |= LIVE_CPU_REG_Z;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						!mIns[i + 1].ChangesXReg() &&
						mIns[i + 2].mType == ASMIT_TXA && !(mIns[i + 2].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						!mIns[i + 1].ChangesXReg() && !mIns[i + 1].UsesAccu() &&
						mIns[i + 2].mType == ASMIT_STX && !(mIns[i + 2].mLive & LIVE_CPU_REG_X))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_STA;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						!mIns[i + 1].ChangesYReg() && !mIns[i + 1].UsesAccu() &&
						mIns[i + 2].mType == ASMIT_STY && !(mIns[i + 2].mLive & LIVE_CPU_REG_Y))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_STA;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_SEC &&
						mIns[i + 1].mType == ASMIT_TXA &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mAddress == 1 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_X)))
					{
						mIns[i + 0].mType = ASMIT_DEX;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_SEC &&
						mIns[i + 1].mType == ASMIT_TYA &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mAddress == 1 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Y)))
					{
						mIns[i + 0].mType = ASMIT_DEY;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_SEC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE)
					{
						int	t = (mIns[i + 2].mAddress ^ 0xff) + mIns[i + 1].mAddress + 1;

						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_LDA; mIns[i + 2].mAddress = t & 0xff;
						if (t < 256)
							mIns[i + 0].mType = ASMIT_CLC;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE)
					{
						int	t = mIns[i + 2].mAddress + mIns[i + 1].mAddress;

						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_LDA; mIns[i + 2].mAddress = t & 0xff;
						if (t >= 256)
							mIns[i + 0].mType = ASMIT_SEC;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
						mIns[i + 2].mType == ASMIT_ADC)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_LDA;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0)
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_IMMEDIATE || mIns[i + 0].mMode == ASMIM_IMMEDIATE_ADDRESS || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC)
					{
						mIns[i + 1] = mIns[i + 0];
						mIns[i + 0].mType = ASMIT_CLC;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_C;
						mIns[i + 1].mLive |= LIVE_CPU_REG_C;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0 &&
						mIns[i + 2].mType == ASMIT_LSR && mIns[i + 2].mMode == ASMIM_IMPLIED)
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_EOR && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0x80 &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0x80 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}

#if 1
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_TYA &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Y)))
					{
						int t = mIns[i + 2].mAddress;
						mIns[i + 0].mType = ASMIT_INY;
						if (t > 1)
						{
							mIns[i + 1].mType = ASMIT_INY;
							mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						}
						else
						{
							mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						}
						mIns[i + 2].mType = ASMIT_TYA; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Y)))
					{
						int t = mIns[i + 2].mAddress;
						mIns[i + 0].mType = ASMIT_INY;
						if (t > 1)
						{
							mIns[i + 1].mType = ASMIT_INY;
							mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						}
						else
						{
							mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						}
						mIns[i + 2].mType = ASMIT_TYA; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_ADC && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && !(mIns[i + 2].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 0].mAddress += mIns[i + 2].mAddress;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_SBC && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && !(mIns[i + 2].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 0].mAddress += mIns[i + 2].mAddress;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && !(mIns[i + 0].mFlags & NCIF_VOLATILE) &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 2]) &&
						!mIns[i + 1].ChangesAddress() && !mIns[i + 1].ChangesGlobalMemory() &&
						!mIns[i + 1].ChangesYReg() && !mIns[i + 1].ChangesXReg())
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_LDX &&
						mIns[i + 2].mType == ASMIT_STX && !(mIns[i + 2].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 2] = ins;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_LDY &&
						mIns[i + 2].mType == ASMIT_STY && !(mIns[i + 2].mLive & (LIVE_CPU_REG_Y | LIVE_CPU_REG_Z)))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 2] = ins;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDX &&
						mIns[i + 2].mType == ASMIT_STX && !(mIns[i + 2].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_Z)) && !mIns[i + 2].SameEffectiveAddress(mIns[i + 0]))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 2] = ins;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDY &&
						mIns[i + 2].mType == ASMIT_STY && !(mIns[i + 2].mLive & (LIVE_CPU_REG_Y | LIVE_CPU_REG_Z)) && !mIns[i + 2].SameEffectiveAddress(mIns[i + 0]))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 2] = ins;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INC && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].ChangesZeroPage(mIns[i + 0].mAddress) && !mIns[i + 1].RequiresYReg() &&
						mIns[i + 2].mType == ASMIT_LDY && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) && !(mIns[i + 2].mLive & LIVE_MEM))
					{
						mIns[i + 0] = mIns[i + 2];
						mIns[i + 2].mType = ASMIT_INY; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].ChangesZeroPage(mIns[i + 0].mAddress) && !mIns[i + 1].RequiresYReg() &&
						mIns[i + 2].mType == ASMIT_LDY && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) && !(mIns[i + 2].mLive & LIVE_MEM))
					{
						mIns[i + 0].mType = ASMIT_TAY; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif

#endif
					else if (
						mIns[i + 0].mType == ASMIT_ASL && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_ROL && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress != mIns[i + 0].mAddress &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress && !(mIns[i + 2].mLive & (LIVE_MEM | LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mType = ASMIT_ROL; mIns[i + 2].mAddress = mIns[i + 1].mAddress; mIns[i + 2].mLive |= LIVE_MEM;
						mIns[i + 1].mType = ASMIT_ASL; mIns[i + 1].mMode = ASMIM_IMPLIED; mIns[i + 1].mLive |= LIVE_CPU_REG_A | LIVE_CPU_REG_C;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && 
						mIns[i + 1].mType == ASMIT_ROR && mIns[i + 1].mMode == ASMIM_IMPLIED &&
						mIns[i + 2].mType == ASMIT_AND && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0x80 && !(mIns[i + 2].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mMode = ASMIM_IMMEDIATE; mIns[i + 0].mAddress = 0;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_ASL && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_ASL && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 0].mAddress &&
						mIns[i + 2].mType == ASMIT_ASL && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
						!(mIns[i + 2].mLive & LIVE_CPU_REG_A))
					{
						int addr = mIns[i + 0].mAddress;

						mIns.Insert(i, NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, addr));
						mIns.Insert(i + 4, NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, addr));

						mIns[i + 0].mLive = mIns[i + 1].mLive | LIVE_CPU_REG_A;
						mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mLive |= LIVE_CPU_REG_A;
						mIns[i + 3].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mLive |= LIVE_CPU_REG_A;
						mIns[i + 4].mLive = mIns[i + 3].mLive;

						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						!mIns[i + 1].ChangesYReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_TAY && !(mIns[i + 2].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						!mIns[i + 1].ChangesXReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_TAX && !(mIns[i + 2].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						!mIns[i + 1].ChangesYReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_TYA && !(mIns[i + 2].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						!mIns[i + 1].ChangesXReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_TXA && !(mIns[i + 2].mLive & LIVE_CPU_REG_Z))
					{
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}

					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						!mIns[i + 1].ChangesYReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_STY && !(mIns[i + 2].mLive & LIVE_CPU_REG_Y))
					{
						mIns[i + 2].mType = ASMIT_STA; 
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						!mIns[i + 1].ChangesXReg() && !mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_STX && !(mIns[i + 2].mLive & LIVE_CPU_REG_X))
					{
						mIns[i + 2].mType = ASMIT_STA; 
						progress = true;
					}

#endif
					else if (
						mIns[i + 0].mType == ASMIT_TAX &&
						mIns[i + 1].mType == ASMIT_TAY &&
						mIns[i + 2].mMode == ASMIM_ABSOLUTE_Y && (mIns[i + 2].mLive & LIVE_CPU_REG_X) && !(mIns[i + 2].mLive & LIVE_CPU_REG_Y) && HasAsmInstructionMode(mIns[i + 2].mType, ASMIM_ABSOLUTE_X))
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mMode = ASMIM_ABSOLUTE_X;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TAY &&
						mIns[i + 1].mType == ASMIT_TAX &&
						mIns[i + 2].mMode == ASMIM_ABSOLUTE_Y && (mIns[i + 2].mLive & LIVE_CPU_REG_X) && !(mIns[i + 2].mLive & LIVE_CPU_REG_Y) && HasAsmInstructionMode(mIns[i + 2].mType, ASMIM_ABSOLUTE_X))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mMode = ASMIM_ABSOLUTE_X;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 1].mType == ASMIT_STA && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 2].mType == ASMIT_LDY && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = ASMIT_LDY; mIns[i + 0].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 1].mType = ASMIT_STY; mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA && (mIns[i + 0].mMode == ASMIM_ZERO_PAGE || mIns[i + 0].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 1].mType == ASMIT_STA && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE || mIns[i + 1].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 2].mType == ASMIT_LDX && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]) && !(mIns[i + 2].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = ASMIT_LDX; mIns[i + 0].mLive |= LIVE_CPU_REG_X;
						mIns[i + 1].mType = ASMIT_STX; mIns[i + 1].mLive |= LIVE_CPU_REG_X;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 2].mLive & LIVE_CPU_REG_Y) &&
						!mIns[i + 1].UsesZeroPage(mIns[i + 0].mAddress) &&
						mIns[i + 2].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress && !(mIns[i + 2].mLive & (LIVE_MEM | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mType = ASMIT_TAY; mIns[i + 0].mMode = ASMIM_IMPLIED; mIns[i + 0].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 2].mLive & LIVE_CPU_REG_X) &&
						!mIns[i + 1].UsesZeroPage(mIns[i + 0].mAddress) &&
						mIns[i + 2].mType == ASMIT_LDX && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress && !(mIns[i + 2].mLive & (LIVE_MEM | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mType = ASMIT_TAX; mIns[i + 0].mMode = ASMIM_IMPLIED; mIns[i + 0].mLive |= LIVE_CPU_REG_X;
						mIns[i + 1].mLive |= LIVE_CPU_REG_X;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INC && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 1].mAddress && !(mIns[i + 1].mLive & LIVE_MEM) &&
						mIns[i + 2].mType == ASMIT_STA && (mIns[i + 2].mMode == ASMIM_ZERO_PAGE || mIns[i + 2].mMode == ASMIM_ABSOLUTE) && !(mIns[i + 2].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y)))
					{
						mIns[i + 0].mType = ASMIT_LDY; mIns[i + 0].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 1].mType = ASMIT_INY; mIns[i + 1].mMode = ASMIM_IMPLIED; mIns[i + 1].mLive |= LIVE_CPU_REG_Y;
						mIns[i + 2].mType = ASMIT_STY; 
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && !mIns[i + 1].SameEffectiveAddress(mIns[i + 0]) &&
						mIns[i + 2].IsShift() && mIns[i + 2].SameEffectiveAddress(mIns[i + 0]))
					{
						AsmInsType	type = mIns[i + 2].mType;

						mIns[i + 2] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 0];

						mIns[i + 0].mType = type;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && 
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 1].mLive & LIVE_CPU_REG_A) &&
						mIns[i + 2].mType == ASMIT_CPX && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress == mIns[i + 2].mAddress && !(mIns[i + 2].mLive & LIVE_MEM))
					{
						mIns[i + 1] = mIns[i + 0];
						mIns[i + 1].mType = ASMIT_CMP;
						mIns[i + 1].mLive |= mIns[i + 2].mLive;
						mIns[i + 0].mType = ASMIT_TXA; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif

					else if (
						mIns[i + 0].mType == ASMIT_LDA && !mIns[i + 0].RequiresXReg() &&
						mIns[i + 1].mType == ASMIT_LDX && 
						mIns[i + 2].mType == ASMIT_STX && !(mIns[i + 2].mLive & LIVE_CPU_REG_X) && !mIns[i + 0].MayBeChangedOnAddress(mIns[i + 2]))
					{
						NativeCodeInstruction	ins = mIns[i + 0];
						mIns[i + 0] = mIns[i + 1];
						mIns[i + 1] = mIns[i + 2];
						mIns[i + 2] = ins;
						mIns[i + 0].mType = ASMIT_LDA; mIns[i + 0].mLive |= LIVE_CPU_REG_A | mIns[i + 2].mLive;
						mIns[i + 1].mType = ASMIT_STA; mIns[i + 1].mLive |= mIns[i + 2].mLive;
						progress = true;
					}

#if 1
					if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress <= 1 &&
						mIns[i + 1].mType == ASMIT_LDA &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y && !(mIns[i + 2].mLive & LIVE_MEM))
					{
						int	apos, breg, ireg;
						if (FindAddressSumY(i, mIns[i + 2].mAddress, apos, breg, ireg))
						{
							int yoffset = mIns[i + 0].mAddress;

							if (breg == mIns[i + 2].mAddress || ireg == mIns[i + 2].mAddress)
							{
								mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
								mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
							}
							if (mIns[i + 2].mLive & LIVE_CPU_REG_Y)
							{
								mIns.Insert(i + 3, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, yoffset));
								mIns[i + 3].mLive |= LIVE_CPU_REG_Y;
							}
							
							int ypos = i;
							if (mIns[i + 1].mMode != ASMIM_INDIRECT_Y && mIns[i + 1].mMode != ASMIM_ABSOLUTE_Y)
							{
								mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
								mIns[i + 0].mAddress = ireg;
								mIns[i + 0].mLive |= LIVE_MEM;
								mIns[i + 2].mAddress = breg;
							}
							else
							{
								mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, ireg));
								mIns[i + 2].mLive = mIns[i + 3].mLive | LIVE_CPU_REG_Y | LIVE_MEM;
								ypos = i + 2;
								mIns[i + 3].mAddress = breg;
							}

							if (yoffset == 1)
							{
								mIns.Insert(ypos + 1, NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
								mIns[ypos + 1].mLive = mIns[ypos].mLive;
							}

							progress = true;
						}
					}

					if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_LDA &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y && !(mIns[i + 2].mLive & LIVE_CPU_REG_X))
					{
						const NativeCodeInstruction* ains;
						if (FindImmediateStore(i, mIns[i + 2].mAddress + 1, ains))
						{
							mIns.Insert(i + 2, NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, mIns[i + 2].mAddress));
							mIns[i + 2].mLive = mIns[i + 3].mLive | LIVE_CPU_REG_X;
							mIns[i + 3].mMode = ASMIM_ABSOLUTE_X;

							if (ains->mMode == ASMIM_IMMEDIATE)
								mIns[i + 3].mAddress = (ains->mAddress << 8) + mIns[i + 0].mAddress;
							else
							{
								mIns[i + 3].mLinkerObject = ains->mLinkerObject;
								mIns[i + 3].mAddress = mIns[i + 0].mAddress + ains->mAddress;
								mIns[i + 3].mFlags |= NCIF_UPPER;
								mIns[i + 3].mFlags &= ~NCIF_LOWER;
							}
							progress = true;
						}
					}

#endif
				}

				if (i + 3 < mIns.Size())
				{
					if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 1].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_INC;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 1].mType == ASMIT_SEC && mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_DEC;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 1].mType == ASMIT_SEC && mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & LIVE_CPU_REG_C) == 0)
					{
						mIns[i + 0].mType = ASMIT_DEC;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_LDA;
						progress = true;
					}
					else if (mIns[i + 1].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 1].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 0].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 1].mMode == ASMIM_ABSOLUTE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_INC;
						progress = true;
					}
					else if (mIns[i + 1].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 1].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 0].mType == ASMIT_SEC && mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 1].mMode == ASMIM_ABSOLUTE || mIns[i + 1].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_DEC;
						progress = true;
					}
					else if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 1].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && (mIns[i + 2].mAddress & 0xff) == 0xff &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_DEC;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress != mIns[i + 1].mAddress &&
						mIns[i + 3].mType == ASMIT_CMP && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress && !(mIns[i + 3].mLive & LIVE_MEM))
					{
						mIns[i + 3].CopyMode(mIns[i + 0]);

						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_LDX && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
						mIns[i + 3].mType == ASMIT_STX && mIns[i + 3].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].ChangesZeroPage(mIns[i + 0].mAddress) && !mIns[i + 1].UsesZeroPage(mIns[i + 3].mAddress))
					{
						mIns.Insert(i + 1, NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, mIns[i + 3].mAddress));

						mIns[i + 4].mType = ASMIT_NOP; mIns[i + 4].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y &&
						mIns[i + 3].mType == ASMIT_EOR && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 0].mAddress)
					{
						mIns[i + 2].mType = mIns[i + 3].mType;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && 
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && !mIns[i + 0].MayBeChangedOnAddress(mIns[i + 1]) &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_ORA && mIns[i + 3].SameEffectiveAddress(mIns[i + 0]))
					{
						mIns[i + 2].mType = ASMIT_ORA;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && 
						mIns[i + 1].mType == ASMIT_EOR && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0xff &&
						mIns[i + 2].mType == ASMIT_SEC &&
						mIns[i + 3].mType == ASMIT_ADC)
					{
						mIns.Insert(i + 4, mIns[i + 0]);

						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_LDA; mIns[i + 3].mLive |= LIVE_CPU_REG_C;
						mIns[i + 4].mType = ASMIT_SBC;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress != mIns[i + 1].mAddress &&
						mIns[i + 2].mType == ASMIT_SEC &&
						mIns[i + 3].mType == ASMIT_SBC && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 3].mAddress && !(mIns[i + 3].mLive & LIVE_MEM))
					{
						mIns[i + 0].mType = ASMIT_EOR;
						mIns[i + 0].mMode = ASMIM_IMMEDIATE;
						mIns[i + 0].mAddress = 0xff;

						mIns[i + 3].mType = ASMIT_ADC;
						mIns[i + 3].mAddress = mIns[i + 1].mAddress;

						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_LDA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress != mIns[i + 2].mAddress &&
						mIns[i + 3].mType == ASMIT_SBC && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 3].mAddress && !(mIns[i + 3].mLive & LIVE_MEM))
					{
						mIns[i + 0].mType = ASMIT_EOR;
						mIns[i + 0].mMode = ASMIM_IMMEDIATE;
						mIns[i + 0].mAddress = 0xff;

						mIns[i + 3].mType = ASMIT_ADC;
						mIns[i + 3].mAddress = mIns[i + 2].mAddress;

						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDA && (mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress != mIns[i + 2].mAddress || mIns[i + 1].mMode == ASMIM_ABSOLUTE) &&
						mIns[i + 2].mType == ASMIT_CLC &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 3].mAddress && !(mIns[i + 3].mLive & LIVE_MEM))
					{
						mIns[i + 3] = mIns[i + 1];
						mIns[i + 3].mType = ASMIT_ADC;

						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 1].mType == ASMIT_ADC && 
						mIns[i + 2].mType == ASMIT_CLC &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_IMMEDIATE && !(mIns[i + 3].mLive & LIVE_CPU_REG_C))
					{
						mIns[i + 0].mAddress += mIns[i + 3].mAddress;

						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0x00 &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0xff &&
						mIns[i + 2].mType == ASMIT_AND && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0x01 &&
						mIns[i + 3].mType == ASMIT_EOR && mIns[i + 3].mMode == ASMIM_IMMEDIATE && mIns[i + 3].mAddress == 0x01)
					{
						mIns[i + 1].mAddress = 0x00;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_LDA &&
						mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						mIns[i + 1].mType == ASMIT_ASL && mIns[i + 1].mMode == ASMIM_IMPLIED &&
						mIns[i + 2].mType == ASMIT_ORA && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 && !(mIns[i + 3].mLive & LIVE_CPU_REG_A))

					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_SEC;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mLive |= LIVE_CPU_REG_C;
						mIns[i + 3].mType = ASMIT_ROL;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_LDA &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mAddress && mIns[i + 3].mAddress &&
						mIns[i + 2].mMode == ASMIM_IMPLIED && !(mIns[i + 3].mLive & LIVE_MEM) &&
						(mIns[i + 2].mType == ASMIT_ASL || mIns[i + 2].mType == ASMIT_LSR || mIns[i + 2].mType == ASMIT_ROL || mIns[i + 2].mType == ASMIT_ROR) &&
						(mIns[i + 3].mType == ASMIT_ORA || mIns[i + 3].mType == ASMIT_AND || mIns[i + 3].mType == ASMIT_EOR || mIns[i + 3].mType == ASMIT_ADC || mIns[i + 3].mType == ASMIT_SBC))
					{
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mMode = mIns[i + 0].mMode;
						mIns[i + 3].mAddress = mIns[i + 0].mAddress;
						mIns[i + 3].mLinkerObject = mIns[i + 0].mLinkerObject;
						mIns[i + 3].mFlags = mIns[i + 0].mFlags;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mAddress == 0xff &&
						mIns[i + 2].mType == ASMIT_EOR && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 0xff &&
						mIns[i + 3].mType == ASMIT_LSR && mIns[i + 3].mMode == ASMIM_IMPLIED && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_TAY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_LDY;
						mIns[i + 2].mType = ASMIT_INY; mIns[i + 2].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 3].mType = ASMIT_INY;
						else
							mIns[i + 3].mType = ASMIT_NOP;
						mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_SEC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_TAY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_LDY;
						mIns[i + 2].mType = ASMIT_DEY; mIns[i + 2].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 3].mType = ASMIT_DEY;
						else
							mIns[i + 3].mType = ASMIT_NOP;
						mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_TAY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_INY; mIns[i + 2].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 3].mType = ASMIT_INY;
						else
							mIns[i + 3].mType = ASMIT_NOP;
						mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_TAY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_DEY; mIns[i + 2].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 3].mType = ASMIT_DEY;
						else
							mIns[i + 3].mType = ASMIT_NOP;
						mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_STA && (mIns[i + 3].mMode == ASMIM_ZERO_PAGE || mIns[i + 3].mMode == ASMIM_ABSOLUTE) &&
						!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_INX; mIns[i + 1].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 2].mType = ASMIT_INX;
						else
							mIns[i + 2].mType = ASMIT_NOP;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_STX;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TXA &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_STA && (mIns[i + 3].mMode == ASMIM_ZERO_PAGE || mIns[i + 3].mMode == ASMIM_ABSOLUTE) &&
						!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_X | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_DEX; mIns[i + 1].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 2].mType = ASMIT_DEX;
						else
							mIns[i + 2].mType = ASMIT_NOP;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_STX;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_STA && (mIns[i + 3].mMode == ASMIM_ZERO_PAGE || mIns[i + 3].mMode == ASMIM_ABSOLUTE) &&
						!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_INY; mIns[i + 1].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 2].mType = ASMIT_INY;
						else
							mIns[i + 2].mType = ASMIT_NOP;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_STY;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_TYA &&
						mIns[i + 1].mType == ASMIT_SEC &&
						mIns[i + 2].mType == ASMIT_SBC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress <= 2 &&
						mIns[i + 3].mType == ASMIT_STA && (mIns[i + 3].mMode == ASMIM_ZERO_PAGE || mIns[i + 3].mMode == ASMIM_ABSOLUTE) &&
						!(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_DEY; mIns[i + 1].mMode = ASMIM_IMPLIED;
						if (mIns[i + 2].mAddress == 2)
							mIns[i + 2].mType = ASMIT_DEY;
						else
							mIns[i + 2].mType = ASMIT_NOP;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_STY;
						progress = true;
					}

					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].IsShift() && mIns[i + 1].mMode == ASMIM_IMPLIED &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress == mIns[i + 0].mAddress &&
						mIns[i + 3].mType == ASMIT_ORA && mIns[i + 3].mMode == ASMIM_IMMEDIATE && mIns[i + 3].mAddress == 0x00 && !(mIns[i + 3].mLive & LIVE_CPU_REG_A))
					{
						mIns[i + 0].mType = mIns[i + 1].mType;
						mIns[i + 0].mLive |= LIVE_MEM | LIVE_CPU_REG_C | LIVE_CPU_REG_Z;

						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;

						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!(mIns[i + 1].ChangesYReg() || mIns[i + 1].mMode == ASMIM_INDIRECT_Y || mIns[i + 1].RequiresXReg()) &&
						mIns[i + 2].mType == ASMIT_TYA &&
						mIns[i + 3].mType == ASMIT_TAX && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z | LIVE_CPU_REG_Y)))
					{
						mIns[i + 0].mType = ASMIT_LDX;
						mIns[i + 0].mLive |= LIVE_CPU_REG_X;
						mIns[i + 1].ReplaceYRegWithXReg();
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 3].mType = ASMIT_NOP; mIns[i + 3].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#endif
#if 1
					else if (
						mIns[i + 0].IsShift() && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_CLC &&
						mIns[i + 2].mType == ASMIT_LDA && (mIns[i + 2].mMode == ASMIM_IMMEDIATE || mIns[i + 2].mMode == ASMIM_IMMEDIATE_ADDRESS) &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 0].mAddress &&
						!(mIns[i + 3].mLive & LIVE_MEM))
					{
						mIns[i + 3] = mIns[i + 2];
						mIns[i + 2] = mIns[i + 1];
						mIns[i + 1].mType = mIns[i + 0].mType;
						mIns[i + 0].mType = ASMIT_LDA;
						mIns[i + 3].mType = ASMIT_ADC;

						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mLive |= LIVE_CPU_REG_A;
						progress = true;
					}
#endif
#if 1
					if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_INDIRECT_Y &&
						!mIns[i + 2].ChangesYReg() && (mIns[i + 2].mMode == ASMIM_IMMEDIATE || mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress != mIns[i + 1].mAddress) &&
						mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_INDIRECT_Y && mIns[i + 1].mAddress == mIns[i + 3].mAddress && !(mIns[i + 3].mLive & LIVE_MEM))
					{
						int	apos, breg, ireg;
						if (FindAddressSumY(i, mIns[i + 1].mAddress, apos, breg, ireg))
						{
							if (breg == mIns[i + 1].mAddress || ireg == mIns[i + 1].mAddress)
							{
								mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
								mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
							}
							if (mIns[i + 3].mLive & LIVE_CPU_REG_Y)
							{
								mIns.Insert(i + 4, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
								mIns[i + 4].mLive |= LIVE_CPU_REG_Y;
							}
							mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
							mIns[i + 0].mAddress = ireg;
							mIns[i + 0].mLive |= LIVE_MEM;
							mIns[i + 1].mAddress = breg;
							mIns[i + 3].mAddress = breg;
							progress = true;
						}
					}

#endif

				}

				if (i + 4 < mIns.Size())
				{
					if (mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 0].mAddress == mIns[i + 3].mAddress &&
						mIns[i + 1].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_LDA)
					{
						// Flip arguments of ADC if second parameter in accu at entry

						mIns[i + 3] = mIns[i + 2];
						mIns[i + 3].mType = ASMIT_ADC;
						mIns[i + 2].mMode = ASMIM_ZERO_PAGE;
						mIns[i + 2].mAddress = mIns[i + 0].mAddress;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && (mIns[i + 2].mAddress == 1 || mIns[i + 2].mAddress == 2) &&
						mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && 
						mIns[i + 4].mType == ASMIT_TAY && !(mIns[i + 4].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP;
						mIns[i + 1].mType = ASMIT_LDY; mIns[i + 1].mFlags |= LIVE_CPU_REG_Y;
						mIns[i + 2].mType = ASMIT_INY; mIns[i + 2].mMode = ASMIM_IMPLIED; mIns[i + 2].mFlags |= LIVE_CPU_REG_Y;
						mIns[i + 3].mType = ASMIT_STY; mIns[i + 3].mFlags |= LIVE_CPU_REG_Y;
						mIns[i + 4].mType = ASMIT_NOP;
						if (mIns[i + 2].mAddress == 2)
						{
							mIns.Insert(i + 3, mIns[i + 2]);
						}

						progress = true;
					}
#endif
					else if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode != ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_SEC &&
						mIns[i + 3].mType == ASMIT_LDA && !mIns[i + 3].UsesZeroPage(mIns[i + 1].mAddress) &&
						mIns[i + 4].mType == ASMIT_SBC && mIns[i + 4].mMode == ASMIM_ZERO_PAGE && mIns[i + 4].mAddress == mIns[i + 1].mAddress && !(mIns[i + 4].mLive & LIVE_MEM))
					{
						mIns[i + 4] = mIns[i + 0];
						mIns[i + 4].mType = ASMIT_SBC;

						mIns[i + 0].mType = ASMIT_NOP; mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 1].mType = ASMIT_NOP; mIns[i + 1].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_LDA &&
						mIns[i + 3].mType == ASMIT_CLC &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_ZERO_PAGE && mIns[i + 4].mAddress == mIns[i + 0].mAddress)
					{
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;

						mIns[i + 3] = mIns[i + 2];
						mIns[i + 3].mType = ASMIT_ADC;
						mIns[i + 2].mType = ASMIT_CLC;
						mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 4].mType = ASMIT_NOP;
						mIns[i + 4].mMode = ASMIM_IMPLIED;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_STA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						!mIns[i + 1].ChangesAccu() &&
						mIns[i + 2].mType == ASMIT_CLC &&
						mIns[i + 3].mType == ASMIT_LDA &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_ZERO_PAGE && mIns[i + 4].mAddress == mIns[i + 0].mAddress)
					{
						mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mLive |= LIVE_CPU_REG_A;

						mIns[i + 3].mType = ASMIT_ADC;
						mIns[i + 4].mType = ASMIT_NOP;
						mIns[i + 4].mMode = ASMIM_IMPLIED;
						progress = true;
					}
#if 1
					else if (
						mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_INDIRECT_Y &&
						!mIns[i + 2].ChangesYReg() && (mIns[i + 2].mMode == ASMIM_IMMEDIATE || mIns[i + 2].mMode == ASMIM_ZERO_PAGE && mIns[i + 2].mAddress != mIns[i + 1].mAddress) &&
						!mIns[i + 3].ChangesYReg() && (mIns[i + 3].mMode == ASMIM_IMMEDIATE || mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress != mIns[i + 1].mAddress) &&
						mIns[i + 4].mType == ASMIT_STA && mIns[i + 4].mMode == ASMIM_INDIRECT_Y && mIns[i + 1].mAddress == mIns[i + 4].mAddress && !(mIns[i + 4].mLive & LIVE_MEM))
					{
						int	apos, breg, ireg;
						if (FindAddressSumY(i, mIns[i + 1].mAddress, apos, breg, ireg))
						{
							if (breg == mIns[i + 1].mAddress)
							{
								mIns[apos + 3].mType = ASMIT_NOP; mIns[apos + 3].mMode = ASMIM_IMPLIED;
								mIns[apos + 6].mType = ASMIT_NOP; mIns[apos + 6].mMode = ASMIM_IMPLIED;
							}
							if (mIns[i + 4].mLive & LIVE_CPU_REG_Y)
							{
								mIns.Insert(i + 5, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
								mIns[i + 5].mLive |= LIVE_CPU_REG_Y;
							}
							mIns[i + 0].mMode = ASMIM_ZERO_PAGE;
							mIns[i + 0].mAddress = ireg;
							mIns[i + 0].mLive |= LIVE_MEM;
							mIns[i + 1].mAddress = breg;
							mIns[i + 4].mAddress = breg;
							progress = true;
						}
					}


#endif
				}

				if (i + 3 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_INY &&
						mIns[i + 1].mType == ASMIT_TYA &&
						mIns[i + 2].mType == ASMIT_CLC &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_IMMEDIATE && !(mIns[i + 3].mLive & (LIVE_CPU_REG_Y | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP;
						mIns[i + 3].mAddress++;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_TAY &&
						mIns[i + 3].mType == ASMIT_INY && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 1].mAddress++;
						mIns[i + 3].mType = ASMIT_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INX &&
						mIns[i + 1].mType == ASMIT_TXA &&
						mIns[i + 2].mType == ASMIT_CLC &&
						mIns[i + 3].mType == ASMIT_ADC && mIns[i + 3].mMode == ASMIM_IMMEDIATE && !(mIns[i + 3].mLive & (LIVE_CPU_REG_X | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_NOP;
						mIns[i + 3].mAddress++;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_TAX &&
						mIns[i + 3].mType == ASMIT_INX && !(mIns[i + 3].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 1].mAddress++;
						mIns[i + 3].mType = ASMIT_NOP;
						progress = true;
					}
				}
#if 1
				if (pass > 2 && i + 4 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_INY &&
						mIns[i + 1].mType == ASMIT_INY &&
						mIns[i + 2].mType == ASMIT_INY &&
						mIns[i + 3].mType == ASMIT_INY &&
						mIns[i + 4].mType == ASMIT_INY && !(mIns[i + 4].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_TYA; mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_CLC; mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mType = ASMIT_ADC; mIns[i + 2].mMode = ASMIM_IMMEDIATE; mIns[i + 2].mAddress = 5; mIns[i + 2].mLive |= LIVE_CPU_REG_A;
						mIns[i + 3].mType = ASMIT_TAY;
						mIns[i + 4].mType = ASMIT_NOP;
						progress = true;
					}
					else if (
						mIns[i + 0].mType == ASMIT_INX &&
						mIns[i + 1].mType == ASMIT_INX &&
						mIns[i + 2].mType == ASMIT_INX &&
						mIns[i + 3].mType == ASMIT_INX &&
						mIns[i + 4].mType == ASMIT_INX && !(mIns[i + 4].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_C)))
					{
						mIns[i + 0].mType = ASMIT_TXA; mIns[i + 0].mLive |= LIVE_CPU_REG_A;
						mIns[i + 1].mType = ASMIT_CLC; mIns[i + 1].mLive |= LIVE_CPU_REG_A;
						mIns[i + 2].mType = ASMIT_ADC; mIns[i + 2].mMode = ASMIM_IMMEDIATE; mIns[i + 2].mAddress = 5; mIns[i + 2].mLive |= LIVE_CPU_REG_A;
						mIns[i + 3].mType = ASMIT_TAX;
						mIns[i + 4].mType = ASMIT_NOP;
						progress = true;
					}
				}
#endif
				if (i + 5 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_LDA && mIns[i + 0].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 1].mType == ASMIT_LDY && mIns[i + 1].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_INDIRECT_Y && !(mIns[i + 2].mFlags & NCIF_VOLATILE) &&
						mIns[i + 3].mType == ASMIT_TXA &&
						mIns[i + 4].mType == ASMIT_LDY && mIns[i + 4].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 5].mType == ASMIT_STA && mIns[i + 5].mMode == ASMIM_INDIRECT_Y && mIns[i + 2].mAddress == mIns[i + 5].mAddress &&
						!(mIns[i + 5].mFlags & NCIF_VOLATILE) && !(mIns[i + 5].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y | LIVE_CPU_REG_Z)))
					{
						mIns[i + 3] = mIns[i + 0];
						mIns[i + 0].mType = ASMIT_TXA; mIns[i + 0].mMode = ASMIM_IMPLIED;
						int a = mIns[i + 1].mAddress; mIns[i + 1].mAddress = mIns[i + 4].mAddress; mIns[i + 4].mAddress = a;
						progress = true;
					}
				}

				if (i + 6 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_LDA && mIns[i + 1].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[i + 1].mFlags & NCIF_LOWER) &&
						mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 3].mType == ASMIT_STA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 4].mType == ASMIT_LDA && mIns[i + 4].mMode == ASMIM_IMMEDIATE_ADDRESS && (mIns[i + 4].mFlags & NCIF_UPPER) && mIns[i + 4].mLinkerObject == mIns[i + 1].mLinkerObject && mIns[i + 4].mAddress == mIns[i + 1].mAddress &&
						mIns[i + 5].mType == ASMIT_ADC && mIns[i + 5].mMode == ASMIM_IMMEDIATE &&
						mIns[i + 6].mType == ASMIT_STA && mIns[i + 6].mMode == ASMIM_ZERO_PAGE && !(mIns[i + 6].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Y | LIVE_CPU_REG_Z)))
					{
						mIns[i + 1].mAddress = mIns[i + 4].mAddress = mIns[i + 1].mAddress + mIns[i + 2].mAddress + 256 * mIns[i + 5].mAddress;
						mIns[i + 2].mType = ASMIT_NOP; mIns[i + 2].mMode = ASMIM_IMPLIED;
						mIns[i + 5].mType = ASMIT_NOP; mIns[i + 5].mMode = ASMIM_IMPLIED;
						progress = true;
					}
				}

#if 1
				if (i + 1 < mIns.Size() && mIns[i + 0].mType == ASMIT_LDY && mIns[i + 0].mMode == ASMIM_IMMEDIATE && mIns[i + 0].mAddress == 0 && mIns[i + 1].mMode == ASMIM_INDIRECT_Y)
				{
					int	apos, breg;
					if (FindDirectAddressSumY(i, mIns[i + 1].mAddress, apos, breg))
					{
						if (PatchDirectAddressSumY(i, mIns[i + 1].mAddress, apos, breg))
							progress = true;
					}
				}
				
				if (mIns[i + 0].mMode == ASMIM_INDIRECT_Y && (mIns[i + 0].mFlags & NCIF_YZERO))
				{
					int	apos, breg;
					if (FindDirectAddressSumY(i, mIns[i].mAddress, apos, breg))
					{
						if (PatchDirectAddressSumY(i, mIns[i].mAddress, apos, breg))
							progress = true;
					}
				}

#endif
#if 0
				if (i + 7 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_LDA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress + 1 &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_IMMEDIATE && mIns[i + 4].mAddress == 0 &&
						mIns[i + 5].mType == ASMIT_STA && mIns[i + 5].mMode == ASMIM_ZERO_PAGE && mIns[i + 5].mAddress == mIns[i + 2].mAddress + 1 &&

						mIns[i + 6].mType == ASMIT_LDY && mIns[i + 6].mMode == ASMIM_IMMEDIATE && mIns[i + 6].mAddress == 0 &&
						mIns[i + 7].mMode == ASMIM_INDIRECT_Y && mIns[i + 7].mAddress == mIns[i + 2].mAddress && !(mIns[i + 7].mLive & LIVE_MEM))
					{
						for (int j = 0; j < 6; j++)
						{
							mIns[i + j].mType = ASMIT_NOP; mIns[i + j].mMode = ASMIM_IMPLIED;
						}

						if (mIns[i + 7].mLive & LIVE_CPU_REG_Y)
						{
							mIns.Insert(i + 8, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
							mIns[i + 8].mLive |= LIVE_CPU_REG_Y;
						}

						mIns[i + 6].mType = ASMIT_TAY;
						mIns[i + 6].mMode = ASMIM_IMPLIED;
						mIns[i + 7].mAddress = mIns[i + 1].mAddress;
						progress = true;
					}
				}
#endif					
#if 0
				if (i + 8 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_LDA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress + 1 &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_IMMEDIATE && mIns[i + 4].mAddress == 0 &&
						mIns[i + 5].mType == ASMIT_STA && mIns[i + 5].mMode == ASMIM_ZERO_PAGE && mIns[i + 5].mAddress == mIns[i + 2].mAddress + 1 &&
						!mIns[i + 6].ChangesZeroPage(mIns[i + 1].mAddress) && !mIns[i + 6].ChangesZeroPage(mIns[i + 1].mAddress + 1) && !mIns[i + 6].RequiresYReg() &&
						mIns[i + 7].mType == ASMIT_LDY && mIns[i + 7].mMode == ASMIM_IMMEDIATE && mIns[i + 7].mAddress == 0 &&
						mIns[i + 8].mMode == ASMIM_INDIRECT_Y && mIns[i + 8].mAddress == mIns[i + 2].mAddress && !(mIns[i + 8].mLive & LIVE_MEM))
					{
						for (int j = 0; j < 6; j++)
						{
							mIns[i + j].mType = ASMIT_NOP; mIns[i + j].mMode = ASMIM_IMPLIED;
						}

						if (mIns[i + 8].mLive & LIVE_CPU_REG_Y)
						{
							mIns.Insert(i + 9, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
							mIns[i + 9].mLive |= LIVE_CPU_REG_Y;
						}

						mIns[i + 0].mType = ASMIT_TAY;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 7].mType = ASMIT_NOP; mIns[i + 7].mMode = ASMIM_IMPLIED;
						mIns[i + 8].mAddress = mIns[i + 1].mAddress;
						progress = true;
					}
				}
#endif					
#if 0
				if (i + 11 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_LDA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress + 1 &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_IMMEDIATE && mIns[i + 4].mAddress == 0 &&
						mIns[i + 5].mType == ASMIT_STA && mIns[i + 5].mMode == ASMIM_ZERO_PAGE && mIns[i + 5].mAddress == mIns[i + 2].mAddress + 1 &&
						!mIns[i + 6].ChangesZeroPage(mIns[i + 1].mAddress) && !mIns[i + 6].ChangesZeroPage(mIns[i + 1].mAddress + 1) && !mIns[i + 6].RequiresYReg() &&
						mIns[i + 7].mType == ASMIT_LDY && mIns[i + 7].mMode == ASMIM_IMMEDIATE && mIns[i + 7].mAddress == 0 &&
						mIns[i + 8].mMode == ASMIM_INDIRECT_Y && mIns[i + 8].mAddress == mIns[i + 2].mAddress &&
						!mIns[i + 9].ChangesZeroPage(mIns[i + 1].mAddress) && !mIns[i + 9].ChangesZeroPage(mIns[i + 1].mAddress + 1) && !mIns[i + 9].RequiresYReg() &&
						mIns[i + 10].mType == ASMIT_LDY && mIns[i + 10].mMode == ASMIM_IMMEDIATE && mIns[i + 10].mAddress == 1 &&
						mIns[i + 11].mMode == ASMIM_INDIRECT_Y && mIns[i + 11].mAddress == mIns[i + 2].mAddress &&
						!(mIns[i + 11].mLive & LIVE_MEM))
					{
						for (int j = 0; j < 6; j++)
						{
							mIns[i + j].mType = ASMIT_NOP; mIns[i + j].mMode = ASMIM_IMPLIED;
						}

						if (mIns[i + 11].mLive & LIVE_CPU_REG_Y)
						{
							mIns.Insert(i + 12, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 1));
							mIns[i + 12].mLive |= LIVE_CPU_REG_Y;
						}

						mIns[i + 0].mType = ASMIT_TAY;
						mIns[i + 0].mMode = ASMIM_IMPLIED;
						mIns[i + 7].mType = ASMIT_NOP; mIns[i + 7].mMode = ASMIM_IMPLIED;
						mIns[i + 8].mAddress = mIns[i + 1].mAddress;
						mIns[i + 10].mType = ASMIT_INY; mIns[i + 10].mMode = ASMIM_IMPLIED;
						mIns[i + 11].mAddress = mIns[i + 1].mAddress;
						progress = true;
					}
				}
#endif					
#if 0
				if (i + 9 < mIns.Size())
				{
					if (
						mIns[i + 0].mType == ASMIT_CLC &&
						mIns[i + 1].mType == ASMIT_ADC && mIns[i + 1].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 2].mType == ASMIT_STA && mIns[i + 2].mMode == ASMIM_ZERO_PAGE &&
						mIns[i + 3].mType == ASMIT_LDA && mIns[i + 3].mMode == ASMIM_ZERO_PAGE && mIns[i + 3].mAddress == mIns[i + 1].mAddress + 1 &&
						mIns[i + 4].mType == ASMIT_ADC && mIns[i + 4].mMode == ASMIM_IMMEDIATE && mIns[i + 4].mAddress == 0 &&
						mIns[i + 5].mType == ASMIT_STA && mIns[i + 5].mMode == ASMIM_ZERO_PAGE && mIns[i + 5].mAddress == mIns[i + 2].mAddress + 1 &&

						mIns[i + 6].mType == ASMIT_LDY && mIns[i + 6].mMode == ASMIM_IMMEDIATE && mIns[i + 6].mAddress == 0 &&
						mIns[i + 7].mType == ASMIT_LDA && mIns[i + 7].mMode == ASMIM_INDIRECT_Y && mIns[i + 7].mAddress == mIns[i + 2].mAddress &&
						!mIns[i + 8].ChangesYReg() && (mIns[i + 8].mMode == ASMIM_IMMEDIATE || mIns[i + 8].mMode == ASMIM_ZERO_PAGE && mIns[i + 8].mAddress != mIns[i + 2].mAddress && mIns[i + 8].mAddress != mIns[i + 1].mAddress) &&
						mIns[i + 9].mType == ASMIT_STA && mIns[i + 9].mMode == ASMIM_INDIRECT_Y && mIns[i + 9].mAddress == mIns[i + 2].mAddress && !(mIns[i + 9].mLive & LIVE_MEM)
						)
					{
						for(int j=0; j<6; j++)
						{
							mIns[i + j].mType = ASMIT_NOP; mIns[i + j].mMode = ASMIM_IMPLIED;
						}

						if (mIns[i + 9].mLive & LIVE_CPU_REG_Y)
						{
							mIns.Insert(i + 10, NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
							mIns[i + 10].mLive |= LIVE_CPU_REG_Y;
						}

						mIns[i + 6].mType = ASMIT_TAY;
						mIns[i + 6].mMode = ASMIM_IMPLIED;
						mIns[i + 7].mAddress = mIns[i + 1].mAddress;
						mIns[i + 9].mAddress = mIns[i + 1].mAddress;
						progress = true;
					}
				}
#endif

#endif
#if 1
				if (pass > 1 && mIns[i].mMode == ASMIM_IMMEDIATE_ADDRESS && mIns[i].mLinkerObject && (mIns[i].mFlags & NCIF_LOWER) && !(mIns[i].mAddress & 0xff) && !(mIns[i].mLinkerObject->mAlignment & 0xff))
				{
					mIns[i].mMode = ASMIM_IMMEDIATE;
					mIns[i].mAddress = 0;
					mIns[i].mLinkerObject = nullptr;
					mIns[i].mFlags &= ~NCIF_LOWER;
					progress = true;
				}
#endif
#endif
			}

			if (progress)
				changed = true;

		} while (progress);


		if (this->mTrueJump && this->mTrueJump->PeepHoleOptimizer(pass))
			changed = true;
		if (this->mFalseJump && this->mFalseJump->PeepHoleOptimizer(pass))
			changed = true;

		assert(mIns.Size() == 0 || mIns[0].mType != ASMIT_INV);

		return changed;
	}

	return false;
}

void NativeCodeBasicBlock::Assemble(void)
{
	if (!mAssembled)
	{
		mAssembled = true;

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

NativeCodeBasicBlock* NativeCodeBasicBlock::BypassEmptyBlocks(void)
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

int NativeCodeBasicBlock::LeadsInto(NativeCodeBasicBlock* block, int dist)
{
	if (mPlaced)
		return 6;
	else if (mTrueJump == block || mFalseJump == block)
		return dist;
	else if (dist < 5)
	{
		int d0 = mTrueJump ? mTrueJump->LeadsInto(block, dist + 1) : 6;
		int d1 = mFalseJump ? mFalseJump->LeadsInto(block, dist + 1) : 6;

		if (d0 < d1)
			return d0;
		else
			return d1;
	}

	return 6;
}

void NativeCodeBasicBlock::BuildPlacement(GrowingArray<NativeCodeBasicBlock*>& placement)
{
	if (!mPlaced)
	{
		mPlaced = true;
		mPlace = placement.Size();
		placement.Push(this);

		if (mFalseJump)
		{
			if (mFalseJump->mPlaced)
				mTrueJump->BuildPlacement(placement);
			else if (mTrueJump->mPlaced)
				mFalseJump->BuildPlacement(placement);
			else if (!mTrueJump->mFalseJump && !mFalseJump->mFalseJump && mTrueJump->mTrueJump == mFalseJump->mTrueJump)
			{
				mFalseJump->mPlaced = true;
				mFalseJump->mPlace = placement.Size();
				placement.Push(mFalseJump);

				mTrueJump->BuildPlacement(placement);
			}
			else if (mTrueJump->LeadsInto(mFalseJump, 0) < mFalseJump->LeadsInto(mTrueJump, 0))
			{
				mTrueJump->BuildPlacement(placement);
				mFalseJump->BuildPlacement(placement);
			}
			else if (mTrueJump->LeadsInto(mFalseJump, 0) > mFalseJump->LeadsInto(mTrueJump, 0))
			{
				mFalseJump->BuildPlacement(placement);
				mTrueJump->BuildPlacement(placement);
			}
			else if (
				!mTrueJump->mFalseJump && mTrueJump->mTrueJump && mTrueJump->mTrueJump->mPlaced && mTrueJump->mCode.Size() < 120 ||
				mTrueJump->mFalseJump && mTrueJump->mTrueJump && mTrueJump->mFalseJump->mPlaced && mTrueJump->mTrueJump->mPlaced && mTrueJump->mCode.Size() < 120)
			{
				mTrueJump->BuildPlacement(placement);
				mFalseJump->BuildPlacement(placement);
			}
#if 1
			else if (!mTrueJump->mFalseJump && mTrueJump->mTrueJump && mFalseJump->mFalseJump && !mTrueJump->mTrueJump->mPlaced && mTrueJump->mTrueJump->mNumEntries > 1)
			{
				mTrueJump->mPlaced = true;
				mTrueJump->mPlace = placement.Size();
				placement.Push(mTrueJump);

				mFalseJump->BuildPlacement(placement);
				mTrueJump->mTrueJump->BuildPlacement(placement);
			}
#endif
			else
			{
				mFalseJump->BuildPlacement(placement);
				mTrueJump->BuildPlacement(placement);
			}
		}
		else if (mTrueJump)
		{
			mTrueJump->BuildPlacement(placement);
		}
	}
}

void NativeCodeBasicBlock::InitialOffset(int& total)
{
	mOffset = total;
	total += mCode.Size();
	if (mFalseJump)
	{
		total += 5;
		if (mFalseJump->mPlace != mPlace + 1 && mTrueJump->mPlace != mPlace + 1)
			total += 3;
	}
	else if (mTrueJump)
	{
		if (mTrueJump->mPlace != mPlace + 1)
			total += 3;
	}

	mSize = total - mOffset;
}

bool NativeCodeBasicBlock::CalculateOffset(int& total)
{
	bool	changed = total != mOffset;
	mOffset = total;

	total += mCode.Size();

	if (mFalseJump)
	{
		if (mFalseJump->mPlace == mPlace + 1)
			total += BranchByteSize(total, mTrueJump->mOffset);
		else if (mTrueJump->mPlace == mPlace + 1)
			total += BranchByteSize(total, mFalseJump->mOffset);
		else
		{
			total += BranchByteSize(total, mTrueJump->mOffset);
			total += JumpByteSize(mFalseJump);
		}
	}
	else if (mTrueJump)
	{
		if (mTrueJump->mPlace != mPlace + 1)
			total += JumpByteSize(mTrueJump);
	}

	if (mOffset + mSize != total)
		changed = true;

	mSize = total - mOffset;

	return changed;
}

void NativeCodeBasicBlock::CopyCode(NativeCodeProcedure * proc, uint8* target)
{
	int i;
	int next, end;

#if 1
	end = mOffset + mCode.Size();
	next = mOffset + mSize;

	if (mFalseJump)
	{
		if (mFalseJump->mPlace == mPlace + 1)
			end += PutBranch(proc, mBranch, mTrueJump->mOffset - end);
		else if (mTrueJump->mPlace == mPlace + 1)
			end += PutBranch(proc, InvertBranchCondition(mBranch), mFalseJump->mOffset - end);
		else
		{
			end += PutBranch(proc, mBranch, mTrueJump->mOffset - end);
			end += PutJump(proc, mFalseJump);
		}
	}
	else if (mTrueJump)
	{
		if (mTrueJump->mPlace != mPlace + 1)
			end += PutJump(proc, mTrueJump);
	}

	assert(end == next);

	for (int i = 0; i < mRelocations.Size(); i++)
	{
		LinkerReference& rl(mRelocations[i]);
		rl.mOffset += mOffset;
		if (rl.mFlags & LREF_INBLOCK)
		{
			rl.mRefOffset += mOffset;
			rl.mFlags &= ~LREF_INBLOCK;
		}
		proc->mRelocations.Push(rl);
	}

	for (i = 0; i < mCode.Size(); i++)
	{
		target[i + mOffset] = mCode[i];
	}


#else
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
			if (mTrueJump == this || mTrueJump->mOffset != next)
			{
				next += PutJump(proc, mTrueJump->mOffset - next);
			}
		}

		assert(next - mOffset == mSize);

		for (i = 0; i < mCode.Size(); i++)
		{
			target[i + mOffset] = mCode[i];
		}

		for (int i = 0; i < mRelocations.Size(); i++)
		{
			LinkerReference& rl(mRelocations[i]);
			rl.mOffset += mOffset;
			if (rl.mFlags & LREF_INBLOCK)
			{
				rl.mRefOffset += mOffset;
				rl.mFlags &= ~LREF_INBLOCK;
			}
			proc->mRelocations.Push(rl);
		}

		if (mTrueJump) mTrueJump->CopyCode(proc, target);
		if (mFalseJump) mFalseJump->CopyCode(proc, target);
	}
#endif
}
#if 0
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
				// 
				
				if (mTrueJump->mFalseJump == mFalseJump || mTrueJump->mTrueJump == mFalseJump)
				{
					NativeCodeBasicBlock* block = mFalseJump;
					mFalseJump = mTrueJump;
					mTrueJump = block;
					mBranch = InvertBranchCondition(mBranch);
				}

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
#endif

NativeCodeBasicBlock::NativeCodeBasicBlock(void)
	: mIns(NativeCodeInstruction(ASMIT_INV, ASMIM_IMPLIED)), mRelocations({ 0 }), mEntryBlocks(nullptr), mCode(0)
{
	mTrueJump = mFalseJump = NULL;
	mOffset = -1;
	mPlaced = false;
	mCopied = false;
	mKnownShortBranch = false;
	mBypassed = false;
	mAssembled = false;
	mLocked = false;
	mLoopHeadBlock = nullptr;
}

NativeCodeBasicBlock::~NativeCodeBasicBlock(void)
{

}

NativeCodeProcedure::NativeCodeProcedure(NativeCodeGenerator* generator)
	: mGenerator(generator), mRelocations({ 0 }), mBlocks(nullptr)
{
	mTempBlocks = 1000;
}

NativeCodeProcedure::~NativeCodeProcedure(void)
{

}

void NativeCodeProcedure::CompressTemporaries(void)
{
	if (mInterProc->mTempSize > 16)
	{
		ResetVisited();

		NumberSet	used(256), modified(256), pairs(256);

		mEntryBlock->CollectZeroPageUsage(used, modified, pairs);

		uint8	remap[256];
		for (int i = 0; i < 256; i++)
			remap[i] = i;

		int tpos = BC_REG_TMP_SAVED;
		if (mInterProc->mLeafProcedure)
			tpos = BC_REG_TMP;

		for (int i = 0; i < mInterProc->mTempOffset.Size(); i++)
		{
			bool	tused = false;

			int	reg = BC_REG_TMP + mInterProc->mTempOffset[i];
			if (mInterProc->mLeafProcedure || reg >= BC_REG_TMP_SAVED)
			{
				int size = mInterProc->mTempSizes[i];
				int usize = 0;

				for (int j = 0; j < size; j++)
					if (used[reg + j])
						usize = j + 1;

				if (usize)
				{
					if (tpos < BC_REG_TMP_SAVED && tpos + usize > BC_REG_TMP + mInterProc->mCallerSavedTemps)
						tpos = BC_REG_TMP_SAVED;

					for (int j = 0; j < usize; j++)
						remap[reg + j] = tpos + j;

					mInterProc->mTempOffset[i] = tpos - BC_REG_TMP;
					mInterProc->mTempSizes[i] = usize;
					tpos += usize;

				}
				else
				{
					mInterProc->mTempOffset[i] = 0;
					mInterProc->mTempSizes[i] = 0;
				}
			}
		}

		ResetVisited();
		mEntryBlock->RemapZeroPage(remap);

		mInterProc->mTempSize = tpos - BC_REG_TMP;

		if (mNoFrame && !used[BC_REG_STACK] && mInterProc->mTempSize <= 16)
			mStackExpand = 0;
	}
}

void NativeCodeProcedure::Compile(InterCodeProcedure* proc)
{
	mInterProc = proc;

	int	nblocks = proc->mBlocks.Size();
	tblocks = new NativeCodeBasicBlock * [nblocks];
	for (int i = 0; i < nblocks; i++)
		tblocks[i] = nullptr;

	mIndex = proc->mID;

	int		tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;
	int		commonFrameSize = proc->mCommonFrameSize;

	mStackExpand = tempSave + proc->mLocalSize;

	if (proc->mCallsByteCode || commonFrameSize > 0)
		commonFrameSize += 2;

	mFrameOffset = 0;
	mNoFrame = (mStackExpand + proc->mCommonFrameSize) < 64 && !proc->mHasDynamicStack;// && !(proc->mHasInlineAssembler && !proc->mLeafProcedure);

	if (mNoFrame)
		proc->mLinkerObject->mFlags |= LOBJF_NO_FRAME;

	if (mNoFrame)
	{
		if (mStackExpand > 0)
			mFrameOffset = tempSave;
	}
	else
	{
		mStackExpand += 2;
	}

	if (!proc->mLeafProcedure)
	{
		if (mNoFrame)
			mFrameOffset = commonFrameSize + tempSave;
	}

	mEntryBlock = AllocateBlock();
	mEntryBlock->mLocked = true;
	mBlocks.Push(mEntryBlock);

	mExitBlock = AllocateBlock();
	mExitBlock->mLocked = true;
	mBlocks.Push(mExitBlock);

	// Place a temporary RTS

	mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_RTS, ASMIM_IMPLIED, 0, nullptr, 0));

	mEntryBlock->mTrueJump = CompileBlock(mInterProc, mInterProc->mBlocks[0]);
	mEntryBlock->mBranch = ASMIT_JMP;

	Optimize();

	assert(mEntryBlock->mIns.Size() == 0);

	// Remove temporary RTS

	mExitBlock->mIns.Pop();

	CompressTemporaries();

	int frameSpace = tempSave;

	tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;

	if (!(mGenerator->mCompilerOptions & COPT_NATIVE))
		mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BYTE, ASMIM_IMPLIED, 0xea));

	bool	ignoreExpandCommonFrame = false;

	if (mInterProc->mInterrupt)
	{
		if (!mNoFrame || mStackExpand > 0 || commonFrameSize > 0)
			mGenerator->mErrors->Error(mInterProc->mLocation, ERRR_INTERRUPT_TO_COMPLEX, "Function to complex for interrupt");

		ZeroPageSet	zpLocal, zpGlobal;
		ResetVisited();
		mEntryBlock->CollectZeroPageSet(zpLocal, zpGlobal);
		zpLocal |= zpGlobal;

		if (proc->mHardwareInterrupt)
		{
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_PHA));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_TXA));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_PHA));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_TYA));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_PHA));
		}

		bool	usesStack = false;

		if (zpLocal[BC_REG_STACK])
		{
			usesStack = true;
			zpLocal -= BC_REG_STACK;
			zpLocal -= BC_REG_STACK + 1;
		}

		if (usesStack)
		{
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}

		for (int i = 2; i < 256; i++)
		{
			if (zpLocal[i])
			{
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, i));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_PHA));
			}
		}

		for (int i = 255; i >= 2; i--)
		{
			if (zpLocal[i])
			{
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_PLA));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, i));
			}
		}
		if (usesStack)
		{
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}

		if (proc->mHardwareInterrupt)
		{
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_PLA));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_TAY));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_PLA));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_TAX));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_PLA));
		}
	}
	else
	{
		if (mNoFrame)
		{
			if (mStackExpand > 0)
			{
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (mStackExpand + commonFrameSize) & 0xff));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BCS, ASMIM_RELATIVE, 2));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
				ignoreExpandCommonFrame = true;

				if (tempSave)
				{
					mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, commonFrameSize + tempSave - 1));
					if (tempSave == 1)
					{
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
					else if (tempSave == 2)
					{
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED + 1));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));

					}
					else if (commonFrameSize > 0)
					{
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, tempSave - 1));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE_X, BC_REG_TMP_SAVED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
					}
					else
					{
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
					}
				}
			}
		}
		else
		{
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, mStackExpand & 0xff));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (mStackExpand >> 8) & 0xff));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));

			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));

			if (tempSave)
			{
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));

				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				if (tempSave > 1)
				{
					mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
					mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
				}
			}

			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, frameSpace + 2));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));
		}

		if (!proc->mLeafProcedure && commonFrameSize > 0 && !ignoreExpandCommonFrame)
		{
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, commonFrameSize & 0xff));
			mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			if (commonFrameSize >= 256)
			{
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (commonFrameSize >> 8) & 0xff));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			}
			else
			{
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BCS, ASMIM_RELATIVE, 2));
				mEntryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			}

			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, commonFrameSize & 0xff));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			if (commonFrameSize >= 256)
			{
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (commonFrameSize >> 8) & 0xff));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			}
			else
			{
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BCC, ASMIM_RELATIVE, 2));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			}
		}

		if (mNoFrame)
		{
			if (mStackExpand > 0)
			{
				if (tempSave)
				{
					mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, commonFrameSize + tempSave - 1));
					if (tempSave == 1)
					{
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED));
					}
					else if (tempSave == 2)
					{
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED + 1));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP_SAVED));

					}
					else if (commonFrameSize > 0)
					{
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, tempSave - 1));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE_X, BC_REG_TMP_SAVED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
					}
					else
					{
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
						mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
					}
				}

				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (mStackExpand + commonFrameSize) & 0xff));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BCC, ASMIM_RELATIVE, 2));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			}
		}
		else
		{
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));

			if (tempSave)
			{
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));

				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
				if (tempSave > 1)
				{
					mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
					mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -8));
				}
			}

			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, mStackExpand & 0xff));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (mStackExpand >> 8) & 0xff));
			mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));

		}

		ZeroPageSet	zpLocal, zpGlobal;
		ResetVisited();
		mEntryBlock->CollectZeroPageSet(zpLocal, zpGlobal);
		zpLocal |= zpGlobal;

		proc->mLinkerObject->mZeroPageSet = zpLocal;
	}

	if (proc->mHardwareInterrupt)
		mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_RTI, ASMIM_IMPLIED));
	else
		mExitBlock->mIns.Push(NativeCodeInstruction(ASMIT_RTS, ASMIM_IMPLIED));

	mEntryBlock->Assemble();

	NativeCodeBasicBlock* lentryBlock = mEntryBlock->BypassEmptyBlocks();

	proc->mLinkerObject->mType = LOT_NATIVE_CODE;

	GrowingArray<NativeCodeBasicBlock*>	placement(nullptr);

	int	total;
	total = 0;

	lentryBlock->BuildPlacement(placement);

	for (int i = 0; i < placement.Size(); i++)
		placement[i]->InitialOffset(total);

	bool	progress;
	do {
		progress = false;
		total = 0;
		for (int i = 0; i < placement.Size(); i++)
			if (placement[i]->CalculateOffset(total))
				progress = true;
	} while (progress);

	uint8* data = proc->mLinkerObject->AddSpace(total);

	for (int i = 0; i < placement.Size(); i++)
		placement[i]->CopyCode(this, data);


	for (int i = 0; i < mRelocations.Size(); i++)
	{
		LinkerReference& rl(mRelocations[i]);
		rl.mObject = proc->mLinkerObject;
		if (!rl.mRefObject)
			rl.mRefObject = proc->mLinkerObject;
		proc->mLinkerObject->AddReference(rl);
	}
}


bool NativeCodeProcedure::MapFastParamsToTemps(void)
{
	NumberSet	used(256), modified(256), statics(256), pairs(256);

	ResetVisited();
	mEntryBlock->CollectZeroPageUsage(used, modified, pairs);

	used.Fill();

	for (int i = BC_REG_TMP; i < 256; i++)
		used -= i;
	
	for (int i = BC_REG_FPARAMS; i < BC_REG_FPARAMS_END; i++)
		if (!modified[i])
			statics += i;

	uint8	alias[256];
	for (int i = 0; i < 256; i++)
		alias[i] = 0;

	ResetVisited();
	mEntryBlock->FindZeroPageAlias(statics, used, alias, -1);

	for (int i = 1; i < 256; i++)
	{
		if (used[i] || !alias[i] ||
			(pairs[i] && (used[i + 1] || alias[i + 1] != alias[i] + 1)) ||
			(pairs[i - 1] && (used[i - 1] || alias[i - 1] + 1 != alias[i])))
		{
			alias[i] = i;
		}
	}

	ResetVisited();
	return mEntryBlock->RemapZeroPage(alias);
}

void NativeCodeProcedure::Optimize(void)
{
#if 1
	int		step = 0;
	int cnt = 0;

	bool	changed, xmapped = false, ymapped = false;
	do
	{
		changed = false;

		ResetVisited();
		for (int i = 0; i < mBlocks.Size(); i++)
		{
			mBlocks[i]->mNumEntries = 0;
			mBlocks[i]->mVisiting = false;
			mBlocks[i]->mLoopHead = false;
			mBlocks[i]->mFromJump = nullptr;
		}
		mEntryBlock->CountEntries(nullptr);

#if 1
		do
		{
			BuildDataFlowSets();
			ResetVisited();
			changed = mEntryBlock->RemoveUnusedResultInstructions();

			ResetVisited();
			NativeRegisterDataSet	data;
			if (mEntryBlock->ValueForwarding(data, step > 0))
				changed = true;

		} while (changed);
#endif
#if 1
		ResetVisited();
		if (mEntryBlock->PeepHoleOptimizer(step))
			changed = true;
#endif
#if 1
	
		ResetVisited();
		if (mEntryBlock->OptimizeSelect(this))
		{
			changed = true;
		}
		
		if (step > 0)
		{
			ResetVisited();
			if (mEntryBlock->OptimizeSimpleLoop(this))
				changed = true;


			ResetVisited();
			if (mEntryBlock->SimpleLoopReversal(this))
				changed = true;
		}

		ResetVisited();
		if (mEntryBlock->MergeBasicBlocks())
			changed = true;

		ResetEntryBlocks();
		ResetVisited();
		mEntryBlock->CollectEntryBlocks(nullptr);

#if 1
		if (step == 2)
		{
			if (MapFastParamsToTemps())
				changed = true;
		}
#endif
#if 1
		if (step > 2)
		{
			ResetVisited();
			if (mEntryBlock->JoinTailCodeSequences(this))
				changed = true;
		}
#endif
#if 1
		if (step == 3)
		{
			ResetVisited();
			changed = mEntryBlock->OptimizeInnerLoops(this);

			ResetVisited();
			if (mEntryBlock->ReduceLocalYPressure())
				changed = true;
		}
#endif
#if 1
		else if (step == 4)
		{
#if 1
#if 1
			int	xregs[256], yregs[256];
			for (int i = 0; i < 256; i++)
				xregs[i] = yregs[i] = 0;

			for (int i = 0; i < 4; i++)
			{
				xregs[BC_REG_ACCU + i] = -1;
				yregs[BC_REG_ACCU + i] = -1;
				xregs[BC_REG_WORK + i] = -1;
				yregs[BC_REG_WORK + i] = -1;
			}

			if (!mInterProc->mLeafProcedure)
			{
				for (int i = BC_REG_FPARAMS; i < BC_REG_FPARAMS_END; i++)
				{
					xregs[i] = -1;
					yregs[i] = -1;
				}
			}

			if (xmapped)
				xregs[0] = -1;
			if (ymapped)
				yregs[0] = -1;

			ResetVisited();
			mEntryBlock->GlobalRegisterXYCheck(xregs, yregs);
			if (xregs[0] >= 0)
			{
				int j = 1;
				for (int i = 0; i < 256; i++)
					if (xregs[i] > xregs[j])
						j = i;
				if (xregs[j] > 0)
				{
					ResetVisited();
					mEntryBlock->GlobalRegisterXMap(j);
					if (j >= BC_REG_FPARAMS && j < BC_REG_FPARAMS_END)
						mEntryBlock->mTrueJump->mIns.Insert(0, NativeCodeInstruction(ASMIT_LDX, ASMIM_ZERO_PAGE, j));
					changed = true;
					xmapped = true;
				}
			}
			
			if (!changed && yregs[0] >= 0)
			{
				int j = 1;
				for (int i = 0; i < 256; i++)
					if (yregs[i] > yregs[j])
						j = i;
				if (yregs[j] > 0)
				{
					ResetVisited();
					mEntryBlock->GlobalRegisterYMap(j);
					if (j >= BC_REG_FPARAMS && j < BC_REG_FPARAMS_END)
						mEntryBlock->mTrueJump->mIns.Insert(0, NativeCodeInstruction(ASMIT_LDY, ASMIM_ZERO_PAGE, j));
					changed = true;
					ymapped = true;
				}
			}
#endif
			if (!changed)
			{
				ResetVisited();
				if (mEntryBlock->LocalRegisterXYMap())
					changed = true;
			}
#endif
		}
#endif
#if 1
		ResetVisited();
		NativeRegisterDataSet	data;
		mEntryBlock->BuildEntryDataSet(data);

		ResetVisited();
		if (mEntryBlock->ApplyEntryDataSet())
			changed = true;
#endif
#endif
		if (!changed && step < 5)
		{
			step++;
			changed = true;
		}
		cnt++;
	} while (changed);

#if 1
	ResetVisited();
	mEntryBlock->BlockSizeReduction();
#endif

#endif
}

void NativeCodeProcedure::BuildDataFlowSets(void)
{
	//
	//	Build set with local provided/required temporaries
	//
	ResetVisited();
	mBlocks[0]->BuildLocalRegSets();

	//
	// Build set of globaly provided temporaries
	//
	ResetVisited();
	mBlocks[0]->BuildGlobalProvidedRegSet(NumberSet(NUM_REGS));

	//
	// Build set of globaly required temporaries, might need
	// multiple iterations until it stabilizes
	//
	NumberSet	totalRequired(NUM_REGS);

	do {
		ResetVisited();
	} while (mBlocks[0]->BuildGlobalRequiredRegSet(totalRequired));
}

NativeCodeBasicBlock* NativeCodeProcedure::AllocateBlock(void)
{
	NativeCodeBasicBlock* block = new NativeCodeBasicBlock();
	block->mNoFrame = mNoFrame;
	block->mFrameOffset = mFrameOffset;
	block->mIndex = mTempBlocks++;
	mBlocks.Push(block);

	return block;
}

NativeCodeBasicBlock* NativeCodeProcedure::CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* sblock)
{
	if (tblocks[sblock->mIndex])
		return tblocks[sblock->mIndex];

	NativeCodeBasicBlock* block = new NativeCodeBasicBlock();
	block->mNoFrame = mNoFrame;
	block->mFrameOffset = mFrameOffset;
	mBlocks.Push(block);

	tblocks[sblock->mIndex] = block;
	block->mIndex = sblock->mIndex;

	CompileInterBlock(iproc, sblock, block);

	return block;
}

void NativeCodeProcedure::ResetEntryBlocks(void)
{
	for (int i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mEntryBlocks.SetSize(0);
}

void NativeCodeProcedure::ResetVisited(void)
{
	int i;

	for (i = 0; i < mBlocks.Size(); i++)
	{
//		assert(mBlocks[i]->mIns.Size() > 0 || (mBlocks[i]->mTrueJump != mBlocks[i] && mBlocks[i]->mFalseJump != mBlocks[i]));

		mBlocks[i]->mVisited = false;
		mBlocks[i]->mNumEntered = 0;
		mBlocks[i]->mLoopHeadBlock = nullptr;

	}
}


void NativeCodeProcedure::CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock* block)
{
	int	i = 0;
	while (i < iblock->mInstructions.Size())
	{
		const InterInstruction * ins = iblock->mInstructions[i];

		switch (ins->mCode)
		{
		case IC_STORE:
			block->StoreValue(iproc, ins);
			break;
		case IC_LOAD:
			if (i + 1 < iblock->mInstructions.Size() &&
				iblock->mInstructions[i + 1]->mCode == IC_STORE &&
				iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp &&
				iblock->mInstructions[i + 1]->mSrc[0].mFinal)
			{
				block->LoadStoreIndirectValue(iproc, ins, iblock->mInstructions[i + 1]);
				i++;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				(ins->mDst.mType == IT_INT8 || ins->mDst.mType == IT_INT16 || ins->mDst.mType == IT_INT32) &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 2]->mCode == IC_STORE &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp && 
				(iblock->mInstructions[i + 2]->mSrc[0].mFinal || iblock->mInstructions[i + 2]->mSrc[0].mTemp != ins->mSrc[0].mTemp) &&
				block->LoadOpStoreIndirectValue(iproc, ins, iblock->mInstructions[i + 1], 1, iblock->mInstructions[i + 2]))
			{				
				i += 2;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				(ins->mDst.mType == IT_INT8 || ins->mDst.mType == IT_INT16 || ins->mDst.mType == IT_INT32) &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[1].mFinal &&
				iblock->mInstructions[i + 2]->mCode == IC_STORE &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp &&
				(iblock->mInstructions[i + 2]->mSrc[0].mFinal || iblock->mInstructions[i + 2]->mSrc[0].mTemp != ins->mSrc[0].mTemp) &&
				block->LoadOpStoreIndirectValue(iproc, ins, iblock->mInstructions[i + 1], 0, iblock->mInstructions[i + 2]))
			{				
				i += 2;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				(ins->mDst.mType == IT_INT8 || ins->mDst.mType == IT_INT16 || ins->mDst.mType == IT_INT32) &&
				iblock->mInstructions[i + 1]->mCode == IC_UNARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 2]->mCode == IC_STORE &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp &&
				(iblock->mInstructions[i + 2]->mSrc[0].mFinal || iblock->mInstructions[i + 2]->mSrc[0].mTemp != ins->mSrc[0].mTemp) &&
				block->LoadUnopStoreIndirectValue(iproc, ins, iblock->mInstructions[i + 1], iblock->mInstructions[i + 2]))
			{
				i += 2;
			}
			else if (i + 3 < iblock->mInstructions.Size() &&
				(ins->mDst.mType == IT_INT8 || ins->mDst.mType == IT_INT16 || ins->mDst.mType == IT_INT32) &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 2]->mSrc[1].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[1].mFinal &&
				iblock->mInstructions[i + 3]->mCode == IC_STORE &&
				iblock->mInstructions[i + 3]->mSrc[0].mTemp == iblock->mInstructions[i + 2]->mDst.mTemp && //iblock->mInstructions[i + 3]->mSrc[0].mFinal &&
				block->LoadLoadOpStoreIndirectValue(iproc, ins, iblock->mInstructions[i + 1], iblock->mInstructions[i + 2], iblock->mInstructions[i + 3]))
			{
				i += 3;
			}
			else if (i + 3 < iblock->mInstructions.Size() &&
				(ins->mDst.mType == IT_INT8 || ins->mDst.mType == IT_INT16 || ins->mDst.mType == IT_INT32) &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[1].mFinal &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 3]->mCode == IC_STORE &&
				iblock->mInstructions[i + 3]->mSrc[0].mTemp == iblock->mInstructions[i + 2]->mDst.mTemp && //iblock->mInstructions[i + 3]->mSrc[0].mFinal &&
				block->LoadLoadOpStoreIndirectValue(iproc, iblock->mInstructions[i + 1], ins, iblock->mInstructions[i + 2], iblock->mInstructions[i + 3]))
			{
				i += 3;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 1]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[1].mFinal)
			{
				block = block->BinaryOperator(iproc, this, iblock->mInstructions[i + 1], ins, ins);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[0].mFinal)
			{
				block = block->BinaryOperator(iproc, this, iblock->mInstructions[i + 1], nullptr, ins);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[1].mFinal)
			{
				block = block->BinaryOperator(iproc, this, iblock->mInstructions[i + 1], ins, nullptr);
				i++;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD && InterTypeSize[iblock->mInstructions[i + 1]->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mDst.mTemp != ins->mDst.mTemp &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[0].mFinal &&
				iblock->mInstructions[i + 2]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[1].mFinal)
			{
				block = block->BinaryOperator(iproc, this, iblock->mInstructions[i + 2], ins, iblock->mInstructions[i + 1]);
				i += 2;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD && InterTypeSize[iblock->mInstructions[i + 1]->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mDst.mTemp != ins->mDst.mTemp &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSrc[1].mTemp == iblock->mInstructions[i + 1]->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[1].mFinal &&
				iblock->mInstructions[i + 2]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 2]->mSrc[0].mFinal)
			{
				block = block->BinaryOperator(iproc, this, iblock->mInstructions[i + 2], iblock->mInstructions[i + 1], ins);
				i += 2;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				InterTypeSize[ins->mDst.mType] >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LEA &&
				iblock->mInstructions[i + 1]->mSrc[1].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[1].mFinal)
			{
				block->LoadEffectiveAddress(iproc, iblock->mInstructions[i + 1], ins, nullptr);
				i++;
			}
			else
				block->LoadValue(iproc, ins);
			break;
		case IC_COPY:
			block = block->CopyValue(iproc, ins, this);
			break;
		case IC_STRCPY:
			block = block->StrcpyValue(iproc, ins, this);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			{
				for (int i = 0; i < InterTypeSize[ins->mDst.mType]; i++)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + i));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp] + i));
				}
			}
		}	break;
		case IC_BINARY_OPERATOR:
			block = block->BinaryOperator(iproc, this, ins, nullptr, nullptr);
			break;
		case IC_UNARY_OPERATOR:
			if (i + 1 < iblock->mInstructions.Size() && ins->mOperator == IA_NEG && iblock->mInstructions[i + 1]->mCode == IC_LEA && iblock->mInstructions[i + 1]->mSrc[0].mTemp == ins->mDst.mTemp && iblock->mInstructions[i + 1]->mSrc[0].mFinal)
			{
				block->LoadEffectiveAddress(iproc, iblock->mInstructions[i + 1], nullptr, ins);
				i++;
			}
			else
				block->UnaryOperator(iproc, this, ins);
			break;
		case IC_CONVERSION_OPERATOR:
			block->NumericConversion(iproc, this, ins);
			break;
		case IC_LEA:
			block->LoadEffectiveAddress(iproc, ins, nullptr, nullptr);
			break;
		case IC_CONSTANT:
			block->LoadConstant(iproc, ins);
			break;
		case IC_CALL:
			block->CallFunction(iproc, this, ins);
			break;
		case IC_CALL_NATIVE:
		case IC_ASSEMBLER:
			block->CallAssembler(iproc, this, ins);
			break;
		case IC_PUSH_FRAME:
		{
			block->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins->mConst.mIntConst + 2) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			block->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ((ins->mConst.mIntConst + 2) >> 8) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}	break;
		case IC_POP_FRAME:
		{
			block->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mConst.mIntConst + 2) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			block->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ((ins->mConst.mIntConst + 2) >> 8) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (i + 1 < iblock->mInstructions.Size() && iblock->mInstructions[i + 1]->mCode == IC_BRANCH && iblock->mInstructions[i + 1]->mSrc[0].mFinal)
			{
				block->RelationalOperator(iproc, ins, this, CompileBlock(iproc, iblock->mTrueJump), CompileBlock(iproc, iblock->mFalseJump));
				return;
			}
			else
			{
				NativeCodeBasicBlock* tblock, * fblock, * rblock;

				tblock = AllocateBlock();
				fblock = AllocateBlock();
				rblock = AllocateBlock();

				block->RelationalOperator(iproc, ins, this, tblock, fblock);

				tblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 1));
				tblock->Close(rblock, nullptr, ASMIT_JMP);
				fblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
				fblock->Close(rblock, nullptr, ASMIT_JMP);

				rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp]));
				if (InterTypeSize[ins->mDst.mType] > 1)
				{
					rblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp] + 1));
				}

				block = rblock;
			}
			break;

		case IC_RETURN_VALUE:
		{
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[0].mType == IT_FLOAT)
				{
					union { float f; unsigned int v; } cc;
					cc.f = ins->mSrc[0].mFloatConst;

					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

					mExitBlock->mIns[0].mFlags |= NCIF_LOWER | NCIF_UPPER | NCIF_LONG;
				}
				else
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSrc[0].mIntConst & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));

					mExitBlock->mIns[0].mFlags |= NCIF_LOWER;

					if (InterTypeSize[ins->mSrc[0].mType] > 1)
					{
						block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 8) & 0xff));
						block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

						mExitBlock->mIns[0].mFlags |= NCIF_UPPER;

						if (InterTypeSize[ins->mSrc[0].mType] > 2)
						{
							block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 16) & 0xff));
							block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
							block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSrc[0].mIntConst >> 24) & 0xff));
							block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

							mExitBlock->mIns[0].mFlags |= NCIF_LONG;
						}
					}
				}
			}				
			else
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));

				mExitBlock->mIns[0].mFlags |= NCIF_LOWER;

				if (InterTypeSize[ins->mSrc[0].mType] > 1)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

					mExitBlock->mIns[0].mFlags |= NCIF_UPPER;

					if (InterTypeSize[ins->mSrc[0].mType] > 2)
					{
						block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 2));
						block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
						block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 3));
						block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

						mExitBlock->mIns[0].mFlags |= NCIF_LONG;
					}
				}
			}

			block->Close(mExitBlock, nullptr, ASMIT_JMP);
			return;
		}

		case IC_RETURN:
			block->Close(mExitBlock, nullptr, ASMIT_JMP);
			return;

		case IC_TYPECAST:
			if (ins->mSrc[0].mTemp != ins->mDst.mTemp)
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp]));
				if (InterTypeSize[ins->mDst.mType] > 1)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 1));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mDst.mTemp] + 1));
				}
			}
			break;

		case IC_BRANCH:
			if (ins->mSrc[0].mTemp < 0)
			{
				if (ins->mSrc[0].mIntConst == 0)
					block->Close(CompileBlock(iproc, iblock->mFalseJump), nullptr, ASMIT_JMP);
				else
					block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
			}
			else
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp]));
				if (InterTypeSize[ins->mSrc[0].mType] > 1)
					block->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 1));
				if (InterTypeSize[ins->mSrc[0].mType] > 2)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSrc[0].mTemp] + 3));
				}
				block->Close(CompileBlock(iproc, iblock->mTrueJump), CompileBlock(iproc, iblock->mFalseJump), ASMIT_BNE);
			}
			return;

		case IC_UNREACHABLE:
			block->Close(mExitBlock, nullptr, ASMIT_JMP);
			return;
		}

		i++;
	}

	block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
}


NativeCodeGenerator::NativeCodeGenerator(Errors* errors, Linker* linker, LinkerSection* runtimeSection)
	: mErrors(errors), mLinker(linker), mRuntimeSection(runtimeSection), mCompilerOptions(COPT_DEFAULT), mRuntime({ 0 }), mMulTables({nullptr})
{
}

NativeCodeGenerator::~NativeCodeGenerator(void)
{

}

void NativeCodeGenerator::CompleteRuntime(void)
{
	for (int i = 0; i < mMulTables.Size(); i++)
	{
		const MulTable& m(mMulTables[i]);
		m.mLinkerLSB->AddSpace(m.mSize);
		m.mLinkerMSB->AddSpace(m.mSize);

		for (int j = 0; j < m.mSize; j++)
		{
			m.mLinkerLSB->mData[j] = (uint8)(m.mFactor * j);
			m.mLinkerMSB->mData[j] = (uint8)(m.mFactor * j >> 8);
		}
	}
}


LinkerObject* NativeCodeGenerator::AllocateShortMulTable(int factor, int size, bool msb)
{
	int	i = 0;
	while (i < mMulTables.Size() && mMulTables[i].mFactor != factor)
		i++;

	if (i == mMulTables.Size())
	{
		Location	loc;
		MulTable	mt;
		
		char	name[20];
		sprintf_s(name, "__multab%dL", factor);
		mt.mLinkerLSB = mLinker->AddObject(loc, Ident::Unique(name), mRuntimeSection, LOT_DATA);
		sprintf_s(name, "__multab%dH", factor);
		mt.mLinkerMSB = mLinker->AddObject(loc, Ident::Unique(name), mRuntimeSection, LOT_DATA);

		mt.mFactor = factor;
		mt.mSize = size;

		mMulTables.Push(mt);

		return msb ? mt.mLinkerMSB : mt.mLinkerLSB;
	}
	else
	{
		if (size > mMulTables[i].mSize)
			mMulTables[i].mSize = size;

		return msb ? mMulTables[i].mLinkerMSB : mMulTables[i].mLinkerLSB;
	}
}

NativeCodeGenerator::Runtime& NativeCodeGenerator::ResolveRuntime(const Ident* ident)
{
	int	i = 0;
	while (i < mRuntime.Size() && mRuntime[i].mIdent != ident)
		i++;
	Location	loc;
	if (i == mRuntime.Size() || !mRuntime[i].mLinkerObject)
		mErrors->Error(loc, EERR_RUNTIME_CODE, "Undefied runtime function", ident->mString);
	return mRuntime[i];
}

void NativeCodeGenerator::RegisterRuntime(const Ident* ident, LinkerObject* object, int offset)
{
	Runtime	rt;
	rt.mIdent = ident;
	rt.mLinkerObject = object;
	rt.mOffset = offset;
	mRuntime.Push(rt);
}

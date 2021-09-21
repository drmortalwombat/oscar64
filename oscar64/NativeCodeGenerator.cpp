#include "NativeCodeGenerator.h"

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


NativeRegisterData::NativeRegisterData(void)
	: mImmediate(false), mZeroPage(false)
{

}

void NativeRegisterData::Reset(void)
{
	mImmediate = false;
	mZeroPage = false;
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
		if (mRegs[i].mZeroPage && mRegs[i].mValue == addr)
			mRegs[i].mZeroPage = false;
	}
}

NativeCodeInstruction::NativeCodeInstruction(AsmInsType type, AsmInsMode mode, int address, LinkerObject* linkerObject, uint32 flags)
	: mType(type), mMode(mode), mAddress(address), mLinkerObject(linkerObject), mFlags(flags)
{}

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
		}
		else
		{
			requiredTemps += BC_REG_LOCALS;
			requiredTemps += BC_REG_LOCALS + 1;
		}

		return true;
	}

	if (mType == ASMIT_RTS)
	{
		for (int i = 0; i < 4; i++)
		{
			requiredTemps += BC_REG_ACCU + i;
		}

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
		case ASMIT_ADC:
		case ASMIT_SBC:
		case ASMIT_ORA:
		case ASMIT_EOR:
		case ASMIT_AND:
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

bool NativeCodeInstruction::ChangesAddress(void) const
{
	if (mMode != ASMIM_IMPLIED)
		return mType == ASMIT_INC || mType == ASMIT_DEC || mType == ASMIT_ASL || mType == ASMIT_LSR || mType == ASMIT_ROL || mType == ASMIT_ROR || mType == ASMIT_STA || mType == ASMIT_STX || mType == ASMIT_STY;
	else
		return false;
}

bool NativeCodeInstruction::IsCommutative(void) const
{
	return mType == ASMIT_ADC || mType == ASMIT_AND || mType == ASMIT_ORA || mType == ASMIT_EOR;
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

bool NativeCodeInstruction::ValueForwarding(NativeRegisterDataSet& data)
{
	bool	changed = false;

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

		return false;
	}

	switch (mType)
	{
	case ASMIT_ROL:
	case ASMIT_ROR:
	case ASMIT_ASL:
	case ASMIT_LSR:
		if (mMode == ASMIM_IMPLIED)
			data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_INC:
	case ASMIT_DEC:
		data.mRegs[CPU_REG_Z].Reset();
		break;

	case ASMIT_LDA:
		if (mMode == ASMIM_IMMEDIATE)
		{
			if (data.mRegs[CPU_REG_A].mImmediate && data.mRegs[CPU_REG_A].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_A].mImmediate = true;
				data.mRegs[CPU_REG_A].mZeroPage = false;
				data.mRegs[CPU_REG_A].mValue = mAddress;
			}

			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
		}
		else
		{
			if (mMode != ASMIM_ZERO_PAGE)
				data.mRegs[CPU_REG_A].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;

	case ASMIT_ADC:
	case ASMIT_SBC:
		data.mRegs[CPU_REG_A].Reset();
		data.mRegs[CPU_REG_C].Reset();
		data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_CMP:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_A].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue - mAddress;
			data.mRegs[CPU_REG_C].mImmediate = true;
			data.mRegs[CPU_REG_C].mValue = data.mRegs[CPU_REG_A].mValue >= mAddress;
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_CPX:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_X].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_X].mValue - mAddress;
			data.mRegs[CPU_REG_C].mImmediate = true;
			data.mRegs[CPU_REG_C].mValue = data.mRegs[CPU_REG_X].mValue >= mAddress;
		}
		else
		{
			data.mRegs[CPU_REG_C].Reset();
			data.mRegs[CPU_REG_Z].Reset();
		}
		break;
	case ASMIT_CPY:
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_Y].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_Y].mValue - mAddress;
			data.mRegs[CPU_REG_C].mImmediate = true;
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
		if (mMode == ASMIM_IMMEDIATE && data.mRegs[CPU_REG_A].mImmediate)
		{
			if (mType == ASMIT_ORA)
				mAddress |= data.mRegs[CPU_REG_A].mValue;
			else if (mType == ASMIT_AND)
				mAddress &= data.mRegs[CPU_REG_A].mValue;
			else if (mType == ASMIT_EOR)
				mAddress ^= data.mRegs[CPU_REG_A].mValue;
			mType = ASMIT_LDA;
			data.mRegs[CPU_REG_A].mValue = mAddress;
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = mAddress;
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
			if (data.mRegs[CPU_REG_X].mImmediate && data.mRegs[CPU_REG_X].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_X].mImmediate = true;
				data.mRegs[CPU_REG_X].mZeroPage = false;
				data.mRegs[CPU_REG_X].mValue = mAddress;

				data.mRegs[CPU_REG_Z].mImmediate = true;
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
			if (data.mRegs[CPU_REG_Y].mImmediate && data.mRegs[CPU_REG_Y].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_Y].mImmediate = true;
				data.mRegs[CPU_REG_Y].mZeroPage = false;
				data.mRegs[CPU_REG_Y].mValue = mAddress;

				data.mRegs[CPU_REG_Z].mImmediate = true;
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
		if (data.mRegs[CPU_REG_A].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TYA:
		data.mRegs[CPU_REG_A] = data.mRegs[CPU_REG_Y];
		if (data.mRegs[CPU_REG_A].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TAX:
		data.mRegs[CPU_REG_X] = data.mRegs[CPU_REG_A];
		if (data.mRegs[CPU_REG_A].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	case ASMIT_TAY:
		data.mRegs[CPU_REG_Y] = data.mRegs[CPU_REG_A];
		if (data.mRegs[CPU_REG_A].mImmediate)
		{
			data.mRegs[CPU_REG_Z].mImmediate = true;
			data.mRegs[CPU_REG_Z].mValue = data.mRegs[CPU_REG_A].mValue;
		}
		else
			data.mRegs[CPU_REG_Z].Reset();
		break;
	}

	if (mMode == ASMIM_ZERO_PAGE)
	{
		switch (mType)
		{
		case ASMIT_LDA:
			if (data.mRegs[CPU_REG_A].mZeroPage && data.mRegs[CPU_REG_A].mValue == mAddress && !(mLive & LIVE_CPU_REG_Z))
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else if (data.mRegs[mAddress].mImmediate)
			{
				data.mRegs[CPU_REG_A] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_A].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[mAddress].mZeroPage)
			{
				data.mRegs[CPU_REG_A] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_A].mValue;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_A].Reset();
				if (data.mRegs[mAddress].mImmediate)
				{
					data.mRegs[CPU_REG_A].mImmediate = true;
					data.mRegs[CPU_REG_A].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_A].mZeroPage = true;
					data.mRegs[CPU_REG_A].mValue = mAddress;
				}
			}
			break;

		case ASMIT_LDX:
			if (data.mRegs[CPU_REG_X].mZeroPage && data.mRegs[CPU_REG_X].mValue == mAddress)
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else if (data.mRegs[mAddress].mImmediate)
			{
				data.mRegs[CPU_REG_X] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_X].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_X].Reset();
				if (data.mRegs[mAddress].mImmediate)
				{
					data.mRegs[CPU_REG_X].mImmediate = true;
					data.mRegs[CPU_REG_X].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_X].mZeroPage = true;
					data.mRegs[CPU_REG_X].mValue = mAddress;
				}
			}
			break;

		case ASMIT_LDY:
			if (data.mRegs[CPU_REG_Y].mZeroPage && data.mRegs[CPU_REG_Y].mValue == mAddress)
			{
				mType = ASMIT_NOP;
				changed = true;
			}
			else if (data.mRegs[mAddress].mImmediate)
			{
				data.mRegs[CPU_REG_Y] = data.mRegs[mAddress];
				mAddress = data.mRegs[CPU_REG_Y].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else
			{
				data.mRegs[CPU_REG_Y].Reset();
				if (data.mRegs[mAddress].mImmediate)
				{
					data.mRegs[CPU_REG_Y].mImmediate = true;
					data.mRegs[CPU_REG_Y].mValue = data.mRegs[mAddress].mValue;
				}
				else
				{
					data.mRegs[CPU_REG_Y].mZeroPage = true;
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
			if (data.mRegs[mAddress].mImmediate)
			{
				mAddress = data.mRegs[mAddress].mValue;
				mMode = ASMIM_IMMEDIATE;
				changed = true;
			}
			else if (data.mRegs[mAddress].mZeroPage)
			{
				mAddress = data.mRegs[mAddress].mValue;
				changed = true;
			}
			break;

		case ASMIT_STA:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_A].mImmediate)
			{
				data.mRegs[mAddress].mImmediate = true;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_A].mValue;
			}
			else if (data.mRegs[CPU_REG_A].mZeroPage)
			{
				data.mRegs[mAddress].mZeroPage = true;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_A].mValue;
			}
			else
			{
				data.mRegs[CPU_REG_A].mZeroPage = true;
				data.mRegs[CPU_REG_A].mValue = mAddress;
			}
			break;
		case ASMIT_STX:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_X].mImmediate)
			{
				data.mRegs[mAddress].mImmediate = true;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_X].mValue;
			}
			else
			{
				data.mRegs[CPU_REG_X].mZeroPage = true;
				data.mRegs[CPU_REG_X].mValue = mAddress;
			}
			break;
		case ASMIT_STY:
			data.ResetZeroPage(mAddress);
			if (data.mRegs[CPU_REG_Y].mImmediate)
			{
				data.mRegs[mAddress].mImmediate = true;
				data.mRegs[mAddress].mValue = data.mRegs[CPU_REG_Y].mValue;
			}
			else
			{
				data.mRegs[CPU_REG_Y].mZeroPage = true;
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

	return changed;
}

void NativeCodeInstruction::FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps)
{
	// check runtime calls

	if (mType == ASMIT_JSR)
	{
		for (int i = 0; i < 4; i++)
		{
			if (!providedTemps[BC_REG_ACCU + i])
				requiredTemps += BC_REG_ACCU + i;
			providedTemps += BC_REG_ACCU + i;
			if (!providedTemps[BC_REG_WORK + i])
				requiredTemps += BC_REG_WORK + i;
			providedTemps += BC_REG_WORK + i;
			if (!providedTemps[BC_REG_ADDR + i])
				requiredTemps += BC_REG_ADDR + i;
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
		for (int i = 0; i < 4; i++)
		{
			if (!providedTemps[BC_REG_ACCU + i])
				requiredTemps += BC_REG_ACCU + i;
		}

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

void NativeCodeInstruction::Assemble(NativeCodeBasicBlock* block)
{
	if (mType == ASMIT_BYTE)
		block->PutByte(mAddress);
	else
	{
		if (mMode == ASMIM_IMMEDIATE_ADDRESS)
			block->PutByte(AsmInsOpcodes[mType][ASMIM_IMMEDIATE]);
		else
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
		case ASMIM_IMMEDIATE_ADDRESS:
			if (mLinkerObject)
			{
				LinkerReference		rl;
				rl.mOffset = block->mCode.Size();
				rl.mLowByte = mFlags & NCIF_LOWER;
				rl.mHighByte = mFlags & NCIF_UPPER;
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
				rl.mLowByte = true;
				rl.mHighByte = true;
				rl.mRefObject = mLinkerObject;
				rl.mRefOffset = mAddress;
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

	LinkerReference		rl;
	rl.mObject = nullptr;
	rl.mOffset = mCode.Size();
	rl.mLowByte = true;
	rl.mHighByte = true;
	rl.mRefObject = nullptr;
	rl.mRefOffset = mOffset + mCode.Size() + offset - 1;
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

		LinkerReference		rl;
		rl.mObject = nullptr;
		rl.mOffset = mCode.Size();
		rl.mLowByte = true;
		rl.mHighByte = true;
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
		cc.f = ins->mFloatValue;

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
		if (ins->mMemory == IM_GLOBAL)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mIntValue, ins->mLinkerObject, NCIF_LOWER));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mIntValue, ins->mLinkerObject, NCIF_UPPER));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mMemory == IM_ABSOLUTE)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mIntValue & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mIntValue >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
		else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
		{
			int	index = ins->mIntValue;
			int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
			if (ins->mMemory == IM_LOCAL)
				index += proc->mLocalVars[ins->mVarIndex]->mOffset;
			else
				index += ins->mVarIndex + proc->mLocalSize + 2;
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
		else if (ins->mMemory == IM_PROCEDURE)
		{
			NativeCodeInstruction	lins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSIntConst[0], ins->mLinkerObject, NCIF_LOWER);
			NativeCodeInstruction	hins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSIntConst[0], ins->mLinkerObject, NCIF_UPPER);

			mIns.Push(lins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
			mIns.Push(hins);
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mIntValue & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
		if (InterTypeSize[ins->mTType] > 1)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mIntValue >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
		}
	}

}

void NativeCodeBasicBlock::LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins)
{
	LoadConstantToReg(proc, ins, ins->mTType, BC_REG_TMP + proc->mTempOffset[ins->mTTemp]);
}

void NativeCodeBasicBlock::CheckFrameIndex(int& reg, int& index, int size)
{
	if (index + size > 256)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, index & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ADDR));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (index >> 8) & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ADDR + 1));
		index = 0;
		reg = BC_REG_ADDR;
	}
}

void NativeCodeBasicBlock::StoreValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	if (ins->mSType[0] == IT_FLOAT)
	{
		if (ins->mSTemp[1] < 0)
		{
			if (ins->mSTemp[0] < 0)
			{
				union { float f; unsigned int v; } cc;
				cc.f = ins->mSFloatConst[0];

				if (ins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 2, ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 3, ins->mLinkerObject));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 3));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mMemory == IM_FRAME)
				{
					int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y,BC_REG_STACK));
				}
			}
			else
			{
				int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];

				if (ins->mSFinal[0] && CheckPredAccuStore(sreg))
				{
					// cull previous store from accu to temp using direcrt forwarding from accu
					mIns.SetSize(mIns.Size() - 8);
					sreg = BC_REG_ACCU;
				}

				if (ins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 2, ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 3, ins->mLinkerObject));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 3));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(reg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mMemory == IM_FRAME)
				{
					int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 16) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 24) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 2));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 3));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
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
				if (ins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mMemory == IM_FRAME)
				{
					int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
			else
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[1];
					int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(reg, index, 2);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
				}
				else if (ins->mMemory == IM_FRAME)
				{
					int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
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
				if (ins->mOperandSize == 1)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;
						CheckFrameIndex(reg, index, 1);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mMemory == IM_FRAME)
					{
						int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (ins->mOperandSize == 2)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;
						CheckFrameIndex(reg, index, 2);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mMemory == IM_FRAME)
					{
						int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
			}
			else
			{
				if (ins->mOperandSize == 1)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;
						CheckFrameIndex(reg, index, 1);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mMemory == IM_FRAME)
					{
						int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
				else if (ins->mOperandSize == 2)
				{
					if (ins->mMemory == IM_GLOBAL)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1], ins->mLinkerObject));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1, ins->mLinkerObject));
					}
					else if (ins->mMemory == IM_ABSOLUTE)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1]));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, ins->mSIntConst[1] + 1));
					}
					else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
					{
						int	index = ins->mSIntConst[1];
						int	reg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;

						if (ins->mMemory == IM_LOCAL)
							index += proc->mLocalVars[ins->mVarIndex]->mOffset;
						else
							index += ins->mVarIndex + proc->mLocalSize + 2;
						CheckFrameIndex(reg, index, 2);

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, reg));
					}
					else if (ins->mMemory == IM_FRAME)
					{
						int	index = ins->mVarIndex + ins->mSIntConst[1] + 2;

						mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
					}
				}
			}
		}
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));

					if (ins->mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					}
				}
			}
			else
			{
				if (ins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[1]));

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));

					if (ins->mOperandSize == 2)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					}
				}
			}
		}
	}

}

void NativeCodeBasicBlock::LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins)
{
	if (rins->mTType == IT_FLOAT)
	{

	}
	else if (rins->mTType == IT_POINTER)
	{

	}
	else
	{
		if (wins->mOperandSize == 1)
		{
			if (rins->mSTemp[0] < 0)
			{
				if (rins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins->mSIntConst[0], rins->mLinkerObject));
				}
				else if (rins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, rins->mSIntConst[0]));
				}
				else if (rins->mMemory == IM_LOCAL || rins->mMemory == IM_PARAM)
				{
					int	index = rins->mSIntConst[0];
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (rins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[rins->mVarIndex]->mOffset;
					else
						index += rins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(areg, index, 4);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				}
			}
			else
			{
				if (rins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, rins->mSIntConst[0]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[rins->mSTemp[0]]));
				}
			}

			if (wins->mSTemp[1] < 0)
			{
				if (wins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins->mSIntConst[1], wins->mLinkerObject));
				}
				else if (wins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE, wins->mSIntConst[1]));
				}
				else if (wins->mMemory == IM_LOCAL || wins->mMemory == IM_PARAM)
				{
					int	index = wins->mSIntConst[1];
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (wins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[wins->mVarIndex]->mOffset;
					else
						index += wins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(areg, index, 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, areg));
				}
				else if (wins->mMemory == IM_FRAME)
				{
					int	index = wins->mVarIndex + wins->mSIntConst[1] + 2;

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
				}
			}
			else
			{
				if (wins->mMemory == IM_INDIRECT)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, wins->mSIntConst[1]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[wins->mSTemp[1]]));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction* ainsl, const NativeCodeInstruction* ainsh)
{
	if (ins->mTType == IT_FLOAT)
	{
		if (ins->mSTemp[0] < 0)
		{
			if (ins->mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0], ins->mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1, ins->mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 2, ins->mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 3, ins->mLinkerObject));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
			else if (ins->mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 3));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
			else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
			{
				int	index = ins->mSIntConst[0];
				int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
				if (ins->mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mVarIndex]->mOffset;
				else
					index += ins->mVarIndex + proc->mLocalSize + 2;
				CheckFrameIndex(areg, index, 4);

				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, index));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[0]));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 3));
			}
		}
	}
	else if (ins->mTType == IT_POINTER)
	{
		if (ins->mSTemp[0] < 0)
		{
			if (ins->mMemory == IM_GLOBAL)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0], ins->mLinkerObject));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1, ins->mLinkerObject));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins->mMemory == IM_ABSOLUTE)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0]));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
			else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
			{
				int	index = ins->mSIntConst[0];
				int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
				if (ins->mMemory == IM_LOCAL)
					index += proc->mLocalVars[ins->mVarIndex]->mOffset;
				else
					index += ins->mVarIndex + proc->mLocalSize + 2;
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
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				int	src = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];
				mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[0]));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, src));
				if (ainsl)
				{
					if (ainsl->mType == ASMIT_ADC)
						mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
					else if (ainsl->mType == ASMIT_SBC)
						mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(*ainsl);
				}
				if (reg == src)
					mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
				else
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, src));
				if (ainsh) mIns.Push(*ainsh);
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				if (reg == src)
					mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, reg));
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
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0], ins->mLinkerObject));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0]));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[0];
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
					CheckFrameIndex(areg, index, 2);

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

				if (InterTypeSize[ins->mTType] > 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
			else if (ins->mOperandSize == 2)
			{
				if (ins->mMemory == IM_GLOBAL)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0], ins->mLinkerObject));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1, ins->mLinkerObject));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins->mMemory == IM_ABSOLUTE)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0]));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE, ins->mSIntConst[0] + 1));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
				else if (ins->mMemory == IM_LOCAL || ins->mMemory == IM_PARAM)
				{
					int	index = ins->mSIntConst[0];
					int areg = mNoFrame ? BC_REG_STACK : BC_REG_LOCALS;
					if (ins->mMemory == IM_LOCAL)
						index += proc->mLocalVars[ins->mVarIndex]->mOffset;
					else
						index += ins->mVarIndex + proc->mLocalSize + 2;
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
					mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, areg));
					if (ainsh) mIns.Push(*ainsh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
				}
			}
		}
		else
		{
			if (ins->mMemory == IM_INDIRECT)
			{
				if (ins->mOperandSize == 1)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[0]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
					if (InterTypeSize[ins->mTType] > 1)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
						if (ainsh) mIns.Push(*ainsh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
					}
				}
				else if (ins->mOperandSize == 2)
				{
					int	src = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];

					mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, ins->mSIntConst[0]));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, src));
					if (ainsl)
					{
						if (ainsl->mType == ASMIT_ADC)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						else if (ainsl->mType == ASMIT_SBC)
							mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
						mIns.Push(*ainsl);
					}

					if (InterTypeSize[ins->mTType] > 1)
					{
						if (reg == src)
							mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
						else
							mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));

						mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, src));
						if (ainsh) mIns.Push(*ainsh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 1));
						if (reg == src)
							mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, reg));
					}
					else
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg));
				}
			}
		}
	}
}

void NativeCodeBasicBlock::LoadValue(InterCodeProcedure* proc, const InterInstruction * ins)
{
	LoadValueToReg(proc, ins, BC_REG_TMP + proc->mTempOffset[ins->mTTemp], nullptr, nullptr);
}

void NativeCodeBasicBlock::CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc)
{
	int	sreg = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]], dreg = BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]];

	int	size = ins->mOperandSize;
	if (size < 4)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, 0));
		for (int i = 0; i < size; i++)
		{
			if (i > 0)
				mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
		}
	}
	else if (size < 128)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, size - 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
		mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, -7));
	}
	else if (size <= 256)
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, size - 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
		mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -7));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, sreg));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, dreg));
	}
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
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 0));
		for (int i = 0; i < shift; i++)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
			mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_ZERO_PAGE, reg + 1));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, reg + 0));
	}
}

int NativeCodeBasicBlock::ShortMultiply(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins, int index, int mul)
{
	if (sins)
		LoadValueToReg(proc, sins, BC_REG_ACCU, nullptr, nullptr);
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[index]]));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[index]] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
	}

	int	lshift = 0, lmul = mul;
	while (!(lmul & 1))
	{
		lmul >>= 1;
		lshift++;
	}

	switch (lmul)
	{
	case 1:
		ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 3:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TAY, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_TYA, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		ShiftRegisterLeft(proc, BC_REG_ACCU, lshift);
		return BC_REG_ACCU;
	case 5:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_TXA, ASMIM_IMPLIED));
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
	default:
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));

		NativeCodeGenerator::Runtime& rt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16by8")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, rt.mOffset, rt.mLinkerObject, NCIF_RUNTIME));

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
void NativeCodeBasicBlock::BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction * sins1, const InterInstruction * sins0)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];

	if (ins->mTType == IT_FLOAT)
	{
		int	sreg0 = ins->mSTemp[0] < 0 ? -1 : BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]];

		if (ins->mSTemp[1] < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSFloatConst[1];

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
		else if (ins->mSFinal[1] && CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]))
		{
			// cull previous store from accu to temp using direcrt forwarding
			mIns.SetSize(mIns.Size() - 8);
			if (sreg0 == BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]])
				sreg0 = BC_REG_ACCU;
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}

		if (ins->mSTemp[0] < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSFloatConst[0];

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
		}
		else if (sins0)
		{
			LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, sreg0 + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
		}

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fsplitt")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));

		switch (ins->mOperator)
		{
		case IA_ADD:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("faddsub")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME));
		}	break;
		case IA_SUB:
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("faddsub")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME));
		}	break;
		case IA_MUL:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("fmul")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME));
		}	break;
		case IA_DIVS:
		case IA_DIVU:
		{
			NativeCodeGenerator::Runtime& art(nproc->mGenerator->ResolveRuntime(Ident::Unique("fdiv")));
			mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, art.mOffset, art.mLinkerObject, NCIF_RUNTIME));
		}	break;
		}

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));
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
				ins->mSTemp[0] < 0 && ins->mSIntConst[0] == 1 && !sins1 && ins->mSTemp[1] == ins->mTTemp ||
				ins->mSTemp[1] < 0 && ins->mSIntConst[1] == 1 && !sins0 && ins->mSTemp[0] == ins->mTTemp))
			{
				mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, 2));
				mIns.Push(NativeCodeInstruction(ASMIT_INC, ASMIM_ZERO_PAGE, treg + 1));
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

				if (ins->mSTemp[1] < 0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff);
					insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff);
					if (sins0)
						LoadValueToReg(proc, sins0, treg, &insl, &insh);
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(insl);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(insh);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else if (ins->mSTemp[0] < 0)
				{
					insl = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff);
					insh = NativeCodeInstruction(atype, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff);
					if (sins1)
						LoadValueToReg(proc, sins1, treg, &insl, &insh);
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(insl);
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
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

						LoadValueToReg(proc, sins1, treg, nullptr, nullptr);
						LoadValueToReg(proc, sins0, treg, &insl, &insh);
					}
					else if (sins1)
					{
						insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]);
						insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1);

						LoadValueToReg(proc, sins1, treg, &insl, &insh);
					}
					else if (sins0)
					{
						insl = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]);
						insh = NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1);

						LoadValueToReg(proc, sins0, treg, &insl, &insh);
					}
					else
					{
						if (ins->mOperator == IA_ADD)
							mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(atype, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
			}
		} break;
		case IA_SUB:
		{
			NativeCodeInstruction	insl, insh;

			if (ins->mSTemp[0] < 0)
			{
				insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff);
				insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff);
				if (sins1)
					LoadValueToReg(proc, sins1, treg, &insl, &insh);
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
					mIns.Push(insh);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
				}
			}
			else if (ins->mSTemp[1] < 0)
			{
				if (sins0)
				{
					insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg);
					insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, treg + 1);

					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));

					LoadValueToReg(proc, sins0, treg, &insl, &insh);
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
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
					insl = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]);
					insh = NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1);
				}

				if (sins1)
				{
					LoadValueToReg(proc, sins1, treg, &insl, &insh);
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(insl);
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
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
			int	reg = BC_REG_ACCU;

			if (ins->mOperator == IA_MUL && ins->mSTemp[1] < 0 && (ins->mSIntConst[1] & ~0xff) == 0)
			{
				reg = ShortMultiply(proc, nproc, ins, sins0, 0, ins->mSIntConst[1] & 0xff);
			}
			else if (ins->mOperator == IA_MUL && ins->mSTemp[0] < 0 && (ins->mSIntConst[0] & ~0xff) == 0)
			{
				reg = ShortMultiply(proc, nproc, ins, sins1, 1, ins->mSIntConst[0] & 0xff);
			}
			else
			{
				if (sins1)
					LoadValueToReg(proc, sins1, BC_REG_ACCU, nullptr, nullptr);
				else if (ins->mSTemp[1] < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}

				if (sins0)
					LoadValueToReg(proc, sins0, BC_REG_WORK, nullptr, nullptr);
				else if (ins->mSTemp[0] < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
				}

				switch (ins->mOperator)
				{
				case IA_MUL:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mul16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
					reg = BC_REG_WORK + 2;
				}	break;
				case IA_DIVS:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divs16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
				}	break;
				case IA_MODS:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("mods16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
					reg = BC_REG_WORK + 2;
				}	break;
				case IA_DIVU:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("divu16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
				}	break;
				case IA_MODU:
				{
					NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("modu16")));
					mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
					reg = BC_REG_WORK + 2;
				}	break;
				}
			}

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp]));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, reg + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		} break;
		case IA_SHL:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]], nullptr, nullptr);

			if (ins->mSTemp[0] < 0)
			{
				int	shift = ins->mSIntConst[0] & 15;
				if (shift == 0)
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else if (shift == 1)
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
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
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
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
			else if (ins->mSTemp[1] < 0 && IsPowerOf2(ins->mSIntConst[1] & 0xffff))
			{
				int	l = Binlog(ins->mSIntConst[1] & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
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
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSTemp[1] < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
				}
				else if (ins->mSTemp[1] != ins->mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				mIns.Push(NativeCodeInstruction(ASMIT_BEQ, ASMIM_RELATIVE, 2 + 1 + 1 + 2));

				mIns.Push(NativeCodeInstruction(ASMIT_ASL, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -(2 + 1 + 1 + 2)));

				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		} break;
		case IA_SHR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]], nullptr, nullptr);

			if (ins->mSTemp[0] < 0)
			{
				int	shift = ins->mSIntConst[0] & 15;
				if (shift == 0)
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else if (shift == 1)
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					}
					else
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
					}
				}
				else
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
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
						mIns.Push(NativeCodeInstruction(ASMIT_ROL, ASMIM_IMPLIED));
					}
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg ));
				}
			}
			else if (ins->mSTemp[1] < 0 && IsPowerOf2(ins->mSIntConst[1] & 0xffff))
			{
				int	l = Binlog(ins->mSIntConst[1] & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
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
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSTemp[1] < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
				}
				else if (ins->mSTemp[1] != ins->mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
				}
				else
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, treg + 1));
				}

				mIns.Push(NativeCodeInstruction(ASMIT_CPX, ASMIM_IMMEDIATE, 0x00));
				mIns.Push(NativeCodeInstruction(ASMIT_BEQ, ASMIM_RELATIVE, 2 + 1 + 1 + 2));

				mIns.Push(NativeCodeInstruction(ASMIT_LSR, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_ROR, ASMIM_ZERO_PAGE, treg));
				mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
				mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -(2 + 1 + 1 + 2)));

				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			}
		} break;
		case IA_SAR:
		{
			if (sins1) LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]], nullptr, nullptr);
			if (sins0) LoadValueToReg(proc, sins0, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]], nullptr, nullptr);

			if (ins->mSTemp[0] < 0)
			{
				int	shift = ins->mSIntConst[0] & 15;
				if (shift == 0)
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
					}
				}
				else
				{
					if (ins->mSTemp[1] != ins->mTTemp)
					{
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
						mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
						mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
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
			else if (ins->mSTemp[1] < 0 && IsPowerOf2(ins->mSIntConst[1] & 0xffff))
			{
				int	l = Binlog(ins->mSIntConst[1] & 0xffff);

				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
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
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x0f));
				mIns.Push(NativeCodeInstruction(ASMIT_TAX, ASMIM_IMPLIED));

				if (ins->mSTemp[1] < 0)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
				}
				else if (ins->mSTemp[1] != ins->mTTemp)
				{
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));
					mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
					mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));
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
}

void NativeCodeBasicBlock::UnaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins)
{
	int	treg = BC_REG_TMP + proc->mTempOffset[ins->mTTemp];

	if (ins->mTType == IT_FLOAT)
	{
		switch (ins->mOperator)
		{
		case IA_NEG:
		case IA_ABS:
			if (ins->mSTemp[0] != ins->mTTemp)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 3));
			}
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));

			if (ins->mOperator == IA_NEG)
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_AND, ASMIM_IMMEDIATE, 0x7f));

			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));
			break;
		case IA_FLOOR:
		case IA_CEIL:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

			if (ins->mOperator == IA_FLOOR)
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ffloor")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
			}
			else
			{
				NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fceil")));
				mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));
			}

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));
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
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
			mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
			break;

		case IA_NOT:
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, treg + 1));
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
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ftoi")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));

	}	break;
	case IA_INT2FLOAT:
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("ffromi")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));

		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));

	} break;
	case IA_EXT8TO16S:
		mIns.Push(NativeCodeInstruction(ASMIT_LDX, ASMIM_IMMEDIATE, 0));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
		if (ins->mSTemp[0] != ins->mTTemp)
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp]));
		mIns.Push(NativeCodeInstruction(ASMIT_BPL, ASMIM_RELATIVE, 1));
		mIns.Push(NativeCodeInstruction(ASMIT_DEX, ASMIM_IMPLIED));
		mIns.Push(NativeCodeInstruction(ASMIT_STX, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		break;
	case IA_EXT8TO16U:
		if (ins->mSTemp[0] != ins->mTTemp)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp]));
		}
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		break;
	}
}

void NativeCodeBasicBlock::RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump)
{
	InterOperator	op = ins->mOperator;

	if (ins->mSType[0] == IT_FLOAT)
	{
		int	li = 0, ri = 1;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 1; ri = 0;
		}

		if (ins->mSTemp[li] < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSFloatConst[li];

			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}
		else if (ins->mSFinal[li] && CheckPredAccuStore(BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]]))
		{
			// cull previous store from accu to temp using direcrt forwarding
			mIns.SetSize(mIns.Size() - 8);
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
		}

		if (ins->mSTemp[ri] < 0)
		{
			union { float f; unsigned int v; } cc;
			cc.f = ins->mSFloatConst[ri];

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
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK + 3));
		}

		NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("fcmp")));
		mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));

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
	else
	{
		NativeCodeBasicBlock* eblock = nproc->AllocateBlock();
		NativeCodeBasicBlock* nblock = nproc->AllocateBlock();

		int	li = 1, ri = 0;
		if (op == IA_CMPLEU || op == IA_CMPGU || op == IA_CMPLES || op == IA_CMPGS)
		{
			li = 0; ri = 1;
		}

		if (op >= IA_CMPGES && ins->mOperator <= IA_CMPLS)
		{
			if (ins->mSTemp[ri] >= 0)
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
				mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_WORK));
			}

			if (ins->mSTemp[li] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ((ins->mSIntConst[li] >> 8) & 0xff) ^ 0x80));
			else
			{
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 1));
				mIns.Push(NativeCodeInstruction(ASMIT_EOR, ASMIM_IMMEDIATE, 0x80));
			}

			if (ins->mSTemp[ri] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ((ins->mSIntConst[ri] >> 8) & 0xff) ^ 0x80));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_WORK));
		}
		else
		{
			if (ins->mSTemp[li] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[li] >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]] + 1));
			if (ins->mSTemp[ri] < 0)
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, (ins->mSIntConst[ri] >> 8) & 0xff));
			else
				mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]] + 1));
		}

		this->Close(nblock, eblock, ASMIT_BNE);

		if (ins->mSTemp[li] < 0)
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[li] & 0xff));
		else
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[li]]));
		if (ins->mSTemp[ri] < 0)
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_IMMEDIATE, ins->mSIntConst[ri] & 0xff));
		else
			eblock->mIns.Push(NativeCodeInstruction(ASMIT_CMP, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[ri]]));

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
	if (sins1)
	{
		if (ins->mSTemp[0] < 0 && ins->mSIntConst[0] == 0)
			LoadValueToReg(proc, sins1, ins->mTTemp, nullptr, nullptr);
		else
		{
			if (ins->mSTemp[0] < 0)
			{
				NativeCodeInstruction	ainsl(ASMIT_ADC, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff);
				NativeCodeInstruction	ainsh(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff);

				LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mTTemp], &ainsl, &ainsh);
			}
			else
			{
				NativeCodeInstruction	ainsl(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]);
				NativeCodeInstruction	ainsh(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1);

				LoadValueToReg(proc, sins1, BC_REG_TMP + proc->mTempOffset[ins->mTTemp], &ainsl, &ainsh);
			}
		}
	}
	else
	{
		if (ins->mSTemp[0] >= 0 || ins->mSIntConst[0] != 0)
			mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));

		if (ins->mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[1] & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]]));

		if (ins->mSTemp[0] < 0)
		{
			if (ins->mSIntConst[0])
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
		}
		else
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]]));

		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp]));

		if (ins->mSTemp[1] < 0)
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[1] >> 8) & 0xff));
		else
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[1]] + 1));

		if (ins->mSTemp[0] < 0)
		{
			if (ins->mSIntConst[0])
				mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
		}
		else
			mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));

		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
	}
}

void NativeCodeBasicBlock::CallFunction(InterCodeProcedure* proc, NativeCodeProcedure * nproc, const InterInstruction * ins)
{
	if (ins->mSTemp[0] < 0)
	{
		NativeCodeInstruction	lins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSIntConst[0], ins->mLinkerObject, NCIF_LOWER);
		NativeCodeInstruction	hins(ASMIT_LDA, ASMIM_IMMEDIATE_ADDRESS, ins->mSIntConst[0], ins->mLinkerObject, NCIF_UPPER);

		mIns.Push(lins);
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
		mIns.Push(hins);
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
	}
	else
	{
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 0));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
		mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mSTemp[0]] + 1));
		mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
	}

	NativeCodeGenerator::Runtime& frt(nproc->mGenerator->ResolveRuntime(Ident::Unique("bcexec")));
	mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, frt.mOffset, frt.mLinkerObject, NCIF_RUNTIME));

	if (ins->mTTemp >= 0)
	{
		if (ins->mTType == IT_FLOAT)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
		}
	}
}

void NativeCodeBasicBlock::CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins)
{
	mIns.Push(NativeCodeInstruction(ASMIT_JSR, ASMIM_ABSOLUTE, ins->mSIntConst[0], ins->mLinkerObject));
	
	if (ins->mTTemp >= 0)
	{
		if (ins->mTType == IT_FLOAT)
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 2));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 3));
		}
		else
		{
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 0));
			mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
			mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + proc->mTempOffset[ins->mTTemp] + 1));
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
	int	i;

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
			if (!mIns[i].IsUsedResultInstructions(requiredRegs))
			{
				if (i > 0 && mIns[i - 1].mMode == ASMIM_RELATIVE && mIns[i - 1].mAddress > 0)
				{
					mIns[i - 1].mType = ASMIT_NOP;
				}
				mIns[i].mType = ASMIT_NOP;
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

void NativeCodeBasicBlock::CountEntries(void)
{
	mNumEntries++;
	
	if (!mVisited)
	{
		mVisited = true;

		if (mTrueJump)
			mTrueJump->CountEntries();
		if (mFalseJump)
			mFalseJump->CountEntries();
	}
}

bool NativeCodeBasicBlock::MergeBasicBlocks(void)
{
	bool	changed = false;

	if (!mVisited)
	{
		mVisited = true;

		while (mTrueJump && !mFalseJump && mTrueJump->mNumEntries == 1 && mTrueJump != this)
		{
			for (int i = 0; i < mTrueJump->mIns.Size(); i++)
				mIns.Push(mTrueJump->mIns[i]);
			mBranch = mTrueJump->mBranch;
			mFalseJump = mTrueJump->mFalseJump;
			mTrueJump = mTrueJump->mTrueJump;
			changed = true;
		}

		if (mTrueJump)
			mTrueJump->MergeBasicBlocks();
		if (mFalseJump)
			mFalseJump->MergeBasicBlocks();
	}
	return changed;
}

bool NativeCodeBasicBlock::MoveLoadStoreUp(int at)
{
	int	j = at;
	while (j > 0 && !((mIns[j - 1].mType == ASMIT_STA || mIns[j - 1].mType == ASMIT_LDA) && mIns[j - 1].mMode == ASMIM_ZERO_PAGE && mIns[j - 1].mAddress == mIns[at].mAddress))
	{
		j--;
		if ((mIns[j].mMode == ASMIM_ZERO_PAGE || mIns[j].mMode == ASMIM_INDIRECT_Y) && mIns[j].mAddress == mIns[at + 1].mAddress)
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
		mIns[j] = mIns[at + 1];
		mIns[at + 1].mType = ASMIT_NOP;

		return true;
	}

	return false;
}

bool NativeCodeBasicBlock::PeepHoleOptimizer(void)
{
	if (!mVisited)
	{
		bool	changed = false;

		mVisited = true;

		NativeRegisterDataSet	data;

		for (int i = 0; i < mIns.Size(); i++)
		{
			mIns[i].ValueForwarding(data);
		}
#if 1
		if (mFalseJump)
		{
			switch (mBranch)
			{
#if 1
			case ASMIT_BCS:
				if (data.mRegs[CPU_REG_C].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if (!data.mRegs[CPU_REG_C].mValue)
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BCC:
				if (data.mRegs[CPU_REG_C].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if (data.mRegs[CPU_REG_C].mValue)
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
#endif
#if 1
			case ASMIT_BNE:
				if (data.mRegs[CPU_REG_Z].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if (!data.mRegs[CPU_REG_Z].mValue)
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BEQ:
				if (data.mRegs[CPU_REG_Z].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if (data.mRegs[CPU_REG_Z].mValue)
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
#endif
			case ASMIT_BPL:
				if (data.mRegs[CPU_REG_Z].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if ((data.mRegs[CPU_REG_Z].mValue & 0x80))
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			case ASMIT_BMI:
				if (data.mRegs[CPU_REG_Z].mImmediate)
				{
					mBranch = ASMIT_JMP;
					if (!(data.mRegs[CPU_REG_Z].mValue & 0x80))
						mTrueJump = mFalseJump;
					mFalseJump = nullptr;
					changed = true;
				}
				break;
			}
		}
#endif

#if 1
		// move load store pairs up to initial store

		for (int i = 2; i + 2 < mIns.Size(); i++)
		{
			if (mIns[i].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mType == ASMIT_STA && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && (mIns[i + 1].mLive & (LIVE_CPU_REG_A | LIVE_CPU_REG_Z)) == 0)
			{
				if (MoveLoadStoreUp(i))
					changed = true;
			}
		}
#endif

		bool	progress = false;
		do {
			progress = false;

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

			for (int i = 0; i < mIns.Size(); i++)
			{
#if 1
				if (mIns[i].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0)
				{
					mIns[i].mType = ASMIT_LDA;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0xff && (mIns[i].mLive & LIVE_CPU_REG_Z) == 0)
				{
					mIns[i].mType = ASMIT_NOP;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0xff)
				{
					mIns[i].mType = ASMIT_LDA;
					progress = true;
				}
				else if (mIns[i].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i].mAddress == 0x00 && (mIns[i].mLive & LIVE_CPU_REG_Z) == 0)
				{
					mIns[i].mType = ASMIT_NOP;
					progress = true;
				}

				if (i + 1 < mIns.Size())
				{
					if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_LDA)
					{
						mIns[i].mType = ASMIT_NOP;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_LDA && mIns[i + 1].mType == ASMIT_STA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[i + 1].mAddress)
					{
						mIns[i + 1].mType = ASMIT_NOP;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_STA && mIns[i + 1].mType == ASMIT_LDA && mIns[i].mMode == ASMIM_ZERO_PAGE && mIns[i + 1].mMode == ASMIM_ZERO_PAGE && mIns[i].mAddress == mIns[i + 1].mAddress && (mIns[i + 1].mLive & LIVE_CPU_REG_Z) == 0)
					{
						mIns[i + 1].mType = ASMIT_NOP;
						progress = true;
					}
					else if (mIns[i].mType == ASMIT_AND && mIns[i + 1].mType == ASMIT_AND && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress &= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP;
					}
					else if (mIns[i].mType == ASMIT_ORA && mIns[i + 1].mType == ASMIT_ORA && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress |= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP;
					}
					else if (mIns[i].mType == ASMIT_EOR && mIns[i + 1].mType == ASMIT_EOR && mIns[i].mMode == ASMIM_IMMEDIATE && mIns[i + 1].mMode == ASMIM_IMMEDIATE)
					{
						mIns[i].mAddress ^= mIns[i + 1].mAddress;
						mIns[i + 1].mType = ASMIT_NOP;
					}
				}

				if (i + 2 < mIns.Size())
				{
					if (mIns[i].mType == ASMIT_LDA && mIns[i + 2].mType == ASMIT_LDA && (mIns[i + 1].mType == ASMIT_CLC || mIns[i + 1].mType == ASMIT_SEC))
					{
						mIns[i].mType = ASMIT_NOP;
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
						mIns[i + 0].mType = ASMIT_NOP;
						mIns[i + 2].mType = ASMIT_NOP;
					}
				}

				if (i + 3 < mIns.Size())
				{
					if (mIns[i + 0].mType == ASMIT_LDA && mIns[i + 3].mType == ASMIT_STA && mIns[i + 0].SameEffectiveAddress(mIns[i + 3]) &&
						mIns[i + 1].mType == ASMIT_CLC && mIns[i + 2].mType == ASMIT_ADC && mIns[i + 2].mMode == ASMIM_IMMEDIATE && mIns[i + 2].mAddress == 1 &&
						(mIns[i + 0].mMode == ASMIM_ABSOLUTE || mIns[i + 0].mMode == ASMIM_ZERO_PAGE) &&
						(mIns[i + 3].mLive & (LIVE_CPU_REG_C | LIVE_CPU_REG_A)) == 0)
					{
						mIns[i + 0].mType = ASMIT_NOP;
						mIns[i + 1].mType = ASMIT_NOP;
						mIns[i + 2].mType = ASMIT_NOP;
						mIns[i + 3].mType = ASMIT_INC;
					}
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
					}
				}
#endif
			}

			if (progress)
				changed = true;

		} while (progress);

		if (this->mTrueJump && this->mTrueJump->PeepHoleOptimizer())
			changed = true;
		if (this->mFalseJump && this->mFalseJump->PeepHoleOptimizer())
			changed = true;

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
			LinkerReference& rl(mRelocations[i]);
			rl.mOffset += mOffset;
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

NativeCodeProcedure::NativeCodeProcedure(NativeCodeGenerator* generator)
	: mGenerator(generator), mRelocations({ 0 }), mBlocks(nullptr)
{
	mTempBlocks = 1000;
}

NativeCodeProcedure::~NativeCodeProcedure(void)
{

}

void NativeCodeProcedure::Compile(InterCodeProcedure* proc)
{
	int	nblocks = proc->mBlocks.Size();
	tblocks = new NativeCodeBasicBlock * [nblocks];
	for (int i = 0; i < nblocks; i++)
		tblocks[i] = nullptr;

	mIndex = proc->mID;

	int		tempSave = proc->mTempSize > 16 ? proc->mTempSize - 16 : 0;
	int		stackExpand = tempSave + proc->mLocalSize + 2;

	mNoFrame = proc->mLocalSize == 0 && tempSave == 0 && proc->mLeafProcedure;

	entryBlock = new NativeCodeBasicBlock();
	mBlocks.Push(entryBlock);
	entryBlock->mNoFrame = mNoFrame;
	entryBlock->mIndex = 0;

//	generator->mByteCodeUsed[BC_NATIVE] = true;

	entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BYTE, ASMIM_IMPLIED, 0xea));

	if (!mNoFrame)
	{
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, stackExpand & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (stackExpand >> 8) & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));

		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));

		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, tempSave + 2));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, 0));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));

		if (tempSave)
		{
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED - 1));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, - 8));
		}
	}

	if (!proc->mLeafProcedure)
	{
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (proc->mCommonFrameSize + 2) & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ((proc->mCommonFrameSize + 2) >> 8) & 0xff));
		entryBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
	}


	tblocks[0] = entryBlock;

	exitBlock = new NativeCodeBasicBlock();
	exitBlock->mNoFrame = mNoFrame;
	mBlocks.Push(exitBlock);

	if (!proc->mLeafProcedure)
	{
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (proc->mCommonFrameSize + 2) & 0xff));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ((proc->mCommonFrameSize + 2) >> 8) & 0xff));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
	}

	if (!mNoFrame)
	{
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDY, ASMIM_IMMEDIATE, tempSave));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_INY, ASMIM_IMPLIED));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
		exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_LOCALS + 1));

		if (tempSave)
		{
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));

			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_DEY, ASMIM_IMPLIED));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_BNE, ASMIM_RELATIVE, -8));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_INDIRECT_Y, BC_REG_STACK));
			exitBlock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ABSOLUTE_Y, BC_REG_TMP_SAVED));
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

#if 1
	bool	changed;
	do
	{
		BuildDataFlowSets();
		ResetVisited();

		changed = entryBlock->RemoveUnusedResultInstructions();

		ResetVisited();
		if (entryBlock->PeepHoleOptimizer())
			changed = true;

		ResetVisited();
		for (int i = 0; i < mBlocks.Size(); i++)
			mBlocks[i]->mNumEntries = 0;
		entryBlock->CountEntries();

		ResetVisited();
		if (entryBlock->MergeBasicBlocks())
			changed = true;

	} while (changed);

#endif

	entryBlock->Assemble();

	int	total, base;

	NativeCodeBasicBlock* lentryBlock = entryBlock->BypassEmptyBlocks();

	total = 0;
	lentryBlock->CalculateOffset(total);

	proc->mLinkerObject->mType = LOT_NATIVE_CODE;
	lentryBlock->CopyCode(this, proc->mLinkerObject->AddSpace(total));
	
	for (int i = 0; i < mRelocations.Size(); i++)
	{
		LinkerReference& rl(mRelocations[i]);
		rl.mObject = proc->mLinkerObject;
		if (!rl.mRefObject)
			rl.mRefObject = proc->mLinkerObject;
		mGenerator->mLinker->AddReference(rl);
	}
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
	mBlocks.Push(block);

	tblocks[sblock->mIndex] = block;
	block->mIndex = sblock->mIndex;

	CompileInterBlock(iproc, sblock, block);

	return block;
}

void NativeCodeProcedure::ResetVisited(void)
{
	int i;

	for (i = 0; i < mBlocks.Size(); i++)
		mBlocks[i]->mVisited = false;
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
				iblock->mInstructions[i + 1]->mSTemp[0] == ins->mTTemp &&
				iblock->mInstructions[i + 1]->mSFinal[0] &&
				iblock->mInstructions[i + 1]->mOperandSize == 1)
			{
				block->LoadStoreValue(iproc, ins, iblock->mInstructions[i + 1]);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				ins->mOperandSize >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSTemp[0] == ins->mTTemp && iblock->mInstructions[i + 1]->mSFinal[0])
			{
				block->BinaryOperator(iproc, this, iblock->mInstructions[i + 1], nullptr, ins);
				i++;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				ins->mOperandSize >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 1]->mSTemp[1] == ins->mTTemp && iblock->mInstructions[i + 1]->mSFinal[1])
			{
				block->BinaryOperator(iproc, this, iblock->mInstructions[i + 1], ins, nullptr);
				i++;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				ins->mOperandSize >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD && iblock->mInstructions[i + 1]->mOperandSize == 2 &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSTemp[0] == iblock->mInstructions[i + 1]->mTTemp && iblock->mInstructions[i + 2]->mSFinal[0] &&
				iblock->mInstructions[i + 2]->mSTemp[1] == ins->mTTemp && iblock->mInstructions[i + 2]->mSFinal[1])
			{
				block->BinaryOperator(iproc, this, iblock->mInstructions[i + 2], ins, iblock->mInstructions[i + 1]);
				i += 2;
			}
			else if (i + 2 < iblock->mInstructions.Size() &&
				ins->mOperandSize >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LOAD && iblock->mInstructions[i + 1]->mOperandSize == 2 &&
				iblock->mInstructions[i + 2]->mCode == IC_BINARY_OPERATOR &&
				iblock->mInstructions[i + 2]->mSTemp[1] == iblock->mInstructions[i + 1]->mTTemp && iblock->mInstructions[i + 2]->mSFinal[1] &&
				iblock->mInstructions[i + 2]->mSTemp[0] == ins->mTTemp && iblock->mInstructions[i + 2]->mSFinal[0])
			{
				block->BinaryOperator(iproc, this, iblock->mInstructions[i + 2], iblock->mInstructions[i + 1], ins);
				i += 2;
			}
			else if (i + 1 < iblock->mInstructions.Size() &&
				ins->mOperandSize >= 2 &&
				iblock->mInstructions[i + 1]->mCode == IC_LEA &&
				iblock->mInstructions[i + 1]->mSTemp[1] == ins->mTTemp && iblock->mInstructions[i + 1]->mSFinal[1])
			{
				block->LoadEffectiveAddress(iproc, iblock->mInstructions[i + 1], ins, nullptr);
				i++;
			}
			else
				block->LoadValue(iproc, ins);
			break;
		case IC_COPY:
			block->CopyValue(iproc, ins, this);
			break;
		case IC_LOAD_TEMPORARY:
		{
			if (ins->mSTemp[0] != ins->mTTemp)
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 1));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp] + 1));
				if (ins->mSType[0] == IT_FLOAT)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp] + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 3));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp] + 3));
				}
			}
		}	break;
		case IC_BINARY_OPERATOR:
			block->BinaryOperator(iproc, this, ins, nullptr, nullptr);
			break;
		case IC_UNARY_OPERATOR:
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
		case IC_JSR:
			block->CallAssembler(iproc, ins);
			break;
		case IC_PUSH_FRAME:
		{
			block->mIns.Push(NativeCodeInstruction(ASMIT_SEC, ASMIM_IMPLIED));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, (ins->mIntValue + 2) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			block->mIns.Push(NativeCodeInstruction(ASMIT_SBC, ASMIM_IMMEDIATE, ((ins->mIntValue + 2) >> 8) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}	break;
		case IC_POP_FRAME:
		{
			block->mIns.Push(NativeCodeInstruction(ASMIT_CLC, ASMIM_IMPLIED));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, (ins->mIntValue + 2) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK));
			block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
			block->mIns.Push(NativeCodeInstruction(ASMIT_ADC, ASMIM_IMMEDIATE, ((ins->mIntValue + 2) >> 8) & 0xff));
			block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_STACK + 1));
		}	break;

		case IC_RELATIONAL_OPERATOR:
			if (iblock->mInstructions[i + 1]->mCode == IC_BRANCH)
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

				rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp]));
				if (InterTypeSize[ins->mTType] > 1)
				{
					rblock->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, 0));
					rblock->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp] + 1));
				}

				block = rblock;
			}
			break;

		case IC_RETURN_VALUE:
		{
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mSType[0] == IT_FLOAT)
				{
					union { float f; unsigned int v; } cc;
					cc.f = ins->mSFloatConst[0];

					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, cc.v & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 8) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 16) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (cc.v >> 24) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
				}
				else
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, ins->mSIntConst[0] & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 0));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_IMMEDIATE, (ins->mSIntConst[0] >> 8) & 0xff));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				}
			}				
			else
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU));
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 1));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 1));
				if (ins->mSType[0] == IT_FLOAT)
				{
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 2));
					block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 3));
					block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_ACCU + 3));
				}
			}

			block->Close(exitBlock, nullptr, ASMIT_JMP);
			return;
		}

		case IC_RETURN:
			block->Close(exitBlock, nullptr, ASMIT_JMP);
			return;

		case IC_TYPECAST:
			if (ins->mSTemp[0] != ins->mTTemp)
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp]));
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 1));
				block->mIns.Push(NativeCodeInstruction(ASMIT_STA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mTTemp] + 1));
			}
			break;

		case IC_BRANCH:
			if (ins->mSTemp[0] < 0)
			{
				if (ins->mSIntConst[0] == 0)
					block->Close(CompileBlock(iproc, iblock->mFalseJump), nullptr, ASMIT_JMP);
				else
					block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
			}
			else
			{
				block->mIns.Push(NativeCodeInstruction(ASMIT_LDA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]]));
				if (InterTypeSize[ins->mSType[0]] > 1)
					block->mIns.Push(NativeCodeInstruction(ASMIT_ORA, ASMIM_ZERO_PAGE, BC_REG_TMP + iproc->mTempOffset[ins->mSTemp[0]] + 1));

				block->Close(CompileBlock(iproc, iblock->mTrueJump), CompileBlock(iproc, iblock->mFalseJump), ASMIT_BNE);
			}
			return;

		}

		i++;
	}

	block->Close(CompileBlock(iproc, iblock->mTrueJump), nullptr, ASMIT_JMP);
}


NativeCodeGenerator::NativeCodeGenerator(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker), mRuntime({ 0 })
{
}

NativeCodeGenerator::~NativeCodeGenerator(void)
{

}

NativeCodeGenerator::Runtime& NativeCodeGenerator::ResolveRuntime(const Ident* ident)
{
	int	i = 0;
	while (i < mRuntime.Size() && mRuntime[i].mIdent != ident)
		i++;
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

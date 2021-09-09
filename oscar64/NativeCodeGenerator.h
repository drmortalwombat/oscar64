#pragma once

#include "ByteCodeGenerator.h"
#include "Assembler.h"

class NativeCodeProcedure;
class NativeCodeBasicBlock;

struct NativeRegisterData
{
	bool		mImmediate, mZeroPage;
	int			mValue;

	NativeRegisterData(void);

	void Reset(void);
};

struct NativeRegisterDataSet
{
	NativeRegisterData		mRegs[261];

	void Reset(void);
	void ResetZeroPage(int addr);
};

class NativeCodeInstruction
{
public:
	NativeCodeInstruction(AsmInsType type = ASMIT_INV, AsmInsMode mode = ASMIM_IMPLIED, int address = 0, int varIndex = -1, bool lower = true, bool upper = true);
	NativeCodeInstruction(const char* runtime);

	AsmInsType		mType;
	AsmInsMode		mMode;

	int				mAddress, mVarIndex;
	bool			mLower, mUpper;
	const char	*	mRuntime;

	void Assemble(NativeCodeBasicBlock* block);
	void FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	bool IsUsedResultInstructions(NumberSet& requiredTemps);
	bool ValueForwarding(NativeRegisterDataSet& data);
};

class NativeCodeBasicBlock
{
public:
	NativeCodeBasicBlock(void);
	~NativeCodeBasicBlock(void);

	DynamicArray<uint8>					mCode;
	int									mIndex;

	NativeCodeBasicBlock* mTrueJump, * mFalseJump;
	AsmInsType							mBranch;

	GrowingArray<NativeCodeInstruction>	mIns;
	GrowingArray<ByteCodeRelocation>	mRelocations;

	int						mOffset, mSize;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled, mNoFrame, mVisited;

	int PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, int offset);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);
	void CalculateOffset(int& total);

	void CopyCode(NativeCodeProcedure* proc, uint8* target);
	void Assemble(void);
	void Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	void PeepHoleOptimizer(void);

	void PutByte(uint8 code);
	void PutWord(uint16 code);

	void LoadValueToReg(InterCodeProcedure* proc, const InterInstruction& ins, int reg, const NativeCodeInstruction * ainsl, const NativeCodeInstruction* ainsh);

	void LoadConstant(InterCodeProcedure* proc, const InterInstruction& ins);
	void StoreValue(InterCodeProcedure* proc, const InterInstruction& ins);
	void LoadValue(InterCodeProcedure* proc, const InterInstruction& ins);
	void LoadStoreValue(InterCodeProcedure* proc, const InterInstruction& rins, const InterInstruction& wins);
	void BinaryOperator(InterCodeProcedure* proc, const InterInstruction& ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void UnaryOperator(InterCodeProcedure* proc, const InterInstruction& ins);
	void RelationalOperator(InterCodeProcedure* proc, const InterInstruction& ins, NativeCodeProcedure * nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock * falseJump);
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction& ins);
	void NumericConversion(InterCodeProcedure* proc, const InterInstruction& ins);

	bool CheckPredAccuStore(int reg);

	NumberSet		mLocalRequiredRegs, mLocalProvidedRegs;
	NumberSet		mEntryRequiredRegs, mEntryProvidedRegs;
	NumberSet		mExitRequiredRegs, mExitProvidedRegs;

	void BuildLocalRegSets(void);
	void BuildGlobalProvidedRegSet(NumberSet fromProvidedTemps);
	bool BuildGlobalRequiredRegSet(NumberSet& fromRequiredTemps);
	bool RemoveUnusedResultInstructions(void);
};

class NativeCodeProcedure
{
	public:
		NativeCodeProcedure(void);
		~NativeCodeProcedure(void);

		NativeCodeBasicBlock* entryBlock, * exitBlock;
		NativeCodeBasicBlock** tblocks;

		int		mProgStart, mProgSize, mIndex;
		bool	mNoFrame;
		int		mTempBlocks;

		GrowingArray<ByteCodeRelocation>	mRelocations;
		GrowingArray < NativeCodeBasicBlock*>	 mBlocks;

		void Compile( ByteCodeGenerator * generator, InterCodeProcedure* proc);
		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);
		NativeCodeBasicBlock* AllocateBlock(void);
		NativeCodeBasicBlock* TransientBlock(void);

		void CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock*block);

		void BuildDataFlowSets(void);
		void ResetVisited(void);

};


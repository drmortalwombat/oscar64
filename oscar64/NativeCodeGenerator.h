#pragma once

#include "Assembler.h"
#include "Linker.h"
#include "InterCode.h"

class NativeCodeProcedure;
class NativeCodeBasicBlock;
class NativeCodeGenerator;

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

static const uint32 NCIF_LOWER = 0x00000001;
static const uint32 NCIF_UPPER = 0x00000002;
static const uint32 NCIF_RUNTIME = 0x00000004;

class NativeCodeInstruction
{
public:
	NativeCodeInstruction(AsmInsType type = ASMIT_INV, AsmInsMode mode = ASMIM_IMPLIED, int address = 0, LinkerObject * linkerObject = nullptr, uint32 flags = NCIF_LOWER | NCIF_UPPER);

	AsmInsType		mType;
	AsmInsMode		mMode;

	int				mAddress;
	uint32			mFlags;
	uint32			mLive;
	LinkerObject*	mLinkerObject;

	void Assemble(NativeCodeBasicBlock* block);
	void FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	bool IsUsedResultInstructions(NumberSet& requiredTemps);
	bool ValueForwarding(NativeRegisterDataSet& data);

	bool LoadsAccu(void) const;
	bool ChangesAccuAndFlag(void) const;
	bool ChangesAddress(void) const;
	bool ChangesAccu(void) const;
	bool RequiresAccu(void) const;
	bool SameEffectiveAddress(const NativeCodeInstruction& ins) const;
	bool IsCommutative(void) const;
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
	GrowingArray<LinkerReference>	mRelocations;

	int						mOffset, mSize, mNumEntries;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled, mNoFrame, mVisited;

	int PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, int offset);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);
	void CalculateOffset(int& total);

	void CopyCode(NativeCodeProcedure* proc, uint8* target);
	void Assemble(void);
	void Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	bool PeepHoleOptimizer(void);

	void PutByte(uint8 code);
	void PutWord(uint16 code);

	void CheckFrameIndex(int & reg, int & index, int size);
	void LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction * ainsl, const NativeCodeInstruction* ainsh);
	void LoadConstantToReg(InterCodeProcedure* proc, const InterInstruction * ins, InterType type, int reg);

	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins);
	void BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void UnaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure * nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock * falseJump);
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void NumericConversion(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	NativeCodeBasicBlock * CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc);

	void CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);

	void ShiftRegisterLeft(InterCodeProcedure* proc, int reg, int shift);
	int ShortMultiply(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins, int index, int mul);

	bool CheckPredAccuStore(int reg);

	NumberSet		mLocalRequiredRegs, mLocalProvidedRegs;
	NumberSet		mEntryRequiredRegs, mEntryProvidedRegs;
	NumberSet		mExitRequiredRegs, mExitProvidedRegs;

	void BuildLocalRegSets(void);
	void BuildGlobalProvidedRegSet(NumberSet fromProvidedTemps);
	bool BuildGlobalRequiredRegSet(NumberSet& fromRequiredTemps);
	bool RemoveUnusedResultInstructions(void);

	void CountEntries(void);
	bool MergeBasicBlocks(void);

	bool MoveLoadStoreUp(int at);
	bool FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg);
	bool FindGlobalAddress(int at, int reg, int& apos);

	bool ValueForwarding(const NativeRegisterDataSet& data);

};

class NativeCodeProcedure
{
	public:
		NativeCodeProcedure(NativeCodeGenerator* generator);
		~NativeCodeProcedure(void);

		NativeCodeBasicBlock* entryBlock, * exitBlock;
		NativeCodeBasicBlock** tblocks;

		NativeCodeGenerator* mGenerator;

		int		mProgStart, mProgSize, mIndex;
		bool	mNoFrame;
		int		mTempBlocks;

		GrowingArray<LinkerReference>	mRelocations;
		GrowingArray < NativeCodeBasicBlock*>	 mBlocks;

		void Compile(InterCodeProcedure* proc);
		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);
		NativeCodeBasicBlock* AllocateBlock(void);
		NativeCodeBasicBlock* TransientBlock(void);

		void CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock*block);

		void BuildDataFlowSets(void);
		void ResetVisited(void);

};

class NativeCodeGenerator
{
public:
	NativeCodeGenerator(Errors * errors, Linker* linker);
	~NativeCodeGenerator(void);

	void RegisterRuntime(const Ident * ident, LinkerObject * object, int offset);

	struct Runtime
	{
		const Ident		*	mIdent;
		LinkerObject	*	mLinkerObject;
		int					mOffset;
	};

	Runtime& ResolveRuntime(const Ident* ident);

	Errors* mErrors;
	Linker* mLinker;
	GrowingArray<Runtime>	mRuntime;
};
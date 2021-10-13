#pragma once

#include "Assembler.h"
#include "Linker.h"
#include "InterCode.h"

class NativeCodeProcedure;
class NativeCodeBasicBlock;
class NativeCodeGenerator;

enum NativeRegisterDataMode
{
	NRDM_UNKNOWN,
	NRDM_IMMEDIATE,
	NRDM_IMMEDIATE_ADDRESS,
	NRDM_ZERO_PAGE
};

struct NativeRegisterData
{
	NativeRegisterDataMode	mMode;
	int						mValue;
	uint32					mFlags;
	LinkerObject		*	mLinkerObject;

	NativeRegisterData(void);

	void Reset(void);
};

struct NativeRegisterDataSet
{
	NativeRegisterData		mRegs[261];

	void Reset(void);
	void ResetZeroPage(int addr);
	void Intersect(const NativeRegisterDataSet& set);
};


static const uint32 NCIF_LOWER = 0x00000001;
static const uint32 NCIF_UPPER = 0x00000002;
static const uint32 NCIF_RUNTIME = 0x00000004;
static const uint32 NCIF_YZERO = 0x00000008;

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

	void Simulate(NativeRegisterDataSet& data);
	bool ApplySimulation(const NativeRegisterDataSet& data);

	bool LoadsAccu(void) const;
	bool ChangesAccuAndFlag(void) const;
	bool ChangesAddress(void) const;
	bool ChangesAccu(void) const;
	bool RequiresAccu(void) const;
	bool RequiresYReg(void) const;
	bool ChangesYReg(void) const;
	bool ChangesZeroPage(int address) const;
	bool UsesZeroPage(int address) const;
	bool ChangesGlobalMemory(void) const;
	bool SameEffectiveAddress(const NativeCodeInstruction& ins) const;
	bool IsSame(const NativeCodeInstruction& ins) const;
	bool IsCommutative(void) const;
};

class NativeCodeBasicBlock
{
public:
	NativeCodeBasicBlock(void);
	~NativeCodeBasicBlock(void);

	GrowingArray<uint8>					mCode;
	int									mIndex;

	NativeCodeBasicBlock* mTrueJump, * mFalseJump, * mFromJump;
	AsmInsType							mBranch;

	GrowingArray<NativeCodeInstruction>	mIns;
	GrowingArray<LinkerReference>	mRelocations;

	GrowingArray<NativeCodeBasicBlock*>	mEntryBlocks;

	int						mOffset, mSize, mNumEntries, mNumEntered, mFrameOffset;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled, mNoFrame, mVisited, mLoopHead, mVisiting;

	NativeRegisterDataSet	mDataSet, mNDataSet;

	int PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, int offset);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);
	void CalculateOffset(int& total);

	void CopyCode(NativeCodeProcedure* proc, uint8* target);
	void Assemble(void);
	void Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	bool PeepHoleOptimizer(void);
	bool OptimizeSimpleLoop(NativeCodeProcedure* proc);

	void PutByte(uint8 code);
	void PutWord(uint16 code);

	void CheckFrameIndex(int & reg, int & index, int size);
	void LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction * ainsl, const NativeCodeInstruction* ainsh);
	void LoadConstantToReg(InterCodeProcedure* proc, const InterInstruction * ins, InterType type, int reg);

	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins);
	void LoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, int oindex, const InterInstruction* wins);
	void LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins);
	NativeCodeBasicBlock* BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
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

	void CountEntries(NativeCodeBasicBlock* fromJump);
	bool MergeBasicBlocks(void);
	void MarkLoopHead(void);

	bool MoveLoadStoreUp(int at);
	bool MoveIndirectLoadStoreUp(int at);
	bool MoveAbsoluteLoadStoreUp(int at);
	bool FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg);
	bool FindGlobalAddress(int at, int reg, int& apos);
	bool FindGlobalAddressSumY(int at, int reg, const NativeCodeInstruction * & ains, int& ireg);

	bool ValueForwarding(const NativeRegisterDataSet& data);

	void CollectEntryBlocks(NativeCodeBasicBlock* block);

	bool JoinTailCodeSequences(void);
	bool SameTail(const NativeCodeInstruction& ins) const;

	NativeRegisterDataSet	mEntryRegisterDataSet;

	void BuildEntryDataSet(const NativeRegisterDataSet& set);
	bool ApplyEntryDataSet(void);
};

class NativeCodeProcedure
{
	public:
		NativeCodeProcedure(NativeCodeGenerator* generator);
		~NativeCodeProcedure(void);

		NativeCodeBasicBlock* entryBlock, * exitBlock;
		NativeCodeBasicBlock** tblocks;

		NativeCodeGenerator* mGenerator;

		int		mProgStart, mProgSize, mIndex, mFrameOffset;
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
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
	NRDM_ZERO_PAGE,
	NRDM_ABSOLUTE
};

struct NativeRegisterData
{
	NativeRegisterDataMode	mMode;
	int						mValue;
	uint32					mFlags;
	LinkerObject		*	mLinkerObject;

	NativeRegisterData(void);

	void Reset(void);

	bool SameData(const NativeRegisterData& d) const;
};

struct NativeRegisterDataSet
{
	NativeRegisterData		mRegs[261];

	void Reset(void);
	void ResetZeroPage(int addr);
	void ResetAbsolute(LinkerObject * linkerObject, int addr);
	void ResetIndirect(void);
	void Intersect(const NativeRegisterDataSet& set);
};


static const uint32 NCIF_LOWER = 0x00000001;
static const uint32 NCIF_UPPER = 0x00000002;
static const uint32 NCIF_RUNTIME = 0x00000004;
static const uint32 NCIF_YZERO = 0x00000008;
static const uint32 NCIF_VOLATILE = 0x00000010;
static const uint32 NCIF_LONG = 0x00000020;
static const uint32 NCIF_FEXEC = 0x00000040;

static const uint32 NCIF_USE_CPU_REG_A = 0x00001000;
static const uint32 NCIF_USE_CPU_REG_X = 0x00002000;
static const uint32 NCIF_USE_CPU_REG_Y = 0x00004000;

// use a 32bit zero page register indexed by X for JSR
static const uint32 NCIF_USE_ZP_32_X = 0x00008000;

class NativeCodeInstruction
{
public:
	NativeCodeInstruction(AsmInsType type = ASMIT_INV, AsmInsMode mode = ASMIM_IMPLIED, int address = 0, LinkerObject * linkerObject = nullptr, uint32 flags = NCIF_LOWER | NCIF_UPPER, int param = 0);

	AsmInsType		mType;
	AsmInsMode		mMode;

	int				mAddress, mParam;
	uint32			mFlags;
	uint32			mLive;
	LinkerObject*	mLinkerObject;

	void CopyMode(const NativeCodeInstruction& ins);

	void Assemble(NativeCodeBasicBlock* block);
	void FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	bool IsUsedResultInstructions(NumberSet& requiredTemps);
	bool ValueForwarding(NativeRegisterDataSet& data, AsmInsType & carryop);

	void Simulate(NativeRegisterDataSet& data);
	bool ApplySimulation(const NativeRegisterDataSet& data);

	bool LoadsAccu(void) const;
	bool ChangesAccuAndFlag(void) const;
	bool ChangesAddress(void) const;
	bool ChangesAccu(void) const;
	bool UsesAccu(void) const;
	bool ChangesCarry(void) const;
	bool ChangesZFlag(void) const;
	bool RequiresCarry(void) const;
	bool RequiresAccu(void) const;
	bool RequiresYReg(void) const;
	bool RequiresXReg(void) const;
	bool ChangesYReg(void) const;
	bool ChangesXReg(void) const;
	bool ChangesZeroPage(int address) const;
	bool UsesZeroPage(int address) const;
	bool ChangesGlobalMemory(void) const;
	bool SameEffectiveAddress(const NativeCodeInstruction& ins) const;
	bool MayBeChangedOnAddress(const NativeCodeInstruction& ins) const;
	bool IsSame(const NativeCodeInstruction& ins) const;
	bool IsCommutative(void) const;
	bool IsShift(void) const;

	void ReplaceYRegWithXReg(void);
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

	int						mOffset, mSize, mPlace, mNumEntries, mNumEntered, mFrameOffset;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled, mNoFrame, mVisited, mLoopHead, mVisiting, mLocked, mPatched;
	NativeCodeBasicBlock* mLoopHeadBlock;

	NativeRegisterDataSet	mDataSet, mNDataSet;

	int PutBranch(NativeCodeProcedure* proc, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, NativeCodeBasicBlock* target);
	int JumpByteSize(NativeCodeBasicBlock * target);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);

	int LeadsInto(NativeCodeBasicBlock* block, int dist);
	void BuildPlacement(GrowingArray<NativeCodeBasicBlock*>& placement);
	void InitialOffset(int& total);
	bool CalculateOffset(int& total);

	void CopyCode(NativeCodeProcedure* proc, uint8* target);
	void Assemble(void);
	void Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	bool RemoveNops(void);
	bool PeepHoleOptimizer(NativeCodeProcedure* proc, int pass);
	void BlockSizeReduction(void);

	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc);
	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, NativeCodeBasicBlock * prevBlock, NativeCodeBasicBlock* exitBlock);

	bool OptimizeSimpleLoop(NativeCodeProcedure* proc);
	bool SimpleLoopReversal(NativeCodeProcedure* proc);
	bool OptimizeInnerLoop(NativeCodeProcedure* proc, NativeCodeBasicBlock* head, NativeCodeBasicBlock* tail, GrowingArray<NativeCodeBasicBlock*>& blocks);

	bool OptimizeSelect(NativeCodeProcedure* proc);

	NativeCodeBasicBlock* FindTailBlock(NativeCodeBasicBlock* head);
	bool OptimizeInnerLoops(NativeCodeProcedure* proc);
	void CollectInnerLoop(NativeCodeBasicBlock* head, GrowingArray<NativeCodeBasicBlock*>& lblocks);

	void PutByte(uint8 code);
	void PutWord(uint16 code);

	void CheckFrameIndex(int & reg, int & index, int size, int treg = 0);
	void LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction * ainsl, const NativeCodeInstruction* ainsh);
	void LoadConstantToReg(InterCodeProcedure* proc, const InterInstruction * ins, InterType type, int reg);

	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins);
	bool LoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, int oindex, const InterInstruction* wins);
	bool LoadUnopStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, const InterInstruction* wins);
	bool LoadLoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins1, const InterInstruction* rins0, const InterInstruction* oins, const InterInstruction* wins);
	void LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins);
	NativeCodeBasicBlock* BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void UnaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure * nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock * falseJump);
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void NumericConversion(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	NativeCodeBasicBlock * CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc);
	NativeCodeBasicBlock * StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);

	void CallAssembler(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);

	void ShiftRegisterLeft(InterCodeProcedure* proc, int reg, int shift);
	void ShiftRegisterLeftByte(InterCodeProcedure* proc, int reg, int shift);
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
	bool MoveLoadStoreXUp(int at);
	bool MoveLoadImmStoreAbsoluteUp(int at);

	bool MoveIndirectLoadStoreDown(int at);

	bool MoveIndirectLoadStoreUp(int at);
	bool MoveAbsoluteLoadStoreUp(int at);
	bool MoveLoadStoreOutOfXYRangeUp(int at);
	bool MoveLoadAddImmStoreUp(int at);
	bool MoveCLCLoadAddZPStoreUp(int at);
	bool MoveLoadAddZPStoreUp(int at);
	bool MoveCLCLoadAddZPStoreDown(int at);
	bool FindDirectAddressSumY(int at, int reg, int& apos, int& breg);
	bool PatchDirectAddressSumY(int at, int reg, int apos, int breg);
	bool FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg);
	bool PatchAddressSumY(int at, int reg, int apos, int breg, int ireg);
	bool FindGlobalAddress(int at, int reg, int& apos);
	bool FindGlobalAddressSumY(int at, int reg, bool direct, int& apos, const NativeCodeInstruction * & ains, const NativeCodeInstruction*& iins, uint32 & flags, int & addr);
	bool FindExternAddressSumY(int at, int reg, int& breg, int& ireg);
	bool FindPageStartAddress(int at, int reg, int& addr);
	bool MoveStoreXUp(int at);
	bool MoveLoadXUp(int at);
	bool MoveStoreYUp(int at);
	bool MoveLoadYUp(int at);
	bool MoveStoreHighByteDown(int at);
	bool MoveAddHighByteDown(int at);
	bool ReverseLoadCommutativeOpUp(int aload, int aop);
	bool ReplaceZeroPageUp(int at);
	bool ReplaceYRegWithXReg(int start, int end);

	bool FindImmediateStore(int at, int reg, const NativeCodeInstruction*& ains);

	bool JoinTAXARange(int from, int to);
	bool JoinTAYARange(int from, int to);
	bool PatchGlobalAdressSumYByX(int at, int reg, const NativeCodeInstruction& ains, int addr);

	bool ValueForwarding(const NativeRegisterDataSet& data, bool global);

	void CollectEntryBlocks(NativeCodeBasicBlock* block);

	void AddEntryBlock(NativeCodeBasicBlock* block);
	void RemEntryBlock(NativeCodeBasicBlock* block);

	NativeCodeBasicBlock * SplitMatchingTails(NativeCodeProcedure* proc);

	bool JoinTailCodeSequences(NativeCodeProcedure* proc);
	bool SameTail(const NativeCodeInstruction& ins) const;

	NativeRegisterDataSet	mEntryRegisterDataSet;

	void BuildEntryDataSet(const NativeRegisterDataSet& set);
	bool ApplyEntryDataSet(void);

	void CollectZeroPageSet(ZeroPageSet& locals, ZeroPageSet& global);
	void CollectZeroPageUsage(NumberSet& used, NumberSet& modified, NumberSet& pairs);
	void FindZeroPageAlias(const NumberSet& statics, NumberSet& invalid, uint8* alias, int accu);
	bool RemapZeroPage(const uint8* remap);

	void GlobalRegisterXYCheck(int* xregs, int * yregs);
	void GlobalRegisterXMap(int reg);
	void GlobalRegisterYMap(int reg);
	bool LocalRegisterXYMap(void);
	bool ReduceLocalYPressure(void);

	bool CheckGlobalAddressSumYPointer(int reg, int at, int yval);
	bool PatchGlobalAddressSumYPointer(int reg, int at, int yval, LinkerObject * lobj, int address);
};

class NativeCodeProcedure
{
	public:
		NativeCodeProcedure(NativeCodeGenerator* generator);
		~NativeCodeProcedure(void);

		NativeCodeBasicBlock* mEntryBlock, * mExitBlock;
		NativeCodeBasicBlock** tblocks;

		NativeCodeGenerator* mGenerator;

		InterCodeProcedure* mInterProc;

		int		mProgStart, mProgSize, mIndex, mFrameOffset, mStackExpand;
		bool	mNoFrame;
		int		mTempBlocks;

		GrowingArray<LinkerReference>	mRelocations;
		GrowingArray < NativeCodeBasicBlock*>	 mBlocks;

		void Compile(InterCodeProcedure* proc);
		void Optimize(void);

		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);
		NativeCodeBasicBlock* AllocateBlock(void);

		void CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock*block);

		bool MapFastParamsToTemps(void);
		void CompressTemporaries(void);

		void BuildDataFlowSets(void);
		void ResetEntryBlocks(void);
		void ResetVisited(void);
		void ResetPatched(void);

};

class NativeCodeGenerator
{
public:
	NativeCodeGenerator(Errors * errors, Linker* linker, LinkerSection * runtimeSection);
	~NativeCodeGenerator(void);

	void RegisterRuntime(const Ident * ident, LinkerObject * object, int offset);
	void CompleteRuntime(void);

	uint64		mCompilerOptions;

	struct Runtime
	{
		const Ident		*	mIdent;
		LinkerObject	*	mLinkerObject;
		int					mOffset;
	};

	struct MulTable
	{
		LinkerObject* mLinkerLSB, * mLinkerMSB;
		int	mFactor, mSize;
	};

	LinkerObject* AllocateShortMulTable(int factor, int size, bool msb);

	Runtime& ResolveRuntime(const Ident* ident);

	Errors* mErrors;
	Linker* mLinker;
	LinkerSection* mRuntimeSection;

	GrowingArray<Runtime>	mRuntime;
	GrowingArray<MulTable>	mMulTables;
};
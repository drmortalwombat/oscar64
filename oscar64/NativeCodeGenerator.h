#pragma once

#include "Assembler.h"
#include "Linker.h"
#include "InterCode.h"

class NativeCodeProcedure;
class NativeCodeBasicBlock;
class NativeCodeGenerator;
class NativeCodeInstruction;

enum NativeRegisterDataMode
{
	NRDM_UNKNOWN,
	NRDM_IMMEDIATE,
	NRDM_IMMEDIATE_ADDRESS,
	NRDM_ZERO_PAGE,
	NRDM_ABSOLUTE,
	NRDM_ABSOLUTE_X,
	NRDM_ABSOLUTE_Y,
	NRDM_INDIRECT_Y
};

struct NativeRegisterData
{
	NativeRegisterDataMode	mMode;
	int						mValue, mMask;
	uint32					mFlags;
	LinkerObject		*	mLinkerObject;

	NativeRegisterData(void);

	void Reset(void);
	void ResetMask(void);
	void ResetAliasing(void);

	bool SameData(const NativeRegisterData& d) const;
	bool SameData(const NativeCodeInstruction& ins) const;
};

struct NativeRegisterDataSet
{
	NativeRegisterData		mRegs[261];

	void Reset(void);
	void ResetMask(void);
	void ResetCall(const NativeCodeInstruction & ins, int fastCallBase);

	void ResetZeroPage(int addr);
	void ResetZeroPageRange(int addr, int num);
	void ResetAbsolute(LinkerObject * linkerObject, int addr);
	void ResetAbsoluteXY(LinkerObject* linkerObject, int addr);
	int FindAbsolute(LinkerObject* linkerObject, int addr);
	void ResetIndirect(int reg);
	void ResetX(void);
	void ResetY(void);
	void ResetWorkRegs(void);
	void ResetWorkMasks(void);
	void Intersect(const NativeRegisterDataSet& set);
	void IntersectMask(const NativeRegisterDataSet& set);
	void ResetAliasing(void);
};

struct ValueNumberingData
{
	uint32				mIndex, mOffset;

	ValueNumberingData(void);

	void Reset(void);
	bool SameBase(const  ValueNumberingData& d) const;
};

struct ValueNumberingDataSet
{
	ValueNumberingData	mRegs[261];

	void Reset(void);
	void ResetWorkRegs(void);
	void ResetCall(const NativeCodeInstruction& ins);

	void Intersect(const ValueNumberingDataSet& set);
};

struct NativeRegisterSum16Info
{
	NativeCodeInstruction	*	mSrcL, * mSrcH, * mDstL, * mDstH, * mAddL, * mAddH;

	bool						mImmediate;
	int							mAddress;
	LinkerObject			*	mLinkerObject;

	bool operator==(const NativeRegisterSum16Info& ri) const;
	bool operator!=(const NativeRegisterSum16Info& ri) const;
};

struct NativeSimpleSubExpression
{
	AsmInsType						mType;
	AsmInsMode						mMode;
	int								mValue;
	NativeCodeInstruction		*	mSrc, * mDst, * mOp;
	int								mIndex;

	bool MayBeChangedBy(const NativeCodeInstruction& ins);
};

struct NativeSimpleSubExpressions
{
	ExpandingArray<NativeSimpleSubExpression>	mExps;

	int FindCommon(const NativeSimpleSubExpression& ex) const;
	void Filter(const NativeCodeInstruction& ins);
};


static const uint32 NCIF_LOWER = 0x00000001;
static const uint32 NCIF_UPPER = 0x00000002;
static const uint32 NCIF_RUNTIME = 0x00000004;
static const uint32 NCIF_YZERO = 0x00000008;
static const uint32 NCIF_VOLATILE = 0x00000010;
static const uint32 NCIF_LONG = 0x00000020;
static const uint32 NCIF_FEXEC = 0x00000040;
static const uint32 NCIF_JSRFLAGS = 0x00000080;
static const uint32 NICT_INDEXFLIPPED = 0x00000100;
static const uint32 NICT_ZPFLIPPED = 0x00000200;
static const uint32 NICF_TMPREF = 0x00000400;
static const uint32 NCIF_ALIASING = 0x00000800;

static const uint32 NCIF_USE_CPU_REG_A = 0x00001000;
static const uint32 NCIF_USE_CPU_REG_X = 0x00002000;
static const uint32 NCIF_USE_CPU_REG_Y = 0x00004000;

// use a 32bit zero page register indexed by X for JSR
static const uint32 NCIF_USE_ZP_32_X = 0x00008000;
static const uint32 NICF_USE_ZP_ADDR = 0x00010000;
static const uint32 NICF_USE_WORKREGS = 0x00020000;

static const uint32 NCIF_BREAKPOINT = 0x00040000;

class NativeCodeInstruction
{
public:
	NativeCodeInstruction(void);
	NativeCodeInstruction(const InterInstruction * ins, AsmInsType type, AsmInsMode mode = ASMIM_IMPLIED, int64 address = 0, LinkerObject * linkerObject = nullptr, uint32 flags = NCIF_LOWER | NCIF_UPPER, int param = 0);
	NativeCodeInstruction(const InterInstruction* ins, AsmInsType type, const NativeCodeInstruction & addr);

	AsmInsType				mType;
	AsmInsMode				mMode;

	int						mAddress, mParam;
	uint32					mFlags;
	uint32					mLive;
	LinkerObject		*	mLinkerObject;
	const InterInstruction	*	mIns;

	void CopyMode(const NativeCodeInstruction& ins);

	void Assemble(NativeCodeBasicBlock* block);
	void FilterRegUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	bool IsUsedResultInstructions(NumberSet& requiredTemps);
	bool BitFieldForwarding(NativeRegisterDataSet& data, AsmInsType& carryop);
	bool ValueForwarding(NativeRegisterDataSet& data, AsmInsType & carryop, bool initial, bool final, int fastCallBase);

	void Simulate(NativeRegisterDataSet& data);
	bool ApplySimulation(const NativeRegisterDataSet& data);

	bool LoadsAccu(void) const;
	bool ChangesAccuAndFlag(void) const;
	bool ChangesAddress(void) const;
	bool UsesAddress(void) const;
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

	bool ReferencesCarry(void) const;
	bool ReferencesAccu(void) const;
	bool ReferencesYReg(void) const;
	bool ReferencesXReg(void) const;

	bool ChangesZeroPage(int address) const;
	bool UsesZeroPage(int address) const;
	bool ReferencesZeroPage(int address) const;


	bool ChangesGlobalMemory(void) const;
	bool UsesMemoryOf(const NativeCodeInstruction& ins) const;
	bool SameEffectiveAddress(const NativeCodeInstruction& ins) const;
	bool MayBeChangedOnAddress(const NativeCodeInstruction& ins, bool sameXY = false) const;
	bool MayReference(const NativeCodeInstruction& ins, bool sameXY = false) const;
	bool MayBeSameAddress(const NativeCodeInstruction& ins, bool sameXY = false) const;
	bool IsSame(const NativeCodeInstruction& ins) const;
	bool IsSameLS(const NativeCodeInstruction& ins) const;
	bool IsCommutative(void) const;
	bool IsLogic(void) const;
	bool IsShift(void) const;
	bool IsShiftOrInc(void) const;
	bool IsSimpleJSR(void) const;
	bool MayBeMovedBefore(const NativeCodeInstruction& ins);

	bool ReplaceYRegWithXReg(void);
	bool ReplaceXRegWithYReg(void);

	bool CanSwapXYReg(void);
	bool SwapXYReg(void);

	void BuildCollisionTable(NumberSet& liveTemps, NumberSet* collisionSets);

	uint32 NeedsLive(void) const;
};

struct NativeCodeLoadStorePair
{
	NativeCodeInstruction	mLoad, mStore;
};


class NativeCodeBasicBlock
{
public:
	NativeCodeBasicBlock(NativeCodeProcedure * proc);
	~NativeCodeBasicBlock(void);

	NativeCodeProcedure				*	mProc;
	ExpandingArray<uint8>				mCode;
	ExpandingArray<CodeLocation>		mCodeLocations;

	int									mIndex;

	NativeCodeBasicBlock* mTrueJump, * mFalseJump, * mFromJump;
	AsmInsType							mBranch;
	const InterInstruction*				mBranchIns;

	ExpandingArray<NativeCodeInstruction>	mIns;
	ExpandingArray<LinkerReference>	mRelocations;

	ExpandingArray<NativeCodeBasicBlock*>	mEntryBlocks;

	int							mOffset, mSize, mPlace, mNumEntries, mNumEntered, mFrameOffset, mTemp;
	bool						mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled, mNoFrame, mVisited, mLoopHead, mVisiting, mLocked, mPatched, mPatchFail, mPatchChecked, mPatchStart, mPatchLoop, mPatchLoopChanged, mPatchExit;
	bool						mEntryRegA, mEntryRegX, mEntryRegY, mExitRegA, mExitRegX, mChecked;
	NativeCodeBasicBlock	*	mDominator, * mSameBlock;

	NativeCodeBasicBlock* mLoopHeadBlock, * mLoopTailBlock;

	NativeRegisterDataSet	mDataSet, mNDataSet, mFDataSet;
	ValueNumberingDataSet	mNumDataSet, mNNumDataSet, mFNumDataSet;

	int						mYAlias[256];
	int						mYReg, mYOffset, mYValue, mXReg, mXOffset, mXValue;

	ExpandingArray<NativeRegisterSum16Info>	mRSumInfos;

	NativeCodeInstruction		mALSIns, mXLSIns, mYLSIns;

	NativeCodeInstruction DecodeNative(const InterInstruction* ins, LinkerObject * lobj, int& offset) const;

	int PutBranch(NativeCodeProcedure* proc, NativeCodeBasicBlock* target, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, NativeCodeBasicBlock* target, int offset, AsmInsType code = ASMIT_INV);
	int JumpByteSize(NativeCodeBasicBlock * target, int offset, bool second);
	int BranchByteSize(NativeCodeBasicBlock* target, int from, int to);

	NativeCodeBasicBlock* SplitAt(int at);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);

	int LeadsInto(NativeCodeBasicBlock* block, int dist);
	void BuildPlacement(ExpandingArray<NativeCodeBasicBlock*>& placement);
	void InitialOffset(int& total);
	bool CalculateOffset(int& total);

	void CopyCode(NativeCodeProcedure* proc, uint8* target);
	void Assemble(void);
	void Close(const InterInstruction * ins, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	void PrependInstruction(const NativeCodeInstruction& ins);

	void ShortcutTailRecursion();
	void ShortcutJump(int offset);

	bool ReferencesAccu(int from = 0, int to = 65536) const;
	bool ReferencesYReg(int from = 0, int to = 65536) const;
	bool ReferencesXReg(int from = 0, int to = 65536) const;

	bool ChangesAccu(int from = 0, int to = 65536) const;
	bool ChangesYReg(int from = 0, int to = 65536) const;
	bool ChangesXReg(int from = 0, int to = 65536) const;

	bool ChangesZeroPage(int address, int from = 0, int to = 65536) const;
	bool UsesZeroPage(int address, int from = 0, int to = 65536) const;
	bool ReferencesZeroPage(int address, int from = 0, int to = 65536) const;


	bool RemoveNops(void);
	bool PeepHoleOptimizer(int pass);

	bool PeepHoleOptimizerShuffle(int pass);
	bool PeepHoleOptimizerIterate1(int i, int pass);
	bool PeepHoleOptimizerIterate2(int i, int pass);
	bool PeepHoleOptimizerIterate3(int i, int pass);
	bool PeepHoleOptimizerIterate4(int i, int pass);
	bool PeepHoleOptimizerIterate5(int i, int pass);
	bool PeepHoleOptimizerIterate4b(int i, int pass);
	bool PeepHoleOptimizerIterate5b(int i, int pass);
	bool PeepHoleOptimizerIterate6(int i, int pass);
	bool PeepHoleOptimizerIterateN(int i, int pass);

	bool PeepHoleOptimizerIterate(int pass);
	bool PeepHoleOptimizerExits(int pass);

	void BlockSizeReduction(NativeCodeProcedure* proc, int xenter, int yenter);
	bool BlockSizeCopyReduction(NativeCodeProcedure* proc, int & si, int & di);

	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, bool full);
	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, NativeCodeBasicBlock * prevBlock, NativeCodeBasicBlock* exitBlock, bool full);
	bool RemoveSimpleLoopUnusedIndex(void);
	bool OptimizeLoopCarryOver(void);

	bool OptimizeSingleEntryLoopInvariant(NativeCodeProcedure* proc, NativeCodeBasicBlock* prev, NativeCodeBasicBlock* tail, ExpandingArray<NativeCodeBasicBlock*>& blocks);
	bool OptimizeSingleEntryLoop(NativeCodeProcedure* proc);

	bool OptimizeSimpleLoop(NativeCodeProcedure* proc, bool full);
	bool SimpleLoopReversal(NativeCodeProcedure* proc);
	bool OptimizeInnerLoop(NativeCodeProcedure* proc, NativeCodeBasicBlock* head, NativeCodeBasicBlock* tail, ExpandingArray<NativeCodeBasicBlock*>& blocks);
	bool OptimizeXYSimpleLoop(void);

	bool OptimizeXYSpilling(void);

	bool OptimizeSelect(NativeCodeProcedure* proc);

	bool OptimizeInnerLoops(NativeCodeProcedure* proc);
	NativeCodeBasicBlock* CollectInnerLoop(NativeCodeBasicBlock* head, ExpandingArray<NativeCodeBasicBlock*>& lblocks);

	bool OptimizeGenericLoop(NativeCodeProcedure* proc);
	bool CollectGenericLoop(NativeCodeProcedure* proc, ExpandingArray<NativeCodeBasicBlock*>& lblocks);
	bool CollectSingleEntryGenericLoop(NativeCodeProcedure* proc, ExpandingArray<NativeCodeBasicBlock*>& lblocks);
	void CollectReachable(ExpandingArray<NativeCodeBasicBlock*>& lblock);

	bool OptimizeFindLoop(NativeCodeProcedure* proc);

	NativeCodeBasicBlock* BuildSingleEntry(NativeCodeProcedure* proc, NativeCodeBasicBlock* block);
	NativeCodeBasicBlock* BuildSingleExit(NativeCodeProcedure* proc, NativeCodeBasicBlock* block);

	void PutLocation(const Location& loc, bool weak);
	void PutOpcode(short opcode);
	void PutByte(uint8 code);
	void PutWord(uint16 code);

	void CheckFrameIndex(const InterInstruction * ins, int & reg, int & index, int size, int treg = 0);
	void LoadValueToReg(InterCodeProcedure* proc, const InterInstruction * ins, int reg, const NativeCodeInstruction * ainsl, const NativeCodeInstruction* ainsh);
	void LoadConstantToReg(InterCodeProcedure* proc, const InterInstruction* ins, InterType type, int reg);
	void LoadConstantToReg(InterCodeProcedure* proc, const InterInstruction* ins, const InterOperand & op, InterType type, int reg);

	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadStoreValue(InterCodeProcedure* proc, const InterInstruction * rins, const InterInstruction * wins);
	void LoadStoreIndirectPair(InterCodeProcedure* proc, const InterInstruction* wins0, const InterInstruction* wins1);
	bool LoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, int oindex, const InterInstruction* wins);
	bool LoadUnopStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* oins, const InterInstruction* wins);
	bool LoadLoadOpStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins1, const InterInstruction* rins0, const InterInstruction* oins, const InterInstruction* wins);
	void LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins);
	void StoreByteOffsetIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* sins);
	void StoreAbsoluteByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* wins);
	void LoadAbsoluteByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* rins);

	NativeCodeBasicBlock* BinaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0);
	void UnaryOperator(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure * nproc, NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock * falseJump);
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0, bool addrvalid);
	void LoadStoreOpAbsolute2D(InterCodeProcedure* proc, const InterInstruction* lins1, const InterInstruction* lins2, const InterInstruction* mins);
	void SignExtendAddImmediate(InterCodeProcedure* proc, const InterInstruction* xins, const InterInstruction* ains);
	void BinaryDivModPair(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction* ins1, const InterInstruction* ins2);
	void BinaryFloatOperatorLookup(InterCodeProcedure* proc, const InterInstruction* cins, const InterInstruction* ins);

	void NumericConversion(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	NativeCodeBasicBlock * FillValue(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);
	NativeCodeBasicBlock * CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc);
	NativeCodeBasicBlock * StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);
	void AddAsrSignedByte(InterCodeProcedure* proc, const InterInstruction* ains, const InterInstruction* sins);

	void CallMalloc(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);
	void CallFree(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);

	void LoadByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* rins);
	void StoreByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* rins);

	void CallAssembler(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);

	void ShiftRegisterRight( const InterInstruction* ins, int reg, int shift);
	void ShiftRegisterLeft(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift);
	void ShiftRegisterLeftByte(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift);
	void ShiftRegisterLeftFromByte(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift, int max);
	int ShortMultiply(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins, int index, int mul);
	int ShortSignedDivide(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction* ins, const InterInstruction* sins, int mul);

	bool CheckPredAccuStore(int reg);

	NumberSet		mLocalRequiredRegs, mLocalProvidedRegs;
	NumberSet		mEntryRequiredRegs, mEntryProvidedRegs;
	NumberSet		mExitRequiredRegs, mExitProvidedRegs;
	NumberSet		mNewRequiredRegs;
	NumberSet		mTempRegs;

	void BuildLocalRegSets(void);
	void BuildGlobalProvidedRegSet(NumberSet fromProvidedTemps);
	bool BuildGlobalRequiredRegSet(NumberSet& fromRequiredTemps);
	bool RemoveUnusedResultInstructions(void);

	void BuildCollisionTable(NumberSet* collisionSets);

	bool IsSame(const NativeCodeBasicBlock* block) const;
	bool FindSameBlocks(NativeCodeProcedure* nproc);
	bool MergeSameBlocks(NativeCodeProcedure* nproc);

	void CountEntries(NativeCodeBasicBlock* fromJump);
	NativeCodeBasicBlock * ForwardAccuBranch(bool eq, bool ne, bool pl, bool mi, int limit);
	bool MergeBasicBlocks(void);
	void RemoveJumpToBranch(void);

	void MarkLoopHead(void);

	struct DominatorStacks
	{
		ExpandingArray< NativeCodeBasicBlock* >	d1, d2;
	};

	void BuildDominatorTree(NativeCodeBasicBlock * from, DominatorStacks & stacks);

	bool MoveLoadStoreUp(int at);
	bool MoveLoadStoreXUp(int at);
	bool MoveLoadImmStoreAbsoluteUp(int at);

	bool MoveIndirectLoadStoreDown(int at);
	bool MoveIndirectLoadStoreDownY(int at);

	bool MoveLDXUp(int at);
	bool MoveLDYUp(int at);

	bool MoveLDXBeforeZ(int at);

	bool MoveIndirectLoadStoreUp(int at);
	bool MoveAbsoluteLoadStoreUp(int at);
	bool MoveLoadStoreOutOfXYRangeUp(int at);
	bool MoveLoadIndirectTempStoreUp(int at);
	bool MoveLoadIndirectBypassYUp(int at);

	bool MoveLoadAddImmStoreAbsXUp(int at);
	bool MoveStaTaxLdaStaDown(int at);

	bool MoveLoadAddImmStoreUp(int at);
	bool MoveLoadEorImmStoreUp(int at);
	bool MoveCLCLoadAddZPStoreUp(int at);
	bool MoveLoadAddZPStoreUp(int at);
	bool MoveLoadShiftRotateUp(int at);
	bool MoveLoadShiftStoreUp(int at);
	bool MoveTYADCStoreDown(int at);
	bool MoveShiftZeroPageUp(int at);

	bool MoveLoadLogicStoreAbsUp(int at);

	bool MoveLDSTXOutOfRange(int at);

	bool MoveCLCLoadAddZPStoreDown(int at);
	bool FindDirectAddressSumY(int at, int reg, int& apos, int& breg);
	bool PatchDirectAddressSumY(int at, int reg, int apos, int breg);
	bool FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg);
	bool PatchAddressSumY(int at, int reg, int apos, int breg, int ireg);

	bool FindLoadAddressSumY(int at, int reg, int& apos, int& ireg);
	bool PatchLoadAddressSumY(int at, int reg, int apos, int ireg);

	bool FindGlobalAddress(int at, int reg, int& apos);
	bool FindGlobalAddressSumY(int at, int reg, bool direct, int& apos, const NativeCodeInstruction * & ains, const NativeCodeInstruction*& iins, uint32 & flags, int & addr);
	bool FindExternAddressSumY(int at, int reg, int& breg, int& ireg);
	bool FindSharedGlobalAddressSumY(int at, int reg, const NativeCodeInstruction*& ains, const NativeCodeInstruction*& iins);
	bool FindPageStartAddress(int at, int reg, int& addr);
	bool FindBypassAddressSumY(int at, int reg, int& apos, int& breg);
	bool PatchBypassAddressSumY(int at, int reg, int apos, int breg);
	bool MoveStoreXUp(int at);
	bool MoveLoadXUp(int at);
	bool MoveStoreYUp(int at);
	bool MoveLoadYUp(int at);
	bool MoveStoreHighByteDown(int at);
	bool MoveAddHighByteDown(int at);
	bool ReverseLoadCommutativeOpUp(int aload, int aop);
	bool ReplaceZeroPageUp(int at);
	bool ReplaceZeroPageDown(int at);
	bool ReplaceYRegWithXReg(int start, int end);
	bool ReplaceXRegWithYReg(int start, int end);
	bool MoveASLMemUp(int start);
	bool CombineImmediateADCUp(int at);
	bool CombineImmediateADCUpX(int at);
	bool MoveTXADCDown(int at);
	bool FoldShiftORAIntoLoadImmUp(int at);

	bool FindAccuExitValue(int& at);
	bool MoveLoadXAbsUpCrossBlock(int at);

	bool MoveSimpleADCToINCDECDown(int at);
	bool MoveTAXADCSTADown(int at);


	bool MoveZeroPageCrossBlockUp(int at, const NativeCodeInstruction & lins, const NativeCodeInstruction & sins);
	bool ShortcutCrossBlockMoves(NativeCodeProcedure* proc);
	bool ShortcutCrossBlockCondition(void);

	bool CanReplaceYRegWithXReg(int start, int end);
	bool CanReplaceXRegWithYReg(int start, int end);

	bool ForwardAccuAddSub(void);
	bool ForwardZpYIndex(bool full);
	bool ForwardZpXIndex(bool full);
	bool ForwardAXYReg(void);

	// Join sequences of TXA, CLC, ADC #xx into INX, TXA sequences if possible
	bool JoinXYCascade(void);

	bool GlobalLoadStoreForwarding(bool zpage, const NativeCodeInstruction & als, const NativeCodeInstruction & xls, const NativeCodeInstruction & yls);

	bool RegisterValueForwarding(void);
	bool CanCombineSameXtoY(int start, int end);
	bool CanCombineSameYtoX(int start, int end);
	bool CombineSameXY(void);
	bool CombineSameXtoY(int xpos, int ypos, int end);
	bool CombineSameYtoX(int xpos, int ypos, int end);

	bool FindImmediateStore(int at, int reg, const NativeCodeInstruction*& ains);

	bool JoinXYCrossBlock(void);
	bool CanCombineSameXtoYCrossBlock(int from);
	bool CanCombineSameYtoXCrossBlock(int from);
	bool CombineSameXtoYCrossBlock(int from);
	bool CombineSameYtoXCrossBlock(int from);

	bool JoinTAXARange(int from, int to);
	bool JoinTAYARange(int from, int to);
	bool PatchGlobalAdressSumYByX(int at, int reg, const NativeCodeInstruction& ains, int addr);
	bool MergeXYSameValue(int from);
	void RepairLoadYImmediate(int at, int val);
	int RetrieveAValue(int at) const;
	int RetrieveXValue(int at) const;
	int RetrieveYValue(int at) const;
	int RetrieveZPValue(int reg, int at) const;
	int FindFreeAccu(int at) const;

	bool ReverseReplaceTAX(int at);

	void ResetModifiedDataSet(NativeRegisterDataSet& data);

	bool ValueForwarding(NativeCodeProcedure* proc, const NativeRegisterDataSet& data, bool global, bool final);
	bool GlobalValueForwarding(NativeCodeProcedure* proc, bool final);
	bool BitFieldForwarding(const NativeRegisterDataSet& data);
	bool ReverseBitfieldForwarding(void);
	bool OffsetValueForwarding(const ValueNumberingDataSet & data);
	bool AbsoluteValueForwarding(const ExpandingArray<NativeCodeLoadStorePair>& npairs);
	bool IndexXYValueForwarding(int xreg, int xoffset, int xvalue, int yreg, int yoffset, int yvalue);

	void MarkLocalUsedLinkerObjects(void);
	bool RemoveLocalUnusedLinkerObjects(void);

	void CollectEntryBlocks(NativeCodeBasicBlock* block);

	void AddEntryBlock(NativeCodeBasicBlock* block);
	void RemEntryBlock(NativeCodeBasicBlock* block);

	NativeCodeBasicBlock * SplitMatchingTails(NativeCodeProcedure* proc);

	NativeCodeBasicBlock* AddDominatorBlock(NativeCodeProcedure* proc, NativeCodeBasicBlock* pblock);
	bool JoinTailCodeSequences(NativeCodeProcedure* proc, bool loops);
	bool SameTail(const NativeCodeInstruction& ins) const;
	bool HasTailSTA(int& addr, int& index) const;
	bool HasTailSTX(int& addr, int& index) const;
	bool HasTailSTY(int& addr, int& index) const;
	bool HasTailSTAX16(int& addr, int& index0) const;

	bool MayBeMovedBeforeBlock(int at);
	bool MayBeMovedBeforeBlock(int start, int end);
	bool SafeInjectSequenceFromBack(NativeCodeBasicBlock* block, int start, int end);
	bool JoinCommonBranchCodeSequences(void);

	bool CanJoinEntryLoadStoreZP(int saddr, int daddr);
	bool DoJoinEntryLoadStoreZP(int saddr, int daddr);
	bool JoinEntryLoadStoreZP(void);

	bool IsExitYRegZP(int addr, int& index, NativeCodeBasicBlock * & block);
	bool IsExitXRegZP(int addr, int& index, NativeCodeBasicBlock * & block);
	bool IsExitARegZP(int addr, int& index, NativeCodeBasicBlock * & block);

	void MarkLiveBlockChain(int index, NativeCodeBasicBlock* block, uint32 live, uint32 reg);

	bool ShortcutBlockExit(void);
	bool PropagateSinglePath(void);
	bool ShortcutORACascade(void);

	bool CanChangeTailZPStoreToX(int addr, const NativeCodeBasicBlock * nblock, const NativeCodeBasicBlock* fblock = nullptr) const;
	void ChangeTailZPStoreToX(int addr);
	bool CanChangeTailZPStoreToY(int addr, const NativeCodeBasicBlock* nblock, const NativeCodeBasicBlock* fblock = nullptr) const;
	void ChangeTailZPStoreToY(int addr);

	bool CanCrossBlockAShortcut(int addr);
	void DoCrossBlockAShortcut(int addr);

	bool CanCrossBlockXShortcut(int addr);
	void DoCrossBlockXShortcut(int addr);

	bool CanCrossBlockYShortcut(int addr);
	void DoCrossBlockYShortcut(int addr);

	bool CrossBlockXYShortcut(void);
	void BypassAccuLoadStoreXY(void);

	bool CrossBlockYAliasProgpagation(const int * yalias);

	bool CrossBlockRegisterAlias(bool sameAX, bool sameAY);

	bool BypassRegisterConditionBlock(void);
	bool FoldLoopEntry(void);
	bool CombineAlternateLoads(void);

	bool Is16BitImmSum(int at, int & val, int& reg) const;

	bool Check16BitSum(int at, NativeRegisterSum16Info& info);
	bool Propagate16BitSum(const ExpandingArray<NativeRegisterSum16Info>& cinfo);
	
	bool Propagate16BitHighSum(void);

	bool Check16BitSum(const NativeCodeBasicBlock* block, int origin, int at, int reg);
	bool EliminateUpper16BitSum(NativeCodeProcedure* nproc);

	bool IsFinalZeroPageUse(const NativeCodeBasicBlock* block, int at, int from, int to, bool pair, bool fchanged);
	bool IsFinalZeroPageUseTail(const NativeCodeBasicBlock* block, int from, int to, bool pair);
	bool ReplaceFinalZeroPageUse(NativeCodeProcedure* nproc);
	bool ForwardReplaceZeroPage(int at, int from, int to, bool pair);

	bool CanZeroPageCopyUp(int at, int from, int to, bool diamond);
	bool ShortcutZeroPageCopyUp(NativeCodeProcedure* nproc);
	bool BackwardReplaceZeroPage(int at, int from, int to, bool diamond);

	bool IsSameRegisterSource(const NativeCodeInstruction& rins) const;
	bool PartialBackwardValuePropagation(void);
	bool HoistCommonLoads(void);

	bool FindInitialLoadA(NativeCodeInstruction *& ins);
	bool FindInitialLoadX(NativeCodeInstruction *& ins);
	bool FindInitialLoadY(NativeCodeInstruction *& ins);

	NativeRegisterDataSet	mEntryRegisterDataSet;

	void BuildEntryDataSet(const NativeRegisterDataSet& set);
	bool ApplyEntryDataSet(void);

	bool CollectZeroPageSet(ZeroPageSet& locals, ZeroPageSet& global, bool ignorefcall);
	void CollectZeroPageUsage(NumberSet& used, NumberSet& modified, NumberSet& pairs);
	void FindZeroPageAlias(const NumberSet& statics, NumberSet& invalid, uint8* alias, int accu);
	bool RemapZeroPage(const uint8* remap);

	bool LoopRegisterXYMap(void);
	void GlobalRegisterXYCheck(int* xregs, int * yregs);
	void GlobalRegisterXMap(int reg);
	void GlobalRegisterYMap(int reg);
	bool LocalRegisterXYMap(void);
	bool ReduceLocalYPressure(void);
	bool ReduceLocalXPressure(void);
	bool LocalZeroPageValueNumbering(void);

	bool CombineZPPair(int at, int r0, int r1, bool use0, bool use1, bool & swap);
	bool RemoveDoubleZPStore(void);

	bool ExpandADCToBranch(NativeCodeProcedure* proc);
	bool Expand16BitLoopBranch(void);
	bool SimpleInlineCalls(void);

	bool Split16BitLoopCount(NativeCodeProcedure* proc);
	bool SimplifyDiamond(NativeCodeProcedure* proc);
	bool SimplifyLoopEnd(NativeCodeProcedure* proc);
	bool CrossBlockStoreLoadBypass(NativeCodeProcedure* proc);
	bool EliminateDeadLoops(void);
	bool LoopRegisterWrapAround(void);
	bool CrossBlockFlagsForwarding(void);

	bool SinglePathRegisterForwardY(NativeCodeBasicBlock* path, int yreg);
	bool SinglePathRegisterForward(void);
	bool SinglePathStoreForward(void);

	bool CanBytepassLoad(const NativeCodeInstruction& ains, int from = 0) const;
	bool CanHoistStore(const NativeCodeInstruction& ains) const;

	bool MoveAccuTrainUp(int at, int end);
	bool MoveAccuTrainsUp(void);
	bool MoveAccuTrainsDown(void);
	bool MoveAccuTrainDown(int end, int start);

	bool MoveImmediateStoreUp(int at);
	bool MoveImmediateStoreDown(int at);
	bool RecycleImmediates(void);
	bool MoveLoadStoreDown(int at);
	bool RecycleLoadStore(void);

	void BuildUseChangeSets(int start, int end, unsigned & used, unsigned & changed, uint32 & flags);
	bool CanExchangeSegments(int start, int mid, int end);
	bool CanSwapInstructions(int at);

	bool CrossBlockXYPreservation(void);
	bool CrossBlockIncToZeroShortcut(void);
	bool EliminateMicroBlocks(void);

	bool AlternateXYUsage(void);
	bool OptimizeXYPairUsage(void);
	bool CanGlobalSwapXY(void);
	bool GlobalSwapXY(void);
	bool LocalSwapXY(void);
	bool UntangleXYUsage(void);
	bool AlternateXXUsage(void);

	bool IsSimpleSubExpression(int at, NativeSimpleSubExpression & ex);
	bool PropagateCommonSubExpression(void);

	bool CanForwardZPMove(int saddr, int daddr, int & index) const;
	bool CanForwardLoadStore(const NativeCodeInstruction & lins, const NativeCodeInstruction & sins, int& index) const;
	bool Is16BitAddSubImmediate(int at, int& sreg, int &dreg, int& offset) const;
	bool CanForward16BitAddSubImmediate(int sreg, int dreg, int offset, int & index) const;

	bool CheckShortcutPointerAddForward(int at);
	bool ShortcutPointerAddForward(void);

	bool ShortcutIndirectLoadStore(void);
	bool MoveIndirectLoadZeroStoreDown(int at);
	bool MoveLoadZeroStoreIndirectUp(int at);

	bool CommonSubExpressionElimination(void);

	bool CheckPatchFailReg(const NativeCodeBasicBlock* block, int reg);
	bool CheckPatchFailRegPair(const NativeCodeBasicBlock* block, int reg);
	bool CheckPatchFailUse(void);

	bool CheckPatchFailLoop(const NativeCodeBasicBlock* block, const NativeCodeBasicBlock* head, int reg, bool changed);

	bool JoinSameBranch(NativeCodeBasicBlock* block);
	bool MergeSameBranch(void);

	// reg : base register pair to replace
	// index: index register
	// at : start position in block
	// yval: known y immediate value of -1 if not known
	// lobj: linker object addressed
	// address: offset into linker object
	bool CheckGlobalAddressSumYPointer(const NativeCodeBasicBlock * block, int reg, int index, int at, int yval);
	bool PatchGlobalAddressSumYPointer(const NativeCodeBasicBlock* block, int reg, int index, int at, int yval, LinkerObject * lobj, int address, uint32 flags = NCIF_LOWER | NCIF_UPPER);

	bool CheckSingleUseGlobalLoad(const NativeCodeBasicBlock* block, int reg, int at, const NativeCodeInstruction& ains, int cycles);
	bool PatchSingleUseGlobalLoad(const NativeCodeBasicBlock* block, int reg, int at, const NativeCodeInstruction& ains);

	// reg : base register pair to replace
	// base: new base register
	// iins : indexing instruction
	// at : start position in block
	// yval: known y immediate value of -1 if not known
	bool CheckForwardSumYPointer(const NativeCodeBasicBlock* block, int reg, int base, const NativeCodeInstruction & iins, int at, int yval, int ymax);
	bool PatchForwardSumYPointer(const NativeCodeBasicBlock* block, int reg, int base, const NativeCodeInstruction & iins, int at, int yval);

	// reg : base register pair to replace LSB with zero
	// ireg : index register
	// at : start position in block
	// yval: known y immediate value of -1 if not known
	bool CheckForwardLowYPointer(const NativeCodeBasicBlock* block, int reg, int yreg, int at, int yval);
	bool PatchForwardLowYPointer(const NativeCodeBasicBlock* block, int reg, int yreg, int at, int yval);

	bool CrossBlock16BitFlood(NativeCodeProcedure* proc);
	bool CheckCrossBlock16BitFlood(const NativeCodeBasicBlock* block, int sreg, int dreg, int at, bool rvalid);
	bool CheckCrossBlock16BitFloodExit(const NativeCodeBasicBlock* block, int sreg, int dreg, bool rvalid);
	bool PatchCrossBlock16BitFlood(const NativeCodeBasicBlock* block, int sreg, int dreg, int at);
	bool PatchCrossBlock16BitFloodExit(const NativeCodeBasicBlock* block, int sreg, int dreg);

	bool CrossBlockXYFlood(NativeCodeProcedure * proc);

	bool CheckCrossBlockXFlood(const NativeCodeBasicBlock* block, int reg, int at, bool rvalid);
	bool CheckCrossBlockXFloodExit(const NativeCodeBasicBlock* block, int reg, bool rvalid);
	bool PatchCrossBlockXFlood(const NativeCodeBasicBlock* block, int reg, int at);
	bool PatchCrossBlockXFloodExit(const NativeCodeBasicBlock* block, int reg);

	bool CheckCrossBlockYFlood(const NativeCodeBasicBlock* block, int reg, int at, bool rvalid);
	bool CheckCrossBlockYFloodExit(const NativeCodeBasicBlock* block, int reg, bool rvalid);
	bool PatchCrossBlockYFlood(const NativeCodeBasicBlock* block, int reg, int at);
	bool PatchCrossBlockYFloodExit(const NativeCodeBasicBlock* block, int reg);

	bool CrossBlockY2XFlood(NativeCodeProcedure* proc);
	bool CheckCrossBlockY2XFlood(const NativeCodeBasicBlock* block, int at);
	bool CheckCrossBlockY2XFloodExit(const NativeCodeBasicBlock* block);
	bool PatchCrossBlockY2XFlood(const NativeCodeBasicBlock* block, int at);
	bool PatchCrossBlockY2XFloodExit(const NativeCodeBasicBlock* block);

	void PropagateZPAbsolute(const NativeRegisterDataSet& data);

	void PropagateAddGlobalCarry(void);

	void RegisterFunctionCalls(void);
	bool MergeFunctionCalls(void);

	bool IsDominatedBy(const NativeCodeBasicBlock* block) const;

	void CheckLive(void);
	void CheckBlocks(bool sequence = false);
	void CheckAsmCode(void);
	void CheckVisited(void);
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
		int		mFastCallBase;
		bool	mNoFrame, mSimpleInline;
		int		mTempBlocks;

		ExpandingArray<LinkerReference>	mRelocations;
		ExpandingArray< NativeCodeBasicBlock*>	 mBlocks;
		ExpandingArray<CodeLocation>		mCodeLocations;


		void Compile(InterCodeProcedure* proc);
		void Optimize(void);
		void Assemble(void);

		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);
		NativeCodeBasicBlock* AllocateBlock(void);

		void CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock*block);

		bool MapFastParamsToTemps(void);
		void CompressTemporaries(bool singles);

		void BuildDataFlowSets(void);
		void ResetEntryBlocks(void);
		void ResetChecked(void);
		void ResetVisited(void);
		void ResetPatched(void);
		void RebuildEntry(void);
		void ResetIndexFlipped(void);
		void CheckBlocks(bool sequence = false);

		void SaveTempsToStack(int tempSave);
		void LoadTempsFromStack(int tempSave);
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
		LinkerObject	*	mLinkerLSB, * mLinkerMSB;
		int					mFactor, mSize;
		InterOperator		mOperator;
	};

	struct FloatTable
	{
		LinkerObject	*	mLinker[4];
		float				mConst;
		int					mMinValue, mMaxValue;
		InterOperator		mOperator;
		bool				mReverse;
	};

	LinkerObject* AllocateShortMulTable(InterOperator op, int factor, int size, bool msb);
	LinkerObject* AllocateFloatTable(InterOperator op, bool reverse, int minval, int maxval, float fval, int index);
	void PopulateShortMulTables(void);

	Runtime& ResolveRuntime(const Ident* ident);

	Errors* mErrors;
	Linker* mLinker;
	LinkerSection* mRuntimeSection;

	ExpandingArray<Runtime>		mRuntime;
	ExpandingArray<MulTable>	mMulTables;
	ExpandingArray<FloatTable>	mFloatTables;

	struct FunctionCall
	{
		LinkerObject			*	mLinkerObject, * mProxyObject;
		NativeCodeInstruction		mIns[64];
		FunctionCall			*	mNext, * mSame;
		int							mCount, mOffset;

		bool IsSame(const FunctionCall* fc) const;
		int Matches(const FunctionCall* fc) const;
		int PotentialMatches(const FunctionCall* fc) const;
	};

	FunctionCall* mFunctionCalls;

	void RegisterFunctionCall(NativeCodeBasicBlock* block, int at);
	void BuildFunctionProxies(void);
	bool MergeFunctionCall(NativeCodeBasicBlock* block, int at);
};

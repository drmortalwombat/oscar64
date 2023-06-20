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

	bool SameData(const NativeRegisterData& d) const;
	bool SameData(const NativeCodeInstruction& ins) const;
};

struct NativeRegisterDataSet
{
	NativeRegisterData		mRegs[261];

	void Reset(void);
	void ResetMask(void);

	void ResetZeroPage(int addr);
	void ResetAbsolute(LinkerObject * linkerObject, int addr);
	int FindAbsolute(LinkerObject* linkerObject, int addr);
	void ResetIndirect(int reg);
	void ResetX(void);
	void ResetY(void);
	void ResetWorkRegs(void);
	void ResetWorkMasks(void);
	void Intersect(const NativeRegisterDataSet& set);
	void IntersectMask(const NativeRegisterDataSet& set);
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

static const uint32 NCIF_USE_CPU_REG_A = 0x00001000;
static const uint32 NCIF_USE_CPU_REG_X = 0x00002000;
static const uint32 NCIF_USE_CPU_REG_Y = 0x00004000;

// use a 32bit zero page register indexed by X for JSR
static const uint32 NCIF_USE_ZP_32_X = 0x00008000;
static const uint32 NICF_USE_ZP_ADDR = 0x00010000;
static const uint32 NICF_USE_WORKREGS = 0x00020000;

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
	bool MayBeSameAddress(const NativeCodeInstruction& ins, bool sameXY = false) const;
	bool IsSame(const NativeCodeInstruction& ins) const;
	bool IsSameLS(const NativeCodeInstruction& ins) const;
	bool IsCommutative(void) const;
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
	bool						mEntryRegA, mEntryRegX, mEntryRegY, mExitRegA, mExitRegX;
	NativeCodeBasicBlock	*	mDominator, * mSameBlock;

	NativeCodeBasicBlock* mLoopHeadBlock, * mLoopTailBlock;

	NativeRegisterDataSet	mDataSet, mNDataSet, mFDataSet;
	ValueNumberingDataSet	mNumDataSet, mNNumDataSet, mFNumDataSet;

	int						mYAlias[256], mYOffset;

	ExpandingArray<NativeRegisterSum16Info>	mRSumInfos;

	NativeCodeInstruction DecodeNative(const InterInstruction* ins, LinkerObject * lobj, int& offset) const;

	int PutBranch(NativeCodeProcedure* proc, NativeCodeBasicBlock* target, AsmInsType code, int offset);
	int PutJump(NativeCodeProcedure* proc, NativeCodeBasicBlock* target, int offset);
	int JumpByteSize(NativeCodeBasicBlock * target, int offset);
	int BranchByteSize(NativeCodeBasicBlock* target, int from, int to);

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
	bool PeepHoleOptimizer(NativeCodeProcedure* proc, int pass);
	void BlockSizeReduction(NativeCodeProcedure* proc, int xenter, int yenter);
	bool BlockSizeCopyReduction(NativeCodeProcedure* proc, int & si, int & di);

	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, bool full);
	bool OptimizeSimpleLoopInvariant(NativeCodeProcedure* proc, NativeCodeBasicBlock * prevBlock, NativeCodeBasicBlock* exitBlock, bool full);
	bool RemoveSimpleLoopUnusedIndex(void);
	bool OptimizeLoopCarryOver(void);
	bool OptimizeSingleEntryLoop(NativeCodeProcedure* proc);

	bool OptimizeSimpleLoop(NativeCodeProcedure* proc, bool full);
	bool SimpleLoopReversal(NativeCodeProcedure* proc);
	bool OptimizeInnerLoop(NativeCodeProcedure* proc, NativeCodeBasicBlock* head, NativeCodeBasicBlock* tail, ExpandingArray<NativeCodeBasicBlock*>& blocks);
	bool OptimizeXYSimpleLoop(void);

	bool OptimizeSelect(NativeCodeProcedure* proc);

	bool OptimizeInnerLoops(NativeCodeProcedure* proc);
	NativeCodeBasicBlock* CollectInnerLoop(NativeCodeBasicBlock* head, ExpandingArray<NativeCodeBasicBlock*>& lblocks);

	bool OptimizeGenericLoop(NativeCodeProcedure* proc);
	bool CollectGenericLoop(NativeCodeProcedure* proc, ExpandingArray<NativeCodeBasicBlock*>& lblocks);
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
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins, const InterInstruction* sins1, const InterInstruction* sins0, bool addrvalid);
	void LoadStoreOpAbsolute2D(InterCodeProcedure* proc, const InterInstruction* lins1, const InterInstruction* lins2, const InterInstruction* mins);
	void SignExtendAddImmediate(InterCodeProcedure* proc, const InterInstruction* xins, const InterInstruction* ains);

	void NumericConversion(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	NativeCodeBasicBlock * CopyValue(InterCodeProcedure* proc, const InterInstruction * ins, NativeCodeProcedure* nproc);
	NativeCodeBasicBlock * StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins, NativeCodeProcedure* nproc);
	void AddAsrSignedByte(InterCodeProcedure* proc, const InterInstruction* ains, const InterInstruction* sins);

	void LoadByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* rins);
	void StoreByteIndexedValue(InterCodeProcedure* proc, const InterInstruction* iins, const InterInstruction* rins);

	void CallAssembler(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins);

	void ShiftRegisterLeft(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift);
	void ShiftRegisterLeftByte(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift);
	void ShiftRegisterLeftFromByte(InterCodeProcedure* proc, const InterInstruction* ins, int reg, int shift, int max);
	int ShortMultiply(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction * ins, const InterInstruction* sins, int index, int mul);
	int ShortSignedDivide(InterCodeProcedure* proc, NativeCodeProcedure* nproc, const InterInstruction* ins, const InterInstruction* sins, int mul);

	bool CheckPredAccuStore(int reg);

	NumberSet		mLocalRequiredRegs, mLocalProvidedRegs;
	NumberSet		mEntryRequiredRegs, mEntryProvidedRegs;
	NumberSet		mExitRequiredRegs, mExitProvidedRegs;
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
	void MarkLoopHead(void);
	void BuildDominatorTree(NativeCodeBasicBlock * from);

	bool MoveLoadStoreUp(int at);
	bool MoveLoadStoreXUp(int at);
	bool MoveLoadImmStoreAbsoluteUp(int at);

	bool MoveIndirectLoadStoreDown(int at);
	bool MoveIndirectLoadStoreDownY(int at);

	bool MoveIndirectLoadStoreUp(int at);
	bool MoveAbsoluteLoadStoreUp(int at);
	bool MoveLoadStoreOutOfXYRangeUp(int at);
	bool MoveLoadIndirectTempStoreUp(int at);
	bool MoveLoadIndirectBypassYUp(int at);

	bool MoveLoadAddImmStoreAbsXUp(int at);
	bool MoveStaTaxLdaStaDown(int at);

	bool MoveLoadAddImmStoreUp(int at);
	bool MoveCLCLoadAddZPStoreUp(int at);
	bool MoveLoadAddZPStoreUp(int at);
	bool MoveLoadShiftRotateUp(int at);
	bool MoveLoadShiftStoreUp(int at);

	bool MoveLDSTXOutOfRange(int at);

	bool MoveCLCLoadAddZPStoreDown(int at);
	bool FindDirectAddressSumY(int at, int reg, int& apos, int& breg);
	bool PatchDirectAddressSumY(int at, int reg, int apos, int breg);
	bool FindAddressSumY(int at, int reg, int & apos, int& breg, int& ireg);
	bool PatchAddressSumY(int at, int reg, int apos, int breg, int ireg);
	bool FindGlobalAddress(int at, int reg, int& apos);
	bool FindGlobalAddressSumY(int at, int reg, bool direct, int& apos, const NativeCodeInstruction * & ains, const NativeCodeInstruction*& iins, uint32 & flags, int & addr);
	bool FindExternAddressSumY(int at, int reg, int& breg, int& ireg);
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

	bool MoveZeroPageCrossBlockUp(int at, const NativeCodeInstruction & lins, const NativeCodeInstruction & sins);
	bool ShortcutCrossBlockMoves(NativeCodeProcedure* proc);

	bool CanReplaceYRegWithXReg(int start, int end);
	bool CanReplaceXRegWithYReg(int start, int end);

	bool ForwardAccuAddSub(void);
	bool ForwardZpYIndex(bool full);
	bool ForwardZpXIndex(bool full);
	bool ForwardAXYReg(void);

	// Join sequences of TXA, CLC, ADC #xx into INX, TXA sequences if possible
	bool JoinXYCascade(void);

	bool RegisterValueForwarding(void);
	bool CanCombineSameXtoY(int start, int end);
	bool CanCombineSameYtoX(int start, int end);
	bool CombineSameXY(void);
	bool CombineSameXtoY(int xpos, int ypos, int end);
	bool CombineSameYtoX(int xpos, int ypos, int end);

	bool FindImmediateStore(int at, int reg, const NativeCodeInstruction*& ains);

	bool JoinTAXARange(int from, int to);
	bool JoinTAYARange(int from, int to);
	bool PatchGlobalAdressSumYByX(int at, int reg, const NativeCodeInstruction& ains, int addr);
	bool MergeXYSameValue(int from);
	void InsertLoadYImmediate(const InterInstruction * iins, int at, int val);
	int RetrieveAValue(int at) const;
	int RetrieveXValue(int at) const;
	int RetrieveYValue(int at) const;
	int RetrieveZPValue(int reg, int at) const;
	int FindFreeAccu(int at) const;

	bool ReverseReplaceTAX(int at);

	bool ValueForwarding(NativeCodeProcedure* proc, const NativeRegisterDataSet& data, bool global, bool final);
	bool GlobalValueForwarding(NativeCodeProcedure* proc, bool final);
	bool BitFieldForwarding(const NativeRegisterDataSet& data);
	bool ReverseBitfieldForwarding(void);
	bool OffsetValueForwarding(const ValueNumberingDataSet & data);
	bool AbsoluteValueForwarding(void);

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

	bool IsExitYRegZP(int addr, int& index) const;
	bool IsExitXRegZP(int addr, int& index) const;
	bool IsExitARegZP(int addr, int& index) const;

	bool PropagateSinglePath(void);

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

	bool CrossBlockYAliasProgpagation(const int * yalias, int yoffset);

	bool BypassRegisterConditionBlock(void);
	bool FoldLoopEntry(void);

	bool Is16BitImmSum(int at, int & val, int& reg) const;

	bool Check16BitSum(int at, NativeRegisterSum16Info& info);
	bool Propagate16BitSum(const ExpandingArray<NativeRegisterSum16Info>& cinfo);
	
	bool Propagate16BitHighSum(void);

	bool Check16BitSum(const NativeCodeBasicBlock* block, int origin, int at, int reg);
	bool EliminateUpper16BitSum(NativeCodeProcedure* nproc);

	bool IsFinalZeroPageUse(const NativeCodeBasicBlock* block, int at, int from, int to, bool pair);
	bool ReplaceFinalZeroPageUse(NativeCodeProcedure* nproc);
	bool ForwardReplaceZeroPage(int at, int from, int to);

	bool CanZeroPageCopyUp(int at, int from, int to, bool diamond);
	bool ShortcutZeroPageCopyUp(NativeCodeProcedure* nproc);
	bool BackwardReplaceZeroPage(int at, int from, int to, bool diamond);

	NativeRegisterDataSet	mEntryRegisterDataSet;

	void BuildEntryDataSet(const NativeRegisterDataSet& set);
	bool ApplyEntryDataSet(void);

	bool CollectZeroPageSet(ZeroPageSet& locals, ZeroPageSet& global);
	void CollectZeroPageUsage(NumberSet& used, NumberSet& modified, NumberSet& pairs);
	void FindZeroPageAlias(const NumberSet& statics, NumberSet& invalid, uint8* alias, int accu);
	bool RemapZeroPage(const uint8* remap);

	void GlobalRegisterXYCheck(int* xregs, int * yregs);
	void GlobalRegisterXMap(int reg);
	void GlobalRegisterYMap(int reg);
	bool LocalRegisterXYMap(void);
	bool ReduceLocalYPressure(void);
	bool ReduceLocalXPressure(void);

	bool CombineZPPair(int at, int r0, int r1, bool use0, bool use1, bool & swap);
	bool RemoveDoubleZPStore(void);

	bool ExpandADCToBranch(NativeCodeProcedure* proc);
	bool Split16BitLoopCount(NativeCodeProcedure* proc);
	bool SimplifyDiamond(NativeCodeProcedure* proc);
	bool SimplifyLoopEnd(NativeCodeProcedure* proc);
	bool CrossBlockStoreLoadBypass(NativeCodeProcedure* proc);

	bool CanBytepassLoad(const NativeCodeInstruction& ains) const;
	bool CanHoistStore(const NativeCodeInstruction& ains) const;

	bool MoveAccuTrainUp(int at, int end);
	bool MoveAccuTrainsUp(void);
	bool MoveAccuTrainsDown(void);
	bool MoveAccuTrainDown(int end, int start);

	void BuildUseChangeSets(int start, int end, unsigned & used, unsigned & changed, uint32 & flags);
	bool CanExchangeSegments(int start, int mid, int end);

	bool CrossBlockXYPreservation(void);

	bool AlternateXYUsage(void);
	bool OptimizeXYPairUsage(void);
	bool CanGlobalSwapXY(void);
	bool GlobalSwapXY(void);

	bool IsSimpleSubExpression(int at, NativeSimpleSubExpression & ex);
	bool PropagateCommonSubExpression(void);

	bool ForwardAbsoluteLoadStores(void);
	bool CanForwardZPMove(int saddr, int daddr, int & index) const;
	bool Is16BitAddSubImmediate(int at, int& sreg, int &dreg, int& offset) const;
	bool CanForward16BitAddSubImmediate(int sreg, int dreg, int offset, int & index) const;

	bool CheckShortcutPointerAddForward(int at);
	bool ShortcutPointerAddForward(void);

	bool CommonSubExpressionElimination(void);

	bool CheckPatchFailReg(const NativeCodeBasicBlock* block, int reg);
	bool CheckPatchFailRegPair(const NativeCodeBasicBlock* block, int reg);
	bool CheckPatchFailUse(void);

	bool CheckPatchFailLoop(const NativeCodeBasicBlock* block, const NativeCodeBasicBlock* head, int reg, bool changed);

	bool CheckGlobalAddressSumYPointer(const NativeCodeBasicBlock * block, int reg, int index, int at, int yval);
	bool PatchGlobalAddressSumYPointer(const NativeCodeBasicBlock* block, int reg, int index, int at, int yval, LinkerObject * lobj, int address, uint32 flags = NCIF_LOWER | NCIF_UPPER);

	bool CheckSingleUseGlobalLoad(const NativeCodeBasicBlock* block, int reg, int at, const NativeCodeInstruction& ains, int cycles);
	bool PatchSingleUseGlobalLoad(const NativeCodeBasicBlock* block, int reg, int at, const NativeCodeInstruction& ains);

	// reg : base register pair to replace
	// base: new base register
	// iins : indexing instruction
	// at : start position in block
	// yval: known y immediate value of -1 if not known
	bool CheckForwardSumYPointer(const NativeCodeBasicBlock* block, int reg, int base, const NativeCodeInstruction & iins, int at, int yval);
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

	void PropagateZPAbsolute(void);

	bool IsDominatedBy(const NativeCodeBasicBlock* block) const;

	void CheckLive(void);
	void CheckBlocks(bool sequence = false);
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
		bool	mNoFrame;
		int		mTempBlocks;

		ExpandingArray<LinkerReference>	mRelocations;
		ExpandingArray< NativeCodeBasicBlock*>	 mBlocks;
		ExpandingArray<CodeLocation>		mCodeLocations;


		void Compile(InterCodeProcedure* proc);
		void Optimize(void);

		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);
		NativeCodeBasicBlock* AllocateBlock(void);

		void CompileInterBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* iblock, NativeCodeBasicBlock*block);

		bool MapFastParamsToTemps(void);
		void CompressTemporaries(bool singles);

		void BuildDataFlowSets(void);
		void ResetEntryBlocks(void);
		void ResetVisited(void);
		void ResetPatched(void);
		void RebuildEntry(void);

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

	LinkerObject* AllocateShortMulTable(InterOperator op, int factor, int size, bool msb);

	Runtime& ResolveRuntime(const Ident* ident);

	Errors* mErrors;
	Linker* mLinker;
	LinkerSection* mRuntimeSection;

	ExpandingArray<Runtime>	mRuntime;
	ExpandingArray<MulTable>	mMulTables;
};

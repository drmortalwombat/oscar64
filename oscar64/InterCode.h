#pragma once

#include "Array.h"
#include "NumberSet.h"
#include "Errors.h"
#include "BitVector.h"
#include <stdio.h>
#include "MachineTypes.h"
#include "Ident.h"
#include "Linker.h"

class Declaration;

enum InterCode
{
	IC_NONE,
	IC_LOAD_TEMPORARY,
	IC_BINARY_OPERATOR,
	IC_UNARY_OPERATOR,
	IC_RELATIONAL_OPERATOR,
	IC_CONVERSION_OPERATOR,
	IC_STORE,			// store src[0] into src[1]
	IC_LOAD,			// load from src[0]
	IC_LEA,
	IC_COPY,			// Copy from src[0] to src[1]
	IC_STRCPY,
	IC_FILL,			// Fill src[1] with src[0]
	IC_MALLOC,
	IC_FREE,
	IC_TYPECAST,
	IC_CONSTANT,
	IC_BRANCH,
	IC_JUMP,
	IC_PUSH_FRAME,
	IC_POP_FRAME,
	IC_CALL,
	IC_CALL_NATIVE,
	IC_RETURN_VALUE,
	IC_RETURN_STRUCT,
	IC_RETURN,
	IC_ASSEMBLER,
	IC_JUMPF,
	IC_SELECT,
	IC_DISPATCH,
	IC_UNREACHABLE,
	IC_BREAKPOINT
};

enum InterType
{
	IT_NONE,
	IT_BOOL,
	IT_INT8,
	IT_INT16,
	IT_INT32,
	IT_FLOAT,
	IT_POINTER
};

extern int InterTypeSize[];

enum InterMemory
{
	IM_NONE,
	IM_PARAM,		// Memory used to access parameters on stack
	IM_LOCAL,
	IM_GLOBAL,
	IM_FRAME,		// Memory used to pass parameters on stack
	IM_PROCEDURE,
	IM_INDIRECT,
	IM_TEMPORARY,
	IM_ABSOLUTE,
	IM_FPARAM,		// Memory used to access parameters in zp
	IM_FFRAME,		// Memory used to pass parameters in zp
};

enum InterOperator
{
	IA_NONE,
	IA_ADD,
	IA_SUB,
	IA_MUL,
	IA_DIVU,
	IA_DIVS,
	IA_MODU,
	IA_MODS,
	IA_OR,
	IA_AND,
	IA_XOR,
	IA_NEG,
	IA_ABS,
	IA_FLOOR,
	IA_CEIL,
	IA_NOT,
	IA_SHL,
	IA_SHR,
	IA_SAR,
	IA_CMPEQ,
	IA_CMPNE,
	IA_CMPGES,
	IA_CMPLES,
	IA_CMPGS,
	IA_CMPLS,
	IA_CMPGEU,
	IA_CMPLEU,
	IA_CMPGU,
	IA_CMPLU,

	IA_FLOAT2INT,
	IA_INT2FLOAT,
	IA_FLOAT2UINT,
	IA_UINT2FLOAT,
	IA_FLOAT2LINT,
	IA_LINT2FLOAT,
	IA_FLOAT2LUINT,
	IA_LUINT2FLOAT,

	IA_EXT8TO16U,
	IA_EXT8TO32U,
	IA_EXT16TO32U,
	IA_EXT8TO16S,
	IA_EXT8TO32S,
	IA_EXT16TO32S
};

class InterInstruction;
class InterCodeBasicBlock;
class InterCodeProcedure;
class InterVariable;
class InterCodeModule;

typedef InterInstruction* InterInstructionPtr;
typedef InterCodeBasicBlock* InterCodeBasicBlockPtr;
typedef InterCodeProcedure* InterCodeProcedurePtr;

typedef GrowingArray<InterType>					GrowingTypeArray;
typedef GrowingArray<int>						GrowingIntArray;
typedef GrowingArray<InterInstructionPtr>		GrowingInstructionPtrArray;
typedef GrowingArray<InterCodeBasicBlockPtr>	GrowingInterCodeBasicBlockPtrArray;
typedef GrowingArray<InterInstruction *>		GrowingInstructionArray;
typedef GrowingArray<InterCodeProcedurePtr	>	GrowingInterCodeProcedurePtrArray;


typedef GrowingArray<InterVariable * >			GrowingVariableArray;

#define INVALID_TEMPORARY	(-1)

InterOperator MirrorRelational(InterOperator oper);
InterOperator InvertRelational(InterOperator oper);

class IntegerValueRange
{
public:
	IntegerValueRange(void);
	~IntegerValueRange(void);

	void Reset(void);
	void Restart(void);

	int64		mMinValue, mMaxValue;
	uint8		mMinExpanded, mMaxExpanded;
	
	enum State : uint8
	{
		S_UNKNOWN,
		S_UNBOUND,
		S_WEAK,
		S_BOUND
	}			mMinState, mMaxState;

	bool Same(const IntegerValueRange& range) const;
	bool Weaker(const IntegerValueRange& range) const;
	bool Merge(const IntegerValueRange& range, bool head, bool initial);
	void Expand(const IntegerValueRange& range);
	void Union(const IntegerValueRange& range);

	void Limit(const IntegerValueRange& range);
	void LimitWeak(const IntegerValueRange& range);
	void MergeUnknown(const IntegerValueRange& range);
	void SetLimit(int64 minValue, int64 maxValue);
	void SetBounds(State minState, int64 minValue, State maxState, int64 maxValue);
	void SetConstant(int64 value);

	bool IsBound(void) const;
	bool IsConstant(void) const;
	bool IsInvalid(void) const;

	void LimitMin(int64 value);
	void LimitMax(int64 value);

	void LimitMinBound(int64 value);
	void LimitMaxBound(int64 value);
	
	void LimitMinWeak(int64 value);
	void LimitMaxWeak(int64 value);

	void AddConstValue(InterType type, int64 value);
};



typedef ExpandingArray<IntegerValueRange>		GrowingIntegerValueRangeArray;

class ValueSet
{
protected:
	InterInstructionPtr	* mInstructions;
	int								mNum, mSize;
public:
	ValueSet(void);
	ValueSet(const ValueSet& values);
	~ValueSet(void);

	ValueSet& operator=(const ValueSet& values);

	void FlushAll(void);
	void FlushCallAliases(const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams);
	void FlushFrameAliases(void);


	void RemoveValue(int index);
	void InsertValue(InterInstruction * ins);

	void UpdateValue(InterCodeBasicBlock* block, InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs);
	void Intersect(ValueSet& set);
};

class TempForwardingTable
{
protected:
	struct Assoc
	{
		int	mAssoc, mSucc, mPred;

		Assoc(void) {}
		Assoc(int assoc, int succ, int pred) { this->mAssoc = assoc; this->mSucc = succ; this->mPred = pred; }
		Assoc(const Assoc& assoc) { this->mAssoc = assoc.mAssoc; this->mSucc = assoc.mSucc; this->mPred = assoc.mPred; }
	};

	GrowingArray<Assoc>		mAssoc;

public:
	TempForwardingTable(void);
	TempForwardingTable(const TempForwardingTable& table);

	TempForwardingTable& operator=(const TempForwardingTable& table);

	void Intersect(const TempForwardingTable& table);

	int Size(void) const;
	void SetSize(int size);

	void Reset(void);
	void Shrink(void);

	int operator[](int n);

	void Destroy(int n);

	void Build(int from, int to);
};

class InterVariable
{
public:
	bool							mUsed, mAliased, mTemp, mNotAliased;
	int								mIndex, mSize, mOffset, mTempIndex, mByteIndex;
	int								mNumReferences;
	const Ident					*	mIdent;
	LinkerObject				*	mLinkerObject;
	Declaration					*	mDeclaration;

	InterVariable(void)
		: mUsed(false), mAliased(false), mTemp(false), mNotAliased(false), mIndex(-1), mSize(0), mOffset(0), mIdent(nullptr), mLinkerObject(nullptr), mTempIndex(-1), mDeclaration(nullptr)
	{
	}
};

class InterOperand
{
public:
	int					mTemp;
	InterType			mType;
	bool				mFinal;
	int64				mIntConst;
	double				mFloatConst;
	int					mVarIndex, mOperandSize, mStride, mRestricted;
	LinkerObject	*	mLinkerObject;
	InterMemory			mMemory, mMemoryBase;
	IntegerValueRange	mRange;

	void Forward(const InterOperand& op);
	void ForwardTemp(const InterOperand& op);
	void ForwardMem(const InterOperand& op);

	InterOperand(void);

	bool IsEqual(const InterOperand & op) const;

	bool IsUByte(void) const;
	bool IsSByte(void) const;
	bool IsUnsigned(void) const;
	bool IsPositive(void) const;
	bool IsInRange(int lower, int upper) const;

	bool IsNotUByte(void) const;

	void Disassemble(FILE* file, InterCodeProcedure* proc);
};

class InterInstruction
{
public:
	Location							mLocation;
	InterCode							mCode;
	InterOperand					*	mSrc;
	InterOperand						mDst;
	InterOperand						mConst;
	InterOperator						mOperator;
	InterOperand						mOps[4];
	int									mNumOperands;

	bool								mInUse, mInvariant, mVolatile, mExpensive, mSingleAssignment, mNoSideEffects, mConstExpr, mRemove, mAliasing;

	InterInstruction(const Location& loc, InterCode code);
	InterInstruction(const InterInstruction&) = delete;
	InterInstruction& operator=(const InterInstruction&) = delete;

	bool IsEqual(const InterInstruction* ins) const;
	bool IsEqualSource(const InterInstruction* ins) const;

	InterInstruction* Clone(void) const;

	bool ReferencesTemp(int temp) const;
	bool UsesTemp(int temp) const;
	int NumUsedTemps(void) const;

	void CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable, int& nlocals, int& nparams);
	void MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams);

	void FilterTempUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	void FilterVarsUsage(const GrowingVariableArray& localVars, NumberSet& requiredVars, NumberSet& providedVars, const GrowingVariableArray& params, NumberSet& requiredParams, NumberSet& providedParams, InterMemory paramMemory);
	void FilterStaticVarsUsage(const GrowingVariableArray& staticVars, NumberSet& requiredVars, NumberSet& providedVars);
	void FilterStaticVarsByteUsage(const GrowingVariableArray& staticVars, NumberSet& requiredVars, NumberSet& providedVars, Errors * errors);

	bool RemoveUnusedResultInstructions(InterInstruction* pre, NumberSet& requiredTemps);
	bool RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, NumberSet& requiredVars, const GrowingVariableArray& params, NumberSet& requiredParams, InterMemory paramMemory);
	bool RemoveUnusedStaticStoreInstructions(InterCodeBasicBlock * block, const GrowingVariableArray& staticVars, NumberSet& requiredVars, GrowingInstructionPtrArray& storeIns);
	bool RemoveUnusedStaticStoreByteInstructions(InterCodeBasicBlock* block, const GrowingVariableArray& staticVars, NumberSet& requiredVars);
	void PerformValueForwarding(GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid);
	void BuildCallerSaveTempSet(NumberSet& requiredTemps, NumberSet& callerSaveTemps);

	void LocalRenameRegister(GrowingIntArray& renameTable, int& num);
	void GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void PerformTempForwarding(TempForwardingTable& forwardingTable, bool reverse);
	bool PropagateConstTemps(const GrowingInstructionPtrArray& ctemps);

	void BuildCollisionTable(NumberSet& liveTemps, NumberSet* collisionSets);
	void ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void CollectActiveTemporaries(FastNumberSet& set);
	void ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries);
	
	void CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes);
	void SimpleLocalToTemp(int vindex, int temp);

	bool ConstantFolding(void);
	bool ConstantFoldingRelationRange(void);

	void UnionRanges(InterInstruction* ins);

	void Disassemble(FILE* file, InterCodeProcedure * proc);
};

class InterCodeBasicBlock
{
public:
	InterCodeProcedure			*	mProc;
	int								mIndex, mNumEntries, mNumEntered, mTraceIndex;
	InterCodeBasicBlock			*	mTrueJump, * mFalseJump, * mLoopPrefix, * mDominator;
	GrowingInstructionArray			mInstructions;

	bool							mVisited, mInPath, mLoopHead, mChecked, mConditionBlockTrue, mUnreachable, mLoopPath, mValueRangeValid, mPatched, mLoopDebug;
	mutable int						mMark;

	NumberSet						mLocalUsedTemps, mLocalModifiedTemps;
	NumberSet						mLocalRequiredTemps, mLocalProvidedTemps;
	NumberSet						mEntryRequiredTemps, mEntryProvidedTemps, mEntryPotentialTemps;
	NumberSet						mExitRequiredTemps, mExitProvidedTemps, mExitPotentialTemps;
	NumberSet						mEntryConstTemp, mExitConstTemp;
	NumberSet						mNewRequiredTemps;

	NumberSet						mLocalRequiredVars, mLocalProvidedVars;
	NumberSet						mEntryRequiredVars, mEntryProvidedVars;
	NumberSet						mExitRequiredVars, mExitProvidedVars;

	NumberSet						mLocalRequiredStatics, mLocalProvidedStatics;
	NumberSet						mEntryRequiredStatics, mEntryProvidedStatics;
	NumberSet						mExitRequiredStatics, mExitProvidedStatics;

	NumberSet						mLocalRequiredParams, mLocalProvidedParams;
	NumberSet						mEntryRequiredParams, mEntryProvidedParams;
	NumberSet						mExitRequiredParams, mExitProvidedParams;

	NumberSet						mEntryRequiredArgs;

	GrowingInstructionArray			mLoadStoreInstructions;

	GrowingIntegerValueRangeArray	mEntryValueRange, mTrueValueRange, mFalseValueRange;
	GrowingIntegerValueRangeArray	mEntryParamValueRange, mTrueParamValueRange, mFalseParamValueRange, mLocalParamValueRange;

	GrowingArray<int64>				mMemoryValueSize, mEntryMemoryValueSize;

	ExpandingArray<InterCodeBasicBlock*>	mEntryBlocks, mLoopPathBlocks;

	GrowingInstructionPtrArray		mMergeTValues, mMergeAValues;
	ValueSet						mMergeValues;
	TempForwardingTable				mMergeForwardingTable;

	InterCodeBasicBlock(InterCodeProcedure * proc);
	~InterCodeBasicBlock(void);

	InterCodeBasicBlock* Clone(void);

	void Append(InterInstruction * code);
	void AppendBeforeBranch(InterInstruction* code, bool loopindex = false);
	const InterInstruction* FindByDst(int dst) const;
	void Close(InterCodeBasicBlock* trueJump, InterCodeBasicBlock* falseJump);

	void CollectEntries(void);
	void CollectEntryBlocks(InterCodeBasicBlock* from);
	void GenerateTraces(int expand, bool compact);
	void BuildDominatorTree(InterCodeBasicBlock * from);
	bool StripLoopHead(int size);

	bool MergeSameConditionTraces(void);

	void LocalToTemp(int vindex, int temp);
	void LoadConstantFold(InterInstruction* ins, InterInstruction* ains, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs);

	void CollectAllUsedDefinedTemps(NumberSet& defined, NumberSet& used);

	void CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable, int & nlocals, int & nparams);
	void MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams);
	void RecheckLocalAliased(void);

	void CollectLocalUsedTemps(int numTemps);
	bool PropagateNonLocalUsedConstTemps(void);
	void CollectConstTemps(GrowingInstructionPtrArray& ctemps, NumberSet& assignedTemps);
	bool PropagateConstTemps(const GrowingInstructionPtrArray& ctemps);
	bool ForwardConstTemps(const GrowingInstructionPtrArray& ctemps);
	bool PropagateConstCompareResults(void);

	bool EarlyBranchElimination(const GrowingInstructionPtrArray& ctemps);

	bool PropagateVariableCopy(const GrowingInstructionPtrArray& ctemps, const GrowingVariableArray& staticVars, const NumberSet & aliasedLocals, const NumberSet & aliasedParams);

	void BuildLocalTempSets(int num);
	void BuildGlobalProvidedTempSet(const NumberSet & fromProvidedTemps, const NumberSet& potentialProvidedTemps);
	bool BuildGlobalRequiredTempSet(NumberSet& fromRequiredTemps);
	bool RemoveUnusedResultInstructions(void);
	void BuildCallerSaveTempSet(NumberSet& callerSaveTemps);
	void BuildConstTempSets(void);
	bool PropagateConstOperationsUp(void);
	bool RemoveUnusedLocalStoreInstructions(void);

	void BuildLocalVariableSets(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory);
	void BuildGlobalProvidedVariableSet(const GrowingVariableArray& localVars, NumberSet fromProvidedVars, const GrowingVariableArray& params, NumberSet fromProvidedParams, InterMemory paramMemory);
	bool BuildGlobalRequiredVariableSet(const GrowingVariableArray& localVars, NumberSet& fromRequiredVars, const GrowingVariableArray& params, NumberSet& fromRequiredParams, InterMemory paramMemory);
	bool RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory);
	bool RemoveUnusedIndirectStoreInstructions(void);

	bool RemoveUnusedArgumentStoreInstructions(void);

	void BuildStaticVariableSet(const GrowingVariableArray& staticVars);
	void BuildGlobalProvidedStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet fromProvidedVars);
	bool BuildGlobalRequiredStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet& fromRequiredVars);
	bool RemoveUnusedStaticStoreInstructions(const GrowingVariableArray& staticVars);

	void BuildStaticVariableByteSet(const GrowingVariableArray& staticVars, int bsize);
	bool RemoveUnusedStaticStoreByteInstructions(const GrowingVariableArray& staticVars, int bsize);

	bool CheckSingleBlockLimitedLoop(InterCodeBasicBlock*& pblock, int64 & nloop);

	bool TempIsUnsigned(int temp);

	void RestartLocalIntegerRangeSets(int num, const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);
	void BuildLocalIntegerRangeSets(int num, const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);
	void UpdateLocalIntegerRangeSets(const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);

	void UpdateLocalIntegerRangeSetsForward(const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);
	void UpdateLocalIntegerRangeSetsBackward(const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);

	bool BuildGlobalIntegerRangeSets(bool initial, const GrowingVariableArray& localVars, const GrowingVariableArray& paramVars);
	void SimplifyIntegerRangeRelops(void);
	void MarkIntegerRangeBoundUp(int temp, int64 value, GrowingIntegerValueRangeArray& range);
	void UnionIntegerRanges(const InterCodeBasicBlock* block);
	void PruneUnusedIntegerRangeSets(void);

	bool CombineIndirectAddressing(void);

	void WarnUnreachable(void);

	GrowingIntArray			mEntryRenameTable;
	GrowingIntArray			mExitRenameTable;

	void LinkerObjectForwarding(const GrowingInstructionPtrArray& tvalue);
	bool LoadStoreForwarding(const GrowingInstructionPtrArray& tvalue, const GrowingVariableArray& staticVars);
	void ReduceRecursionTempSpilling(InterMemory paramMemory, const GrowingInstructionPtrArray& tvalue);

	void LocalRenameRegister(const GrowingIntArray& renameTable, int& num);
	void BuildGlobalRenameRegisterTable(const GrowingIntArray& renameTable, GrowingIntArray& globalRenameTable);
	void GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);
	void RenameValueRanges(const GrowingIntArray& renameTable, int numTemps);

	void CheckValueUsage(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs, FastNumberSet& fsingle);
	void PerformTempForwarding(const TempForwardingTable& forwardingTable, bool reverse, bool checkloops);
	void PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, int & spareTemps, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs);
	void PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid, const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs, FastNumberSet& fsingle);
	bool EliminateDeadBranches(void);

	bool EliminateIntegerSumAliasTemps(const GrowingInstructionPtrArray& tvalue);

	bool MergeIndexedLoadStore(const GrowingInstructionPtrArray& tvalue);
	bool SimplifyIntegerNumeric(const GrowingInstructionPtrArray& tvalue, int& spareTemps);
	bool SimplifyPointerOffsets(void);
	bool EliminateAliasValues(const GrowingInstructionPtrArray& tvalue, const GrowingInstructionPtrArray& avalue);

	bool ForwardShortLoadStoreOffsets(void);

	void CalculateSingleUsedTemps(FastNumberSet& fused, FastNumberSet& fsingle);

	bool CalculateSingleAssignmentTemps(FastNumberSet& tassigned, GrowingInstructionPtrArray& tvalue, NumberSet& modifiedParams, InterMemory paramMemory);
	bool SingleAssignmentTempForwarding(const GrowingInstructionPtrArray& tunified, const GrowingInstructionPtrArray& tvalues);

	void BuildCollisionTable(NumberSet* collisionSets);
	void ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray & localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes);
	void SimpleLocalToTemp(int vindex, int temp);

	void CollectActiveTemporaries(FastNumberSet& set);
	void ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries);
	void RemapActiveTemporaries(const FastNumberSet& set);

	void Disassemble(FILE* file, bool dumpSets);

	void CollectVariables(GrowingVariableArray & globalVars, GrowingVariableArray & localVars, GrowingVariableArray& paramVars, InterMemory	paramMemory);
	void MapVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars);
	
	void CollectOuterFrame(int level, int& size, bool& inner, bool& inlineAssembler, bool& byteCodeCall);
	bool RecheckOuterFrame(void);
	void CollectParamVarsSize(int& size);

	bool IsLeafProcedure(void);
	bool PreventsCallerStaticStack(void);

	void UnionEntryValueRange(const GrowingIntegerValueRangeArray & range, const GrowingIntegerValueRangeArray& paramRange);
	bool ForwardDiamondMovedTemp(void);
	bool ForwardLoopMovedTemp(void);

	bool MoveTrainCrossBlock(void);
	bool HoistCommonConditionalPath(void);
	bool IsDominator(InterCodeBasicBlock* block);
	bool IsDirectDominatorBlock(InterCodeBasicBlock* block);
	bool IsDirectLoopPathBlock(InterCodeBasicBlock* block);
	bool CollectBlocksToDominator(InterCodeBasicBlock* dblock, ExpandingArray<InterCodeBasicBlock*>& body);

	void MarkRelevantStatics(void);
	void RemoveNonRelevantStatics(void);

	bool IsTempModifiedOnPath(int temp, int at) const;
	bool IsTempReferencedOnPath(int temp, int at) const;

	// The memory referenced by lins may be writtne by sind
	bool DestroyingMem(const InterInstruction* lins, const InterInstruction* sins) const;
	bool DestroyingMem(InterCodeBasicBlock* block, InterInstruction* lins, int from, int to) const;

	// The two memory operations may have overlapping reads/writs and writes
	bool CollidingMem(const InterInstruction* ins1, const InterInstruction* ins2) const;
	bool CollidingMem(const InterOperand& op, InterType type, const InterInstruction* ins) const;
	bool CollidingMem(const InterOperand& op1, InterType type1, const InterOperand& op2, InterType type2) const;
	bool CollidingMem(InterCodeBasicBlock* block, InterInstruction* lins, int from, int to) const;
	bool InvalidatedBy(const InterInstruction* ins, const InterInstruction* by) const;

	// The two memory regions may be aliased but not just the same
	bool AliasingMem(const InterInstruction* ins1, const InterInstruction* ins2) const;
	bool AliasingMem(const InterOperand& op, InterType type, const InterInstruction* ins) const;
	bool AliasingMem(const InterOperand& op1, InterType type1, const InterOperand& op2, InterType type2) const;
	bool AliasingMem(InterCodeBasicBlock* block, InterInstruction* lins, int from, int to) const;

	bool PushSinglePathResultInstructions(void);

	bool CanSwapInstructions(const InterInstruction* ins0, const InterInstruction* ins1) const;
	bool CanMoveInstructionBeforeBlock(int ii) const;
	bool CanMoveInstructionBeforeBlock(int ii, const InterInstruction * ins) const;
	bool CanMoveInstructionBehindBlock(int ii) const;
	bool CanMoveInstructionDown(int si, int ti) const;
	int FindSameInstruction(const InterInstruction* ins) const;
	bool MergeCommonPathInstructions(void);

	bool IsTempModified(int temp);
	bool IsTempModifiedInRange(int from, int to, int temp);
	bool IsTempUsedInRange(int from, int to, int temp);
	bool IsTempReferenced(int temp);
	bool IsTempReferencedInRange(int from, int to, int temp);

	bool IsInsModified(const InterInstruction* ins);
	bool IsInsModifiedInRange(int from, int to, const InterInstruction* ins);

	InterInstruction* FindTempOrigin(int temp) const;
	InterInstruction* FindTempOriginSinglePath(int temp) const;

	bool CheapInlining(int & numTemps);
	bool CollapseDispatch();
	bool StructReturnPropagation(void);

	void CheckFinalLocal(void);
	void CheckFinal(void);
	void CheckBlocks(void);

	bool IsConstExitTemp(int temp) const;
	bool CommonTailCodeMerge(void);
	bool SplitSingleBranchUseConst(void);

	void PeepholeOptimization(const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs);
	bool PeepholeReplaceOptimization(const GrowingVariableArray& staticVars, const GrowingInterCodeProcedurePtrArray& staticProcs);

	bool MoveLoopHeadCheckToTail(void);
	void SingleBlockLoopOptimisation(const NumberSet& aliasedParams, const GrowingVariableArray& staticVars);
	void SingleBlockLoopUnrolling(void);
	bool SingleBlockLoopPointerSplit(int& spareTemps);
	bool SingleBlockLoopPointerToByte(int& spareTemps);
	bool SingleBlockLoopSinking(int& spareTemps);
	bool CollectLoopBody(InterCodeBasicBlock* head, ExpandingArray<InterCodeBasicBlock*> & body);
	bool CollectLoopBodyRecursive(InterCodeBasicBlock* head, ExpandingArray<InterCodeBasicBlock*>& body);
	void CollectLoopPath(const ExpandingArray<InterCodeBasicBlock*>& body, ExpandingArray<InterCodeBasicBlock*>& path);
	void InnerLoopOptimization(const NumberSet& aliasedParams);
	void ConstLoopOptimization(void);
	bool EmptyLoopOptimization(void);
	void EliminateDoubleLoopCounter(void);
	void PushMoveOutOfLoop(void);
	bool MoveConditionOutOfLoop(void);
	void SingleLoopCountZeroCheck(void);
	void InnerLoopCountZeroCheck(void);
	bool PostDecLoopOptimization(void);
	bool Flatten2DLoop(void);

	void PropagateMemoryAliasingInfo(const GrowingInstructionPtrArray& tvalue, bool loops);
	void RemoveUnusedMallocs(void);

	bool PullStoreUpToConstAddress(void);

	bool CollectSingleHeadLoopBody(InterCodeBasicBlock* head, InterCodeBasicBlock* tail, ExpandingArray<InterCodeBasicBlock*>& body);

	bool CollectGenericLoop(ExpandingArray<InterCodeBasicBlock*>& lblocks);
	bool CollectSingleEntryGenericLoop(ExpandingArray<InterCodeBasicBlock*>& lblocks);
	void CollectReachable(ExpandingArray<InterCodeBasicBlock*>& lblock);

	bool SingleTailLoopOptimization(const NumberSet& aliasedParams, const GrowingVariableArray& staticVars);
	bool MergeLoopTails(void);

	bool ChangeTrueJump(InterCodeBasicBlock* block);
	bool ChangeFalseJump(InterCodeBasicBlock* block);

	InterCodeBasicBlock* CheckIsSimpleIntRangeBranch(const GrowingIntegerValueRangeArray & irange);
	InterCodeBasicBlock* CheckIsConstBranch(const GrowingInstructionPtrArray& cins);
	bool ShortcutConstBranches(const GrowingInstructionPtrArray& cins);
	bool ShortcutDuplicateBranches(void);

	InterCodeBasicBlock* BuildLoopPrefix(void);
	void BuildLoopSuffix(void);

	void ExpandSelect(void);

	void SplitBranches(void);
	void FollowJumps(void);

	bool ShortLeaMerge(int& spareTemps);
	bool ShortLeaCleanup(void);

	bool IsEqual(const InterCodeBasicBlock* block) const;

	void CompactInstructions(void);
	bool OptimizeIntervalCompare(void);

	bool DropUnreachable(void);
	
	bool CheckStaticStack(void);
	void ApplyStaticStack(InterOperand& iop, const GrowingVariableArray& localVars);
	void CollectStaticStack(LinkerObject * lobj, const GrowingVariableArray& localVars);
	void CollectStaticStackDependencies(LinkerObject* lobj);
	void PromoteStaticStackParams(LinkerObject* paramlobj);

	bool SameExitCode(const InterCodeBasicBlock* block) const;

	void WarnUsedUndefinedVariables(void);
	void WarnInvalidValueRanges(void);
	void CheckValueReturn(void);
	void CheckNullptrDereference(void);

	void CollectGlobalReferences(NumberSet& referencedGlobals, NumberSet& modifiedGlobals, bool & storesIndirect, bool & loadsIndirect, bool & globalsChecked);
	
	void MarkAliasing(const NumberSet& aliasedParams);
};

class InterCodeProcedure
{
friend class InterCodeBasicBlock;
protected:
	GrowingIntArray						mRenameTable, mRenameUnionTable, mGlobalRenameTable;
	TempForwardingTable					mTempForwardingTable;
	GrowingInstructionPtrArray			mValueForwardingTable;
	GrowingIntegerValueRangeArray		mLocalValueRange, mReverseValueRange;

	void ResetVisited(void);
	void ResetPatched(void);
	void ResetEntryBlocks(void);
public:
	InterCodeBasicBlock				*	mEntryBlock;
	int									mNumBlocks;
	GrowingInterCodeBasicBlockPtrArray	mBlocks;
	GrowingTypeArray					mTemporaries;
	GrowingIntArray						mTempOffset, mTempSizes;
	int									mTempSize, mCommonFrameSize, mCallerSavedTemps, mFreeCallerSavedTemps, mFastCallBase, mNumRestricted;
	bool								mLeafProcedure, mNativeProcedure, mCallsFunctionPointer, mHasDynamicStack, mHasInlineAssembler, mCallsByteCode, mFastCallProcedure;
	bool								mInterrupt, mHardwareInterrupt, mCompiled, mInterruptCalled, mValueReturn, mFramePointer, mDynamicStack, mAssembled;
	bool								mDispatchedCall;
	bool								mCheckUnreachable;
	GrowingInterCodeProcedurePtrArray	mCalledFunctions;
	bool								mCheapInline;
	bool								mNoInline;

	InterCodeModule					*	mModule;
	int									mID;

	int									mLocalSize, mNumLocals, mNumParams, mParamVarsSize;
	GrowingVariableArray				mLocalVars, mParamVars;
	NumberSet							mLocalAliasedSet, mParamAliasedSet;

	Location							mLocation;
	const Ident						*	mIdent, * mSection;

	LinkerObject					*	mLinkerObject, * mSaveTempsLinkerObject;
	Declaration						*	mDeclaration;
	InterType							mReturnType;
	uint64								mCompilerOptions;

	bool								mLoadsIndirect, mStoresIndirect, mGlobalsChecked;
	NumberSet							mReferencedGlobals, mModifiedGlobals;

	InterCodeProcedure(InterCodeModule * module, const Location & location, const Ident * ident, LinkerObject* linkerObject);
	~InterCodeProcedure(void);

	int AddTemporary(InterType type);
	int AddRestricted(void);

	void Close(void);

//	void Set(InterCodeIDMapper* mapper, BitVector localStructure, Scanner scanner, bool debug);

	void AddCalledFunction(InterCodeProcedure* proc);
	void CallsFunctionPointer(void);

	void MarkRelevantStatics(void);
	void RemoveNonRelevantStatics(void);

	void MapCallerSavedTemps(void);

	bool ReferencesGlobal(int varindex);
	bool ModifiesGlobal(int varindex);

	void MapVariables(void);
	void ReduceTemporaries(bool final = false);
	void Disassemble(FILE* file);
	void Disassemble(const char* name, bool dumpSets = false);
protected:
	void BuildLocalAliasTable(void);
	void BuildTraces(int expand, bool dominators = true, bool compact = false);
	void TrimBlocks(void);
	void EarlyBranchElimination(void);
	void BuildDataFlowSets(void);
	void RenameTemporaries(void);
	void TempForwarding(bool reverse = false, bool checkloops = false);
	void RemoveUnusedInstructions(void);
	bool GlobalConstantPropagation(void);
	bool PropagateNonLocalUsedTemps(void);
	void BuildLoopPrefix(void);
	void SingleAssignmentForwarding(void);
	void RemoveUnusedStoreInstructions(InterMemory	paramMemory);
	void RemoveUnusedPartialStoreInstructions(void);
	void RemoveUnusedLocalStoreInstructions(void);
	void MergeCommonPathInstructions(void);
	void PushSinglePathResultInstructions(void);
	void CollectVariables(InterMemory paramMemory);
	void PromoteSimpleLocalsToTemp(InterMemory paramMemory, int nlocals, int nparams);
	void SimplifyIntegerNumeric(FastNumberSet& activeSet);
	void SingleBlockLoopPointerSplit(FastNumberSet& activeSet);
	void SingleBlockLoopPointerToByte(FastNumberSet& activeSet);
	void SingleBlockLoopSinking(FastNumberSet& activeSet);
	void MergeIndexedLoadStore(void);
	void EliminateIntegerSumAliasTemps(void);
	void EliminateAliasValues();
	void LoadStoreForwarding(InterMemory paramMemory);
	void ReduceRecursionTempSpilling(InterMemory paramMemory);
	void ExpandSelect(void);
	void PropagateConstOperationsUp(void);
	void RebuildIntegerRangeSet(void);
	void CombineIndirectAddressing(void);
	void SingleTailLoopOptimization(InterMemory paramMemory);
	void HoistCommonConditionalPath(void);
	void RemoveUnusedMallocs(void);
	void RecheckLocalAliased(void);
	void ConstLoopOptimization(void);

	bool ShortLeaMerge(FastNumberSet& activeSet);

	void MergeBasicBlocks(FastNumberSet& activeSet);
	void CheckUsedDefinedTemps(void);
	void WarnUsedUndefinedVariables(void);
	void WarnInvalidValueRanges(void);
	void PropagateMemoryAliasingInfo(bool loops);
	void MoveConditionsOutOfLoop(void);
	void ShortcutConstBranches(void);
	void ShortcutDuplicateBranches(void);
	void EliminateDoubleLoopCounter(void);
	void StructReturnPropagation(void);

	void CollapseDispatch(void);

	void PeepholeOptimization(void);

	void CheckFinal(void);
	void CheckBlocks(void);

	void DisassembleDebug(const char* name);
};

class InterCodeModule
{
public:
	InterCodeModule(Errors* errors, Linker * linker);
	~InterCodeModule(void);

	void InitParamStack(LinkerSection * stackSection);

	bool Disassemble(const char* name);

	GrowingInterCodeProcedurePtrArray	mProcedures;

	GrowingVariableArray				mGlobalVars;
	LinkerObject					*	mParamLinkerObject;
	LinkerSection					*	mParamLinkerSection;

	Linker							*	mLinker;
	Errors* mErrors;

	uint64				mCompilerOptions;

};

#pragma once

#include "Array.h"
#include "NumberSet.h"
#include "Errors.h"
#include "BitVector.h"
#include <stdio.h>
#include "MachineTypes.h"
#include "Ident.h"
#include "Linker.h"

enum InterCode
{
	IC_NONE,
	IC_LOAD_TEMPORARY,
	IC_STORE_TEMPORARY,
	IC_BINARY_OPERATOR,
	IC_UNARY_OPERATOR,
	IC_RELATIONAL_OPERATOR,
	IC_CONVERSION_OPERATOR,
	IC_STORE,
	IC_LOAD,
	IC_LEA,
	IC_COPY,
	IC_STRCPY,
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
	IC_UNREACHABLE
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
	IM_PARAM,
	IM_LOCAL,
	IM_GLOBAL,
	IM_FRAME,
	IM_PROCEDURE,
	IM_INDIRECT,
	IM_TEMPORARY,
	IM_ABSOLUTE,
	IM_FPARAM,
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

class IntegerValueRange
{
public:
	IntegerValueRange(void);
	~IntegerValueRange(void);

	int64		mMinValue, mMaxValue;
	
	enum State
	{
		S_UNKNOWN,
		S_UNBOUND,
		S_WEAK,
		S_BOUND
	}			mMinState, mMaxState;

	bool Same(const IntegerValueRange& range) const;
	bool Merge(const IntegerValueRange& range, bool head);

	bool IsConstant(void) const;

	void LimitMin(int64 value);
	void LimitMax(int64 value);

	void LimitMinWeak(int64 value);
	void LimitMaxWeak(int64 value);
};



typedef GrowingArray<IntegerValueRange>		GrowingIntegerValueRangeArray;

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

	void UpdateValue(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, const GrowingVariableArray& staticVars);
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

	void SetSize(int size);

	void Reset(void);

	int operator[](int n);

	void Destroy(int n);

	void Build(int from, int to);
};

class InterVariable
{
public:
	Location						mLocation;
	bool							mUsed, mAliased;
	int								mIndex, mSize, mOffset, mAddr;
	int								mNumReferences;
	const Ident					*	mIdent;
	LinkerObject				*	mLinkerObject;

	InterVariable(void)
		: mUsed(false), mAliased(false), mIndex(-1), mSize(0), mOffset(0), mIdent(nullptr), mLinkerObject(nullptr)
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
	int					mVarIndex, mOperandSize;
	LinkerObject	*	mLinkerObject;
	InterMemory			mMemory;
	IntegerValueRange	mRange;

	InterOperand(void);

	bool IsEqual(const InterOperand & op) const;

	bool IsUByte(void) const;
	bool IsUnsigned(void) const;

	void Disassemble(FILE* file);
};

class InterInstruction
{
public:
	InterCode							mCode;
	InterOperand						mSrc[8];
	InterOperand						mDst;
	InterOperand						mConst;
	InterOperator						mOperator;
	int									mNumOperands;
	Location							mLocation;

	bool								mInUse, mInvariant, mVolatile, mExpensive, mSingleAssignment;

	InterInstruction(void);

	bool IsEqual(const InterInstruction* ins) const;
	bool IsEqualSource(const InterInstruction* ins) const;

	InterInstruction* Clone(void) const;

	bool ReferencesTemp(int temp) const;
	bool UsesTemp(int temp) const;

	void SetCode(const Location & loc, InterCode code);

	void CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable);
	void MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams);

	void FilterTempUsage(NumberSet& requiredTemps, NumberSet& providedTemps);
	void FilterVarsUsage(const GrowingVariableArray& localVars, NumberSet& requiredVars, NumberSet& providedVars, const GrowingVariableArray& params, NumberSet& requiredParams, NumberSet& providedParams, InterMemory paramMemory);
	void FilterStaticVarsUsage(const GrowingVariableArray& staticVars, NumberSet& requiredVars, NumberSet& providedVars);
	
	bool RemoveUnusedResultInstructions(InterInstruction* pre, NumberSet& requiredTemps);
	bool RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, NumberSet& requiredVars, const GrowingVariableArray& params, NumberSet& requiredParams, InterMemory paramMemory);
	bool RemoveUnusedStaticStoreInstructions(const GrowingVariableArray& staticVars, NumberSet& requiredVars);
	void PerformValueForwarding(GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid);
	void BuildCallerSaveTempSet(NumberSet& requiredTemps, NumberSet& callerSaveTemps);

	void LocalRenameRegister(GrowingIntArray& renameTable, int& num);
	void GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void PerformTempForwarding(TempForwardingTable& forwardingTable);
	bool PropagateConstTemps(const GrowingInstructionPtrArray& ctemps);

	void BuildCollisionTable(NumberSet& liveTemps, NumberSet* collisionSets);
	void ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void CollectActiveTemporaries(FastNumberSet& set);
	void ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries);
	
	void CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes);
	void SimpleLocalToTemp(int vindex, int temp);

	bool ConstantFolding(void);
	bool ConstantFoldingRelationRange(void);

	void Disassemble(FILE* file);
};

class InterCodeBasicBlock
{
public:
	int								mIndex, mNumEntries, mNumEntered, mTraceIndex;
	InterCodeBasicBlock			*	mTrueJump, * mFalseJump, * mLoopPrefix, * mDominator;
	GrowingInstructionArray			mInstructions;

	bool							mVisited, mInPath, mLoopHead, mChecked, mConditionBlockTrue, mUnreachable, mLoopPath;

	NumberSet						mLocalRequiredTemps, mLocalProvidedTemps;
	NumberSet						mEntryRequiredTemps, mEntryProvidedTemps;
	NumberSet						mExitRequiredTemps, mExitProvidedTemps;

	NumberSet						mLocalRequiredVars, mLocalProvidedVars;
	NumberSet						mEntryRequiredVars, mEntryProvidedVars;
	NumberSet						mExitRequiredVars, mExitProvidedVars;

	NumberSet						mLocalRequiredStatics, mLocalProvidedStatics;
	NumberSet						mEntryRequiredStatics, mEntryProvidedStatics;
	NumberSet						mExitRequiredStatics, mExitProvidedStatics;

	NumberSet						mLocalRequiredParams, mLocalProvidedParams;
	NumberSet						mEntryRequiredParams, mEntryProvidedParams;
	NumberSet						mExitRequiredParams, mExitProvidedParams;

	GrowingInstructionArray			mLoadStoreInstructions;

	GrowingIntegerValueRangeArray	mEntryValueRange, mTrueValueRange, mFalseValueRange, mLocalValueRange;

	GrowingArray<InterCodeBasicBlock*>	mEntryBlocks, mLoopPathBlocks;

	GrowingInstructionPtrArray		mMergeTValues;
	ValueSet						mMergeValues;
	TempForwardingTable				mMergeForwardingTable;

	InterCodeBasicBlock(void);
	~InterCodeBasicBlock(void);

	void Append(InterInstruction * code);
	void Close(InterCodeBasicBlock* trueJump, InterCodeBasicBlock* falseJump);

	void CollectEntries(void);
	void CollectEntryBlocks(InterCodeBasicBlock* from);
	void GenerateTraces(bool expand);
	void BuildDominatorTree(InterCodeBasicBlock * from);

	void LocalToTemp(int vindex, int temp);

	void CollectLocalAddressTemps(GrowingIntArray& localTable, GrowingIntArray& paramTable);
	void MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals, const GrowingIntArray& paramTable, NumberSet& aliasedParams);

	void CollectConstTemps(GrowingInstructionPtrArray& ctemps, NumberSet& assignedTemps);
	bool PropagateConstTemps(const GrowingInstructionPtrArray& ctemps);

	void BuildLocalTempSets(int num);
	void BuildGlobalProvidedTempSet(NumberSet fromProvidedTemps);
	bool BuildGlobalRequiredTempSet(NumberSet& fromRequiredTemps);
	bool RemoveUnusedResultInstructions(void);
	void BuildCallerSaveTempSet(NumberSet& callerSaveTemps);

	void BuildLocalVariableSets(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory);
	void BuildGlobalProvidedVariableSet(const GrowingVariableArray& localVars, NumberSet fromProvidedVars, const GrowingVariableArray& params, NumberSet fromProvidedParams, InterMemory paramMemory);
	bool BuildGlobalRequiredVariableSet(const GrowingVariableArray& localVars, NumberSet& fromRequiredVars, const GrowingVariableArray& params, NumberSet& fromRequiredParams, InterMemory paramMemory);
	bool RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, const GrowingVariableArray& params, InterMemory paramMemory);

	void BuildStaticVariableSet(const GrowingVariableArray& staticVars);
	void BuildGlobalProvidedStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet fromProvidedVars);
	bool BuildGlobalRequiredStaticVariableSet(const GrowingVariableArray& staticVars, NumberSet& fromRequiredVars);
	bool RemoveUnusedStaticStoreInstructions(const GrowingVariableArray& staticVars);

	void RestartLocalIntegerRangeSets(void);
	void BuildLocalIntegerRangeSets(int num);
	void UpdateLocalIntegerRangeSets(void);
	bool BuildGlobalIntegerRangeSets(void);
	void SimplifyIntegerRangeRelops(void);

	GrowingIntArray			mEntryRenameTable;
	GrowingIntArray			mExitRenameTable;

	void LoadStoreForwarding(const GrowingInstructionPtrArray& tvalue);

	void LocalRenameRegister(const GrowingIntArray& renameTable, int& num);
	void BuildGlobalRenameRegisterTable(const GrowingIntArray& renameTable, GrowingIntArray& globalRenameTable);
	void GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void CheckValueUsage(InterInstruction * ins, const GrowingInstructionPtrArray& tvalue);
	void PerformTempForwarding(TempForwardingTable& forwardingTable);
	void PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals, const NumberSet& aliasedParams, int & spareTemps, const GrowingVariableArray& staticVars);
	void PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid);
	bool EliminateDeadBranches(void);

	bool CalculateSingleAssignmentTemps(FastNumberSet& tassigned, GrowingInstructionPtrArray& tvalue, NumberSet& modifiedParams, InterMemory paramMemory);
	bool SingleAssignmentTempForwarding(const GrowingInstructionPtrArray& tunified, const GrowingInstructionPtrArray& tvalues);

	void BuildCollisionTable(NumberSet* collisionSets);
	void ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries);

	void CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray & localTypes, FastNumberSet& complexParams, FastNumberSet& simpleParams, GrowingTypeArray& paramTypes);
	void SimpleLocalToTemp(int vindex, int temp);

	void CollectActiveTemporaries(FastNumberSet& set);
	void ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries);

	void ApplyExceptionStackChanges(GrowingInterCodeBasicBlockPtrArray& exceptionStack);

	void Disassemble(FILE* file, bool dumpSets);

	void CollectVariables(GrowingVariableArray & globalVars, GrowingVariableArray & localVars, GrowingVariableArray& paramVars, InterMemory	paramMemory);
	void MapVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars);
	
	void CollectOuterFrame(int level, int& size, bool& inner, bool& inlineAssembler, bool& byteCodeCall);

	bool IsLeafProcedure(void);

	void MarkRelevantStatics(void);
	void RemoveNonRelevantStatics(void);

	bool IsTempModifiedOnPath(int temp, int at) const;

	bool PushSinglePathResultInstructions(void);
	bool MergeCommonPathInstructions(void);

	void PeepholeOptimization(void);
	void SingleBlockLoopOptimisation(const NumberSet& aliasedParams);
	void SingleBlockLoopUnrolling(void);
	bool CollectLoopBody(InterCodeBasicBlock* head, GrowingArray<InterCodeBasicBlock*> & body);
	void CollectLoopPath(const GrowingArray<InterCodeBasicBlock*>& body, GrowingArray<InterCodeBasicBlock*>& path);
	void InnerLoopOptimization(const NumberSet& aliasedParams);

	InterCodeBasicBlock* BuildLoopPrefix(InterCodeProcedure * proc);

	void SplitBranches(InterCodeProcedure* proc);
	void FollowJumps(void);

	bool IsEqual(const InterCodeBasicBlock* block) const;

	void CompactInstructions(void);
	bool OptimizeIntervalCompare(void);

	bool DropUnreachable(void);
};

class InterCodeModule;

class InterCodeProcedure
{
protected:
	GrowingIntArray						mRenameTable, mRenameUnionTable, mGlobalRenameTable;
	TempForwardingTable					mTempForwardingTable;
	GrowingInstructionPtrArray			mValueForwardingTable;
	NumberSet							mLocalAliasedSet, mParamAliasedSet;

	void ResetVisited(void);
	void ResetEntryBlocks(void);
public:
	InterCodeBasicBlock				*	mEntryBlock;
	GrowingInterCodeBasicBlockPtrArray	mBlocks;
	GrowingTypeArray					mTemporaries;
	GrowingIntArray						mTempOffset, mTempSizes;
	int									mTempSize, mCommonFrameSize, mCallerSavedTemps;
	bool								mLeafProcedure, mNativeProcedure, mCallsFunctionPointer, mHasDynamicStack, mHasInlineAssembler, mCallsByteCode, mFastCallProcedure, mInterrupt;
	GrowingInterCodeProcedurePtrArray	mCalledFunctions;

	InterCodeModule					*	mModule;
	int									mID;

	int									mLocalSize, mNumLocals;
	GrowingVariableArray				mLocalVars, mParamVars;

	Location							mLocation;
	const Ident						*	mIdent, * mSection;

	LinkerObject					*	mLinkerObject;

	InterCodeProcedure(InterCodeModule * module, const Location & location, const Ident * ident, LinkerObject* linkerObject);
	~InterCodeProcedure(void);

	int AddTemporary(InterType type);

	void Append(InterCodeBasicBlock * block);
	void Close(void);

//	void Set(InterCodeIDMapper* mapper, BitVector localStructure, Scanner scanner, bool debug);

	void AddCalledFunction(InterCodeProcedure* proc);
	void CallsFunctionPointer(void);

	void MarkRelevantStatics(void);
	void RemoveNonRelevantStatics(void);

	void MapVariables(void);
	void ReduceTemporaries(void);
	void Disassemble(FILE* file);
	void Disassemble(const char* name, bool dumpSets = false);
protected:
	void BuildTraces(bool expand);
	void BuildDataFlowSets(void);
	void RenameTemporaries(void);
	void TempForwarding(void);
	void RemoveUnusedInstructions(void);
	bool GlobalConstantPropagation(void);
	void BuildLoopPrefix(void);
	void SingleAssignmentForwarding(void);

	void MergeBasicBlocks(void);

	void DisassembleDebug(const char* name);
};

class InterCodeModule
{
public:
	InterCodeModule(void);
	~InterCodeModule(void);

	bool Disassemble(const char* name);

	GrowingInterCodeProcedurePtrArray	mProcedures;

	GrowingVariableArray				mGlobalVars;

	uint64				mCompilerOptions;

};

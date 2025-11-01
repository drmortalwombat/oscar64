#pragma once

#include "Declaration.h"
#include "Linker.h"
#include "CompilerTypes.h"

class GlobalOptimizer
{
public:
	GlobalOptimizer(Errors* errors, Linker* linker);
	~GlobalOptimizer(void);

	void Reset(void);
	bool Optimize(void);

	void AnalyzeProcedure(Expression* exp, Declaration* dec);
	void AnalyzeAssembler(Expression* exp, Declaration* dec);
	void AnalyzeGlobalVariable(Declaration* dec);

	uint64		mCompilerOptions;

protected:
	Errors			*	mErrors;
	Linker			*	mLinker;

	ExpandingArray<Declaration*>		mCalledFunctions, mCallingFunctions, mVariableFunctions, mFunctions;
	ExpandingArray<Declaration*>		mGlobalVariables;

	void AnalyzeInit(Declaration* mdec);
	Declaration* Analyze(Expression* exp, Declaration* procDec, uint32 flags);

	void RegisterCall(Declaration* from, Declaration* to);
	void RegisterProc(Declaration* to);

	bool EstimateCost(Expression* exp, Declaration* vindex, int64& cycles, int64& bytes, bool& cconst);

	bool CheckConstFunction(Expression* exp);
	void RemoveValueReturn(Expression* exp);
	bool CheckUnusedReturns(Expression*& exp);
	bool CheckConstReturns(Expression*& exp);
	bool CheckUnusedLocals(Expression*& exp);
	void UndoParamReference(Expression* exp, Declaration* param);
	bool ReplaceParamConst(Expression* exp, Declaration* param);
	void PropagateCommas(Expression*& exp);
	void PropagateParamCommas(Expression *& fexp, Expression*& exp);
	bool ReplaceGlobalConst(Expression* exp);
	bool ReplaceConstCalls(Expression*& exp);
	bool UnrollLoops(Expression*& exp);

	bool CheckSplitAggregates(Declaration* func, Expression* exp);
	void SplitLocalAggregate(Expression* exp, Declaration* var, ExpandingArray<Declaration *> & evars);
};

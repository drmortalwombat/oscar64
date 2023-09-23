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
	Errors* mErrors;
	Linker* mLinker;

	ExpandingArray<Declaration*>		mCalledFunctions, mCallingFunctions, mVariableFunctions, mFunctions;
	ExpandingArray<Declaration*>		mGlobalVariables;

	void AnalyzeInit(Declaration* mdec);
	Declaration* Analyze(Expression* exp, Declaration* procDec, uint32 flags);

	void RegisterCall(Declaration* from, Declaration* to);
	void RegisterProc(Declaration* to);

	void RemoveValueReturn(Expression* exp);
	bool CheckConstReturns(Expression*& exp);
	bool CheckUnusedLocals(Expression*& exp);
	void UndoParamReference(Expression* exp, Declaration* param);
	bool ReplaceParamConst(Expression* exp, Declaration* param);
	void PropagateCommas(Expression*& exp);
	void PropagateParamCommas(Expression *& fexp, Expression*& exp);

};

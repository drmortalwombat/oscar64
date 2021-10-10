#pragma once

#include "Declaration.h"
#include "Linker.h"

class GlobalAnalyzer
{
public:
	GlobalAnalyzer(Errors* errors, Linker* linker);
	~GlobalAnalyzer(void);

	void DumpCallGraph(void);
	void AutoInline(void);

	void AnalyzeProcedure(Expression* exp, Declaration* procDec);
	void AnalyzeAssembler(Expression* exp, Declaration* procDec);
	void AnalyzeGlobalVariable(Declaration* dec);

protected:
	Errors* mErrors;
	Linker* mLinker;

	GrowingArray<Declaration*>		mCalledFunctions, mCallingFunctions, mVariableFunctions, mFunctions;

	Declaration* Analyze(Expression* exp, Declaration* procDec);

	void RegisterCall(Declaration* from, Declaration* to);
	void RegisterProc(Declaration* to);
};


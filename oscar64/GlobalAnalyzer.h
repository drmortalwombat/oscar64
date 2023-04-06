#pragma once

#include "Declaration.h"
#include "Linker.h"
#include "CompilerTypes.h"

class GlobalAnalyzer
{
public:
	GlobalAnalyzer(Errors* errors, Linker* linker);
	~GlobalAnalyzer(void);

	void DumpCallGraph(void);
	void AutoInline(void);
	void CheckFastcall(Declaration* procDec);
	void CheckInterrupt(void);

	void AnalyzeProcedure(Expression* exp, Declaration* procDec);
	void AnalyzeAssembler(Expression* exp, Declaration* procDec);
	void AnalyzeGlobalVariable(Declaration* dec);

	uint64		mCompilerOptions;

protected:
	Errors* mErrors;
	Linker* mLinker;

	GrowingArray<Declaration*>		mCalledFunctions, mCallingFunctions, mVariableFunctions, mFunctions;

	Declaration* Analyze(Expression* exp, Declaration* procDec, bool lhs);

	uint64 GetProcFlags(Declaration* to) const;
	void RegisterCall(Declaration* from, Declaration* to);
	void RegisterProc(Declaration* to);
};


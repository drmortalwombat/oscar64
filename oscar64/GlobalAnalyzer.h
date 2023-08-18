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
	void CheckFastcall(Declaration* procDec, bool head);
	void CheckInterrupt(void);
	void AutoZeroPage(LinkerSection * lszp, int zpsize);
	void MarkRecursions(void);

	void AnalyzeProcedure(Expression* exp, Declaration* procDec);
	void AnalyzeAssembler(Expression* exp, Declaration* procDec);
	void AnalyzeGlobalVariable(Declaration* dec);

	uint64		mCompilerOptions;

protected:
	Errors* mErrors;
	Linker* mLinker;

	GrowingArray<Declaration*>		mCalledFunctions, mCallingFunctions, mVariableFunctions, mFunctions;
	GrowingArray<Declaration*>		mGlobalVariables;

	void AnalyzeInit(Declaration* mdec);

	Declaration* Analyze(Expression* exp, Declaration* procDec, bool lhs);

	bool IsStackParam(const Declaration* pdec) const;
	bool MarkCycle(Declaration* rootDec, Declaration* procDec);

	uint64 GetProcFlags(Declaration* to) const;
	void RegisterCall(Declaration* from, Declaration* to);
	void RegisterProc(Declaration* to);
	void UndoParamReference(Expression* ex, Declaration* param);
};


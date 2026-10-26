#pragma once

#include "Declaration.h"
#include "Linker.h"
#include "CompilerTypes.h"

class FunctionSets
{
public:
	FunctionSets(Errors* errors, Linker* linker);
	~FunctionSets(void);

	void AnalyzeProcedure(Expression* exp, Declaration* procDec);
	void AnalyzeAssembler(Expression* exp, Declaration* procDec);
	void AnalyzeGlobalVariable(Declaration* dec);

	void CompleteAnalyze(void);

	ExpandingArray<Declaration*>		mUnions;
	ExpandingArray<Declaration*>		mFunctions, mVariables, mTypes;
protected:
	Errors* mErrors;
	Linker* mLinker;

	Declaration * AnalyzeInit(Declaration* mdec);
	Declaration* CanonicalType(Declaration* tdec);
	Declaration * Analyze(Expression* exp, Declaration* procDec);
	Declaration* Find(Declaration* ldec);
	Declaration * Unify(Declaration* ldec, Declaration* rdec);
};
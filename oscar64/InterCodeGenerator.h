#pragma once

#include "Parser.h"
#include "InterCode.h"
#include "Linker.h"
#include "CompilerTypes.h"

class InterCodeGenerator
{
public:
	InterCodeGenerator(Errors * errors, Linker * linker);
	~InterCodeGenerator(void);

	struct ExValue
	{
		Declaration* mType;
		int		mTemp, mReference;

		ExValue(Declaration* type = nullptr, int temp = -1, int reference = 0)
			: mType(type), mTemp(temp), mReference(reference)
		{}
	};

	uint64		mCompilerOptions;

	InterCodeProcedure* TranslateProcedure(InterCodeModule* mod, Expression* exp, Declaration * dec);
	void TranslateAssembler(InterCodeModule* mod, Expression * exp, GrowingArray<Declaration *>	* refvars);
	void InitGlobalVariable(InterCodeModule* mod, Declaration* dec);
protected:

	Errors* mErrors;
	Linker* mLinker;
	
	struct InlineMapper
	{
		GrowingArray<int>		mParams;
		InterCodeBasicBlock	*	mReturn;
		int						mResult, mDepth, mVarIndex;
		bool					mConstExpr;

		InlineMapper(void)
			: mParams(-1), mResult(-1), mDepth(0)
		{}
	};

	struct SwitchNode
	{
		int						mValue;
		InterCodeBasicBlock* mBlock;
	};

	typedef GrowingArray<SwitchNode>	SwitchNodeArray;

	void BuildSwitchTree(InterCodeProcedure* proc, InterCodeBasicBlock* block, ExValue v, const SwitchNodeArray& nodes, int left, int right, InterCodeBasicBlock* dblock);

	ExValue Dereference(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, int level = 0);
	ExValue CoerceType(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, Declaration * type);
	ExValue TranslateExpression(Declaration * procType, InterCodeProcedure * proc, InterCodeBasicBlock*& block, Expression* exp, InterCodeBasicBlock* breakBlock, InterCodeBasicBlock* continueBlock, InlineMapper * inlineMapper, ExValue * lrexp = nullptr);
	void TranslateLogic(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock* block, InterCodeBasicBlock* tblock, InterCodeBasicBlock* fblock, Expression* exp, InlineMapper* inlineMapper);

	void BuildInitializer(InterCodeModule* mod, uint8 * dp, int offset, Declaration* data, InterVariable * variable);
};

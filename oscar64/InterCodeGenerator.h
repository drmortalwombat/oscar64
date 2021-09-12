#pragma once

#include "Parser.h"
#include "InterCode.h"

class InterCodeGenerator
{
public:
	InterCodeGenerator(Errors * errors);
	~InterCodeGenerator(void);

	struct ExValue
	{
		Declaration* mType;
		int		mTemp, mReference;

		ExValue(Declaration* type = nullptr, int temp = -1, int reference = 0)
			: mType(type), mTemp(temp), mReference(reference)
		{}
	};

	bool		mForceNativeCode;

	InterCodeProcedure* TranslateProcedure(InterCodeModule* mod, Expression* exp, Declaration * dec);
	void TranslateAssembler(InterCodeModule* mod, Expression * exp);
	void InitGlobalVariable(InterCodeModule* mod, Declaration* dec);
protected:

	Errors* mErrors;

	ExValue Dereference(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, int level = 0);
	ExValue CoerceType(InterCodeProcedure* proc, InterCodeBasicBlock*& block, ExValue v, Declaration * type);
	ExValue TranslateExpression(Declaration * procType, InterCodeProcedure * proc, InterCodeBasicBlock*& block, Expression* exp, InterCodeBasicBlock* breakBlock, InterCodeBasicBlock* continueBlock);
	void TranslateLogic(Declaration* procType, InterCodeProcedure* proc, InterCodeBasicBlock* block, InterCodeBasicBlock* tblock, InterCodeBasicBlock* fblock, Expression* exp);

	void BuildInitializer(InterCodeModule* mod, uint8 * dp, int offset, Declaration* data, GrowingArray<InterVariable::Reference> & references);
};

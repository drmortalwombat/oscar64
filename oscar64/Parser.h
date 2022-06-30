#pragma once

#include "Scanner.h"
#include "Declaration.h"
#include "CompilationUnits.h"

class Parser
{
public:
	Parser(Errors * errors, Scanner* scanner, CompilationUnits * compilationUnits);
	~Parser(void);

	DeclarationScope	*	mGlobals, * mScope;
	int						mLocalIndex;
	CompilationUnits	*	mCompilationUnits;
	
	LinkerSection	* mCodeSection, * mDataSection, * mBSSection;

	uint64		mCompilerOptions;

	void Parse(void);
protected:
	bool ConsumeToken(Token token);
	bool ConsumeTokenIf(Token token);

	char			mCharMap[256];
	int				mUnrollLoop;

	uint8* ParseStringLiteral(int msize);

	void ParsePragma(void);

	Declaration* ParseBaseTypeDeclaration(uint64 flags);
	Declaration* ParseDeclaration(bool variable);
	Declaration* ParseStructDeclaration(uint64 flags, DecType dt);

	Declaration* CopyConstantInitializer(int offset, Declaration* dtype, Expression* exp);
	Expression* ParseInitExpression(Declaration* dtype);
	Expression* ParseDeclarationExpression(void);

	Declaration* ParsePostfixDeclaration(void);
	Declaration* ReverseDeclaration(Declaration* odec, Declaration* bdec);

	Expression* ParseFunction(Declaration* dec);
	Expression* ParseAssembler(void);

	Expression* ParseAssemblerBaseOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerMulOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerAddOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerOperand(Declaration * pcasm, int pcoffset);

	void AddAssemblerRegister(const Ident* ident, int value);

	Expression* ParseStatement(void);
	Expression* ParseSwitchStatement(void);

	Expression* ParseSimpleExpression(void);
	Expression* ParsePrefixExpression(void);
	Expression* ParsePostfixExpression(void);
	Expression* ParseMulExpression(void);
	Expression* ParseAddExpression(void);
	Expression* ParseShiftExpression(void);
	Expression* ParseRelationalExpression(void);
	Expression* ParseBinaryAndExpression(void);
	Expression* ParseBinaryXorExpression(void);
	Expression* ParseBinaryOrExpression(void);
	Expression* ParseLogicAndExpression(void);
	Expression* ParseLogicOrExpression(void);
	Expression* ParseConditionalExpression(void);
	Expression* ParseAssignmentExpression(void);
	Expression* ParseExpression(void);
	Expression* ParseRExpression(void);
	Expression* ParseListExpression(void);

	Expression* ParseParenthesisExpression(void);

	Errors* mErrors;
	Scanner* mScanner;
};

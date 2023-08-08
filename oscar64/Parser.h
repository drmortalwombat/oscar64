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
	Declaration			*	mThisPointer, * mReturnType;
	
	LinkerSection	* mCodeSection, * mDataSection, * mBSSection;

	uint64			mCompilerOptions;
	uint64			mCompilerOptionStack[32];
	int				mCompilerOptionSP;

	void Parse(void);
protected:
	bool ExpectToken(Token token);
	bool ConsumeToken(Token token);
	bool ConsumeTokenIf(Token token);
	bool ConsumeIdentIf(const char* ident);

	char			mCharMap[256];
	int				mUnrollLoop;
	bool			mUnrollLoopPage;
	bool			mInlineCall;

	uint8* ParseStringLiteral(int msize);

	void ParseNamespace(void);

	void ParsePragma(void);

	Declaration * ParseFunctionDeclaration(Declaration* bdec);
	void PrependThisArgument(Declaration* fdec, Declaration * pthis);
	void AppendMemberDestructor(Declaration* pthis);
	void BuildMemberConstructor(Declaration* pthis, Declaration* cfunc);
	Expression* BuildMemberInitializer(Expression* vexp);
	void PrependMemberConstructor(Declaration* pthis, Declaration* cfunc);

	void AddDefaultConstructors(Declaration* pthis);

	void ParseVariableInit(Declaration* ndec);
	void AddMemberFunction(Declaration* dec, Declaration* mdec);
	Declaration* FindBaseMemberFunction(Declaration* dec, Declaration* mdec);

	Expression * AddFunctionCallRefReturned(Expression * exp);
	Expression* CleanupExpression(Expression* exp);

	Declaration* ParseBaseTypeDeclaration(uint64 flags, bool qualified);
	Declaration* ParseDeclaration(Declaration* pdec, bool variable, bool expression, Declaration * pthis = nullptr);
	Declaration* ParseStructDeclaration(uint64 flags, DecType dt);

	Declaration* CopyConstantInitializer(int offset, Declaration* dtype, Expression* exp);
	Expression* ParseInitExpression(Declaration* dtype);
	Expression* ParseDeclarationExpression(Declaration* pdec);

	Declaration* ParsePostfixDeclaration(void);
	Declaration* ReverseDeclaration(Declaration* odec, Declaration* bdec);

	Expression* ParseFunction(Declaration* dec);
	Expression* ParseAssembler(void);

	Expression* ParseAssemblerBaseOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerMulOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerAddOperand(Declaration* pcasm, int pcoffset);
	Expression* ParseAssemblerOperand(Declaration * pcasm, int pcoffset);

	Expression* CheckOperatorOverload(Expression* exp);

	void AddAssemblerRegister(const Ident* ident, int value);

	Declaration* ParseQualIdent(void);

	Expression* ParseStatement(void);
	Expression* ParseSwitchStatement(void);

	Declaration* MemberLookup(Declaration* dtype, const Ident * ident, int& offset, uint64 & flags);

	Expression* ParseQualify(Expression * exp);
	
	int OverloadDistance(Declaration* pdec, Expression* pexp);
	Expression * ResolveOverloadCall(Expression* exp, Expression * exp2 = nullptr);
	Expression* CoerceExpression(Expression* exp, Declaration* type);
	bool CanCoerceExpression(Expression* exp, Declaration* type);
	void CompleteFunctionDefaultParams(Expression* exp);

	void ParseTemplate(void);

	Expression* ParseSimpleExpression(bool lhs);
	Expression* ParsePrefixExpression(bool lhs);
	Expression* ParsePostfixExpression(bool lhs);
	Expression* ParseMulExpression(bool lhs);
	Expression* ParseAddExpression(bool lhs);
	Expression* ParseShiftExpression(bool lhs);
	Expression* ParseRelationalExpression(bool lhs);
	Expression* ParseBinaryAndExpression(bool lhs);
	Expression* ParseBinaryXorExpression(bool lhs);
	Expression* ParseBinaryOrExpression(bool lhs);
	Expression* ParseLogicAndExpression(bool lhs);
	Expression* ParseLogicOrExpression(bool lhs);
	Expression* ParseConditionalExpression(bool lhs);
	Expression* ParseAssignmentExpression(bool lhs);
	Expression* ParseExpression(bool lhs);
	Expression* ParseRExpression(void);
	Expression* ParseListExpression(bool lhs);

	Expression* ParseParenthesisExpression(void);

	Errors* mErrors;
	Scanner* mScanner;
};

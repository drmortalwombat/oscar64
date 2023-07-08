#pragma once

#include "Ident.h"
#include "Errors.h"
#include "Preprocessor.h"

enum Token
{
	TK_NONE,
	TK_EOF,
	TK_ERROR,
	TK_EOL,

	TK_IF,
	TK_ELSE,
	TK_WHILE,
	TK_DO,
	TK_FOR,
	TK_VOID,
	TK_INT,
	TK_CHAR,
	TK_FLOAT,
	TK_UNSIGNED,
	TK_SIGNED,
	TK_SWITCH,
	TK_CASE,
	TK_DEFAULT,
	TK_BREAK,
	TK_RETURN,
	TK_SHORT,
	TK_LONG,
	TK_CONTINUE,
	TK_INTEGER,
	TK_INTEGERU,
	TK_INTEGERL,
	TK_INTEGERUL,
	TK_BOOL,
	TK_CONST,
	TK_VOLATILE,
	TK_TYPEDEF,
	TK_STRUCT,
	TK_UNION,
	TK_ENUM,
	TK_SIZEOF,
	TK_BANKOF,
	TK_STATIC,
	TK_AUTO,
	TK_EXTERN,
	TK_INLINE,
	TK_ASSUME,

	TK_ASM,
	TK_INTERRUPT,
	TK_HWINTERRUPT,
	TK_NATIVE,
	TK_FASTCALL,
	TK_EXPORT,
	TK_ZEROPAGE,
	TK_NOINLINE,
	TK_STRIPED,
	TK_DYNSTACK,

	TK_NUMBER,
	TK_CHARACTER,
	TK_STRING,
	TK_IDENT,
	TK_TRUE,
	TK_FALSE,
	TK_NULL,

	TK_ADD,
	TK_SUB,
	TK_MUL,
	TK_DIV,
	TK_MOD,

	TK_LEFT_SHIFT,
	TK_RIGHT_SHIFT,

	TK_INC,
	TK_DEC,

	TK_LOGICAL_NOT,
	TK_LOGICAL_AND,
	TK_LOGICAL_OR,

	TK_BINARY_NOT,
	TK_BINARY_AND,
	TK_BINARY_OR,
	TK_BINARY_XOR,

	TK_EQUAL,
	TK_NOT_EQUAL,
	TK_GREATER_THAN,
	TK_GREATER_EQUAL,
	TK_LESS_THAN,
	TK_LESS_EQUAL,

	TK_ASSIGN,
	TK_ASSIGN_ADD,
	TK_ASSIGN_SUB,
	TK_ASSIGN_MUL,
	TK_ASSIGN_DIV,
	TK_ASSIGN_MOD,
	TK_ASSIGN_SHL,
	TK_ASSIGN_SHR,
	TK_ASSIGN_AND,
	TK_ASSIGN_XOR,
	TK_ASSIGN_OR,

	TK_ARROW,
	TK_HASH,
	TK_DOLLAR,

	TK_OPEN_PARENTHESIS,
	TK_CLOSE_PARENTHESIS,

	TK_OPEN_BRACE,
	TK_CLOSE_BRACE,

	TK_OPEN_BRACKET,
	TK_CLOSE_BRACKET,

	TK_DOT,
	TK_DOTDOT,
	TK_ELLIPSIS,
	TK_COMMA,
	TK_SEMICOLON,
	TK_COLON,
	TK_QUESTIONMARK,

	TK_EMBEDDED,

	TK_PREP_DEFINE,
	TK_PREP_UNDEF,
	TK_PREP_INCLUDE,
	TK_PREP_IF,
	TK_PREP_ELIF,
	TK_PREP_ELSE,
	TK_PREP_ENDIF,
	TK_PREP_IFDEF,
	TK_PREP_IFNDEF,
	TK_PREP_PRAGMA,

	TK_PREP_ASSIGN,
	TK_PREP_REPEAT,
	TK_PREP_UNTIL,
	TK_PREP_EMBED,

	TK_PREP_CONCAT,

	TK_NAMESPACE,
	TK_USING,
	TK_THIS,
	TK_COLCOLON,
	TK_CLASS,
	TK_PUBLIC,
	TK_PROTECTED,
	TK_PRIVATE,
	TK_NEW,
	TK_DELETE,
	TK_VIRTUAL,

	NUM_TOKENS
};

extern const char* TokenNames[];

class MacroDict;

class Macro
{
public:
	Macro(const Ident* ident, MacroDict* scope);
	~Macro(void);

	void SetString(const char* str);
	void SetString(const char* str, int length);
	void AddArgument(const Ident * ident);

	const Ident* mIdent;
	const char* mString;
	int		mNumArguments;
	const Ident	* mArguments[32];
	MacroDict* mScope;
};

typedef Macro* MacroPtr;

class MacroDict
{
public:
	MacroDict(void);
	~MacroDict(void);

	void Insert(Macro * macro);
	void Remove(const Ident* ident);
	Macro* Lookup(const Ident* ident);

protected:
	MacroPtr	*	mHash;
	int				mHashSize, mHashFill;
	MacroDict	*	mParent;
};

class Scanner
{
public:
	Scanner(Errors * errors, Preprocessor * preprocessor);
	~Scanner(void);

	const char* TokenName(Token token) const;

	void NextToken(void);

	void Warning(const char * error);
	void Error(const char * error);

	Errors* mErrors;
	Preprocessor	*	mPreprocessor;

	int				mPrepCondFalse, mPrepCondDepth;
	bool			mPrepCondExit;

	int				mOffset;
	const char	*	mLine;
	bool			mStartOfLine;

	const Ident	*	mTokenIdent;
	char			mTokenString[1024], mTokenChar;

	uint8		*	mTokenEmbed;
	int				mTokenEmbedSize;

	Token			mToken;
	double			mTokenNumber;
	int64			mTokenInteger;

	Location		mLocation;

	void SetAssemblerMode(bool mode);

	bool			mAssemblerMode;
	bool			mPreprocessorMode;

	uint64			mCompilerOptions;

	void AddMacro(const Ident* ident, const char* value);
protected:
	void NextRawToken(void);

	struct MacroExpansion
	{
		MacroExpansion	*	mLink;
		const char		*	mLine;
		int					mOffset;
		char				mChar;
		MacroDict* mDefinedArguments;
	}	*	mMacroExpansion;

	MacroDict* mDefines, * mDefineArguments;

	void StringToken(char terminator, char mode);
	void CharToken(char mode);
	bool NextChar(void);
	void ParseNumberToken(void);

	int64 PrepParseSimple(void);
	int64 PrepParseMul(void);
	int64 PrepParseAdd(void);
	int64 PrepParseShift(void);
	int64 PrepParseRel(void);
	int64 PrepParseBinaryAnd(void);
	int64 PrepParseBinaryXor(void);
	int64 PrepParseBinaryOr(void);
	int64 PrepParseLogicalAnd(void);
	int64 PrepParseLogicalOr(void);
	int64 PrepParseConditional(void);

};

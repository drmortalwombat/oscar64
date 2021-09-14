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
	TK_BOOL,
	TK_CONST,
	TK_VOLATILE,
	TK_TYPEDEF,
	TK_STRUCT,
	TK_UNION,
	TK_ENUM,
	TK_SIZEOF,
	TK_STATIC,
	TK_EXTERN,

	TK_ASM,

	TK_NUMBER,
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

	TK_PREP_DEFINE,
	TK_PREP_INCLUDE,
	TK_PREP_IF,
	TK_PREP_ELIF,
	TK_PREP_ELSE,
	TK_PREP_ENDIF,
	TK_PREP_IFDEF,
	TK_PREP_IFNDEF,
	TK_PREP_PRAGMA,

	NUM_TOKENS
};

extern const char* TokenNames[];

class Macro
{
public:
	Macro(const Ident* ident);
	~Macro(void);

	void SetString(const char* str);
	void SetString(const char* str, int length);
	void AddArgument(const Ident * ident);

	const Ident* mIdent;
	const char* mString;
	int		mNumArguments;
	const Ident	* mArguments[32];
};

typedef Macro* MacroPtr;

class MacroDict
{
public:
	MacroDict(void);
	~MacroDict(void);

	void Insert(Macro * macro);
	Macro* Lookup(const Ident* ident);

protected:
	MacroPtr	*	mHash;
	int				mHashSize, mHashFill;
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

	int				mPrepCondition, mPrepPending;

	int				mOffset;
	const char	*	mLine;

	const Ident	*	mTokenIdent;
	char			mTokenString[1024], mTokenChar;

	Token			mToken;
	double			mTokenNumber;
	unsigned int	mTokenInteger;

	Location		mLocation;

	void SetAssemblerMode(bool mode);

	bool			mAssemblerMode;
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

	void StringToken(char terminator);
	void CharToken(void);
	bool NextChar(void);
	void ParseNumberToken(void);

};

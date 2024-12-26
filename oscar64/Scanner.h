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
	TK_GOTO,
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
	TK_FORCEINLINE,
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
	TK_PREP_ERROR,
	TK_PREP_WARN,
	TK_PREP_PRAGMA,
	TK_PREP_LINE,

	TK_PREP_ASSIGN,
	TK_PREP_REPEAT,
	TK_PREP_UNTIL,
	TK_PREP_EMBED,
	TK_PREP_FOR,

	TK_PREP_CONCAT,
	TK_PREP_IDENT,

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
	TK_OPERATOR,
	TK_TEMPLATE,
	TK_FRIEND,
	TK_CONSTEXPR,
	TK_TYPENAME,
	TK_DECLTYPE,

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
	void SetString(const char* str, ptrdiff_t length);
	void AddArgument(const Ident * ident);

	const Ident	*	mIdent;
	const char	*	mString;
	int				mNumArguments;
	const Ident	*	mArguments[32];
	MacroDict	*	mScope;
	bool			mVariadic;
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

class Scanner;

struct TokenSequence
{
	TokenSequence	*	mNext;
	Location			mLocation;
	Token				mToken;

	const Ident		*	mTokenIdent;
	const uint8 *		mTokenString;
	double				mTokenNumber;
	int64				mTokenInteger;

	TokenSequence(Scanner* scanner);
	~TokenSequence(void);
};

class Scanner
{
public:
	Scanner(Errors * errors, Preprocessor * preprocessor);
	~Scanner(void);

	const char* TokenName(Token token) const;

	void NextToken(void);
	void UngetToken(Token token);

	void BeginRecord(void);
	TokenSequence* CompleteRecord(void);

	const TokenSequence* Replay(const TokenSequence * sequence);

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
	uint8			mTokenString[1024], mTokenChar;
	int				mTokenStringSize;

	uint8		*	mTokenEmbed;
	int				mTokenEmbedSize;

	Token			mUndoToken;
	Token			mToken;
	double			mTokenNumber;
	int64			mTokenInteger;

	Location		mLocation;

	void SetAssemblerMode(bool mode);

	bool			mAssemblerMode;
	bool			mPreprocessorMode;

	uint64			mCompilerOptions;

	bool IsIntegerToken(void) const;

	void AddMacro(const Ident* ident, const char* value);
	void MarkSourceOnce(void);
protected:
	void NextSkipRawToken(void);
	void NextRawToken(void);
	void NextPreToken(void);

	struct MacroExpansion
	{
		MacroExpansion	*	mLink;
		const char		*	mLine;
		int					mOffset;
		char				mChar;
		Macro			*	mLoopIndex;
		int64				mLoopCount, mLoopLimit;
		MacroDict* mDefinedArguments;

		MacroExpansion(void)
			: mLink(nullptr), mLine(nullptr), mOffset(0), mChar(0), mLoopIndex(nullptr), mLoopCount(0), mLoopLimit(0), mDefinedArguments(nullptr)
		{}

	}	*	mMacroExpansion;

	int			mMacroExpansionDepth;

	MacroDict* mDefines, * mDefineArguments, * mOnceDict;

	Token		mUngetToken;

	const TokenSequence* mReplay;
	TokenSequence* mRecord, * mRecordLast, * mRecordPrev;

	void StringToken(char terminator, char mode);
	void CharToken(char mode);
	bool NextChar(void);
	void ParseNumberToken(void);

	int64 PrepParseSimple(bool skip);
	int64 PrepParseMul(bool skip);
	int64 PrepParseAdd(bool skip);
	int64 PrepParseShift(bool skip);
	int64 PrepParseRel(bool skip);
	int64 PrepParseBinaryAnd(bool skip);
	int64 PrepParseBinaryXor(bool skip);
	int64 PrepParseBinaryOr(bool skip);
	int64 PrepParseLogicalAnd(bool skip);
	int64 PrepParseLogicalOr(bool skip);
	int64 PrepParseConditional(bool skip = false);

};

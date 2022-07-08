#include "Scanner.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "MachineTypes.h"


const char* TokenNames[] = 
{
	"tk_none",
	"tk_eof",
	"tk_error",
	"tk_eols",

	"'if'",
	"'else'",
	"'while'",
	"'do'",
	"'for'",
	"'void'",
	"'int'",
	"'char'",
	"'float'",
	"'unsigned'",
	"'signed'",
	"'switch'",
	"'case'",
	"'default'",
	"'break'",
	"'return'",
	"'short'",
	"'long'",
	"'continue'",
	"'integer'",
	"'integeru'",
	"'integerl'",
	"'integerul'",
	"'bool'",
	"'const'",
	"'volatile'",
	"'typedef'",
	"'struct'",
	"'union'",
	"'enum'",
	"'sizeof'",
	"'static'",
	"'extern'",
	"'inline'",
	"'__assume'",

	"__asm",
	"__interrupt",
	"__hwinterrupt",
	"__native",
	"__fastcall",
	"__export",
	"__zeropage",
	"__noinline",

	"number",
	"char",
	"string literal",
	"identifier",
	"'true'",
	"'false'",
	"'nullptr'",

	"'+'",
	"'-'",
	"'*'",
	"'/'",
	"'%'",

	"'<<'",
	"'>>'",

	"'++'",
	"'--'",

	"'!'",
	"'&&'",
	"'||'",

	"'~'",
	"'&'",
	"'|'",
	"'^'",

	"'=='",
	"'!='",
	"'>'",
	"'>='",
	"'<'",
	"'<='",

	"'='",
	"'+='",
	"'-='",
	"'*='",
	"'/='",
	"'%='",
	"'<<='",
	"'>>='",
	"'&='",
	"'^='",
	"'|='",

	"'->'",
	"'#'",
	"'$'",

	"'('",
	"')'",

	"'{'",
	"'}'",

	"'['",
	"']'",

	"'.'",
	"'..'",
	"'...'",
	"','",
	"';'",
	"':'",
	"'?'",

	"embedded",

	"'#define'",
	"'#undef'",
	"'#include'",
	"'#if'",
	"'#elif'",
	"'#else'",
	"'#endif'",
	"'#ifdef'",
	"'#ifndef'",
	"'#pragma'",

	"'#assign'",
	"'#repeat'",
	"'#until'",
	"'#embed'"
};


Macro::Macro(const Ident* ident)
	: mIdent(ident), mString(nullptr), mNumArguments(-1)
{

}

Macro::~Macro(void)
{

}

void Macro::SetString(const char* str)
{
	int	s = strlen(str);
	SetString(str, s);
}

void Macro::SetString(const char* str, int length)
{
	char* nstr = new char[length + 2];

	while (*str != 0 && *str <= ' ')
	{
		length--;
		str++;
	}
	while (length > 0 && str[length - 1] <= ' ')
		length--;

	if (length > 0)
		memcpy(nstr, str, length);

	nstr[length] = ' ';
	nstr[length + 1] = 0;
	mString = nstr;
}


void Macro::AddArgument(const Ident * ident)
{
	mArguments[mNumArguments++] = ident;
}

MacroDict::MacroDict(void)
{
	mHashSize = 0;
	mHashFill = 0;
	mHash = nullptr;
}

MacroDict::~MacroDict(void)
{
	delete[] mHash;
}

void MacroDict::Insert(Macro * macro)
{
	if (!mHash)
	{
		mHashSize = 16;
		mHashFill = 0;
		mHash = new MacroPtr[mHashSize];
		for (int i = 0; i < mHashSize; i++)
			mHash[i] = nullptr;
	}

	int		hm = mHashSize - 1;
	int		hi = macro->mIdent->mHash & hm;

	while (mHash[hi])
	{
		if (macro->mIdent == mHash[hi]->mIdent)
		{
			mHash[hi] = macro;
			return;
		}

		hi = (hi + 1) & hm;
	}

	mHash[hi] = macro;
	mHashFill++;

	if (2 * mHashFill >= mHashSize)
	{
		int		size = mHashSize;
		Macro	** entries = mHash;
		mHashSize *= 2;
		mHashFill = 0;
		mHash = new MacroPtr[mHashSize];
		for (int i = 0; i < mHashSize; i++)
			mHash[i] = nullptr;

		for (int i = 0; i < size; i++)
		{
			if (entries[i] && entries[i]->mIdent)
				Insert(entries[i]);
		}
		delete[] entries;
	}
}

void MacroDict::Remove(const Ident* ident)
{
	if (mHashSize > 0)
	{
		int		hm = mHashSize - 1;
		int		hi = ident->mHash & hm;

		while (mHash[hi])
		{
			if (ident == mHash[hi]->mIdent)
			{
				mHash[hi]->mIdent = nullptr;
				return;
			}

			hi = (hi + 1) & hm;
		}
	}
}

Macro* MacroDict::Lookup(const Ident* ident)
{
	if (mHashSize > 0)
	{
		int		hm = mHashSize - 1;
		int		hi = ident->mHash & hm;

		while (mHash[hi])
		{
			if (ident == mHash[hi]->mIdent)
				return mHash[hi];
			hi = (hi + 1) & hm;
		}
	}

	return nullptr;
}



Scanner::Scanner(Errors* errors, Preprocessor* preprocessor)
	: mErrors(errors), mPreprocessor(preprocessor)
{
	mOffset = 0;
	mLine = mPreprocessor->mLine;
	mPrepCondFalse = 0;
	mPrepCondDepth = 0;
	mPrepCondExit = 0;
	mAssemblerMode = false;
	mPreprocessorMode = false;
	mMacroExpansion = nullptr;

	mDefines = new MacroDict();
	mDefineArguments = nullptr;

	NextChar();
	NextToken();

	assert(sizeof(TokenNames) == NUM_TOKENS * sizeof(char*));
}

Scanner::~Scanner(void)
{
	delete mDefines;
}


const char* Scanner::TokenName(Token token) const
{
	return TokenNames[token];
}


void Scanner::SetAssemblerMode(bool mode)
{
	mAssemblerMode = mode;
}

static bool IsIdentChar(char c)
{
	if (c >= 'a' && c <= 'z')
		return true;
	else if (c >= 'A' && c <= 'Z')
		return true;
	else if (c >= '0' && c <= '9')
		return true;
	else if (c == '_')
		return true;
	else
		return false;
}

static inline bool IsWhitespace(char ch)
{
	return ch <= 32;
}

static inline bool IsLineBreak(char ch)
{
	return ch == '\n' || ch == '\r';
}

static inline bool IsAlpha(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static inline bool IsAlphaNumeric(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || (ch >= '0' && ch <= '9');
}


static inline bool IsNumeric(char ch)
{
	return ch >= '0' && ch <= '9';
}


static inline bool IsHex(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static inline int HexValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else
		return 0;
}

void Scanner::AddMacro(const Ident* ident, const char* value)
{
	Macro* macro = new Macro(ident);
	macro->SetString(value);
	mDefines->Insert(macro);
}

void Scanner::NextToken(void)
{
	for (;;)
	{
		NextRawToken();
		if (mToken == TK_PREP_ENDIF)
		{
			if (mPrepCondFalse > 0)
				mPrepCondFalse--;
			else if (mPrepCondExit)
				mPrepCondExit = false;
			else if (mPrepCondDepth > 0)
				mPrepCondDepth--;
			else
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Unexpected #endif");
		}
		else if (mToken == TK_PREP_ELSE)
		{
			if (mPrepCondExit)
			{

			}
			else if (mPrepCondFalse == 1)
			{
				mPrepCondFalse = 0;
				mPrepCondDepth++;
			}
			else if (mPrepCondFalse > 1)
			{
			}
			else if (mPrepCondDepth > 0)
			{
				mPrepCondExit = true;
			}
			else
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Unexpected #else");
		}
		else if (mToken == TK_PREP_ELIF)
		{
			if (mPrepCondExit)
			{

			}
			else if (mPrepCondFalse == 1)
			{
				mPreprocessorMode = true;
				mPrepCondFalse = 0;

				NextToken();
				int v = PrepParseConditional();
				if (v)
				{
					mPrepCondFalse = 0;
					mPrepCondDepth++;
				}
				else
					mPrepCondFalse++;

				mPreprocessorMode = false;
				if (mToken != TK_EOL)
					mErrors->Error(mLocation, ERRR_PREPROCESSOR, "End of line expected");
			}
			else if (mPrepCondFalse > 1)
			{
			}
			else if (mPrepCondDepth > 0)
			{
				mPrepCondExit = true;
			}
			else
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Unexpected #else");
		}
		else if (mToken == TK_EOF)
		{
			if (!mPreprocessor->CloseSource())
				return;
			mToken = TK_NONE;
			mOffset = 0;
		}
		else if (mPrepCondFalse > 0 || mPrepCondExit)
		{
			if (mToken == TK_PREP_IFDEF || mToken == TK_PREP_IFNDEF || mToken == TK_PREP_IF)
				mPrepCondFalse++;
		}
		else if (mToken == TK_PREP_INCLUDE)
		{
			NextRawToken();
			if (mToken == TK_STRING)
			{
				if (!mPreprocessor->OpenSource("Including", mTokenString, true))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", mTokenString);
			}
			else if (mToken == TK_LESS_THAN)
			{
				mOffset--;
				StringToken('>', 'a');
				if (!mPreprocessor->OpenSource("Including", mTokenString, false))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", mTokenString);
			}
		}
		else if (mToken == TK_PREP_DEFINE)
		{
			NextRawToken();
			if (mToken == TK_IDENT || mToken >= TK_IF && mToken <= TK_ASM || mToken == TK_TRUE || mToken == TK_FALSE)
			{
				if (mToken != TK_IDENT)
					mTokenIdent = Ident::Unique(TokenNames[mToken]);
				Macro* macro = new Macro(mTokenIdent);

				if (mTokenChar == '(')
				{
					macro->mNumArguments = 0;

					NextRawToken();

					if (mTokenChar == ')')
					{
						NextRawToken();
					}
					else
					{
						// Macro with argument
						do
						{
							NextRawToken();
							if (mToken == TK_IDENT)
							{
								macro->AddArgument(mTokenIdent);
								NextRawToken();
							}
							else
								mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid define argument");

						} while (mToken == TK_COMMA);
					}

					if (mToken == TK_CLOSE_PARENTHESIS)
					{
						// No need to goto next token, mOffset is already behind it
					}
					else
						mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "')' expected in defined parameter list");
				}

				int		slen = mOffset;
				bool	quote = false;
				while (mLine[slen] && (quote || mLine[slen] != '/' || mLine[slen + 1] != '/'))
				{
					if (mLine[slen] == '"')
						quote = !quote;
					slen++;
				}

				macro->SetString(mLine + mOffset, slen - mOffset);
				mDefines->Insert(macro);
				mOffset = slen;
				while (mLine[mOffset])
					mOffset++;
			}
		}
		else if (mToken == TK_PREP_UNDEF)
		{
			NextRawToken();
			if (mToken == TK_IDENT)
			{
				mDefines->Remove(mTokenIdent);
			}

		}
		else if (mToken == TK_PREP_IFDEF)
		{
			NextRawToken();
			if (mToken == TK_IDENT)
			{
				Macro	*	 def = mDefines->Lookup(mTokenIdent);
				if (def)
					mPrepCondDepth++;
				else
					mPrepCondFalse++;
			}
		}
		else if (mToken == TK_PREP_IFNDEF)
		{
			NextRawToken();
			if (mToken == TK_IDENT)
			{
				Macro	* def = mDefines->Lookup(mTokenIdent);
				if (!def)
					mPrepCondDepth++;
				else
					mPrepCondFalse++;
			}
		}
		else if (mToken == TK_PREP_IF)
		{
			mPreprocessorMode = true;
			NextToken();
			int v = PrepParseConditional();
			if (v)
				mPrepCondDepth++;
			else
				mPrepCondFalse++;
			mPreprocessorMode = false;
			if (mToken != TK_EOL)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "End of line expected");
		}
		else if (mToken == TK_PREP_ASSIGN)
		{
			mPreprocessorMode = true;
			NextRawToken();
			if (mToken == TK_IDENT)
			{
				const Ident* ident = mTokenIdent;

				NextToken();

				int v = PrepParseConditional();
				Macro* macro = mDefines->Lookup(ident);
				if (!macro)
				{
					macro = new Macro(ident);
					mDefines->Insert(macro);
				}
				char	buffer[20];
				sprintf_s(buffer, "%d", v);
				macro->SetString(buffer);
				mPreprocessorMode = false;
				if (mToken != TK_EOL)
					mErrors->Error(mLocation, ERRR_PREPROCESSOR, "End of line expected");
			}
		}
		else if (mToken == TK_PREP_REPEAT)
		{
			mPreprocessor->PushSource();
		}
		else if (mToken == TK_PREP_UNTIL)
		{
			mPreprocessorMode = true;
			NextToken();
			int v = PrepParseConditional();
			if (mToken != TK_EOL)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "End of line expected");

			if (v)
				mPreprocessor->DropSource();
			else
			{
				mPreprocessor->PopSource();
				mPreprocessor->PushSource();
				mPreprocessor->NextLine();
				mOffset = 0;
				NextChar();
			}
			mPreprocessorMode = false;
		}
		else if (mToken == TK_PREP_EMBED)
		{
			int	limit = 65536, skip = 0;
			SourceFileMode	mode = SFM_BINARY;

			NextRawToken();
			if (mToken == TK_INTEGER)
			{
				limit = mTokenInteger;
				NextRawToken();

				if (mToken == TK_INTEGER)
				{
					skip = mTokenInteger;
					NextRawToken();
				}
			}

			if (mToken == TK_IDENT)
			{
				if (!strcmp(mTokenIdent->mString, "rle"))
					mode = SFM_BINARY_RLE;
				else if (!strcmp(mTokenIdent->mString, "lzo"))
					mode = SFM_BINARY_LZO;
				else
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Invalid embed compression mode", mTokenIdent);

				NextRawToken();
			}

			if (mToken == TK_STRING)
			{
				if (!mPreprocessor->EmbedData("Embedding", mTokenString, true, skip, limit, mode))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", mTokenString);
			}
			else if (mToken == TK_LESS_THAN)
			{
				mOffset--;
				StringToken('>', 'a');
				if (!mPreprocessor->EmbedData("Embedding", mTokenString, false, skip, limit, mode))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", mTokenString);
			}
		}
		else if (mToken == TK_IDENT)
		{
			Macro* def = nullptr;
			if (mDefineArguments)
				def = mDefineArguments->Lookup(mTokenIdent);
			if (!def)
				def = mDefines->Lookup(mTokenIdent);

			if (def)
			{
				MacroExpansion* ex = new MacroExpansion();
				ex->mDefinedArguments = mDefineArguments;

				if (def->mNumArguments == 0)
				{
					NextRawToken();
					if (mToken != TK_OPEN_PARENTHESIS)
						mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Missing '(' for macro expansion");
					else
						NextRawToken();
					if (mToken != TK_CLOSE_PARENTHESIS)
						mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Missing ')' for macro expansion");
					else
						NextRawToken();

				}
				else if (def->mNumArguments > 0)
				{
					mDefineArguments = new MacroDict();
					NextRawToken();
					if (mToken == TK_OPEN_PARENTHESIS)
					{
						mOffset--;
						for (int i = 0; i < def->mNumArguments; i++)
						{
							int		offset = mOffset;
							int		level = 0;
							bool	quote = false;
							while (mLine[offset] && (quote || level > 0 || (mLine[offset] != ',' && mLine[offset] != ')')))
							{
								if (mLine[offset] == '"')
								{
									quote = !quote;
								}
								else if (mLine[offset] == '(')
								{
									level++;
								}
								else if (mLine[offset] == ')')
								{
									level--;
								}
								offset++;
							}
							Macro* arg = new Macro(def->mArguments[i]);
							arg->SetString(mLine + mOffset, offset - mOffset);
							mDefineArguments->Insert(arg);
							mOffset = offset;
							if (i + 1 != def->mNumArguments)
							{
								if (mLine[mOffset] == ',')
									mOffset++;
								else
									mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid define expansion argument");
							}
							else
							{
								if (mLine[mOffset] == ')')
									mOffset++;
								else
									mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid define expansion closing argument");
							}
						}
					}
					else
						mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Missing arguments for macro expansion");
					NextChar();
				}
				else
					mDefineArguments = nullptr;

				ex->mLine = mLine;
				ex->mOffset = mOffset;
				ex->mLink = mMacroExpansion;
				ex->mChar = mTokenChar;

				mMacroExpansion = ex;
				mLine = def->mString;
				mOffset = 0;
				NextChar();
			}
			else
			{
				while (mTokenChar == '#' && mLine[mOffset] == '#')
				{
					mOffset++;
					NextChar();

					char	tkbase[256];
					strcpy_s(tkbase, mTokenIdent->mString);

					int		n = 0;
					char	tkident[256];
					while (IsIdentChar(mTokenChar))
					{
						if (n < 255)
							tkident[n++] = mTokenChar;
						NextChar();
					}
					tkident[n] = 0;

					const Ident* ntkident = Ident::Unique(tkident);

					Macro* def = nullptr;
					if (mDefineArguments)
						def = mDefineArguments->Lookup(ntkident);
					if (!def)
						def = mDefines->Lookup(ntkident);

					if (def)
						strcat_s(tkbase, def->mString);
					else
						strcat_s(tkbase, tkident);

					n = strlen(tkbase);
					while (n > 0 && tkbase[n - 1] == ' ')
						n--;
					tkbase[n] = 0;

					mTokenIdent = Ident::Unique(tkbase);
				}

				return;
			}
		}
		else
			return;		
	}
}

void Scanner::NextRawToken(void)
{
	if (mToken != TK_EOF)
	{
		mToken = TK_ERROR;

		if (mOffset > 1 || !IsWhitespace(mTokenChar))
			mStartOfLine = false;

		while (IsWhitespace(mTokenChar))
		{
			if ((mAssemblerMode || mPreprocessorMode) && mTokenChar == '\n')
			{
				mToken = TK_EOL;
				NextChar();
				return;
			}
			if (!NextChar())
			{
				mToken = TK_EOF;
				return;
			}
		}

		mLocation = mPreprocessor->mLocation;
		mLocation.mColumn = mOffset;

		switch (mTokenChar)
		{
		case '+':
			mToken = TK_ADD;
			NextChar();
			if (mTokenChar == '+')
			{
				NextChar();
				mToken = TK_INC;
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_ADD;
			}
			break;
		case '-':
			mToken = TK_SUB;
			NextChar();
			if (mTokenChar == '-')
			{
				NextChar();
				mToken = TK_DEC;
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_SUB;
			}
			else if (mTokenChar == '>')
			{
				NextChar();
				mToken = TK_ARROW;
			}
			break;
		case '*':
			mToken = TK_MUL;
			NextChar();
			if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_MUL;
			}
			break;
		case '/':
			mToken = TK_DIV;
			NextChar();
			if (mTokenChar == '*')
			{
				bool	first = true;
				while (first || mTokenChar != '/')
				{
					if (mTokenChar == '*')
						first = false;
					else
						first = true;
					if (!NextChar())
					{
						mToken = TK_ERROR;
						Error("Multiline comment not closed");
						return;
					}
				}
				NextChar();
				NextToken();
			}
			else if (mTokenChar == '/')
			{
				NextChar();
				while (!IsLineBreak(mTokenChar) && NextChar())
					;
				NextToken();
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_DIV;
			}
			break;
		case '%':
			if (mAssemblerMode)
			{
				int	n = 0;
				int64	mant = 0;
				while (NextChar())
				{
					if (mTokenChar >= '0' && mTokenChar <= '9')
						mant = mant * 16 + (int)mTokenChar - (int)'0';
					else if (mTokenChar >= 'a' && mTokenChar <= 'f')
						mant = mant * 16 + 10 + (int)mTokenChar - (int)'a';
					else if (mTokenChar >= 'A' && mTokenChar <= 'F')
						mant = mant * 16 + 10 + (int)mTokenChar - (int)'A';
					else
						break;
					n++;
				}

				if (n == 0)
					mErrors->Error(mLocation, EERR_SYNTAX, "Missing digits in hex constant");

				mToken = TK_INTEGER;
				mTokenInteger = mant;
			}
			mToken = TK_MOD;
			NextChar();
			if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_MOD;
			}
			break;
		case '~':
			mToken = TK_BINARY_NOT;
			NextChar();
			break;
		case '^':
			mToken = TK_BINARY_XOR;
			NextChar();
			if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_XOR;
			}
			break;

		case '&':
			mToken = TK_BINARY_AND;
			NextChar();
			if (mTokenChar == '&')
			{
				NextChar();
				mToken = TK_LOGICAL_AND;
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_AND;
			}
			break;
		case '|':
			mToken = TK_BINARY_OR;
			NextChar();
			if (mTokenChar == '|')
			{
				NextChar();
				mToken = TK_LOGICAL_OR;
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_OR;
			}
			break;

		case '(':
			mToken = TK_OPEN_PARENTHESIS;
			NextChar();
			break;
		case ')':
			mToken = TK_CLOSE_PARENTHESIS;
			NextChar();
			break;
		case '{':
			mToken = TK_OPEN_BRACE;
			NextChar();
			break;
		case '}':
			mToken = TK_CLOSE_BRACE;
			NextChar();
			break;
		case '[':
			mToken = TK_OPEN_BRACKET;
			NextChar();
			break;
		case ']':
			mToken = TK_CLOSE_BRACKET;
			NextChar();
			break;

		case '.':
			mToken = TK_DOT;
			NextChar();
			if (mTokenChar == '.')
			{
				NextChar();
				mToken = TK_DOTDOT;
				if (mTokenChar == '.')
				{
					NextChar();
					mToken = TK_ELLIPSIS;
				}
			}
			break;
		case ',':
			mToken = TK_COMMA;
			NextChar();
			break;
		case ';':
			mToken = TK_SEMICOLON;
			NextChar();
			break;
		case ':':
			mToken = TK_COLON;
			NextChar();
			break;
		case '?':
			mToken = TK_QUESTIONMARK;
			NextChar();
			break;

		case '=':
			mToken = TK_ASSIGN;
			NextChar();
			if (mTokenChar == '=')
			{
				mToken = TK_EQUAL;
				NextChar();
			}

			break;
		case '!':
			mToken = TK_LOGICAL_NOT;
			NextChar();
			if (mTokenChar == '=')
			{
				mToken = TK_NOT_EQUAL;
				NextChar();
			}
			break;
		case '<':
			mToken = TK_LESS_THAN;
			NextChar();
			if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_LESS_EQUAL;
			}
			else if (mTokenChar == '<')
			{
				NextChar();
				mToken = TK_LEFT_SHIFT;
				if (mTokenChar == '=')
				{
					NextChar();
					mToken = TK_ASSIGN_SHL;
				}
			}
			break;
		case '>':
			mToken = TK_GREATER_THAN;
			NextChar();
			if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_GREATER_EQUAL;
			}
			else if (mTokenChar == '>')
			{
				mToken = TK_RIGHT_SHIFT;
				NextChar();
				if (mTokenChar == '=')
				{
					NextChar();
					mToken = TK_ASSIGN_SHR;
				}
			}
			break;

		case '\'':
			CharToken('a');
			break;
		case '"':
			StringToken(mTokenChar, 'a');
			break;

		case '#':
		{
			if (!(mAssemblerMode || mPrepCondFalse) || mOffset == 1 || mStartOfLine)
			{
				int		n = 0;
				char	tkprep[128];

				while (NextChar() && IsAlpha(mTokenChar))
				{
					if (n < 127)
						tkprep[n++] = mTokenChar;
				}
				tkprep[n] = 0;

				if (!strcmp(tkprep, "define"))
					mToken = TK_PREP_DEFINE;
				else if (!strcmp(tkprep, "undef"))
					mToken = TK_PREP_UNDEF;
				else if (!strcmp(tkprep, "include"))
					mToken = TK_PREP_INCLUDE;
				else if (!strcmp(tkprep, "if"))
					mToken = TK_PREP_IF;
				else if (!strcmp(tkprep, "ifdef"))
					mToken = TK_PREP_IFDEF;
				else if (!strcmp(tkprep, "ifndef"))
					mToken = TK_PREP_IFNDEF;
				else if (!strcmp(tkprep, "elif"))
					mToken = TK_PREP_ELIF;
				else if (!strcmp(tkprep, "else"))
					mToken = TK_PREP_ELSE;
				else if (!strcmp(tkprep, "endif"))
					mToken = TK_PREP_ENDIF;
				else if (!strcmp(tkprep, "pragma"))
					mToken = TK_PREP_PRAGMA;
				else if (!strcmp(tkprep, "assign"))
					mToken = TK_PREP_ASSIGN;
				else if (!strcmp(tkprep, "repeat"))
					mToken = TK_PREP_REPEAT;
				else if (!strcmp(tkprep, "until"))
					mToken = TK_PREP_UNTIL;
				else if (!strcmp(tkprep, "embed"))
					mToken = TK_PREP_EMBED;
				else
					mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid preprocessor command", tkprep);
			}
			else
			{
				NextChar();
				mToken = TK_HASH;
			}
			break;
		}

		case '$':

			if (mAssemblerMode)
			{
				int	n = 0;
				int64	mant = 0;
				while (NextChar())
				{
					if (mTokenChar >= '0' && mTokenChar <= '9')
						mant = mant * 16 + (int)mTokenChar - (int)'0';
					else if (mTokenChar >= 'a' && mTokenChar <= 'f')
						mant = mant * 16 + 10 + (int)mTokenChar - (int)'a';
					else if (mTokenChar >= 'A' && mTokenChar <= 'F')
						mant = mant * 16 + 10 + (int)mTokenChar - (int)'A';
					else
						break;
					n++;
				}

				if (n == 0)
					mErrors->Error(mLocation, EERR_SYNTAX, "Missing digits in hex constant");

				if (mTokenChar == 'L' || mTokenChar == 'l')
				{
					NextChar();
					mToken = TK_INTEGERUL;
				}
				else
					mToken = TK_INTEGERU;
				mTokenInteger = mant;
			}
			else
			{
				NextChar();
				mToken = TK_DOLLAR;
			}
			break;

		default:
			if (mTokenChar >= '0' && mTokenChar <= '9')
			{
				ParseNumberToken();
			}
			else if (mTokenChar >= 'A' && mTokenChar <= 'Z' || mTokenChar >= 'a' && mTokenChar <= 'z' || mTokenChar == '_')
			{
				int		n = 0;
				char	tkident[256];
				for (;;)
				{
					if (IsIdentChar(mTokenChar))
					{
						if (n < 255)
							tkident[n++] = mTokenChar;
						NextChar();
					}
					else
						break;
				}
				if (n == 1)
				{
					if (mTokenChar == '"')
					{
						StringToken(mTokenChar, tkident[0]);
						break;
					}
					else if (mTokenChar == '\'')
					{
						CharToken(tkident[0]);
						break;
					}
				}
				tkident[n] = 0;
				if (n == 256)
					Error("Identifier exceeds max character limit");

				if (!strcmp(tkident, "true"))
					mToken = TK_TRUE;
				else if (!strcmp(tkident, "false"))
					mToken = TK_FALSE;
				else if (!strcmp(tkident, "nullptr"))
					mToken = TK_NULL;
				else if (!strcmp(tkident, "int"))
					mToken = TK_INT;
				else if (!strcmp(tkident, "float"))
					mToken = TK_FLOAT;
				else if (!strcmp(tkident, "bool") || !strcmp(tkident, "_Bool"))
					mToken = TK_BOOL;
				else if (!strcmp(tkident, "char"))
					mToken = TK_CHAR;
				else if (!strcmp(tkident, "short"))
					mToken = TK_SHORT;
				else if (!strcmp(tkident, "long"))
					mToken = TK_LONG;
				else if (!strcmp(tkident, "unsigned"))
					mToken = TK_UNSIGNED;
				else if (!strcmp(tkident, "signed"))
					mToken = TK_SIGNED;
				else if (!strcmp(tkident, "const"))
					mToken = TK_CONST;
				else if (!strcmp(tkident, "volatile"))
					mToken = TK_VOLATILE;
				else if (!strcmp(tkident, "if"))
					mToken = TK_IF;
				else if (!strcmp(tkident, "else"))
					mToken = TK_ELSE;
				else if (!strcmp(tkident, "while"))
					mToken = TK_WHILE;
				else if (!strcmp(tkident, "do"))
					mToken = TK_DO;
				else if (!strcmp(tkident, "for"))
					mToken = TK_FOR;
				else if (!strcmp(tkident, "switch"))
					mToken = TK_SWITCH;
				else if (!strcmp(tkident, "case"))
					mToken = TK_CASE;
				else if (!strcmp(tkident, "default"))
					mToken = TK_DEFAULT;
				else if (!strcmp(tkident, "break"))
					mToken = TK_BREAK;
				else if (!strcmp(tkident, "continue"))
					mToken = TK_CONTINUE;
				else if (!strcmp(tkident, "return"))
					mToken = TK_RETURN;
				else if (!strcmp(tkident, "void"))
					mToken = TK_VOID;
				else if (!strcmp(tkident, "struct"))
					mToken = TK_STRUCT;
				else if (!strcmp(tkident, "union"))
					mToken = TK_UNION;
				else if (!strcmp(tkident, "enum"))
					mToken = TK_ENUM;
				else if (!strcmp(tkident, "sizeof"))
					mToken = TK_SIZEOF;
				else if (!strcmp(tkident, "typedef"))
					mToken = TK_TYPEDEF;
				else if (!strcmp(tkident, "static"))
					mToken = TK_STATIC;
				else if (!strcmp(tkident, "extern"))
					mToken = TK_EXTERN;
				else if (!strcmp(tkident, "inline"))
					mToken = TK_INLINE;
				else if (!strcmp(tkident, "__asm"))
					mToken = TK_ASM;
				else if (!strcmp(tkident, "__assume"))
					mToken = TK_ASSUME;
				else if (!strcmp(tkident, "__interrupt"))
					mToken = TK_INTERRUPT;
				else if (!strcmp(tkident, "__hwinterrupt"))
					mToken = TK_HWINTERRUPT;
				else if (!strcmp(tkident, "__native"))
					mToken = TK_NATIVE;
				else if (!strcmp(tkident, "__fastcall"))
					mToken = TK_FASTCALL;
				else if (!strcmp(tkident, "__export"))
					mToken = TK_EXPORT;
				else if (!strcmp(tkident, "__zeropage"))
					mToken = TK_ZEROPAGE;
				else if (!strcmp(tkident, "__noinline"))
					mToken = TK_NOINLINE;
				else
				{
					mToken = TK_IDENT;
					mTokenIdent = Ident::Unique(tkident);
				}
			}
			else
			{
				NextChar();
			}
			break;
		}
	}
	else
	{
		mToken = TK_EOF;
	}

}

void Scanner::Warning(const char* error) 
{
	mErrors->Error(mLocation, EWARN_SYNTAX, error);
}

void Scanner::Error(const char* error)
{
	mErrors->Error(mLocation, EERR_SYNTAX, error);
}
static char p2smap[] = { 0x00, 0x20, 0x00, 0x40, 0x00, 0x60, 0x40, 0x60 };

static inline char p2s(char ch)
{
	return (ch & 0x1f) | p2smap[ch >> 5];
}

static inline char transchar(char mode, char ch)
{
	switch (mode)
	{
	default:
	case 'a':
		return ch;
	case 'p':
		if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
			return ch ^ 0x20;
		else
			return ch;
		break;
	case 'P':
		if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
			return (ch ^ 0x20) & 0xdf;
		else
			return ch;
		break;
	case 's':
		if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
			return p2s(ch ^ 0x20);
		else
			return p2s(ch);
		break;
	case 'S':
		if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
			return p2s((ch ^ 0x20) & 0xdf);
		else
			return p2s(ch);
		break;
	}
}

void Scanner::StringToken(char terminator, char mode)
{
	switch (mode)
	{
	case 'a':
	case 'p':
	case 'P':
	case 's':
	case 'S':
		break;
	default:
		Error("Invalid string literal mode");
	}

	int	n = 0;

	while (mLine[mOffset] && mLine[mOffset] != terminator && mLine[mOffset] != '\n')
	{
		char ch = mLine[mOffset++];

		if (ch == '\\' && mLine[mOffset])
		{
			ch = mLine[mOffset++];
			switch (ch)
			{
			case '0':
				mTokenChar = '\0';
				break;
			case 'n':
				mTokenChar = '\n';
				break;
			case 'r':
				mTokenChar = '\r';
				break;
			case 't':
				mTokenChar = '\t';
				break;
			case 'f':
				mTokenChar = '\f';
				break;
			case 'b':
				mTokenChar = '\b';
				break;
			case 'v':
				mTokenChar = '\v';
				break;
			case 'x':
			{
				char	c0 = mLine[mOffset++];
				char	c1 = mLine[mOffset++];

				if (IsHex(c0) && IsHex(c1))
					mTokenChar = transchar(mode, 16 * HexValue(c0) + HexValue(c1));
				else
					mErrors->Error(mLocation, EERR_SYNTAX, "Invalid hex escape code");
			}
				break;
			case 'y':
			{
				char	c0 = mLine[mOffset++];
				char	c1 = mLine[mOffset++];

				if (IsHex(c0) && IsHex(c1))
					mTokenChar = 16 * HexValue(c0) + HexValue(c1);
				else
					mErrors->Error(mLocation, EERR_SYNTAX, "Invalid hex escape code");
			}
				break;
			default:
				mTokenChar = transchar(mode, ch);
				break;
			}
		}
		else
			mTokenChar = transchar(mode, ch);

		mTokenString[n++] = mTokenChar;
	}

	mTokenString[n] = 0;

	if (mLine[mOffset] && mLine[mOffset] == terminator)
	{
		mToken = TK_STRING;
		mOffset++;
		NextChar();
	}
	else
	{
		NextChar();
		Error("String constant not terminated");
		mToken = TK_ERROR;
	}

}

void Scanner::CharToken(char mode)
{
	switch (mode)
	{
	case 'a':
	case 'p':
	case 'P':
	case 's':
	case 'S':
		break;
	default:
		Error("Invalid string literal mode");
	}

	int	n = 0;

	char ch = mLine[mOffset++];

	if (ch == '\\' && mLine[mOffset])
	{
		ch = mLine[mOffset++];
		switch (ch)
		{
		case '0':
			mTokenChar = '\0';
			break;
		case 'n':
			mTokenChar = '\n';
			break;
		case 'r':
			mTokenChar = '\r';
			break;
		case 't':
			mTokenChar = '\t';
			break;
		case 'f':
			mTokenChar = '\f';
			break;
		case 'b':
			mTokenChar = '\b';
			break;
		case 'v':
			mTokenChar = '\v';
			break;
		case 'x':
		{
			char	c0 = mLine[mOffset++];
			char	c1 = mLine[mOffset++];

			if (IsHex(c0) && IsHex(c1))
				mTokenChar = transchar(mode, 16 * HexValue(c0) + HexValue(c1));
			else
				mErrors->Error(mLocation, EERR_SYNTAX, "Invalid hex escape code");
		}
		break;
		case 'y':
		{
			char	c0 = mLine[mOffset++];
			char	c1 = mLine[mOffset++];

			if (IsHex(c0) && IsHex(c1))
				mTokenChar = 16 * HexValue(c0) + HexValue(c1);
			else
				mErrors->Error(mLocation, EERR_SYNTAX, "Invalid hex escape code");
		}
		break;
		default:
			mTokenChar = transchar(mode, ch);
		}
	}
	else
		mTokenChar = transchar(mode, ch);

	mTokenInteger = mTokenChar;

	if (mLine[mOffset] && mLine[mOffset] == '\'')
	{
		mToken = TK_CHARACTER;
		mOffset++;
		NextChar();
	}
	else
	{
		NextChar();
		Error("Character constant not terminated");
		mToken = TK_ERROR;
	}
}

bool Scanner::NextChar(void) 
{
	while (!(mLine[mOffset]))
	{
		if (mMacroExpansion)
		{
			MacroExpansion* mac = mMacroExpansion->mLink;
			delete mDefineArguments;

			mLine = mMacroExpansion->mLine;
			mOffset = mMacroExpansion->mOffset;
			mTokenChar = mMacroExpansion->mChar;
			mDefineArguments = mMacroExpansion->mDefinedArguments;

			delete mMacroExpansion;
			mMacroExpansion = mac;
			return true;
		}
		else if (mPreprocessor->NextLine())
		{
			mOffset = 0;
			mStartOfLine = true;
		}
		else
		{
			mTokenChar = 0;
			return false;
		}
	}

	mTokenChar = mLine[mOffset++];
	return true;
}

void Scanner::ParseNumberToken(void)
{
	uint64	mant = (int)mTokenChar - (int)'0';

	NextChar();

	if (mant == 0 && (mTokenChar == 'x' || mTokenChar == 'X'))
	{
		int	n = 0;
		while (NextChar())
		{
			if (mTokenChar >= '0' && mTokenChar <= '9')
				mant = mant * 16 + (int)mTokenChar - (int)'0';
			else if (mTokenChar >= 'a' && mTokenChar <= 'f')
				mant = mant * 16 + 10 + (int)mTokenChar - (int)'a';
			else if (mTokenChar >= 'A' && mTokenChar <= 'F')
				mant = mant * 16 + 10 + (int)mTokenChar - (int)'A';
			else
				break;
			n++;
		}

		if (n == 0)
			Error("Missing digits in hex constant");

		if (mTokenChar == 'U' || mTokenChar == 'u')
		{
			NextChar();
			if (mTokenChar == 'L' || mTokenChar == 'l')
			{
				NextChar();
				mToken = TK_INTEGERUL;
			}
			else
				mToken = TK_INTEGERU;
		}
		else
		{
			if (mTokenChar == 'L' || mTokenChar == 'l')
			{
				NextChar();
				mToken = TK_INTEGERL;
			}
			else
				mToken = TK_INTEGER;
		}

		mTokenInteger = mant;
	}
	else if (mant == 0 && (mTokenChar == 'b' || mTokenChar == 'B'))
	{
		int	n = 0;
		while (NextChar())
		{
			if (mTokenChar >= '0' && mTokenChar <= '1')
				mant = mant * 2 + (int)mTokenChar - (int)'0';
			else
				break;
			n++;
		}

		if (n == 0)
			Error("Missing digits in binary constant");

		if (mTokenChar == 'U' || mTokenChar == 'u')
		{
			NextChar();
			if (mTokenChar == 'L' || mTokenChar == 'l')
			{
				NextChar();
				mToken = TK_INTEGERUL;
			}
			else
				mToken = TK_INTEGERU;
		}
		else
		{
			if (mTokenChar == 'L' || mTokenChar == 'l')
			{
				NextChar();
				mToken = TK_INTEGERL;
			}
			else
				mToken = TK_INTEGER;
		}
		mTokenInteger = mant;
	}
	else
	{
		int	n = 0;
		if (mTokenChar >= '0' && mTokenChar <= '9')
		{
			mant = mant * 10 + (int)mTokenChar - (int)'0';
			while (NextChar())
			{
				if (mTokenChar >= '0' && mTokenChar <= '9')
					mant = mant * 10 + (int)mTokenChar - (int)'0';
				else
					break;
				n++;
			}
		}

		if (mTokenChar != '.')
		{
			if (mTokenChar == 'U' || mTokenChar == 'u')
			{
				NextChar();
				if (mTokenChar == 'L' || mTokenChar == 'l')
				{
					NextChar();
					mToken = TK_INTEGERUL;
				}
				else
					mToken = TK_INTEGERU;
			}
			else
			{
				if (mTokenChar == 'L' || mTokenChar == 'l')
				{
					NextChar();
					mToken = TK_INTEGERL;
				}
				else
					mToken = TK_INTEGER;
			}
			mTokenInteger = mant;
		}
		else
		{
			double	facc = mant, fract = 1.0;

			NextChar();

			while (mTokenChar >= '0' && mTokenChar <= '9')
			{
				facc *= 10;
				fract *= 10;
				facc += (int)mTokenChar - (int)'0';
				NextChar();
			}

			facc /= fract;

			if (mTokenChar == 'e' || mTokenChar == 'E')
			{
				NextChar();
				bool	sign = false;
				if (mTokenChar == '+')
					NextChar();
				else if (mTokenChar == '-')
				{
					NextChar();
					sign = true;
				}

				int	expval = 0;

				while (mTokenChar >= '0' && mTokenChar <= '9')
				{
					expval = expval * 10 + (int)mTokenChar - (int)'0';
					NextChar();
				}

				if (expval > 600)
					expval = 600;

				double	fexp = pow(10.0, expval);

				if (sign)
					facc /= fexp;
				else
					facc *= fexp;
			}

			mTokenNumber = facc;
			mToken = TK_NUMBER;
		}
	}
}


int64 Scanner::PrepParseSimple(void)
{
	int64	v = 0;
	
	switch (mToken)
	{
	case TK_CHARACTER:
	case TK_INTEGER:
	case TK_INTEGERU:
	case TK_INTEGERL:
	case TK_INTEGERUL:
		v = mTokenInteger;
		NextToken();
		break;
	case TK_SUB:
		NextToken();
		v = -PrepParseSimple();
		break;
	case TK_LOGICAL_NOT:
		NextToken();
		v = !PrepParseSimple();
		break;
	case TK_BINARY_NOT:
		NextToken();
		v = ~PrepParseSimple();
		break;
	case TK_OPEN_PARENTHESIS:
		NextToken();
		v = PrepParseConditional();
		if (mToken == TK_CLOSE_PARENTHESIS)
			NextToken();
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "')' expected");
		break;
	case TK_IDENT:
		if (strcmp(mTokenIdent->mString, "defined") == 0)
		{
			NextToken();
			if (mToken == TK_OPEN_PARENTHESIS)
			{
				NextRawToken();
				if (mToken == TK_IDENT)
				{
					Macro* def = nullptr;
					if (mDefineArguments)
						def = mDefineArguments->Lookup(mTokenIdent);
					if (!def)
						def = mDefines->Lookup(mTokenIdent);
					if (def)
						v = 1;
					else
						v = 0;
					NextToken();
				}
				else
					mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Identifier expected");

				if (mToken == TK_CLOSE_PARENTHESIS)
					NextToken();
				else
					mErrors->Error(mLocation, ERRR_PREPROCESSOR, "')' expected");
			}
			else
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "'(' expected");
		}
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Invalid preprocessor symbol", mTokenIdent);
		break;
	default:
		mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Invalid preprocessor token", TokenName(mToken));
		if (mToken != TK_EOL)
			NextToken();
	}
	
	return v;
}

int64 Scanner::PrepParseMul(void)
{
	int64	v = PrepParseSimple();
	int64	u;
	for (;;)
	{
		switch (mToken)
		{
		case TK_MUL:
			NextToken();
			v *= PrepParseSimple();
			break;
		case TK_DIV:
			NextToken();
			u = PrepParseSimple();
			if (u == 0)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Division by zero");
			else
				v /= u;
			break;
		case TK_MOD:
			u = PrepParseSimple();
			if (u == 0)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Division by zero");
			else
				v %= u;
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseAdd(void)
{
	int64	v = PrepParseMul();
	for (;;)
	{
		switch (mToken)
		{
		case TK_ADD:
			NextToken();
			v += PrepParseMul();
			break;
		case TK_SUB:
			NextToken();
			v -= PrepParseMul();
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseShift(void)
{
	int64	v = PrepParseAdd();
	for (;;)
	{
		switch (mToken)
		{
		case TK_LEFT_SHIFT:
			NextToken();
			v <<= PrepParseAdd();
			break;
		case TK_RIGHT_SHIFT:
			NextToken();
			v >>= PrepParseAdd();
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseRel(void)
{
	int64	v = PrepParseShift();
	for (;;)
	{
		switch (mToken)
		{
		case TK_LESS_THAN:
			NextToken();
			v = v < PrepParseShift();
			break;
		case TK_GREATER_THAN:
			NextToken();
			v = v > PrepParseShift();
			break;
		case TK_LESS_EQUAL:
			NextToken();
			v = v <= PrepParseShift();
			break;
		case TK_GREATER_EQUAL:
			NextToken();
			v = v >= PrepParseShift();
			break;
		case TK_EQUAL:
			NextToken();
			v = v == PrepParseShift();
			break;
		case TK_NOT_EQUAL:
			NextToken();
			v = v != PrepParseShift();
			break;
		default:
			return v;
		}
	}

}

int64 Scanner::PrepParseBinaryAnd(void)
{
	int64	v = PrepParseRel();
	while (mToken == TK_BINARY_AND)
	{
		NextToken();
		v &= PrepParseRel();
	}
	return v;
}

int64 Scanner::PrepParseBinaryXor(void)
{
	int64	v = PrepParseBinaryAnd();
	while (mToken == TK_BINARY_XOR)
	{
		NextToken();
		v ^= PrepParseBinaryAnd();
	}
	return v;
}

int64 Scanner::PrepParseBinaryOr(void)
{
	int64	v = PrepParseBinaryXor();
	while (mToken == TK_BINARY_OR)
	{
		NextToken();
		v |= PrepParseBinaryXor();
	}
	return v;
}

int64 Scanner::PrepParseLogicalAnd(void) 
{
	int64	v = PrepParseBinaryOr();
	while (mToken == TK_LOGICAL_AND)
	{
		NextToken();
		if (!PrepParseBinaryOr())
			v = 0;
	}
	return v;
}

int64 Scanner::PrepParseLogicalOr(void)
{
	int64	v = PrepParseLogicalAnd();
	while (mToken == TK_LOGICAL_OR)
	{
		NextToken();
		if (PrepParseLogicalAnd())
			v = 1;
	}
	return v;
}

int64 Scanner::PrepParseConditional(void)
{
	int64	v = PrepParseLogicalOr();
	if (mToken == TK_QUESTIONMARK)
	{
		NextToken();
		int64	vt = PrepParseConditional();
		if (mToken == TK_COLON)
			NextToken();
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "':' expected");
		int64	vf = PrepParseConditional();
		if (v)
			v = vt;
		else
			v = vf;
	}

	return v;
}

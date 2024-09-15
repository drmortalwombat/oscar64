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
	"'goto'",
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
	"'__bankof'",
	"'static'",
	"'auto'",
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
	"__striped",
	"__dynstack",

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
	"'#error'",
	"'#warning'",
	"'#pragma'",
	"'#line'",

	"'#assign'",
	"'#repeat'",
	"'#until'",
	"'#embed'",
	"'#for'",
	"'##'",
	"'#string'",

	"'namespace'",
	"'using'",
	"'this'",
	"'::'",
	"'class'",
	"'public'",
	"'protected'",
	"'private'",
	"'new'",
	"'delete'",
	"'virtual'",
	"'operator'",
	"'template'",
	"'friend'",
	"'constexpr'",
	"'typename'",
};


Macro::Macro(const Ident* ident, MacroDict * scope)
	: mIdent(ident), mString(nullptr), mNumArguments(-1), mScope(scope)
{

}

Macro::~Macro(void)
{

}

void Macro::SetString(const char* str)
{
	ptrdiff_t	s = strlen(str);
	SetString(str, s);
}

void Macro::SetString(const char* str, ptrdiff_t length)
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


TokenSequence::TokenSequence(Scanner* scanner)
	: mNext(nullptr), mLocation(scanner->mLocation), mToken(scanner->mToken),
	mTokenIdent(scanner->mTokenIdent), 
	mTokenInteger(scanner->mTokenInteger), mTokenNumber(scanner->mTokenNumber),
	mTokenString(nullptr)
{
	if (mToken == TK_STRING)
	{
		uint8 * str = new uint8[scanner->mTokenStringSize + 1];
		memcpy(str, scanner->mTokenString, scanner->mTokenStringSize + 1);
		mTokenInteger = scanner->mTokenStringSize;
		mTokenString = str;
	}
}

TokenSequence::~TokenSequence(void)
{
	delete[] mTokenString;
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
	mMacroExpansionDepth = 0;

	mDefines = new MacroDict();
	mDefineArguments = nullptr;
	mToken = TK_NONE;
	mUngetToken = TK_NONE;
	mReplay = nullptr;
	mRecord = mRecordLast = nullptr;

	mOnceDict = new MacroDict();

	NextChar();

	assert(sizeof(TokenNames) == NUM_TOKENS * sizeof(char*));
}

Scanner::~Scanner(void)
{
	delete mDefines;
}


void Scanner::BeginRecord(void)
{
	mRecord = mRecordLast = new TokenSequence(this);
}

TokenSequence* Scanner::CompleteRecord(void)
{
	TokenSequence* seq = mRecord;
	mRecord = mRecordLast = nullptr;
	return seq;
}

const TokenSequence* Scanner::Replay(const TokenSequence* replay)
{
	const TokenSequence* seq = mReplay;
	mReplay = replay;
	NextToken();
	return seq;
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
	Macro* macro = new Macro(ident, nullptr);
	macro->SetString(value);
	mDefines->Insert(macro);
}

void Scanner::MarkSourceOnce(void)
{
	const Ident* fident = Ident::Unique(mPreprocessor->mSource->mFileName);

	Macro* macro = new Macro(fident, nullptr);
	mOnceDict->Insert(macro);
}

bool Scanner::IsIntegerToken(void) const
{
	return mToken == TK_INTEGER || mToken == TK_INTEGERU || mToken == TK_INTEGERL || mToken == TK_INTEGERUL;
}

void Scanner::NextToken(void)
{
	if (mReplay)
	{
		mLocation = mReplay->mLocation;
		mToken = mReplay->mToken;

		mTokenIdent = mReplay->mTokenIdent;
		mTokenNumber = mReplay->mTokenNumber;
		mTokenInteger = mReplay->mTokenInteger;
		if (mReplay->mTokenString)
		{
			mTokenStringSize = (int)mReplay->mTokenInteger;
			memcpy(mTokenString, mReplay->mTokenString, mTokenStringSize + 1);
		}

		mReplay = mReplay->mNext;
	}
	else
		NextPreToken();

	if (mRecord)
	{
		mRecordLast->mNext = new TokenSequence(this);
		mRecordLast = mRecordLast->mNext;			
	}
}

void Scanner::NextPreToken(void)
{
	for (;;)
	{
		if (mPrepCondFalse > 0 || mPrepCondExit)
			NextSkipRawToken();
		else
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

				NextPreToken();
				int64 v = PrepParseConditional();
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
			// Make sure include file names are not petsciid
			uint64 op = mCompilerOptions;
			mCompilerOptions &= ~COPT_PETSCII;
			NextRawToken();
			mCompilerOptions = op;

			if (mToken == TK_STRING)
			{
				if (!mPreprocessor->OpenSource("Including", (const char *)mTokenString, true))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", (const char*)mTokenString);
				else if (mOnceDict->Lookup(Ident::Unique(mPreprocessor->mSource->mFileName)))
					mPreprocessor->CloseSource();
			}
			else if (mToken == TK_LESS_THAN)
			{
				mOffset--;
				StringToken('>', 'a');
				if (!mPreprocessor->OpenSource("Including", (const char*)mTokenString, false))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", (const char*)mTokenString);
				else if (mOnceDict->Lookup(Ident::Unique(mPreprocessor->mSource->mFileName)))
					mPreprocessor->CloseSource();
			}
		}
		else if (mToken == TK_PREP_LINE)
		{
			mPreprocessorMode = true;
			NextPreToken();
			int l = mLocation.mLine;
			int64 v = PrepParseConditional();
			if (mLocation.mLine == l && mToken == TK_STRING)
			{
				strcpy_s(mPreprocessor->mSource->mLocationFileName, (const char*)mTokenString);
				NextRawToken();
			}
			mPreprocessor->mLocation.mLine = int(v) + mLocation.mLine - l;
			mPreprocessorMode = false;
		}
		else if (mToken == TK_PREP_FOR)
		{
			NextRawToken();
			if (mToken == TK_OPEN_PARENTHESIS)
			{
				NextRawToken();
				Macro* macro = new Macro(Ident::Unique("@for"), nullptr);
				if (mToken == TK_IDENT)
				{
					const Ident* loopindex = mTokenIdent;
					NextRawToken();

					if (mToken == TK_COMMA)
					{
						mPreprocessorMode = true;
						NextPreToken();
						int64 loopCount = PrepParseConditional();
						mPreprocessorMode = false;

						if (mToken == TK_CLOSE_PARENTHESIS)
						{
							int		slen = mOffset;
							bool	quote = false;
							while (mLine[slen] && (quote || mLine[slen] != '/' || mLine[slen + 1] != '/'))
							{
								if (mLine[slen] == '"')
									quote = !quote;
								slen++;
							}

							macro->SetString(mLine + mOffset, slen - mOffset);

							mOffset = slen;
							while (mLine[mOffset])
								mOffset++;

							if (loopCount > 0)
							{
								MacroExpansion* ex = new MacroExpansion();
								MacroDict* scope = mDefineArguments;
								mDefineArguments = new MacroDict();

								Macro* arg = new Macro(loopindex, scope);
								mDefineArguments->Insert(arg);

								arg->SetString("0");

								ex->mLine = mLine;
								ex->mOffset = mOffset;
								ex->mLink = mMacroExpansion;
								ex->mChar = mTokenChar;
								ex->mLoopCount = 0;
								ex->mLoopIndex = arg;
								ex->mLoopLimit = loopCount;

								mMacroExpansion = ex;
								mMacroExpansionDepth++;
								if (mMacroExpansionDepth > 1024)
									mErrors->Error(mLocation, EFATAL_MACRO_EXPANSION_DEPTH, "Maximum macro expansion depth exceeded", mTokenIdent);
								mLine = macro->mString;
								mOffset = 0;
								NextChar();
							}
						}
						else
							mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "')' expected in defined parameter list");
					}
					else
						mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "',' expected");

				}
				else
					mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "'loop index variable expected");
			}
			else
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "'('(' expected");

		}
		else if (mToken == TK_PREP_DEFINE)
		{
			NextRawToken();
			if (mToken == TK_IDENT || mToken >= TK_IF && mToken <= TK_ASM || mToken == TK_TRUE || mToken == TK_FALSE)
			{
				if (mToken != TK_IDENT)
					mTokenIdent = Ident::Unique(TokenNames[mToken]);
				Macro* macro = new Macro(mTokenIdent, nullptr);

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
		else if (mToken == TK_PREP_ERROR)
		{
			NextRawToken();

			if (mToken == TK_STRING)
			{
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, (const char*)mTokenString);
			}
			else
			{
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Missing or invalid error message");
			}
			exit(EXIT_FAILURE);
		}
		else if (mToken == TK_PREP_WARN)
		{
			NextRawToken();

			if (mToken == TK_STRING)
			{
				this->Warning((const char*)mTokenString);
			}
			else
			{
				this->Warning("Missing or invalid warning message");
			}
		}
		else if (mToken == TK_PREP_IDENT)
		{
			Macro* def = nullptr;
			if (mDefineArguments)
				def = mDefineArguments->Lookup(mTokenIdent);
			if (!def)
				def = mDefines->Lookup(mTokenIdent);

			if (def)
			{
				if (def->mNumArguments == -1)
				{
					mToken = TK_STRING;
					int i = 0;
					while ((mTokenString[i] = def->mString[i]))
						i++;
					return;
				}
				else
					mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid preprocessor command", mTokenIdent);
			}
			else
				mErrors->Error(mLocation, EERR_INVALID_PREPROCESSOR, "Invalid preprocessor command", mTokenIdent);
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
			NextPreToken();
			int64 v = PrepParseConditional();
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

				NextPreToken();

				int64 v = PrepParseConditional();
				Macro* macro = mDefines->Lookup(ident);
				if (!macro)
				{
					macro = new Macro(ident, nullptr);
					mDefines->Insert(macro);
				}
				char	buffer[20];
				sprintf_s(buffer, "%d", int(v));
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
			NextPreToken();
			int64 v = PrepParseConditional();
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
			SourceFileDecoder decoder = SFD_NONE;

			uint64 op = mCompilerOptions;
			mCompilerOptions &= ~COPT_PETSCII;

			NextPreToken();
			if (IsIntegerToken())
			{
				limit = int(mTokenInteger);
				NextPreToken();

				if (IsIntegerToken())
				{
					skip = int(mTokenInteger);
					NextPreToken();
				}
			}

			while (mToken == TK_IDENT)
			{
				if (!strcmp(mTokenIdent->mString, "rle"))
					mode = SFM_BINARY_RLE;
				else if (!strcmp(mTokenIdent->mString, "lzo"))
					mode = SFM_BINARY_LZO;
				else if (!strcmp(mTokenIdent->mString, "word"))
					mode = SFM_BINARY_WORD;
				else if (!strcmp(mTokenIdent->mString, "ctm_chars"))
					decoder = SFD_CTM_CHARS;
				else if (!strcmp(mTokenIdent->mString, "ctm_attr1"))
					decoder = SFD_CTM_CHAR_ATTRIB_1;
				else if (!strcmp(mTokenIdent->mString, "ctm_attr2"))
					decoder = SFD_CTM_CHAR_ATTRIB_2;
				else if (!strcmp(mTokenIdent->mString, "ctm_tiles8"))
					decoder = SFD_CTM_TILES_8;
				else if (!strcmp(mTokenIdent->mString, "ctm_tiles8sw"))
					decoder = SFD_CTM_TILES_8_SW;
				else if (!strcmp(mTokenIdent->mString, "ctm_tiles16"))
					decoder = SFD_CTM_TILES_16;
				else if (!strcmp(mTokenIdent->mString, "ctm_map8"))
					decoder = SFD_CTM_MAP_8;
				else if (!strcmp(mTokenIdent->mString, "ctm_map16"))
					decoder = SFD_CTM_MAP_16;
				else if (!strcmp(mTokenIdent->mString, "spd_sprites"))
					decoder = SFD_SPD_SPRITES;
				else
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Invalid embed compression mode", mTokenIdent);

				NextPreToken();
			}

			if (mToken == TK_STRING)
			{
				if (!mPreprocessor->EmbedData("Embedding", (const char*)mTokenString, true, skip, limit, mode, decoder))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", (const char*)mTokenString);
			}
			else if (mToken == TK_LESS_THAN)
			{
				mOffset--;
				StringToken('>', 'a');
				if (!mPreprocessor->EmbedData("Embedding", (const char*)mTokenString, false, skip, limit, mode, decoder))
					mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", (const char*)mTokenString);
			}

			mCompilerOptions = op;
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
					MacroDict* scope = mDefineArguments;

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
							Macro* arg = new Macro(def->mArguments[i], scope);
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
					mDefineArguments = def->mScope;

				ex->mLine = mLine;
				ex->mOffset = mOffset;
				ex->mLink = mMacroExpansion;
				ex->mChar = mTokenChar;

				mMacroExpansion = ex;
				mMacroExpansionDepth++;
				if (mMacroExpansionDepth > 1024)
					mErrors->Error(mLocation, EFATAL_MACRO_EXPANSION_DEPTH, "Maximum macro expansion depth exceeded", mTokenIdent);
				mLine = def->mString;
				mOffset = 0;
				NextChar();
			}
			else
			{
				while (mTokenChar == ' ')
					NextChar();

				while (mTokenChar == '#' && mLine[mOffset] == '#')
				{
					mOffset++;
					NextChar();

					char	tkbase[256];
					strcpy_s(tkbase, mTokenIdent->mString);

					ptrdiff_t	n = 0;
					char		tkident[256];
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

void Scanner::NextSkipRawToken(void)
{
	while (mToken != TK_EOF)
	{
		mToken = TK_ERROR;

		if (mOffset > 1 || !IsWhitespace(mTokenChar))
			mStartOfLine = false;

		while (IsWhitespace(mTokenChar))
		{
			if (mTokenChar == '\n')
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

		if (mTokenChar == '#')
		{
			if (mOffset == 1 || mStartOfLine)
			{
				int		n = 0;
				char	tkprep[128];
				tkprep[0] = 0;

				while (NextChar() && IsAlpha(mTokenChar))
				{
					if (n < 127)
						tkprep[n++] = mTokenChar;
				}
				tkprep[n] = 0;

				if (!strcmp(tkprep, "define"))
					mToken = TK_PREP_DEFINE;
				else if (!strcmp(tkprep, "error"))
					mToken = TK_PREP_ERROR;
				else if (!strcmp(tkprep, "warning"))
					mToken = TK_PREP_WARN;
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
				else if (!strcmp(tkprep, "line"))
					mToken = TK_PREP_LINE;
				else if (!strcmp(tkprep, "assign"))
					mToken = TK_PREP_ASSIGN;
				else if (!strcmp(tkprep, "repeat"))
					mToken = TK_PREP_REPEAT;
				else if (!strcmp(tkprep, "until"))
					mToken = TK_PREP_UNTIL;
				else if (!strcmp(tkprep, "embed"))
					mToken = TK_PREP_EMBED;
				else if (!strcmp(tkprep, "for"))
					mToken = TK_PREP_FOR;
				else
				{
					mToken = TK_PREP_IDENT;
					mTokenIdent = Ident::Unique(tkprep);
				}
			}
			else
			{
				NextChar();
				mToken = TK_HASH;
			}

			return;
		}

		while (mTokenChar != '\n')
		{
			if (!NextChar())
			{
				mToken = TK_EOF;
				return;
			}
		}
	}
}

void Scanner::NextRawToken(void)
{
	if (mUngetToken)
	{
		mToken = mUngetToken;
		mUngetToken = TK_NONE;
	}
	else if (mToken != TK_EOF)
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
				NextPreToken();
			}
			else if (mTokenChar == '/')
			{
				NextChar();
				while (!IsLineBreak(mTokenChar) && NextChar())
					;
				NextPreToken();
			}
			else if (mTokenChar == '=')
			{
				NextChar();
				mToken = TK_ASSIGN_DIV;
			}
			break;
		case '%':
			NextChar();
			if (mAssemblerMode && mTokenChar >= '0' && mTokenChar <= '1')
			{
				int	n = 0;
				int64	mant = 0;
				while (mTokenChar >= '0' && mTokenChar <= '1')
				{
					mant = mant * 2 + (mTokenChar - '0');
					NextChar();
					n++;
				}

				if (n == 0)
					mErrors->Error(mLocation, EERR_SYNTAX, "Missing digits in hex constant");

				mToken = TK_INTEGER;
				mTokenInteger = mant;
			}
			else
			{
				mToken = TK_MOD;
				if (mTokenChar == '=')
				{
					NextChar();
					mToken = TK_ASSIGN_MOD;
				}
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
			if (mTokenChar == ':' && (mCompilerOptions & COPT_CPLUSPLUS))
			{
				mToken = TK_COLCOLON;
				NextChar();
			}
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
			if (mCompilerOptions & COPT_PETSCII)
				CharToken('p');
			else
				CharToken('a');
			break;
		case '"':
			if (mCompilerOptions & COPT_PETSCII)
				StringToken(mTokenChar, 'p');
			else
				StringToken(mTokenChar, 'a');
			break;

		case '#':
		{
			if (!(mAssemblerMode || mPrepCondFalse) || mOffset == 1 || mStartOfLine)
			{
				int		n = 0;
				char	tkprep[128];
				tkprep[0] = 0;

				while (NextChar() && IsAlpha(mTokenChar))
				{
					if (n < 127)
						tkprep[n++] = mTokenChar;
				}
				tkprep[n] = 0;

				if (!strcmp(tkprep, "define"))
					mToken = TK_PREP_DEFINE;
				else if (!strcmp(tkprep, "error"))
					mToken = TK_PREP_ERROR;
				else if (!strcmp(tkprep, "warning"))
					mToken = TK_PREP_WARN;
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
				else if (!strcmp(tkprep, "line"))
					mToken = TK_PREP_LINE;
				else if (!strcmp(tkprep, "assign"))
					mToken = TK_PREP_ASSIGN;
				else if (!strcmp(tkprep, "repeat"))
					mToken = TK_PREP_REPEAT;
				else if (!strcmp(tkprep, "until"))
					mToken = TK_PREP_UNTIL;
				else if (!strcmp(tkprep, "embed"))
					mToken = TK_PREP_EMBED;
				else if (!strcmp(tkprep, "for"))
					mToken = TK_PREP_FOR;
				else
				{
					mToken = TK_PREP_IDENT;
					mTokenIdent = Ident::Unique(tkprep);
				}
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
				tkident[0] = 0;

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
				else if (!strcmp(tkident, "goto"))
					mToken = TK_GOTO;
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
				else if (!strcmp(tkident, "__bankof"))
					mToken = TK_BANKOF;
				else if (!strcmp(tkident, "typedef"))
					mToken = TK_TYPEDEF;
				else if (!strcmp(tkident, "static"))
					mToken = TK_STATIC;
				else if (!strcmp(tkident, "auto"))
					mToken = TK_AUTO;
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
				else if (!strcmp(tkident, "__striped"))
					mToken = TK_STRIPED;
				else if (!strcmp(tkident, "__dynstack"))
					mToken = TK_DYNSTACK;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "namespace"))
					mToken = TK_NAMESPACE;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "using"))
					mToken = TK_USING;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "this"))
					mToken = TK_THIS;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "class"))
					mToken = TK_CLASS;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "public"))
					mToken = TK_PUBLIC;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "protected"))
					mToken = TK_PROTECTED;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "private"))
					mToken = TK_PRIVATE;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "new"))
					mToken = TK_NEW;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "delete"))
					mToken = TK_DELETE;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "virtual"))
					mToken = TK_VIRTUAL;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "template"))
					mToken = TK_TEMPLATE;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "friend"))
					mToken = TK_FRIEND;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "constexpr"))
					mToken = TK_CONSTEXPR;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "typename"))
					mToken = TK_TYPENAME;
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && !strcmp(tkident, "operator"))
				{
					NextRawToken();
					switch (mToken)
					{
					case TK_ASSIGN:
						mTokenIdent = Ident::Unique("operator=");
						break;

					case TK_ASSIGN_ADD:
						mTokenIdent = Ident::Unique("operator+=");
						break;
					case TK_ASSIGN_SUB:
						mTokenIdent = Ident::Unique("operator-=");
						break;
					case TK_ASSIGN_MUL:
						mTokenIdent = Ident::Unique("operator*=");
						break;
					case TK_ASSIGN_DIV:
						mTokenIdent = Ident::Unique("operator/=");
						break;
					case TK_ASSIGN_MOD:
						mTokenIdent = Ident::Unique("operator%=");
						break;
					case TK_ASSIGN_SHL:
						mTokenIdent = Ident::Unique("operator<<=");
						break;
					case TK_ASSIGN_SHR:
						mTokenIdent = Ident::Unique("operator>>=");
						break;
					case TK_ASSIGN_AND:
						mTokenIdent = Ident::Unique("operator&=");
						break;
					case TK_ASSIGN_XOR:
						mTokenIdent = Ident::Unique("operator^=");
						break;
					case TK_ASSIGN_OR:
						mTokenIdent = Ident::Unique("operator|=");
						break;

					case TK_ADD:
						mTokenIdent = Ident::Unique("operator+");
						break;
					case TK_SUB:
						mTokenIdent = Ident::Unique("operator-");
						break;
					case TK_MUL:
						mTokenIdent = Ident::Unique("operator*");
						break;
					case TK_DIV:
						mTokenIdent = Ident::Unique("operator/");
						break;
					case TK_MOD:
						mTokenIdent = Ident::Unique("operator%");
						break;

					case TK_BINARY_AND:
						mTokenIdent = Ident::Unique("operator&");
						break;
					case TK_BINARY_OR:
						mTokenIdent = Ident::Unique("operator|");
						break;
					case TK_BINARY_XOR:
						mTokenIdent = Ident::Unique("operator^");
						break;
					case TK_LOGICAL_NOT:
						mTokenIdent = Ident::Unique("operator!");
						break;

					case TK_LEFT_SHIFT:
						mTokenIdent = Ident::Unique("operator<<");
						break;
					case TK_RIGHT_SHIFT:
						mTokenIdent = Ident::Unique("operator>>");
						break;

					case TK_EQUAL:
						mTokenIdent = Ident::Unique("operator==");
						break;
					case TK_NOT_EQUAL:
						mTokenIdent = Ident::Unique("operator!=");
						break;
					case TK_GREATER_THAN:
						mTokenIdent = Ident::Unique("operator>");
						break;
					case TK_GREATER_EQUAL:
						mTokenIdent = Ident::Unique("operator>=");
						break;
					case TK_LESS_THAN:
						mTokenIdent = Ident::Unique("operator<");
						break;
					case TK_LESS_EQUAL:
						mTokenIdent = Ident::Unique("operator<=");
						break;

					case TK_INC:
						mTokenIdent = Ident::Unique("operator++");
						break;
					case TK_DEC:
						mTokenIdent = Ident::Unique("operator--");
						break;

					case TK_OPEN_BRACKET:
						NextRawToken();
						if (mToken != TK_CLOSE_BRACKET)
							mErrors->Error(mLocation, EERR_INVALID_OPERATOR, "']' expected");
						mTokenIdent = Ident::Unique("operator[]");
						break;

					case TK_OPEN_PARENTHESIS:
						NextRawToken();
						if (mToken != TK_CLOSE_PARENTHESIS)
							mErrors->Error(mLocation, EERR_INVALID_OPERATOR, "')' expected");
						mTokenIdent = Ident::Unique("operator()");
						break;

					case TK_ARROW:
						mTokenIdent = Ident::Unique("operator->");
						break;

					case TK_NEW:
						mTokenIdent = Ident::Unique("operator-new");
						break;
					case TK_DELETE:
						mTokenIdent = Ident::Unique("operator-delete");
						break;

					default:
						// dirty little hack to implement token preview, got to fix
						// this with an infinit preview sequence at one point
						mUngetToken = mToken;
						mToken = TK_OPERATOR;
						return;
					}

					mToken = TK_IDENT;
				}
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
static uint8 p2smap[] = { 0x00, 0x20, 0x00, 0x40, 0x00, 0x60, 0x40, 0x60 };

static inline uint8 p2s(uint8 ch)
{
	return (ch & 0x1f) | p2smap[ch >> 5];
}

static inline uint8 transchar(char mode, uint8 ch)
{
	switch (mode)
	{
	default:
	case 'a':
		return ch;
	case 'p':
		if (ch >= 'a' && ch <= 'z')
			return ch ^ 0x20;
		else if (ch >= 'A' && ch <= 'Z')
			return ch ^ 0x80;
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
	mTokenStringSize = n;
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

	uint8 ch = mLine[mOffset++];

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
	assert(mTokenInteger >= 0);

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
			if (mMacroExpansion->mLoopIndex)
			{
				mMacroExpansion->mLoopCount++;
				if (mMacroExpansion->mLoopCount < mMacroExpansion->mLoopLimit)
				{
					char	buffer[20];
					sprintf_s(buffer, "%d", int(mMacroExpansion->mLoopCount));
					mMacroExpansion->mLoopIndex->SetString(buffer);
					mOffset = 0;
					continue;
				}
			}

			MacroExpansion* mac = mMacroExpansion->mLink;
//			delete mDefineArguments;

			mLine = mMacroExpansion->mLine;
			mOffset = mMacroExpansion->mOffset;
			mTokenChar = mMacroExpansion->mChar;
			mDefineArguments = mMacroExpansion->mDefinedArguments;

			delete mMacroExpansion;
			mMacroExpansion = mac;
			mMacroExpansionDepth--;

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
			else if (mant < 65536)
			{
				mToken = TK_INTEGERU;
			}
			else
			{
				mToken = TK_INTEGERUL;
			}
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
			else if (mant < 32768)
				mToken = TK_INTEGER;
			else if (mant < 65536)
				mToken = TK_INTEGERU;
			else if (mant < 0x80000000)
				mToken = TK_INTEGERL;
			else
				mToken = TK_INTEGERUL;
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
				else if (mant < 32768)
					mToken = TK_INTEGER;
				else
					mToken = TK_INTEGERL;
			}
			mTokenInteger = mant;
		}
		else
		{
			double	facc = double(mant), fract = 1.0;

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


int64 Scanner::PrepParseSimple(bool skip)
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
		NextPreToken();
		break;
	case TK_SUB:
		NextPreToken();
		v = -PrepParseSimple(skip);
		break;
	case TK_LOGICAL_NOT:
		NextPreToken();
		v = !PrepParseSimple(skip);
		break;
	case TK_BINARY_NOT:
		NextPreToken();
		v = ~PrepParseSimple(skip);
		break;
	case TK_OPEN_PARENTHESIS:
		NextPreToken();
		v = PrepParseConditional(skip);
		if (mToken == TK_CLOSE_PARENTHESIS)
			NextPreToken();
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "')' expected");
		break;
	case TK_IDENT:
		if (strcmp(mTokenIdent->mString, "defined") == 0)
		{
			bool	parenthesis = false;

			NextRawToken();
			if (mToken == TK_OPEN_PARENTHESIS)
			{
				NextRawToken();
				parenthesis = true;
			}

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
				NextPreToken();
			}
			else
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Identifier expected");

			if (parenthesis)
			{
				if (mToken == TK_CLOSE_PARENTHESIS)
					NextPreToken();
				else
					mErrors->Error(mLocation, ERRR_PREPROCESSOR, "')' expected");
			}
		}
		else if (skip)
		{
			NextPreToken();
			v = 0;
		}
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Invalid preprocessor symbol", mTokenIdent);
		break;
	default:
		mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Invalid preprocessor token", TokenName(mToken));
		if (mToken != TK_EOL)
			NextPreToken();
	}
	
	return v;
}

int64 Scanner::PrepParseMul(bool skip)
{
	int64	v = PrepParseSimple(skip);
	int64	u;
	for (;;)
	{
		switch (mToken)
		{
		case TK_MUL:
			NextPreToken();
			v *= PrepParseSimple(skip);
			break;
		case TK_DIV:
			NextPreToken();
			u = PrepParseSimple(skip);
			if (skip)
				;
			else if (u == 0)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Division by zero");
			else
				v /= u;
			break;
		case TK_MOD:
			u = PrepParseSimple(skip);
			if (skip)
				;
			else if (u == 0)
				mErrors->Error(mLocation, ERRR_PREPROCESSOR, "Division by zero");
			else
				v %= u;
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseAdd(bool skip)
{
	int64	v = PrepParseMul(skip);
	for (;;)
	{
		switch (mToken)
		{
		case TK_ADD:
			NextPreToken();
			v += PrepParseMul(skip);
			break;
		case TK_SUB:
			NextPreToken();
			v -= PrepParseMul(skip);
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseShift(bool skip)
{
	int64	v = PrepParseAdd(skip);
	for (;;)
	{
		switch (mToken)
		{
		case TK_LEFT_SHIFT:
			NextPreToken();
			v <<= PrepParseAdd(skip);
			break;
		case TK_RIGHT_SHIFT:
			NextPreToken();
			v >>= PrepParseAdd(skip);
			break;
		default:
			return v;
		}
	}
}

int64 Scanner::PrepParseRel(bool skip)
{
	int64	v = PrepParseShift(skip);
	for (;;)
	{
		switch (mToken)
		{
		case TK_LESS_THAN:
			NextPreToken();
			v = v < PrepParseShift(skip);
			break;
		case TK_GREATER_THAN:
			NextPreToken();
			v = v > PrepParseShift(skip);
			break;
		case TK_LESS_EQUAL:
			NextPreToken();
			v = v <= PrepParseShift(skip);
			break;
		case TK_GREATER_EQUAL:
			NextPreToken();
			v = v >= PrepParseShift(skip);
			break;
		case TK_EQUAL:
			NextPreToken();
			v = v == PrepParseShift(skip);
			break;
		case TK_NOT_EQUAL:
			NextPreToken();
			v = v != PrepParseShift(skip);
			break;
		default:
			return v;
		}
	}

}

int64 Scanner::PrepParseBinaryAnd(bool skip)
{
	int64	v = PrepParseRel(skip);
	while (mToken == TK_BINARY_AND)
	{
		NextPreToken();
		v &= PrepParseRel(skip);
	}
	return v;
}

int64 Scanner::PrepParseBinaryXor(bool skip)
{
	int64	v = PrepParseBinaryAnd(skip);
	while (mToken == TK_BINARY_XOR)
	{
		NextPreToken();
		v ^= PrepParseBinaryAnd(skip);
	}
	return v;
}

int64 Scanner::PrepParseBinaryOr(bool skip)
{
	int64	v = PrepParseBinaryXor(skip);
	while (mToken == TK_BINARY_OR)
	{
		NextPreToken();
		v |= PrepParseBinaryXor(skip);
	}
	return v;
}

int64 Scanner::PrepParseLogicalAnd(bool skip)
{
	int64	v = PrepParseBinaryOr(skip);
	while (mToken == TK_LOGICAL_AND)
	{
		NextPreToken();
		if (!PrepParseBinaryOr(skip || !v))
			v = 0;
	}
	return v;
}

int64 Scanner::PrepParseLogicalOr(bool skip)
{
	int64	v = PrepParseLogicalAnd(skip);
	while (mToken == TK_LOGICAL_OR)
	{
		NextPreToken();
		if (PrepParseLogicalAnd(skip || v))
			v = 1;
	}
	return v;
}

int64 Scanner::PrepParseConditional(bool skip)
{
	int64	v = PrepParseLogicalOr(skip);
	if (mToken == TK_QUESTIONMARK)
	{
		NextPreToken();
		int64	vt = PrepParseConditional(skip || v);
		if (mToken == TK_COLON)
			NextPreToken();
		else
			mErrors->Error(mLocation, ERRR_PREPROCESSOR, "':' expected");
		int64	vf = PrepParseConditional(skip || !v);
		if (v)
			v = vt;
		else
			v = vf;
	}

	return v;
}

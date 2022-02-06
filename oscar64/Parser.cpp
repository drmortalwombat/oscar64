#include "Parser.h"
#include <string.h>
#include "Assembler.h"
#include "MachineTypes.h"

Parser::Parser(Errors* errors, Scanner* scanner, CompilationUnits* compilationUnits)
	: mErrors(errors), mScanner(scanner), mCompilationUnits(compilationUnits)
{
	mGlobals = new DeclarationScope(compilationUnits->mScope);
	mScope = mGlobals;

	mCodeSection = compilationUnits->mSectionCode;
	mDataSection = compilationUnits->mSectionData;
	mBSSection = compilationUnits->mSectionBSS;

	for (int i = 0; i < 256; i++)
		mCharMap[i] = i;
}

Parser::~Parser(void)
{

}

Declaration* Parser::ParseStructDeclaration(uint64 flags, DecType dt)
{
	const Ident* structName = nullptr;

	Declaration	*	dec = new Declaration(mScanner->mLocation, dt);

	mScanner->NextToken();
	if (mScanner->mToken == TK_IDENT)
	{
		structName = mScanner->mTokenIdent;
		mScanner->NextToken();
		Declaration* edec = mScope->Lookup(structName);
		if (edec && mScanner->mToken != TK_OPEN_BRACE)
		{
			dec = edec;
		}
		else
		{
			Declaration* pdec = mScope->Insert(structName, dec);
			if (pdec)
			{
				if (pdec->mType == dt && !(pdec->mFlags & DTF_DEFINED))
				{
					dec = pdec;
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Error duplicate struct declaration", structName->mString);
				}
			}
		}
	}

	if (!dec->mIdent || !dec->mScope)
	{
		dec->mIdent = structName;
		dec->mScope = new DeclarationScope(nullptr);
	}

	if (mScanner->mToken == TK_OPEN_BRACE)
	{
		mScanner->NextToken();
		Declaration* mlast = nullptr;
		for (;;)
		{
			Declaration* mdec = ParseDeclaration(false);

			int	offset = dec->mSize;
			if (dt == DT_TYPE_UNION)
				offset = 0;

			while (mdec)
			{
				if (!(mdec->mBase->mFlags & DTF_DEFINED))
					mErrors->Error(mdec->mLocation, EERR_UNDEFINED_OBJECT, "Undefined type used in struct member declaration");
				mdec->mType = DT_ELEMENT;
				mdec->mOffset = offset;

				offset += mdec->mBase->mSize;
				if (offset > dec->mSize)
					dec->mSize = offset;

				if (dec->mScope->Insert(mdec->mIdent, mdec))
					mErrors->Error(mdec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate struct member declaration", mdec->mIdent->mString);

				if (mlast)
					mlast->mNext = mdec;
				else
					dec->mParams = mdec;
				mlast = mdec;
				mdec = mdec->mNext;
			}

			if (mScanner->mToken == TK_SEMICOLON)
				mScanner->NextToken();
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "';' expected");
				break;
			}

			if (mScanner->mToken == TK_CLOSE_BRACE)
			{
				mScanner->NextToken();
				break;
			}
		}

		if (mlast)
			mlast->mNext = nullptr;
		else
			dec->mParams = nullptr;

		dec->mFlags |= DTF_DEFINED;
	}

	return dec;
}

Declaration* Parser::ParseBaseTypeDeclaration(uint64 flags)
{
	Declaration* dec = nullptr;

	switch (mScanner->mToken)
	{
	case TK_UNSIGNED:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_INTEGER);
		dec->mFlags = flags | DTF_DEFINED;
		mScanner->NextToken();
		if (mScanner->mToken == TK_INT || mScanner->mToken == TK_SHORT)
		{
			dec->mSize = 2;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_LONG)
		{
			dec->mSize = 4;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_CHAR)
		{
			dec->mSize = 1;
			mScanner->NextToken();
		}
		else
			dec->mSize = 2;

		break;

	case TK_SIGNED:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_INTEGER);
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
		mScanner->NextToken();
		if (mScanner->mToken == TK_INT || mScanner->mToken == TK_SHORT)
		{
			dec->mSize = 2;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_LONG)
		{
			dec->mSize = 4;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_CHAR)
		{
			dec->mSize = 1;
			mScanner->NextToken();
		}
		else
			dec->mSize = 2;
		break;

	case TK_CONST:
		mScanner->NextToken();
		return ParseBaseTypeDeclaration(flags | DTF_CONST);

	case TK_VOLATILE:
		mScanner->NextToken();
		return ParseBaseTypeDeclaration(flags | DTF_VOLATILE);

	case TK_LONG:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_INTEGER);
		dec->mSize = 4;
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
		mScanner->NextToken();
		break;

	case TK_SHORT:
	case TK_INT:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_INTEGER);
		dec->mSize = 2;
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
		mScanner->NextToken();
		break;

	case TK_CHAR:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_INTEGER);
		dec->mSize = 1;
		dec->mFlags = flags | DTF_DEFINED;
		mScanner->NextToken();
		break;

	case TK_BOOL:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_BOOL);
		dec->mSize = 1;
		dec->mFlags = flags | DTF_DEFINED;
		mScanner->NextToken();
		break;

	case TK_FLOAT:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_FLOAT);
		dec->mSize = 4;
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
		mScanner->NextToken();
		break;

	case TK_VOID:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_VOID);
		dec->mSize = 0;
		dec->mFlags = flags | DTF_DEFINED;
		mScanner->NextToken();
		break;

	case TK_IDENT:
		dec = mScope->Lookup(mScanner->mTokenIdent);
		if (dec && dec->mType <= DT_TYPE_FUNCTION)
		{
			if (dec->IsSimpleType() && (flags & ~dec->mFlags))
			{
				Declaration* ndec = new Declaration(dec->mLocation, dec->mType);
				ndec->mFlags = dec->mFlags | flags;
				ndec->mSize = dec->mSize;
				ndec->mBase = dec->mBase;
				dec = ndec;
			}
			else if (dec->mType == DT_TYPE_STRUCT && (flags & ~dec->mFlags))
			{
				Declaration* ndec = new Declaration(dec->mLocation, dec->mType);
				ndec->mFlags = dec->mFlags | flags;
				ndec->mSize = dec->mSize;
				ndec->mBase = dec->mBase;
				ndec->mScope = dec->mScope;
				ndec->mParams = dec->mParams;
				ndec->mIdent = dec->mIdent;
				dec = ndec;
			}
			mScanner->NextToken();
		}
		else if (!dec)
		{
			mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Identifier not defined", mScanner->mTokenIdent->mString);
			mScanner->NextToken();
		}
		else
		{			
			mErrors->Error(mScanner->mLocation, EERR_NOT_A_TYPE, "Identifier is no type", mScanner->mTokenIdent->mString);
			mScanner->NextToken();
		}
		break;

	case TK_ENUM:
	{
		dec = new Declaration(mScanner->mLocation, DT_TYPE_ENUM);
		dec->mFlags = flags;
		dec->mSize = 1;

		mScanner->NextToken();
		if (mScanner->mToken == TK_IDENT)
		{
			dec->mIdent = mScanner->mTokenIdent;
			
			if (mScope->Insert(dec->mIdent, dec))
				mErrors->Error(dec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate name");
			mScanner->NextToken();
		}

		int	nitem = 0;
		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			mScanner->NextToken();
			if (mScanner->mToken == TK_IDENT)
			{
				int	minValue = 0, maxValue = 0;

				for (;;)
				{
					Declaration* cdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
					cdec->mBase = dec;
					cdec->mSize = 2;
					if (mScanner->mToken == TK_IDENT)
					{
						cdec->mIdent = mScanner->mTokenIdent;
						if (mScope->Insert(cdec->mIdent, cdec) != nullptr)
							mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate declaration", mScanner->mTokenIdent->mString);
						mScanner->NextToken();
					}
					if (mScanner->mToken == TK_ASSIGN)
					{
						mScanner->NextToken();
						Expression* exp = ParseRExpression();
						if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
							nitem = int(exp->mDecValue->mInteger);
						else
							mErrors->Error(mScanner->mLocation, EERR_CONSTANT_TYPE, "Integer constant expected");
					}
					cdec->mInteger = nitem++;
					if (cdec->mInteger < minValue)
						minValue = cdec->mInteger;
					else if (cdec->mInteger > maxValue)
						maxValue = cdec->mInteger;

					if (mScanner->mToken == TK_COMMA)
						mScanner->NextToken();
					else
						break;
				}

				if (minValue < 0)
				{
					dec->mFlags |= DTF_SIGNED;
					if (minValue < -128 || maxValue > 127)
						dec->mSize = 2;
				}
				else if (maxValue > 255)
					dec->mSize = 2;
			}

			if (mScanner->mToken == TK_CLOSE_BRACE)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'}' expected");
		}
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'{' expected");

		dec->mFlags |= DTF_DEFINED;
		break;
	}
	case TK_STRUCT:
		dec = ParseStructDeclaration(flags, DT_TYPE_STRUCT);
		break;
	case TK_UNION:
		dec = ParseStructDeclaration(flags, DT_TYPE_UNION);
		break;
	default:
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Declaration starts with invalid token", TokenNames[mScanner->mToken]);
		mScanner->NextToken();
	}

	if (!dec)
	{
		dec = new Declaration(mScanner->mLocation, DT_TYPE_VOID);
		dec->mSize = 0;
		dec->mFlags |= DTF_DEFINED;
	}

	if (mScanner->mToken == TK_CONST)
	{
		dec = dec->ToConstType();
		mScanner->NextToken();
	}

	return dec;

}

Declaration* Parser::ParsePostfixDeclaration(void)
{
	Declaration* dec;

	if (mScanner->mToken == TK_MUL)
	{
		mScanner->NextToken();

		Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
		ndec->mSize = 2;
		ndec->mFlags |= DTF_DEFINED;

		for (;;)
		{
			if (mScanner->mToken == TK_CONST)
			{
				ndec->mFlags |= DTF_CONST;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_VOLATILE)
			{
				ndec->mFlags |= DTF_VOLATILE;
				mScanner->NextToken();
			}
			else
				break;
		}

		Declaration* dec = ParsePostfixDeclaration();
		ndec->mBase = dec;
		return ndec;
	}
	else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
	{
		mScanner->NextToken();
		Declaration* vdec = ParsePostfixDeclaration();
		if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
		dec = vdec;
	}
	else if (mScanner->mToken == TK_IDENT)
	{
		dec = new Declaration(mScanner->mLocation, DT_VARIABLE);
		dec->mIdent = mScanner->mTokenIdent;
		dec->mSection = mBSSection;
		dec->mBase = nullptr;
		mScanner->NextToken();
	}
	else
	{
		dec = new Declaration(mScanner->mLocation, DT_ANON);
		dec->mBase = nullptr;
	}

	for (;;)
	{
		if (mScanner->mToken == TK_OPEN_BRACKET)
		{
			Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_ARRAY);
			ndec->mSize = 0;
			ndec->mFlags = 0;
			mScanner->NextToken();
			if (mScanner->mToken != TK_CLOSE_BRACKET)
			{
				Expression* exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecType->IsIntegerType() && exp->mDecValue->mType == DT_CONST_INTEGER)
					ndec->mSize = int(exp->mDecValue->mInteger);
				else
					mErrors->Error(exp->mLocation, EERR_CONSTANT_TYPE, "Constant integer expression expected");
				ndec->mFlags |= DTF_DEFINED;
			}
			if (mScanner->mToken == TK_CLOSE_BRACKET)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "']' expected");
			ndec->mBase = dec;
			dec = ndec;
		}
		else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
		{
			Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
			ndec->mSize = 0;
			Declaration* pdec = nullptr;
			mScanner->NextToken();
			if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
			{
				int	vi = 0;
				for(;;)
				{
					if (mScanner->mToken == TK_ELLIPSIS)
					{
						ndec->mFlags |= DTF_VARIADIC;
						mScanner->NextToken();
						break;
					}

					Declaration* bdec = ParseBaseTypeDeclaration(0);
					Declaration* adec = ParsePostfixDeclaration();

					adec = ReverseDeclaration(adec, bdec);

					if (adec->mBase->mType == DT_TYPE_VOID)
					{
						if (pdec)
							mErrors->Error(pdec->mLocation, EERR_WRONG_PARAMETER, "Invalid void argument");
						break;
					}
					else
					{
						if (!(adec->mBase->mFlags & DTF_DEFINED) && adec->mBase->mType != DT_TYPE_ARRAY)
							mErrors->Error(adec->mLocation, EERR_UNDEFINED_OBJECT, "Type of argument not defined");

						adec->mType = DT_ARGUMENT;
						adec->mVarIndex = vi;
						adec->mOffset = 0;
						if (adec->mBase->mType == DT_TYPE_ARRAY)
						{
							Declaration		*	ndec = new Declaration(adec->mBase->mLocation, DT_TYPE_POINTER);
							ndec->mBase = adec->mBase->mBase;
							ndec->mSize = 2;
							ndec->mFlags |= DTF_DEFINED;
							adec->mBase = ndec;
						}

						adec->mSize = adec->mBase->mSize;

						vi += adec->mSize;
						if (pdec)
							pdec->mNext = adec;
						else
							ndec->mParams = adec;
						pdec = adec;

						if (mScanner->mToken == TK_COMMA)
							mScanner->NextToken();
						else
							break;
					}
				}

				if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
			}
			else
				mScanner->NextToken();
			ndec->mBase = dec;
			dec = ndec;
		}
		else
			return dec;
	}
}

Declaration* Parser::ReverseDeclaration(Declaration* odec, Declaration* bdec)
{
	Declaration* cdec = odec->mBase;

	if (bdec)
	{
		if (odec->mType == DT_TYPE_ARRAY)
			odec->mSize *= bdec->mSize;
		else if (odec->mType == DT_VARIABLE || odec->mType == DT_ARGUMENT || odec->mType == DT_ANON)
			odec->mSize = bdec->mSize;
		odec->mBase = bdec;
	}

	if (cdec)
		return ReverseDeclaration(cdec, odec);
	else
		return odec;
}

Declaration * Parser::CopyConstantInitializer(int offset, Declaration* dtype, Expression* exp)
{
	Declaration* dec = exp->mDecValue;

	if (exp->mType == EX_CONSTANT)
	{
		if (!dtype->CanAssign(exp->mDecType))
			mErrors->Error(exp->mLocation, EERR_CONSTANT_INITIALIZER, "Incompatible constant initializer");
		else
		{
			if (dec->mType == DT_CONST_FLOAT)
			{
				if (dtype->IsIntegerType() || dtype->mType == DT_TYPE_POINTER)
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_INTEGER);
					ndec->mInteger = int(dec->mNumber);
					ndec->mBase = dtype;
					dec = ndec;
				}
				else
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_FLOAT);
					ndec->mNumber = dec->mNumber;
					ndec->mBase = dtype;
					dec = ndec;
				}
			}
			else if (dec->mType == DT_CONST_INTEGER)
			{
				if (dtype->mType == DT_TYPE_FLOAT)
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_FLOAT);
					ndec->mNumber = double(dec->mInteger);
					ndec->mBase = dtype;
					dec = ndec;
				}
				else
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_INTEGER);
					ndec->mInteger = dec->mInteger;
					ndec->mBase = dtype;
					dec = ndec;
				}
			}
			else if (dec->mType == DT_CONST_DATA)
			{
				if (dtype->mType == DT_TYPE_POINTER)
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_POINTER);
					ndec->mValue = exp;
					ndec->mBase = dtype;
					dec = ndec;
				}
				else
				{
					Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_DATA);
					ndec->mSection = dec->mSection;
					ndec->mData = dec->mData;
					ndec->mSize = dec->mSize;
					ndec->mBase = dtype;
					dec = ndec;
				}
			}

			dec->mOffset = offset;
		}
	}
	else if (dtype->mType == DT_TYPE_POINTER && dec->mType == DT_VARIABLE && dec->mBase->mType == DT_TYPE_ARRAY && (dec->mFlags & DTF_GLOBAL))
	{
		if (dtype->CanAssign(exp->mDecType))
		{
			Declaration	*	ndec = new Declaration(dec->mLocation, DT_CONST_POINTER);
			ndec->mValue = exp;
			ndec->mBase = dtype;
			dec = ndec;
			dec->mOffset = offset;
		}
		else
			mErrors->Error(exp->mLocation, EERR_CONSTANT_INITIALIZER, "Incompatible constant initializer");
	}
	else
		mErrors->Error(exp->mLocation, EERR_CONSTANT_INITIALIZER, "Constant initializer expected");

	return dec;
}

uint8* Parser::ParseStringLiteral(int msize)
{
	int	size = strlen(mScanner->mTokenString);
	if (size + 1 > msize)
		msize = size + 1;
	uint8* d = new uint8[msize];

	int i = 0;
	while (i < size)
	{
		d[i] = mCharMap[(uint8)mScanner->mTokenString[i]];
		i++;
	}

	mScanner->NextToken();

	while (mScanner->mToken == TK_PREP_PRAGMA)
	{
		mScanner->NextToken();
		ParsePragma();
	}

	// automatic string concatenation
	while (mScanner->mToken == TK_STRING)
	{
		int	s = strlen(mScanner->mTokenString);

		if (size + s + 1 > msize)
			msize = size + s + 1;

		uint8* nd = new uint8[msize];
		memcpy(nd, d, size);
		int i = 0;
		while (i < s)
		{
			nd[i + size] = mCharMap[(uint8)mScanner->mTokenString[i]];
			i++;
		}
		size += s;
		delete[] d;
		d = nd;
		mScanner->NextToken();

		while (mScanner->mToken == TK_PREP_PRAGMA)
		{
			mScanner->NextToken();
			ParsePragma();
		}
	}

	while (size < msize)
		d[size++] = 0;

	return d;
}

Expression* Parser::ParseInitExpression(Declaration* dtype)
{
	Expression* exp = nullptr;
	Declaration* dec;

	if (dtype->mType == DT_TYPE_ARRAY || dtype->mType == DT_TYPE_STRUCT || dtype->mType == DT_TYPE_UNION)
	{
		if (dtype->mType != DT_TYPE_ARRAY && !(dtype->mFlags & DTF_DEFINED))
		{
			if (dtype->mIdent)
				mErrors->Error(mScanner->mLocation, EERR_UNDEFINED_OBJECT, "Constant for undefined type", dtype->mIdent->mString);
			else
				mErrors->Error(mScanner->mLocation, EERR_UNDEFINED_OBJECT, "Constant for undefined annonymous type");
		}
		if (ConsumeTokenIf(TK_OPEN_BRACE))
		{
			dec = new Declaration(mScanner->mLocation, DT_CONST_STRUCT);
			dec->mBase = dtype;
			dec->mSize = dtype->mSize;
			dec->mSection = mDataSection;

			Declaration* last = nullptr;

			if (dtype->mType == DT_TYPE_ARRAY)
			{
				int	index = 0;
				while (!(dtype->mFlags & DTF_DEFINED) || index < dtype->mSize)
				{
					Expression* texp = ParseInitExpression(dtype->mBase);
					Declaration* cdec = CopyConstantInitializer(index, dtype->mBase, texp);

					if (last)
						last->mNext = cdec;
					else
						dec->mParams = cdec;
					last = cdec;

					index += dtype->mBase->mSize;
					if (!ConsumeTokenIf(TK_COMMA))
						break;
					if (mScanner->mToken == TK_CLOSE_BRACE)
						break;
				}

				if (!(dtype->mFlags & DTF_DEFINED))
				{
					dtype->mFlags |= DTF_DEFINED;
					dtype->mSize = index;
					dec->mSize = index;
				}
			}
			else
			{
				Declaration* mdec = dtype->mParams;
				while (mdec)
				{
					if (ConsumeTokenIf(TK_DOT))
					{
						if (mScanner->mToken == TK_IDENT)
						{
							Declaration* ndec = dtype->mScope->Lookup(mScanner->mTokenIdent);
							if (ndec)
								mdec = ndec;
							else
								mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Struct member not found");

							mScanner->NextToken();
							ConsumeToken(TK_ASSIGN);
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Identifier expected");
					}

					Expression* texp = ParseInitExpression(mdec->mBase);

					Declaration* cdec = CopyConstantInitializer(mdec->mOffset, mdec->mBase, texp);

					if (last)
						last->mNext = cdec;
					else
						dec->mParams = cdec;
					last = cdec;

					if (!ConsumeTokenIf(TK_COMMA))
						break;
					mdec = mdec->mNext;
				}
			}

			ConsumeToken(TK_CLOSE_BRACE);

			if (last)
				last->mNext = nullptr;
			else
				dec->mParams = nullptr;
		}
		else if (mScanner->mToken == TK_STRING && dtype->mType == DT_TYPE_ARRAY && dtype->mBase->mType == DT_TYPE_INTEGER && dtype->mBase->mSize == 1)
		{
			uint8* d = ParseStringLiteral(dtype->mSize);
			int		ds = strlen((char *)d);

			if (!(dtype->mFlags & DTF_DEFINED))
			{
				dtype->mFlags |= DTF_DEFINED;
				dtype->mSize = ds + 1;
			}

			dec = new Declaration(mScanner->mLocation, DT_CONST_DATA);
			dec->mBase = dtype;
			dec->mSize = dtype->mSize;
			dec->mSection = mDataSection;

			dec->mData = d;

			if (ds > dtype->mSize)
				mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "String constant is too large for char array");
		}
		else
		{
			exp = ParseRExpression();
			return exp;
		}

		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecType = dtype;
		exp->mDecValue = dec;
	}
	else
	{
		exp = ParseRExpression();
		if (dtype->mType == DT_TYPE_POINTER && exp->mDecType && exp->mDecType->mType == DT_TYPE_ARRAY && exp->mType == EX_CONSTANT)
		{
			Declaration* ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_POINTER);
			ndec->mValue = exp;
			dec = ndec;

			Expression	*	nexp = new Expression(mScanner->mLocation, EX_CONSTANT);
			nexp->mDecType = new Declaration(dec->mLocation, DT_TYPE_POINTER);
			nexp->mDecType->mBase = exp->mDecType->mBase;
			nexp->mDecType->mSize = 2;
			nexp->mDecType->mFlags |= DTF_DEFINED;
			nexp->mDecValue = ndec;
			exp = nexp;
		}
		else if (exp->mType == EX_CONSTANT && exp->mDecType != dtype)
		{
			Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTANT);
			nexp->mDecType = dtype;

			if (dtype->mType == DT_TYPE_INTEGER || dtype->mType == DT_TYPE_ENUM || dtype->mType == DT_TYPE_BOOL)
			{
				Declaration* ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_INTEGER);
				ndec->mBase = dtype;
				ndec->mSize = dtype->mSize;					

				if (exp->mDecValue->mType == DT_CONST_INTEGER)
					ndec->mInteger = exp->mDecValue->mInteger;
				else if (exp->mDecValue->mType == DT_CONST_FLOAT)
					ndec->mInteger = int64(exp->mDecValue->mNumber);
				else
					mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Illegal integer constant initializer");

				nexp->mDecValue = ndec;
			}
			else if (dtype->mType == DT_TYPE_FLOAT)
			{
				Declaration* ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_FLOAT);
				ndec->mBase = dtype;
				ndec->mSize = dtype->mSize;

				if (exp->mDecValue->mType == DT_CONST_INTEGER)
					ndec->mNumber = double(exp->mDecValue->mInteger);
				else if (exp->mDecValue->mType == DT_CONST_FLOAT)
					ndec->mNumber = exp->mDecValue->mNumber;
				else
					mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Illegal float constant initializer");

				nexp->mDecValue = ndec;
			}
			else if (dtype->mType == DT_TYPE_POINTER)
			{
				if (exp->mDecValue->mType == DT_CONST_ADDRESS)
					;
				else if (exp->mDecValue->mType == DT_CONST_FUNCTION)
				{
					Declaration* ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_POINTER);
					ndec->mValue = exp;
					dec = ndec;

					Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTANT);
					nexp->mDecType = new Declaration(dec->mLocation, DT_TYPE_POINTER);
					nexp->mDecType->mBase = exp->mDecType;
					nexp->mDecType->mSize = 2;
					nexp->mDecType->mFlags |= DTF_DEFINED;
					nexp->mDecValue = ndec;
					exp = nexp;
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Illegal pointer constant initializer");

				nexp = exp;
			}
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Illegal constant initializer");
				nexp = exp;
			}

			exp = nexp;
		}

	}

	return exp;
}

Declaration* Parser::ParseDeclaration(bool variable)
{
	bool	definingType = false;
	uint64	storageFlags = 0, typeFlags = 0;

	if (mScanner->mToken == TK_TYPEDEF)
	{
		definingType = true;
		variable = false;
		mScanner->NextToken();
	}
	else
	{
		for (;;)
		{
			if (mScanner->mToken == TK_STATIC)
			{
				storageFlags |= DTF_STATIC;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_EXTERN)
			{
				storageFlags |= DTF_EXTERN;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_INLINE)
			{
				storageFlags |= DTF_REQUEST_INLINE;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_EXPORT)
			{
				storageFlags |= DTF_EXPORT;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_FASTCALL)
			{
				storageFlags |= DTF_FASTCALL;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_NATIVE)
			{
				storageFlags |= DTF_NATIVE;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_INTERRUPT)
			{
				storageFlags |= DTF_INTERRUPT | DTF_NATIVE;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_HWINTERRUPT)
			{
				storageFlags |= DTF_INTERRUPT | DTF_HWINTERRUPT | DTF_NATIVE;
				mScanner->NextToken();
			}
			else
				break;
		}
	}

	Declaration* bdec = ParseBaseTypeDeclaration(0);

	Declaration* rdec = nullptr, * ldec = nullptr;

	for (;;)
	{
		Declaration* ndec = ParsePostfixDeclaration();

		ndec = ReverseDeclaration(ndec, bdec);

		Declaration* npdec = ndec;

		if (npdec->mBase->mType == DT_TYPE_POINTER)
			npdec = npdec->mBase;

		// Make room for return value pointer on struct return
		if (npdec->mBase->mType == DT_TYPE_FUNCTION && npdec->mBase->mBase->mType == DT_TYPE_STRUCT)
		{
			Declaration* pdec = npdec->mBase->mParams;
			while (pdec)
			{
				pdec->mVarIndex += 2;
				pdec = pdec->mNext;
			}
		}
		

		if (definingType)
		{
			if (ndec->mIdent)
			{
				Declaration* pdec = mScope->Insert(ndec->mIdent, ndec->mBase);
				if (pdec && pdec != ndec->mBase)
					mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate type declaration", ndec->mIdent->mString);
			}
		}
		else
		{
			if (variable)
			{
				ndec->mFlags |= storageFlags;
				ndec->mFlags |= ndec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

				if (ndec->mBase->mType == DT_TYPE_FUNCTION)
				{
					ndec->mType = DT_CONST_FUNCTION;
					ndec->mSection = mCodeSection;
					ndec->mBase->mFlags |= typeFlags;
				}

				if (ndec->mIdent)
				{
					Declaration* pdec;
					
					if (mGlobals == mScope && !(storageFlags & DTF_STATIC))
					{
						pdec = mCompilationUnits->mScope->Insert(ndec->mIdent, ndec);

						Declaration	*	ldec = mScope->Insert(ndec->mIdent, pdec ? pdec : ndec);
						if (ldec && ldec != pdec)
							mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate definition");
					}
					else
						pdec = mScope->Insert(ndec->mIdent, ndec);

					if (pdec)
					{
						if (pdec->mType == DT_CONST_FUNCTION && ndec->mBase->mType == DT_TYPE_FUNCTION)
						{
							if (!ndec->mBase->IsSame(pdec->mBase))
								mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Function declaration differs", ndec->mIdent->mString);
							else if (ndec->mFlags & ~pdec->mFlags & (DTF_HWINTERRUPT | DTF_INTERRUPT | DTF_FASTCALL | DTF_NATIVE))
								mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Function call type declaration differs", ndec->mIdent->mString);
							else
							{
								// 
								// Take parameter names from new declaration
								//
								Declaration* npdec = ndec->mBase->mParams, *ppdec = pdec->mBase->mParams;
								while (npdec && ppdec)
								{
									if (npdec->mIdent)
										ppdec->mIdent = npdec->mIdent;
									npdec = npdec->mNext;
									ppdec = ppdec->mNext;
								}
							}

							ndec = pdec;
						}
						else if (ndec->mFlags & DTF_EXTERN)
						{
							if (!ndec->mBase->IsSame(pdec->mBase))
							{
								if (ndec->mBase->mType == DT_TYPE_ARRAY && pdec->mBase->mType == DT_TYPE_ARRAY && ndec->mBase->mBase->IsSame(pdec->mBase->mBase) && ndec->mBase->mSize == 0)
									;
								else if (ndec->mBase->mType == DT_TYPE_POINTER && pdec->mBase->mType == DT_TYPE_ARRAY && ndec->mBase->mBase->IsSame(pdec->mBase->mBase))
									;
								else
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Variable declaration differs", ndec->mIdent->mString);
							}

							ndec = pdec;
						}
						else if (pdec->mFlags & DTF_EXTERN)
						{
							if (!ndec->mBase->IsSame(pdec->mBase))
							{
								if (ndec->mBase->mType == DT_TYPE_ARRAY && pdec->mBase->mType == DT_TYPE_ARRAY && ndec->mBase->mBase->IsSame(pdec->mBase->mBase) && pdec->mBase->mSize == 0)
									pdec->mBase->mSize = ndec->mBase->mSize;
								else if (pdec->mBase->mType == DT_TYPE_POINTER && ndec->mBase->mType == DT_TYPE_ARRAY && ndec->mBase->mBase->IsSame(pdec->mBase->mBase))
								{
									pdec->mBase = ndec->mBase;
								}
								else
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Variable declaration differs", ndec->mIdent->mString);
							}

							if (!(ndec->mFlags & DTF_EXTERN))
								pdec->mFlags &= ~DTF_EXTERN;

							ndec = pdec;
						}
						else
							mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate variable declaration", ndec->mIdent->mString);
					}
				}

				if (mGlobals == mScope || (ndec->mFlags & DTF_STATIC))
				{
					ndec->mFlags |= DTF_GLOBAL;
					ndec->mVarIndex = -1;
				}
				else
					ndec->mVarIndex = mLocalIndex++;
			}

			ndec->mOffset = 0;

			if (rdec)
				ldec->mNext = ndec;
			else
				rdec = ndec;
			ldec = ndec;
			ndec->mNext = nullptr;

			if (mScanner->mToken == TK_ASSIGN)
			{
				mScanner->NextToken();
				ndec->mValue = ParseInitExpression(ndec->mBase);
				if (ndec->mFlags & DTF_GLOBAL)
					ndec->mSection = mDataSection;
				ndec->mSize = ndec->mBase->mSize;
			}

			if (storageFlags & DTF_EXPORT)
			{
				mCompilationUnits->AddReferenced(ndec);
			}
		}

		if (mScanner->mToken == TK_COMMA)
			mScanner->NextToken();
		else if (mScanner->mToken == TK_OPEN_BRACE)
		{
			if (ndec->mBase->mType == DT_TYPE_FUNCTION)
			{
				if (ndec->mFlags & DTF_DEFINED)
					mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate function definition");

				ndec->mVarIndex = -1;
				ndec->mValue = ParseFunction(ndec->mBase);
				ndec->mFlags |= DTF_DEFINED;
				ndec->mNumVars = mLocalIndex;
			}
			return rdec;
		}
		else
			return rdec;
	}

	return rdec;
}

Expression* Parser::ParseDeclarationExpression(void)
{
	Declaration* dec;
	Expression* exp = nullptr, * rexp = nullptr;

	dec = ParseDeclaration(true);
	if (dec->mType == DT_ANON && dec->mNext == 0)
	{
		exp = new Expression(dec->mLocation, EX_TYPE);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;
	}
	else
	{
		while (dec)
		{
			if (dec->mValue && !(dec->mFlags & DTF_GLOBAL))
			{
				Expression* nexp = new Expression(dec->mValue->mLocation, EX_INITIALIZATION);
				nexp->mToken = TK_ASSIGN;
				nexp->mLeft = new Expression(dec->mLocation, EX_VARIABLE);
				nexp->mLeft->mDecValue = dec;
				nexp->mLeft->mDecType = dec->mBase;
				nexp->mDecType = nexp->mLeft->mDecType;

				nexp->mRight = dec->mValue;

				if (!exp)
					exp = nexp;
				else
				{
					if (!rexp)
					{
						rexp = new Expression(nexp->mLocation, EX_SEQUENCE);
						rexp->mLeft = exp;
						exp = rexp;
					}
					rexp->mRight = new Expression(nexp->mLocation, EX_SEQUENCE);
					rexp = rexp->mRight;
					rexp->mLeft = nexp;
				}
			}

			dec = dec->mNext;
		}
	}

	return exp;
}

Expression* Parser::ParseSimpleExpression(void)
{
	Declaration* dec;
	Expression* exp = nullptr, * rexp = nullptr;

	switch (mScanner->mToken)
	{
	case TK_INT:
	case TK_SHORT:
	case TK_LONG:
	case TK_FLOAT:
	case TK_CHAR:
	case TK_BOOL:
	case TK_VOID:
	case TK_UNSIGNED:
	case TK_SIGNED:
	case TK_CONST:
	case TK_VOLATILE:
	case TK_STRUCT:
	case TK_UNION:
	case TK_TYPEDEF:
	case TK_STATIC:
		exp = ParseDeclarationExpression();
		break;
	case TK_CHARACTER:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mCharMap[(unsigned char)mScanner->mTokenInteger];
		dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_INTEGER:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		if (dec->mInteger < 32768)
			dec->mBase = TheSignedIntTypeDeclaration;
		else
			dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_INTEGERU:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_INTEGERL:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		if (dec->mInteger < 0x80000000)
			dec->mBase = TheSignedLongTypeDeclaration;
		else
			dec->mBase = TheUnsignedLongTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_INTEGERUL:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		dec->mBase = TheUnsignedLongTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_NUMBER:
		dec = new Declaration(mScanner->mLocation, DT_CONST_FLOAT);
		dec->mBase = TheFloatTypeDeclaration;
		dec->mNumber = mScanner->mTokenNumber;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_STRING:
	{
		dec = new Declaration(mScanner->mLocation, DT_CONST_DATA);
		int	size = strlen(mScanner->mTokenString);
		dec->mSize = size + 1;
		dec->mVarIndex = -1;
		dec->mSection = mCodeSection;
		dec->mBase = new Declaration(mScanner->mLocation, DT_TYPE_ARRAY);
		dec->mBase->mSize = dec->mSize;
		dec->mBase->mBase = TheConstCharTypeDeclaration;
		dec->mBase->mFlags |= DTF_DEFINED;
		uint8* d = new uint8[size + 1];
		dec->mData = d;

		int i = 0;
		while (i < size)
		{
			d[i] = mCharMap[(uint8)mScanner->mTokenString[i]];
			i++;
		}
		d[size] = 0;

		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();

		// automatic string concatenation
		while (mScanner->mToken == TK_STRING)
		{
			int	s = strlen(mScanner->mTokenString);
			uint8* d = new uint8[size + s + 1];
			memcpy(d, dec->mData, size);
			int i = 0;
			while (i < s)
			{
				d[size + i] = mCharMap[(uint8)mScanner->mTokenString[i]];
				i++;
			}
			size += s;
			d[size] = 0;
			dec->mSize = size + 1;
			delete[] dec->mData;
			dec->mData = d;
			mScanner->NextToken();
		}
		break;
	}
	case TK_TRUE:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mBase = TheBoolTypeDeclaration;
		dec->mInteger = 1;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_FALSE:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mBase = TheBoolTypeDeclaration;
		dec->mInteger = 0;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_NULL:
		dec = new Declaration(mScanner->mLocation, DT_CONST_ADDRESS);
		dec->mBase = TheVoidPointerTypeDeclaration;
		dec->mInteger = 0;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_IDENT:
		dec = mScope->Lookup(mScanner->mTokenIdent);
		if (dec)
		{
			if (dec->mType == DT_CONST_INTEGER || dec->mType == DT_CONST_FLOAT || dec->mType == DT_CONST_FUNCTION || dec->mType == DT_CONST_ASSEMBLER || dec->mType == DT_LABEL || dec->mType == DT_LABEL_REF)
			{
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = dec;
				exp->mDecType = dec->mBase;
				exp->mConst = true;
				mScanner->NextToken();
			}
			else if (dec->mType == DT_VARIABLE || dec->mType == DT_ARGUMENT)
			{
				if ((dec->mFlags & DTF_STATIC) && (dec->mFlags & DTF_CONST) && dec->mValue && dec->mBase->IsNumericType())
				{
					exp = dec->mValue;
				}
				else
				{
					exp = new Expression(mScanner->mLocation, EX_VARIABLE);
					exp->mDecValue = dec;
					exp->mDecType = dec->mBase;
				}
				mScanner->NextToken();
			}
			else if (dec->mType <= DT_TYPE_FUNCTION)
			{
				exp = ParseDeclarationExpression();
			}
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_INVALID_IDENTIFIER, "Invalid identifier", mScanner->mTokenIdent->mString);
				mScanner->NextToken();
			}
		}
		else
		{
			mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown identifier", mScanner->mTokenIdent->mString);
			mScanner->NextToken();
		}

		break;
	case TK_SIZEOF:
		mScanner->NextToken();
		rexp = ParseParenthesisExpression();
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mBase = TheSignedIntTypeDeclaration;
		dec->mInteger = rexp->mDecType->mSize;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;
		break;

	case TK_OPEN_PARENTHESIS:
		mScanner->NextToken();
		exp = ParseExpression();
		if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");

		if (exp->mType == EX_TYPE)
		{
			Expression* nexp = new Expression(mScanner->mLocation, EX_TYPECAST);
			nexp->mDecType = exp->mDecType;
			nexp->mLeft = exp;
			nexp->mRight = ParsePrefixExpression();
			exp = nexp->ConstantFold(mErrors);
		}
		break;
	default:
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Term starts with invalid token", TokenNames[mScanner->mToken]);
		mScanner->NextToken();
	}

	if (!exp)
	{
		exp = new Expression(mScanner->mLocation, EX_VOID);
		exp->mDecType = TheVoidTypeDeclaration;
	}

	return exp;
}


Expression* Parser::ParsePostfixExpression(void)
{
	Expression* exp = ParseSimpleExpression();

	for (;;)
	{
		if (mScanner->mToken == TK_OPEN_BRACKET)
		{
			if (exp->mDecType->mType != DT_TYPE_ARRAY && exp->mDecType->mType != DT_TYPE_POINTER)
				mErrors->Error(mScanner->mLocation, EERR_INVALID_INDEX, "Array expected for indexing");
			mScanner->NextToken();
			Expression* nexp = new Expression(mScanner->mLocation, EX_INDEX);
			nexp->mLeft = exp;
			nexp->mRight = ParseExpression();
			if (mScanner->mToken == TK_CLOSE_BRACKET)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "']' expected");
			nexp->mDecType = exp->mDecType->mBase;
			if (!nexp->mDecType)
				nexp->mDecType = TheVoidTypeDeclaration;
			exp = nexp;
		}
		else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
		{
			if (exp->mDecType->mType == DT_TYPE_POINTER && exp->mDecType->mBase->mType == DT_TYPE_FUNCTION)
			{				
			}
			else if (exp->mDecType->mType == DT_TYPE_FUNCTION)
			{
			}
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Function expected for call");
				exp->mDecType = TheVoidFunctionTypeDeclaration;
			}

			mScanner->NextToken();
			Expression* nexp = new Expression(mScanner->mLocation, EX_CALL);
			nexp->mLeft = exp;
			nexp->mDecType = exp->mDecType->mBase;
			if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
			{
				nexp->mRight = ParseListExpression();
				ConsumeToken(TK_CLOSE_PARENTHESIS);
			}
			else
			{
				nexp->mRight = nullptr;
				mScanner->NextToken();
			}
			exp = nexp;
		}
		else if (mScanner->mToken == TK_INC || mScanner->mToken == TK_DEC)
		{
			Expression* nexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
			nexp->mToken = mScanner->mToken;
			nexp->mLeft = exp;
			nexp->mDecType = exp->mDecType;
			exp = nexp;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_ARROW)
		{
			mScanner->NextToken();
			if (exp->mDecType->mType == DT_TYPE_POINTER)
			{
				Expression * dexp = new Expression(mScanner->mLocation, EX_PREFIX);
				dexp->mToken = TK_MUL;
				dexp->mDecType = exp->mDecType->mBase;
				dexp->mLeft = exp;

				if (dexp->mDecType->mType == DT_TYPE_STRUCT || dexp->mDecType->mType == DT_TYPE_UNION)
				{
					Expression* nexp = new Expression(mScanner->mLocation, EX_QUALIFY);
					nexp->mLeft = dexp;
					if (mScanner->mToken == TK_IDENT)
					{
						Declaration* mdec = dexp->mDecType->mScope->Lookup(mScanner->mTokenIdent);
						if (mdec)
						{
							nexp->mDecValue = mdec;
							nexp->mDecType = mdec->mBase;
							exp = nexp;
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Struct member identifier not found", mScanner->mTokenIdent->mString);
						mScanner->NextToken();
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Struct member identifier expected");
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Struct expected");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Pointer expected");
		}
		else if (mScanner->mToken == TK_DOT)
		{
			mScanner->NextToken();
			if (exp->mDecType->mType == DT_TYPE_STRUCT || exp->mDecType->mType == DT_TYPE_UNION)
			{
				Expression* nexp = new Expression(mScanner->mLocation, EX_QUALIFY);
				nexp->mLeft = exp;
				if (mScanner->mToken == TK_IDENT)
				{
					Declaration* mdec = exp->mDecType->mScope->Lookup(mScanner->mTokenIdent);
					if (mdec)
					{
						nexp->mDecValue = mdec;
						nexp->mDecType = mdec->mBase;
						exp = nexp;
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Struct member identifier not found", mScanner->mTokenIdent->mString);
					mScanner->NextToken();
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Struct member identifier expected");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Struct expected");
		}
		else
			return exp;
	}
}


Expression* Parser::ParsePrefixExpression(void)
{
	if (mScanner->mToken == TK_SUB || mScanner->mToken == TK_BINARY_NOT || mScanner->mToken == TK_LOGICAL_NOT || mScanner->mToken == TK_MUL || mScanner->mToken == TK_INC || mScanner->mToken == TK_DEC || mScanner->mToken == TK_BINARY_AND)
	{
		Expression* nexp;
		if (mScanner->mToken == TK_LOGICAL_NOT)
		{
			mScanner->NextToken();
			nexp = ParsePrefixExpression();
			nexp = nexp->LogicInvertExpression();
		}
		else if (mScanner->mToken == TK_INC || mScanner->mToken == TK_DEC)
		{
			nexp = new Expression(mScanner->mLocation, EX_PREINCDEC);
			nexp->mToken = mScanner->mToken;
			mScanner->NextToken();
			nexp->mLeft = ParsePrefixExpression();;
			nexp->mDecType = nexp->mLeft->mDecType;
		}
		else
		{
			nexp = new Expression(mScanner->mLocation, EX_PREFIX);
			nexp->mToken = mScanner->mToken;
			mScanner->NextToken();
			nexp->mLeft = ParsePrefixExpression();
			if (nexp->mToken == TK_MUL)
			{
				if (nexp->mLeft->mDecType->mType == DT_TYPE_POINTER || nexp->mLeft->mDecType->mType == DT_TYPE_ARRAY)
				{
					// no pointer to function dereferencing
					if (nexp->mLeft->mDecType->mBase->mType == DT_TYPE_FUNCTION)
						return nexp->mLeft;
					nexp->mDecType = nexp->mLeft->mDecType->mBase;
				}
				else
				{
					mErrors->Error(nexp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Pointer or array type expected");
					nexp->mDecType = TheVoidTypeDeclaration;
				}
			}
			else if (nexp->mToken == TK_BINARY_AND)
			{
				Declaration* pdec = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
				pdec->mBase = nexp->mLeft->mDecType;
				pdec->mSize = 2;
				nexp->mDecType = pdec;
			}
			else
				nexp->mDecType = nexp->mLeft->mDecType;
		}
		return nexp->ConstantFold(mErrors);
	}
	else
		return ParsePostfixExpression();
}

Expression* Parser::ParseMulExpression(void)
{
	Expression* exp = ParsePrefixExpression();

	while (mScanner->mToken == TK_MUL || mScanner->mToken == TK_DIV || mScanner->mToken == TK_MOD)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParsePrefixExpression();

		if (nexp->mLeft->mDecType->mType == DT_TYPE_FLOAT || nexp->mRight->mDecType->mType == DT_TYPE_FLOAT)
			nexp->mDecType = TheFloatTypeDeclaration;
		else
			nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseAddExpression(void)
{
	Expression* exp = ParseMulExpression();

	while (mScanner->mToken == TK_ADD || mScanner->mToken == TK_SUB)
	{
		Expression * nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseMulExpression();
		if (nexp->mLeft->mDecType->mType == DT_TYPE_POINTER && nexp->mRight->mDecType->IsIntegerType())
			nexp->mDecType = nexp->mLeft->mDecType;
		else if (nexp->mRight->mDecType->mType == DT_TYPE_POINTER && nexp->mLeft->mDecType->IsIntegerType())
			nexp->mDecType = nexp->mRight->mDecType;
		else if (nexp->mLeft->mDecType->mType == DT_TYPE_ARRAY && nexp->mRight->mDecType->IsIntegerType())
		{
			Declaration* dec = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
			dec->mSize = 2;
			dec->mBase = nexp->mLeft->mDecType->mBase;
			nexp->mDecType = dec;
		}
		else if (nexp->mRight->mDecType->mType == DT_TYPE_ARRAY && nexp->mLeft->mDecType->IsIntegerType())
		{
			Declaration* dec = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
			dec->mSize = 2;
			dec->mBase = nexp->mRight->mDecType->mBase;
			nexp->mDecType = dec;
		}
		else if (nexp->mLeft->mDecType->mType == DT_TYPE_FLOAT || nexp->mRight->mDecType->mType == DT_TYPE_FLOAT)
			nexp->mDecType = TheFloatTypeDeclaration;
		else
			nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseShiftExpression(void)
{
	Expression* exp = ParseAddExpression();

	while (mScanner->mToken == TK_LEFT_SHIFT || mScanner->mToken == TK_RIGHT_SHIFT)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAddExpression();
		nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseRelationalExpression(void)
{
	Expression* exp = ParseShiftExpression();

	while (mScanner->mToken >= TK_EQUAL && mScanner->mToken <= TK_LESS_EQUAL)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_RELATIONAL);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseShiftExpression();
		nexp->mDecType = TheBoolTypeDeclaration;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryAndExpression(void)
{
	Expression* exp = ParseRelationalExpression();

	while (mScanner->mToken == TK_BINARY_AND)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseRelationalExpression();
		nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryXorExpression(void)
{
	Expression* exp = ParseBinaryAndExpression();

	while (mScanner->mToken == TK_BINARY_XOR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryAndExpression();
		nexp->mDecType = exp->mDecType;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryOrExpression(void)
{
	Expression* exp = ParseBinaryXorExpression();

	while (mScanner->mToken == TK_BINARY_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryXorExpression();
		nexp->mDecType = exp->mDecType;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseLogicAndExpression(void)
{
	Expression* exp = ParseBinaryOrExpression();

	while (mScanner->mToken == TK_LOGICAL_AND)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_LOGICAL_AND);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryOrExpression();
		nexp->mDecType = TheBoolTypeDeclaration;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseLogicOrExpression(void)
{
	Expression* exp = ParseLogicAndExpression();

	while (mScanner->mToken == TK_LOGICAL_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_LOGICAL_OR);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseLogicAndExpression();
		nexp->mDecType = TheBoolTypeDeclaration;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseConditionalExpression(void)
{
	Expression* exp = ParseLogicOrExpression();
	
	if (mScanner->mToken == TK_QUESTIONMARK)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_CONDITIONAL);
		nexp->mLeft = exp;
		mScanner->NextToken();
		Expression* texp = new Expression(mScanner->mLocation, EX_SEQUENCE);
		nexp->mRight = texp;

		texp->mLeft = ParseLogicOrExpression();
		ConsumeToken(TK_COLON);
		texp->mRight = ParseConditionalExpression();
		
		nexp->mDecType = texp->mLeft->mDecType;
		exp = nexp;
	}

	return exp;
}

Expression* Parser::ParseRExpression(void)
{
	return ParseConditionalExpression();
}

Expression* Parser::ParseParenthesisExpression(void)
{
	if (mScanner->mToken == TK_OPEN_PARENTHESIS)
		mScanner->NextToken();
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'(' expected");

	Expression* exp = ParseExpression();

	if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
		mScanner->NextToken();
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");

	return exp;
}

Expression* Parser::ParseAssignmentExpression(void)
{
	Expression* exp = ParseConditionalExpression();

	if (mScanner->mToken >= TK_ASSIGN && mScanner->mToken <= TK_ASSIGN_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAssignmentExpression();
		nexp->mDecType = exp->mDecType;
		exp = nexp;
		assert(exp->mDecType);
	}

	return exp;
}

Expression* Parser::ParseExpression(void)
{
	return ParseAssignmentExpression();
}

Expression* Parser::ParseListExpression(void)
{
	Expression* exp = ParseExpression();
	if (mScanner->mToken == TK_COMMA)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_LIST);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseListExpression();
		exp = nexp;
	}
	return exp;

}

Expression* Parser::ParseFunction(Declaration * dec)
{
	DeclarationScope* scope = new DeclarationScope(mScope);
	mScope = scope;

	Declaration* pdec = dec->mParams;
	while (pdec)
	{
		if (pdec->mIdent)
			scope->Insert(pdec->mIdent, pdec);
		pdec = pdec->mNext;
	}
	mLocalIndex = 0;

	Expression * exp = ParseStatement();

	mScope = mScope->mParent;

	return exp;
}

Expression* Parser::ParseStatement(void)
{
	Expression* exp = nullptr;

	if (mScanner->mToken == TK_OPEN_BRACE)
	{
		DeclarationScope* scope = new DeclarationScope(mScope);
		mScope = scope;

		mScanner->NextToken();
		if (mScanner->mToken != TK_CLOSE_BRACE)
		{
			Expression* pexp = nullptr;
			do
			{
				Expression* nexp = ParseStatement();
				if (exp)
				{
					if (!pexp)
					{
						pexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
						pexp->mLeft = exp;
						exp = pexp;
					}
					
					pexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
					pexp = pexp->mRight;
					pexp->mLeft = nexp;
				}
				else
					exp = nexp;

			} while (mScanner->mToken != TK_CLOSE_BRACE && mScanner->mToken != TK_EOF);
			if (mScanner->mToken != TK_CLOSE_BRACE)
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'}' expected");
			mScanner->NextToken();
		}
		else
		{
			exp = new Expression(mScanner->mLocation, EX_VOID);
			mScanner->NextToken();
		}

		mScope = mScope->mParent;
	}
	else
	{
		switch (mScanner->mToken)
		{
		case TK_IF:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_IF);		
			exp->mLeft = ParseParenthesisExpression();
			exp->mRight = new Expression(mScanner->mLocation, EX_ELSE);
			exp->mRight->mLeft = ParseStatement();
			if (mScanner->mToken == TK_ELSE)
			{
				mScanner->NextToken();
				exp->mRight->mRight = ParseStatement();
			}
			else
				exp->mRight->mRight = nullptr;
			break;
		case TK_WHILE:
		{
			mScanner->NextToken();

			DeclarationScope* scope = new DeclarationScope(mScope);
			mScope = scope;

			exp = new Expression(mScanner->mLocation, EX_WHILE);
			exp->mLeft = ParseParenthesisExpression();
			exp->mRight = ParseStatement();

			mScope = mScope->mParent;
		}
			break;
		case TK_DO:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_DO);
			exp->mRight = ParseStatement();
			if (mScanner->mToken == TK_WHILE)
			{
				mScanner->NextToken();
				exp->mLeft = ParseParenthesisExpression();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'while' expected");
			break;
		case TK_FOR:
			mScanner->NextToken();
			if (mScanner->mToken == TK_OPEN_PARENTHESIS)
			{
				DeclarationScope* scope = new DeclarationScope(mScope);
				mScope = scope;

				mScanner->NextToken();
				exp = new Expression(mScanner->mLocation, EX_FOR);
				exp->mLeft = new Expression(mScanner->mLocation, EX_SEQUENCE);
				exp->mLeft->mLeft = new Expression(mScanner->mLocation, EX_SEQUENCE);

				// Assignment
				if (mScanner->mToken != TK_SEMICOLON)
					exp->mLeft->mRight = ParseExpression();
				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "';' expected");

				// Condition
				if (mScanner->mToken != TK_SEMICOLON)
					exp->mLeft->mLeft->mLeft = ParseExpression();
				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "';' expected");

				// Iteration
				if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
					exp->mLeft->mLeft->mRight = ParseExpression();
				if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
				exp->mRight = ParseStatement();

				mScope = mScope->mParent;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'(' expected");
			break;
		case TK_SWITCH:
			mScanner->NextToken();
			exp = ParseSwitchStatement();
			break;
		case TK_RETURN:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_RETURN);
			if (mScanner->mToken != TK_SEMICOLON)
				exp->mLeft = ParseRExpression();
			break;
		case TK_BREAK:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_BREAK);
			break;
		case TK_CONTINUE:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_CONTINUE);
			break;
		case TK_SEMICOLON:
			exp = new Expression(mScanner->mLocation, EX_VOID);
			break;
		case TK_ASM:
			mScanner->NextToken();
			if (mScanner->mToken == TK_OPEN_BRACE)
			{
				mScanner->NextToken();
				exp = ParseAssembler();
				if (mScanner->mToken == TK_CLOSE_BRACE)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'}' expected");
			}
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'{' expected");
				exp = new Expression(mScanner->mLocation, EX_VOID);
			}
			break;
		case TK_ASSUME:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_ASSUME);
			exp->mLeft = ParseParenthesisExpression();
			break;


		default:
			exp = ParseExpression();
		}
		if (mScanner->mToken == TK_SEMICOLON)
			mScanner->NextToken();
	}

	assert(exp);

	return exp;
}

Expression* Parser::ParseSwitchStatement(void)
{
	Expression* sexp = new Expression(mScanner->mLocation, EX_SWITCH);

	if (mScanner->mToken == TK_OPEN_PARENTHESIS)
	{
		mScanner->NextToken();
		sexp->mLeft = ParseRExpression();
		if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
	}
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'(' expected");

	if (mScanner->mToken == TK_OPEN_BRACE)
	{
		mScanner->NextToken();

		if (mScanner->mToken != TK_CLOSE_BRACE)
		{
			Expression* pcsexp = nullptr;

			Expression* cexp = nullptr;

			Expression* pexp = nullptr;
			do
			{
				if (mScanner->mToken == TK_CASE)
				{
					mScanner->NextToken();
					cexp = new Expression(mScanner->mLocation, EX_CASE);
					pexp = cexp;
					cexp->mLeft = ParseRExpression();

					if (mScanner->mToken == TK_COLON)
						mScanner->NextToken();
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "':' expected");

					Expression* csexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
					csexp->mLeft = cexp;
					if (pcsexp)
						pcsexp->mRight = csexp;
					else
						sexp->mRight = csexp;
					pcsexp = csexp;

				}
				else if (mScanner->mToken == TK_DEFAULT)
				{
					mScanner->NextToken();
					cexp = new Expression(mScanner->mLocation, EX_DEFAULT);
					pexp = cexp;

					if (mScanner->mToken == TK_COLON)
						mScanner->NextToken();
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "':' expected");

					Expression * csexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
					csexp->mLeft = cexp;					
					if (pcsexp)
						pcsexp->mRight = csexp;
					else
						sexp->mRight = csexp;
					pcsexp = csexp;					
				}
				else
				{
					Expression* nexp = ParseStatement();
					if (cexp)
					{
						Expression* sexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
						sexp->mLeft = nexp;
						pexp->mRight = sexp;
						pexp = sexp;
					}
				}

			} while (mScanner->mToken != TK_CLOSE_BRACE && mScanner->mToken != TK_EOF);
		}

		if (mScanner->mToken == TK_CLOSE_BRACE)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'}' expected");
	}
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'{' expected");

	return sexp;
}



Expression* Parser::ParseAssemblerBaseOperand(Declaration* pcasm, int pcoffset)
{
	Expression* exp = nullptr;
	Declaration* dec;

	switch (mScanner->mToken)
	{
	case TK_SUB:
		mScanner->NextToken();
		exp = ParseAssemblerBaseOperand(pcasm, pcoffset);
		if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
		{
			dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
			dec->mInteger = - exp->mDecValue->mInteger;
			if (dec->mInteger < 32768)
				dec->mBase = TheSignedIntTypeDeclaration;
			else
				dec->mBase = TheUnsignedIntTypeDeclaration;
			exp = new Expression(mScanner->mLocation, EX_CONSTANT);
			exp->mDecValue = dec;
			exp->mDecType = dec->mBase;
		}
		else
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Cannot negate expression");
		break;

	case TK_MUL:
		mScanner->NextToken();
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		dec = new Declaration(mScanner->mLocation, DT_LABEL);
		exp->mDecType = TheUnsignedIntTypeDeclaration;
		exp->mDecValue = dec;
		dec->mInteger = pcoffset;
		dec->mBase = pcasm;
		break;

	case TK_INTEGER:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		if (dec->mInteger < 32768)
			dec->mBase = TheSignedIntTypeDeclaration;
		else
			dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_INTEGERU:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mScanner->mTokenInteger;
		dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;
	case TK_CHARACTER:
		dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		dec->mInteger = mCharMap[(unsigned char)mScanner->mTokenInteger];
		dec->mBase = TheUnsignedIntTypeDeclaration;
		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();
		break;

	case TK_IDENT:
		dec = mScope->Lookup(mScanner->mTokenIdent);
		if (!dec)
		{ 
			exp = new Expression(mScanner->mLocation, EX_CONSTANT);
			dec = new Declaration(mScanner->mLocation, DT_LABEL);
			dec->mIdent = mScanner->mTokenIdent;
			exp->mDecType = TheUnsignedIntTypeDeclaration;
			mScope->Insert(dec->mIdent, dec);
		}
		else
		{
			exp = new Expression(mScanner->mLocation, EX_CONSTANT);
			if (dec->mType == DT_ARGUMENT)
			{
				exp->mDecType = TheUnsignedIntTypeDeclaration;
			}
			else if (dec->mType == DT_CONST_ASSEMBLER)
			{
				exp->mDecType = dec->mBase;
			}
			else
				exp->mDecType = TheUnsignedIntTypeDeclaration;
		}

		exp->mDecValue = dec;

		mScanner->NextToken();

		while (ConsumeTokenIf(TK_DOT))
		{
			if (mScanner->mToken == TK_IDENT)
			{
				if (exp->mDecValue->mType == DT_CONST_ASSEMBLER)
				{
					Declaration* ldec = exp->mDecValue->mBase->mScope->Lookup(mScanner->mTokenIdent);
					if (ldec)
					{
						exp->mDecValue = ldec;
						exp->mDecType = TheUnsignedIntTypeDeclaration;
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Assembler label not found", mScanner->mTokenIdent->mString);
				}
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Identifier for qualification expected");
		}

		if (exp->mDecValue->mType == DT_CONST_ASSEMBLER)
		{
			Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL);
			ndec->mIdent = exp->mDecValue->mIdent;
			ndec->mBase = exp->mDecValue;
			ndec->mInteger = 0;
			exp->mDecValue = ndec;
			exp->mDecType = TheUnsignedIntTypeDeclaration;
		}

		break;
	default:
		mErrors->Error(mScanner->mLocation, EERR_ASM_INVALD_OPERAND, "Invalid assembler operand");
	}

	if (!exp)
	{
		exp = new Expression(mScanner->mLocation, EX_VOID);
		exp->mDecType = TheVoidTypeDeclaration;
		exp->mDecValue = TheConstVoidValueDeclaration;
	}

	return exp;
}

Expression* Parser::ParseAssemblerMulOperand(Declaration* pcasm, int pcoffset)
{
	Expression* exp = ParseAssemblerBaseOperand(pcasm, pcoffset);
	while (mScanner->mToken == TK_MUL || mScanner->mToken == TK_DIV || mScanner->mToken == TK_MOD)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAssemblerBaseOperand(pcasm, pcoffset);
		exp = nexp->ConstantFold(mErrors);
	}
	return exp;
}

Expression* Parser::ParseAssemblerAddOperand(Declaration* pcasm, int pcoffset)
{
	Expression* exp = ParseAssemblerMulOperand(pcasm, pcoffset);
	while (mScanner->mToken == TK_ADD || mScanner->mToken == TK_SUB)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAssemblerMulOperand(pcasm, pcoffset);
		if (!nexp->mLeft->mDecValue || !nexp->mRight->mDecValue)
		{
			mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Invalid assembler operand");
		}
		else if (nexp->mLeft->mDecValue->mType == DT_VARIABLE || nexp->mLeft->mDecValue->mType == DT_ARGUMENT)
		{
			if (nexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = nexp->mLeft->mDecValue;
				ndec->mOffset = nexp->mToken == TK_SUB ? -int(nexp->mRight->mDecValue->mInteger) : int(nexp->mRight->mDecValue->mInteger);
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Integer offset expected");
		}
		else if (nexp->mLeft->mDecValue->mType == DT_CONST_FUNCTION)
		{
			if (nexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_FUNCTION_REF);
				ndec->mBase = nexp->mLeft->mDecValue;
				ndec->mOffset = nexp->mToken == TK_SUB ? -int(nexp->mRight->mDecValue->mInteger) : int(nexp->mRight->mDecValue->mInteger);
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Integer offset expected");
		}
		else if (nexp->mLeft->mDecValue->mType == DT_LABEL)
		{
			if (nexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = nexp->mLeft->mDecValue;
				ndec->mOffset = nexp->mToken == TK_SUB ? -int(nexp->mRight->mDecValue->mInteger) : int(nexp->mRight->mDecValue->mInteger);
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Integer offset expected");
		}
		else
			exp = nexp->ConstantFold(mErrors);
	}
	return exp;
}

Expression* Parser::ParseAssemblerOperand(Declaration* pcasm, int pcoffset)
{
	if (mScanner->mToken == TK_LESS_THAN)
	{
		mScanner->NextToken();
		Expression* exp = ParseAssemblerOperand(pcasm, pcoffset);

		if (exp->mType == EX_CONSTANT)
		{
			if (exp->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
				dec->mInteger = exp->mDecValue->mInteger & 0xff;
				dec->mBase = TheUnsignedIntTypeDeclaration;
				exp->mDecValue = dec;			
			}
			else if (exp->mDecValue->mType == DT_LABEL)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_LABEL_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_VARIABLE)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_VARIABLE_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_CONST_FUNCTION)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_FUNCTION_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_FUNCTION_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_FUNCTION_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_LOWER_BYTE;
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Label or integer value for lower byte operator expected");
		}
		else
			mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Constant for lower byte operator expected");

		return exp;
	}
	else if (mScanner->mToken == TK_GREATER_THAN)
	{
		mScanner->NextToken();
		Expression* exp = ParseAssemblerOperand(pcasm, pcoffset);

		if (exp->mType == EX_CONSTANT)
		{
			if (exp->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
				dec->mInteger = (exp->mDecValue->mInteger >> 8) & 0xff;
				dec->mBase = TheUnsignedIntTypeDeclaration;
				exp->mDecValue = dec;
			}
			else if (exp->mDecValue->mType == DT_LABEL)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_LABEL_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_VARIABLE)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_VARIABLE_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_CONST_FUNCTION)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_FUNCTION_REF);
				ndec->mBase = exp->mDecValue;
				ndec->mOffset = 0;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else if (exp->mDecValue->mType == DT_FUNCTION_REF)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_FUNCTION_REF);
				ndec->mBase = exp->mDecValue->mBase;
				ndec->mOffset = exp->mDecValue->mOffset;
				ndec->mFlags |= DTF_UPPER_BYTE;
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Label or integer value for lower byte operator expected");
		}
		else
			mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Constant for upper byte operator expected");

		return exp;
	}
	else
		return ParseAssemblerAddOperand(pcasm, pcoffset);
}

void Parser::AddAssemblerRegister(const Ident* ident, int value)
{
	Declaration* decaccu = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
	decaccu->mIdent = ident;
	decaccu->mBase = TheUnsignedIntTypeDeclaration;
	decaccu->mSize = 2;
	decaccu->mInteger = value;
	mScope->Insert(decaccu->mIdent, decaccu);
}

Expression* Parser::ParseAssembler(void)
{
	DeclarationScope* scope = new DeclarationScope(mScope);
	mScope = scope;

	mScanner->SetAssemblerMode(true);

	AddAssemblerRegister(Ident::Unique("__tmpy"), BC_REG_WORK_Y);
	AddAssemblerRegister(Ident::Unique("__tmp"), BC_REG_WORK);
	AddAssemblerRegister(Ident::Unique("__ip"), BC_REG_IP);
	AddAssemblerRegister(Ident::Unique("__accu"), BC_REG_ACCU);
	AddAssemblerRegister(Ident::Unique("__addr"), BC_REG_ADDR);
	AddAssemblerRegister(Ident::Unique("__fp"), BC_REG_LOCALS);
	AddAssemblerRegister(Ident::Unique("__sp"), BC_REG_STACK);
	AddAssemblerRegister(Ident::Unique("__sregs"), BC_REG_TMP_SAVED);
	AddAssemblerRegister(Ident::Unique("__regs"), BC_REG_TMP);

	AddAssemblerRegister(Ident::Unique("accu"), BC_REG_ACCU);

	Declaration* decaccu = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
	decaccu->mIdent = Ident::Unique("accu");
	decaccu->mBase = TheUnsignedIntTypeDeclaration;
	decaccu->mSize = 2;
	decaccu->mInteger = BC_REG_ACCU;
	mScope->Insert(decaccu->mIdent, decaccu);

	Declaration* dassm = new Declaration(mScanner->mLocation, DT_TYPE_ASSEMBLER);
	dassm->mScope = scope;

	Declaration* vdasm = new Declaration(mScanner->mLocation, DT_CONST_ASSEMBLER);
	vdasm->mVarIndex = -1;
	vdasm->mSection = mCodeSection;

	vdasm->mBase = dassm;

	Expression* ifirst = new Expression(mScanner->mLocation, EX_ASSEMBLER);
	Expression* ilast = ifirst, * ifinal = ifirst;

	bool		exitLabel = false;

	ifirst->mDecType = dassm;
	ifirst->mDecValue = vdasm;
	vdasm->mValue = ifirst;

	int		offset = 0;

	while (mScanner->mToken != TK_CLOSE_BRACE && mScanner->mToken != TK_EOF)
	{
		if (mScanner->mToken == TK_IDENT)
		{
			AsmInsType	ins = FindAsmInstruction(mScanner->mTokenIdent->mString);
			if (ins == ASMIT_INV)
			{
				const Ident* label = mScanner->mTokenIdent;
				mScanner->NextToken();
				if (mScanner->mToken != TK_COLON)
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "':' expected");
				else
					mScanner->NextToken();

				exitLabel = true;

				Declaration* dec = mScope->Lookup(label);
				if (dec)
				{
					if (dec->mType != DT_LABEL || dec->mBase)
						mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate label definition");
				}
				else
					dec = new Declaration(mScanner->mLocation, DT_LABEL);

				dec->mIdent = label;
				dec->mValue = ilast;
				dec->mInteger = offset;
				dec->mBase = vdasm;
				mScope->Insert(dec->mIdent, dec);
			}
			else
			{
				exitLabel = false;

				ilast->mAsmInsType = ins;
				mScanner->NextToken();
				if (mScanner->mToken == TK_EOL || mScanner->mToken == TK_CLOSE_BRACE)
					ilast->mAsmInsMode = ASMIM_IMPLIED;
				else if (mScanner->mToken == TK_HASH)
				{
					ilast->mAsmInsMode = ASMIM_IMMEDIATE;
					mScanner->NextToken();
					ilast->mLeft = ParseAssemblerOperand(vdasm, offset);
				}
				else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
				{
					mScanner->NextToken();
					ilast->mLeft = ParseAssemblerOperand(vdasm, offset);
					if (mScanner->mToken == TK_COMMA)
					{
						mScanner->NextToken();
						if (mScanner->mToken == TK_IDENT && (!strcmp(mScanner->mTokenIdent->mString, "x") || !strcmp(mScanner->mTokenIdent->mString, "X")))
						{
							ilast->mAsmInsMode = ASMIM_INDIRECT_X;
							mScanner->NextToken();
							if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
								mScanner->NextToken();
							else
								mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "',x' expected");
					}
					else if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
					{
						mScanner->NextToken();
						if (mScanner->mToken == TK_COMMA)
						{
							mScanner->NextToken();
							if (mScanner->mToken == TK_IDENT && (!strcmp(mScanner->mTokenIdent->mString, "y") || !strcmp(mScanner->mTokenIdent->mString, "Y")))
							{
								ilast->mAsmInsMode = ASMIM_INDIRECT_Y;
								mScanner->NextToken();
							}
							else
								mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "',y' expected");
						}
						else
						{
							ilast->mAsmInsMode = ASMIM_INDIRECT;
						}
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "',' or ')' expected");
				}
				else
				{
					ilast->mLeft = ParseAssemblerOperand(vdasm, offset);
					if (mScanner->mToken == TK_COMMA)
					{
						mScanner->NextToken();
						if (mScanner->mToken == TK_IDENT && (!strcmp(mScanner->mTokenIdent->mString, "x") || !strcmp(mScanner->mTokenIdent->mString, "X")))
						{
							ilast->mAsmInsMode = ASMIM_ABSOLUTE_X;
							mScanner->NextToken();
						}
						else if (mScanner->mToken == TK_IDENT && (!strcmp(mScanner->mTokenIdent->mString, "y") || !strcmp(mScanner->mTokenIdent->mString, "Y")))
						{
							ilast->mAsmInsMode = ASMIM_ABSOLUTE_Y;
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "',x' or ',y' expected");
					}
					else
					{
						if (HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_RELATIVE))
							ilast->mAsmInsMode = ASMIM_RELATIVE;
						else
							ilast->mAsmInsMode = ASMIM_ABSOLUTE;
					}
				}

				if (ilast->mLeft && ilast->mLeft->mDecValue)
				{
					if ((ilast->mLeft->mDecValue->mType == DT_CONST_INTEGER && ilast->mLeft->mDecValue->mInteger < 256) ||
						(ilast->mLeft->mDecValue->mType == DT_VARIABLE_REF && !(ilast->mLeft->mDecValue->mBase->mFlags & DTF_GLOBAL)) ||
						(ilast->mLeft->mDecValue->mType == DT_VARIABLE && !(ilast->mLeft->mDecValue->mFlags & DTF_GLOBAL)) ||
						 ilast->mLeft->mDecValue->mType == DT_ARGUMENT)
					{
						if (ilast->mAsmInsMode == ASMIM_ABSOLUTE && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE))
							ilast->mAsmInsMode = ASMIM_ZERO_PAGE;
						else if (ilast->mAsmInsMode == ASMIM_ABSOLUTE_X && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE_X))
							ilast->mAsmInsMode = ASMIM_ZERO_PAGE_X;
						else if (ilast->mAsmInsMode == ASMIM_ABSOLUTE_Y && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE_Y))
							ilast->mAsmInsMode = ASMIM_ZERO_PAGE_Y;
					}
				}

				if (ilast->mAsmInsType == ASMIT_BYTE)
					ilast->mAsmInsMode = ASMIM_IMMEDIATE;

				if (mScanner->mToken != TK_EOL && mScanner->mToken != TK_CLOSE_BRACE)
				{
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "End of line expected");
				}

				while (mScanner->mToken != TK_EOL && mScanner->mToken != TK_EOF && mScanner->mToken != TK_CLOSE_BRACE)
					mScanner->NextToken();

				offset += AsmInsSize(ilast->mAsmInsType, ilast->mAsmInsMode);

				ifinal = ilast;

				ilast->mRight = new Expression(mScanner->mLocation, EX_ASSEMBLER);
				ilast = ilast->mRight;
			}
		}
		else if (mScanner->mToken == TK_EOL)
		{
			mScanner->NextToken();
		}
		else
		{
			mErrors->Error(mScanner->mLocation, EERR_ASM_INVALID_INSTRUCTION, "Invalid assembler token");

			while (mScanner->mToken != TK_EOL && mScanner->mToken != TK_EOF)
				mScanner->NextToken();
		 }
	}

	if ((ifinal->mAsmInsType == ASMIT_RTS || ifinal->mAsmInsType == ASMIT_JMP) && !exitLabel)
	{
		delete ilast;
		ilast = ifinal;
		ifinal->mRight = nullptr;
	}
	else
	{
		ilast->mAsmInsType = ASMIT_RTS;
		ilast->mAsmInsMode = ASMIM_IMPLIED;
	}

#if 0
	offset = 0;
	ilast = ifirst;
	while (ilast)
	{
		if (ilast->mLeft && ilast->mLeft->mType == EX_UNDEFINED)
		{
			Declaration* dec = mScope->Lookup(ilast->mLeft->mDecValue->mIdent);
			if (dec)
			{
				ilast->mLeft->mType = EX_CONSTANT;
				ilast->mLeft->mDecValue = dec;
			}
			else
				mErrors->Error(ilast->mLeft->mLocation, "Undefined label", ilast->mLeft->mDecValue->mIdent->mString);
		}
		offset += AsmInsSize(ilast->mAsmInsType, ilast->mAsmInsMode);

		ilast = ilast->mRight;
	}
#endif

	mScope = mScope->mParent;
	dassm->mSize = offset;
	dassm->mScope->mParent = nullptr;

	mScanner->SetAssemblerMode(false);

	return ifirst;
}

bool Parser::ConsumeToken(Token token)
{
	if (mScanner->mToken == token)
	{
		mScanner->NextToken();
		return true;
	}
	else
	{
		char	buffer[100];
		sprintf_s(buffer, "%s expected", TokenNames[token]);
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, buffer);
		return false;
	}
}

bool Parser::ConsumeTokenIf(Token token)
{
	if (mScanner->mToken == token)
	{
		mScanner->NextToken();
		return true;
	}
	else
		return false;

}

void Parser::ParsePragma(void)
{
	if (mScanner->mToken == TK_IDENT)
	{
		if (!strcmp(mScanner->mTokenIdent->mString, "message"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_STRING)
			{
				printf("%s\n", mScanner->mTokenString);
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "compile"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_STRING)
			{
				mCompilationUnits->AddUnit(mScanner->mLocation, mScanner->mTokenString, mScanner->mLocation.mFileName);
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "intrinsic"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_FUNCTION)
					dec->mFlags |= DTF_INTRINSIC;
				else
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Intrinsic function not found");
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "native"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (!dec)
					dec = mScope->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_FUNCTION)
					dec->mFlags |= DTF_NATIVE;
				else
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Native function not found");
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "startup"))
		{
			if (mCompilationUnits->mStartup)
				mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate startup pragma");

			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_ASSEMBLER)
					mCompilationUnits->mStartup = dec;
				else
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Startup function not found");
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "register"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* reg = mScanner->mTokenIdent;
				int	index = 0;
				mScanner->NextToken();

				ConsumeToken(TK_COMMA);
				Expression* exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER && exp->mDecValue->mInteger >= 2 && exp->mDecValue->mInteger < 0x100)
					index = exp->mDecValue->mInteger;
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for start expected");

				if (!strcmp(reg->mString, "__tmpy"))
					BC_REG_WORK_Y = index;
				else if (!strcmp(reg->mString, "__tmp"))
					BC_REG_WORK = index;
				else if (!strcmp(reg->mString, "__ip"))
					BC_REG_IP = index;
				else if (!strcmp(reg->mString, "__accu"))
					BC_REG_ACCU = index;
				else if (!strcmp(reg->mString, "__addr"))
					BC_REG_ADDR = index;
				else if (!strcmp(reg->mString, "__sp"))
					BC_REG_STACK = index;
				else if (!strcmp(reg->mString, "__fp"))
					BC_REG_LOCALS = index;
				else if (!strcmp(reg->mString, "__sregs"))
					BC_REG_TMP_SAVED = index;
				else if (!strcmp(reg->mString, "__regs"))
					BC_REG_TMP = index;
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Unknown register name");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Register name expected");
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "bytecode"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			Expression* exp = ParseAssemblerOperand(nullptr, 0);

			ConsumeToken(TK_COMMA);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_ASSEMBLER)
				{
					mScanner->NextToken();
					if (ConsumeTokenIf(TK_DOT))
					{
						if (mScanner->mToken == TK_IDENT)
						{
							Declaration* ndec = dec->mBase->mScope->Lookup(mScanner->mTokenIdent);
							if (ndec)
								dec = ndec;
							else
								mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Label not found in assembler code");
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");
					}

					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER && exp->mDecValue->mInteger >= 0 && exp->mDecValue->mInteger < 256)
					{
						if (mCompilationUnits->mByteCodes[exp->mDecValue->mInteger])
							mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate bytecode function");

						mCompilationUnits->mByteCodes[exp->mDecValue->mInteger] = dec;
					}
					else
						mErrors->Error(exp->mLocation, EERR_CONSTANT_TYPE, "Numeric value for byte code expected");
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Bytecode function not found");
					mScanner->NextToken();
				}

			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "runtime"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			const Ident* rtident = nullptr;

			if (mScanner->mToken == TK_IDENT)
			{
				rtident = mScanner->mTokenIdent;
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");

			ConsumeToken(TK_COMMA);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_ASSEMBLER)
				{
					mScanner->NextToken();
					if (ConsumeTokenIf(TK_DOT))
					{
						if (mScanner->mToken == TK_IDENT)
						{
							Declaration* ndec = dec->mBase->mScope->Lookup(mScanner->mTokenIdent);
							if (ndec)
								dec = ndec;
							else
								mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Label not found in assembler code");
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");
					}

					if (rtident)
						mCompilationUnits->mRuntimeScope->Insert(rtident, dec);
				}
				else if (dec && dec->mType == DT_VARIABLE && (dec->mFlags & DTF_GLOBAL))
				{
					if (rtident)
						mCompilationUnits->mRuntimeScope->Insert(rtident, dec);
					mScanner->NextToken();
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Runtime function not found");
					mScanner->NextToken();
				}

			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "stacksize"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_INTEGER)
			{
				mCompilationUnits->mSectionStack->mSize = mScanner->mTokenInteger;
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Stack size expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "charmap"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_INTEGER)
			{
				int	cindex = mScanner->mTokenInteger;
				mScanner->NextToken();
				ConsumeToken(TK_COMMA);

				if (mScanner->mToken == TK_INTEGER)
				{
					int	ccode = mScanner->mTokenInteger;
					int	ccount = 1;
					mScanner->NextToken();
					if (ConsumeTokenIf(TK_COMMA))
					{
						if (mScanner->mToken == TK_INTEGER)
						{
							ccount = mScanner->mTokenInteger;
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Character run expected");
					}

					for (int i = 0; i < ccount; i++)
					{
						mCharMap[cindex] = ccode;
						cindex = (cindex + 1) & 255;
						ccode = (ccode + 1) & 255;
					}
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Character code expected");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Character index expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "region"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* regionIdent = mScanner->mTokenIdent;
				mScanner->NextToken();

				Expression* exp;
				int	start = 0, end = 0, flags = 0, bank = -1;

				ConsumeToken(TK_COMMA);
				
				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					start = exp->mDecValue->mInteger;
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for start expected");
				
				ConsumeToken(TK_COMMA);
				
				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					end = exp->mDecValue->mInteger;
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for end expected");
				
				ConsumeToken(TK_COMMA);

				if (mScanner->mToken != TK_COMMA)
				{
					exp = ParseRExpression();
					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
						flags = exp->mDecValue->mInteger;
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for flags expected");
				}

				ConsumeToken(TK_COMMA);

				if (mScanner->mToken != TK_COMMA)
				{
					exp = ParseRExpression();
					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
						bank = exp->mDecValue->mInteger;
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");
				}

				LinkerRegion* rgn = mCompilationUnits->mLinker->FindRegion(regionIdent);
				if (!rgn)
					rgn = mCompilationUnits->mLinker->AddRegion(regionIdent, start, end);
				else if (rgn->mStart != start || rgn->mEnd != end)
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Conflicting linker region definition");

				rgn->mFlags = flags;
				rgn->mCartridge = bank;

				ConsumeToken(TK_COMMA);
				ConsumeToken(TK_OPEN_BRACE);
				if (!ConsumeTokenIf(TK_CLOSE_BRACE))
				{
					do {
						if (mScanner->mToken == TK_IDENT)
						{
							LinkerSection* lsec = mCompilationUnits->mLinker->FindSection(mScanner->mTokenIdent);
							if (lsec)
							{
								rgn->mSections.Push(lsec);
							}
							else
								mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section name not defined");
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section name expected");
					} while (ConsumeTokenIf(TK_COMMA));
					ConsumeToken(TK_CLOSE_BRACE);
				}

				if (ConsumeTokenIf(TK_COMMA))
				{
					exp = ParseRExpression();
					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
						rgn->mReloc = exp->mDecValue->mInteger - rgn->mStart;
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Region name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "section"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* sectionIdent = mScanner->mTokenIdent;
				mScanner->NextToken();

				int	flags = 0;
				Expression* exp;
				Declaration* dstart = nullptr, * dend = nullptr;

				ConsumeToken(TK_COMMA);

				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					flags = exp->mDecValue->mInteger;
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for flags expected");

				if (ConsumeTokenIf(TK_COMMA))
				{
					exp = ParseExpression();
					if (exp->mDecValue && exp->mDecValue->mType == DT_VARIABLE)
						dstart = exp->mDecValue;

					if (ConsumeTokenIf(TK_COMMA))
					{
						exp = ParseExpression();
						if (exp->mDecValue && exp->mDecValue->mType == DT_VARIABLE)
							dend = exp->mDecValue;
					}
				}

				LinkerSection* lsec = mCompilationUnits->mLinker->FindSection(sectionIdent);
				if (!lsec)
					lsec = mCompilationUnits->mLinker->AddSection(sectionIdent, LST_DATA);

				if (dstart)
				{
					dstart->mSection = lsec;
					dstart->mFlags |= DTF_SECTION_START;
				}
				if (dend)
				{
					dend->mSection = lsec;
					dend->mFlags |= DTF_SECTION_END;
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "code"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* sectionIdent = mScanner->mTokenIdent;
				mScanner->NextToken();
				LinkerSection* lsec = mCompilationUnits->mLinker->FindSection(sectionIdent);
				if (lsec)
				{
					mCodeSection = lsec;
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section not defined");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "data"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* sectionIdent = mScanner->mTokenIdent;
				mScanner->NextToken();
				LinkerSection* lsec = mCompilationUnits->mLinker->FindSection(sectionIdent);
				if (lsec)
				{
					mDataSection = lsec;
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section not defined");
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Section name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "align"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_VARIABLE && (dec->mFlags & DTF_GLOBAL))
				{
					mScanner->NextToken();
					ConsumeToken(TK_COMMA);

					Expression * exp = ParseRExpression();
					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
						dec->mAlignment = exp->mDecValue->mInteger;
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for alignment expected");
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Variable not found");
					mScanner->NextToken();
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Variable name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "reference"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && (dec->mType == DT_CONST_FUNCTION || (dec->mType == DT_VARIABLE && (dec->mFlags & DTF_GLOBAL))))
				{
					mCompilationUnits->AddReferenced(dec);
					mScanner->NextToken();
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Variable not found");
					mScanner->NextToken();
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Variable name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else
		{
			mScanner->NextToken();
			if (ConsumeTokenIf(TK_OPEN_PARENTHESIS))
			{
				while (mScanner->mToken != TK_CLOSE_PARENTHESIS && mScanner->mToken != TK_EOF)
					mScanner->NextToken();
				ConsumeToken(TK_CLOSE_PARENTHESIS);
			}
			mErrors->Error(mScanner->mLocation, EWARN_UNKNOWN_PRAGMA, "Unknown pragma, ignored", mScanner->mTokenIdent->mString);
		}
	}
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Invalid pragma directive");
}

void Parser::Parse(void)
{
	mLocalIndex = 0;

	while (mScanner->mToken != TK_EOF)
	{
		if (mScanner->mToken == TK_PREP_PRAGMA)
		{
			mScanner->NextToken();
			ParsePragma();
		}
		else if (mScanner->mToken == TK_ASM)
		{
			mScanner->NextToken();
			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* ident = mScanner->mTokenIdent;
				mScanner->NextToken();

				if (mScanner->mToken == TK_OPEN_BRACE)
				{
					mScanner->NextToken();

					Expression* exp = ParseAssembler();

					exp->mDecValue->mIdent = ident;
					mScope->Insert(ident, exp->mDecValue);

					if (mScanner->mToken == TK_CLOSE_BRACE)
						mScanner->NextToken();
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'}' expected");
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");
		}
		else if (mScanner->mToken == TK_SEMICOLON)
			mScanner->NextToken();
		else
			ParseDeclaration(true);
	}
}

#include "Parser.h"
#include <string.h>
#include "Assembler.h"
#include "MachineTypes.h"

Parser::Parser(Errors* errors, Scanner* scanner, CompilationUnits* compilationUnits)
	: mErrors(errors), mScanner(scanner), mCompilationUnits(compilationUnits)
{
	mGlobals = new DeclarationScope(compilationUnits->mScope, SLEVEL_STATIC);
	mScope = mGlobals;

	mCodeSection = compilationUnits->mSectionCode;
	mDataSection = compilationUnits->mSectionData;
	mBSSection = compilationUnits->mSectionBSS;

	mUnrollLoop = 0;
	mUnrollLoopPage = false;
	mInlineCall = false;
	mCompilerOptionSP = 0;
	mThisPointer = nullptr;

	for (int i = 0; i < 256; i++)
		mCharMap[i] = i;
}

Parser::~Parser(void)
{

}

void Parser::AddMemberFunction(Declaration* dec, Declaration* mdec)
{
	Declaration* gdec = mCompilationUnits->mScope->Insert(mdec->mQualIdent, mdec);
	if (gdec)
	{
		if (gdec->mType == DT_CONST_FUNCTION)
		{
			Declaration* pcdec = nullptr;
			Declaration* pdec = gdec;

			while (pdec && !mdec->mBase->IsSameParams(pdec->mBase))
			{
				pcdec = pdec;
				pdec = pdec->mNext;
			}

			if (!pdec)
				pcdec->mNext = mdec;

			dec->mScope->Insert(mdec->mIdent, gdec);
		}
		else
			mErrors->Error(mdec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate struct member declaration", mdec->mIdent);
	}
	else
		dec->mScope->Insert(mdec->mIdent, mdec);
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
		if (edec && mScanner->mToken != TK_OPEN_BRACE && mScanner->mToken != TK_COLON)
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
					mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Error duplicate struct declaration", structName);
				}
			}
		}
	}

	if (!dec->mIdent || !dec->mScope)
	{
		dec->mIdent = structName;
		dec->mQualIdent = mScope->Mangle(structName);
		dec->mScope = new DeclarationScope(nullptr, SLEVEL_CLASS, structName);
	}

	if ((mCompilerOptions & COPT_CPLUSPLUS) && mScanner->mToken == TK_COLON)
	{
		mScanner->NextToken();
		Declaration* pdec = ParseQualIdent();
		if (pdec)
		{
			if (pdec->mType == DT_TYPE_STRUCT)
			{
				dec->mBase = pdec;
				dec->mSize = pdec->mSize;
			}
			else
				mErrors->Error(mScanner->mLocation, ERRO_NOT_A_BASE_CLASS, "Not a base class", dec->mIdent);
		}
	}

	if (mScanner->mToken == TK_OPEN_BRACE)
	{
		Declaration* pthis = nullptr;
		
		if (mCompilerOptions & COPT_CPLUSPLUS)
		{
			pthis = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
			pthis->mFlags |= DTF_CONST | DTF_DEFINED;
			pthis->mBase = dec;
			pthis->mSize = 2;
		}

		mScanner->NextToken();
		Declaration* mlast = nullptr;
		for (;;)
		{
			Declaration* mdec = ParseDeclaration(nullptr, false, false, pthis);

			mdec->mQualIdent = dec->mScope->Mangle(mdec->mIdent);

			int	offset = dec->mSize;
			if (dt == DT_TYPE_UNION)
				offset = 0;

			if (mdec->mBase->mType == DT_TYPE_FUNCTION)
			{
				mdec->mType = DT_CONST_FUNCTION;
				mdec->mSection = mCodeSection;
				mdec->mFlags |= DTF_GLOBAL;
				mdec->mBase->mFlags |= DTF_FUNC_THIS;

				if (mCompilerOptions & COPT_NATIVE)
					mdec->mFlags |= DTF_NATIVE;

				if (!(mdec->mFlags & DTF_DEFINED))
					ConsumeToken(TK_SEMICOLON);

				AddMemberFunction(dec, mdec);
			}
			else
			{
				while (mdec)
				{
					if (!(mdec->mBase->mFlags & DTF_DEFINED))
						mErrors->Error(mdec->mLocation, EERR_UNDEFINED_OBJECT, "Undefined type used in struct member declaration");

					if (mdec->mType != DT_VARIABLE)
					{
						mErrors->Error(mdec->mLocation, EERR_UNDEFINED_OBJECT, "Named structure element expected");
						break;
					}

					mdec->mType = DT_ELEMENT;
					mdec->mOffset = offset;

					offset += mdec->mBase->mSize;
					if (offset > dec->mSize)
						dec->mSize = offset;

					if (dec->mScope->Insert(mdec->mIdent, mdec))
						mErrors->Error(mdec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate struct member declaration", mdec->mIdent);

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

		dec->SetDefined();

		if ((mCompilerOptions & COPT_CPLUSPLUS) && dec->mType == DT_TYPE_STRUCT && dec->mIdent)
		{
			AddDefaultConstructors(pthis);

			// Lookup constructors, have same name as class
			Declaration* cdec = dec->mScope->Lookup(dec->mIdent);
			while (cdec)
			{
				if (cdec->mFlags & DTF_DEFINED)
					PrependMemberConstructor(pthis, cdec);
				cdec = cdec->mNext;
			}

			AppendMemberDestructor(pthis);
		}
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

	case TK_AUTO:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_AUTO);
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
				if ((flags & ~dec->mFlags) == DTF_CONST)
					dec = dec->ToConstType();
				else
				{
					Declaration* ndec = new Declaration(dec->mLocation, dec->mType);
					ndec->mFlags = dec->mFlags | flags;
					ndec->mSize = dec->mSize;
					ndec->mBase = dec->mBase;
					ndec->mScope = dec->mScope;
					ndec->mParams = dec->mParams;
					ndec->mIdent = dec->mIdent;
					ndec->mQualIdent = dec->mQualIdent;
					dec = ndec;
				}
			}
			mScanner->NextToken();
		}
		else if (!dec)
		{
			mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Identifier not defined", mScanner->mTokenIdent);
			mScanner->NextToken();
		}
		else
		{			
			mErrors->Error(mScanner->mLocation, EERR_NOT_A_TYPE, "Identifier is no type", mScanner->mTokenIdent);
			mScanner->NextToken();
		}
		break;

	case TK_ENUM:
	{
		dec = new Declaration(mScanner->mLocation, DT_TYPE_ENUM);
		dec->mFlags = flags;
		dec->mSize = 1;
		dec->mScope = new DeclarationScope(nullptr, SLEVEL_CLASS);

		mScanner->NextToken();
		if (mScanner->mToken == TK_IDENT)
		{
			dec->mIdent = mScanner->mTokenIdent;
			dec->mQualIdent = mScope->Mangle(dec->mIdent);
			
			if (mScope->Insert(dec->mIdent, dec))
				mErrors->Error(dec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate name", dec->mIdent);
			mScanner->NextToken();
		}

		int	nitem = 0;
		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			mScanner->NextToken();
			if (mScanner->mToken == TK_IDENT)
			{
				int64	minValue = 0, maxValue = 0;

				for (;;)
				{
					Declaration* cdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
					cdec->mBase = dec;
					cdec->mSize = 2;
					if (mScanner->mToken == TK_IDENT)
					{
						cdec->mIdent = mScanner->mTokenIdent;
						cdec->mQualIdent = mScope->Mangle(cdec->mIdent);
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

					cdec->mNext = dec->mParams;
					dec->mParams = cdec;

					if (mScanner->mToken == TK_COMMA)
						mScanner->NextToken();
					else
						break;
				}

				dec->mMinValue = minValue;
				dec->mMaxValue = maxValue;

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
	else if (mScanner->mToken == TK_BINARY_AND && (mCompilerOptions & COPT_CPLUSPLUS))
	{
		mScanner->NextToken();

		Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_REFERENCE);
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
		dec->mQualIdent = mScope->Mangle(dec->mIdent);
		dec->mSection = mBSSection;
		dec->mBase = nullptr;
		mScanner->NextToken();
		if (mScanner->mToken == TK_OPEN_PARENTHESIS && mScope->mLevel >= SLEVEL_FUNCTION)
		{
			// Can't be a function declaration in local context, so it must be an object
			// declaration with initializer, so return immediately

			return dec;
		}

		while (ConsumeTokenIf(TK_COLCOLON))
		{
			if (mScanner->mToken == TK_IDENT)
			{
				dec->mIdent = mScanner->mTokenIdent;

				char	buffer[200];
				strcpy_s(buffer, dec->mQualIdent->mString);
				strcat_s(buffer, "::");
				strcat_s(buffer, dec->mIdent->mString);
				dec->mQualIdent = Ident::Unique(buffer);

				mScanner->NextToken();
			}
		}
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
			dec = ParseFunctionDeclaration(dec);
		}
		else
			return dec;
	}
}

Declaration * Parser::ParseFunctionDeclaration(Declaration* bdec)
{
	Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
	ndec->mSize = 0;
	Declaration* pdec = nullptr;
	mScanner->NextToken();
	if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
	{
		int	vi = 0;
		for (;;)
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
					Declaration* ndec = new Declaration(adec->mBase->mLocation, DT_TYPE_POINTER);
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
	ndec->mBase = bdec;
	ndec->mFlags |= DTF_DEFINED;

	return ndec;
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
			else if (dec->mType == DT_CONST_ADDRESS)
			{
				Declaration* ndec = new Declaration(dec->mLocation, DT_CONST_ADDRESS);
				ndec->mInteger = dec->mInteger;
				ndec->mBase = dtype;
				dec = ndec;
			}

			dec->mOffset = offset;
		}
	}
	else if (dec && dtype->mType == DT_TYPE_POINTER && (dec->mType == DT_VARIABLE || dec->mType == DT_VARIABLE_REF) && exp->mDecType->mType == DT_TYPE_ARRAY && (dec->mFlags & DTF_GLOBAL))
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
		{
			dtype->CanAssign(exp->mDecType);
			mErrors->Error(exp->mLocation, EERR_CONSTANT_INITIALIZER, "Incompatible constant initializer");
		}
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

	if (dtype->mType == DT_TYPE_AUTO)
	{
		exp = ParseRExpression();
	}
	else if (dtype->mType == DT_TYPE_ARRAY || dtype->mType == DT_TYPE_STRUCT || dtype->mType == DT_TYPE_UNION)
	{
		if (dtype->mType != DT_TYPE_ARRAY && !(dtype->mFlags & DTF_DEFINED))
		{
			if (dtype->mIdent)
				mErrors->Error(mScanner->mLocation, EERR_UNDEFINED_OBJECT, "Constant for undefined type", dtype->mIdent);
			else
				mErrors->Error(mScanner->mLocation, EERR_UNDEFINED_OBJECT, "Constant for undefined anonymous type");
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
				int	index = 0, stride = dtype->Stride(), size = 0;

				if (dtype->mFlags & DTF_STRIPED)
					dec->mStripe = dtype->mBase->mStripe;

				while (!(dtype->mFlags & DTF_DEFINED) || size < dtype->mSize)
				{
					int	nrep = 1;

					if (ConsumeTokenIf(TK_OPEN_BRACKET))
					{
						Expression* istart = ParseRExpression();
						if (istart->mType != EX_CONSTANT || istart->mDecValue->mType != DT_CONST_INTEGER)
							mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Constant index expected");
						else
						{
							index = stride * int(istart->mDecValue->mInteger);
							if (index >= dtype->mSize)
							{
								mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Constant initializer out of range");
								break;
							}

							if (ConsumeTokenIf(TK_ELLIPSIS))
							{
								Expression* iend = ParseRExpression();
								if (iend->mType != EX_CONSTANT || iend->mDecValue->mType != DT_CONST_INTEGER)
									mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Constant index expected");
								else
								{
									nrep = int(iend->mDecValue->mInteger - istart->mDecValue->mInteger + 1);

									if (size + nrep * dtype->mBase->mSize > dtype->mSize)
									{
										mErrors->Error(mScanner->mLocation, EERR_CONSTANT_INITIALIZER, "Constant initializer out of range");
										break;
									}
								}
							}
						}

						ConsumeToken(TK_CLOSE_BRACKET);
						ConsumeToken(TK_ASSIGN);
					}

					Expression* texp = ParseInitExpression(dtype->mBase);
					for (int i = 0; i < nrep; i++)
					{
						Declaration* cdec = CopyConstantInitializer(index, dtype->mBase, texp);

						if (last)
							last->mNext = cdec;
						else
							dec->mParams = cdec;
						last = cdec;

						index += stride;
						size += dtype->mBase->mSize;
					}

					if (!ConsumeTokenIf(TK_COMMA))
						break;
					if (mScanner->mToken == TK_CLOSE_BRACE)
						break;
				}

				if (!(dtype->mFlags & DTF_DEFINED))
				{
					dtype->mFlags |= DTF_DEFINED;
					dtype->mSize = size;
					dec->mSize = size;
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
			ndec->mBase = dtype;
			ndec->mValue = exp;
			dec = ndec;

			Expression	*	nexp = new Expression(mScanner->mLocation, EX_CONSTANT);
			nexp->mDecType = new Declaration(dec->mLocation, DT_TYPE_POINTER);
			nexp->mDecType->mBase = exp->mDecType->mBase;
			nexp->mDecType->mSize = 2;
			nexp->mDecType->mStride = exp->mDecType->mStride;
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
				if (exp->mDecValue->mType == DT_CONST_ADDRESS || exp->mDecValue->mType == DT_CONST_POINTER)
					;
				else if (exp->mDecValue->mType == DT_CONST_FUNCTION)
				{
					Declaration* ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_POINTER);
					ndec->mBase = dtype;
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
				else if (exp->mDecValue->mType == DT_CONST_INTEGER && exp->mDecValue->mInteger == 0)
				{
					Declaration	*	ndec = new Declaration(exp->mDecValue->mLocation, DT_CONST_ADDRESS);
					ndec->mBase = TheVoidPointerTypeDeclaration;
					ndec->mInteger = 0;
					dec = ndec;

					Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTANT);
					nexp->mDecValue = ndec;
					nexp->mDecType = ndec->mBase;
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

Expression* Parser::BuildMemberInitializer(Expression* vexp)
{
	Declaration* fcons = (vexp->mDecType->mType == DT_TYPE_STRUCT && vexp->mDecType->mScope) ? vexp->mDecType->mScope->Lookup(vexp->mDecType->mIdent) : nullptr;

	if (fcons)
	{
		Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
		cexp->mDecValue = fcons;
		cexp->mDecType = cexp->mDecValue->mBase;

		Expression* fexp = new Expression(mScanner->mLocation, EX_CALL);
		fexp->mLeft = cexp;

		mScanner->NextToken();
		if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
		{
			fexp->mRight = ParseListExpression();
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else
		{
			fexp->mRight = nullptr;
			mScanner->NextToken();
		}

		Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
		texp->mToken = TK_BINARY_AND;
		texp->mLeft = vexp;
		texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
		texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
		texp->mDecType->mBase = vexp->mDecType;
		texp->mDecType->mSize = 2;

		if (fexp->mRight)
		{
			Expression* lexp = new Expression(mScanner->mLocation, EX_LIST);
			lexp->mLeft = texp;
			lexp->mRight = fexp->mRight;
			fexp->mRight = lexp;
		}
		else
			fexp->mRight = texp;

		ResolveOverloadCall(cexp, fexp->mRight);

		return fexp;
	}
	else
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_INITIALIZATION);
		nexp->mToken = TK_ASSIGN;
		nexp->mDecType = vexp->mDecType;
		nexp->mLeft = vexp;
		ConsumeToken(TK_OPEN_PARENTHESIS);
		nexp->mRight = ParseRExpression();
		ConsumeToken(TK_CLOSE_PARENTHESIS);

		return nexp;
	}
}

void Parser::PrependMemberConstructor(Declaration* pthis, Declaration* cfunc)
{
	Expression* pthisexp = new Expression(pthis->mLocation, EX_VARIABLE);
	pthisexp->mDecType = pthis;
	pthisexp->mDecValue = pthis->mBase->mDestructor->mBase->mParams;

	Expression* thisexp = new Expression(mScanner->mLocation, EX_PREFIX);
	thisexp->mToken = TK_MUL;
	thisexp->mDecType = pthis->mBase;
	thisexp->mLeft = pthisexp;

	Declaration* dec = pthis->mBase->mParams;
	if (dec)
	{
		dec = dec->Last();
		while (dec)
		{
			if (dec->mType == DT_ELEMENT)
			{
				Declaration* mfunc = cfunc->mScope ? cfunc->mScope->Lookup(dec->mIdent) : nullptr;

				if (mfunc)
				{
					Expression* sexp = new Expression(cfunc->mLocation, EX_SEQUENCE);
					sexp->mLeft = mfunc->mValue;
					sexp->mRight = cfunc->mValue;
					cfunc->mValue = sexp;
				}
				else 
				{
					Declaration* bdec = dec->mBase;
					while (bdec->mType == DT_TYPE_ARRAY)
						bdec = bdec->mBase;

					if (bdec->mType == DT_TYPE_STRUCT && bdec->mDefaultConstructor)
					{
						Expression* qexp = new Expression(pthis->mLocation, EX_QUALIFY);
						qexp->mLeft = thisexp;
						qexp->mDecValue = dec;

						Expression* dexp;

						if (dec->mSize == bdec->mSize)
						{
							qexp->mDecType = bdec;

							Expression* pexp = new Expression(pthis->mLocation, EX_PREFIX);
							pexp->mLeft = qexp;
							pexp->mToken = TK_BINARY_AND;
							pexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
							pexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
							pexp->mDecType->mBase = bdec;
							pexp->mDecType->mSize = 2;

							Declaration* mdec = bdec->mDefaultConstructor;

							Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
							if (mdec)
							{
								cexp->mDecValue = mdec;
								cexp->mDecType = cexp->mDecValue->mBase;
							}
							else
								mErrors->Error(dec->mLocation, EERR_NO_DEFAULT_CONSTRUCTOR, "No default constructor");

							dexp = new Expression(mScanner->mLocation, EX_CALL);
							dexp->mLeft = cexp;
							dexp->mRight = pexp;
						}
						else
						{
							qexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
							qexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
							qexp->mDecType->mBase = bdec;
							qexp->mDecType->mSize = 2;

							Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
							cexp->mDecValue = bdec->mVectorConstructor;
							cexp->mDecType = cexp->mDecValue->mBase;

							Declaration* ncdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
							ncdec->mBase = TheUnsignedIntTypeDeclaration;
							ncdec->mInteger = dec->mSize / bdec->mSize;

							dexp = new Expression(mScanner->mLocation, EX_CALL);
							dexp->mLeft = cexp;
							dexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
							dexp->mRight->mLeft = qexp;
							dexp->mRight->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
							dexp->mRight->mRight->mDecType = ncdec->mBase;
							dexp->mRight->mRight->mDecValue = ncdec;
						}

						Expression* sexp = new Expression(mScanner->mLocation, EX_SEQUENCE);

						sexp->mLeft = dexp;
						sexp->mRight = cfunc->mValue;
						cfunc->mValue = sexp;
					}
				}
			}

			dec = dec->mPrev;
		}
	}
}

void Parser::BuildMemberConstructor(Declaration* pthis, Declaration* cfunc)
{
	Expression* pthisexp = new Expression(pthis->mLocation, EX_VARIABLE);
	pthisexp->mDecType = pthis;
	pthisexp->mDecValue = cfunc->mBase->mParams;

	Expression* thisexp = new Expression(mScanner->mLocation, EX_PREFIX);
	thisexp->mToken = TK_MUL;
	thisexp->mDecType = pthis->mBase;
	thisexp->mLeft = pthisexp;

	cfunc->mScope = new DeclarationScope(nullptr, SLEVEL_CLASS);

	DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_FUNCTION);
	mScope = scope;

	Declaration* pdec = cfunc->mBase->mParams;
	while (pdec)
	{
		if (pdec->mIdent)
			scope->Insert(pdec->mIdent, pdec);
		pdec = pdec->mNext;
	}
	Declaration* othis = mThisPointer;
	mThisPointer = pthis;

	mScanner->NextToken();
	do {
		if (ExpectToken(TK_IDENT))
		{
			Declaration* dec = pthis->mBase->mScope->Lookup(mScanner->mTokenIdent);
			if (dec && dec->mType == DT_ELEMENT)
			{
				Expression* qexp = new Expression(pthis->mLocation, EX_QUALIFY);
				qexp->mLeft = thisexp;
				qexp->mDecValue = dec;
				qexp->mDecType = dec->mBase;

				Declaration* dec = new Declaration(mScanner->mLocation, DT_CONST_CONSTRUCTOR);
				dec->mIdent = mScanner->mTokenIdent;

				mScanner->NextToken();

				dec->mValue = BuildMemberInitializer(qexp);

				cfunc->mScope->Insert(dec->mIdent, dec);
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Base class or member not found", mScanner->mTokenIdent);
		}

	} while (ConsumeTokenIf(TK_COMMA));

	mScope = mScope->mParent;
	mThisPointer = othis;
}

void Parser::AddDefaultConstructors(Declaration* pthis)
{
	bool	simpleDestructor = true, simpleAssignment = true;

	char	dname[100];
	strcpy_s(dname, "~");
	strcat_s(dname, pthis->mBase->mIdent->mString);

	const Ident	*	dtorident = Ident::Unique(dname);

	// Extract constructor and destructor from scope

	Declaration* cdec = pthis->mBase->mScope->Lookup(pthis->mBase->mIdent);
	while (cdec)
	{
		Declaration* ctdec = cdec->mBase;
		Declaration* tparam = ctdec->mParams->mNext;

		if (!tparam)
			pthis->mBase->mDefaultConstructor = cdec;
		else if (!tparam->mNext && tparam->mBase->mType == DT_TYPE_REFERENCE && pthis->mBase->IsConstSame(tparam->mBase->mBase))
			pthis->mBase->mCopyConstructor = cdec;

		cdec = cdec->mNext;
	}

	Declaration* ddec = pthis->mBase->mScope->Lookup(dtorident);
	if (ddec)
		pthis->mBase->mDestructor = ddec;

	Declaration* adec = pthis->mBase->mScope->Lookup(Ident::Unique("operator="));
	while (adec)
	{
		Declaration* atdec = adec->mBase;
		Declaration* tparam = atdec->mParams->mNext;

		if (!tparam->mNext && tparam->mBase->mType == DT_TYPE_REFERENCE && pthis->mBase->IsConstSame(tparam->mBase->mBase))
			pthis->mBase->mCopyAssignment = adec;

		adec = adec->mNext;
	}

	Declaration* dec = pthis->mBase->mParams;
	while (dec)
	{
		if (dec->mType == DT_ELEMENT)
		{
			Declaration* bdec = dec->mBase;
			int			nitems = 1;

			while (bdec->mType == DT_TYPE_ARRAY)
				bdec = bdec->mBase;

			if (bdec->mType == DT_TYPE_STRUCT)
			{
				if (bdec->mDestructor)
					simpleDestructor = false;
				if (bdec->mCopyAssignment)
					simpleAssignment = false;
			}
		}
		dec = dec->mNext;
	}

	if (!simpleDestructor)
	{
		if (!pthis->mBase->mDestructor)
		{
			Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
			ctdec->mSize = 0;
			Declaration* pdec = nullptr;
			ctdec->mBase = TheVoidTypeDeclaration;
			ctdec->mFlags |= DTF_DEFINED;

			PrependThisArgument(ctdec, pthis);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			pthis->mBase->mDestructor = cdec;

			cdec->mIdent = dtorident;
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			cdec->mCompilerOptions = mCompilerOptions;
			cdec->mBase->mCompilerOptions = mCompilerOptions;

			cdec->mVarIndex = -1;

			cdec->mFlags |= DTF_DEFINED;
			cdec->mNumVars = mLocalIndex;

			cdec->mValue = new Expression(mScanner->mLocation, EX_VOID);
		}

		if (!pthis->mBase->mDefaultConstructor)
		{
			Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
			ctdec->mSize = 0;
			Declaration* pdec = nullptr;
			ctdec->mBase = TheVoidTypeDeclaration;
			ctdec->mFlags |= DTF_DEFINED;

			PrependThisArgument(ctdec, pthis);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			pthis->mBase->mDefaultConstructor = cdec;

			cdec->mIdent = pthis->mBase->mIdent;
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			AddMemberFunction(pthis->mBase, cdec);

			cdec->mCompilerOptions = mCompilerOptions;
			cdec->mBase->mCompilerOptions = mCompilerOptions;

			cdec->mVarIndex = -1;

			cdec->mFlags |= DTF_DEFINED;
			cdec->mNumVars = mLocalIndex;

			cdec->mValue = new Expression(mScanner->mLocation, EX_VOID);
		}

		if (!pthis->mBase->mCopyConstructor)
		{
			Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
			ctdec->mSize = 0;
			ctdec->mBase = TheVoidTypeDeclaration;
			ctdec->mFlags |= DTF_DEFINED;

			Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
			adec->mVarIndex = 0;
			adec->mOffset = 0;
			adec->mBase = new Declaration(mScanner->mLocation, DT_TYPE_REFERENCE);
			adec->mBase->mSize = 2;
			adec->mBase->mBase = pthis->mBase;
			adec->mBase->mFlags |= DTF_CONST | DTF_DEFINED;
			adec->mSize = adec->mBase->mSize;
			adec->mIdent = adec->mQualIdent = Ident::Unique("_");

			ctdec->mParams = adec;

			PrependThisArgument(ctdec, pthis);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			pthis->mBase->mCopyConstructor = cdec;

			cdec->mIdent = pthis->mBase->mIdent;
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			AddMemberFunction(pthis->mBase, cdec);

			cdec->mCompilerOptions = mCompilerOptions;
			cdec->mBase->mCompilerOptions = mCompilerOptions;

			cdec->mVarIndex = -1;

			cdec->mValue = new Expression(mScanner->mLocation, EX_VOID);

			// Now add all the copying

			Expression* pthisexp = new Expression(pthis->mLocation, EX_VARIABLE);
			pthisexp->mDecType = pthis;
			pthisexp->mDecValue = cdec->mBase->mParams;

			Expression* thisexp = new Expression(mScanner->mLocation, EX_PREFIX);
			thisexp->mToken = TK_MUL;
			thisexp->mDecType = pthis->mBase;
			thisexp->mLeft = pthisexp;

			Expression* thatexp = new Expression(pthis->mLocation, EX_VARIABLE);
			thatexp->mDecType = adec->mBase;
			thatexp->mDecValue = cdec->mBase->mParams->mNext;

			cdec->mScope = new DeclarationScope(nullptr, SLEVEL_CLASS);

			Declaration* dec = pthis->mBase->mParams;
			while (dec)
			{
				if (dec->mType == DT_ELEMENT)
				{
					Expression* lexp = new Expression(pthis->mLocation, EX_QUALIFY);
					lexp->mLeft = thisexp;
					lexp->mDecValue = dec;
					lexp->mDecType = dec->mBase;

					Expression* rexp = new Expression(pthis->mLocation, EX_QUALIFY);
					rexp->mLeft = thatexp;
					rexp->mDecValue = dec;
					rexp->mDecType = dec->mBase;

					Expression* mexp;

					Declaration* bdec = dec->mBase;
					while (bdec->mType == DT_TYPE_ARRAY)
						bdec = bdec->mBase;

					if (bdec->mType == DT_TYPE_STRUCT && bdec->mCopyConstructor)
					{
						if (dec->mSize == bdec->mSize)
						{
							Expression* pexp = new Expression(pthis->mLocation, EX_PREFIX);
							pexp->mLeft = lexp;
							pexp->mToken = TK_BINARY_AND;
							pexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
							pexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
							pexp->mDecType->mBase = dec->mBase;
							pexp->mDecType->mSize = 2;

							Declaration* mdec = dec->mBase->mCopyConstructor;

							Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
							cexp->mDecValue = mdec;
							cexp->mDecType = cexp->mDecValue->mBase;

							mexp = new Expression(mScanner->mLocation, EX_CALL);
							mexp->mLeft = cexp;
							mexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
							mexp->mRight->mLeft = pexp;
							mexp->mRight->mRight = rexp;
						}
						else
						{
							lexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
							lexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
							lexp->mDecType->mBase = bdec;
							lexp->mDecType->mSize = 2;

							rexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
							rexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
							rexp->mDecType->mBase = bdec;
							rexp->mDecType->mSize = 2;

							Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
							cexp->mDecValue = bdec->mVectorCopyConstructor;
							cexp->mDecType = cexp->mDecValue->mBase;

							Declaration* ncdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
							ncdec->mBase = TheUnsignedIntTypeDeclaration;
							ncdec->mInteger = dec->mSize / bdec->mSize;

							mexp = new Expression(mScanner->mLocation, EX_CALL);
							mexp->mLeft = cexp;
							mexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
							mexp->mRight->mLeft = lexp;
							mexp->mRight->mRight = new Expression(mScanner->mLocation, EX_LIST);
							mexp->mRight->mRight->mLeft = rexp;
							mexp->mRight->mRight->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
							mexp->mRight->mRight->mRight->mDecType = ncdec->mBase;
							mexp->mRight->mRight->mRight->mDecValue = ncdec;

						}
					}
					else
					{
						mexp = new Expression(mScanner->mLocation, EX_INITIALIZATION);
						mexp->mLeft = lexp;
						mexp->mRight = rexp;
						mexp->mDecType = lexp->mDecType;
					}

					Declaration* mcdec = new Declaration(mScanner->mLocation, DT_CONST_CONSTRUCTOR);
					mcdec->mIdent = mScanner->mTokenIdent;
					mcdec->mValue = mexp;

					cdec->mScope->Insert(dec->mIdent, mcdec);
				}

				dec = dec->mNext;
			}

			cdec->mFlags |= DTF_DEFINED;
			cdec->mNumVars = mLocalIndex;
		}
	}

	if (!simpleAssignment)
	{
		// Copy assignment operator
		if (!pthis->mBase->mCopyAssignment)
		{
			Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
			ctdec->mSize = 0;
			ctdec->mBase = new Declaration(mScanner->mLocation, DT_TYPE_REFERENCE);
			ctdec->mBase->mSize = 2;
			ctdec->mBase->mBase = pthis->mBase;
			ctdec->mBase->mFlags |= DTF_DEFINED;
			ctdec->mFlags |= DTF_DEFINED;

			Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
			adec->mVarIndex = 0;
			adec->mOffset = 0;
			adec->mBase = new Declaration(mScanner->mLocation, DT_TYPE_REFERENCE);
			adec->mBase->mSize = 2;
			adec->mBase->mBase = pthis->mBase;
			adec->mBase->mFlags |= DTF_CONST | DTF_DEFINED;
			adec->mSize = adec->mBase->mSize;
			adec->mIdent = adec->mQualIdent = Ident::Unique("_");

			ctdec->mParams = adec;

			PrependThisArgument(ctdec, pthis);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			cdec->mIdent = Ident::Unique("operator=");
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			pthis->mBase->mCopyAssignment = cdec;

			cdec->mCompilerOptions = mCompilerOptions;
			cdec->mBase->mCompilerOptions = mCompilerOptions;

			AddMemberFunction(pthis->mBase, cdec);

			cdec->mVarIndex = -1;

			Expression* pthisexp = new Expression(pthis->mLocation, EX_VARIABLE);
			pthisexp->mDecType = pthis;
			pthisexp->mDecValue = cdec->mBase->mParams;

			Expression* thisexp = new Expression(mScanner->mLocation, EX_PREFIX);
			thisexp->mToken = TK_MUL;
			thisexp->mDecType = pthis->mBase;
			thisexp->mLeft = pthisexp;

			cdec->mValue = new Expression(mScanner->mLocation, EX_RETURN);
			cdec->mValue->mLeft = thisexp;
			cdec->mValue->mDecType = cdec->mBase;

			// Now add all the copying

			Expression* thatexp = new Expression(pthis->mLocation, EX_VARIABLE);
			thatexp->mDecType = adec->mBase;
			thatexp->mDecValue = cdec->mBase->mParams->mNext;

			cdec->mScope = new DeclarationScope(nullptr, SLEVEL_CLASS);

			Declaration* dec = pthis->mBase->mParams;
			if (dec)
			{
				dec = dec->Last();
				while (dec)
				{
					if (dec->mType == DT_ELEMENT)
					{
						Expression* lexp = new Expression(pthis->mLocation, EX_QUALIFY);
						lexp->mLeft = thisexp;
						lexp->mDecValue = dec;
						lexp->mDecType = dec->mBase;

						Expression* rexp = new Expression(pthis->mLocation, EX_QUALIFY);
						rexp->mLeft = thatexp;
						rexp->mDecValue = dec;
						rexp->mDecType = dec->mBase;

						Expression* mexp;

						Declaration* bdec = dec->mBase;
						while (bdec->mType == DT_TYPE_ARRAY)
							bdec = bdec->mBase;

						if (bdec->mType == DT_TYPE_STRUCT && bdec->mCopyAssignment)
						{
							if (dec->mSize == bdec->mSize)
							{
								Expression* pexp = new Expression(pthis->mLocation, EX_PREFIX);
								pexp->mLeft = lexp;
								pexp->mToken = TK_BINARY_AND;
								pexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
								pexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
								pexp->mDecType->mBase = dec->mBase;
								pexp->mDecType->mSize = 2;

								Declaration* mdec = bdec->mCopyAssignment;

								Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
								cexp->mDecValue = mdec;
								cexp->mDecType = cexp->mDecValue->mBase;

								mexp = new Expression(mScanner->mLocation, EX_CALL);
								mexp->mLeft = cexp;
								mexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
								mexp->mRight->mLeft = pexp;
								mexp->mRight->mRight = rexp;
							}
							else
							{
								lexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
								lexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
								lexp->mDecType->mBase = bdec;
								lexp->mDecType->mSize = 2;

								rexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
								rexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
								rexp->mDecType->mBase = bdec;
								rexp->mDecType->mSize = 2;

								Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
								cexp->mDecValue = bdec->mVectorCopyAssignment;
								cexp->mDecType = cexp->mDecValue->mBase;

								Declaration* ncdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
								ncdec->mBase = TheUnsignedIntTypeDeclaration;
								ncdec->mInteger = dec->mSize / bdec->mSize;

								mexp = new Expression(mScanner->mLocation, EX_CALL);
								mexp->mLeft = cexp;
								mexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
								mexp->mRight->mLeft = lexp;
								mexp->mRight->mRight = new Expression(mScanner->mLocation, EX_LIST);
								mexp->mRight->mRight->mLeft = rexp;
								mexp->mRight->mRight->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
								mexp->mRight->mRight->mRight->mDecType = ncdec->mBase;
								mexp->mRight->mRight->mRight->mDecValue = ncdec;
							}
						}
						else
						{
							mexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
							mexp->mToken = TK_ASSIGN;
							mexp->mLeft = lexp;
							mexp->mRight = rexp;
							mexp->mDecType = lexp->mDecType;
						}

						Expression* sexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
						sexp->mLeft = mexp;
						sexp->mRight = cdec->mValue;
						cdec->mValue = sexp;
					}

					dec = dec->mPrev;
				}
			}

			cdec->mFlags |= DTF_DEFINED;
			cdec->mNumVars = mLocalIndex;

		}
	}

	if (pthis->mBase->mDefaultConstructor && !pthis->mBase->mVectorConstructor)
	{
		Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
		ctdec->mSize = 0;
		Declaration* pdec = nullptr;
		ctdec->mBase = TheVoidTypeDeclaration;
		ctdec->mFlags |= DTF_DEFINED;

		Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		adec->mVarIndex = 0;
		adec->mOffset = 0;
		adec->mBase = TheUnsignedIntTypeDeclaration;
		adec->mSize = adec->mBase->mSize;
		adec->mIdent = adec->mQualIdent = Ident::Unique("n");

		ctdec->mParams = adec;

		Declaration* vthis = pthis->Clone();
		vthis->mFlags &= ~DTF_CONST;
		PrependThisArgument(ctdec, vthis);

		Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
		cdec->mBase = ctdec;

		cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

		cdec->mSection = mCodeSection;

		if (mCompilerOptions & COPT_NATIVE)
			cdec->mFlags |= DTF_NATIVE;

		pthis->mBase->mVectorConstructor = cdec;

		cdec->mIdent = pthis->mBase->mIdent;
		cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

		cdec->mCompilerOptions = mCompilerOptions;
		cdec->mBase->mCompilerOptions = mCompilerOptions;

		cdec->mVarIndex = -1;

		cdec->mFlags |= DTF_DEFINED;
		cdec->mNumVars = mLocalIndex;

		Expression* pexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		pexp->mDecType = vthis;
		pexp->mDecValue = ctdec->mParams;

		Expression* aexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		aexp->mDecType = TheUnsignedIntTypeDeclaration;
		aexp->mDecValue = adec;

		Expression* iexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		iexp->mToken = TK_INC;
		iexp->mLeft = pexp;

		Expression* dexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		dexp->mToken = TK_DEC;
		dexp->mLeft = aexp;

		Expression* fexp = new Expression(mScanner->mLocation, EX_CONSTANT);
		fexp->mDecValue = pthis->mBase->mDefaultConstructor;
		fexp->mDecType = fexp->mDecValue->mBase;

		Expression*cexp = new Expression(mScanner->mLocation, EX_CALL);
		cexp->mLeft = fexp;
		cexp->mRight = iexp;

		Expression	*	wexp = new Expression(mScanner->mLocation, EX_WHILE);
		wexp->mLeft = aexp;
		wexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
		wexp->mRight->mLeft = cexp;
		wexp->mRight->mRight = dexp;
	
		cdec->mValue = wexp;
	}

	if (pthis->mBase->mDestructor && !pthis->mBase->mVectorDestructor)
	{
		Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
		ctdec->mSize = 0;
		Declaration* pdec = nullptr;
		ctdec->mBase = TheVoidTypeDeclaration;
		ctdec->mFlags |= DTF_DEFINED;

		Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		adec->mVarIndex = 0;
		adec->mOffset = 0;
		adec->mBase = TheUnsignedIntTypeDeclaration;
		adec->mSize = adec->mBase->mSize;
		adec->mIdent = adec->mQualIdent = Ident::Unique("n");

		ctdec->mParams = adec;

		Declaration* vthis = pthis->Clone();
		vthis->mFlags &= ~DTF_CONST;
		PrependThisArgument(ctdec, vthis);

		Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
		cdec->mBase = ctdec;

		cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

		cdec->mSection = mCodeSection;

		if (mCompilerOptions & COPT_NATIVE)
			cdec->mFlags |= DTF_NATIVE;

		pthis->mBase->mVectorDestructor = cdec;

		char	dname[100];
		strcpy_s(dname, "~");
		strcat_s(dname, pthis->mBase->mIdent->mString);
		cdec->mIdent = Ident::Unique(dname);
		cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

		cdec->mCompilerOptions = mCompilerOptions;
		cdec->mBase->mCompilerOptions = mCompilerOptions;

		cdec->mVarIndex = -1;

		cdec->mFlags |= DTF_DEFINED;
		cdec->mNumVars = mLocalIndex;

		Expression* pexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		pexp->mDecType = vthis;
		pexp->mDecValue = ctdec->mParams;

		Expression* aexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		aexp->mDecType = TheUnsignedIntTypeDeclaration;
		aexp->mDecValue = adec;

		Expression* iexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		iexp->mToken = TK_INC;
		iexp->mLeft = pexp;

		Expression* dexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		dexp->mToken = TK_DEC;
		dexp->mLeft = aexp;

		Expression* fexp = new Expression(mScanner->mLocation, EX_CONSTANT);
		fexp->mDecValue = pthis->mBase->mDestructor;
		fexp->mDecType = fexp->mDecValue->mBase;

		Expression* cexp = new Expression(mScanner->mLocation, EX_CALL);
		cexp->mLeft = fexp;
		cexp->mRight = iexp;

		Expression* wexp = new Expression(mScanner->mLocation, EX_WHILE);
		wexp->mLeft = aexp;
		wexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
		wexp->mRight->mLeft = cexp;
		wexp->mRight->mRight = dexp;

		cdec->mValue = wexp;
	}

	if (pthis->mBase->mCopyConstructor && !pthis->mBase->mVectorCopyConstructor)
	{
		Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
		ctdec->mSize = 0;
		Declaration* pdec = nullptr;
		ctdec->mBase = TheVoidTypeDeclaration;
		ctdec->mFlags |= DTF_DEFINED;

		Declaration* sdec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		sdec->mVarIndex = 0;
		sdec->mOffset = 0;
		sdec->mBase = pthis->Clone();
		sdec->mBase->mFlags &= ~DTF_CONST;
		sdec->mSize = sdec->mBase->mSize;
		sdec->mIdent = sdec->mQualIdent = Ident::Unique("_");

		Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		adec->mVarIndex = 2;
		adec->mOffset = 0;
		adec->mBase = TheUnsignedIntTypeDeclaration;
		adec->mSize = adec->mBase->mSize;
		adec->mIdent = adec->mQualIdent = Ident::Unique("n");

		sdec->mNext = adec;
		ctdec->mParams = sdec;

		Declaration* vthis = pthis->Clone();
		vthis->mFlags &= ~DTF_CONST;
		PrependThisArgument(ctdec, vthis);

		Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
		cdec->mBase = ctdec;

		cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

		cdec->mSection = mCodeSection;

		if (mCompilerOptions & COPT_NATIVE)
			cdec->mFlags |= DTF_NATIVE;

		pthis->mBase->mVectorCopyConstructor = cdec;

		cdec->mIdent = pthis->mBase->mIdent;
		cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

		cdec->mCompilerOptions = mCompilerOptions;
		cdec->mBase->mCompilerOptions = mCompilerOptions;

		cdec->mVarIndex = -1;

		cdec->mFlags |= DTF_DEFINED;
		cdec->mNumVars = mLocalIndex;

		Expression* pexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		pexp->mDecType = vthis;
		pexp->mDecValue = ctdec->mParams;

		Expression* psexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		psexp->mDecType = vthis;
		psexp->mDecValue = sdec;

		Expression* aexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		aexp->mDecType = TheUnsignedIntTypeDeclaration;
		aexp->mDecValue = adec;

		Expression* iexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		iexp->mToken = TK_INC;
		iexp->mLeft = pexp;

		Expression* isexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		isexp->mToken = TK_INC;
		isexp->mLeft = psexp;

		Expression* disexp = new Expression(mScanner->mLocation, EX_PREFIX);
		disexp->mToken = TK_MUL;
		disexp->mLeft = isexp;
		disexp->mDecType = vthis->mBase;

		Expression* dexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		dexp->mToken = TK_DEC;
		dexp->mLeft = aexp;

		Expression* fexp = new Expression(mScanner->mLocation, EX_CONSTANT);
		fexp->mDecValue = pthis->mBase->mCopyConstructor;
		fexp->mDecType = fexp->mDecValue->mBase;

		Expression* cexp = new Expression(mScanner->mLocation, EX_CALL);
		cexp->mLeft = fexp;
		cexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
		cexp->mRight->mLeft = iexp;
		cexp->mRight->mRight = disexp;

		Expression* wexp = new Expression(mScanner->mLocation, EX_WHILE);
		wexp->mLeft = aexp;
		wexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
		wexp->mRight->mLeft = cexp;
		wexp->mRight->mRight = dexp;

		cdec->mValue = wexp;
	}

	if (pthis->mBase->mCopyAssignment && !pthis->mBase->mVectorCopyAssignment)
	{
		Declaration* ctdec = new Declaration(mScanner->mLocation, DT_TYPE_FUNCTION);
		ctdec->mSize = 0;
		Declaration* pdec = nullptr;
		ctdec->mBase = TheVoidTypeDeclaration;
		ctdec->mFlags |= DTF_DEFINED;

		Declaration* sdec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		sdec->mVarIndex = 0;
		sdec->mOffset = 0;
		sdec->mBase = pthis->Clone();
		sdec->mBase->mFlags &= ~DTF_CONST;
		sdec->mSize = sdec->mBase->mSize;
		sdec->mIdent = sdec->mQualIdent = Ident::Unique("_");

		Declaration* adec = new Declaration(mScanner->mLocation, DT_ARGUMENT);
		adec->mVarIndex = 2;
		adec->mOffset = 0;
		adec->mBase = TheUnsignedIntTypeDeclaration;
		adec->mSize = adec->mBase->mSize;
		adec->mIdent = adec->mQualIdent = Ident::Unique("n");

		sdec->mNext = adec;
		ctdec->mParams = sdec;

		Declaration* vthis = pthis->Clone();
		vthis->mFlags &= ~DTF_CONST;
		PrependThisArgument(ctdec, vthis);

		Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
		cdec->mBase = ctdec;

		cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

		cdec->mSection = mCodeSection;

		if (mCompilerOptions & COPT_NATIVE)
			cdec->mFlags |= DTF_NATIVE;

		pthis->mBase->mVectorCopyAssignment = cdec;

		cdec->mIdent = pthis->mBase->mIdent;
		cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

		cdec->mCompilerOptions = mCompilerOptions;
		cdec->mBase->mCompilerOptions = mCompilerOptions;

		cdec->mVarIndex = -1;

		cdec->mFlags |= DTF_DEFINED;
		cdec->mNumVars = mLocalIndex;

		Expression* pexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		pexp->mDecType = vthis;
		pexp->mDecValue = ctdec->mParams;

		Expression* psexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		psexp->mDecType = vthis;
		psexp->mDecValue = sdec;

		Expression* aexp = new Expression(mScanner->mLocation, EX_VARIABLE);
		aexp->mDecType = TheUnsignedIntTypeDeclaration;
		aexp->mDecValue = adec;

		Expression* iexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		iexp->mToken = TK_INC;
		iexp->mLeft = pexp;

		Expression* isexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		isexp->mToken = TK_INC;
		isexp->mLeft = psexp;

		Expression* disexp = new Expression(mScanner->mLocation, EX_PREFIX);
		disexp->mToken = TK_MUL;
		disexp->mLeft = isexp;
		disexp->mDecType = vthis->mBase;

		Expression* dexp = new Expression(mScanner->mLocation, EX_POSTINCDEC);
		dexp->mToken = TK_DEC;
		dexp->mLeft = aexp;

		Expression* fexp = new Expression(mScanner->mLocation, EX_CONSTANT);
		fexp->mDecValue = pthis->mBase->mCopyAssignment;
		fexp->mDecType = fexp->mDecValue->mBase;

		Expression* cexp = new Expression(mScanner->mLocation, EX_CALL);
		cexp->mLeft = fexp;
		cexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
		cexp->mRight->mLeft = iexp;
		cexp->mRight->mRight = disexp;

		Expression* wexp = new Expression(mScanner->mLocation, EX_WHILE);
		wexp->mLeft = aexp;
		wexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
		wexp->mRight->mLeft = cexp;
		wexp->mRight->mRight = dexp;

		cdec->mValue = wexp;
	}
}

void Parser::AppendMemberDestructor(Declaration* pthis)
{
	if (pthis->mBase->mDestructor)
	{
		if (pthis->mBase->mDestructor->mFlags & DTF_DEFINED)
		{
			Expression* pthisexp = new Expression(pthis->mLocation, EX_VARIABLE);
			pthisexp->mDecType = pthis;
			pthisexp->mDecValue = pthis->mBase->mDestructor->mBase->mParams;

			Expression* thisexp = new Expression(mScanner->mLocation, EX_PREFIX);
			thisexp->mToken = TK_MUL;
			thisexp->mDecType = pthis->mBase;
			thisexp->mLeft = pthisexp;

			Declaration * dec = pthis->mBase->mParams;
			if (dec)
			{
				dec = dec->Last();
				while (dec)
				{
					if (dec->mType == DT_ELEMENT)
					{
						Declaration* bdec = dec->mBase;
						while (bdec->mType == DT_TYPE_ARRAY)
							bdec = bdec->mBase;

						if (bdec->mType == DT_TYPE_STRUCT && bdec->mDestructor)
						{
							Expression* qexp = new Expression(pthis->mLocation, EX_QUALIFY);
							qexp->mLeft = thisexp;
							qexp->mDecValue = dec;

							Expression* dexp;

							if (dec->mSize == bdec->mSize)
							{
								qexp->mDecType = bdec;

								Expression* pexp = new Expression(pthis->mLocation, EX_PREFIX);
								pexp->mLeft = qexp;
								pexp->mToken = TK_BINARY_AND;
								pexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
								pexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
								pexp->mDecType->mBase = bdec;
								pexp->mDecType->mSize = 2;

								Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
								cexp->mDecValue = bdec->mDestructor;
								cexp->mDecType = cexp->mDecValue->mBase;

								dexp = new Expression(mScanner->mLocation, EX_CALL);
								dexp->mLeft = cexp;
								dexp->mRight = pexp;
							}
							else
							{
								qexp->mDecType = new Declaration(pthis->mLocation, DT_TYPE_POINTER);
								qexp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
								qexp->mDecType->mBase = bdec;
								qexp->mDecType->mSize = 2;

								Expression* cexp = new Expression(pthis->mLocation, EX_CONSTANT);
								cexp->mDecValue = bdec->mVectorDestructor;
								cexp->mDecType = cexp->mDecValue->mBase;

								Declaration* ncdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
								ncdec->mBase = TheUnsignedIntTypeDeclaration;
								ncdec->mInteger = dec->mSize / bdec->mSize;

								dexp = new Expression(mScanner->mLocation, EX_CALL);
								dexp->mLeft = cexp;
								dexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
								dexp->mRight->mLeft = qexp;
								dexp->mRight->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
								dexp->mRight->mRight->mDecType = ncdec->mBase;
								dexp->mRight->mRight->mDecValue = ncdec;
							}

							Expression* sexp = new Expression(mScanner->mLocation, EX_SEQUENCE);

							sexp->mLeft = pthis->mBase->mDestructor->mValue;
							sexp->mRight = dexp;

							pthis->mBase->mDestructor->mValue = sexp;
						}
					}

					dec = dec->mPrev;
				}
			}
		}
	}
}

void Parser::PrependThisArgument(Declaration* fdec, Declaration* pthis)
{
	Declaration* adec = new Declaration(fdec->mLocation, DT_ARGUMENT);

	if (fdec->mBase->mType == DT_TYPE_STRUCT)
		adec->mVarIndex = 2;
	else
		adec->mVarIndex = 0;

	adec->mOffset = 0;
	adec->mBase = pthis;
	adec->mSize = adec->mBase->mSize;
	adec->mNext = fdec->mParams;
	adec->mIdent = adec->mQualIdent = Ident::Unique("this");

	Declaration* p = fdec->mParams;
	while (p)
	{
		p->mVarIndex += 2;
		p = p->mNext;
	}

	fdec->mParams = adec;

	fdec->mFlags |= DTF_FUNC_THIS;
}

static Expression* ConcatExpression(Expression* e1, Expression* e2)
{
	if (e1)
	{
		if (e2)
		{
			Expression* seq = new Expression(e1->mLocation, EX_SEQUENCE);
			seq->mLeft = e1;
			seq->mRight = e2;
			return seq;
		}
		return e1;
	}
	else
		return e2;
}

Expression* Parser::CleanupExpression(Expression* exp)
{
	if (exp)
	{
		Expression* xexp = AddFunctionCallRefReturned(exp);
		if (xexp)
		{
			Expression* cexp = new Expression(exp->mLocation, EX_CLEANUP);
			cexp->mLeft = exp;
			cexp->mRight = xexp;
			return cexp;
		}
	}
	return exp;
}

Expression* Parser::AddFunctionCallRefReturned(Expression* exp)
{
	Expression* lexp = nullptr, * rexp = nullptr;

	if (exp->mType == EX_PREFIX && exp->mToken == TK_BINARY_AND && exp->mLeft->mType == EX_CALL)
	{
		lexp = AddFunctionCallRefReturned(exp->mLeft);

		// Returning a value object for pass by address
		// add a temporary variable

		int	nindex = mLocalIndex++;

		Declaration* rtdec = exp->mLeft->mDecType;

		Declaration* vdec = new Declaration(exp->mLocation, DT_VARIABLE);

		vdec->mVarIndex = nindex;
		vdec->mBase = rtdec;
		vdec->mSize = rtdec->mSize;

		Expression* vexp = new Expression(exp->mLocation, EX_VARIABLE);
		vexp->mDecType = rtdec;
		vexp->mDecValue = vdec;

		Expression * pex = new Expression(exp->mLocation, EX_INITIALIZATION);
		pex->mToken = TK_ASSIGN;
		pex->mLeft = vexp;
		pex->mRight = exp->mLeft;
		pex->mDecValue = nullptr;
		pex->mDecType = vdec->mBase;

		exp->mLeft = pex;

		if (rtdec->mDestructor)
		{
			Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
			texp->mToken = TK_BINARY_AND;
			texp->mLeft = vexp;
			texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
			texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
			texp->mDecType->mBase = vdec->mBase;
			texp->mDecType->mSize = 2;

			Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
			cexp->mDecValue = rtdec->mDestructor;
			cexp->mDecType = cexp->mDecValue->mBase;

			Expression* dexp = new Expression(mScanner->mLocation, EX_CALL);
			dexp->mLeft = cexp;
			dexp->mRight = texp;

			rexp = ConcatExpression(rexp, dexp);
		}
	}
	else if (exp->mType == EX_CALL)
	{
		lexp = AddFunctionCallRefReturned(exp->mLeft);
		if (exp->mRight)
		{
			Declaration* pdec = exp->mLeft->mDecType->mParams;
			Expression* rex = exp->mRight;

			while (pdec && rex)
			{
				Expression* pex = rex->mType == EX_LIST ? rex->mLeft : rex;
				if (rex->mType == EX_LIST)
					rex = rex->mRight;
				else
					rex = nullptr;

				rexp = ConcatExpression(rexp, AddFunctionCallRefReturned(pex));

				if (pdec->mBase->mType == DT_TYPE_REFERENCE && pex->mDecType->mType != DT_TYPE_REFERENCE && pex->mType == EX_CALL)
				{
					// Returning a value object for pass as reference
					// add a temporary variable

					int	nindex = mLocalIndex++;

					Declaration* vdec = new Declaration(exp->mLocation, DT_VARIABLE);

					vdec->mVarIndex = nindex;
					vdec->mBase = pdec->mBase->mBase;
					vdec->mSize = pdec->mBase->mBase->mSize;

					Expression* vexp = new Expression(pex->mLocation, EX_VARIABLE);
					vexp->mDecType = pdec->mBase->mBase;
					vexp->mDecValue = vdec;

					Expression* cexp = new Expression(pex->mLocation, pex->mType);
					cexp->mDecType = pex->mDecType;
					cexp->mDecValue = pex->mDecValue;
					cexp->mLeft = pex->mLeft;
					cexp->mRight = pex->mRight;
					cexp->mToken = pex->mToken;

					pex->mType = EX_INITIALIZATION;
					pex->mLeft = vexp;
					pex->mRight = cexp;
					pex->mDecValue = nullptr;
					pex->mDecType = vdec->mBase;

					if (vdec->mBase->mDestructor)
					{
						Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
						texp->mToken = TK_BINARY_AND;
						texp->mLeft = vexp;
						texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
						texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
						texp->mDecType->mBase = vdec->mBase;
						texp->mDecType->mSize = 2;

						Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
						cexp->mDecValue = vdec->mBase->mDestructor;
						cexp->mDecType = cexp->mDecValue->mBase;

						Expression * dexp = new Expression(mScanner->mLocation, EX_CALL);
						dexp->mLeft = cexp;
						dexp->mRight = texp;

						rexp = ConcatExpression(rexp, dexp);
					}
				}

				pdec = pdec->mNext;
			}
		}
	}
	else
	{
		if (exp->mLeft)
			lexp = AddFunctionCallRefReturned(exp->mLeft);
		if (exp->mRight)
			rexp = AddFunctionCallRefReturned(exp->mRight);
	}

	return ConcatExpression(lexp, rexp);
}

void Parser::ParseVariableInit(Declaration* ndec)
{
	Expression* pexp = nullptr;

	mScanner->NextToken();
	if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
	{
		pexp = ParseListExpression();
		ConsumeToken(TK_CLOSE_PARENTHESIS);
	}
	else
		mScanner->NextToken();

	if (pexp && pexp->mType != EX_LIST && ndec->mBase->CanAssign(pexp->mDecType))
	{
		ndec->mValue = pexp;
	}
	else
	{
		Declaration* fcons = ndec->mBase->mScope ? ndec->mBase->mScope->Lookup(ndec->mBase->mIdent) : nullptr;

		if (fcons)
		{
			Expression* vexp = new Expression(mScanner->mLocation, EX_VARIABLE);
			vexp->mDecType = ndec->mBase;
			vexp->mDecValue = ndec;

			Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
			cexp->mDecValue = fcons;
			cexp->mDecType = cexp->mDecValue->mBase;

			Expression* fexp = new Expression(mScanner->mLocation, EX_CALL);
			fexp->mLeft = cexp;
			fexp->mRight = pexp;

			Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
			texp->mToken = TK_BINARY_AND;
			texp->mLeft = vexp;
			texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
			texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
			texp->mDecType->mBase = ndec->mBase;
			texp->mDecType->mSize = 2;

			if (fexp->mRight)
			{
				Expression* lexp = new Expression(mScanner->mLocation, EX_LIST);
				lexp->mLeft = texp;
				lexp->mRight = fexp->mRight;
				fexp->mRight = lexp;
			}
			else
				fexp->mRight = texp;

			ResolveOverloadCall(cexp, fexp->mRight);

			Expression* dexp = nullptr;
			if (ndec->mBase->mDestructor)
			{
				Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
				cexp->mDecValue = ndec->mBase->mDestructor;
				cexp->mDecType = cexp->mDecValue->mBase;

				dexp = new Expression(mScanner->mLocation, EX_CALL);
				dexp->mLeft = cexp;
				dexp->mRight = texp;
			}

			Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

			nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
			nexp->mLeft->mLeft = fexp;
			nexp->mLeft->mRight = dexp;

			nexp->mRight = vexp;
			nexp->mDecType = vexp->mDecType;

			ndec->mValue = nexp;
		}
		else
		{
			ndec->mValue = pexp;
		}
	}
}

Declaration* Parser::ParseDeclaration(Declaration * pdec, bool variable, bool expression, Declaration* pthis)
{
	bool	definingType = false, destructor = false;
	uint64	storageFlags = 0, typeFlags = 0;
	Declaration* bdec;

	if (pdec)
	{
		bdec = pdec;
	}
	else
	{
		if (mScanner->mToken == TK_TYPEDEF)
		{
			definingType = true;
			variable = false;
			mScanner->NextToken();
		}
		else if (mScanner->mToken == TK_USING)
		{
			mScanner->NextToken();
			if (ConsumeTokenIf(TK_NAMESPACE))
			{
				Declaration* dec = ParseQualIdent();
				if (dec)
				{
					if (dec->mType == DT_NAMESPACE)
					{
						mScope->UseScope(dec->mScope);
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a class or namespace");
				}

				return dec;
			}
			else
			{
				Declaration* dec = ParseQualIdent();
				if (dec)
				{
					Declaration* pdec = mScope->Insert(dec->mIdent, dec);
					if (pdec && pdec != dec)
						mErrors->Error(dec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate declaration", dec->mIdent);
				}

				return dec;
			}
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
				else if (mScanner->mToken == TK_ZEROPAGE)
				{
					storageFlags |= DTF_ZEROPAGE;
					mScanner->NextToken();
				}
				else if (mScanner->mToken == TK_STRIPED)
				{
					storageFlags |= DTF_STRIPED;
					mScanner->NextToken();
				}
				else if (mScanner->mToken == TK_NOINLINE)
				{
					storageFlags |= DTF_PREVENT_INLINE;
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
				else if (mScanner->mToken == TK_DYNSTACK)
				{
					storageFlags |= DTF_DYNSTACK;
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

		if ((mCompilerOptions & COPT_CPLUSPLUS) && pthis && mScanner->mToken == TK_BINARY_NOT)
		{
			// Destructor declaration
			mScanner->NextToken();
			if (mScanner->mToken == TK_IDENT)
			{
				if (mScanner->mTokenIdent != pthis->mBase->mIdent)
					mErrors->Error(mScanner->mLocation, EERR_INVALID_IDENTIFIER, "Wrong class name for destructor", pthis->mBase->mIdent);
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");

			Declaration* ctdec = ParseFunctionDeclaration(TheVoidTypeDeclaration);

			if (ctdec->mParams)				
				mErrors->Error(ctdec->mLocation, EERR_WRONG_PARAMETER, "Destructor can't have parameter");

			PrependThisArgument(ctdec, pthis);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);
			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;
			cdec->mBase->mFlags |= typeFlags;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			char	dname[100];
			strcpy_s(dname, "~");
			strcat_s(dname, pthis->mBase->mIdent->mString);
			cdec->mIdent = Ident::Unique(dname);
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			if (mScanner->mToken == TK_OPEN_BRACE)
			{
				cdec->mCompilerOptions = mCompilerOptions;
				cdec->mBase->mCompilerOptions = mCompilerOptions;

				cdec->mVarIndex = -1;

				cdec->mValue = ParseFunction(cdec->mBase);

				cdec->mFlags |= DTF_DEFINED;
				cdec->mNumVars = mLocalIndex;
			}

			return cdec;
		}

		bdec = ParseBaseTypeDeclaration(typeFlags);
	}

	Declaration* rdec = nullptr, * ldec = nullptr;

	if (mCompilerOptions & COPT_CPLUSPLUS)
	{
		if (bdec && pthis && bdec == pthis->mBase && mScanner->mToken == TK_OPEN_PARENTHESIS)
		{
			Declaration* ctdec = ParseFunctionDeclaration(TheVoidTypeDeclaration);

			Declaration* cdec = new Declaration(ctdec->mLocation, DT_CONST_FUNCTION);

			PrependThisArgument(ctdec, pthis);

			cdec->mBase = ctdec;

			cdec->mFlags |= cdec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

			cdec->mSection = mCodeSection;
			cdec->mBase->mFlags |= typeFlags;

			if (mCompilerOptions & COPT_NATIVE)
				cdec->mFlags |= DTF_NATIVE;

			Declaration* pdec = pthis->mBase->mScope ? pthis->mBase->mScope->Lookup(pthis->mBase->mIdent) : nullptr;
			if (pdec)
			{
				while (pdec && !cdec->mBase->IsSameParams(pdec->mBase))
					pdec = pdec->mNext;
			}

			if (pdec)
			{
				if (!cdec->mBase->IsSame(pdec->mBase))
					mErrors->Error(cdec->mLocation, EERR_DECLARATION_DIFFERS, "Function declaration differs");
				else if (cdec->mFlags & ~pdec->mFlags & (DTF_HWINTERRUPT | DTF_INTERRUPT | DTF_FASTCALL | DTF_NATIVE))
					mErrors->Error(cdec->mLocation, EERR_DECLARATION_DIFFERS, "Function call type declaration differs");
				else
				{
					// 
					// Take parameter names from new declaration
					//
					Declaration* npdec = cdec->mBase->mParams, * ppdec = pdec->mBase->mParams;
					while (npdec && ppdec)
					{
						if (npdec->mIdent)
						{
							ppdec->mIdent = npdec->mIdent;
							ppdec->mQualIdent = npdec->mQualIdent;
						}
						npdec = npdec->mNext;
						ppdec = ppdec->mNext;
					}
				}

				cdec = pdec;
			}

			cdec->mIdent = pthis->mBase->mIdent;
			cdec->mQualIdent = pthis->mBase->mScope->Mangle(cdec->mIdent);

			// Initializer list
			if (mScanner->mToken == TK_COLON)
			{
				BuildMemberConstructor(pthis, cdec);
			}

			if (mScanner->mToken == TK_OPEN_BRACE)
			{
				if (cdec->mFlags & DTF_DEFINED)
					mErrors->Error(cdec->mLocation, EERR_DUPLICATE_DEFINITION, "Function already has a body");

				cdec->mCompilerOptions = mCompilerOptions;
				cdec->mBase->mCompilerOptions = mCompilerOptions;

				cdec->mVarIndex = -1;

				cdec->mValue = ParseFunction(cdec->mBase);

				cdec->mFlags |= DTF_DEFINED;
				cdec->mNumVars = mLocalIndex;
			}

			return cdec;
		}

		if (bdec && bdec->mType == DT_TYPE_STRUCT && ConsumeTokenIf(TK_COLCOLON))
		{
			if (mScanner->mToken == TK_IDENT && mScanner->mTokenIdent == bdec->mIdent)
			{
				mScanner->NextToken();

				Declaration* ctdec = ParseFunctionDeclaration(TheVoidTypeDeclaration);

				Declaration * bthis = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
				bthis->mFlags |= DTF_CONST | DTF_DEFINED;
				bthis->mBase = bdec;
				bthis->mSize = 2;

				PrependThisArgument(ctdec, bthis);

				Declaration* cdec = bdec->mScope->Lookup(bdec->mIdent);
				if (cdec)
				{
					while (cdec && !cdec->mBase->IsSameParams(ctdec))
						cdec = cdec->mNext;
				}

				if (cdec)
				{
					cdec->mFlags |= storageFlags & DTF_REQUEST_INLINE;

					// Initializer list
					if (mScanner->mToken == TK_COLON)
					{
						BuildMemberConstructor(bthis, cdec);
					}

					if (mScanner->mToken == TK_OPEN_BRACE)
					{
						if (cdec->mFlags & DTF_DEFINED)
							mErrors->Error(cdec->mLocation, EERR_DUPLICATE_DEFINITION, "Function already has a body");

						cdec->mCompilerOptions = mCompilerOptions;
						cdec->mBase->mCompilerOptions = mCompilerOptions;

						cdec->mVarIndex = -1;

						cdec->mValue = ParseFunction(cdec->mBase);

						cdec->mFlags |= DTF_DEFINED;
						cdec->mNumVars = mLocalIndex;

						PrependMemberConstructor(bthis, cdec);
					}

					return cdec;
				}
				else
				{
					mErrors->Error(bdec->mLocation, ERRO_NO_MATCHING_FUNCTION_CALL, "No matching constructor");
					return bdec;
				}
			}

			if (ConsumeTokenIf(TK_BINARY_NOT))
			{
				if (mScanner->mToken == TK_IDENT)
				{
					if (mScanner->mTokenIdent != bdec->mIdent)
						mErrors->Error(mScanner->mLocation, EERR_INVALID_IDENTIFIER, "Wrong class name for destructor", bdec->mIdent);
					mScanner->NextToken();
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");

				Declaration* ctdec = ParseFunctionDeclaration(TheVoidTypeDeclaration);

				if (ctdec->mParams)
					mErrors->Error(ctdec->mLocation, EERR_WRONG_PARAMETER, "Destructor can't have parameter");

				if (bdec->mDestructor)
				{
					bdec->mDestructor->mFlags |= storageFlags & DTF_REQUEST_INLINE;

					Declaration* bthis = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
					bthis->mFlags |= DTF_CONST | DTF_DEFINED;
					bthis->mBase = bdec;
					bthis->mSize = 2;

					bdec->mDestructor->mCompilerOptions = mCompilerOptions;
					bdec->mDestructor->mBase->mCompilerOptions = mCompilerOptions;

					bdec->mDestructor->mVarIndex = -1;

					bdec->mDestructor->mValue = ParseFunction(bdec->mDestructor->mBase);

					bdec->mDestructor->mFlags |= DTF_DEFINED;
					bdec->mDestructor->mNumVars = mLocalIndex;

					AppendMemberDestructor(bthis);
				}
				else
					mErrors->Error(ctdec->mLocation, EERR_DUPLICATE_DEFINITION, "Destructor not declared", bdec->mIdent);

				return bdec->mDestructor;
			}
		}
	}

	for (;;)
	{
		Declaration* ndec = ParsePostfixDeclaration();

		ndec = ReverseDeclaration(ndec, bdec);

		if (storageFlags & DTF_STRIPED)
			ndec = ndec->ToStriped(mErrors);

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
		
		if (npdec->mBase->mType == DT_TYPE_FUNCTION)
			npdec->mBase->mFlags |= storageFlags & (DTF_INTERRUPT | DTF_NATIVE | TK_FASTCALL);

		if (definingType)
		{
			if (ndec->mIdent)
			{
				Declaration* pdec = mScope->Insert(ndec->mIdent, ndec->mBase);
				if (pdec && pdec != ndec->mBase)
					mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate type declaration", ndec->mIdent);
			}
		}
		else
		{
			if (ndec->mBase->mType == DT_TYPE_FUNCTION && pthis)
			{
					
				PrependThisArgument(ndec->mBase, pthis);

			}

			if (variable)
			{
				ndec->mFlags |= storageFlags;
				ndec->mFlags |= ndec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

				if (ndec->mBase->mType == DT_TYPE_FUNCTION)
				{
					ndec->mType = DT_CONST_FUNCTION;
					ndec->mSection = mCodeSection;
					ndec->mBase->mFlags |= typeFlags;

					if (mCompilerOptions & COPT_NATIVE)
						ndec->mFlags |= DTF_NATIVE;
				}

				if (ndec->mIdent)
				{
					Declaration* pdec;
					
					if (storageFlags & DTF_ZEROPAGE)
					{
						if (ndec->mType == DT_VARIABLE)
							ndec->mSection = mCompilationUnits->mSectionZeroPage;
						else
							mErrors->Error(ndec->mLocation, ERRR_INVALID_STORAGE_TYPE, "Invalid storage type", ndec->mIdent);
					}

					if (mScope->mLevel < SLEVEL_FUNCTION && !(storageFlags & DTF_STATIC))
					{
						pdec = mCompilationUnits->mScope->Insert(ndec->mQualIdent, ndec);

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
							if (pdec->mBase->mFlags & DTF_FUNC_THIS)
							{
								Declaration* adec = pdec->mBase->mParams->Clone();
								adec->mNext = ndec->mBase->mParams;

								Declaration* p = adec->mBase->mParams;
								while (p)
								{
									p->mVarIndex += 2;
									p = p->mNext;
								}

								ndec->mBase->mParams = adec;

								ndec->mBase->mFlags |= DTF_FUNC_THIS;
							}

							if (mCompilerOptions & COPT_CPLUSPLUS)
							{
								Declaration* pcdec = nullptr;

								while (pdec && !ndec->mBase->IsSameParams(pdec->mBase))
								{
									pcdec = pdec;
									pdec = pdec->mNext;
								}

								if (!pdec)
									pcdec->mNext = ndec;
							}

							if (pdec)
							{
								if (!ndec->mBase->IsSame(pdec->mBase))
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Function declaration differs", ndec->mIdent);
								else if (ndec->mFlags & ~pdec->mFlags & (DTF_HWINTERRUPT | DTF_INTERRUPT | DTF_FASTCALL | DTF_NATIVE))
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Function call type declaration differs", ndec->mIdent);
								else
								{
									// 
									// Take parameter names from new declaration
									//
									Declaration* npdec = ndec->mBase->mParams, * ppdec = pdec->mBase->mParams;
									while (npdec && ppdec)
									{
										if (npdec->mIdent)
										{
											ppdec->mIdent = npdec->mIdent;
											ppdec->mQualIdent = npdec->mQualIdent;
										}
										npdec = npdec->mNext;
										ppdec = ppdec->mNext;
									}
								}

								ndec = pdec;
							}
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
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Variable declaration differs", ndec->mIdent);
							}

							pdec->mFlags |= ndec->mFlags & DTF_ZEROPAGE;

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
									mErrors->Error(ndec->mLocation, EERR_DECLARATION_DIFFERS, "Variable declaration differs", ndec->mIdent);
							}

							if (!(ndec->mFlags & DTF_EXTERN))
							{
								pdec->mFlags &= ~DTF_EXTERN;
								pdec->mSection = ndec->mSection;
							}

							pdec->mFlags |= ndec->mFlags & DTF_ZEROPAGE;

							ndec = pdec;
						}
						else
							mErrors->Error(ndec->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate variable declaration", ndec->mIdent);
					}
				}

				if (mScope->mLevel < SLEVEL_FUNCTION || (ndec->mFlags & DTF_STATIC))
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
//			ndec->mNext = nullptr;

			if (mScanner->mToken == TK_ASSIGN)
			{
				mScanner->NextToken();
				ndec->mValue = ParseInitExpression(ndec->mBase);
				if (ndec->mBase->mType == DT_TYPE_AUTO)
				{
					ndec->mBase = ndec->mValue->mDecType;
					if (ndec->mBase->mType == DT_TYPE_ARRAY)
					{
						ndec->mBase = ndec->mBase->Clone();
						ndec->mBase->mType = DT_TYPE_POINTER;
						ndec->mBase->mSize = 2;
					}
				}

				if (ndec->mFlags & DTF_GLOBAL)
				{
					if (ndec->mFlags & DTF_ZEROPAGE)
						;
					else
						ndec->mSection = mDataSection;
				}
				ndec->mSize = ndec->mBase->mSize;
			}
			else if (mScanner->mToken == TK_OPEN_PARENTHESIS && (mCompilerOptions & COPT_CPLUSPLUS))
			{
				ParseVariableInit(ndec);
			}
			else if ((mCompilerOptions & COPT_CPLUSPLUS) &&  ndec->mType == DT_VARIABLE && !pthis)
			{
				// Find default constructor

				Declaration* bdec = ndec->mBase;
				while (bdec && bdec->mType == DT_TYPE_ARRAY)
					bdec = bdec->mBase;

				if (bdec && bdec->mDefaultConstructor)
				{
					Expression* vexp = new Expression(ndec->mLocation, EX_VARIABLE);
					vexp->mDecType = ndec->mBase;
					vexp->mDecValue = ndec;

					Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
					texp->mToken = TK_BINARY_AND;
					texp->mLeft = vexp;
					texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
					texp->mDecType->mFlags |= DTF_DEFINED;
					texp->mDecType->mBase = bdec;
					texp->mDecType->mSize = 2;

					if (bdec->mSize == ndec->mBase->mSize)
					{
						Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
						cexp->mDecValue = bdec->mDefaultConstructor;
						cexp->mDecType = cexp->mDecValue->mBase;

						Expression* fexp = new Expression(mScanner->mLocation, EX_CALL);
						fexp->mLeft = cexp;
						fexp->mRight = texp;

						Expression* dexp = nullptr;
						if (ndec->mBase->mDestructor)
						{
							Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
							cexp->mDecValue = ndec->mBase->mDestructor;
							cexp->mDecType = cexp->mDecValue->mBase;

							dexp = new Expression(mScanner->mLocation, EX_CALL);
							dexp->mLeft = cexp;
							dexp->mRight = texp;
						}

						Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

						nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
						nexp->mLeft->mLeft = fexp;
						nexp->mLeft->mRight = dexp;

						nexp->mRight = vexp;
						nexp->mDecType = vexp->mDecType;

						ndec->mValue = nexp;
					}
					else
					{
						Declaration* ncdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
						ncdec->mBase = TheUnsignedIntTypeDeclaration;
						ncdec->mInteger = ndec->mSize / bdec->mSize;

						Expression * ncexp = new Expression(mScanner->mLocation, EX_CONSTANT);
						ncexp->mDecType = ncdec->mBase;
						ncexp->mDecValue = ncdec;

						Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
						cexp->mDecValue = bdec->mVectorConstructor;
						cexp->mDecType = cexp->mDecValue->mBase;

						Expression* fexp = new Expression(mScanner->mLocation, EX_CALL);
						fexp->mLeft = cexp;
						fexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
						fexp->mRight->mLeft = vexp;
						fexp->mRight->mRight = ncexp;

						Expression* dexp = nullptr;
						if (bdec->mDestructor)
						{
							Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
							cexp->mDecValue = bdec->mVectorDestructor;
							cexp->mDecType = cexp->mDecValue->mBase;

							dexp = new Expression(mScanner->mLocation, EX_CALL);
							dexp->mLeft = cexp;
							dexp->mRight = new Expression(mScanner->mLocation, EX_LIST);
							dexp->mRight->mLeft = vexp;
							dexp->mRight->mRight = ncexp;
						}

						Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

						nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
						nexp->mLeft->mLeft = fexp;
						nexp->mLeft->mRight = dexp;

						nexp->mRight = vexp;
						nexp->mDecType = vexp->mDecType;

						ndec->mValue = nexp;
					}
				}
				else if (bdec && bdec->mDestructor)
					mErrors->Error(ndec->mLocation, EERR_NO_DEFAULT_CONSTRUCTOR, "No default constructor for class", ndec->mBase->mIdent);
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

				ndec->mCompilerOptions = mCompilerOptions;
				ndec->mBase->mCompilerOptions = mCompilerOptions;

				ndec->mVarIndex = -1;

				ndec->mValue = ParseFunction(ndec->mBase);

				ndec->mFlags |= DTF_DEFINED;
				ndec->mNumVars = mLocalIndex;

			}
			return rdec;
		}
		else
		{
			if (!expression && mScanner->mToken != TK_SEMICOLON)
				mErrors->Error(mScanner->mLocation, ERRR_SEMICOLON_EXPECTED, "Semicolon expected");

			return rdec;
		}
	}

	return rdec;
}

Expression* Parser::ParseDeclarationExpression(Declaration * pdec)
{
	Declaration* dec;
	Expression* exp = nullptr, * rexp = nullptr;

	dec = ParseDeclaration(pdec, true, true);
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
				Expression* nexp;

				if ((mCompilerOptions & COPT_CPLUSPLUS) && dec->mValue->mType == EX_CONSTRUCT)
				{
					nexp = dec->mValue;

					Expression* vdec = nexp->mRight;
					if (vdec->mDecValue != dec)
					{
						vdec->mDecValue = dec;
					}
				}
				else if ((mCompilerOptions & COPT_CPLUSPLUS) && dec->mBase->mDestructor)
				{
					Expression* vexp = new Expression(pdec->mLocation, EX_VARIABLE);
					vexp->mDecType = dec->mBase;
					vexp->mDecValue = dec;

					Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
					texp->mToken = TK_BINARY_AND;
					texp->mLeft = vexp;
					texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
					texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
					texp->mDecType->mBase = pdec->mBase;
					texp->mDecType->mSize = 2;

					Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
					cexp->mDecValue = dec->mBase->mDestructor;
					cexp->mDecType = cexp->mDecValue->mBase;

					Expression* dexp = new Expression(mScanner->mLocation, EX_CALL);
					dexp->mLeft = cexp;
					dexp->mRight = texp;

					Expression* aexp;
					
					if (dec->mBase->mCopyConstructor && !(dec->mValue->mType == EX_CALL && dec->mValue->mDecType->mType == DT_TYPE_STRUCT))
					{
						Declaration* mdec = dec->mBase->mCopyConstructor;

						Expression* cexp = new Expression(dec->mLocation, EX_CONSTANT);
						cexp->mDecValue = mdec;
						cexp->mDecType = cexp->mDecValue->mBase;

						aexp = new Expression(mScanner->mLocation, EX_CALL);
						aexp->mLeft = cexp;
						aexp->mRight = new Expression(dec->mLocation, EX_LIST);
						aexp->mRight->mLeft = texp;
						aexp->mRight->mRight = dec->mValue;
					}
					else
					{
						aexp = new Expression(dec->mValue->mLocation, EX_INITIALIZATION);
						aexp->mToken = TK_ASSIGN;
						aexp->mLeft = new Expression(dec->mLocation, EX_VARIABLE);
						aexp->mLeft->mDecValue = dec;
						aexp->mLeft->mDecType = dec->mBase;
						aexp->mDecType = aexp->mLeft->mDecType;
						aexp->mRight = dec->mValue;
					}

					nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

					nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
					nexp->mLeft->mLeft = aexp;
					nexp->mLeft->mRight = dexp;
					nexp->mRight = vexp;
					nexp->mDecType = vexp->mDecType;
				}
				else
				{
					nexp = new Expression(dec->mValue->mLocation, EX_INITIALIZATION);
					nexp->mToken = TK_ASSIGN;
					nexp->mLeft = new Expression(dec->mLocation, EX_VARIABLE);
					nexp->mLeft->mDecValue = dec;
					nexp->mLeft->mDecType = dec->mBase;
					nexp->mDecType = nexp->mLeft->mDecType;

					nexp->mRight = dec->mValue;
				}

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

Declaration* Parser::ParseQualIdent(void)
{
	Declaration* dec = mScope->Lookup(mScanner->mTokenIdent);
	if (dec)
	{
		mScanner->NextToken();
		while (ConsumeTokenIf(TK_COLCOLON))
		{
			if (mScanner->mToken == TK_IDENT)
			{
				if (dec->mType == DT_NAMESPACE)
				{
					Declaration* ndec = dec->mScope->Lookup(mScanner->mTokenIdent);

					if (ndec)
						dec = ndec;
					else
						mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown identifier", mScanner->mTokenIdent);
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not a class or namespace");

			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Identifier expected");

			mScanner->NextToken();
		}
	}
	else
	{
		mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown identifier", mScanner->mTokenIdent);
		mScanner->NextToken();
	}

	return dec;
}

Expression* Parser::ParseSimpleExpression(bool lhs)
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
		if (lhs)
			exp = ParseDeclarationExpression(nullptr);
		else
		{
			exp = new Expression(mScanner->mLocation, EX_TYPE);
			exp->mDecValue = nullptr;
			exp->mDecType = ParseBaseTypeDeclaration(0);
		}
		break;
	case TK_CONST:
	case TK_VOLATILE:
	case TK_STRUCT:
	case TK_UNION:
	case TK_TYPEDEF:
	case TK_STATIC:
	case TK_AUTO:
	case TK_STRIPED:
		exp = ParseDeclarationExpression(nullptr);
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
	case TK_THIS:
		if (mThisPointer)
		{
			exp = new Expression(mScanner->mLocation, EX_VARIABLE);
			exp->mDecType = mThisPointer->mBase;
			exp->mDecValue = mThisPointer;
		}
		else
			mErrors->Error(mScanner->mLocation, ERRO_THIS_OUTSIDE_OF_METHOD, "Use of this outside of method");

		mScanner->NextToken();
		break;
	case TK_IDENT:
		if (mThisPointer && !mScope->Lookup(mScanner->mTokenIdent, SLEVEL_FUNCTION))
		{
			dec = mThisPointer->mBase->mBase->mScope->Lookup(mScanner->mTokenIdent);
			if (dec)
			{
				Expression * texp = new Expression(mScanner->mLocation, EX_VARIABLE);
				texp->mDecType = mThisPointer->mBase;
				texp->mDecValue = mThisPointer;

				Expression* dexp = new Expression(mScanner->mLocation, EX_PREFIX);
				dexp->mToken = TK_MUL;
				dexp->mDecType = texp->mDecType->mBase;
				dexp->mLeft = texp;

				exp = ParseQualify(dexp);
			}
		}

		if (!exp)
		{
			dec = ParseQualIdent();
			if (dec)
			{
				if (dec->mType == DT_CONST_INTEGER || dec->mType == DT_CONST_FLOAT || dec->mType == DT_CONST_FUNCTION || dec->mType == DT_CONST_ASSEMBLER || dec->mType == DT_LABEL || dec->mType == DT_LABEL_REF)
				{
					exp = new Expression(mScanner->mLocation, EX_CONSTANT);
					exp->mDecValue = dec;
					exp->mDecType = dec->mBase;
					exp->mConst = true;
				}
				else if (dec->mType == DT_VARIABLE || dec->mType == DT_ARGUMENT)
				{
					if (/*(dec->mFlags & DTF_STATIC) &&*/ (dec->mFlags & DTF_CONST) && dec->mValue)
					{
						if (dec->mBase->IsNumericType())
							exp = dec->mValue;
						else if (dec->mBase->mType == DT_TYPE_POINTER)
						{
							if (dec->mValue->mType == EX_CONSTANT)
							{
								if (dec->mValue->mDecValue->mType == DT_CONST_ADDRESS || dec->mValue->mDecValue->mType == DT_CONST_POINTER)
									exp = dec->mValue;
							}
						}
					}

					if (!exp)
					{
						exp = new Expression(mScanner->mLocation, EX_VARIABLE);
						exp->mDecValue = dec;
						exp->mDecType = dec->mBase;
					}
				}
				else if (dec->mType <= DT_TYPE_FUNCTION)
				{
					if (lhs)
						exp = ParseDeclarationExpression(dec);
					else
					{
						exp = new Expression(mScanner->mLocation, EX_TYPE);
						exp->mDecValue = nullptr;
						exp->mDecType = dec;
					}
				}
				else
				{
					mErrors->Error(mScanner->mLocation, EERR_INVALID_IDENTIFIER, "Invalid identifier", mScanner->mTokenIdent);
				}
			}
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
		exp = ParseExpression(true);
		if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");

		if (exp->mType == EX_TYPE)
		{
			if (mScanner->mToken == TK_OPEN_BRACE)
			{
				Declaration* vdec = new Declaration(mScanner->mLocation, DT_VARIABLE);
				vdec->mBase = exp->mDecType;
				vdec->mFlags |= DTF_CONST | DTF_STATIC | DTF_GLOBAL;

				Expression* nexp = new Expression(mScanner->mLocation, EX_VARIABLE);
				nexp->mDecValue = vdec;
				nexp->mDecType = vdec->mBase;

				vdec->mValue = ParseInitExpression(vdec->mBase);
				vdec->mSection = mDataSection;
				vdec->mSize = vdec->mBase->mSize;

				exp = nexp;
			}
			else
			{
				Expression* nexp = new Expression(mScanner->mLocation, EX_TYPECAST);
				nexp->mDecType = exp->mDecType;
				nexp->mLeft = exp;
				nexp->mRight = ParsePrefixExpression(false);
				exp = nexp->ConstantFold(mErrors);
			}
		}
		break;
	case TK_ASM:
		mScanner->NextToken();
		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			mScanner->NextToken();
			exp = ParseAssembler();
			exp->mDecType = TheSignedLongTypeDeclaration;
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


Expression* Parser::ParseQualify(Expression* exp)
{
	Declaration* dtype = exp->mDecType;

	if (dtype->mType == DT_TYPE_REFERENCE)
		dtype = dtype->mBase;

	if (dtype->mType == DT_TYPE_STRUCT || dtype->mType == DT_TYPE_UNION)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_QUALIFY);
		nexp->mLeft = exp;
		if (mScanner->mToken == TK_IDENT)
		{
			Declaration* mdec = dtype->mScope->Lookup(mScanner->mTokenIdent);

			if (mdec)
			{
				mScanner->NextToken();
				if (mdec->mType == DT_ELEMENT)
				{
					nexp->mDecValue = mdec;
					nexp->mDecType = mdec->mBase;
					if (exp->mDecType->mFlags & DTF_CONST)
						nexp->mDecType = nexp->mDecType->ToConstType();

					exp = nexp->ConstantFold(mErrors);
				}
				else if (mdec->mType == DT_CONST_FUNCTION)
				{
					ConsumeToken(TK_OPEN_PARENTHESIS);

					nexp = new Expression(mScanner->mLocation, EX_CALL);
					if (mInlineCall)
						nexp->mType = EX_INLINE;
					mInlineCall = false;

					nexp->mLeft = new Expression(mScanner->mLocation, EX_CONSTANT);
					nexp->mLeft->mDecType = mdec->mBase;
					nexp->mLeft->mDecValue = mdec;

					nexp->mDecType = mdec->mBase;
					if (ConsumeTokenIf(TK_CLOSE_PARENTHESIS))
						nexp->mRight = nullptr;
					else
					{
						nexp->mRight = ParseListExpression();
						ConsumeToken(TK_CLOSE_PARENTHESIS);
					}

					Expression* texp = new Expression(nexp->mLocation, EX_PREFIX);
					texp->mToken = TK_BINARY_AND;
					texp->mLeft = exp;
					texp->mDecType = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
					texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
					texp->mDecType->mBase = exp->mDecType;
					texp->mDecType->mSize = 2;

					if (nexp->mRight)
					{
						Expression* lexp = new Expression(nexp->mLocation, EX_LIST);
						lexp->mLeft = texp;
						lexp->mRight = nexp->mRight;
						nexp->mRight = lexp;
					}
					else
						nexp->mRight = texp;

					ResolveOverloadCall(nexp->mLeft, nexp->mRight);
					nexp->mDecType = nexp->mLeft->mDecType->mBase;

					exp = nexp;
				}
			}
			else
			{
				mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Struct member identifier not found", mScanner->mTokenIdent);
				mScanner->NextToken();
			}

		}
		else
			mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Struct member identifier expected");
	}
	else
		mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Struct expected");

	return exp;
}

static const int NOOVERLOAD = 0x7fffffff;

int Parser::OverloadDistance(Declaration* fdec, Expression* pexp)
{
	Declaration* pdec = fdec->mParams;

	int dist = 0;

	while (pexp)
	{
		Expression* ex = pexp;
		if (pexp->mType == EX_LIST)
		{
			ex = pexp->mLeft;
			pexp = pexp->mRight;
		}
		else
			pexp = nullptr;

		if (pdec)
		{
			Declaration* ptype = pdec->mBase;
			Declaration* etype = ex->mDecType;
			if (etype->mType == DT_TYPE_REFERENCE)
				etype = etype->mBase;

			if (ptype->mType == DT_TYPE_INTEGER && etype->mType == DT_TYPE_INTEGER)
				;
			else if (ptype->mType == DT_TYPE_FLOAT && etype->mType == DT_TYPE_FLOAT)
				;
			else if (ptype->mType == DT_TYPE_INTEGER && etype->mType == DT_TYPE_FLOAT)
				dist += 8;
			else if (ptype->mType == DT_TYPE_FLOAT && etype->mType == DT_TYPE_INTEGER)
				dist++;
			else if (ptype->IsSame(ex->mDecType))
				;
			else if (ptype->mType == DT_TYPE_REFERENCE && ptype->mBase->mType == DT_TYPE_STRUCT && etype->mType == DT_TYPE_STRUCT)
			{
				int	ncast = 0;
				Declaration* ext = ex->mDecType;
				while (ext && !ext->IsConstSame(ptype->mBase))
					ext = ext->mBase;

				if (ext)
				{
					if ((etype->mFlags & DTF_CONST) && !(ptype->mBase->mFlags & DTF_CONST))
						return NOOVERLOAD;

					dist += 16 * ncast;
				}
				else
					return NOOVERLOAD;
			}
			else if (ptype->mType == DT_TYPE_POINTER && etype->mType == DT_TYPE_ARRAY && ptype->mBase->IsSame(etype->mBase))
				dist += 1;
			else if (ptype->IsSubType(etype))
				dist += 256;
			else
				return NOOVERLOAD;

			pdec = pdec->mNext;
		}
		else if (fdec->mFlags & DTF_VARIADIC)
		{
			dist += 1024;
			break;
		}
		else
			return NOOVERLOAD;
	}

	if (pdec)
		return NOOVERLOAD;

	return dist;
}

void Parser::ResolveOverloadCall(Expression* cexp, Expression* pexp)
{
	if (cexp->mDecValue)
	{
		Declaration* fdec = cexp->mDecValue;
		if (fdec->mType == DT_CONST_FUNCTION && fdec->mNext)
		{
			Declaration* dbest = nullptr;
			int				ibest = NOOVERLOAD, nbest = 0;

			while (fdec)
			{
				int d = OverloadDistance(fdec->mBase, pexp);
				if (d < ibest)
				{
					dbest = fdec;
					ibest = d;
					nbest = 1;
				}
				else if (d == ibest)
					nbest++;
				fdec = fdec->mNext;
			}

			if (ibest == NOOVERLOAD)
				mErrors->Error(cexp->mLocation, ERRO_NO_MATCHING_FUNCTION_CALL, "No matching function call");
			else if (nbest > 1)
				mErrors->Error(cexp->mLocation, ERRO_AMBIGUOUS_FUNCTION_CALL, "Ambiguous function call");
			else
			{
				cexp->mDecValue = dbest;
				cexp->mDecType = dbest->mBase;
			}
		}
	}
}

Expression* Parser::ParsePostfixExpression(bool lhs)
{
	Expression* exp = ParseSimpleExpression(lhs);

	for (;;)
	{
		if (mScanner->mToken == TK_OPEN_BRACKET)
		{
			if (exp->mDecType->mType != DT_TYPE_ARRAY && exp->mDecType->mType != DT_TYPE_POINTER)
				mErrors->Error(mScanner->mLocation, EERR_INVALID_INDEX, "Array expected for indexing");
			mScanner->NextToken();
			Expression* nexp = new Expression(mScanner->mLocation, EX_INDEX);
			nexp->mLeft = exp;
			nexp->mRight = ParseExpression(false);
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
			if (exp->mType == EX_TYPE)
			{
				Expression * pexp = nullptr;

				mScanner->NextToken();
				if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
				{
					pexp = ParseListExpression();
					ConsumeToken(TK_CLOSE_PARENTHESIS);
				}
				else
					mScanner->NextToken();

				if (pexp && pexp->mType != EX_LIST && pexp->mDecType->IsSame(exp->mDecType))
				{
					// Simple copy
					exp = pexp;
				}
				else
				{
					Declaration* fcons = exp->mDecType->mScope ? exp->mDecType->mScope->Lookup(exp->mDecType->mIdent) : nullptr;
					if (fcons)
					{
						Declaration* tdec = new Declaration(mScanner->mLocation, DT_VARIABLE);

						tdec->mBase = exp->mDecType;
						tdec->mVarIndex = mLocalIndex++;
						tdec->mSize = exp->mDecType->mSize;
						tdec->mFlags |= DTF_DEFINED;

						Expression* vexp = new Expression(mScanner->mLocation, EX_VARIABLE);
						vexp->mDecType = exp->mDecType;
						vexp->mDecValue = tdec;

						Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
						cexp->mDecValue = fcons;
						cexp->mDecType = cexp->mDecValue->mBase;

						Expression* fexp = new Expression(mScanner->mLocation, EX_CALL);
						fexp->mLeft = cexp;

						fexp->mRight = pexp;

						Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
						texp->mToken = TK_BINARY_AND;
						texp->mLeft = vexp;
						texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
						texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
						texp->mDecType->mBase = exp->mDecType;
						texp->mDecType->mSize = 2;

						if (fexp->mRight)
						{
							Expression* lexp = new Expression(mScanner->mLocation, EX_LIST);
							lexp->mLeft = texp;
							lexp->mRight = fexp->mRight;
							fexp->mRight = lexp;
						}
						else
							fexp->mRight = texp;

						ResolveOverloadCall(cexp, fexp->mRight);

						Expression* dexp = nullptr;
						if (exp->mDecType->mDestructor)
						{
							Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
							cexp->mDecValue = exp->mDecType->mDestructor;
							cexp->mDecType = cexp->mDecValue->mBase;

							dexp = new Expression(mScanner->mLocation, EX_CALL);
							dexp->mLeft = cexp;
							dexp->mRight = texp;
						}

						Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

						nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
						nexp->mLeft->mLeft = fexp;
						nexp->mLeft->mRight = dexp;

						nexp->mRight = vexp;
						nexp->mDecType = vexp->mDecType;

						exp = nexp;
					}
					else
					{
						Expression* nexp = new Expression(mScanner->mLocation, EX_TYPECAST);
						nexp->mDecType = exp->mDecType;
						nexp->mLeft = exp;
						nexp->mRight = pexp;
						exp = nexp->ConstantFold(mErrors);
					}
				}
			}
			else
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
				if (mInlineCall)
					nexp->mType = EX_INLINE;
				mInlineCall = false;

				nexp->mLeft = exp;
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

				ResolveOverloadCall(exp, nexp->mRight);
				nexp->mDecType = exp->mDecType->mBase;

				exp = nexp;
			}
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

				exp = ParseQualify(dexp);
/*
				if (dexp->mDecType->mType == DT_TYPE_STRUCT || dexp->mDecType->mType == DT_TYPE_UNION)
				{
					Expression* nexp = new Expression(mScanner->mLocation, EX_QUALIFY);
					nexp->mLeft = dexp;
					if (mScanner->mToken == TK_IDENT)
					{
						Declaration* mdec = dexp->mDecType->mScope->Lookup(mScanner->mTokenIdent);
						if (mdec)
						{
							if (mdec->mType == DT_ELEMENT)
							{
								nexp->mDecValue = mdec;
								nexp->mDecType = mdec->mBase;
								if (exp->mDecType->mFlags & DTF_CONST)
									nexp->mDecType = nexp->mDecType->ToConstType();

								exp = nexp->ConstantFold(mErrors);
							}
							else if (mdec->mType == DT_CONST_FUNCTION)
							{
								assert(false);
							}
						}
						else
							mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Struct member identifier not found", mScanner->mTokenIdent);
						mScanner->NextToken();
					}
					else
						mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Struct member identifier expected");
				}
				else
					mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Struct expected");
*/
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Pointer expected");
		}
		else if (mScanner->mToken == TK_DOT)
		{
			mScanner->NextToken();

			exp = ParseQualify(exp);
		}
		else
			return exp;
	}
}


Expression* Parser::ParsePrefixExpression(bool lhs)
{
	if (mScanner->mToken == TK_SUB || mScanner->mToken == TK_BINARY_NOT || mScanner->mToken == TK_LOGICAL_NOT || 
		mScanner->mToken == TK_MUL || mScanner->mToken == TK_INC || mScanner->mToken == TK_DEC || mScanner->mToken == TK_BINARY_AND ||
		mScanner->mToken == TK_BANKOF)
	{
		Expression* nexp;
		if (mScanner->mToken == TK_LOGICAL_NOT)
		{
			mScanner->NextToken();
			nexp = ParsePrefixExpression(false);
			nexp = nexp->LogicInvertExpression();
		}
		else if (mScanner->mToken == TK_INC || mScanner->mToken == TK_DEC)
		{
			nexp = new Expression(mScanner->mLocation, EX_PREINCDEC);
			nexp->mToken = mScanner->mToken;
			mScanner->NextToken();
			nexp->mLeft = ParsePrefixExpression(false);;
			nexp->mDecType = nexp->mLeft->mDecType;
		}
		else
		{
			nexp = new Expression(mScanner->mLocation, EX_PREFIX);
			nexp->mToken = mScanner->mToken;
			mScanner->NextToken();
			nexp->mLeft = ParsePrefixExpression(false);
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
				pdec->mFlags |= DTF_DEFINED;
				nexp->mDecType = pdec;
			}
			else if (nexp->mToken == TK_BANKOF)
			{
				nexp->mDecType = TheUnsignedCharTypeDeclaration;
			}
			else
				nexp->mDecType = nexp->mLeft->mDecType;
		}
		return nexp->ConstantFold(mErrors);
	}
	else
		return ParsePostfixExpression(lhs);
}

Expression* Parser::ParseMulExpression(bool lhs)
{
	Expression* exp = ParsePrefixExpression(lhs);

	while (mScanner->mToken == TK_MUL || mScanner->mToken == TK_DIV || mScanner->mToken == TK_MOD)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParsePrefixExpression(false);

		if (nexp->mLeft->mDecType->mType == DT_TYPE_FLOAT || nexp->mRight->mDecType->mType == DT_TYPE_FLOAT)
			nexp->mDecType = TheFloatTypeDeclaration;
		else
			nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseAddExpression(bool lhs)
{
	Expression* exp = ParseMulExpression(lhs);

	while (mScanner->mToken == TK_ADD || mScanner->mToken == TK_SUB)
	{
		Expression * nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseMulExpression(false);
		if (nexp->mLeft->mDecType->mType == DT_TYPE_POINTER && nexp->mRight->mDecType->IsIntegerType())
			nexp->mDecType = nexp->mLeft->mDecType;
		else if (nexp->mRight->mDecType->mType == DT_TYPE_POINTER && nexp->mLeft->mDecType->IsIntegerType())
			nexp->mDecType = nexp->mRight->mDecType;
		else if (nexp->mLeft->mDecType->mType == DT_TYPE_ARRAY && nexp->mRight->mDecType->IsIntegerType())
		{
			Declaration* dec = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
			dec->mSize = 2;
			dec->mBase = nexp->mLeft->mDecType->mBase;
			dec->mStride = nexp->mLeft->mDecType->mStride;
			dec->mStripe = nexp->mLeft->mDecType->mStripe;
			dec->mFlags |= DTF_DEFINED;
			nexp->mDecType = dec;
		}
		else if (nexp->mRight->mDecType->mType == DT_TYPE_ARRAY && nexp->mLeft->mDecType->IsIntegerType())
		{
			Declaration* dec = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
			dec->mSize = 2;
			dec->mBase = nexp->mRight->mDecType->mBase;
			dec->mStride = nexp->mRight->mDecType->mStride;
			dec->mStripe = nexp->mRight->mDecType->mStripe;
			dec->mFlags |= DTF_DEFINED;
			nexp->mDecType = dec;
		}
		else if (nexp->mLeft->mDecType->mType == DT_TYPE_FLOAT || nexp->mRight->mDecType->mType == DT_TYPE_FLOAT)
			nexp->mDecType = TheFloatTypeDeclaration;
		else
			nexp->mDecType = exp->mDecType;

		exp = CheckOperatorOverload(nexp);

		exp = exp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseShiftExpression(bool lhs)
{
	Expression* exp = ParseAddExpression(lhs);

	while (mScanner->mToken == TK_LEFT_SHIFT || mScanner->mToken == TK_RIGHT_SHIFT)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAddExpression(false);
		nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseRelationalExpression(bool lhs)
{
	Expression* exp = ParseShiftExpression(lhs);

	while (mScanner->mToken >= TK_EQUAL && mScanner->mToken <= TK_LESS_EQUAL)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_RELATIONAL);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseShiftExpression(false);
		nexp->mDecType = TheBoolTypeDeclaration;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryAndExpression(bool lhs)
{
	Expression* exp = ParseRelationalExpression(lhs);

	while (mScanner->mToken == TK_BINARY_AND)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseRelationalExpression(false);
		nexp->mDecType = exp->mDecType;

		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryXorExpression(bool lhs)
{
	Expression* exp = ParseBinaryAndExpression(lhs);

	while (mScanner->mToken == TK_BINARY_XOR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryAndExpression(false);
		nexp->mDecType = exp->mDecType;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseBinaryOrExpression(bool lhs)
{
	Expression* exp = ParseBinaryXorExpression(lhs);

	while (mScanner->mToken == TK_BINARY_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryXorExpression(false);
		nexp->mDecType = exp->mDecType;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseLogicAndExpression(bool lhs)
{
	Expression* exp = ParseBinaryOrExpression(lhs);

	while (mScanner->mToken == TK_LOGICAL_AND)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_LOGICAL_AND);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseBinaryOrExpression(false);
		nexp->mDecType = TheBoolTypeDeclaration;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseLogicOrExpression(bool lhs)
{
	Expression* exp = ParseLogicAndExpression(lhs);

	while (mScanner->mToken == TK_LOGICAL_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_LOGICAL_OR);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseLogicAndExpression(false);
		nexp->mDecType = TheBoolTypeDeclaration;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseConditionalExpression(bool lhs)
{
	Expression* exp = ParseLogicOrExpression(lhs);
	
	if (mScanner->mToken == TK_QUESTIONMARK)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_CONDITIONAL);
		nexp->mLeft = exp;
		mScanner->NextToken();
		Expression* texp = new Expression(mScanner->mLocation, EX_SEQUENCE);
		nexp->mRight = texp;

		texp->mLeft = ParseLogicOrExpression(false);
		ConsumeToken(TK_COLON);
		texp->mRight = ParseConditionalExpression(false);
		
		nexp->mDecType = texp->mLeft->mDecType;
		exp = nexp->ConstantFold(mErrors);
	}

	return exp;
}

Expression* Parser::ParseRExpression(void)
{
	return ParseConditionalExpression(false);
}

Expression* Parser::ParseParenthesisExpression(void)
{
	if (mScanner->mToken == TK_OPEN_PARENTHESIS)
		mScanner->NextToken();
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'(' expected");

	Expression* exp = ParseExpression(true);

	if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
		mScanner->NextToken();
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");

	return exp;
}

Expression* Parser::CheckOperatorOverload(Expression* exp)
{
	if (mCompilerOptions & COPT_CPLUSPLUS)
	{
		if (exp->mType == EX_ASSIGNMENT)
		{
			Declaration* tdec = exp->mLeft->mDecType;
			if (tdec->mType == DT_TYPE_STRUCT)
			{
				if (exp->mToken == TK_ASSIGN)
				{
					Declaration* mdec = tdec->mScope->Lookup(Ident::Unique("operator="));
					if (mdec)
					{
						Expression * nexp = new Expression(mScanner->mLocation, EX_CALL);

						nexp->mLeft = new Expression(mScanner->mLocation, EX_CONSTANT);
						nexp->mLeft->mDecType = mdec->mBase;
						nexp->mLeft->mDecValue = mdec;

						nexp->mDecType = mdec->mBase;
						nexp->mRight = exp->mRight;

						Expression* texp = new Expression(nexp->mLocation, EX_PREFIX);
						texp->mToken = TK_BINARY_AND;
						texp->mLeft = exp->mLeft;
						texp->mDecType = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
						texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
						texp->mDecType->mBase = exp->mDecType;
						texp->mDecType->mSize = 2;

						Expression* lexp = new Expression(nexp->mLocation, EX_LIST);
						lexp->mLeft = texp;
						lexp->mRight = nexp->mRight;
						nexp->mRight = lexp;

						ResolveOverloadCall(nexp->mLeft, nexp->mRight);
						nexp->mDecType = nexp->mLeft->mDecType->mBase;

						exp = nexp;
					}
				}
			}
		}
		else if (exp->mType == EX_BINARY)
		{
			const Ident* opident = nullptr;
			switch (exp->mToken)
			{
			case TK_ADD:
				opident = Ident::Unique("operator+");
				break;
			case TK_SUB:
				opident = Ident::Unique("operator+");
				break;
			case TK_MUL:
				opident = Ident::Unique("operator+");
				break;
			case TK_DIV:
				opident = Ident::Unique("operator+");
				break;
			case TK_MOD:
				opident = Ident::Unique("operator%");
				break;
			}

			if (opident)
			{
				Declaration* tdec = exp->mLeft->mDecType;
				if (tdec->mType == DT_TYPE_STRUCT)
				{
					Declaration* mdec = tdec->mScope->Lookup(opident);
					if (mdec)
					{
						Expression* nexp = new Expression(mScanner->mLocation, EX_CALL);

						nexp->mLeft = new Expression(mScanner->mLocation, EX_CONSTANT);
						nexp->mLeft->mDecType = mdec->mBase;
						nexp->mLeft->mDecValue = mdec;

						nexp->mDecType = mdec->mBase;
						nexp->mRight = exp->mRight;

						Expression* texp = new Expression(nexp->mLocation, EX_PREFIX);
						texp->mToken = TK_BINARY_AND;
						texp->mLeft = exp->mLeft;
						texp->mDecType = new Declaration(nexp->mLocation, DT_TYPE_POINTER);
						texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
						texp->mDecType->mBase = exp->mDecType;
						texp->mDecType->mSize = 2;

						Expression* lexp = new Expression(nexp->mLocation, EX_LIST);
						lexp->mLeft = texp;
						lexp->mRight = nexp->mRight;
						nexp->mRight = lexp;

						ResolveOverloadCall(nexp->mLeft, nexp->mRight);
						nexp->mDecType = nexp->mLeft->mDecType->mBase;

						exp = nexp;
					}
				}
			}
		}
	}

	return exp;
}

Expression* Parser::ParseAssignmentExpression(bool lhs)
{
	Expression* exp = ParseConditionalExpression(lhs);

	if (mScanner->mToken >= TK_ASSIGN && mScanner->mToken <= TK_ASSIGN_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAssignmentExpression(false);
		nexp->mDecType = exp->mDecType;

		exp = CheckOperatorOverload(nexp);

		assert(exp->mDecType);
	}

	return exp;
}

Expression* Parser::ParseExpression(bool lhs)
{
	return ParseAssignmentExpression(lhs);
}

Expression* Parser::ParseListExpression(void)
{
	Expression* exp = ParseExpression(true);
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
	Declaration* othis = mThisPointer;
	if (dec->mFlags & DTF_FUNC_THIS)
		mThisPointer = dec->mParams;


	DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_FUNCTION);
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

	if (mCompilerOptions & COPT_CPLUSPLUS)
	{
		// Add destructors for parameters
		Declaration* pdec = dec->mParams;

		while (pdec)
		{
			if (pdec->mBase->mType == DT_TYPE_STRUCT && pdec->mBase->mDestructor)
			{
				Expression* vexp = new Expression(pdec->mLocation, EX_VARIABLE);
				vexp->mDecType = pdec->mBase;
				vexp->mDecValue = pdec;

				Expression* texp = new Expression(mScanner->mLocation, EX_PREFIX);
				texp->mToken = TK_BINARY_AND;
				texp->mLeft = vexp;
				texp->mDecType = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
				texp->mDecType->mFlags |= DTF_CONST | DTF_DEFINED;
				texp->mDecType->mBase = pdec->mBase;
				texp->mDecType->mSize = 2;

				Expression* cexp = new Expression(mScanner->mLocation, EX_CONSTANT);
				cexp->mDecValue = pdec->mBase->mDestructor;
				cexp->mDecType = cexp->mDecValue->mBase;
				
				Expression * dexp = new Expression(mScanner->mLocation, EX_CALL);
				dexp->mLeft = cexp;
				dexp->mRight = texp;

				Expression* nexp = new Expression(mScanner->mLocation, EX_CONSTRUCT);

				nexp->mLeft = new Expression(mScanner->mLocation, EX_LIST);
				nexp->mLeft->mLeft = nullptr;
				nexp->mLeft->mRight = dexp;
				nexp->mRight = vexp;
				nexp->mDecType = vexp->mDecType;

				Expression* sexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
				sexp->mLeft = nexp;
				sexp->mRight = exp;
				exp = sexp;
			}

			pdec = pdec->mNext;
		}
	}

	mScope->End(mScanner->mLocation);
	mScope = mScope->mParent;

	mThisPointer = othis;

	return exp;
}

Expression* Parser::ParseStatement(void)
{
	Expression* exp = nullptr;

	while (mScanner->mToken == TK_PREP_PRAGMA)
	{
		mScanner->NextToken();
		ParsePragma();
	}
	
	if (mScanner->mToken == TK_OPEN_BRACE)
	{
		DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_LOCAL);
		mScope = scope;

		mScanner->NextToken();
		if (mScanner->mToken != TK_CLOSE_BRACE)
		{
			Expression* sexp = new Expression(mScanner->mLocation, EX_SCOPE);

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

			exp->mEndLocation = mScanner->mLocation;
			mScope->End(mScanner->mLocation);
			mScanner->NextToken();

			sexp->mLeft = exp;

			exp = sexp;
		}
		else
		{
			exp = new Expression(mScanner->mLocation, EX_VOID);
			exp->mEndLocation = mScanner->mLocation;
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
			exp->mLeft = CleanupExpression(ParseParenthesisExpression());
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

			DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_LOCAL);
			mScope = scope;

			exp = new Expression(mScanner->mLocation, EX_WHILE);
			exp->mLeft = CleanupExpression(ParseParenthesisExpression());
			exp->mRight = ParseStatement();

			mScope->End(mScanner->mLocation);
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
				exp->mLeft = CleanupExpression(ParseParenthesisExpression());
				ConsumeToken(TK_SEMICOLON);
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "'while' expected");
			break;
		case TK_FOR:
			mScanner->NextToken();
			if (mScanner->mToken == TK_OPEN_PARENTHESIS)
			{
				DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_LOCAL);
				mScope = scope;

				mScanner->NextToken();
				exp = new Expression(mScanner->mLocation, EX_FOR);

				int		unrollLoop = mUnrollLoop;
				bool	unrollPage = mUnrollLoopPage;
				mUnrollLoop = 0;
				mUnrollLoopPage = false;

				Expression* initExp = nullptr, * iterateExp = nullptr, * conditionExp = nullptr, * bodyExp = nullptr, * finalExp = nullptr;


				// Assignment
				if (mScanner->mToken != TK_SEMICOLON)
					initExp = CleanupExpression(ParseExpression(true));
				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "';' expected");

				// Condition
				if (mScanner->mToken != TK_SEMICOLON)
					conditionExp = CleanupExpression(ParseExpression(false));

				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "';' expected");

				// Iteration
				if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
					iterateExp = CleanupExpression(ParseExpression(false));
				if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "')' expected");
				bodyExp = ParseStatement();
				mScope->End(mScanner->mLocation);

				exp->mLeft = new Expression(mScanner->mLocation, EX_SEQUENCE);
				exp->mLeft->mLeft = new Expression(mScanner->mLocation, EX_SEQUENCE);

				if (unrollLoop > 1 && initExp && iterateExp && conditionExp)
				{
					if ((initExp->mType == EX_ASSIGNMENT || initExp->mType == EX_INITIALIZATION) && initExp->mLeft->mType == EX_VARIABLE && initExp->mRight->mType == EX_CONSTANT &&
						(iterateExp->mType == EX_POSTINCDEC || iterateExp->mType == EX_PREINCDEC || iterateExp->mType == EX_ASSIGNMENT && iterateExp->mToken == TK_ASSIGN_ADD && iterateExp->mRight->mType == EX_CONSTANT) && 
						iterateExp->mLeft->IsSame(initExp->mLeft) &&
						conditionExp->mType == EX_RELATIONAL && (conditionExp->mToken == TK_LESS_THAN || conditionExp->mToken == TK_GREATER_THAN) && conditionExp->mLeft->IsSame(initExp->mLeft) && conditionExp->mRight->mType == EX_CONSTANT)
					{
						if (initExp->mRight->mDecValue->mType == DT_CONST_INTEGER && conditionExp->mRight->mDecValue->mType == DT_CONST_INTEGER)
						{
							int	startValue = int(initExp->mRight->mDecValue->mInteger);
							int	endValue = int(conditionExp->mRight->mDecValue->mInteger);
							int	stepValue = 1;

							if (iterateExp->mType == EX_ASSIGNMENT)
								stepValue = int(iterateExp->mRight->mDecValue->mInteger);
							else if (iterateExp->mToken == TK_DEC)
								stepValue = -1;

							if (unrollPage)
							{
								int numLoops = (endValue - startValue + 255) / 256;
								int	numIterations = (endValue - startValue) / numLoops;
								int	stride = (endValue - startValue + numLoops - 1) / numLoops;
								int	remain = (endValue - startValue) - numIterations * numLoops;

								Expression* unrollBody = new Expression(mScanner->mLocation, EX_SEQUENCE);
								unrollBody->mLeft = bodyExp;
								Expression* bexp = unrollBody;
								
								Expression*	iexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
								iexp->mToken = TK_ASSIGN_ADD;
								iexp->mLeft = iterateExp->mLeft;
								iexp->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);

								Declaration	*	idec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
								idec->mInteger = stride;
								idec->mBase = TheSignedIntTypeDeclaration;
								iexp->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
								iexp->mRight->mDecValue = idec;
								iexp->mRight->mDecType = idec->mBase;

								Expression* dexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
								dexp->mToken = TK_ASSIGN_SUB;
								dexp->mLeft = iterateExp->mLeft;
								dexp->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);

								Declaration* ddec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
								ddec->mInteger = stride * (numLoops - 1);
								ddec->mBase = TheSignedIntTypeDeclaration;
								dexp->mRight = new Expression(mScanner->mLocation, EX_CONSTANT);
								dexp->mRight->mDecValue = ddec;
								dexp->mRight->mDecType = ddec->mBase;

								for (int i = 1; i < numLoops; i++)
								{
									bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
									bexp = bexp->mRight;
									bexp->mLeft = iexp;
									bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
									bexp = bexp->mRight;
									bexp->mLeft = bodyExp;
								}
								bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
								bexp = bexp->mRight;
								bexp->mLeft = dexp;

								conditionExp->mRight->mDecValue->mInteger = numIterations;

								if (remain)
								{
									finalExp = new Expression(mScanner->mLocation, EX_SEQUENCE);
									finalExp->mLeft = bodyExp;
									Expression* bexp = finalExp;

									for (int i = 1; i < remain; i++)
									{
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = iexp;
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = bodyExp;
									}
								}

								bodyExp = unrollBody;
							}
							else
							{
								int	numSteps = (endValue - startValue) / stepValue;
								int	remain = numSteps % unrollLoop;
								endValue -= remain * stepValue;

								conditionExp->mRight->mDecValue->mInteger = endValue;

								Expression* unrollBody = new Expression(mScanner->mLocation, EX_SEQUENCE);
								unrollBody->mLeft = bodyExp;
								Expression* bexp = unrollBody;
								if (endValue > startValue)
								{
									for (int i = 1; i < unrollLoop; i++)
									{
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = iterateExp;
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = bodyExp;
									}
								}

								if (remain)
								{
									finalExp = new Expression(mScanner->mLocation, EX_SEQUENCE);
									finalExp->mLeft = bodyExp;
									Expression* bexp = finalExp;

									for (int i = 1; i < remain; i++)
									{
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = iterateExp;
										bexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
										bexp = bexp->mRight;
										bexp->mLeft = bodyExp;
									}
								}

								bodyExp = unrollBody;
							}
						}
						else
							mErrors->Error(exp->mLocation, EWARN_LOOP_UNROLL_IGNORED, "Loop unroll ignored, bounds and step not integer");
					}
					else
						mErrors->Error(exp->mLocation, EWARN_LOOP_UNROLL_IGNORED, "Loop unroll ignored, bounds and step not const");
				}

				exp->mLeft->mRight = initExp;
				exp->mLeft->mLeft->mLeft = conditionExp;
				exp->mLeft->mLeft->mRight = iterateExp;
				exp->mRight = bodyExp;

				if (finalExp)
				{
					Expression* nexp = new Expression(mScanner->mLocation, EX_SEQUENCE);
					nexp->mLeft = exp;
					nexp->mRight = new Expression(mScanner->mLocation, EX_SEQUENCE);
					nexp->mRight->mLeft = finalExp;
					exp = nexp;
				}

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
				exp->mLeft = CleanupExpression(ParseRExpression());
			ConsumeToken(TK_SEMICOLON);
			break;
		case TK_BREAK:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_BREAK);
			ConsumeToken(TK_SEMICOLON);
			break;
		case TK_CONTINUE:
			mScanner->NextToken();
			exp = new Expression(mScanner->mLocation, EX_CONTINUE);
			ConsumeToken(TK_SEMICOLON);
			break;
		case TK_SEMICOLON:
			exp = new Expression(mScanner->mLocation, EX_VOID);
			mScanner->NextToken();
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
			ConsumeToken(TK_SEMICOLON);
			break;

		default:
			exp = CleanupExpression(ParseListExpression());
			ConsumeToken(TK_SEMICOLON);
		}
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
		sexp->mLeft = CleanupExpression(ParseRExpression());
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

	case TK_OPEN_BRACKET:
		mScanner->NextToken();
		exp = ParseAssemblerOperand(pcasm, pcoffset);
		ConsumeToken(TK_CLOSE_BRACKET);
		break;

	case TK_IDENT:
		dec = mScope->Lookup(mScanner->mTokenIdent);
		if (!dec)
		{ 
			exp = new Expression(mScanner->mLocation, EX_CONSTANT);
			dec = new Declaration(mScanner->mLocation, DT_LABEL);
			dec->mIdent = mScanner->mTokenIdent;
			dec->mQualIdent = mScanner->mTokenIdent;
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
						mErrors->Error(mScanner->mLocation, EERR_OBJECT_NOT_FOUND, "Assembler label not found", mScanner->mTokenIdent);
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
			ndec->mQualIdent = exp->mDecValue->mQualIdent;
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
	decaccu->mQualIdent = ident;
	decaccu->mBase = TheUnsignedIntTypeDeclaration;
	decaccu->mSize = 2;
	decaccu->mInteger = value;
	mScope->Insert(decaccu->mIdent, decaccu);
}

Expression* Parser::ParseAssembler(void)
{
	DeclarationScope* scope = new DeclarationScope(mScope, SLEVEL_LOCAL);
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
	decaccu->mQualIdent = decaccu->mIdent;
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
				dec->mQualIdent = dec->mIdent;
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

	mScope->End(mScanner->mLocation);
	mScope = mScope->mParent;
	dassm->mSize = offset;
	dassm->mScope->mParent = nullptr;

	mScanner->SetAssemblerMode(false);

	return ifirst;
}

bool Parser::ExpectToken(Token token)
{
	if (mScanner->mToken == token)
	{
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

bool Parser::ConsumeIdentIf(const char* ident)
{
	if (mScanner->mToken == TK_IDENT && !strcmp(ident, mScanner->mTokenIdent->mString))
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
					index = int(exp->mDecValue->mInteger);
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
				mCompilationUnits->mSectionStack->mSize = int(mScanner->mTokenInteger);
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Stack size expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "heapsize"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_INTEGER)
			{
				mCompilationUnits->mSectionHeap->mSize = int(mScanner->mTokenInteger);
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Heap size expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "charmap"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_INTEGER)
			{
				int	cindex = int(mScanner->mTokenInteger);
				mScanner->NextToken();
				ConsumeToken(TK_COMMA);

				if (mScanner->mToken == TK_INTEGER)
				{
					int	ccode = int(mScanner->mTokenInteger);
					int	ccount = 1;
					mScanner->NextToken();
					if (ConsumeTokenIf(TK_COMMA))
					{
						if (mScanner->mToken == TK_INTEGER)
						{
							ccount = int(mScanner->mTokenInteger);
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
				int	start = 0, end = 0, flags = 0;
				uint64	bank = 0;

				ConsumeToken(TK_COMMA);
				
				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					start = int(exp->mDecValue->mInteger);
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for start expected");
				
				ConsumeToken(TK_COMMA);
				
				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					end = int(exp->mDecValue->mInteger);
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for end expected");
				
				ConsumeToken(TK_COMMA);

				if (mScanner->mToken != TK_COMMA)
				{
					exp = ParseRExpression();
					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
						flags = int(exp->mDecValue->mInteger);
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for flags expected");
				}

				ConsumeToken(TK_COMMA);

				if (mScanner->mToken != TK_COMMA)
				{
					if (mScanner->mToken == TK_OPEN_BRACE)
					{
						do
						{
							mScanner->NextToken();

							exp = ParseRExpression();
							if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
								bank |= 1ULL << exp->mDecValue->mInteger;
							else
								mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");

						} while (mScanner->mToken == TK_COMMA);

						ConsumeToken(TK_CLOSE_BRACE);
					}
					else
					{
						exp = ParseRExpression();
						if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
							bank = 1ULL << exp->mDecValue->mInteger;
						else
							mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");
					}
				}

				LinkerRegion* rgn = mCompilationUnits->mLinker->FindRegion(regionIdent);
				if (!rgn)
				{
					rgn = mCompilationUnits->mLinker->AddRegion(regionIdent, start, end);
					rgn->mFlags = flags;
					rgn->mCartridgeBanks = bank;
				}
				else if (rgn->mStart != start || rgn->mEnd != end || rgn->mFlags != flags || rgn->mCartridgeBanks != bank)
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Conflicting linker region definition");


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
								if (!rgn->mSections.Contains(lsec))
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
						rgn->mReloc = int(exp->mDecValue->mInteger - rgn->mStart);
					else
						mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");
				}
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Region name expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "overlay"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				const Ident* overlayIdent = mScanner->mTokenIdent;
				mScanner->NextToken();

				int	bank = 0;

				ConsumeToken(TK_COMMA);

				Expression* exp;

				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					bank = int(exp->mDecValue->mInteger);
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for bank expected");

				LinkerOverlay* lovl = mCompilationUnits->mLinker->AddOverlay(mScanner->mLocation, overlayIdent, bank);
			}

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
				LinkerSectionType	type = LST_DATA;

				ConsumeToken(TK_COMMA);

				exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
					flags = int(exp->mDecValue->mInteger);
				else
					mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer number for flags expected");

				if (ConsumeTokenIf(TK_COMMA))
				{
					if (mScanner->mToken != TK_COMMA)
					{
						exp = ParseExpression(false);
						if (exp->mDecValue && exp->mDecValue->mType == DT_VARIABLE)
							dstart = exp->mDecValue;
					}

					if (ConsumeTokenIf(TK_COMMA))
					{
						if (mScanner->mToken != TK_COMMA)
						{
							exp = ParseExpression(false);
							if (exp->mDecValue && exp->mDecValue->mType == DT_VARIABLE)
								dend = exp->mDecValue;
						}

						if (ConsumeTokenIf(TK_COMMA))
						{
							if (mScanner->mToken == TK_IDENT)
							{
								if (!strcmp(mScanner->mTokenIdent->mString, "bss"))
									type = LST_BSS;
								else if (!strcmp(mScanner->mTokenIdent->mString, "data"))
									type = LST_DATA;
								else if (!strcmp(mScanner->mTokenIdent->mString, "packed"))
								{
									type = LST_DATA;
									flags |= LSECF_PACKED;
								}
								else
									mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Unknown section type");
							}
							else
								mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Identifier expected");

							mScanner->NextToken();

						}
					}
				}

				LinkerSection* lsec = mCompilationUnits->mLinker->FindSection(sectionIdent);
				if (!lsec)
					lsec = mCompilationUnits->mLinker->AddSection(sectionIdent, type);

				lsec->mFlags |= flags;

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
		else if (!strcmp(mScanner->mTokenIdent->mString, "bss"))
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
					mBSSection = lsec;
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
					{
						dec->mAlignment = int(exp->mDecValue->mInteger);
					}
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
		else if (!strcmp(mScanner->mTokenIdent->mString, "zeropage"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_VARIABLE && (dec->mFlags & DTF_GLOBAL))
				{
					mScanner->NextToken();

					dec->mFlags |= DTF_ZEROPAGE;
					dec->mSection = mCompilationUnits->mSectionZeroPage;
					if (dec->mLinkerObject)
					{
						dec->mLinkerObject->MoveToSection(dec->mSection);
						dec->mLinkerObject->mFlags |= LOBJF_ZEROPAGE;
					}
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
		else if (!strcmp(mScanner->mTokenIdent->mString, "unroll"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);

			mUnrollLoopPage = false;

			if (mScanner->mToken == TK_INTEGER)
			{
				mUnrollLoop = int(mScanner->mTokenInteger);
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_IDENT && !strcmp(mScanner->mTokenIdent->mString, "full"))
			{
				mUnrollLoop = 0x10000;
				mScanner->NextToken();
			}
			else if (mScanner->mToken == TK_IDENT && !strcmp(mScanner->mTokenIdent->mString, "page"))
			{
				mUnrollLoop = 0x10000;
				mUnrollLoopPage = true;
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, EERR_PRAGMA_PARAMETER, "Integer literal expected");

			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "callinline"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			ConsumeToken(TK_CLOSE_PARENTHESIS);
			mInlineCall = true;
		}
		else if (ConsumeIdentIf("optimize"))
		{
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (!ConsumeTokenIf(TK_CLOSE_PARENTHESIS))
			{
				do {
					if (ConsumeIdentIf("push"))
					{
						if (mCompilerOptionSP < 32)
							mCompilerOptionStack[mCompilerOptionSP++] = mCompilerOptions;
						else
							mErrors->Error(mScanner->mLocation, ERRR_STACK_OVERFLOW, "Stack overflow");
					}
					else if (ConsumeIdentIf("pop"))
					{
						if (mCompilerOptionSP > 0)
							mCompilerOptions = mCompilerOptionStack[--mCompilerOptionSP] = mCompilerOptions;
						else
							mErrors->Error(mScanner->mLocation, ERRR_STACK_OVERFLOW, "Stack underflow");
					}
					else if (mScanner->mToken == TK_INTEGER)
					{
						mCompilerOptions &= ~(COPT_OPTIMIZE_ALL);
						switch (mScanner->mTokenInteger)
						{
						case 0:
							break;
						case 1:
							mCompilerOptions |= COPT_OPTIMIZE_DEFAULT;
							break;
						case 2:
							mCompilerOptions |= COPT_OPTIMIZE_SPEED;
							break;
						case 3:
							mCompilerOptions |= COPT_OPTIMIZE_ALL;
							break;
						default:
							mErrors->Error(mScanner->mLocation, ERRR_INVALID_NUMBER, "Invalid number");
						}
						mScanner->NextToken();
					}
					else if (ConsumeIdentIf("asm"))
						mCompilerOptions |= COPT_OPTIMIZE_ASSEMBLER;
					else if (ConsumeIdentIf("noasm"))
						mCompilerOptions &= ~COPT_OPTIMIZE_ASSEMBLER;
					else if (ConsumeIdentIf("size"))
						mCompilerOptions |= COPT_OPTIMIZE_SIZE;
					else if (ConsumeIdentIf("speed"))
						mCompilerOptions &= ~COPT_OPTIMIZE_SIZE;
					else if (ConsumeIdentIf("noinline"))
						mCompilerOptions &= ~(COPT_OPTIMIZE_INLINE | COPT_OPTIMIZE_AUTO_INLINE | COPT_OPTIMIZE_AUTO_INLINE_ALL);
					else if (ConsumeIdentIf("inline"))
						mCompilerOptions |= COPT_OPTIMIZE_AUTO_INLINE;
					else if (ConsumeIdentIf("autoinline"))
						mCompilerOptions |= COPT_OPTIMIZE_AUTO_INLINE | COPT_OPTIMIZE_AUTO_INLINE;
					else if (ConsumeIdentIf("maxinline"))
						mCompilerOptions |= COPT_OPTIMIZE_INLINE | COPT_OPTIMIZE_AUTO_INLINE | COPT_OPTIMIZE_AUTO_INLINE_ALL;
					else
						mErrors->Error(mScanner->mLocation, EERR_INVALID_IDENTIFIER, "Invalid option");

				} while (ConsumeTokenIf(TK_COMMA));

				ConsumeToken(TK_CLOSE_PARENTHESIS);
			}
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
			mErrors->Error(mScanner->mLocation, EWARN_UNKNOWN_PRAGMA, "Unknown pragma, ignored", mScanner->mTokenIdent);
		}
	}
	else
		mErrors->Error(mScanner->mLocation, EERR_SYNTAX, "Invalid pragma directive");
}

void Parser::ParseNamespace(void)
{
	if (mScanner->mToken == TK_IDENT)
	{
		const Ident* ident = mScanner->mTokenIdent;

		mScanner->NextToken();

		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			Declaration* ns = mScope->Lookup(ident);
			if (ns)
			{
				if (ns->mType != DT_NAMESPACE)
				{
					mErrors->Error(mScanner->mLocation, ERRO_NOT_A_NAMESPACE, "Not a namespace", ident);
					ns = nullptr;
				}
			}

			if (!ns)
			{
				ns = new Declaration(mScanner->mLocation, DT_NAMESPACE);
				mScope->Insert(ident, ns);
				ns->mScope = new DeclarationScope(mScope, SLEVEL_NAMESPACE, ident);
			}

			mScanner->NextToken();

			DeclarationScope* outerScope = mScope;
			mScope = ns->mScope;

			while (mScanner->mToken != TK_EOF && mScanner->mToken != TK_CLOSE_BRACE)
			{
				if (mScanner->mToken == TK_PREP_PRAGMA)
				{
					mScanner->NextToken();
					ParsePragma();
				}
				else if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else if (mScanner->mToken == TK_NAMESPACE)
				{
					mScanner->NextToken();
					ParseNamespace();
				}
				else
					ParseDeclaration(nullptr, true, false);
			}

			ConsumeToken(TK_CLOSE_BRACE);

			mScope = outerScope;
		}
		else
		{
			ConsumeToken(TK_ASSIGN);
			Declaration* dec = ParseQualIdent();
			if (dec)
			{
				if (dec->mType == DT_NAMESPACE)
				{
					Declaration* ns = mScope->Insert(ident, dec);
					if (ns)
						mErrors->Error(mScanner->mLocation, EERR_DUPLICATE_DEFINITION, "Duplicate definition", ident);
				}
				else
					mErrors->Error(mScanner->mLocation, ERRO_NOT_A_NAMESPACE, "Not a namespace", ident);
			}
			ConsumeToken(TK_SEMICOLON);
		}
	}
	else
	{
		// Annonymous namespace
	}
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
					exp->mDecValue->mQualIdent = ident;

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
		else if (mScanner->mToken == TK_NAMESPACE)
		{
			mScanner->NextToken();
			ParseNamespace();
		}
		else
			ParseDeclaration(nullptr, true, false);
	}
}

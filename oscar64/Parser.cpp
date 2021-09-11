#include "Parser.h"
#include <string.h>
#include "Assembler.h"
#include "MachineTypes.h"

Parser::Parser(Errors* errors, Scanner* scanner, CompilationUnits* compilationUnits)
	: mErrors(errors), mScanner(scanner), mCompilationUnits(compilationUnits)
{
	mGlobals = new DeclarationScope(compilationUnits->mScope);
	mScope = mGlobals;
}

Parser::~Parser(void)
{

}

Declaration* Parser::ParseBaseTypeDeclaration(uint32 flags)
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
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
		mScanner->NextToken();
		break;

	case TK_BOOL:
		dec = new Declaration(mScanner->mLocation, DT_TYPE_BOOL);
		dec->mSize = 1;
		dec->mFlags = flags | DTF_DEFINED | DTF_SIGNED;
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
			mScanner->NextToken();
		else if (!dec)
		{
			mErrors->Error(mScanner->mLocation, "Identifier not defined", mScanner->mTokenIdent->mString);
			mScanner->NextToken();
		}
		else
		{			
			mErrors->Error(mScanner->mLocation, "Identifier is no type", mScanner->mTokenIdent->mString);
			mScanner->NextToken();
		}
		break;

	case TK_ENUM:
	{
		dec = new Declaration(mScanner->mLocation, DT_TYPE_ENUM);
		dec->mFlags = flags | DTF_SIGNED;
		dec->mSize = 2;

		mScanner->NextToken();
		if (mScanner->mToken == TK_IDENT)
		{
			dec->mIdent = mScanner->mTokenIdent;
			
			if (mScope->Insert(dec->mIdent, dec))
				mErrors->Error(dec->mLocation, "Duplicate name");
			mScanner->NextToken();
		}

		int	nitem = 0;
		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			mScanner->NextToken();
			if (mScanner->mToken == TK_IDENT)
			{
				for (;;)
				{
					Declaration* cdec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
					cdec->mBase = dec;
					cdec->mSize = 2;
					if (mScanner->mToken == TK_IDENT)
					{
						cdec->mIdent = mScanner->mTokenIdent;
						if (mScope->Insert(cdec->mIdent, cdec) != nullptr)
							mErrors->Error(mScanner->mLocation, "Duplicate declaration", mScanner->mTokenIdent->mString);
						mScanner->NextToken();
					}
					if (mScanner->mToken == TK_ASSIGN)
					{
						mScanner->NextToken();
						Expression* exp = ParseRExpression();
						if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER)
							nitem = exp->mDecValue->mInteger;
						else
							mErrors->Error(mScanner->mLocation, "Integer constant expected");
					}
					cdec->mInteger = nitem++;

					if (mScanner->mToken == TK_COMMA)
						mScanner->NextToken();
					else
						break;
				}
			}

			if (mScanner->mToken == TK_CLOSE_BRACE)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, "'}' expected");
		}
		else
			mErrors->Error(mScanner->mLocation, "'{' expected");

		dec->mFlags |= DTF_DEFINED;
		break;
	}
	case TK_STRUCT:
	{
		const Ident* structName = nullptr;

		dec = new Declaration(mScanner->mLocation, DT_TYPE_STRUCT);

		mScanner->NextToken();
		if (mScanner->mToken == TK_IDENT)
		{
			structName = mScanner->mTokenIdent;
			mScanner->NextToken();
			Declaration	*	pdec = mScope->Insert(structName, dec);
			if (pdec)
			{
				if (pdec->mType == DT_TYPE_STRUCT && (pdec->mFlags & DTF_DEFINED))
				{
					dec = pdec;
				}
				else
				{
					mErrors->Error(mScanner->mLocation, "Error duplicate struct declaration", structName->mString);
				}
			}
		}

		dec->mIdent = structName;
		dec->mScope = new DeclarationScope(nullptr);

		if (mScanner->mToken == TK_OPEN_BRACE)
		{
			mScanner->NextToken();
			Declaration* mlast = nullptr;
			for (;;)
			{
				Declaration* mdec = ParseDeclaration(false);
				while (mdec)
				{
					if (!(mdec->mBase->mFlags & DTF_DEFINED))
						mErrors->Error(mdec->mLocation, "Undefined type used in struct member declaration");
					mdec->mType = DT_ELEMENT;
					mdec->mOffset = dec->mSize;
					dec->mSize += mdec->mBase->mSize;
					dec->mScope->Insert(mdec->mIdent, mdec);
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
					mErrors->Error(mScanner->mLocation, "';' expected");
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
		break;
	}

	default:
		mErrors->Error(mScanner->mLocation, "Declaration starts with invalid token", TokenNames[mScanner->mToken]);
		mScanner->NextToken();
	}

	if (!dec)
	{
		dec = new Declaration(mScanner->mLocation, DT_TYPE_VOID);
		dec->mSize = 0;
		dec->mFlags |= DTF_DEFINED;
	}

	return dec;

}

Declaration* Parser::ParsePostfixDeclaration(void)
{
	Declaration* dec;

	if (mScanner->mToken == TK_MUL)
	{
		mScanner->NextToken();
		Declaration* dec = ParsePostfixDeclaration();
		Declaration* ndec = new Declaration(mScanner->mLocation, DT_TYPE_POINTER);
		ndec->mBase = dec;
		ndec->mSize = 2;
		ndec->mFlags |= DTF_DEFINED;
		return ndec;
	}
	else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
	{
		mScanner->NextToken();
		Declaration* vdec = ParsePostfixDeclaration();
		if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
			mScanner->NextToken();
		else
			mErrors->Error(mScanner->mLocation, "')' expected");
		dec = vdec;
	}
	else if (mScanner->mToken == TK_IDENT)
	{
		dec = new Declaration(mScanner->mLocation, DT_VARIABLE);
		dec->mIdent = mScanner->mTokenIdent;
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
			ndec->mFlags |= DTF_DEFINED;
			mScanner->NextToken();
			if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
			{
				Expression* exp = ParseRExpression();
				if (exp->mType == EX_CONSTANT && exp->mDecType->IsIntegerType() && exp->mDecValue->mType == DT_CONST_INTEGER)
					ndec->mSize = exp->mDecValue->mInteger;
				else
					mErrors->Error(exp->mLocation, "Constant integer expression expected");
				ndec->mFlags |= DTF_DEFINED;
			}
			if (mScanner->mToken == TK_CLOSE_BRACKET)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, "']' expected");
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
							mErrors->Error(pdec->mLocation, "Invalid void argument");
						break;
					}
					else
					{
						if (!(adec->mBase->mFlags & DTF_DEFINED))
							mErrors->Error(adec->mLocation, "Type of argument not defined");

						adec->mType = DT_ARGUMENT;
						adec->mVarIndex = vi;
						adec->mOffset = 0;
						adec->mSize = adec->mBase->mSize;
						vi += adec->mBase->mSize;
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
					mErrors->Error(mScanner->mLocation, "')' expected");
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
			mErrors->Error(exp->mLocation, "Incompatible constant initializer");
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
					ndec->mNumber = dec->mInteger;
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
					ndec->mData = dec->mData;
					ndec->mSize = dec->mSize;
					ndec->mBase = dtype;
					dec = ndec;
				}
			}

			dec->mOffset = offset;
		}
	}
	else
		mErrors->Error(exp->mLocation, "Constant initializer expected");

	return dec;
}

Expression* Parser::ParseInitExpression(Declaration* dtype)
{
	Expression* exp = nullptr;
	Declaration* dec;

	if (dtype->mType == DT_TYPE_ARRAY || dtype->mType == DT_TYPE_STRUCT)
	{
		if (!(dtype->mFlags & DTF_DEFINED))
		{
			if (dtype->mIdent)
				mErrors->Error(mScanner->mLocation, "Constant for undefined type", dtype->mIdent->mString);
			else
				mErrors->Error(mScanner->mLocation, "Constant for undefined annonymous type");
		}
		if (ConsumeTokenIf(TK_OPEN_BRACE))
		{
			dec = new Declaration(mScanner->mLocation, DT_CONST_STRUCT);
			dec->mBase = dtype;
			dec->mSize = dtype->mSize;

			Declaration* last = nullptr;

			if (dtype->mType == DT_TYPE_ARRAY)
			{
				int	index = 0;
				while (index < dtype->mSize)
				{
					Expression* texp = ParseInitExpression(dtype->mBase);
					Declaration* cdec = CopyConstantInitializer(index * dtype->mBase->mSize, dtype->mBase, texp);

					if (last)
						last->mNext = cdec;
					else
						dec->mParams = cdec;
					last = cdec;

					index++;
					if (!ConsumeTokenIf(TK_COMMA))
						break;
					if (mScanner->mToken == TK_CLOSE_BRACE)
						break;
				}
			}
			else
			{
				Declaration* mdec = dtype->mParams;
				while (mdec)
				{
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
			dec = new Declaration(mScanner->mLocation, DT_CONST_DATA);
			dec->mBase = dtype;
			dec->mSize = dtype->mSize;
			uint8* d = new uint8[dtype->mSize];
			dec->mData = d;

			if (strlen(mScanner->mTokenString) < dtype->mSize)
			{
				strcpy_s((char *)d, dec->mSize, mScanner->mTokenString);
			}
			else
				mErrors->Error(mScanner->mLocation, "String constant is too large for char array");

			mScanner->NextToken();
		}
		else
		{
			mErrors->Error(mScanner->mLocation, "Struct/Array constant expression expected");
			dec = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
		}

		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecType = dtype;
		exp->mDecValue = dec;
	}
	else
	{
		exp = ParseRExpression();
		if (dtype->mType == DT_TYPE_POINTER && exp->mDecType && exp->mDecType->mType == DT_TYPE_ARRAY)
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
	}

	return exp;
}

Declaration* Parser::ParseDeclaration(bool variable)
{
	bool	definingType = false;
	uint32	storageFlags = 0;

	if (mScanner->mToken == TK_TYPEDEF)
	{
		definingType = true;
		variable = false;
		mScanner->NextToken();
	}
	else if (mScanner->mToken == TK_STATIC)
	{
		storageFlags |= DTF_STATIC;
		mScanner->NextToken();
	}
	else if (mScanner->mToken == TK_EXTERN)
	{
		storageFlags |= DTF_EXTERN;
		mScanner->NextToken();
	}

	Declaration* bdec = ParseBaseTypeDeclaration(0);

	Declaration* rdec = nullptr, * ldec = nullptr;

	for (;;)
	{
		Declaration* ndec = ParsePostfixDeclaration();

		ndec = ReverseDeclaration(ndec, bdec);

		if (definingType)
		{
			if (ndec->mIdent)
			{
				Declaration* pdec = mScope->Insert(ndec->mIdent, ndec->mBase);
				if (pdec)
					mErrors->Error(ndec->mLocation, "Duplicate type declaration", ndec->mIdent->mString);
			}
		}
		else
		{
			if (variable)
			{
				ndec->mFlags |= storageFlags;
				ndec->mFlags |= ndec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE);

				if (ndec->mBase->mType == DT_TYPE_FUNCTION)
					ndec->mType = DT_CONST_FUNCTION;

				if (ndec->mIdent)
				{
					Declaration* pdec;
					
					if (mGlobals == mScope)
					{
						pdec = mCompilationUnits->mScope->Insert(ndec->mIdent, ndec);
						Declaration	*	ldec = mScope->Insert(ndec->mIdent, pdec ? pdec : ndec);
						if (ldec && ldec != pdec)
							mErrors->Error(ndec->mLocation, "Duplicate definition");
					}
					else
						pdec = mScope->Insert(ndec->mIdent, ndec);

					if (pdec)
					{
						if (pdec->mType == DT_CONST_FUNCTION && ndec->mBase->mType == DT_TYPE_FUNCTION)
						{
							if (!ndec->mBase->IsSame(pdec->mBase))
								mErrors->Error(ndec->mLocation, "Function declaration differs");
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
						else
							mErrors->Error(ndec->mLocation, "Duplicate variable declaration", ndec->mIdent->mString);
					}
				}

				if (mGlobals == mScope)
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
			}
		}

		if (mScanner->mToken == TK_COMMA)
			mScanner->NextToken();
		else if (mScanner->mToken == TK_OPEN_BRACE)
		{
			if (ndec->mBase->mType == DT_TYPE_FUNCTION)
			{
				if (ndec->mFlags & DTF_DEFINED)
					mErrors->Error(ndec->mLocation, "Duplicate function definition");

				ndec->mVarIndex = -1;
				ndec->mValue = ParseFunction(ndec->mBase);
				ndec->mFlags |= DTF_DEFINED;
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
			if (dec->mValue)
			{
				Expression* nexp = new Expression(dec->mValue->mLocation, EX_ASSIGNMENT);
				nexp->mToken = TK_ASSIGN;
				nexp->mLeft = new Expression(dec->mLocation, EX_VARIABLE);
				nexp->mLeft->mDecValue = dec;
				nexp->mLeft->mDecType = dec->mBase;

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
	case TK_CONST:
	case TK_VOLATILE:
	case TK_STRUCT:
	case TK_TYPEDEF:
	case TK_STATIC:
		exp = ParseDeclarationExpression();
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
		dec->mSize = strlen(mScanner->mTokenString) + 1;
		dec->mVarIndex = -1;
		dec->mBase = new Declaration(mScanner->mLocation, DT_TYPE_ARRAY);
		dec->mBase->mSize = dec->mSize;
		dec->mBase->mBase = TheConstSignedCharTypeDeclaration;
		dec->mBase->mFlags |= DTF_DEFINED;
		uint8* d = new uint8[dec->mSize];
		dec->mData = d;
		memcpy(d, mScanner->mTokenString, dec->mSize);

		exp = new Expression(mScanner->mLocation, EX_CONSTANT);
		exp->mDecValue = dec;
		exp->mDecType = dec->mBase;

		mScanner->NextToken();

		// automatic string concatenation
		while (mScanner->mToken == TK_STRING)
		{
			int	s = strlen(mScanner->mTokenString);
			uint8* d = new uint8[dec->mSize + s];
			memcpy(d, dec->mData, dec->mSize - 1);
			memcpy(d + dec->mSize - 1, mScanner->mTokenString, s + 1);
			dec->mSize += s;
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
			if (dec->mType == DT_CONST_INTEGER || dec->mType == DT_CONST_FLOAT || dec->mType == DT_CONST_FUNCTION)
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
				mErrors->Error(mScanner->mLocation, "Invalid identifier", mScanner->mTokenIdent->mString);
				mScanner->NextToken();
			}
		}
		else
		{
			mErrors->Error(mScanner->mLocation, "Unknown identifier", mScanner->mTokenIdent->mString);
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
			mErrors->Error(mScanner->mLocation, "')' expected");

		if (exp->mType == EX_TYPE)
		{
			Expression* nexp = new Expression(mScanner->mLocation, EX_TYPECAST);
			nexp->mDecType = exp->mDecType;
			nexp->mLeft = exp;
			nexp->mRight = ParsePrefixExpression();
			exp = nexp->ConstantFold();
		}
		break;
	default:
		mErrors->Error(mScanner->mLocation, "Term starts with invalid token", TokenNames[mScanner->mToken]);
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
				mErrors->Error(mScanner->mLocation, "Array expected for indexing");
			mScanner->NextToken();
			Expression* nexp = new Expression(mScanner->mLocation, EX_INDEX);
			nexp->mLeft = exp;
			nexp->mRight = ParseExpression();
			if (mScanner->mToken == TK_CLOSE_BRACKET)
				mScanner->NextToken();
			else
				mErrors->Error(mScanner->mLocation, "']' expected");
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
				mErrors->Error(mScanner->mLocation, "Function expected for call");

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

				if (dexp->mDecType->mType == DT_TYPE_STRUCT)
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
							mErrors->Error(mScanner->mLocation, "Struct member identifier not found", mScanner->mTokenIdent->mString);
						mScanner->NextToken();
					}
					else
						mErrors->Error(mScanner->mLocation, "Struct member identifier expected");
				}
				else
					mErrors->Error(mScanner->mLocation, "Struct expected");
			}
			else
				mErrors->Error(mScanner->mLocation, "Pointer expected");
		}
		else if (mScanner->mToken == TK_DOT)
		{
			mScanner->NextToken();
			if (exp->mDecType->mType == DT_TYPE_STRUCT)
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
						mErrors->Error(mScanner->mLocation, "Struct member identifier not found", mScanner->mTokenIdent->mString);
					mScanner->NextToken();
				}
				else
					mErrors->Error(mScanner->mLocation, "Struct member identifier expected");
			}
			else
				mErrors->Error(mScanner->mLocation, "Struct expected");
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
					nexp->mDecType = nexp->mLeft->mDecType;
				else
					mErrors->Error(nexp->mLocation, "Pointer or array type expected");
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
		return nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
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
		exp = nexp->ConstantFold();
	}

	return exp;
}

Expression* Parser::ParseConditionalExpression(void)
{
	Expression* exp = ParseLogicAndExpression();
	
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
		mErrors->Error(mScanner->mLocation, "'(' expected");

	Expression* exp = ParseExpression();

	if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
		mScanner->NextToken();
	else
		mErrors->Error(mScanner->mLocation, "')' expected");

	return exp;
}

Expression* Parser::ParseAssignmentExpression(void)
{
	Expression* exp = ParseLogicOrExpression();

	while (mScanner->mToken >= TK_ASSIGN && mScanner->mToken <= TK_ASSIGN_OR)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_ASSIGNMENT);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseConditionalExpression();
		exp = nexp;
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
				mErrors->Error(mScanner->mLocation, "'}' expected");
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
				mErrors->Error(mScanner->mLocation, "'while' expected");
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
				exp->mLeft->mRight = ParseExpression();
				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, "';' expected");
				exp->mLeft->mLeft->mLeft = ParseExpression();
				if (mScanner->mToken == TK_SEMICOLON)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, "';' expected");
				if (mScanner->mToken != TK_CLOSE_PARENTHESIS)
					exp->mLeft->mLeft->mRight = ParseExpression();
				if (mScanner->mToken == TK_CLOSE_PARENTHESIS)
					mScanner->NextToken();
				else
					mErrors->Error(mScanner->mLocation, "')' expected");
				exp->mRight = ParseStatement();

				mScope = mScope->mParent;
			}
			else
				mErrors->Error(mScanner->mLocation, "'(' expected");
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
					mErrors->Error(mScanner->mLocation, "'}' expected");
			}
			break;
		default:
			exp = ParseExpression();
		}
		if (mScanner->mToken == TK_SEMICOLON)
			mScanner->NextToken();
	}

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
			mErrors->Error(mScanner->mLocation, "')' expected");
	}
	else
		mErrors->Error(mScanner->mLocation, "'(' expected");

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
						mErrors->Error(mScanner->mLocation, "':' expected");

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
						mErrors->Error(mScanner->mLocation, "':' expected");

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
			mErrors->Error(mScanner->mLocation, "'}' expected");
	}
	else
		mErrors->Error(mScanner->mLocation, "'{' expected");

	return sexp;
}



Expression* Parser::ParseAssemblerBaseOperand(void)
{
	Expression* exp = nullptr;
	Declaration* dec;

	switch (mScanner->mToken)
	{
	case TK_SUB:
		mScanner->NextToken();
		exp = ParseAssemblerBaseOperand();
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
			mErrors->Error(exp->mLocation, "Cannot negate expression");
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
				Declaration* ndec = new Declaration(exp->mLocation, DT_CONST_INTEGER);
				ndec->mBase = TheUnsignedIntTypeDeclaration;
				ndec->mInteger = 2 + dec->mVarIndex;
				dec = ndec;
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
						mErrors->Error(mScanner->mLocation, "Assembler label not found", mScanner->mTokenIdent->mString);
				}
				mScanner->NextToken();
			}
			else
				mErrors->Error(mScanner->mLocation, "Identifier for qualification expected");
		}
		break;
	default:
		mErrors->Error(mScanner->mLocation, "Invalid assembler operand");
	}

	if (!exp)
	{
		exp = new Expression(mScanner->mLocation, EX_VOID);
		exp->mDecType = TheVoidTypeDeclaration;
	}

	return exp;
}

Expression* Parser::ParseAssemblerAddOperand(void)
{
	Expression* exp = ParseAssemblerBaseOperand();
	while (mScanner->mToken == TK_ADD || mScanner->mToken == TK_SUB)
	{
		Expression* nexp = new Expression(mScanner->mLocation, EX_BINARY);
		nexp->mToken = mScanner->mToken;
		nexp->mLeft = exp;
		mScanner->NextToken();
		nexp->mRight = ParseAssemblerBaseOperand();
		if (nexp->mLeft->mDecValue->mType == DT_VARIABLE)
		{
			if (nexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_VARIABLE_REF);
				ndec->mBase = nexp->mLeft->mDecValue;
				ndec->mOffset = nexp->mRight->mDecValue->mInteger;
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, "Integer offset expected");
		}
		else if (nexp->mLeft->mDecValue->mType == DT_LABEL)
		{
			if (nexp->mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Declaration* ndec = new Declaration(mScanner->mLocation, DT_LABEL_REF);
				ndec->mBase = nexp->mLeft->mDecValue;
				ndec->mOffset = nexp->mRight->mDecValue->mInteger;
				exp = new Expression(mScanner->mLocation, EX_CONSTANT);
				exp->mDecValue = ndec;
			}
			else
				mErrors->Error(mScanner->mLocation, "Integer offset expected");
		}
		else
			exp = nexp->ConstantFold();
	}
	return exp;
}

Expression* Parser::ParseAssemblerOperand(void)
{
	if (mScanner->mToken == TK_LESS_THAN)
	{
		mScanner->NextToken();
		Expression* exp = ParseAssemblerOperand();

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
			else
				mErrors->Error(mScanner->mLocation, "Label or integer value for lower byte operator expected");
		}
		else
			mErrors->Error(mScanner->mLocation, "Constant for lower byte operator expected");

		return exp;
	}
	else if (mScanner->mToken == TK_GREATER_THAN)
	{
		mScanner->NextToken();
		Expression* exp = ParseAssemblerOperand();

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
			else
				mErrors->Error(mScanner->mLocation, "Label or integer value for lower byte operator expected");
		}
		else
			mErrors->Error(mScanner->mLocation, "Constant for upper byte operator expected");
	}
	else
		return ParseAssemblerAddOperand();
}

Expression* Parser::ParseAssembler(void)
{
	DeclarationScope* scope = new DeclarationScope(mScope);
	mScope = scope;

	mScanner->SetAssemblerMode(true);

	Declaration* decfp = new Declaration(mScanner->mLocation, DT_CONST_INTEGER);
	decfp->mIdent = Ident::Unique("fp");
	decfp->mBase = TheUnsignedIntTypeDeclaration;
	decfp->mSize = 2;
	decfp->mInteger = BC_REG_LOCALS;
	mScope->Insert(decfp->mIdent, decfp);

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

	vdasm->mBase = dassm;

	Expression* ifirst = new Expression(mScanner->mLocation, EX_ASSEMBLER);
	Expression* ilast = ifirst;

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
					mErrors->Error(mScanner->mLocation, "':' expected");
				else
					mScanner->NextToken();

				Declaration* dec = mScope->Lookup(label);
				if (dec)
				{
					if (dec->mType != DT_LABEL || dec->mBase)
						mErrors->Error(mScanner->mLocation, "Duplicate label definition");
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
				ilast->mAsmInsType = ins;
				mScanner->NextToken();
				if (mScanner->mToken == TK_EOL)
					ilast->mAsmInsMode = ASMIM_IMPLIED;
				else if (mScanner->mToken == TK_HASH)
				{
					ilast->mAsmInsMode = ASMIM_IMMEDIATE;
					mScanner->NextToken();
					ilast->mLeft = ParseAssemblerOperand();
				}
				else if (mScanner->mToken == TK_OPEN_PARENTHESIS)
				{
					mScanner->NextToken();
					ilast->mLeft = ParseAssemblerOperand();
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
								mErrors->Error(mScanner->mLocation, "')' expected");
						}
						else
							mErrors->Error(mScanner->mLocation, "',x' expected");
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
								mErrors->Error(mScanner->mLocation, "',y' expected");
						}
						else
						{
							ilast->mAsmInsMode = ASMIM_INDIRECT;
						}
					}
					else
						mErrors->Error(mScanner->mLocation, "',' or ')' expected");
				}
				else
				{
					ilast->mLeft = ParseAssemblerOperand();
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
							mErrors->Error(mScanner->mLocation, "',x' or ',y' expected");
					}
					else
					{
						if (HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_RELATIVE))
							ilast->mAsmInsMode = ASMIM_RELATIVE;
						else
							ilast->mAsmInsMode = ASMIM_ABSOLUTE;
					}
				}

				if (ilast->mLeft && ilast->mLeft->mDecValue && ilast->mLeft->mDecValue->mType == DT_CONST_INTEGER && ilast->mLeft->mDecValue->mInteger < 256)
				{
					if (ilast->mAsmInsMode == ASMIM_ABSOLUTE && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE))
						ilast->mAsmInsMode = ASMIM_ZERO_PAGE;
					else if (ilast->mAsmInsMode == ASMIM_ABSOLUTE_X && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE_X))
						ilast->mAsmInsMode = ASMIM_ZERO_PAGE_X;
					else if (ilast->mAsmInsMode == ASMIM_ABSOLUTE_Y && HasAsmInstructionMode(ilast->mAsmInsType, ASMIM_ZERO_PAGE_Y))
						ilast->mAsmInsMode = ASMIM_ZERO_PAGE_Y;
				}

				if (mScanner->mToken != TK_EOL)
				{
					mErrors->Error(mScanner->mLocation, "End of line expected");
				}

				while (mScanner->mToken != TK_EOL && mScanner->mToken != TK_EOF)
					mScanner->NextToken();

				offset += AsmInsSize(ilast->mAsmInsType, ilast->mAsmInsMode);

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
			mErrors->Error(mScanner->mLocation, "Invalid assembler token");

			while (mScanner->mToken != TK_EOL && mScanner->mToken != TK_EOF)
				mScanner->NextToken();
		 }
	}

	ilast->mAsmInsType = ASMIT_RTS;
	ilast->mAsmInsMode = ASMIM_IMPLIED;

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
		mErrors->Error(mScanner->mLocation, buffer);
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
					mErrors->Error(mScanner->mLocation, "Intrinsic function not found");
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
				if (dec && dec->mType == DT_CONST_FUNCTION)
					dec->mFlags |= DTF_NATIVE;
				else
					mErrors->Error(mScanner->mLocation, "Native function not found");
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "startup"))
		{
			if (mCompilationUnits->mStartup)
				mErrors->Error(mScanner->mLocation, "Duplicate startup pragma");

			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			if (mScanner->mToken == TK_IDENT)
			{
				Declaration* dec = mGlobals->Lookup(mScanner->mTokenIdent);
				if (dec && dec->mType == DT_CONST_ASSEMBLER)
					mCompilationUnits->mStartup = dec;
				else
					mErrors->Error(mScanner->mLocation, "Startup function not found");
				mScanner->NextToken();
			}
			ConsumeToken(TK_CLOSE_PARENTHESIS);
		}
		else if (!strcmp(mScanner->mTokenIdent->mString, "bytecode"))
		{
			mScanner->NextToken();
			ConsumeToken(TK_OPEN_PARENTHESIS);
			Expression* exp = ParseAssemblerOperand();

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
								mErrors->Error(mScanner->mLocation, "Label not found in assembler code");
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, "Identifier expected");
					}

					if (exp->mType == EX_CONSTANT && exp->mDecValue->mType == DT_CONST_INTEGER && exp->mDecValue->mInteger >= 0 && exp->mDecValue->mInteger < 128)
					{
						if (mCompilationUnits->mByteCodes[exp->mDecValue->mInteger])
							mErrors->Error(mScanner->mLocation, "Duplicate bytecode function");

						mCompilationUnits->mByteCodes[exp->mDecValue->mInteger] = dec;
					}
					else
						mErrors->Error(exp->mLocation, "Numeric value for byte code expected");
				}
				else
				{
					mErrors->Error(mScanner->mLocation, "Bytecode function not found");
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
				mErrors->Error(mScanner->mLocation, "Identifier expected");

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
								mErrors->Error(mScanner->mLocation, "Label not found in assembler code");
							mScanner->NextToken();
						}
						else
							mErrors->Error(mScanner->mLocation, "Identifier expected");
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
					mErrors->Error(mScanner->mLocation, "Runtime function not found");
					mScanner->NextToken();
				}

			}
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
			mErrors->Error(mScanner->mLocation, "Unknown pragma, ignored", mScanner->mTokenIdent->mString);
		}
	}
	else
		mErrors->Error(mScanner->mLocation, "Invalid pragma directive");
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
						mErrors->Error(mScanner->mLocation, "'}' expected");
				}
			}
			else
				mErrors->Error(mScanner->mLocation, "Identifier expected");
		}
		else if (mScanner->mToken == TK_SEMICOLON)
			mScanner->NextToken();
		else
			ParseDeclaration(true);
	}
}

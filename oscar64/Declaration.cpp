#include "Declaration.h"
#include "Constexpr.h"
#include "Linker.h"
#include <math.h>

DeclarationScope::DeclarationScope(DeclarationScope* parent, ScopeLevel level, const Ident* name)
{
	mParent = parent;
	mLevel = level;
	mName = name;
	mHashSize = 0;
	mHashFill = 0;
	mHash = nullptr;
}

void DeclarationScope::Clear(void)
{
	mHashFill = 0;
	if (mHash)
	{
		for (int i = 0; i < mHashSize; i++)
		{
			mHash[i].mDec = nullptr;
			mHash[i].mIdent = nullptr;
		}
	}
}

DeclarationScope::~DeclarationScope(void)
{
	delete[] mHash;
}

const Ident* DeclarationScope::Mangle(const Ident* ident) const
{
	if (mName && ident)
	{
		char	buffer[200];
		strcpy_s(buffer, mName->mString);
		strcat_s(buffer, "::");
		strcat_s(buffer, ident->mString);
		return Ident::Unique(buffer);
	}
	else
		return ident;
}

void DeclarationScope::UseScope(DeclarationScope* scope)
{
	mUsed.Push(scope);
}

Declaration * DeclarationScope::Insert(const Ident* ident, Declaration* dec)
{
	if (!mHash)
	{
		mHashSize = 16;
		mHashFill = 0;
		mHash = new Entry[mHashSize];
		for (int i = 0; i < mHashSize; i++)
		{
			mHash[i].mDec = nullptr;
			mHash[i].mIdent = nullptr;
		}
	}

	int		hm = mHashSize - 1;
	int		hi = ident->mHash & hm;

	while (mHash[hi].mIdent)
	{
		if (ident == mHash[hi].mIdent)
			return mHash[hi].mDec;
		hi = (hi + 1) & hm;
	}

	mHash[hi].mIdent = ident;
	mHash[hi].mDec = dec;
	mHashFill++;

	if (2 * mHashFill >= mHashSize)
	{
		int		size  = mHashSize;
		Entry* entries = mHash;
		mHashSize *= 2;
		mHashFill = 0;
		mHash = new Entry[mHashSize];
		for (int i = 0; i < mHashSize; i++)
		{
			mHash[i].mDec = nullptr;
			mHash[i].mIdent = nullptr;
		}
		for (int i = 0; i < size; i++)
		{
			if (entries[i].mIdent)
				Insert(entries[i].mIdent, entries[i].mDec);
		}
		delete[] entries;
	}

	return nullptr;
}

Declaration* DeclarationScope::Lookup(const Ident* ident, ScopeLevel limit)
{
	if (mLevel < limit)
		return nullptr;

	if (mHashSize > 0)
	{
		unsigned int		hm = mHashSize - 1;
		unsigned int		hi = ident->mHash & hm;

		while (mHash[hi].mIdent)
		{
			if (ident == mHash[hi].mIdent)
				return mHash[hi].mDec;
			hi = (hi + 1) & hm;
		}
	}

	if (limit == SLEVEL_SCOPE)
		return nullptr;

	for (int i = 0; i < mUsed.Size(); i++)
	{
		Declaration* dec = mUsed[i]->Lookup(ident, SLEVEL_NAMESPACE);
		if (dec)
			return dec;
	}

	if (limit == SLEVEL_USING)
		return nullptr;

	return mParent ? mParent->Lookup(ident, limit) : nullptr;
}

void DeclarationScope::End(const Location& loc)
{
	for (int i = 0; i < mHashSize; i++)
	{
		if (mHash[i].mDec)
			mHash[i].mDec->mEndLocation = loc;
	}
}

Expression::Expression(const Location& loc, ExpressionType type)
	:	mLocation(loc), mEndLocation(loc), mType(type), mLeft(nullptr), mRight(nullptr), mConst(false), mDecType(nullptr), mDecValue(nullptr), mToken(TK_NONE)
{
	static uint32	gUID = 0;
	mUID = gUID++;
}

Expression::~Expression(void)
{

}

Expression* Expression::ListAppend(Expression* lexp)
{
	if (lexp)
	{
		Expression* nexp = new Expression(mLocation, EX_LIST);
		nexp->mLeft = lexp;
		nexp->mRight = this;
		nexp->mDecType = mDecType;

		return nexp;
	}
	else
		return this;
}

void Expression::Dump(int ident) const
{
	for (int i = 0; i < ident; i++)
		printf("|  ");

	switch (mType)
	{
	case EX_ERROR:
		printf("ERROR");
		break;
	case EX_VOID:
		printf("VOID");
		break;
	case EX_CONSTANT:
		printf("CONST");
		if (mDecValue->mIdent)
			printf(" '%s'", mDecValue->mIdent->mString);
		break;
	case EX_VARIABLE:
		printf("VAR");
		if (mDecValue->mIdent)
			printf(" '%s'", mDecValue->mIdent->mString);
		break;
	case EX_ASSIGNMENT:
		printf("ASSIGN<%s>", TokenNames[mToken]);
		break;
	case EX_INITIALIZATION:
		printf("INIT");
		break;
	case EX_BINARY:
		printf("BINARY<%s>", TokenNames[mToken]);
		break;
	case EX_RELATIONAL:
		printf("RELATIONAL<%s>", TokenNames[mToken]);
		break;
	case EX_PREINCDEC:
		printf("PREOP<%s>", TokenNames[mToken]);
		break;
	case EX_PREFIX:
		printf("PREFIX<%s>", TokenNames[mToken]);
		break;
	case EX_POSTFIX:
		printf("POSTFIX<%s>", TokenNames[mToken]);
		break;
	case EX_POSTINCDEC:
		printf("POSTOP<%s>", TokenNames[mToken]);
		break;
	case EX_INDEX:
		printf("INDEX");
		break;
	case EX_QUALIFY:
		if (mDecValue->mIdent)
			printf("QUALIFY<%s>", mDecValue->mIdent->mString);
		else
			printf("QUALIFY<%d>", mDecValue->mOffset);
		break;
	case EX_CALL:
		printf("CALL");
		break;
	case EX_INLINE:
		printf("INLINE");
		break;
	case EX_VCALL:
		printf("VCALL");
		break;
	case EX_DISPATCH:
		printf("DISPATCH");
		break;
	case EX_LIST:
		printf("LIST");
		break;
	case EX_COMMA:
		printf("COMMA");
		break;
	case EX_RETURN:
		printf("RETURN");
		break;
	case EX_SEQUENCE:
		printf("SEQUENCE");
		break;
	case EX_WHILE:
		printf("WHILE");
		break;
	case EX_IF:
		printf("IF");
		break;
	case EX_ELSE:
		printf("ELSE");
		break;
	case EX_FOR:
		printf("FOR");
		break;
	case EX_DO:
		printf("DO");
		break;
	case EX_SCOPE:
		printf("SCOPE");
		break;
	case EX_BREAK:
		printf("BREAK");
		break;
	case EX_CONTINUE:
		printf("CONTINUE");
		break;
	case EX_TYPE:
		printf("TYPE");
		break;
	case EX_TYPECAST:
		printf("TYPECAST");
		break;
	case EX_LOGICAL_AND:
		printf("AND");
		break;
	case EX_LOGICAL_OR:
		printf("OR");
		break;
	case EX_LOGICAL_NOT:
		printf("NOT");
		break;
	case EX_ASSEMBLER:
		printf("ASSEMBLER");
		break;
	case EX_UNDEFINED:
		printf("UNDEFINED");
		break;
	case EX_SWITCH:
		printf("SWITCH");
		break;
	case EX_CASE:
		printf("CASE");
		break;
	case EX_DEFAULT:
		printf("DEFAULT");
		break;
	case EX_CONDITIONAL:
		printf("COND");
		break;
	case EX_ASSUME:
		printf("ASSUME");
		break;
	case EX_BANKOF:
		printf("BANKOF");
		break;
	case EX_CONSTRUCT:
		printf("CONSTRUCT");
		break;
	case EX_CLEANUP:
		printf("CLEANUP");
		break;
	case EX_RESULT:
		printf("RESULT");
		break;
	case EX_PACK:
		printf("PACK");
		break;
	case EX_PACK_TYPE:
		printf("PACK_TYPE");
		break;
	case EX_GOTO:
		printf("GOTO %s", mDecValue->mIdent->mString);
		break;
	case EX_LABEL:
		printf("LABEL %s", mDecValue->mIdent->mString);
		break;
	}
	printf("\n");

	if (mLeft)
		mLeft->Dump(ident + 1);
	if (mRight)
		mRight->Dump(ident + 1);
}

bool Expression::HasSideEffects(void) const
{
	switch (mType)
	{
	case EX_VARIABLE:
	case EX_CONSTANT:
		return false;

	case EX_BINARY:
	case EX_RELATIONAL:
	case EX_INDEX:
		return mLeft->HasSideEffects() || mRight->HasSideEffects();

	case EX_QUALIFY:
	case EX_TYPECAST:
	case EX_PREFIX:
	case EX_POSTFIX:
		return mLeft->HasSideEffects();

	default:
		return true;
	}
}

bool Expression::IsRValue(void) const
{
	if (mDecType->mType == DT_TYPE_RVALUEREF)
		return true;
	else if (mDecType->mType == DT_TYPE_REFERENCE)
		return false;
	else if (mType == EX_VARIABLE)
	{
		if (mDecValue->mFlags & DTF_TEMPORARY)
			return true;
		else
			return false;
	}
	else if (mType == EX_QUALIFY || mType == EX_INDEX || mType == EX_ASSIGNMENT || mType == EX_CONSTANT || mType == EX_PREFIX && mToken == TK_MUL)
		return false;
	else
		return true;
}

bool Expression::IsConstRef(void) const
{
	if (mDecType->mType == DT_TYPE_RVALUEREF || mDecType->mType == DT_TYPE_REFERENCE)
		return true;
	else if (mType == EX_VARIABLE || mType == EX_QUALIFY || mType == EX_INDEX || mType == EX_PREFIX && mToken == TK_MUL)
		return true;
	else
		return false;
}

bool Expression::IsLValue(void) const
{
	if (mDecType->mFlags & DTF_CONST)
		return false;
	else if (mDecType->mType == DT_TYPE_RVALUEREF || mDecType->mType == DT_TYPE_REFERENCE)
		return true;
	else if (mType == EX_VARIABLE || mType == EX_QUALIFY || mType == EX_INDEX || mType == EX_PREFIX && mToken == TK_MUL)
		return true;
	else
		return false;
}

bool Expression::IsSame(const Expression* exp) const
{
	if (!exp || mType != exp->mType)
		return false;

	switch (mType)
	{
	case EX_VARIABLE:
		return mDecValue == exp->mDecValue;

	default:
		return false;
	}
}

Expression* Expression::LogicInvertExpression(void) 
{
	if (mType == EX_LOGICAL_NOT)
		return mLeft;
	else if (mType == EX_LOGICAL_AND)
	{
		mType = EX_LOGICAL_OR;
		mLeft = mLeft->LogicInvertExpression();
		mRight = mRight->LogicInvertExpression();
		return this;
	}
	else if (mType == EX_LOGICAL_OR)
	{
		mType = EX_LOGICAL_AND;
		mLeft = mLeft->LogicInvertExpression();
		mRight = mRight->LogicInvertExpression();
		return this;
	}
	else if (mType == EX_RELATIONAL)
	{
		switch (mToken)
		{
		case TK_EQUAL:
			mToken = TK_NOT_EQUAL;
			break;
		case TK_NOT_EQUAL:
			mToken = TK_EQUAL;
			break;
		case TK_GREATER_THAN:
			mToken = TK_LESS_EQUAL;
			break;
		case TK_GREATER_EQUAL:
			mToken = TK_LESS_THAN;
			break;
		case TK_LESS_THAN:
			mToken = TK_GREATER_EQUAL;
			break;
		case TK_LESS_EQUAL:
			mToken = TK_GREATER_THAN;
			break;
		}

		return this;
	}
	else
	{
		Expression* ex = new Expression(mLocation, EX_LOGICAL_NOT);
		ex->mLeft = this;
		ex->mDecType = TheBoolTypeDeclaration;
		return ex;
	}
}

Expression* Expression::ConstantDereference(Errors* errors, LinkerSection* dataSection)
{
	if (mType == EX_VARIABLE)
	{
		if (mDecType->IsSimpleType())
		{
			if (mDecValue->mType == DT_VARIABLE && (mDecValue->mFlags & DTF_CONST) && mDecValue->mValue)
				return mDecValue->mValue;
			else if (mDecValue->mType == DT_VARIABLE_REF && (mDecValue->mFlags & DTF_CONST) && mDecValue->mBase->mValue && mDecValue->mBase->mValue->mType == EX_CONSTANT && mDecValue->mBase->mValue->mDecValue->mType == DT_CONST_STRUCT)
			{
				Declaration* mdec = mDecValue->mBase->mValue->mDecValue->mParams;
				int	coffset = mDecValue->mOffset;
				while (mdec)
				{
					if (mdec->mType == DT_CONST_STRUCT)
					{
						if (mdec->mOffset > coffset || mdec->mOffset + mdec->mSize <= coffset)
							mdec = mdec->mNext;
						else
						{
							coffset -= mdec->mOffset;
							mdec = mdec->mParams;
						}
					}
					else if (mdec->mOffset != coffset)
						mdec = mdec->mNext;
					else
					{
						Expression* ex = new Expression(mLocation, EX_CONSTANT);
						ex->mDecValue = mdec;
						ex->mDecType = mDecType;
						return ex;
					}
				}
			}
		}
	}
	else if (mType == EX_BINARY)
	{
		Expression* lexp = mLeft->ConstantDereference(errors, dataSection);
		Expression* rexp = mRight->ConstantDereference(errors, dataSection);
		if (lexp != mLeft || rexp != mRight)
		{
			Expression* nexp = new Expression(mLocation, EX_BINARY);
			nexp->mToken = mToken;
			nexp->mDecType = mDecType;
			nexp->mLeft = lexp;
			nexp->mRight = rexp;
			return nexp->ConstantFold(errors, dataSection)->ConstantDereference(errors, dataSection);
		}
	}

	return this;
}

static int64 signextend(int64 v, Declaration* type)
{
	if (type->mFlags & DTF_SIGNED)
	{
		switch (type->mSize)
		{
		case 1:
			if (v & 0x80)
				return (v & 0xff) - 0x100;
			else
				return v;
		case 2:
			if (v & 0x8000)
				return (v & 0xffff) - 0x10000;
			else
				return v;
		default:
			return v;
		}

	}
	else
	{
		switch (type->mSize)
		{
		case 1:
			return v & 0xff;
		case 2:
			return v & 0xffff;
		default:
			return v & 0xffffffff;
		}
	}
}

Expression* Expression::ConstantFold(Errors * errors, LinkerSection * dataSection, Linker* linker)
{
	if (mType == EX_PREFIX && mToken == TK_BANKOF && linker)
	{
		LinkerRegion* rgn;
		if (mLeft->mDecValue && mLeft->mDecValue->mSection && (rgn = linker->FindRegionOfSection(mLeft->mDecValue->mSection)))
		{
			uint64	i = 0;
			while (i < 64 && rgn->mCartridgeBanks != (1ULL << i))
				i++;
			if (i >= 64)
				i = 0xff;
			
			if (i < 64 || (mLeft->mDecValue->mFlags & DTF_DEFINED))
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = TheSignedIntTypeDeclaration;
				dec->mInteger = i;
				ex->mDecValue = dec;
				ex->mDecType = dec->mBase;
				return ex;
			}
		}

		return this;
	}
	else if (mType == EX_PREFIX && mLeft->mType == EX_CONSTANT)
	{

		if (mLeft->mDecValue->mType == DT_CONST_INTEGER)
		{
			switch (mToken)
			{
			case TK_ADD:
				return mLeft;
			case TK_SUB:
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration	*	dec = new Declaration(mLocation, DT_CONST_INTEGER);

				if (mLeft->mDecValue->mBase->mSize < 2)
					dec->mBase = TheSignedIntTypeDeclaration;
				else
					dec->mBase = mLeft->mDecValue->mBase;

				dec->mInteger = signextend ( - mLeft->mDecValue->mInteger, dec->mBase );

				ex->mDecValue = dec;
				ex->mDecType = dec->mBase;
				return ex;
			}
			case TK_BINARY_NOT:
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);

				if (mLeft->mDecValue->mBase->mSize < 2)
					dec->mBase = TheSignedIntTypeDeclaration;
				else
					dec->mBase = mLeft->mDecValue->mBase;

				dec->mInteger = signextend( ~mLeft->mDecValue->mInteger, dec->mBase );

				ex->mDecValue = dec;
				ex->mDecType = dec->mBase;
				return ex;
			}

			}
		}
		else if (mLeft->mDecValue->mType == DT_CONST_FLOAT)
		{
			switch (mToken)
			{
			case TK_ADD:
				return mLeft;
			case TK_SUB:
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_FLOAT);
				dec->mBase = TheFloatTypeDeclaration;
				dec->mNumber = -mLeft->mDecValue->mNumber;
				ex->mDecValue = dec;
				ex->mDecType = TheFloatTypeDeclaration;
				return ex;
			}

			}
		}
		else if (mLeft->mDecValue->mType == DT_CONST_FUNCTION)
		{
			switch (mToken)
			{
			case TK_BINARY_AND:
				return mLeft;
			}
		}
	}
#if 1
	else if (mType == EX_PREFIX && mToken == TK_BINARY_AND && mLeft->mType == EX_VARIABLE && (mLeft->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)))
	{
		Expression* ex = new Expression(mLocation, EX_CONSTANT);
		Declaration* dec = new Declaration(mLocation, DT_CONST_POINTER);
		dec->mValue = mLeft;
		dec->mBase = mDecType;
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}
	else if (mType == EX_PREFIX && mToken == TK_BINARY_AND && mLeft->mType == EX_INDEX && mLeft->mLeft->mType == EX_VARIABLE && (mLeft->mLeft->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)) && mLeft->mRight->mType == EX_CONSTANT)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mLeft->mDecValue;
		dec->mSize = mLeft->mLeft->mDecType->mBase->mSize - int(mLeft->mRight->mDecValue->mInteger) * dec->mSize;
		dec->mOffset = int(mLeft->mRight->mDecValue->mInteger) * dec->mSize;
		ex->mDecValue = dec;
		ex->mDecType = mLeft->mLeft->mDecType;
		return ex;
	}
	else if (mType == EX_PREFIX && mToken == TK_BINARY_AND && mLeft->mType == EX_PREFIX && mLeft->mToken == TK_MUL)
	{
		return mLeft->mLeft;
	}
#endif
#if 1
	else if (mType == EX_PREFIX && mToken == TK_BINARY_AND && 
			 mLeft->mType == EX_QUALIFY &&
			 mLeft->mLeft->mType == EX_PREFIX && mLeft->mLeft->mToken == TK_MUL &&
			 mLeft->mLeft->mLeft->mType == EX_CONSTANT)
	{
		Expression* bex = mLeft->mLeft->mLeft;
		if (bex->mDecValue->mType == DT_CONST_ADDRESS)
		{
			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_ADDRESS);
			dec->mBase = mDecType;
			dec->mInteger = bex->mDecValue->mInteger + mLeft->mDecValue->mOffset;
			ex->mDecValue = dec;
			ex->mDecType = mDecType;
			return ex;
		}
		return this;
	}
#endif
	else if (mType == EX_TYPECAST && mLeft->mType == EX_CONSTANT)
	{
		if (mDecType->mType == DT_TYPE_POINTER)
		{
			if (mLeft->mDecValue->mType == DT_CONST_ADDRESS || mLeft->mDecValue->mType == DT_CONST_INTEGER)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_ADDRESS);
				dec->mBase = mDecType;
				dec->mInteger = mLeft->mDecValue->mInteger;
				ex->mDecValue = dec;
				ex->mDecType = mDecType;
				return ex;
			}
			else if (mLeft->mDecValue->mType == DT_CONST_FUNCTION)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				ex->mDecValue = mLeft->mDecValue;
				ex->mDecType = mDecType;
				return ex;
			}
			else if (mLeft->mDecValue->mType == DT_CONST_POINTER)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				ex->mDecValue = mLeft->mDecValue;
				ex->mDecType = mDecType;
				return ex;
			}
		}
		else if (mDecType->mType == DT_TYPE_INTEGER)
		{
			if (mLeft->mDecValue->mType == DT_CONST_FLOAT)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = mDecType;
				dec->mInteger = int64(mLeft->mDecValue->mNumber);
				ex->mDecValue = dec;
				ex->mDecType = mDecType;
				return ex;
			}
			else if (mLeft->mDecValue->mType == DT_CONST_INTEGER || mLeft->mDecValue->mType == DT_CONST_ADDRESS)
			{
				int64	sval = 1ULL << (8 * mDecType->mSize);
				int64	v = mLeft->mDecValue->mInteger & (sval - 1);

				if (mDecType->mFlags & DTF_SIGNED)
				{
					if (v & (sval >> 1))
						v -= sval;
				}

				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = mDecType;
				dec->mInteger = v;
				ex->mDecValue = dec;
				ex->mDecType = mDecType;
				return ex;
			}
		}
		else if (mDecType->mType == DT_TYPE_FLOAT)
		{
			if (mLeft->mDecValue->mType == DT_CONST_INTEGER)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_FLOAT);
				dec->mBase = mDecType;
				dec->mNumber = double(mLeft->mDecValue->mInteger);
				ex->mDecValue = dec;
				ex->mDecType = mDecType;
				return ex;
			}
		}
	}
	else if (mType == EX_TYPECAST && mLeft->mType == EX_VARIABLE)
	{
		if (mDecType->mType == DT_TYPE_POINTER && (mLeft->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)) && mLeft->mDecType->mType == DT_TYPE_ARRAY)
		{
			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_POINTER);
			dec->mValue = mLeft;
			dec->mBase = mDecType;
			ex->mDecValue = dec;
			ex->mDecType = mDecType;
			return ex;
		}
	}
	else if (mType == EX_BINARY && mLeft->mType == EX_CONSTANT && mRight->mType == EX_CONSTANT)
	{
		if (mLeft->mDecValue->mType == DT_CONST_INTEGER && mRight->mDecValue->mType == DT_CONST_INTEGER)
		{
			int64	ival = 0, ileft = mLeft->mDecValue->mInteger, iright = mRight->mDecValue->mInteger;

			bool	signop =
				(mLeft->mDecValue->mBase->mSize < 2 || (mLeft->mDecValue->mBase->mFlags & DTF_SIGNED)) &&
				(mRight->mDecValue->mBase->mSize < 2 || (mRight->mDecValue->mBase->mFlags & DTF_SIGNED));

			bool	promote = true;
			switch (mToken)
			{
			case TK_ADD:
				ival = ileft + iright;
				break;
			case TK_SUB:
				ival = ileft - iright;
				break;
			case TK_MUL:
				ival = ileft * iright;
				break;
			case TK_DIV:
				if (iright == 0)
					errors->Error(mLocation, EERR_INVALID_VALUE, "Constant division by zero");
				else if (signop)
					ival = ileft / iright;
				else
					ival = (uint64)ileft / (uint64)iright;
				break;
			case TK_MOD:
				if (iright == 0)
					errors->Error(mLocation, EERR_INVALID_VALUE, "Constant division by zero");
				else if (signop)
					ival = ileft % iright;
				else
					ival = (uint64)ileft % (uint64)iright;
				break;
			case TK_LEFT_SHIFT:
				ival = ileft << iright;
				promote = false;
				break;
			case TK_RIGHT_SHIFT:
				ival = ileft >> iright;
				promote = false;
				break;
			case TK_BINARY_AND:
				ival = ileft & iright;
				break;
			case TK_BINARY_OR:
				ival = ileft | iright;
				break;
			case TK_BINARY_XOR:
				ival = ileft ^ iright;
				break;
			default:
				ival = 0;
			}

			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
			if (promote)
			{
				if (mLeft->mDecValue->mBase->mSize <= 2 && mRight->mDecValue->mBase->mSize <= 2)
					dec->mBase = ival < 32768 ? TheSignedIntTypeDeclaration : TheUnsignedIntTypeDeclaration;
				else
					dec->mBase = ival < 2147483648 ? TheSignedLongTypeDeclaration : TheUnsignedLongTypeDeclaration;
			}
			else
			{
				if (mLeft->mDecValue->mBase->mSize < 2)
					dec->mBase = TheSignedIntTypeDeclaration;
				else
					dec->mBase = mLeft->mDecValue->mBase;
			}

			dec->mInteger = ival;
			ex->mDecValue = dec;
			ex->mDecType = dec->mBase;
			return ex;
		}
		else if ((mLeft->mDecValue->mType == DT_CONST_INTEGER || mLeft->mDecValue->mType == DT_CONST_FLOAT) && (mRight->mDecValue->mType == DT_CONST_INTEGER || mRight->mDecValue->mType == DT_CONST_FLOAT))
		{

			double	dval;
			double	dleft = mLeft->mDecValue->mType == DT_CONST_INTEGER ? mLeft->mDecValue->mInteger : mLeft->mDecValue->mNumber;
			double	dright = mRight->mDecValue->mType == DT_CONST_INTEGER ? mRight->mDecValue->mInteger : mRight->mDecValue->mNumber;

			switch (mToken)
			{
			case TK_ADD:
				dval = dleft + dright;
				break;
			case TK_SUB:
				dval = dleft - dright;
				break;
			case TK_MUL:
				dval = dleft * dright;
				break;
			case TK_DIV:
				dval = dleft / dright;
				break;
			default:
				dval = 0;
			}

			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_FLOAT);
			dec->mBase = TheFloatTypeDeclaration;
			dec->mNumber = dval;
			ex->mDecValue = dec;
			ex->mDecType = dec->mBase;
			return ex;
		}
		else if (mLeft->mDecValue->mType == DT_CONST_ADDRESS && mRight->mDecValue->mType == DT_CONST_INTEGER)
		{
			int64	ival = 0, ileft = mLeft->mDecValue->mInteger, iright = mRight->mDecValue->mInteger;

			switch (mToken)
			{
			case TK_ADD:
				ival = ileft + iright;
				break;
			case TK_SUB:
				ival = ileft - iright;
				break;
			default:
				return this;
			}

			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_ADDRESS);
			dec->mBase = mLeft->mDecType;
			dec->mInteger = ival;
			ex->mDecValue = dec;
			ex->mDecType = dec->mBase;
			return ex;
		}
#if 0
		else if (mLeft->mDecValue->mType == DT_CONST_POINTER && mRight->mDecValue->mType == DT_CONST_INTEGER && (mToken == TK_ADD || mToken == TK_SUB))
		{
			int64	ileft = 0;

			Declaration* pdec = mLeft->mDecValue->mValue->mDecValue;
			if (pdec->mType == DT_VARIABLE_REF)
			{
				ileft = pdec->mOffset;
				pdec = pdec->mBase;
			}

			switch (mToken)
			{
			case TK_ADD:
				ileft += mRight->mDecValue->mInteger * mLeft->mDecType->mBase->mSize;
				break;
			case TK_SUB:
				ileft -= mRight->mDecValue->mInteger * mLeft->mDecType->mBase->mSize;
				break;
			}

			Expression* vex = new Expression(mLocation, EX_VARIABLE);
			Declaration* vdec = new Declaration(mLocation, DT_VARIABLE_REF);
			vdec->mFlags = pdec->mFlags;
			vdec->mBase = pdec;
			vdec->mSize = pdec->mBase->mSize;			
			vdec->mOffset = ileft;
			vex->mDecValue = vdec;
			vex->mDecType = mLeft->mDecType->mBase;

			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_POINTER);
			dec->mBase = mLeft->mDecValue->mBase;
			dec->mValue = vex;
			ex->mDecValue = dec;
			ex->mDecType = dec->mBase;
			return ex;
		}
#endif
	}
	else if (mType == EX_RELATIONAL && mLeft->mType == EX_CONSTANT && mRight->mType == EX_CONSTANT)
	{
		if (mLeft->mDecValue->mType == DT_CONST_INTEGER && mRight->mDecValue->mType == DT_CONST_INTEGER)
		{
			int64	ival = 0, ileft = mLeft->mDecValue->mInteger, iright = mRight->mDecValue->mInteger;

			bool	check = false;
			switch (mToken)
			{
			case TK_EQUAL:
				check = ileft == iright;
				break;
			case TK_NOT_EQUAL:
				check = ileft != iright;
				break;
			case TK_GREATER_THAN:
				check = ileft > iright;
				break;
			case TK_GREATER_EQUAL:
				check = ileft >= iright;
				break;
			case TK_LESS_THAN:
				check = ileft < iright;
				break;
			case TK_LESS_EQUAL:
				check = ileft <= iright;
				break;
			}

			Declaration	*	dec = new Declaration(mLocation, DT_CONST_INTEGER);
			dec->mBase = TheBoolTypeDeclaration;
			dec->mInteger = check ? 1 : 0;
			Expression	*	exp = new Expression(mLocation, EX_CONSTANT);
			exp->mDecValue = dec;
			exp->mDecType = dec->mBase;
			return exp;

		}
	}
	else if (mType == EX_CONDITIONAL && mLeft->mType == EX_CONSTANT)
	{
		if (mLeft->mDecValue->mType == DT_CONST_INTEGER)
		{
			if (mLeft->mDecValue->mInteger != 0)
				return mRight->mLeft->ConstantFold(errors, dataSection);
			else
				return mRight->mRight->ConstantFold(errors, dataSection);
		}
	}
	else if (mType == EX_BINARY && mToken == TK_ADD && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_ARRAY && mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue;
		dec->mSize = mLeft->mDecType->mBase->mSize - int(mRight->mDecValue->mInteger) * dec->mSize;
		dec->mOffset = int(mRight->mDecValue->mInteger) * dec->mSize;
		ex->mDecValue = dec;
		ex->mDecType = mLeft->mDecType;
		return ex;
	}
	else if (mType == EX_BINARY && mToken == TK_ADD && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE_REF && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_ARRAY && mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue->mBase;
		dec->mSize = mLeft->mDecType->mBase->mSize - int(mRight->mDecValue->mInteger) * dec->mSize;
		dec->mOffset = mLeft->mDecValue->mOffset + int(mRight->mDecValue->mInteger) * dec->mSize;
		ex->mDecValue = dec;
		ex->mDecType = mLeft->mDecType;
		return ex;
	}
	else if (mType == EX_BINARY && mToken == TK_ADD && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_CONST) && 
		mLeft->mDecValue && mRight->mDecValue && mLeft->mDecValue->mValue->mType == EX_CONSTANT &&
		mLeft->mDecType->mType == DT_TYPE_POINTER && mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		mLeft = mLeft->mDecValue->mValue;
		return this->ConstantFold(errors, dataSection);
	}
	else if (mType == EX_QUALIFY && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_STRUCT)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue;
		dec->mOffset = mDecValue->mOffset;
		dec->mSize = mDecValue->mSize;
		dec->mBits = mDecValue->mBits;
		dec->mShift = mDecValue->mShift;
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}
	else if (mType == EX_QUALIFY && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE_REF && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_STRUCT)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue->mBase;
		dec->mOffset = mLeft->mDecValue->mOffset + mDecValue->mOffset;
		dec->mSize = mDecValue->mSize;
		dec->mBits = mDecValue->mBits;
		dec->mShift = mDecValue->mShift;
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}
	else if (mType == EX_INDEX && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && 
			mLeft->mDecType->mType == DT_TYPE_ARRAY && mLeft->mDecType->mStride == 0 &&
			mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		int offset = int(mDecType->mSize * mRight->mDecValue->mInteger);

		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue;
		dec->mOffset = offset;
		dec->mSize = mDecType->mSize;
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}
	else if (mType == EX_INDEX && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE_REF && (mLeft->mDecValue->mFlags & DTF_GLOBAL) &&
		mLeft->mDecType->mType == DT_TYPE_ARRAY && mLeft->mDecType->mStride == 0 &&
		mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
		{
			int offset = mLeft->mDecValue->mOffset + int(mDecType->mSize * mRight->mDecValue->mInteger);

			Expression* ex = new Expression(mLocation, EX_VARIABLE);
			Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
			dec->mFlags = mLeft->mDecValue->mFlags;
			dec->mBase = mLeft->mDecValue->mBase;
			dec->mOffset = offset;
			dec->mSize = mDecType->mSize;
			ex->mDecValue = dec;
			ex->mDecType = mDecType;
			return ex;
			}
	else if (mType == EX_CALL && mLeft->mType == EX_CONSTANT && (mLeft->mDecValue->mFlags & DTF_INTRINSIC) && mRight && mRight->mType == EX_CONSTANT)
	{
		Declaration* decf = mLeft->mDecValue, * decp = mRight->mDecValue;
		const Ident* iname = decf->mQualIdent;

		if (decp->mType == DT_CONST_FLOAT || decp->mType == DT_CONST_INTEGER)
		{
			double d = decp->mType == DT_CONST_FLOAT ? decp->mNumber : decp->mInteger;

			bool	check = false;

			if (!strcmp(iname->mString, "fabs"))
				d = fabs(d);
			else if (!strcmp(iname->mString, "floor"))
				d = floor(d);
			else if (!strcmp(iname->mString, "ceil"))
				d = ceil(d);
			else if (!strcmp(iname->mString, "sin"))
				d = sin(d);
			else if (!strcmp(iname->mString, "cos"))
				d = cos(d);
			else if (!strcmp(iname->mString, "tan"))
				d = tan(d);
			else if (!strcmp(iname->mString, "log"))
				d = log(d);
			else if (!strcmp(iname->mString, "exp"))
				d = exp(d);
			else
				return this;

			Expression* ex = new Expression(mLocation, EX_CONSTANT);
			Declaration* dec = new Declaration(mLocation, DT_CONST_FLOAT);
			dec->mBase = TheFloatTypeDeclaration;
			dec->mNumber = d;
			ex->mDecValue = dec;
			ex->mDecType = dec->mBase;
			return ex;
		}
	}
	else if (mType == EX_CALL && mLeft->mType == EX_CONSTANT && (mLeft->mDecValue->mFlags & DTF_CONSTEXPR) && dataSection)
	{
		ConstexprInterpreter	cinter(mLocation, errors, dataSection);
		return cinter.EvalCall(this);
	}
	else if (mType == EX_CONSTRUCT && mLeft->mType == EX_LIST && !mLeft->mRight && mLeft->mLeft->mType == EX_CALL && 
			mLeft->mLeft->mLeft->mType == EX_CONSTANT && (mLeft->mLeft->mLeft->mDecValue->mFlags & DTF_CONSTEXPR) && 
			(mRight->mDecValue->mFlags & DTF_TEMPORARY) &&
			dataSection)
	{
		ConstexprInterpreter	cinter(mLocation, errors, dataSection);
		return cinter.EvalTempConstructor(this);
	}

	return this;
}

Declaration::Declaration(const Location& loc, DecType type)
	: mLocation(loc), mEndLocation(loc), mType(type), mScope(nullptr), mData(nullptr), mIdent(nullptr), mQualIdent(nullptr), mMangleIdent(nullptr),
	mSize(0), mOffset(0), mFlags(0), mComplexity(0), mLocalSize(0), mNumVars(0),
	mBase(nullptr), mParams(nullptr), mParamPack(nullptr), mValue(nullptr), mReturn(nullptr), mNext(nullptr), mPrev(nullptr),
	mConst(nullptr), mMutable(nullptr), mVolatile(nullptr),
	mDefaultConstructor(nullptr), mDestructor(nullptr), mCopyConstructor(nullptr), mCopyAssignment(nullptr), mMoveConstructor(nullptr), mMoveAssignment(nullptr),
	mVectorConstructor(nullptr), mVectorDestructor(nullptr), mVectorCopyConstructor(nullptr), mVectorCopyAssignment(nullptr),
	mVTable(nullptr), mTemplate(nullptr), mForwardParam(nullptr), mForwardCall(nullptr),
	mVarIndex(-1), mLinkerObject(nullptr), mCallers(nullptr), mCalled(nullptr), mAlignment(1), mFriends(nullptr),
	mInteger(0), mNumber(0), mMinValue(-0x80000000LL), mMaxValue(0x7fffffffLL), mFastCallBase(0), mFastCallSize(0), mStride(0), mStripe(1),
	mCompilerOptions(0), mUseCount(0), mTokens(nullptr), mParser(nullptr),
	mShift(0), mBits(0), mOptFlags(0), mInlayRegion(nullptr),
	mReferences(nullptr)
{
	static uint32 gUID = 0;
	mUID = gUID++;
}

Declaration::~Declaration(void)
{
	delete mScope;
	delete[] mData;
}

int Declaration::Stride(void) const
{
	if (mStride > 0)
		return mStride;
	else if (mBase)
		return mBase->mSize;
	else
		return 1;
}

int Declaration::Alignment(void) const
{
	if (mType == DT_TYPE_ARRAY)
		return mBase->Alignment();
	else if (mType == DT_TYPE_STRUCT)
	{
		int alignment = 0;
		Declaration* dec = mParams;
		while (dec)
		{
			alignment |= dec->mBase->Alignment() - 1;
			dec = dec->mNext;
		}
		return alignment + 1;
	}
	else if (mType == DT_TYPE_INTEGER || mType == DT_TYPE_FLOAT || mType == DT_TYPE_POINTER)
		return mSize;
	else
		return 1;
}

Declaration* Declaration::BuildConstPointer(const Location& loc)
{
	Declaration* pdec = new Declaration(loc, DT_TYPE_POINTER);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED | DTF_CONST;
	pdec->mSize = 2;
	return pdec;
}

Declaration* Declaration::BuildConstReference(const Location& loc, DecType type)
{
	Declaration* pdec = new Declaration(loc, type);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED | DTF_CONST;
	pdec->mSize = 2;
	return pdec;
}

Declaration* Declaration::BuildArrayPointer(void)
{
	if (mType == DT_TYPE_ARRAY)
	{
		Declaration* pdec = new Declaration(mLocation, DT_TYPE_POINTER);
		pdec->mBase = mBase;
		pdec->mFlags = DTF_DEFINED;
		pdec->mSize = 2;
		pdec->mStride = mStride;
		return pdec;
	}
	else
		return this;
}

Declaration* Declaration::BuildAddressOfPointer(void)
{
	/*if (mType == DT_TYPE_ARRAY)
		return BuildArrayPointer();
	else*/ if (mType == DT_TYPE_REFERENCE)
		return mBase->BuildAddressOfPointer();
	else
		return BuildPointer(mLocation);
}

bool Declaration::IsNullConst(void) const
{
	if (mType == DT_CONST_INTEGER || mType == DT_CONST_ADDRESS)
		return mInteger == 0;
	else if (mType == DT_CONST_FLOAT)
		return mNumber == 0;
	else
		return false;
}

Declaration* Declaration::ConstCast(Declaration* ntype)
{
	if (ntype == mBase)
		return this;
	else if (ntype->mType == DT_TYPE_POINTER)
	{
		if (mBase->mType == DT_TYPE_POINTER)
		{
			if (mBase->mBase->IsSame(ntype->mBase))
				return this;

			Declaration* pdec = this->Clone();
			pdec->mBase = ntype;
			return pdec;
		}
		else if (mType == DT_TYPE_INTEGER)
		{
			Declaration* pdec = this->Clone();
			pdec->mType = DT_CONST_ADDRESS;
			pdec->mBase = ntype;
			pdec->mSize = 2;
			return pdec;
		}
		else
			return this;
	}
	else if (ntype->mType == DT_TYPE_INTEGER || ntype->mType == DT_TYPE_BOOL || ntype->mType == DT_TYPE_ENUM)
	{
		if (mType == DT_CONST_FLOAT)
		{
			Declaration* pdec = this->Clone();
			pdec->mType = DT_CONST_INTEGER;
			pdec->mInteger = int64(mNumber);
			pdec->mBase = ntype;
			pdec->mSize = ntype->mSize;
			return pdec;
		}
		else
		{
			Declaration* pdec = this->Clone();
			pdec->mBase = ntype;
			pdec->mSize = ntype->mSize;
			return pdec;
		}
	}
	else if (ntype->mType == DT_TYPE_FLOAT)
	{
		if (mType == DT_CONST_FLOAT)
		{
			Declaration* pdec = this->Clone();
			pdec->mBase = ntype;
			return pdec;
		}
		else
		{
			Declaration* pdec = this->Clone();
			pdec->mType = DT_CONST_FLOAT;
			pdec->mNumber = float(mInteger);
			pdec->mBase = ntype;
			pdec->mSize = ntype->mSize;
			return pdec;
		}
	}
	else
		return this;
}

Declaration* Declaration::BuildPointer(const Location& loc)
{
	Declaration* pdec = new Declaration(loc, DT_TYPE_POINTER);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED;
	pdec->mSize = 2;
	return pdec;
}

Declaration* Declaration::BuildReference(const Location& loc, DecType type)
{
	Declaration* pdec = new Declaration(loc, type);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED;
	pdec->mSize = 2;
	return pdec;
}

Declaration* Declaration::BuildRValueRef(const Location& loc)
{
	Declaration* pdec = new Declaration(loc, DT_TYPE_RVALUEREF);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED;
	pdec->mSize = 2;
	return pdec;
}

Declaration* Declaration::NonRefBase(void)
{
	if (IsReference())
		return mBase;
	else
		return this;
}

bool Declaration::IsAuto(void) const
{
	return (mType == DT_TYPE_AUTO || IsReference() && mBase->mType == DT_TYPE_AUTO);
}

Declaration* Declaration::DeduceAuto(Declaration * dec)
{
	if (mType == DT_TYPE_AUTO || IsReference() && mBase->mType == DT_TYPE_AUTO)
	{
		dec = dec->NonRefBase();

		if (dec->mType == DT_TYPE_ARRAY)
			dec = dec->mBase->BuildPointer(mLocation);

		if ((IsReference() ? mBase->mFlags : mFlags) & DTF_CONST)
			dec = dec->ToConstType();
		else
			dec = dec->ToMutableType();

		if (IsReference())
			dec = dec->BuildReference(mLocation, mType);

		return dec;
	}
	else
		return this;
}


Declaration* Declaration::BuildConstRValueRef(const Location& loc)
{
	Declaration* pdec = new Declaration(loc, DT_TYPE_RVALUEREF);
	pdec->mBase = this;
	pdec->mFlags = DTF_DEFINED | DTF_CONST;
	pdec->mSize = 2;
	return pdec;
}

DecType  Declaration::ValueType(void) const
{
	if (IsReference())
		return mBase->mType;
	else
		return mType;
}

Declaration* Declaration::Last(void)
{
	mPrev = nullptr;
	Declaration* p = this;
	while (p->mNext)
	{
		p->mNext->mPrev = p;
		p = p->mNext;
	}
	return p;
}

const Ident* Declaration::FullIdent(void)
{
	if (mType == DT_CONST_FUNCTION)
	{
		const Ident* tident = MangleIdent()->Mangle("(");
		Declaration* dec = mBase->mParams;
		while (dec)
		{
			tident = tident->Mangle(dec->mBase->MangleIdent()->mString);
			dec = dec->mNext;
			if (dec)
				tident = tident->Mangle(",");
		}
		tident = tident->Mangle(")->");
		tident = tident->Mangle(mBase->mBase ? mBase->mBase->MangleIdent()->mString : "null");
		return tident;
	}
	else
		return MangleIdent();

}

const Ident* Declaration::MangleIdent(void)
{
	if (!mMangleIdent)
	{
		if (mType == DT_CONST_INTEGER)
		{
			char	buffer[20];
			sprintf_s(buffer, "%d", (int)mInteger);
			mMangleIdent = Ident::Unique(buffer);
		}
		else if (mType == DT_CONST_FUNCTION)
		{
			mMangleIdent = mQualIdent;
		}
		else if (mType == DT_TYPE_FUNCTION)
		{
			mMangleIdent = mBase->MangleIdent();
			mMangleIdent = mMangleIdent->Mangle("(*)(");
			Declaration* dec = mParams;
			while (dec)
			{
				mMangleIdent = mMangleIdent->Mangle(dec->mBase ? dec->mBase->MangleIdent()->mString : "null");
				dec = dec->mNext;
				if (dec)
					mMangleIdent = mMangleIdent->Mangle(",");
			}
			mMangleIdent = mMangleIdent->Mangle(")");
		}
		else if (mType == DT_TYPE_INTEGER)
		{
			char	buffer[20];
			sprintf_s(buffer, "%c%d", (mFlags & DTF_SIGNED) ? 'i' : 'u', mSize * 8);
			mMangleIdent = Ident::Unique(buffer);
		}
		else if (mType == DT_TYPE_FLOAT)
		{
			mMangleIdent = Ident::Unique("float");
		}
		else if (mType == DT_TYPE_BOOL)
		{
			mMangleIdent = Ident::Unique("bool");
		}
		else if (mType == DT_TYPE_REFERENCE)
		{
			mMangleIdent = mBase->MangleIdent()->Mangle("&");
		}
		else if (mType == DT_TYPE_RVALUEREF)
		{
			mMangleIdent = mBase->MangleIdent()->Mangle("&&");
		}
		else if (mType == DT_TYPE_POINTER)
		{
			mMangleIdent = mBase->MangleIdent()->Mangle("*");
		}
		else if (mType == DT_TYPE_ARRAY)
		{
			mMangleIdent = mBase->MangleIdent()->Mangle("[]");
		}
		else if (mType == DT_TYPE_STRUCT)
		{
			if (mQualIdent)
				mMangleIdent = mQualIdent->PreMangle("struct ");
			else
				mMangleIdent = Ident::Unique("struct S", mUID);
		}
		else if (mType == DT_TYPE_UNION)
		{
			if (mQualIdent)
				mMangleIdent = mQualIdent->PreMangle("union ");
			else
				mMangleIdent = Ident::Unique("union U", mUID);
		}
		else if (mType == DT_TYPE_ENUM)
		{
			if (mQualIdent)
				mMangleIdent = mQualIdent->PreMangle("enum ");
			else
				mMangleIdent = Ident::Unique("enum E", mUID);
		}
		else if (mType == DT_TYPE_VOID)
		{
			mMangleIdent = Ident::Unique("void");
		}
		else if (mType == DT_TEMPLATE)
		{
			mMangleIdent = Ident::Unique("<");

			Declaration* dec = mParams;
			while (dec)
			{
				const Ident* id;
				if (dec->mBase)
					id = dec->mBase->MangleIdent();
				else
					id = dec->MangleIdent();
				if (id)
					mMangleIdent = mMangleIdent->Mangle(id->mString);

				dec = dec->mNext;
				if (dec)
					mMangleIdent = mMangleIdent->Mangle(",");
			}
			mMangleIdent = mMangleIdent->Mangle(">");
		}
		else if (mType == DT_PACK_TYPE)
		{
			Declaration* dec = mParams;
			if (dec)
			{
				while (dec)
				{
					const Ident* id = dec->MangleIdent();

					if (id)
					{
						if (mMangleIdent)
							mMangleIdent = mMangleIdent->Mangle(id->mString);
						else
							mMangleIdent = id;
					}

					dec = dec->mNext;
					if (dec)
						mMangleIdent = mMangleIdent->Mangle(",");
				}
			}
			else
				mMangleIdent = Ident::Unique("void");
		}
		else
			mMangleIdent = mQualIdent;

		if (mTemplate)
		{

		}

		if (mFlags & DTF_CONST)
			mMangleIdent = mMangleIdent->PreMangle("const ");
		if (mFlags & DTF_VOLATILE)
			mMangleIdent = mMangleIdent->PreMangle("volatile ");
	}

	return mMangleIdent;
}

Declaration* Declaration::ExpandTemplate(DeclarationScope* scope)
{
	if (mType == DT_CONST_FUNCTION)
	{
		Declaration* dec = this->Clone();
		dec->mBase = dec->mBase->ExpandTemplate(scope);
		return dec;
	}
	else if (mType == DT_TYPE_FUNCTION)
	{
		Declaration* dec = this->Clone();
		dec->mParams = mParams ? mParams->ExpandTemplate(scope) : nullptr;
		dec->mBase = mBase->ExpandTemplate(scope);
		Declaration* pdec = dec->mParams;
		if (pdec)
		{
			int	vi = pdec->mVarIndex;
			while (pdec)
			{
				pdec->mVarIndex = vi;
				vi += pdec->mSize;
				pdec = pdec->mNext;
			}
		}
		return dec;
	}
	else if (mType == DT_ARGUMENT)
	{
		Declaration* ndec = mNext ? mNext->ExpandTemplate(scope) : nullptr;
		Declaration* bdec = mBase->ExpandTemplate(scope);
		if (ndec != mNext || bdec != mBase)
		{
			Declaration* dec = this->Clone();
			dec->mBase = bdec;
			dec->mSize = bdec->mSize;
			dec->mNext = ndec;
			return dec;
		}
	}
	else if (mType == DT_TYPE_REFERENCE || mType == DT_TYPE_POINTER || mType == DT_TYPE_RVALUEREF)
	{
		Declaration* bdec = mBase->ExpandTemplate(scope);
		if (bdec != mBase)
		{
			Declaration* dec = this->Clone();
			dec->mBase = bdec;
			return dec;
		}
	}
	else if (mType == DT_TYPE_TEMPLATE)
		return scope->Lookup(mIdent);

	return this;
}

bool Declaration::ResolveTemplate(Expression* pexp, Declaration* tdec)
{
	Declaration* pdec = tdec->mBase->mParams;

	// Insert partially resolved templates
	Declaration* ptdec = tdec->mTemplate->mParams;
	while (ptdec)
	{
		if (ptdec->mBase)
			mScope->Insert(ptdec->mIdent, ptdec->mBase);
		ptdec = ptdec->mNext;
	}

	Declaration* phead = nullptr, * ptail = nullptr;
	int	pcnt = 0;

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
			if (pdec->mType == DT_PACK_ARGUMENT)
			{
				Declaration* tpdec = ex->mDecType;
				if (tpdec->IsReference())
					tpdec = tpdec->mBase;

				if (tpdec->mType == DT_TYPE_ARRAY)
					tpdec = tpdec->mBase->BuildPointer(tpdec->mLocation);
				else
					tpdec = tpdec->Clone();

				if (ptail)
					ptail->mNext = tpdec;
				else
					phead = tpdec;
				ptail = tpdec;
			}
			else
			{
				if (!ResolveTemplate(ex->mDecType, pdec->mBase))
					return false;

				pdec = pdec->mNext;
			}
		}
		else
			return false;
	}

	if (pdec)
	{
		if (pdec->mType == DT_PACK_ARGUMENT)
		{
			Declaration* tpdec = new Declaration(pdec->mLocation, DT_PACK_TYPE);
			if (pdec->mBase->mType == DT_TYPE_REFERENCE)
				tpdec->mIdent = pdec->mBase->mBase->mIdent;
			else
				tpdec->mIdent = pdec->mBase->mIdent;
			tpdec->mParams = phead;
			tpdec->mFlags |= DTF_DEFINED;
			mScope->Insert(tpdec->mIdent, tpdec);
		}
		else
			return false;
	}

	Declaration* ppdec = nullptr;
	ptdec = tdec->mTemplate->mParams;
	while (ptdec)
	{
		Declaration* pdec = mScope->Lookup(ptdec->mIdent);
		if (!pdec)
			return false;

		Declaration * epdec = ptdec->Clone();
		epdec->mBase = pdec;
		epdec->mFlags |= DTF_DEFINED;

		if (ppdec)
			ppdec->mNext = epdec;
		else
			mParams = epdec;
		ppdec = epdec;
		ptdec = ptdec->mNext;
	}

	mScope->Clear();

	return true;
}

bool Declaration::CanResolveTemplate(Expression* pexp, Declaration* tdec)
{
	Declaration* pdec = tdec->mBase->mParams;

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
			if (!ResolveTemplate(ex->mDecType, pdec->mBase))
				return false;

			if (pdec->mType != DT_PACK_ARGUMENT)
				pdec = pdec->mNext;
		}
		else
			return false;
	}

	if (pdec)
		return pdec->mType == DT_PACK_ARGUMENT;
	else
		return true;
}

bool Declaration::ResolveTemplate(Declaration* fdec, Declaration* tdec)
{
	if (tdec->IsSame(fdec))
		return true;
	else if (fdec->IsReference())
		return ResolveTemplate(fdec->mBase, tdec);
	else if (tdec->mType == DT_TYPE_FUNCTION)
	{
		if (fdec->mType == DT_TYPE_FUNCTION)
		{
			if (fdec->mBase)
			{
				if (!tdec->mBase || !ResolveTemplate(fdec->mBase, tdec->mBase))
					return false;
			}
			else if (tdec->mBase)
				return false;

			Declaration* fpdec = fdec->mParams;
			Declaration* tpdec = tdec->mParams;
			while (tpdec)
			{
				if (tpdec->mBase->mType == DT_PACK_TEMPLATE)
				{
					if (fpdec && fpdec->mBase->mType == DT_PACK_TEMPLATE)
						mScope->Insert(tdec->mIdent, fpdec);
					else
					{
						Declaration* ptdec = new Declaration(fdec->mLocation, DT_PACK_TYPE);
						Declaration* ppdec = nullptr;
						while (fpdec)
						{
							Declaration* pdec = fpdec->mBase->Clone();
							if (ppdec)
								ppdec->mNext = pdec;
							else
								ptdec->mParams = pdec;
							ppdec = pdec;
							fpdec = fpdec->mNext;
						}

						ptdec->mFlags |= DTF_DEFINED;
						mScope->Insert(tpdec->mBase->mIdent, ptdec);
					}

					return true;
				}

				if (!fpdec)
					return false;

				if (!ResolveTemplate(fpdec->mBase, tpdec->mBase))
					return false;

				fpdec = fpdec->mNext;
				tpdec = tpdec->mNext;
			}
			if (fpdec)
				return false;
		}
		else
			return false;

		return true;
	}
	else if (tdec->mType == DT_TYPE_REFERENCE)
	{
		return ResolveTemplate(fdec, tdec->mBase);
	}
	else if (tdec->mType == DT_TYPE_RVALUEREF)
	{
		return ResolveTemplate(fdec, tdec->mBase);
	}
	else if (tdec->mType == DT_TYPE_POINTER)
	{
		if (fdec->mType == DT_TYPE_POINTER || fdec->mType == DT_TYPE_ARRAY)
			return ResolveTemplate(fdec->mBase, tdec->mBase);
		else
			return false;
	}
	else if (tdec->mType == DT_TYPE_TEMPLATE)
	{
		if (fdec->mType == DT_TYPE_ARRAY)
			fdec = fdec->mBase->BuildPointer(fdec->mLocation);
		else if (fdec->mType == DT_TYPE_FUNCTION)
			fdec = fdec->BuildPointer(fdec->mLocation);

		Declaration* pdec;
		if (tdec->mBase)
		{
			pdec = mScope->Lookup(tdec->mBase->mIdent);
			if (!pdec)
				return false;
			if (pdec->mType == DT_TYPE_STRUCT)
				pdec = pdec->mScope->Lookup(tdec->mIdent);
		}
		else
			pdec = mScope->Insert(tdec->mIdent, fdec);

		if (pdec && !pdec->IsSame(fdec))
			return false;

		return true;
	}
	else if (tdec->mType == DT_PACK_TEMPLATE)
	{
		Declaration* pdec;
		if (tdec->mBase)
		{
			pdec = mScope->Lookup(tdec->mBase->mIdent);
			if (!pdec)
				return false;
		}
		else if (fdec->mType == DT_PACK_TYPE)
		{
			mScope->Insert(tdec->mIdent, fdec);
		}
		else
		{
			pdec = mScope->Lookup(tdec->mIdent);
			if (!pdec)
			{
				pdec = new Declaration(fdec->mLocation, DT_PACK_TYPE);
				mScope->Insert(tdec->mIdent, pdec);
			}

			if (!pdec->mParams)
			{
				pdec->mParams = fdec->Clone();
			}
			else
			{
				Declaration* ppdec = pdec->mParams;
				while (ppdec->mNext)
					ppdec = ppdec->mNext;
				ppdec->mNext = fdec->Clone();
			}
		}

		return true;
	}
	else if (tdec->mType == DT_TYPE_STRUCT && fdec->mType == DT_TYPE_STRUCT && tdec->mTemplate)
	{
		Declaration	*ftdec = tdec->mTemplate;
		while (ftdec)
		{
			if (ftdec->mBase->mQualIdent == fdec->mQualIdent)
			{
				Declaration* fpdec = ftdec->mParams;
				Declaration* tpdec = tdec->mTemplate->mParams;

				while (fpdec && tpdec)
				{
					if (tpdec->mBase->mType == DT_TYPE_TEMPLATE || tpdec->mBase->mType == DT_CONST_TEMPLATE)
					{
						Declaration * pdec = mScope->Insert(tpdec->mBase->mIdent, fpdec->mBase);
						if (pdec && !pdec->IsSame(fpdec->mBase))
							return false;
					}
					else if (tpdec->mBase->mType == DT_PACK_TEMPLATE)
					{
						Declaration* tppack;
						if (fpdec->mType == DT_PACK_TEMPLATE)
							tppack = fpdec->mBase;
						else
						{
							tppack = new Declaration(fpdec->mLocation, DT_PACK_TYPE);

							Declaration* ppdec = nullptr;

							while (fpdec)
							{
								if (fpdec->mType == DT_PACK_TEMPLATE)
								{
									if (ppdec)
										ppdec->mNext = fpdec->mBase->mParams;
									else
										tppack->mParams = fpdec->mBase->mParams;
									break;
								}

								Declaration* ndec = fpdec->mBase->Clone();

								if (ppdec)
									ppdec->mNext = ndec;
								else
									tppack->mParams = ndec;
								ppdec = ndec;

								fpdec = fpdec->mNext;
							}

						}

						Declaration* pdec = mScope->Insert(tpdec->mBase->mIdent, tppack);
						if (pdec && !pdec->IsSame(fpdec->mBase))
							return false;

						return true;
					}

					fpdec = fpdec->mNext;
					tpdec = tpdec->mNext;
				}

				return !fpdec && !tpdec;
			}
			
			ftdec = ftdec->mNext;
		}
		return false;
	}
	else
		return tdec->IsSame(fdec);
}


Declaration* Declaration::Clone(void)
{
	Declaration* ndec = new Declaration(mLocation, mType);
	ndec->mSize = mSize;
	ndec->mOffset = mOffset;
	ndec->mStride = mStride;
	ndec->mStripe = mStripe;
	ndec->mBase = mBase;
	ndec->mFlags = mFlags;
	ndec->mScope = mScope;
	ndec->mParams = mParams;
	ndec->mParamPack = mParamPack;
	ndec->mIdent = mIdent;
	ndec->mQualIdent = mQualIdent;
	ndec->mValue = mValue;
	ndec->mVarIndex = mVarIndex;
	ndec->mLinkerObject = mLinkerObject;
	ndec->mAlignment = mAlignment;
	ndec->mSection = mSection;
	ndec->mVTable = mVTable;
	ndec->mInteger = mInteger;
	ndec->mNumber = mNumber;
	ndec->mMinValue = mMinValue;
	ndec->mMaxValue = mMaxValue;
	ndec->mCompilerOptions = mCompilerOptions;
	ndec->mParser = mParser;

	return ndec;
}

Declaration* Declaration::ToStriped(Errors * errors)
{
	Declaration* ndec = this->Clone();

	if (mType == DT_TYPE_ARRAY)
	{
		if (mSize == 0)
			errors->Error(ndec->mLocation, ERRR_STRIPE_REQUIRES_FIXED_SIZE_ARRAY, "__striped requires fixed size array");

		ndec->mFlags |= DTF_STRIPED;
		if (mBase->mType == DT_TYPE_ARRAY)
		{
			ndec->mBase = mBase->Clone();
			if (mBase->mSize)
			{
				ndec->mStride = mBase->mSize / mBase->mBase->mSize;
				ndec->mBase->mStride = 1;
				ndec->mBase->mBase = mBase->mBase->ToStriped(mSize / mBase->mBase->mSize);
			}
			else
				errors->Error(ndec->mLocation, ERRR_STRIPE_REQUIRES_FIXED_SIZE_ARRAY, "__striped with zero size");

		}
		else
		{
			ndec->mStride = 1;
			if (mBase->mSize > 0)
				ndec->mBase = mBase->ToStriped(mSize / mBase->mSize);
		}
	}
	else if (ndec->mBase)
	{
		ndec->mBase = mBase->ToStriped(errors);
	}
	else
		errors->Error(ndec->mLocation, ERRR_STRIPE_REQUIRES_FIXED_SIZE_ARRAY, "__striped requires fixed size array");

	return ndec;
}

Declaration* Declaration::ToStriped(int stripe)
{
	Declaration* ndec = new Declaration(mLocation, mType);
	ndec->mSize = mSize;
	ndec->mOffset = mOffset * stripe;
	ndec->mStride = mStride;
	ndec->mStripe = stripe;
	ndec->mFlags = mFlags;
	ndec->mIdent = mIdent;
	ndec->mQualIdent = mQualIdent;

	if (mType == DT_ELEMENT)
		ndec->mBase = mBase->ToStriped(stripe);
	else
		ndec->mBase = mBase;

	if (mType == DT_TYPE_STRUCT || mType == DT_TYPE_UNION)
	{
		ndec->mScope = new DeclarationScope(nullptr, mScope->mLevel);
		Declaration	* p = mParams;
		Declaration* prev = nullptr;
		while (p)
		{
			Declaration* pnec = p->ToStriped(stripe);

			ndec->mScope->Insert(pnec->mIdent, pnec);

			if (prev)
				prev->mNext = pnec;
			else
				ndec->mParams = pnec;
			prev = pnec;
			p = p->mNext;
		}		
	}
	else if (mType == DT_TYPE_FUNCTION)
	{
		ndec->mParams = mParams;
	}
	else if (mType == DT_TYPE_ARRAY)
	{
		ndec->mStride = stripe;
		ndec->mBase = mBase->ToStriped(stripe);
	}
	else
	{
		ndec->mScope = mScope;
		ndec->mParams = mParams;
	}

	return ndec;
}

Declaration* Declaration::ToVolatileType(void)
{
	if (mFlags & DTF_VOLATILE)
		return this;

	if (!mVolatile)
	{
		Declaration* ndec = new Declaration(mLocation, mType);
		ndec->mSize = mSize;
		ndec->mStride = mStride;
		ndec->mBase = mBase;
		ndec->mFlags = mFlags | DTF_VOLATILE;
		ndec->mScope = mScope;
		ndec->mParams = mParams;
		ndec->mIdent = mIdent;
		ndec->mQualIdent = mQualIdent;
		ndec->mTemplate = mTemplate;

		if (mType == DT_TYPE_STRUCT)
		{
			ndec->mScope = new DeclarationScope(nullptr, mScope->mLevel);
			Declaration* p = mParams;
			Declaration* prev = nullptr;
			while (p)
			{
				Declaration* pnec = p->Clone();
				pnec->mBase = pnec->mBase->ToVolatileType();

				ndec->mScope->Insert(pnec->mIdent, pnec);

				if (prev)
					prev->mNext = pnec;
				else
					ndec->mParams = pnec;
				prev = pnec;
				p = p->mNext;
			}
		}

		ndec->mDestructor = mDestructor;
		ndec->mDefaultConstructor = mDefaultConstructor;
		ndec->mCopyConstructor = mCopyConstructor;
		ndec->mMoveConstructor = mMoveConstructor;
		ndec->mVectorConstructor = mVectorConstructor;
		ndec->mVectorDestructor = mVectorDestructor;
		ndec->mVectorCopyConstructor = mVectorCopyConstructor;
		ndec->mVTable = mVTable;

		mVolatile = ndec;
	}

	return mVolatile;
}

Declaration* Declaration::ToConstType(void)
{
	if (mFlags & DTF_CONST)
		return this;	

	if (!mConst)
	{
		Declaration* ndec = new Declaration(mLocation, mType);
		ndec->mSize = mSize;
		ndec->mStride = mStride;
		ndec->mStripe = mStripe;
		ndec->mBase = mBase;
		ndec->mFlags = mFlags | DTF_CONST;
		ndec->mScope = mScope;
		ndec->mParams = mParams;
		ndec->mIdent = mIdent;
		ndec->mQualIdent = mQualIdent;
		ndec->mTemplate = mTemplate;

		ndec->mDestructor = mDestructor;
		ndec->mDefaultConstructor = mDefaultConstructor;
		ndec->mCopyConstructor = mCopyConstructor;
		ndec->mMoveConstructor = mMoveConstructor;
		ndec->mVectorConstructor = mVectorConstructor;
		ndec->mVectorDestructor = mVectorDestructor;
		ndec->mVectorCopyConstructor = mVectorCopyConstructor;
		ndec->mVTable = mVTable;

		ndec->mMutable = this;
		mConst = ndec;
	}
	
	return mConst;
}

Declaration* Declaration::ToMutableType(void)
{
	if (!(mFlags & DTF_CONST))
		return this;

	if (!mMutable)
	{
		Declaration* ndec = new Declaration(mLocation, mType);
		ndec->mSize = mSize;
		ndec->mStride = mStride;
		ndec->mStripe = mStripe;
		ndec->mBase = mBase;
		ndec->mFlags = mFlags | DTF_CONST;
		ndec->mScope = mScope;
		ndec->mParams = mParams;
		ndec->mIdent = mIdent;
		ndec->mQualIdent = mQualIdent;
		ndec->mTemplate = mTemplate;

		ndec->mDestructor = mDestructor;
		ndec->mDefaultConstructor = mDefaultConstructor;
		ndec->mCopyConstructor = mCopyConstructor;
		ndec->mMoveConstructor = mMoveConstructor;
		ndec->mVectorConstructor = mVectorConstructor;
		ndec->mVectorDestructor = mVectorDestructor;
		ndec->mVectorCopyConstructor = mVectorCopyConstructor;
		ndec->mVTable = mVTable;

		ndec->mConst = this;
		mMutable = ndec;
	}

	return mMutable;
}

bool Declaration::IsSameTemplate(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (this->mType != dec->mType)
		return false;

	if (mType == DT_CONST_FUNCTION)
		return mBase->IsSame(dec->mBase);
	else if (mType == DT_TYPE_STRUCT)
		return true;

	return false;
}

bool Declaration::IsSubType(const Declaration* dec) const
{
	if (this == dec)
		return true;

	if (mType == DT_TYPE_VOID)
		return true;

	if (IsReference() && dec->IsReference())
		return mBase->IsSubType(dec->mBase);

	if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
	{
		if (dec->mType == DT_TYPE_POINTER || dec->mType == DT_TYPE_ARRAY)
			return /*this->Stride() == dec->Stride() &&*/ mBase->IsSubType(dec->mBase);
	}

	if (mType != dec->mType)
		return false;
	if (mType != DT_TYPE_STRUCT && mSize != dec->mSize)
		return false;
	if (mStripe != dec->mStripe)
		return false;

	if ((mFlags & DTF_SIGNED) != (dec->mFlags & DTF_SIGNED))
		return false;

	if ((dec->mFlags & ~mFlags) & (DTF_CONST | DTF_VOLATILE))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_STRUCT || mType == DT_TYPE_ENUM)
	{
		if (mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize))
			return true;

		if (dec->mBase)
		{
			Declaration* bcdec = dec->mBase;
			while (bcdec)
			{
				if (IsSubType(bcdec->mBase))
					return true;
				bcdec = bcdec->mNext;
			}
		}

		return false;
	}
	else if (mType == DT_TYPE_UNION)
		return false;
	else if (mType == DT_TYPE_ARRAY)
		return mSize <= dec->mSize && mBase->IsSubType(dec->mBase);
	else if (mType == DT_TYPE_FUNCTION)
	{
		if (!dec->mBase->IsSubType(mBase))
			return false;
		Declaration* dl = mParams, * dr = dec->mParams;
		while (dl && dr)
		{
			if (!dl->mBase->IsSubType(dr->mBase))
				return false;
			dl = dl->mNext;
			dr = dr->mNext;
		}

		if (dl || dr)
			return false;

		if ((mFlags & DTF_INTERRUPT) && !(dec->mFlags & DTF_INTERRUPT))
			return false;

		if ((mFlags & DTF_VARIADIC) != (dec->mFlags & DTF_VARIADIC))
			return false;

		return true;
	}

	return false;
}

bool Declaration::IsSameMutable(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (mType != dec->mType)
		return false;
	if (mSize != dec->mSize)
		return false;
	if (mStripe != dec->mStripe)
		return false;

	if ((mFlags & DTF_SIGNED) != (dec->mFlags & DTF_SIGNED))
		return false;
	if ((dec->mFlags & DTF_CONST) && !(mFlags & DTF_CONST))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_ENUM)
		return mIdent == dec->mIdent;
	else if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
		return this->Stride() == dec->Stride() && mBase->IsSame(dec->mBase);
	else if (mType == DT_TYPE_STRUCT)
		return mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize);
	else if (mType == DT_TYPE_FUNCTION)
	{
		if (!mBase->IsSame(dec->mBase))
			return false;
		Declaration* dl = mParams, * dr = dec->mParams;
		while (dl && dr)
		{
			if (!dl->mBase->IsSame(dr->mBase))
				return false;
			dl = dl->mNext;
			dr = dr->mNext;
		}

		if (dl || dr)
			return false;

		if ((mFlags & DTF_VARIADIC) != (dec->mFlags & DTF_VARIADIC))
			return false;

		return true;
	}

	return false;
}

bool Declaration::IsConstRefSame(const Declaration* dec) const
{
	if (IsReference())
		return mBase->IsConstRefSame(dec);
	else if (dec->IsReference())
		return IsConstRefSame(dec->mBase);
	else
		return IsConstSame(dec);
}

bool Declaration::IsConstSame(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (mType != dec->mType)
		return false;
	if (mSize != dec->mSize)
		return false;
	if (mStripe != dec->mStripe)
		return false;

	if ((mFlags & DTF_SIGNED) != (dec->mFlags & DTF_SIGNED))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_ENUM)
		return mIdent == dec->mIdent;
	else if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
		return this->Stride() == dec->Stride() && mBase->IsSame(dec->mBase);
	else if (mType == DT_TYPE_STRUCT)
		return mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize);
	else if (mType == DT_TYPE_FUNCTION)
	{
		if (!mBase->IsSame(dec->mBase))
			return false;
		Declaration* dl = mParams, * dr = dec->mParams;
		while (dl && dr)
		{
			if (!dl->mBase->IsSame(dr->mBase))
				return false;
			dl = dl->mNext;
			dr = dr->mNext;
		}

		if (dl || dr)
			return false;

		if ((mFlags & DTF_VARIADIC) != (dec->mFlags & DTF_VARIADIC))
			return false;

		return true;
	}

	return false;
}

bool Declaration::IsTemplateSameParams(const Declaration* dec, const Declaration* tdec) const
{
	if (mType == DT_TYPE_FUNCTION && dec->mType == DT_TYPE_FUNCTION)
	{
		Declaration* ld = mParams, * rd = dec->mParams;
		if (mFlags & DTF_FUNC_THIS)
			ld = ld->mNext;

		while (ld && rd)
		{
			if (!ld->mBase->IsTemplateSame(rd->mBase, tdec))
				return false;
			ld = ld->mNext;
			rd = rd->mNext;
		}

		return !ld && !rd;
	}
	else
		return false;
}

bool Declaration::IsSameParams(const Declaration* dec) const
{
	if (mType == DT_TYPE_FUNCTION && dec->mType == DT_TYPE_FUNCTION)
	{
		Declaration* ld = mParams, * rd = dec->mParams;
		while (ld && rd)
		{
			if (!ld->mBase->IsSame(rd->mBase))
				return false;
			ld = ld->mNext;
			rd = rd->mNext;
		}

		return !ld && !rd;
	}
	else if (mType == DT_TEMPLATE && dec->mType == DT_TEMPLATE)
	{
		Declaration* ld = mParams, * rd = dec->mParams;
		while (ld && rd)
		{
			if (ld->mType == DT_CONST_FUNCTION && rd->mType == DT_CONST_FUNCTION)
			{
				if (ld->mValue != rd->mValue)
					return false;
			}
			else if (ld->mType == DT_PACK_TEMPLATE && rd->mType == DT_PACK_TEMPLATE)
			{
				if (!ld->mBase->IsSameParams(rd->mBase))
					return false;
			}
			else if (!ld->mBase || !rd->mBase)
				return false;
			else if (!ld->mBase->IsSame(rd->mBase))
				return false;
			ld = ld->mNext;
			rd = rd->mNext;
		}

		return !ld && !rd;
	}
	else if (mType == DT_PACK_TYPE && dec->mType == DT_PACK_TYPE)
	{
		Declaration* ld = mParams, * rd = dec->mParams;
		while (ld && rd)
		{
			if (!ld->IsSame(rd))
				return false;
			ld = ld->mNext;
			rd = rd->mNext;
		}

		return !ld && !rd;
	}
	else
		return false;
}

bool Declaration::IsDerivedFrom(const Declaration* dec) const
{
	if (mType != DT_TYPE_FUNCTION || dec->mType != DT_TYPE_FUNCTION)
		return false;

	if (!(mFlags & DTF_FUNC_THIS) || !(dec->mFlags & DTF_FUNC_THIS))
		return false;

	if (!mBase->IsSame(dec->mBase))
		return false;
	Declaration* dl = mParams->mNext, * dr = dec->mParams->mNext;
	while (dl && dr)
	{
		if (!dl->mBase->IsSame(dr->mBase))
			return false;
		dl = dl->mNext;
		dr = dr->mNext;
	}

	if (dl || dr)
		return false;

	return true;
}

bool Declaration::IsSame(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (mType != dec->mType)
		return false;

	if (!(mFlags & dec->mFlags & DTF_DEFINED) && mType == DT_TYPE_STRUCT)
		return mIdent == dec->mIdent;

	if (mSize != dec->mSize)
		return false;
	if (mStripe != dec->mStripe)
		return false;

	if ((mFlags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)) != (dec->mFlags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)))
		return false;

	if (mType == DT_CONST_INTEGER)
		return mInteger == dec->mInteger;
	else if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID || mType == DT_TYPE_AUTO)
		return true;
	else if (mType == DT_TYPE_ENUM)
		return mIdent == dec->mIdent;
	else if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
	{
		if (mBase->mType == DT_TYPE_STRUCT && dec->mBase->mType == DT_TYPE_STRUCT && mBase->mStripe == dec->mBase->mStripe)
		{
			if (mBase->mQualIdent == dec->mBase->mQualIdent &&
				(mBase->mFlags & (DTF_CONST | DTF_VOLATILE)) == (dec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE)))
				return true;
			else
				return false;
		}
		else
			return this->Stride() == dec->Stride() && mBase->IsSame(dec->mBase);
	}
	else if (IsReference())
		return mBase->IsSame(dec->mBase);
	else if (mType == DT_TYPE_STRUCT)
		return mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize);
	else if (mType == DT_TYPE_FUNCTION)
	{
		if (!mBase->IsSame(dec->mBase))
			return false;
		Declaration* dl = mParams, * dr = dec->mParams;
		while (dl && dr)
		{
			if (!dl->mBase->IsSame(dr->mBase))
				return false;
			dl = dl->mNext;
			dr = dr->mNext;
		}

		if (dl || dr)
			return false;

		if ((mFlags & DTF_VARIADIC) != (dec->mFlags & DTF_VARIADIC))
			return false;

		return true;
	}
	else if (mType == DT_TYPE_TEMPLATE)
	{
		return mIdent == dec->mIdent;
	}

	return false;
}

bool Declaration::IsTemplateSame(const Declaration* dec, const Declaration * tdec) const
{
	uint64	dflags = dec->mFlags;

	if (dec->mType == DT_TYPE_TEMPLATE)
	{
		dec = tdec->mScope->Lookup(dec->mIdent);
		if (!dec)
			return true;
		dflags |= dec->mFlags;
	}
	else if (dec->mType == DT_PACK_TEMPLATE)
	{
		dec = tdec->mScope->Lookup(dec->mIdent);
		if (!dec)
			return true;
		dflags |= dec->mFlags;
	}

	if (this == dec)
		return true;

	if (mType != dec->mType)
		return false;

	if (dec->mType == DT_TYPE_STRUCT && dec->mTemplate)
		return true;

	if (mSize != dec->mSize)
		return false;
	if (mStripe != dec->mStripe)
		return false;

	if ((mFlags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)) != (dflags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_ENUM)
		return mIdent == dec->mIdent;
	else if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
	{
		if (mBase->mType == DT_TYPE_STRUCT && dec->mBase->mType == DT_TYPE_STRUCT && mBase->mStripe == dec->mBase->mStripe)
		{
			if (mBase->mQualIdent == dec->mBase->mQualIdent &&
				(mBase->mFlags & (DTF_CONST | DTF_VOLATILE)) == (dec->mBase->mFlags & (DTF_CONST | DTF_VOLATILE)))
				return true;
			else
				return false;
		}
		else if (dec->mBase->mType == DT_TYPE_TEMPLATE)
			return mBase->IsTemplateSame(dec->mBase, tdec);
		else
			return this->Stride() == dec->Stride() && mBase->IsTemplateSame(dec->mBase, tdec);
	}
	else if (IsReference())
		return mBase->IsTemplateSame(dec->mBase, tdec);
	else if (mType == DT_TYPE_STRUCT)
		return mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize);
	else if (mType == DT_TYPE_FUNCTION)
	{
		if (!mBase->IsTemplateSame(dec->mBase, tdec))
			return false;
		Declaration* dl = mParams, * dr = dec->mParams;
		while (dl && dr)
		{
			if (!dl->mBase->IsTemplateSame(dr->mBase, tdec))
				return false;
			dl = dl->mNext;
			dr = dr->mNext;
		}

		if (dl || dr)
			return false;

		if ((mFlags & DTF_VARIADIC) != (dec->mFlags & DTF_VARIADIC))
			return false;

		return true;
	}

	return false;
}

bool Declaration::IsSameValue(const Declaration* dec) const 
{
	if (mType != dec->mType || mSize != dec->mSize)
		return false;

	switch (mType)
	{
	case DT_CONST_INTEGER:
		return mInteger == dec->mInteger;
	case DT_CONST_FLOAT:
		return mNumber == dec->mNumber;
	case DT_CONST_ADDRESS:
		return mInteger == dec->mInteger;
	case DT_CONST_POINTER:
		return mValue && dec->mValue && mValue->mType == dec->mValue->mType && mValue->mDecValue == dec->mValue->mDecValue;
	}

	return false;
}

bool Declaration::CanAssign(const Declaration* fromType) const
{
	if (fromType->IsReference())
		return this->CanAssign(fromType->mBase);
	if (IsReference())
		return mBase->IsSubType(fromType);

	if (this->IsSame(fromType))
		return true;
	else if (IsNumericType())
	{
		if (fromType->IsNumericType())
			return true;
	}
	else if (mType == DT_TYPE_STRUCT && fromType->mType == DT_TYPE_STRUCT)
	{
		if (mScope == fromType->mScope || (mIdent == fromType->mIdent && mSize == fromType->mSize))
			return true;
		if (fromType->mBase)
			return this->CanAssign(fromType->mBase);
		return false;
	}
	else if (mType == DT_TYPE_ARRAY && fromType->mType == DT_TYPE_ARRAY)
	{
		return mSize == fromType->mSize && mStride == fromType->mStride && mBase->CanAssign(fromType->mBase);
	}
	else if (mType == DT_TYPE_POINTER)
	{
		if (fromType->mType == DT_TYPE_POINTER || fromType->mType == DT_TYPE_ARRAY)
		{
			if (mBase->mType == DT_TYPE_VOID || fromType->mBase->mType == DT_TYPE_VOID)
			{
				if (mCompilerOptions & COPT_CPLUSPLUS)
				{
					if (fromType == TheNullPointerTypeDeclaration)
						return true;
					return mBase->mType == DT_TYPE_VOID && ((mBase->mFlags & DTF_CONST) || !(fromType->mBase->mFlags & DTF_CONST));
				}
				else
					return true;
			}
			else if (mBase->mStripe == fromType->mBase->mStripe && mBase->IsSubType(fromType->mBase))
				return true;
		}
		else if (mBase->mType == DT_TYPE_FUNCTION && fromType->mType == DT_TYPE_FUNCTION)
		{
			return mBase->IsSubType(fromType);
		}
		else if (mBase->mType == DT_TYPE_VOID && fromType->mType == DT_TYPE_ASSEMBLER)
		{
			return true;
		}
		else if (mBase->mType == DT_TYPE_VOID && fromType->mType == DT_TYPE_FUNCTION)
		{
			return true;
		}
	}

	return false;
}

bool Declaration::IsIntegerType(void) const
{
	return mType == DT_TYPE_INTEGER || mType == DT_TYPE_BOOL || mType == DT_TYPE_ENUM;
}

bool Declaration::IsNumericType(void) const
{
	return mType == DT_TYPE_INTEGER || mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_ENUM;
}


bool Declaration::IsReference(void) const
{
	return mType == DT_TYPE_REFERENCE || mType == DT_TYPE_RVALUEREF;
}

bool Declaration::IsIndexed(void) const
{
	return mType == DT_TYPE_ARRAY || mType == DT_TYPE_POINTER;
}


bool Declaration::IsSimpleType(void) const
{
	return mType == DT_TYPE_INTEGER || mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_ENUM || mType == DT_TYPE_POINTER;
}

void Declaration::SetDefined(void)
{
	mFlags |= DTF_DEFINED;
	if (mConst)
		mConst->mFlags |= DTF_DEFINED;
}

Declaration* TheVoidTypeDeclaration, * TheConstVoidTypeDeclaration, * TheSignedIntTypeDeclaration, * TheUnsignedIntTypeDeclaration, * TheConstCharTypeDeclaration, * TheCharTypeDeclaration, * TheSignedCharTypeDeclaration, * TheUnsignedCharTypeDeclaration;
Declaration* TheBoolTypeDeclaration, * TheFloatTypeDeclaration, * TheConstVoidPointerTypeDeclaration, * TheVoidPointerTypeDeclaration, * TheSignedLongTypeDeclaration, * TheUnsignedLongTypeDeclaration;
Declaration* TheVoidFunctionTypeDeclaration, * TheConstVoidValueDeclaration;
Declaration* TheCharPointerTypeDeclaration, * TheConstCharPointerTypeDeclaration;
Expression* TheVoidExpression;
Declaration* TheNullptrConstDeclaration, * TheZeroIntegerConstDeclaration, * TheZeroFloatConstDeclaration, * TheNullPointerTypeDeclaration;

void InitDeclarations(void)
{
	static Location	noloc;
	TheVoidTypeDeclaration = new Declaration(noloc, DT_TYPE_VOID);
	TheVoidTypeDeclaration->mFlags = DTF_DEFINED;

	TheConstVoidTypeDeclaration = new Declaration(noloc, DT_TYPE_VOID);
	TheConstVoidTypeDeclaration->mFlags = DTF_DEFINED | DTF_CONST;

	TheVoidPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheVoidPointerTypeDeclaration->mBase = TheVoidTypeDeclaration;
	TheVoidPointerTypeDeclaration->mSize = 2;
	TheVoidPointerTypeDeclaration->mFlags = DTF_DEFINED;

	TheConstVoidPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheConstVoidPointerTypeDeclaration->mBase = TheConstVoidTypeDeclaration;
	TheConstVoidPointerTypeDeclaration->mSize = 2;
	TheConstVoidPointerTypeDeclaration->mFlags = DTF_DEFINED;

	TheNullPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheNullPointerTypeDeclaration->mBase = TheVoidTypeDeclaration;
	TheNullPointerTypeDeclaration->mSize = 2;
	TheNullPointerTypeDeclaration->mFlags = DTF_DEFINED;

	TheVoidFunctionTypeDeclaration = new Declaration(noloc, DT_TYPE_FUNCTION);
	TheVoidFunctionTypeDeclaration->mBase = TheVoidTypeDeclaration;
	TheVoidFunctionTypeDeclaration->mSize = 2;
	TheVoidFunctionTypeDeclaration->mFlags = DTF_DEFINED;

	TheConstVoidValueDeclaration = new Declaration(noloc, DT_CONST_INTEGER);
	TheVoidFunctionTypeDeclaration->mBase = TheVoidTypeDeclaration;
	TheVoidFunctionTypeDeclaration->mSize = 2;
	TheVoidFunctionTypeDeclaration->mInteger = 0;
	TheVoidFunctionTypeDeclaration->mFlags = DTF_DEFINED;

	TheSignedIntTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheSignedIntTypeDeclaration->mSize = 2;
	TheSignedIntTypeDeclaration->mFlags = DTF_DEFINED | DTF_SIGNED;

	TheUnsignedIntTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheUnsignedIntTypeDeclaration->mSize = 2;
	TheUnsignedIntTypeDeclaration->mFlags = DTF_DEFINED;

	TheSignedLongTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheSignedLongTypeDeclaration->mSize = 4;
	TheSignedLongTypeDeclaration->mFlags = DTF_DEFINED | DTF_SIGNED;

	TheUnsignedLongTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheUnsignedLongTypeDeclaration->mSize = 4;
	TheUnsignedLongTypeDeclaration->mFlags = DTF_DEFINED;

	TheSignedCharTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheSignedCharTypeDeclaration->mSize = 1;
	TheSignedCharTypeDeclaration->mFlags = DTF_DEFINED | DTF_SIGNED;

	TheConstCharTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheConstCharTypeDeclaration->mSize = 1;
	TheConstCharTypeDeclaration->mFlags = DTF_DEFINED | DTF_CONST;

	TheUnsignedCharTypeDeclaration = new Declaration(noloc, DT_TYPE_INTEGER);
	TheUnsignedCharTypeDeclaration->mSize = 1;
	TheUnsignedCharTypeDeclaration->mFlags = DTF_DEFINED;
	TheCharTypeDeclaration = TheUnsignedCharTypeDeclaration;

	TheBoolTypeDeclaration = new Declaration(noloc, DT_TYPE_BOOL);
	TheBoolTypeDeclaration->mSize = 1;
	TheBoolTypeDeclaration->mFlags = DTF_DEFINED;

	TheFloatTypeDeclaration = new Declaration(noloc, DT_TYPE_FLOAT);
	TheFloatTypeDeclaration->mSize = 4;
	TheFloatTypeDeclaration->mFlags = DTF_DEFINED | DTF_SIGNED;


	TheCharPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheCharPointerTypeDeclaration->mBase = TheCharTypeDeclaration;
	TheCharPointerTypeDeclaration->mSize = 2;
	TheCharPointerTypeDeclaration->mFlags = DTF_DEFINED;

	TheConstCharPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheConstCharPointerTypeDeclaration->mBase = TheConstCharTypeDeclaration;
	TheConstCharPointerTypeDeclaration->mSize = 2;
	TheConstCharPointerTypeDeclaration->mFlags = DTF_DEFINED;

	TheVoidExpression = new Expression(noloc, EX_CONSTANT);
	TheVoidExpression->mDecType = TheConstVoidTypeDeclaration;
	TheVoidExpression->mDecValue = TheConstVoidValueDeclaration;


	TheNullptrConstDeclaration = new Declaration(noloc, DT_CONST_ADDRESS);
	TheNullptrConstDeclaration->mBase = TheNullPointerTypeDeclaration;
	TheNullptrConstDeclaration->mSize = 2;
	TheZeroIntegerConstDeclaration = new Declaration(noloc, DT_CONST_INTEGER);
	TheZeroIntegerConstDeclaration->mBase = TheSignedIntTypeDeclaration;
	TheZeroIntegerConstDeclaration->mSize = 2;
	TheZeroFloatConstDeclaration = new Declaration(noloc, DT_CONST_FLOAT);
	TheZeroFloatConstDeclaration->mBase = TheFloatTypeDeclaration;
	TheZeroFloatConstDeclaration->mSize = 4;

}

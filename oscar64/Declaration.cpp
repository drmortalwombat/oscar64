#include "Declaration.h"

DeclarationScope::DeclarationScope(DeclarationScope* parent)
{
	mParent = parent;
	mHashSize = 0;
	mHashFill = 0;
	mHash = nullptr;
}

DeclarationScope::~DeclarationScope(void)
{
	delete[] mHash;
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

Declaration* DeclarationScope::Lookup(const Ident* ident)
{
	if (mHashSize > 0)
	{
		int		hm = mHashSize - 1;
		int		hi = ident->mHash & hm;

		while (mHash[hi].mIdent)
		{
			if (ident == mHash[hi].mIdent)
				return mHash[hi].mDec;
			hi = (hi + 1) & hm;
		}
	}

	return mParent ? mParent->Lookup(ident) : nullptr;
}


Expression::Expression(const Location& loc, ExpressionType type)
	:	mLocation(loc), mType(type), mLeft(nullptr), mRight(nullptr), mConst(false)
{

}

Expression::~Expression(void)
{

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

Expression* Expression::ConstantFold(Errors * errors)
{
	if (mType == EX_PREFIX && mLeft->mType == EX_CONSTANT)
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
				if (mLeft->mDecValue->mBase->mSize <= 2)
					dec->mBase = TheSignedIntTypeDeclaration;
				else
					dec->mBase = TheSignedLongTypeDeclaration;
				dec->mInteger = - mLeft->mDecValue->mInteger;
				ex->mDecValue = dec;
				ex->mDecType = dec->mBase;
				return ex;
			}
			case TK_BINARY_NOT:
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = mLeft->mDecValue->mBase;
				dec->mInteger = ~mLeft->mDecValue->mInteger;
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
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}
#endif
	else if (mType == EX_TYPECAST && mRight->mType == EX_CONSTANT)
	{
		if (mLeft->mDecType->mType == DT_TYPE_POINTER)
		{
			if (mRight->mDecValue->mType == DT_CONST_ADDRESS || mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_ADDRESS);
				dec->mBase = mLeft->mDecType;
				dec->mInteger = mRight->mDecValue->mInteger;
				ex->mDecValue = dec;
				ex->mDecType = mLeft->mDecType;
				return ex;
			}
		}
		else if (mLeft->mDecType->mType == DT_TYPE_INTEGER)
		{
			if (mRight->mDecValue->mType == DT_CONST_FLOAT)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = mLeft->mDecType;
				dec->mInteger = mRight->mDecValue->mNumber;
				ex->mDecValue = dec;
				ex->mDecType = mLeft->mDecType;
				return ex;
			}
			else if (mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				int64	sval = 1ULL << (8 * mLeft->mDecType->mSize);
				int64	v = mRight->mDecValue->mInteger & (sval - 1);

				if (mLeft->mDecType->mFlags & DTF_SIGNED)
				{
					if (v & (sval >> 1))
						v -= sval;
				}

				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_INTEGER);
				dec->mBase = mLeft->mDecType;
				dec->mInteger = v;
				ex->mDecValue = dec;
				ex->mDecType = mLeft->mDecType;
				return ex;
			}
		}
		else if (mLeft->mDecType->mType == DT_TYPE_FLOAT)
		{
			if (mRight->mDecValue->mType == DT_CONST_INTEGER)
			{
				Expression* ex = new Expression(mLocation, EX_CONSTANT);
				Declaration* dec = new Declaration(mLocation, DT_CONST_FLOAT);
				dec->mBase = mLeft->mDecType;
				dec->mNumber = mRight->mDecValue->mInteger;
				ex->mDecValue = dec;
				ex->mDecType = mLeft->mDecType;
				return ex;
			}
		}
	}
	else if (mType == EX_BINARY && mLeft->mType == EX_CONSTANT && mRight->mType == EX_CONSTANT)
	{
		if (mLeft->mDecValue->mType == DT_CONST_INTEGER && mRight->mDecValue->mType == DT_CONST_INTEGER)
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
			case TK_MUL:
				ival = ileft * iright;
				break;
			case TK_DIV:
				if (iright == 0)
					errors->Error(mLocation, EERR_INVALID_VALUE, "Constant division by zero");
				else
					ival = ileft / iright;
				break;
			case TK_MOD:
				if (iright == 0)
					errors->Error(mLocation, EERR_INVALID_VALUE, "Constant division by zero");
				else
					ival = ileft % iright;
				break;
			case TK_LEFT_SHIFT:
				ival = ileft << iright;
				break;
			case TK_RIGHT_SHIFT:
				ival = ileft >> iright;
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
			if (mLeft->mDecValue->mBase->mSize <= 2 && mRight->mDecValue->mBase->mSize <= 2)
				dec->mBase = ival < 32768 ? TheSignedIntTypeDeclaration : TheUnsignedIntTypeDeclaration;
			else
				dec->mBase = ival < 2147483648 ? TheSignedLongTypeDeclaration : TheUnsignedLongTypeDeclaration;
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
	else if (mType == EX_BINARY && mToken == TK_ADD && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_ARRAY && mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue;
		dec->mSize = mLeft->mDecType->mBase->mSize - mRight->mDecValue->mInteger * dec->mSize;
		dec->mOffset = mRight->mDecValue->mInteger * dec->mSize;
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
		dec->mSize = mLeft->mDecType->mBase->mSize - mRight->mDecValue->mInteger * dec->mSize;
		dec->mOffset = mLeft->mDecValue->mOffset + mRight->mDecValue->mInteger * dec->mSize;
		ex->mDecValue = dec;
		ex->mDecType = mLeft->mDecType;
		return ex;
	}
	else if (mType == EX_BINARY && mToken == TK_ADD && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_CONST) && mLeft->mDecType->mType == DT_TYPE_POINTER && mRight->mType == EX_CONSTANT && mRight->mDecValue->mType == DT_CONST_INTEGER)
	{
		mLeft = mLeft->mDecValue->mValue;
		return this->ConstantFold(errors);
	}
	else if (mType == EX_QUALIFY && mLeft->mType == EX_VARIABLE && mLeft->mDecValue->mType == DT_VARIABLE && (mLeft->mDecValue->mFlags & DTF_GLOBAL) && mLeft->mDecType->mType == DT_TYPE_STRUCT)
	{
		Expression* ex = new Expression(mLocation, EX_VARIABLE);
		Declaration* dec = new Declaration(mLocation, DT_VARIABLE_REF);
		dec->mFlags = mLeft->mDecValue->mFlags;
		dec->mBase = mLeft->mDecValue;
		dec->mOffset = mDecValue->mOffset;
		dec->mSize = mDecValue->mSize;
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
		ex->mDecValue = dec;
		ex->mDecType = mDecType;
		return ex;
	}

	return this;
}

Declaration::Declaration(const Location& loc, DecType type)
	: mLocation(loc), mType(type), mScope(nullptr), mData(nullptr), mIdent(nullptr), mSize(0), mOffset(0), mFlags(0), mComplexity(0), mLocalSize(0), 
	mBase(nullptr), mParams(nullptr), mValue(nullptr), mNext(nullptr), mVarIndex(-1), mLinkerObject(nullptr), mCallers(nullptr), mCalled(nullptr), mAlignment(1), 
	mInteger(0), mNumber(0), mMinValue(-0x80000000LL), mMaxValue(0x7fffffffLL), mFastCallBase(0), mFastCallSize(0)
{}

Declaration::~Declaration(void)
{
	delete mScope;
	delete[] mData;
}

Declaration* Declaration::ToConstType(void)
{
	if (mFlags & DTF_CONST)
		return this;

	Declaration* ndec = new Declaration(mLocation, mType);
	ndec->mSize = mSize;
	ndec->mBase = mBase;
	ndec->mFlags = mFlags | DTF_CONST;
	ndec->mScope = mScope;
	ndec->mParams = mParams;
	ndec->mIdent = mIdent;
	
	return ndec;
}

bool Declaration::IsSubType(const Declaration* dec) const
{
	if (this == dec)
		return true;

	if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
	{
		if (dec->mType == DT_TYPE_POINTER)
			return mBase->IsSubType(dec->mBase);
	}

	if (mType != dec->mType)
		return false;
	if (mSize != dec->mSize)
		return false;

	if ((mFlags & DTF_SIGNED) != (dec->mFlags & DTF_SIGNED))
		return false;

	if ((dec->mFlags & ~mFlags) & (DTF_CONST | DTF_VOLATILE))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_STRUCT)
		return mScope == dec->mScope || (mIdent == dec->mIdent && mSize == dec->mSize);
	else if (mType == DT_TYPE_STRUCT || mType == DT_TYPE_ENUM || mType == DT_TYPE_UNION)
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

bool Declaration::IsConstSame(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (mType != dec->mType)
		return false;
	if (mSize != dec->mSize)
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

	return false;
}

bool Declaration::IsSame(const Declaration* dec) const
{
	if (this == dec)
		return true;
	if (mType != dec->mType)
		return false;
	if (mSize != dec->mSize)
		return false;

	if ((mFlags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)) != (dec->mFlags & (DTF_SIGNED | DTF_CONST | DTF_VOLATILE)))
		return false;

	if (mType == DT_TYPE_INTEGER)
		return true;
	else if (mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_VOID)
		return true;
	else if (mType == DT_TYPE_ENUM)
		return mIdent == dec->mIdent;
	else if (mType == DT_TYPE_POINTER || mType == DT_TYPE_ARRAY)
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

	return false;
}

bool Declaration::CanAssign(const Declaration* fromType) const
{
	if (this->IsSame(fromType))
		return true;
	else if (IsNumericType())
	{
		if (fromType->IsNumericType())
			return true;
	}
	else if (mType == DT_TYPE_STRUCT && fromType->mType == DT_TYPE_STRUCT)
		return mScope == fromType->mScope || (mIdent == fromType->mIdent && mSize == fromType->mSize);
	else if (mType == DT_TYPE_POINTER)
	{
		if (fromType->mType == DT_TYPE_POINTER || fromType->mType == DT_TYPE_ARRAY)
		{
			if (mBase->mType == DT_TYPE_VOID || fromType->mBase->mType == DT_TYPE_VOID)
				return true;
			else if (mBase->IsSubType(fromType->mBase))
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

bool Declaration::IsSimpleType(void) const
{
	return mType == DT_TYPE_INTEGER || mType == DT_TYPE_BOOL || mType == DT_TYPE_FLOAT || mType == DT_TYPE_ENUM || mType == DT_TYPE_POINTER;
}


Declaration* TheVoidTypeDeclaration, * TheSignedIntTypeDeclaration, * TheUnsignedIntTypeDeclaration, * TheConstCharTypeDeclaration, * TheCharTypeDeclaration, * TheSignedCharTypeDeclaration, * TheUnsignedCharTypeDeclaration;
Declaration* TheBoolTypeDeclaration, * TheFloatTypeDeclaration, * TheVoidPointerTypeDeclaration, * TheSignedLongTypeDeclaration, * TheUnsignedLongTypeDeclaration;
Declaration* TheVoidFunctionTypeDeclaration, * TheConstVoidValueDeclaration;
Declaration* TheCharPointerTypeDeclaration, * TheConstCharPointerTypeDeclaration;

void InitDeclarations(void)
{
	static Location	noloc;
	TheVoidTypeDeclaration = new Declaration(noloc, DT_TYPE_VOID);
	TheVoidTypeDeclaration->mFlags = DTF_DEFINED;

	TheVoidPointerTypeDeclaration = new Declaration(noloc, DT_TYPE_POINTER);
	TheVoidPointerTypeDeclaration->mBase = TheVoidTypeDeclaration;
	TheVoidPointerTypeDeclaration->mSize = 2;
	TheVoidPointerTypeDeclaration->mFlags = DTF_DEFINED;

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
}

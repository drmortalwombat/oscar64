#include "Constexpr.h"
#include <math.h>

ConstexprInterpreter::Value::Value(void)
	: mDecType(TheVoidTypeDeclaration),
	mBaseValue(nullptr), mOffset(0),
	mData(mShortData), mDataSize(0)
{
}

ConstexprInterpreter::Value::Value(const Location & location)
	: mLocation(location), 
	mDecType(TheVoidTypeDeclaration),
	mBaseValue(nullptr), mOffset(0),
	mData(mShortData), mDataSize(0)
{
}

ConstexprInterpreter::Value::Value(Expression* exp)
	: mLocation(exp->mLocation), 
	mDecType(exp->mDecType), 
	mBaseValue(nullptr), mOffset(0),
	mDataSize(exp->mDecType->mSize)
{
	assert(exp->mType == EX_CONSTANT);

	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];

	PutConst(0, exp->mDecValue);
}

void ConstexprInterpreter::Value::PutConst(int offset, Declaration* dec)
{
	switch (dec->mType)
	{
	case DT_CONST_INTEGER:
		PutIntAt(dec->mInteger, offset, dec->mBase);
		break;
	case DT_CONST_FLOAT:
		PutFloatAt(float(dec->mNumber), offset, dec->mBase);
		break;
	case DT_CONST_STRUCT:
		for (Declaration* pdec = dec->mParams; pdec; pdec = pdec->mNext)
			PutConst(pdec->mOffset, pdec);
		break;
	}
}

ConstexprInterpreter::Value::Value(const Location& location, Declaration* dec)
	: mLocation(location),
	mDecType(dec),
	mBaseValue(nullptr), mOffset(0),
	mDataSize(dec->mSize)
{
	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];
}

ConstexprInterpreter::Value::Value(const Location& location, Declaration* dec, int size)
	: mLocation(location),
	mDecType(dec),
	mBaseValue(nullptr), mOffset(0),
	mDataSize(size)
{
	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];
}

ConstexprInterpreter::Value::Value(const Value& value)
	: mLocation(value.mLocation),
	mDecType(value.mDecType),
	mBaseValue(value.mBaseValue), mOffset(value.mOffset),
	mDataSize(value.mDataSize)
	  
{
	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];

	for (int i = 0; i < mDataSize; i++)
		mData[i] = value.mData[i];
}

ConstexprInterpreter::Value::Value(Value&& value)
	: mLocation(value.mLocation),
	mDecType(value.mDecType),
	mBaseValue(value.mBaseValue), mOffset(value.mOffset),
	mDataSize(value.mDataSize)
{
	if (mDataSize <= 4)
	{
		mData = mShortData;
		for (int i = 0; i < mDataSize; i++)
			mData[i] = value.mData[i];
	}
	else
	{
		mData = value.mData;
		value.mData = value.mShortData;
	}
}

ConstexprInterpreter::Value::Value(Value* value)
	: mLocation(value->mLocation),
	mDecType(value->mDecType),
	mBaseValue(value), mOffset(0),
	mDataSize(0), mData(mShortData)
{
}

ConstexprInterpreter::Value::Value(Value* value, Declaration* type, int offset)
	: mLocation(value->mLocation),
	mDecType(type),
	mBaseValue(value), mOffset(offset),
	mDataSize(0), mData(mShortData)
{
}

ConstexprInterpreter::Value::Value(const Location& location, const uint8* data, Declaration* type)
	: mLocation(location),
	mDecType(type),
	mBaseValue(nullptr), mOffset(0),
	mDataSize(type->mSize)
{
	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];

	for (int i = 0; i < mDataSize; i++)
		mData[i].mByte = data[i];
}

ConstexprInterpreter::Value::Value(const Location& location, const ValueItem* data, Declaration* type)
	: mLocation(location),
	mDecType(type),
	mBaseValue(nullptr), mOffset(0),
	mDataSize(type->mSize)
{
	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];

	for (int i = 0; i < mDataSize; i++)
		mData[i] = data[i];
}


ConstexprInterpreter::Value::~Value(void)
{
	if (mData != mShortData)
		delete[] mData;
}



ConstexprInterpreter::Value& ConstexprInterpreter::Value::operator=(const Value& v)
{
	if (mData != mShortData)
	{
		delete[] mData;
		mData = mShortData;
	}

	mLocation = v.mLocation;
	mDecType = v.mDecType;
	mBaseValue = v.mBaseValue;
	mDataSize = v.mDataSize;
	mOffset = v.mOffset;

	if (mDataSize <= 4)
		mData = mShortData;
	else
		mData = new ValueItem[mDataSize];

	for (int i = 0; i < mDataSize; i++)
		mData[i] = v.mData[i];

	return *this;
}

ConstexprInterpreter::Value& ConstexprInterpreter::Value::operator=(Value&& v)
{
	if (mData != mShortData)
	{
		delete[] mData;
		mData = mShortData;
	}

	mLocation = v.mLocation;
	mDecType = v.mDecType;
	mBaseValue = v.mBaseValue;
	mDataSize = v.mDataSize;
	mOffset = v.mOffset;

	if (mDataSize <= 4)
	{
		mData = mShortData;
		for (int i = 0; i < mDataSize; i++)
			mData[i] = v.mData[i];
	}
	else
	{
		mData = v.mData;
		v.mData = v.mShortData;
	}

	return *this;
}


ConstexprInterpreter::ValueItem* ConstexprInterpreter::Value::GetAddr(void)
{
	if (mBaseValue)
		return mBaseValue->mData + mOffset;
	else
		return mData;
}

const ConstexprInterpreter::ValueItem* ConstexprInterpreter::Value::GetAddr(void) const
{
	if (mBaseValue)
		return mBaseValue->mData + mOffset;
	else
		return mData;
}

int64 ConstexprInterpreter::Value::GetInt(void) const
{
	return GetIntAt(0, mDecType);
}

double ConstexprInterpreter::Value::GetFloat(void) const
{
	return GetFloatAt(0, mDecType);
}

ConstexprInterpreter::Value ConstexprInterpreter::Value::GetPtr(void) const
{
	return GetPtrAt(0, mDecType->mBase);
}

void ConstexprInterpreter::Value::PutInt(int64 v)
{
	PutIntAt(v, 0, mDecType);
}

void ConstexprInterpreter::Value::PutFloat(double v)
{
	PutFloatAt(v, 0, mDecType);
}

void ConstexprInterpreter::Value::PutPtr(const Value& v)
{
	PutPtrAt(v, 0, mDecType);
}


int64 ConstexprInterpreter::Value::GetIntAt(int at, Declaration* type) const
{
	if (type->mType == DT_TYPE_FLOAT)
	{
		return int64(GetFloatAt(at, type));
	}
	else
	{
		const ValueItem* dp = GetAddr() + at;

		if (type->mFlags & DTF_SIGNED)
		{
			switch (type->mSize)
			{
			case 1:
				return int8(dp[0].mByte);
			case 2:
				return int16(dp[0].mByte | ((uint32)(dp[1].mByte) << 8));
			case 4:
				return int32(dp[0].mByte | ((uint32)(dp[1].mByte) << 8) | ((uint32)(dp[2].mByte) << 16) | ((uint32)(dp[3].mByte) << 24));
			}
		}
		else
		{
			switch (type->mSize)
			{
			case 1:
				return uint8(dp[0].mByte);
			case 2:
				return uint16(dp[0].mByte | ((uint32)(dp[1].mByte) << 8));
			case 4:
				return uint32(dp[0].mByte | ((uint32)(dp[1].mByte) << 8) | ((uint32)(dp[2].mByte) << 16) | ((uint32)(dp[3].mByte) << 24));
			}
		}
	}

	return 0;
}

double ConstexprInterpreter::Value::GetFloatAt(int at, Declaration* type) const
{
	if (type->mType == DT_TYPE_FLOAT)
	{
		const ValueItem* dp = GetAddr() + at;

		union
		{
			float	f;
			uint32	i;
		}	u;

		u.i = uint32(dp[0].mByte | ((uint32)(dp[1].mByte) << 8) | ((uint32)(dp[2].mByte) << 16) | ((uint32)(dp[3].mByte) << 24));
		return u.f;
	}
	else
	{
		int64	iv = GetIntAt(at, type);
		return double(iv);
	}

}

ConstexprInterpreter::Value ConstexprInterpreter::Value::GetPtrAt(int at, Declaration* type) const
{
	const ValueItem* dp = GetAddr() + at;

	return Value(dp->mBaseValue, type, uint16(dp[0].mByte | ((uint32)(dp[1].mByte) << 8)));
}

void ConstexprInterpreter::Value::PutIntAt(int64 v, int at, Declaration* type)
{
	if (type->mType == DT_TYPE_FLOAT)
	{
		PutFloatAt(float(v), at, type);
	}
	else
	{
		ValueItem* dp = GetAddr() + at;

		switch (type->mSize)
		{
		case 4:
			dp[3].mByte = uint8((v >> 24) & 0xff);
			dp[2].mByte = uint8((v >> 16) & 0xff);
		case 2:
			dp[1].mByte = uint8((v >> 8) & 0xff);
		case 1:
			dp[0].mByte = uint8(v & 0xff);
		}
	}
}

void ConstexprInterpreter::Value::PutFloatAt(double v, int at, Declaration* type)
{
	if (type->mType == DT_TYPE_FLOAT)
	{
		ValueItem* dp = GetAddr() + at;

		union
		{
			float	f;
			uint32	i;
		}	u;

		u.f = float(v);

		dp[3].mByte = uint8((u.i >> 24) & 0xff);
		dp[2].mByte = uint8((u.i >> 16) & 0xff);
		dp[1].mByte = uint8((u.i >> 8) & 0xff);
		dp[0].mByte = uint8(u.i & 0xff);
	}
	else
	{
		PutIntAt(int64(v), at, type);
	}
}

void ConstexprInterpreter::Value::PutPtrAt(const Value& v, int at, Declaration* type)
{
	ValueItem* dp = GetAddr() + at;

	dp[0].mBaseValue = v.mBaseValue;
	dp[1].mByte = uint8((v.mOffset >> 8) & 0xff);
	dp[0].mByte = uint8(v.mOffset & 0xff);
}

ConstexprInterpreter::Value ConstexprInterpreter::Value::ToRValue(void) const
{
	if (mBaseValue)
		return Value(mLocation, GetAddr(), mDecType);
	else
		return *this;
}

void ConstexprInterpreter::Value::Assign(const Value& v)
{
	switch (mDecType->mType)
	{
	case DT_TYPE_INTEGER:
	case DT_TYPE_BOOL:
		PutInt(v.GetInt());
		break;
	case DT_TYPE_FLOAT:
		PutFloat(v.GetFloat());
		break;
	case DT_TYPE_STRUCT:
	case DT_TYPE_UNION:
	case DT_TYPE_ARRAY:
		memcpy(GetAddr(), v.GetAddr(), mDecType->mSize);
		break;
	case DT_TYPE_POINTER:
		PutPtr(v.GetPtr());
		break;
	}
}

Declaration* ConstexprInterpreter::Value::GetConst(int offset, Declaration* type, LinkerSection* dataSection) const
{
	Declaration* dec = nullptr;

	switch (type->mType)
	{
	case DT_TYPE_INTEGER:
	case DT_TYPE_BOOL:
		dec = new Declaration(mLocation, DT_CONST_INTEGER);
		dec->mBase = type;
		dec->mFlags = type->mFlags & DTF_SIGNED;
		dec->mSize = type->mSize;
		dec->mInteger = GetIntAt(offset, type);
		break;
	case DT_TYPE_FLOAT:
		dec = new Declaration(mLocation, DT_CONST_FLOAT);
		dec->mBase = type;
		dec->mSize = type->mSize;
		dec->mNumber = GetFloatAt(offset, type);
		break;
	case DT_TYPE_STRUCT:
	{
		dec = new Declaration(mLocation, DT_CONST_STRUCT);	
		dec->mBase = type;
		dec->mSize = type->mSize;
		dec->mSection = dataSection;
		dec->mOffset = offset;
		Declaration* ldec = nullptr;
		for (Declaration* mdec = type->mParams; mdec; mdec = mdec->mNext)
		{
			Declaration	*	cdec = GetConst(offset + mdec->mOffset, mdec->mBase, dataSection);
			cdec->mOffset = mdec->mOffset;

			if (ldec)
				ldec->mNext = cdec;
			else
				dec->mParams = cdec;

			ldec = cdec;

		}
		break;
	}
	case DT_TYPE_ARRAY:
	{
		dec = new Declaration(mLocation, DT_CONST_STRUCT);
		dec->mBase = type;
		dec->mSize = type->mSize;
		dec->mSection = dataSection;
		dec->mOffset = offset;
		Declaration* ldec = nullptr;
		for (int i=0; i<type->mSize; i += type->mBase->mSize)
		{
			Declaration* cdec = GetConst(offset + i, type->mBase, dataSection);
			cdec->mOffset = i;

			if (ldec)
				ldec->mNext = cdec;
			else
				dec->mParams = cdec;

			ldec = cdec;
		}
		break;
	}

	}

	return dec;
}

Expression* ConstexprInterpreter::Value::ToExpression(LinkerSection* dataSection) const
{
	Expression* exp = new Expression(mLocation, EX_CONSTANT);

	exp->mDecType = mDecType;
	exp->mDecValue = GetConst(0, mDecType, dataSection);

	return exp;
}


ConstexprInterpreter::ConstexprInterpreter(const Location & location, Errors* err, LinkerSection* dataSection)
	: mLocation(location), mErrors(err), mDataSection(dataSection), mParams(Value()), mLocals(Value()), mHeap(nullptr)
{

}

ConstexprInterpreter::~ConstexprInterpreter(void)
{

}

ConstexprInterpreter::Value* ConstexprInterpreter::NewValue(Expression* exp, Declaration* type, int size)
{
	Value* v = new Value(exp->mLocation, type, size);
	mHeap->Push(v);
	return v;
}

void ConstexprInterpreter::DeleteValue(Value* v)
{
	int i = mHeap->IndexOf(v);
	if (i >= 0)
	{
		delete v;
		mHeap->Remove(i);
	}
	else
		mErrors->Error(v->mLocation, EERR_DOUBLE_FREE, "Freeing not allocated memory");
}

Expression* ConstexprInterpreter::EvalCall(Expression* exp)
{
	mProcType = exp->mLeft->mDecType;

	Expression* pex = exp->mRight;
	Declaration* dec = exp->mLeft->mDecType->mParams;

	int	pos = 0;
	while (pex && pex->mType == EX_LIST)
	{
		if (dec)
			pos = dec->mVarIndex;

		if (pex->mLeft->mType == EX_CONSTANT)
			mParams[pos] = Value(pex->mLeft);
		else if (pex->mLeft->mType == EX_VARIABLE && (pex->mLeft->mDecValue->mFlags & DTF_CONST))
		{
			mParams[pos] = Value(pex->mLeft->mLocation, pex->mLeft->mDecValue->mBase);
			mParams[pos].PutConst(0, pex->mLeft->mDecValue->mValue->mDecValue);
		}
		else
			return exp;

		pex = pex->mRight;
		if (dec)
			dec = dec->mNext;
	}
	if (pex)	
	{
		if (dec)
			pos = dec->mVarIndex;

		if (pex->mType == EX_CONSTANT)
			mParams[pos] = Value(pex);
		else if (pex->mType == EX_VARIABLE && (pex->mDecValue->mFlags & DTF_CONST))
		{
			mParams[pos] = Value(pex->mLocation, pex->mDecValue->mBase);
			if (pex->mDecValue->mSize > 0)
				mParams[pos].PutConst(0, pex->mDecValue->mValue->mDecValue);
		}
		else
			return exp;
	}

	mHeap = new ExpandingArray<Value*>();

	Execute(exp->mLeft->mDecValue->mValue);

	if (mHeap->Size() > 0)
		mErrors->Error(exp->mLocation, EERR_UNBALANCED_HEAP_USE, "Unbalanced heap use in constexpr");
	delete mHeap;

	return mResult.ToExpression(mDataSection);
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalBinary(Expression * exp, const Value& vl, const Value& vr)
{
	Value	v(exp->mLocation, exp->mDecType);

	if (exp->mDecType->mType == DT_TYPE_FLOAT)
	{
		switch (exp->mToken)
		{
		case TK_ADD:
		case TK_ASSIGN_ADD:
			v.PutFloat(vl.GetFloat() + vr.GetFloat());
			break;
		case TK_SUB:
		case TK_ASSIGN_SUB:
			v.PutFloat(vl.GetFloat() - vr.GetFloat());
			break;
		case TK_MUL:
		case TK_ASSIGN_MUL:
			v.PutFloat(vl.GetFloat() * vr.GetFloat());
			break;
		case TK_DIV:
		case TK_ASSIGN_DIV:
			if (vr.GetInt() == 0)
				mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Constant division by zero");
			else
				v.PutFloat(vl.GetFloat() / vr.GetFloat());
			break;
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}
	else
	{
		switch (exp->mToken)
		{
		case TK_ADD:
		case TK_ASSIGN_ADD:
			v.PutInt(vl.GetInt() + vr.GetInt());
			break;
		case TK_SUB:
		case TK_ASSIGN_SUB:
			v.PutInt(vl.GetInt() - vr.GetInt());
			break;
		case TK_MUL:
		case TK_ASSIGN_MUL:
			v.PutInt(vl.GetInt() * vr.GetInt());
			break;
		case TK_DIV:
		case TK_ASSIGN_DIV:
			if (vr.GetInt() == 0)
				mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Constant division by zero");
			else
				v.PutInt(vl.GetInt() / vr.GetInt());
			break;
		case TK_MOD:
		case TK_ASSIGN_MOD:
			if (vr.GetInt() == 0)
				mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Constant division by zero");
			else
				v.PutInt(vl.GetInt() % vr.GetInt());
			break;
		case TK_LEFT_SHIFT:
		case TK_ASSIGN_SHL:
			v.PutInt(vl.GetInt() << vr.GetInt());
			break;
		case TK_RIGHT_SHIFT:
		case TK_ASSIGN_SHR:
			v.PutInt(vl.GetInt() >> vr.GetInt());
			break;
		case TK_BINARY_AND:
		case TK_ASSIGN_AND:
			v.PutInt(vl.GetInt() & vr.GetInt());
			break;
		case TK_BINARY_OR:
		case TK_ASSIGN_OR:
			v.PutInt(vl.GetInt() | vr.GetInt());
			break;
		case TK_BINARY_XOR:
		case TK_ASSIGN_XOR:
			v.PutInt(vl.GetInt() ^ vr.GetInt());
			break;
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}

	return v;
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalRelational(Expression* exp, const Value& vl, const Value& vr)
{
	Value	v(exp->mLocation, TheBoolTypeDeclaration);

	bool	check = false;
	if (vl.mDecType->mType == DT_TYPE_FLOAT || vr.mDecType->mType == DT_TYPE_FLOAT)
	{
		switch (exp->mToken)
		{
		case TK_EQUAL:
			check = vl.GetFloat() == vr.GetFloat();
			break;
		case TK_NOT_EQUAL:
			check = vl.GetFloat() != vr.GetFloat();
			break;
		case TK_GREATER_THAN:
			check = vl.GetFloat() > vr.GetFloat();
			break;
		case TK_GREATER_EQUAL:
			check = vl.GetFloat() >= vr.GetFloat();
			break;
		case TK_LESS_THAN:
			check = vl.GetFloat() < vr.GetFloat();
			break;
		case TK_LESS_EQUAL:
			check = vl.GetFloat() <= vr.GetFloat();
			break;
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}
	else if (vl.mDecType->mType == DT_TYPE_POINTER && vr.mDecType->mType == DT_TYPE_POINTER)
	{
		Value	pl = vl.GetPtr();
		Value	pr = vr.GetPtr();

		if (pl.mBaseValue == pr.mBaseValue)
		{
			switch (exp->mToken)
			{
			case TK_EQUAL:
				check = pl.mOffset == pr.mOffset;
				break;
			case TK_NOT_EQUAL:
				check = pl.mOffset != pr.mOffset;
				break;
			case TK_GREATER_THAN:
				check = pl.mOffset > pr.mOffset;
				break;
			case TK_GREATER_EQUAL:
				check = pl.mOffset >= pr.mOffset;
				break;
			case TK_LESS_THAN:
				check = pl.mOffset < pr.mOffset;
				break;
			case TK_LESS_EQUAL:
				check = pl.mOffset <= pr.mOffset;
				break;
			default:
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
			}
		}
		else
		{
			switch (exp->mToken)
			{
			case TK_EQUAL:
				check = false;
				break;
			case TK_NOT_EQUAL:
				check = true;
				break;
			default:
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
			}
		}
	}
	else
	{
		switch (exp->mToken)
		{
		case TK_EQUAL:
			check = vl.GetInt() == vr.GetInt();
			break;
		case TK_NOT_EQUAL:
			check = vl.GetInt() != vr.GetInt();
			break;
		case TK_GREATER_THAN:
			check = vl.GetInt() > vr.GetInt();
			break;
		case TK_GREATER_EQUAL:
			check = vl.GetInt() >= vr.GetInt();
			break;
		case TK_LESS_THAN:
			check = vl.GetInt() < vr.GetInt();
			break;
		case TK_LESS_EQUAL:
			check = vl.GetInt() <= vr.GetInt();
			break;
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}

	v.PutInt(check ? 1 : 0);

	return v;
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalUnary(Expression* exp, const Value& vl)
{
	Value	v(exp->mLocation, exp->mDecType);

	if (exp->mDecType->mType == DT_TYPE_FLOAT)
	{
		switch (exp->mToken)
		{
		case TK_ADD:
			v.PutFloat(vl.GetFloat());
			break;
		case TK_SUB:
			v.PutFloat(-vl.GetFloat());
			break;
		case TK_INC:
			v.PutFloat(vl.GetFloat() + 1);
			break;
		case TK_DEC:
			v.PutFloat(vl.GetFloat() - 1);
			break;
		case TK_MUL:
			return vl.GetPtr();
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}
	else
	{
		switch (exp->mToken)
		{
		case TK_ADD:
			v.PutInt(vl.GetInt());
			break;
		case TK_SUB:
			v.PutInt(-vl.GetInt());
			break;
		case TK_BINARY_NOT:
			v.PutInt(~vl.GetInt());
			break;
		case TK_INC:
			v.PutInt(vl.GetInt() + 1);
			break;
		case TK_DEC:
			v.PutInt(vl.GetInt() - 1);
			break;
		case TK_BINARY_AND:
			if (vl.mBaseValue)
				v.PutPtr(vl);
			else
				mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Not an addressable value");
			break;
		case TK_MUL:
			return vl.GetPtr();
		case TK_NEW:
			v.PutPtr(Value(NewValue(exp, exp->mDecType->mBase, int(vl.GetInt()))));
			break;
		case TK_DELETE:
			DeleteValue(vl.GetPtr().mBaseValue);
			break;
		default:
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
		}
	}

	return v;
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalTypeCast(Expression* exp, const Value& vl, Declaration* type)
{
	Value	v(exp->mLocation, type);

	if (type->mType == DT_TYPE_FLOAT)
		v.PutFloat(vl.GetFloat());
	else if (type->IsIntegerType())
		v.PutInt(vl.GetInt());

	return v;
}

ConstexprInterpreter::Value ConstexprInterpreter::REval(Expression* exp)
{
	return Eval(exp).ToRValue();
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalCall(Expression* exp, ConstexprInterpreter* caller)
{
	mHeap = caller->mHeap;

	mProcType = exp->mLeft->mDecType;

	Expression* pex = exp->mRight;
	Declaration* dec = exp->mLeft->mDecType->mParams;

	int	pos = 0;
	while (pex && pex->mType == EX_LIST)
	{
		if (dec)
			pos = dec->mVarIndex;

		if (dec)
			mParams[pos] = caller->EvalCoerce(pex->mLeft, caller->Eval(pex->mLeft), dec->mBase);
		else
			mParams[pos] = caller->REval(pex->mLeft);

		pos += dec->mSize;
		pex = pex->mRight;
		if (dec)
			dec = dec->mNext;
	}
	if (pex)
	{
		if (dec)
			pos = dec->mVarIndex;
		if (dec)
			mParams[pos] = caller->EvalCoerce(pex, caller->Eval(pex), dec->mBase);
		else
			mParams[pos] = caller->REval(pex);
	}

	if (exp->mLeft->mDecValue->mFlags & DTF_INTRINSIC)
	{
		const Ident* iname = exp->mLeft->mDecValue->mIdent;

		if (!strcmp(iname->mString, "fabs"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(fabs(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "floor"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(floor(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "ceil"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(ceil(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "sin"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(sin(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "cos"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(cos(mParams[0].GetFloat()));
		}
		else
			mErrors->Error(exp->mLeft->mDecValue->mLocation, EERR_OBJECT_NOT_FOUND, "Unknown intrinsic function", iname);
	}
	else
	{
		Execute(exp->mLeft->mDecValue->mValue);
		UnwindDestructStack(0);
	}

	return mResult;
}

ConstexprInterpreter::Value ConstexprInterpreter::EvalCoerce(Expression* exp, const Value& vl, Declaration* type)
{
	if (type->IsReference())
		return vl;
	else
	{
		Value	v = vl.ToRValue();
		while (v.mDecType->IsReference())
			v.mDecType = v.mDecType->mBase;

		if (type->mType == DT_TYPE_FLOAT && v.mDecType->IsIntegerType())
		{
			Value	vf(exp->mLocation, type);
			vf.PutFloat(v.GetFloat());
			v = vf;
		}
		else if (v.mDecType->mType == DT_TYPE_FLOAT && type->IsIntegerType())
		{
			Value	vf(exp->mLocation, type);
			vf.PutInt(v.GetInt());
			v = vf;
		}

		return v;
	}
}

ConstexprInterpreter::Value ConstexprInterpreter::Eval(Expression* exp)
{
	switch (exp->mType)
	{
	case EX_SCOPE:
		return Eval(exp->mLeft);
	case EX_CONSTANT:
		return Value(exp);
	case EX_VARIABLE:
		if (exp->mDecValue->mType == DT_ARGUMENT)
		{
			if (mParams[exp->mDecValue->mVarIndex].mBaseValue)
				return mParams[exp->mDecValue->mVarIndex];
			else
				return Value(&mParams[exp->mDecValue->mVarIndex]);
		}
		else if (exp->mDecValue->mType == DT_VARIABLE)
		{
			if (!(exp->mDecValue->mFlags & (DTF_STATIC | DTF_GLOBAL)))
			{
				if (!mLocals[exp->mDecValue->mVarIndex].mDataSize)
				{
					mLocals[exp->mDecValue->mVarIndex] = Value(exp->mDecValue->mLocation, exp->mDecValue->mBase);
					return Value(&mLocals[exp->mDecValue->mVarIndex]);
				}
				else if (mLocals[exp->mDecValue->mVarIndex].mBaseValue)
					return mLocals[exp->mDecValue->mVarIndex];
				else
					return Value(&mLocals[exp->mDecValue->mVarIndex]);
			}
		}
		break;
	case EX_BINARY:
		return EvalBinary(exp, REval(exp->mLeft), REval(exp->mRight));

	case EX_RELATIONAL:
		return EvalRelational(exp, REval(exp->mLeft), REval(exp->mRight));

	case EX_PREFIX:
		return EvalUnary(exp, Eval(exp->mLeft));

	case EX_TYPECAST:
		return EvalTypeCast(exp, REval(exp->mLeft), exp->mDecType);

	case EX_CALL:
	{
		ConstexprInterpreter	cinter(exp->mLocation, mErrors, mDataSection);
		return cinter.EvalCall(exp, this);
	}

	case EX_LIST:
		Eval(exp->mLeft);
		return Eval(exp->mRight);

	case EX_CONDITIONAL:
	{
		Value v = REval(exp->mLeft);
		if (v.GetInt())
			return Eval(exp->mRight->mLeft);
		else
			return Eval(exp->mRight->mRight);
	}

	case EX_LOGICAL_AND:
	{
		Value v = REval(exp->mLeft);
		if (!v.GetInt())
			return v;
		else
			return REval(exp->mRight);
	}

	case EX_LOGICAL_OR:
	{
		Value v = REval(exp->mLeft);
		if (v.GetInt())
			return v;
		else
			return REval(exp->mRight);
	}

	case EX_LOGICAL_NOT:
	{
		Value v(exp->mLocation, TheBoolTypeDeclaration);
		Value vr = REval(exp->mLeft);
		if (v.GetInt())
			v.PutInt(0);
		else
			v.PutInt(1);
		return v;
	}

	case EX_INITIALIZATION:
	case EX_ASSIGNMENT:
	{
		Value	lexp = Eval(exp->mLeft);
		Value	rexp = REval(exp->mRight);

		if (exp->mToken != TK_ASSIGN)
			rexp = EvalBinary(exp, lexp.ToRValue(), rexp);
		lexp.Assign(EvalCoerce(exp, rexp, lexp.mDecType));
		return lexp;
	}

	case EX_POSTINCDEC:
	{
		Value vl = Eval(exp->mLeft);
		Value vr = vl.ToRValue();
		vl.Assign(EvalUnary(exp, vr));
		return vr;
	}

	case EX_PREINCDEC:
	{
		Value vl = Eval(exp->mLeft);
		vl.Assign(EvalUnary(exp, vl.ToRValue()));
		return vl;
	}

	case EX_QUALIFY:
	{
		Value	v = Eval(exp->mLeft);
		if (v.mBaseValue)
			return Value(v.mBaseValue, exp->mDecType, v.mOffset + exp->mDecValue->mOffset);
	}

	case EX_INDEX:
	{
		Value	v = Eval(exp->mLeft);
		Value	vi = REval(exp->mRight);

		if (v.mDecType->mType == DT_TYPE_ARRAY)
		{
			if (v.mBaseValue)
				return Value(v.mBaseValue, exp->mDecType, v.mOffset + v.mDecType->mBase->mSize * int(vi.GetInt()));
		}
		else if (v.mDecType->mType == DT_TYPE_POINTER)
		{
			Value	p = v.GetPtr();
			return Value(p.mBaseValue, exp->mDecType, p.mOffset + v.mDecType->mBase->mSize * int(vi.GetInt()));
		}
	}

	case EX_VOID:
		return Value(exp->mLocation);

	}

	mErrors->Error(exp->mLocation, EERR_INVALID_CONSTEXPR, "Invalid constexpr");

	return exp;
}

ConstexprInterpreter::Flow ConstexprInterpreter::Execute(Expression* exp)
{
	for (;;)
	{
		switch (exp->mType)
		{
		case EX_SCOPE:
		{
			int ds = mDestructStack.Size();
			Flow	f = Execute(exp->mLeft);
			UnwindDestructStack(ds);

			return f;
		}

		case EX_RETURN:
			mResult = EvalCoerce(exp, Eval(exp->mLeft), mProcType->mBase);
			return FLOW_RETURN;
		case EX_CONSTANT:
		case EX_VARIABLE:
		case EX_BINARY:
		case EX_RELATIONAL:
		case EX_PREFIX:
		case EX_TYPECAST:
		case EX_CALL:
		case EX_LIST:
		case EX_CONDITIONAL:
		case EX_LOGICAL_AND:
		case EX_LOGICAL_OR:
		case EX_LOGICAL_NOT:
		case EX_INITIALIZATION:
		case EX_ASSIGNMENT:
		case EX_POSTINCDEC:
		case EX_PREINCDEC:
		case EX_QUALIFY:
		case EX_INDEX:
			Eval(exp);
			return FLOW_NEXT;

		case EX_SEQUENCE:
			if (exp->mRight)
			{
				Flow	f = Execute(exp->mLeft);
				if (f == FLOW_NEXT)
					return Execute(exp->mRight);
				return f;
			}
			else
				return Execute(exp->mLeft);

		case EX_BREAK:
			return FLOW_BREAK;

		case EX_CONTINUE:
			return FLOW_CONTINUE;

		case EX_IF:
			if (REval(exp->mLeft).GetInt())
				return Execute(exp->mRight->mLeft);
			else if (exp->mRight->mRight)
				return Execute(exp->mRight->mRight);
			else
				return FLOW_NEXT;

		case EX_SWITCH:
		{
			int64	v = REval(exp->mLeft).GetInt();

			bool	found = false;
			Expression* sexp = exp->mRight;
			while (sexp)
			{
				Expression* cexp = sexp->mLeft;
				if (found || cexp->mType == EX_DEFAULT || v == REval(cexp->mLeft).GetInt())
				{
					found = true;
					if (cexp->mRight)
					{
						Flow	f = Execute(cexp->mRight);
						if (f == FLOW_BREAK)
							return FLOW_NEXT;
						else if (f != FLOW_NEXT)
							return f;
					}
				}
				sexp = sexp->mRight;
			}

			return FLOW_NEXT;
		}

		case EX_WHILE:
		{
			int ds = mDestructStack.Size();
			while (REval(exp->mLeft).GetInt())
			{
				int ls = mDestructStack.Size();
				Flow	f = Execute(exp->mRight);
				UnwindDestructStack(ls);

				if (f == FLOW_RETURN)
					return FLOW_RETURN;
				else if (f == FLOW_BREAK)
					break;
			}
			UnwindDestructStack(ds);
			return FLOW_NEXT;
		}

		case EX_DO:
		{
			int ds = mDestructStack.Size();
			do {
				int ls = mDestructStack.Size();
				Flow	f = Execute(exp->mRight);
				UnwindDestructStack(ls);

				if (f == FLOW_RETURN)
					return FLOW_RETURN;
				else if (f == FLOW_BREAK)
					break;

			} while (REval(exp->mLeft).GetInt());
			UnwindDestructStack(ds);
			return FLOW_NEXT;
		}

		case EX_FOR:
		{
			int ds = mDestructStack.Size();
			if (exp->mLeft->mRight)
				Eval(exp->mLeft->mRight);

			while (!exp->mLeft->mLeft->mLeft || REval(exp->mLeft->mLeft->mLeft).GetInt())
			{
				int ls = mDestructStack.Size();
				Flow	f = Execute(exp->mRight);
				UnwindDestructStack(ls);

				if (f == FLOW_RETURN)
					return FLOW_RETURN;
				else if (f == FLOW_BREAK)
					break;

				if (exp->mLeft->mLeft->mRight)
					Eval(exp->mLeft->mLeft->mRight);
			}
			UnwindDestructStack(ds);

			return FLOW_NEXT;
		}

		case EX_CONSTRUCT:
			if (exp->mLeft->mLeft)
				Eval(exp->mLeft->mLeft);

			if (exp->mLeft->mRight)
				mDestructStack.Push(exp->mLeft->mRight);

			if (exp->mRight)
				return Execute(exp->mRight);
			return FLOW_NEXT;

		case EX_VOID:
			return FLOW_NEXT;

		default:
			mErrors->Error(exp->mLocation, EERR_INVALID_CONSTEXPR, "Invalid constexpr");
		}
	}
}

void ConstexprInterpreter::UnwindDestructStack(int level)
{
	while (mDestructStack.Size() > level)
		Eval(mDestructStack.Pop());
}

ConstexprInterpreter::ValueItem::ValueItem(void)
	: mByte(0), mBaseValue(nullptr)
{
}

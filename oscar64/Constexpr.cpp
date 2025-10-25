#include "Constexpr.h"
#include <math.h>

ConstexprInterpreter::Value::Value(void)
	: mDecType(TheVoidTypeDeclaration), mDecValue(nullptr),
	mBaseValue(nullptr), mOffset(0),
	mData(mShortData), mDataSize(0)
{
}

ConstexprInterpreter::Value::Value(const Location & location)
	: mLocation(location), 
	mDecType(TheVoidTypeDeclaration), mDecValue(nullptr),
	mBaseValue(nullptr), mOffset(0),
	mData(mShortData), mDataSize(0)
{
}

ConstexprInterpreter::Value::Value(Expression* exp)
	: mLocation(exp->mLocation), 
	mDecType(exp->mDecType), mDecValue(nullptr),
	mBaseValue(nullptr), mOffset(0),
	mDataSize(exp->mDecValue->mBase->mSize)
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
	case DT_CONST_ADDRESS:
		PutIntAt(dec->mInteger, offset, dec->mBase);
		break;
	case DT_CONST_FLOAT:
		PutFloatAt(float(dec->mNumber), offset, dec->mBase);
		break;
	case DT_CONST_STRUCT:
		for (Declaration* pdec = dec->mParams; pdec; pdec = pdec->mNext)
			PutConst(pdec->mOffset, pdec);
		break;
	case DT_CONST_DATA:
		for (int i = 0; i < dec->mBase->mSize; i++)
			PutIntAt(dec->mData[i], offset + i, TheConstCharTypeDeclaration);
		break;
	case DT_CONST_POINTER:
		PutPtrAt(new Value(mLocation, dec->mValue->mDecValue, dec->mBase, 0), offset, dec);
		break;
	}
}

ConstexprInterpreter::Value::Value(const Location& location, Declaration* dec)
	: mLocation(location),
	mDecType(dec), mDecValue(nullptr),
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
	mDecType(dec), mDecValue(nullptr),
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
	mDecType(value.mDecType), mDecValue(nullptr),
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
	mDecType(value.mDecType), mDecValue(nullptr),
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
	mDecType(value->mDecType), mDecValue(value->mDecValue),
	mBaseValue(value), mOffset(0),
	mDataSize(0), mData(mShortData)
{
}

ConstexprInterpreter::Value::Value(const Location& location, const Value* value, Declaration* type, int offset)
	: mLocation(location),
	mDecType(type), mDecValue(nullptr),
	mBaseValue(value), mOffset(offset),
	mDataSize(0), mData(mShortData)
{
}

ConstexprInterpreter::Value::Value(const Location& location, Declaration* value, Declaration* type, int offset)
	: mLocation(location),
	mDecType(type), mDecValue(value),
	mBaseValue(nullptr), mOffset(offset),
	mDataSize(0), mData(mShortData)
{
}

ConstexprInterpreter::Value::Value(const Location& location, const uint8* data, Declaration* type)
	: mLocation(location),
	mDecType(type), mDecValue(nullptr),
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
	mDecType(type), mDecValue(nullptr),
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

int64 ConstexprInterpreter::Value::GetInt(Declaration* type) const
{
	int64	val = GetInt();
	val &= 0xffffffffll >> (8 * (4 - type->mSize));
	if (type->mFlags & DTF_SIGNED)
	{
		if (val & (0x80000000ll >> (8 * (4 - type->mSize))))
			val -= 0x100000000ll >> (8 * (4 - type->mSize));
	}
	return val;
}

double ConstexprInterpreter::Value::GetFloat(void) const
{
	return GetFloatAt(0, mDecType);
}

ConstexprInterpreter::Value ConstexprInterpreter::Value::GetPtr(void) const
{
	return GetPtrAt(0, mDecType->mBase);
}

bool ConstexprInterpreter::Value::GetBool(void) const
{
	bool	check = false;
	if (mDecType->IsIntegerType())
		return GetInt() != 0;
	else if (mDecType->IsPointerType())
	{
		Value	p = GetPtr();
		return p.mBaseValue != nullptr || p.mOffset != 0;
	}
	else if (mDecType->mType == DT_TYPE_FLOAT)
		return GetFloat() != 0;

	return false;
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

	return Value(mLocation, dp->mBaseValue, type, uint16(dp[0].mByte | ((uint32)(dp[1].mByte) << 8)));
}

void ConstexprInterpreter::Value::PutVarAt(Declaration* var, int64 v, int at, Declaration* type)
{
	ValueItem* dp = GetAddr() + at;
	dp[0].mByte = uint8(v & 0xff);
	dp[1].mByte = uint8((v >> 8) & 0xff);
	mDecValue = var;
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
	{
		if (mDecType->mType == DT_TYPE_ARRAY)
		{
			Value	v(mBaseValue->mLocation, mDecType->BuildArrayPointer());
			v.PutPtr(*this);
			return v;
		}
		else
			return Value(mLocation, GetAddr(), mDecType);
	}
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
	{
		const ConstexprInterpreter::ValueItem* sp = v.GetAddr();
		ConstexprInterpreter::ValueItem* dp = GetAddr();

		for (int i = 0; i < mDecType->mSize; i++)
			dp[i] = sp[i];
	}	break;
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
	case DT_TYPE_ENUM:
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

		while (type)
		{
			for (Declaration* mdec = type->mParams; mdec; mdec = mdec->mNext)
			{
				Declaration* cdec = GetConst(offset + mdec->mOffset, mdec->mBase, dataSection);
				cdec->mOffset = mdec->mOffset;
				cdec->mNext = dec->mParams;
				dec->mParams = cdec;
			}

			if (type->mVTable)
			{
				Declaration * cdec = new Declaration(mLocation, DT_CONST_INTEGER);
				cdec->mBase = TheConstCharTypeDeclaration;
				cdec->mSize = 1;
				cdec->mInteger = type->mVTable->mDefaultConstructor->mInteger;
				cdec->mOffset = offset + type->mVTable->mOffset;
				cdec->mNext = dec->mParams;
				dec->mParams = cdec;
			}

			if (type->mBase)
				type = type->mBase->mBase;
			else
				type = nullptr;
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
			if (type->mStride)
				cdec->mOffset = i / type->mBase->mSize;
			else
				cdec->mOffset = i;

			if (ldec)
				ldec->mNext = cdec;
			else
				dec->mParams = cdec;

			ldec = cdec;
		}
		break;
	}
	case DT_TYPE_POINTER:
	{
		Value	vp = GetPtrAt(offset, type);
		if (vp.mBaseValue)
		{
			dec = new Declaration(mLocation, DT_CONST_POINTER);
			dec->mBase = type;
			dec->mSize = type->mSize;

			Declaration* target;

			if (vp.mBaseValue->mDecValue)
			{
				target = new Declaration(mLocation, DT_VARIABLE_REF);

				if (vp.mBaseValue->mDecValue->mType == DT_VARIABLE_REF)
				{
					target->mBase = vp.mBaseValue->mDecValue->mBase;
					target->mOffset = vp.mBaseValue->mDecValue->mOffset;
				}
				else
					target->mBase = vp.mBaseValue->mDecValue;

				target->mOffset += vp.mOffset;

				dec->mValue = new Expression(mLocation, EX_VARIABLE);
				dec->mValue->mDecType = type;
				dec->mValue->mDecValue = target;
				return dec;
			}
			else if (vp.mBaseValue->mDecType->mType == DT_TYPE_ARRAY)
			{
				target = new Declaration(mLocation, DT_CONST_DATA);
				target->mSize = vp.mBaseValue->mDataSize;
				target->mBase = vp.mBaseValue->mDecType;
				target->mSection = dataSection;

				uint8* buffer = new uint8[target->mSize];
				for (int i = 0; i < target->mSize; i++)
					buffer[i] = uint8(vp.mBaseValue->GetIntAt(i, TheUnsignedCharTypeDeclaration));
				target->mData = buffer;

				dec->mOffset = vp.mOffset;
#if 0
				if (vp.mOffset)
				{
					Declaration* tt = new Declaration(mLocation, DT_CONST_POINTER);
					tt->mBase = target->mBase;
					tt->mValue = new Expression(mLocation, EX_CONSTANT);
					tt->mValue->mDecType = target->mBase;
					tt->mValue->mDecValue = target;
					tt->mOffset = vp.mOffset;
					target = tt;
				}
#endif
			}
			else
				target = vp.mBaseValue->GetConst(0, vp.mBaseValue->mDecType, dataSection);

			dec->mValue = new Expression(mLocation, EX_CONSTANT);
			dec->mValue->mDecType = target->mBase;
			dec->mValue->mDecValue = target;
		}
		else if (vp.mDecValue)
		{
			dec = new Declaration(mLocation, DT_VARIABLE_REF);
			dec->mBase = vp.mDecValue;
			dec->mFlags = 0;
			dec->mSize = type->mSize;
			dec->mOffset = vp.mOffset;
		}
		else
		{
			dec = new Declaration(mLocation, DT_CONST_ADDRESS);
			dec->mBase = type;
			dec->mFlags = 0;
			dec->mSize = type->mSize;
			dec->mInteger = vp.mOffset;
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
	for (int i = 0; i < mTemps.Size(); i++)
		delete mTemps[i];
}

ConstexprInterpreter::Value* ConstexprInterpreter::NewValue(Expression* exp, Declaration* type, int size)
{
	Value* v = new Value(exp->mLocation, type, size);
	mHeap->Push(v);
	return v;
}

void ConstexprInterpreter::DeleteValue(const Value* v)
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

Expression* ConstexprInterpreter::EvalConstructor(Expression* exp)
{
	mProcType = exp->mLeft->mDecType;

	Expression* pex = exp->mRight;
	Declaration* cdec = exp->mLeft->mDecType->mParams;

	int	pos = 0;
	mResult = Value(exp->mLocation, cdec->mBase->mBase);
	mParams[pos] = Value(exp->mLocation, cdec->mBase);
	mParams[pos].PutPtr(Value(&mResult));
	pos = 2;

	if (pex->mType == EX_LIST)
		pex = pex->mRight;
	else
		pex = nullptr;
	cdec = cdec->mNext;

	while (pex && pex->mType == EX_LIST)
	{
		if (!AddParam(pos, pex->mLeft, cdec))
			return exp;

		pex = pex->mRight;
		if (cdec)
			cdec = cdec->mNext;
	}

	if (pex)
	{
		if (!AddParam(pos, pex, cdec))
			return exp;
	}

	mHeap = new ExpandingArray<const Value*>();

	Execute(exp->mLeft->mDecValue->mValue);

	if (mHeap->Size() > 0)
		mErrors->Error(exp->mLocation, EERR_UNBALANCED_HEAP_USE, "Unbalanced heap use in constexpr");
	delete mHeap;

	return mResult.ToExpression(mDataSection);
}

Expression* ConstexprInterpreter::EvalTempConstructor(Expression* exp)
{
	Expression* cexp = exp->mLeft->mLeft;

	mProcType = cexp->mLeft->mDecType;

	Expression* pex = cexp->mRight;
	Declaration* dec = cexp->mLeft->mDecType->mParams;

	Value othis = Value(exp->mLocation, exp->mRight->mDecType);

	mParams[0] = Value(exp->mLocation, dec->mBase);
	mParams[0].PutPtr(Value(&othis));

	int pos = 2;
	if (pex->mType == EX_LIST)
		pex = pex->mRight;
	else
		pex = nullptr;
	dec = dec->mNext;

	while (pex && pex->mType == EX_LIST)
	{
		if (!AddParam(pos, pex->mLeft, dec))
			return exp;

		pex = pex->mRight;
		if (dec)
			dec = dec->mNext;
	}

	if (pex)
	{
		if (!AddParam(pos, pex, dec))
			return exp;
	}

	mHeap = new ExpandingArray<const Value*>();

	Execute(cexp->mLeft->mDecValue->mValue);

	if (mHeap->Size() > 0)
		mErrors->Error(exp->mLocation, EERR_UNBALANCED_HEAP_USE, "Unbalanced heap use in constexpr");
	delete mHeap;

	return othis.ToExpression(mDataSection);
}

bool ConstexprInterpreter::AddParam(int& pos, Expression* pex, Declaration* dec)
{
	if (dec)
		pos = dec->mVarIndex;

	if (pex->mType == EX_CONSTANT)
	{
		if (pex->mDecType->mType == DT_TYPE_ARRAY)
		{
			Value* tmp = new Value(pex);
			mTemps.Push(tmp);
			mParams[pos] = Value(pex->mLocation, pex->mDecType->BuildArrayPointer());
			mParams[pos].PutPtr(Value(tmp));
		}
		else
			mParams[pos] = Value(pex);
	}
	else if (pex->mType == EX_VARIABLE && (pex->mDecValue->mFlags & DTF_CONST))
	{
		mParams[pos] = Value(pex->mLocation, pex->mDecValue->mBase);
		if (pex->mDecValue->mSize > 0)
		{
			if (pex->mDecValue->mValue)
				mParams[pos].PutConst(0, pex->mDecValue->mValue->mDecValue);
			else
				return false;
		}
	}
	else
		return false;

	return true;
}

Expression* ConstexprInterpreter::EvalCall(Expression* exp)
{
	if (!exp->mLeft->mDecValue || !exp->mLeft->mDecValue->mValue)
		return exp;

	mProcType = exp->mLeft->mDecType;

	Expression* pex = exp->mRight;
	Declaration* dec = exp->mLeft->mDecType->mParams;

	int	pos = 0;
	if (mProcType->mBase && mProcType->mBase->mType == DT_TYPE_STRUCT)
	{
		mResult = Value(exp->mLocation, mProcType->mBase);
		mParams[0] = Value(&mResult);
		pos = 2;
	}

	while (pex && pex->mType == EX_LIST)
	{
		if (!AddParam(pos, pex->mLeft, dec))
			return exp;

		pex = pex->mRight;
		if (dec)
			dec = dec->mNext;
	}

	if (pex)
	{
		if (!AddParam(pos, pex, dec))
			return exp;
	}

	mHeap = new ExpandingArray<const Value*>();

	Execute(exp->mLeft->mDecValue->mValue);

	if (mHeap->Size() > 0)
		mErrors->Error(exp->mLocation, EERR_UNBALANCED_HEAP_USE, "Unbalanced heap use in constexpr");
	delete mHeap;

	return mResult.ToExpression(mDataSection);
}

static Declaration* CombinedIntType(Declaration* ld, Declaration* rd)
{
	if (ld->mSize < 2 && rd->mSize < 2)
		return TheSignedIntTypeDeclaration;
	else if (ld->mSize > rd->mSize)
		return ld;
	else if (rd->mSize > ld->mSize)
		return rd;
	else if (!(rd->mFlags & DTF_SIGNED))
		return rd;
	else
		return ld;
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
	else if (exp->mDecType->mType == DT_TYPE_POINTER)
	{
		if (exp->mLeft->mDecType->IsPointerType())
		{
			Value	vlp = vl.GetPtr();
			vlp.mOffset += int(vr.GetInt() * vl.mDecType->mBase->mSize);
			v.PutPtr(vlp);
		}
		else if (exp->mRight->mDecType->IsPointerType())
		{
			Value	vrp = vr.GetPtr();
			vrp.mOffset += int(vl.GetInt() * vr.mDecType->mBase->mSize);
			v.PutPtr(vrp);
		}
		else
			mErrors->Error(exp->mLocation, EERR_INCOMPATIBLE_OPERATOR, "Incompatible operator", TokenNames[exp->mToken]);
	}
	else
	{
		Declaration* ctype = CombinedIntType(vl.mDecType, vr.mDecType);
		switch (exp->mToken)
		{
		case TK_ADD:
		case TK_ASSIGN_ADD:
			v.PutInt(vl.GetInt(ctype) + vr.GetInt(ctype));
			break;
		case TK_SUB:
		case TK_ASSIGN_SUB:
			v.PutInt(vl.GetInt(ctype) - vr.GetInt(ctype));
			break;
		case TK_MUL:
		case TK_ASSIGN_MUL:
			v.PutInt(vl.GetInt(ctype) * vr.GetInt(ctype));
			break;
		case TK_DIV:
		case TK_ASSIGN_DIV:
			if (vr.GetInt(ctype) == 0)
				mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Constant division by zero");
			else
				v.PutInt(vl.GetInt(ctype) / vr.GetInt(ctype));
			break;
		case TK_MOD:
		case TK_ASSIGN_MOD:
			if (vr.GetInt(ctype) == 0)
				mErrors->Error(exp->mLocation, EERR_INVALID_VALUE, "Constant division by zero");
			else
				v.PutInt(vl.GetInt(ctype) % vr.GetInt(ctype));
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
			v.PutInt(vl.GetInt(ctype) & vr.GetInt(ctype));
			break;
		case TK_BINARY_OR:
		case TK_ASSIGN_OR:
			v.PutInt(vl.GetInt(ctype) | vr.GetInt(ctype));
			break;
		case TK_BINARY_XOR:
		case TK_ASSIGN_XOR:
			v.PutInt(vl.GetInt(ctype) ^ vr.GetInt(ctype));
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
		Declaration* ctype = CombinedIntType(vl.mDecType, vr.mDecType);
		switch (exp->mToken)
		{
		case TK_EQUAL:
			check = vl.GetInt(ctype) == vr.GetInt(ctype);
			break;
		case TK_NOT_EQUAL:
			check = vl.GetInt(ctype) != vr.GetInt(ctype);
			break;
		case TK_GREATER_THAN:
			check = vl.GetInt(ctype) > vr.GetInt(ctype);
			break;
		case TK_GREATER_EQUAL:
			check = vl.GetInt(ctype) >= vr.GetInt(ctype);
			break;
		case TK_LESS_THAN:
			check = vl.GetInt(ctype) < vr.GetInt(ctype);
			break;
		case TK_LESS_EQUAL:
			check = vl.GetInt(ctype) <= vr.GetInt(ctype);
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
		case TK_SIZEOF:
			v.PutInt(vl.mDecType->mSize);
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
	if (mProcType->mBase && mProcType->mBase->mType == DT_TYPE_STRUCT)
	{
		mResult = Value(exp->mLocation, mProcType->mBase);
		mParams[0] = Value(&mResult);
		pos = 2;
	}

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
		else if (!strcmp(iname->mString, "tan"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(tan(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "log"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(log(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "exp"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(::exp(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "sqrt"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(::sqrt(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "atan"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(::atan(mParams[0].GetFloat()));
		}
		else if (!strcmp(iname->mString, "atan2"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(::atan2(mParams[0].GetFloat(), mParams[1].GetFloat()));
		}
		else if (!strcmp(iname->mString, "pow"))
		{
			mResult = Value(exp->mLocation, TheFloatTypeDeclaration);
			mResult.PutFloat(::pow(mParams[0].GetFloat(), mParams[1].GetFloat()));
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
	if (type->IsReference() || vl.mDecType->mType == DT_TYPE_ARRAY && type->mType == DT_TYPE_ARRAY)
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
		else if (v.mDecType->IsIntegerType() && type->IsIntegerType() && v.mDecType != type)
		{
			int64	val = vl.GetInt();
			val &= 0xffffffffll >> (8 * (4 - type->mSize));
			if (type->mFlags & DTF_SIGNED)
			{
				if (val & (0x80000000ll >> (8 * (4 - type->mSize))))
					val -= 0x100000000ll >> (8 * (4 - type->mSize));
			}

			Value	vf(exp->mLocation, type);
			vf.PutInt(val);
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
		if (exp->mDecType->mSize < 4)
			return Value(exp);
		else
		{
			Value* tmp = new Value(exp);
			mTemps.Push(tmp);
			return Value(tmp);
		}
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
	case EX_COMMA:
		Eval(exp->mLeft);
		return Eval(exp->mRight);

	case EX_CONDITIONAL:
	{
		Value v = REval(exp->mLeft);
		if (v.GetBool())
			return Eval(exp->mRight->mLeft);
		else
			return Eval(exp->mRight->mRight);
	}

	case EX_LOGICAL_AND:
	{
		Value v(exp->mLocation, TheBoolTypeDeclaration);
		Value vl = REval(exp->mLeft);
		if (!vl.GetBool())
		{
			v.PutInt(0);
			return v;
		}
		Value vr = REval(exp->mRight);
		if (!vr.GetBool())
		{
			v.PutInt(0);
			return v;
		}
		v.PutInt(1);
		return v;
	}

	case EX_LOGICAL_OR:
	{
		Value v(exp->mLocation, TheBoolTypeDeclaration);
		Value vl = REval(exp->mLeft);
		if (vl.GetBool())
		{
			v.PutInt(1);
			return v;
		}
		Value vr = REval(exp->mRight);
		if (vr.GetBool())
		{
			v.PutInt(1);
			return v;
		}
		v.PutInt(0);
		return v;
	}

	case EX_LOGICAL_NOT:
	{
		Value v(exp->mLocation, TheBoolTypeDeclaration);
		Value vr = REval(exp->mLeft);

		if (vr.GetBool())
			v.PutInt(0);
		else
			v.PutInt(1);
		return v;
	}

	case EX_INITIALIZATION:
	{
		Value	lexp = Eval(exp->mLeft);
		Value	rexp = Eval(exp->mRight);
		lexp.Assign(EvalCoerce(exp, rexp, lexp.mDecType));
		return lexp;
	}

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
			return Value(exp->mLocation, v.mBaseValue, exp->mDecType, v.mOffset + exp->mDecValue->mOffset);
		break;
	}

	case EX_INDEX:
	{
		Value	v = Eval(exp->mLeft);
		Value	vi = REval(exp->mRight);

		if (v.mDecType->mType == DT_TYPE_ARRAY)
		{
			if (v.mBaseValue)
				return Value(exp->mLocation, v.mBaseValue, exp->mDecType, v.mOffset + v.mDecType->mBase->mSize * int(vi.GetInt()));
		}
		else if (v.mDecType->mType == DT_TYPE_POINTER)
		{
			Value	p = v.GetPtr();
			return Value(exp->mLocation, p.mBaseValue, exp->mDecType, p.mOffset + v.mDecType->mBase->mSize * int(vi.GetInt()));
		}
		break;
	}

	case EX_RESULT:
		if (mParams[0].mBaseValue)
			return mParams[0];
		else
			return Value(&mParams[0]);

	case EX_CONSTRUCT:
		if (exp->mLeft->mLeft)
			Eval(exp->mLeft->mLeft);
		return Eval(exp->mRight);

	case EX_VOID:
		return Value(exp->mLocation);

	}

	mErrors->Error(exp->mLocation, EERR_INVALID_CONSTEXPR, "Invalid constexpr");

	return Value();
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
		case EX_COMMA:
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
		case EX_RESULT:
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

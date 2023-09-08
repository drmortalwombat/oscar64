#pragma once

#include "Declaration.h"

class ConstexprInterpreter
{
public:
	ConstexprInterpreter(const Location & loc, Errors * err, LinkerSection * dataSection);
	~ConstexprInterpreter(void);

	Expression* EvalCall(Expression* exp);
protected:
	struct Value;

	struct ValueItem
	{
		uint8		mByte;
		Value* mBaseValue;

		ValueItem(void);
	};

	struct Value
	{
		~Value(void);
		Value(const Location& location);
		Value(Expression * exp);
		Value(const Location& location, Declaration * dec);
		Value(const Value& value);
		Value(Value&& value);
		Value(Value * value, Declaration * type, int offset);
		Value(Value* value);
		Value(const Location& location, const uint8 * data, Declaration* type);
		Value(const Location& location, const ValueItem* data, Declaration* type);
		Value(void);

		Value& operator=(const Value& v);
		Value& operator=(Value&& v);

		Value ToRValue(void) const;
		Expression* ToExpression(LinkerSection* dataSection) const;
		void Assign(const Value& v);

		Location		mLocation;
		Declaration	*	mDecType;
		Value		*	mBaseValue;
		int				mOffset;
		ValueItem	*	mData;
		int				mDataSize;
		ValueItem		mShortData[4];

		ValueItem* GetAddr(void);
		const ValueItem* GetAddr(void) const;

		int64 GetInt(void) const;
		double GetFloat(void) const;
		Value GetPtr(void) const;
		void PutInt(int64 v);
		void PutFloat(double v);
		void PutPtr(const Value& v);

		int64 GetIntAt(int at, Declaration* type) const;
		double GetFloatAt(int at, Declaration* type) const;
		Value GetPtrAt(int at, Declaration* type) const;
		void PutIntAt(int64 v, int at, Declaration* type);
		void PutFloatAt(double v, int at, Declaration* type);
		void PutPtrAt(const Value& v, int at, Declaration* type);

		void PutConst(int offset, Declaration * dec);
		Declaration* GetConst(int offset, Declaration* type, LinkerSection* dataSection) const;
	};

	Value EvalCall(Expression* exp, ConstexprInterpreter* caller);
	Value EvalBinary(Expression* exp, const Value& vl, const Value& vr);
	Value EvalUnary(Expression* exp, const Value& vl);
	Value EvalRelational(Expression* exp, const Value& vl, const Value& vr);
	Value EvalTypeCast(Expression* exp, const Value& vl, Declaration* type);
	Value EvalCoerce(Expression* exp, const Value& vl, Declaration* type);

	Value REval(Expression* exp);
	Value Eval(Expression* exp);

	Declaration* mProcType;
	Location		mLocation;
	LinkerSection* mDataSection;
	GrowingArray<Value>	mParams, mLocals;
	Errors	* mErrors;
	Value	  mResult;
};
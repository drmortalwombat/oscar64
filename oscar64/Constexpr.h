#pragma once

#include "Declaration.h"

class ConstexprInterpreter
{
public:
	ConstexprInterpreter(const Location & loc, Errors * err, LinkerSection * dataSection);
	~ConstexprInterpreter(void);

	Expression* EvalConstructor(Expression* exp);
	Expression* EvalCall(Expression* exp);
	Expression* EvalTempConstructor(Expression* exp);
protected:

	bool AddParam(int& pos, Expression* pex, Declaration* dec);

	struct Value;

	struct ValueItem
	{
		uint8			mByte;
		const Value	*	mBaseValue;

		ValueItem(void);
	};

	struct Value
	{
		~Value(void);
		Value(const Location& location);
		Value(Expression * exp);
		Value(const Location& location, Declaration * dec);
		Value(const Location& location, Declaration* dec, int count);
		Value(const Value& value);
		Value(Value&& value);
		Value(const Location& location, const Value * value, Declaration * type, int offset);
		Value(const Location& location, const Value* value, Declaration* type, int offset, int shift, int bits);
		Value(const Location& location, Declaration * value, Declaration* type, int offset);
		Value(Value* value);
		Value(const Location& location, const uint8 * data, Declaration* type);
		Value(const Location& location, const ValueItem* data, Declaration* type);
		Value(void);

		Value& operator=(const Value& v);
		Value& operator=(Value&& v);

		Value ToRValue(Errors* err) const;
		Expression* ToExpression(LinkerSection* dataSection) const;
		void Assign(const Value& v, Errors* err);

		Location		mLocation;
		Declaration	*	mDecType, * mDecValue;
		const Value	*	mBaseValue;
		int				mOffset, mShift, mBits;
		ValueItem	*	mData;
		int				mDataSize;
		ValueItem		mShortData[4];

		bool CheckSpace(int at, int size) const;

		ValueItem* GetAddr(void);
		const ValueItem* GetAddr(void) const;

		int64 GetInt(void) const;
		int64 GetInt(Declaration * type) const;
		double GetFloat(void) const;
		Value GetPtr(void) const;
		bool GetBool(void) const;

		void PutInt(int64 v);
		void PutFloat(double v);
		void PutPtr(const Value& v);

		int64 GetIntAt(int at, Declaration* type) const;
		double GetFloatAt(int at, Declaration* type) const;
		Value GetPtrAt(int at, Declaration* type) const;
		void PutIntAt(int64 v, int at, Declaration* type);
		void PutFloatAt(double v, int at, Declaration* type);
		void PutPtrAt(const Value& v, int at, Declaration* type);
		void PutVarAt(Declaration* var, int64 v, int at, Declaration* type);

		void PutConst(int offset, Declaration * dec);
		Declaration* GetConst(int offset, Declaration* type, LinkerSection* dataSection) const;

	};

	Value * NewValue(Expression* exp, Declaration* type, int size);
	void DeleteValue(const Value* v);

	Value EvalCall(Expression* exp, ConstexprInterpreter* caller);
	Value EvalBinary(Expression* exp, const Value& vl, const Value& vr);
	Value EvalUnary(Expression* exp, const Value& vl);
	Value EvalRelational(Expression* exp, const Value& vl, const Value& vr);
	Value EvalTypeCast(Expression* exp, const Value& vl, Declaration* type);
	Value EvalCoerce(Expression* exp, const Value& vl, Declaration* type);

	Value REval(Expression* exp);
	Value Eval(Expression* exp);

	enum Flow
	{
		FLOW_NEXT,
		FLOW_CONTINUE,
		FLOW_BREAK,
		FLOW_RETURN
	};

	Flow Execute(Expression* exp);
	void UnwindDestructStack(int level);

	Declaration* mProcType;
	Location		mLocation;
	LinkerSection* mDataSection;
	GrowingArray<Value>	mParams, mLocals;
	ExpandingArray<Expression*>	mDestructStack;
	ExpandingArray<const Value *>	*	mHeap;
	ExpandingArray<Value*>		mTemps;

	Errors	* mErrors;
	Value	  mResult;

};
#pragma once

enum TType
{
	TT_VOID,
	TT_NULL,
	TT_BOOL,
	TT_INTEGER,
	TT_FLOAT,
	TT_POINTER,
	TT_ARRAY,
	TT_STRUCT,
	TT_UNION,
	TT_FUNCTION
};

class Type
{
public:
	TType		mType;
	Type* mBase;
	Scope* mScope;
};


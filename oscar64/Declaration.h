#pragma once

#include "Ident.h"
#include "Scanner.h"
#include "MachineTypes.h"
#include "Assembler.h"

class LinkerObject;
class LinkerSection;

enum DecType
{
	DT_TYPE_VOID,
	DT_TYPE_NULL,
	DT_TYPE_BOOL,
	DT_TYPE_INTEGER,
	DT_TYPE_FLOAT,
	DT_TYPE_ENUM,
	DT_TYPE_POINTER,
	DT_TYPE_ARRAY,
	DT_TYPE_STRUCT,
	DT_TYPE_UNION,
	DT_TYPE_FUNCTION,
	DT_TYPE_ASSEMBLER,

	DT_TYPE_CONST,
	DT_TYPE_VOLATILE,

	DT_CONST_INTEGER,
	DT_CONST_FLOAT,
	DT_CONST_FUNCTION,
	DT_CONST_ADDRESS,
	DT_CONST_DATA,
	DT_CONST_STRUCT,
	DT_CONST_POINTER,
	DT_CONST_ASSEMBLER,

	DT_VARIABLE,
	DT_ARGUMENT,
	DT_ELEMENT,
	DT_ANON,
	DT_LABEL,
	DT_VARIABLE_REF,
	DT_FUNCTION_REF,
	DT_LABEL_REF
};

// TypeFlags

static const uint32	DTF_SIGNED			= 0x00000001;
static const uint32	DTF_DEFINED			= 0x00000002;
static const uint32	DTF_GLOBAL			= 0x00000004;
static const uint32	DTF_VARIADIC		= 0x00000008;
static const uint32	DTF_INTRINSIC		= 0x00000010;
static const uint32	DTF_STATIC			= 0x00000020;
static const uint32	DTF_CONST			= 0x00000040;
static const uint32	DTF_VOLATILE		= 0x00000080;
static const uint32	DTF_EXTERN			= 0x00000100;
static const uint32 DTF_NATIVE			= 0x00000200;
static const uint32 DTF_UPPER_BYTE		= 0x00000400;
static const uint32 DTF_LOWER_BYTE		= 0x00000800;
static const uint32 DTF_SECTION_START	= 0x00001000;
static const uint32 DTF_SECTION_END		= 0x00002000;

class Declaration;

class DeclarationScope
{
public:
	DeclarationScope(DeclarationScope * parent);
	~DeclarationScope(void);

	Declaration* Insert(const Ident* ident, Declaration* dec);
	Declaration* Lookup(const Ident* ident);

	DeclarationScope* mParent;
protected:
	struct Entry
	{
		const Ident* mIdent;
		Declaration* mDec;
	};
	Entry	* mHash;
	int			mHashSize, mHashFill;
};

enum ExpressionType
{
	EX_ERROR,
	EX_VOID,
	EX_CONSTANT,
	EX_VARIABLE,
	EX_ASSIGNMENT,
	EX_BINARY,
	EX_RELATIONAL,
	EX_PREINCDEC,
	EX_PREFIX,
	EX_POSTFIX,
	EX_POSTINCDEC,
	EX_INDEX,
	EX_QUALIFY,
	EX_CALL,
	EX_LIST,
	EX_RETURN,
	EX_SEQUENCE,
	EX_WHILE,
	EX_IF,
	EX_ELSE,
	EX_FOR,
	EX_DO,
	EX_BREAK,
	EX_CONTINUE,
	EX_TYPE,
	EX_TYPECAST,
	EX_LOGICAL_AND,
	EX_LOGICAL_OR,
	EX_LOGICAL_NOT,
	EX_ASSEMBLER,
	EX_UNDEFINED,
	EX_SWITCH,
	EX_CASE,
	EX_DEFAULT,
	EX_CONDITIONAL
};

class Expression
{
public:
	Expression(const Location& loc, ExpressionType type);
	~Expression(void);

	Location				mLocation;
	ExpressionType			mType;
	Expression			*	mLeft, * mRight;
	Token					mToken;
	Declaration			*	mDecValue, * mDecType;
	AsmInsType				mAsmInsType;
	AsmInsMode				mAsmInsMode;
	bool					mConst;

	Expression* LogicInvertExpression(void);
	Expression* ConstantFold(void);
};

class Declaration
{
public:
	Declaration(const Location & loc, DecType type);
	~Declaration(void);

	Location			mLocation;
	DecType				mType;
	Token				mToken;
	Declaration*		mBase, *mParams, * mNext;
	Expression*			mValue;
	DeclarationScope*	mScope;
	int					mOffset, mSize, mVarIndex;
	int64				mInteger;
	double				mNumber;
	uint32				mFlags;
	const Ident		*	mIdent;
	LinkerSection	*	mSection;
	const uint8		*	mData;
	LinkerObject	*	mLinkerObject;

	bool CanAssign(const Declaration* fromType) const;
	bool IsSame(const Declaration* dec) const;
	bool IsSubType(const Declaration* dec) const;

	bool IsIntegerType(void) const;
	bool IsNumericType(void) const;
};

void InitDeclarations(void);

extern Declaration* TheVoidTypeDeclaration, * TheSignedIntTypeDeclaration, * TheUnsignedIntTypeDeclaration, * TheConstCharTypeDeclaration, * TheCharTypeDeclaration, * TheSignedCharTypeDeclaration, * TheUnsignedCharTypeDeclaration, * TheBoolTypeDeclaration, * TheFloatTypeDeclaration, * TheVoidPointerTypeDeclaration;

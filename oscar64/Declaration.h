#pragma once

#include "Ident.h"
#include "Scanner.h"
#include "MachineTypes.h"
#include "Assembler.h"
#include "Array.h"

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
	DT_TYPE_AUTO,

	DT_TYPE_CONST,
	DT_TYPE_VOLATILE,

	DT_CONST_INTEGER,
	DT_CONST_FLOAT,
	DT_CONST_FUNCTION,
	DT_CONST_ADDRESS,
	DT_CONST_DATA,
	DT_CONST_STRUCT,
	DT_CONST_POINTER,
	DT_CONST_REFERENCE,
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

static const uint64	DTF_SIGNED			= (1ULL << 0);
static const uint64	DTF_DEFINED			= (1ULL << 1);
static const uint64	DTF_GLOBAL			= (1ULL << 2);
static const uint64	DTF_VARIADIC		= (1ULL << 3);
static const uint64	DTF_INTRINSIC		= (1ULL << 4);
static const uint64	DTF_STATIC			= (1ULL << 5);
static const uint64	DTF_CONST			= (1ULL << 6);
static const uint64	DTF_VOLATILE		= (1ULL << 7);
static const uint64	DTF_EXTERN			= (1ULL << 8);
static const uint64 DTF_NATIVE			= (1ULL << 9);
static const uint64 DTF_UPPER_BYTE		= (1ULL << 10);
static const uint64 DTF_LOWER_BYTE		= (1ULL << 11);
static const uint64 DTF_SECTION_START	= (1ULL << 12);
static const uint64 DTF_SECTION_END		= (1ULL << 13);
static const uint64 DTF_FASTCALL		= (1ULL << 14);
static const uint64 DTF_INLINE			= (1ULL << 15);
static const uint64	DTF_ANALYZED	    = (1ULL << 16);
static const uint64 DTF_REQUEST_INLINE  = (1ULL << 17);
static const uint64 DTF_INTERRUPT		= (1ULL << 18);
static const uint64 DTF_EXPORT			= (1ULL << 19);
static const uint64 DTF_HWINTERRUPT		= (1ULL << 20);
static const uint64 DTF_STACKCALL		= (1ULL << 21);
static const uint64 DTF_ZEROPAGE		= (1ULL << 22);
static const uint64 DTF_PREVENT_INLINE  = (1ULL << 23);
static const uint64 DTF_STRIPED			= (1ULL << 24);

static const uint64 DTF_FUNC_VARIABLE	= (1ULL << 32);
static const uint64 DTF_FUNC_ASSEMBLER	= (1ULL << 33);
static const uint64 DTF_FUNC_RECURSIVE  = (1ULL << 34);
static const uint64 DTF_FUNC_ANALYZING  = (1ULL << 35);
static const uint64 DTF_FUNC_CONSTEXPR	= (1ULL << 36);
static const uint64 DTF_FUNC_INTRSAVE   = (1ULL << 37);
static const uint64 DTF_FUNC_INTRCALLED = (1ULL << 38);

static const uint64 DTF_VAR_ALIASING	= (1ULL << 39);


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
	EX_INITIALIZATION,
	EX_BINARY,
	EX_RELATIONAL,
	EX_PREINCDEC,
	EX_PREFIX,
	EX_POSTFIX,
	EX_POSTINCDEC,
	EX_INDEX,
	EX_QUALIFY,
	EX_CALL,
	EX_INLINE,
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
	EX_CONDITIONAL,
	EX_ASSUME
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
	Expression* ConstantFold(Errors * errors);
	bool HasSideEffects(void) const;

	bool IsSame(const Expression* exp) const;
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
	int					mOffset, mSize, mVarIndex, mNumVars, mComplexity, mLocalSize, mAlignment, mFastCallBase, mFastCallSize, mStride, mStripe;
	int64				mInteger, mMinValue, mMaxValue;
	double				mNumber;
	uint64				mFlags;
	const Ident		*	mIdent;
	LinkerSection	*	mSection;
	const uint8		*	mData;
	LinkerObject	*	mLinkerObject;

	GrowingArray<Declaration*>	mCallers, mCalled;

	bool CanAssign(const Declaration* fromType) const;
	bool IsSame(const Declaration* dec) const;
	bool IsSubType(const Declaration* dec) const;
	bool IsConstSame(const Declaration* dec) const;

	bool IsIntegerType(void) const;
	bool IsNumericType(void) const;
	bool IsSimpleType(void) const;

	Declaration* ToConstType(void);
	Declaration* ToStriped(int stripe);
	Declaration* ToStriped(Errors* errors);
	Declaration* Clone(void);

	int Stride(void) const;
};

void InitDeclarations(void);

extern Declaration* TheVoidTypeDeclaration, * TheSignedIntTypeDeclaration, * TheUnsignedIntTypeDeclaration, * TheConstCharTypeDeclaration, * TheCharTypeDeclaration, * TheSignedCharTypeDeclaration, * TheUnsignedCharTypeDeclaration;
extern Declaration* TheBoolTypeDeclaration, * TheFloatTypeDeclaration, * TheVoidPointerTypeDeclaration, * TheSignedLongTypeDeclaration, * TheUnsignedLongTypeDeclaration;
extern Declaration* TheVoidFunctionTypeDeclaration, * TheConstVoidValueDeclaration;
extern Declaration* TheCharPointerTypeDeclaration, * TheConstCharPointerTypeDeclaration;


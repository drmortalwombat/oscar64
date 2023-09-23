#pragma once

#include "Ident.h"
#include "Scanner.h"
#include "MachineTypes.h"
#include "Assembler.h"
#include "Array.h"

class LinkerObject;
class LinkerSection;
class Parser;

enum DecType
{
	DT_TYPE_VOID,
	DT_TYPE_NULL,
	DT_TYPE_BOOL,
	DT_TYPE_INTEGER,
	DT_TYPE_FLOAT,
	DT_TYPE_ENUM,
	DT_TYPE_POINTER,
	DT_TYPE_REFERENCE,
	DT_TYPE_RVALUEREF,
	DT_TYPE_ARRAY,
	DT_TYPE_STRUCT,
	DT_TYPE_UNION,
	DT_TYPE_TEMPLATE,
	DT_TYPE_FUNCTION,
	DT_TYPE_ASSEMBLER,
	DT_TYPE_AUTO,

	DT_CONST_INTEGER,
	DT_CONST_FLOAT,
	DT_CONST_FUNCTION,
	DT_CONST_ADDRESS,
	DT_CONST_DATA,
	DT_CONST_STRUCT,
	DT_CONST_POINTER,
	DT_CONST_REFERENCE,
	DT_CONST_ASSEMBLER,
	DT_CONST_CONSTRUCTOR,
	DT_CONST_TEMPLATE,

	DT_PACK_TEMPLATE,
	DT_PACK_VARIABLE,
	DT_PACK_ARGUMENT,
	DT_PACK_TYPE,

	DT_VARIABLE,
	DT_ARGUMENT,
	DT_ELEMENT,
	DT_ANON,
	DT_LABEL,
	DT_VARIABLE_REF,
	DT_FUNCTION_REF,
	DT_LABEL_REF,
	DT_NAMESPACE,
	DT_BASECLASS,

	DT_TEMPLATE,

	DT_VTABLE
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
static const uint64 DTF_DYNSTACK		= (1ULL << 25);
static const uint64 DTF_PRIVATE			= (1ULL << 26);
static const uint64 DTF_PROTECTED		= (1ULL << 27);
static const uint64 DTF_VIRTUAL			= (1ULL << 28);
static const uint64 DTF_TEMPORARY		= (1ULL << 29);
static const uint64	DTF_COMPLETED		= (1ULL << 30);
static const uint64	DTF_CONSTEXPR		= (1ULL << 31);

static const uint64 DTF_AUTO_TEMPLATE	= (1ULL << 32);

static const uint64 DTF_FUNC_VARIABLE	= (1ULL << 36);
static const uint64 DTF_FUNC_ASSEMBLER	= (1ULL << 37);
static const uint64 DTF_FUNC_RECURSIVE  = (1ULL << 38);
static const uint64 DTF_FUNC_ANALYZING  = (1ULL << 39);

static const uint64 DTF_FUNC_CONSTEXPR	= (1ULL << 40);
static const uint64 DTF_FUNC_INTRSAVE   = (1ULL << 41);
static const uint64 DTF_FUNC_INTRCALLED = (1ULL << 42);
static const uint64 DTF_FUNC_PURE		= (1ULL << 43);

static const uint64 DTF_FPARAM_CONST	= (1ULL << 44);
static const uint64 DTF_FPARAM_NOCONST	= (1ULL << 45);
static const uint64 DTF_VAR_ADDRESS		= (1ULL << 46);

static const uint64 DTF_FUNC_THIS		= (1ULL << 47);

static const uint64 DTF_VAR_ALIASING	= (1ULL << 48);
static const uint64 DTF_FPARAM_UNUSED	= (1ULL << 49);


class Declaration;

enum ScopeLevel
{
	SLEVEL_SCOPE,
	SLEVEL_USING,
	SLEVEL_GLOBAL,
	SLEVEL_STATIC,
	SLEVEL_NAMESPACE,
	SLEVEL_TEMPLATE,
	SLEVEL_CLASS,
	SLEVEL_FUNCTION,
	SLEVEL_LOCAL,
};

class DeclarationScope
{
public:
	DeclarationScope(DeclarationScope * parent, ScopeLevel level, const Ident * name = nullptr);
	~DeclarationScope(void);

	const Ident* Mangle(const Ident* ident) const;

	Declaration* Insert(const Ident* ident, Declaration* dec);
	Declaration* Lookup(const Ident* ident, ScopeLevel limit = SLEVEL_GLOBAL);

	void End(const Location & loc);
	void Clear(void);

	void UseScope(DeclarationScope* scope);

	template<typename F> void Iterate(F && f);

	ScopeLevel		mLevel;
	const Ident	*	mName;

	DeclarationScope* mParent;
protected:
	struct Entry
	{
		const Ident* mIdent;
		Declaration* mDec;
	};
	Entry	* mHash;
	int			mHashSize, mHashFill;
	ExpandingArray<DeclarationScope*>	mUsed;
};

template<typename F> void DeclarationScope::Iterate(F&& f)
{
	for (int i = 0; i < mHashSize; i++)
	{
		if (mHash[i].mIdent)
			f(mHash[i].mIdent, mHash[i].mDec);
	}
}

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
	EX_VCALL,
	EX_DISPATCH,
	EX_LIST,
	EX_COMMA,
	EX_RETURN,
	EX_SEQUENCE,
	EX_WHILE,
	EX_IF,
	EX_ELSE,
	EX_FOR,
	EX_DO,
	EX_SCOPE,
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
	EX_ASSUME,
	EX_BANKOF,
	EX_CONSTRUCT,
	EX_CLEANUP,
	EX_RESULT,
	EX_PACK,
	EX_PACK_TYPE,
};

class Expression
{
public:
	Expression(const Location& loc, ExpressionType type);
	~Expression(void);

	Location				mLocation, mEndLocation;
	ExpressionType			mType;
	Expression			*	mLeft, * mRight;
	Token					mToken;
	Declaration			*	mDecValue, * mDecType;
	AsmInsType				mAsmInsType;
	AsmInsMode				mAsmInsMode;
	bool					mConst;

	Expression* LogicInvertExpression(void);
	Expression* ConstantFold(Errors * errors, LinkerSection* dataSection);
	bool HasSideEffects(void) const;

	bool IsSame(const Expression* exp) const;
	bool IsRValue(void) const;
	bool IsLValue(void) const;
	bool IsConstRef(void) const;

	void Dump(int ident) const;
};

class Declaration
{
public:
	Declaration(const Location & loc, DecType type);
	~Declaration(void);

	Location			mLocation, mEndLocation;
	DecType				mType;
	Token				mToken;
	Declaration		*	mBase, * mParams, * mParamPack, * mNext, * mPrev, * mConst, * mMutable;
	Declaration		*	mDefaultConstructor, * mDestructor, * mCopyConstructor, * mCopyAssignment, * mMoveConstructor, * mMoveAssignment;
	Declaration		*	mVectorConstructor, * mVectorDestructor, * mVectorCopyConstructor, * mVectorCopyAssignment;
	Declaration		*	mVTable, * mClass, * mTemplate;

	Expression*			mValue, * mReturn;
	DeclarationScope*	mScope;
	int					mOffset, mSize, mVarIndex, mNumVars, mComplexity, mLocalSize, mAlignment, mFastCallBase, mFastCallSize, mStride, mStripe;
	uint8				mShift, mBits;
	int64				mInteger, mMinValue, mMaxValue;
	double				mNumber;
	uint64				mFlags, mCompilerOptions, mOptFlags;
	const Ident		*	mIdent, * mQualIdent, * mMangleIdent;
	LinkerSection	*	mSection;
	const uint8		*	mData;
	LinkerObject	*	mLinkerObject;
	int					mUseCount;
	TokenSequence	*	mTokens;
	Parser			*	mParser;

	GrowingArray<Declaration*>	mCallers, mCalled, mFriends;

	bool CanAssign(const Declaration* fromType) const;
	bool IsSame(const Declaration* dec) const;
	bool IsDerivedFrom(const Declaration* dec) const;
	bool IsSubType(const Declaration* dec) const;
	bool IsConstSame(const Declaration* dec) const;
	bool IsSameValue(const Declaration* dec) const;
	bool IsSameParams(const Declaration* dec) const;
	bool IsSameMutable(const Declaration* dec) const;

	bool IsTemplateSame(const Declaration* dec, const Declaration* tdec) const;
	bool IsTemplateSameParams(const Declaration* dec, const Declaration* tdec) const;
	bool IsSameTemplate(const Declaration* dec) const;

	bool IsIntegerType(void) const;
	bool IsNumericType(void) const;
	bool IsSimpleType(void) const;
	bool IsReference(void) const;
	bool IsIndexed(void) const;

	void SetDefined(void);

	Declaration* ToConstType(void);
	Declaration* ToMutableType(void);

	Declaration* ToStriped(int stripe);
	Declaration* ToStriped(Errors* errors);
	Declaration* Clone(void);
	Declaration* Last(void);

	Declaration* BuildPointer(const Location& loc);
	Declaration* BuildReference(const Location& loc, DecType type = DT_TYPE_REFERENCE);
	Declaration* BuildConstPointer(const Location& loc);
	Declaration* BuildConstReference(const Location& loc, DecType type = DT_TYPE_REFERENCE);
	Declaration* BuildRValueRef(const Location& loc);
	Declaration* BuildConstRValueRef(const Location& loc);
	Declaration* NonRefBase(void);
	Declaration* BuildArrayPointer(void);
	Declaration* DeduceAuto(Declaration* dec);
	Declaration* ConstCast(Declaration* ntype);
	bool IsNullConst(void) const;
	bool IsAuto(void) const;

	DecType ValueType(void) const;

	bool CanResolveTemplate(Expression* pexp, Declaration* tdec);
	bool ResolveTemplate(Declaration* fdec, Declaration * tdec);
	bool ResolveTemplate(Expression* pexp, Declaration* tdec);

	Declaration* ExpandTemplate(DeclarationScope* scope);

	const Ident* MangleIdent(void);

	int Stride(void) const;
};

void InitDeclarations(void);

extern Declaration* TheVoidTypeDeclaration, * TheConstVoidTypeDeclaration, * TheSignedIntTypeDeclaration, * TheUnsignedIntTypeDeclaration, * TheConstCharTypeDeclaration, * TheCharTypeDeclaration, * TheSignedCharTypeDeclaration, * TheUnsignedCharTypeDeclaration;
extern Declaration* TheBoolTypeDeclaration, * TheFloatTypeDeclaration, * TheVoidPointerTypeDeclaration, * TheConstVoidPointerTypeDeclaration, * TheSignedLongTypeDeclaration, * TheUnsignedLongTypeDeclaration;
extern Declaration* TheVoidFunctionTypeDeclaration, * TheConstVoidValueDeclaration;
extern Declaration* TheCharPointerTypeDeclaration, * TheConstCharPointerTypeDeclaration;
extern Declaration* TheNullptrConstDeclaration, * TheZeroIntegerConstDeclaration, * TheZeroFloatConstDeclaration;
extern Expression* TheVoidExpression;


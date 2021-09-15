#pragma once

#include "InterCode.h"
#include "Ident.h"

enum ByteCode
{
	BC_NOP,
	BC_EXIT,

	BC_CONST_8,
	BC_CONST_P8,
	BC_CONST_N8,
	BC_CONST_16,
	BC_CONST_32,

	BC_LOAD_REG_8,
	BC_STORE_REG_8,
	BC_LOAD_REG_16,
	BC_STORE_REG_16,
	BC_ADDR_REG,
	BC_LOAD_REG_32,
	BC_STORE_REG_32,

	BC_LOAD_ABS_8,
	BC_LOAD_ABS_U8,
	BC_LOAD_ABS_I8,
	BC_LOAD_ABS_16,
	BC_LOAD_ABS_32,

	BC_STORE_ABS_8,
	BC_STORE_ABS_16,
	BC_STORE_ABS_32,

	BC_LEA_ABS,

	BC_LOAD_LOCAL_8,
	BC_LOAD_LOCAL_U8,
	BC_LOAD_LOCAL_I8,
	BC_LOAD_LOCAL_16,
	BC_LOAD_LOCAL_32,

	BC_STORE_LOCAL_8,
	BC_STORE_LOCAL_16,
	BC_STORE_LOCAL_32,

	BC_LEA_LOCAL,

	BC_STORE_FRAME_8,
	BC_STORE_FRAME_16,
	BC_STORE_FRAME_32,

	BC_LOAD_ADDR_8,
	BC_LOAD_ADDR_U8,
	BC_LOAD_ADDR_I8,
	BC_LOAD_ADDR_16,
	BC_LOAD_ADDR_32,

	BC_STORE_ADDR_8,
	BC_STORE_ADDR_16,
	BC_STORE_ADDR_32,

	BC_BINOP_ADDR_16,
	BC_BINOP_SUBR_16,
	BC_BINOP_ANDR_16,
	BC_BINOP_ORR_16,
	BC_BINOP_XORR_16,
	BC_BINOP_MULR_16,
	BC_BINOP_DIVR_U16,
	BC_BINOP_MODR_U16,
	BC_BINOP_DIVR_I16,
	BC_BINOP_MODR_I16,
	BC_BINOP_SHLR_16,
	BC_BINOP_SHRR_U16,
	BC_BINOP_SHRR_I16,

	BC_BINOP_ADDI_16,
	BC_BINOP_SUBI_16,
	BC_BINOP_ANDI_16,
	BC_BINOP_ORI_16,
	BC_BINOP_MULI8_16,

	BC_BINOP_SHLI_16,
	BC_BINOP_SHRI_U16,
	BC_BINOP_SHRI_I16,

	BC_BINOP_CMPUR_16,
	BC_BINOP_CMPSR_16,
	
	BC_BINOP_CMPUI_16,
	BC_BINOP_CMPSI_16,

	BC_OP_NEGATE_16,
	BC_OP_INVERT_16,

	BC_BINOP_ADD_F32,
	BC_BINOP_SUB_F32,
	BC_BINOP_MUL_F32,
	BC_BINOP_DIV_F32,
	BC_BINOP_CMP_F32,
	BC_OP_NEGATE_F32,
	BC_OP_ABS_F32,
	BC_OP_FLOOR_F32,
	BC_OP_CEIL_F32,

	BC_CONV_U16_F32,
	BC_CONV_I16_F32,
	BC_CONV_F32_U16,
	BC_CONV_F32_I16,

	BC_CONV_I8_I16,

	BC_JUMPS,
	BC_BRANCHS_EQ,
	BC_BRANCHS_NE,
	BC_BRANCHS_GT,
	BC_BRANCHS_GE,
	BC_BRANCHS_LT,
	BC_BRANCHS_LE,

	BC_JUMPF,
	BC_BRANCHF_EQ,
	BC_BRANCHF_NE,
	BC_BRANCHF_GT,
	BC_BRANCHF_GE,
	BC_BRANCHF_LT,
	BC_BRANCHF_LE,

	BC_SET_EQ,
	BC_SET_NE,
	BC_SET_GT,
	BC_SET_GE,
	BC_SET_LT,
	BC_SET_LE,

	BC_ENTER,
	BC_RETURN,
	BC_CALL,
	BC_PUSH_FRAME,
	BC_POP_FRAME,

	BC_JSR,

	BC_COPY,
	BC_COPY_LONG,

	BC_NATIVE = 0x75
};

class ByteCodeProcedure;
class ByteCodeGenerator;

class ByteCodeRelocation
{
public:
	uint16			mAddr;
	bool			mFunction, mLower, mUpper;
	uint16			mIndex, mOffset;
	const char  *	mRuntime;
};

class ByteCodeBasicBlock;

class ByteCodeInstruction
{
public:
	ByteCodeInstruction(ByteCode code = BC_NOP);

	void Assemble(ByteCodeGenerator* generator, ByteCodeBasicBlock* block);

	ByteCode	mCode;
	uint32		mRegister;
	int			mValue, mVIndex;
	bool		mRelocate, mFunction, mRegisterFinal;

	bool IsStore(void) const;
	bool ChangesAccu(void) const;
	bool ChangesAddr(void) const;
	bool ChangesRegister(uint32 reg) const;
	bool LoadsRegister(uint32 reg) const;
	bool StoresRegister(uint32 reg) const;

	bool IsCommutative(void) const;
};

class ByteCodeBasicBlock
{
public:
	DynamicArray<uint8>					mCode;
	int									mIndex;

	ByteCodeBasicBlock				*	mTrueJump, * mFalseJump;
	ByteCodeBasicBlock				*	mTrueLink, * mFalseLink;
	ByteCode							mBranch;

	GrowingArray<ByteCodeInstruction>	mIns;
	GrowingArray<ByteCodeRelocation>	mRelocations;

	int						mOffset, mSize;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled;

	ByteCodeBasicBlock(void);

	void Assemble(ByteCodeGenerator* generator);
	void Compile(InterCodeProcedure* iproc, ByteCodeProcedure * proc, InterCodeBasicBlock * block);
	void Close(ByteCodeBasicBlock* trueJump, ByteCodeBasicBlock* falseJump, ByteCode branch);

	void PutByte(uint8 code);
	void PutWord(uint16 code);
	void PutDWord(uint32 code);
	void PutBytes(const uint8* code, int num);

	void PutCode(ByteCodeGenerator* generator, ByteCode code);
	int PutBranch(ByteCodeGenerator* generator, ByteCode code, int offset);

	ByteCodeBasicBlock* BypassEmptyBlocks(void);
	void CalculateOffset(int& total);
	void CopyCode(ByteCodeGenerator* generator, uint8* target);

	void IntConstToAccu(__int64 val);
	void IntConstToAddr(__int64 val);
	void FloatConstToAccu(double val);
	void FloatConstToWork(double val);
	void CopyValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreDirectValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadDirectValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins);
	void BinaryOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void UnaryOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void BinaryRROperator(InterCodeProcedure* proc, const InterInstruction * ins);
	ByteCode RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void BinaryIntOperator(InterCodeProcedure* proc, const InterInstruction * ins, ByteCode code);
	void NumericConversion(InterCodeProcedure* proc, const InterInstruction * ins);

	void PeepHoleOptimizer(void);
};

class ByteCodeGenerator;

class ByteCodeProcedure
{
public:
	ByteCodeProcedure(void);
	~ByteCodeProcedure(void);

	ByteCodeBasicBlock	* entryBlock, * exitBlock;
	ByteCodeBasicBlock	** tblocks;

	int		mProgStart, mProgSize, mID;

	void Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc);
	ByteCodeBasicBlock * CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);

	void Disassemble(FILE * file, ByteCodeGenerator* generator, InterCodeProcedure* proc);
protected:
	const char* TempName(uint8 tmp, char * buffer, InterCodeProcedure* proc);
};

class ByteCodeGenerator
{
public:
	ByteCodeGenerator(void);
	~ByteCodeGenerator(void);

	struct Address
	{
		int				mIndex, mAddress, mSize;
		bool			mFunction, mAssembler;
		const Ident* mIdent;
	};

	GrowingArray<Address>				mProcedureAddr, mGlobalAddr;
	GrowingArray<ByteCodeRelocation>	mRelocations;

	bool	mByteCodeUsed[128];

	uint8	mMemory[0x10000];
	int		mProgEnd, mProgStart, mProgEntry;

	void WriteBasicHeader(void);
	void WriteByteCodeHeader(void);
	void SetBasicEntry(int index);

	bool WritePRGFile(const char* filename);
	bool WriteMapFile(const char* filename);

	void WriteAsmFile(FILE * file);

	void WriteAsmFile(FILE* file, Address & addr);

	void ResolveRelocations(void);

	int AddGlobal(int index, const Ident* ident, int size, const uint8* data, bool assembler);

	void AddAddress(int index, bool function, int address, int size, const Ident * ident, bool assembler);

};

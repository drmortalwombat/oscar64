#pragma once

#include "InterCode.h"
#include "Ident.h"
#include "Disassembler.h"

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
	BC_LOAD_ABS_16,
	BC_LOAD_ABS_32,

	BC_STORE_ABS_8,
	BC_STORE_ABS_16,
	BC_STORE_ABS_32,

	BC_LEA_ABS,
	BC_LEA_ABS_INDEX,
	BC_LEA_ABS_INDEX_U8,
	BC_LEA_ACCU_INDEX,

	BC_LOAD_LOCAL_8,
	BC_LOAD_LOCAL_U8,
	BC_LOAD_LOCAL_16,
	BC_LOAD_LOCAL_32,

	BC_STORE_LOCAL_8,
	BC_STORE_LOCAL_16,
	BC_STORE_LOCAL_32,

	BC_LEA_LOCAL,

	BC_STORE_FRAME_8,
	BC_STORE_FRAME_16,
	BC_STORE_FRAME_32,

	BC_LEA_FRAME,

	BC_LOAD_ADDR_8,
	BC_LOAD_ADDR_U8,
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

	BC_BINOP_ADDA_16,

	BC_BINOP_ADDI_16,
	BC_BINOP_SUBI_16,
	BC_BINOP_ANDI_16,
	BC_BINOP_ORI_16,
	BC_BINOP_MULI8_16,

	BC_BINOP_ADDI_8,
	BC_BINOP_ANDI_8,
	BC_BINOP_ORI_8,

	BC_BINOP_SHLI_16,
	BC_BINOP_SHRI_U16,
	BC_BINOP_SHRI_I16,

	BC_BINOP_CMPUR_16,
	BC_BINOP_CMPSR_16,
	
	BC_BINOP_CMPUI_16,
	BC_BINOP_CMPSI_16,

	BC_BINOP_CMPUR_8,
	BC_BINOP_CMPSR_8,

	BC_BINOP_CMPUI_8,
	BC_BINOP_CMPSI_8,

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

	BC_JSR,

	BC_NATIVE = 0x75,

	BC_ENTER,
	BC_RETURN,
	BC_CALL_ADDR,
	BC_CALL_ABS,
	BC_PUSH_FRAME,
	BC_POP_FRAME,

	BC_COPY,
	BC_COPY_LONG,
	BC_STRCPY,

	BC_EXTRT,

	BC_CONV_I16_I32 = 0x80,
	BC_CONV_U16_U32,

	BC_OP_NEGATE_32,
	BC_OP_INVERT_32,

	BC_BINOP_ADD_L32,
	BC_BINOP_SUB_L32,
	BC_BINOP_AND_L32,
	BC_BINOP_OR_L32,
	BC_BINOP_XOR_L32,
	BC_BINOP_MUL_L32,
	BC_BINOP_DIV_U32,
	BC_BINOP_MOD_U32,
	BC_BINOP_DIV_I32,
	BC_BINOP_MOD_I32,
	BC_BINOP_SHL_L32,
	BC_BINOP_SHR_U32,
	BC_BINOP_SHR_I32,
	BC_BINOP_CMP_U32,
	BC_BINOP_CMP_S32
};

class ByteCodeProcedure;
class ByteCodeGenerator;

class ByteCodeBasicBlock;

class ByteCodeInstruction
{
public:
	ByteCodeInstruction(ByteCode code = BC_NOP);

	void Assemble(ByteCodeGenerator* generator, ByteCodeBasicBlock* block);

	ByteCode	mCode;
	uint32		mRegister;
	int			mValue;
	bool		mRelocate, mRegisterFinal;
	LinkerObject* mLinkerObject;
	const char* mRuntime;
	uint32		mLive;

	bool IsStore(void) const;
	bool ChangesAccu(void) const;
	bool ChangesAddr(void) const;
	bool ChangesRegister(uint32 reg) const;

	bool UsesAccu(void) const;
	bool UsesAddr(void) const;
	bool UsesRegister(uint32 reg) const;

	bool LoadsRegister(uint32 reg) const;
	bool StoresRegister(uint32 reg) const;
	bool IsLocalStore(void) const;
	bool IsLocalLoad(void) const;
	bool IsLocalAccess(void) const;
	bool IsShiftByRegister(void) const;
	bool IsIntegerConst(void) const;

	bool IsCommutative(void) const;
	bool IsSame(const ByteCodeInstruction& ins) const;

	bool ValueForwarding(ByteCodeInstruction*& accuIns, ByteCodeInstruction*& addrIns);

	bool CheckAccuSize(uint32 & used);
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
	GrowingArray<LinkerReference>	mRelocations;
	GrowingArray<ByteCodeBasicBlock*>	mEntryBlocks;

	int						mOffset, mSize, mPlace, mLinear;
	bool					mPlaced, mNeedsNop, mBypassed, mAssembled, mVisited;
	uint32					mExitLive;

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
	void BuildPlacement(GrowingArray<ByteCodeBasicBlock*> & placement);
	void InitialOffset(int& total, int& linear);
	bool CalculateOffset(int & total);

	void CopyCode(ByteCodeGenerator* generator, LinkerObject * linkerObject, uint8* target);

	void LongConstToAccu(int64 val);
	void LongConstToWork(int64 val);
	void IntConstToAccu(int64 val);
	void IntConstToAddr(int64 val);
	void FloatConstToAccu(double val);
	void FloatConstToWork(double val);
	void CopyValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void StrcpyValue(InterCodeProcedure* proc, const InterInstruction* ins);
	void LoadConstant(InterCodeProcedure* proc, const InterInstruction * ins);
	void StoreDirectValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadDirectValue(InterCodeProcedure* proc, const InterInstruction * ins);
	void LoadStoreIndirectValue(InterCodeProcedure* proc, const InterInstruction* rins, const InterInstruction* wins);

	void LoadEffectiveAddress(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallFunction(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallAssembler(InterCodeProcedure* proc, const InterInstruction * ins);
	void CallNative(InterCodeProcedure* proc, const InterInstruction* ins);
	void BinaryOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void UnaryOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void BinaryRROperator(InterCodeProcedure* proc, const InterInstruction * ins);
	ByteCode RelationalOperator(InterCodeProcedure* proc, const InterInstruction * ins);
	void BinaryIntOperator(InterCodeProcedure* proc, const InterInstruction * ins, ByteCode code);
	void NumericConversion(InterCodeProcedure* proc, const InterInstruction * ins);



	void CollectEntryBlocks(ByteCodeBasicBlock * block);

	bool JoinTailCodeSequences(void);
	bool SameTail(ByteCodeInstruction& ins);
	bool PropagateAccuCrossBorder(int accu, int addr);

	bool PeepHoleOptimizer(int phase);
};

class ByteCodeGenerator;

class ByteCodeProcedure
{
public:
	ByteCodeProcedure(void);
	~ByteCodeProcedure(void);

	ByteCodeBasicBlock	* entryBlock, * exitBlock;
	ByteCodeBasicBlock	** tblocks;
	GrowingArray < ByteCodeBasicBlock*>	 mBlocks;

	int		mProgSize, mID, mNumBlocks;

	void Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc);
	ByteCodeBasicBlock * CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);

	void ResetVisited(void);

protected:
	ByteCodeDisassembler	mDisassembler;
};

class ByteCodeGenerator
{
public:
	ByteCodeGenerator(Errors* errors, Linker* linker);
	~ByteCodeGenerator(void);

	Errors* mErrors;
	Linker* mLinker;

	uint32	mByteCodeUsed[128];
	LinkerObject* mExtByteCodes[128];

	bool WriteByteCodeStats(const char* filename);
};

#pragma once

#include "ByteCodeGenerator.h"
#include "Assembler.h"

class NativeCodeProcedure;
class NativeCodeBasicBlock;

class NativeCodeInstruction
{
public:
	NativeCodeInstruction(AsmInsType type, AsmInsMode mode);

	AsmInsType		mType;
	AsmInsMode		mMode;

	int				mAddress, mVarIndex;
	bool			mGlobal;

	void Assemble(ByteCodeGenerator* generator, NativeCodeBasicBlock* block);
};

class NativeCodeBasicBlock
{
public:
	NativeCodeBasicBlock(void);
	~NativeCodeBasicBlock(void);

	DynamicArray<uint8>					mCode;
	int									mIndex;

	NativeCodeBasicBlock* mTrueJump, * mFalseJump;
	AsmInsType							mBranch;

	GrowingArray<NativeCodeInstruction>	mIns;
	GrowingArray<ByteCodeRelocation>	mRelocations;

	int						mOffset, mSize;
	bool					mPlaced, mCopied, mKnownShortBranch, mBypassed, mAssembled;

	int PutBranch(ByteCodeGenerator* generator, AsmInsType code, int offset);

	NativeCodeBasicBlock* BypassEmptyBlocks(void);
	void CalculateOffset(int& total);
	void CopyCode(ByteCodeGenerator* generator, uint8* target);

	void Assemble(ByteCodeGenerator* generator);
	void Compile(InterCodeProcedure* iproc, NativeCodeProcedure* proc, InterCodeBasicBlock* block);
	void Close(NativeCodeBasicBlock* trueJump, NativeCodeBasicBlock* falseJump, AsmInsType branch);

	void PutByte(uint8 code);
	void PutWord(uint16 code);

};

class NativeCodeProcedure
{
	public:
		NativeCodeProcedure(void);
		~NativeCodeProcedure(void);

		NativeCodeBasicBlock* entryBlock, * exitBlock;
		NativeCodeBasicBlock** tblocks;

		int		mProgStart, mProgSize;

		void Compile(ByteCodeGenerator* generator, InterCodeProcedure* proc);
		NativeCodeBasicBlock* CompileBlock(InterCodeProcedure* iproc, InterCodeBasicBlock* block);

};


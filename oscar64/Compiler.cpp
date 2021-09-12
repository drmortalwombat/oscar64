#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"
#include "InterCodeGenerator.h"
#include "InterCode.h"
#include "ByteCodeGenerator.h"
#include "NativeCodeGenerator.h"
#include "Emulator.h"
#include <stdio.h>

Compiler::Compiler(void)
	: mByteCodeFunctions(nullptr), mNativeCode(false)
{
	mErrors = new Errors();
	mCompilationUnits = new CompilationUnits(mErrors);
	mPreprocessor = new Preprocessor(mErrors);
	mByteCodeGenerator = new ByteCodeGenerator();
	mInterCodeGenerator = new InterCodeGenerator(mErrors);
	mInterCodeModule = new InterCodeModule();
}

Compiler::~Compiler(void)
{

}

void Compiler::ForceNativeCode(bool native)
{
	mNativeCode = native;
}

bool Compiler::ParseSource(void)
{
	CompilationUnit* cunit;
	while (mErrors->mErrorCount == 0 && (cunit = mCompilationUnits->PendingUnit()))
	{
		if (mPreprocessor->OpenSource(cunit->mFileName, true))
		{
			Scanner* scanner = new Scanner(mErrors, mPreprocessor);
			Parser* parser = new Parser(mErrors, scanner, mCompilationUnits);

			parser->Parse();
		}
		else
			mErrors->Error(cunit->mLocation, "Could not open source file", cunit->mFileName);
	}

	return mErrors->mErrorCount == 0;
}

bool Compiler::GenerateCode(void)
{
	Location	loc;

	Declaration* dcrtstart = mCompilationUnits->mStartup;
	if (!dcrtstart)
	{
		mErrors->Error(loc, "Runtime startup not found");
		return false;
	}

	mInterCodeGenerator->mForceNativeCode = mNativeCode;
	mInterCodeGenerator->TranslateAssembler(mInterCodeModule, dcrtstart->mValue);

	if (mErrors->mErrorCount != 0)
		return false;

	mByteCodeGenerator->WriteBasicHeader();

	mInterCodeModule->UseGlobal(dcrtstart->mVarIndex);

	InterVariable& vmain(mInterCodeModule->mGlobalVars[dcrtstart->mVarIndex]);
	vmain.mAddr = mByteCodeGenerator->AddGlobal(vmain.mIndex, vmain.mIdent, vmain.mSize, vmain.mData, vmain.mAssembler);
	vmain.mPlaced = true;
	mByteCodeGenerator->SetBasicEntry(dcrtstart->mVarIndex);

	mByteCodeGenerator->mProgEnd = 0x0a00;
	mByteCodeGenerator->WriteByteCodeHeader();

	const Ident* imain = Ident::Unique("main");
	Declaration* dmain = mCompilationUnits->mScope->Lookup(imain);
	if (!dmain)
	{
		mErrors->Error(loc, "main function not found");
		return false;
	}

	InterCodeProcedure* iproc = mInterCodeGenerator->TranslateProcedure(mInterCodeModule, dmain->mValue, dmain);

	if (mErrors->mErrorCount != 0)
		return false;

	for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
	{
		InterCodeProcedure* proc = mInterCodeModule->mProcedures[i];

		proc->ReduceTemporaries();

#if _DEBUG
		proc->Disassemble("final");
#endif


		if (proc->mNativeProcedure)
		{
			NativeCodeProcedure* ncproc = new NativeCodeProcedure();
			ncproc->Compile(mByteCodeGenerator, proc);
		}
		else
		{
			ByteCodeProcedure* bgproc = new ByteCodeProcedure();

			bgproc->Compile(mByteCodeGenerator, proc);
			mByteCodeFunctions.Push(bgproc);

#if _DEBUG
			FILE* file;
			fopen_s(&file, "r:\\cldiss.txt", "a");

			if (file)
			{
				bgproc->Disassemble(file, mByteCodeGenerator, mInterCodeModule->mProcedures[i]);
				fclose(file);
			}
#endif
		}
	}

	// Compile used runtime functions

	for (int i = 0; i < mByteCodeGenerator->mRelocations.Size(); i++)
	{
		if (mByteCodeGenerator->mRelocations[i].mRuntime)
		{
			Declaration* bcdec = mCompilationUnits->mRuntimeScope->Lookup(Ident::Unique(mByteCodeGenerator->mRelocations[i].mRuntime));
			if (bcdec)
			{
				int	index = -1, offset = 0;
				if (bcdec->mType == DT_CONST_ASSEMBLER)
				{
					if (bcdec->mVarIndex < 0)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue);
					index = bcdec->mVarIndex;
				}
				else if (bcdec->mType == DT_LABEL)
				{
					if (bcdec->mBase->mVarIndex < 0)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue);
					index = bcdec->mBase->mVarIndex;
					offset = bcdec->mInteger;
				}
				else if (bcdec->mType == DT_VARIABLE)
				{
					if (bcdec->mBase->mVarIndex < 0)
						mInterCodeGenerator->InitGlobalVariable(mInterCodeModule, bcdec);
					index = bcdec->mVarIndex;
					offset = bcdec->mOffset + mByteCodeGenerator->mRelocations[i].mOffset;
				}
				assert(index > 0);
				mInterCodeModule->UseGlobal(index);

				mByteCodeGenerator->mRelocations[i].mIndex = index;
				mByteCodeGenerator->mRelocations[i].mOffset = offset;
			}
			else
			{
				mErrors->Error(loc, "Missing runtime code implementation", mByteCodeGenerator->mRelocations[i].mRuntime);
			}
		}
	}

	// Compile used byte code functions

	for (int i = 0; i < 128; i++)
	{
		if (mByteCodeGenerator->mByteCodeUsed[i])
		{
			Declaration* bcdec = mCompilationUnits->mByteCodes[i];
			if (bcdec)
			{
				int	index = -1, offset = 0;
				if (bcdec->mType == DT_CONST_ASSEMBLER)
				{
					if (bcdec->mVarIndex < 0)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue);
					index = bcdec->mVarIndex;
				}
				else if (bcdec->mType == DT_LABEL)
				{
					if (bcdec->mBase->mVarIndex < 0)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue);
					index = bcdec->mBase->mVarIndex;
					offset = bcdec->mInteger;
				}

				assert(index > 0);
				mInterCodeModule->UseGlobal(index);

				ByteCodeRelocation	rel;
				rel.mAddr = 0x900 + 2 * i;
				rel.mFunction = false;
				rel.mLower = true;
				rel.mUpper = true;
				rel.mIndex = index;
				rel.mOffset = offset;
				rel.mRuntime = nullptr;
				mByteCodeGenerator->mRelocations.Push(rel);
			}
			else
			{
				char	n[10];
				sprintf_s(n, "%d", i);
				mErrors->Error(loc, "Missing byte code implementation", n);
			}
		}
	}

	for (int i = 0; i < mInterCodeModule->mGlobalVars.Size(); i++)
	{
		InterVariable& var(mInterCodeModule->mGlobalVars[i]);
		if (var.mUsed)
		{
			if (!var.mPlaced)
			{
				var.mAddr = mByteCodeGenerator->AddGlobal(var.mIndex, var.mIdent, var.mSize, var.mData, var.mAssembler);
				var.mPlaced = true;
			}
			for (int j = 0; j < var.mNumReferences; j++)
			{
				InterVariable::Reference& ref(var.mReferences[j]);
				ByteCodeRelocation	rel;
				rel.mAddr = var.mAddr + ref.mAddr;
				rel.mFunction = ref.mFunction;
				rel.mLower = ref.mLower;
				rel.mUpper = ref.mUpper;
				rel.mIndex = ref.mIndex;
				rel.mOffset = ref.mOffset;
				rel.mRuntime = nullptr;
				mByteCodeGenerator->mRelocations.Push(rel);
			}
		}
	}

	mByteCodeGenerator->ResolveRelocations();

	return mErrors->mErrorCount == 0;
}

bool Compiler::WriteOutputFile(const char* targetPath)
{
	char	prgPath[200], mapPath[200], asmPath[200];

	strcpy_s(prgPath, targetPath);
	int		i = strlen(prgPath);
	while (i > 0 && prgPath[i - 1] != '.')
		i--;
	prgPath[i] = 0;
	
	strcpy_s(mapPath, prgPath);
	strcpy_s(asmPath, prgPath);

	strcat_s(prgPath, "prg");
	strcat_s(mapPath, "map");
	strcat_s(asmPath, "asm");

	printf("Writing <%s>\n", prgPath);
	mByteCodeGenerator->WritePRGFile(prgPath);

	printf("Writing <%s>\n", mapPath);
	mByteCodeGenerator->WriteMapFile(mapPath);

	printf("Writing <%s>\n", asmPath);
	{
		FILE* file;
		fopen_s(&file, asmPath, "w");

		if (file)
		{
			for (int i = 0; i < mByteCodeFunctions.Size(); i++)
				mByteCodeFunctions[i]->Disassemble(file, mByteCodeGenerator, mInterCodeModule->mProcedures[mByteCodeFunctions[i]->mID]);

			mByteCodeGenerator->WriteAsmFile(file);

			fclose(file);
		}
	}

	return true;
}

int Compiler::ExecuteCode(void)
{
	Location	loc;

	printf("Running emulation...\n");
	Emulator* emu = new Emulator();
	memcpy(emu->mMemory + mByteCodeGenerator->mProgStart, mByteCodeGenerator->mMemory + mByteCodeGenerator->mProgStart, mByteCodeGenerator->mProgEnd - mByteCodeGenerator->mProgStart);
	emu->mMemory[0x2d] = mByteCodeGenerator->mProgEnd & 0xff;
	emu->mMemory[0x2e] = mByteCodeGenerator->mProgEnd >> 8;
	int ecode = emu->Emulate(2061);
	printf("Emulation result %d\n", ecode);

	if (ecode != 0)
	{
		char	sd[20];
		sprintf_s(sd, "%d", ecode);
		mErrors->Error(loc, "Execution failed", sd);
	}

	return ecode;
}

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
	: mByteCodeFunctions(nullptr), mNativeCode(false), mDefines({nullptr, nullptr})
{
	mErrors = new Errors();
	mLinker = new Linker(mErrors);
	mCompilationUnits = new CompilationUnits(mErrors);
	mPreprocessor = new Preprocessor(mErrors);
	mByteCodeGenerator = new ByteCodeGenerator(mErrors, mLinker);
	mInterCodeGenerator = new InterCodeGenerator(mErrors, mLinker);
	mNativeCodeGenerator = new NativeCodeGenerator(mErrors, mLinker);
	mInterCodeModule = new InterCodeModule();

	mCompilationUnits->mLinker = mLinker;

	mCompilationUnits->mSectionCode = mLinker->AddSection(Ident::Unique("code"), LST_DATA);
	mCompilationUnits->mSectionData = mLinker->AddSection(Ident::Unique("data"), LST_DATA);
	mCompilationUnits->mSectionBSS = mLinker->AddSection(Ident::Unique("bss"), LST_BSS);
	mCompilationUnits->mSectionHeap = mLinker->AddSection(Ident::Unique("heap"), LST_HEAP);
	mCompilationUnits->mSectionStack = mLinker->AddSection(Ident::Unique("stack"), LST_STACK);
	mCompilationUnits->mSectionStack->mSize = 4096;
}

Compiler::~Compiler(void)
{

}

void Compiler::ForceNativeCode(bool native)
{
	mNativeCode = native;
}

void Compiler::AddDefine(const Ident* ident, const char* value)
{
	Define	define;
	define.mIdent = ident;
	define.mValue = value;
	mDefines.Push(define);
}


bool Compiler::ParseSource(void)
{
	CompilationUnit* cunit;
	while (mErrors->mErrorCount == 0 && (cunit = mCompilationUnits->PendingUnit()))
	{
		if (mPreprocessor->OpenSource("Compiling", cunit->mFileName, true))
		{
			Scanner* scanner = new Scanner(mErrors, mPreprocessor);

			for (int i = 0; i < mDefines.Size(); i++)
				scanner->AddMacro(mDefines[i].mIdent, mDefines[i].mValue);

			Parser* parser = new Parser(mErrors, scanner, mCompilationUnits);

			parser->Parse();
		}
		else
			mErrors->Error(cunit->mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", cunit->mFileName);
	}

	return mErrors->mErrorCount == 0;
}

void Compiler::RegisterRuntime(const Location & loc, const Ident* ident)
{
	Declaration* bcdec = mCompilationUnits->mRuntimeScope->Lookup(ident);
	if (bcdec)
	{
		LinkerObject* linkerObject = nullptr;
		int			offset = 0;

		if (bcdec->mType == DT_CONST_ASSEMBLER)
		{
			if (!bcdec->mLinkerObject)
				mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue);
			linkerObject = bcdec->mLinkerObject;
		}
		else if (bcdec->mType == DT_LABEL)
		{
			if (!bcdec->mBase->mLinkerObject)
				mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue);

			linkerObject = bcdec->mBase->mLinkerObject;
			offset = bcdec->mInteger;
		}
		else if (bcdec->mType == DT_VARIABLE)
		{
			if (!bcdec->mBase->mLinkerObject)
				mInterCodeGenerator->InitGlobalVariable(mInterCodeModule, bcdec);
			linkerObject = bcdec->mLinkerObject;
			offset = bcdec->mOffset;
		}

		mNativeCodeGenerator->RegisterRuntime(ident, linkerObject, offset);
	}
	else
	{
		mErrors->Error(loc, EERR_RUNTIME_CODE, "Missing runtime code implementation", ident->mString);
	}
}

bool Compiler::GenerateCode(void)
{
	Location	loc;

	Declaration* dcrtstart = mCompilationUnits->mStartup;
	if (!dcrtstart)
	{
		mErrors->Error(loc, EERR_RUNTIME_CODE, "Runtime startup not found");
		return false;
	}

	const Ident* identStartup = Ident::Unique("startup");
	const Ident* identBytecode = Ident::Unique("bytecode");
	const Ident* identMain = Ident::Unique("main");
	const Ident* identCode = Ident::Unique("code");

	LinkerRegion* regionStartup = mLinker->FindRegion(identStartup);
	if (!regionStartup)		
		regionStartup = mLinker->AddRegion(identStartup, 0x0801, 0x0900);
	
	LinkerRegion* regionBytecode = mLinker->FindRegion(identBytecode);
	if (!regionBytecode)		
		regionBytecode = mLinker->AddRegion(identBytecode, 0x0900, 0x0a00);

	LinkerRegion* regionMain = mLinker->FindRegion(identMain);

	LinkerSection * sectionStartup = mLinker->AddSection(identStartup, LST_DATA);
	LinkerSection * sectionBytecode = mLinker->AddSection(identBytecode, LST_DATA);

	regionStartup->mSections.Push(sectionStartup);
	regionBytecode->mSections.Push(sectionBytecode);

	if (!mLinker->IsSectionPlaced(mCompilationUnits->mSectionCode))
	{
		if (!regionMain)
			regionMain = mLinker->AddRegion(identMain, 0x0a00, 0xa000);

		regionMain->mSections.Push(mCompilationUnits->mSectionCode);
		regionMain->mSections.Push(mCompilationUnits->mSectionData);
		regionMain->mSections.Push(mCompilationUnits->mSectionBSS);
		regionMain->mSections.Push(mCompilationUnits->mSectionHeap);
		regionMain->mSections.Push(mCompilationUnits->mSectionStack);
	}

	dcrtstart->mSection = sectionStartup;

	mInterCodeGenerator->mForceNativeCode = mNativeCode;
	mInterCodeGenerator->TranslateAssembler(mInterCodeModule, dcrtstart->mValue);

	if (mErrors->mErrorCount != 0)
		return false;

	// Register native runtime functions

	RegisterRuntime(loc, Ident::Unique("mul16by8"));
	RegisterRuntime(loc, Ident::Unique("fsplitt"));
	RegisterRuntime(loc, Ident::Unique("fsplita"));
	RegisterRuntime(loc, Ident::Unique("faddsub"));
	RegisterRuntime(loc, Ident::Unique("fmul"));
	RegisterRuntime(loc, Ident::Unique("fdiv"));
	RegisterRuntime(loc, Ident::Unique("mul16"));
	RegisterRuntime(loc, Ident::Unique("divs16"));
	RegisterRuntime(loc, Ident::Unique("mods16"));
	RegisterRuntime(loc, Ident::Unique("divu16"));
	RegisterRuntime(loc, Ident::Unique("modu16"));
	RegisterRuntime(loc, Ident::Unique("bitshift"));
	RegisterRuntime(loc, Ident::Unique("ffloor"));
	RegisterRuntime(loc, Ident::Unique("fceil"));
	RegisterRuntime(loc, Ident::Unique("ftoi"));
	RegisterRuntime(loc, Ident::Unique("ffromi"));
	RegisterRuntime(loc, Ident::Unique("fcmp"));
	RegisterRuntime(loc, Ident::Unique("bcexec"));

	// Register extended byte code functions

	for (int i = 0; i < 128; i++)
	{
		Declaration* bcdec = mCompilationUnits->mByteCodes[i + 128];
		if (bcdec)
		{
			LinkerObject* linkerObject = nullptr;

			int	offset = 0;
			if (bcdec->mType == DT_CONST_ASSEMBLER)
			{
				if (!bcdec->mLinkerObject)
					mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue);
				mByteCodeGenerator->mExtByteCodes[i] = bcdec->mLinkerObject;
			}
		}
	}

	if (mErrors->mErrorCount != 0)
		return false;

	for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
	{
		InterCodeProcedure* proc = mInterCodeModule->mProcedures[i];

//		proc->ReduceTemporaries();

#if _DEBUG
		proc->Disassemble("final");
#endif


		if (proc->mNativeProcedure)
		{
			NativeCodeProcedure* ncproc = new NativeCodeProcedure(mNativeCodeGenerator);
			ncproc->Compile(proc);
		}
		else
		{
			ByteCodeProcedure* bgproc = new ByteCodeProcedure();

			bgproc->Compile(mByteCodeGenerator, proc);
			mByteCodeFunctions.Push(bgproc);
		}
	}

	// Compile used byte code functions

	LinkerObject* byteCodeObject = mLinker->AddObject(loc, Ident::Unique("bytecode"), sectionBytecode, LOT_RUNTIME);

	for (int i = 0; i < 128; i++)
	{
		if (mByteCodeGenerator->mByteCodeUsed[i])
		{
			Declaration* bcdec = mCompilationUnits->mByteCodes[i];
			if (bcdec)
			{
				LinkerObject* linkerObject = nullptr;

				int	offset = 0;
				if (bcdec->mType == DT_CONST_ASSEMBLER)
				{
					if (!bcdec->mLinkerObject)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue);
					linkerObject = bcdec->mLinkerObject;
				}
				else if (bcdec->mType == DT_LABEL)
				{
					if (!bcdec->mBase->mLinkerObject)
						mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue);
					linkerObject = bcdec->mBase->mLinkerObject;
					offset = bcdec->mInteger;
				}

				assert(linkerObject);

				LinkerReference	lref;
				lref.mObject = byteCodeObject;
				lref.mFlags = LREF_HIGHBYTE | LREF_LOWBYTE;
				lref.mOffset = 2 * i;
				lref.mRefObject = linkerObject;
				lref.mRefOffset = offset;
				byteCodeObject->AddReference(lref);
			}
			else
			{
				char	n[10];
				sprintf_s(n, "%d", i);
				mErrors->Error(loc, EERR_RUNTIME_CODE, "Missing byte code implementation", n);
			}
		}
	}

	mLinker->CollectReferences();

	mLinker->ReferenceObject(dcrtstart->mLinkerObject);
	mLinker->ReferenceObject(byteCodeObject);

	mLinker->Link();

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
	mLinker->WritePrgFile(prgPath);

	printf("Writing <%s>\n", mapPath);
	mLinker->WriteMapFile(mapPath);

	printf("Writing <%s>\n", asmPath);
	mLinker->WriteAsmFile(asmPath);

	return true;
}

int Compiler::ExecuteCode(void)
{
	Location	loc;

	printf("Running emulation...\n");
	Emulator* emu = new Emulator();
	memcpy(emu->mMemory + mLinker->mProgramStart, mLinker->mMemory + mLinker->mProgramStart, mLinker->mProgramEnd - mLinker->mProgramStart);
	emu->mMemory[0x2d] = mLinker->mProgramEnd & 0xff;
	emu->mMemory[0x2e] = mLinker->mProgramEnd >> 8;
	int ecode = emu->Emulate(2061);
	printf("Emulation result %d\n", ecode);

	if (ecode != 0)
	{
		char	sd[20];
		sprintf_s(sd, "%d", ecode);
		mErrors->Error(loc, EERR_EXECUTION_FAILED, "Execution failed", sd);
	}

	return ecode;
}

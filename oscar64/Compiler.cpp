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
	: mByteCodeFunctions(nullptr), mCompilerOptions(COPT_DEFAULT), mDefines({nullptr, nullptr})
{
	mErrors = new Errors();
	mLinker = new Linker(mErrors);
	mCompilationUnits = new CompilationUnits(mErrors);

	mCompilationUnits->mLinker = mLinker;
	mCompilationUnits->mSectionCode = mLinker->AddSection(Ident::Unique("code"), LST_DATA);
	mCompilationUnits->mSectionData = mLinker->AddSection(Ident::Unique("data"), LST_DATA);
	mCompilationUnits->mSectionBSS = mLinker->AddSection(Ident::Unique("bss"), LST_BSS);
	mCompilationUnits->mSectionHeap = mLinker->AddSection(Ident::Unique("heap"), LST_HEAP);
	mCompilationUnits->mSectionStack = mLinker->AddSection(Ident::Unique("stack"), LST_STACK);
	mCompilationUnits->mSectionZeroPage = mLinker->AddSection(Ident::Unique("zeropage"), LST_ZEROPAGE);
	mCompilationUnits->mSectionStack->mSize = 4096;
	mCompilationUnits->mSectionHeap->mSize = 1024;

	mPreprocessor = new Preprocessor(mErrors);
	mByteCodeGenerator = new ByteCodeGenerator(mErrors, mLinker);
	mInterCodeGenerator = new InterCodeGenerator(mErrors, mLinker);
	mNativeCodeGenerator = new NativeCodeGenerator(mErrors, mLinker, mCompilationUnits->mSectionCode);
	mInterCodeModule = new InterCodeModule(mLinker);
	mGlobalAnalyzer = new GlobalAnalyzer(mErrors, mLinker);
}

Compiler::~Compiler(void)
{

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
	mPreprocessor->mCompilerOptions = mCompilerOptions;
	mLinker->mCompilerOptions = mCompilerOptions;

	CompilationUnit* cunit;
	while (mErrors->mErrorCount == 0 && (cunit = mCompilationUnits->PendingUnit()))
	{
		if (mPreprocessor->OpenSource("Compiling", cunit->mFileName, true))
		{
			Scanner* scanner = new Scanner(mErrors, mPreprocessor);

			for (int i = 0; i < mDefines.Size(); i++)
				scanner->AddMacro(mDefines[i].mIdent, mDefines[i].mValue);

			Parser* parser = new Parser(mErrors, scanner, mCompilationUnits);

			parser->mCompilerOptions = mCompilerOptions;

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
				mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue, nullptr);
			linkerObject = bcdec->mLinkerObject;
		}
		else if (bcdec->mType == DT_LABEL)
		{
			if (!bcdec->mBase->mLinkerObject)
				mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue, nullptr);

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
		mErrors->Error(loc, EERR_RUNTIME_CODE, "Missing runtime code implementation", ident);
	}
}

void Compiler::CompileProcedure(InterCodeProcedure* proc)
{
	if (!proc->mCompiled)
	{
		proc->mCompiled = true;

		for (int i = 0; i < proc->mCalledFunctions.Size(); i++)
			CompileProcedure(proc->mCalledFunctions[i]);

		proc->MapCallerSavedTemps();

		if (proc->mNativeProcedure)
		{
			NativeCodeProcedure* ncproc = new NativeCodeProcedure(mNativeCodeGenerator);
			if (mCompilerOptions & COPT_VERBOSE2)
				printf("Generate native code <%s>\n", proc->mIdent->mString);

			ncproc->Compile(proc);
		}
		else
		{
			ByteCodeProcedure* bgproc = new ByteCodeProcedure();

			if (mCompilerOptions & COPT_VERBOSE2)
				printf("Generate byte code <%s>\n", proc->mIdent->mString);

			bgproc->Compile(mByteCodeGenerator, proc);
			mByteCodeFunctions.Push(bgproc);
		}
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
	const Ident* identZeroPage = Ident::Unique("zeropage");

	LinkerRegion* regionZeroPage = mLinker->FindRegion(identZeroPage);
	if (!regionZeroPage)
	{
		regionZeroPage = mLinker->AddRegion(identZeroPage, 0x0080, 0x00ff);
	}

	LinkerRegion* regionStartup = mLinker->FindRegion(identStartup);
	if (!regionStartup)
	{
		if (mCompilerOptions & COPT_TARGET_PRG)
			regionStartup = mLinker->AddRegion(identStartup, 0x0801, 0x0900);
		else
			regionStartup = mLinker->AddRegion(identStartup, 0x0800, 0x0900);
	}

	LinkerRegion* regionBytecode = nullptr;
	if (!(mCompilerOptions & COPT_NATIVE))
	{
		regionBytecode = mLinker->FindRegion(identBytecode);
		if (!regionBytecode)
			regionBytecode = mLinker->AddRegion(identBytecode, 0x0900, 0x0a00);
	}

	LinkerRegion* regionMain = mLinker->FindRegion(identMain);

	LinkerSection * sectionStartup = mLinker->AddSection(identStartup, LST_DATA);
	LinkerSection* sectionBytecode = nullptr;
	if (regionBytecode)
	{
		sectionBytecode = mLinker->AddSection(identBytecode, LST_DATA);
	}

	regionStartup->mSections.Push(sectionStartup);

	regionZeroPage->mSections.Push(mCompilationUnits->mSectionZeroPage);

	if (regionBytecode)
		regionBytecode->mSections.Push(sectionBytecode);

	if (!mLinker->IsSectionPlaced(mCompilationUnits->mSectionCode))
	{
		if (!regionMain)
		{
			if (!(mCompilerOptions & COPT_TARGET_PRG))
				regionMain = mLinker->AddRegion(identMain, 0x0900, 0x4700);
			else if (regionBytecode)
				regionMain = mLinker->AddRegion(identMain, 0x0a00, 0xa000);
			else
				regionMain = mLinker->AddRegion(identMain, 0x0900, 0xa000);
		}

		regionMain->mSections.Push(mCompilationUnits->mSectionCode);
		regionMain->mSections.Push(mCompilationUnits->mSectionData);
		regionMain->mSections.Push(mCompilationUnits->mSectionBSS);
		regionMain->mSections.Push(mCompilationUnits->mSectionHeap);
		regionMain->mSections.Push(mCompilationUnits->mSectionStack);
	}

	dcrtstart->mSection = sectionStartup;

	mGlobalAnalyzer->mCompilerOptions = mCompilerOptions;

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Global analyzer\n");

	mGlobalAnalyzer->AnalyzeAssembler(dcrtstart->mValue, nullptr);

	for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
	{
		Declaration* dec = mCompilationUnits->mReferenced[i];
		if (dec->mType == DT_CONST_FUNCTION)
			mGlobalAnalyzer->AnalyzeProcedure(dec->mValue, dec);
		else
			mGlobalAnalyzer->AnalyzeGlobalVariable(dec);
	}

	mGlobalAnalyzer->CheckInterrupt();
	mGlobalAnalyzer->AutoInline();
	//mGlobalAnalyzer->DumpCallGraph();

	mInterCodeGenerator->mCompilerOptions = mCompilerOptions;
	mNativeCodeGenerator->mCompilerOptions = mCompilerOptions;
	mInterCodeModule->mCompilerOptions = mCompilerOptions;

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Generate intermediate code\n");

	mInterCodeGenerator->TranslateAssembler(mInterCodeModule, dcrtstart->mValue, nullptr);
	for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
	{
		Declaration* dec = mCompilationUnits->mReferenced[i];
		if (dec->mType == DT_CONST_FUNCTION)
		{
			if (!dec->mLinkerObject)
				mInterCodeGenerator->TranslateProcedure(mInterCodeModule, dec->mValue, dec);
		}
		else
		{
			if (!dec->mLinkerObject)
				mInterCodeGenerator->InitGlobalVariable(mInterCodeModule, dec);
		}
	}

	if (mErrors->mErrorCount != 0)
		return false;

	// Register native runtime functions

	if (mInterCodeModule->mProcedures.Size() > 0)
	{
		RegisterRuntime(loc, Ident::Unique("mul16by8"));
		RegisterRuntime(loc, Ident::Unique("fsplitt"));
		RegisterRuntime(loc, Ident::Unique("fsplitx"));
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
		RegisterRuntime(loc, Ident::Unique("jmpaddr"));
		RegisterRuntime(loc, Ident::Unique("mul32"));
		RegisterRuntime(loc, Ident::Unique("divs32"));
		RegisterRuntime(loc, Ident::Unique("mods32"));
		RegisterRuntime(loc, Ident::Unique("divu32"));
		RegisterRuntime(loc, Ident::Unique("modu32"));
	}

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
					mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue, nullptr);
				mByteCodeGenerator->mExtByteCodes[i] = bcdec->mLinkerObject;
			}
		}
	}

	if (mErrors->mErrorCount != 0)
		return false;

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Optimize static variable usage\n");

#if 1
	for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
	{
		mInterCodeModule->mProcedures[i]->MarkRelevantStatics();
	}

	for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
	{
		mInterCodeModule->mProcedures[i]->RemoveNonRelevantStatics();
	}
#endif

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Generate native code\n");

	for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
	{
		InterCodeProcedure* proc = mInterCodeModule->mProcedures[i];

//		proc->ReduceTemporaries();

#if _DEBUG
		proc->Disassemble("final");
#endif

		CompileProcedure(proc);

		if (proc->mLinkerObject->mStackSection)
			mCompilationUnits->mSectionStack->mSections.Push(proc->mLinkerObject->mStackSection);
	}

	LinkerObject* byteCodeObject = nullptr;
	if (!(mCompilerOptions & COPT_NATIVE))
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Generate byte code runtime\n");

		// Compile used byte code functions

		byteCodeObject = mLinker->AddObject(loc, Ident::Unique("bytecode"), sectionBytecode, LOT_RUNTIME);

		for (int i = 0; i < 128; i++)
		{
			if (mByteCodeGenerator->mByteCodeUsed[i] > 0)
			{
				Declaration* bcdec = mCompilationUnits->mByteCodes[i];
				if (bcdec)
				{
					LinkerObject* linkerObject = nullptr;

					int	offset = 0;
					if (bcdec->mType == DT_CONST_ASSEMBLER)
					{
						if (!bcdec->mLinkerObject)
							mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mValue, nullptr);
						linkerObject = bcdec->mLinkerObject;
					}
					else if (bcdec->mType == DT_LABEL)
					{
						if (!bcdec->mBase->mLinkerObject)
							mInterCodeGenerator->TranslateAssembler(mInterCodeModule, bcdec->mBase->mValue, nullptr);
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
	}

	mNativeCodeGenerator->CompleteRuntime();

	mLinker->CollectReferences();

	mLinker->ReferenceObject(dcrtstart->mLinkerObject);

	if (!(mCompilerOptions & COPT_NATIVE))
		mLinker->ReferenceObject(byteCodeObject);

	for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
		mLinker->ReferenceObject(mCompilationUnits->mReferenced[i]->mLinkerObject);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Link executable\n");

	mLinker->Link();

	return mErrors->mErrorCount == 0;
}

bool Compiler::BuildLZO(const char* targetPath)
{
	mPreprocessor->mCompilerOptions = mCompilerOptions;
	mLinker->mCompilerOptions = mCompilerOptions;

	CompilationUnit* cunit;

	char	data[65536];
	int		n = 0;

	while (mErrors->mErrorCount == 0 && (cunit = mCompilationUnits->PendingUnit()))
	{
		if (mPreprocessor->EmbedData("Compressing", cunit->mFileName, true, 0, 65536, SFM_BINARY_LZO, SFD_NONE))
		{
			Scanner* scanner = new Scanner(mErrors, mPreprocessor);
			while (scanner->mToken == TK_INTEGER)
			{
				data[n++] = scanner->mTokenInteger;
				do {
					scanner->NextToken();
				} while (scanner->mToken == TK_COMMA);				
			}
		}
		else
			mErrors->Error(cunit->mLocation, EERR_FILE_NOT_FOUND, "Could not open source file", cunit->mFileName);
	}

	if (mErrors->mErrorCount == 0)
	{
		char	prgPath[200];

		strcpy_s(prgPath, targetPath);
		int		i = strlen(prgPath);
		while (i > 0 && prgPath[i - 1] != '.')
			i--;
		if (i > 0)
			prgPath[i] = 0;

		strcat_s(prgPath, "lzo");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);

		FILE* file;
		fopen_s(&file, prgPath, "wb");
		if (file)
		{
			int	done = fwrite(data, 1, n, file);
			fclose(file);
			return done == n;
		}
		else
			return false;
	}
	else
		return false;

}

bool Compiler::WriteOutputFile(const char* targetPath, DiskImage * d64)
{
	char	prgPath[200], mapPath[200], asmPath[200], lblPath[200], intPath[200], bcsPath[200];

	strcpy_s(prgPath, targetPath);
	int		i = strlen(prgPath);
	while (i > 0 && prgPath[i - 1] != '.')
		i--;
	if (i > 0)
		prgPath[i] = 0;
	
	strcpy_s(mapPath, prgPath);
	strcpy_s(asmPath, prgPath);
	strcpy_s(lblPath, prgPath);
	strcpy_s(intPath, prgPath);
	strcpy_s(bcsPath, prgPath);

	strcat_s(mapPath, "map");
	strcat_s(asmPath, "asm");
	strcat_s(lblPath, "lbl");
	strcat_s(intPath, "int");
	strcat_s(bcsPath, "bcs");

	if (mCompilerOptions & COPT_TARGET_PRG)
	{
		strcat_s(prgPath, "prg");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WritePrgFile(prgPath);
	}
	else if (mCompilerOptions & COPT_TARGET_CRT16)
	{
		strcat_s(prgPath, "crt");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WriteCrtFile(prgPath);
	}
	else if (mCompilerOptions & COPT_TARGET_BIN)
	{
		strcat_s(prgPath, "bin");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WriteBinFile(prgPath);
	}


	if (d64)
	{
		int		i = strlen(prgPath);
		while (i > 0 && prgPath[i - 1] != '.')
			i--;
		if (i > 0)
			prgPath[i - 1] = 0;

		while (i > 0 && prgPath[i - 1] != '/' && prgPath[i - 1] != '\\')
			i--;

		d64->OpenFile(prgPath + i);
		mLinker->WritePrgFile(d64);
		d64->CloseFile();
	}

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", mapPath);
	mLinker->WriteMapFile(mapPath);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", asmPath);
	mLinker->WriteAsmFile(asmPath);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", lblPath);
	mLinker->WriteLblFile(lblPath);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", intPath);
	mInterCodeModule->Disassemble(intPath);

	if (!(mCompilerOptions & COPT_NATIVE))
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", bcsPath);
		mByteCodeGenerator->WriteByteCodeStats(bcsPath);
	}

	return true;
}

int Compiler::ExecuteCode(bool profile)
{
	Location	loc;

	printf("Running emulation...\n");
	Emulator* emu = new Emulator(mLinker);

	int ecode = 20;
	if (mCompilerOptions & COPT_TARGET_PRG)
	{
		memcpy(emu->mMemory + mLinker->mProgramStart, mLinker->mMemory + mLinker->mProgramStart, mLinker->mProgramEnd - mLinker->mProgramStart);
		emu->mMemory[0x2d] = mLinker->mProgramEnd & 0xff;
		emu->mMemory[0x2e] = mLinker->mProgramEnd >> 8;
		ecode = emu->Emulate(2061);
	}
	else if (mCompilerOptions & COPT_TARGET_CRT16)
	{
		memcpy(emu->mMemory + 0x8000, mLinker->mMemory + 0x0800, 0x4000);
		ecode = emu->Emulate(0x8009);
	}

	printf("Emulation result %d\n", ecode);

	if (profile)
		emu->DumpProfile();

	if (ecode != 0)
	{
		char	sd[20];
		sprintf_s(sd, "%d", ecode);
		mErrors->Error(loc, EERR_EXECUTION_FAILED, "Execution failed", sd);
	}

	return ecode;
}

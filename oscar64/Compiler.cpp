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
	mCompilationUnits->mSectionLowCode = nullptr;
	mCompilationUnits->mSectionBoot = nullptr;
	mCompilationUnits->mSectionStack->mSize = 4096;
	mCompilationUnits->mSectionHeap->mSize = 1024;

	mPreprocessor = new Preprocessor(mErrors);
	mByteCodeGenerator = new ByteCodeGenerator(mErrors, mLinker);
	mInterCodeGenerator = new InterCodeGenerator(mErrors, mLinker);
	mNativeCodeGenerator = new NativeCodeGenerator(mErrors, mLinker, mCompilationUnits->mSectionCode);
	mInterCodeModule = new InterCodeModule(mErrors, mLinker);
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
	if (mTargetMachine == TMACH_ATARI)
	{
		BC_REG_WORK_Y = 0x80;
		BC_REG_WORK = 0x81;
		BC_REG_FPARAMS = 0x8b;
		BC_REG_FPARAMS_END = 0x97;

		BC_REG_IP = 0x97;
		BC_REG_ACCU = 0x99;
		BC_REG_ADDR = 0x9d;
		BC_REG_STACK = 0xa1;
		BC_REG_LOCALS = 0xa3;

		BC_REG_TMP = 0xa5;
		BC_REG_TMP_SAVED = 0xc5;
	}
	else if (mCompilerOptions & COPT_EXTENDED_ZERO_PAGE)
	{
		BC_REG_FPARAMS = 0x0d;
		BC_REG_FPARAMS_END = 0x25;

		BC_REG_IP = 0x25;
		BC_REG_ACCU = 0x27;
		BC_REG_ADDR = 0x2b;
		BC_REG_STACK = 0x2f;
		BC_REG_LOCALS = 0x31;
#if 1
		BC_REG_TMP = 0x33;
		BC_REG_TMP_SAVED = 0x53;
#endif
	}

	switch (mTargetMachine)
	{
	case TMACH_VIC20:
	case TMACH_VIC20_3K:
	case TMACH_VIC20_8K:
	case TMACH_PET_8K:
		mCompilationUnits->mSectionStack->mSize = 512;
		mCompilationUnits->mSectionHeap->mSize = 512;
		break;
	case TMACH_PET_16K:
	case TMACH_VIC20_16K:
	case TMACH_VIC20_24K:
		mCompilationUnits->mSectionStack->mSize = 1024;
		mCompilationUnits->mSectionHeap->mSize = 1024;
		break;
	case TMACH_C128:
	case TMACH_PLUS4:
		mCompilationUnits->mSectionLowCode = mLinker->AddSection(Ident::Unique("lowcode"), LST_DATA);
		break;
	case TMACH_NES:
		mCompilationUnits->mSectionStack->mSize = 256;
		mCompilationUnits->mSectionHeap->mSize = 256;
		mCompilationUnits->mSectionBoot = mLinker->AddSection(Ident::Unique("boot"), LST_DATA);
		break;
	}

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
	const Ident* identRom = Ident::Unique("rom");
	const Ident* identBoot = Ident::Unique("boot");
	const Ident* identCode = Ident::Unique("code");
	const Ident* identZeroPage = Ident::Unique("zeropage");
	const Ident* identLowcode = Ident::Unique("lowcode");

	LinkerRegion* regionZeroPage = mLinker->FindRegion(identZeroPage);
	if (!regionZeroPage)
	{
		if (mTargetMachine == TMACH_ATARI)
			regionZeroPage = mLinker->AddRegion(identZeroPage, 0x00e0, 0x00ff);
		else
			regionZeroPage = mLinker->AddRegion(identZeroPage, 0x0080, 0x00ff);
	}

	LinkerRegion* regionStartup = mLinker->FindRegion(identStartup);
	LinkerRegion* regionLowcode = nullptr, * regionBoot = nullptr;

	if (!regionStartup)
	{
		if (mCompilerOptions & COPT_TARGET_PRG)
		{
			switch (mTargetMachine)
			{
			case TMACH_C64:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x0801, 0x0880);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x0801, 0x0900);
				break;
			case TMACH_C128:
				if (mCompilerOptions & COPT_NATIVE)
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1c80);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1c80, 0x1d00);
				}
				else
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1c80);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1c80, 0x1d00);
				}
				regionLowcode->mSections.Push(mCompilationUnits->mSectionLowCode);
				break;
			case TMACH_C128B:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1c80);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1d00);
				break;
			case TMACH_PLUS4:
				if (mCompilerOptions & COPT_NATIVE)
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1080);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1080, 0x1100);
				}
				else
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1080);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1080, 0x1000);
				}
				regionLowcode->mSections.Push(mCompilationUnits->mSectionLowCode);
				break;
			case TMACH_VIC20:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1080);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1100);
				break;
			case TMACH_VIC20_3K:
			case TMACH_PET_8K:
			case TMACH_PET_16K:
			case TMACH_PET_32K:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x0401, 0x0480);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x0401, 0x0500);
				break;
			case TMACH_VIC20_8K:
			case TMACH_VIC20_16K:
			case TMACH_VIC20_24K:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x1201, 0x1280);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x1201, 0x1300);
				break;
			case TMACH_ATARI:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x2000, 0x2080);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x2000, 0x2100);
				break;
			}
		}
		else if (mTargetMachine == TMACH_NES)
			regionStartup = mLinker->AddRegion(identStartup, 0x8000, 0x8080);
		else
			regionStartup = mLinker->AddRegion(identStartup, 0x0800, 0x0900);
	}

	LinkerRegion* regionBytecode = nullptr;
	if (!(mCompilerOptions & COPT_NATIVE))
	{
		regionBytecode = mLinker->FindRegion(identBytecode);
		if (!regionBytecode)
		{
			switch (mTargetMachine)
			{
			case TMACH_C64:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x0900, 0x0a00);
				break;
			case TMACH_C128:
			case TMACH_C128B:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x1d00, 0x1e00);
				break;
			case TMACH_PLUS4:
			case TMACH_VIC20:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x1100, 0x1200);
				break;
			case TMACH_VIC20_3K:
			case TMACH_PET_8K:
			case TMACH_PET_16K:
			case TMACH_PET_32K:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x0500, 0x0600);
				break;
			case TMACH_VIC20_8K:
			case TMACH_VIC20_16K:
			case TMACH_VIC20_24K:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x1300, 0x1400);
				break;
			case TMACH_ATARI:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x2100, 0x2200);
				break;
			}

		}
	}

	LinkerRegion* regionMain = mLinker->FindRegion(identMain);
	LinkerRegion* regionRom = mLinker->FindRegion(identRom);

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
			if (mTargetMachine == TMACH_NES)
			{
				regionBoot = mLinker->AddRegion(identBoot, 0xfffa, 0x10000);
				regionBoot->mSections.Push(mCompilationUnits->mSectionBoot);
				regionRom = mLinker->AddRegion(identRom, 0x8080, 0xfffa);
				regionMain = mLinker->AddRegion(identMain, 0x0200, 0x0800);
			}
			else if (!(mCompilerOptions & COPT_TARGET_PRG))
				regionMain = mLinker->AddRegion(identMain, 0x0900, 0x4700);
			else if (regionBytecode)
			{
				switch (mTargetMachine)
				{
				case TMACH_C64:
					regionMain = mLinker->AddRegion(identMain, 0x0a00, 0xa000);
					break;
				case TMACH_C128:
					regionMain = mLinker->AddRegion(identMain, 0x1e00, 0xfe00);
					break;
				case TMACH_C128B:
					regionMain = mLinker->AddRegion(identMain, 0x1e00, 0x4000);
					break;
				case TMACH_PLUS4:
					regionMain = mLinker->AddRegion(identMain, 0x1200, 0xfc00);
					break;
				case TMACH_VIC20:
					regionMain = mLinker->AddRegion(identMain, 0x1200, 0x1e00);
					break;
				case TMACH_VIC20_3K:
					regionMain = mLinker->AddRegion(identMain, 0x0600, 0x1e00);
					break;
				case TMACH_VIC20_8K:
					regionMain = mLinker->AddRegion(identMain, 0x1400, 0x4000);
					break;
				case TMACH_VIC20_16K:
					regionMain = mLinker->AddRegion(identMain, 0x1400, 0x6000);
					break;
				case TMACH_VIC20_24K:
					regionMain = mLinker->AddRegion(identMain, 0x1400, 0x8000);
					break;
				case TMACH_PET_8K:
					regionMain = mLinker->AddRegion(identMain, 0x0600, 0x2000);
					break;
				case TMACH_PET_16K:
					regionMain = mLinker->AddRegion(identMain, 0x0600, 0x4000);
					break;
				case TMACH_PET_32K:
					regionMain = mLinker->AddRegion(identMain, 0x0600, 0x8000);
					break;
				case TMACH_ATARI:
					regionMain = mLinker->AddRegion(identMain, 0x2200, 0xbc00);
					break;
				}
			}
			else
			{
				switch (mTargetMachine)
				{
				case TMACH_C64:
					regionMain = mLinker->AddRegion(identMain, 0x0880, 0xa000);
					break;
				case TMACH_C128:
					regionMain = mLinker->AddRegion(identMain, 0x1d00, 0xfe00);
					break;
				case TMACH_C128B:
					regionMain = mLinker->AddRegion(identMain, 0x1c80, 0x4000);
					break;
				case TMACH_PLUS4:
					regionMain = mLinker->AddRegion(identMain, 0x1100, 0xfc00);
					break;
				case TMACH_VIC20:
					regionMain = mLinker->AddRegion(identMain, 0x1080, 0x1e00);
					break;
				case TMACH_VIC20_3K:
					regionMain = mLinker->AddRegion(identMain, 0x0580, 0x1e00);
					break;
				case TMACH_VIC20_8K:
					regionMain = mLinker->AddRegion(identMain, 0x1280, 0x4000);
					break;
				case TMACH_VIC20_16K:
					regionMain = mLinker->AddRegion(identMain, 0x1280, 0x6000);
					break;
				case TMACH_VIC20_24K:
					regionMain = mLinker->AddRegion(identMain, 0x1280, 0x8000);
					break;
				case TMACH_PET_8K:
					regionMain = mLinker->AddRegion(identMain, 0x0480, 0x2000);
					break;
				case TMACH_PET_16K:
					regionMain = mLinker->AddRegion(identMain, 0x0480, 0x4000);
					break;
				case TMACH_PET_32K:
					regionMain = mLinker->AddRegion(identMain, 0x0480, 0x8000);
					break;
				case TMACH_ATARI:
					regionMain = mLinker->AddRegion(identMain, 0x2080, 0xbc00);
					break;
				}
			}
		}

		if (regionRom)
		{
			regionRom->mSections.Push(mCompilationUnits->mSectionCode);
			regionRom->mSections.Push(mCompilationUnits->mSectionData);
		}
		else
		{
			regionMain->mSections.Push(mCompilationUnits->mSectionCode);
			regionMain->mSections.Push(mCompilationUnits->mSectionData);
		}

		regionMain->mSections.Push(mCompilationUnits->mSectionBSS);
		regionMain->mSections.Push(mCompilationUnits->mSectionHeap);
		regionMain->mSections.Push(mCompilationUnits->mSectionStack);
	}

	mInterCodeModule->InitParamStack(mCompilationUnits->mSectionStack);

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
		RegisterRuntime(loc, Ident::Unique("ftou"));
		RegisterRuntime(loc, Ident::Unique("ffromi"));
		RegisterRuntime(loc, Ident::Unique("ffromu"));
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
		if (mTargetMachine == TMACH_ATARI)
		{
			strcat_s(prgPath, "xex");
			if (mCompilerOptions & COPT_VERBOSE)
				printf("Writing <%s>\n", prgPath);
			mLinker->WriteXexFile(prgPath);
		}
		else
		{
			strcat_s(prgPath, "prg");
			if (mCompilerOptions & COPT_VERBOSE)
				printf("Writing <%s>\n", prgPath);
			mLinker->WritePrgFile(prgPath);
		}
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
	else if (mCompilerOptions & COPT_TARGET_NES)
	{
		strcat_s(prgPath, "nes");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WriteNesFile(prgPath);
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

		mLinker->WritePrgFile(d64, prgPath + i);
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

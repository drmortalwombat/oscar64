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
	mGlobalOptimizer = new GlobalOptimizer(mErrors, mLinker);

	mCartridgeID = 0x0000;
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
	else if (mTargetMachine == TMACH_X16)
	{
		BC_REG_WORK_Y = 0x22;
		BC_REG_WORK = 0x23;
		BC_REG_FPARAMS = 0x2d;
		BC_REG_FPARAMS_END = 0x39;

		BC_REG_IP = 0x39;
		BC_REG_ACCU = 0x3b;
		BC_REG_ADDR = 0x3f;
		BC_REG_STACK = 0x43;
		BC_REG_LOCALS = 0x45;

		BC_REG_TMP = 0x47;
		BC_REG_TMP_SAVED = 0x67;
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
	case TMACH_X16:
		mCompilationUnits->mSectionStack->mSize = 1024;
		mCompilationUnits->mSectionHeap->mSize = 1024;
		break;
	case TMACH_C128:
	case TMACH_PLUS4:
		mCompilationUnits->mSectionLowCode = mLinker->AddSection(Ident::Unique("lowcode"), LST_DATA);
		break;
	case TMACH_NES:
	case TMACH_NES_NROM_H:
	case TMACH_NES_NROM_V:
	case TMACH_NES_MMC1:
	case TMACH_NES_MMC3:
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

			scanner->mCompilerOptions = mCompilerOptions;

			scanner->NextToken();

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
			offset = int(bcdec->mInteger);
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

static void IndexVTableTree(Declaration* vdec, int & index)
{
	vdec->mVarIndex = index;
	vdec->mDefaultConstructor->mInteger = index;
	index++;
	Declaration* cvdec = vdec->mParams;
	while (cvdec)
	{
		IndexVTableTree(cvdec, index);
		cvdec = cvdec->mNext;
	}
	vdec->mSize = index - vdec->mVarIndex;
}

static void FillVTableTree(Declaration* vdec)
{
	Declaration* cdec = vdec->mClass;
	cdec->mScope->Iterate([=](const Ident* mident, Declaration* mdec)
		{
			if (mdec->mType == DT_CONST_FUNCTION)
			{
				while (mdec)
				{
					if (mdec->mBase->mFlags & DTF_VIRTUAL)
					{
						for (int i = 0; i < vdec->mSize; i++)
							mdec->mVTable->mCalled[vdec->mVarIndex + i - mdec->mVTable->mDefaultConstructor->mVarIndex] = mdec;
					}
					mdec = mdec->mNext;
				}
			}
		});

	Declaration* cvdec = vdec->mParams;
	while (cvdec)
	{
		FillVTableTree(cvdec);
		cvdec = cvdec->mNext;
	}
}
void Compiler::CompleteTemplateExpansion(void)
{
}

bool IsSimpleConstReturn(Declaration * mdec)
{
	if (mdec->mBase->mBase->IsSimpleType() && mdec->mBase->mParams->mNext == nullptr)
	{
		Expression* ex = mdec->mValue;
		if (ex->mType == EX_SCOPE)
			ex = ex->mLeft;

		if (ex->mType == EX_RETURN && ex->mLeft->mType == EX_CONSTANT)
			return true;
	}

	return false;
}

void Compiler::BuildVTables(void)
{
	// Connect vdecs with parents
	mCompilationUnits->mVTableScope->Iterate([=](const Ident* ident, Declaration* vdec)
		{
			if (vdec->mBase)
			{
				vdec->mNext = vdec->mBase->mParams;
				vdec->mBase->mParams = vdec;
			}
		});

	// Number the child vtables
	mCompilationUnits->mVTableScope->Iterate([=](const Ident* ident, Declaration* vdec)
		{
			if (!vdec->mBase)
			{
				int index = 0;
				IndexVTableTree(vdec, index);
			}
		});

	mCompilationUnits->mVTableScope->Iterate([=](const Ident* ident, Declaration* vdec)
		{
			if (!vdec->mBase)
			{
				FillVTableTree(vdec);
			}
		});

	// Build vtables for functions
	mCompilationUnits->mVTableScope->Iterate([=](const Ident* ident, Declaration* vdec)
		{
			vdec->mScope->Iterate([=](const Ident* mident, Declaration* mdec)
				{					
					bool	simpleConst = vdec->mSize > 0;
					for (int i = 0; i < vdec->mSize; i++)
						if (!IsSimpleConstReturn(mdec->mCalled[i]))
							simpleConst = false;

					if (simpleConst)
					{
						Declaration* vtabt = new Declaration(mdec->mLocation, DT_TYPE_ARRAY);
						vtabt->mBase = mdec->mBase->mBase->ToConstType()->ToStriped(vdec->mSize);
						vtabt->mSize = vdec->mSize * mdec->mBase->mBase->mSize;
						vtabt->mStride = 1;
						vtabt->mStripe = 1;
						vtabt->mFlags |= DTF_CONST | DTF_DEFINED;

						Declaration* vtaba = new Declaration(mdec->mLocation, DT_VARIABLE);
						vtaba->mFlags = DTF_CONST | DTF_GLOBAL | DTF_DEFINED;
						vtaba->mBase = vtabt;
						vtaba->mSize = vtabt->mSize;
						vtaba->mValue = new Expression(mdec->mLocation, EX_CONSTANT);
						vtaba->mValue->mDecType = vtabt;
						vtaba->mValue->mDecValue = new Declaration(mdec->mLocation, DT_CONST_STRUCT);
						vtaba->mIdent = mdec->mIdent;
						vtaba->mQualIdent = mdec->mQualIdent->Mangle("$vltable");
						vtaba->mSection = mdec->mSection;
						vtaba->mOffset = -vdec->mVarIndex;

						Declaration* last = nullptr;

						for (int i = 0; i < vdec->mSize; i++)
						{
							Declaration* vmdec = mdec->mCalled[i];

							Expression* texp = vmdec->mValue;
							if (texp->mType == EX_SCOPE)
								texp = texp->mLeft;
							texp = texp->mLeft;
							
							Declaration* cdec = texp->mDecValue->Clone();
							cdec->mOffset = i;

							if (last)
								last->mNext = cdec;
							else
								vtaba->mValue->mDecValue->mParams = cdec;
							last = cdec;
						}

						Expression* vexp = new Expression(mdec->mLocation, EX_QUALIFY);
						vexp->mLeft = new Expression(mdec->mLocation, EX_PREFIX);
						vexp->mLeft->mDecType = mdec->mBase->mParams->mBase->mBase;
						vexp->mLeft->mToken = TK_MUL;
						vexp->mLeft->mLeft = new Expression(mdec->mLocation, EX_VARIABLE);
						vexp->mLeft->mLeft->mDecType = mdec->mBase->mParams->mBase;
						vexp->mLeft->mLeft->mDecValue = mdec->mBase->mParams;

						vexp->mDecValue = new Declaration(mdec->mLocation, DT_ELEMENT);
						vexp->mDecValue->mBase = TheCharTypeDeclaration;
						vexp->mDecValue->mOffset = vdec->mOffset;
						vexp->mDecValue->mSize = 1;
						vexp->mDecType = TheCharTypeDeclaration;

						Expression* ecall = new Expression(mdec->mLocation, EX_RETURN);
						ecall->mLeft = new Expression(mdec->mLocation, EX_INDEX);
						ecall->mLeft->mDecType = mdec->mBase->mBase;
						ecall->mDecType = mdec->mBase->mBase;
						ecall->mLeft->mLeft = new Expression(mdec->mLocation, EX_VARIABLE);
						ecall->mLeft->mLeft->mDecType = vtabt;
						ecall->mLeft->mLeft->mDecValue = vtaba;
						ecall->mLeft->mRight = vexp;

						mdec->mCalled.SetSize(0);

						mdec->mFlags |= DTF_DEFINED;
						mdec->mBase->mFlags &= ~DTF_VIRTUAL;
						mdec->mValue = ecall;
					}
					else
					{
						Declaration* vtabt = new Declaration(mdec->mLocation, DT_TYPE_ARRAY);
						vtabt->mBase = mdec->mBase->ToStriped(vdec->mSize);
						vtabt->mSize = vdec->mSize * 2;
						vtabt->mStride = 1;
						vtabt->mStripe = 1;
						vtabt->mFlags |= DTF_CONST | DTF_DEFINED;

						Declaration* vtaba = new Declaration(mdec->mLocation, DT_VARIABLE);
						vtaba->mFlags = DTF_CONST | DTF_GLOBAL | DTF_DEFINED;
						vtaba->mBase = vtabt;
						vtaba->mSize = vtabt->mSize;
						vtaba->mValue = new Expression(mdec->mLocation, EX_CONSTANT);
						vtaba->mValue->mDecType = vtabt;
						vtaba->mValue->mDecValue = new Declaration(mdec->mLocation, DT_CONST_STRUCT);
						vtaba->mIdent = mdec->mIdent;
						vtaba->mQualIdent = mdec->mQualIdent->Mangle("$vtable");
						vtaba->mSection = mdec->mSection;
						vtaba->mOffset = -vdec->mVarIndex;

						Declaration* last = nullptr;

						for (int i = 0; i < vdec->mSize; i++)
						{
							Declaration* vmdec = mdec->mCalled[i];

							Expression* texp = new Expression(vmdec->mLocation, EX_CONSTANT);
							texp->mDecType = vtabt->mBase;
							texp->mDecValue = vmdec;

							Declaration* cdec = new Declaration(vmdec->mLocation, DT_CONST_POINTER);
							cdec->mValue = texp;
							cdec->mBase = vtabt->mBase;
							cdec->mOffset = i;

							if (last)
								last->mNext = cdec;
							else
								vtaba->mValue->mDecValue->mParams = cdec;
							last = cdec;
						}

						//					mCompilationUnits->AddReferenced(vtaba);

						Expression* vexp = new Expression(mdec->mLocation, EX_QUALIFY);
						vexp->mLeft = new Expression(mdec->mLocation, EX_PREFIX);
						vexp->mLeft->mDecType = mdec->mBase->mParams->mBase->mBase;
						vexp->mLeft->mToken = TK_MUL;
						vexp->mLeft->mLeft = new Expression(mdec->mLocation, EX_VARIABLE);
						vexp->mLeft->mLeft->mDecType = mdec->mBase->mParams->mBase;
						vexp->mLeft->mLeft->mDecValue = mdec->mBase->mParams;

						vexp->mDecValue = new Declaration(mdec->mLocation, DT_ELEMENT);
						vexp->mDecValue->mBase = TheCharTypeDeclaration;
						vexp->mDecValue->mOffset = vdec->mOffset;
						vexp->mDecValue->mSize = 1;
						vexp->mDecType = TheCharTypeDeclaration;

						Expression* ecall = new Expression(mdec->mLocation, EX_DISPATCH);
						ecall->mLeft = new Expression(mdec->mLocation, EX_INDEX);
						ecall->mLeft->mDecType = mdec->mBase;
						ecall->mDecType = vtabt->mBase;
						ecall->mLeft->mLeft = new Expression(mdec->mLocation, EX_VARIABLE);
						ecall->mLeft->mLeft->mDecType = vtabt;
						ecall->mLeft->mLeft->mDecValue = vtaba;
						ecall->mLeft->mRight = vexp;

						mdec->mFlags |= DTF_DEFINED;
						mdec->mValue = ecall;
					}

					mdec->mQualIdent = mdec->mQualIdent->Mangle("$vcall");
				});
		});
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
			mNativeProcedures.Push(ncproc);
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
		else if (mCompilerOptions & (COPT_EXTENDED_ZERO_PAGE | COPT_TARGET_NES))
			regionZeroPage = mLinker->AddRegion(identZeroPage, 0x0080, 0x00ff);
		else
			regionZeroPage = mLinker->AddRegion(identZeroPage, 0x00f7, 0x00ff);
	}

	LinkerRegion* regionStartup = mLinker->FindRegion(identStartup);
	LinkerRegion* regionLowcode = nullptr, * regionBoot = nullptr;

	if (!regionStartup)
	{
		if (mCompilerOptions & (COPT_TARGET_PRG | COPT_TARGET_NES))
		{
			switch (mTargetMachine)
			{
			case TMACH_C64:
			case TMACH_X16:
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
			case TMACH_C128E:
				if (mCompilerOptions & COPT_NATIVE)
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1c80);
				else
					regionStartup = mLinker->AddRegion(identStartup, 0x1c01, 0x1d00);
				break;
			case TMACH_PLUS4:
				if (mCompilerOptions & COPT_NATIVE)
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1080);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1080, 0x1180);
				}
				else
				{
					regionStartup = mLinker->AddRegion(identStartup, 0x1001, 0x1080);
					regionLowcode = mLinker->AddRegion(identLowcode, 0x1080, 0x1180);
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
			case TMACH_NES:
			case TMACH_NES_NROM_H:
			case TMACH_NES_NROM_V:
				regionStartup = mLinker->AddRegion(identStartup, 0xff80, 0xfffa);
				regionStartup->mCartridgeBanks = 1;
				break;
			case TMACH_NES_MMC1:
				regionStartup = mLinker->AddRegion(identStartup, 0xff80, 0xfffa);
				regionStartup->mCartridgeBanks = 1ULL << 15;
				break;
			case TMACH_NES_MMC3:
				regionStartup = mLinker->AddRegion(identStartup, 0xff80, 0xfffa);
				regionStartup->mCartridgeBanks = 1ULL << 31;
				break;
			}
		}
		else if (mCompilerOptions & (COPT_TARGET_CRT8 | COPT_TARGET_CRT16))
		{
			regionStartup = mLinker->AddRegion(identStartup, 0x8000, 0x8080);
			regionStartup->mCartridgeBanks = 1;
		}
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
			case TMACH_X16:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x0900, 0x0a00);
				break;
			case TMACH_C128:
			case TMACH_C128B:
			case TMACH_C128E:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x1d00, 0x1e00);
				break;
			case TMACH_PLUS4:
				regionBytecode = mLinker->AddRegion(identBytecode, 0x1200, 0x1300);
				break;
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
			if (!(mCompilerOptions & (COPT_TARGET_PRG | COPT_TARGET_NES)))
				regionMain = mLinker->AddRegion(identMain, 0x0900, 0x4700);
			else if (regionBytecode)
			{
				switch (mTargetMachine)
				{
				case TMACH_C64:
					regionMain = mLinker->AddRegion(identMain, 0x0a00, 0xa000);
					break;
				case TMACH_X16:
					regionMain = mLinker->AddRegion(identMain, 0x0a00, 0x9f00);
					break;
				case TMACH_C128:
					regionMain = mLinker->AddRegion(identMain, 0x1e00, 0xfe00);
					break;
				case TMACH_C128B:
					regionMain = mLinker->AddRegion(identMain, 0x1e00, 0x4000);
					break;
				case TMACH_C128E:
					regionMain = mLinker->AddRegion(identMain, 0x1e00, 0xc000);
					break;
				case TMACH_PLUS4:
					regionMain = mLinker->AddRegion(identMain, 0x1300, 0xfc00);
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

					if (mCompilerOptions & (COPT_TARGET_CRT8 | COPT_TARGET_CRT16))
						regionMain = mLinker->AddRegion(identMain, 0x0800, 0x8000);
					else
						regionMain = mLinker->AddRegion(identMain, 0x0880, 0xa000);
					break;
				case TMACH_X16:
					regionMain = mLinker->AddRegion(identMain, 0x0880, 0x9f00);
					break;
				case TMACH_C128:
					regionMain = mLinker->AddRegion(identMain, 0x1d00, 0xfe00);
					break;
				case TMACH_C128B:
					regionMain = mLinker->AddRegion(identMain, 0x1c80, 0x4000);
					break;
				case TMACH_C128E:
					regionMain = mLinker->AddRegion(identMain, 0x1c80, 0xc000);
					break;
				case TMACH_PLUS4:
					regionMain = mLinker->AddRegion(identMain, 0x1180, 0xfc00);
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
				case TMACH_NES:
				case TMACH_NES_NROM_H:
				case TMACH_NES_NROM_V:
					regionBoot = mLinker->AddRegion(identBoot, 0xfffa, 0x10000);
					regionBoot->mCartridgeBanks = 1;
					regionBoot->mSections.Push(mCompilationUnits->mSectionBoot);
					regionRom = mLinker->AddRegion(identRom, 0x8000, 0xff80);
					regionRom->mCartridgeBanks = 1;
					regionMain = mLinker->AddRegion(identMain, 0x0200, 0x0800);
					break;
				case TMACH_NES_MMC1:
					regionBoot = mLinker->AddRegion(identBoot, 0xfffa, 0x10000);
					regionBoot->mCartridgeBanks = 1ULL << 15;
					regionBoot->mSections.Push(mCompilationUnits->mSectionBoot);
					regionRom = mLinker->AddRegion(identRom, 0xc000, 0xff80);
					regionRom->mCartridgeBanks = 1ULL << 15;
					regionMain = mLinker->AddRegion(identMain, 0x0200, 0x0800);
					break;
				case TMACH_NES_MMC3:
					regionBoot = mLinker->AddRegion(identBoot, 0xfffa, 0x10000);
					regionBoot->mCartridgeBanks = 1ULL << 31;
					regionBoot->mSections.Push(mCompilationUnits->mSectionBoot);
					regionRom = mLinker->AddRegion(identRom, 0xc000, 0xff80);
					regionRom->mCartridgeBanks = 1ULL << 31;
					regionMain = mLinker->AddRegion(identMain, 0x0200, 0x0800);
					break;
				}
			}
		}

		if (!regionRom)
		{
			if (mCompilerOptions & COPT_TARGET_CRT8)
			{
				regionRom = mLinker->AddRegion(identRom, 0x8080, 0xa000);
				regionRom->mCartridgeBanks = 1;
			}
			else if (mCompilerOptions & COPT_TARGET_CRT16)
			{
				regionRom = mLinker->AddRegion(identRom, 0x8080, 0xc000);
				regionRom->mCartridgeBanks = 1;
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

	if (mCompilerOptions & COPT_CPLUSPLUS)
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Build VTables\n");

		BuildVTables();
	}

	if (mCompilerOptions & COPT_OPTIMIZE_GLOBAL)
	{
		mGlobalOptimizer->mCompilerOptions = mCompilerOptions;

		if (mCompilerOptions & COPT_VERBOSE)
			printf("Global optimizer\n");

		do {
			mGlobalOptimizer->Reset();

			mGlobalOptimizer->AnalyzeAssembler(dcrtstart->mValue, nullptr);

			for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
			{
				Declaration* dec = mCompilationUnits->mReferenced[i];
				if (dec->mType == DT_CONST_FUNCTION)
					mGlobalOptimizer->AnalyzeProcedure(dec->mValue, dec);
				else
					mGlobalOptimizer->AnalyzeGlobalVariable(dec);
			}
		} while (mGlobalOptimizer->Optimize());

	}

	mGlobalAnalyzer->mCompilerOptions = mCompilerOptions;

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Global analyzer\n");

	mGlobalAnalyzer->AnalyzeAssembler(dcrtstart->mValue, nullptr);

	for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
	{
		Declaration* dec = mCompilationUnits->mReferenced[i];
		if (dec->mType == DT_CONST_FUNCTION)
			mGlobalAnalyzer->AnalyzeProcedure(nullptr, dec->mValue, dec);
		else
			mGlobalAnalyzer->AnalyzeGlobalVariable(dec);
	}

	mGlobalAnalyzer->CheckInterrupt();
	mGlobalAnalyzer->MarkRecursions();
	mGlobalAnalyzer->AutoInline();
	mGlobalAnalyzer->AutoZeroPage(mCompilationUnits->mSectionZeroPage, regionZeroPage->mEnd - regionZeroPage->mStart);
	if (mCompilerOptions & COPT_VERBOSE3)
		mGlobalAnalyzer->DumpCallGraph();

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
	}
	for (int i = 0; i < mCompilationUnits->mReferenced.Size(); i++)
	{
		Declaration* dec = mCompilationUnits->mReferenced[i];
		if (dec->mType != DT_CONST_FUNCTION)
		{
			if (!dec->mLinkerObject)
				mInterCodeGenerator->InitGlobalVariable(mInterCodeModule, dec);
		}
	}

	mInterCodeGenerator->CompleteMainInit();

	if (mErrors->mErrorCount != 0)
		return false;

	// Register native runtime functions

	if (mInterCodeModule->mProcedures.Size() > 0)
	{
		RegisterRuntime(loc, Ident::Unique("mul16by8"));
		RegisterRuntime(loc, Ident::Unique("fsplitt"));
		RegisterRuntime(loc, Ident::Unique("fsplitx"));
		RegisterRuntime(loc, Ident::Unique("fsplita"));
		RegisterRuntime(loc, Ident::Unique("fadd"));
		RegisterRuntime(loc, Ident::Unique("fsub"));
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
		RegisterRuntime(loc, Ident::Unique("ftoli"));
		RegisterRuntime(loc, Ident::Unique("ftolu"));
		RegisterRuntime(loc, Ident::Unique("ffromi"));
		RegisterRuntime(loc, Ident::Unique("ffromu"));
		RegisterRuntime(loc, Ident::Unique("ffromli"));
		RegisterRuntime(loc, Ident::Unique("ffromlu"));
		RegisterRuntime(loc, Ident::Unique("fcmp"));
		RegisterRuntime(loc, Ident::Unique("bcexec"));
		RegisterRuntime(loc, Ident::Unique("jmpaddr"));
		RegisterRuntime(loc, Ident::Unique("mul32"));
		RegisterRuntime(loc, Ident::Unique("divs32"));
		RegisterRuntime(loc, Ident::Unique("mods32"));
		RegisterRuntime(loc, Ident::Unique("divu32"));
		RegisterRuntime(loc, Ident::Unique("modu32"));

		RegisterRuntime(loc, Ident::Unique("store32"));
		RegisterRuntime(loc, Ident::Unique("load32"));

		RegisterRuntime(loc, Ident::Unique("malloc"));
		RegisterRuntime(loc, Ident::Unique("free"));
		RegisterRuntime(loc, Ident::Unique("breakpoint"));
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

	mNativeCodeGenerator->BuildFunctionProxies();

	for (int i = 0; i < mNativeProcedures.Size(); i++)
	{
		if (mCompilerOptions & COPT_VERBOSE2)
			printf("Assemble native code <%s>\n", mNativeProcedures[i]->mInterProc->mIdent->mString);
		mNativeProcedures[i]->Assemble();
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
						offset = int(bcdec->mInteger);
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

	if (mCompilerOptions & COPT_OPTIMIZE_BASIC)
	{
		mLinker->CombineSameConst();
		mLinker->InlineSimpleJumps();
		mLinker->CheckDirectJumps();
	}

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

	char	*	data = new char[65536];
	int		n = 0;

	while (mErrors->mErrorCount == 0 && (cunit = mCompilationUnits->PendingUnit()))
	{
		if (mPreprocessor->EmbedData("Compressing", cunit->mFileName, true, 0, 65536, SFM_BINARY_LZO, SFD_NONE))
		{
			Scanner* scanner = new Scanner(mErrors, mPreprocessor);
			while (scanner->mToken == TK_INTEGER)
			{
				if (scanner->mTokenInteger < 0 || scanner->mTokenInteger > 255)
					mErrors->Error(scanner->mLocation, EWARN_CONSTANT_TRUNCATED, "Constant integer truncated");
				data[n++] = uint8(scanner->mTokenInteger);
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
		ptrdiff_t		i = strlen(prgPath);
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
			ptrdiff_t	done = fwrite(data, 1, n, file);
			fclose(file);
			delete[] data;
			return done == n;
		}
		else
		{
			delete[] data;
			return false;
		}
	}
	else
	{
		delete[] data;
		return false;
	}
}

bool Compiler::RemoveErrorFile(const char* targetPath)
{
	char	prgPath[200], mapPath[200], asmPath[200], intPath[200];
	char	basePath[200];

	strcpy_s(basePath, targetPath);
	ptrdiff_t	i = strlen(basePath);
	while (i > 0 && basePath[i - 1] != '/' && basePath[i - 1] != '\\' && basePath[i - 1] != ':')
		i--;
	if (i > 0)
		basePath[i] = 0;

	strcpy_s(prgPath, targetPath);
	i = strlen(prgPath);
	while (i > 0 && prgPath[i - 1] != '.')
		i--;
	if (i > 0)
		prgPath[i] = 0;

	strcpy_s(mapPath, prgPath);
	strcpy_s(asmPath, prgPath);
	strcpy_s(intPath, prgPath);

	strcat_s(mapPath, "error.map");
	strcat_s(asmPath, "error.asm");
	strcat_s(intPath, "error.int");

	remove(mapPath);
	remove(asmPath);
	remove(intPath);

	return true;
}

bool Compiler::WriteErrorFile(const char* targetPath)
{
	char	prgPath[200], mapPath[200], asmPath[200], intPath[200];
	char	basePath[200];

	strcpy_s(basePath, targetPath);
	ptrdiff_t	i = strlen(basePath);
	while (i > 0 && basePath[i - 1] != '/' && basePath[i - 1] != '\\' && basePath[i - 1] != ':')
		i--;
	if (i > 0)
		basePath[i] = 0;

	strcpy_s(prgPath, targetPath);
	i = strlen(prgPath);
	while (i > 0 && prgPath[i - 1] != '.')
		i--;
	if (i > 0)
		prgPath[i] = 0;

	strcpy_s(mapPath, prgPath);
	strcpy_s(asmPath, prgPath);
	strcpy_s(intPath, prgPath);

	strcat_s(mapPath, "error.map");
	strcat_s(asmPath, "error.asm");
	strcat_s(intPath, "error.int");

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", mapPath);
	mLinker->WriteMapFile(mapPath);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", asmPath);
	mLinker->WriteAsmFile(asmPath, mVersion);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", intPath);
	mInterCodeModule->Disassemble(intPath);

	return true;
}

bool Compiler::WriteOutputFile(const char* targetPath, DiskImage * d64)
{
	char	prgPath[200], mapPath[200], asmPath[200], lblPath[200], intPath[200], bcsPath[200], dbjPath[200];
	char	basePath[200];

	strcpy_s(basePath, targetPath);
	ptrdiff_t	i = strlen(basePath);
	while (i > 0 && basePath[i - 1] != '/' && basePath[i - 1] != '\\' && basePath[i - 1] != ':')
		i--;
	if (i > 0)
		basePath[i] = 0;

	strcpy_s(prgPath, targetPath);
	i = strlen(prgPath);
	while (i > 0 && prgPath[i - 1] != '.')
		i--;
	if (i > 0)
		prgPath[i] = 0;
	
	strcpy_s(mapPath, prgPath);
	strcpy_s(asmPath, prgPath);
	strcpy_s(lblPath, prgPath);
	strcpy_s(intPath, prgPath);
	strcpy_s(bcsPath, prgPath);
	strcpy_s(dbjPath, prgPath);

	strcat_s(mapPath, "map");
	strcat_s(asmPath, "asm");
	strcat_s(lblPath, "lbl");
	strcat_s(intPath, "int");
	strcat_s(bcsPath, "bcs");
	strcat_s(dbjPath, "dbj");

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
			mLinker->WritePrgFile(prgPath, basePath);
		}
	}
	else if (mCompilerOptions & COPT_TARGET_CRT)
	{
		strcat_s(prgPath, "crt");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WriteCrtFile(prgPath, mCartridgeID);
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
		strcpy_s(lblPath, prgPath);
		strcat_s(lblPath, "mlb");

		strcat_s(prgPath, "nes");
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", prgPath);
		mLinker->WriteNesFile(prgPath, mTargetMachine);

	}


	if (d64)
	{
		ptrdiff_t	i = strlen(prgPath);
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
	mLinker->WriteAsmFile(asmPath, mVersion);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", lblPath);

	if (mCompilerOptions & COPT_TARGET_NES)
		mLinker->WriteMlbFile(lblPath, mTargetMachine);
	else
		mLinker->WriteLblFile(lblPath);

	if (mCompilerOptions & COPT_VERBOSE)
		printf("Writing <%s>\n", intPath);
	mInterCodeModule->Disassemble(intPath);

	if (mCompilerOptions & COPT_DEBUGINFO)
		WriteDbjFile(dbjPath);

	if (!(mCompilerOptions & COPT_NATIVE))
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("Writing <%s>\n", bcsPath);
		mByteCodeGenerator->WriteByteCodeStats(bcsPath);
	}

	return true;
}

int Compiler::ExecuteCode(bool profile, int trace)
{
	Location	loc;

	printf("Running emulation...\n");
	Emulator* emu = new Emulator(mLinker);

	if (mCompilerOptions & COPT_EXTENDED_ZERO_PAGE)
		emu->mJiffies = false;

	int ecode = 20;
	if (mCompilerOptions & COPT_TARGET_PRG)
	{
		memcpy(emu->mMemory + mLinker->mProgramStart, mLinker->mMemory + mLinker->mProgramStart, mLinker->mProgramEnd - mLinker->mProgramStart);
		emu->mMemory[0x2d] = mLinker->mProgramEnd & 0xff;
		emu->mMemory[0x2e] = mLinker->mProgramEnd >> 8;
		ecode = emu->Emulate(2061, trace);
	}
	else if (mCompilerOptions & COPT_TARGET_CRT)
	{
		memcpy(emu->mMemory + 0x8000, mLinker->mMemory + 0x0800, 0x4000);
		ecode = emu->Emulate(0x8009, trace);
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

static void DumpReferences(FILE* file, Declaration* dec)
{
	if (dec)
	{
		fprintf(file, ", \"references\": [");
		fprintf(file, "\n\t\t\t{\"source\": \"%s\", \"line\": %d, \"column\": %d}", dec->mLocation.mFileName, dec->mLocation.mLine, dec->mLocation.mColumn);
		for (int i = 0; i < dec->mReferences.Size(); i++)
		{
			fprintf(file, ",");
			Expression* exp = dec->mReferences[i];
			fprintf(file, "\n\t\t\t{\"source\": \"%s\", \"line\": %d, \"column\": %d}", exp->mLocation.mFileName, exp->mLocation.mLine, exp->mLocation.mColumn);
		}
		fprintf(file, "]");
	}

}

bool Compiler::WriteDbjFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		fprintf(file, "{");
		mLinker->WriteDbjFile(file);
		fprintf(file, ",\n");

		ExpandingArray<Declaration*>	types;

		fprintf(file, "\t\"variables\": [\n");
		bool	first = true;
		for (int i = 0; i < mInterCodeModule->mGlobalVars.Size(); i++)
		{
			InterVariable* v(mInterCodeModule->mGlobalVars[i]);
			if (v->mLinkerObject && v->mIdent && v->mDeclaration)
			{
				if (v->mLinkerObject->mSection->mType != LST_STATIC_STACK && v->mLinkerObject->mFlags & LOBJF_PLACED)
				{
					if (!first)
						fprintf(file, ",\n");
					first = false;

					fprintf(file, "\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"typeid\": %d", v->mIdent->mString, v->mLinkerObject->mAddress, v->mLinkerObject->mAddress + v->mLinkerObject->mSize, types.IndexOrPush(v->mDeclaration->mBase));
					DumpReferences(file, v->mDeclaration);
					fprintf(file, "}");
				}
			}
		}
		fprintf(file, "\t],\n");

		fprintf(file, "\t\"functions\": [\n");
		first = true;
		for (int i = 0; i < mInterCodeModule->mProcedures.Size(); i++)
		{
			InterCodeProcedure* p(mInterCodeModule->mProcedures[i]);
			if (p->mLinkerObject && p->mIdent && p->mDeclaration)
			{
				if (!first)
					fprintf(file, ",\n");
				first = false;

				fprintf(file, "\t\t{\"name\": \"%s\", \"xname\": \"%s\", \"start\": %d, \"end\": %d, \"typeid\": %d, \"source\": \"%s\", \"line\": %d, \"lines\": [\n", 
					p->mIdent->mString, p->mLinkerObject->mFullIdent->mString, p->mLinkerObject->mAddress, p->mLinkerObject->mAddress + p->mLinkerObject->mSize, types.IndexOrPush(p->mDeclaration->mBase),
					p->mLocation.mFileName, p->mLocation.mLine);

				bool lfirst = true;
				LinkerObject* lo = p->mLinkerObject;

				for (int j = 0; j < lo->mCodeLocations.Size(); j++)
				{
					if (!lfirst)
						fprintf(file, ",\n");
					lfirst = false;

					fprintf(file, "\t\t\t{\"start\": %d, \"end\": %d, \"source\": \"%s\", \"line\": %d}",
						lo->mCodeLocations[j].mStart + lo->mAddress,
						lo->mCodeLocations[j].mEnd + lo->mAddress,
						lo->mCodeLocations[j].mLocation.mFileName,
						lo->mCodeLocations[j].mLocation.mLine);
				}

				fprintf(file, "], \n\t\t\t\"variables\":[\n");

				bool	vfirst = true;
				for (int i = 0; i < p->mParamVars.Size(); i++)
				{
					InterVariable* v(p->mParamVars[i]);
					if (v && v->mIdent)
					{
						if (!vfirst)
							fprintf(file, ",\n");
						vfirst = false;

						if (p->mFastCallProcedure)
							fprintf(file, "\t\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"typeid\": %d}", v->mIdent->mString, i + BC_REG_FPARAMS, i + BC_REG_FPARAMS + v->mSize, types.IndexOrPush(v->mDeclaration->mBase));
						else if (p->mFramePointer)
							fprintf(file, "\t\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"base\": %d, \"typeid\": %d}", v->mIdent->mString, v->mOffset, v->mOffset + v->mSize, BC_REG_LOCALS, types.IndexOrPush(v->mDeclaration->mBase));
						else
							fprintf(file, "\t\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"base\": %d, \"typeid\": %d}", v->mIdent->mString, v->mOffset, v->mOffset + v->mSize, BC_REG_STACK, types.IndexOrPush(v->mDeclaration->mBase));
					}
				}
				for (int i = 0; i < p->mLocalVars.Size(); i++)
				{
					InterVariable* v(p->mLocalVars[i]);
					if (v && v->mIdent)
					{
						bool	skipped = false;
						if (v->mLinkerObject)
						{
							if (v->mLinkerObject->mFlags & LOBJF_PLACED)
							{
								if (!vfirst)
									fprintf(file, ",\n");
								vfirst = false;

								fprintf(file, "\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"enter\": %d, \"leave\": %d, \"typeid\": %d", 
									v->mIdent->mString, v->mLinkerObject->mAddress, v->mLinkerObject->mAddress + v->mLinkerObject->mSize, 
									v->mDeclaration->mLocation.mLine, v->mDeclaration->mEndLocation.mLine,
									types.IndexOrPush(v->mDeclaration->mBase));
							}
							else
							{
								skipped = true;
								// Prepared space on the stack but not used
							}
						}
						else if (v->mTemp)
						{
							skipped = true;
							// Promoted to local variable
						}
						else if (p->mFramePointer)
						{
							if (!vfirst)
								fprintf(file, ",\n");
							vfirst = false;

							fprintf(file, "\t\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"base\": %d, \"enter\": %d, \"leave\": %d, \"typeid\": %d", 
								v->mIdent->mString, v->mOffset, v->mOffset + v->mSize, BC_REG_LOCALS, 
								v->mDeclaration->mLocation.mLine, v->mDeclaration->mEndLocation.mLine,
								types.IndexOrPush(v->mDeclaration->mBase));
						}
						else
						{
							if (!vfirst)
								fprintf(file, ",\n");
							vfirst = false;

							fprintf(file, "\t\t\t{\"name\": \"%s\", \"start\": %d, \"end\": %d, \"base\": %d, \"enter\": %d, \"leave\": %d, \"typeid\": %d", 
								v->mIdent->mString, v->mOffset, v->mOffset + v->mSize, BC_REG_STACK, 
								v->mDeclaration->mLocation.mLine, v->mDeclaration->mEndLocation.mLine,
								types.IndexOrPush(v->mDeclaration->mBase));
						}

						if (!skipped)
						{
							if (v->mDeclaration)
								DumpReferences(file, v->mDeclaration);
							fprintf(file, "}");
						}
					}
				}

				fprintf(file, "]");
				DumpReferences(file, p->mDeclaration);
				fprintf(file, "}\n");
			}
		}
		fprintf(file, "\t],\n");

		first = true;
		fprintf(file, "\t\"types\": [\n");
		for (int i = 0; i < types.Size(); i++)
		{
			if (!first)
				fprintf(file, ",\n");
			first = false;

			Declaration* dec = types[i];
			switch (dec->mType)
			{
			case DT_TYPE_INTEGER:
				if (dec->mFlags & DTF_SIGNED)
					fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"int\"}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
				else
					fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"uint\"}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
				break;
			case DT_TYPE_FLOAT:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"float\"}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
				break;
			case DT_TYPE_BOOL:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"bool\"}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
				break;
			case DT_TYPE_ARRAY:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"array\", \"eid\": %d}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize, types.IndexOrPush(dec->mBase));
				break;
			case DT_TYPE_POINTER:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"ptr\", eid: %d}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize, types.IndexOrPush(dec->mBase));
				break;
			case DT_TYPE_REFERENCE:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"ref\", eid: %d}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize, types.IndexOrPush(dec->mBase));
				break;
			case DT_TYPE_RVALUEREF:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"rref\", eid: %d}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize, types.IndexOrPush(dec->mBase));
				break;
			case DT_TYPE_ENUM:
			{
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"enum\",\"members\": [\n", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
				bool	tfirst = true;
				Declaration* mdec = dec->mParams;
				while (mdec)
				{
					if (mdec->mIdent)
					{
						if (!tfirst)
							fprintf(file, ",\n");
						tfirst = false;

						fprintf(file, "\t\t\t{\"name\": \"%s\", \"value\": %d}", mdec->mIdent->mString, int(mdec->mInteger));
					}

					mdec = mdec->mNext;
				}
				fprintf(file, "]}");
			}
			break;
			break;
			case DT_TYPE_STRUCT:
			{
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d, \"type\": \"struct\",\"members\": [\n", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
					bool	tfirst = true;
					Declaration* mdec = dec->mParams;
					while (mdec)
					{
						if (!tfirst)
							fprintf(file, ",\n");
						tfirst = false;

						fprintf(file, "\t\t\t{\"name\": \"%s\", \"offset\": %d, \"typeid\": %d}", mdec->mIdent->mString, mdec->mOffset, types.IndexOrPush(mdec->mBase));

						mdec = mdec->mNext;
					}
					fprintf(file, "]}");
				}
				break;
			default:
				fprintf(file, "\t\t{\"name\": \"%s\", \"typeid\": %d, \"size\": %d}", dec->mQualIdent ? dec->mQualIdent->mString : "", i, dec->mSize);
			}
		}

		fprintf(file, "\t]");

		fprintf(file, "}");
		fclose(file);

		return true;
	}

	return false;
}

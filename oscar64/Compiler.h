#pragma once

#include "Errors.h"
#include "CompilationUnits.h"
#include "Preprocessor.h"
#include "ByteCodeGenerator.h"
#include "NativeCodeGenerator.h"
#include "InterCodeGenerator.h"
#include "GlobalAnalyzer.h"
#include "Linker.h"
#include "CompilerTypes.h"

class Compiler
{
public:
	Compiler(void);
	~Compiler(void);

	Errors* mErrors;
	Linker* mLinker;
	CompilationUnits* mCompilationUnits;
	Preprocessor* mPreprocessor;
	ByteCodeGenerator* mByteCodeGenerator;
	NativeCodeGenerator* mNativeCodeGenerator;
	InterCodeGenerator* mInterCodeGenerator;
	InterCodeModule* mInterCodeModule;
	GlobalAnalyzer* mGlobalAnalyzer;

	GrowingArray<ByteCodeProcedure*>	mByteCodeFunctions;

	uint64	mCompilerOptions;

	struct Define
	{
		const Ident* mIdent;
		const char* mValue;
	};

	GrowingArray<Define>	mDefines;

	bool ParseSource(void);
	bool GenerateCode(void);
	bool WriteOutputFile(const char* targetPath);
	int ExecuteCode(bool profile);

	void AddDefine(const Ident* ident, const char* value);

	void RegisterRuntime(const Location& loc, const Ident* ident);

	void CompileProcedure(InterCodeProcedure* proc);
};

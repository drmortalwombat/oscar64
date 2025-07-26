#pragma once

#include "Errors.h"
#include "CompilationUnits.h"
#include "Preprocessor.h"
#include "ByteCodeGenerator.h"
#include "NativeCodeGenerator.h"
#include "InterCodeGenerator.h"
#include "GlobalAnalyzer.h"
#include "GlobalOptimizer.h"
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
	GlobalOptimizer* mGlobalOptimizer;

	GrowingArray<ByteCodeProcedure*>	mByteCodeFunctions;

	TargetMachine	mTargetMachine;
	uint64			mCompilerOptions;
	uint16			mCartridgeID;
	char			mVersion[32];

	struct Define
	{
		const Ident* mIdent;
		const char* mValue;
	};

	GrowingArray<Define>	mDefines;

	bool BuildLZO(const char* targetPath);
	bool ParseSource(void);
	bool GenerateCode(void);
	bool WriteOutputFile(const char* targetPath, DiskImage * d64);
	bool WriteErrorFile(const char* targetPath);
	bool RemoveErrorFile(const char* targetPath);
	int ExecuteCode(bool profile, int trace, bool asserts);

	void AddDefine(const Ident* ident, const char* value);

	void RegisterRuntime(const Location& loc, const Ident* ident);

	void CompileProcedure(InterCodeProcedure* proc);
	void BuildVTables(void);
	void CompleteTemplateExpansion(void);

	bool WriteDbjFile(const char* filename);
	bool WriteCszFile(const char* filename);
};

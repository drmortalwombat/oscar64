#pragma once

#include "Errors.h"
#include "CompilationUnits.h"
#include "Preprocessor.h"
#include "ByteCodeGenerator.h"
#include "NativeCodeGenerator.h"
#include "InterCodeGenerator.h"
#include "Linker.h"

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

	GrowingArray<ByteCodeProcedure*>	mByteCodeFunctions;

	bool mNativeCode;

	struct Define
	{
		const Ident* mIdent;
		const char* mValue;
	};

	GrowingArray<Define>	mDefines;

	bool ParseSource(void);
	bool GenerateCode(void);
	bool WriteOutputFile(const char* targetPath);
	int ExecuteCode(void);

	void ForceNativeCode(bool native);
	void AddDefine(const Ident* ident, const char* value);

	void RegisterRuntime(const Location& loc, const Ident* ident);
};

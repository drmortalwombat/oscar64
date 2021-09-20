#pragma once

#include <stdio.h>
#include "MachineTypes.h"
#include "Ident.h"

class ByteCodeGenerator;
class InterCodeProcedure;

class ByteCodeDisassembler
{
public:
	ByteCodeDisassembler(void);
	~ByteCodeDisassembler(void);

	void Disassemble(FILE* file, const uint8* memory, int start, int size, InterCodeProcedure* proc, const Ident* ident);
protected:
	const char* TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc);
};

class NativeCodeDisassembler
{
public:
	NativeCodeDisassembler(void);
	~NativeCodeDisassembler(void);

	void Disassemble(FILE* file, const uint8* memory, int start, int size, InterCodeProcedure* proc, const Ident* ident);
protected:
	const char* TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc);
};



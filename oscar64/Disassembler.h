#pragma once

#include <stdio.h>
#include "MachineTypes.h"
#include "Ident.h"

class ByteCodeGenerator;
class InterCodeProcedure;
class Linker;
class LinkerObject;

class ByteCodeDisassembler
{
public:
	ByteCodeDisassembler(void);
	~ByteCodeDisassembler(void);

	void Disassemble(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker * linker);
protected:
	const char* TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc, Linker* linker);
	const char* AddrName(int addr, char* buffer, Linker* linker);
};

class NativeCodeDisassembler
{
public:
	NativeCodeDisassembler(void);
	~NativeCodeDisassembler(void);

	void Disassemble(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker* linker);
	void DumpMemory(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker* linker, LinkerObject * lobj);
protected:
	const char* TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc, Linker* linker);
	const char* AddrName(int bank, int addr, char* buffer, InterCodeProcedure* proc, Linker* linker);
};



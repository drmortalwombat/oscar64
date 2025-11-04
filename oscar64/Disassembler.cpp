#include "Disassembler.h"
#include "ByteCodeGenerator.h"
#include "Assembler.h"
#include "InterCode.h"
#include "Linker.h"

ByteCodeDisassembler::ByteCodeDisassembler(void)
{

}

ByteCodeDisassembler::~ByteCodeDisassembler(void)
{

}

const char* ByteCodeDisassembler::TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc, Linker* linker)
{
	if (tmp == BC_REG_ADDR)
		return "ADDR";
	else if (tmp == BC_REG_ACCU)
		return "ACCU";
	else if (tmp >= BC_REG_FPARAMS && tmp < BC_REG_FPARAMS_END)
	{
		sprintf_s(buffer, 10, "P%d", tmp - BC_REG_FPARAMS);
		return buffer;
	}
	else if (proc && tmp >= BC_REG_TMP && tmp < BC_REG_TMP + proc->mTempSize)
	{
		int	i = 0;
		while (i < proc->mTempOffset.Size() && proc->mTempOffset[i] != tmp - BC_REG_TMP)
			i++;
		if (i < proc->mTempOffset.Size())
			sprintf_s(buffer, 10, "T%d", i);
		else
			sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}
	else if (linker)
	{
		LinkerObject* obj = linker->FindObjectByAddr(tmp);
		if (obj && obj->mIdent)
			sprintf_s(buffer, 40, "$%02x; %s + %d", tmp, obj->mIdent->mString, tmp - obj->mAddress);
		else
			sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}
	else
	{
		sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}
}

const char* ByteCodeDisassembler::AddrName(int addr, char* buffer, Linker* linker)
{
	if (linker)
	{
		LinkerObject* obj = linker->FindObjectByAddr(addr);
		if (obj && obj->mIdent)
		{
			sprintf_s(buffer, 160, "%s + %d", obj->mIdent->mString, addr - obj->mAddress);
			return buffer;
		}
	}

	sprintf_s(buffer, 10, "$%04x", addr);
	return buffer;
}

void ByteCodeDisassembler::Disassemble(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker* linker)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc && proc->mIdent)
		fprintf(file, "%s:\n", proc->mIdent->mString);
	else if (ident)
		fprintf(file, "%s:\n", ident->mString);

	char	tbuffer[160], abuffer[160];
#if 0
	for (int i = 0; i < proc->mTemporaries.Size(); i++)
		printf("T%d = $%.2x\n", i, BC_REG_TMP + proc->mTempOffset[i]);
#endif
	int	i = 0;
	while (i < size)
	{
		ByteCode	bc = ByteCode(memory[start + i] / 2);

		if (bank >= 0)
			fprintf(file, "%02x:", bank);

		fprintf(file, "%04x:\t", start + i);
		i++;

		switch (bc)
		{
		case BC_NOP:
			fprintf(file, "NOP");
			break;
		case BC_EXIT:
			fprintf(file, "EXIT");
			break;

		case BC_CONST_8:
			fprintf(file, "MOVB\t%s, #%d", TempName(memory[start + i], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_CONST_P8:
			fprintf(file, "MOV\t%s, #%d", TempName(memory[start + i], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_CONST_N8:
			fprintf(file, "MOV\t%s, #%d", TempName(memory[start + i], tbuffer, proc, linker), int(memory[start + i + 1]) - 0x100);
			i += 2;
			break;
		case BC_CONST_16:
			fprintf(file, "MOV\t%s, #$%04x", TempName(memory[start + i], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_CONST_32:
			fprintf(file, "MOVD\t%s, #$%08x", TempName(memory[start + i], tbuffer, proc, linker), uint32(memory[start + i + 1] + 256 * memory[start + i + 2] + 0x10000 * memory[start + i + 3] + 0x1000000 * memory[start + i + 4]));
			i += 5;
			break;

		case BC_LOAD_REG_8:
			fprintf(file, "MOVB\tACCU, %s", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_STORE_REG_8:
			fprintf(file, "MOVB\t%s, ACCU", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_LOAD_REG_16:
			fprintf(file, "MOV\tACCU, %s", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_STORE_REG_16:
			fprintf(file, "MOV\t%s, ACCU", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_LOAD_REG_32:
			fprintf(file, "MOVD\tACCU, %s", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_STORE_REG_32:
			fprintf(file, "MOVD\t%s, ACCU", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_ADDR_REG:
			fprintf(file, "MOV\tADDR, %s", TempName(memory[start + i], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_LOAD_ABS_8:
			fprintf(file, "MOVB\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_U8:
			fprintf(file, "MOVUB\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_16:
			fprintf(file, "MOV\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_32:
			fprintf(file, "MOVD\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_ADDR:
			fprintf(file, "MOV\tADDR, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 2;
			break;

		case BC_LEA_ABS:
			fprintf(file, "LEA\t%s, %s", TempName(memory[start + i + 0], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 1] + 256 * memory[start + i + 2]), abuffer, linker));
			i += 3;
			break;

		case BC_LEA_ABS_INDEX:
			fprintf(file, "LEAX\tADDR, %s + %s", AddrName(uint16(memory[start + i + 1] + 256 * memory[start + i + 2]), abuffer, linker), TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 3;
			break;

		case BC_LEA_ACCU_INDEX:
			fprintf(file, "LEAX\tADDR, %s + ACCU", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_LEA_ABS_INDEX_U8:
			fprintf(file, "LEAXB\tADDR, %s + %s", AddrName(uint16(memory[start + i + 1] + 256 * memory[start + i + 2]), abuffer, linker), TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 3;
			break;

		case BC_STORE_ABS_8:
			fprintf(file, "MOVB\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc, linker));
			i += 3;
			break;
		case BC_STORE_ABS_16:
			fprintf(file, "MOV\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc, linker));
			i += 3;
			break;
		case BC_STORE_ABS_32:
			fprintf(file, "MOVD\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc, linker));
			i += 3;
			break;

		case BC_LOAD_LOCAL_8:
			fprintf(file, "MOVB\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_U8:
			fprintf(file, "MOVUB\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_16:
			fprintf(file, "MOV\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_32:
			fprintf(file, "MOVD\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;

		case BC_STORE_LOCAL_8:
			fprintf(file, "MOVB\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_LOCAL_16:
			fprintf(file, "MOV\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_LOCAL_32:
			fprintf(file, "MOVD\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;

		case BC_LEA_LOCAL:
			fprintf(file, "LEA\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;

		case BC_LEA_FRAME:
			fprintf(file, "LEA\t%s, %d(SP)", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;

		case BC_STORE_FRAME_8:
			fprintf(file, "MOVB\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_FRAME_16:
			fprintf(file, "MOV\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_FRAME_32:
			fprintf(file, "MOVD\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;

		case BC_BINOP_ADDR_16:
			fprintf(file, "ADD\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_ADDA_16:
			fprintf(file, "ADD\t%s, ACCU", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_SUBR_16:
			fprintf(file, "SUB\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_MULR_16:
			fprintf(file, "MUL\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_DIVR_U16:
			fprintf(file, "DIVU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_MODR_U16:
			fprintf(file, "MODU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_DIVR_I16:
			fprintf(file, "DIVS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_MODR_I16:
			fprintf(file, "MODS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_ANDR_16:
			fprintf(file, "AND\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_ORR_16:
			fprintf(file, "OR\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_XORR_16:
			fprintf(file, "XOR\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_SHLR_16:
			fprintf(file, "SHL\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_SHRR_I16:
			fprintf(file, "SHRU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_SHRR_U16:
			fprintf(file, "SHRI\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_BINOP_ADDI_16:
			fprintf(file, "ADD\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_SUBI_16:
			fprintf(file, "SUBR\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ANDI_16:
			fprintf(file, "AND\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ORI_16:
			fprintf(file, "OR\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_MULI8_16:
			fprintf(file, "MUL\t%s, #%d", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;

		case BC_BINOP_ADDI_8:
			fprintf(file, "ADDB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_BINOP_ANDI_8:
			fprintf(file, "ANDB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_BINOP_ORI_8:
			fprintf(file, "ORB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;

		case BC_LOOP_U8:
			fprintf(file, "LOOPB\t%s, #$%02X", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;

		case BC_CONV_I8_I16:
			fprintf(file, "SEXT8\t%s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i++;
			break;

		case BC_BINOP_SHLI_16:
			fprintf(file, "SHL\tACCU, #%d", uint8(memory[start + i + 0]));
			i += 1;
			break;
		case BC_BINOP_SHRI_U16:
			fprintf(file, "SHRU\tACCU, #%d", uint8(memory[start + i + 0]));
			i += 1;
			break;
		case BC_BINOP_SHRI_I16:
			fprintf(file, "SHRS\tACCU, #%d", uint8(memory[start + i + 0]));
			i += 1;
			break;

		case BC_BINOP_CMPUR_16:
			fprintf(file, "CMPU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_CMPSR_16:
			fprintf(file, "CMPS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_BINOP_CMPUI_16:
			fprintf(file, "CMPU\tACCU, #$%04X", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BINOP_CMPSI_16:
			fprintf(file, "CMPS\tACCU, #$%04X", int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_BINOP_CMPUR_8:
			fprintf(file, "CMPUB\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_CMPSR_8:
			fprintf(file, "CMPSB\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_BINOP_CMPUI_8:
			fprintf(file, "CMPUB\tACCU, #$%04X", uint8(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 1;
			break;
		case BC_BINOP_CMPSI_8:
			fprintf(file, "CMPSB\tACCU, #$%04X", int8(memory[start + i + 0]));
			i += 1;
			break;

		case BC_BINOP_ADD_F32:
			fprintf(file, "ADDF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_SUB_F32:
			fprintf(file, "SUBF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_MUL_F32:
			fprintf(file, "MULF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_DIV_F32:
			fprintf(file, "DIVF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;
		case BC_BINOP_CMP_F32:
			fprintf(file, "CMPF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 1;
			break;

		case BC_FILL:
			fprintf(file, "FILL\t#%d", memory[start + i + 0]);
			i++;
			break;
		case BC_COPY:
			fprintf(file, "COPY\t#%d", memory[start + i + 0]);
			i++;
			break;
		case BC_STRCPY:
			fprintf(file, "STRCPY");
			break;

		case BC_COPY_LONG:
			fprintf(file, "COPYL\t#%d", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_FILL_LONG:
			fprintf(file, "FILLL\t#%d", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_OP_NEGATE_16:
			fprintf(file, "NEG\tACCU");
			break;
		case BC_OP_INVERT_16:
			fprintf(file, "NOT\tACCU");
			break;

		case BC_OP_NEGATE_F32:
			fprintf(file, "NEGF\tACCU");
			break;
		case BC_OP_ABS_F32:
			fprintf(file, "ABSF\tACCU");
			break;
		case BC_OP_FLOOR_F32:
			fprintf(file, "FLOORF\tACCU");
			break;
		case BC_OP_CEIL_F32:
			fprintf(file, "CEILF\tACCU");
			break;


		case BC_CONV_U16_F32:
			fprintf(file, "CNVUF\tACCU");
			break;
		case BC_CONV_I16_F32:
			fprintf(file, "CNVSF\tACCU");
			break;
		case BC_CONV_F32_U16:
			fprintf(file, "CNVFU\tACCU");
			break;
		case BC_CONV_F32_I16:
			fprintf(file, "CNVFS\tACCU");
			break;

		case BC_CONV_U32_F32:
			fprintf(file, "CNVLUF\tACCU");
			break;
		case BC_CONV_I32_F32:
			fprintf(file, "CNVLSF\tACCU");
			break;
		case BC_CONV_F32_U32:
			fprintf(file, "CNVFLU\tACCU");
			break;
		case BC_CONV_F32_I32:
			fprintf(file, "CNVFLS\tACCU");
			break;

		case BC_MALLOC:
			fprintf(file, "MALLOC\tACCU");
			break;
		case BC_FREE:
			fprintf(file, "FREE\tACCU");
			break;

		case BC_JUMPS:
			fprintf(file, "JUMP\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_EQ:
			fprintf(file, "BEQ\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_NE:
			fprintf(file, "BNE\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_GT:
			fprintf(file, "BGT\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_GE:
			fprintf(file, "BGE\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_LT:
			fprintf(file, "BLT\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;
		case BC_BRANCHS_LE:
			fprintf(file, "BLE\t$%04X", start + i + 1 + int8(memory[start + i + 0]));
			i++;
			break;

		case BC_JUMPF:
			fprintf(file, "JUMPF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_EQ:
			fprintf(file, "BEQF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_NE:
			fprintf(file, "BNEF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_GT:
			fprintf(file, "BGTF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_GE:
			fprintf(file, "BGEF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_LT:
			fprintf(file, "BLTF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BRANCHF_LE:
			fprintf(file, "BLEF\t$%04X", start + i + 2 + int16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_ENTER:
			fprintf(file, "ENTER\t%d, %d", memory[start + i + 2], uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 3;
			break;
		case BC_RETURN:
			fprintf(file, "RETURN\t%d, %d", memory[start + i], uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_CALL_ADDR:
			fprintf(file, "CALL\tADDR");
			break;
		case BC_CALL_ABS:
			fprintf(file, "CALL\t%s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 2;
			break;
		case BC_JSR:
			fprintf(file, "JSR\t%s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 2;
			break;

		case BC_PUSH_FRAME:
			fprintf(file, "PUSH\t#$%04X", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_POP_FRAME:
			fprintf(file, "POP\t#$%04X", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_LOAD_ADDR_8:
			fprintf(file, "MOVB\t%s, %d(ADDR)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_U8:
			fprintf(file, "MOVUB\t%s, %d(ADDR)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_16:
			fprintf(file, "MOV\t%s, %d(ADDR)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_32:
			fprintf(file, "MOVD\t%s, %d(ADDR)", TempName(memory[start + i + 0], tbuffer, proc, linker), memory[start + i + 1]);
			i += 2;
			break;

		case BC_STORE_ADDR_8:
			fprintf(file, "MOVB\t%d(ADDR), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_ADDR_16:
			fprintf(file, "MOV\t%d(ADDR), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;
		case BC_STORE_ADDR_32:
			fprintf(file, "MOVD\t%d(ADDR), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc, linker));
			i += 2;
			break;

		case BC_EXTRT:
			fprintf(file, "EXTRT\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc, linker), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		}

		fprintf(file, "\n");
	}
}


NativeCodeDisassembler::NativeCodeDisassembler(void)
{

}

NativeCodeDisassembler::~NativeCodeDisassembler(void)
{

}

void NativeCodeDisassembler::DumpMemory(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker* linker, LinkerObject * lobj)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc && proc->mIdent)
		fprintf(file, "%s:\n", proc->mIdent->mString);
	else if (ident)
		fprintf(file, "%s:\n", ident->mString);

	if (lobj->mSection->mType == LST_BSS)
	{
		if (bank >= 0)
			fprintf(file, "%02x:", bank);

		fprintf(file, "%04x : __ __ __ BSS\t%d\n", start, size);
	}
	else
	{
		int		ip = start;
		while (ip < start + size)
		{
			int	n = 16;
			if (ip + n > start + size)
				n = start + size - ip;

			if (bank >= 0)
				fprintf(file, "%02x:", bank);

			fprintf(file, "%04x : __ __ __ BYT", ip);

			for (int i = 0; i < n; i++)
				fprintf(file, " %02x", memory[ip + i]);
			for (int i = n; i < 16; i++)
				fprintf(file, "   ");
			fprintf(file, " : ");
			for (int i = 0; i < n; i++)
			{
				int k = memory[ip + i];
				if (k >= 32 && k < 127)
					fprintf(file, "%c", k);
				else
					fprintf(file, ".");
			}
			fprintf(file, "\n");

			ip += n;
		}
	}
}

void NativeCodeDisassembler::Disassemble(FILE* file, const uint8* memory, int bank, int start, int size, InterCodeProcedure* proc, const Ident * ident, Linker* linker, const Ident * fident)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc && proc->mIdent)
		fprintf(file, "%s: ; %s\n", proc->mIdent->mString, fident->mString);
	else if (ident)
		fprintf(file, "%s: ; %s\n", ident->mString, fident->mString);

	char	tbuffer[160], abuffer[160];

	int		ip = start;
	while (ip < start + size)
	{
		int			iip = ip;
		uint8	opcode = memory[ip++];
		AsmInsData	d = DecInsData[opcode];
		int	addr = 0;

		if (proc && proc->mLinkerObject)
		{
			int i = 0;
			while (i < proc->mLinkerObject->mRanges.Size() && iip - start != proc->mLinkerObject->mRanges[i].mOffset)
				i++;
			if (i < proc->mLinkerObject->mRanges.Size())
				fprintf(file, ".%s:\n", proc->mLinkerObject->mRanges[i].mIdent->mString);

			i = 0;
			while (i < proc->mLinkerObject->mCodeOrigins.Size() && iip - start != proc->mLinkerObject->mCodeOrigins[i].mStart)
				i++;
			if (i < proc->mLinkerObject->mCodeOrigins.Size())
				fprintf(file, ";%4d, \"%s\"\n", proc->mLinkerObject->mCodeOrigins[i].mLocation.mLine, proc->mLinkerObject->mCodeOrigins[i].mLocation.mFileName);
		}

		if (bank >= 0)
			fprintf(file, "%02x:", bank);

		switch (d.mMode)
		{
		case ASMIM_IMPLIED:
			fprintf(file, "%04x : %02x __ __ %s\n", iip, memory[iip], AsmInstructionNames[d.mType]);
			break;
		case ASMIM_IMMEDIATE:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s #$%02x\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], addr);
			break;
		case ASMIM_ZERO_PAGE:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s %s %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc, linker), AddrName(bank, addr, abuffer, proc, linker));
			break;
		case ASMIM_ZERO_PAGE_X:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s %s,x %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc, linker), AddrName(bank, addr, abuffer, proc, linker));
			break;
		case ASMIM_ZERO_PAGE_Y:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s %s,y %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc, linker), AddrName(bank, addr, abuffer, proc, linker));
			break;
		case ASMIM_ABSOLUTE:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s $%04x %s\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], addr, AddrName(bank, addr, abuffer, proc, linker));
			ip += 2;
			break;
		case ASMIM_ABSOLUTE_X:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s $%04x,x %s\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], addr, AddrName(bank, addr, abuffer, proc, linker));
			ip += 2;
			break;
		case ASMIM_ABSOLUTE_Y:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s $%04x,y %s\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], addr, AddrName(bank, addr, abuffer, proc, linker));
			ip += 2;
			break;
		case ASMIM_INDIRECT:
			addr = memory[ip] + 256 * memory[ip + 1];
			ip += 2;
			fprintf(file, "%04x : %02x %02x %02x %s ($%04x)\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], addr);
			break;
		case ASMIM_INDIRECT_X:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s (%s,x) %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc, linker), AddrName(bank, addr, abuffer, proc, linker));
			break;
		case ASMIM_INDIRECT_Y:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s (%s),y %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc, linker), AddrName(bank, addr, abuffer, proc, linker));
			break;
		case ASMIM_RELATIVE:
			addr = memory[ip++];
			if (addr & 0x80)
				addr = addr + ip - 256;
			else
				addr = addr + ip;
			fprintf(file, "%04x : %02x %02x __ %s $%04x %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], addr, AddrName(bank, addr, abuffer, proc, linker));
			break;
		}
	}

}

const char* NativeCodeDisassembler::AddrName(int bank, int addr, char* buffer, InterCodeProcedure* proc, Linker* linker)
{
	if (linker)
	{
		LinkerObject* obj;

		if (proc && proc->mLinkerObject && addr < 256)
		{
			obj = proc->mLinkerObject;

			int	i = 0;
			while (i < obj->mZeroPageRanges.Size() && !(addr >= obj->mZeroPageRanges[i].mOffset && addr < obj->mZeroPageRanges[i].mOffset + obj->mZeroPageRanges[i].mSize))
				i++;

			if (i < obj->mZeroPageRanges.Size())
			{
				sprintf_s(buffer, 160, "; (%s + %d)", obj->mZeroPageRanges[i].mIdent->mString, addr - obj->mZeroPageRanges[i].mOffset);
				return buffer;

			}
		}
		
		if (proc && proc->mLinkerObject && addr >= proc->mLinkerObject->mAddress && addr < proc->mLinkerObject->mAddress + proc->mLinkerObject->mSize)
			obj = proc->mLinkerObject;
		else
			obj = linker->FindObjectByAddr(bank, addr, proc);

		if (obj && obj->mIdent)
		{
			int i = 0;
			while (i < obj->mRanges.Size() && (addr - obj->mAddress < obj->mRanges[i].mOffset || addr - obj->mAddress - obj->mRanges[i].mOffset >= obj->mRanges[i].mSize))
				i++;
			if (i < obj->mRanges.Size() && obj->mRanges[i].mIdent)
				sprintf_s(buffer, 160, "; (%s.%s + %d)", obj->mIdent->mString, obj->mRanges[i].mIdent->mString, addr - obj->mAddress - obj->mRanges[i].mOffset);
			else
				sprintf_s(buffer, 160, "; (%s + %d)", obj->mIdent->mString, addr - obj->mAddress);
			return buffer;
		}
	}

	return "";
}


const char* NativeCodeDisassembler::TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc, Linker * linker)
{
	if (tmp >= BC_REG_ADDR && tmp <= BC_REG_ADDR + 3)
	{
		sprintf_s(buffer, 10, "ADDR + %d", tmp - BC_REG_ADDR);
		return buffer;
	}
	else if (tmp >= BC_REG_ACCU && tmp <= BC_REG_ACCU + 3)
	{
		sprintf_s(buffer, 10, "ACCU + %d", tmp - BC_REG_ACCU);
		return buffer;
	}
	else if (tmp >= BC_REG_WORK && tmp <= BC_REG_WORK + 8)
	{
		sprintf_s(buffer, 10, "WORK + %d", tmp - BC_REG_WORK);
		return buffer;
	}
	else if (tmp >= BC_REG_STACK && tmp <= BC_REG_STACK + 1)
	{
		sprintf_s(buffer, 10, "SP + %d", tmp - BC_REG_STACK);
		return buffer;
	}
	else if (tmp >= BC_REG_IP && tmp <= BC_REG_IP + 1)
	{
		sprintf_s(buffer, 10, "IP + %d", tmp - BC_REG_IP);
		return buffer;
	}
	else if (tmp >= BC_REG_FPARAMS && tmp < BC_REG_FPARAMS_END)
	{
		sprintf_s(buffer, 10, "P%d", tmp - BC_REG_FPARAMS);
		return buffer;
	}
	else if (tmp >= BC_REG_LOCALS && tmp <= BC_REG_LOCALS + 1)
	{
		sprintf_s(buffer, 10, "FP + %d", tmp - BC_REG_LOCALS);
		return buffer;
	}
	else if (proc && tmp >= BC_REG_TMP && tmp < BC_REG_TMP + proc->mTempSize)
	{
		int	i = 0;
		while (i < proc->mTempOffset.Size() && !(tmp >= proc->mTempOffset[i] + BC_REG_TMP && tmp < proc->mTempOffset[i] + proc->mTempSizes[i] + BC_REG_TMP))
			i++;
		if (i < proc->mTempOffset.Size())
			sprintf_s(buffer, 10, "T%d + %d", i, tmp - (proc->mTempOffset[i] + BC_REG_TMP));
		else
			sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}
	else
	{
		sprintf_s(buffer, 10, "$%02x", tmp);
		return buffer;
	}

}


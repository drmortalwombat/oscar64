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

const char* ByteCodeDisassembler::TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc)
{
	if (tmp == BC_REG_ADDR)
		return "ADDR";
	else if (tmp == BC_REG_ACCU)
		return "ACCU";
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
			return obj->mIdent->mString;
	}

	sprintf_s(buffer, 10, "$%04x", addr);
	return buffer;
}

void ByteCodeDisassembler::Disassemble(FILE* file, const uint8* memory, int start, int size, InterCodeProcedure* proc, const Ident* ident, Linker* linker)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc && proc->mIdent)
		fprintf(file, "%s:\n", proc->mIdent->mString);
	else if (ident)
		fprintf(file, "%s:\n", ident->mString);

	char	tbuffer[10], abuffer[10];
#if 0
	for (int i = 0; i < proc->mTemporaries.Size(); i++)
		printf("T%d = $%.2x\n", i, BC_REG_TMP + proc->mTempOffset[i]);
#endif
	int	i = 0;
	while (i < size)
	{
		ByteCode	bc = ByteCode(memory[start + i] / 2);

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
			fprintf(file, "MOVB\t%s, #%d", TempName(memory[start + i], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_CONST_P8:
			fprintf(file, "MOV\t%s, #%d", TempName(memory[start + i], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_CONST_N8:
			fprintf(file, "MOV\t%s, #%d", TempName(memory[start + i], tbuffer, proc), int(memory[start + i + 1]) - 0x100);
			i += 2;
			break;
		case BC_CONST_16:
			fprintf(file, "MOV\t%s, #$%04x", TempName(memory[start + i], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_CONST_32:
			fprintf(file, "MOVD\t%s, #$%08x", TempName(memory[start + i], tbuffer, proc), uint32(memory[start + i + 1] + 256 * memory[start + i + 2] + 0x10000 * memory[start + i + 3] + 0x1000000 * memory[start + i + 4]));
			i += 5;
			break;

		case BC_LOAD_REG_8:
			fprintf(file, "MOVB\tACCU, %s", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_STORE_REG_8:
			fprintf(file, "MOVB\t%s, ACCU", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_LOAD_REG_16:
			fprintf(file, "MOV\tACCU, %s", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_STORE_REG_16:
			fprintf(file, "MOV\t%s, ACCU", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_LOAD_REG_32:
			fprintf(file, "MOVD\tACCU, %s", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_STORE_REG_32:
			fprintf(file, "MOVD\t%s, ACCU", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;
		case BC_ADDR_REG:
			fprintf(file, "MOV\tADDR, %s", TempName(memory[start + i], tbuffer, proc));
			i += 1;
			break;

		case BC_LOAD_ABS_8:
			fprintf(file, "MOVUB\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_U8:
			fprintf(file, "MOVUB\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_I8:
			fprintf(file, "MOVSB\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_16:
			fprintf(file, "MOV\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;
		case BC_LOAD_ABS_32:
			fprintf(file, "MOVD\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
			i += 3;
			break;

		case BC_LEA_ABS:
			fprintf(file, "LEA\t%s, %s", TempName(memory[start + i + 0], tbuffer, proc), AddrName(uint16(memory[start + i + 1] + 256 * memory[start + i + 2]), abuffer, linker));
			i += 3;
			break;

		case BC_STORE_ABS_8:
			fprintf(file, "MOVB\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc));
			i += 3;
			break;
		case BC_STORE_ABS_16:
			fprintf(file, "MOV\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc));
			i += 3;
			break;
		case BC_STORE_ABS_32:
			fprintf(file, "MOVD\t%s, %s", AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker), TempName(memory[start + i + 2], tbuffer, proc));
			i += 3;
			break;

		case BC_LOAD_LOCAL_8:
			fprintf(file, "MOVB\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_U8:
			fprintf(file, "MOVUB\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_I8:
			fprintf(file, "MOVSB\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_16:
			fprintf(file, "MOV\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_LOCAL_32:
			fprintf(file, "MOVD\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;

		case BC_STORE_LOCAL_8:
			fprintf(file, "MOVB\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_LOCAL_16:
			fprintf(file, "MOV\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_LOCAL_32:
			fprintf(file, "MOVD\t%d(FP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;

		case BC_LEA_LOCAL:
			fprintf(file, "LEA\t%s, %d(FP)", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;

		case BC_LEA_FRAME:
			fprintf(file, "LEA\t%s, %d(SP)", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;

		case BC_STORE_FRAME_8:
			fprintf(file, "MOVB\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_FRAME_16:
			fprintf(file, "MOV\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_FRAME_32:
			fprintf(file, "MOVD\t%d(SP), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;

		case BC_BINOP_ADDR_16:
			fprintf(file, "ADD\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SUBR_16:
			fprintf(file, "SUB\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MULR_16:
			fprintf(file, "MUL\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIVR_U16:
			fprintf(file, "DIVU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MODR_U16:
			fprintf(file, "MODU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIVR_I16:
			fprintf(file, "DIVS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MODR_I16:
			fprintf(file, "MODS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_ANDR_16:
			fprintf(file, "AND\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_ORR_16:
			fprintf(file, "OR\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_XORR_16:
			fprintf(file, "XOR\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHLR_16:
			fprintf(file, "SHL\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHRR_I16:
			fprintf(file, "SHRU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SHRR_U16:
			fprintf(file, "SHRI\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_BINOP_ADDI_16:
			fprintf(file, "ADD\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_SUBI_16:
			fprintf(file, "SUBR\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ANDI_16:
			fprintf(file, "AND\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_ORI_16:
			fprintf(file, "OR\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_BINOP_MULI8_16:
			fprintf(file, "MUL\t%s, #%d", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;

		case BC_BINOP_ADDI_8:
			fprintf(file, "ADDB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_BINOP_ANDI_8:
			fprintf(file, "ANDB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_BINOP_ORI_8:
			fprintf(file, "ORB\t%s, #$%04X", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;

		case BC_CONV_I8_I16:
			fprintf(file, "SEXT8\t%s", TempName(memory[start + i + 0], tbuffer, proc));
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
			fprintf(file, "CMPU\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_CMPSR_16:
			fprintf(file, "CMPS\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_BINOP_CMPUI_16:
			fprintf(file, "CMPU\tACCU, #$%04X", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;
		case BC_BINOP_CMPSI_16:
			fprintf(file, "CMPS\tACCU, #$%04X", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 2;
			break;

		case BC_BINOP_ADD_F32:
			fprintf(file, "ADDF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_SUB_F32:
			fprintf(file, "SUBF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_MUL_F32:
			fprintf(file, "MULF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_DIV_F32:
			fprintf(file, "DIVF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;
		case BC_BINOP_CMP_F32:
			fprintf(file, "CMPF\tACCU, %s", TempName(memory[start + i + 0], tbuffer, proc));
			i += 1;
			break;

		case BC_COPY:
			fprintf(file, "COPY\t#%d", memory[start + i + 0]);
			i++;
			break;

		case BC_COPY_LONG:
			fprintf(file, "COPYL\t#%d", uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
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

		case BC_SET_EQ:
			fprintf(file, "SEQ");
			break;
		case BC_SET_NE:
			fprintf(file, "SNE");
			break;
		case BC_SET_GT:
			fprintf(file, "SGT");
			break;
		case BC_SET_GE:
			fprintf(file, "SGE");
			break;
		case BC_SET_LT:
			fprintf(file, "SLT");
			break;
		case BC_SET_LE:
			fprintf(file, "SLE");
			break;

		case BC_ENTER:
			fprintf(file, "ENTER\t%d, %d", memory[start + i + 2], uint16(memory[start + i + 0] + 256 * memory[start + i + 1]));
			i += 3;
			break;
		case BC_RETURN:
			fprintf(file, "RETURN\t%d, %d", memory[start + i], uint16(memory[start + i + 1] + 256 * memory[start + i + 2]));
			i += 3;
			break;
		case BC_CALL:
			fprintf(file, "CALL");
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
			fprintf(file, "MOVB\t%s, (ADDR + %d)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_U8:
			fprintf(file, "MOVUB\t%s, (ADDR + %d)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_I8:
			fprintf(file, "MOVSB\t%s, (ADDR + %d)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_16:
			fprintf(file, "MOV\t%s, (ADDR + %d)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;
		case BC_LOAD_ADDR_32:
			fprintf(file, "MOVD\t%s, (ADDR + %d)", TempName(memory[start + i + 0], tbuffer, proc), memory[start + i + 1]);
			i += 2;
			break;

		case BC_STORE_ADDR_8:
			fprintf(file, "MOVB\t(ADDR + %d), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_ADDR_16:
			fprintf(file, "MOV\t(ADDR + %d), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;
		case BC_STORE_ADDR_32:
			fprintf(file, "MOV\t(ADDR + %d), %s", memory[start + i + 1], TempName(memory[start + i + 0], tbuffer, proc));
			i += 2;
			break;

		case BC_EXTRT:
			fprintf(file, "EXTRT\t%s, %s", TempName(memory[start + i + 2], tbuffer, proc), AddrName(uint16(memory[start + i + 0] + 256 * memory[start + i + 1]), abuffer, linker));
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

void NativeCodeDisassembler::Disassemble(FILE* file, const uint8* memory, int start, int size, InterCodeProcedure* proc, const Ident * ident, Linker* linker)
{
	fprintf(file, "--------------------------------------------------------------------\n");
	if (proc && proc->mIdent)
		fprintf(file, "%s:\n", proc->mIdent->mString);
	else if (ident)
		fprintf(file, "%s:\n", ident->mString);

	char	tbuffer[10], abuffer[10];

	int		ip = start;
	while (ip < start + size)
	{
		int			iip = ip;
		uint8	opcode = memory[ip++];
		AsmInsData	d = DecInsData[opcode];
		int	addr = 0;

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
			fprintf(file, "%04x : %02x %02x __ %s %s\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc));
			break;
		case ASMIM_ZERO_PAGE_X:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s %s,x\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc));
			break;
		case ASMIM_ZERO_PAGE_Y:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s %s,y\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc));
			break;
		case ASMIM_ABSOLUTE:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s %s\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], AddrName(addr, abuffer, linker));
			ip += 2;
			break;
		case ASMIM_ABSOLUTE_X:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s %s,x\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], AddrName(addr, abuffer, linker));
			ip += 2;
			break;
		case ASMIM_ABSOLUTE_Y:
			addr = memory[ip] + 256 * memory[ip + 1];
			fprintf(file, "%04x : %02x %02x %02x %s %s,y\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], AddrName(addr, abuffer, linker));
			ip += 2;
			break;
		case ASMIM_INDIRECT:
			addr = memory[ip] + 256 * memory[ip + 1];
			ip += 2;
			fprintf(file, "%04x : %02x %02x %02x %s ($%04x)\n", iip, memory[iip], memory[iip + 1], memory[iip + 2], AsmInstructionNames[d.mType], addr);
			break;
		case ASMIM_INDIRECT_X:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s (%s,x)\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc));
			break;
		case ASMIM_INDIRECT_Y:
			addr = memory[ip++];
			fprintf(file, "%04x : %02x %02x __ %s (%s),y\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], TempName(addr, tbuffer, proc));
			break;
		case ASMIM_RELATIVE:
			addr = memory[ip++];
			if (addr & 0x80)
				addr = addr + ip - 256;
			else
				addr = addr + ip;
			fprintf(file, "%04x : %02x %02x __ %s $%04x\n", iip, memory[iip], memory[iip + 1], AsmInstructionNames[d.mType], addr);
			break;
		}
	}

}

const char* NativeCodeDisassembler::AddrName(int addr, char* buffer, Linker* linker)
{
	if (linker)
	{
		LinkerObject* obj = linker->FindObjectByAddr(addr);
		if (obj && obj->mIdent)
			return obj->mIdent->mString;
	}

	sprintf_s(buffer, 10, "$%04x", addr);
	return buffer;
}


const char* NativeCodeDisassembler::TempName(uint8 tmp, char* buffer, InterCodeProcedure* proc)
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
	else if (tmp >= BC_REG_STACK && tmp <= BC_REG_STACK + 1)
	{
		sprintf_s(buffer, 10, "SP + %d", tmp - BC_REG_STACK);
		return buffer;
	}
	else if (tmp >= BC_REG_LOCALS && tmp <= BC_REG_LOCALS + 3)
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


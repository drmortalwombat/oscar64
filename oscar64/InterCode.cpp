#include "InterCode.h"

#include "InterCode.h"
#include <stdio.h>
#include <math.h>
#include <crtdbg.h>

ValueSet::ValueSet(void)
{
	size = 32;
	num = 0;
	instructions = new InterInstructionPtr[size];
}

ValueSet::ValueSet(const ValueSet& values)
{
	int	i;

	size = values.size;
	num = values.num;
	instructions = new InterInstructionPtr[size];

	for (i = 0; i < num; i++)
		instructions[i] = values.instructions[i];
}

ValueSet::~ValueSet(void)
{
	delete[] instructions;
}

void ValueSet::FlushAll(void)
{
	num = 0;
}

void ValueSet::FlushCallAliases(void)
{
	int	i;

	i = 0;

	while (i < num)
	{
		if ((instructions[i]->code == IC_LOAD || instructions[i]->code == IC_STORE) && instructions[i]->mem != IM_PARAM && instructions[i]->mem != IM_LOCAL)
		{
			//
			// potential alias load
			//
			num--;
			if (i < num)
			{
				instructions[i] = instructions[num];
			}
		}
		else
			i++;
	}
}

static __int64 ConstantFolding(InterOperator oper, __int64 val1, __int64 val2)
{
	switch (oper)
	{
	case IA_ADD:
		return val1 + val2;
		break;
	case IA_SUB:
		return val1 - val2;
		break;
	case IA_MUL:
		return val1 * val2;
		break;
	case IA_DIVU:
		return (unsigned __int64)val1 / (unsigned __int64)val2;
		break;
	case IA_DIVS:
		return val1 / val2;
		break;
	case IA_MODU:
		return (unsigned __int64)val1 % (unsigned __int64)val2;
		break;
	case IA_MODS:
		return val1 % val2;
		break;
	case IA_OR:
		return val1 | val2;
		break;
	case IA_AND:
		return val1 & val2;
		break;
	case IA_XOR:
		return val1 ^ val2;
		break;
	case IA_NEG:
		return -val1;
		break;
	case IA_NOT:
		return ~val1;
		break;
	case IA_SHL:
		return val1 << val2;
		break;
	case IA_SHR:
		return (unsigned __int64)val1 >> (unsigned __int64)val2;
		break;
	case IA_SAR:
		return val1 >> val2;
		break;
	case IA_CMPEQ:
		return val1 == val2 ? 1 : 0;
		break;
	case IA_CMPNE:
		return val1 != val2 ? 1 : 0;
		break;
	case IA_CMPGES:
		return val1 >= val2 ? 1 : 0;
		break;
	case IA_CMPLES:
		return val1 <= val2 ? 1 : 0;
		break;
	case IA_CMPGS:
		return val1 > val2 ? 1 : 0;
		break;
	case IA_CMPLS:
		return val1 < val2 ? 1 : 0;
		break;
	case IA_CMPGEU:
		return (unsigned __int64)val1 >= (unsigned __int64)val2 ? 1 : 0;
		break;
	case IA_CMPLEU:
		return (unsigned __int64)val1 <= (unsigned __int64)val2 ? 1 : 0;
		break;
	case IA_CMPGU:
		return (unsigned __int64)val1 > (unsigned __int64)val2 ? 1 : 0;
		break;
	case IA_CMPLU:
		return (unsigned __int64)val1 < (unsigned __int64)val2 ? 1 : 0;
		break;
	default:
		return 0;
	}
}

static __int64 ConstantRelationalFolding(InterOperator oper, double val1, double val2)
{
	switch (oper)
	{
	case IA_CMPEQ:
		return val1 == val2 ? 1 : 0;
		break;
	case IA_CMPNE:
		return val1 != val2 ? 1 : 0;
		break;
	case IA_CMPGES:
	case IA_CMPGEU:
		return val1 >= val2 ? 1 : 0;
		break;
	case IA_CMPLES:
	case IA_CMPLEU:
		return val1 <= val2 ? 1 : 0;
		break;
	case IA_CMPGS:
	case IA_CMPGU:
		return val1 > val2 ? 1 : 0;
		break;
	case IA_CMPLS:
	case IA_CMPLU:
		return val1 < val2 ? 1 : 0;
		break;
	default:
		return 0;
	}
}

static double ConstantFolding(InterOperator oper, double val1, double val2 = 0.0)
{
	switch (oper)
	{
	case IA_ADD:
		return val1 + val2;
		break;
	case IA_SUB:
		return val1 - val2;
		break;
	case IA_MUL:
		return val1 * val2;
		break;
	case IA_DIVU:
	case IA_DIVS:
		return val1 / val2;
		break;
	case IA_NEG:
		return -val1;
		break;
	case IA_ABS:
		return fabs(val1);
		break;
	case IA_FLOOR:
		return floor(val1);
		break;
	case IA_CEIL:
		return ceil(val1);
		break;

	default:
		return 0;
	}
}

void ValueSet::InsertValue(InterInstruction& ins)
{
	InterInstructionPtr* nins;
	int								i;

	if (num == size)
	{
		size *= 2;
		nins = new InterInstructionPtr[size];
		for (i = 0; i < num; i++)
			nins[i] = instructions[i];
		delete[] instructions;
		instructions = nins;
	}

	instructions[num++] = &ins;
}

static bool MemPtrRange(const InterInstruction* ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset)
{
	while (ins && ins->mem == IM_INDIRECT && ins->code == IC_LEA)
		ins = tvalue[ins->stemp[1]];

	if (ins && (ins->code == IC_CONSTANT || ins->code == IC_LEA))
	{
		mem = ins->mem;
		vindex = ins->vindex;
		offset = ins->ivalue;

		return true;
	}
	else
		return false;
}


static bool MemRange(const InterInstruction * ins, const GrowingInstructionPtrArray& tvalue, InterMemory& mem, int& vindex, int& offset)
{
	if (ins->mem == IM_INDIRECT)
	{
		if (ins->code == IC_LOAD)
			return MemPtrRange(tvalue[ins->stemp[0]], tvalue, mem, vindex, offset);
		else
			return MemPtrRange(tvalue[ins->stemp[1]], tvalue, mem, vindex, offset);
	}
	if (ins)
	{
		mem = ins->mem;
		vindex = ins->vindex;
		offset = ins->ivalue;

		return true;
	}
	else
		return false;
}

static bool StoreAliasing(const InterInstruction * lins, const InterInstruction* sins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals)
{
	InterMemory	lmem, smem;
	int			lvindex, svindex;
	int			loffset, soffset;

	if (MemRange(lins, tvalue, lmem, lvindex, loffset) && MemRange(sins, tvalue, smem, svindex, soffset))
	{
		if (smem == lmem && svindex == lvindex)
		{
			if (soffset + sins->opsize >= loffset && loffset + lins->opsize >= soffset)
				return true;
		}

		return false;
	}

	if (lmem == IM_LOCAL)
		return aliasedLocals[lvindex];

	return true;
}

void ValueSet::UpdateValue(InterInstruction& ins, const GrowingInstructionPtrArray& tvalue, const NumberSet& aliasedLocals)
{
	int	i, value, temp;

	temp = ins.ttemp;

	if (temp >= 0)
	{
		i = 0;
		while (i < num)
		{
			if (temp == instructions[i]->ttemp ||
				temp == instructions[i]->stemp[0] ||
				temp == instructions[i]->stemp[1] ||
				temp == instructions[i]->stemp[2])
			{
				num--;
				if (i < num)
					instructions[i] = instructions[num];
			}
			else
				i++;
		}
	}

	for (i = 0; i < 3; i++)
	{
		temp = ins.stemp[i];
		if (temp >= 0 && tvalue[temp])
		{
			ins.stemp[i] = tvalue[temp]->ttemp;
		}
	}

	switch (ins.code)
	{
	case IC_LOAD:
		i = 0;
		while (i < num &&
			(instructions[i]->code != IC_LOAD ||
				instructions[i]->stemp[0] != ins.stemp[0] ||
				instructions[i]->opsize != ins.opsize))
		{
			i++;
		}

		if (i < num)
		{
			ins.code = IC_LOAD_TEMPORARY;
			ins.stemp[0] = instructions[i]->ttemp;
			ins.stype[0] = instructions[i]->ttype;
			assert(ins.stemp[0] >= 0);
		}
		else
		{
			i = 0;
			while (i < num &&
				(instructions[i]->code != IC_STORE ||
					instructions[i]->stemp[1] != ins.stemp[0] ||
					instructions[i]->opsize != ins.opsize))
			{
				i++;
			}

			if (i < num)
			{
				if (instructions[i]->stemp[0] < 0)
				{
					ins.code = IC_CONSTANT;
					ins.stemp[0] = -1;
					ins.stype[0] = instructions[i]->stype[0];
					ins.ivalue = instructions[i]->siconst[0];
				}
				else
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->stemp[0];
					ins.stype[0] = instructions[i]->stype[0];
					assert(ins.stemp[0] >= 0);
				}
			}
			else
			{
				InsertValue(ins);
			}
		}

		break;
	case IC_STORE:
		i = 0;
		while (i < num)
		{
			if ((instructions[i]->code == IC_LOAD || instructions[i]->code == IC_STORE) && StoreAliasing(instructions[i], &ins, tvalue, aliasedLocals))
			{
				num--;
				if (num > 0)
					instructions[i] = instructions[num];
			}
			else
				i++;
		}

		InsertValue(ins);
		break;
	case IC_COPY:
		i = 0;
		while (i < num)
		{
			if ((instructions[i]->code == IC_LOAD || instructions[i]->code == IC_STORE) && StoreAliasing(instructions[i], &ins, tvalue, aliasedLocals))
			{
				num--;
				if (num > 0)
					instructions[i] = instructions[num];
			}
			else
				i++;
		}

		break;

	case IC_CONSTANT:
		switch (ins.ttype)
		{
		case IT_FLOAT:
			i = 0;
			while (i < num &&
				(instructions[i]->code != IC_CONSTANT ||
					instructions[i]->ttype != ins.ttype ||
					instructions[i]->fvalue != ins.fvalue))
			{
				i++;
			}
			break;
		case IT_POINTER:
			i = 0;
			while (i < num &&
				(instructions[i]->code != IC_CONSTANT ||
					instructions[i]->ttype != ins.ttype ||
					instructions[i]->ivalue != ins.ivalue ||
					instructions[i]->mem != ins.mem ||
					instructions[i]->vindex != ins.vindex))
			{
				i++;
			}
			break;
		default:

			i = 0;
			while (i < num &&
				(instructions[i]->code != IC_CONSTANT ||
					instructions[i]->ttype != ins.ttype ||
					instructions[i]->ivalue != ins.ivalue))
			{
				i++;
			}
		}

		if (i < num)
		{
			ins.code = IC_LOAD_TEMPORARY;
			ins.stemp[0] = instructions[i]->ttemp;
			ins.stype[0] = instructions[i]->ttype;
			assert(ins.stemp[0] >= 0);
		}
		else
		{
			InsertValue(ins);
		}
		break;

	case IC_LEA:
		i = 0;
		while (i < num &&
			(instructions[i]->code != IC_LEA ||
				instructions[i]->stemp[0] != ins.stemp[0] ||
				instructions[i]->stemp[1] != ins.stemp[1]))
		{
			i++;
		}

		if (i < num)
		{
			ins.code = IC_LOAD_TEMPORARY;
			ins.stemp[0] = instructions[i]->ttemp;
			ins.stype[0] = instructions[i]->ttype;
			ins.stemp[1] = -1;
			assert(ins.stemp[0] >= 0);
		}
		else
		{
			InsertValue(ins);
		}
		break;

	case IC_BINARY_OPERATOR:
		switch (ins.stype[0])
		{
		case IT_FLOAT:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT &&
				ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.fvalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->fvalue, tvalue[ins.stemp[0]]->fvalue);
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;

				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_CONSTANT ||
						instructions[i]->ttype != ins.ttype ||
						instructions[i]->fvalue != ins.fvalue))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_BINARY_OPERATOR ||
						instructions[i]->oper != ins.oper ||
						instructions[i]->stemp[0] != ins.stemp[0] ||
						instructions[i]->stemp[1] != ins.stemp[1]))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT &&
				ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->ivalue, tvalue[ins.stemp[0]]->ivalue);
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals);

				return;
			}

			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				if ((ins.oper == IA_ADD || ins.oper == IA_SUB ||
					ins.oper == IA_OR || ins.oper == IA_XOR ||
					ins.oper == IA_SHL || ins.oper == IA_SHR || ins.oper == IA_SAR) && tvalue[ins.stemp[0]]->ivalue == 0 ||
					(ins.oper == IA_MUL || ins.oper == IA_DIVU || ins.oper == IA_DIVS) && tvalue[ins.stemp[0]]->ivalue == 1 ||
					(ins.oper == IA_AND) && tvalue[ins.stemp[0]]->ivalue == -1)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = ins.stemp[1];
					ins.stype[0] = ins.stype[1];
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
				else if ((ins.oper == IA_MUL || ins.oper == IA_AND) && tvalue[ins.stemp[0]]->ivalue == 0)
				{
					ins.code = IC_CONSTANT;
					ins.ivalue = 0;
					ins.stemp[0] = -1;
					ins.stemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
			}
			else if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
			{
				if ((ins.oper == IA_ADD || ins.oper == IA_OR || ins.oper == IA_XOR) && tvalue[ins.stemp[1]]->ivalue == 0 ||
					(ins.oper == IA_MUL) && tvalue[ins.stemp[1]]->ivalue == 1 ||
					(ins.oper == IA_AND) && tvalue[ins.stemp[1]]->ivalue == -1)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
				else if ((ins.oper == IA_MUL || ins.oper == IA_AND ||
					ins.oper == IA_SHL || ins.oper == IA_SHR || ins.oper == IA_SAR) && tvalue[ins.stemp[1]]->ivalue == 0)
				{
					ins.code = IC_CONSTANT;
					ins.ivalue = 0;
					ins.stemp[0] = -1;
					ins.stemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
				else if (ins.oper == IA_SUB && tvalue[ins.stemp[1]]->ivalue == 0)
				{
					ins.code = IC_UNARY_OPERATOR;
					ins.oper = IA_NEG;
					ins.stemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
			}
			else if (ins.stemp[0] == ins.stemp[1])
			{
				if (ins.oper == IA_SUB || ins.oper == IA_XOR)
				{
					ins.code = IC_CONSTANT;
					ins.ivalue = 0;
					ins.stemp[0] = -1;
					ins.stemp[1] = -1;

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
				else if (ins.oper == IA_AND || ins.oper == IA_OR)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);

					UpdateValue(ins, tvalue, aliasedLocals);

					return;
				}
			}
			
			i = 0;
			while (i < num &&
				(instructions[i]->code != IC_BINARY_OPERATOR ||
					instructions[i]->oper != ins.oper ||
					instructions[i]->stemp[0] != ins.stemp[0] ||
					instructions[i]->stemp[1] != ins.stemp[1]))
			{
				i++;
			}

			if (i < num)
			{
				ins.code = IC_LOAD_TEMPORARY;
				ins.stemp[0] = instructions[i]->ttemp;
				ins.stype[0] = instructions[i]->ttype;
				ins.stemp[1] = -1;
				assert(ins.stemp[0] >= 0);
			}
			else
			{
				InsertValue(ins);
			}
			break;
		}
		break;

	case IC_CONVERSION_OPERATOR:
		if (ins.oper == IA_INT2FLOAT)
		{
			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.fvalue = (double)(tvalue[ins.stemp[0]]->ivalue);
				ins.stemp[0] = -1;

				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_CONSTANT ||
						instructions[i]->ttype != ins.ttype ||
						instructions[i]->fvalue != ins.fvalue))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_CONVERSION_OPERATOR ||
						instructions[i]->oper != ins.oper ||
						instructions[i]->stemp[0] != ins.stemp[0]))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
		}
		else if (ins.oper == IA_FLOAT2INT)
		{
		}
		break;

	case IC_UNARY_OPERATOR:
		switch (ins.stype[0])
		{
		case IT_FLOAT:
			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.fvalue = ConstantFolding(ins.oper, tvalue[ins.stemp[0]]->fvalue);
				ins.stemp[0] = -1;

				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_CONSTANT ||
						instructions[i]->ttype != ins.ttype ||
						instructions[i]->fvalue != ins.fvalue))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_UNARY_OPERATOR ||
						instructions[i]->oper != ins.oper ||
						instructions[i]->stemp[0] != ins.stemp[0]))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[0]]->ivalue);
				ins.stemp[0] = -1;

				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_CONSTANT ||
						instructions[i]->ttype != ins.ttype ||
						instructions[i]->ivalue != ins.ivalue))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			else
			{
				i = 0;
				while (i < num &&
					(instructions[i]->code != IC_UNARY_OPERATOR ||
						instructions[i]->oper != ins.oper ||
						instructions[i]->stemp[0] != ins.stemp[0]))
				{
					i++;
				}

				if (i < num)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = instructions[i]->ttemp;
					ins.stype[0] = instructions[i]->ttype;
					assert(ins.stemp[0] >= 0);
				}
				else
				{
					InsertValue(ins);
				}
			}
			break;
		}
		break;

	case IC_RELATIONAL_OPERATOR:
		switch (ins.stype[1])
		{
		case IT_FLOAT:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT &&
				ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ivalue = ConstantRelationalFolding(ins.oper, tvalue[ins.stemp[1]]->fvalue, tvalue[ins.stemp[0]]->fvalue);
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals);
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT &&
				ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->ivalue, tvalue[ins.stemp[0]]->ivalue);
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals);
			}
			else if (ins.stemp[1] == ins.stemp[0])
			{
				ins.code = IC_CONSTANT;

				switch (ins.oper)
				{
				case IA_CMPEQ:
				case IA_CMPGES:
				case IA_CMPLES:
				case IA_CMPGEU:
				case IA_CMPLEU:
					ins.ivalue = 1;
					break;
				case IA_CMPNE:
				case IA_CMPGS:
				case IA_CMPLS:
				case IA_CMPGU:
				case IA_CMPLU:
					ins.ivalue = 0;
					break;
				}
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;

				UpdateValue(ins, tvalue, aliasedLocals);
			}
			break;
		}
		break;
	case IC_CALL:
	case IC_JSR:
		FlushCallAliases();
		break;

	}
}

InterInstruction::InterInstruction(void)
{
	code = IC_NONE;

	ttype = IT_NONE;
	stype[0] = IT_NONE;
	stype[1] = IT_NONE;
	stype[2] = IT_NONE;

	ttemp = INVALID_TEMPORARY;
	stemp[0] = INVALID_TEMPORARY;
	stemp[1] = INVALID_TEMPORARY;
	stemp[2] = INVALID_TEMPORARY;

	exceptionJump = NULL;
}

void InterInstruction::SetCode(const Location& loc, InterCode code)
{
	this->code = code;
	this->loc = loc;
}

static bool TypeInteger(InterType t)
{
	return t == IT_UNSIGNED || t == IT_SIGNED || t == IT_BOOL || t == IT_POINTER;
}

static bool TypeCompatible(InterType t1, InterType t2)
{
	return t1 == t2 || TypeInteger(t1) && TypeInteger(t2);
}

static bool TypeArithmetic(InterType t)
{
	return t == IT_UNSIGNED || t == IT_SIGNED || t == IT_BOOL || t == IT_FLOAT;
}

static InterType TypeCheckArithmecitResult(InterType t1, InterType t2)
{
	if (t1 == IT_FLOAT && t2 == IT_FLOAT)
		return IT_FLOAT;
	else if (TypeInteger(t1) && TypeInteger(t2))
		return IT_SIGNED;
	else
		throw InterCodeTypeMismatchException();
}

static void TypeCheckAssign(InterType& t, InterType s)
{
	if (s == IT_NONE)
		throw InterCodeUninitializedException();
	else if (t == IT_NONE)
		t = s;
	else if (!TypeCompatible(t, s))
		throw InterCodeTypeMismatchException();
}



static void FilterTempUseUsage(NumberSet& requiredTemps, NumberSet& providedTemps, int temp)
{
	if (temp >= 0)
	{
		if (!providedTemps[temp]) requiredTemps += temp;
	}
}

static void FilterTempDefineUsage(NumberSet& requiredTemps, NumberSet& providedTemps, int temp)
{
	if (temp >= 0)
	{
		providedTemps += temp;
	}
}

void InterInstruction::CollectLocalAddressTemps(GrowingIntArray& localTable)
{
	if (code == IC_CONSTANT)
	{
		if (ttype == IT_POINTER && mem == IM_LOCAL)
			localTable[ttemp] = vindex;
	}
	else if (code == IC_LEA)
	{
		if (mem == IM_LOCAL)
			localTable[ttemp] = localTable[stemp[1]];
	}
	else if (code == IC_LOAD_TEMPORARY)
	{
		localTable[ttemp] = localTable[stemp[0]];
	}
}

void InterInstruction::MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals)
{
	if (code == IC_STORE)
	{
		int	l = localTable[stemp[0]];
		if (l >= 0)
			aliasedLocals += l;
	}
}

void InterInstruction::FilterTempUsage(NumberSet& requiredTemps, NumberSet& providedTemps)
{
	FilterTempUseUsage(requiredTemps, providedTemps, stemp[0]);
	FilterTempUseUsage(requiredTemps, providedTemps, stemp[1]);
	FilterTempUseUsage(requiredTemps, providedTemps, stemp[2]);
	FilterTempDefineUsage(requiredTemps, providedTemps, ttemp);
}

void InterInstruction::FilterVarsUsage(const GrowingVariableArray& localVars, NumberSet& requiredVars, NumberSet& providedVars)
{
	if (code == IC_LOAD && mem == IM_LOCAL)
	{
		assert(stemp[0] < 0);
		if (!providedVars[vindex])
			requiredVars += vindex;
	}
	else if (code == IC_STORE && mem == IM_LOCAL)
	{
		assert(stemp[1] < 0);
		if (!providedVars[vindex] && (siconst[1] != 0 || opsize != localVars[vindex].mSize))
			requiredVars += vindex;
		providedVars += vindex;
	}
}

static void PerformTempUseForwarding(int& temp, TempForwardingTable& forwardingTable)
{
	if (temp >= 0)
		temp = forwardingTable[temp];
}

static void PerformTempDefineForwarding(int temp, TempForwardingTable& forwardingTable)
{
	if (temp >= 0)
	{
		forwardingTable.Destroy(temp);
	}
}

void InterInstruction::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	PerformTempUseForwarding(stemp[0], forwardingTable);
	PerformTempUseForwarding(stemp[1], forwardingTable);
	PerformTempUseForwarding(stemp[2], forwardingTable);
	PerformTempDefineForwarding(ttemp, forwardingTable);
	if (code == IC_LOAD_TEMPORARY && ttemp != stemp[0])
	{
		forwardingTable.Build(ttemp, stemp[0]);
	}
}

bool HasSideEffect(InterCode code)
{
	return code == IC_CALL || code == IC_JSR;
}

bool InterInstruction::RemoveUnusedResultInstructions(InterInstruction* pre, NumberSet& requiredTemps, int numStaticTemps)
{
	bool	changed = false;

	if (pre && code == IC_LOAD_TEMPORARY && pre->ttemp == stemp[0] && !requiredTemps[stemp[0]] && pre->ttemp >= numStaticTemps)
	{
		// previous instruction produced result, but it is not needed here
		pre->ttemp = ttemp;

		code = IC_NONE;
		ttemp = -1;
		stemp[0] = -1;
		stemp[1] = -1;
		stemp[2] = -1;

		changed = true;
	}
	else if (ttemp != -1)
	{
		if (!requiredTemps[ttemp] && ttemp >= numStaticTemps)
		{
			if (!HasSideEffect(code))
			{
				code = IC_NONE;
				ttemp = -1;
				stemp[0] = -1;
				stemp[1] = -1;
				stemp[2] = -1;

				changed = true;
			}
			else
			{
				ttemp = -1;

				changed = true;
			}
		}
		else
			requiredTemps -= ttemp;
	}

	if (stemp[0] >= 0) sfinal[0] = !requiredTemps[stemp[0]] && stemp[0] >= numStaticTemps;
	if (stemp[1] >= 0) sfinal[1] = !requiredTemps[stemp[1]] && stemp[1] >= numStaticTemps;
	if (stemp[2] >= 0) sfinal[2] = !requiredTemps[stemp[2]] && stemp[2] >= numStaticTemps;

	if (stemp[0] >= 0) requiredTemps += stemp[0];
	if (stemp[1] >= 0) requiredTemps += stemp[1];
	if (stemp[2] >= 0) requiredTemps += stemp[2];

	return changed;
}

bool InterInstruction::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars, InterInstruction* pre, NumberSet& requiredTemps)
{
	bool	changed = false;

	if (code == IC_LOAD)
	{
		if (mem == IM_LOCAL)
		{
			requiredTemps += vindex;
		}
	}
	else if (code == IC_STORE)
	{
		if (mem == IM_LOCAL)
		{
			if (localVars[vindex].mAliased)
				;
			else if (requiredTemps[vindex])
			{
				if (siconst[1] == 0 && opsize == localVars[vindex].mSize)
					requiredTemps -= vindex;
			}
			else
			{
				code = IC_NONE;
				changed = true;
			}
		}
	}

	return changed;
}

static void DestroySourceValues(int temp, GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
{
	int i, j;
	const	InterInstruction* ins;

	if (temp >= 0)
	{
		i = 0;
		while (i < tvalid.Num())
		{
			j = tvalid.Element(i);

			ins = tvalue[j];

			if (ins->stemp[0] == temp || ins->stemp[1] == temp || ins->stemp[2] == temp)
			{
				tvalue[j] = NULL;
				tvalid -= j;
			}
			else
				i++;
		}
	}
}

void InterInstruction::PerformValueForwarding(GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
{
	DestroySourceValues(ttemp, tvalue, tvalid);

	if (code == IC_LOAD_TEMPORARY)
	{
		if (tvalue[stemp[0]])
		{
			tvalue[ttemp] = tvalue[stemp[0]];
			tvalid += ttemp;
		}
	}
	else
	{
		if (ttemp >= 0)
		{
			tvalue[ttemp] = this;
			tvalid += ttemp;
		}
	}
}

void InterInstruction::LocalRenameRegister(GrowingIntArray& renameTable, int& num, int fixed)
{
	if (stemp[0] >= 0) stemp[0] = renameTable[stemp[0]];
	if (stemp[1] >= 0) stemp[1] = renameTable[stemp[1]];
	if (stemp[2] >= 0) stemp[2] = renameTable[stemp[2]];

	if (ttemp >= fixed)
	{
		renameTable[ttemp] = num;
		ttemp = num++;
	}
}

void InterInstruction::GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	if (stemp[0] >= 0) stemp[0] = renameTable[stemp[0]];
	if (stemp[1] >= 0) stemp[1] = renameTable[stemp[1]];
	if (stemp[2] >= 0) stemp[2] = renameTable[stemp[2]];

	if (ttemp >= 0)
	{
		ttemp = renameTable[ttemp];
		temporaries[ttemp] = ttype;
	}
}

static void UpdateCollisionSet(NumberSet& liveTemps, NumberSet* collisionSets, int temp)
{
	int i;

	if (temp >= 0 && !liveTemps[temp])
	{
		for (i = 0; i < liveTemps.Size(); i++)
		{
			if (liveTemps[i])
			{
				collisionSets[i] += temp;
				collisionSets[temp] += i;
			}
		}

		liveTemps += temp;
	}
}

void InterInstruction::BuildCollisionTable(NumberSet& liveTemps, NumberSet* collisionSets)
{
	if (ttemp >= 0)
	{
		//		if (!liveTemps[ttemp]) __asm int 3
		liveTemps -= ttemp;
	}

	UpdateCollisionSet(liveTemps, collisionSets, stemp[0]);
	UpdateCollisionSet(liveTemps, collisionSets, stemp[1]);
	UpdateCollisionSet(liveTemps, collisionSets, stemp[2]);

	if (exceptionJump)
	{
		int	i;

		for (i = 0; i < exceptionJump->entryRequiredTemps.Size(); i++)
		{
			if (exceptionJump->entryRequiredTemps[i])
				UpdateCollisionSet(liveTemps, collisionSets, i);
		}
	}
}

void InterInstruction::ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	if (stemp[0] >= 0) stemp[0] = renameTable[stemp[0]];
	if (stemp[1] >= 0) stemp[1] = renameTable[stemp[1]];
	if (stemp[2] >= 0) stemp[2] = renameTable[stemp[2]];

	if (ttemp >= 0)
	{
		ttemp = renameTable[ttemp];
		temporaries[ttemp] = ttype;
	}
}


void InterInstruction::CollectActiveTemporaries(FastNumberSet& set)
{
	if (ttemp >= 0) set += ttemp;
	if (stemp[0] >= 0) set += stemp[0];
	if (stemp[1] >= 0) set += stemp[1];
	if (stemp[2] >= 0) set += stemp[2];
}

void InterInstruction::ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries)
{
	if (ttemp >= 0)
	{
		ttemp = set.Index(ttemp);
		temporaries[ttemp] = ttype;
	}
	if (stemp[0] >= 0) stemp[0] = set.Index(stemp[0]);
	if (stemp[1] >= 0) stemp[1] = set.Index(stemp[1]);
	if (stemp[2] >= 0) stemp[2] = set.Index(stemp[2]);
}

void InterInstruction::CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes)
{
	switch (code)
	{
	case IC_LOAD:
		if (mem == IM_LOCAL && stemp[0] < 0)
		{
			localTypes[vindex] = ttype;
			if (opsize == 2)
				simpleLocals += vindex;
			else
				complexLocals += vindex;
		}
		break;
	case IC_STORE:
		if (mem == IM_LOCAL && stemp[1] < 0)
		{
			localTypes[vindex] = stype[0];
			if (opsize == 2)
				simpleLocals += vindex;
			else
				complexLocals += vindex;
		}
		break;
	case IC_LEA:
		if (mem == IM_LOCAL && stemp[1] < 0)
			complexLocals += vindex;
		break;
	case IC_CONSTANT:
		if (ttype == IT_POINTER && mem == IM_LOCAL)
			complexLocals += vindex;
		break;
	}
}

void InterInstruction::SimpleLocalToTemp(int vindex, int temp)
{
	switch (code)
	{
	case IC_LOAD:
		if (mem == IM_LOCAL && stemp[0] < 0 && vindex == this->vindex)
		{
			code = IC_LOAD_TEMPORARY;
			stemp[0] = temp;
			stype[0] = ttype;

			assert(stemp[0] >= 0);

		}
		break;
	case IC_STORE:
		if (mem == IM_LOCAL && stemp[1] < 0 && vindex == this->vindex)
		{
			if (stemp[0] < 0)
			{
				code = IC_CONSTANT;
				ivalue = siconst[0];
			}
			else
			{
				code = IC_LOAD_TEMPORARY;
				assert(stemp[0] >= 0);
			}

			ttemp = temp;
			ttype = stype[0];
		}
		break;
	}
}

void InterInstruction::Disassemble(FILE* file)
{
	if (this->code != IC_NONE)
	{
		static char memchars[] = "NPLGFPITA";

		fprintf(file, "\t");
		switch (this->code)
		{
		case IC_LOAD_TEMPORARY:
		case IC_STORE_TEMPORARY:
			fprintf(file, "MOVE");
			break;
		case IC_BINARY_OPERATOR:
			fprintf(file, "BINOP");
			break;
		case IC_UNARY_OPERATOR:
			fprintf(file, "UNOP");
			break;
		case IC_RELATIONAL_OPERATOR:
			fprintf(file, "RELOP");
			break;
		case IC_CONVERSION_OPERATOR:
			fprintf(file, "CONV");
			break;
		case IC_STORE:
			fprintf(file, "STORE%c%d", memchars[mem], opsize);
			break;
		case IC_LOAD:
			fprintf(file, "LOAD%c%d", memchars[mem], opsize);
			break;
		case IC_COPY:
			fprintf(file, "COPY%c", memchars[mem]);
			break;
		case IC_LEA:
			fprintf(file, "LEA%c", memchars[mem]);
			break;
		case IC_TYPECAST:
			fprintf(file, "CAST");
			break;
		case IC_CONSTANT:
			fprintf(file, "CONST");
			break;
		case IC_BRANCH:
			fprintf(file, "BRANCH");
			break;
		case IC_JUMP:
			fprintf(file, "JUMP");
			break;
		case IC_PUSH_FRAME:
			fprintf(file, "PUSHF\t%d", int(ivalue));
			break;
		case IC_POP_FRAME:
			fprintf(file, "POPF\t%d", int(ivalue));
			break;
		case IC_CALL:
			fprintf(file, "CALL");
			break;
		case IC_JSR:
			fprintf(file, "JSR");
			break;
		case IC_RETURN_VALUE:
			fprintf(file, "RETV");
			break;
		case IC_RETURN_STRUCT:
			fprintf(file, "RETS");
			break;
		case IC_RETURN:
			fprintf(file, "RET");
			break;
		}
		static char typechars[] = "NUSFPB";

		fprintf(file, "\t");
		if (ttemp >= 0) fprintf(file, "R%d(%c)", ttemp, typechars[ttype]);
		fprintf(file, "\t<-\t");
		if (stemp[2] >= 0) fprintf(file, "R%d(%c%c), ", stemp[2], typechars[stype[2]], sfinal[2] ? 'F' : '-');
		if (stemp[1] >= 0) fprintf(file, "R%d(%c%c), ", stemp[1], typechars[stype[1]], sfinal[1] ? 'F' : '-');
		if (stemp[0] >= 0) fprintf(file, "R%d(%c%c)", stemp[0], typechars[stype[0]], sfinal[0] ? 'F' : '-');
		if (this->code == IC_CONSTANT)
		{
			if (ttype == IT_POINTER)
				fprintf(file, "C%d", opsize);
			else if (ttype == IT_FLOAT)
				fprintf(file, "C%f", fvalue);
			else
				fprintf(file, "C%I64d", ivalue);
		}
		if (this->exceptionJump)
		{
			fprintf(file, " EX: %d", this->exceptionJump->num);
		}

		fprintf(file, "\n");
	}
}

InterCodeBasicBlock::InterCodeBasicBlock(void)
	: code(InterInstruction()), entryRenameTable(-1), exitRenameTable(-1)
{
}

InterCodeBasicBlock::~InterCodeBasicBlock(void)
{
}


void InterCodeBasicBlock::Append(InterInstruction & code)
{
	this->code.Push(code);
}

void InterCodeBasicBlock::Close(InterCodeBasicBlock* trueJump, InterCodeBasicBlock* falseJump)
{
	this->trueJump = trueJump;
	this->falseJump = falseJump;
	this->numEntries = 0;
}


void InterCodeBasicBlock::CollectEntries(void)
{
	numEntries++;
	if (!visited)
	{
		visited = true;

		if (trueJump) trueJump->CollectEntries();
		if (falseJump) falseJump->CollectEntries();
	}
}

void InterCodeBasicBlock::GenerateTraces(void)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (;;)
		{
			if (trueJump && trueJump->code.Size() == 1 && trueJump->code[0].code == IC_JUMP)
			{
				trueJump->numEntries--;
				trueJump = trueJump->trueJump;
				if (trueJump)
					trueJump->numEntries++;
			}
			else if (falseJump && falseJump->code.Size() == 1 && falseJump->code[0].code == IC_JUMP)
			{
				falseJump->numEntries--;
				falseJump = falseJump->trueJump;
				if (falseJump)
					falseJump->numEntries++;
			}
			else if (trueJump && !falseJump && ((trueJump->code.Size() < 10 && trueJump->code.Size() > 1) || trueJump->numEntries == 1))
			{
				trueJump->numEntries--;

				code.Pop();
				for (i = 0; i < trueJump->code.Size(); i++)
					code.Push(trueJump->code[i]);

				falseJump = trueJump->falseJump;
				trueJump = trueJump->trueJump;

				if (trueJump)
					trueJump->numEntries++;
				if (falseJump)
					falseJump->numEntries++;
			}
			else
				break;
		}

		if (trueJump) trueJump->GenerateTraces();
		if (falseJump) falseJump->GenerateTraces();
	}
}

static bool IsSimpleAddressMultiply(int val)
{
	switch (val)
	{
	case 1:		//	SHR	3
	case 2:		// SHR	2
	case 4:		// SHR	1
	case 8:
	case 16:		// * 2
	case 32:		// * 4
	case 64:		// * 8
	case 128:	// LEA	r * 2, * 8
	case 192:	// LEA	r, r * 2, * 8
	case 256:	// LEA	r * 4, * 8
	case 512:	// LEA	r * 8, * 8
		return true;
	}

	return false;
}

static void OptimizeAddress(InterInstruction& ins, const GrowingInstructionPtrArray& tvalue, int offset)
{
	ins.siconst[offset] = 0;

	if (ins.stemp[offset] >= 0 && tvalue[ins.stemp[offset]])
	{
		if (tvalue[ins.stemp[offset]]->code == IC_CONSTANT)
		{
			ins.siconst[offset] = tvalue[ins.stemp[offset]]->ivalue;
			ins.vindex = tvalue[ins.stemp[offset]]->vindex;
			ins.mem = tvalue[ins.stemp[offset]]->mem;
			ins.stemp[offset] = -1;
		}
	}
}


void InterCodeBasicBlock::CheckValueUsage(InterInstruction& ins, const GrowingInstructionPtrArray& tvalue)
{
	switch (ins.code)
	{
	case IC_CALL:
	case IC_JSR:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			ins.mem = tvalue[ins.stemp[0]]->mem;
			ins.vindex = tvalue[ins.stemp[0]]->vindex;
			ins.opsize = tvalue[ins.stemp[0]]->opsize;
			ins.stemp[0] = -1;
		}

		break;
	case IC_LOAD_TEMPORARY:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			switch (ins.stype[0])
			{
			case IT_FLOAT:
				ins.code = IC_CONSTANT;
				ins.fvalue = tvalue[ins.stemp[0]]->fvalue;
				ins.stemp[0] = -1;
				break;
			case IT_POINTER:
				ins.code = IC_CONSTANT;
				ins.mem = tvalue[ins.stemp[0]]->mem;
				ins.vindex = tvalue[ins.stemp[0]]->vindex;
				ins.ivalue = tvalue[ins.stemp[0]]->ivalue;
				ins.opsize = tvalue[ins.stemp[0]]->opsize;
				ins.stemp[0] = -1;
				break;
			default:
				ins.code = IC_CONSTANT;
				ins.ivalue = tvalue[ins.stemp[0]]->ivalue;
				ins.stemp[0] = -1;
				break;
			}
		}
		break;

	case IC_LOAD:
		OptimizeAddress(ins, tvalue, 0);
		break;
	case IC_STORE:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			switch (ins.stype[0])
			{
			case IT_FLOAT:
				break;
			case IT_POINTER:
				break;
			default:
				if (ins.stype[0] == IT_UNSIGNED)
					ins.siconst[0] = unsigned short (tvalue[ins.stemp[0]]->ivalue);
				else
					ins.siconst[0] = tvalue[ins.stemp[0]]->ivalue;
				ins.stemp[0] = -1;
				break;
			}
		}
		OptimizeAddress(ins, tvalue, 1);
		break;
	case IC_LEA:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ttype = IT_POINTER;
				ins.mem = tvalue[ins.stemp[1]]->mem;
				ins.vindex = tvalue[ins.stemp[1]]->vindex;
				ins.ivalue = tvalue[ins.stemp[1]]->ivalue + tvalue[ins.stemp[0]]->ivalue;
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;
			}
			else if (tvalue[ins.stemp[0]]->ivalue == 0)
			{
				ins.code = IC_LOAD_TEMPORARY;
				ins.stype[0] = ins.stype[1];
				ins.stemp[0] = ins.stemp[1];
				ins.stemp[1] = -1;
				assert(ins.stemp[0] >= 0);
			}
		}
		break;
	case IC_TYPECAST:
		if (ins.stype[0] == ins.ttype)
		{
			ins.code = IC_LOAD_TEMPORARY;
			assert(ins.stemp[0] >= 0);
		}
		else if ((ins.stype[0] == IT_SIGNED || ins.stype[0] == IT_UNSIGNED) && ins.ttype == IT_POINTER)
		{
			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ttype = IT_POINTER;
				ins.mem = IM_ABSOLUTE;
				ins.vindex = 0;
				ins.ivalue = tvalue[ins.stemp[0]]->ivalue;
				ins.stemp[0] = -1;
			}
		}
		break;
	case IC_RETURN_VALUE:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			switch (ins.stype[0])
			{
			case IT_FLOAT:
				break;
			case IT_POINTER:
				break;
			default:
				if (ins.stype[0] == IT_UNSIGNED)
					ins.siconst[0] = unsigned short(tvalue[ins.stemp[0]]->ivalue);
				else
					ins.siconst[0] = tvalue[ins.stemp[0]]->ivalue;
				ins.stemp[0] = -1;
				break;
			}
		}
		break;
	case IC_BINARY_OPERATOR:
		switch (ins.stype[0])
		{
		case IT_FLOAT:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
			{
				if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
				{
					ins.code = IC_CONSTANT;
					ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->fvalue, tvalue[ins.stemp[0]]->fvalue);
					ins.stemp[0] = -1;
					ins.stemp[1] = -1;
				}
				else
				{
					if (ins.oper == IA_ADD && tvalue[ins.stemp[1]]->fvalue == 0)
					{
						ins.code = IC_LOAD_TEMPORARY;
						assert(ins.stemp[0] >= 0);
					}
					else if (ins.oper == IA_MUL)
					{
						if (tvalue[ins.stemp[1]]->fvalue == 1.0)
						{
							ins.code = IC_LOAD_TEMPORARY;
							assert(ins.stemp[0] >= 0);
						}
						else if (tvalue[ins.stemp[1]]->fvalue == 0.0)
						{
							ins.code = IC_CONSTANT;
							ins.fvalue = 0.0;
							ins.stemp[0] = -1;
							ins.stemp[1] = -1;
						}
						else if (tvalue[ins.stemp[1]]->fvalue == 2.0)
						{
							ins.oper = IA_ADD;
							ins.stemp[1] = ins.stemp[0];
							assert(ins.stemp[0] >= 0);
						}
					}
				}
			}
			else if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				if (ins.oper == IA_ADD && tvalue[ins.stemp[0]]->fvalue == 0)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = ins.stemp[1];
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);
				}
				else if (ins.oper == IA_MUL)
				{
					if (tvalue[ins.stemp[0]]->fvalue == 1.0)
					{
						ins.code = IC_LOAD_TEMPORARY;
						ins.stemp[0] = ins.stemp[1];
						ins.stemp[1] = -1;
						assert(ins.stemp[0] >= 0);
					}
					else if (tvalue[ins.stemp[0]]->fvalue == 0.0)
					{
						ins.code = IC_CONSTANT;
						ins.fvalue = 0.0;
						ins.stemp[0] = -1;
						ins.stemp[1] = -1;
					}
					else if (tvalue[ins.stemp[0]]->fvalue == 2.0)
					{
						ins.oper = IA_ADD;
						ins.stemp[0] = ins.stemp[1];
						assert(ins.stemp[0] >= 0);
					}
				}
			}
			break;
		case IT_POINTER:
			break;
		default:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
			{
				if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
				{
					ins.code = IC_CONSTANT;
					ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->ivalue, tvalue[ins.stemp[0]]->ivalue);
					ins.stemp[0] = -1;
					ins.stemp[1] = -1;
				}
				else
				{
					ins.siconst[1] = tvalue[ins.stemp[1]]->ivalue;
					ins.stemp[1] = -1;
#if 1
					if (ins.oper == IA_ADD && ins.siconst[1] == 0)
					{
						ins.code = IC_LOAD_TEMPORARY;
						assert(ins.stemp[0] >= 0);
					}
					else if (ins.oper == IA_MUL)
					{
						if (ins.siconst[1] == 1)
						{
							ins.code = IC_LOAD_TEMPORARY;
							assert(ins.stemp[0] >= 0);
						}
						else if (ins.siconst[1] == 2)
						{
							ins.oper = IA_SHL;
ins.stemp[1] = ins.stemp[0];
ins.stype[1] = ins.stype[0];
ins.stemp[0] = -1;
ins.siconst[0] = 1;
						}
						else if (ins.siconst[1] == 4)
						{
						ins.oper = IA_SHL;
						ins.stemp[1] = ins.stemp[0];
						ins.stype[1] = ins.stype[0];
						ins.stemp[0] = -1;
						ins.siconst[0] = 2;
						}
						else if (ins.siconst[1] == 8)
						{
						ins.oper = IA_SHL;
						ins.stemp[1] = ins.stemp[0];
						ins.stype[1] = ins.stype[0];
						ins.stemp[0] = -1;
						ins.siconst[0] = 3;
						}

					}
#endif
				}
			}
			else if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
			ins.siconst[0] = tvalue[ins.stemp[0]]->ivalue;
			ins.stemp[0] = -1;
#if 1
			if (ins.oper == IA_ADD && ins.siconst[0] == 0)
			{
				ins.code = IC_LOAD_TEMPORARY;
				ins.stemp[0] = ins.stemp[1];
				ins.stemp[1] = -1;
				assert(ins.stemp[0] >= 0);
			}
			else if (ins.oper == IA_MUL)
			{
				if (ins.siconst[0] == 1)
				{
					ins.code = IC_LOAD_TEMPORARY;
					ins.stemp[0] = ins.stemp[1];
					ins.stemp[1] = -1;
					assert(ins.stemp[0] >= 0);
				}
				else if (ins.siconst[0] == 2)
				{
					ins.oper = IA_SHL;
					ins.siconst[0] = 1;
				}
				else if (ins.siconst[0] == 4)
				{
					ins.oper = IA_SHL;
					ins.siconst[0] = 2;
				}
				else if (ins.siconst[0] == 8)
				{
					ins.oper = IA_SHL;
					ins.siconst[0] = 3;
				}
			}
#endif
			}

			if (ins.stemp[1] < 0 && ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins.stemp[0]];
				if (ins.oper == pins->oper && (ins.oper == IA_ADD || ins.oper == IA_MUL || ins.oper == IA_AND || ins.oper == IA_OR))
				{
					if (pins->stemp[1] < 0)
					{
						ins.siconst[1] = ConstantFolding(ins.oper, ins.siconst[1], pins->siconst[1]);
						ins.stemp[0] = pins->stemp[0];
					}
					else if (pins->stemp[0] < 0)
					{
						ins.siconst[1] = ConstantFolding(ins.oper, ins.siconst[1], pins->siconst[0]);
						ins.stemp[0] = pins->stemp[1];
					}
				}
			}
			else if (ins.stemp[0] < 0 && ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_BINARY_OPERATOR)
			{
				InterInstruction* pins = tvalue[ins.stemp[1]];
				if (ins.oper == pins->oper && (ins.oper == IA_ADD || ins.oper == IA_MUL || ins.oper == IA_AND || ins.oper == IA_OR))
				{
					if (pins->stemp[1] < 0)
					{
						ins.siconst[0] = ConstantFolding(ins.oper, ins.siconst[0], pins->siconst[1]);
						ins.stemp[1] = pins->stemp[0];
					}
					else if (pins->stemp[0] < 0)
					{
						ins.siconst[0] = ConstantFolding(ins.oper, ins.siconst[0], pins->siconst[0]);
						ins.stemp[1] = pins->stemp[1];
					}
				}
				else if (ins.oper == IA_SHL && (pins->oper == IA_SHR || pins->oper == IA_SAR) && pins->stemp[0] < 0 && ins.siconst[0] == pins->siconst[0])
				{
					ins.oper = IA_AND;
					ins.siconst[0] = -1LL << ins.siconst[0];
					ins.stemp[1] = pins->stemp[1];
				}
			}

			break;
		}
		break;
	case IC_UNARY_OPERATOR:
		switch (ins.stype[0])
		{
		case IT_FLOAT:
			if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.fvalue = ConstantFolding(ins.oper, tvalue[ins.stemp[0]]->fvalue);
				ins.stemp[0] = -1;
			}
			break;
		case IT_POINTER:
			break;
		default:
			break;
		}
		break;
	case IC_RELATIONAL_OPERATOR:
		switch (ins.stype[1])
		{
		case IT_FLOAT:
			break;
		case IT_POINTER:
			if (ins.oper == IA_CMPEQ || ins.oper == IA_CMPNE)
			{
				if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
				{
					ins.opsize = tvalue[ins.stemp[0]]->opsize;
					ins.stemp[0] = -1;
				}
				else if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
				{
					ins.opsize = tvalue[ins.stemp[1]]->opsize;
					ins.stemp[1] = -1;
				}
			}
			break;
		default:
			if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT &&
				ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
			{
				ins.code = IC_CONSTANT;
				ins.ivalue = ConstantFolding(ins.oper, tvalue[ins.stemp[1]]->ivalue, tvalue[ins.stemp[0]]->ivalue);
				ins.stemp[0] = -1;
				ins.stemp[1] = -1;
			}
			else
			{
				if (ins.stemp[1] >= 0 && tvalue[ins.stemp[1]] && tvalue[ins.stemp[1]]->code == IC_CONSTANT)
				{
					ins.siconst[1] = tvalue[ins.stemp[1]]->ivalue;
					ins.stemp[1] = -1;
				}
				else if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
				{
					ins.siconst[0] = tvalue[ins.stemp[0]]->ivalue;
					ins.stemp[0] = -1;
				}
			}
			break;
		}
		break;
	case IC_BRANCH:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			ins.siconst[0] = tvalue[ins.stemp[0]]->ivalue;
			ins.stemp[0] = -1;
		}
		break;
	case IC_PUSH_FRAME:
		if (ins.stemp[0] >= 0 && tvalue[ins.stemp[0]] && tvalue[ins.stemp[0]]->code == IC_CONSTANT)
		{
			ins.spconst[0] = tvalue[ins.stemp[0]]->opsize;
			ins.stemp[0] = -1;
		}
		break;
	}
}


void InterCodeBasicBlock::CollectLocalAddressTemps(GrowingIntArray& localTable)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
			code[i].CollectLocalAddressTemps(localTable);

		if (trueJump) trueJump->CollectLocalAddressTemps(localTable);
		if (falseJump) falseJump->CollectLocalAddressTemps(localTable);
	}
}

void InterCodeBasicBlock::MarkAliasedLocalTemps(const GrowingIntArray& localTable, NumberSet& aliasedLocals)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
			code[i].MarkAliasedLocalTemps(localTable, aliasedLocals);

		if (trueJump) trueJump->MarkAliasedLocalTemps(localTable, aliasedLocals);
		if (falseJump) falseJump->MarkAliasedLocalTemps(localTable, aliasedLocals);
	}
}

void InterCodeBasicBlock::BuildLocalTempSets(int num, int numFixed)
{
	int i;

	if (!visited)
	{
		visited = true;

		localRequiredTemps = NumberSet(num);
		localProvidedTemps = NumberSet(num);

		entryRequiredTemps = NumberSet(num);
		entryProvidedTemps = NumberSet(num);
		exitRequiredTemps = NumberSet(num);
		exitProvidedTemps = NumberSet(num);

		for (i = 0; i < code.Size(); i++)
		{
			code[i].FilterTempUsage(localRequiredTemps, localProvidedTemps);
		}

		entryRequiredTemps = localRequiredTemps;
		exitProvidedTemps = localProvidedTemps;

		for (i = 0; i < numFixed; i++)
		{
			entryRequiredTemps += i;
			exitProvidedTemps += i;
		}

		if (trueJump) trueJump->BuildLocalTempSets(num, numFixed);
		if (falseJump) falseJump->BuildLocalTempSets(num, numFixed);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedTempSet(NumberSet fromProvidedTemps)
{
	if (!visited || !(fromProvidedTemps <= entryProvidedTemps))
	{
		entryProvidedTemps |= fromProvidedTemps;
		fromProvidedTemps |= exitProvidedTemps;

		visited = true;

		if (trueJump) trueJump->BuildGlobalProvidedTempSet(fromProvidedTemps);
		if (falseJump) falseJump->BuildGlobalProvidedTempSet(fromProvidedTemps);
	}
}

void InterCodeBasicBlock::PerformTempForwarding(TempForwardingTable& forwardingTable)
{
	int i;

	if (!visited)
	{
		visited = true;

		TempForwardingTable	localForwardingTable(forwardingTable);

		if (numEntries > 1)
			localForwardingTable.Reset();

		for (i = 0; i < code.Size(); i++)
		{
			code[i].PerformTempForwarding(localForwardingTable);
		}

		if (trueJump) trueJump->PerformTempForwarding(localForwardingTable);
		if (falseJump) falseJump->PerformTempForwarding(localForwardingTable);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredTempSet(NumberSet& fromRequiredTemps)
{
	bool revisit = false;
	int	i;

	if (!visited)
	{
		visited = true;

		NumberSet	newRequiredTemps(exitRequiredTemps);

		if (trueJump && trueJump->BuildGlobalRequiredTempSet(newRequiredTemps)) revisit = true;
		if (falseJump && falseJump->BuildGlobalRequiredTempSet(newRequiredTemps)) revisit = true;

		if (!(newRequiredTemps <= exitRequiredTemps))
		{
			revisit = true;

			exitRequiredTemps = newRequiredTemps;
			newRequiredTemps -= localProvidedTemps;
			entryRequiredTemps |= newRequiredTemps;
		}

	}

	fromRequiredTemps |= entryRequiredTemps;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedResultInstructions(int numStaticTemps)
{
	bool	changed = false;

	if (!visited)
	{
		visited = true;

		NumberSet		requiredTemps(exitRequiredTemps);
		int i;

		for (i = 0; i < numStaticTemps; i++)
			requiredTemps += i;

		for (i = code.Size() - 1; i > 0; i--)
		{
			if (code[i].RemoveUnusedResultInstructions(&(code[i - 1]), requiredTemps, numStaticTemps))
				changed = true;
		}
		if (code[0].RemoveUnusedResultInstructions(NULL, requiredTemps, numStaticTemps))
			changed = true;

		if (trueJump)
		{
			if (trueJump->RemoveUnusedResultInstructions(numStaticTemps))
				changed = true;
		}
		if (falseJump)
		{
			if (falseJump->RemoveUnusedResultInstructions(numStaticTemps))
				changed = true;
		}
	}

	return changed;
}


void InterCodeBasicBlock::BuildLocalVariableSets(const GrowingVariableArray& localVars)
{
	int i;

	if (!visited)
	{
		visited = true;

		mLocalRequiredVars = NumberSet(localVars.Size());
		mLocalProvidedVars = NumberSet(localVars.Size());

		mEntryRequiredVars = NumberSet(localVars.Size());
		mEntryProvidedVars = NumberSet(localVars.Size());
		mExitRequiredVars = NumberSet(localVars.Size());
		mExitProvidedVars = NumberSet(localVars.Size());

		for (i = 0; i < code.Size(); i++)
		{
			code[i].FilterVarsUsage(localVars, mLocalRequiredVars, mLocalProvidedVars);
		}

		mEntryRequiredVars = mLocalRequiredVars;
		mExitProvidedVars = mLocalProvidedVars;

		if (trueJump) trueJump->BuildLocalVariableSets(localVars);
		if (falseJump) falseJump->BuildLocalVariableSets(localVars);
	}
}

void InterCodeBasicBlock::BuildGlobalProvidedVariableSet(const GrowingVariableArray& localVars, NumberSet fromProvidedVars)
{
	if (!visited || !(fromProvidedVars <= mEntryProvidedVars))
	{
		mEntryProvidedVars |= fromProvidedVars;
		fromProvidedVars |= mExitProvidedVars;

		visited = true;

		if (trueJump) trueJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars);
		if (falseJump) falseJump->BuildGlobalProvidedVariableSet(localVars, fromProvidedVars);
	}
}

bool InterCodeBasicBlock::BuildGlobalRequiredVariableSet(const GrowingVariableArray& localVars, NumberSet& fromRequiredVars)
{
	bool revisit = false;
	int	i;

	if (!visited)
	{
		visited = true;

		NumberSet	newRequiredVars(mExitRequiredVars);

		if (trueJump && trueJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars)) revisit = true;
		if (falseJump && falseJump->BuildGlobalRequiredVariableSet(localVars, newRequiredVars)) revisit = true;

		if (!(newRequiredVars <= mExitRequiredVars))
		{
			revisit = true;

			mExitRequiredVars = newRequiredVars;
			newRequiredVars -= mLocalProvidedVars;
			mEntryRequiredVars |= newRequiredVars;
		}

	}

	fromRequiredVars |= mEntryRequiredVars;

	return revisit;
}

bool InterCodeBasicBlock::RemoveUnusedStoreInstructions(const GrowingVariableArray& localVars)
{
	bool	changed = false;

	if (!visited)
	{
		visited = true;

		NumberSet		requiredVars(mExitRequiredVars);
		int i;

		for (i = code.Size() - 1; i > 0; i--)
		{
			if (code[i].RemoveUnusedStoreInstructions(localVars, &(code[i - 1]), requiredVars))
				changed = true;
		}
		if (code[0].RemoveUnusedStoreInstructions(localVars, nullptr, requiredVars))
			changed = true;

		if (trueJump)
		{
			if (trueJump->RemoveUnusedStoreInstructions(localVars))
				changed = true;
		}
		if (falseJump)
		{
			if (falseJump->RemoveUnusedStoreInstructions(localVars))
				changed = true;
		}
	}

	return changed;

}

void InterCodeBasicBlock::PerformValueForwarding(const GrowingInstructionPtrArray& tvalue, const ValueSet& values, FastNumberSet& tvalid, const NumberSet& aliasedLocals)
{
	int i;

	if (!visited)
	{
		GrowingInstructionPtrArray	ltvalue(tvalue);
		ValueSet					lvalues(values);

		visited = true;

		tvalid.Clear();

		if (numEntries != 1)
		{
			lvalues.FlushAll();
			ltvalue.Clear();
		}
		else
		{
			for (i = 0; i < ltvalue.Size(); i++)
			{
				if (ltvalue[i])
					tvalid += i;
			}
		}

		for (i = 0; i < code.Size(); i++)
		{
			lvalues.UpdateValue(code[i], ltvalue, aliasedLocals);
			code[i].PerformValueForwarding(ltvalue, tvalid);
		}

		if (trueJump) trueJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals);
		if (falseJump) falseJump->PerformValueForwarding(ltvalue, lvalues, tvalid, aliasedLocals);
	}
}

void InterCodeBasicBlock::PerformMachineSpecificValueUsageCheck(const GrowingInstructionPtrArray& tvalue, FastNumberSet& tvalid)
{
	int i;

	if (!visited)
	{
		visited = true;

		GrowingInstructionPtrArray ltvalue(tvalue);

		tvalid.Clear();

		if (numEntries != 1)
			ltvalue.Clear();
		else
		{
			for (i = 0; i < tvalue.Size(); i++)
			{
				if (ltvalue[i])
					tvalid += i;
			}
		}

		for (i = 0; i < code.Size(); i++)
		{
			CheckValueUsage(code[i], ltvalue);
			code[i].PerformValueForwarding(ltvalue, tvalid);
		}

		if (trueJump) trueJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid);
		if (falseJump) falseJump->PerformMachineSpecificValueUsageCheck(ltvalue, tvalid);
	}
}

static void Union(GrowingIntArray& table, int i, int j)
{
	int k, l;

	k = table[j];
	while (j != k)
	{
		l = table[k];
		table[j] = l;
		j = k; k = l;
	}

	table[j] = table[i];
}

static int Find(GrowingIntArray& table, int i)
{
	int j, k, l;

	j = i;
	k = table[j];
	while (j != k)
	{
		l = table[k];
		table[j] = l;
		j = k; k = l;
	}

	return j;
}


void InterCodeBasicBlock::LocalRenameRegister(const GrowingIntArray& renameTable, int& num, int fixed)
{
	int i;

	if (!visited)
	{
		visited = true;

		entryRenameTable.SetSize(renameTable.Size());
		exitRenameTable.SetSize(renameTable.Size());

		for (i = 0; i < renameTable.Size(); i++)
		{
			if (i < fixed)
			{
				entryRenameTable[i] = i;
				exitRenameTable[i] = i;
			}
			else if (entryRequiredTemps[i])
			{
				entryRenameTable[i] = renameTable[i];
				exitRenameTable[i] = renameTable[i];
			}
			else
			{
				entryRenameTable[i] = -1;
				exitRenameTable[i] = -1;
			}
		}

		for (i = 0; i < code.Size(); i++)
		{
			code[i].LocalRenameRegister(exitRenameTable, num, fixed);
		}

		if (trueJump) trueJump->LocalRenameRegister(exitRenameTable, num, fixed);
		if (falseJump) falseJump->LocalRenameRegister(exitRenameTable, num, fixed);
	}
}

void InterCodeBasicBlock::BuildGlobalRenameRegisterTable(const GrowingIntArray& renameTable, GrowingIntArray& globalRenameTable)
{
	int i;

	for (i = 0; i < renameTable.Size(); i++)
	{
		if (renameTable[i] >= 0 && entryRenameTable[i] >= 0 && renameTable[i] != entryRenameTable[i])
		{
			Union(globalRenameTable, renameTable[i], entryRenameTable[i]);
		}
	}

	if (!visited)
	{
		visited = true;

		if (trueJump) trueJump->BuildGlobalRenameRegisterTable(exitRenameTable, globalRenameTable);
		if (falseJump) falseJump->BuildGlobalRenameRegisterTable(exitRenameTable, globalRenameTable);
	}
}

void InterCodeBasicBlock::GlobalRenameRegister(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].GlobalRenameRegister(renameTable, temporaries);
		}

		if (trueJump) trueJump->GlobalRenameRegister(renameTable, temporaries);
		if (falseJump) falseJump->GlobalRenameRegister(renameTable, temporaries);
	}
}

void InterCodeBasicBlock::BuildCollisionTable(NumberSet* collisionSets)
{
	if (!visited)
	{
		visited = true;

		NumberSet		requiredTemps(exitRequiredTemps);
		int i, j;

		for (i = 0; i < exitRequiredTemps.Size(); i++)
		{
			if (exitRequiredTemps[i])
			{
				for (j = 0; j < exitRequiredTemps.Size(); j++)
				{
					if (exitRequiredTemps[j])
					{
						collisionSets[i] += j;
					}
				}
			}
		}

		for (i = code.Size() - 1; i >= 0; i--)
		{
			code[i].BuildCollisionTable(requiredTemps, collisionSets);
		}

		if (trueJump) trueJump->BuildCollisionTable(collisionSets);
		if (falseJump) falseJump->BuildCollisionTable(collisionSets);
	}
}

void InterCodeBasicBlock::ReduceTemporaries(const GrowingIntArray& renameTable, GrowingTypeArray& temporaries)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].ReduceTemporaries(renameTable, temporaries);
		}

		if (trueJump) trueJump->ReduceTemporaries(renameTable, temporaries);
		if (falseJump) falseJump->ReduceTemporaries(renameTable, temporaries);
	}
}

static void UseGlobal(GrowingVariableArray& globalVars, int index)
{
	if (!globalVars[index].mUsed)
	{
		globalVars[index].mUsed = true;
		for (int i = 0; i < globalVars[index].mNumReferences; i++)
		{
			if (!globalVars[index].mReferences[i].mFunction)
				UseGlobal(globalVars, globalVars[index].mReferences[i].mIndex);
		}
	}
}

void InterCodeModule::UseGlobal(int index)
{
	if (!mGlobalVars[index].mUsed)
	{
		mGlobalVars[index].mUsed = true;
		for (int i = 0; i < mGlobalVars[index].mNumReferences; i++)
		{
			if (!mGlobalVars[index].mReferences[i].mFunction)
				UseGlobal( mGlobalVars[index].mReferences[i].mIndex);
		}
	}
}

void InterCodeBasicBlock::MapVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			bool	found = false;

			switch (code[i].code)
			{
			case IC_STORE:
			case IC_LOAD:
			case IC_CONSTANT:
			case IC_JSR:
				if (code[i].mem == IM_GLOBAL)
				{
					UseGlobal(globalVars, code[i].vindex);
				}
				else if (code[i].mem == IM_LOCAL)
				{
					localVars[code[i].vindex].mUsed = true;
				}
				break;
			}
		}

		if (trueJump) trueJump->MapVariables(globalVars, localVars);
		if (falseJump) falseJump->MapVariables(globalVars, localVars);
	}
}

void InterCodeBasicBlock::CollectOuterFrame(int level, int& size)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			if (code[i].code == IC_PUSH_FRAME)
			{
				level++;
				if (level == 1)
				{
					if (code[i].ivalue > size)
						size = code[i].ivalue;
					code[i].code = IC_NONE;
				}
			}
			else if (code[i].code == IC_POP_FRAME)
			{
				if (level == 1)
				{
					code[i].code = IC_NONE;
				}
				level--;
			}
		}

		if (trueJump) trueJump->CollectOuterFrame(level, size);
		if (falseJump) falseJump->CollectOuterFrame(level, size);
	}
}

bool InterCodeBasicBlock::IsLeafProcedure(void)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
			if (code[i].code == IC_CALL)
				return false;

		if (trueJump && !trueJump->IsLeafProcedure())
			return false;
		if (falseJump && !falseJump->IsLeafProcedure())
			return false;
	}

	return true;
}

void InterCodeBasicBlock::CollectVariables(GrowingVariableArray& globalVars, GrowingVariableArray& localVars)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			bool	found = false;

			switch (code[i].code)
			{
			case IC_STORE:
			case IC_LOAD:							
			case IC_CONSTANT:
			case IC_JSR:
				if (code[i].mem == IM_LOCAL)
				{
					int	size = code[i].opsize + code[i].ivalue;
					if (size > localVars[code[i].vindex].mSize)
						localVars[code[i].vindex].mSize = size;
					if (code[i].code == IC_CONSTANT)
						localVars[code[i].vindex].mAliased = true;
				}
				break;
			}
		}

		if (trueJump) trueJump->CollectVariables(globalVars, localVars);
		if (falseJump) falseJump->CollectVariables(globalVars, localVars);
	}
}

void InterCodeBasicBlock::CollectSimpleLocals(FastNumberSet& complexLocals, FastNumberSet& simpleLocals, GrowingTypeArray& localTypes)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].CollectSimpleLocals(complexLocals, simpleLocals, localTypes);
		}

		if (trueJump) trueJump->CollectSimpleLocals(complexLocals, simpleLocals, localTypes);
		if (falseJump) falseJump->CollectSimpleLocals(complexLocals, simpleLocals, localTypes);
	}
}

void InterCodeBasicBlock::SimpleLocalToTemp(int vindex, int temp)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].SimpleLocalToTemp(vindex, temp);
		}

		if (trueJump) trueJump->SimpleLocalToTemp(vindex, temp);
		if (falseJump) falseJump->SimpleLocalToTemp(vindex, temp);
	}

}

void InterCodeBasicBlock::CollectActiveTemporaries(FastNumberSet& set)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].CollectActiveTemporaries(set);
		}

		if (trueJump) trueJump->CollectActiveTemporaries(set);
		if (falseJump) falseJump->CollectActiveTemporaries(set);
	}
}

void InterCodeBasicBlock::ShrinkActiveTemporaries(FastNumberSet& set, GrowingTypeArray& temporaries)
{
	int i;

	if (!visited)
	{
		visited = true;

		for (i = 0; i < code.Size(); i++)
		{
			code[i].ShrinkActiveTemporaries(set, temporaries);
		}

		if (trueJump) trueJump->ShrinkActiveTemporaries(set, temporaries);
		if (falseJump) falseJump->ShrinkActiveTemporaries(set, temporaries);
	}
}

void InterCodeBasicBlock::Disassemble(FILE* file, bool dumpSets)
{
	int i;

	if (!visited)
	{
		visited = true;

		fprintf(file, "L%d: (%d)\n", num, numEntries);

		if (dumpSets)
		{
			fprintf(file, "Entry required temps : ");
			for (i = 0; i < entryRequiredTemps.Size(); i++)
			{
				if (entryRequiredTemps[i])
					fprintf(file, "#");
				else
					fprintf(file, "-");
			}
			fprintf(file, "\n\n");
		}

		for (i = 0; i < code.Size(); i++)
		{
			if (code[i].code != IT_NONE)
			{
				fprintf(file, "%04x\t", i);
				code[i].Disassemble(file);
			}
		}

		if (trueJump) fprintf(file, "\t\t==> %d\n", trueJump->num);
		if (falseJump) fprintf(file, "\t\t==> %d\n", falseJump->num);

		if (trueJump) trueJump->Disassemble(file, dumpSets);
		if (falseJump) falseJump->Disassemble(file, dumpSets);
	}
}

InterCodeProcedure::InterCodeProcedure(InterCodeModule * mod, const Location & location, const Ident* ident)
	: temporaries(IT_NONE), blocks(nullptr), mLocation(location), mTempOffset(-1), 
	renameTable(-1), renameUnionTable(-1), globalRenameTable(-1),
	valueForwardingTable(NULL), mLocalVars(InterVariable()), mModule(mod),
	mIdent(ident)
{
	mID = mModule->mProcedures.Size();
	mModule->mProcedures.Push(this);
}

InterCodeProcedure::~InterCodeProcedure(void)
{
}

void InterCodeProcedure::ResetVisited(void)
{
	int i;

	for (i = 0; i < blocks.Size(); i++)
		blocks[i]->visited = false;
}

void InterCodeProcedure::Append(InterCodeBasicBlock* block)
{
	block->num = blocks.Size();
	blocks.Push(block);
}

int InterCodeProcedure::AddTemporary(InterType type)
{
	int	temp = temporaries.Size();
	temporaries.Push(type);
	return temp;
}

void InterCodeProcedure::DisassembleDebug(const char* name)
{
	Disassemble(name);
}

void InterCodeProcedure::BuildTraces(void)
{
	// Count number of entries
//
	ResetVisited();
	for (int i = 0; i < blocks.Size(); i++)
		blocks[i]->numEntries = 0;
	blocks[0]->CollectEntries();

	//
	// Build traces
	//
	ResetVisited();
	blocks[0]->GenerateTraces();

	ResetVisited();
	for (int i = 0; i < blocks.Size(); i++)
		blocks[i]->numEntries = 0;
	blocks[0]->CollectEntries();

	DisassembleDebug("BuildTraces");
}

void InterCodeProcedure::BuildDataFlowSets(void)
{
	int	numTemps = temporaries.Size();

	//
	//	Build set with local provided/required temporaries
	//
	ResetVisited();
	blocks[0]->BuildLocalTempSets(numTemps, numFixedTemporaries);

	//
	// Build set of globaly provided temporaries
	//
	ResetVisited();
	blocks[0]->BuildGlobalProvidedTempSet(NumberSet(numTemps));

	//
	// Build set of globaly required temporaries, might need
	// multiple iterations until it stabilizes
	//
	NumberSet	totalRequired(numTemps);

	do {
		ResetVisited();
	} while (blocks[0]->BuildGlobalRequiredTempSet(totalRequired));
}

void InterCodeProcedure::RenameTemporaries(void)
{
	int	numTemps = temporaries.Size();

	//
	// Now we can rename temporaries to remove false dependencies
	//
	renameTable.SetSize(numTemps, true);

	int		i, j, numRename;

	numRename = numFixedTemporaries;
	for (i = 0; i < numRename; i++)
		renameTable[i] = i;

	//
	// First localy rename all temporaries
	//
	ResetVisited();
	blocks[0]->LocalRenameRegister(renameTable, numRename, numFixedTemporaries);

	DisassembleDebug("local renamed temps");

	//
	// Build a union find data structure for rename merging, this
	// merges renames temporaries back, that have been renamed differently
	// on separate paths.
	//
	renameUnionTable.SetSize(numRename);
	for (i = 0; i < numRename; i++)
		renameUnionTable[i] = i;

	//
	// Build global rename table using a union/find algorithm
	//
	renameTable.SetSize(numTemps, true);

	ResetVisited();
	blocks[0]->BuildGlobalRenameRegisterTable(renameTable, renameUnionTable);

	//
	// Now calculate the global temporary IDs for all local ids
	//
	int		numRenamedTemps;

	globalRenameTable.SetSize(numRename, true);

	for (i = 0; i < numFixedTemporaries; i++)
		globalRenameTable[i] = i;

	numRenamedTemps = numFixedTemporaries;

	for (i = numFixedTemporaries; i < numRename; i++)
	{
		j = Find(renameUnionTable, i);

		if (globalRenameTable[j] < 0)
			globalRenameTable[j] = numRenamedTemps++;

		globalRenameTable[i] = globalRenameTable[j];
	}

	temporaries.SetSize(numRenamedTemps);

	//
	// Set global temporary IDs
	//
	ResetVisited();
	blocks[0]->GlobalRenameRegister(globalRenameTable, temporaries);

	numTemps = numRenamedTemps;

	DisassembleDebug("global renamed temps");
}

void InterCodeProcedure::TempForwarding(void)
{
	int	numTemps = temporaries.Size();

	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps);

	//
	// Now remove needless temporary moves, that apear due to
	// stack evaluation
	//
	tempForwardingTable.SetSize(numTemps);

	tempForwardingTable.Reset();
	ResetVisited();
	blocks[0]->PerformTempForwarding(tempForwardingTable);

	DisassembleDebug("temp forwarding");
}

void InterCodeProcedure::Close(void)
{
	int				i, j, k, start;
	GrowingTypeArray	tstack(IT_NONE);

	_CrtCheckMemory();

	numFixedTemporaries = 0;

	DisassembleDebug("start");

	BuildTraces();

	ResetVisited();
	mLeafProcedure = blocks[0]->IsLeafProcedure();

	if (!mLeafProcedure)
	{
		int		size = 0;

		ResetVisited();
		blocks[0]->CollectOuterFrame(0, size);
		mCommonFrameSize = size;
	}
	else
		mCommonFrameSize = 0;

	BuildDataFlowSets();

	RenameTemporaries();

	TempForwarding();

	int	numTemps = temporaries.Size();

	//
	// Find all local variables that are never aliased
	//
	GrowingIntArray		localTable(-1);
	ResetVisited();
	blocks[0]->CollectLocalAddressTemps(localTable);

	int			nlocals = 0;
	for (int i = 0; i < localTable.Size(); i++)
		if (localTable[i] + 1 > nlocals)
			nlocals = localTable[i] + 1;

	localAliasedSet.Reset(nlocals);
	ResetVisited();
	blocks[0]->MarkAliasedLocalTemps(localTable, localAliasedSet);

	//
	//	Now forward constant values
	//
	ValueSet		valueSet;
	FastNumberSet	tvalidSet(numTemps);

	valueForwardingTable.SetSize(numTemps, true);

	ResetVisited();
	blocks[0]->PerformValueForwarding(valueForwardingTable, valueSet, tvalidSet, localAliasedSet);

	DisassembleDebug("value forwarding");

	valueForwardingTable.Clear();

	ResetVisited();
	blocks[0]->PerformMachineSpecificValueUsageCheck(valueForwardingTable, tvalidSet);

	DisassembleDebug("machine value forwarding");

	//
	// Now remove needless temporary moves, that apear due to
	// stack evaluation
	//
	tempForwardingTable.Reset();
	tempForwardingTable.SetSize(numTemps);

	ResetVisited();
	blocks[0]->PerformTempForwarding(tempForwardingTable);

	DisassembleDebug("temp forwarding 2");


	//
	// Now remove unused instructions
	//

	do {
		ResetVisited();
		blocks[0]->BuildLocalTempSets(numTemps, numFixedTemporaries);

		ResetVisited();
		blocks[0]->BuildGlobalProvidedTempSet(NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (blocks[0]->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (blocks[0]->RemoveUnusedResultInstructions(numFixedTemporaries));

	DisassembleDebug("removed unused instructions");

	ResetVisited();
	blocks[0]->CollectVariables(mModule->mGlobalVars, mLocalVars);


	if (mLocalVars.Size() > 0)
	{
		for (int i = 0; i < mLocalVars.Size(); i++)
		{
			if (localAliasedSet[i])
				mLocalVars[i].mAliased = true;
		}

		//
		// Now remove unused stores
		//

		do {
			ResetVisited();
			blocks[0]->BuildLocalVariableSets(mLocalVars);

			ResetVisited();
			blocks[0]->BuildGlobalProvidedVariableSet(mLocalVars, NumberSet(mLocalVars.Size()));

			NumberSet	totalRequired2(mLocalVars.Size());

			do {
				ResetVisited();
			} while (blocks[0]->BuildGlobalRequiredVariableSet(mLocalVars, totalRequired2));

			ResetVisited();
		} while (blocks[0]->RemoveUnusedStoreInstructions(mLocalVars));

		DisassembleDebug("removed unused local stores");
	}

	//
	// Promote local variables to temporaries
	//

	FastNumberSet	simpleLocals(nlocals), complexLocals(nlocals);	
	GrowingTypeArray	localTypes(IT_NONE);

	ResetVisited();
	blocks[0]->CollectSimpleLocals(complexLocals, simpleLocals, localTypes);

	for (int i = 0; i < simpleLocals.Num(); i++)
	{
		int vi = simpleLocals.Element(i);
		if (!complexLocals[vi])
		{
			ResetVisited();
			blocks[0]->SimpleLocalToTemp(vi, AddTemporary(localTypes[vi]));
		}
	}

	DisassembleDebug("local variables to temps");

	BuildTraces();

	BuildDataFlowSets();

	RenameTemporaries();

	TempForwarding();

	//
	// Now remove unused instructions
	//

	do {
		ResetVisited();
		blocks[0]->BuildLocalTempSets(numTemps, numFixedTemporaries);

		ResetVisited();
		blocks[0]->BuildGlobalProvidedTempSet(NumberSet(numTemps));

		NumberSet	totalRequired2(numTemps);

		do {
			ResetVisited();
		} while (blocks[0]->BuildGlobalRequiredTempSet(totalRequired2));

		ResetVisited();
	} while (blocks[0]->RemoveUnusedResultInstructions(numFixedTemporaries));

	DisassembleDebug("removed unused instructions 2");



	FastNumberSet	activeSet(numTemps);

	//
	// And remove unused temporaries
	//
	for (i = 0; i < numFixedTemporaries; i++)
		activeSet += i;

	ResetVisited();
	blocks[0]->CollectActiveTemporaries(activeSet);


	temporaries.SetSize(activeSet.Num());

	ResetVisited();
	blocks[0]->ShrinkActiveTemporaries(activeSet, temporaries);

	MapVariables();

	DisassembleDebug("mapped variabled");

	_CrtCheckMemory();
}

void InterCodeProcedure::MapVariables(void)
{
	ResetVisited();
	blocks[0]->MapVariables(mModule->mGlobalVars, mLocalVars);
	mLocalSize = 0;
	for (int i = 0; i < mLocalVars.Size(); i++)
	{
		if (mLocalVars[i].mUsed)
		{
			mLocalVars[i].mOffset = mLocalSize;
			mLocalSize += mLocalVars[i].mSize;
		}
	}
}

void InterCodeProcedure::ReduceTemporaries(void)
{
	NumberSet* collisionSet;
	int i, j, numRenamedTemps;
	int numTemps = temporaries.Size();

	ResetVisited();
	blocks[0]->BuildLocalTempSets(numTemps, numFixedTemporaries);

	ResetVisited();
	blocks[0]->BuildGlobalProvidedTempSet(NumberSet(numTemps));

	NumberSet	totalRequired2(numTemps);

	do {
		ResetVisited();
	} while (blocks[0]->BuildGlobalRequiredTempSet(totalRequired2));

	collisionSet = new NumberSet[numTemps];

	for (i = 0; i < numTemps; i++)
		collisionSet[i].Reset(numTemps);

	ResetVisited();
	blocks[0]->BuildCollisionTable(collisionSet);

	renameTable.SetSize(numTemps, true);

	for (i = 0; i < numFixedTemporaries; i++)
		renameTable[i] = i;

	numRenamedTemps = numFixedTemporaries;

	NumberSet	usedTemps(numTemps);

	for (i = numFixedTemporaries; i < numTemps; i++)
	{
		usedTemps.Clear();

		for (j = numFixedTemporaries; j < numTemps; j++)
		{
			if (renameTable[j] >= 0 && (collisionSet[i][j] || !TypeCompatible(temporaries[j], temporaries[i])))
			{
				usedTemps += renameTable[j];
			}
		}

		j = numFixedTemporaries;
		while (usedTemps[j])
			j++;

		renameTable[i] = j;
		if (j >= numRenamedTemps) numRenamedTemps = j + 1;
	}

	temporaries.SetSize(numRenamedTemps);

	ResetVisited();
	blocks[0]->GlobalRenameRegister(renameTable, temporaries);

	delete[] collisionSet;

	ResetVisited();
	blocks[0]->BuildLocalTempSets(numRenamedTemps, numFixedTemporaries);

	ResetVisited();
	blocks[0]->BuildGlobalProvidedTempSet(NumberSet(numRenamedTemps));

	NumberSet	totalRequired3(numRenamedTemps);

	do {
		ResetVisited();
	} while (blocks[0]->BuildGlobalRequiredTempSet(totalRequired3));

	mTempOffset.SetSize(0);
	int	offset = 0;
	if (!mLeafProcedure)
		offset += 16;

	for (int i = 0; i < temporaries.Size(); i++)
	{
		mTempOffset.Push(offset);
		switch (temporaries[i])
		{
		case IT_FLOAT:
			offset += 4;
			break;
		default:
			offset += 2;
			break;
		}
	}
	mTempSize = offset;
}

void InterCodeProcedure::Disassemble(const char* name, bool dumpSets)
{
	FILE* file;
	static bool	initial = true;

	if (!initial)
	{
		fopen_s(&file, "r:\\cldiss.txt", "a");
	}
	else
	{
		fopen_s(&file, "r:\\cldiss.txt", "w");
		initial = false;
	}

	if (file)
	{
		fprintf(file, "--------------------------------------------------------------------\n");
		fprintf(file, "%s : %s:%d\n", name, mLocation.mFileName, mLocation.mLine);

		ResetVisited();
		blocks[0]->Disassemble(file, dumpSets);

		fclose(file);
	}
}

InterCodeModule::InterCodeModule(void)
	: mGlobalVars(InterVariable()), mProcedures(nullptr)
{
}

InterCodeModule::~InterCodeModule(void)
{

}

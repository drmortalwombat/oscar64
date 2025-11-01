#include "GlobalOptimizer.h"
#include "Constexpr.h"


#define DUMP_OPTS	0


static const uint64	OPTF_ANALYZED = (1ULL << 0);
static const uint64	OPTF_ANALYZING = (1ULL << 1);
static const uint64	OPTF_RECURSIVE = (1ULL << 2);
static const uint64	OPTF_FUNC_VARIABLE = (1ULL << 3);
static const uint64	OPTF_CALLED = (1ULL << 4);
static const uint64	OPTF_CALLING = (1ULL << 5);
static const uint64 OPTF_VAR_MODIFIED = (1ULL << 6);
static const uint64 OPTF_VAR_ADDRESS = (1ULL << 7);
static const uint64 OPTF_VAR_USED = (1ULL << 8);
static const uint64 OPTF_VAR_CONST = (1ULL << 9);
static const uint64 OPTF_VAR_NOCONST = (1ULL << 10);
static const uint64 OPTF_SINGLE_RETURN = (1ULL << 11);
static const uint64 OPTF_MULTI_RETURN = (1ULL << 12);
static const uint64 OPTF_VAR_NO_FORWARD = (1UL << 13);
static const uint64 OPTF_SINGLE_CALL = (1ULL << 14);
static const uint64 OPTF_MULTI_CALL = (1ULL << 15);
static const uint64 OPTF_CONST_FUNCTION = (1ULL << 16);
static const uint64 OPTF_VAR_RANGE_LIMITED = (1ULL << 17);
static const uint64 OPTF_VAR_COPY = (1ULL << 18);
static const uint64 OPTF_VAR_INDEXED = (1ULL << 19);

GlobalOptimizer::GlobalOptimizer(Errors* errors, Linker* linker)
	: mErrors(errors), mLinker(linker)
{

}

GlobalOptimizer::~GlobalOptimizer(void)
{

}

void GlobalOptimizer::Reset(void)
{
	for (int i = 0; i < mFunctions.Size(); i++)
	{
		Declaration* func = mFunctions[i];
		Declaration* ftype = func->mBase;
		func->mOptFlags = 0;
		ftype->mOptFlags = 0;
		func->mReturn = nullptr;

		Declaration* pdec = ftype->mParams;
		while (pdec)
		{
			pdec->mOptFlags = 0;
			pdec->mForwardParam = nullptr;
			pdec = pdec->mNext;
		}
	}

	for (int i = 0; i < mGlobalVariables.Size(); i++)
	{
		mGlobalVariables[i]->mOptFlags = 0;
	}

	mFunctions.SetSize(0);
	mGlobalVariables.SetSize(0);
	mCalledFunctions.SetSize(0);
	mCallingFunctions.SetSize(0);
}

void GlobalOptimizer::PropagateParamCommas(Expression*& fexp, Expression*& exp)
{
	PropagateCommas(exp);
	if (exp->mType == EX_COMMA)
	{
		Expression* cexp = exp;
		exp = cexp->mRight;
		cexp->mRight = fexp;
		fexp = cexp;
		fexp->mDecType = cexp->mRight->mDecType;
	}
}

void GlobalOptimizer::PropagateCommas(Expression*& exp)
{
	if (exp->mType == EX_PREFIX && exp->mLeft->mType == EX_COMMA)
	{
		Expression* cexp = exp->mLeft;
		exp->mLeft = cexp->mRight;
		cexp->mDecType = exp->mDecType;
		cexp->mRight = exp->ConstantFold(mErrors, nullptr);
		exp = cexp;
	}
	else if (exp->mType == EX_CALL)
	{
		Expression* pexp = exp;
		Expression* rexp = pexp->mRight;
		if (rexp)
		{
			while (rexp && rexp->mType == EX_LIST)
			{
				PropagateParamCommas(exp, rexp->mLeft);
				if (rexp->mLeft->	mType == EX_COMMA)
				pexp = rexp;
				rexp = rexp->mRight;
			}
			if (rexp)
				PropagateParamCommas(exp, pexp->mRight);
		}
	}
	else
	{
		if (exp->mLeft)
			PropagateCommas(exp->mLeft);
		if (exp->mRight)
			PropagateCommas(exp->mRight);
	}

}

bool GlobalOptimizer::CheckSplitAggregates(Declaration* func, Expression* exp)
{
	bool	changed = false;

	if (exp->mType == EX_VARIABLE)
	{
#if 1
		Declaration* dvar = exp->mDecValue;
		if (dvar->mType == DT_VARIABLE && dvar->mIdent && !(dvar->mFlags & (DTF_GLOBAL | DTF_STATIC)))
		{
			if (exp->mDecType->mType == DT_TYPE_ARRAY && exp->mDecType->mSize <= 16)
			{
				if (!(dvar->mOptFlags & OPTF_VAR_ADDRESS) && !(dvar->mOptFlags & OPTF_VAR_COPY) && (dvar->mOptFlags & OPTF_VAR_USED))
				{
//					printf("Split %s\n", exp->mDecValue->mIdent->mString);
					ExpandingArray<Declaration*> evars;
					int num = exp->mDecType->mSize / exp->mDecType->mBase->mSize;
					for (int i = 0; i < num; i++)
					{
						Declaration* dec = new Declaration(dvar->mLocation, DT_VARIABLE);
						char str[8];
						sprintf_s(str, "[%d]", i);
						dec->mIdent = dvar->mIdent->Mangle(str);
						dec->mBase = exp->mDecType->mBase;
						dec->mSize = dec->mSize;
						dec->mVarIndex = func->mNumVars++;
						evars.Push(dec);
					}

					SplitLocalAggregate(func->mValue, dvar, evars);
					changed = true;
				}
			}
			else if (exp->mDecType->mType == DT_TYPE_STRUCT && exp->mDecType->mSize <= 16)
			{
				if (!(dvar->mOptFlags & OPTF_VAR_ADDRESS) && !(dvar->mOptFlags & OPTF_VAR_COPY) && (dvar->mOptFlags & OPTF_VAR_USED))
				{
					Declaration* edec = exp->mDecType->mParams;
					while (edec && !edec->mBits)
						edec = edec->mNext;
						if (!edec)
						{
//							printf("Split %s in %s\n", exp->mDecValue->mIdent->mString, func->mQualIdent->mString);
							ExpandingArray<Declaration*> evars;
							evars.SetSize(exp->mDecType->mSize);
							Declaration* edec = exp->mDecType->mParams;
							while (edec)
							{
								Declaration* dec = new Declaration(dvar->mLocation, DT_VARIABLE);
								char str[240];
								sprintf_s(str, ".%s", edec->mIdent->mString);
								dec->mIdent = dvar->mIdent->Mangle(str);
								dec->mBase = edec->mBase;
								dec->mSize = dec->mSize;
								dec->mVarIndex = func->mNumVars++;
								evars[edec->mOffset] = dec;
								edec = edec->mNext;
							}

							SplitLocalAggregate(func->mValue, dvar, evars);
							changed = true;
						}
				}
			}
		}
#endif
	}
	else
	{
		if (exp->mLeft && CheckSplitAggregates(func, exp->mLeft)) changed = true;
		if (exp->mRight && CheckSplitAggregates(func, exp->mRight)) changed = true;
	}

	return changed;
}

void GlobalOptimizer::SplitLocalAggregate(Expression* exp, Declaration* var, ExpandingArray<Declaration*>& evars)
{
	if (exp->mType == EX_INDEX)
	{
		if (exp->mLeft->mType == EX_VARIABLE && exp->mLeft->mDecValue == var)
		{
			exp->mType = EX_VARIABLE;
			exp->mDecValue = evars[int(exp->mRight->mDecValue->mInteger)];
			exp->mLeft = nullptr;
			exp->mRight = nullptr;
		}
	}
	else if (exp->mType == EX_QUALIFY)
	{
		if (exp->mLeft->mType == EX_VARIABLE && exp->mLeft->mDecValue == var)
		{
			exp->mType = EX_VARIABLE;
			exp->mDecValue = evars[int(exp->mDecValue->mOffset)];
			exp->mLeft = nullptr;
			exp->mRight = nullptr;
		}
	}
	else if (exp->mType == EX_VARIABLE && exp->mDecValue == var)
	{
		exp->mType = EX_VOID;
	}

	if (exp->mLeft)
		SplitLocalAggregate(exp->mLeft, var, evars);
	if (exp->mRight)
		SplitLocalAggregate(exp->mRight, var, evars);
}

bool GlobalOptimizer::CheckConstFunction(Expression* exp)
{
	if (exp->mType == EX_CALL || exp->mType == EX_INLINE)
	{
		if (exp->mLeft->mType == EX_CONSTANT)
		{
			Declaration* pcall = exp->mLeft->mDecValue;
			if (!(pcall->mOptFlags & OPTF_CONST_FUNCTION))
				return false;
		}
		else
			return false;
	}
	else if (exp->mType == EX_DISPATCH)
		return false;
	else if (exp->mType == EX_RETURN && !exp->mLeft)
		return false;

	if (exp->mLeft && !CheckConstFunction(exp->mLeft))
		return false;
	if (exp->mRight && !CheckConstFunction(exp->mRight))
		return false;

	return true;

}

bool GlobalOptimizer::CheckUnusedReturns(Expression*& exp)
{
	bool	changed = false;

	if (exp->mType == EX_CALL && exp->mDecType && exp->mDecType->mType != DT_TYPE_VOID)
	{
		if (exp->mLeft->mType == EX_CONSTANT)
		{
			Declaration* pcall = exp->mLeft->mDecValue;
			if (pcall->mBase->mBase->mType == DT_TYPE_VOID)
			{
				Expression* lexp = new Expression(exp->mLocation, EX_COMMA);
				lexp->mLeft = exp;
				lexp->mRight = new Expression(exp->mLocation, EX_CONSTANT);
				lexp->mRight->mDecValue = TheZeroIntegerConstDeclaration->ConstCast(exp->mDecType);
				lexp->mRight->mDecType = exp->mDecType;
				lexp->mDecType = exp->mDecType;
				exp->mDecType = TheVoidTypeDeclaration;
				exp = lexp;
				return true;
			}
		}
	}

	if (exp->mLeft && CheckUnusedReturns(exp->mLeft))
		changed = true;
	if (exp->mRight && CheckUnusedReturns(exp->mRight))
		changed = true;

	return changed;
}

bool GlobalOptimizer::ReplaceConstCalls(Expression*& exp) 
{
	bool	changed = false;

	if (exp->mType == EX_CALL && exp->mDecType && exp->mDecType->mType != DT_TYPE_VOID)
	{
		if (exp->mLeft->mType == EX_CONSTANT)
		{
			Declaration* pcall = exp->mLeft->mDecValue;
			if (pcall->mOptFlags & OPTF_CONST_FUNCTION)
			{
				Expression* args = exp->mRight;
				bool	constargs = true;
				while (args && constargs)
				{
					if (args->mType == EX_LIST)
					{
						if (args->mLeft->mType != EX_CONSTANT)
							constargs = false;
						else
							args = args->mRight;
					}
					else if (args->mType != EX_CONSTANT)
						constargs = false;
					else
						args = nullptr;

				}

				if (constargs)
				{
					ConstexprInterpreter	cinter(exp->mLocation, mErrors, pcall->mSection);
					Expression * nexp = cinter.EvalCall(exp);

					if (nexp && nexp != exp)
					{
						exp = nexp;
						changed = true;
					}
				}
			}
		}
	}

	if (exp->mLeft && ReplaceConstCalls(exp->mLeft))
		changed = true;
	if (exp->mRight && ReplaceConstCalls(exp->mRight))
		changed = true;

	return changed;
}


bool GlobalOptimizer::EstimateCost(Expression* exp, Declaration* vindex, int64& cycles, int64& bytes, bool &cconst)
{
	cycles = 0;
	bytes = 0;
	cconst = false;

	if (exp)
	{
		int64	lcycles, lbytes, rcycles, rbytes;
		bool	lconst, rconst;

		switch (exp->mType)
		{
		case EX_CONSTANT:
			cconst = true;
			return true;

		case EX_VARIABLE:
			if (exp->mDecValue != vindex)
			{
				if (exp->mDecValue->mSize > 1 || (exp->mDecValue->mFlags & (DTF_GLOBAL | DTF_STATIC)))
				{
					cycles = 2;
					bytes = 4;
				}
				else if (!vindex)
				{
					cycles = 1;
					bytes = 1;
				}
			}
			else
				cconst = true;
			return true;

		case EX_ASSIGNMENT:
		case EX_INITIALIZATION:
			if (exp->mLeft->mType == EX_VARIABLE && exp->mLeft->mDecValue == vindex)
				return false;

			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst) && EstimateCost(exp->mRight, vindex, rcycles, rbytes, rconst))
			{
				cycles = lcycles + rcycles + exp->mDecType->mSize * (exp->mToken == TK_ASSIGN ? 1 : 3);
				bytes = lbytes + rbytes + exp->mDecType->mSize * (exp->mToken == TK_ASSIGN ? 2 : 5);
				cconst = rconst;
				return true;
			}
			break;
		case EX_BINARY:
		case EX_RELATIONAL:
			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst) && EstimateCost(exp->mRight, vindex, rcycles, rbytes, rconst))
			{
				if (!lconst || !rconst)
				{
					if (exp->mType == EX_RELATIONAL &&
						(exp->mLeft->mType == EX_CONSTANT && exp->mLeft->mDecType->mType == DT_TYPE_INTEGER && exp->mLeft->mDecType->mSize <= 2 && exp->mLeft->mDecValue->mInteger == 0 ||
							exp->mRight->mType == EX_CONSTANT && exp->mRight->mDecType->mType == DT_TYPE_INTEGER && exp->mRight->mDecType->mSize <= 2 && exp->mRight->mDecValue->mInteger == 0))
					{
						bytes += lbytes + rbytes;
						cycles += lcycles + rcycles;
					}
					else
					{
						bytes = lbytes + rbytes + 2 * exp->mDecType->mSize;
						cycles = lcycles + rcycles;
						if (exp->mToken == TK_MUL && lcycles > 0 && rcycles > 0)
							cycles += exp->mDecType->mSize * exp->mDecType->mSize * 25;
						else if (exp->mToken == TK_DIV || exp->mToken == TK_MOD)
							cycles += exp->mDecType->mSize * exp->mDecType->mSize * 40;
						else
							cycles += exp->mDecType->mSize;
					}
				}
				else
					cconst = true;

				return true;
			}

			break;
		case EX_POSTINCDEC:
		case EX_PREINCDEC:
			if (exp->mLeft->mType == EX_VARIABLE && exp->mLeft->mDecValue == vindex)
				return false;

			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst))
			{
				bytes = lbytes + 2 * exp->mDecType->mSize;
				cycles = lcycles + exp->mDecType->mSize;
				cconst = lconst;

				return true;
			}
			break;

		case EX_PREFIX:
			if (exp->mToken == TK_ADD || exp->mToken == TK_SUB)
			{
				if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst))
				{
					if (!lconst)
					{
						bytes = lbytes + exp->mDecType->mSize;
						cycles = lcycles + exp->mDecType->mSize;
					}
					else
						cconst = true;

					return true;
				}
			}
			else if (exp->mToken == TK_BINARY_AND || exp->mToken == TK_MUL)
			{
				if (exp->mLeft->mType == EX_VARIABLE && exp->mLeft->mDecValue == vindex)
					return false;

				return EstimateCost(exp->mLeft, vindex, cycles, bytes, cconst);
			}
			break;

		case EX_QUALIFY:
			return EstimateCost(exp->mLeft, vindex, cycles, bytes, cconst);

		case EX_INDEX:
			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst) && EstimateCost(exp->mRight, vindex, rcycles, rbytes, rconst))
			{
				cycles = lcycles + rcycles;
				bytes = lbytes + rbytes;
				if (!rconst)
				{
					bytes += 5;
					cycles += exp->mDecType->mSize == 1 ? 4 : 8;
				}
				return true;
			}
			break;

		case EX_SEQUENCE:
		case EX_COMMA:
		case EX_SCOPE:
			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst) && EstimateCost(exp->mRight, vindex, rcycles, rbytes, rconst))
			{
				cycles = lcycles + rcycles;
				bytes = lbytes + rbytes;
				cconst = lconst && rconst;
				return true;
			}
			break;
		case EX_IF:
			if (EstimateCost(exp->mLeft, vindex, lcycles, lbytes, lconst))
			{
				int64	tcycles, tbytes, fcycles, fbytes;
				bool	tconst, fconst;
				if (EstimateCost(exp->mRight->mLeft, vindex, tcycles, tbytes, tconst) && EstimateCost(exp->mRight->mRight, vindex, fcycles, fbytes, fconst))
				{
					cycles = lcycles + (tcycles + fcycles + 1) / 2;
					bytes = lbytes + tbytes + fbytes;
					if (!lconst)
					{
						cycles += 2;
						bytes += 2;
					}
				}
				return true;
			}
			break;

		case EX_CALL:
			if (exp->mDecType && exp->mDecType->mType != DT_TYPE_VOID && exp->mLeft->mType == EX_CONSTANT)
			{
				Declaration* pcall = exp->mLeft->mDecValue;
				if (pcall->mOptFlags & OPTF_CONST_FUNCTION)
				{
					if (exp->mRight && exp->mRight->mType == EX_VARIABLE && exp->mRight->mDecValue == vindex)
					{
						cycles = 0;
						bytes = 0;
						cconst = true;

						return true;
					}

					if (pcall->mFlags & OPTF_RECURSIVE)
						cycles = 500;
					else
						cycles = 100;
					bytes = 3;

					Expression* lexp = exp->mRight;
					while (lexp)
					{
						Expression* pexp = lexp;
						if (lexp->mType == EX_LIST)
						{
							pexp = lexp->mLeft;
							lexp = lexp->mRight;
						}
						else
							lexp = nullptr;

						if (!EstimateCost(lexp, vindex, lcycles, lbytes, lconst))
							return false;

						if (!lconst)
						{
							bytes += lbytes + 2 * exp->mDecType->mSize;
							cycles += lcycles + exp->mDecType->mSize;
						}
					}

					return true;
				}
			}

			return false;
		}

		return false;
	}

	return true;

}


bool GlobalOptimizer::UnrollLoops(Expression*& exp)
{
	bool	changed = false;

	if (exp->mType == EX_FOR)
	{
		Expression* assignment = exp->mLeft->mRight;
		Expression* condition = exp->mLeft->mLeft->mLeft;
		Expression* body = exp->mRight;
		Expression* increment = exp->mLeft->mLeft->mRight;

		if (assignment && condition && body && increment)
		{
			if ((assignment->mType == EX_INITIALIZATION || assignment->mType == EX_ASSIGNMENT) &&
				assignment->mRight->mType == EX_CONSTANT &&
				assignment->mLeft->mType == EX_VARIABLE &&
				assignment->mLeft->mDecType->IsIntegerType() &&
				!(assignment->mLeft->mDecValue->mFlags & (DTF_GLOBAL | DTF_STATIC)))
			{
				Declaration* ivdec = assignment->mLeft->mDecValue;

				if (condition->mType == EX_VARIABLE && condition->mDecValue == ivdec ||

					condition->mType == EX_RELATIONAL &&
					condition->mLeft->mType == EX_VARIABLE &&
					condition->mLeft->mDecValue == ivdec &&
					condition->mRight->mType == EX_CONSTANT)
				{
					if ((increment->mType == EX_POSTINCDEC || increment->mType == EX_PREINCDEC) &&
						increment->mLeft->mType == EX_VARIABLE &&
						increment->mLeft->mDecValue == ivdec)
					{
						int64	start = assignment->mRight->mDecValue->mInteger;
						int64	end;
						Token	cmp;

						if (condition->mType == EX_RELATIONAL)
						{
							end = condition->mRight->mDecValue->mInteger;
							cmp = condition->mToken;
						}
						else
						{
							end = 0;
							cmp = TK_NOT_EQUAL;
						}

						int64	step = 1;
						int64	count = 0;

						if (increment->mType == EX_POSTINCDEC || increment->mType == EX_PREINCDEC)
						{
							if (increment->mToken == TK_INC)
								step = 1;
							else
								step = -1;
						}

						if (step > 0)
						{
							if (end > start)
							{
								if (cmp == TK_LESS_THAN || cmp == TK_NOT_EQUAL)
									count = (end + step - 1 - start) / step;
								else if (cmp == TK_LESS_EQUAL)
									count = (end + step - start) / step;
							}
						}
						else
						{
							if (end < start)
							{
								if (cmp == TK_GREATER_THAN || cmp == TK_NOT_EQUAL)
									count = (-end - step - 1 + start) / - step;
								else if (cmp == TK_GREATER_EQUAL)
									count = (-end - step + start) / - step;
							}
						}

						if (count > 0 && count < 20)
						{
							int64	pcycles, pbytes, ucycles, ubytes;
							bool	pconst, uconst;

							if (EstimateCost(body, nullptr, pcycles, pbytes, pconst) && EstimateCost(body, ivdec, ucycles, ubytes, uconst))
							{
								int64	byteLimit = 20;

								int64 gcycles = (pcycles - ucycles) * count;

								if (mCompilerOptions & COPT_OPTIMIZE_CODE_SIZE)
									byteLimit = 0;
								else if (mCompilerOptions & COPT_OPTIMIZE_AUTO_INLINE_ALL)
								{
									byteLimit += 80;
									byteLimit += gcycles / 20;
									if (byteLimit > 400)
										byteLimit = 400;
								}
								else
								{
									byteLimit += gcycles / 100;
									if (byteLimit > 100)
										byteLimit = 100;
								}

								if (ubytes * count < pbytes + 4 + 4 * ivdec->mSize + byteLimit)
								{
									Expression* seq = nullptr;
									for (int64 i = 0; i<count; i++)
									{
										Declaration* icdec = assignment->mRight->mDecValue->Clone();
										icdec->mBase = ivdec->mBase;
										icdec->mInteger = ivdec->mBase->CastInteger(start + i * step);
										Expression* bexp = body->ToVarConst(ivdec, icdec);
										if (seq)
										{
											Expression* nseq = new Expression(exp->mLocation, EX_SEQUENCE);
											nseq->mLeft = seq;
											nseq->mRight = bexp;
											seq = nseq;
										}
										else
											seq = bexp;
									}
									Declaration* ficdec = assignment->mRight->mDecValue->Clone();
									ficdec->mInteger = start + count * step;

									exp = new Expression(exp->mLocation, EX_SEQUENCE);
									exp->mLeft = seq;
									exp->mRight = assignment;
									assignment->mRight->mDecValue = ficdec;

									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	if (exp->mLeft && UnrollLoops(exp->mLeft))
		changed = true;
	if (exp->mRight && UnrollLoops(exp->mRight))
		changed = true;

	return changed;
}


bool GlobalOptimizer::CheckConstReturns(Expression*& exp)
{
	bool	changed = false;

	if (exp->mType == EX_CALL && exp->mDecType && exp->mDecType->mType != DT_TYPE_VOID)
	{
		if (exp->mLeft->mType == EX_CONSTANT)
		{
			Declaration* pcall = exp->mLeft->mDecValue;
			if ((pcall->mOptFlags & OPTF_SINGLE_RETURN) && pcall->mReturn->mLeft)
			{
				Expression	* rexp = pcall->mReturn->mLeft;
				if (rexp->mType == EX_CONSTANT)
				{
#if DUMP_OPTS
					printf("Extract const return\n");
#endif

					Expression* lexp = new Expression(exp->mLocation, EX_COMMA);
					lexp->mLeft = exp;
					lexp->mRight = new Expression(pcall->mReturn->mLocation, EX_CONSTANT);
					lexp->mRight->mDecValue = pcall->mReturn->mLeft->mDecValue->ConstCast(exp->mDecType);
					lexp->mRight->mDecType = exp->mDecType;
					lexp->mDecType = exp->mDecType;
					exp->mDecType = TheVoidTypeDeclaration;
					exp = lexp;
					return true;
				}
				else if (rexp->mType == EX_PREFIX && rexp->mToken == TK_MUL && rexp->mLeft->mType == EX_VARIABLE && rexp->mLeft->mDecValue->mType == DT_ARGUMENT)
				{
					Declaration* pdec = rexp->mLeft->mDecValue;
					if (pdec->mVarIndex == 0 && !(pdec->mOptFlags & OPTF_VAR_ADDRESS))
					{
						Expression* pex = exp->mRight;
						if (pex->mType == EX_LIST)
							pex = pex->mLeft;
						if (pex->mType == EX_CONSTANT)
						{
#if DUMP_OPTS
							printf("Forward this pointer\n");
#endif
							Expression* lexp = new Expression(exp->mLocation, EX_COMMA);
							lexp->mLeft = exp;
							lexp->mRight = new Expression(pcall->mReturn->mLocation, EX_PREFIX);
							lexp->mRight->mToken = TK_MUL;
							lexp->mRight->mLeft = pex;
							lexp->mRight->mDecType = pcall->mBase->mBase;
							lexp->mDecType = lexp->mRight->mDecType;
							exp->mDecType = TheVoidTypeDeclaration;
							exp = lexp;
							return true;
						}
					}
				}
			}
		}
	}

	if (exp->mLeft && CheckConstReturns(exp->mLeft))
		changed = true;
	if (exp->mRight && CheckConstReturns(exp->mRight))
		changed = true;

	return changed;
}

bool GlobalOptimizer::CheckUnusedLocals(Expression*& exp)
{
	bool	changed = false;

	if (exp->mType == EX_INITIALIZATION)
	{
		Expression* vexp = exp->mLeft;
		if (vexp->mType == EX_VARIABLE)
		{
			Declaration* vdec = vexp->mDecValue;
			if (vdec->mType == DT_VARIABLE && !(vdec->mFlags & (DTF_GLOBAL | DTF_STATIC)) && !(vdec->mOptFlags & OPTF_VAR_USED) && !exp->mRight->IsVolatile())
			{
				exp = exp->mRight;
				return true;
			}
		}
	}

	if (exp->mLeft && CheckUnusedLocals(exp->mLeft))
		changed = true;
	if (exp->mRight && CheckUnusedLocals(exp->mRight))
		changed = true;

	return changed;
}

void GlobalOptimizer::RemoveValueReturn(Expression* exp)
{
	if (exp->mType == EX_RETURN && exp->mLeft)
	{
		if (exp->mLeft->HasSideEffects())
		{
			exp->mType = EX_SEQUENCE;
			exp->mRight = new Expression(exp->mLocation, EX_RETURN);
		}
		else
			exp->mLeft = nullptr;
	}

	if (exp->mLeft)
		RemoveValueReturn(exp->mLeft);
	if (exp->mRight)
		RemoveValueReturn(exp->mRight);
}

void GlobalOptimizer::UndoParamReference(Expression* exp, Declaration* param)
{
	if (exp)
	{
		if (exp->mType == EX_VARIABLE && exp->mDecValue == param)
		{
			exp->mDecType = param->mBase;
		}

		UndoParamReference(exp->mLeft, param);
		UndoParamReference(exp->mRight, param);
	}
}

bool GlobalOptimizer::ReplaceParamConst(Expression* exp, Declaration* param)
{
	bool	changed = false;
	if (exp)
	{
		if (ReplaceParamConst(exp->mLeft, param))
			changed = true;
		if (ReplaceParamConst(exp->mRight, param))
			changed = true;

		if (changed)
		{
			if (exp->mLeft)
				exp->mLeft = exp->mLeft->ConstantFold(mErrors, nullptr);
			if (exp->mRight)
				exp->mRight = exp->mRight->ConstantFold(mErrors, nullptr);
		}

		if (exp->mType == EX_VARIABLE && exp->mDecValue == param)
		{
			exp->mType = EX_CONSTANT;
			exp->mDecType = param->mBase;
			exp->mDecValue = param->mValue->mDecValue->ConstCast(param->mBase);
			changed = true;
		}

	}
	return changed;
}

bool GlobalOptimizer::ReplaceGlobalConst(Expression* exp)
{
	bool	changed = false;
	if (exp)
	{
		if (exp->mType == EX_VARIABLE && (exp->mDecValue->mFlags & (DTF_GLOBAL | DTF_STATIC)) && !(exp->mDecValue->mOptFlags & (OPTF_VAR_MODIFIED | OPTF_VAR_ADDRESS)) && exp->mDecValue->mValue)
		{

			Expression* cexp = exp->mDecValue->mValue;
			if (cexp->mType == EX_CONSTANT)
			{

				if (cexp->mDecValue->mType == DT_CONST_ADDRESS || cexp->mDecValue->mType == DT_CONST_INTEGER ||
					cexp->mDecValue->mType == DT_CONST_POINTER || cexp->mDecValue->mType == DT_CONST_FLOAT)
				{
					exp->mType = EX_CONSTANT;
					exp->mDecValue = cexp->mDecValue->ConstCast(exp->mDecType);
					changed = true;
				}
				else if (exp->mDecValue->mBase->mType == DT_TYPE_STRUCT)
				{
					exp->mDecValue->mBase = exp->mDecValue->mBase->ToConstType();
				}
			}
		}

		if (ReplaceGlobalConst(exp->mLeft))
			changed = true;
		if (ReplaceGlobalConst(exp->mRight))
			changed = true;
	}
	return changed;
}

bool GlobalOptimizer::Optimize(void)
{
	bool	changed = false;

	do {
		changed = false;
		for (int i = 0; i < mFunctions.Size(); i++)
		{
			Declaration* func = mFunctions[i];
			if ((func->mOptFlags & OPTF_CONST_FUNCTION) && !(func->mValue && CheckConstFunction(func->mValue)))
			{
				func->mOptFlags &= ~OPTF_CONST_FUNCTION;
				changed = true;
			}
		}

	} while (changed);

#if DUMP_OPTS
	printf("OPT---\n");
#endif
	for (int i = 0; i < mFunctions.Size(); i++)
	{
		Declaration* func = mFunctions[i];
		Declaration* ftype = func->mBase;

#if DUMP_OPTS
		if (func->mOptFlags & OPTF_CONST_FUNCTION)
		{
			printf("%s const function\n", mFunctions[i]->mQualIdent->mString);
		}
#endif

		if (func->mValue && func->mValue->mType != EX_DISPATCH)
		{
#if DUMP_OPTS
			printf("%s %08llx\n", mFunctions[i]->mQualIdent->mString, mFunctions[i]->mOptFlags);
#endif
			if (CheckUnusedLocals(func->mValue))
				changed = true;

			if (CheckConstReturns(func->mValue))
				changed = true;

			if (CheckUnusedReturns(func->mValue))
				changed = true;

			if (ReplaceGlobalConst(func->mValue))
				changed = true;
#if 1
			if (ReplaceConstCalls(func->mValue))
				changed = true;

			if (CheckSplitAggregates(func, func->mValue))
				changed = true;

			if (mCompilerOptions & COPT_OPTIMIZE_AUTO_UNROLL)
			{
				if (UnrollLoops(func->mValue))
					changed = true;
			}
#endif
			if (func->mOptFlags & OPTF_MULTI_CALL)
			{
				Declaration* pdata = ftype->mParams;
				while (pdata)
				{
					pdata->mForwardCall = nullptr;
					pdata->mForwardParam = nullptr;
					pdata = pdata->mNext;
				}
			}

			if (!(func->mOptFlags & OPTF_FUNC_VARIABLE) && !(func->mBase->mFlags & DTF_VIRTUAL))
			{
				if (!(func->mOptFlags & (OPTF_VAR_USED | OPTF_VAR_ADDRESS)) && func->mBase->mBase && (func->mBase->mBase->IsSimpleType() || func->mBase->mBase->IsReference()))
				{
#if DUMP_OPTS
					printf("Remove return value\n");
#endif
					RemoveValueReturn(func->mValue);
					func->mBase->mBase = TheVoidTypeDeclaration;
					changed = true;
				}
				else if (!(func->mOptFlags & OPTF_VAR_ADDRESS) && func->mBase->mBase && func->mBase->mBase->IsReference() && func->mBase->mBase->mBase->IsSimpleType())
				{
#if DUMP_OPTS
					printf("Demote reference return\n");
#endif
					func->mBase->mBase = func->mBase->mBase->mBase;
					changed = true;
				}

//				if (!(ftype->mFlags & DTF_VARIADIC))
				{
					Declaration* pdec = ftype->mParams;
					int vi = 0;
					while (pdec)
					{
						if ((ftype->mFlags & DTF_VARIADIC) && !pdec->mNext)
							break;

						pdec->mVarIndex += vi;
						if (!(pdec->mOptFlags & OPTF_VAR_USED) && !(pdec->mFlags & DTF_FPARAM_UNUSED))
						{
							if (!pdec->mBase->IsReference() || !(pdec->mOptFlags & OPTF_VAR_ADDRESS))
							{
#if DUMP_OPTS
								printf("Unused parameter %s\n", pdec->mIdent ? pdec->mIdent->mString : "_");
#endif
								vi -= pdec->mSize;

								pdec->mFlags |= DTF_FPARAM_UNUSED;
								pdec->mVarIndex = func->mNumVars++;

								changed = true;
							}
						}
						else if (!(pdec->mOptFlags & OPTF_VAR_ADDRESS) && pdec->mBase->IsReference() && pdec->mBase->mBase->IsSimpleType())
						{
#if DUMP_OPTS
							printf("Reference parameter %s to value\n", pdec->mIdent ? pdec->mIdent->mString : "_");
#endif
							vi += pdec->mSize - 2;

							pdec->mBase = pdec->mBase->mBase;
							pdec->mSize = pdec->mBase->mSize;

							UndoParamReference(func->mValue, pdec);

							changed = true;
						}
						else if ((pdec->mOptFlags & OPTF_VAR_CONST) && !(pdec->mOptFlags & OPTF_VAR_ADDRESS))
						{
							if (ReplaceParamConst(func->mValue, pdec))
							{
								func->mValue = func->mValue->ConstantFold(mErrors, nullptr);
#if DUMP_OPTS
								printf("Const parameter %s\n", pdec->mIdent ? pdec->mIdent->mString : "_");
#endif
								changed = true;
							}
						}

						if (pdec->mOptFlags & OPTF_VAR_RANGE_LIMITED)
							pdec->mFlags |= DTF_FPARAM_RANGE_LIMITED;

						pdec->mOptFlags = 0;
						pdec = pdec->mNext;
					}
				}
			}

			PropagateCommas(func->mValue);
		}
		else if (func->mValue && func->mValue->mType == EX_DISPATCH)
		{
			if (!(func->mOptFlags & OPTF_FUNC_VARIABLE))
			{
				Declaration* pdec = ftype->mParams;
				while (pdec)
				{
					if ((pdec->mOptFlags & OPTF_VAR_CONST) && !(pdec->mOptFlags & OPTF_VAR_ADDRESS))
					{
						if (ReplaceParamConst(func->mValue, pdec))
						{
#if DUMP_OPTS
							printf("Const parameter %s\n", pdec->mIdent ? pdec->mIdent->mString : "_");
#endif
							changed = true;
						}
					}

					pdec->mOptFlags = 0;
					pdec = pdec->mNext;
				}
			}
		}
	}

	return changed;
}

void GlobalOptimizer::AnalyzeProcedure(Expression* exp, Declaration* procDec)
{
	if (procDec->mOptFlags & OPTF_ANALYZING)
	{
		procDec->mOptFlags |= OPTF_RECURSIVE;
	}
	else if (!(procDec->mOptFlags & OPTF_ANALYZED))
	{
		procDec->mOptFlags |= OPTF_ANALYZING | OPTF_ANALYZED | OPTF_CONST_FUNCTION;

		if (procDec->mFlags & DTF_PREVENT_INLINE)
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;

		mFunctions.Push(procDec);

		if ((procDec->mFlags & DTF_INTRINSIC) && !procDec->mValue)
			;
		else if (procDec->mFlags & DTF_DEFINED)
		{
			Analyze(exp, procDec, false);
		}
		else
			mErrors->Error(procDec->mLocation, EERR_UNDEFINED_OBJECT, "Calling undefined function", procDec->FullIdent());

		procDec->mOptFlags &= ~OPTF_ANALYZING;
	}
}

void GlobalOptimizer::AnalyzeAssembler(Expression* exp, Declaration* procDec)
{
	while (exp)
	{
		if (exp->mLeft && exp->mLeft->mDecValue)
		{
			Declaration* adec = exp->mLeft->mDecValue;
			if (adec->mType == DT_LABEL_REF)
			{

			}
			else if (adec->mType == DT_VARIABLE_REF)
			{
				if (adec->mBase->mFlags & DTF_GLOBAL)
					AnalyzeGlobalVariable(adec->mBase);
				adec->mBase->mOptFlags |= OPTF_VAR_USED | OPTF_VAR_ADDRESS;
			}
			else if (adec->mType == DT_LABEL)
			{

			}
			else if (adec->mType == DT_VARIABLE)
			{
				if (adec->mFlags & DTF_GLOBAL)
					AnalyzeGlobalVariable(adec);
				adec->mOptFlags |= OPTF_VAR_USED | OPTF_VAR_ADDRESS;
			}
			else if (adec->mType == DT_ARGUMENT)
			{
				adec->mOptFlags |= OPTF_VAR_USED;
			}
			else if (adec->mType == DT_FUNCTION_REF)
			{
				adec->mBase->mOptFlags |= OPTF_VAR_USED;
				AnalyzeProcedure(adec->mBase->mValue, adec->mBase);
				RegisterProc(adec->mBase);
			}
			else if (adec->mType == DT_CONST_FUNCTION)
			{
				adec->mOptFlags |= OPTF_VAR_USED;
				AnalyzeProcedure(adec->mValue, adec);
				RegisterCall(procDec, adec);
			}
		}

		exp = exp->mRight;
	}

}

void GlobalOptimizer::AnalyzeGlobalVariable(Declaration* dec)
{
	while (dec->mType == DT_VARIABLE_REF)
		dec = dec->mBase;

	if (!(dec->mOptFlags & OPTF_ANALYZED))
	{
		dec->mOptFlags |= OPTF_ANALYZED;

		if (dec->mValue && dec->mValue->mType == EX_CONSTRUCT)
		{
			if (dec->mValue->mLeft->mLeft->mType == EX_CALL && (dec->mValue->mLeft->mLeft->mLeft->mDecValue->mFlags & DTF_CONSTEXPR))
			{
				ConstexprInterpreter	cinter(dec->mLocation, mErrors, dec->mSection);
				dec->mValue = cinter.EvalConstructor(dec->mValue->mLeft->mLeft);
			}
		}

		mGlobalVariables.Push(dec);

		if (dec->mValue)
		{
			Analyze(dec->mValue, dec, false);
		}
	}
}

void GlobalOptimizer::AnalyzeInit(Declaration* mdec)
{
	while (mdec)
	{
		if (mdec->mValue)
			RegisterProc(Analyze(mdec->mValue, mdec, false));
		else if (mdec->mParams)
			AnalyzeInit(mdec->mParams);
		mdec = mdec->mNext;
	}
}


void GlobalOptimizer::RegisterCall(Declaration* from, Declaration* to)
{
	if (from)
	{
		if (to->mType == DT_VARIABLE || to->mType == DT_ARGUMENT)
			to = to->mBase;

		if (to->mType == DT_CONST_FUNCTION)
		{
			if (!(to->mOptFlags & OPTF_CALLED))
			{
				to->mOptFlags |= OPTF_CALLED;
				mCalledFunctions.Push(to);
			}

			if (!(from->mOptFlags & OPTF_CALLING))
			{
				from->mOptFlags |= OPTF_CALLING;
				mCallingFunctions.Push(from);
			}
		}
		else if (to->mType == DT_TYPE_FUNCTION)
		{
			if (!(from->mOptFlags & OPTF_CALLING))
			{
				from->mOptFlags |= OPTF_CALLING;
				mCallingFunctions.Push(from);
			}
		}
		else if (to->mType == DT_TYPE_POINTER && to->mBase->mType == DT_TYPE_FUNCTION)
		{
			if (!(from->mOptFlags & OPTF_CALLING))
			{
				from->mOptFlags |= OPTF_CALLING;
				mCallingFunctions.Push(from);
			}
		}
	}
}

void GlobalOptimizer::RegisterProc(Declaration* to)
{
	if (to->mType == DT_CONST_FUNCTION)
	{
		if (to->mBase->mFlags & DTF_VIRTUAL)
		{

		}
		else if (!(to->mOptFlags & OPTF_FUNC_VARIABLE))
		{
			to->mOptFlags |= OPTF_FUNC_VARIABLE;
			mVariableFunctions.Push(to);
		}
	}
}

Declaration* GlobalOptimizer::Analyze(Expression* exp, Declaration* procDec, uint32 flags)
{
	Declaration* ldec, * rdec;

	switch (exp->mType)
	{
	case EX_ERROR:
	case EX_VOID:
		break;
	case EX_GOTO:
		procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
		break;
	case EX_CONSTANT:
		if (exp->mDecValue->mType == DT_CONST_FUNCTION)
		{
			AnalyzeProcedure(exp->mDecValue->mValue, exp->mDecValue);
		}
		else if (exp->mDecValue->mType == DT_CONST_STRUCT)
		{
			AnalyzeInit(exp->mDecValue->mParams);
		}
		else if (exp->mDecValue->mType == DT_CONST_POINTER)
		{
			ldec = Analyze(exp->mDecValue->mValue, procDec, ANAFL_LHS | ANAFL_RHS);
			RegisterProc(ldec);
		}
		else if (exp->mDecValue->mType == DT_CONST_ADDRESS)
		{
		}
		else if (exp->mDecValue->mType == DT_CONST_ASSEMBLER)
		{
			AnalyzeAssembler(exp->mDecValue->mValue, procDec);
		}

		return exp->mDecValue;
	case EX_VARIABLE:
	{
		Declaration* vdec = exp->mDecValue;
		if (vdec->mType == DT_VARIABLE_REF)
			vdec = vdec->mBase;

		if ((vdec->mFlags & DTF_STATIC) || (vdec->mFlags & DTF_GLOBAL))
		{
			Declaration* type = vdec->mBase;
			while (type->mType == DT_TYPE_ARRAY)
				type = type->mBase;

			AnalyzeGlobalVariable(vdec);
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
		}
		if (flags & ANAFL_COPY)
			vdec->mOptFlags |= OPTF_VAR_COPY;
		if (flags & ANAFL_RHS)
			vdec->mOptFlags |= OPTF_VAR_USED;
		if (flags & ANAFL_LHS)
			vdec->mOptFlags |= OPTF_VAR_ADDRESS;
		if (vdec->mBase->IsReference() && (flags & ANAFL_ASSIGN))
			vdec->mOptFlags |= OPTF_VAR_USED;

		if (vdec->mType == DT_ARGUMENT && (flags & ANAFL_LHS))
		{
			vdec->mOptFlags |= OPTF_VAR_NO_FORWARD;
			vdec->mForwardParam = nullptr;
			vdec->mForwardCall = nullptr;
		}

		if (vdec->mBase->mFlags & DTF_VOLATILE)
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;

		return vdec;
	}
	case EX_INITIALIZATION:
	case EX_ASSIGNMENT:
		if (exp->mToken == TK_ASSIGN)
		{
			if (exp->mType == EX_ASSIGNMENT)
				ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_ASSIGN | ANAFL_COPY | flags);
			else
				ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_COPY | flags);
		}
		else
			ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS | ANAFL_COPY | flags);

		if (exp->mLeft->mDecType->IsReference())
			rdec = Analyze(exp->mRight, procDec, ANAFL_LHS | ANAFL_RHS | ANAFL_COPY);
		else
			rdec = Analyze(exp->mRight, procDec, ANAFL_RHS | ANAFL_COPY);

		RegisterProc(rdec);
		return ldec;

	case EX_BINARY:
		ldec = Analyze(exp->mLeft, procDec, flags);
		rdec = Analyze(exp->mRight, procDec, flags);
		return ldec;

	case EX_RELATIONAL:
		ldec = Analyze(exp->mLeft, procDec, flags);
		rdec = Analyze(exp->mRight, procDec, flags);
		return TheBoolTypeDeclaration;

	case EX_PREINCDEC:
		return Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS);
	case EX_PREFIX:
		if (exp->mToken == TK_BINARY_AND)
			ldec = Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS | ANAFL_ADDRESS);
		else if (exp->mToken == TK_MUL)
		{
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
			ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
			return exp->mDecType;
		}
		else
			return Analyze(exp->mLeft, procDec, flags);
		break;
	case EX_POSTFIX:
		break;
	case EX_POSTINCDEC:
		return Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS);
	case EX_INDEX:
		if (exp->mDecType->mFlags & DTF_VOLATILE)
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;

		procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
#if 0
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
#else
		if (exp->mLeft->mDecType->mType == DT_TYPE_POINTER)
			ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		else if (exp->mRight->mType != EX_CONSTANT)
			ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS | ANAFL_LHS);
		else if (flags & ANAFL_ADDRESS)
			ldec = Analyze(exp->mLeft, procDec, flags);
		else
			ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
#endif
		if (ldec->mType == DT_VARIABLE || ldec->mType == DT_ARGUMENT)
			ldec = ldec->mBase;
		rdec = Analyze(exp->mRight, procDec, ANAFL_RHS);
		if (ldec->mBase)
			return ldec->mBase;
		break;
	case EX_QUALIFY:
		if (exp->mDecType->mFlags & DTF_VOLATILE)
			procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;

		if (flags & ANAFL_ADDRESS)
			Analyze(exp->mLeft, procDec, flags);
		else
			Analyze(exp->mLeft, procDec, ANAFL_RHS);
		return exp->mDecValue->mBase;
	case EX_DISPATCH:
		procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
		Analyze(exp->mLeft, procDec, flags);
		break;
	case EX_VCALL:
		exp->mType = EX_CALL;
		exp->mLeft->mDecValue = exp->mLeft->mDecValue->mVTable;
		// intentional fall through
	case EX_CALL:
	case EX_INLINE:
		if (exp->mLeft->mType == EX_CONSTANT)
		{
			if (flags & ANAFL_RHS)
				exp->mLeft->mDecValue->mOptFlags |= OPTF_VAR_USED;
			if (flags & ANAFL_LHS)
				exp->mLeft->mDecValue->mOptFlags |= OPTF_VAR_ADDRESS;
		}

		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		if ((ldec->mFlags & DTF_INTRINSIC) && !ldec->mValue)
		{

		}
		else
		{
			if (procDec->mOptFlags & OPTF_SINGLE_CALL)
				procDec->mOptFlags |= OPTF_MULTI_CALL;
			else
				procDec->mOptFlags |= OPTF_SINGLE_CALL;
			RegisterCall(procDec, ldec);
		}

		if (exp->mRight)
		{
			// Check for struct to struct forwarding

			Expression* rex = exp->mRight;
			if (rex)
			{
				Expression* prex = exp;
				while (rex && rex->mType == EX_LIST)
				{
					rex->mLeft = rex->mLeft->ConstantFold(mErrors, nullptr);
					prex = rex;
					rex = rex->mRight;
				}
				if (rex)
					prex->mRight = rex->ConstantFold(mErrors, nullptr);
				rex = exp->mRight;
			}

			Declaration* pdec = ldec->mBase->mParams;
			while (rex)
			{
				Expression	*& pex(rex->mType == EX_LIST ? rex->mLeft : rex);

				if (pdec)
				{
					if ((pdec->mFlags & DTF_FPARAM_UNUSED) && !pex->HasSideEffects() && (pex->mType != EX_CONSTANT || !pex->mDecValue->IsNullConst()))
					{
						if (pdec->mBase->IsSimpleType())
						{
							pex = new Expression(pex->mLocation, EX_CONSTANT);
							switch (pdec->mBase->mType)
							{
							case DT_TYPE_INTEGER:
							case DT_TYPE_ENUM:
							case DT_TYPE_BOOL:
								pex->mDecValue = TheZeroIntegerConstDeclaration;
								break;
							case DT_TYPE_FLOAT:
								pex->mDecValue = TheZeroFloatConstDeclaration;
								break;
							case DT_TYPE_POINTER:
							case DT_TYPE_FUNCTION:
								pex->mDecValue = TheNullptrConstDeclaration;
								break;								
							}
							pex->mDecType = pdec->mBase;
						}
					}
					if (!(pdec->mFlags & DTF_FPARAM_UNUSED) && !(pdec->mOptFlags & OPTF_VAR_NOCONST))
					{
						if (pex->mType == EX_CONSTANT && pdec->mBase->IsSimpleType() && pdec->mBase->CanAssign(pex->mDecType))
						{
							if (pdec->mOptFlags & OPTF_VAR_CONST)
							{
								if (!pex->mDecValue->IsSameValue(pdec->mValue->mDecValue))
								{
									pdec->mOptFlags |= OPTF_VAR_NOCONST;
									pdec->mOptFlags &= ~OPTF_VAR_CONST;
									if (pdec->mBase->IsIntegerType())
									{
										pdec->mOptFlags |= OPTF_VAR_RANGE_LIMITED;
										pdec->mMinValue = int64min(pex->mDecValue->mInteger, pdec->mValue->mDecValue->mInteger);
										pdec->mMaxValue = int64max(pex->mDecValue->mInteger, pdec->mValue->mDecValue->mInteger);
									}
								}
							}
							else
							{
								pdec->mValue = pex;
								pdec->mOptFlags |= OPTF_VAR_CONST;
							}
						}
						else if (pex->mType == EX_VARIABLE && pdec->mBase->mType == DT_TYPE_POINTER && pex->mDecType->mType == DT_TYPE_ARRAY && (pex->mDecValue->mFlags & (DTF_GLOBAL | DTF_STATIC)) && pdec->mBase->CanAssign(pex->mDecType))
						{
							if (pdec->mOptFlags & OPTF_VAR_CONST)
							{
								if (pdec->mValue->mType != EX_VARIABLE || pdec->mValue->mDecValue != pex->mDecValue)
								{
									pdec->mOptFlags |= OPTF_VAR_NOCONST;
									pdec->mOptFlags &= ~OPTF_VAR_CONST;
								}
							}
							else
							{
								pdec->mValue = pex;
								pdec->mOptFlags |= OPTF_VAR_CONST;
							}
						}
						else
						{
							pdec->mOptFlags |= OPTF_VAR_NOCONST;
							pdec->mOptFlags &= ~OPTF_VAR_CONST;
						}
					}
					else if (!(pdec->mFlags & DTF_FPARAM_UNUSED) && (pdec->mOptFlags & OPTF_VAR_RANGE_LIMITED))
					{
						if (pex->mType == EX_CONSTANT && pdec->mBase->IsIntegerType() && pdec->mBase->CanAssign(pex->mDecType))
						{
							pdec->mMinValue = int64min(pex->mDecValue->mInteger, pdec->mMinValue);
							pdec->mMaxValue = int64max(pex->mDecValue->mInteger, pdec->mMaxValue);
						}
						else
							pdec->mOptFlags &= ~OPTF_VAR_RANGE_LIMITED;
					}

					if (pdec->mBase->mType == DT_TYPE_STRUCT && pdec->mBase->mCopyConstructor)
					{
						if (pdec->mBase->mMoveConstructor)
						{
							AnalyzeProcedure(pdec->mBase->mMoveConstructor->mValue, pdec->mBase->mMoveConstructor);
							RegisterCall(procDec, pdec->mBase->mMoveConstructor);
						}
						AnalyzeProcedure(pdec->mBase->mCopyConstructor->mValue, pdec->mBase->mCopyConstructor);
						RegisterCall(procDec, pdec->mBase->mCopyConstructor);
					}
				}

				if (pdec && (pdec->mFlags & DTF_FPARAM_UNUSED))
					RegisterProc(Analyze(pex, procDec, 0));
				else if (pdec && pdec->mBase->IsReference())
				{
					if (!(procDec->mBase->mFlags & DTF_VIRTUAL) && pdec && pex && pex->mType == EX_VARIABLE && pex->mDecValue->mType == DT_ARGUMENT && pex->mDecValue->mBase->IsReference() && !pex->mDecValue->mForwardParam && !(pex->mDecValue->mOptFlags & OPTF_VAR_NO_FORWARD))
					{
						pex->mDecValue->mOptFlags |= OPTF_VAR_USED | OPTF_VAR_ADDRESS;
						pex->mDecValue->mForwardParam = pdec;
						pex->mDecValue->mForwardCall = ldec;
					}
					else
						RegisterProc(Analyze(pex, procDec, ANAFL_LHS | ANAFL_RHS | ANAFL_COPY));
				}
				else if (!(procDec->mBase->mFlags & DTF_VIRTUAL) && pdec && pex && pex->mType == EX_VARIABLE && pex->mDecValue->mType == DT_ARGUMENT && !pex->mDecValue->mForwardParam && !(pex->mDecValue->mOptFlags & OPTF_VAR_NO_FORWARD))
				{
					pex->mDecValue->mOptFlags |= OPTF_VAR_USED;
					pex->mDecValue->mForwardParam = pdec;
					pex->mDecValue->mForwardCall = ldec;
				}
				else
					RegisterProc(Analyze(pex, procDec, ANAFL_RHS | ANAFL_COPY));

				if (pdec)
					pdec = pdec->mNext;

				if (rex->mType == EX_LIST)
					rex = rex->mRight;
				else
					rex = nullptr;
			}
		}
		break;
	case EX_LIST:
	case EX_COMMA:
	{
		RegisterProc(Analyze(exp->mLeft, procDec, 0));
		Declaration* dec = Analyze(exp->mRight, procDec, flags);
		RegisterProc(dec);
		return dec;
	}
	case EX_RETURN:
		if (exp->mLeft)
		{
			exp->mLeft = exp->mLeft->ConstantFold(mErrors, nullptr);

			if (procDec->mOptFlags & OPTF_SINGLE_RETURN)
			{
				procDec->mOptFlags &= ~OPTF_SINGLE_RETURN;
				procDec->mOptFlags |= OPTF_MULTI_RETURN;
			}
			else if (!(procDec->mOptFlags & OPTF_MULTI_RETURN))
			{
				procDec->mOptFlags |= OPTF_SINGLE_RETURN;
				procDec->mReturn = exp;
			}

			if (procDec->mBase->mBase->IsReference())
				RegisterProc(Analyze(exp->mLeft, procDec, ANAFL_LHS | ANAFL_RHS));
			else
				RegisterProc(Analyze(exp->mLeft, procDec, ANAFL_RHS | ANAFL_COPY));

			if (procDec->mBase->mBase && procDec->mBase->mBase->mType == DT_TYPE_STRUCT && procDec->mBase->mBase->mCopyConstructor)
			{
				if (procDec->mBase->mBase->mMoveConstructor)
				{
					AnalyzeProcedure(procDec->mBase->mBase->mMoveConstructor->mValue, procDec->mBase->mBase->mMoveConstructor);
					RegisterCall(procDec, procDec->mBase->mBase->mMoveConstructor);
				}
				AnalyzeProcedure(procDec->mBase->mBase->mCopyConstructor->mValue, procDec->mBase->mBase->mCopyConstructor);
				RegisterCall(procDec, procDec->mBase->mBase->mCopyConstructor);
			}
		}
		break;
	case EX_SEQUENCE:
		do
		{
			if (exp->mType == EX_SEQUENCE)
			{
				if (exp->mLeft)
					ldec = Analyze(exp->mLeft, procDec, 0);
				exp = exp->mRight;
			}
			else
				return Analyze(exp, procDec, 0);

		} while (exp);
		break;

	case EX_SCOPE:
		Analyze(exp->mLeft, procDec, 0);
		break;

	case EX_CONSTRUCT:
		procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
		if (exp->mLeft->mLeft)
			Analyze(exp->mLeft->mLeft, procDec, 0);
		if (exp->mLeft->mRight)
			Analyze(exp->mLeft->mRight, procDec, 0);
		if (exp->mRight)
			return Analyze(exp->mRight, procDec, ANAFL_RHS);
		break;

	case EX_CLEANUP:
		Analyze(exp->mRight, procDec, 0);
		return Analyze(exp->mLeft, procDec, flags);

	case EX_WHILE:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_IF:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight->mLeft, procDec, 0);
		if (exp->mRight->mRight)
			rdec = Analyze(exp->mRight->mRight, procDec, 0);
		break;
	case EX_ELSE:
		break;
	case EX_FOR:
		if (exp->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mRight, procDec, 0);
		if (exp->mLeft->mLeft->mLeft)
			ldec = Analyze(exp->mLeft->mLeft->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, 0);
		if (exp->mLeft->mLeft->mRight)
			ldec = Analyze(exp->mLeft->mLeft->mRight, procDec, 0);
		break;
	case EX_FORBODY:
		ldec = Analyze(exp->mLeft, procDec, 0);
		if (exp->mRight)
			rdec = Analyze(exp->mRight, procDec, 0);
		break;
	case EX_DO:
		ldec = Analyze(exp->mRight, procDec, 0);
		rdec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		break;
	case EX_BREAK:
	case EX_CONTINUE:
		break;
	case EX_ASSUME:
		return Analyze(exp->mLeft, procDec, ANAFL_RHS);
		break;
	case EX_TYPE:
		break;
	case EX_TYPECAST:
		return Analyze(exp->mLeft, procDec, ANAFL_RHS);
		break;
	case EX_LOGICAL_AND:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, ANAFL_RHS);
		break;
	case EX_LOGICAL_OR:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		rdec = Analyze(exp->mRight, procDec, ANAFL_RHS);
		break;
	case EX_LOGICAL_NOT:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		break;
	case EX_ASSEMBLER:
		procDec->mOptFlags &= ~OPTF_CONST_FUNCTION;
		AnalyzeAssembler(exp, procDec);
		break;
	case EX_UNDEFINED:
		break;
	case EX_SWITCH:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		exp = exp->mRight;
		while (exp)
		{
			if (exp->mLeft->mRight)
				rdec = Analyze(exp->mLeft->mRight, procDec, 0);
			exp = exp->mRight;
		}
		break;
	case EX_CASE:
		break;
	case EX_DEFAULT:
		break;
	case EX_CONDITIONAL:
		ldec = Analyze(exp->mLeft, procDec, ANAFL_RHS);
		RegisterProc(Analyze(exp->mRight->mLeft, procDec, flags));
		RegisterProc(Analyze(exp->mRight->mRight, procDec, flags));
		break;
	}

	return TheVoidTypeDeclaration;
}

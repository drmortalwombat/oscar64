#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


typedef float (*fop)(float a, float b);

float fadd(float a, float b)
{
	return a + b;	
}

float fmul(float a, float b)
{
	return a * b;	
}


struct FNode
{
	FNode	*	left, * right;
	float		value;
	fop			call;
};

FNode	*	root;

float fevalB(FNode * f)
{
	if (f->call)
		return f->call(fevalB(f->left), fevalB(f->right));
	else
		return f->value;
}

float fevalN(FNode * f)
{
	if (f->call)
		return f->call(fevalN(f->left), fevalN(f->right));
	else
		return f->value;
}

#pragma native(fevalN)

FNode * bop(fop call, FNode * left, FNode * right)
{
	FNode	*	f = (FNode *)malloc(sizeof(FNode));
	f->call = call;
	f->left = left;
	f->right = right;
	return f;
}

FNode * bc(float value)
{
	FNode	*	f = (FNode *)malloc(sizeof(FNode));
	f->value = value;
	return f;	
}

int main(void)
{
	FNode	*	tree = bop(fadd, bop(fmul, bc(3), bc(5)), bc(6));

	printf("Eval %f, %d\n", fevalB(tree), fevalN(tree));

	assert(fevalB(tree) == 3 * 5 + 6);
	assert(fevalN(tree) == 3 * 5 + 6);

	return 0;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int fib(int a) 
{
	if (a < 2) 
		return a;
	else 
		return fib(a - 1) + fib(a - 2);
} 

struct Node
{
	char		value;
	Node	*	left, * right;
};

typedef Node *	NodePtr;


NodePtr newnode(void) 
{
	return (NodePtr)malloc(sizeof(Node));
}

Node * insert(Node * tree, char v)
{
	if (tree)
	{
		if (v < tree->value)
		{
			tree->left = insert(tree->left, v);
		}
		else
		{
			tree->right = insert(tree->right, v);
		}
	}
	else
	{
		tree = newnode();
		tree->value = v;
		tree->left = nullptr;
		tree->right = nullptr;
	}
	
	return tree;	
}

char * collect(Node * tree, char * p)
{
	if (tree)
	{
		p = collect(tree->left, p);
		*p++= tree->value;
		p = collect(tree->right, p);
	}
	
	return p;
}

void btest(void)
{
	const char	*	str = "HELLO WORLD";
	char			buff[20];
	
	int i = 0;
	Node	*	tree = nullptr;
	
	while (str[i])
	{
		tree = insert(tree, str[i]);
		i++;
	}
	
	collect(tree, buff)[0] = 0;
	
	assert(strcmp(buff, " DEHLLLOORW") == 0);	
}

int main(void)
{
	assert(fib(23) == 28657);
	
	btest();
	
	return 0;
}

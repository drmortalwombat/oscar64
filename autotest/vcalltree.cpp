#include <assert.h>

struct Node
{
	virtual float eval(void)
		{
			return 1.0e6;
		}
};

struct ConstNode : Node
{
	float	c;

	ConstNode(float c_)
		: c(c_) {}

	virtual float eval(void)
	{
		return c;
	}
};

struct BinopNode : Node
{
	Node	*	left, * right;

	BinopNode(Node * left_, Node * right_)
		: left(left_), right(right_)
		{}
};

struct AddNode : BinopNode
{
	AddNode(Node * left_, Node * right_)
		: BinopNode(left_, right_)
		{}

	virtual float eval(void)
	{
		return left->eval() + right->eval();
	}
};

struct SubNode : BinopNode
{
	SubNode(Node * left_, Node * right_)
		: BinopNode(left_, right_)
		{}

	virtual float eval(void)
	{
		return left->eval() - right->eval();
	}
};

struct MulNode : BinopNode
{
	MulNode(Node * left_, Node * right_)
		: BinopNode(left_, right_)
		{}

	virtual float eval(void)
	{
		return left->eval() * right->eval();
	}
};

int main(void)
{
	Node	*	n = 
		new SubNode(
			new MulNode(new ConstNode(4), new ConstNode(5)),
			new AddNode(new ConstNode(12), new ConstNode(7)));

	assert(n->eval() == 1.0);

	return 0;
}

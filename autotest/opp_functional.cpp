#include <opp/functional.h>

class Node
{
public:
	virtual int eval(void) const = 0;
};

class ConstNode : public Node
{
private:
	int 	v;
public:
	ConstNode(int v_) : v(v_) {}
	virtual int eval(void) const
	{
		return v;
	}
};

class BinaryNode : public Node
{
private:
	opp::function<int(int, int)>	op;
	Node	*	left, * right;
public:
	BinaryNode(opp::function<int(int, int)> op_, Node * left_, Node * right_);

	virtual int eval(void) const
	{
		return op(left->eval(), right->eval());
	}
};

inline BinaryNode::BinaryNode(opp::function<int(int, int)> op_, Node * left_, Node * right_)
	: op(op_), left(left_), right(right_) {}

int main(void)
{
	Node	*	s1 = 
		new BinaryNode([=](int a, int b){return a + b;},
			new ConstNode(7), new ConstNode(11)
		);

	return s1->eval() - 18;
}

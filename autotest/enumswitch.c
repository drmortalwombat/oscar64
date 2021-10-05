enum E {
	e1, e2, e3, e4
};

int check(E e)
{
	switch(e)
	{
	case e1:
		return 10;
	case e2:
		return 20;
	case e3:
		return 30;
	default:
		return 100;
	}
}

int main(void)
{
	return check(e1) + check(e2) + check(e3) + check(e4) - 160;
}

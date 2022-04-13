
int (*funcs[10])(int);

#assign x 0
#repeat
int f##x(int i)
{
	return i + x;
}
#assign x x + 1
#until x == 10

int test(int k)
{
	for(char i=0; i<10; i++)
		k = funcs[i](k);
	return k;
}

int main(void)
{
#assign x 0
#repeat
	funcs[x] = f##x;
#assign x x + 1
#until x == 10

	int k = test(-45);
	return k;
}

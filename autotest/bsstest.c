
#include <string.h>

char ch[100];
char p[] = "HELLO";
int v[10];
int w[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

int sum(int * k)
{
	int s = 0;
	for(int i=0; i<10; i++)
		s += k[i];
	return s;	
}

int main(void)
{
	strcpy(ch, p);
	strcat(ch, " WORLD");

	for(int i=0; i<10; i++)
		v[i] = w[i];
	
	return strcmp(ch, "HELLO WORLD") + sum(v) - 55;
}


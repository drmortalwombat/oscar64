#include <assert.h>

char	buffer[200];
char	buffer_fill;


void buf_init(void)
{
	for(int i=0; i<190; i++)
		buffer[i] = i + 0x10;
	buffer_fill = 190;
}

void buf_check_insert(char at, char ch)
{
	assert(buffer_fill == 191);
	for(int i=0; i<at; i++)
		assert(buffer[i] == i + 0x10);
	assert(buffer[at] == ch);
	for(int i=at + 1; i<191; i++)
		assert(buffer[i] == i + 0x0f);
}

void buf_check_delete(char at, char n)
{
	assert(buffer_fill == 190 - n);
	for(int i=0; i<at; i++)
		assert(buffer[i] == i + 0x10);
	for(int i=at; i<190 - n; i++)
		assert(buffer[i] == i + 0x10 + n);
}


void buf_insert(char at, char ch)
{
	for(char i=buffer_fill; i>at; i--)
		buffer[i] = buffer[i - 1];
	buffer[at] = ch;
	buffer_fill++;
}

char ptr_insert(char * buf, char fill, char at, char ch)
{
	for(char i=fill; i>at; i--)
		buf[i] = buf[i - 1];
	buf[at] = ch;
	return fill + 1;
}

void buf_delete_1(char at)
{
	buffer_fill--;
	for(char i=at; i<buffer_fill; i++)
		buffer[i] = buffer[i + 1];
}

void buf_delete_n(char at, char n)
{
	buffer_fill -= n;
	for(char i=at; i<buffer_fill; i++)
		buffer[i] = buffer[i + n];
}

char ptr_delete_1(char * buf, char fill, char at)
{
	fill --;
	for(char i=at; i<fill; i++)
		buf[i] = buf[i + 1];
	return fill;
}

char ptr_delete_n(char * buf, char fill, char at, char n)
{
	fill -= n;
	for(char i=at; i<fill; i++)
		buf[i] = buf[i + n];
	return fill;
}


int main(void)
{
	buf_init(); buf_insert(0, 0xff); buf_check_insert(0, 0xff);
	buf_init(); buf_insert(37, 0xff); buf_check_insert(37, 0xff);
	buf_init(); buf_insert(190, 0xff); buf_check_insert(190, 0xff);

	buf_init(); buffer_fill = ptr_insert(buffer, buffer_fill, 0, 0xff); buf_check_insert(0, 0xff);
	buf_init(); buffer_fill = ptr_insert(buffer, buffer_fill, 37, 0xff); buf_check_insert(37, 0xff);
	buf_init(); buffer_fill = ptr_insert(buffer, buffer_fill, 190, 0xff); buf_check_insert(190, 0xff);

	buf_init(); buf_delete_1(0); buf_check_delete(0, 1);
	buf_init(); buf_delete_1(37); buf_check_delete(37, 1);
	buf_init(); buf_delete_1(189); buf_check_delete(189, 1);

	buf_init(); buf_delete_n(0, 10); buf_check_delete(0, 10);
	buf_init(); buf_delete_n(37, 10); buf_check_delete(37, 10);
	buf_init(); buf_delete_n(180, 10); buf_check_delete(189, 10);

	buf_init(); buffer_fill = ptr_delete_1(buffer, buffer_fill, 0); buf_check_delete(0, 1);
	buf_init(); buffer_fill = ptr_delete_1(buffer, buffer_fill, 37); buf_check_delete(37, 1);
	buf_init(); buffer_fill = ptr_delete_1(buffer, buffer_fill, 189); buf_check_delete(189, 1);

	buf_init(); buffer_fill = ptr_delete_n(buffer, buffer_fill, 0, 10); buf_check_delete(0, 10);
	buf_init(); buffer_fill = ptr_delete_n(buffer, buffer_fill, 37, 10); buf_check_delete(37, 10);
	buf_init(); buffer_fill = ptr_delete_n(buffer, buffer_fill, 180, 10); buf_check_delete(189, 10);

	return 0;
}
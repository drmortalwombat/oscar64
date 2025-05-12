#include <assert.h>

__noinline unsigned long rollright32(unsigned long a) {
    unsigned long tmp = a & 1;
    return ( a >> 1) + (tmp << 31);
}

__noinline unsigned rollright16(unsigned a) {
    unsigned tmp = a & 1;
    return ( a >> 1) + (tmp << 15);
}

__noinline char rollright8(char a) {
    char tmp = a & 1;
    return ( a >> 1) + (tmp << 7); 
}

__noinline unsigned long rollleft32(unsigned long a) {
    unsigned long tmp = (a >> 31) & 1;
    return ( a << 1) + tmp; 
}

__noinline unsigned rollleft16(unsigned a) {
    unsigned tmp = (a >> 15) & 1;
    return ( a << 1) + tmp; 
}

__noinline char rollleft8(char a) {
    char tmp = (a >> 7) & 1;
    return ( a << 1) + tmp; 
}

int main() {
    unsigned long lv = 0x12345678ul;
    unsigned val = 0x1234;
    char c=0x12;

    unsigned long lvt[33];
    unsigned valt[17];
    char ct[9];

    lvt[0] = lv;
    valt[0] = val;
    ct[0] = c;  

    assert(rollleft8(rollright8(c)) == c);
    assert(rollleft16(rollright16(val)) == val);
    assert(rollleft32(rollright32(lv)) == lv);
     
    for(int i=0; i<32; i++)
        lvt[i + 1] = rollright32(lvt[i]);
    for(int i=0; i<16; i++)
        valt[i + 1] = rollright16(valt[i]);
    for(int i=0; i<8; i++)
        ct[i + 1] = rollright8(ct[i]);

    for(int i=0; i<=32; i++)
    {        
        assert(lvt[32 - i] == lv);
        lv = rollleft32(lv);
    }

    for(int i=0; i<=16; i++)
    {        
        assert(valt[16 - i] == val);
        val = rollleft16(val);
    }

    for(int i=0; i<=8; i++)
    {        
        assert(ct[8 - i] == c);
        c = rollleft8(c);
    }

    return 0;
}

CC=../bin/oscar64
CXX=$(CC)
SRCS=$(wildcard *.c *.cpp)
EXES=$(patsubst %.c,%,$(SRCS))
EXES:=$(patsubst %.cpp,%,$(EXES))

all: $(EXES)

%: %.c
	$(CC) -e -bc $<
	$(CC) -e -n $<
	$(CC) -e -O2 -bc $<
	$(CC) -e -O2 -n $<
	$(CC) -e -O0 -bc $<
	$(CC) -e -O0 -n $<
	$(CC) -e -Os -bc $<
	$(CC) -e -Os -n $<
	$(CC) -e -O3 -bc $<
	$(CC) -e -O3 -n $<

%: %.cpp
	$(CXX) -e -bc $<
	$(CXX) -e -n $<
	$(CXX) -e -O2 -bc $<
	$(CXX) -e -O2 -n $<
	$(CXX) -e -O0 -bc $<
	$(CXX) -e -O0 -n $<
	$(CXX) -e -Os -bc $<
	$(CXX) -e -Os -n $<
	$(CXX) -e -O3 -bc $<
	$(CXX) -e -O3 -n $<

# testb
bitshifttes: bitshifttest.c
	$(CC) -e -bc $<
	$(CC) -e -bc -O2 $<
	$(CC) -e -bc -O0 $<
	$(CC) -e -bc -Os $<
	$(CC) -e -bc -O3 $<
	$(CC) -e -n $<

# testn
stripedarraytes: stripedarraytest.c
	$(CC) -e -O2 -n $<
	$(CC) -e -O0 -n $<
	$(CC) -e -Os -n $<
	$(CC) -e -O3 -n $<

clean:
	$(RM) *.asm *.bcs *.int *.lbl *.map *.prg

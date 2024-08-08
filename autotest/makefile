SRCS=$(filter-out opp_part1.cpp opp_part2.cpp, $(wildcard *.c *.cpp))
EXES=$(patsubst %.c,%,$(SRCS))
EXES:=$(patsubst %.cpp,%,$(EXES))

all: $(EXES)

%: %.c
	$(OSCAR64_CC) -e -bc $<
	$(OSCAR64_CC) -e -n $<
	$(OSCAR64_CC) -e -O2 -bc $<
	$(OSCAR64_CC) -e -O2 -n $<
	$(OSCAR64_CC) -e -O0 -bc $<
	$(OSCAR64_CC) -e -O0 -n $<
	$(OSCAR64_CC) -e -Os -bc $<
	$(OSCAR64_CC) -e -Os -n $<
	$(OSCAR64_CC) -e -O3 -bc $<
	$(OSCAR64_CC) -e -O3 -n $<

%: %.cpp
	$(OSCAR64_CXX) -e -bc $<
	$(OSCAR64_CXX) -e -n $<
	$(OSCAR64_CXX) -e -O2 -bc $<
	$(OSCAR64_CXX) -e -O2 -n $<
	$(OSCAR64_CXX) -e -O0 -bc $<
	$(OSCAR64_CXX) -e -O0 -n $<
	$(OSCAR64_CXX) -e -Os -bc $<
	$(OSCAR64_CXX) -e -Os -n $<
	$(OSCAR64_CXX) -e -O3 -bc $<
	$(OSCAR64_CXX) -e -O3 -n $<

# testb
bitshifttest: bitshifttest.c
	$(OSCAR64_CC) -e -bc $<
	$(OSCAR64_CC) -e -bc -O2 $<
	$(OSCAR64_CC) -e -bc -O0 $<
	$(OSCAR64_CC) -e -bc -Os $<
	$(OSCAR64_CC) -e -bc -O3 $<
	$(OSCAR64_CC) -e -n $<

# testn
stripedarraytest: stripedarraytest.c
	$(OSCAR64_CC) -e -O2 -n $<
	$(OSCAR64_CC) -e -O0 -n $<
	$(OSCAR64_CC) -e -Os -n $<
	$(OSCAR64_CC) -e -O3 -n $<

autorefreturn: autorefreturn.cpp
	$(OSCAR64_CC) -e -O2 -n $<
	$(OSCAR64_CC) -e -O0 -n $<
	$(OSCAR64_CC) -e -Os -n $<
	$(OSCAR64_CC) -e -O3 -n $<

clean:
	@$(RM) *.asm *.bcs *.int *.lbl *.map *.prg

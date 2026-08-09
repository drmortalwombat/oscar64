SRCS=$(filter-out ambiguousoverload_trigger.c opp_part1.cpp opp_part2.cpp, $(wildcard *.c *.cpp))
EXES=$(patsubst %.c,%,$(SRCS))
EXES:=$(patsubst %.cpp,%,$(EXES))

all: $(EXES) diskimagefulltest

DISK_FULL_RESOURCE=../samples/resources/blumba2.bin
DISK_FULL_FILES=$(foreach n,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18,-f=$(DISK_FULL_RESOURCE))

.PHONY: diskimagefulltest
diskimagefulltest:
	@output="$$($(OSCAR64_CC) -d64=diskimagefulltest.d64 $(DISK_FULL_FILES) 2>&1)"; status=$$?; \
	$(RM) diskimagefulltest.d64; \
	if [ $$status -ne 20 ] || ! printf '%s\n' "$$output" | grep -Fq "oscar64: error" || ! printf '%s\n' "$$output" | grep -Fq "Disk image capacity exceeded"; then \
		printf '%s\n' "$$output"; \
		printf 'Expected disk-capacity error and exit status 20, got %s\n' "$$status"; \
		exit 1; \
	fi

%: %.c
	$(OSCAR64_CC) -ea -g -bc $<
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -bc $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -bc $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -bc $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -bc $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

%: %.cpp
	$(OSCAR64_CXX) -ea -g -bc $<
	$(OSCAR64_CXX) -ea -g -n $<
	$(OSCAR64_CXX) -ea -g -O2 -bc $<
	$(OSCAR64_CXX) -ea -g -O2 -n $<
	$(OSCAR64_CXX) -ea -g -O0 -bc $<
	$(OSCAR64_CXX) -ea -g -O0 -n $<
	$(OSCAR64_CXX) -ea -g -Os -bc $<
	$(OSCAR64_CXX) -ea -g -Os -n $<
	$(OSCAR64_CXX) -ea -g -O3 -bc $<
	$(OSCAR64_CXX) -ea -g -O3 -n $<

# testb
bitshifttest: bitshifttest.c
	$(OSCAR64_CC) -ea -g -bc $<
	$(OSCAR64_CC) -ea -g -bc -O2 $<
	$(OSCAR64_CC) -ea -g -bc -O0 $<
	$(OSCAR64_CC) -ea -g -bc -Os $<
	$(OSCAR64_CC) -ea -g -bc -O3 $<
	$(OSCAR64_CC) -ea -g -n $<

# testn
stripedarraytest: stripedarraytest.c
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

stripedstructtest: stripedstructtest.c
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

structreturncontrolflow: structreturncontrolflow.c
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

autorefreturn: autorefreturn.cpp
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

copyconstructor: copyconstructor.cpp
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

andmultest: andmultest.cpp
	$(OSCAR64_CC) -ea -g -n $<
	$(OSCAR64_CC) -ea -g -O2 -n $<
	$(OSCAR64_CC) -ea -g -O0 -n $<
	$(OSCAR64_CC) -ea -g -Os -n $<
	$(OSCAR64_CC) -ea -g -O3 -n $<

# testi
volatiletest: volatiletest.c
	$(OSCAR64_CC) -eai -g -n $<
	$(OSCAR64_CC) -eai -g -O2 -n $<
	$(OSCAR64_CC) -eai -g -O0 -n $<
	$(OSCAR64_CC) -eai -g -Os -n $<
	$(OSCAR64_CC) -eai -g -O3 -n $<

variadic-macros: variadic_macros.c
	$(OSCAR64_CC) -eai -g -n $<
	$(OSCAR64_CC) -eai -g -O2 -n $<
	$(OSCAR64_CC) -eai -g -O0 -n $<
	$(OSCAR64_CC) -eai -g -Os -n $<
	$(OSCAR64_CC) -eai -g -O3 -n $<

clean:
	@$(RM) *.asm *.bcs *.d64 *.int *.lbl *.map *.prg

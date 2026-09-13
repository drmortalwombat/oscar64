SRCS=$(filter-out ambiguousoverload_trigger.c opp_part1.cpp opp_part2.cpp, $(wildcard *.c *.cpp))
EXES=$(patsubst %.c,%,$(SRCS))
EXES:=$(patsubst %.cpp,%,$(EXES))

all: $(EXES) diskimagefulltest diskimaged81test

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

.PHONY: diskimaged81test
diskimaged81test:
	@$(OSCAR64_CC) -d81=diskimaged81test.d81 -f=$(DISK_FULL_RESOURCE)
	@size="$$(wc -c < diskimaged81test.d81)"; \
	header="$$(od -An -tx1 -j 399360 -N 3 diskimaged81test.d81 | tr -d ' \n')"; \
	bam="$$(od -An -tx1 -j 399616 -N 3 diskimaged81test.d81 | tr -d ' \n')"; \
	directory="$$(od -An -tx1 -j 400128 -N 5 diskimaged81test.d81 | tr -d ' \n')"; \
	$(RM) diskimaged81test.d81; \
	if [ "$$size" -ne 819200 ] || [ "$$header" != "280344" ] || [ "$$bam" != "280244" ] || [ "$$directory" != "00ff822701" ]; then \
		printf 'Invalid D81 layout: size=%s header=%s bam=%s directory=%s\n' "$$size" "$$header" "$$bam" "$$directory"; \
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
	@$(RM) *.asm *.bcs *.d64 *.d81 *.int *.lbl *.map *.prg

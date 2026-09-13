SRCS=$(filter-out ambiguousoverload_trigger.c opp_part1.cpp opp_part2.cpp, $(wildcard *.c *.cpp))
EXES=$(patsubst %.c,%,$(SRCS))
EXES:=$(patsubst %.cpp,%,$(EXES))

all: $(EXES) diskimaged64test diskimagefulltest diskimaged71test diskimaged81test

DISK_FULL_RESOURCE=../samples/resources/blumba2.bin
DISK_FULL_FILES=$(foreach n,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18,-f=$(DISK_FULL_RESOURCE))
DISK_D71_FILES=$(foreach n,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17,-f=$(DISK_FULL_RESOURCE))
DISK_D64_RESOURCE=diskimagefixture.dat

.PHONY: diskimaged64test
diskimaged64test:
	@$(OSCAR64_CC) -d64=diskimaged64test.d64 -f=$(DISK_D64_RESOURCE)
	@size="$$(wc -c < diskimaged64test.d64)"; \
	header="$$(od -An -tx1 -j 91392 -N 4 diskimaged64test.d64 | tr -d ' \n')"; \
	bam="$$(od -An -tx1 -j 91460 -N 4 diskimaged64test.d64 | tr -d ' \n')"; \
	disk_info="$$(od -An -tx1 -j 91536 -N 27 diskimaged64test.d64 | tr -d ' \n')"; \
	directory="$$(od -An -tx1 -j 91648 -N 32 diskimaged64test.d64 | tr -d ' \n')"; \
	data_header="$$(od -An -tx1 -j 88576 -N 2 diskimaged64test.d64 | tr -d ' \n')"; \
	payload=valid; \
	dd if=diskimaged64test.d64 bs=1 skip=88578 count=43 2>/dev/null | cmp -n 43 - $(DISK_D64_RESOURCE) >/dev/null || payload=invalid; \
	$(RM) diskimaged64test.d64; \
	if [ "$$size" -ne 174848 ] || [ "$$header" != "12014100" ] || [ "$$bam" != "14fffb1f" ] || [ "$$disk_info" != "4449534b494d41474544363454455354a0a0434ea03241a0a0a0a0" ] || [ "$$directory" != "00ff82110a4449534b494d414745464958545552450000000000000000000100" ] || [ "$$data_header" != "002c" ] || [ "$$payload" != "valid" ]; then \
		printf 'Invalid D64 layout: size=%s header=%s bam=%s disk_info=%s directory=%s data_header=%s payload=%s\n' "$$size" "$$header" "$$bam" "$$disk_info" "$$directory" "$$data_header" "$$payload"; \
		exit 1; \
	fi

.PHONY: diskimagefulltest
diskimagefulltest:
	@output="$$($(OSCAR64_CC) -d64=diskimagefulltest.d64 $(DISK_FULL_FILES) 2>&1)"; status=$$?; \
	$(RM) diskimagefulltest.d64; \
	if [ $$status -ne 20 ] || ! printf '%s\n' "$$output" | grep -Fq "oscar64: error" || ! printf '%s\n' "$$output" | grep -Fq "Disk image capacity exceeded"; then \
		printf '%s\n' "$$output"; \
		printf 'Expected disk-capacity error and exit status 20, got %s\n' "$$status"; \
		exit 1; \
	fi

.PHONY: diskimaged71test
diskimaged71test:
	@$(OSCAR64_CC) -d71=diskimaged71test.d71 $(DISK_D71_FILES)
	@size="$$(wc -c < diskimaged71test.d71)"; \
	header="$$(od -An -tx1 -j 91392 -N 4 diskimaged71test.d71 | tr -d ' \n')"; \
	side_count="$$(od -An -tx1 -j 91613 -N 1 diskimaged71test.d71 | tr -d ' \n')"; \
	side_bam="$$(od -An -tx1 -j 266240 -N 3 diskimaged71test.d71 | tr -d ' \n')"; \
	directory="$$(od -An -tx1 -j 91648 -N 5 diskimaged71test.d71 | tr -d ' \n')"; \
	$(RM) diskimaged71test.d71; \
	if [ "$$size" -ne 349696 ] || [ "$$header" != "12014180" ] || [ "$$side_count" != "05" ] || [ "$$side_bam" != "071800" ] || [ "$$directory" != "120b82110a" ]; then \
		printf 'Invalid D71 layout: size=%s header=%s side_count=%s side_bam=%s directory=%s\n' "$$size" "$$header" "$$side_count" "$$side_bam" "$$directory"; \
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
	@$(RM) *.asm *.bcs *.d64 *.d71 *.d81 *.int *.lbl *.map *.prg

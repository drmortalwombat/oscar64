# oscar64
 Optimizing small space C Compiler Assembler and Runtime for C64

## History and motivation

It is a sad fact that the 6502 used in the Commodore64 and other home computers of the 80s has a poor code density when it comes to 16 bit code.  The C standard requires computations to be made with ints which work best if they have the same size as a pointer.  

The 6502 also has a very small stack of 256 bytes which cannot be easily addressed and thus cannot be used for local variables.  Therefore a second stack for variables has to be maintained, resulting in costly indexing operations.

A C compiler for the 6502 thus generates large binaries if it translates to native machine code.  The idea for the **oscar64** compiler is to translate the C source to an intermediate 16 bit byte code with the option to use native machine code for crucial functions.  Using embedded assembly for runtime libraries or critical code should also be possible.

The resulting compiler is a frankenstein constructed from a converted javascript parser a intermediate code optimizer based on a 15 year old compiler for 64bit x86 code and some new components for the backend.

The performance of interpreted code is clearly not as good as native machine code but the penalty for 16bit code is around 40-50% and less than 10% for floating point.  Code that can use 8bit my suffer up to a factor of 10 to 20.

## Limits and Errors

The first release of the compiler is severely limited considering it is only two weeks old, so there is quite a lot missing or broken.  I hope to cross out most of the problems in the coming weeks.

### Language

* No union type
* No long integer
* No struct function return
* Missing const checks for structs and enums
* No static variables in functions
* Missing warnings for all kind of abuses
* no #if in preprocessor

### Linker

* No explicit sections for code, data bss or stack
* No media file import

### Standard Libraries

* Limited formatting in printf
* No file functions

### Runtime

* No INF and NaN support for floats
* Underflow in float multiply and divide not checked
* Basic zero page variables not restored on stop/restore

### Optimizing

* All global variables are considered volatile
* No loop opmtimization
* Static const not propagated
* Poor bookeeping of callee saved registers
* Missing livetime reduction of intermediates
* No block domination analysis
* No register use for arguments

### Intermediate code generation

* No check for running out of temporary registers
* Wasted 7 codes for far jumps

### Native code generation

* Missing

## Implementation Details

The compiler does a full program compile, the linker step is part of the compilation.  It knows all functions during the compilation run and includes only reachable code in the output.  Source files are added to the build with the help of a pragma:
		
    #pragma compile("stdio.c")


The byte code interpreter is compiled by the compiler itself and placed in the source file "crt.c".  Functions implementing byte codes are marked with a pragma:

    #pragma	bytecode(BC_CONST_P8, inp_const_p8)

The functions are written in 6502 assembly with the __asm keyword

    __asm inp_const_p8
    {
    lda	(ip), y
    tax
    iny
    lda	(ip), y
    sta	$00, x
    lda	#0
    sta	$01, x
    iny
    jmp	startup.exec
    }

The current byte code program counter is (ip),y. The interpreter loop guarantees that y is always <= 128 and can thus be used to index the additional byte code arguments without the need to check the 16 bit pointer.  The interpreter loop itself is quite compact and takes 21 cycles (including the final jump of the byte code function itself).  Moving it to zero page would reduce this by another two cycles but is most likely not worth the waste of temporary space.

    exec:
        lda	(ip), y
        sta	execjmp + 1
        iny		
        bmi	incip	
    execjmp:
        jmp 	(0x0900)

The intermediate code generator assumes a large number of registers so the zero page is used for this purpose.  The allocation is not yet final:

**0x02-0x02** spilling of y register
**0x03-0x09** workspace for mul/div and floating point routines
**0x19-0x1a** instruction pointer
**0x1b-0x1e** integer and floating point accumulator
**0x1f-0x22** pointers for indirect addressing
**0x23-0x24** stack pointer
**0x25-0x26** frame pointer
**0x43-0x52** caller saved registers
**0x53-0x8f** callee saved registers






# oscar64
 Optimizing small space C Compiler Assembler and Runtime for C64

## History and motivation

It is a sad fact that the 6502 used in the Commodore64 and other home computers of the 80s has a poor code density when it comes to 16 bit code.  The C standard requires computations to be made with ints which work best if they have the same size as a pointer.  

The 6502 also has a very small stack of 256 bytes which cannot be easily addressed and thus cannot be used for local variables.  Therefore a second stack for variables has to be maintained, resulting in costly indexing operations.

A C compiler for the 6502 thus generates large binaries if it translates to native machine code.  The idea for the **oscar64** compiler is to translate the C source to an intermediate 16 bit byte code with the option to use native machine code for crucial functions.  Using embedded assembly for runtime libraries or critical code should also be possible.

The resulting compiler is a frankenstein constructed from a converted javascript parser a intermediate code optimizer based on a 15 year old compiler for 64bit x86 code and some new components for the backend.

The performance of interpreted code is clearly not as good as native machine code but the penalty for 16bit code is around 40-50% and less than 10% for floating point.  Code that can use 8bit my suffer up to a factor of 10 to 20.

The goal is to implement the actual C standard and not some subset for performance reasons.  So the compiler must support:

* Floating point
* Recursion
* Multi dimensional arrays
* Pointer to structs


## Limits and Errors

There are still several open areas, but most targets have been reached.  The current Dhrystone performance is 35 iterations per second with byte code (12259) and 203 iterations with native code (13572 Bytes).

### Language

* No struct function return
* Missing const checks for structs and enums
* Missing warnings for all kind of abuses

### Linker

* No media file import

### Standard Libraries

* No file functions

### Runtime

* No INF and NaN support for floats
* Underflow in float multiply and divide not checked
* Basic zero page variables not restored on stop/restore

### Optimizing

* Simple loop opmtimization
* Partial block domination analysis
* Auto variables placed on fixed stack for known call sequence

### Intermediate code generation

* No check for running out of temporary registers
* Wasted 7 codes for far jumps

### Native code generation

* More byte operation optimisation required
* Simple loop detection and optimisation not complete

## Compiler arguments

The compiler is command line driven, and creates an executable .prg file.

    oscar64 {-i=includePath} [-o=output.prg] [-rt=runtime.c] [-e] [-n] [-dSYMBOL[=value]] {source.c}
    
* -i : additional include paths
* -o : optional output file name
* -rt : alternative runtime library, replaces the crt.c
* -e : execute the result in the integrated emulator
* -n : create pure native code for all functions
* -d : define a symbol (e.g. NOFLOAT or NOLONG to avoid float/long code in printf)

A list of source files can be provided.

## Console input and output

The C64 does not use ASCII it uses a derivative called PETSCII.  There are two fonts, one with uppercase and one with uppercase and lowercase characters.  It also used CR (13) as line terminator instead of LF (10).  The stdio and conio libaries can perform translations.

The translation mode is selected in conio with the variable "giocharmap" and the function "iocharmap" which will also switch the font.

	iocharmap(IOCHM_PETSCII_2);
	printf("Hello World\");
	
Will switch to the lowercase PETSCII font and translate the strings while printing.

Input from the console will also be translated accordingly.

## Inline Assembler

Inline assembler can be embedded inside of any functions, regardles of their compilation target of byte code or native.  

### Accessing variables in assembler

Access to local variables and parameters is done with zero page registers, global variables are accessed using absolute addressing.

    void putchar(char c)
    {
        __asm {
            lda c
            bne w1
            lda #13
        w1:
            jsr 0xffd2
        }
    }

A function return value can be provided in the zero page addresses ACCU (+0..+3).

    char getchar(void)
    {
        __asm {
            jsr 0xffcf
            sta accu
            lda #0
            sta accu + 1
            }
    }

Labels are defined with a colon after the name.  Pure assembler functions can be defined outside of the scope of a function and accessed using their name inside of other assembler function.  One can e.g. set up an interrupt

### Interrupt routines

The C compiler will not generate good interrupt code, it is simply too greedy with the zero page registers.  Interrupt code should therefore be written in assembler.


	#include <math.h>

    // Next line for interrupt
    char npos;

    // Interrupt routine
    __asm irq
    {   
        lda $d019   // Check if it is raster IRQ
        and #$01
        beq w1
        
        inc $d020   // Start colored section
        inc $d021
        
        ldx #20     // Wait for 2/3 lines
    l1: dex
        bne l1
        
        dec $d020   // End colored section
        dec $d021
        
        lda npos    // Setup next interrupt
        sta $d012
    w1:
        asl $d019   // Ack interrupt
        
        jmp $ea31   // System IRQ routine
    }

    int main(void)
    {
        __asm { sei }   // Disable interrupt
        
        
        *(void **)0x0314 = irq;     // Install interrupt routine
        *(char *)0xd01a = 1;        // Enable raster interrupt
        *(char *)0xd011 &= 0x7f;    // Set raster line for IRQ
        *(char *)0xd012 = 100;

        npos = 100;
        
        __asm { cli }   // Re-enable interrupt
        
        // Move the interrupt raster line up/down
        float f = 0;
        while (true)
        {
            npos = 130 + (int)(100 * sin(f));
            f += 0.1;
        }
        
        return 0;
    }

## Implementation Details

The compiler does a full program compile, the linker step is part of the compilation.  It knows all functions during the compilation run and includes only reachable code in the output.  Source files are added to the build with the help of a pragma:
        
    #pragma compile("stdio.c")

The character map for string and char constants can be changed with a pragma to match a custon character set or PETSCII.

    #pragma charmap(char, code [,count])

The byte code interpreter is compiled by the compiler itself and placed in the source file "crt.c".  Functions implementing byte codes are marked with a pragma:

    #pragma bytecode(BC_CONST_P8, inp_const_p8)

The functions are written in 6502 assembly with the __asm keyword

    __asm inp_const_p8
    {
		lda (ip), y
		tax
		iny
		lda (ip), y
		sta $00, x
		lda #0
		sta $01, x
		iny
		jmp startup.exec
    }

The current byte code program counter is (ip),y. The interpreter loop guarantees that y is always <= 128 and can thus be used to index the additional byte code arguments without the need to check the 16 bit pointer.  The interpreter loop itself is quite compact and takes 21 cycles (including the final jump of the byte code function itself).  Moving it to zero page would reduce this by another two cycles but is most likely not worth the waste of temporary space.

    exec:
        lda (ip), y
        sta execjmp + 1
        iny     
        bmi incip   
    execjmp:
        jmp     (0x0900)

The intermediate code generator assumes a large number of registers so the zero page is used for this purpose.  The allocation is not yet final:

* **0x02-0x02** spilling of y register
* **0x03-0x09** workspace for mul/div and floating point routines
* **0x19-0x1a** instruction pointer
* **0x1b-0x1e** integer and floating point accumulator
* **0x1f-0x22** pointers for indirect addressing
* **0x23-0x24** stack pointer
* **0x25-0x26** frame pointer
* **0x43-0x52** caller saved registers
* **0x53-0x8f** callee saved registers

Routines can be marked to be compiled to 6502 machine code with the native pragma:

    void Plot(int x, int y)
    {
        (*Bitmap)[y >> 3][x >> 3][y & 7] |= 0x80 >> (x & 7);
    }

    #pragma native(Plot)





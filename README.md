# oscar64
 Optimizing small space C Compiler Assembler and Runtime for C64

## History and motivation

It is a sad fact that the 6502 used in the Commodore64 and other home computers of the 80s is widely believet to have a poor code density when it comes to compiled or wider than eight bit code.  The C standard requires computations to be made with ints which work best if they have the same size as a pointer.  

The 6502 also has a very small stack of 256 bytes which cannot be easily addressed and thus cannot be used for local variables.  Therefore a second stack for variables has to be maintained, resulting in costly indexing operations.  The 6502 is also pretty poor when it comes to indexed operations, it has no index with constant offset addressing mode, and requires the y register to be used for indexing.

Most C compilers for the 6502 thus generate large binaries when translating to native machine code.  The original idea for the **oscar64** compiler was to translate the C source to an intermediate 16 bit byte code with the option to use native machine code for crucial functions.  Using embedded assembly for runtime libraries or critical code should also be possible.

The performance of interpreted code is clearly not as good as native machine code but the penalty for 16bit code is around 40-50% and less than 10% for floating point.  Code that can use 8bit may suffer up to a factor of 10 to 20.

The goal was also to implement the C99 standard and not some subset for performance reasons.  So the compiler must support:

* Floating point
* Recursion
* Multi dimensional arrays
* Pointer to structs

After extensive optimizations it turns out, that the interpreted code is not significantly smaller than the native code in most scenarios (although there are cases where the difference is significant).  
 

## Limits and Errors

There are still several open areas, but most targets have been reached.  The current Dhrystone performance is 81 iterations per second with byte code (11108) and 354 iterations with native code (10965 Bytes).  This clearly shows that Dhrystone is not a valid benchmark for optimizing compilers, because it puts the 6502 on par with a 4MHz 8088 or 68k, which it clearly is not.

### Language

* Missing const checks for structs and enums
* Missing warnings for all kind of abuses

### Linker

* No external libraries

### Standard Libraries

* No standard file functions, but CBM based file ops

### Runtime

* No NaN support for floats
* Basic zero page variables not restored on stop/restore

### Intermediate code generation

* No check for running out of temporary registers

### Native code generation

## Installation and Usage

### Installing on windows

A windows installer is provided with the release, the compiler is installed into "%programfiles(x86)%\oscar64\bin\oscar64".  When not using batch or make files, it might be a good idea to add the folder to the path environment variable.

### Building

The compiler can also built using MSVC or GCC.  A visual studio project and a makefile are part of the source repository. The makefile is in the make folder.


### Compiler arguments

The compiler is command line driven, and creates an executable .prg file.

    oscar64 {-i=includePath} [-o=output.prg] [-rt=runtime.c] [-tf=format] [-e] [-n] [-dSYMBOL[=value]] {source.c}
    
* -v : verbose output for diagnostics
* -i : additional include paths
* -o : optional output file name
* -rt : alternative runtime library, replaces the crt.c
* -e : execute the result in the integrated emulator
* -ep : execute and profile the result in the integrated emulator
* -n : create pure native code for all functions
* -d : define a symbol (e.g. NOFLOAT or NOLONG to avoid float/long code in printf)
* -O1 or -O : default optimizations
* -O0: disable optimizations
* -O2: more aggressive speed optimizations including auto inline of small functions
* -O3: aggressive optimization for speed
* -Os: optimize for size
* -tf: target format, may be prg, crt or bin

A list of source files can be provided.

## Language extensions

The compiler has various extensions to simplify developing for the C64.

### Embedding binary data

The compiler supports the #embed preprocessor directive to import binary data.  It converts a section of an external binary file into a sequence of numbers that can be placed into an initializer of an array.

	byte data[] = {
	
		#embed "data.bin"
		
	};

A section of the file can be selected by providing a limit and or an offset into the file before the file name.

	byte data[] = {
	
		#embed 4096 126 "data.bin"
		
	};


### Console input and output

The C64 does not use ASCII it uses a derivative called PETSCII.  There are two fonts, one with uppercase and one with uppercase and lowercase characters.  It also used CR (13) as line terminator instead of LF (10).  The stdio and conio libaries can perform translations.

The translation mode is selected in conio with the variable "giocharmap" and the function "iocharmap" which will also switch the font.

	iocharmap(IOCHM_PETSCII_2);
	printf("Hello World\n");
	
Will switch to the lowercase PETSCII font and translate the strings while printing.

PETSCII string literals can also be generated using a "p" or "P" prefix such as:

	printf(p"Hello World\n");
	
Screen codes can be generated similar using "s" or "S" prefix.

Input from the console will also be translated accordingly.

The character map for string and char constants can be changed with a pragma to match a custon character set or PETSCII.

    #pragma charmap(char, code [,count])


## Language extensions for optimization

### Additional Optimizer information using __assume()

The compiler can be provided with additional information using the built in function __assume(cond).  This can be useful to mark unreachable code using __assume(false) for e.g. the default of a switch statement.  Another good option is to limit the value range of arguments to allow the compiler using byte operations without the need for integer promotion.

### Marking functions as native

Routines can be marked to be compiled to 6502 machine code with the native pragma:

    void Plot(int x, int y)
    {
        (*Bitmap)[y >> 3][x >> 3][y & 7] |= 0x80 >> (x & 7);
    }

    #pragma native(Plot)

Or alternatively with a __native storage class specifier

    __native void Plot(int x, int y)
    {
        (*Bitmap)[y >> 3][x >> 3][y & 7] |= 0x80 >> (x & 7);
    }

### Linker control

The linker includes only objects that are referenced, starting by the startup code into main() and so on.

If you need to have a function or variable present regardless, you can specify it with the __export storage class specifier or use the #pragma reference(name) pragma.

#### Using libraries

The compiler does a full program compile, the linker step is part of the compilation.  It knows all functions during the compilation run and includes only reachable code in the output.  Source files are added to the build with the help of a pragma:
        
    #pragma compile("stdio.c")

This way you do not need a makefile to build your project.  All header files of the provided libraries add their implementation to the build using this pragma.

#### Placement

The linker uses three levels of objects:

* Region : A physical region of memory or a bank in a cartridge
* Section : A logical region of memory, may span several sections
* Object : Generated code or data, either initialized or empty (e.g. stack or bss)

With the default prg target and no further changes, the compiler creates the following regions and sections:

* "startup" : **0x0801-0x0900** Basic and assembler startup code and interpreter loop
	* "startup"
* "bytecode" : **0x0900-0x0a00** Interpreter jump table if not all native
	* "bytecode"
* "main": **0x0a00-0xa000** Main region for code, data, bss, heap and stack
	* "code"	: Compiled code
	* "data" : Constant data
	* "bss" : Non constant data, initialized to zero on program start
	* "heap" : Memory available for allocation
	* "stack" : Data stack
	
The layout can be changed using #pragma commands.  One may e.g. use all memory up to 0xd000 with the following code:

	#include <c64/memmap.h>

	#pragma region( main, 0x0a00, 0xd000, , , {code, data, bss, heap, stack} )

	int main(void)
	{
		mmap_set(MMAP_NO_BASIC)

Regions can also be used to place assets such as character sets at fixed location in the prg file to avoid copying:

	#pragma region( lower, 0x0a00, 0x2000, , , {code, data} )

	#pragma section( charset, 0)

	#pragma region( charset, 0x2000, 0x2800, , , {charset} )

	#pragma region( main, 0x2800, 0xa000, , , {code, data, bss, heap, stack} )

	#pragma data(charset)

	char charset[2048] = {
		#embed "../resources/charset.bin"
	}

	#pragma data(data)

The #pragma data(), #pragma code() and #pragma bss() control the placement of the generated objects into sections other than the default sections.



### Inline Assembler

Inline assembler can be embedded inside of any functions, regardles of their compilation target of byte code or native.  

#### Accessing variables in assembler

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

The compiler provides two levels of interrupt safe functions.  The specifier __interrupt caues all zero page registers used by the function to be saved, the __hwinterrupt also saves the CPU registers and exits the function with rti


	#include <c64/memmap.h>
	#include <c64/cia.h>
	#include <c64/vic.h>

	__hwinterrupt void irq(void)
	{
		vic.color_border++;

		// some interrupt code

		vic.color_border--;
		vic.intr_ctrl <<= 1;	
	}

	int main(void)
	{
		__asm { sei }   // Disable interrupt
		mmap_set(MMAP_NO_ROM);	// Disable kernal rom
		cia_init();		// No more CIA interrupts
		
		*(void **)0xfffe = irq;     // Install interrupt routine
		vic.intr_enable = VIC_INTR_RST;	// Init raster interrupt
		vic.ctrl1 &= ~VIC_CTRL1_RST8;
		vic.raster = 100;
		
		__asm { cli }   // Re-enable interrupt
		
		for(;;)
		{
			// Non interrupt code
		}

		return 0
	}
	


## Implementation Details



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
* **0x03-0x0c** workspace for mul/div and floating point routines
* **0x0d-0x1a** function arguments for leaf functions
* **0x19-0x1a** instruction pointer
* **0x1b-0x1e** integer and floating point accumulator
* **0x1f-0x22** pointers for indirect addressing
* **0x23-0x24** stack pointer
* **0x25-0x26** frame pointer
* **0x43-0x52** caller saved registers
* **0x53-0x8f** callee saved registers





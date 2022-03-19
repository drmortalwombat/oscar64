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

### Building the samples

The windows installer puts the samples into the users documents folder, using the directory "%userprofile%\documents\oscar64\samples".  A batch file *make.bat* is also placed into this directory which invokes the compiler and builds all samples.  It invokes a second batch file in "%userprofile%\documents\oscar64\bin\oscar64.bat" that calls the compiler.

On a linux installation one can build the samples invoking the *build.sh* shell script in the samples directory.

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
	
## Samples

### Character input and output "stdio"

#### helloworld.c

### Disk file access "kernalio"

The C64 uses various kernal routines to read and write files on disk.  These routines are available with a library <c64/kernalio.h>  All samples in this directory use drive 9, but can easily be changed to drive 8.

#### Reading the directory "diskdir.c"

Reads the directory from the current disk in drive 9 and displays it on screen.


#### Writing characters "charwrite.c"

Opens a ".prg" file on drive 9 for writing and writes 128 characters into the file.

#### Reading characters "charread.c"

Opens a ".prg" file on drive 9 for reading, reads all characters from the file and prints there character code on screen.

#### Writing binary data "filewrite.c"

Opens a ".prg" file on drive 9 for writing and writes an array of structs as binary data into the file.

#### Reading binary data "fileread.c"

Opens a ".prg" file on drive 9 for reading and reads an array of structs as binary data into the file.

#### Writing image data "hireswrite.c"

Renders a hires image into a buffer at 0xe000..0xff40 and saves it to disk.  The added complexity is, that the kernal itself cannot access the memory in this region because it is covered by the kernal ROM itself.  The operation is therefore implemented using a 200 byte RAM buffer.

#### Reading image data "hiresread.c"

Reads a hires image from disk into a buffer at 0xe000..0xff40 and displays it.  The read can be performed without a secondary buffer, because writes to the ROM end up in the RAM underneath.

### Remapping memory "memmap"

The C64 memory map is very flexible.  These samples show how to manipulate the memory map and use easyflash ROMs to execute larger programs.

#### Using more memory "largemem.c"

Moves the BASIC ROM out of the way and allows the use of memory from 0x0800..0xcfff for the compiled C program.

#### Using all memory "allmem.c"

Moves the BASIC ROM, Kernal ROM and the IO area out of the way and allows the use of memory from 0x0800 to 0xfff0 for the compiled C code.  An interrupt trampoline is installed to keep the kernal going.

#### Custom character set "charsetlo.c"

Embedds a custom character set into the prg file at 0x2000..0x27ff and switches the character set base address in the VIC to this address.  The code and data portion of the compiled program is split into two areas to make room for this fixed location data.

#### Himem character set "charsethi.c"

Embedds a custom character set into the prg file at 0xc800..0xcfff and switches the character set base address in the VIC to this address.
  
#### Copy character set "charsetcopy.c"

Embedds a custom character set into the prg file at 0xc000..0xc7ff and copies it to 0xd000 on startup.  This frees this area for stack and heap usage.

#### Easyflash banking "easyflash.c"

When compiling for easyflash, the linker will place the code and data section into bank 0 and copy it to 0x0900..0x7fff at startup.  The remaining banks are free to be used for data or additional codes and can be banked in as needed.  This sample uses banks one to six for additional functions.

#### Easyflash common area "easyflashshared.c"

This sample reserves a fixed area of several banks for a common code segment.  A multiplexer function is placed into this common segment to select a different bank and call a function in that bank.

#### Easyflash relocated area "easyflashreloc.c"

An alternative to calling the function in the cartridge ROM itself is to copy it to a different location in RAM and execute it there.  This sample creates functions in different ROM banks that are linked for a memory area in RAM at 0x7000.  The code is copied from the ROM to RAM and then executed.

#### Terminate stay resident "tsr.c"

A common usage for the RAM area in 0xc000..0xcfff which is visible but not usable from BASIC is to install a terminate stay resident BASIC extension, which can then be called using sys 49152.

It would be a waste of disk space and load time to load this directly into 0xc000 together with a BASIC start code at 0x0800.  This sample uses the linker to link the code for the TSR at 0xc000 but place it into the prg at 0x0900.  The startup then copies this code to 0xc000.

### Hires graphics "hires"

The C64 has a hires graphics mode with 320x200 pixels.  Oscar provides a library <gfx/bitmap.h> for rendering in on screen and off screen bitmaps.

#### Draw lines "lines.c"

Draws and clears lines with various patterns.

#### Draw 3D wireframe "cube3d.c"

Draws a rotating 3D wireframe cube using draw (OR) and clear (AND) operations.  The 3D operations are performed using 12.4 bit fixpoint math.

#### Software bit blit engine "bitblit.c"

![Various bitblit modes](samples/hires/bitblit.png)

Demonstrates the various bit blit modes of the software blit engine, including mask and pattern.

#### Draw polygons "polygon.c"

Draws a series of pattern filled stars with changing orientation and size.

#### Mixed text and hires screen "splitscreen.c"

Uses the <c64/rasterirq.h> library to split the screen into an upper hires and lower text mode screen.  The text area is used to query the user for x, y and radius of circles to draw.

#### 3D Function plotter "func3d.c"

![3D Function plotter](samples/hires/func3d.png)

Draws a 3D function using flat shading and the painters algorithm.  All 3D operations are performed with floating point numbers using the vector library <gfx/vector3d.h>.

### Multicolor bitmaps "hiresmc"

The C64 bitmap graphics mode can also use 2bit per pixel, resulting in an image of 160x200 with four colors.  The oscar library <gfx/bitmapmc.h> implements various drawing operations in this mode.  All x coordinates still range from 0 to 320 to keep a quasi square pixel layout.

#### Draw polygons "polygon.c"

![Colored stars](samples/hiresmc/polygon.png)

Similar to its hires counterpart but using different colors and patterns.

#### Fill similar colored areas "floodfill.c"

Draws filled random circles and fills the space using flood fill.

#### 3D Function plotter "func3d.c"

Similar to its hires counterpart but using four shades of grey.


### Mandelbrot renderer "fractals"

Various versions of the mandelbrot set using float arithmetic.

#### Text mode fractal "mbtext.c"

Simple mandelbrot renderer using text cells and colors to generate a 40x25 pixel image.

#### Hires fractal "mbhires.c"

Hires version using black and white to show the mandelbrot set.

#### Multi color fractal "mbmulti.c"

Multi color version using pure and mixed colors.

#### 3D shaded fractal "mbmulti3d.c"

![Mandelbrot fractal in 3D](samples/fractals/mbmulti3d.png)

Mandelbrot rendered in 3D with shading.  The image is drawn in columns from back to front, using two adjacent columns to calculate slope and brightness.


### Raster beam interrupts "rasterirq"

Interrupts based on the raster beam are an important part of the C64 programmers toolbox.  Switching VIC registers on specific lines opens up many additional features, such a more sprites using multiplexing, combining modes or changing colors or scroll offsets.  The <c64/rasterirq.h> library provides easy access to this feature using on the fly code generation.

#### Static color changes "colorbars.c"

![Color bars](samples/rasterirq/colorbars.png)

Changes the background and border colors several times per frame, creating horizontal color bars.

#### Crawling text at bottom "textcrawler.c"

Draws a scrolling line of text at the bottom of the screen.

#### Crawling text at bottom in IRQ "autocrawler.c"

Draws a scrolling line of text at the bottom of the screen, using an interrupt to update the text.

#### Chasing bars "movingbars.c"

Changing the background and border color at varying vertical positions giving the impression of two chasing colored bars.

#### Freedom for sprites "openborders.c"

Open the vertical screen borders by switching the vertical size bit at the appropriate raster lines.  The opened area is available for sprites only.



### Expand the screen "scrolling"

Scrolling is an important component of many games and editors, it extends the limited real estate of the screen to provide a small view to a larger world or document.  Pixel accurate scrolling is an important feature of the C64's VIC chip.


#### Large scrolling text "bigfont.c"

![Large text](samples/scrolling/bigfont.png)

Expands a text to gigantic size, each pixel covering 2x2 character cells and scrolling from right to left.

#### Fly through tunnel "tunnel.c"

Scroll a dynamic generated tunnel with variable speed.

#### Scroll text and color "colorram.c"

Scrolls the screen text buffer and color buffer horizontally at the same time with one pixel per frame and no double buffering, relying on exact raster timing.


#### X/Y colored tile scrolling "cgrid8way.c"

Expands coloured 4x4 character tiles and scrolls vertically and horizontally at two pixel per frame without double buffering.

#### Free full speed tile expansion "grid2d.c"

Expands a 2D 4x4 tile grid at any scroll speed.  Uses a raster IRQ to limit the scrolled area.


### Moving image blocks "sprites"

Sprites are independed image blocks, such as players, missiles or enemies that can be shown on top of the background.

#### Control a sprite with a joy stick "joycontrol.c"

Combines reading the joystick with the <c64/joystick.h> library and sprite movement with the <c64/sprites.h> library.

#### Use raster IRQ to show 16 sprites "multiplexer.c"

![Large text](samples/sprites/multiplexer.png)

Shows 16 virtual sprites multiplexed from the phyiscal eight sprites with raster interrupts, using the oscar sprite multiplexer library.


#### Fill the screen with sprites "creditroll.c"

![Many sprites](samples/sprites/creditroll.png)

Uses sprite multiplexing and dynamic character to sprite copying to create a vertical scrolling text on top of any image or potentially moving background.

### What you came for "games"

The C64 games define much of the 8 bit area and oscar64 is intended as a of a proof that one can write great 8 bit games using C.


#### Don't bite your end "snake.c"

One of the least complex computer game and more iconic for the smartphone area.  Does not need sprites, scrolling or custom graphics.

#### The eagle has landed "lander.c"

![Landing the eagle](samples/games/lander.png)

Controlling a sprite using floating point physics and checking for collision with the background.

#### No way out "maze3d.c"

![Move through the maze](samples/games/maze3d.png)

A double buffered 3D maze rendering using intermediate positions for forward movement, and simulated rotation using scrolling.

#### This one has balls "breakout.c"

![Breakout](samples/games/breakout.png)

Simplified ball physics using fixed point math. More complex collision checks with the blocks to determine the edge and thus reflection.

#### Too smart for me "connectfour.c"

![Connect Four](samples/games/connectfour.png)

Simple turn based strategy game using tree search with alpha-beta pruning and an opening library to determine the computer moves.

#### Defend your city "missile.c"

![Defend your city](samples/games/missile.png)

Classic computer game using the multicolor hires mode for circles and lines.  The user sprite is controlled in an interrupt routine to cover up the multi frame variable rendering.

#### Fast scroller "hscrollshmup.c"

![Scroll and Shoot](samples/games/hscrollshmup.png)

Fast horizontal scrolling shoot 'em up.  The scroll code expands a 4x4 tile grid of chars in less than a frame time, so the scroll speed is "unlimited".  A parallax level of stars in the background is implemented using an adapted star character.

The shots usd dynamic created characters to overlay on the background.



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





# slinput

## Single line input library

This library provides a horizontally scrollable single line input routine.

## Motivation

I created this so I could use the terminal in Atari ST low resolution mode (40 columns by 25 lines) without causing unnecessary vertical scrolling due to the cursor wrapping to the next line during input.

## Adaptations

The library builds for both Atari ST and Linux. The library itself uses C. The unit tests build optionally and will only work for Linux - these use Google Test and C++.

### Atari ST build systems
* vbcc cross compiler (Linux host) with cmake
* gcc cross compiler (Linux host) with cmake (tested with gcc 4.6.4)
* Pure C native (tested with v1.1)
* Lattice C native (tested with v5.6)

### Linux build system
* g++ with cmake

## Building using cmake on Linux host

### Atari ST vbcc cross compile

A directory exists called **build/tos/vbcc**. Within this directory are toolchain files for vbcc: **vbcc32.cmk** and **vbcc16.cmk**. The 32 version uses 32 bit int, the 16 version 16 bit int.

Change to this directory then invoke configuration with:
**ccmake --toolchain ./vbcc32.cmk ../../..**

The toolchain file expects the VBCC environment variable to be set and the directory containing vc to be in the PATH search path (see VBCC documents on how to setup VBCC).

In the ccmake GUI there will be options to set **CMAKE_BUILD_TYPE** (Release, Debug, ...) and **CMAKE_INSTALL_PREFIX** (installation path for make install). There is also the option **UNITTESTS_ENABLED** which defaults to OFF. Leave this OFF for Atari ST build as the unit tests are not supported.

Note the directive **set(CMAKE_C_COMPILER vc +tos)** in the vbcc32.cmk file. The +tos sets vc to build for tos using 32 bit int. vbcc16.cmk uses +tos16 instead to build with 16 bit ints.

### Atari ST gcc cross compile

A directory exists called **build/tos/gcc**. Within this directory are toolchain files for gcc: **gcc32.cmk** and **gcc16.cmk**. The 32 version uses 32 bit int, the 16 version 16 bit int.
The compiler is set to **m68k-atari-mint-gcc**.

In the ccmake GUI there will be options to set **CMAKE_BUILD_TYPE** (Release, Debug, ...) and **CMAKE_INSTALL_PREFIX** (installation path for make install). There is also the option **UNITTESTS_ENABLED** which defaults to OFF. Leave this OFF for Atari ST build as the unit tests are not supported.

There is also the option for the gcc cross compile which is **LIBCMINI_ENABLED** which defaults to off. When enabled three more settings will appear: **LIBCMINI_INCLUDE_PATH**, **LIBCMINI_LIBRARY_PATH** and **LIBCMINI_STARTUP_PATH** which can be set to the libcmini paths for an already existing libcmini installation.

NOTE: libcmini is only used when linking the example application slinputx.tos described below. When cross compiling with gcc for 16 bit int then it is better to use libcmini, otherwise e.g. isspace gives unexpected results. For this reason SLINPUT_IsSpace_Default in tos.c has been changed to a simple value check.

### Linux native compile

A directory exists called **build/linux**. This just contains a .gitignore file.

Change to this directory then invoke configuration with:
**ccmake ../..**

Again, there will be the **CMAKE_BUILD_TYPE** and **CMAKE_INSTALL_PREFIX** settings. However for Linux the unit tests can be enabled by flipping **UNITTESTS_ENABLED** from OFF to ON.

### Compilation and Installation

After the ccmake configuration step, make followed by make install will build and install the library and header files to the installation path indicated by **CMAKE_INSTALL_PREFIX**.
The **libslinput.a** library will be installed along with the API header **slinput.h**.

If unit tests are enabled, then an executable **slinputt** will also be installed containing the unit tests, along with some other google test headers and libraries.

A simple example application (source code at **src/example/main.c**) will also be compiled and linked. However this is not installed by make install. After building it can be found at **build/tos/vbcc/src/example/slinputx.tos**, **build/tos/gcc/src/example/slinputx.tos** or **build/linux/src/example/slinputx**. If libcmini is enabled for gcc cross compile, slinputx.tos will be linked to use libcmini instead of the default standard library. 

## Atari ST native compile

### Pure C

Within **build/tos/purec** are two Pure C project files, **slinput.prj** and **slinputx.prj**. The former builds the slinput library as **SLINPUT.LIB**, the latter builds the example executable **SLINPUTX.TOS**. As SLINPUTX.TOS is linked with SLINPUT.LIB, the library must be built first.

Within the Pure C user interface, choose menu option Project->Select and choose SLINPUT.PRJ. Then build the library using menu option Project->Make all "SLINPUT.PRJ". Repeat the steps with SLINPUTX.PRJ to build the example application. The library and executable are created in the build/tos/purec directory.

### Lattice C

Within **build/tos/latticec** are two Lattice C project files, **slinput.prj** and **slinputx.prj**. The former builds the slinput library as **SLINPUT.LIB**, the latter buids the example executable **SLINPUTX.TOS**. As SLINPUTX.TOS is linked with SLINPUT.LIB, the library must be built first.

Within the Lattice C user interface, choose menu option Project->Load and choose SLINPUT.PRJ. Then build the library using menu option Project->Make all "SLINPUT". Repeat the steps with SLINPUTX.PRJ to build the example application. The library and executable are created in the build/tos/latticec directory.

## Using the library

Follow these steps to use the library, as shown in **src/example/main.c**:

1) Include the header with **#include "include/slinput.h"**  
2) Call **SLINPUT_CreateState**. This will create a state pointer. The function takes parameters for allocation callbacks, but these can be left at null to use defaults.  
3) Call **SLINPUT_Get** in your input loop. The function takes a parameter for the prompt to display, a parameter for the initial string to place in the buffer (which can be null), and also a buffer in which to store the input text. **SLINPUT_Get** returns an int value. This will be >= 1 if text was input (actually the number of characters in the buffer), 0 if CTRL-D was pressed, or negative if an error occurred. The buffer_chars parameter is the size of the buffer in **sli_char** characters, not the buffer size in bytes.  
4) Optionally, save the input text into history using **SLINPUT_Save**. The next time **SLINPUT_Get** is called it will appear in history (select with cursor up or down and choose with enter).  
5) When finished, call **SLINPUT_DestroyState**.

## Character type and size

The character type used by slinput is a typedef **sli_char**. The typedef declaration is in **slinput.h** and is inferred from system-specific macros. sli_char is one byte in size for the Atari ST. On Linux it is four bytes.

**slinput.h** also contains some defines, **SLI_CHAR_SIZE** and **SLI_CHAR_STRL**. SLI_CHAR_SIZE gives the size of sli_char in bytes as a preprocessor define, which can be useful in client code for conditional compilation (e.g. on the Atari ST which doesn't use multibyte characters). SLI_CHAR_STRL can be used when writing C string literals. It will place L infront of the literal when sli_char is defined to be wchar_t. 

The Linux adaptation uses **mbsrtowcs** and **wcsrtombs** to convert between multibyte and wide characters. It is important therefore to set the locale for **LC_CTYPE** appropriately.

## Issues

Combining diacriticals don't render/work correctly on Linux.

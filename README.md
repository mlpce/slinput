# slinput

## Single line input library

This library provides a horizontally scrollable single line input routine.

## Motivation

I created this so I could use the terminal in Atari ST low resolution mode (40 columns by 25 lines) without causing unnecessary vertical scrolling due to the cursor wrapping to the next line during input.

## Adaptations

The library builds for both Atari ST (using vbcc cross compiler) and Linux (tested with g++) using cmake. The library itself uses C. The unit tests build optionally (configurable via cmake) and will only work for Linux - these use Google Test and C++.

## Building

### Atari ST

A directory exists called **build/tos**. Within this directory is a toolchain file for vbcc: **vbcc_tc.cmk**.

Change to this directory then invoke configuration with:
**ccmake --toolchain ./vbcc_tc.cmk ../..**

The toolchain file expects the VBCC environment variable to be set and the directory containing vc to be in the PATH search path (see VBCC documents on how to setup VBCC).

In the ccmake GUI there will be options to set **CMAKE_BUILD_TYPE** (Release, Debug, ...) and **CMAKE_INSTALL_PREFIX** (installation path for make install). There is also the option **UNITTESTS_ENABLED** which defaults to OFF. Leave this OFF for Atari ST build as the unit tests are not supported.

Note the directive set(CMAKE_C_COMPILER vc +tos) in the toolchain file. This sets vc to build using 32 bit int. Change +tos to +tos16 here to build with 16 bit ints.

### Linux

A directory exists called **build/linux**. This just contains a .gitignore file.

Change to this directory then invoke configuration with:
**ccmake ../..**

Again, there will be the **CMAKE_BUILD_TYPE** and **CMAKE_INSTALL_PREFIX** settings. However for Linux the unit tests can be enabled by flipping **UNITTESTS_ENABLED** from OFF to ON.

### Compilation and Installation

After the ccmake configuration step, make followed by make install will build and install the library and header files to the installation path indicated by **CMAKE_INSTALL_PREFIX**.
The **libslinput.a** library will be installed along with the API header **slinput.h**.

If unit tests are enabled, then an executable **slinputt** will also be installed containing the unit tests, along with some other google test headers and libraries.

A simple example application (source code at **src/example/main.c**) will also be compiled and linked. However this is not installed by make install. After building it can be found at **build/tos/src/example/slinputx.tos** or **build/linux/src/example/slinputx**.

## Using the library

Follow these steps to use the library, as shown in **src/example/main.c**:

1) Include the header with **#include "include/slinput.h"**  
2) Call **SLINPUT_CreateState**. This will create a state pointer. The function takes parameters for allocation callbacks, but these can be left at null to use defaults.  
3) Call **SLINPUT_Get** in your input loop. The function takes a parameter for the prompt to display, a parameter for the initial string to place in the buffer (which can be null), and also a buffer in which to store the input text. **SLINPUT_Get** returns an int value. This will be >= 1 if text was input (actually the number of characters in the buffer), 0 if CTRL-D was pressed, or negative if an error occurred. The buffer_chars parameter is the size of the buffer in **sli_char** characters, not the buffer size in bytes.  
4) Optionally, save the input text into history using **SLINPUT_Save**. The next time **SLINPUT_Get** is called it will appear in history (select with cursor up or down and choose with enter).  
5) When finished, call **SLINPUT_DestroyState**.

## Character type and size

The character type used by slinput is a typedef **sli_char**. The typedef declaration is in **slinput.h** and is inferred from system-specific macros. **sli_char** is one byte in size for the Atari ST. On Linux it is four bytes.

**slinput.h** also contains a define, **SLI_CHAR_SIZE**. This gives the size of **sli_char** in bytes as a preprocessor define, which can be useful in client code for conditional compilation (e.g. on the Atari ST which doesn't use multibyte characters).

The Linux adaptation uses **mbsrtowcs** and **wcsrtombs** to convert between multibyte and wide characters. It is important therefore to set the locale for **LC_CTYPE** appropriately.

## Issues

Combining diacriticals don't render/work correctly on Linux.

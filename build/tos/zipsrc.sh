#!/usr/bin/env bash
set -e

# This script converts the source files to GEMDOS compatible 8.3 filenames.
# It then creates a zip file, adding the files with LF converted to CRLF.

# Make 8.3 directory structure
rm -Rfv 8.3/SLINPUT*
mkdir -p 8.3/SLINPUT/BUILD/TOS/LATTICEC
mkdir -p 8.3/SLINPUT/BUILD/TOS/PUREC
mkdir -p 8.3/SLINPUT/INCLUDE
mkdir -p 8.3/SLINPUT/SRC
mkdir -p 8.3/SLINPUT/SRC/ADAPT
mkdir -p 8.3/SLINPUT/SRC/EXAMPLE

# Copy files as 8.3 filenames
cp -v ../../LICENSE 8.3/SLINPUT
cp -v purec/slinput.prj 8.3/SLINPUT/BUILD/TOS/PUREC/SLINPUT.PRJ
cp -v purec/slinputx.prj 8.3/SLINPUT/BUILD/TOS/PUREC/SLINPUTX.PRJ
cp -v latticec/slinput.prj 8.3/SLINPUT/BUILD/TOS/LATTICEC/SLINPUT.PRJ
cp -v latticec/slinputx.prj 8.3/SLINPUT/BUILD/TOS/LATTICEC/SLINPUTX.PRJ
cp -v ../../include/slinput.h 8.3/SLINPUT/INCLUDE/SLINPUT.H
cp -v ../../src/adapt/tos.c 8.3/SLINPUT/SRC/ADAPT/TOS.C
cp -v ../../src/example/main.c 8.3/SLINPUT/SRC/EXAMPLE/MAIN.C
cp -v ../../src/slinput.c 8.3/SLINPUT/SRC/SLINPUT.C
cp -v ../../src/slinputi.h 8.3/SLINPUT/SRC/SLINPUTI.H

# Zip up files, converting LF to CRLF
pushd 8.3
zip -l -r SLINPUT.ZIP SLINPUT
popd

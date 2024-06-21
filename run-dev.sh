#!/bin/sh

# build and run in debug mode
zig build -Drelease=false || exit 1

if [ -e ./zig-out/bin/digilogic ]; then
  ./zig-out/bin/digilogic
else
  ./zig-out/digilogic.app/Contents/MacOS/digilogic
fi

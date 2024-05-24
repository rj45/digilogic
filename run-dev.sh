#!/bin/sh

# build and run in debug mode on my mac
zig build -Drelease=false && ./zig-out/digilogic.app/Contents/MacOS/digilogic

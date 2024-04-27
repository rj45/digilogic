
HEADERS = src/main.h src/core/core.h src/font.h src/view/view.h src/ux/ux.h src/shaders/msdf_shader.h src/import/import.h $(THIRDPARTY)
SRCS = src/core/circuit.c src/ux/ux.c src/ux/input.c src/ux/undo.c src/ux/autoroute.c src/view/view.c src/import/digital.c
THIRDPARTY = $(wildcard thirdparty/*.h)
THIRDPARTY_LIBS = thirdparty/adaptagrams/libavoid.a
MAIN_SRCS = $(SRCS) src/main.c src/apple.m src/noto_sans_regular.c
TEST_SRCS = $(SRCS) src/test.c src/view/view_test.c src/core/core_test.c

CFLAGS = -I thirdparty -I src -Wall -Werror -DDEBUG -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
LIBFLAGS = -fobjc-arc -framework Metal -framework Cocoa -framework MetalKit -framework Quartz \
	-Lthirdparty/adaptagrams -lavoid -lstdc++

.PHONY: all clean

all: digilogic test gen

test: $(TEST_SRCS) $(HEADERS) $(THIRDPARTY_LIBS) avoid.o
	gcc $(CFLAGS) $(LIBFLAGS) $(TEST_SRCS) avoid.o -o test
	./test

digilogic: $(MAIN_SRCS) $(HEADERS) $(THIRDPARTY_LIBS) avoid.o
	gcc $(CFLAGS) $(LIBFLAGS) $(MAIN_SRCS) -rdynamic avoid.o -o digilogic

gen: src/gen.c thirdparty/cjson.c
	gcc $(CFLAGS) $(LIBFLAGS) src/gen.c thirdparty/cjson.c -o gen

avoid.o: src/avoid/avoid.cpp
	g++ -c $(CFLAGS) -std=c++17 -Ithirdparty/adaptagrams/cola src/avoid/avoid.cpp -o avoid.o

thirdparty/adaptagrams/libavoid.a: thirdparty/adaptagrams/cola/libavoid/.libs/libavoid.a
	ln -sf $(shell pwd)/thirdparty/adaptagrams/cola/libavoid/.libs/libavoid.a thirdparty/adaptagrams/libavoid.a

thirdparty/adaptagrams/cola/libavoid/.libs/libavoid.a:
	cd thirdparty/adaptagrams/cola && ./autogen.sh

src/shaders/msdf_shader.h: src/shaders/msdf.glsl
	cd src/shaders/ && ../../../fips-deploy/sokol-tools/osx-xcode-release/sokol-shdc -i msdf.glsl -o msdf_shader.h --slang glsl330:glsl300es:hlsl4:metal_macos:metal_ios:metal_sim:wgsl --ifdef

clean:
	rm -f digilogic test gen avoid.o

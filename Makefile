
HEADERS = src/core/core.h src/font.h src/view/view.h src/ux/ux.h src/shaders/alphaonly.h src/import/import.h src/autoroute/autoroute.h src/render/fons_sgp.h $(THIRDPARTY)
SRCS = src/core/circuit.c src/ux/ux.c src/ux/input.c src/ux/snap.c src/ux/undo.c src/ux/autoroute.c src/view/view.c src/import/digital.c src/autoroute/autoroute.c src/core/timer.c src/core/smap.c
THIRDPARTY = $(wildcard thirdparty/*.h)
THIRDPARTY_LIBS = thirdparty/routing/target/release/libdigilogic_routing.a
MAIN_SRCS = $(SRCS) src/main.c src/apple.m src/noto_sans_regular.c src/render/fons_sgp.c
TEST_SRCS = $(SRCS) src/test.c src/ux/ux_test.c src/view/view_test.c src/core/core_test.c src/noto_sans_regular.c

CFLAGS = -std=c11 -DSOKOL_METAL -I thirdparty -I src -Wall -Werror \
	-DDEBUG -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
	`pkg-config --cflags freetype2`
LIBFLAGS = -fobjc-arc -framework Metal -framework Cocoa -framework MetalKit -framework Quartz \
	-Lthirdparty/routing/target/release -ldigilogic_routing \
	`pkg-config --libs freetype2`

.PHONY: all clean

all: digilogic test gen

test: $(TEST_SRCS) $(HEADERS) $(THIRDPARTY_LIBS)
	gcc $(CFLAGS) $(LIBFLAGS) $(TEST_SRCS)  -o test
	./test

digilogic: $(MAIN_SRCS) $(HEADERS) $(THIRDPARTY_LIBS)
	gcc $(CFLAGS) $(LIBFLAGS) $(MAIN_SRCS) -rdynamic  -o digilogic

gen: src/gen.c thirdparty/cjson.c
	gcc $(CFLAGS) $(LIBFLAGS) src/gen.c thirdparty/cjson.c -o gen

thirdparty/routing/target/release/libdigilogic_routing.a:
	cd thirdparty/routing && cargo build --release

src/shaders/alphaonly.h: src/shaders/alphaonly.glsl
	cd src/shaders/ && ../../../fips-deploy/sokol-tools/osx-xcode-release/sokol-shdc -i alphaonly.glsl -o alphaonly.h --slang glsl330:glsl300es:hlsl4:metal_macos:metal_ios:metal_sim:wgsl --ifdef

src/apple.m: src/nonapple.c
	cp src/nonapple.c src/apple.m

clean:
	rm -f digilogic test gen src/apple.m

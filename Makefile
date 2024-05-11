
HEADERS = src/core/core.h src/assets.h src/view/view.h src/ux/ux.h src/shaders/alphaonly.h src/import/import.h src/autoroute/autoroute.h src/render/fons_sgp.h src/render/polyline.h $(THIRDPARTY)
SRCS = src/core/circuit.c src/ux/ux.c src/ux/input.c src/ux/snap.c src/ux/undo.c src/ux/autoroute.c src/view/view.c src/import/digital.c src/autoroute/autoroute.c src/core/smap.c
THIRDPARTY = $(wildcard thirdparty/*.h)
THIRDPARTY_LIBS = thirdparty/routing/target/release/libdigilogic_routing.a
MAIN_SRCS = $(SRCS) src/main.c src/apple.m src/assets.c src/render/fons_sgp.c src/render/polyline.c src/render/draw.c
TEST_SRCS = $(SRCS) src/test.c src/ux/ux_test.c src/view/view_test.c src/core/core_test.c src/render/draw_test.c

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

digilogic: $(MAIN_SRCS) $(HEADERS) $(THIRDPARTY_LIBS) $(THIRDPARTY_SRCS)
	gcc $(CFLAGS) $(LIBFLAGS) $(MAIN_SRCS) $(THIRDPARTY_SRCS) -rdynamic  -o digilogic

gen: src/gen.c thirdparty/cjson.c
	gcc $(CFLAGS) $(LIBFLAGS) src/gen.c thirdparty/cjson.c -o gen

thirdparty/routing/target/release/libdigilogic_routing.a:
	cd thirdparty/routing && cargo build --release

src/shaders/alphaonly.h: src/shaders/alphaonly.glsl
	cd src/shaders/ && ../../../fips-deploy/sokol-tools/osx-xcode-release/sokol-shdc -i alphaonly.glsl -o alphaonly.h --slang glsl330:glsl300es:hlsl4:metal_macos:metal_ios:metal_sim:wgsl --ifdef

thirdparty/nanovg/shd.glsl.h: thirdparty/nanovg/shd.glsl thirdparty/nanovg/internal_shd.vs.glsl thirdparty/nanovg/internal_shd.fs.glsl
	cd thirdparty/nanovg/ && ../../../fips-deploy/sokol-tools/osx-xcode-release/sokol-shdc -i shd.glsl -o shd.glsl.h --slang glsl330:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl --defines=USE_SOKOL --ifdef

thirdparty/nanovg/shd.aa.glsl.h: thirdparty/nanovg/shd.aa.glsl thirdparty/nanovg/internal_shd.vs.glsl thirdparty/nanovg/internal_shd.fs.glsl
	cd thirdparty/nanovg/ && ../../../fips-deploy/sokol-tools/osx-xcode-release/sokol-shdc -i shd.aa.glsl -o shd.aa.glsl.h --slang glsl330:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl --defines=USE_SOKOL:EDGE_AA --ifdef

src/apple.m: src/nonapple.c
	cp src/nonapple.c src/apple.m

res/assets.zip: res/assets/*
	cd res && zip -r -9 assets.zip assets

src/assets.c: res/assets.zip gen
	./gen > src/assets.c

clean:
	rm -f digilogic test gen src/apple.m

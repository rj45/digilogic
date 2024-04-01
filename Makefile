test: src/main.c src/node_editor.c thirdparty/*
	gcc -I thirdparty -I src -Wall -Werror -o test thirdparty/sokol_gfx.m src/main.c -fobjc-arc -framework Metal -framework Cocoa -framework MetalKit -framework Quartz
	./test
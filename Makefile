
HEADERS = src/main.h src/core/core.h $(THIRDPARTY)
SRCS = src/core/circuit.c
THIRDPARTY = $(wildcard thirdparty/*.h)
MAIN_SRCS = $(SRCS) src/main.c src/view/view.c src/apple.m
TEST_SRCS = $(SRCS) src/test.c src/view/view_test.c src/core/core_test.c

CFLAGS = -I thirdparty -I src -Wall -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
LIBFLAGS = -fobjc-arc -framework Metal -framework Cocoa -framework MetalKit -framework Quartz

.PHONY: all clean

all: digilogic test

test: $(TEST_SRCS) $(HEADERS)
	gcc $(CFLAGS) $(LIBFLAGS) $(TEST_SRCS) -o test
	./test

digilogic: $(MAIN_SRCS) $(HEADERS)
	gcc $(CFLAGS) $(LIBFLAGS) $(MAIN_SRCS) -o digilogic

clean:
	rm -f digilogic test

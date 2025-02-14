INC = $(wildcard src/*.h) $(wildcard include/*/*.h)
SRC = $(wildcard src/*.c)
CFLAGS = -Wall -g

fonter: ${SRC} ${INC}
	cc ${CFLAGS} ${SRC} -o fonter -lglfw -lGL -Iinclude

run: fonter
	./fonter

tags: ${SRC} ${INC}
	ctags $^

.PHONY: run

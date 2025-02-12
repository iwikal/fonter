INC = $(wildcard src/*.h) $(wildcard include/*/*.h)
SRC = $(wildcard src/*.c)

fonter: ${SRC} ${INC}
	cc -g ${SRC} -o fonter -lglfw -lGL -Iinclude

run: fonter
	./fonter

tags: ${SRC} ${INC}
	ctags $^

.PHONY: run

fonter: $(wildcard src/*.c) $(wildcard include/*.h)
	cc -g $^ -o fonter -lglfw -lGL -Iinclude

run: fonter
	./fonter

.PHONY: run

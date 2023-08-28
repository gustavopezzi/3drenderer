build:
	gcc -Wall -Wfatal-errors -std=c99 ./src/*.c `sdl2-config --libs --cflags` -lm -o renderer

run:
	./renderer

clean:
	rm renderer

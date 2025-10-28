all : main.c image_loader.c window_manager.c
	gcc -o main main.c image_loader.c window_manager.c -lSDL2 -lm -Wall -Wextra
	./main

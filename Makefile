all : main.c image_loader.c window_manager.c cmd_window.c
	gcc -o main main.c image_loader.c window_manager.c cmd_window.c `sdl2-config --cflags --libs` -lSDL2_ttf -lm -Wall -Wextra
	./main

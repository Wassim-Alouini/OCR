all : reader.c main.c
	gcc reader.c main.c -Wall -Wextra -Werror -fsanitize=address -o reader
	./reader

clean :
	rm reader

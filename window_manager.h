#include <SDL2/SDL.h>
#include <err.h>

void terminate_texture(SDL_Texture *texture);
void terminate_renderer(SDL_Renderer *renderer);
void terminate_window(SDL_Window *window);
void initialize_renderer(SDL_Renderer **renderer, SDL_Window *window);
void initialize_window_texture(SDL_Texture **texture, SDL_Renderer *renderer, int w, int h);
void initialize_window(SDL_Window **window);

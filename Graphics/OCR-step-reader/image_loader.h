#include <SDL2/SDL.h>
#include <err.h>

void load_image(SDL_Surface** surface_out, const char* file);
void create_texture_from_surface(SDL_Texture** texture_out, SDL_Renderer* renderer, SDL_Surface* surface);
void render_texture_rotated(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Texture* master_texture,int maxw, int maxh, int srcw, int srch, const double angle);


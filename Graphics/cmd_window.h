#include <SDL2/SDL.h>


int cmdwindow_init(void);
void cmdwindow_quit(void);
int cmdwindow_handle_event(SDL_Event *e, char **command);


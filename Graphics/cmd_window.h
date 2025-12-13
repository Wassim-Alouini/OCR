#ifndef CMD_WINDOW_H
#define CMD_WINDOW_H

#include <SDL2/SDL.h>

#include "../NeuralNetwork/neuralnetwork.h"   // <-- for LogFn



#ifndef CMD_MAX_LEN
#define CMD_MAX_LEN 256
#endif


typedef struct {
    int value;                 // same as old returned int
    char operation[100];       // first token
    char raw[CMD_MAX_LEN];     // full command line typed
} CmdResult;

CmdResult cmdwindow_handle_event(SDL_Event *e);
int cmdwindow_init(void);
void cmdwindow_quit(void);

LogFn cmdwindow_get_logger(void);
void *cmdwindow_get_logger_userdata(void);


#endif
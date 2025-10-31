#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cmd_window.h"
#define CMD_MAX_LEN 256

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Color textColor;
    char input[CMD_MAX_LEN];
    int cursor;
    int isActive;
} cmdwindow;

static cmdwindow gcmd;

#define CMD_WINDOW_TITLE "Command Input"
#define CMD_WINDOW_WIDTH 600
#define CMD_WINDOW_HEIGHT 60
#define CMD_FONT_PATH "Inconsolata-Regular.ttf"
#define CMD_FONT_SIZE 18

static void cmdwindow_rendertext(void) {
    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    if (strlen(gcmd.input) > 0) {
        SDL_Surface *surf = TTF_RenderText_Blended(gcmd.font, gcmd.input, gcmd.textColor);
        SDL_Texture *tex = SDL_CreateTextureFromSurface(gcmd.renderer, surf);

        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
        SDL_Rect dst = {10, (CMD_WINDOW_HEIGHT - th) / 2, tw, th};
        SDL_RenderCopy(gcmd.renderer, tex, NULL, &dst);

        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }

    SDL_RenderPresent(gcmd.renderer);
}


int cmdwindow_init(void) {
    memset(&gcmd, 0, sizeof(gcmd));

    gcmd.window = SDL_CreateWindow(
        CMD_WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        CMD_WINDOW_WIDTH, CMD_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!gcmd.window) {
        fprintf(stderr, "CmdWindow_Init: failed to create window: %s\n", SDL_GetError());
        return 0;
    }

    gcmd.renderer = SDL_CreateRenderer(gcmd.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gcmd.renderer) {
        fprintf(stderr, "CmdWindow_Init: failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(gcmd.window);
        return 0;
    }

    gcmd.font = TTF_OpenFont(CMD_FONT_PATH, CMD_FONT_SIZE);
    if (!gcmd.font) {
        fprintf(stderr, "CmdWindow_Init: failed to open font '%s': %s\n", CMD_FONT_PATH, TTF_GetError());
        SDL_DestroyRenderer(gcmd.renderer);
        SDL_DestroyWindow(gcmd.window);
        return 0;
    }

    gcmd.textColor = (SDL_Color){255, 255, 255, 255};
    gcmd.cursor = 0;
    gcmd.input[0] = '\0';
    gcmd.isActive = 1;

    SDL_StartTextInput();
    cmdwindow_rendertext();
    return 1;
}

void cmdwindow_quit(void) {
    if (gcmd.font) TTF_CloseFont(gcmd.font);
    if (gcmd.renderer) SDL_DestroyRenderer(gcmd.renderer);
    if (gcmd.window) SDL_DestroyWindow(gcmd.window);
    SDL_StopTextInput();
}

int cmdwindow_handle_event(SDL_Event *e, char** operation) {
    if (!gcmd.isActive) return 0;
    if (e->type == SDL_TEXTINPUT) {
        if (strlen(gcmd.input) + strlen(e->text.text) < CMD_MAX_LEN - 1) {
            strcat(gcmd.input, e->text.text);
            cmdwindow_rendertext();
        }
    } else if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKSPACE) {
            size_t len = strlen(gcmd.input);
            if (len > 0) {
                gcmd.input[len - 1] = '\0';
                cmdwindow_rendertext();
            }
        } else if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) {
            int code = 0;
            
	    char word1[100];
	    char word2[100] = "0";
	    sscanf(gcmd.input, "%99s %99s", word1, word2);

	    *operation = word1;
	    code = atoi(word2);

            printf("[CMD] Entered: '%s' -> %d\n", *operation, code);

            gcmd.input[0] = '\0';
            cmdwindow_rendertext();
            return code;
        }
    }
    return 0;
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_window.h"

#define CMD_WINDOW_TITLE "Command Input"
#define CMD_WINDOW_WIDTH 600
#define CMD_WINDOW_HEIGHT 60
#define CMD_FONT_PATH "Inconsolata-Regular.ttf"
#define CMD_FONT_SIZE 18

typedef struct
{
    const char* name;
    const char* desc;
} HelpItem;

// Items to display in the learn menu
static HelpItem HELP_ITEMS[] = {
    {"load", "load <file>\nLoads an image into master_surface.\nExample: load "
             "input.png"},
    {"rotate", "rotate <deg>\nRotates the current image by <deg> "
               "degrees.\nExample: rotate 90"},
    {"autorotate",
     "autorotate\nEstimates and applies the rotation angle automatically."},
    {"grayscale", "grayscale\nConverts the image to grayscale."},
    {"binarize", "binarize\nConverts the image to black/white using adaptive "
                 "thresholding."},
    {"median", "median\nApplies a 3x3 median filter (denoise)."},
    {"blur", "blur\nApplies a small gaussian blur."},
    {"close", "close\nMorphological close (repairs broken strokes)."},
    {"repair", "repair\nHeuristic stroke repair."},
    {"boxes", "boxes\nDetects letter boxes and draws grid/word separation."},
    {"auto", "auto\nRuns the full pipeline: preprocess, rotate, binarize, "
             "boxes, export."},
    {"help", "help / man\nShows this help menu."},
    {"learn",
     "learn\nOpens the model training UI, must provide <dataset>, "
     "<output_file>, <epochs>, <learning_rate>, <number_of_iterations>."},
};

static const int HELP_COUNT = (int)(sizeof(HELP_ITEMS) / sizeof(HELP_ITEMS[0]));

// cmdwindow modes
typedef enum
{
    CMD_MODE_INPUT = 0,
    CMD_MODE_HELP_LIST = 1,
    CMD_MODE_HELP_DESC = 2,
    CMD_MODE_LEARN_FORM = 3,
    CMD_MODE_LEARN_LOG = 4
} CmdMode;

// cmdwindow necessary data
typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Color textColor;

    char input[CMD_MAX_LEN];
    int cursor;
    int isActive;

    CmdMode mode;
    int helpIndex;
    char learn_dataset[256];
    char learn_out[256];
    char learn_epochs[32];
    char learn_lr[32];
    char learn_hidden[32];
    int learn_field;

    char* logbuf;
    size_t loglen;
    size_t logcap;

    int log_scroll;
    int log_line_height;
} cmdwindow;

static cmdwindow gcmd;
static void cmdwindow_render_learn_log(void);

// log training info in learning mode
static void cmdwindow_logger(void* userdata, const char* msg)
{
    (void)userdata;

    if (!gcmd.logbuf)
        return;

    size_t len = strlen(msg);
    if (gcmd.loglen + len + 1 >= gcmd.logcap)
    {
        gcmd.loglen = 0;
    }

    memcpy(gcmd.logbuf + gcmd.loglen, msg, len);
    gcmd.loglen += len;
    gcmd.logbuf[gcmd.loglen] = '\0';

    if (gcmd.mode == CMD_MODE_LEARN_LOG)
    {
        cmdwindow_render_learn_log();
    }
}

// render a text line
static void render_line(const char* text, int x, int y)
{
    if (!text || !*text)
        return;

    SDL_Surface* surf = TTF_RenderText_Blended(gcmd.font, text, gcmd.textColor);
    if (!surf)
        return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(gcmd.renderer, surf);
    if (!tex)
    {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(gcmd.renderer, tex, NULL, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

// render a wrapped text line
static void render_wrapped(const char* text, int x, int y, int wrapWidth,
                           int* outHeight)
{
    if (outHeight)
        *outHeight = 0;
    if (!text || !*text)
        return;
    SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(
        gcmd.font, text, gcmd.textColor, (Uint32)wrapWidth);
    if (!surf)
        return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(gcmd.renderer, surf);
    if (!tex)
    {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(gcmd.renderer, tex, NULL, &dst);

    if (outHeight)
        *outHeight = surf->h;

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

// -------------------- normal input render --------------------
static void cmdwindow_rendertext(void)
{
    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    if (strlen(gcmd.input) > 0)
    {
        SDL_Surface* surf =
            TTF_RenderText_Blended(gcmd.font, gcmd.input, gcmd.textColor);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(gcmd.renderer, surf);

        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
        SDL_Rect dst = {10, (CMD_WINDOW_HEIGHT - th) / 2, tw, th};
        SDL_RenderCopy(gcmd.renderer, tex, NULL, &dst);

        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }

    SDL_RenderPresent(gcmd.renderer);
}

// transform cmdwindow to learning mode
static void cmdwindow_render_learn_form(void)
{
    const int W = 900;
    const int H = 360;
    SDL_SetWindowSize(gcmd.window, W, H);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    render_line("LEARN (Tab/Up/Down = change field, type = edit, Enter = run, "
                "Esc = cancel)",
                10, 10);

    const char* labels[5] = {"Dataset file", "Output model (.txt)", "Epochs",
                             "Learning rate", "Hidden size"};

    const char* vals[5] = {gcmd.learn_dataset, gcmd.learn_out,
                           gcmd.learn_epochs, gcmd.learn_lr, gcmd.learn_hidden};

    int y = 60;
    for (int i = 0; i < 5; i++)
    {
        char line[1024];
        snprintf(line, sizeof(line), "%c %s: %s",
                 (i == gcmd.learn_field ? '>' : ' '), labels[i],
                 (vals[i][0] ? vals[i] : "(empty)"));
        render_line(line, 20, y);
        y += 45;
    }

    SDL_RenderPresent(gcmd.renderer);
}

// init and call learn form
static void cmdwindow_enter_learn(void)
{
    gcmd.mode = CMD_MODE_LEARN_FORM;
    gcmd.learn_field = 0;
    cmdwindow_render_learn_form();
}

// exit learn mode, switch to input mode
static void cmdwindow_exit_learn_to_input(void)
{
    gcmd.mode = CMD_MODE_INPUT;

    SDL_SetWindowSize(gcmd.window, CMD_WINDOW_WIDTH, CMD_WINDOW_HEIGHT);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    gcmd.input[0] = '\0';
    cmdwindow_rendertext();
}

// -------------------- HELP renders --------------------
static void cmdwindow_render_help_list(void)
{
    const int W = 450;
    const int H = 500;
    SDL_SetWindowSize(gcmd.window, W, H);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    render_line("HELP (Up/Down, Enter=details, Esc=exit)", 10, 10);

    int y = 40;
    for (int i = 0; i < HELP_COUNT; i++)
    {
        char line[160];
        snprintf(line, sizeof(line), "%c %s", (i == gcmd.helpIndex ? '>' : ' '),
                 HELP_ITEMS[i].name);
        render_line(line, 20, y);
        y += 22;
    }

    SDL_RenderPresent(gcmd.renderer);
}

// display description of selected command
static void cmdwindow_render_help_desc(void)
{
    const int W = 900;
    const int H = 500;
    SDL_SetWindowSize(gcmd.window, W, H);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    const HelpItem* it = &HELP_ITEMS[gcmd.helpIndex];

    char title[160];
    snprintf(title, sizeof(title), "[ %s ]", it->name);
    render_line(title, 10, 10);

    int descH = 0;
    render_wrapped(it->desc, 10, 50, W - 20, &descH);

    render_line("Esc = back   Enter = back", 10, H - 30);

    SDL_RenderPresent(gcmd.renderer);
}
// -------------------- help mode transitions --------------------
static void cmdwindow_enter_help(void)
{
    gcmd.mode = CMD_MODE_HELP_LIST;
    gcmd.helpIndex = 0;
    gcmd.learn_dataset[0] = '\0';
    gcmd.learn_out[0] = '\0';
    snprintf(gcmd.learn_epochs, sizeof(gcmd.learn_epochs), "10");
    snprintf(gcmd.learn_lr, sizeof(gcmd.learn_lr), "0.01");
    snprintf(gcmd.learn_hidden, sizeof(gcmd.learn_hidden), "64");
    gcmd.learn_field = 0;
    cmdwindow_render_help_list();
}

// exit help mode and go back to input mode
static void cmdwindow_exit_help_to_input(void)
{
    gcmd.mode = CMD_MODE_INPUT;
    gcmd.input[0] = '\0';
    SDL_SetWindowSize(gcmd.window, CMD_WINDOW_WIDTH, CMD_WINDOW_HEIGHT);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    cmdwindow_rendertext();
}

// call render help desc
static void cmdwindow_open_help_desc(void)
{
    gcmd.mode = CMD_MODE_HELP_DESC;
    cmdwindow_render_help_desc();
}

// display man list
static void cmdwindow_back_to_help_list(void)
{
    gcmd.mode = CMD_MODE_HELP_LIST;
    cmdwindow_render_help_list();
}

// -------------------- public API --------------------
int cmdwindow_init(void)
{
    memset(&gcmd, 0, sizeof(gcmd));

    gcmd.logcap = 64 * 1024;
    gcmd.logbuf = malloc(gcmd.logcap);
    gcmd.loglen = 0;
    if (gcmd.logbuf)
        gcmd.logbuf[0] = '\0';

    gcmd.window = SDL_CreateWindow(CMD_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, CMD_WINDOW_WIDTH,
                                   CMD_WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!gcmd.window)
    {
        fprintf(stderr, "CmdWindow_Init: failed to create window: %s\n",
                SDL_GetError());
        return 0;
    }

    gcmd.renderer = SDL_CreateRenderer(
        gcmd.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gcmd.renderer)
    {
        fprintf(stderr, "CmdWindow_Init: failed to create renderer: %s\n",
                SDL_GetError());
        SDL_DestroyWindow(gcmd.window);
        return 0;
    }

    gcmd.font = TTF_OpenFont(CMD_FONT_PATH, CMD_FONT_SIZE);
    if (!gcmd.font)
    {
        fprintf(stderr, "CmdWindow_Init: failed to open font '%s': %s\n",
                CMD_FONT_PATH, TTF_GetError());
        SDL_DestroyRenderer(gcmd.renderer);
        SDL_DestroyWindow(gcmd.window);
        return 0;
    }
    gcmd.log_scroll = 0;
    gcmd.log_line_height = TTF_FontHeight(gcmd.font);

    gcmd.textColor = (SDL_Color){255, 255, 255, 255};
    gcmd.cursor = 0;
    gcmd.input[0] = '\0';
    gcmd.isActive = 1;

    gcmd.mode = CMD_MODE_INPUT;
    gcmd.helpIndex = 0;

    SDL_StartTextInput();
    cmdwindow_rendertext();
    return 1;
}

void cmdwindow_quit(void)
{
    if (gcmd.font)
        TTF_CloseFont(gcmd.font);
    if (gcmd.renderer)
        SDL_DestroyRenderer(gcmd.renderer);
    if (gcmd.window)
        SDL_DestroyWindow(gcmd.window);
    if (gcmd.logbuf)
        free(gcmd.logbuf);
    SDL_StopTextInput();
}

CmdResult cmdwindow_handle_event(SDL_Event* e)
{
    CmdResult res;
    res.value = 0;
    res.operation[0] = '\0';
    res.raw[0] = '\0';

    if (!gcmd.isActive)
        return res;

    if (gcmd.mode == CMD_MODE_LEARN_LOG)
    {
        if (e->type == SDL_KEYDOWN)
        {
            SDL_Keycode k = e->key.keysym.sym;

            if (k == SDLK_ESCAPE)
            {
                cmdwindow_exit_learn_to_input();
                return res;
            }
            int step =
                (gcmd.log_line_height > 0 ? gcmd.log_line_height : 18) * 3;

            if (k == SDLK_UP)
            {
                gcmd.log_scroll -= step;
                if (gcmd.log_scroll < 0)
                    gcmd.log_scroll = 0;
                cmdwindow_render_learn_log();
                return res;
            }
            if (k == SDLK_DOWN)
            {
                gcmd.log_scroll += step;
                cmdwindow_render_learn_log();
                return res;
            }
            if (k == SDLK_PAGEUP)
            {
                gcmd.log_scroll -= step * 10;
                if (gcmd.log_scroll < 0)
                    gcmd.log_scroll = 0;
                cmdwindow_render_learn_log();
                return res;
            }
            if (k == SDLK_PAGEDOWN)
            {
                gcmd.log_scroll += step * 10;
                cmdwindow_render_learn_log();
                return res;
            }
            if (k == SDLK_HOME)
            {
                gcmd.log_scroll = 0;
                cmdwindow_render_learn_log();
                return res;
            }
            if (k == SDLK_END)
            {
                gcmd.log_scroll = 1 << 30;
                cmdwindow_render_learn_log();
                return res;
            }
        }

        return res;
    }
    if (gcmd.mode == CMD_MODE_HELP_LIST || gcmd.mode == CMD_MODE_HELP_DESC)
    {
        if (e->type == SDL_KEYDOWN)
        {
            SDL_Keycode k = e->key.keysym.sym;

            if (k == SDLK_ESCAPE)
            {
                if (gcmd.mode == CMD_MODE_HELP_DESC)
                    cmdwindow_back_to_help_list();
                else
                    cmdwindow_exit_help_to_input();
                return res;
            }

            if (gcmd.mode == CMD_MODE_HELP_LIST)
            {
                if (k == SDLK_UP)
                {
                    gcmd.helpIndex =
                        (gcmd.helpIndex - 1 + HELP_COUNT) % HELP_COUNT;
                    cmdwindow_render_help_list();
                    return res;
                }
                if (k == SDLK_DOWN)
                {
                    gcmd.helpIndex = (gcmd.helpIndex + 1) % HELP_COUNT;
                    cmdwindow_render_help_list();
                    return res;
                }
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                {
                    cmdwindow_open_help_desc();
                    return res;
                }
            }
            else
            {
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                {
                    cmdwindow_back_to_help_list();
                    return res;
                }
            }
        }
        return res;
    }
    if (gcmd.mode == CMD_MODE_LEARN_FORM)
    {

        char* field = (gcmd.learn_field == 0)   ? gcmd.learn_dataset
                      : (gcmd.learn_field == 1) ? gcmd.learn_out
                      : (gcmd.learn_field == 2) ? gcmd.learn_epochs
                      : (gcmd.learn_field == 3) ? gcmd.learn_lr
                                                : gcmd.learn_hidden;

        size_t fieldCap = (gcmd.learn_field <= 1)   ? 255
                          : (gcmd.learn_field == 2) ? 31
                          : (gcmd.learn_field == 3) ? 31
                                                    : 31;

        if (e->type == SDL_KEYDOWN)
        {
            SDL_Keycode k = e->key.keysym.sym;

            if (k == SDLK_ESCAPE)
            {
                cmdwindow_exit_learn_to_input();
                return res;
            }

            if (k == SDLK_TAB || k == SDLK_DOWN)
            {
                gcmd.learn_field = (gcmd.learn_field + 1) % 5;
                cmdwindow_render_learn_form();
                return res;
            }

            if (k == SDLK_UP)
            {
                gcmd.learn_field = (gcmd.learn_field + 4) % 5;
                cmdwindow_render_learn_form();
                return res;
            }

            if (k == SDLK_BACKSPACE)
            {
                size_t len = strlen(field);
                if (len > 0)
                    field[len - 1] = '\0';
                cmdwindow_render_learn_form();
                return res;
            }

            if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
            {
                if (gcmd.learn_dataset[0] == '\0' || gcmd.learn_out[0] == '\0')
                {
                    printf(
                        "learn: please fill Dataset file and Output model\n");
                    return res;
                }
                snprintf(res.operation, sizeof(res.operation), "learn");
                snprintf(res.raw, sizeof(res.raw), "learn %s %s %s %s %s",
                         gcmd.learn_dataset, gcmd.learn_out, gcmd.learn_epochs,
                         gcmd.learn_lr, gcmd.learn_hidden);
                gcmd.mode = CMD_MODE_LEARN_LOG;
                gcmd.log_scroll = 0;
                if (gcmd.logbuf)
                {
                    gcmd.loglen = 0;
                    gcmd.logbuf[0] = '\0';
                }
                cmdwindow_render_learn_log();
                return res;
            }
        }

        if (e->type == SDL_TEXTINPUT)
        {
            if (strlen(field) + strlen(e->text.text) < fieldCap)
            {
                strcat(field, e->text.text);
                cmdwindow_render_learn_form();
            }
            return res;
        }

        return res;
    }

    if (e->type == SDL_TEXTINPUT)
    {
        if (strlen(gcmd.input) + strlen(e->text.text) < CMD_MAX_LEN - 1)
        {
            strcat(gcmd.input, e->text.text);
            cmdwindow_rendertext();
        }
    }
    else if (e->type == SDL_KEYDOWN)
    {
        if (e->key.keysym.sym == SDLK_BACKSPACE)
        {
            size_t len = strlen(gcmd.input);
            if (len > 0)
            {
                gcmd.input[len - 1] = '\0';
                cmdwindow_rendertext();
            }
        }
        else if (e->key.keysym.sym == SDLK_RETURN ||
                 e->key.keysym.sym == SDLK_KP_ENTER)
        {

            strncpy(res.raw, gcmd.input, CMD_MAX_LEN - 1);
            res.raw[CMD_MAX_LEN - 1] = '\0';

            char word1[100] = {0};
            char word2[100] = {0};
            sscanf(gcmd.input, "%99s %99s", word1, word2);

            strncpy(res.operation, word1, sizeof(res.operation) - 1);
            res.operation[sizeof(res.operation) - 1] = '\0';

            res.value = atoi(word2);

            if (strcmp(res.operation, "help") == 0 ||
                strcmp(res.operation, "man") == 0)
            {
                gcmd.input[0] = '\0';
                cmdwindow_enter_help();
                return (CmdResult){0};
            }

            if (strcmp(res.operation, "learn") == 0)
            {
                gcmd.input[0] = '\0';
                gcmd.learn_dataset[0] = '\0';
                gcmd.learn_out[0] = '\0';
                snprintf(gcmd.learn_epochs, sizeof(gcmd.learn_epochs), "10");
                snprintf(gcmd.learn_lr, sizeof(gcmd.learn_lr), "0.01");
                snprintf(gcmd.learn_hidden, sizeof(gcmd.learn_hidden), "64");
                gcmd.learn_field = 0;

                cmdwindow_enter_learn();
                return (CmdResult){0};
            }

            printf("[CMD] Entered: '%s' -> %d\n", res.raw, res.value);

            gcmd.input[0] = '\0';
            cmdwindow_rendertext();
            return res;
        }
    }

    return res;
}

// log into learning mode (training steps)
static void cmdwindow_render_learn_log(void)
{
    const int W = 900;
    const int H = 600;
    SDL_SetWindowSize(gcmd.window, W, H);
    SDL_SetWindowPosition(gcmd.window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    SDL_SetRenderDrawColor(gcmd.renderer, 20, 20, 20, 255);
    SDL_RenderClear(gcmd.renderer);

    render_line("TRAINING LOG (Esc = back, Up/Down/PgUp/PgDn scroll)", 10, 10);
    const int viewX = 10;
    const int viewY = 45;
    const int viewW = W - 20;
    const int viewH = H - 55;

    const char* txt =
        (gcmd.logbuf && gcmd.logbuf[0]) ? gcmd.logbuf : "(no output yet)";
    SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(
        gcmd.font, txt, gcmd.textColor, (Uint32)viewW);
    if (!surf)
    {
        SDL_RenderPresent(gcmd.renderer);
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(gcmd.renderer, surf);
    if (!tex)
    {
        SDL_FreeSurface(surf);
        SDL_RenderPresent(gcmd.renderer);
        return;
    }

    int texW = surf->w;
    int texH = surf->h;
    int maxScroll = texH - viewH;
    if (maxScroll < 0)
        maxScroll = 0;

    if (gcmd.log_scroll < 0)
        gcmd.log_scroll = 0;
    if (gcmd.log_scroll > maxScroll)
        gcmd.log_scroll = maxScroll;

    SDL_Rect src = {0, gcmd.log_scroll, texW, viewH};
    if (src.h > texH - src.y)
        src.h = texH - src.y;

    SDL_Rect dst = {viewX, viewY, texW, src.h};
    SDL_RenderCopy(gcmd.renderer, tex, &src, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);

    SDL_RenderPresent(gcmd.renderer);
}

LogFn cmdwindow_get_logger(void) { return cmdwindow_logger; }
void* cmdwindow_get_logger_userdata(void) { return NULL; }

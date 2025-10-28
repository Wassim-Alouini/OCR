#include <stdio.h>
#include "image_loader.h"
#include "window_manager.h"
#include <SDL2/SDL.h>
#include <math.h>

//----PROGRAM STEPS----//
//Load image
//UI displays image
//Edit image
////Grayscale
////Rotation

int main() 
{
    const char* input = "input.png";
    const char* output = "output.bmp";

    char command[256];
    snprintf(command, sizeof(command), "magick \"%s\" \"%s\"", input, output);

    int res = system(command);

    if(res)
    {
	errx(EXIT_FAILURE, "Couldn't convert image file");
    }


    SDL_Window *window = NULL;
    initialize_window(&window);

    //init image "input.bmp" as surface DONE
    //init renderer DONE
    //resize window (trig calculate maximum bounding box after rotation) DONE
    //create texture (of same new window size) DONE
    //create texture from surface  DONE
    //copy surfacetexture to window texture DONE

    SDL_Surface *master_surface = NULL;
    load_image(&master_surface, "output.bmp");

    SDL_Renderer *renderer = NULL;
    initialize_renderer(&renderer, window);

    double w = (double) (master_surface -> w);
    double h = (double) (master_surface -> h);

    double angle = 45;
    double rad = angle * M_PI / 180.0;
    double bw = fabs(w * cos(rad)) + fabs(h * sin(rad));
    double bh = fabs(w * sin(rad)) + fabs(h * cos(rad));

    int maxw = (int)ceil(bw);
    int maxh = (int)ceil(bh);

    SDL_SetWindowSize(window, maxw, maxh);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_Texture *window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, maxw, maxh);

    SDL_Texture *image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, master_surface);

    render_texture_rotated(renderer, image_texture, window_texture,maxw, maxh,w, h,  angle);
    SDL_RenderCopy(renderer, window_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_Delay(4000); //This is temporary, to give you time to see the window

    terminate_window(window);
    terminate_renderer(renderer);
    terminate_texture(image_texture);
    terminate_texture(window_texture);
    SDL_FreeSurface(master_surface);
    SDL_Quit();
    printf("Window successfully created!\n");
    return EXIT_SUCCESS;
}


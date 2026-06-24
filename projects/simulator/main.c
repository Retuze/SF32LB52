/**
 * @file main.c
 * @brief PC Simulator — watch UI development & debugging harness
 *
 * Build: cmake --preset simulator && cmake --build build/simulator
 *
 * Stubs out HAL/BSP so LVGL-based UI code can run on the host.
 * Optional SDL2 backend for display output.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAS_SDL2
#include <SDL2/SDL.h>
#endif

int main(int argc, char **argv)
{
    printf("[simulator] Watch simulator starting...\n");

#ifdef HAS_SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Watch Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        466, 466, SDL_WINDOW_SHOWN);
    if (win == NULL) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *rend = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    int running = 1;
    SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            }
        }

        SDL_SetRenderDrawColor(rend, 0x22, 0x22, 0x22, 0xFF);
        SDL_RenderClear(rend);
        /* TODO: LVGL render flush callback */
        SDL_RenderPresent(rend);

        SDL_Delay(16); /* ~60 fps */
    }

    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
#else
    printf("[simulator] Headless mode — no SDL2 display\n");
    printf("[simulator] Press Ctrl+C to exit\n");
    while (1) {
        /* TODO: run LVGL tick in headless mode */
    }
#endif

    return 0;
}

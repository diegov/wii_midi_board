/* midi_board: a little program that reads info from the wii balance board
 * and outputs midi control messages through jack
 *
 * Copyright (C) 2012 Diego Veralli <diegoveralli@yahoo.co.uk>
 *
 *  This file is part of midi_board.
 *  
 *  midi_board is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  midi_board is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with midi_board. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <SDL/SDL.h>
#include "gui.h"

typedef struct _sdl_context 
{
  SDL_Surface* screen;
} _sdl_context_t;

sdl_context midi_board_sdl_init_screen(int width, int height)
{
  SDL_Init(SDL_INIT_VIDEO);

  _sdl_context_t *ctx = malloc(sizeof(_sdl_context_t));

  ctx->screen = SDL_SetVideoMode(width, height, 16, SDL_HWSURFACE|SDL_DOUBLEBUF);
  return (void*)ctx;
}

void midi_board_sdl_draw_dot(sdl_context ctx, int x, int y, int radius)
{
  SDL_Surface *screen = ((_sdl_context_t*)ctx)->screen;
  Uint32 *pixmem32;
  Uint32 colour;

  SDL_Rect rect;
  rect.x = x - radius; 
  rect.y = y - radius;

  rect.h = radius * 2;
  rect.w = radius * 2;

  colour = SDL_MapRGB(screen->format, 200, 200, 10);

  SDL_FillRect(screen, &rect, colour);
}

void midi_board_sdl_draw_dot_rel(sdl_context ctx, double x, double y, int radius)
{
  SDL_Surface *screen = ((_sdl_context_t*)ctx)->screen;

  int real_x = (int)(x * screen->w);
  int real_y = (int)(y * screen->h);
  midi_board_sdl_draw_dot(ctx, real_x, real_y, radius);
}

void midi_board_sdl_wipe(sdl_context ctx)
{
  SDL_Surface *screen = ((_sdl_context_t*)ctx)->screen;
  SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 0, 0, 0));
}

void midi_board_sdl_flip_screen(sdl_context ctx)
{
  SDL_Surface *screen = ((_sdl_context_t*)ctx)->screen;
  SDL_Flip(screen);
}

void midi_board_sdl_teardown(sdl_context ctx)
{
  free(ctx);
  SDL_Quit();
}







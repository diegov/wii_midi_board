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

typedef void* sdl_context;

sdl_context midi_board_sdl_init_screen(int width, int height);

void midi_board_sdl_draw_dot(sdl_context ctx, int x, int y, int radius);

void midi_board_sdl_draw_dot_rel(sdl_context ctx, double x, double y, int radius);

void midi_board_sdl_wipe(sdl_context ctx);

void midi_board_sdl_flip_screen(sdl_context ctx);

void midi_board_sdl_teardown(sdl_context ctx);


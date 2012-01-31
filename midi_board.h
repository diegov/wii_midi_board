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

#include "config.h"
#include <stdio.h>
#include <cwiid.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include "gui.h"

#ifdef JACK_SESSION
  #include <jack/session.h>
#endif

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

typedef struct midi_board_centre {
  double X;
  double Y;
  int processed;
} midi_board_centre_t;

typedef struct midi_board_jack_runtime_data {
  jack_client_t *client;
  jack_port_t *port;
  jack_nframes_t sample_rate;
} midi_board_jack_runtime_data_t;

typedef struct midi_board_options {
  char *session_id;
  double min_x;
  double max_x;
  double min_y;
  double max_y;
} midi_board_options_t;

typedef struct midi_board_data {
  midi_board_options_t options;
  int quit;

  cwiid_wiimote_t *wiimote;

  midi_board_centre_t centre;
  struct balance_cal calibration;

  midi_board_jack_runtime_data_t jack_runtime_data;
  jack_nframes_t control_ticks;

  unsigned int previous_X;
  unsigned int previous_Y;
  
  sdl_context screen;
} midi_board_data_t;

int midi_board_jack_send_cc(void *port_buffer, jack_nframes_t nframes, 
			    unsigned char chan, unsigned char cc, 
			    unsigned char value);

int midi_board_init_jack(midi_board_data_t *data);

int midi_board_jack_process(jack_nframes_t nframes, void *arg);

void* midi_board_wii_io_thread(void *arg);

void midi_board_jack_shutdown(void *arg);

void midi_board_jack_session_callback(jack_session_event_t *event, void *arg);

int midi_board_init_wiimote(cwiid_wiimote_t **wiimote, struct balance_cal *calibration);

int midi_board_update_centre(midi_board_data_t *data);


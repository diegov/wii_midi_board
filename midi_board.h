/* midi_board.h: header file for midi_board */

#include <stdio.h>
#include <cwiid.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

typedef struct midi_board_center {
  float X;
  float Y;
} midi_board_center_t;

typedef struct midi_board_jack_runtime_data {
  jack_client_t *client;
  jack_port_t *port;
  jack_nframes_t sample_rate;
  unsigned int previous_X;
  unsigned int previous_Y;
} midi_board_jack_runtime_data_t;

int midi_board_jack_send_cc(void *port_buffer, jack_nframes_t nframes, 
		       unsigned char chan, unsigned char cc, 
		       unsigned char value);

int midi_board_init_jack(midi_board_jack_runtime_data_t *jack_runtime_data,
			 JackProcessCallback process_callback, 
			 JackShutdownCallback shutdown_callback);

int midi_board_jack_process(jack_nframes_t nframes, void *arg);

void midi_board_jack_shutdown(void *arg);

int midi_board_init_wiimote(cwiid_wiimote_t **wiimote);

int midi_board_get_center(cwiid_wiimote_t *wiimote, midi_board_center_t *center);

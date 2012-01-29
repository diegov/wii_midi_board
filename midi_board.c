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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <cwiid.h>
#include "midi_board.h"
#include <time.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

cwiid_wiimote_t *wiimote;
struct balance_cal *calibration;

midi_board_jack_runtime_data_t jack_runtime_data;

jack_nframes_t ticks;
midi_board_centre_t *centre;

pthread_mutex_t wii_io_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  wii_must_fetch = PTHREAD_COND_INITIALIZER;

int main(int argc, char *argv[]) 
{
  ticks = 0;
  wiimote = NULL;
  calibration = malloc(sizeof(struct balance_cal));
  centre = malloc(sizeof(midi_board_centre_t));

  if (midi_board_init_wiimote(&wiimote, calibration)) {
    printf("Failed to init wiimote");
    return 1;
  }

  printf("Connected to the wii controller!\n");

  midi_board_init_jack_args_t args;
  args.runtime_data = &jack_runtime_data;
  args.process_callback = &midi_board_jack_process;
  args.shutdown_callback = &midi_board_jack_shutdown;

#ifdef JACK_SESSION

  args.session_id = NULL;
  args.session_callback = &midi_board_jack_session_callback;

#endif

  if (midi_board_init_jack(args))
    {
      fprintf(stderr, "Oh.. Error initialising jack\n");
      cwiid_close(wiimote);
      return 1;
    }

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, midi_board_wii_io_thread, NULL);

  while(1)
    {
      sleep(1);
      if (jack_runtime_data.quit)
	{
	  break;
	}
    }

  cwiid_close(wiimote);
  jack_client_close(jack_runtime_data.client);

  printf("Done\n");

  return 0;
}

int midi_board_jack_send_cc(void *port_buffer, jack_nframes_t nframes, 
			    unsigned char chan, 
			    unsigned char cc, unsigned char value)
{
  jack_midi_data_t *data_buffer;
  if ((data_buffer = jack_midi_event_reserve(port_buffer, 0, 3)) == NULL)
    {
      fprintf(stderr, "Failed to send cc data.\n");
      return 1;
    }

  data_buffer[0] |= 0xb0;
  data_buffer[0] |= (chan - 1);
  data_buffer[1] = cc;
  data_buffer[2] = value;

  return 0;
}

#ifdef JACK_SESSION 

void midi_board_jack_session_callback(jack_session_event_t *event, void *arg)
{
  char retval[100];

  snprintf(retval, 100, "midi_board_session_client %s", event->client_uuid);
  event->command_line = strdup(retval);

  jack_session_reply(jack_runtime_data.client, event);

  if (event->type == JackSessionSaveAndQuit) 
    {
      jack_runtime_data.quit = 1;
    }

  jack_session_event_free(event);
}

#endif

int midi_board_init_jack(midi_board_init_jack_args_t args)
{
  midi_board_jack_runtime_data_t *runtime_data = args.runtime_data;
#ifdef JACK_SESSION
  if (args.session_id) 
    {
      runtime_data->client = jack_client_open("wii_midi_board", JackSessionID, NULL, args.session_id);
    }
  else
    {
      runtime_data->client = jack_client_open("wii_midi_board", JackNullOption, NULL);
    }
#else
  runtime_data->client = jack_client_open("wii_midi_board", JackNullOption, NULL);
#endif
  if (!runtime_data->client)
    {
      fprintf(stderr, "Failed to connect to JACK. Make sure the server is running.\n");
      return 1;
    }

  runtime_data->port = jack_port_register(runtime_data->client, "midi_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (!runtime_data->port)
    {
      fprintf(stderr, "Failed to create the midi out port!\n");
      return 1;
    }

  if (jack_set_process_callback(runtime_data->client, args.process_callback, 0))
    {
      fprintf(stderr, "Failed to set the process callback!\n");
      return 1;
    }

  jack_on_shutdown(runtime_data->client, args.shutdown_callback, 0);

#ifdef JACK_SESSION
  if (jack_set_session_callback(runtime_data->client, args.session_callback, NULL))
    {
      fprintf(stderr, "Failed to set the session callback!\n");
      return 1;
    }
#endif

  runtime_data->sample_rate = jack_get_sample_rate(runtime_data->client);

  if (jack_activate(runtime_data->client))
    {
      fprintf(stderr, "Failed to activate the client!\n");
      return 1;
    }

  return 0;
}

int midi_board_should_send_cc(jack_nframes_t samples_passed) 
{
  const jack_nframes_t control_rate = 30; //TODO: configurable
  jack_nframes_t control_step = jack_runtime_data.sample_rate / control_rate;

  ticks += samples_passed;
  if (ticks > (control_step))
    {
      ticks -= control_step;
      return 1;
    }
  else
    {
      return 0;
    }
}

int midi_board_send_if_changed(unsigned int *old_val, unsigned int new_val, 
			       unsigned char cc, void *port_buffer, 
			       jack_nframes_t nframes)
{
  unsigned char new_val_normalised = MAX(MIN(127, new_val), 0);
  if (new_val_normalised != *old_val)
    {
      midi_board_jack_send_cc(port_buffer, nframes, 1, cc, 
			      new_val_normalised);
      *old_val = new_val_normalised;
    }
}

void* midi_board_wii_io_thread(void *arg)
{
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_mutex_lock(&wii_io_lock);

  while(1)
    {
      if (midi_board_update_centre(wiimote, centre, calibration))
      	{
      	  fprintf(stderr, "Oh.. Error getting the centre\n");
	  //TODO: Signal
      	  /* cwiid_close(wiimote); */
      	  /* jack_client_close(jack_runtime_data.client); */
      	  /* return 1; */
      	}
      else 
	{
	  centre->processed = 0;
	}

      //printf("We should be getting data here!\n");
      pthread_cond_wait(&wii_must_fetch, &wii_io_lock);
    }

  pthread_mutex_unlock(&wii_io_lock);
}

int midi_board_jack_process(jack_nframes_t nframes, void *arg)
{
  jack_port_t *port = jack_runtime_data.port;
  void *port_buffer = jack_port_get_buffer(port, nframes);

  if (!port_buffer)
    {
      fprintf(stderr, "Failed to find the buffer to wite cc data to!\n");
      return 1;
    }

  jack_midi_clear_buffer(port_buffer);
  if (pthread_mutex_trylock(&wii_io_lock) == 0) 
    {
      if (centre->processed == 0)
	{
	  unsigned char send_val = (unsigned char)((centre->X + 1) * 64);
      
	  midi_board_send_if_changed(&(jack_runtime_data.previous_X),
				     send_val,
				     11, port_buffer, nframes);

	  send_val = (unsigned char)((centre->Y + 1) * 64);
	  midi_board_send_if_changed(&(jack_runtime_data.previous_Y),
				     send_val,
				     7, port_buffer, nframes);
	  centre->processed = 1;
	}

      if (midi_board_should_send_cc(nframes))
	{
	  pthread_cond_signal(&wii_must_fetch);
	}

      pthread_mutex_unlock(&wii_io_lock);
    }
  return 0;
}

void midi_board_jack_shutdown(void *arg)
{
  exit(1);
}

int midi_board_update_centre(cwiid_wiimote_t *wiimote, 
			     midi_board_centre_t *centre, 
			     struct balance_cal *balance_cal) 
{
  if (cwiid_request_status(wiimote))
    {
      fprintf(stderr, "Oh.. Error getting the status\n");
      return 1;
    }
  
  struct cwiid_state state; 
  if (cwiid_get_state(wiimote, &state))
    {
      fprintf(stderr, "Oh... Error getting the state.\n");
      return 1;
    }

  if (midi_board_calculate_centre(&(state.ext.balance), balance_cal, 
				  centre))
    {
      fprintf(stderr, "Failed to calculate the centre.\n");
      return 1;
    }
  
  return 0;
}

uint16_t midi_board_get_calibrated_value(uint16_t reading, uint16_t *calibration)
{
  uint16_t weight;

  if (reading < calibration[1])
    {            
      weight = (1700 * (reading - calibration[0])) / (calibration[1] - calibration[0]);
    }
  else if (reading < calibration[2])
    {
      weight = (1700 * (reading - calibration[1])) / (calibration[2] - calibration[1]) + 1700;
    }
  else 
    {
      weight = (1700 * (reading - calibration[2])) / (calibration[2] - calibration[1]) + (3400);
    }
  
  return weight;
}

int midi_board_calculate_centre(struct balance_state *readings, 
				struct balance_cal *balance_cal,
				midi_board_centre_t *centre) 
{
  uint16_t top_right, top_left, bottom_right, bottom_left;
  top_right = midi_board_get_calibrated_value(readings->right_top,
					      balance_cal->right_top);

  top_left = midi_board_get_calibrated_value(readings->left_top,
					     balance_cal->left_top);

  bottom_right = midi_board_get_calibrated_value(readings->right_bottom,
						 balance_cal->right_bottom);

  bottom_left = midi_board_get_calibrated_value(readings->left_bottom,
						balance_cal->left_bottom);

  //printf("Got values! (%u %u %u %u)\n", top_left, top_right, bottom_right, bottom_left);

  double top, bottom, left, right, total_weight;
  top = (top_right + top_left);
  bottom = (bottom_right + bottom_left);

  left = (top_left + bottom_left);
  right = (top_right + bottom_right);

  total_weight = top + bottom;

  centre->X = top / total_weight - bottom / total_weight;
  centre->Y = left / total_weight - right / total_weight;

  //printf("X: %f. y: %f\n", centre->X, centre->Y);

  return 0;
}

int midi_board_init_wiimote(cwiid_wiimote_t **wiimote, 
			    struct balance_cal *calibration) 
{
  bdaddr_t bdaddr = *BDADDR_ANY;

  printf("Synchronise balance board now. Press enter to continue!\n");
  getchar();

  *wiimote = cwiid_open(&bdaddr, CWIID_FLAG_CONTINUOUS);
  if (!(*wiimote)) 
    {
      fprintf(stderr, "Failed to connect, shit!\n");
      return 1;
    }

  if (cwiid_set_rpt_mode(*wiimote, CWIID_RPT_EXT | CWIID_RPT_ACC | CWIID_RPT_BALANCE))
    {
      fprintf(stderr, "Failed to set the report mode!\n");
      return 1;
    }

  cwiid_command(*wiimote, CWIID_CMD_LED, CWIID_LED1_ON | CWIID_LED4_ON);

  printf("Now getting the calibration. Press enter to continue...\n");
  getchar();
  if (cwiid_get_balance_cal(*wiimote, calibration))
    {
      fprintf(stderr, "Failed to balance board calibration data.\n");
      return 1;
    }

  printf("Calibration: (%u %u %u, %u %u %u, %u %u %u, %u %u %u)\n\n", 
	 calibration->left_top[0],
	 calibration->left_top[1],
	 calibration->left_top[2],
	 calibration->right_top[0],
	 calibration->right_top[1],
	 calibration->right_top[2],
	 calibration->right_bottom[0],
	 calibration->right_bottom[1],
	 calibration->right_bottom[2],
	 calibration->left_bottom[0],
	 calibration->left_bottom[1],
	 calibration->left_bottom[2]);

  return 0;
}


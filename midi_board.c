/* midi_board.c: a little program that reads info from the wii balance board
   and outputs midi control messages through jack */

#include <stdio.h>
#include <bluetooth/bluetooth.h>
#include <cwiid.h>
#include "midi_board.h"
#include <time.h>
#include <jack/jack.h>
#include <jack/midiport.h>

cwiid_wiimote_t *wiimote;
midi_board_jack_runtime_data_t jack_runtime_data;

jack_nframes_t ticks;

int main(int argc, char* argv[]) 
{
  ticks = 0;
  wiimote = NULL;

  if (midi_board_init_wiimote(&wiimote)) {
    printf("Failed to init wiimote");
    return 1;
  }

  printf("Connected to the wii controller!\n");

  if (midi_board_init_jack(&jack_runtime_data, 
			   &midi_board_jack_process, 
			   &midi_board_jack_shutdown)) 
    {
      printf("Oh.. Error initialising jack\n");
      cwiid_close(wiimote);
      return 1;
    }

  while(1)
    {
      sleep(1);
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
      printf("Failed to send cc data.\n");
      return 1;
    }

  data_buffer[0] |= 0xb0;
  data_buffer[0] |= (chan - 1);
  data_buffer[1] = cc;
  data_buffer[2] = value;

  return 0;
}

int midi_board_init_jack(midi_board_jack_runtime_data_t *runtime_data,
			 JackProcessCallback process_callback, 
			 JackShutdownCallback shutdown_callback) 
{
  runtime_data->client = jack_client_open("wii_midi_board", (jack_options_t)0, NULL);
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

  if (jack_set_process_callback(runtime_data->client, process_callback, 0))
    {
      fprintf(stderr, "Failed to set the process callback!\n");
      return 1;
    }

  jack_on_shutdown(runtime_data->client, shutdown_callback, 0);

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
  const jack_nframes_t control_rate = 30;
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
			       unsigned char cc, void* port_buffer, 
			       jack_nframes_t nframes)
{
  unsigned int new_val_normalised = MIN(127, (new_val - 80));
  if (new_val_normalised != *old_val)
    {
      midi_board_jack_send_cc(port_buffer, nframes, 1, cc, 
			      new_val_normalised);
      *old_val = new_val_normalised;
    }
}

int midi_board_jack_process(jack_nframes_t nframes, void *arg)
{
  jack_port_t *port = jack_runtime_data.port;
  void *port_buffer = jack_port_get_buffer(port, nframes);

  if (!port_buffer)
    {
      printf("Failed to find the buffer to wite cc data to!\n");
      return 1;
    }

  jack_midi_clear_buffer(port_buffer);

  if (midi_board_should_send_cc(nframes))
    {
      midi_board_center_t center;

      if (midi_board_get_center(wiimote, &center)) 
	{
	  printf("Oh.. Error getting the center\n");
	  cwiid_close(wiimote);
	  jack_client_close(jack_runtime_data.client);
	  return 1;
	}

      midi_board_send_if_changed(&(jack_runtime_data.previous_X), 
       				 center.X, 11, port_buffer, nframes); 

      midi_board_send_if_changed(&(jack_runtime_data.previous_Y),
      				 center.Y, 7, port_buffer, nframes);
    }
  return 0;
}

void midi_board_jack_shutdown(void *arg)
{
  exit(1);
}

int midi_board_get_center(cwiid_wiimote_t *wiimote, midi_board_center_t *center) 
{
  if (cwiid_request_status(wiimote))
    {
      printf("Oh.. Error getting the status\n");
      return 1;
    }
  
  struct cwiid_state state;
  if (cwiid_get_state(wiimote, &state))
    {
      printf("Oh... Error getting the state.\n");
      return 1;
    }

  center->X = state.acc[0];
  center->Y = state.acc[1];

  /* printf("acc[0]: %u\n", state.acc[0]);  */
  /* printf("acc[1]: %u\n", state.acc[1]);  */
  /* printf("acc[2]: %u\n", state.acc[2]);  */
  
  return 0;
}

int midi_board_init_wiimote(cwiid_wiimote_t **wiimote) 
{
  bdaddr_t bdaddr = *BDADDR_ANY;

  *wiimote = cwiid_open(&bdaddr, CWIID_FLAG_CONTINUOUS);
  if (!(*wiimote)) 
    {
      printf("Failed to connect, shit!\n");
      return 1;
    }

  if (cwiid_set_rpt_mode(*wiimote, CWIID_RPT_EXT | CWIID_RPT_ACC))
    {
      printf("Failed to set the report mode!\n");
      return 1;
    }

  cwiid_command(*wiimote, CWIID_CMD_LED, CWIID_LED1_ON | CWIID_LED4_ON);

  return 0;
}


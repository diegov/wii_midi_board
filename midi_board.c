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
struct balance_cal *calibration;

midi_board_jack_runtime_data_t jack_runtime_data;

jack_nframes_t ticks;

int main(int argc, char *argv[]) 
{
  ticks = 0;
  wiimote = NULL;
  calibration = malloc(sizeof(struct balance_cal));

  if (midi_board_init_wiimote(&wiimote, calibration)) {
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
      midi_board_centre_t centre;

      if (midi_board_update_centre(wiimote, &centre, calibration)) 
	{
	  printf("Oh.. Error getting the centre\n");
	  cwiid_close(wiimote);
	  jack_client_close(jack_runtime_data.client);
	  return 1;
	}

      unsigned char send_val = (unsigned char)((centre.X + 1) * 64);
      //printf("Original: %f. Converted: %u\n", centre.X, send_val);
      midi_board_send_if_changed(&(jack_runtime_data.previous_X), 
       				 send_val, 
				 11, port_buffer, nframes); 

      send_val = (unsigned char)((centre.Y + 1) * 64);
      midi_board_send_if_changed(&(jack_runtime_data.previous_Y),
       				 send_val,
       				 7, port_buffer, nframes); 
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
      printf("Oh.. Error getting the status\n");
      return 1;
    }
  
  struct cwiid_state state;
  if (cwiid_get_state(wiimote, &state))
    {
      printf("Oh... Error getting the state.\n");
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
      printf("Failed to connect, shit!\n");
      return 1;
    }

  if (cwiid_set_rpt_mode(*wiimote, CWIID_RPT_EXT | CWIID_RPT_ACC | CWIID_RPT_BALANCE))
    {
      printf("Failed to set the report mode!\n");
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


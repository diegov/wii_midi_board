/* midi_board.c: a little program that reads info from the wii balance board
   and outputs midi control messages through jack */

#include <stdio.h>
#include <bluetooth/bluetooth.h>
#include <cwiid.h>
#include "midi_board.h"
#include <time.h>

int midi_board_get_center(cwiid_wiimote_t *wiimote, midi_board_center_t *center) 
{
  if (cwiid_request_status(wiimote))
    {
      printf("Oh.. Error getting the status\n");
      return 1;
    }
  printf("Got status!\n");

  struct cwiid_state state;
  if (cwiid_get_state(wiimote, &state))
    {
      printf("Oh... Error getting the state.\n");
      return 1;
    }

  printf("Got state!\n");
  printf("acc: %u\n", state.acc[0]);

  return 0;
}

int init_wiimote(cwiid_wiimote_t **wiimote) 
{
  bdaddr_t bdaddr = *BDADDR_ANY;

  *wiimote = cwiid_open(&bdaddr, CWIID_FLAG_CONTINUOUS);
  if (!(*wiimote)) 
    {
      printf("Failed to connect, shit!\n");
      return 1;
    }

  cwiid_command(*wiimote, CWIID_CMD_LED, CWIID_LED1_ON | CWIID_LED4_ON);
  return 0;
}

int main(int argc, char* argv[]) 
{
  cwiid_wiimote_t *wiimote = NULL;

  if (init_wiimote(&wiimote)) {
    printf("Failed to init wiimote");
    return 1;
  }
  
  printf("Connected!\n");

  midi_board_center_t center;

  int j;
  for (j = 0; j < 20; ++j) 
    {
      if (midi_board_get_center(wiimote, &center)) 
	{
	  printf("Oh.. Error getting the center\n");
	  cwiid_close(wiimote);
	  return 1;
	}
      sleep(1);
    }

  cwiid_close(wiimote);

  printf("Done\n");
  return 0;
}


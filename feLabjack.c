/********************************************************************\
 Labjack readout frontend
 Thomas Lindner (TRIUMF)
\********************************************************************/


#include <vector>
#include <stdio.h>
#include <algorithm>
#include <stdlib.h>
#include "midas.h"
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <LabJackM.h>


#include "LJM_Utilities.h"

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

/*-- Globals -------------------------------------------------------*/

/* The frontend name (client name) as seen by other MIDAS clients   */
char *frontend_name = "feLabjack";
/* The frontend file name, don't change it */
char *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
BOOL frontend_call_loop = TRUE;

/* a frontend status page is displayed with this frequency in ms */
INT display_period = 0;

/* maximum event size produced by this frontend */
INT max_event_size =  3 * 1024 * 1024;

/* maximum event size for fragmented events (EQ_FRAGMENTED) --- not really used here */
INT max_event_size_frag = 2 * 1024 * 1024;

/* buffer size to hold events */
INT event_buffer_size = 20 * 1000000;

/* handle for labjack device*/
INT handle;
  
enum { NUM_CHANNELS = 15 };

const char *CHANNEL_NAMES[] = {"AIN96", "AIN97", "AIN98", "AIN3", "AIN4",
			 "AIN5", "AIN6", "AIN7", "AIN8", "AIN9", 
 			 "AIN10", "AIN11", "AIN12", "AIN13", "AIN14"};


double INIT_SCAN_RATE = 100000/NUM_CHANNELS*0.9;

INT SCANS_PER_READ = INIT_SCAN_RATE/NUM_CHANNELS;
  

  /*  // How fast to stream in Hz
  double INIT_SCAN_RATE = 2000;
  
  // How many scans to get per call to LJM_eStreamRead. INIT_SCAN_RATE/2 is
  // recommended
  int SCANS_PER_READ = INIT_SCAN_RATE / 2;
  
  
  // Channels/Addresses to stream. NUM_CHANNELS can be less than or equal to
  // the size of CHANNEL_NAMES
  enum { NUM_CHANNELS = 2 };
  const char * CHANNEL_NAMES[] = {"AIN0", "AIN1"};
  */
INT err, iteration, channel;
  //INT numSkippedScans = 0;
  //INT totalSkippedScans = 0;
  //INT deviceScanBacklog = 0;
  //INT LJMScanBacklog = 0;

INT * aScanList = (INT *) malloc(sizeof(int) * NUM_CHANNELS);
INT aDataSize = NUM_CHANNELS * SCANS_PER_READ;
double * aData = (double *) malloc(sizeof(double) * aDataSize);

/*-- Function declarations -----------------------------------------*/

INT frontend_init();
INT frontend_exit();
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT HardcodedConfigureStream(INT handle);

INT frontend_loop();

INT read_labjack_event(char *pevent, INT iter);

/*-- Equipment list ------------------------------------------------*/

EQUIPMENT equipment[] = {

   {"Labjack01",               /* equipment name */
    {1, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     EQ_PERIODIC,              /* equipment type */
     LAM_SOURCE(0, 0xFFFFFF),        /* event source crate 0, all stations */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     RO_ALWAYS,           /* read only when running */
     1000,                    /* poll for 1000ms */
     0,                      /* stop run after this event limit */
     0,                      /* number of sub events */
     1,                      /* don't log history */
     "", "", "",},
    read_labjack_event,      /* readout routine */
    },

   {""}
};

#ifdef __cplusplus
}
#endif

/********************************************************************\
              Callback routines for system transitions

  These routines are called whenever a system transition like start/
  stop of a run occurs. The routines are called on the following
  occations:

  frontend_init:  When the frontend program is started. This routine
                  should initialize the hardware.

  frontend_exit:  When the frontend program is shut down. Can be used
                  to releas any locked resources like memory, commu-
                  nications ports etc.

  begin_of_run:   When a new run is started. Clear scalers, open
                  rungates, etc.

  end_of_run:     Called on a request to stop a run. Can send
                  end-of-run event and close run gates.

  pause_run:      When a run is paused. Should disable trigger events.

  resume_run:     When a run is resumed. Should enable trigger events.
\********************************************************************/

/*-- Frontend Init -------------------------------------------------*/
INT frontend_init()
{  
  
  // Connect to the labjack
  printf("Connecting to labjack01.ucn.triumf.ca...\n");
  
  // Attempts to open the first Labjack found
  handle = OpenOrDie(LJM_dtANY, LJM_ctANY, "142.90.151.7");
  
  PrintDeviceInfoFromHandle(handle);
  printf("\n");

  /*
  int err, iteration, channel;
  int numSkippedScans = 0;
  int totalSkippedScans = 0;
  int deviceScanBacklog = 0;  int LJMScanBacklog = 0;

  int * aScanList = malloc(sizeof(int) * NUM_CHANNELS);

  unsigned int aDataSize = NUM_CHANNELS * SCANS_PER_READ;
  double * aData = malloc(sizeof(double) * aDataSize); */

  // Clear aData. This is not strictly necessary, but can help debugging.
  memset(aData, 0, sizeof(double) * aDataSize);
  err = LJM_NamesToAddresses(NUM_CHANNELS, CHANNEL_NAMES, aScanList, NULL);
  ErrorCheck(err, "Getting positive channel addresses");

  HardcodedConfigureStream(handle);

  printf("\n");
  printf("Starting stream...\n");
  err = LJM_eStreamStart(handle, SCANS_PER_READ, NUM_CHANNELS, aScanList,
			 &INIT_SCAN_RATE);
  ErrorCheck(err, "LJM_eStreamStart");
  printf("Stream started. Actual scan rate: %.02f Hz (%.02f sample rate)\n",
	 INIT_SCAN_RATE, INIT_SCAN_RATE * NUM_CHANNELS);
  printf("\n");


  return SUCCESS;

}


/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{

  // Do any disconnecting of the labjack
  printf("Disconnecting from labjack!\n");

  free(aData);
  free(aScanList);

  CloseOrDie(handle);

  WaitForUserIfWindows();

  return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/
// Upon run stasrt, read ODB settings and write them to DCRC
INT begin_of_run(INT run_number, char *error)
{

  // Do any setup of the labjack that needs to be done at the begin of a run 
  printf("BOR setup\n");
  

  
  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{

  /* if (totalSkippedScans) {
     printf("\n****** Total number of skipped scans: %d ******\n\n",
	   totalSkippedScans);
   }
  */
   printf("Stopping stream\n");
   err = LJM_eStreamStop(handle);
   ErrorCheck(err, "Stopping stream");

   free(aData);
   free(aScanList);

   return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
   return SUCCESS;
}

/*-- Resume Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{
   return SUCCESS;
}

/*-- Create Stream Configuration------------------------------------*/

INT HardcodedConfigureStream(INT handle)
{
  const int STREAM_TRIGGER_INDEX = 0;
  const int STREAM_CLOCK_SOURCE = 0;
  const int STREAM_RESOLUTION_INDEX = 0;
  const double STREAM_SETTLING_US = 0;
  const double AIN_ALL_RANGE = 0;
  const int AIN_ALL_NEGATIVE_CH = LJM_GND;

  printf("Writing configurations:\n");

  if (STREAM_TRIGGER_INDEX == 0) {
    printf("    Ensuring triggered stream is disabled:");
  }
  printf("    Setting STREAM_TRIGGER_INDEX to %d\n", STREAM_TRIGGER_INDEX);
  WriteNameOrDie(handle, "STREAM_TRIGGER_INDEX", STREAM_TRIGGER_INDEX);

  if (STREAM_CLOCK_SOURCE == 0) {
    printf("    Enabling internally-clocked stream:");
  }
  printf("    Setting STREAM_CLOCK_SOURCE to %d\n", STREAM_CLOCK_SOURCE);
  WriteNameOrDie(handle, "STREAM_CLOCK_SOURCE", STREAM_CLOCK_SOURCE);

  // Configure the analog inputs' negative channel, range, settling time and
  // resolution.
  // Note: when streaming, negative channels and ranges can be configured for
  // individual analog inputs, but the stream has only one settling time and
  // resolution.
  printf("    Setting STREAM_RESOLUTION_INDEX to %d\n",
	 STREAM_RESOLUTION_INDEX);
  WriteNameOrDie(handle, "STREAM_RESOLUTION_INDEX", STREAM_RESOLUTION_INDEX);

  printf("    Setting STREAM_SETTLING_US to %f\n", STREAM_SETTLING_US);
  WriteNameOrDie(handle, "STREAM_SETTLING_US", STREAM_SETTLING_US);

  printf("    Setting AIN_ALL_RANGE to %f\n", AIN_ALL_RANGE);
  WriteNameOrDie(handle, "AIN_ALL_RANGE", AIN_ALL_RANGE);

  printf("    Setting AIN_ALL_NEGATIVE_CH to ");

  if (AIN_ALL_NEGATIVE_CH == LJM_GND) {
    printf("LJM_GND");
  }
  else {
    printf("%d", AIN_ALL_NEGATIVE_CH);
  }

  printf("\n");
  WriteNameOrDie(handle, "AIN_ALL_NEGATIVE_CH", AIN_ALL_NEGATIVE_CH);

  return SUCCESS;
}


/*-- Frontend Loop -------------------------------------------------*/

INT frontend_loop()
{
   /* if frontend_call_loop is true, this routine gets called when
      the frontend is idle or once between every event */
  usleep(50);
  return SUCCESS;
}

/*------------------------------------------------------------------*/

/********************************************************************\

  Readout routines for different events

\********************************************************************/

/*-- Trigger event routines ----------------------------------------*/
// Not currently used for DCRC readout
extern "C" { INT poll_event(INT source, INT count, BOOL test)
/* Polling routine for events. Returns TRUE if event
   is available. If test equals TRUE, don't return. The test
   flag is used to time the polling */
{
   int i;

   for (i = 0; i < count; i++) {
//      cam_lam_read(LAM_SOURCE_CRATE(source), &lam);

//      if (lam & LAM_SOURCE_STATION(source))
         if (!test)
            return 1;
   }

   usleep(1000);
   return 0;
}
}

/*-- Interrupt configuration ---------------------------------------*/
// This is not currently used by the DCRC readout
extern "C" { INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
   switch (cmd) {
   case CMD_INTERRUPT_ENABLE:
      break;
   case CMD_INTERRUPT_DISABLE:
      break;
   case CMD_INTERRUPT_ATTACH:
      break;
   case CMD_INTERRUPT_DETACH:
      break;
   }
   return SUCCESS;
}
}


/*-- Event readout -------------------------------------------------*/
INT read_labjack_event(char *pevent, INT iter)
{

  /* init bank structure */
  bk_init32(pevent);
  
  double *pdata;
  /* create bank of double words */
  bk_create(pevent, "LBJK", TID_DOUBLE, (void **)&pdata);
 
  printf("Reading data from labjack, making bank\n");  
  
  int deviceScanBacklog = 0;
  int LJMScanBacklog = 0;

  err = LJM_eStreamRead(handle, aData, &deviceScanBacklog,
  			&LJMScanBacklog);
  ErrorCheck(err, "LJM_eStreamRead");

  // printf("iteration: %d - deviceScanBacklog: %d, LJMScanBacklog: %d\n",
  //	 iteration, deviceScanBacklog, LJMScanBacklog);
  
  printf("Scan #%d of %d:\n",iter, SCANS_PER_READ);
  for (channel = 0; channel < NUM_CHANNELS; channel++) {
    printf("    %s = %0.5f\n", CHANNEL_NAMES[channel], aData[channel]);
    *pdata++ = aData[channel]*100;  
    //*pdata++ = channel;
  }

  /* numSkippedScans = CountAndOutputNumSkippedScans(NUM_CHANNELS,
						  SCANS_PER_READ, aData);

  if (numSkippedScans) {
    printf("  %d skipped scans in this LJM_eStreamRead\n",
	   numSkippedScans);
    totalSkippedScans += numSkippedScans;
    }*/
  int size = bk_close(pevent, pdata);
  return bk_size(pevent);
}




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

#include <math.h>

#include <LabJackM.h>


#include "LJM_Utilities.h"

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

/*-- Globals -------------------------------------------------------*/

/* The frontend name (client name) as seen by other MIDAS clients   */
char *frontend_name = "feLabjack02";
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


/*
// FOR OLD DAQ BOARD, SINGLE 
const char *CHANNEL_NAMES[] = {"AIN4", "AIN5", "AIN6"};
*/


/*
// For Fluxgate Input Orientation (x, y, z) in order: 1, 4, 5, 8, 9
const char *CHANNEL_NAMES[] = {
	"AIN73", "AIN75", "AIN77",
	"AIN78", "AIN80", "AIN82",
	"AIN99", "AIN101", "AIN103",
	"AIN102", "AIN104", "AIN106",
	"AIN115", "AIN117", "AIN119"
};
*/


// For Fluxgate Input Orientation (x, y, z) in order: 2, 3, 6, 7, 10
const char *CHANNEL_NAMES[] = {
	"AIN72", "AIN74", "AIN76", 
	"AIN79", "AIN81", "AIN83",
	"AIN96", "AIN98", "AIN100", 
	"AIN107", "AIN109", "AIN110", 
	"AIN108", "AIN111", "AIN113"
};

// Data slotFluxgate plugins
// FG#    DAQ IN#      Data_Slot_MIDAS#
// 1      10           4
// 2      7            3
// 3      6            2
// 4      3            1
// 5      2            0


enum { NUM_CHANNELS = sizeof(CHANNEL_NAMES) / sizeof(CHANNEL_NAMES[0]) };


/* double INIT_SCAN_RATE = 100000/NUM_CHANNELS*0.9;

INT SCANS_PER_READ = INIT_SCAN_RATE/NUM_CHANNELS;

*/  

// How fast to stream in Hz
//double INIT_SCAN_RATE = 1000; // Edited by Stewart - was 2000 originally
double INIT_SCAN_RATE = 1500; // Edited by Stewart - was 2000 originally

// Cliff comment INIT_SCAN_RATE, labjack variable, check documentation for true meaning. Cliff's guess: Determines the sampling frequency of the labjack 
// which is then distributed amongst all the channels

// How many scans to get per call to LJMeStreamRead. INIT_SCAN_RATE/2 is
// recommended
//int SCANS_PER_READ = INIT_SCAN_RATE/10; // Edited by Stewart - was INIT_SCAN_RATE / 2 initially


// Cliffs comments on SAMPLES_PER_AVG: Cliff defined variable. Dictactes how many samples from a channel is taken before a mean and std calculation is made and then passed to MIDAS.
int SAMPLES_PER_AVG = 10;
int DP_PER_MEVENT = 1;   // Data points per MIDAS event for each channel each second (Hz)

// Cliff's comments on SCANS_PER_READ: Labjack defined variable. Dictates how many times labjack will sample a channel before the channel value is reported when read.
int SCANS_PER_READ = (INIT_SCAN_RATE / (SAMPLES_PER_AVG*DP_PER_MEVENT)) + 1;  // plus 1 to ensure labjack buffer empties faster than it fills

// Cliff comment's example sampling frequencies:
// 1 Hz:
// DP_PER_MEVENT = 1
// SAMPLES_PER_AVG = 10
// INIT_SCAN_RATE = 1500 (labjack samples channel each of the 15 channels 100 times per read)
// SCANS_PER_READ (calculated) = 150


/*
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

//INT aDataSize = NUM_CHANNELS * SCANS_PER_READ;
//double * aData = (double *) malloc(sizeof(double) * aDataSize);  // Unused


INT streamDataSize = NUM_CHANNELS * SCANS_PER_READ;  // Can be smaller? Labjack requires it this size?
double * streamData = (double *) malloc(sizeof(double) * streamDataSize);

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

   {"Labjack02",               /* equipment name */
    {1, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     EQ_PERIODIC,              /* equipment type */
     LAM_SOURCE(0, 0xFFFFFF),        /* event source crate 0, all stations */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     RO_ALWAYS,           /* read only when running */

     1,                    /* poll for 1000ms */ // Edited by Stewart - runs readout routine every 1ms(?)
     0,                      /* stop run after this event limit */
     0,                      /* number of sub events */
     1,                      /* don't log history */
     "", "", "",},
//     1,                      /* don't log history */ - Edited by Stewart, uncertain why this was here
//     "", "", "",},
//    1,                      /* don't log history */
//     "", "", "",},
    read_labjack_event,      /* readout routine */
    },

    {""}
};

#ifdef __cplusplus
 }
#endif

/********************************************************************\
              Callback routines for system transitions

network settings for labkjack01
gateway: 142.90.151.254
DNS : 142.90.100.19
alt DNS: 142.90.113.69

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

  //handle = OpenOrDie(LJM_dtANY, LJM_ctANY, LJM_idANY);   // Currently only works for direct USB connection
  
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
  //memset(aData, 0, sizeof(double) * aDataSize);
  memset(streamData, 0, sizeof(double) * streamDataSize);
  err = LJM_NamesToAddresses(NUM_CHANNELS, CHANNEL_NAMES, aScanList, NULL);
  ErrorCheck(err, "Getting positive channel addresses");

  HardcodedConfigureStream(handle);

  printf("\nINIT_SCAN_RATE is %02f, SCANS_PER_READ is %d, aScanList is %d  \n", INIT_SCAN_RATE, SCANS_PER_READ, aScanList); // Stewart testing some stuff
  
  printf("\nNumber of channels: %d\n", NUM_CHANNELS);

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

  printf("Stopping stream\n");
  err = LJM_eStreamStop(handle);
  ErrorCheck(err, "Stopping stream");

  // Do any disconnecting of the labjack
  printf("Disconnecting from labjack!\n");

  free(streamData);
  //free(aData);
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

   //usleep(1000);
   usleep(250);
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
} // insert by Stewart trying to fix stuff
/*-- Event readout -------------------------------------------------*/
INT read_labjack_event(char *pevent, INT iter)
{

  /* init bank structure */
  bk_init32(pevent);
  
  double *pdata;
  /* create bank of double words */
  bk_create(pevent, "LBJK", TID_DOUBLE, (void **)&pdata);
 
  //printf("Reading data from labjack, making bank\n");  
  
  int deviceScanBacklog = 0;
  int LJMScanBacklog = 0;

  
  // MODIFICATION TO COLLECT DATA AT 100Hz (FEB 4th)
  
  // COLLECT DATA FOR MEAN AND STD
  double subData[NUM_CHANNELS*SAMPLES_PER_AVG];
  double sum[NUM_CHANNELS] = {0};
  double mean[NUM_CHANNELS] = {0};
  double std[NUM_CHANNELS] = {0};
  int i, j;
  for (j=0; j<DP_PER_MEVENT; j++) {
    // POLL LABJACK SAMPLES_PER_AVG TIMES
    for(i=0; i<SAMPLES_PER_AVG; i++) {
      err = LJM_eStreamRead(handle, streamData, &deviceScanBacklog, &LJMScanBacklog);
      if(err == 1221){ // ignore errors of type 1221

	cm_msg(MINFO,"read_labjack_event","Gotten labjack error with error number = 1221");

	static int error_count = 0;
	error_count++;
	if(error_count > 100) ErrorCheck(err, "LJM_eStreamRead too many errors");
	
	  
      }else{
	ErrorCheck(err, "LJM_eStreamRead Cann I add extra info???");
      }

      for (channel = 0; channel < NUM_CHANNELS; channel++) {
        subData[SAMPLES_PER_AVG*channel + i] = streamData[channel];
      }
    }

    // CALCULATE MEAN AND STD
    for(channel = 0; channel < NUM_CHANNELS; channel++) {
      sum[channel] = 0;
      std[channel] = 0;

      // MEAN
      for(i=0; i<SAMPLES_PER_AVG; i++){
        sum[channel] += subData[channel*SAMPLES_PER_AVG + i];
      }
      mean[channel] = sum[channel] / SAMPLES_PER_AVG;

      // STD
      for(i=0; i<SAMPLES_PER_AVG; i++){
        std[channel] += pow(subData[channel*SAMPLES_PER_AVG + i] - mean[channel], 2);
      }
      std[channel] = sqrt(std[channel] / SAMPLES_PER_AVG);
    }

    // Timestamp once per second
    if (j == 0){
      printf("iteration: %d - deviceScanBacklog: %d, LJMScanBacklog: %d\n", iteration, deviceScanBacklog, LJMScanBacklog);
      *pdata++ = (double)time(NULL);
      //*pdata++ = (float)time(NULL);   // test
    }
    // ASSEMBLE DATA FOR MIDAS
    // time, sample0, sample1, sample2.... sample99
    // sample# = ch0_val, ch0_std, ch1_val, ch1_std... etc.
    for (channel = 0; channel < NUM_CHANNELS; channel++) {
      if (j == 0){
        //printf(" Channel: %i    \t Mean: %f \t Std %f \n", channel, mean[channel], std[channel]);
	printf(" %s\t Mean: %f \t Std %f \n", CHANNEL_NAMES[channel], mean[channel], std[channel]);
      }
      *pdata++ = mean[channel];
      *pdata++ = std[channel];
    }
  }

  //int size = bk_close(pevent, pdata);
  bk_close(pevent, pdata);
  return bk_size(pevent);

}
}

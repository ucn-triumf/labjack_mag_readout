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
#include <sys/time.h>
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
// These channel names are assigned to LabJack addresses in frontend_init()
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

enum { NumAddresses = sizeof(CHANNEL_NAMES) / sizeof(CHANNEL_NAMES[0]) };

/********************************************************************\
	Some notes on LabJack configuration

An "address" refers to the Modbus address of a single channel. A "sample"
refers to a single value, from a single address. A "scan" refers to a reading
of one value from each of the addresses. Hence the "Sample Rate" will be a 
factor of NumAddresses larger than the ScanRate. The DAQ sequence follows the
following steps:

	1) The flux gates make measurements, and place values in their buffer,
		LabJack calls it the "device buffer". This process occurs at the 
		ScanRate specified by the user.
	2) When eStreamStart is first called, a "stream" is initialized, along
		with a second buffer, called the "LJM buffer". In the background,
		LabJack is continuously transferring data from the device buffer to
		the LabJack buffer. This process is quite efficient, happening on a
		separate thread, which explains why one shouldn't ever really see 
		a non-zero value in the print out of "deviceScanBacklog" (the present
		size of data in the device buffer upon most recent reading). 
	3) When eStreamRead is called, the amount of data specified by 
		ScansPerRead is transferred to the array that is given to eStreamRead
		as an argument. 
	4) Analysis is performed on this acquired data, and then it is passed to 
		MIDAS as an event.

Wanting to change the time interval over which values are collected, averaged,
and given to MIDAS as a single event? -> Change the ODB Period setting (link
below)

Wanting to change the number of measurements used in the averaging for a 
single MIDAS event? -> Change the ScanRate parameter below. Note how this 
implies a frequency of (ScanRate) / (MIDAS Period) for the independent 
samples before averaging. 

\********************************************************************/

// This determines how many values will be grabbed, per address, per 
// MIDAS ODB Period. The "Period" parameter under MIDAS' ODB settings is changed
// at https://daq01.ucn.triumf.ca/Equipment/Labjack02/Common . The Period parameter controls how often
// eStreamRead is called, and eStreamRead is confgured such that it will halt
// until enough scanned data is available for it to read (as specified by 
// ScansPerRead). Thus one needs to be careful to coordinate these timing
// parameters, as described above. The ScanRate variable is not necessarily in
// Hz, in fact it is only in Hz if the ODB Period is equal to 1000 (in 
// micro-sec). This rate is really in units of [scans per ODB Period].
double ScanRate = 10; 

// Data points per MIDAS event for each channel
// (!!!) I do not understand why this would ever change?
int DP_PER_MEVENT = 1;  

// This determines the number of scans that are pulled from the LJM buffer
// each call to eStreamRead.
int ScansPerRead = ScanRate*DP_PER_MEVENT;

/*
// Channels/Addresses to stream. NumAddresses can be less than or equal to
// the size of CHANNEL_NAMES
enum { NumAddresses = 2 };
const char * CHANNEL_NAMES[] = {"AIN0", "AIN1"};
*/
INT err, iteration, channel;
  //INT numSkippedScans = 0;
  //INT totalSkippedScans = 0;
  //INT deviceScanBacklog = 0;
  //INT LJMScanBacklog = 0; 

INT * aScanList = (INT *) malloc(sizeof(int) * NumAddresses);

// streamData is the array buffer within which LabJack will place data from
// the device, before it is read by eStreamRead. This just needs to be large
// enough to handle the ScanRate as initialized in eStreamStart
INT streamDataSize = NumAddresses * ScansPerRead;
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

// https://midas.triumf.ca/MidasWiki/index.php/Equipment_List_Parameters
// The above docs reference covers this aspect of MIDAS interaction well
// These parameters are read only once from this program by MIDAS, on the
// very first initialization of the frontend. Henceforth, any modification
// must be done on the ODB, at:
// https://daq01.ucn.triumf.ca/Equipment/Labjack02/Common
//
// The EQ_PERIODIC equipment type is one where "no hardware requirement is 
// necessary to trigger the readout function. Instead, the readout routine 
// associated with this equipment is called periodically.The Period field in 
// the equipment declaration is used in this case to specify the time interval
// between calls to the readout function."

EQUIPMENT equipment[] = {

	{"Labjack02",               // equipment name 
		{1, 0,                     // event ID, trigger mask 
     	"SYSTEM",                 // event buffer 
     	EQ_PERIODIC,              // equipment type (see MIDAS docs)
     	LAM_SOURCE(0, 0xFFFFFF),  // event source crate 0, all stations 
     	"MIDAS",                  // format 
     	TRUE,                     // enabled 
     	RO_ALWAYS,                // read only when running 
     	1000,                     // period: run readout routine every 1000ms
     	0,                        // stop run after this event limit 
     	0,                        // number of sub events 
     	1,                        // don't log history 
     	"", "", "",
    	},
   read_labjack_event,        // readout routine 
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
  	// LJM_dtANY - 'DeviceType' option opens any supported LabJack device type
  	// LJM_ctANY - 'ConnectionType' option for USB connections 
  	// The IP address is specified in the third, 'Identifier' option
  	handle = OpenOrDie(LJM_dtANY, LJM_ctANY, "142.90.151.7");

	// Currently only works for direct USB connection
  	//handle = OpenOrDie(LJM_dtANY, LJM_ctANY, LJM_idANY);   
  
  	PrintDeviceInfoFromHandle(handle);
  	printf("\n");

  	/*
  	int err, iteration, channel;
  	int numSkippedScans = 0;
  	int totalSkippedScans = 0;
  	int deviceScanBacklog = 0;  int LJMScanBacklog = 0;

  	int * aScanList = malloc(sizeof(int) * NumAddresses);

  	unsigned int aDataSize = NumAddresses * ScansPerRead;
  	double * aData = malloc(sizeof(double) * aDataSize); */

  	// Clear aData. This is not strictly necessary, but can help debugging.
  	//memset(aData, 0, sizeof(double) * aDataSize);

  	// streamData is cleared
	memset(streamData, 0, sizeof(double) * streamDataSize);

	// Assigns the Channel_Names to the LabJack addresses
	err = LJM_NamesToAddresses(NumAddresses, CHANNEL_NAMES, aScanList, NULL);
	ErrorCheck(err, "Getting positive channel addresses");

	// Sets the stream configuration, see definition
	HardcodedConfigureStream(handle);

	printf("\nNumber of channels: %d\n", NumAddresses);

	printf("\n");
	printf("Starting stream...\n");

	// Initializes a stream object and begins streaming (data from LabJack). 
	err = LJM_eStreamStart(handle, ScansPerRead, NumAddresses, aScanList,
				 &ScanRate);

	ErrorCheck(err, "LJM_eStreamStart");

	printf("Stream started. Actual scan rate: %.02f Hz (%.02f sample rate)\n",
		 ScanRate, ScanRate * NumAddresses);
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
	// Note: when streaming, negative channels and ranges can be configured 
	// for individual analog inputs, but the stream has only one settling time
	// and resolution.
	printf("    Setting STREAM_RESOLUTION_INDEX to %d\n",\
	 STREAM_RESOLUTION_INDEX);
	WriteNameOrDie(handle, "STREAM_RESOLUTION_INDEX", \
					STREAM_RESOLUTION_INDEX);

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
		if (!test) {

			return 1;
		}

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

	struct timeval te; 
  	gettimeofday(&te, NULL); // get current time

  	// Printing out the time in milliseconds to confirm time between calls to 
  	// read_labjack_evennt function - debugging
   	// long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
   	// printf("milliseconds: %lld\n", milliseconds);

  	/* init bank structure */
  	bk_init32(pevent);
  
  	double *pdata;
  	/* create bank of double words */
  	bk_create(pevent, "LBJK", TID_DOUBLE, (void **)&pdata); 
  	int deviceScanBacklog = 0;
  	int LJMScanBacklog = 0;
  
  	// Arrays are initialized to hold the calculated values for MIDAS.
  	double sum[NumAddresses] = {0};
  	double mean[NumAddresses] = {0};
  	double std[NumAddresses] = {0};
  	int i, j;

	// Call to eStreamRead should read "ScanRate" many values from each address,
	err = LJM_eStreamRead(handle, streamData, &deviceScanBacklog, &LJMScanBacklog);

	// Some error checks are performed
	if(err == 1221){ // ignore errors of type 1221

		static int error_count = 0;
		error_count++;
		cm_msg(MINFO,"read_labjack_event",\
		       "Gotten labjack error with error number = 1221, ", 
		       "Number errors: %i",error_count);

		if(error_count > 100) {

			ErrorCheck(err, "LJM_eStreamRead too many errors");

		}
	  
	}else{

		ErrorCheck(err, "LJM_eStreamRead Cann I add extra info???");
      	}
	

    // The mean and STD of the 10 scans are calculated for each channel.
    for(channel = 0; channel < NumAddresses; channel++) {

		// These array values are initialized to 0.
      	sum[channel] = 0;
      	std[channel] = 0;

      	// The mean is calculated for an individual channel by looping through
		// the 10 samples read for the channel.
      	for(i=0; i<ScanRate; i++){

        	sum[channel] += streamData[channel + NumAddresses*i];
			
			// a print test
			//printf("Presently indexing %d from streamData\n", \
					channel + NumAddresses*i);
			//printf("Channel %d, sample %d value: %f\n", channel, i, \
					streamData[channel + NumAddresses*i]);

      	}

      	mean[channel] = sum[channel] / ScanRate;

      	// The standard deviation is calculating using a similar iteration.
		for(i=0; i<ScanRate; i++){

        	std[channel] += pow(streamData[channel + NumAddresses*i] \
						 - mean[channel], 2);
      	
		}

      	std[channel] = sqrt(std[channel] / ScanRate);
    	
	}

    // Timestamp once per second
    if (j == 0){

      	// deviceScanBackLog is the number of scans left in the device buffer
      	// whereas LJMScanBackLog is the number of scans left in the LabJack
      	// buffer. Recall that a single "scan" refers to a single reading from 
      	// each channel.
      	printf("iteration: %d - deviceScanBacklog: %d, LJMScanBacklog: %d\n",\
	     	   iteration, deviceScanBacklog, LJMScanBacklog);

      	*pdata++ = (double)time(NULL);

    }

    // ASSEMBLE DATA FOR MIDAS
    // time, sample0, sample1, sample2.... sample99
    // sample# = ch0_val, ch0_std, ch1_val, ch1_std... etc.
    for (channel = 0; channel < NumAddresses; channel++) {

      	if (j == 0){

			printf(" %s\t Mean: %f \t Std %f \n", \
					CHANNEL_NAMES[channel], mean[channel], std[channel]);
      
		}

	    *pdata++ = mean[channel];
		*pdata++ = std[channel];
    }
  


	//int size = bk_close(pevent, pdata);
  	bk_close(pevent, pdata);
  	return bk_size(pevent);

}
}




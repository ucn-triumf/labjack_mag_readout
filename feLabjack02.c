
/********************************************************************\
 Labjack readout frontend
 Thomas Lindner (TRIUMF)

The commenting of this file is verbose, as it will continue to be
worked on by novice students working with the TUCAN collaboration.
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
#include <iomanip>
#include <iostream>
#include <fstream>
using namespace std;
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

// Handle for ODB
HNDLE hDB;

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
refers to a single measurement, from a single address. A "scan" refers to a 
reading of one value from each of the addresses. Hence the "Sample Rate" will 
be a factor of NumAddresses larger than the ScanRate. 

The DAQ sequence follows the following steps:

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
		ScansPerRead is transferred from the LJM buffer to the array that 
		is given to eStreamRead as an argument. 
	4) Brief analysis - averaging and standard deviation - is performed on these 
		scans and then they are sent to MIDAS

The ScanRate and ScansPerRead variables are controlled from the MIDAS ODB parameters
online GUI. These are then reset in this program every time the program is restarted.
\********************************************************************/

// These variables are initialized outside the functions so that they may 
// be updated globally. (!!!) Is this necessary?
double ScanRate;
int ScansPerRead;
//int streamDataSize;
// double * streamData;

// (!!!) It is not clear why one would ever want to add more than one data 
// point per MIDAS event. For now, this is removed from the program.

// Data points per MIDAS event for each channel
//int DP_PER_MEVENT = 1;  

// ScansPerRead is set within the MIDAS ODB GUI; the initialization below
// has been commented out.
// This determines the number of scans that are pulled from the LJM buffer
// each call to eStreamRead.
//int ScansPerRead = ScanRate*DP_PER_MEVENT;

// (!!!) It is not clear why this was commented, or what it used to do.
/*
// Channels/Addresses to stream. NumAddresses can be less than or equal to
// the size of CHANNEL_NAMES
enum { NumAddresses = 2 };
const char * CHANNEL_NAMES[] = {"AIN0", "AIN1"};
*/


INT err, iteration, channel;
// (!!!) Are these INT variables necessary anymore?
  //INT numSkippedScans = 0;
  //INT totalSkippedScans = 0;
  //INT deviceScanBacklog = 0;
  //INT LJMScanBacklog = 0; 

// The address list.
INT * aScanList = (INT *) malloc(sizeof(int) * NumAddresses);

// streamData is the array buffer within which LabJack will place data from
// the device, before it is read by eStreamRead. This just needs to be large
// enough to handle the ScanRate as initialized in eStreamStart
INT streamDataSize = NumAddresses * ScansPerRead;
double * streamData = (double *) malloc(sizeof(double) * streamDataSize);

/*-- Function declarations -----------------------------------------*/

// These two functions are executed every time this program is started or 
// stopped. 
INT frontend_init();
INT frontend_exit();

// These functions are executed when MIDAS runs are started, stopped, paused,
// or resumed.
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);

// The Labjack stream configuration is hardcoded within this program. Instead
// of using a standard stream type from Labjack, this just gives the program
// behaviour so custom functionality.
INT HardcodedConfigureStream(INT handle);

// (!!!) It is not clear what this function does or when it is called.
INT frontend_loop();

// This read function runs every MIDAS period, which is defined in the ensuing
// section of code. The routine reads scan data from Labjack and sends it to
// MIDAS.
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

	{"Labjack02",             // equipment name 
		{1, 0,            // event ID, trigger mask 
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
   read_labjack_event,      	// readout routine 
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
	
	// ____________________________________________        
	// ********************************************
	// ODB PARAMETER RETRIEVAL

	// The ODB parameters set in the MIDAS GUI are updated here. This update
	// occurs every time the feLabjack02.exe program is re-executed. Only the
	// ScanRate and ScansPerRead parameters are set in this way.  	

	// (!!!) The device identifier is retrieved first. It is not clear 
	// why this is necessary or what it does.
	int size;
        char device[256];
        size = sizeof(device);
        db_get_value(hDB,0,"/Equipment/Labjack02/Settings/Device",&device,\
			&size,TID_STRING,1);

	// ScanRate is set to an extern variable, such that it will be updated for
	// use globally. Most importantly in the read_labjack_event function which
	// runs every MIDAS period.
	extern double ScanRate; 
	int ScanRate_size;
        ScanRate_size = sizeof(ScanRate);
        db_get_value(hDB,0,"/Equipment/Labjack02/Settings/ScanRate",&ScanRate,\
			&ScanRate_size,TID_DOUBLE,1);
        printf("ScanRate is set to %.2f\n",ScanRate); 	

	// ScansPerRead is treated analogously.
	extern int ScansPerRead;
	int ScansPerRead_size;	
	ScansPerRead_size = sizeof(ScansPerRead);
        db_get_value(hDB,0,"/Equipment/Labjack02/Settings/ScansPerRead",\
			&ScansPerRead,&ScansPerRead_size,TID_INT,1);
        printf("ScansPerRead is set to %d\n",ScansPerRead); 

	// The streamData array is reconfigured to be appropriately sized for
	// the new ScansPerRead value.
	extern INT streamDataSize;
	streamDataSize = NumAddresses * ScansPerRead;
	extern double * streamData;
	streamData = (double *) malloc(sizeof(double) * streamDataSize);
 
	// streamData is cleared.
	memset(streamData, 0, sizeof(double) * streamDataSize); 

	// ____________________________________________        
        // ********************************************
	// STARTING A STREAM

  	// Connect to the labjack
	printf("Connecting to %s...\n",device);
  
  	// Attempts to open the first Labjack found
  	// LJM_dtANY - 'DeviceType' option opens any supported LabJack device type
  	// LJM_ctANY - 'ConnectionType' option for USB connections 
  	// The IP address is specified in the third, 'Identifier' option
  	handle = OpenOrDie(LJM_dtANY, LJM_ctANY, "142.90.151.7");
 	printf("opening 142.90.151.7.\n");
  
	// The Labjack Device information is printed to the console.
  	PrintDeviceInfoFromHandle(handle);
  	printf("\n");

	// (!!!) It is not clear why any of the below * comment encapsulated
	// code is necessary, nor why it is currently commented.
  	/*
  	int err, iteration, channel;
  	int numSkippedScans = 0;
  	int totalSkippedScans = 0;
  	int deviceScanBacklog = 0;  int LJMScanBacklog = 0;

  	int * aScanList = malloc(sizeof(int) * NumAddresses);

  	unsigned int aDataSize = NumAddresses * ScansPerRead;
  	double * aData = malloc(sizeof(double) * aDataSize); 

  	// Clear aData. This is not strictly necessary, but can help debugging.
  	memset(aData, 0, sizeof(double) * aDataSize);
	*/
	
  	//memset(aData, 0, sizeof(double) * aDataSize);

  	// streamData is cleared
	memset(streamData, 0, sizeof(double) * streamDataSize);

	// Assigns the Channel_Names to the LabJack addresses
	err = LJM_NamesToAddresses(NumAddresses, CHANNEL_NAMES, aScanList, NULL);
	printf("Channel names have been assigned to Labjack addresses.\n");
	printf("\nNumber of channels: %d\n", NumAddresses);
	ErrorCheck(err, "Getting positive channel addresses");

	// (!!!) This is a major issue that should be resolved. Sometimes, 
	// depending on how the program exits, the Labjack stream is left 
	// running in the background. Then if one tries to restart the 
	// program, it will throw an error. By simply uncommenting the
	// following line, and thus stopping the stream first, this problem
	// appears to be mitigated. This is a poor, and temporary solution.
	err = LJM_eStreamStop(handle);

	// Sets the stream configuration, see definition
	printf("Configuring the stream...\n");	
	HardcodedConfigureStream(handle);


	// Initializes a stream object and begins streaming (data from LabJack). 
	// A Labjack error check is performed.
	printf("Starting stream...\n");
	// (!!!) This is another major issue to be resolved. At present, the
	// ScansPerRead variable is increased by 5 here to avoid an overflow
	// of the LJMBuffer. Some thought is required in order to see how to 
	// perform this adjustment systematically and automatically.
	err = LJM_eStreamStart(handle, ScansPerRead + 5, NumAddresses, aScanList,
				 &ScanRate);
	ErrorCheck(err, "LJM_eStreamStart");

	// Once the stream is started, some infromation on its rates are
	// printed.
	printf("Stream started. Actual scan rate: %.02f Hz (%.02f sample rate)\n",
		 ScanRate, ScanRate * NumAddresses);

  return SUCCESS;

}


/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
	
	// The stream is stopped.
	printf("Stopping stream...\n");
	//	err = LJM_eStreamStop(handle);
	printf("Stopping stream...\n");
	// Do any disconnecting of the labjack
	printf("Disconnecting from labjack...\n");

	// (!!!) In C, we need to free the memory we previously allocated.
	// Is aData being used? Can we delete that line for good?
	//free(streamData);
	//free(aData);
	//free(aScanList);
	printf("finished free\n");

	// Close Labjack
	CloseOrDie(handle);
	printf("closed connection to labjack\n");

	// (!!!) What's this for?
	//WaitForUserIfWindows();
	printf("finished end_of_run\n");

	return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/
INT begin_of_run(INT run_number, char *error)
{

	return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/
INT end_of_run(INT run_number, char *error)
{

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
	
	// (!!!) In general, this section of the code is quite poorly
	// understood (as demonstrated by the sparse commenting). It 
	// would be good to document it further.

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
	
	// (!!!) What? Where is frontend_call_loop? Is this necessary?

	/* if frontend_call_loop is true, this routine gets called when
	  the frontend is idle or once between every event */
	usleep(50);
	return SUCCESS;
}

/*------------------------------------------------------------------*/

/********************************************************************\

  Readout routines for different events

\********************************************************************/

// (!!!) "Trigger event routines" and "Interrupt configuration" 
// are of what use? These two pieces of code are not understood.

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
} 

/*-- Event readout -------------------------------------------------*/
INT read_labjack_event(char *pevent, INT iter)
{

	// ____________________________________________        
        // ********************************************
	// VARIABLE INITIALIZATION

	// (!!!) Could this time business be removed?
	struct timeval te; 
  	gettimeofday(&te, NULL);

  	// Printing out the time in milliseconds to confirm time between calls to 
  	// read_labjack_evennt function - useful to keep around for debugging
	// calculate milliseconds
   	// long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
   	// printf("milliseconds: %lld\n", milliseconds);

	// (!!!) This bank, and pdata piece is unclear, could be better 
	// explained or documented.
  	/* init bank structure */
  	bk_init32(pevent);
  	double *pdata;
  	/* create bank of double words */
  	bk_create(pevent, "LBJK", TID_DOUBLE, (void **)&pdata); 

	// These backlog variables are for keeping track of how many scans are
	// left over in each of the deviceBuffer and the LJMBuffer, after each
	// read.
  	int deviceScanBacklog = 0;
  	int LJMScanBacklog = 0;
  
  	// Arrays are initialized to hold the calculated values for MIDAS.
  	double sum[NumAddresses] = {0};
  	double mean[NumAddresses] = {0};
  	double std[NumAddresses] = {0};
  	int i;

	// Call to eStreamRead should read "ScanRate" many values from each address,
	err = LJM_eStreamRead(handle, streamData, &deviceScanBacklog, &LJMScanBacklog);

	// Can be useful for testing:
	// A loop to check the the individual voltage measurements, 
	// i.e. the samples from individual channels.
	//std::cout.precision(15);
	//for(j = 0; j < ScanRate * NumAddresses; j++) {
	//	std::cout << std::fixed << streamData[j] << std::endl;	
	//}

	// Some error checks are performed
	if(err == 1221){ // ignore errors of type 1221

		static int error_count = 0;
		error_count++;
		cm_msg(MINFO,"read_labjack_event",
		       "Gotten labjack error with error number = 1221, ",
		       "Number errors: %i",error_count);

		if(error_count > 100) {

			ErrorCheck(err, "LJM_eStreamRead too many errors");

		}
	  
	}else{

		ErrorCheck(err, "LJM_eStreamRead Can I add extra info???");
      	}
	

	// The mean and STD of the 10 scans are calculated for each channel.
	for(channel = 0; channel < NumAddresses; channel++) {

		// These array values are initialized to 0.
	      	sum[channel] = 0;
	      	std[channel] = 0;

	      	// The mean is calculated for an individual channel by looping through
		// the 10 samples read for the channel.
	      	for(i=0; i<ScansPerRead; i++){

			sum[channel] += streamData[channel + NumAddresses*i];
				
			// a print test useful for debugging
			//printf("Presently indexing %d from streamData\n", 
			//		channel + NumAddresses*i);
			//printf("Channel %d, sample %d value: %f\n", channel, i, 
			//		streamData[channel + NumAddresses*i]);

      		}

		// (!!!) This could be where the ~-e05 values are coming from
		// If somehow ScansPerRead is made arbitrarily small, say 0
		// somehow for one data point, then the mean calculation could
		// yield the results were seeing. Should be investigated. 
      		mean[channel] = sum[channel] / ScansPerRead;

      		// The standard deviation is calculating using a similar iteration.
		for(i=0; i<ScansPerRead; i++){

        		std[channel] += pow(streamData[channel + NumAddresses*i] \
						 - mean[channel], 2);
      	
		}
		
      		std[channel] = sqrt(std[channel] / ScansPerRead);
    	
	}

	// (!!!) This is the ideal place for looking for a channel swap. 
	// The feLabjack02_Jul4_backup.c file contains a rather rushed 
	// skeleton of what should be implemented for this.

      	// deviceScanBackLog is the number of scans left in the device buffer
      	// whereas LJMScanBackLog is the number of scans left in the LabJack
      	// buffer. Recall that a single "scan" refers to a single reading from 
      	// each channel.
      	printf("iteration: %d - deviceScanBacklog: %d, LJMScanBacklog: %d\n",\
	     	   iteration, deviceScanBacklog, LJMScanBacklog);

	// (!!!) What does this do?
      	*pdata++ = (double)time(NULL);


	// ASSEMBLE DATA FOR MIDAS
	// time, sample0, sample1, sample2.... sample99
	// sample# = ch0_val, ch0_std, ch1_val, ch1_std... etc.
	for (channel = 0; channel < NumAddresses; channel++) {


		printf(" %s\t Mean: %f \t Std %f \n", \
			CHANNEL_NAMES[channel], mean[channel], std[channel]);

		// (!!!) why?
		*pdata++ = mean[channel];
		*pdata++ = std[channel];

	}

	// (!!!) What's happening here?
	//int size = bk_close(pevent, pdata);
	bk_close(pevent, pdata);
	return bk_size(pevent);

}
}




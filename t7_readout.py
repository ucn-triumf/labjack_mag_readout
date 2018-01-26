#!/usr/bin/python

#Reads AIN channels on T7 LabJack with a MUX80 multiplexer.
#The AIN channels are sampled in differential mode.

#Labjack input channels have 720 Hz 2nd order AA filter (RC filter with 475k + 470nF)


from labjack import ljm
from time import sleep
import time
import logging
import sys
from datetime import datetime

MAX_REQUESTS = 50 # The number of eStreamRead calls that will be performed.

# Open first found T7 LabJack
# handle = ljm.openS("T7", "USB", "ANY")
handle = ljm.openS("T7", "USB", "ANY")

info = ljm.getHandleInfo(handle)
print("Opened a LabJack with Device type: %i, Connection type: %i,\n" \
   	"Serial number: %i, IP address: %s, Port: %i,\nMax bytes per MB: %i" % \
    	(info[0], info[1], info[2], ljm.numberToIP(info[3]), info[4], info[5]))

# Setup and call eWriteNames to configure AINs on the LabJack.
numChannels = 32
numFrames = numChannels*3
# Generate lists with channel number pairings which
# match the MUX80 data sheet for Differential readings
pChan = range(48,56)+range(64,72)+range(80,88)+range(96,104)
nChan = range(56,64)+range(72,80)+range(88,96)+range(104,112)

#List of analog input names
ChanNames = []
for p in pChan:
	ChanNames = ChanNames+["AIN%d"%p]

# Stream Configuration
numAddresses = len(ChanNames)
aScanList = ljm.namesToAddresses(numAddresses, ChanNames)[0]
scanRate = 1500 #Value must be less than 100,000/(32*2) for 32 differential channels
scansPerRead = int(scanRate/2)

#Set up the Analog input channels
names = []
aValues = []
for p in pChan:
	names = names+["AIN%d_NEGATIVE_CH"%p, "AIN%d_RANGE"%p, "STREAM_SETTLING_US",
              "STREAM_RESOLUTION_INDEX"]
for n in nChan:
	aValues = aValues+[n, 10, 0, 0]  #+/-10V range with default resolution
ljm.eWriteNames(handle, numFrames, names, aValues)

try:
    # Configure and start stream
    scanRate = ljm.eStreamStart(handle, scansPerRead, numAddresses, aScanList, scanRate)
    print("\nStream started with a scan rate of %0.0f Hz." % scanRate)

    print("\nPerforming %i stream reads." % MAX_REQUESTS)
    start = datetime.now()
    totScans = 0
    totSkip = 0 # Total skipped samples

    i = 1
    while i <= MAX_REQUESTS:
        ret = ljm.eStreamRead(handle)
        
        data = ret[0]
        scans = len(data)/numAddresses
        totScans += scans
        
        # Count the skipped samples which are indicated by -9999 values. Missed
        # samples occur after a device's stream buffer overflows and are
        # reported after auto-recover mode ends.
        curSkip = data.count(-9999.0)
        totSkip += curSkip
        
        print("\neStreamRead %i" % i)
        ainStr = ""
        for j in range(0, numAddresses):
	        ainStr += "%s = %0.5f " % (ChanNames[j], data[j])
        print("  1st scan out of %i: %s" % (scans, ainStr))
        print("  Scans Skipped = %0.0f, Scan Backlogs: Device = %i, LJM = " \
              "%i" % (curSkip/numAddresses, ret[1], ret[2]))
        i += 1

    end = datetime.now()

    print("\nTotal scans = %i" % (totScans))
    tt = (end-start).seconds + float((end-start).microseconds)/1000000
    print("Time taken = %f seconds" % (tt))
    print("LJM Scan Rate = %f scans/second" % (scanRate))
    print("Timed Scan Rate = %f scans/second" % (totScans/tt))
    print("Timed Sample Rate = %f samples/second" % (totScans*numAddresses/tt))
    print("Skipped scans = %0.0f" % (totSkip/numAddresses))
except ljm.LJMError:
    ljme = sys.exc_info()[1]
    print(ljme)
except Exception:
    e = sys.exc_info()[1]
    print(e)

print("\nStop Stream")
ljm.eStreamStop(handle)

# Close handle
ljm.close(handle)

# Labjack magnetometry readout

Code for reading the black 10-channel fluxgate DAQ box over ethernet. 

## Notes

C code is primarily for talking to MIDAS, as a part of the official DAQ. 

Python code is for reading directly to a device on request.

## IP Addresses

The labjack IP address should be reconfigured depending on the network: 

* TRIUMF VLAN1: `142.90.100.26` (aka `ucnlabjack03.triumf.ca`)
* UCN VLAN: `142.90.151.7`

One can do this via the [`labjack-kipling`](https://labjack.com/pages/support/?doc=/software-driver/labjack-applications/kipling/) program over the UCN VLAN or direct USB connection. The code will have to be edited and (possibly) recompiled should the IP address change. 

---

## LabJackT7

### Installation

1. Clone this repository
2. Install with local pip: `python3 -m pip install --user -e path/labjack_mag_readout`
3. Install labjack manager code: https://labjack.com/support/software/installers/ljm

### API Documentation 

Documentation generated with [pydoc3](https://pypi.org/project/pdoc3/)

__Functions__

    
`from_csv(filename)`
:   read data from csv file
    returns LabJackT7 object

__Class Definition__

`LabJackT7(channel_list=None)`
:   LabJack class for reading T7 output
    
    ATTRIBUTES
    
    channel_ids     list of strings, formated as CH1x, human-readable corresponding to channel_names
    channel_names   list of strings, selections from CHANNEL_NAMES to read from
    CONNECTION_TYPE string, type of connection
    data            list of pd.DataFrames containing all data. Each list element is a separate stream. Data is in volts.
    DEVICE_TYPE     string, type of device to open
    IP              string, IP address of device
    lj_handle       handle address for labjack object
    MAX_SAMPLE_RATE float, max rate, specific to device type
    n_addresses     length of channel_names
    
    scan_list       list of addresses for each name      
    scan_rate       float, scan rate of stream read
    stream_times    list of strings, start times of each stream in self.data
    
    Initialize object
    
    channel_list: list of numerical channels which one wants to read from. Ex: [1, 2, 6]

__Class variables__

* `CHANNEL_NAMES`
* `CONNECTION_TYPE`
* `DEVICE_TYPE`
* `IP`
* `MAX_SAMPLE_RATE`
* `STREAM_SETTINGS`

__Methods__

`connect(self)`
:   Connect to the T7 LabJack

`disconnect(self)`
:   Stop connection

`draw(self, scan_rate=1500, scan_duration=1)`
:   Draw the result in realtime
    
    scan rate in hz
    scan duration in s

`get_data(self)`
:

`get_stream_times(self)`
:

`read(self, scan_rate=1500, scan_length=100, nreads=1, save=True)`
:   Read data from labjack and append to internal data structures: self.data and self.stream_times 
    scan_rate:      Hz, must be less than 100000/(n_addreses*2)
    scan_length:    number of measurements in each scan
    nreads:         number of stream reads to conduct before closing stream

`reset(self)`
:   Erase internal data lists

`to_csv(self, filename, idx=-2)`
:   Write data to csv
    
    filename: name of file to write to
    if idx < -1, then write all streams to file, else write stream of that index to file

# Reads AIN channels on T7 LabJack with a MUX80 multIPlexer.
# The AIN channels are sampled in differential mode.
# Labjack input channels have 720 Hz 2nd order AA filter (RC filter with 475k + 470nF)
#
# Updated by Derek Fujimoto
# July 2022

"""
    Some install notes
    
    pIP install --user labjack-ljm
    also install the base code from here: https://labjack.com/support/software/installers/ljm
    
    
    For maximum steam rates see https://labjack.com/support/datasheets/t-series/appendix-a-1
"""

from labjack import ljm
from datetime import datetime
import numpy as np
import pandas as pd
from tqdm import tqdm
from io import StringIO

DEBUG = False   # if True, print debugging statements

class LabJackT7(object):
    """
        LabJack class for reading T7 output
        
        ATTRIBUTES
        
        channel_ids     list of strings, formated as CH1x, human-readable corresponding to channel_names
        channel_names   list of strings, selections from CHANNEL_NAMES to read from
        CONNECTION_TYPE string, type of connection
        data            list of pd.DataFrames containing all data. Each list element is a separate stream
        DEVICE_TYPE     string, type of device to open
        IP              string, IP address of device
        lj_handle       handle address for labjack object
        MAX_SAMPLE_RATE float, max rate, specific to device type
        n_addresses     length of channel_names
        
        scan_list       list of addresses for each name      
        scan_rate       float, scan rate of stream read
        stream_times    list of strings, start times of each stream in self.data
    """
    
    # device settings
    DEVICE_TYPE = 'T7'
    CONNECTION_TYPE = 'ETHERNET'
    IP = "142.90.151.7"         # IP address of device
    MAX_SAMPLE_RATE = 1e5       # for T7
    
    # stream settings from feLabjack02
    STREAM_SETTINGS = { 'STREAM_TRIGGER_INDEX':     0,                  # Controls when stream scanning will start. 0 = Start when stream is enabled
                        'STREAM_CLOCK_SOURCE':      0,                  # Controls which clock source will be used to run the main stream clock. 0 = Internal crystal, 2 = External clock source on CIO3.
                        'STREAM_RESOLUTION_INDEX':  0,                  # The resolution index for stream readings. A larger resolution index generally results in lower noise and longer sample times
                        'STREAM_SETTLING_US':       0,                  # Time in microseconds to allow signals to settle after switching the mux. Does not apply to the 1st channel in the scan list, as that settling is controlled by scan rate (the time from the last channel until the start of the next scan). Default=0. When set to less than 1, automatic settling will be used. 
                        'AIN_ALL_RANGE':            0,                  # A write to this global parameter affects all AIN. A read will return the correct setting if all channels are set the same, but otherwise will return -9999. 
                        'AIN_ALL_NEGATIVE_CH':      ljm.constants.GND,  #  write to this global parameter affects all AIN. Writing 1 will set all AINs to differential. Writing 199 (GND) will set all AINs to single-ended. A read will return 1 if all AINs are set to differential and 199 if all AINs are set to single-ended. If AIN configurations are not consistent 0xFFFF will be returned.
                      }
                      
    # all possible channels  x        y        z
    CHANNEL_NAMES = [	    "AIN72", "AIN74", "AIN76",  # CH1
                            "AIN73", "AIN75", "AIN77",  # CH2
                            "AIN78", "AIN80", "AIN82",  # CH3
                            "AIN79", "AIN81", "AIN83",  # CH4
                            "AIN96", "AIN98", "AIN100", # CH5
                            "AIN99", "AIN101","AIN103", # CH6
                            "AIN102","AIN104","AIN106", # CH7
                            "AIN107","AIN109","AIN110", # CH8
                            "AIN108","AIN111","AIN113"  # CH9
                    ]

    def __init__(self, channel_list=None): 
        """
            Initialize object
            
            channel_list: list of numerical channels which one wants to read from. Ex: [1, 2, 6]
        """
        
        # set up channel names and ids
        self.channel_names = [self.CHANNEL_NAMES[(i-1)*3:i*3] for i in channel_list]
        self.channel_names = np.concatenate(self.channel_names).tolist()
        self.channel_ids = []
        for ch in self.channel_names:
            i = self.CHANNEL_NAMES.index(ch)
            xyz = 'xyz'[i%3] 
            idn = i//3+1
            self.channel_ids.append(f'CH{idn}{xyz}')
        
        # get ready for stream configuration
        self.n_addresses = len(self.channel_names)
        self.scan_list = ljm.namesToAddresses(self.n_addresses, self.channel_names)[0]        
        self.max_scan_rate = self.MAX_SAMPLE_RATE/self.n_addresses
        
        # initilize results
        self.data = []
        self.stream_times = []

    def connect(self):
        """
            Connect to the T7 LabJack
        """
        
        # open connection
        self.lj_handle = ljm.openS(self.DEVICE_TYPE, self.CONNECTION_TYPE, self.IP)
        device, connection, serial, IP, port, bytesperMB = ljm.getHandleInfo(self.lj_handle)
        
        # set stream settings
        for key, value in self.STREAM_SETTINGS.items():
            ljm.eWriteName(self.lj_handle, key, value)
        
        if DEBUG:
            print_lines = ( f"Opened a LabJack",
                            f"Device type: {device}",
                            f"Connection type: {connection}",
                            f"Serial number: {serial}",
                            f"IP address: {ljm.numberToIP(IP)}",
                            f"Port: {port}",
                            f"Max bytes per MB: {bytesperMB}"
                            )
            print('\n  '.join(print_lines))
    
    def disconnect(self):
        """
            Stop connection
        """        
        ljm.close(self.lj_handle)
        
    def get_data(self):         return self.data
    def get_stream_times(self): return self.stream_times
    
    def read(self, scan_rate=1500, scan_length=100, nreads=1):
        """
            Read data from labjack and append to internal data structures: self.data and self.stream_times 
            scan_rate:      Hz, must be less than 100000/(n_addreses*2)
            scan_length:    number of measurements in each scan
            nreads:         number of stream reads to conduct before closing stream
        """
        
        # check input
        if scan_rate > self.max_scan_rate:
            raise RuntimeError(f"scan_rate ({scan_rate}) exceeds max_scan_rate ({self.max_scan_rate}).")
        
        # configure and start stream
        try:
            scan_rate = ljm.eStreamStart(self.lj_handle, 
                                         scan_length, 
                                         self.n_addresses, 
                                         self.scan_list, 
                                         scan_rate)
        except ljm.LJMError as err:
            if '1224' in str(err):
                self.connect()
                return self.read(scan_rate, scan_length, nreads)
            else:
                raise err
        except AttributeError:
            self.connect()
            return self.read(scan_rate, scan_length, nreads)
            
        if DEBUG:
            print('\nStream started. \n\nRequested settings:')
            print(f'  scan rate:        {scan_rate} Hz')
            print(f'  scan length:      {scan_length} samples/scan')
            print(f'  number of scans:  {nreads}')
            print("\nPerforming %i stream reads." % nreads)
            print('')
            
        all_data = {}
        for i in tqdm(range(1, nreads+1), desc='Stream reads', leave=DEBUG):
                       
            # read data
            start_date = datetime.now()
            start = ljm.getHostTick()
            ret = ljm.eStreamRead(self.lj_handle)
            end = ljm.getHostTick()

            data = ret[0]            
            
            if DEBUG:
                
                print(f'\nScan {i}')
                
                # get scan rate
                t = (end-start)/1e6
                scan_rate_measured = len(data)/t/self.n_addresses
                scans = len(data)/self.n_addresses
                        
                # Count the skIPped samples which are indicated by -9999 values. Missed
                # samples occur after a device's stream buffer overflows and are
                # reported after auto-recover mode ends.
                current_skipped = data.count(-9999.0)
                
                # print summary
                print(f'  measured scan rate:   {scan_rate_measured} Hz')
                print(f'  scan duration:        {t} s')
                print(f'  scans skipped:        {current_skipped/self.n_addresses}')
                print(f'  scan backlogs:        Device ({ret[1]}), LJM ({ret[2]})')
                print(f'  start:                {start_date}')
            
            all_data[str(start_date)] = data
            
        ljm.eStreamStop(self.lj_handle)
        
        if DEBUG:
            print('\nStream stopped')

        # process data: split into arrays, save as data frame
        index = np.arange(0, scan_length/scan_rate, 1/scan_rate)
        df_all = []
        times_all = []
        for date, data in all_data.items():
            df = {ch: data[i::self.n_addresses] for i, ch in enumerate(self.channel_names)}
            df = pd.DataFrame(df, index=index)
            df.rename(columns={n:i for n, i in zip(self.channel_names, self.channel_ids)}, inplace=True)
            df.index.name = 'dt (s)'
            
            df_all.append(df)
            times_all.append(date)
        
        self.data.extend(df_all)
        self.stream_times.extend(times_all)
        self.scan_rate = scan_rate
        return (times_all, df_all)

    def reset(self):
        """
            Erase internal data lists
        """
        self.data = []
        self.stream_times = []
        
    def to_csv(self, filename, idx=-1):
        """
            Write data to csv
            
            filename: name of file to write to
            if idx < 0, then write all streams to file, else write stream of that index to file
        """
        
        if len(self.data) == 1:
            idx = 0
        
        # check that data exists
        if len(self.data) == 0:
            raise RuntimeError('No data saved')
        
        # write file header
        if idx < 0: msg = 'multiple reads of stream'
        else:       msg = 'single read of stream'
            
        header = [  f'# labjack output {msg}',
                    '# Settings:',
                    f'#   DEVICE_TYPE:              {self.DEVICE_TYPE}',
                    f'#   CONNECTION_TYPE:          {self.CONNECTION_TYPE}',
                    f'#   IP:                       {self.IP}',
                 ]
        header.extend([f'#   {key}:' + ' '*(25-len(key)) + f'{val}' 
                       for key, val in self.STREAM_SETTINGS.items()])
        
        header.extend([ f'# Scan rate of last read:    {self.scan_rate} Hz',
                        '#',
                        f'# File written at {datetime.now()}',
                        '# \n'
                      ])
        
        with open(filename, 'w') as fid:
            fid.write('\n'.join(header))
        
        # write a single stream
        if idx >= 0:
            with open(filename, 'a+') as fid:
                fid.write(f'# START stream {self.stream_times[idx]}\n#\n')
            self.data[idx].to_csv(filename, mode='a+')
            
        # write a set of streams
        else:
            for dat, start in zip(self.data, self.stream_times):
                with open(filename, 'a+') as fid:
                    fid.write(f'START stream {start}\n')
                dat.to_csv(filename, mode='a+')
        
def from_csv(filename):
    """
        read data from csv file
        returns LabJackT7 object
    """
    
    # read the header
    with open(filename, 'r') as fid: 
        header = [fid.readline() for i in range(100)]
        header = [line for line in header if line[0] == '#']
        
        # get number of channels
        fid.seek(0)
        line = fid.readline()
        while True:
            line = fid.readline()
            if 'START stream' in line:
                line = fid.readline()
                line = line.split(',')[1:]
                line = [int(element[2]) for element in line]
                ch = np.unique(line).tolist()
                break
        
    # initialize return object
    lj = LabJackT7(ch)
    
    # check file type: single or multiple streams
    is_single = 'single' in header[0]

    # set header settings
    for line in header:
        if 'DEVICE_TYPE' in line:           lj.DEVICE_TYPE = line.split(':')[1].strip()
        elif 'CONNECTION_TYPE' in line:     lj.CONNECTION_TYPE = line.split(':')[1].strip()
        elif 'IP' in line:                  lj.IP = line.split(':')[1].strip()
        elif 'STREAM' in line or 'AIN_ALL' in line:
            line = line[1:]
            key, val = line.split(':')
            key = key.strip()
            val = int(val.strip())
            lj.STREAM_SETTINGS[key] = val
        elif 'Scan rate' in line:           
            lj.scan_rate = float(line.split(':')[1].strip().split()[0].strip())
        elif 'START stream' in line:
            lj.steam_times = [line.split('stream').strip()]

    # read single stream
    if is_single:
        dat = pd.read_csv(filename, comment='#')
        dat.set_index('dt (s)', inplace=True)
        lj.data = [dat]
        
    else:
        with open(filename, 'r') as fid:
            contents = fid.read()
        contents = contents.split('START stream ')[1:]
        
        for cont in contents:
            timestamp, rest = cont.split('\n', 1)
            
            # convert data to dataframe
            df = pd.read_csv(StringIO(rest))
            df.set_index('dt (s)', inplace=True)
            
            # save
            lj.stream_times.append(timestamp)
            lj.data.append(df)
            
    return lj

// Example Program for converting MIDAS format to ROOT format.
//
// T. Lindner (Jan 2016) 
//
// Example is for the CAEN V792 ADC module

#include <stdio.h>
#include <iostream>
#include <time.h>
#include <vector>

#include "TRootanaEventLoop.hxx"
#include "TFile.h"
#include "TTree.h"

#include "TAnaManager.hxx"

#ifdef USE_V792
#include "TV792Data.hxx"
#endif

class Analyzer: public TRootanaEventLoop {

public:

  // An analysis manager.  Define and fill histograms in 
  // analysis manager.
  TAnaManager *anaManager;

  // The tree to fill.
  TTree *fTree;

#ifdef USE_V792
  // CAEN V792 tree variables
  int nchannels;
  int adc_value[32];
#endif


  Analyzer() {

  };

  virtual ~Analyzer() {};

  void Initialize(){


  }
  
  
  void BeginRun(int transition,int run,int time){
    
    // Create a TTree
    fTree = new TTree("midas_data","MIDAS data");

#ifdef USE_V792    
    fTree->Branch("nchannels",&nchannels,"nchannels/I");
    fTree->Branch("adc_value",adc_value,"adc_value[nchannels]/I");
#endif

  }   


  void EndRun(int transition,int run,int time){
        printf("\n");
  }

  
  
  // Main work here; create ttree events for every sequenced event in 
  // Lecroy data packets.
  bool ProcessMidasEvent(TDataContainer& dataContainer){

    int id = dataContainer.GetMidasEvent().GetSerialNumber();
    if(id%10 == 0) printf(".");


#ifdef USE_V792    
    TV792Data *data = dataContainer.GetEventData<TV792Data>("ADC0");
    if(data){
      nchannels = 32;
      for(int i = 0; i < nchannels;i++) adc_value[i] = 0;
      
      /// Get the Vector of ADC Measurements.
      std::vector<VADCMeasurement> measurements = data->GetMeasurements();
      for(unsigned int i = 0; i < measurements.size(); i++){ // loop over measurements
        
        int chan = measurements[i].GetChannel();
        uint32_t adc = measurements[i].GetMeasurement();
        
        if(chan >= 0 && chan < nchannels)
          adc_value[chan] = adc;
      }
    }
#endif

    fTree->Fill();

    return true;

  };
  
  // Complicated method to set correct filename when dealing with subruns.
  std::string SetFullOutputFileName(int run, std::string midasFilename)
  {
    char buff[128]; 
    Int_t in_num = 0, part = 0;
    Int_t num[2] = { 0, 0 }; // run and subrun values
    // get run/subrun numbers from file name
    for (int i=0; ; ++i) {
      char ch = midasFilename[i];
        if (!ch) break;
        if (ch == '/') {
          // skip numbers in the directory name
          num[0] = num[1] = in_num = part = 0;
        } else if (ch >= '0' && ch <= '9' && part < 2) {
          num[part] = num[part] * 10 + (ch - '0');
          in_num = 1;
        } else if (in_num) {
          in_num = 0;
          ++part;
        }
    }
    if (part == 2) {
      if (run != num[0]) {
        std::cerr << "File name run number (" << num[0]
                  << ") disagrees with MIDAS run (" << run << ")" << std::endl;
        exit(1);
      }
      sprintf(buff,"output_%.6d_%.4d.root", run, num[1]);
      printf("Using filename %s\n",buff);
    } else {
      sprintf(buff,"output_%.6d.root", run);
    }
    return std::string(buff);
  };





}; 


int main(int argc, char *argv[])
{

  Analyzer::CreateSingleton<Analyzer>();
  return Analyzer::Get().ExecuteLoop(argc, argv);

}


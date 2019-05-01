// Default program for dealing with various standard TRIUMF VME setups:
// V792, V1190 (VME), L2249 (CAMAC), Agilent current meter
//
//

#include <stdio.h>
#include <iostream>
#include <time.h>

#include "TRootanaEventLoop.hxx"
//#include "TAnaManager.hxx"
#include <iostream>
#include <fstream>


class Analyzer: public TRootanaEventLoop {

  
  ofstream myfile;

public:

  
  Analyzer() {
    UseBatchMode();
    
  };

  virtual ~Analyzer() {};

  void Initialize(){
    myfile.open ("run01242.txt");
  }

  void InitManager(){
    
    
  }
  
  
  void BeginRun(int transition,int run,int time){
        
  }

  void EndRun(int transition,int run,int time){
    
    myfile.close();
  }


  bool ProcessMidasEvent(TDataContainer& dataContainer){

    // Use the sequence bank to see when a new run starts:
    TGenericData *data = dataContainer.GetEventData<TGenericData>("LBJK");
    
    if(!data) return false;
    
    // Save the unix timestamp
    int timestamp = dataContainer.GetMidasData().GetTimeStamp();
    
    myfile << timestamp;
    int channels = 15;
    int reads_per_sec = 30;   // Hz
    //for(int i = 0; i < 7; i++){   // For 3 channels every 1 second
    for(int i = 0; i < (1+reads_per_sec*channels*2); i++){   //1 time value, 2 values per channel (val and std), 3 channels, 20 values per second
      double data2 =  data->GetDouble()[i];
      myfile << ", " << data2;
    }
    myfile << std::endl;
    


    return true;
  }


}; 


int main(int argc, char *argv[])
{

  Analyzer::CreateSingleton<Analyzer>();
  return Analyzer::Get().ExecuteLoop(argc, argv);

}


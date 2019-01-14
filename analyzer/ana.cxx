// Default program for dealing with various standard TRIUMF VME setups:
// V792, V1190 (VME), L2249 (CAMAC), Agilent current meter
//
//

#include <stdio.h>
#include <iostream>
#include <time.h>

#include "TRootanaEventLoop.hxx"
#include "TAnaManager.hxx"
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
    myfile.open ("run01208.txt");
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
    for(int i = 0; i < 15; i++){
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


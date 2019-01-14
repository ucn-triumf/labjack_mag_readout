#include "TAgilentHistogram.h"

#include "TDirectory.h"

const int Nchannels = 5;

/// Reset the histograms for this canvas
TAgilentHistograms::TAgilentHistograms(){  
  
  CreateHistograms();
}

/// Create Histograms and Fill
void TAgilentHistograms::CreateHistograms(){
  
  // Otherwise make histograms
  clear();

  std::cout << "Create Agilent Histos" << std::endl;
  for(int i = 0; i < Nchannels; i++){ // loop over channels    

    char name[100];
    char title[100];
    sprintf(name,"Agilent_%i_%i",0,i);

    // Delete old histograms, if we already have them
    TH1D *old = (TH1D*)gDirectory->Get(name);
    if (old){
      delete old;
    }

    // Create new histograms
    sprintf(title,"Agilent histogram for channel=%i",i);
    
    TH1D *tmp = new TH1D(name,title,10000,0,10);
    tmp->SetXTitle("Current (uA)");
    push_back(tmp);
   }

}


 #include <sys/time.h>

  
/// Update the histograms for this canvas.
void TAgilentHistograms::UpdateHistograms(TDataContainer& dataContainer){



    TGenericData *bert = dataContainer.GetEventData<TGenericData>("BERT");
    if(bert ){ 
      for(int i = 0; i < Nchannels; i++){
        GetHistogram(i)->Fill(100*((double*)bert->GetData64())[i]);

      }
    }
}



/// Take actions at begin run
void TAgilentHistograms::BeginRun(int transition,int run,int time){

  CreateHistograms();
}

/// Take actions at end run  
void TAgilentHistograms::EndRun(int transition,int run,int time){
  printf("in end run for TAgilent\n");
}

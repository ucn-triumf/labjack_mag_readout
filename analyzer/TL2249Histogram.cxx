#include "TL2249Histogram.h"
#include "TL2249Data.hxx"
#include "TDirectory.h"
#include "TTree.h"

const int Nchannels = 4;

/// Reset the histograms for this canvas
TL2249Histograms::TL2249Histograms(){  
  
  CreateHistograms();

}

/// Create Histograms and set labels and range
void TL2249Histograms::CreateHistograms(){
  
  // Otherwise make histograms
  clear();
  
  std::cout << "Create Histos" << std::endl;
  for(int i = 0; i < Nchannels; i++){ // loop over channels    

    char name[100];
    char title[100];
    sprintf(name,"L2249_%i_%i",0,i);

    // Delete old histograms, if we already have them
    TH1D *old = (TH1D*)gDirectory->Get(name);
    if (old){
      delete old;
    }

    // Create new histograms
    sprintf(title,"L2249 histogram for channel=%i",i);	
    
    TH1D *tmp = new TH1D(name,title,2048,0,2047);
    tmp->SetXTitle("ADC");
    tmp->SetYTitle("Number of Entries");
    push_back(tmp);
  }

}


/// Update the histograms for this canvas.
void TL2249Histograms::UpdateHistograms(TDataContainer& dataContainer){


  TL2249Data *data = dataContainer.GetEventData<TL2249Data>("ADC0");
  if(!data) return;
  
  /// Get the Vector of ADC Measurements.
  std::vector<LADCMeasurement> measurements = data->GetMeasurements();
  for(unsigned int i = 0; i < measurements.size(); i++){ // loop over measurements
	
    int chan = i;
    GetHistogram(chan)->Fill(measurements[i].GetMeasurement());
  }
}



/// Take actions at begin run
void TL2249Histograms::BeginRun(int transition,int run,int time){

  printf("in begin run for TL2249\n");  
  CreateHistograms();

}

/// Take actions at end run  
void TL2249Histograms::EndRun(int transition,int run,int time){
  printf("in end run for TL2249\n");
}

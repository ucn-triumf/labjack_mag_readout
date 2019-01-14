#include "TV1720Correlations.h"

#include "TV1720RawData.h"
#include "TDirectory.h"

#include "TInterestingEventManager.hxx"



void TV1720Correlations::CreateHistograms(){

  // check if we already have histogramss.
  char tname[100];
  sprintf(tname,"V1720_Correlations_%i",0);

  TH2D *tmp = (TH2D*)gDirectory->Get(tname);
  if (tmp) return;
  
  // Otherwise make histograms
  clear();

  for(int i = 0; i < 8; i++){ // loop over 8 channels
    
    char name[100];
    char title[100];
    sprintf(name,"V1720_Correlations_%i",i);
    
    sprintf(title,"V1720 Max ADC vs Max ADC time ch=%i",i);	
    
    TH2D *tmp = new TH2D(name, title, 100,0,2000,100,0,1000);
    //TH2D *tmp = new TH2D(name, title, 4,0,2000,5,0,1000);
    tmp->SetXTitle("max ADC time (ns)");
    tmp->SetYTitle("max bin value");
    
    push_back(tmp);
  }

}


void TV1720Correlations::UpdateHistograms(TDataContainer& dataContainer){

  TV1720RawData *v1720 = dataContainer.GetEventData<TV1720RawData>("W200");
  
  if(v1720 && v1720->IsZLECompressed()){      

    // NOTHING
    // can't handle compressed data yet...
  }
	
  if(v1720 && !v1720->IsZLECompressed()){      
    
    for(int i = 0; i < 8; i++){ // loop over channels

      TV1720RawChannel channelData = v1720->GetChannelData(i);
      if(channelData.GetNSamples() <= 0) continue;

      double max_adc_value = -1.0;
      double max_adc_time = -1;
      
      
      for(int j = 0; j < channelData.GetNSamples(); j++){
	double adc = channelData.GetADCSample(j);
	if(adc > max_adc_value){
	  max_adc_value = adc;
	  max_adc_time = j * 4.0; // 4ns per bin
	}
	
      }

      GetHistogram(i)->Fill(max_adc_time,max_adc_value);

      // As test, set any event where time for max bin < 200 as 'interesting'
      if(max_adc_time < 400) iem_t::instance()->SetInteresting();

    }
  }
  

}



void TV1720Correlations::Reset(){


  for(int i = 0; i < 8; i++){ // loop over channels
    GetHistogram(i)->Reset();
    
  }
}

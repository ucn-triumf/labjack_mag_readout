#include "TDT724Waveform.h"

#include "TDT724RawData.hxx"
#include "TDirectory.h"


/// Reset the histograms for this canvas
TDT724Waveform::TDT724Waveform(){

  SetNanosecsPerSample(10); //ADC clock runs at 100Mhz on the 724 = units of 10 nsecs
	
	CreateHistograms();

}


void TDT724Waveform::CreateHistograms(){

  // check if we already have histogramss.
  char tname[100];
  sprintf(tname,"DT724_%i",0);

  TH1D *tmp = (TH1D*)gDirectory->Get(tname);
  if (tmp) return;

	int fWFLength = 2000; // Need a better way of detecting this...
  int numSamples = fWFLength / nanosecsPerSample;

  // Otherwise make histograms
  clear();

	for(int i = 0; i < 2; i++){ // loop over 2 channels
		
		char name[100];
		char title[100];
		sprintf(name,"DT724_%i",i);

		sprintf(title,"DT724 Waveform for channel=%i",i);	
		
		TH1D *tmp = new TH1D(name, title, numSamples, 0, fWFLength);
		tmp->SetXTitle("ns");
		tmp->SetYTitle("ADC value");
		
		push_back(tmp);
	}
	std::cout << "TDT724Waveform done init...... " << std::endl;

}


void TDT724Waveform::UpdateHistograms(TDataContainer& dataContainer){

  int eventid = dataContainer.GetMidasData().GetEventId();
  int timestamp = dataContainer.GetMidasData().GetTimeStamp();

	TDT724RawData *dt724 = dataContainer.GetEventData<TDT724RawData>("D724");
	
	if(dt724){      
		

		std::vector<RawChannelMeasurement> measurements = dt724->GetMeasurements();

		for(int i = 0; i < measurements.size(); i++){
			
			int chan = measurements[i].GetChannel();
			
			
			// Reset the histogram...
			for(int ib = 0; ib < GetHistogram(chan)->GetNbinsX(); ib++)
				GetHistogram(chan)->SetBinContent(ib+1,0);

			//std::cout << "Nsamples " <<  measurements[i].GetNSamples() << std::endl;
			for(int ib = 0; ib < measurements[i].GetNSamples(); ib++){
				GetHistogram(chan)->SetBinContent(ib+1, measurements[i].GetSample(ib));
			}

		}
  }

}



void TDT724Waveform::Reset(){


	for(int i = 0; i < 8; i++){ // loop over channels
		int index =  i;

		// Reset the histogram...
		for(int ib = 0; ib < GetHistogram(index)->GetNbinsX(); ib++) {
			GetHistogram(index)->SetBinContent(ib, 0);
		}

		GetHistogram(index)->Reset();
    
  }
}

#include "TV1720Waveform.h"

#include "TV1720RawData.h"
#include "TDirectory.h"


/// Reset the histograms for this canvas
TV1720Waveform::TV1720Waveform(){

  SetNanosecsPerSample(4); //ADC clock runs at 250Mhz on the v1720 = units of 4 nsecs

  CreateHistograms();
}


void TV1720Waveform::CreateHistograms(){

  // check if we already have histogramss.
  char tname[100];
  sprintf(tname,"V1720_%i",0);

  TH1D *tmp = (TH1D*)gDirectory->Get(tname);
  if (tmp) return;

	int fWFLength = 2000; // Need a better way of detecting this...
  int numSamples = fWFLength / nanosecsPerSample;

  // Otherwise make histograms
  clear();

	for(int i = 0; i < 8; i++){ // loop over 8 channels
		
		char name[100];
		char title[100];
		sprintf(name,"V1720_%i",i);

		sprintf(title,"V1720 Waveform for channel=%i",i);	
		
		TH1D *tmp = new TH1D(name, title, numSamples, 0, fWFLength);
		tmp->SetXTitle("ns");
		tmp->SetYTitle("ADC value");
		
		push_back(tmp);
	}

}


void TV1720Waveform::UpdateHistograms(TDataContainer& dataContainer){

  int eventid = dataContainer.GetMidasData().GetEventId();
  int timestamp = dataContainer.GetMidasData().GetTimeStamp();


	//    char name[100];
	//sprintf(name,"W2%02d",iBoard);

	TV1720RawData *v1720 = dataContainer.GetEventData<TV1720RawData>("W200");
	
	if(v1720 && v1720->IsZLECompressed()){      
		
		for(int i = 0; i < 8; i++){ // loop over channels
			
			// Check if this channel has any data in this event.
			int chhex = 1 << i  ;
			if(!(v1720->GetChannelMask() & chhex)){
				std::cout << "No data ... " << std::endl;
				continue;
			}
			
			int index =  i;
			// Reset the histogram...
			for(int ib = 0; ib < 250; ib++)
				GetHistogram(index)->SetBinContent(ib+1,0);
			
			TV1720RawChannel channelData = v1720->GetChannelData(i);
			
			// Loop over pulses, filling the histogram
			for(int j = 0; j < channelData.GetNZlePulses(); j++){
				TV1720RawZlePulse pulse = channelData.GetZlePulse(j);
				for(int k = 0; k < pulse.GetNSamples(); k++){
					int bin = pulse.GetFirstBin() + k;
					GetHistogram(index)->SetBinContent(bin,pulse.GetSample(k));
				}
			}
			
		}
	}
	
	if(v1720 && !v1720->IsZLECompressed()){      
		
		for(int i = 0; i < 8; i++){ // loop over channels
			
			int index = i;
			
			// Reset the histogram...
			//        for(int ib = 0; ib < 2500; ib++)
			for(int ib = 0; ib < 250; ib++)
				GetHistogram(index)->SetBinContent(ib+1,0);
			
			
			TV1720RawChannel channelData = v1720->GetChannelData(i);
			for(int j = 0; j < channelData.GetNSamples(); j++){
				double adc = channelData.GetADCSample(j);
				GetHistogram(index)->SetBinContent(j+1,adc);
				
			}            		
    }
  }

}



void TV1720Waveform::Reset(){


	for(int i = 0; i < 8; i++){ // loop over channels
		int index =  i;

		// Reset the histogram...
		for(int ib = 0; ib < GetHistogram(index)->GetNbinsX(); ib++) {
			GetHistogram(index)->SetBinContent(ib, 0);
		}

		GetHistogram(index)->Reset();
    
  }
}

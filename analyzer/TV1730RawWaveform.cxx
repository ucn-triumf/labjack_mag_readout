#include "TV1730RawWaveform.h"

#include "TV1730RawData.hxx"
#include "TDirectory.h"          

/// Reset the histogram for this canvas
TV1730RawWaveform::TV1730RawWaveform(){

	SetNumSamples(64);     
  SetNanosecsPerSample(2); //ADC clock runs at 500Mhz on the v1730 = units of 2 nsecs

  CreateHistograms();
}


void TV1730RawWaveform::CreateHistograms(){

	std::cout << "Creating V1730 raw waveforms... " << std::endl;

  // check if we already have histograms.
  char tname[100];
  sprintf(tname,"V1730Raw_%i_%i",0,0);

  TH1D *tmp = (TH1D*)gDirectory->Get(tname);
  if (tmp) return;

  // Otherwise make histogram
  clear();

	for(int i = 0; i < 16; i++){ // loop over 16 channels
		
		char name[100];
		char title[100];
		sprintf(name,"V1730Raw_%i",i);
		
		sprintf(title,"V1730 Waveform for channel=%i",i);	
		
		TH1D *tmp = new TH1D(name, title, this->numSamples, 0, this->numSamples);
		tmp->GetXaxis()->SetLimits(0, this->numSamples*this->nanosecsPerSample);
		tmp->SetXTitle("ns");
		tmp->SetYTitle("ADC value");
		
		push_back(tmp);
	}

	std::cout << "Finish V1730 raw waveforms... " << std::endl;

}




void TV1730RawWaveform::UpdateHistograms(TDataContainer& dataContainer){

  int eventid = dataContainer.GetMidasData().GetEventId();
  int timestamp = dataContainer.GetMidasData().GetTimeStamp();
	
	TV1730RawData *v1730 = dataContainer.GetEventData<TV1730RawData>("V730");

	if(v1730 ){      

		std::vector<RawChannelMeasurement> measurements = v1730->GetMeasurements();

		for(int i = 0; i < measurements.size(); i++){
			
			int chan = measurements[i].GetChannel();
			
        // Reset the histogram...
			for(int ib = 0; ib < this->numSamples; ib++)
				GetHistogram(chan)->SetBinContent(ib+1,0);

			// Hack!
			float offset = 0;
			if(chan == 1)
				offset = 35;
			//std::cout << "Nsamples " <<  measurements[i].GetNSamples() << std::endl;
			for(int ib = 0; ib < measurements[i].GetNSamples(); ib++){


				GetHistogram(chan)->SetBinContent(ib+1, measurements[i].GetSample(ib)-offset);
				//std::cout << "Setting " << chan << " " << ib << " " << measurements[i].GetSample(ib) << std::endl;

			}

		}

	}

}


void TV1730RawWaveform::Reset(){

	// Loop over all the data, reset histos
  for(int iBoard=0; iBoard<32; iBoard++){// Loop over V1730Dpp boards        
		for(int i = 0; i < 8; i++){ // loop over channels			
			int index = iBoard*8 + i;
			
			// Reset the histogram...
			for(int ib = 0; ib < 2500; ib++)
				GetHistogram(index)->SetBinContent(ib,0);
			GetHistogram(index)->Reset();
		}
	}
}
		
/// Take actions at begin run
void TV1730RawWaveform::BeginRun(int transition,int run,int time){

  CreateHistograms();

}

/// Take actions at end run  
void TV1730RawWaveform::EndRun(int transition,int run,int time){

}

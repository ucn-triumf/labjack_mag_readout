#include "TAnaManager.hxx"
#include "TV1720RawData.h"


TAnaManager::TAnaManager(){

	fV792Histogram = 0;
#ifdef USE_V792
	fV792Histogram = new TV792Histograms();
	fV792Histogram->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fV1190Histogram = 0;
#ifdef USE_V1190
	fV1190Histogram = new TV1190Histograms();
	fV1190Histogram->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fL2249Histogram = 0;
#ifdef USE_L2249
  fL2249Histogram = new TL2249Histograms();
	fL2249Histogram->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fAgilentHistogram = 0;
#ifdef USE_AGILENT
	fAgilentHistogram = new TAgilentHistograms();
	fAgilentHistogram->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fV1720Waveform = 0;
#ifdef USE_V1720

	fV1720Waveform = new TV1720Waveform();
	fV1720Waveform->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.

        fV1720PHCompare = new TH2F("V1720PHCompare","Pulse Height: Channel 1 vs 0",100,300,700,100,300,700);
        fV1720PHCompare->SetXTitle("Channel 0");
        fV1720PHCompare->SetYTitle("Channel 1");
        fV1720TimeCompare = new TH2F("V1720TimeCompare","Time: Channel 1 vs 0",100,0,2000,100,0,2000);
        fV1720TimeCompare->SetXTitle("Channel 0");
        fV1720TimeCompare->SetYTitle("Channel 1");
#endif

	fV1720Correlations = 0;
#ifdef USE_V1720
	fV1720Correlations = new TV1720Correlations();
	fV1720Correlations->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fV1730DppWaveform = 0;
#ifdef USE_V1730DPP
  fV1730DppWaveform = new TV1730DppWaveform();
	fV1730DppWaveform->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fV1730RawWaveform = 0;
#ifdef USE_V1730RAW
	fV1730RawWaveform = new TV1730RawWaveform();
	fV1730RawWaveform->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

	fDT724Waveform = 0;
#ifdef USE_DT724
	fDT724Waveform = new TDT724Waveform();
	fDT724Waveform->DisableAutoUpdate();  // disable auto-update.  Update histo in AnaManager.
#endif

};



int TAnaManager::ProcessMidasEvent(TDataContainer& dataContainer){


	if(fV792Histogram) fV792Histogram->UpdateHistograms(dataContainer); 
  	if(fV1190Histogram)  fV1190Histogram->UpdateHistograms(dataContainer); 
	if(fL2249Histogram)  fL2249Histogram->UpdateHistograms(dataContainer); 
  	if(fAgilentHistogram)  fAgilentHistogram->UpdateHistograms(dataContainer); 
	if(fV1720Waveform)  fV1720Waveform->UpdateHistograms(dataContainer);
	if(fV1720Correlations)  fV1720Correlations->UpdateHistograms(dataContainer); 
  	if(fV1730DppWaveform)  fV1730DppWaveform->UpdateHistograms(dataContainer); 
	if(fV1730RawWaveform)  fV1730RawWaveform->UpdateHistograms(dataContainer);

        
	if(fDT724Waveform)  fDT724Waveform->UpdateHistograms(dataContainer); 

        // Do little analysis of the V1720 data, as example...
        if(fV1720Waveform){
          
          TV1720RawData *v1720 = dataContainer.GetEventData<TV1720RawData>("W200");

          if(v1720 && !v1720->IsZLECompressed()){      

            double time[2],ph[2];
            
            for(int i = 0; i < 2; i++){ // loop first two channels
              
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
              time[i] = max_adc_time;
              ph[i] = max_adc_value;
              //std::cout << i << " "  << max_adc_time << " " << max_adc_value << std::endl;
            }
            fV1720PHCompare->Fill(ph[0],ph[1]);
            fV1720TimeCompare->Fill(time[0],time[1]);
          }
        }
        return 1;
}



bool TAnaManager::HaveV792Histograms(){
	if(fV792Histogram) return true; 
	return false;
}
bool TAnaManager::HaveV1190Histograms(){
	if(fV1190Histogram) return true; 
	return false;
};
bool TAnaManager::HaveL2249Histograms(){
	if(fL2249Histogram) return true; 
	return false;
};
bool TAnaManager::HaveAgilentistograms(){
	if(fAgilentHistogram) return true; 
	return false;
};
bool TAnaManager::HaveV1720Histograms(){
	if(fV1720Waveform) return true; 
	return false;
};
bool TAnaManager::HaveV1720Correlations(){
	if(fV1720Correlations) return true; 
	return false;
};
bool TAnaManager::HaveV1730DPPistograms(){
	if(fV1730DppWaveform) return true; 
	return false;
};
bool TAnaManager::HaveV1730Rawistograms(){
	if(fV1730RawWaveform) return true; 
	return false;
};
bool TAnaManager::HaveDT724Histograms(){
	if(fDT724Waveform) return true; 
	return false;
};

TV792Histograms* TAnaManager::GetV792Histograms() {return fV792Histogram;}
TV1190Histograms* TAnaManager::GetV1190Histograms(){return fV1190Histogram;}
TL2249Histograms* TAnaManager::GetL2249Histograms(){return fL2249Histogram;}
TAgilentHistograms* TAnaManager::GetAgilentistograms(){return fAgilentHistogram;}
TV1720Waveform* TAnaManager::GetV1720Histograms(){return fV1720Waveform;}
TV1720Correlations* TAnaManager::GetV1720Correlations(){return fV1720Correlations;}
TV1730DppWaveform* TAnaManager::GetV1730DPPistograms(){return fV1730DppWaveform;}
TV1730RawWaveform* TAnaManager::GetV1730Rawistograms(){return fV1730RawWaveform;}
TDT724Waveform* TAnaManager::GetDT724Histograms(){return fDT724Waveform;}


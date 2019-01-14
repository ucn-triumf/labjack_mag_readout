#include <stdio.h>
#include <iostream>

#include "TRootanaDisplay.hxx"
#include "TH1D.h"
#include "TV792Data.hxx"

#include "TFancyHistogramCanvas.hxx"
#include "TInterestingEventManager.hxx"


#include "TAnaManager.hxx"


class MyTestLoop: public TRootanaDisplay { 

public:
	
  // An analysis manager.  Define and fill histograms in 
  // analysis manager.
  TAnaManager *anaManager;

  MyTestLoop() {
    SetOutputFilename("example_output");
    DisableRootOutput(false);
    anaManager = new TAnaManager();
    // Number of events to skip before plotting one.
    //SetNumberSkipEvent(10);
    // Choose to use functionality to update after X seconds
    SetOnlineUpdatingBasedSeconds();
    // Uncomment this to enable the 'interesting event' functionality.
    //iem_t::instance()->Enable();
  }

  void AddAllCanvases(){

    // Set up tabbed canvases
        
    if(anaManager->HaveV792Histograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV792Histograms(),"V792"));
    
    if(anaManager->HaveV1190Histograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV1190Histograms(),"V1190"));
    
    if(anaManager->HaveL2249Histograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetL2249Histograms(),"L2249"));
    
    if(anaManager->HaveAgilentistograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetAgilentistograms(),"AGILENT"));
    
    if(anaManager->HaveV1720Histograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV1720Histograms(),"V1720 Waveforms"));

    if(anaManager->HaveV1720Correlations()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV1720Correlations(),"V1720 Correlations"));
    
    if(anaManager->HaveV1730DPPistograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV1730DPPistograms(),"V1730 Waveforms"));
    
    if(anaManager->HaveV1730Rawistograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetV1730Rawistograms(),"V1730 Waveforms"));
    
    if(anaManager->HaveDT724Histograms()) 
      AddSingleCanvas(new TFancyHistogramCanvas(anaManager->GetDT724Histograms(),"DT724 Waveforms"));
    
    SetDisplayName("Example Display");
  };

  virtual ~MyTestLoop() {};

  void BeginRun(int transition,int run,int time) {
    std::cout << "User BOR method" << std::endl;
  }

  void EndRun(int transition,int run,int time) {
    std::cout << "User EOR method" << std::endl;
  }

  void ResetHistograms(){}

  void UpdateHistograms(TDataContainer& dataContainer){
    
    anaManager->ProcessMidasEvent(dataContainer);
  }

  void PlotCanvas(TDataContainer& dataContainer){}


}; 






int main(int argc, char *argv[])
{
  MyTestLoop::CreateSingleton<MyTestLoop>();  
  return MyTestLoop::Get().ExecuteLoop(argc, argv);
}


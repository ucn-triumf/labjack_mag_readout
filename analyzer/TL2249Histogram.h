#ifndef TL2249Histograms_h
#define TL2249Histograms_h

#include <string>
#include "THistogramArrayBase.h"

/// Class for making histograms of L2249 ADC data.
class TL2249Histograms : public THistogramArrayBase {
 public:
  TL2249Histograms();
  virtual ~TL2249Histograms(){};
  
  /// Update the histograms for this canvas.
  void UpdateHistograms(TDataContainer& dataContainer);

  /// Take actions at begin run
  void BeginRun(int transition,int run,int time);

  /// Take actions at end run  
  void EndRun(int transition,int run,int time);

private:

  void CreateHistograms();
    
};

#endif



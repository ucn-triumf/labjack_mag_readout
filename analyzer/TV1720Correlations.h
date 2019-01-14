#ifndef TV1720Correlations_h
#define TV1720Correlations_h

#include <string>
#include "THistogramArrayBase.h"
#include "TH2D.h"
/// Class for making 2D histogram of correlation
/// between V1720 pulse height and bin
class TV1720Correlations : public THistogramArrayBase {
public:
  TV1720Correlations(){
    CreateHistograms();
  };
  virtual ~TV1720Correlations(){};

  void UpdateHistograms(TDataContainer& dataContainer);

  // Reset the histograms; needed before we re-fill each histo.
  void Reset();
  
  void CreateHistograms();
  
  /// Take actions at begin run
  void BeginRun(int transition,int run,int time){		
    CreateHistograms();		
  }

private:
};

#endif



#ifndef TAgilentHistograms_h
#define TAgilentHistograms_h

#include <string>
#include "THistogramArrayBase.h"

/// Class for making histograms of Agilent ampere data.
class TAgilentHistograms : public THistogramArrayBase {
 public:
  TAgilentHistograms();
  virtual ~TAgilentHistograms(){};
  
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



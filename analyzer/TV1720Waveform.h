#ifndef TV1720Waveform_h
#define TV1720Waveform_h

#include <string>
#include "THistogramArrayBase.h"

/// Class for making histograms of raw V1720 waveforms;
/// right now is only for raw or ZLE compressed data.
class TV1720Waveform : public THistogramArrayBase {
public:
  TV1720Waveform();
  virtual ~TV1720Waveform(){};

  void UpdateHistograms(TDataContainer& dataContainer);

  /// Getters/setters
  int GetNsecsPerSample() { return nanosecsPerSample; }
  void SetNanosecsPerSample(int nsecsPerSample) { this->nanosecsPerSample = nsecsPerSample; }

	// Reset the histograms; needed before we re-fill each histo.
	void Reset();

  void CreateHistograms();

	/// Take actions at begin run
	void BeginRun(int transition,int run,int time){		
		CreateHistograms();		
	}

private:
  int nanosecsPerSample;
};

#endif



#ifndef TDT724Waveform_h
#define TDT724Waveform_h

#include <string>
#include "THistogramArrayBase.h"

/// Class for making histograms of raw DT724 waveforms;
class TDT724Waveform : public THistogramArrayBase {
public:
  TDT724Waveform();
  virtual ~TDT724Waveform(){};

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



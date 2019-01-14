#ifndef TV1730DppWaveforms_h
#define TV1730DppWaveforms_h

#include <string>
#include "THistogramArrayBase.h"

/// Class for making histogram of V1730 waveforms (with DPP event encoding)
class TV1730DppWaveform : public THistogramArrayBase {
public:
  TV1730DppWaveform();
  virtual ~TV1730DppWaveform(){};

  /// Update the histogram for this canvas.
  /// This method works, but is generally not used; instead, TDeapAnaManager handles filling this histogram.
  void UpdateHistograms(TDataContainer& dataContainer);

  /// Take actions at begin run
  void BeginRun(int transition,int run,int time);

  /// Take actions at end run  
  void EndRun(int transition,int run,int time);

  int GetNumSamples() { return numSamples; }
  void SetNumSamples(int numSamples) { this->numSamples = numSamples; }
  int GetNsecsPerSample() { return nanosecsPerSample; }
  void SetNanosecsPerSample(int nsecsPerSample) { this->nanosecsPerSample = nsecsPerSample; }

	// Reset the histogram; needed before we re-fill each histo.
	void Reset();

private:

  int numSamples;
  int nanosecsPerSample;
  void CreateHistograms();


};

#endif



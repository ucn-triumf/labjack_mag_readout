This directory contains an example of a rootana analyzer based on the
libAnalyzer/libAnalyzerDisplay framework.  It contains analysis code appropriate for
processing 'standard' TRIUMF MIDAS data taken with CAEN V792, V1190, V1720, V1730, DT5724, CAMAC TL2249 and Agilent data.

 To use the example code in this directory, I recommend the following steps

1) Install rootana in usual place ($HOME/packages) and make (cd $HOME/packages/rootana; make)

2) Set environment variable ROOTANASYS to rootana location (usually $HOME/packages/rootana)

3) Copy this directory (rootana/examples) to somewhere to play with code and build

mkdir ~/analyzer/.
cp ~/packages/rootana/examples/* ~/analyzer/.
cd ~/analyzer
make

4) You will have now built a default analyzer display (anaDisplay.exe) and batch-mode analyzer (ana.exe) 
that you can use to display CAEN V792 and V1190 data.

You can then modify the files to create new plots.

The files anaDisplay.cxx and ana.cxx mostly just use histograms setup inside TAnaManager.hxx/cxx.  In particular
you can edit the file TAnaManager.hxx to quickly choose which equipment to use with ifdef.  For instance, the 
selection

#define USE_V792
#define USE_V1190
//#define USE_L2249
//#define USE_AGILENT
//#define USE_V1720
//#define USE_V1730DPP
//#define USE_V1730RAW
//#define USE_DT724

will create histograms of V792 and V1190 data.  By changing the commented out precompiler flags you can
modify the expected hardware.


Thomas Lindner
lindner@triumf.ca




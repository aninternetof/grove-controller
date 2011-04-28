#ifndef __IPlugFFT__
#define __IPlugFFT__

#include "../IPlug_include_in_plug_hdr.h"
#include "fft.h"

//#define FFTLENGTH 32768
#define FFTLENGTH 2048
//#define FFTLENGTH 10240

class IDistributionGraph : public IControl{
public:
	IDistributionGraph(IPlugBase* pPlug, int x, int y, int width, int height, int paramIdx);
	bool Draw(IGraphics* pGraphics);
	bool IsDirty() {return true;}
	void SetGraphVals(double* mag, double* loud);
private:
	IPlugBase *m_pPlug;
	double * FFTmag;
	double * loudLine;
};

class Grove : public IPlug
{
public:

	Grove(IPlugInstanceInfo instanceInfo);
	~Grove() {}
	void ProcessDoubleReplacing(double** inputs, double ** outputs, int nFrames);

private:
	int sampleRate;
	IDistributionGraph * graph;
	ITextControl * debugText;
	int fftbuffersize;
	int count, samps;
	int oldNote;
	int newCount;
	double loudness;
	WDL_FFT_COMPLEX fftbuffer[FFTLENGTH];
	WDL_FFT_COMPLEX sortedbuffer[FFTLENGTH];
	WDL_FFT_COMPLEX unsortedbuffer[FFTLENGTH];
	double magFFT[FFTLENGTH];
	int PitchDetect();
};




#endif
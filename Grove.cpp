#include "Grove.h"
#include "../IPlug_include_in_plug_src.h"
#include "../IControl.h"
#include "resource.h"
#include <math.h>
#include "fft.h"

enum EParams {
	kIGain,
	kOGain,
	kThresh,
	kResp,
	kGraphDummy,
	kNumParams
};
const int kNumPrograms = 1;

enum ELayout {
	//size of entire GUI
	kW = 500,
	kH = 250,

	//debugging text
	kText_L = 5,
	kText_R = 30,
	kText_T = 5,
	kText_B = 30,

	//length for all faders
	kFader_Len = 140,

	//IGainFader
	kIGain_X = 37,
	kIGain_Y = 19,

	//Threshold Fader
	kThresh_X = 91,
	kThresh_Y = 19,

	//OGain Fader
	kOGain_X = 435,
	kOGain_Y = 19,

	//Response Knob
	kResp_X = 56,
	kResp_Y = 178,

	//Spectrum Graph
	kGraph_X = 158,
	kGraph_Y = 9,
	kGraph_W = 243,
	kGraph_H = 231
};

IDistributionGraph::IDistributionGraph(IPlugBase *pPlug, int x, int y, int width, int height, int paramIdx)
: IControl(pPlug, &IRECT(x,y,x+width,y+height ))
{
	OutputDebugString("dist graph ctor");
	m_pPlug=pPlug;
}

bool IDistributionGraph::Draw(IGraphics* pGraphics){
	OutputDebugString("dist graph draw()");
	double mag = 0;
	for (int i=0; i<kGraph_W-20; i++){
		if (FFTmag[i] > kGraph_H-100){
			mag = kGraph_H-100;
		}else{
			mag = FFTmag[i];
		}
		pGraphics->DrawLine(new IColor(0,0,i,0),kGraph_X+10+i,kGraph_Y+kGraph_H-50,kGraph_X+10+i,kGraph_Y+kGraph_H-50-mag);
		pGraphics->DrawLine(new IColor(0,0,0,*loudLine*500),kGraph_X+10,kGraph_Y+kGraph_H-50-*loudLine*100,kGraph_X+kGraph_W-10,kGraph_Y+kGraph_H-50-*loudLine*100);
	}
	return true;
}

void IDistributionGraph::SetGraphVals(double* mag, double* loud){
	FFTmag = mag;
	loudLine = loud;
	return;
}


int Grove::PitchDetect(){

	//HPS pitch detection http://sig.sapp.org/src/sigAudio/Pitch_HPS.cpp
	int spectrum[FFTLENGTH] = {0};
	//make local copy of magFFT to mess with
	for (int i=0; i<FFTLENGTH; i++){
		spectrum[i] = magFFT[i];
	}

	int harmonics = 2;
	int maxHIndex = FFTLENGTH/harmonics; //must have this to keep from going out of bounds
										//but this also sets an upper limit on detection
	int minIndex = 0; //lowest possible place to look for a pitch. Need to get it to hit about lowest note a human can sing.

	int maxLoc = minIndex;
	for (int i=0; i<maxHIndex; i++){
		for (int j=1; j<=harmonics; j++){  //j=1 means that the fundamental is doubled
			spectrum[i] *= spectrum[i*j];
		}
		if (spectrum[i] > spectrum[maxLoc]){
			maxLoc = i;
		}
	}


	//double frequency protection
	//first search for the maximum (max2) in the range below the prev'ly detected one
	int max2 = minIndex;
	int maxSearch = maxLoc * 3/4;
	for (int i=minIndex+1; i<maxSearch; i++){
		if (spectrum[i] > spectrum[max2]){
			max2 = i;
		}
	}
   if (abs(max2 * 2 - maxLoc) < 4) {	//if it's within 4 of being half of the prev'ly detected
	   if(spectrum[maxLoc] != 0){
			if (spectrum[max2]/spectrum[maxLoc] > 0.2) { //if it's at least so big compared the prev'ly detected (default 0.2)
			 maxLoc = max2;
			}
	   }
   }

 	//use the following lines to display a value during debugging
	char text[50];
	sprintf(text,"%i",maxLoc);
	debugText->SetTextFromPlug(text);

	double freq = maxLoc*sampleRate/FFTLENGTH;
	int midiNote = floor(17.3123*log(freq/440)+69 + 0.5); //calculate MIDI note from fund freq and round.
															// http://audiores.uint8.com.ar/blog/2007/08/26/fundamental-in-hz-to-a-midi-note

	return midiNote;
};

Grove::Grove(IPlugInstanceInfo instanceInfo)
:	IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo), fftbuffersize(FFTLENGTH), count(0)
{
	TRACE;

	for (int i=0;i<FFTLENGTH;i++) magFFT[i]=0; //initialize buffer
	
	//load sample rate from host
	sampleRate = GetSampleRate();
	
	//initialize values for MIDI transmission
	oldNote = 0;
	newCount = 0;

	//***SET UP PARAMETERS***
	GetParam(kIGain)->InitDouble("Input Gain", 0.0, -70.0, 12.0, 0.1, "dB");
	GetParam(kOGain)->InitDouble("Ouput Gain", 0.0, -70.0, 12.0, 0.1, "dB");
	GetParam(kThresh)->InitDouble("Threshold", 0.01, 0.0, 1.0, 0.01, "");
	GetParam(kResp)->InitInt("Response",8,0,20,"in a row");

	//***SET UP GUI***
	IGraphics* pGraphics = MakeGraphics(this,kW,kH);
	pGraphics->AttachBackground(BG_ID, BG_FN);
	//IGain fader graphic
	IBitmap bitmap = pGraphics->LoadIBitmap(FADER_ID, FADER_FN);
	pGraphics->AttachControl(new IFaderControl(this, kIGain_X, kIGain_Y, kFader_Len, kIGain, &bitmap, kVertical));
	//Threshold fader graphic
	pGraphics->AttachControl(new IFaderControl(this, kThresh_X, kThresh_Y, kFader_Len, kThresh, &bitmap, kVertical));
	//OGain fader graphic
	pGraphics->AttachControl(new IFaderControl(this, kOGain_X, kOGain_Y, kFader_Len, kOGain, &bitmap, kVertical));
	//Respose knob graphic
	bitmap = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN);
	pGraphics->AttachControl(new IKnobRotaterControl(this, kResp_X, kResp_Y, kResp, &bitmap));
	//Spectrum Graph
	graph = new IDistributionGraph(this, kGraph_X, kGraph_Y, kGraph_W, kGraph_H, kGraphDummy);
	graph->SetGraphVals(magFFT,&loudness); //initialize pointer
	pGraphics->AttachControl(this->graph);
	//Pitch text
	debugText = new ITextControl(this,new IRECT(kText_L,kText_T,kText_R,kText_B),new IText(&COLOR_BLACK));
	pGraphics->AttachControl(debugText);
	//Enable GUI
	AttachGraphics(pGraphics);

	WDL_fft_init();

	MakeDefaultPreset("-", kNumPrograms);
}


void Grove::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
	double* in1 = inputs[0];
	double* in2 = inputs[1];
	double* out1 = outputs[0];
	double* out2 = outputs[1];

	//load parameters
	double iGain = GetParam(kIGain)->DBToAmp();
	double oGain = GetParam(kOGain)->DBToAmp();
	double thresh = GetParam(kThresh)->Value();
	int resp = GetParam(kResp)->Value();

	//use the following lines to display a value during debugging
	//char text[50];
	//sprintf(text,"%i",resp);
	//debugText->SetTextFromPlug(text);
	
	for (int s = 0; s < nFrames; ++s, ++in1, ++in2, ++out1, ++out2) 
	{
		if(count == fftbuffersize-1)
		{

			//use FFT buffer to find loudness for simplicity's sake. Must do it here while it's still in the time domain.
			loudness = 0;
			for (int i = 0; i < fftbuffersize; i++){	//RMS
				loudness += pow(fftbuffer[i].re,2);
			}
			loudness = (sqrt(loudness/fftbuffersize)*oGain)/0.4; //this is going to need an adjustable input gain to deal with varying input levels

			WDL_fft(fftbuffer, fftbuffersize, false);

			for (int i = 0; i < fftbuffersize; i++)
			{
				int j = WDL_fft_permute(fftbuffersize, i);
				sortedbuffer[i].re = fftbuffer[j].re;
				sortedbuffer[i].im = fftbuffer[j].im;
			} 

			//do processing on sortedbuffer here
			for (int i = 0; i < fftbuffersize; i++)
			{
				//cartesian to polar
				double mag = sqrt(sortedbuffer[i].re*sortedbuffer[i].re + sortedbuffer[i].im*sortedbuffer[i].im);
				double phase = atan2(sortedbuffer[i].im, sortedbuffer[i].re);
				magFFT[i] = mag; 
			}

			int newNote = PitchDetect();

			if (loudness > thresh){	//if loudness above threshold
					if ((newNote != oldNote) && (newNote != oldNote+12) && (newNote != oldNote-12)){ //if new note that's not an octave jump
							newCount++;
							if (newCount == 8){
								IMidiMsg* pMsg = new IMidiMsg;
								pMsg->MakeNoteOffMsg(oldNote,0);
								SendMidiMsg(pMsg);
								pMsg->MakeNoteOnMsg(newNote,loudness*127,0);
								SendMidiMsg(pMsg);
								pMsg->MakeControlChangeMsg(pMsg->kGeneralPurposeController1,loudness);
								SendMidiMsg(pMsg);
								oldNote = newNote;
								newCount = 0;
							}else{
								IMidiMsg* pMsg = new IMidiMsg;
								pMsg->MakeControlChangeMsg(pMsg->kGeneralPurposeController1,loudness);
								SendMidiMsg(pMsg);
							}
					}else{					//same note or octave jump
						IMidiMsg* pMsg = new IMidiMsg;
						pMsg->MakeControlChangeMsg(pMsg->kGeneralPurposeController1,loudness);
						SendMidiMsg(pMsg);
					}
				}else{					//loudness not above threshold
					IMidiMsg* pMsg = new IMidiMsg;
					pMsg->MakeNoteOffMsg(oldNote,0);
					SendMidiMsg(pMsg);
				}
			count = 0;
		}else{
			count++;
		}

	fftbuffer[count].re = (WDL_FFT_REAL) *in1*iGain; 
	fftbuffer[count].im = 0.;

	*out1 = *in1; //directly from input
	*out2 = *out1; //mono

	}
}


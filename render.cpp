// Example of FFT on Bela using Neon library in Auxiliary Task 
// and sending frequency domain data to WebSocket

#include <string>
#include <vector>
#include <math.h>
#include <ctime>
#include <cmath>
#include <stdio.h>

// Bela
#include <Bela.h> 
#include <libraries/Gui/Gui.h>
#include <libraries/ne10/NE10.h>

#define BUFFER_SIZE 16384

float gInputBuffer[BUFFER_SIZE];
float gOutputBuffer[BUFFER_SIZE];
float *gWindowBuffer;
int gInputBufferPointer       = 0;
int gOutputBufferWritePointer = 0;
int gOutputBufferReadPointer  = 0;
int gSampleCount              = 0;

ne10_fft_cpx_float32_t* timeDomainIn;
ne10_fft_cpx_float32_t* timeDomainOut;
ne10_fft_cpx_float32_t* frequencyDomain;
ne10_fft_cfg_float32_t  cfg;
float gFFTScaleFactor = 0;
int gFFTSize = 2048;
int gHopSize = 512;
int gPeriod  = 512;

AuxiliaryTask gFFTTask;
void process_fft_background(void*);
int gFFTInputBufferPointer  = 0;
int gFFTOutputBufferPointer = 0;

// GUI
Gui gui;
std::vector<float> freqDomainReal;
std::vector<float> freqDomainImag;

float *gInputAudio = NULL;
int gAudioChannelNum;

bool setup(BelaContext *context, void *userData)
{
	gAudioChannelNum = 2;

	gFFTScaleFactor = 1.0f / (float)gFFTSize;
	gOutputBufferWritePointer += gHopSize;

	timeDomainIn    = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	timeDomainOut   = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	frequencyDomain = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	cfg = ne10_fft_alloc_c2c_float32_neon (gFFTSize);

	memset(timeDomainOut, 0, gFFTSize    * sizeof (ne10_fft_cpx_float32_t));
	memset(gOutputBuffer, 0, BUFFER_SIZE * sizeof (float));

	gInputAudio   = (float *)malloc(context->audioFrames * gAudioChannelNum * sizeof(float));
	gWindowBuffer = (float *)malloc(gFFTSize * sizeof(float));
	if(gInputAudio   == 0) return false;
	if(gWindowBuffer == 0) return false;

	// Calculate a Hann window
	for(int n = 0; n < gFFTSize; n++)
		gWindowBuffer[n] = 0.5f * (1.0f - cosf(2.0f * M_PI * n / (float)(gFFTSize - 1)));

	// Initialise auxiliary tasks
	if((gFFTTask = Bela_createAuxiliaryTask(&process_fft_background, 90, "fft-calculation")) == 0)
		return false;

	gui.setup(context->projectName);

	return true; 
}

void process_fft(float *inBuffer, int inWritePointer, float *outBuffer, int outWritePointer)
{
	// Copy buffer into FFT input
	int pointer = (inWritePointer - gFFTSize + BUFFER_SIZE) % BUFFER_SIZE;
	for(int n = 0; n < gFFTSize; n++) {
		timeDomainIn[n].r = (ne10_float32_t) inBuffer[pointer] * gWindowBuffer[n];
		timeDomainIn[n].i = 0;
		if(++pointer >= BUFFER_SIZE) pointer = 0;
	}

	// FFT
	ne10_fft_c2c_1d_float32_neon (frequencyDomain, timeDomainIn, cfg, 0);

	// Do Freq domain stuff
	if(gui.isConnected()) {
		freqDomainReal.clear();
		freqDomainImag.clear();
		for(int n = 0; n < gFFTSize; n++) {
			float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
			freqDomainReal.push_back(amplitude);
			freqDomainImag.push_back((float)frequencyDomain[n].i);
		}
		gui.sendBuffer(0, freqDomainReal);
		gui.sendBuffer(1, freqDomainImag);
	}

	// IFFT
	ne10_fft_c2c_1d_float32_neon (timeDomainOut, frequencyDomain, cfg, 1);
	
	// into the output buffer
	pointer = outWritePointer;
	for(int n = 0; n < gFFTSize; n++) {
		outBuffer[pointer] += (timeDomainOut[n].r) * gFFTScaleFactor;
		if(std::isnan(outBuffer[pointer])) rt_printf("outBuffer OLA\n");
		if(++pointer >= BUFFER_SIZE) pointer = 0;
	}
}

void process_fft_background(void*) {
	process_fft(gInputBuffer, gFFTInputBufferPointer, gOutputBuffer, gFFTOutputBufferPointer);
}

void render(BelaContext* context, void* userData)
{
	for(int n = 0; n < context->audioFrames; n++) {
		
		gInputBuffer[gInputBufferPointer] = audioRead(context, n, 0);

		for(int channel = 0; channel < gAudioChannelNum; channel++)
			audioWrite(context, n, channel, gOutputBuffer[gOutputBufferReadPointer]);
		gOutputBuffer[gOutputBufferReadPointer] = 0;

		if (++gOutputBufferReadPointer  >= BUFFER_SIZE) gOutputBufferReadPointer  = 0;
		if (++gOutputBufferWritePointer >= BUFFER_SIZE) gOutputBufferWritePointer = 0;
		if (++gInputBufferPointer       >= BUFFER_SIZE) gInputBufferPointer       = 0;
		if (++gSampleCount >= gHopSize) {
			gFFTInputBufferPointer  = gInputBufferPointer;
			gFFTOutputBufferPointer = gOutputBufferWritePointer;
			Bela_scheduleAuxiliaryTask(gFFTTask);
			gSampleCount = 0;
		}
		
	}
}

void cleanup(BelaContext* context, void* userData)
{
	NE10_FREE(timeDomainIn);
	NE10_FREE(timeDomainOut);
	NE10_FREE(frequencyDomain);
	NE10_FREE(cfg);
	free(gInputAudio);
	free(gWindowBuffer);
}


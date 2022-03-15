// Bell 103 300 Baud Modem audio demodulator using two point Fourier Transform
// inspired by Costin Stroie's Arabell 300 modem  https://github.com/cstroie/Arabell300
// received character handler taken from that code, and simplified
// Thanks, Costin!!

// S James Remington 3/2022
// NOTE: Audio demodulator is too slow for real time decoding on 16 MHz Arduino.

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

//for Code::Blocks
// project->build options->search directories, add (lib,include)
// linker settings: link libraries, add path to libsndfil\lib\libsndfile.lib
// copy bin\libsndfile-1.dll to source directory
// make sure paths are set to libsndfile\include

#include <sndfile.h>

#define F_SAMPLE 9600
#define BAUD 300
#define NPERBAUD (F_SAMPLE/BAUD)   //32 sampled bits/baud
#define PI 3.1415926536
#define twoPI (2*PI)
// receive frequencies for originate modem. Replace with 1070/1270 for answer modem.
#define F_LOW 2025
#define F_HIGH 2225

#define NPRINT 3  //debug prints for rxDecoder

// globals for decoder

// push only circular buffer, overwrites oldest data

int8_t circ_buf[NPERBAUD];
int8_t circ_buf_index = 0; //next "open" slot in circular buffer


#include "bell103_fsk.h"


// global Fourier coefficient tables

int8_t C0r[NPERBAUD],C0i[NPERBAUD],C1r[NPERBAUD],C1i[NPERBAUD];

// populate the sine/cosine table
void init_FT(void)
{
    int i;

    for (i=0; i<NPERBAUD; i++)
    {
    C0r[i] = 127.0*cos(twoPI*i*F_LOW/F_SAMPLE);  //real
  	C0i[i] = 127.0*sin(twoPI*i*F_LOW/F_SAMPLE);  //imag
    C1r[i] = 127.0*cos(twoPI*i*F_HIGH/F_SAMPLE); //real
	C1i[i] = 127.0*sin(twoPI*i*F_HIGH/F_SAMPLE); //imag
  }
}

// two point Fourier transform demodulator
// processes all points in the current circular sample buffer, with NPERBAUD samples

// 32 bit math required!!

      int demodulate (void) {
      static int printed=0;
      int32_t F0r = 0, F0i = 0, F1r = 0, F1i = 0;  //real, imag,
      int ii;
      int8_t sample;

      //max F 32*127*127=516128
        for (ii = 0; ii < NPERBAUD; ii++) {

        sample = circ_buf[circ_buf_index++];  //get NPERBAUD samples from the circular buffer
        if (circ_buf_index == NPERBAUD) circ_buf_index = 0;

        if (printed < NPRINT) printf("%4d,", sample);

          F0r += sample * C0r[ii];
          F0i += sample * C0i[ii];
          F1r += sample * C1r[ii];
          F1i += sample * C1i[ii];
        }

        int32_t m0 = (F0r >> 8) * (F0r >> 8) + (F0i >> 8) * (F0i >> 8);
        int32_t m1 = (F1r >> 8) * (F1r >> 8) + (F1i >> 8) * (F1i >> 8);
        int ret = m1 - m0;

        if (printed < NPRINT) {
            printed++;
            printf("\n0r %d 0i %d 1r %d 1i %d\n", F0r, F0i, F1r, F1i);
            printf("m0 %d m1 %d r %d\n", m0, m1, ret);
        }
        return ret;
      }

void demod_test(void) {

    int8_t i, tmp;

// test demod by sending it a pure cosine, shuffled a bit. Set NPRINT appropriately!

  for (i = 0; i < NPERBAUD; i++)
  {
    circ_buf[circ_buf_index++] = 127.0 * cos(twoPI * i * F_LOW / F_SAMPLE); //real
    if (circ_buf_index == NPERBAUD) circ_buf_index = 0; //cycle round
  }
    tmp = demodulate(); //result should be zero

    circ_buf_index++; // shuffle data left
    if (circ_buf_index == NPERBAUD) circ_buf_index = 0; //cycle round
    tmp = demodulate();  //still zero?

    circ_buf_index++; // shuffle data left
    if (circ_buf_index == NPERBAUD) circ_buf_index = 0; //cycle round
    tmp = demodulate();

}


int main()
    {
    SNDFILE *sf;
    SF_INFO info;
    int num, num_items;
    int frames,sr,channels;
    int i;

    signed long average = 0;
    static int print=0;
    printf("Bell 103 300 Baud decoder, starting...\n\n");
    init_FT();
    demod_test();
 //  return 0;

/* Open the WAV file. In this case, sample rate 9600, format 32 bit floats */

    info.format = 0;
    sf = sf_open("300bps_9600.wav",SFM_READ,&info); //From Information Society CD "Peace and Love, Inc."

    if (sf == NULL)
        {
        printf("Failed to open the .wav file.\n");
        exit(-1);
        }

// Print some of the info, and figure out how much data to read.

    frames = info.frames;
    sr = info.samplerate;
    channels = info.channels;
    printf("\nAudio file opened!\n");
    printf("frames = %d\n",frames);
    printf("sample rate = %d\n",sr);
    printf("channels = %d\n",channels);
    printf("format = %X\n",info.format);
    num_items = frames*channels;
    printf("num_items = %d\n",num_items);

    /* Allocate space for the data to be read, then read it. */
    float* buf = (float *) malloc(num_items*sizeof(float));
    num = sf_read_float(sf,buf,num_items);
    sf_close(sf);

    printf("Read %d items\n\n",num);

// decode the audio

    int num_samples=0;
    int8_t sample;
    for (i = 0; i < num_items-NPERBAUD; i += channels) // channels should be 1
        {
        sample = (int8_t)(400.0*buf[i]);  //comes in as volts, 32 bit float
//        if (print<100){
//                print++;
//        printf("%d\n",sample);
//        }

        num_samples++;
        rxHandle (sample);
        average += sample;  //just to check!
        }

    printf("\navg of %d audio samples %ld \ndone\n", num_samples, average/num_samples);

    return 0;
    }

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wdsp.h>

//#define TIMING
#ifdef TIMING
#include <sys/time.h>
#endif

#include "audio.h"
#include "channel.h"
#include "discovered.h"
#include "lime_protocol.h"
#include "radio.h"
#include "receiver.h"
#include "SoapySDR/Constants.h"
#include "SoapySDR/Device.h"

static double bandwidth=3000000.0;

static size_t lime_receiver;
static SoapySDRDevice *lime_device;
static SoapySDRStream *stream;
static int lime_sample_rate;
static int display_width;
static int lime_buffer_size=BUFFER_SIZE;
static int outputsamples;
static int lime_fft_size=4096;
static int dspRate=48000;
static int outputRate=48000;
static float *buffer;
static int max_samples;

static long long saved_frequency=0LL;
static int saved_antenna=-1;

//static double iqinputbuffer[BUFFER_SIZE*2];
static double audiooutputbuffer[BUFFER_SIZE*2];
static int samples=0;

static GThread *receive_thread_id;
static gpointer receive_thread(gpointer data);

static int actual_rate;
#ifdef RESAMPLE
static void *resampler;
static double resamples[1024*2];
static double resampled[1024*2];
#endif

#ifdef TIMING
static int rate_samples;
#endif

static int running;

void lime_protocol_change_sample_rate(int rate) {
}

void lime_protocol_init(int rx,int pixels,int sample_rate) {
  SoapySDRKwargs args;
  int rc;

fprintf(stderr,"lime_protocol_init: lime_receiver=%d pixels=%d sample_rate=%d\n",rx,pixels,sample_rate);

  lime_receiver=(size_t)rx;
  display_width=pixels;
  lime_sample_rate=sample_rate;

  outputsamples=BUFFER_SIZE/(sample_rate/48000);
/*
  switch(sample_rate) {
    case 48000:
      outputsamples=BUFFER_SIZE;
      break;
    case 96000:
      outputsamples=BUFFER_SIZE/2;
      break;
    case 192000:
      outputsamples=BUFFER_SIZE/4;
      break;
    case 384000:
      outputsamples=BUFFER_SIZE/8;
      break;
    case 768000:
      outputsamples=BUFFER_SIZE/16;
      break;
    case 1536000:
      outputsamples=BUFFER_SIZE/32;
      break;
  }
*/




  args.size=0;

  // initialize the radio
fprintf(stderr,"lime_protocol: receive_thread: SoapySDRDevice_make\n");
  lime_device=SoapySDRDevice_make(radio->info.soapy.args);
  if(lime_device==NULL) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_make failed: %s\n",SoapySDRDevice_lastError());
    _exit(-1);
  }

fprintf(stderr,"lime_protocol: set antenna to NONE\n");
  lime_protocol_set_antenna(0);

fprintf(stderr,"lime_protocol: setting samplerate=%f\n",(double)sample_rate);
  rc=SoapySDRDevice_setSampleRate(lime_device,SOAPY_SDR_RX,lime_receiver,(double)sample_rate);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setSampleRate(%f) failed: %s\n",(double)sample_rate,SoapySDRDevice_lastError());
  }

  actual_rate=(int)SoapySDRDevice_getSampleRate(lime_device, SOAPY_SDR_RX, lime_receiver);
fprintf(stderr,"lime_protocol: actual samplerate= %d\n",actual_rate);
#ifdef RESAMPLE
if(sample_rate==768000 && actual_rate==767999) {
  actual_rate=768000;
  fprintf(stderr,"lime_protocol: forced actual_rate\n");
}
#endif

fprintf(stderr,"lime_protocol: setting bandwidth =%f\n",bandwidth);
  rc=SoapySDRDevice_setBandwidth(lime_device,SOAPY_SDR_RX,lime_receiver,bandwidth);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setBandwidth(%f) failed: %s\n",bandwidth,SoapySDRDevice_lastError());
  }

if(saved_frequency!=0LL) {
fprintf(stderr,"lime_protocol: setting save_frequency: %lld\n",saved_frequency);
    lime_protocol_set_frequency(saved_frequency);
}

/*
fprintf(stderr,"lime_protocol: set baseband frequency\n");
  rc=SoapySDRDevice_setFrequencyComponent(lime_device,SOAPY_SDR_RX,lime_receiver,"BB",0.0,&args);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setFrequencyComponent(BB) failed: %s\n",SoapySDRDevice_lastError());
  }
*/
  
fprintf(stderr,"setting antennal to LNAL\n");
  lime_protocol_set_antenna(2);

fprintf(stderr,"setting Gain LNA=30.0\n");
  rc=SoapySDRDevice_setGainElement(lime_device,SOAPY_SDR_RX,lime_receiver,"LNA",30.0);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setGain LNA failed: %s\n",SoapySDRDevice_lastError());
  }
fprintf(stderr,"setting Gain PGA=19.0\n");
  rc=SoapySDRDevice_setGainElement(lime_device,SOAPY_SDR_RX,lime_receiver,"PGA",19.0);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setGain PGA failed: %s\n",SoapySDRDevice_lastError());
  }
fprintf(stderr,"setting Gain TIA=12.0\n");
  rc=SoapySDRDevice_setGainElement(lime_device,SOAPY_SDR_RX,lime_receiver,"TIA",12.0);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setGain TIA failed: %s\n",SoapySDRDevice_lastError());
  }

fprintf(stderr,"lime_protocol: receive_thread: SoapySDRDevice_setupStream\n");
  size_t channels=(size_t)lime_receiver;
  rc=SoapySDRDevice_setupStream(lime_device,&stream,SOAPY_SDR_RX,"CF32",&channels,1,&args);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setupStream failed: %s\n",SoapySDRDevice_lastError());
    _exit(-1);
  }

  max_samples=SoapySDRDevice_getStreamMTU(lime_device,stream);
fprintf(stderr,"max_samples=%d\n",max_samples);

  buffer=(float *)malloc(max_samples*sizeof(float)*2);

#ifdef RESAMPLE
  if(actual_rate!=sample_rate) {
fprintf(stderr,"lime_protocol: creating resampler from %d to %d\n",actual_rate,sample_rate);
    resampler=create_resample (1, max_samples, resamples, resampled, actual_rate, sample_rate, 0.0, 0, 1.0);
  }
#endif

  rc=SoapySDRDevice_activateStream(lime_device, stream, 0, 0LL, 0);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_activateStream failed: %s\n",SoapySDRDevice_lastError());
    _exit(-1);
  }


  if(saved_antenna!=-1) {
fprintf(stderr,"lime_protocol: setting save_antenna: %d\n",saved_antenna);
    lime_protocol_set_antenna(saved_antenna);
  }

  if(saved_frequency!=0LL) {
fprintf(stderr,"lime_protocol: setting save_frequency: %lld\n",saved_frequency);
      lime_protocol_set_frequency(saved_frequency);
  }

fprintf(stderr,"lime_protocol_init: audio_open_output\n");
  if(audio_open_output(receiver[0])!=0) {
    receiver[0]->local_audio=false;
    fprintf(stderr,"audio_open_output failed\n");
  }

fprintf(stderr,"lime_protocol_init: create receive_thread\n");
  receive_thread_id = g_thread_new( "lime protocol", receive_thread, NULL);
  if( ! receive_thread_id )
  {
    fprintf(stderr,"g_thread_new failed for receive_thread\n");
    exit( -1 );
  }
  fprintf(stderr, "receive_thread: id=%p\n",receive_thread_id);
}

static void *receive_thread(void *arg) {
  float isample;
  float qsample;
  int outsamples;
  int elements;
  int flags=0;
  long long timeNs=0;
  long timeoutUs=10000L;
  int i;
#ifdef TIMING
  struct timeval tv;
  long start_time, end_time;
  rate_samples=0;
  gettimeofday(&tv, NULL); start_time=tv.tv_usec + 1000000 * tv.tv_sec;
#endif
  running=1;
fprintf(stderr,"lime_protocol: receive_thread\n");
  while(running) {
    elements=SoapySDRDevice_readStream(lime_device,stream,(void *)&buffer,max_samples,&flags,&timeNs,timeoutUs);
//fprintf(stderr,"read %d elements\n",elements);
#ifdef RESAMPLE
    if(actual_rate!=lime_sample_rate) {
      for(i=0;i<elements;i++) {
        resamples[i*2]=(double)buffer[i*2];
        resamples[(i*2)+1]=(double)buffer[(i*2)+1];
      }

      outsamples=xresample(resampler);

      for(i=0;i<outsamples;i++) {
        add_iq_samples(receiver[0],(double)resampled[i*2],(double)resampled[(i*2)+1]);
      }
    } else {
#endif
      for(i=0;i<elements;i++) {
        add_iq_samples(receiver[0],(double)buffer[i*2],(double)buffer[(i*2)+1]);
      }
#ifdef RESAMPLE
    }
#endif
  }

fprintf(stderr,"lime_protocol: receive_thread: SoapySDRDevice_closeStream\n");
  SoapySDRDevice_closeStream(lime_device,stream);
fprintf(stderr,"lime_protocol: receive_thread: SoapySDRDevice_unmake\n");
  SoapySDRDevice_unmake(lime_device);

}


void lime_protocol_stop() {
  running=0;
}

void lime_protocol_set_frequency(long long f) {
  int rc;
  char *ant;

  if(lime_device!=NULL) {
    SoapySDRKwargs args;
    args.size=0;
fprintf(stderr,"lime_protocol: setFrequency: %lld\n",f);
    rc=SoapySDRDevice_setFrequency(lime_device,SOAPY_SDR_RX,lime_receiver,(double)f,&args);
    if(rc!=0) {
      fprintf(stderr,"lime_protocol: SoapySDRDevice_setFrequency() failed: %s\n",SoapySDRDevice_lastError());
    }
  } else {
fprintf(stderr,"lime_protocol: setFrequency: %lld device is NULL\n",f);
    saved_frequency=f;
  }
}

void lime_protocol_set_antenna(int ant) {
  int rc;
 // char *antstr;
  if(lime_device!=NULL) {
/*
    antstr=SoapySDRDevice_getAntenna(lime_device,SOAPY_SDR_RX,lime_receiver);
    fprintf(stderr,"lime_protocol: set_antenna: current antenna=%s\n",antstr);
*/
    switch(ant) {
      case 0:
fprintf(stderr,"lime_protocol: setAntenna: NONE\n");
        rc=SoapySDRDevice_setAntenna(lime_device,SOAPY_SDR_RX,lime_receiver,"NONE");
        if(rc!=0) {
          fprintf(stderr,"lime_protocol: SoapySDRDevice_setAntenna NONE failed: %s\n",SoapySDRDevice_lastError());
        }
        break;
      case 1:
fprintf(stderr,"lime_protocol: setAntenna: LNAH\n");
        rc=SoapySDRDevice_setAntenna(lime_device,SOAPY_SDR_RX,lime_receiver,"LNAH");
        if(rc!=0) {
          fprintf(stderr,"lime_protocol: SoapySDRDevice_setAntenna LNAH failed: %s\n",SoapySDRDevice_lastError());
        }
        break;
      case 2:
fprintf(stderr,"lime_protocol: setAntenna: LNAL\n");
        rc=SoapySDRDevice_setAntenna(lime_device,SOAPY_SDR_RX,lime_receiver,"LNAL");
        if(rc!=0) {
          fprintf(stderr,"lime_protocol: SoapySDRDevice_setAntenna LNAL failed: %s\n",SoapySDRDevice_lastError());
        }
        break;
      case 3:
fprintf(stderr,"lime_protocol: setAntenna: LNAW\n");
        rc=SoapySDRDevice_setAntenna(lime_device,SOAPY_SDR_RX,lime_receiver,"LNAW");
        if(rc!=0) {
          fprintf(stderr,"lime_protocol: SoapySDRDevice_setAntenna LNAW failed: %s\n",SoapySDRDevice_lastError());
        }
        break;
    }
/*
    antstr=SoapySDRDevice_getAntenna(lime_device,SOAPY_SDR_RX,lime_receiver);
    fprintf(stderr,"lime_protocol: set_antenna: antenna=%s\n",antstr);
*/
  } else {
    fprintf(stderr,"lime_protocol: setAntenna: device is NULL\n");
    saved_antenna=ant;
  }
}

void lime_protocol_set_attenuation(int attenuation) {
  int rc;
  fprintf(stderr,"setting Gain LNA=%f\n",30.0-(double)attenuation);
  rc=SoapySDRDevice_setGainElement(lime_device,SOAPY_SDR_RX,lime_receiver,"LNA",30.0-(double)attenuation);
  if(rc!=0) {
    fprintf(stderr,"lime_protocol: SoapySDRDevice_setGain LNA failed: %s\n",SoapySDRDevice_lastError());
  }
}

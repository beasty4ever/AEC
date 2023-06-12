#include <math.h>
#include "platform.h"

__declspec(dllexport) void aec_init();
__declspec(dllexport) void aec_filter_frame(float*,float*,float*);

#define AEC_MU    (int16)(0.05*32768)
#define AEC_ORDER 256

#define ALFAS 5  // 4ms    - 32  отсчета
#define ALFAM 7  // 16ms   - 128 отсчетов
#define ALFAL 14 // 2048ms - 16384 отсчетов

#define HANGOVER_TIME 1600 

int32 aec_w[AEC_ORDER];  
int32 aec_x[AEC_ORDER]; 

Uint32 TrTime;

int16  RumpUp,RumpDn;

Uint32 FlgCng,FlgSnd,FlgMic;
Uint32 CntCng,CntSnd,CntMic;

int32 OutGain,CngGain;

int32 ErrPowS,ErrPowM;
int32 ErrAccS,ErrAccM;

int32 SndPowS,SndPowM,SndPowL;
int32 SndAccS,SndAccM,SndAccL;

int32 MicAccS,MicAccM,MicAccL;
int32 MicPowS,MicPowM,MicPowL;

void aec_init() {
  Uint32  c0;  

  TrTime = 16000;     // 2 sec
  
  OutGain =    32767; // 1
  CngGain = 50*32767; // 50

  RumpUp  = 512;      // 1/64
  RumpDn  = 64;       // 1/512 


  FlgCng  = 0;
  FlgSnd  = 0;
  FlgMic  = 0;

  CntCng  = HANGOVER_TIME;
  CntSnd  = HANGOVER_TIME;
  CntMic  = HANGOVER_TIME;

  ErrPowS = 0;
  ErrPowM = 0;
  ErrAccS = 0;
  ErrAccM = 0;

  SndPowS = 0;
  SndPowM = 0;
  SndPowL = 0;
  SndAccS = 0;
  SndAccM = 0;
  SndAccL = 0;
  
  MicAccS = 0;
  MicAccM = 0;
  MicAccL = 0;
  MicPowS = 0;
  MicPowM = 0;
  MicPowL = 0;

  for(c0 = 0; c0 < AEC_ORDER; c0++) {
    aec_x[c0] = 0;
	aec_w[c0] = 0;
  }
}

static  int32 n = (int32)12357;  // Seed I(0) = 12357
int16 uran(void) {
  int16 ran;                     // Random noise r(n)
    
  n = 2045*n;                    // I(n) = 2045 * I(n-1) + 1
  n = n&0xFFFFF000;              // I(n) = I(n) - INT[I(n)/1048576] * 1048576
  ran = (int16)(n>>20);          // r(n) = FLOAT[I(n) + 1] / 1048577
  return ran;                    // Return r(n) to main function  
}

float32 aec_lms(int16 Mic,int16 Snd) {    
  Uint32  c0;

  int32 aec_y = 0;
  int16 aec_e = 0;
  int32 aec_c = 0;
   
  int16 Out;
  int32 Cng;

  int32 TrhSnd,TrhMic;
  int32 ClpSnd,ClpMic;
  
  int16 AbsErr;
  int16 AbsSnd = abs(Snd);
  int16 AbsMic = abs(Mic);

  // Усредняем уровен с динамика 
  SndAccS = SndAccS - SndPowS + AbsSnd;
  SndAccM = SndAccM - SndPowM + AbsSnd;
  
  if(SndPowL < SndPowS) {
    SndAccL = SndAccL - SndPowL + AbsSnd;
  }                 
  else {
    SndAccL = SndAccL - (SndAccL >> ALFAM) + AbsSnd;
  }  

  SndPowS = SndAccS >> ALFAS;
  SndPowM = SndAccM >> ALFAM;
  SndPowL = SndAccL >> ALFAL;

  // Усредняем уровен с микрофона 
  MicAccS = MicAccS - MicPowS + AbsMic;
  MicAccM = MicAccM - MicPowM + AbsMic;
  
  if(MicPowL < MicPowS) {
    MicAccL = MicAccL - MicPowL + AbsMic;
  }                 
  else {
    MicAccL = MicAccL - (MicAccL >> ALFAM) + AbsMic;
  }  

  MicPowS = MicAccS >> ALFAS;
  MicPowM = MicAccM >> ALFAM;
  MicPowL = MicAccL >> ALFAL;

  // Фильтруем сигнал с динамика
  for(c0 = 0; c0 < (AEC_ORDER-1); c0++) {
    aec_x[c0] = aec_x[c0+1];
  }
  aec_x[AEC_ORDER - 1] = Snd;
  
  aec_y = 0;
  for(c0 = 0; c0 < AEC_ORDER; c0++) {
	aec_y = aec_y + ((int32)aec_x[c0])*aec_w[c0];
  }
  aec_e  = (int16)((aec_y + 0x4000)>>15);   
  
  // Считеам ошибку
  aec_e = Mic - aec_e;

  // Усредняем ошибку
  AbsErr  = abs(aec_e);
  ErrAccS = ErrAccS - ErrPowS + AbsErr;
  ErrAccM = ErrAccM - ErrPowM + AbsErr;
 
  ErrPowS = ErrAccS >> ALFAS;
  ErrPowM = ErrAccM >> ALFAM;

  // Считаем пороги для обнаружения речи со входов
  TrhSnd = (SndPowL + (SndPowL>>1) + 150);  
  TrhMic = (MicPowL + (MicPowL>>1) + 150)<<2;
  
  // Детектор речи со входов
  if(SndPowS > TrhSnd) {
    FlgSnd = 1;                
    CntSnd = HANGOVER_TIME;    
  }
  else {
	CntSnd = (CntSnd > 0)? (CntSnd - 1) : 0; 
    if(CntSnd == 0) FlgSnd = 0;
  }

  if((MicPowS > TrhMic) && (ErrPowS > TrhMic)) {
    FlgMic = 1;                
    CntMic = HANGOVER_TIME;    
  }
  else {
	CntMic = (CntMic > 0)? (CntMic - 1) : 0; 
    if(CntMic == 0) FlgMic = 0;
  }

  // NLP процессор
  ClpMic = MicPowM/4;  
  ClpSnd = SndPowM/4;         

  if (FlgSnd == 1) {
    if((FlgMic == 0) || (TrTime > 0)) {

      if (TrTime > 0) {
        TrTime = TrTime - 1;                            
        if(OutGain > 8192) OutGain = OutGain - RumpDn;                            
        Out = (int16)((OutGain*aec_e) >> 15); 
      }
      
      if((ErrPowM < ClpMic) || (ErrPowM < ClpSnd)) {    
		FlgCng = 1;
		CntCng = HANGOVER_TIME; 
        if(CngGain > (ErrPowM + (ErrPowM>>1))) CngGain = ErrPowM; 
	  }
	  else {
	    CntCng = (CntCng > 0)? (CntCng - 1) : 0; 
        if(CntCng == 0) FlgCng = 0;
	  }

	  if(FlgCng == 1) {  
		Cng = (CngGain*uran()) >> 15;
        Out = (int16)((OutGain*Cng) >> 15);  
	  }
	  else {
        if(OutGain > 8192) OutGain = OutGain - RumpDn;                            
        Out = (int16)((OutGain*aec_e) >> 15);  
      }
      
      if(SndPowM < 16000) { 
        // LMS алгоритм обновления коэффициентов
        aec_c = (SndPowL*32767)/5;   

        if(aec_c > 8*32767) aec_c = 8*32767;
        if(aec_c < 1*32767) aec_c = 1*32767;
  
        aec_c = ((SndPowM + 150)*aec_c) >> 15;

        aec_c = ((int32)aec_e * AEC_MU) / aec_c;  

        for(c0 = 0; c0 < AEC_ORDER; c0++) {
          aec_w[c0] = ((aec_w[c0] << 15) + (aec_c*aec_x[c0]) + 0x4000)>>15;
        }
      }
    }
    else {
      if(OutGain > 16384) OutGain = OutGain - RumpDn;  
      if(OutGain < 16384) OutGain = OutGain + RumpUp;
      Out = (int16)((OutGain*Mic) >> 15);  
    }
  }
  else {
    if(FlgMic == 1) {
      if(OutGain < 32767) OutGain = OutGain + RumpUp;
      Out = (int16)((OutGain*Mic) >> 15); 
    }
    else {
      if(OutGain > 16384) OutGain = OutGain - RumpDn;  
      if(OutGain < 16384) OutGain = OutGain + RumpUp;      
	  
	  Out = (int16)((OutGain*Mic) >> 15); 
      CngGain = ErrPowM;
    }
  }
 
  return ((float32)(Out));
}

void aec_filter_frame(float *MicIn,float *SndIn,float *MicOut) {
  Uint32 c0;
  
  for(c0 = 0; c0 < 160; c0++) {
	MicOut[c0] = aec_lms((int16)MicIn[c0],(int16)SndIn[c0]);
  }
}

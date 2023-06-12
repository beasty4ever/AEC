#include <math.h>
#include "platform.h"

__declspec(dllexport) void aec_init();
__declspec(dllexport) void aec_filter_frame(float*,float*,float*);

#define AEC_MU    (0.01f/32767.0f)
#define AEC_ORDER 1024

#define WINDS  32.0f   //  4 ms
#define WINDM 128.0f   // 16 ms
#define WINDL 16000.0f //  2  s

#define ALFAS (1.0f/WINDS)
#define ALFAM (1.0f/WINDM)
#define ALFAL (1.0f/WINDL)

#define HANGOVER_TIME 1600 // 200 ms

float32 aec_w[AEC_ORDER];  
float32 aec_x[AEC_ORDER]; 

Uint32  TrTime;

float32 RumpUp,RumpDn;

Uint32  FlgSnd,FlgMic;
Uint32  CntSnd,CntMic;

float32 OutGain,CngGain;

float32 ErrPowS,ErrPowM;
float32 SndPowS,SndPowM,SndPowL;
float32 MicPowS,MicPowM,MicPowL;

void aec_init() {
  Uint32  c0;  

  TrTime = 16000;
  
  OutGain =  1.0f;
  CngGain = 50.0f;

  RumpUp  = 1/80.0f;
  RumpDn  = 1/800.0f;

  FlgSnd  = 0;
  FlgMic  = 0;
  CntSnd  = HANGOVER_TIME;
  CntMic  = HANGOVER_TIME;

  ErrPowS = 0.0f;
  ErrPowM = 0.0f;
  SndPowS = 0.0f;
  SndPowM = 0.0f;
  SndPowL = 0.0f;
  MicPowS = 0.0f;
  MicPowM = 0.0f;
  MicPowL = 0.0f;

  for(c0 = 0; c0 < AEC_ORDER; c0++) {
    aec_x[c0] = 0.0f;
	aec_w[c0] = 0.0f;
  }
}

static  int32  n = (int32)12357;   // Seed I(0) = 12357
float32 uran(void) {
  int16 ran;                     // Random noise r(n)
    
  n = 2045*n;                    // I(n) = 2045 * I(n-1) + 1
  n = n&0xFFFFF000;              // I(n) = I(n) - INT[I(n)/1048576] * 1048576
  ran = (int16)(n>>20);          // r(n) = FLOAT[I(n) + 1] / 1048577
  return ((float32)ran / 32767.0f); // Return r(n) to main function
  
}

float32 aec_lms(float32 Mic,float32 Snd) {    
  Uint32  c0;
  
  float32 aec_e;
  float32 aec_c;
  
  float32 Out;

  float32 TrhSnd,TrhMic;
  float32 ClpSnd,ClpMic;

  float32 AbsSnd = fabsf(Snd);
  float32 AbsMic = fabsf(Mic);

  // Усредняем уровен с динамика 
  SndPowS = (1.0f - ALFAS)*SndPowS  + ALFAS*AbsSnd;
  SndPowM = (1.0f - ALFAM)*SndPowM  + ALFAM*AbsSnd;

  if(SndPowL < SndPowS) {
    SndPowL = (1.0f - ALFAL)*SndPowL  + ALFAL*AbsSnd;
  }                 
  else {
    SndPowL = (1.0f - ALFAM)*SndPowL  + ALFAM*AbsSnd;
  }  

  // Усредняем уровен с динамика 
  MicPowS = (1.0f - ALFAS)*MicPowS  + ALFAS*AbsMic;
  MicPowM = (1.0f - ALFAM)*MicPowM  + ALFAM*AbsMic;

  if(MicPowL < MicPowS) {
    MicPowL = (1.0f - ALFAL)*MicPowL  + ALFAL*AbsMic;
  }                 
  else {
    MicPowL = (1.0f - ALFAM)*MicPowL  + ALFAM*AbsMic;
  } 

  // Фильтруем сигнал с динамика
  for(c0 = 0; c0 < (AEC_ORDER-1); c0++) {
    aec_x[c0] = aec_x[c0+1];
  }
  aec_x[AEC_ORDER - 1] = Snd;
  
  aec_e = 0.0f;
  for(c0 = 0; c0 < AEC_ORDER; c0++) {
	aec_e = aec_e + aec_x[c0]*aec_w[c0];
  }
   
  // Считеам ошибку
  aec_e = Mic - aec_e;     

  // Усредняем ошибку 
  ErrPowS = (1.0f - ALFAS)*ErrPowS  + ALFAS*fabsf(aec_e);
  ErrPowM = (1.0f - ALFAM)*ErrPowM  + ALFAM*fabsf(aec_e);  

  // Считаем пороги для обнаружения речи со входов
  TrhSnd = (1.414f*SndPowL + 150.0f);  
  TrhMic = (1.414f*MicPowL + 150.0f)*4.0f;
  
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
  ClpMic = MicPowM/(4.0f*1.414f);  
  ClpSnd = SndPowM/(8.0f);         
 
  if (FlgSnd == 1) {
    if((FlgMic == 0) || (TrTime > 0)) {

      if (TrTime > 0) {
        TrTime = TrTime - 1;                            
        if(OutGain > 0.25) OutGain = OutGain - RumpDn;                            
        Out = OutGain*aec_e;                          
      }
      
      if((ErrPowM < ClpMic) || (ErrPowM < ClpSnd)) {                                        
        if(CngGain > (1.414f*ErrPowM)) CngGain = ErrPowM;                       		
        Out = OutGain*CngGain*(uran() - 0.5f);  
	  }
	  else {
        if(OutGain > 0.25) OutGain = OutGain - RumpDn;                            
        Out = OutGain*aec_e; 
      }
      
      if(SndPowM < 16000.0f) { 
        aec_c = SndPowL/5.0f;               
        if (aec_c > 8.0f) aec_c = 8.0f;
        if (aec_c < 1.0f) aec_c = 1.0f;

        aec_c = (aec_e*AEC_MU)/((SndPowM + 150.0f)*aec_c);  
        
        for(c0 = 0; c0 < AEC_ORDER; c0++) {
          aec_w[c0] = (0.999969482f*aec_w[c0]) + (aec_c*aec_x[c0]);
        }
      }
    }
    else {
      if(OutGain > 0.5f) OutGain = OutGain - RumpDn;  
      if(OutGain < 0.5f) OutGain = OutGain + RumpUp;
      Out = OutGain*aec_e; 
    }
  }
  else {
    if(FlgMic == 1) {
      if(OutGain < 1.0f) OutGain = OutGain + RumpUp;
      Out = OutGain*Mic;                               
    }
    else {
      if(OutGain > 0.5f) OutGain = OutGain - RumpDn;  
      if(OutGain < 0.5f) OutGain = OutGain + RumpUp;      
	  
	  Out = OutGain*Mic; 
      CngGain = ErrPowM;
    }
  }
  return Out;
}

void aec_filter_frame(float *MicIn,float *SndIn,float *MicOut) {
  Uint32 c0;
  
  for(c0 = 0; c0 < 160; c0++) {
	MicOut[c0] = aec_lms(MicIn[c0],SndIn[c0]);
  }
}

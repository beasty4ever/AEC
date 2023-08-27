#include <string.h>
#include "platform.h"

//#define ARM_MATH_CM4

#ifdef ARM_MATH_CM4
#include "arm_math.h"
#else
int32 __SMLAD(int32 x,int32 y,int32 sum) {
  return (sum + (((int16)(x>>16))*((int16)(y >> 16))) + (((int16) x) * ((int16) y)));
}

__declspec(dllexport) void aec_init();
__declspec(dllexport) void aec_filter_frame(float*,float*,float*);
#endif

#define AEC_MU    (int16)(0.05*32768)
#define AEC_ORDER 256

#define ALFAS 5  // 4ms    - 32  �������
#define ALFAM 7  // 16ms   - 128 ��������
#define ALFAL 14 // 2048ms - 16384 ��������

#define HANGOVER_TIME 1600 

int16  aec_w[AEC_ORDER];  
int16  aec_x[AEC_ORDER]; 

Uint32 TrTime;

Uint32 FlgCng,FlgSnd,FlgMic;
Uint32 CntCng,CntSnd,CntMic;

int32 SeedCng;

int32 RumpUp,RumpDn;
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
  
  SeedCng = 12357;  

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

int16 uran() {
  int16 ran;          
  SeedCng = 2045*SeedCng;     
  SeedCng = SeedCng&0xFFFFF000;
  ran = (int16)(SeedCng>>20);          
  return ran;                    
}

int16 aec_lms(int16 Mic,int16 Snd) {    
  Uint32  c0;
    
  int32 *aec_px;
  int32 *aec_pw;
  
  int16 aec_e = 0;
  int32 aec_y = 0;  
  int32 aec_c = 0;
   
  int16 Out;
  int16 Cng;

  int32 TrhSnd,TrhMic;
  int32 ClpSnd,ClpMic;
  
  int16 AbsErr;
  int16 AbsSnd = (Snd > 0)? Snd : (-Snd);
  int16 AbsMic = (Mic > 0)? Mic : (-Mic);

  // ��������� ������ � �������� 
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

  // ��������� ������ � ��������� 
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

  // ��������� ������ � ��������  
  memcpy(&aec_x[0],&aec_x[1],(AEC_ORDER-1)*sizeof(int16));
  aec_x[AEC_ORDER - 1] = Snd;  

  aec_y = 0;  
  aec_px = (int32*)&aec_x[0];
  aec_pw = (int32*)&aec_w[0];
  for(c0 = 0; c0 < (AEC_ORDER/2); c0+=4) {        
    aec_y = __SMLAD(aec_px[c0+0],aec_pw[c0+0],aec_y);    
    aec_y = __SMLAD(aec_px[c0+1],aec_pw[c0+1],aec_y);   
    aec_y = __SMLAD(aec_px[c0+2],aec_pw[c0+2],aec_y);   
    aec_y = __SMLAD(aec_px[c0+3],aec_pw[c0+3],aec_y);   
  }     
  aec_e = (int16)((aec_y + 0x4000)>>15);   

  // ������� ������
  aec_e = Mic - aec_e; 

  // ��������� ������
  AbsErr  = (aec_e > 0)? aec_e : (-aec_e);
  ErrAccS = ErrAccS - ErrPowS + AbsErr;
  ErrAccM = ErrAccM - ErrPowM + AbsErr;
 
  ErrPowS = ErrAccS >> ALFAS;
  ErrPowM = ErrAccM >> ALFAM;

  // ������� ������ ��� ����������� ���� �� ������
  TrhSnd = (SndPowL + (SndPowL>>1) + 150);  
  TrhMic = (MicPowL + (MicPowL>>1) + 150)<<2;
  
  // �������� ���� �� ������
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

  // NLP ���������
  ClpMic = MicPowM/4; // 12db 
  ClpSnd = SndPowM/4; // 12db         

  if (FlgSnd == 1) {
    if((FlgMic == 0) || (TrTime > 0)) {

      if (TrTime > 0) TrTime = TrTime - 1;                            
      
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
		Cng = (int16)((CngGain*uran()) >> 15);
        Out = (int16)((OutGain*Cng) >> 15);  
	  }
	  else {
        if(OutGain > 8192) OutGain = OutGain - RumpDn;                            
        Out = (int16)((OutGain*aec_e) >> 15);  
      }
      
      if(SndPowM < 16000) { 
        // LMS �������� ���������� �������������
        aec_c = (SndPowL*32767)/5;   

        if(aec_c > 8*32767) aec_c = 8*32767;
        if(aec_c < 1*32767) aec_c = 1*32767;
  
        aec_c = ((SndPowM + 150)*aec_c) >> 15;

        aec_c = ((int32)aec_e * AEC_MU) / aec_c;  

        for(c0 = 0; c0 < (AEC_ORDER); c0+=4) {
          aec_w[c0+0] = (int16)(((((int32)aec_w[c0+0]) << 15) + (aec_c*aec_x[c0+0]) + 0x4000)>>15);
	      aec_w[c0+1] = (int16)(((((int32)aec_w[c0+1]) << 15) + (aec_c*aec_x[c0+1]) + 0x4000)>>15);
	      aec_w[c0+2] = (int16)(((((int32)aec_w[c0+2]) << 15) + (aec_c*aec_x[c0+2]) + 0x4000)>>15);
	      aec_w[c0+3] = (int16)(((((int32)aec_w[c0+3]) << 15) + (aec_c*aec_x[c0+3]) + 0x4000)>>15);
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
 
  return Out;
}

void aec_filter_frame(float *MicIn,float *SndIn,float *MicOut) {
  Uint32 c0;
  
  for(c0 = 0; c0 < 160; c0++) {
	MicOut[c0] = (float32) aec_lms((int16)MicIn[c0],(int16)SndIn[c0]);
  }
}

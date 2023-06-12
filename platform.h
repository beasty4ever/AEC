#ifndef PLATFORM_H
#define PLATFORM_H

//---------------------------------------------------------------------------
// Определение платформо-независимых типов данных
typedef char            int8;    
typedef short           int16;
typedef long            int32;
typedef unsigned char   Uint8;   
typedef unsigned short  Uint16;
typedef unsigned long   Uint32;
typedef float           float32;
typedef long double     float64; 

typedef enum {
   false = 0,
   true  = 1
}BOOL;

#endif


import wave
import os.path
import numpy  as np
import pylab  as pl
import ctypes as ct
import sounddevice as sd

fmic = 'rtmic'
fsnd = 'rtfar'

dll_name = "aeclib.dll"
dllabspath = os.path.dirname(os.path.abspath(__file__)) + os.path.sep + dll_name

aeclib = ct.CDLL(dllabspath)

aeclib.aec_filter_frame.argtypes = [np.ctypeslib.ndpointer(np.float32), np.ctypeslib.ndpointer(np.float32), np.ctypeslib.ndpointer(np.float32)]

aeclib.aec_init()

FRAME_SIZE = 160 # Количество отсчетов в 20ms на частоте Fs = 8000Hz

# Считываем файл с микрофона Fs = 8000Hz
fp = wave.open(fmic + '.wav', 'rb')
inputLength = fp.getnframes()
ain_str = fp.readframes(inputLength)
fp.close()

# Считаем длину файла кратную 20ms если не хватает дописываем 0
mic_buf = np.frombuffer(ain_str,np.int16).astype(np.float64)
rest = (inputLength - FRAME_SIZE*np.floor(inputLength/FRAME_SIZE)).astype(np.int32)

if rest == 0:
  numberOfFrames = np.floor(inputLength/FRAME_SIZE).astype(np.int32) 
else :  
  numberOfFrames = (np.floor(inputLength/FRAME_SIZE) + 1).astype(np.int32) 
  mic_buf = np.concatenate((mic_buf,np.zeros(FRAME_SIZE-rest)))
 
# Считываем файл с динамика Fs = 8000Hz
fp = wave.open(fsnd + '.wav', 'rb')
inputLength = fp.getnframes()
ain_str = fp.readframes(inputLength)
fp.close()

# Считаем длину файла кратную 20ms если не хватает дописываем 0
snd_buf = np.frombuffer(ain_str,np.int16).astype(np.float64)
rest = (inputLength - FRAME_SIZE*np.floor(inputLength/FRAME_SIZE)).astype(np.int32)

if rest == 0:
  numberOfFrames = np.floor(inputLength/FRAME_SIZE).astype(np.int32) 
else :  
  numberOfFrames = (np.floor(inputLength/FRAME_SIZE) + 1).astype(np.int32) 
  snd_buf = np.concatenate((snd_buf,np.zeros(FRAME_SIZE-rest))) 
  
blockIndex  = np.arange(0,FRAME_SIZE)
signalIndex = np.arange(0,FRAME_SIZE*numberOfFrames)

mic_frame   = np.zeros(FRAME_SIZE,dtype = 'float32')
snd_frame   = np.zeros(FRAME_SIZE,dtype = 'float32')
out_frame   = np.zeros(FRAME_SIZE,dtype = 'float32')

out_buf     = np.zeros(FRAME_SIZE*numberOfFrames,dtype = 'float32')

# Блочная обработка входных фреймов
for frame_num in np.arange(0,numberOfFrames) :         
  mic_frame = mic_buf[blockIndex].astype(np.float32)  
  snd_frame = snd_buf[blockIndex].astype(np.float32)
  
  aeclib.aec_filter_frame(mic_frame,snd_frame,out_frame)
  
  out_buf[blockIndex] = out_frame
  
  blockIndex = blockIndex + FRAME_SIZE

# Конец обработки и вывод результатов
pl.figure(1)
# Выводим сигнал
pl.subplot(3,1,1)
pl.plot(signalIndex,mic_buf,'r')
pl.grid(True)

pl.subplot(3,1,2)
pl.plot(signalIndex,snd_buf,'r')
pl.grid(True)

pl.subplot(3,1,3)
pl.plot(signalIndex,out_buf,'r')
pl.grid(True)

pl.show()

# Проигрываем выходной массив
sd.play(1.0*(mic_buf/32767),8000)
input("Нажмите Enter...")

# Проигрываем выходной массив
sd.play(1.0*(out_buf/32767),8000)
input("Нажмите Enter...")

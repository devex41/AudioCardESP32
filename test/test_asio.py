import sys
sys.stdout.reconfigure(encoding='utf-8')
import numpy as np
import sounddevice as sd

# Настройки сигнала
fs = 48000       # Частота дискретизации
freq = 255.0     # Частота синуса (A4)
amplitude = 0.5  # Амплитуда сигнала (0.0-1.0)
block_size = 64  # Размер блока для потоковой передачи

# Индекс устройства виртуального кабеля
device_index = 14  # WASAPI: CABLE Input (VB-Audio Virtual Cable)

# Фаза для непрерывного сигнала
phase = 0.0

def callback(outdata, frames, time, status):
    global phase
    if status:
        print(status)
    t = (np.arange(frames) + phase) / fs
    outdata[:] = amplitude * np.sin(2 * np.pi * freq * t).reshape(-1,1)
    phase += frames
    phase %= fs  # избегаем переполнения

# Создаем поток вывода
with sd.OutputStream(device=device_index, channels=1,
                     samplerate=fs, blocksize=block_size,
                     callback=callback):
    print("Сигнал синуса идет в Guitar Rig. Нажми Ctrl+C для остановки.")
    try:
        while True:
            sd.sleep(1000)
    except KeyboardInterrupt:
        print("\nПоток остановлен.")
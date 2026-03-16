import sys
import numpy as np
import sounddevice as sd
import serial
import queue
import time 

sys.stdout.reconfigure(encoding='utf-8')

# --- Настройки ---
PORT = "COM3"
BAUD = 1200000
FS = 48000

SAMPLES_PER_PACKET = 64 
# 64 сэмпла * 2 байта + 2 байта стоп (\r\n) = 130 байт
PACKET_SIZE = 130 

# Очередь (увеличим до 50 для стабильности, если будет задержка — уменьшим)
data_queue = queue.Queue(maxsize=500)

# --- Настройки Мока ---
MOCK_FS = 48000
MOCK_FREQ = 255.0
phase_accumulator = 0

def mock_audio_callback(outdata, frames, time_info, status):
    global phase_accumulator
    if status:
        print(f"Status: {status}", flush=True)

    t = (np.arange(frames) + phase_accumulator) / MOCK_FS
    mock_data = 0.5 * np.sin(2 * np.pi * MOCK_FREQ * t)
    print(mock_data)
    outdata[:] = mock_data.reshape(-1, 1).astype(np.float32)
    phase_accumulator += frames
    phase_accumulator %= MOCK_FS 


def audio_callback(outdata, frames, time, status):
    if status:
        print(f"Status: {status}", flush=True)
    try:
        # Пытаемся взять данные. frames всегда 64, так как blocksize=64
        # print(data_queue.qsize())
        data = data_queue.get_nowait()
        outdata[:] = data.reshape(-1, 1)
    except queue.Empty:
        print("empty")
        outdata.fill(0) # Если данных нет — тишина, а не треск

# --- НОВАЯ ФУНКЦИЯ ПОИСКА УСТРОЙСТВА ---
def find_vb_cable_index():
    devices = sd.query_devices()
    hostapis = sd.query_hostapis()
    
    for i, dev in enumerate(devices):
        # Нам нужно устройство вывода (output_channels > 0)
        if dev['max_output_channels'] > 0:
            dev_name = dev['name'].lower()
            hostapi_name = hostapis[dev['hostapi']]['name'].lower()
            
            # Ищем ключевые слова "cable" (от VB-Audio) и "wasapi"
            if 'cable' in dev_name and 'wasapi' in hostapi_name:
                return i
    return None


def main():
    # 1. Ищем нужное аудиоустройство
    device_index = find_vb_cable_index()
    
    if device_index is None:
        print("Ошибка: Виртуальный кабель 'VB-Audio Virtual Cable' (WASAPI) не найден!")
        print("Доступные устройства вывода:")
        # Выведем список, чтобы было проще отладить, если название отличается
        print(sd.query_devices())
        return
        
    dev_info = sd.query_devices(device_index)
    print(f"Успешно найдено устройство: {dev_info['name']} (Индекс: {device_index})")

    # 2. Подключаем Serial
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        ser.set_buffer_size(96000*10)
        ser.flushInput()
        print(f"Подключено к {PORT}. Трансляция в Guitar Rig...")
    except Exception as e:
        print(f"Ошибка порта: {e}")
        return

    # Запускаем поток вывода. Используем найденный device_index
    with sd.OutputStream(device=device_index, channels=1, 
                         samplerate=FS, blocksize=SAMPLES_PER_PACKET, 
                         callback=audio_callback, dtype='float32'):
        try:
            ser.reset_input_buffer()
            ser.read_until(b'\xff\xff')
            b = time.perf_counter_ns()
            
            while True:
                count = ser.in_waiting // 130
                raw_data = ser.read(count*130)
                
                for i in range(count):
                    if raw_data[(i*130)+128 : i*130+128+2] != b'\xff\xff':
                        a =  time.perf_counter_ns() - b
                        print(a/1000000)
                        print(len(ser.read_all()))
                        print("Потеряна синхронизация (битый пакет)")
                        ser.reset_input_buffer()
                        b = time.perf_counter_ns()
                        ser.read_until(b'\xff\xff')
                        break
                    
                    ints = np.frombuffer(raw_data[i*130:(i*130)+128], dtype='<u2')
                    floats = (ints.astype(np.float32) - 921) / 2048.0
                    floats = np.clip(floats, -1.0, 1.0)
                    # print(floats)
                    
                    data_queue.put(floats)

        except KeyboardInterrupt:
            print("\nОстановка...")
        finally:
            ser.close()

if __name__ == "__main__":
    main()
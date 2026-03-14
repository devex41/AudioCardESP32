import sys
sys.stdout.reconfigure(encoding='utf-8')
import numpy as np
import sounddevice as sd

# --- НАСТРОЙКИ ---
fs = 48000
amplitude = 0.4
device_index = 14 # Убедись, что это твой VB-Cable
sixteenth_note = 0.15  # Длительность 1/16 ноты

# Частоты нот по ладам (Стандартный строй гитары)
# Струна D (4-я): лад 0=146.8, 2=164.8, 4=185.0, 5=196.0
# Струна G (3-я): лад 0=196.0, 2=220.0, 4=246.9
# Струна A (5-я): лад 0=110.0, 2=123.5
NOTES = {
    'D0': 146.8, 'D2': 164.8, 'D4': 185.0, 'D5': 196.0,
    'G0': 196.0, 'G2': 220.0, 'G4': 246.9,
    'A0': 110.0, 'A2': 123.5,
    '0': 0, 'D3': 146.83, 'E3': 164.81, 'H2': 246.94 
}

# Партитура по скриншоту (Нота, Длительность в 16-х долях)
# 1 четверть = 4 доли, 1 восьмая = 2 доли, 1/16 = 1 доля.
score = [
    # Такт 9-10
    ('D4', 8),              # Длинная F#
    ('A2', 8), ('D3', 1),    # B -> A
    
    # Такт 11: 5-4-2 на струне D (G-F#-E)
    ('E3', 1), ('D4', 8), ('A2', 8), ('D3', 1),
    
    # Такт 12: Повтор 5-4-2
    ('E3', 1), ('H2', 8), ('D2', 1), ('D5', 4),
    
    # Такт 13-14: Удержание ноты (Е)
    ('D2', 12), ('D2', 12),
    
    # Такт 15-16: 4 на D, потом 0-2-4 на G
    ('D4', 6), ('G2', 6),    # F# -> A
    ('G0', 2), ('G2', 2), ('G4', 8), # G -> A -> B
    
    # Такт 17-18: Спуск
    ('D4', 12), ('D4', 12),
    
    # Такт 19-21: Финальная фраза страницы
    ('G2', 6), ('G0', 6), 
    ('D5', 6), ('D4', 1), ('D2', 1), ('D5', 4),
    ('D5', 4), ('D4', 4), ('D2', 4)
]

def generate_guitar_string(freq, duration):
    samples = int(duration * fs)
    t = np.linspace(0, duration, samples, endpoint=False)
    if freq == 0: return np.zeros(samples)
    
    # Имитируем тембр гитары: основной тон + гармоники
    # Добавляем небольшое затухание амплитуды (экспоненциальное)
    decay = np.exp(-3 * t / duration)
    
    wave = (0.6 * np.sin(2 * np.pi * freq * t) + 
            0.3 * np.sin(4 * np.pi * freq * t) + 
            0.1 * np.sin(6 * np.pi * freq * t))
    
    wave *= decay * amplitude
    
    # Резкая атака в начале (щелчок медиатора)
    attack = int(fs * 0.01)
    if samples > attack:
        wave[:attack] *= np.linspace(0, 1, attack)
        
    return wave

# Сборка всей мелодии
melody_array = np.concatenate([generate_guitar_string(NOTES[n], d * sixteenth_note) for n, d in score]).astype(np.float32)

# Воспроизведение
current_pos = 0
def callback(outdata, frames, time, status):
    global current_pos
    chunk_end = current_pos + frames
    if chunk_end > len(melody_array):
        chunk1 = melody_array[current_pos:]
        remain = frames - len(chunk1)
        outdata[:len(chunk1)] = chunk1.reshape(-1, 1)
        outdata[len(chunk1):] = melody_array[:remain].reshape(-1, 1)
        current_pos = remain
    else:
        outdata[:] = melody_array[current_pos:chunk_end].reshape(-1, 1)
        current_pos = chunk_end

try:
    with sd.OutputStream(device=device_index, channels=1, samplerate=fs, callback=callback):
        print("--- Мелодия по табам Songsterr запущена ---")
        print("Синхронизация: 3/4, темп умеренный. Ctrl+C для выхода.")
        while True: sd.sleep(1000)
except KeyboardInterrupt:
    print("\nОстановка.")
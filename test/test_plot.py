import serial
import numpy as np
import matplotlib.pyplot as plt
import sys
sys.stdout.reconfigure(encoding='utf-8')

PORT = "COM3"
BAUD = 1200000

BLOCK_BYTES = 64*2
SAMPLES_PER_BLOCK = BLOCK_BYTES // 2
TOTAL_SAMPLES = 64 * 100 *2

ser = serial.Serial(PORT, BAUD, timeout=1)
ser.set_buffer_size(4096)


# ждём первый \n
ser.read_until(b'\xff\xff')

samples = []

while len(samples) < TOTAL_SAMPLES:
    block = ser.read(BLOCK_BYTES+2)
    # block = ser.read_until(b'\xff\xff')
    # print(len(block))
    # print('------------------------------------')
    #block = ser.read_until(bytes(4095))
    
    # print(block)
    block = block[:-2]
    # print(block)
    # print('------------------------------------')
    if len(block) != BLOCK_BYTES:
       
        print(len(block), 'qq')
        continue

    values = np.frombuffer(block, dtype='<u2')  # little-endian uint16
    print(values)
    samples.extend(values)


ser.close()

print("\nread ready")

data = np.array(samples[:TOTAL_SAMPLES])

print("Min:", data.min())
print("Max:", data.max())

plt.figure(figsize=(12,4))
plt.plot(data)
plt.title("48000 ADC samples")
plt.xlabel("Sample")
plt.ylabel("ADC value")
plt.grid(True)
plt.show()
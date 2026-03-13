import serial
import time

# Настройки порта
PORT = "COM3"        # например COM3 (Windows) или /dev/ttyUSB0 (Linux)
BAUDRATE = 1200000

ser = serial.Serial(PORT, BAUDRATE, timeout=0)

while True:
    start = time.time()
    byte_count = 0

    # измеряем 100 мс
    while (time.time() - start) < 1:
        data = ser.read(ser.in_waiting or 1)
        byte_count += len(data)

    print(f"Bytes received in 100 ms: {byte_count}")
import serial
import time
import sys
sys.stdout.reconfigure(encoding='utf-8')

# Настройки порта
PORT = "COM3"        # например COM3 (Windows) или /dev/ttyUSB0 (Linux)
BAUDRATE = 1200000

ser = serial.Serial(PORT, BAUDRATE, timeout=0)

while True:
    start = time.time()
    byte_count = 0

    # измеряем 100 мс
    while (time.time() - start) < 10:
        data = ser.read_all()
        # print(data)
        byte_count += len(data)

    print(f"Bytes received : {(byte_count - ((byte_count/130)*2))/10}")
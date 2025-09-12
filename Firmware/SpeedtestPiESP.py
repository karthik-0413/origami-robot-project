import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
data = b"x" * 64  # 64 bytes per write
count = 0
start = time.time()

while time.time() - start < 10:
    ser.write(data)
    count += 1

elapsed = time.time() - start
print(f"Sent {count*64} bytes in {elapsed:.2f}s, approx throughput = {count*64/elapsed:.2f} B/s")
# 11573.56 B per second over USB-UART
import serial
import time

SERIAL_PORT = "COM3"       # Make sure Serial Monitor is closed
BAUD_RATE = 115200         # Match Arduino sketch
IMAGE_COUNT = 0            # To keep track of saved images

# Open serial port
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)  # Wait for ESP32 to reset

print("Listening for images...")

while True:
    line = ser.readline().decode('latin1').strip()
    if line.startswith("IMG_LEN:"):
        length = int(line.split(":")[1])
        print(f"Receiving image of length {length} bytes...")

        img_bytes = bytearray()
        while len(img_bytes) < length:
            chunk = ser.read(length - len(img_bytes))
            if chunk:
                img_bytes.extend(chunk)

        # Wait for IMG_DONE marker
        done_marker = ser.readline().decode('latin1').strip()
        if done_marker == "IMG_DONE":
            IMAGE_COUNT += 1
            filename = f"photo_{IMAGE_COUNT}.jpg"
            with open(filename, "wb") as f:
                f.write(img_bytes)
            print(f"Saved {filename}")

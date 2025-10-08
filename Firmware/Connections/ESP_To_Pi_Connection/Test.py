# receive_display.py
import serial
import time
import re
import sys
import cv2
import numpy as np

COM_PORT = "COM5"   # change if needed
BAUD = 115200
TIMEOUT = 1        # seconds read timeout

# Regex to parse header: <IMG_START:MACHEX:IMGID:SIZE>
HEADER_RE = re.compile(rb'^<IMG_START:([0-9A-Fa-f]{12}):([0-9]+):([0-9]+)>\s*$')

def read_exact(ser, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            # timeout or no data
            return None
        buf.extend(chunk)
    return bytes(buf)

def main():
    print(f"Opening {COM_PORT} at {BAUD} baud...")
    ser = serial.Serial(COM_PORT, BAUD, timeout=TIMEOUT)
    # use a small delay for the ESP to boot or for connection readiness
    time.sleep(0.5)
    print("Listening for images. Press Ctrl+C to quit.")
    partial = b""

    try:
        while True:
            # read a line (text) from serial to catch header lines or other messages
            line = ser.readline()
            if not line:
                # no line read - continue
                continue

            # Trim newline(s)
            line_stripped = line.strip()
            m = HEADER_RE.match(line_stripped)
            if m:
                mac = m.group(1).decode('ascii')
                imgid = int(m.group(2).decode('ascii'))
                size = int(m.group(3).decode('ascii'))
                print(f"Receiving image {imgid} from {mac} size {size} bytes")

                # Now read exact `size` bytes
                img_bytes = read_exact(ser, size)
                if img_bytes is None:
                    print("Failed to read expected number of bytes (timeout). Skipping.")
                    continue

                # Attempt to read trailing newline and <IMG_END> line (some senders print newline then marker)
                # read until we get a line that contains IMG_END or timeout
                # First consume a single possible newline
                ser.read(1)

                # Save to file
                filename = f"frame_{mac}_{imgid}.jpg"
                with open(filename, "wb") as f:
                    f.write(img_bytes)
                print(f"Saved {filename}")

                # Display using OpenCV
                try:
                    img_array = np.frombuffer(img_bytes, dtype=np.uint8)
                    img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                    if img is None:
                        print("OpenCV failed to decode image.")
                    else:
                        cv2.imshow(f"{mac}_{imgid}", img)
                        cv2.waitKey(1)  # needed to update window
                except Exception as e:
                    print("Error displaying image:", e)

            else:
                # not an img header; print text messages to console for debugging
                try:
                    text = line.decode('utf-8', errors='replace').strip()
                    if text:
                        print("[ESP]", text)
                except:
                    pass

    except KeyboardInterrupt:
        print("Exiting.")
    finally:
        ser.close()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()

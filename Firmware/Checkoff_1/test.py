import serial
import time
import re
import cv2
import numpy as np

COM_PORT = "COM3"
BAUD = 115200
TIMEOUT = 1

# Regex to match image headers: <IMG_START:TCP:<img_id>:<size>>
HEADER_RE = re.compile(rb'^<IMG_START:TCP:([0-9]+):([0-9]+)>\s*$')

def read_exact(ser, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            return None
        buf.extend(chunk)
    return bytes(buf)

def main():
    print(f"Opening {COM_PORT} at {BAUD} baud...")
    ser = serial.Serial(COM_PORT, BAUD, timeout=TIMEOUT)
    time.sleep(0.5)
    print("Listening for images. Press Ctrl+C to quit.")

    try:
        while True:
            line = ser.readline()
            if not line:
                continue

            line_stripped = line.strip()
            m = HEADER_RE.match(line_stripped)
            if m:
                imgid = int(m.group(1).decode('ascii'))
                size = int(m.group(2).decode('ascii'))
                # Determine camera index based on imgid parity (0 → ESP1, 1 → ESP2)
                cam_index = 0 if imgid % 2 == 1 else 1
                window_name = f"Camera {cam_index + 1}"

                print(f"Receiving image {imgid} from Camera {cam_index+1} ({size} bytes)")

                # Read image bytes
                img_bytes = read_exact(ser, size)
                if img_bytes is None:
                    print("Failed to read expected number of bytes. Skipping.")
                    continue

                # Consume trailing newline and <IMG_END>
                while True:
                    end_line = ser.readline()
                    if not end_line:
                        break
                    if b"<IMG_END>" in end_line:
                        break

                # Decode image
                img_array = np.frombuffer(img_bytes, dtype=np.uint8)
                img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                if img is None:
                    print(f"Failed to decode Camera {cam_index+1} image.")
                    continue

                # Display
                cv2.imshow(window_name, img)
                cv2.waitKey(1)

            else:
                # Print text messages from ESP (optional)
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

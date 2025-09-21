import serial
import socket
import time

# --- ESP32 via USB-UART ---
esp = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# --- Laptop via Ethernet ---
HOST = '192.168.100.1'  # Laptop IP
PORT = 5000
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

counter = 0
while True:
    msg = f"Message {counter} from Pi\n"

    # Send to ESP32
    esp.write(msg.encode())
    if esp.in_waiting > 0:
        data_esp = esp.readline().decode().strip()
        print("Received from ESP32:", data_esp)

    # Send to Laptop
    s.sendall(msg.encode())
    data_laptop = s.recv(1024)
    print("Received from Laptop:", data_laptop.decode().strip())

    counter += 1
    time.sleep(1)

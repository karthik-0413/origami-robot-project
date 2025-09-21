# Orange Pi 5 <-> ESP32 and Laptop UART Communication
import serial
import time

# UART to ESP32 (GPIO UART3)
esp = serial.Serial("/dev/ttyS3", 9600, timeout=1)

# UART to Laptop (USB-to-UART adapter)
laptop = serial.Serial("/dev/ttyUSB0", 9600, timeout=1)

counter = 0
while True:
    msg_esp = f"OrangePi->ESP32: {counter}\n"
    msg_laptop = f"OrangePi->Laptop: {counter}\n"

    # Send data
    esp.write(msg_esp.encode())
    laptop.write(msg_laptop.encode())
    
    print("Sent to ESP32:", msg_esp.strip())
    print("Sent to Laptop:", msg_laptop.strip())

    # Read from ESP32
    if esp.in_waiting > 0:
        data = esp.readline().decode().strip()
        if data:
            print("Received from ESP32:", data)

    # Read from Laptop
    if laptop.in_waiting > 0:
        data = laptop.readline().decode().strip()
        if data:
            print("Received from Laptop:", data)

    counter += 1
    time.sleep(1)

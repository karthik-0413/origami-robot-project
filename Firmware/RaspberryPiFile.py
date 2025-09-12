import serial, time

# Use the detected USB device
ser = serial.Serial("/dev/ttyUSB0", 9600, timeout=1)

counter = 0
while True:
    # Send data to ESP32
    msg = f"OrangePi says hello {counter}\n"
    ser.write(msg.encode())
    print("Sent:", msg.strip())

    # Read response from ESP32
    if ser.in_waiting > 0:
        data = ser.readline().decode().strip()
        if data:
            print("Received from ESP32:", data)

    counter += 1
    time.sleep(1)

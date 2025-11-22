"""
Sensor Data Receiver - Receives sensor data from ESP32 via Serial
Displays IMU and distance sensor readings in real-time
NO timestamps, Sender 2 has NO IMU
"""

import serial
import time
import csv
from datetime import datetime
import os

# Configuration
SERIAL_PORT = "COM3"
BAUD_RATE = 115200

# CSV logging
LOG_DIR = "sensor_logs"
os.makedirs(LOG_DIR, exist_ok=True)

# Data storage
sensor1_data = {
    'sensor1': False, 'sensor2': False, 'sensor3': False,
    'accelX': 0.0, 'accelY': 0.0, 'accelZ': 0.0,
    'gyroX': 0.0, 'gyroY': 0.0, 'gyroZ': 0.0
}

sensor2_data = {
    'sensor4': False, 'sensor5': False, 'sensor6': False
}

# Statistics
packet_count = {'sensor1': 0, 'sensor2': 0}
start_time = time.time()

def parse_sensor_packet(line):
    """Parse CSV sensor data from serial"""
    try:
        parts = line.strip().split(',')
        
        sensor_type = parts[0]
        
        # SENDER 1: Distance sensors + IMU (10 fields)
        if sensor_type == "SENSOR1" and len(parts) >= 10:
            sensor1_data['sensor1'] = bool(int(parts[1]))
            sensor1_data['sensor2'] = bool(int(parts[2]))
            sensor1_data['sensor3'] = bool(int(parts[3]))
            sensor1_data['accelX'] = float(parts[4])
            sensor1_data['accelY'] = float(parts[5])
            sensor1_data['accelZ'] = float(parts[6])
            sensor1_data['gyroX'] = float(parts[7])
            sensor1_data['gyroY'] = float(parts[8])
            sensor1_data['gyroZ'] = float(parts[9])
            
            packet_count['sensor1'] += 1
            return ('sensor1', sensor1_data.copy())
        
        # SENDER 2: Distance sensors ONLY (4 fields)
        elif sensor_type == "SENSOR2" and len(parts) >= 4:
            sensor2_data['sensor4'] = bool(int(parts[1]))
            sensor2_data['sensor5'] = bool(int(parts[2]))
            sensor2_data['sensor6'] = bool(int(parts[3]))
            
            packet_count['sensor2'] += 1
            return ('sensor2', sensor2_data.copy())
            
    except (ValueError, IndexError) as e:
        # Silently ignore parse errors
        return None
    
    return None

def display_sensor_data():
    """Display sensor data in terminal"""
    os.system('cls' if os.name == 'nt' else 'clear')
    
    elapsed = time.time() - start_time
    
    print("=" * 60)
    print("           SENSOR DATA MONITOR")
    print("=" * 60)
    print(f"Runtime: {elapsed:.1f}s | Packets: S1={packet_count['sensor1']}, S2={packet_count['sensor2']}")
    print()
    
    # Sender 1 (with IMU)
    print("┌─── SENDER 1 (Camera 1) ──────────────────────────────┐")
    print(f"│ Distance Sensors:                                    │")
    print(f"│   IR1: {'🔴 DETECT' if sensor1_data['sensor1'] else '⚪ clear'}  ", end="")
    print(f"IR2: {'🔴 DETECT' if sensor1_data['sensor2'] else '⚪ clear'}  ", end="")
    print(f"IR3: {'🔴 DETECT' if sensor1_data['sensor3'] else '⚪ clear'}   │")
    print(f"│                                                      │")
    print(f"│ IMU (MPU6050):                                       │")
    print(f"│   Accel: X={sensor1_data['accelX']:6.2f} Y={sensor1_data['accelY']:6.2f} Z={sensor1_data['accelZ']:6.2f} m/s²│")
    print(f"│   Gyro:  X={sensor1_data['gyroX']:6.2f} Y={sensor1_data['gyroY']:6.2f} Z={sensor1_data['gyroZ']:6.2f} rad/s│")
    print("└──────────────────────────────────────────────────────┘")
    print()
    
    # Sender 2 (distance sensors only)
    print("┌─── SENDER 2 (Camera 2) ──────────────────────────────┐")
    print(f"│ Distance Sensors:                                    │")
    print(f"│   IR4: {'🔴 DETECT' if sensor2_data['sensor4'] else '⚪ clear'}  ", end="")
    print(f"IR5: {'🔴 DETECT' if sensor2_data['sensor5'] else '⚪ clear'}  ", end="")
    print(f"IR6: {'🔴 DETECT' if sensor2_data['sensor6'] else '⚪ clear'}   │")
    print(f"│                                                      │")
    print(f"│ (No IMU on this sender)                              │")
    print("└──────────────────────────────────────────────────────┘")
    print()
    print("Press Ctrl+C to exit")

def main():
    print(f"Connecting to {SERIAL_PORT} at {BAUD_RATE} baud...")
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)  # Wait for connection
        print("✅ Connected!")
        print("Waiting for sensor data...\n")
        
        # CSV logging
        log_file = os.path.join(LOG_DIR, f"sensors_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
        csv_file = open(log_file, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        
        # CSV header
        csv_writer.writerow(['time', 'sensor_id', 'IR1', 'IR2', 'IR3', 
                            'accelX', 'accelY', 'accelZ', 
                            'gyroX', 'gyroY', 'gyroZ'])
        
        last_display = time.time()
        
        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore')
                    
                    result = parse_sensor_packet(line)
                    
                    if result:
                        sensor_type, data = result
                        
                        # Log to CSV
                        current_time = time.time() - start_time
                        
                        if sensor_type == 'sensor1':
                            csv_writer.writerow([
                                f"{current_time:.3f}",
                                'sender1',
                                int(data['sensor1']), 
                                int(data['sensor2']), 
                                int(data['sensor3']),
                                f"{data['accelX']:.3f}", 
                                f"{data['accelY']:.3f}", 
                                f"{data['accelZ']:.3f}",
                                f"{data['gyroX']:.3f}", 
                                f"{data['gyroY']:.3f}", 
                                f"{data['gyroZ']:.3f}"
                            ])
                        elif sensor_type == 'sensor2':
                            csv_writer.writerow([
                                f"{current_time:.3f}",
                                'sender2',
                                int(data['sensor4']), 
                                int(data['sensor5']), 
                                int(data['sensor6']),
                                '', '', '',  # No IMU data
                                '', '', ''
                            ])
                        
                        csv_file.flush()  # Write immediately
                        
                        # Update display every 100ms
                        if time.time() - last_display > 0.1:
                            display_sensor_data()
                            last_display = time.time()
                            
                except UnicodeDecodeError:
                    pass
                    
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
        print("\nTroubleshooting:")
        print("1. Check COM port (Device Manager on Windows)")
        print("2. Make sure ESP32 receiver is connected")
        print("3. Close Arduino Serial Monitor if open")
        print("4. Try unplugging and replugging USB")
        
    except KeyboardInterrupt:
        print("\n\n👋 Shutting down...")
        print(f"Total packets received:")
        print(f"  Sender 1 (with IMU): {packet_count['sensor1']}")
        print(f"  Sender 2 (no IMU):   {packet_count['sensor2']}")
        
    finally:
        if 'ser' in locals():
            ser.close()
        if 'csv_file' in locals():
            csv_file.close()
            print(f"📊 Data logged to: {log_file}")
        print("✅ Closed")

if __name__ == "__main__":
    main()
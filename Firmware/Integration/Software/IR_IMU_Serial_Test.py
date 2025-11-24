"""
Jetson Sensor Receiver - Receives IR and IMU sensor data from ESP32 receiver
Format: SS:MMM:UUU,IR1,IR2,IR3,IR4,IR5,IR6,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,END
"""

import serial
import time
import csv
import os
from datetime import datetime

# Configuration
SERIAL_PORT = "COM3"  # Change for your system
BAUD_RATE = 115200

# CSV logging
LOG_DIR = "sensor_logs"
os.makedirs(LOG_DIR, exist_ok=True)

# Statistics
packet_count = 0
start_time = time.time()

def parse_timestamp(timestamp_str):
    """
    Parse timestamp in format SS:MMM:UUU
    Returns total microseconds
    """
    try:
        parts = timestamp_str.split(':')
        if len(parts) != 3:
            return None
        
        seconds = int(parts[0])
        milliseconds = int(parts[1])
        microseconds = int(parts[2])
        
        total_us = (seconds * 1000000) + (milliseconds * 1000) + microseconds
        return total_us
        
    except (ValueError, IndexError):
        return None

def parse_sensor_line(line):
    """
    Parse sensor data line
    Format: SS:MMM:UUU,IR1,IR2,IR3,IR4,IR5,IR6,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,END
    
    Returns:
        dict with all sensor data or None if parse fails
    """
    try:
        parts = line.strip().split(',')
        
        # Check for END marker (should be 14 parts total)
        if len(parts) != 14 or parts[13] != "END":
            return None
        
        # Parse timestamp
        timestamp_us = parse_timestamp(parts[0])
        if timestamp_us is None:
            return None
        
        sensor_data = {
            'timestamp_us': timestamp_us,
            'timestamp_str': parts[0],
            'ir1': bool(int(parts[1])),
            'ir2': bool(int(parts[2])),
            'ir3': bool(int(parts[3])),
            'ir4': bool(int(parts[4])),
            'ir5': bool(int(parts[5])),
            'ir6': bool(int(parts[6])),
            'accel_x': float(parts[7]),
            'accel_y': float(parts[8]),
            'accel_z': float(parts[9]),
            'gyro_x': float(parts[10]),
            'gyro_y': float(parts[11]),
            'gyro_z': float(parts[12])
        }
        
        return sensor_data
        
    except (ValueError, IndexError) as e:
        print(f"Parse error: {e}")
        return None

def format_timestamp(micros):
    """Convert microseconds to readable format"""
    seconds = micros / 1000000.0
    minutes = int(seconds // 60)
    secs = seconds % 60
    return f"{minutes}m {secs:.6f}s"

def display_sensor_data(sensor_data):
    """Display sensor data in terminal"""
    print("\n" + "="*70)
    print(f"SENSOR DATA - Timestamp: {sensor_data['timestamp_str']}")
    print("="*70)
    
    print(f"🕐 ESP32 Runtime: {format_timestamp(sensor_data['timestamp_us'])}")
    
    print("\n🔴 Distance Sensors:")
    print(f"  IR1: {'🔴 DETECT' if sensor_data['ir1'] else '⚪ clear'}  ", end="")
    print(f"IR2: {'🔴 DETECT' if sensor_data['ir2'] else '⚪ clear'}  ", end="")
    print(f"IR3: {'🔴 DETECT' if sensor_data['ir3'] else '⚪ clear'}")
    print(f"  IR4: {'🔴 DETECT' if sensor_data['ir4'] else '⚪ clear'}  ", end="")
    print(f"IR5: {'🔴 DETECT' if sensor_data['ir5'] else '⚪ clear'}  ", end="")
    print(f"IR6: {'🔴 DETECT' if sensor_data['ir6'] else '⚪ clear'}")
    
    print("\n📊 IMU Data:")
    print(f"  Accel: X={sensor_data['accel_x']:7.3f}  Y={sensor_data['accel_y']:7.3f}  Z={sensor_data['accel_z']:7.3f} m/s²")
    print(f"  Gyro:  X={sensor_data['gyro_x']:7.3f}  Y={sensor_data['gyro_y']:7.3f}  Z={sensor_data['gyro_z']:7.3f} rad/s")

def main():
    print(f"Connecting to {SERIAL_PORT}...")
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        print("✅ Connected to ESP32 Receiver")
        print("Waiting for sensor data...\n")
        
        # CSV logging
        log_file = os.path.join(LOG_DIR, f"sensors_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
        csv_file = open(log_file, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        
        # CSV header
        csv_writer.writerow(['runtime_s', 'timestamp_us', 'timestamp_str',
                            'ir1', 'ir2', 'ir3', 'ir4', 'ir5', 'ir6',
                            'accel_x', 'accel_y', 'accel_z',
                            'gyro_x', 'gyro_y', 'gyro_z'])
        
        last_timestamp_us = None
        packet_count = 0
        
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore')
                
                sensor_data = parse_sensor_line(line)
                
                if sensor_data:
                    packet_count += 1
                    timestamp_us = sensor_data['timestamp_us']
                    
                    # Calculate time between messages
                    if last_timestamp_us is not None:
                        delta_us = timestamp_us - last_timestamp_us
                        delta_ms = delta_us / 1000.0
                        rate_hz = 1000000.0 / delta_us if delta_us > 0 else 0
                        print(f"⏱️  Delta: {delta_ms:.3f}ms ({rate_hz:.1f} Hz)")
                    
                    last_timestamp_us = timestamp_us
                    
                    # Display data
                    display_sensor_data(sensor_data)
                    
                    # Log to CSV
                    runtime_s = time.time() - start_time
                    csv_writer.writerow([
                        f"{runtime_s:.6f}",
                        sensor_data['timestamp_us'],
                        sensor_data['timestamp_str'],
                        int(sensor_data['ir1']),
                        int(sensor_data['ir2']),
                        int(sensor_data['ir3']),
                        int(sensor_data['ir4']),
                        int(sensor_data['ir5']),
                        int(sensor_data['ir6']),
                        f"{sensor_data['accel_x']:.3f}",
                        f"{sensor_data['accel_y']:.3f}",
                        f"{sensor_data['accel_z']:.3f}",
                        f"{sensor_data['gyro_x']:.3f}",
                        f"{sensor_data['gyro_y']:.3f}",
                        f"{sensor_data['gyro_z']:.3f}"
                    ])
                    csv_file.flush()
                    
                    print(f"\n📦 Total packets: {packet_count}")
                    
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
        print("\nTroubleshooting:")
        print("1. Check serial port (Linux: ls /dev/ttyUSB*, Mac: ls /dev/cu.*)")
        print("2. Make sure ESP32 receiver is connected")
        print("3. Close Arduino Serial Monitor if open")
        
    except KeyboardInterrupt:
        print("\n\n👋 Shutting down...")
        print(f"\n📊 Session Summary:")
        print(f"   Total packets received: {packet_count}")
        print(f"   Runtime: {time.time() - start_time:.1f}s")
        
    finally:
        if 'ser' in locals():
            ser.close()
        if 'csv_file' in locals():
            csv_file.close()
            print(f"📊 Data logged to: {log_file}")
        print("✅ Closed")

if __name__ == "__main__":
    main()
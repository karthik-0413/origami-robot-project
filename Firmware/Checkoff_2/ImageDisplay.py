import serial
import matplotlib.pyplot as plt
from io import BytesIO
from PIL import Image

# Configuration
SERIAL_PORT = 'COM3'  # Change to your port (COM3, COM4 on Windows or /dev/ttyUSB0 on Linux)
BAUD_RATE = 115200

# Protocol markers
START_MARKER = bytes([0xFF, 0xD8, 0xFF, 0xAA])
END_MARKER = bytes([0xFF, 0xD9, 0xFF, 0xBB])

# Initialize serial connection
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

# Create matplotlib figure for live display
plt.ion()  # Interactive mode
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
fig.suptitle('Live Camera Feed')

ax1.set_title('Camera 1')
ax1.axis('off')
ax2.set_title('Camera 2')
ax2.axis('off')

print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud")
print("Waiting for images...")

def find_marker(buffer, marker):
    """Find marker in buffer and return position"""
    try:
        return buffer.index(marker)
    except ValueError:
        return -1

def receive_and_display_image():
    """Receive binary image data and display it"""
    buffer = b''
    
    try:
        while True:
            # Read available bytes
            if ser.in_waiting > 0:
                buffer += ser.read(ser.in_waiting)
            
            # Look for start marker
            start_pos = find_marker(buffer, START_MARKER)
            
            if start_pos >= 0:
                # Remove everything before start marker
                buffer = buffer[start_pos:]
                
                # Check if we have enough bytes for header (4 marker + 1 camera_id + 4 size)
                if len(buffer) >= 9:
                    # Parse header
                    camera_id = buffer[4]
                    image_size = (buffer[5] << 24) | (buffer[6] << 16) | (buffer[7] << 8) | buffer[8]
                    
                    # Total packet size: header + image + end marker
                    total_size = 9 + image_size + 4
                    
                    # Wait until we have the complete packet
                    if len(buffer) >= total_size:
                        # Extract image data
                        image_data = buffer[9:9+image_size]
                        
                        # Verify end marker
                        end_marker_pos = 9 + image_size
                        if buffer[end_marker_pos:end_marker_pos+4] == END_MARKER:
                            # Valid image received!
                            try:
                                img = Image.open(BytesIO(image_data))
                                
                                # Display in appropriate subplot
                                if camera_id == 1:
                                    ax1.clear()
                                    ax1.imshow(img)
                                    ax1.set_title(f'Camera 1 ({image_size} bytes)')
                                    ax1.axis('off')
                                elif camera_id == 2:
                                    ax2.clear()
                                    ax2.imshow(img)
                                    ax2.set_title(f'Camera 2 ({image_size} bytes)')
                                    ax2.axis('off')
                                
                                plt.pause(0.001)
                                print(f"✓ Displayed Camera {camera_id} image ({image_size} bytes)")
                                
                            except Exception as e:
                                print(f"✗ Error decoding image from Camera {camera_id}: {e}")
                            
                            # Remove processed packet from buffer
                            buffer = buffer[total_size:]
                        else:
                            print(f"✗ Invalid end marker for Camera {camera_id}")
                            buffer = buffer[1:]  # Skip one byte and try again
                
                # Prevent buffer from growing too large
                if len(buffer) > 100000:
                    print("Buffer overflow, resetting...")
                    buffer = b''
    
    except KeyboardInterrupt:
        print("\nStopping...")
        ser.close()
        plt.close()
        exit()
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

# Main loop
try:
    while True:
        receive_and_display_image()
except KeyboardInterrupt:
    print("\nClosing connection...")
    ser.close()
    plt.close()
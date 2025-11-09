import socket
import matplotlib.pyplot as plt
from io import BytesIO
from PIL import Image
import time

# Configuration
RECEIVER_PORT = 9999
LAPTOP_IP = '0.0.0.0'

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LAPTOP_IP, RECEIVER_PORT))
sock.settimeout(0.1)

# Create matplotlib figure
plt.ion()
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
fig.suptitle('Live Camera Feed via WiFi')

ax1.set_title('Camera 1')
ax1.axis('off')
ax2.set_title('Camera 2')
ax2.axis('off')

print(f"Listening for images on port {RECEIVER_PORT}")
print("Waiting for images...")

# Track FPS
last_time_cam1 = 0
last_time_cam2 = 0
fps_cam1 = 0
fps_cam2 = 0

# Image buffers for reassembly
cam1_chunks = {}
cam2_chunks = {}

def reassemble_image(chunks, total_chunks):
    """Reassemble image from chunks"""
    if len(chunks) != total_chunks:
        return None
    
    # Concatenate all chunks in order
    image_data = b''
    for seq in range(total_chunks):
        if seq not in chunks:
            return None
        image_data += chunks[seq]
    
    return image_data

def receive_and_display():
    """Receive UDP packets and display images"""
    global last_time_cam1, last_time_cam2, fps_cam1, fps_cam2
    global cam1_chunks, cam2_chunks
    
    try:
        data, addr = sock.recvfrom(65536)
        
        if len(data) < 9:
            return
        
        # Check for done marker
        if data[0:4] == bytes([0xFF, 0xD9, 0xFF, 0xBB]):
            # Done packet: [MARKER(4)][CAMERA_ID(1)][TOTAL_CHUNKS(2)][TOTAL_SIZE(2)]
            camera_id = data[4]
            total_chunks = (data[5] << 8) | data[6]
            total_size = (data[7] << 8) | data[8]
            
            # Reassemble image
            chunks = cam1_chunks if camera_id == 1 else cam2_chunks
            image_data = reassemble_image(chunks, total_chunks)
            
            if image_data:
                # Calculate FPS
                now = time.time()
                if camera_id == 1:
                    if last_time_cam1 > 0:
                        fps_cam1 = 1.0 / (now - last_time_cam1)
                    last_time_cam1 = now
                elif camera_id == 2:
                    if last_time_cam2 > 0:
                        fps_cam2 = 1.0 / (now - last_time_cam2)
                    last_time_cam2 = now
                
                # Display image
                try:
                    img = Image.open(BytesIO(image_data))
                    
                    if camera_id == 1:
                        ax1.clear()
                        ax1.imshow(img)
                        ax1.set_title(f'Camera 1 - {fps_cam1:.1f} FPS')
                        ax1.axis('off')
                    elif camera_id == 2:
                        ax2.clear()
                        ax2.imshow(img)
                        ax2.set_title(f'Camera 2 - {fps_cam2:.1f} FPS')
                        ax2.axis('off')
                    
                    plt.pause(0.001)
                    print(f"✓ Camera {camera_id} - {len(image_data)} bytes - {fps_cam1 if camera_id == 1 else fps_cam2:.1f} FPS")
                    
                except Exception as e:
                    print(f"✗ Error decoding Camera {camera_id}: {e}")
            
            # Clear chunks for next image
            if camera_id == 1:
                cam1_chunks = {}
            else:
                cam2_chunks = {}
            
            return
        
        # Regular chunk packet
        if data[0:4] == bytes([0xFF, 0xD8, 0xFF, 0xAA]):
            # Parse: [MARKER(4)][CAMERA_ID(1)][SEQ(2)][TOTAL(2)][DATA]
            camera_id = data[4]
            seq = (data[5] << 8) | data[6]
            total = (data[7] << 8) | data[8]
            chunk_data = data[9:]
            
            # Store chunk
            if camera_id == 1:
                cam1_chunks[seq] = chunk_data
            elif camera_id == 2:
                cam2_chunks[seq] = chunk_data
    
    except socket.timeout:
        pass
    except KeyboardInterrupt:
        print("\nStopping...")
        sock.close()
        plt.close()
        exit()
    except Exception as e:
        print(f"Error: {e}")

# Main loop
try:
    while True:
        receive_and_display()
except KeyboardInterrupt:
    print("\nClosing connection...")
    sock.close()
    plt.close()
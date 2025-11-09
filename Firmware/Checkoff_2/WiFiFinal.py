
"""
Stereo Camera Display - Receives images from ESP32 receiver via UDP
Displays Camera 1 and Camera 2 side-by-side in real-time
"""

import socket
import numpy as np
import cv2
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time
from collections import defaultdict

# Configuration
UDP_IP = "0.0.0.0"  # Listen on all interfaces
UDP_PORT = 9999
BUFFER_SIZE = 2048

# Image buffers for reconstruction
image_buffers = {1: {}, 2: {}}  # camera_id -> {packet_num -> data}
image_sizes = {1: 0, 2: 0}      # camera_id -> total_size
last_display_time = {1: 0, 2: 0}
fps_counters = {1: [], 2: []}

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.01)  # 10ms timeout for non-blocking

print(f"📡 Listening for stereo camera images on {UDP_IP}:{UDP_PORT}")
print("Waiting for images from ESP32 receiver...")

# Setup matplotlib figure
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
fig.suptitle('Stereo Camera Feed - ESP32 via WiFi', fontsize=14, fontweight='bold')

# Initialize with black images
img1 = np.zeros((240, 320, 3), dtype=np.uint8)
img2 = np.zeros((240, 320, 3), dtype=np.uint8)

im1 = ax1.imshow(img1)
ax1.set_title('Camera 1\nFPS: --')
ax1.axis('off')

im2 = ax2.imshow(img2)
ax2.set_title('Camera 2\nFPS: --')
ax2.axis('off')

plt.tight_layout()

def calculate_fps(camera_id):
    """Calculate FPS based on recent frame times"""
    current_time = time.time()
    fps_counters[camera_id].append(current_time)
    
    # Keep only last 10 frames
    fps_counters[camera_id] = [t for t in fps_counters[camera_id] if current_time - t < 2.0]
    
    if len(fps_counters[camera_id]) > 1:
        time_diff = fps_counters[camera_id][-1] - fps_counters[camera_id][0]
        if time_diff > 0:
            return (len(fps_counters[camera_id]) - 1) / time_diff
    return 0.0

def process_packet(data):
    """Process incoming UDP packet and reconstruct image"""
    if len(data) < 3:
        return None
    
    # Parse header: [camera_id][img_size_high][img_size_low][chunk_data...]
    camera_id = data[0]
    img_size = (data[1] << 8) | data[2]
    chunk_data = data[3:]
    
    if camera_id not in [1, 2]:
        return None
    
    # Initialize new image buffer if size changed
    if image_sizes[camera_id] != img_size:
        image_buffers[camera_id] = {}
        image_sizes[camera_id] = img_size
    
    # Calculate packet number based on received data
    MAX_PAYLOAD = 1397  # 1400 - 3 byte header
    packet_num = len(image_buffers[camera_id])
    
    # Store chunk
    image_buffers[camera_id][packet_num] = chunk_data
    
    # Check if we have all packets
    expected_packets = (img_size + MAX_PAYLOAD - 1) // MAX_PAYLOAD
    if len(image_buffers[camera_id]) >= expected_packets:
        # Reconstruct complete image
        img_data = bytearray()
        for i in range(expected_packets):
            if i in image_buffers[camera_id]:
                img_data.extend(image_buffers[camera_id][i])
        
        # Clear buffer for next image
        image_buffers[camera_id] = {}
        
        # Decode JPEG
        if len(img_data) > 0:
            try:
                img_array = np.frombuffer(img_data, dtype=np.uint8)
                img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                if img is not None:
                    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
                    return camera_id, img_rgb
            except Exception as e:
                print(f"❌ Error decoding Camera {camera_id}: {e}")
    
    return None

def update_plot(frame):
    """Update matplotlib plot with new images"""
    # Process incoming packets (non-blocking)
    packets_processed = 0
    max_packets_per_frame = 50  # Process multiple packets per animation frame
    
    while packets_processed < max_packets_per_frame:
        try:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            result = process_packet(data)
            
            if result:
                camera_id, img = result
                fps = calculate_fps(camera_id)
                
                if camera_id == 1:
                    im1.set_array(img)
                    ax1.set_title(f'Camera 1\nFPS: {fps:.1f}')
                    print(f"✓ Camera 1 - {len(img)} pixels - {fps:.1f} FPS")
                elif camera_id == 2:
                    im2.set_array(img)
                    ax2.set_title(f'Camera 2\nFPS: {fps:.1f}')
                    print(f"✓ Camera 2 - {len(img)} pixels - {fps:.1f} FPS")
            
            packets_processed += 1
            
        except socket.timeout:
            break
        except Exception as e:
            print(f"❌ Error: {e}")
            break
    
    return im1, im2

# Start animation (50ms interval = 20 FPS display refresh)
ani = FuncAnimation(fig, update_plot, interval=50, blit=True, cache_frame_data=False)

print("\n🎥 Display window opened!")
print("Images will appear as they are received from the ESP32...")
print("Press Ctrl+C in terminal to exit.\n")

try:
    plt.show()
except KeyboardInterrupt:
    print("\n\n👋 Shutting down...")
finally:
    sock.close()
    print("✅ Socket closed. Goodbye!")
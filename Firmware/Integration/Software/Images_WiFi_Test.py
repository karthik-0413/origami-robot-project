"""
Stereo Camera Display - Receives images from ESP32 receiver via UDP
Displays Camera 1 and Camera 2 side-by-side in real-time
Saves images to disk
"""

import socket
import numpy as np
import cv2
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time
from collections import defaultdict
import os
from datetime import datetime

# Configuration
UDP_IP = "0.0.0.0"
UDP_PORT = 9999  # ⭐ Must match LAPTOP_PORT in Comms.h
BUFFER_SIZE = 2048

# Image save directory
SAVE_DIR = "captured_images"
os.makedirs(SAVE_DIR, exist_ok=True)
os.makedirs(os.path.join(SAVE_DIR, "camera1"), exist_ok=True)
os.makedirs(os.path.join(SAVE_DIR, "camera2"), exist_ok=True)

print(f"📁 Saving images to: {os.path.abspath(SAVE_DIR)}")

# Image buffers for reconstruction
image_buffers = {1: {}, 2: {}}
image_sizes = {1: 0, 2: 0}
last_display_time = {1: 0, 2: 0}
fps_counters = {1: [], 2: []}
image_counters = {1: 0, 2: 0}

decode_times = {1: [], 2: []}

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.01)

print(f"📡 Listening for stereo camera images on {UDP_IP}:{UDP_PORT}")
print("Waiting for images from ESP32 receiver...")

# Setup matplotlib figure
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
fig.suptitle('Stereo Camera Feed - ESP32 via WiFi', fontsize=14, fontweight='bold')

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
    
    fps_counters[camera_id] = [t for t in fps_counters[camera_id] if current_time - t < 2.0]
    
    if len(fps_counters[camera_id]) > 1:
        time_diff = fps_counters[camera_id][-1] - fps_counters[camera_id][0]
        if time_diff > 0:
            return (len(fps_counters[camera_id]) - 1) / time_diff
    return 0.0

def save_image(camera_id, img_bgr, img_rgb):
    """Save image to disk in both JPEG and RGB formats"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
    image_counters[camera_id] += 1
    
    jpg_filename = os.path.join(SAVE_DIR, f"camera{camera_id}", 
                                f"cam{camera_id}_{timestamp}_{image_counters[camera_id]:04d}.jpg")
    cv2.imwrite(jpg_filename, img_bgr)
    
    print(f"💾 Saved: {jpg_filename}")

def process_packet(data):
    """Process incoming UDP packet and reconstruct image"""
    if len(data) < 3:
        return None
    
    camera_id = data[0]
    img_size = (data[1] << 8) | data[2]
    chunk_data = data[3:]
    
    if camera_id not in [1, 2]:
        return None
    
    if image_sizes[camera_id] != img_size:
        image_buffers[camera_id] = {}
        image_sizes[camera_id] = img_size
    
    MAX_PAYLOAD = 1397
    packet_num = len(image_buffers[camera_id])
    
    image_buffers[camera_id][packet_num] = chunk_data
    
    expected_packets = (img_size + MAX_PAYLOAD - 1) // MAX_PAYLOAD
    if len(image_buffers[camera_id]) >= expected_packets:
        img_data = bytearray()
        for i in range(expected_packets):
            if i in image_buffers[camera_id]:
                img_data.extend(image_buffers[camera_id][i])
        
        image_buffers[camera_id] = {}
        
        if len(img_data) > 0:
            try:
                img_array = np.frombuffer(img_data, dtype=np.uint8)
                
                decode_start = time.time()
                img_bgr = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                decode_end = time.time()
                
                decode_time_ms = (decode_end - decode_start) * 1000
                
                if img_bgr is not None:
                    decode_times[camera_id].append(decode_time_ms)
                    
                    print(f"[Camera {camera_id}] Decode: {decode_time_ms:.1f} ms", end="")
                    
                    if len(decode_times[camera_id]) >= 10:
                        avg_decode = np.mean(decode_times[camera_id][-10:])
                        print(f" | Avg (last 10): {avg_decode:.1f} ms", end="")
                    
                    print()
                    
                    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
                    
                    save_image(camera_id, img_bgr, img_rgb)
                    
                    return camera_id, img_rgb
                    
            except Exception as e:
                print(f"❌ Error decoding Camera {camera_id}: {e}")
    
    return None

def update_plot(frame):
    """Update matplotlib plot with new images"""
    packets_processed = 0
    max_packets_per_frame = 50
    
    while packets_processed < max_packets_per_frame:
        try:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            result = process_packet(data)
            
            if result:
                camera_id, img = result
                fps = calculate_fps(camera_id)
                
                if camera_id == 1:
                    im1.set_array(img)
                    ax1.set_title(f'Camera 1\nFPS: {fps:.1f} | Saved: {image_counters[1]}')
                    print(f"✓ Camera 1 - {img.shape} - {fps:.1f} FPS")
                elif camera_id == 2:
                    im2.set_array(img)
                    ax2.set_title(f'Camera 2\nFPS: {fps:.1f} | Saved: {image_counters[2]}')
                    print(f"✓ Camera 2 - {img.shape} - {fps:.1f} FPS")
            
            packets_processed += 1
            
        except socket.timeout:
            break
        except Exception as e:
            print(f"❌ Error: {e}")
            break
    
    return im1, im2

ani = FuncAnimation(fig, update_plot, interval=50, blit=True, cache_frame_data=False)

print("\n🎥 Display window opened!")
print(f"📁 Images will be saved to: {os.path.abspath(SAVE_DIR)}")
print("Press Ctrl+C in terminal to exit.\n")

try:
    plt.show()
except KeyboardInterrupt:
    print("\n\n👋 Shutting down...")
    print(f"📊 Total images saved:")
    print(f"   Camera 1: {image_counters[1]} images")
    print(f"   Camera 2: {image_counters[2]} images")
    
    print(f"\n⏱️  Decode Time Statistics:")
    if len(decode_times[1]) > 0:
        avg1 = np.mean(decode_times[1])
        min1 = np.min(decode_times[1])
        max1 = np.max(decode_times[1])
        print(f"   Camera 1: Avg={avg1:.1f}ms, Min={min1:.1f}ms, Max={max1:.1f}ms")
    if len(decode_times[2]) > 0:
        avg2 = np.mean(decode_times[2])
        min2 = np.min(decode_times[2])
        max2 = np.max(decode_times[2])
        print(f"   Camera 2: Avg={avg2:.1f}ms, Min={min2:.1f}ms, Max={max2:.1f}ms")
    
    all_times = decode_times[1] + decode_times[2]
    if len(all_times) > 0:
        overall_avg = np.mean(all_times)
        print(f"   Overall Average: {overall_avg:.1f}ms")
        
finally:
    sock.close()
    print("✅ Socket closed. Goodbye!")
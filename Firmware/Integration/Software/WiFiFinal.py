"""
Jetson Image Receiver - Minimal version
Receives stereo camera images from ESP32 via UDP and saves as JPEG
No FPS, no decode timing, no display - just receive and save
"""

import socket
import numpy as np
import cv2
import os
from datetime import datetime
from threading import Thread, Lock

# Configuration
UDP_IP = "0.0.0.0"
UDP_PORT = 9999
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
image_counters = {1: 0, 2: 0}

# Latest images available for processing
latest_images = {1: None, 2: None}
latest_images_lock = Lock()

def save_image(camera_id, img_bgr):
    """Save image to disk as JPEG"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
    image_counters[camera_id] += 1
    
    jpg_filename = os.path.join(SAVE_DIR, f"camera{camera_id}", 
                                f"cam{camera_id}_{timestamp}_{image_counters[camera_id]:04d}.jpg")
    
    cv2.imwrite(jpg_filename, img_bgr, [cv2.IMWRITE_JPEG_QUALITY, 95])
    
    return jpg_filename

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
                img_bgr = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                
                if img_bgr is not None:
                    # Save to disk
                    filename = save_image(camera_id, img_bgr)
                    
                    # Update latest image (thread-safe)
                    with latest_images_lock:
                        latest_images[camera_id] = img_bgr.copy()
                    
                    print(f"[Camera {camera_id}] Saved: {filename}")
                    
                    return camera_id, img_bgr
                    
            except Exception as e:
                print(f"❌ Error decoding Camera {camera_id}: {e}")
    
    return None

def get_latest_image(camera_id):
    """
    Get the most recent image from a specific camera
    
    Args:
        camera_id: 1 or 2
    
    Returns:
        numpy array (BGR) or None if no image available
    """
    with latest_images_lock:
        if latest_images[camera_id] is not None:
            return latest_images[camera_id].copy()
    return None

def get_latest_stereo_pair():
    """
    Get the most recent stereo pair (both cameras)
    
    Returns:
        tuple: (img1, img2) or (None, None) if not available
    """
    with latest_images_lock:
        img1 = latest_images[1].copy() if latest_images[1] is not None else None
        img2 = latest_images[2].copy() if latest_images[2] is not None else None
    return img1, img2

def main():
    print("Starting Jetson Image Receiver...")
    print(f"📡 Listening on {UDP_IP}:{UDP_PORT}")
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    
    print("\n✅ Receiver started")
    print("📁 Images saving to:", os.path.abspath(SAVE_DIR))
    print("\nPress Ctrl+C to exit\n")
    
    try:
        while True:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            process_packet(data)
            
    except KeyboardInterrupt:
        print("\n\n👋 Shutting down...")
        print(f"\n📊 Total images saved:")
        print(f"   Camera 1: {image_counters[1]} images")
        print(f"   Camera 2: {image_counters[2]} images")
        
    finally:
        sock.close()
        print("✅ Closed")

if __name__ == "__main__":
    main()
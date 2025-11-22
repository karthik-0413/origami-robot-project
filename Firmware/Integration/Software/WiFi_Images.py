"""
Stereo Camera Receiver - Single File Solution
Receives images from ESP32 via UDP and provides them to callback functions
No GUI - headless operation for Jetson or any Linux system
"""

import socket
import numpy as np
import cv2
import time
import threading
from typing import Callable, Optional, Tuple
import signal
import sys


class StereoImageReceiver:
    """
    Handles UDP reception and image reconstruction for stereo camera system.
    Provides decoded images to registered callback functions.
    """
    
    def __init__(self, udp_ip: str = "0.0.0.0", udp_port: int = 9999, buffer_size: int = 2048):
        """
        Initialize the stereo image receiver.
        
        Args:
            udp_ip: IP address to bind to (default: "0.0.0.0" for all interfaces)
            udp_port: UDP port to listen on (default: 9999)
            buffer_size: UDP receive buffer size (default: 2048)
        """
        self.UDP_IP = udp_ip
        self.UDP_PORT = udp_port
        self.BUFFER_SIZE = buffer_size
        
        # Image reconstruction buffers
        self.image_buffers = {1: {}, 2: {}}
        self.image_sizes = {1: 0, 2: 0}
        
        # FPS tracking
        self.fps_counters = {1: [], 2: []}
        
        # Statistics
        self.stats = {
            1: {'received': 0, 'errors': 0, 'fps': 0.0},
            2: {'received': 0, 'errors': 0, 'fps': 0.0}
        }
        
        # Callbacks list
        self.callbacks = []
        
        # Threading
        self.sock = None
        self.running = False
        self.receive_thread = None
        
        # Max payload size from ESP32
        self.MAX_PAYLOAD = 1397
        
    def register_callback(self, callback: Callable[[int, np.ndarray, np.ndarray, float, float], None]):
        """
        Register a callback function to receive images.
        
        The callback will be called with:
            camera_id (int): 1 or 2
            img_rgb (np.ndarray): Image in RGB format (H, W, 3)
            img_bgr (np.ndarray): Image in BGR format (H, W, 3) - ready for cv2.imwrite()
            timestamp (float): Unix timestamp when image was received
            fps (float): Current FPS for this camera
        
        Example:
            def my_callback(camera_id, img_rgb, img_bgr, timestamp, fps):
                print(f"Camera {camera_id}: {img_rgb.shape}, {fps:.1f} FPS")
                cv2.imwrite(f"cam{camera_id}.jpg", img_bgr)
            
            receiver.register_callback(my_callback)
        """
        self.callbacks.append(callback)
        print(f"✓ Registered callback: {callback.__name__}")
    
    def calculate_fps(self, camera_id: int) -> float:
        """Calculate FPS based on recent frame times."""
        current_time = time.time()
        self.fps_counters[camera_id].append(current_time)
        
        # Keep only last 2 seconds of timestamps
        self.fps_counters[camera_id] = [
            t for t in self.fps_counters[camera_id] 
            if current_time - t < 2.0
        ]
        
        if len(self.fps_counters[camera_id]) > 1:
            time_diff = self.fps_counters[camera_id][-1] - self.fps_counters[camera_id][0]
            if time_diff > 0:
                fps = (len(self.fps_counters[camera_id]) - 1) / time_diff
                self.stats[camera_id]['fps'] = fps
                return fps
        
        return 0.0
    
    def process_packet(self, data: bytes) -> Optional[Tuple[int, np.ndarray, np.ndarray]]:
        """
        Process incoming UDP packet and reconstruct image.
        
        Returns:
            Tuple of (camera_id, img_rgb, img_bgr) if image complete, None otherwise
        """
        if len(data) < 3:
            return None
        
        # Parse packet header
        camera_id = data[0]
        img_size = (data[1] << 8) | data[2]
        chunk_data = data[3:]
        
        if camera_id not in [1, 2]:
            return None
        
        # Reset buffer if image size changed
        if self.image_sizes[camera_id] != img_size:
            self.image_buffers[camera_id] = {}
            self.image_sizes[camera_id] = img_size
        
        # Store chunk
        packet_num = len(self.image_buffers[camera_id])
        self.image_buffers[camera_id][packet_num] = chunk_data
        
        # Check if we have all packets
        expected_packets = (img_size + self.MAX_PAYLOAD - 1) // self.MAX_PAYLOAD
        
        if len(self.image_buffers[camera_id]) >= expected_packets:
            # Reconstruct complete image
            img_data = bytearray()
            for i in range(expected_packets):
                if i in self.image_buffers[camera_id]:
                    img_data.extend(self.image_buffers[camera_id][i])
            
            # Clear buffer for next image
            self.image_buffers[camera_id] = {}
            
            # Decode JPEG
            if len(img_data) > 0:
                try:
                    img_array = np.frombuffer(img_data, dtype=np.uint8)
                    img_bgr = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                    
                    if img_bgr is not None:
                        img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
                        self.stats[camera_id]['received'] += 1
                        return camera_id, img_rgb, img_bgr
                    else:
                        self.stats[camera_id]['errors'] += 1
                        
                except Exception as e:
                    self.stats[camera_id]['errors'] += 1
                    print(f"❌ Error decoding Camera {camera_id}: {e}")
        
        return None
    
    def _receive_loop(self):
        """Main receive loop (runs in separate thread)."""
        print(f"📡 Receive loop started")
        
        while self.running:
            try:
                data, addr = self.sock.recvfrom(self.BUFFER_SIZE)
                result = self.process_packet(data)
                
                if result:
                    camera_id, img_rgb, img_bgr = result
                    fps = self.calculate_fps(camera_id)
                    timestamp = time.time()
                    
                    # Call all registered callbacks
                    for callback in self.callbacks:
                        try:
                            callback(camera_id, img_rgb, img_bgr, timestamp, fps)
                        except Exception as e:
                            print(f"❌ Callback error in {callback.__name__}: {e}")
                
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"❌ Error in receive loop: {e}")
        
        print("📡 Receive loop stopped")
    
    def start(self):
        """Start the receiver in a background thread."""
        if self.running:
            print("⚠️  Receiver already running")
            return
        
        try:
            # Create and bind socket
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind((self.UDP_IP, self.UDP_PORT))
            self.sock.settimeout(0.1)  # 100ms timeout for clean shutdown
            
            # Start receive thread
            self.running = True
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()
            
            print(f"✅ Stereo receiver started on {self.UDP_IP}:{self.UDP_PORT}")
            
        except Exception as e:
            print(f"❌ Failed to start receiver: {e}")
            self.running = False
            if self.sock:
                self.sock.close()
            raise
    
    def stop(self):
        """Stop the receiver and clean up resources."""
        if not self.running:
            return
        
        print("🛑 Stopping receiver...")
        self.running = False
        
        # Wait for thread to finish
        if self.receive_thread:
            self.receive_thread.join(timeout=2.0)
        
        # Close socket
        if self.sock:
            self.sock.close()
        
        print("✅ Receiver stopped")
    
    def get_stats(self) -> dict:
        """Get reception statistics."""
        return {
            'camera_1': self.stats[1].copy(),
            'camera_2': self.stats[2].copy()
        }
    
    def print_stats(self):
        """Print current statistics."""
        print("\n📊 Reception Statistics:")
        for cam_id in [1, 2]:
            stats = self.stats[cam_id]
            print(f"  Camera {cam_id}: "
                  f"Received={stats['received']}, "
                  f"Errors={stats['errors']}, "
                  f"FPS={stats['fps']:.1f}")
    
    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.stop()


# ============================================================================
# EXAMPLE USAGE
# ============================================================================

def example_callback(camera_id: int, img_rgb: np.ndarray, img_bgr: np.ndarray, 
                     timestamp: float, fps: float):
    """
    Example callback function that prints info about received images.
    Replace this with your own processing logic.
    """
    print(f"📸 Camera {camera_id}: {img_rgb.shape}, {fps:.1f} FPS, t={timestamp:.3f}")
    
    # Example: Save image (uncomment if needed)
    # cv2.imwrite(f"camera_{camera_id}_latest.jpg", img_bgr)


def main():
    """Example main function showing how to use the receiver."""
    
    print("=" * 60)
    print("Stereo Camera Receiver - Headless Mode")
    print("=" * 60)
    
    # Create receiver
    receiver = StereoImageReceiver(udp_ip="0.0.0.0", udp_port=9999)
    
    # Register callback(s)
    receiver.register_callback(example_callback)
    
    # Setup signal handler for clean shutdown
    def signal_handler(sig, frame):
        print("\n\n🛑 Interrupt received, shutting down...")
        receiver.stop()
        receiver.print_stats()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    
    # Start receiver
    receiver.start()
    
    print("\n✅ Receiver running. Waiting for images...")
    print("Press Ctrl+C to exit\n")
    
    # Keep main thread alive
    try:
        while True:
            time.sleep(1)
            # Optionally print stats periodically
            # receiver.print_stats()
    except KeyboardInterrupt:
        pass
    finally:
        receiver.stop()
        receiver.print_stats()


if __name__ == "__main__":
    main()
    
    
    
    
# # your_processing.py
# from stereo_receiver import StereoImageReceiver
# import cv2
# import os

# def my_image_processor(camera_id, img_rgb, img_bgr, timestamp, fps):
#     """Your custom processing function"""
#     # Save to disk
#     filename = f"cam{camera_id}_{int(timestamp*1000)}.jpg"
#     cv2.imwrite(filename, img_bgr)
    
#     # Do inference, detection, etc.
#     # ...

# # Create and run
# receiver = StereoImageReceiver()
# receiver.register_callback(my_image_processor)
# receiver.start()

# # Keep running...
import socket
import struct

UDP_IP = "0.0.0.0"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening on {UDP_IP}:{UDP_PORT}")

while True:
    print("Inside loop")
    data, addr = sock.recvfrom(1024)
    if len(data) == 4:
        counter = struct.unpack('i', data)[0]
        print(f"Received counter from {addr}: {counter}")

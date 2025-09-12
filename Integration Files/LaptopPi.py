# laptop_server.py
import socket

HOST = '192.168.100.1'  # Laptop IP
PORT = 5000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))
s.listen(1)
print("Server listening...")

conn, addr = s.accept()
print(f"Connected by {addr}")

while True:
    data = conn.recv(1024)
    if not data:
        break
    print("Received:", data.decode().strip())
    conn.sendall(b"ACK\n")

# ethernet_server.py
import socket
import time

HOST = '192.168.100.1'
PORT = 5000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))
s.listen(1)
conn, addr = s.accept()
print("Connected by", addr)

while True:
    data = conn.recv(4096)
    if not data:
        break
    # Simply discard data to measure throughput

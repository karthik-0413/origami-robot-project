# ethernet_client_test.py
import socket
import time

HOST = '192.168.100.1'
PORT = 5000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

data = b"x" * 1024  # 1 KB per message
count = 0
start = time.time()

while time.time() - start < 10:  # run 10 seconds
    s.sendall(data)
    count += 1

elapsed = time.time() - start
print(f"Sent {count} KB in {elapsed:.2f} s, throughput = {count/elapsed:.2f} KB/s")
# 113823.53 KB per second on a wired connection
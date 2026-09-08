import serial
import numpy as np
import socket

PORT = "COM5"
BAUD = 115200
ALPHA = 0.05

UDP_IP = "127.0.0.1"
UDP_PORT = 5005

gravity = np.array([0.0, 0.0, -1.0])

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"Connecting to Arduino on {PORT}...")
ser = serial.Serial(PORT, BAUD, timeout=1)
print(f"Connected. Sending UDP to {UDP_IP}:{UDP_PORT}")

while True:
    try:
        line = ser.readline().decode().strip()
        if not line:
            continue

        parts = line.split(',')
        if len(parts) != 3:
            continue

        ax, ay, az = map(int, parts)

        raw = np.array([ax, ay, az], dtype=float) / 16384.0

        mag = np.linalg.norm(raw)
        if mag < 0.1:
            continue

        normalized = raw / mag

        gravity = gravity * (1.0 - ALPHA) + normalized * ALPHA

        # Pack as 3 floats and send over UDP
        message = f"{gravity[0]:.4f},{-gravity[1]:.4f},{gravity[2]:.4f}"
        sock.sendto(message.encode(), (UDP_IP, UDP_PORT))

    except ValueError:
        continue
    except KeyboardInterrupt:
        print("Stopped.")
        ser.close()
        sock.close()
        break
    except Exception as e:
        print(f"Error: {e}")
        continue
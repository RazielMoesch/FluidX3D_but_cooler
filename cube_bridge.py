"""Reads FluidX3D's rendered faces from a named pipe, streams each face to its own Qualia over USB,
and sends gravity back to FluidX3D from the keyboard.

    pip install numpy opencv-python pyserial
    python cube_bridge.py                 # then start FluidX3D (./make.sh) in another terminal
    python cube_bridge.py --faces 2       # must match NUM_FACES in fluidcube_setup.cpp
    python cube_bridge.py --size 240      # must match GRAPHICS_FRAME_WIDTH/HEIGHT in defines.hpp
    python cube_bridge.py --ports /dev/cu.usbmodem1101 /dev/cu.usbmodem1201   # face 0, face 1

Without --ports, boards are found automatically and sorted by USB serial number, so the same
board always gets the same face. The face -> port mapping is printed at startup.

Faces are sent in lockstep: each face gets the frame from the same simulation time step, and the
next frame only goes out once every board has acked. That keeps neighbouring faces in sync at the
edges (fps = slowest board).

Keys (press in this terminal, no Enter needed):
  W / S     gravity up / down        (+z / -z)
  A / D     gravity left / right     (-x / +x)
  Q / E     gravity toward / away    (-y / +y)
  R         toggle slowly rotating gravity
  SPACE     zero gravity
  + / -     stronger / weaker gravity
If a key moves the water the "wrong" way for your camera view, swap its vector in KEYS below.

Pipeline: FluidX3D draw_frame() x faces -> /tmp/fluidcube (raw BGRA) -> JPEG per face -> USB -> one Qualia per face
          keyboard -> /tmp/fluidcube_ctrl (3 floats) -> FluidX3D lbm.set_f()
"""
import argparse, math, os, select, struct, sys, termios, threading, time, tty
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import cv2
import serial
from serial.tools import list_ports

FIFO = "/tmp/fluidcube"
CTRL = "/tmp/fluidcube_ctrl"
HEADER = b"\xA5\x5A"
BOARD_VIDS = {0x303A, 0x239A}     # Espressif, Adafruit

KEYS = {                          # key -> gravity direction (x, y, z) in sim coordinates
    "w": (0, 0, 1),   "s": (0, 0, -1),
    "a": (-1, 0, 0),  "d": (1, 0, 0),
    "q": (0, -1, 0),  "e": (0, 1, 0),
    " ": (0, 0, 0),
}


class Latest:
    """Newest frame from FluidX3D; older unsent frames are simply overwritten."""
    def __init__(self):
        self.cond = threading.Condition()
        self.frame, self.seq = None, 0

    def put(self, f):
        with self.cond:
            self.frame, self.seq = f, self.seq + 1
            self.cond.notify_all()

    def wait_newer(self, seq):
        with self.cond:
            self.cond.wait_for(lambda: self.seq != seq)
            return self.frame, self.seq


class Gravity:
    """Current gravity setting; writes it to FluidX3D's control pipe whenever it changes."""
    def __init__(self):
        self.fd = None
        self.dir = (0.0, 0.0, -1.0)
        self.strength = 1.0
        self.rotating = False
        self.angle = 0.0

    def connect(self):
        self.fd = os.open(CTRL, os.O_WRONLY)       # FluidX3D already has the read end open
        self.send()

    def send(self):
        if self.fd is None:
            return
        v = [c * self.strength for c in self.dir]
        try:
            os.write(self.fd, struct.pack("<3f", *v))
        except OSError:
            pass                                    # FluidX3D gone; frame reader will exit

    def key(self, ch):
        if ch in KEYS:
            self.rotating = False
            self.dir = tuple(float(c) for c in KEYS[ch])
            print(f"gravity {self.dir}")
        elif ch == "r":
            self.rotating = not self.rotating
            print("rotating gravity", "on" if self.rotating else "off")
        elif ch in "+=":
            self.strength = min(self.strength * 1.25, 4.0)
            print(f"strength {self.strength:.2f}x")
        elif ch == "-":
            self.strength = max(self.strength / 1.25, 0.1)
            print(f"strength {self.strength:.2f}x")
        else:
            return
        self.send()

    def tick(self, dt):                             # advances the rotating mode
        if self.rotating:
            self.angle += 0.6 * dt                  # rad/s
            self.dir = (math.sin(self.angle), 0.0, -math.cos(self.angle))
            self.send()


def key_loop(grav):
    """Single keypresses from this terminal (cbreak mode is set in main)."""
    last = time.perf_counter()
    while True:
        if select.select([sys.stdin], [], [], 0.05)[0]:
            grav.key(sys.stdin.read(1).lower())
        now = time.perf_counter()
        grav.tick(now - last)
        last = now


def pipe_reader(latest, grav, size, faces):
    """Read one bundle (all faces of one time step) at a time, as fast as FluidX3D writes them."""
    nbytes = faces * size * size * 4
    with open(FIFO, "rb", buffering=0) as f:          # waits here until FluidX3D opens the pipe
        grav.connect()
        print("FluidX3D connected. Keys: WASD/QE direction, R rotate, SPACE zero-g, +/- strength")
        while True:
            buf = bytearray(nbytes)
            view, got = memoryview(buf), 0
            while got < nbytes:
                n = f.readinto(view[got:])
                if not n:
                    print("FluidX3D closed the pipe")
                    os._exit(0)
                got += n
            latest.put(buf)


def find_ports(n):
    """All connected boards, sorted by USB serial number (unique per chip) so face order is stable."""
    boards = sorted((p for p in list_ports.comports() if p.vid in BOARD_VIDS), key=lambda p: p.serial_number or p.device)
    if len(boards) < n:
        raise SystemExit(f"Found {len(boards)} board(s), need {n}. Check data cables, close the Arduino "
                         f"Serial Monitor, or pass --ports.")
    for i, p in enumerate(boards[:n]):
        print(f"face {i}: {p.device}  (serial {p.serial_number})")
    return [p.device for p in boards[:n]]


def open_port(port):
    ser = serial.Serial()
    ser.port, ser.baudrate = port, 115200
    ser.timeout, ser.write_timeout = 1, 2
    ser.dtr, ser.rts = True, False        # DTR on: TinyUSB drops data without it. RTS off: avoids reset
    ser.open()
    time.sleep(2.0)
    ser.reset_input_buffer()
    return ser


def send_face(ser, bgr, params):
    """Encode one face, send it, wait for that board's ack. Runs in its own thread per face;
    OpenCV and serial I/O release the GIL, so faces really run in parallel."""
    t0 = time.perf_counter()
    ok, jpg = cv2.imencode(".jpg", bgr, params)
    data = jpg.tobytes()
    try:
        ser.write(HEADER + struct.pack("<I", len(data)) + data)
        acked = ser.read(1) == b"K"
    except serial.SerialException:
        acked = False
    return acked, len(data), (time.perf_counter() - t0) * 1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", nargs="+", help="one port per face, in face order")
    ap.add_argument("--faces", type=int, default=2, help="must match NUM_FACES in fluidcube_setup.cpp")
    ap.add_argument("--size", type=int, default=240, help="must match GRAPHICS_FRAME_WIDTH/HEIGHT")
    ap.add_argument("--quality", type=int, default=85)
    args = ap.parse_args()

    for p in (FIFO, CTRL):
        if not os.path.exists(p):
            os.mkfifo(p)
    ports = args.ports or find_ports(args.faces)
    if len(ports) != args.faces:
        raise SystemExit(f"--ports lists {len(ports)} port(s) but --faces is {args.faces}")
    sers = [open_port(p) for p in ports]
    pool = ThreadPoolExecutor(max_workers=args.faces)
    latest, grav = Latest(), Gravity()
    threading.Thread(target=pipe_reader, args=(latest, grav, args.size, args.faces), daemon=True).start()
    print(f"Waiting for FluidX3D on {FIFO} ...")

    old_term = termios.tcgetattr(sys.stdin)        # keypresses without Enter; Ctrl+C still works
    tty.setcbreak(sys.stdin.fileno())
    threading.Thread(target=key_loop, args=(grav,), daemon=True).start()

    params = [cv2.IMWRITE_JPEG_QUALITY, args.quality]
    seq, shown, nbytes, lat = 0, 0, 0, 0.0
    sim_prev, t_print = 0, time.perf_counter()
    try:
        while True:
            raw, seq = latest.wait_newer(seq)
            imgs = np.frombuffer(raw, np.uint8).reshape(args.faces, args.size, args.size, 4)[..., :3]  # BGRA -> BGR views
            results = list(pool.map(send_face, sers, imgs, [params] * args.faces))   # all faces at once
            if all(r[0] for r in results):
                shown += 1
                nbytes += sum(r[1] for r in results) / args.faces
                lat += max(r[2] for r in results)          # a frame is done when the slowest face is

            now = time.perf_counter()
            if now - t_print >= 5.0:                   # every 5 s, so key feedback stays readable
                dt = now - t_print
                print(f"sim {(latest.seq - sim_prev) / dt:5.1f} fps | shown {shown / dt:5.1f} fps | "
                      f"{nbytes / max(shown, 1) / 1024:5.1f} KB | lat {lat / max(shown, 1):5.1f} ms")
                sim_prev, shown, nbytes, lat, t_print = latest.seq, 0, 0, 0.0, now
    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_term)
        for ser in sers:
            ser.close()


if __name__ == "__main__":
    main()
"""Reinicia el ESP32 y muestra lo que imprime por el puerto serie.

Uso: python leer_serial.py [PUERTO] [SEGUNDOS] [--sin-reset]
"""
import serial, time, sys

args = [a for a in sys.argv[1:] if not a.startswith("--")]
port = args[0] if len(args) > 0 else "COM4"
secs = float(args[1]) if len(args) > 1 else 6
s = serial.Serial(port, 115200, timeout=0.2)
if "--sin-reset" not in sys.argv:
    # Reinicio por RTS (EN) con DTR bajo (GPIO0 libre) para arrancar en modo normal
    s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
else:
    s.dtr = False; s.rts = False
t0 = time.time(); buf = b""
while time.time() - t0 < secs:
    buf += s.read(4096)
s.close()
print(buf.decode("utf-8", errors="replace"))

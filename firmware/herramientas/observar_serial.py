"""Observa el puerto serie sin reiniciar el ESP32 y consulta la calibración.

DTR y RTS se fijan antes de abrir el puerto para no activar el reinicio automático.
Uso: python observar_serial.py [PUERTO] [SEGUNDOS]
"""
import serial
import sys
import time

port = sys.argv[1] if len(sys.argv) > 1 else "COM4"
segundos = float(sys.argv[2]) if len(sys.argv) > 2 else 15

s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 0.1
s.dtr = False
s.rts = False
s.open()
s.write(b"D\n")
t0 = time.time()
buf = b""
while time.time() - t0 < segundos:
    buf += s.read(4096)
s.close()
print(buf.decode("utf-8", "replace"))

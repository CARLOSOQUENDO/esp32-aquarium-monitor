"""Envía comandos al ESP32 con pausas y muestra la respuesta.

Uso: python comando_serial.py PUERTO "ESPERA:3" "P0" "ESPERA:5" "P1" "ESPERA:5"
"""
import serial, time, sys
s = serial.Serial(sys.argv[1], 115200, timeout=0.1)
s.dtr = False; s.rts = False
out = b""
for paso in sys.argv[2:]:
    if paso.startswith("ESPERA:"):
        t0 = time.time()
        while time.time() - t0 < float(paso.split(":")[1]): out += s.read(4096)
    else:
        s.write((paso + "\n").encode()); out += f"\n### enviado {paso}\n".encode()
s.close()
print(out.decode("utf-8", "replace"))

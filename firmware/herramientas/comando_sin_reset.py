"""Envía comandos al monitor sin reiniciarlo (DTR y RTS fijos antes de abrir).

Uso: python comando_sin_reset.py PUERTO "ESPERA:2" "R" "S" "ESPERA:15"
"""
import serial, sys, time

s = serial.Serial()
s.port, s.baudrate, s.timeout = sys.argv[1], 115200, 0.1
s.dtr = False
s.rts = False
s.open()
out = b""
for paso in sys.argv[2:]:
    if paso.startswith("ESPERA:"):
        t0 = time.time()
        while time.time() - t0 < float(paso.split(":")[1]):
            out += s.read(4096)
    else:
        s.write((paso + "\n").encode())
        out += f"\n### enviado {paso}\n".encode()
s.close()
print(out.decode("utf-8", "replace"))

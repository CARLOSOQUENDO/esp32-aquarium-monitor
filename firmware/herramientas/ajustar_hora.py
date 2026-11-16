"""Reinicia el ESP32, espera el arranque y ajusta el RTC con la hora del computador.

El firmware debe aceptar el comando  T2026-10-29 15:04:05  por el puerto serie.
Uso: python ajustar_hora.py [PUERTO] [SEGUNDOS_DE_ARRANQUE]
"""
import datetime as dt
import serial
import sys
import time

port = sys.argv[1] if len(sys.argv) > 1 else "COM4"
arranque = float(sys.argv[2]) if len(sys.argv) > 2 else 6

s = serial.Serial(port, 115200, timeout=0.1)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False


def leer(segundos):
    t0 = time.time(); buf = b""
    while time.time() - t0 < segundos:
        buf += s.read(4096)
    return buf.decode("utf-8", "replace")


print(leer(arranque))

# Esperar el cambio de segundo para que la hora quede exacta
ahora = dt.datetime.now()
time.sleep(1 - ahora.microsecond / 1e6)
hora = dt.datetime.now().replace(microsecond=0)
s.write(f"T{hora:%Y-%m-%d %H:%M:%S}\n".encode())
print(f">> enviado T{hora:%Y-%m-%d %H:%M:%S}")
print(leer(4))
s.close()

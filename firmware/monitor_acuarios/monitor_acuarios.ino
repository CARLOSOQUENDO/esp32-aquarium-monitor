/*
 * Monitor de acuarios ESP32 - firmware principal
 *
 *   Acuario A: temperatura y TDS
 *   Acuario B: temperatura, pH y TDS
 *
 * Archivos:
 *   config.h      pines, constantes y valores de fábrica
 *   almacen.h     calibración y su guardado en flash
 *   entradas.h    encoder y pulsador
 *   mediciones.h  lectura de sensores sin bloqueo y cálculos
 *   pantalla.h    LCD 16x2
 *   interfaz.h    pantallas, menú y calibración guiada
 *
 * Comandos por el puerto serie (115200 baudios):
 *   R / L [n]   girar el encoder n pasos a la derecha / izquierda
 *   S / H       pulsación corta / larga
 *   T2026-10-29 15:04:05   ajustar la hora del RTC
 *   D           mostrar la calibración guardada
 *   E           activar o desactivar la copia del LCD por el puerto serie
 */

#include <Wire.h>
#include "config.h"
#include "almacen.h"
#include "entradas.h"
#include "mediciones.h"
#include "pantalla.h"
#include "interfaz.h"

const char* VERSION_FIRMWARE = "1.2";

void mostrarCalibracion() {
  Serial.printf("CAL version=%lu\n", (unsigned long)cal.version);
  Serial.printf("CAL pH n=%d tCal=%.2f fecha=%lu\n", cal.nPH, cal.tCalPH, (unsigned long)cal.fechaPH);
  for (int i = 0; i < cal.nPH; i++)
    Serial.printf("CAL pH punto %d: pH=%.3f Po=%.4f V\n", i, cal.ph[i].ph, cal.ph[i].po);
  Serial.printf("CAL TDS B k=%.4f fecha=%lu | TDS A k=%.4f fecha=%lu\n", cal.kTdsB,
                (unsigned long)cal.fechaTdsB, cal.kTdsA, (unsigned long)cal.fechaTdsA);
  Serial.printf("CAL temp offA=%+.3f offB=%+.3f fecha=%lu\n", cal.offA, cal.offB,
                (unsigned long)cal.fechaTemp);
}

void procesarSerie() {
  if (!Serial.available()) return;
  String c = Serial.readStringUntil('\n');
  c.trim();
  if (c.length() == 0) return;
  char cmd = toupper(c[0]);
  int n = c.length() > 1 ? max(1, (int)c.substring(1).toInt()) : 1;
  switch (cmd) {
    case 'R': simPasos += n; break;
    case 'L': simPasos -= n; break;
    case 'S': simPulsacion = Pulsacion::CORTA; break;
    case 'H': simPulsacion = Pulsacion::LARGA; break;
    case 'D': mostrarCalibracion(); break;
    case 'E': espejoSerie = !espejoSerie; break;
    case 'T': {
      int a, me, d, h, mi, s;
      if (rtcPresente && sscanf(c.c_str() + 1, "%d-%d-%d %d:%d:%d", &a, &me, &d, &h, &mi, &s) == 6) {
        rtc.adjust(DateTime(a, me, d, h, mi, s));
        rtcSinHora = false;
        Serial.printf("HORA AJUSTADA a %04d-%02d-%02d %02d:%02d:%02d\n", a, me, d, h, mi, s);
      }
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(300);
  Serial.printf("\n=== Monitor de acuarios v%s ===\n", VERSION_FIRMWARE);

  Wire.begin(PIN_SDA, PIN_SCL);
  iniciarPantalla();
  linea(0, "Monitor acuarios");
  linea(1, "v%s", VERSION_FIRMWARE);
  refrescarPantalla();

  cargarCalibracion();
  mostrarCalibracion();

  rtcPresente = rtc.begin(&Wire);
  rtcSinHora = rtcPresente && rtc.lostPower();
  Serial.printf("RTC: %s\n", !rtcPresente ? "no responde" : rtcSinHora ? "perdio la hora" : "OK");

  iniciarEntradas();
  iniciarMediciones();
  if (!adsPresente) Serial.println("ADS1115 no responde");

  delay(1500);
  tUltimaAccion = millis();
}

void loop() {
  actualizarMediciones();
  procesarSerie();
  manejarEntrada(leerGiro(), leerPulsador());

  static uint32_t tDibujo = 0;
  if (millis() - tDibujo >= 100) {
    tDibujo = millis();
    dibujarInterfaz();
  }
}

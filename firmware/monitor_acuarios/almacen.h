// Calibración del equipo y su guardado en la memoria flash (NVS) del ESP32.
#pragma once
#include <Preferences.h>
#include "config.h"

struct PuntoPH {
  float ph;  // valor real del buffer a la temperatura de calibración
  float po;  // voltaje Po medido (V)
};

struct Calibracion {
  uint32_t version;
  // pH: 2 o 3 puntos, ordenados de menor a mayor pH
  uint8_t nPH;
  PuntoPH ph[3];
  float tCalPH;        // temperatura media durante la calibración (°C)
  uint32_t fechaPH;    // segundos Unix; 0 = valores de fábrica
  // TDS: factor de corrección sobre la fórmula del fabricante
  float kTdsB, kTdsA;
  uint32_t fechaTdsB, fechaTdsA;
  // Temperatura: corrección sumada a la lectura del sensor
  float offA, offB;
  uint32_t fechaTemp;
};

constexpr uint32_t VERSION_CAL = 1;
Calibracion cal;

void calibracionFabrica(Calibracion& c) {
  c.version = VERSION_CAL;
  c.nPH = 3;
  c.ph[0] = {4.00f, FAB_PO_PH4};
  c.ph[1] = {7.00f, FAB_PO_PH7};
  c.ph[2] = {10.00f, FAB_PO_PH10};
  c.tCalPH = FAB_T_CAL_PH;
  c.fechaPH = 0;
  c.kTdsB = c.kTdsA = 1.0f;
  c.fechaTdsB = c.fechaTdsA = 0;
  c.offA = c.offB = 0.0f;
  c.fechaTemp = 0;
}

void cargarCalibracion() {
  Preferences p;
  bool ok = false;
  if (p.begin("acuario", true)) {
    if (p.getBytesLength("cal") == sizeof(cal)) {
      p.getBytes("cal", &cal, sizeof(cal));
      ok = (cal.version == VERSION_CAL);
    }
    p.end();
  }
  if (!ok) calibracionFabrica(cal);
}

void guardarCalibracion() {
  Preferences p;
  p.begin("acuario", false);
  p.putBytes("cal", &cal, sizeof(cal));
  p.end();
}

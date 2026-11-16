// Encoder EC11 y pulsador: giros por interrupción, pulsación corta y larga.
// También acepta entradas simuladas desde el puerto serie (R, L, S, H).
#pragma once
#include "config.h"

enum class Pulsacion { NINGUNA, CORTA, LARGA };

constexpr uint32_t T_ANTIRREBOTE_MS = 25;
constexpr uint32_t T_PULSACION_LARGA_MS = 1000;

volatile int32_t encDetentes = 0;
volatile int8_t encCuartos = 0;
volatile uint8_t encAnterior = 3;

// Decodificación en cuadratura con tabla de transiciones. Cada detente
// ("clic") del EC11 son 4 transiciones y termina con A y B en alto (estado 3).
// Se cuenta un paso al llegar al reposo; las transiciones inválidas (rebotes)
// valen 0 y no acumulan error.
void IRAM_ATTR isrEncoder() {
  static const int8_t TABLA[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  int8_t cuartos = encCuartos + TABLA[(encAnterior << 2) | ab];
  encAnterior = ab;
  if (ab == 3) {
    if (cuartos >= 2) encDetentes = encDetentes + 1;
    else if (cuartos <= -2) encDetentes = encDetentes - 1;
    cuartos = 0;
  }
  encCuartos = cuartos;
}

int simPasos = 0;
Pulsacion simPulsacion = Pulsacion::NINGUNA;

void iniciarEntradas() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encAnterior = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isrEncoder, CHANGE);
}

// Devuelve los pasos girados desde la última llamada.
int leerGiro() {
  static int32_t previo = 0;
  noInterrupts();
  int32_t actual = encDetentes;
  interrupts();
  int pasos = actual - previo + simPasos;
  previo = actual;
  simPasos = 0;
  return pasos;
}

// La pulsación larga se informa apenas se cumple 1 s, sin esperar a soltar.
Pulsacion leerPulsador() {
  static bool estable = HIGH, ultimoLeido = HIGH, largaInformada = false;
  static uint32_t tCambio = 0, tPresion = 0;

  if (simPulsacion != Pulsacion::NINGUNA) {
    Pulsacion p = simPulsacion;
    simPulsacion = Pulsacion::NINGUNA;
    return p;
  }

  bool leido = digitalRead(PIN_ENC_SW);
  uint32_t ahora = millis();
  if (leido != ultimoLeido) { ultimoLeido = leido; tCambio = ahora; }

  if (ahora - tCambio >= T_ANTIRREBOTE_MS && leido != estable) {
    estable = leido;
    if (estable == LOW) {
      tPresion = ahora;
      largaInformada = false;
    } else if (!largaInformada) {
      return Pulsacion::CORTA;
    }
  }
  if (estable == LOW && !largaInformada && ahora - tPresion >= T_PULSACION_LARGA_MS) {
    largaInformada = true;
    return Pulsacion::LARGA;
  }
  return Pulsacion::NINGUNA;
}

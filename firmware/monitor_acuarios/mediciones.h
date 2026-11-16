// Mediciones sin bloqueo: una máquina de estados avanza un paso corto en cada
// llamada, así el menú sigue respondiendo mientras se mide.
//
// Modo normal (TDS encendidos):
//   promedia 32 muestras del TDS B y luego del TDS A, recoge las temperaturas
//   y espera 1 s antes del siguiente ciclo. El pH no se mide.
// Modo pH (bajo pedido o durante la calibración):
//   apaga ambos TDS (su corriente altera el electrodo de pH), espera 500 ms y
//   promedia el pH continuamente en bloques de 250 ms (15 ciclos de 60 Hz).
#pragma once
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "almacen.h"

Adafruit_ADS1115 ads;
OneWire busOneWire(PIN_ONEWIRE);
DallasTemperature termometros(&busOneWire);
bool adsPresente = false;

struct Lecturas {
  float tCrudoA = NAN, tCrudoB = NAN;  // sin corrección
  float tA = NAN, tB = NAN;            // con la corrección de calibración
  float po = NAN, ph = NAN;
  bool phValido = false;
  float vB = NAN, vA = NAN;            // voltaje de cada TDS
  float tdsCrudoB = NAN, tdsCrudoA = NAN;  // con compensación de temperatura, k = 1
  float tdsB = NAN, tdsA = NAN;        // con el factor k de calibración
  uint32_t ciclos = 0;
};
Lecturas lec;

// Historial de lecturas recientes, para decidir si una lectura está estable.
// En modo pH se agrega una muestra cada 250 ms: 64 muestras cubren 16 s.
struct Muestra { uint32_t t; float po, tdsCrudoB, tdsCrudoA; };
constexpr int N_HISTORIAL = 64;
Muestra historial[N_HISTORIAL];
int nHistorial = 0, posHistorial = 0;

void agregarHistorial(const Muestra& m) {
  historial[posHistorial] = m;
  posHistorial = (posHistorial + 1) % N_HISTORIAL;
  if (nHistorial < N_HISTORIAL) nHistorial++;
}

// ---------------- Cálculos ----------------

// Tablas de buffers comerciales: valor real entre 10 y 40 °C, cada 5 °C.
// Verifique siempre contra la tabla impresa en el frasco.
struct TablaBuffer { float nominal; float v[7]; };
const TablaBuffer TABLAS_BUFFER[] = {
  {4.00f,  {4.00f, 4.00f, 4.00f, 4.00f, 4.01f, 4.02f, 4.03f}},
  {4.01f,  {4.00f, 4.00f, 4.00f, 4.01f, 4.01f, 4.02f, 4.03f}},
  {6.86f,  {6.92f, 6.90f, 6.88f, 6.86f, 6.85f, 6.84f, 6.84f}},
  {7.00f,  {7.07f, 7.04f, 7.02f, 7.00f, 6.99f, 6.98f, 6.97f}},
  {9.18f,  {9.33f, 9.28f, 9.23f, 9.18f, 9.14f, 9.11f, 9.07f}},
  {10.00f, {10.18f, 10.12f, 10.06f, 10.00f, 9.96f, 9.92f, 9.88f}},
  {10.01f, {10.18f, 10.12f, 10.06f, 10.01f, 9.97f, 9.93f, 9.89f}},
};

float valorBuffer(float nominal, float t) {
  if (isnan(t)) return nominal;
  for (const auto& tb : TABLAS_BUFFER) {
    if (fabs(tb.nominal - nominal) > 0.001f) continue;
    float x = constrain((t - 10.0f) / 5.0f, 0.0f, 6.0f);
    int i = min((int)x, 5);
    return tb.v[i] + (tb.v[i + 1] - tb.v[i]) * (x - i);
  }
  return nominal;
}

// Pendiente teórica de Nernst (V por unidad de pH) a la temperatura t.
float pendienteNernst(float t) { return 0.00019842f * (t + 273.15f); }

// pH por tramos. Cada tramo es la recta entre dos puntos de calibración; la
// pendiente se corrige por temperatura (Nernst es proporcional a la temperatura
// absoluta) alrededor del punto más cercano a pH 7.
float calcularPH(float po, float t) {
  const PuntoPH* p = cal.ph;
  int iMedio = 0;
  for (int i = 1; i < cal.nPH; i++)
    if (fabs(p[i].ph - 7.0f) < fabs(p[iMedio].ph - 7.0f)) iMedio = i;

  int i1, i2;
  if (cal.nPH == 2) { i1 = 0; i2 = 1; }
  else if (po >= p[iMedio].po) { i1 = 0; i2 = 1; }  // voltaje mayor = zona ácida
  else { i1 = 1; i2 = 2; }

  float m = (p[i2].ph - p[i1].ph) / (p[i2].po - p[i1].po);  // pH por voltio
  float kT = isnan(t) ? 1.0f : (t + 273.15f) / (cal.tCalPH + 273.15f);
  return p[iMedio].ph + (m / kT) * (po - p[iMedio].po);
}

// Fórmula del fabricante (DFRobot SEN0244), compensada a 25 °C con 2 %/°C.
float tdsFormula(float v, float t) {
  if (isnan(t)) t = 25.0f;
  float vc = v / (1.0f + 0.02f * (t - 25.0f));
  return (133.42f * vc * vc * vc - 255.86f * vc * vc + 857.39f * vc) * 0.5f;
}

// Verdadero si la variable cambió menos que la tolerancia en la ventana,
// contando solo muestras posteriores a 'desde' (inicio de la medición).
bool lecturaEstable(float Muestra::*campo, float tolAbs, float tolRel, uint32_t desde) {
  uint32_t ahora = millis();
  float vmin = INFINITY, vmax = -INFINITY, ultimo = NAN;
  uint32_t tMasAntigua = ahora;
  int n = 0;
  for (int k = 0; k < nHistorial; k++) {
    const Muestra& m = historial[k];
    if (m.t < desde || ahora - m.t > VENTANA_ESTABILIDAD_MS) continue;
    float v = m.*campo;
    if (isnan(v)) return false;
    vmin = min(vmin, v); vmax = max(vmax, v); ultimo = v;
    if (m.t < tMasAntigua) tMasAntigua = m.t;
    n++;
  }
  if (n < 3 || ahora - tMasAntigua < VENTANA_ESTABILIDAD_MS * 0.7f) return false;
  float tol = max(tolAbs, tolRel * fabs(ultimo));
  return (vmax - vmin) <= tol;
}

// Promedio de las últimas n muestras válidas del historial.
float promedioReciente(float Muestra::*campo, int n) {
  float suma = 0; int c = 0;
  for (int k = 0; k < nHistorial && c < n; k++) {
    int i = (posHistorial - 1 - k + N_HISTORIAL) % N_HISTORIAL;
    float v = historial[i].*campo;
    if (isnan(v)) continue;
    suma += v; c++;
  }
  return c ? suma / c : NAN;
}

// ---------------- Máquina de estados ----------------

enum class Fase { ESPERA_TDS, MUESTREO_TDS_B, MUESTREO_TDS_A, ESPERA_PH, MUESTREO_PH };
Fase fase = Fase::ESPERA_TDS;
uint32_t tFase = 0;
uint32_t esperaFase = T_TDS_ARRANQUE_MS;
uint32_t tPedidoTemp = 0;
bool modoPH = false;

const uint16_t MUX_CANAL[4] = {ADS1X15_REG_CONFIG_MUX_SINGLE_0, ADS1X15_REG_CONFIG_MUX_SINGLE_1,
                               ADS1X15_REG_CONFIG_MUX_SINGLE_2, ADS1X15_REG_CONFIG_MUX_SINGLE_3};
int muestrasTomadas = 0;
float sumaMuestras = 0;
bool conversionEnCurso = false;

void reiniciarMuestreo() {
  muestrasTomadas = 0;
  sumaMuestras = 0;
  conversionEnCurso = false;
}

// Toma muestras del canal sin bloquear. Devuelve true y el promedio en
// 'resultado' cuando completa las N_MUESTRAS.
bool muestrear(uint8_t canal, float& resultado) {
  if (!conversionEnCurso) {
    ads.startADCReading(MUX_CANAL[canal], false);
    conversionEnCurso = true;
    return false;
  }
  if (!ads.conversionComplete()) return false;
  sumaMuestras += ads.computeVolts(ads.getLastConversionResults());
  conversionEnCurso = false;
  if (++muestrasTomadas < N_MUESTRAS) return false;
  resultado = sumaMuestras / N_MUESTRAS;
  reiniciarMuestreo();
  return true;
}

void encenderTDS(bool on) {
  digitalWrite(PIN_TDS_B_VCC, on ? HIGH : LOW);
  digitalWrite(PIN_TDS_A_VCC, on ? HIGH : LOW);
}

// Cambia entre el modo normal (TDS) y el modo pH.
void activarModoPH(bool activar) {
  if (activar == modoPH) return;
  modoPH = activar;
  reiniciarMuestreo();
  encenderTDS(!activar);
  tFase = millis();
  if (activar) { fase = Fase::ESPERA_PH; esperaFase = T_TDS_APAGADO_MS; }
  else { fase = Fase::ESPERA_TDS; esperaFase = T_TDS_ARRANQUE_MS; lec.phValido = false; }
}

void actualizarTemperaturas() {
  if (millis() - tPedidoTemp < T_CONVERSION_DS18B20_MS) return;
  float a = termometros.getTempC(DIR_SENSOR_A);
  float b = termometros.getTempC(DIR_SENSOR_B);
  lec.tCrudoA = (a == DEVICE_DISCONNECTED_C) ? NAN : a;
  lec.tCrudoB = (b == DEVICE_DISCONNECTED_C) ? NAN : b;
  lec.tA = lec.tCrudoA + cal.offA;
  lec.tB = lec.tCrudoB + cal.offB;
  termometros.requestTemperatures();
  tPedidoTemp = millis();
}

void iniciarMediciones() {
  pinMode(PIN_TDS_B_VCC, OUTPUT);
  pinMode(PIN_TDS_A_VCC, OUTPUT);
  encenderTDS(true);

  adsPresente = ads.begin(DIR_ADS, &Wire);
  if (adsPresente) {
    ads.setGain(GAIN_ONE);  // +/-4,096 V, 0,125 mV por cuenta
    ads.setDataRate(RATE_ADS1115_128SPS);
  }
  termometros.begin();
  termometros.setResolution(12);
  termometros.setWaitForConversion(false);
  termometros.requestTemperatures();
  tPedidoTemp = tFase = millis();
}

void cerrarCicloTDS() {
  actualizarTemperaturas();
  lec.tdsCrudoB = tdsFormula(lec.vB, lec.tB);
  lec.tdsCrudoA = tdsFormula(lec.vA, lec.tB);  // ambos TDS se compensan con el sensor B
  lec.tdsB = lec.tdsCrudoB * cal.kTdsB;
  lec.tdsA = lec.tdsCrudoA * cal.kTdsA;
  lec.ciclos++;
  agregarHistorial({millis(), NAN, lec.tdsCrudoB, lec.tdsCrudoA});
  Serial.printf("M A=%.2f B=%.2f vB=%.4f tdsB=%.1f vA=%.4f tdsA=%.1f\n",
                lec.tA, lec.tB, lec.vB, lec.tdsB, lec.vA, lec.tdsA);
}

void cerrarBloquePH(float v) {
  actualizarTemperaturas();
  lec.po = v * FACTOR_DIVISOR;
  lec.phValido = lec.po > PO_MIN_VALIDO && lec.po < PO_MAX_VALIDO;
  lec.ph = lec.phValido ? calcularPH(lec.po, lec.tB) : NAN;
  agregarHistorial({millis(), lec.phValido ? lec.po : NAN, NAN, NAN});
  static uint32_t tImpresion = 0;
  if (millis() - tImpresion >= 1000) {
    tImpresion = millis();
    Serial.printf("P B=%.2f Po=%.4f pH=%.3f\n", lec.tB, lec.po, lec.ph);
  }
}

void actualizarMediciones() {
  if (!adsPresente) return;
  float v;
  switch (fase) {
    case Fase::ESPERA_TDS:
      if (millis() - tFase >= esperaFase) fase = Fase::MUESTREO_TDS_B;
      break;
    case Fase::MUESTREO_TDS_B:
      if (muestrear(CANAL_TDS_B, lec.vB)) fase = Fase::MUESTREO_TDS_A;
      break;
    case Fase::MUESTREO_TDS_A:
      if (muestrear(CANAL_TDS_A, lec.vA)) {
        cerrarCicloTDS();
        tFase = millis(); esperaFase = T_PAUSA_TDS_MS; fase = Fase::ESPERA_TDS;
      }
      break;
    case Fase::ESPERA_PH:
      if (millis() - tFase >= esperaFase) fase = Fase::MUESTREO_PH;
      break;
    case Fase::MUESTREO_PH:
      if (muestrear(CANAL_PH, v)) cerrarBloquePH(v);
      break;
  }
}

/*
 * Prueba 05 - Revisión general del montaje
 * Monitor de acuarios ESP32
 *
 * Al arrancar verifica cada componente e imprime OK o FALLA:
 *   - Bus I2C: LCD (0x27), ADS1115 (0x48), RTC (0x68)
 *   - DS18B20 A y B por su dirección
 *   - RTC: si conserva la hora
 *   - Módulos TDS 1 y 2: que respondan al encenderlos y apagarlos
 *     (TDS 1: GPIO 32 / A1;  TDS 2: GPIO 33 / A2)
 *
 * Comando por serie para ajustar el RTC:  T2026-10-29 15:04:05
 *
 * Luego mide en ciclo y alterna dos pantallas en el LCD:
 *   1. Temperaturas de A y B
 *   2. pH y TDS (el TDS se apaga 500 ms antes de leer el pH)
 * El encoder se lee por interrupciones: cada paso y cada pulsación
 * se informan por el puerto serie y en el LCD.
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>

// ---- Pines ----
const int PIN_SDA = 21, PIN_SCL = 22;
const int PIN_ONEWIRE = 4;
const int PIN_ENC_A = 25, PIN_ENC_B = 26, PIN_ENC_SW = 27;
const int PIN_TDS_VCC = 32;   // TDS 1, señal en ADS1115 A1
const int PIN_TDS2_VCC = 33;  // TDS 2, señal en ADS1115 A2

// ---- Sensores de temperatura ----
DeviceAddress SENSOR_A = {0x28, 0xA6, 0xF2, 0x9E, 0xC0, 0x24, 0x06, 0x55};
DeviceAddress SENSOR_B = {0x28, 0x9B, 0x72, 0xB5, 0xC0, 0x24, 0x07, 0x83};

// ---- Calibración de pH del 27-oct ----
const float FACTOR_DIVISOR = 1.5;
const float PO_PH7 = 2.5283, PO_PH4 = 3.0549, PO_PH10 = 2.0076;
const int N_MUESTRAS = 32;  // 250 ms a 128 SPS = 15 ciclos de 60 Hz

Adafruit_ADS1115 ads;
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire bus(PIN_ONEWIRE);
DallasTemperature termometros(&bus);
RTC_DS3231 rtc;

bool okLcd, okAds, okRtc, okA, okB;

// ---- Encoder por interrupciones (tabla de transiciones de cuadratura) ----
volatile int32_t pasosEncoder = 0;
volatile uint8_t estadoEnc = 0;
void IRAM_ATTR isrEncoder() {
  // Cada fila es el estado anterior (AB) y cada columna el nuevo; +1 o -1
  // según el sentido, 0 para transiciones inválidas (rebotes).
  static const int8_t tabla[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  estadoEnc = ((estadoEnc << 2) | ab) & 0x0F;
  pasosEncoder += tabla[estadoEnc];
}

bool hayDispositivo(uint8_t dir) {
  Wire.beginTransmission(dir);
  return Wire.endTransmission() == 0;
}

float promedioCanal(uint8_t canal) {
  float suma = 0;
  for (int i = 0; i < N_MUESTRAS; i++) suma += ads.computeVolts(ads.readADC_SingleEnded(canal));
  return suma / N_MUESTRAS;
}

float calcularPH(float po) {
  if (po >= PO_PH7) return 7.0 - 3.0 * (po - PO_PH7) / (PO_PH4 - PO_PH7);
  return 7.0 + 3.0 * (PO_PH7 - po) / (PO_PH7 - PO_PH10);
}

float calcularTDS(float v, float t) {
  float vc = v / (1.0 + 0.02 * (t - 25.0));
  return (133.42 * vc * vc * vc - 255.86 * vc * vc + 857.39 * vc) * 0.5;
}

void informe(const char* nombre, bool ok, const char* detalle = "") {
  Serial.printf("  %-26s %s  %s\n", nombre, ok ? "OK   " : "FALLA", detalle);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 05: revision general ===");

  pinMode(PIN_TDS_VCC, OUTPUT);
  digitalWrite(PIN_TDS_VCC, HIGH);
  pinMode(PIN_TDS2_VCC, OUTPUT);
  digitalWrite(PIN_TDS2_VCC, HIGH);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  okLcd = hayDispositivo(0x27);
  okAds = hayDispositivo(0x48) && ads.begin(0x48, &Wire);
  okRtc = hayDispositivo(0x68) && rtc.begin(&Wire);

  if (okLcd) { lcd.init(); lcd.backlight(); lcd.print("Revision general"); }
  if (okAds) { ads.setGain(GAIN_ONE); ads.setDataRate(RATE_ADS1115_128SPS); }

  termometros.begin();
  okA = termometros.isConnected(SENSOR_A);
  okB = termometros.isConnected(SENSOR_B);
  termometros.setResolution(12);

  Serial.println("--- Componentes ---");
  informe("LCD (0x27)", okLcd);
  informe("ADS1115 (0x48)", okAds);
  char det[48] = "";
  if (okRtc) {
    DateTime t = rtc.now();
    snprintf(det, sizeof(det), "%04d-%02d-%02d %02d:%02d:%02d %s", t.year(), t.month(), t.day(),
             t.hour(), t.minute(), t.second(), rtc.lostPower() ? "(PERDIO LA HORA)" : "(hora conservada)");
  }
  informe("RTC DS3231 (0x68)", okRtc, det);
  informe("DS18B20 A", okA);
  informe("DS18B20 B", okB);
  Serial.printf("  Sensores en el bus OneWire: %d\n", termometros.getDeviceCount());

  // El módulo TDS debe cambiar su salida al apagarlo: encendido fija la
  // señal; apagado queda flotando con otro valor.
  if (okAds) {
    float on = promedioCanal(1);
    digitalWrite(PIN_TDS_VCC, LOW);  delay(500);
    float off = promedioCanal(1);
    digitalWrite(PIN_TDS_VCC, HIGH); delay(500);
    snprintf(det, sizeof(det), "encendido %.1f mV, apagado %.1f mV", on * 1000, off * 1000);
    informe("Modulo TDS 1 (GPIO 32, A1)", fabs(on - off) > 0.005, det);

    on = promedioCanal(2);
    digitalWrite(PIN_TDS2_VCC, LOW);  delay(500);
    off = promedioCanal(2);
    digitalWrite(PIN_TDS2_VCC, HIGH); delay(500);
    snprintf(det, sizeof(det), "encendido %.1f mV, apagado %.1f mV", on * 1000, off * 1000);
    informe("Modulo TDS 2 (GPIO 33, A2)", fabs(on - off) > 0.005, det);
    float po = promedioCanal(0) * FACTOR_DIVISOR;
    snprintf(det, sizeof(det), "Po = %.4f V", po);
    // Po válido: entre ~1,2 V (pH 14) y ~3,8 V (pH 0). Una entrada flotante
    // (módulo desconectado) da ~0,87 V y un módulo sin alimentación ~0 V.
    informe("Modulo pH (alimentado)", po > 1.0 && po < 4.0, det);
  }

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isrEncoder, CHANGE);
  Serial.println("--- Encoder: gire y presione para probarlo ---\n");
  delay(1500);
}

void loop() {
  static uint32_t tCiclo = 0;
  static int pantalla = 0;
  static int32_t pasosPrevios = 0;
  static bool swPrevio = HIGH;
  static float tA = NAN, tB = NAN, ph = NAN, tds = NAN, tds2 = NAN;

  // Ajuste de hora: T2026-10-29 15:04:05
  if (Serial.available()) {
    String c = Serial.readStringUntil('\n');
    c.trim();
    int a, me, d, h, mi, se;
    if (okRtc && c.length() && c[0] == 'T' &&
        sscanf(c.c_str() + 1, "%d-%d-%d %d:%d:%d", &a, &me, &d, &h, &mi, &se) == 6) {
      rtc.adjust(DateTime(a, me, d, h, mi, se));
      Serial.printf("HORA AJUSTADA a %04d-%02d-%02d %02d:%02d:%02d\n", a, me, d, h, mi, se);
    }
  }

  // Encoder y pulsador (cada detente del EC11 son 4 transiciones)
  int32_t p = pasosEncoder / 4;
  if (p != pasosPrevios) {
    Serial.printf("ENCODER: %s  (posicion %ld)\n", p > pasosPrevios ? "derecha" : "izquierda", p);
    if (okLcd) { lcd.setCursor(0, 1); lcd.printf("Encoder %-8ld", p); }
    pasosPrevios = p;
  }
  bool sw = digitalRead(PIN_ENC_SW);
  if (sw != swPrevio) {
    delay(20);  // antirrebote simple
    if (digitalRead(PIN_ENC_SW) == sw) {
      Serial.printf("PULSADOR: %s\n", sw == LOW ? "presionado" : "suelto");
      if (okLcd && sw == LOW) { lcd.setCursor(0, 1); lcd.print("Pulsador OK     "); }
      swPrevio = sw;
    }
  }

  if (millis() - tCiclo < 3000) return;
  tCiclo = millis();

  termometros.requestTemperatures();
  if (okA) tA = termometros.getTempC(SENSOR_A);
  if (okB) tB = termometros.getTempC(SENSOR_B);

  if (okAds) {
    digitalWrite(PIN_TDS_VCC, LOW);  delay(500);   // TDS apagado para leer el pH
    ph = calcularPH(promedioCanal(0) * FACTOR_DIVISOR);
    digitalWrite(PIN_TDS_VCC, HIGH); delay(300);   // el TDS se estabiliza en < 1 s
    tds = calcularTDS(promedioCanal(1), isnan(tB) ? 25.0 : tB);
    tds2 = calcularTDS(promedioCanal(2), isnan(tA) ? 25.0 : tA);
  }

  char hora[9] = "--:--:--";
  if (okRtc) { DateTime t = rtc.now(); snprintf(hora, sizeof(hora), "%02d:%02d:%02d", t.hour(), t.minute(), t.second()); }
  Serial.printf("%s  A=%.2f C  B=%.2f C  pH=%.2f  TDS1=%.0f ppm  TDS2=%.0f ppm\n",
                hora, tA, tB, ph, tds, tds2);

  if (okLcd) {
    char l1[17], l2[17];
    if (pantalla == 0) {
      snprintf(l1, sizeof(l1), "A %5.1f%c B %5.1f%c", tA, (char)223, tB, (char)223);
      snprintf(l2, sizeof(l2), "%s        ", hora);
    } else {
      snprintf(l1, sizeof(l1), "pH %5.2f        ", ph);
      snprintf(l2, sizeof(l2), "T1 %4.0f T2 %4.0f ", tds, tds2);
    }
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
    pantalla = 1 - pantalla;
  }
}

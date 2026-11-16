/*
 * Prueba 01c - Convertidor ADS1115
 * Monitor de acuarios ESP32
 *
 * 1. Verifica que el ADS1115 responde en 0x48.
 * 2. Lee las 4 entradas cada 2 s.
 * 3. Cada 5 lecturas hace un barrido con el DAC del ESP32 (GPIO 25)
 *    para comparar voltajes conocidos contra lo que mide el ADS1115 en A2.
 *
 * Puentes de prueba sugeridos (solo para esta prueba):
 *   A0 -> GND     (debe leer ~0,000 V)
 *   A1 -> 3V3     (debe leer ~3,3 V)
 *   A2 -> GPIO 25 (salida del DAC, para el barrido)
 *   A3 sin conectar (lectura flotante, valor aleatorio)
 *
 * Librería: Adafruit ADS1X15
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;
const int PIN_DAC = 25;  // GPIO 25 es el canal 1 del DAC de 8 bits del ESP32

Adafruit_ADS1115 ads;
bool adsOk = false;

float leerVoltios(uint8_t canal) {
  int16_t cuentas = ads.readADC_SingleEnded(canal);
  return ads.computeVolts(cuentas);
}

void barridoDac() {
  Serial.println("--- Barrido DAC (GPIO 25) -> ADS1115 A2 ---");
  Serial.println(" DAC   esperado   medido A2");
  const uint8_t pasos[] = {0, 32, 64, 96, 128, 160, 192, 224, 255};
  for (uint8_t v : pasos) {
    dacWrite(PIN_DAC, v);
    delay(50);  // tiempo para que la salida del DAC se estabilice
    float esperado = v * 3.3 / 255.0;
    float medido = leerVoltios(2);
    Serial.printf(" %3u    %5.3f V    %6.4f V\n", v, esperado, medido);
  }
  dacWrite(PIN_DAC, 0);
  Serial.println("(El DAC del ESP32 es aproximado: importa que A2 suba de forma ordenada)\n");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 01c: ADS1115 ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  adsOk = ads.begin(0x48, &Wire);
  if (!adsOk) {
    Serial.println("ADS1115 NO responde en 0x48. Revise VDD, GND, SDA, SCL y ADDR a GND.");
    return;
  }

  // GAIN_ONE: rango de +/-4,096 V, resolución de 0,125 mV por cuenta.
  // Cubre todo el rango útil (0 a 3,3 V) sin saturar.
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.println("ADS1115 detectado. Ganancia +/-4,096 V, 128 muestras/s\n");
}

void loop() {
  if (!adsOk) { delay(1000); return; }

  static int n = 0;
  Serial.printf("A0=%7.4f V   A1=%7.4f V   A2=%7.4f V   A3=%7.4f V\n",
                leerVoltios(0), leerVoltios(1), leerVoltios(2), leerVoltios(3));

  if (++n % 5 == 0) barridoDac();
  delay(2000);
}

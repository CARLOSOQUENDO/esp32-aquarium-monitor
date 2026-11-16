/*
 * Prueba 03 - Módulo de pH a través del divisor y el ADS1115
 * Monitor de acuarios ESP32
 *
 * Paso A (sin sonda, BNC en corto): ajustar el potenciómetro de offset
 *   hasta que Po marque 2,50 V. Un corto en el BNC equivale a 0 mV del
 *   electrodo, que es lo que entrega una sonda ideal en pH 7.
 * Paso B (con sonda en solución): observar el voltaje y su estabilidad.
 *
 * El LCD muestra los valores en vivo para poder girar el potenciómetro
 * mirando la pantalla. El puerto serie imprime el mismo dato con más detalle.
 *
 * Conexiones:
 *   Módulo pH: V+ -> VIN (5 V), G -> GND, Po -> divisor 10k/20k -> ADS1115 A0
 *   ADS1115 y LCD en I2C (GPIO 21/22)
 *   DS18B20 B en GPIO 4 (temperatura del buffer)
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;

// Divisor: Po -- R1 (10k) -- A0 -- R2 (20k) -- GND  =>  Po = A0 * (R1 + R2) / R2
const float R1 = 10000.0;
const float R2 = 20000.0;
const float FACTOR_DIVISOR = (R1 + R2) / R2;  // 1,5

// Valores nominales SOLO para orientarse; la calibración con buffers los reemplaza.
const float PO_PH7_NOMINAL = 2.50;    // V
const float PENDIENTE_NOMINAL = 0.177;  // V por unidad de pH (59,16 mV x ganancia ~3)

const int N_MUESTRAS = 25;

// Sensor del acuario B (identificado en la prueba 02)
const int PIN_ONEWIRE = 4;
DeviceAddress SENSOR_B = {0x28, 0x9B, 0x72, 0xB5, 0xC0, 0x24, 0x07, 0x83};
OneWire bus(PIN_ONEWIRE);
DallasTemperature termometros(&bus);
float tempB = NAN;

Adafruit_ADS1115 ads;
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool adsOk = false;

// Toma N muestras y devuelve la media; también calcula la desviación estándar
// como medida del ruido de la señal.
float medirA0(float &ruido) {
  float suma = 0, suma2 = 0;
  for (int i = 0; i < N_MUESTRAS; i++) {
    float v = ads.computeVolts(ads.readADC_SingleEnded(0));
    suma += v;
    suma2 += v * v;
  }
  float media = suma / N_MUESTRAS;
  float var = suma2 / N_MUESTRAS - media * media;
  ruido = var > 0 ? sqrt(var) : 0;
  return media;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 03: modulo de pH ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();

  adsOk = ads.begin(0x48, &Wire);
  if (!adsOk) {
    Serial.println("ADS1115 NO responde en 0x48.");
    lcd.print("ADS1115 no resp.");
    return;
  }
  termometros.begin();
  termometros.setResolution(SENSOR_B, 12);
  // Sin espera: se pide la conversión y se recoge en el ciclo siguiente,
  // para no bloquear 750 ms la lectura del pH.
  termometros.setWaitForConversion(false);
  termometros.requestTemperatures();

  ads.setGain(GAIN_ONE);                 // +/-4,096 V, 0,125 mV por cuenta
  ads.setDataRate(RATE_ADS1115_128SPS);
  Serial.printf("Factor del divisor: %.3f\n\n", FACTOR_DIVISOR);
}

void loop() {
  if (!adsOk) { delay(1000); return; }

  float ruidoA0;
  float vA0 = medirA0(ruidoA0);
  float vPo = vA0 * FACTOR_DIVISOR;
  float ruidoPo = ruidoA0 * FACTOR_DIVISOR;
  float phAprox = 7.0 + (PO_PH7_NOMINAL - vPo) / PENDIENTE_NOMINAL;

  static uint32_t tPedido = 0;
  if (millis() - tPedido >= 800) {
    float t = termometros.getTempC(SENSOR_B);
    tempB = (t == DEVICE_DISCONNECTED_C) ? NAN : t;
    termometros.requestTemperatures();
    tPedido = millis();
  }

  Serial.printf("t=%lu s  A0=%.4f V  Po=%.4f V  ruido=%.2f mV  pH aprox=%.2f  T=%.2f C\n",
                millis() / 1000, vA0, vPo, ruidoPo * 1000, phAprox, tempB);

  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "Po %6.4f V     ", vPo);
  snprintf(l2, sizeof(l2), "pH~%5.2f r%4.1fmV", phAprox, ruidoPo * 1000);
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);

  delay(300);
}

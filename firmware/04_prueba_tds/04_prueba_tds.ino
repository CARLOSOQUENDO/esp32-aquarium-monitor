/*
 * Prueba 04 - Sensor de TDS (y su interferencia con el pH)
 * Monitor de acuarios ESP32
 *
 * Mide cada segundo:
 *   - TDS: ADS1115 A1, módulo alimentado desde GPIO 32
 *   - pH:  ADS1115 A0 (divisor x1,5), con la calibración por tramos del 27-oct
 *   - Temperatura: DS18B20 B (compensación del TDS)
 *
 * Comandos por el puerto serie:
 *   P1  enciende el módulo TDS (GPIO 32 en alto)
 *   P0  lo apaga
 *
 * Conexiones del TDS: + -> GPIO 32, - -> GND, A -> ADS1115 A1
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int PIN_TDS_VCC = 32;
const int PIN_ONEWIRE = 4;
DeviceAddress SENSOR_B = {0x28, 0x9B, 0x72, 0xB5, 0xC0, 0x24, 0x07, 0x83};

// Calibración de pH medida el 27-oct (Po en voltios, después de deshacer el divisor)
const float FACTOR_DIVISOR = 1.5;
const float PO_PH7 = 2.5283, PO_PH4 = 3.0549, PO_PH10 = 2.0076;

// 32 muestras a 128 SPS = 250 ms = 15 ciclos completos de 60 Hz
const int N_MUESTRAS = 32;

Adafruit_ADS1115 ads;
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire bus(PIN_ONEWIRE);
DallasTemperature termometros(&bus);

bool tdsEncendido = true;
float tempB = 25.0;

float promedioCanal(uint8_t canal) {
  float suma = 0;
  for (int i = 0; i < N_MUESTRAS; i++) suma += ads.computeVolts(ads.readADC_SingleEnded(canal));
  return suma / N_MUESTRAS;
}

// Dos rectas que se unen en pH 7: una para la zona ácida y otra para la alcalina
float calcularPH(float po) {
  if (po >= PO_PH7) return 7.0 - 3.0 * (po - PO_PH7) / (PO_PH4 - PO_PH7);
  return 7.0 + 3.0 * (PO_PH7 - po) / (PO_PH7 - PO_PH10);
}

// Fórmula del fabricante (DFRobot SEN0244): polinomio de tercer grado que
// convierte el voltaje compensado a TDS. La compensación de temperatura
// usa un coeficiente de 2 % por °C, típico de soluciones acuosas.
float calcularTDS(float v, float t) {
  float coef = 1.0 + 0.02 * (t - 25.0);
  float vc = v / coef;
  return (133.42 * vc * vc * vc - 255.86 * vc * vc + 857.39 * vc) * 0.5;
}

void leerComandos() {
  if (!Serial.available()) return;
  String c = Serial.readStringUntil('\n');
  c.trim();
  if (c == "P1" || c == "P0") {
    tdsEncendido = (c == "P1");
    digitalWrite(PIN_TDS_VCC, tdsEncendido ? HIGH : LOW);
    Serial.printf(">> TDS %s\n", tdsEncendido ? "ENCENDIDO" : "APAGADO");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(500);
  Serial.println("\n=== Prueba 04: TDS e interferencia con pH ===");

  pinMode(PIN_TDS_VCC, OUTPUT);
  digitalWrite(PIN_TDS_VCC, HIGH);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  if (!ads.begin(0x48, &Wire)) {
    Serial.println("ADS1115 NO responde.");
    lcd.print("ADS1115 no resp.");
    while (true) delay(1000);
  }
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);

  termometros.begin();
  termometros.setResolution(SENSOR_B, 12);
  termometros.setWaitForConversion(false);
  termometros.requestTemperatures();
}

void loop() {
  leerComandos();

  static uint32_t tTemp = 0;
  if (millis() - tTemp >= 800) {
    float t = termometros.getTempC(SENSOR_B);
    if (t != DEVICE_DISCONNECTED_C) tempB = t;
    termometros.requestTemperatures();
    tTemp = millis();
  }

  float vTds = promedioCanal(1);
  float po = promedioCanal(0) * FACTOR_DIVISOR;
  float ph = calcularPH(po);
  float tds = tdsEncendido ? calcularTDS(vTds, tempB) : 0;

  Serial.printf("t=%lu s  TDS=%s  Vtds=%.4f V  tds=%6.1f ppm  EC=%6.1f uS/cm  "
                "Po=%.4f V  pH=%.3f  T=%.2f C\n",
                millis() / 1000, tdsEncendido ? "ON " : "OFF", vTds, tds, tds * 2,
                po, ph, tempB);

  char l1[17], l2[17];
  if (tdsEncendido) snprintf(l1, sizeof(l1), "TDS%5.0fppm %4.2fV", tds, vTds);
  else              snprintf(l1, sizeof(l1), "TDS apagado     ");
  snprintf(l2, sizeof(l2), "pH%5.2f  T%5.2f%c", ph, tempB, (char)223);
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);

  delay(200);
}

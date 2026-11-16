/*
 * Prueba 03b - Solo pH, con los TDS apagados
 * Monitor de acuarios ESP32
 *
 * Lee las 4 entradas del ADS1115 para localizar la señal del divisor.
 * Con el divisor bien conectado, A0 marca Po x 2/3 (~1,7 V en buffer 7).
 */

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>

const int PIN_TDS_B_VCC = 32, PIN_TDS_A_VCC = 33;
const int N_MUESTRAS = 32;  // 250 ms a 128 SPS = 15 ciclos de 60 Hz

Adafruit_ADS1115 ads;
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool adsOk = false;

float promedio(uint8_t canal) {
  float suma = 0;
  for (int i = 0; i < N_MUESTRAS; i++) suma += ads.computeVolts(ads.readADC_SingleEnded(canal));
  return suma / N_MUESTRAS;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 03b: solo pH ===");

  pinMode(PIN_TDS_B_VCC, OUTPUT); digitalWrite(PIN_TDS_B_VCC, LOW);
  pinMode(PIN_TDS_A_VCC, OUTPUT); digitalWrite(PIN_TDS_A_VCC, LOW);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  adsOk = ads.begin(0x48, &Wire);
  if (!adsOk) { Serial.println("ADS1115 NO responde"); lcd.print("ADS1115 no resp."); return; }
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
}

void loop() {
  if (!adsOk) { delay(1000); return; }
  float a0 = promedio(0), a1 = promedio(1), a2 = promedio(2), a3 = promedio(3);
  Serial.printf("A0=%.4f V  A1=%.4f V  A2=%.4f V  A3=%.4f V  Po(A0x1,5)=%.4f V\n", a0, a1, a2, a3, a0 * 1.5);

  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "A0 %.3f A3 %.3f", a0, a3);
  snprintf(l2, sizeof(l2), "Po %.3f V", a0 * 1.5);
  lcd.setCursor(0, 0); lcd.print(l1); lcd.print("   ");
  lcd.setCursor(0, 1); lcd.print(l2); lcd.print("      ");
  delay(300);
}

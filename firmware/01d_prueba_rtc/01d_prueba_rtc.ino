/*
 * Prueba 01d - Reloj de tiempo real DS3231
 * Monitor de acuarios ESP32
 *
 * 1. Verifica que el DS3231 responde en 0x68.
 * 2. Informa si el reloj perdió la hora (sin batería o batería agotada).
 * 3. Imprime fecha, hora y temperatura interna del DS3231 cada segundo.
 * 4. Permite poner la hora desde el computador enviando por el puerto serie:
 *        T2026-10-17 15:04:05
 *
 * 5. Muestra la fecha, la hora y el estado del reloj en el LCD.
 *
 * Conexiones: VCC-3V3, GND-GND, SDA-GPIO 21, SCL-GPIO 22 (RTC y LCD en el mismo bus)
 * Librerías: RTClib (Adafruit), LiquidCrystal_I2C (Frank de Brabander)
 */

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool rtcOk = false;
bool horaPerdida = false;  // se muestra en el LCD hasta que se ajuste la hora

void ponerHoraDesdeSerial() {
  if (!Serial.available()) return;
  String linea = Serial.readStringUntil('\n');
  linea.trim();
  int a, mes, d, h, mi, s;
  if (linea.length() > 0 && linea[0] == 'T' &&
      sscanf(linea.c_str() + 1, "%d-%d-%d %d:%d:%d", &a, &mes, &d, &h, &mi, &s) == 6) {
    rtc.adjust(DateTime(a, mes, d, h, mi, s));
    horaPerdida = false;
    Serial.printf("HORA AJUSTADA a %04d-%02d-%02d %02d:%02d:%02d\n", a, mes, d, h, mi, s);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(200);
  delay(500);
  Serial.println("\n=== Prueba 01d: RTC DS3231 ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();

  rtcOk = rtc.begin(&Wire);
  if (!rtcOk) {
    Serial.println("DS3231 NO responde en 0x68. Revise VCC, GND, SDA y SCL.");
    lcd.print("RTC no responde");
    return;
  }

  // El DS3231 tiene un indicador interno (OSF) que se activa cuando su
  // oscilador se detuvo: ocurre si se quedó sin VCC y sin batería.
  horaPerdida = rtc.lostPower();
  if (horaPerdida) {
    Serial.println("ESTADO: el reloj perdio la hora (normal sin bateria). Envie T<fecha hora> para ajustarlo.");
  } else {
    Serial.println("ESTADO: el reloj conservo la hora desde el ultimo ajuste.");
  }
}

void loop() {
  if (!rtcOk) { delay(1000); return; }
  ponerHoraDesdeSerial();

  static uint32_t ultimo = 0;
  if (millis() - ultimo >= 1000) {
    ultimo = millis();
    DateTime t = rtc.now();
    Serial.printf("RTC %04d-%02d-%02d %02d:%02d:%02d   temp. interna %.2f C\n",
                  t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second(),
                  rtc.getTemperature());

    // Línea 1: fecha y temperatura interna. Línea 2: hora y estado.
    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "%02d/%02d/%04d %4.1f%c", t.day(), t.month(), t.year(),
             rtc.getTemperature(), (char)223);
    snprintf(l2, sizeof(l2), "%02d:%02d:%02d %s", t.hour(), t.minute(), t.second(),
             horaPerdida ? "AJUSTAR" : "OK     ");
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
  }
}

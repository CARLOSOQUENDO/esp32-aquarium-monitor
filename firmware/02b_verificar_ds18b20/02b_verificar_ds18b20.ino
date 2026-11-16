/*
 * Prueba 02b - Verificar la identidad de los DS18B20 A y B
 * Monitor de acuarios ESP32
 *
 * Lee cada sensor por su DIRECCIÓN fija (la registrada en la prueba 02),
 * no por su orden en el bus. Muestra ambas temperaturas en el LCD y marca
 * con una flecha la que está subiendo, para identificar la sonda que se
 * sostiene en la mano.
 *
 * Si en el bus aparece un sensor con otra dirección, lo informa.
 */

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

const int PIN_ONEWIRE = 4;
DeviceAddress SENSOR_A = {0x28, 0xA6, 0xF2, 0x9E, 0xC0, 0x24, 0x06, 0x55};
DeviceAddress SENSOR_B = {0x28, 0x9B, 0x72, 0xB5, 0xC0, 0x24, 0x07, 0x83};

OneWire bus(PIN_ONEWIRE);
DallasTemperature sensores(&bus);
LiquidCrystal_I2C lcd(0x27, 16, 2);

float refA = NAN, refB = NAN;  // temperaturas de referencia para detectar subidas

bool mismaDireccion(const DeviceAddress x, const DeviceAddress y) {
  return memcmp(x, y, 8) == 0;
}

void revisarBus() {
  int n = sensores.getDeviceCount();
  Serial.printf("Sensores en el bus: %d\n", n);
  for (int i = 0; i < n; i++) {
    DeviceAddress d;
    if (!sensores.getAddress(d, i)) continue;
    const char* nombre = mismaDireccion(d, SENSOR_A) ? "A" :
                         mismaDireccion(d, SENSOR_B) ? "B" : "DESCONOCIDO";
    Serial.printf("  %02X %02X %02X %02X %02X %02X %02X %02X -> %s\n",
                  d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], nombre);
  }
  Serial.println();
}

// Texto de una temperatura con indicador: "^" si subió más de 0,3 °C
// respecto de la referencia, espacio si no.
void formatear(char* out, size_t len, char letra, float t, float ref) {
  if (t == DEVICE_DISCONNECTED_C) { snprintf(out, len, "%c: --.--  ", letra); return; }
  char marca = (!isnan(ref) && t - ref > 0.3) ? '^' : ' ';
  snprintf(out, len, "%c:%6.2f%c%c", letra, t, (char)223, marca);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 02b: verificar DS18B20 A y B ===");

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  sensores.begin();
  sensores.setResolution(12);
  revisarBus();
}

void loop() {
  sensores.requestTemperatures();
  float tA = sensores.getTempC(SENSOR_A);
  float tB = sensores.getTempC(SENSOR_B);

  // La referencia se toma en la primera lectura válida y luego sigue
  // lentamente a la temperatura (se enfría hacia el ambiente sin marcar).
  if (isnan(refA) && tA != DEVICE_DISCONNECTED_C) refA = tA;
  if (isnan(refB) && tB != DEVICE_DISCONNECTED_C) refB = tB;
  if (!isnan(refA) && tA < refA) refA = tA;
  if (!isnan(refB) && tB < refB) refB = tB;

  char l1[17], l2[17];
  formatear(l1, sizeof(l1), 'A', tA, refA);
  formatear(l2, sizeof(l2), 'B', tB, refB);
  lcd.setCursor(0, 0); lcd.print(l1); lcd.print("      ");
  lcd.setCursor(0, 1); lcd.print(l2); lcd.print("      ");

  Serial.printf("A=%6.2f C (ref %.2f)   B=%6.2f C (ref %.2f)\n", tA, refA, tB, refB);
  delay(1000);
}

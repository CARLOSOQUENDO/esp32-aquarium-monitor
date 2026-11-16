/*
 * Prueba 02 - Dos sensores DS18B20 en un solo bus OneWire
 * Monitor de acuarios ESP32
 *
 * Objetivo:
 *   1. Detectar los dos sensores y mostrar su dirección única (ROM de 64 bits).
 *   2. Leer ambas temperaturas cada 2 s.
 *
 * Las direcciones impresas se copiarán al firmware final para saber
 * siempre cuál sensor es el del acuario A y cuál el del acuario B.
 *
 * Conexiones:
 *   VCC  (rojo)            -> 3,3 V
 *   GND  (negro)           -> GND
 *   DATA (amarillo/blanco) -> GPIO 4
 *   Resistencia de 4,7 kΩ entre DATA y 3,3 V (una sola para todo el bus)
 *
 * Librerías (Gestor de librerías del Arduino IDE):
 *   - OneWire            (Paul Stoffregen)
 *   - DallasTemperature  (Miles Burton)
 */

#include <OneWire.h>
#include <DallasTemperature.h>

const int PIN_ONEWIRE = 4;

OneWire bus(PIN_ONEWIRE);
DallasTemperature sensores(&bus);

DeviceAddress direcciones[2];
int cantidad = 0;

void imprimirDireccion(const DeviceAddress d) {
  for (int i = 0; i < 8; i++) {
    Serial.printf("0x%02X", d[i]);
    if (i < 7) Serial.print(", ");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 02: DS18B20 ===");

  sensores.begin();
  cantidad = sensores.getDeviceCount();
  Serial.printf("Sensores detectados: %d\n", cantidad);

  if (cantidad > 2) cantidad = 2;

  for (int i = 0; i < cantidad; i++) {
    if (sensores.getAddress(direcciones[i], i)) {
      Serial.printf("Sensor %d -> { ", i);
      imprimirDireccion(direcciones[i]);
      Serial.println(" }");
      // 12 bits: resolución de 0,0625 °C; la conversión tarda hasta 750 ms
      sensores.setResolution(direcciones[i], 12);
    } else {
      Serial.printf("Sensor %d: no se pudo leer la direccion\n", i);
    }
  }

  if (cantidad == 0) {
    Serial.println("Revise: resistencia de 4,7 kOhm, GPIO 4 y colores de los cables.");
  }
  Serial.println();
}

void loop() {
  // Una sola orden inicia la conversión en todos los sensores del bus a la vez
  sensores.requestTemperatures();

  for (int i = 0; i < cantidad; i++) {
    float t = sensores.getTempC(direcciones[i]);
    if (t == DEVICE_DISCONNECTED_C) {
      Serial.printf("Sensor %d: DESCONECTADO   ", i);
    } else if (t == 85.0) {
      // 85 °C es el valor de encendido del DS18B20: suele indicar
      // alimentación insuficiente o que la conversión no terminó
      Serial.printf("Sensor %d: 85.00 C (lectura invalida)   ", i);
    } else {
      Serial.printf("Sensor %d: %.2f C   ", i, t);
    }
  }
  Serial.println();
  delay(2000);
}

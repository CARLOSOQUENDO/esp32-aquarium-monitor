/*
 * Prueba 01 - Escáner del bus I2C
 * Monitor de acuarios ESP32
 *
 * Objetivo: confirmar que el ESP32 detecta los tres módulos I2C
 * (LCD, ADS1115 y RTC DS3231) antes de escribir cualquier otra lógica.
 *
 * Conexiones (todos los módulos alimentados a 3,3 V):
 *   SDA -> GPIO 21
 *   SCL -> GPIO 22
 *
 * Direcciones esperadas:
 *   0x27 o 0x3F  LCD 16x2 (adaptador PCF8574 / PCF8574A)
 *   0x48         ADS1115 (pin ADDR a GND)
 *   0x57         EEPROM AT24C32 del módulo RTC (ZS-042)
 *   0x68         RTC DS3231
 */

#include <Wire.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;

// Devuelve el nombre probable del dispositivo según su dirección
const char* nombreProbable(uint8_t dir) {
  switch (dir) {
    case 0x27: return "LCD (PCF8574)";
    case 0x3F: return "LCD (PCF8574A)";
    case 0x48: return "ADS1115";
    case 0x57: return "EEPROM del modulo RTC";
    case 0x68: return "RTC DS3231";
    default:   return "desconocido";
  }
}

void escanear() {
  int encontrados = 0;
  Serial.println("Escaneando direcciones 0x01 a 0x7F...");

  for (uint8_t dir = 1; dir < 127; dir++) {
    // Se inicia una transmisión vacía: si un dispositivo responde
    // con ACK en esa dirección, endTransmission() devuelve 0.
    Wire.beginTransmission(dir);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  Encontrado en 0x%02X -> %s\n", dir, nombreProbable(dir));
      encontrados++;
    } else if (error == 4) {
      Serial.printf("  Error desconocido en 0x%02X\n", dir);
    }
  }

  if (encontrados == 0) {
    Serial.println("  Ningun dispositivo. Revise SDA/SCL, GND comun y alimentacion.");
  } else {
    Serial.printf("Total: %d dispositivo(s)\n", encontrados);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 01: escaner I2C ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);  // 100 kHz: modo estándar, el más tolerante con cables largos
}

void loop() {
  escanear();
  delay(5000);  // repite cada 5 s para poder conectar/desconectar módulos y ver el cambio
}

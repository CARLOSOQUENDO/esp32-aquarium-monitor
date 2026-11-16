/*
 * Prueba 00 - Diagnóstico del ESP32 (sin periféricos)
 * Monitor de acuarios ESP32
 *
 * Objetivo:
 *   1. Mostrar la información del chip (modelo, núcleos, memoria).
 *   2. Verificar que cada pin del proyecto responde: se activa la
 *      resistencia interna de pull-up (debe leerse 1) y luego la de
 *      pull-down (debe leerse 0). Si un pin no cambia, está en corto,
 *      dañado o tiene algo conectado.
 *   3. Hacer parpadear el LED de la placa (GPIO 2) como señal de vida.
 *
 * IMPORTANTE: ejecutar con la placa sola, sin nada conectado a los pines.
 */

const int PIN_LED = 2;

struct PinProyecto {
  int gpio;
  const char* funcion;
};

// Pines que usará el monitor (GPIO 34 y 35 no se prueban: son solo
// de entrada y no tienen resistencias internas)
const PinProyecto PINES[] = {
  {4,  "Bus OneWire (DS18B20)"},
  {21, "I2C SDA"},
  {22, "I2C SCL"},
  {25, "Encoder CLK"},
  {26, "Encoder DT"},
  {27, "Encoder pulsador"},
  {32, "Alimentacion del TDS"},
};
const int N_PINES = sizeof(PINES) / sizeof(PINES[0]);

void infoChip() {
  Serial.println("--- Informacion del chip ---");
  Serial.printf("Modelo:        %s (revision %d)\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("Nucleos:       %d a %d MHz\n", ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("Flash:         %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("RAM libre:     %u bytes\n", ESP.getFreeHeap());
  Serial.printf("SDK:           %s\n", ESP.getSdkVersion());
  uint64_t mac = ESP.getEfuseMac();
  Serial.printf("MAC (eFuse):   %02X:%02X:%02X:%02X:%02X:%02X\n",
                (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
                (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  Serial.println();
}

bool probarPin(const PinProyecto& p) {
  pinMode(p.gpio, INPUT_PULLUP);
  delay(5);  // tiempo para que la línea se estabilice
  int conPullUp = digitalRead(p.gpio);

  pinMode(p.gpio, INPUT_PULLDOWN);
  delay(5);
  int conPullDown = digitalRead(p.gpio);

  pinMode(p.gpio, INPUT);  // se deja el pin en alta impedancia

  bool ok = (conPullUp == HIGH && conPullDown == LOW);
  Serial.printf("GPIO %2d  %-24s pull-up=%d  pull-down=%d  -> %s\n",
                p.gpio, p.funcion, conPullUp, conPullDown, ok ? "OK" : "REVISAR");
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 00: diagnostico del ESP32 ===\n");

  infoChip();

  Serial.println("--- Prueba de pines del proyecto ---");
  int fallas = 0;
  for (int i = 0; i < N_PINES; i++) {
    if (!probarPin(PINES[i])) fallas++;
  }
  Serial.printf("\nResultado: %d de %d pines OK\n\n", N_PINES - fallas, N_PINES);

  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  static uint32_t ultimo = 0;
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));

  if (millis() - ultimo >= 5000) {
    ultimo = millis();
    Serial.printf("Funcionando: %lu s, RAM libre %u bytes\n", millis() / 1000, ESP.getFreeHeap());
  }
  delay(500);
}

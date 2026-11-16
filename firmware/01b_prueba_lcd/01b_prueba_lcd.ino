/*
 * Prueba 01b - Display LCD 16x2 por I2C
 * Monitor de acuarios ESP32
 *
 * Recorre 5 pantallas, 4 s cada una, y repite:
 *   1. Bienvenida
 *   2. Todos los caracteres llenos (revisa contraste y píxeles muertos)
 *   3. Caracteres personalizados (gota, termómetro, check)
 *   4. Maqueta de la pantalla real con valores simulados
 *   5. Parpadeo de la luz de fondo
 *
 * Conexiones: GND-GND, VCC-3V3, SDA-GPIO 21, SCL-GPIO 22
 * Librería: LiquidCrystal_I2C (Frank de Brabander)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;
const uint8_t DIR_LCD = 0x27;

LiquidCrystal_I2C lcd(DIR_LCD, 16, 2);

// El HD44780 permite definir 8 caracteres propios de 5x8 píxeles.
// Cada byte es una fila; los 5 bits menos significativos son los píxeles.
uint8_t gota[8]   = {0b00100, 0b00100, 0b01010, 0b01010, 0b10001, 0b10001, 0b01110, 0b00000};
uint8_t termo[8]  = {0b00100, 0b01010, 0b01010, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110};
uint8_t check[8]  = {0b00000, 0b00001, 0b00011, 0b10110, 0b11100, 0b01000, 0b00000, 0b00000};
uint8_t bloque[8] = {0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};

const char GRADO = (char)223;  // símbolo de grado en la ROM del HD44780

void pantallaBienvenida() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Monitor acuarios");
  lcd.setCursor(0, 1); lcd.print("Prueba LCD  OK ");
  lcd.write(2);
}

void pantallaLlena() {
  lcd.clear();
  for (int fila = 0; fila < 2; fila++) {
    lcd.setCursor(0, fila);
    for (int col = 0; col < 16; col++) lcd.write(3);
  }
}

void pantallaIconos() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.write(0); lcd.print(" gota  "); lcd.write(1); lcd.print(" termo");
  lcd.setCursor(0, 1); lcd.write(2); lcd.print(" check  "); lcd.print(GRADO); lcd.print(" grado");
}

void pantallaMaqueta(int ciclo) {
  // Valores simulados que cambian un poco en cada ciclo
  float tA  = 24.5 + 0.1 * (ciclo % 5);
  float tB  = 25.1 - 0.1 * (ciclo % 4);
  float ph  = 7.02 + 0.01 * (ciclo % 7);
  int   tds = 180 + (ciclo % 9);

  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "A %4.1f%c B %4.1f%c", tA, GRADO, tB, GRADO);
  snprintf(l2, sizeof(l2), "pH %4.2f  %3dppm", ph, tds);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

void pantallaLuz() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Luz de fondo");
  for (int i = 0; i < 3; i++) {
    lcd.noBacklight(); delay(500);
    lcd.backlight();   delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba 01b: LCD 16x2 ===");

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, gota);
  lcd.createChar(1, termo);
  lcd.createChar(2, check);
  lcd.createChar(3, bloque);
}

void loop() {
  static int ciclo = 0;

  Serial.println("Pantalla 1: bienvenida");       pantallaBienvenida(); delay(4000);
  Serial.println("Pantalla 2: todo lleno");       pantallaLlena();      delay(4000);
  Serial.println("Pantalla 3: iconos");           pantallaIconos();     delay(4000);
  Serial.println("Pantalla 4: maqueta");          pantallaMaqueta(ciclo); delay(4000);
  Serial.println("Pantalla 5: luz de fondo");     pantallaLuz();        delay(1000);

  ciclo++;
}

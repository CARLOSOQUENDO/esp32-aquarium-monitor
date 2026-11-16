// LCD 16x2: se escribe solo lo que cambió (evita parpadeo y tráfico I2C) y,
// si 'espejoSerie' está activo, cada cambio se replica por el puerto serie.
#pragma once
#include <LiquidCrystal_I2C.h>
#include <stdarg.h>
#include "config.h"

LiquidCrystal_I2C lcd(DIR_LCD, 16, 2);
const char GRADO = (char)223;  // símbolo de grado en la ROM del HD44780

char lcdActual[2][17];
char lcdNuevo[2][17];
bool espejoSerie = true;

void iniciarPantalla() {
  lcd.init();
  lcd.backlight();
  memset(lcdActual, 0, sizeof(lcdActual));
  memset(lcdNuevo, ' ', sizeof(lcdNuevo));
  lcdNuevo[0][16] = lcdNuevo[1][16] = '\0';
}

// Escribe una línea con formato printf, recortada o rellenada a 16 caracteres.
void linea(int fila, const char* fmt, ...) {
  char buf[33];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  size_t n = strlen(buf);
  for (size_t i = 0; i < 16; i++) lcdNuevo[fila][i] = i < n ? buf[i] : ' ';
  lcdNuevo[fila][16] = '\0';
}

void refrescarPantalla() {
  bool cambio = false;
  for (int f = 0; f < 2; f++) {
    if (strcmp(lcdActual[f], lcdNuevo[f]) != 0) {
      lcd.setCursor(0, f);
      lcd.print(lcdNuevo[f]);
      strcpy(lcdActual[f], lcdNuevo[f]);
      cambio = true;
    }
  }
  if (cambio && espejoSerie) {
    char a[17], b[17];
    for (int i = 0; i < 17; i++) {
      a[i] = lcdActual[0][i] == GRADO ? 'o' : lcdActual[0][i];
      b[i] = lcdActual[1][i] == GRADO ? 'o' : lcdActual[1][i];
    }
    Serial.printf("LCD|%s|%s|\n", a, b);
  }
}

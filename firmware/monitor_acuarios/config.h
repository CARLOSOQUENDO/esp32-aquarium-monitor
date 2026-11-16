// Configuración del hardware y valores de fábrica del monitor de acuarios.
#pragma once
#include <Arduino.h>

// ---------------- Pines ----------------
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr int PIN_ONEWIRE = 4;
constexpr int PIN_ENC_A = 25;
constexpr int PIN_ENC_B = 26;
constexpr int PIN_ENC_SW = 27;
constexpr int PIN_TDS_B_VCC = 32;  // TDS del acuario B
constexpr int PIN_TDS_A_VCC = 33;  // TDS del acuario A

// ---------------- I2C y canales del ADS1115 ----------------
constexpr uint8_t DIR_LCD = 0x27;
constexpr uint8_t DIR_ADS = 0x48;
constexpr uint8_t CANAL_PH = 0;     // salida Po del módulo de pH, a través del divisor
constexpr uint8_t CANAL_TDS_B = 1;
constexpr uint8_t CANAL_TDS_A = 2;

// ---------------- Sensores de temperatura (prueba 02) ----------------
uint8_t DIR_SENSOR_A[8] = {0x28, 0xA6, 0xF2, 0x9E, 0xC0, 0x24, 0x06, 0x55};
uint8_t DIR_SENSOR_B[8] = {0x28, 0x9B, 0x72, 0xB5, 0xC0, 0x24, 0x07, 0x83};

// ---------------- Medición ----------------
constexpr float FACTOR_DIVISOR = 1.5f;     // Po = A0 * (10k + 20k) / 20k
constexpr int N_MUESTRAS = 32;             // a 128 SPS: 250 ms = 15 ciclos de 60 Hz
constexpr uint32_t T_TDS_APAGADO_MS = 500;   // espera tras apagar los TDS, antes del pH
constexpr uint32_t T_TDS_ARRANQUE_MS = 1000; // espera tras encender los TDS (medido: < 1 s)
constexpr uint32_t T_PAUSA_TDS_MS = 1000;      // pausa entre ciclos de TDS en modo normal
constexpr uint32_t T_CONVERSION_DS18B20_MS = 800;

// ---------------- Medición de pH bajo pedido ----------------
constexpr float TOL_MEDICION_PH_V = 0.001f;        // cambio < 1 mV en 10 s (~0,006 pH)
constexpr uint32_t T_MIN_MEDICION_PH_MS = 15000;
constexpr uint32_t T_MAX_MEDICION_PH_MS = 180000;  // si no se estabiliza, se marca con "?"

constexpr float PO_MIN_VALIDO = 1.0f;  // fuera de 1,0-4,0 V el módulo de pH no está conectado
constexpr float PO_MAX_VALIDO = 4.0f;

// ---------------- Criterios de estabilidad para calibrar ----------------
constexpr uint32_t VENTANA_ESTABILIDAD_MS = 10000;
constexpr float TOL_ESTAB_PO_V = 0.0005f;   // pH: cambio < 0,5 mV en 10 s (~0,003 pH)
constexpr float TOL_ESTAB_TDS_REL = 0.01f;  // TDS: cambio < 1 % en 10 s
constexpr uint32_t T_MIN_CALIBRACION_MS = 20000;  // tiempo mínimo en cada solución

// ---------------- Límites de aceptación de una calibración ----------------
constexpr float GANANCIA_NOMINAL_PH = 3.0f;      // ganancia aproximada del módulo
constexpr float PENDIENTE_MIN_PCT = 85.0f;       // respecto a Nernst x ganancia nominal
constexpr float PENDIENTE_MAX_PCT = 110.0f;
constexpr float PO_NEUTRO_MIN = 2.2f;            // offset aceptable en pH ~7
constexpr float PO_NEUTRO_MAX = 2.8f;
constexpr float K_TDS_MIN = 0.3f, K_TDS_MAX = 3.0f;
constexpr float OFFSET_TEMP_MAX = 3.0f;          // °C

// ---------------- Valores de fábrica (calibración del 27-oct-2026, ~24,6 °C) ----------------
constexpr float FAB_PO_PH4 = 3.0549f;
constexpr float FAB_PO_PH7 = 2.5283f;
constexpr float FAB_PO_PH10 = 2.0076f;
constexpr float FAB_T_CAL_PH = 24.6f;

constexpr uint32_t T_INACTIVIDAD_MENU_MS = 120000;  // vuelve a la pantalla principal

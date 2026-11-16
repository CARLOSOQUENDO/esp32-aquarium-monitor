// Pantallas, menú y procedimientos de calibración guiada.
//
// Pantallas (girar para cambiar): temperatura, TDS, pH, calibración de pH y de TDS. En la
// pantalla de pH, pulsación corta = medir el pH. Pulsación larga = menú.
// TDS 1 = módulo en GPIO 32 / A1 (internamente 'B'); TDS 2 = GPIO 33 / A2 ('A').
// En el menú y los procedimientos: girar = elegir; pulsación corta = aceptar;
// pulsación larga = cancelar y volver.
#pragma once
#include <RTClib.h>
#include "config.h"
#include "almacen.h"
#include "mediciones.h"
#include "pantalla.h"
#include "entradas.h"

RTC_DS3231 rtc;
bool rtcPresente = false;
bool rtcSinHora = false;

uint32_t fechaActual() { return rtcPresente ? rtc.now().unixtime() : 0; }

// Número con ancho fijo, o "--" si no hay lectura válida.
const char* num(float v, int ancho, int dec) {
  static char bufs[6][16];
  static int k = 0;
  char* b = bufs[k]; k = (k + 1) % 6;
  if (isnan(v)) snprintf(b, 16, "%*s", ancho, "--");
  else snprintf(b, 16, "%*.*f", ancho, dec, v);
  return b;
}

// Pendiente de un tramo como porcentaje de Nernst x ganancia nominal del módulo.
float pendientePct(const PuntoPH& a, const PuntoPH& b, float t) {
  float s = (a.po - b.po) / (b.ph - a.ph);  // V por pH, positiva
  return 100.0f * s / (pendienteNernst(t) * GANANCIA_NOMINAL_PH);
}

// ======================= Estado de la interfaz =======================

enum class Modo { PRINCIPAL, MENU, EDITOR, PH_PUNTOS, PH_BUFFER, PH_ESPERA, PH_RESULTADO,
                  TDS_ESPERA, MENSAJE };
Modo modo = Modo::PRINCIPAL;
uint32_t tUltimaAccion = 0;

// 0 = temperatura, 1 = TDS, 2 = pH bajo pedido, 3 = calibración de pH, 4 = calibración de TDS
constexpr int N_PANTALLAS = 5;
constexpr int PANTALLA_PH = 2;
int pantallaPrincipal = 0;

const char* const MENU[] = {"Calibrar pH", "Calibrar TDS 1", "Calibrar TDS 2", "Salir"};
constexpr int N_MENU = sizeof(MENU) / sizeof(MENU[0]);
int itemMenu = 0;

void irPrincipal() { modo = Modo::PRINCIPAL; }
void irMenu() { modo = Modo::MENU; }

// ---------------- Mensajes ----------------
char mensaje[2][17];
void (*trasMensaje)() = irPrincipal;

void mostrarMensaje(const char* l1, const char* l2, void (*despues)()) {
  strncpy(mensaje[0], l1, 16); mensaje[0][16] = '\0';
  strncpy(mensaje[1], l2, 16); mensaje[1][16] = '\0';
  trasMensaje = despues;
  modo = Modo::MENSAJE;
}

// ---------------- Editor de valores numéricos ----------------
struct Editor {
  char titulo[17];
  float valor, vmin, vmax, paso;
  uint8_t decimales;
  char unidad[6];
  void (*info)(char* linea1);   // opcional: primera línea con datos en vivo
  void (*alAceptar)(float);
  void (*alCancelar)();
} ed;

void abrirEditor(const char* titulo, float valor, float vmin, float vmax, float paso,
                 uint8_t dec, const char* unidad, void (*alAceptar)(float),
                 void (*info)(char*) = nullptr, void (*alCancelar)() = irMenu) {
  strncpy(ed.titulo, titulo, 16); ed.titulo[16] = '\0';
  ed.valor = constrain(valor, vmin, vmax);
  ed.vmin = vmin; ed.vmax = vmax; ed.paso = paso; ed.decimales = dec;
  strncpy(ed.unidad, unidad, 5); ed.unidad[5] = '\0';
  ed.info = info; ed.alAceptar = alAceptar; ed.alCancelar = alCancelar;
  modo = Modo::EDITOR;
}

// Aceleración: si se gira rápido, cada clic vale más.
int aceleracion() {
  static uint32_t tPrevio = 0;
  uint32_t dt = millis() - tPrevio;
  tPrevio = millis();
  return dt < 60 ? 10 : (dt < 150 ? 3 : 1);
}

// ======================= Calibración de pH =======================

const float OPCIONES_BUFFER[3][3] = {{7.00f, 6.86f, NAN}, {4.00f, 4.01f, NAN}, {10.00f, 9.18f, 10.01f}};
int nOpcionesBuffer(int paso) { return paso == 2 ? 3 : 2; }

struct {
  uint8_t n, paso;
  int opcion;
  float nominal[3];
  PuntoPH pts[3];
  float sumaT; int nT;
  uint32_t tInicio;
  float tCal, pct[2];
  bool valida;
  const char* motivo;
} cph;

bool listoParaRegistrar(uint32_t tInicio, float Muestra::*campo, float tolAbs, float tolRel) {
  return millis() - tInicio >= T_MIN_CALIBRACION_MS &&
         lecturaEstable(campo, tolAbs, tolRel, tInicio);
}

void evaluarCalPH() {
  // Ordenar los puntos de menor a mayor pH
  for (int i = 0; i < cph.n; i++)
    for (int j = i + 1; j < cph.n; j++)
      if (cph.pts[j].ph < cph.pts[i].ph) { PuntoPH t = cph.pts[i]; cph.pts[i] = cph.pts[j]; cph.pts[j] = t; }

  cph.tCal = cph.nT ? cph.sumaT / cph.nT : 25.0f;
  cph.valida = true;
  cph.motivo = "OK Pulse=guardar";

  int iMedio = 0;
  for (int i = 1; i < cph.n; i++)
    if (fabs(cph.pts[i].ph - 7.0f) < fabs(cph.pts[iMedio].ph - 7.0f)) iMedio = i;
  float poMedio = cph.pts[iMedio].po;

  for (int i = 0; i < cph.n - 1; i++) {
    if (cph.pts[i + 1].ph - cph.pts[i].ph < 1.5f) { cph.valida = false; cph.motivo = "Buffers iguales"; }
    cph.pct[i] = pendientePct(cph.pts[i], cph.pts[i + 1], cph.tCal);
    if (cph.pct[i] < PENDIENTE_MIN_PCT || cph.pct[i] > PENDIENTE_MAX_PCT) {
      cph.valida = false; cph.motivo = "Error pendiente";
    }
  }
  if (poMedio < PO_NEUTRO_MIN || poMedio > PO_NEUTRO_MAX) { cph.valida = false; cph.motivo = "Error offset"; }
}

void guardarCalPH() {
  cal.nPH = cph.n;
  for (int i = 0; i < cph.n; i++) cal.ph[i] = cph.pts[i];
  cal.tCalPH = cph.tCal;
  cal.fechaPH = fechaActual();
  guardarCalibracion();
  mostrarMensaje("pH calibrado", "Pulse: salir", irPrincipal);
}

void registrarPuntoPH() {
  float nominal = cph.nominal[cph.paso];
  cph.pts[cph.paso] = {valorBuffer(nominal, lec.tB), promedioReciente(&Muestra::po, 8)};
  if (!isnan(lec.tB)) { cph.sumaT += lec.tB; cph.nT++; }
  cph.paso++;
  if (cph.paso < cph.n) {
    mostrarMensaje("Enjuague sondas", "Pulse: sig. buf", []() { cph.opcion = 0; modo = Modo::PH_BUFFER; });
  } else {
    evaluarCalPH();
    modo = Modo::PH_RESULTADO;
  }
}

// ======================= Calibración de TDS =======================

struct { bool acuarioB; uint32_t tInicio; } ctds;

float tdsCrudoSel() { return ctds.acuarioB ? lec.tdsCrudoB : lec.tdsCrudoA; }
float kTdsSel() { return ctds.acuarioB ? cal.kTdsB : cal.kTdsA; }
float Muestra::*campoTdsSel() { return ctds.acuarioB ? &Muestra::tdsCrudoB : &Muestra::tdsCrudoA; }

void infoTDS(char* l1) {
  snprintf(l1, 17, "Lee %s uS/cm", num(tdsCrudoSel() * kTdsSel() * 2.0f, 5, 0));
}

void aceptarRefTDS(float refEC) {
  float crudo = promedioReciente(campoTdsSel(), 3);
  float k = (refEC / 2.0f) / crudo;  // TDS = EC x 0,5
  char l1[17];
  if (isnan(k) || crudo < 0.5f || k < K_TDS_MIN || k > K_TDS_MAX) {
    snprintf(l1, sizeof(l1), "Error k=%s", num(k, 0, 2));
    mostrarMensaje(l1, "fuera de rango", irMenu);
    return;
  }
  if (ctds.acuarioB) { cal.kTdsB = k; cal.fechaTdsB = fechaActual(); }
  else { cal.kTdsA = k; cal.fechaTdsA = fechaActual(); }
  guardarCalibracion();
  snprintf(l1, sizeof(l1), "TDS %c k=%.3f", ctds.acuarioB ? '1' : '2', k);
  mostrarMensaje(l1, "Guardado", irPrincipal);
}

// ======================= Medición de pH bajo pedido =======================

enum class EstadoPH { SIN_MEDIR, MIDIENDO, LISTO };
struct {
  EstadoPH estado = EstadoPH::SIN_MEDIR;
  bool hayResultado = false;
  uint32_t tInicio;
  float ph;
  bool estable;    // falso si se cumplió el tiempo máximo sin estabilizarse
  bool conHora;
  uint8_t hora, minuto;
} medPH;

void iniciarMedicionPH() {
  medPH.estado = EstadoPH::MIDIENDO;
  medPH.tInicio = millis();
}

void cancelarMedicionPH() {
  if (medPH.estado == EstadoPH::MIDIENDO)
    medPH.estado = medPH.hayResultado ? EstadoPH::LISTO : EstadoPH::SIN_MEDIR;
}

// Termina la medición cuando el pH se estabiliza o se cumple el tiempo máximo.
void actualizarMedicionPH() {
  if (medPH.estado != EstadoPH::MIDIENDO) return;
  uint32_t dt = millis() - medPH.tInicio;
  bool estable = dt >= T_MIN_MEDICION_PH_MS && lec.phValido &&
                 lecturaEstable(&Muestra::po, TOL_MEDICION_PH_V, 0, medPH.tInicio);
  if (!estable && dt < T_MAX_MEDICION_PH_MS) return;

  float po = promedioReciente(&Muestra::po, 8);
  if (isnan(po)) { cancelarMedicionPH(); return; }  // sin sonda: no hay resultado
  medPH.ph = calcularPH(po, lec.tB);
  medPH.estable = estable;
  medPH.conHora = rtcPresente && !rtcSinHora;
  if (medPH.conHora) { DateTime n = rtc.now(); medPH.hora = n.hour(); medPH.minuto = n.minute(); }
  medPH.hayResultado = true;
  medPH.estado = EstadoPH::LISTO;
  Serial.printf("PH RESULTADO %.3f %s\n", medPH.ph, estable ? "estable" : "NO estable");
}

// El modo pH (TDS apagados) se activa solo mientras se mide o calibra el pH.
bool calPHActiva = false;

void sincronizarModoPH() {
  switch (modo) {
    case Modo::PH_PUNTOS: case Modo::PH_BUFFER: case Modo::PH_ESPERA: case Modo::PH_RESULTADO:
      calPHActiva = true; break;
    case Modo::MENSAJE: break;  // los mensajes intermedios de la calibración la mantienen
    default: calPHActiva = false; break;
  }
  if (modo != Modo::PRINCIPAL || pantallaPrincipal != PANTALLA_PH) cancelarMedicionPH();
  activarModoPH(calPHActiva || medPH.estado == EstadoPH::MIDIENDO);
}

// ======================= Pantallas principales =======================

// Días desde una calibración, o -1 si es de fábrica y -2 si el reloj no tiene hora.
long diasDesde(uint32_t fecha) {
  if (fecha == 0) return -1;
  if (!rtcPresente || rtcSinHora) return -2;
  return (fechaActual() - fecha) / 86400UL;
}

// Versión corta para la pantalla de TDS: "16d", "hoy", "fab" o "?".
void antiguedadCorta(char* out, size_t len, uint32_t fecha) {
  long d = diasDesde(fecha);
  if (d == -1) snprintf(out, len, "fab");
  else if (d == -2) snprintf(out, len, "?");
  else if (d == 0) snprintf(out, len, "hoy");
  else snprintf(out, len, "%ldd", min(d, 999L));
}

void dibujarPrincipal() {
  switch (pantallaPrincipal) {
    case 0:  // temperatura
      linea(0, "Temp A   %s%cC", num(lec.tA, 5, 1), GRADO);
      linea(1, "Temp B   %s%cC", num(lec.tB, 5, 1), GRADO);
      return;
    case 1:  // TDS
      linea(0, "TDS 1   %s ppm", num(lec.tdsB, 4, 0));
      linea(1, "TDS 2   %s ppm", num(lec.tdsA, 4, 0));
      return;
    case 3: {  // calibración de pH
      linea(0, "Calibracion pH");
      long d = diasDesde(cal.fechaPH);
      if (d == -1) linea(1, "De fabrica");
      else if (d == -2) linea(1, "Fecha sin reloj");
      else if (d == 0) linea(1, "Hoy");
      else if (d == 1) linea(1, "Hace 1 dia");
      else linea(1, "Hace %ld dias", d);
      return;
    }
    case 4: {  // calibración de cada TDS
      char t1[8], t2[8];
      antiguedadCorta(t1, sizeof(t1), cal.fechaTdsB);
      antiguedadCorta(t2, sizeof(t2), cal.fechaTdsA);
      linea(0, "Calibracion TDS");
      linea(1, "T1:%4s  T2:%4s", t1, t2);
      return;
    }
  }
  switch (medPH.estado) {  // pantalla de pH
    case EstadoPH::SIN_MEDIR:
      linea(0, "Medicion de pH");
      linea(1, "Pulse para medir");
      break;
    case EstadoPH::MIDIENDO:
      linea(0, "Midiendo pH %3lus", (unsigned long)((millis() - medPH.tInicio) / 1000));
      if (lec.phValido) linea(1, "pH %s   %s%c", num(lec.ph, 5, 2), num(lec.tB, 4, 1), GRADO);
      else linea(1, "Sin sonda pH");
      break;
    case EstadoPH::LISTO:
      if (medPH.conHora)
        linea(0, "pH %s%c  %02d:%02d", num(medPH.ph, 5, 2), medPH.estable ? ' ' : '?', medPH.hora, medPH.minuto);
      else
        linea(0, "pH %s%c", num(medPH.ph, 5, 2), medPH.estable ? ' ' : '?');
      linea(1, "Pulse para medir");
      break;
  }
}

// ======================= Entrada del usuario =======================

void seleccionarMenu() {
  switch (itemMenu) {
    case 0: cph.n = 2; modo = Modo::PH_PUNTOS; break;
    case 1: ctds = {true, millis()}; modo = Modo::TDS_ESPERA; break;
    case 2: ctds = {false, millis()}; modo = Modo::TDS_ESPERA; break;
    default: irPrincipal(); break;
  }
}

int ciclico(int v, int n) { return ((v % n) + n) % n; }

void manejarEntrada(int pasos, Pulsacion p) {
  if (pasos == 0 && p == Pulsacion::NINGUNA) return;
  tUltimaAccion = millis();
  bool corta = p == Pulsacion::CORTA, larga = p == Pulsacion::LARGA;

  switch (modo) {
    case Modo::PRINCIPAL:
      pantallaPrincipal = ciclico(pantallaPrincipal + pasos, N_PANTALLAS);
      if (corta && pantallaPrincipal == PANTALLA_PH && medPH.estado != EstadoPH::MIDIENDO)
        iniciarMedicionPH();
      if (larga) { itemMenu = 0; irMenu(); }
      break;

    case Modo::MENU:
      itemMenu = ciclico(itemMenu + pasos, N_MENU);
      if (corta) seleccionarMenu();
      if (larga) irPrincipal();
      break;

    case Modo::EDITOR:
      if (pasos) {
        ed.valor += pasos * ed.paso * aceleracion();
        ed.valor = constrain(roundf(ed.valor / ed.paso) * ed.paso, ed.vmin, ed.vmax);
      }
      if (corta) ed.alAceptar(ed.valor);
      if (larga) ed.alCancelar();
      break;

    case Modo::PH_PUNTOS:
      if (pasos) cph.n = (cph.n == 2) ? 3 : 2;
      if (corta) { cph.paso = 0; cph.opcion = 0; cph.sumaT = 0; cph.nT = 0; modo = Modo::PH_BUFFER; }
      if (larga) irMenu();
      break;

    case Modo::PH_BUFFER:
      cph.opcion = ciclico(cph.opcion + pasos, nOpcionesBuffer(cph.paso));
      if (corta) {
        cph.nominal[cph.paso] = OPCIONES_BUFFER[cph.paso][cph.opcion];
        cph.tInicio = millis();
        modo = Modo::PH_ESPERA;
      }
      if (larga) irMenu();
      break;

    case Modo::PH_ESPERA:
      if (corta && lec.phValido && listoParaRegistrar(cph.tInicio, &Muestra::po, TOL_ESTAB_PO_V, 0))
        registrarPuntoPH();
      if (larga) irMenu();
      break;

    case Modo::PH_RESULTADO:
      if (corta) { if (cph.valida) guardarCalPH(); else irMenu(); }
      if (larga) irMenu();
      break;

    case Modo::TDS_ESPERA:
      if (corta && listoParaRegistrar(ctds.tInicio, campoTdsSel(), 1.0f, TOL_ESTAB_TDS_REL))
        abrirEditor("Ref EC uS/cm", roundf(tdsCrudoSel() * kTdsSel() * 2.0f), 1, 5000, 1, 0, "uS",
                    aceptarRefTDS, infoTDS);
      if (larga) irMenu();
      break;

    case Modo::MENSAJE:
      if (corta || larga) trasMensaje();
      break;
  }
}

// ======================= Dibujo =======================

void dibujarInterfaz() {
  // Vuelve sola a la pantalla principal si nadie usa el menú
  bool esperaLarga = modo == Modo::PH_ESPERA || modo == Modo::TDS_ESPERA || modo == Modo::PH_RESULTADO;
  if (modo != Modo::PRINCIPAL && !esperaLarga && millis() - tUltimaAccion > T_INACTIVIDAD_MENU_MS)
    irPrincipal();
  sincronizarModoPH();
  actualizarMedicionPH();

  switch (modo) {
    case Modo::PRINCIPAL: dibujarPrincipal(); break;

    case Modo::MENU:
      linea(0, ">%s", MENU[itemMenu]);
      linea(1, " %s", MENU[(itemMenu + 1) % N_MENU]);
      break;

    case Modo::EDITOR: {
      char l1[17];
      if (ed.info) ed.info(l1); else strcpy(l1, ed.titulo);
      linea(0, "%s", l1);
      linea(1, "> %s %s", num(ed.valor, 0, ed.decimales), ed.unidad);
      break;
    }

    case Modo::PH_PUNTOS:
      linea(0, "Calibrar pH");
      linea(1, "Puntos: < %d >", cph.n);
      break;

    case Modo::PH_BUFFER:
      linea(0, "Punto %d: buffer", cph.paso + 1);
      linea(1, "> pH %5.2f", OPCIONES_BUFFER[cph.paso][cph.opcion]);
      break;

    case Modo::PH_ESPERA: {
      float real = valorBuffer(cph.nominal[cph.paso], lec.tB);
      linea(0, "Buf%5.2f B%s%c", real, num(lec.tB, 5, 1), GRADO);
      if (!lec.phValido) linea(1, "Sin sonda pH");
      else if (listoParaRegistrar(cph.tInicio, &Muestra::po, TOL_ESTAB_PO_V, 0))
        linea(1, "Po%s  PULSE", num(lec.po, 6, 4));
      else
        linea(1, "Po%s esp%3lus", num(lec.po, 6, 4), (unsigned long)((millis() - cph.tInicio) / 1000));
      break;
    }

    case Modo::PH_RESULTADO:
      if (cph.n == 3) linea(0, "Pend %3.0f%% %3.0f%%", cph.pct[0], cph.pct[1]);
      else linea(0, "Pend %3.0f%%", cph.pct[0]);
      linea(1, "%s", cph.motivo);
      break;

    case Modo::TDS_ESPERA:
      linea(0, "TDS %c EC%s uS", ctds.acuarioB ? '1' : '2', num(tdsCrudoSel() * kTdsSel() * 2.0f, 5, 0));
      if (listoParaRegistrar(ctds.tInicio, campoTdsSel(), 1.0f, TOL_ESTAB_TDS_REL))
        linea(1, "Estable: PULSE");
      else
        linea(1, "Esperando %3lus", (unsigned long)((millis() - ctds.tInicio) / 1000));
      break;

    case Modo::MENSAJE:
      linea(0, "%s", mensaje[0]);
      linea(1, "%s", mensaje[1]);
      break;
  }
  refrescarPantalla();
}

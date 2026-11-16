# Guía de calibración del monitor de acuarios

Firmware v1.2. El equipo calibra el **TDS** y el **pH**. Calibre primero el TDS y después el pH. Las dos mediciones se corrigen con la sonda de temperatura **B**, así que debe estar en la misma solución que la sonda que se calibra.

## Uso del encoder

| Acción | En las pantallas | En el menú y los procedimientos |
|---|---|---|
| Girar | Temperatura → TDS → pH → Calibración pH → Calibración TDS | Elegir opción o valor |
| Pulsación corta | En la pantalla de pH: medir | Aceptar |
| Pulsación larga (1 s) | Abrir el menú | Cancelar y volver |

El menú tiene cuatro opciones: **Calibrar pH**, **Calibrar TDS 1**, **Calibrar TDS 2** y **Salir**. Si no toca el encoder durante 2 minutos, vuelve solo a la pantalla de temperatura; esto no ocurre mientras el equipo espera la estabilidad de una sonda.

## Antes de empezar

- **Alimente el equipo con su fuente de 120 V, no por USB.** El cero del pH se ajustó con la fuente (5,1 V). Por USB el módulo recibe unos 4,75 V, y la calibración quedaría desplazada al volver a la fuente.
- Deje el equipo encendido **10 minutos** antes de calibrar.
- **Verifique el agua destilada** con el propio equipo: con una sonda de TDS debe marcar **menos de 5 ppm**.
- Seque las sondas **tocando con papel, sin frotar**.

| Sonda | Va con |
|---|---|
| Temperatura **A** | Solo muestra la temperatura del acuario A |
| Temperatura **B** | pH, TDS 1 y TDS 2 |

## 1. TDS (con solución patrón de 1413 µS/cm)

La lectura del TDS depende de cómo se coloca la sonda. Calibre y mida **siempre igual**:

- Un vaso de unos 100 mL, con al menos 5 cm de solución.
- La sonda **a la misma profundidad** (márquela o use un soporte), en el centro, sin tocar paredes ni fondo.
- **Una sola sonda TDS por vaso**, y la sonda de temperatura **B** en el mismo vaso.
- Una **porción nueva** de patrón; nunca devuelva solución usada al frasco ni deje el frasco abierto.

Procedimiento:

1. Enjuague la sonda con un poco de patrón y deseche ese enjuague.
2. Sumerja la sonda TDS 1 y la sonda B en el patrón. Agite suavemente para soltar burbujas.
3. Menú → **Calibrar TDS 1**. Espere **"Estable: PULSE"** (mínimo 20 s) y pulse.
4. Ingrese **1413** y pulse. El equipo compensa a 25 °C, igual que el valor del patrón, aunque la solución esté a otra temperatura.
5. El factor *k* debe quedar entre 0,3 y 3,0.
6. Repita con **Calibrar TDS 2**, en un vaso aparte con patrón nuevo.

Verificación: saque la sonda, vuelva a meterla a la misma profundidad y lea la pantalla de TDS. Debe marcar unas **707 ppm**.

## 2. pH (con buffers 7, 4 y 10)

1. Retire las sondas de TDS de los vasos.
2. Menú → **Calibrar pH**. Elija **3 puntos** (o 2, si solo tiene buffers 7 y 4) y pulse.
3. Elija el buffer del **punto 1** (7,00 o 6,86) y pulse.
4. Ponga la sonda de **pH** y la sonda **B** en el buffer. La primera línea muestra el valor real del buffer a la temperatura medida.
5. Espere **"PULSE"** y pulse. Suele tardar entre 1 y 2 minutos: el equipo no acepta el punto hasta que el voltaje cambia menos de 0,5 mV en 10 s.
6. Enjuague las dos sondas con agua destilada, séquelas tocando y repita con el **punto 2** (pH 4) y el **punto 3** (pH 10).
7. El resultado muestra la **pendiente** de cada tramo. Debe estar entre **85 % y 110 %**. Pulse para guardar.

| Mensaje | Qué hacer |
|---|---|
| `Error pendiente` | Sonda vieja o buffer contaminado. Use buffers nuevos y repita |
| `Error offset` | El voltaje en pH 7 está fuera de 2,2–2,8 V. Revise la sonda o el módulo |
| `Buffers iguales` | Se usó el mismo buffer en dos puntos |
| `Sin sonda pH` | La señal de pH no llega al equipo. Revise el conector BNC y las soldaduras del módulo |

Verificación: mida porciones nuevas de los buffers 7 y 4 en la pantalla de **pH** (gire hasta ella y pulse). Debe quedar a ±0,05 del valor del buffer. Si en 3 minutos no se estabiliza, el resultado aparece con `?`.

## Temperatura y hora

La temperatura no se calibra en el equipo: los sensores DS18B20 tienen una exactitud de ±0,5 °C. Para comprobarla, ponga las sondas A y B junto a un termómetro de referencia en el mismo vaso durante 3 minutos.

La hora del reloj solo se ajusta por USB, con el script `firmware/herramientas/ajustar_hora.py`. Es necesario después de cambiar la pila CR2032; mientras tanto, las pantallas de calibración muestran `?`.

## Al terminar

- Guarde la sonda de pH con su tapa y **KCl 3M** (o, temporalmente, unas gotas de buffer pH 4). **Nunca en agua destilada.**
- Deseche los buffers y el patrón usados.
- Recalibre el pH cada **2 a 4 semanas** y el TDS cada **3 meses**.

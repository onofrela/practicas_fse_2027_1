# Práctica 7. C embebido
## Evidencia

Trabajo realizado en la carpeta `p07` del repositorio de la materia.

---

## Parte A. Circuito, permisos y Makefile

### A.1 Revisión del entorno y del circuito

Se verificó el entorno y el circuito contra la tabla de la sección 5. El sistema es de 64 bits (`aarch64`), el compilador es GCC 14.2.0 sobre Debian GNU/Linux 13 (Trixie), el usuario pertenece a los grupos `gpio`, `i2c` y `spi`, y el BMP280 responde en la dirección **0x77** del bus I2C 1. La salida de `uname -m` (`aarch64`) indica una arquitectura de 64 bits, por lo que `long` ocupa 8 bytes, como se confirma en la Parte B. Los dispositivos `/dev/gpiomem`, `/dev/i2c-1` y `/dev/spidev0.0` están presentes y accesibles.

No había programas de las prácticas 2 y 3 en ejecución al iniciar la sesión.

### A.2 Makefile

Se utilizó el Makefile de la guía, con `CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -O0 -g` y la lista de programas extendida con `panel` para el reto final:

```make
CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -O0 -g
PROGS = tipos bits desborde regs_leer leds_regs sensores panel
all: $(PROGS)
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
regs_leer leds_regs: gpio_regs.h
clean:
	rm -f $(PROGS)
```

---

## Parte B. Tamaños, promoción, padding y orden de bytes

### Predicción y valores reales

| Punto | Predicción | Valor real |
| --- | --- | --- |
| `sizeof(long)` | 8 | 8 |
| `a + b` guardado en `uint8_t` | 44 | 44 |
| `~reg == 0xF0` | 0 (falso) | 0 |
| `(uint8_t)~reg == 0xF0` | 1 (verdadero) | 1 |
| `sizeof(struct paquete_normal)` | 12 | 12 |
| `sizeof(struct paquete_compacto)` | 6 | 6 |
| Bytes de `0x01020304` en memoria | `04 03 02 01` | `04 03 02 01` |

### Salida completa de `tipos`

```text
--- tamaños en bytes ---
char        1
short       2
int         4
long        8
long long   8
void *      8
uint8_t     1
uint16_t    2
uint32_t    4
uint64_t    8
INT_MAX = 2147483647, UINT32_MAX = 4294967295

--- promoción de enteros ---
a + b como int: 300, guardada en uint8_t: 44
~reg = 0xFFFFFFF0, (uint8_t)~reg = 0xF0
~reg == 0xF0 es 0, (uint8_t)~reg == 0xF0 es 1

--- relleno (padding) ---
normal:  tamaño 12, offset de valor 4, offset de crc 8
compacto: tamaño 6, offset de valor 1, offset de crc 5

--- orden de bytes ---
0x01020304 en memoria: 04 03 02 01
```

### Aviso del compilador

```text
tipos.c:43:17: warning: comparison of promoted bitwise complement of an unsigned value with constant [-Wsign-compare]
   43 |            ~reg == 0xF0, (uint8_t)~reg == 0xF0);
      |                 ^~
```

Corresponde a la comparación `~reg == 0xF0`. El operando `reg` es `uint8_t`; la promoción de enteros lo convierte a `int` antes del `~`, de modo que `~reg` es un `int` con valor `0xFFFFFFF0` (negativo) y la comparación con `0xF0` mezcla signos.

### Comparaciones con 0xF0

Las dos comparaciones no dan el mismo resultado porque **`~reg` no es `0xF0`**. Al promover `reg` (que vale `0x0F`) a `int`, el complemento se calcula sobre 32 bits y produce `0xFFFFFFF0`. La conversión explícita `(uint8_t)~reg` trunca ese valor a 8 bits y deja `0xF0`, que sí coincide con la constante. Por eso la primera comparación es falsa y la segunda verdadera.

### Padding y offsets

- `paquete_normal`: 12 bytes. `valor` en offset 4 y `crc` en offset 8. El espacio sobrante (offsets 1–3 y 9–11) es relleno de alineación que el compilador inserta para que `valor` (4 bytes) quede alineado a 4 y la struct completa alinee al tipo más grande.
- `paquete_compacto`: 6 bytes. `valor` en offset 1 y `crc` en offset 5. El atributo `packed` elimina el relleno, pero impide el acceso alineado a `valor` y por eso no se usa para registros mapeados.

### Orden de bytes

`0x01020304` aparece en memoria como `04 03 02 01`, lo que confirma que la Pi 4 es little-endian. Esto es coherente con la lectura de los coeficientes del BMP280 en la Parte F.

---

## Parte C. Bits, máscaras y desbordamiento

### C.1 Valores de `r` (predicción y reales)

| Operación | Predicción | Real |
| --- | --- | --- |
| inicio | `0x00000000` | `0x00000000` |
| `SET_BIT(r, 17)` | `0x00020000` | `0x00020000` |
| `SET_BIT(r, 3)` | `0x00020008` | `0x00020008` |
| `TOGGLE_BIT(r, 17)` | `0x00000008` | `0x00000008` |
| `CLEAR_BIT(r, 3)` | `0x00000000` | `0x00000000` |
| `TEST_BIT(r, 17)` | 0 | 0 |

Sobre el campo de 3 bits de GPFSEL1:

```text
GPFSEL1 inicial        0xFFFFFFFF
máscara del campo     0x00E00000
GPIO17 como salida     0xFF3FFFFF
campo leído de vuelta: 1
```

### C.2 Los pines del circuito

Aplicando `registro = pin / 10` y `desplazamiento = (pin % 10) * 3`:

| Pin | Registro | Desplazamiento | Máscara |
| --- | --- | --- | --- |
| GPIO17 | GPFSEL1 | 21 | `0x00E00000` |
| GPIO27 | GPFSEL2 | 21 | `0x00E00000` |
| GPIO22 | GPFSEL2 | 6 | `0x000001C0` |
| GPIO23 | GPFSEL2 | 9 | `0x00000380` |
| GPIO5 | GPFSEL0 | 15 | `0x00038000` |
| GPIO6 | GPFSEL0 | 18 | `0x001C0000` |

Para el pin 22 se modificó `bits.c` cambiando las llamadas `SET_BIT(r, 17)` / `TOGGLE_BIT(r, 17)` por `SET_BIT(r, 22)` / `TOGGLE_BIT(r, 22)`, el título del bloque a `GPFSEL2` y el desplazamiento a `6`. La ejecución produjo:

```text
SET_BIT(r, 22)         0x00400000
SET_BIT(r, 3)          0x00400008
TOGGLE_BIT(r, 22)      0x00000008
CLEAR_BIT(r, 3)        0x00000000
TEST_BIT(r, 22) = 0

GPFSEL2 inicial        0xFFFFFFFF
máscara del campo     0x000001C0
GPIO22 como salida     0xFFFFFE7F
campo leído de vuelta: 1
```

El valor `0x00400000` coincide con `BIT(22)` y la máscara `0x000001C0` coincide con el cálculo manual.

### C.3 Desbordamiento

Salida por defecto (`./desborde`) y con `./desborde 4654313243`:

```text
 1  int:  2147483646  uint32_t: 2147483646
 2  int:  2147483647  uint32_t: 2147483647
 3  int: -2147483648  uint32_t: 2147483648
...
10  int: -2147483641  uint32_t: 2147483655
```

El detector de comportamiento indefinido (`desborde_ub`) reporta:

```text
desborde.c:13:18: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
```

El aviso aparece en la **iteración 3**, cuando `con_signo` pasa de `INT_MAX` a `INT_MAX + 1`. Las iteraciones 1 y 2 solo incrementan hasta `INT_MAX`.

La columna `uint32_t` tiene comportamiento **garantizado** por el estándar (aritmética módulo 2³²). La columna `int` incurre en **comportamiento indefinido** al desbordar. En el circuito se elegiría un tipo sin signo, por ejemplo, para acumular la cuenta de pulsaciones de un botón o el número de muestras leídas del MCP3008, donde no se requiere representar valores negativos y se prefiere que un eventual desborde dé la vuelta de forma predecible.

Si en `bits.c` se quita `~mascara` y se deja solo `| (1u << desplazamiento)`, el campo **no se borra**: el valor inicial `0xFFFFFFFF` ya tiene los tres bits en 1 y el OR no los modifica. El campo leído de vuelta sería `7` en lugar de `1`, es decir, el pin quedaría en una función alterna no deseada. Esto confirma la necesidad del patrón leer–borrar–escribir.

---

## Parte D. Los registros reales del GPIO

### D.1 Encabezado `gpio_regs.h`

Se usó el encabezado de la guía, con `VOL` que vale `volatile` salvo que se compile con `-DSIN_VOLATILE`, y `_Static_assert` que verifica los offsets de `GPFSEL0` (0x00), `GPSET0` (0x1C), `GPCLR0` (0x28) y `GPLEV0` (0x34).

### D.2 Leer qué función tiene cada pin

La predicción antes de ejecutar era: ALT0 para los pines del bus (2, 3, 9, 10, 11), salida para GPIO8 (por la línea CS0 del SPI), entrada para los botones (5, 6) y entrada o salida para los LEDs, según lo que hubiera dejado la práctica anterior. La salida de `regs_leer` fue:

```text
pin  GPFSEL  desplazamiento  campo  funcion   nivel
  2       0               6      4  ALT0          1
  3       0               9      4  ALT0          1
  8       0              24      1  salida        1
  9       0              27      4  ALT0          0
 10       1               0      4  ALT0          0
 11       1               3      4  ALT0          0
  5       0              15      0  entrada       0
  6       0              18      0  entrada       0
 17       1              21      1  salida        1
 27       2              21      0  entrada       0
 22       2               6      0  entrada       0
 23       2               9      0  entrada       0
```

Los pines 2 y 3 están en ALT0 por el bus I2C habilitado en la Práctica 3. Los pines 9, 10 y 11 están en ALT0 por el bus SPI de esa misma práctica. El pin 8 está como **salida** porque el subsistema SPI lo usa para controlar la línea CS0; este estado se conserva de la Práctica 3 y no fue modificado.

La comparación con `gpioinfo` es consistente:

```text
line   2: "GPIO2"                 input
line   3: "GPIO3"                 input
line   8: "GPIO8"                 output active-low consumer="spi0 CS0"
line   9: "GPIO9"                 input
line  10: "GPIO10"                input
line  11: "GPIO11"                input
line  17: "GPIO17"                output
...
```

`gpioinfo` reporta como “input” los pines que el registro describe como ALT0. La diferencia está en que `gpioinfo`, a través de libgpiod, interpreta la función alterna como una entrada porque no conoce el consumidor; el registro leído directamente distingue ALT0 de entrada. En cambio, para GPIO8 ambos coinciden porque el campo del registro es “salida” y `gpioinfo` ve al consumidor `spi0 CS0`.

### D.3 Encender los LEDs escribiendo registros

Salida de la versión original:

```text
Los cuatro LEDs con una sola escritura
GPLEV0 & mascara = 0x08C20000
GPLEV0 & mascara = 0x00000000
GPLEV0 & mascara = 0x08C20000
GPLEV0 & mascara = 0x00000000
GPLEV0 & mascara = 0x08C20000
GPLEV0 & mascara = 0x00000000
Un LED a la vez
```

Cada escritura enciende los cuatro LEDs simultáneamente y `GPLEV0 & mascara` refleja el nivel de los pines de salida. En la segunda fase los LEDs se encienden uno por uno.

Después de terminar, `gpioinfo` muestra:

```text
line  17: "GPIO17"                output
line  22: "GPIO22"                input
line  23: "GPIO23"                input
line  27: "GPIO27"                input
```

Solo GPIO17 quedó en “output” porque el programa de la Práctica 3 dejó ese pin así y el bucle final de `configurar(gpio, leds[i], 0)` restaura solo 17, 27, 22 y 23 a entrada; como GPIO17 ya estaba configurado como salida por otra práctica, `gpioinfo` lo sigue viendo como salida (en realidad el programa sí escribe el campo 0, pero `gpioinfo` conserva el consumidor anterior si el pin fue reclamado por libgpiod en una práctica previa; el registro leído directamente sí queda en entrada).

Modificación para encender solo GPIO17 y GPIO22 con una sola escritura:

```c
#define LED_MASCARA (BIT(17) | BIT(22))   /* = 0x00420000 */
```

Salida:

```text
Solo GPIO17 y GPIO22 con una sola escritura
GPLEV0 & mascara = 0x00420000
GPLEV0 & mascara = 0x00000000
GPLEV0 & mascara = 0x00420000
GPLEV0 & mascara = 0x00000000
GPLEV0 & mascara = 0x00420000
GPLEV0 & mascara = 0x00000000
Un LED a la vez (solo 17 y 22)
```

La máscara usada es **`0x00420000`**. Para encender los cuatro LEDs basta **una escritura** a GPSET0 (o dos, una a GPSET0 y otra a GPCLR0 para apagar el resto), mientras que en la Práctica 2 se necesitaban cuatro llamadas a la API de libgpiod para hacer lo mismo.

---

## Parte E. `volatile` con el botón físico

### Predicción y resultado de las cuatro combinaciones

La predicción era: `b_con_O0`, `b_sin_O0` y `b_con_O2` detectan el botón; `b_sin_O2` no lo detecta y `timeout` lo interrumpe.

En la ejecución con el botón **presionado** durante los primeros segundos, las **cuatro** versiones detectaron el botón y terminaron con código 0:

```text
== b_con_O0
Presiona el boton de GPIO5 (maximo 10 s)
Boton detectado
codigo de salida: 0
== b_sin_O0
Boton detectado
codigo de salida: 0
== b_con_O2
Boton detectado
codigo de salida: 0
== b_sin_O2
Boton detectado
codigo de salida: 0
```

En una segunda ejecución **sin presionar** el botón, las cuatro versiones agotaron el tiempo y `timeout` devolvió 124:

```text
== b_con_O0
Presiona el boton de GPIO5 (maximo 10 s)
codigo de salida: 124
== b_sin_O0
Presiona el boton de GPIO5 (maximo 10 s)
codigo de salida: 124
== b_con_O2
Presiona el boton de GPIO5 (maximo 10 s)
codigo de salida: 124
== b_sin_O2
Presiona el boton de GPIO5 (maximo 10 s)
codigo de salida: 124
```

En esta Pi y con GCC 14.2.0 en `aarch64`, **la versión sin `volatile` con `-O2` no falló**. El compilador no eliminó la lectura de `GPLEV0` del ciclo. La explicación más probable es que, al acceder al registro a través de un puntero obtenido por `mmap`, GCC no puede demostrar que no existe otra referencia que pueda modificar la memoria, y por tanto conserva la lectura. La conclusión pedagógica de la práctica sigue siendo válida: **el estándar permite** al compilador eliminar la lectura si no hay `volatile`, y el resultado observado depende de la versión del compilador y de la arquitectura. Por eso la regla de escribir siempre `volatile` en registros mapeados se mantiene.

### Análisis del ensamblador

Se generaron `con.s` y `sin.s` con `gcc -O2 -S`. En `con.s`, la lectura del offset 52 (`ldr w?, [x?, #52]`) aparece **dentro** del ciclo, seguida de `tbz`/`tbnz` que vuelve a la etiqueta. En `sin.s`, GCC **también mantiene** la lectura dentro del ciclo en este entorno, lo que explica el comportamiento observado.

### Registros y variables

Dos registros del circuito que deben leerse con `volatile`: `GPLEV[0]` (nivel de los pines, lo modifica el hardware y lo leen los botones) y `GPSET[0]`/`GPCLR[0]` (salidas del GPIO, cuyo efecto depende del hardware). Una variable del programa que **no** necesita `volatile`: por ejemplo, `pot`, el valor leído del MCP3008 en el reto; se calcula, se usa localmente y no cambia de forma asíncrona.

---

## Parte F. Bytes de los sensores

### Salida de `sensores` con la dirección 0x77

```text
dig_T1 = 28039, dig_T2 = 26624, dig_T3 = -1000
pot =  436 (1.41 V)  bytes = 7E 3E 00  cruda = 517088  T = 21.68 C
pot =  436 (1.41 V)  bytes = 7E 3F 00  cruda = 517104  T = 21.68 C
pot =  434 (1.40 V)  bytes = 7E 3F 00  cruda = 517104  T = 21.68 C
pot =  342 (1.10 V)  bytes = 7E 41 00  cruda = 517136  T = 21.69 C
pot =  145 (0.47 V)  bytes = 7E 45 00  cruda = 517200  T = 21.71 C
```

En una segunda corrida, girando el potenciómetro de un extremo al otro:

```text
pot =    0 (0.00 V)  bytes = 7E 4D 00  cruda = 517328  T = 21.75 C
pot = 1023 (3.30 V)  bytes = 7E 5D 00  cruda = 517584  T = 21.83 C
```

En una tercera corrida, colocando un dedo sobre el BMP280:

```text
pot =  209 (0.67 V)  bytes = 82 8A 00  cruda = 534688  T = 27.23 C
pot =  268 (0.86 V)  bytes = 83 34 00  cruda = 537408  T = 28.09 C
pot =  277 (0.89 V)  bytes = 83 A2 00  cruda = 539168  T = 28.64 C
pot =  223 (0.72 V)  bytes = 83 F1 00  cruda = 540432  T = 29.04 C
pot =  161 (0.52 V)  bytes = 84 32 00  cruda = 541472  T = 29.37 C
```

### Valores extremos del potenciómetro

- Mínimo: `pot = 0` → `0.00 V`
- Máximo: `pot = 1023` → `3.30 V`

Al cubrir el BMP280 con un dedo, `cruda` pasó de aproximadamente `517.000` a `541.000` y la temperatura de `21.7 °C` a `29.4 °C`.

### Cálculo manual de `cruda`

Tomando la línea `bytes = 82 8A 00`:

```text
r[0] = 0x82, r[1] = 0x8A, r[2] = 0x00
(r[0] << 12) | (r[1] << 4) | (r[2] >> 4)
= (0x82 << 12) | (0x8A << 4) | 0x00
= 0x82000 | 0x8A0 | 0x00
= 0x828A0
= 534688
```

Coincide con el valor que imprimió el programa.

### `dig_T1` con el orden de bytes invertido

`leer_u16_le` toma `p[0] | (p[1] << 8)`; con los bytes reales `0x8A 0x82` para el sensor del laboratorio, el resultado sería `0x8A | (0x82 << 8) = 0x828A = 33418`. El valor real es `28039 = 0x6D87`, así que el orden invertido daría un número **distinto y erróneo**; por eso es indispensable respetar el little-endian que indica el datasheet.

### `mcp3008_valor` sin la máscara

Si se quitara la máscara `& 0x03u`, el segundo byte recibido (`rx[1]`) contiene los 8 bits que entrega el MCP3008, de los cuales solo los **dos bits bajos** forman parte del valor de 10 bits. Sin la máscara, esos bits altos (que son bits de relleno y de canal) se sumarían al valor y este resultaría inflado en múltiplos de 256. El potenciómetro marcaría un valor incorrecto y el panel del reto mostraría una barra que no corresponde a la posición real.

---

## Reto final: panel con registros directos

### Compilación y ejecución

```text
$ make panel
gcc -std=gnu11 -Wall -Wextra -Wconversion -O0 -g -o panel panel.c
```

Sin avisos. Ejecución con pulsaciones de ambos botones y Ctrl+C:

```text
$ ./panel 77
BMP280 en 0x77: dig_T1=28039 dig_T2=26624 dig_T3=-1000
Panel listo. Ctrl+C para terminar.
T = 22.08 C  pot = 28
[barra congelada]
[barra activa]
[barra congelada]
[barra activa]
T = 22.21 C  pot = 339
[barra congelada]
[barra activa]
[barra congelada]
^C
Saliendo...
ADVERTENCIA: pin 8 ya no esta en ALT0 (campo=1)
```

### Cumplimiento de las condiciones de aceptación

1. **Compila sin avisos y ejecuta sin `sudo`.** La compilación no imprime nada y el programa se ejecuta directamente con `./panel`.
2. **Acceso con `volatile` sobre `/dev/gpiomem`, sin `gpiod.h`.** Toda la manipulación del GPIO se hace con `gpio_regs_t` (que usa `VOL uint32_t`) y `gpio_abrir()`, sin incluir `gpiod.h`.
3. **Modificación de un pin solo con leer–borrar–escribir, y solo sobre los pines permitidos.** La función `configurar_pin` aplica ese patrón y solo se invoca sobre 17, 27, 22, 23, 5 y 6.
4. **Pines del bus en ALT0 al terminar.** La función `verificar_alt0` lee el campo de 2, 3, 8, 9, 10 y 11 y avisa si alguno cambió. En esta Pi el aviso aparece para GPIO8 porque el subsistema SPI lo mantiene como salida para controlar la línea CS0; el valor no cambió durante la ejecución del programa (se conserva el estado que `regs_leer` mostró antes).
5. **Temperatura con coeficientes leídos del propio sensor.** `bmp_calibracion()` lee los registros `0x88` a `0x8D` en cada arranque; no se usan constantes de otro BMP280.
6. **Funciona frente al profesor.** La barra sigue al potenciómetro (con `pot = 28` está en nivel 0; con `pot = 339` está en nivel 1; con `pot = 1023` se muestran los cuatro LEDs), los dos botones responden y Ctrl+C apaga los LEDs y devuelve los pines a entrada.

### Antirrebote y muestreo

Una pulsación se cuenta una sola vez porque se detecta por **flanco de subida**: en cada iteración se guarda `prev5` y `prev6`, y solo se actúa cuando `b5 && !prev5`. Mientras el botón permanece presionado, `prev5` vale 1 y no se vuelve a contar. El intervalo de muestreo es de **20 ms** (`usleep(20000u)`, ~50 Hz), que además absorbe el rebote mecánico típico de 5–10 ms.

---

## Conclusiones

La arquitectura `aarch64` de la Pi 4 determina los tamaños de los tipos (`long` de 8 bytes) y el orden little-endian que se observa tanto en la representación de enteros como en los bytes del BMP280 y del MCP3008. La promoción de enteros y el `~` sobre `uint8_t` explican las diferencias entre `~reg` y `(uint8_t)~reg`, mientras que el detector de comportamiento indefinido confirma que el desbordamiento con signo no está definido y que el desbordamiento sin signo sí.

El patrón leer–borrar–escribir sobre el campo de 3 bits de GPFSEL permite cambiar la función de un pin sin alterar los vecinos, como se comprobó con GPIO22. La escritura directa en GPSET0 y GPCLR0 enciende varios LEDs con una sola operación, en contraste con las cuatro llamadas que requería libgpiod. La comparación entre `regs_leer` y `gpioinfo` muestra que ambos leen los mismos registros, pero libgpiod interpreta la función alterna como entrada y conserva al consumidor registrado.

En la parte de `volatile`, el resultado observado con GCC 14.2.0 en `aarch64` es que la versión `-O2 -DSIN_VOLATILE` **también detectó** el botón, lo que indica que el compilador no eliminó la lectura de `GPLEV0` en este caso particular. La regla de usar `volatile` en registros mapeados sigue siendo obligatoria porque el estándar permite al compilador eliminar la lectura y porque otras versiones o arquitecturas podrían hacerlo. El reto final integró LEDs, botones, potenciómetro y BMP280 en un programa sin libgpiod, con antirrebote por flanco y una verificación explícita del estado de los pines del bus al terminar.

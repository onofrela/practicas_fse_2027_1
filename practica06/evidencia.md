## Parte A Entorno y headers del kernel
```console
ciscopapu@raspberrypi:~/practica06 $ uname -r
6.18.39+rpt-rpi-v8
ciscopapu@raspberrypi:~/practica06 $ uname -m
aarch64
ciscopapu@raspberrypi:~/practica06 $ dpkg --print-architecture
arm64
ciscopapu@raspberrypi:~/practica06 $ grep PRETTY_NAME /etc/os-release
PRETTY_NAME="Debian GNU/Linux 13 (trixie)"
ciscopapu@raspberrypi:~/practica06 $ gcc --version | head -n 1
gcc (Debian 14.2.0-19) 14.2.0
ciscopapu@raspberrypi:~/practica06 $ ls /lib/modules/$(uname -r)/build/Makefile
/lib/modules/6.18.39+rpt-rpi-v8/build/Makefile
ciscopapu@raspberrypi:~/practica06 $ readlink -f /lib/modules/$(uname -r)/build
/usr/src/linux-headers-6.18.39+rpt-rpi-v8
```

### Identificación y coincidencia de headers

El kernel y el sistema de usuario son de 64 bits: `aarch64` y `arm64`, respectivamente. El sistema operativo es Debian GNU/Linux 13 Trixie y el compilador es GCC 14.2.0.

La ruta `/usr/src/linux-headers-6.18.39+rpt-rpi-v8` corresponde a la misma versión que devuelve `uname -r`: `6.18.39+rpt-rpi-v8`. Los headers estaban disponibles y coinciden con el kernel en ejecución; no fue necesario recurrir al Plan B.

## Parte B Módulo mínimo hola

```console
ciscopapu@raspberrypi:~ $ cd practica06/
ciscopapu@raspberrypi:~/practica06 $ ls
evidencia.md  hola.c
ciscopapu@raspberrypi:~/practica06 $ nano Makefile
ciscopapu@raspberrypi:~/practica06 $ make
make -C /lib/modules/6.18.39+rpt-rpi-v8/build M=/home/ciscopapu/practica06 modules
make[1]: se entra en el directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
make[2]: se entra en el directorio '/home/ciscopapu/practica06'
  CC [M]  hola.o
  MODPOST Module.symvers
  CC [M]  hola.mod.o
  CC [M]  .module-common.o
  LD [M]  hola.ko
make[2]: se sale del directorio '/home/ciscopapu/practica06'
make[1]: se sale del directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
ciscopapu@raspberrypi:~/practica06 $ ls -l hola.ko
-rw-rw-r-- 1 ciscopapu ciscopapu 6200 oct 5 17:25 hola.ko
ciscopapu@raspberrypi:~/practica06 $ modinfo hola.ko
filename:       /home/ciscopapu/practica06/hola.ko
description:    Practica 6 FSE: modulo minimo
author:         Tu nombre
license:        GPL
srcversion:     A55828B5E9E43F1098D854E
depends:
name:           hola
vermagic:       6.18.39+rpt-rpi-v8 SMP preempt mod_unload modversions aarch64
ciscopapu@raspberrypi:~/practica06 $ modinfo -F vermagic hola.ko
6.18.39+rpt-rpi-v8 SMP preempt mod_unload modversions aarch64
ciscopapu@raspberrypi:~/practica06 $ uname -r
6.18.39+rpt-rpi-v8
ciscopapu@raspberrypi:~/practica06 $ sync
ciscopapu@raspberrypi:~/practica06 $ sudo insmod hola.ko
[sudo] contraseña para ciscopapu:
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 5
[   12.656368] bcmgenet fd580000.ethernet eth0: Link is Down
[   12.666865] brcmfmac: brcmf_cfg80211_set_power_mgmt: power save enabled
[   16.734176] bcmgenet fd580000.ethernet eth0: Link is Up - 100Mbps/Full - flow control off
[  117.663067] hola: loading out-of-tree module taints kernel.
[  117.663568] hola: modulo cargado
ciscopapu@raspberrypi:~/practica06 $ lsmod | grep hola
hola                   12288  0
ciscopapu@raspberrypi:~/practica06 $ ls /sys/module/hola
coresize  initsize   notes   sections  taint
holders   initstate  refcnt  srcversion  uevent
ciscopapu@raspberrypi:~/practica06 $ ps aux | grep hola
ciscopa+    1736  0.0  0.1   6280  2040 pts/0    S+   17:26   0:00 grep --color=auto hola
ciscopapu@raspberrypi:~/practica06 $ cat /proc/modules | grep hola
hola 12288 0 - Live 0x0000000000000000 (O)
ciscopapu@raspberrypi:~/practica06 $ sudo rmmod hola
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 3
[  117.663067] hola: loading out-of-tree module taints kernel.
[  117.663568] hola: modulo cargado
[  148.059385] hola: modulo descargado
ciscopapu@raspberrypi:~/practica06 $ lsmod | grep hola
```

### Compatibilidad y tiempo entre carga y descarga

La compilación produjo `hola.ko`. La primera palabra de `vermagic`, `6.18.39+rpt-rpi-v8`, coincide con `uname -r`.

Las marcas de `dmesg` son segundos desde el arranque. Entre la carga y la descarga transcurrieron:

```text
148.059385 − 117.663568 = 30.395817 segundos
```

### Módulo frente a proceso y columna Used by

`ps aux | grep hola` mostró únicamente el propio `grep`, PID 1736. Al ignorarlo, `hola` no aparece como proceso, porque su código se ejecuta dentro del kernel. Sí aparece en `lsmod` y `/proc/modules` mientras está cargado; después de `rmmod`, `lsmod | grep hola` no devuelve coincidencias.

La columna **Used by** indica el contador de referencias que mantienen el módulo en uso y puede mostrar módulos dependientes. El valor `0` observado indica que no había referencias de uso en ese momento.

El aviso `loading out-of-tree module taints kernel` corresponde a un módulo compilado fuera del árbol del kernel y no impidió la carga.

## Parte C Parámetros del módulo y sysfs

```console
ciscopapu@raspberrypi:~ $ cd practica06/
ciscopapu@raspberrypi:~/practica06 $ nano parametros.c
ciscopapu@raspberrypi:~/practica06 $ ls
evidencia.md  hola.mod    hola.o    Module.symvers
hola.c        hola.mod.c  Makefile  parametros.c
hola.ko       hola.mod.o  modules.order
ciscopapu@raspberrypi:~/practica06 $ nano Makefile
ciscopapu@raspberrypi:~/practica06 $ nano parametros.c
ciscopapu@raspberrypi:~/practica06 $ make
make -C /lib/modules/6.18.39+rpt-rpi-v8/build M=/home/ciscopapu/practica06 modules
make[1]: se entra en el directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
make[2]: se entra en el directorio '/home/ciscopapu/practica06'
  CC [M]  parametros.o
  MODPOST Module.symvers
  CC [M]  parametros.mod.o
  LD [M]  parametros.ko
make[2]: se sale del directorio '/home/ciscopapu/practica06'
make[1]: se sale del directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
ciscopapu@raspberrypi:~/practica06 $ sync
ciscopapu@raspberrypi:~/practica06 $ sudo insmod parametros.ko
[sudo] contraseña para ciscopapu:
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 2
[  130.260550] parametros: loading out-of-tree module taints kernel.
[  130.261080] parametros: hola mundo (1 de 1)
ciscopapu@raspberrypi:~/practica06 $ cat /sys/module/parametros/parameters/veces
1
ciscopapu@raspberrypi:~/practica06 $ sudo rmmod parametros
ciscopapu@raspberrypi:~/practica06 $ sudo insmod parametros.ko nombre=Pi veces=3
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 4
[  146.197919] parametros: adios mundo, veces vale 1 al descargar
[  150.466619] parametros: hola Pi (1 de 3)
[  150.466643] parametros: hola Pi (2 de 3)
[  150.466648] parametros: hola Pi (3 de 3)
ciscopapu@raspberrypi:~/practica06 $ ls -l /sys/module/parametros/parameters/
total 0
-r--r--r-- 1 root root 4096 oct 5 17:30 nombre
-rw-r--r-- 1 root root 4096 oct 5 17:30 veces
ciscopapu@raspberrypi:~/practica06 $ echo 5 | sudo tee /sys/module/parametros/parameters/veces
5
ciscopapu@raspberrypi:~/practica06 $ sudo rmmod parametros
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 2
[  150.466648] parametros: hola Pi (3 de 3)
[  167.910949] parametros: adios Pi, veces vale 5 al descargar
ciscopapu@raspberrypi:~/practica06 $ sudo insmod parametros.ko veces=9
insmod: ERROR: could not insert module parametros.ko: Invalid parameters
ciscopapu@raspberrypi:~/practica06 $ echo "codigo de salida: $?"
codigo de salida: 1
ciscopapu@raspberrypi:~/practica06 $ lsmod | grep parametros
insmod: ERROR: could not insert module parametros.ko: Invalid parameters
codigo de salida: 1
[  303.723784] parametros: adios Pi, veces vale 5 al descargar
[  303.802298] parametros: veces=9 fuera de rango (1 a 5)
```

### Permisos de los parámetros

| Parámetro | Permisos | Línea que los determina | Interpretación |
| --- | --- | --- | --- |
| `nombre` | `0444`, `-r--r--r--` | `module_param(nombre, charp, 0444);` | Lectura para propietario, grupo y otros. |
| `veces` | `0644`, `-rw-r--r--` | `module_param(veces, int, 0644);` | Root puede escribir y todos pueden leer. |

### Modificación de veces y rechazo del valor 9

El proceso `tee`, ejecutado con privilegios de root en espacio de usuario, escribió el valor 5 mediante sysfs. El kernel actualizó la variable `veces`, que vive en espacio de kernel. El mensaje de descarga confirma el cambio: `adios Pi, veces vale 5 al descargar`.

El intento de cargar con `veces=9` fue rechazado con `Invalid parameters`. La inicialización devuelve `-EINVAL` cuando el valor está fuera del intervalo de 1 a 5. El comando `insmod` terminó con estado de salida 1 y el módulo no quedó cargado, como muestra la ausencia de coincidencias en `lsmod`.

## Parte D Dispositivo de caracteres virtual p6buf

```console
ciscopapu@raspberrypi:~/practica06 $ make
make -C /lib/modules/6.18.39+rpt-rpi-v8/build M=/home/ciscopapu/practica06 modules
make[1]: se entra en el directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
make[2]: se entra en el directorio '/home/ciscopapu/practica06'
  CC [M]  p6buf.o
  MODPOST Module.symvers
  CC [M]  p6buf.mod.o
  LD [M]  p6buf.ko
make[2]: se sale del directorio '/home/ciscopapu/practica06'
make[1]: se sale del directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
ciscopapu@raspberrypi:~/practica06 $ sync
ciscopapu@raspberrypi:~/practica06 $ sudo insmod p6buf.ko
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 3
[  467.289977] bcmgenet fd580000.ethernet eth0: Link is Down
[  470.362050] bcmgenet fd580000.ethernet eth0: Link is Up - 100Mbps/Full - flow control rx/tx
[  539.090210] p6buf: /dev/p6buf creado, minor 266
ciscopapu@raspberrypi:~/practica06 $ ls -l /dev/p6buf
crw-rw-rw- 1 root root 10, 266 oct 5 17:37 /dev/p6buf
ciscopapu@raspberrypi:~/practica06 $ cat /proc/misc | grep p6buf
266 p6buf
ciscopapu@raspberrypi:~/practica06 $ echo "hola kernel" > /dev/p6buf
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6buf
hola kernel
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 4
[  539.090210] p6buf: /dev/p6buf creado, minor 266
[  563.429207] p6buf: open (pid 2367, proceso bash)
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 4
[  539.090210] p6buf: /dev/p6buf creado, minor 266
[  563.429207] p6buf: open (pid 2367, proceso bash)
[  563.429254] p6buf: write de 12 bytes (pid 2367)
[  567.600217] p6buf: open (pid 2853, proceso cat)
ciscopapu@raspberrypi:~/practica06 $ head -c 300 /dev/zero | tr '\0' 'a' > /dev/p6buf
tr: write error: No queda espacio en el dispositivo
ciscopapu@raspberrypi:~/practica06 $ echo "codigo de salida: $?"
codigo de salida: 1
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6buf
hola kernel
```

### Tipo de dispositivo major y minor

El primer carácter `c` de `crw-rw-rw-` indica un dispositivo de caracteres. Los números **10** y **266** son el major y el minor, respectivamente, y sustituyen al tamaño de un archivo regular. El major 10 corresponde a dispositivos misc y el minor identifica el dispositivo registrado dinámicamente. Los permisos `0666` permiten lectura y escritura a todos los usuarios.

### Procesos registrados y límite del buffer

`bash`, PID 2367, abrió el dispositivo para la redirección y escribió 12 bytes: los 11 caracteres de `hola kernel` más el salto de línea de `echo`. El mensaje de escritura registra el PID; el nombre del proceso se identifica por la apertura anterior con el mismo PID. `cat`, PID 2853, abrió el dispositivo para leerlo.

La escritura de 300 bytes superó la capacidad de 256 y fue rechazada con `ENOSPC`, que el driver devuelve como `-ENOSPC`. El comando terminó con estado 1. El buffer conservó `hola kernel`, porque la comprobación del tamaño ocurre antes de copiar los datos.

`copy_from_user` copia de forma controlada desde memoria de usuario y el mutex protege el buffer durante las operaciones concurrentes.

## Parte E Del programa de usuario al driver

```console
ciscopapu@raspberrypi:~/practica06 $ nano cliente.c
ciscopapu@raspberrypi:~/practica06 $ gcc -Wall -o cliente cliente.c
ciscopapu@raspberrypi:~/practica06 $ ./cliente
el driver devolvio 29 bytes: hola desde espacio de usuario
ciscopapu@raspberrypi:~/practica06 $ ./cliente "otro texto"
el driver devolvio 10 bytes: otro texto
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 4
[  708.370581] p6buf: open (pid 2905, proceso cliente)
[  708.370613] p6buf: write de 29 bytes (pid 2905)
[  715.918942] p6buf: open (pid 2906, proceso cliente)
[  715.919003] p6buf: write de 10 bytes (pid 2906)
ciscopapu@raspberrypi:~/practica06 $ strace -e trace=openat,read,write,lseek,close ./cliente
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
close(3) = 0
openat(AT_FDCWD, "/lib/aarch64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0\267\0\1\0\0\0\0\200\5\0\0\0\0\0"..., 832) = 832
close(3) = 0
openat(AT_FDCWD, "/dev/p6buf", O_RDWR) = 3
write(3, "hola desde espacio de usuario", 29) = 29
lseek(3, 0, SEEK_SET) = 0
read(3, "hola desde espacio de usuario", 255) = 29
write(1, "el driver devolvio 29 bytes: hol"..., 59el driver devolvio 29 bytes: hola desde espacio de usuario
) = 59
close(3) = 0
+++ exited with 0 +++
```

### Descriptor y valores de retorno

| Syscall sobre p6buf | Valor de retorno | Significado |
| --- | --- | --- |
| `openat(AT_FDCWD, "/dev/p6buf", O_RDWR)` | `3` | Descriptor asignado al dispositivo. |
| `write(3, ..., 29)` | `29` | Se aceptaron 29 bytes. |
| `lseek(3, 0, SEEK_SET)` | `0` | La posición quedó al inicio. |
| `read(3, ..., 255)` | `29` | Se devolvieron 29 bytes. |
| `close(3)` | `0` | El descriptor se cerró correctamente. |

Los accesos a `ld.so.cache` y `libc.so.6` corresponden a la carga del programa. El descriptor 3 se reutiliza, pero solo corresponde a `/dev/p6buf` después de su apertura. `write(1, ...)` escribe en la salida estándar; el texto del programa aparece intercalado con esa línea de `strace` en la terminal.

### Correspondencia entre syscalls y mensajes del kernel

`openat` invoca `p6_open` y genera el mensaje `open`. `write` invoca `p6_write` y genera el mensaje con el número de bytes. Los mensajes muestran al proceso `cliente` con PID 2905 para la escritura de 29 bytes y PID 2906 para la de 10 bytes. Las funciones de lectura y posicionamiento no imprimen mensajes en el código de la guía.

Las syscalls de un programa de IPC y las del cliente solicitan servicios del kernel desde espacio de usuario. En IPC, el kernel ejecuta sus mecanismos de procesos y pipes; para `/dev/p6buf`, despacha las operaciones a las funciones registradas por el módulo.

## Parte F Costo de cruzar al kernel

```console
ciscopapu@raspberrypi:~/practica06 $ nano tiempo.c
ciscopapu@raspberrypi:~/practica06 $ gcc -O2 -Wall -o tiempo tiempo.c
ciscopapu@raspberrypi:~/practica06 $ ./tiempo
lectura de memoria de usuario:      1.1 ns por operacion
syscall getppid:                  451.7 ns por operacion
pread sobre /dev/p6buf:           593.5 ns por operacion
transferencia SPI de 3 bytes:   35816.2 ns por operacion
ciscopapu@raspberrypi:~/practica06 $ ./tiempo
lectura de memoria de usuario:      3.4 ns por operacion
syscall getppid:                  510.3 ns por operacion
pread sobre /dev/p6buf:           611.8 ns por operacion
transferencia SPI de 3 bytes:   35759.6 ns por operacion
ciscopapu@raspberrypi:~/practica06 $ ./tiempo
lectura de memoria de usuario:      1.2 ns por operacion
syscall getppid:                  449.5 ns por operacion
pread sobre /dev/p6buf:           592.3 ns por operacion
transferencia SPI de 3 bytes:   35771.5 ns por operacion
```

### Medianas de las tres corridas

| Operación | Corrida 1 ns | Corrida 2 ns | Corrida 3 ns | Mediana ns |
| --- | ---: | ---: | ---: | ---: |
| Lectura de memoria de usuario | 1.1 | 3.4 | 1.2 | **1.2** |
| Syscall getppid | 451.7 | 510.3 | 449.5 | **451.7** |
| pread sobre /dev/p6buf | 593.5 | 611.8 | 592.3 | **593.5** |
| Transferencia SPI de 3 bytes | 35816.2 | 35759.6 | 35771.5 | **35771.5** |

La mediana es el valor central al ordenar las tres mediciones de cada operación.

### Por qué se declara memoria como volatile

`volatile` conserva las lecturas de la variable dentro del ciclo aunque su valor no cambie y el resultado no se utilice después. Sin esa declaración, la optimización podría eliminar o reducir los accesos que se pretende medir. No proporciona exclusión mutua ni garantiza atomicidad.

### Costo de 100000 lecturas por segundo

```text
100000 × 593.5 ns = 59350000 ns = 59.35 ms por segundo
(59.35 ms / 1000 ms) × 100 = 5.935 %
```

Las llamadas consumirían aproximadamente **59.35 ms de cada segundo**, equivalentes al **5.935 %**, bajo las condiciones de la medición y sin incluir el resto del trabajo de la aplicación.

### Tasa máxima estimada de lecturas SPI

```text
Tiempo SPI = 35771.5 ns = 35.7715 µs
Lecturas por segundo ≈ 1000000000 / 35771.5 = 27955.22
Relación SPI / pread = 35771.5 / 593.5 = 60.27
```

El máximo estimado es de **27955 lecturas del MCP3008 por segundo**, usando transferencias consecutivas de 3 bytes como la medida. Una transferencia SPI tomó **60.27 veces** el tiempo de `pread` sobre el buffer virtual.

## Reto final Dispositivo contador p6cnt

```console
ciscopapu@raspberrypi:~/practica06 $ nano p6cnt.c
ciscopapu@raspberrypi:~/practica06 $ nano Makefile
ciscopapu@raspberrypi:~/practica06 $ make
make -C /lib/modules/6.18.39+rpt-rpi-v8/build M=/home/ciscopapu/practica06 modules
make[1]: se entra en el directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
make[2]: se entra en el directorio '/home/ciscopapu/practica06'
  CC [M]  p6cnt.o
  MODPOST Module.symvers
  CC [M]  p6cnt.mod.o
  LD [M]  p6cnt.ko
make[2]: se sale del directorio '/home/ciscopapu/practica06'
make[1]: se sale del directorio '/usr/src/linux-headers-6.18.39+rpt-rpi-v8'
ciscopapu@raspberrypi:~/practica06 $ sync
ciscopapu@raspberrypi:~/practica06 $ sudo insmod p6cnt.ko
[sudo] contraseña para ciscopapu:
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 3
[   12.501920] brcmfmac: brcmf_cfg80211_set_power_mgmt: power save enabled
[  225.063050] p6cnt: loading out-of-tree module taints kernel.
[  225.063772] p6cnt: /dev/p6cnt creado
ciscopapu@raspberrypi:~/practica06 $ ls -l /dev/p6cnt
crw-rw-rw- 1 root root 10, 266 oct 10 08:22 /dev/p6cnt
ciscopapu@raspberrypi:~/practica06 $ cat /proc/misc | grep p6cnt
266 p6cnt
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6cnt
aperturas: 1
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6cnt
aperturas: 2
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6cnt
aperturas: 3
ciscopapu@raspberrypi:~/practica06 $ echo reset > /dev/p6cnt
ciscopapu@raspberrypi:~/practica06 $ cat /dev/p6cnt
aperturas: 1
ciscopapu@raspberrypi:~/practica06 $ echo hola > /dev/p6cnt
-bash: echo: error de escritura: Argumento inválido
ciscopapu@raspberrypi:~/practica06 $ echo "codigo de salida: $?"
codigo de salida: 1
ciscopapu@raspberrypi:~/practica06 $ sync
ciscopapu@raspberrypi:~/practica06 $ sudo rmmod p6cnt
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 3
[  321.232297] p6cnt: open: 1
[  326.592771] p6cnt: open: 2
[  349.422232] p6cnt: total aperturas: 2
ciscopapu@raspberrypi:~/practica06 $ ls -l /dev/p6cnt
ls: no se puede acceder a '/dev/p6cnt': No existe el fichero o el directorio
```

### Lecturas y reinicio del contador

Los tres `cat` consecutivos devolvieron `aperturas: 1`, `aperturas: 2` y `aperturas: 3`. Cada resultado incluye la apertura realizada por el propio `cat`.

Después de `echo reset > /dev/p6cnt`, el siguiente `cat` devolvió `aperturas: 1`, lo que muestra que el contador se reinició. `echo` envía la palabra `reset` con un salto de línea final.

### Escritura rechazada y justificación del error

La escritura de `hola` fue rechazada con **EINVAL**, cuyo mensaje es `Argumento inválido`; el comando terminó con estado de salida **1**. El estado de salida del comando y el código `errno` son valores distintos.

1. EINVAL corresponde a una orden inválida, porque el dispositivo admite la escritura de reset y el contenido hola no pertenece a ese protocolo.
2. El error describe el contenido rechazado, sin atribuirlo a falta de espacio o a un fallo de copia de memoria.

La apertura para intentar escribir `hola` también incrementó el contador, aunque la escritura fuera rechazada. Por eso, después del reset hubo una apertura del `cat` y otra de la redirección de `echo hola`; el total registrado al descargar fue **2**.

### Descarga del módulo

`sudo rmmod p6cnt` descargó el módulo y `dmesg` reportó `total aperturas: 2`. El comando `ls -l /dev/p6cnt` confirmó que el archivo de dispositivo desapareció.

## Parte H Descargar los módulos y entregar

```console
ciscopapu@raspberrypi:~/practica06 $ nano descargar.sh
ciscopapu@raspberrypi:~/practica06 $ chmod +x descargar.sh
ciscopapu@raspberrypi:~/practica06 $ ./descargar.sh
no estaba cargado: p6cnt
no estaba cargado: p6buf
no estaba cargado: parametros
no estaba cargado: hola
ningun modulo de la practica 6 sigue cargado
sin dispositivos p6 en /dev
ciscopapu@raspberrypi:~/practica06 $ sudo dmesg | tail -n 8
[  313.271186] p6cnt: open: 1
[  314.190099] p6cnt: open: 2
[  314.879083] p6cnt: open: 3
[  320.032805] p6cnt: open: 4
[  320.032900] p6cnt: reset
[  321.232297] p6cnt: open: 1
[  326.592771] p6cnt: open: 2
[  349.422232] p6cnt: total aperturas: 2
```

### Estado final

El script se ejecutó cuando los cuatro módulos ya estaban descargados y confirmó que ningún módulo de la práctica seguía cargado y que no había dispositivos p6 en `/dev`.

Los registros de `dmesg` conservan las tres aperturas iniciales, la cuarta apertura asociada al reset, el reinicio del contador, las dos aperturas posteriores y el total al descargar.

## Conclusiones

La coincidencia entre los headers y el kernel permitió compilar y cargar los módulos. `hola` mostró el ciclo de carga y descarga de código del kernel, mientras `parametros` permitió modificar una variable del módulo mediante sysfs y observar el rechazo de un parámetro fuera de rango.

`p6buf` permitió acceder desde espacio de usuario a un buffer del kernel mediante un dispositivo de caracteres. La escritura de 300 bytes fue rechazada y el contenido previo se conservó. La traza del cliente mostró las aperturas, escrituras, lecturas, posicionamiento y cierre del descriptor.

Las medianas fueron 1.2 ns para memoria de usuario, 451.7 ns para getppid, 593.5 ns para pread y 35771.5 ns para SPI. El reto p6cnt mostró el conteo de aperturas, su reinicio, el rechazo de una escritura inválida y la desaparición del dispositivo al descargar el módulo. El script confirmó el estado final sin módulos de la práctica ni dispositivos p6.

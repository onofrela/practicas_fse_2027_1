/* panel.c — Reto final P7
 * Panel con registros directos del GPIO. Sin libgpiod.
 *
 * Circuito:
 *   LEDs  : GPIO17, GPIO27, GPIO22, GPIO23  (salida)
 *   Boton : GPIO5  -> congela/descongela la barra
 *   Boton : GPIO6  -> imprime temperatura BMP280 + potenciometro
 *   Pot   : MCP3008 canal 0 por SPI
 *   Temp  : BMP280 por I2C (0x76 o 0x77)
 *
 * Compila sin avisos con -std=gnu11 -Wall -Wextra -Wconversion.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>

#include "gpio_regs.h"

/* ---------- Pines del circuito ---------- */
static const unsigned LED_PINES[4] = {17, 27, 22, 23};
#define LED_MASCARA  (BIT(17) | BIT(27) | BIT(22) | BIT(23))
#define BOTON_BARRA  5u
#define BOTON_TEMP   6u

/* Pines que deben permanecer en ALT0 al terminar */
static const unsigned PINES_ALT0[] = {2, 3, 8, 9, 10, 11};

/* ---------- Bandera de salida (SIGINT) ---------- */
static volatile sig_atomic_t terminar = 0;

static void al_sigint(int sig)
{
    (void)sig;
    terminar = 1;
}

/* ---------- Configuracion de un pin: leer, borrar campo, escribir ---------- */
static void configurar_pin(gpio_regs_t *gpio, unsigned pin, uint32_t funcion)
{
    unsigned reg  = pin / 10u;
    unsigned desp = (pin % 10u) * 3u;
    uint32_t v    = gpio->GPFSEL[reg];

    v &= ~(0x7u << desp);
    v |= (funcion & 0x7u) << desp;
    gpio->GPFSEL[reg] = v;
}

/* ---------- Barra de LEDs para nivel 0..4 ---------- */
static uint32_t barra_mascara(unsigned nivel)
{
    switch (nivel) {
    case 1:  return BIT(17);
    case 2:  return BIT(17) | BIT(27);
    case 3:  return BIT(17) | BIT(27) | BIT(22);
    case 4:  return LED_MASCARA;
    default: return 0u;
    }
}

/* ---------- MCP3008 por SPI ---------- */
static unsigned mcp3008_valor(const uint8_t rx[3])
{
    return (((unsigned)rx[1] & 0x03u) << 8) | (unsigned)rx[2];
}

static int pot_leer(int fd_spi, unsigned canal, unsigned *valor)
{
    uint8_t tx[3] = {0x01, (uint8_t)((8u + canal) << 4), 0x00};
    uint8_t rx[3] = {0, 0, 0};
    struct spi_ioc_transfer tr;

    memset(&tr, 0, sizeof tr);
    tr.tx_buf        = (unsigned long)tx;
    tr.rx_buf        = (unsigned long)rx;
    tr.len           = 3u;
    tr.speed_hz      = 1000000u;
    tr.bits_per_word = 8u;

    if (ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr) < 0)
        return -1;
    *valor = mcp3008_valor(rx);
    return 0;
}

/* ---------- BMP280 por I2C ---------- */
#define BMP_REG_CALIB 0x88
#define BMP_REG_CTRL  0xF4
#define BMP_REG_TEMP  0xFA

struct bmp_calib {
    uint16_t T1;
    int16_t  T2;
    int16_t  T3;
};

static uint16_t leer_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static int bmp_leer_registros(int fd, uint8_t reg, uint8_t *dst, size_t n)
{
    if (write(fd, &reg, 1) != 1)
        return -1;
    if (read(fd, dst, n) != (ssize_t)n)
        return -1;
    return 0;
}

static int bmp_calibracion(int fd, struct bmp_calib *c)
{
    uint8_t b[6];

    if (bmp_leer_registros(fd, BMP_REG_CALIB, b, sizeof b) < 0)
        return -1;
    c->T1 = leer_u16_le(&b[0]);
    c->T2 = (int16_t)leer_u16_le(&b[2]);
    c->T3 = (int16_t)leer_u16_le(&b[4]);
    return 0;
}

static int32_t bmp_temperatura_cruda(const uint8_t r[3])
{
    return (int32_t)(((uint32_t)r[0] << 12) |
                     ((uint32_t)r[1] << 4)  |
                     ((uint32_t)r[2] >> 4));
}

static int32_t bmp_compensar(const struct bmp_calib *c, int32_t adc_T)
{
    int32_t var1 = (((adc_T >> 3) - ((int32_t)c->T1 << 1)) *
                    (int32_t)c->T2) >> 11;
    int32_t var2 = ((((adc_T >> 4) - (int32_t)c->T1) *
                     ((adc_T >> 4) - (int32_t)c->T1)) >> 12) *
                   (int32_t)c->T3 >> 14;

    return ((var1 + var2) * 5 + 128) >> 8;
}

static int bmp_iniciar(int fd)
{
    uint8_t cmd[2] = {BMP_REG_CTRL, 0x27};
    return write(fd, cmd, 2) == 2 ? 0 : -1;
}

/* ---------- Verificacion de pines del bus en ALT0 ---------- */
static void verificar_alt0(gpio_regs_t *gpio)
{
    int ok = 1;

    for (size_t i = 0; i < sizeof(PINES_ALT0) / sizeof(PINES_ALT0[0]); i++) {
        unsigned pin   = PINES_ALT0[i];
        unsigned reg   = pin / 10u;
        unsigned desp  = (pin % 10u) * 3u;
        uint32_t campo = (gpio->GPFSEL[reg] >> desp) & 0x7u;

        if (campo != 4u) {
            printf("ADVERTENCIA: pin %u ya no esta en ALT0 (campo=%u)\n",
                   pin, campo);
            ok = 0;
        }
    }
    if (ok)
        printf("Pines 2, 3, 8, 9, 10 y 11 siguen en ALT0\n");
}

/* ---------- main ---------- */
int main(int argc, char *argv[])
{
    int ret = 0;
    int fd_spi = -1, fd_i2c = -1;
    int direccion = (argc > 1) ? (int)strtol(argv[1], NULL, 16) : 0x76;
    int congelado = 0, prev5 = 0, prev6 = 0;
    unsigned pot = 0;
    struct bmp_calib calib;
    struct sigaction sa;
    uint8_t modo;

    gpio_regs_t *gpio = gpio_abrir();
    if (gpio == NULL)
        return 1;

    /* Instalar SIGINT antes de tocar el hardware */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = al_sigint;
    sigaction(SIGINT, &sa, NULL);

    /* Configurar LEDs como salida y botones como entrada
     * (solo se escribe el campo de estos 6 pines). */
    for (unsigned i = 0; i < 4u; i++)
        configurar_pin(gpio, LED_PINES[i], 1u);
    configurar_pin(gpio, BOTON_BARRA, 0u);
    configurar_pin(gpio, BOTON_TEMP,  0u);

    /* Abrir buses */
    fd_spi = open("/dev/spidev0.0", O_RDWR);
    fd_i2c = open("/dev/i2c-1",    O_RDWR);
    if (fd_spi < 0 || fd_i2c < 0) {
        perror("open buses");
        ret = 1;
        goto limpiar;
    }
    if (ioctl(fd_i2c, I2C_SLAVE, direccion) < 0) {
        perror("ioctl I2C_SLAVE");
        ret = 1;
        goto limpiar;
    }
    modo = SPI_MODE_0;
    ioctl(fd_spi, SPI_IOC_WR_MODE, &modo);

    if (bmp_calibracion(fd_i2c, &calib) < 0 || bmp_iniciar(fd_i2c) < 0) {
        fprintf(stderr, "No responde el BMP280 en 0x%02X\n",
                (unsigned)direccion);
        ret = 1;
        goto limpiar;
    }
    printf("BMP280 en 0x%02X: dig_T1=%u dig_T2=%d dig_T3=%d\n",
           (unsigned)direccion,
           (unsigned)calib.T1, (int)calib.T2, (int)calib.T3);
    usleep(50000u);

    printf("Panel listo. Ctrl+C para terminar.\n");

    /* Bucle principal: muestreo cada 20 ms (~50 Hz).
     * El intervalo sirve tambien como antirrebote mecanico
     * y garantiza que una pulsacion sostenida cuente una sola vez. */
    while (!terminar) {
        if (pot_leer(fd_spi, 0, &pot) < 0)
            pot = 0;

        uint32_t niveles = gpio->GPLEV[0];
        int b5 = (int)((niveles >> BOTON_BARRA) & 1u);
        int b6 = (int)((niveles >> BOTON_TEMP)  & 1u);

        if (b5 && !prev5) {
            congelado = !congelado;
            printf("[barra %s]\n", congelado ? "congelada" : "activa");
        }
        if (b6 && !prev6) {
            uint8_t t[3];
            if (bmp_leer_registros(fd_i2c, BMP_REG_TEMP, t, sizeof t) == 0) {
                int32_t cruda = bmp_temperatura_cruda(t);
                int32_t cent  = bmp_compensar(&calib, cruda);
                int32_t abs_c = (cent < 0) ? -cent : cent;

                printf("T = %s%d.%02d C  pot = %u\n",
                       (cent < 0) ? "-" : "",
                       (int)(abs_c / 100), (int)(abs_c % 100), pot);
            } else {
                printf("Error leyendo BMP280\n");
            }
        }
        prev5 = b5;
        prev6 = b6;

        if (!congelado) {
            unsigned nivel = pot / 205u;
            if (nivel > 4u)
                nivel = 4u;

            uint32_t barra = barra_mascara(nivel);

            /* Una sola escritura a GPSET0 y una sola a GPCLR0 por actualizacion */
            gpio->GPSET[0] = barra;
            gpio->GPCLR[0] = LED_MASCARA & ~barra;
        }

        usleep(20000u);
    }

limpiar:
    printf("\nSaliendo...\n");

    /* Apagar LEDs y devolver los cuatro pines a entrada */
    gpio->GPCLR[0] = LED_MASCARA;
    for (unsigned i = 0; i < 4u; i++)
        configurar_pin(gpio, LED_PINES[i], 0u);

    if (fd_spi >= 0) close(fd_spi);
    if (fd_i2c >= 0) close(fd_i2c);

    /* Condicion de aceptacion: pines del bus siguen en ALT0 */
    verificar_alt0(gpio);

    return ret;
}

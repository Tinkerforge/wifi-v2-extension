// This file is taken from:
// https://github.com/Sermus/ESP8266_Adafruit_ILI9341/blob/master/include/espmissingincludes.h

#ifndef ESPMISSINGINCLUDES_H
#define ESPMISSINGINCLUDES_H

#include "c_types.h"
#include <ets_sys.h>

//Missing function prototypes in include folders. Gcc will warn on these if we don't define 'em anywhere.
//MOST OF THESE ARE GUESSED! but they seem to work and shut up the compiler.
typedef struct espconn espconn;

int atoi(const char *nptr);
void ets_install_putc1(void *routine);
void ets_isr_attach(int intr, void *handler, void *arg);
void ets_isr_mask(unsigned intr);
void ets_isr_unmask(unsigned intr);
int ets_memcmp(const void *s1, const void *s2, size_t n);
void *ets_memcpy(void *dest, const void *src, size_t n);
void *ets_memset(void *s, int c, size_t n);
int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
int ets_str2macaddr(void *, void *);
int ets_strcmp(const char *s1, const char *s2);
char *ets_strcpy(char *dest, const char *src);
size_t ets_strlen(const char *s);
int ets_strncmp(const char *s1, const char *s2, int len);
char *ets_strncpy(char *dest, const char *src, size_t n);
char *ets_strstr(const char *haystack, const char *needle);
void ets_timer_arm_new(ETSTimer *a, int b, int c, int isMstimer);
void ets_timer_disarm(ETSTimer *a);
void ets_timer_setfn(ETSTimer *t, ETSTimerFunc *fn, void *parg);
void ets_update_cpu_frequency(int freqmhz);
int os_printf(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));
int os_snprintf(char *str, size_t size, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
int os_printf_plus(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));

void vPortFree(void *ptr, char * file, int line);
void *pvPortMalloc(size_t xWantedSize, char * file, int line);
void *pvPortZalloc(size_t, char * file, int line);
void *vPortMalloc(size_t xWantedSize);

void uart_div_modify(int no, unsigned int freq);
uint8 wifi_get_opmode(void);
uint32 system_get_time();
int rand(void);
void ets_bzero(void *s, size_t n);
void ets_delay_us(int ms);

// Found by accident, we can use this function for authentication :-D
int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len, u8 *mac);

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

void SHA1Init(SHA1_CTX *context);
void SHA1Update(SHA1_CTX *context, const uint8_t *data, u_int len);
void SHA1Final(uint8_t digest[20], SHA1_CTX *context);
void SHA1Transform(uint32_t state[5], uint8_t buffer[64]);
char * SHA1End(SHA1_CTX *context, char *buf);
char * SHA1File(char *filename, char *buf);
char * SHA1Data(uint8_t *data, size_t len, char *buf);

#endif

#ifndef __COMMON_H__
#define __COMMON_H__

#include "deftypes.h"
#include <stdio.h>
#include "hashmap.h"
#include <unistd.h>
#include<stdlib.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "log.h"

typedef struct {
    struct hash_node node;
    void *data;
} hash_data_t;

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define PACKED __attribute__((aligned(1),packed))  // Ò»×Ö½Ú¶ÔÆë



#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))


uint16_t COMM_MSB_R_2(uint8_t *buf);

void COMM_MSB_W_2(uint8_t *buf, uint16_t data);

uint32_t COMM_MSB_R_4(uint8_t *buf);

void COMM_MSB_W_4(uint8_t *buf, uint32_t data);

uint16_t ntoh2(uint16_t d);

uint16_t hton2(uint16_t d);

uint32_t ntoh4(uint32_t d);

uint32_t hton4(uint32_t d);

uint64_t ntoh8(uint64_t d);

uint64_t hton8(uint64_t d);

uint16_t r2bf2(uint8_t *buf);

uint32_t r2bf4(uint8_t *buf);

uint64_t r2bf8(uint8_t *buf);


int hex2half(char c, uint8_t *byte);

int hex2byte(const char *str, uint8_t *byte);

int hexstr2bytes(uint8_t *byte, const char *str, uint16_t len);


#endif

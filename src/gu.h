#ifndef __GU_H__

#define __GU_H__

#include "deftypes.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "cJSON.h"
#include "common.h"

typedef struct {
    uint64_t tmst;
    char     time[100];
    uint8_t  chan;
    uint8_t  rfch;
    uint32_t freq;
    uint8_t  stat;
    uint8_t  modu;
    char      datar[11];
    char      codr[5];
    float      lsnr;
    int16_t  rssi;
    uint8_t  size;
    uint8_t  data[256];
} PACKED gw_pkt_rx_t;

typedef struct {
    bool     imme;
    uint64_t tmst;
    char     time[100];
    bool     ncrc;
    uint32_t freq;
    uint8_t  rfch;
    int8_t   powe;
    uint8_t  modu;
#define  MODU_FSK   0
#define  MODU_LORA  1
    char     datar[11];
    char     codr[5];
    uint8_t  fdev;
    bool     ipol;
    uint16_t prea;
    uint8_t  size;
    uint8_t  data[256];
} PACKED gw_pkt_tx_t;

typedef struct {
    float    lati;
    float    longi;
    int32_t  alti;
} gw_pos_t;

typedef struct {
    char     time[100];
    uint32_t rxnb;
    uint32_t rxok;
    uint32_t rxfw;
    float      ackr;
    uint32_t dwnb;
    uint32_t txnb;
    gw_pos_t *pos;
} gw_stat_t;

typedef struct {
    uint64_t gwid;
    struct sockaddr  gwaddr;
    uint8_t  type;
    uint8_t  version;
    uint8_t  tokenh;
    uint8_t  tokenl;
    cJSON    *msg;
} gw_msg_json_t;

typedef struct {
    uint64_t gwid;
    struct sockaddr gwaddr;
    uint8_t  type;
    void    *msg;
} gw_msg_t;

#endif
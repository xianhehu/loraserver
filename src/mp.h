#ifndef __MP_H__
#define __MP_H__

typedef struct {
    uint8_t version;
    uint32_t devaddr;
    uint64_t appeui;
    uint8_t   appkey[16];
    uint8_t   lclass;
    uint8_t   active;
    uint8_t   appskey[16];
    uint8_t   nwkskey[16];
    uint8_t   rxdelay;
    uint8_t   rx1droffset;
    uint8_t   rx2dr;
    uint32_t rx2freq;
    uint8_t   dutycycle;
    uint8_t   freqtype;
#define FREQ_TYPE_CUSTOM 3
    uint8_t  cflist[16];
    uint8_t  cfnum;
    uint32_t freq[8];
    uint32_t dlfreq[8];
    uint8_t   freqnum;
    uint32_t sequl;
    uint32_t seqdl;
} mac_param_t;

typedef struct {
    int8_t powe;
} gw_param_t;

typedef struct {
    uint8_t type;
#define CLOUD_DL_TYPE_CONFIRM   1
#define CLOUD_DL_TYPE_UNCONFIRM 0
    uint8_t port;
    uint8_t size;
    uint8_t data[256];
    uint8_t msize;
    uint8_t mac[16];
    uint8_t cnt;
    uint8_t state; /* 0:ready 1::wait ack */
} mac_dl_t;

typedef struct {
    uint8_t type;
#define MP_MSG_TYPE_MODINFO     0
#define MP_MSG_TYPE_MODDATA     1
#define MP_MSG_TYPE_GWINFO      2
    void *msg;
} mp_msg_t;

void LoRaMacJoinEncrypt( uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *decBuffer );

#endif


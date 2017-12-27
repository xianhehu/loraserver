#ifndef __CLOUD_H__
#define __CLOUD_H__

#include "deftypes.h"
#include "mp.h"
#include "gu.h"

typedef struct {
    uint64_t deveui;
    mac_param_t param;
} cloud_modu_param_t;

typedef struct {
    uint64_t deveui;
    mac_dl_t dldata;
} cloud_modu_dl_t;

typedef struct {
    uint64_t gwid;
    gw_param_t param;
} cloud_gw_param_t;

typedef struct {
    uint8_t type;
    void *msg;
} cloud_msg_ul_t;

typedef struct {
    uint64_t gwid;
} cloud_gw_info_req_t;

typedef struct {
    uint64_t deveui;
} cloud_mote_info_req_t;


typedef struct {
    uint64_t deveui;
    uint64_t gwid;
    uint32_t devaddr;
    uint8_t  fport;
    uint64_t tmst;
    char     time[100];
    uint32_t freq;
    char     datar[10];
    char     codr[5];
    float    lsnr;
    int16_t  rssi;
    uint32_t sequl;
    uint32_t seqdl;
    uint8_t  size;
    uint8_t  data[256];
} cloud_modu_ul_t;

#endif


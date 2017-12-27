#include "common.h"

uint16_t COMM_MSB_R_2(uint8_t *buf)
{
    return (buf[0]<<8)+buf[1];
}

void COMM_MSB_W_2(uint8_t *buf, uint16_t data)
{
    buf[0]=data>>8;
    buf[1]=data;
}

uint32_t COMM_MSB_R_4(uint8_t *buf)
{
    return (buf[0]<<24)+(buf[1]<<16)+(buf[2]<<8)+buf[3];
}

void COMM_MSB_W_4(uint8_t *buf, uint32_t data)
{
    buf[0]=data>>24;
    buf[1]=data>>16;
    buf[2]=data>>8;
    buf[3]=data;
}

uint16_t ntoh2(uint16_t d)
{
    return ntohs(d);
}

uint16_t hton2(uint16_t d)
{
    return htons(d);
}

uint32_t ntoh4(uint32_t d)
{
    return ntohl(d);
}

uint32_t hton4(uint32_t d)
{
    return htonl(d);
}

uint64_t ntoh8(uint64_t d)
{
    return (uint64_t)ntohl(d>>32)+((uint64_t)ntohl(d)<<32);
}

uint64_t hton8(uint64_t d)
{
    return (uint64_t)htonl(d>>32)+((uint64_t)htonl(d)<<32);
}

uint16_t r2bf2(uint8_t *buf)
{
    uint16_t tmp = 0;

    tmp += (uint16_t)buf[1]<<8;
    tmp += (uint16_t)buf[0];

    return tmp;
}

uint32_t r2bf4(uint8_t *buf)
{
    uint32_t tmp = 0;
    
    tmp += (uint32_t)r2bf2(buf+2)<<16;
    tmp += (uint32_t)r2bf2(buf);

    return tmp;
}

uint64_t r2bf8(uint8_t *buf)
{
    uint64_t tmp = 0;

    tmp += (uint64_t)r2bf4(buf+4)<<32;
    tmp += (uint64_t)r2bf4(buf);

    return tmp;
}

int hex2half(char c, uint8_t *byte)
{
    if (c>='A' && c<='F') {
        *byte=c-'A'+0xA;
    }
    else if (c>='a' && c<='f') {
        *byte=c-'a'+0xA;
    }
    else if (c>='0'&&c<='9') {
        *byte=c-'0';
    }
    else {
        return -1;
        }
    return 0;
}

int hex2byte(const char *str, uint8_t *byte)
{
    uint8_t tmp;
    if (hex2half(*str, &tmp)!=0) {
        return -1;
    }
    *byte=tmp<<4;
    if (hex2half(*(str+1), &tmp)!=0) {
        return -1;
    }
    *byte+=tmp;

    return 0;
}

int hexstr2bytes(uint8_t *byte, const char *str, uint16_t len)
{
    for (int i=0; i<len>>1; i++) {
        uint8_t tmp;
        if (hex2byte(&str[i<<1], &tmp)!=0) {
            return -1;
        }
        byte[i]=tmp;
    }

    return 0;
}
#include "gu.h"
#include "main.h"
#include "cloud.h"
#include "queue.h"
#include "hashmap.h"
#include "common.h"
#include "base64.h"
#include <stdarg.h>
#include <arpa/inet.h>

uint32_t gw_port=6001;

typedef struct {
    uint8_t version;
    uint8_t tokenh;
    uint8_t tokenl;
    uint8_t type;
} PACKED gw_dl_head_t;

#define GW_DL_MSG_HEAD_SIZE sizeof(gw_dl_head_t)

extern queue_t *lora_hanle_queue;
extern queue_t *gu_dl_queue;

static struct hashmap gw_dl_map;

static int sockfd=-1;


#define MODULE_NAME  "GU"

static size_t GU_DlMapHash(void *key)
{
    return *(uint64_t *)key;
}

static int GU_DlMapCmp(struct hash_node *node, void *key)
{
    uint64_t gwid=*(uint64_t *)key;

    return node->hash==(size_t)gwid;
}

int GU_CreateConnect(void)
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(gw_port);

    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    return sockfd;
}

gw_msg_json_t *GU_Recv(int sockfd)
{
    static char        recvline[4096];
    uint32_t           n, len;
    gw_msg_json_t      *gwmsg;
    struct sockaddr_in clientAddr;

    len = sizeof(clientAddr);

    memset(recvline, 0, sizeof(recvline));

    n = recvfrom(sockfd, recvline, 4096, 0, (struct sockaddr*)&clientAddr, &len);
    if (n<=0)
    {
        return NULL;
    }

    struct sockaddr_in sin;

    int i=0;
    uint8_t version=recvline[i++];
    gwmsg=(gw_msg_json_t *)malloc(sizeof(gw_msg_json_t));
    gwmsg->tokenh=recvline[i++];
    gwmsg->tokenl=recvline[i++];
    gwmsg->type=recvline[i++];
    gwmsg->version=version;
    gwmsg->gwid=ntoh8(r2bf8((uint8_t *)&recvline[i]));
    i+=8;

    memcpy(&gwmsg->gwaddr, &clientAddr, sizeof(struct sockaddr));
    gwmsg->msg=NULL;

    memcpy(&sin, &gwmsg->gwaddr, sizeof(sin));
    log(LOG_DEBUG, "recv from %s:%d %p gwid:%u", inet_ntoa(sin.sin_addr), sin.sin_port,   
    &gwmsg->gwaddr,  gwmsg->gwid);

    if (n>12) {
        cJSON *json=cJSON_Parse(&recvline[i]);

        if (json==NULL) {
            return NULL;
        }

        log(LOG_NORMAL, (const char *)&recvline[i]);
        gwmsg->msg=json;
    }

    return gwmsg;
}

int GU_ParseRxPkt(gw_pkt_rx_t *pkt, cJSON *pos)
{
    char *str;
    /* check whether message is complete */
    if (!cJSON_HasObjectItem(pos,"tmst") || !cJSON_HasObjectItem(pos,"time") || !cJSON_HasObjectItem(pos,"chan")
        || !cJSON_HasObjectItem(pos,"rfch") || !cJSON_HasObjectItem(pos,"freq") || !cJSON_HasObjectItem(pos,"stat")
        || !cJSON_HasObjectItem(pos,"modu") || !cJSON_HasObjectItem(pos,"datar") || !cJSON_HasObjectItem(pos,"codr")
        || !cJSON_HasObjectItem(pos,"lsnr") || !cJSON_HasObjectItem(pos,"rssi") || !cJSON_HasObjectItem(pos,"size")
        || !cJSON_HasObjectItem(pos,"data")) {
             log(LOG_ERR, "packet isn't complete");
            return -1;
    }

    pkt->tmst=cJSON_GetObjectItem(pos, "tmst")->valuedouble;

    str=cJSON_GetObjectItem(pos, "time")->valuestring;

    if (str==NULL || strlen(str)>=sizeof(pkt->time)) {
        log(LOG_ERR, "time isn't string or time string is too long");
        return -1;
    }

    memcpy(pkt->time, str, strlen(str));
    pkt->chan=cJSON_GetObjectItem(pos, "chan")->valueint;
    pkt->rfch=cJSON_GetObjectItem(pos, "rfch")->valueint;
    pkt->freq=cJSON_GetObjectItem(pos, "freq")->valueint;
    pkt->stat=cJSON_GetObjectItem(pos, "stat")->valueint;
    str=cJSON_GetObjectItem(pos, "modu")->valuestring;

    if (str==NULL) {
        log(LOG_ERR, "modu isn't string");
        return -1;
    }
    
    if (!strncmp(str, "LORA", 4)) {
        pkt->modu=MODU_LORA;
    }
    else if (!strncmp(str, "FSK", 3)) {
        pkt->modu=MODU_FSK;
    }
    else {
        log(LOG_ERR, "modu %s is error", str);
        return -1;
    }
    str=cJSON_GetObjectItem(pos, "datar")->valuestring;
    if (str==NULL || strlen(str)>=sizeof(pkt->datar)){
        log(LOG_ERR, "datar isn't string or datar string is too long");
        return -1;
    }
    memcpy(pkt->datar, str, strlen(str));
    str=cJSON_GetObjectItem(pos, "codr")->valuestring;
    if (str==NULL || strlen(str)>=sizeof(pkt->codr)){
        log(LOG_ERR, "codr isn't string or codr string is too long");
        return -1;
    }
    memcpy(pkt->codr, str, strlen(str));
    pkt->lsnr=cJSON_GetObjectItem(pos, "lsnr")->valuedouble;
    pkt->rssi=cJSON_GetObjectItem(pos, "rssi")->valueint;
    pkt->size=cJSON_GetObjectItem(pos, "size")->valueint;
    str=cJSON_GetObjectItem(pos, "data")->valuestring;
    if (str==NULL) {
        log(LOG_ERR, "data isn't string");
        return -1;
    }
    b64_to_bin(str, strlen(str), pkt->data, sizeof(pkt->data));
    log(LOG_NORMAL, "data packet is ok,data:");
    dump(pkt->data, pkt->size);
    return 0;
}

int GU_ParseGwStat(gw_stat_t *stat, cJSON *pos)
{
    char *str;
    /* check whether message is complete */
    if (    !cJSON_HasObjectItem(pos,"time") || !cJSON_HasObjectItem(pos,"rxnb") || !cJSON_HasObjectItem(pos,"rxok" )
        || !cJSON_HasObjectItem(pos,"rxfw") || !cJSON_HasObjectItem(pos,"ackr") || !cJSON_HasObjectItem(pos,"dwnb")
        || !cJSON_HasObjectItem(pos,"txnb")
      ) {
        return -1;
    }
    str=cJSON_GetObjectItem(pos, "time")->valuestring;
    if (strlen(str)>=sizeof(stat->time)) {
        return -1;
    }

    memcpy(stat->time, str, strlen(str));
    stat->rxnb  =cJSON_GetObjectItem(pos, "rxnb")->valueint;
    stat->rxok  =cJSON_GetObjectItem(pos, "rxok")->valueint;
    stat->rxfw  =cJSON_GetObjectItem(pos, "rxfw")->valueint;
    stat->ackr  =cJSON_GetObjectItem(pos, "ackr")->valuedouble;
    stat->dwnb =cJSON_GetObjectItem(pos, "dwnb")->valueint;
    stat->txnb  =cJSON_GetObjectItem(pos, "txnb")->valueint;

    if (!cJSON_HasObjectItem(pos,"lati") || !cJSON_HasObjectItem(pos,"long") || !cJSON_HasObjectItem(pos,"alti")) {
        stat->pos=NULL;
        return 0;
    }

    gw_pos_t *gp=(gw_pos_t *)malloc(sizeof(gw_pos_t));
    gp->lati=cJSON_GetObjectItem(pos, "lati")->valuedouble;
    gp->longi=cJSON_GetObjectItem(pos, "long")->valuedouble;
    gp->alti=cJSON_GetObjectItem(pos, "alti")->valueint;

    stat->pos=gp;

    return 0;
}

void GU_HandlePush(gw_msg_json_t *jmsg)
{
    struct sockaddr_in sin;
    gw_msg_t *msg;
    gw_dl_head_t *head=NULL;
    cloud_msg_ul_t *msg2c = NULL;
    gw_stat_t *stat=NULL;
    cJSON *jstat=NULL;
    cJSON *pkts=cJSON_GetObjectItem(jmsg->msg, "rxpkt");

    if (pkts!=NULL) {
        cJSON *pos;
        cJSON_ArrayForEach(pos, pkts)
        {
            msg=(gw_msg_t *)malloc(sizeof(gw_msg_t));
            msg->gwaddr=jmsg->gwaddr;
            msg->type=jmsg->type;
            msg->gwid=jmsg->gwid;
            gw_pkt_rx_t *pkt=(gw_pkt_rx_t *)malloc(sizeof(gw_pkt_rx_t));
            memset(pkt, 0, sizeof(gw_pkt_rx_t));
            if (GU_ParseRxPkt(pkt, pos) < 0) {
                free(pkt);
                free(msg);
                
                continue;
            }
            msg->msg=pkt;
            //dump(pkt, sizeof(gw_pkt_rx_t));
            /* push packet to loramac handle */
            log(LOG_NORMAL, "push message to lora_hanle_queue");
            queue_put(lora_hanle_queue, msg);
        }
    }
    else {
        log(LOG_NORMAL, "no rxpkt");
    }

    if (!cJSON_HasObjectItem(jmsg->msg, "stat")) {
        log(LOG_NORMAL, "no stat");
        goto pushacktogw;
    }
    
    stat=(gw_stat_t *)malloc(sizeof(gw_stat_t));
    jstat=cJSON_GetObjectItem(jmsg->msg, "stat");

    if (stat==NULL) {
        goto pushacktogw;
    }

    memset(stat, 0, sizeof(gw_stat_t));

    if (GU_ParseGwStat(stat, jstat)<0) {
        free(stat);
        goto pushacktogw;
    }

    /* push gw state to appserver */
    msg2c=(cloud_msg_ul_t *)malloc(sizeof(cloud_msg_ul_t));
    msg=(gw_msg_t *)malloc(sizeof(gw_msg_t));
    msg->gwaddr=jmsg->gwaddr;
    msg->type=jmsg->type;
    msg->gwid=jmsg->gwid;
    msg->msg=stat;
    msg2c->type=0;
    msg2c->msg=msg;

    queue_put(cloud_ul_queue, msg2c);

pushacktogw:
    head=(gw_dl_head_t *)malloc(sizeof(gw_dl_head_t));
    head->version=jmsg->version;
    head->type=2;
    head->tokenh=jmsg->tokenh;
    head->tokenl=jmsg->tokenl;
    /* send to gw */
    
     memcpy(&sin, &jmsg->gwaddr, sizeof(sin));
    log(LOG_NORMAL, "response no packet ack to addr %s:%d %p", inet_ntoa(sin.sin_addr), sin.sin_port, &jmsg->gwaddr);
    sendto(sockfd, head, GW_DL_MSG_HEAD_SIZE, 0, &jmsg->gwaddr, sizeof(struct sockaddr));
    free(head);
}

void GU_HandlePull(gw_msg_json_t *jmsg)
{
    struct hash_node *node=hashmap_get(&gw_dl_map, &jmsg->gwid);

    gw_dl_head_t *head=NULL;
    hash_data_t *hd=NULL;
    queue_t *q=NULL;
    cJSON *jresp=NULL;
    cJSON *jpkts=NULL;
    char *str=NULL;

    if (node==NULL) {
        log(LOG_DEBUG, "gw %llu pull packet failed, no queue", jmsg->gwid);
        goto pullacktogw;
    }
    
    hd=container_of(node, hash_data_t, node);
    q=(queue_t *)hd->data;

    if (queue_empty(q)) {
        log(LOG_DEBUG, "gw %llu pull packet failed, queue empty", jmsg->gwid);
        goto pullacktogw;
    }
    
    jresp=cJSON_CreateObject();
    jpkts=cJSON_CreateArray();

    while(!queue_empty(q)) {
        gw_msg_t *msg=NULL;
        queue_get(q, (void **)&msg);
        gw_pkt_tx_t *tp=(gw_pkt_tx_t *)msg->msg;

        log(LOG_DEBUG, "process tx pkt %p", tp);
        
        cJSON *obj=cJSON_CreateObject();
        
        cJSON_AddBoolToObject(obj,"imme",tp->imme);
        cJSON_AddNumberToObject(obj,"tmst",tp->tmst);
        cJSON_AddStringToObject(obj,"time",tp->time);
        cJSON_AddBoolToObject(obj,"ncrc",tp->ncrc);
        cJSON_AddNumberToObject(obj,"freq",tp->freq);
        cJSON_AddNumberToObject(obj,"rfch",tp->rfch);
        cJSON_AddNumberToObject(obj,"powe",tp->powe);
        cJSON_AddStringToObject(obj,"modu",tp->modu==MODU_FSK?"FSK":"LORA");
        cJSON_AddStringToObject(obj,"datar",tp->datar);
        cJSON_AddStringToObject(obj,"codr",tp->codr);
        cJSON_AddNumberToObject(obj,"fdev",tp->fdev);
        cJSON_AddBoolToObject(obj,"ipol",tp->ipol);
        cJSON_AddNumberToObject(obj,"prea",tp->prea);
        cJSON_AddNumberToObject(obj,"size",tp->size);
        
        char data[512]={0};
        bin_to_b64(tp->data, tp->size, data, sizeof(data));
        cJSON_AddStringToObject(obj,"data",data);
        cJSON_AddItemToArray(jpkts, obj);
        
        free(msg->msg);
        free(msg);
    }

    cJSON_AddItemToObject(jresp,"txpkt",jpkts);

    str= cJSON_Print(jresp);

    log(LOG_DEBUG, "tx pkt: \n%s", str);

    if (str != NULL) {
        //log(LOG_NORMAL, "send packet to gw %d:\n %s", jmsg->gwid, str);
        
        head=(gw_dl_head_t *)malloc(GW_DL_MSG_HEAD_SIZE+strlen(str));
        head->version=jmsg->version;
        head->tokenh=rand()&0xff;
        head->tokenl=rand()&0xff;
        head->type=3;
        memcpy((uint8_t*)head+GW_DL_MSG_HEAD_SIZE, str, strlen(str));

        /* send to gw */
        struct sockaddr_in sin;
        memcpy(&sin, &jmsg->gwaddr, sizeof(sin));

        log(LOG_DEBUG, "send to %s:%d", inet_ntoa(sin.sin_addr), sin.sin_port);

        sendto(sockfd, head, GW_DL_MSG_HEAD_SIZE+strlen(str), 0, &jmsg->gwaddr, sizeof(struct sockaddr));
    }
    else {
        log(LOG_ERR, "can't print json data");
        cJSON_Delete(jresp);
        goto pullacktogw;
    }

    free(head);
    cJSON_Delete(jresp);

    return;

pullacktogw:
    head=(gw_dl_head_t *)malloc(sizeof(gw_dl_head_t));
    head->version=jmsg->version;
    head->type=4;
    head->tokenh=jmsg->tokenh;
    head->tokenl=jmsg->tokenl;
    /* send to gw */
    struct sockaddr_in sin;
       memcpy(&sin, &jmsg->gwaddr, sizeof(sin));
    log(LOG_DEBUG, "response no packet ack to addr %s:%d %p", inet_ntoa(sin.sin_addr), sin.sin_port, &jmsg->gwaddr);
    sendto(sockfd, head, GW_DL_MSG_HEAD_SIZE, 0, &jmsg->gwaddr, sizeof(struct sockaddr));
    free(head);
}

void *GU_ThreadUl(void *arg)
{
    arg=arg;
    sockfd=GU_CreateConnect();

    while(true) {
        gw_msg_json_t *json=GU_Recv(sockfd);

        if (json==NULL) {
            usleep(100);
            continue;
        }

        switch(json->type) {
            case 0:
                GU_HandlePush(json);
                break;
            case 2:
                GU_HandlePull(json);
                break;
            default:
                log(LOG_ERR, "Error request type %d\r\n", json->type);
                break;
        }

        if (json->msg!=NULL)
            cJSON_Delete(json->msg);
        free(json);
        usleep(1);
    }

    return NULL;
}

void *GU_ThreadDl(void *arg)
{
    arg=arg;
    hashmap_init(&gw_dl_map,GU_DlMapHash,GU_DlMapCmp);
    
    while (true) {
        if (queue_empty(gu_dl_queue)) {
            usleep(10);
            continue;
        }
        
        gw_msg_t *msg=NULL;
        queue_get(gu_dl_queue, (void **)&msg);
        
        queue_t *dlqueue=NULL;
        if (hashmap_get(&gw_dl_map, &msg->gwid)==NULL) {
            dlqueue=queue_create();
            hash_data_t *hd=(hash_data_t *)malloc(sizeof(hash_data_t));
            memset(hd, 0, sizeof(hash_data_t));
            hd->data=dlqueue;
            hashmap_insert(&gw_dl_map, &hd->node, &msg->gwid);
        }
        else {
            struct hash_node *node=hashmap_get(&gw_dl_map, &msg->gwid);
            hash_data_t *hd=container_of(node,hash_data_t,node);
            dlqueue=(queue_t*)hd->data;
        }
        
        log(LOG_NORMAL, "save gw %llu donwlink packet to queue", msg->gwid);
        queue_put(dlqueue, msg);
        usleep(1);
    }

    return NULL;
}


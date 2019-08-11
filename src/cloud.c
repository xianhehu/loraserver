#include "cloud.h"
#include "mp.h"
#include "main.h"
#include "common.h"
#include "queue.h"
#include "cJSON.h"

typedef struct {
    uint8_t  start[5];
    uint8_t  version;
    uint64_t id;
    uint16_t type;
    uint16_t len;
} PACKED cloud_head_t;

typedef struct {
    cloud_head_t head;
    cJSON *msg;
} cloud_msg_t;

uint32_t cloud_port   = 5000;
char     cloud_ip[50] = "192.168.199.192";

#define CLOUD_MSG_HEAD_LEN          sizeof(cloud_head_t)

static int sockfd = -1;

int CLOUD_Connect(void)
{
    int cPort = cloud_port;
    int cClient = 0;
    struct sockaddr_in cli;
    
    cli.sin_family = AF_INET;
    cli.sin_port = htons(cPort);
    cli.sin_addr.s_addr = inet_addr(cloud_ip);
    
    cClient = socket(AF_INET, SOCK_STREAM, 0);
    if(cClient < 0)
    {
        log(LOG_ERR, "socket() failure");
        return -1; 
    }
 
    if(connect(cClient, (struct sockaddr*)&cli, sizeof(cli)) < 0)
    {
        log(LOG_ERR, "connect() failure");
        return -1;
    }

    return cClient;
}

cloud_msg_t *CLOUD_Recv(int sockfd)
{
    uint8_t buf[4096];

    int32_t len=recv(sockfd, buf, sizeof(buf),0);

    if (len<=0) {
        return NULL;
    }
    
    if (len<=(int)CLOUD_MSG_HEAD_LEN) {
        log(LOG_ERR, "length is wrong");
        return NULL;
    }
    cloud_head_t *h=(cloud_head_t *)buf;

    h->len=ntoh2(h->len);
    h->type=ntoh2(h->type);
    h->id=ntoh8(h->id);

    if (h->len != len-CLOUD_MSG_HEAD_LEN) {
        log(LOG_ERR, "length is wrong");
        return NULL;
    }
    
    cJSON *msg=cJSON_Parse((char *)buf+CLOUD_MSG_HEAD_LEN);
    
    if (msg==NULL) {
        log(LOG_ERR, "json data is error");
        return NULL;
    }
    
    cloud_msg_t *d=(cloud_msg_t *)malloc(sizeof(cloud_msg_t));
    d->head=*h;
    d->msg=msg;

    log(LOG_NORMAL, (const char *)buf+CLOUD_MSG_HEAD_LEN);
    
    return d;
}

bool CLOUD_Send(int s, uint8_t *buf, uint32_t len)
{
    int ret=-1;
    ret=send(s, buf, len, 0);
    log(LOG_DEBUG, "send result:%d", ret);
    if (ret<(int)len) { /* MSG_NOSIGNAL */
        pthread_mutex_lock(&lock);
        sockfd=-1;
        pthread_mutex_unlock(&lock);
        log(LOG_ERR, "send failed, connect break");
        return false;
    }

    return true;
}

void CLOUD_HandleModuInfoResp(cloud_msg_t *d)
{
    cJSON *info=NULL;
    if (cJSON_HasObjectItem(d->msg,"MoteInfoResp")) {
        info=cJSON_GetObjectItem(d->msg, "MoteInfoResp");
    }
    if (info==NULL) {
        log(LOG_ERR, "json have no mote info");
        return;
    }
    /* check wether information is complete */
    if (!cJSON_HasObjectItem(info, "DevEUI") || !cJSON_HasObjectItem(info, "AppEUI") || !cJSON_HasObjectItem(info, "AppKey")
        || !cJSON_HasObjectItem(info, "LoRaMode") || !cJSON_HasObjectItem(info, "MacMajorVersion") || !cJSON_HasObjectItem(info, "RXDelay1")
        || !cJSON_HasObjectItem(info, "RXDROffset1") || !cJSON_HasObjectItem(info, "RXDataRate2") || !cJSON_HasObjectItem(info, "RXFreq2")
        || !cJSON_HasObjectItem(info, "MaxDutyCycle") || !cJSON_HasObjectItem(info, "ActivationMode") || !cJSON_HasObjectItem(info, "FreqType")
        || !cJSON_HasObjectItem(info, "FreqPair")) {
        log(LOG_ERR, "mote info isn't complete");
        return;
    }

    cloud_modu_param_t *cmp=(cloud_modu_param_t *)malloc(sizeof(cloud_modu_param_t));
    memset(cmp, 0, sizeof(cloud_modu_param_t));
    cmp->deveui=cJSON_GetObjectItem(info, "DevEUI")->valuedouble;
    cmp->param.appeui=cJSON_GetObjectItem(info, "AppEUI")->valueint;
    char *str=cJSON_GetObjectItem(info, "AppKey")->valuestring;
    
    if (hexstr2bytes(cmp->param.appkey, str, 32)<0) {
        free(cmp);
        log(LOG_ERR, "appkey long is wrong");
        return;
    }

    cmp->param.lclass         = cJSON_GetObjectItem(info, "LoRaMode")->valueint;
    cmp->param.rxdelay      = cJSON_GetObjectItem(info, "RXDelay1")->valueint;
    cmp->param.rx1droffset = cJSON_GetObjectItem(info, "RXDROffset1")->valueint;
    cmp->param.rx2dr         = cJSON_GetObjectItem(info, "RXDataRate2")->valueint;
    cmp->param.rx2freq      = cJSON_GetObjectItem(info, "RXFreq2")->valueint;

    cmp->param.version      = cJSON_GetObjectItem(info, "MacMajorVersion")->valueint;
    cmp->param.dutycycle   = cJSON_GetObjectItem(info, "MaxDutyCycle")->valueint;
    cmp->param.freqtype    = cJSON_GetObjectItem(info, "FreqType")->valueint;

    cJSON *freqs                = cJSON_GetObjectItem(info, "FreqPair");
    cJSON *pos;
    cJSON_ArrayForEach(pos,freqs) {
        if (cmp->param.freqnum>=8) {
            break;
        }

        cmp->param.freq[cmp->param.freqnum]        = cJSON_GetObjectItem(pos, "UlFreq")->valueint;
        cmp->param.dlfreq[cmp->param.freqnum++] = cJSON_GetObjectItem(pos, "DlFreq")->valueint;
    }

    /* push to mp */
    mp_msg_t *m = (mp_msg_t *)malloc(sizeof(mp_msg_t));
    m->type         = MP_MSG_TYPE_MODINFO;
    m->msg         = cmp;
    log(LOG_NORMAL, "push msg to mp");
    queue_put(mp_dl_queue, m);
}

void CLOUD_HandleModuInfoPush(int s, cloud_msg_t *d)
{
    cJSON *info=NULL;

    uint8_t txbuf[1024]={0};

    if (cJSON_HasObjectItem(d->msg,"MoteInfoPush")) {
        info=cJSON_GetObjectItem(d->msg, "MoteInfoPush");
    }

    if (info==NULL) {
        log(LOG_ERR, "json have no mote info");
        
        return;
    }

    /* check wether information is complete */
    if (    !cJSON_HasObjectItem(info, "DevEUI"          ) || !cJSON_HasObjectItem(info, "AppEUI"             ) || !cJSON_HasObjectItem(info, "AppKey"    )
        || !cJSON_HasObjectItem(info, "LoRaMode"      ) || !cJSON_HasObjectItem(info, "MacMajorVersion") || !cJSON_HasObjectItem(info, "RXDelay1" )
        || !cJSON_HasObjectItem(info, "RXDROffset1"  ) || !cJSON_HasObjectItem(info, "RXDataRate2"     ) || !cJSON_HasObjectItem(info, "RXFreq2"   )
        || !cJSON_HasObjectItem(info, "MaxDutyCycle") || !cJSON_HasObjectItem(info, "ActivationMode"   ) || !cJSON_HasObjectItem(info, "FreqType" )
        || !cJSON_HasObjectItem(info, "FreqPair"         )
     ) 
    {
        log(LOG_ERR, "mote info isn't complete");
        
        return;
    }

    cJSON *resp=cJSON_CreateObject();
    cJSON *obj=cJSON_CreateObject();
    
    cloud_modu_param_t *cmp=(cloud_modu_param_t *)malloc(sizeof(cloud_modu_param_t));
    
    memset(cmp, 0, sizeof(cloud_modu_param_t));
    
    cmp->deveui           = cJSON_GetObjectItem(info, "DevEUI")->valuedouble;
    cmp->param.appeui = cJSON_GetObjectItem(info, "AppEUI")->valueint;
    
    char *str=cJSON_GetObjectItem(info, "AppKey")->valuestring;
    
    if (hexstr2bytes(cmp->param.appkey, str, 32)<0) {
        free(cmp);
        log(LOG_ERR, "appkey long is wrong");
        return;
    }

    memset(&cmp->param, 0, sizeof(cmp->param));

    cmp->param.rx2dr         = cJSON_GetObjectItem(info, "RXDataRate2"     )->valueint;
    cmp->param.lclass         = cJSON_GetObjectItem(info, "LoRaMode"          )->valueint;
    cmp->param.rxdelay      = cJSON_GetObjectItem(info, "RXDelay1"          )->valueint;
    cmp->param.rx2freq      = cJSON_GetObjectItem(info, "RXFreq2"            )->valueint;
    cmp->param.version      = cJSON_GetObjectItem(info, "MacMajorVersion")->valueint;
    cmp->param.dutycycle   = cJSON_GetObjectItem(info, "MaxDutyCycle"    )->valueint;
    cmp->param.rx1droffset = cJSON_GetObjectItem(info, "RXDROffset1"     )->valueint;
    cmp->param.freqtype     = cJSON_GetObjectItem(info, "FreqType"          )->valueint;

    str = cJSON_GetObjectItem(info, "FreqPair")->valuestring;
    printf(str);
    cJSON *freqs = cJSON_Parse(str);
    cJSON *pos;

    printf("freqlist handle\n");

    cJSON_ArrayForEach(pos, freqs)  {
        if (pos==NULL || cmp->param.freqnum>=8) {
            break;
        }

        if (cmp->param.freqtype == FREQ_TYPE_CUSTOM) {
            cmp->param.freq[cmp->param.freqnum]        = cJSON_GetObjectItem(pos, "UlFreq")->valueint;
            cmp->param.dlfreq[cmp->param.freqnum++] = cJSON_GetObjectItem(pos, "DlFreq")->valueint;
            printf("freqlist %u:%u\n", cmp->param.cfnum-1, pos->valueint);
        }
        else {
            cmp->param.cflist[cmp->param.cfnum++]      = pos->valueint;
            printf("cflist %u:%u\n", cmp->param.cfnum-1, pos->valueint);
        }
    }

    /* push to mp */
    mp_msg_t *m = (mp_msg_t *)malloc(sizeof(mp_msg_t));
    m->type         = MP_MSG_TYPE_MODINFO;
    m->msg         = cmp;
    log(LOG_NORMAL, "push msg to mp");
    queue_put(mp_dl_queue, m);

    /* response to cloud */
    cJSON_AddNumberToObject(obj  , "DevEUI"             , cmp->deveui);
    cJSON_AddNumberToObject(obj  , "ACK"                  , 0                 );
    cJSON_AddItemToObject     (resp, "MoteInfoPushAck", obj              );
    str=cJSON_Print(resp);
    cloud_msg_t *ack=(cloud_msg_t *)txbuf;
    memcpy(ack, d, CLOUD_MSG_HEAD_LEN);
    ack->head.id     = hton8(ack->head.id);
    ack->head.type = hton2(11);
    ack->head.len   = hton2(strlen(str));
    memcpy(txbuf+CLOUD_MSG_HEAD_LEN, str, strlen(str));
    /* send to cloud */
    CLOUD_Send(s, txbuf, CLOUD_MSG_HEAD_LEN+strlen(str));
    /* free buffer */
    cJSON_Delete(resp);
}


void CLOUD_HandleGwInfoResp(cloud_msg_t *d)
{
    cJSON *info=NULL;
    
    if (cJSON_HasObjectItem(d->msg,"GwInfoResp")) {
        info=cJSON_GetObjectItem(d->msg, "GwInfoResp");
    }
    
    if (info==NULL) {
        log(LOG_ERR, "json have no gw info");
        return;
    }
    
    /* check wether information is complete */
    if (!cJSON_HasObjectItem(info, "GwID") || !cJSON_HasObjectItem(info, "TxPower")) {
        log(LOG_ERR, "gw info isn't complete");
        return;
    }

    cloud_gw_param_t *cgp=(cloud_gw_param_t *)malloc(sizeof(cloud_gw_param_t));
    cgp->gwid=cJSON_GetObjectItem(info, "GwID")->valuedouble;
    cgp->param.powe=cJSON_GetObjectItem(info, "TxPower")->valueint;

    /* push to mp */
    mp_msg_t *m=(mp_msg_t *)malloc(sizeof(mp_msg_t));
    m->type=MP_MSG_TYPE_GWINFO;
    m->msg=cgp;
    log(LOG_NORMAL, "push gateway info to mp");
    queue_put(mp_dl_queue, m);
}

void CLOUD_HandleGwInfoPush(int s, cloud_msg_t *d)
{
    cJSON *info=NULL;
    
    uint8_t txbuf[1024]={0};

    if (cJSON_HasObjectItem(d->msg,"GwInfoPush")) {
        info=cJSON_GetObjectItem(d->msg, "GwInfoPush");
    }
    
    if (info==NULL) {
        log(LOG_ERR, "json have no gw info");
        return;
    }
    
    /* check wether information is complete */
    if (!cJSON_HasObjectItem(info, "GwID") || !cJSON_HasObjectItem(info, "TxPower")) {
        log(LOG_ERR, "gw info isn't complete");
        return;
    }

    cloud_gw_param_t *cgp=(cloud_gw_param_t *)malloc(sizeof(cloud_gw_param_t));
    cgp->gwid=cJSON_GetObjectItem(info, "GwID")->valuedouble;
    cgp->param.powe=cJSON_GetObjectItem(info, "TxPower")->valueint;

    /* push to mp */
    mp_msg_t *m=(mp_msg_t *)malloc(sizeof(mp_msg_t));
    m->type=MP_MSG_TYPE_GWINFO;
    m->msg=cgp;
    log(LOG_NORMAL, "push gateway info to mp");
    queue_put(mp_dl_queue, m);

    /* response to cloud */
    cJSON *resp=cJSON_CreateObject();
    cJSON *obj=cJSON_CreateObject();
    
    cJSON_AddNumberToObject(obj, "GwID",cgp->gwid);
    cJSON_AddNumberToObject(obj, "ACK" ,0);
    cJSON_AddItemToObject(resp,  "GwInfoPushAck", obj);
    char *str=cJSON_Print(resp);
    cloud_msg_t *ack=(cloud_msg_t *)txbuf;
    memcpy(ack, d, CLOUD_MSG_HEAD_LEN);
    ack->head.id     = hton8(ack->head.id);
    ack->head.type=hton2(13);
    ack->head.len=hton2(strlen(str));
    memcpy(txbuf+CLOUD_MSG_HEAD_LEN, str, strlen(str));
    /* send to cloud */
    CLOUD_Send(s, txbuf, CLOUD_MSG_HEAD_LEN+strlen(str));
    /* free buffer */
    cJSON_Delete(resp);
}


void CLOUD_HandleModuData(cloud_msg_t *d)
{
    cJSON *jd=NULL;
    if (cJSON_HasObjectItem(d->msg,"MoteDLData")) {
        jd=cJSON_GetObjectItem(d->msg, "MoteDLData");
    }

    if (jd==NULL) {
        log(LOG_ERR, "json have no mote data");
        return;
    }

    /* check wether information is complete */
    if (!cJSON_HasObjectItem(jd, "DevEUI") || !cJSON_HasObjectItem(jd, "FPort") || !cJSON_HasObjectItem(jd, "DataLen")
        || !cJSON_HasObjectItem(jd, "AppData")) {
        log(LOG_ERR, "mote data isn't complete");
        return;
    }

    log(LOG_DEBUG, (const char *)cJSON_Print(jd));
    
    cloud_modu_dl_t *dl=(cloud_modu_dl_t *)malloc(sizeof(cloud_modu_dl_t));
    memset(dl, 0, sizeof(cloud_modu_dl_t));
    dl->deveui=cJSON_GetObjectItem(jd, "DevEUI")->valuedouble;
    dl->dldata.port=cJSON_GetObjectItem(jd, "FPort")->valueint;
    dl->dldata.size=cJSON_GetObjectItem(jd, "DataLen")->valueint;
    char *str=cJSON_GetObjectItem(jd, "AppData")->valuestring;

    log(LOG_DEBUG, "deveui:%llu", dl->deveui);

    uint16_t slen=strlen(str);
    if (slen>460 || (slen&1)!=0) {
        free(dl);
        log(LOG_ERR, "data long is wrong");
        return;
    }
    if (hexstr2bytes(dl->dldata.data, str, slen)!=0) {
        free(dl);
        log(LOG_ERR, "data string is wrong");
        return;
    }

    /* push to mp */
    mp_msg_t *m=(mp_msg_t *)malloc(sizeof(mp_msg_t));
    m->type=MP_MSG_TYPE_MODDATA;
    m->msg=dl;
    log(LOG_NORMAL, "push msg to mp");
    queue_put(mp_dl_queue, m);
}


void CLOUD_HandleDlMsg(int s, cloud_msg_t * d)
{
    /* dispatch msg */
    switch(d->head.type) {
        case 2:
            CLOUD_HandleModuData(d);
            break;
		case 5:
			log(LOG_DEBUG, "connect appserver success!");
			break;
        case 7:
            CLOUD_HandleModuInfoResp(d);
            break;
        case 9:
            CLOUD_HandleGwInfoResp(d);
            break;
        case 10:
            CLOUD_HandleModuInfoPush(s, d);
            break;
        case 12:
            CLOUD_HandleGwInfoPush(s, d);
            break;
        default:
            log(LOG_ERR, "unknown type %d", d->head.type);
            break;
    }
}

void CLOUD_UpdateLink(int s)
{
    cJSON *ulmsg=cJSON_CreateObject();
    cJSON *data=cJSON_CreateObject();

    cJSON_AddNumberToObject(data,"NwkLink",0);
    cJSON_AddItemToObject(ulmsg,"LinkUpdate",data);

    char *str=cJSON_Print(ulmsg);
    cloud_head_t *msg2c=(cloud_head_t *)malloc(4096);
    memset(msg2c, 0, 4096);
    memcpy(msg2c->start, "start", 5);
    msg2c->id=hton8(1);
    msg2c->type=hton2(4);

    uint16_t len=strlen(str);
    
    msg2c->len=hton2(len);
    memcpy((uint8_t *)msg2c+CLOUD_MSG_HEAD_LEN, str, len);

    CLOUD_Send(s, (uint8_t *)msg2c, len+CLOUD_MSG_HEAD_LEN);

    dump((uint8_t *)msg2c, len+CLOUD_MSG_HEAD_LEN);

    log(LOG_NORMAL, "send to cloud:");
    log(LOG_NORMAL, (const char *)str);

    cJSON_Delete(ulmsg);
}

void *CLOUD_DlTask(void *arg)
{
    int s=CLOUD_Connect();

    arg = arg;
    
    if (s<=0) {
        log(LOG_ERR, "can't connect cloud!");
        abort();
    }

    CLOUD_UpdateLink(s);

    pthread_mutex_init(&lock, NULL);
    
    pthread_mutex_lock(&lock);
    sockfd=s;
    pthread_mutex_unlock(&lock);
    
    while(true) {
        pthread_mutex_lock(&lock);
        s=sockfd;
        pthread_mutex_unlock(&lock);

        if (s<=0) {
            while (s<=0) {
                s=CLOUD_Connect();
                if (s<=0)
                    usleep(1000000);
                else
                    CLOUD_UpdateLink(s);
            }
            
            pthread_mutex_lock(&lock);
            sockfd=s;
            pthread_mutex_unlock(&lock);
        }

        cloud_msg_t *msg=CLOUD_Recv(s);
        if (msg==NULL) {
            usleep(100);
            continue;
        }
        CLOUD_HandleDlMsg(s, msg);
        cJSON_Delete(msg->msg);
        free(msg);
        usleep(1);
    }

    return NULL;
}

void CLOUD_HandleGwStat(int s, gw_msg_t *msg)
{
    cJSON *ulmsg=cJSON_CreateObject();
    cJSON *state=cJSON_CreateObject();
    gw_stat_t *gs=(gw_stat_t *)msg->msg;
    cJSON_AddNumberToObject(state, "GwID",msg->gwid);
    cJSON_AddStringToObject   (state, "Time",  gs->time);
    cJSON_AddNumberToObject(state, "Lati",    gs->pos->lati);
    cJSON_AddNumberToObject(state, "Long",  gs->pos->longi);
    cJSON_AddNumberToObject(state, "Alti",    gs->pos->alti);
    cJSON_AddNumberToObject(state, "RxNb", gs->rxnb);
    cJSON_AddNumberToObject(state, "RxOK", gs->rxok);
    cJSON_AddNumberToObject(state, "RxFw", gs->rxfw);
    cJSON_AddNumberToObject(state, "AckR", gs->ackr);
    cJSON_AddNumberToObject(state, "DwNb", gs->dwnb);
    cJSON_AddNumberToObject(state, "TxNb", gs->txnb);

    cJSON_AddItemToObject(ulmsg, "GWDemoStat", state);

    char *str=cJSON_Print(ulmsg);

    cloud_head_t *msghead=(cloud_head_t *)malloc(4096);
    memset(msghead, 0, 4096);
    msghead->id=hton8(msg->gwid);
    memcpy(msghead->start, "start", 5);
    msghead->type=hton2(3);
    uint16_t len=strlen(str);
    msghead->len=hton2(len);
    memcpy((uint8_t *)msghead+CLOUD_MSG_HEAD_LEN, str, len);

    CLOUD_Send(s, (uint8_t *)msghead, len+CLOUD_MSG_HEAD_LEN);

    log(LOG_NORMAL, "send to cloud");
    log(LOG_NORMAL, (const char *)str);

    cJSON_Delete(ulmsg);
    free(msghead);
    free(gs);
}

void CLOUD_HandleMoteUlData(int s, cloud_modu_ul_t *msg)
{
    cJSON *ulmsg=cJSON_CreateObject();
    cJSON *data=cJSON_CreateObject();

    cJSON_AddNumberToObject(data,"DevEUI",msg->deveui);
    cJSON_AddNumberToObject(data,"GwID",msg->gwid);
    cJSON_AddNumberToObject(data,"DevAddr",msg->devaddr);
    cJSON_AddNumberToObject(data,"FPort",msg->fport);
    cJSON_AddNumberToObject(data,"FcntDown",msg->seqdl);
    cJSON_AddNumberToObject(data,"FcntUp",msg->sequl);
    cJSON_AddStringToObject(data,"DataRate",msg->datar);
    cJSON_AddStringToObject(data,"CodeRate",msg->codr);
    cJSON_AddNumberToObject(data,"ULFreq",msg->freq);
    cJSON_AddStringToObject(data,"RecvTime",msg->time);
    cJSON_AddNumberToObject(data,"Tmst",msg->tmst);
    cJSON_AddNumberToObject(data,"RSSI",msg->rssi);
    cJSON_AddNumberToObject(data,"SNR",msg->lsnr);
    cJSON_AddNumberToObject(data,"DataLen",msg->size);

    char appdata[512]={0};

    dump(msg->data, msg->size);

    int tmplen=0;
    for (int i=0; i<msg->size; i++) {
        tmplen+=snprintf(&appdata[tmplen], sizeof(appdata)-tmplen-2, "%02X", msg->data[i]);
    }

    //log(LOG_DEBUG, "appdata:%s", appdata);
    
    cJSON_AddStringToObject(data,"AppData",appdata);

    cJSON_AddItemToObject(ulmsg, "MoteULData", data);

    char *str=cJSON_Print(ulmsg);
    cloud_head_t *msg2c=(cloud_head_t *)malloc(4096);
    memset(msg2c, 0, 4096);
    msg2c->id=hton8(1);
    memcpy(msg2c->start, "start", 5);
    msg2c->type=hton2(1);
    uint16_t len=strlen(str);
    msg2c->len=hton2(len);
    memcpy((uint8_t *)msg2c+CLOUD_MSG_HEAD_LEN, str, len);

    CLOUD_Send(s, (uint8_t *)msg2c, len+CLOUD_MSG_HEAD_LEN);

    log(LOG_NORMAL, "send to cloud:");
    log(LOG_NORMAL, (const char *)str);

    cJSON_Delete(ulmsg);
    free(msg2c);
}


void CLOUD_HandleGwInfoReq(int s, cloud_gw_info_req_t *req)
{
    cJSON *ulmsg=cJSON_CreateObject();
    cJSON *data=cJSON_CreateObject();

    cJSON_AddNumberToObject(data,"GwID",req->gwid);
    cJSON_AddItemToObject(ulmsg,"GwInfoReq",data);

    char *str=cJSON_Print(ulmsg);
    cloud_head_t *msg2c=(cloud_head_t *)malloc(4096);
    memset(msg2c, 0, 4096);
    msg2c->id=hton8(1);
    memcpy(msg2c->start, "start", 5);
    msg2c->type=hton2(8);

    uint16_t len=strlen(str);
    msg2c->len=hton2(len);
    memcpy((uint8_t *)msg2c+CLOUD_MSG_HEAD_LEN, str, len);

    CLOUD_Send(s, (uint8_t *)msg2c, len+CLOUD_MSG_HEAD_LEN);

    log(LOG_NORMAL, "send to cloud:");
    log(LOG_NORMAL, (const char *)str);

    cJSON_Delete(ulmsg);
    free(msg2c);
}


void CLOUD_HandleMoteInfoReq(int s, cloud_mote_info_req_t *req)
{
    cJSON *ulmsg=cJSON_CreateObject();
    cJSON *data=cJSON_CreateObject();

    cJSON_AddNumberToObject(data,"DevEUI",req->deveui);
    cJSON_AddItemToObject(ulmsg,"MoteInfoReq",data);

    char *str=cJSON_Print(ulmsg);
    cloud_head_t *msg2c=(cloud_head_t *)malloc(4096);
    memset(msg2c, 0, 4096);
    msg2c->id=hton8((uint64_t)1);
    memcpy(msg2c->start, "start", 5);
    msg2c->type=hton2(6);

    uint16_t len=strlen(str);
    msg2c->len=hton2(len);
    memcpy((uint8_t *)msg2c+CLOUD_MSG_HEAD_LEN, str, len);

    CLOUD_Send(s, (uint8_t *)msg2c, len+CLOUD_MSG_HEAD_LEN);

    log(LOG_NORMAL, "send to cloud:");
    log(LOG_NORMAL, (const char *)str);

    cJSON_Delete(ulmsg);
    free(msg2c);
}

void *CLOUD_HandleUlMsg(int s, cloud_msg_ul_t *msg)
{
    log(LOG_DEBUG, "handle %d message %p", msg->type, msg);
    switch(msg->type) {
        case 0:
            CLOUD_HandleGwStat(s, (gw_msg_t *)msg->msg);
            break;
        case 1:
            CLOUD_HandleMoteUlData(s, (cloud_modu_ul_t *)msg->msg);
            break;
        case 2:
            CLOUD_HandleGwInfoReq(s, (cloud_gw_info_req_t *)msg->msg);
            break;
        case 3:
            CLOUD_HandleMoteInfoReq(s, (cloud_mote_info_req_t *)msg->msg);
            break;
		case 5:
			log(LOG_DEBUG, "connect appserver success!");
			break;
        default:
            log(LOG_ERR, "unknow type %d", msg->type);
            break;
    }
}

void *CLOUD_UlTask(void *arg)
{   
    arg = arg;
    
    while(true) {
        if (queue_empty(cloud_ul_queue)) {
            usleep(1000);
            continue;
        }

        cloud_msg_ul_t *msg=NULL;
        queue_get(cloud_ul_queue, (void **)&msg);
        
        pthread_mutex_lock(&lock);
        int s=sockfd;
        pthread_mutex_unlock(&lock);

        if (s<=0) {
            free(msg->msg);
            free(msg);
            log(LOG_NORMAL, "wait connect to cloud");
            usleep(1000);
            continue;
        }

        /* process message from mp&gu */
        CLOUD_HandleUlMsg(s, msg);

        free(msg->msg);
        free(msg);
    }

    return NULL;
}


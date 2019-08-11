#include "hashmap.h"
#include "queue.h"
#include "LoRaMacCrypto.h"
#include "gu.h"
#include "cloud.h"
#include "main.h"
#include "common.h"
#include "sql.h"

struct hashmap devaddr_deveui_map;
struct hashmap deveui_param_map;            /* mac parameter */
struct hashmap deveui_data_dl;             /* save downlink data */
struct hashmap deveui_data_ul;
struct hashmap deveui_seq_map;

struct hashmap gwid_param_map;

static SQL_CTX db;

typedef union
{
    uint8_t Value;
    struct
    {
        uint8_t FOptsLen        : 4;
        uint8_t FPending        : 1;
        uint8_t Ack             : 1;
        uint8_t AdrAckReq       : 1;
        uint8_t Adr             : 1;
    }PACKED Bits;
} PACKED LoRaMacFrameCtrl_t;

typedef struct {
    bool    confirm;
    uint8_t port;
    uint8_t size;
    uint8_t data[256];
    uint8_t msize;
    uint8_t mac[16];
    LoRaMacFrameCtrl_t fctrl;
} mac_resp_t;

typedef struct {
    uint8_t type;
    uint8_t size;
    uint8_t *data;
} cloud_req_t;

typedef struct {
    uint8_t type;
#define CLOUD_DL_TYPE_CONFIRM   1
#define CLOUD_DL_TYPE_UNCONFIRM 0
    uint8_t size;
    uint8_t data[256];
    uint8_t port;
    uint8_t state; /* 0:ready 1::wait ack */
} cloud_ul_t;

typedef struct {
    uint32_t sequl;
    uint32_t seqdl;
} mote_seq_pre_t;


uint32_t devaddr_max=1;

void sendMoteInfoReq(uint64_t deveui);



static size_t Devaddr_DevEui_Hash(void *key)
{
    return *(uint32_t *)key;
}

static int Devaddr_DevEui_Cmp(struct hash_node *node, void *key)
{
    uint32_t devaddr=*(uint32_t *)key;

    return node->hash==(size_t)devaddr;
}

static size_t DevEui_Param_Hash(void *key)
{
    return *(uint64_t *)key;
}

static int DevEui_Param_Cmp(struct hash_node *node, void *key)
{
    uint64_t tmp=*(uint64_t *)key;

    return node->hash==(size_t)tmp;
}

static size_t DevEui_Data_Hash(void *key)
{
    return *(uint64_t *)key;
}

static int DevEui_Data_Cmp(struct hash_node *node, void *key)
{
    uint64_t tmp=*(uint64_t *)key;

    return node->hash==(size_t)tmp;
}

static size_t GwId_Param_Hash(void *key)
{
    return *(uint64_t *)key;
}

static int GwId_Param_Cmp(struct hash_node *node, void *key)
{
    uint64_t tmp=*(uint64_t *)key;

    return node->hash==(size_t)tmp;
}


void DB_MoteInfoSave(uint64_t deveui, mac_param_t *mp)
{
    char *statement=(char *)malloc(4096);

    SQL_ROW *dbres=sqlfmt(&db, statement, 4096, "select * from moteinfo where DevEUI=%u", deveui);

    char appkey [40]={0};
    char appskey[40]={0};
    char nwkskey[40]={0};

    cJSON *freqarry=cJSON_CreateArray();

    for (int i=0; i<16; i++) {
        sprintf(appkey+i*2, "%02X", mp->appkey[i]);
    }

    for (int i=0; i<16; i++) {
        sprintf(appskey+i*2, "%02X", mp->appskey[i]);
    }

    for (int i=0; i<16; i++) {
        sprintf(nwkskey+i*2, "%02X", mp->nwkskey[i]);
    }

    log(LOG_DEBUG, "appkey:%s", appkey);

    for (int i=0; i<mp->freqnum; i++) {
        cJSON *obj=cJSON_CreateObject();
        cJSON_AddNumberToObject(obj,"UlFreq",mp->freq[i]);
        cJSON_AddNumberToObject(obj,"DlFreq",mp->dlfreq[i]);
        cJSON_AddItemToArray(freqarry, obj);
    }

    char *str=cJSON_Print(freqarry);

    if (dbres!=NULL) { /* update mote information */
        snprintf(statement, 4096, "update moteinfo set Version=%d, DevAddr=%u, AppEUI=%llu,\
                 "AppKey=\"%s\", Class=%d, AppSKey=\"%s\", NwkSKey=\"%s\", RxDelay=%d, Rx1DrOff=%d,\
                 "Rx2Dr=%d, Rx2Freq=%u, DutyCycle=%d, FreqType=%d, Freqs=\"%s\", SeqUl=%lu, SeqDl=%lu where DevEUI=%llu",
                  mp->version, mp->devaddr, mp->appeui, appkey, mp->lclass, appskey, nwkskey,
                  mp->rxdelay, mp->rx1droffset, mp->rx2dr, mp->rx2freq, mp->dutycycle, mp->freqtype,
                  str, mp->sequl, mp->seqdl, deveui);
        //sqlfmt(&db, );
    }
    else { /* insert mote information */
        snprintf(statement, 4095, "insert into moteinfo values (%llu, %d, %d, %llu, \"%s\", %d,\
                 "\"%s\", \"%s\", %d, %d, %d, %u, %d, %d, \"%s\", 0, 0)", deveui, mp->version, mp->devaddr,
                 mp->appeui, appkey, mp->lclass, appskey, nwkskey, mp->rxdelay, mp->rx1droffset,
                 mp->rx2dr, mp->rx2freq, mp->dutycycle, mp->freqtype, str);
        //sqlfmt(&db, statement, 4096, ");
    }

    log(LOG_DEBUG, statement);

    runsql(&db, statement);

    sqldb_free_rows(dbres);
    cJSON_Delete(freqarry);

    free(statement);
}

bool DB_MoteInfoRead(uint64_t deveui, mac_param_t *mp)
{
    char *statement=(char *)malloc(4096);
    SQL_ROW *dbres=sqlfmt(&db, statement, 4096, "select * from moteinfo where DevEUI=%u", deveui);

    if (dbres==NULL) {
        free(statement);
        log(LOG_ERR, "have no mote %llu info", deveui);
        return false;
    }

    memset(mp, 0, sizeof(mac_param_t));

    mp->version=atoi(get_column(dbres, "Version"));
    mp->devaddr=strtoul(get_column(dbres, "DevAddr"), 0, 0);
    mp->appeui=strtoul(get_column(dbres, "AppEUI"), 0, 0);
    mp->lclass=atoi(get_column(dbres, "Class"));
    mp->rxdelay=atoi(get_column(dbres, "RxDelay"));
    mp->rx1droffset=atoi(get_column(dbres, "Rx1DrOff"));
    mp->rx2dr=atoi(get_column(dbres, "Rx2Dr"));
    mp->rx2freq=atoi(get_column(dbres, "Rx2Freq"));
    mp->dutycycle=atoi(get_column(dbres, "DutyCycle"));
    mp->freqtype=atoi(get_column(dbres, "FreqType"));
    mp->sequl=atoi(get_column(dbres, "SeqUl"));
    mp->seqdl=atoi(get_column(dbres, "SeqDl"));

    char *appkey=get_column(dbres, "AppKey");
    char *appskey=get_column(dbres, "AppSKey");
    char *nwkskey=get_column(dbres, "NwkSKey");
    char *freqstr=get_column(dbres, "Freqs");

    if (hexstr2bytes(mp->appkey, appkey, 32)<0) {
        log(LOG_ERR, "%s is wrong AppKey", appkey);
        return false;
    }

    if (hexstr2bytes(mp->appskey, appskey, 32)<0) {
        log(LOG_ERR, "%s is wrong AppSKey", appskey);
        return false;
    }

    if (hexstr2bytes(mp->nwkskey, nwkskey, 32)<0) {
        log(LOG_ERR, "%s is wrong NwkSKey", nwkskey);
        return false;
    }

    cJSON *obj=cJSON_Parse(freqstr);

    for (int i=0; i<cJSON_GetArraySize(obj); i++) {
        cJSON *obj1=cJSON_GetArrayItem(obj, i);
        mp->freq[i]=cJSON_GetObjectItem(obj1,"UlFreq")->valueint;
        mp->dlfreq[i]=cJSON_GetObjectItem(obj1,"DlFreq")->valueint;
        mp->freqnum++;
    }

    sqldb_free_rows(dbres);
    cJSON_Delete(obj);
    free(statement);

    return true;
}

bool DB_MoteDevEUIRead(uint32_t devaddr, uint64_t *deveui)
{
    char *statement=(char *)malloc(4096);
    SQL_ROW *dbres=sqlfmt(&db, statement, 4096, "select DevEUI from moteinfo where DevAddr=%u", devaddr);

    if (dbres==NULL) {
        log(LOG_ERR, "have no devaddr %u info in database", devaddr);
        free(statement);
        return false;
    }

    *deveui=strtoul(get_column(dbres, "DevEUI"), 0, 0);
    sqldb_free_rows(dbres);
    free(statement);

    return true;
}

void DB_GwInfoSave(uint64_t gwid, gw_param_t *gp)
{
    char *statement=(char *)malloc(1024);
    SQL_ROW *dbres=sqlfmt(&db, statement, 1024, "select * from gwinfo where GwID=%llu", gwid);

    if (dbres!=NULL) { /* update mote information */
        sqlfmt(&db, statement, 1024, "update GwInfo set Pow=%d where GwID=%llu", gp->powe, gwid);
    }
    else { /* insert mote information */
        sqlfmt(&db, statement, 1024, "insert into GwInfo values (%llu, %d)", gwid, gp->powe);
    }

    sqldb_free_rows(dbres);
    free(statement);
}

bool DB_GwInfoRead(uint64_t gwid, gw_param_t *gp)
{
    char *statement=(char *)malloc(4096);
    SQL_ROW *dbres=sqlfmt(&db, statement, 4096, "select * from gwinfo where GwID=%llu", gwid);

    if (dbres==NULL) { /* update mote information */
        free(statement);
        return false;
    }

    gp->powe=atoi(get_column(dbres, "Pow"));
    sqldb_free_rows(dbres);
    free(statement);
    return true;
}


void LoRaMacProcessMacCommands(uint8_t *data, uint8_t size, mac_resp_t *res)
{
}

void MP_DBMoteInfoSave(uint64_t deveui, mac_param_t *mp, bool ack)
{
    struct hash_node *node=hashmap_get(&deveui_seq_map, &deveui);

    if (node!=NULL) {
        hash_data_t *hd=container_of(node,hash_data_t,node);
        mote_seq_pre_t *seq_pre=(mote_seq_pre_t *)hd->data;
        int diff=mp->sequl-seq_pre->sequl;
        if (((diff>3 || diff<0) && ack) || diff > 100) {
            seq_pre->sequl=mp->sequl;
            DB_MoteInfoSave(deveui, mp);
        }
    }
    else {
        /* seq info lost */
        log(LOG_ERR, "mote %llu info lost", deveui);
    }
}

void MP_DBMoteInfoRead(uint64_t deveui)
{
    struct hash_node *node=hashmap_get(&deveui_param_map, &deveui);
    mac_param_t *mp=NULL;
    mote_seq_pre_t *seq=NULL;

    if (node!=NULL) {
        log(LOG_ERR, "mote %llu info exist, don't need get from database", deveui);
        return;
    }

    node=hashmap_get(&deveui_seq_map, &deveui);

    if (node!=NULL) {
        log(LOG_ERR, "mote %llu info exist, don't need get from database", deveui);
        return;
    }

    mp=(mac_param_t *)malloc(sizeof(mac_param_t));

    if (DB_MoteInfoRead(deveui, mp)==true) {
        seq=(mote_seq_pre_t *)malloc(sizeof(mote_seq_pre_t));
        seq->seqdl=mp->seqdl;
        seq->sequl=mp->sequl;

        if (mp->sequl > 0 && mp->seqdl > 0) {
            mp->seqdl += 5;
        }

        hash_data_t *hd=(hash_data_t *)malloc(sizeof(hash_data_t));
        hd->data=mp;
        hashmap_insert(&deveui_param_map, &hd->node, &deveui);
        hd=(hash_data_t *)malloc(sizeof(hash_data_t));
        hd->data=seq;
        hashmap_insert(&deveui_seq_map, &hd->node, &deveui);

        log(LOG_DEBUG, "read deveui:%016lX info from database\n", deveui);
    }
    else { /* request to cloud */
        free(mp);
        log(LOG_ERR, "have no mote %llu parameter, please configure mote before", deveui);

        sendMoteInfoReq(deveui);
    }
}

uint64_t MP_DBMoteInfoRead2(uint32_t devaddr)
{
    uint64_t deveui;

    if (DB_MoteDevEUIRead(devaddr, &deveui)==false) {
        return 0;
    }

    struct hash_node *node=hashmap_get(&deveui_param_map, &deveui);

    if (node != NULL) {
        log(LOG_ERR, "DevEUI %llu info exist already", deveui);
        return 0;
    }

    MP_DBMoteInfoRead(deveui);

    hash_data_t *hd=(hash_data_t *)malloc(sizeof(hash_data_t));
    hd->data=malloc(sizeof(uint64_t));
    *(uint64_t *)hd->data=deveui;
    hashmap_insert(&devaddr_deveui_map, &hd->node, &devaddr);

    return deveui;
}

void MP_FillMacCommand(uint8_t *p,  mac_dl_t *d)
{

}

void MP_SendMsg2Cloud(void)
{
}

void MP_InitDlData(gw_pkt_tx_t *tp, gw_pkt_rx_t *rp, mac_param_t *p)
{
    memcpy(tp->codr,rp->codr,sizeof(rp->codr));
    memcpy(tp->datar,rp->datar,sizeof(rp->datar));

    /* calculate time to send for gw */
    memcpy(tp->time, rp->time, sizeof(rp->time));

    /* TODO: add CN470&Other */
    switch(p->freqtype) {
        case 0:
        case 1:
            tp->freq=rp->freq;
            break;
        case 2:
            tp->freq=rp->freq;
            if (tp->freq>=470000000&&tp->freq<=510000000)
                tp->freq+=30000000;
            break;
        default:
            tp->freq=rp->freq;
            if (p->freqnum>0) {
                for (int i=0; i<p->freqnum; i++) {
                    if (p->freq[i] == rp->freq) {
                        tp->freq = p->dlfreq[i];
                    }
                }
            }
            break;
    }

    log(LOG_DEBUG, "freq type %d!", p->freqtype);

    tp->imme=false;
    tp->ipol=true;

    uint8_t rxdelay=p->rxdelay==0?1:p->rxdelay;
    if (p->lclass==2) { /* class C */
        rxdelay=p->rxdelay==0?2:p->rxdelay+1;
        tp->imme=true;
    }

    tp->tmst=rp->tmst+rxdelay*1000000;
    tp->ncrc=true;
    tp->prea=8;
    tp->modu=rp->modu;
}

mac_dl_t *MP_GetDlData(uint64_t deveui)
{
    struct hash_node *node=hashmap_get(&deveui_data_dl,&deveui);

    if (node==NULL) {
        return NULL;
    }

    hash_data_t *hd=container_of(node,hash_data_t,node);
    queue_t *dqueue=(queue_t *)hd->data;
    if (queue_empty(dqueue)) {
        return NULL;
    }

    mac_dl_t *md=NULL;
    queue_get(dqueue, (void **)&md);
    return md;
}

void MP_BuildDlRespData(gw_pkt_tx_t *tp, uint64_t deveui, mac_resp_t *rsp, mac_param_t* mp)
{
#if 0
    struct hash_node *node=hashmap_get(&deveui_param_map, &deveui);

    if (node==NULL) {
        log(LOG_ERR, "have no mote %llu parameter", deveui);
        return;
    }

    hash_data_t *hd=container_of(node,hash_data_t,node);
    mac_param_t *mp=(mac_param_t *)hd->data;
#endif

    if (mp->lclass==2) { /* class C */
        memcpy(tp->codr, "4/5", strlen("4/5"));
        char *dr=NULL;
        switch(mp->rx2dr) {
            case 0:
                dr="SF12BW125K";
                break;
            case 1:
                dr="SF11BW125K";
                break;
            case 2:
                dr="SF10BW125K";
                break;
            case 3:
                dr="SF9BW125K";
                break;
            case 4:
                dr="SF8BW125K";
                break;
            case 5:
                dr="SF7BW125K";
                break;
            default:
                log(LOG_ERR, "wrong rx2 datarate %d, use datarate 0", mp->rx2dr);
                dr="SF12BW125K";
                break;
        }

        memcpy(tp->datar, dr, strlen(dr));
        tp->freq=mp->rx2freq;
        tp->ipol=true;
        tp->modu=1;
        tp->imme=true;
    }

    /* build mac frame */
    uint8_t data[256]={0};
    uint8_t txdata=rsp->size>0?1:0;

    // Piggyback MAC options
    // Prioritize by importance
    int  end = 8;
    if (rsp != NULL) { /* fill data and mac command */
        if (rsp->msize>15) {
            log(LOG_ERR, "mac command is too long, len:%d", rsp->msize);
            memcpy(&data[end], rsp->mac, 15);
            end+=15;
        }
        else {
            memcpy(&data[end], rsp->mac, rsp->msize);
            end+=rsp->msize;
        }
    }

    uint8_t flen = end + (rsp->size>0 ? 5+rsp->size : 4);

    if( flen > 255 ) {
        // Options and payload too big - delay payload
        txdata = 0;
        flen = end+4;
    }

    data[0] = rsp->confirm?0xA0:0x60;
    data[5] = (rsp->fctrl.Value&0xF0) | (end-8);
    //*(uint32_t *)&data[1]=mp->devaddr;
    //*(uint16_t *)&data[6]=mp->seqdl++;
    COMM_MSB_W_4(&data[1], htonl(mp->devaddr));
    COMM_MSB_W_2(&data[6], htons(mp->seqdl-1));

    if( txdata) {
        data[end] = rsp->port;
        LoRaMacPayloadEncrypt(rsp->data,rsp->size,rsp->port==0?mp->nwkskey:mp->appskey,
            mp->devaddr,1,mp->seqdl-1,data+end+1);
    }

    uint32_t mic=0;
    LoRaMacComputeMic(data,flen-4,mp->nwkskey,mp->devaddr,1,mp->seqdl-1,&mic);
    //*(uint32_t *)&data[flen-4]=mic;
    COMM_MSB_W_4(&data[flen-4], htonl(mic));
    memcpy(tp->data, data, flen);
    tp->size=flen;

    log(LOG_NORMAL, "downlink packet:");
    dump(tp->data, tp->size);
}

gw_pkt_tx_t* MP_BuildClassCData(mac_param_t *mp, mac_dl_t *md)
{
    gw_pkt_tx_t *tp=(gw_pkt_tx_t *)malloc(sizeof(gw_pkt_tx_t));

    /* set packet parameter */
    memset(tp, 0, sizeof(gw_pkt_tx_t));
    memcpy(tp->codr, "4/5", strlen("4/5"));
    char *dr=NULL;
    switch(mp->rx2dr) {
        case 0:
            dr="SF12BW125K";
            break;
        case 1:
            dr="SF11BW125K";
            break;
        case 2:
            dr="SF10BW125K";
            break;
        case 3:
            dr="SF9BW125K";
            break;
        case 4:
            dr="SF8BW125K";
            break;
        case 5:
            dr="SF7BW125K";
            break;
        default:
            log(LOG_ERR, "wrong rx2 datarate %d, use datarate 0", mp->rx2dr);
            dr="SF12BW125K";
            break;
    }
    memcpy(tp->datar, dr, strlen(dr));
    tp->freq=mp->rx2freq;
    tp->ipol=true;
    tp->modu=1;
    tp->imme=true;
    tp->ncrc=true;
    tp->prea=8;
    tp->rfch=0;

    /* build mac frame */
    uint8_t *data=tp->data;
    int i=0;
    if (md->type==CLOUD_DL_TYPE_CONFIRM) { /* MHDR */
        data[i++]=(5<<5);
    }
    else
        data[i++]=(3<<5);
    *(uint32_t *)&data[i]=mp->devaddr;     /* DevAddr */
    i+=4;

    if (md->msize>=16) {
        log(LOG_ERR, "mac command len %u error", md->msize);
        md->msize=0;
    }

    if (md->msize==0)                      /* FCtrl */
        data[i++]=0;
    else
        data[i++]=md->msize;

    mp->seqdl++;
    *(uint16_t *)&data[i]=mp->seqdl;       /* FCnt */
    i+=2;

    if (md->msize>0) {                     /* FOpts */
        memcpy(&data[i], md->mac, md->msize);
        i+=md->msize;
    }

    data[i++]=md->port;                    /* FPort */

    memcpy(&data[i], md->data, md->size);  /* AppData */
    i+=md->size;

    LoRaMacComputeMic(data, i, mp->appskey, mp->devaddr, 1, mp->seqdl-1, (uint32_t *)&data[i]);
    i+=4;

    tp->size=i;

    return tp;
}


void MP_InitJoinAccept(gw_pkt_tx_t *tp, gw_pkt_rx_t *rp, mac_param_t *p)
{
    memcpy(tp->codr,rp->codr,sizeof(rp->codr));
    memcpy(tp->datar,rp->datar,sizeof(rp->datar));

    /* calculate time to send for gw */

    memcpy(tp->time, rp->time, sizeof(rp->time));

    /* TODO: add CN470&Other */
    switch(p->freqtype) {
        case 0:
        case 1:
            tp->freq=rp->freq;
            break;
        case 2:
            tp->freq=rp->freq;
            if (tp->freq>=470000000&&tp->freq<=510000000)
                tp->freq+=30000000;
            break;
        default:
            tp->freq=rp->freq;
            if (p->freqnum>0) {
                for (int i=0; i<p->freqnum; i++) {
                    if (p->freq[i] == rp->freq) {
                        tp->freq = p->dlfreq[i];
                    }
                }
            }
            break;
    }

    log(LOG_DEBUG, "freq type %d!", p->freqtype);

    tp->imme=false;
    tp->ipol=true;
    tp->tmst=rp->tmst+5000000;
    tp->ncrc=true;
    tp->modu=rp->modu;
    tp->prea=8;
}

void sendGwInfoReq(uint64_t gwid)
{
    cloud_msg_ul_t *ulmsg=(cloud_msg_ul_t *)malloc(sizeof(cloud_msg_ul_t));
    cloud_gw_info_req_t *req=(cloud_gw_info_req_t *)malloc(sizeof(cloud_gw_info_req_t));

    req->gwid=gwid;
    ulmsg->type=2;
    ulmsg->msg=req;
    queue_put(cloud_ul_queue, ulmsg);
}

void sendMoteInfoReq(uint64_t deveui)
{
    cloud_msg_ul_t *ulmsg=(cloud_msg_ul_t *)malloc(sizeof(cloud_msg_ul_t));
    cloud_mote_info_req_t *req=(cloud_mote_info_req_t *)malloc(sizeof(cloud_mote_info_req_t));

    req->deveui=deveui;
    ulmsg->type=3;
    ulmsg->msg=req;
    queue_put(cloud_ul_queue, ulmsg);
    log(LOG_NORMAL, "send mote info req %p to cloud", ulmsg);
}

uint8_t MP_FillCflist(uint8_t* buf, mac_param_t* p)
{
    if (p->freqtype!=3 && p->cfnum<=16) {
        memcpy(buf, p->cflist, p->cfnum);

        return p->cfnum;
    }

    return 0;
}

gw_pkt_tx_t* MP_HandleJoinReq(uint64_t gwid, gw_pkt_rx_t *p)
{
    uint8_t *data=p->data;

    if (p->size != 23) {
        log(LOG_ERR, "join request length is wrong");
        return NULL;
    }

    log(LOG_NORMAL, "join request packet:");
    dump(p->data, p->size);
    uint8_t  version=data[0]&0x03;
    uint64_t appeui=*(uint64_t *)&data[1];
    uint64_t deveui=*(uint64_t *)&data[9];
    uint16_t devnonce=*(uint16_t *)&data[17];
    /* check mic */
    uint32_t mic=*(uint32_t *)&data[19];
    uint32_t calc=0;
    /* get mote info */
    struct hash_node *node=hashmap_get(&deveui_param_map, &deveui);
    if (node==NULL) {
        log(LOG_ERR, "get mote info from database");
        MP_DBMoteInfoRead(deveui);
        return NULL;
    }
    hash_data_t *hd=container_of(node, hash_data_t, node);
    mac_param_t *mp=(mac_param_t *)hd->data;
    LoRaMacJoinComputeMic(data, 19, mp->appkey, &calc);

    if (mic!=calc) {
        log(LOG_ERR, "mote %llu join mic error! AppKey:", deveui);
        dump(mp->appkey, 16);
        sendMoteInfoReq(deveui);
        return NULL;
    }

    mp->version=version;

    /* build join accept */
    gw_pkt_tx_t *tp=(gw_pkt_tx_t *)malloc(sizeof(gw_pkt_tx_t));
    memset(tp, 0, sizeof(gw_pkt_tx_t));

    /* get gateway power parameter */
    tp->powe=20;

    data=tp->data;
    int i=0;
    data[i++]=0x20 | (p->data[0]&0x03);            /* MHDR */
    //uint32_t appnonce=rand()&0x00ffffff;           /* AppNonce */
    uint32_t appnonce=1;
    data[i++]=appnonce&0xff;
    data[i++]=(appnonce>>8)&0xff;
    data[i++]=(appnonce>>16)&0xff;
    data[i++]=1;                                   /* Netid */
    data[i++]=0;
    data[i++]=0;
    /* get devaddr */
    uint32_t devaddr;
    if (mp->devaddr==0) {
        devaddr=devaddr_max++;
        mp->devaddr=devaddr;
    }
    else {
        devaddr=mp->devaddr;
    }

    /* DevAddr */
    *(uint32_t *)&data[i]=devaddr;
    i+=4;

    hash_data_t *hp=(hash_data_t *)malloc(sizeof(hash_data_t));
    hp->data=malloc(sizeof(uint64_t));
    *(uint64_t *)hp->data=deveui;

    hashmap_insert(&devaddr_deveui_map, &hp->node,  &devaddr);
    mp->devaddr=devaddr;

    /* DLSetting */
    data[i++]=((mp->rx1droffset&0x07)<<4)|(mp->rx2dr&0x0f);
     /* rx1delay */
    data[i++]=mp->rxdelay;
    /* CFList */
    i+=MP_FillCflist(&data[i], mp);
    /* MIC */
    LoRaMacJoinComputeMic(data, i, mp->appkey, (uint32_t *)&data[i]);
    i+=4;

    tp->data[0]=data[0];

    /* generate appskey&nwkskey */
    LoRaMacJoinComputeSKeys(mp->appkey, data+1, devnonce, mp->nwkskey, mp->appskey);
    /* Encrypt */
    LoRaMacJoinEncrypt(data+1, i-1, mp->appkey, tp->data+1);

    /* init packet parameter */
    MP_InitJoinAccept(tp, p, mp);
    if (hashmap_get(&gwid_param_map, &gwid)==NULL) {
        tp->powe=20;
    }
    tp->size=i;
    memcpy(tp->data, data, i);

    /* update mote info */
    mp->seqdl=0;
    mp->sequl=0;
    node=hashmap_get(&deveui_seq_map, &deveui);
    if (node==NULL) {
        mote_seq_pre_t *seq_pre=(mote_seq_pre_t *)malloc(sizeof(mote_seq_pre_t));
        memset(seq_pre, 0, sizeof(mote_seq_pre_t));
        hd=(hash_data_t *)malloc(sizeof(hash_data_t));
        hd->data=seq_pre;
        hashmap_insert(&deveui_seq_map, &hd->node, &deveui);
    }
    else {
        hd=container_of(node,hash_data_t,node);
        memset(hd->data, 0, sizeof(mote_seq_pre_t));
    }

    /* update database */
    DB_MoteInfoSave(deveui, mp);

    /* update join state to cloud */

    return tp;
}

gw_pkt_tx_t* MP_HandleData(uint64_t gwid, gw_pkt_rx_t *rp)
{
    uint8_t *payload=rp->data;
    uint8_t pktHeaderLen=0;
    gw_pkt_tx_t *tp=NULL;
    mac_resp_t mresp={0};
    uint64_t deveui=0;
    hash_data_t *hd=NULL;

    pktHeaderLen++;

    log(LOG_DEBUG, "size:%u", rp->size);
    //dump(rp, sizeof(gw_pkt_rx_t));

    uint32_t address = *(uint32_t *)&payload[pktHeaderLen];
    pktHeaderLen+=4;

    /* get module parameters */
    struct hash_node *node=hashmap_get(&devaddr_deveui_map, &address);
    if (node==NULL) {
        deveui=MP_DBMoteInfoRead2(address);
        log(LOG_ERR, "read devaddr %04X info, deveui:%016lX\n", address, deveui);
    }
    else {
        hd=container_of(node,hash_data_t,node);
        deveui=*(uint64_t *)hd->data;
    }

    if (deveui==0) {
        log(LOG_ERR, "have no devaddr %u info", address);
        log(LOG_ERR, "please rejoin before");
        return NULL;
    }

    node=hashmap_get(&deveui_param_map, &deveui);
    hd=container_of(node,hash_data_t,node);
    mac_param_t *mp=(mac_param_t *)hd->data;

    LoRaMacFrameCtrl_t fCtrl;
    fCtrl.Value = payload[pktHeaderLen++];

    uint16_t sequenceCounter = payload[pktHeaderLen++];
    sequenceCounter |= payload[pktHeaderLen++] << 8;

    uint8_t appPayloadStartIndex = 8 + fCtrl.Bits.FOptsLen;

    uint32_t micRx=*(uint32_t *)&payload[rp->size- 4];

    int32_t sequence = (int32_t)sequenceCounter - (int32_t)( mp->sequl & 0xFFFF );

    bool isMicOk=false;
    uint32_t sequl;
    uint32_t mic=0;
    if( sequence < 0 )
    {
        // sequence reset or roll over happened

        sequl = ( mp->sequl & 0xFFFF0000 ) | ( sequenceCounter + ( uint32_t )0x10000 );

        LoRaMacComputeMic( payload, rp->size- 4, mp->nwkskey, address, 0, sequl, &mic );
        if( micRx == mic )
        {
            isMicOk = true;
        }
    }
    else
    {
        sequl = ( mp->sequl & 0xFFFF0000 ) | sequenceCounter;
        LoRaMacComputeMic( payload, rp->size-4, mp->nwkskey, address, 0, sequl, &mic );
    }

    bool ack=false;
    if( ( isMicOk == false ) &&
        ( micRx != mic ) )
    {
        log(LOG_ERR, "data packet mic error");
        log(LOG_ERR, "size:%u, address:%u, sequl:%u", rp->size, address, sequl);
        dump(mp->nwkskey, 16);
        dump(payload, rp->size);

        return NULL;
    }

    if(payload[0]>>5 == 4) /* confirm packet */
    {
        ack = true;
        if (mp->sequl!=sequl) {
            mp->seqdl++;
        }
        else if (sequl == 0) {
            mp->seqdl++;
        }
    }

    mp->sequl=sequl;

    // Check if the frame is an acknowledgement
    if( fCtrl.Bits.Ack == 1 )
    {
        /* delete confirm dowlink wait data from downlink queue */
    }
    else
    {
        /* check if timeout */
    }

    if( fCtrl.Bits.FOptsLen > 0 )
    {
        // Decode Options field MAC commands
        if (tp==NULL) {
            tp=(gw_pkt_tx_t*)malloc(sizeof(gw_pkt_tx_t));
            MP_InitDlData(tp, rp, mp);
        }
        LoRaMacProcessMacCommands(payload+8, appPayloadStartIndex-8, &mresp);
    }

    uint8_t port=0;

    if( ( ( rp->size - 4 ) - appPayloadStartIndex ) > 0 )
    {
        port = payload[appPayloadStartIndex++];
        uint8_t frameLen = ( rp->size - 4 ) - appPayloadStartIndex;

        if( port == 0 )
        {
            LoRaMacPayloadDecrypt( payload + appPayloadStartIndex,
                                   frameLen,
                                   mp->nwkskey,
                                   address,
                                   0,
                                   sequl,
                                   payload);

            // Decode frame payload MAC commands
            LoRaMacProcessMacCommands( payload, 0, &mresp );
        }
        else
        {
            LoRaMacPayloadDecrypt( payload + appPayloadStartIndex,
                                   frameLen,
                                   mp->appskey,
                                   address,
                                   0,
                                   sequl,
                                   payload );
        }
    }

    if (ack==true) {
        if (tp==NULL) {
            tp=(gw_pkt_tx_t*)malloc(sizeof(gw_pkt_tx_t));
            memset(tp, 0, sizeof(gw_pkt_tx_t));
            MP_InitDlData(tp, rp, mp);
        }

        mresp.fctrl.Bits.Ack=1;
    }

    mac_dl_t *md=MP_GetDlData(deveui);
    if (md!=NULL) {
        if (tp==NULL) {
            tp=(gw_pkt_tx_t *)malloc(sizeof(gw_pkt_tx_t));
            memset(tp, 0, sizeof(gw_pkt_tx_t));
        }
        mresp.confirm=(md->type==1);
        mresp.port=md->port;
        mresp.msize=md->msize;
        memcpy(mresp.mac, md->mac, md->msize);
        mresp.size =md->size;
        memcpy(mresp.data, md->data, md->size);
        free(md);
    }

    if (tp!=NULL) {
        MP_InitDlData(tp,rp,mp);
        MP_BuildDlRespData(tp, deveui, &mresp, mp);
    }

    /* save info to database */
    MP_DBMoteInfoSave(deveui, mp, ack);

    /* send to cloud */
    cloud_msg_ul_t *cmsg=(cloud_msg_ul_t *)malloc(sizeof(cloud_msg_ul_t));
    cmsg->type=1;
    cloud_modu_ul_t *mudata=(cloud_modu_ul_t *)malloc(sizeof(cloud_modu_ul_t));
    mudata->deveui=deveui;
    mudata->gwid=gwid;
    memcpy(mudata->codr, rp->codr, sizeof(rp->codr));
    memcpy(mudata->datar, rp->datar, sizeof(rp->datar));
    memcpy(mudata->time, rp->time, sizeof(rp->time));
    mudata->size=( rp->size - 4 ) - appPayloadStartIndex;
    if (mudata->size>0) {
        memcpy(mudata->data, payload, mudata->size);
    }
    mudata->devaddr=mp->devaddr;
    mudata->fport=port;
    mudata->freq=rp->freq;
    mudata->lsnr=rp->lsnr;
    mudata->rssi=rp->rssi;
    mudata->sequl=mp->sequl;
    mudata->seqdl=mp->seqdl>0?mp->seqdl-1:0;
    mudata->tmst=rp->tmst;

    cmsg->msg=mudata;

    log(LOG_NORMAL, "push lora data packet to cloud thread");
    queue_put(cloud_ul_queue,cmsg);

    return tp;
}

gw_pkt_tx_t *MP_HandleLoraPkt(uint64_t gwid, gw_pkt_rx_t *rp)
{
    /* get packet type */
    uint8_t type=rp->data[0]>>5;
    gw_pkt_tx_t *tp=NULL;

    switch(type) {
        case 0:
            tp=MP_HandleJoinReq(gwid,rp);
            break;
        case 2:
        case 4:
            tp=MP_HandleData(gwid, rp);
            break;
        default:
            log(LOG_ERR, "unknow type %d", type);
            break;
    }

    if (tp != NULL) {
        struct hash_node *node=hashmap_get(&gwid_param_map, &gwid);

        if (node==NULL) {
            log(LOG_ERR, "don't have gateway %llu param", gwid);
            tp->powe=20;
        }
        else {
            hash_data_t *hd=container_of(node,hash_data_t,node);
            gw_param_t *gp=(gw_param_t *)hd->data;
            tp->powe=gp->powe;
        }
    }

    return tp;
}

void MP_HandleGuMsg(gw_msg_t *msg)
{
    gw_pkt_tx_t *tp=MP_HandleLoraPkt(msg->gwid, (gw_pkt_rx_t*)msg->msg);

    if (tp==NULL) {
        log(LOG_DEBUG, "no downlink packet");
        return;
    }

    gw_msg_t *res=(gw_msg_t *)malloc(sizeof(gw_msg_t));
    memcpy(res, msg, sizeof(gw_msg_t));
    res->msg=tp;

    log(LOG_NORMAL, "push downlink to gu thread, pkt:%p", tp);
    queue_put(gu_dl_queue, res);
}

void MP_HandleCloudMoteInfo(mp_msg_t *msg)
{
    cloud_modu_param_t *p=(cloud_modu_param_t *)msg->msg;
    mac_param_t *mp=NULL;

    /* save to hash map */
    struct hash_node *node=hashmap_get(&deveui_param_map, &p->deveui);

    if (node==NULL) {
        mp=(mac_param_t *)malloc(sizeof(mac_param_t));

        memcpy(mp, &p->param, sizeof(mac_param_t));

        hash_data_t *hd=(hash_data_t *)malloc(sizeof(hash_data_t));

        hd->data=mp;
        hashmap_insert(&deveui_param_map, &hd->node, &p->deveui);
        log(LOG_NORMAL, "insert mote %llu param to hash map", p->deveui);
    }
    else {
        hash_data_t *hd=container_of(node,hash_data_t,node);
        mp=(mac_param_t *)hd->data;

        /* remain mac frame seq and devaddr, change other param */
        mp->version=p->param.version;
        mp->appeui=p->param.appeui;
        memcpy(mp->appkey, p->param.appkey, 16);
        mp->lclass=p->param.lclass;
        mp->active=p->param.active;

        if (p->param.active>0) {
            memcpy(mp->appskey, p->param.appskey, 16);
            memcpy(mp->nwkskey, p->param.nwkskey, 16);
            mp->sequl=p->param.sequl;
            mp->seqdl=p->param.seqdl;
        }

        mp->rxdelay=p->param.rxdelay;
        mp->rx1droffset=p->param.rx1droffset;
        mp->rx2dr=p->param.rx2dr;
        mp->rx2freq=p->param.rx2freq;
        mp->dutycycle=p->param.dutycycle;
        mp->freqtype=p->param.freqtype;
        mp->freqnum=p->param.freqnum;

        memcpy(mp->freq, p->param.freq, sizeof(p->param.freq));
        memcpy(mp->dlfreq, p->param.dlfreq, sizeof(p->param.freq));
    }

    /* save to database */
    DB_MoteInfoSave(p->deveui, mp);
}

void MP_HandleCloudMoteData(mp_msg_t *msg)
{
    cloud_modu_dl_t *d=(cloud_modu_dl_t *)msg->msg;
    mac_dl_t *dd=(mac_dl_t *)malloc(sizeof(mac_dl_t));
    memcpy(dd, &d->dldata, sizeof(mac_dl_t));

    /* get mote parameter */
    struct hash_node *node=hashmap_get(&deveui_param_map, &d->deveui);
    if (node==NULL) {
        log(LOG_ERR, "must configure mote %llu before!", d->deveui);
        return;
    }

    hash_data_t *hd=container_of(node,hash_data_t,node);
    mac_param_t *mp=(mac_param_t *)hd->data;

    if (mp->lclass !=2) {
        /* save to hash map */
        queue_t *dqueue=NULL;

        if (hashmap_get(&deveui_data_dl, &d->deveui)==NULL) {
            dqueue=queue_create();
            hash_data_t *hd=(hash_data_t *)malloc(sizeof(hash_data_t));
            hd->data=dqueue;
            hashmap_insert(&deveui_data_dl,&hd->node,&d->deveui);
        }

        if (dqueue==NULL) {
            struct hash_node *node=hashmap_get(&deveui_data_dl, &d->deveui);
            hash_data_t *hd=container_of(node,hash_data_t,node);
            dqueue=(queue_t*)hd->data;
        }

        /* save to downlink queue */
        queue_put(dqueue, dd);

        return;
    }

    /* Send immediately for Class C */
    gw_pkt_tx_t *tp=MP_BuildClassCData(mp, dd);
    gw_msg_t *gm=(gw_msg_t *)malloc(sizeof(gw_msg_t));

    gm->msg=tp;
    log(LOG_NORMAL, "insert mote %llu data to queue", d->deveui);

    /* send to gu thread */
    queue_put(gu_dl_queue,gm);
}

void MP_HandleCloudGwInfo(mp_msg_t *msg)
{
    cloud_gw_param_t *p=(cloud_gw_param_t *)msg->msg;

    gw_param_t *gp=(gw_param_t *)malloc(sizeof(gw_param_t));
    gp->powe=p->param.powe;
}

void MP_HandleCloudMsg(mp_msg_t *msg)
{
    switch(msg->type) {
        case 0:
            MP_HandleCloudMoteInfo(msg);
            break;
        case 1:
            MP_HandleCloudMoteData(msg);
            break;
        case 2:
            MP_HandleCloudGwInfo(msg);
            break;
        default:
            log(LOG_ERR, "unkown cloud message type %d!", msg->type);
            break;
    }
}

void *MP_Task(void *arg)
{
    hashmap_init(&deveui_data_dl    , DevEui_Data_Hash   , DevEui_Data_Cmp   );
    hashmap_init(&gwid_param_map    , GwId_Param_Hash    , GwId_Param_Cmp    );
    hashmap_init(&deveui_seq_map    , DevEui_Data_Hash   , DevEui_Data_Cmp   );
    hashmap_init(&deveui_param_map  , DevEui_Param_Hash  , DevEui_Param_Cmp  );
    hashmap_init(&devaddr_deveui_map, Devaddr_DevEui_Hash, Devaddr_DevEui_Cmp);

    if (connectdb(&db)==false) {
        log(LOG_ERR, "connect database failed");
        while(1);
    }

    while(true) {
        bool empty=true;

        if (!queue_empty(lora_hanle_queue)) {
            gw_msg_t *msg;

            queue_get(lora_hanle_queue, (void**)&msg);

            if (msg!=NULL) {
                MP_HandleGuMsg(msg);
                free(msg->msg);
                free(msg);
            }
            else {
                log(LOG_ERR, "lora handle queue is empty");
            }

            empty=false;
        }

        if (!queue_empty(mp_dl_queue)) {
            mp_msg_t *msg=NULL;

            queue_get(mp_dl_queue, (void**)&msg);

            if (msg!=NULL) {
                MP_HandleCloudMsg(msg);
                free(msg->msg);
                free(msg);
            }

            empty=false;
        }

        if (empty==false)
            usleep(10);
        else
            usleep(1000);
    }

    return NULL;
}

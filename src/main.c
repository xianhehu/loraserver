#include "main.h"
#include "queue.h"
#include "common.h"
#include "configure.h"

queue_t *lora_hanle_queue=NULL;
queue_t *mp_dl_queue=NULL;
queue_t *gu_dl_queue=NULL;
queue_t *cloud_ul_queue=NULL;

pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

void *GU_ThreadUl(void *arg);
void *GU_ThreadDl(void *arg);
void *MP_Task(void *arg);
void *CLOUD_DlTask(void *arg);
void *CLOUD_UlTask(void *arg);



void Queue_Init(void)
{
    lora_hanle_queue=queue_create();
    gu_dl_queue=queue_create();
    mp_dl_queue=queue_create();
    cloud_ul_queue=queue_create();
}

int main(int argc, char* const argv[])
{
    pthread_t gu1,gu2,mp,cloud;

    getlogger("loraserver");

    char c = 0;
    int  level = 0;

    while((c = getopt(argc, argv, "gc:l:p::d")) != -1){
        switch(c){
        case 'g':
            log(LOG_NORMAL, "gateway listen at port:%s", argv[optind]);
            gw_port=atoi(argv[optind]);

			break;
        case 'c':
            log(LOG_NORMAL, "appserver address:%s", optarg);

            if (strlen(optarg)>=sizeof(cloud_ip)) {
                log(LOG_ERR, "param \"%s\" is too length", optarg);

                return 0;
            }

            memset(cloud_ip, 0, sizeof(cloud_ip));
            memcpy(cloud_ip, optarg, strlen(optarg));

            break;
            
        case 'l':
            level = atoi(optarg);

            if (level < 0 || level > LOG_DEBUG) {
                printf("无效的日志级别：%d，有效值为0~2对应ERROR~DEBUG", level);

                exit(-1);
            }

            setloggerlevel(level);
        
            break;
        case 'p':
            log(LOG_NORMAL, "appserver port:%s", optarg);

            cloud_port=atoi(optarg);

            break;
        case 'd':
            log(LOG_NORMAL, "database name:%s", optarg);

            break;
        default: {
            }
        }
    }

    log(LOG_DEBUG, "init all queues");
    Queue_Init();

    log(LOG_DEBUG, "start all threads");
    pthread_create(&gu1, NULL, GU_ThreadUl, NULL);
    pthread_create(&gu2, NULL, GU_ThreadDl, NULL);
    pthread_create(&mp, NULL, MP_Task, NULL);
    pthread_create(&cloud, NULL, CLOUD_DlTask, NULL);
    pthread_create(&cloud, NULL, CLOUD_UlTask, NULL);

    while(1) {
        usleep(1000000);
    }
}

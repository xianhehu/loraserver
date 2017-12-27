#ifndef __MAIN_H__

#define __MAIN_H__

#include <pthread.h>
#include "queue.h"

extern pthread_mutex_t lock;

extern queue_t *lora_hanle_queue;
extern queue_t *mp_dl_queue;
extern queue_t *gu_dl_queue;
extern queue_t *cloud_ul_queue;


#endif


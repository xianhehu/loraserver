/**************************************
文件名：Log.h
描述：  写日志功能
版本： 0.1
作者： jj
完成日期：2008/03/13
***************************************/
#ifndef _LOG_H_
#define _LOG_H_

#include <pthread.h>
#include <semaphore.h>
#include <log4cplus/logger.h>
#include <semaphore.h>
#include <log4cplus/log4cplus.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/layout.h>

#define LOGFILEDIR "./"
#define LOGFILEPATH "log"
//根据日志级别写日志
#define LOG_ERR     0   //0=记录错误日志
#define LOG_NORMAL  1   //1=记录日常日志和错误日志
#define LOG_DEBUG   2   //2=记录错误日志日常日志调试日志
#define LOG_LEVEL   LOG_DEBUG

#ifndef _MSC_VER  
#define _MSC_VER 1600 
#endif

#if 0
void log1(int loglev,const char * format, ... );
void loghex(int loglev,unsigned char *logmsg,int len);

extern char logbuf[1024];

#define LOG_BUF_SIZE 1024

#define log(loglev, fmt,...)\
{\
    int len=0;\
    memset(logbuf, 0, LOG_BUF_SIZE);\
    len+=snprintf(logbuf, LOG_BUF_SIZE-len-2, "[%s:%d]:", __FILE__, __LINE__);\
    len+=snprintf(logbuf+len, LOG_BUF_SIZE-len-2, fmt, ##__VA_ARGS__);\
    log1(loglev, logbuf);\
}

#define dump(data, size) loghex(LOG_NORMAL, data, size)

class Log {
private:
    //static CStatic *d_textlog;
    static char d_logtxt[1024 * 1024];

    static void saveLog(char *buffer);
    log4cplus::Logger d_log;

public:
    static char d_file_name[100];
    static pthread_mutex_t d_lock;
    static sem_t           d_sem;
    static FILE *fp;
    Log();
    Log(log4cplus::Logger log);
    //static void setLog(CStatic *cs);
    static void init(void);
    static void write(bool time, char *fmt...);
    static void dump(uint8_t *buf, int len);
    static char* Log::readLog(void);
    static Log* getlogger(char *name);
    void warn(char *fmt...);
    void debug(char *fmt...);
    void error(char *fmt...);
};
#endif

void getlogger(char *name);
void setloggerlevel(int level);
void log(int loglev,const char * format, ... );
void dump(uint8_t *data, int size);

#endif


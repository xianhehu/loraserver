/**************************************
文件名：Log.h
描述：  写日志功能
版本： 0.1
作者： jj
完成日期：2008/03/13
***************************************/

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
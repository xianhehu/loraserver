
//#include "stdafx.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "log.h"

#if 0
char logbuf[1024];

char Log::d_file_name[100] = { 0 };
//CStatic *Log::d_textlog    = NULL;
char Log::d_logtxt[1024 * 1024] = { 0 };
pthread_mutex_t Log::d_lock = { 0 };
sem_t Log::d_sem = { 0 };
FILE* Log::fp = NULL;

Log::Log()
{
    pthread_mutex_init(&d_lock, NULL);
}

Log::Log(log4cplus::Logger log)
{
    pthread_mutex_init(&d_lock, NULL);
    d_log = log;
}

void Log::init(void)
{
    pthread_mutex_init(&Log::d_lock, NULL);
    sem_init(&d_sem, 0, 0);

#if 0
    log4cplus::Initializer initializer;
    log4cplus::BasicConfigurator config;
    config.configure();
    log4cplus::PropertyConfigurator::doConfigure("log4cplus.properties");
#endif
}

void Log::write(bool withtime, char *fmt...)
{
    char buffer[500] = { 0 };
    int  len = 0;
    va_list vArgs;
    static bool first = true;

    time_t tim = time(NULL);

    struct tm t = { 0 };

    gmtime_s(&t, &tim);

    strftime(buffer, sizeof(buffer)-1, "%F %T %Z", &t);
    
    len += strlen(buffer);

    if (strlen(Log::d_file_name) <= 0) {
        //snprintf(Log::d_file_name, sizeof(Log::d_file_name) - 1, "loragateway-%s.log", buffer);
        snprintf(Log::d_file_name, sizeof(Log::d_file_name) - 1, "loragateway.log");
    }

    memcpy(buffer + len, ": ", 2);
    len += 2;

    va_start(vArgs, fmt);
    vsnprintf(buffer+len, sizeof(buffer)-len-1, (char const *)fmt, vArgs);
    va_end(vArgs);

    pthread_mutex_lock(&Log::d_lock);

#if 1
    if (fp == NULL) {
        fopen_s(&fp, Log::d_file_name, "a+");

        if (fp == NULL) {
            //pthread_mutex_unlock(&Log::d_lock);
            //return;
            fopen_s(&fp, Log::d_file_name, "w+");

            if (fp == NULL) {
                pthread_mutex_unlock(&Log::d_lock);
                return;
            }
        }
    }

    fprintf(fp, buffer);

    //fclose(fp);
#endif

    //if (d_textlog) {
    //    return;
    //}
    
    //d_textlog->SetWindowTextW((LPCTSTR)buffer);
    saveLog(buffer);

    pthread_mutex_unlock(&Log::d_lock);
}

Log* Log::getlogger(char *name)
{
#if 1
    //return new Log(log4cplus::Logger::getInstance(name));
    //Log* log = new Log();
    //log->d_log = log4cplus::Logger::getRoot();
    /* step 1: Instantiate an appender object */
    char filename[100] = { 0 };
    snprintf(filename, 99, "%s.log", name);
    log4cplus::SharedAppenderPtr _append(new log4cplus::FileAppender(filename));
    _append->setName(name);
    /* step 2: Instantiate a layout object */
    std::string pattern = "%d{%m/%d/%y %H:%M:%S:%Q}- %p- %m %n";
    /* step 3: Attach the layout object to the appender */
    _append->setLayout((std::unique_ptr<log4cplus::Layout>)new log4cplus::PatternLayout(pattern));
    /* step 4: Instantiate a logger object */
    log4cplus::Logger _logger = log4cplus::Logger::getInstance(name);
    /* step 5: Attach the appender object to the logger  */
    _logger.addAppender(_append);
    /* log activity */
#endif

    return new Log(_logger);
}

void Log::warn(char *fmt...)
{
    char buffer[500] = { 0 };
    int  len = 0;
    va_list vArgs;

    va_start(vArgs, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len - 1, (char const *)fmt, vArgs);
    va_end(vArgs);

    LOG4CPLUS_WARN(d_log, buffer);
}

void Log::error(char *fmt...)
{
    char buffer[500] = { 0 };
    int  len = 0;
    va_list vArgs;

    va_start(vArgs, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len - 1, (char const *)fmt, vArgs);
    va_end(vArgs);

    LOG4CPLUS_ERROR(d_log, buffer);
}

void Log::debug(char *fmt...)
{
    char buffer[500] = { 0 };
    int  len = 0;
    va_list vArgs;

    va_start(vArgs, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len - 1, (char const *)fmt, vArgs);
    va_end(vArgs);

    LOG4CPLUS_DEBUG(d_log, buffer);
}

void Log::dump(uint8_t *buf, int len)
{
    char *str = (char *)malloc(len * sizeof(char) * 3+100);
    int   l   = 0;

    memset(str, 0, len * 3 + 100);

    for (int i = 0; i < len-1; i++)
    {
        l += snprintf(str+l, 3 * sizeof(char) * len -l, "%02X ", buf[i]);
    }

    l += snprintf(str + l, 3 * sizeof(char) * len - l, "%02X", buf[len-1]);

    pthread_mutex_lock(&Log::d_lock);

#if 1
    if (fp == NULL) {
        fopen_s(&fp, Log::d_file_name, "a+");

        if (fp == NULL) {
            pthread_mutex_unlock(&Log::d_lock);

            return;
        }
    }

    fprintf(fp, str);
    //fclose(fp);
#endif

    //d_textlog->SetWindowTextW((LPCTSTR)str);
    saveLog(str);

    pthread_mutex_unlock(&Log::d_lock);
}
#endif

static log4cplus::Logger d_log;
static int loglevel = LOG_DEBUG;

void setloggerlevel(int level)
{
    loglevel = level;
}

void getlogger(char *name)
{
#if 1
    //return new Log(log4cplus::Logger::getInstance(name));
    //Log* log = new Log();
    //log->d_log = log4cplus::Logger::getRoot();
    /* step 1: Instantiate an appender object */
    char filename[100] = { 0 };
    snprintf(filename, 99, "%s.log", name);
    //log4cplus::SharedAppenderPtr _append(new log4cplus::FileAppender(filename));
    log4cplus::SharedAppenderPtr _append(new log4cplus::RollingFileAppender(filename,10*1024*1024,3));
    _append->setName(name);
    /* step 2: Instantiate a layout object */
    std::string pattern = "%d{%m/%d/%y %H:%M:%S:%Q}- %p- %m %n";
    /* step 3: Attach the layout object to the appender */
    _append->setLayout((std::unique_ptr<log4cplus::Layout>)new log4cplus::PatternLayout(pattern));
    /* step 4: Instantiate a logger object */
    log4cplus::Logger _logger = log4cplus::Logger::getInstance(name);
    /* step 5: Attach the appender object to the logger  */
    _logger.addAppender(_append);
    /* log activity */
#endif

    d_log = _logger;
}

using namespace log4cplus;
using namespace log4cplus::helpers;

// write log
void log(int loglev,const char* fmt, ... )
{
    if (loglev > loglevel)
        return;
    
    char buffer[500] = { 0 };
    int  len = 0;
    va_list vArgs;

    va_start(vArgs, fmt);
    vsnprintf(buffer + len, sizeof(buffer) - len - 1, (char const *)fmt, vArgs);
    va_end(vArgs);

    switch (loglev)
    {
    case LOG_DEBUG:
        LOG4CPLUS_DEBUG(d_log, buffer);
        break;
    case LOG_NORMAL:
        LOG4CPLUS_WARN(d_log, buffer);
        break;
    case LOG_ERR:
        LOG4CPLUS_ERROR(d_log, buffer);
        break;
    default:
        LOG4CPLUS_DEBUG(d_log, buffer);
        break;
    }
}

void dump(uint8_t *data, int size)
{
    return;
}

#if 0
// get current date with format yyyyMMdd
int currentdate(char * currDate)
{
    struct tm *ptm = NULL;
    time_t tme;
    tme = time(NULL);
    ptm = localtime(&tme);
    char szTime[256];
    memset(szTime, 0, 256);
    sprintf(szTime, "%d%02d%02d", (ptm->tm_year + 1900),
        ptm->tm_mon + 1, ptm->tm_mday);
    strcpy(currDate, szTime);
    return 0;
}

// get current date time with format yyyy-MM-dd hh:mm:ss
int currenttime(char * currTime)
{
    struct tm *ptm = NULL;
    time_t tme;
    tme = time(NULL);
    ptm = localtime(&tme);
    char szTime[256];
    memset(szTime, 0, 256);
    sprintf(szTime, "[%d-%02d-%02d %02d:%02d:%02d] ", (ptm->tm_year + 1900),
        ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    strcpy(currTime, szTime);
    return 0;
}

int filepath(char* path)
{
    char currDate[256];
    memset(currDate, 0, 256);
    currentdate(currDate);
    
    strcat(path, LOGFILEDIR);
    strcat(path, LOGFILEPATH);
    strcat(path, currDate);
    strcat(path, ".log");
    return 0;
}

// write log
void log1(int loglev,const char * format, ... )
{
    if (loglev>LOG_LEVEL) return;

    //CreateDirectory(LOGFILEDIR, NULL);
    
    char temp[1024];
    memset(temp, 0, 1024);
    
    va_list args;
    va_start( args, format );
    vsprintf(temp, format, args );
    va_end( args );
    
    char currTime[256];
    memset(currTime, 0, 256);
    currenttime(currTime);
    
    char logpath[256];
    memset(logpath, 0, 256);
    filepath(logpath);
    
    FILE * pf = fopen(logpath, "aw");

    if (pf)
    {
        fputs(currTime, pf);
        fputs(temp, pf);
        fputs("\n", pf);
        
        fclose(pf);
    }
}

void loghex(int loglev,unsigned char *logmsg,int len)
{
    if (loglev>LOG_LEVEL) 
        return;
    char msg[1024];
    int ilen,i;
    ilen=len;
    if (ilen>512) ilen=512;
    for (i=0;i<ilen;i++)
    {
        sprintf(msg+i*2,"%02X",logmsg[i]);
    }
    log1(loglev,msg);
}
#endif


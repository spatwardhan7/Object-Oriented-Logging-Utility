#ifndef LOGUTILITY_H_INCLUDED
#define LOGUTILITY_H_INCLUDED

#include <pthread.h>
#include <queue>
#include <unistd.h>

#define INIT_LOGGER_OK           0
#define INIT_LOGGER_FAILED       1

#define DESTROY_LOGGER_OK        0
#define DESTROY_LOGGER_FAILED    1

typedef enum
{
    DM_LOG_ERROR,
    DM_LOG_WARNING,
    DM_LOG_TRACE,
    DM_LOG_INFO
}DMLogLevel;


class Logger
{
    public: 
        void DMLog(DMLogLevel logLevel, char *format, ...);
        int initLogger();
        int destroyLogger(); 
        static Logger* Instance();        
    
    private:
        Logger(){};
        Logger(Logger const&){};             // copy constructor is private

        static Logger* m_pInstance;

        int initSuccess; 

        int createTimer(timer_t *timer, int signo);
        int startTimerRep(timer_t *timer, unsigned long time);
        void flushQueue();
        static void * consumer(void *arg);
        int openLogFile();
        int createThread(pthread_t *thread,pthread_attr_t *attr, void *(*start_routine) (void *));

        std::queue<char*> logQueue;
        pthread_mutex_t     queueMutex;
        FILE *fp;
        pthread_t consumer_t;
        timer_t timerid;
};

#endif /* LOGUTILITY_H_INCLUDED*/

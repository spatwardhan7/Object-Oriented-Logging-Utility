#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <queue>
#include <string>

#include "logutility.h"
#include "config.h"

//JUST FOR GIT

#define NSEC_PER_SEC         1000000000
#define MSEC_TO_NSEC         1000000
#define SIG                  SIGUSR1
#define SIGSTOPLOGGER         SIGUSR2

const char * DM_ERROR_MSG    =  "<DM_LOG_ERROR>   :";
const char * DM_WARNING_MSG  =  "<DM_LOG_WARNING> :";
const char * DM_TRACE_MSG    =  "<DM_LOG_TRACE>   :";
const char * DM_INFO_MSG     =  "<DM_LOG_INFO>    :";



Logger* Logger::m_pInstance = NULL; 

Logger* Logger::Instance()
{
    if (!m_pInstance)   // Only allow one instance of class to be generated.
    {
        m_pInstance = new Logger();
    }
    
    return m_pInstance;
}

int Logger::initLogger()
{    
    if(LOG_TO_FILE == 1)
    {
        if(openLogFile())
        {
            return INIT_LOGGER_FAILED;
        }
    }

    pthread_attr_t attr;
    
    /* 
     * Create waitset for sigwait info to wait on and add SIG to this set
     * call pthread_sigmask which is similar to sigprocmask for a pthread
     */
    sigset_t waitset;
    sigemptyset(&waitset);
    sigaddset( &waitset, SIG);
    sigaddset( &waitset, SIGSTOPLOGGER);    
    
    pthread_sigmask(SIG_BLOCK, &waitset, NULL);

    /*
     * Create Consumer thread for consuming log messages 
     * Consumer thread waits on timer and manually generated signals
     * to print messages from log queue
     */


    if(createThread(&consumer_t,&attr,&consumer))
    {
        return INIT_LOGGER_FAILED;
    }
    /* Create timer which generate SIG signal */
    if(createTimer(&timerid,SIG))
    {     
       return INIT_LOGGER_FAILED;
    }

    /* Start Timer to begin generating signals */
    if(startTimerRep(&timerid,TIMER_MS))
    {
        return INIT_LOGGER_FAILED;
    }

    return INIT_LOGGER_OK;
}

int Logger::destroyLogger()
{
    if(timer_delete(timerid) == -1)
    {
        fprintf(stderr,"Timer Delete Failed: %s",strerror(errno));
        return DESTROY_LOGGER_FAILED;        
    }
    printf("Timer Deleted\n");

    if(pthread_kill(consumer_t,SIGSTOPLOGGER))
    {
        fprintf(stderr,"PTHREAD_KILL FAILED\n");
        return DESTROY_LOGGER_FAILED;
    }

    pthread_join(consumer_t,NULL);
    
    return DESTROY_LOGGER_OK;        
}

int Logger::createTimer(timer_t *timer, int signo)
{
    struct sigevent sev;

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_value.sival_ptr = timer;
    sev.sigev_signo = signo;

    if (timer_create(CLOCK_REALTIME, &sev, timer) == -1) 
    {
        fprintf(stderr,"%s\n", strerror(errno));
        return INIT_LOGGER_FAILED;
    }

    return INIT_LOGGER_OK;
}

int Logger::startTimerRep(timer_t *timer, unsigned long time)
{
    int ret;
    struct itimerspec its;

    if (!timer) 
    {
        fprintf(stderr,"Invalid Timer");        
        return INIT_LOGGER_FAILED;
    }

    its.it_value.tv_sec  = (time * MSEC_TO_NSEC ) / NSEC_PER_SEC;
    its.it_value.tv_nsec = (time * MSEC_TO_NSEC ) % NSEC_PER_SEC;

    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    ret = timer_settime(*timer, 0, &its, NULL);

    return ret;
}



void Logger::flushQueue()
{
	pthread_mutex_lock(&queueMutex);	
    while(!logQueue.empty())
    {
        char *str = logQueue.front(); 
	    logQueue.pop();
        if(LOG_TO_FILE == 1)
        {
            fprintf(fp,"%s\n",str);
        }
        if(LOG_TO_CONSOLE == 1)
        {
            printf("%s\n",str);
        }
    }
    fflush(fp);	
    pthread_mutex_unlock(&queueMutex);	    
}


void* Logger::consumer(void *arg)
{
    sigset_t waitset;
    siginfo_t info;

    int stopLogger = 0;

    int result;
    printf("Consumer Thread Started\n");

    sigaddset(&waitset, SIG);
    sigaddset(&waitset, SIGSTOPLOGGER);

    while(stopLogger == 0)
    {
        result = sigwaitinfo( &waitset, &info );
        if( result == SIG )
        {
            m_pInstance->flushQueue();
        }
        else if( result == SIGSTOPLOGGER)
        {
            stopLogger = 1;   
        }        
        else 
        {
            if(errno == EINTR) continue;
            fprintf( stderr,"sigwait() function failed error number %d\n", errno );
        }

    }
    return NULL;
}

int Logger::openLogFile()
{
    if(LOG_TO_FILE == 1)
    {
        time_t t = time(NULL);
	    struct tm tm= *localtime(&t);	
        std::string buffer;
 
        sprintf((char*)buffer.c_str(),"%d-%d-%d_%d-%d-%d.txt", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

    	fp = fopen(buffer.c_str(),"w");
	    if(!fp)
        {
	        fprintf(stderr,"Failed to open log file: %s\n", strerror(errno));
            return INIT_LOGGER_FAILED;   
        }
    }
    return INIT_LOGGER_OK;
}

int Logger::createThread(pthread_t *thread,pthread_attr_t *attr, void *(*start_routine) (void *))
{
        
    if(pthread_attr_init(attr))
    {
	    fprintf(stderr,"Failed to init logger thread: %s\n", strerror(errno));
        return INIT_LOGGER_FAILED;
    }    
    
    if(pthread_create(thread, attr,start_routine, NULL))
    {
	    fprintf(stderr,"Failed to create logger thread: %s\n", strerror(errno));
        return INIT_LOGGER_FAILED;
    }
/*
    if(pthread_detach(*thread))
    {
	    fprintf(stderr,"Failed to detach thread: %s\n", strerror(errno));
        return INIT_LOGGER_FAILED;
    }
*/
    return INIT_LOGGER_OK; 
}


void Logger::DMLog(DMLogLevel logLevel, char *format, ...)
{
    va_list arglist;
    char buffer[256];

    va_start( arglist, format);
    vsprintf( buffer,format, arglist);
    va_end( arglist );

    char *logMessage = (char*)malloc(256);

    if(!logMessage)
    {
        fprintf(stderr,"Malloc failed\n");
        return;
    }

    switch(logLevel)    
    {

	    case DM_LOG_ERROR:
                strcat(logMessage,DM_ERROR_MSG);
                break;

        case DM_LOG_WARNING:
                strcat(logMessage,DM_WARNING_MSG);
                break; 

        case DM_LOG_TRACE:
                strcat(logMessage,DM_TRACE_MSG);                
                break;		

        case DM_LOG_INFO:
                strcat(logMessage,DM_INFO_MSG);
                break;

	    default:
		        break;
    }    
    
    strcat(logMessage,buffer);
  	
    pthread_mutex_lock(&queueMutex);
    logQueue.push(logMessage);   
    pthread_mutex_unlock(&queueMutex);

    if(logLevel == DM_LOG_ERROR)
    {
        if(pthread_kill(consumer_t,SIGUSR1))
        {
            fprintf(stderr,"PTHREAD_KILL FAILED\n");
        }
    }
}


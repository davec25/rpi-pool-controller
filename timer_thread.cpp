/*
 * Dave Cabot
 *
 */

#include <iostream>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>

#include "timer_thread.hpp"

TimerThread::TimerThread() 
{ 
    timer_secs = 0;
    timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &TimerThreadHelper;
    sev.sigev_value.sival_ptr = (void *)this;

    int res = timer_create(CLOCK_REALTIME, &sev, &timerId);
    if (res != 0){
        std::cerr << "Error timer_create: " << strerror(errno);
//        return(-1);
    }

    struct itimerspec its = { 0 };

    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        std::cerr << "Error timer_settime: " << strerror(errno);
//        return(-1);
    }
}

int TimerThread::InitTimerThread(int secs)
{
//    std::cout << "InitTimerThread\n";

    struct itimerspec its = { 0 };

    if (secs < 0) secs = 0;
    its.it_value.tv_sec  = secs;

    int res = 0;
//    std::cout << "Timer thread - thread-id: " << pthread_self() << 
//					", seconds: " << secs << "\n";

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        std::cerr << "Error timer_settime: " << strerror(errno);
        return(-1);
    }

    timer_secs = secs;
    return(0);
}


extern "C" void TimerThreadHelper(union sigval timer_data)
{
    TimerThread *tt = 
		static_cast<TimerThread *>(timer_data.sival_ptr);

    tt->ThreadProcess();
    // DEBUG state change?
    tt->InitTimerThread(tt->GetTimerSecs());
}



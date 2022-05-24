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

int TimerThread::InitTimerThread(int secs)
{
//    std::cout << "InitTimerThread\n";

    if (secs < 1) return(-1);

    int res = 0;
    timer_t timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = { 0 };
    its.it_value.tv_sec  = timer_secs;
//    std::cout << "Timer thread - thread-id: " << pthread_self() << "\n";

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &TimerThreadHelper;
    sev.sigev_value.sival_ptr = (void *)this;

    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);

    if (res != 0){
        std::cerr << "Error timer_create: " << strerror(errno);
        return(-1);
    }

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



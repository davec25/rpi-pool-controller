#ifndef TIMER_THREAD_HPP
#define TIMER_THREAD_HPP

/*
 * Dave Cabot
 *
 */
#include <pthread.h>

extern "C" void TimerThreadHelper(union sigval timer_data);

class TimerThread
{

private:
    pthread_t pid_t;
    int timer_secs;

public:
    TimerThread() { timer_secs = 0; };
    int InitTimerThread(int);
    int GetTimerSecs() { return timer_secs; };
    virtual void ThreadProcess(void) = 0;
};
#endif // TIMER_THREAD_HPP

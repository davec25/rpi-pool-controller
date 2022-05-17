#ifndef HAYWARD_CONTROLLER_HPP
#define HAYWARD_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include <iostream>
#include <pthread.h>
#include <string.h>

class HaywardController
{

private:
pthread_t pid_t;
bool commState;
int tries;
int pump_fd;

struct t_eventData { int myData; };

int InitTimerThread(void);
int SetupPort(const char *device, int baud, int parity);
void TimerThread(void); 
static void *TimerThreadHelper(void *context) 
	{ return ((HaywardController *)context)->TimerThread(void); }
int getPacket(unsigned char buf[], int max);


public:
HaywardController(const char *device, int baud);
~HaywardController();

bool CommState();

};
#endif // HAYWARD_CONTROLLER_HPP

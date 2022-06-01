#ifndef PUMP_CONTROLLER_HPP
#define PUMP_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include "hayward_controller.hpp"

class PumpController : public HaywardController
{

private:
    int percent_speed;
    int watts;

public:
    PumpController(const char *device="/dev/ttyS0", int baud=B19200);

    int SetPumpPercent(int);
    int SetPumpRPMs(int);
    int GetPumpPercent();
    int GetPumpRPMs();
    int GetPumpWatts();

    int ProcessPacket(const unsigned char *, int);

};
#endif // PUMP_CONTROLLER_HPP

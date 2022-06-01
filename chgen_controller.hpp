#ifndef CHGEN_CONTROLLER_HPP
#define CHGEN_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include "hayward_controller.hpp"

class ChGenController : public HaywardController
{

private:
    int percent_gen;
    int ppm;
    int status;

public:
    ChGenController(const char *device="/dev/ttyUSB0", int baud=B9600) : 
					HaywardController(device, baud) {};

    int SetGenPercent(int gen);
    int GetGenPercent() { return percent_gen; };
    int GetPPM() { return ppm; };
    int GetStatus() { return status; };

    int ProcessPacket(const unsigned char *, int);

};
#endif // CHGEN_CONTROLLER_HPP

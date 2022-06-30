/*
 * Dave Cabot
 *
 */

#include "pump_controller.hpp"

PumpController::PumpController(const char *device, int baud) :
                                        	HaywardController(device, baud)
{
// this doesn't set any speed.  it just gets the response with the watts
    const unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;

    percent_speed = -1;
    if (SendPacket(buf, sizeof buf, 60)) {
        std::cerr << "SendPacket failed\n";
        watts = 0;
        exit(-1);
    }
}
            
int PumpController::SetPumpRPMs(int rpm)
{
    if (rpm > 3450 || rpm < 35) {
       std::cerr << "SetPumpRPMs: bad rpms: " << rpm << "\n";
       return (-1);
    }
    int percent = rpm * 100 / 3450;
    SetPumpPercent(percent);
    return (0);
}

int PumpController::GetPumpRPMs()
{
    if (-1 == percent_speed)
        return 0;

    int rpm = 3450 * percent_speed / 100 ;
    return (rpm);
}

int PumpController::StopPump()
{
    unsigned char stop[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x03 } ;
    unsigned char buf[] =  { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;

    if (SendPacket(stop, sizeof stop, 0)) {
        std::cerr << "SendPacket failed\n";
        watts = 0;
        return(-1);
    }

    sleep(5);
    if (SendPacket(buf, sizeof buf, 60)) {
        std::cerr << "SendPacket failed\n";
        watts = 0;
        return(-1);
    }

    percent_speed = 0;
    return(0);
}

int PumpController::SetPumpPercent(int percent)
{
    if (percent < 1 || percent > 100) {
        std::cerr << "SetPumpPercent: bad percentage: " << percent << "\n";
        return (-1);
    }

    if (percent == percent_speed)
        return(0);

    unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;
    percent_speed = percent;
    buf[5] = percent & 0xFF;

//    std::cout << "SetPumpPercent sendpacket\n";

    if (SendPacket(buf, sizeof buf, 3)) {
        std::cerr << "SendPacket failed\n";
        watts = 0;
        return(-1);
    }

    return(0);
}

int PumpController::GetPumpPercent()
{
    if (-1 == percent_speed)
        return 0;

    return(percent_speed);
}


int PumpController::GetPumpWatts()
{
    return (watts);
}

int PumpController::ProcessPacket(const unsigned char *pData, int len)
{
    if (len != 13) {
        std::cerr << "ProcessPacket: packet size unexpected, " << len << "\n";
        percent_speed = 0;
        watts = 0;
        return(-1);
    }

    watts  = (pData[7] >> 4) * 1000;
    watts += (pData[7] & 0xF) * 100;
    watts += (pData[8] >> 4) * 10;
    watts +=  pData[8] & 0xF;

    percent_speed = pData[6];
//    printf("motor at %d%% (%d rpms) and using %x%2.2x  %d watts\n",
//                   GetPumpPercent(), GetPumpRPMs(), pData[7], pData[8], watts);
    return(0);
}


#ifdef PUMP_DEBUG
int main(int argc, char **argv)
{
//declare
    class PumpController pc;

    std::cout << "class initiated\n";

    pc.SetPumpRPMs(1900);
    std::cout << "RPMs set to 1900\n";
    while(1) {
        char a;
        read(0, &a, 1);
        std::cout << "motor speed: " << pc.GetPumpRPMs();
        std::cout << "motor watts: " << pc.GetPumpWatts();
    }
}
#endif // PUMP_DEBUG


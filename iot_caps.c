/* ***************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>                     //Needed for I2C port
#include <fcntl.h>                      //Needed for I2C port
#include <sys/ioctl.h>                  //Needed for I2C port
#include <linux/i2c-dev.h>              //Needed for I2C port
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>



#include "st_dev.h"
#include "iot.h"
#include "relay.h"

#include "caps_switch.h"
#include "caps_switchLevel.h"
#include "caps_powerMeter.h"
#include "caps_thermostatHeatingSetpoint.h"
#include "caps_temperatureMeasurement.h"
#include "caps_voltageMeasurement.h"
#include "caps_relativeHumidityMeasurement.h"
#include "caps_valve.h"
#include "pump_controller.hpp"
#include "chgen_controller.hpp"

#define ENUM_SKIMMER    10 // skimmer
#define ENUM_VACUUM     9  // vacuum
#define ENUM_HEATER     8
#define ENUM_SPA_DRAIN  7  // SPA drain
#define ENUM_SPA_FLOOR  6  // SPA floor
#define ENUM_SPA_JETS   5  // SPA jets
#define ENUM_ACID       4  // acid pump
#define ENUM_BLOWER     3
#define ENUM_DRAIN_MAIN 2  // main drain
#define ENUM_PRESET     1  // pump reset

caps_switch_data_t *jacuzzi=NULL, *blower=NULL, *v1=NULL, *v2=NULL, *v3=NULL;
caps_switch_data_t *v4=NULL, *v5=NULL;
caps_switch_data_t *pump=NULL, *cleaner=NULL, *vacuum=NULL;

caps_switchLevel_data_t *pump_level=NULL, *chgen_level=NULL;

caps_powerMeter_data_t *pump_power=NULL;

caps_thermostatHeatingSetpoint_data_t *setTemp=NULL;

caps_temperatureMeasurement_data_t *tempWater=NULL;
caps_temperatureMeasurement_data_t *tempCase=NULL;
caps_temperatureMeasurement_data_t *tempOutside=NULL;

caps_voltageMeasurement_data_t *ionizer=NULL;

//caps_voltageMeasurement_data_t *orp=NULL;
caps_powerMeter_data_t *orp=NULL;

caps_relativeHumidityMeasurement_data_t *relHum=NULL;

caps_valve_data_t *fill=NULL;
//IOT_CAP_HANDLE *jacuzziState=NULL;

IOT_CAP_HANDLE *pH=NULL;

const char setReady[] = "Ready";
const char setNotReady[] = "notReady";
char *isJacuzziReady = (char *)setNotReady;
const float waterAdj = -4.0;

int relayDevice;
double currentPH=7.0;
double voltsIonizer=13.0;
bool heaterUsing = false;
bool pumpWasOn = false;
int pumpLastPercent = 0;
int pumpLastWatts = 0;
bool ChGenWasOn = false;
int ChGenLastPercent = 0;

PumpController *pc;
ChGenController *gc;

int getTemp(double *valF, double *valC, char *file)
{
    int val = -1;
    FILE *fp = fopen(file, "r");
    if (fp) {
	fscanf(fp, "%*x %*x %*x %*x %*x %*x %*x %*x %*x : crc=%*x %*s");
	fscanf(fp, "%*x %*x %*x %*x %*x %*x %*x %*x %*x t=%d", &val);
	fclose(fp);

        if (-1.0 != val) {
            *valC = val/1000.0;
            *valF = *valC * 9.0 / 5.0 + 32.0;
            return(0);
        }
    } else std::cerr << "unable to open file " << file << "\n";
    return(-1);
}

int getWaterTemp(double *valF, double *valC)
{
    char file[46];
    sprintf(file, "/sys/bus/w1/devices/%s/w1_slave", "28-03049779eb93");
    int ret = getTemp(valF, valC, file);
    *valF -= waterAdj;
    return ret;
}

int getCaseTemp(double *valF, double *valC)
{
    char file[46];
    sprintf(file, "/sys/bus/w1/devices/%s/w1_slave", "28-03049779a4d8");
    return getTemp(valF, valC, file);
}

int getOutsideTemp(double *valF, double *valC)
{
    char file[46];
    sprintf(file, "/sys/bus/w1/devices/%s/w1_slave", "28-030497796715");
    return getTemp(valF, valC, file);
}

double SensorAS(int addr)
{
    int file_i2c;
    int length;
    unsigned char buffer[60] = {0};

    //----- OPEN THE I2C BUS -----
    char *filename = (char*)"/dev/i2c-1";
    if ((file_i2c = open(filename, O_RDWR)) < 0)
    {
        //ERROR HANDLING: you can check errno to see what went wrong
        std::cerr << "Failed to open the i2c bus\n";
        return(-1.0);
    }

    if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
    {
        std::cerr << "Failed to acquire bus access and/or talk to slave.\n";
        //ERROR HANDLING; you can check errno to see what went wrong
        close(file_i2c);
        return(-1.0);
    }

    //----- READ BYTES -----
    buffer[0]='R'; buffer[1]=0;
    write(file_i2c, buffer, 2);

    length = 31; 
    sleep(2);
    if (read(file_i2c, buffer, length) != length)           
    {
        //ERROR HANDLING: i2c transaction failed
        std::cerr << "Failed to read from the i2c bus.\n";
    }
    else
    {
        if (1 == buffer[0])
        {
            float val = -1.0;
            sscanf((const char *)&buffer[1], "%f", &val);
            close(file_i2c);
            return(val);
        }
    }

    close(file_i2c);
    return(-1.0);
}

double getORP()
{
    return round(SensorAS(0x62) * 10.0)/10.0;
}

double getPH()
{
    return round(SensorAS(0x63) * 10.0)/10.0;
}

double getHumidity()
{
    return SensorAS(0x6F);
}

void cap_pH_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    currentPH = getPH();

    /* Send initial pH attribute */
    ST_CAP_SEND_ATTR_NUMBER(handle, "pH", currentPH, NULL, NULL, sequence_no);

    if (sequence_no < 0)
        std::cerr << "fail to send pH value\n";
}

void cap_voltage_init_cb_orp(struct caps_switch_data *caps_data)
{
    double val = getORP();

//    orp->set_voltage_value(orp, val);
//    orp->set_voltage_unit(orp,
//                        caps_helper_voltageMeasurement.attr_voltage.unit_V);
    orp->set_power_value(orp, val);
    orp->set_power_unit(orp,
                        caps_helper_powerMeter.attr_power.unit_W);
}

void cap_voltage_init_cb_ionizer(struct caps_switch_data *caps_data)
{
    ionizer->set_voltage_value(ionizer, voltsIonizer);
    ionizer->set_voltage_unit(ionizer,
                        caps_helper_voltageMeasurement.attr_voltage.unit_V);
}

void cap_relHum_init_cb(struct caps_switch_data *caps_data)
{
    double val = getHumidity();
    relHum->set_humidity_value(relHum,( -1 != val ? val : 0));
    relHum->set_humidity_unit(relHum,  caps_helper_relativeHumidityMeasurement.attr_humidity.unit_percent);
}
 
void cap_temperature_init_cb_case(struct caps_switch_data *caps_data)
{
    double valF = -1.0, valC = -1.0;

    getCaseTemp(&valF, &valC);
    tempCase->set_temperature_value(tempCase, valF);
    tempCase->set_temperature_unit(tempCase,
    		caps_helper_temperatureMeasurement.attr_temperature.unit_F);
}

void cap_temperature_init_cb_water(struct caps_switch_data *caps_data)
{
    double valF = -1.0, valC = -1.0;

    getWaterTemp(&valF, &valC);
    tempWater->set_temperature_value(tempWater, valF);
    tempWater->set_temperature_unit(tempWater,
    		caps_helper_temperatureMeasurement.attr_temperature.unit_F);
}

void cap_temperature_init_cb_outside(struct caps_switch_data *caps_data)
{
    double valF = -1.0, valC = -1.0;

    getOutsideTemp(&valF, &valC);
    tempOutside->set_temperature_value(tempOutside, valF);
    tempOutside->set_temperature_unit(tempOutside,
    		caps_helper_temperatureMeasurement.attr_temperature.unit_F);
}

void SetJacuzziNotReady() 
{
    int32_t sequence_no = 1;
    isJacuzziReady = (char *)setNotReady;
//    ST_CAP_SEND_ATTR_STRING(jacuzziState, "isReady", isJacuzziReady, NULL, NULL, sequence_no);
}

void SetJacuzziReady() 
{
    int32_t sequence_no = 1;
    isJacuzziReady = (char *)setReady;
//    ST_CAP_SEND_ATTR_STRING(jacuzziState, "isReady", isJacuzziReady, NULL, NULL, sequence_no);
}


void StopJacuzzi(void)
{
    if (strcmp(caps_helper_switch.attr_switch.value_off, 
						jacuzzi->switch_value))
        return;

    doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 0);
    doRelayWrite(relayDevice, ENUM_SPA_FLOOR, 0);
    doRelayWrite(relayDevice, ENUM_SPA_JETS, 0);
    doRelayWrite(relayDevice, ENUM_HEATER, 0);
    heaterUsing = 0;
}

void JacuzziSwitchOff(void)
{
    if (strcmp(caps_helper_switch.attr_switch.value_off, 
						jacuzzi->switch_value))
        return;

    jacuzzi->set_switch_value(jacuzzi, 
				caps_helper_switch.attr_switch.value_off);
    jacuzzi->attr_switch_send(jacuzzi);
}

static void cap_switch_cmd_cb_pump(struct caps_switch_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!caps_data->switch_value) {
        std::cerr << "value is NULL\n";
        return;
    }

    std::cout << "Pump switch state " << caps_data->switch_value << "\n";
    if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						caps_data->switch_value)) {
        pc->StopPump();
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on,
						caps_data->switch_value)) {
        if (!pumpLastPercent)
            pumpLastPercent = 55;
        pc->SetPumpPercent(pumpLastPercent);
    } else {
        std::cerr << "unknown switch state " << caps_data->switch_value << "\n";
        return;
    }
}

static void cap_switch_init_cb_pump(struct caps_switch_data *caps_data)
{
    caps_data->cmd_on_usr_cb = cap_switch_cmd_cb_pump;
    caps_data->cmd_off_usr_cb = cap_switch_cmd_cb_pump;

    caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
}

static void cap_switch_cmd_cb_cleaner(struct caps_switch_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!caps_data->switch_value) {
        std::cerr << "value is NULL\n";
        return;
    }

    std::cout << "Cleaner switch state " << caps_data->switch_value << "\n";
    if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						caps_data->switch_value)) {
        pc->StopPump();
// FIXME
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on,
						caps_data->switch_value)) {
// turn off jacuzzi
        StopJacuzzi();
        JacuzziSwitchOff();

        doRelayWrite(relayDevice, ENUM_DRAIN_MAIN, 1);// main drain open
        doRelayWrite(relayDevice, ENUM_SKIMMER, 1);   // skimmer open
        doRelayWrite(relayDevice, ENUM_VACUUM, 0);    // vacuum closed
        pc->SetPumpRPMs(1900);
    } else {
        std::cerr << "unknown switch state " << caps_data->switch_value << "\n";
        return;
    }
}

static void cap_switch_init_cb_cleaner(struct caps_switch_data *caps_data)
{
    caps_data->cmd_on_usr_cb = cap_switch_cmd_cb_cleaner;
    caps_data->cmd_off_usr_cb = cap_switch_cmd_cb_cleaner;

    caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
}

static void cap_switch_cmd_cb_vacuum(struct caps_switch_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!caps_data->switch_value) {
        std::cerr << "value is NULL\n";
        return;
    }

    std::cout << "Vacuum switch state " << caps_data->switch_value << "\n";
    if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						caps_data->switch_value)) {
        pc->StopPump();
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on,
						caps_data->switch_value)) {
// turn off jacuzzi
        StopJacuzzi();
        JacuzziSwitchOff();

        doRelayWrite(relayDevice, ENUM_DRAIN_MAIN, 0);// main drain closed
        doRelayWrite(relayDevice, ENUM_SKIMMER, 0);   // skimmer closed
        doRelayWrite(relayDevice, ENUM_VACUUM, 1);    // vacuum open
        pc->SetPumpRPMs(1900); // speed for the vacuum
    } else {
        std::cerr << "unknown switch state " << caps_data->switch_value << "\n";
        return;
    }
}

static void cap_switch_init_cb_vacuum(struct caps_switch_data *caps_data)
{
    caps_data->cmd_on_usr_cb = cap_switch_cmd_cb_vacuum;
    caps_data->cmd_off_usr_cb = cap_switch_cmd_cb_vacuum;

    caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
}

void StopFill(union sigval timer_data)
{
    if (!strcmp(caps_helper_switch.attr_switch.value_on, 
						jacuzzi->switch_value)) {
        doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 1);
        doRelayWrite(relayDevice, ENUM_HEATER, 1);
        pc->SetPumpRPMs(1900);
        fill->set_valve_value(fill, caps_helper_valve.attr_valve.value_closed);
        fill->attr_valve_send(fill);
    }
}

void TimerHelper(int secs, void func(union sigval timer_data))
{
    int res = 0;
    timer_t timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = { 0 } ;
    its.it_value.tv_sec  = secs;

    std::cout << "Jacuzzi Timer - thread-id: " << pthread_self() << "\n";
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = func;
    //sev.sigev_value.sival_ptr = &eventData;

    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);

    if (res != 0){
        std::cerr << "Error timer_create: " << strerror(errno) << "\n" ;
        return;
    }

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        std::cerr << "Error timer_settime: " << strerror(errno) << "\n" ;
        return;
    }
    return;
}

void StartFillTimer(void)
{
    TimerHelper(900, StopFill);
}

static void cap_valve_cmd_cb_jacuzzi(struct caps_valve_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!strcmp(caps_helper_valve.attr_valve.value_closed, 
						caps_data->valve_value)) {
        
        if (!strcmp(caps_helper_switch.attr_switch.value_on, 
						jacuzzi->switch_value)) {
            doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 1);
            pc->SetPumpRPMs(1900);
        }
    } else if (!strcmp(caps_helper_valve.attr_valve.value_open, 
						caps_data->valve_value)) {
        if (!strcmp(caps_helper_switch.attr_switch.value_on, 
						jacuzzi->switch_value)) {
            doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 0);
            doRelayWrite(relayDevice, ENUM_HEATER, 1);
            pc->SetPumpPercent(100);
            StartFillTimer();
        }
    }
}

static void cap_level_cmd_cb_pump(struct caps_switchLevel_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    std::cout << "Pump switchLevel " << caps_data->level_value << "\n";
    caps_data->level_value = pumpLastPercent;
    if (caps_data->level_value)
        pc->SetPumpPercent(caps_data->level_value);
    else
        pc->StopPump();
    pumpLastPercent = caps_data->level_value;

    if (caps_data->level_value) {
        if (!strcmp(caps_helper_switch.attr_switch.value_off, 
							pump->switch_value)) {
            pump->set_switch_value(pump, 
				caps_helper_switch.attr_switch.value_on);
            pump->attr_switch_send(pump);
        }
    } else {
        if (!strcmp(caps_helper_switch.attr_switch.value_on, 
							pump->switch_value)) {
            pump->set_switch_value(pump, 
				caps_helper_switch.attr_switch.value_on);
            pump->attr_switch_send(pump);
        }
    }
    pump_level->attr_level_send(pump_level);
}

static void cap_level_init_cb_pump(struct caps_switchLevel_data *caps_data)
{
    pump_level->cmd_setLevel_usr_cb = cap_level_cmd_cb_pump;
    pump_level->set_level_value(pump_level, 0);
}

static void cap_level_cmd_cb_chgen(struct caps_switchLevel_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    std::cout << "ChGen switchLevel " << caps_data->level_value << "\n";
//    gc->SetGenPercent(caps_data->level_value);
//    ChGenLastPercent = caps_data->level_value;
    caps_data->level_value = ChGenLastPercent;

#ifdef NOT
    if (caps_data->level_value) {
        if (!strcmp(caps_helper_switch.attr_switch.value_off, 
							chgen->switch_value)) {
            chgen->set_switch_value(chgen, 
				caps_helper_switch.attr_switch.value_on);
            chgen->attr_switch_send(chgen);
        }
    } else {
        if (!strcmp(caps_helper_switch.attr_switch.value_on, 
							chgen->switch_value)) {
            chgen->set_switch_value(chgen, 
				caps_helper_switch.attr_switch.value_on);
            chgen->attr_switch_send(chgen);
        }
    }
#endif
}

static void cap_level_init_cb_chgen(struct caps_switchLevel_data *caps_data)
{
    chgen_level->cmd_setLevel_usr_cb = cap_level_cmd_cb_chgen;
    chgen_level->set_level_value(chgen_level, 0);
}


void StartHeater(union sigval timer_data)
{
    heaterUsing = true;
}

void StartJacuzziSeries(void)
{
    SetJacuzziNotReady();

    if (!strcmp(caps_helper_valve.attr_valve.value_closed,
                                                fill->valve_value)) {
        pc->SetPumpRPMs(1900);
        doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 1);
    } else {
        pc->SetPumpPercent(100);
        doRelayWrite(relayDevice, ENUM_SPA_DRAIN, 0);
        StartFillTimer();
    }
    doRelayWrite(relayDevice, ENUM_SPA_FLOOR, 1);
    doRelayWrite(relayDevice, ENUM_SPA_JETS, 1);

    TimerHelper(20, StartHeater);
}

static void cap_switch_cmd_cb_jacuzzi(struct caps_switch_data *caps_data)
{
 //   int32_t sequence_no = 1;

    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!caps_data->switch_value) {
        std::cerr << "value is NULL\n";
        return;
    }

    std::cerr << "Jacuzzi switch state " << caps_data->switch_value << "\n";
    if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						caps_data->switch_value)) {
        StopJacuzzi();
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on,
						caps_data->switch_value)) {
        StartJacuzziSeries();
    } else {
        std::cerr << "unknown switch state " << caps_data->switch_value << "\n";
        return;
    }
}

static void cap_switch_init_cb_jacuzzi(struct caps_switch_data *caps_data)
{
    caps_data->cmd_on_usr_cb = cap_switch_cmd_cb_jacuzzi;
    caps_data->cmd_off_usr_cb = cap_switch_cmd_cb_jacuzzi;

    caps_data->set_switch_value(jacuzzi, 
				caps_helper_switch.attr_switch.value_off);
    SetJacuzziNotReady();
}

static void cap_valve_init_cb_jacuzzi(struct caps_valve_data *caps_data)
{
    caps_data->cmd_open_usr_cb = cap_valve_cmd_cb_jacuzzi;
    caps_data->cmd_close_usr_cb = cap_valve_cmd_cb_jacuzzi;

    caps_data->set_valve_value(fill, 
				caps_helper_valve.attr_valve.value_closed);
}

void cap_setpoint_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t val=95.0;
    setTemp->set_heatingSetpoint_value(setTemp, val);
    setTemp->heatingSetpoint_unit =
      (char*)caps_helper_thermostatHeatingSetpoint.attr_heatingSetpoint.unit_F;
}

static void cap_switch_cmd_cb_blower(struct caps_switch_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        std::cerr << "fail to get handle\n";
        return;
    }

    if (!caps_data->switch_value) {
        std::cerr << "value is NULL\n";
        return;
    }

    if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						jacuzzi->switch_value)) {
        caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
        return;
    }

    std::cerr << "blower switch state " << caps_data->switch_value << "\n";
    if (!strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
        doRelayWrite(relayDevice, ENUM_BLOWER, 0);
        pc->SetPumpRPMs(1900);
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on, 
						caps_data->switch_value)) {
// if the jacuzzi isn't on, I'm not going to let you turn on the blower
        if (!strcmp(caps_helper_switch.attr_switch.value_off, 
						jacuzzi->switch_value)) {
            caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
            return;
        }

        doRelayWrite(relayDevice, ENUM_BLOWER, 1);
        pc->SetPumpPercent(100);
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
    }
}

static void cap_switch_init_cb_blower(struct caps_switch_data *caps_data)
{
    caps_data->cmd_on_usr_cb = cap_switch_cmd_cb_blower;
    caps_data->cmd_off_usr_cb = cap_switch_cmd_cb_blower;
    if (doRelayRead(relayDevice,ENUM_BLOWER)) {
        caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_on);
    } else {
        caps_data->set_switch_value(caps_data, 
				caps_helper_switch.attr_switch.value_off);
    }
}

void StopAcid(union sigval timer_data)
{
    doRelayWrite(relayDevice, ENUM_ACID, 0);
}

void DeployAcid(int secs)
{
    doRelayWrite(relayDevice, ENUM_ACID, 1);
    TimerHelper(secs, StopAcid);
}

void ManagePH(void)
{
    if (pH) {
        cap_pH_init_cb(pH, NULL);

        if (currentPH < 6.7) {
            gc->SetGenPercent(101);
        } else if (currentPH < 6.8) {
            gc->SetGenPercent(80);
        } else if (currentPH < 6.9) {
            gc->SetGenPercent(60);
        } else if (currentPH < 7.0) {
            gc->SetGenPercent(40);
        } else if (currentPH < 7.1) {
            gc->SetGenPercent(20);
        } else if (currentPH < 7.4) {
            gc->SetGenPercent(0);
        } else if (currentPH < 7.5) {
            gc->SetGenPercent(0);
            DeployAcid(1);
        } else if (currentPH < 7.6) {
            gc->SetGenPercent(0);
            DeployAcid(2);
        } else {
            gc->SetGenPercent(0);
            DeployAcid(3);
        }
    } 
}

void UpdateChGenDisplay(void)
{
    int percent = gc->GetGenPercent();
    std::cout << "Gen%: " << percent << "%, Status: " << 
						gc->GetStatus() << "\n";

    if (percent) {
        if (!ChGenWasOn) {
//            chgen->set_switch_value(chgen, 
//				caps_helper_switch.attr_switch.value_on);
//            chgen->attr_switch_send(chgen);
        }
        ChGenWasOn = true;
        if (percent != ChGenLastPercent) {
            chgen_level->set_level_value(chgen_level, percent);
            chgen_level->attr_level_send(chgen_level);
        }
        ChGenLastPercent = percent;
        return;
    } 

    if (ChGenWasOn) {
//        chgen->set_switch_value(chgen, 
//				caps_helper_switch.attr_switch.value_off);
//        chgen->attr_switch_send(chgen);

        chgen_level->set_level_value(chgen_level, 0);
        chgen_level->attr_level_send(chgen_level);
    } 
    ChGenWasOn = false;
    ChGenLastPercent = 0;
}

void poll_sensors(union sigval timer_data)
{
    std::cout << "poll_sensors: thread-id: " << pthread_self() << "\n";

    int pumpP = pc->GetPumpPercent();
    bool pumpOn = pumpP > 0;

    if (pumpLastWatts != pc->GetPumpWatts()) {
        pumpLastWatts = pc->GetPumpWatts();
        pump_power->set_power_value(pump_power, pumpLastWatts);
        pump_power->attr_power_send(pump_power);
    }

    if (!pc->CommState()) {
        std::cerr << "pump communications down\n";
        pumpOn = false;
    }

    if (!gc->CommState()) {
        std::cerr << "chlorinator communications is down\n";
    }

    if (tempCase && tempCase->handle) {
	double valC, valF;
        int32_t sequence_no = 1;

        if (-1 != getCaseTemp(&valF, &valC)) {
            tempCase->set_temperature_value(tempCase, valF);
            tempCase->attr_temperature_send(tempCase);
        }
    }

    if (relHum && relHum->handle) {
	double val=getHumidity();

        if (-1.0 != val) {
            relHum->set_humidity_value(relHum, val);
            relHum->attr_humidity_send(relHum);
        }
    }

    if (tempOutside && tempOutside->handle) {
	double valC, valF;

        if (-1 != getOutsideTemp(&valF, &valC)) {
            tempOutside->set_temperature_value(tempOutside, valF);
            tempOutside->attr_temperature_send(tempOutside);
        }
    }

    if (!pumpOn) {
        if (pumpWasOn) {
            int32_t sequence_no = 1;

            pump->set_switch_value(pump, 
				caps_helper_switch.attr_switch.value_on);
            pump->attr_switch_send(pump);

            pump_level->set_level_value(pump_level, 0);
            pump_level->attr_level_send(pump_level);

//            chgen->set_switch_value(chgen, 
//				caps_helper_switch.attr_switch.value_on);
//            chgen->attr_switch_send(chgen);
        }
        pumpWasOn = false;
        pumpLastPercent = 0;

        if (ChGenWasOn) {
//            chgen->set_switch_value(chgen, 
//				caps_helper_switch.attr_switch.value_off);
//            chgen->attr_switch_send(chgen);

            chgen_level->set_level_value(chgen_level, 0);
            chgen_level->attr_level_send(chgen_level);
        } 
        ChGenWasOn = false;
        ChGenLastPercent = 0;
        return;
    }

    if (!pumpWasOn) {
        pump->set_switch_value(pump, 
				caps_helper_switch.attr_switch.value_on);
        pump->attr_switch_send(pump);
    }
    pumpWasOn = true;

    if (pumpLastPercent != pumpP) {
        pump_level->set_level_value(pump_level, pumpP);
        pump_level->attr_level_send(pump_level);
    }
    pumpLastPercent = pumpP;

    double valC, valF;
    if (tempWater && tempWater->handle) {

        int val = getWaterTemp(&valF, &valC);
        if (-1 != val) {
            tempWater->set_temperature_value(tempWater, valF);
            tempWater->attr_temperature_send(tempWater);
        }
    }

    if (heaterUsing) {
        float heaterTemp = setTemp->get_heatingSetpoint_value(setTemp);
        if (valF < (heaterTemp - 0.5) ) {
            doRelayWrite(relayDevice, ENUM_HEATER, 1);
        } else if (valF > heaterTemp) {
            doRelayWrite(relayDevice, ENUM_HEATER, 0);
            SetJacuzziReady();
        }
    }
        
    if (orp && orp->handle ) {
	double val=getORP();
        int32_t sequence_no = 1;

        if (-1.0 != val) {
            orp->set_power_value(orp, val);
            orp->attr_power_send(orp);
//            orp->set_voltage_value(orp, val);
//            orp->attr_voltage_send(orp);
        }
    }

    ManagePH();

    UpdateChGenDisplay();
}

int init_polling_thread()
{
    std::cout << "init_polling_thread() \n";

    int res = 0;
    timer_t timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = { 0 } ;
    its.it_interval.tv_sec  = 60;
    its.it_value.tv_sec  = 60;

    std::cout << "Simple Threading Timer - thread-id: " << pthread_self() << "\n";
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &poll_sensors;
    //sev.sigev_value.sival_ptr = &eventData;

    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);

    if (res != 0){
        std::cerr << "Error timer_create: " << strerror(errno) << "\n" ;
        return(-1);
    }

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        std::cerr << "Error timer_settime: " << strerror(errno) << "\n" ;
        return(-1);
    }
    return(0);
}

void iot_caps_setup(IOT_CTX *ctx)
{
    IOT_CAP_HANDLE *handle = NULL;
    int iot_err = st_conn_set_noti_cb(ctx, iot_noti_cb, NULL);
    if (iot_err)
        std::cerr << "fail to set notification callback function\n";

    relayDevice = doBoardInit(0);
    if (ERROR == iot_err)
        std::cerr << "fail to setup relay board\n";

//    orp = caps_voltageMeasurement_initialize(ctx, "Water", 
    orp = caps_powerMeter_initialize(ctx, "Water", 
					(void*)cap_voltage_init_cb_orp, NULL);

    pH = st_cap_handle_init(ctx, "Water", "pHMeasurement", 
							cap_pH_init_cb, NULL);

    jacuzzi = caps_switch_initialize(ctx, "Jacuzzi", 
				(void*)cap_switch_init_cb_jacuzzi, NULL);

    fill = caps_valve_initialize(ctx, "Jacuzzi", 
				(void*)cap_valve_init_cb_jacuzzi, NULL);

//    jacuzziState = st_cap_handle_init(ctx, "Jacuzzi", 
//			"heartcircle52521.jacuzzistate", NULL, NULL);

#ifdef NOT
    chgen = caps_switch_initialize(ctx, "Chlorinator", 
					(void*)cap_switch_init_cb_chgen, NULL);
    if (!chgen)
        std::cerr << "caps_switch_initialize failed for ChGen\n";
#endif

    chgen_level = caps_switchLevel_initialize(ctx, "Chlorinator", 
					(void*)cap_level_init_cb_chgen, NULL);
    if (!chgen_level)
        std::cerr << "caps_switchLevel_initialize failed for ChGen\n";

    pump = caps_switch_initialize(ctx, "Pump", 
					(void*)cap_switch_init_cb_pump, NULL);
    if (!pump)
        std::cerr << "caps_switch_initialize failed for Pump\n";

    pump_level = caps_switchLevel_initialize(ctx, "Pump", 
					(void*)cap_level_init_cb_pump, NULL);
    if (!pump_level)
        std::cerr << "caps_switchLevel_initialize failed for Pump\n";

    pump_power = caps_powerMeter_initialize(ctx, "Pump", NULL, NULL);
    if (!pump_power)
        std::cerr << "caps_powerMeter_initialize failed for Pump\n";

    cleaner = caps_switch_initialize(ctx, "Clean", 
				(void*)cap_switch_init_cb_cleaner, NULL);
    if (!cleaner)
        std::cerr << "caps_switch_initialize failed for Cleaner\n";

    vacuum = caps_switch_initialize(ctx, "Vacuum", 
				(void*)cap_switch_init_cb_vacuum, NULL);
    if (!vacuum)
        std::cerr << "caps_switch_initialize failed for Vacuum\n";

    relHum = caps_relativeHumidityMeasurement_initialize(ctx, "Outside", 
					(void*)cap_relHum_init_cb, NULL);

    setTemp = caps_thermostatHeatingSetpoint_initialize(ctx, "Jacuzzi", 
				(void*)cap_setpoint_init_cb, NULL);

    tempWater = caps_temperatureMeasurement_initialize(ctx, "Water", 
				(void*)cap_temperature_init_cb_water, NULL);
    if (!tempWater)
    	std::cerr << "no water temp\n";

    tempCase = caps_temperatureMeasurement_initialize(ctx, "Case", 
				(void*)cap_temperature_init_cb_case, NULL);
    if (!tempCase) 
        std::cerr << "no case temp\n";

    tempOutside = caps_temperatureMeasurement_initialize(ctx, "Outside", 
				(void*)cap_temperature_init_cb_outside, NULL);
    if (!tempOutside) 
        std::cerr << "no outside temp\n";

    blower = caps_switch_initialize(ctx, "Blower", 
				(void*)cap_switch_init_cb_blower, NULL);

    ionizer = caps_voltageMeasurement_initialize(ctx, "Ionizer", 
				(void*)cap_voltage_init_cb_ionizer, NULL);

    pc = new PumpController();
    gc = new ChGenController();

    init_polling_thread();
}

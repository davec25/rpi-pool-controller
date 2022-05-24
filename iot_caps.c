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
#include "caps_thermostatHeatingSetpoint.h"
#include "caps_temperatureMeasurement.h"
#include "caps_voltageMeasurement.h"
#include "caps_relativeHumidityMeasurement.h"
#include "pump_controller.hpp"
#include "chgen_controller.hpp"

#define ENUM_HEATER 8
#define ENUM_VALVE1 8
#define ENUM_VALVE2 8
#define ENUM_VALVE3 8
#define ENUM_VALVE4 8
#define ENUM_VALVE5 8

caps_switch_data_t *jacuzzi=NULL, *blower=NULL, *v1=NULL, *v2=NULL, *v3=NULL;
caps_thermostatHeatingSetpoint_data_t *setTemp=NULL;
caps_temperatureMeasurement_data_t *tempWater=NULL;
caps_temperatureMeasurement_data_t *tempCase=NULL;
caps_temperatureMeasurement_data_t *tempOutside=NULL;
caps_voltageMeasurement_data_t *orp=NULL;
caps_relativeHumidityMeasurement_data_t *relHum=NULL;
IOT_CAP_HANDLE jacuzziState=NULL;
IOT_CAP_HANDLE pH=NULL;
IOT_CAP_HANDLE mainHealth=NULL;
const char setReady[] = "Ready";
const char setNotReady[] = "notReady";
char *isJacuzziReady = (char *)setNotReady;

int relayDevice;

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
    } else printf("unable to open file %s\n", file);
    return(-1);
}

int getWaterTemp(double *valF, double *valC)
{
    char file[46];
    sprintf(file, "/sys/bus/w1/devices/%s/w1_slave", "28-03049779eb93");
    return getTemp(valF, valC, file);
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
        printf("Failed to open the i2c bus");
        return(-1.0);
    }

    if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
    {
        printf("Failed to acquire bus access and/or talk to slave.\n");
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
        printf("Failed to read from the i2c bus.\n");
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
    double t=getPH();

    /* Send initial pH attribute */
    ST_CAP_SEND_ATTR_NUMBER(handle, "acidity", t, NULL, NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send pH value\n");
}

void cap_main_health_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    bool pumpOn = (pc->GetPumpPercent() > 0);

    /* Send attribute */
    if (pumpOn) {
        ST_CAP_SEND_ATTR_STRING(handle, "healthStatus", 
				(char *)"online", NULL, NULL, sequence_no);
    } else {
        ST_CAP_SEND_ATTR_STRING(handle, "healthStatus", 
				(char *)"offline", NULL, NULL, sequence_no);
    }

    if (sequence_no < 0)
        printf("fail to send main health status\n");
}

void SetJacuzziNotReady() 
{
    int32_t sequence_no = 1;
    isJacuzziReady = (char *)setNotReady;
    ST_CAP_SEND_ATTR_STRING(&jacuzziState, "isReady", isJacuzziReady, NULL, NULL, sequence_no);
}

void SetJacuzziReady() 
{
    int32_t sequence_no = 1;
    isJacuzziReady = (char *)setReady;
    ST_CAP_SEND_ATTR_STRING(&jacuzziState, "isReady", isJacuzziReady, NULL, NULL, sequence_no);
}


void cap_JS_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;

    SetJacuzziNotReady();
    if (sequence_no < 0)
        printf("fail to send Jacuzzi:isReady state\n");
}


void cap_temp_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    float t=20.3;

    /* Send initial temp attribute */
    ST_CAP_SEND_ATTR_NUMBER(handle, "temperature", t, "F", NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send temp value\n");
}

void cap_CGswitch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;

    /* Send initial switch attribute */
    ST_CAP_SEND_ATTR_STRING(handle, "switch", 
					(char *)"on", NULL, NULL, sequence_no)

    if (sequence_no < 0)
        printf("fail to send CGswitch value\n");
}

void cap_CGflow_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;

    /* Send initial switch attribute */
    ST_CAP_SEND_ATTR_STRING(handle, "water", 
				(char *)"dry", NULL, NULL, sequence_no)

    if (sequence_no < 0)
        printf("fail to send CGflow value\n");
}

void cap_Hswitch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;

    /* Send initial switch attribute */
    ST_CAP_SEND_ATTR_STRING(handle, "switch", 
					(char *)"on", NULL, NULL, sequence_no)

    if (sequence_no < 0)
        printf("fail to send Hswitch value\n");
}

void cap_setTemp_cmd_cb(struct caps_thermostatHeatingSetpoint_data *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        printf("fail to get handle\n");
        return;
    }

    int32_t sequence_no = 1;

    /* Send initial switch attribute */
    setTemp->heatingSetpoint_value = caps_data->heatingSetpoint_value;
    ST_CAP_SEND_ATTR_NUMBER(caps_data->handle, "heatingSetpoint",
		setTemp->heatingSetpoint_value, setTemp->heatingSetpoint_unit, 
		NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send HsetPoint value\n");
}

void cap_setTempUnit_cmd_cb(
		struct caps_thermostatHeatingSetpoint_data *caps_data, 
							const char *unit)
{
    if (!caps_data || !caps_data->handle) {
        printf("fail to get handle\n");
        return;
    }

    if (!unit) {
        printf("fail to get unit\n");
        return;
    }

    int32_t sequence_no = 1;

    setTemp->heatingSetpoint_unit = (char*)unit;
    /* Send initial attribute */
    ST_CAP_SEND_ATTR_NUMBER(caps_data->handle, "heatingSetpointUnit", 
	setTemp->heatingSetpoint_value, caps_data->heatingSetpoint_unit, 
	NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send HsetUnit value\n");
}

void cap_temp_cmd_cb(caps_temperatureMeasurement_data_t *caps_data)
{
    if (!caps_data || !caps_data->handle) {
        printf("fail to get handle\n");
        return;
    }

    int32_t sequence_no = 1;
    double val=95.0;

    /* Send initial switch attribute */
//    ST_CAP_SEND_ATTR_NUMBER(caps_data->handle, "temperature", val, 
//					caps_data->, NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send temperature value\n");
}

static void cap_switch_cmd_cb_jswitch(struct caps_switch_data *caps_data)
{
    int32_t sequence_no = 1;

    if (!caps_data || !caps_data->handle) {
        printf("fail to get handle\n");
        return;
    }

    if (!caps_data->switch_value) {
        printf("value is NULL\n");
        return;
    }

    printf("Jacuzzi switch state %s\n", caps_data->switch_value);
    if (!strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
// turn heater off
        int val = doRelayWrite(relayDevice,ENUM_HEATER,0);
        SetJacuzziNotReady();
// stop pump
    } else if (!strcmp(caps_helper_switch.attr_switch.value_on,caps_data->switch_value)) {
// maybe not all at once
// set valves
        SetJacuzziNotReady();
// turn on pump
// turn heater on
        int val = doRelayWrite(relayDevice,ENUM_HEATER,1);
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
        return;
    }

    ST_CAP_SEND_ATTR_STRING(caps_data->handle, "switch", caps_data->switch_value, NULL, NULL, sequence_no);
    if (sequence_no < 0)
        printf("fail to send Jswitch value\n");
}

void cap_JsetPoint_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    int32_t val=95.0;

    /* Send initial switch attribute */
    ST_CAP_SEND_ATTR_NUMBER(handle, "heatingSetpoint", val, "F", NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send JsetPoint value\n");
}

void cap_Jtemp_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    float val=77.0;

    /* Send initial switch attribute */
    ST_CAP_SEND_ATTR_NUMBER(handle, "temperature", val, "F", NULL, sequence_no);

    if (sequence_no < 0)
        printf("fail to send Jtemp value\n");
}

void cap_Bswitch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    int val = doRelayRead(relayDevice,8);
    if (-1 == val) {
        printf("failed to get relay 0\n");
        return;
    }

    /* Send initial switch attribute */
    if (val) {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"on", NULL, NULL, sequence_no)
    } else {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"off", NULL, NULL, sequence_no)
    }

    if (sequence_no < 0)
        printf("fail to send Bswitch value\n");
}
void cap_V1switch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    int val = doRelayRead(relayDevice,2);
    if (-1 == val) {
        printf("failed to get relay 0\n");
        return;
    }

    /* Send initial switch attribute */
    if (val) {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
					(char *)"on", NULL, NULL, sequence_no)
    } else {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
					(char *)"off", NULL, NULL, sequence_no)
    }

    if (sequence_no < 0)
        printf("fail to send V1switch value\n");
}
void cap_V2switch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    int val = doRelayRead(relayDevice,3);
    if (-1 == val) {
        printf("failed to get relay 0\n");
        return;
    }

    /* Send initial switch attribute */
    if (val) {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"on", NULL, NULL, sequence_no)
    } else {
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"off", NULL, NULL, sequence_no)
    }

    if (sequence_no < 0)
        printf("fail to send V2switch value\n");
}

void cap_V3switch_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 1;
    int val = doRelayRead(relayDevice,4);
    if (-1 == val) {
        printf("failed to get relay 0\n");
        return;
    }

    /* Send initial switch attribute */
    if (val)
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"on", NULL, NULL, sequence_no)
    else
        ST_CAP_SEND_ATTR_STRING(handle, "switch", 
				(char *)"off", NULL, NULL, sequence_no)

    if (sequence_no < 0)
        printf("fail to send V3switch value\n");
}


static void cap_switch_cmd_cb_blower(struct caps_switch_data *caps_data)
{
    int32_t sequence_no = 1;

    if (!caps_data || !caps_data->handle) {
        printf("fail to get handle\n");
        return;
    }

    if (!caps_data->switch_value) {
        printf("value is NULL\n");
        return;
    }

    printf("blower switch state %s\n", caps_data->switch_value);
    if (0 == strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
        doRelayWrite(relayDevice,1,0);
        /* Update switch attribute */
        ST_CAP_SEND_ATTR_STRING(caps_data->handle, "switch", 
					(char *)"off", NULL, NULL, sequence_no)
    } else if (0 == strcmp(caps_helper_switch.attr_switch.value_on,caps_data->switch_value)) {
        doRelayWrite(relayDevice,1,1);
        /* Update switch attribute */
        ST_CAP_SEND_ATTR_STRING(caps_data->handle, "switch", 
					(char *)"on", NULL, NULL, sequence_no)
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
    }

}

static void cap_switch_cmd_cb_v1(struct caps_switch_data *caps_data)
{
    printf("v1 switch state %s\n", caps_data->switch_value);
    if (0 == strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
        doRelayWrite(relayDevice,2,0);
    } else if (0 == strcmp(caps_helper_switch.attr_switch.value_on,caps_data->switch_value)) {
        doRelayWrite(relayDevice,2,1);
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
    }
}

static void cap_switch_cmd_cb_v2(struct caps_switch_data *caps_data)
{
    printf("v2 switch state %s\n", caps_data->switch_value);
    if (0 == strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
        doRelayWrite(relayDevice,3,0);
    } else if (0 == strcmp(caps_helper_switch.attr_switch.value_on,caps_data->switch_value)) {
        doRelayWrite(relayDevice,3,1);
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
    }
}

static void cap_switch_cmd_cb_v3(struct caps_switch_data *caps_data)
{
    printf("v3 switch state %s\n", caps_data->switch_value);
    if (0 == strcmp(caps_helper_switch.attr_switch.value_off, caps_data->switch_value)) {
        doRelayWrite(relayDevice,4,0);
    } else if (0 == strcmp(caps_helper_switch.attr_switch.value_on,caps_data->switch_value)) {
        doRelayWrite(relayDevice,4,1);
    } else {
        printf("unknown switch state %s\n", caps_data->switch_value);
    }
}

void poll_sensors(union sigval timer_data)
{
    printf("Timer fired - thread-id: %d\n", pthread_self());

    bool pumpOn = (pc->GetPumpPercent() > 0);
    if (tempWater && tempWater->handle && pumpOn) {
	double valC, valF;
        int32_t sequence_no = 1;

        if (-1 != getWaterTemp(&valF, &valC)) {
            tempWater->set_temperature_value(tempWater, valF);
            ST_CAP_SEND_ATTR_NUMBER(tempWater->handle, "temperature", 
			valF, tempWater->temperature_unit, NULL, sequence_no)
        }
    }

    if (tempCase && tempCase->handle) {
	double valC, valF;
        int32_t sequence_no = 1;

        if (-1 != getCaseTemp(&valF, &valC)) {
            tempCase->set_temperature_value(tempCase, valF);
            ST_CAP_SEND_ATTR_NUMBER(tempCase->handle, "temperature", 
			valF, tempCase->temperature_unit, NULL, sequence_no)
        }
    }

    if (tempOutside && tempOutside->handle) {
	double valC, valF;
        int32_t sequence_no = 1;

        if (-1 != getOutsideTemp(&valF, &valC)) {
            tempOutside->set_temperature_value(tempOutside, valF);
            ST_CAP_SEND_ATTR_NUMBER(tempOutside->handle, "temperature", valF, tempOutside->temperature_unit, NULL, sequence_no);
        }
    }

    if (orp && orp->handle && pumpOn) {
	double val=getORP();
        int32_t sequence_no = 1;

        if (-1.0 != val) {
            orp->set_voltage_value(orp, val);
            ST_CAP_SEND_ATTR_NUMBER(orp->handle, "voltage", val, NULL, NULL, sequence_no);
        }
    }

    if (relHum && relHum->handle) {
	double val=getHumidity();
        int32_t sequence_no = 1;

        if (-1.0 != val) {
            relHum->set_humidity_value(relHum, val);
            ST_CAP_SEND_ATTR_NUMBER(relHum->handle, "humidity", val, 
				relHum->humidity_unit, NULL, sequence_no);
        }
    }

    if (pH && pumpOn) {
        cap_pH_init_cb(&pH, NULL);
    } else {

    }

    if (!strcmp(jacuzzi->switch_value,"On")) {
        if (setTemp->heatingSetpoint_value < tempWater->temperature_value) {
            SetJacuzziReady();
	    // turn heater off
            int val = doRelayWrite(relayDevice,ENUM_HEATER,0);
        } else if (tempWater->temperature_value < 
				(setTemp->heatingSetpoint_value - 1.0)) {
	    // turn heater on
            int val = doRelayWrite(relayDevice,ENUM_HEATER,1);
        }
    }
}

int init_polling_thread()
{
    int res = 0;
    timer_t timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = { 0 } ;
    its.it_value.tv_sec  = 30,

    printf("Simple Threading Timer - thread-id: %d\n", pthread_self());

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &poll_sensors;
    //sev.sigev_value.sival_ptr = &eventData;

    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);

    if (res != 0){
        fprintf(stderr, "Error timer_create: %s\n", strerror(errno));
        return(-1);
    }

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        fprintf(stderr, "Error timer_settime: %s\n", strerror(errno));
        return(-1);
    }
    return(0);
}

void iot_caps_setup(IOT_CTX ctx)
{
    IOT_CAP_HANDLE *handle = NULL;
    int iot_err = st_conn_set_noti_cb(&ctx, iot_noti_cb, NULL);
    if (iot_err)
        printf("fail to set notification callback function\n");

    relayDevice = doBoardInit(0);
    if (ERROR == iot_err)
        printf("fail to setup relay board\n");

    orp = caps_voltageMeasurement_initialize(&ctx, NULL, NULL, NULL);
    if (orp) {
        int32_t sequence_no = 1;
        double val = getORP();

        orp->set_voltage_value(orp, val);
        orp->set_voltage_unit(orp, 
			caps_helper_voltageMeasurement.attr_voltage.unit_V);
    }

    pH = st_cap_handle_init(&ctx, "main", "heartcircle52521.ph", cap_pH_init_cb, NULL);
//    mainHealth = st_cap_handle_init(&ctx, "main", "healthCheck", cap_main_health_init_cb, NULL);

//    handle = st_cap_handle_init(&ctx, "ChlorineGenerator", "switch", cap_CGswitch_init_cb, NULL);
//   handle = st_cap_handle_init(&ctx, "ChlorineGenerator", "waterSensor", cap_CGflow_init_cb, NULL);


    jacuzzi = caps_switch_initialize(&ctx, "Jacuzzi", NULL, NULL);
    if (jacuzzi) {
        int32_t sequence_no = 1;
        jacuzzi->cmd_on_usr_cb = cap_switch_cmd_cb_jswitch;
        jacuzzi->cmd_off_usr_cb = cap_switch_cmd_cb_jswitch;

        jacuzzi->set_switch_value(jacuzzi, caps_helper_switch.attr_switch.value_off);
    }

    jacuzziState = st_cap_handle_init(&ctx, "Jacuzzi", "heartcircle52521.jacuzzistate", cap_JS_init_cb, NULL);


/*    handle = st_cap_handle_init(&ctx, "ChlorineGenerator", "switch", cap_CGswitch_init_cb, NULL);
*/

    relHum = caps_relativeHumidityMeasurement_initialize(&ctx, "Outside", NULL, NULL);
    if (relHum) {
        double val = getHumidity();
        relHum->humidity_value = ( -1 != val ? val : 0);
        relHum->humidity_unit = 
		(char *)caps_helper_relativeHumidityMeasurement.attr_humidity.unit_percent;
    }

    setTemp = caps_thermostatHeatingSetpoint_initialize(&ctx, "Jacuzzi", NULL, NULL);
    if (setTemp) {
        int32_t sequence_no = 1;
        double val = 95.0;
        setTemp->cmd_setHeatingSetpoint_usr_cb = cap_setTemp_cmd_cb;
        setTemp->set_heatingSetpoint_value(setTemp, val);
        setTemp->heatingSetpoint_unit = 
      (char*)caps_helper_thermostatHeatingSetpoint.attr_heatingSetpoint.unit_F;
    }

    tempWater = caps_temperatureMeasurement_initialize(&ctx, "main", NULL, NULL);
    if (tempWater) {
        int32_t sequence_no = 1;
        double valF = -1.0, valC = -1.0;

        getWaterTemp(&valF, &valC);
	tempWater->set_temperature_value(tempWater, valF);
        tempWater->temperature_unit = 
        (char*)caps_helper_temperatureMeasurement.attr_temperature.unit_F;
    } else printf("no water temp\n");

    tempCase = caps_temperatureMeasurement_initialize(&ctx, "Case", NULL, NULL);
    if (tempCase) {
        int32_t sequence_no = 1;
        double valF = -1.0, valC = -1.0;

        getCaseTemp(&valF, &valC);
	tempCase->set_temperature_value(tempCase, valF);
        tempCase->temperature_unit = 
        (char*)caps_helper_temperatureMeasurement.attr_temperature.unit_F;
    } else printf("no water temp\n");

    tempOutside = caps_temperatureMeasurement_initialize(&ctx, "Outside", NULL, NULL);
    if (tempOutside) {
        int32_t sequence_no = 1;
        double valF = -1.0, valC = -1.0;

        getOutsideTemp(&valF, &valC);
	tempOutside->set_temperature_value(tempOutside, valF);
        tempOutside->temperature_unit = 
        (char*)caps_helper_temperatureMeasurement.attr_temperature.unit_F;
    } else printf("no water temp\n");

    blower = caps_switch_initialize(&ctx, "Blower", NULL, NULL);
    if (blower) {
        blower->cmd_on_usr_cb = cap_switch_cmd_cb_blower;
        blower->cmd_off_usr_cb = cap_switch_cmd_cb_blower;

        const char *switch_init_value = caps_helper_switch.attr_switch.value_off;
        int val = doRelayRead(relayDevice,8);
        if (1 == val)
            switch_init_value = caps_helper_switch.attr_switch.value_on;
        blower->set_switch_value(blower, switch_init_value);
    }

    pc = new PumpController();
    gc = new ChGenController();

    init_polling_thread();
}


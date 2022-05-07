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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "st_dev.h"
#include "iot.h"
#include "iot_caps.h"


// onboarding_config_start is null-terminated string
extern const uint8_t onboarding_config_start[] asm("_binary_onboarding_config_json_start");
extern const uint8_t onboarding_config_end[] asm("_binary_onboarding_config_json_end");

// device_info_start is null-terminated string
extern const uint8_t device_info_start[] asm("_binary_device_info_json_start");
extern const uint8_t device_info_end[] asm("_binary_device_info_json_end");


IOT_CTX *ctx = NULL;

volatile sig_atomic_t is_exit = false;

void signal_handler(int sig_num)
{
    is_exit = true;
}

void event_loop()
{
    while (!is_exit) {
        // event loop
        sleep(1);
    }

    printf("\nExit\n");
}


int main(int argc, char **argv)
{
    FILE *fp;

    if (!(fp = freopen("/var/local/log/pool/pool_controller.log", "a", stdout))) {
        printf ("log file open failed\n");
        exit (-1);
    }
    if (setvbuf(fp, NULL, _IOLBF, 1024)) {
        printf ("setvbuf failed\n");
        exit (-1);
    }

    if (!(fp = freopen("/var/local/log/pool/pool_controller.err", "a", stderr))) {
        printf ("err file open failed\n");
        exit (-1);
    }
    if (setvbuf(fp, NULL, _IOLBF, 1024)) {
        printf ("setvbuf failed\n");
        exit (-1);
    }

#ifdef NOT
    // make thyself a daemon
    pid_t cpid = fork();
    if (cpid == -1){perror("fork");exit(1);}
    if (cpid > 0){exit(0);}

    setpgid(getpid(), 0);
#endif

    /**
	  SmartThings Device SDK(STDK) aims to make it easier to develop IoT devices by providing
	  additional st_iot_core layer to the existing chip vendor SW Architecture.

      That is, you can simply develop a basic application by just calling the APIs provided by st_iot_core layer
	  like below. st_iot_core currently offers 14 API.

      //create a iot context
	  1. st_conn_init();

      //create a handle to process capability
	  2. st_cap_handle_init();

      //register a callback function to process capability command when it comes from the SmartThings Server.
	  3. st_cap_cmd_set_cb();

      //needed when it is necessary to keep monitoring the device status
	  4. user_defined_task()

      //process on-boarding procedure. There is nothing more to do on the app side than call the API.
	  5. st_conn_start();
	 */

    unsigned char *onboarding_config = (unsigned char *)onboarding_config_start;
    unsigned int onboarding_config_len = onboarding_config_end - onboarding_config_start;
    unsigned char *device_info = (unsigned char *)device_info_start;
    unsigned int device_info_len = device_info_end - device_info_start;
    //IOT_CAP_HANDLE *handle = NULL;
    //int iot_err;

    // 1. create a iot context
    ctx = st_conn_init(onboarding_config, onboarding_config_len, device_info, device_info_len);
    if (ctx != NULL) {
        iot_caps_setup(ctx);
    } else {
        printf("fail to create the iot_context\n");
    }

    // 4. needed when it is necessary to keep monitoring the device status
    //xTaskCreate(user_defined_task, "user_defined_task", 2048, (void *)handle, 10, NULL);

    // 5. process on-boarding procedure. There is nothing more to do on the app side than call the API.
    st_conn_start(ctx, (st_status_cb)&iot_status_cb, IOT_STATUS_ALL, NULL, NULL);

    // exit by using Ctrl+C
    signal(SIGINT, signal_handler);

    event_loop();
}

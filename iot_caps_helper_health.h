/* ***************************************************************************
 *
 * Copyright 2019-2020 Samsung Electronics All Rights Reserved.
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

#ifndef _IOT_CAPS_HELPER_HEALTH_
#define _IOT_CAPS_HELPER_HEALTH_

#include "iot_caps_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CAP_ENUM_HEALTH_STATUS_VALUE_ONLINE,
    CAP_ENUM_HEALTH_STATUS_VALUE_OFFLINE,
    CAP_ENUM_HEALTH_STATUS_VALUE_MAX
};

const static struct iot_caps_health {
    const char *id;
    const struct health_attr_status {
        const char *name;
        const unsigned char property;
        const unsigned char valueType;
        const char *values[CAP_ENUM_HEALTH_STATUS_VALUE_MAX];
        const char *value_online;
        const char *value_offline;
    } attr_health;
    const struct health_cmd_on { const char* name; } cmd_online;
    const struct health_cmd_off { const char* name; } cmd_offline;
} caps_helper_health = {
    .id = "healthCheck",
    .attr_status = {
        .name = "healthStatus",
        .property = ATTR_SET_VALUE_REQUIRED,
        .valueType = VALUE_TYPE_STRING,
        .values = {"online", "offline"},
        .value_on = "online",
        .value_off = "offline",
    },
    .cmd_on = { .name = "online" },
    .cmd_off = { .name = "offline" },
};

#ifdef __cplusplus
}
#endif

#endif /* _IOT_CAPS_HELPER_HEALTH_ */

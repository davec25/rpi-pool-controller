
CC = gcc
RM = rm -rf
CFLAGS = -std=c99 
LIB_NAME = iotcore
INC = -I../src/include
LIB_DIR = -L../output
LIBS = -l$(LIB_NAME) -lssl -lpthread -lrt -lcrypto -lm
SRCS = main.c iot_caps.c iot.c relay.c comm.c thread.c caps_switch.c caps_temperatureMeasurement.c caps_thermostatHeatingSetpoint.c caps_voltageMeasurement.c caps_relativeHumidityMeasurement.c
JSONS = device_info.json onboarding_config.json
OBJS = $(JSONS:%.json=%.o)
TARGET = poolController
LD_CMD = ld

.PHONY: all clean
all: $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS)

%.o : %.json
	@echo $@
	$(LD_CMD) -r -b binary -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(SRCS) $(OBJS) $(LIB_DIR) $(INC) $(LIBS)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/


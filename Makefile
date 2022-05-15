
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
	sudo systemctl stop pool 2>/dev/null || true
	sudo cp $(TARGET) /usr/local/bin/
	sudo mkdir -r /var/local/run/pool/
	sudo chown pi:pi /var/local/run/pool
	sudp cp softapstart softapstop /var/loca/run/pool/
	sudo cp pool.service /etc/systemd/system/
	sudo systemctl enable pool 
	sudo systemctl start pool 


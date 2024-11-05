CONTIKI_PROJECT = tagalong

#CONTIKI=../../../iot-lab-contiki-ng/contiki-ng
CONTIKI=../../../../iot-lab-contiki-ng-priv/contiki-ng

PLATFORMS_ONLY = zoul sky iotlab m3

all: $(CONTIKI_PROJECT)

MAKE_MAC = MAKE_MAC_TSCH
ifeq ($(TARGET), sky)
MAKE_NET= MAKE_NET_NULLNET
MAKE_ROUTING = MAKE_ROUTING_NULLROUTING
else
MODULES += os/net/ipv6/multicast
endif

MODULES_REL += services
MODULES_REL += net/mac/tsch
MODULES += os/services/deployment

include $(CONTIKI)/Makefile.include

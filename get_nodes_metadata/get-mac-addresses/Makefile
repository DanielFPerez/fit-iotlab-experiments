CONTIKI_PROJECT = get-mac
all: $(CONTIKI_PROJECT)

CONTIKI = ../../iot-lab-contiki-ng/contiki-ng
include $(CONTIKI)/Makefile.include

# Disable the specific warning about array bounds being treated as an error
CFLAGS += -Wno-error=array-bounds
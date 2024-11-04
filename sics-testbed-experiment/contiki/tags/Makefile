CONTIKI_PROJECT = tagalong-tag
include Makefile.local

PLATFORMS_ONLY = sky

all: $(CONTIKI_PROJECT)

MAKE_MAC = MAKE_MAC_NULLMAC
MAKE_NET = MAKE_NET_NULLNET
MAKE_ROUTING = MAKE_ROUTING_NULLROUTING

MODULES_REL += dev/ca-tag
MODULES_REL += net/mac/nullmac

include $(CONTIKI)/Makefile.include

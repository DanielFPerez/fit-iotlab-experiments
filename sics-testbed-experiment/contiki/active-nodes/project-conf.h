#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 120

/*#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_DBG */
/*#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_DBG */
/*#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_INFO */
/*#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN */
/*#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN */
/*#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO */
/*#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_DBG */

#define NBR_TABLE_CONF_MAX_NEIGHBORS 32
#define NETSTACK_MAX_ROUTE_ENTRIES 32

#define LPM_CONF_MAX_PM 0
#define TSCH_CONF_AUTOSTART 0
#define TSCH_CONF_CCA_ENABLED 0
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0
#define TSCH_CONF_INIT_SCHEDULE_FROM_EB 0
#define TSCH_PACKET_CONF_EB_WITH_SLOTFRAME_AND_LINK 0
#define TSCH_CONF_EB_PERIOD (5 * CLOCK_SECOND)

#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_1_1

#define TSCH_CONF_WITH_LINK_SELECTOR 1

/*#define TSCH_CONF_CARRIER_CHANNEL_OFFSET 2 */

/*/ * Disable DCO calibration (uses timerB) * / */
/*#undef DCOSYNCH_CONF_ENABLED */
/*#define DCOSYNCH_CONF_ENABLED		0 */
/* */
/*/ * Enable SFD timestamps (uses timerB) * / */
/*#undef CC2420_CONF_SFD_TIMESTAMPS */
/*#define CC2420_CONF_SFD_TIMESTAMPS      1 */

#define RPL_CONF_DIS_INTERVAL (1 * CLOCK_SECOND)
#define RPL_CONF_DIO_INTERVAL_MIN 10

#define ENERGEST_CONF_ON 1

#define CC2538_RF_CONF_TX_POWER 0xFF

#define DEPLOYMENT_MAPPING deployment_fit_iot_lab_lille

#include "net/ipv6/multicast/uip-mcast6-engines.h"
#ifndef UIP_MCAST6_CONF_ENGINE
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_ROLL_TM
#endif

/* For Imin: Use 16 over CSMA, 64 over Contiki MAC */
#define ROLL_TM_CONF_IMIN_1         128

#define UIP_MCAST6_ROUTE_CONF_ROUTES 1

#define LOG_CONF_LEVEL_RPL LOG_LEVEL_NONE
#define RPL_CONF_WITH_NON_STORING 1
#define HEAPMEM_CONF_ARENA_SIZE 100

#define NBR_TABLE_CONF_MAX_NEIGHBORS 50
#define QUEUEBUF_CONF_NUM 16

#if 1
// Adjust these parameters to change topology density.
#define RF2XX_TX_POWER  PHY_POWER_m17dBm
#define RF2XX_RX_RSSI_THRESHOLD  RF2XX_PHY_RX_THRESHOLD__m75dBm
#endif

#ifndef NEIGHBOR_DISCOVERY_H_
#define NEIGHBOR_DISCOVERY_H_

#include "net/nbr-table.h"

#define BEACON_INTERVAL (3 * CLOCK_SECOND)
#define REPORT_INTERVAL BEACON_INTERVAL
/*#define RANK_INCREMENT 10 */

#define NBR_DISC_MAX_RANK 0xFF
#define NBR_DISC_SINK_RANK 0x00

#define NBR_DISC_BEACON_LEN 4
#define NBR_DISC_APP_CODE 0x42

#define NBR_DISC_FTYPE_BEACON 0x08
#define NBR_DISC_FTYPE_BACK   0x02

struct topo_nbr {
  uint8_t id;
  int16_t rssi;
};
typedef struct topo_nbr topo_nbr_t;

NBR_TABLE_DECLARE(topo_neighbors);

void neighbor_discovery_start(struct process *proc);
void neighbor_discovery_stop();

#endif /* NEIGHBOR_DISCOVERY_H_ */

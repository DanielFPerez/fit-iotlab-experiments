#ifndef _TSCH_TAGALONG_H_
#define _TSCH_TAGALONG_H_

#include "net/nbr-table.h"
#include <string.h>

struct nbr_tag {
  uint8_t slotframe;
  uint8_t timeslot;
  linkaddr_t *carr_addr;
};
typedef struct nbr_tag nbr_tag_t;

NBR_TABLE_DECLARE(nbr_tags);

void tsch_tagalong_init();

uint8_t tsch_tagalong_add_tag(const linkaddr_t *addr,
                              uint8_t slotframe,
                              uint8_t timeslot,
                              const linkaddr_t *carr_addr);

typedef void (*tsch_tagalong_reply_callback_t)();
void tsch_tagalong_set_reply_callback(tsch_tagalong_reply_callback_t callback);
void tsch_tagalong_reply_input();

void tsch_tagalong_clear_table();

extern rtimer_clock_t tsch_tagalong_send_time, tsch_tagalong_actual_send_time;
extern uint8_t tsch_tagalong_actual_sent;

#endif /* _TSCH_TAGALONG_H_ */

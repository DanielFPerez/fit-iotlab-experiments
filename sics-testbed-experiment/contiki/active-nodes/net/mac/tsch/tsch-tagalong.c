#include "tsch-tagalong.h"
#include <stdlib.h>

#include "sys/log.h"
#define LOG_MODULE "TSCH-TA"
#define LOG_LEVEL LOG_LEVEL_INFO

NBR_TABLE_GLOBAL(nbr_tag_t, nbr_tags);

rtimer_clock_t tsch_tagalong_send_time = 0;
rtimer_clock_t tsch_tagalong_actual_send_time = 0;
uint8_t tsch_tagalong_actual_sent = 0;

void
tsch_tagalong_init()
{
  nbr_table_register(nbr_tags, NULL);
}
uint8_t
tsch_tagalong_add_tag(const linkaddr_t *addr,
                      uint8_t slotframe,
                      uint8_t timeslot,
                      const linkaddr_t *carr_addr)
{
  nbr_tag_t *n = malloc(sizeof(nbr_tag_t));
  nbr_tag_t *t = nbr_table_add_lladdr(
    nbr_tags,
    addr,
    NBR_TABLE_REASON_UNDEFINED,
    n);
  t->slotframe = slotframe;
  t->timeslot = timeslot;
  t->carr_addr = malloc(sizeof(linkaddr_t));
  /*memcpy(t->carr_addr, carr_addr, sizeof(linkaddr_t)); */
  linkaddr_copy(t->carr_addr, carr_addr);

  LOG_DBG("ADD_TAG SF: %d, TS: %d, CG: ", t->slotframe, t->timeslot);
  LOG_DBG_LLADDR(carr_addr);
  LOG_DBG_("\n");
  return 1;
}
static tsch_tagalong_reply_callback_t tsch_tagalong_reply_callback = NULL;
void
tsch_tagalong_set_reply_callback(tsch_tagalong_reply_callback_t callback)
{
  tsch_tagalong_reply_callback = callback;
}
void
tsch_tagalong_reply_input()
{
  if(tsch_tagalong_reply_callback != NULL) {
    tsch_tagalong_reply_callback();
  }
}
void
tsch_tagalong_clear_table()
{
  nbr_tag_t *current = nbr_table_head(nbr_tags);
  while(current != NULL) {
    nbr_table_remove(nbr_tags, current);
    current = nbr_table_next(nbr_tags, current);
  }
}

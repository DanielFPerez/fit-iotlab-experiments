#include "contiki.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl-neighbor.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/link-stats.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");

}
/*---------------------------------------------------------------------------*/
static void
send_neighbor_list(const uip_ipaddr_t *dest)
{
  static uint8_t buf[64];
  uip_ipaddr_t *temp;

  buf[0] = 0x42; /* Packet magic byte to identify */
  buf[1] = rpl_neighbor_count();
  LOG_INFO("neighbor count %d:\n", rpl_neighbor_count());
  unsigned index = 2;
  memset(buf, 0, sizeof(buf));
  for(rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors);
      nbr != NULL && index + 3 <= sizeof(buf);
      nbr = nbr_table_next(rpl_neighbors, nbr)) {
    temp = rpl_neighbor_get_ipaddr(nbr);
    const linkaddr_t *lladdr = rpl_neighbor_get_lladdr(nbr);
    int16_t rssi = 0;
    if(lladdr == NULL) {
      LOG_WARN("Unable to get LLADDR!\n");
    } else {
      const struct link_stats *link_stats = link_stats_from_lladdr(lladdr);
      if(link_stats == NULL) {
	LOG_WARN("Unable to get link stats!\n");
      } else {
	rssi = link_stats->rssi;
      }
    }
    LOG_INFO("NBR_SEND %u.%u:%d\n", temp->u8[14], temp->u8[15], (int)rssi);
    /* Use the last two bytes as the unique ID. */
    memcpy(&buf[index], &temp->u8[14], 2);
    buf[index + 2] = (uint8_t)(ABS(rssi) & 0xff);
    index += 3;
  }
  if(index >= sizeof(buf)) {
    LOG_WARN("Sending a truncated neighbor list because the buffer is too small!\n");
    index = sizeof(buf);
  }
  simple_udp_sendto(&udp_conn, buf, index, dest);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count;
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      /* Send to DAG root */
      LOG_INFO("Sending request %u to ", count);
      LOG_INFO_6ADDR(&dest_ipaddr);
      LOG_INFO_("\n");
      send_neighbor_list(&dest_ipaddr);
      count++;
    } else {
      LOG_INFO("Not reachable yet\n");
    }

    /* Add some jitter */
    etimer_set(&periodic_timer, SEND_INTERVAL
      - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

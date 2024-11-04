#include "contiki.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl-neighbor.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;

#define START_INTERVAL    (15 * CLOCK_SECOND)
#define SEND_INTERVAL     (60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;

static uint8_t should_report = 0;

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
  if(data[0] == 0x42) {
    should_report = 0;
  }

/*  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data); */
/*  LOG_INFO_6ADDR(sender_addr); */
/*#if LLSEC802154_CONF_ENABLED */
/*  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL)); */
/*#endif */
/*  LOG_INFO_("\n"); */
}
static void
send_neighbors_list(uip_ipaddr_t *dest)
{
  static char buff[32];
  uip_ipaddr_t *temp;

  rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors);
  buff[0] = 0x42; /* Packet magic byte to identify */
  /*buff[1] = rpl_neighbor_count(); */
  buff[1] = 0;
  /*LOG_INFO("neighbor count %d:\n", rpl_neighbor_count()); */
  while(nbr != NULL) {
    buff[1]++;

    temp = rpl_neighbor_get_ipaddr(nbr);
    buff[buff[1] + 1] = temp->u8[15];
    /*LOG_INFO("\tneighbor %d: %d\n", buff[1], buff[buff[1]+1]); */

    nbr = nbr_table_next(rpl_neighbors, nbr);
  }
  simple_udp_sendto(&udp_conn, buff, strlen(buff), dest);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  /*static unsigned count; */
  /*static char str[32]; */
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  should_report = 1;

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      /* Send to DAG root */
      /*LOG_INFO("Sending request %u to ", count); */
      /*LOG_INFO_6ADDR(&dest_ipaddr); */
      /*LOG_INFO_("\n"); */
      /*snprintf(str, sizeof(str), "hello %d", count); */
      /*simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr); */
      /*count++; */

      if(should_report) {
        send_neighbors_list(&dest_ipaddr);
      }
      /*rpl_neighbor_print_list("test"); */
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

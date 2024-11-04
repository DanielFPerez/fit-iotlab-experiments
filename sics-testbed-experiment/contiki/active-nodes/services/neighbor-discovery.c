#include "contiki.h"
#include "net/netstack.h"
#include <string.h>
#include <stdlib.h>
#include "net/mac/tsch/tsch.h"
#include "random.h"
#include "dev/serial-line.h"
#include "deployment.h"
#include "net/link-stats.h"

#if !CONTIKI_TARGET_SKY
#include "net/ipv6/simple-udp.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl-neighbor.h"
#include "net/ipv6/uip-ds6.h"

#include "net/ipv6/multicast/uip-mcast6.h"
#include "contiki-net.h"
#endif

#include "net/nullnet/nullnet.h"

#include "neighbor-discovery.h"

#include "sys/log.h"
#define LOG_MODULE "ND"
#define LOG_LEVEL LOG_LEVEL_INFO

NBR_TABLE_GLOBAL(topo_nbr_t, topo_neighbors);

/*---------------------------------------------------------------------------*/
PROCESS(neigh_discover, "Neighbor discovery");
/*---------------------------------------------------------------------------*/
static uint8_t is_running = 0;
/*---------------------------------------------------------------------------*/
#if CONTIKI_TARGET_SKY
static uint8_t rank = NBR_DISC_MAX_RANK;
static char packet_beacon[NBR_DISC_BEACON_LEN];
/*---------------------------------------------------------------------------*/
void
send_beacon()
{
  packet_beacon[0] = NBR_DISC_APP_CODE;
  packet_beacon[1] = NBR_DISC_FTYPE_BEACON;
  packet_beacon[2] = deployment_id_from_lladdr(&linkaddr_node_addr);
  packet_beacon[3] = rank;

  nullnet_buf = (uint8_t *)packet_beacon;
  nullnet_len = NBR_DISC_BEACON_LEN;
  LOG_DBG("Sending beacon now.\n");
  NETSTACK_NETWORK.output(NULL);
}
/**---------------------------------------------------------------------------* / */
void
send_beacon_ack(const linkaddr_t *dest, uint8_t new_rank)
{
  packet_beacon[0] = NBR_DISC_APP_CODE;
  packet_beacon[1] = NBR_DISC_FTYPE_BACK;
  packet_beacon[2] = deployment_id_from_lladdr(dest); /* Dest ID */
  packet_beacon[3] = new_rank;

  nullnet_buf = (uint8_t *)packet_beacon;
  nullnet_len = NBR_DISC_BEACON_LEN;
  LOG_DBG("Sending ACK now.\n");
  NETSTACK_NETWORK.output(NULL);
}
/*---------------------------------------------------------------------------*/
int
add_neighbor(const linkaddr_t *addr, uint8_t id, int16_t rssi)
{
  topo_nbr_t *d = malloc(sizeof(topo_nbr_t));
  d->id = id;
  d->rssi = rssi;
  topo_nbr_t *t = nbr_table_add_lladdr(
    topo_neighbors,
    addr,
    NBR_TABLE_REASON_UNDEFINED,
    d);
  t->id = deployment_id_from_lladdr(addr);
  t->rssi = rssi;

  return 1;
}
/*---------------------------------------------------------------------------*/
void
print_neigh_table()
{
  topo_nbr_t *nbr = nbr_table_head(topo_neighbors);
  printf(" %d -> (", deployment_id_from_lladdr(&linkaddr_node_addr));
  while(nbr != NULL) {
    printf("%d:%d, ", nbr->id, nbr->rssi);
    nbr = nbr_table_next(topo_neighbors, nbr);
  }
  printf(")\n");
}
/**---------------------------------------------------------------------------* / */
void
nd_input_callback(const void *data, uint16_t len,
                  const linkaddr_t *src, const linkaddr_t *dest)
{
  uint8_t *packet = (uint8_t *)data;
  if(packet[0] == NBR_DISC_APP_CODE) {
    switch(packet[1]) {
    case NBR_DISC_FTYPE_BEACON:   /* Neighbor advertisement */
      LOG_DBG("Neigh adv from %d with rank: %d, local rank: %d\n",
              packet[2], packet[3], rank);
      /*if (packet[3] < rank) { */
      /*rank = packet[3] + RANK_INCREMENT; */
      add_neighbor(src, packet[2], packetbuf_attr(PACKETBUF_ATTR_RSSI));
      send_beacon_ack(src, rank);
      /*} */
      break;
    case NBR_DISC_FTYPE_BACK:   /* Neighbor advertisement ACK */
      if(packet[2] == deployment_id_from_lladdr(&linkaddr_node_addr)) {
        LOG_DBG("Neigh ACK from %d\n", deployment_id_from_lladdr(src));
        add_neighbor(src, packet[2],
                     packetbuf_attr(PACKETBUF_ATTR_RSSI));
      }
      break;
    default:
      LOG_ERR("Unknown frame type: %d\n", packet[1]);
    }
  }
}
/*---------------------------------------------------------------------------*/
#else
static uint8_t should_report = 1;

static struct simple_udp_connection udp_conn;
static struct simple_udp_connection bc_conn;

static struct uip_udp_conn *mcast_conn;

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */
/**---------------------------------------------------------------------------* / */
static void
prepare_mcast(void)
{
  uip_ipaddr_t ipaddr;

  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&ipaddr, 0xFF1E, 0, 0, 0, 0, 0, 0x89, 0xABCD);
  mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_UDP_PORT), NULL);
}
/*---------------------------------------------------------------------------*/
static uip_ds6_maddr_t *
join_mcast_group(void)
{
  uip_ipaddr_t addr;
  uip_ds6_maddr_t *rv;

/*  / * First, set our v6 global * / */
/*  uip_ip6addr(&addr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0); */
/*  uip_ds6_set_addr_iid(&addr, &uip_lladdr); */
/*  uip_ds6_addr_add(&addr, 0, ADDR_AUTOCONF); */

  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&addr, 0xFF1E, 0, 0, 0, 0, 0, 0x89, 0xABCD);
  rv = uip_ds6_maddr_add(&addr);

  if(rv) {
    LOG_INFO("Joined multicast group ");
    LOG_INFO_6ADDR(&uip_ds6_maddr_lookup(&addr)->ipaddr);
    LOG_INFO_("\n");
  }
  return rv;
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    LOG_INFO("RX Multicast! %s\n", (char *)uip_appdata);

    /*count++; */
    /*LOG_INFO("In: [0x%08lx], TTL %u, total %u\n", */
    /*(unsigned long)uip_ntohl((unsigned long) *((uint32_t *)(uip_appdata))), */
    /*UIP_IP_BUF->ttl, count); */

    char *str = malloc(uip_len + 1);
    memcpy(str, uip_appdata, uip_len);
    str[uip_len] = 0;
    LOG_INFO("%c RX on BC conn!!: %s\n", str[0], str);
    if(str[0] == 0x42 && str[1] == 'C' && str[3] == 'N') {
      if(!tsch_is_coordinator) {
        LOG_INFO("Should Stop!!.\n");
        should_report = 0;
        /*simple_udp_sendto(&bc_conn, str, 4, sender_addr); */
        neighbor_discovery_stop();
      } else {
        /*LOG_INFO("%d: ACK %s\n", */
        /*deployment_id_from_iid(sender_addr), "*N"); */
      }
    }
  }
  return;
}
/*---------------------------------------------------------------------------*/
static void
print_neighbor_report(const uint8_t data_len, const uint8_t *data, uint8_t sender)
{
  static uint8_t i, neighbors;
  neighbors = data[1];
  LOG_INFO("%d -> (", sender);
  for(i = 0; i < neighbors; i++) {
    LOG_INFO_("%d:%d, ", data[3 * i + 2], *((int16_t *)(data + 3 * i + 3)));
  }
  LOG_INFO_(")\n");
}
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
  if(data[0] == 0x42) {
    print_neighbor_report(datalen, data,
                          deployment_id_from_iid(sender_addr) & 0xFF);
    /*LOG_INFO("Sending ACK.\n"); */
  }
}
/*---------------------------------------------------------------------------*/
static void
udp_bc_rx_callback(struct simple_udp_connection *c,
                   const uip_ipaddr_t *sender_addr,
                   uint16_t sender_port,
                   const uip_ipaddr_t *receiver_addr,
                   uint16_t receiver_port,
                   const uint8_t *data,
                   uint16_t datalen)
{
  char *str = malloc(datalen + 1);
  memcpy(str, data, datalen);
  str[datalen] = 0;
  LOG_INFO("%c RX on BC conn!!: %s\n", data[0], str);
  if(data[0] == 0x42 && data[1] == 'C' && data[3] == 'N') {
    if(!tsch_is_coordinator) {
      LOG_INFO("Should Stop!!.\n");
      should_report = 0;
      simple_udp_sendto(&bc_conn, data, 4, sender_addr);
      neighbor_discovery_stop();
    } else {
      LOG_INFO("%d: ACK %s\n",
               deployment_id_from_iid(sender_addr), "*N");
    }
  }
/*  if (data[0] == 0x42) { */
  /*print_neighbor_report(datalen, data, sender_addr->u8[15]); */
  /*LOG_INFO("Sending ACK.\n"); */
  /*} */
}
/*---------------------------------------------------------------------------*/
static void
forward_command(char *const command)
{
  LOG_INFO("Forwarding command to %d: %s\n", 0, command);
  static char buff[32];
  uip_ipaddr_t dest_addr;

  memset(&dest_addr, 0, sizeof(uip_ipaddr_t));
  dest_addr.u8[0] = (UIP_DS6_DEFAULT_PREFIX >> 8) & 0xFF;
  dest_addr.u8[1] = (UIP_DS6_DEFAULT_PREFIX) & 0xFF;

  buff[0] = 0x42; /* Packet magic byte to identify */
  buff[1] = 'C';
  memcpy(buff + 2, command, strlen(command));
  uip_udp_packet_send(mcast_conn, buff, strlen(command) + 2);
}
/*---------------------------------------------------------------------------*/
static void
send_neighbors_list(uip_ipaddr_t *dest)
{
  static char buff[32];
  uip_ipaddr_t *temp;
  uint16_t tmp_metric;
  uint16_t tmp_id;
  const struct link_stats *ls;
  int16_t rssi;

  rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors);
  buff[0] = 0x42; /* Packet magic byte to identify */
  /*buff[1] = rpl_neighbor_count(); */
  buff[1] = 0;
  LOG_INFO("neighbor count %d:\n", rpl_neighbor_count());
  LOG_DBG("-> (");
  while(nbr != NULL) {
    temp = rpl_neighbor_get_ipaddr(nbr);
    tmp_id = deployment_id_from_iid(temp);
    tmp_metric = rpl_neighbor_get_link_metric(nbr);
    ls = rpl_neighbor_get_link_stats(nbr);
    rssi = ls->rssi;
    buff[1 + 3 * buff[1] + 1] = tmp_id & 0xFF;
    memcpy(buff + 1 + 3 * buff[1] + 2, &rssi, sizeof(rssi));
    /*LOG_INFO("\tneighbor %d: %d\n", buff[1], buff[buff[1]+1]); */
    LOG_DBG_("%d:%u:%d, ", tmp_id, tmp_metric, rssi);

    nbr = nbr_table_next(rpl_neighbors, nbr);
    buff[1]++;
  }
  LOG_DBG_(")\n");
  simple_udp_sendto(&udp_conn, buff, 3 * buff[1] + 2, dest);
}
#endif
/*---------------------------------------------------------------------------*/
static void
handle_serial(char *data)
{
  char *d = data;
  uint8_t do_loopback = 1;
/*printf("SErial got line: %s\n", d); */
  if(*d == '*') {
    do_loopback = 0;
    char *token;
    uint8_t cursor = 0;
    /*token = strsep(&d, " ");*/
    token = strtok(d, " ");
    while(token[cursor] == '*') {
      cursor++;
    }
    switch(token[cursor]) {
    case 'c':
    case 'C':
      /*token = strsep(&d, " ");*/
      token = strtok(d, " ");
      uint8_t len = atoi(token);
      len = len;
      /*token = strsep(&d, " ");*/
      token = strtok(d, " ");
      uint8_t id = atoi(token);
      id = id;
#if CONTIKI_TARGET_SKY
      printf("|%d|%s\n", id, d);
      /*printf("|%d|%s\n", -1, d); */
#else
      forward_command(d);
#endif
      break;
    case 'n':
    case 'N':
      printf("Stopping neighbor discovery!!\n");
#if CONTIKI_TARGET_SKY
      printf("%d: ACK %s\n",
             deployment_id_from_lladdr(&linkaddr_node_addr),
             token);
#else
#endif
      neighbor_discovery_stop();
      break;
    case 'r':
    case 'R':
      watchdog_reboot();
      break;
    case 'p':
    case 'P':
      tsch_schedule_print();

/*				nbr_tag_t *current = nbr_table_head(nbr_tags); */
/*				while (current != NULL) { */
/*					LOG_INFO_LLADDR(nbr_table_get_lladdr(nbr_tags, current)); */
/*					LOG_INFO_(" SF: %d TS: %d\n", */
/*							current->slotframe, */
/*							current->timeslot */
/*					      ); */
/*					current = nbr_table_next(nbr_tags, current); */
/*				} */
/*				  break; */
    }
  }
  if(do_loopback) {
    printf("%s\n", d);
  }
}
/*---------------------------------------------------------------------------*/
static struct process *calling_process = NULL;
PROCESS_THREAD(neigh_discover, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();

  LOG_INFO("Neighbor discovery process started.\n");

  calling_process = data;

  nbr_table_register(topo_neighbors, NULL);
#if CONTIKI_TARGET_SKY
  nullnet_set_input_callback(nd_input_callback);
#else
  uip_ipaddr_t dest_ipaddr;
  /* Initialize UDP connection */
  if(tsch_is_coordinator) {
    simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                        UDP_CLIENT_PORT, udp_rx_callback);
    simple_udp_register(&bc_conn, UDP_SERVER_PORT + 1, NULL,
                        UDP_CLIENT_PORT + 1, udp_bc_rx_callback);
    prepare_mcast();
  } else {
    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                        UDP_SERVER_PORT, udp_rx_callback);
    simple_udp_register(&bc_conn, UDP_CLIENT_PORT + 1, NULL,
                        UDP_SERVER_PORT + 1, udp_bc_rx_callback);

    if(join_mcast_group() == NULL) {
      LOG_INFO("Failed to join multicast group\n");
      PROCESS_EXIT();
    }
    mcast_conn = udp_new(NULL, UIP_HTONS(0), NULL);
    udp_bind(mcast_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));
    LOG_INFO("Listening: ");
    LOG_INFO_6ADDR(&mcast_conn->ripaddr);
    LOG_INFO_(" local/remote port %u/%u\n",
              UIP_HTONS(mcast_conn->lport), UIP_HTONS(mcast_conn->rport));
  }
#endif

  etimer_set(&periodic_timer, REPORT_INTERVAL);
  while(1) {
    PROCESS_YIELD_UNTIL(
      ev == serial_line_event_message ||
      etimer_expired(&periodic_timer) ||
#if !CONTIKI_TARGET_SKY
      ev == tcpip_event ||
#endif
      ev == PROCESS_EVENT_POLL);

    if(ev == serial_line_event_message && data != NULL) {
      handle_serial((char *)data);
    } else if(etimer_expired(&periodic_timer)) {
#if CONTIKI_TARGET_SKY
      if(tsch_is_associated) {
        send_beacon();
        print_neigh_table();
      }
#else
      if(!NETSTACK_ROUTING.node_is_reachable()) {
        LOG_INFO("Status: TSCH: %d, REACH: %d\n",
                 tsch_is_associated, NETSTACK_ROUTING.node_is_reachable());
      }
      if(!NETSTACK_ROUTING.node_is_root() &&
         NETSTACK_ROUTING.node_is_reachable() &&
         NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        if(should_report) {
          LOG_INFO("Sending report.\n");
          send_neighbors_list(&dest_ipaddr);
        }
      }
#endif
      etimer_set(&periodic_timer,
                 REPORT_INTERVAL / 2 + (random_rand() % (REPORT_INTERVAL / 2)));
    } else if(ev == PROCESS_EVENT_POLL) {
      if(is_running == 0) {
        /*TODO Cancel beacon timer? */
/*			  LOG_DBG("Poll calling process: %d\n", (int)calling_process); */
/*			  process_poll(calling_process); */
        break;
      }
    }
#if !CONTIKI_TARGET_SKY
    else if(ev == tcpip_event) {
      if(!tsch_is_coordinator) {
        tcpip_handler();
      }
    }
#endif
  }
  LOG_INFO("Neighbor discovery done.");

  PROCESS_END();
}
/**---------------------------------------------------------------------------* / */
void
neighbor_discovery_start(struct process *proc)
{
  is_running = 1;
  process_start(&neigh_discover, proc);
}
/**---------------------------------------------------------------------------* / */
void
neighbor_discovery_stop()
{
  if(is_running) {
    is_running = 0;
    process_poll(&neigh_discover);

#if !CONTIKI_TARGET_SKY
    if(tsch_is_coordinator) {
      simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                          UDP_CLIENT_PORT, NULL);
      simple_udp_register(&bc_conn, UDP_SERVER_PORT + 1, NULL,
                          UDP_CLIENT_PORT + 1, NULL);
    } else {
      simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                          UDP_SERVER_PORT, NULL);
      simple_udp_register(&bc_conn, UDP_CLIENT_PORT + 1, NULL,
                          UDP_SERVER_PORT + 1, NULL);
    }
#endif
    LOG_DBG("Poll calling process: %d\n", (int)calling_process);
    process_poll(calling_process);
  }
}
/*---------------------------------------------------------------------------*/

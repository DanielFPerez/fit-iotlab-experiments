#include "contiki.h"
#include "dev/serial-line.h"
#include <stdlib.h>

#include "deployment.h"
#include "deployment_mappings.h"

#include "../tagalong-const.h"
#include "neighbor-discovery.h"

#include "net/mac/tsch/tsch.h"
#include "net/nullnet/nullnet.h"
#include "net/routing/routing.h"
#include "net/mac/tsch/tsch-tagalong.h"
#include "sys/energest.h"
#include "random.h"

#if !CONTIKI_TARGET_SKY
#include "net/ipv6/simple-udp.h"

#include "net/ipv6/multicast/uip-mcast6.h"
#include "contiki-net.h"
#endif

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

static struct tsch_slotframe *sf;
static unsigned count = 0;

static uint8_t buff[TAGALONG_INTERROGATION_LEN];

static linkaddr_t coordinator_addr;

static uint16_t measure_cycles = 0;
static uint16_t current_measure_cycle = 0;

static rtimer_clock_t tag_int_tx_times[16];

void handle_serial(char *line);
/*---------------------------------------------------------------------------*/
PROCESS(tagalong_process, "TagAlong process");
PROCESS(tagalong_worker_process, "TagAlong worker process");
AUTOSTART_PROCESSES(&tagalong_process);
/*---------------------------------------------------------------------------*/
void
energest_report()
{
  energest_flush();
  printf("ES %u Radio RX %4llu TX %4llu CG %4llu OFF %4llu TTL %llu\n",
         ENERGEST_SECOND,
         (energest_type_time(ENERGEST_TYPE_LISTEN)),
         (energest_type_time(ENERGEST_TYPE_TRANSMIT)),
         (energest_type_time(ENERGEST_TYPE_CARRIER)),
         (ENERGEST_GET_TOTAL_TIME()
          - energest_type_time(ENERGEST_TYPE_TRANSMIT)
          - energest_type_time(ENERGEST_TYPE_LISTEN)
          - energest_type_time(ENERGEST_TYPE_CARRIER)),
         (ENERGEST_GET_TOTAL_TIME()));
}
/*---------------------------------------------------------------------------*/
void
tag_reply_callback()
{
  uint8_t *msg = packetbuf_dataptr();
  const linkaddr_t *src = packetbuf_addr(PACKETBUF_ADDR_SENDER);
  uint8_t seq = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);
  int8_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  if(msg[0] == TAGALONG_APP_CODE && msg[1] == TAGALONG_MESSAGE_TAGREPLY) {
    uint8_t tag_id = deployment_id_from_lladdr(src);
    rtimer_clock_t timestamp = RTIMER_NOW();
    uint32_t latency = (rtimer_clock_t)(timestamp - tag_int_tx_times[tag_id - 100]);
    LOG_INFO("%d RX from %d SEQ %d RSSI %d LAT %lu\n",
             deployment_id_from_lladdr(&linkaddr_node_addr),
             tag_id,
             seq, rssi, latency);
  }
}
/*---------------------------------------------------------------------------*/
void
interrogate_tag(const linkaddr_t *tag_addr)
{
  uint8_t tag_id = deployment_id_from_lladdr(tag_addr);
  LOG_INFO("%d TX to %d\n",
           deployment_id_from_lladdr(&linkaddr_node_addr),
           tag_id);

  buff[0] = TAGALONG_APP_CODE;
  buff[1] = TAGALONG_MESSAGE_TAGINTERROGATE;
  memcpy(buff + 2, tag_addr, LINKADDR_SIZE);
  packetbuf_clear();
  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, tag_addr);
  packetbuf_copyfrom(buff, TAGALONG_INTERROGATION_LEN);
  NETSTACK_MAC.send(NULL, NULL);
  tag_int_tx_times[tag_id - 100] = RTIMER_NOW();
  count++;
}
/*---------------------------------------------------------------------------*/
void
interrogate_all_tags()
{
  nbr_tag_t *current = nbr_table_head(nbr_tags);
  while(current != NULL) {
    interrogate_tag(nbr_table_get_lladdr(nbr_tags, current));
    current = nbr_table_next(nbr_tags, current);
  }
}
/*---------------------------------------------------------------------------*/
void
lattency_dummy_send()
{
  if(tsch_tagalong_actual_sent) {
    LOG_INFO("AN %d TX LATT %lu\n",
             deployment_id_from_lladdr(&linkaddr_node_addr),
             (uint32_t)(tsch_tagalong_actual_send_time - tsch_tagalong_send_time));
    tsch_tagalong_actual_sent = 0;
  }

  LOG_INFO("AN %d TX\n",
           deployment_id_from_lladdr(&linkaddr_node_addr));

  buff[0] = TAGALONG_APP_CODE + 3;
  packetbuf_clear();
  /*packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, tag_addr); */
  packetbuf_copyfrom(buff, 1);
  NETSTACK_MAC.send(NULL, NULL);
  tsch_tagalong_send_time = RTIMER_NOW();
  count++;
}
/*---------------------------------------------------------------------------*/
void
init_control_slotframe(uint8_t n_slots)
{
  tsch_schedule_remove_all_slotframes();
  sf = tsch_schedule_add_slotframe(0, n_slots + 1);
  tsch_schedule_add_link(sf,
                         LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
                         LINK_TYPE_ADVERTISING,
                         &tsch_broadcast_address,
                         0, 0);
  sf = tsch_schedule_add_slotframe(1, n_slots + 1);
  tsch_schedule_add_link(sf,
                         LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
                         LINK_TYPE_ADVERTISING,
                         &tsch_broadcast_address,
                         0, 0);
  tsch_tagalong_clear_table();
  tsch_queue_reset();
}
/*---------------------------------------------------------------------------*/
void
init_tags_slotframe(uint8_t slotframe, uint8_t n_slots)
{
  struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(slotframe);
  tsch_schedule_remove_slotframe(sf);
  sf = tsch_schedule_add_slotframe(slotframe, n_slots + 1);
  tsch_schedule_add_link(sf,
                         /*0,0, */
                         LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
                         LINK_TYPE_ADVERTISING,
                         &tsch_broadcast_address,
                         0, 0);
  tsch_tagalong_clear_table();
  tsch_queue_reset();
}
/*---------------------------------------------------------------------------*/
#if !CONTIKI_TARGET_SKY
static struct simple_udp_connection controll_conn;
static struct uip_udp_conn *mcast_conn;
#define UDP_CLIENT_PORT 9876
#define UDP_SERVER_PORT 6789
#define MCAST_SINK_UDP_PORT 3002 /* Host byte order */
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
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    LOG_INFO("RX Multicast! %s\n", (char *)uip_appdata);

    char *str = malloc(uip_len + 1);
    memcpy(str, uip_appdata, uip_len);
    str[uip_len] = 0;
    LOG_INFO("%c RX on BC conn!!: %s\n", str[0], str);
    if(str[0] == 0x42 && str[1] == 'C') {
      LOG_INFO("Controll connection message!: %s\n", str);
      handle_serial(str + 2);
/*      buff[1] = 'A'; */
/*      simple_udp_sendto(&controll_conn, buff, datalen, sender_addr); */
    }
/*    if (tsch_is_coordinator && data[0] == 0x42 && data[1] == 'A') { */
/*      LOG_INFO("%d: ACK %s\n", deployment_id_from_iid(sender_addr), buff+2); */
/*    } */
  }
  return;
}
/*---------------------------------------------------------------------------*/
static void
udp_uc_rx_callback(struct simple_udp_connection *c,
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
  LOG_INFO("%c RX on UC conn!!: %s\n", data[0], str);
  if(data[0] == 0x42 && data[1] == 'C') {
    LOG_INFO("Controll connection message!: %s\n", str);
    handle_serial(str + 2);
    str[1] = 'A';
    simple_udp_sendto(&controll_conn, str, datalen, sender_addr);
  }
  if(tsch_is_coordinator && data[0] == 0x42 && data[1] == 'A') {
    LOG_INFO("%d: ACK %s\n", deployment_id_from_iid(sender_addr), str + 2);
  }
/*  if (data[0] == 0x42) { */
  /*print_neighbor_report(datalen, data, sender_addr->u8[15]); */
  /*LOG_INFO("Sending ACK.\n"); */
  /*} */
}
/*---------------------------------------------------------------------------*/
void
forward_command(const uint8_t node_id, const char *line)
{
  LOG_INFO("Forwarding command to %d: %s\n", node_id, line);
  static char buff[64];

  buff[0] = 0x42; /* Packet magic byte to identify */
  buff[1] = 'C';
  memcpy(buff + 2, line, strlen(line));
  if(node_id <= 0) {
    uip_udp_packet_send(mcast_conn, buff, strlen(line) + 2);
  } else {
    uip_ipaddr_t dest_addr;
    memset(&dest_addr, 0, sizeof(uip_ipaddr_t));
    dest_addr.u8[0] = (UIP_DS6_DEFAULT_PREFIX >> 8) & 0xFF;
    dest_addr.u8[1] = (UIP_DS6_DEFAULT_PREFIX) & 0xFF;
    deployment_iid_from_id(&dest_addr, node_id);
    simple_udp_sendto(&controll_conn, buff, strlen(line) + 2, &dest_addr);
  }
}
#endif
/*---------------------------------------------------------------------------*/
void
handle_serial(char *line)
{
  uint8_t do_loopback = 1;
  char *original_line = line;
  LOG_INFO("Serial, got line: %s\n", line);
  if(line[0] == '*') {
    line++;
    do_loopback = 0;
    if(line[0] == '*') {
      do_loopback = 1;
    }

    original_line = line;
    char *token;
    /*token = strsep(&line, " ");*/
    token = strtok(line, " ");
    struct tsch_slotframe *sf;

#if CONTIKI_TARGET_SKY
    if(!tsch_is_coordinator) {
      printf("%d: ACK *%c %s\n",
             deployment_id_from_lladdr(&linkaddr_node_addr),
             token[do_loopback],
             line);
    }
#endif

    switch(token[do_loopback]) {
    case 'c':
    case 'C':
      /*token = strsep(&line, " ");*/
      token = strtok(line, " ");
      uint8_t len = atoi(token);
      /*token = strsep(&line, " ");*/
      token = strtok(line, " ");
      uint8_t id = atoi(token);
      if(len != strlen(line)) {
        printf("Wrong command length! IGNORING\n");
        break;
      }

#if CONTIKI_TARGET_SKY
      printf("|%d|%s\n", id, line);
#else
      forward_command(id, line);
#endif
      break;
    case 'f':
    case 'F':
      /*token = strsep(&line, " ");*/
      token = strtok(line, " ");
      init_tags_slotframe(0, atoi(token));
      original_line[2] = ' ';
      break;
    case 's':
    case 'S':
      sf = tsch_schedule_get_slotframe_by_handle(0);
      tsch_tagalong_clear_table();
      while(line != NULL) {
        /*token = strsep(&line, " ");*/
        token = strtok(line, " ");
        uint8_t timeslot = atoi(token);

        /*token = strsep(&line, " ");*/
        token = strtok(line, " ");
        uint8_t type = (token[0] == 'T') ? LINK_OPTION_TX : (
          (token[0] == 'C') ? LINK_OPTION_TX_CARRIER :
          LINK_OPTION_RX);
        tsch_schedule_add_link(sf,
                               type,
                               LINK_TYPE_ADVERTISING,
                               &tsch_broadcast_address,
                               timeslot, 0);

        if(token[0] == 'T') {
          linkaddr_t addr;
          linkaddr_t carr_addr;

          /*token = strsep(&line, " ");*/
          token = strtok(line, " ");
          deployment_lladdr_from_id(&addr, atoi(token));
          /*token = strsep(&line, " ");*/
          token = strtok(line, " ");
          deployment_lladdr_from_id(&carr_addr, atoi(token));
          tsch_tagalong_add_tag(&addr, 0, timeslot, &carr_addr);
        }
      }
      break;
    case 'p':
    case 'P':
      tsch_schedule_print();

      nbr_tag_t *current = nbr_table_head(nbr_tags);
      while(current != NULL) {
        LOG_INFO_LLADDR(nbr_table_get_lladdr(nbr_tags, current));
        LOG_INFO_(" SF: %d TS: %d\n",
                  current->slotframe,
                  current->timeslot
                  );
        current = nbr_table_next(nbr_tags, current);
      }

      break;
    case 'm':
    case 'M':
      /*token = strsep(&line, " ");*/
      token = strtok(line, " ");
      measure_cycles = atoi(token);
      if(measure_cycles > 0) {
        current_measure_cycle = 0;
        energest_init();
        sf = tsch_schedule_get_slotframe_by_handle(1);
        tsch_schedule_remove_slotframe(sf);
      }
      break;
    case 'r':
    case 'R':
      watchdog_reboot();
      break;
#if CONTIKI_TARGET_SKY
    case 'n':
    case 'N':
      printf("%d: ACK %s\n",
             deployment_id_from_lladdr(&linkaddr_node_addr),
             "*N");
      break;
#endif
    }
  }
  if(do_loopback) {
    printf("%s\n", original_line);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(tagalong_process, ev, data)
{
 static struct etimer timer;

  PROCESS_BEGIN();

  /* Setup a periodic timer that expires after 10 seconds. */
  etimer_set(&timer, CLOCK_SECOND * 10);
    /* Wait for the periodic timer to expire and then restart the timer. */
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

  printf("Link size: %d\n", LINKADDR_SIZE);
  int i;
  for(i = 0; i < LINKADDR_SIZE; i++) {
    printf("0x%02x,", linkaddr_node_addr.u8[i]);
  }
  printf("\n");

  deployment_lladdr_from_id(&coordinator_addr, TAGALONG_COORDINATOR_ID);

  LOG_DBG("COORD_ID: %d MY_ID: %d MY_MAC: ", TAGALONG_COORDINATOR_ID, deployment_id_from_lladdr(&linkaddr_node_addr));
  LOG_DBG_LLADDR(&linkaddr_node_addr);
  LOG_DBG_("\n");

  tsch_set_coordinator(0);
  if(linkaddr_cmp(&linkaddr_node_addr, &coordinator_addr)) {
    printf("Starting as coordinator!!!\n");
    tsch_set_coordinator(1);

    NETSTACK_ROUTING.root_start();
  }
  NETSTACK_MAC.on();

  init_control_slotframe(6);

  neighbor_discovery_start(&tagalong_process);
  PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
  process_start(&tagalong_worker_process, NULL);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(tagalong_worker_process, ev, data)
{
  static struct etimer periodic_timer;
  PROCESS_BEGIN();

  printf("Link size: %d\n", LINKADDR_SIZE);
  int i;
  for(i = 0; i < LINKADDR_SIZE; i++) {
    printf("0x%02x,", linkaddr_node_addr.u8[i]);
  }
  printf("\n");

  tsch_tagalong_clear_table();
  tsch_tagalong_set_reply_callback(tag_reply_callback);

#if !CONTIKI_TARGET_SKY
  /* Initialize UDP connection */
  if(tsch_is_coordinator) {
    simple_udp_register(&controll_conn, UDP_SERVER_PORT, NULL,
                        UDP_CLIENT_PORT, udp_uc_rx_callback);
    prepare_mcast();
  } else {
    simple_udp_register(&controll_conn, UDP_CLIENT_PORT, NULL,
                        UDP_SERVER_PORT, udp_uc_rx_callback);
    /*if(join_mcast_group() == NULL) { */
    /*  LOG_INFO("Failed to join multicast group\n"); */
    /*  PROCESS_EXIT(); */
    /*} */
    mcast_conn = udp_new(NULL, UIP_HTONS(0), NULL);
    udp_bind(mcast_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));
    LOG_INFO("Listening: ");
    LOG_INFO_6ADDR(&mcast_conn->ripaddr);
    LOG_INFO_(" local/remote port %u/%u\n",
              UIP_HTONS(mcast_conn->lport), UIP_HTONS(mcast_conn->rport));
  }
#endif

  etimer_set(&periodic_timer, TAGALONG_INTERROGATION_INTERVAL);
  while(1) {
    PROCESS_YIELD_UNTIL(
      ev == serial_line_event_message ||
#if !CONTIKI_TARGET_SKY
      ev == tcpip_event ||
#endif
      etimer_expired(&periodic_timer));

    if(ev == serial_line_event_message && data != NULL) {
      handle_serial((char *)data);
    } else if(etimer_expired(&periodic_timer)) {
      lattency_dummy_send();
      interrogate_all_tags();
      etimer_set(&periodic_timer, TAGALONG_INTERROGATION_INTERVAL +
                 (random_rand() % CLOCK_SECOND));

      if(current_measure_cycle != 0 &&
         current_measure_cycle >= measure_cycles) {
        current_measure_cycle = 0;
        measure_cycles = 0;

        sf = tsch_schedule_add_slotframe(1, 7);
        tsch_schedule_add_link(sf,
                               LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
                               LINK_TYPE_ADVERTISING,
                               &tsch_broadcast_address,
                               0, 0);
        energest_report();
      }
      if(current_measure_cycle < measure_cycles) {
        current_measure_cycle++;
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
  PROCESS_END();
}

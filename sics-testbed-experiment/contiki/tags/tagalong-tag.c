/*
 * Copyright (c) 2019, Uppsala University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Simple example of carrier-assisted tag broadcast
 * \author
 *         Carlos Perez Penichet
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"

#include "../tagalong-const.h"

#include <string.h>

#include "dev/leds.h"

#include "sys/log.h"
#define LOG_MODULE "TagAlong Tag"
#define LOG_LEVEL LOG_LEVEL_INFO

#define RTIMER_MILLI (RTIMER_SECOND / 1000)
#define WAIT_BEFORE_TX (14 * RTIMER_MILLI)

static struct rtimer rt;
static rtimer_clock_t ref_time;

/*---------------------------------------------------------------------------*/
PROCESS(transmit_process, "Transmit process");
AUTOSTART_PROCESSES(&transmit_process);

void
do_tx(struct rtimer *rtimer, void *dst)
{
  leds_toggle(LEDS_RED);

  linkaddr_t dest_node = *(linkaddr_t *)dst;

  char replyMessage[2];
  replyMessage[0] = TAGALONG_APP_CODE;
  replyMessage[1] = TAGALONG_MESSAGE_TAGREPLY;
  nullnet_buf = (uint8_t *)replyMessage;
  nullnet_len = 2;
  NETSTACK_NETWORK.output(&dest_node);
  leds_off(LEDS_RED);
}
/*---------------------------------------------------------------------------*/
void
input_callback(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dst)
{
  uint8_t *msg;
  msg = (uint8_t *)data;
  if(msg[0] == TAGALONG_APP_CODE && msg[1] == TAGALONG_MESSAGE_TAGINTERROGATE) {
    linkaddr_t dst_tag;
    memcpy(&dst_tag, msg + 2, LINKADDR_SIZE);
    if(linkaddr_cmp(&dst_tag, &linkaddr_node_addr)) {
      ref_time = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP) + WAIT_BEFORE_TX;
      rtimer_set(&rt, ref_time, 1, (void (*)(struct rtimer *, void *))do_tx, (void *)src);
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(transmit_process, ev, data)
{
  static struct  etimer periodic_timer;

  PROCESS_BEGIN();

  nullnet_set_input_callback(input_callback);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

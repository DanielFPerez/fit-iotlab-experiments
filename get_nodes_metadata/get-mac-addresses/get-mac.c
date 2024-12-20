/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/linkaddr.h"
#include "lib/random.h" // Include for random_rand()

#include <stdio.h> /* For printf() */

/*---------------------------------------------------------------------------*/
PROCESS(get_mac_process, "Get mac address process");
AUTOSTART_PROCESSES(&get_mac_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(get_mac_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  // Base delay of 120 seconds
  clock_time_t base_delay = CLOCK_SECOND * 120;
  // Random delay between 1 and 20 seconds
  clock_time_t random_delay = CLOCK_SECOND * (1 + (random_rand() % 15));

  // Set timer to 120 seconds + random delay between 1 and 20 seconds
  etimer_set(&timer, base_delay + random_delay);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

  // Print MAC address in hexadecimal format
  printf("MAC address: { ");
  for (int i = 0; i < LINKADDR_SIZE; i++) {
    printf("%d", linkaddr_node_addr.u8[i]);
    if (i < LINKADDR_SIZE - 1) {
      printf(".");
    }
  }
  printf(" }\n");

  /* Setup a periodic timer that expires after 10 seconds. */
  etimer_set(&timer, CLOCK_SECOND * 60);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
  etimer_set(&timer, CLOCK_SECOND * 60);
  
  while(1) {
    // printf("Hello, world\n");
    /* Wait for the periodic timer to expire and then restart the timer. */
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

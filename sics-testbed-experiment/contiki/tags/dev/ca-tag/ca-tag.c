/*
 * Copyright (c) 2018, Uppsala University.
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
 *         Carrier-assisted tag driver
 * \author
 *         Carlos Perez Penichet
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "contiki.h"
#include "sys/energest.h"

#if defined(__AVR__)
#include <avr/io.h>
#endif

#include "dev/leds.h"
#include "dev/spi-legacy.h"
#include "ca-tag.h"

#include "net/packetbuf.h"
#include "net/netstack.h"

#define CHECKSUM_LEN        2
#define FOOTER_LEN          2

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while (0)
#endif

#define DEBUG_LEDS DEBUG
#undef LEDS_ON
#undef LEDS_OFF
#if DEBUG_LEDS
#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#else
#define LEDS_ON(x)
#define LEDS_OFF(x)
#endif

/* XXX hack: these will be made as Chameleon packet attributes */
rtimer_clock_t ca_tag_time_of_arrival, ca_tag_time_of_departure;

volatile uint16_t ca_tag_sfd_start_time;
volatile uint16_t ca_tag_sfd_end_time;
volatile uint16_t ca_tag_sfd_counter;

static volatile uint16_t last_packet_timestamp;
/*---------------------------------------------------------------------------*/
PROCESS(ca_tag_process, "ca-tag driver");
/*---------------------------------------------------------------------------*/

void cc2420_arch_init(void);

int ca_tag_on(void);
int ca_tag_off(void);

static int ca_tag_read(void *buf, unsigned short bufsize);

static int ca_tag_prepare(const void *data, unsigned short len);
static int ca_tag_transmit(unsigned short len);
static int ca_tag_send(const void *data, unsigned short len);

static int ca_tag_receiving_packet(void);
static int pending_packet(void);
static int ca_tag_cca(void);

static uint8_t receive_on;
static uint8_t packetbuf_cursor;

static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
const struct radio_driver ca_tag_driver =
{
  ca_tag_init,
  ca_tag_prepare,
  ca_tag_transmit,
  ca_tag_send,
  ca_tag_read,
  ca_tag_cca,
  ca_tag_receiving_packet,
  pending_packet,
  ca_tag_on,
  ca_tag_off,
  get_value,
  set_value,
  get_object,
  set_object
};

/*---------------------------------------------------------------------------*/
/* Sends a strobe */
static void
strobe(enum ca_tag_register regname)
{
  TAG_SPI_ENABLE();
  SPI_WRITE(regname);
  TAG_SPI_DISABLE();
}
/*---------------------------------------------------------------------------*/
static void
write_fifo_buf(const uint8_t *buffer, uint16_t count)
{
  uint8_t i;

  TAG_SPI_ENABLE();
  SPI_WRITE_FAST(TAG_TXFIFO);
  for(i = 0; i < count; i++) {
    SPI_WRITE_FAST((buffer)[i]);
  }
  SPI_WAITFORTx_ENDED();
  TAG_SPI_DISABLE();
}
/*---------------------------------------------------------------------------*/
static void
getrxdata(uint8_t *buffer, int count)
{
  uint8_t i;

  TAG_SPI_ENABLE();
  SPI_WRITE(TAG_RXFIFO);
  (void)SPI_RXBUF;
  for(i = 0; i < count; i++) {
    SPI_READ(buffer[i]);
  }
  clock_delay(1);
  TAG_SPI_DISABLE();
}
/*---------------------------------------------------------------------------*/
static void
flushrx(void)
{
  uint8_t dummy;

  getrxdata(&dummy, 1);
  strobe(TAG_SFLUSHRX);
  strobe(TAG_SFLUSHRX);
  if(dummy) {
    /* avoid unused variable compiler warning */
  }
}
/*---------------------------------------------------------------------------*/
static void
wait_for_transmission(void)
{
  rtimer_clock_t t0;
  t0 = RTIMER_NOW();
  PRINTF("ca_tag: waiting for TX: %d\n", TAG_SFD_IS_1);
  while(TAG_SFD_IS_1
        && RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + (RTIMER_SECOND / 10)));
  PRINTF("ca_tag: done waiting for TX: %d\n", TAG_SFD_IS_1);
}
/*---------------------------------------------------------------------------*/
void
reset_packetbuf()
{
  packetbuf_clear();
  packetbuf_cursor = 0;
}
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  PRINTF("ca_tag: actual on\n");

  strobe(TAG_SRXON);
  ENERGEST_ON(ENERGEST_TYPE_LISTEN);
  receive_on = 1;
}
/*---------------------------------------------------------------------------*/
static void
off(void)
{
  PRINTF("ca_tag: actual off\n");
  receive_on = 0;

  /* Wait for transmission to end before turning radio off. */
  wait_for_transmission();

  ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
  strobe(TAG_SOFF);
}
/*---------------------------------------------------------------------------*/
static uint8_t locked, lock_on, lock_off;
#define GET_LOCK() locked++
static void
RELEASE_LOCK(void)
{
  if(locked == 1) {
    if(lock_on) {
      on();
      lock_on = 0;
    }
    if(lock_off) {
      off();
      lock_off = 0;
    }
  }
  locked--;
}
/*---------------------------------------------------------------------------*/
int
ca_tag_init(void)
{
  PRINTF("ca_tag: ca_tag_driver_init\n");

  cc2420_arch_init();

  /* Set CSN pin as output (Input by default). */
  TAG_CSN_PORT(DIR) |= BV(TAG_CSN_PIN);
  strobe(TAG_SOFF);

  receive_on = 0;

  process_start(&ca_tag_process, NULL);
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
ca_tag_transmit(unsigned short payload_len)
{
  int i;

  GET_LOCK();

  /* The TX FIFO can only hold one packet. Make sure to not overrun
   * FIFO by waiting for transmission to start here and synchronizing
   * with the CC2420_TX_ACTIVE check in cc2420_send.
   *
   * Note that we may have to wait up to 320 us (20 symbols) before
   * transmission starts.
   */
#ifndef CC2420_CONF_SYMBOL_LOOP_COUNT
#error CC2420_CONF_SYMBOL_LOOP_COUNT needs to be set!!!
#else
#define LOOP_20_SYMBOLS CC2420_CONF_SYMBOL_LOOP_COUNT
#endif

  /* Initiate transmission */
  strobe(TAG_STXON);

  for(i = LOOP_20_SYMBOLS; i > 0; i--) {
    if(TAG_SFD_IS_1) {
      if(receive_on) {
        ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
      }
      ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
      /* We wait until transmission has ended so that we get an
         accurate measurement of the transmission time.*/
      wait_for_transmission();

      /*#ifdef ENERGEST_CONF_LEVELDEVICE_LEVELS */
      /*      ENERGEST_OFF_LEVEL(ENERGEST_TYPE_TRANSMIT,ca_tag_get_txpower()); */
      /*#endif */
      ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
      if(receive_on) {
        on();
      } else {
        /* We need to explicitly turn off the radio,
         * since STXON[CCA] -> TX_ACTIVE -> RX_ACTIVE */
        off();
      }

      RELEASE_LOCK();
      return RADIO_TX_OK;
    }
  }

  RELEASE_LOCK();
  return RADIO_TX_COLLISION;
}
/*---------------------------------------------------------------------------*/
static int
ca_tag_prepare(const void *payload, unsigned short payload_len)
{
  uint8_t total_len;
  GET_LOCK();

  PRINTF("ca_tag: sending %d bytes\n", payload_len);

  strobe(TAG_SFLUSHTX);
  total_len = payload_len + CHECKSUM_LEN;
  write_fifo_buf(&total_len, 1);
  write_fifo_buf(payload, payload_len);

  RELEASE_LOCK();
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
ca_tag_send(const void *payload, unsigned short payload_len)
{
  ca_tag_prepare(payload, payload_len);
  return ca_tag_transmit(payload_len);
}
/*---------------------------------------------------------------------------*/
int
ca_tag_off(void)
{
  /* Don't do anything if we are already turned off. */
  if(receive_on == 0) {
    return 1;
  }

  /* If we are called when the driver is locked, we indicate that the
     radio should be turned off when the lock is unlocked. */
  if(locked) {
    /*    printf("Off when locked (%d)\n", locked);*/
    lock_off = 1;
    return 1;
  }

  GET_LOCK();
  off();
  RELEASE_LOCK();
  return 1;
}
/*---------------------------------------------------------------------------*/
int
ca_tag_on(void)
{
  if(receive_on) {
    return 1;
  }
  if(locked) {
    lock_on = 1;
    return 1;
  }

  GET_LOCK();
  on();
  RELEASE_LOCK();
  return 1;
}
/*---------------------------------------------------------------------------*/
int
ca_tag_interrupt(void)
{
  CC2420_CLEAR_FIFOP_INT();

  last_packet_timestamp = ca_tag_sfd_start_time;
  process_poll(&ca_tag_process);
  return 1;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ca_tag_process, ev, data)
{
  int len;
  PROCESS_BEGIN();

  PRINTF("ca_tag_process: started\n");

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    PRINTF("ca_tag_process: calling receiver callback\n");

    packetbuf_clear();
    packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, ca_tag_sfd_start_time);
    len = ca_tag_read(packetbuf_dataptr(), PACKETBUF_SIZE);

    packetbuf_set_datalen(len);
    packetbuf_cursor = 0;

    NETSTACK_MAC.input();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static int
ca_tag_read(void *buf, unsigned short bufsize)
{
  /*uint8_t footer[FOOTER_LEN]; */
  uint8_t len;

  if(!TAG_FIFOP_IS_1) {
    return 0;
  }

  GET_LOCK();

  getrxdata(&len, 1);

  /* TODO check length is legal... */

  getrxdata((uint8_t *)buf, len - FOOTER_LEN);

  /* TODO check CRC etc.. */

  flushrx();
  RELEASE_LOCK();

  return len - FOOTER_LEN;
}
/*---------------------------------------------------------------------------*/
static int
ca_tag_cca(void)
{
  return 1;
}
/*---------------------------------------------------------------------------*/
int
ca_tag_receiving_packet(void)
{
  return TAG_SFD_IS_1;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
  return TAG_FIFOP_IS_1;
}
/*---------------------------------------------------------------------------*/

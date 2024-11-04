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
 *         Carrier-assisted tag driver header file
 * \author
 *         Carlos Perez Penichet
 */

#ifndef CA_TAG_H_
#define CA_TAG_H_

#include "contiki.h"
#include "isr_compat.h"

/* P2.1 - Input:  SFD from TAG */
#define TAG_SFD_PORT(type)      P4##type
#define TAG_SFD_PIN             1
#define TAG_SFD_IS_1   (!!(TAG_SFD_PORT(IN) & BV(TAG_SFD_PIN)))
/* P2.0 - Output: SPI Chip Select (CS_N) */
#define TAG_CSN_PORT(type)      P2##type
#define TAG_CSN_PIN             0
/* P1.0 - Input: FIFOP from TAG */
#define TAG_FIFOP_PORT(type)   P1##type
#define TAG_FIFOP_PIN          0
#define TAG_FIFOP_IS_1 (!!(TAG_FIFOP_PORT(IN) & BV(TAG_FIFOP_PIN)))

/*
 * Enables/disables tag access to the SPI bus (not the bus).
 * (Chip Select)
 */
/* ENABLE CSn (active low) */
#define TAG_SPI_ENABLE()     (TAG_CSN_PORT(OUT) &= ~BV(TAG_CSN_PIN))
/* DISABLE CSn (active low) */
#define TAG_SPI_DISABLE()    (TAG_CSN_PORT(OUT) |= BV(TAG_CSN_PIN))
#define TAG_SPI_IS_ENABLED() ((TAG_CSN_PORT(OUT) & BV(TAG_CSN_PIN)) != BV(TAG_CSN_PIN))

enum ca_tag_register {
  TAG_MODE   = 0x00,
  TAG_TXFIFO = 0x01,
  TAG_RXFIFO = 0x02,
  TAG_SOFF   = 0xC0,
  TAG_STXON  = 0xC1,
  TAG_SRXON  = 0xC2,
  TAG_SFLUSHTX = 0xC3,
  TAG_SFLUSHRX = 0xC4
};

#define CHECKSUM_LEN 2

int ca_tag_init(void);
int ca_tag_interrupt(void);

#define CA_TAG_MAX_PACKET_LEN      127

int ca_tag_set_channel(int channel);
int ca_tag_get_channel(void);

void ca_tag_set_pan_addr(unsigned pan,
                         unsigned addr,
                         const uint8_t *ieee_addr);

extern signed char ca_tag_last_rssi;
extern uint8_t ca_tag_last_correlation;

int ca_tag_rssi(void);

extern const struct radio_driver ca_tag_driver;

void ca_tag_set_txpower(uint8_t power);
int ca_tag_get_txpower(void);

/* XXX hack: these will be made as Chameleon packet attributes */
extern rtimer_clock_t ca_tag_time_of_arrival,
                      ca_tag_time_of_departure;
extern int ca_tag_authority_level_of_sender;

int ca_tag_on(void);
int ca_tag_off(void);

void ca_tag_set_cca_threshold(int value);

#endif /* CA_TAG_H_ */

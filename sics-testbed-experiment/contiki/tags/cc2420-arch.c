/*
 * Copyright (c) 2006, Swedish Institute of Computer Science
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
 */

#include "contiki.h"
#include "contiki-net.h"

#include "dev/spi-legacy.h"
#include "ca-tag.h"
#include "isr_compat.h"

#ifdef CC2420_CONF_SFD_TIMESTAMPS
#define CONF_SFD_TIMESTAMPS CC2420_CONF_SFD_TIMESTAMPS
#endif /* CC2420_CONF_SFD_TIMESTAMPS */

#ifndef CONF_SFD_TIMESTAMPS
#define CONF_SFD_TIMESTAMPS 0
#endif /* CONF_SFD_TIMESTAMPS */

extern volatile uint8_t ca_tag_sfd_counter;
extern volatile uint16_t ca_tag_sfd_start_time;
extern volatile uint16_t ca_tag_sfd_end_time;

/*---------------------------------------------------------------------------*/
/* SFD interrupt for timestamping radio packets */
ISR(TIMERB1, cc2420_timerb1_interrupt)
{
  int tbiv;

  /* always read TBIV to clear IFG */
  tbiv = TBIV;
  /* read and discard tbiv to avoid "variable set but not used" warning */
  (void)tbiv;
  if(TAG_SFD_IS_1) {
    ca_tag_sfd_counter++;
    ca_tag_sfd_start_time = TBCCR1;
  } else {
    ca_tag_sfd_counter = 0;
    ca_tag_sfd_end_time = TBCCR1;
  }
}
/*---------------------------------------------------------------------------*/
void
ca_tag_arch_sfd_init(void)
{
  /* Need to select the special function! */
  TAG_SFD_PORT(SEL) = BV(TAG_SFD_PIN);

  /* start timer B - 32768 ticks per second */
  TBCTL = TBSSEL_1 | TBCLR;

  /* CM_3 = capture mode - capture on both edges */
  TBCCTL1 = CM_3 | CAP | SCS;
  TBCCTL1 |= CCIE;

  /* Start Timer_B in continuous mode. */
  TBCTL |= MC1;

  TBR = RTIMER_NOW();
}
/*---------------------------------------------------------------------------*/
ISR(CC2420_IRQ, cc2420_port1_interrupt)
{
  if(ca_tag_interrupt()) {
    LPM4_EXIT;
  }
}
/*---------------------------------------------------------------------------*/
void
cc2420_arch_init(void)
{
  spi_init();

  /* all input by default, set these as output */
  TAG_CSN_PORT(DIR) |= BV(TAG_CSN_PIN);

#if CONF_SFD_TIMESTAMPS
  ca_tag_arch_sfd_init();
#endif

  TAG_SPI_DISABLE();                /* Unselect radio. */
}
/*---------------------------------------------------------------------------*/

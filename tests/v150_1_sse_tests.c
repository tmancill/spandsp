/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v150_1_sse_tests.c - Test V.150_1 SSE processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2022 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

/*! \page v150_1_sse_tests_page V.150_1 tests
\section v150_1_sse_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <ctype.h>
#include <termios.h>
#include <errno.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#include "pseudo_terminals.h"
#include "socket_dgram_harness.h"

socket_dgram_harness_state_t *dgram_state;
v150_1_sse_state_t *v150_1_sse_state;

int pace_no = 0;

span_timestamp_t pace_timer = 0;
span_timestamp_t app_timer = 0;

bool send_messages = false;

int seq = 0;
int timestamp = 0;

static void terminal_callback(void *user_data, const uint8_t msg[], int len)
{
    int i;

    printf("terminal callback %d\n", len);
    for (i = 0;  i < len;  i++)
    {
        printf("0x%x ", msg[i]);
    }
    printf("\n");
    /* TODO: connect AT input to V.150.1 SSE */
}
/*- End of function --------------------------------------------------------*/

static int termios_callback(void *user_data, struct termios *termios)
{
    //data_modems_state_t *s;

    //s = (data_modems_state_t *) user_data;
    printf("termios callback\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void hangup_callback(void *user_data, int status)
{
}
/*- End of function --------------------------------------------------------*/

static int terminal_free_space_callback(void *user_data)
{
    return 42;
}
/*- End of function --------------------------------------------------------*/

static void rx_callback(void *user_data, const uint8_t buff[], int len)
{
    v150_1_sse_rx_packet((v150_1_sse_state_t *) user_data, seq, timestamp, buff, len);
    seq++;
    timestamp += 160;
}
/*- End of function --------------------------------------------------------*/

static int tx_callback(void *user_data, uint8_t buff[], int len)
{
    int res;

    res = v150_1_sse_tx_packet((v150_1_sse_state_t *) user_data, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V32BIS_AA, 0);
    return res;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(void *user_data, const uint8_t pkt[], int len)
{
    int i;
    int n;
    socket_dgram_harness_state_t *s;
    int sent_len;
    
    n = (int) (intptr_t) user_data;    
    printf("Message");
    for (i = 0;  i < len;  i++)
        printf(" %02x", pkt[i]);
    /*endfor*/
    printf("\n");

    s = (socket_dgram_harness_state_t *) user_data;
    /* We need a packet loss mechanism here */
    //if ((rand() % 20) != 0)
    {
        fprintf(stderr, "Pass\n");
        if ((sent_len = sendto(s->net_fd, pkt, len, 0, (struct sockaddr *) &s->far_addr, s->far_addr_len)) < 0)
        {
            if (errno != EAGAIN)
            {
                fprintf(stderr, "Error: Net write: %s\n", strerror(errno));
                return -1;
            }
            /*endif*/
            /* TODO: */
        }
        /*endif*/
        if (sent_len != len)
            fprintf(stderr, "Net write = %d\n", sent_len);
        /*endif*/
    }
    //else
    //{
    //    fprintf(stderr, "Block\n");
    //}
    /*endif*/

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int status_handler(void *user_data, int status)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void paced_operations(void)
{
    //fprintf(stderr, "Pace at %lu\n", now_us());

    if (send_messages  &&   (pace_no & 0x3F) == 0)
    {
        fprintf(stderr, "Sending message\n");
        printf("Sending message\n");
        if (v150_1_sse_tx_packet(v150_1_sse_state, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V32BIS_AA, 0) != 0)
            fprintf(stderr, "ERROR: Failed to send message\n");
        /*endif*/

        //v150_1_sse_tx_packet(v150_1_sse_state, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_V8_CM, 0x123);

        //v150_1_sse_tx_packet(v150_1_sse_state, V150_1_SSE_MEDIA_STATE_MODEM_RELAY, V150_1_SSE_RIC_CLEARDOWN, V150_1_SSE_RIC_INFO_CLEARDOWN_ON_HOOK << 8);
    }
    /*endif*/

    pace_no++;
}
/*- End of function --------------------------------------------------------*/

static void timer_callback(void *user_data)
{
    span_timestamp_t now;

    now = now_us();
    if (now >= pace_timer)
    {
        //fprintf(stderr, "Pace timer expired at %lu\n", now);
        paced_operations();
        pace_timer += 20000;
    }
    /*endif*/
    if (app_timer  &&  now >= app_timer)
    {
        fprintf(stderr, "V.150.1 SSE timer expired at %lu\n", now);
        app_timer = 0;
        v150_1_sse_timer_expired((v150_1_sse_state_t *) user_data, now);
    }
    /*endif*/
    if (app_timer  &&  app_timer < pace_timer)
        socket_dgram_harness_timer = app_timer;
    else
        socket_dgram_harness_timer = pace_timer;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static span_timestamp_t timer_handler(void *user_data, span_timestamp_t timeout)
{
    span_timestamp_t now;

    now = now_us();
    if (timeout == 0)
    {
        fprintf(stderr, "V.150_1 SSE timer stopped at %lu\n", now);
        app_timer = 0;
        //socket_dgram_harness_timer = pace_timer;
    }
    else if (timeout == ~0)
    {
        fprintf(stderr, "V.150_1 SSE get the time %lu\n", now);
        /* Just return the current time */
    }
    else
    {
        fprintf(stderr, "V.150_1 SSE timer set to %lu at %lu\n", timeout, now);
        if (timeout < now)
            timeout = now;
        /*endif*/
        app_timer = timeout;
        //if (app_timer < pace_timer)
        //    socket_dgram_harness_timer = timeout;
        /*endif*/
    }
    /*endif*/
    return now;
}
/*- End of function --------------------------------------------------------*/

static int v150_1_sse_tests(bool calling_party)
{
    logging_state_t *logging;

    send_messages = true; //calling_party;

    if ((dgram_state = socket_dgram_harness_init(NULL,
                                                 (calling_party)  ?  "/tmp/sse_socket_a"  :  "/tmp/sse_socket_b",
                                                 (calling_party)  ?  "/tmp/sse_socket_b"  :  "/tmp/sse_socket_a",
                                                 (calling_party)  ?  "C"  :  "A",
                                                 calling_party,
                                                 terminal_callback,
                                                 termios_callback,
                                                 hangup_callback,
                                                 terminal_free_space_callback,
                                                 rx_callback,
                                                 tx_callback,
                                                 timer_callback,
                                                 v150_1_sse_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start the socket harness\n");
        exit(2);
    }
    /*endif*/

    if ((v150_1_sse_state = v150_1_sse_init(NULL,
                                            tx_packet_handler,
                                            dgram_state,
                                            status_handler,
                                            dgram_state,
                                            timer_handler,
                                            dgram_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start V.150.1 SSE\n");
        exit(2);
    }
    /*endif*/
    socket_dgram_harness_set_user_data(dgram_state, v150_1_sse_state);

    logging = v150_1_sse_get_logging_state(v150_1_sse_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, "0");

    v150_1_sse_explicit_acknowledgements(v150_1_sse_state, true);

    socket_dgram_harness_timer = 
    pace_timer = now_us() + 20000;
    socket_dgram_harness_run(dgram_state);

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int opt;
    bool calling_party;

    calling_party = false;
    while ((opt = getopt(argc, argv, "ac")) != -1)
    {
        switch (opt)
        {
        case 'a':
            calling_party = false;
            break;
        case 'c':
            calling_party = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
        /*endswitch*/
    }
    /*endwhile*/

    if (v150_1_sse_tests(calling_party))
        exit(2);
    /*endif*/
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

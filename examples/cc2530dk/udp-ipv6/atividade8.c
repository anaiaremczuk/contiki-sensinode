/*
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

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/bmp280-sensor.h"
#include "dev/bmp280-sensor.c"
#include "debug.h"

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#define SEND_INTERVAL		1 * CLOCK_SECOND
#define MAX_PAYLOAD_LEN		40

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

static char buf[MAX_PAYLOAD_LEN];
static uint16_t TEMP_C=0;
static uint16_t P_MSB,P_LSB = 0;
static uint32_t P_Pa = 0;

/* Our destinations and udp conns. One link-local and one global */
#define LOCAL_CONN_PORT 8802
static struct uip_udp_conn *l_conn;
#if UIP_CONF_ROUTER
#define GLOBAL_CONN_PORT 8802
static struct uip_udp_conn *g_conn;
#endif
#define LED_TOGGLE_REQUEST (0x79)
#define LED_SET_STATE (0x7A)
#define LED_GET_STATE (0x7B)
#define LED_STATE (0x7C)

#define BMP_SENSOR_TYPE_TEMP 0
#define BMP_SENSOR_TYPE_PRESS_hPa 1
#define BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_MSB 2
#define BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_LSB 3

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
#if BUTTON_SENSOR_ON
PROCESS_NAME(ping6_process);
AUTOSTART_PROCESSES(&udp_client_process, &ping6_process);
#else
AUTOSTART_PROCESSES(&udp_client_process);
#endif
/*---------------------------------------------------------------------------*/
static void tcpip_handler ( void )
{
	char i=0;
	#define SEND_ECHO (0xBA)
	if( uip_newdata ()) // verifica se novos dados foram recebidos
	{
		char * dados = (( char *) uip_appdata ); // este buffer eh pardao do contiki
		PRINTF(" Recebidos %d bytes \n" , uip_datalen ());
		switch (dados [0])
		{
		case SEND_ECHO :
		{
			uip_ipaddr_copy (& g_conn -> ripaddr , & UIP_IP_BUF -> srcipaddr );
			g_conn -> rport = UIP_UDP_BUF -> destport ;
			uip_udp_packet_send (g_conn , dados , uip_datalen ());
			PRINTF(" Enviando eco para [");
			PRINT6ADDR (& g_conn -> ripaddr );
			PRINTF("]:% u\n", UIP_HTONS (g_conn -> rport ));
			break ;
		}
		case LED_GET_STATE:
		{
			PRINTF(" LED_GET_STATE Recebido :");
			for (i=0;i<uip_datalen ();i++)
			{
				PRINTF("0x%02X" ,dados [i]);
			}

			memset(buf, LED_STATE + leds_get(), MAX_PAYLOAD_LEN);
			PRINTF("0x%02X", LED_STATE + leds_get());
			uip_udp_packet_send(g_conn, buf, 1);

			break;
		}
		case LED_SET_STATE:
		{
			leds_off(LEDS_ALL);
			PRINTF(" LED_SET_STATE Recebido :");
			for (i=0;i<uip_datalen ();i++)
			{
				PRINTF("0x%02X" ,dados [i]);
			}
			leds_on(dados[1]);

			buf[0]=LED_STATE;
			buf[1]=leds_get();
			PRINTF("\n0x%02Xx%02X", buf[0], buf[1]);
			uip_ipaddr_copy (& g_conn -> ripaddr , & UIP_IP_BUF -> srcipaddr );
			g_conn -> rport = UIP_UDP_BUF -> destport ;
			uip_udp_packet_send(g_conn, buf, 2);

			break;
		}
		default :
		{
			PRINTF(" Comando Invalido :");
			for (i=0;i<uip_datalen ();i++)
			{
				PRINTF("0x%02X" ,dados [i]);
			}
			PRINTF("\n");
			break ;
		}
		}
	}
	return;

}
/*---------------------------------------------------------------------------*/
static void
timeout_handler(void)
{  
  memset(buf, 0, MAX_PAYLOAD_LEN); //zera o buffer global


  if (uip_ds6_get_global(ADDR_PREFERRED)==NULL)
  {
	  PRINTF("O No ainda nao tem um IP global Valido\n");
  }
  else
  {

	  TEMP_C = value(BMP_SENSOR_TYPE_TEMP);
	  P_MSB = value(BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_MSB);
	  P_LSB = value(BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_LSB);

	  P_Pa = 65536 * (uint32_t)P_MSB + P_LSB;
	  //PRINTF("\n Cliente para [");
	  //PRINT6ADDR(&g_conn->ripaddr);
	  //PRINTF("]:%u,", UIP_HTONS(g_conn->rport));
	  PRINTF("\n Temperatura (C): %f", TEMP_C*0.01);
	  PRINTF("\n Pressao (hPa): %u", value(BMP_SENSOR_TYPE_PRESS_hPa));
	  PRINTF("\n Pressao (MSB): %u", value(2));
	  PRINTF("\n Pressao (LSB): %u", value(3));
	  PRINTF("\n Pressao (Pa): %lu", P_Pa);
	  //memset(buf, LED_TOGGLE_REQUEST, MAX_PAYLOAD_LEN);
	  //uip_udp_packet_send(g_conn, buf, 1);

		buf[0]=(uint8_t)(TEMP_C & 0xff);
		buf[1]=(uint8_t)(TEMP_C >> 8);

		buf[2]=(uint8_t)(P_MSB & 0xff);
		buf[3]=(uint8_t)(P_MSB >> 8);

		buf[4]=(uint8_t)(P_LSB & 0xff);
		buf[5]=(uint8_t)(P_LSB >> 8);

		//uip_ipaddr_copy (& g_conn -> ripaddr , & UIP_IP_BUF -> srcipaddr );
		//g_conn -> rport = UIP_UDP_BUF -> destport ;
		uip_udp_packet_send(g_conn, buf, 6);


  }

  

}
/*---------------------------------------------------------------------------*/

static void
print_local_addresses(void)
{
  int i;
  uint8_t state;
  PRINTF("\nNodes's IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINTF("  \n");
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      if(state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}

PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer et;
  uip_ipaddr_t ipaddr;

  PROCESS_BEGIN();
  PRINTF("UDP client process started\n");

  //uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0x0212, 0x4b00, 0x07b9, 0x5ecc);
  //uip_ip6addr(&ipaddr, 0xaaaa, 0,0,0,0x212, 0x4b00,0x7c3 , 0xb5e4);
  uip_ip6addr(&ipaddr, 0xbbbb, 0,0,0,0xa00, 0x27ff,0xfe1e , 0xff44);

  /* new connection with remote host */
  l_conn = udp_new(&ipaddr, UIP_HTONS(LOCAL_CONN_PORT), NULL);
  if(!l_conn) {
    PRINTF("udp_new l_conn error.\n");
  }
  udp_bind(l_conn, UIP_HTONS(LOCAL_CONN_PORT));

  PRINTF("Link-Local connection with ");
  PRINT6ADDR(&l_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
         UIP_HTONS(l_conn->lport), UIP_HTONS(l_conn->rport));

  //uip_ip6addr(&ipaddr, 0xbbbb, 0,0,0,0x45d1, 0x8c36,0x41ab , 0x112d);
  uip_ip6addr(&ipaddr, 0xbbbb, 0,0,0,0xa00, 0x27ff,0xfe1e , 0xff44);
  //uip_ip6addr(&ipaddr, 0xbbbb, 0,0,0,0x6409, 0x352d,0x301b , 0x26b6);
  //bbbb::6409:352d:301b:26b6
  //bbbb::a00:27ff:fe1e:ff44
  //bbbb::e1b5:4bcd:3f68:8b8f

  g_conn = udp_new(&ipaddr, UIP_HTONS(GLOBAL_CONN_PORT), NULL);
  if(!g_conn) {
    PRINTF("udp_new g_conn error.\n");
  }
  udp_bind(g_conn, UIP_HTONS(GLOBAL_CONN_PORT));

  print_local_addresses();

  PRINTF("Global connection with ");
  PRINT6ADDR(&g_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
         UIP_HTONS(g_conn->lport), UIP_HTONS(g_conn->rport));

  etimer_set(&et, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT();
    //print_local_addresses();
    if(etimer_expired(&et)) {
      timeout_handler();
      etimer_restart(&et);
    } else if(ev == tcpip_event) {
    	//printf("Um pacote udp  foi recebido \n");
      tcpip_handler();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

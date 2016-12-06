/*
  Basic MQTT-SN client library
  Copyright (C) 2013 Nicholas Humfrey

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Modifications:
  Copyright (C) 2013 Adam Renner
*/



#include <string.h>
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "debug.h"

#define DEBUG DEBUG_PRINT

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "stack.h"

#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "mqtt-sn.h"
#include "list.h"
#include "net/rime.h"

//#include "simple-udp.h"
#include "net/uip-debug.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 1883

#define REQUEST_RETRIES 4
#define DEFAULT_SEND_INTERVAL		(10 * CLOCK_SECOND)
#define REPLY_TIMEOUT (3 * CLOCK_SECOND)

extern mqtt_sn_request* requests;

struct mqtt_sn_connection mqtt_sn_c;
struct uip_udp_conn *g_conn;

static char mqtt_client_id[]="sensor";
static char ctrl_topic[] = "0000000000000000/ctrl";//of form "0011223344556677/ctrl" it is null terminated, and is 21 charactes
static char pub_topic[] = "0000000000000000/msg";
static uint16_t ctrl_topic_id=0xaa;
static uint16_t publisher_topic_id;
static publish_packet_t incoming_packet;
static uint16_t ctrl_topic_msg_id;
static uint16_t reg_topic_msg_id;
static uint16_t mqtt_keep_alive=12;
static int8_t qos = 1;
static uint8_t retain = FALSE;
static char device_id[17];
static clock_time_t send_interval;
//uint8_t debug = FALSE;

static enum mqttsn_connection_status connection_state = MQTTSN_DISCONNECTED;

/*A few events for managing device state*/
static process_event_t mqttsn_connack_event;

PROCESS(example_mqttsn_process, "Configure Connection and Topic Registration");
PROCESS(publish_process, "register topic and publish data");
PROCESS(ctrl_subscription_process, "subscribe to a device control channel");


AUTOSTART_PROCESSES(&example_mqttsn_process);


/*---------------------------------------------------------------------------*/
static void
puback_receiver(/*struct mqtt_sn_connection *mqc,*/  const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  printf("Puback received\n");
}
/*---------------------------------------------------------------------------*/
static void
connack_receiver(/*struct mqtt_sn_connection *mqc,*/  const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  uint8_t connack_return_code;
  connack_return_code = *(data + 3);
  printf("Connack received\n");
  if (connack_return_code == ACCEPTED) {
    process_post(&example_mqttsn_process, mqttsn_connack_event, NULL);
  } else {
    printf("Connack error: %s\n", mqtt_sn_return_code_string(connack_return_code));
  }
}
/*---------------------------------------------------------------------------*/
static void
regack_receiver(/*struct mqtt_sn_connection *mqc,*/  const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  regack_packet_t incoming_regack;
  memcpy(&incoming_regack, data, datalen);
  printf("Regack received\n");
  if (incoming_regack.message_id == reg_topic_msg_id) {
    if (incoming_regack.return_code == ACCEPTED) {
      publisher_topic_id = uip_htons(incoming_regack.topic_id);
    } else {
      printf("Regack error: %s\n", mqtt_sn_return_code_string(incoming_regack.return_code));
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
suback_receiver(/*struct mqtt_sn_connection *mqc,*/ const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  suback_packet_t incoming_suback;
  memcpy(&incoming_suback, data, datalen);
  printf("Suback received\n");
  if (incoming_suback.message_id == ctrl_topic_msg_id) {
    if (incoming_suback.return_code == ACCEPTED) {
      ctrl_topic_id = uip_htons(incoming_suback.topic_id);
      printf("Topic ID: %d\n",ctrl_topic_id);
    } else {
      printf("Suback error: %s\n", mqtt_sn_return_code_string(incoming_suback.return_code));
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_receiver(/*struct mqtt_sn_connection *mqc,*/ const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  memcpy(&incoming_packet, data, datalen);
  printf("Published message received -> ");
  //see if this message corresponds to ctrl channel subscription request
  if (uip_htons(incoming_packet.topic_id) == ctrl_topic_id) {
    //the new message interval will be read from the first byte of the recieved packet
    //send_interval = (uint8_t)incoming_packet.data[0] * CLOCK_CONF_SECOND;
    incoming_packet.data[incoming_packet.length-7]=0;
    printf("%s\n",incoming_packet.data);
  } else {
    printf("unknown publication received - received topic id=%d, expected topic id=%d\n",uip_htons(incoming_packet.topic_id),ctrl_topic_id);
  }

}
/*---------------------------------------------------------------------------*/
/*Add callbacks here if we make them*/
static const struct mqtt_sn_callbacks mqtt_sn_call = {
  publish_receiver,
  NULL,
  NULL,
  connack_receiver,
  regack_receiver,
  puback_receiver,
  suback_receiver,
  NULL,
  NULL
  };

/*---------------------------------------------------------------------------*/
/*this process will publish data at regular intervals*/
PROCESS_THREAD(publish_process, ev, data)
{
  static uint8_t registration_tries;
  static struct etimer send_timer;
  static uint8_t buf_len;
  static uint8_t message_number;
  static char buf[20];

  PROCESS_BEGIN();
  send_interval = DEFAULT_SEND_INTERVAL;
  memcpy(pub_topic,device_id,16);
  printf("registering topic\n");
  registration_tries =0;
  while (registration_tries < REQUEST_RETRIES)
  {
	stack_max_sp_print("register try - 0x");
	stack_dump("current SP: 0x");
    reg_topic_msg_id = mqtt_sn_register_try(pub_topic,REPLY_TIMEOUT);
    PROCESS_WAIT_EVENT_UNTIL(mqtt_sn_request_returned(REGISTER_IDX));
    if (mqtt_sn_request_success(REGISTER_IDX)) {
      registration_tries = 4;
      printf("registration acked\n");
    }
    else {
      registration_tries++;
      if (requests[REGISTER_IDX].state == MQTTSN_REQUEST_FAILED) {
          printf("Regack error: %s\n", mqtt_sn_return_code_string(requests[REGISTER_IDX].return_code));
      }
      else
      {
    	  printf("Regack error (else): %s\n", mqtt_sn_return_code_string(requests[REGISTER_IDX].return_code));
      }
    }
  }
  if (mqtt_sn_request_success(REGISTER_IDX))
  {
	  //start topic publishing to topic at regular intervals
	  etimer_set(&send_timer, send_interval);
	  while(1) {
		  PROCESS_WAIT_EVENT();

		  if(ev == PROCESS_EVENT_TIMER) {

			  sprintf(buf, "Message %d", message_number);
			  printf("publishing '%s' to topic '%s' \n ", buf, pub_topic);
			  message_number++;
			  buf_len = strlen(buf);
			  stack_max_sp_print("send publish - 0x");
			  stack_dump("current SP: 0x");
			  mqtt_sn_send_publish(publisher_topic_id,MQTT_SN_TOPIC_TYPE_NORMAL,buf, buf_len,qos,retain);
			  etimer_set(&send_timer, send_interval);
		  }
	  }
  }
  else
  {
	  printf("unable to register topic\n");
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*this process will create a subscription and monitor for incoming traffic*/
PROCESS_THREAD(ctrl_subscription_process, ev, data)
{
  static uint8_t subscription_tries;

  PROCESS_BEGIN();

  subscription_tries = 0;
  memcpy(ctrl_topic,device_id,16);
  printf("requesting subscription\n");
  while(subscription_tries < REQUEST_RETRIES) {
	stack_max_sp_print("subslcribe try - 0x");
	stack_dump("current SP: 0x");
    ctrl_topic_msg_id = mqtt_sn_subscribe_try(ctrl_topic,0,REPLY_TIMEOUT);
    PROCESS_WAIT_EVENT_UNTIL(mqtt_sn_request_returned(SUBSCRIBE_IDX));
    if (mqtt_sn_request_success(SUBSCRIBE_IDX)) {
      subscription_tries = 4;
      printf("subscription acked\n");
    }
    else {
      subscription_tries++;
      if (requests[SUBSCRIBE_IDX].state == MQTTSN_REQUEST_FAILED) {
          printf("Suback error: %s\n", mqtt_sn_return_code_string(requests[SUBSCRIBE_IDX].return_code));
      }
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*this main process will create connection and register topics*/
/*---------------------------------------------------------------------------*/


static struct ctimer connection_timer;
static process_event_t connection_timeout_event;

static void connection_timer_callback(void *mqc)
{
  process_post(&example_mqttsn_process, connection_timeout_event, NULL);
}

PROCESS_THREAD(example_mqttsn_process, ev, data)
{
  static struct etimer periodic_timer;
  static uip_ipaddr_t broker_addr;
  static uint8_t connection_retries = 0;
  static struct etimer et;
  static uint8_t stack;
  static uint8_t loop=1;
  static uint8_t phases=0;

  PROCESS_BEGIN();

  stack_max_sp_print("inicio do exemplo - 0x");
  stack_dump("current SP: 0x");

   mqttsn_connack_event = process_alloc_event();

  mqtt_sn_set_debug(1);
  //uip_ip6addr(&broker_addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
  //uip_ip6addr(&broker_addr, 0x2001, 0x0db8, 1, 0xffff, 0, 0, 0xc0a8, 0xd480);//192.168.212.128 with tayga
  //uip_ip6addr(&broker_addr, 0xaaaa, 0, 2, 0xeeee, 0, 0, 0xc0a8, 0xd480);//192.168.212.128 with tayga
  //uip_ip6addr(&broker_addr, 0xaaaa, 0, 2, 0xeeee, 0, 0, 0xac10, 0xdc01);//172.16.220.1 with tayga
  uip_ip6addr(&broker_addr, 0xbbbb, 0, 0, 0, 0xa914, 0x34d8, 0xb169, 0x6ae9);//172.16.220.128 with tayga
  //mqtt_sn_create_socket(&mqtt_sn_c,UDP_PORT, &broker_addr, UDP_PORT);

  //simple_udp_register(&(mqc->sock), local_port, remote_addr, remote_port, mqtt_sn_receiver);
  //{
  g_conn = udp_new(&broker_addr, UIP_HTONS(UDP_PORT), NULL);
  if(!g_conn) {
    PRINTF("udp_new g_conn error.\n");
  }
  udp_bind(g_conn, UIP_HTONS(UDP_PORT));
  //}
  mqtt_sn_c.stat = MQTTSN_DISCONNECTED;
  mqtt_sn_c.keep_alive=0;
  mqtt_sn_c.next_message_id = 1;
  mqtt_sn_c.connection_retries = 0;
  //LIST_STRUCT_INIT(&mqtt_sn_c,requests);
  init_request();
  mqtt_sn_request_event = process_alloc_event();
  start_mqttsnproccess();
  (&mqtt_sn_c)->mc = &mqtt_sn_call;

  printf("waiting 10 seconds for the network \n ");

  sprintf(mqtt_client_id,"s%02X%02X",rimeaddr_node_addr.u8[6], rimeaddr_node_addr.u8[7]);

  sprintf(device_id,"%02X%02X%02X%02X%02X%02X%02X%02X",rimeaddr_node_addr.u8[0],
            rimeaddr_node_addr.u8[1],rimeaddr_node_addr.u8[2],rimeaddr_node_addr.u8[3],
            rimeaddr_node_addr.u8[4],rimeaddr_node_addr.u8[5],rimeaddr_node_addr.u8[6],
            rimeaddr_node_addr.u8[7]);

  printf("device ID is %s \nMQTT client ID is %s \n",device_id,mqtt_client_id);
  /*Wait a little to let system get set*/
  etimer_set(&periodic_timer, 10*CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

  stack_max_sp_print("connection request - 0x");
  stack_dump("current SP: 0x");

  /*Request a connection and wait for connack*/
  connection_timeout_event = process_alloc_event();
  ctimer_set( &connection_timer, REPLY_TIMEOUT, connection_timer_callback, NULL);
  mqtt_sn_send_connect(/*&mqtt_sn_c,*/mqtt_client_id,mqtt_keep_alive);
  connection_state = MQTTSN_WAITING_CONNACK;

  etimer_set(&et, 3 * CLOCK_SECOND);

  while(loop) {
	  stack_max_sp_print("connack wait - 0x");
	  stack_dump("current SP: 0x");
	  PROCESS_WAIT_EVENT();
	  if(ev == PROCESS_EVENT_TIMER) {
		  connection_state = MQTTSN_CONNECTION_FAILED;
		  connection_retries++;
		  printf("connection timeout\n");
		  mqtt_sn_send_connect(/*&mqtt_sn_c,*/mqtt_client_id,mqtt_keep_alive);
		  connection_state = MQTTSN_WAITING_CONNACK;
		  etimer_restart(&et);
	  } else if(ev == tcpip_event) {
		  mqtt_sn_receiver();
	  } else  if (ev == mqttsn_connack_event) {
		  //if success
		  printf("connection acked\n");
		  stack_max_sp_print("subscribe thread - 0x");
		  stack_dump("current SP: 0x");
		  ctimer_stop(&connection_timer);
		  connection_state = MQTTSN_CONNECTED;
		  loop = 0;
	  }
  }
  ctimer_stop(&connection_timer);

  etimer_set(&et, 3 * CLOCK_SECOND);
  if (connection_state == MQTTSN_CONNECTED)
  {
	  while(1)
	  {
		  PROCESS_WAIT_EVENT();
		  if(ev == tcpip_event)
		  {
			  //tcpip_handler();
			  mqtt_sn_receiver();
		  }
		  else if (ev == PROCESS_EVENT_TIMER)
		  {
			  if(phases==0)
			  {
				  printf("starting subscription process\n");
				  stack_max_sp_print("subscribe - 0x");
				  stack_dump("current SP: 0x");
				  process_start(&ctrl_subscription_process, 0);
				  etimer_set(&et, 3 * CLOCK_SECOND);
			  }
			  else if(phases==1)
			  {
				  printf("starting publish process\n");
				  stack_max_sp_print("publish - 0x");
				  stack_dump("current SP: 0x");
				  process_start(&publish_process, 0);
				  etimer_set(&et, 3 * CLOCK_SECOND);
			  }
			  phases++;
		  }
	  }
  }
  else
  {
	  printf("unable to connect\n");
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


/*hello-world blink-hello timer-test sensors-demo */

/* This is a very simple hello_world program.
 * It aims to demonstrate the co-existence of two processes:
 * One of them prints a hello world message and the other blinks the LEDs
 *
 * It is largely based on hello_world in $(CONTIKI)/examples/sensinode
 *
 * Author: George Oikonomou - <oikonomou@users.sourceforge.net>
 * Aletrado para a disciplina de Rede de Sensores Sem Fio
 */

#include "contiki.h"
#include "dev/leds.h"

#include <stdio.h> /* For printf() */
/*---------------------------------------------------------------------------*/
#define LED_PING_EVENT (44)
#define LED_PONG_EVENT (45)

static struct etimer et_hello;
static struct etimer et_blink;
static struct etimer et_proc3;
static struct etimer et_blue;
static uint16_t count;
static uint8_t blinks;
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
PROCESS(red_blink_process, "RED LED blink process");
PROCESS(blink_process, "LED blink process");
PROCESS(pong_process, "Pong process");
AUTOSTART_PROCESSES(&blink_process,&hello_world_process,&red_blink_process, &pong_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  PROCESS_BEGIN();

  etimer_set(&et_hello, CLOCK_SECOND * 4);
  count = 0;

  while(1) {
    PROCESS_WAIT_EVENT();

   if(ev == PROCESS_EVENT_TIMER) {
	   printf("hello: pinging...\n");
	   count++;


	   process_post(&pong_process,LED_PING_EVENT,(void*)(&hello_world_process));

	   PROCESS_WAIT_EVENT_UNTIL(ev == LED_PONG_EVENT);
	   leds_toggle(LEDS_GREEN);
	   printf("hello: Led pong \n");

	   etimer_reset(&et_hello);
   }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(pong_process, ev, data)
{
  PROCESS_BEGIN();



  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == LED_PING_EVENT);
    process_post((struct process*)data,LED_PONG_EVENT,(void*)(&pong_process));
    printf("pong: Led ping. Ponging...\n");
    leds_on(LEDS_BLUE);
    etimer_set(&et_blue, CLOCK_SECOND/5);
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    leds_off(LEDS_BLUE);


  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(red_blink_process, ev, data)
{
  PROCESS_BEGIN();

  etimer_set(&et_proc3, CLOCK_SECOND * 5);
  count = 0;

  while(1) {
	  PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
	  leds_toggle(LEDS_RED);
      etimer_reset(&et_proc3);
      printf("red_proc3: pinging...\n");
      process_post(&pong_process,LED_PING_EVENT,(void*)(&red_blink_process));

      PROCESS_WAIT_EVENT_UNTIL(ev == LED_PONG_EVENT);
      leds_toggle(LEDS_RED);
      printf("red_proc3: Led pong\n");

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_process, ev, data)
{
  PROCESS_BEGIN();

  blinks = 0;
  printf("LEDS_ALL = %d\n", LEDS_ALL);

  leds_off(LEDS_ALL);
  etimer_set(&et_blink, 5*CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

  while(1) {
	etimer_set(&et_blink, 2*CLOCK_SECOND);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_toggle(LEDS_YELLOW);
    //leds_on(blinks & LEDS_ALL);
    //blinks++;
    //printf("Blink... (state %X)\n", leds_get());
    //printf("Blink YELLOW (state %X)\n", leds_get());
    printf("blink: pinging...\n");
    process_post(&pong_process,LED_PING_EVENT,(void*)(&blink_process));

    PROCESS_WAIT_EVENT_UNTIL(ev == LED_PONG_EVENT);
    leds_toggle(LEDS_YELLOW);
    printf("blink: Led pong\n");

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

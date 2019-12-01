/* @(#) pfcustom.c 98/01/26 1.3 */

#ifdef PF_USER_CUSTOM

/***************************************************************
** Call Custom Functions for pForth
**
** Create a file similar to this and compile it into pForth
** by setting -DPF_USER_CUSTOM="mycustom.c"
**
** Using this, you could, for example, call X11 from Forth.
** See "pf_cglue.c" for more information.
**
** Author: Phil Burk
** Copyright 1994 3DO, Phil Burk, Larry Polansky, David Rosenboom
**
** The pForth software code is dedicated to the public domain,
** and any third party may reproduce, distribute and modify
** the pForth software code or any derivative works thereof
** without any compensation or license.  The pForth software
** code is provided on an "as is" basis without any warranty
** of any kind, including, without limitation, the implied
** warranties of merchantability and fitness for a particular
** purpose and their equivalents under the laws of any jurisdiction.
**
***************************************************************/

#include "pf_all.h"
#include "mqtt.h"
#include "posix_sockets.h"
#include <unistd.h>

static int connected = 0;
static int debug = 0;

#define QUEUE_LENGTH 256
struct QUEUE {
  char *topic,
    *message;
} message_queue[QUEUE_LENGTH];
static int queue_first = 0,
           queue_end = 0;

struct reconnect_state_t {
  const char *hostname;
  const char *port;
  const char *topic;
  uint8_t *sendbuf;
  size_t sendbufsz;
  uint8_t *recvbuf;
  size_t recvbufsz;
};
static struct reconnect_state_t reconnect_state;
static uint8_t sendbuf[2048];
static uint8_t recvbuf[1024];
static struct mqtt_client client;

static void reconnect_client(struct mqtt_client *client, void **reconnect_state_vptr);
static void publish_callback(void **unused, struct mqtt_response_publish *published);
static void *client_refresher(void *client);

static void forth_mqtt_debug(cell_t flag) {
  debug = flag;
}

static cell_t forth_mqtt_connect(cell_t hostname, cell_t hlen, cell_t topic, cell_t tlen) {
  char buf[2048];
  if (connected) {
    return !0;
  }
  connected = !0;

  {
    int i;
    char *dst = buf, *src = (char *)hostname;
    for (i = 0; i < hlen; i++) {
      *dst++ = *src++;
    }
    *dst = '\0';
  }

  reconnect_state.hostname = strdup(buf);
  reconnect_state.port = "1883";
  {
    int i;
    char *dst = buf, *src = (char *)topic;
    for (i = 0; i < tlen; i++) {
      *dst++ = *src++;
    }
    *dst = '\0';
  }
  reconnect_state.topic = strdup(buf);

  reconnect_state.sendbuf = sendbuf;
  reconnect_state.sendbufsz = sizeof(sendbuf);
  reconnect_state.recvbuf = recvbuf;
  reconnect_state.recvbufsz = sizeof(recvbuf);
  /* setup a client */
  mqtt_init_reconnect(&client, reconnect_client, &reconnect_state, publish_callback);

  /* start a thread to refresh the client (handle egress and ingree client traffic) */
  {
    pthread_t client_daemon;
    if (pthread_create(&client_daemon, NULL, client_refresher, &client)) {
      fprintf(stderr, "Failed to start client daemon.\n");
      exit(1);
    }
  }

  return !0;
}

#ifndef max
#define max(a, b) (a < b ? b : a)
#endif

#ifndef min
#define min(a, b) (a > b ? b : a)
#endif

static void forth_sleep(cell_t seconds) {
  sleep(seconds);
}

static void forth_usleep(cell_t microseconds) {
  usleep(microseconds);
}

static cell_t forth_mqtt_publish(cell_t retain, cell_t message, cell_t msize, cell_t topic, cell_t tsize) {
  char mbuf[2048], tbuf[2048];

  {
    char *dst = tbuf, *src = (char *)topic, count = tsize;
    while (count-- > 0) {
      *dst++ = *src++;
    }
    *dst = '\0';
  }

  {
    char *dst = mbuf, *src = (char *)message, count = msize;
    while (count-- > 0) {
      *dst++ = *src++;
    }
    *dst = '\0';
  }

  if (debug) {
    printf(">>> %s -> %s (%s)\n", tbuf, mbuf, retain ? "retain" : "");
  }
  mqtt_publish(&client, tbuf, mbuf, strlen(mbuf), retain ? MQTT_PUBLISH_RETAIN : MQTT_PUBLISH_QOS_0);
  /* check for errors */
  if (client.error != MQTT_OK) {
    fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
    return 0;
  }
  return !0;
}

/* 
   The count (length) of forth strings are limited to a byte, so we implement a sort of buffer to handle
   longer length data.  Instead of a leading byte, we have a  cell with length, followed by length bytes.
*/
static cell_t forth_mqtt_read(cell_t size, cell_t message, cell_t topic) {
  /*  printf("mqtt-read %lx %lx %ld\n", topic, message, size);*/
  if (queue_first == queue_end) {
    return 0;
  }

  if (debug) {
    printf("MQTT-READ ");
    printf("topic: '%s' ", message_queue[queue_first].topic);
    printf("message: '%s'\n", message_queue[queue_first].message);
  }

  {
    char *src = message_queue[queue_first].topic, *dst = (char *)topic;
    int i, len = min(strlen(src), size);
    cell_t *lptr = (cell_t *)dst;
    *lptr++ = len;
    dst = (char *)lptr;
    for (i = 0; i < len; i++) {
      *dst++ = *src++;
    }
    *dst++ = '\0';
  }

  {
    char *src = message_queue[queue_first].message, *dst = (char *)message;
    int i, len = min(strlen(src), size);
    cell_t *lptr = (cell_t *)dst;
    *lptr++ = len;
    dst = (char *)lptr;
    for (i = 0; i < len; i++) {
      *dst++ = *src++;
    }
    *dst++ = '\0';
  }
  free(message_queue[queue_first].message);
  free(message_queue[queue_first].topic);
  queue_first++;
  queue_first %= QUEUE_LENGTH;
  return !0;
}

void reconnect_client(struct mqtt_client *client, void **reconnect_state_vptr) {
  struct reconnect_state_t *reconnect_state = *((struct reconnect_state_t **)reconnect_state_vptr);
  int sockfd;
  /* Close the clients socket if this isn't the initial reconnect call */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    close(client->socketfd);
  }
  /* Perform error handling here. */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    printf("reconnect_client: called while client was in error state \"%s\"\n",
      mqtt_error_str(client->error));
  }
  /* Open a new socket. */
  sockfd = open_nb_socket(reconnect_state->hostname, reconnect_state->port);
  if (sockfd == -1) {
    perror("Failed to open socket: ");
    exit(1);
  }
  /* Reinitialize the client. */
  mqtt_reinit(client, sockfd,
    reconnect_state->sendbuf, reconnect_state->sendbufsz,
    reconnect_state->recvbuf, reconnect_state->recvbufsz);

  /* Send connection request to the broker. */
  mqtt_connect(client, "subscribing_client", NULL, NULL, 0, NULL, NULL, 0, 400);
  /* Subscribe to the topic. */
  mqtt_subscribe(client, reconnect_state->topic, 0);
}

static void publish_callback(void **unused, struct mqtt_response_publish *published) {
  /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
  char *topic_name = (char *)malloc(published->topic_name_size + 1);
  char *application_message = (char *)malloc(published->application_message_size + 1);

  memcpy(topic_name, published->topic_name, published->topic_name_size);
  topic_name[published->topic_name_size] = '\0';
  message_queue[queue_end].topic = topic_name;

  memcpy(application_message, published->application_message, published->application_message_size);
  application_message[published->application_message_size] = '\0';
  message_queue[queue_end].message = application_message;

  if (debug) {
    printf("callback received '%s' -> '%s'\n", message_queue[queue_end].topic, message_queue[queue_end].message);
  }
  queue_end++;
  queue_end %= QUEUE_LENGTH;
}

static void *client_refresher(void *client) {
  while (1) {
    mqtt_sync((struct mqtt_client *)client);
    usleep(100000U);
  }
  return NULL;
}

#if 0
static cell_t CTest0(cell_t Val);
static void CTest1(cell_t Val1, cell_t Val2);

/****************************************************************
** Step 1: Put your own special glue routines here
**     or link them in from another file or library.
****************************************************************/
static cell_t CTest0(cell_t Val) {
  MSG_NUM_D("CTest0x: Val = ", Val);
  return Val + 1;
}

static void CTest1(cell_t Val1, cell_t Val2) {

  MSG("CTest1x: Val1 = ");
  ffDot(Val1);
  MSG_NUM_D(", Val2 = ", Val2);
}

#endif

/****************************************************************
** Step 2: Create CustomFunctionTable.
**     Do not change the name of CustomFunctionTable!
**     It is used by the pForth kernel.
****************************************************************/

#ifdef PF_NO_GLOBAL_INIT
/******************
** If your loader does not support global initialization, then you
** must define PF_NO_GLOBAL_INIT and provide a function to fill
** the table. Some embedded system loaders require this!
** Do not change the name of LoadCustomFunctionTable()!
** It is called by the pForth kernel.
*/
#define NUM_CUSTOM_FUNCTIONS (6)
CFunc0 CustomFunctionTable[NUM_CUSTOM_FUNCTIONS];

Err LoadCustomFunctionTable(void) {
  CustomFunctionTable[0] = forth_sleep;
  CustomFunctionTable[1] = forth_usleep;
  CustomFunctionTable[2] = forth_mqtt_debug;
  CustomFunctionTable[3] = forth_mqtt_connect;
  CustomFunctionTable[4] = forth_mqtt_publish;
  CustomFunctionTable[5] = forth_mqtt_read;
  return 0;
}

#else
/******************
** If your loader supports global initialization (most do.) then just
** create the table like this.
*/
CFunc0 CustomFunctionTable[] = {
  (CFunc0)forth_sleep,
  (CFunc0)forth_usleep,
  (CFunc0)forth_mqtt_debug,
  (CFunc0)forth_mqtt_connect,
  (CFunc0)forth_mqtt_publish,
  (CFunc0)forth_mqtt_read,
};
#endif

/****************************************************************
** Step 3: Add custom functions to the dictionary.
**     Do not change the name of CompileCustomFunctions!
**     It is called by the pForth kernel.
****************************************************************/

#if (!defined(PF_NO_INIT)) && (!defined(PF_NO_SHELL))
Err CompileCustomFunctions(void) {
  Err err;
  int i = 0;
  /* Compile Forth words that call your custom functions.
** Make sure order of functions matches that in LoadCustomFunctionTable().
** Parameters are: Name in UPPER CASE, Function, Index, Mode, NumParams
*/
  err = CreateGlueToC("SLEEP", i++, C_RETURNS_VOID, 1);
  if (err < 0)
    return err;
  err = CreateGlueToC("USLEEP", i++, C_RETURNS_VOID, 1);
  if (err < 0)
    return err;
  err = CreateGlueToC("MQTT-DEBUG", i++, C_RETURNS_VOID, 1);
  if (err < 0)
    return err;
  err = CreateGlueToC("MQTT-CONNECT", i++, C_RETURNS_VALUE, 4);
  if (err < 0)
    return err;
  err = CreateGlueToC("MQTT-PUBLISH", i++, C_RETURNS_VALUE, 5);
  if (err < 0)
    return err;
  err = CreateGlueToC("MQTT-READ", i++, C_RETURNS_VALUE, 3);
  if (err < 0)
    return err;

  return 0;
}
#else
Err CompileCustomFunctions(void) { return 0; }
#endif

/****************************************************************
** Step 4: Recompile using compiler option PF_USER_CUSTOM
**         and link with your code.
**         Then rebuild the Forth using "pforth -i system.fth"
**         Test:   10 Ctest0 ( should print message then '11' )
****************************************************************/

#endif /* PF_USER_CUSTOM */

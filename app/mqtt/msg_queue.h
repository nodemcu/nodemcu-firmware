#ifndef _MSG_QUEUE_H
#define _MSG_QUEUE_H 1
#include "mqtt_msg.h"
#ifdef __cplusplus
extern "C" {
#endif

struct msg_queue_t;

typedef struct msg_queue_t {
  struct msg_queue_t *next;
  mqtt_message_t msg;
  uint16_t msg_id;
  int msg_type;
  int publish_qos;
} msg_queue_t;

msg_queue_t * msg_enqueue(msg_queue_t **head, mqtt_message_t *msg, uint16_t msg_id, int msg_type, int publish_qos);
void msg_destroy(msg_queue_t *node);
msg_queue_t * msg_dequeue(msg_queue_t **head);
msg_queue_t * msg_peek(msg_queue_t **head);
int msg_size(msg_queue_t **head);

#ifdef __cplusplus
}
#endif

#endif

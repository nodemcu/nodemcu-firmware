#include "c_string.h"
#include "c_stdlib.h"
#include "c_stdio.h"
#include "msg_queue.h"

msg_queue_t *msg_enqueue(msg_queue_t **head, mqtt_message_t *msg, uint16_t msg_id, int msg_type, int publish_qos){
  if(!head){
    return NULL;
  }
  if (!msg || !msg->data || msg->length == 0){
    NODE_DBG("empty message\n");
    return NULL;
  }
  msg_queue_t *node = (msg_queue_t *)c_zalloc(sizeof(msg_queue_t));
  if(!node){
    NODE_DBG("not enough memory\n");
    return NULL;
  }
  
  node->msg.data = (uint8_t *)c_zalloc(msg->length);
  if(!node->msg.data){
    NODE_DBG("not enough memory\n");
    c_free(node);
    return NULL;
  }
  c_memcpy(node->msg.data, msg->data, msg->length);
  node->msg.length = msg->length;
  node->next = NULL;
  node->msg_id = msg_id;
  node->msg_type = msg_type;
  node->publish_qos = publish_qos;

  msg_queue_t *tail = *head;
  if(tail){
    while(tail->next!=NULL) tail = tail->next;
    tail->next = node;
  } else {
    *head = node;
  }
  return node;
}

void msg_destroy(msg_queue_t *node){
  if(!node) return;
  if(node->msg.data){
    c_free(node->msg.data);
    node->msg.data = NULL;
  }
  c_free(node);
}

msg_queue_t * msg_dequeue(msg_queue_t **head){
  if(!head || !*head){
    return NULL;
  }
  msg_queue_t *node = *head;  // fetch head.
  *head = node->next; // update head.
  node->next = NULL;
  return node;
}

msg_queue_t * msg_peek(msg_queue_t **head){
  if(!head || !*head){
    return NULL;
  }
  return *head;  // fetch head.
}

int msg_size(msg_queue_t **head){
  if(!head || !*head){
    return 0;
  }
  int i = 1;
  msg_queue_t *tail = *head;
  if(tail){
    while(tail->next!=NULL){
      tail = tail->next;
      i++;
    }
  }
  return i;
}

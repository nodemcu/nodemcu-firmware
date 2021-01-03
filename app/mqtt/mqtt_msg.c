/*
* Copyright (c) 2014, Stephen Robinson
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <string.h>
#include "mqtt_msg.h"

#define MQTT_MAX_FIXED_HEADER_SIZE 3

enum mqtt_connect_flag
{
  MQTT_CONNECT_FLAG_USERNAME = 1 << 7,
  MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,
  MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,
  MQTT_CONNECT_FLAG_WILL = 1 << 2,
  MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1
};

struct __attribute((__packed__)) mqtt_connect_variable_header
{
  uint8_t lengthMsb;
  uint8_t lengthLsb;
  uint8_t magic[4];
  uint8_t version;
  uint8_t flags;
  uint8_t keepaliveMsb;
  uint8_t keepaliveLsb;
};

static int append_string(mqtt_message_buffer_t *msgb, const char* string, int len)
{
  if(msgb->message.length + len + 2 > msgb->buffer_length)
    return -1;

  msgb->buffer[msgb->message.length++] = len >> 8;
  msgb->buffer[msgb->message.length++] = len & 0xff;
  memcpy(msgb->buffer + msgb->message.length, string, len);
  msgb->message.length += len;

  return len + 2;
}

static uint16_t append_message_id(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  if(msgb->message.length + 2 > msgb->buffer_length)
    return 0;

  msgb->buffer[msgb->message.length++] = message_id >> 8;
  msgb->buffer[msgb->message.length++] = message_id & 0xff;

  return 1;
}

static int init_message(mqtt_message_buffer_t* msgb)
{
  msgb->message.length = MQTT_MAX_FIXED_HEADER_SIZE;
  return MQTT_MAX_FIXED_HEADER_SIZE;
}

static mqtt_message_t* fail_message(mqtt_message_buffer_t* msgb)
{
  msgb->message.data = msgb->buffer;
  msgb->message.length = 0;
  return &msgb->message;
}

static mqtt_message_t* fini_message(mqtt_message_buffer_t* msgb, int type, int dup, int qos, int retain)
{
  int remaining_length = msgb->message.length - MQTT_MAX_FIXED_HEADER_SIZE;

  if(remaining_length > 127)
  {
    msgb->buffer[0] = ((type & 0x0f) << 4) | ((dup & 1) << 3) | ((qos & 3) << 1) | (retain & 1);
    msgb->buffer[1] = 0x80 | (remaining_length % 128);
    msgb->buffer[2] = remaining_length / 128;
    msgb->message.length = remaining_length + 3;
    msgb->message.data = msgb->buffer;
  }
  else
  {
    msgb->buffer[1] = ((type & 0x0f) << 4) | ((dup & 1) << 3) | ((qos & 3) << 1) | (retain & 1);
    msgb->buffer[2] = remaining_length;
    msgb->message.length = remaining_length + 2;
    msgb->message.data = msgb->buffer + 1;
  }

  return &msgb->message;
}

void mqtt_msg_init(mqtt_message_buffer_t* msgb, uint8_t* buffer, uint16_t buffer_length)
{
  memset(msgb, 0, sizeof(msgb));
  msgb->buffer = buffer;
  msgb->buffer_length = buffer_length;
}

// Returns total length of message, or -1 if not enough bytes are available
int32_t mqtt_get_total_length(uint8_t* buffer, uint16_t buffer_length)
{
  int i;
  int totlen = 0;

  if(buffer_length == 1)
    return -1;

  for(i = 1; i < buffer_length; ++i)
  {
    totlen += (buffer[i] & 0x7f) << (7 * (i - 1));
    if((buffer[i] & 0x80) == 0)
    {
      ++i;
      break;
    }

    if(i == buffer_length)
      return -1;
  }

  totlen += i;

  return totlen;
}

const char* mqtt_get_publish_topic(uint8_t* buffer, uint16_t* buffer_length)
{
  int i;
  int totlen = 0;
  int topiclen;

  for(i = 1; i < *buffer_length; ++i)
  {
    totlen += (buffer[i] & 0x7f) << (7 * (i -1));
    if((buffer[i] & 0x80) == 0)
    {
      ++i;
      break;
    }
  }
  totlen += i;

  if(i + 2 > *buffer_length)
    return NULL;
  topiclen = buffer[i++] << 8;
  topiclen |= buffer[i++];

  if(i + topiclen > *buffer_length)
    return NULL;

  *buffer_length = topiclen;
  return (const char*)(buffer + i);
}

const char* mqtt_get_publish_data(uint8_t* buffer, uint16_t* buffer_length)
{
  int i;
  int totlen = 0;
  int topiclen;

  for(i = 1; i < *buffer_length; ++i)
  {
    totlen += (buffer[i] & 0x7f) << (7 * (i - 1));
    if((buffer[i] & 0x80) == 0)
    {
      ++i;
      break;
    }
  }
  totlen += i;

  if(i + 2 > *buffer_length)
    return NULL;
  topiclen = buffer[i++] << 8;
  topiclen |= buffer[i++];

  if(i + topiclen > *buffer_length){
	*buffer_length = 0;
    return NULL;
  }
  i += topiclen;

  if(mqtt_get_qos(buffer) > 0)
  {
    if(i + 2 > *buffer_length)
      return NULL;
    i += 2;
  }

  if(totlen < i)
    return NULL;

  if(totlen <= *buffer_length)
    *buffer_length = totlen - i;
  else
    *buffer_length = *buffer_length - i;
  return (const char*)(buffer + i);
}

uint16_t mqtt_get_id(uint8_t* buffer, uint16_t buffer_length)
{
  if(buffer_length < 1)
    return 0;

  switch(mqtt_get_type(buffer))
  {
    case MQTT_MSG_TYPE_PUBLISH:
    {
      int i;
      int topiclen;

      if(mqtt_get_qos(buffer) <= 0)
        return 0;

      for(i = 1; i < buffer_length; ++i)
      {
        if((buffer[i] & 0x80) == 0)
        {
          ++i;
          break;
        }
      }

      if(i + 2 > buffer_length)
        return 0;
      topiclen = buffer[i++] << 8;
      topiclen |= buffer[i++];

      if(i + topiclen > buffer_length)
        return 0;
      i += topiclen;

      if(i + 2 > buffer_length)
        return 0;

      return (buffer[i] << 8) | buffer[i + 1];
    }
    case MQTT_MSG_TYPE_PUBACK:
    case MQTT_MSG_TYPE_PUBREC:
    case MQTT_MSG_TYPE_PUBREL:
    case MQTT_MSG_TYPE_PUBCOMP:
    case MQTT_MSG_TYPE_SUBACK:
    case MQTT_MSG_TYPE_UNSUBACK:
    case MQTT_MSG_TYPE_SUBSCRIBE:
    {
      // This requires the remaining length to be encoded in 1 byte,
      // which it should be.
      if(buffer_length >= 4 && (buffer[1] & 0x80) == 0)
        return (buffer[2] << 8) | buffer[3];
      else
        return 0;
    }

    default:
      return 0;
  }
}

mqtt_message_t* mqtt_msg_connect(mqtt_message_buffer_t* msgb, mqtt_connect_info_t* info)
{
  struct mqtt_connect_variable_header* variable_header;

  init_message(msgb);

  if(msgb->message.length + sizeof(*variable_header) > msgb->buffer_length)
    return fail_message(msgb);
  variable_header = (void*)(msgb->buffer + msgb->message.length);
  msgb->message.length += sizeof(*variable_header);

  variable_header->lengthMsb = 0;
  variable_header->lengthLsb = 4;
  memcpy(variable_header->magic, "MQTT", 4);
  variable_header->version = 4;
  variable_header->flags = 0;
  variable_header->keepaliveMsb = info->keepalive >> 8;
  variable_header->keepaliveLsb = info->keepalive & 0xff;

  if(info->clean_session)
    variable_header->flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;

  if(info->client_id != NULL && info->client_id[0] != '\0')
  {
    if(append_string(msgb, info->client_id, strlen(info->client_id)) < 0)
      return fail_message(msgb);
  }
  else
    return fail_message(msgb);

  if(info->will_topic != NULL && info->will_topic[0] != '\0')
  {
    if(append_string(msgb, info->will_topic, strlen(info->will_topic)) < 0)
      return fail_message(msgb);

    if(append_string(msgb, info->will_message, strlen(info->will_message)) < 0)
      return fail_message(msgb);

    variable_header->flags |= MQTT_CONNECT_FLAG_WILL;
    if(info->will_retain)
      variable_header->flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
    variable_header->flags |= (info->will_qos & 3) << 3;
  }

  if(info->username != NULL && info->username[0] != '\0')
  {
    if(append_string(msgb, info->username, strlen(info->username)) < 0)
      return fail_message(msgb);

    variable_header->flags |= MQTT_CONNECT_FLAG_USERNAME;
  }

  if(info->password != NULL && info->password[0] != '\0')
  {
    if(append_string(msgb, info->password, strlen(info->password)) < 0)
      return fail_message(msgb);

    variable_header->flags |= MQTT_CONNECT_FLAG_PASSWORD;
  }

  return fini_message(msgb, MQTT_MSG_TYPE_CONNECT, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_publish(mqtt_message_buffer_t* msgb, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t message_id)
{
  init_message(msgb);

  if(topic == NULL || topic[0] == '\0')
    return fail_message(msgb);

  if(append_string(msgb, topic, strlen(topic)) < 0)
    return fail_message(msgb);

  if(qos > 0)
  {
    if(!append_message_id(msgb, message_id))
      return fail_message(msgb);
  }

  if(msgb->message.length + data_length > msgb->buffer_length)
    return fail_message(msgb);
  memcpy(msgb->buffer + msgb->message.length, data, data_length);
  msgb->message.length += data_length;

  return fini_message(msgb, MQTT_MSG_TYPE_PUBLISH, 0, qos, retain);
}

mqtt_message_t* mqtt_msg_puback(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  init_message(msgb);
  if(!append_message_id(msgb, message_id))
    return fail_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PUBACK, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_pubrec(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  init_message(msgb);
  if(!append_message_id(msgb, message_id))
    return fail_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PUBREC, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_pubrel(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  init_message(msgb);
  if(!append_message_id(msgb, message_id))
    return fail_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PUBREL, 0, 1, 0);
}

mqtt_message_t* mqtt_msg_pubcomp(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  init_message(msgb);
  if(!append_message_id(msgb, message_id))
    return fail_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PUBCOMP, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_subscribe_init(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  init_message(msgb);

  if(!append_message_id(msgb, message_id))
    return fail_message(msgb);

  return &msgb->message;
}

mqtt_message_t* mqtt_msg_subscribe_topic(mqtt_message_buffer_t* msgb, const char* topic, int qos)
{
  if(topic == NULL || topic[0] == '\0')
    return fail_message(msgb);

  if(append_string(msgb, topic, strlen(topic)) < 0)
    return fail_message(msgb);

  if(msgb->message.length + 1 > msgb->buffer_length)
    return fail_message(msgb);
  msgb->buffer[msgb->message.length++] = qos;

  return &msgb->message;
}

mqtt_message_t* mqtt_msg_subscribe_fini(mqtt_message_buffer_t* msgb)
{
  return fini_message(msgb, MQTT_MSG_TYPE_SUBSCRIBE, 0, 1, 0);
}

mqtt_message_t* mqtt_msg_subscribe(mqtt_message_buffer_t* msgb, const char* topic, int qos, uint16_t message_id)
{
  mqtt_message_t* result;

  result = mqtt_msg_subscribe_init(msgb, message_id);
  if (result->length != 0) {
    result = mqtt_msg_subscribe_topic(msgb, topic, qos);
  }
  if (result->length != 0) {
    result = mqtt_msg_subscribe_fini(msgb);
  }

  return result;
}

mqtt_message_t* mqtt_msg_unsubscribe_init(mqtt_message_buffer_t* msgb, uint16_t message_id)
{
  return mqtt_msg_subscribe_init(msgb, message_id);
}

mqtt_message_t* mqtt_msg_unsubscribe_topic(mqtt_message_buffer_t* msgb, const char* topic)
{
  if(topic == NULL || topic[0] == '\0')
    return fail_message(msgb);

  if(append_string(msgb, topic, strlen(topic)) < 0)
    return fail_message(msgb);

  return &msgb->message;
}

mqtt_message_t* mqtt_msg_unsubscribe_fini(mqtt_message_buffer_t* msgb)
{
  return fini_message(msgb, MQTT_MSG_TYPE_UNSUBSCRIBE, 0, 1, 0);
}

mqtt_message_t* mqtt_msg_unsubscribe(mqtt_message_buffer_t* msgb, const char* topic, uint16_t message_id)
{
  mqtt_message_t* result;

  result = mqtt_msg_unsubscribe_init(msgb, message_id);
  if (result->length != 0) {
    result = mqtt_msg_unsubscribe_topic(msgb, topic);
  }
  if (result->length != 0) {
    result = mqtt_msg_unsubscribe_fini(msgb);
  }

  return result;
}

mqtt_message_t* mqtt_msg_pingreq(mqtt_message_buffer_t* msgb)
{
  init_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PINGREQ, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_pingresp(mqtt_message_buffer_t* msgb)
{
  init_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_PINGRESP, 0, 0, 0);
}

mqtt_message_t* mqtt_msg_disconnect(mqtt_message_buffer_t* msgb)
{
  init_message(msgb);
  return fini_message(msgb, MQTT_MSG_TYPE_DISCONNECT, 0, 0, 0);
}

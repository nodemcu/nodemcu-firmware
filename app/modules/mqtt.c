// Module for mqtt

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "espconn.h"

#include "mqtt_msg.h"

static lua_State *gL = NULL;

#define MQTT_BUF_SIZE 1024
#define MQTT_DEFAULT_KEEPALIVE 60
#define MQTT_MAX_CLIENT_LEN   64
#define MQTT_MAX_USER_LEN     64
#define MQTT_MAX_PASS_LEN     64
#define MQTT_SEND_TIMEOUT			5

typedef enum {
  MQTT_INIT,
  MQTT_CONNECT_SEND,
  MQTT_CONNECT_SENDING,
  MQTT_DATA
} tConnState;

typedef struct mqtt_event_data_t
{
  uint8_t type;
  const char* topic;
  const char* data;
  uint16_t topic_length;
  uint16_t data_length;
  uint16_t data_offset;
} mqtt_event_data_t;

typedef struct mqtt_state_t
{
  uint16_t port;
  int auto_reconnect;
  mqtt_connect_info_t* connect_info;
  uint8_t* in_buffer;
  uint8_t* out_buffer;
  int in_buffer_length;
  int out_buffer_length;
  uint16_t message_length;
  uint16_t message_length_read;
  mqtt_message_t* outbound_message;
  mqtt_connection_t mqtt_connection;

  uint16_t pending_msg_id;
  int pending_msg_type;
  int pending_publish_qos;
} mqtt_state_t;

typedef struct lmqtt_userdata
{
  struct espconn *pesp_conn;
  int self_ref;
  int cb_connect_ref;
  int cb_disconnect_ref;
  int cb_message_ref;
  int cb_suback_ref;
  int cb_puback_ref;
  mqtt_state_t  mqtt_state;
  mqtt_connect_info_t connect_info;
  uint32_t keep_alive_tick;
  uint32_t send_timeout;
  uint8_t secure;
  uint8_t connected;
  ETSTimer mqttTimer;
  tConnState connState;
}lmqtt_userdata;

static void mqtt_socket_disconnected(void *arg)    // tcp only
{
  NODE_DBG("mqtt_socket_disconnected is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lmqtt_userdata *mud = (lmqtt_userdata *)pesp_conn->reverse;
  if(mud == NULL)
    return;
  if(mud->cb_disconnect_ref != LUA_NOREF && mud->self_ref != LUA_NOREF)
  {
    lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_disconnect_ref);
    lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata(client) to callback func in lua
    lua_call(gL, 1, 0);
  }
  mud->connected = 0;
  os_timer_disarm(&mud->mqttTimer);

  if(pesp_conn->proto.tcp)
    c_free(pesp_conn->proto.tcp);
  pesp_conn->proto.tcp = NULL;
  if(mud->pesp_conn)
    c_free(mud->pesp_conn);
  mud->pesp_conn = NULL;  // espconn is already disconnected
  lua_gc(gL, LUA_GCSTOP, 0);
  if(mud->self_ref != LUA_NOREF){
    luaL_unref(gL, LUA_REGISTRYINDEX, mud->self_ref);
    mud->self_ref = LUA_NOREF; // unref this, and the mqtt.socket userdata will delete it self
  }
  lua_gc(gL, LUA_GCRESTART, 0);
}

static void mqtt_socket_reconnected(void *arg, sint8_t err)
{
  NODE_DBG("mqtt_socket_reconnected is called.\n");
  mqtt_socket_disconnected(arg);
}

static void deliver_publish(lmqtt_userdata * mud, uint8_t* message, int length)
{
  const char comma[] = ",";
  mqtt_event_data_t event_data;

  event_data.topic_length = length;
  event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);

  event_data.data_length = length;
  event_data.data = mqtt_get_publish_data(message, &event_data.data_length);

  if(mud->cb_message_ref == LUA_NOREF)
    return;
  if(mud->self_ref == LUA_NOREF)
    return;
  lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_message_ref);
  lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata to callback func in lua
  // expose_array(gL, pdata, len);
  // *(pdata+len) = 0;
  // NODE_DBG(pdata);
  // NODE_DBG("\n");
  lua_pushlstring(gL, event_data.topic, event_data.topic_length);
  if(event_data.data_length > 0){
    lua_pushlstring(gL, event_data.data, event_data.data_length);
    lua_call(gL, 3, 0);
  } else {
    lua_call(gL, 2, 0);
  }
}

static void mqtt_socket_received(void *arg, char *pdata, unsigned short len)
{
  NODE_DBG("mqtt_socket_received is called.\n");

  uint8_t msg_type;
  uint8_t msg_qos;
  uint16_t msg_id;

  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lmqtt_userdata *mud = (lmqtt_userdata *)pesp_conn->reverse;
  if(mud == NULL)
    return;

READPACKET:
  if(len > MQTT_BUF_SIZE && len <= 0)
	  return;

  c_memcpy(mud->mqtt_state.in_buffer, pdata, len);
  mud->mqtt_state.outbound_message = NULL;
  switch(mud->connState){
    case MQTT_CONNECT_SENDING:
      if(mqtt_get_type(mud->mqtt_state.in_buffer) != MQTT_MSG_TYPE_CONNACK){
        NODE_DBG("MQTT: Invalid packet\r\n");
        mud->connState = MQTT_INIT;
        if(mud->secure){
          espconn_secure_disconnect(pesp_conn);
        }
        else {
          espconn_disconnect(pesp_conn);
        }
      } else {
        mud->connState = MQTT_DATA;
        NODE_DBG("MQTT: Connected\r\n");
        if(mud->cb_connect_ref == LUA_NOREF)
          return;
        if(mud->self_ref == LUA_NOREF)
          return;
        lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_connect_ref);
        lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata(client) to callback func in lua
        lua_call(gL, 1, 0);
        return;
      }
      break;

    case MQTT_DATA:
      mud->mqtt_state.message_length_read = len;
      mud->mqtt_state.message_length = mqtt_get_total_length(mud->mqtt_state.in_buffer, mud->mqtt_state.message_length_read);
      msg_type = mqtt_get_type(mud->mqtt_state.in_buffer);
      msg_qos = mqtt_get_qos(mud->mqtt_state.in_buffer);
      msg_id = mqtt_get_id(mud->mqtt_state.in_buffer, mud->mqtt_state.in_buffer_length);

      NODE_DBG("MQTT_DATA: type: %d, qos: %d, msg_id: %d, pending_id: %d\r\n",
            msg_type,
            msg_qos,
            msg_id,
            mud->mqtt_state.pending_msg_id);
      switch(msg_type)
      {
        case MQTT_MSG_TYPE_SUBACK:
          if(mud->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && mud->mqtt_state.pending_msg_id == msg_id)
            NODE_DBG("MQTT: Subscribe successful\r\n");
          if (mud->cb_suback_ref == LUA_NOREF)
            break;
          if (mud->self_ref == LUA_NOREF)
            break;
          lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_suback_ref);
          lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);
          lua_call(gL, 1, 0);
          break;
        case MQTT_MSG_TYPE_UNSUBACK:
          if(mud->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && mud->mqtt_state.pending_msg_id == msg_id)
            NODE_DBG("MQTT: UnSubscribe successful\r\n");
          break;
        case MQTT_MSG_TYPE_PUBLISH:
          if(msg_qos == 1)
            mud->mqtt_state.outbound_message = mqtt_msg_puback(&mud->mqtt_state.mqtt_connection, msg_id);
          else if(msg_qos == 2)
            mud->mqtt_state.outbound_message = mqtt_msg_pubrec(&mud->mqtt_state.mqtt_connection, msg_id);

          deliver_publish(mud, mud->mqtt_state.in_buffer, mud->mqtt_state.message_length_read);
          break;
        case MQTT_MSG_TYPE_PUBACK:
          if(mud->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && mud->mqtt_state.pending_msg_id == msg_id){
            NODE_DBG("MQTT: Publish with QoS = 1 successful\r\n");
            if(mud->cb_puback_ref == LUA_NOREF)
              break;
            if(mud->self_ref == LUA_NOREF)
              break;
            lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_puback_ref);
            lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata to callback func in lua
            lua_call(gL, 1, 0);
          }

          break;
        case MQTT_MSG_TYPE_PUBREC:
            mud->mqtt_state.outbound_message = mqtt_msg_pubrel(&mud->mqtt_state.mqtt_connection, msg_id);
            NODE_DBG("MQTT: Response PUBREL\r\n");
          break;
        case MQTT_MSG_TYPE_PUBREL:
            mud->mqtt_state.outbound_message = mqtt_msg_pubcomp(&mud->mqtt_state.mqtt_connection, msg_id);
            NODE_DBG("MQTT: Response PUBCOMP\r\n");
          break;
        case MQTT_MSG_TYPE_PUBCOMP:
          if(mud->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && mud->mqtt_state.pending_msg_id == msg_id){
            NODE_DBG("MQTT: Publish  with QoS = 2 successful\r\n");
            if(mud->cb_puback_ref == LUA_NOREF)
              break;
            if(mud->self_ref == LUA_NOREF)
              break;
            lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_puback_ref);
            lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata to callback func in lua
            lua_call(gL, 1, 0);
          }
          break;
        case MQTT_MSG_TYPE_PINGREQ:
            mud->mqtt_state.outbound_message = mqtt_msg_pingresp(&mud->mqtt_state.mqtt_connection);
          break;
        case MQTT_MSG_TYPE_PINGRESP:
          // Ignore
          break;
      }
      // NOTE: this is done down here and not in the switch case above
      // because the PSOCK_READBUF_LEN() won't work inside a switch
      // statement due to the way protothreads resume.
      if(msg_type == MQTT_MSG_TYPE_PUBLISH)
      {

        len = mud->mqtt_state.message_length_read;

        if(mud->mqtt_state.message_length < mud->mqtt_state.message_length_read)
				{
					len -= mud->mqtt_state.message_length;
					pdata += mud->mqtt_state.message_length;

					NODE_DBG("Get another published message\r\n");
					goto READPACKET;
				}
      }
      break;
  }

  if(mud->mqtt_state.outbound_message != NULL){
    if(mud->secure)
      espconn_secure_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
    else
      espconn_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
    mud->mqtt_state.outbound_message = NULL;
  }
  return;
}

static void mqtt_socket_sent(void *arg)
{
  // NODE_DBG("mqtt_socket_sent is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lmqtt_userdata *mud = (lmqtt_userdata *)pesp_conn->reverse;
  if(mud == NULL)
    return;
  if(!mud->connected)
  	return;
  // call mqtt_sent()
  mud->send_timeout = 0;
  if(mud->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && mud->mqtt_state.pending_publish_qos == 0) {
  	if(mud->cb_puback_ref == LUA_NOREF)
			return;
		if(mud->self_ref == LUA_NOREF)
			return;
		lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->cb_puback_ref);
		lua_rawgeti(gL, LUA_REGISTRYINDEX, mud->self_ref);  // pass the userdata to callback func in lua
		lua_call(gL, 1, 0);
  }
}

static int mqtt_socket_client( lua_State* L );
static void mqtt_socket_connected(void *arg)
{
  NODE_DBG("mqtt_socket_connected is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL)
    return;
  lmqtt_userdata *mud = (lmqtt_userdata *)pesp_conn->reverse;
  if(mud == NULL)
    return;
  mud->connected = true;
  espconn_regist_recvcb(pesp_conn, mqtt_socket_received);
  espconn_regist_sentcb(pesp_conn, mqtt_socket_sent);
  espconn_regist_disconcb(pesp_conn, mqtt_socket_disconnected);

  // call mqtt_connect() to start a mqtt connect stage.
  mqtt_msg_init(&mud->mqtt_state.mqtt_connection, mud->mqtt_state.out_buffer, mud->mqtt_state.out_buffer_length);
  mud->mqtt_state.outbound_message = mqtt_msg_connect(&mud->mqtt_state.mqtt_connection, mud->mqtt_state.connect_info);
  NODE_DBG("Send MQTT connection infomation, data len: %d, d[0]=%d \r\n", mud->mqtt_state.outbound_message->length,  mud->mqtt_state.outbound_message->data[0]);
  if(mud->secure){
    espconn_secure_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
  }
  else
  {
    espconn_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
  }
  mud->mqtt_state.outbound_message = NULL;
  mud->connState = MQTT_CONNECT_SENDING;
  return;
}

void mqtt_socket_timer(void *arg)
{
  lmqtt_userdata *mud = (lmqtt_userdata*) arg;

  if(mud->connState == MQTT_DATA){
    mud->keep_alive_tick ++;
    if(mud->keep_alive_tick > mud->mqtt_state.connect_info->keepalive){
      mud->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PINGREQ;
      mud->send_timeout = MQTT_SEND_TIMEOUT;
      NODE_DBG("\r\nMQTT: Send keepalive packet\r\n");
      mud->mqtt_state.outbound_message = mqtt_msg_pingreq(&mud->mqtt_state.mqtt_connection);

      if(mud->secure)
        espconn_secure_sent(mud->pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
      else
        espconn_sent(mud->pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
      mud->keep_alive_tick = 0;
    }
  }
  if(mud->send_timeout > 0)
  	mud->send_timeout --;
}

// Lua: mqtt.Client(clientid, keepalive, user, pass)
static int mqtt_socket_client( lua_State* L )
{
  NODE_DBG("mqtt_socket_client is called.\n");

  lmqtt_userdata *mud;
  char tempid[20] = {0};
  c_sprintf(tempid, "%s%x", "NodeMCU_", system_get_chip_id() );
  NODE_DBG(tempid);
  NODE_DBG("\n");
  size_t il = c_strlen(tempid);
  const char *clientId = tempid, *username = NULL, *password = NULL;
  int stack = 1;
  unsigned secure = 0;
  int top = lua_gettop(L);

  // create a object
  mud = (lmqtt_userdata *)lua_newuserdata(L, sizeof(lmqtt_userdata));
  // pre-initialize it, in case of errors
  mud->self_ref = LUA_NOREF;
  mud->cb_connect_ref = LUA_NOREF;
  mud->cb_disconnect_ref = LUA_NOREF;

  mud->cb_message_ref = LUA_NOREF;
  mud->cb_suback_ref = LUA_NOREF;
  mud->cb_puback_ref = LUA_NOREF;
  mud->pesp_conn = NULL;
  mud->secure = 0;

  mud->keep_alive_tick = 0;
  mud->send_timeout = 0;
  mud->connState = MQTT_INIT;
  c_memset(&mud->mqttTimer, 0, sizeof(ETSTimer));
  c_memset(&mud->mqtt_state, 0, sizeof(mqtt_state_t));
  c_memset(&mud->connect_info, 0, sizeof(mqtt_connect_info_t));

  // set its metatable
  luaL_getmetatable(L, "mqtt.socket");
  lua_setmetatable(L, -2);

  if( lua_isstring(L,stack) )   // deal with the clientid string
  {
    clientId = luaL_checklstring( L, stack, &il );
    stack++;
  }

  // TODO: check the zalloc result.
  mud->connect_info.client_id = (uint8_t *)c_zalloc(il+1);
  if(!mud->connect_info.client_id){
  	return luaL_error(L, "not enough memory");
  }
  c_memcpy(mud->connect_info.client_id, clientId, il);
  mud->connect_info.client_id[il] = 0;

  mud->mqtt_state.in_buffer = (uint8_t *)c_zalloc(MQTT_BUF_SIZE);
  if(!mud->mqtt_state.in_buffer){
		return luaL_error(L, "not enough memory");
	}

  mud->mqtt_state.out_buffer = (uint8_t *)c_zalloc(MQTT_BUF_SIZE);
  if(!mud->mqtt_state.out_buffer){
		return luaL_error(L, "not enough memory");
	}

  mud->mqtt_state.in_buffer_length = MQTT_BUF_SIZE;
  mud->mqtt_state.out_buffer_length = MQTT_BUF_SIZE;

  mud->connState = MQTT_INIT;
  mud->connect_info.clean_session = 1;
  mud->connect_info.will_qos = 0;
  mud->connect_info.will_retain = 0;
  mud->keep_alive_tick = 0;
  mud->connect_info.keepalive = 0;
  mud->mqtt_state.connect_info = &mud->connect_info;

  gL = L;   // global L for mqtt module.

  if(lua_isnumber( L, stack ))
  {   
    mud->connect_info.keepalive = luaL_checkinteger( L, stack);
    stack++;
  }

  if(mud->connect_info.keepalive == 0){
    mud->connect_info.keepalive = MQTT_DEFAULT_KEEPALIVE;
    return 1;
  }

  if(lua_isstring( L, stack )){
    username = luaL_checklstring( L, stack, &il );
    stack++;
  }
  if(username == NULL)
	  il = 0;
  NODE_DBG("lengh username: %d\r\n", il);
  mud->connect_info.username = (uint8_t *)c_zalloc(il + 1);
  if(!mud->connect_info.username){
		return luaL_error(L, "not enough memory");
	}

  c_memcpy(mud->connect_info.username, username, il);
  mud->connect_info.username[il] = 0;
    
  if(lua_isstring( L, stack )){
    password = luaL_checklstring( L, stack, &il );
    stack++;
  }
  if(password == NULL)
  	  il = 0;
  NODE_DBG("lengh password: %d\r\n", il);

  mud->connect_info.password = (uint8_t *)c_zalloc(il + 1);
  if(!mud->connect_info.password){
		return luaL_error(L, "not enough memory");
	}

  c_memcpy(mud->connect_info.password, password, il);
  mud->connect_info.password[il] = 0;
  
  NODE_DBG("MQTT: Init info: %s, %s, %s\r\n", mud->connect_info.client_id, mud->connect_info.username, mud->connect_info.password);

  return 1;
}

// Lua: mqtt.delete( socket )
// call close() first
// socket: unref everything
static int mqtt_delete( lua_State* L )
{
  NODE_DBG("mqtt_delete is called.\n");

  lmqtt_userdata *mud = (lmqtt_userdata *)luaL_checkudata(L, 1, "mqtt.socket");
  luaL_argcheck(L, mud, 1, "mqtt.socket expected");
  if(mud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  os_timer_disarm(&mud->mqttTimer);
  mud->connected = 0;
  if(mud->pesp_conn){     // for client connected to tcp server, this should set NULL in disconnect cb
    mud->pesp_conn->reverse = NULL;
    if(mud->pesp_conn->proto.tcp)
      c_free(mud->pesp_conn->proto.tcp);
    mud->pesp_conn->proto.tcp = NULL;
    c_free(mud->pesp_conn);
    mud->pesp_conn = NULL;    // for socket, it will free this when disconnected
  }

  if(mud->connect_info.will_topic){
  	c_free(mud->connect_info.will_topic);
  	mud->connect_info.will_topic = NULL;
  }

  if(mud->connect_info.will_message){
    	c_free(mud->connect_info.will_message);
    	mud->connect_info.will_message = NULL;
    }

  if(mud->connect_info.client_id){
    c_free(mud->connect_info.client_id);
    mud->connect_info.client_id = NULL;
  }
  if(mud->connect_info.username){
    c_free(mud->connect_info.username);
    mud->connect_info.username = NULL;
  }
  if(mud->connect_info.password){
    c_free(mud->connect_info.password);
    mud->connect_info.password = NULL;
  }
  if(mud->mqtt_state.in_buffer){
    c_free(mud->mqtt_state.in_buffer);
    mud->mqtt_state.in_buffer = NULL;
  }
  if(mud->mqtt_state.out_buffer){
    c_free(mud->mqtt_state.out_buffer);
    mud->mqtt_state.out_buffer = NULL;
  }

  // free (unref) callback ref
  if(LUA_NOREF!=mud->cb_connect_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_connect_ref);
    mud->cb_connect_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=mud->cb_disconnect_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_disconnect_ref);
    mud->cb_disconnect_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=mud->cb_message_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_message_ref);
    mud->cb_message_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=mud->cb_suback_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_suback_ref);
    mud->cb_suback_ref = LUA_NOREF;
  }
  if(LUA_NOREF!=mud->cb_puback_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_puback_ref);
    mud->cb_puback_ref = LUA_NOREF;
  }
  lua_gc(gL, LUA_GCSTOP, 0);
  if(LUA_NOREF!=mud->self_ref){
    luaL_unref(L, LUA_REGISTRYINDEX, mud->self_ref);
    mud->self_ref = LUA_NOREF;
  }
  lua_gc(gL, LUA_GCRESTART, 0);
  return 0;  
}

static void socket_connect(struct espconn *pesp_conn)
{
  if(pesp_conn == NULL)
    return;
  lmqtt_userdata *mud = (lmqtt_userdata *)pesp_conn->reverse;
  if(mud == NULL)
    return;

  if(mud->secure){
    espconn_secure_connect(pesp_conn);
  }
  else
  {
    espconn_connect(pesp_conn);
  }
  
  NODE_DBG("socket_connect is called.\n");
}

static void socket_dns_found(const char *name, ip_addr_t *ipaddr, void *arg);
static dns_reconn_count = 0;
static ip_addr_t host_ip; // for dns
static void socket_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  NODE_DBG("socket_dns_found is called.\n");
  struct espconn *pesp_conn = arg;
  if(pesp_conn == NULL){
    NODE_DBG("pesp_conn null.\n");
    return;
  }

  if(ipaddr == NULL)
  {
    dns_reconn_count++;
    if( dns_reconn_count >= 5 ){
      NODE_ERR( "DNS Fail!\n" );
      // Note: should delete the pesp_conn or unref self_ref here.
      mqtt_socket_disconnected(arg);   // although not connected, but fire disconnect callback to release every thing.
      return;
    }
    NODE_ERR( "DNS retry %d!\n", dns_reconn_count );
    host_ip.addr = 0;
    espconn_gethostbyname(pesp_conn, name, &host_ip, socket_dns_found);
    return;
  }

  // ipaddr->addr is a uint32_t ip
  if(ipaddr->addr != 0)
  {
    dns_reconn_count = 0;
    c_memcpy(pesp_conn->proto.tcp->remote_ip, &(ipaddr->addr), 4);
    NODE_DBG("TCP ip is set: ");
    NODE_DBG(IPSTR, IP2STR(&(ipaddr->addr)));
    NODE_DBG("\n");
    socket_connect(pesp_conn);
  }
}

// Lua: mqtt:connect( host, port, secure, function(client) )
static int mqtt_socket_lwt( lua_State* L )
{
	uint8_t stack = 1;
	size_t topicSize, il;
  NODE_DBG("mqtt_socket_lwt.\n");
  lmqtt_userdata *mud = NULL;
  const char *lwtTopic, *lwtMsg;
  uint8_t lwtQoS, lwtRetain;

  mud = (lmqtt_userdata *)luaL_checkudata( L, stack, "mqtt.socket" );
  luaL_argcheck( L, mud, stack, "mqtt.socket expected" );

  if(mud == NULL)
    return 0;

  stack++;
  lwtTopic = luaL_checklstring( L, stack, &topicSize );
	if (lwtTopic == NULL)
	{
		return luaL_error( L, "need lwt topic");
	}

	stack++;
	lwtMsg = luaL_checklstring( L, stack, &il );
	if (lwtMsg == NULL)
	{
		return luaL_error( L, "need lwt message");
	}

	mud->connect_info.will_topic = (uint8_t*) c_zalloc( topicSize + 1 );
	if(!mud->connect_info.will_topic){
		return luaL_error( L, "not enough memory");
	}
	c_memcpy(mud->connect_info.will_topic, lwtTopic, topicSize);
	mud->connect_info.will_topic[topicSize] = 0;

	mud->connect_info.will_message = (uint8_t*) c_zalloc( il + 1 );
	if(!mud->connect_info.will_message){
		return luaL_error( L, "not enough memory");
	}
	c_memcpy(mud->connect_info.will_message, lwtMsg, il);
	mud->connect_info.will_message[il] = 0;


	stack++;
	mud->connect_info.will_qos = luaL_checkinteger( L, stack );

	stack++;
	mud->connect_info.will_retain = luaL_checkinteger( L, stack );

	NODE_DBG("mqtt_socket_lwt: topic: %s, message: %s, qos: %d, retain: %d\n",
			mud->connect_info.will_topic,
			mud->connect_info.will_message,
			mud->connect_info.will_qos,
			mud->connect_info.will_retain);
  return 0;
}

// Lua: mqtt:connect( host, port, secure, function(client) )
static int mqtt_socket_connect( lua_State* L )
{
  NODE_DBG("mqtt_socket_connect is called.\n");
  lmqtt_userdata *mud = NULL;
  unsigned port = 1883;
  size_t il;
  ip_addr_t ipaddr;
  const char *domain;
  int stack = 1;
  unsigned secure = 0;
  int top = lua_gettop(L);

  mud = (lmqtt_userdata *)luaL_checkudata(L, stack, "mqtt.socket");
  luaL_argcheck(L, mud, stack, "mqtt.socket expected");
  stack++;
  if(mud == NULL)
    return 0;

  if(mud->pesp_conn){   //TODO: should I free tcp struct directly or ask user to call close()???
    mud->pesp_conn->reverse = NULL;
    if(mud->pesp_conn->proto.tcp)
      c_free(mud->pesp_conn->proto.tcp);
    mud->pesp_conn->proto.tcp = NULL;
    c_free(mud->pesp_conn);
    mud->pesp_conn = NULL;
  }

  struct espconn *pesp_conn = NULL;
	pesp_conn = mud->pesp_conn = (struct espconn *)c_zalloc(sizeof(struct espconn));
	if(!pesp_conn)
		return luaL_error(L, "not enough memory");

	pesp_conn->proto.udp = NULL;
	pesp_conn->proto.tcp = (esp_tcp *)c_zalloc(sizeof(esp_tcp));
	if(!pesp_conn->proto.tcp){
		c_free(pesp_conn);
		pesp_conn = mud->pesp_conn = NULL;
		return luaL_error(L, "not enough memory");
	}
	// reverse is for the callback function
	pesp_conn->reverse = mud;
	pesp_conn->type = ESPCONN_TCP;
	pesp_conn->state = ESPCONN_NONE;
  mud->connected = 0;

  if( (stack<=top) && lua_isstring(L,stack) )   // deal with the domain string
  {
    domain = luaL_checklstring( L, stack, &il );

    stack++;
    if (domain == NULL)
    {
      domain = "127.0.0.1";
    }
    ipaddr.addr = ipaddr_addr(domain);
    c_memcpy(pesp_conn->proto.tcp->remote_ip, &ipaddr.addr, 4);
    NODE_DBG("TCP ip is set: ");
    NODE_DBG(IPSTR, IP2STR(&ipaddr.addr));
    NODE_DBG("\n");
  }

  if ( (stack<=top) && lua_isnumber(L, stack) )
  {
    port = lua_tointeger(L, stack);
    stack++;
    NODE_DBG("TCP port is set: %d.\n", port);
  }
  pesp_conn->proto.tcp->remote_port = port;
  pesp_conn->proto.tcp->local_port = espconn_port();

  if ( (stack<=top) && lua_isnumber(L, stack) )
  {
    secure = lua_tointeger(L, stack);
    stack++;
    if ( secure != 0 && secure != 1 ){
      secure = 0; // default to 0
    }
  } else {
    secure = 0; // default to 0
  }
  mud->secure = secure; // save

  // call back function when a connection is obtained, tcp only
  if ((stack<=top) && (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)){
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    if(mud->cb_connect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_connect_ref);
    mud->cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    stack++;
  }

  lua_pushvalue(L, 1);  // copy userdata to the top of stack
  if(mud->self_ref != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, mud->self_ref);
  mud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  espconn_regist_connectcb(pesp_conn, mqtt_socket_connected);
  espconn_regist_reconcb(pesp_conn, mqtt_socket_reconnected);

  if((ipaddr.addr == IPADDR_NONE) && (c_memcmp(domain,"255.255.255.255",16) != 0))
  {
    host_ip.addr = 0;
    dns_reconn_count = 0;
    if(ESPCONN_OK == espconn_gethostbyname(pesp_conn, domain, &host_ip, socket_dns_found)){
      socket_dns_found(domain, &host_ip, pesp_conn);  // ip is returned in host_ip.
    }
  }
  else
  {
    socket_connect(pesp_conn);
  }

  os_timer_disarm(&mud->mqttTimer);
	os_timer_setfn(&mud->mqttTimer, (os_timer_func_t *)mqtt_socket_timer, mud);
	os_timer_arm(&mud->mqttTimer, 1000, 1);

  return 0;
}

// Lua: mqtt:close()
// client disconnect and unref itself
static int mqtt_socket_close( lua_State* L )
{
  NODE_DBG("mqtt_socket_close is called.\n");
  int i = 0;
  lmqtt_userdata *mud = NULL;

  mud = (lmqtt_userdata *)luaL_checkudata(L, 1, "mqtt.socket");
  luaL_argcheck(L, mud, 1, "mqtt.socket expected");
  if(mud == NULL)
    return 0;

  if(mud->pesp_conn == NULL)
    return 0;

  // call mqtt_disconnect()

  if(mud->secure){
    if(mud->pesp_conn->proto.tcp->remote_port || mud->pesp_conn->proto.tcp->local_port)
      espconn_secure_disconnect(mud->pesp_conn);
  }
  else
  {
    if(mud->pesp_conn->proto.tcp->remote_port || mud->pesp_conn->proto.tcp->local_port)
      espconn_disconnect(mud->pesp_conn);
  }
  return 0;  
}

// Lua: mqtt:on( "method", function() )
static int mqtt_socket_on( lua_State* L )
{
  NODE_DBG("mqtt_on is called.\n");
  lmqtt_userdata *mud;
  size_t sl;

  mud = (lmqtt_userdata *)luaL_checkudata(L, 1, "mqtt.socket");
  luaL_argcheck(L, mud, 1, "mqtt.socket expected");
  if(mud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  const char *method = luaL_checklstring( L, 2, &sl );
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );

  luaL_checkanyfunction(L, 3);
  lua_pushvalue(L, 3);  // copy argument (func) to the top of stack

  if( sl == 7 && c_strcmp(method, "connect") == 0){
    if(mud->cb_connect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_connect_ref);
    mud->cb_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if( sl == 7 && c_strcmp(method, "offline") == 0){
    if(mud->cb_disconnect_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_disconnect_ref);
    mud->cb_disconnect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else if( sl == 7 && c_strcmp(method, "message") == 0){
    if(mud->cb_message_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_message_ref);
    mud->cb_message_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }else{
    lua_pop(L, 1);
    return luaL_error( L, "method not supported" );
  }

  return 0;
}

// Lua: mqtt:subscribe(topic, qos, function())
static int mqtt_socket_subscribe( lua_State* L ) {
	NODE_DBG("mqtt_socket_subscribe is called.\n");
	typedef struct SUB_STORAGE {
		uint32_t length;
		uint8_t *data;
		struct SUB_STORAGE *next;
	} SUB_STORAGE;

	uint8_t stack = 1, qos = 0;
	const char *topic;
	size_t il;
	lmqtt_userdata *mud;

	mud = (lmqtt_userdata *) luaL_checkudata( L, stack, "mqtt.socket" );
	luaL_argcheck( L, mud, stack, "mqtt.socket expected" );
	stack++;

	if( mud->send_timeout != 0 )
		return luaL_error( L, "sending in process" );

	if( !mud->connected )
		return luaL_error( L, "not connected" );

	if( lua_istable( L, stack ) ) {
		NODE_DBG("subscribe table\n");
		lua_pushnil( L ); /* first key */
		SUB_STORAGE *first, *last, *curr;
		first = (SUB_STORAGE*) c_zalloc(sizeof(SUB_STORAGE));
		if( first == NULL )
			return luaL_error( L, "not enough memory" );
		first->length = 0;
		last = first;
		first->next = NULL;
		while( lua_next( L, stack ) != 0 ) {
			curr = (SUB_STORAGE*) c_zalloc(sizeof(SUB_STORAGE));

			if( curr == NULL )
				return luaL_error( L, "not enough memory" );
			topic = luaL_checkstring( L, -2 );
			qos = luaL_checkinteger( L, -1 );

			mud->mqtt_state.outbound_message = mqtt_msg_subscribe( &mud->mqtt_state.mqtt_connection, topic, qos, &mud->mqtt_state.pending_msg_id );
			NODE_DBG("topic: %s - qos: %d, length: %d\n", topic, qos, mud->mqtt_state.outbound_message->length);
			curr->data = (uint8_t*) c_zalloc(mud->mqtt_state.outbound_message->length);
			if( curr->data == NULL )
				return luaL_error( L, "not enough memory" );
			c_memcpy( curr->data, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length );

			curr->length = mud->mqtt_state.outbound_message->length;
			curr->next = NULL;
			last->next = curr;
			last = curr;
			lua_pop( L, 1 );
		}

		curr = first;
		uint32_t ptr = 0;
		while( curr != NULL ) {
			if( curr->length == 0 ) {
				curr = curr->next;
				continue;
			}
			if( ptr + curr->length < mud->mqtt_state.out_buffer_length ) {
				c_memcpy( mud->mqtt_state.out_buffer + ptr, curr->data, curr->length );
				ptr += curr->length;
			}
			c_free(curr->data);
			c_free(curr);
			curr = curr->next;
		}
		c_free(first);
		if( ptr == 0 ) {
			return luaL_error( L, "invalid data" );
		}
		mud->mqtt_state.outbound_message->data = mud->mqtt_state.out_buffer;
		mud->mqtt_state.outbound_message->length = ptr;
		stack++;
	} else {
		NODE_DBG("subscribe string\n");
		topic = luaL_checklstring( L, stack, &il );
		stack++;
		if( topic == NULL )
			return luaL_error( L, "need topic name" );
		qos = luaL_checkinteger( L, stack );
		mud->mqtt_state.outbound_message = mqtt_msg_subscribe( &mud->mqtt_state.mqtt_connection, topic, qos, &mud->mqtt_state.pending_msg_id );
		stack++;
	}

	mud->send_timeout = MQTT_SEND_TIMEOUT;
	mud->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_SUBSCRIBE;
	mud->mqtt_state.pending_publish_qos = mqtt_get_qos( mud->mqtt_state.outbound_message->data );

	if( lua_type( L, stack ) == LUA_TFUNCTION || lua_type( L, stack ) == LUA_TLIGHTFUNCTION ) {
		lua_pushvalue( L, stack );  // copy argument (func) to the top of stack
		if( mud->cb_suback_ref != LUA_NOREF )
			luaL_unref( L, LUA_REGISTRYINDEX, mud->cb_suback_ref );
		mud->cb_suback_ref = luaL_ref( L, LUA_REGISTRYINDEX );
	}
	NODE_DBG("Sent: %d\n", mud->mqtt_state.outbound_message->length);
	if( mud->secure )
		espconn_secure_sent( mud->pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length );
	else
		espconn_sent( mud->pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length );

	return 0;
}

// Lua: mqtt:publish( topic, payload, qos, retain, function() )
static int mqtt_socket_publish( lua_State* L )
{
  // NODE_DBG("mqtt_publish is called.\n");
  struct espconn *pesp_conn = NULL;
  lmqtt_userdata *mud;
  size_t l;
  uint8_t stack = 1;
  mud = (lmqtt_userdata *)luaL_checkudata(L, stack, "mqtt.socket");
  luaL_argcheck(L, mud, stack, "mqtt.socket expected");
  stack++;
  if(mud==NULL){
    NODE_DBG("userdata is nil.\n");
    return 0;
  }

  if(mud->pesp_conn == NULL){
    NODE_DBG("mud->pesp_conn is NULL.\n");
    return 0;
  }
  if(mud->send_timeout != 0)
  	return luaL_error( L, "sending in process" );
  pesp_conn = mud->pesp_conn;

#if 0
  char temp[20] = {0};
  c_sprintf(temp, IPSTR, IP2STR( &(pesp_conn->proto.tcp->remote_ip) ) );
  NODE_DBG("remote ");
  NODE_DBG(temp);
  NODE_DBG(":");
  NODE_DBG("%d",pesp_conn->proto.tcp->remote_port);
  NODE_DBG(" sending data.\n");
#endif
  const char *topic = luaL_checklstring( L, stack, &l );
  stack ++;
  if (topic == NULL)
    return luaL_error( L, "need topic" );

  const char *payload = luaL_checklstring( L, stack, &l );
  stack ++;
  uint8_t qos = luaL_checkinteger( L, stack);
  stack ++;
  uint8_t retain = luaL_checkinteger( L, stack);
  stack ++;


  mud->mqtt_state.outbound_message = mqtt_msg_publish(&mud->mqtt_state.mqtt_connection,
                       topic, payload, l,
                       qos, retain,
                       &mud->mqtt_state.pending_msg_id);
  mud->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PUBLISH;
  mud->mqtt_state.pending_publish_qos = qos;
  mud->send_timeout = MQTT_SEND_TIMEOUT;
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    if(mud->cb_puback_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, mud->cb_puback_ref);
    mud->cb_puback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if(mud->secure)
    espconn_secure_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
  else
    espconn_sent(pesp_conn, mud->mqtt_state.outbound_message->data, mud->mqtt_state.outbound_message->length);
  mud->mqtt_state.outbound_message = NULL;
  return 0;
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE mqtt_socket_map[] =
{
	{ LSTRKEY( "lwt" ), LFUNCVAL ( mqtt_socket_lwt ) },
  { LSTRKEY( "connect" ), LFUNCVAL ( mqtt_socket_connect ) },
  { LSTRKEY( "close" ), LFUNCVAL ( mqtt_socket_close ) },
  { LSTRKEY( "publish" ), LFUNCVAL ( mqtt_socket_publish ) },
  { LSTRKEY( "subscribe" ), LFUNCVAL ( mqtt_socket_subscribe ) },
  { LSTRKEY( "on" ), LFUNCVAL ( mqtt_socket_on ) },
  { LSTRKEY( "__gc" ), LFUNCVAL ( mqtt_delete ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "__index" ), LROVAL ( mqtt_socket_map ) },
#endif
  { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE mqtt_map[] = 
{
  { LSTRKEY( "Client" ), LFUNCVAL ( mqtt_socket_client ) },
#if LUA_OPTIMIZE_MEMORY > 0

  { LSTRKEY( "__metatable" ), LROVAL( mqtt_map ) },
#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_mqtt( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  luaL_rometatable(L, "mqtt.socket", (void *)mqtt_socket_map);  // create metatable for mqtt.socket
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  int n;
  luaL_register( L, AUXLIB_MQTT, mqtt_map );

  // Set it as its own metatable
  lua_pushvalue( L, -1 );
  lua_setmetatable( L, -2 );

  // Module constants  
  // MOD_REG_NUMBER( L, "TCP", TCP );

  // create metatable
  luaL_newmetatable(L, "mqtt.socket");
  // metatable.__index = metatable
  lua_pushliteral(L, "__index");
  lua_pushvalue(L,-2);
  lua_rawset(L,-3);
  // Setup the methods inside metatable
  luaL_register( L, NULL, mqtt_socket_map );

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}

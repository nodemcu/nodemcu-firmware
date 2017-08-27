// Module for coapwork

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "espconn.h"
#include "driver/uart.h"

#include "coap.h"
#include "uri.h"
#include "node.h"
#include "coap_timer.h"
#include "coap_io.h"
#include "coap_peer.h"


static int lua_coap_peer_regist(lua_State* L){
    coap_peer_t *pkt = (coap_peer_t *)luaL_checkudata(L,1,"coap_peer");
    if (lua_isfunction(L, 2) || lua_islightfunction(L, 2)) {
        pkt->server_ref = luaL_ref ( L,LUA_REGISTRYINDEX );
    }
    return 0;
}

static int lua_coap_peer_sender(lua_State* L){
    coap_peer_t *pkt = (coap_peer_t *)luaL_checkudata(L,1,"coap_peer");
    if (lua_isfunction(L, 2) || lua_islightfunction(L, 2)) {
        pkt->sender_ref = luaL_ref ( L,LUA_REGISTRYINDEX );
    }
    return 0;
}

static int lua_coap_peer_recv(lua_State* L){
    // 收到请求
    coap_peer_t *peer = (coap_peer_t *) luaL_checkudata(L,1,"coap_peer");
    coap_pdu_t *pdu = coap_new_pdu();
    int len,rc;
    const char *data = luaL_checklstring( L, 2 ,&len);
    const char *ip = luaL_checkstring( L, 3 );
    pdu->port = luaL_checkinteger( L, 4 );
    
    c_memcpy(pdu->ip,ip,16);
    
    //coap_packet_t pkt;
    NODE_DBG("receive package from %s:%d\n",ip,pdu->port);
    if (0 != (rc = coap_parse(pdu->pkt, data, len))){
        NODE_DBG("Bad packet rc=%d\n", rc);
        return 0;
    }
    
#ifdef COAP_DEBUG
    coap_dumpPacket(pdu->pkt);
#endif

    if(pdu->pkt->hdr.code < 0x20){
        // receive request
        NODE_DBG("receive request\n");
        lua_rawgeti (L,LUA_REGISTRYINDEX,peer->server_ref);
        lua_pushlightuserdata (L,(void*)pdu->pkt);
        luaL_getmetatable(L, "coap_package");
        lua_setmetatable(L, -2);
        lua_pushstring(L,pdu->ip);
        lua_pushinteger(L,pdu->port);
        lua_call(L,3,1);
        
        if(pdu->pkt->hdr.code < 0x20 &&  pdu->pkt->hdr.code>0xC0){
            pdu->pkt->hdr.code = COAP_RSPCODE_NOT_FOUND;
        }
        
        if(pdu->pkt->hdr.t == COAP_TYPE_CON){
            pdu->pkt->hdr.t = COAP_TYPE_ACK;
        }
        
        if (0 != (rc = coap_build(pdu->msg.p, &pdu->msg.len,pdu->pkt))){
            NODE_DBG("Bad packet rc=%d\n", rc);
            return 0;
        }
        coap_send(peer,pdu);
        
    }else if(pdu->pkt->hdr.t == COAP_TYPE_CON && pdu->pkt->hdr.code == 0){
        
    }else if(pdu->pkt->hdr.t == COAP_TYPE_NON && pdu->pkt->hdr.code > 0x20 &&  pdu->pkt->hdr.code < 0xC0){
        
        // receive non response
        NODE_DBG("receive non response\n");
        
    }else if( (pdu->pkt->hdr.t == COAP_TYPE_ACK || pdu->pkt->hdr.t == COAP_TYPE_CON) && 
            (pdu->pkt->hdr.code > 0x20 && pdu->pkt->hdr.code < 0xC0)){

        // receive confirm response
        NODE_DBG("receive confirm response\n");
        
        uint32_t bip;
        coap_tid_t id = COAP_INVALID_TID;
        ip_addr_t ipaddr;
        ipaddr.addr = ipaddr_addr(ip);
        c_memcpy(&bip, &ipaddr.addr, 4);
        NODE_DBG("bip is %d\n",bip);
        coap_transaction_id(bip, pdu->port, pdu->pkt, &id);
        NODE_DBG("transaction id is %d\n",id);
        
        coap_queue_t *result = coap_find_node(peer->queue, id);
        
        NODE_DBG("search result is %d\n",result);
        NODE_DBG("search response_ref is %d\n",result->pdu->response_ref);
        
        if(result){
            lua_rawgeti (L,LUA_REGISTRYINDEX,result->pdu->response_ref);
            //call handler(pkg,ip,port)
            lua_pushlightuserdata (L,pdu->pkt);
            luaL_getmetatable(L, "coap_package");
            lua_setmetatable(L, -2);
            lua_pushstring(L,pdu->ip);
            lua_pushinteger(L,pdu->port);
            lua_call(L,3,0);
            // 收到响应
            // 调用handler
            // 删除PDU队列
            
            // stop timer
            coap_timer_stop();
            // remove the node
            coap_remove_node(&peer->queue, id);
            // calculate time elapsed
            coap_timer_update(peer);
            coap_timer_start(peer);
        }
    }
    coap_delete_pdu(pdu);
    return 0;
}

static int lua_coap_peer_request(lua_State* L){
    // 准备参数
    int stack = 1;
    coap_msgtype_t type;
    coap_peer_t *peer = (coap_peer_t *) luaL_checkudata(L,stack,"coap_peer");
    stack++;
    coap_pdu_t *pdu = coap_new_pdu();
    coap_method_t method = luaL_checkinteger(L,stack);
    stack++;
    if(lua_isnumber(L,stack)){
        type = lua_tointeger(L, stack);
        stack++;
    }else{
        type = COAP_TYPE_NON;
    }
    size_t lurl;
    const char *uri = luaL_checklstring( L, stack, &lurl );
    stack++;
    const char *payload = NULL;
    int lpay = 0;
    if( lua_isstring(L, stack) ){
        payload = luaL_checklstring( L, stack, &lpay );
        stack++;
        if (payload == NULL)
            lpay = 0;
    }
    pdu->pkt->numopts = 0;
    // 解析URI
    coap_uri_t *url = coap_new_uri(uri,lurl);
    coap_make_request(&(pdu->scratch),peer->message_id,pdu->pkt,type,method,url,payload,lpay);
    pdu->port = url->port;
    c_memcpy(pdu->ip,url->host.s,url->host.length);
    int rc;
    if (0 != (rc = coap_build(pdu->msg.p, &(pdu->msg.len), pdu->pkt))){
        NODE_DBG("coap_build failed rc=%d\n", rc);
        return 0;
    }
    
    // 构建数据包
    // 注册Handler
    if (lua_isfunction(L, stack) || lua_islightfunction(L, stack)) {
        pdu->response_ref = luaL_ref ( L,LUA_REGISTRYINDEX );
    }
    int num;
    if (pdu->pkt->hdr.t == COAP_TYPE_CON){
        num = coap_send_confirmed(peer, pdu);
    } else {
        coap_send(peer, pdu);
        num = 1;
    }
    if(!num){
        coap_delete_pdu(pdu);
    }
    
    // 发送请求
    return 0;
}

static int lua_coap_peer_release(lua_State* L){
    coap_peer_t *pkt = (coap_peer_t *)luaL_checkudata(L,1,"coap_peer");
    luaL_unref(L, LUA_REGISTRYINDEX, pkt->sender_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, pkt->server_ref);
    //删除所有queue对象
    coap_delete_all(pkt->queue);
}



static int lua_coap_new(lua_State* L){
    extern unsigned short messageid;
    coap_peer_t *up = (coap_peer_t *)lua_newuserdata (L,sizeof(coap_peer_t));
    up->queue = NULL;
    up->L = L;
    up->message_id = (unsigned short)os_random();
    luaL_getmetatable(L, "coap_peer");
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_coap_read( lua_State* L ){
    size_t i;
    int value;
    coap_packet_t *pkt = (coap_packet_t *)luaL_checkudata(L,1,"coap_package");
    const char *name = luaL_checkstring( L, 2 );
    if(c_strcmp("code",name) == 0){
        lua_pushinteger (L, pkt->hdr.code);
        return 1;
    }else if(c_strcmp(name,"type") == 0){
        lua_pushinteger (L, pkt->hdr.t);
        return 1;
    }else if(c_strcmp(name,"mid") == 0){
        value =(int)((pkt->hdr.id[1] & 0xFF) | (pkt->hdr.id[0] & 0xFF)<<8);
        lua_pushinteger (L, value);
        return 1;
    }else if(c_strcmp(name,"token") == 0){
        lua_pushlstring (L, pkt->tok.p, pkt->tok.len);
        return 1;
    }else if(c_strcmp(name,"options") == 0){
        lua_newtable (L);// 3
        for (i=0;i<pkt->numopts;i++){
            lua_pushinteger(L,pkt->opts[i].num);
            lua_rawseti(L,3,i*2+1);
            if(pkt->opts[i].num == COAP_OPTION_CONTENT_FORMAT){
                value =(int)((pkt->opts[i].buf.p[1] & 0xFF) | (pkt->opts[i].buf.p[0] & 0xFF)<<8);
                lua_pushinteger(L,value);
            }else{
                lua_pushlstring(L,pkt->opts[i].buf.p,pkt->opts[i].buf.len);
            }
            lua_rawseti(L,3,i*2+2);
        }
        return 1;
    }else if(c_strcmp(name,"payload") == 0){
        lua_pushlstring (L, pkt->payload.p, pkt->payload.len);
        return 1;
/*    }else if(c_strcmp(name,"build") == 0){
        lua_pushcfunction (L,lua_coap_build);
        return 1;*/
    }else{
        return 0;
    }
}

static int lua_coap_write( lua_State* L ){
    int value,i,opts;
    coap_packet_t *pkt = (coap_packet_t *)luaL_checkudata(L,1,"coap_package");
    const char *name = luaL_checkstring( L, 2 );
    if(c_strcmp(name,"code") == 0){
        pkt->hdr.code = luaL_checkinteger (L, 3);
        return 0;
    }else if(c_strcmp(name,"type") == 0){
        pkt->hdr.t = luaL_checkinteger (L, 3);
        return 0;
    }else if(c_strcmp(name,"mid") == 0){
        value = luaL_checkinteger (L, 3);
        pkt->hdr.id[0] =  (uint8_t) (value & 0xFF);
        pkt->hdr.id[1] =  (uint8_t) ((value>>8) & 0xFF);
        return 0;
    }else if(c_strcmp(name,"token") == 0){
        pkt->tok.p = luaL_checklstring (L, 3 , &(pkt->tok.len));
        pkt->hdr.tkl = pkt->tok.len;
        return 0;
    }else if(c_strcmp(name,"options") == 0){
        luaL_checktype(L, 3, LUA_TTABLE);
        opts = lua_objlen (L,3);
        if(opts % 2){
            luaL_error(L,"options must be even");
        }
        pkt->numopts = opts / 2;
        for (i=0;i<pkt->numopts;i++){
            lua_rawgeti(L,3,i*2+1);
            pkt->opts[i].num = luaL_checkinteger (L, 4);
            NODE_DBG("table index is %d\n",i*2+1);
            lua_pop(L,1);
            lua_rawgeti(L,3,i*2+2);
            if(pkt->opts[i].num == COAP_OPTION_CONTENT_FORMAT){
                value = luaL_checkinteger (L, 4);
                NODE_DBG("value is %d\n",value);
                pkt->opts[i].buf.len = 2;
                pkt->content.p[1] = (uint8_t) (value & 0xFF);
                pkt->content.p[0] = (uint8_t) ((value>>8) & 0xFF);
                pkt->opts[i].buf.p = pkt->content.p;
                NODE_DBG("value is %d\n",value);
            }else{
                pkt->opts[i].buf.p = luaL_checklstring (L, 4 , &(pkt->opts[i].buf.len));
            }
            
            lua_pop(L,1);
        }
        return 0;
    }else if(c_strcmp(name,"payload") == 0){
        pkt->payload.p = luaL_checklstring (L, 3 , &(pkt->payload.len));
        return 0;
    }else{
        return 0;
    }
}

static int lua_coap_release( lua_State* L ){
    coap_packet_t *pkt = (coap_packet_t *)luaL_checkudata(L,1,"coap_package");
    if(pkt)
        c_free(pkt->content.p);
}

static const LUA_REG_TYPE coap_content_type[] = {
    { LSTRKEY( "none" ),        LNUMVAL( COAP_CONTENTTYPE_NONE ) },
    { LSTRKEY( "text_plain" ),  LNUMVAL( COAP_CONTENTTYPE_TEXT_PLAIN ) },
    { LSTRKEY( "link_format" ), LNUMVAL( COAP_CONTENTTYPE_APPLICATION_LINKFORMAT ) },
    { LSTRKEY( "xml" ),         LNUMVAL( COAP_CONTENTTYPE_APPLICATION_XML ) },
    { LSTRKEY( "octet_stream"), LNUMVAL( COAP_CONTENTTYPE_APPLICATION_OCTET_STREAM ) },
    { LSTRKEY( "exi" ),         LNUMVAL( COAP_CONTENTTYPE_APPLICATION_EXI ) },
    { LSTRKEY( "json" ),        LNUMVAL( COAP_CONTENTTYPE_APPLICATION_JSON ) },
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_msg_type[] = {
    { LSTRKEY( "con" ),    LNUMVAL( COAP_TYPE_CON ) },
    { LSTRKEY( "non" ),    LNUMVAL( COAP_TYPE_NON ) },
    { LSTRKEY( "ack" ),    LNUMVAL( COAP_TYPE_ACK ) },
    { LSTRKEY( "rst" ),    LNUMVAL( COAP_TYPE_RESET ) },
    { LNILKEY, LNILVAL}
};


static const LUA_REG_TYPE coap_method[] = {
    { LSTRKEY( "get" ),    LNUMVAL( COAP_METHOD_GET ) },
    { LSTRKEY( "post" ),   LNUMVAL( COAP_METHOD_POST ) },
    { LSTRKEY( "put" ),    LNUMVAL( COAP_METHOD_PUT ) },
    { LSTRKEY( "delete" ), LNUMVAL( COAP_METHOD_DELETE ) },
    
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_code[] = {
    { LSTRKEY( "created" ),LNUMVAL( COAP_RSPCODE_CREATED ) },
    { LSTRKEY( "deleted" ),LNUMVAL( COAP_RSPCODE_DELETED ) },
    { LSTRKEY( "vaild" ),  LNUMVAL( COAP_RSPCODE_VALID ) },
    { LSTRKEY( "changed" ),LNUMVAL( COAP_RSPCODE_CHANGED ) },
    { LSTRKEY( "content" ),LNUMVAL( COAP_RSPCODE_CONTENT ) },
    
    { LSTRKEY( "bad_request" ),        LNUMVAL(COAP_RSPCODE_BAD_REQUEST) },
    { LSTRKEY( "unauthorized" ),       LNUMVAL( COAP_RSPCODE_UNAUTHORIZED ) },
    { LSTRKEY( "bad_option" ),         LNUMVAL( COAP_RSPCODE_BAD_OPTION ) },
    { LSTRKEY( "forbidden" ),          LNUMVAL( COAP_RSPCODE_FORBIDDEN ) },
    { LSTRKEY( "not_found" ),          LNUMVAL( COAP_RSPCODE_NOT_FOUND ) },
    { LSTRKEY( "method_not_allowed" ), LNUMVAL( COAP_RSPCODE_METHOD_NOT_ALLOWED ) },
    { LSTRKEY( "not_acceptable" ),     LNUMVAL( COAP_RSPCODE_NOT_ACCEPTABLE ) },
    { LSTRKEY( "precondition_failed" ),LNUMVAL( COAP_RSPCODE_PRECONDITION_FAILED ) },
    { LSTRKEY( "request_entity_too_large" ), LNUMVAL( COAP_RSPCODE_REQUEST_ENTITY_TOO_LARGE ) },
    { LSTRKEY( "unsupported_content_format" ),LNUMVAL( COAP_RSPCODE_UNSUPPORTED_CONTENT_FORMAT ) },
    
    { LSTRKEY( "internal_server_error" ),  LNUMVAL( COAP_RSPCODE_INTERNAL_SERVER_ERROR ) },
    { LSTRKEY( "not_implemented" ),        LNUMVAL( COAP_RSPCODE_NOT_IMPLEMENTED ) },
    { LSTRKEY( "bad_gateway" ),            LNUMVAL( COAP_RSPCODE_BAD_GATEWAY ) },
    { LSTRKEY( "service_unavailable" ),    LNUMVAL( COAP_RSPCODE_SERVICE_UNAVAILABLE ) },
    { LSTRKEY( "gateway_timeout" ),        LNUMVAL( COAP_RSPCODE_GATEWAY_TIMEOUT ) },
    { LSTRKEY( "proxying_not_supported" ), LNUMVAL( COAP_RSPCODE_PROXYING_NOT_SUPPORTED ) },
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_options[] = {
    { LSTRKEY( "if_match" ),      LNUMVAL( COAP_OPTION_IF_MATCH ) },
    { LSTRKEY( "uri_host" ),      LNUMVAL( COAP_OPTION_URI_HOST ) },
    { LSTRKEY( "etag" ),          LNUMVAL( COAP_OPTION_ETAG ) },
    { LSTRKEY( "if_none_match" ), LNUMVAL( COAP_OPTION_IF_NONE_MATCH ) },
    { LSTRKEY( "observe" ),       LNUMVAL( COAP_OPTION_OBSERVE ) },
    { LSTRKEY( "uri_port" ),      LNUMVAL( COAP_OPTION_URI_PORT ) },
    { LSTRKEY( "location_path" ), LNUMVAL( COAP_OPTION_LOCATION_PATH ) },
    { LSTRKEY( "content_format" ),LNUMVAL( COAP_OPTION_CONTENT_FORMAT ) },
    { LSTRKEY( "max_age" ),       LNUMVAL( COAP_OPTION_MAX_AGE ) },
    { LSTRKEY( "uri_query" ),     LNUMVAL( COAP_OPTION_URI_QUERY ) },
    { LSTRKEY( "accept" ),        LNUMVAL( COAP_OPTION_ACCEPT ) },
    { LSTRKEY( "location_query" ),LNUMVAL( COAP_OPTION_LOCATION_QUERY ) },
    { LSTRKEY( "proxy_uri" ),     LNUMVAL( COAP_OPTION_PROXY_URI ) },
    { LSTRKEY( "proxy_scheme" ),  LNUMVAL( COAP_OPTION_PROXY_SCHEME ) },
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_peer[] = {
    { LSTRKEY( "handler" ),     LFUNCVAL( lua_coap_peer_regist ) }, //注册服务器处理函数
    { LSTRKEY( "sender" ),  LFUNCVAL( lua_coap_peer_sender ) }, 
    { LSTRKEY( "receive" ),       LFUNCVAL( lua_coap_peer_recv ) },
    { LSTRKEY( "request" ),    LFUNCVAL( lua_coap_peer_request ) }, 
    { LSTRKEY( "__index" ),    LROVAL( coap_peer ) }, 
    { LSTRKEY( "__gc" ),       LFUNCVAL( lua_coap_peer_release ) }, 
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_package[] = {
    { LSTRKEY( "__index" ),    LFUNCVAL( lua_coap_read ) },  //写入值
    { LSTRKEY( "__newindex" ), LFUNCVAL( lua_coap_write ) }, //读取值
    { LSTRKEY( "__gc" ),       LFUNCVAL( lua_coap_release ) }, //读取值
    { LNILKEY, LNILVAL}
};

static const LUA_REG_TYPE coap_map[] = {
    { LSTRKEY( "new" ),         LFUNCVAL( lua_coap_new ) },
//    { LSTRKEY( "packet" ),      LFUNCVAL( lua_coap_packet ) },
    { LSTRKEY( "__metatable" ), LROVAL( coap_map ) },
    { LSTRKEY( "options" ),     LROVAL( coap_options ) },
    { LSTRKEY( "content_type" ),LROVAL( coap_content_type ) },
    { LSTRKEY( "type" ),    LROVAL( coap_msg_type ) },
    { LSTRKEY( "code" ),        LROVAL( coap_code ) },
    { LSTRKEY( "method" ),        LROVAL( coap_method ) },
    { LNILKEY, LNILVAL}
};

int luaopen_coap( lua_State *L )
{
    luaL_rometatable(L, "coap_package", (void *)coap_package);
    luaL_rometatable(L, "coap_peer", (void *)coap_peer);  // create metatable for coap_package  
    return 0;
}

NODEMCU_MODULE(COAP, "coap", coap_map, luaopen_coap);
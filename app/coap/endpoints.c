#include "c_stdio.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "coap.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "os_type.h"
#include "user_interface.h"
#include "user_config.h"

void build_well_known_rsp(char *rsp, uint16_t rsplen);

void endpoint_setup(void)
{
    coap_setup();
}

static const coap_endpoint_path_t path_well_known_core = {2, {".well-known", "core"}};
static int handle_get_well_known_core(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    outpkt->content.p = (uint8_t *)c_zalloc(MAX_PAYLOAD_SIZE);      // this should be free-ed when outpkt is built in coap_server_respond()
    if(outpkt->content.p == NULL){
        NODE_DBG("not enough memory\n");
        return COAP_ERR_BUFFER_TOO_SMALL;
    }
    outpkt->content.len = MAX_PAYLOAD_SIZE;
    build_well_known_rsp(outpkt->content.p, outpkt->content.len);
    return coap_make_response(scratch, outpkt, (const uint8_t *)outpkt->content.p, c_strlen(outpkt->content.p), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_APPLICATION_LINKFORMAT);
}

static const coap_endpoint_path_t path_variable = {2, {"v1", "v"}};
static int handle_get_variable(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    const coap_option_t *opt;
    uint8_t count;
    int n;
    lua_State *L = lua_getstate();
    if (NULL != (opt = coap_findOptions(inpkt, COAP_OPTION_URI_PATH, &count)))
    {
        if ((count != ep->path->count ) && (count != ep->path->count + 1)) // +1 for /f/[function], /v/[variable]
        {
            NODE_DBG("should never happen.\n");
            goto end;
        }
        if (count == ep->path->count + 1)
        {
            coap_luser_entry *h = ep->user_entry->next;     // ->next: skip the first entry(head)
            while(NULL != h){
                if (opt[count-1].buf.len != c_strlen(h->name))
                {
                    h = h->next;
                    continue;
                }
                if (0 == c_memcmp(h->name, opt[count-1].buf.p, opt[count-1].buf.len))
                {
                    NODE_DBG("/v1/v/");
                    NODE_DBG((char *)h->name);
                    NODE_DBG(" match.\n");
                    if(c_strlen(h->name))
                    {
                        n = lua_gettop(L);
                        lua_getglobal(L, h->name);
                        if (!lua_isnumber(L, -1) && !lua_isstring(L, -1)) {
                            NODE_DBG ("should be a number or string.\n");
                            lua_settop(L, n);
                            return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_NOT_FOUND, COAP_CONTENTTYPE_NONE);
                        } else {
                            const char *res = lua_tostring(L,-1);
                            lua_settop(L, n);
                            return coap_make_response(scratch, outpkt, (const uint8_t *)res, c_strlen(res), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, h->content_type);
                        }
                    }
                } else {
                    h = h->next;
                }
            }
        }else{
            NODE_DBG("/v1/v match.\n");
            goto end;
        }
    }
    NODE_DBG("none match.\n");
end:
    return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
}

static const coap_endpoint_path_t path_function = {2, {"v1", "f"}};
static int handle_post_function(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    const coap_option_t *opt;
    uint8_t count;
    int n;
    lua_State *L = lua_getstate();
    if (NULL != (opt = coap_findOptions(inpkt, COAP_OPTION_URI_PATH, &count)))
    {
        if ((count != ep->path->count ) && (count != ep->path->count + 1)) // +1 for /f/[function], /v/[variable]
        {
            NODE_DBG("should never happen.\n");
            goto end;
        }
        if (count == ep->path->count + 1)
        {
            coap_luser_entry *h = ep->user_entry->next;     // ->next: skip the first entry(head)
            while(NULL != h){
                if (opt[count-1].buf.len != c_strlen(h->name))
                {
                    h = h->next;
                    continue;
                }
                if (0 == c_memcmp(h->name, opt[count-1].buf.p, opt[count-1].buf.len))
                {
                    NODE_DBG("/v1/f/");
                    NODE_DBG((char *)h->name);
                    NODE_DBG(" match.\n");

                    if(c_strlen(h->name))
                    {
                        n = lua_gettop(L);
                        lua_getglobal(L, h->name);
                        if (lua_type(L, -1) != LUA_TFUNCTION) {
                            NODE_DBG ("should be a function\n");
                            lua_settop(L, n);
                            return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_NOT_FOUND, COAP_CONTENTTYPE_NONE);
                        } else {
                            lua_pushlstring(L, inpkt->payload.p, inpkt->payload.len);     // make sure payload.p is filled with '\0' after payload.len, or use lua_pushlstring
                            lua_call(L, 1, 1);
                            if (!lua_isnil(L, -1)){  /* get return? */
                                if( lua_isstring(L, -1) )   // deal with the return string
                                {
                                    size_t len = 0;
                                    const char *ret = luaL_checklstring( L, -1, &len );
                                    if(len > MAX_PAYLOAD_SIZE){
                                        lua_settop(L, n);
                                        luaL_error( L, "return string:<MAX_PAYLOAD_SIZE" );
                                        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_NOT_FOUND, COAP_CONTENTTYPE_NONE);
                                    }
                                    NODE_DBG((char *)ret);
                                    NODE_DBG("\n");
                                    lua_settop(L, n);
                                    return coap_make_response(scratch, outpkt, ret, len, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
                                } 
                            } else {
                                lua_settop(L, n);
                                return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
                            }
                        }
                    }
                } else {
                    h = h->next;
                }
            }
        }else{
            NODE_DBG("/v1/f match.\n");
            goto end;
        }
    }
    NODE_DBG("none match.\n");    
end:
    return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_NOT_FOUND, COAP_CONTENTTYPE_NONE);
}

extern int lua_put_line(const char *s, size_t l);

static const coap_endpoint_path_t path_command = {2, {"v1", "c"}};
static int handle_post_command(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len == 0)
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_BAD_REQUEST, COAP_CONTENTTYPE_TEXT_PLAIN);
    if (inpkt->payload.len > 0)
    {
        char line[LUA_MAXINPUT];
        if (!coap_buffer_to_string(line, LUA_MAXINPUT, &inpkt->payload) &&
            lua_put_line(line, c_strlen(line))) {
            NODE_DBG("\nResult(if any):\n");
            system_os_post (LUA_TASK_PRIO, LUA_PROCESS_LINE_SIG, 0);
        }
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
    }
}

static uint32_t id = 0;
static const coap_endpoint_path_t path_id = {2, {"v1", "id"}};
static int handle_get_id(const coap_endpoint_t *ep, coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    id = system_get_chip_id();
    return coap_make_response(scratch, outpkt, (const uint8_t *)(&id), sizeof(uint32_t), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
}

coap_luser_entry var_head = {NULL,NULL,0};
coap_luser_entry *variable_entry = &var_head;

coap_luser_entry func_head = {NULL,NULL,0};
coap_luser_entry *function_entry = &func_head;

const coap_endpoint_t endpoints[] =
{
    {COAP_METHOD_GET, handle_get_well_known_core, &path_well_known_core, "ct=40", NULL},
    {COAP_METHOD_GET, handle_get_variable, &path_variable, "ct=0", &var_head},
    {COAP_METHOD_POST, handle_post_function, &path_function, NULL, &func_head},
    {COAP_METHOD_POST, handle_post_command, &path_command, NULL, NULL},
    {COAP_METHOD_GET, handle_get_id, &path_id, "ct=0", NULL},
    {(coap_method_t)0, NULL, NULL, NULL, NULL}
};

void build_well_known_rsp(char *rsp, uint16_t rsplen)
{
    const coap_endpoint_t *ep = endpoints;
    int i;
    uint16_t len = rsplen;

    c_memset(rsp, 0, len);

    len--; // Null-terminated string

    while(NULL != ep->handler)
    {
        if (NULL == ep->core_attr) {
            ep++;
            continue;
        }
        if (NULL == ep->user_entry){
            if (0 < c_strlen(rsp)) {
                c_strncat(rsp, ",", len);
                len--;
            }
        
            c_strncat(rsp, "<", len);
            len--;

            for (i = 0; i < ep->path->count; i++) {
                c_strncat(rsp, "/", len);
                len--;

                c_strncat(rsp, ep->path->elems[i], len);
                len -= c_strlen(ep->path->elems[i]);
            }

            c_strncat(rsp, ">;", len);
            len -= 2;

            c_strncat(rsp, ep->core_attr, len);
            len -= c_strlen(ep->core_attr);
        } else {
            coap_luser_entry *h = ep->user_entry->next;     // ->next: skip the first entry(head)
            while(NULL != h){
                if (0 < c_strlen(rsp)) {
                    c_strncat(rsp, ",", len);
                    len--;
                }
            
                c_strncat(rsp, "<", len);
                len--;

                for (i = 0; i < ep->path->count; i++) {
                    c_strncat(rsp, "/", len);
                    len--;

                    c_strncat(rsp, ep->path->elems[i], len);
                    len -= c_strlen(ep->path->elems[i]);
                }

                c_strncat(rsp, "/", len);
                len--;

                c_strncat(rsp, h->name, len);
                len -= c_strlen(h->name);

                c_strncat(rsp, ">;", len);
                len -= 2;

                c_strncat(rsp, ep->core_attr, len);
                len -= c_strlen(ep->core_attr);  

                h = h->next;
            }
        }
        ep++;
    }
}


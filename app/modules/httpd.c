// Module for network

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "lmem.h"

#include <string.h>
#include <strings.h>
#include <stddef.h>

#include <stdint.h>
#include "mem.h"
#include "osapi.h"
#include "vfs.h"

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/opt.h"

#include <lua.h>

#if defined(CLIENT_SSL_ENABLE) && defined(LUA_USE_MODULES_HTTPD) && defined(LUA_USE_MODULES_TLS)
#define TLS_MODULE_PRESENT
#endif

#include "net.h"

#define net_get_udata(L) net_get_udata_s(L, 1)

static int http_content_type(const char * uri) {
	char* v = strstr(uri, ".");
	if (v != 0) {
		v++;
		if (strncmp(v, "html\0", 5) == 0) return 1;
		else if (strncmp(v, "txt\0", 4) == 0) return 2;
		else if (strncmp(v, "css\0", 4) == 0) return 3;
		else if (strncmp(v, "lua\0", 4) == 0) return 4;
		else if (strncmp(v, "jvs\0", 4) == 0) return 6;
		else if (strncmp(v, "j\0", 2) == 0) return 6;
	}
	//printf("'no extension, using text/plain'\n");
	return 2;
}

/*
 * Request - R
 *		user: guest				default user (0x0007)
 *		uri:  esp.conf			The requested resource
 *		opts: a=2				Parameters to the URL separated by '?'
 *		data: abcd...			the data part from a POST request
 *		meth: GET				the methode, GET | POST
 *		Host:	192.168.1.204   the host (ourselfs)
 *		data-saved	-1			saved data, if -1 there is no data to be safed, no upload
 *		frm:1					the X-th frame we get (the 1st is intresting)
 *		User-Agent:	curl/7.35.0
 *		data-len:	0           the datalength of the packed (only for POST)
 *		action: 1-6				action to be taken as outlined below
 *
 *  action: the action derived by the request
 *  	1  GET - Request of predefined function
 *  	    curl -v http://192.168.1.204/heap
 *  	2. GET - Normal request
 *  		curl -v http://192.168.1.204/esp.conf
 *  	3
 *  	4
 *  	5
 *  	6. Fileupload
 *  		curl -H Expect: -v --data-binary @testIt http://192.168.1.204/up?filen=testIt
 *
 */
int httpd_checkPerm(lua_State *L, char* uri, char* user, char* meth, char* opts) {

	//printf("'httpd_checkPerm top:%i, uri:%s, user:%s, meth:%s '\n", lua_gettop(L), uri, user, meth);
	int uri_perm = 0;  // the permission the uri requestes
	int user_perm = 0; // the permission the user has
	int action = 0; //    the action to be executed 1:exec func, 2: send file,
	int rp = 0;

	lua_getglobal(L, "http");
	if (!lua_istable(L, -1) && !lua_isrotable(L, -1)) { // also need table just in case encoder has been overloaded
		luaL_error(L, "Cannot find http"); // fixme hier könnte man auch eine http tabelle erstellen
	}
	else {
		lua_getfield(L, -1, uri); // (sock,T-R, T-http, uri-permisson)
		//printf("= %s, ", lua_tostring(L, -1));

		if (lua_isnumber(L, -1)) {
			uri_perm = lua_tonumber(L, -1);
			//	printf("%i'\n", uri_perm);
		}
		else {
			lua_getfield(L, -2, "_any_"); // (sock,T-R, T-http, nil, _any_-permission)
			uri_perm = lua_tonumber(L, -1);
			//	printf("ee %i'\n", uri_perm);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
	}

	if (strcmp(user, "admin\0") == 0) user_perm = 0x0777;
	else if (strcmp(user, "user\0") == 0) user_perm = 0x0077;
	else
		user_perm = 0x0007;
	//printf("'user_perm: %i'\n", user_perm);

	if (strcmp(meth, "GET\0") == 0) {
		lua_pushstring(L, "f_");
		lua_pushstring(L, uri);
//		printf("'  -GET 1 top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, -2), lua_type(L, -1));
		lua_concat(L, 2);
//		printf("'  s:%s'\n", lua_tostring(L, -1));
//		printf("'  -2 top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, -2), lua_type(L, -1));
		lua_gettable(L, -2);
//		printf("'  -3 top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, -2), lua_type(L, -1));
		if (lua_type(L, -1) == 8) // 8 is lua_type 'function'
				{
			action = 1;
			rp = 0x0111; // we need the exec bit set
		}
		else // nil,
		{
			action = 2; // -> send file
			rp = 0x0444; // we need the read bit set
		}
		lua_pop(L, 1);
	}
	else if (strcmp(meth, "POST\0") == 0) {
		if (strcmp(uri, "up\0") == 0) {

			if (strncmp(opts, "filen=", 6) == 0) {
				int offset=6;
				if (*(opts + 6) == '/') offset=7; // remove leading / in filenames
				lua_pushstring(L, opts + offset);
				lua_setfield(L, -3, "fname");
				lua_pushstring(L, opts + offset);
//				printf("'  up, filen:%s'\n", lua_tostring(L, -1));
				lua_gettable(L, -2); // (sock,T-R, T-http, uri-permisson)
//				printf("= %s, ", lua_tostring(L, -1));
				if (lua_isnumber(L, -1)) {
					uri_perm = lua_tonumber(L, -1);
//					printf("%i'\n", uri_perm);
				}
				else {
					lua_getfield(L, -2, "_any_"); // (sock,T-R, T-http, nil, _any_-permission)
					uri_perm = lua_tonumber(L, -1);
//					printf("ee %i'\n", uri_perm);
					lua_pop(L, 1);
				}
				lua_pop(L, 1); // (sock,T-R, T-http )
				action = 6; // -> save file
				rp = 0x0222; // we need the write bit set
			}
			else {
//				printf("'  up, opt:%s not found'\n", lua_tostring(L, -1));
			}
		}
		else if (strcmp(uri, "up_n_run\0") == 0) {
			if (strncmp(opts, "filen=", 6) == 0) {
				lua_pushstring(L, opts + 6);
			//	printf("'  up, filen:%s'\n", lua_tostring(L, -1));
				lua_gettable(L, -2); // (sock,T-R, T-http, uri-permisson)
				if (lua_isnumber(L, -1)) {
					uri_perm = lua_tonumber(L, -1);
				}
				else {
					lua_getfield(L, -2, "_any_"); // (sock,T-R, T-http, nil, _any_-permission)
					uri_perm = lua_tonumber(L, -1);
					lua_pop(L, 1);
				}
				lua_pop(L, 1); // (sock,T-R, T-http )
				action = 5; // -> save file
				rp = 0x0333; // we need the write bit set
			}
			else {
			}
		}
		else if (strcmp(uri, "cmd\0") == 0) {
			action = 4; // -> save file
			rp = 0x0111; // we need the write bit set
		}
		else {
			action = 5; // -> save file
			rp = 0x0111; // we need the write bit set
		}
	}

//	printf("'  -e top:%i, action=%i, rp=%i, a:%i b:%i, actionR=%i '\n", lua_gettop(L), action, rp, lua_type(L, -2), lua_type(L, -1), rp & user_perm & uri_perm);

	lua_pop(L, 1);
	return ((rp & user_perm & uri_perm) > 0) ? action : -action;
}

/*
 *  called by httpd_recv_cb
 *  Return the number of args put on the stack
 */
static int parse_http_header(lnet_userdata *ud, lua_State *L, struct pbuf *p) {
	//printf("\' parse_http_header top:%i\'\n", lua_gettop(L) );
	int ret = 0;

	// add table
	lua_newtable(L);
	lua_pushinteger(L, ud->ht_head);
	lua_setfield(L, -2, "frm");
	//
	char* req = p->payload;
	char* req_end = req + p->len;
	char* uri;
	char* meth;
	char* opts;
	char* user = "guest";
	int c_len = -1;
	if (ud->ht_head == 1) {
		char* header_end = strstr(req, "\r\n\r\n"); // sollte unsigned sein

		// Check general header
		if (header_end == 0) {
//			printf("bad header end\n");
			pbuf_free(p);
			return -1;
		}
		// Get request methode
		meth = req;
		req = strstr(meth, " ");
		if ((req - meth) <= 4) {
			*req++ = 0;
			if (strcmp(meth, "GET") == 0 || strcmp(meth, "POST") == 0) {
				lua_pushstring(L, meth);
				lua_setfield(L, -2, "meth");
			}
		}
		// Get uri
		uri = req;
		req = strstr(uri, " HTTP");
		if (req > 0) {
			*req = 0;
			req += 11; // ' HTTP/1.1\r\n'

			opts = strstr(uri, "?"); // a trailing ? indicates a function to execute
			if (opts == 0) lua_pushstring(L, "");
			else {
				*opts++ = 0;
				(req - opts) == 11 ? lua_pushstring(L, "?") : lua_pushstring(L, opts);
			}
			lua_setfield(L, -2, "opts");

			strlen(uri) == 1 ? *uri = '/' : *uri++; // remove leading / in uri

			if (*uri == '/') {
				lua_getglobal(L, "http"); // (sock,T-R, T-http)
				lua_getfield(L, -1, "/"); // (sock,T-R, T-http, uri)
				lua_setfield(L, -3, "uri");
				lua_pop(L, 1);
			}
			else {
				lua_pushstring(L, uri);
				lua_setfield(L, -2, "uri");
			}
			lua_pushstring(L, user);
			lua_setfield(L, -2, "user");
		}

		// get HTTP header fields
		char * pch = strstr(req, "\r\n");
		while (pch != 0) {
			*pch++ = 0;
			*pch++ = 0;

			char* v = strstr(req, ": ");
			if (v != 0) {
				*v++ = 0;
				*v++ = 0;
				if (strncmp(req, "Content-Length", 14) == 0) {
					c_len = atol(v);
					lua_pushinteger(L, c_len);
				}
				else
					lua_pushstring(L, v);
				lua_setfield(L, -2, req);
			}

			req = pch;
			pch = strstr(req, "\r\n");
			if (pch == req) {
				pch = 0;
				req += 2;
//				printf("** End of header **\n");
			}
			if (pch >= req_end) pch = 0;
		}
//		printf("\' M:%s, U:%s, u:%s, O:%s, C-len:%i, data-len:%i End of header \'\n", meth, uri, user, opts, c_len, req_end - req);

// permission
		int action = httpd_checkPerm(L, uri, user, meth, opts);
		//printf("' action = %i'\n", action);
		lua_pushinteger(L, action);
		lua_setfield(L, -2, "action");

// do action
		ud->state = action;
		switch (action) {
		case 0:
			//printf("'don`t send file %i (404) %s'\n", action, uri);
			// Send back 404
			break;
		case 1:
			//printf("'send a file %i (200) %s'\n", action, uri);
			lua_pushinteger(L, http_content_type(uri));
			lua_setfield(L, -2, "content");
			// send back 200 and file
			break;
		case 2:
			//printf("'send file %i (200) %s'\n", action, uri);
			// send back 200 and file
			break;
		case 3:
			//printf("'don`t send file %i (404) %s'\n", action, uri);
			// send back 404
			break;
		case 4:
			//printf("'run cmd %i (201) %s ...'\n", action, uri);
				if (c_len > req_end - req) { // printf("\'wait for next frm\'\n");
					ud->state = -4;
					ud->refT = luaL_ref(L, LUA_REGISTRYINDEX); // push the table to the registry
					return -2;
			}
			// run cmd
			break;
		case 5:
			//printf("'do function POST/form %i (302) %s '\n", action, uri);
				if (c_len > req_end - req) { // printf("\'wait for next frm\'\n");
					ud->state = -5;
					ud->refT = luaL_ref(L, LUA_REGISTRYINDEX); // push the table to the registry
					return -2;
				}

			// execute function
			break;
		case 6: // uri=up uploading a file - we are receiving a file
			//printf("'do uri=%i (201) %s ... save file'\n", action, uri);
			if ((strcmp(meth, "POST\0") == 0) && (strcmp(uri, "up") == 0)) {
				// fixme check free space ...

				lua_pushstring(L, "~");
				lua_getglobal(L, "http_reqs"); // We use the req nr of http reqs as temp filename
				const char * cnt = lua_tolstring(L, -1, 0);
				lua_pop(L, 1);
				lua_pushstring(L, cnt);
				lua_concat(L, 2);
				cnt = lua_tolstring(L, -1, 0);
				lua_setfield(L, -2, "tfilen");

				int fd = vfs_open(cnt, "w");
				if (fd) {
					//printf("' fd save opened %i as %s\n", fd, cnt);
					ud->fd = fd;
				}
				//else
					//printf("' could not open..\n");

				ud->req_left = c_len;
				//ud->show_prog = 1;
				int wsize = 0;

				if (req_end > req) {
					wsize = vfs_write(ud->fd, req, req_end - req);
					//fixme catch errors when not able to write random
					ud->req_left = ud->req_left - wsize;
				}

				if (ud->show_prog == 1) printf("--%i %i written, left:%i \n", ud->ht_head, wsize, ud->req_left);

				if (ud->req_left > 0) { // printf("\'wait for next frm\'\n");
					ud->state = -6;
					//return -2;
				}
				else {
//					printf("'close fd %i, case 6\n'", ud->fd);
					vfs_close(ud->fd); // we close the socket in lua
					ud->fd = 0;
				}
			}
			// execute function
			break;
		default:
			ud->state = 0;
			// send Back 404
			break;
		}
	}
	//printf("2.frm %i\n", lua_gettop(L));
	// act on it ....
	// a 'POST /cmd' with data (cmd) in 1st or 2nd packet
	//         do node.input(R['cmd']) when we have the complete cmd
	//printf("\' state is %i top:%i 1\'\n",ud->state, lua_gettop(L) );

	if (ud->state == -4) {
		lua_pop(L, 1); // drop current header table and load saved
		lua_rawgeti(L, LUA_REGISTRYINDEX, ud->refT);
		luaL_unref(L, LUA_REGISTRYINDEX, ud->refT);
		ud->refT = 0;
		ud->state = 0;
	}
	else if (ud->state == -5) { // we have a complete command Json
		lua_pop(L, 1); // drop current header table and load saved
		lua_rawgeti(L, LUA_REGISTRYINDEX, ud->refT);
		luaL_unref(L, LUA_REGISTRYINDEX, ud->refT);
		ud->state = 0;
		ud->refT = 0;
	}
	else if (ud->state == -68) { // we have a complete file
		lua_pop(L, 1); // drop current header table and load saved
		lua_rawgeti(L, LUA_REGISTRYINDEX, ud->refT);
		luaL_unref(L, LUA_REGISTRYINDEX, ud->refT);
		ud->refT = 0;
		ud->state = 0;
	}
	//printf("\' state is %i top:%i left:%i, len:%i\'\n",ud->state, lua_gettop(L), ud->req_left, req_end - req );

//	printf("2.frm %i\n", lua_gettop(L));

	lua_pushinteger(L, ud->req_left);
	lua_setfield(L, -2, "data-saved");
	lua_pushlstring(L, req, req_end - req);
	lua_setfield(L, -2, "data");
	lua_pushinteger(L, req_end - req);
	lua_setfield(L, -2, "data-len");

	//printf("ret\n");

	return ret;
}

/*
 * called by lwhttp_tcp_recv_cb when receiving someting (1st or consecutive pkg)
 */
void httpd_recv_cb(lnet_userdata *ud, struct pbuf *p, ip_addr_t *addr, u16_t port) {
	if (ud->client.cb_receive_ref == LUA_NOREF) {
		pbuf_free(p);
		return;
	}
	//printf("'httpd_recv_cb'\n");

	ud->ht_head += 1;
	/* when we want to write the received data to a file, we open a file and set
	 the filehandle and the requested size with net:set_recv_fd(fd, size) in the llwhttp_userdata
	 */
	if (ud->state == -6) // && (ud->fd != 0))
			{
		int wsize = vfs_write(ud->fd, p->payload, p->len);
		//fixme catch errors when not able to write
		ud->req_left = ud->req_left - wsize;
		if (ud->show_prog == 1) printf("--%i %i written, left:%i \n", ud->ht_head, wsize, ud->req_left);

		//ud->tcp_pcb->flags |= TF_ACK_NOW;
		tcp_recved(ud->tcp_pcb, TCP_WND);

		if (ud->req_left == 0) {
			wsize = vfs_close(ud->fd); // we close the socket in lua
			//printf("'close fd %i, recv %i'\n", ud->fd, wsize);
			ud->fd = 0;
		}
	}

	if (ud->req_left <= 0) {
		lua_State *L = lua_getstate();
		int num_args = 2;
		lua_rawgeti(L, LUA_REGISTRYINDEX, ud->client.cb_receive_ref);
		lua_rawgeti(L, LUA_REGISTRYINDEX, ud->self_ref);
		//lua_pushlstring(L, p->payload, p->len);

		int ret = parse_http_header(ud, L, p);
		//printf("\' post parse is %i top:%i ret:%i 1\'\n",ud->state, lua_gettop(L), ret );

		if (ret == -2) { // -2 means we are waiting for more data
			lua_settop(L, 0);
		}
		else {
			//printf("\' post parse is %i top:%i ret:%i 2\'\n",ud->state, lua_gettop(L), ret );

			/* here we have finished parsing the header, and we have now
			 * the table on top of the stack
			 *  Note: for 'cmd' and 'cmdJson' request we might have two frames read so far but the data should be complete
			 */
			lua_call(L, num_args, 0);
		}
	}

	if (p->next) {
		struct pbuf *pp = p->next;
		//printf("'next is set'\n");
		httpd_recv_cb(ud, pp, addr, port);
	}
	pbuf_free(p);
}

/*
 * Send a response back - this is the header ...
 *
 * Lua: client:send_H (data, function(c)), socket:send(port, ip, data, function(s))
 * (sock,uri,ct,size,1 ,'')
 *
 * status code:
 * 	  200:
 * 	  201:
 * 	  301:
 * 	  401:
 *
 * content type:
 * 		0: - none -
 *		1: text/html
 *		2: text/plain
 *		3: text/css
 *		4: text/x-lua
 *		5: text/event-stream
 *		6: application/x-javascript
 *		7: application/json
 *
 * content lenght:
 *      -1 : none
 *      else lenght
 */
int http_send_H(lua_State *L) {
	//printf("'httpd_send_H'\n");
	lnet_userdata *ud = net_get_udata(L);
	if (!ud || ud->type == TYPE_TCP_SERVER)
		return luaL_error(L, "invalid user data");
	ip_addr_t addr;
	uint16_t port;
	const char *data;
	size_t datalen = 0;
	int stack = 2;
	int bsize = 0;
	char buf[40]; /* to store the formatted item */

	uint16_t statusCode = luaL_checkinteger(L, stack++); // 2
	uint16_t contentTyp = luaL_checkinteger(L, stack++); // 3
	int32_t contLength = luaL_checkinteger(L, stack++); // 4
	uint16_t cacheContr = luaL_checkinteger(L, stack++); // 5
	data = luaL_checklstring(L, stack++, &datalen); // 6

	if (!ud->pcb || ud->self_ref == LUA_NOREF) {
		//fixme this should be cleaner done
		//printf("'prep header' top:%i\n", lua_gettop(L));
		vfs_close(ud->fd); // we close the socket in lua
		ud->fd = 0;
		lua_settop(L, 0);
		//lwhttp_send_file(L);
		return 0;
		return luaL_error(L, "not connected");
	}
	err_t err;

	if (ud->type == TYPE_TCP_CLIENT) {
		switch (statusCode) {
		case 201:
			bsize = sprintf(buf, "HTTP/1.1 201 Completed\r\n");
			break;
		case 302:
			bsize = sprintf(buf, "HTTP/1.1 302 Found\r\n");
			break;
		case 401:
			bsize = sprintf(buf, "HTTP/1.1 401 Unauthorized\r\n");
			break;
		case 404:
			bsize = sprintf(buf, "HTTP/1.1 404 Not Found\r\n");
			break;
		case 408:
			bsize = sprintf(buf, "HTTP/1.1 408 Request Time-out\r\n");
			break;
		case 501:
			bsize = sprintf(buf, "HTTP/1.1 501 Not Implemented\r\n");
			break;
		case 503:
			bsize = sprintf(buf, "HTTP/1.1 503 Service Unavailable\r\n");
			break;
		case 100:
			bsize = sprintf(buf, "HTTP/1.1 100 Continue\r\n");
			break;
		case 0:
		default:
			bsize = sprintf(buf, "HTTP/1.1 200 OK\r\n");
			break;
		}
		err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);


		bsize = sprintf(buf, "Server: NodeMCU on ESP8266\r\n");
		err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);

		if ( statusCode == 401) {
			err = tcp_write(ud->tcp_pcb, data, datalen, TCP_WRITE_FLAG_COPY);
			datalen=0;
		}

		if (contentTyp >= 1) {
			switch (contentTyp) {
			case 1: // text/html
				bsize = sprintf(buf, "Content-Type: text/html\r\n");
				break;
			case 2: // text/plain
				bsize = sprintf(buf, "Content-Type: text/plain\r\n");
				break;
			case 3: // application/json
				bsize = sprintf(buf, "Content-Type: text/css\r\n");
				break;
			case 4: // application/json
				bsize = sprintf(buf, "Content-Type: text/x-lua\r\n");
				break;
			case 5: // application/json
				bsize = sprintf(buf, "Content-Type: text/event-stream\r\n");
				break;
			case 6: // application/json
				bsize = sprintf(buf, "Content-Type: application/x-javascript\r\n");
				break;
			case 7: // application/json
				bsize = sprintf(buf, "Content-Type: application/json\r\n");
				break;
			}
			err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);
		}

		switch (cacheContr) {
		case 1:
			bsize = sprintf(buf, "Cache-control: no-store\r\n");
			err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);
			break;
		case 2:
			err = tcp_write(ud->tcp_pcb,
					"Cache-control: public, immutable, max-age=31536000\r\n",
					52, TCP_WRITE_FLAG_COPY);
			break;
		case 3:
			err = tcp_write(ud->tcp_pcb,
					"Cache-control: public, immutable, max-age=31536000\r\n",
					52, TCP_WRITE_FLAG_COPY);
			err = tcp_write(ud->tcp_pcb, "Content-Encoding: gzip\r\n", 24,
					TCP_WRITE_FLAG_COPY);
			break;
		}

		if (contentTyp == 5) {
			bsize = sprintf(buf, "Connection: Keep-Alive\r\n");
			err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);

		} else {
			bsize = sprintf(buf, "Connection: close\r\n");
			err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);

			if (contLength >= 0) {
				bsize = sprintf(buf, "Content-Length: %d\r\n",
						contLength ? contLength : datalen);
			}

			if (datalen >= 1)
				err = tcp_write(ud->tcp_pcb, buf, bsize, TCP_WRITE_FLAG_COPY);
		}

		if (statusCode != 302) {
			err = tcp_write(ud->tcp_pcb, "\r\n", 2, TCP_WRITE_FLAG_COPY);
		}

		if (datalen > 0)
			err = tcp_write(ud->tcp_pcb, data, datalen, TCP_WRITE_FLAG_COPY);
		else {
			//printf("'sent now\n");
			tcp_output(ud->tcp_pcb); // send data immediately
		}
	}
	lua_pop(L, 5); // 1
	//printf("'sent header' top:%i, len:%i\n", lua_gettop(L), datalen);
	return lwip_lua_checkerr(L, err);
}

int http_processReq(lua_State *L) {
	lnet_userdata *ud = net_get_udata(L);
	if (!ud || ud->type != TYPE_TCP_CLIENT)
		return luaL_error(L, "invalid user data");

//	printf("'http_processReq top:%i, a:%i b:%i, req_left:%i '\n", lua_gettop(L), lua_type(L, 1), lua_type(L, 2), ud->req_left );

	if (!lua_istable(L, -1) && !lua_isrotable(L, -1)) { //
		luaL_error(L, "Arg 1 is not a table");
	}

	lua_getfield(L, -1, "action"); // socket, R-Table, action,

	if (lua_tointeger(L, -1) <= 0) {
		lua_settop(L, 1); // (sock)
		lua_pushinteger(L, 404); // (sock,200)
		lua_pushinteger(L, 0); // (sock,uri,ct)
		lua_pushinteger(L, 0); // (sock,uri,ct, size)
		lua_pushinteger(L, 1); // (sock,uri,ct, size, 1)
		lua_pushstring(L, ""); // (sock,uri,ct,size,1,'')
		http_send_H(L); // (sock,uri,ct,size,1,'') -> ()
	} else {
		switch (lua_tointeger(L, -1)) {
		case 1:
			lua_getglobal(L, "http"); // socket, R-Table, action, http-Table,
			lua_pushstring(L, "f_");
			lua_getfield(L, 2, "uri");
			lua_concat(L, 2);
			lua_gettable(L, -2); // socket, R-Table, action, http-Table, f_<function>
			lua_insert(L, 1); // f_<function>, socket, R-Table, action, http-Table
			lua_settop(L, 3); // f_<function>, socket, R-Table
			//printf("' do:1, top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, 1), lua_type(L, 2), lua_type(L, 3));
			lua_call(L, 2, 0);
			break;
		case 2:
			lua_getfield(L, 2, "uri"); // socket, R-Table, action, uri
			lua_remove(L, 2); // socket, action, uri
			lua_remove(L, 2); // socket, uri
			//printf("' do:2, top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, 1), lua_type(L, 2));

			//lwhttp_send_fileX(L);
			httpd_send_fileX(L);
			break;
		case 4:
			lua_getfield(L, 2, "data"); // socket, R-Table, action, data
			lua_remove(L, 2); // socket, action, data
			lua_remove(L, 2); // socket, data
			//printf("' do:4, top:%i, a:%i'\n", lua_gettop(L), lua_type(L, 1));
			int ret = luaL_dostring(L, lua_tostring(L, -1));
			//printf("' do:4, top:%i, ret:%i'\n", lua_gettop(L), ret);
			lua_settop(L, 1); // (sock)
			lua_pushinteger(L, (ret == 0) ? 201 : 501); // (sock,200)
			lua_pushinteger(L, 0); // (sock,uri,ct)
			lua_pushinteger(L, 0); // (sock,uri,ct, size)
			lua_pushinteger(L, 1); // (sock,uri,ct, size, 1)
			lua_pushstring(L, ""); // (sock,uri,ct,size,1,'')
			http_send_H(L); // (sock,uri,ct,size,1,'') -> ()
			break;
		case 5:
			lua_getglobal(L, "http"); // socket, R-Table, action, http-Table,
			lua_pushstring(L, "f_");
			lua_getfield(L, 2, "uri");
			lua_concat(L, 2);
			lua_gettable(L, -2); // socket, R-Table, action, http-Table, f_<function>
			lua_insert(L, 1); // f_<function>, socket, R-Table, action, http-Table
			lua_settop(L, 3); // f_<function>, socket, R-Table
			//printf("' do:1, top:%i, a:%i b:%i'\n", lua_gettop(L), lua_type(L, 1), lua_type(L, 2), lua_type(L, 3));
			lua_call(L, 2, 0);
			break;

		case 6:
			if (ud->req_left <= 0) {
				lua_getfield(L, 2, "tfilen"); // socket, R-Table, action, data
				const char* filename = lua_tostring(L, -1);
				struct vfs_stat stat;
				if (!vfs_stat(filename, &stat)) {
					lua_getfield(L, 2, "Content-Length");
					lua_getfield(L, 2, "fname");
					//printf("'have file %s, size:%i, Con-size%i'\n", filename, stat.size, lua_tointeger(L, -2));
					if (lua_tointeger(L, -2) == stat.size) {
						vfs_remove(lua_tostring(L, -1));
						vfs_rename(filename, lua_tostring(L, -1));
					}
					lua_pop(L, 2);
				} else {
					//printf("'bad filehandle send file %s'\n", filename);
				}

				lua_settop(L, 1); // (sock)
				lua_pushinteger(L, (ret == 0) ? 201 : 501); // (sock,200)
				lua_pushinteger(L, 0); // (sock,uri,ct)
				lua_pushinteger(L, 0); // (sock,uri,ct, size)
				lua_pushinteger(L, 1); // (sock,uri,ct, size, 1)
				lua_pushinteger(L, stat.size);
				lua_pushstring(L, lua_tostring(L, -1)); // (sock,uri,ct,size,1,'')
				lua_remove(L, -2);
				http_send_H(L); // (sock,uri,ct,size,1,'') -> ()
			}
			break;
		}
	}
	lua_settop(L, 0);
	return 0;
}

/*
 *  send a fileX - direct without one-by-one (sock,uri)
 */
int httpd_send_fileX(lua_State *L) {
//	printf("httpd_send_fileX  top:%i, a:%i b:%i\n", lua_gettop(L), lua_type(L, 1), lua_type(L, 2));

	lnet_userdata *ud = net_get_udata(L);
	if (!ud || ud->type != TYPE_TCP_CLIENT)
		return luaL_error(L, "invalid user data");

	int stack = 2;
	const char * filename = luaL_checkstring(L, 2);
	int ct = http_content_type(filename);
	int cCtrl = 1;
	if (filename[0] == 'z' && filename[1] == '/')
		cCtrl = 3;
	struct vfs_stat stat;
	if (!vfs_stat(filename, &stat)) {
//		printf("'new send file %s, type %i, size:%i %c %c'\n", filename, ct, stat.size, filename[0], filename[1]);
		int fd = vfs_open(filename, "r");

		if (fd) {
//			printf("fd opened %i\n", fd);
			ud->fd = fd;
		}
//		else
//			printf("could not open %s\n", filename);

	}
	//else {
//		printf("'bad filehandle send file %s, type %i'\n", filename, ct);
//	}

	if (ud->fd > 0) {
		lua_pop(L, 1); // (sock)
		lua_pushinteger(L, 200); // (sock,200)
		lua_pushinteger(L, ct); // (sock,uri,ct)
		lua_pushinteger(L, stat.size); // (sock,uri,ct, size)
		lua_pushinteger(L, cCtrl); // (sock,uri,ct, size, 1)
		lua_pushstring(L, ""); // (sock,uri,ct,size,1,'')
		http_send_H(L); // (sock,uri,ct,size,1,'') -> ()
		return 0;
	} else {
		lua_pop(L, 1); // (sock)
		lua_pushinteger(L, 404); // (sock,200)
		lua_pushinteger(L, 0); // (sock,uri,ct)
		lua_pushinteger(L, 0); // (sock,uri,ct, size)
		lua_pushinteger(L, 1); // (sock,uri,ct, size, 1)
		lua_pushstring(L, ""); // (sock,uri,ct,size,1,'')
		http_send_H(L); // (sock,uri,ct,size,1,'') -> ()
		//lua_settop(L, 0);
		//lwhttp_send_file(L); // (sock,uri,ct,size,1,'') -> ()
		return 0;
	}
}

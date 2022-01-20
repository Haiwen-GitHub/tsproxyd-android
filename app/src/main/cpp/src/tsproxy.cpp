/*
 * tsproxyd         ts http proxy server
 *
 */

/*
 * workflow:
 * hloop_new -> hloop_create_tcp_server -> hloop_run ->
 * on_accept -> HV_ALLOC(http_conn_t) -> hio_readline ->
 * on_recv -> parse_http_request_line -> hio_readline ->
 * on_recv -> parse_http_head -> ...  -> hio_readline ->
 * on_head_end -> hio_setup_upstream ->
 * on_upstream_connect -> hio_write_upstream(head) ->
 * on_body -> hio_write_upstream(body) ->
 * on_upstream_close -> hio_close ->
 * on_close -> HV_FREE(http_conn_t)
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hv/hv.h"
#include "hv/hloop.h"
#include "hv/hlog.h"
#include "hv/hbase.h"
#include "hv/hversion.h"
#include "hv/hlog.h"
#include "hv/iniparser.h"

#if (defined(ANDROID) || defined(__ANDROID__))
#define printf hlogi
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_PROXY_PORT 1080
#define DEFAULT_DEST_PORT 8080

#define DEFAULT_DEST_PORT_IM 8080
#define DEFAULT_DEST_PORT_FILE 8084
#define DEFAULT_DEST_PORT_AUDIO 8086
#define DEFAULT_DEST_PORT_VIDEO 8088
#define DEFAULT_DEST_PORT_CONTACT 8090
#define DEFAULT_DEST_PORT_SETTING 8092
#define DEFAULT_DEST_PORT_BR_PTT 8094
#define DEFAULT_DEST_PORT_LOCATION 8096


#define HTTP_KEEPALIVE_TIMEOUT  60000 // ms
#define HTTP_MAX_URL_LENGTH     256
#define HTTP_MAX_HEAD_LENGTH    1024


#define HTTP_MAX_METHOD_LENGTH 32
#define HTTP_MAX_HEAD_MSG_LENGTH 64
#define HTTP_MAX_IP_LENGTH 64

typedef struct proxyd_ctx_s {
    char    run_dir[MAX_PATH];
    char    program_name[MAX_PATH];

    char    confile[MAX_PATH]; // default etc/${program}.conf
    char    logfile[MAX_PATH]; // default logs/${program}.log

    char proxy_host[HTTP_MAX_IP_LENGTH];
    int proxy_port;
    int dest_port;
    int dest_port_im;
    int dest_port_file;
    int dest_port_audio;
    int dest_port_video;
    int dest_port_contact;
    int dest_port_settings;
    int dest_port_br_ptt;
    int dest_port_location;
    int  proxy_ssl;
    int thread_num;
    int multi_mode;

    hloop_t*  accept_loop;
    hloop_t** worker_loops;
} proxyd_ctx_s;

typedef enum {
    s_begin,
    s_first_line,
    s_request_line = s_first_line,
    s_status_line = s_first_line,
    s_head,
    s_head_end,
    s_body,
    s_end
} http_state_e;


typedef struct {
    // first line
    int             major_version;
    int             minor_version;
    union {
        // request line
        struct {
            char method[HTTP_MAX_METHOD_LENGTH];
            char path[HTTP_MAX_URL_LENGTH];
        };

        // status line
        struct {
            int  status_code;
            char status_message[HTTP_MAX_HEAD_MSG_LENGTH];
        };
    };
    // headers
    char        host[HTTP_MAX_HEAD_MSG_LENGTH];
    char        backend_host[HTTP_MAX_HEAD_MSG_LENGTH];
    int         content_length;
    char        content_type[HTTP_MAX_HEAD_MSG_LENGTH*2];
    unsigned    keepalive:  1;
    unsigned    proxy:      1;
    unsigned    connected:  1;
    char        user_agent[HTTP_MAX_HEAD_MSG_LENGTH];
    char        accept[HTTP_MAX_HEAD_MSG_LENGTH];
    char        transfer_encoding[HTTP_MAX_HEAD_MSG_LENGTH];
    char        expect[HTTP_MAX_HEAD_MSG_LENGTH];
    char        cache_control[HTTP_MAX_HEAD_MSG_LENGTH];
    char        origin[HTTP_MAX_HEAD_MSG_LENGTH];
    char        pragma[HTTP_MAX_HEAD_MSG_LENGTH];
    char        head[HTTP_MAX_HEAD_LENGTH];
    int         head_len;
    // body
    char*       body;
    int         body_len; // body_len = content_length
} http_msg_t;

typedef struct {
    hio_t*          io;
    http_state_e    state;
    http_msg_t      request;
//  http_msg_t      response;
} http_conn_t;


proxyd_ctx_s   g_proxyd_ctx;

static void proxy_ctx_init(int argc, char** argv) {
    get_run_dir(g_proxyd_ctx.run_dir, sizeof(g_proxyd_ctx.run_dir));
    strncpy(g_proxyd_ctx.program_name, hv_basename(argv[0]), sizeof(g_proxyd_ctx.program_name));

    memset(g_proxyd_ctx.confile, 0, sizeof(g_proxyd_ctx.confile));
    memset(g_proxyd_ctx.logfile, 0, sizeof(g_proxyd_ctx.logfile));

    // original no use now
    strcpy(g_proxyd_ctx.proxy_host, "0.0.0.0");
    g_proxyd_ctx.proxy_port = DEFAULT_PROXY_PORT;
    g_proxyd_ctx.dest_port = DEFAULT_DEST_PORT;
    g_proxyd_ctx.dest_port_im = DEFAULT_DEST_PORT_IM;
    g_proxyd_ctx.dest_port_file = DEFAULT_DEST_PORT_FILE;
    g_proxyd_ctx.dest_port_audio = DEFAULT_DEST_PORT_AUDIO;
    g_proxyd_ctx.dest_port_video = DEFAULT_DEST_PORT_VIDEO;
    g_proxyd_ctx.dest_port_contact = DEFAULT_DEST_PORT_CONTACT;
    g_proxyd_ctx.dest_port_settings = DEFAULT_DEST_PORT_SETTING;
    g_proxyd_ctx.dest_port_br_ptt = DEFAULT_DEST_PORT_BR_PTT;
    g_proxyd_ctx.dest_port_location = DEFAULT_DEST_PORT_LOCATION;
    g_proxyd_ctx.proxy_ssl = 0;
    g_proxyd_ctx.thread_num = 1;
    g_proxyd_ctx.accept_loop = NULL;
    g_proxyd_ctx.worker_loops = NULL;
    g_proxyd_ctx.multi_mode = 0;
}

static int http_request_dump(http_conn_t* conn, char* buf, int len) {
    http_msg_t* msg = &conn->request;
    int offset = 0;

    // request line
    const char *path = msg->path;
    if (msg->proxy) {
        const char* pos = strstr(msg->path, "://");
        pos = pos ? pos + 3 : msg->path;
        path = strchr(pos, '/');

        // const char* pos1 = strstr(msg->path, "proxy");
        // pos = pos1 ? pos1 + 5 : pos;
        // path = strchr(pos, '/');
    }

    if (path == NULL) path = "/";

    offset += snprintf(buf + offset, len - offset, "%s %s HTTP/%d.%d\r\n", msg->method, path, msg->major_version, msg->minor_version);
   
    // headers
    if (msg->proxy) {
        if (strlen(msg->backend_host) > 0) {
            offset += snprintf(buf + offset, len - offset, "Host: %s\r\n", msg->backend_host);
        }
        
        /*
        if (msg->head_len) {
            memcpy(buf + offset, msg->head, msg->head_len);
            offset += msg->head_len;
        }
        */

        offset += snprintf(buf + offset, len - offset, "Connection: %s\r\n", msg->keepalive ? "keep-alive" : "close");

        if (msg->content_length > 0) {
            offset += snprintf(buf + offset, len - offset, "Content-Length: %d\r\n", msg->content_length);
        }
        if (*msg->content_type) {
            offset += snprintf(buf + offset, len - offset, "Content-Type: %s\r\n", msg->content_type);
        }

        if (strlen(msg->user_agent) > 0) {
            offset += snprintf(buf + offset, len - offset, "User-Agent: %s\r\n", msg->user_agent);
        } 

        if (strlen(msg->accept) > 0) {
            offset += snprintf(buf + offset, len - offset, "Accept: %s\r\n", msg->accept);
        }

        if (strlen(msg->transfer_encoding) > 0) {
            offset += snprintf(buf + offset, len - offset, "Transfer-Encoding: %s\r\n", msg->transfer_encoding);
        }

        if (strlen(msg->expect) > 0) {
            offset += snprintf(buf + offset, len - offset, "Expect: %s\r\n", msg->expect);
        }

        if (strlen(msg->cache_control) > 0) {
            offset += snprintf(buf + offset, len - offset, "Cache-Control: %s\r\n", msg->cache_control);
        }

        if (strlen(msg->origin) > 0) {
            offset += snprintf(buf + offset, len - offset, "Origin: %s\r\n", msg->origin);
        }

        if (strlen(msg->pragma) > 0) {
            offset += snprintf(buf + offset, len - offset, "Pragma: %s\r\n", msg->pragma);
        }

        // char peeraddrstr[SOCKADDR_STRLEN] = {0};
        // SOCKADDR_STR(hio_peeraddr(conn->io), peeraddrstr);
        // offset += snprintf(buf + offset, len - offset, "X-Origin-IP: %s\r\n", peeraddrstr);
    } else {
        offset += snprintf(buf + offset, len - offset, "Connection: %s\r\n", msg->keepalive ? "keep-alive" : "close");
        if (msg->content_length > 0) {
            offset += snprintf(buf + offset, len - offset, "Content-Length: %d\r\n", msg->content_length);
        }
        if (*msg->content_type) {
            offset += snprintf(buf + offset, len - offset, "Content-Type: %s\r\n", msg->content_type);
        }
    }

    // TODO: Add your headers
    offset += snprintf(buf + offset, len - offset, "\r\n");

    // body
    if (msg->body && msg->content_length > 0) {
        memcpy(buf + offset, msg->body, msg->content_length);
        offset += msg->content_length;
    }

    printf("http_request_dump offset: %d\n", offset);

    return offset;
}


static bool parse_http_request_line(http_conn_t* conn, char* buf, int len) {
    // GET / HTTP/1.1
    http_msg_t* req = &conn->request;
    sscanf(buf, "%s %s HTTP/%d.%d", req->method, req->path, &req->major_version, &req->minor_version);
    if (req->major_version != 1) return false;
    if (req->minor_version == 1) {
        req->keepalive = 1;
    } else {
        req->keepalive = 0;
    }

    printf("%s %s HTTP/%d.%d\r\n", req->method, req->path, req->major_version, req->minor_version);

    return true;
}

static bool parse_http_head(http_conn_t* conn, char* buf, int len) {
    http_msg_t* req = &conn->request;
    // Content-Type: text/html
    const char* key = buf;
    const char* val = buf;

    char* delim = strchr(buf, ':');
    if (!delim) return false;
    *delim = '\0';
    val = delim + 1;

    // trim space
    while (*val == ' ') ++val;
    printf("%s: %s\r\n", key, val);
    if (stricmp(key, "Host") == 0) {
        strncpy(req->host, val, sizeof(req->host) - 1);
    } else if (stricmp(key, "Content-Length") == 0) {
        req->content_length = atoi(val);
    } else if (stricmp(key, "Content-Type") == 0) {
        strncpy(req->content_type, val, sizeof(req->content_type) - 1);
    } else if (stricmp(key, "Connection") == 0 || stricmp(key, "Proxy-Connection") == 0) {
        if (stricmp(val, "close") == 0) {
            req->keepalive = 0;
        }
    } else if (stricmp(key, "User-Agent") == 0) {
        strncpy(req->user_agent, val, sizeof(req->user_agent) - 1);
    } else if (stricmp(key, "Accept") == 0) {
        strncpy(req->accept, val, sizeof(req->accept) - 1);
    } else if (stricmp(key, "Transfer-Encoding") == 0) {
        strncpy(req->transfer_encoding, val, sizeof(req->transfer_encoding) - 1);
    } else if (stricmp(key, "Expect") == 0) {
        strncpy(req->expect, val, sizeof(req->expect) - 1);
    } else if (stricmp(key, "Cache-Control") == 0) {
        strncpy(req->cache_control, val, sizeof(req->cache_control) - 1);
    } else if (stricmp(key, "Origin") == 0) {
        strncpy(req->origin, val, sizeof(req->origin) - 1);
    } else if (stricmp(key, "Pragma") == 0) {
        strncpy(req->pragma, val, sizeof(req->pragma) - 1);
    }
    else {
        /*
        SSE：
        Content-Type: text/event-stream
        Cache-Control: no-cache
        Connection: keep-alive
        Origin: null
        Pragma: no-cache
        "Access-Control-Allow-Origin": '*',
        */
    }

    return true;
}

static void on_upstream_connect(hio_t* upstream_io) {

    http_conn_t* conn = (http_conn_t*)hevent_userdata(upstream_io);
    http_msg_t* req = &conn->request;

    // send head
    char stackbuf[HTTP_MAX_HEAD_LENGTH + 1024] = {0};
    char* buf = stackbuf;
    int buflen = sizeof(stackbuf);
    int msglen = http_request_dump(conn, buf, buflen);
    printf("dump:\n[\n%s\n]\nlen: %d\n", stackbuf, msglen);

    int nsize = 0;
    while (nsize < msglen) {
        nsize += hio_write(upstream_io, buf, msglen);
        printf("hio_write nsize: %d\n", nsize);
    }

    //hv_msleep(100);
    if (conn->state != s_end) {
        // start recv body then upstream
        req->connected = 1;
        hio_read_start(conn->io);
    } else {
        if (req->keepalive) {
            // Connection: keep-alive\r\n
            // reset and receive next request
            memset(&conn->request,  0, sizeof(http_msg_t));
            // memset(&conn->response, 0, sizeof(http_msg_t));
            conn->state = s_first_line;
            hio_readline(conn->io);
        }
    }

    // start recv response
    hio_read_start(upstream_io);   
}

static int parse_proxy_port(http_msg_t* req) {
    int dest_port = g_proxyd_ctx.dest_port;

    do {
        if (strlen(req->path) <= 0) {
            fprintf(stderr, "path is empty!\n");
            break;
        }

        const char* path = req->path;
        const char* pos = strstr(req->path, "://");
        pos = pos ? pos + 3 : req->path;
        path = strchr(pos, '/');
        if (path == NULL) {
            fprintf(stderr, "path is NULL!\n");
            break;
        }

        printf("parse dest path:%s\n", path);

        if (strstr(path, "/im") == path) {
            dest_port = g_proxyd_ctx.dest_port_im;
            break;
        }

        if (strstr(path, "/audio/ptt/broadcast") == path) {
            dest_port = g_proxyd_ctx.dest_port_br_ptt;
            break;
        }

        if (strstr(path, "/audio") == path) {
            dest_port = g_proxyd_ctx.dest_port_audio;
            break;
        }

        if (strstr(path, "/video") == path) {
            dest_port = g_proxyd_ctx.dest_port_video;
            break;
        }

        if (strstr(path, "/file") == path) {
            dest_port = g_proxyd_ctx.dest_port_file;
            break;
        }

        if (strstr(path, "/contact") == path || strstr(path, "/group") == path) {
            dest_port = g_proxyd_ctx.dest_port_contact;
            break;
        }

        if (strstr(path, "/settings") == path) {
            dest_port = g_proxyd_ctx.dest_port_settings;
            break;
        }

        if (strstr(path, "/location") == path) {
            dest_port = g_proxyd_ctx.dest_port_location;
            break;
        }

    } while(0);

    if (dest_port == 0) {
        printf("find dest_port failed dest_port:%d\n", dest_port);
        dest_port = g_proxyd_ctx.dest_port;
    }

    printf("parse dest port:%d\n", dest_port);
    return dest_port;
}

static int on_head_end(http_conn_t* conn) {
    http_msg_t* req = &conn->request;
    if (req->host[0] == '\0') {
        fprintf(stderr, "No Host header!\n");
        return -1;
    }

    char backend_host[64] = {0};
    strcpy(backend_host, req->host);
    char* pos = strchr(backend_host, ':');
    if (pos) {
        *pos = '\0';
        // TODO : do not use port in url
        // backend_port = atoi(pos + 1);
    }

    int backend_port = parse_proxy_port(req);
    // do not proxy my self
    if (backend_port == g_proxyd_ctx.proxy_port &&
        (strcmp(backend_host, g_proxyd_ctx.proxy_host) == 0 ||
         strcmp(backend_host, "localhost") == 0 ||
         strcmp(backend_host, "127.0.0.1") == 0)) {
        req->proxy = 0;
        fprintf(stderr, "same machine port！\n");
        return 0;
    }

    // if (!strstr(req->path, "proxy")) {
    //     fprintf(stderr, "path is not proxy!\n");
    //     return 0;
    // }

    // NOTE: below for proxy
    req->proxy = 1;
    req->connected = 0;
    int backend_ssl = strncmp(req->path, "https", 5) == 0 ? 1 : 0;
    
    printf("upstream %s:%d ssl:%d\n", backend_host, backend_port, backend_ssl);
    memset(req->backend_host, 0, sizeof(req->backend_host));
    snprintf(req->backend_host, sizeof(req->backend_host), "%s:%d", backend_host, backend_port);

    hloop_t* loop = hevent_loop(conn->io);
    // hio_t* upstream_io = hio_setup_tcp_upstream(conn->io, backend_host, backend_port, backend_ssl);
    hio_t* upstream_io = hio_create_socket(loop, backend_host, backend_port, HIO_TYPE_TCP, HIO_CLIENT_SIDE);
    if (upstream_io == NULL) {
        fprintf(stderr, "Failed to upstream %s:%d!\n", backend_host, backend_port);
        return -3;
    }
    if (backend_ssl) {
        hio_enable_ssl(upstream_io);
    }
    hevent_set_userdata(upstream_io, conn);
    hio_setup_upstream(conn->io, upstream_io);
    //hio_setcb_read(conn->io, hio_write_upstream);
    hio_setcb_read(upstream_io, hio_write_upstream);
    //hio_setcb_close(conn->io, hio_close_upstream);
    hio_setcb_close(upstream_io, hio_close_upstream);
    hio_setcb_connect(upstream_io, on_upstream_connect);
    hio_connect(upstream_io);
    return 0;
}

static int on_body(http_conn_t* conn, void* buf, int readbytes) {
    // empty
    char* str = (char*)buf;
    if (readbytes == 2 && str[0] == '\r' && str[1] == '\n') {
        return 0;
    }

    http_msg_t* req = &conn->request;
    if (req->proxy && req->connected) {
        //hv_msleep(100);
        hio_write_upstream(conn->io, buf, readbytes);
    }
    return 0;
}

static int on_request(http_conn_t* conn) {
    // NOTE: just reply 403, please refer to examples/tinyhttpd if you want to reply other.
    http_msg_t* req = &conn->request;
    char buf[256] = {0};
    int len = snprintf(buf, sizeof(buf), "HTTP/%d.%d %d %s\r\nContent-Length: 0\r\n\r\n",
            req->major_version, req->minor_version, 403, "Forbidden");
    hio_write(conn->io, buf, len);
    return 403;
}

static void on_close(hio_t* io) {
    printf("on_close fd=%d error=%d\n", hio_fd(io), hio_error(io));
    http_conn_t* conn = (http_conn_t*)hevent_userdata(io);
    if (conn) {
        HV_FREE(conn);
        hevent_set_userdata(io, NULL);
    }
    hio_close_upstream(io);
}

static const char* get_state_message(int state) {
    switch(state) {
    case s_begin:
        return (const char*)"ns_begin";
    case s_first_line:
        return (const char*)"s_first_line";
    case s_head:
        return (const char*)"s_head";
    case s_head_end:
        return (const char*)"s_head_end";
    case s_body:
        return (const char*)"s_body";
    case s_end:
        return (const char*)"s_end";
    default:
        return (const char*)"";
    }
}

static void on_recv(hio_t* io, void* buf, int readbytes) {
    char* str = (char*)buf;
    
    printf("\non_recv fd = %d readbytes = %d\n", hio_fd(io), readbytes);
    printf("%.*s", readbytes, str);
    
    http_conn_t* conn = (http_conn_t*)hevent_userdata(io);
    http_msg_t* req = &conn->request;

    printf("state=%s\r\n", get_state_message(conn->state));
    switch (conn->state) {
    case s_begin:
        conn->state = s_first_line;
    case s_first_line:
        if (readbytes < 2) {
            fprintf(stderr, "Not match \r\n!");
            printf("Not match \r\n!");
            hio_close(io);
            return;
        }
        str[readbytes - 2] = '\0';
        
        if (parse_http_request_line(conn, str, readbytes - 2) == false) {
            fprintf(stderr, "Failed to parse http request line:\n%s\n", str);
            printf("Failed to parse http request line:\n%s\n", str);
            hio_close(io);
            return;
        }

        // start read head
        conn->state = s_head;
        hio_readline(io);
        break;
    case s_head:
        if (readbytes < 2) {
            fprintf(stderr, "Not match \r\n!");
            printf("Not match \r\n!");
            hio_close(io);
            return;
        }
        if (readbytes == 2 && str[0] == '\r' && str[1] == '\n') {
            conn->state = s_head_end;
        } else {
            if (req->head_len + readbytes < HTTP_MAX_HEAD_LENGTH) {
                memcpy(req->head + req->head_len, buf, readbytes);
                req->head_len += readbytes;
            }

            str[readbytes - 2] = '\0';
            if (parse_http_head(conn, str, readbytes - 2) == false) {
                fprintf(stderr, "Failed to parse http head:\n%s\n", str);
                printf("Failed to parse http head:\n%s\n", str);
                hio_close(io);
                return;
            }
            hio_readline(io);
            break;
        }
    case s_head_end:
        if (on_head_end(conn) < 0) {
            fprintf(stderr, "Failed to on_head_end\n");
            printf("Failed to on_head_end\n");
            hio_close(io);
            return;
        }

        if (req->content_length == 0) {
            conn->state = s_end;
            if (req->proxy) {
                // NOTE: wait upstream connect!
            } else {
                goto s_end;
            }
        } else {
            conn->state = s_body;
            if (req->proxy) {
                // NOTE: start read body on_upstream_connect
                // hio_read_start(io);
            } else {
                // WARN: too large content_length should read multiple times!
                hio_readbytes(io, req->content_length);
            }
            break;
        }
    case s_body:
        if (on_body(conn, buf, readbytes) < 0) {
            fprintf(stderr, "Failed to on_body\n");
            printf("Failed to on_body\n");
            hio_close(io);
            return;
        }

        req->body = str;
        req->body_len += readbytes;
        if (readbytes == req->content_length) {
            conn->state = s_end;
        } else {
            // Not end
            break;
        }
    case s_end:
s_end:
        // received complete request
        if (req->proxy) {
            // NOTE: reply by upstream
        } else {
            on_request(conn);
        }

        if (hio_is_closed(io)) return;

        if (req->keepalive) {
            // Connection: keep-alive\r\n
            // reset and receive next request
            memset(&conn->request,  0, sizeof(http_msg_t));
            // memset(&conn->response, 0, sizeof(http_msg_t));
            conn->state = s_first_line;
            hio_readline(io);
        } else {
            // Connection: close\r\n
            if (req->proxy) {
                // NOTE: wait upstream close!
            } else {
                printf("end state hio_close\n");
                hio_close(io);
            }
        }
        break;
    default: break;
    }
}

static void new_conn_event(hevent_t* ev) {
    hloop_t* loop = ev->loop;
    hio_t* io = (hio_t*)hevent_userdata(ev);
    hio_attach(loop, io);

    
    char localaddrstr[SOCKADDR_STRLEN] = {0};
    char peeraddrstr[SOCKADDR_STRLEN] = {0};
    
    printf("tid=%ld connfd=%d [%s] <= [%s]\n",
            (long)hv_gettid(),
            (int)hio_fd(io),
            SOCKADDR_STR(hio_localaddr(io), localaddrstr),
            SOCKADDR_STR(hio_peeraddr(io), peeraddrstr));

    hio_setcb_close(io, on_close);
    hio_setcb_read(io, on_recv);
    hio_set_keepalive_timeout(io, HTTP_KEEPALIVE_TIMEOUT);

    http_conn_t* conn = NULL;
    HV_ALLOC_SIZEOF(conn);
    conn->io = io;
    
    hevent_set_userdata(io, conn);
    // start read first line
    conn->state = s_first_line;
    hio_readline(io);
}

static hloop_t* get_next_loop() {
    static int s_cur_index = 0;
    if (s_cur_index == g_proxyd_ctx.thread_num) {
        s_cur_index = 0;
    }
    return g_proxyd_ctx.worker_loops[s_cur_index++];
}

static void on_accept(hio_t* io) {
    hio_detach(io);

    hloop_t* worker_loop = get_next_loop();
    hevent_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.loop = worker_loop;
    ev.cb = new_conn_event;
    ev.userdata = io;
    hloop_post_event(worker_loop, &ev);
}

static HTHREAD_ROUTINE(worker_thread) {
    hloop_t* loop = (hloop_t*)userdata;
    hloop_run(loop);
    return 0;
}

static HTHREAD_ROUTINE(accept_thread) {
    hloop_t* loop = (hloop_t*)userdata;
    hio_t* listenio = hloop_create_tcp_server(loop, g_proxyd_ctx.proxy_host, g_proxyd_ctx.proxy_port, on_accept);
    if (listenio == NULL) {
        exit(1);
    }
    if (g_proxyd_ctx.proxy_ssl) {
        hio_enable_ssl(listenio);
    }
    printf("tsproxyd listening on %s:%d, listenfd=%d, thread_num=%d\n",
            g_proxyd_ctx.proxy_host, g_proxyd_ctx.proxy_port, hio_fd(listenio), g_proxyd_ctx.thread_num);
    hloop_run(loop);
    return 0;
}

void print_help(char *program_name) {
    printf("Usage: %s proxy_port proxy_dest_port [thread_num]\n", program_name);
    printf("Usage: %s -c <confile>\n", program_name);
}

static void printf_ctx_args() {
    printf("run_dir=%s\n", g_proxyd_ctx.run_dir);
    hlogi("run_dir: %s\n", g_proxyd_ctx.run_dir);
    printf("program_name=%s\n", g_proxyd_ctx.program_name);
    hlogi("program_name: %s\n", g_proxyd_ctx.program_name);
    printf("log_file: %s\n", g_proxyd_ctx.logfile);
    hlogi("log_file: %s\n", g_proxyd_ctx.logfile);
    printf("proxy_port: %d\n", g_proxyd_ctx.proxy_port);
    hlogi("proxy_port: %d\n", g_proxyd_ctx.proxy_port);
    printf("dest_port: %d\n", g_proxyd_ctx.dest_port);
    hlogi("dest_port: %d\n", g_proxyd_ctx.dest_port);
    printf("multi_mode: %d\n", g_proxyd_ctx.multi_mode);
    hlogi("multi_mode: %d\n", g_proxyd_ctx.multi_mode);
    printf("dest_port_im: %d\n", g_proxyd_ctx.dest_port_im);
    hlogi("dest_port_im: %d\n", g_proxyd_ctx.dest_port_im);
    printf("dest_port_file: %d\n", g_proxyd_ctx.dest_port_file);
    hlogi("dest_port_file: %d\n", g_proxyd_ctx.dest_port_file);
    printf("dest_port_audio: %d\n", g_proxyd_ctx.dest_port_audio);
    hlogi("dest_port_audio: %d\n", g_proxyd_ctx.dest_port_audio);
    printf("dest_port_video: %d\n", g_proxyd_ctx.dest_port_video);
    hlogi("dest_port_video: %d\n", g_proxyd_ctx.dest_port_video);
    printf("dest_port_contact: %d\n", g_proxyd_ctx.dest_port_contact);
    hlogi("dest_port_contact: %d\n", g_proxyd_ctx.dest_port_contact);
    printf("dest_port_settings: %d\n", g_proxyd_ctx.dest_port_settings);
    hlogi("dest_port_settings: %d\n", g_proxyd_ctx.dest_port_settings);
    printf("dest_port_br_ptt: %d\n", g_proxyd_ctx.dest_port_br_ptt);
    hlogi("dest_port_br_ptt: %d\n", g_proxyd_ctx.dest_port_br_ptt);
    printf("dest_port_location: %d\n", g_proxyd_ctx.dest_port_location);
    hlogi("dest_port_location: %d\n", g_proxyd_ctx.dest_port_location);
    printf("thread_num: %d\n", g_proxyd_ctx.thread_num);
    hlogi("thread_num: %d\n", g_proxyd_ctx.thread_num);
}

static int parse_confile(const char* confile) {
    if (strlen(confile) <= 0) {
        fprintf(stderr, "confile [%s] is null\n", confile);
        exit(-40);
    }

    printf("confile: %s\n", confile);

    IniParser ini;
    int ret = ini.LoadFromFile(confile);
    if (ret != 0) {
        printf("Load confile [%s] failed: %d\n", confile, ret);
        fprintf(stderr, "Load confile [%s] failed: %d\n", confile, ret);
        exit(-40);
    }

    // logfile
    std::string str = ini.GetValue("log_file");
    if (!str.empty()) {
        strncpy(g_proxyd_ctx.logfile, str.c_str(), sizeof(g_proxyd_ctx.logfile));
    } else {
        strncpy(g_proxyd_ctx.logfile, g_proxyd_ctx.run_dir, sizeof(g_proxyd_ctx.logfile));

        strcat(g_proxyd_ctx.logfile + strlen(g_proxyd_ctx.run_dir), "/logs/tsproxyd_default.log");
    }
    hlog_set_file(g_proxyd_ctx.logfile);

    // loglevel
    str = ini.GetValue("log_level");
    if (!str.empty()) {
        hlog_set_level_by_str(str.c_str());
    }

    // log_filesize
    str = ini.GetValue("log_filesize");
    if (!str.empty()) {
        hlog_set_max_filesize_by_str(str.c_str());
    }

    // log_remain_days
    str = ini.GetValue("log_remain_days");
    if (!str.empty()) {
        hlog_set_remain_days(atoi(str.c_str()));
    }

    // log_fsync
    str = ini.GetValue("log_fsync");
    if (!str.empty()) {
        logger_enable_fsync(hlog, getboolean(str.c_str()));
    }

    hlogi("%s libhv version: %s", g_proxyd_ctx.program_name, hv_compile_version());
    hlog_fsync();

    // proxy port
    g_proxyd_ctx.proxy_port = ini.Get<int>("port_proxyd");
    if (g_proxyd_ctx.proxy_port <= 0) {
        g_proxyd_ctx.proxy_port = DEFAULT_PROXY_PORT;
    }

    //default single proxy port
    g_proxyd_ctx.dest_port = ini.Get<int>("port_dest");
    if (g_proxyd_ctx.dest_port <= 0) {
        g_proxyd_ctx.dest_port = DEFAULT_DEST_PORT;
    }

    // multi mode
    g_proxyd_ctx.multi_mode = ini.Get<int>("multi_mode");
    if (g_proxyd_ctx.multi_mode <= 0) {
        g_proxyd_ctx.multi_mode = 0;
    }

    // im port
    g_proxyd_ctx.dest_port_im = ini.Get<int>("port_dest_im");
    if (g_proxyd_ctx.dest_port_im <= 0) {
        g_proxyd_ctx.dest_port_im = DEFAULT_DEST_PORT_IM;
    }

    // file port
    g_proxyd_ctx.dest_port_file = ini.Get<int>("port_dest_file");
    if (g_proxyd_ctx.dest_port_file <= 0) {
        g_proxyd_ctx.dest_port_file = DEFAULT_DEST_PORT_FILE;
    }

    // audio port
    g_proxyd_ctx.dest_port_audio = ini.Get<int>("port_dest_audio");
    if (g_proxyd_ctx.dest_port_audio <= 0) {
        g_proxyd_ctx.dest_port_audio = DEFAULT_DEST_PORT_AUDIO;
    }

    // video port
    g_proxyd_ctx.dest_port_video = ini.Get<int>("port_dest_video");
    if (g_proxyd_ctx.dest_port_video <= 0) {
        g_proxyd_ctx.dest_port_video = DEFAULT_DEST_PORT_VIDEO;
    }

    // contact port
    g_proxyd_ctx.dest_port_contact = ini.Get<int>("port_dest_contact");
    if (g_proxyd_ctx.dest_port_contact <= 0) {
        g_proxyd_ctx.dest_port_contact = DEFAULT_DEST_PORT_CONTACT;
    }

    // setting port
    g_proxyd_ctx.dest_port_settings = ini.Get<int>("port_dest_settings");
    if (g_proxyd_ctx.dest_port_settings <= 0) {
        g_proxyd_ctx.dest_port_settings = DEFAULT_DEST_PORT_SETTING;
    }

    // setting port
    g_proxyd_ctx.dest_port_br_ptt = ini.Get<int>("port_dest_br_ptt");
    if (g_proxyd_ctx.dest_port_br_ptt <= 0) {
        g_proxyd_ctx.dest_port_br_ptt = DEFAULT_DEST_PORT_BR_PTT;
    }

    // location port
    g_proxyd_ctx.dest_port_location = ini.Get<int>("port_dest_location");
    if (g_proxyd_ctx.dest_port_location <= 0) {
        g_proxyd_ctx.dest_port_location = DEFAULT_DEST_PORT_LOCATION;
    }

    // ssl
    g_proxyd_ctx.proxy_ssl = ini.Get<int>("ssl");
    if (g_proxyd_ctx.proxy_ssl == 0) {
        g_proxyd_ctx.proxy_ssl = 0;
    }

    g_proxyd_ctx.thread_num = ini.Get<int>("thread_num");
    if (g_proxyd_ctx.thread_num == 0) {
        g_proxyd_ctx.thread_num = get_ncpu();
    }
    if (g_proxyd_ctx.thread_num == 0) {
        g_proxyd_ctx.thread_num = 1;
    }

    hlogi("parse_confile('%s') OK", confile);
    return 0;
}

static int parse_args(int argc, char** argv) {
    if (argc < 3) {
        print_help(argv[0]);
        exit(-10);
    } else if (argc == 3 && (strcmp(argv[1], "-c") == 0)) {
        proxy_ctx_init(argc, argv);
        parse_confile(argv[2]);
        printf_ctx_args();
        return 0;
    } else if (argc == 3 && (strcmp(argv[1], "-t") == 0)) {
        proxy_ctx_init(argc, argv);
        parse_confile(argv[2]);
        printf_ctx_args();
        exit(0);
    }  else if  (argc > 4) {
        print_help(argv[0]);
        exit(-10);
    } else {
        proxy_ctx_init(argc, argv);
        g_proxyd_ctx.proxy_port = atoi(argv[1]);
        if (g_proxyd_ctx.proxy_port <= 0) {
            g_proxyd_ctx.proxy_port = DEFAULT_PROXY_PORT;
        }

        g_proxyd_ctx.dest_port = atoi(argv[2]);
        if (g_proxyd_ctx.dest_port <= 0) {
            g_proxyd_ctx.dest_port = DEFAULT_DEST_PORT;
        }

        if (argc > 3) {
            g_proxyd_ctx.thread_num = atoi(argv[3]);
        } else {
            g_proxyd_ctx.thread_num = get_ncpu();
        }
        if (g_proxyd_ctx.thread_num == 0) {
            g_proxyd_ctx.thread_num = 1;
        }

        g_proxyd_ctx.multi_mode = 0;

        // log
        if (strlen(g_proxyd_ctx.logfile) == 0) {
            strncpy(g_proxyd_ctx.logfile, g_proxyd_ctx.run_dir, sizeof(g_proxyd_ctx.logfile));
            strcat(g_proxyd_ctx.logfile + strlen(g_proxyd_ctx.run_dir), "/logs/tsproxyd_default.log");
            hlog_set_file(g_proxyd_ctx.logfile);
        }
        printf_ctx_args();

        return 0;
    }
}


int main(int argc, char** argv) {
    if (parse_args(argc, argv) != 0) {
        printf("parse args failed!\n");
        print_help(argv[0]);
        return -10;
    }

    // debug debug
    // else {
    //     return 0;
    // }

    // malloc work threads
    g_proxyd_ctx.worker_loops = (hloop_t**)malloc(sizeof(hloop_t*) * g_proxyd_ctx.thread_num);
    for (int i = 0; i < g_proxyd_ctx.thread_num; ++i) {
        g_proxyd_ctx.worker_loops[i] = hloop_new(HLOOP_FLAG_AUTO_FREE);
        hthread_create(worker_thread, g_proxyd_ctx.worker_loops[i]);
    }

    g_proxyd_ctx.accept_loop = hloop_new(HLOOP_FLAG_AUTO_FREE);
    accept_thread(g_proxyd_ctx.accept_loop);
    printf("roxyd exit\n");
    return 0;
}


/*
int init_daemon(int argc, char** argv)
{
    pid_t pid;
    printf("init_daemon start!\n");
    if ((pid = fork()) == -1) {
        printf("Fork error !\n");
        //exit(1);
        return -1;
    }
    printf("init_daemon 1\n");
    // 父进程退出
    if (pid != 0) {
        //exit(0);
    } else {
        printf("child 1 \n");
        // 子进程开启新会话，并成为会话首进程和组长进程
        setsid();
        if ((pid = fork()) == -1) {
            printf("Fork error !\n");
            //exit(-1);
            return -1;
        }

        printf("init_daemon 3\n");
        // 结束第一子进程，第二子进程不再是会话首进程
        if (pid != 0) {
            //exit(0);
        } else {
            printf("child 2 \n");
            int ret = chdir("/sdcard");      // 改变工作目录
            printf("ret:%d\n", ret);
            umask(0);           // 重设文件掩码

            int file_max = sysconf(_SC_OPEN_MAX);
            printf("init_daemon 4 file_max:%d\n", file_max);
        //    for (; i < file_max; ++i) {
        //       close(i);        // 关闭打开的文件描述符
        //    }
             main(argc, argv);

        }
    }
    printf("init_daemon end!\n");
    return 0;
}
*/

#ifdef __cplusplus
}
#endif

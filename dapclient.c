#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <string.h>
#include <stdlib.h>

#include "dapclient.h"
#include "utils.h"

static const char *daprq_msg_format =
        "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>"
        "<attestationRequest>"
        "<version>"
        "<majorVersion>1</majorVersion>"
        "<minorVersion>2</minorVersion>"
        "</version>"
        "<trustRoot>4L4k+5F5wyBbYmR4z/k55ZY+jJq+5m41zcOuKmO/IlY=</trustRoot>"
        "<nonce>%s</nonce>"
        "<componentID>*</componentID>"
        "</attestationRequest>";

static void dapclient_event_cb(struct bufferevent *bev, short what, void *ctx);
static void dapclient_data_cb(struct bufferevent *bev, void *ctx);

int dapclient_request(struct event_base *base, struct evhttp_uri *uri)
{
    unsigned char *buf = NULL;
    char msg[1024] = {0};
    unsigned int size = 0;
    struct sockaddr_in sin;
    evutil_socket_t fd;
    struct bufferevent *conn;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    inet_aton(evhttp_uri_get_host(uri), &sin.sin_addr);
    sin.sin_port = htons(evhttp_uri_get_port(uri));
    fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd == -1) {
        return -1;
    }
    conn = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!conn) {
        evutil_closesocket(fd);
        return -1;
    }
    bufferevent_setcb(conn, dapclient_data_cb, NULL, dapclient_event_cb, NULL);
    if (bufferevent_enable(conn, EV_READ | EV_WRITE)) {
        bufferevent_free(conn);
        return -1;
    }
    if (bufferevent_socket_connect(conn, (struct sockaddr *)&sin, sizeof(sin))) {
        bufferevent_free(conn);
        return -1;
    }
    if (-1 == get_certificate_nonce(&buf, &size)) {
        return -1;
    }
    snprintf(msg, 1024, daprq_msg_format, buf);
    bufferevent_write(conn, msg, strlen(msg));
    free(buf);
    return 0;
}

static void dapclient_event_cb(struct bufferevent *bev, short what, void *ctx)
{
    (void)bev;
    (void)ctx;
    switch (what) {
    case BEV_EVENT_READING:
        break;
    case BEV_EVENT_WRITING:
        break;
    case BEV_EVENT_EOF:
        break;
    case BEV_EVENT_ERROR:
        break;
    case BEV_EVENT_TIMEOUT:
        break;
    case BEV_EVENT_CONNECTED:
        break;
    default:
        break;
    }
}

static void dapclient_data_cb(struct bufferevent *bev, void *ctx)
{
    (void)ctx;
    short event = bufferevent_get_enabled(bev);
    if (EV_READ & event) {
        struct evbuffer *input = bufferevent_get_input(bev);
        if (input) {
            size_t len = evbuffer_get_length(input);
            if (len) {
                char *data = (char *)malloc(len + 1);
                if (data) {
                    data[len] = 0;
                    if (-1 != evbuffer_copyout(input, data, len)) {
                        xmlDocPtr doc;
                        doc = xmlParseDoc(data);
                        if (doc) {
                            evbuffer_drain(input, len);
                            xmlNodePtr rootElement = xmlDocGetRootElement(doc);
                            if (!rootElement) {
                                xmlFreeDoc(doc);
                                free(data);
                                return;
                            }
                        }
                    }
                    /* parse event message */
                    free(data);
                }
            }
        }
    }
}

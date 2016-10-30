#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <string.h>

#include "connection.h"
#include "list.h"

struct tmclient {
	struct event_base *base;
    struct connection_cb cb;
    struct list_head servers;
	int port;
};

struct tmserver {
	struct tmclient *client;
	int status;
	const char *uuid;
    char local_addr[16];
	struct evhttp_uri *remote_uri;

	struct evhttp_uri *appserver_ctrl;
	struct evhttp_uri *appserver_event;
	int is_appserver_sub;

	struct evhttp_uri *profile_ctrl;
	struct evhttp_uri *profile_event;
	int is_profile_sub;
    struct list_head list;
};

static void tmclient_event_callback(struct evhttp_request *req, void *args);
static void tmclient_soap_callback(struct evhttp_request *req, void *args);

static xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name);
static int parse_device_desc(struct tmserver *server, xmlChar *data);

static struct tmserver *find_tmserver_by_uri(struct tmclient *client, const struct evhttp_uri *uri);

struct tmclient *tmclient_start(struct event_base *base, ev_uint16_t port, struct connection_cb cb)
{
	struct evhttp_bound_socket *handle = 0;
	struct sockaddr_storage ss;
	evutil_socket_t fd = 0;
	ev_socklen_t socklen = sizeof(ss);
	struct tmclient *client = (struct tmclient *)calloc(1, sizeof(*client));

	if (!client) {
		return 0;
	}
	client->base = base;
    client->cb = cb;
    INIT_LIST_HEAD(&(client->servers));
	struct evhttp *http = evhttp_new(base);
	if (!http) {
		free(client);
		return 0;
	}
	evhttp_set_gencb(http, tmclient_event_callback, client);
	if (0 == (handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port))) {
		evhttp_free(http);
		free(client);
		return 0;
	}
	fd = evhttp_bound_socket_get_fd(handle);
	if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
		evhttp_free(http);
		free(client);
		return 0;
	}
	if (ss.ss_family != AF_INET) {
		evhttp_free(http);
		free(client);
		return 0;
	}
	client->port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
	return client;
}

struct tmserver *tmclient_get_description(struct tmclient *client, const char *remote_uri)
{
    struct evhttp_request *req = 0;
    struct evhttp_uri *uri = evhttp_uri_parse(remote_uri);
    struct evhttp_connection *conn =
            evhttp_connection_base_new(client->base, 0,
            evhttp_uri_get_host(uri), evhttp_uri_get_port(uri));
    if (!conn) {
        evhttp_uri_free(uri);
        return NULL;
    }
    evhttp_connection_set_timeout(conn, -1);
    evhttp_connection_set_retries(conn, 5000);
    struct tmserver *server = (struct tmserver *)calloc(1, sizeof(*server));
    if (!server) {
        evhttp_uri_free(uri);
        evhttp_connection_free(conn);
        return NULL;
    }
    server->remote_uri = uri;
    list_add(&(server->list), &(client->servers));
    req = evhttp_request_new(tmclient_soap_callback, server);
    if (!req) {
        evhttp_connection_free(conn);
        free(server);
        return NULL;
    }
    if (-1 == evhttp_make_request(conn, req, EVHTTP_REQ_GET, evhttp_uri_get_path(uri))) {
        evhttp_request_free(req);
        evhttp_connection_free(conn);
        free(server);
        return NULL;
    }
    return server;
}

int tmclient_subscribe_service(struct tmclient *client, struct tmserver *server, int sid)
{
	if (server) {
		struct evhttp_request *req = 0;
		struct evhttp_connection *conn = 0;
		struct evhttp_uri *event_uri = 0;
		char path_buf[256] = {0};
		if (!server->status) {
			return -1;
		}

		if (0 == sid) {
			event_uri = server->appserver_event;
		}
		else if (1 == sid) {
			event_uri = server->profile_event;
		}
		else {
			//
		}

		if (!event_uri) {
			return -1;
		}
		conn = evhttp_connection_base_new(client->base, 0,
				evhttp_uri_get_host(event_uri), evhttp_uri_get_port(event_uri));
		if (!conn) {
			return -1;
		}
		evhttp_connection_set_timeout(conn, -1);
		evhttp_connection_set_retries(conn, 5000);
		req = evhttp_request_new(tmclient_soap_callback, server);
		if (!req) {
			evhttp_connection_free(conn);
			return -1;
		}

		sprintf(path_buf, "%s: %d", evhttp_uri_get_host(event_uri), evhttp_uri_get_port(event_uri));
		evhttp_add_header(evhttp_request_get_output_headers(req), "HOST", path_buf);

		memset(path_buf, 0, 256);
		sprintf(path_buf, "%s: %d", server->local_addr, client->port);
		evhttp_add_header(evhttp_request_get_output_headers(req), "CALLBACK", path_buf);
		evhttp_add_header(evhttp_request_get_output_headers(req), "NT", "upnp:event");

		memset(path_buf, 0, 256);
		strcpy(path_buf, evhttp_uri_get_path(event_uri));
		if (0 != evhttp_uri_get_query(event_uri)) {
			strcat(path_buf, "?");
			strcat(path_buf, evhttp_uri_get_query(event_uri));
		}

		if (-1 == evhttp_make_request(conn, req, EVHTTP_REQ_SUB, path_buf)) {
			evhttp_request_free(req);
			evhttp_connection_free(conn);
			return -1;
		}
		return 0;
	}
	return -1;
}

static void tmclient_event_callback(struct evhttp_request *req, void *args)
{
	struct tmclient *client = (struct tmclient *)args;
	struct tmserver *server = 0;
	struct evkeyvalq *headers = 0;
	struct evkeyval *header = 0;
	int checked = 0;
	struct evbuffer *buf = 0;
	ssize_t buflen = 0;
    const struct evhttp_uri *remote_uri = 0;
    const struct evhttp_uri *decoded = evhttp_request_get_evhttp_uri(req);

	if (EVHTTP_REQ_NOTIFY != evhttp_request_get_command(req)) {
		//
		return;
	}

	headers = evhttp_request_get_input_headers(req);
	for (header = headers->tqh_first; header;
	    header = header->next.tqe_next) {
		if ((0 == strcmp("NTS", header->key))
			&& (0 == strcmp("upnp:propchange", header->value))) {
			checked = 1;
			break;
		}
	}

	if (!checked) {
		//
		return;
	}

	remote_uri = evhttp_request_get_evhttp_uri(req);
    server = find_tmserver_by_uri(client, remote_uri);
	if (server) {
        if (!strcmp(evhttp_uri_get_path(server->appserver_event),
			evhttp_uri_get_path(decoded))) {
			if (!server->is_appserver_sub) {
				return;
			}
		}
        else if (!strcmp(evhttp_uri_get_path(server->profile_event),
			evhttp_uri_get_path(decoded))) {
			if (!server->is_profile_sub) {
				return;
			}
		}
		else {
			return;
		}
		
		buf = evhttp_request_get_input_buffer(req);
		buflen = evbuffer_get_length(buf);
		if (buflen) {
			char *data = (char *)malloc(buflen + 1);
			if (data) {
				data[buflen] = 0;
				evbuffer_remove(buf, data, buflen);
				/* parse event message */
				free(data);
			}
		}

		evhttp_send_reply(req, 200, "OK", NULL);
	}
}

static void tmclient_soap_callback(struct evhttp_request *req, void *args)
{
	switch (evhttp_request_get_command(req)) {
		case EVHTTP_REQ_GET:
		{
			struct tmserver *server = (struct tmserver *)args;
			if (0 == strcmp(evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req)),
				evhttp_uri_get_path(server->remote_uri))) {
				if (200 == evhttp_request_get_response_code(req)) {
					struct sockaddr_storage ss;
                    struct evbuffer *buf;
                    size_t buflen;
					ev_socklen_t socklen = sizeof(ss);
					evutil_socket_t fd = 
						bufferevent_getfd(
							evhttp_connection_get_bufferevent(
								evhttp_request_get_connection(req)));
					if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
                        return;
					}
					if (ss.ss_family != AF_INET) {
                        return;
					}
                    if (!evutil_inet_ntop(ss.ss_family,
                        &((struct sockaddr_in*)&ss)->sin_addr, server->local_addr, 16)) {
                        return;
                    }
                    buf = evhttp_request_get_input_buffer(req);
                    buflen = evbuffer_get_length(buf);
                    if (buflen) {
                        char *data = (char *)malloc(buflen + 1);
                        if (data) {
                            data[buflen] = 0;
                            evbuffer_remove(buf, data, buflen);
                            if (!parse_device_desc(server, BAD_CAST data)) {
                                server->status = 1;
                            }
                            free(data);
                        }
                    }
				}
			}
		}
		break;
		case EVHTTP_REQ_POST:
		break;
		case EVHTTP_REQ_SUB:
		{
			if (200 == evhttp_request_get_response_code(req)) {
				struct tmserver *server = (struct tmserver *)args;
				if (0 == strcmp(evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req)),
					evhttp_uri_get_path(server->appserver_event))) {
					server->is_appserver_sub = 1;
				}
				else if (0 == strcmp(evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req)),
					evhttp_uri_get_path(server->profile_event))) {
					server->is_profile_sub = 1;
				}
			}
		}
		break;
		default:
		break;
	}
}

static xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name)
{
    xmlNodePtr cur = xmlFirstElementChild(parent);
    for (; cur; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) {
            continue;
        }
        if (!xmlStrcmp(cur->name, name)) {
            return cur;
        }
    }
    return NULL;
}

static int parse_device_desc(struct tmserver *server, xmlChar *data)
{
    xmlDocPtr doc;
    doc = xmlParseDoc(data);
    if (!doc) {
        return -1;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -1;
    }
    xmlNodePtr deviceElement = xmlFindChildElement(rootElement, BAD_CAST"device");
    if (deviceElement) {
        xmlNodePtr deviceTypeElement = xmlFindChildElement(deviceElement, BAD_CAST"deviceType");
        if (deviceTypeElement) {
            xmlChar *deviceType = xmlNodeGetContent(deviceTypeElement->children);
            if (xmlStrcmp(deviceType, BAD_CAST"urn:schemas-upnp-org:device:TmServerDevice:1")) {
                xmlFree(deviceType);
                xmlFreeNode(rootElement);
                xmlFreeDoc(doc);
                return -1;
            }
        }
        xmlNodePtr udnElement = xmlFindChildElement(deviceElement, BAD_CAST"UDN");
        if (udnElement) {
            xmlChar *udn = xmlNodeGetContent(udnElement->children);
            server->uuid = (const char *)xmlStrdup(udn);
            xmlFree(udn);
        }
        xmlNodePtr serviceListElement = xmlFindChildElement(deviceElement, BAD_CAST"serviceList");
        if (serviceListElement) {
            xmlNodePtr serviceElement = NULL;
            for (serviceElement = xmlFirstElementChild(serviceListElement);
                 serviceElement; serviceElement = serviceElement->next) {
                xmlNodePtr serviceTypeElement = xmlFindChildElement(serviceElement, BAD_CAST"serviceType");
                xmlNodePtr controlURLElement = xmlFindChildElement(serviceElement, BAD_CAST"controlURL");
                xmlNodePtr eventSubURLElement = xmlFindChildElement(serviceElement, BAD_CAST"eventSubURL");
                if (serviceTypeElement) {
                    xmlChar *controlURL = NULL;
                    xmlChar *eventSubURL = NULL;
                    xmlChar *serviceType = xmlNodeGetContent(serviceTypeElement->children);
                    if (!xmlStrcmp(serviceType, BAD_CAST"urn:schemas-upnp-org:service:TmApplicationServer:1")) {
                        if (controlURLElement) {
                            controlURL = xmlNodeGetContent(controlURLElement->children);
                        }
                        if (eventSubURLElement) {
                            eventSubURL = xmlNodeGetContent(controlURLElement->children);
                        }
                        server->appserver_ctrl = evhttp_uri_parse((const char *)controlURL);
                        server->appserver_event = evhttp_uri_parse((const char *)eventSubURL);
                    }
                    else if (!xmlStrcmp(serviceType, BAD_CAST"urn:schemas-upnp-org:service:TmClientProfile:1")) {
                        if (controlURLElement) {
                            controlURL = xmlNodeGetContent((controlURLElement->children));
                        }
                        if (eventSubURLElement) {
                            eventSubURL = xmlNodeGetContent((controlURLElement->children));
                        }
                        server->profile_ctrl = evhttp_uri_parse((const char *)controlURL);
                        server->profile_event = evhttp_uri_parse((const char *)eventSubURL);
                    }
                    xmlFree(serviceType);
                    xmlFree(eventSubURL);
                    xmlFree(controlURL);
                }
            }
        }
    }
    xmlFreeNode(rootElement);
    xmlFreeDoc(doc);
    return 0;
}

static struct tmserver *find_tmserver_by_uri(struct tmclient *client, const struct evhttp_uri *uri)
{
    struct tmserver *server;
    list_for_each_entry(server, &(client->servers), list) {
        if (strcmp(evhttp_uri_get_host(uri),
                   evhttp_uri_get_host(server->remote_uri))) {
            continue;
        }
        if (strcmp(evhttp_uri_get_path(uri),
                   evhttp_uri_get_path(server->remote_uri))) {
            continue;
        }
        if (evhttp_uri_get_port(uri) ==
                evhttp_uri_get_port(server->remote_uri)) {
            return server;
        }
    }
    return NULL;
}

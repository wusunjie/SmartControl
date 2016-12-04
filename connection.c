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
    unsigned char *uuid;
    char local_addr[16];
	struct evhttp_uri *remote_uri;

	struct evhttp_uri *appserver_ctrl;
	struct evhttp_uri *appserver_event;
	int is_appserver_sub;
    char *appserver_sid;

	struct evhttp_uri *profile_ctrl;
	struct evhttp_uri *profile_event;
	int is_profile_sub;
    char *profile_sid;
    struct list_head list;
};

static void tmclient_event_callback(struct evhttp_request *req, void *args);
static void tmclient_soap_callback(struct evhttp_request *req, void *args);

static xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name);
static int parse_device_desc(struct tmserver *server, xmlChar *data);
static int parse_action_response(struct tmserver *server, xmlChar *data, int errcode);
static struct app *parse_application_list(xmlNodePtr appListing, unsigned int *count);

static struct tmserver *check_event(struct tmclient *client, struct evhttp_request *req);
static int get_localaddr(char *local, const char *remote);
static int get_localport(int fd, int *port);
static int tmclient_send_soap_action(struct tmclient *client, struct tmserver *server, int sid, const char *action, const char *body);

struct tmclient *tmclient_start(struct event_base *base, ev_uint16_t port, struct connection_cb cb)
{
	struct evhttp_bound_socket *handle = 0;
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
    if (get_localport(evhttp_bound_socket_get_fd(handle), &(client->port))) {
        evhttp_free(http);
        free(client);
        return 0;
    }
	return client;
}

void tmclient_stop(struct tmclient *client)
{
    struct tmserver *server;
    struct tmserver *tmp;
    if (client) {
        event_base_loopexit(client->base, NULL);
        list_for_each_entry_safe(server, tmp, &(client->servers), list) {
            evhttp_uri_free(server->remote_uri);
            evhttp_uri_free(server->appserver_ctrl);
            evhttp_uri_free(server->appserver_event);
            evhttp_uri_free(server->profile_ctrl);
            evhttp_uri_free(server->profile_event);
            free(server->uuid);
            free(server->appserver_sid);
            free(server->profile_sid);
            list_del(&(server->list));
            free(server);
        }
        free(client);
    }
}


struct tmserver *get_description(struct tmclient *client, const char *remote_uri)
{
    char path_buf[256] = {0};
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
    server->client = client;
    get_localaddr(server->local_addr, evhttp_uri_get_host(uri));
    list_add(&(server->list), &(client->servers));
    req = evhttp_request_new(tmclient_soap_callback, server);
    if (!req) {
        evhttp_connection_free(conn);
        free(server);
        return NULL;
    }
    sprintf(path_buf, "%s:%d", evhttp_uri_get_host(uri), evhttp_uri_get_port(uri));
    evhttp_add_header(evhttp_request_get_output_headers(req), "HOST", path_buf);
    if (-1 == evhttp_make_request(conn, req, EVHTTP_REQ_GET, evhttp_uri_get_path(uri))) {
        evhttp_request_free(req);
        evhttp_connection_free(conn);
        free(server);
        return NULL;
    }
    return server;
}

int subscribe_service(struct tmclient *client, struct tmserver *server, int sid)
{
	if (server) {
		struct evhttp_request *req = 0;
		struct evhttp_connection *conn = 0;
		struct evhttp_uri *event_uri = 0;
		char path_buf[256] = {0};
        const char *host = 0;
        int port = -1;
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
        host = evhttp_uri_get_host(event_uri);
        port = evhttp_uri_get_port(event_uri);
        if ((NULL == host) || (0 == strlen(host))) {
            host = evhttp_uri_get_host(server->remote_uri);
        }
        if (-1 == port) {
            port = evhttp_uri_get_port(server->remote_uri);
        }
		conn = evhttp_connection_base_new(client->base, 0,
                host, port);
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

        sprintf(path_buf, "%s:%d", host, port);
		evhttp_add_header(evhttp_request_get_output_headers(req), "HOST", path_buf);

		memset(path_buf, 0, 256);
        sprintf(path_buf, "<http://%s:%d>", server->local_addr, client->port);
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

int get_application_list(struct tmclient *client, struct tmserver *server, int profile, const char *filter)
{
    char body[256] = {0};
    sprintf(body, "<AppListingFilter>%s</AppListingFilter><ProfileID>%d</ProfileID>", filter, profile);
    return tmclient_send_soap_action(client, server, 0, "GetApplicationList", body);
}

static int tmclient_send_soap_action(struct tmclient *client, struct tmserver *server, int sid, const char *action, const char *body)
{
    if (server) {
        struct evhttp_request *req = NULL;
        struct evhttp_connection *conn = NULL;
        struct evhttp_uri *ctrl_uri = NULL;
        const char *serviceType = NULL;
        char path_buf[65536] = {0};
        const char *host = 0;
        int port = -1;
        if (!server->status) {
            return -1;
        }

        if (0 == sid) {
            ctrl_uri = server->appserver_ctrl;
            serviceType = "urn:schemas-upnp-org:service:TmApplicationServer:1";
        }
        else if (1 == sid) {
            ctrl_uri = server->profile_ctrl;
            serviceType = "urn:schemas-upnp-org:service:TmClientProfile:1";
        }
        else {
            //
        }

        if (!ctrl_uri) {
            return -1;
        }
        host = evhttp_uri_get_host(ctrl_uri);
        port = evhttp_uri_get_port(ctrl_uri);
        if ((NULL == host) || (0 == strlen(host))) {
            host = evhttp_uri_get_host(server->remote_uri);
        }
        if (-1 == port) {
            port = evhttp_uri_get_port(server->remote_uri);
        }
        conn = evhttp_connection_base_new(client->base, 0,
                host, port);
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

        sprintf(path_buf, "%s:%d", host, port);
        evhttp_add_header(evhttp_request_get_output_headers(req), "HOST", path_buf);

        memset(path_buf, 0, 65536);
        sprintf(path_buf, "\"%s#%s\"", serviceType, action);
        evhttp_add_header(evhttp_request_get_output_headers(req), "CONTENT-TYPE", "text/xml; charset=\"utf-8\"");
        evhttp_add_header(evhttp_request_get_output_headers(req), "SOAPACTION", path_buf);

        memset(path_buf, 0, 65536);
        strcpy(path_buf, evhttp_uri_get_path(ctrl_uri));
        if (0 != evhttp_uri_get_query(ctrl_uri)) {
            strcat(path_buf, "?");
            strcat(path_buf, evhttp_uri_get_query(ctrl_uri));
        }

        memset(path_buf, 0, 65536);
        sprintf(path_buf, "<?xml version=\"1.0\"?>"
                "<s:Envelope "
                "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                "<s:Body>"
                "<u:%s xmlns:u=\"%s\">"
                "%s"
                "</u:%s>"
                "</s:Body>"
                "</s:Envelope>", action, serviceType, body, action);

        evbuffer_add(evhttp_request_get_output_buffer(req), path_buf, strlen(path_buf));

        if (-1 == evhttp_make_request(conn, req, EVHTTP_REQ_POST, evhttp_uri_get_path(ctrl_uri))) {
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
	struct evbuffer *buf = 0;
	ssize_t buflen = 0;

    server = check_event(client, req);

    if (server) {
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
            if (!server) {
                return;
            }
            if (!strcmp(evhttp_request_get_uri(req),
                evhttp_uri_get_path(server->remote_uri))) {
				if (200 == evhttp_request_get_response_code(req)) {
                    struct evbuffer *buf;
                    size_t buflen;
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
                if (server->client && server->client->cb.server_added) {
                    server->client->cb.server_added(server->client, server);
                }
            }
		}
		break;
		case EVHTTP_REQ_POST:
        {
            struct tmserver *server = (struct tmserver *)args;
            if (!server) {
                return;
            }
            struct evbuffer *buf;
            size_t buflen;
            buf = evhttp_request_get_input_buffer(req);
            buflen = evbuffer_get_length(buf);
            if (buflen) {
                char *data = (char *)malloc(buflen + 1);
                if (data) {
                    data[buflen] = 0;
                    evbuffer_remove(buf, data, buflen);
                    parse_action_response(server, BAD_CAST data, evhttp_request_get_response_code(req));
                    free(data);
                }
            }
        }
		break;
		case EVHTTP_REQ_SUB:
		{
			if (200 == evhttp_request_get_response_code(req)) {
				struct tmserver *server = (struct tmserver *)args;
                if (!strcmp(evhttp_request_get_uri(req),
					evhttp_uri_get_path(server->appserver_event))) {
                    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
                    struct evkeyval *header;
                    for (header = headers->tqh_first; header;
                        header = header->next.tqe_next) {
                        if (0 == strcasecmp("SID", header->key)) {
                            server->appserver_sid = strdup(header->value);
                            server->is_appserver_sub = 1;
                            break;
                        }
                    }
				}
                else if (!strcmp(evhttp_request_get_uri(req),
					evhttp_uri_get_path(server->profile_event))) {
                    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
                    struct evkeyval *header;
                    for (header = headers->tqh_first; header;
                        header = header->next.tqe_next) {
                        if (0 == strcasecmp("SID", header->key)) {
                            server->profile_sid = strdup(header->value);
                            server->is_profile_sub = 1;
                            break;
                        }
                    }
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
                xmlFreeDoc(doc);
                return -1;
            }
        }
        xmlNodePtr udnElement = xmlFindChildElement(deviceElement, BAD_CAST"UDN");
        if (udnElement) {
            xmlChar *udn = xmlNodeGetContent(udnElement->children);
            server->uuid = xmlStrdup(udn);
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
                            controlURL = xmlNodeGetContent(controlURLElement);
                        }
                        if (eventSubURLElement) {
                            eventSubURL = xmlNodeGetContent(eventSubURLElement);
                        }
                        server->appserver_ctrl = evhttp_uri_parse((const char *)controlURL);
                        server->appserver_event = evhttp_uri_parse((const char *)eventSubURL);
                    }
                    else if (!xmlStrcmp(serviceType, BAD_CAST"urn:schemas-upnp-org:service:TmClientProfile:1")) {
                        if (controlURLElement) {
                            controlURL = xmlNodeGetContent(controlURLElement);
                        }
                        if (eventSubURLElement) {
                            eventSubURL = xmlNodeGetContent(eventSubURLElement);
                        }
                        server->profile_ctrl = evhttp_uri_parse((const char *)controlURL);
                        server->profile_event = evhttp_uri_parse((const char *)eventSubURL);
                    }
                    if (serviceType) {
                        xmlFree(serviceType);
                    }
                    if (eventSubURL) {
                        xmlFree(eventSubURL);
                    }
                    if (controlURL) {
                        xmlFree(controlURL);
                    }
                }
            }
        }
    }
    xmlFreeDoc(doc);
    return 0;
}

static struct app *parse_application_list(xmlNodePtr appListing, unsigned int *count)
{
    xmlDocPtr doc;
    xmlChar *document = xmlNodeGetContent(appListing);
    unsigned int cnt = 0;
    struct app *appList = NULL;
    doc = xmlParseDoc(document);
    if (!doc) {
        return NULL;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return NULL;
    }
    for (xmlNodePtr cur = xmlFirstElementChild(rootElement); cur; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) {
            continue;
        }
        if (!xmlStrcmp(cur->name, BAD_CAST"app")) {
            xmlNodePtr appElement = cur;
            if (appElement) {
                appList = (struct app *)realloc(appList, (cnt + 1) * sizeof(*appList));
                xmlNodePtr appIDElement = xmlFindChildElement(appElement, BAD_CAST"appID");
                if (appIDElement) {
                    xmlChar *appID = xmlNodeGetContent(appIDElement->children);
                    appList[cnt].appID = (const char *)xmlStrdup(appID);
                    xmlFree(appID);
                }
                xmlNodePtr nameElement = xmlFindChildElement(appElement, BAD_CAST"name");
                if (nameElement) {
                    xmlChar *name = xmlNodeGetContent(nameElement->children);
                    appList[cnt].name = (const char *)xmlStrdup(name);
                    xmlFree(name);
                }
                xmlNodePtr remotingInfoElement = xmlFindChildElement(appElement, BAD_CAST"remotingInfo");
                if (remotingInfoElement) {
                    xmlNodePtr protocolIDElement = xmlFindChildElement(remotingInfoElement, BAD_CAST"protocolID");
                    if (protocolIDElement) {
                        xmlChar *protocolID = xmlNodeGetContent(protocolIDElement->children);
                        if (!xmlStrcmp(protocolID, BAD_CAST"VNC")) {
                            appList[cnt].remoteinfo.protocolID = PROTOCOL_VNC;
                        }
                        else if (!xmlStrcmp(protocolID, BAD_CAST"DAP")) {
                            appList[cnt].remoteinfo.protocolID = PROTOCOL_DAP;
                        }
                        else if (!xmlStrcmp(protocolID, BAD_CAST"WFD")) {
                            appList[cnt].remoteinfo.protocolID = PROTOCOL_WFD;
                        }
                        else {
                            appList[cnt].remoteinfo.protocolID = PROTOCOL_NONE;
                        }
                        xmlFree(protocolID);
                    }
                }
                cnt++;
            }
        }
    }
    *count = cnt;
    xmlFree(document);
    xmlFreeDoc(doc);
    return appList;
}

static int parse_action_response(struct tmserver *server, xmlChar *data, int errcode)
{
    switch (errcode) {
    case 200:
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
        xmlNodePtr bodyElement = xmlFindChildElement(rootElement, BAD_CAST"Body");
        if (bodyElement) {
            xmlNodePtr actionRspElement = xmlFirstElementChild(bodyElement);
            if (actionRspElement) {
                xmlChar *actionName = (xmlChar *)xmlStrstr(actionRspElement->name, BAD_CAST"Response");
                if (actionName) {
                    actionName = xmlStrsub(actionRspElement->name, 0, actionName - actionRspElement->name);
                }
                else {
                    xmlFreeDoc(doc);
                    return -1;
                }
                if (actionRspElement->ns) {
                    const xmlChar *serviceType = actionRspElement->ns->href;
                    if (!xmlStrcmp(serviceType, BAD_CAST"urn:schemas-upnp-org:service:TmApplicationServer:1")) {
                        if (!xmlStrcmp(actionName, BAD_CAST"GetApplicationList")) {
                            unsigned int count = 0;
                            struct app *appList = parse_application_list(xmlFirstElementChild(actionRspElement), &count);
                            if (appList) {
                                if (server->client && server->client->cb.applist_update) {
                                    server->client->cb.applist_update(server->client, server, appList, count);
                                }
                                free(appList);
                            }
                            else {
                                xmlFree(actionName);
                                xmlFreeDoc(doc);
                                return -1;
                            }
                        }
                        else if (!xmlStrcmp(actionName, BAD_CAST"LaunchApplication")) {

                        }
                    }
                    else if (!xmlStrcmp(serviceType, BAD_CAST"urn:schemas-upnp-org:service:TmClientProfile:1")) {

                    }
                }
                xmlFree(actionName);
            }
        }
        xmlFreeDoc(doc);
    }
    }
    return 0;
}

static struct tmserver *check_event(struct tmclient *client, struct evhttp_request *req)
{
    struct tmserver *server;
    struct evkeyvalq *headers = 0;
    struct evkeyval *header = 0;
    int is_event = 0;
    char *sid = 0;
    if (EVHTTP_REQ_NOTIFY != evhttp_request_get_command(req)) {
        return NULL;
    }

    headers = evhttp_request_get_input_headers(req);
    if (!headers) {
        return NULL;
    }

    for (header = headers->tqh_first; header;
        header = header->next.tqe_next) {
        if ((0 == strcasecmp("NTS", header->key))
            && (0 == strcasecmp("upnp:propchange", header->value))) {
            is_event = 1;
        }
        if (0 == strcasecmp("SID", header->key)) {
            sid = header->value;
        }
    }

    if (!is_event || !sid) {
        return NULL;
    }

    list_for_each_entry(server, &(client->servers), list) {
        if (server->appserver_sid) {
            if (!strcmp(sid, server->appserver_sid)) {
                if (server->is_appserver_sub) {
                    return server;
                }
                else {
                    return NULL;
                }
            }
        }
        if (server->profile_sid) {
            if (!strcmp(sid, server->profile_sid)) {
                if (server->is_profile_sub) {
                    return server;
                }
                else {
                    return NULL;
                }
            }
        }
    }
    return NULL;
}

#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static int get_localaddr(char *local, const char *remote)
{
    struct ifaddrs *ifaddr, *ifa;
    struct in_addr addr;
    memset(&addr, 0, sizeof(addr));
    if (!inet_aton(remote, &addr)) {
        return -1;
    }
    if (-1 == getifaddrs(&ifaddr)) {
        return -1;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (NULL == ifa->ifa_addr) {
            continue;
        }
        if (AF_INET != ifa->ifa_addr->sa_family) {
            continue;
        }
        if (!ifa->ifa_netmask) {
            continue;
        }
        if (((((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr) & (((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr.s_addr)) ==
        (addr.s_addr & (((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr.s_addr))) {
            strcpy(local, inet_ntoa(((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr));
            freeifaddrs(ifaddr);
            return 0;
        }
    }
    freeifaddrs(ifaddr);
    return -1;
}

static int get_localport(int fd, int *port)
{
    struct sockaddr_storage ss;
    ev_socklen_t socklen = sizeof(ss);

    if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
        return -1;
    }
    if (ss.ss_family != AF_INET) {
        return -1;
    }
    *port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
    return 0;
}

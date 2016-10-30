#ifndef CONNECTION_H
#define CONNECTION_H

struct tmclient;
struct tmserver;

struct connection_cb {
    void (*server_added)(struct tmclient *client, struct tmserver *server);
    void (*server_removed)(struct tmclient *client, struct tmserver *server);
};

extern struct tmclient *tmclient_start(struct event_base *base, ev_uint16_t port, struct connection_cb cb);
extern void tmclient_stop(struct tmclient *client);

extern struct tmserver *tmclient_get_description(struct tmclient *client, const char *remote_uri);
extern int tmclient_subscribe_service(struct tmclient *client, struct tmserver *server, int sid);

extern void disconnect_server(struct tmclient *client, struct tmserver *server);

#endif // CONNECTION_H


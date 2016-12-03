#include <stdio.h>

#include <event2/event.h>
#include "connection.h"

static void test_server_added(struct tmclient *client, struct tmserver *server);
static void test_server_removed(struct tmclient *client, struct tmserver *server);

int main(void)
{
    struct connection_cb cb = {
        test_server_added,
        test_server_removed
    };
    struct event_base *base = event_base_new();
    struct tmclient *client = tmclient_start(base, 1900, cb);
    get_description(client, "http://192.168.42.129:44364/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
    return event_base_loop(base, 0);
}

static void test_server_added(struct tmclient *client, struct tmserver *server)
{
    subscribe_service(client, server, 1);
}

static void test_server_removed(struct tmclient *client, struct tmserver *server)
{
    (void)client;
    (void)server;
}

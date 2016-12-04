#include <event2/event.h>

#include <string.h>
#include <stdio.h>

#include "conn.h"

static void test_server_added(struct tmclient *client, struct tmserver *server);
static void test_server_removed(struct tmclient *client, struct tmserver *server);
static void test_applist_update(struct tmclient *client, struct tmserver *server, struct app *apps, unsigned int count);

int main(void)
{
    struct connection_cb cb = {
        test_server_added,
        test_server_removed,
        test_applist_update
    };
    struct event_base *base = event_base_new();
    struct tmclient *client = tmclient_start(base, 1900, cb);
    get_description(client, "http://192.168.42.129:60237/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
    return event_base_loop(base, 0);
}

static void test_server_added(struct tmclient *client, struct tmserver *server)
{
    get_application_list(client, server, 0, "remotingInfo@protocolID=\"DAP\",appInfo@appCategory=\"0xF0000001\"");
}

static void test_server_removed(struct tmclient *client, struct tmserver *server)
{
    (void)client;
    (void)server;
}

static void test_applist_update(struct tmclient *client, struct tmserver *server, struct app *apps, unsigned int count)
{
    if (count > 0) {
        unsigned int i;
        unsigned int cur = 0;
        unsigned int ver = 0;
        for (i = 0; i < count; i++) {
            unsigned int min = 2;
            if (!strlen(apps[i].remoteinfo.format)) {
                sscanf(apps[i].remoteinfo.format, "%*d.%d", &min);
            }
            if (min > ver) {
                ver = min;
                cur = i;
            }
        }
        launch_application(client, server, 0, apps[cur].appID);
    }
}

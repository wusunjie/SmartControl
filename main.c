#include <event2/event.h>

#include <string.h>
#include <stdio.h>

#include "conn.h"
#include "dapclient.h"

struct event_base *global_base = NULL;

static void test_server_added(struct tmclient *client, struct tmserver *server);
static void test_server_removed(struct tmclient *client, struct tmserver *server);
static void test_applist_update(struct tmclient *client, struct tmserver *server, struct app *apps, unsigned int count);
static void test_application_launched(struct tmclient *client, struct tmserver *server, struct evhttp_uri *uri, int result);

int main(void)
{
    struct connection_cb cb = {
        test_server_added,
        test_server_removed,
        test_applist_update,
        test_application_launched
    };
    global_base = event_base_new();
    struct tmclient *client = tmclient_start(global_base, 1900, cb);
    get_description(client, "http://192.168.42.129:50929/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
    return event_base_loop(global_base, 0);
//    #include "sigverify.h"
//    const unsigned char *pubkey = (const unsigned char *)"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsglEk+HADXZ8LrvK2eeNKQIrRs/drYD9AQ/Aw1SVoCVqh3wqW8Lkv6GIqG36v+B2cCunrfIg1YYrxBPK38nL9UBY/uMQpea4/kGaHN4T0qxuksfLtUJFagpX7/1s44zbN3zjCkLsvSqIqmRS4tvO3JTUozuN4xKM9YtNjhBHfRNWrb4Tun/7cJJ6na/Hd91QYScvwHlhYG4ZcJmR7rE1DjNZTvWawiOzy1QHSzNBjREpkY0QJ9Yas4AD2bu1AX+IUhbK+bm8K2EeYuZDEzRRh4gWgrh7KZTHmKC72K16O489wdWuF2hL8s8Vas9977qD4ZxPlUUDMXyWgr4zXvoboQIDAQAB";
//    const unsigned char *AppList = (const unsigned char *)"<appList xml:id=\"mlServerAppList\"><app><appID>0x80000000</appID><name>DAPServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>DAP</protocolID><direction>bi</direction></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><CanonicalizationMethod Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></CanonicalizationMethod><SignatureMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#rsa-sha1\"></SignatureMethod><Reference URI=\"#mlServerAppList\"><Transforms><Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"></Transform><Transform Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></Transform></Transforms><DigestMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#sha1\"></DigestMethod><DigestValue>8FLxBkpA/v0BVQsdqCMmHtcMfmQ=</DigestValue></Reference></SignedInfo><SignatureValue>Rh0tHOqja0bmNl6c+izDtFW+rooGDBlAvIOc5KTlq5XIzyHLknJCMiz1Y5JcC3M9EN6Cv1vjUBlTADt+CkQeg07rkdusgUrrjkOx4TksPWZtnIgDP24YzqbU0QsiFn/Xvlc5bv6f5bsFU8FQobvX3YLJM/8rvQ6vEUZbvLRMIOyoYiz9y015UmzMAgTUydqxep4GfdpkfAHokkJb/Xc+sFCw8F81zJ7JL+ijB89vbdJUf8eV0Cw0PPC/vp7QKjiiz2j9Jyg7bmaw1UZob1M/GRjrpi6LfE75nTpaih1dehD4IcFTltqt8eivEma+3X3eiQSSxOSeL+6F6KO81oJDUQ==</SignatureValue></Signature></appList>";
//    return sigverify(AppList, pubkey);
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

static void test_application_launched(struct tmclient *client, struct tmserver *server, struct evhttp_uri *uri, int result)
{
    (void)client;
    (void)server;
    (void)result;
    dapclient_request(global_base, uri);
}

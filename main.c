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
//    struct connection_cb cb = {
//        test_server_added,
//        test_server_removed,
//        test_applist_update,
//        test_application_launched
//    };
//    global_base = event_base_new();
//    struct tmclient *client = tmclient_start(global_base, 1900, cb);
//    get_description(client, "http://192.168.42.129:50929/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
//    return event_base_loop(global_base, 0);
    #include "sigverify.h"
    const unsigned char *pubkey = (const unsigned char *)"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAl9IePwV2Wj8MRJzzTgRopP0Vl9bZNitgDRdDynXZkXlf+pUAB6dBgOTVhfEmCW3IHULNZbU1yP1RmMcYJPZIYn7oRgjRYji7xZEAjatUNFsiU6J6XaztRU4RYuIXTSxrWLuPezUq4GVzsr2ldwit7cZ2NJf6c90uMelmbb9suciQ7sIJQkY3OAIQ5RTYYay4XT3AIogEjmhcruFWkqkTLDsotbBpSM0Z0TrvcdiEbaL4BY9BhrwBH1jiiCcon9Oc4EgGZ/cv/cJLgluPK4wwb7Vnh82THIIvlDGGxkacOmtC5RYH3Iv7tiN6i+GwO0gSCauscqJU/6VPfuTyN8aLVwIDAQAB";
    const unsigned char *AppList = (const unsigned char *)"<appList>"
            "<app><appID>0x0000002d</appID><name>DAP Server</name><iconList><icon><mimetype>image/png</mimetype><width>128</width><height>128</height><depth>32</depth><url>/0x0000002d.png</url></icon></iconList><remotingInfo><protocolID>DAP</protocolID></remotingInfo><appInfo><appCategory>0xf0000001</appCategory></appInfo></app>"
            "<Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\">"
            "<SignedInfo>"
            "<CanonicalizationMethod Algorithm=\"http://www.w3.org/TR/2001/REC-xml-c14n-20010315\"/>"
            "<SignatureMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#rsa-sha1\"/>"
            "<Reference URI=\"\">"
            "<Transforms>"
            "<Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"/>"
            "</Transforms>"
            "<DigestMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#sha1\"/>"
            "<DigestValue>otCkjiWfv1gulTJ9xKE5anRLUlk=</DigestValue>"
            "</Reference>"
            "</SignedInfo>"
            "<SignatureValue>Yt8/H9FIZzFjbZIOzOrPlz2v77QpUfWd/Xx/fNEntBhb3ueJqA8rrnCPtofPWqYo"
            "y32453GMS3SAI0bePHdbvsQ2BOyoV8R3hDoL79O9uupGYRrfhcINxvMxUz06xVUY"
            "pXrfFmLBgjsvAA8uBI4192xaNPxxA1Do8AHLPfxlpn7RPyWbOvEoPp5mpRj138BE"
            "HjHbZri7eCocdCE2zrTWTgclqW5Ts5tOEE9mp+mkTFPws8v5M6AVd36oXSMMG/HZ"
            "texZtCqt6m097XC4/7ent0hz7HZZbuDfF/OqcaGu5a6pOo0ERtRe5QNtrqgsQdzi"
            "wwxVMfvXEbpkgzCxnmPw9w==</SignatureValue>"
            "<KeyInfo><KeyName/></KeyInfo>"
            "</Signature>"
            "</appList>";
    return sigverify(AppList, pubkey);
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

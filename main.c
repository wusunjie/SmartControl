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
//    get_description(client, "http://192.168.42.129:49172/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
//    return event_base_loop(global_base, 0);
    #include "sigverify.h"
    const unsigned char *pubkey = (const unsigned char *)"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAriHX+0Dbpp3i9ZdVShERdb/iNCI7oy0TIi7aNo+9dO8ecIUxupTAiLeofR33XVf66tUjbMjNBCQN+EmkHWlwTG4cEV/sHQ8Q+ogPuyOOQUezNpB3sGqFQ/BdmBrmE/8hLdxGnEMbwPg5YS+20hIMWkDQycEpHvVfprHo5vbAnRwdZnPjCVY/seOKI/VvB76Lun7vLpz7l8TLFu5nrJbGAiOrqylE12fZ/ZDbQ2kR8bFlsEaAMbVVdGHC2Lc41QgsRGITNw349Bb1rj6PipoKQFU9aBkXWDNacTX4cpnUKPfm9mOBohLhAdE/wuRAlFbFy5HB+vG5wEnlsYBQAPHrDQIDAQAB";
    const unsigned char *AppList = (const unsigned char *)"<appList xml:id=\"mlServerAppList\"><app><appID>0x80000000</appID><name>DAPServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>DAP</protocolID><direction>bi</direction></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><CanonicalizationMethod Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></CanonicalizationMethod><SignatureMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#rsa-sha1\"></SignatureMethod><Reference URI=\"#mlServerAppList\"><Transforms><Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"></Transform><Transform Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></Transform></Transforms><DigestMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#sha1\"></DigestMethod><DigestValue>8FLxBkpA/v0BVQsdqCMmHtcMfmQ=</DigestValue></Reference></SignedInfo><SignatureValue>DFlNK19IIyxrSQ+cH+XYDIQmEbKmcPzcylm5s2EPUPNkpOpT3wg0Y7fpxbmhIniTSkF+2Ce6rTpKBREXdzBBiHyxnYtXJQdocARiapLyASeBWpf9UUCklqPSWcqWlhbGEa9IGQB1eBXO45dz2F81NoFoFpplhsgG0ebwqv5TjzjwfVi4EEKFfgKrorNbNQ+vujqiDiGy4UD0eNBObLOTyTIfLCiACVKwAX6PEkmknoXjrjMcsuWVaZUwlKzYWZrWFsSXbQfryZNFXfMuZnDWE54ms9W6H6C1vUxtJehdphQ6BS9rxRWUI4s1DmAlEPe8XEfMuKu96wbzJU5+3Tqj4w==</SignatureValue></Signature></appList>";
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

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

int main(int argc, char *argv[])
{
    //    struct connection_cb cb = {
    //        test_server_added,
    //        test_server_removed,
    //        test_applist_update,
    //        test_application_launched
    //    };
    //    global_base = event_base_new();
    //    struct tmclient *client = tmclient_start(global_base, 1900, cb);
    //    get_description(client, "http://192.168.42.129:49810/upnp/dev/b7c06478-06db-208c-0000-0000741ab1e1/desc");
    //    return event_base_loop(global_base, 0);
#include "sigverify.h"
    const unsigned char *pubkey = (const unsigned char *)"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyoZPmfDcPxMcoKSjlqMcaFMh7IcMbRLP7Pbz9lkLHdAG4iSeKKZmW4/NCR6hH/72GmEMYyYzVaR0uPrY02xpcaZDxQ2gcz+2ZnhrxRjqgka1qo3XsENqUVVESjYYNqeOcblLCrvGquDIR5+DkCcZr6LZKhqU3W3WAw3gLy2XZ3Hg9EUEAkgFQPuVosOusWUApHnl9+3bE2j3LKD3u1P3aC3/Ytga3xhJQhZz43uh0NZDSWACvxyjKc8BW5ixLpVt+6AiNVBuUfNZQ7icBJ2MNu0f5NJBNbh8jxZBse+u4vzX/Zi44xc78E5Du/CmWnac+uN1y4sOW7fFHRJARx0EMwIDAQAB";
    const unsigned char *AppList = (const unsigned char *)"<appList xml:id=\"mlServerAppList\"><app><appID>0x80000000</appID><name>DAPServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>DAP</protocolID><direction>bi</direction></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><app><appID>0x80000001</appID><name>VNCServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><app><appID>0x80000002</appID><name>RTPServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>RTP</protocolID><format>99</format><direction>out</direction></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo><audioInfo><audioType>application</audioType><contentCategory>0x2</contentCategory></audioInfo><resourceStatus>free</resourceStatus></app><app><appID>0x80000003</appID><name>RTP Client for Voice</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>RTP</protocolID><format>99</format><direction>in</direction><audioIPL>340</audioIPL><audioMPL>3600</audioMPL></remotingInfo><appInfo><appCategory>0xF0000002</appCategory></appInfo><audioInfo><audioType>phone</audioType><contentCategory>0x10</contentCategory></audioInfo><resourceStatus>free</resourceStatus></app><app><appID>0x80000004</appID><name>CDBServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>CDB</protocolID></remotingInfo><appInfo><appCategory>0xF0000000</appCategory></appInfo></app><app><appID>0x80000005</appID><name>HsmlServer</name><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>HTC-HSML</protocolID></remotingInfo><appInfo><appCategory>0xF0000001</appCategory></appInfo></app><app><appID>0x00000001</appID><name>RockScout</name><providerName>Car Connectivity Consortium</providerName><providerURL>http://www.mirrorlink.com</providerURL><description>RockScout</description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/D0020FF6.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/D0020FF6.cert</appCertificateURL><appInfo><appCategory>0x00030001</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10005</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>all</audioType><contentCategory>0x80000002</contentCategory></audioInfo></app><app><appID>0x00000002</appID><name>MirrorLink Test</name><providerName>Comarch</providerName><providerURL>http://comarch.com</providerURL><description>Comarch</description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/52272F7F.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/52272F7F.cert</appCertificateURL><appInfo><appCategory>0xFFFEFFFF</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x20</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>all</audioType><contentCategory>0x6</contentCategory></audioInfo></app><app><appID>0x00000003</appID><name>Base Cert Audio App</name><providerName>Ed Pichon</providerName><providerURL>carconnectivity.org</providerURL><description>Test app with base certified audio only.</description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/12A9005A.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/12A9005A.cert</appCertificateURL><appInfo><appCategory>0x00010000</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x25</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>application</audioType><contentCategory>0x2</contentCategory></audioInfo></app><app><appID>0x00000004</appID><name>Glympse for Auto</name><providerName>Glympse Inc</providerName><providerURL>http://glympse.com</providerURL><description>Share your location</description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/29653C80.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/29653C80.cert</appCertificateURL><appInfo><appCategory>0x00090000</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10025</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>none</audioType><contentCategory>0x1</contentCategory></audioInfo></app><app><appID>0x00000005</appID><name>Car</name><providerName>HTC</providerName><providerURL>www.htc.com</providerURL><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/145E58B9.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/145E58B9.cert</appCertificateURL><appInfo><appCategory>0x00010001</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10000</contentCategory><trustLevel>0xA0</trustLevel></displayInfo></app><app><appID>0x00000006</appID><name>Phone</name><providerName>HTC</providerName><providerURL>www.htc.com</providerURL><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/CB3222D7.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appInfo><appCategory>0x00020000</appCategory><trustLevel>0x80</trustLevel></appInfo><displayInfo><contentCategory>0x10025</contentCategory><trustLevel>0x80</trustLevel></displayInfo></app><app><appID>0x00000007</appID><name>MirrorLink Map</name><providerName>HTC</providerName><providerURL>www.htc.com</providerURL><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/5787CE6F.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appInfo><appCategory>0x00050000</appCategory><trustLevel>0x80</trustLevel></appInfo><displayInfo><contentCategory>0x10025</contentCategory><trustLevel>0x80</trustLevel></displayInfo><audioInfo><audioType>application</audioType><contentCategory>0x2</contentCategory></audioInfo></app><app><appID>0x00000008</appID><name>Music</name><providerName>HTC</providerName><providerURL>www.htc.com</providerURL><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/F4D8DA21.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/F4D8DA21.cert</appCertificateURL><appInfo><appCategory>0x00030001</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10000</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>application</audioType><contentCategory>0x2</contentCategory></audioInfo></app><app><appID>0x00000009</appID><name>Sygic Car Navigation</name><providerName></providerName><providerURL></providerURL><description></description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/0493CF8F.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/0493CF8F.cert</appCertificateURL><appInfo><appCategory>0x00050000</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10005</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType>none</audioType><contentCategory>0x2</contentCategory></audioInfo></app><app><appID>0x0000000A</appID><name>T-Connect</name><providerName>Toyota</providerName><providerURL></providerURL><description></description><iconList><icon><mimetype>image/png</mimetype><width>144</width><height>144</height><depth>32</depth><url>/upnp/66A19537.png</url></icon></iconList><allowedProfileIDs>0</allowedProfileIDs><remotingInfo><protocolID>VNC</protocolID></remotingInfo><appCertificateURL>/upnp/cert/66A19537.cert</appCertificateURL><appInfo><appCategory>0x00050000</appCategory><trustLevel>0xA0</trustLevel></appInfo><displayInfo><contentCategory>0x10020</contentCategory><trustLevel>0xA0</trustLevel></displayInfo><audioInfo><audioType></audioType><contentCategory>0x0</contentCategory></audioInfo></app><Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><CanonicalizationMethod Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></CanonicalizationMethod><SignatureMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#rsa-sha1\"></SignatureMethod><Reference URI=\"#mlServerAppList\"><Transforms><Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"></Transform><Transform Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"></Transform></Transforms><DigestMethod Algorithm=\"http://www.w3.org/2000/09/xmldsig#sha1\"></DigestMethod><DigestValue>q0ojR5S0UO2zq3b6WUkF+eZ4+mE=</DigestValue></Reference></SignedInfo><SignatureValue>ZcasJxIaFETxc3d6F41PzDRx9c88saUa8w0mbPcigo/UD2+0Un927gekQl6dDZqZl1DEdTtEx8jCfBU43Nh5tKFrvMq43aUIMO35qRpvWksP0uaVJ0hcMxxb+OZezNd5ggGZumh51OdPcZia+ENMmcDhgZi97Dxb90CsoAKDAZpuOdH5chXaOA0HqgqfOCR4nKJ5OVknuuYGrG48W8msegc9rKerjxt1qeguTHZLfKnitCAWmybkP1Z94d/LP+wmI4hc3eRq+jhqiIqyyI9t06J22HkkkU9kNYEM+SxgBFOxUV6jGnlBViFRaOfiaDIs2J1oa+K2sRyznli+DfkFaQ==</SignatureValue></Signature></appList>";
    return sigverify(AppList, pubkey);
}

static void test_server_added(struct tmclient *client, struct tmserver *server)
{
    //    get_application_list(client, server, 0, "remotingInfo@protocolID=\"DAP\",appInfo@appCategory=\"0xF0000001\"");
    get_application_list(client, server, 0, "*");
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

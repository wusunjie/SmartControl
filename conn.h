#ifndef CONN_H
#define CONN_H

struct tmclient;
struct tmserver;

enum direction_info {
    DIRECTION_INFO_OUT,
    DIRECTION_INFO_IN,
    DIRECTION_INFO_BI
};

enum app_category {
    APP_CATEGORY_UNKNOWN = 0x00000000,
    APP_CATEGORY_HOME = 0x00010001
};

enum app_trustlevel {
    APP_TRUSTLEVEL_NONE = 0x0000,
    APP_TRUSTLEVEL_USER = 0x0040,
    APP_TRUSTLEVEL_APP = 0x0060,
    APP_TRUSTLEVEL_Server = 0x0080,
    APP_TRUSTLEVEL_CERT = 0x00a0
};

enum audio_type {
    AUDIO_TYPE_PHONE,
    AUDIO_TYPE_APP,
    AUDIO_TYPE_ALL,
    AUDIO_TYPE_NONE
};

enum protocol {
    PROTOCOL_VNC,
    PROTOCOL_RTP,
    PROTOCOL_BTA2DP,
    PROTOCOL_BTHFP,
    PROTOCOL_DAP,
    PROTOCOL_CDB,
    PROTOCOL_WFD,
    PROTOCOL_NONE
};

struct remoting_info {
    enum protocol protocolID;
    char format[50];
    enum direction_info direction;
    int audioIPL;
    int audioMPL;
};

struct app_info {
    enum app_category appCategory;
    enum app_trustlevel trustLevel;
};

struct audio_info {
    enum audio_type audioType;
};

struct app {
    unsigned int appID;
    char name[50];
    struct remoting_info remoteinfo;
    struct app_info appinfo;
    struct audio_info audioinfo;
};

struct connection_cb {
    void (*server_added)(struct tmclient *client, struct tmserver *server);
    void (*server_removed)(struct tmclient *client, struct tmserver *server);
    void (*applist_update)(struct tmclient *client, struct tmserver *server, struct app *apps, unsigned int count);
};

extern struct tmclient *tmclient_start(struct event_base *base, ev_uint16_t port, struct connection_cb cb);

extern void tmclient_stop(struct tmclient *client);

extern struct tmserver *get_description(struct tmclient *client, const char *remote_uri);

extern int subscribe_service(struct tmclient *client, struct tmserver *server, int sid);

extern void disconnect_server(struct tmclient *client, struct tmserver *server);

extern int get_application_list(struct tmclient *client, struct tmserver *server, int profile, const char *filter);

extern int launch_application(struct tmclient *client, struct tmserver *server, int profile, unsigned int appid);

#endif // CONN_H


#ifndef DAPCLIENT
#define DAPCLIENT

#include <event2/event.h>
#include <event2/http.h>

extern int dapclient_request(struct event_base *base, struct evhttp_uri *uri);

#endif // DAPCLIENT


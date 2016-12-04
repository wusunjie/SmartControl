#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <event2/http.h>

#include "utils.h"

xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name)
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

int get_localaddr(char *local, const char *remote)
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
            strncpy(local, inet_ntoa(((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr), 16);
            freeifaddrs(ifaddr);
            return 0;
        }
    }
    freeifaddrs(ifaddr);
    return -1;
}

int get_localport(int fd, int *port)
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

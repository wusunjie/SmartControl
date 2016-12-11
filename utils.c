#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <event2/http.h>

#include <time.h>
#include <stdint.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

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

int get_certificate_nonce(unsigned char **buf, unsigned int *size)
{
    uint64_t t;
    struct timespec ts;
    unsigned char tmp[100] = {0};
    unsigned char *result = NULL;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
        return -1;
    }
    t = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    snprintf((char *)tmp, 100, "%lu", t);
    RAND_seed(tmp, 100);
    if (!RAND_bytes(tmp, 20)) {
        return -1;
    }
    *size = ((20 + 2) / 3) << 2;
    result = (unsigned char *)calloc(1, *size + 1);
    if (!result) {
        return -1;
    }
    if (!EVP_EncodeBlock(result, tmp, 20)) {
        free(result);
        return -1;
    }
    *buf = result;
    return 0;
}

int base64_decode(const unsigned char *encoded, unsigned int len, struct decode_buffer *buffer)
{
    unsigned char *buf = (unsigned char *)malloc(len + 1);
    int l;
    if (!buf) {
        return -1;
    }
    if (encoded[len - 1] == '\0') {
        len--;
    }
    l = EVP_DecodeBlock(buf, encoded, len);
    if (-1 == l) {
        free(buf);
        return -1;
    }
    while ('=' == encoded[--len]) {
        l--;
    }
    buffer->len = l;
    buffer->data = buf;
    return 0;
}

void decode_buffer_init(struct decode_buffer *buffer)
{
    buffer->data = NULL;
    buffer->len = 0;
}

void decode_buffer_clear(struct decode_buffer *buffer)
{
    free(buffer->data);
    buffer->len = 0;
}

void decode_buffer_append(struct decode_buffer *buffer, unsigned long len)
{
    buffer->data = (unsigned char *)malloc(len);
    buffer->len = len;
}

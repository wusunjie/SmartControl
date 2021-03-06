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
#include <string.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#include "utils.h"

#define BSIZE   (8*1024)

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

int base64_decode(const unsigned char *encoded, unsigned int len, unsigned char **out, unsigned int *olen)
{
    BIO *b64, *rbio;
    int inl = 0;
    unsigned char *buff = (unsigned char *)malloc(BSIZE);
    b64 = BIO_new(BIO_f_base64());
    if (b64 == NULL) {
        free(buff);
        return -1;
    }
    if (NULL == strchr((const char *)encoded, '\n')) {
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    }
    rbio = BIO_new_mem_buf((void *)encoded, len);
    rbio = BIO_push(b64, rbio);

    for (;;) {
        int ret = BIO_read(rbio, (char *)buff + inl, BSIZE);
        if (ret <= 0) {
            free(*out);
            *out = buff;
            *olen = inl;
            break;
        }
        else {
            inl += ret;
        }
    }
    if (NULL == *out) {
        free(buff);
        return -1;
    }
    BIO_free(b64);
    return 0;
}

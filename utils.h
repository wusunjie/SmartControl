#ifndef UTILS
#define UTILS

#include <libxml/tree.h>

struct decode_buffer {
    unsigned char *data;
    unsigned long len;
};

extern xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name);

extern int get_localaddr(char *local, const char *remote);

extern int get_localport(int fd, int *port);

extern int get_certificate_nonce(unsigned char **buf, unsigned int *size);

extern int base64_decode(const unsigned char *encoded, unsigned int len, struct decode_buffer *buffer);

extern void decode_buffer_init(struct decode_buffer *buffer);

extern void decode_buffer_append(struct decode_buffer *buffer, unsigned long len);

extern void decode_buffer_clear(struct decode_buffer *buffer);

#endif // UTILS


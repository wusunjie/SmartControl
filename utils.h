#ifndef UTILS
#define UTILS

#include <libxml/tree.h>


extern xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name);

extern int get_localaddr(char *local, const char *remote);

extern int get_localport(int fd, int *port);

extern int get_certificate_nonce(unsigned char **buf, unsigned int *size);

extern int base64_decode(const unsigned char *encoded, unsigned int len, unsigned char **out, unsigned int *olen);

#endif // UTILS


#ifndef UTILS
#define UTILS

#include <libxml/tree.h>

extern xmlNodePtr xmlFindChildElement(xmlNodePtr parent, xmlChar *name);

extern int get_localaddr(char *local, const char *remote);

extern int get_localport(int fd, int *port);

#endif // UTILS


#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

static const char *M_SEARCH_QUERY_STRING = "M-SEARCH * HTTP/1.1\r\nHOST: 192.168.42.129:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: urn:schemas-upnp-org:device:TmServerDevice:1\r\n\r\n";

static int getaddr_from_ifname(const char *ifname, struct sockaddr *addr);
static int mainloop(int fd);

int main(int argc, char *argv[])
{
	struct sockaddr_in si;
	int s, slen = sizeof(si);
	if (argc < 2) {
		return -1;
	}
	while (getaddr_from_ifname(argv[1], (struct sockaddr *)&si));
	s = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
	if (-1 == s) {
		printf("socket open failed\n");
		return -1;
	}
	si.sin_port = htons(1900);
	printf("interface addr is %s\n", inet_ntoa(si.sin_addr));
	if (-1 == bind(s, (struct sockaddr *)&si, sizeof(si))) {
		close(s);
		printf("bind failed\n");
		return -1;
	}
	return mainloop(s);
}

static int getaddr_from_ifname(const char *ifname, struct sockaddr *addr)
{
	struct ifaddrs *ifaddr, *ifa;
	if (-1 == getifaddrs(&ifaddr)) {
		return -1;
	}
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (0 != strcmp(ifa->ifa_name, ifname)) {
			continue;
		}
		if (NULL == ifa->ifa_addr) {
			continue;
		}
		if (AF_INET != ifa->ifa_addr->sa_family) {
			continue;
		}
		memcpy(addr, ifa->ifa_addr, sizeof(struct sockaddr_in));
		break;
	}
	freeifaddrs(ifaddr);
	if (NULL == ifa) {
		return -1;
	} else {
		return 0;
	}
}

static int mainloop(int fd)
{
	struct pollfd pfd;
	int n;
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (-1 != (n = poll(&pfd, 1, 1000))) {
		if (0 == n) {
			struct sockaddr_in si;
			si.sin_family = AF_INET;
			si.sin_port = htons(1900);
			inet_aton("192.168.42.129", &si.sin_addr);
			sendto(fd, M_SEARCH_QUERY_STRING, strlen(M_SEARCH_QUERY_STRING), 0, (struct sockaddr *)&si, sizeof(si));
			printf("M-SEARCH Request Sent\n");
		}
		else if (pfd.events == pfd.revents) {
			char buf[1024];
			read(fd, buf, 1024);
			printf("%s\n", buf);
			printf("M-SEARCH Response Received\n");
		}
	}
	close(fd);
	return 0;
}


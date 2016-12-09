#include <asm/types.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#include <string.h>
#include <stdint.h>

#include "netmgr.h"
#include "netdev.h"


int netmgr_create(struct netmgr *mgr)
{
	int fd;
	struct sockaddr_nl sa;
	if (0 == mgr) {
		return -1;
	}

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE);
	if (-1 == fd) {
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = RTMGRP_IPV4_IFADDR;
	if (-1 == bind(fd, (struct sockaddr *) &sa, sizeof(sa))) {
		close(fd);
		return -1;
	}
	mgr->fd = fd;
	return 0;
}

int netmgr_destory(struct netmgr *mgr)
{
	if (0 == mgr) {
		return -1;
	}

	return close(mgr->fd);
}

int netmgr_dowork(struct netmgr *mgr)
{
	int len;
	char buf[4096];
	struct nlmsghdr *nh;

	struct pollfd pfd;
	int ret;
	pfd.fd = mgr->fd;
	pfd.events = POLLPRI;
	ret = poll(&pfd, 1, timeout);

	if (ret < 0) {
		return -1;
	}
	else if (!ret) {
		return 0;
	}

	len = recv(mgr->fd, buf, 4096, 0);
	if (-1 == len) {
		return 0;
	}

	for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len);
			nh = NLMSG_NEXT(nh, len)) {
		uint32_t ipaddr;
		char *ifname;
		/* The end of multipart message. */
		if (nh->nlmsg_type == NLMSG_DONE)
			return 1;

		if (nh->nlmsg_type == NLMSG_ERROR) {
			/* Do some error handling. */
		}

		/* Continue with parsing payload. */
		if (nh->nlmsg_type == RTM_NEWADDR ||
				nh->nlmsg_type == RTM_DELADDR) {
			struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(nh);
			struct rtattr *rth = IFA_RTA(ifa);
			int rtl = IFA_PAYLOAD(nh);
			int state = 0;
			while (rtl && RTA_OK(rth, rtl)) {
				if (rth->rta_type == IFA_LOCAL) {
			        	ipaddr = htonl(*((uint32_t *)RTA_DATA(rth)));
					state++;
				}
				if (rth->rta_type == IFA_LABEL) {
					ifname = RTA_DATA(rth);
					state++;
				}
				if (2 == state) {
					if (nh->nlmsg_type == RTM_NEWADDR) {
						netdev_add(ifname, ipaddr);
					}
					else {
						netdev_remove(ifname, ipaddr);
					}
					state = 0;
				}
				rth = RTA_NEXT(rth, rtl);
			}
		}
	}
}


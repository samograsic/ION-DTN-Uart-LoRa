/*	libipv6.c:	ipv6 related functions		*/
/*	Author: Scott Johnson, solarnetone.org		*/
/*							*/
#include "platform.h"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int     parseSocketSpecSix(char *socketSpec, struct sockaddr_in6 *ip6Address)
{
	char            *delimiter;
        char            *hostname;
	

	CHKERR(ip6Address);
	if (socketSpec == NULL || *socketSpec == '\0')
        {
                return 0;               /*      Use defaults.           */
        }

	ip6Address->sin6_family = AF_INET6;
	ip6Address->sin6_addr = in6addr_any;
	ip6Address->sin6_port = htons(0);
	ip6Address->sin6_flowinfo = 0;
	
	delimiter = strchr(socketSpec, '!');	
	if (delimiter)
	{
		*delimiter ='\0';	/*      Delimit host name.      */
	}

	/* Extract IPv6 address, convert to network byte order, and store in value in struct returned to calling daemon*/

	hostname = socketSpec;
	if (strlen(hostname) != 0)
	{
		inet_pton(AF_INET6, hostname, &ip6Address->sin6_addr);
	}	

	if (delimiter == NULL)
		{
			return 0;
		}
	/* Extract port number, convert to network byte order, and store in value in struct returned to calling daemon*/

	delimiter = strchr(socketSpec, '!');	
	if (delimiter)
	{
		ip6Address->sin6_port = (atoi(delimiter + 1)); 
		if (ip6Address->sin6_port != 0) 
		{
			if (ip6Address->sin6_port < 1024 || ip6Address->sin6_port > 65535)
			{
				writeMemoNote("[?] Invalid port number.", utoa(ip6Address->sin6_port));	
				return -1;
			}
			else
			{
				ip6Address->sin6_port = htons(ip6Address->sin6_port);
			}
		}
	}
	return 0;
}

/*
	libudpcla.c:	common functions for BP UDP-based
			convergence-layer daemons.

	Author: Ted Piotrowski, APL 
		Scott Burleigh, JPL
		Scott Johnson, SolarNetOne.org
	Copyright (c) 2006, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
	
									*/
#include "udpcla6.h"

/*	*	*	Sender functions	*	*	*	*/

static int	openUdp6Socket(int *sock)
{
	*sock = -1;

	*sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (*sock < 0)
	{
		putSysErrmsg("CLO can't open UDP socket", NULL);
		return -1;
	}

	return 0;
}

int	sendBytesBy6UDP(int *bundleSocket, char *from, int length,
		struct sockaddr_in6 *socketName)
{
	int	bytesWritten;

	CHKERR(socketName && bundleSocket && from);
	while (1)	/*	Continue until not interrupted.		*/
	{
		bytesWritten = isendto(*bundleSocket, from, length, 0,
			(struct sockaddr *) socketName, sizeof(struct sockaddr_in6));
		if (bytesWritten < 0)
		{
			switch (errno)
			{
			case EINTR:	/*	Interrupted; retry.	*/
				continue;

			case EPIPE:	/*	Lost connection.	*/
			case EBADF:
			case ETIMEDOUT:
			case ECONNRESET:
				closesocket(*bundleSocket);
				*bundleSocket = -1;
			}

			putSysErrmsg("CLO write() error on socket", NULL);
		}

		return bytesWritten;
	}
}

int	sendBundleBy6UDP(struct sockaddr_in6 *socketName, int *bundleSocket,
		unsigned int bundleLength, Object bundleZco,
		unsigned char *buffer)
{
	Sdr		sdr;
	ZcoReader	reader;
	int		bytesToSend;
	int		bytesSent;

	CHKERR(socketName && bundleSocket && buffer);
	if (bundleLength > UDPCLA_BUFSZ)
	{
		putErrmsg("Bundle is too big for UDP CLA.", itoa(bundleLength));
		return -1;
	}

	/*	Connect to CLI as necessary.				*/

	if (*bundleSocket < 0)
	{
		if (openUdp6Socket(bundleSocket) < 0)
		{
			/*	Treat I/O error as a transient anomaly,
			 *	note incomplete transmission.		*/

			return 0;
		}
	}

	/*	Send the bundle in a single UDP datagram.		*/

	sdr = getIonsdr();
	zco_start_transmitting(bundleZco, &reader);
	zco_track_file_offset(&reader);
	CHKERR(sdr_begin_xn(sdr));
	bytesToSend = zco_transmit(sdr, &reader, UDPCLA_BUFSZ, (char *) buffer);
	if (sdr_end_xn(sdr) < 0 || bytesToSend < 0)
	{
		putErrmsg("Can't issue from ZCO.", NULL);
		return -1;
	}

	bytesSent = sendBytesBy6UDP(bundleSocket, (char *) buffer, bytesToSend,
			 socketName);
	if (bytesSent < 0)
	{
		if (bpHandleXmitFailure(bundleZco) < 0)
		{
			putErrmsg("Can't handle xmit failure.", NULL);
			return -1;
		}

		if (*bundleSocket == -1)
		{
			/*	Just lost connection; treat as a
			 *	transient anomaly, note the incomplete
			 *	transmission.				*/

			writeMemo("[i] Lost UDP connection to CLI; restart CLO \
when connectivity is restored.");
			return 0;
		}
		else
		{
			/*	Big problem; shut down.			*/

			putErrmsg("Failed to send by UDP.", NULL);
			return -1;
		}
	}
	else
	{
		if (bpHandleXmitSuccess(bundleZco) < 0)
		{
			putErrmsg("Can't handle xmit success.", NULL);
			return -1;
		}
	}

	return bytesSent;
}

/*	*	*	Receiver functions	*	*	*	*/

int	receiveBytesBy6UDP(int bundleSocket, struct sockaddr_in6 *fromAddr,
		char *into, int length)
{
	int		bytesRead;
	socklen_t	fromSize;

	CHKERR(fromAddr && length);
	fromSize = sizeof(struct sockaddr_in6);
	bytesRead = irecvfrom(bundleSocket, into, length, 0,
			(struct sockaddr *) fromAddr, &fromSize);
	if (bytesRead < 0)
	{
		if (errno == EBADF)	/*	Shutdown.		*/
		{
			return 0;
		}

		putSysErrmsg("CLI read() error on socket", NULL);
		return -1;
	}

	return bytesRead;
}

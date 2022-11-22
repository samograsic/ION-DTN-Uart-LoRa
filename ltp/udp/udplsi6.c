/*
	udplsi.c:	LTP UDP-based link service daemon.

	Author: Scott Burleigh, JPL

	Copyright (c) 2007, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
	10/7/2022, modified for IPv6 support.  Scott Johnson,
        SolarNetOne.org.  Well paid contracts welcome.
	
									*/
#include "udplsa.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>

static void	interruptThread(int signum)
{
	isignal(SIGTERM, interruptThread);
	ionKillMainThread("udplsi6");
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	udplsi6(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	char	*endpointSpec = (char *) a1;
#else
int	main(int argc, char *argv[])
{
	char	*endpointSpec = (argc > 1 ? argv[1] : NULL);
#endif
	Sdr			sdr;
	char			lsiCmd[256];
	LtpVseat		*vseat;
	PsmAddress		vseatElt;
	struct sockaddr_in6	inetName;
	ReceiverThreadParms	rtp;
	socklen_t		nameLength;
	pthread_t		receiverThread;
	int			fd;
	char			quit = '\0';

	/*	Note that ltpadmin must be run before the first
	 *	invocation of ltplsi, to initialize the LTP database
	 *	(as necessary) and dynamic database.			*/ 

	if (ltpInit(0) < 0)
	{
		putErrmsg("udplsi6 can't initialize LTP.", NULL);
		return 1;
	}

	sdr = getIonsdr();
	isprintf(lsiCmd, sizeof lsiCmd, "udplsi6 %s", endpointSpec);
	CHKERR(sdr_begin_xn(sdr));
	findSeat(lsiCmd, &vseat, &vseatElt);
	sdr_exit_xn(sdr);
	if (vseatElt == 0)
	{
		putErrmsg("Undefined IPv6 LSI", lsiCmd);
		return 1;
	}

	if (vseat->lsiPid != ERROR && vseat->lsiPid != sm_TaskIdSelf())
	{
		putErrmsg("LSI task is already started.", itoa(vseat->lsiPid));
		return 1;
	}

	/*	All command-line arguments are now validated.		*/

	if (endpointSpec)
	{
		if (parseSocketSpecSix(endpointSpec, (struct sockaddr_in6 *) &inetName) != 0)
		{
			putErrmsg("Can't get IP/port for endpointSpec.",
					endpointSpec);
			return -1;
		}
	}

	if (inetName.sin6_port == 0)
	{
		inetName.sin6_port = htons(1113);
	}

	rtp.linkSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (rtp.linkSocket < 0)
	{
		putSysErrmsg("LSI can't open UDP socket", NULL);
		return -1;
	}

	nameLength = sizeof(inetName);
	if (reUseAddress(rtp.linkSocket)
	|| bind(rtp.linkSocket,  (struct sockaddr *) &inetName, nameLength) < 0
	|| getsockname(rtp.linkSocket, (struct sockaddr *) &inetName, &nameLength) < 0)
	{
		closesocket(rtp.linkSocket);
		putSysErrmsg("LSI can't initialize UDP socket", NULL);
		return 1;
	}

	/*	Set up signal handling; SIGTERM is shutdown signal.	*/

	ionNoteMainThread("udplsi6");
	isignal(SIGTERM, interruptThread);

	/*	Start the receiver thread.				*/

	rtp.running = 1;
	if (pthread_begin(&receiverThread, NULL, udplsa_handle_datagrams,
			&rtp, "udplsi6_receiver"))
	{
		closesocket(rtp.linkSocket);
		putSysErrmsg("udplsi6 can't create receiver thread", NULL);
		return 1;
	}

	/*	Now sleep until interrupted by SIGTERM, at which point
	 *	it's time to stop the link service.			*/

	{
		char	txt[500];

		isprintf(txt, sizeof(txt),
			"[i] udplsi6 is running, spec=[%s:%d].", 
			endpointSpec, ntohs(inetName.sin6_port));
		writeMemo(txt);
	}

	ionPauseMainThread(-1);

	/*	Time to shut down.					*/

	rtp.running = 0;

	/*	Wake up the receiver thread by opening a single-use
	 *	transmission socket and sending a 1-byte datagram
	 *	to the reception socket.				*/

	fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd >= 0)
	{
		if (isendto(fd, &quit, 1, 0, (struct sockaddr *) &inetName,
				sizeof(struct sockaddr_in6)) == 1)
		{
			pthread_join(receiverThread, NULL);
		}

		closesocket(fd);
	}

	closesocket(rtp.linkSocket);
	writeErrmsgMemos();
	writeMemo("[i] udplsi6 has ended.");
	ionDetach();
	return 0;
}

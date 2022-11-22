/*
	udpcli.c:	BP UDP-based convergence-layer input
			daemon, designed to serve as an input
			duct.

	Author: Ted Piotrowski, APL
		Scott Burleigh, JPL
		Scott Johnson, SolarNetOne.org
	Copyright (c) 2006, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
	
									*/
#include "udpcla6.h"
#include "ipnfw.h"
#include "dtn2fw.h"
#include <sys/socket.h>

static void	interruptThread(int signum)
{
	isignal(SIGTERM, interruptThread);
	ionKillMainThread("udpcli6");
}

/*	*	*	Receiver thread functions	*	*	*/

typedef struct
{
	VInduct		*vduct;
	int		ductSocket;
	int		running;
} ReceiverThreadParms;

static void	*handle6Datagrams(void *parm)
{
	/*	Main loop for UDP datagram reception and handling.	*/

	ReceiverThreadParms	*rtp = (ReceiverThreadParms *) parm;
	char			*procName = "udpcli6";
	AcqWorkArea		*work;
	char			*buffer;
	int			bundleLength;
	struct sockaddr_in6	fromAddr;
	struct in6_addr		hostNbr;
	char			hostName[MAXHOSTNAMELEN + 1];
	int			sin6_len; 

	snooze(1);	/*	Let main thread become interruptible.	*/
	work = bpGetAcqArea(rtp->vduct);
	if (work == NULL)
	{
		putErrmsg("udpcli6 can't get acquisition work area.", NULL);
		ionKillMainThread(procName);
		return NULL;
	}

	buffer = MTAKE(UDPCLA_BUFSZ);
	if (buffer == NULL)
	{
		putErrmsg("udpcli6 can't get UDP buffer.", NULL);
		ionKillMainThread(procName);
		return NULL;
	}

	/*	Can now start receiving bundles.  On failure, take
	 *	down the CLI.						*/

	while (rtp->running)
	{	
		bundleLength = receiveBytesBy6UDP(rtp->ductSocket, &fromAddr,
				buffer, UDPCLA_BUFSZ);
		switch (bundleLength)
		{
		case -1:
		case 0:
			putErrmsg("Can't acquire bundle.", NULL);
			ionKillMainThread(procName);

			/*	Intentional fall-through to next case.	*/

		case 1:				/*	Normal stop.	*/
			rtp->running = 0;
			continue;

		default:
			break;			/*	Out of switch.	*/
		}
		sin6_len = sizeof(fromAddr.sin6_addr);
		memcpy((char *) &hostNbr.s6_addr,
			(char *) &(fromAddr.sin6_addr), sin6_len);
		inet_ntop(AF_INET6, &hostNbr, (char* )hostName, sin6_len);
		if (bpBeginAcq(work, 0, NULL) < 0
		|| bpContinueAcq(work, buffer, bundleLength, 0, 0) < 0
		|| bpEndAcq(work) < 0)
		{
			putErrmsg("Can't acquire bundle.", NULL);
			ionKillMainThread(procName);
			rtp->running = 0;
			continue;
		}

		/*	Make sure other tasks have a chance to run.	*/

		sm_TaskYield();
	}

	writeErrmsgMemos();
	writeMemo("[i] udpcli6 receiver thread has ended.");

	/*	Free resources.						*/

	bpReleaseAcqArea(work);
	MRELEASE(buffer);
	return NULL;
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	udpcli6(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	char	*ductName = (char *) a1;
#else
int	main(int argc, char *argv[])
{
	char	*ductName = (argc > 1 ? argv[1] : NULL);
#endif
	VInduct			*vduct;
	PsmAddress		vductElt;
	Sdr			sdr;
	Induct			duct;
	ClProtocol		protocol;
	char			*hostName;
	struct sockaddr_in6	hostNbr;
	socklen_t		nameLength;
	ReceiverThreadParms	rtp;
	pthread_t		receiverThread;
	int			fd;
	char			quit = 0;
	if (ductName == NULL)
	{
		PUTS("Usage: udpcli6 <local host name>[!<port number>]");
		return 0;
	}

	if (bpAttach() < 0)
	{
		putErrmsg("udpcli6 can't attach to BP.", NULL);
		return -1;
	}

	findInduct("udp", ductName, &vduct, &vductElt);
	if (vductElt == 0)
	{
		putErrmsg("No such udp duct.", ductName);
		return -1;
	}

	if (vduct->cliPid != ERROR && vduct->cliPid != sm_TaskIdSelf())
	{
		putErrmsg("CLI task is already started for this duct.",
				itoa(vduct->cliPid));
		return -1;
	}

	/*	All command-line arguments are now validated.		*/

	sdr = getIonsdr();
	CHKZERO(sdr_begin_xn(sdr));
	sdr_read(sdr, (char *) &duct, sdr_list_data(sdr, vduct->inductElt),
			sizeof(Induct));
	sdr_read(sdr, (char *) &protocol, duct.protocol, sizeof(ClProtocol));
	sdr_exit_xn(sdr);
	hostName = ductName;
	if (parseSocketSpecSix(ductName, &hostNbr) != 0)
	{
		putErrmsg("Can't get IP/port for host.", hostName);
		return -1;
	}

	if (hostNbr.sin6_port == 0)
	{
		hostNbr.sin6_port = htons(4556);
	}

	rtp.vduct = vduct;
	rtp.ductSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (rtp.ductSocket < 0)
	{
		putSysErrmsg("Can't open UDP socket", NULL);
		return -1;
	}

	nameLength = sizeof(hostNbr);
	if (reUseAddress(rtp.ductSocket)
	|| bind(rtp.ductSocket, (struct sockaddr *) &hostNbr, nameLength) < 0 
	|| getsockname(rtp.ductSocket, (struct sockaddr *) &hostNbr, &nameLength) < 0)
	{
		closesocket(rtp.ductSocket);
		putSysErrmsg("Can't initialize socket", NULL);
		return -1;
	}

	/*	Set up signal handling; SIGTERM is shutdown signal.	*/

	ionNoteMainThread("udpcli6");
	isignal(SIGTERM, interruptThread);

	/*	Start the receiver thread.				*/

	rtp.running = 1;
	if (pthread_begin(&receiverThread, NULL, handle6Datagrams, &rtp))
	{
		closesocket(rtp.ductSocket);
		putSysErrmsg("udpcli6 can't create receiver thread", NULL);
		return -1;
	}

	/*	Now sleep until interrupted by SIGTERM, at which point
	 *	it's time to stop the induct.				*/

	{
		char	txt[500];
		isprintf(txt, sizeof(txt),
			"[i] udpcli6 is running, spec=[%s:%d].",
			ductName, ntohs(hostNbr.sin6_port));
		writeMemo(txt);
	}

	ionPauseMainThread(-1);

	/*	Time to shut down.					*/

	rtp.running = 0;

	/*	Create one-use socket for the closing quit byte.	*/

/*	if (strcmp (char *) hostNbr.sin6_addr, "0")
	{*/
		/*	Can't send to host number 0, so send to
		 *	loopback address.				*/

/*		hostNbr.sin6_addr =  in6addr_loopback;		ipv6 localhost*/	
	

	/*	Wake up the receiver thread by opening a single-use
	 *	transmission socket and sending a 1-byte datagram
	 *	to the reception socket.				*/

	fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd >= 0)
	{
		if (isendto(fd, &quit, 1, 0, (struct sockaddr *) &hostNbr,
				sizeof(struct sockaddr_in6)) == 1)
		{
			pthread_join(receiverThread, NULL);
		}

		closesocket(fd);
	}

	closesocket(rtp.ductSocket);
	writeErrmsgMemos();
	writeMemo("[i] udpcli6 duct has ended.");
	ionDetach();
	return 0;
}

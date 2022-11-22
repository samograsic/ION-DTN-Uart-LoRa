/*
	uartcli.c:	BP UART convergence-layer input daemon.

	Author: Samo Grasic (samo@grasic.net), Luleå University of Technology, Sweden

*/
#include "uartcla.h"
#include "ipnfw.h"
#include "dtn2fw.h"
#include <sys/socket.h>

static void	interruptThread(int signum)
{
	isignal(SIGTERM, interruptThread);
	ionKillMainThread("uartcli");
}

/*	*	*	Receiver thread functions	*	*	*/

typedef struct
{
	VInduct		*vduct;
	int		*ductSocket;
	int		running;
	struct uartdescriptor *uartPort;
} ReceiverThreadParms;

static void	*handleDatagrams(void *parm)
{
	/*	Main loop for UDP datagram reception and handling.	*/

	ReceiverThreadParms	*rtp = (ReceiverThreadParms *) parm;
	char			*procName = "uartcli";
	AcqWorkArea		*work;
	char			*buffer;
	int			bundleLength;

	snooze(1);	/*	Let main thread become interruptible.	*/
	work = bpGetAcqArea(rtp->vduct);
	if (work == NULL)
	{
		putErrmsg("uartcli can't get acquisition work area.", NULL);
		ionKillMainThread(procName);
		return NULL;
	}

	buffer = MTAKE(UARTCLA_BUFSZ);
	if (buffer == NULL)
	{
		putErrmsg("uartcli can't get UART buffer.", NULL);
		ionKillMainThread(procName);
		return NULL;
	}

	/*	Can now start receiving bundles.  On failure, take
	 *	down the CLI.						*/

	while (rtp->running)
	{	
		bundleLength = receiveBytesByUart(rtp->ductSocket, rtp->uartPort,buffer, UARTCLA_BUFSZ);
		switch (bundleLength)
		{
		case -1:
		case 0:
		//	putErrmsg("Can't acquire bundle.", NULL);
			break;
			//ionKillMainThread(procName);

			/*	Intentional fall-through to next case.	*/

		case 1:				/*	Normal stop.	*/
			//rtp->running = 0;
			continue;

		default:
			break;			/*	Out of switch.	*/
		}
		if(bundleLength>0)
		{		if (bpBeginAcq(work, 0, NULL) < 0
		|| bpContinueAcq(work, buffer, bundleLength, 0, 0) < 0
		|| bpEndAcq(work) < 0)
		{
			putErrmsg("Can't acquire bundle.", NULL);
			ionKillMainThread(procName);
			rtp->running = 0;
			continue;
		}
		}
		/*	Make sure other tasks have a chance to run.	*/

		sm_TaskYield();
	}

	writeErrmsgMemos();
	writeMemo("[i] uartcli receiver thread has ended.");

	/*	Free resources.						*/

	bpReleaseAcqArea(work);
	MRELEASE(buffer);
	return NULL;
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	udpcli(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
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
	struct uartdescriptor	hostNbr;
	ReceiverThreadParms	rtp;
	pthread_t		receiverThread;
	int ductSocket = -1;

	if (ductName == NULL)
	{
		PUTS("Usage: uartcli {<uart file descriptor>,<uart speed>}");
		return 0;
	}
	if (bpAttach() < 0)
	{
		putErrmsg("uartcli can't attach to BP.", NULL);
		return -1;
	}

	findInduct("uart", ductName, &vduct, &vductElt);
	if (vductElt == 0)
	{
		putErrmsg("No such uart duct.", ductName);
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
	if (parseUartSpec(ductName, &hostNbr) != 0)
	{
		putErrmsg("UART CLI Can't get UART spec.", ductName);
		return -1;
	}
	rtp.vduct = vduct;
	rtp.ductSocket = &ductSocket;
	rtp.uartPort = &hostNbr;

	/*	Set up signal handling; SIGTERM is shutdown signal.	*/

	ionNoteMainThread("uartcli");
	isignal(SIGTERM, interruptThread);

	/*	Start the receiver thread.				*/

	rtp.running = 1;
	if (pthread_begin(&receiverThread, NULL, handleDatagrams, &rtp))
	{
		putSysErrmsg("uartcli can't create receiver thread", NULL);
		return -1;
	}

	/*	Now sleep until interrupted by SIGTERM, at which point
	 *	it's time to stop the induct.				*/

	{
		char	mBuf[1024]; //Debug string buffer
		isprintf(mBuf, sizeof(mBuf),"[i] uartcli is running, spec = '%s'",	ductName);
		writeMemo(mBuf);
		writeErrmsgMemos();
	}

	ionPauseMainThread(-1);

	/*	Time to shut down.					*/

	rtp.running = 0;
	
	writeErrmsgMemos();
	writeMemo("[i] uart CLI duct has ended.");
	ionDetach();
	return 0;
}


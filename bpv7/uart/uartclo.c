/*
	uartclo.c:	BP UART convergence-layer output daemon.

	Author: Samo Grasic (samo@grasic.net), Luleå University of Technology, Sweden

*/

#include "uartcla.h"

static sm_SemId	 uartcloSemaphore(sm_SemId *semid)
{
	static sm_SemId	semaphore = -1;
	
	if (semid)
	{
		semaphore = *semid;
	}

	return semaphore;
}

static void	shutDownClo(int signum)
{
	sm_SemEnd(uartcloSemaphore(NULL));
}

/*	*	*	Main thread functions	*	*	*	*/

static unsigned long	getUsecTimestamp()
{
	struct timeval	tv;

	getCurrentTime(&tv);
	return ((tv.tv_sec * 1000000) + tv.tv_usec);
}

#if defined (ION_LWT)
int	uartclo(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	char			*fileDescriptor = (char *) a1;
#else
int	main(int argc, char *argv[])
{
	char			*fileDescriptor = (argc > 1 ? argv[1] : NULL);
#endif
/*	char			ownHostName[MAXHOSTNAMELEN];*/
	struct uartdescriptor	hostNbr;

	unsigned char		*buffer;
	VOutduct		*vduct;
	PsmAddress		vductElt;
	Sdr			sdr;
	Outduct			outduct;
	Object			planDuctList;
	Object			planObj = 0;
	BpPlan			plan;
	IonNeighbor		*neighbor = NULL;
	PsmAddress		nextElt;
	Object			bundleZco;
	BpAncillaryData		ancillaryData;
	unsigned int		bundleLength;
	int			ductSocket = -1;
	int			bytesSent;

	/*	Rate control calculation is based on treating elapsed
	 *	time as a currency.					*/

	float			timeCostPerByte;/*	In seconds.	*/
	unsigned long		startTimestamp;	/*	Billing cycle.	*/
	unsigned int		totalPaid;	/*	Since last send.*/
	unsigned int		currentPaid;	/*	Sending seg.	*/
	float			totalCostSecs;	/*	For this seg.	*/
	unsigned int		totalCost;	/*	Microseconds.	*/
	unsigned int		balanceDue;	/*	Until next seg.	*/
	unsigned int		prevPaid = 0;	/*	Prior snooze.	*/

	/*	Note: for backward compatibility, we accept and ignore
	 *	a round-trip time value that precedes the fileDescriptor.	*/

char	mBuf[1024]; //Debug string buffer



	if (fileDescriptor == NULL)
	{
		PUTS("Usage: uartclo {<uart file descriptor>,<uart speed>}");
			return 0;
	}
	parseUartSpec(fileDescriptor, &hostNbr);

	//isprintf(mBuf, sizeof(mBuf),"[i] uartclo uses fileDescriptor = '%s'",fileDescriptor);
	//writeMemo(mBuf);
	//writeErrmsgMemos();



	if (bpAttach() < 0)
	{
		putErrmsg("uartclo can't attach to BP.", NULL);
		return -1;
	}
	isprintf(mBuf, sizeof(mBuf),"[i] UART, attached to the BP",fileDescriptor);
	writeMemo(mBuf);
	writeErrmsgMemos();



	buffer = MTAKE(UARTCLA_BUFSZ);
	if (buffer == NULL)
	{
		putErrmsg("No memory for uart buffer in uartclo.", NULL);
		return -1;
	}
			isprintf(mBuf, sizeof(mBuf),"[i] UART Memory buffer OK",fileDescriptor);
		writeMemo(mBuf);
	writeErrmsgMemos();

	findOutduct("uart", fileDescriptor, &vduct, &vductElt);
	if (vductElt == 0)
	{
		putErrmsg("No such uart duct.", fileDescriptor);
		MRELEASE(buffer);
		return -1;
	}

	isprintf(mBuf, sizeof(mBuf),"[i] UART duct found, OK",fileDescriptor);
	writeMemo(mBuf);
	writeErrmsgMemos();

	if (vduct->cloPid != ERROR && vduct->cloPid != sm_TaskIdSelf())
	{
		putErrmsg("CLO task is already started for this duct.",
				itoa(vduct->cloPid));
		MRELEASE(buffer);
		return -1;
	}
	isprintf(mBuf, sizeof(mBuf),"[i] UART CLO task started for this duct., OK",hostNbr.uart_file_descriptor);
	writeMemo(mBuf);
	writeErrmsgMemos();

	/*	All command-line arguments are now validated.		*/

	neighbor = NULL;
	sdr = getIonsdr();
	CHKZERO(sdr_begin_xn(sdr));
	sdr_read(sdr, (char *) &outduct, sdr_list_data(sdr, vduct->outductElt),
			sizeof(Outduct));
	if (outduct.planDuctListElt)
	{
		planDuctList = sdr_list_list(sdr, outduct.planDuctListElt);
		planObj = sdr_list_user_data(sdr, planDuctList);
		if (planObj)
		{
			sdr_read(sdr, (char *) &plan, planObj, sizeof(BpPlan));
		}
	}

	sdr_exit_xn(sdr);

	/*	Set up signal handling.  SIGTERM is shutdown signal.	*/

	oK(uartcloSemaphore(&(vduct->semaphore)));
	isignal(SIGTERM, shutDownClo);

	/*	Can now begin transmitting to remote duct.		*/

	{
		char	memoBuf[1024];

		isprintf(memoBuf, sizeof(memoBuf),
				"[i] uartclo is running, spec = '%s'",
				fileDescriptor);
		writeMemo(memoBuf);
	}

	startTimestamp = getUsecTimestamp();
	while (!(sm_SemEnded(vduct->semaphore)))
	{
		if (bpDequeue(vduct, &bundleZco, &ancillaryData, 0) < 0)
		{
			putErrmsg("Can't dequeue bundle.", NULL);
			break;
		}

		if (bundleZco == 0)	/*	Outduct closed.		*/
		{
			writeMemo("[i] uartclo outduct closed.");
			sm_SemEnd(uartcloSemaphore(NULL));/*	Stop.	*/
			continue;
		}

		if (bundleZco == 1)	/*	Got a corrupt bundle.	*/
		{
			continue;	/*	Get next bundle.	*/
		}

		CHKZERO(sdr_begin_xn(sdr));
		bundleLength = zco_length(sdr, bundleZco);
		sdr_exit_xn(sdr);
		isprintf(mBuf, sizeof(mBuf),"[i] UART: Sending out BUNDLE with length:%d",bundleLength);
		writeMemo(mBuf);
		writeErrmsgMemos();



		bytesSent = sendBundleByUart(&hostNbr, &ductSocket,
				bundleLength, bundleZco, buffer);
		if (bytesSent < bundleLength)
		{
			sm_SemEnd(uartcloSemaphore(NULL));/*	Stop.	*/
			continue;
		}

		/*	Rate control calculation is based on treating
		 *	elapsed time as a currency, the price you
		 *	pay (by microsnooze) for sending a segment
		 *	of a given size.  All cost figures are
		 *	expressed in microseconds except the computed
		 *	totalCostSecs of the segment.			*/

		totalPaid = getUsecTimestamp() - startTimestamp;

		/*	Start clock for next bill.			*/

		startTimestamp = getUsecTimestamp();

		/*	Compute time balance due.			*/

		if (totalPaid >= prevPaid)
		{
		/*	This should always be true provided that
		 *	clock_gettime() is supported by the O/S.	*/

			currentPaid = totalPaid - prevPaid;
		}
		else
		{
			currentPaid = 0;
		}

		/*	Get current time cost, in seconds, per byte.	*/

		if (neighbor == NULL)
		{
			if (planObj && plan.neighborNodeNbr)
			{
				neighbor = findNeighbor(getIonVdb(),
						plan.neighborNodeNbr, &nextElt);
			}
		}

		if (neighbor && neighbor->xmitRate > 0)
		{
			timeCostPerByte = 1.0 / (neighbor->xmitRate);
		}
		else	/*	No link service rate control.		*/ 
		{
			timeCostPerByte = 0.0;
		}

		totalCostSecs = timeCostPerByte * computeECCC(bundleLength);
		totalCost = totalCostSecs * 1000000.0;	/*	usec.	*/
		if (totalCost > currentPaid)
		{
			balanceDue = totalCost - currentPaid;
		}
		else
		{
			balanceDue = 0;
		}

		if (balanceDue > 0)
		{
			microsnooze(balanceDue);
		}

		prevPaid = balanceDue;

		/*	Make sure other tasks have a chance to run.	*/

		sm_TaskYield();
	}

	if (ductSocket != -1)
	{
		closesocket(ductSocket);
	}

	writeErrmsgMemos();
	writeMemo("[i] uartclo duct has ended.");
	MRELEASE(buffer);
	ionDetach();
	return 0;
}

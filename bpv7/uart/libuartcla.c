/*
	libuartcla.c:	Common library for UART convergence-layer output daemon.

	Author: Samo Grasic (samo@grasic.net), Luleå University of Technology, Sweden

*/
#include "uartcla.h"
// Linux UART  headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()


/*	*	*	Sender functions	*	*	*	*/

static int	openUartPortRead(int *uartPort,struct uartdescriptor *uartDes)
{

	char	mBuf[1024]; //Debug string buffer
	isprintf(mBuf, sizeof(mBuf),"[i] UART file descriptor:%s, length:%d",uartDes->uart_file_descriptor,strlen(uartDes->uart_file_descriptor));
	writeMemo(mBuf);
	writeErrmsgMemos();

  	*uartPort = open(uartDes->uart_file_descriptor, O_RDONLY);
	if (*uartPort < 0) {
		isprintf(mBuf, sizeof(mBuf),"[i] UART Error while opening device to read: %d  %s", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
        return -1;
	}
	else
	{
		isprintf(mBuf, sizeof(mBuf),"[i] UART Port Nr: %d ",*uartPort);
		writeMemo(mBuf);
		writeErrmsgMemos();
	}
	struct termios tty;
	writeMemo("[i] Uart Port Opened....");
	writeErrmsgMemos();
	// Reading uart settings
  	if(tcgetattr(*uartPort, &tty) != 0) {
		isprintf(mBuf, sizeof(mBuf),"[i] UART read Error %d from tcgetattr: %s", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
        return -1;
  	}
	writeMemo("[i] tcgetattr read....");
	writeErrmsgMemos();

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
  	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
  	tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size 
  	tty.c_cflag |= CS8; // 8 bits per byte (most common)
  	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
  	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
	tty.c_lflag &= ~ICANON;
  	tty.c_lflag &= ~ECHO; // Disable echo
  	tty.c_lflag &= ~ECHOE; // Disable erasure
  	tty.c_lflag &= ~ECHONL; // Disable new-line echo
  	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
  	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
  	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
	tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
  	tty.c_cc[VMIN] = 0;
	switch (uartDes->baud_rate)
  	{
  		case 4800:   cfsetispeed(&tty, B4800);   break;
  		case 9600:   cfsetispeed(&tty, B9600);   break;
  		case 19200:  cfsetispeed(&tty, B19200);  break;
  		case 38400:  cfsetispeed(&tty, B38400);  break;
  		case 115200: cfsetispeed(&tty, B115200); 
		
		writeMemo("[i] baud rate supported, using 115200");
					writeErrmsgMemos();
		break;
  		default:	writeMemo("[i] warning: baud rate %u is not supported, using 115200");
					writeErrmsgMemos();
					cfsetispeed(&tty, B115200); break;
  	}
  	// Save tty settings, also checking for error
  	if (tcsetattr(*uartPort, TCSANOW, &tty) != 0) {
		isprintf(mBuf, sizeof(mBuf),"[i] UART Read Error from tcgetattr: %s\n", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
      	return -1;
  	}	
	writeMemo("[i] UART Read Port Open");
	writeErrmsgMemos();
	return 0;
}

static int	openUartPortWrite(int *uartPort,struct uartdescriptor *uartDes)
{

	char	mBuf[1024]; //Debug string buffer
	writeMemo("[i] Opening UartWriteSocket with file descriptor");
	writeErrmsgMemos();
	isprintf(mBuf, sizeof(mBuf),"[i] UartWriteSocket  file descriptor:%s, length:%d",uartDes->uart_file_descriptor,strlen(uartDes->uart_file_descriptor));
	writeMemo(mBuf);
	writeErrmsgMemos();
  	int uart_port = open(uartDes->uart_file_descriptor, O_WRONLY);
	if (uart_port < 0) {
		isprintf(mBuf, sizeof(mBuf),"UART Error while opening device to write: %d  %s", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
        return -1;
	}
	struct termios tty;
	writeMemo("[i] Uart Write Port Opened....");
	writeErrmsgMemos();
	// Reading uart settings
  	if(tcgetattr(uart_port, &tty) != 0) {
		isprintf(mBuf, sizeof(mBuf),"UART Error %d from tcgetattr: %s", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
        return -1;
  	}
	writeMemo("[i] tcgetattr read....");
	writeErrmsgMemos();

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
  	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
  	tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size 
  	tty.c_cflag |= CS8; // 8 bits per byte (most common)
  	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
  	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
	tty.c_lflag &= ~ICANON;
  	tty.c_lflag &= ~ECHO; // Disable echo
  	tty.c_lflag &= ~ECHOE; // Disable erasure
  	tty.c_lflag &= ~ECHONL; // Disable new-line echo
  	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
  	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
  	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
	tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
  	tty.c_cc[VMIN] = 0;
	// Set in/out baud rate to be 9600
	switch (uartDes->baud_rate)
  	{
  		case 4800:   cfsetospeed(&tty, B4800);   break;
  		case 9600:   cfsetospeed(&tty, B9600);   break;
  		case 19200:  cfsetospeed(&tty, B19200);  break;
  		case 38400:  cfsetospeed(&tty, B38400);  break;
  		case 115200: cfsetospeed(&tty, B115200); break;
  		default:	writeMemo("[i] warning: baud rate %u is not supported, using 115200");
					writeErrmsgMemos();
					cfsetospeed(&tty, B115200); break;
  	}
  	// Save tty settings, also checking for error
  	if (tcsetattr(uart_port, TCSANOW, &tty) != 0) {
		isprintf(mBuf, sizeof(mBuf),"[i] UART Error from tcgetattr: %s\n", errno, strerror(errno));
		writeMemo(mBuf);
		writeErrmsgMemos();
      	return -1;
  	}	
	*uartPort=uart_port;
	writeMemo("[i] UART Port Open");
	writeErrmsgMemos();
	return 0;
}


int	sendBytesByUart(int *bundleSocket, char *from, int length,
		struct uartdescriptor *socketName)
{
	int	bytesWritten;
		char	mBuf[1024]; //Debug string buffer

	CHKERR(socketName && bundleSocket && from);
	while (1)	/*	Continue until not interrupted.		*/
	{
		  // Write to UART port
  		bytesWritten = write(*bundleSocket, from, length);
		isprintf(mBuf, sizeof(mBuf),"[i] UART, writing %d bytes to UART...", bytesWritten);
		writeMemo(mBuf);
		writeErrmsgMemos();

		if (bytesWritten < 0)
		{

				*bundleSocket = -1;
			putSysErrmsg("UART CLO write() error on socket", NULL);
		}
		return bytesWritten;

	}
}

int	sendBundleByUart(struct uartdescriptor *socketName, int *bundleSocket,
		unsigned int bundleLength, Object bundleZco,
		unsigned char *buffer)
{
	writeErrmsgMemos();

	Sdr		sdr;
	ZcoReader	reader;
	int		bytesToSend;
	int		bytesSent;

	CHKERR(socketName && bundleSocket && buffer);
	if (bundleLength > UARTCLA_BUFSZ)
	{
		putErrmsg("Bundle is too big for UART CLA.", itoa(bundleLength));
		return -1;
	}

	/*	Connect to CLI as necessary.				*/

	if (*bundleSocket < 0)
	{
		if (openUartPortWrite(bundleSocket,socketName) < 0)
		{
			/*	Treat I/O error as a transient anomaly,
			 *	note incomplete transmission.		*/

			return 0;
		}
	}

	/*	Send the bundle in a single UART datagram.		*/

	sdr = getIonsdr();
	zco_start_transmitting(bundleZco, &reader);
	zco_track_file_offset(&reader);
	CHKERR(sdr_begin_xn(sdr));
	bytesToSend = zco_transmit(sdr, &reader, UARTCLA_BUFSZ, (char *) buffer);
	if (sdr_end_xn(sdr) < 0 || bytesToSend < 0)
	{
		putErrmsg("Can't issue from ZCO.", NULL);
		return -1;
	}
	bytesSent=bytesToSend;
	bytesSent = sendBytesByUart(bundleSocket, (char *) buffer, bytesToSend,socketName);
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

			writeMemo("[i] Lost UART connection to CLI; restart CLO \
when connectivity is restored.");
			return 0;
		}
		else
		{
			/*	Big problem; shut down.			*/

			putErrmsg("Failed to send by UART.", NULL);
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

	return bytesSent;}

/*	*	*	Receiver functions	*	*	*	*/

int	receiveBytesByUart(int *bundleSocket, struct uartdescriptor *socketName,
		char *into, int length)
{
	char	mBuf[1024]; //Debug string buffer
	int *uartPort=bundleSocket;
	if (*uartPort < 0)
	{
		if (openUartPortRead(uartPort,socketName) < 0)
		{
			/*	Treat I/O error as a transient anomaly,
			 *	note incomplete transmission.		*/
			return -1;
		}
		bundleSocket=uartPort;
	}
	int received = 0;
	//writeMemo("[i] UART CLI port open...");
	//	writeErrmsgMemos();
	//	isprintf(mBuf, sizeof(mBuf),"UART Port Nr: %d ",*uartPort);
	//	writeMemo(mBuf);
	//		writeErrmsgMemos();
	//
	while (received < length)
	{
		int r = read(*uartPort, into + received, length - received);
		//isprintf(mBuf, sizeof(mBuf),"r: %d, received:%d, length:%d ",r,received,length);
		//writeMemo(mBuf);
		writeErrmsgMemos();
		if (r < 0)
    	{
			writeMemo("[i] CLI read() error, failed to read from UART port");
			writeErrmsgMemos();      
			isprintf(mBuf, sizeof(mBuf),"[i] UART Error %d from read: %s", errno, strerror(errno));
			writeMemo(mBuf);
			writeErrmsgMemos();
			isprintf(mBuf, sizeof(mBuf),"[i] UART Port Nr: %d ",*uartPort);
			writeMemo(mBuf);
			writeErrmsgMemos();

			return -1;
    	}
    	if (r == 0)
    	{
 	
			if(received>0) // UART read Timeout

			{
				isprintf(mBuf, sizeof(mBuf),"[i] UART Induct Received: %d bytes...",received);
				writeMemo(mBuf);
				writeErrmsgMemos();
				return received;
			}


		}
	    received += r;
		sm_TaskYield();


	}
  	// UART read Buffer full
  	return received;
}

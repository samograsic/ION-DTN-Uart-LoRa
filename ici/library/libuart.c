/*
	libuart.c:	Common library for UART convergence-layer output daemon.

	Author: Samo Grasic (samo@grasic.net), Luleå University of Technology, Sweden

*/
#include "platform.h"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

int parseUartSpec(char *socketSpec,struct uartdescriptor *uart)
{
    const char ch = ',';
	char            *delimiter;
    char            *baudRate;
    char            *socketSpecCopy=strdup(socketSpec);
	delimiter = strchr(socketSpecCopy, ch);	

    baudRate = delimiter + 1;
    if (socketSpecCopy == NULL || *socketSpecCopy == '\0')
    {
            writeMemoNote("[I] UART, Can not parse uart descriptor and baud rate!", socketSpecCopy);	
            return -1;
    }
    int n=strcspn(socketSpecCopy,",");
    *delimiter = '\0';
    strncpy(uart->uart_file_descriptor, socketSpecCopy, n+1);
	if (strlen(uart->uart_file_descriptor) != 0)
    {
  //      writeMemoNote("[I] UART File Descriptor:", uart->uart_file_descriptor);	
    if (strlen(baudRate) != 0)
    {
        uart->baud_rate=atoi(baudRate);
   //     writeMemoNote("[I] UART Baud Rate:", baudRate);	
        return 0;
    }
    else 
    {
        writeMemoNote("[I] UART, Baud rate missing!", socketSpecCopy);	
        return 0;

    }

    }
    writeMemoNote("[I] UART, Can not parse uart descriptor and/or baud rate!", socketSpecCopy);	
    return -1;

}

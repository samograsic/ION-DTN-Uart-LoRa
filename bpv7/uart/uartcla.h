
/*
	libuartcla.h:	Common library for UART convergence-layer output daemon.

	Author: Samo Grasic (samo@grasic.net), Luleå University of Technology, Sweden

*/
#ifndef _UARTCLA_H_
#define _UARTCLA_H_

#include "bpP.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UARTCLA_BUFSZ		((256) - 1)


extern int	sendBytesByUart(int *bundleSocket, char *from, int length,
			struct uartdescriptor *socketName);
extern int	sendBundleByUart(struct uartdescriptor *socketName, int *bundleSocket,
			unsigned int bundleLength, Object bundleZco,
			unsigned char *buffer);
extern int	receiveBytesByUart(int *bundleSocket,
			struct uartdescriptor *socketName,char *into, int length);

#ifdef __cplusplus
}
#endif

#endif	/* _UARTCLA_H */

#ifndef LIB_SIO_H
#define LIB_SIO_H

#include "LibBase.h"

#ifdef __cplusplus
extern "C"
{
#endif

	FORCE_EXPORT void sioPrint_(rtclosure *closure, rtdata data, rtt_t rtt);
	EXPORT_CLOSURE(sioPrint)

	FORCE_EXPORT rtstring *sioRead_(rtclosure *closure, rtt_t *retvalrtt);
	EXPORT_CLOSURE(sioRead)

#ifdef __cplusplus
}
#endif

#endif

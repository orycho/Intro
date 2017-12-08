#ifndef LIB_SIO_H
#define LIB_SIO_H

#include "LibBase.h"

#ifdef __cplusplus
extern "C"
{
#endif
	/// Print input to stdout (convert to string if needed).
	FORCE_EXPORT void sioPrint_(rtclosure *closure, rtdata data, rtt_t rtt);
	EXPORT_CLOSURE(sioPrint)
	/// Read a line of text from stdin.
	FORCE_EXPORT rtstring *sioRead_(rtclosure *closure, rtt_t *retvalrtt);
	EXPORT_CLOSURE(sioRead)
	/// Read an entire UTF-8 text file into a string
	FORCE_EXPORT rtvariant *sioLoadFile_(rtclosure *closure, rtt_t *retvalrtt, rtdata path, rtt_t rtt);
	EXPORT_CLOSURE(sioLoadFile)
	/// Save the data in the second parameter as a string to the file named in the first parameter.
	FORCE_EXPORT rtdata sioSaveFile_(rtclosure *closure, rtt_t *retvalrtt, rtdata path, rtt_t pathrtt, rtdata data, rtt_t datartt);
	EXPORT_CLOSURE(sioSaveFile)

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBBASIC_H
#define LIBBASIC_H

#include "LibBase.h"

#ifdef __cplusplus
extern "C"
{
#endif
	/// Print input to stdout (convert to string if needed).
	FORCE_EXPORT rtdata mathToReal_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt);
	EXPORT_CLOSURE(mathToReal)
	
	FORCE_EXPORT rtdata genNext_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt);
	EXPORT_CLOSURE(genNext)
	
	FORCE_EXPORT rtdata genGet_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt);
	EXPORT_CLOSURE(genGet)
	
#ifdef __cplusplus
}
#endif

#endif
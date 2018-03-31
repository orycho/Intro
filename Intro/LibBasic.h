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

	FORCE_EXPORT rtdata seqSize_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt);
	EXPORT_CLOSURE(seqSize)
	FORCE_EXPORT rtdata seqAppendTo_(rtclosure *closure, rtt_t *retvalrtt, rtdata destseq, rtt_t destseqrtt, rtdata data, rtt_t datartt);
	EXPORT_CLOSURE(seqAppendTo)

#ifdef __cplusplus
}
#endif

#endif
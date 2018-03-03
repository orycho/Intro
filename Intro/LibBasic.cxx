#include "stdafx.h"
#include "LibBasic.h"
#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

rtdata mathToReal_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt)
{
	rtdata retval;
	retval.real=(double)data.integer;
	*retvalrtt = intro::rtt::Real;
	return retval;
}

MKCLOSURE(mathToReal, mathToReal_)

#ifdef __cplusplus
}
#endif

intro::Type integer_type(intro::Type::Integer);
intro::Type real_type(intro::Type::Real);
//intro::VariantType maybe_string_type(&string_type);
intro::FunctionType mathToRealType(&integer_type, &real_type);

REGISTER_MODULE(math)
	EXPORT(L"toReal", "mathToReal", &mathToRealType)
CLOSE_MODULE

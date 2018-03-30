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

rtdata genNext_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt)
{
	*retvalrtt=intro::rtt::Boolean;
	rtgenerator *generator=(rtgenerator*)gen.ptr;
	rtdata retval;
	retval.boolean=callGenerator(generator);
	return retval;
}
MKCLOSURE(genNext, genNext_)

rtdata genGet_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt)
{
	*retvalrtt=intro::rtt::Variant;
	rtgenerator *generator=(rtgenerator*)gen.ptr;
	rtt_t valtype=getResultTypeGenerator(generator);
	rtdata retval;
	if (valtype==intro::rtt::Undefined) retval.ptr=(gcdata*)getNoneVariant();
	else retval.ptr=(gcdata*)allocSomeVariant(getResultGenerator(generator), valtype);
	return retval;
}
MKCLOSURE(genGet, genGet_)

#ifdef __cplusplus
}
#endif

namespace {
intro::Type integer_type(intro::Type::Integer);
intro::Type real_type(intro::Type::Real);
//intro::VariantType maybe_string_type(&string_type);
intro::FunctionType mathToRealType(&integer_type, &real_type);

REGISTER_MODULE(math)
	EXPORT(L"toReal", "mathToReal", &mathToRealType)
CLOSE_MODULE

intro::TypeVariable genvar(L"?a");
intro::Type gentoptype(intro::Type::Generator,&genvar);
intro::Type boolean(intro::Type::Boolean);
intro::VariantType maybe_genvar_type(&genvar);
intro::FunctionType genNextType(&gentoptype, &boolean);
intro::FunctionType genGetType(&gentoptype, &maybe_genvar_type);

REGISTER_MODULE(gen)
	EXPORT(L"next", "genNext", &genNextType)
	EXPORT(L"get", "genGet", &genGetType)
CLOSE_MODULE

}
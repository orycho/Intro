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

rtdata seqSize_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt)
{
	rtdata retval;
	*retvalrtt = intro::rtt::Integer;
	if (rtt == intro::rtt::String)
	{
		rtstring *string = (rtstring*)data.ptr;
		retval.integer = sizeString(string);
	}
	else
	{
		rtlist *list = (rtlist*)data.ptr;
		retval.integer = sizeList(list);
	}
	return retval;
}
MKCLOSURE(seqSize, seqSize_)

// front and back accessors-return maybe?!
// pop_back
// filterFrom (modify passed sequence)
// reverse
rtdata seqAppendTo_(rtclosure *closure, rtt_t *retvalrtt, rtdata destseq, rtt_t destseqrtt, rtdata data, rtt_t datartt)
{
	if (destseqrtt == intro::rtt::String)
	{
		rtstring *src = (rtstring*)data.ptr;
		appendString((rtstring*)destseq.ptr, src->data, src->used);
	}
	else
	{
		rtlist *list=(rtlist*)destseq.ptr;
		// In case of empty list make sure element type is set
		if (getElemTypeList(list)==intro::rtt::Undefined)
			setElemTypeList(list,datartt);
		appendList(list, data);
	}
	*retvalrtt = destseqrtt;
	return destseq;
}
MKCLOSURE(seqAppendTo, seqAppendTo_)

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
	intro::Type gentoptype(intro::Type::Generator, &genvar);
	intro::Type boolean(intro::Type::Boolean);
	intro::VariantType maybe_genvar_type(&genvar);
	intro::FunctionType genNextType(&gentoptype, &boolean);
	intro::FunctionType genGetType(&gentoptype, &maybe_genvar_type);

	REGISTER_MODULE(gen)
		EXPORT(L"next", "genNext", &genNextType)
		EXPORT(L"get", "genGet", &genGetType)
	CLOSE_MODULE

	intro::TypeVariable elemvar(L"?elem");
	intro::Type sequence_type(intro::Type::Sequence, &elemvar);
	intro::FunctionType seqSizeType(&sequence_type, &integer_type);
	intro::FunctionType seqAppendToType(&sequence_type, &elemvar);

	REGISTER_MODULE(seq)
		EXPORT(L"size", "seqSize", &genNextType)
		EXPORT(L"appendTo", "seqAppendTo", &genGetType)
	CLOSE_MODULE
}
#include "stdafx.h"
#include "LibBasic.h"
#include <wchar.h>

std::vector<LibLoader *> LibLoader::instances;

#ifdef __cplusplus
extern "C"
{
#endif

	rtdata mathToReal_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt)
	{
		rtdata retval;
		retval.real = (double)data.integer;
		*retvalrtt = intro::rtt::Real;
		return retval;
	}
	MKCLOSURE(mathToReal, mathToReal_)

	rtdata genNext_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt)
	{
		*retvalrtt = intro::rtt::Boolean;
		rtgenerator *generator = (rtgenerator *)gen.ptr;
		rtdata retval;
		retval.boolean = callGenerator(generator);
		return retval;
	}
	MKCLOSURE(genNext, genNext_)

	rtdata genGet_(rtclosure *closure, rtt_t *retvalrtt, rtdata gen, rtt_t rtt)
	{
		*retvalrtt = intro::rtt::Variant;
		rtgenerator *generator = (rtgenerator *)gen.ptr;
		rtt_t valtype = getResultTypeGenerator(generator);
		rtdata retval;
		if (valtype == intro::rtt::Undefined) retval.ptr = (gcdata *)getNoneVariant();
		else retval.ptr = (gcdata *)allocSomeVariant(getResultGenerator(generator), valtype);
		return retval;
	}
	MKCLOSURE(genGet, genGet_)

	rtdata seqSize_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t rtt)
	{
		rtdata retval;
		*retvalrtt = intro::rtt::Integer;
		if (rtt == intro::rtt::String)
		{
			rtstring *string = (rtstring *)data.ptr;
			retval.integer = sizeString(string);
		}
		else
		{
			rtlist *list = (rtlist *)data.ptr;
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
			rtstring *src = (rtstring *)data.ptr;
			appendString((rtstring *)destseq.ptr, src->data, src->used);
		}
		else
		{
			rtlist *list = (rtlist *)destseq.ptr;
			// In case of empty list make sure element> type is set
			if (getElemTypeList(list) == intro::rtt::Undefined)
				setElemTypeList(list, datartt);
			appendList(list, data);
		}
		*retvalrtt = destseqrtt;
		return destseq;
	}
	MKCLOSURE(seqAppendTo, seqAppendTo_)

	// front and back accessors-return maybe?!
	// pop_back
	// filterFrom (modify passed sequence)
	// reverse
	rtdata seqFirst_(rtclosure *closure, rtt_t *retvalrtt, rtdata data, rtt_t datartt)
	{
		rtdata retval;
		if (datartt == intro::rtt::String)
		{
			rtstring *src = (rtstring *)data.ptr;
			if (src->used == 0)
				retval.ptr = (gcdata *)getNoneVariant();
			else
			{
				rtstring *chr = allocString(1);
				appendString(chr, src->data, 1);
				rtdata buf;
				buf.ptr = (gcdata *)chr;
				retval.ptr = (gcdata *)allocSomeVariant(buf, intro::rtt::String);
			}
		}
		else
		{
			rtlist *src = (rtlist *)data.ptr;
			if (src->used == 0)
				retval.ptr = (gcdata *)getNoneVariant();
			else
			{
				increment(src->data[0], src->elem_type);
				retval.ptr = (gcdata *)allocSomeVariant(src->data[0], src->elem_type);
			}
		}
		*retvalrtt = intro::rtt::Variant;
		return retval;
	}
	MKCLOSURE(seqFirst, seqFirst_)

	////////////////////////////////
	//
	// WARNING: Before continuing, read the section on goto in "Code Complete"!!!!
	// You're been warned, rage is pointless :)
	//
	////////////////////////////////


	rtdata stringTrimLeft_(rtclosure *closure, rtt_t *retvalrtt, rtdata str1, rtt_t str1rtt, rtdata str2, rtt_t str2rtt)
	{
		rtstring *s1 = (rtstring *)str1.ptr, *s2 = (rtstring *)str2.ptr;
		size_t first = 0;
		while (first < s1->used)
		{
			for (size_t i = 0; i < s2->used; ++i)
				if (s1->data[first] == s2->data[i]) goto first_iter;
			break;
		first_iter:
			++first;
		}

		rtstring *result = allocString(s1->used - first);
		appendString(result, &(s1->data[first]), s1->used - first);
		rtdata retval;
		retval.ptr = (gcdata *)result;
		*retvalrtt = intro::rtt::String;
		return retval;
	}
	MKCLOSURE(stringTrimLeft, stringTrimLeft_)

	rtdata stringTrimRight_(rtclosure *closure, rtt_t *retvalrtt, rtdata str1, rtt_t str1rtt, rtdata str2, rtt_t str2rtt)
	{
		rtstring *s1 = (rtstring *)str1.ptr, *s2 = (rtstring *)str2.ptr;
		size_t last = s1->used - 1;
		while (last < s1->used)
		{
			for (size_t i = 0; i < s2->used; ++i)
				if (s1->data[last] == s2->data[i]) goto last_iter;
			++last;
			break;
		last_iter:
			--last;
		}
		if (last > s1->used) last = 0;
		rtstring *result = allocString(last);
		if (last != 0) appendString(result, s1->data, last);
		rtdata retval;
		retval.ptr = (gcdata *)result;
		*retvalrtt = intro::rtt::String;
		return retval;
	}
	MKCLOSURE(stringTrimRight, stringTrimRight_)

	rtdata stringTrim_(rtclosure *closure, rtt_t *retvalrtt, rtdata str1, rtt_t str1rtt, rtdata str2, rtt_t str2rtt)
	{
		rtstring *s1 = (rtstring *)str1.ptr, *s2 = (rtstring *)str2.ptr;
		size_t first = 0;
		while (first < s1->used)
		{
			for (size_t i = 0; i < s2->used; ++i)
				if (s1->data[first] == s2->data[i]) goto first_iter;
			break;
		first_iter:
			++first;
		}

		size_t last = s1->used - 1;
		while (last < s1->used)
		{
			bool match = false;
			for (size_t i = 0; i < s2->used; ++i)
				if (s1->data[last] == s2->data[i]) goto last_iter;
			break;
		last_iter:
			--last;
		}

		rtstring *result = allocString(last - first + 1);
		appendString(result, &(s1->data[first]), last - first + 1);
		rtdata retval;
		retval.ptr = (gcdata *)result;
		*retvalrtt = intro::rtt::String;
		return retval;
	}
	MKCLOSURE(stringTrim, stringTrim_)

#ifdef __cplusplus
}
#endif

namespace {
	intro::Type::pointer_t integer_type = std::make_shared<intro::Type>(intro::Type::Integer);
	intro::Type::pointer_t real_type = std::make_shared<intro::Type>(intro::Type::Real);
	intro::Type::pointer_t mathToRealType = intro::Type::pointer_t(new intro::FunctionType(integer_type, real_type));

	REGISTER_MODULE(math)
		EXPORT(L"toReal", "mathToReal", mathToRealType)
	CLOSE_MODULE

	intro::Type::pointer_t genvar = intro::Type::pointer_t(new intro::TypeVariable(L"?value"));
	intro::Type::pointer_t gentoptype = intro::Type::pointer_t(new intro::Type(intro::Type::Generator, genvar));
	intro::Type::pointer_t boolean = intro::Type::pointer_t(new intro::Type(intro::Type::Boolean));
	intro::Type::pointer_t maybe_genvar_type = intro::Type::pointer_t(new intro::VariantType(genvar));
	intro::Type::pointer_t genNextType = intro::Type::pointer_t(new intro::FunctionType(gentoptype, boolean));
	intro::Type::pointer_t genGetType = intro::Type::pointer_t(new intro::FunctionType(gentoptype, maybe_genvar_type));

	REGISTER_MODULE(gen)
		EXPORT(L"next", "genNext", genNextType)
		EXPORT(L"get", "genGet", genGetType)
	CLOSE_MODULE

	intro::Type::pointer_t unit_type = intro::Type::pointer_t(new intro::Type(intro::Type::Unit));
	intro::Type::pointer_t elemvar = intro::Type::pointer_t(new intro::TypeVariable(L"?element"));
	intro::Type::pointer_t sequence_type = intro::Type::pointer_t(new intro::Type(intro::Type::Sequence, elemvar));
	intro::Type::pointer_t seqSizeType = intro::Type::pointer_t(new intro::FunctionType(sequence_type, integer_type));
	intro::Type::pointer_t seqAppendToType = intro::Type::pointer_t(new intro::FunctionType(sequence_type, elemvar, sequence_type));
	intro::Type::pointer_t maybe_elem_type = intro::Type::pointer_t(new intro::VariantType(elemvar));
	intro::Type::pointer_t seqFirstType = intro::Type::pointer_t(new intro::FunctionType(sequence_type, maybe_elem_type));

	REGISTER_MODULE(seq)
		EXPORT(L"size", "seqSize", seqSizeType)
		EXPORT(L"appendTo", "seqAppendTo", seqAppendToType)
		EXPORT(L"first", "seqFirst", seqFirstType)
	CLOSE_MODULE

	intro::Type::pointer_t  string_type = intro::Type::pointer_t(new intro::Type(intro::Type::String));
intro::Type::pointer_t  stringTransform = intro::Type::pointer_t(new intro::FunctionType(string_type, string_type, string_type));
	REGISTER_MODULE(string)
		EXPORT(L"trimLeft", "stringTrimLeft", stringTransform)
		EXPORT(L"trimRight", "stringTrimRight", stringTransform)
		EXPORT(L"trim", "stringTrim", stringTransform)
	CLOSE_MODULE
}
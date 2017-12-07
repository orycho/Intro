#include "stdafx.h"
#include "LibSIO.h"

#ifdef __cplusplus
extern "C"
{
#endif

void sioPrint_(rtclosure *closure, rtdata data, rtt_t rtt)
{
	// Be smart and avoid string allocation overhead on common case of getting a string passed in.
	if (rtt != intro::rtt::String)
	{
		rtstring *str = allocString(0);
		toStringPoly(str, data, rtt);
		fputws(str->data, stdout);
		freeString(str);
	}
	else
	{
		rtstring *str = (rtstring*)data.ptr;
		fputws(str->data, stdout);
	}
}

MKCLOSURE(sioPrint, sioPrint_)

rtstring *sioRead_(rtclosure *closure, rtt_t *retvalrtt)
{
	rtstring *str = allocString(8);
	wchar_t buffer[128];
	wchar_t *ret = nullptr;
	size_t length = 0;
	do
	{
		ret = fgetws(buffer, 128, stdin);
		if (ret != nullptr)
		{
			length = wcslen(ret);
			appendString(str, ret, length);
		}
		else length = 0;
	} while (ret != nullptr && ret[length - 1] != L'\n');
	*retvalrtt = intro::rtt::String;
	return str;
}

MKCLOSURE(sioRead, sioRead_)

#ifdef __cplusplus
}
#endif

intro::Type unit_type(intro::Type::Unit);
intro::Type top_type(intro::Type::Top);
intro::Type string_type(intro::Type::String);
intro::FunctionType sioPrintType(&top_type, &unit_type);
intro::FunctionType sioReadType(&string_type);

REGISTER_MODULE(sio)
	{L"print", "sioPrint", &sioPrintType},
	{L"read","sioRead", &sioReadType }
CLOSE_MODULE

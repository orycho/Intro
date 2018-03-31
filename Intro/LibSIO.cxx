#include "stdafx.h"
#include "LibSIO.h"
#include <wchar.h>

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

rtvariant *sioLoadFile_(rtclosure *closure, rtt_t *retvalrtt, rtdata path, rtt_t rtt)
{
	*retvalrtt = intro::rtt::Variant;
	rtstring *strbuf = (rtstring *)path.ptr;
	char *cpath = new char[(strbuf->used + 1)*MB_CUR_MAX];
	size_t count = wcstombs(cpath, strbuf->data, strbuf->used);
	cpath[count] = '\0'; // needed!
	FILE *file = fopen(cpath, "r");
	delete[] cpath;
	// If the file could not be opened, return None variant
	if (file == nullptr)
		return getNoneVariant();
	// Get file length
	fseek(file, 0, SEEK_END);
	long fileLen = ftell(file);
	fseek(file, 0, SEEK_SET);
	// Allocate buffer and read file contents
	char *buffer = new char[fileLen];
	fread(buffer, sizeof(char), fileLen, file);
	fclose(file);
	// allocate result string and convert from multibyte into it
	strbuf = allocString((uint64_t)fileLen+1);
	setlocale(LC_ALL, "en_US.utf8");
	mbstate_t state = std::mbstate_t();
	char *p_in = buffer, *end = buffer + fileLen;
	wchar_t *p_out = strbuf->data;
	int rc;
	while ((rc = mbrtowc(p_out, p_in, end - p_in, &state)) > 0)
	{
		p_in += rc;
		++p_out;
	}
	// Strings never count the terminating 0 char, but always alocate space for it!
	strbuf->used = p_out - strbuf->data;
	strbuf->data[strbuf->used+1] = L'\0';
	// cleanup and return
	delete[] buffer;
	rtdata retbuf;
	retbuf.ptr = &(strbuf->gc);
	return allocSomeVariant(retbuf, intro::rtt::String);
}
MKCLOSURE(sioLoadFile, sioLoadFile_)

rtdata sioSaveFile_(rtclosure *closure, rtt_t *retvalrtt, rtdata path, rtt_t pathrtt, rtdata data, rtt_t datartt)
{
	*retvalrtt = intro::rtt::Boolean;
	rtdata rtbuf;
	// OPen file for writing
	rtstring *str = (rtstring*)path.ptr;
	char *cpath = new char[MB_CUR_MAX * (str->used + 1)];
	size_t count = wcstombs(cpath, str->data, str->used);
	cpath[count] = '\0'; // needed!
	FILE *file = fopen(cpath, "w");
	delete[] cpath;
	if (file == nullptr)
	{
		rtbuf.boolean = false;
		return rtbuf;
	}
	// convert data to string
	if (datartt == intro::rtt::String)
	{
		str = (rtstring*)data.ptr;
		increment(data, intro::rtt::String);
	}
	else
	{
		str = allocString(0);
		toStringPoly(str, data, datartt);
	}
	// convert to multibyte and write to file, copied 
	// from http://en.cppreference.com/w/c/string/multibyte/c16rtomb
	setlocale(LC_ALL, "en_US.utf8");
	mbstate_t state = std::mbstate_t();
	char *out = new char[MB_CUR_MAX * str->used];
	char *p = out;
	for (size_t n = 0; n < str->used; ++n)
	{
		size_t rc = wcrtomb(p, str->data[n], &state);
		if (rc == (size_t)-1) break;
		p += rc;
	}
	size_t out_sz = p - out;
	fwrite(out, sizeof(char), out_sz, file);
	// cleanup and exit
	fclose(file);
	delete[] out;
	rtbuf.ptr = &(str->gc);
	decrement(rtbuf, intro::rtt::String);
	rtbuf.boolean = true;
	return rtbuf;
}
MKCLOSURE(sioSaveFile, sioSaveFile_)

#ifdef __cplusplus
}
#endif

namespace {
	intro::Type unit_type(intro::Type::Unit);
	intro::Type top_type(intro::Type::Top);
	intro::Type string_type(intro::Type::String);
	intro::Type bool_type(intro::Type::Boolean);
	intro::VariantType maybe_string_type(&string_type);
	intro::FunctionType sioPrintType(&top_type, &unit_type);
	intro::FunctionType sioReadType(&string_type);
	intro::FunctionType sioLoadFileType(&string_type, &maybe_string_type);
	intro::FunctionType sioSaveFileType(&string_type, &top_type, &bool_type);

	REGISTER_MODULE(sio)
		EXPORT(L"print", "sioPrint", &sioPrintType)
		EXPORT(L"read", "sioRead", &sioReadType)
		EXPORT(L"loadFile", "sioLoadFile", &sioLoadFileType)
		EXPORT(L"saveFile", "sioSaveFile", &sioSaveFileType)
	CLOSE_MODULE
}
// FormatMessage(FORMAT_MESSAGE_FROM_STRING) for Linux. The games' .rc strings use its
// inserts (%1, %1!s!, %2!d!), which vsprintf printed verbatim.
// Handles %N and %N!spec! (d i u x X o c s, with flags, width, precision and h/l/ll/I64),
// %0, %n %r %t, and %<other> as the character. Floating point prints the letter alone,
// as FormatMessage does. Arguments are read in index order with their first spec's type

#ifndef __FMTMESSAGE_H__
#define __FMTMESSAGE_H__

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

namespace fmtmessage
{
	enum ArgKind { kNone, kString, kInt, kLong, kLongLong, kDouble };

	struct Insert
	{
		std::string	spec;	// printf spec without the '%', e.g. "s", "5d"
		ArgKind		kind;
	};

	// Parses "%N!spec!" or "%N" just past the '%'. Returns the index (1-99), or 0 if not an insert
	inline int ParseInsert(const char *&p, std::string &spec)
	{
		if (*p < '1' || *p > '9')
			return 0;
		int n = *p++ - '0';
		if (*p >= '0' && *p <= '9')
			n = n * 10 + (*p++ - '0');
		spec = "s";
		if (*p == '!')
		{
			const char *pEnd = strchr(p + 1, '!');
			if (pEnd)
			{
				spec.assign(p + 1, pEnd - (p + 1));
				p = pEnd + 1;
			}
		}
		return n;
	}

	inline ArgKind KindOf(const std::string &spec)
	{
		if (spec.empty())
			return kString;
		char c = spec[spec.size() - 1];
		bool bLL = spec.find("ll") != std::string::npos || spec.find("I64") != std::string::npos;
		bool bL  = !bLL && spec.find('l') != std::string::npos;
		switch (c)
		{
			case 's': case 'S':									return kString;
			case 'f': case 'e': case 'E': case 'g': case 'G':	return kDouble;
			case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c':
				return bLL ? kLongLong : (bL ? kLong : kInt);
		}
		return kString;
	}

	// The printf spec with FormatMessage's I64 spelled the C way.
	inline std::string CSpec(const std::string &spec)
	{
		std::string s = spec;
		size_t i = s.find("I64");
		if (i != std::string::npos)
			s.replace(i, 3, "ll");
		return "%" + s;
	}

	inline std::string Format(const char *pFormat, va_list args)
	{
		// Pass 1: which inserts exist, and each one's type.
		Insert aInserts[100];
		int nMax = 0;
		for (int i = 0; i < 100; ++i)
			aInserts[i].kind = kNone;
		for (const char *p = pFormat; *p; )
		{
			if (*p++ != '%')
				continue;
			std::string spec;
			int n = ParseInsert(p, spec);
			if (n == 0)
			{
				if (*p) ++p;
				continue;
			}
			if (aInserts[n].kind == kNone)
			{
				aInserts[n].spec = spec;
				aInserts[n].kind = KindOf(spec);
			}
			if (n > nMax)
				nMax = n;
		}

		// Read in index order. A skipped index reads as a pointer, every 64 bit vararg's size
		union Value { const char *s; int i; long l; long long ll; double d; };
		Value aValues[100];
		for (int n = 1; n <= nMax; ++n)
		{
			switch (aInserts[n].kind)
			{
				case kInt:		aValues[n].i  = va_arg(args, int);			break;
				case kLong:		aValues[n].l  = va_arg(args, long);			break;
				case kLongLong:	aValues[n].ll = va_arg(args, long long);	break;
				case kDouble:	aValues[n].d  = va_arg(args, double);		break;
				default:		aValues[n].s  = va_arg(args, const char*);	break;
			}
		}

		// Pass 2: render.
		std::string out;
		char aBuf[512];
		for (const char *p = pFormat; *p; )
		{
			char c = *p++;
			if (c != '%')
			{
				out += c;
				continue;
			}
			std::string spec;
			int n = ParseInsert(p, spec);
			if (n)
			{
				// This occurrence's own spec, read with the first one's type.
				// No floating point: the conversion letter alone
				const std::string cspec = CSpec(spec);
				if (aInserts[n].kind == kDouble)
				{
					out += spec[spec.size() - 1];
					continue;
				}
				switch (aInserts[n].kind)
				{
					case kInt:		snprintf(aBuf, sizeof(aBuf), cspec.c_str(), aValues[n].i);	break;
					case kLong:		snprintf(aBuf, sizeof(aBuf), cspec.c_str(), aValues[n].l);	break;
					case kLongLong:	snprintf(aBuf, sizeof(aBuf), cspec.c_str(), aValues[n].ll);	break;
					case kDouble:	snprintf(aBuf, sizeof(aBuf), cspec.c_str(), aValues[n].d);	break;
					default:
						snprintf(aBuf, sizeof(aBuf), cspec.c_str(), aValues[n].s ? aValues[n].s : "");
						break;
				}
				out += aBuf;
				continue;
			}
			c = *p;
			if (!c)
				break;
			++p;
			switch (c)
			{
				case '0':	return out;
				case 'n':	out += "\r\n";	break;	// FormatMessage's hard break
				case 'r':	out += '\r';	break;
				case 't':	out += '\t';	break;
				default:	out += c;		break;	// %% %! %. %space and the rest
			}
		}
		return out;
	}
}

#endif

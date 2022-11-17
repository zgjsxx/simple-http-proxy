#ifndef __SS_HEX_CODER_H__
#define __SS_HEX_CODER_H__

#include <vector>
#include <stddef.h>
#include <string>
#include <string.h>

class HexCoder
{
public:
	static std::string encode(const char * stuff, size_t len);
	static std::string encode(const char * stuff, size_t len, char sep);

	static bool decode(const char * stuff, std::vector<char> & output);

	static int decodeChar(const char * stuff);

private:
	static const char ENC_TABLE[16];

	static void encode(std::string & output, size_t idx, unsigned int bt);
};

namespace ssdefs
{

const int UTF8BOM[3] =
		{ 0xEF, 0xBB, 0xBf };

const char SPACE = ' ';
const char TAB = '\t';
const char LF = '\n';
const char CR = '\r';
const char BACKSLASH = '\\';
const char POUND = '#';
const char EQUALS = '=';
const char DASH = '-';
const char UNDERSCORE = '_';
const char DOT = '.';
const char BRACKET_L = '[';
const char BRACKET_R = ']';
const char BRACE_L = '{';
const char BRACE_R = '}';
const char COLON = ':';
const char SEMICOLON = ';';
const char K = 'K';
const char M = 'M';
};


#endif // __SS_HEX_CODER_H__

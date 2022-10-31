#include "HexCoder.h"

const char HexCoder::ENC_TABLE[] =
{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

std::string HexCoder::encode(const char * stuff, size_t len)
{
	if (len == 0 || stuff == NULL)
	{
		return "";
	}

	std::string output;
	output.resize(len * 2);

	for (size_t ii = 0; ii < len; ii++)
	{
		encode(output, ii * 2, (unsigned int) stuff[ii]);
	}

	return output.c_str();
}

std::string HexCoder::encode(const char * stuff, size_t len, char sep)
{
	if (len == 0 || stuff == NULL)
	{
		return "";
	}

	std::string output;
	output.resize(len * 3);

	for (size_t ii = 0; ii < len; ii++)
	{
		encode(output, ii * 3, (unsigned int) stuff[ii]);
		output[ii * 3 + 2] = sep;
	}

	output[output.size() - 1] = 0;

	return output.c_str();
}

void HexCoder::encode(std::string & output, size_t idx, unsigned int bt)
{
	output[idx] = ENC_TABLE[0x0f & (bt >> 4)];
	output[idx + 1] = ENC_TABLE[0x0f & bt];
}

static int getCharValue(int ch)
{
	if ('0' <= ch && '9' >= ch)
	{
		return ch - '0';
	}
	else if ('a' <= ch && 'f' >= ch)
	{
		return 10 + (ch - 'a');
	}
	else if ('A' <= ch && 'F' >= ch)
	{
		return 10 + (ch - 'A');
	}
	return -1;
}

bool HexCoder::decode(const char * stuff, std::vector<char> & output)
{
	if (stuff == NULL)
	{
		output.resize(0);
		return false;
	}

	size_t len = strlen(stuff);

	size_t outCursor = 0;
	output.resize(len);

	for (size_t ii = 0; ii < len; ii++)
	{
		switch (stuff[ii])
		{
		case ssdefs::SPACE:
		case ssdefs::CR:
		case ssdefs::LF:
		case ssdefs::TAB:
			// -- ignore whitespace
			break;

		default:
			if ((ii + 2) > len)
			{
				break;
			}

			int ch = decodeChar(stuff + ii);
			if (-1 == ch)
			{
				break;
			}

			output[outCursor] = (char) ch;
			outCursor++;
			ii++; // we used up 2 chars
			break;
		}
	}

	output.resize(outCursor);
	return true;
}

int HexCoder::decodeChar(const char * stuff)
{
	int ch1 = getCharValue(stuff[0]);
	int ch2 = getCharValue(stuff[1]);

	if (-1 == ch1 || -1 == ch2)
	{
		return -1;
	}

	return (ch1 << 4) | ch2;
}

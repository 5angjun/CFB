#include "Utils.h"


/**
 *
 * adapted from base64.cpp and base64.h by Ren� Nyffenegger
 *
 */

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) 
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}


std::string Utils::base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) 
{
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';

	}

	return ret;

}


const byte* Utils::base64_decode(std::string const& encoded_string)
{
	size_t in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) 
	{
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) 
		{
			for (i = 0; i < 4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]) & 0xff;

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) 
	{
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]) & 0xff;

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
	}

	return (const byte*)(ret.c_str());
}



std::vector<std::string> Utils::EnumerateDriversFromRoot()
{
	DWORD cbNeeded=0, dwSize=0, dwNbDrivers=0;

	BOOL bRes = ::EnumDeviceDrivers(NULL, 0, &cbNeeded);
	if (!bRes)
	{
		if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			throw std::runtime_error("Unexpected result from EnumDeviceDrivers1");
	}

	dwSize = cbNeeded;

	LPVOID* lpDrivers = (LPVOID*)::LocalAlloc(LPTR, dwSize);
	if(!lpDrivers)
		throw std::runtime_error("LocalAlloc");

	bRes = ::EnumDeviceDrivers(lpDrivers, dwSize, &cbNeeded);

	if (!bRes || dwSize != cbNeeded)
	{
		::LocalFree(lpDrivers);
		throw std::runtime_error("EnumDeviceDrivers2 failed");
	}


	std::vector<std::string> DriverList;
	CHAR wszDriverBaseName[MAX_PATH] = { 0, };
	dwNbDrivers = dwSize / sizeof(LPVOID);

	for (DWORD i = 0; i < dwNbDrivers; i++)
	{
		memset(wszDriverBaseName, 0, sizeof(wszDriverBaseName));

		if (::GetDeviceDriverBaseNameA(lpDrivers[i], wszDriverBaseName, _countof(wszDriverBaseName)))
			DriverList.push_back(std::string(wszDriverBaseName));
	}

	::LocalFree(lpDrivers);

	return DriverList;
}


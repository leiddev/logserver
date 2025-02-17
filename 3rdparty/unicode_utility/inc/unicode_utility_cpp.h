/*
 * Leid 2023.8
 */
#ifndef _UNICODE_UTILITY_CPP_H
#define _UNICODE_UTILITY_CPP_H

#include <string>

namespace UnicodeUtility {

std::string UTF8ToGB2312(std::string &utf8_str);
std::string UTF8ToGB2312(unsigned char *utf8_str, unsigned int len);

std::string GB2312ToUTF8(std::string &gb2312_str);
std::string GB2312ToUTF8(unsigned char *gb2312_str, unsigned int len);

} // namespace UnicodeUtility

#endif // !_UNICODE_UTILITY_CPP_H
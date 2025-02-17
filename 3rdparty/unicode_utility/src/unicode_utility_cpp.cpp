/*
 * Leid 2023.8
 */
#include "unicode_utility_cpp.h"
#include "unicode_utility.h"
#include <string>
#include <string.h>

namespace UnicodeUtility {

std::string UTF8ToGB2312(std::string &utf8_str) {
    unsigned char *gb2312_buffer = new unsigned char[utf8_str.size() + 1];

    if (nullptr == gb2312_buffer) {
      return std::string();
    }

    int ret = UTF8ToGB2312Str((unsigned char *)utf8_str.c_str(), gb2312_buffer, utf8_str.size() + 1);
    if (ret > 0) {
        std::string gb2312_str((char *)gb2312_buffer);
        delete[] gb2312_buffer;
        return gb2312_str;
    } else {
        delete[] gb2312_buffer;
        return std::string();
    }
}

std::string UTF8ToGB2312(unsigned char *utf8_str, unsigned int len) {
    unsigned char *gb2312_buffer = new unsigned char[len + 1];
    unsigned char *utf8_buffer = new unsigned char[len + 1];
    
    if ((nullptr == gb2312_buffer) || (nullptr == utf8_buffer)) {
        return std::string();
    }

    memcpy(utf8_buffer, utf8_str, len);
    utf8_buffer[len] = '\0';

    int ret = UTF8ToGB2312Str(utf8_buffer, gb2312_buffer, len + 1);
    if (ret > 0) {
        std::string gb2312_str((char *)gb2312_buffer);
        delete[] gb2312_buffer;
        delete[] utf8_buffer;
        return gb2312_str;
    } else {
        delete[] gb2312_buffer;
        delete[] utf8_buffer;
        return std::string();
    }
}

std::string GB2312ToUTF8(std::string &gb2312_str) {
    unsigned char *utf8_buffer = new unsigned char[gb2312_str.size() * 2 + 1];

    if (nullptr == utf8_buffer) {
        return std::string();
    }

    int ret = GB2312ToUTF8Str((unsigned char *)gb2312_str.c_str(), utf8_buffer, gb2312_str.size() * 2 + 1);
    if (ret > 0) {
        std::string utf8_str((char *)utf8_buffer);
        delete[] utf8_buffer;
        return utf8_str;
    } else {
        delete[] utf8_buffer;
        return std::string();
    }
}

std::string GB2312ToUTF8(unsigned char *gb2312_str, unsigned int len) {
    unsigned char *utf8_buffer = new unsigned char[len * 2 + 1];
    unsigned char *gb2312_buffer = new unsigned char[len + 1];

    if ((nullptr == gb2312_buffer) || (nullptr == utf8_buffer)) {
        return std::string();
    }

    memcpy(gb2312_buffer, gb2312_str, len);
    gb2312_buffer[len] = '\0';

    int ret = GB2312ToUTF8Str(gb2312_buffer, utf8_buffer, len * 2 + 1);
    if (ret > 0) {
        std::string utf8_str((char *)utf8_buffer);
        delete[] gb2312_buffer;
        delete[] utf8_buffer;
        return utf8_str;
    } else {
        delete[] gb2312_buffer;
        delete[] utf8_buffer;
        return std::string();
    }
}

} // namespace UnicodeUtility


/*
 * Leid 2023.8
 */
#include "unicode_utility_cpp.h"
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

static void xdump(const void * data, size_t length);
template<typename T>
static void xdump(T x);

int main() {
    std::string utf8_str;
    std::string gb2312_str;

    std::cout << "Test begin." << std::endl;
    utf8_str = "\xE6\x88\x91\xE4\xBB\xAC\x31\x32\x33\xE5\xA5\xB9\xE4\xBB\xAC\x34\x35\x36\xFF\xFF\x20\x37\x38\x39";  // 我们123她们456\xFF\xFF 789
    xdump(utf8_str.c_str(), utf8_str.size() + 1);

    printf("Test 1:\n");
    gb2312_str = UnicodeUtility::UTF8ToGB2312(utf8_str);
    std::cout << "GB2312: " << gb2312_str << std::endl;
    xdump(gb2312_str.c_str(), gb2312_str.size() + 1);

    printf("Test 2:\n");
    utf8_str = UnicodeUtility::GB2312ToUTF8(gb2312_str);
    std::cout << "UTF8: " << utf8_str << std::endl;
    xdump(utf8_str.c_str(), utf8_str.size() + 1);

    printf("Test 3:\n");
    gb2312_str = UnicodeUtility::UTF8ToGB2312((uint8_t *)utf8_str.c_str(), 8);
    std::cout << "GB2312: " << gb2312_str << std::endl;
    xdump(gb2312_str.c_str(), gb2312_str.size() + 1);

    printf("Test 4:\n");
    utf8_str = UnicodeUtility::GB2312ToUTF8((uint8_t *)gb2312_str.c_str(), 5);
    std::cout << "UTF8: " << utf8_str << std::endl;
    xdump(utf8_str.c_str(), utf8_str.size() + 1);

    printf("Test end.\n");
    return 0;
}

static void xdump(const void * data, size_t length) {
    const unsigned char * const cp = (const unsigned char*)data;
    std::stringstream res;

    for (int i = 0; i < length; i++) {
        if (i)
            res << ' ';
        res << std::setfill('0') << std::setw(2) << std::hex << (int)cp[i];
    }
    std::cout << res.str() << std::endl;
}

template<typename T>
static void xdump(T x) {
    std::stringstream res;
    auto p = reinterpret_cast<const unsigned char*>(&x);
    for (int i = 0; i < sizeof(x); ++i) {
        if (i)
            res << ' ';
        res << std::setfill('0') << std::setw(2) << std::hex << (int)p[i];
    }
    std::cout << res.str() << std::endl;
}

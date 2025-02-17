/*
 * Leid 2023.8
 */
#ifndef _UNICODE_UTILITY_H_
#define _UNICODE_UTILITY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief utf8编码转unicode字符集(usc4)，最大支持4字节utf8编码，(4字节以上在中日韩文中为生僻字)
 * @param *utf8 utf8变长编码字节集1~4个字节
 * @param *unicode utf8编码转unicode字符集结果，最大4个字节，返回的字节序与utf8编码序一致
 * @return length 0：utf8解码异常，others：本次utf8编码长度
 */
int32_t UTF8ToUnicode(uint8_t *utf8, uint32_t *unicode);

/** 
 * UnicodeToUTF8 - convert unicode char to UTF-8 char 
 * @param *Unicodevalue: a UNICODE character 
 * @param *utf8dst: a buffer to contain a utf-8 characters 
 * 
 * @return number of bytes for the utf-8 sequence, supports up to 4 bytes of UTF8, returns -1 if require more than 4 bytes.
 */ 
int UnicodeToUTF8(unsigned long Unicodevalue, unsigned char *utf8dst);


/**
 * @brief 4字节unicode(usc4)字符集转utf16编码
 * @param unicode unicode字符值
 * @param *utf16 utf16编码结果
 * @return utf16长度，(2字节)单位
 */
int32_t UnicodeToUTF16(uint32_t unicode, uint16_t *utf16);


/** 
 * UTF16toUnicode - Converts UTF16 multiword character to Unicode, supports up to 4 bytes of UTF8. 
 * @param *utf16MW: UTF16 multiword character
 * @param *outunicode: a unicode character output 
 * 
 * @return the number of words this UTF16 character was composed of. Returns -1 if utf16MW=NULL.
 */ 
int UTF16toUnicode(unsigned short *utf16MW, unsigned int *outunicode);


/**
 * @brief 2字节utf16编码转gb2312编码
 * @param utf16 utf16编码
 * @param *gb2812 gb2312编码结果
 * @return gb2812长度，字节为单位
 */
int32_t UTF16ToGB2312(uint16_t utf16, uint16_t *gb2812);


/**
 * @brief gb2312编码转2字节utf16编码
 * @param utf16 utf16编码
 * @param *gb2812 gb2312编码结果
 * @return gb2812长度，字节为单位
 */
int32_t GB2312ToUTF16(uint16_t gb2312, uint16_t *utf16);


/**
 * @brief utf8编码转gb2312编码
 * @param *utf8 utf8变长编码字节集(1~4个字节)
 * @param *out_utf8_size 输出当前转换utf8字符的utf8编码长度(1~4个字节)
 * @param *gb2812 gb2312编码结果
 * @return gb2812长度，字节为单位
 */
int32_t UTF8ToGB2312(uint8_t *utf8, uint32_t *out_utf8_size, uint16_t *gb2812);


/**
 * @brief gb2312编码转utf8编码
 * @param *gb2312 gb2312编码字节集(1~2个字节)
 * @param *out_gb2312_size 输出当前转换gb2312字符的gb2312编码长度(1-2个字节)
 * @param *utf8 utf8编码结果
 * @return utf8长度，字节为单位
 */
int32_t GB2312ToUTF8(uint16_t *gb2312, uint32_t *out_gb2312_size, uint8_t *utf8);


/**
 * @brief utf8编码字符串转gb2312编码字符串
 * @param *utf8_str utf8编码字符串(\0结尾)
 * @param *gb2312_str 输出gb2312编码字符串(\0结尾)
 * @param *gb2312_str_buff_size 输出缓冲区大小
 * @return gb2312编码字符串长度，字节为单位
 */
int32_t UTF8ToGB2312Str(uint8_t *utf8_str, uint8_t *gb2312_str, uint32_t gb2312_str_buff_size);


/**
 * @brief gb2312编码字符串转utf8编码字符串
 * @param *gb2312 gb2312编码字符串(\0结尾)
 * @param *utf8 输出utf8编码字符串(\0结尾)
 * @param *utf8_str_buff_size 输出缓冲区大小
 * @return utf8编码字符串长度，字节为单位
 */
int32_t GB2312ToUTF8Str(uint8_t *gb2312_str, uint8_t *utf8_str, uint32_t utf8_str_buff_size);


#ifdef __cplusplus
}
#endif

#endif  // !_UNICODE_UTILITY_H_

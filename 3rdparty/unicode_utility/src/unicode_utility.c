/*
 * Leid 2023.8
 */
#include <stdint.h>
#include <string.h>

#include "utf16_to_gb2312_table.h"

/*
Unicode符号范围     |        UTF-8编码方式
(十六进制)        |              （二进制）
----------------------+---------------------------------------------
0000 0000-0000 007F | 0xxxxxxx
0000 0080-0000 07FF | 110xxxxx 10xxxxxx
0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

/**
 * @brief utf8编码转unicode字符集(usc4)，最大支持4字节utf8编码，(4字节以上在中日韩文中为生僻字)
 * @param *utf8 utf8变长编码字节集1~4个字节
 * @param *unicode utf8编码转unicode字符集结果，最大4个字节，返回的字节序与utf8编码序一致
 * @return length 0：utf8解码异常，others：本次utf8编码长度
 */
int32_t UTF8ToUnicode(uint8_t *utf8, uint32_t *unicode) {
    const uint8_t lut_size = 3;
    const uint8_t length_lut[] = {2, 3, 4};
    const uint8_t range_lut[] = {0xE0, 0xF0, 0xF8};
    const uint8_t mask_lut[] = {0x1F, 0x0F, 0x07};

    uint8_t length = 0;
    uint8_t b = *(utf8 + 0);
    uint32_t i = 0;

    if(utf8 == NULL) {
        *unicode = 0;
        return -1;
    }
    // utf8编码兼容ASCII编码,使用0xxxxxx 表示00~7F
    if(b < 0x80) {
        *unicode =  b;
        return 1;
    }
    // utf8不兼容ISO8859-1 ASCII拓展字符集
    // 同时最大支持编码6个字节即1111110X
    if(b < 0xC0 || b > 0xFD) {
        *unicode = 0;
        return -1;
    }
    for(i = 0; i < lut_size; i++) {
        if(b < range_lut[i]) {
            *unicode = b & mask_lut[i];
            length = length_lut[i];
            break;
        }
    }
    // 超过四字节的utf8编码不进行解析
    if(length == 0) {
        *unicode = 0;
        return -1;
    }
    // 取后续字节数据
    for(i = 1; i < length; i++ ) {
        b = *(utf8 + i);
        // 多字节utf8编码后续字节范围10xxxxxx ~ 10111111
        if( b < 0x80 || b > 0xBF ) {
            break;
        }
        *unicode <<= 6;
        // 00111111
        *unicode |= (b & 0x3F);
    }
    // 长度校验
    return (i < length) ? (-1) : length;
}

/** 
 * UnicodeToUTF8 - convert unicode char to UTF-8 char 
 * @param *Unicodevalue: a UNICODE character 
 * @param *utf8dst: a buffer to contain a utf-8 characters 
 * 
 * @return number of bytes for the utf-8 sequence, supports up to 4 bytes of UTF8, returns -1 if require more than 4 bytes.
 */ 
int UnicodeToUTF8(unsigned long Unicodevalue, unsigned char *utf8dst)
{
    unsigned char *dst=utf8dst;
    unsigned char utf8_1, utf8_2, utf8_3, utf8_4;
    
    Unicodevalue=Unicodevalue & 0xffffffff;
    
    if (Unicodevalue<0x80)
    {
        if (utf8dst==NULL) return -1;
        utf8_1=(char)Unicodevalue;
        *dst=utf8_1;
        return 1;
    }

    if (Unicodevalue<0x800)
    {
        if (utf8dst==NULL) return -1;
        unsigned long v=Unicodevalue << 2;

        unsigned char *pU=(unsigned char*)&v;
        unsigned char ub1=(*pU) >> 2, ub2= *(pU+1);
    
        utf8_1=0xc0 | ub2;
        utf8_2=0x80 | ub1;

        *dst=utf8_1;dst++;
        *dst=utf8_2;
        return 2;
    }

    if (Unicodevalue<0x10000)
    {
        if (utf8dst==NULL) return -1;
        unsigned char *pU=(unsigned char*)&Unicodevalue;
        unsigned char ub1=(*pU), ub2= *(pU+1);
    
        utf8_1=0xe0 | (ub2 >> 4);
        utf8_2=0x80 | ((ub2 & 0xf) <<2) | (ub1 >> 6);
        utf8_3=0x80 | (ub1 & 0x3f);

        *dst=utf8_1;dst++;
        *dst=utf8_2;dst++;
        *dst=utf8_3;
        return 3;
    }

    if (Unicodevalue<0x200000)
    {
        if (utf8dst==NULL) return -1;
        unsigned long v=Unicodevalue;
        unsigned char *pU=(unsigned char*)&v;
        unsigned char ub1=(*pU), ub2= *(pU+1), ub3= *(pU+2);
    
        utf8_1=0xf0 | (ub3 >> 2);
        utf8_2=0x80 | ((ub3 & 3) << 4) | (ub2 >> 4);
        utf8_3=0x80 | ((ub2 & 0xf) << 2) | ( ub1 >> 6);
        utf8_4=0x80 | (ub1 & 0x3f);

        *dst=utf8_1;dst++;
        *dst=utf8_2;dst++;
        *dst=utf8_3;dst++;
        *dst=utf8_4;
        return 4;
    }
    if (Unicodevalue>=0x200000)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief 4字节unicode(usc4)字符集转utf16编码
 * @param unicode unicode字符值
 * @param *utf16 utf16编码结果
 * @return utf16长度，(2字节)单位
 */
int32_t UnicodeToUTF16(uint32_t unicode, uint16_t *utf16) {
    // Unicode范围 U+000~U+FFFF
    // utf16编码方式：2 Byte存储，编码后等于Unicode值
    if(unicode <= 0xFFFF) {
        if(utf16 != NULL) {
            *utf16 = (unicode & 0xFFFF);
        }
        return 1;
    }else if( unicode <= 0xEFFFF ) {
        if( utf16 != NULL ) {
            // 高10位
            *(utf16 + 0) = 0xD800 + (unicode >> 10) - 0x40;
            // 低10位
            *(utf16 + 1) = 0xDC00 + (unicode &0x03FF);
        }
        return 2;
    }

    return -1;
}

/** 
 * UTF16toUnicode - Converts UTF16 multiword character to Unicode, supports up to 4 bytes of UTF8. 
 * @param *utf16MW: UTF16 multiword character
 * @param *outunicode: a unicode character output 
 * 
 * @return the number of words this UTF16 character was composed of. Returns -1 if utf16MW=NULL.
 */ 
int UTF16toUnicode(unsigned short *utf16MW, unsigned int *outunicode)
{
    unsigned short first = (unsigned short)*utf16MW;

    if (utf16MW == NULL) return -1;
    if (outunicode == NULL) return -1;

    *outunicode=0;

    if ((first >= 0xD800)&&(first <= 0xDBFF)) // this is a pair of words
    {
        unsigned short trail=(unsigned short)*(utf16MW + 1);
    
        *outunicode= (((first - 0xd800) << 10 ) | ((trail - 0xdc00) &0x3ff)) + 0x10000;

        return 2;
    }
    else		// this is a one word
    {
        *outunicode= (unsigned int)first;
        return 1;
    }
}

/**
 * @brief 2字节utf16编码转gb2312编码
 * @param utf16 utf16编码
 * @param *gb2812 gb2312编码结果
 * @return gb2812长度，字节为单位
 */
int32_t UTF16ToGB2312(uint16_t utf16, uint16_t *gb2312) {
    uint16_t temp_gb2312;
    uint32_t i;

    if (utf16 < 0xFF) {  //ASCII
        *((uint8_t *)gb2312) = utf16;
        return 1;
    } else {
        temp_gb2312 = 0;
        for (i = 0; i < MAX_UTF16_SIZE; i++) {
            if (utf16_to_gb2312_table[i][0] == utf16) {
                temp_gb2312 = utf16_to_gb2312_table[i][1];
                break;
            }
        }
        if (0 != temp_gb2312) {
            *gb2312 = temp_gb2312;
            return 2;
        } else {
            return -1;
        }
    }
}

/**
 * @brief gb2312编码转2字节utf16编码
 * @param utf16 utf16编码
 * @param *gb2812 gb2312编码结果
 * @return gb2812长度，字节为单位
 */
int32_t GB2312ToUTF16(uint16_t gb2312, uint16_t *utf16) {
    uint16_t temp_utf16;
    uint32_t i;

    if (gb2312 < 0xA1A0) {  //ASCII
        *((uint8_t *)utf16) = gb2312 & 0xFF;
        return 1;
    } else {
        temp_utf16 = 0;
        for (i = 0; i < MAX_UTF16_SIZE; i++) {
            if (utf16_to_gb2312_table[i][1] == gb2312) {
                temp_utf16 = utf16_to_gb2312_table[i][0];
                break;
            }
        }
        if (0 != temp_utf16) {
            *utf16 = temp_utf16;
            return 2;
        } else {
            return -1;
        }
    }
}

/**
 * @brief utf8编码转gb2312编码
 * @param *utf8 utf8变长编码字节集(1~4个字节)
 * @param *out_utf8_size 输出当前转换utf8字符的utf8编码长度(1-4个字节)
 * @param *gb2312 gb2312编码结果
 * @return gb2312长度，字节为单位
 */
int32_t UTF8ToGB2312(uint8_t *utf8, uint32_t *out_utf8_size, uint16_t *gb2312) {
    int32_t len;
    uint32_t buffer = 0;
    uint16_t utf16[2] = {0, 0};
    
    len = UTF8ToUnicode(utf8, &buffer);
    if (len < 0) {
        return -1;
    }
    
    if (out_utf8_size) {
        *out_utf8_size = len;
    }
    
    len = UnicodeToUTF16(buffer, utf16);
    if (len < 0) {
        return -1;
    }
    
    if (len != 1) { //其他的不支持转化为GB2312
        return -1;
    }
    
    len = UTF16ToGB2312(utf16[0], gb2312);
    
    return len;
}

/**
 * @brief gb2312编码转utf8编码
 * @param *gb2312 gb2312编码字节集(1~2个字节)
 * @param *out_gb2312_size 输出当前转换gb2312字符的gb2312编码长度(1-2个字节)
 * @param *utf8 utf8编码结果
 * @return utf8长度，字节为单位
 */
int32_t GB2312ToUTF8(uint16_t *gb2312, uint32_t *out_gb2312_size, uint8_t *utf8) {
    int32_t len;
    uint32_t buffer = 0;
    uint16_t utf16[2] = {0, 0};

    len = GB2312ToUTF16(*gb2312, utf16);
    if (len < 0) {
        return -1;
    }
    
    if (out_gb2312_size) {
        *out_gb2312_size = len;
    }
    
    len = UTF16toUnicode(utf16, &buffer);
    if (len < 0) {
        return -1;
    }

    len = UnicodeToUTF8(buffer, utf8);
    if (len < 0) {
        return -1;
    }

    return len;
}

/**
 * @brief utf8编码字符串转gb2312编码字符串
 * @param *utf8_str utf8编码字符串(\0结尾)
 * @param *gb2312_str 输出gb2312编码字符串(\0结尾)
 * @param *gb2312_str_buff_size 输出缓冲区大小
 * @return gb2312编码字符串长度，字节为单位
 */
int32_t UTF8ToGB2312Str(uint8_t *utf8_str, uint8_t *gb2312_str, uint32_t gb2312_str_buff_size) {
    uint16_t gb2312 = 0;
    uint32_t n = 0;
    uint32_t m = 0;
    uint32_t count = 0;  
    unsigned char *s = NULL;  
    unsigned char *e = NULL; 

    if ((utf8_str == NULL) || (gb2312_str == NULL)) {
        return -1;
    }

    s = utf8_str;  
    e = gb2312_str;  
    
    while (*s) {
        m = UTF8ToGB2312 (s, &n, &gb2312);
        if (m > 0) {
            if ((count + 1) >= gb2312_str_buff_size) {
                return count;
            } else {
                if (m == 1) {
                    *e = gb2312 & 0xFF;
                    e++;
                    *e = '\0';
                 } else if (m == 2) {
                    *e = (gb2312 & 0xFF00) >> 8;  // gb2312_str里面是大端模式
                    e++;
                    *e = gb2312 & 0xFF;
                    e++;
                    *e = '\0';
                } else { /* Converting error occurs */
                    return count;
                }

                /* Step to next utf-8 character */
                s += n;
                count += m;
            }
        } else { /* Converting error occurs */
            return count;
        }
    }

    return count;   
}

/**
 * @brief gb2312编码字符串转utf8编码字符串
 * @param *gb2312 gb2312编码字符串(\0结尾)
 * @param *utf8 输出utf8编码字符串(\0结尾)
 * @param *utf8_str_buff_size 输出缓冲区大小
 * @return utf8编码字符串长度，字节为单位
 */
int32_t GB2312ToUTF8Str(uint8_t *gb2312_str, uint8_t *utf8_str, uint32_t utf8_str_buff_size) {
    uint16_t gb2312 = 0;
    uint32_t n = 0;
    uint32_t m = 0;
    uint32_t count = 0;  
    unsigned char *s = NULL;  
    unsigned char *e = NULL;
    unsigned char utf8_ch[8]; 

    if ((gb2312_str == NULL) || (utf8_str == NULL)) {
        return -1;
    }

    s = gb2312_str;  
    e = utf8_str;  
    
    while (*s) {
        memset (utf8_ch, 0, sizeof (utf8_ch));
        if (s[0] < 0xA0) { // ASCII
            gb2312 = s[0];
        } else {
            gb2312 = ((uint16_t)s[0] << 8) | s[1]; // gb2312_str里面是大端模式
        }
        m = GB2312ToUTF8 (&gb2312, &n, &utf8_ch[0]);
        if (m > 0) {
            *e = '\0';

            /* Judge whether exceed the destination buffer */ 
            if ((e - utf8_str + m) >= utf8_str_buff_size) {
                return count;
            } else {
                memcpy (e, utf8_ch, m);
                e += m;  
                *e = '\0';

                /* Step to next gb2312 character */
                s += n;
                count += m;
            }
        } else { /* Converting error occurs */
            return count;
        }
    }

    return count;   
}

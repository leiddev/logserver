# unicode_utility

简单好用的UTF8与GB2312字符串转换库. 包含C与C++的API.

## Functions:

```c
int32_t UTF8ToGB2312Str(uint8_t *utf8_str, uint8_t *gb2312_str, uint32_t gb2312_str_buff_size);
```

utf8编码字符串转gb2312编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*utf8_str* | utf8编码字符串(\0结尾)
*gb2312_str* | 输出gb2312编码字符串(\0结尾)
*gb2312_str_buff_size* | 输出缓冲区大小


#### Return:
gb2312编码字符串长度，字节为单位.

---

```c
int32_t GB2312ToUTF8Str(uint8_t *gb2312_str, uint8_t *utf8_str, uint32_t utf8_str_buff_size);
```

gb2312编码字符串转utf8编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*gb2312_str* | gb2312编码字符串(\0结尾)
*utf8_str* | 输出utf8编码字符串(\0结尾)
*utf8_str_buff_size* | 输出缓冲区大小


#### Return:
utf8编码字符串长度，字节为单位.

---

```c++
std::string UTF8ToGB2312(std::string &utf8_str);
```

utf8编码字符串转gb2312编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*utf8_str* | utf8编码string


#### Return:
gb2312编码string.

---

```c++
std::string UTF8ToGB2312(unsigned char *utf8_str, unsigned int len);
```

utf8编码字符串转gb2312编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*utf8_str* | utf8编码的字符串
*len* | 要转换的utf8字节数

#### Return:
gb2312编码string.

---

```c++
std::string GB2312ToUTF8(std::string &gb2312_str);
```

gb2312编码字符串转utf8编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*gb2312_str* | gb2312编码string


#### Return:
utf8编码string.

---

```c++
std::string GB2312ToUTF8(unsigned char *gb2312_str, unsigned int len);
```

gb2312编码字符串转utf8编码字符串.

#### Arguments:
Arg | Description
--- | -----------
*gb2312_str* | gb2312编码的字符串
*len* | 要转换的gb2312字节数


#### Return:
utf8编码string.



# LICENSE
> Copyright (c) 2023-2023 [Leid](mailto:ld-1@163.com)  
>  
> Permission to use, copy, modify, and/or distribute this software for any  
> purpose with or without fee is hereby granted, provided that the above  
> copyright notice and this permission notice appear in all copies.  
>  
> THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES  
> WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF  
> MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR  
> ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES  
> WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN  
> ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF  
> OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

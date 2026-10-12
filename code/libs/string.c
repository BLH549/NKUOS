/*
 * 裸机内核的字符串和内存基础函数。编译时不链接宿主系统的 C 标准库。
 * 这些函数保留模板的实际行为；存在的标准库语义差异在相应函数旁说明。
 */
#include <string.h>

/* *
 * strlen - calculate the length of the string @s, not including
 * the terminating '\0' character.
 * @s:        the input string
 *
 * The strlen() function returns the length of string @s.
 * */
/*
 * strlen：计算以 NUL 结尾的字符串长度。
 * 参数 s：可读字符串地址，必须能访问到结束符。
 * 逐字节扫描并计数，返回 size_t 长度，不包含结束符；没有最大扫描长度。
 */
size_t
strlen(const char *s) {
    size_t cnt = 0;
    while (*s ++ != '\0') {
        cnt ++;
    }
    return cnt;
}

/* *
 * strnlen - calculate the length of the string @s, not including
 * the terminating '\0' char acter, but at most @len.
 * @s:        the input string
 * @len:    the max-length that function will scan
 *
 * Note that, this function looks only at the first @len characters
 * at @s, and never beyond @s + @len.
 *
 * The return value is strlen(s), if that is less than @len, or
 * @len if there is no '\0' character among the first @len characters
 * pointed by @s.
 * */
/*
 * strnlen：在给定范围内测量字符串长度。
 * 参数 s：可读字节序列；len：最多检查的字节数。
 * 遇到 NUL 或计数达到 len 即停止，返回已扫描的非零字符数。
 * len 为零时不读取 s；格式化器用它限制字符串精度或计算补齐长度。
 */
size_t
strnlen(const char *s, size_t len) {
    size_t cnt = 0;
    while (cnt < len && *s ++ != '\0') {
        cnt ++;
    }
    return cnt;
}

/* *
 * strcpy - copies the string pointed by @src into the array pointed by @dst,
 * including the terminating null character.
 * @dst:    pointer to the destination array where the content is to be copied
 * @src:    string to be copied
 *
 * The return value is @dst.
 *
 * To avoid overflows, the size of array pointed by @dst should be long enough to
 * contain the same string as @src (including the terminating null character), and
 * should not overlap in memory with @src.
 * */
char *
/*
 * strcpy：复制完整字符串，包括末尾 NUL。
 * 参数 dst：足够大的目标缓冲区；src：有效源字符串，二者不可重叠。
 * 返回原始 dst。保存目标起点，再边赋值边推进指针，复制结束符后停止。
 * 若定义 __HAVE_ARCH_STRCPY，则改用对应架构实现；本函数不检查容量。
 */
strcpy(char *dst, const char *src) {
#ifdef __HAVE_ARCH_STRCPY
    return __strcpy(dst, src);
#else
    char *p = dst;
    while ((*p ++ = *src ++) != '\0')
        /* nothing */;
    return dst;
#endif /* __HAVE_ARCH_STRCPY */
}

/* *
 * strncpy - copies the first @len characters of @src to @dst. If the end of string @src
 * if found before @len characters have been copied, @dst is padded with '\0' until a
 * total of @len characters have been written to it.
 * @dst:    pointer to the destination array where the content is to be copied
 * @src:    string to be copied
 * @len:    maximum number of characters to be copied from @src
 *
 * The return value is @dst
 * */
char *
/*
 * strncpy：向 dst 写入恰好 len 个字节。
 * 参数 dst：至少 len 字节的目标区域；src：源字符串；len：写入长度。
 * 源串提前结束时，src 停在 NUL 上，使剩余位置全部填零；返回原始 dst。
 * 源串长度大于等于 len 时不会额外添加结束符，调用者必须自行处理。
 */
strncpy(char *dst, const char *src, size_t len) {
    char *p = dst;
    while (len > 0) {
        if ((*p = *src) != '\0') {
            src ++;
        }
        p ++, len --;
    }
    return dst;
}

/* *
 * strcmp - compares the string @s1 and @s2
 * @s1:        string to be compared
 * @s2:        string to be compared
 *
 * This function starts comparing the first character of each string. If
 * they are equal to each other, it continues with the following pairs until
 * the characters differ or until a terminanting null-character is reached.
 *
 * Returns an integral value indicating the relationship between the strings:
 * - A zero value indicates that both strings are equal;
 * - A value greater than zero indicates that the first character that does
 *   not match has a greater value in @s1 than in @s2;
 * - And a value less than zero indicates the opposite.
 * */
/*
 * strcmp：按无符号字节值比较两个 NUL 结尾的字符串。
 * 参数 s1、s2：有效字符串地址；返回首个不同字节的差，相等返回零。
 * 跳过相同的非零字节，再比较停止位置；差的正负表示字典序关系。
 * 若配置架构专用实现，则转交 __strcmp。
 */
int
strcmp(const char *s1, const char *s2) {
#ifdef __HAVE_ARCH_STRCMP
    return __strcmp(s1, s2);
#else
    while (*s1 != '\0' && *s1 == *s2) {
        s1 ++, s2 ++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
#endif /* __HAVE_ARCH_STRCMP */
}

/* *
 * strncmp - compares up to @n characters of the string @s1 to those of the string @s2
 * @s1:        string to be compared
 * @s2:        string to be compared
 * @n:        maximum number of characters to compare
 *
 * This function starts comparing the first character of each string. If
 * they are equal to each other, it continues with the following pairs until
 * the characters differ, until a terminating null-character is reached, or
 * until @n characters match in both strings, whichever happens first.
 * */
/*
 * strncmp：最多比较 n 个字节的字符串前缀。
 * 参数 s1、s2：输入字符串；n：最大比较长度。
 * 遇到不同字节、NUL 或 n 次匹配后停止；返回首个不同无符号字节的差。
 * n 为零或指定范围内完全相同则返回零，不再访问后续字节。
 */
int
strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        n --, s1 ++, s2 ++;
    }
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/* *
 * strchr - locates first occurrence of character in string
 * @s:        the input string
 * @c:        character to be located
 *
 * The strchr() function returns a pointer to the first occurrence of
 * character in @s. If the value is not found, the function returns 'NULL'.
 * */
char *
/*
 * strchr：在字符串非零字符中寻找第一个等于 c 的字符。
 * 参数 s：有效字符串；c：要寻找的字符；找到则返回该位置，否则返回 NULL。
 * 注意当前实现的循环不检查末尾 NUL 本身，因此搜索 c=0 也返回 NULL，
 * 这与标准库 strchr 的行为不同；这里只注释现有逻辑。
 */
strchr(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            return (char *)s;
        }
        s ++;
    }
    return NULL;
}

/* *
 * strfind - locates first occurrence of character in string
 * @s:        the input string
 * @c:        character to be located
 *
 * The strfind() function is like strchr() except that if @c is
 * not found in @s, then it returns a pointer to the null byte at the
 * end of @s, rather than 'NULL'.
 * */
char *
/*
 * strfind：寻找字符，未找到时返回字符串结束符的位置。
 * 参数 s：有效字符串；c：目标字符；返回指向匹配处或末尾 NUL 的指针。
 * 与本文件 strchr 的区别是失败时不返回 NULL，可直接用于计算前缀长度。
 */
strfind(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            break;
        }
        s ++;
    }
    return (char *)s;
}

/* *
 * strtol - converts string to long integer
 * @s:        the input string that contains the representation of an integer number
 * @endptr:    reference to an object of type char *, whose value is set by the
 *             function to the next character in @s after the numerical value. This
 *             parameter can also be a null pointer, in which case it is not used.
 * @base:    x
 *
 * The function first discards as many whitespace characters as necessary until
 * the first non-whitespace character is found. Then, starting from this character,
 * takes as many characters as possible that are valid following a syntax that
 * depends on the base parameter, and interprets them as a numerical value. Finally,
 * a pointer to the first character following the integer representation in @s
 * is stored in the object pointed by @endptr.
 *
 * If the value of base is zero, the syntax expected is similar to that of
 * integer constants, which is formed by a succession of:
 * - An optional plus or minus sign;
 * - An optional prefix indicating octal or hexadecimal base ("0" or "0x" respectively)
 * - A sequence of decimal digits (if no base prefix was specified) or either octal
 *   or hexadecimal digits if a specific prefix is present
 *
 * If the base value is between 2 and 36, the format expected for the integral number
 * is a succession of the valid digits and/or letters needed to represent integers of
 * the specified radix (starting from '0' and up to 'z'/'Z' for radix 36). The
 * sequence may optionally be preceded by a plus or minus sign and, if base is 16,
 * an optional "0x" or "0X" prefix.
 *
 * The strtol() function returns the converted integral number as a long int value.
 * */
long
/*
 * strtol：把文本中的整数转换为 long。
 * 参数 s：输入字符串；endptr：可选的停止位置输出地址；base：进制。
 * 先跳过空格和制表符，读取符号，再识别小写 0x 或自动进制的前导零。
 * base=0 时自动选十六、八或十进制；调用者应传入 0 或 2～36。
 * 数字及字母被转换为数位，以 val*base+dig 累加；遇到无效数位即停止。
 * 返回带符号的结果；endptr 非空时记录停止处。当前实现不检查溢出、
 * 不设置 errno，也不完整复现标准库对空白、前缀和无有效数字的处理。
 */
strtol(const char *s, char **endptr, int base) {
    int neg = 0;
    long val = 0;

    // 跳过开头的空格和制表符；这里不识别所有标准空白字符。
    while (*s == ' ' || *s == '\t') {
        s ++;
    }

    // 读取可选正负号，用 neg 记录最终是否取负。
    if (*s == '+') {
        s ++;
    }
    else if (*s == '-') {
        s ++, neg = 1;
    }

    // 根据 base 和前缀选择进制；当前仅识别小写 0x。
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x')) {
        s += 2, base = 16;
    }
    else if (base == 0 && s[0] == '0') {
        s ++, base = 8;
    }
    else if (base == 0) {
        base = 10;
    }

    // 把数字或字母转为数位，超出当前进制范围即结束。
    while (1) {
        int dig;

        if (*s >= '0' && *s <= '9') {
            dig = *s - '0';
        }
        else if (*s >= 'a' && *s <= 'z') {
            dig = *s - 'a' + 10;
        }
        else if (*s >= 'A' && *s <= 'Z') {
            dig = *s - 'A' + 10;
        }
        else {
            break;
        }
        if (dig >= base) {
            break;
        }
        s ++, val = (val * base) + dig;
        // 这里没有溢出检查，过大的数值不保证正确结果。
    }

    if (endptr) {
        *endptr = (char *) s;
    }
    return (neg ? -val : val);
}

/* *
 * memset - sets the first @n bytes of the memory area pointed by @s
 * to the specified value @c.
 * @s:        pointer the the memory area to fill
 * @c:        value to set
 * @n:        number of bytes to be set to the value
 *
 * The memset() function returns @s.
 * */
void *
/*
 * memset：将内存区域前 n 个字节填成 c。
 * 参数 s：目标区域；c：本接口采用 char 类型的填充值；n：字节数。
 * 返回原始 s。默认实现通过 char 指针逐字节写入；n=0 时不写内存。
 * Lab1 中 kern_init 用它清零链接脚本给出的 [edata,end) 范围。
 * 定义 __HAVE_ARCH_MEMSET 时使用架构专用实现。
 */
memset(void *s, char c, size_t n) {
#ifdef __HAVE_ARCH_MEMSET
    return __memset(s, c, n);
#else
    char *p = s;
    while (n -- > 0) {
        *p ++ = c;
    }
    return s;
#endif /* __HAVE_ARCH_MEMSET */
}

/* *
 * memmove - copies the values of @n bytes from the location pointed by @src to
 * the memory area pointed by @dst. @src and @dst are allowed to overlap.
 * @dst        pointer to the destination array where the content is to be copied
 * @src        pointer to the source of data to by copied
 * @n:        number of bytes to copy
 *
 * The memmove() function returns @dst.
 * */
void *
/*
 * memmove：复制 n 字节，允许源区域和目标区域重叠。
 * 参数 dst：目标；src：源；n：字节数；返回原始 dst。
 * 若目标起点落在源区间内部且地址更高，先把指针移至末尾，再倒序复制，
 * 避免覆盖尚未读取的源字节；其他情况正序复制。
 * 定义 __HAVE_ARCH_MEMMOVE 时使用架构专用实现。
 */
memmove(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMMOVE
    return __memmove(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    /* 目标位于源区间内部的高地址端：倒序复制才能保留未读取的源字节。 */
    if (s < d && s + n > d) {
        s += n, d += n;
        while (n -- > 0) {
            *-- d = *-- s;
        }
    } else {
        while (n -- > 0) {
            *d ++ = *s ++;
        }
    }
    return dst;
#endif /* __HAVE_ARCH_MEMMOVE */
}

/* *
 * memcpy - copies the value of @n bytes from the location pointed by @src to
 * the memory area pointed by @dst.
 * @dst        pointer to the destination array where the content is to be copied
 * @src        pointer to the source of data to by copied
 * @n:        number of bytes to copy
 *
 * The memcpy() returns @dst.
 *
 * Note that, the function does not check any terminating null character in @src,
 * it always copies exactly @n bytes. To avoid overflows, the size of arrays pointed
 * by both @src and @dst, should be at least @n bytes, and should not overlap
 * (for overlapping memory area, memmove is a safer approach).
 * */
void *
/*
 * memcpy：从 src 向 dst 复制恰好 n 个字节。
 * 参数 dst、src：分别至少有 n 字节可写、可读的区域；n：复制长度。
 * 返回原始 dst；默认实现始终正序逐字节复制，不识别 NUL、不检查容量。
 * 源、目标区域必须不重叠；重叠时应使用 memmove。可由架构实现替代。
 */
memcpy(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMCPY
    return __memcpy(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    while (n -- > 0) {
        *d ++ = *s ++;
    }
    return dst;
#endif /* __HAVE_ARCH_MEMCPY */
}

/* *
 * memcmp - compares two blocks of memory
 * @v1:        pointer to block of memory
 * @v2:        pointer to block of memory
 * @n:        number of bytes to compare
 *
 * The memcmp() functions returns an integral value indicating the
 * relationship between the content of the memory blocks:
 * - A zero value indicates that the contents of both memory blocks are equal;
 * - A value greater than zero indicates that the first byte that does not
 *   match in both memory blocks has a greater value in @v1 than in @v2
 *   as if evaluated as unsigned char values;
 * - And a value less than zero indicates the opposite.
 * */
/*
 * memcmp：比较两个内存区域的前 n 个字节。
 * 参数 v1、v2：至少 n 字节可读的区域；n：比较长度。
 * 找到首个不同字节时按 unsigned char 值作差并返回；全部相同则返回零。
 * 不把 NUL 当作结束符，返回值的正负表示两个字节序列的大小关系。
 */
int
memcmp(const void *v1, const void *v2, size_t n) {
    const char *s1 = (const char *)v1;
    const char *s2 = (const char *)v2;
    while (n -- > 0) {
        if (*s1 != *s2) {
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
        }
        s1 ++, s2 ++;
    }
    return 0;
}


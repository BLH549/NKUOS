/*
 * 字符串和内存操作接口，size_t 等基础类型由 defs.h 提供。
 * 实现见 string.c；函数名称与标准库相似，但需遵守本实现的使用前提。
 */
#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__

#include <defs.h>

/*
 * strlen：计算以 NUL 结尾的字符串长度。
 * 参数 s：可读字符串地址，必须能访问到结束符。
 * 逐字节扫描并计数，返回 size_t 长度，不包含结束符；没有最大扫描长度。
 */
size_t strlen(const char *s);
/*
 * strnlen：在给定范围内测量字符串长度。
 * 参数 s：可读字节序列；len：最多检查的字节数。
 * 遇到 NUL 或计数达到 len 即停止，返回已扫描的非零字符数。
 * len 为零时不读取 s；格式化器用它限制字符串精度或计算补齐长度。
 */
size_t strnlen(const char *s, size_t len);

/*
 * strcpy：复制完整字符串，包括末尾 NUL。
 * 参数 dst：足够大的目标缓冲区；src：有效源字符串，二者不可重叠。
 * 返回原始 dst。保存目标起点，再边赋值边推进指针，复制结束符后停止。
 * 若定义 __HAVE_ARCH_STRCPY，则改用对应架构实现；本函数不检查容量。
 */
char *strcpy(char *dst, const char *src);
/*
 * strncpy：向 dst 写入恰好 len 个字节。
 * 参数 dst：至少 len 字节的目标区域；src：源字符串；len：写入长度。
 * 源串提前结束时，src 停在 NUL 上，使剩余位置全部填零；返回原始 dst。
 * 源串长度大于等于 len 时不会额外添加结束符，调用者必须自行处理。
 */
char *strncpy(char *dst, const char *src, size_t len);

/*
 * strcmp：按无符号字节值比较两个 NUL 结尾的字符串。
 * 参数 s1、s2：有效字符串地址；返回首个不同字节的差，相等返回零。
 * 跳过相同的非零字节，再比较停止位置；差的正负表示字典序关系。
 * 若配置架构专用实现，则转交 __strcmp。
 */
int strcmp(const char *s1, const char *s2);
/*
 * strncmp：最多比较 n 个字节的字符串前缀。
 * 参数 s1、s2：输入字符串；n：最大比较长度。
 * 遇到不同字节、NUL 或 n 次匹配后停止；返回首个不同无符号字节的差。
 * n 为零或指定范围内完全相同则返回零，不再访问后续字节。
 */
int strncmp(const char *s1, const char *s2, size_t n);

/*
 * strchr：在字符串非零字符中寻找第一个等于 c 的字符。
 * 参数 s：有效字符串；c：要寻找的字符；找到则返回该位置，否则返回 NULL。
 * 注意当前实现的循环不检查末尾 NUL 本身，因此搜索 c=0 也返回 NULL，
 * 这与标准库 strchr 的行为不同；这里只注释现有逻辑。
 */
char *strchr(const char *s, char c);
/*
 * strfind：寻找字符，未找到时返回字符串结束符的位置。
 * 参数 s：有效字符串；c：目标字符；返回指向匹配处或末尾 NUL 的指针。
 * 与本文件 strchr 的区别是失败时不返回 NULL，可直接用于计算前缀长度。
 */
char *strfind(const char *s, char c);
/*
 * strtol：把文本中的整数转换为 long。
 * 参数 s：输入字符串；endptr：可选的停止位置输出地址；base：进制。
 * 先跳过空格和制表符，读取符号，再识别小写 0x 或自动进制的前导零。
 * base=0 时自动选十六、八或十进制；调用者应传入 0 或 2～36。
 * 数字及字母被转换为数位，以 val*base+dig 累加；遇到无效数位即停止。
 * 返回带符号的结果；endptr 非空时记录停止处。当前实现不检查溢出、
 * 不设置 errno，也不完整复现标准库对空白、前缀和无有效数字的处理。
 */
long strtol(const char *s, char **endptr, int base);

/*
 * memset：将内存区域前 n 个字节填成 c。
 * 参数 s：目标区域；c：本接口采用 char 类型的填充值；n：字节数。
 * 返回原始 s。默认实现通过 char 指针逐字节写入；n=0 时不写内存。
 * Lab1 中 kern_init 用它清零链接脚本给出的 [edata,end) 范围。
 * 定义 __HAVE_ARCH_MEMSET 时使用架构专用实现。
 */
void *memset(void *s, char c, size_t n);
/*
 * memmove：复制 n 字节，允许源区域和目标区域重叠。
 * 参数 dst：目标；src：源；n：字节数；返回原始 dst。
 * 若目标起点落在源区间内部且地址更高，先把指针移至末尾，再倒序复制，
 * 避免覆盖尚未读取的源字节；其他情况正序复制。
 * 定义 __HAVE_ARCH_MEMMOVE 时使用架构专用实现。
 */
void *memmove(void *dst, const void *src, size_t n);
/*
 * memcpy：从 src 向 dst 复制恰好 n 个字节。
 * 参数 dst、src：分别至少有 n 字节可写、可读的区域；n：复制长度。
 * 返回原始 dst；默认实现始终正序逐字节复制，不识别 NUL、不检查容量。
 * 源、目标区域必须不重叠；重叠时应使用 memmove。可由架构实现替代。
 */
void *memcpy(void *dst, const void *src, size_t n);
/*
 * memcmp：比较两个内存区域的前 n 个字节。
 * 参数 v1、v2：至少 n 字节可读的区域；n：比较长度。
 * 找到首个不同字节时按 unsigned char 值作差并返回；全部相同则返回零。
 * 不把 NUL 当作结束符，返回值的正负表示两个字节序列的大小关系。
 */
int memcmp(const void *v1, const void *v2, size_t n);

#endif /* !__LIBS_STRING_H__ */


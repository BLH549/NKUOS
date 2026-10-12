/*
 * 行输入辅助代码：使用 1024 字节静态缓冲区，并在输入时回显。
 * 虽然会被编译，但当前启动只打印消息，不使用这条输入路径。
 */
#include <stdio.h>

#define BUFSIZE 1024
static char buf[BUFSIZE];

/* *
 * readline - get a line from stdin
 * @prompt:        the string to be written to stdout
 *
 * The readline() function will write the input string @prompt to
 * stdout first. If the @prompt is NULL or the empty string,
 * no prompt is issued.
 *
 * This function will keep on reading characters and saving them to buffer
 * 'buf' until '\n' or '\r' is encountered.
 *
 * Note that, if the length of string that will be read is longer than
 * buffer size, the end of string will be discarded.
 *
 * The readline() function returns the text of the line read. If some errors
 * are happened, NULL is returned. The return value is a global variable,
 * thus it should be copied before it is used.
 * */
/*
 * readline：显示提示并读取一行控制台文本。
 * 参数 prompt：可为 NULL 的提示字符串；返回静态 buf 地址，输入错误返回 NULL。
 * 通过 getchar 忙等读取，可打印字符在容量允许时保存并回显；退格减少长度，
 * 回车或换行结束输入并添加 NUL。最多保存 BUFSIZE-1 个字符，超出部分丢弃。
 * 返回的缓冲区由后续调用覆盖，不可释放；需要保留时应自行复制。
 * 本函数不支持并发调用，当前 Lab1 启动没有调用输入功能。
 */
char *readline(const char *prompt) {
    if (prompt != NULL) {
        cprintf("%s", prompt);
    }
    int i = 0, c;
    while (1) {
        c = getchar();
        if (c < 0) {
            return NULL;
        } else if (c >= ' ' && i < BUFSIZE - 1) {
            /* 为最终 NUL 留一个位置；只在尚有空间时保存并回显字符。 */
            cputchar(c);
            buf[i++] = c;
        } else if (c == '\b' && i > 0) {
            /* 退格只撤销已有字符；这里回显退格，不额外输出擦除序列。 */
            cputchar(c);
            i--;
        } else if (c == '\n' || c == '\r') {
            /* 回车或换行结束一行，结束符不保存到返回文本中。 */
            cputchar(c);
            buf[i] = '\0';
            return buf;
        }
    }
}

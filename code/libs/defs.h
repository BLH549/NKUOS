/*
 * 内核公共类型、编译器属性和地址计算宏。
 * 本项目按 RV64 构建，下面 intptr_t、uintptr_t、size_t 均采用 64 位定义。
 */
/* 逐行：若该名字尚未定义才处理后续内容；头文件保护避免重复声明，NULL 检查避免覆盖已有定义。 */
#ifndef __LIBS_DEFS_H__
/* 逐行：设置头文件保护标记，使后续包含跳过同一组定义。 */
#define __LIBS_DEFS_H__

/* 逐行：若该名字尚未定义才处理后续内容；头文件保护避免重复声明，NULL 检查避免覆盖已有定义。 */
#ifndef NULL
/* 逐行：把空指针表示定义为转换成 void 指针的零值，不指向可访问对象。 */
#define NULL ((void *)0)
/* 逐行：结束对应条件编译范围；预处理阶段生效，不是运行时分支。 */
#endif

/*
 * 编译器属性：always_inline 请求强制内联，noinline 禁止内联，noreturn 声明不正常返回。
 */
/* 逐行：用 inline 加 GCC 属性请求强制内联，不是修改函数的计算结果。 */
#define __always_inline inline __attribute__((always_inline))
/* 逐行：用 GCC 属性要求该函数不内联，保留独立调用。 */
#define __noinline __attribute__((noinline))
/* 逐行：声明函数不正常返回，让编译器按该控制流约定优化和检查。 */
#define __noreturn __attribute__((noreturn))

/* Represents true-or-false values */
/* 逐行：将 int 别名用于布尔值；零表示假，非零表示真。 */
typedef int bool;

/* Explicitly-sized versions of integer types */
/* 逐行：以普通 char 声明 8 位类型别名；char 的有符号性取决于目标，不能仅由名称断言。 */
typedef char int8_t;
/* 逐行：定义 uint8_t，在本工具链中表示 8 位无符号整数；typedef 不分配变量空间。 */
typedef unsigned char uint8_t;
/* 逐行：定义 int16_t，在本工具链中表示 16 位有符号整数；typedef 不分配变量空间。 */
typedef short int16_t;
/* 逐行：定义 uint16_t，在本工具链中表示 16 位无符号整数；typedef 不分配变量空间。 */
typedef unsigned short uint16_t;
/* 逐行：定义 int32_t，在本工具链中表示 32 位有符号整数；typedef 不分配变量空间。 */
typedef int int32_t;
/* 逐行：定义 uint32_t，在本工具链中表示 32 位无符号整数；typedef 不分配变量空间。 */
typedef unsigned int uint32_t;
/* 逐行：定义 int64_t，在本工具链中表示 64 位有符号整数；typedef 不分配变量空间。 */
typedef long long int64_t;
/* 逐行：定义 uint64_t，在本工具链中表示 64 位无符号整数；typedef 不分配变量空间。 */
typedef unsigned long long uint64_t;

/* Add fast types */
/* 逐行：定义 int_fast8_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef signed char int_fast8_t;
/* 逐行：定义 int_fast16_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef short int_fast16_t;
/* 逐行：定义 int_fast32_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef long int_fast32_t;
/* 逐行：定义 int_fast64_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef long long int_fast64_t;

/* 逐行：定义 uint_fast8_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef unsigned char uint_fast8_t;
/* 逐行：定义 uint_fast16_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef unsigned short uint_fast16_t;
/* 逐行：定义 uint_fast32_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef unsigned long uint_fast32_t;
/* 逐行：定义 uint_fast64_t 的基础整数别名；fast 名称表示模板的快速类型选择，不保证恰好为名称中的位数。 */
typedef unsigned long long uint_fast64_t;

/* 逐行：定义 INT8_MIN 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT8_MIN    (-0x7f - 1)
/* 逐行：定义 INT16_MIN 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT16_MIN   (-0x7fff - 1)
/* 逐行：定义 INT32_MIN 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT32_MIN   (-0x7fffffff - 1)
/* 逐行：定义 INT64_MIN 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT64_MIN   (-0x7fffffffffffffff - 1)

/* 逐行：定义 INT8_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT8_MAX    (0x7f)
/* 逐行：定义 INT16_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT16_MAX   (0x7fff)
/* 逐行：定义 INT32_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT32_MAX   (0x7fffffff)
/* 逐行：定义 INT64_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define INT64_MAX   (0x7fffffffffffffff)

/* 逐行：定义 UINT8_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define UINT8_MAX   (0xff)
/* 逐行：定义 UINT16_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define UINT16_MAX  (0xffff)
/* 逐行：定义 UINT32_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define UINT32_MAX  (0xffffffff)
/* 逐行：定义 UINT64_MAX 的整数边界常量；MIN 用负最大值再减一表示最小值，MAX 表示最大可表示数。 */
#define UINT64_MAX  (0xffffffffffffffff)

/* *
 * 本项目的指针和地址按 RV64 使用 64 位表示。
 * We use pointer types to represent addresses,
 * uintptr_t to represent the numerical values of addresses.
 * */
/* 逐行：使用有符号 64 位整数表示 RV64 地址数值。 */
typedef int64_t intptr_t;
/* 逐行：使用无符号 64 位整数表示 RV64 地址数值。 */
typedef uint64_t uintptr_t;

/* size_t is used for memory object sizes */
/* 逐行：用地址宽度的无符号整数表示对象大小和字节计数，当前为 64 位。 */
typedef uintptr_t size_t;

/* used for page numbers */
/* 逐行：复用 size_t 表示物理页号；类型定义本身不创建页表。 */
typedef size_t ppn_t;

/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
/*
 * ROUNDDOWN(a,n)：将 a 向下取整到 n 的整数倍，返回类型保持 typeof(a)。
 * GNU 语句表达式用局部变量保存 a；n 必须非零，避免除零。
 */
/* 逐行解读 ROUNDDOWN（按下面代码行的顺序）：
 * 第1行：定义带两个参数的 GNU 语句表达式宏，计算向下对齐；末尾反斜杠续接下一行。
 * 第2行：把 a 转成 size_t 并保存为 __a，避免对 a 重复求值。
 * 第3行：用 __a 减去除以 n 的余数，得到最近的 n 倍数，再转换回 typeof(a)。
 * 第4行：结束语句表达式；最后一个表达式的值就是宏结果，n 必须非零。
 */
#define ROUNDDOWN(a, n) ({                                          \
            size_t __a = (size_t)(a);                               \
            (typeof(a))(__a - __a % (n));                           \
        })

/* Round up to the nearest multiple of n */
/*
 * ROUNDUP(a,n)：先加 n-1，再向下取整，得到不小于 a 的最近 n 倍数。
 * 要求 n 非零且 a+n-1 不溢出；n 的值只求取一次。
 */
/* 逐行解读 ROUNDUP（按下面代码行的顺序）：
 * 第1行：定义向上对齐宏，结果类型与 a 相同。
 * 第2行：把对齐单位 n 保存为 __n，以避免反复计算该参数。
 * 第3行：先加 __n-1，再向下对齐，实现向上取整；要求加法不溢出。
 * 第4行：结束 GNU 语句表达式，返回上一行计算值。
 */
#define ROUNDUP(a, n) ({                                            \
            size_t __n = (size_t)(n);                               \
            (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n));     \
        })

/* Return the offset of 'member' relative to the beginning of a struct type */
/*
 * offsetof(type,member)：计算结构成员相对结构起点的字节偏移。
 * 使用空地址上的成员取址惯用写法，不在这里实际读取成员内容。
 */
/* 逐行解读 offsetof（按下面代码行的顺序）：
 * 第1行：定义成员偏移宏，参数分别为结构类型和成员名。
 * 第2行：取得空起点结构中成员的地址作为字节偏移，不实际读取成员；转换为 size_t。
 */
#define offsetof(type, member)                                      \
    ((size_t)(&((type *)0)->member))

/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
/*
 * to_struct(ptr,type,member)：由成员指针减去成员偏移，恢复包含它的结构指针。
 * ptr 必须确实指向 type 对象中的 member，否则所得地址不可用于访问。
 */
/* 逐行解读 to_struct（按下面代码行的顺序）：
 * 第1行：定义由成员指针恢复外层结构指针的宏。
 * 第2行：把成员地址转成字节指针，减去成员偏移，再转换为外层结构指针类型。
 */
#define to_struct(ptr, type, member)                               \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* 逐行：结束对应条件编译范围；预处理阶段生效，不是运行时分支。 */
#endif /* !__LIBS_DEFS_H__ */


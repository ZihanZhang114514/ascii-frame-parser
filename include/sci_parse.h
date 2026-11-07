#ifndef SCI_PARSE_H
#define SCI_PARSE_H

/**
 * @file  sci_parse.h
 * @brief 把 ASCII 数字帧（如 "11.4,5.14,9.81"）解析成 float 数组。
 *
 * 纯函数，无全局变量、无动态内存、无 printf，可重入。
 */

#include <stdint.h>

#define SCI_PARSE_VERSION_MAJOR  1
#define SCI_PARSE_VERSION_MINOR  0

/** 一帧最多解析几个数（按需在编译时改） */
#ifndef SCI_PARSE_MAX_VALUES
#define SCI_PARSE_MAX_VALUES     8
#endif

/** 单个数值最多多少个字符（含符号和小数点，不含分隔符） */
#ifndef SCI_PARSE_MAX_FIELD_LEN
#define SCI_PARSE_MAX_FIELD_LEN  15
#endif

/* 返回值：>=0 为解析出的个数；<0 为错误码 */
#define SCI_PARSE_OK              0    /* 仅为文档保留，成功时返回的是个数 */
#define SCI_PARSE_ERR_PARAM     (-1)   /* buf / out 为空，或 out_max 为 0 */
#define SCI_PARSE_ERR_EMPTY     (-2)   /* 空帧 */
#define SCI_PARSE_ERR_BAD_CHAR  (-3)   /* 非法字符（字母、全角逗号、字段内空格等） */
#define SCI_PARSE_ERR_NO_DIGIT  (-4)   /* 字段里没有数字（如 "1,,2"） */
#define SCI_PARSE_ERR_TOO_MANY  (-5)   /* 数值个数超过 out_max */
#define SCI_PARSE_ERR_TOO_LONG  (-6)   /* 单个数值字符数超限 */

/**
 * 解析 "11.4,5.14,9.81" 这样的 ASCII 帧。
 *
 * @param buf     收到的字节，不要求 '\0' 结尾
 * @param len     实际字节数
 * @param out     结果数组
 * @param out_max out 的容量
 *
 * @return >=0 解析出的个数；<0 见上面的错误码。
 *
 * 备注：
 *  - 结尾的 '\r' / '\n' / 空格会被忽略；
 *  - 支持正负号和逗号后的空格；
 *  - 帧中间出现空字段（"1,,2"）或非数字字符会返回错误。
 */
int sci_parse_ascii(const char *buf, uint32_t len, float *out, uint32_t out_max);

#endif /* SCI_PARSE_H */

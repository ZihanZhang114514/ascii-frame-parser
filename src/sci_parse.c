#include "sci_parse.h"

static int sci_isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n') ? 1 : 0;
}

int sci_parse_ascii(const char *buf, uint32_t len, float *out, uint32_t out_max)
{
	uint32_t i = 0;
	uint32_t n = 0;

	if (buf == 0 || out == 0 || out_max == 0)
		return SCI_PARSE_ERR_PARAM;

	/* 去掉尾部空白：串口助手常常会带 \r\n */
	while (len > 0 && sci_isspace(buf[len - 1]))
		len--;
	if (len == 0)
		return SCI_PARSE_ERR_EMPTY;

	for (;;)
	{
		float    value = 0.0f;
		uint32_t digits_after_dot = 0;
		uint32_t field_len = 0;
		int      negative = 0;
		int      seen_dot = 0;
		int      has_digit = 0;
		int      space_after = 0;   /* 字段中间出现空格：后面不允许再跟数字 */

		while (i < len && sci_isspace(buf[i]))   /* 跳过字段前的空白 */
			i++;

		if (i < len && (buf[i] == '-' || buf[i] == '+'))
		{
			negative = (buf[i] == '-') ? 1 : 0;
			i++;
		}

		while (i < len && buf[i] != ',')
		{
			char c = buf[i];

			if (field_len >= SCI_PARSE_MAX_FIELD_LEN)
				return SCI_PARSE_ERR_TOO_LONG;

			if (c == '.')
			{
				if (seen_dot) return SCI_PARSE_ERR_BAD_CHAR;   /* 两个小数点 */
				seen_dot = 1;
			}
			else if (c >= '0' && c <= '9')
			{
				if (space_after) return SCI_PARSE_ERR_BAD_CHAR; /* "1 2" 这种 */
				value = value * 10.0f + (float)(c - '0');
				if (seen_dot) digits_after_dot++;
				has_digit = 1;
			}
			else if (sci_isspace(c))
			{
				space_after = 1;   /* 数字后出现空格：字段到此为止 */
			}
			else
			{
				return SCI_PARSE_ERR_BAD_CHAR;   /* 字母、全角符号等 */
			}
			field_len++;
			i++;
		}

		if (!has_digit)
			return SCI_PARSE_ERR_NO_DIGIT;

		while (digits_after_dot > 0)   /* 小数点后有几位就除几次 10 */
		{
			value /= 10.0f;
			digits_after_dot--;
		}
		if (negative)
			value = -value;

		if (n >= out_max)
			return SCI_PARSE_ERR_TOO_MANY;
		out[n++] = value;

		if (i < len && buf[i] == ',')
		{
			i++;
			if (i >= len)
				break;      /* 允许结尾多一个逗号 */
		}
		else
		{
			break;
		}
	}

	return (int)n;
}

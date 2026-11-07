#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sci_parse.h"
#include "sci_rx.h"

static int test_count = 0;
static int fail_count = 0;

/* ---------------- 解析器测试 ---------------- */

static void expect_parse(const char *name, const char *buf, uint32_t len,
                         uint32_t out_max, int expect_ret, const float *expect_vals)
{
	float out[SCI_PARSE_MAX_VALUES];
	int ret, ok, i;

	memset(out, 0, sizeof(out));
	ret = sci_parse_ascii(buf, len, out, out_max);

	ok = (ret == expect_ret);
	if (ok && ret > 0 && expect_vals != 0)
	{
		for (i = 0; i < ret; i++)
		{
			if (fabsf(out[i] - expect_vals[i]) > 1e-4f)
				ok = 0;
		}
	}

	test_count++;
	if (!ok) fail_count++;

	printf("%s  %-36s ret=%-3d", ok ? "PASS" : "FAIL", name, ret);
	if (!ok)
	{
		printf("   (期望 ret=%d", expect_ret);
		for (i = 0; i < ret; i++) printf(", out[%d]=%g", i, (double)out[i]);
		printf(")");
	}
	printf("\n");
}

static void test_parser(void)
{
	const float v3[] = { 11.4f, 5.14f, 9.81f };
	const float v2[] = { 11.4f, 5.0f };
	const float v1[] = { 0.5f };
	const float vs[] = { -12.5f, 0.3f, -0.05f };
	const float v6[] = { 1, 2, 3, 4, 5, 6 };
	char buf[32];

	expect_parse("正常三值", "11.4,5.14,9.81", (uint32_t)(sizeof("11.4,5.14,9.81") - 1), 8, 3, v3);
	expect_parse("带 \\r\\n 结尾", "11.4,5.14,9.81\r\n", 15, 8, 3, v3);
	expect_parse("逗号后带空格", "11.4, 5.14, 9.81", (uint32_t)(sizeof("11.4, 5.14, 9.81") - 1), 8, 3, v3);
	expect_parse("单个值", "0.5", (uint32_t)(sizeof("0.5") - 1), 8, 1, v1);
	expect_parse("负号/正号", "-12.5,+0.3,-0.05", (uint32_t)(sizeof("-12.5,+0.3,-0.05") - 1), 8, 3, vs);
	expect_parse("六个值", "1,2,3,4,5,6", (uint32_t)(sizeof("1,2,3,4,5,6") - 1), 8, 6, v6);
	expect_parse("结尾多一个逗号", "1,2,", (uint32_t)(sizeof("1,2,") - 1), 8, 2, 0);

	/* 非 NUL 结尾：长度只给 6，后面全是 'X'，解析器不能读过去 */
	memcpy(buf, "11.4,5", 6);
	for (int i = 6; i < 32; i++) buf[i] = 'X';
	expect_parse("不要求 '\\0'，按长度截断", buf, 6, 8, 2, v2);

	/* 各种错误 */
	expect_parse("空字符串", "", 0, 8, SCI_PARSE_ERR_EMPTY, 0);
	expect_parse("只有空白", " \r\n", (uint32_t)(sizeof(" \r\n") - 1), 8, SCI_PARSE_ERR_EMPTY, 0);
	expect_parse("非数字字符", "1.2,ab", (uint32_t)(sizeof("1.2,ab") - 1), 8, SCI_PARSE_ERR_BAD_CHAR, 0);
	expect_parse("两个小数点", "1..2", (uint32_t)(sizeof("1..2") - 1), 8, SCI_PARSE_ERR_BAD_CHAR, 0);
	expect_parse("字段内空格", "1 2", (uint32_t)(sizeof("1 2") - 1), 8, SCI_PARSE_ERR_BAD_CHAR, 0);
	expect_parse("空字段", "1,,2", (uint32_t)(sizeof("1,,2") - 1), 8, SCI_PARSE_ERR_NO_DIGIT, 0);
	expect_parse("单个负号", "-", (uint32_t)(sizeof("-") - 1), 8, SCI_PARSE_ERR_NO_DIGIT, 0);
	expect_parse("数值太长", "1234567890123456", (uint32_t)(sizeof("1234567890123456") - 1), 8, SCI_PARSE_ERR_TOO_LONG, 0);
	expect_parse("超出 out_max", "1,2,3,4,5,6,7,8,9", (uint32_t)(sizeof("1,2,3,4,5,6,7,8,9") - 1), 8, SCI_PARSE_ERR_TOO_MANY, 0);
	expect_parse("out_max=0", "1", (uint32_t)(sizeof("1") - 1), 0, SCI_PARSE_ERR_PARAM, 0);
}

/* ---------------- 收帧测试 ---------------- */

static void feed(sci_rx_t *rx, const char *s, int *frames, int *last_ok,
                 char *last_frame, uint16_t *last_len)
{
	char frame[SCI_RX_MAX];

	for (const char *p = s; *p; p++)
	{
		if (sci_rx_push(rx, *p))
		{
			uint16_t n = 0;
			int ok = sci_rx_take(rx, frame, sizeof(frame), &n);
			(*frames)++;
			*last_ok = ok;
			if (ok)
			{
				memcpy(last_frame, frame, n);
				*last_len = n;
			}
		}
	}
}

static void test_rx(void)
{
	sci_rx_t rx;
	char frame[SCI_RX_MAX];
	uint16_t len = 0;
	int frames = 0, ok = 0;

	/* 一帧，结尾 \r\n */
	sci_rx_init(&rx);
	frames = 0;
	feed(&rx, "11.4,5.14,9.81\r\n", &frames, &ok, frame, &len);
	test_count++;
	if (!(frames == 1 && ok && len == 14 && memcmp(frame, "11.4,5.14,9.81", 14) == 0))
	{
		fail_count++;
		printf("FAIL  收帧: \\r\\n 结尾 (frames=%d ok=%d len=%u)\n", frames, ok, len);
	}
	else printf("PASS  收帧: \\r\\n 结尾\n");

	/* 两帧，空行忽略 */
	sci_rx_init(&rx);
	frames = 0;
	feed(&rx, "1,2\n\n3,4\n", &frames, &ok, frame, &len);
	test_count++;
	if (!(frames == 2 && ok && len == 3 && memcmp(frame, "3,4", 3) == 0))
	{
		fail_count++;
		printf("FAIL  收帧: 空行忽略 (frames=%d len=%u)\n", frames, len);
	}
	else printf("PASS  收帧: 空行忽略\n");

	/* 超长帧：应该被丢弃 */
	{
		char big[SCI_RX_MAX + 10];
		memset(big, '1', sizeof(big));
		big[sizeof(big) - 1] = '\n';
		sci_rx_init(&rx);
		frames = 0;
		ok = 1;
		for (int i = 0; i < (int)sizeof(big); i++)
		{
			if (sci_rx_push(&rx, big[i]))
			{
				frames++;
				ok = sci_rx_take(&rx, frame, sizeof(frame), &len);
			}
		}
		test_count++;
		if (frames != 1 || ok != 0)
		{
			fail_count++;
			printf("FAIL  收帧: 超长帧丢弃 (frames=%d ok=%d)\n", frames, ok);
		}
		else printf("PASS  收帧: 超长帧丢弃\n");
	}
}

int main(void)
{
	test_parser();
	test_rx();

	printf("\n共 %d 项，失败 %d 项\n", test_count, fail_count);
	return fail_count ? 1 : 0;
}


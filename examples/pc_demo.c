#include <stdio.h>

#include "sci_ascii.h"

/* 把错误码翻译成中文，方便在电脑上测试时看 */
static const char *err_text(int code)
{
	switch (code)
	{
	case SCI_PARSE_ERR_PARAM:    return "参数为空";
	case SCI_PARSE_ERR_EMPTY:    return "空帧";
	case SCI_PARSE_ERR_BAD_CHAR: return "含非法字符（字母、全角逗号等）";
	case SCI_PARSE_ERR_NO_DIGIT: return "某个字段里没有数字（如 1,,2）";
	case SCI_PARSE_ERR_TOO_MANY: return "数值个数超过上限";
	case SCI_PARSE_ERR_TOO_LONG: return "单个数值太长";
	default:                     return "未知错误";
	}
}

int main(void)
{
	char     line[SCI_RX_MAX * 2];
	char     frame[SCI_RX_MAX];
	sci_rx_t rx;
	uint16_t len = 0;
	float    vals[SCI_PARSE_MAX_VALUES];
	int      n;
	int      got = 0;

	printf("请输入串口收到的 ASCII 数据帧（如 11.4,5.14,9.81）：\n");

	if (fgets(line, sizeof line, stdin) == NULL)
	{
		printf("输入失败\n");
		return 1;
	}

	/* 模拟串口中断：逐字节喂给 sci_rx，遇到 \r 或 \n 就算一帧结束 */
	sci_rx_init(&rx);
	for (const char *p = line; *p != '\0'; p++)
	{
		if (sci_rx_push(&rx, *p))
		{
			got = sci_rx_take(&rx, frame, sizeof frame, &len);
			break;
		}
	}

	/* 输入没有换行（比如从文件重定向）时，把已收到的字节当作一帧
	   （在 STM32 上就相当于 DMA 空闲中断：等不到换行，按空闲切帧） */
	if (!got)
		got = sci_rx_take(&rx, frame, sizeof frame, &len);

	if (!got)
	{
		printf("没有收到有效数据（空行，或这一帧超过 %d 字节被丢弃）\n", SCI_RX_MAX);
		return 1;
	}

	n = sci_parse_ascii(frame, len, vals, SCI_PARSE_MAX_VALUES);
	if (n < 0)
	{
		printf("解析失败：%s（错误码 %d）\n", err_text(n), n);
		return 1;
	}

	printf("解析到 %d 个浮点数：\n", n);
	for (int i = 0; i < n; i++)
		printf("  [%d] %.6g\n", i, (double)vals[i]);

	return 0;
}

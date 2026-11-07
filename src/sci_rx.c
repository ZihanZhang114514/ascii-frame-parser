#include "sci_rx.h"

void sci_rx_init(sci_rx_t *rx)
{
	rx->len = 0;
	rx->overflow = 0;
}

int sci_rx_push(sci_rx_t *rx, char c)
{
	if (c == '\r' || c == '\n')
	{
		if (rx->len == 0 && !rx->overflow)
			return 0;            /* 空行，忽略 */
		return 1;                /* 一帧结束 */
	}

	if (rx->overflow)
		return 0;                /* 已经超长，继续丢 */

	if (rx->len < SCI_RX_MAX)
		rx->buf[rx->len++] = c;
	else
		rx->overflow = 1;        /* 超长，整帧作废 */

	return 0;
}

int sci_rx_take(sci_rx_t *rx, char *out, uint16_t out_max, uint16_t *out_len)
{
	uint16_t n = rx->len;
	int ok;

	if (rx->overflow || out == 0 || n == 0 || n > out_max)
	{
		ok = 0;
		n = 0;
	}
	else
	{
		for (uint16_t i = 0; i < n; i++)
			out[i] = rx->buf[i];
		ok = 1;
	}

	rx->len = 0;
	rx->overflow = 0;
	if (out_len)
		*out_len = n;

	return ok;
}

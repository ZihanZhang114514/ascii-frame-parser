#ifndef SCI_RX_H
#define SCI_RX_H

/**
 * @file  sci_rx.h
 * @brief 逐字节攒帧：把串口收到的字节流按 '\r' 或 '\n' 切成一条条数据帧。
 *
 * 适合"串口中断逐字节接收"的场景；DMA + 空闲中断本来就按帧交付，
 * 可以直接把缓冲区交给 sci_parse_ascii，不必经过这个模块。
 */

#include <stdint.h>

/** 一帧最多多少字节（含逗号和小数点，不含结尾换行） */
#ifndef SCI_RX_MAX
#define SCI_RX_MAX  64
#endif

typedef struct
{
	char     buf[SCI_RX_MAX];   /* 攒帧用的缓冲区 */
	uint16_t len;               /* 当前帧已有多少字节 */
	uint8_t  overflow;          /* 1 = 这一帧超长，整帧丢弃 */
} sci_rx_t;

/** 初始化（在开始接收前调用一次） */
void sci_rx_init(sci_rx_t *rx);

/**
 * 喂一个字节（轮询、串口中断、DMA 里都可以调）。
 *
 * @return 1 表示收到了一帧的结尾（遇到 '\r' 或 '\n'），调用 sci_rx_take 取走；
 *         0 表示还没收完。
 */
int sci_rx_push(sci_rx_t *rx, char c);

/**
 * 取走一帧，成功后 rx 自动清空。
 *
 * @return 1 成功（out_len 为帧长度）；0 失败（没有数据，或超长帧被丢弃）。
 *         这个函数很短，放在临界区里调用是安全的。
 */
int sci_rx_take(sci_rx_t *rx, char *out, uint16_t out_max, uint16_t *out_len);

#endif /* SCI_RX_H */

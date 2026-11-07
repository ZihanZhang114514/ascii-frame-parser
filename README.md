# ascii-frame-parser

轻量级 C 库：把串口收到的 ASCII 数字帧（如 `11.4,5.14,9.81`）解析成 `float` 数组。
面向 STM32 / 裸机，也能在 PC 上跑单元测试 —— 零依赖、无全局变量、无动态内存、可重入。

> A tiny, dependency-free C library that parses comma-separated ASCII numbers
> from a UART frame into a `float` array. Designed for STM32 / bare-metal;
> also runs on desktop for unit testing.

## 特性

- **按"字节 + 长度"解析**：输入不要求 `'\0'` 结尾，UART / DMA 收到的原始数据可以直接喂进来；
- **容错**：自动忽略结尾的 `\r\n`、逗号后的空格，支持正负号、结尾多一个逗号；
- **一次解析多个值**：默认最多 8 个（`SCI_PARSE_MAX_VALUES`），每个值都做边界检查；
- **无全局变量、无 `malloc`、无 `printf`**：纯函数，可重入；
- **纯 C99，只依赖 `<stdint.h>`**：Keil MDK、IAR、STM32CubeIDE、GCC、MSVC 通用；
- **自带 21 项单元测试**（正常值、错误输入、超长帧丢弃等）。

## 数据格式

一帧由若干"数值"组成，用英文逗号 `,` 分隔：

```
11.4,5.14,9.81
-12.5,0.3
1,2,3
```

- 数值：`[+-]?digits[.digits]`，小数点后可以没有数字（`1.` 也接受）；
- 分隔符只认**半角**逗号 `,`（0x2C），中文全角 `，` 会被判为非法字符；
- 帧结尾的 `\r` / `\n` 会自动去掉；
- 帧中间出现空字段（`1,,2`）或字母等非法字符会返回错误码。

## 目录结构

```
ascii-frame-parser/
├── include/
│   ├── sci_ascii.h      # 汇总头文件（include 这一个即可）
│   ├── sci_parse.h      # 解析接口
│   └── sci_rx.h         # 逐字节攒帧接口
├── src/
│   ├── sci_parse.c
│   └── sci_rx.c
├── tests/
│   └── test_sci_parse.c # 单元测试
├── examples/
│   └── pc_demo.c        # PC 上的演示程序
├── CMakeLists.txt
└── LICENSE
```

## 快速开始（PC）

```bash
git clone https://github.com/ZihanZhang114514/ascii-frame-parser.git
cd ascii-frame-parser

cmake -B build
cmake --build build

# 跑单元测试
ctest --test-dir build --output-on-failure

# 交互式演示
./build/pc_demo        # Windows: build\Debug\pc_demo.exe
```

## 集成到你的工程

### 方式一：CMake（子目录引入）

```cmake
add_subdirectory(ascii-frame-parser)
target_link_libraries(your_app PRIVATE sci_ascii)
```

### 方式二：直接拷贝（Keil / IAR / STM32CubeIDE）

把 `include/` 里的头文件和 `src/` 里的源文件拷进工程，把 `include/` 加入头文件搜索路径，
然后：

```c
#include "sci_ascii.h"
```

## API 说明

### `sci_parse_ascii`

```c
int sci_parse_ascii(const char *buf, uint32_t len, float *out, uint32_t out_max);
```

| 参数 | 说明 |
|---|---|
| `buf` | 收到的字节，不要求 `'\0'` 结尾 |
| `len` | 实际字节数 |
| `out` | 结果数组 |
| `out_max` | 数组容量 |

返回值：`>=0` 为解析出的个数；`<0` 为错误码。

```c
float vals[SCI_PARSE_MAX_VALUES];
int n = sci_parse_ascii((const char *)frame, frame_len, vals, SCI_PARSE_MAX_VALUES);
if (n == 3) { k_p = vals[0]; k_d = vals[1]; rad = vals[2]; }
```

### `sci_rx_*`（逐字节攒帧）

用于"串口中断按字节接收"的场景；如果用的是 DMA + 空闲中断，收到的本来就是完整帧，能跳过这个模块。

```c
void sci_rx_init(sci_rx_t *rx);
int  sci_rx_push(sci_rx_t *rx, char c);                        /* 返回 1 = 收到一帧结尾 */
int  sci_rx_take(sci_rx_t *rx, char *out, uint16_t out_max, uint16_t *out_len);
```

### 错误码

| 宏 | 值 | 含义 |
|---|---|---|
| `SCI_PARSE_ERR_PARAM` | -1 | 参数为空，或 `out_max` 为 0 |
| `SCI_PARSE_ERR_EMPTY` | -2 | 空帧 |
| `SCI_PARSE_ERR_BAD_CHAR` | -3 | 非法字符（字母、全角逗号、字段内空格等） |
| `SCI_PARSE_ERR_NO_DIGIT` | -4 | 字段里没有数字（如 `1,,2`） |
| `SCI_PARSE_ERR_TOO_MANY` | -5 | 数值个数超过 `out_max` |
| `SCI_PARSE_ERR_TOO_LONG` | -6 | 单个数值字符数超限 |

## 在 STM32 上使用

### 方式一：串口中断逐字节 + 换行结尾

**前提**：串口调试助手勾上"发送新行"（`\r\n`）。

```c
#include "sci_ascii.h"

static sci_rx_t          rx;
static uint8_t           rx_byte;
static char              frame[SCI_RX_MAX];
static volatile uint16_t frame_len;
static volatile uint8_t  frame_ready;

void rx_start(void)
{
    sci_rx_init(&rx);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    if (sci_rx_push(&rx, (char)rx_byte))          /* 一帧收齐了 */
    {
        uint16_t n = 0;
        if (sci_rx_take(&rx, frame, sizeof frame, &n))
        {
            frame_len   = n;
            frame_ready = 1;                      /* 只置标志，解析交给主循环 */
        }
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);    /* 重新武装 */
}

/* ---- 主循环里 ---- */
if (frame_ready)
{
    char    snap[SCI_RX_MAX];
    uint16_t len;
    float   vals[SCI_PARSE_MAX_VALUES];
    int     n, i;

    __disable_irq();                     /* 拷快照，别让中断改 */
    len = frame_len;
    for (i = 0; i < len; i++) snap[i] = frame[i];
    frame_ready = 0;
    __enable_irq();

    n = sci_parse_ascii(snap, len, vals, SCI_PARSE_MAX_VALUES);
    if (n == 3) { k_p = vals[0]; k_d = vals[1]; rad = vals[2]; }
}
```

### 方式二：DMA + 空闲中断（推荐，不需要换行）

```c
#define RX_DMA_SIZE  64
static uint8_t  rx_dma[RX_DMA_SIZE];
static uint8_t  rx_snap[2][RX_DMA_SIZE];   /* 双缓冲，防止解析时被下一帧覆盖 */
static volatile uint8_t  rx_idx;
static volatile uint16_t rx_len;
static volatile uint8_t  rx_ready;

void rx_start(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma, sizeof rx_dma);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);   /* 只保留空闲事件 */
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1) return;

    uint8_t idx = rx_idx ^ 1;                 /* 写另一块 */
    for (uint16_t i = 0; i < Size; i++) rx_snap[idx][i] = rx_dma[i];
    rx_len = Size;
    rx_idx = idx;
    rx_ready = 1;

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma, sizeof rx_dma);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}

/* ---- 主循环里 ---- */
if (rx_ready)
{
    uint8_t  idx = rx_idx;
    uint16_t len = rx_len;
    float    vals[SCI_PARSE_MAX_VALUES];
    int      n;

    rx_ready = 0;
    n = sci_parse_ascii((const char *)rx_snap[idx], len, vals, SCI_PARSE_MAX_VALUES);
    if (n == 3) { /* 更新 PID 参数 */ }
}
```

## 注意事项

1. **先定义"一帧到哪里结束"**：换行符（最简单）或 DMA 空闲中断。UART 是字节流，没有结束符就切不了帧。
2. **解析放主循环，不要放中断里**：浮点运算在 M3 上是软件模拟，会拖慢中断响应。
3. **中断和主循环之间要隔离**：拷贝快照或双缓冲，别让中断一边写、主循环一边读。
4. **`float` 精度**：约 7 位有效数字，PID 参数（2～4 位小数）完全够用；M4F 上走硬件单精度。
5. **全角逗号**：中文输入法打出的 `，` 不是 `0x2C`，会返回 `SCI_PARSE_ERR_BAD_CHAR`。
6. **工具链**：代码是 C99（`for` 里声明变量）；Keil ARMCC 5 需要 `--c99`，AC6 / CubeIDE 默认即可。

## 配置项

| 宏 | 默认值 | 说明 |
|---|---|---|
| `SCI_PARSE_MAX_VALUES` | 8 | 一帧最多解析几个数 |
| `SCI_PARSE_MAX_FIELD_LEN` | 15 | 单个数值最多多少个字符 |
| `SCI_RX_MAX` | 64 | 一帧最多多少字节 |

三个宏都可以在编译时用 `-D` 覆盖，或直接改头文件。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

当前覆盖 21 项：正常三值、`\r\n` 结尾、逗号后空格、单值、正负号、六值、结尾多逗号、
非 `'\0'` 结尾、空串、空白、非法字符、两个小数点、字段内空格、空字段、单个负号、
超长数值、超出 `out_max`、`out_max=0`、收帧换行、空行忽略、超长帧丢弃。

## 版本

- 1.0.0 —— 首个版本：`sci_parse_ascii` + `sci_rx_*` + PC 演示 + 21 项单元测试

## 许可证

[MIT](LICENSE)

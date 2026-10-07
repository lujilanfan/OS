# 朱彦泽答辩稿：SBI、控制台与格式化输出

> 身份：拟定的输出链答辩负责人。实验实际由李潇健在当前电脑执行；本稿用于解释已有源码和本机结果，不能说自己在另一台电脑运行过或亲自截过图。

## 一、先讲总体架构：做了什么、为什么做、怎样做

**做了什么。** Lab1 构建最小 RISC-V 内核，QEMU 4.1.1 启动后打印 `(THU.CST) os is loading ...`，再进入死循环。GDB 验证 `0x1000` 复位、`0x80000000` OpenSBI、`0x80200000` 内核入口，以及入口建立 SP 和尾跳转。指导书正式练习是解释 `la`/`tail` 与复位到内核的调试过程；输出链是理解核心模块的答辩重点，不是新增编程题。

**为什么做。** 最小内核没有 Linux 的 `printf` 或宿主标准库，S 模式代码也不能把“向终端写字符”当作现成魔法。必须自己把格式字符串拆成字符，经 console 层交给 SBI `ecall`，再由 OpenSBI 提供服务。出现内核消息证明控制流至少到达了 C 入口和输出链。

**怎样做。** 源码编译链接为 ELF，QEMU 在 guest 运行前把裸镜像放在 `0x80200000`；复位代码在 `0x1000` 准备参数并跳到 `0x80000000` 的 OpenSBI；固件交权到 `kern_entry`，入口建栈后进入 `kern_init`，后者调用 `cprintf`。输出链为：

```text
kern_init 的 cprintf("%s\n\n", message)
→ vcprintf → vprintfmt（解释 %s，逐字符调用回调）
→ cputch → cons_putc → sbi_console_putchar
→ sbi_call：a7=服务号 1；a0=字符；ecall → OpenSBI → QEMU 终端
```

三人共同应懂的总图：源码→ELF→裸镜像，与 guest PC 的 `0x1000 → 0x80000000 → 0x80200000` 是两条不同的链；前者是文件的构建/预装载，后者是虚拟 CPU 的执行。QEMU 预装载，OpenSBI 初始化并交权。

## 二、我的负责部分的详细架构

| 层 | 关键文件 | 职责与调用方向 |
|---|---|---|
| C 入口 | `kern/init/init.c` | 构造字符串，调用 `cprintf`，之后不返回 |
| 高层控制台 | `kern/libs/stdio.c` | 可变参数、计数、逐字符回调 |
| 格式解析 | `libs/printfmt.c` | 识别 `%s` 等格式，调用传入的 `putch` |
| 设备抽象 | `kern/driver/console.c` | `cons_putc` 把一个字符交给 SBI |
| 固件接口 | `libs/sbi.c` | 设置寄存器后 `ecall`，使用 legacy SBI 控制台输出 |

打印流程实际涉及 `%s`、普通字符和换行；`printfmt.c` 的数字、错误码、缓冲区格式化分支是可复用库能力，不要声称本次消息测试覆盖了所有格式。输入相关 `getchar/readline/cons_getc` 在源码里有声明/实现片段，但当前最小内核没有调用它们。尤其 `sbi_console_getchar` 在当前 `sbi.c` 中只有声明没有定义；链接成功依赖未引用函数节被丢弃，不表示当前输入功能可运行。

## 三、按实际源码逐段加注释

### 3.1 `kern/init/init.c` 中触发输出的代码

```c
const char *message = "(THU.CST) os is loading ...\n";
cprintf("%s\n\n", message); // 【重点】%s 输出 message（自身已有换行），随后再输出两个换行。
while (1)                      // 输出之后不再运行其他任务，所以 QEMU 保持运行。
    ;
```

打印内容的前缀不等于我们真的运行了 THU 的完整课程源码；只是现有最小内核字符串。`cprintf` 在这里是内核自己实现的函数。

### 3.2 `kern/libs/stdio.c`：所有函数及声明关系

```c
static void cputch(int c, int *cnt) {
    cons_putc(c);            // 【重点】每个格式化后的字符都流向 console。
    (*cnt)++;                // 累计本次 cprintf 输出的字符数。
}

int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap); // 传回调、计数器、格式串、参数表。
    return cnt;
}

int cprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);       // 由编译器内建接口定位可变参数。
    cnt = vcprintf(fmt, ap); // 【重点】从这里进入格式化层。
    va_end(ap);              // 对应结束；当前 stdarg.h 宏实现为空操作。
    return cnt;
}

void cputchar(int c) { cons_putc(c); } // 单字符直接输出接口；本次启动字符串未用。

int cputs(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') cputch(c, &cnt); // 字符串每字节输出。
    cputch('\n', &cnt);       // 末尾自动加换行。
    return cnt;
}

int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0) /* do nothing */; // 等待输入；当前主路径未调用。
    return c;
}
```

`libs/stdio.h` 声明上述 `cprintf/vcprintf/cputchar/cputs/getchar`，还声明 `readline`、`printfmt/vprintfmt/snprintf/vsnprintf`；`libs/stdarg.h` 把 `va_list` 和 `va_start/va_arg/va_end` 对应到编译器内建能力。这里的 `va_list` 是参数迭代机制，不代表从用户态陷入内核。

### 3.3 `libs/printfmt.c`：格式化模块的全部执行分支

文件开头 `error_string[MAXERROR+1]` 用 `libs/error.h` 的错误码映射为文字，供 `%e` 使用。`getuint/getint` 根据 `lflag` 取普通、`long`、`long long` 的变参；有符号与无符号要分开处理。实际本次 `%s` 不走它们。

```c
static void printnum(void (*putch)(int, void*), void *putdat,
                     unsigned long long num, unsigned base, int width, int padc) {
    unsigned long long result = num;
    unsigned mod = do_div(result, base); // 【重点】取余数，同时把 result 改为商。
    if (num >= base)
        printnum(putch, putdat, result, base, width - 1, padc); // 先递归高位。
    else
        while (--width > 0) putch(padc, putdat); // 高位前填充。
    putch("0123456789abcdef"[mod], putdat);     // 再输出当前最低位。
}

static unsigned long long getuint(va_list *ap, int lflag) {
    if (lflag >= 2) return va_arg(*ap, unsigned long long); // ll
    else if (lflag) return va_arg(*ap, unsigned long);       // l
    else return va_arg(*ap, unsigned int);                   // 默认
}

static long long getint(va_list *ap, int lflag) {
    if (lflag >= 2) return va_arg(*ap, long long);
    else if (lflag) return va_arg(*ap, long);
    else return va_arg(*ap, int);
}

void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintfmt(putch, putdat, fmt, ap); // 包装可变参数版本。
    va_end(ap);
}
```

`do_div` 在 `libs/riscv.h` 中定义：计算 `n % base` 作为返回余数，并把 `n` 原地改成 `n / base`。该头文件还有大量 CSR 编号/读写宏，供后续底层操作使用；本次输出链直接使用的是 `do_div`，不把未调用的 CSR 宏说成现已完成异常/页表功能。

`vprintfmt` 是核心。按原文件的所有分支逐项解释：

```c
while (1) {
    while ((ch = *(unsigned char *)fmt++) != '%') {
        if (ch == '\0') return; // 格式串结束。
        putch(ch, putdat);      // 【重点】普通字符直接回调输出。
    }
    char padc = ' ';
    width = precision = -1;
    lflag = altflag = 0;       // 为当前一个 % 序列重置解析状态。
reswitch:
    switch (ch = *(unsigned char *)fmt++) {
    case '-': padc = '-'; goto reswitch;   // 左对齐标记。
    case '0': padc = '0'; goto reswitch;   // 用 0 补数位。
    case '1' ... '9': /* 逐位读宽度；先放 precision，交给 process_precision 决定宽/精度。 */
    case '*':         /* 从变参中读宽度或精度。 */
    case '.':         /* 之后数字属于精度。 */
    case '#': altflag = 1; goto reswitch; // 对字符串中不可打印字符用 ? 代替。
    case 'l': lflag++; goto reswitch;     // 一个 l/两个 ll 控制整数尺寸。
    case 'c': putch(va_arg(ap, int), putdat); break; // 字符参数。
    case 'e': /* 错误码取绝对值；查 error_string，未知则输出 error N。 */ break;
    case 's': /* 【重点】取 char*；NULL→"(null)"；处理宽度/精度，然后逐字符 putch。 */ break;
    case 'd': /* getint；负数先输出 -；转入 number: 十进制。 */ break;
    case 'u': /* getuint；十进制。 */ break;
    case 'o': /* getuint；八进制。 */ break;
    case 'p': /* 输出 0x；取指针并按十六进制打印。 */ break;
    case 'x': /* getuint；十六进制。 */ break;
number:      /* 上述数字格式共用 printnum。 */
    case '%': /* 输出一个字面量 %。 */ break;
    default:  /* 不认识的 %-序列按原字符输出。 */ break;
    }
}
```

上面的 `switch` 是**教学流程伪代码**，用于说明每个源码分支，不可复制替换项目文件。要逐行核对时请打开 [`libs/printfmt.c`](../../code/libs/printfmt.c) 第 117—276 行。真正的 `%s` 路径：`message` 被取出 → 宽度不足时可补空格 → 逐字符回调 `putch` → 指针碰到 `\0` 或达到精度后停止。由于本次格式是 `%s\n\n`，随后两个普通换行也经过同一回调。

文件尾还有缓冲区版本，不走本次 QEMU 打印路径：

```c
struct sprintbuf { char *buf; char *ebuf; int cnt; }; // 当前写位置、末尾限制、总计数。
static void sprintputch(int ch, struct sprintbuf *b) {
    b->cnt++;
    if (b->buf < b->ebuf) *b->buf++ = ch; // 容量够才写入，但总计数仍增加。
}
int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int cnt = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return cnt;
}
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    struct sprintbuf b = {str, str + size - 1, 0};
    if (str == NULL || b.buf > b.ebuf) return -E_INVAL;
    vprintfmt((void*)sprintputch, &b, fmt, ap);
    *b.buf = '\0';                 // 保留结尾空字节。
    return b.cnt;                  // 返回理论生成字符数，不计结尾 \0。
}
```

以上摘录为了讲解合并了原源码行；`vsnprintf` 的 `size=0`/空指针检查顺序有边界疑点，本次没有调用它，也没有进行独立格式化库测试，不能拿内核打印成功证明所有格式与边界都正确。`libs/string.c` 里的 `strnlen` 被 `%s` 的宽度逻辑使用：最多看 `len` 字节，遇 `\0` 先停止；`libs/error.h` 的 `E_.../MAXERROR` 只为 `%e` 提供映射。

### 3.4 `kern/driver/console.c`：输出和未用输入函数

```c
void kbd_intr(void) {}          // 键盘中断占位，当前没有真实处理逻辑。
void serial_intr(void) {}       // 串口中断占位。
void cons_init(void) {}         // 控制台初始化占位；不能说已配置设备驱动。
void cons_putc(int c) {
    sbi_console_putchar((unsigned char)c); // 【重点】把单字符交给 SBI。
}
int cons_getc(void) {
    int c = 0;
    c = sbi_console_getchar();  // 本包没有该函数定义；此路径未用于当前内核。
    return c;
}
```

`kern/driver/console.h` 只声明上述五个接口，不负责具体实现。`libs/readline.c` 用 `getchar` 读一行到 1024 字节静态缓冲区，处理退格/换行并回显；当前 `kern_init` 没有调用，不能据此宣称交互输入可用。

### 3.5 `libs/sbi.c`：全部定义与寄存器约定

```c
uint64_t SBI_SET_TIMER = 0;
uint64_t SBI_CONSOLE_PUTCHAR = 1; // 【重点】legacy SBI 的字符输出服务号。
uint64_t SBI_CONSOLE_GETCHAR = 2;
uint64_t SBI_CLEAR_IPI = 3;
uint64_t SBI_SEND_IPI = 4;
uint64_t SBI_REMOTE_FENCE_I = 5;
uint64_t SBI_REMOTE_SFENCE_VMA = 6;
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;
uint64_t SBI_SHUTDOWN = 8;
// 以上是当前文件定义的服务号变量；定义不等于功能都已调用/实现。

uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t ret_val;
    __asm__ volatile (
        "mv x17, %[sbi_type]\n" // 【重点】x17 即 a7，放 SBI legacy 服务号。
        "mv x10, %[arg0]\n"     // x10 即 a0，第一个参数/返回寄存器。
        "mv x11, %[arg1]\n"     // x11 即 a1。
        "mv x12, %[arg2]\n"     // x12 即 a2。
        "ecall\n"              // 【重点】S 模式请求更高特权级固件服务。
        "mv %[ret_val], x10"    // 从 a0 取回结果。
        : [ret_val] "=r" (ret_val)
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0),
          [arg1] "r" (arg1), [arg2] "r" (arg2)
        : "memory"
    );
    return ret_val;
}
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0); // 实际输出路径。
}
void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0); // 存在但本次启动消息未调用。
}
```

`libs/sbi.h` 定义了 `memory_block_info` 结构和 `sbi_query_memory/sbi_set_timer/sbi_send_ipi/sbi_clear_ipi/sbi_shutdown/sbi_console_putchar/sbi_console_getchar` 等声明。当前 `sbi.c` 只实现字符输出、定时器设置及内部调用封装；其余声明不能当作已实现功能。`ecall` 从 S 模式触发异常并由固件处理，与用户态应用发起 Linux 系统调用的方向和服务边界不同。OpenSBI 版本在本机显示 v0.4，Runtime SBI Version 为 0.1；本代码的服务号/参数布局对应 legacy SBI，不应套用现代 SBI 扩展号与 function ID 的约定。

负责代码复核表：`sbi.c` 1—38 行包括全部服务号变量、汇编调用、字符输出和定时器函数；`sbi.h` 1—20 行是结构体与各声明；`console.c` 1—24 行是三个占位函数及 `cons_putc/cons_getc`，`console.h` 1—11 行是对应声明；`stdio.c` 1—68 行的 `cputch/vcprintf/cprintf/cputchar/cputs/getchar` 均在 3.2 注释；`printfmt.c` 1—340 行的错误表、`printnum/getuint/getint/printfmt/vprintfmt` 全部分支和 `sprintbuf/sprintputch/snprintf/vsnprintf` 在 3.3 注释。`stdio.h`、`stdarg.h`、`error.h` 的声明/宏用途也在相应段落解释。`string.c` 很大，本答辩范围只涉及被格式化链调用的 `strnlen`；李潇健的稿负责 `memset`。其余字符串函数是当前工程的通用库，不构成本次已测试的输出链。

## 四、如何现场证明输出链

实际操作人用原 `make qemu QEMU=/home/lxj/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64` 查看内核消息，并在 GDB 从 `0x1000 → 0x80000000 → 0x80200000` 后进入 `kern_init`。如果老师要代码证据，按文件顺序指给他看 `init.c` 的 `cprintf`、`stdio.c` 的 `vcprintf/cputch`、`printfmt.c` 的 `%s`、`console.c` 的 `cons_putc`、`sbi.c` 的 `a7/a0/ecall`。这些构成因果链；只看到消息不能证明未调用的输入、定时器、各种格式都通过。

## 五、老师可能继续追问

1. **打印为什么不是直接用宿主 `printf`？** 裸机内核没有宿主 glibc；用自己的格式化层和 SBI 字符输出。
2. **`cprintf` 如何理解 `%s`？** `va_start` 取得变参，`vprintfmt` 解析 `%s`，逐字符调 `cputch`。
3. **`putch` 是哪一层的函数？** `vprintfmt` 接收的回调；本次传入 `cputch`，缓冲区格式化可传另一种回调 `sprintputch`。
4. **SBI 调用最关键的寄存器？** legacy SBI 中 a7/x17 放服务号，a0/x10 放第一个参数并承接返回值；字符输出服务号为 1。
5. **`ecall` 后去了哪里？** 当前固件的陷入处理，提供 SBI 服务，返回内核继续执行；不是跳回 QEMU 宿主 C 代码给 GDB 逐行看。
6. **有输入功能吗？** 声明和若干包装函数存在，但本包缺 `sbi_console_getchar` 实现，当前内核未调用输入链，不能说已验证交互输入。
7. **为什么没看到内核退出？** `kern_init` 在打印后 `while (1)`；这正是源码预期。
8. **本实验已经有设备驱动吗？** `console` 是调用 SBI 的薄封装，`kbd_intr/serial_intr/cons_init` 是空函数，不能说已实现完整驱动。

## 六、OS 原理和共同复习清单

这部分体现固件接口、特权级转换、ABI、模块分层及“内核不能依赖宿主运行库”。实验未实现系统调用、用户进程、外设中断处理或真正的交互控制台。答辩前要和另两位交叉练习三个地址、两道正式题、ELF/裸镜像区别、QEMU 预装载及 `grade.sh` 缺失；只熟悉输出链不足以应付整体提问。

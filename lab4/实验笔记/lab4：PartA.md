# 实验 4：抢占式多任务处理

## 介绍

在本实验中，您将在多个同时处于活动状态的用户模式环境中实现抢占式多任务处理。

在 A 部分，您将为 JOS 添加多处理器支持，实现**循环调度**，并添加基本的环境管理系统调用（创建和销毁环境以及分配/映射内存的调用）。同时实现了**spinlock**和**sleep lock**

在 B 部分，您将实现一个类 Unix 的**fork**，它允许用户模式环境创建其自身的副本。

最后，在 C 部分，您将添加对**进程间通信** (IPC) 的支持，允许不同的用户模式环境显式地相互通信和同步。您还将添加对硬件时钟中断和抢占的支持。

### 入门

使用 Git 提交您的 Lab 3 源，获取课程存储库的最新版本，然后基于我们的 lab4 分支`origin/lab4`创建一个名为`lab4`的本地分支：``

```
athena%git checkout -b lab4 origin/lab4
分支 lab4 设置为跟踪远程分支 refs/remotes/origin/lab4。
切换到一个新的分支“lab4” 
athena% git merge lab3
Merge 通过递归进行。
```

实验 4 包含许多新的源文件，您应该在开始之前浏览其中的一些：

| `kern/cpu.h`      | 多处理器支持的内核私有定义             |
| ----------------- | -------------------------------------- |
| `kern/mpconfig.c` | 读取多处理器配置的代码                 |
| `kern/lapic.c`    | 驱动每个处理器中的LAPIC 单元的内核代码 |
| `kern/mpentry.S`  | 非引导 CPU 的汇编语言入口代码          |
| `kern/spinlock.h` | 自旋锁的内核私有定义，包括大内核锁     |
| `kern/spinlock.c` | 实现自旋锁的内核代码                   |
| `kern/sched.c`    | 您将要实现的调度程序的代码框架         |

## A 部分：多处理器支持和协作多任务处理

在本实验的第一部分，您将首先扩展 JOS 以在多处理器系统上运行，然后实现一些新的 JOS 内核系统调用以允许用户级环境创建额外的新环境。您还将实现*协作*循环调度，允许内核在当前环境自愿放弃 CPU（或退出）时从一种环境切换到另一种环境。在后面的 C 部分中，您将实现*抢占式*调度，即使环境不合作，它也允许内核在经过一定时间后从环境中重新控制 CPU。

### 多处理器支持

我们将让 JOS 支持“对称多处理”（SMP），这是一种多处理器模型，其中所有 CPU 对系统资源（如内存和 I/O 总线）具有同等访问权限。尽管 SMP 中所有 CPU 的功能都相同，但在引导过程中它们可以分为两种类型：

1. 引导处理器 (BSP) 负责初始化系统和引导操作系统
2. 并且应用处理器（AP）只有在操作系统启动并运行后才被 BSP 激活。哪个处理器是 BSP 由硬件和 BIOS 决定。到目前为止，您现有的所有 JOS 代码都已在 BSP 上运行。

在 SMP 系统中，每个 CPU 都有一个伴随的本地 APIC (LAPIC) 单元。LAPIC 单元负责在整个系统中**传送中**断。LAPIC 还为其连接的 CPU 提供唯一标识符。在本实验中，我们使用 LAPIC 单元（在`kern/lapic.c 中`）的以下基本功能：

- 读取 LAPIC 标识符 (APIC ID) 以了解我们的代码当前正在哪个 CPU 上运行（请参阅 参考资料`cpunum()`）。
- 发送`STARTUP`从BSP到的AP间中断（IPI）带来的其他CPU（见图 `lapic_startap()`）。
- 在 C 部分，我们对 LAPIC 的内置计时器进行编程以触发时钟中断以支持抢占式多任务处理（请参阅 参考资料 `apic_init()`）。

处理器使用内存映射 I/O (MMIO) 访问其 LAPIC。在 MMIO 中，一部分*物理*内存硬连线到一些 I/O 设备的寄存器，因此通常用于访问内存的相同加载/存储指令可用于访问设备寄存器。您已经在物理地址`0xA0000`处看到了一个 IO 孔 （我们使用它来写入 VGA 显示缓冲区）。LAPIC 位于从物理地址`0xFE000000`（32MB 比 4GB 短）开始的一个洞中 ，所以它太高了，我们无法使用我们通常在 KERNBASE 的直接映射来访问。JOS 虚拟内存映射在`MMIOBASE`上留下了 4MB 的空白,所以我们需要有一个地方可以映射这样的设备。由于后面的实验会引入更多的 MMIO 区域，您将编写一个简单的函数来从该区域分配空间并将设备内存映射到它。

## Exercise 1

实现`mmio_map_region`在`kern/ pmap.c`。要看到这是如何使用的，看看 `lapic_init`在`kern/ lapic.c`。在`mmio_map_region`运行测试之前，您还必须进行下一个练习 。

#### 应用处理器引导程序

在启动 AP 之前，BSP 应首先收集有关多处理器系统的信息，例如 CPU 总数、它们的 APIC ID 和 LAPIC 单元的 MMIO 地址。`kern/mpconfig.c 中`的`mp_init()`函数 通过读取驻留在 BIOS 内存区域中的 MP 配置表来检索此信息。``

该`boot_aps()`函数（在`kern/init.c 中`）驱动 AP 引导程序。AP 以实模式启动，很像引导加载程序在`boot/boot.S`中`启动的方式`，因此`boot_aps()` 将 AP 入口代码 ( `kern/mpentry.S` )`复制`到可在实模式下寻址的内存位置。与引导加载程序不同，我们可以控制 AP 开始执行代码的位置；我们将入口代码复制到`0x7000` ( `MPENTRY_PADDR`)，但任何未使用的、页面对齐的低于 640KB 的物理地址都可以使用。

之后`boot_aps()`，通过向`STARTUP`相应 AP 的 LAPIC 单元发送IPI 以及`CS:IP`AP 应开始运行其入口代码（`MPENTRY_PADDR`在我们的示例中）的初始地址，一个接一个地激活 AP 。`kern/mpentry.S 中`的入口代码与`boot/boot.S 中`的入口代码非常相似。经过一些简短的设置后，它会将 AP 置于启用分页的保护模式，然后调用 C 设置例程`mp_main()`（也在`kern/init.c 中`）。 在继续唤醒下一个之前，`boot_aps()`等待 AP`CPU_STARTED`在`cpu_status`其字段中发出标志信号 `struct CpuInfo`。

## Exercise 2

读取`boot_aps()`和`mp_main()`在 `kern/ init.c`，并在汇编代码 `kern/ mpentry.S`。确保您了解 AP 引导期间的控制流传输。然后修改您`page_init()`在`kern/pmap.c 中的实现`以避免将页面添加`MPENTRY_PADDR`到空闲列表中，以便我们可以安全地在该物理地址复制和运行 AP 引导代码。您的代码应该通过更新的`check_page_free_list()` 测试（但可能无法通过更新的`check_kern_pgdir()` 测试，我们将很快修复）。

**题**

1. 将`kern/mpentry.S`与 `boot/boot.S 并排比较`。记住`kern/mpentry.S` 被编译并链接到上面运行，`KERNBASE`就像内核中的其他东西一样，宏的目的是 `MPBOOTPHYS`什么？为什么需要在`kern/mpentry.S 中`而不是在 `boot/boot.S 中`？换句话说，如果在`kern/mpentry.S`中省略它会出什么问题？
   提示：回忆一下我们在实验 1 中讨论过的链接地址和加载地址之间的区别。

#### 每个 CPU 状态和初始化

在编写多处理器操作系统时，区分每个处理器私有的每个 CPU 状态和整个系统共享的全局状态很重要。 `kern/cpu.h`定义了大部分 per-CPU 状态，包括`struct CpuInfo`存储 per-CPU 变量的 。 `cpunum()`总是返回调用它的 CPU 的 ID，它可以用作数组的索引，如 `cpus`. 或者，该宏`thiscpu`是当前 CPU 的`struct CpuInfo`.

以下是您应该注意的每个 CPU 状态：

- **每 CPU 内核堆栈**。
  由于多个 CPU 可以同时陷入内核，因此我们需要为每个处理器使用单独的内核堆栈，以防止它们干扰彼此的执行。该数组 `percpu_kstacks[NCPU][KSTKSIZE]`为 NCPU 的内核堆栈保留空间。

  在实验 2 中，您映射了`bootstack` 位于 下方的称为 BSP 内核堆栈 的物理内存`KSTACKTOP`。类似地，在本实验中，您将每个 CPU 的内核堆栈映射到该区域，保护页面充当它们之间的缓冲区。CPU 0 的堆栈仍将从`KSTACKTOP`; CPU 1 的堆栈将从`KSTKGAP`CPU 0 堆栈底部下方的字节开始，依此类推。`inc/memlayout.h`显示映射布局。

- **每 CPU TSS 和 TSS 描述符**。
  每个 CPU 的任务状态段 (TSS) 也需要用于指定每个 CPU 的内核堆栈所在的位置。CPU *i*的 TSS存储在 中`cpus[i].cpu_ts`，相应的 TSS 描述符在 GDT 条目中定义`gdt[(GD_TSS0 >> 3) + i]`。`kern/trap.c 中``ts`定义的全局变量将不再有用。``

- **每个 CPU 当前环境指针**。
  由于每个 CPU 可以同时运行不同的用户进程，我们重新定义了符号`curenv`来引用 `cpus[cpunum()].cpu_env`（或`thiscpu->cpu_env`），它指向*当前*在*当前*CPU（运行代码的 CPU）上执行 的环境。

- **每个 CPU 系统寄存器**。
  所有寄存器，包括系统寄存器，都是 CPU 私有的。因此，初始化这些寄存器的指令，例如`lcr3()`， `ltr()`，`lgdt()`，`lidt()`等，必须进行一次各CPU上执行。函数`env_init_percpu()` 和`trap_init_percpu()`就是为此目的而定义的。

- 除此之外，如果您在解决方案中添加了任何额外的每个 CPU 状态或执行了任何额外的特定于 CPU 的初始化（例如，在 CPU 寄存器中设置新位）以挑战早期实验室中的问题，请务必复制它们在每个 CPU 上！

## Exercise 3

修改`mem_init_mp()`（在`kern/pmap.c 中`）以映射从 开始的每个 CPU 堆栈`KSTACKTOP`，如`inc/memlayout.h 中`所示 。每个堆栈的大小是 `KSTKSIZE`字节加上`KSTKGAP`未映射保护页的字节。您的代码应该通过新的检查 `check_kern_pgdir()`。

## Exercise 4

`trap_init_percpu()` ( `kern/trap.c` ) 中 的代码初始化 BSP 的 TSS 和 TSS 描述符。它在实验室 3 中工作，但在其他 CPU 上运行时不正确。更改代码，使其可以在所有 CPU 上运行。（注意：您的新代码不应再使用全局 `ts`变量。）

完成上述练习后，在带有 4 个 CPU 的 QEMU 中使用make qemu CPUS=4(或make qemu-nox CPUS=4)运行 JOS ，您应该看到如下输出：

```
...
物理内存：66556K 可用，base = 640K，extended = 65532K 
check_page_alloc() 成功！
check_page() 成功！
check_kern_pgdir() 成功！
check_page_installed_pgdir() 成功！
SMP：CPU 0 发现 4 个 CPU
启用中断：1 2 
SMP：CPU 1 启动
SMP：CPU 2 启动
SMP：CPU 3 启动
```

#### 锁定

我们当前的代码在 `mp_main()`. 在让 AP 更进一步之前，我们需要首先解决多个 CPU 同时运行内核代码时的竞争条件。实现这一点的最简单方法是使用*大内核锁*。大内核锁是一个全局锁，每当环境进入内核模式时都会持有，并在环境返回用户模式时释放。在这个模型中，用户态的环境可以在任何可用的 CPU 上并发运行，但内核态下只能运行一个环境；任何其他尝试进入内核模式的环境都被迫等待。

`kern/spinlock.h`声明了大内核锁，即 `kernel_lock`. 它还提供`lock_kernel()` 和`unlock_kernel()`，获取和释放锁的快捷方式。您应该在四个位置应用大内核锁：

- 在 中`i386_init()`，在 BSP 唤醒其他 CPU 之前获取锁。
- 中`mp_main()`，初始化AP后获取锁，然后调用`sched_yield()`在该AP上启动运行环境。
- 在`trap()`，从用户模式被困时获取锁。要确定陷阱发生在用户模式还是内核模式，请检查`tf_cs`.
- 在 中`env_run()`，*在* 切换到用户模式*之前*释放锁定。不要太早或太晚这样做，否则你会遇到竞争或僵局。

## Exercise 5

通过在适当的位置调用`lock_kernel()`和`unlock_kernel()`，如上所述应用大内核锁 。

如何测试您的锁定是否正确？你现在不能！但是在下一个练习中实现调度程序后，您将能够做到。

**题**

1. 似乎使用大内核锁可以保证一次只有一个 CPU 可以运行内核代码。为什么我们仍然需要为每个 CPU 使用单独的内核堆栈？描述一个使用共享内核堆栈会出错的场景，即使有大内核锁的保护。

*挑战！* 大内核锁简单易用。尽管如此，它消除了内核模式下的所有并发性。大多数现代操作系统使用不同的锁来保护其共享状态的不同部分，这种方法称为*细粒度锁定*。细粒度锁定可以显着提高性能，但更难实现且容易出错。如果你足够勇敢，放下大内核锁，拥抱 JOS 中的并发！

由您决定锁定粒度（锁定保护的数据量）。作为提示，您可以考虑使用自旋锁来确保对 JOS 内核中这些共享组件的独占访问：

- 页面分配器。
- 控制台驱动程序。
- 调度程序。
- 您将在 C 部分中实现的进程间通信 (IPC) 状态。

### 循环调度

您在本实验中的下一个任务是更改 JOS 内核，以便它可以以“循环”方式在多个环境之间交替。JOS 中的循环调度工作如下：

- 该功能`sched_yield()`在新的`克恩/ sched.c中` 负责选择一个新的环境中运行。它`envs[]`以循环方式在数组中顺序搜索，从之前运行的环境之后开始（或者如果之前没有运行环境，则在数组的开头），选择它找到的第一个状态为`ENV_RUNNABLE` （参见`inc/env.env ）的环境。 h`），并调用`env_run()`跳转到该环境。
- `sched_yield()`绝不能同时在两个 CPU 上运行相同的环境。它可以判断环境当前正在某个 CPU（可能是当前 CPU）上运行，因为该环境的状态将为`ENV_RUNNING`.
- 我们为您实现了一个新的系统调用 `sys_yield()`，用户环境可以调用它来调用内核的`sched_yield()`功能，从而自愿将 CPU 交给不同的环境。

## Exercise 6

`sched_yield()` 如上所述实施循环调度。不要忘记修改 `syscall()`为 dispatch `sys_yield()`。

确保`sched_yield()`在`mp_main`.

修改`kern/init.c`以创建三个（或更多！）运行程序`user/yield.c 的环境`。

运行make qemu。在终止之前，您应该看到环境在彼此之间来回切换五次，如下所示。

还使用多个 CPU 进行测试：make qemu CPUS=2.

```
...
您好，我是环境 00001000。
您好，我是环境 00001001。
您好，我是环境 00001002。
返回环境 00001000，迭代 0。
返回环境 00001001，迭代 0。
返回环境 00001002，迭代 0 
。环境 00001000，迭代 1。
回到环境 00001001，迭代 1。
回到环境 00001002，迭代 1 
。...
```

在之后`的产量`计划退出，会有系统中没有可运行环境，调度应该调用内核JOS显示器。如果其中任何一个没有发生，请在继续之前修复您的代码。

**题**

1. 在您的实现中，`env_run()`您应该调用`lcr3()`. 在调用 之前和之后 `lcr3()`，您的代码会引用（至少应该）变量`e`，即 的参数`env_run`。加载`%cr3`寄存器后，MMU 使用的寻址上下文会立即更改。但是虚拟地址（即`e`）相对于给定的地址上下文具有意义——地址上下文指定了虚拟地址映射到的物理地址。为什么指针`e`在寻址切换之前和之后都可以取消引用？
2. 每当内核从一种环境切换到另一种环境时，它必须确保旧环境的寄存器得到保存，以便以后可以正确恢复。为什么？这是在哪里发生的？

*挑战！* 向内核添加一个不太重要的调度策略，例如固定优先级调度程序，它允许为每个环境分配一个优先级，并确保始终优先选择较高优先级的环境而不是较低优先级的环境。如果您真的喜欢冒险，请尝试实施 Unix 风格的可调整优先级调度程序，甚至是彩票或跨步调度程序。（在谷歌中查找“彩票调度”和“步幅调度”。）

编写一两个测试程序来验证您的调度算法是否正常工作（即，正确的环境以正确的顺序运行）。一旦您`fork()`在本实验的 B 和 C 部分实施和 IPC，编写这些测试程序可能会更容易。

*挑战！* JOS 内核目前不允许应用程序使用 x86 处理器的 x87 浮点单元 (FPU)、MMX 指令或流式 SIMD 扩展 (SSE)。扩展`Env`结构为处理器的浮点状态提供一个保存区域，并扩展上下文切换代码以在从一种环境切换到另一种环境时正确保存和恢复该状态。该`FXSAVE`和`FXRSTOR`指令可能是有用的，但要注意，这些都不是在旧的i386用户手册，因为他们在最近的处理器中引入。编写一个用户级测试程序，用浮点数做一些很酷的事情。

### 创建环境的系统调用

尽管您的内核现在能够在多个用户级环境之间运行和切换，但它仍然仅限于*内核*最初设置的运行环境。您现在将实现必要的 JOS 系统调用，以允许*用户*环境创建和启动其他新用户环境。

Unix 提供`fork()`系统调用作为其进程创建原语。Unix`fork()`复制调用进程（父进程）的整个地址空间以创建一个新进程（子进程）。用户空间中两个可观察对象之间的唯一区别是它们的进程 ID 和父进程 ID（由`getpid`和返回`getppid`）。在父`fork()`进程中， 返回子进程ID，而在子进程中，`fork()`返回0。默认情况下，每个进程都有自己的私有地址空间，并且两个进程对内存的修改对另一个都不可见。

您将提供一组不同的、更原始的 JOS 系统调用来创建新的用户模式环境。通过这些系统调用`fork()`，除了其他风格的环境创建之外，您还可以完全在用户空间中实现类 Unix 系统。您将为 JOS 编写的新系统调用如下：

- `sys_exofork`：

  这个系统调用创建了一个几乎空白的新环境：在其地址空间的用户部分没有映射任何内容，并且它是不可运行的。新环境将在`sys_exofork`调用时与父环境具有相同的注册状态。在父级中，`sys_exofork` 将返回`envid_t`新创建的环境（如果环境分配失败，则返回负错误代码）。然而，`sys_exofork`在子进程中，它将返回 0。（由于子进程一开始被标记为不可运行，因此在父进程通过使用...标记子进程可运行来明确允许之前， 它实际上不会在子进程中返回。）

- `sys_env_set_status`：

  将指定环境的状态设置为`ENV_RUNNABLE`或`ENV_NOT_RUNNABLE`。该系统调用通常用于标记新环境准备好运行，一旦其地址空间和寄存器状态已完全初始化。

- `sys_page_alloc`：

  分配一页物理内存并将其映射到给定环境地址空间中的给定虚拟地址。

- `sys_page_map`：

  将页面映射（*不是*页面的内容！）从一个环境的地址空间复制到另一个环境，保留内存共享安排，以便新旧映射都引用物理内存的同一页面。

- `sys_page_unmap`：

  取消映射在给定环境中的给定虚拟地址上映射的页面。

对于上述所有接受环境 ID 的系统调用，JOS 内核支持值 0 表示“当前环境”的约定。本公约对实现`envid2env()` 在`克恩/ env.c`。

我们`fork()` 在测试程序`user/dumbfork.c 中`提供了一个非常原始的类 Unix 实现。这个测试程序使用上面的系统调用来创建和运行一个子环境，它有自己的地址空间的副本。然后这两个环境来回切换，`sys_yield` 就像前面的练习一样。父进程在 10 次迭代后退出，而子进程在 20 次迭代后退出。

## Exercise 7

在`kern/syscall.c 中`实现上述系统调用，并确保`syscall()`调用它们。您将需要使用`kern/pmap.c`和`kern/env.c 中的`各种函数，特别是`envid2env()`. 现在，无论何时调用`envid2env()`，都在`checkperm`参数中传递 1 。确保检查任何无效的系统调用参数，`-E_INVAL`在这种情况下返回。使用`user/dumbfork`测试您的 JOS 内核 并确保它在继续之前可以正常工作。

*挑战！* 添加*读取*现有环境的所有重要状态以及对其进行设置所需的额外系统调用。然后实现一个用户模式程序，分叉子环境，运行一段时间（例如，几次迭代`sys_yield()`），然后获取 子环境的完整快照或*检查点*，运行子环境一段时间，最后恢复子环境到它在检查点所处的状态，并从那里继续它。因此，您可以有效地从中间状态“重放”子环境的执行。使用`sys_cgetc()`或使子环境与用户进行一些交互`readline()` 以便用户可以查看和改变其内部状态，并通过检查点/重启验证您可以给子环境一个选择性失忆的情况，使其“忘记”超过某个点发生的一切。

这完成了实验室的 A 部分；使用检查make grade并make handin照常使用。如果您试图找出特定测试用例失败的原因，请运行./grade-lab4 -v，它将显示内核构建的输出和 QEMU 为每个测试运行，直到测试失败。当测试失败时，脚本将停止，然后您可以检查`jos.out`以查看内核实际打印的内容。

# 回答问题汇总

## Exercise 1

kern/lapic中到底什么是有用的

```
- 读取 LAPIC 标识符 (APIC ID) （lapic=mmio_map_region（...））以了解我们的代码当前正在哪个 CPU 上运行（请参阅 参考资料cpunum()）。
- 发送STARTUP从BSP到的AP间中断（IPI）带来的其他CPU
- 在 C 部分，我们对 LAPIC 的内置计时器进行编程以触发时钟中断以支持抢占式多任务处理
```

mmio_map_region

```
void *
mmio_map_region(physaddr_t pa, size_t size)
{
	// 就像boot_alloc中的nextfree一样，base会在调用之间保存和迭代
	static uintptr_t base = MMIOBASE;

	// 从物理内存[pa,pa+size)映射并分配到虚拟内存[base,base+size)。由于这是设备内存而不是常规的DRAM，所以你应该在页表的位进行“不安全”的设置：PTE_PCD|PTE_PWT(cache-disable and write-through)，添加到PTE_W上。
	
	// Hint: The staff solution uses boot_map_region.
	//
	// Your code here:
	//我们的目的是对CPU进行映射，以便lapic能够被访问和传递中断，所以直接调用boot_map_region就好了
	//size对齐
	size = ROUNUP(size+pa, PGSIZE);
	size -= pa;
	if (base+size >= MMIOLIM) panic("mmio_map_region : out of memory");
	boot_map_region(kern_pgdir, base, size, pa, PTE_PCD|PTE_PWT|PTE_W);
	base += size;
	return (void*) (base - size);//lapic接受这个base
}
```



## Exercise 2

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/截屏2021-11-19 下午9.38.57.png)

多CPU来完成多线程的实现：每个AP共享内核部分（比如代码，用户页表），但是又有独有的栈和寄存器等内容。

给出Cpu_Info

```
struct CpuInfo {
	uint8_t cpu_id;                 // Local APIC ID; index into cpus[] below
	volatile unsigned cpu_status;   // The status of the CPU
	struct Env *cpu_env;            // The currently-running environment.
	struct Taskstate cpu_ts;        // Used by x86 to find stack for interrupt
};
```

给出AP初始化的部分

boot_aps()：

1. 分配空闲内存给CPU
2. 按时启动CPU
3. 给定CPU应该访问栈的位置mpentry
4. 等待基本设置

mp_main()：

1. 为每个在函数里可见的CPU进行kern_pgdir来EIP的切换，并进行独有的lapic，进程，中断初始化
2. 调用sched_yield()
3. 当CPU准备好的时候执行进程（用户环境）

AP 引导期间的控制流传输

```
对于SMP，我们的前面lab都是对这个CPU操作。
对于AP，我们基本流程为boot_aps->mpentry.S->mp_main，
对于每个AP，我们使用数组来存储cpu，针对每个cpu分配gdt，内存，特定的页表，栈，寄存器，来保证每个cpu陷入内核都是不一样的堆栈。然后我们分配完页表，并检查cpu state之后，我们会执行用户环境。
```

#### 每个 CPU 状态和初始化

在编写多处理器操作系统时，区分每个处理器私有的每个 CPU 状态和整个系统共享的全局状态很重要。 `kern/cpu.h`定义了大部分 per-CPU 状态，包括`struct CpuInfo`存储 per-CPU 变量的 。 `cpunum()`总是返回调用它的 CPU 的 ID，它可以用作数组的索引，如 `cpus`. 或者，该宏`thiscpu`是当前 CPU 的`struct CpuInfo`.

以下是您应该注意的每个 CPU 状态：

- **每 CPU 内核堆栈**。
  每个处理器使用单独的内核堆栈，以防止它们干扰彼此的执行。该数组 `percpu_kstacks[NCPU][KSTKSIZE]`为 NCPU 的内核堆栈保留空间。

  将每个 CPU 的内核堆栈映射到该区域，保护页面充当它们之间的缓冲区。CPU 0 的堆栈仍将从`KSTACKTOP`; CPU 1 的堆栈将从`KSTKGAP`CPU 0 堆栈底部下方的字节开始，依此类推。`inc/memlayout.h`显示映射布局。

- **每 CPU TSS 和 TSS 描述符**。
  每个 CPU 的任务状态段 (TSS) 也需要用于指定每个 CPU 的内核堆栈所在的位置。CPU *i*的 TSS存储在 中`cpus[i].cpu_ts`，相应的 TSS 描述符在 GDT 条目中定义`gdt[(GD_TSS0 >> 3) + i]`。`kern/trap.c 中ts`定义的全局变量将不再有用。

- **每个 CPU 当前环境指针**。
  由于每个 CPU 可以同时运行不同的用户进程，我们重新定义了符号`curenv`来引用 `cpus[cpunum()].cpu_env`（或`thiscpu->cpu_env`），它指向*当前*在*当前*CPU（运行代码的 CPU）上执行 的环境。

- **每个 CPU 系统寄存器**。
  所有寄存器，包括系统寄存器，都是 CPU 私有的。因此，初始化这些寄存器的指令，例如`lcr3()`， `ltr()`，`lgdt()`，`lidt()`等，必须进行一次各CPU上执行。函数`env_init_percpu()` 和`trap_init_percpu()`就是为此目的而定义的。

page_init()

```
//由于base memroy中需要将MPENTRY_PADDR需要将符合的部分use，所以在修改这部分代码
if(i == MPENTRY_PADDR / PGSIZE){
	pages[i].pp_ref = 1;
	pages[i].pp_link = NULL:
}
```

**题**

1. 将`kern/mpentry.S`与 `boot/boot.S 并排比较`。记住`kern/mpentry.S` 被编译并链接到上面运行，`KERNBASE`就像内核中的其他东西一样，宏的目的是 `MPBOOTPHYS`什么？为什么需要在`kern/mpentry.S 中`而不是在 `boot/boot.S 中`？换句话说，如果在`kern/mpentry.S`中省略它会出什么问题？
   提示：回忆一下我们在实验 1 中讨论过的链接地址和加载地址之间的区别。

和boot.S区别

1.不开启A20来进入保护模式

2.不通过linker进行符号表的加载，而是通过.code16来保证实模式下，MPBOOTPHS来计算加载地址

那么省略到底会有什么问题？首先是加载地址会被转换，而不通过宏，就会被linker加载到高地址，而不是低地址（boot时候进入A20保护模式已经有了一个简单的页表来映射到高地址）。

## Exercise 3

根据exercise2的理解，可以按照memlayout.h来写出代码

mem_init_mp()

```
static void
mem_init_mp(void)
{
	// 从 KSTACKTOP SMP顶开始向下映射每个cpu的内核栈：for 'NCPU' CPUs.
	//
	// 我们对每个CPU从0开始，但是从顶KSTACKTOP向下映射，同时每个栈分成实际部分和缓冲区，来保证“guard”，因此percpu_kstacks[i] 是内核栈的va我们使用宏转换为pa， 想要映射的部分顶是 KSTACKTOP - i * (KSTKSIZE + KSTKGAP)，权限是内核RW：PTE_W
	// mem_init:
	//     * [kstacktop_i - KSTKSIZE, kstacktop_i)
	//          -- backed by physical memory
	//     * [kstacktop_i - (KSTKSIZE + KSTKGAP), kstacktop_i - KSTKSIZE)
	//          -- not backed; so if the kernel overflows its stack,
	//             it will fault rather than overwrite another CPU's stack.
	//             Known as a "guard page".
	//     Permissions: kernel RW, user NONE
	//
	// LAB 4: Your code here:
	for (int i = 0; i < NCPU; i++) {
		boot_map_region(kern_pgdir, KSTACKTOP - KSTKSIZE - i * (KSTKSIZE + KSTKGAP), KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);//这里的va最为0号不必有gap，否则会报错
	}
}
```



## Exercise 4

对于gdt简单总结：gdt存放了cpu的Taskstate里的tss，而用户环境则通过CPUInfo里的env指针来找到，tss顺序存放，而不是linux初版本的那样LDT和TSS成对顺序存放

trap_init_percpu()

```


int i = thiscpu->cpu_id;
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	// 对cpu的特定修改
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);
	
	// Initialize the TSS slot of the gdt.
	//对于gdt也要修改，
	gdt[(GD_TSS0 >> 3)+i] = SEG16(STS_T32A, (uint32_t) (&(thiscpu->cpu_ts)),
					sizeof(struct Taskstate)-1, 0);
	gdt[(GD_TSS0 >> 3)+i].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	// 加载tss选择子来找到描述符，否则会报错。。。
	// 其中：我们往后加载时会将tss slector >> 3来忽略3bits作为索引，因此前后都要修改初始化的值确保最终加载正确
	ltr(GD_TSS0+(i << 3));
```



## Exercise 5

我们知道通过多CPU来完成多线程，但是由于大内核锁，因此内核线程切换都是用户环境下才能进行切换。若此而言，一个进程可以约等于一个线程。当我们切换进程时，仍然在同一个线程（内核独有部分不变）。

`kern/spinlock.h`声明了大内核锁，即 `kernel_lock`. 它还提供`lock_kernel()` 和`unlock_kernel()`，获取和释放锁的快捷方式。您应该在四个位置应用大内核锁：

- 在 中`i386_init()`，在 BSP 唤醒其他 CPU 之前获取锁。
- 中`mp_main()`，初始化AP后获取锁，然后调用`sched_yield()`在该AP上启动运行环境。
- 在`trap()`，从用户模式被困时获取锁。要确定陷阱发生在用户模式还是内核模式，请检查`tf_cs`.
- 在 中`env_run()`，*在* 切换到用户模式*之前*释放锁定。不要太早或太晚这样做，否则你会遇到竞争或僵局。给出锁的实现代码：jos现在并没有实现sleep（协调）



```
//我们将要插入代码原型
static inline void lock_kernel(void);
static inline void unlock_kernel(void);
//实际上是调用 了spin_lock(&kernel_lock)，给出关键代码实现xchg
//加锁
while (xchg(&lk->locked, 1) != 0)
		asm volatile ("pause");
//解锁
xchg(&lk->locked, 0);
```

上面的说明很清楚，这里省略代码粘贴

**题**

1. 似乎使用大内核锁可以保证一次只有一个 CPU 可以运行内核代码。为什么我们仍然需要为每个 CPU 使用单独的内核堆栈？描述一个使用共享内核堆栈会出错的场景，即使有大内核锁的保护。

在trapentry.S中，锁之前就压入了部分寄存器，所以如果共享堆栈，另一个CPU抢占使用就会破坏这部分。

## Exercise 6

sched_yield()

```
int cur = 0;
if(curenv)  cur = ENVX(curenv->env_id);//这里的curenv无判断即会kernel page fault，原因是env_id不存在，于是内核中断尝试寻找相应的映射，我们的策略应该是从0开始找可运行的进程
for(int i = 0; i < NENV; i++){
	int j = (i + cur) % NENV;
	if(envs[j].env_status == RUNNABLE){
		env_run(&envs[j]);
		break;
	}
}
if(curenv && curenv->env_status == RUNNING)env_run(curenv);
```

**题**

1. 我们的`env_run()`调用`lcr3()`. 在调用之前和之后 `lcr3()`，您的代码会引用e。加载`%cr3`寄存器后，MMU 使用的寻址上下文会立即更改。为什么指针`e`在寻址切换之前和之后都可以解引用？

e是thiscpu_curenv，对于内核来说不过是对于envs数组的引用。因此kern_pgdir和env_pgdir映射是一样的，实际上编写env_pgdir就是以前者为模板。

2. 每当内核从一种环境切换到另一种环境时，它必须确保旧环境的寄存器得到保存，以便以后可以正确恢复。为什么？这是在哪里发生的？

trapframe，在环境进入中断时压入寄存器，在trap从内核栈保存入tf，在env_run前pop_tf来读取然后进入新环境。

## Exercise 7

创造系统调用来使用户能够创建进程，而不是不同CPU下内核里创建env

1. sys_exofork()：创建子进程，不进行映射，但寄存器复制，使用id和0返回值来区分父子
2. sys_env_set_status：设置进程状态为ENV_RUNNABLE或ENV_NOT_RUNNABLE。
3. sys_page_alloc：为进程分配物理页
4. sys_page_map：拷贝页表来共享映射内存
5. sys_page_unmap：释放页面映射

sys_exofork(void)：

```
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env *e;
	int ret = env_alloc(&e, curenv->env_id);    
	if (ret < 0) {//bad env_alloc
		return ret;
	}
	e->env_tf = curenv->env_tf;			
	e->env_status = ENV_NOT_RUNNABLE;   
	e->env_tf.tf_regs.reg_eax = 0;		
	return e->env_id;
	//返回user-mode时候pop_tf，父进程返回trap_dispatch后eax设置为id,但是子进程通过env_run才从中断返回，所以eax为0
}
```

sys_env_set_status(envid_t envid, int status):

```
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.
	if (status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE) return -E_INVAL;

	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret < 0) {
		return ret;
	}
	e->env_status = status;
	return 0;
}
```

sys_page_alloc(envid_t envid, void *va, int perm):

```
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	struct Env *e; 									
	int ret = envid2env(envid, &e, 1);
	if (ret) return ret;	//bad_env

	if ((va >= (void*)UTOP) || (ROUNDDOWN(va, PGSIZE) != va)) return -E_INVAL;		
	int flag = PTE_U | PTE_P;
	if ((perm & flag) != flag) return -E_INVAL;

	struct PageInfo *pg = page_alloc(1);			
	if (!pg) return -E_NO_MEM;
	ret = page_insert(e->env_pgdir, pg, va, perm);	
	if (ret) {
		page_free(pg);
		return ret;
	}

	return 0;
}
```

sys_page_map(envid_t srcenvid, void *srcva,envid_t dstenvid, void *dstva, int perm):

```
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	struct Env *se, *de;
	int ret = envid2env(srcenvid, &se, 1);
	if (ret) return ret;	//bad_env
	ret = envid2env(dstenvid, &de, 1);
	if (ret) return ret;	//bad_env

	//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
	//		or dstva >= UTOP or dstva is not page-aligned.
	if (srcva >= (void*)UTOP || dstva >= (void*)UTOP || 
		ROUNDDOWN(srcva,PGSIZE) != srcva || ROUNDDOWN(dstva,PGSIZE) != dstva) 
		return -E_INVAL;

	//	-E_INVAL is srcva is not mapped in srcenvid's address space.
	pte_t *pte;
	struct PageInfo *pg = page_lookup(se->env_pgdir, srcva, &pte);
	if (!pg) return -E_INVAL;

	//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
	int flag = PTE_U|PTE_P;
	if ((perm & flag) != flag) return -E_INVAL;

	//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
	//		address space.
	if (((*pte&PTE_W) == 0) && (perm&PTE_W)) return -E_INVAL;

	//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
	ret = page_insert(de->env_pgdir, pg, dstva, perm);
	return ret;

}
```

sys_page_unmap(envid_t envid, void *va):

```
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env *env;
	int ret = envid2env(envid, &env, 1);
	if (ret) return ret;

	if ((va >= (void*)UTOP) || (ROUNDDOWN(va, PGSIZE) != va)) return -E_INVAL;
	page_remove(env->env_pgdir, va);
	return 0;
}
```
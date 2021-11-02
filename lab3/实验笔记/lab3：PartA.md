# Lab3:用户环境

作者说：从这个实验开始，我逐渐开始感觉到随着理论的补充和实验能力的提升，关键文件注释可以进行简化。因此，从此以后实验笔记的目录将按情况删减“关键文件注释”部分，取而代之是实验笔记中的更直接，简短的提出。

大家应该要多看课程，理论书籍，而不是一开始直接上实验：因为大部分实验都是理论的简化实现（逃）。

## 介绍

在本实验中，您将实现运行受保护的用户模式环境（即“进程”）所需的基本内核工具。您将增强 JOS 内核以设置数据结构以跟踪用户环境、创建单个用户环境、将程序映像加载到其中并启动它运行。您还将使 JOS 内核能够处理用户环境进行的任何系统调用并处理它引起的任何其他异常。

**注意：** 在本实验中，术语*用户环境*和*进程*可互换 - 两者均指允许您运行程序的抽象。我们引入术语“环境”而不是传统术语“进程”是为了强调 JOS 环境和 UNIX 进程提供不同的接口，并且不提供相同的语义。

### 入门

在您提交实验 2 后使用 Git 提交您的更改（如果有），获取课程存储库的最新版本，然后基于我们的 lab3 分支`origin/lab3`创建一个名为`lab3`的本地分支：``

```
%cd ~/6.828/lab
%add git
%git commit -am 'changes to lab2 after handin'
%git merge lab2
```

实验 3 包含许多新的源文件，您应该浏览这些文件：

| `inc/`  | `env.h`      | 用户模式环境的公共定义                                |
| ------- | ------------ | ----------------------------------------------------- |
|         | `trap.h`     | 陷阱处理的公共定义                                    |
|         | `syscall.h`  | 从用户环境到内核的系统调用的公共定义                  |
|         | `lib`        | 用户模式库的公共定义                                  |
| `kern/` | `env.h`      | 用户模式环境的内核私有定义                            |
|         | `env.c`      | 实现用户模式环境的内核代码                            |
|         | `trap.h`     | 内核私有陷阱处理定义                                  |
|         | `trap.c`     | 陷阱处理代码                                          |
|         | `Traentry.S` | 汇编语言陷阱处理程序入口点                            |
|         | `syscall.h`  | 系统调用处理的内核私有定义                            |
|         | `syscall.c`  | 系统调用实现代码                                      |
| `lib/`  | Makefrag     | 构建用户模式库的 Makefile 片段： `obj/lib/libjos.a`   |
|         | `entry.S`    | 用户环境的汇编语言入口点                              |
|         | `libmain`    | 从`entry.S 调用的`用户模式库设置代码                  |
|         | `syscall`    | 用户模式系统调用存根函数                              |
|         | `console`    | `putchar`和`getchar 的` 用户模式实现 ，提供控制台 I/O |
|         | `exit.c`     | `exit` 用户模式实现                                   |
|         | `panic.c`    | `panic` 用户模式实现                                  |
| `user/` | `*`          | 用于检查内核实验室 3 代码的各种测试程序               |

此外，我们为 lab2 分发的一些源文件在 lab3 中进行了修改。要查看差异，您可以键入：

```
$ git diff lab2
```

您可能还想再看看[实验室工具指南](https://pdos.csail.mit.edu/6.828/2017/labguide.html)，因为它包含有关调试与本实验室相关的用户代码的信息。

### 内联汇编

在本实验中，您可能会发现 GCC 的内联汇编语言功能很有用，尽管也可以不使用它来完成实验。至少，您需要能够理解我们提供给您的源代码中已经存在的内联汇编语言（“ `asm` ”语句）的片段。您可以在类[参考资料](https://pdos.csail.mit.edu/6.828/2017/reference.html)页面上找到有关 GCC 内联汇编语言的多个信息来源。

## A 部分：用户环境和异常处理

新的包含文件`inc/env.h` 包含 JOS 中用户环境的基本定义。内核使用`Env`数据结构来跟踪每个用户环境。在本实验中，您最初将只创建一个环境，但您需要设计 JOS 内核以支持多个进程；实验 4 将通过允许用户环境进入`fork`其他环境来完成。

正如您在`kern/env.c 中`看到的，内核维护着三个与env相关的主要全局变量：

```
struct Env *envs = NULL; // 所有进程
struct Env *curenv = NULL; // 当前进程
static struct Env *env_free_list; // 空闲进程表
```

一旦 JOS 启动并运行，`envs`指针就会指向`Env`代表系统中所有环境的结构数组。在我们的设计中，JOS 内核将支持最多`NENV` 同时处于活动状态的环境，尽管在任何给定时间运行的环境通常会少得多。（`NENV`为常数宏定义在`inc / env.h`。）一旦它被分配，该`envs`阵列会包含的单个实例`Env`的数据结构为每个的`NENV`可能的环境。

JOS 内核将所有非活动`Env`结构保存在`env_free_list`. 这种设计允许轻松分配和释放环境，因为它们只需添加到空闲表或从空闲表中删除。

内核在任何给定时间使用该`curenv`符号来跟踪*当前正在执行的*环境。在启动期间，在运行第一个环境之前， `curenv`最初设置为`NULL`.

因此我们知道，这个设置跟lab2的PageInfo数组和free_list是一样的，只是“空闲”的定义不同：本实验中为未占用CPU的意思

### 环境状态

该`Env`结构在`inc/env.h 中`定义如下（尽管在未来的实验室中将添加更多字段）：

```
struct{
	struct Trapframe env_tf; // 保存的寄存器
	struct Env *env_link; // 下一个自由环境
	env_t env_id; // 唯一的环境标识符
	env_t env_parent_id; // 此 env 的父级的 env_id
	enum EnvType env_type; // 表示特殊的系统环境
	unsigned env_status; // 环境状态
	uint32_t env_runs; // 环境运行的次数

	// 地址空间
	pde_t *env_pgdir; // 页目录的内核虚拟地址
```

以下是这些`Env`字段的用途：

- **env_tf**：

  这个在`inc/trap.h 中`定义的结构保存了该环境*未*运行时为该环境保存的寄存器值：即，当内核或其他环境正在运行时。当从用户模式切换到内核模式时，内核会保存这些信息，以便稍后可以从中断处恢复环境。

- **env_link**：

   `env_free_list`指向列表中的第一个空闲环境,然后每个空闲环境指向下一个空闲环境，从而构造空闲表，因此我们同样也知道curenv指向的环境此项为NULL。

- **env_id**：

  内核在此处存储一个值，该值唯一标识当前使用此`Env`结构的环境（即，使用`envs`数组中的此特定插槽）。在用户环境终止后，内核可能会将相同的`Env`结构重新分配给不同的环境 - 但是这种重复利用至少会改变env_id来区分环境。即unix系统里的pid

- **env_parent_id**：

  内核在此处存储`env_id` 创建此环境的环境的 。通过这种方式，环境可以形成一个“家谱”，这将有助于做出关于允许哪些环境对谁做什么的安全决策。这就构建了一个反向链表。

- **env_type**：

  这用于区分特殊环境。对于大多数环境，它将是`ENV_TYPE_USER`. 我们将在后面的实验中为特殊系统服务环境引入更多类型。

- **env_status**：

  此变量包含以下值之一：

  1.`ENV_FREE`：表示`Env`结构处于非活动状态，因此在`env_free_list`.`ENV_RUNNABLE`：表示该`Env`结构表示等待在处理器上运行的环境。**这就是“就绪态”**

  2.`ENV_RUNNING`：表示`Env`结构代表当前运行的环境。**这就是“运行态”**

  3.`ENV_NOT_RUNNABLE`：表示该`Env`结构表示当前处于活动状态的环境，但它当前尚未准备好运行：例如，因为它正在等待来自另一个环境的进程间通信 (IPC)。**这就是“阻塞态”**

  4.`ENV_DYING`：表示该`Env`结构代表僵尸环境。僵尸环境将在下一次陷入内核时被释放。在实验 4 之前我们不会使用这个标志。**这就是“僵尸态”**

- **env_pgdir** :

  这个变量保存了 这个环境页面目录的内核*虚拟地址*。**(注意不是kern_pgdir，后面会创建这个部分)**

与 Unix 进程一样，JOS 环境将“线程”和“地址空间”的概念结合在一起。线程主要由保存的寄存器（`env_tf`字段）定义，地址空间由 指向的页目录和页表定义 `env_pgdir`。运行环境，内核必须设置CPU与*两个*保存的寄存器和相应的地址空间。

我们`struct Env`的类似于xv6中`struct proc` 。两个结构都在一个结构中保存环境（即进程的）用户模式寄存器状态`Trapframe` 。在 JOS 中，单个环境没有像 xv6 中的进程那样拥有自己的内核堆栈。只能有一个JOS环境中进行活动在时间的内核，所以需要JOS只有一个 *单一的*内核堆栈。这点是和正常os不同的地方之一。

### 分配环境数组

在实验 2 中，我们在`mem_init()` 为`pages[]`数组分配了内存，该数组是内核用来跟踪哪些页是空闲的，哪些不是的表。您现在需要`mem_init()`进一步修改以分配一个类似的`Env`结构数组，称为`envs`.

### Exercise1

`mem_init()`在`kern/pmap.c 中` 修改 以分配和映射`envs`数组。这个数组完全由分配结构的`NENV`实例组成，`Env`就像你分配 `pages`数组的方式一样。与`pages`数组一样，内存支持 `envs`也应该映射到用户只读 `UENVS`（在`inc/memlayout.h 中`定义），以便用户进程可以从该数组中读取。

您应该运行您的代码并确保 `check_kern_pgdir()`成功。

### 创建和运行环境

您现在将在`kern/env.c 中`编写运行用户环境所需的代码。因为我们还没有文件系统，我们将设置内核以加载*嵌入内核本身*的静态二进制映像。JOS 将此二进制文件作为 ELF 可执行映像嵌入内核中。

Lab 3新增的文件： `GNUmakefile`在`obj/user/`目录中生成许多二进制图像。如果您查看 `kern/Makefrag`，您会注意到将这些二进制文件直接“链接”到内核可执行文件中的一些魔法，就好像它们是 `.o`文件一样。

`-b binary`命令选项对于链接器来说，使这些文件被当作“原始”未解释的二进制文件，而不是作为常规关联`的.o`由编译器产生的文件。（就链接器而言，这些文件根本不必是 ELF 图像——它们可以是任何东西，例如文本文件或图片！）如果您查看`obj/kern/kernel.sym`构建内核后，您会注意到链接器“神奇地”生成了许多有趣的符号，它们的名称晦涩难懂，例如 `_binary_obj_user_hello_start`、 `_binary_obj_user_hello_end`和 `_binary_obj_user_hello_size`。

链接器通过修改二进制文件的文件名来生成这些符号名；这些符号为常规内核代码提供了一种引用嵌入式二进制文件的方法。

在`i386_init()`在`kern/ init.c`你会在代码运行的环境中找到这些二进制映像之一。但是，设置用户环境的关键功能并不完整；你需要填写它们。

### Exercise2

在文件`env.c 中`，完成以下函数的编码：

- `env_init()`

  初始化数组`Env`中的所有结构`envs`并将它们添加到`env_free_list`. 还调用`env_init_percpu`，它使用单独的段为权限级别 0（内核）和权限级别 3（用户）配置分段硬件。

- `env_setup_vm()`

  为新环境分配页目录并初始化新环境地址空间的内核部分。

- `region_alloc()`

  为环境分配和映射物理内存

- `load_icode()`

  您将需要解析一个 ELF 二进制映像，就像引导加载程序已经做的那样，并将其内容加载到新环境的用户地址空间中。

- `env_create()`

  分配环境`env_alloc` 并调用`load_icode`将 ELF 二进制文件加载到其中。

- `env_run()`

  启动在用户模式下运行的给定环境。

在编写这些函数时，您可能会发现新的 cprintf 的`%e` 很有用——它会打印与错误代码对应的描述。例如，

```
	r = -E_NO_MEM;
	panic("env_alloc: %e", r);
```

会给出消息“env_alloc：out of memory”。

下面是调用用户代码之前的代码调用图。确保您了解每个步骤的目的。

- `start`( `kern/entry.S`)

- `i386_init`( `kern/init.c`)
  - `cons_init`
  
  - `mem_init`
  
  - `env_init`
  
  - `trap_init` （此时仍不完整）
  
  - `env_create`
  
  - `env_run`
    - `env_pop_tf`

完成后，您应该编译内核并在 QEMU 下运行它。如果一切顺利，您的系统应该进入用户空间并执行 `hello`二进制文件，直到它使用该`int`指令进行系统调用 。那时会有麻烦，因为 JOS 还没有设置硬件来允许从用户空间到内核的任何类型的转换。当CPU发现它没有设置处理这个系统调用中断时，就会产生一个通用保护异常，发现它不能处理一场，就会产生一个双故障异常，发现它也不能处理第二个异常，并最终放弃所谓的“三重故障”。通常，您会看到 CPU 复位和系统重新启动。虽然这对于遗留应用程序很重要（请参阅[ 此博客文章](http://blogs.msdn.com/larryosterman/archive/2005/02/08/369243.aspx)原因的解释），这对内核开发来说很痛苦，所以使用 6.828 修补的 QEMU，您将看到寄存器转储和“三重故障”。信息。

给出env_pop_tf，他的作用就是中断返回时通过env里的trapframe恢复现场：**因此当我们创建一个用户环境或者返回用户环境时，都会调用这个函数。**

```
void
env_pop_tf(struct Trapframe *tf)
{
	asm volatile(
		"\tmovl %0,%%esp\n"				
		"\tpopal\n"						//通用寄存器的pop
		"\tpopl %%es\n"					
		"\tpopl %%ds\n"					
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tiret\n"						//从Trapframe pop tf_eip,tf_cs,tf_eflags,tf_esp,tf_ss到寄存器
		: : "g" (tf) : "memory");
	panic("iret failed");  /* mostly to placate the compiler */
}
```

我们很快就会解决这个问题，但现在我们可以使用调试器来检查我们是否正在进入用户模式。在 处使用make qemu-gdb并设置 GDB 断点`env_pop_tf`，这应该是您在实际进入用户模式之前点击的最后一个函数。使用si;单步执行此功能 处理器应在`iret`指令后进入用户模式。然后，您应该看到在用户环境中的可执行文件，这是第一个指令`cmpl`在标签说明书`start` 中`的lib / entry.S中`。现在用于在`hello`中的inb *0x...处设置断点 （请参阅`obj/user/hello.asm`以获取用户空间地址）。这个`int $0x30``sys_cputs()``````int`是向控制台显示字符的系统调用。如果你不能执行到`int`，那么你的地址空间设置或程序加载代码有问题；在继续之前返回并修复它。

### 处理中断和异常

此时`int $0x30`，用户空间的第一条系统调用指令就进入了死胡同：由于没有设置中断，因此一旦处理器进入用户态，就没有办法到内核态了。您现在需要实现基本的异常和系统调用处理，以便内核可以从用户模式代码中恢复对处理器的控制。您应该做的第一件事是彻底熟悉 x86 中断和异常机制。

### Exercise3 

阅读 [第9章，异常和中断](https://pdos.csail.mit.edu/6.828/2017/readings/i386/c09.htm) 的 [80386程序员手册](https://pdos.csail.mit.edu/6.828/2017/readings/i386/toc.htm) （或第5章的[ IA-32开发者手册](https://pdos.csail.mit.edu/6.828/2017/readings/ia32/IA32-3A.pdf)），如果您还没有。

在本实验中，我们通常遵循 Intel 的中断、异常等术语。但是，异常、陷阱、中断、故障和中止等术语在架构或操作系统之间没有标准含义，并且经常在不考虑它们在特定架构（例如 x86）上的细微区别的情况下使用。当您在本实验室之外看到这些术语时，其含义可能略有不同。

### 受保护控制转移的基础知识

异常和中断都是“受保护的控制传输”，它们会导致处理器从用户模式切换到内核模式 (CPL=0)，而不会给用户模式代码任何机会干扰内核或其他环境的功能。在英特尔的术语中，*中断*是一种受保护的控制传输，它通常由处理器外部的异步事件引起，例如外部设备 I/O 活动的通知。一个*例外*，与此相反，是当前正在运行的代码同步地引起受保护的控制传输，例如由于通过零除法或一个无效的存储器访问。

为了确保这些受保护的控制传输实际上*受到保护*，处理器的中断/异常机制被设计成使得当前运行的代码在中断或异常发生时 *不能随意选择进入内核的位置或方式*。相反，处理器确保只有在仔细控制的条件下才能进入内核。在 x86 上，两种机制协同工作以提供这种保护：

1. **中断描述符表IDT。** 处理器确保中断和异常只能*在内核本身确定*的几个特定的、定义良好的入口点处进入 *内核*，而不是在发生中断或异常时运行的代码。

   x86 允许多达 256 个不同的中断或异常入口点进入内核，每个都有不同的*中断向量*。向量是 0 到 255 之间的数字。中断向量由中断源决定：不同的设备、错误条件和对内核的应用程序请求会生成具有不同向量的中断。CPU 使用向量作为处理器*中断描述符表*(IDT)的索引，内核在内核私有内存中设置该*表*，很像 GDT。处理器从该表中的相应条目加载：

   - 要加载到指令指针 ( `EIP` ) 寄存器中的值，指向指定用于处理该类型异常的内核代码。
   - 要加载到代码段 ( `CS` ) 寄存器中的值，该值在位 0-1 中包含异常处理程序运行的特权级别。（在 JOS 中，所有异常都在内核模式下处理，特权级别为 0。）

2. **任务状态段TSS。** 处理器需要一个地方来保存中断或异常发生之前的*旧*处理器状态，例如 处理器调用异常处理程序之前的`EIP`和`CS`的原始值，以便异常处理程序稍后可以恢复旧状态并恢复中断代码从它停止的地方开始。但是这个旧处理器状态的保存区域必须反过来保护不受非特权用户模式代码的影响；否则错误或恶意的用户代码可能会危及内核。

   出于这个原因，当 x86 处理器接受导致特权级别从用户模式更改为内核模式的中断或陷阱时，它也会切换到内核内存中的堆栈。称为*任务状态段*(TSS) 的结构指定了此堆栈所在的段选择器和地址。处理器推送（在这个新堆栈上） `SS`、`ESP`、`EFLAGS`、`CS`、`EIP`和一个可选的错误代码。然后它从中断描述符中加载`CS`和`EIP`，并设置`ESP`和`SS`以引用新堆栈。

   尽管 TSS 很大并且可能有多种用途，但 JOS 仅使用它来定义处理器从用户模式转换到内核模式时应切换到的内核堆栈。由于“内核模式”是JOS特权级别0在x86，处理器使用`ESP0`和`SS0`的TSS的字段中输入内核模式时，以限定内核栈。JOS 不使用任何其他 TSS 字段。

### 异常和中断的类型

x86 处理器可以在内部生成的所有同步异常都使用 0 到 31 之间的中断向量，因此映射到 IDT 条目 0-31。例如，页面错误总是通过向量 14 引起异常。大于 31 的中断向量仅用于 *软件中断*，它可以由`int`指令产生，或者异步*硬件中断*，由外部设备在需要注意时引起。

在本节中，我们将扩展 JOS 以处理向量 0-31 中内部生成的 x86 异常。在下一节中，我们将使 JOS 处理软件中断向量 48 (0x30)，JOS（相当随意）将其用作其系统调用中断向量。在实验 4 中，我们将扩展 JOS 以处理外部生成的硬件中断，例如时钟中断。

### 一个例子

让我们将这些部分放在一起并通过一个示例进行跟踪。假设处理器正在用户环境中执行代码并遇到试图除以零的除法指令。

1. 所述处理器切换到由定义的堆栈 `SS0`和`ESP0`的TSS的字段，它们在书将保存的值 `GD_KD`和`KSTACKTOP`分别。

2. 处理器将异常参数推送到内核堆栈上，从地址开始

   ```
   KSTACKTOP
   ```

   ：

   ```
                        +--------------------+ KSTACKTOP             
                        | 0x00000 | 旧SS | ” - 4
                        | 旧ESP | ” - 8
                        | 旧 EFLAGS | ” - 12
                        | 0x00000 | 旧CS | “ - 16
                        | 旧EIP | " - 20 <---- ESP
                        +--------------------+             
   	
   ```

3. 因为我们正在处理除法错误，即 x86 上的中断向量 0，处理器读取 IDT 条目 0 并将 `CS:EIP 设置`为指向条目描述的处理程序函数。

4. 处理程序函数取得控制权并处理异常，例如通过终止用户环境。

对于某些类型的 x86 异常，除了上面的五个word外，处理器还会将另一个包含*错误代码的word压入堆栈。页错误异常，编号 14，是一个重要的例子。请参阅 80386 手册以确定处理器推送错误代码的异常编号，以及错误代码在这种情况下的含义。当处理器推送错误代码时，当从用户模式进入时，堆栈将在异常处理程序的开头如下所示：

```
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | 老SS | ” - 4
                     | 旧ESP | ” - 8
                     | 旧 EFLAGS | ” - 12
                     | 0x00000 | 老CS | “ - 16
                     | 旧EIP | “ - 20
                     | 错误代码 | " - 24 <---- ESP
                     +--------------------+             
	
```

### 嵌套异常和中断

处理器可以从内核和用户模式接受异常和中断。然而，只有当从用户模式进入内核时，x86 处理器才会在将其旧寄存器状态推送到堆栈并通过 IDT 调用适当的异常处理程序之前自动切换堆栈。如果在中断或异常发生时处理器*已经*处于内核模式（`CS`寄存器的低 2 位已经为零），那么 CPU 只会在同一内核堆栈上压入更多值。通过这种方式，内核可以优雅地处*理由* 内核本身内的代码引起的*嵌套异常*。此功能是实施保护的重要工具，我们将在稍后有关系统调用的部分中看到。

如果处理器已经处于内核模式并出现嵌套异常，由于不需要切换堆栈，它不会保存旧的`SS`或`ESP`寄存器。对于不推送错误代码的异常类型，内核堆栈因此在异常处理程序入口处如下所示：

```
                     +--------------------+ <---- 旧 ESP
                     | 旧 EFLAGS | ” - 4
                     | 0x00000 | 旧CS | ” - 8
                     | 旧EIP | ” - 12
                     +--------------------+             
```

对于推送错误代码的异常类型，处理器会像以前一样在旧的`EIP`之后立即推送错误代码。

处理器的嵌套异常功能有一个可能的问题。如果处理器在已经处于内核模式时发生异常，并且由于任何原因（例如堆栈空间不足）而*无法将其旧状态推送到内核堆栈上*，那么处理器无法进行任何恢复，因此它只会重置自己。不用说，内核的设计应该避免这种情况发生。

### 设置 IDT

您现在应该拥有在 JOS 中设置 IDT 和处理异常所需的基本信息。现在，您将设置 IDT 来处理中断向量 0-31（处理器异常）。我们将在本实验的稍后部分处理系统调用中断，并在稍后的实验中添加中断 32-47（设备 IRQ）。

头文件`inc/trap.h`和`kern/trap.h` 包含与您需要熟悉的中断和异常相关的重要定义。文件`kern/trap.h`包含对内核严格私有的定义，而`inc/trap.h` 包含对用户级程序和库也可能有用的定义。

注意：0-31 范围内的一些异常由 Intel 定义为保留。由于它们永远不会由处理器生成，因此您如何处理它们并不重要。

您应该实现的总体控制流程如下所示：

```
      IDT trapentry.S trap.c
   
+----------------+                        
| &handler1 |---------> handler1: trap (struct Trapframe *tf)
| | // 做点什么 {
| | call trap // 处理异常/中断
| | // ... }
+----------------+
| &handler2 |--------> handler2：
| | // 做点什么
| | call陷阱
| | // ...
+----------------+
       .
       .
       .
+----------------+
| &handlerX |--------> handlerX：
| | // 做东西
| | 呼叫陷阱
| | // ...
+----------------+
```

每个异常或中断都应该在`trapentry.S 中`有自己的处理程序， 并且`trap_init()`应该用这些处理程序的地址初始化 IDT。每个处理程序都应该在堆栈上构建一个`struct Trapframe` （参见`inc/trap.h`）并使用指向 Trapframe 的指针调用 `trap()`（在`trap.c 中`）。 `trap()`然后处理异常/中断或分派到特定的处理程序函数。

### Exercise4 

编辑`trapentry.S`和`trap.c`并实现上述功能。宏 `TRAPHANDLER`和`TRAPHANDLER_NOEC`在 `trapentry.S`应该帮助你，还有T_ *定义`INC / trap.h`。您需要在添加一个条目点`trapentry.S`中定义的每个陷阱（使用这些宏）`INC / trap.h`，你就必须提供`_alltraps`该 `TRAPHANDLER`宏参考。您还需要修改`trap_init()`以初始化 `idt` 指向`trapentry.S 中`定义的每个入口点；该`SETGATE` 宏将有助于在这里。

`_alltraps`应该做的事：

1. 保存现场：压入值使堆栈使看起来像一个结构体 Trapframe
2. 加载`GD_KD`到`%ds`和`%es`
3. `pushl %esp` 将指向 Trapframe 的指针作为参数传递给 trap()
4. `call trap`（能返回吗？）

考虑使用`pushal` 指令；它非常适合`struct Trapframe`.

在进行任何系统调用之前 ，使用`用户`目录中的一些导致异常的测试程序来测试您的陷阱处理代码，例如`user/divzero`。此时您应该能够make grade 在`divzero`、`softint`和`badsegment`测试中取得成功。

*挑战！* 您现在可能有很多非常相似的代码，`TRAPHANDLER`在 `trapentry.S 中`的列表和它们在`trap.c 中`的安装 `之间`。清理这个。更改`trapentry.S 中`的宏 以自动生成一个表供 `trap.c`使用。请注意，您可以使用指令`.text`和`.data`.

**问题**

在你的`answers-lab3.txt 中`回答以下问题：

1. 为每个异常/中断设置单独的处理函数的目的是什么？（即，如果所有异常/中断都传递给同一个处理程序，则无法提供当前实现中存在的哪些功能？）
2. 您是否必须采取任何措施才能使`user/softint`程序正确运行？等级脚本预计它会产生一般保护错误（陷阱 13），但`softint`的代码会使用中断 `int $14`。 *为什么*这会产生中断向量 13？如果内核实际上允许`softint`的 `int $14`指令调用内核的页面错误处理程序（即中断向量 14）会发生什么？

## 回答问题汇总

在你写exercise代码前，给你这些部分函数的调用关系，希望对你有帮助

```
env_init()
env_create()
	env_alloc()
		env_setup_vm()
	load_icode()
		region_alloc()
env_run()
	pop_tf()
```

### Exercise1

我们就像分配pages数组那样分配envs数组，做三件事

1.虚拟alloc，初始化0

2.页面分配，建立映射

3.权限设置

mem_init():分配用户环境表，建立映射

```
1.
	envs = (struct Env*)boot_alloc(sizeof(struct Env) * NENV);
	memset(envs, 0, sizeof(struct Env) * NENV);
2/3.
	boot_map_region(kern_pgdir, UENVS, PTSIZE, PADDR(envs), PTE_U);
```

### Exercise2

env_init():就像我们前面构建free_list一样，保证每一次从env_free_list取出的都是最小的

```
void
env_init(void)
{
	// Set up envs array
	// LAB 3: Your code here.
	env_free_list = NULL;
	for (int i = NENV - 1; i >= 0; i--) {	//如果你的直接让env_link指向i+1，最后env_free_list指向i=0，部分也行，不过实际情况并不是顺序分配，所以我们使用前插法
		envs[i].env_id = 0;
		envs[i].env_link = env_free_list;
		env_free_list = &envs[i];
	}

	// Per-CPU part of the initialization
	env_init_percpu();    //加载GDT
}
```

Env_setup_vm():给定struct Env *e指针，初始化用户页目录

做三件事

1.分配页，初始化页

2.将e->env_pgdir指向页，并将此页当作页目录初始化（模仿lab2的内核页目录）

3.权限设置

```
static int
env_setup_vm(struct Env *e)
{
	int i;
	struct PageInfo *p = NULL;

	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO)))  //分配一页，page alloc只会从空闲表中找一页，修改link部分为NULL，因此我们需要增加ref
		return -E_NO_MEM;

	// LAB 3: Your code here.
	p->pp_ref++;
	e->env_pgdir = (pde_t *) page2kva(p);  //页目录指向
	memcpy(e->env_pgdir, kern_pgdir, PGSIZE); //模仿内核页目录
	
	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;  //模仿，修改页目录项为用户的
	
	return 0;
}
```

region_alloc():为每个用户，通过页目录e->env_pgdir和page_insert，为[va, va+len)分配物理内存,建立多页映射

做三件事

1.确定分配页的范围：va如果在页中就应该找到页基址，va+len反而应该在下一页的基址

2.page_alloc循环分配物理页

3..page_insert建立映射

```
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	void *start = ROUNDDOWN(va, PGSIZE), 
	void *end = ROUNDUP(va+len, PGSIZE);
	while (start < end) {//开区间所以不等
		struct PageInfo *p = page_alloc(0); //分配一个物理页
		if (!p) {
			panic("fault: region_alloc: page_alloc failed\n");
		}
		if(page_insert(e->env_pgdir, p, start, PTE_W | PTE_U) != 0){//建立映射,成功返回0
			panic("fault: region_alloc: page_insert failed\n");
		};   
	}
}
```

load_icode():当内核初始化时，前面所说用户文件二进制插入：解析binary地址开始处的ELF文件，紧接着我们才能进入用户环境

做三件事

1.得到ELF header和其他有用的指针

2.模仿boot/main.c,按照ELF头设置（ph->p_type == ELF_PROG_LOAD）加载ELF段到用户虚拟内存（ph->p_va，大小ph->p_memsz），初始化bss为0（把binary + ph->p_offset，大小ph->p_filesz复制到ph->p_va，剩余部分置为0）

3.设置eip为entry，为用户的main栈分配内存，建立映射

```
static void
load_icode(struct Env *e, uint8_t *binary)
{
	// LAB 3: Your code here.
	struct Elf *ELF = (struct Elf *) binary;
	struct Proghdr *ph;				//ELF header
	int ph_cnt;						//load counter
	if (ELF->e_magic != ELF_MAGIC) {
		panic("fault: The binary is not ELF format\n");
	}
	if(ELF->e_entry == 0){
     panic("fault: The ELF file can't be executed.\n");
  }
  
	ph = (struct Proghdr *) ((uint8_t *) ELF + ELF->e_phoff);//得到ELF header指针
	ph_cnt = ELF->e_phnum;//得到load counter

	lcr3(PADDR(e->env_pgdir));			//设置cr3为用户页目录

	for (int i = 0; i < ph_cnt; i++) {
		if (ph[i].p_type == ELF_PROG_LOAD) {		//只加载LOAD类型的Segment
			region_alloc(e, (void *)ph[i].p_va, ph[i].p_memsz);//用户态建立映射
			memset((void *)ph[i].p_va, 0, ph[i].p_memsz);	//分配为0
			memcpy((void *)ph[i].p_va, binary + ph[i].p_offset, ph[i].p_filesz); //不为0的部分
		}
	}


	e->env_tf.tf_eip = ELF->e_entry;
	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.
	// LAB 3: Your code here.
	region_alloc(e, (void *) (USTACKTOP - PGSIZE), PGSIZE);//建立main stack映射
}
```

env_create()：

1.申请一个env，设置用户type

2.加载ELF：使用load_icode

```
void
env_create(uint8_t *binary, enum EnvType type)
{
    // LAB 3: Your code here.
    struct Env *e;
    int rc;
    if((rc = env_alloc(&e, 0)) != 0) {
        panic("env_create failed: env_alloc failed.\n");
    }

    load_icode(e, binary);
    e->env_type = type;
}
```

env_run(struct Env *e)：开始运行用户环境（按照jos注释要求写即可）

```
void
env_run(struct Env *e)
{

    if(curenv != NULL && curenv->env_status == ENV_RUNNING) {
        curenv->env_status = ENV_RUNNABLE;
    }

    curenv = e;
    curenv->env_status = ENV_RUNNING;
    curenv->env_runs++;
    lcr3(PADDR(curenv->env_pgdir));//页目录切换
    env_pop_tf(&curenv->env_tf);
}
```



### Exercise4

先给出trapframe的结构定义

```
struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	/* below here defined by x86 hardware */
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	/* below here only when crossing rings, such as from user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
}
```

1.IDT设置：定义每一种中断向量，为trapentry.S的handler预处理提供向量定义

这部分代码参考了网络上的，主要是一开始global name不知道是啥。

其中，0-16号中断号设置为异常部分，比如除0错误。页面错误，保护错误，tss错误等。

[0-16异常](https://pdos.csail.mit.edu/6.828/2017/readings/i386/s09_08.htm)这部分先看完，决定下面的设置，以及错误码是否需要的预处理。

trap.c/trap_init()（包含了partB的代码）

```
void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	void th0();
	void th1();
	void th3();
	void th4();
	void th5();
	void th6();
	void th7();
	void th8();
	void th9();
	void th10();
	void th11();
	void th12();
	void th13();
	void th14();
	void th16();
	void th_syscall();
	SETGATE(idt[0], 0, GD_KT, th0, 0);		//定义在inc/mmu.h中
	SETGATE(idt[1], 0, GD_KT, th1, 0);  
	SETGATE(idt[3], 0, GD_KT, th3, 3);    //DPL必须是3
	SETGATE(idt[4], 0, GD_KT, th4, 0);
	SETGATE(idt[5], 0, GD_KT, th5, 0);
	SETGATE(idt[6], 0, GD_KT, th6, 0);
	SETGATE(idt[7], 0, GD_KT, th7, 0);
	SETGATE(idt[8], 0, GD_KT, th8, 0);
	SETGATE(idt[9], 0, GD_KT, th9, 0);
	SETGATE(idt[10], 0, GD_KT, th10, 0);
	SETGATE(idt[11], 0, GD_KT, th11, 0);
	SETGATE(idt[12], 0, GD_KT, th12, 0);
	SETGATE(idt[13], 0, GD_KT, th13, 0);
	SETGATE(idt[14], 0, GD_KT, th14, 0);
	SETGATE(idt[16], 0, GD_KT, th16, 0);

	SETGATE(idt[T_SYSCALL], 0, GD_KT, th_syscall, 3);		//DPL定义为3，因为中断判断DPL只能低特权级调用高特权级，partB部分

	// Per-CPU setup 
	trap_init_percpu();
}
```

trapentry.S：插入内联汇编(包含了partB的代码)

2.使用宏来预处理每种中断向量，预处理后都会跳转到alltraps进行处理，最后跳转到trap函数

其中，._alltraps应该做的事：

1. 保存现场：压入值使堆栈使看起来像一个结构体 Trapframe
2. 加载`GD_KD`到`%ds`和`%es`
3. `pushl %esp` 将指向 Trapframe 的指针作为参数传递给 trap()
4. `call trap`（想想能返回吗？）

```
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
	TRAPHANDLER_NOEC(th0, 0)
	TRAPHANDLER_NOEC(th1, 1)
	TRAPHANDLER_NOEC(th3, 3)
	TRAPHANDLER_NOEC(th4, 4)
	TRAPHANDLER_NOEC(th5, 5)
	TRAPHANDLER_NOEC(th6, 6)
	TRAPHANDLER_NOEC(th7, 7)
	TRAPHANDLER(th8, 8)
	TRAPHANDLER_NOEC(th9, 9)
	TRAPHANDLER(th10, 10)
	TRAPHANDLER(th11, 11)
	TRAPHANDLER(th12, 12)
	TRAPHANDLER(th13, 13)
	TRAPHANDLER(th14, 14)
	TRAPHANDLER_NOEC(th16, 16)
	
	TRAPHANDLER_NOEC(th_syscall, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
	_alltraps:
	pushl %ds
	pushl %es
	pushal
	pushl $GD_KD
	popl %ds
	pushl $GD_KD
	popl %es
	pushl %esp	
	call trap       
```

Happy!

![截屏2021-11-01 下午8.40.31](/Users/khalilchen/Library/Application Support/typora-user-images/截屏2021-11-01 下午8.40.31.png)

1. 为每个异常/中断设置单独的处理函数的目的是什么？（即，如果所有异常/中断都传递给同一个处理程序，则无法提供当前实现中存在的哪些功能？）

首先内核的嵌套中断不需要推送错误号，因为能直接处理，其次内核中断为了避免内存不够问题，必须直接panic。最后，并且用户态中断处理返回，内核则不必返回。

2. 您是否必须采取任何措施才能使`user/softint`程序正确运行？等级脚本预计它会产生一般保护错误（trap 13），但`softint`的代码会使用中断 `int $14`。 *为什么*这会产生中断向量 13？如果内核实际上允许`softint`的 `int $14`指令调用内核的页面错误处理程序（即中断向量 14）会发生什么？

14是缺页中断，这是用户态到内核的唯一方法。用户态是不允许直接调用内核函数。会产生保护错误trap 13:特权级保护错误。


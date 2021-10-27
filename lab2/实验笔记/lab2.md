# 实验 2：内存管理

## 介绍

在本实验中，您将为您的操作系统编写内存管理代码。内存管理有两个组成部分。

第一个组件是内核的物理内存分配器，以便内核可以分配内存并稍后释放它。您的分配器将以 4096 字节为单位运行，称为 *pages*。您的任务是维护数据结构，记录哪些物理页面是空闲的，哪些已分配，以及有多少进程正在共享每个分配的页面。您还将编写分配和释放内存页的例程。

内存管理的第二个组件是*虚拟内存*，它将内核和用户软件使用的虚拟地址映射到物理内存中的地址。x86 硬件的内存管理单元 (MMU) 在指令使用内存时执行映射，查询一组页表。您将根据我们提供的规范修改 JOS 以设置 MMU 的页表。

### 入门

在这个和以后的实验中，您将逐步构建您的内核。我们还将为您提供一些额外的来源。要获取该源，请使用 Git 提交自上交实验室 1 以来所做的更改（如果有），获取课程存储库的最新版本，然后基于我们的 lab2 分支`origin/lab2`创建一个名为`lab2`的本地分支：``

```
cd 你存放的lab路径下
%git checkout -b lab2 origin/lab2
```

git checkout -b上面显示 的命令实际上做了两件事：首先创建一个基于课程人员提供的`origin/lab2`分支的本地分支`lab2`，其次，它更改您的`lab` 目录的内容以反映存储在`lab2`上的`文件`分支。Git 允许使用 切换现有分支，但您应该在切换到另一个分支之前在一个分支上提交任何未完成的更改。 

```
%git checkout branch-name//切换格式
```

您现在需要将您在`lab1` 分支中所做的更改合并到`lab2`分支中，如下所示：

```
%git merge lab1
```

在某些情况下，Git 可能无法弄清楚如何将您的更改与新的实验室作业合并（例如，如果您修改了在第二个实验室作业中更改的某些代码）。在这种情况下，该git merge命令会告诉您哪些文件有*冲突*，您应该首先解决冲突（通过编辑相关文件），然后使用git commit -a.

实验 2 包含以下新源文件，您应该浏览这些文件：（看关键文件代码注释）

- `inc/memlayout.h`
- `kern/pmap.c`
- `kern/pmap.h`
- `kern/kclock.h`
- `kern/kclock.c`

`memlayout.h`描述了必须通过修改`pmap.c`来实现的虚拟地址空间的布局。 `memlayout.h`和`pmap.h`定义了`PageInfo` 用于跟踪哪些物理内存页面空闲的结构。 `kclock.c`和`kclock.h` 操作 PC 的电池供电时钟和 CMOS RAM 硬件，其中 BIOS 记录 PC 包含的物理内存量等。`pmap.c 中`的代码需要读取这个设备硬件，以便计算出有多少物理内存，但这部分代码是为你完成的：你不需要知道 CMOS 硬件如何工作的细节。

请特别注意`memlayout.h`和`pmap.h`，因为本实验要求您使用并理解它们包含的许多定义。您可能还想查看`inc/mmu.h`，因为它还包含许多对本实验有用的定义。

### 实验室要求

在本实验和后续实验中，完成实验中描述的所有常规练习和*至少一个*挑战题。（当然，有些挑战问题比其他问题更具挑战性！）此外，写下对实验室中提出的问题的简短回答，并简短（例如，一两段）描述您为解决所选挑战问题所做的工作。如果你实现了多个挑战问题，你只需要在文章中描述其中一个，当然欢迎你做更多。

### 测试程序

```
%./grade-lab2
```

会给你一个评分和错误显示。

**首先**你必须明白一个道理：当你写代码的时候，代码里的给定**地址操作**中地址都会被mmu进行转换。因此即便是内核代码：假如你编写页表时候，你根据基址和偏移取页表项这个动作，也应该给出虚拟基址而不是物理地址，否则你直接变量名。为什么用变量名可以呢？前面的lab中已经知道了ELF规定里内核的代码也按照虚拟内存布局来存放。因此当你创建一个数组a的时候，数组自然存放在物理内存中，但是如果你把a当作地址，那么很不幸你只能得到虚拟地址，除非你做更多的转换动作。

那么内核的地址到底怎么在页表创立之前进行高低转换呢？前面的lab给出了答案：位移

如果没有明白以上的内容，你将完全看不懂本实验。

## 第 1 部分：物理页面管理

操作系统必须跟踪物理 RAM 的哪些部分是空闲的，哪些是当前正在使用的。JOS 以*页面粒度*管理 PC 的物理内存， 以便它可以使用 MMU 来映射和保护每块分配的内存。

您现在将编写物理页面分配器。它通过一个`struct PageInfo`对象链接列表来跟踪哪些页面是空闲的（与 xv6 不同的是，它们*没有*嵌入到空闲页面本身中），每个页面对应一个物理页面。您需要先编写物理页分配器，然后才能编写其余的虚拟内存实现，因为页表管理代码需要分配物理内存来存储页表。

### **Exercise1**

在文件`kern/pmap.c 中`，您必须为以下函数实现代码（可以按照给定的顺序）。

```
boot_alloc()
mem_init()（仅限于调用`check_page_free_list(1)`）
page_init()
page_alloc()
page_free()
```

`check_page_free_list()`并 `check_page_alloc()`测试您的物理页面分配器。您应该启动 JOS 并查看是否`check_page_alloc()` 报告成功。修复您的代码，使其通过。您可能会发现添加您自己的`assert()` 以验证您的假设是否正确很有帮助。

该实验室以及所有 6.828 实验室将要求您进行一些侦查工作，以确定您需要做什么。此作业并未描述您必须添加到 JOS 的代码的所有细节。在您必须修改的 JOS 源代码部分中查找注释；这些注释通常包含规范和提示。您还需要查看 JOS 的相关部分、Intel 手册以及 6.004 或 6.033 注释。

## 第 2 部分：虚拟内存

在做任何其他事情之前，先熟悉 x86 的保护模式内存管理架构：即*分段*和*页面转换*。

### **Exercise2** 

查看[ Intel 80386 参考手册的](https://pdos.csail.mit.edu/6.828/2017/readings/i386/toc.htm)第 5 章和第 6 章 ，如果您还没有这样做的话。仔细阅读有关页面转换和基于页面的保护的部分（5.2 和 6.4）。我们建议您还浏览有关细分的部分；虽然 JOS 使用分页硬件进行虚拟内存和保护，但不能在 x86 上禁用段转换和基于段的保护，因此您需要对其有基本的了解。

### 虚拟、线性和物理地址

本文对于段转换进行了一个放弃。但是你也应该知道段的转换。

```
                 Selector  +--------------+         +-----------+
          ---------->|              |         |           |
                     | Segmentation |         |  Paging   |
Software             |              |-------->|           |---------->  RAM
            Offset   |  Mechanism   |         | Mechanism |
          ---------->|              |         |           |
                     +--------------+         +-----------+
            Virtual                   Linear                Physical

```

我们通常把指针是当作虚拟地址的“偏移量”。在`boot/boot.S 中`，我们安装了一个全局描述符表 (GDT)，它通过将所有段基地址设置为 0 并将限制设置为 来有效地禁用段转换`0xffffffff`。因此“选择子”不起作用，线性地址总是等于虚拟地址的偏移量。在实验 3 中，我们将不得不与分段进行更多交互以设置权限级别，但是对于地址映射，我们可以在整个 JOS 实验中忽略分段，而只关注页表映射。

回想一下，在实验 1 的第 3 部分中，我们安装了一个简单的页表，以便内核可以在其链接地址 0xf0100000 处运行，即使它实际上加载到 ROM BIOS 上方 0x00100000 处的物理内存中。这个页表只映射了 4MB 的内存。在本实验中您要为 JOS 设置的虚拟地址空间布局中，我们将扩展它以映射从虚拟地址 0xf0000000 开始的前 256MB 物理内存，并映射虚拟地址空间的许多其他区域。

### **Exercise3** 

虽然 GDB 只能通过虚拟地址访问 QEMU 的内存，但在设置虚拟内存时能够检查物理内存通常很有用。查看实验室工具指南中的 QEMU[监视器命令](https://pdos.csail.mit.edu/6.828/2017/labguide.html#qemu)，尤其是 `xp`命令，它可以让您检查物理内存。要访问 QEMU 监视器，请Ctrl-a c在终端中按下（相同的绑定返回到串行控制台）。

使用xpQEMU 监视器中的x命令和 GDB 中的 命令检查相应物理和虚拟地址处的内存，并确保您看到相同的数据。

我们的 QEMU 补丁版本提供了一个info pg 可能也很有用的命令：它显示了当前页表的紧凑但详细的表示，包括所有映射的内存范围、权限和标志。Stock QEMU 还提供了一个info mem命令，该命令显示映射了哪些虚拟地址范围以及具有哪些权限的概述。



从在 CPU 上执行的代码来看，一旦我们处于保护模式（我们首先在`boot/boot.S 中输入`），就无法直接使用线性或物理地址。 *所有*内存引用都被解释为虚拟地址并由 MMU 翻译，这意味着 C 中的所有指针都是虚拟地址。

JOS 内核通常需要将地址作为不透明值或整数进行操作，而不是取消引用它们，例如在物理内存分配器中。有时这些是虚拟地址，有时它们是物理地址。为了帮助记录代码，JOS 源区分了两种情况：类型`uintptr_t`表示不透明的虚拟地址，和`physaddr_t`表示物理地址。这两种类型实际上只是 32 位整数 ( `uint32_t`) 的同义词，因此编译器不会阻止您将一种类型分配给另一种类型！由于它们是整数类型（不是指针），如果您尝试取消引用它们，编译器*会*抱怨。

JOS 内核可以`uintptr_t`通过首先将其转换为指针类型来取消引用 a 。相比之下，内核无法合理地取消引用物理地址，因为 MMU 会转换所有内存引用。如果您将 a`physaddr_t`转换为指针并取消引用它，您可能能够加载并存储到结果地址（硬件会将其解释为虚拟地址），但您可能无法获得您想要的内存位置。

总结一下：

| 类型声明     | 地址类型 |
| ------------ | -------- |
| `T*`         | 虚拟     |
| `uintptr_t`  | 虚拟     |
| `physaddr_t` | 物理     |



**问题**

1. 假设下面的 JOS 内核代码是正确的，变量x应该是什么类型： unitptr_t,还是physaddr_？

   ```
   	mystery_t x;
   	char* value = return_a_pointer();
   	*value= 10；
   	x = (mysetyry_t)value；
   ```

JOS 内核有时需要读取或修改，它只知道物理地址的内存。例如，向页表添加映射可能需要分配物理内存来存储页目录，然后初始化该内存。但是，内核无法绕过虚拟地址转换，因此无法直接加载和存储到物理地址。JOS 重新映射从物理地址 0 开始到虚拟地址 0xf0000000 的所有物理内存的原因之一是帮助内核读取和写入它只知道物理地址的内存。为了将一个物理地址转换成内核可以实际读写的虚拟地址，内核必须在物理地址上加上0xf0000000，才能在重映射的区域中找到其对应的虚拟地址。你应该使用`KADDR(pa)` 做那个加法。

给定存储内核数据结构的内存的虚拟地址，JOS 内核有时也需要能够找到物理地址。内核全局变量和分配的内存 `boot_alloc()`位于加载内核的区域，从 0xf0000000 开始，也就是我们映射所有物理内存的区域。因此，要将这个区域中的虚拟地址转换为物理地址，内核可以简单地减去 0xf0000000。你应该`PADDR(va)` 用来做那个减法。

### 引用计数

在未来的实验中，您通常会将相同的物理页面同时映射到多个虚拟地址（或多个环境的地址空间中）。您将在物理页面对应的`pp_ref`字段中记录对每个物理页面的引用次数`struct PageInfo`。当一个物理页面的这个计数变为零时，该页面可以被释放，因为它不再被使用。一般来说，这个计数应该等于物理页在所有页表中出现*在下面 `UTOP`*的次数（上面的映射 `UTOP`大部分在启动时由内核设置，永远不应该被释放，所以没有必要对它们进行引用计数）。我们还将使用它来跟踪我们保留的指向页表的指针的数量，进而跟踪页目录对页表页的引用数量。

使用`page_alloc`时要小心。它返回的页面将**始终具有 0 的引用计数**，因此，只要您对返回的页面进行了某些操作（例如将其插入到页表中），就应该增加`pp_ref`。有时这由其他函数处理（例如，`page_insert`），有时调用`page_alloc`的函数必须直接执行此操作。

### 页表管理

现在您将编写一组例程来管理页表：插入和删除线性到物理映射，以及在需要时创建页表页。

### **Exercise4** 

在文件`kern/pmap.c 中`，您必须实现以下函数的代码。

```
        pgdir_walk()
        boot_map_region()
        page_lookup()
        page_remove()
        page_insert()
```

`check_page()`调用  `mem_init()`，测试您的页表管理。在继续之前，您应该确保它报告成功。

## 第 3 部分：内核地址空间

JOS 将处理器的 32 位线性地址空间分为两部分。我们将在实验 3 中开始加载和运行的用户环境（进程）将控制下部的布局和内容，而内核始终保持对上部的完全控制。分割线由符号定义ULIM`在`INC / memlayout.h`，保留约256MB的虚拟地址空间是内核。这就解释了为什么我们需要在lab 1 中给内核一个如此高的链接地址：否则内核的虚拟地址空间将没有足够的空间同时映射到它下面的用户环境。

您会发现参考`inc/memlayout.h 中`的 JOS 内存布局图 对本部分和后续实验`很有帮助`。

### 权限和故障隔离

由于内核和用户内存都存在于每个环境的地址空间中，我们将不得不在 x86 页表中使用权限位来允许用户代码仅访问地址空间的用户部分。否则用户代码中的错误可能会覆盖内核数据，导致崩溃或更微妙的故障；用户代码也可能窃取其他环境的私人数据。请注意，可写权限位 ( `PTE_W` ) 会影响用户和内核代码！

用户环境对上面的任何内存都没有权限`ULIM`，而内核将能够读写这块内存。对于地址范围 `[UTOP,ULIM)`，内核和用户环境都拥有相同的权限：可以读但不能写这个地址范围。该地址范围用于向用户环境公开某些只读的内核数据结构。最后，下面的地址空间 `UTOP`是供用户环境使用的；用户环境将设置访问此内存的权限。

### 初始化内核地址空间

现在您将设置上面的地址空间`UTOP`：地址空间的内核部分。 `inc/memlayout.h`显示您应该使用的布局。您将使用刚刚编写的函数来设置适当的线性到物理映射。

### **Exercise5** 

在`mem_init()`调用 之后填写缺少的代码`check_page()`。

您的代码现在应该通过`check_kern_pgdir()` 和`check_page_installed_pgdir()`检查。

**问题**

1. 页面目录中的哪些条目（行）此时已被填充？它们映射哪些地址以及指向何处？换句话说，尽可能多地填写这张表：

   | 入口 | 基本虚拟地址 | 指向（逻辑上）：      |
   | ---- | ------------ | --------------------- |
   | 1023 | ?            | 前 4MB 物理内存的页表 |
   | 1022 | ?            | ?                     |
   | .    | ?            | ?                     |
   | .    | ?            | ?                     |
   | .    | ?            | ?                     |
   | 2    | 0x00800000   | ?                     |
   | 1    | 0x00400000   | ?                     |
   | 0    | 0x00000000   | [见下一个问题]        |

2. 我们已经将内核和用户环境放在相同的地址空间中。为什么用户程序不能读写内核内存？什么具体机制保护内核内存？

3. 此操作系统可以支持的最大物理内存量是多少？为什么？

4. 如果我们实际上拥有最大数量的物理内存，那么管理内存有多少空间开销？这个开销是如何分解的？

5. 重新访问`kern/entry.S`和 `kern/entrypgdir.c 中`的页表设置。在我们打开分页后，EIP 仍然是一个很小的数字（略高于 1MB）。我们在什么时候过渡到在 KERNBASE 之上的 EIP 上运行？是什么让我们可以在启用分页和开始在高于 KERNBASE 的 EIP 上运行之间继续以低 EIP 执行？为什么需要这种转变？

*挑战！* 我们使用了许多物理页来保存 KERNBASE 映射的页表。使用页目录条目中的 PTE_PS（“页大小”）位执行更节省空间的工作。原始 80386*不*支持此位，但在更新的 x86 处理器上支持。因此，您必须参考 [当前英特尔手册的第 3 卷](https://pdos.csail.mit.edu/6.828/2017/readings/ia32/IA32-3A.pdf)。确保您将内核设计为仅在支持它的处理器上使用此优化！

*挑战！* 使用以下命令扩展 JOS 内核监视器：

- 以有用且易于阅读的格式显示适用于当前活动地址空间中特定范围的虚拟/线性地址的所有物理页面映射（或缺少）。例如，您可以输入`“showmappings 0x3000 0x5000”` 以显示物理页面映射以及适用于虚拟地址 0x3000、0x4000 和 0x5000 处的页面的相应权限位。
- 显式设置、清除或更改当前地址空间中任何映射的权限。
- 转储给定虚拟或物理地址范围的内存范围的内容。当范围跨越页面边界时，确保转储代码的行为正确！
- 执行您认为稍后可能对调试内核有用的任何其他操作。（很有可能是这样！）

### 地址空间布局替代方案

我们在 JOS 中使用的地址空间布局并不是唯一可能的。操作系统可能会将内核映射到低线性地址，而将线性地址空间的*高*部分留给用户进程。然而，x86 内核通常不采用这种方法，因为 x86 的一种向后兼容模式，称为*虚拟 8086 模式*，在处理器中“硬连线”以使用线性地址空间的底部，因此不能如果内核映射到那里，则根本不会使用。

甚至有可能将内核设计为不必为自己保留处理器线性或虚拟地址空间的*任何*固定部分，而是有效地允许用户级进程不受限制地使用*整个*4GB虚拟地址空间 - 同时仍然完全保护内核免受这些进程的影响，并保护不同的进程相互之间！

### *附加题1* 

概述如何设计内核以允许用户环境不受限制地使用完整的 4GB 虚拟和线性地址空间。提示：该技术有时被称为“*跟随弹跳内核”*。在您的设计中，一定要准确说明处理器在内核模式和用户模式之间转换时会发生什么，以及内核如何完成这种转换。还描述内核将如何访问该方案中的物理内存和 I/O 设备，以及内核将如何在系统调用等期间访问用户环境的虚拟地址空间。最后，从灵活性、性能、内核复杂度和其他你能想到的因素考虑和描述这种方案的优缺点。



### *附加题2* 

由于我们的 JOS 内核的内存管理系统仅在页面粒度上分配和释放内存，因此我们没有任何可与我们可以在内核中使用的通用`malloc`/`free`设施相媲美的东西。如果我们想要支持某些类型的 I/O 设备需要大于 4KB 的*物理连续*缓冲区，或者如果我们希望用户级环境（而不仅仅是内核）能够分配和映射，这可能是一个问题4MB*超级页面可* 实现最大处理器效率。（参见之前关于 PTE_PS 的挑战问题。）

概括内核的内存分配系统以支持各种 2 的幂分配单元大小的页面，从 4KB 到您选择的某个合理的最大值。确保您有某种方法可以根据需要将较大的分配单元划分为较小的分配单元，并在可能的情况下将多个小分配单元合并回较大的单元。想想在这样的系统中可能出现的问题。

**这样就完成了实验室。** 确保您通过了所有make grade测试，并且不要忘记在`answers-lab2.txt 中`写下您的问题答案和挑战练习解决方案 `的描述`。提交你的修改（包括添加`答案-lab2.txt`）和类型make handin的`实验室`在实验室目录下的手。

## 回答问题汇总

### Exercise1

boot_alloc:分配虚拟地址，实际分配为page_alloc，（因为只是增加指针而已，页表项，PageInfo等都没有设置）

```
static void *
boot_alloc(uint32_t n)
{
	static char *nextfree;	// 下一个空页虚拟地址，static非常重要，接下来有用
	char *result;

	// 初始化nextfree ：第一次被boot_alloc时候
	// 'end' 指向.bss节最高处:
	// 从此代码和数据分配完成，接下来自由分配
	if (!nextfree) {
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}

	//你应该1.分配足够大的内存，2.并更新nextfree，确保了nextfree（地址）以PGSIZE倍数对齐，如果不明白对齐含义，你应该看《深入理解操作系统》中虚拟内存里堆malloc地址对齐的讲解
	// 你的代码
	result = nextfree;
	nextfree = ROUNDUP(result + n, PGSIZE);
	return result;//分配页的va
}


```

mem_init：创建二级页表，填充页表项，建立映射

```
void
mem_init(void)
{
	uint32_t cr0;
	size_t n;

	// 找到这个机器以页为单位到底有多大 (npages & npages_basemem).
	i386_detect_memory();

	// 当你准备好测试的时候，把这一行注释：因为是报错用
	//panic("mem_init: This function is not finished\n");

	//////////////////////////////////////////////////////////////////////
	// 创建初始页目录表
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);

	//////////////////////////////////////////////////////////////////////
	// 插入 PD 作为页表映射：
	// 目前而言，下面这行，不用太多的理解，即：
	// 权限: 内核 R, 用户 R
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	//////////////////////////////////////////////////////////////////////
	// 你应该分配一个PageInfo的数组，你应该知道这个结构体定义，什么用：所有能分配和访问的物理页
	// 内核通过这个数组追踪页， 'npages' 代表了数组的长度：页面总数。 你应该使用 memset，来完成初始化页面为0。同时虚拟页表映射部分pages数组
	// 你的代码:模仿上面的页目录表设置即可
	pages = (struct PageInfo *) boot_alloc(sizeof(struct PageInfo) * npages);
	memset(pages, 0, sizeof(struct PageInfo) * npages);
	//////////////////////////////////////////////////////////////////////
	// 完成了内核的页目录表，页表，空页分配，初始化后，以后的操作将会通过 page_* functions. 完成。其中的操作，指映射内存：boot_map_region或者page_insert
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	check_page();

	//////////////////////////////////////////////////////////////////////
	// 现在我们解决以下虚拟内存sd：flags

	//////////////////////////////////////////////////////////////////////
	// 对于线性地址 UPAGES （用户只读）进行页面映射
	// 权限设定:
	//    -  UPAGES 的新快照--权限 内核 读，用户 读
	//      (ie. perm = PTE_U | PTE_P)
	//    - 页面本身权限 -- 内核 RW, 用户无权限
	// 你的代码:
	boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U);
	//////////////////////////////////////////////////////////////////////
	//  使用 'bootstack' 引用的物理内存作为内核栈.  内核栈从虚拟地址 KSTACKTOP 开始向下增长.
	// 从 [KSTACKTOP-PTSIZE, KSTACKTOP)
	// 作为内核栈, 但是划分为两块:
	//     * [KSTACKTOP-KSTKSIZE, KSTACKTOP) -- 有物理内存作为映射
	//     * [KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- 没有物理内存对应; 所以如果内核栈溢出, 会出错而不是覆写溢出对应的内存，比如guard page
	//     权限: 内核 可读可写, 用户 无权限
	// 你的代码:
	boot_map_region(kern_pgdir, KSTACKTOP-KSTKSIZE, KSTKSIZE, PADDR(bootstack), PTE_W);
	//////////////////////////////////////////////////////////////////////
	// 映射所有 KERNBASE 开始的物理内存.
	// Ie.  虚拟地址范围 [KERNBASE, 2^32) 应该映射
	//      到实际物理地址范围 [0, 2^32 - KERNBASE)
	// 我们可能实际没有这么大： 2^32 - KERNBASE 物理内存的字节, but
	// 但是你应该还是要设置这个映射.
	// 权限: 内核 可读可写, 用户 无权限
	// 你的代码:
boot_map_region(kern_pgdir, KERNBASE, 0xffffffff - KERNBASE, 0, PTE_W);
	// 检查初始化的页目录表是否正确设置
	check_kern_pgdir();

	// 从小页表转换到 我们创建的全kern_pgdir
	// 页目录表.	我们的pc现在应该指向
	// 在 KERNBASE and KERNBASE+4MB , 那么两个页表都会有这样的地址映射
	// 如果机器这时候重启了, 你应该就是设置kern_pgdir页目录表的时候错了.
	lcr3(PADDR(kern_pgdir));

	check_page_free_list(0);

	// entry.S 设置了cr0中的 flags  (包括了小页表建立).  我们这里设置一些我们需要的flags
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// 一些更多的设置,只有当kern_pgdir页目录表初始化后才能起效果
	check_page_installed_pgdir();
}
```

page_init：对pages数组初始化

```
void
page_init(void)
{ 
	size_t i;
	for (i = 0; i < npages; i++) {
		if(i == 0)
			{	pages[i].pp_ref = 1;
				pages[i].pp_link = NULL;
			}
		else if(i>=1 && i<npages_basemem)
		{
			pages[i].pp_ref = 0;
			pages[i].pp_link = page_free_list; 
			page_free_list = &pages[i];
		}
		else if(i>=IOPHYSMEM/PGSIZE && i< EXTPHYSMEM/PGSIZE )
		{
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
		}
	
		else if( i >= EXTPHYSMEM / PGSIZE && 
				i < ( (int)(boot_alloc(0)) - KERNBASE)/PGSIZE)//KERNBASE注意要加，pages里i就是物理地址从0开始的页数
		{
			pages[i].pp_ref = 1;
			pages[i].pp_link =NULL;
		}
		else
		{
			pages[i].pp_ref = 0;
			pages[i].pp_link = page_free_list;
			page_free_list = &pages[i];
		}

	}
	
}
```

page_alloc：从空闲表中分配一页

```
struct PageInfo *
page_alloc(int alloc_flags)
{
	struct PageInfo *res = page_free_list;
	if (res == NULL) {
		cprintf("page_alloc: out of free memory\n");
		return NULL;
	}
	page_free_list = res->pp_link;
	res->pp_link = NULL;
	if (alloc_flags & ALLOC_ZERO) {
		memset(page2kva(res), 0, PGSIZE);
	}
	return res;
}


```

page_free释放页到空闲表中

```
void
page_free(struct PageInfo *pp)
{
	// 你的代码
	// Hint: 你应该调用panic如果 pp->pp_ref 非0（有被引用） 或pp->pp_link 非空（找不到空闲表就无法找到要做空的页面）.
	if (pp->pp_ref != 0 || pp->pp_link != NULL) {
		panic("page_free: pp->pp_ref is nonzero or pp->pp_link is not NULL\n");
	}
	pp->pp_link = page_free_list;
	page_free_list = pp;//放入空闲表头部
	
}
```

### Exercise2

无

### Exercise3

虚拟地址：我们的代码都会被mmu进行转换，因此指针只能作为虚拟地址（或者偏移量）

### Exercise4

page_walk从页目录开始找到va point to PTE

```
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	// Fill this function in
	pde_t* ppde = pgdir + PDX(va);
	if (!(*ppde & PTE_P)) {								
		if (create) {
			struct PageInfo *pp = page_alloc(1);
			if (pp == NULL) {
				return NULL;
			}
			pp->pp_ref++;
			*ppde = (page2pa(pp)) | PTE_P | PTE_U | PTE_W;	
		} else {
			return NULL;
		}
	}

	return (pte_t *)KADDR(PTE_ADDR(*ppde)) + PTX(va);		//注意返回的是页表项虚址
}
```

boot_map_region给定范围进行映射

```
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// Fill this function in
	size_t pgs = size / PGSIZE;
	if (size % PGSIZE != 0) {//多一页
		pgs++;
	}
	for (int i = 0; i < pgs; i++) {//对页表进行映射：填充页表项
		pte_t *pte = pgdir_walk(pgdir, (void *)va, 1);
		if (pte == NULL) {
			panic("boot_map_region(): out of memory\n");
		}
		*pte = pa | PTE_P | perm;//填充页表项
		pa += PGSIZE;
		va += PGSIZE;
	}
}
```

page_insert插入一页：页表项的创建或者替换

```
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	// Fill this function in
	pte_t *pte = pgdir_walk(pgdir, va, 1);//我们可能新建了一个页表项
	if (pte == NULL) {
		return -E_NO_MEM;
	}
	pp->pp_ref++;										
	if ((*pte) & PTE_P) {								//替换这个页表项
		page_remove(pgdir, va);
	}
	physaddr_t pa = page2pa(pp);
	*pte = pa | perm | PTE_P;
	pgdir[PDX(va)] |= perm;//页目录也要同步权限
	
	return 0;
}
```

page_lookup根据虚拟地址找到PageInfo

```
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	// Fill this function in
	struct PageInfo *pp;
	pte_t *pte =  pgdir_walk(pgdir, va, 0);			//不允许创建页表项
	if (pte == NULL) {
		return NULL;
	}
	if (!(*pte) & PTE_P) {
		return NULL;
	}
	physaddr_t pa = PTE_ADDR(*pte);					
	pp = pa2page(pa);								
	if (pte_store != NULL) {
		*pte_store = pte;
	}
	return pp;
}

```

page_remove删除一页

```
void
page_remove(pde_t *pgdir, void *va)
{
	// Fill this function in
	pte_t *pte_store;
	struct PageInfo *pp = page_lookup(pgdir, va, &pte_store);
	if (pp == NULL) {
		return;
	}
	page_decref(pp);
	*pte_store = 0;
	tlb_invalidate(pgdir, va);
}
```

happy！

![](https://tva1.sinaimg.cn/large/008i3skNly1gvsrhkz6r0j30oi07mjsf.jpg)

### Exercise5

1. 我们的虚拟内存包含了内核和用户空间映射. 为什么用户程序不能访问内核空间?有什么机制？

A：页表的PTE_U做权限保护。

2. 操作系统最大能支持多大的物理空间？为什么？

A：总共有4KB * 1K = 4MB的大小，用来存放pages数组，PageInfo有8B，也就是总共可以存4MB/8B=0.5M个页面，页大小4KB，那么内存支持为0.5M*4KB=2GB，所以最多提供2GB。之所以不是4GB是因为这里最终使用pages（UPAGES）来管理内存并且只有4MB，所以二级页表好像有点拉垮？总之如果pages扩展到8MB就会有4GB

3. 如果物理内存尽可能用，那么我们能有多大的地方来管理内存?

A：pages数组4MB，总页表对应页表项4B*0.5M=2MB，对应只用了内核页目录2KB，所以6MB+2KB。

4. 是什么时候EIP从低地址到KERNBASE上开始运行？是什么导致了这种转变？为什么？

A：是页表设置完毕之后。mmu会将每个地址访问（数据/代码）通过cr3来进行转换。原先给定的偏移只能映射4MB，但是代码会给出高于这个偏移能力的地址。因此只能通过划分地址，访问编写的多级页表，根据所给的**填写物理地址的页表/页目录项**来完成映射。

### 附加题1

### 附加题2

## 关键文件代码注释

本实验中有用的东西快速回忆

```
宏 
PADDR(kva)
kDDR(pa)
UVPT
UPAGES
KERNBASE
IOPHYSMEM
PGSIZE
PTSIZE
PTE_ADDR
PDX(la)
PTX(la)
npages
npages_basemem
npages_extmem

函数
page2pa(struct PageInfo *pp)
pa2page(pa)
page2kva(struct PageInfo *pp)

定义
unit32_t pte_t
unit32_t pde_t
struct PageInfo{
struct PageInfo *pp_link;
int pp_ref;
}
struct PageInfo *pages;
pde_t *kern_pgdir
```



### inc/memlayout.h

```
#ifndef JOS_INC_MEMLAYOUT_H
#define JOS_INC_MEMLAYOUT_H

#ifndef __ASSEMBLER__
#include <inc/types.h>
#include <inc/mmu.h>
#endif /* not __ASSEMBLER__ */

/*
 * 这个文件定义了实验os中的虚拟空间布局,
 * 包括了内核空间和用户空间布局.
 */

// 全局描述符的存放偏移
#define GD_KT     0x08     // 内核代码
#define GD_KD     0x10     // 内核数据
#define GD_UT     0x18     // 用户代码
#define GD_UD     0x20     // 用户数据
#define GD_TSS0   0x28     // 对于CPU0的段选择子

/*
 *           虚拟空间   :                              权限
 *                                                    kernel/user
 *
 *    4 G   -------->  +------------------------------+
 *                     |                              | RW/--
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     :              .               :
 *                     :              .               :
 *                     :              .               :
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| RW/--
 *                     |                              | RW/--
 *                     |   内核空间：映射lab1放的物理地址 | RW/--
 *                     |                              | RW/--
 *    内核空间起点, ---->  +------------------------------+ 0xf0000000      
 *                     |     CPU0's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                   |
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     |     CPU1's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                 PTSIZE
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     :              .               :                   |
 *                     :              .               :                   |
 *    MMIOLIM ------>  +------------------------------+ 0xefc00000      --+
 *                     |       Memory-mapped I/O      | RW/--  PTSIZE
 * ULIM, MMIOBASE -->  +------------------------------+ 0xef800000
 *                     |  Cur. 页表存放：（用户只读）     | R-/R-  PTSIZE
 *    UVPT      ---->  +------------------------------+ 0xef400000
 *                     |          RO PAGES            | R-/R-  PTSIZE
 *    UPAGES    ---->  +------------------------------+ 0xef000000
 *                     |           RO ENVS            | R-/R-  PTSIZE
 * UTOP,UENVS ------>  +------------------------------+ 0xeec00000
 * UXSTACKTOP -/       |     User Exception Stack     | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebff000
 *                     |       Empty Memory (*)       | --/--  PGSIZE
 *    用户栈顶    --->  +------------------------------+ 0xeebfe000
 *                     |              用户栈           | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebfd000
 *                     |                              |
 *                     |                              |
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     .                              .
 *                     .                              .
 *                     .                              .
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 *                     |     用户代码，数据 & 堆         |
 *    UTEXT -------->  +------------------------------+ 0x00800000
 *    PFTEMP ------->  |       Empty Memory (*)       |        PTSIZE
 *                     |                              |
 *    UTEMP -------->  +------------------------------+ 0x00400000      --+
 *                     |       Empty Memory (*)       |                   |
 *                     | - - - - - - - - - - - - - - -|                   |
 *                     |  User 符号表 Data (可选)       |                 PTSIZE
 *    用户数据起始 ---->  +------------------------------+ 0x00200000        |
 *                     |       Empty Memory (*)       |                   |
 *    0 ------------>  +------------------------------+                 --+
 *
 * (*) Note: 内核保证invalid memory永远不会被映射.
 *     "Empty Memory" 可以用于需要情况下的映射
 *     如果有必要，用户程序可以暂时占用虚拟内存内存UTEMP.
 */


// 页目录结构从这个地址开始
#define	KERNBASE	0xF0000000

// At IOPHYSMEM (640K) 有个 384K 空洞留给 I/O.  From the kernel,
// IOPHYSMEM 可以指定为 KERNBASE + IOPHYSMEM.  空洞
// 在物理地址 EXTPHYSMEM 截止
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// 内核栈
#define KSTACKTOP	KERNBASE
#define KSTKSIZE	(8*PGSIZE)   		// 内核栈大小
#define KSTKGAP		(8*PGSIZE)   		// guard内核栈大小：防止访问越界

// 内存映射IO虚址.
#define MMIOLIM		(KSTACKTOP - PTSIZE)
#define MMIOBASE	(MMIOLIM - PTSIZE)

#define ULIM		(MMIOBASE)

/*
 * 只读的用户映射！ 从这里往下直到UTOP都是用户只读
 * 在env分配时候，这是全局页表的映射地址
 */

// 用户只读的页表基虚址 (see 'uvpt' below)
#define UVPT		(ULIM - PTSIZE)
// 只读的页表复制
#define UPAGES		(UVPT - PTSIZE)
// 只读的全局env页表虚址
#define UENVS		(UPAGES - PTSIZE)

/*
 * 用户虚址顶部，用户可以从 UTOP-1 开始向下访问!
 */

// 用户访问虚址顶
#define UTOP		UENVS
// 用户的异常栈顶部
#define UXSTACKTOP	UTOP
// Next page left invalid to guard against exception stack overflow; then:
// Top of normal user stack
#define USTACKTOP	(UTOP - 2*PGSIZE)

// Where user programs generally begin
#define UTEXT		(2*PTSIZE)

// Used for temporary page mappings.  Typed 'void*' for convenience
#define UTEMP		((void*) PTSIZE)
// Used for temporary page mappings for the user page-fault handler
// (should not conflict with other temporary page mappings)
#define PFTEMP		(UTEMP + PTSIZE - PGSIZE)
// The location of the user-level STABS data structure
#define USTABDATA	(PTSIZE / 2)

#ifndef __ASSEMBLER__

typedef uint32_t pte_t;
typedef uint32_t pde_t;

#if JOS_USER
/*
 * 首先根据虚拟地址进入页目录（lib/entry.S里看设定pte_t/pde_t结构（uvpd和uvpt）），从
 * [UVPT, UVPT + PTSIZE) 范围，找到页表物理地址。因此
 * 页目录表同时也可以认为是页表：实际上就这么干了，具体代码看下面
 *
 * 简单来说，进程自有的页表映射由于内存问题被分为两部分：页目录表和页表，我们根据虚拟地址就可以进入自有页表，根据uvpt[N]（得到uvpt[N]方法： (UVPT + (UVPT >> PGSHIFT))）得到”页“pa，完成物理映射
 * 不过还有问题：1.从虚拟地址访问的页目录表，那么他的物理地址在哪？2.页表直接访问页的地址吗？
 */
 
 //1.进行位移来映射：PADDR，KADDR来完成
 //2.页表访问了pages数组，用于管理页的申请，引用，释放
 //说句题外话，xv6设计为内核/用户的页表切换，但是linux不进行切换页表，因为内核/用户切换开销大。并且对于内核页表，有两种方式：1.共享2.复制。linux选择了后者，并且统一内核/用户的页表。并且访问得到”物理地址“后，并没有直接访问页。还需要一个空闲表（哈希队列）来完成页的管理，所以很麻烦。
 
extern volatile pte_t uvpt[];     // 页表
extern volatile pde_t uvpd[];     // 页目录表
#endif

/*
 * 在UPAGES里存储的PageInfo，可以认为是个描述符，实际上就是个链表：
 * Read/write 内核, read-only 用户.
 *
 * 我们知道每个页表项唯一映射一个虚页和物理页的地址
 * 但是这里的PageInfo没有存储物理页，只是有个指针，具体物理地址去kern/pmap.h里的page2pa()找
 */
 
struct PageInfo {
	// 物理内存空闲（未被进程使用：让我们考虑生产者-消费者问题）表的第一个页指针：注意这两种PageInfo的不同使用情景：页表项存放PageInfo->空闲表的头->空闲表访问
	struct PageInfo *pp_link;

	// pp_ref是指向空闲表里空闲页面的引用计数：考虑多个paddr映射同一个vaddr，如果为0，我们就可以认为这个物理页可以被重新分配给任何进程使用
	// 通过 page_alloc来分配内存，还有映射
	// 在boot时候就会使用pamp.c来分配内存映射，做一个小页表
	// boot_alloc 并没有有效的引用计数，所以可以认为是个”小“页表，第一个实验就要实现这个

	uint16_t pp_ref;
};

#endif /* !__ASSEMBLER__ */
#endif /* !JOS_INC_MEMLAYOUT_H */
```

### kern/pmap.h

```
/* See COPYRIGHT for copyright information. */
//pmap一般用来分配内存，建立/删除映射，所以相当于虚拟空间和物理空间之间的接口：接受/返回 虚拟地址/物理地址，从而进行内存交互。


#ifndef JOS_KERN_PMAP_H
#define JOS_KERN_PMAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/memlayout.h>
#include <inc/assert.h>

extern char bootstacktop[], bootstack[];

extern struct PageInfo *pages;
extern size_t npages;

extern pde_t *kern_pgdir;


/* 这个宏 接受虚拟的内核地址--当然指向KERNEBASE上, 返回相应的物理地址.  如果传递非内核地址，报错。其中，他最高能够达到256MB物理地址的映射
 */
#define PADDR(kva) _paddr(__FILE__, __LINE__, kva)

static inline physaddr_t
_paddr(const char *file, int line, void *kva)
{
	if ((uint32_t)kva < KERNBASE)
		_panic(file, line, "PADDR called with invalid kva %08lx", kva);
	return (physaddr_t)kva - KERNBASE;
}

/* 这个宏 接受物理地址，返回虚拟地址.  如果物理地址是错误的，就报错
#define KADDR(pa) _kaddr(__FILE__, __LINE__, pa)

static inline void*
_kaddr(const char *file, int line, physaddr_t pa)
{
	if (PGNUM(pa) >= npages)
		_panic(file, line, "KADDR called with invalid pa %08lx", pa);
	return (void *)(pa + KERNBASE);
}


enum {
	// 对于分配内存页, 我们对所有的内存进行置0.（当然很多内存分配不置0，比如malloc这样的，也有置0的，比如linux中，mmap加载ELF只分配虚拟地址，访问.bss节时，没有页表项，中断分配内存时就并不访问磁盘，而是找个物理内存页，置0）
	ALLOC_ZERO = 1<<0,
};

void	mem_init(void);

void	page_init(void);
struct PageInfo *page_alloc(int alloc_flags);
void	page_free(struct PageInfo *pp);
int	page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm);
void	page_remove(pde_t *pgdir, void *va);
struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void	page_decref(struct PageInfo *pp);

void	tlb_invalidate(pde_t *pgdir, void *va);

static inline physaddr_t
page2pa(struct PageInfo *pp)
{
	return (pp - pages) << PGSHIFT;
}

static inline struct PageInfo*
pa2page(physaddr_t pa)
{
	if (PGNUM(pa) >= npages)
		panic("pa2page called with invalid pa");
	return &pages[PGNUM(pa)];
}

static inline void*
page2kva(struct PageInfo *pp)
{
	return KADDR(page2pa(pp));
}

pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

#endif /* !JOS_KERN_PMAP_H */
```



### kern/pmap.c

```
/* See COPYRIGHT for copyright information. */
//在这里我们将会建立完整的页面映射，你会填写以下代码

/*exercise1
* 1.boot_alloc() 启动的时候给内核分配虚拟页
* 2.mem_init()（仅限于调用`check_page_free_list(1)`）给内核建立页目录，并建立pages数组，然后建立映射
* 3.page_init() 建立空闲表
* 4.page_alloc() 从空闲表中找到空页并分配
* 5.page_free() 释放页面为空页放入空闲表中
*/
/*exercise 4
* 1.pgdir_walk() 访问两级”页表“，找到PDE的虚拟地址
* 2.boot_map_region() 填充映射
* 3.page_lookup() 根据虚拟地址，得到PageInfo的虚拟地址
* 4.page_remove() 删除页
* 5.page_insert() 插入页
*/

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// 这些变量由i386_detect_memory()设定
size_t npages;			// 物理页数量 (in pages)
static size_t npages_basemem;	// 基础内存页数量

// 这些变量由 mem_init()设定
pde_t *kern_pgdir;		// 内核的初始页目录虚基址
struct PageInfo *pages;		// 能够管理的页面
static struct PageInfo *page_free_list;	// 空闲表


// --------------------------------------------------------------
// 检测机器的物理内存配置
// --------------------------------------------------------------

static int
nvram_read(int r)
{
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void
i386_detect_memory(void)
{
	size_t basemem, extmem, ext16mem, totalmem;

	// 使用CMOS调用来测量基本和扩展内存
	// (CMOS calls 以KB单位返回结果.)
	basemem = nvram_read(NVRAM_BASELO);
	extmem = nvram_read(NVRAM_EXTLO);
	ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	// 计算可用物理页数量：基本和扩展内存中
	if (ext16mem)
		totalmem = 16 * 1024 + ext16mem;
	else if (extmem)
		totalmem = 1 * 1024 + extmem;
	else
		totalmem = basemem;

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	cprintf("Physical memory: %uK available, base = %uK, extended = %uK\n",
		totalmem, basemem, totalmem - basemem);
}


// --------------------------------------------------------------
// 从 UTOP ：内核上开始建立页表映射.
// --------------------------------------------------------------

static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
static void check_page_free_list(bool only_low_memory);
static void check_page_alloc(void);
static void check_kern_pgdir(void);
static physaddr_t check_va2pa(pde_t *pgdir, uintptr_t va);
static void check_page(void);
static void check_page_installed_pgdir(void);

// 我们的boot内存分配调用page_alloc() 来实现.注：为了区分，空页意味着未被分配，空闲页则可能是空页；也可能不是，只是未被进程使用
//
// 如果指定内存大小n字节>0, 分配连续的内存，一般是稍大一点，来足够满足n字节的大小需求
// 但是并没有初始化这个内存. 返回内核的虚拟地址.
//
// 如果n==0, 返回紧邻着的下一个空页（一开始分配，一定是引用计数为0的空闲表结点）
//
// 如果内存耗尽, boot_alloc 应该报错
// 这个函数仅在boot给内核分配内存使用,
// 在 page_free_list 空闲表完成之前调用函数.
static void *
boot_alloc(uint32_t n)
{
	static char *nextfree;	// 下一个空页虚拟地址
	char *result;

	// 初始化nextfree ：第一次被boot_alloc时候
	// 'end' 指向.bss节最高处:
	// 从此代码和数据分配完成，紧接着自由分配
	if (!nextfree) {
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}

	//你应该1.分配足够大的内存，2.并更新nextfree，确保了nextfree（地址）以PGSIZE倍数对齐，如果不明白对齐，你应该看《深入理解操作系统》中虚拟内存里堆malloc地址对齐的讲解
	// 你的代码
	
	return NULL;
}

// 建立两级页表和pages数组
//    对于内核：kern_pgdir 是是内核页目录表虚拟基址
//
// 这个部分仅仅分配了内核的映射
// (ie. addresses >= UTOP).  用户映射建立会在进程创建的时候发生
//
// 从 UTOP 到 ULIM, 用户只读
// 在 ULIM 上，用户禁止访问
void
mem_init(void)
{
	uint32_t cr0;
	size_t n;

	// 找到这个机器以页为单位到底有多大 (npages & npages_basemem).
	i386_detect_memory();

	// 当你准备好测试的时候，把这一行注释：因为是报错用
	panic("mem_init: This function is not finished\n");

	//////////////////////////////////////////////////////////////////////
	// 创建初始页目录表
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);//只有4KB
	memset(kern_pgdir, 0, PGSIZE);

	//////////////////////////////////////////////////////////////////////
	// 递归插入 PD 作为页表映射：
	// 只设置了一个页目录项，相当于同时只建立了一个页表，剩下暂时没有建立映射:这里非常奇怪的是将UVPT通过页目录项映射到了kern_pgdir上，相当于一个4kb虚拟页表映射到了内核页目录上（为什么虚拟内存中要映射页表UVPT和UPAGES呢），而不是映射到新的物理页作为页表的存储。
	// 权限: 内核 , 用户 
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	//////////////////////////////////////////////////////////////////////
	// 你应该分配一个PageInfo的数组，你应该知道这个结构体定义，什么用
	// 内核通过这个数组追踪物理页， 'npages' 代表了数组的长度：页面总数。 你应该使用 memset，来完成初始化页面为0
	// 你的代码:


	//////////////////////////////////////////////////////////////////////
	// 完成了内核的页目录表，页表，空页分配，初始化后，以后的操作将会通过 page_* functions. 完成。其中的操作，指分配内存：boot_map_region或者page_insert？？？
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	check_page();

	//////////////////////////////////////////////////////////////////////
	// 现在我们解决以下虚拟内存sd：flags

	//////////////////////////////////////////////////////////////////////
	// 对于线性地址 UPAGES （用户只读）进行页面映射
	// 权限设定:
	//    -  UPAGES 的新快照-- 内核 读，用户 读
	//      (ie. perm = PTE_U | PTE_P)
	//    - 页面本身权限 -- 内核 RW, 用户无权限
	// 你的代码:

	//////////////////////////////////////////////////////////////////////
	//  使用 'bootstack' 引用的物理内存作为内核栈.  内核栈从虚拟地址 KSTACKTOP 开始向下增长.
	// 从 [KSTACKTOP-PTSIZE, KSTACKTOP)
	// 作为内核栈, 但是划分为两块:
	//     * [KSTACKTOP-KSTKSIZE, KSTACKTOP) -- 有物理内存作为映射
	//     * [KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- 没有物理内存对应; 所以如果内核栈溢出, 会出错而不是覆写溢出对应的内存，比如guard page
	//     权限: 内核 可读可写, 用户 无权限
	// 你的代码:

	//////////////////////////////////////////////////////////////////////
	// 映射所有 KERNBASE 开始的物理内存.
	// Ie.  虚拟地址范围 [KERNBASE, 2^32) 应该映射
	//      到实际物理地址范围 [0, 2^32 - KERNBASE)
	// 我们可能实际没有这么大： 2^32 - KERNBASE 物理内存的字节, but
	// 但是你应该还是要设置这个映射.
	// 权限: 内核 可读可写, 用户 无权限
	// 你的代码:

	// 检查初始化的页目录表是否正确设置
	check_kern_pgdir();

	// 从小页表转换到 我们创建的全kern_pgdir
	// 页目录表.	我们的pc现在应该指向
	// 在 KERNBASE and KERNBASE+4MB , 那么两个页表都会有这样的地址映射
	// 如果机器这时候重启了, 你应该就是设置kern_pgdir页目录表的时候错了.
	lcr3(PADDR(kern_pgdir));

	check_page_free_list(0);

	// entry.S 设置了cr0中的 flags  (包括了小页表建立).  我们这里设置一些我们需要的flags
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// 一些更多的设置,只有当kern_pgdir页目录表初始化后才能起效果
	check_page_installed_pgdir();
}

// -------------------------------------------------------------
// 追踪物理页表.
// 不要忘记我们的页表项PageInfo数组已经建立，但是空闲表还没建立
// 页面应该在空闲表里, 并且空闲表里存在空页结点
// --------------------------------------------------------------

// 初始化页结构，还有空闲表
// 完成之后, 不应该再使用boot_alloc. 而是应该使用下面的allocator函数来通过空闲表来分配，重分配物理页面
void
page_init(void)
{ 
	// 现在我们的页面都是空的：引用计数为0.
	// 但是这当然不正确.  什么样的内存是真的空的?
	//  1)将物理页面0标记为正在使用（非空闲）。这样我们就保留了实模式的IDT和基本输入输出系统结构，以防我们需要它们。(目前我们没有，但是...)
  //
  // 2)基础内存的剩余部分，[PGSIZE，npages_basemem * PGSIZE)认为空闲。
  //
  // 3)然后是IO空[IOPHYSMEM，EXTPHYSMEM)，它必须从不被分配。
  //
  // 4)然后扩展内存[EXTPHYSMEM，...).
  
	//既然有的页正在被使用，有的是空闲的。那么
	1.内核在物理内存中的哪里？
	2.哪些页面已经在使用页表和其他数据结构？
	// 请你修改代码？？来回答这一点.
	// 注意: 请不要碰那些和空闲页对应的物理内存
	size_t i;
	for (i = 0; i < npages; i++) {
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
}


// 分配一页物理页.  如果 (alloc_flags & ALLOC_ZERO), 那么你就应该多做一点：填充0
// 返回这个物理页面必须以 '\0' 结尾.  不要增加引用计数 - 这是由调用函数做的 (或者通过 page_insert函数来做)，因为分配页面只是空页，还未被使用.
//
// 确保将上述页面设置了 pp_link  为 NULL 以便于
// page_free 这个页面释放函数可以防止重复释放的bug.
//
// 如果可分配物理内存为0返回 NULL .
//
// Hint: 使用 page2kva 和 memset来完成代码书写
struct PageInfo *
page_alloc(int alloc_flags)
{
	// 你的代码
	return 0;
}

//
// 向空闲表返回一页.
// (当pp->pp_ref引用计数为0，代表页面应该释放，于是应该从空闲表拿出，放入头部，方便下次当作空页取出使用)
//
void
page_free(struct PageInfo *pp)
{
	// 你的代码
	// Hint: 你应该调用panic如果 pp->pp_ref 非0（有被引用） 或pp->pp_link 非空（找不到空闲表就无法找到要做空的页面）.
	
	
}

// 减少引用计数，当为0时候，free掉它
void
page_decref(struct PageInfo* pp)
{
	if (--pp->pp_ref == 0)
		page_free(pp);
}

// 给定pgdir作为指向页目录表的指针, pgdir_walk 返回指向页表的虚拟地址'va'（pte）.
// 这需要走两级“页表”：进程的”页表“来找到页目录表，页目录表来找到页表
//
// 寻找页表过程中相关的页表项可能并不存在.我们可能就应该创建页表，来开始新的映射建立
// 如果创造新页表失败, 那么 create == false, 那么pgdir_walk 返回NULL.
// 否则, pgdir_walk 通过调用page_alloc分配一个新页作为新页表.
//    - 如果分配失败, pgdir_walk 返回 NULL.
//    - 否则, 增加新页的引用计数,清除页内容
//	然后 pgdir_walk 返回一个新的页表页的指针.
//
// Hint 1: 你可以把PageInfo * 通过page2pa(在kern/pmap.h里)转换得到页物理地址 
//
// Hint 2:  x86 MMU 检查权限位：无论是页目录表还是页表，所以你或许应该放宽你的权限：走个形式
//
// Hint 3: 看  inc/mmu.h 里有对多级页表（页目录表和页表）的宏定义，有些帮助
//
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	// Fill this function in
	return NULL;
}

// 给定页目录表的根外设的页表映射 
// 在pgdir中映射虚拟地址 [va, va+size) 到物理地址[pa, pa+size)
// 大小是 PGSIZE, 并且va和pa都要地址页对齐
// 使用权限位 perm|PTE_P 来进行入口时mmu检查.
//
// 这个函数仅仅为了建立从UTOP上的静态映射（RO ENVS）,比如，我们不应该增加pp_ref引用计数。
//
// Hint: 通过使用 pgdir_walk解决寻找到页表项填充映射问题
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// 你的代码
}


// 分配页表中：映射物理页 'pp' 到虚拟页 'va'.
// 页表PTE权限 (低 12 bits) 
// 应该被设置为'perm|PTE_P'.
//
// 
//   - 如果已经有虚拟页映射 'va', 应该调用page_remove().
//   - 如果有必要, on demand, 一个页表应该被分配，然后快照插入到pgdir页目录表
//   - 如果插入成功，那么pp->pp_ref 应该增加：进程引用+1
//   - 如果已经有页表项占用了va，那么我们的页目录表项就应该无效
//
//  极端情况处理hint: 你应该考虑在同一个页目录表中，同样的物理页地址重复映射同一个虚拟地址，应该怎么处理。然而，不需要在本函数代码中区分，因为会导致无法避免的bug，你应该在多层调用中分散处理。
//
// 返回值:
//   0 成功插入
//   -E_NO_MEM, 如果页表不能被分配
//
// Hint: 解决办法是调用 pgdir_walk, page_remove,page2pa
//
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	// 你的代码
	return 0;
}

//
// 返回物理页面的虚拟地址
// 如果页表项 pte_store 不为0, 那么我们就应该认为找到了这个页表项，.它会将被page_remove删除，并且会在系统调用参数的时候验证权限，防止用户直接调用。
//
// 如果没有页面对应va，那么返回 NULL
//
// Hint: 通过调用 pgdir_walk 和 pa2page来写代码
//
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	// 你的代码
	return NULL;
}

// 删除页表项
// 给定页虚拟地址 'va'，来取消映射
// 如果没有物理页被映射，那么就应该什么都不干
//
// 细节:
//   - 引用计数应该-1
//   - 如果引用计数是0，那么应该做空页面
//   - 页表项存在的话，那么应该被设置为 0.
//   - 同时对于页表项的缓存也应该无效
//
// Hint: 你赢该通过调用page_lookup,tlb_invalidate,  page_decref来帮助代码书写
//
void
page_remove(pde_t *pgdir, void *va)
{
	//  你的代码
}

//
// 仅仅当页表项已经被修改之后，才对页表项缓存无效化
//
void
tlb_invalidate(pde_t *pgdir, void *va)
{
	// 仅仅当我们修改这个地址空间，才刷新这个页表项缓存
	// 刷新之后，这只有一个地址空间，所以是无效的？？？没懂
	invlpg(va);
}


// --------------------------------------------------------------
// 检查函数
// --------------------------------------------------------------

//
// Check that the pages on the page_free_list are reasonable.
//
static void
check_page_free_list(bool only_low_memory)
{
	struct PageInfo *pp;
	unsigned pdx_limit = only_low_memory ? 1 : NPDENTRIES;
	int nfree_basemem = 0, nfree_extmem = 0;
	char *first_free_page;

	if (!page_free_list)
		panic("'page_free_list' is a null pointer!");

	if (only_low_memory) {
		// Move pages with lower addresses first in the free
		// list, since entry_pgdir does not map all pages.
		struct PageInfo *pp1, *pp2;
		struct PageInfo **tp[2] = { &pp1, &pp2 };
		for (pp = page_free_list; pp; pp = pp->pp_link) {
			int pagetype = PDX(page2pa(pp)) >= pdx_limit;
			*tp[pagetype] = pp;
			tp[pagetype] = &pp->pp_link;
		}
		*tp[1] = 0;
		*tp[0] = pp2;
		page_free_list = pp1;
	}

	// if there's a page that shouldn't be on the free list,
	// try to make sure it eventually causes trouble.
	for (pp = page_free_list; pp; pp = pp->pp_link)
		if (PDX(page2pa(pp)) < pdx_limit)
			memset(page2kva(pp), 0x97, 128);

	first_free_page = (char *) boot_alloc(0);
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		// check that we didn't corrupt the free list itself
		assert(pp >= pages);
		assert(pp < pages + npages);
		assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

		// check a few pages that shouldn't be on the free list
		assert(page2pa(pp) != 0);
		assert(page2pa(pp) != IOPHYSMEM);
		assert(page2pa(pp) != EXTPHYSMEM - PGSIZE);
		assert(page2pa(pp) != EXTPHYSMEM);
		assert(page2pa(pp) < EXTPHYSMEM || (char *) page2kva(pp) >= first_free_page);

		if (page2pa(pp) < EXTPHYSMEM)
			++nfree_basemem;
		else
			++nfree_extmem;
	}

	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);

	cprintf("check_page_free_list() succeeded!\n");
}

//
// Check the physical page allocator (page_alloc(), page_free(),
// and page_init()).
//
static void
check_page_alloc(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	int nfree;
	struct PageInfo *fl;
	char *c;
	int i;

	if (!pages)
		panic("'pages' is a null pointer!");

	// check number of free pages
	for (pp = page_free_list, nfree = 0; pp; pp = pp->pp_link)
		++nfree;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(page2pa(pp0) < npages*PGSIZE);
	assert(page2pa(pp1) < npages*PGSIZE);
	assert(page2pa(pp2) < npages*PGSIZE);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// free and re-allocate?
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(!page_alloc(0));

	// test flags
	memset(page2kva(pp0), 1, PGSIZE);
	page_free(pp0);
	assert((pp = page_alloc(ALLOC_ZERO)));
	assert(pp && pp0 == pp);
	c = page2kva(pp);
	for (i = 0; i < PGSIZE; i++)
		assert(c[i] == 0);

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	// number of free pages should be the same
	for (pp = page_free_list; pp; pp = pp->pp_link)
		--nfree;
	assert(nfree == 0);

	cprintf("check_page_alloc() succeeded!\n");
}

//
// Checks that the kernel part of virtual address space
// has been set up roughly correctly (by mem_init()).
//
// This function doesn't test every corner case,
// but it is a pretty good sanity check.
//

static void
check_kern_pgdir(void)
{
	uint32_t i, n;
	pde_t *pgdir;

	pgdir = kern_pgdir;

	// check pages array
	n = ROUNDUP(npages*sizeof(struct PageInfo), PGSIZE);
	for (i = 0; i < n; i += PGSIZE)
		assert(check_va2pa(pgdir, UPAGES + i) == PADDR(pages) + i);


	// check phys mem
	for (i = 0; i < npages * PGSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KERNBASE + i) == i);

	// check kernel stack
	for (i = 0; i < KSTKSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KSTACKTOP - KSTKSIZE + i) == PADDR(bootstack) + i);
	assert(check_va2pa(pgdir, KSTACKTOP - PTSIZE) == ~0);

	// check PDE permissions
	for (i = 0; i < NPDENTRIES; i++) {
		switch (i) {
		case PDX(UVPT):
		case PDX(KSTACKTOP-1):
		case PDX(UPAGES):
			assert(pgdir[i] & PTE_P);
			break;
		default:
			if (i >= PDX(KERNBASE)) {
				assert(pgdir[i] & PTE_P);
				assert(pgdir[i] & PTE_W);
			} else
				assert(pgdir[i] == 0);
			break;
		}
	}
	cprintf("check_kern_pgdir() succeeded!\n");
}

// This function returns the physical address of the page containing 'va',
// defined by the page directory 'pgdir'.  The hardware normally performs
// this functionality for us!  We define our own version to help check
// the check_kern_pgdir() function; it shouldn't be used elsewhere.

static physaddr_t
check_va2pa(pde_t *pgdir, uintptr_t va)
{
	pte_t *p;

	pgdir = &pgdir[PDX(va)];
	if (!(*pgdir & PTE_P))
		return ~0;
	p = (pte_t*) KADDR(PTE_ADDR(*pgdir));
	if (!(p[PTX(va)] & PTE_P))
		return ~0;
	return PTE_ADDR(p[PTX(va)]);
}


// check page_insert, page_remove, &c
static void
check_page(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	void *va;
	int i;
	extern pde_t entry_pgdir[];

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// there is no page allocated at address 0
	assert(page_lookup(kern_pgdir, (void *) 0x0, &ptep) == NULL);

	// there is no free memory, so we can't allocate a page table
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) < 0);

	// free pp0 and try again: pp0 should be used for page table
	page_free(pp0);
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) == 0);
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	assert(check_va2pa(kern_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp0->pp_ref == 1);

	// should be able to map pp2 at PGSIZE because pp0 is already allocated for page table
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// should be no free memory
	assert(!page_alloc(0));

	// should be able to map pp2 at PGSIZE because it's already there
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	assert(!page_alloc(0));

	// check that pgdir_walk returns a pointer to the pte
	ptep = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(PGSIZE)]));
	assert(pgdir_walk(kern_pgdir, (void*)PGSIZE, 0) == ptep+PTX(PGSIZE));

	// should be able to change permissions too.
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W|PTE_U) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U);
	assert(kern_pgdir[0] & PTE_U);

	// should be able to remap with fewer permissions
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_W);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should not be able to map at PTSIZE because need free page for page table
	assert(page_insert(kern_pgdir, pp0, (void*) PTSIZE, PTE_W) < 0);

	// insert pp1 at PGSIZE (replacing pp2)
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W) == 0);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should have pp1 at both 0 and PGSIZE, pp2 nowhere, ...
	assert(check_va2pa(kern_pgdir, 0) == page2pa(pp1));
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	assert(pp2->pp_ref == 0);

	// pp2 should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp2);

	// unmapping pp1 at 0 should keep pp1 at PGSIZE
	page_remove(kern_pgdir, 0x0);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// test re-inserting pp1 at PGSIZE
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, 0) == 0);
	assert(pp1->pp_ref);
	assert(pp1->pp_link == NULL);

	// unmapping pp1 at PGSIZE should free it
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// 被 page_alloc 返回
	assert((pp = page_alloc(0)) && pp == pp1);

	// 无可分配空页
	assert(!page_alloc(0));

	// 强制 带回 pp0 ？？？？
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// 计算检查 pgdir_walk
	page_free(pp0);
	va = (void*)(PGSIZE * NPDENTRIES + PGSIZE);
	ptep = pgdir_walk(kern_pgdir, va, 1);
	ptep1 = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(va)]));
	assert(ptep == ptep1 + PTX(va));
	kern_pgdir[PDX(va)] = 0;
	pp0->pp_ref = 0;

	// 检查新分配的页表是否被清除干净（否则映射会错误，超级难debug ）
	memset(page2kva(pp0), 0xFF, PGSIZE);
	page_free(pp0);
	pgdir_walk(kern_pgdir, 0x0, 1);
	ptep = (pte_t *) page2kva(pp0);
	for(i=0; i<NPTENTRIES; i++)
		assert((ptep[i] & PTE_P) == 0);
	kern_pgdir[0] = 0;
	pp0->pp_ref = 0;

	// 返回一个新的空闲链表
	page_free_list = fl;

	// 释放页面：引用计数为0
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	cprintf("check_page() succeeded!\n");
}

// 通过一个已经初始化的 kern_pgdir，检查 page_insert, page_remove, &c, 
static void
check_page_installed_pgdir(void)
{
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	uintptr_t va;
	int i;

	// 检查：对于已经初始化的页表，我们应该可以R/W
	pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	page_free(pp0);
	memset(page2kva(pp1), 1, PGSIZE);
	memset(page2kva(pp2), 2, PGSIZE);
	page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W);
	assert(pp1->pp_ref == 1);
	assert(*(uint32_t *)PGSIZE == 0x01010101U);
	page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W);
	assert(*(uint32_t *)PGSIZE == 0x02020202U);
	assert(pp2->pp_ref == 1);
	assert(pp1->pp_ref == 0);
	*(uint32_t *)PGSIZE = 0x03030303U;
	assert(*(uint32_t *)page2kva(pp2) == 0x03030303U);
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(pp2->pp_ref == 0);

	// 强制带回 pp0 
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// 释放我们带回的页
	page_free(pp0);

	cprintf("check_page_installed_pgdir() succeeded!\n");
}
```

### kern/kclock.h

```
/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_KCLOCK_H
#define JOS_KERN_KCLOCK_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#define	IO_RTC		0x070		/* RTC port */

#define	MC_NVRAM_START	0xe	/* start of NVRAM: offset 14 */
#define	MC_NVRAM_SIZE	50	/* 50 bytes of NVRAM */

/* NVRAM bytes 7 & 8: base memory size */
#define NVRAM_BASELO	(MC_NVRAM_START + 7)	/* low byte; RTC off. 0x15 */
#define NVRAM_BASEHI	(MC_NVRAM_START + 8)	/* high byte; RTC off. 0x16 */

/* NVRAM bytes 9 & 10: extended memory size (between 1MB and 16MB) */
#define NVRAM_EXTLO	(MC_NVRAM_START + 9)	/* low byte; RTC off. 0x17 */
#define NVRAM_EXTHI	(MC_NVRAM_START + 10)	/* high byte; RTC off. 0x18 */

/* NVRAM bytes 38 and 39: extended memory size (between 16MB and 4G) */
#define NVRAM_EXT16LO	(MC_NVRAM_START + 38)	/* low byte; RTC off. 0x34 */
#define NVRAM_EXT16HI	(MC_NVRAM_START + 39)	/* high byte; RTC off. 0x35 */

unsigned mc146818_read(unsigned reg);
void mc146818_write(unsigned reg, unsigned datum);

#endif	// !JOS_KERN_KCLOCK_H
```



### kern/kclock.c

```
/* See COPYRIGHT for copyright information. */

/* 为了读取现实时间用的硬件交互代码. */

#include <inc/x86.h>

#include <kern/kclock.h>


unsigned
mc146818_read(unsigned reg)
{
	outb(IO_RTC, reg);
	return inb(IO_RTC+1);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
	outb(IO_RTC, reg);
	outb(IO_RTC+1, datum);
}
```

### inc/mmu.h

```
#ifndef JOS_INC_MMU_H
#define JOS_INC_MMU_H

/*
 * 这个文件定义了x86下的内存管理单元：内存（访问，切换）处理逻辑，内存分配在pmap.c里，sd
 * 包括了使用页表映射，和段相关的数据结构和算法,
 * 还有%cr0, %cr4, and %eflags寄存器，还有中断.
 * 代码书写分为：Part1/Part2/Part3
 */

/*
 *
 *	Part 1.  给定虚址，在虚拟页表，PageInfo，空闲表中，映射处理的所需要的数据结构和算法.
 *
 */

// 虚拟地址'la'根据实际情况划分成三个部分:
//
// +--------10------+-------10-------+---------12----------+
// | 页目录偏移       |      页表偏移   | 页内偏移             |
// |                |                |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \---------- PGNUM(la) ----------/
//
// PDX, PTX, PGOFF, and PGNUM macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// page number field of address
#define PGNUM(la)	(((uintptr_t) (la)) >> PTXSHIFT)

// page directory index
#define PDX(la)		((((uintptr_t) (la)) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(la)		((((uintptr_t) (la)) >> PTXSHIFT) & 0x3FF)

// offset in page
#define PGOFF(la)	(((uintptr_t) (la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d, t, o)	((void*) ((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
#define NPDENTRIES	1024		// page directory entries per page directory
#define NPTENTRIES	1024		// page table entries per page table

#define PGSIZE		4096		// bytes mapped by a page
#define PGSHIFT		12		// log2(PGSIZE)

#define PTSIZE		(PGSIZE*NPTENTRIES) // bytes mapped by a page directory entry
#define PTSHIFT		22		// log2(PTSIZE)

#define PTXSHIFT	12		// offset of PTX in a linear address
#define PDXSHIFT	22		// offset of PDX in a linear address

// 页表/页目录存放的项的flags部分.
#define PTE_P		0x001	// Present
#define PTE_W		0x002	// 可写
#define PTE_U		0x004	// 用户
#define PTE_PWT		0x008	// 直写：无缓存下使用
#define PTE_PCD		0x010	// 禁用cache
#define PTE_A		0x020	// 可访问
#define PTE_D		0x040	// 脏标：配合回写
#define PTE_PS		0x080	// 页大小
#define PTE_G		0x100	// 全局

// The PTE_AVAIL bits aren't used by the kernel or interpreted by the
// hardware, so user processes are allowed to set them arbitrarily.
#define PTE_AVAIL	0xE00	// Available for software use

// Flags in PTE_SYSCALL may be used in system calls.  (Others may not.)
#define PTE_SYSCALL	(PTE_AVAIL | PTE_P | PTE_W | PTE_U)

// Address in page table or page directory entry
#define PTE_ADDR(pte)	((physaddr_t) (pte) & ~0xFFF)

// Control Register flags
#define CR0_PE		0x00000001	// Protection Enable
#define CR0_MP		0x00000002	// Monitor coProcessor
#define CR0_EM		0x00000004	// Emulation
#define CR0_TS		0x00000008	// Task Switched
#define CR0_ET		0x00000010	// Extension Type
#define CR0_NE		0x00000020	// Numeric Errror
#define CR0_WP		0x00010000	// Write Protect
#define CR0_AM		0x00040000	// Alignment Mask
#define CR0_NW		0x20000000	// Not Writethrough
#define CR0_CD		0x40000000	// Cache Disable
#define CR0_PG		0x80000000	// Paging

#define CR4_PCE		0x00000100	// Performance counter enable
#define CR4_MCE		0x00000040	// Machine Check Enable
#define CR4_PSE		0x00000010	// Page Size Extensions
#define CR4_DE		0x00000008	// Debugging Extensions
#define CR4_TSD		0x00000004	// Time Stamp Disable
#define CR4_PVI		0x00000002	// Protected-Mode Virtual Interrupts
#define CR4_VME		0x00000001	// V86 Mode Extensions

// Eflags register
#define FL_CF		0x00000001	// Carry Flag
#define FL_PF		0x00000004	// Parity Flag
#define FL_AF		0x00000010	// Auxiliary carry Flag
#define FL_ZF		0x00000040	// Zero Flag
#define FL_SF		0x00000080	// Sign Flag
#define FL_TF		0x00000100	// Trap Flag
#define FL_IF		0x00000200	// Interrupt Flag
#define FL_DF		0x00000400	// Direction Flag
#define FL_OF		0x00000800	// Overflow Flag
#define FL_IOPL_MASK	0x00003000	// I/O Privilege Level bitmask
#define FL_IOPL_0	0x00000000	//   IOPL == 0
#define FL_IOPL_1	0x00001000	//   IOPL == 1
#define FL_IOPL_2	0x00002000	//   IOPL == 2
#define FL_IOPL_3	0x00003000	//   IOPL == 3
#define FL_NT		0x00004000	// Nested Task
#define FL_RF		0x00010000	// Resume Flag
#define FL_VM		0x00020000	// Virtual 8086 mode
#define FL_AC		0x00040000	// Alignment Check
#define FL_VIF		0x00080000	// Virtual Interrupt Flag
#define FL_VIP		0x00100000	// Virtual Interrupt Pending
#define FL_ID		0x00200000	// ID flag

// 页错误的宏定义
#define FEC_PR		0x1	// Page fault caused by protection violation
#define FEC_WR		0x2	// Page fault caused by a write
#define FEC_U		0x4	// Page fault occured while in user mode


/*
 *
 *	Part 2.  段相关的数据结构和算法
 *
 */

#ifdef __ASSEMBLER__

/*
 * 汇编代码：宏定义的GDT
 */
#define SEG_NULL						\
	.word 0, 0;						\
	.byte 0, 0, 0, 0
#define SEG(type,base,lim)					\
	.word (((lim) >> 12) & 0xffff), ((base) & 0xffff);	\
	.byte (((base) >> 16) & 0xff), (0x90 | (type)),		\
		(0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else	// not __ASSEMBLER__

#include <inc/types.h>

// Segment Descriptors
struct Segdesc {
	unsigned sd_lim_15_0 : 16;  // Low bits of segment limit
	unsigned sd_base_15_0 : 16; // Low bits of segment base address
	unsigned sd_base_23_16 : 8; // Middle bits of segment base address
	unsigned sd_type : 4;       // Segment type (see STS_ constants)
	unsigned sd_s : 1;          // 0 = system, 1 = application
	unsigned sd_dpl : 2;        // Descriptor Privilege Level
	unsigned sd_p : 1;          // Present
	unsigned sd_lim_19_16 : 4;  // High bits of segment limit
	unsigned sd_avl : 1;        // Unused (available for software use)
	unsigned sd_rsv1 : 1;       // Reserved
	unsigned sd_db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
	unsigned sd_g : 1;          // Granularity: limit scaled by 4K when set
	unsigned sd_base_31_24 : 8; // High bits of segment base address
};
// 空段
#define SEG_NULL	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
// 错误段：可加载，但是不能用，会出错
#define SEG_FAULT	{ 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0 }
// 正常段
#define SEG(type, base, lim, dpl) 					\
{ ((lim) >> 12) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,	\
    type, 1, dpl, 1, (unsigned) (lim) >> 28, 0, 0, 1, 1,		\
    (unsigned) (base) >> 24 }
#define SEG16(type, base, lim, dpl) (struct Segdesc)			\
{ (lim) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,		\
    type, 1, dpl, 1, (unsigned) (lim) >> 16, 0, 0, 1, 0,		\
    (unsigned) (base) >> 24 }

#endif /* !__ASSEMBLER__ */

// 段类型
#define STA_X		0x8	    // 可执行
#define STA_E		0x4	    // Expand down (non-executable segments)
#define STA_C		0x4	    // Conforming code segment (executable only)
#define STA_W		0x2	    // Writeable (non-executable segments)
#define STA_R		0x2	    // Readable (executable segments)
#define STA_A		0x1	    // Accessed

// System segment type bits
#define STS_T16A	0x1	    // Available 16-bit TSS
#define STS_LDT		0x2	    // Local Descriptor Table
#define STS_T16B	0x3	    // Busy 16-bit TSS
#define STS_CG16	0x4	    // 16-bit Call Gate
#define STS_TG		0x5	    // Task Gate / Coum Transmitions
#define STS_IG16	0x6	    // 16-bit Interrupt Gate
#define STS_TG16	0x7	    // 16-bit Trap Gate
#define STS_T32A	0x9	    // Available 32-bit TSS
#define STS_T32B	0xB	    // Busy 32-bit TSS
#define STS_CG32	0xC	    // 32-bit Call Gate
#define STS_IG32	0xE	    // 32-bit Interrupt Gate
#define STS_TG32	0xF	    // 32-bit Trap Gate


/*
 *
 *	Part 3.  中断
 *
 */

#ifndef __ASSEMBLER__

// Task state segment format (as described by the Pentium architecture book)
struct Taskstate {
	uint32_t ts_link;	// Old ts selector
	uintptr_t ts_esp0;	// Stack pointers and segment selectors
	uint16_t ts_ss0;	//   after an increase in privilege level
	uint16_t ts_padding1;
	uintptr_t ts_esp1;
	uint16_t ts_ss1;
	uint16_t ts_padding2;
	uintptr_t ts_esp2;
	uint16_t ts_ss2;
	uint16_t ts_padding3;
	physaddr_t ts_cr3;	// Page directory base
	uintptr_t ts_eip;	// Saved state from last task switch
	uint32_t ts_eflags;
	uint32_t ts_eax;	// More saved state (registers)
	uint32_t ts_ecx;
	uint32_t ts_edx;
	uint32_t ts_ebx;
	uintptr_t ts_esp;
	uintptr_t ts_ebp;
	uint32_t ts_esi;
	uint32_t ts_edi;
	uint16_t ts_es;		// Even more saved state (segment selectors)
	uint16_t ts_padding4;
	uint16_t ts_cs;
	uint16_t ts_padding5;
	uint16_t ts_ss;
	uint16_t ts_padding6;
	uint16_t ts_ds;
	uint16_t ts_padding7;
	uint16_t ts_fs;
	uint16_t ts_padding8;
	uint16_t ts_gs;
	uint16_t ts_padding9;
	uint16_t ts_ldt;
	uint16_t ts_padding10;
	uint16_t ts_t;		// Trap on task switch
	uint16_t ts_iomb;	// I/O map base address
};

// 中断门描述符：interrupts and traps的
struct Gatedesc {
	unsigned gd_off_15_0 : 16;   // low 16 bits of offset in segment
	unsigned gd_sel : 16;        // segment selector
	unsigned gd_args : 5;        // # args, 0 for interrupt/trap gates
	unsigned gd_rsv1 : 3;        // reserved(should be zero I guess)
	unsigned gd_type : 4;        // type(STS_{TG,IG32,TG32})
	unsigned gd_s : 1;           // must be 0 (system)
	unsigned gd_dpl : 2;         // descriptor(meaning new) privilege level
	unsigned gd_p : 1;           // Present
	unsigned gd_off_31_16 : 16;  // high bits of offset in segment
};

// 设置 interrupt/trap  门描述符.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
    //   see section 9.6.1.3 of the i386 reference: "The difference between
    //   an interrupt gate and a trap gate is in the effect on IF (the
    //   interrupt-enable flag). An interrupt that vectors through an
    //   interrupt gate resets IF, thereby preventing other interrupts from
    //   interfering with the current interrupt handler. A subsequent IRET
    //   instruction restores IF to the value in the EFLAGS image on the
    //   stack. An interrupt through a trap gate does not change IF."
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//	  the privilege level required for software to invoke
//	  this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, dpl)			\
{								\
	(gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;		\
	(gate).gd_sel = (sel);					\
	(gate).gd_args = 0;					\
	(gate).gd_rsv1 = 0;					\
	(gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;	\
	(gate).gd_s = 0;					\
	(gate).gd_dpl = (dpl);					\
	(gate).gd_p = 1;					\
	(gate).gd_off_31_16 = (uint32_t) (off) >> 16;		\
}

// Set up a call gate descriptor.
#define SETCALLGATE(gate, sel, off, dpl)           	        \
{								\
	(gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;		\
	(gate).gd_sel = (sel);					\
	(gate).gd_args = 0;					\
	(gate).gd_rsv1 = 0;					\
	(gate).gd_type = STS_CG32;				\
	(gate).gd_s = 0;					\
	(gate).gd_dpl = (dpl);					\
	(gate).gd_p = 1;					\
	(gate).gd_off_31_16 = (uint32_t) (off) >> 16;		\
}

// Pseudo-descriptors used for LGDT, LLDT and LIDT instructions.
struct Pseudodesc {
	uint16_t pd_lim;		// Limit
	uint32_t pd_base;		// Base address
} __attribute__ ((packed));

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_MMU_H */
```


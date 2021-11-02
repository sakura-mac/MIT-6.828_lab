# B 部分：页面错误、断点异常和系统调用

既然您的内核具有基本的异常处理功能，您将对其进行改进以提供依赖于异常处理的重要操作系统原语。

## 处理页面错误

页错误异常，中断向量 14 ( `T_PGFLT`)，是一个特别重要的异常，我们将在本实验和下一个实验中大量练习。当处理器发生页面错误时，它会将导致错误的线性（即虚拟）地址存储在特殊的处理器控制寄存器`CR2 中`。在`trap.c 中，` 我们提供了一个特殊函数的开头 `page_fault_handler()`，用于处理页面错误异常。

### Exercise5

修改`trap_dispatch()` 以将页面错误异常分派到`page_fault_handler()`. 您现在应该能够make grade 在`faultread`、`faultreadkernel`、 `faultwrite`和`faultwritekernel`测试中取得成功。如果其中任何一个不起作用，请找出原因并修复它们。请记住，您可以使用或 将JOS 引导到特定的用户程序中。例如，运行*hello*用户程序。 make run-*x*make run-*x*-noxmake run-hello-nox

当您实现系统调用时，您将在下面进一步细化内核的页面错误处理。

## 断点异常

断点异常，中断向量 3 ( `T_BRKPT`)，通常用于允许调试器通过用特殊的 1 字节`int3`软件中断指令临时替换相关程序指令来在程序代码中插入断点。

在 JOS 中，我们将稍微滥用此异常，将其转换为任何用户环境都可以用来调用 JOS 内核监视器的原始伪系统调用。

如果我们将 JOS 内核监视器视为原始调试器。例如，`panic()`在`lib/panic.c 中`的用户模式实现，在`int3`显示其panic消息后执行。

### Exercise6

修改`trap_dispatch()` 使断点异常调用内核监视器。您现在应该能够make grade 在`断点`测试中取得成功。

*挑战！* 修改 JOS 内核监视器，以便您可以从当前位置“继续”执行（例如，在 之后`int3`，如果内核监视器是通过断点异常调用的），并且您可以一次单步执行一条指令。您需要了解`EFLAGS`寄存器的某些位才能实现单步执行。

*可选：* 如果您真的喜欢冒险，可以找到一些 x86 反汇编器源代码 - 例如，从 QEMU 或 GNU binutils 中提取它，或者只是自己编写 - 并扩展 JOS 内核监视器以能够反汇编和在您逐步执行时显示说明。结合实验 1 中的符号表加载，这就是制作真正内核调试器的内容。

**问题**

1. 断点测试用例将生成断点异常或一般保护错误，具体取决于您如何在 IDT 中初始化断点条目（即，您对`SETGATE`from的调用 `trap_init`）。为什么？您需要如何设置它才能使断点异常按照上面指定的方式工作，哪些不正确的设置会导致它触发一般保护故障？
2. 您认为这些机制的重点是什么，特别是考虑到`user/softint`测试程序的作用？

## 系统调用

用户进程通过调用系统调用来要求内核为它们做事。当用户进程调用系统调用时，处理器进入内核模式，处理器和内核协同保存用户进程的状态，内核执行适当的代码以进行系统调用，然后恢复用户进程。用户进程如何引起内核的注意以及它如何指定要执行的调用的确切细节因系统而异。

在 JOS 内核中，我们将使用`int` 导致处理器中断的指令。特别是，我们将`int $0x30` 用作系统调用中断。我们已经`T_SYSCALL`为您定义了常量 为 48 (0x30)。您必须设置中断描述符以允许用户进程引起该中断。注意，中断 0x30 不能由硬件产生，所以不会因为允许用户代码产生而造成歧义。

应用程序将在寄存器中传递系统调用号和系统调用参数。这样，内核就不需要在用户环境的堆栈或指令流中四处游荡。系统调用号会去`%eax`，和参数（其中最多5个）将去`%edx`， `%ecx`，`%ebx`，`%edi`，和`%esi`分别。内核将返回值传回`%eax`. 调用系统调用的汇编代码已为您编写， `syscall()`在`lib/syscall.c 中`。你应该通读它并确保你理解发生了什么。

### Exercise7

在内核中为中断向量添加一个处理程序`T_SYSCALL`。您必须编辑`kern/trapentry.S`和 `kern/trap.c`的`trap_init()`. 您还需要更改`trap_dispatch()`为通过使用适当的参数调用`syscall()` （在`kern/syscall.c 中定义`）来处理系统调用中断，然后安排将返回值传递回`%eax`. 最后，您需要`syscall()`在 `kern/syscall.c 中实现`。 如果系统调用号无效，请确保`syscall()`返回`-E_INVAL`。你应该阅读并理解`lib/syscall.c` （尤其是内联汇编例程）以确认您对系统调用接口的理解。通过为每个调用调用相应的内核函数来处理`inc/syscall.h 中`列出的所有系统调用。

在您的内核 ( )下 运行`user/hello`程序make run-hello。它应该在控制台上打印“ `hello, world` ”，然后在用户模式下导致页面错误。如果这没有发生，则可能意味着您的系统调用处理程序不太正确。您现在也应该能够make grade 在`testbss`测试中取得成功。

*挑战！* 使用`sysenter`and `sysexit`指令而不是使用`int 0x30`and来 实现系统调用`iret`。

这些`sysenter/sysexit`指令是由英特尔设计的，比`int/iret`. 他们通过使用寄存器而不是堆栈并假设分段寄存器的使用方式来做到这一点。这些指令的确切细节可以在英特尔参考手册的第 2B 卷中找到。

在 JOS 中添加对这些指令的支持的最简单方法是`sysenter_handler`在`kern/trapentry.S`中添加一个， 它保存了有关用户环境的足够信息以返回给它，设置内核环境，将参数推送到 `syscall()`并`syscall()` 直接调用。一旦`syscall()`返回时，设置好一切并执行`sysexit`指令。您还需要将代码添加到`kern/init.c`以设置必要的模型特定寄存器 (MSR)。AMD 架构程序员手册第 2 卷中的第 6.1.2 节和英特尔参考手册第 2B 卷中关于 SYSENTER 的参考对相关 MSR 进行了很好的描述。您可以找到`wrmsr`添加到 `inc/x86.h 的实现`在[这里](http://ftp.kh.edu.tw/Linux/SuSE/people/garloff/linux/k6mod.c)写信给这些 MSR 。

最后，必须更改`lib/syscall.c`以支持使用`sysenter`. 以下是该`sysenter` 指令可能的寄存器布局：

```
	eax - 系统调用号
	edx、ecx、ebx、edi - arg1、arg2、arg3、arg4
	esi - 返回电脑
	ebp - 返回 esp
	esp - 被 sysenter 删除
	
```

GCC 的内联汇编器将自动保存您告诉它直接加载值的寄存器。不要忘记保存（推送）和恢复（弹出）您破坏的其他寄存器，或者告诉内联汇编器您正在破坏它们。内联汇编器不支持保存 `%ebp`，因此您需要添加代码来自己保存和恢复它。返回地址可以`%esi`通过使用类似的指令放入`leal after_sysenter_label, %%esi`。

请注意，这仅支持 4 个参数，因此您需要保留执行系统调用的旧方法以支持 5 个参数系统调用。此外，由于此快速路径不会更新当前环境的陷阱帧，因此它不适用于我们在后续实验中添加的某些系统调用。

一旦我们在下一个实验中启用异步中断，您可能需要重新访问您的代码。具体来说，您需要在返回用户进程时启用中断，这 `sysexit`对您不起作用。

## 用户模式启动

用户程序开始运行在`lib/entry.S`的顶部 。经过一些设置后，此代码`libmain()`在`lib/libmain.c 中`调用, 。您应该修改`libmain()`以初始化全局指针 `thisenv`以指向数组`struct Env`中的此环境 `envs[]`。（请注意，`lib/entry.S`已经定义`envs` 为指向`UENVS`您在 A 部分中设置的映射。）提示：查看`inc/env.h`并使用 `sys_getenvid`.

`libmain()`然后调用`umain`，在 hello 程序的情况下，它在 `user/hello.c 中`。请注意，在打印“ `hello, world` ”后，它会尝试访问 `thisenv->env_id`. 这就是它较早出错的原因。现在您已经`thisenv`正确初始化，它应该不会出错。如果它仍然有问题，您可能没有映射`UENVS`用户可读的 区域（回到`pmap.c 的`A 部分 ；这是我们第一次实际使用该`UENVS`区域）。

### Exercise8

将所需的代码添加到用户库中，然后启动内核。您应该看到`user/hello` 打印“ `hello, world` ”，然后打印“`我是环境 00001000` ”。 `user/hello`然后尝试通过调用“退出” `sys_env_destroy()` （参见`lib/libmain.c`和`lib/exit.c`）。由于内核目前只支持一个用户环境，它应该报告它已经破坏了唯一的环境，然后进入内核监视器。您应该能够make grade 在`hello`测试中取得成功。

## 页面错误和内存保护

内存保护是操作系统的一项重要功能，可确保一个程序中的错误不会损坏其他程序或损坏操作系统本身。

操作系统通常依靠硬件支持来实现内存保护。操作系统让硬件知道哪些虚拟地址有效，哪些无效。当程序试图访问无效地址或没有权限访问的地址时，处理器会在导致错误的指令处停止程序，然后将有关尝试操作的信息捕获到内核中。如果故障是可修复的，内核可以修复它并让程序继续运行。如果故障不可修复，则程序无法继续，因为它永远不会越过导致故障的指令。

作为可修复故障的示例，请考虑自动扩展堆栈。在许多系统中，内核最初分配单个堆栈页面，然后如果程序在访问堆栈下方的页面时出错，内核将自动分配这些页面并让程序继续运行。通过这样做，内核只分配程序所需的堆栈内存，但程序可以在拥有任意大堆栈的假象下工作。

系统调用为内存保护提出了一个有趣的问题。大多数系统调用接口允许用户程序将指针传递给内核。这些指针指向要读取或写入的用户缓冲区。然后内核在执行系统调用时取消引用这些指针。这有两个问题：

1. 内核中的页面错误可能比用户程序中的页面错误严重得多。如果内核在操作自己的数据结构时出现页面错误，那就是内核错误，并且错误处理程序应该使内核（以及整个系统）panic。但是当内核取消引用用户程序给它的指针时，它需要一种方法来记住这些取消引用导致的任何页面错误实际上代表用户程序。
2. 内核通常比用户程序具有更多的内存权限。用户程序可能会传递一个指向系统调用的指针，该指针指向内核可以读取或写入但程序不能读取的内存。内核必须小心不要被欺骗取消引用这样的指针，因为这可能会泄露私有信息或破坏内核的完整性。

由于这两个原因，内核在处理用户程序提供的指针时必须非常小心。

您现在将使用一种机制来解决这两个问题，该机制会检查从用户空间传递到内核的所有指针。当程序向内核传递一个指针时，内核将检查地址是否在地址空间的用户部分，以及页表是否允许内存操作。

因此，内核永远不会因为取消引用用户提供的指针而遭受页面错误。如果内核发生页面错误，它应该panic

### Exercise9

更改`kern/trap.c`恐慌，如果页面错误在内核模式下发生的。

提示：判断故障是发生在用户态还是内核态，检查`tf_cs`.

读`user_mem_assert`入`kern/pmap.c` 并`user_mem_check`在同一个文件中实现。

将`kern/syscall.c`更改为系统调用的完整性检查参数。

启动内核，运行`user/buggyhello`。环境应予以销毁，内核应该*不会*恐慌。你应该看到：

```
	[00001000] va 00000001 的 user_mem_check 断言失败
	[00001000] 免费环境 00001000
	破坏了唯一的环境——无事可做！
	
```

最后，改变`debuginfo_eip`在 `克恩/ kdebug.c`调用`user_mem_check`上 `usd`，`stabs`和 `stabstr`。如果您现在运行 `user/breakpoint`，您应该能够backtrace从内核监视器运行 并在内核因页面错误而恐慌之前看到回溯遍历到`lib/libmain.c`。是什么导致了这个页面错误？您不需要修复它，但您应该了解它为什么会发生。

请注意，您刚刚实现的相同机制也适用于恶意用户应用程序（例如`user/evilhello`）。

### Exercise10

启动内核，运行`user/evilhello`。环境要破坏，内核不要慌。你应该看到：

```
	[00000000] 新环境 00001000
	...
	[00001000] va f010000c 的 user_mem_check 断言失败
	[00001000] 免费环境 00001000
	
```

## 回答问题汇总

### Exercise5

修改`trap_dispatch()` 以将页面错误异常分派到`page_fault_handler()`

```
if (tf->tf_trapno == T_PGFLT) {//缺页中断
		page_fault_handler(tf);
		return;
	}
```

### Exercise6

`trap_dispatch()` 添加中断向量 3 ( `T_BRKPT`)的处理，让用户代码能够调用监视器

```
if (tf->tf_trapno == T_BRKPT) {
		monitor(tf);
		return;
	}
```

1. 断点测试用例将生成断点异常或一般保护错误，具体取决于您如何在 IDT 中初始化断点条目（即，您对SETGATE设置）。为什么？您需要如何设置它才能使断点异常按照上面指定的方式工作，哪些不正确的设置会导致它触发一般保护故障？

DPL必须设置成3.因为在中断判断时，必须判断RPL，CPL是优先级高于等于这个门描述符DPL。所以如果int3设置DPL为特权级0，那么就会出发故障。

更一般地说，CPL会更改来进入/退出内核态，RPL不会更改，所以当我们判断DPL时，DPL高特权就保证了用户不能访问内核代码和数据（压栈是内核里的中断代码指令，如果保护异常就会提前从中断返回）。因此本lab中只有int3的trap和syscallDPL设置为3，让用户能访问。

2. ·你觉得中断机制哪部分最重要，考虑到`user/softint`测试程序的作用？

特权检查和保护现场

### Exercise7

`int $0x30` 用作系统调用中断。我们已经`T_SYSCALL`为您定义了常量 为 48 (0x30)

```
TRAPHANDLER_NOEC(th_syscall, T_SYSCALL)//trapentry.S的中断预处理
SETGATE(idt[T_SYSCALL], 0, GD_KT, th_syscall, 3);//trap.c的trap_init()的IDT向量设置

if (tf->tf_trapno == T_SYSCALL) { //trap.c的trap_dispatch()用来跳转到syscall系统调用
		tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx,
			tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
		return;
	}
```

syscall()：就是系统调用跳转表的简单实现

```
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	int32_t ret;
	switch (syscallno) {    //eax保存的系统调用号
		case SYS_cputs:
			sys_cputs((char *)a1, (size_t)a2);
			ret = 0;
			break;
		case SYS_cgetc:
			ret = sys_cgetc();
			break;
		case SYS_getenvid:
			ret = sys_getenvid();
			break;
		case SYS_env_destroy:
			ret = sys_env_destroy((envid_t)a1);
			break;
		default:
			return -E_INVAL;
	}

	return ret;
}
```



### Exercise8

`libmain()`以初始化全局指针 `thisenv`以指向数组`struct Env`中的此环境 `envs[]`

```
void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
	envid_t envid = sys_getenvid();    //系统调用，最终会被syscall处理
	thisenv = envs + ENVX(envid);      //获取Env结构指针

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}
```

user/hello.c应该thisenv就有效了

```
void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);  
}
```



### Exercise9

更改`kern/trap.c/page_fault_handler`处理缺页中断:如果页面错误在内核模式下发生的应该panic

```
if ((tf->tf_cs & 3) == 0) 
		panic("page_fault_handler():page fault in kernel mode!\n");
```

然后按照要求补全一些细节：



kern/pmap.c/user_mem_check()进行检查

```
int
user_mem_check(struct Env *env, const void *va, size_t len, int perm)
{
	// LAB 3: Your code here.
	cprintf("user_mem_check va: %x, len: %x\n", va, len);
	uint32_t start = (uint32_t) ROUNDDOWN(va, PGSIZE); 
	uint32_t end = (uint32_t) ROUNDUP(va+len, PGSIZE);
	for (uint32_t i = start; i < end; i += PGSIZE) {
		pte_t *pte = pgdir_walk(env->env_pgdir, (void*)i, 0);
		if ((i >= ULIM) || !pte || !(*pte & PTE_P) || ((*pte & perm) != perm)) {        
			user_mem_check_addr = (i < (uint32_t)va ? (uint32_t)va : i);                
			return -E_FAULT;
		}
	}
	cprintf("user_mem_check success va: %x, len: %x\n", va, len);
	return 0;
}
```

kern/syscall.c进行我们的sys_cputs的检查curenv指针指向的函数调用

```
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, 0);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}
```

### Exercise10

实际上细节补充完毕后实验10也完成了

![](https://tva1.sinaimg.cn/large/008i3skNly1gw126btkwij30ie0jamzw.jpg)

总结一下我们的中断处理函数

```
1.trap_init初始化中断向量表

2.handler宏的预处理和_alltraps处理负责中断的保护现场：trapframe压栈，然后call trap

3.trap调用trap_dispatch
trap_dispatch负责中断向量跳转，实验只实现了syscall，缺页，trap,特权保护的跳转或直接处理

4.syscall负责调用内核函数，实验采用switch只实现了几个内核函数的call

handler->_alltraps->trap->trap_dispatch->syscall->sys_cputs->cprintf
																			 ->monitor
																			 ->page_fault_handler
```

总结一下整个流程

```
系统调用->（实际上进入了内核）中断处理->调用内核函数->调用后ret返回trap函数->env_run返回用户环境

一些问题
1.为什么有lib/syscall和kern/syscall分别？
因为是系统调用和中断处理跳转内核函数的区别

2.为什么压栈保护现场要分两次？
因为第一次压栈有错误码的分别，第二次cs，ds段寄存器和通用寄存器，节省代码量，并且通用寄存器这样的部分trapframe还没存储

3.为什么trap需要保存栈的trapframe到env的trapframe结构？已经有栈不行吗？
因为当fork时候也同样要从中断结尾处返回，因此设计了env_run，在fork后和trap结尾都被调用，通过env的trapframe“返回”用户态，从而保证了合理。对于栈的trapframe就忽略，也是跟函数返回的不同。

4.什么是CPL，RPL，DPL？
RPL是选择子的末2位，CPL就是cs寄存器（存放代码段选择子）的末两位，所以后者是前者的特例。并且当我们从用户态进入内核RPL认为是调用门的特权级，会被区分。DPL是描述符的特权级。我们要求max{CPL,RPL}和DPL比较。举个例子，当我们中断处理时，中断是内核的函数，CPL=0，但是RPL=3，所以我们中断判断特权才要让int和syscall保持DPL为3，否则会就会产生保护出错，exercise6问题就是问这个的。

5.CPL和DPL和PTE_X有什么区别？不都是特权保护？
对，但是还是有不同。由于我们跟linux一样，JOS放弃了分段管理转向了分页管理。但是硬件强行要求分段。所以段选择子，段描述符里的特权保留，但是描述符里的地址（16位）全部都是0.因此当我们必须规定在分页管理下页的访问权限（PTE_U这样），至于CPL，RPL，DPL权限检查，除了在中断时候有用（因为没访问页所以可以用得上），其他情况下设置不应该影响分页的检查才行。具体来说，16位的访问段大小实在是太小了，所以除了访问GDT，IDT的响应检查等以外。其他的情况下比如guard page，对于用户的特权级就是通过，但是PTE检查就是失败，无论是内核还是用户都会经历这样的过程。从而合理。

6.用户访问内存为什么会中断？
并不是读写内存就会中断，大部分情况下是你使用系统调用才会中断，你对data section，栈，堆读写都不会中断。甚至如果你绕过对文件的读写的系统调用，使用mmap直接映射到用户空间，那么你的读写也不会有系统调用，也就不会中断了。
不要弄混特权检查和中断，是中断需要对特权检查来保证用户进入内核后的约束，而特权检查是访问内存的必经之路。
```


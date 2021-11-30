# C 部分：抢占式多任务和进程间通信 (IPC)

在实验 4 的最后部分，您将修改内核以抢占不合作的环境并允许环境明确地相互传递消息。

## 时钟中断和抢占

运行`用户/自旋`测试程序。这个测试程序分叉出一个子环境，一旦它获得 CPU 的控制权，它就会在一个紧密的循环中永远旋转。父环境和内核都不会重新获得 CPU。就保护系统免受用户模式环境中的错误或恶意代码的影响而言，这显然不是理想的情况，因为任何用户模式环境都可以通过进入无限循环而使整个系统停止，并且永不回馈中央处理器。为了让内核*抢占*运行环境，强行夺回对 CPU 的控制，我们必须扩展 JOS 内核以支持来自时钟硬件的外部硬件中断。

## 中断

外部中断（即设备中断）称为 IRQ。有 16 个可能的 IRQ，编号为 0 到 15。从 IRQ 编号到 IDT 条目的映射不是固定的。 `pic_init`在`picirq.c`映射的IRQ 0-15到IDT入口`IRQ_OFFSET`通过`IRQ_OFFSET+15`。

在`inc/trap.h 中`， `IRQ_OFFSET`定义为十进制 32。因此，IDT 条目 32-47 对应于 IRQ 0-15。例如，时钟中断是IRQ 0。因此，IDT[IRQ_OFFSET+0]（即IDT[32]）包含内核中时钟中断处理程序的地址。`IRQ_OFFSET`选择此选项是为了使设备中断不会与处理器异常重叠，这显然会导致混淆。（事实上，在运行MS-DOS的PC的初期，`IRQ_OFFSET`有效地*是*零，这的确造成处理硬件中断和处理处理器异常之间巨大的混乱！）

在 JOS 中，与 xv6 Unix 相比，我们做了一个关键的简化。外部设备中断在内核中*总是被*禁用（并且像 xv6 一样，在用户空间中启用）。外部中断由寄存器的`FL_IF`标志位控制`%eflags`（参见`inc/mmu.h`）。当该位被设置时，外部中断被使能。虽然可以通过多种方式修改该位，但由于我们的简化，我们将仅通过在`%eflags`进入和离开用户模式时保存和恢复寄存器的过程来处理它。

您必须确保`FL_IF`在用户环境运行时在用户环境中设置该标志，以便在中断到达时将其传递给处理器并由您的中断代码处理。否则，中断将*被屏蔽*或忽略，直到重新启用中断。我们用引导加载程序的第一条指令屏蔽了中断，到目前为止我们还没有考虑重新启用它们。

## Exercise13

修改`kern/ trapentry.S`和`kern/trap.c`初始化所述IDT中的相应条目，并通过15提供为IRQs 0处理程序然后修改代码中的`env_alloc()`在`kern/ env.c`，以确保用户环境总是在启用中断的情况下运行。

还要取消注释`sched_halt() 中`的`sti`指令，以便空闲 CPU 取消屏蔽中断。``

处理器在调用硬件中断处理程序时从不推送错误代码。此时，您可能需要重新阅读[ 80386 参考手册的](https://pdos.csail.mit.edu/6.828/2017/readings/i386/toc.htm)第 9.2 节 或[ IA-32 英特尔架构软件开发人员手册第 3 卷的](https://pdos.csail.mit.edu/6.828/2017/readings/ia32/IA32-3A.pdf)第 5.8 节 。

做完这个练习后，如果你用任何运行时间很长（例如，`spin`）的测试程序运行你的内核，你应该看到内核打印硬件中断的陷阱帧。虽然现在处理器中启用了中断，但 JOS 尚未处理它们，因此您应该看到它错误地将每个中断分配给当前运行的用户环境并销毁它。最终它应该耗尽环境来销毁并放入监视器。

## 处理时钟中断

在`user/spin`程序中，子环境第一次运行后，它只是在一个循环中旋转，内核再也没有获得控制权。我们需要对硬件进行编程以定期生成时钟中断，这将强制控制回到内核，在那里我们可以将控制切换到不同的用户环境。

到通话`lapic_init`和`pic_init` （来自`i386_init`于`INIT.C`），这是我们为你而写，设时钟和中断控制器产生中断。您现在需要编写代码来处理这些中断。

## Exericse14

修改内核`trap_dispatch()`函数，使其`sched_yield()` 在发生时钟中断时调用查找并运行不同的环境。

您现在应该能够让`用户/自旋`测试工作：父环境应该将子环境分叉 `sys_yield()`几次，但在每种情况下都在一个时间片后重新获得对 CPU 的控制，最后杀死子环境并优雅地终止。

这是进行一些*回归测试*的好时机。确保您没有通过启用中断来破坏该实验室以前可以工作的任何部分（例如 `forktree`）。此外，尝试使用. 你现在也应该能够通过`stresssched`。跑过去看看。您现在应该在本实验中获得 65/80 分的总分。 

## 进程间通信 (IPC)

（从技术上讲，在 JOS 中，这是“环境间通信”或“IEC”，但其他人都称其为 IPC，因此我们将使用标准术语。）

我们一直专注于操作系统的隔离方面，它提供了每个程序都拥有一台机器的错觉的方式。操作系统的另一个重要服务是允许程序在**需要时相互通信**。让程序与其他程序交互是非常强大的。Unix 管道模型是典型的例子。

进程间通信有很多模型。即使在今天，仍然存在关于哪种模型最好的争论。我们不会参与那场辩论。相反，我们将实现一个简单的 IPC 机制，然后尝试一下。

## JOS中的IPC

您将实现一些额外的 JOS 内核系统调用，它们共同提供了一个简单的进程间通信机制。您将实现两个系统调用，`sys_ipc_recv`以及 `sys_ipc_try_send`. 然后，您将实现两个库包装器 `ipc_recv`和`ipc_send`.

用户环境可以使用 JOS 的 IPC 机制相互发送的“消息”由两个组件组成：单个 32 位值和可选的单个页面映射。允许环境在消息中传递页面映射提供了一种传输比单个 32 位整数所能容纳的更多数据的有效方法，并且还允许环境轻松设置共享内存安排。

## 发送和接收消息

要接收消息，环境调用 `sys_ipc_recv`. 这个系统调用取消了当前环境的调度，并且在收到消息之前不会再次运行它。当一个环境正在等待接收消息时， *任何*其他环境都可以向它发送消息 - 不仅仅是特定环境，也不仅仅是与接收环境有父/子安排的环境。换句话说，您在 A 部分中实施的权限检查不适用于 IPC，因为 IPC 系统调用经过精心设计以确保“安全”：一个环境不能仅通过向其发送消息而导致另一个环境发生故障（除非目标环境也有问题）。

为了尝试发送一个值，环境调用 `sys_ipc_try_send`，参数是接收者的环境 ID 和要发送的值。如果目标环境实际上正在等待接收（它已调用 `sys_ipc_recv`但尚未获得值），则我们可以发送消息并返回 0。否则发送返回`-E_IPC_NOT_RECV`以指示目标环境当前不期望接收值。

`ipc_recv`用户空间中的 库函数将负责调用`sys_ipc_recv`，然后在当前环境的`struct Env`.

同样，库函数`ipc_send`将负责重复调用，`sys_ipc_try_send` 直到发送成功。

## 传输页面

当环境`sys_ipc_recv` 使用有效`dstva`参数（如下`UTOP`）调用时，环境表示它愿意接收页面映射。如果发送方发送一个页面，那么该页面应该被映射`dstva` 到接收方的地址空间中。如果接收方已经在 处映射了一个页面`dstva`，则该前一个页面将被取消映射。

当环境`sys_ipc_try_send` 使用有效`srcva`（如下`UTOP`）调用时，这意味着发送方希望将当前映射`srcva`到的页面发送给接收方，并具有权限`perm`。IPC 成功后，发送方`srcva`在其地址空间中保留其对 at 页面的原始映射，但接收方也在`dstva`接收方地址空间中的接收方最初指定的同一物理页面上获得了该物理页面的映射。因此，此页面在发送方和接收方之间共享。

如果发送方或接收方未指示应传送页面，则不传送页面。**在任何 IPC 之后**，内核将`env_ipc_perm` 接收者`Env`结构中的新字段**设置为接收到的页面的权限**，如果没有接收到页面，则为零。

## 实现 IPC

## Exercise15

在`kern/syscall.c 中` 实现`sys_ipc_recv`和`sys_ipc_try_send` 。在实施之前阅读两者的注释，因为它们必须一起工作。当您调用这些例程时，您应 该将标志设置为 0，这意味着任何环境都可以向任何其他环境发送 IPC 消息，并且内核除了验证目标环境是否有效之外，不会进行特殊的权限检查。 `sys_ipc_try_send````envid2env``checkperm`

然后在`lib/ipc.c 中`实现`ipc_recv`和`ipc_send`函数。``

使用`user/pingpong`和`user/primes` 函数来测试您的 IPC 机制。 `user/primes` 将为每个素数生成一个新环境，直到 JOS 用完环境。您可能会发现阅读`user/primes.c`以查看幕后发生的所有fork和 IPC很有趣。



*挑战！* 为什么`ipc_send` 一定要循环？更改系统调用接口，使其不必更改。确保您可以处理尝试同时发送到一个环境的多个环境。

*挑战！* 主要筛选只是大量并发程序之间消息传递的一种巧妙用法。阅读 CAR Hoare, ``Communicating Sequential Processes', *Communications of the ACM* 21(8) (August 1978), 666-667，并实现矩阵乘法示例。

*挑战！* 消息传递威力最令人印象深刻的例子之一是 Doug McIlroy 的幂级数计算器，在[M. Douglas McIlroy, ``Squinting at Power Series,'' *Software--Practice and Experience* , 20(7)（1990 年 7 月）中有所](https://swtch.com/~rsc/thread/squint.pdf)描述 [, 661-683](https://swtch.com/~rsc/thread/squint.pdf)。实现他的幂级数计算器并计算*sin* ( *x* + *x* ^3 )的幂级数 。

*挑战！* 通过应用 Liedtke 论文中的一些技术，[通过内核设计改进 IPC](http://dl.acm.org/citation.cfm?id=168633)或您可能想到的任何其他技巧，使 JOS 的 IPC 机制更加高效 。随意为此目的修改内核的系统调用 API，只要您的代码向后兼容我们的评分脚本所期望的。

# 回答问题汇总

我们现在需要实行抢占式调度，并且实现具有权限检查的进程通信

## Exercise13

修改`kern/ trapentry.S`和`kern/trap.c`初始化所述IDT中的相应条目，并通过15提供为IRQs 0处理程序

然后修改代码中的`env_alloc()`在`kern/ env.c`，以确保用户环境总是在启用中断的情况下运行。

不要忘了还要取消注释`sched_halt() 中`的`sti`指令

kern/ trapentry.S

```
	TRAPHANDLER_NOEC(th32, IRQ_OFFSET)
	TRAPHANDLER_NOEC(th33, IRQ_OFFSET + 1)
	TRAPHANDLER_NOEC(th34, IRQ_OFFSET + 2)
	TRAPHANDLER_NOEC(th35, IRQ_OFFSET + 3)
	TRAPHANDLER_NOEC(th36, IRQ_OFFSET + 4)
	TRAPHANDLER_NOEC(th37, IRQ_OFFSET + 5)
	TRAPHANDLER_NOEC(th38, IRQ_OFFSET + 6)
	TRAPHANDLER_NOEC(th39, IRQ_OFFSET + 7)
	TRAPHANDLER_NOEC(th40, IRQ_OFFSET + 8)
	TRAPHANDLER_NOEC(th41, IRQ_OFFSET + 9)
	TRAPHANDLER_NOEC(th42, IRQ_OFFSET + 10)
	TRAPHANDLER_NOEC(th43, IRQ_OFFSET + 11)
	TRAPHANDLER_NOEC(th44, IRQ_OFFSET + 12)
	TRAPHANDLER_NOEC(th45, IRQ_OFFSET + 13)
	TRAPHANDLER_NOEC(th46, IRQ_OFFSET + 14)
	TRAPHANDLER_NOEC(th47, IRQ_OFFSET + 15)
```

kern/trap.c

```
trap_init function:添加IDT的支持

	SETGATE(idt[IRQ_OFFSET], 0, GD_KT, th32, 0);
	SETGATE(idt[IRQ_OFFSET + 1], 0, GD_KT, th33, 0);
	SETGATE(idt[IRQ_OFFSET + 2], 0, GD_KT, th34, 0);
	SETGATE(idt[IRQ_OFFSET + 3], 0, GD_KT, th35, 0);
	SETGATE(idt[IRQ_OFFSET + 4], 0, GD_KT, th36, 0);
	SETGATE(idt[IRQ_OFFSET + 5], 0, GD_KT, th37, 0);
	SETGATE(idt[IRQ_OFFSET + 6], 0, GD_KT, th38, 0);
	SETGATE(idt[IRQ_OFFSET + 7], 0, GD_KT, th39, 0);
	SETGATE(idt[IRQ_OFFSET + 8], 0, GD_KT, th40, 0);
	SETGATE(idt[IRQ_OFFSET + 9], 0, GD_KT, th41, 0);
	SETGATE(idt[IRQ_OFFSET + 10], 0, GD_KT, th42, 0);
	SETGATE(idt[IRQ_OFFSET + 11], 0, GD_KT, th43, 0);
	SETGATE(idt[IRQ_OFFSET + 12], 0, GD_KT, th44, 0);
	SETGATE(idt[IRQ_OFFSET + 13], 0, GD_KT, th45, 0);
	SETGATE(idt[IRQ_OFFSET + 14], 0, GD_KT, th46, 0);
	SETGATE(idt[IRQ_OFFSET + 15], 0, GD_KT, th47, 0);
```





env_alloc:中断是IF flag，所以我们添加即可

```
e->env_tf.tf_eflags |= FL_IF;
```



## Exercise14

trap_dispatch

```
if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		sched_yield();
		return;
	}
```



## Exercise 15

我们的进程通信只是进程之间的mapping page而已，不过比较复杂的是权限检查。

简单来说如何IPC？

1. 当环境调用sys_ipc_recv()后，该环境会阻塞（ENV_NOT_RUNNABLE），直到接受消息。并且sys_ipc_recv()传入dstva参数时，代表请求接受。
2. 环境调用sys_ipc_try_send()向目标环境发送“消息”，根据请求消息的参数决定发送，然后返回0，否则返回-E_IPC_NOT_RECV。当传入srcva参数时，将共享srcva映射的物理页。

问题：为什么要阻塞？

A：假设环境运行， 或许对应的页面进行新映射，而另一个环境毫不知情，所以必须要有一个signal来通知。而这个阻塞能够完成“我等你”的通知，但是并没有保证“你”是谁，所以存在抢占式的共享。并不是锁，更像是信号机制：”等待某个有缘人“

实现sys_ipc_recv()和sys_ipc_try_send()。包装函数ipc_recv()和 ipc_send()。

细节繁多，请注意查看lab给的注释和lab前面的讲解，否则实验debug会没有头绪。

```
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
		// LAB 4: Your code here.
		int r;
  	struct Env * dstenv;
  	pte_t * pte;
  	struct PageInfo *pp;
  	//err begin:
  	r = envid2env(envid, &dstenv, 0);
  	if (r < 0) return -E_BAD_ENV;		//1.env not exit
 	if (!dstenv->env_ipc_recving)return -E_IPC_NOT_RECV;		//2.target env not blocked:

  		//if srcva < UTOP,what err will exit?
  	if ((uint32_t)srcva < UTOP) {
  		pp = page_lookup(curenv->env_pgdir, srcva, &pte);//find pp and pte
  		
    		if ((uint32_t)srcva & 0xfff)return  -E_INVAL;		//3.not page-aligned
    		int flag = PTE_U | PTE_P;
    		if (flag != (flag & perm)) return -E_INVAL;		//4.perm not appropriate

    		if (!pp) return -E_INVAL;		//5.srcva not mapped the same ppage

    		if ((perm & PTE_W) && !(*pte & PTE_W)) return -E_INVAL;		//6.srcva read-only

      			r = page_insert(dstenv->env_pgdir, pp, dstenv->env_ipc_dstva, perm);
      			if (r < 0) return -E_NO_MEM;		//7.not enough memory for new page table

      			dstenv->env_ipc_perm = perm;
  	}

  	//succeed: update
  	dstenv->env_ipc_recving = 0;
  	dstenv->env_ipc_value = value;
  	dstenv->env_ipc_from = curenv->env_id;
  	dstenv->env_status = ENV_RUNNABLE;
	dstenv->env_tf.tf_regs.reg_eax = 0;
  	return 0;
}

static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if (dstva < (void *)UTOP && dstva != ROUNDDOWN(dstva, PGSIZE)) {
		return -E_INVAL;
	}
	curenv->env_ipc_recving = 1;
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_ipc_dstva = dstva;
	sys_yield();
	return 0;
}
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	if (pg == NULL) {
		pg = (void *)-1;
	}
	int r = sys_ipc_recv(pg);
	if (r < 0) {				//err
		if (from_env_store) *from_env_store = 0;
		if (perm_store) *perm_store = 0;
		return r;
	}
	if (from_env_store)
		*from_env_store = thisenv->env_ipc_from;
	if (perm_store)
		*perm_store = thisenv->env_ipc_perm;
	return thisenv->env_ipc_value;
}

void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	if (pg == NULL) {
		pg = (void *)-1;
	}
	int r;
	while(1) {
		r = sys_ipc_try_send(to_env, val, pg, perm);
		if (r == 0) {		//sucess
			return;
		} else if (r == -E_IPC_NOT_RECV) {	//target env not blocked
			sys_yield();
		} else {			//other err
			panic("ipc_send: sys_ipc_try_send failed\n");
		}
		
	}
}
```

happy!

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/截屏2021-11-24 上午12.58.16.png)
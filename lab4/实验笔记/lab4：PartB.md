# B 部分：写时复制fork

如前所述，Unix 提供`fork()`系统调用作为其主要的进程创建原语。该`fork()`系统调用将调用进程的地址空间（父）创建一个子进程。

xv6 Unix`fork()`通过将父页面中的所有数据复制到为子分配的新页面来实现。这基本上与所采用的方法相同`dumbfork()`。将父级地址空间复制到子级是该操作中最昂贵的部分`fork()`。

然而，在子进程中，`fork()` 调用 to 后几乎立即紧随其后`exec()`，这会用新程序替换子进程的内存。例如，这就是 shell 通常所做的。在这种情况下，复制父进程的地址空间所花费的时间在很大程度上被浪费了，因为子进程在调用`exec()`.

出于这个原因，后来的 Unix 版本利用虚拟内存硬件来允许父 进程和子进程*共享*映射到各自地址空间的内存，直到其中一个进程实际修改它。这种技术称为*写时复制*。为此，在`fork()`内核上将复制地址空间*映射* 从父到子而不是映射页面的内容，同时将现在共享的页面标记为只读。当两个进程之一尝试写入这些共享页面之一时，该进程会出现页面错误。此时，Unix 内核意识到该页面实际上是一个“虚拟”或“写时复制”副本，因此它为故障进程创建了一个新的、私有的、可写的页面副本。这样，单个页面的内容在实际写入之前不会被实际复制。这种优化使得子进程中的`fork()`后跟 `exec()`便宜得多：子进程在调用`exec()`之前可能只需要复制一页（页表）。

在本实验的下一部分中，您将实现一个“正确的”类 Unix，`fork()`并带有写时复制，作为用户空间库例程。`fork()`在用户空间中实现和写时复制支持的好处是内核保持简单得多，因此更有可能是正确的。它还允许各个用户模式程序为`fork()`. 一个想要稍微不同的实现的程序（例如，像 那样昂贵的始终复制版本`dumbfork()`，或者之后父子实际共享内存的程序）可以很容易地提供自己的实现。

## 用户级页面错误处理

用户级写时复制`fork()`需要了解写保护页面上的页面错误，因此这是您首先要实现的。写时复制只是用户级页面错误处理的众多可能用途之一。

设置地址空间是很常见的，以便页面错误指示何时需要执行某些操作。例如，大多数 Unix 内核最初只映射新进程堆栈区域中的单个页面，然后随着进程的堆栈消耗增加并在尚未映射的堆栈地址上导致页面错误，“按需”分配和映射额外的堆栈页面。典型的 Unix 内核必须跟踪在进程空间的每个区域发生页面错误时要采取的操作。例如，堆栈区域中的错误通常会分配和映射物理内存的新页面。程序的 BSS 区域中的错误通常会分配一个新页面，用零填充它并映射它。在具有按需分页可执行文件的系统中。

这是内核需要跟踪的大量信息。与采用传统的 Unix 方法不同，您将决定如何处理用户空间中的每个页面错误，其中错误的破坏性较小。这种设计的额外好处是允许程序在定义其内存区域时具有很大的灵活性；稍后您将使用用户级页面错误处理来映射和访问基于磁盘的文件系统上的文件。

## 设置页面错误处理程序

为了处理自己的页面错误，用户环境需要向JOS 内核注册一个*页面错误处理程序入口点*。用户环境通过新的`sys_env_set_pgfault_upcall`系统调用注册其页面错误入口点。我们增加了一个`Env`结构增加了个成员； `env_pgfault_upcall`，来记录这些信息。

## Exercise8

实现`sys_env_set_pgfault_upcall`系统调用。在查找目标环境的环境 ID 时一定要启用权限检查，因为这是一个“危险”的系统调用。

## 用户环境中的正常和异常堆栈

在正常执行过程中，JOS用户环境将在运行*正常的*用户堆栈：它的`ESP`注册开始了在指向`USTACKTOP`且堆栈数据之间是推动在页面上驻留`USTACKTOP-PGSIZE`和`USTACKTOP-1`包容性。然而，当在用户模式下发生页面错误时，内核将重新启动用户环境，在不同的堆栈上运行指定的用户级页面错误处理程序，即*用户异常*堆栈。本质上，我们将让 JOS 内核代表用户环境实现自动“堆栈切换”，这与 x86*处理器* 在从用户模式转换到内核模式时已经代表 JOS 实现堆栈切换非常相似！

JOS 用户异常栈也是一页大小，其顶部定义为虚拟地址`UXSTACKTOP`，因此用户异常栈的有效字节为 from`UXSTACKTOP-PGSIZE`到`UXSTACKTOP-1`inclusive。在这个异常堆栈上运行时，用户级缺页处理程序可以使用 JOS 的常规系统调用来映射新页面或调整映射，以修复最初导致页面错误的任何问题。然后，用户级页面错误处理程序通过汇编语言存根返回原始堆栈上的错误代码。

每个想要支持用户级页面错误处理的用户环境都需要使用`sys_page_alloc()`A 部分介绍的系统调用为其自己的异常堆栈分配内存。

## 调用用户页面错误处理程序

您现在需要更改`kern/trap.c 中`的页面错误处理代码 以处理来自用户模式的页面错误，如下所示。我们将故障时用户环境的状态称为*陷阱时间*状态。

如果没有实现页面错误处理程序，JOS 内核会像以前一样用消息破坏用户环境。否则，内核会在异常堆栈上设置一个trapframe，看起来像`struct UTrapframe`来自`inc/trap.h`：

```
                    <-- UXSTACKTOP 
trap-time esp 
trap-time eflags 
trap-time eip 
trap-time eax start of struct PushRegs 
trap-time ecx 
trap-time edx 
trap-time ebx 
trap-time esp 
trap-time ebp 
trap-time esi 
trap- time edi end of struct PushRegs 
tf_err (error code) 
fault_va <-- %esp 当处理程序运行时
```

内核然后安排用户环境恢复执行，页面错误处理程序运行在具有这个堆栈帧的异常堆栈上；你必须弄清楚如何做到这一点。该`fault_va`是导致页面错误的虚拟地址。

如果在发生异常时用户环境*已经*在用户异常堆栈上运行，则页面错误处理程序本身已出错。在这种情况下，您应该在当前（`tf->tf_esp`）的下方使用32位word分割，然后推送一个`struct UTrapframe`.

为了测试是否`tf->tf_esp`已经是用户异常堆栈上，检查它是否在之间的范围`UXSTACKTOP-PGSIZE`和`UXSTACKTOP-1`，。

## Exercise9

`page_fault_handler`在 `kern/trap.c 中` 实现将页面错误分派到用户模式处理程序所需的代码。写入异常堆栈时，请务必采取适当的预防措施。（如果用户环境用完异常堆栈上的空间会怎样？）

## 用户模式页面错误入口点

接下来，您需要实现汇编例程，该例程将负责调用 C 页错误处理程序并在原始错误指令处恢复执行。此汇编例程是将使用`sys_env_set_pgfault_upcall()`.

## Exercise10

`_pgfault_upcall`在`lib/pfentry.S 中` 实现例程。有趣的部分是返回到用户代码中导致页面错误的原始点。您将直接返回那里，而无需返回内核。困难的部分是同时切换堆栈和重新加载 EIP。

最后，您需要实现用户级页面错误处理机制的 C 用户库端。

## Exercise11

`set_pgfault_handler()` 在`lib/pgfault.c 中` 完成。

#### 测试

运行`user/faultread`( make run-faultread)。你应该看到：

```
... 
[00000000] new env 00001000 
[00001000] user fault va 00000000 ip 0080003a 
TRAP frame ... 
[00001000] free env 00001000
```

运行`user/faultdie`。你应该看到：

```
... 
[00000000] 新环境 00001000
我在 va deadbeef 处出错，错误 6 
[00001000] 优雅退出
[00001000] 免费环境 00001000
```

运行`user/faultalloc`。你应该看到：

```
... 
[00000000]新环保00001000
故障DEADBEEF
此字符串是在DEADBEEF故障
故障cafebffe
故障cafec000
此字符串是在cafebffe故障
[00001000]退出优雅
[00001000]免费ENV 00001000
```

如果您只看到第一个“此字符串”行，则表示您没有正确处理递归页面错误。

运行`user/faultallocbad`。你应该看到：

```
... 
[00000000] 新环境 00001000 
[00001000] va deadbeef 的 user_mem_check 断言失败
[00001000] 免费环境 00001000
```

确保您了解为什么`user/faultalloc`和 `user/faultallocbad`行为不同。

*挑战！* 扩展您的内核，以便不仅页面错误，而且在用户空间中运行的代码可以生成的*所有*类型的处理器异常都可以重定向到用户模式异常处理程序。编写用户模式测试程序来测试用户模式对各种异常的处理，例如被零除、一般保护错误和非法操作码。

## 实现写时复制分叉

您现在拥有`fork()` 完全在用户空间中实现写时复制的内核设施。

我们`fork()` 在`lib/fork.c 中`为您提供了一个骨架。比如`dumbfork()`， `fork()`应该创建一个新环境，然后扫描父环境的整个地址空间，并在子环境中设置相应的页面映射。关键区别在于，虽然`dumbfork()`复制*页面*，但 `fork()`最初只会复制页面*映射*。 `fork()`仅当其中一个环境尝试写入时才会复制每一页。

的基本控制流程`fork()`如下：

1. 父级安装`pgfault()` 为 C 级页面错误处理程序，使用`set_pgfault_handler()`您在上面实现的函数。

2. 父调用`sys_exofork()`以创建子环境。

3. 对于 UTOP 以下地址空间中的每个可写或写时复制页面，父级调用

   ```
   duppage
   ```

   ，它应该将写时复制页面映射到子级的地址空间，然后在其自己的地址空间。[ 注意：这里的排序（即在子级中将页面标记为 COW，然后在父级中标记它）实际上很重要！你明白为什么吗？尝试考虑颠倒顺序可能会导致麻烦的特定情况。]

   ```
duppage
   ```

   设置两个 PTE 使页面不可写，并包含
   
   ```
PTE_COW
   ```

   在“avail”字段中以区分写时复制页面和真正的只读页面。
   
   然而，异常堆栈*不会以*这种方式重新映射。相反，您需要在子级中为异常堆栈分配一个新页面。由于页面错误处理程序将进行实际的复制，并且页面错误处理程序在异常堆栈上运行，因此异常堆栈不能在写入时复制：谁来复制它？

   `fork()` 还需要处理存在但不可写或写时复制的页面。

4. 父级将子级的用户页面错误入口点设置为看起来像它自己的。

5. 孩子现在准备好运行了，所以父母将它标记为可运行。

每次其中一个环境写入尚未写入的写时复制页面时，都会发生页面错误。这是用户页面错误处理程序的控制流：

1. 内核将页面错误传播到`_pgfault_upcall`调用`fork()`的`pgfault()`处理程序。
2. `pgfault()`检查错误是写入（`FEC_WR`在错误代码中检查）并且页面的 PTE 标记为`PTE_COW`。如果没有，恐慌。
3. `pgfault()`分配一个映射在临时位置的新页面，并将故障页面的内容复制到其中。然后故障处理程序将新页面映射到具有读/写权限的适当地址，代替旧的只读映射。

用户级`lib/fork.c`代码必须查询环境的页表以进行上述几个操作（例如，页面的 PTE 被标记为`PTE_COW`）。内核`UVPT`正是为此目的映射环境的页表 。它使用了一种[巧妙的映射技巧](https://pdos.csail.mit.edu/6.828/2017/labs/lab4/uvpt.html)，可以轻松查找用户代码的 PTE。`lib/entry.S`设置`uvpt`和 `uvpd`以便您可以轻松地在`lib/fork.c 中`查找页表信息 。

## Exercise12

在`lib/fork.c 中` 实现`fork`,`duppage`和  `pgfault`

使用`forktree`程序测试您的代码。它应该产生以下消息，其中散布着“新环境”、“自由环境”和“优雅退出”消息。消息可能不会按此顺序出现，并且环境 ID 可能不同。

```
	1000：我是'' 
	1001：我是'0' 
	2000：我是'00' 
	2001：我是'000' 
	1002：我是'1' 
	3000：我是'11' 
	3001：我是'10' 
	4000：我是'100' 
	1003：我是'01' 
	5000：我是'010' 
	4001：我是'011' 
	2002：我是'110' 
	1004：我是'001' 
	1005：我是'111' 
	1006：我是“101”
	
```

*挑战！* 实现一个`fork()` 名为`sfork()`. 这个版本应该让父和子*共享*他们所有的内存页面（所以在一个环境中的写入出现在另一个环境中）除了堆栈区域中的页面，应该以通常的写时复制方式处理。修改`user/forktree.c` 以使用`sfork()`而不是常规`fork()`。此外，一旦您在 C 部分完成了 IPC 的实现，请使用您的`sfork()`来运行`user/pingpongs`。您必须找到一种新方法来提供全局`thisenv`指针的功能。

*挑战！* 您的实现`fork` 会进行大量系统调用。在 x86 上，使用中断切换到内核的成本非常高。扩充系统调用接口，可以一次发送一批系统调用。然后`fork`改用这个接口。

你的新速度有多快`fork`？

您可以（粗略地）通过使用分析参数来估计批处理系统调用对您的性能的改进程度来回答这个问题 `fork`：`int 0x30` 指令有多昂贵？你`int 0x30` 在你的`fork`? 访问`TSS`堆栈交换机也很昂贵吗？等等...

或者，您可以在真实硬件上启动内核并*真正*对代码进行基准测试。请参阅`RDTSC` IA32 手册中定义的（读取时间戳计数器）指令，该指令计算自上次处理器复位以来经过的时钟周期数。QEMU 不会忠实地模拟这条指令（它可以计算执行的虚拟指令的数量，也可以使用主机 TSC，这两者都不能反映真实 CPU 需要的周期数）。

B 部分到此结束。像往常一样，您可以使用 对您的提交进行评分make grade并使用make handin。

# 回答问题汇总

对于fork的写时复制，jos采用了“复制页表”的方式。而我们也可以用“共享页表”的方式：即在内核页目录中增加用户目录映射到父级，而不是前者：映射到新分配的页面作为子进程的页表，并memcpy复制父子页表。

话说回来，我们的复制页表，这种COW通常设置为read-only，而当我们写时会出现PGFLT，这里给出一些通用（不一定是JOS情况）可能PGFLT的例子：

- COW的修改

- 如果exec只映射部分？
  - BSS：通常或许先只映射了全为0的一页
  - STACK：通常先映射了一页而已
  - HEAP：比如linux下mmp和brk
  - TEXT：如果代码很长，那么我们有理由考虑先映射部分磁盘内容

上述的映射情况在linux下会增加一个vm_area_struct来维护和记录映射，比如brk只是在这个结构体中增加，并没有进行真正的映射（增加页表项）。

回到我们的JOS，我们本部分实验最终关注并实现一个fork，我们的fork简化为第一种情况：将父子USTACKTOP下的所有页都COW映射，因此我们将会不断的产生PGFLT来逐页替换为子进程独有部分。

## Exercise8

我们将注册用户的pagefault处理，我们可以让内核处理，也可以让用户处理，jos使用了用户处理，于是我们的中断流程：当我们pagefault时，中断处理会执行user-mode的exception stack，使用upcall处理，然后在user态下回到中断前的状态。

```
/*
 * UTOP,UENVS ------>  +------------------------------+ 0xeec00000
 * UXSTACKTOP -/       |     User Exception Stack     | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebff000
 *                     |       Empty Memory (*)       | --/--  PGSIZE
 *    USTACKTOP  --->  +------------------------------+ 0xeebfe000
 *                     |      Normal User Stack       | RW/RW  PGSIZE
 *                     +------------------------------+ 0xeebfd000
 *                     |                              |
 *                     |                              |
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     .                              .
 *                     .                              .
 *                     .                              .
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 *                     |     Program Data & Heap      |
 *    UTEXT -------->  +------------------------------+ 0x00800000
 */
```

我们在这里进行注册upcall函数。

所以我们记得权限检查，不要忘记还有syscall的调用添加（在syscall.h里的syscallno）

```
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* e;
	if(envid2env(envid, &e, 1))return -E_BAD_ENV;
	env->env_pgfault_upcall = func;
	return 0;
	//panic("sys_env_set_pgfault_upcall not implemented");
}
```

## Exercise9

用来处理用户页面错误，我们的内核之前处理时将会直接panic，所以我们现在在用户模式进行处理自己的pgfault，主要是对exception stack的判断处理，lab给的note非常详细

给出exception stack布局

```
                    <-- UXSTACKTOP 
trap-time esp 
trap-time eflags 
trap-time eip 
trap-time eax start of struct PushRegs 
trap-time ecx 
trap-time edx 
trap-time ebx 
trap-time esp 
trap-time ebp 
trap-time esi 
trap- time edi end of struct PushRegs 
tf_err (error code) 
fault_va <-- %esp 当处理程序运行时
```

我们让这个用户下单异常栈将会被upcall用来回到中断前的代码，而如果本身pagefault处理递归的话，就会多次插入UTrapframe。为什么预留要32bit（4B）？lab10会给出答案：递归时的eip存放。

page_fault_handler

```
if(curenv->env_pgfault_upcall){
	struct UTrapframe * utf;
	if(ROUNDUP(tf->tf_esp, PGSIZE) == UXSTACKTOP){
		utf = (struct UTrapframe *)((tf->tf_esp) - sizeof(struct UTrapframe) - 4);
	}else{
		utf = (struct UTrapframe *)(UXSTACKTOP - sizeof(struct UTrapframe));
		}
		
	user_mem_assert(curenv, (void *)utf, sizeof(struct UTrapframe), PTE_W);
	utf->utf_fault_va = fault_va;
	utf->utf_err = tf->tf_err;
	utf->utf_regs = tf->tf_regs;
	utf->utf_eip = tf->tf_eip;
	utf->utf_eflags = tf->tf_eflags;
	utf->utf_esp = tf->tf_esp;
	tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;//异常处理时的eip记录
	tf->tf_esp = (uintptr_t)utf;//下一次
	env_run(curenv);//返回user-mode
}

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
```



## Exercise10

我们使用

lib/pfentry.S下_page_fault_upcall

```
//  +----------USTACKTOP------+   high
  //  |            ...          |
  //  +-------------------------+
  //  |                         |
  //  +-------------------------+   
  //  |   trap-time-esp    (4B) |
  //  +-------------------------+   
  //  |   trap-time-eflags (4B) |
  //  +-------------------------+   
  //  |   trap-time-eip    (4B) |
  //  +-------------------------|   low
  //  |   trap-time-regs   (32B)|
  //  |   ...                   |
  //  |   ...                   |
  //  +-------------------------+   
  //  |   err              (4B) |
  //  +-------------------------+   
  //  |   fault_va         (4B) | 
  //  +-------------------------+   <-- cur_esp 
  //            (1)
  //  
  //  +----trap-time-stack------+
  //  |            ...          |
  //  +-------------------------+
  //  |   trap-time-eip    (4B) |
  //  +-------------------------+   <-- trap_time_esp
  
// LAB 4: Your code here
	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.
	// trap-time esp -= 4 to push trap-time eip into trap-time stack
	movl 0x30(%esp), %eax
	subl $0x4, %eax
	movl %eax, 0x30(%esp)
	//push trap-time eip into trap-time stack
	movl 0x28(%esp), %ebx
	mov %ebx, (%eax)
	//restore trap-time registers
	addl $8, %esp
	popal
	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.
	addl $4, %esp
	popfl
	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
	popl %esp
	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
	//ret: popl %eip
	ret
```



## Exercise11

lib/pgfault.c

set_pgfault_handler

```
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		r = sys_page_alloc(0, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W;
		if(r < 0)panic("set_pgfault_handler: page alloc fault!");
		r = sys_env_set_pgfault_upcall(0, (void *)_pgfault_upcall);
		if(r < 0)
			panic("set_pgfault_handler: set pgfault upcall failed!");
	}
	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}
```

## Exercise12

lib/fork.c

这里解决一个当初理解错误的问题：UVPT是用户映射，并且分配了4MB虚拟内存（inc/memlayout.h），刚好对应了1M的页数。所以uvpt里的index应该是PGNUM的宏而不是PTX，而uvpd是页目录也确实没错，并且是以kern_pgdir模板，并且用户只读不可改。只有uvpd的内核相应函数查找时候才用到PTX。这是一个比较绕的设计想法。

```
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	uint32_t write_err = err & FEC_WR;
	uint32_t COW = uvpt[PGNUM(addr)] & PTE_COW;
	if(!(write_err && COW))panic("pgfault: not write to the COW page fault!\n");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	//alloc a page by PFTEMP

	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W);
	if(r < 0)panic("pgfault: sys_page_alloc failed!\n");
	//copy data
	memmove(PFTEMP, addr, PGSIZE);
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_U | PTE_P | PTE_W);
	if(r < 0)panic("pgfault: sys_page_map failed!\n");
	
	//remove PTE:PFTEMP
	r = sys_page_unmap(0, PFTEMP);
	if(r < 0)panic("pgfault: sys_page_unmap failed!\n");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//COW check, map page
	pte_t pte = uvpt[pn];
	void *addr = (void *) (pn * PGSIZE);
	
	uint32_t perm = pte&0xfff;
	if(perm & (PTE_W | PTE_COW)){
		perm &= ~PTE_W;
		perm |= PTE_COW;
	}
	
	r = sys_page_map(0, addr, envid, addr, perm & PTE_SYSCALL);
	if(r < 0)panic("duppage: sys_map_page child failed\n");
	//map self again : freeze parent and child
	r = sys_page_map(0, addr, 0, addr, perm & PTE_SYSCALL);
	if(r < 0)panic("duppage: sys_map_page self failed\n");
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//1.set page fault handler
	set_pgfault_handler(pgfault);
	//2.create a child env	
	envid_t envid = sys_exofork();//just the tf copy	
	if (envid == 0) {//must after code below excuted
		thisenv = &envs[ENVX(sys_getenvid())];//fix "thisenv" in the child process
		return 0;
	}
	if (envid < 0) {
		panic("fork: sys_exofork: %e failed\n", envid);
	}
	//COW mapping:duppage(envid, va's page):from 0 - USTACKTOP(under UTOP)
	uint32_t addr;
	for (addr = 0; addr < USTACKTOP; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr))] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)) {
			duppage(envid, PGNUM(addr));	//env already has page directory and page table
		}

	//child's exception stack
	int r;
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_P | PTE_W | PTE_U)) < 0)	
		panic("sys_page_alloc: %e", r);
	//set child's pgfault_upcall
	extern void _pgfault_upcall(void);
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);		
	//runnable
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)	 
		panic("sys_env_set_status: %e", r);
	return envid;
	//panic("fork not implemented");
}
```

## 总结

总结一下PartB的upcall机制和fork处理

1.set upcall

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/截屏2021-11-23 下午8.05.26.png)

2.call upcall 

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/截屏2021-11-23 下午8.16.30.png)

3.COW mechanism

​	1.what in lib/fork.c?

- pagefault: user registers **own** handler **supported by syscall**
- duppage: copy virtual page into appointed env id(**just modify the page table**)
- fork: create a child(**COW mapping**)

​	2.what fork done? 

- set page fault handler
- cread a child 
- fix child thisenv->env_id to real env id
- COW mapping
- child's page fault handler
- set child's state = RUNNABLE

​	3.what syscall supported for handler settings and using？

- sys_env_set_status
- sys_env_set_pgfault_upcall
- sys_page_alloc(just check va perm and **alloc a ppage**)
- sys_page_map
- sys_page_unmap

那么问题来了，这个upcall在user-mode下，使kern做出了这么多事情，也让中断的次数增加了很多，到底是为什么？

给出一些我查阅的资料：

关于*kernel upcall* 引用 **[lkml](http://lkml.indiana.edu/hypermail/linux/kernel/9809.3/0922.html)** 中的一段话 :

> An upcall is a mechanism that allows the kernel to execute a function in userspace, and potentially be returned information as a result.
>
> An upcall is like a signal, except that the kernel may use it at any time, for any purpose, including in an interrupt handler.
>
> A process asks to use upcalls, and passes the kernel the addresses of a series of stacks to execute upcalls on. The kernel wires down down the stacks. The process registers functions associated with a set of predefined events (such as a page fault or blocking I/O). When such an event happens, the thread for which the event occured to doesn't call schedule(), but instead switches to an upcall stack, constructs a dummy trap return so that on return to user space it will execute the upcall, and returns to user space via a trap return.
>
> Even Larry will, I hope, admit that this is a pretty fast process, much faster than a context switch, and way faster than a call to *any* schedule().
>
> Note however that the function *NEVER RETURNS TO THE KERNEL*.

在这个邮件列表里面还给出了 upcall 可以用来实现 scheduler activation 和 timing in user space code :

> Why would you want upcalls ? Well, we implemented upcalls specifically for a thread package that uses an idea called scheduler activations; every time a kernel thread blocks on I/O or suffers a page fault, the kernel "activates" the user level thread scheduler and tells it what happened. This way, the user level thread scheduler can continue to use the processor by deciding to run some other thread.
>
> It would also allow much more precise timing for Linux user space code, because a process could register a function (and yes, it has to be a very carefully designed process) to be executed *by* the timer interrupt (probably the timer code BH), not whenever the process gets woken by the timer interrupt and then run.

大意就是：

1.对于阻塞I/O时候，不再上下文切换，会快

2.对于抢占式切换（时间片），可以随时切换。如果是在内核中，会因为同步问题加锁来禁止切换，导致了时间片效果很差
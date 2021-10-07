参考官网：https://pdos.csail.mit.edu/6.828/2017/labs/lab1/

AT&T汇编：http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html



## 启动操作系统

安装lab的os：

```
git clone https://pdos.csail.mit.edu/6.828/2017/jos.git lab
```

编译

```
#进入lab目录
make && make qemu
```

make之后最后一行出现代表os内核映像编译成功

```
+ mk obj/kern/kernel.img
```

make qemu之后出现如下图说明qemu连接成功

![](https://tva1.sinaimg.cn/large/008i3skNgy1gt5qnshr76j314g0k8n1i.jpg)



打开gdb查看bios

```
#lab目录下
make qemu-gdb
#再开一个相同路径的terminal
make gdb
#si命令逐步执行
#关闭gdb：q
#关闭qemu-gdb：ctrl + A 和 X,需要注意ctrl 与 A同时按住抬起后再按X,不要三个键同时按
```



## Part 1: PC Bootstrap

BIOS：Basic Input/Output System 

物理内存：

```
+------------------+  <- 0xFFFFFFFF (4GB)
|      32-bit      |
|  memory mapped   |
|     devices      |
|                  |
/\/\/\/\/\/\/\/\/\/\

/\/\/\/\/\/\/\/\/\/\
|                  |
|      Unused      |
|                  |
+------------------+  <- depends on amount of RAM
|                  |
|                  |
| Extended Memory  |
|                  |
|                  |
+------------------+  <- 0x00100000 (1MB)
|     BIOS ROM     |
+------------------+  <- 0x000F0000 (960KB)
|  16-bit devices, |
|  expansion ROMs  |
+------------------+  <- 0x000C0000 (768KB)
|   VGA Display    |
+------------------+  <- 0x000A0000 (640KB)
|                  |
|    Low Memory    |
|                  |
+------------------+  <- 0x00000000
```

可以看到bios从1MB结束，早期的PC如8088物理内存只有1MB（0x00000000-0x000fffff），现在这台x86扩展到了4GB。

打开gdb实际运行一些si命令（逐步执行机器码），可以看到：

```
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
#从接近bios顶部而不是960k地址进行长跳转到cs:0xf000(bios段基址),ip:0xe05b,反而往回跳
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb) si
[f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x6ac8
0x0000e05b in ?? ()
```

实验指导告诉我，bios在长跳转之后进行中断描述符表（IDT）的初始化和VGA等各种devices等初始化，于是继续查看得到以下内容：发现存在关中断（clean）命令。

![](https://tva1.sinaimg.cn/large/008i3skNgy1gt5t1uy8rlj30pc0i4go6.jpg)

然后接着BIOS读取boot loader，进行boot驱动

## Part 2: The Boot Loader

硬盘或者软盘最小单位为扇区（sector），一个扇区512字节，如果硬盘可以boot，那么这第一个扇区叫做bootsector，存放了boot loader 代码。

bios通过读取bootsector进入内存，从0x7c00-0x7dff.当然，现在不止读取一个扇区，也不止硬盘/软盘作为boot来源。

对于boot loader，实验已经准备好了boot/boot.S和boot/main.c来模拟。

对于boot loader，做两件事：

1. 从实模式到保护模式，即将物理内存从1MB扩展到4GB，这需要GDT
2. 从硬盘，通过x86的已经写好的IO驱动来使用IDE设备寄存器，从第二个扇区开始读取内核代码（obj/kern/kernel.asm）

### Exercise3

1. 通过GDB在boot入口0x7c00进行断点设置
2. 看 `boot/boot.S`里的代码, 看 `obj/boot/boot.asm` 来确定地址. 显示 boot loader在GDB中反汇编命令,比较三种汇编代码差异
3. 继续看boot/main.c 代码执行readsect()，找到c对应的汇编代码，紧接着代码执行回到bootmain函数 , 找到for循环（读取剩余内核代码），在for循环结束地址设置断点，然后单步执行，看看最终会做什么

回答以下问题:

- 代码从哪里开始执行32位保护模式？是什么导致了16位到32位到转换？
- boot loader最后一个指令是什么,kernel第一个指令是什么？
- kernel第一条指令的地址?
- boot loader如何决定读取内核的所有扇区?从哪里能找到信息?

我们通过GDB来设置breakpoint来跟踪boot loader过程

```
#b *0x7c00 意味着断点
#c 意味着继续执行到下一个断点前
#si N 意味着N步执行机器码
#x/Ni ADDR 意味着从给定地址显示N步机器码，这里没有*
```

![](https://tva1.sinaimg.cn/large/008i3skNly1gt5w9iq3t9j30ve0o4dis.jpg)

对比三个文件可以发现：GDB没有数据类型指定，如movb会执行为mov，同时boot.asm相较于boot.S少了头文件的引入标志，同时对于GDT只剩下CR0的开启

然后再看bootmain，调用两个函数：readseg和readsect

先定义了两个eh和eph，eph=eh+程序num

然后从硬盘读一页，判定elf的魔数来决定内核是否合乎ELF格式，紧接着根据eph和eh使用for循环来加载剩余内核。

最后进入根据ELF节头表进入内核执行的entry



### Exercise4

1. 推荐阅读有关C指针内容：*C Programming Language* by Brian Kernighanand Dennis Ritchie (known as 'K&R')

2. 阅读从5.1到5.5，下载运行[以下C程序](https://pdos.csail.mit.edu/6.828/2017/labs/lab1/pointers.c)，确保你懂了所有输出结果原因，给出以下标准

   ```
   1.第一到第六行的输出地址
   2.第二到第四行输出结果
   3.第五行为什么输出错误
   ```

3. 这里还有一个[参考](https://pdos.csail.mit.edu/6.828/2017/readings/pointers.pdf)，不过不是很推荐

4. 这个阅读实验一定要做，否则后续的代码编写，你会知道什么叫做 “残忍”

5. 可以从本人的关键文件注释下参考代码解读



### Exercise5

1. 重新看boot loader开始一部分代码，如果你修改链接地址，那么会有一些错误，找到关于链接地址的错误，在boot/Makefrag里面修改
2. 使用make clean清除上次make缓存
3. 重新编译，运行代码，解释发生的错误情况

这里对于boot loader再做一些讲解，在main.c里面非常重要的就是那个ELFHDR，是二进制格式，并且对于ELF有以下[讲解](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)，布局如下：![](https://tva1.sinaimg.cn/large/008i3skNly1gt8litmokbj30ec090t9b.jpg)

可以在inc/elf.h里查看ELF布局，左边是链接视图，是目标代码格式，比如我们编写，链接重定位过程中使用。右边是可执行视图，在这里加载到内存时使用，并且section header table不是必须。（简单区分section和segment使用时机即可，详细看《深入理解计算机系统》）因此我们的loader根据ELF读取很多段，在硬盘中读取单位为扇区，详细看关键文件注释，先给出部分的解释：

- .text 程序的执行命令集
- .rodata 只读数据，比如ASCII，当然硬件并不会禁止写操作（？）
- .data 已初始化的全局数据，比如全局定义x=5
- .bss 未初始化的全局数据，在link阶段会留空间给他，并且.bss在程序运行时候必须全为0

编译时候将每个.c文件编译称.o文件，编码为硬件预期的二进制格式。在obj/kern/kernel里把所有的.o文件链接成二进制image（映像），转换为ELF格式，因此ELF英文解释为“可执行可链接的格式”。

对于本实验，你只需要考虑它作为header的功能（所需来自ELF header 和program header table）：将后面的程序节加载入内存。boot loader不区分加载代码还是数据，直接加载然后执行就完事了。

我们可以输入以下指令进入内核program headers看所有节

```
objdump -h obj/kern/kernel  
//自己写的工具链就要多加i386-jos-elf-objdump，呵呵我怎么可能那么折腾
```

你会看到很多节，远超上面列出来的。包括program header记录但是没有写进内存的。

重点来了：前面所说出问题的“链接地址”在此称作”VMA“，我们可以看作逻辑地址，由于还没分段，那么逻辑地址应该等于物理地址，也就是加载地址“LMA”。我们输入指令得到

![](https://tva1.sinaimg.cn/large/008i3skNly1gt86375fgsj310o03ugmc.jpg)

看.text节。

接着还有以下两种指令供你参考：

```
objdump -h obj/boot/boot.out //看boot被加载的所有节
objdump -x obj/kern/kernel   //看内核program headers
```

第二条命令执行后，可以看出Program Headers的字样，加载入内存的节会显示LOAD，和一些其他的信息，比如“paddr”代表物理地址，“vaddr”代表虚拟地址，“-sz”结尾代表大小

假设你已经理解了boot.S文件对于VMA的作用和某某关系（下面会讲），那么我们关注的VMA如何改？在boot/Makefrag文件里，比如boot的VMA是0x7c00，那么对应文件里有-Ttext 0x7C00。

我将0x7c00改为0x2c00，会看到编译后的.asm从0x2c00开始boot，但是网上告诉我实际执行蹦过了这一段，头铁地接着从0x7c00开始运。运行一段时间后发现qemu-gdb报错![](https://tva1.sinaimg.cn/large/008i3skNly1gt85ybiufnj30ti07e76b.jpg)

很显然这里的GDT全0，导致了一些错误，那么我们的链接地址VMA对GDT的初始化出现了问题。

对于GDT初始化，查看代码可以发现有以下指令

![](https://tva1.sinaimg.cn/large/008i3skNly1gt86dwtrk8j30m80gedi1.jpg)

这里的lgdtw命令即将指定内存后的六个字节存放的GDTR寄存器里，那么存放的都是0，说明本应该是0x7c64，这里是第一个错误。

紧接着看：下面的ljmp会卡死，这条指令意思是以32位模式跳到下一条指令，本应该是0x7c32，但是同样出现了问题，后续的无法执行，即第二个错误。

### Exercise6

接着是内核的VMA，由于保护模式的建立（GDT），boot loader地址会指向低地址（VMA）但是实际上代码执行到高地址（LMA）。

看ELF header，里面的e_entry包含了就是我们的内核的（main.c里最后一句）VMA，可以通过以下指令查看LMA

```
objdump -f obj/kern/kernel
```

![](https://tva1.sinaimg.cn/large/008i3skNly1gt96spmegtj30le04gaau.jpg)

![](https://tva1.sinaimg.cn/large/008i3skNly1gt96u5pk3rj30sa02ut99.jpg)

我们可以看到这里这两个地址的区别，就在于保护模式的分段机制：KERNBASE的添加。

```
#The kernel (this code) is linked at address ~(KERNBASE + 1 Meg), 
# but the bootloader loads it at address ~1 Meg.
```



假设你现在已经明白了main.c的过程，我们应该做一些事

1. 通过gdb 的x命令来查看内存，格式

   ```
   x/Nx ADDR //从指定地址以N个word（我这台机器里是4B 32位）查看内存
   ```

   更多命令请看[GDB手册](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html)

2. 在进入boot时候来通过以下指令查看

   ```
   x/8x 0x00100000  //记得这个地址吗？是BIOS结束的下一个地址
   ```

3. 然后再以相同方式看看kernel的进入时候的存放内容，告诉我为什么存放内容不同

在两个阶段查看结果如下：

![](https://tva1.sinaimg.cn/large/008i3skNly1gt97i7mrsoj312q0figp0.jpg)

如果你对boot干的事一清二楚的话，你就会知道，这个地址存放的八个字长内容不同是必然的。

## Part 3: The Kernel

接下来的exercise中种你将要写一些C代码了！

```
作者碎碎念（选看）：先说说保护模式，往往第一次看书，关于保护模式会有很长的章节，最终可能只能知道保护模式通过段选择子->GDT里段描述符->段基址来进行分段操作，但实际上这并不是保护模式的所有内涵。

首先保护模式是一个宽泛的概念，包含了分页机制，文件系统等实现；其次保护模式到底“保护”什么，这得看描述符里关于权限，段限长的信息描述->你应该知道保护什么了吧。

不过这种程度的保护还不够全面，结合分页机制的分段和纯粹的分段效果并不是单纯叠加的。

想想：分页之前我们得到了线性地址（虚拟地址），分页要做的就是转换为物理地址，这里有一个问题：

1. 线性地址和物理地址的关系

内存（物理）空间假设有4G，那么设计线性空间时我们非常大胆地也弄成了4G，于是相同的线性地址（线性空间）就会映射不同的物理空间，这对于用户程序或者说多进程，不仅有“大内存”的爽，也是一种“保护“。

但是问题又来了：内核空间对于进程是共享的，这怎么设计呢？很简单，每个进程的线性空间分成两部分：用户态和内核态，保证内核态映射相同即可，linux下3:1，win下1:1。

还有一个问题：这么大胆的线性空间的抽象层设计，内存不够怎么办？很显然这涉及内存换入换出的问题，关于内存和磁盘的换页，这里就不说算法了。不过换页又和分页机制的页表联系在一起，逻辑自洽，细节部分好好查资料思考吧。
```

### 使用虚拟内存来在独立空间里工作

让我们看看LMA和VMA，在进入内核时候我们就发现了LMA和VMA的开始不同：ELF头显示了0x10001c的执行地址但是boot loader给出了0x10018的调用。

不过现在要完成完整的VMA：分段分页结合的那种！

在kern/kernel.ld里，你就能看到VMA和LMA，一般内核喜欢让程序链接虚拟地址高地址，但是如何转化为内存物理地址呢？这种机制在lab2里会详细讲解。

对于本内核，VMA会给出0xf0100000，通过map分页得到物理地址0x00100000。现在先不说怎么做到，当然下个lab会教你如何在256MB限定下完成从虚拟地址0xf0000000 - 0xf0400000 映射物理地址0x00000000 - 0x00400000的分页map，好了扯远了。

### Exercise7

1. 在内核kern/entry.S代码movl %eax, %cr0处设置断点，看看内存0x00100000 和0xf0100000
2. 单步执行，然后再看这两个内存，理解这其中的变化
3. 如果分页机制的映射错了，那么第一行错的指令是什么，看看源代码文件，找出来











### 控制台的格式化输出

C语言里有print（）格式化输出，同样I/O也有，它叫cprintf，阅读 

```
kern/printf.c, lib/printfmt.c,  kern/console.c
```

这三个文件（看关键文件注释）来明白怎么这三个文件的关系。如果在后续的lab中你知道了为什么printfmt.c在lib目录下，这个关系就会变得很清楚。

### Exercise8

1. 在cprintf（）方法里我们省略了一小段代码：使用“%o”来打印八进制，请你实现这部分

回答以下问题：

1. 解释printf.c和console.c之间的接口，详细点：前者调用了什么功能，后者输出了什么功能

2. 解释下列关于console 的代码

   ```
    	if (crt_pos >= CRT_SIZE) {
                 int i;
                 memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
                 for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
                         crt_buf[i] = 0x0700 | ' ';
                 crt_pos -= CRT_COLS;
        }
   ```

3. 单步执行以下代码

   ```
   int x = 1, y = 3, z = 4;
   cprintf("x %d, y %x, z %d\n", x, y, z);
   ```

   - 在cprintf（），fmt指向什么，ap指向什么
   - 列出每个对于cons_putc, va_arg, 和vcprintf的调用
   - sons_putc:列出形参
   - va_arg:说出ap之前指向与调用之后的指向
   - vcprintf:列出两个实参

4. 运行以下代码

   ```
   unsigned int i = 0x00646c72;
       cprintf("H%x Wo%s", 57616, &i);
   ```

   - 输出是什么
   - 单步执行解释输出的由来，有个[ASCII表](http://web.cs.mun.ca/~michael/c/ascii-table.html)供你参考
   - 本次假设为小端，如果是大端你该怎么更改i的值？

5. 对于以下代码

   ```
   cprintf("x=%d y=%d", 3);
   ```

   “y=”之后应该输出什么（提示：不唯一）？为什么会这样？

6. 假设GCC改变调用约定（高地址->低地址变为反向），在（参数）声明顺序下堆栈传参，你要怎么改cprintf（）或者它的接口，来成功传参数？

7. 提高实验：通过ASCII来显示彩色字符

## 栈



### Exercise9

1. 决定内核在哪初始化，并且确定栈的位置
2. 内核代码如何为栈保留空间
3. 保留空间的end是栈指针初始化esp的指向吗？



我们知道栈是向下增长，具体为esp的逐步递减，在32位系统esp可以被4整除，这意味着esp的递减是4（比如0，4，8这样）。大量的指令比如call，通过使用这样的寄存器来访问地址

对于ebp寄存器，我们称之为“栈底”，很显然，指向高地址。比如在.c文件每个函数的调用时候，esp是通过指向ebp来完成函数栈的建立，随后esp递减。所以ebp用来定位多层函数时非常有用。因此相应的栈回溯功能是十分有必要的



### Exercise10

1. 为了了解函数调用，让我们追踪 `test_backtrace` 函数地址在 `obj/kern/kernel.asm`，设置断点，看看内核会做些什呢
2. 每次递归嵌套多少个word通过函数传入堆栈？他们分别是什么
3. 提示：你必须用本实验匹配的QEMU，不然你得自己翻译线性地址







### Exercise11

1. 实现上述指定的backtrace函数。



对于这个exercise给你一些提示：你需要实现一个栈回溯功能，意味着你应该调用mon_backtrace()，原型在kern/monitor.c里。同时read_ebp() 在inc/x86.h很有用。你应该让用户在交互时使用这个函数

这个栈回溯功能应该长这个格式

```
Stack backtrace:
  ebp f0109e58  eip f0100a62  args 00000001 f0109e80 f0109e98 f0100ed2 00000031
  ebp f0109ed8  eip f01000d6  args 00000000 00000000 f0100058 f0109f28 00000061
  ...
```

是不是很眼熟？对。gdb功能里 i r ebp eip也有类似的功能。

每行我们应该有ebp，eip，args。下面对格式进行一个简短的说明

- ebp为函数调用后的栈底
- eip为函数返回后的ip，偷偷告诉你，就在ebp上面
- args后的五个十六进制是五个参数，可以传参少于这个数，但是仍然列出5个

然后我们对内容进行一个简短对说明

- 第一行反应了现在执行函数，也就是mon_backtrace
- 第二行是调用mon_backtrace对函数
- 第三行往后即再上层调用，以此类推

你应该打印所有特别（？）的栈，通过学习kern/entry.S，你会发现怎么终止打印

接下来是关于接下来实验的理论提示。这是对K&R第五章的一些内容（为啥现在说？）

- Int *p= (int *)100, 则（int）p+1=101,(int) (p+1)=104
- p[i]=*(p+i)
- &p[i]=(p+i)
- 其实指针不难，你画图理解就行了

每当操作系统对内存地址描述时，谨记：是值还是指针（址）。













### 



## 回答问题汇总

### Exercise3

1. .code32伪指令开始；ljmp语句切换，从16位地址模式切换到32位地址模式

2. ```
   7d81:	ff 15 18 00 01 00    	call   *0x10018
   0x10000c:	movw   $0x1234,0x472
   ```

3. 由GDB或者kernel/entry.S可以得知为0x10000c

4. ELF存放了程序段数，段基址，段长

   ```
   //ph每个结点为一个段
   ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
   	eph = ph + ELFHDR->e_phnum;
   	for (; ph < eph; ph++)
   		// p_pa 是加载内核的程序段基址
   		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);
   ```



### Exercise4

1.学习笔记

```
1.单目运算符结合方向为从右向左。
举例：
*p++;  // 将指针p的值加1，然后获取指针ip所指向的数据的值
(*p)++;  // 将指针p所指向的数据的值加1
2.对于a[i]，C编译器会立即将其转换为*(a+i)。同时对于i[a],计算结果相同。

3.数组名和指针的一个区别：指针是变量，可以进行赋值和加减运算；数组名不是变量，不能进行赋值和加减运算。

4.p[-1],在语法上是合法的，只要保证元素存在，因此带来了安全隐患。

5.指向同一个数组（包括数组最后一个元素的下一个元素，可能考虑到兼容字符的'\0'）的两个指针可以进行大小比较，而对没有指向同一个数组的两个指针进行大小比较的行为是undefined的。

6.指针的合法操作：同类指针赋值、指针加减某个整数、指向同一数组的两个指针相减或比较大小（存放的值而不是说指向的值）、将指针赋值为0或与0比较；
指针的非法操作：两个指针相加、相乘、相除，以及指针与浮点数相加减等。

7.char *p = "now is the time"；是将一个指向字符数组的指针赋值给p，而不是字符串拷贝。C语言不提供任何对整个字符串作为一个单元进行处理的操作符。

注意通过字符串指针修改字符串内容的行为是undefined的：

char a[] = "now is the time";  // an array
char *p = "now is the time";   // a pointer
```

2. 解读答案看关键文件注释



### Exercise6

```
刚进入boot时候BIOS上的地址没动过，此时从0x7c00开始boot会通过设置的保护模式将内核代码加载入内存，这时候高地址才会被修改，紧接着进入内核代码。
```



### Exercise7





## 关键文件注释

### /boot/boot.S

```c
#include <inc/mmu.h>

# 转换为32位保护模式，然后跳转到main.c执行
# BIOS从硬盘第一个扇区加载这个代码到内存0x7c00处
# 然后以%cs=0 %ip=7c00开始执行保 护模式

.set PROT_MODE_CSEG, 0x8         # 内核代码段选择子
.set PROT_MODE_DSEG, 0x10        # 内核数据段选择子
.set CR0_PE_ON,      0x1         # 保护模式开关

.globl start
start:
  .code16                     # 汇编为16位模式
  cli                         # 关中断
  cld                         # 字符串增量操作（清eflags里DF，进行一个顺序处理）

  # Set up the important data segment registers (DS, ES, SS).
  xorw    %ax,%ax             # 第0段
  movw    %ax,%ds             # -> 代码段
  movw    %ax,%es             # -> 附加段
  movw    %ax,%ss             # -> 栈段

  # 打开A20:
  #   对于早期PC, 物理地址只有20位地址线，超过则会进位为0，而后来为了兼容即在16位到	 #	 32位转换时打开第21位地址线，从而为GDT做准备
seta20.1:
  inb     $0x64,%al               # 等待不忙
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # 0xd1 -> 端口 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # 等待不忙
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> 端口 0x60
  outb    %al,$0x60

  # 转换实模式到保护模式, 通过使用GDT和段转换确保虚拟地址和物理地址一一对应
  # 因此有效mermory map在这个过程中不改变
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0
  
  # 以32位模式，跳转到下一个指令
  # 即进入32位模式
  ljmp    $PROT_MODE_CSEG, $protcseg

  .code32                     # 汇编伪指令32位模式
protcseg:
  # 设置保护模式到数据寄存器
  movw    $PROT_MODE_DSEG, %ax    # 段选择子
  movw    %ax, %ds                # -> DS: 数据段
  movw    %ax, %es                # -> ES: 附加段
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS
  movw    %ax, %ss                # -> SS: 栈段
  
  # 把栈顶指针esp指向main.c文件中去
  movl    $start, %esp
  call bootmain

  # 当然如果引导gdt返回（失败），那么就死循环
spin:
  jmp spin

# 引导GDT，也可以认为初始化
.p2align 2                                # 4字节对齐
gdt:
  SEG_NULL				# 空段
  SEG(STA_X|STA_R, 0x0, 0xffffffff)	# 代码段
  SEG(STA_W, 0x0, 0xffffffff)	        # 数据段

gdtdesc:
  .word   0x17                            # gdt长度-1
  .long   gdt                             # gdt的地址

```

### /boot/main.c

```c
#include <inc/x86.h>
#include <inc/elf.h>

/**********************************************************************
 *简易的的boot loader，功能为从第一个IDE硬盘读取ELF格式的内核 
 *
 *硬盘格式
 *  * boot.S和main.c是boot loader的全部代码，存放第一个扇区
 *
 *  * 第二个扇区开始存放内核
 *
 *  * 内核img必须是ELF格式
 *
 * boot 步骤
 *  * 当cpu再BIOS中加载硬盘到内存中时
 *
 *  * BIOS初始化设备,中断描述符表, 读取第一个扇区到内存然后jmp到0x7c00
 *
 *  * 然后boot loader执行
 *
 *  * 从boot.S开始执行，它将会设置保护模式，然后main.c接管
 *    
 * 
 *  * bootmain()就是本文件，它接管后到任务就是读取内核，跳转内核
 **********************************************************************/

#define SECTSIZE	512 //扇区字节大小
#define ELFHDR		((struct Elf *) 0x10000) // 判定抓取内核空间

void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void
bootmain(void)
{
	struct Proghdr *ph, *eph;

	// 从硬盘中读取一页
	readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);

	// 判定是否ELF格式
	if (ELFHDR->e_magic != ELF_MAGIC)
		goto bad;

	// 加载每个内核段 (忽略 ph flags)，接下来两行都是源自ELF header
/*ELF program header
 *int type; // loadable code or data, dynamic linking info, etc. 
 *int offset; // ELF里段偏移
 *int virtaddr; // virtual address to map segment 
 *int physaddr; // physical address, not used 还没分页所以没用
 *int filesize; // size of segment in file 
 *int memsize; // size of segment in memory (bigger if contains BSS) 
 *int flags; // Read, Write, Execute bits
 *int align; // required alignment, invariably hardware page size
 */

	ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	eph = ph + ELFHDR->e_phnum;
	for (; ph < eph; ph++)
		// p_pa 是加载内核的程序段偏移，接下来一行源自program header
    //加载每个程序段（忽略ph标志）
		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

	// 从ELF头得到信息进入内核执行entry,源自ELF header
	// notes：不返回！，汇编指令call   *0x10018
	((void (*)(void)) (ELFHDR->e_entry))();

bad:
	outw(0x8A00, 0x8A00);
	outw(0x8A00, 0x8E00);
	while (1)
		/* 什么都不干 */;
}

// 从‘offset’在内核中读取‘count’字节数，返回到地址‘pa’
// 可能复制量超过要求！
void
readseg(uint32_t pa, uint32_t count, uint32_t offset)
{
	uint32_t end_pa;

	end_pa = pa + count;

	// 向下取到扇区边界
	pa &= ~(SECTSIZE - 1);

	// 把字节转换为扇区，然后内核从扇区1开始执行
	offset = (offset / SECTSIZE) + 1;

	// 如果太慢，可以一次读很多扇区
	// 我们可以写内存超过要求，但是没关系 --
	// 因为可以递增加载
	while (pa < end_pa) {
		// 因为还没开启分页，并且使用了指定段表 (see boot.S),所以可以直接访问物理地 			//址.  当然JOS如果使用了MMU就不行了
		readsect((uint8_t*) pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void
waitdisk(void)
{
	// 等待硬盘就绪
	while ((inb(0x1F7) & 0xC0) != 0x40)
		/* do nothing */;
}

void
readsect(void *dst, uint32_t offset)
{
	// 等待硬盘就绪
	waitdisk();

	outb(0x1F2, 1);		// count = 1
	outb(0x1F3, offset);
	outb(0x1F4, offset >> 8);
	outb(0x1F5, offset >> 16);
	outb(0x1F6, (offset >> 24) | 0xE0);
	outb(0x1F7, 0x20);	// 硬件读取命令cmd 0x20 - 来读取扇区

	// 等待硬盘就绪
	waitdisk();

	// 读一个扇区
	insl(0x1F0, dst, SECTSIZE/4);
}

```

### Pointers.c

```c
#include <stdio.h>
#include <stdlib.h>

void
f(void)
{
    int a[4];
    int *b = malloc(16);
    int *c;
    int i;//a,b,c,i相邻存放，存放顺序取决于本身计算机大端还是小端,假设小端存放
//第一行 1: a = %p, b = %p, c = %p 输出各自的值，由于没有定义所以不确定
    printf("1: a = %p, b = %p, c = %p\n", a, b, c);

    c = a;
    for (i = 0; i < 4; i++)
	a[i] = 100 + i;	//a[0...3]= 100, 101, 102, 103
    c[0] = 200;	//a[0]=200
//第二行 2: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("2: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c[1] = 300;	// a[1]=300
    *(c + 2) = 301;	// a[2]=301
    3[c] = 302;	// *（3+c）即a[3]=302
//第三行 3: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("3: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c = c + 1;
    *c = 400;//a[1]=400
//第四行 4: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("4: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c = (int *) ((char *) c + 1); 
    *c = 500;//301=16*(2+16)+13
    //a[1]=0x190,a[2]=0x12d,500=0x1f4小端存放: 90 f4 01 00 00 01 00 00 
    //a[1]=0x1f490=0+144+1024+61140+65536=127844
    //：90 f4 01 00 00 01 00 00->a[1]=127844,a[2]=256
//第五行 5: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("5: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    b = (int *) a + 1;//b指向a[1]
    c = (int *) ((char *) a + 1);//同上，从a[0]第二个字节开始修改，影响a[0]与a[1]
//第六行 6: a = %p, b = %p, c = %p
    printf("6: a = %p, b = %p, c = %p\n", a, b, c);
}

int
main(int ac, char **av)
{
    f();
    return 0;
}
```


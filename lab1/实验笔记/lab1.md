

参考官网：<https://pdos.csail.mit.edu/6.828/2017/labs/lab1/>

AT\&T汇编：<http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html>

## 启动操作系统

安装lab的os：

    git clone https://pdos.csail.mit.edu/6.828/2017/jos.git lab
    
    #报错处理：fatal:unable to access <url> : server certificate...
    #进入你的配置:
    vim ~/.zshrc
    #或者
    vim ~/.bashrc
    #添加以下代码来让你的电脑信任服务器
    export GIT_SSL_NO_VERIFY=1

编译

    #进入lab目录
    make && make qemu

make之后最后一行出现代表os内核映像编译成功

    + mk obj/kern/kernel.img

make qemu之后出现如下图说明qemu连接成功

![](https://tva1.sinaimg.cn/large/008i3skNgy1gt5qnshr76j314g0k8n1i.jpg)

打开gdb查看bios

    #lab目录下
    make qemu-gdb
    #再开一个相同路径的terminal
    make gdb
    #si命令逐步执行
    #关闭gdb：q
    #关闭qemu-gdb：ctrl + A 和 X,需要注意ctrl 与 A同时按住抬起后再按X,不要三个键同时按

## Part 1: PC Bootstrap

BIOS：Basic Input/Output System

物理内存：

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

可以看到bios从1MB结束，早期的PC如8088物理内存只有1MB（0x00000000-0x000fffff），现在这台x86扩展到了4GB。

打开gdb实际运行一些si命令（逐步执行机器码），可以看到：

    [f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
    #从接近bios顶部而不是960k地址进行长跳转到cs:0xf000(bios段基址),ip:0xe05b,反而往回跳
    0x0000fff0 in ?? ()
    + symbol-file obj/kern/kernel
    (gdb) si
    [f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x6ac8
    0x0000e05b in ?? ()

实验指导告诉我，bios在长跳转之后进行中断描述符表（IDT）的初始化和VGA等各种devices等初始化，于是继续查看得到以下内容：发现存在关中断（clean）命令。

![](https://tva1.sinaimg.cn/large/008i3skNgy1gt5t1uy8rlj30pc0i4go6.jpg)

然后接着BIOS读取boot loader，进行boot驱动

## Part 2: The Boot Loader

硬盘或者软盘最小单位为扇区（sector），一个扇区512字节，如果硬盘可以boot，那么这第一个扇区叫做bootsector，存放了boot loader 代码。

bios通过读取bootsector进入内存，从0x7c00-0x7dff.当然，现在不止读取一个扇区，也不止硬盘/软盘作为boot来源。

对于boot loader，实验已经准备好了boot/boot.S和boot/main.c来模拟。

对于boot loader，做两件事：

1.  从实模式到保护模式，即将物理内存从1MB扩展到4GB，这需要GDT

2.  从硬盘，通过x86的已经写好的IO驱动来使用IDE设备寄存器，从第二个扇区开始读取内核代码（obj/kern/kernel.asm）

### Exercise3

1.  通过GDB在boot入口0x7c00进行断点设置

2.  看 `boot/boot.S`里的代码, 看 `obj/boot/boot.asm` 来确定地址. 显示 boot loader在GDB中反汇编命令,比较三种汇编代码差异

3.  继续看boot/main.c 代码执行readsect()，找到c对应的汇编代码，紧接着代码执行回到bootmain函数 , 找到for循环（读取剩余内核代码），在for循环结束地址设置断点，然后单步执行，看看最终会做什么

回答以下问题:

*   代码从哪里开始执行32位保护模式？是什么导致了16位到32位到转换？

*   boot loader最后一个指令是什么,kernel第一个指令是什么？

*   kernel第一条指令的地址?

*   boot loader如何决定读取内核的所有扇区?从哪里能找到信息?

我们通过GDB来设置breakpoint来跟踪boot loader过程

![](https://tva1.sinaimg.cn/large/008i3skNly1gt5w9iq3t9j30ve0o4dis.jpg)

对比三个文件可以发现：GDB没有数据类型指定，如movb会执行为mov，同时boot.asm相较于boot.S少了头文件的引入标志，同时对于GDT只剩下CR0的开启

然后再看bootmain，调用两个函数：readseg和readsect

先定义了两个eh和eph，eph=eh+程序num

然后从硬盘读一页，判定elf的魔数来决定内核是否合乎ELF格式，紧接着根据eph和eh使用for循环来加载剩余内核。

最后进入根据ELF节头表进入内核执行的entry

### Exercise4

1.  推荐阅读有关C指针内容：*C Programming Language* by Brian Kernighanand Dennis Ritchie (known as 'K\&R')

2.  阅读从5.1到5.5，下载运行[以下C程序](https://pdos.csail.mit.edu/6.828/2017/labs/lab1/pointers.c)，确保你懂了所有输出结果原因，给出以下标准

        1.第一到第六行的输出地址
        2.第二到第四行输出结果
        3.第五行为什么输出错误

3.  这里还有一个[参考](https://pdos.csail.mit.edu/6.828/2017/readings/pointers.pdf)，不过不是很推荐

4.  这个阅读实验一定要做，否则后续的代码编写，你会知道什么叫做 “残忍”

5.  可以从本人的关键文件注释下参考代码解读

### Exercise5

1.  重新看boot loader开始一部分代码，如果你修改链接地址，那么会有一些错误，找到关于链接地址的错误，在boot/Makefrag里面修改

2.  使用make clean清除上次make缓存

3.  重新编译，运行代码，解释发生的错误情况

这里对于boot loader再做一些讲解，在main.c里面非常重要的就是那个ELFHDR，是二进制格式，并且对于ELF有以下[讲解](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)，布局如下：![](https://tva1.sinaimg.cn/large/008i3skNly1gt8litmokbj30ec090t9b.jpg)

可以在inc/elf.h里查看ELF布局，左边是链接视图，是目标代码格式，比如我们编写，链接重定位过程中使用。右边是可执行视图，在这里加载到内存时使用，并且section header table不是必须。（简单区分section和segment使用时机即可，详细看《深入理解计算机系统》）因此我们的loader根据ELF读取很多段，在硬盘中读取单位为扇区，详细看关键文件注释，先给出部分的解释：

*   .text 程序的执行命令集

*   .rodata 只读数据，比如ASCII，当然硬件并不会禁止写操作（？）

*   .data 已初始化的全局数据，比如全局定义x=5

*   .bss 未初始化的全局数据，在link阶段会留空间给他，并且.bss在程序运行时候必须全为0

编译时候将每个.c文件编译称.o文件，编码为硬件预期的二进制格式。在obj/kern/kernel里把所有的.o文件链接成二进制image（映像），转换为ELF格式，因此ELF英文解释为“可执行可链接的格式”。

对于本实验，你只需要考虑它作为header的功能（所需来自ELF header 和program headers table）：将后面的程序节加载入内存。boot loader不区分加载代码还是数据，直接加载然后执行就完事了。

我们可以输入以下指令进入内核program headers看所有节

    objdump -h obj/kern/kernel  
    //自己写的工具链就要多加i386-jos-elf-objdump

你会看到很多节，远超上面列出来的。包括program header记录但是没有写进内存的。

重点来了：前面所说出问题的“链接地址”在此称作”VMA“，我们可以看作逻辑地址，由于还没分段，那么逻辑地址应该等于物理地址，也就是加载地址“LMA”。我们输入指令得到

![](https://tva1.sinaimg.cn/large/008i3skNly1gt86375fgsj310o03ugmc.jpg)

看.text节。

接着还有以下两种指令供你参考：

    objdump -h obj/boot/boot.out //看boot被加载的section headers
    objdump -x obj/kern/kernel   //看内核所有headeres

第二条命令执行后，可以看出Program Headers的字样，加载入内存的节会显示LOAD，和一些其他的信息，比如“paddr”代表物理地址，“vaddr”代表虚拟地址，“-sz”结尾代表大小

假设你已经理解了boot.S文件对于VMA的作用和某某关系（下面会讲），那么我们关注的VMA如何改？在boot/Makefrag文件里，比如boot的VMA是0x7c00，那么对应文件里有-Ttext 0x7C00。

我将0x7c00改为0x2c00，会看到编译后的.asm从0x2c00开始boot，但是网上告诉我实际执行蹦过了这一段，头铁地接着从0x7c00开始运。运行一段时间后发现qemu-gdb报错![](https://tva1.sinaimg.cn/large/008i3skNly1gt85ybiufnj30ti07e76b.jpg)

很显然这里的GDT全0，导致了一些错误，那么我们的链接地址VMA对GDT的初始化出现了问题。

对于GDT初始化，查看代码可以发现有以下指令

![](https://tva1.sinaimg.cn/large/008i3skNly1gt86dwtrk8j30m80gedi1.jpg)

这里的lgdtw命令：将指定内存后的六个字节存放的GDTR寄存器里，发现存放的都是0，本应该是0x7c64，这里是第一个错误。

紧接着看：下面的ljmp会卡死，这条指令意思是以32位模式跳到下一条指令，本应该是0x7c32，但是同样出现了问题，后续的无法执行，即第二个错误。

### Exercise6

接着是内核的VMA，执行地址会指向高地址（VMA）但是实际上boot loader代码执行到低地址（LMA）。

看ELF header，里面的e_entry包含了就是我们的内核的（main.c里最后一句）VMA，可以通过以下指令查看entry的VMA

    objdump -f obj/kern/kernel //查看file header

![](https://tva1.sinaimg.cn/large/008i3skNly1gt96spmegtj30le04gaau.jpg)

![](https://tva1.sinaimg.cn/large/008i3skNly1gt96u5pk3rj30sa02ut99.jpg)

我们可以看到这里这两个地址的区别，原因就在于ELF得到的有GDT的线性地址，在没有页表下的解决方案：KERNBASE的删减。

    #The kernel (this code) is linked at address ~(KERNBASE + 1 Meg), 
    # but the bootloader loads it at address ~1 Meg.

假设你现在已经明白了main.c的过程，我们应该做一些事

1.  通过gdb 的x命令来查看内存，格式

        x/Nx ADDR //从指定地址以N个word（我这台机器里是4B 32位）查看内存

    更多命令请看[GDB手册](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html)

2.  在进入boot时候来通过以下指令查看

        x/8x 0x00100000  //内核代码存放

3.  然后再以相同方式看看kernel的进入时候的存放内容，告诉我为什么存放内容不同

在两个阶段查看结果如下：

![](https://tva1.sinaimg.cn/large/008i3skNly1gt97i7mrsoj312q0figp0.jpg)

如果你对boot干的事（1.保护模式2.load kernel sector）一清二楚的话，你就会知道，这个地址存放的八个字长内容不同是必然的。

## Part 3: The Kernel

接下来的exercise中你将要写一些C代码了！

**使用虚拟内存来在独立空间里工作**

让我们看看LMA和VMA，在进入内核时候我们就发现了VMA和LMA的开始不同：ELF头显示了0x10001c的执行地址但是boot loader给出了0x10018的调用。

不过现在要完成完整的LMA转换：分段分页：内核页表结构的生成

在kern/kernel.ld里，你就能看到VMA和LMA，一般内核喜欢让程序链接虚拟地址高地址，但是如何转化为内存物理地址呢？这种机制在lab2里会详细讲解。

对于本内核，VMA会给出0xf0100000，通过map分页得到物理地址0x00100000。现在先不说怎么做到，当然下个lab会教你如何在256MB限定下完成从虚拟地址0xf0000000 - 0xf0400000 映射物理地址0x00000000 - 0x00400000的分页map。

### Exercise7

1.  在内核kern/kernel.asm代码movl %eax, %cr0处设置断点，看看内存0x00100000 和0xf0100000
2.  单步执行，然后再看这两个内存，理解这其中的变化
3.  如果分页机制的映射错了，那么第一行错的指令是什么，看看源代码文件，找出来

**控制台的格式化输出**

C语言里有print（）格式化输出，同样I/O也有，它叫cprintf，阅读

    kern/printf.c, lib/printfmt.c,  kern/console.c

这三个文件（看关键文件注释）来明白怎么这三个文件的关系。如果在后续的lab中你知道了为什么printfmt.c在lib目录下，这个关系就会变得很清楚。

### Exercise8

1.  在cprintf（）方法里我们省略了一小段代码：使用“%o”来打印八进制，

回答以下问题：

1.  解释printf.c和console.c之间的接口，详细点：前者调用了什么功能，后者输出了什么功能

2.  解释下列关于console 的代码

         	if (crt_pos >= CRT_SIZE) {
                      int i;
                      memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
                      for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
                              crt_buf[i] = 0x0700 | ' ';
                      crt_pos -= CRT_COLS;
             }

3.  单步执行以下代码

        int x = 1, y = 3, z = 4;
        cprintf("x %d, y %x, z %d\n", x, y, z);

    *   在cprintf（），fmt指向什么，ap指向什么

    *   列出每个对于cons_putc, va_arg, 和vcprintf的调用

    *   cons_putc:列出形参

    *   va_arg:说出ap之前指向与调用之后的指向

    *   vcprintf:列出两个实参

4.  运行以下代码

        unsigned int i = 0x00646c72;
            cprintf("H%x Wo%s", 57616, &i);

    *   输出是什么

    *   单步执行解释输出的由来，有个[ASCII表](http://web.cs.mun.ca/\~michael/c/ascii-table.html)供你参考

    *   本次假设为小端，如果是大端你该怎么更改i的值？

5.  对于以下代码

        cprintf("x=%d y=%d", 3);

    “y=”之后应该输出什么（提示：不唯一）？为什么会这样？

6.  假设GCC改变调用约定（栈增长变为低地址->高地址），在（参数）声明顺序下堆栈传参，你要怎么改cprintf（）或者它的接口，来成功传参数？

7.  提高实验：通过ASCII来显示彩色字符

## 栈

### Exercise9

1.  决定内核在哪初始化，并且确定栈的位置

2.  内核代码如何为栈申请空间

3.  栈空间的end是栈指针初始化esp的参考吗？

我们知道栈是向下增长，具体为esp的逐步递减，在32位系统esp可以被4整除，这意味着esp的递减是4（比如0，4，8这样）。大量的指令比如call，通过使用这样的寄存器来访问地址

对于ebp寄存器，我们称之为“栈底”，很显然，指向高地址。比如在.c文件每个函数的调用时候，esp是通过指向ebp来完成函数栈的建立，随后esp递减。所以ebp用来定位多层函数时非常有用。因此相应的栈回溯功能是十分有必要的

### Exercise10

1.  为了了解函数调用，让我们追踪 `test_backtrace` 函数地址在 `obj/kern/kernel.asm`，设置断点，看看内核会做些什呢

2.  每次递归嵌套多少个word通过函数传入栈？他们分别是什么

3.  提示：你必须用本实验匹配的QEMU，不然你得自己翻译线性地址

### Exercise11

monitor函数会在qemu的monitor（显示器）中让你看到一些日志，其中backtrace就能实现这个功能，当你输入：backtrace时候，就会打印日志

1.  实现上述指定的backtrace函数。

对于这个exercise给你一些提示：你需要实现一个栈回溯功能，意味着你应该实现并调用mon_backtrace()，原型在kern/monitor.c里。同时read_ebp() 在inc/x86.h很有用。你应该让用户在交互时使用这个函数

这个栈回溯功能应该输出这个格式

    Stack backtrace:
      ebp f0109e58  eip f0100a62  args 00000001 f0109e80 f0109e98 f0100ed2 00000031
      ebp f0109ed8  eip f01000d6  args 00000000 00000000 f0100058 f0109f28 00000061
      ...

是不是很眼熟？对。gdb功能里 i r ebp eip也有类似的功能。

每行我们应该有ebp，eip，args。下面对格式进行一个简短的说明

*   ebp为函数调用后的栈底

*   eip为函数返回后的ip，偷偷告诉你，就在ebp上面

*   args后的五个十六进制是五个参数，可以传参少于这个数，但是仍然列出5个

然后我们对内容进行一个简短对说明

*   第一行反应了现在执行函数，也就是mon_backtrace

*   第二行是调用mon_backtrace对函数

*   第三行往后即再上层调用，以此类推

你应该打印合适的栈内容，从kern/entry.S中，确定什么时候终止打印

接下来是关于接下来实验的理论提示。这是对K\&R第五章的一些内容

*   Int \*p= (int \*)100, 则（int）p+1=101,(int) (p+1)=104

*   p\[i]=\*(p+i)

*   \&p\[i]=(p+i)

*   其实指针不难，你画图理解就行了

每当操作系统对内存地址描述时，谨记：是值还是指针（地址址）。

### Exercise12

1. monitor修改backtrace函数，增加显示：eip，函数名，源文件名，eip对应行数，在kernel/kdebug.c修改debuginfo_eip

2. 问题：在debuginfo_eip 中，__STAB_*是哪里来的？

   ```
   给出以下线索
   1.kern/kernel.ld找出_STAB_*
   2.运行objdump -h/-G kern/kernel
   3.gcc -pipe -nostdinc -O2 -fno-builtin -I. -MD -Wall -Wno-format -DJOS_KERNEL -gstabs -c -S kern/init.c,然后查看init.S
   4.确定bootloader在加载内核ELF时候是否把符号表加载进来
   ```

3. ```
   编写代码，让mon_backtrace能够调用debuginfo_eip，实现后者，输出格式如下：
   K> backtrace
   Stack backtrace:
     ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
            kern/monitor.c:143: monitor+106
     ebp f010ffd8  eip f0100193  args 00000000 00001aac 00000660 00000000 00000000
            kern/init.c:49: i386_init+59
     ebp f010fff8  eip f010003d  args 00000000 00000000 0000ffff 10cf9a00 0000ffff
            kern/entry.S:70: <unknown>+0
   K> 
   tips：printf("%.*s", length, string)可以让你输出指定长度的字符串，对文件名好用
   
   ```

   

## 回答问题汇总

### Exercise3

1.  .code32伪指令开始；ljmp语句切换，从16位地址模式切换到32位地址模式

2.

        7d81:	ff 15 18 00 01 00    	call   *0x10018
        0x10000c:	movw   $0x1234,0x472

3.  由GDB或者kernel/entry.S可以得知为0x10000c

4.  ELF存放了程序段数，段基址，段长

        //ph每个结点为一个段
        ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
        	eph = ph + ELFHDR->e_phnum;
        	for (; ph < eph; ph++)
        		// p_pa 是加载内核的程序段基址
        		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

### Exercise4

1.学习笔记

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

1.  解读答案看关键文件注释

### Exercise6

    刚进入boot时候BIOS上的地址没动过，此时从0x7c00开始boot会通过设置的保护模式将内核代码加载入内存，这时候高地址才会被修改，紧接着进入内核代码。

### Exercise7

1.![](https://tva1.sinaimg.cn/large/008i3skNly1gvgz3gjgpfj612g05qdh002.jpg)

2.单步执行后

![](https://tva1.sinaimg.cn/large/008i3skNly1gvgz4ywqwdj612y068myj02.jpg)

可以发现映射相同

问题：为什么？

A：对于GDB而言，在我们模拟的qemu里，内核代码加载的地址应该是物理地址，而用户代码加载的地址应该是虚拟地址。不过我们写的内核ELF都是按照用户代码给的高地址，应该稍微处理一下映射。

之前做的kernbase消除这一问题，等于做了个“假页表”，而题目给出的汇编代码即开启页表，开启之后的映射应该是准确映射物理地址，即“高地址映射到高地址”

3.关键代码即题目所给代码，如果注释，假页表映射有限，会出现访问越界问题

### Exercise8

1. ```
   //给出八进制代码
   num = getuint(&ap, lflag);
   			//设置基数为8
   			base = 8;
   			goto number;
   ```

   其余解释查看关键文件注释

2. console.c

```
if (crt_pos >= CRT_SIZE) {
        int i;
        memcpy(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
                crt_buf[i] = 0x0700 | ' ';
        crt_pos -= CRT_COLS;
 }
* 变量
crt_pos      CRT_SIZE    CRT_COLS          crt_buf
尾字符位置    页char大小    字符列数         字符buf

* 函数
memcpy 内存复制

* 过程
如果发现尾字符位置超过一页位置，就应该讲这一页向上一行进行复制，然后清空这腾出来的一行，然后修改尾字符位置向上一行
```

3. ```
   int x = 1, y = 3, z = 4;
   cprintf("x %d, y %x, z %d\n", x, y, z);
   
   * 调用cprintf，fmt什么内容，ap什么内容
   "x %d, y %x, z %d\n"，ap为va_list，即所有参数列表，包括了xyz
   
   * 按照执行的顺序列出所有对cons_putc, va_arg，和vcprintf的调用
   看注释
   
   * 列出每个对于cons_putc, va_arg, 和vcprintf的调用
   看注释
   
   * cons_putc:列出形参
   int c
   
   * va_arg:说出ap之前指向与调用之后的指向
   ap:1,3,4=>3,4
   
   * vcprintf:列出两个实参
   "x %d, y %x, z %d\n"  1,3,4
   ```

4. ```
       unsigned int i = 0x00646c72;
           cprintf("H%x Wo%s", 57616, &i);
   
   *   输出是什么
   He110 World
   
   *   单步执行解释输出的由来，有个[ASCII表](http://web.cs.mun.ca/\~michael/c/ascii-table.html)供你参考
   1.第一个参数的值是57616，它对应的16进制的表示形式为e110，所以前面就变成的He110。
   2.输出参数所指向的字符串,变量i代表字符串。但是i定义为int，所以我们按字节分析出char
   3.x86是小端模式，高字节放在高地址。所以存放顺序0x72('r')，0x6c('l')，0x64('d')，0x00('\0').
   5.所以在cprintf将会从i的地址开始一个字节一个字节遍历，正好输出 "World"
   
   *   本次假设为小端，如果是大端你该怎么更改i的值？
   i = 0x726c6400
   ```

5. 对于以下代码

       cprintf("x=%d y=%d", 3);
       
       * “y=”之后应该输出什么（提示：不唯一）？为什么会这样？
       getint调用va_arg时候，取，更改参数因为没有指定y，所以参数下标越界

6. ```
   假设GCC改变调用约定（栈增长变为低地址->高地址），在（参数）声明顺序下堆栈传参，你要怎么改cprintf（）或者它的接口，来成功传参数？
   形参反序
   ```

### Exercise9

给出obj/kern/kernel.asm中有关代码

```
1.内核为栈的开辟做准备：第一次没有esp，ebp，所以额外准备
f010002f:	bd 00 00 00 00       	mov    $0x0,%ebp

	# Set the stack pointer
	movl	$(bootstacktop),%esp
f0100034:	bc 00 00 11 f0       	mov    $0xf0110000,%esp

	# 之前内核的entry作初始化，为栈开辟作准备，然后我们正式开始C代码，为接下来内核的main做准备：先开辟main栈
	call	i386_init
f0100039:	e8 6c 00 00 00       	call   f01000aa <i386_init>


2.内核申请第一个栈：
void
i386_init(void)
{
f01000aa:	f3 0f 1e fb          	endbr32 
f01000ae:	55                   	push   %ebp
f01000af:	89 e5                	mov    %esp,%ebp
f01000b1:	53                   	push   %ebx
f01000b2:	83 ec 08             	sub    $0x8,%esp
//紧接着为新的ELF加载做准备：.bss,static之类，之前只是确定位置，并没有清零
```



### Exercise10

给出test_backtrace及其有关反汇编代码

```
//test_backtrace
void
test_backtrace(int x)
{
f0100040:	f3 0f 1e fb          	endbr32 
f0100044:	55                   	push   %ebp
f0100045:	89 e5                	mov    %esp,%ebp
f0100047:	56                   	push   %esi//第一个参数4B
f0100048:	53                   	push   %ebx//第二个参数4B
f0100049:	e8 7e 01 00 00       	call   f01001cc <__x86.get_pc_thunk.bx>
f010004e:	81 c3 ba 12 01 00    	add    $0x112ba,%ebx
f0100054:	8b 75 08             	mov    0x8(%ebp),%esi//看下面的eax
	cprintf("entering test_backtrace %d\n", x);
f0100057:	83 ec 08             	sub    $0x8,%esp
f010005a:	56                   	push   %esi
f010005b:	8d 83 d8 07 ff ff    	lea    -0xf828(%ebx),%eax
f0100061:	50                   	push   %eax
f0100062:	e8 26 0a 00 00       	call   f0100a8d <cprintf>
	if (x > 0)
f0100067:	83 c4 10             	add    $0x10,%esp
f010006a:	85 f6                	test   %esi,%esi
f010006c:	7e 29                	jle    f0100097 <test_backtrace+0x57>//x==0
		test_backtrace(x-1);
f010006e:	83 ec 0c             	sub    $0xc,%esp
f0100071:	8d 46 ff             	lea    -0x1(%esi),%eax//x-1
f0100074:	50                   	push   %eax//callee会将eax作为返回值使用，caller为callee传入x-1保存在这里
f0100075:	e8 c6 ff ff ff       	call   f0100040 <test_backtrace>//递归调用
f010007a:	83 c4 10             	add    $0x10,%esp
	else
		mon_backtrace(0, 0, 0);
	cprintf("leaving test_backtrace %d\n", x);
f010007d:	83 ec 08             	sub    $0x8,%esp
f0100080:	56                   	push   %esi
f0100081:	8d 83 f4 07 ff ff    	lea    -0xf80c(%ebx),%eax
f0100087:	50                   	push   %eax
f0100088:	e8 00 0a 00 00       	call   f0100a8d <cprintf>
}
f010008d:	83 c4 10             	add    $0x10,%esp
f0100090:	8d 65 f8             	lea    -0x8(%ebp),%esp
f0100093:	5b                   	pop    %ebx
f0100094:	5e                   	pop    %esi
f0100095:	5d                   	pop    %ebp
f0100096:	c3                   	ret    

//		mon_backtrace(0, 0, 0);
f0100097:	83 ec 04             	sub    $0x4,%esp
f010009a:	6a 00                	push   $0x0
f010009c:	6a 00                	push   $0x0
f010009e:	6a 00                	push   $0x0
f01000a0:	e8 0c 08 00 00       	call   f01008b1 <mon_backtrace>
f01000a5:	83 c4 10             	add    $0x10,%esp
f01000a8:	eb d3                	jmp    f010007d <test_backtrace+0x3d>//返回test_backtrace


//调用test_backtrace
	// Test the stack backtrace function (lab 1 only)
	test_backtrace(5);
f01000f0:	c7 04 24 05 00 00 00 	movl   $0x5,(%esp)
f01000f7:	e8 44 ff ff ff       	call   f0100040 <test_backtrace>
```



### Exercise11

给出mon_backtrace的补全代码

```
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)//参数个数，参数列表，还有一个不知道啥，不影响
{
	int *ebp = (int *)read_ebp();//read得到ebp寄存器的值，然后转为指针：就像寄存器那样干
	
	cprintf("Stack backtrace:\n");
	while((int)ebp != 0x0) {//end of print：main栈前
		cprintf("  ebp %08x", *ebp);
		cprintf("  eip %08x", *(ebp+1));
		cprintf("  args");
		cprintf(" %08x", *(ebp+2));
		cprintf(" %08x", *(ebp+3));
		cprintf(" %08x", *(ebp+4));
		cprintf(" %08x", *(ebp+5));
		cprintf(" %08x\n", *(ebp+6));//5个参数
		ebp = (int *)(*ebp);//别忘了类型转换
	}
	return 0;
}
```



### Exercise12

给出以下代码

```
//更详细代码看注释
//backtrace
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	int *ebp = (int *)read_ebp();//like %ebp
	cprintf("Stack backtrace:\n");
	struct Eipdebuginfo info;//文件string信息，定义在kdebug.h

	while (ebp != 0x0) {
		// 第一行的当前ebp和栈环境是mon_backtrace的
		cprintf("ebp %8x eip %8x args %08x %08x %08x %08x %08x ", 
				ebp, ebp[1], ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
		debuginfo_eip(ebp[1], &info);
		cprintf("%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, 
				info.eip_fn_name, ebp[1]-info.eip_fn_addr);
		ebp = (int *)ebp[0];
	}
	return 0;
}


//Eipdebuginfo
struct Eipdebuginfo {
	const char *eip_file;		// Eip所在文件名
	int eip_line;			// eip所在文件源代码行数

	const char *eip_fn_name;	// eip所在函数名
					//  - Note: not null terminated!
	int eip_fn_namelen;		// 函数长度
	uintptr_t eip_fn_addr;		// 函数开始地址
	int eip_fn_narg;		// 函数的参数个数
};

//stab
struct Stab {
	uint32_t n_strx;	// index into string table of name
	uint8_t n_type;         // 类型,比如fun，so
	uint8_t n_other;        // misc info (usually empty)
	uint16_t n_desc;        // 给定类型下的描述域,题目下是代码长度
	uintptr_t n_value;	// 符号值，本题当作地址
}


//debuginfo_eip增加内容 
stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);//二分搜索得到代码行数
if (lline <= rline) {
    info->eip_line = stabs[lline].n_desc;
} else {
    return -1;
}
```

给定符号表stab的例子更直观查看![](https://tva1.sinaimg.cn/large/008i3skNly1gvhgr7zagaj615a0gs0vx02.jpg)

happy！

![](https://tva1.sinaimg.cn/large/008i3skNly1gvhfsjm5x2j60og08kgmt02.jpg)

## 关键文件注释

### boot/boot.S

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

### boot/main.c

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

### kern/printf.c

```
//基于printfmt格式化和cputchar内核控制台输出，来完成内核输出到控制台的功能

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>


static void
putch(int ch, int *cnt)//3.调用cputchar
{
	cputchar(ch);//内核态向控制台输出
	*cnt++;
}

int
vcprintf(const char *fmt, va_list ap)//2.调用vprintfmt，输入putch返回值作参数
{
	int cnt = 0;

	vprintfmt((void*)putch, &cnt, fmt, ap);//四个参数：1.函数指针2.计数3.格式4.参数列表
	return cnt;
}

int
cprintf(const char *fmt, ...)//1.调用cprintf
{
	va_list ap;//参数列表
	int cnt;

	va_start(ap, fmt);//把xyz这样的参数使用ap指向
	cnt = vcprintf(fmt, ap);//传入格式，参数列表
	va_end(ap);

	return cnt;
}

```

### lib/printfmt.c

```
// 分析输出格式：词法分析的感觉，根据格式要求来决定参数的合法判定，输出格式
// 常见于printf, sprintf, fprintf, etc.
// 内核态和用户态都有调用

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#include <inc/error.h>

/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 */

static const char * const error_string[MAXERROR] =
{
	[E_UNSPECIFIED]	= "unspecified error",
	[E_BAD_ENV]	= "bad environment",
	[E_INVAL]	= "invalid parameter",
	[E_NO_MEM]	= "out of memory",
	[E_NO_FREE_ENV]	= "out of environments",
	[E_FAULT]	= "segmentation fault",
};

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void
printnum(void (*putch)(int, void*), void *putdat,
	 unsigned long long num, unsigned base, int width, int padc)
{
	// first recursively print all preceding (more significant) digits
	if (num >= base) {
		printnum(putch, putdat, num / base, base, width - 1, padc);
	} else {
		// print any needed pad characters before first digit
		while (--width > 0)
			putch(padc, putdat);
	}

	// then print this (the least significant) digit
	putch("0123456789abcdef"[num % base], putdat);
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
static unsigned long long
getuint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if (lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

// Same as getuint but signed - can't use getuint
// because of sign extension
static long long//被调用，来解决诸如%d匹配参数的问题，隐性进行合法判定
getint(va_list *ap, int lflag)//ap:1,3,4
{
	if (lflag >= 2)
		return va_arg(*ap, long long);//ap:3,4
	else if (lflag)
		return va_arg(*ap, long);//ap:4
	else
		return va_arg(*ap, int);//ap:null
}


// 格式化，并且调用putchar输出字符串
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);//参数：putch,&cnt,fmt,ap

void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
	register const char *p;
	register int ch, err;
	unsigned long long num;
	int base, lflag, width, precision, altflag;
	char padc;

	while (1) {
	//逐字符分析fmt：根据%后的字符决定参数，将ap参数逐步加入划分中，题例划分："x %d, y %x, z %d\n", 1, 3, 4 => "x %d:1, y %x:2, z %d:3" "\n"四个部分
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0')
				return;
			putch(ch, putdat);
		}

		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {

		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0; ; ++fmt) {
				precision = precision * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0)
				width = 0;
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;

		process_precision:
			if (width < 0)
				width = precision, precision = -1;
			goto reswitch;

		// long flag (doubled for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putch(va_arg(ap, int), putdat);
			break;

		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0)
				err = -err;
			if (err >= MAXERROR || (p = error_string[err]) == NULL)
				printfmt(putch, putdat, "error %d", err);
			else
				printfmt(putch, putdat, "%s", p);
			break;

		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
				p = "(null)";
			if (width > 0 && padc != '-')
				for (width -= strnlen(p, precision); width > 0; width--)
					putch(padc, putdat);
			for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
				if (altflag && (ch < ' ' || ch > '~'))
					putch('?', putdat);
				else
					putch(ch, putdat);
			for (; width > 0; width--)
				putch(' ', putdat);
			break;

		// (signed) decimal
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putch('-', putdat);
				num = -(long long) num;
			}
			base = 10;
			goto number;

		// unsigned decimal
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		// (unsigned) octal
		case 'o':
			// Replace this with your code.
			putch('X', putdat);
			putch('X', putdat);
			putch('X', putdat);
			break;

		// pointer
		case 'p':
			putch('0', putdat);
			putch('x', putdat);
			num = (unsigned long long)
				(uintptr_t) va_arg(ap, void *);
			base = 16;
			goto number;

		// (unsigned) hexadecimal
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putch, putdat, num, base, width, padc);
			break;

		// escaped '%' character
		case '%':
			putch(ch, putdat);
			break;

		// unrecognized escape sequence - just print it literally
		default:
			putch('%', putdat);
			for (fmt--; fmt[-1] != '%'; fmt--)
				/* do nothing */;
			break;
		}
	}
}

void
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
}

struct sprintbuf {
	char *buf;
	char *ebuf;
	int cnt;
};

static void
sprintputch(int ch, struct sprintbuf *b)
{
	b->cnt++;
	if (b->buf < b->ebuf)
		*b->buf++ = ch;
}

int
vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	struct sprintbuf b = {buf, buf+n-1, 0};

	if (buf == NULL || n < 1)
		return -E_INVAL;

	// print the string to the buffer
	vprintfmt((void*)sprintputch, &b, fmt, ap);

	// null terminate the buffer
	*b.buf = '\0';

	return b.cnt;
}

int
snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}


```

### Kern/console.c

```
/* See COPYRIGHT for copyright information. */
//控制台的IO处理逻辑：I，O，控制台

#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/kbdreg.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/console.h>

static void cons_intr(int (*proc)(void));
static void cons_putc(int c);

// PC设计历史原因，只是单纯的IO等待
static void
delay(void)
{
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

/***** IO代码 *****/

#define COM1		0x3F8

#define COM_RX		0	// In:	Receive buffer (DLAB=0)
#define COM_TX		0	// Out: Transmit buffer (DLAB=0)
#define COM_DLL		0	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM		1	// Out: Divisor Latch High (DLAB=1)
#define COM_IER		1	// Out: Interrupt Enable Register
#define   COM_IER_RDI	0x01	//   Enable receiver data interrupt
#define COM_IIR		2	// In:	Interrupt ID Register
#define COM_FCR		2	// Out: FIFO Control Register
#define COM_LCR		3	// Out: Line Control Register
#define	  COM_LCR_DLAB	0x80	//   Divisor latch access bit
#define	  COM_LCR_WLEN8	0x03	//   Wordlength: 8 bits
#define COM_MCR		4	// Out: Modem Control Register
#define	  COM_MCR_RTS	0x02	// RTS complement
#define	  COM_MCR_DTR	0x01	// DTR complement
#define	  COM_MCR_OUT2	0x08	// Out2 complement
#define COM_LSR		5	// In:	Line Status Register
#define   COM_LSR_DATA	0x01	//   Data available
#define   COM_LSR_TXRDY	0x20	//   Transmit buffer avail
#define   COM_LSR_TSRE	0x40	//   Transmitter off

static bool serial_exists;

static int
serial_proc_data(void)
{
	if (!(inb(COM1+COM_LSR) & COM_LSR_DATA))
		return -1;
	return inb(COM1+COM_RX);
}

void
serial_intr(void)
{
	if (serial_exists)
		cons_intr(serial_proc_data);
}

static void
serial_putc(int c)
{
	int i;

	for (i = 0;
	     !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800;
	     i++)
		delay();

	outb(COM1 + COM_TX, c);
}

static void
serial_init(void)
{
	// Turn off the FIFO
	outb(COM1+COM_FCR, 0);

	// Set speed; requires DLAB latch
	outb(COM1+COM_LCR, COM_LCR_DLAB);
	outb(COM1+COM_DLL, (uint8_t) (115200 / 9600));
	outb(COM1+COM_DLM, 0);

	// 8 data bits, 1 stop bit, parity off; turn off DLAB latch
	outb(COM1+COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

	// No modem controls
	outb(COM1+COM_MCR, 0);
	// Enable rcv interrupts
	outb(COM1+COM_IER, COM_IER_RDI);

	// Clear any preexisting overrun indications and interrupts
	// Serial port doesn't exist if COM_LSR returns 0xFF
	serial_exists = (inb(COM1+COM_LSR) != 0xFF);
	(void) inb(COM1+COM_IIR);
	(void) inb(COM1+COM_RX);

}



/***** 并行端口输出代码 *****/
// For information on PC parallel port programming, see the class References
// page.

static void
lpt_putc(int c)
{
	int i;

	for (i = 0; !(inb(0x378+1) & 0x80) && i < 12800; i++)
		delay();
	outb(0x378+0, c);
	outb(0x378+2, 0x08|0x04|0x01);
	outb(0x378+2, 0x08);
}




/***** 以文字形式向 CGA/VGA 显示输出 *****/

static unsigned addr_6845;
static uint16_t *crt_buf;
static uint16_t crt_pos;

static void
cga_init(void)
{
	volatile uint16_t *cp;
	uint16_t was;
	unsigned pos;

	cp = (uint16_t*) (KERNBASE + CGA_BUF);
	was = *cp;
	*cp = (uint16_t) 0xA55A;
	if (*cp != 0xA55A) {
		cp = (uint16_t*) (KERNBASE + MONO_BUF);
		addr_6845 = MONO_BASE;
	} else {
		*cp = was;
		addr_6845 = CGA_BASE;
	}

	/* Extract cursor location */
	outb(addr_6845, 14);
	pos = inb(addr_6845 + 1) << 8;
	outb(addr_6845, 15);
	pos |= inb(addr_6845 + 1);

	crt_buf = (uint16_t*) cp;
	crt_pos = pos;
}



static void
cga_putc(int c)
{
	// if no attribute given, then use black on white
	if (!(c & ~0xFF))
		c |= 0x0700;

	switch (c & 0xff) {
	case '\b':
		if (crt_pos > 0) {
			crt_pos--;
			crt_buf[crt_pos] = (c & ~0xff) | ' ';
		}
		break;
	case '\n':
		crt_pos += CRT_COLS;
		/* fallthru */
	case '\r':
		crt_pos -= (crt_pos % CRT_COLS);
		break;
	case '\t':
		cons_putc(' ');
		cons_putc(' ');
		cons_putc(' ');
		cons_putc(' ');
		cons_putc(' ');
		break;
	default:
		crt_buf[crt_pos++] = c;		/* write the character */
		break;
	}

	// What is the purpose of this?
	if (crt_pos >= CRT_SIZE) {
		int i;

		memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
		for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
			crt_buf[i] = 0x0700 | ' ';
		crt_pos -= CRT_COLS;
	}

	/* move that little blinky thing */
	outb(addr_6845, 14);
	outb(addr_6845 + 1, crt_pos >> 8);
	outb(addr_6845, 15);
	outb(addr_6845 + 1, crt_pos);
}


/***** 键盘输入处理 *****/

#define NO		0

#define SHIFT		(1<<0)
#define CTL		(1<<1)
#define ALT		(1<<2)

#define CAPSLOCK	(1<<3)
#define NUMLOCK		(1<<4)
#define SCROLLLOCK	(1<<5)

#define E0ESC		(1<<6)

static uint8_t shiftcode[256] =
{
	[0x1D] = CTL,
	[0x2A] = SHIFT,
	[0x36] = SHIFT,
	[0x38] = ALT,
	[0x9D] = CTL,
	[0xB8] = ALT
};

static uint8_t togglecode[256] =
{
	[0x3A] = CAPSLOCK,
	[0x45] = NUMLOCK,
	[0x46] = SCROLLLOCK
};

static uint8_t normalmap[256] =
{
	NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',	// 0x00
	'7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
	'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',	// 0x10
	'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
	'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',	// 0x20
	'\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
	'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,	      [0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,      [0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	      [0xCB] = KEY_LF,
	[0xCD] = KEY_RT,	      [0xCF] = KEY_END,
	[0xD0] = KEY_DN,	      [0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,	      [0xD3] = KEY_DEL
};

static uint8_t shiftmap[256] =
{
	NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',	// 0x00
	'&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
	'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',	// 0x10
	'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
	'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',	// 0x20
	'"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
	'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,	      [0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,      [0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	      [0xCB] = KEY_LF,
	[0xCD] = KEY_RT,	      [0xCF] = KEY_END,
	[0xD0] = KEY_DN,	      [0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,	      [0xD3] = KEY_DEL
};

#define C(x) (x - '@')

static uint8_t ctlmap[256] =
{
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
	C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
	C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
	NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
	C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
	[0x97] = KEY_HOME,
	[0xB5] = C('/'),		[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,		[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,		[0xCF] = KEY_END,
	[0xD0] = KEY_DN,		[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,		[0xD3] = KEY_DEL
};

static uint8_t *charcode[4] = {
	normalmap,
	shiftmap,
	ctlmap,
	ctlmap
};

/*
 * 从键盘获取单个字符，如果成功，返回，如果没有，返回0.
 * 无数据返回-1.
 */
static int
kbd_proc_data(void)
{
	int c;
	uint8_t stat, data;
	static uint32_t shift;

	stat = inb(KBSTATP);
	if ((stat & KBS_DIB) == 0)
		return -1;
	// Ignore data from mouse.
	if (stat & KBS_TERR)
		return -1;

	data = inb(KBDATAP);

	if (data == 0xE0) {
		// E0 escape character
		shift |= E0ESC;
		return 0;
	} else if (data & 0x80) {
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	} else if (shift & E0ESC) {
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];

	c = charcode[shift & (CTL | SHIFT)][data];
	if (shift & CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	// Process special keys
	// Ctrl-Alt-Del: reboot
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
		cprintf("Rebooting!\n");
		outb(0x92, 0x3); // courtesy of Chris Frost
	}

	return c;
}

void
kbd_intr(void)
{
	cons_intr(kbd_proc_data);
}

static void
kbd_init(void)
{
}



/***** 设备无关的控制台代码 *****/
// Here we manage the console input buffer,
// where we stash characters received from the keyboard or serial port
// whenever the corresponding interrupt occurs.

#define CONSBUFSIZE 512

static struct {
	uint8_t buf[CONSBUFSIZE];
	uint32_t rpos;
	uint32_t wpos;
} cons;

// called by device interrupt routines to feed input characters
// into the circular console input buffer.
static void
cons_intr(int (*proc)(void))
{
	int c;

	while ((c = (*proc)()) != -1) {
		if (c == 0)
			continue;
		cons.buf[cons.wpos++] = c;
		if (cons.wpos == CONSBUFSIZE)
			cons.wpos = 0;
	}
}

// return the next input character from the console, or 0 if none waiting
int
cons_getc(void)
{
	int c;

	// poll for any pending input characters,
	// so that this function works even when interrupts are disabled
	// (e.g., when called from the kernel monitor).
	serial_intr();
	kbd_intr();

	// grab the next character from the input buffer.
	if (cons.rpos != cons.wpos) {
		c = cons.buf[cons.rpos++];
		if (cons.rpos == CONSBUFSIZE)
			cons.rpos = 0;
		return c;
	}
	return 0;
}

// output a character to the console
static void
cons_putc(int c)
{
	serial_putc(c);
	lpt_putc(c);
	cga_putc(c);
}

// initialize the console devices
void
cons_init(void)
{
	cga_init();
	kbd_init();
	serial_init();

	if (!serial_exists)
		cprintf("Serial port does not exist!\n");
}


// `High'-level console I/O.  Used by readline and cprintf.

void
cputchar(int c)
{
	cons_putc(c);
}

int
getchar(void)
{
	int c;

	while ((c = cons_getc()) == 0)
		/* do nothing */;
	return c;
}

int
iscons(int fdnum)
{
	// used by readline
	return 1;
}
```



### kern/kdebug.c

```
#include <inc/stab.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>

#include <kern/kdebug.h>

extern const struct Stab __STAB_BEGIN__[];	// Beginning of stabs table
extern const struct Stab __STAB_END__[];	// End of stabs table
extern const char __STABSTR_BEGIN__[];		// Beginning of string table
extern const char __STABSTR_END__[];		// End of string table

//Stab是一个增序的指令数组，包含了大量关于代码执行的诸如起始地址，函数名，等我们需要的信息，因此info需要stab数组来填充
//1.stab_binsearch(stabs, region_left, region_right, type, addr)二分搜索指令地址，查找指定代码地址信息
//	给以下例子，
//		Index  Type   Address
//		0      SO     f0100000
//		13     SO     f0100040
//		117    SO     f0100176
//		118    SO     f0100178
//		555    SO     f0100652
//		556    SO     f0100654
//		657    SO     f0100849
//	给定参数:
//		left = 0, right = 657;为左闭右闭边界
//		stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
//	结果得到 left = 118, right = 554.，因此left是指令所在索引，right是下个指令索引，所以left>right代表失败

static void
stab_binsearch(const struct Stab *stabs, int *region_left, int *region_right,
	       int type, uintptr_t addr)
{
	int l = *region_left, r = *region_right, any_matches = 0;

	while (l <= r) {//l不同于*region_left
		int true_m = (l + r) / 2, m = true_m;

		// 过滤不正确的type节省时间，中间值向左偏移
		while (m >= l && stabs[m].n_type != type)
			m--;
		if (m < l) {	// no match in [l, m]
			l = true_m + 1;
			continue;
		}

		// 实际的二分搜索代码
		any_matches = 1;
		if (stabs[m].n_value < addr) {
			*region_left = m;
			l = true_m + 1;
		} else if (stabs[m].n_value > addr) {
			*region_right = m - 1;
			r = m - 1;
		} else {
			// exact match for 'addr', but continue loop to find
			// *region_right
			*region_left = m;
			l = m;
			addr++;
		}
	}

	if (!any_matches)
		*region_right = *region_left - 1;//非法判定成立准备
	else {
		// 由于符号表中存在地址相同的可能，所以排列情况下，我们要的就是最右的那个
		for (l = *region_right;
		     l > *region_left && stabs[l].n_type != type;
		     l--)
			/* do nothing */;
		*region_left = l;//赋值
	}
}

//2.debuginfo_eip调用二分搜索来填充info，成功返回0，失败返回-1，但是失败的时候，info可能也填充了一部分
// debuginfo_eip(addr, info)
//
//	Fill in the 'info' structure with information about the specified
//	instruction address, 'addr'.  Returns 0 if information was found, and
//	negative if not.  But even if it returns negative it has stored some
//	information into '*info'.
//
int
debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
{
	const struct Stab *stabs, *stab_end;
	const char *stabstr, *stabstr_end;
	int lfile, rfile, lfun, rfun, lline, rline;

	// Initialize *info
	info->eip_file = "<unknown>";
	info->eip_line = 0;
	info->eip_fn_name = "<unknown>";
	info->eip_fn_namelen = 9;
	info->eip_fn_addr = addr;
	info->eip_fn_narg = 0;

	// Find the relevant set of stabs
	if (addr >= ULIM) {
		stabs = __STAB_BEGIN__;
		stab_end = __STAB_END__;
		stabstr = __STABSTR_BEGIN__;
		stabstr_end = __STABSTR_END__;
	} else {
		// Can't search for user-level addresses yet!
  	        panic("User address");
	}

	// String table validity checks
	if (stabstr_end <= stabstr || stabstr_end[-1] != 0)
		return -1;

	// Now we find the right stabs that define the function containing
	// 'eip'.  First, we find the basic source file containing 'eip'.
	// Then, we look in that source file for the function.  Then we look
	// for the line number.

	// Search the entire set of stabs for the source file (type N_SO).
	lfile = 0;
	rfile = (stab_end - stabs) - 1;
	stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
	if (lfile == 0)
		return -1;

	// Search within that file's stabs for the function definition
	// (N_FUN).
	lfun = lfile;
	rfun = rfile;
	stab_binsearch(stabs, &lfun, &rfun, N_FUN, addr);

	if (lfun <= rfun) {
		// stabs[lfun] points to the function name
		// in the string table, but check bounds just in case.
		if (stabs[lfun].n_strx < stabstr_end - stabstr)
			info->eip_fn_name = stabstr + stabs[lfun].n_strx;
		info->eip_fn_addr = stabs[lfun].n_value;
		addr -= info->eip_fn_addr;
		// Search within the function definition for the line number.
		lline = lfun;
		rline = rfun;
	} else {
		// Couldn't find function stab!  Maybe we're in an assembly
		// file.  Search the whole file for the line number.
		info->eip_fn_addr = addr;
		lline = lfile;
		rline = rfile;
	}
	// Ignore stuff after the colon.
	info->eip_fn_namelen = strfind(info->eip_fn_name, ':') - info->eip_fn_name;


	// 给定边界[lline, rline] 
	// 找到之后， info->eip_line 赋值为rline代码块的行数
	// 失败返回-1
	//
	// Hint:
	//	对于代码行数，stab已经有特定的字段保存，你只要给addr参数即可，不需要再计算
	//	从<inc/stab.h>找stab定义
	// 我的代码
	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);//代码行数
	if (lline <= rline) {
    	info->eip_line = stabs[rline].n_desc;
	} else {
 	   return -1;
	}

	// Search backwards from the line number for the relevant filename
	// stab.
	// We can't just use the "lfile" stab because inlined functions
	// can interpolate code from a different file!
	// Such included source files use the N_SOL stab type.
	while (lline >= lfile
	       && stabs[lline].n_type != N_SOL
	       && (stabs[lline].n_type != N_SO || !stabs[lline].n_value))
		lline--;
	if (lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
		info->eip_file = stabstr + stabs[lline].n_strx;


	// Set eip_fn_narg to the number of arguments taken by the function,
	// or 0 if there was no containing function.
	if (lfun < rfun)
		for (lline = lfun + 1;
		     lline < rfun && stabs[lline].n_type == N_PSYM;
		     lline++)
			info->eip_fn_narg++;

	return 0;
}
```


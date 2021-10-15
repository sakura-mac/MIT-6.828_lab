# 大纲

- PC结构
- x86命令集
- gcc调用惯例
- PC模拟

## PC结构

- 一个完成的PC包括

  - x86CPU，包括寄存器，执行单元，内存管理（比如mmu寻址）
  - CPU芯片，包括地址和数据信号
  - 内存
  - 硬盘
  - 键盘
  - 显示器
  - 其他的资源：引导ROM，时钟，等

- 我们将会以出事的16位8086CPU开始

- CPU以这样的一个指令开始

  ```
  for(;;){
  	run next instruction
  	}//即顺序执行代码
  ```

- 同时需要一些寄存器

  - 16位的：AX, BX, CX, DX
  - 每个都是有8位的比如AL，AH
  - 小而快：相对于内存来说

- 更大的空间：内存

  - CPU通过地址总线发送地址（每位发送1bit）
  - 数据从数据总线传送，发送或者返回

- 添加地址寄存器：指针

  - SP：栈顶指针
  - BP：栈底指针
  - SI：源索引
  - DI：目标索引

- 不仅数据在内存中，指令也在内存中

  - IP：指令指针
  - 根据IP顺序执行每个指令
  - IP能够被CALL，RET，JMP，条件转移来修改IP

- 如何进行条件转移？

  - FLAGS：一些条件命令
    - of：溢出
    - sf：正负
    - zf：0判定
    - cf：进位（跟溢出不一样，比如8位带符号中128就是一出，而无符号256就是进位）
    - 等等
    - if：中断
    - df：方向（就是IP从地址的小到大或者从大到小）
  - JP，JN，J[N]Z，J[N]C，J[N]O...这些指令来条件转移，后跟地址

- IO

  - 原始PC结构：使用执行的IO空间

    - 同内存访问没什么区别，加个signal而已

    - 只有1024IO地址：即10位

    - 使用特殊的比如IN，OUT指令访问

    - 例子：你应该寻找里面的访问代码，以及函数的功能

      ```
      #define DATA_PORT    0x378
      #define STATUS_PORT  0x379
      #define   BUSY 0x80
      #define CONTROL_PORT 0x37A
      #define   STROBE 0x01
      void
      lpt_putc(int c)
      {
        /* wait for printer to consume previous byte */
        while((inb(STATUS_PORT) & BUSY) == 0)
          ;
      
        /* put the byte on the parallel lines */
        outb(DATA_PORT, c);
      
        /* tell the printer to look at the data */
        outb(CONTROL_PORT, STROBE);
        outb(CONTROL_PORT, 0);
      }
      ```

  - 内存定位IO

    - 使用物理地址
      - 有限的IO空间
      - 不需要特殊指令
      - 使用系统控制器传送到特殊设备
    - 魔法内存：？？？
      - 访问有副作用
      - 读取产生永久改变

- 如果想要超过16位的访问呢？

  - 8086有20位的地址，也就是1Mb内存
  - 因此在16位基础上衍生段寄存器的概念
  - CS：代码段，左移4位（16进制1位）加上额外的IP来访问
  - SS：栈段，加上偏移的SP或BP来访问
  - DS：数据段，加上偏移的其他寄存器来访问
  - ES：额外段，对于字符串操作用
  - 因此虚拟地址为pa=va+seg*16
  - 举个例子，对于65535，CS为4096
  - 坑：跨越边界的指针访问

- 但是还不够，内存依旧太小

  - 80386提供32位
  - 为了兼容性，从16位开始，到保护模式扩展到32位
  - 寄存器从AX到EAX
  - 操作数和地址都因此是32位，（这也意味着指针存放的一些限制），段地址并不再乘，而是直接相加偏移量
  - 为了区别32/16，使用隐含（汇编层面）前缀区别，32位为0x66 ，另一个0x67，这个完全不用记，看不到也没啥意义
  - 转换的开始：.code32，即开始了隐含的前缀，
  - 80386不仅分段，同时也有分页操作

- 例子

  ```
  b8 cd ab		16-bit CPU,  AX <- 0xabcd
  b8 34 12 cd ab		32-bit CPU, EAX <- 0xabcd1234
  66 b8 cd ab		32-bit CPU,  AX <- 0xabcd
  ```

## X86物理内存

- 物理地址空间像传统RAM一样
- 除了一些低地址指向其他东西
- 写VGA部分的内存开始显示，可以被观察
- 重启或者启动-从0xfffffff0的ROM（所以顶部一定是ROM？）

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
+------------------+  <- 0x0000000
```

## X86 指令集

- Intel语法：op dst，src

- AT&Tyufa：op src，dst（相反）

  操作数是寄存器，常量，指针（通过寄存器访问内存），通过内存访问的常量

- 例子

  ```
  AT&T syntax	"C"-ish equivalent
  
  movl %eax, %edx	edx = eax;	register mode
  movl $0x123, %edx	edx = 0x123;	immediate
  movl 0x123, %edx	edx = *(int32_t*)0x123;	direct
  movl (%ebx), %edx	edx = *(int32_t*)ebx;	indirect
  movl 4(%ebx), %edx	edx = *(int32_t*)(ebx+4);	displaced
  ```

- 指令类型

  - 数据移动：MOV，PUSH，POP。。。
  - 算数统计：TEST，SHL，ADD，AND。。。
  - IO：IN，OUT。。。
  - 控制：JMP，JZ，JNZ，CALL，RET
  - 字符串：REP MOVSB。。。
  - 系统：IRET，INT
  - intel架构第二册是参考

## gcc x86 调用惯例

- x86 指定了栈向低地址增长：记住每个指令的对应指令

  ```
  //左边是指令，实际上的干的在右边，对，我们写的汇编也可以不维护栈，会自动生成一些
  pushl %eax	      subl $4, %esp//栈增长
  									movl %eax, (%esp)
  									
  popl %eax					movl (%esp), %eax
  									addl $4, %esp//栈减小
  									
  call 0x12345			pushl %eip (*)
  									movl $0x12345, %eip (*)//call的指令就是eip压栈
  									
  ret								popl %eip (*)//返回，但是实际上指令更多：需要返回到原eip，这代表了eip，ebp，esp都需要改变，不止这一条指令
  //*代表了这是人工造的例子
  ```

- GCC指定栈的使用。约定了x86中调用和被调用的情况

  - call指令：
    - eip压栈
    - 由于在每条指令执行后eip增加，所以返回时pop执行后eip变为call的下一条指令
  
  - 对于一个函数的入口：call之后
  
    - %eip指向函数的第一条命令：新eip
    - 原%ebp压栈，新%ebp指向这部分，标志着函数栈底
    - 每次%esp在存放之前扩展，删除之后缩小，标志着栈顶
    - CPU续取址执行
  
  - 在ret指令后：恢复现场
  
    - %eip保存了返回地址
    - %esp将指向调用的返回值
    - %eax（如果64位，那么还得有个%edx）保存了返回值（如果void类型就丢弃）
    - 调用者：%edx，%ecx没什么变化，可能不用回被恢复
    - 调用函数中：%ebp, %ebx, %esi, %edi，会在调用期间被使用，因此需要恢复现场。当然你得知道，如果函数需要很少，那么寄存器满足，不会被开辟栈帧，而且ebx一般保存上一个ebp
  
 - 专业名词

    - 每个函数有个一个由ebp，esp来标记的栈帧

      ```
             +------------+   |
      		       | arg 2      |   \
      		       +------------+    >- previous function's stack frame
      		       | arg 1      |   /
      		       +------------+   |
      		       | ret %eip   |   /
      		       +============+   -----分界线，call后压eip，然后进入调用栈帧，保存ebp后才是新的局部变量保存
      		       | saved %ebp |   \
      		%ebp-> +------------+   |
      		       |            |   |
      		       |   local    |   \
      		       | variables, |    >- current function's stack frame
      		       |    etc.    |   /
      		       |            |   |
      		       |            |   |
      		%esp-> +------------+   /
      ```

    - %esp为栈顶，开辟前增长，移动后减小

    - %ebp指向前一个%ebp

    - 函数开场：ebp压栈

      ```
      pushl %ebp
      movl %esp, $ebp
      //or,一般不用
      enter $0, $0
      ```

    - 函数结束：ebp出栈（很容易找到压栈的eip）

      ```
      movl %ebp, %esp
      popl %ebp//这是得到了ebp，然后esp到了eip的位置
      //or
      leave
      ```

- 大例子

    ```
    //C代码
    	int main(void){return f(8)+1;}
    	int f(int x){return g(x);}
    	itn g(int x){return x+3;}
    //汇编
    _main:
    													prologue//函数头：保存ebp
    			pushl %ebp
    			movl %esp, %ebp
    													body
    			pushl $8
    			call _f
    			addl $1, %eax
    													epilogue
    			movl %ebp, %esp
    			popl %ebp
    			ret
    		_f:
    													prologue
    			pushl %ebp
    			movl %esp, %ebp
    													body
    			pushl 8(%esp)
    			call _g
    													epilogue
    			movl %ebp, %esp
    			popl %ebp
    			ret
    
    		_g:
    													prologue
    			pushl %ebp
    			movl %esp, %ebp
    													save %ebx，即传入的参数int x
    			pushl %ebx
    													body
    			movl 8(%ebp), %ebx
    			addl $3, %ebx
    			movl %ebx, %eax
    													restore %ebx
    			popl %ebx
    													epilogue
    			movl %ebp, %esp
    			popl %ebp
    			ret
    		
    ```

- 非常小的g函数汇编形式:在完全不需要栈的情况下，这种情况一般是汇编优化的选择

    ```
    _g:
    			movl 4(%esp), %eax
    			addl $3, %eax
    			ret
    ```

    

- 请你写出最小的f函数汇编形式

    ```
    
    ```

    

- 编译，链接，装载

    - 预处理：根据#inlude扩展头文件  --ASCII形式
    - 编译器进行编译得到汇编代码.asm --ASCII形式
    - 编译器汇编处理得到.o可重定位文件，此时机器已经可读 --二进制形式
    - 链接：链接器链接.o文件，产生一个可执行文件的映像 --二进制形式
    - 装载：把映像装载入内存，执行

![](https://tva1.sinaimg.cn/large/008i3skNly1gulrrz4qrmj60sg0dljse02.jpg)

## PC模拟

- Bochs模拟器的工作由

  - 软件模拟硬件实现（下图给出假设你的主机上安装了Linux虚拟机）![](https://tva1.sinaimg.cn/large/008i3skNly1gulrst7k45j60d00bi3yq02.jpg)

- 在已有系统上以一个进程的运行来运行

- 使用内存来存储

  - 使用全局变量来存储模拟的CPU寄存器

    ```
    int32_t regs[8];
    		#define REG_EAX 1;
    		#define REG_EBX 2;
    		#define REG_ECX 3;
    		...
    		int32_t eip;
    		int16_t segregs[4];
    		...
    ```

  - 使用Boch的内存来存储模拟的物理内存

    ```
    char mem[256*1024*1024]
    ```

- 通过for循环模拟CPU指令执行

  ```
  for (;;) {
  		read_instruction();
  		switch (decode_instruction_opcode()) {
  		case OPCODE_ADD:
  			int src = decode_src_reg();
  			int dst = decode_dst_reg();
  			regs[dst] = regs[dst] + regs[src];
  			break;
  		case OPCODE_SUB:
  			int src = decode_src_reg();
  			int dst = decode_dst_reg();
  			regs[dst] = regs[dst] - regs[src];
  			break;
  		...
  		}
  		eip += instruction_length;
  	}
  ```

- 通过页表来实现地址转换寻址物理地址

  ```
  #define KB		1024
  	#define MB		1024*1024
  
  	#define LOW_MEMORY	640*KB
  	#define EXT_MEMORY	10*MB
  
  	uint8_t low_mem[LOW_MEMORY];
  	uint8_t ext_mem[EXT_MEMORY];
  	uint8_t bios_rom[64*KB];
  
  	uint8_t read_byte(uint32_t phys_addr) {
  		if (phys_addr < LOW_MEMORY)
  			return low_mem[phys_addr];
  		else if (phys_addr >= 960*KB && phys_addr < 1*MB)
  			return rom_bios[phys_addr - 960*KB];
  		else if (phys_addr >= 1*MB && phys_addr < 1*MB+EXT_MEMORY) {
  			return ext_mem[phys_addr-1*MB];
  		else ...
  	}
  
  	void write_byte(uint32_t phys_addr, uint8_t val) {
  		if (phys_addr < LOW_MEMORY)
  			low_mem[phys_addr] = val;
  		else if (phys_addr >= 960*KB && phys_addr < 1*MB)
  			; /* ignore attempted write to ROM! */
  		else if (phys_addr >= 1*MB && phys_addr < 1*MB+EXT_MEMORY) {
  			ext_mem[phys_addr-1*MB] = val;
  		else ...
  	}
  ```

- 模拟IO设备：通过特定的内存访问IO空间来模拟

  - R/W的转换：从主机到模拟
  - 使用一个X窗口来模拟W到VGA（屏幕）
  - 同样从X窗口的输入消息队列来模拟R从键盘

​    

总结：

1.Boch模拟了以下硬件、

- 硬盘：主机的内存
- 虚拟屏幕VGA：绘制一个窗口
- 键盘:API调用
- 时钟：主机的时钟
- 等

实验的想法实现：

- 存储和进程
- 栈
- 内存和IO
- 硬件抽象为软件管理

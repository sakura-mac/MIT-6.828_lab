# xv6教程笔记 

[xv6教程原版](https://pdos.csail.mit.edu/6.828/2017/xv6/book-rev10.pdf)

[xv6源代码](https://pdos.csail.mit.edu/6.828/2017/xv6/xv6-rev10.pdf)

## 引论

我们将会通过学习xv6的内核，来完成对操作系统的通用学习。阅读中有疑问请配合[源代码](https://pdos.csail.mit.edu/6.828/2017/xv6/xv6-rev10.pdf)学习，所有的源代码全部都是由C构成。本个教程涵盖了所有lab的大部分知识，所以请结合实验需求，计划阅读相应章节。

## 0：操作系统接口

操作系统接口的设计目的就是通过对硬件的抽象，来获得更好的体验。抽象有以下需求：

1. 对逻辑抽象
2. 多路复用硬件
3. 程序互动，包括资源共享等

用户通过系统接口来使用操作系统，系统接口的设计有以下需求

1. KISS原则
2. 易实现
3. 功能可扩展

xv6是一个类UNIX系统，实际上UNIX更像一个标准而不是一个系统，因此了解xv6，对于Linux，MAC等类UNIX系统有帮助，比如内核中同样的系统调用命名或者实现等等

```
//给出一些系统调用，你应该知道这些调用的意义和使用场景
System call Description
fork() 											Create a process
exit() 											Terminate the current process
wait() 											Wait for a child process to exit
kill(pid) 									Terminate process pid
getpid() 										Return the current process’s pid
sleep(n) 										Sleep for n clock ticks
exec(filename, *argv) 			Load a file and execute it
sbrk(n) 										Grow process’s memory by n bytes
open(filename, flags) 			Open a file; the flags indicate read/write
read(fd, buf, n) 						Read n bytes from an open file into buf
write(fd, buf, n) 					Write n bytes to an open file
close(fd) 									Release open file fd
dup(fd) 										Duplicate fd
pipe(p) 										Create a pipe and return fd’s in p
chdir(dirname) 							Change the current directory
mkdir(dirname) 							Create a new directory
mknod(name, major, minor) 	Create a device file
fstat(fd) 									Return info about an open file
link(f1, f2) 								Create another name (f2) for the file f1
unlink(filename) 						Remove a file
```

进程的概念：进程即运行的程序，所以二者的关系为先有进程后有程序。一个进程包含了程序的指令（代码），数据，栈。而前者通常有相当一部分内容是使用栈来保存。因此一个进程的创建，或者相似概念的某些创建，必须要先开辟一个栈帧，借以组织各种调用和存储。

用户态和内核态：当一个进程使用内核服务的时候，他会调用系统接口，紧接着从用户态转入内核态，即必须进入”中断“，进而执行内核的指令，最后返回到用户态。因此一个进程的栈包含了用户空间和内核空间。

![](https://tva1.sinaimg.cn/large/008i3skNgy1gulvk2lo35j60dl0520sp02.jpg)

CPU的保护模式来确保每个进程可以访问修改自己的一片空间，而不是突然进入其他进程内。这实际上也是一种抽象：即用户态和内核态一层，内存一层。这样的抽象需要地址转换来实现。这涉及到了虚拟内存的知识。留个问题：进程真的完全不同相互访问吗？如果是真的那么不同的进程进入内核态是一致的吗？

Ps：如果现在不了解很正常，当你理解内存管理和进程后，这个问题就会变的非常简单。

接下来的章节讲述了xv6的服务：

1. 进程

2. 内存

3. 描述符（多种）

4. 管道

5. 文件系统

   并且教会你如何使用shell的代码来使用他们，同时也希望你结合源代码来理解实现逻辑。shell实际上就是你的cmd或者terminal。这是一个用户程序，你通过shell来直观地访问内核服务。

### 进程和内存

​	对于多进程来说，调度是一个很正常的事：我们采用的方法是进程之间时间同步，根据时间片来决定调度。在单核（单CPU）前提下，当一个进程切换为另一个进程时，当前进程会被挂起，这意味着CPU的寄存器和一些其他的会被保存，等待接下来的某个时刻，通过pid来辨识，重新切换回来。

​	进程的创建依赖于一个叫做fork的系统调用，即父进程通过调用fork来创建一个子进程，由于某些历史原因，子进程一开始并不是我们想要的那些程序代码或者数据，而是除了pid，内存部分引用父进程。因此还需要一些其他的系统调用来正确地回到我们要的“进程”中（exec等）。同时我可以保证，子进程只是暂时引用了父进程的内存，很快它便会使用“copy on wirte”技术，得到独立于父进程的内存空间。所以，子进程的活动不影响父进程的内存。

```
//给出创建子进程的一个实例：得到同样的进程代码执行，根据pid的不同来识别父，子进程
int pid = fork();
if(pid > 0){
printf("parent: child=%d\n", pid);
pid = wait();
printf("child %d is done\n", pid);
} else if(pid == 0){  //注意
printf("child: exiting\n");
exit();
} else {
printf("fork error\n");
}
```

我们除了fork还关注两个系统调用

- wait：父进程将等待，直到子进程调用exit结束，得到子pid
- exit：结束进程，释放内存资源
- exec：执行新的部分

注意：父进程和子进程的输出顺序并不是绝对的，我们可以很简单地认为随机。比如子进程执行了一行，父进程执行了一行，这样。输出往往看上去很混乱。这里也为后面的锁的实现提供了需求。

同时再强调一遍，虽然fork导致的父子进程几乎完全一样，代码执行位置也一样（所以才只通过pid判断），但是二者的活动互不相关，寄存器，用户态内存，等都可以认为相互独立。

接下来来讨论exec部分，将子进程执行我们想要的正确程序是一件很重要的事，因此exec就承担了这个责任：

- 使用ELF格式文件

- 告诉CPU新的代码，数据，执行起点等信息

- 一旦开始执行，就不再返回

  ```
  //给出exec实例，其中exec函数接收两个参数：1.文件位置 2.传入的字符串参数
  char *argv[3];
  argv[0] = "echo";//注意：第一个指针通常被忽略，默认认为是文件名，因此echo执行为“hello”
  argv[1] = "hello";
  argv[2] = 0;
  exec("/bin/echo", argv);
  printf("exec error\n");
  ```

最后给出一些小细节来结束本小节

- 一些系统调用比如echo，将会在执行后自动调用exit，同理wait也是被写入很多系统调用中，更何况还有调度程序使用这些结束的系统调用来管控进程。所以不要担心开始和结束的问题，就算上面的代码不写exit，他们也会自动结束的
- 进程中内存需要并不是一成不变的，fork得到了一些内存，exec也分配了一些内存，但是执行中仍然需要malloc（内调用sbrk（n），n代表字节）来分配新的内存，从而在返回的location中，继续使用内存
- xv6并没有管理员或者客人模式来设置安全防护，全都是root模式，因此你可以很放心地写关于用户态和内核态的代码交互

### I/O和文件描述符

文件描述符fd是一个int from 0，通常由一个进程独有，通常我们可以通过以下方式得到它

- 打开文件
- 打开目录
- 打开设备
- 创建管道
- 复制dup

那么文件描述符为什么要和文件名字区分开呢？有两个方面

- 抽象以上这些文件和非文件动作，最终大家都只通过字节流的方式读写
- 进程拥有一个描述符表，不同进程的fd不同，通过多级索引指向文件位置（多级索引是不同系统设计方式中非常常用的一个手段）

fd 中

- 0代表用来R
- 1代表用来W
- 2代表error

shell保证了这三个fd在shell进程中都能被使用，从而保证I/O和管道的正常使用

关于fd有以下系统调用

- int fd = open(filename, flags, mode)打开文件，返回fd
- read(fd, buf,n)，返回成功读取的字节数，多个read会维护一个offset来完成顺序读取
- write(fd, buf, n)，返回成功写入的字节数，同样维护一个offset

具体给出以下例子（实际上是cat的实现），你应该查看代码逻辑，并理解错误时发生的故事

```
char buf[512];
int n;

for(;;){
    n = read(0, buf, sizeof buf);//read
    if(n == 0)
        break;
    if(n < 0){
        fprintf(2, "read error\n");
        exit();
    }
    if(write(1, buf, n) != n){//error
        fprintf(2, "write error\n");
        exit();
    }
}
```



### 管道

​	管道是一个小型的内核buffer，进程传递一对fd：R/W来使用管道。我们可以认为对管道的一端写数据，被另一端读取，借此来完成进程间的通讯。

给出以下例子，来完成以上的功能。



```
int p[2];
char *argv[2];
argv[0] = "wc";
argv[1] = 0;
pipe(p);

if(fork() == 0) {
	close(0);
	dup(p[0]);
	close(p[0]);
	close(p[1]);
	exec("/bin/wc", argv);
} else {
	close(p[0]);
	write(p[1], "hello world\n", 12);
	close(p[1]);
}
```

程序调用pipe创建一个管道，使用p数组来记录fd的读写。fork之后，父子进程都有对pipe的引用。

子进程：复制读端fd为0，关闭p所在的管道，执行wc，当wc读取标准输入，是从p来读取的。

父进程：关闭管道的读，并写入管道，然后关闭管道写端

### 文件系统

​	xv6提供数据文件，就只是未解释类型的字节数组，并且提供目录，目录包含了指向其他文件和目录的引用。对于目录构造一个树，从一个称为root的特殊目录开始。一个形如“/a/b/c”指向/目录下的a目录下的b目录下的文件c。这个例子中，从不是“/”目录下开始的目录能够称为进程的当前目录，可以通过chdir系统调用来改变当前目录。给出以下例子，他们打开了相同的文件（假设文件，目录存在）

```
chdir("/a");
chdir("b");//可以看到目录改变有绝对和相对两种
open("c", O_RDONLY);

open("/a/b/c", O_RDONLY);
```

第一部分改变当前目录为“/a/b”，第二部分直接给出绝对路

​	我们有多种系统调用来产生文件和目录（你会很快体会一切皆文件这个理念）：1.mkdir来创建一个新的目录，以一个“O_CREATE”flag来创建一个文件2.mknod来创建一个新的设备文件，给定以下例子来说明设备文件的细微不同

```
mkdir("/dir");
fd = open("/dir/file",O_CREATE|O_WRONLY);
close(fd);
mknod("/console",1,1);
```

mknode通过创建一个文件系统里的文件，但是没有内容。并且文件metadata标记为一个设备文件，并且记录major和minor设备号（你可以认为是mknod的两个必备参数），这两个参数唯一确定一个内核设别。当一个进程打开文件后，R/W系统调用并不操作文件系统（那些字节数组），而是操作设备文件里的内容。

​	fstat从fd指向的对象中检索信息，信息填充在struct stat中。stat定义在stat.h

```
#define T_DIR 1 // 目录
#define T_FILE 2 // 文件
#define T_DEV 3 // 设备
struct stat {
  short type; // 文件类型
	int dev; // 容纳文件系统的磁盘设备号，考虑mknod是否创建一个新的文件系统？
	uint ino; // inode编号
	short nlink; // 连接的数量
	uint size; // 以字节为单位的文件大小
};
```

​	目录下的文件名和文件本身不相同：考虑一个抽象概念：name-inode(可以和这个抽象比较：fd-opened_filename-inode)，name对inode是多对一关系，称为links。link系统调用能够创建另一个名字，但是能够指向同样的文件inode，以下例子创建一个文件，他拥有两个名字.

```
open("a", O_CREATE|O_WRONLY);
link("a","b");
```

至此，R/W能够通过a/b来对同一个文件操作。每个inode有唯一的inode编号。上面的代码完成后，fstat返回相同的inode编号（ino），以此能够决定a和b能够指向同的文件，然后nlink增加为2。（问题：文件名不在stat里，怎么通过filename定向stat？）

​	unlink从文件系统中删除这个filename，文件inode和磁盘空间里，有关的部分将会在nlink为0，并且没有fd指向的时候

```
unlink("a");
```

​	这句代码将会使b唯一指向inode指向文件。玩一个例子：

```
fd = open("/tmp/xyz", O_CREATE|O_RDWR);
unlink("/tmp/xyz");
```

在这种情况会创建一个临时的inode，然后在进程结束后关闭fd，或者退出

​	对于shell里操作文件系统，能够被我们使用并且接手修改的的包括mkdir, ln, rm等。这种设计允许任何人修改命令，而只需要添加用户级别的程序代码。但是后来我们发现，这个想法被其他unix时代设计操作系统摒弃：必须在内核里。

​	幸运的是，除了cd，cd能够改变shell当前查看的目录，如果cd执行，那么首先fork一个子进程，子进程执行cd，而对于父进程的当前目录未变。

### 真实世界

​	

## 1：操作系统组织



## 2：页表

## 3：中断：traps，interrupts，drivers

## 4：锁

## 5：调度

## 6：文件系统 

## 7：总结

## 附录A：PC硬件

## 附录B：引导


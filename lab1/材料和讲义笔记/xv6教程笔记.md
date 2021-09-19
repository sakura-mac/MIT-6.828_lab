# xv6教程笔记 

## 引论

我们将会通过学习xv6的内核，来完成对操作系统的通用学习。请配合[源代码](https://pdos.csail.mit.edu/6.828)学习，所有的源代码全部都是由C构成。本个教程涵盖了所有lab的大部分知识，所以请结合实验需求，计划阅读相应章节。

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

对于多进程来说，调度是一个很正常的事：我们采用的方法是进程之间时间同步，根据时间片来决定调度。在单核（单CPU）前提下，当一个进程切换为另一个进程时，当前进程会被挂起，这意味着CPU的寄存器和一些其他的会被保存，等待接下来的某个时刻，通过pid来辨识，重新切换回来。

进程的创建依赖于一个叫做fork的系统调用，即父进程通过调用fork来创建一个子进程，由于某些历史原因，子进程一开始并不是我们想要的那些程序代码或者数据，而是除了pid，内存部分引用父进程。因此还需要一些其他的系统调用来正确地回到我们要的“进程”中（exec等）。同时我可以保证，子进程只是暂时引用了父进程的内存，很快它便会使用“copy on wirte”技术，得到独立于父进程的内存空间。所以，子进程的活动不影响父进程的内存。

```
//给出创建子进程的一个实例：得到同样的进程代码执行，根据pid的不同来识别父，子进程
int pid = fork();
if(pid > 0){
printf("parent: child=%d\n", pid);
pid = wait();
printf("child %d is done\n", pid);
} else if(pid == 0){
printf("child: exiting\n");
exit();
} else {
printf("fork error\n");
}
```

最终调度程序会使用exit系统调用终止进程，并释放内存

### I/O和文件描述符



### 管道



### 文件系统



### 真实世界

## 1：操作系统组织



## 2：页表

## 3：中断：traps，interrupts，drivers

## 4：锁

## 5：调度

## 6：文件系统 

## 7：总结

## 附录A：PC硬件

## 附录B：引导


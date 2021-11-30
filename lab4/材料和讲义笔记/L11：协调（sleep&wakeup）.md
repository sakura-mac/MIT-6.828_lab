# 笔记计划

  完成调度
  协调
    	xv6：睡眠和唤醒
    	丢失唤醒问题
    	终止睡眠线程

  大图： 
  	  进程、每个进程的内核堆栈、内核、每个内核的调度堆栈
  	  yield/swtch/scheduler/swtch/yield示意图

# 调度

xv6 从内核线程到调度器使用 ptable.lock 和 swtch 是不寻常的

  大多数情况下，xv6 是一个普通的**并行共享内存程序**
  但是这种线程切换和锁的使用是非常特定于操作系统的

关于 xv6 上下文切换与并发的问题

 1.为什么 yield() 跨 sched/swtch 持有 ptable.lock？因为sched/swtch必须是原子操作，否则可能出现切换一个线程堆栈，但是sched选择了另一个线程。
    如果另一个内核的调度程序立即看到 RUNNABLE 进程怎么办？进入switch
    如果在 swtch() 期间发生定时器中断怎么办？关中断
  2.我们怎么知道 scheduler() 线程已经准备好让我们 swtch() 进入？锁
  3.两个scheduler（）可以选择同一个 RUNNABLE 进程吗？ptable.lock

# 为什要协调（coordination）：
  线程需要等待特定的事件或条件：
    等待磁盘读取完成
    等待管道读取器在管道中腾出空间
    等待任何子进程退出

为什么不只是有一个循环直到事件发生？想象死锁：循环等待，所以如果构建一个优先队列，就能破坏这个循环等待

更好的解决方案：产生 CPU 的协调原语

  睡眠和唤醒 (xv6)
  条件变量（作业）
  障碍（家庭作业）
  等等。

# 协调的实现：睡眠和唤醒
  sleep（old，lock）
    睡在一个“channel”上，一个用来命名我们正在睡觉的条件的地址
  wakeup（old）
    唤醒所有沉睡在 chan 上的线程
    可能会唤醒多个线程：加入磁盘读取完毕，那么我们就会唤醒所有等待磁盘写的线程

# 协调的例子

## sleep/wakeup 的使用示例 -- iderw() / ideintr()

  iderw() 排队阻塞读请求，然后 sleep()s
    b 是一个缓冲区，将填充块内容
    iderw() 休眠等待 B_VALID——磁盘读取完成
    chan 是 b -- 这个缓冲区（在 iderw() 中可能还有其他进程）
  当前磁盘读取完成后，通过中断调用 ideintr()
    标记 b B_VALID
    唤醒（b）-与睡眠相同的chan

图像
  iderw() 在调用 sleep 时持有 **idelock**
  但是 ideintr() 需要获取 **idelock**！
  为什么 iderw() 在调用 sleep() 之前不释放 idelock？：ideintr中断需要切换，那么假如未睡眠就先切换，就会可能导致下一个该睡眠的先睡眠，破坏队列的优先级。所以为了保证优先级，必须要睡眠判断（可能是另一个锁）和睡眠操作都在临界区内。

# 协调的问题

  - 丢失唤醒
  - 终止休眠线程

# 协调的第一个问题--丢失唤醒

## 丢失唤醒演示
  修改 iderw() 调用 release ：broken_sleep；acquire
  看看broken_sleep() 
  看看wakeup（）
  发生什么了？

  ideintr() 在 iderw() 没有看到 B_VALID 之后运行
    但在 Broken_sleep() 设置 state = SLEEPING 之前
    wakeup（）扫描可处理但没有线程处于休眠状态
  然后 sleep() 将当前线程设置为 SLEEPING 并产生睡眠错过唤醒 -> 死锁

  这是一个“丢失的唤醒”，简单来说就是wakeup（）执行之后才将state设置为SLEEPING

## xv6 解决方案：
  目标： 
    1）在条件之间的整个时间内锁定wakeup()
       检查并state = 睡眠
    2）在休眠时释放条件锁
  xv6 策略：
    需要wakeup（）在条件和 ptable.lock 上保持锁定
    睡眠者始终持有一个或另一个锁
      持有 ptable.lock 后可以释放条件锁
  看 ideintr() 对wakeup() 的调用
    并唤醒自己
    在寻找chan时，两把锁都被持有
  看看 iderw() 对 sleep() 的调用
    条件锁在调用 sleep() 时被持有
    查看 sleep() -- 获取 ptable.lock，然后在条件下释放锁
  图表：
    |----idelock----|
                  |---ptable.lock---|
                                     |----idelock----|
                                      |-ptable.lock-|
  因此：
    要么运行完整的 sleep() 序列，然后运行wakeup()，
      睡觉的人会被吵醒
    或唤醒（）首先运行，在潜在的睡眠者检查条件之前，
      但唤醒将设置条件为真
  要求 sleep() 接受一个锁定参数

人们已经开发了许多序列协调原语，

  所有这些都必须解决丢失唤醒问题。
  例如**条件变量**（类似于睡眠/唤醒）
  例如计数**信号量**

## 另一个例子：piperead()
  while 循环正在等待缓冲区中多于零字节的数据
    唤醒在 pipewrite() 结束时
    old线程是&p->nread
  如果 piperead() 使用了broken_sleep()，那么竞争是什么？write和read，read和read
  为什么在 sleep() 周围有一个循环？每个字节读取之后睡眠等待
  为什么在 piperead() 的末尾有 wakeup()？因为唤醒之后读取，

# 协调第二个问题--如何终止一个休眠线程？

首先，kill(target_pid) 是如何工作的？

  问题：强行终止进程可能不安全
    它可能在内核中执行
      使用其内核堆栈、页表、proc[] 条目
    可能处于临界区，需要完成以恢复不变量
    所以我们不能立即终止它
  **解决方案**：目标在下一个方便的点退出
    kill() 设置 p->killed 标志 
    目标线程在 trap() 和 exit()s 中检查 p->killed
      所以 kill() 不会干扰例如临界区
    **exit() 关闭 FD，设置 state=ZOMBIE，让出 CPU**
      为什么它不能释放内核堆栈和页表？
    父级 wait() 释放内核堆栈、页表和 proc[] 槽

## 如果kill() 目标是sleep() 呢？
  例如等待控制台输入，或在 wait() 或 iderw()
  我们希望目标停止sleep并exit（）：想想终端命令的ctrl-C
  但这并不总是合理的
    也许目标是在复杂操作的中途睡眠（）
      （为了一致性）必须完成
    例如创建一个文件

## xv6 解决方案
  参见 proc.c 中的 kill()
    将 SLEEPING 更改为 RUNNABLE -- 就像 wakeup()
  **一些睡眠循环检查 p->killed**
    例如piperead()
    **sleep() 将由于 kill 的 state=RUNNABLE 而返回**
    在一个循环中，所以重新检查
      条件为真 -> 读取一些字节，然后陷阱 ret 调用 exit()
      条件 false -> 看到 p->killed、return、trap ret 调用 exit()
    无论哪种方式，对 piperead() 中线程的 kill() 的近乎即时响应
  一些睡眠循环不检查 p->killed
    问：为什么不修改 iderw() 来检查睡眠循环中的 p->killed 并返回？
    A：如果读取，调用sFS代码期望看到磁盘缓冲区中的数据！
       如果写（或读），可能是 create() 的一半
       现在exit会使磁盘 FS 不一致。
  因此 iderw() 中的线程可能会在内核中继续执行一段时间
    trap() 将在系统调用完成后退出()

简单来说，对于kill会对state检查，将sleeping改为runnable，然后像正常退出子线程一样操作

# xv6 kill规范
  如果目标在用户空间：立即放弃使用CPU，但是放弃内存在
    将在下次进行系统调用或接受定时器中断时死亡
  如果目标在内核中：并不直接放弃使用CPU
    目标永远不会执行另一个用户指令
    但可能会在内核中花费相当长的时间

# JOS 是如何应对这些问题的？
  失去了唤醒？
    JOS 是单处理器，内核中禁止中断
    所以唤醒不能在条件检查和睡眠之间潜行
  阻塞时终止？
    JOS 只有几个系统调用，而且它们相当简单
      没有像 create() 这样的阻塞多步操作
      因为内核中没有 FS 和磁盘驱动程序
    实际上只有一个阻塞调用——IPC recv()
      如果 env_destroy() 正在运行，则目标线程未运行
      recv() 使 env 处于可以安全销毁的状态

# 概括

  睡眠/唤醒让线程等待特定事件
  并发和中断意味着我们必须担心丢失的唤醒
  终止是线程系统中的痛苦
    上下文切换 vs 进程退出
    sleep vs kill


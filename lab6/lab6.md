既然你有一个文件系统，任何自尊的操作系统都不应该没有网络堆栈。在本实验中，您将编写网络接口卡的驱动程序。该卡将基于英特尔 82540EM 芯片，也称为 E1000。

### 入门

使用 Git 提交您的 Lab 5 源代码（如果您还没有），获取课程存储库的最新版本，然后基于我们的 lab6 分支`origin/lab6`创建一个名为`lab6`的本地分支：``

```
雅典娜%cd ~/6.828/lab
雅典娜%add git
雅典娜%git commit -am 'my solution to lab5'
无需提交（工作目录干净）
雅典娜% git pull
已经是最新的。
雅典娜% git checkout -b lab6 origin/lab6
分支 lab6 设置为跟踪远程分支 refs/remotes/origin/lab6。
切换到新分支“lab6”
雅典娜% git merge lab5
通过递归进行合并。
 fs/fs.c | 42 +++++++++++++++++++++
 1 个文件更改，42 个插入（+），0 个删除（-）
雅典娜% 
```

但是，网卡驱动程序不足以让您的操作系统连接到 Internet。在新的 lab6 代码中，我们为您提供了网络堆栈和网络服务器。与之前的实验一样，使用 git 获取本实验的代码，合并到您自己的代码中，并探索新`net/`目录的内容 以及`kern/ 中`的新文件。

除了编写驱动程序之外，您还需要创建一个系统调用接口来访问您的驱动程序。您将实现缺少的网络服务器代码以在网络堆栈和驱动程序之间传输数据包。您还将通过完成 Web 服务器将所有内容联系在一起。使用新的 Web 服务器，您将能够从您的文件系统提供文件。

大部分内核设备驱动程序代码都需要您自己从头开始编写。本实验提供的指导比以前的实验少得多：没有骨架文件，没有一成不变的系统调用接口，许多设计决策由您决定。出于这个原因，我们建议您在开始任何单独的练习之前阅读整个作业。许多学生发现这个实验比以前的实验更难，所以请相应地计划你的时间。

### 实验室要求

和以前一样，您需要完成实验室中描述的所有常规练习和*至少一个*挑战题。写下对实验室中提出的问题的简要回答，并在`answers-lab6.txt 中`描述你的挑战练习。

## QEMU 的虚拟网络

我们将使用 QEMU 的用户模式网络堆栈，因为它不需要管理权限即可运行。QEMU 的文档[在这里](http://wiki.qemu.org/download/qemu-doc.html#Using-the-user-mode-network-stack)有更多关于 user-net 的 [信息](http://wiki.qemu.org/download/qemu-doc.html#Using-the-user-mode-network-stack)。我们更新了 makefile 以启用 QEMU 的用户模式网络堆栈和虚拟 E1000 网卡。

默认情况下，QEMU 提供了一个运行在 IP 10.0.2.2 上的虚拟路由器，并将为 JOS 分配 IP 地址 10.0.2.15。为了简单`起见`，我们将这些默认值硬编码到网络服务器的`net/ns.h 中`。

虽然 QEMU 的虚拟网络允许 JOS 与 Internet 进行任意连接，但 JOS 的 10.0.2.15 地址在 QEMU 内部运行的虚拟网络之外没有任何意义（即 QEMU 充当 NAT），因此我们无法直接连接到服务器在 JOS 内部运行，甚至从运行 QEMU 的主机运行。为了解决这个问题，我们将 QEMU 配置为在*主机*上的某个端口上运行一个服务器，该服务器只需连接到 JOS 中的某个端口，并在您的真实主机和虚拟网络之间来回传输数据。

您将在端口 7 (echo) 和 80 (http) 上运行 JOS 服务器。为了避免共享 Athena 机器上的冲突，makefile 会根据您的用户 ID 为这些机器生成转发端口。要找出 QEMU 转发到您的开发主机上的哪些端口，请运行make which-ports. 为方便起见，makefile 还提供了 make nc-7和make nc-80，它允许您直接与在终端中的这些端口上运行的服务器进行交互。（这些目标仅连接到正在运行的 QEMU 实例；您必须单独启动 QEMU 本身。）

### 数据包检查

makefile 还配置 QEMU 的网络堆栈以将所有传入和传出数据包记录到实验室目录中的`qemu.pcap`。

要获取捕获数据包的十六进制/ASCII 转储，请使用`tcpdump，`如下所示：

```
tcpdump -XXnr qemu.pcap
```

或者，您可以使用[Wireshark](http://www.wireshark.org/)以图形方式检查 pcap 文件。Wireshark 还知道如何解码和检查数百种网络协议。如果您在 Athena 上，则必须使用 Wireshark 的前身 ethereal，它位于 sipbnet 储物柜中。

### 调试E1000

我们很幸运能够使用仿真硬件。由于 E1000 在软件中运行，仿真的 E1000 可以以用户可读的格式向我们报告其内部状态和遇到的任何问题。通常，对于使用裸机编写的驱动程序开发人员来说，这种奢侈是不可能的。

E1000 可以产生大量调试输出，因此您必须启用特定的日志记录通道。您可能会觉得有用的一些频道是：

| 旗帜     | 意义                           |
| -------- | ------------------------------ |
| 发送     | 记录数据包传输操作             |
| 发送错误 | 记录传输环错误                 |
| 接收     | 记录对 RCTL 的更改             |
| 过滤器   | 传入数据包的日志过滤           |
| 输入错误 | 记录接收环错误                 |
| 未知     | 记录未知寄存器的读写           |
| 内存     | 从 EEPROM 读取日志             |
| 打断     | 记录中断和对中断寄存器的更改。 |

例如，要启用“tx”和“txerr”日志记录，请使用 make E1000_DEBUG=tx,txerr ....

*注意：* `E1000_DEBUG`标志仅适用于 6.828 版本的 QEMU。

您可以更进一步地使用软件模拟硬件进行调试。如果您曾经被卡住并且不明白为什么 E1000 没有按照您预期的方式响应，您可以在`hw/e1000.c 中`查看 QEMU 的 E1000 实现。

## 网络服务器

从头开始编写网络堆栈是一项艰巨的工作。相反，我们将使用 lwIP，这是一个开源的轻量级 TCP/IP 协议套件，其中包括一个网络堆栈。您可以在[此处](http://www.sics.se/~adam/lwip/)找到有关 lwIP 的更多信息 。在本次作业中，就我们而言，lwIP 是一个实现了 BSD 套接字接口的黑盒，并具有数据包输入端口和数据包输出端口。

网络服务器实际上是四种环境的组合：

- 核心网络服务器环境（包括socket调用调度器和lwIP）
- 输入环境
- 输出环境
- 定时器环境

下图显示了不同的环境及其关系。该图显示了包括设备驱动程序在内的整个系统，稍后将对其进行介绍。在本实验中，您将实现以绿色突出显示的部分。

![网络服务器架构](https://pdos.csail.mit.edu/6.828/2017/labs/lab6/ns.png)

### 核心网服务器环境

核心网服务器环境由socket调用调度器和lwIP本身组成。套接字调用调度程序的工作方式与文件服务器完全一样。用户环境使用存根（在`lib/nsipc.c 中`找到）将 IPC 消息发送到核心网络环境。如果查看 `lib/nsipc.c，`您会发现我们找到核心网络服务器的方式与找到文件服务器的方式相同：`i386_init`使用 NS_TYPE_NS 创建 NS 环境，因此我们进行扫描`envs`，寻找这种特殊的环境类型。对于每个用户环境IPC，网络服务器中的调度器代表用户调用lwIP提供的相应BSD套接字接口函数。

常规用户环境不`nsipc_*`直接使用调用。相反，他们使用`lib/sockets.c 中`的函数，它提供了一个基于文件描述符的套接字 API。因此，用户环境通过文件描述符引用套接字，就像它们引用磁盘文件一样。多个操作（`connect`，`accept`等）特定于插座，但是`read`，`write`和 `close`经过在正常文件描述符设备分派代码`LIB / fd.c`。就像文件服务器如何为所有打开的文件维护内部唯一 ID 一样，lwIP 也为所有打开的套接字生成唯一 ID。在文件服务器和网络服务器中，我们使用存储在其中的信息将`struct Fd`每个环境的文件描述符映射到这些唯一的 ID 空间。

尽管看起来文件服务器和网络服务器的 IPC 调度程序的行为相同，但还是有一个关键的区别。BSD 套接字调用像 `accept`并且`recv`可以无限期地阻塞。如果调度程序让 lwIP 执行这些阻塞调用之一，调度程序也会阻塞，并且整个系统一次只能有一个未完成的网络调用。由于这是不可接受的，网络服务器使用用户级线程来避免阻塞整个服务器环境。对于每个传入的 IPC 消息，调度程序创建一个线程并在新创建的线程中处理请求。如果线程阻塞，则只有该线程进入睡眠状态，而其他线程继续运行。

除了核心网络环境，还有三个辅助环境。除了接受来自用户应用程序的消息，核心网络环境的调度器还接受来自输入和定时器环境的消息。

### 输出环境

在服务用户环境套接字调用时，lwIP 将生成数据包供网卡传输。LwIP 将使用`NSREQ_OUTPUT`IPC 消息将要传输的每个数据包发送到输出帮助程序环境，数据包附加在 IPC 消息的页面参数中。输出环境负责接受这些消息，并通过您即将创建的系统调用接口将数据包转发到设备驱动程序。

### 输入环境

网卡收到的数据包需要注入lwIP。对于设备驱动程序接收到的每个数据包，输入环境将数据包拉出内核空间（使用您将实现的内核系统调用）并使用`NSREQ_INPUT`IPC 消息将数据包发送到核心服务器环境。

数据包输入功能与核心网络环境分离，因为 JOS 很难同时接受 IPC 消息和轮询或等待来自设备驱动程序的数据包。我们`select` 在 JOS 中没有允许环境监视多个输入源以识别哪些输入已准备好进行处理的系统调用。

如果你看看`网/ Input.c中`和`网/ output.c`你会看到，都需要执行。这主要是因为实现取决于您的系统调用接口。在实现驱动程序和系统调用接口后，您将为两个帮助程序环境编写代码。

### 定时器环境

定时器环境定期向`NSREQ_TIMER`核心网络服务器发送类型消息，通知它定时器已到期。lwIP 使用来自该线程的计时器消息来实现各种网络超时。

# A部分：初始化和传输数据包

您的内核没有时间概念，因此我们需要添加它。目前硬件每 10ms 产生一个时钟中断。在每个时钟中断上，我们可以增加一个变量来表示时间已经提前了 10 毫秒。这是在`kern/time.c 中实现的`，但尚未完全集成到您的内核中。

## **练习 1.**

`time_tick`在`kern/trap.c 中`为每个时钟中断 添加一个调用。落实`sys_time_msec`并把它添加到`syscall`在`kern/ syscall.c`使用户空间的访问时间。

使用make INIT_CFLAGS=-DTEST_NO_NS run-testtime来测试你的时间码。您应该看到环境以 1 秒为间隔从 5 倒计时。“-DTEST_NO_NS”禁止启动网络服务器环境，因为此时它会在实验室中出现混乱。

## 网络接口卡

编写驱动程序需要深入了解硬件和呈现给软件的接口。实验课本将提供有关如何与 E1000 交互的高级概述，但您在编写驱动程序时需要大量使用英特尔的手册。

## **练习 2.**

浏览英特尔的E1000[软件开发人员手册](https://pdos.csail.mit.edu/6.828/2017/readings/hardware/8254x_GBe_SDM.pdf)。本手册涵盖了几个密切相关的以太网控制器。QEMU 模拟 82540EM。

您现在应该浏览第 2 章以了解该设备。要编写驱动程序，您需要熟悉第 3 章和第 14 章以及 4.1（尽管不是 4.1 的小节）。您还需要使用第 13 章作为参考。其他章节主要介绍您的驱动程序不必与之交互的 E1000 组件。现在不要担心细节；只需感受一下文档的结构，您就可以稍后查找内容。

在阅读手册时，请记住 E1000 是一款具有许多高级功能的精密设备。正常工作的 E1000 驱动程序只需要 NIC 提供的一小部分功能和接口。仔细考虑与卡交互的最简单方法。我们强烈建议您在使用高级功能之前先让基本驱动程序工作。

### PCI接口

E1000 是 PCI 设备，这意味着它插入主板上的 PCI 总线。PCI总线有地址线、数据线和中断线，允许CPU与PCI设备通信，PCI设备读写内存。PCI 设备需要先被发现并初始化，然后才能使用。发现是在 PCI 总线上寻找连接设备的过程。初始化是分配 I/O 和内存空间以及协商 IRQ 线以供设备使用的过程。

我们在`kern/pci.c 中`为您提供了 PCI 代码。为了在引导期间执行 PCI 初始化，PCI 代码会在 PCI 总线上寻找设备。当它找到一个设备时，它会读取它的供应商 ID 和设备 ID，并使用这两个值作为关键字来搜索`pci_attach_vendor`数组。该数组由如下`struct pci_driver`条目组成 ：

```
结构 pci_driver {
    uint32_t key1, key2;
    int (*attachfn) (struct pci_func *pcif);
};
```

如果发现的设备的供应商 ID 和设备 ID 与阵列中的条目匹配，则 PCI 代码调用该条目的`attachfn`来执行设备初始化。（设备也可以通过类来识别，这是`kern/pci.c 中`另一个驱动程序表的用途。）

attach 函数通过一个*PCI 函数*来初始化。PCI 卡可以提供多种功能，而 E1000 只提供一种功能。以下是我们在 JOS 中表示 PCI 功能的方式：

```
结构 pci_func {
    结构 pci_bus *总线；

    uint32_t 开发；
    uint32_t 函数；

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};
```

上述结构反映了开发人员手册第 4.1 节表 4-1 中的一些条目。`struct pci_func`我们特别感兴趣的是最后三个条目 ，因为它们记录了设备的协商内存、I/O 和中断资源。在`reg_base`与`reg_size`阵列包含多达六个基地址寄存器或条信息。 `reg_base`存储内存映射 I/O 区域（或 I/O 端口资源的基本 I/O 端口）的基本内存地址， `reg_size`包含来自 的相应基本值的大小（以字节为单位）或 I/O 端口数`reg_base`，并`irq_line`包含IRQ 线分配给设备用于中断。E1000 BAR 的具体含义如表4-2 后半部分所示。

当设备的 attach 函数被调用时，该设备已被找到但尚未*启用*。这意味着 PCI 代码尚未确定分配给设备的资源，例如地址空间和 IRQ 线，因此，`struct pci_func`尚未填充结构的最后三个元素。附加函数应调用 `pci_func_enable`，即将启用设备，协商这些资源，并填写`struct pci_func`.

## **练习 3.** 

实现一个附加函数来初始化 E1000。如果找到匹配的 PCI 设备，则向`kern/pci.c 中`的`pci_attach_vendor`数组添加一个条目以 触发您的函数（确保将其放在标记表末尾的条目之前）。您可以在第 5.2 节中找到 QEMU 模拟的 82540EM 的供应商 ID 和设备 ID。当 JOS 在引导时扫描 PCI 总线时，您还应该看到这些列表。 ```{0, 0, 0}`

现在，只需通过 启用 E1000 设备 `pci_func_enable`。我们将在整个实验中添加更多初始化。

我们已经为您提供了`kern/e1000.c`和 `kern/e1000.h`文件，这样您就不需要弄乱构建系统。它们目前是空白的；你需要在这个练习中填写它们。您可能还需要在内核的其他位置包含`e1000.h`文件。

当您启动内核时，您应该看到它打印出 E1000 卡的 PCI 功能已启用。现在您的代码应通过 `PCI附加`的测试make grade。

### 内存映射 I/O

软件通过*内存映射 I/O* (MMIO)与 E1000 通信。您之前在 JOS 中已经见过两次：CGA 控制台和 LAPIC 都是您通过写入和读取“内存”来控制和查询的设备。但是这些读取和写入不会进入 DRAM；他们直接进入这些设备。

`pci_func_enable`与 E1000 协商一个 MMIO 区域，并将其基数和大小存储在 BAR 0（即 `reg_base[0]`和`reg_size[0]`）中。这是分配给设备的*物理内存地址*范围，这意味着您必须做一些事情才能通过虚拟地址访问它。由于 MMIO 区域被分配了非常高的物理地址（通常超过 3GB），`KADDR`由于 JOS 的 256MB 限制，您不能使用它来访问它。因此，您必须创建一个新的内存映射。我们将使用 MMIOBASE 上方的区域（您` mmio_map_region`来自实验 4 的区域将确保我们不会覆盖 LAPIC 使用的映射）。由于 PCI 设备初始化发生在 JOS 创建用户环境之前，因此您可以在其中创建映射，`kern_pgdir`并且它始终可用。

## **练习 4.**

在您的附加函数中，通过调用`mmio_map_region`（您在实验 4 中编写以支持内存映射 LAPIC）为 E1000 的 BAR 0 创建虚拟内存映射 。

您需要将这个映射的位置记录在一个变量中，以便您以后可以访问刚刚映射的寄存器。查看`kern/lapic.c`中的`lapic`变量，以获取执行此操作的一种方法的示例。如果您确实使用指向设备寄存器映射的指针，请务必声明它 ；否则，允许编译器缓存值并重新排序对该内存的访问。```volatile`

要测试您的映射，请尝试打印设备状态寄存器（第 13.4.2 节）。这是一个 4 字节的寄存器，从寄存器空间的第 8 字节开始。您应该得到`0x80080783`，这表明全双工链路的速度为 1000 MB/s，等等。

提示：您将需要很多常量，例如寄存器的位置和位掩码的值。试图从开发人员手册中复制这些内容很容易出错，并且错误会导致痛苦的调试会话。我们建议改为使用 QEMU 的[`e1000_hw.h`](https://pdos.csail.mit.edu/6.828/2017/labs/lab6/e1000_hw.h)头文件作为指导。我们不建议逐字复制它，因为它定义的内容远比您实际需要的多，并且可能无法按照您需要的方式定义事物，但这是一个很好的起点。

### DMA

您可以想象通过从 E1000 的寄存器写入和读取来发送和接收数据包，但这会很慢并且需要 E1000 在内部缓冲数据包数据。相反，E1000 使用*直接内存访问*或者DMA直接从内存中读写包数据，不涉及CPU。驱动程序负责为发送和接收队列分配内存，设置 DMA 描述符，并使用这些队列的位置配置 E1000，但之后的一切都是异步的。为了传输一个数据包，驱动程序将它复制到传输队列中的下一个 DMA 描述符中，并通知 E1000 另一个数据包可用；当有时间发送数据包时，E1000 会将数据从描述符中复制出来。同样，当 E1000 接收到一个数据包时，它会将它复制到接收队列中的下一个 DMA 描述符中，驱动程序可以在下一次机会读取它。

接收和发送队列在高层次上非常相似。两者都由一系列*描述符组成*。虽然这些描述符的确切结构各不相同，但每个描述符都包含一些标志和包含数据包数据的缓冲区的物理地址（卡要发送的数据包数据，或操作系统分配的缓冲区，用于卡将接收到的数据包写入到）。

队列被实现为循环数组，这意味着当卡或驱动程序到达数组的末尾时，它会返回到开头。两者都有一个*头指针*和一个*尾指针*而队列的内容就是这两个指针之间的描述符。硬件总是从头部消耗描述符并移动头指针，而驱动程序总是在尾部添加描述符并移动尾指针。传输队列中的描述符代表等待发送的数据包（因此，在稳定状态下，传输队列为空）。对于接收队列，队列中的描述符是卡可以接收数据包的空闲描述符（因此，在稳定状态下，接收队列由所有可用的接收描述符组成）。在不混淆 E1000 的情况下正确更新尾寄存器是很棘手的；当心！

指向这些数组的指针以及描述符中数据包缓冲区的地址都必须是*物理地址，* 因为硬件直接在物理 RAM 之间执行 DMA，而无需通过 MMU。

## 传输数据包

E1000的发射和接收功能基本上是相互独立的，所以我们可以一次处理一个。我们将首先攻击传输数据包，因为我们无法在不传输“我在这里！”的情况下测试接收。先打包。

首先，您必须按照 14.5 节中描述的步骤初始化要传输的卡（您不必担心小节）。传输初始化的第一步是设置传输队列。队列的精确结构在 3.4 节中描述，描述符的结构在 3.3.3 节中描述。我们不会使用 E1000 的 TCP 卸载功能，因此您可以专注于“传统传输描述符格式”。您现在应该阅读这些部分并熟悉这些结构。

### C 结构

您会发现使用 C`struct`来描述 E1000 的结构很方便。正如您所看到的结构，如 `struct Trapframe`, C`struct`允许您在内存中精确地布局数据。C 可以在字段之间插入填充，但 E1000 的结构布局使得这应该不是问题。如果确实遇到字段对齐问题，请查看 GCC 的“packed”属性。

例如，考虑手册表 3-8 中给出并在此处复制的传统传输描述符：

```
  63 48 47 40 39 32 31 24 23 16 15 0
  +------------------------------------------------- --------------+
  | 缓冲区地址 |
  +--------------+-------+-------+-------+-------+- --------------+
  | 特价 | CSS | 状态| 命令 | 公民社会组织 | 长度 |
  +--------------+-------+-------+-------+-------+- --------------+
```

该结构的第一个字节从右上角开始，因此要将其转换为 C 结构，请从右到左、从上到下读取。如果你眯着眼睛看它，你会看到所有的字段甚至可以很好地适应标准大小的类型：

```
结构体 tx_desc
{
	uint64_t 地址；
	uint16_t 长度；
	uint8_t cso;
	uint8_t cmd;
	uint8_t 状态；
	uint8_t css；
	uint16_t 特殊；
};
```

您的驱动程序必须为传输描述符数组和传输描述符指向的数据包缓冲区保留内存。有几种方法可以做到这一点，从动态分配页面到简单地在全局变量中声明它们。无论您选择什么，请记住，E1000 直接访问物理内存，这意味着它访问的任何缓冲区必须在物理内存中是连续的。

还有多种方法可以处理数据包缓冲区。我们建议从最简单的开始，是在驱动程序初始化期间为每个描述符保留数据包缓冲区的空间，并简单地将数据包数据复制到这些预先分配的缓冲区中或从这些缓冲区中复制出来。以太网数据包的最大大小为 1518 字节，这限制了这些缓冲区需要多大。更复杂的驱动程序可以动态分配数据包缓冲区（例如，在网络使用率较低时减少内存开销），甚至传递由用户空间直接提供的缓冲区（一种称为“零复制”的技术），但最好从简单开始。

## **练习 5.**

执行第 14.5 节（但不是其小节）中描述的初始化步骤。使用第 13 节作为初始化过程参考的寄存器的参考，使用第 3.3.3 和 3.4 节作为传输描述符和传输描述符数组的参考。

请注意传输描述符数组的对齐要求以及该数组长度的限制。由于 TDLEN 必须是 128 字节对齐且每个传输描述符为 16 字节，因此您的传输描述符数组将需要 8 个传输描述符的一些倍数。但是，不要使用超过 64 个描述符，否则我们的测试将无法测试传输环溢出。

对于 TCTL.COLD，您可以假设全双工操作。对于 TIPG，IEEE 802.3 标准 IPG 参考 13.4.34 节表 13-77 中描述的默认值（不要使用 14.5 节表中的值）。

尝试运行make E1000_DEBUG=TXERR,TX qemu。如果您正在使用 qemu 课程，当您设置 TDT 寄存器时，您应该看到“e1000: tx disabled”消息（因为这发生在您设置 TCTL.EN 之前）并且没有更多的“e1000”消息。

现在传输已初始化，您必须编写代码来传输数据包，并通过系统调用使其可访问用户空间。要传输一个数据包，你必须将它添加到传输队列的尾部，这意味着将数据包数据复制到下一个数据包缓冲区，然后更新 TDT（传输描述符尾部）寄存器以通知卡在队列中还有另一个数据包传输队列。（请注意，TDT 是传输描述符数组的*索引*，而不是字节偏移量；文档对此不是很清楚。）

然而，传输队列只有这么大。如果卡已经落后于传输数据包并且传输队列已满，会发生什么情况？为了检测这种情况，您需要 E1000 的一些反馈。不幸的是，您不能只使用 TDH（传输描述符头）寄存器；该文档明确指出从软件读取该寄存器是不可靠的。但是，如果在传输描述符的命令字段中设置 RS 位，那么当卡在该描述符中传输了数据包时，卡将在描述符的状态字段中设置 DD 位。如果设置了描述符的 DD 位，您就知道回收该描述符并使用它来传输另一个数据包是安全的。

如果用户调用您的传输系统调用，但下一个描述符的 DD 位没有设置，表明传输队列已满，该怎么办？您必须决定在这种情况下该怎么做。您可以简单地丢弃数据包。网络协议对此具有弹性，但如果丢弃大量突发数据包，协议可能无法恢复。您可以改为告诉用户环境它必须重试，就像您对`sys_ipc_try_send`. 这具有推回生成数据的环境的优势。

## **练习 6.**

编写一个函数，通过检查下一个描述符是否空闲，将数据包数据复制到下一个描述符，并更新 TDT 来传输数据包。确保您处理传输队列已满。

现在是测试数据包传输代码的好时机。尝试通过直接从内核调用传输函数来仅传输几个数据包。您不必创建符合任何特定网络协议的数据包来进行测试。运行make E1000_DEBUG=TXERR,TX qemu以运行您的测试。你应该看到类似的东西

```
e1000：索引 0：0x271f00：9000002a 0
...
```

当您传输数据包时。每一行都给出了传输数组中的索引、传输描述符的缓冲区地址、cmd/CSO/长度字段和特殊/CSS/状态字段。如果 QEMU 没有从传输描述符中打印出您期望的值，请检查您是否填写了正确的描述符以及您是否正确配置了 TDBAL 和 TDBAH。如果你得到“e1000: TDH wraparound @0, TDT x, TDLEN y”的消息，这意味着E1000一直在传输队列中不停地运行（如果QEMU不检查这个，它会进入一个无限循环），这可能意味着您没有正确操作 TDT。如果您收到很多“e1000: tx disabled”消息，那么您没有正确设置传输控制寄存器。

一旦 QEMU 运行，您就可以运行tcpdump -XXnr qemu.pcap 以查看您传输的数据包数据。如果您看到来自 QEMU 的预期“e1000: index”消息，但您的数据包捕获为空，请仔细检查您是否填写了传输描述符中的每个必要字段和位（E1000 可能通过了您的传输描述符，但没有认为它必须发送任何东西）。

## **练习 7.**

添加一个系统调用，让您可以从用户空间传输数据包。确切的界面由您决定。不要忘记检查从用户空间传递给内核的任何指针。

## 传输数据包：网络服务器

既然您有一个到设备驱动程序传输端的系统调用接口，就该发送数据包了。输出助手环境的目标是在循环中执行以下操作：接受`NSREQ_OUTPUT`来自核心网络服务器的 IPC 消息，并使用您在上面添加的系统调用将伴随这些 IPC 消息的数据包发送到网络设备驱动程序。该`NSREQ_OUTPUT` IPC的由发送`low_level_output`功能在 `净/ LWIP /乔斯/ JIF / jif.c`，该胶合的LWIP的堆书的网络系统。每个 IPC 将包含一个页面，该页面`union Nsipc`由其`struct jif_pkt pkt`字段中的数据包 组成 （参见`inc/ns.h`）。 `struct jif_pkt`好像

```
结构 jif_pkt {
    int jp_len;
    字符 jp_data[0];
};
```

`jp_len`表示数据包的长度。IPC 页上的所有后续字节都专用于数据包内容。`jp_data`在结构的末尾使用零长度数组是一种常见的 C 技巧（有些人会说可恶），用于表示没有预定长度的缓冲区。由于 C 不进行数组边界检查，只要确保结构后面有足够的未使用内存，就可以`jp_data`像使用任何大小的数组一样使用它。

当设备驱动程序的传输队列中没有更多空间时，请注意设备驱动程序、输出环境和核心网络服务器之间的交互。核心网服务器使用IPC向输出环境发送数据包。如果输出环境由于发送数据包系统调用而暂停，因为驱动程序没有更多缓冲区空间用于新数据包，核心网络服务器将阻塞等待输出服务器接受 IPC 调用。

## **练习 8.** 

实现`net/output.c`。

您可以使用`net/testoutput.c`来测试您的输出代码，而无需涉及整个网络服务器。尝试运行 make E1000_DEBUG=TXERR,TX run-net_testoutput。你应该看到类似的东西

```
发送数据包 0
e1000：索引 0：0x271f00：9000009 0
发送数据包1
e1000：索引 1：0x2724ee：9000009 0
...
```

并且tcpdump -XXnr qemu.pcap应该输出

```
从文件 qemu.pcap 中读取，链接类型 EN10MB（以太网）
-5:00:00.600186 [|以太]
	0x0000: 5061 636b 6574 2030 30 Packet.00
-5:00:00.610080 [|以太]
	0x0000: 5061 636b 6574 2030 31 Packet.01
...
```

要使用更大的数据包数进行测试，请尝试 make E1000_DEBUG=TXERR,TX NET_CFLAGS=-DTESTOUTPUT_COUNT=100 run-net_testoutput。如果这溢出了您的传输环，请仔细检查您是否正确处理了 DD 状态位，并且您已告诉硬件设置 DD 状态位（使用 RS 命令位）。

您的代码应通过`testoutput`的测试make grade。

**问题**

1. 你是如何构建你的传输实现的？特别是发送环满了怎么办？

# B 部分：接收数据包和 Web 服务器

## 接收数据包

就像传输数据包一样，您必须配置 E1000 以接收数据包并提供接收描述符队列和接收描述符。3.2节描述了数据包接收的工作原理，包括接收队列结构和接收描述符，14.4节详细介绍了初始化过程。

## **练习 9.**

阅读第 3.2 节。您可以忽略有关中断和校验和卸载的任何内容（如果您决定稍后使用这些功能，您可以返回到这些部分），并且您不必关心阈值的细节以及卡的内部缓存如何工作。

接收队列与发送队列非常相似，不同之处在于它由等待被传入数据包填充的空数据包缓冲区组成。因此，当网络空闲时，传输队列为空（因为所有数据包都已发送），但接收队列已满（空数据包缓冲区）。

当 E1000 收到数据包时，它首先检查它是否与卡配置的过滤器匹配（例如，查看数据包是否寻址到此 E1000 的 MAC 地址），如果不匹配任何过滤器，则忽略该数据包。否则，E1000 会尝试从接收队列的头部检索下一个接收描述符。如果头 (RDH) 已赶上尾 (RDT)，则接收队列中的空闲描述符已用完，因此卡会丢弃数据包。如果有空闲的接收描述符，它将包数据复制到描述符指向的缓冲区中，设置描述符的 DD（描述符完成）和 EOP（包结束）状态位，并递增 RDH。

如果 E1000 接收到的数据包大于一个接收描述符中的数据包缓冲区，它将从接收队列中检索所需数量的描述符以存储数据包的全部内容。为了表明这已经发生，它将在所有这些描述符上设置 DD 状态位，但只在这些描述符中的最后一个上设置 EOP 状态位。您可以在驱动程序中处理这种可能性，或者简单地将卡配置为不接受“长数据包”（也称为*巨型帧*），并确保您的接收缓冲区足够大以存储可能最大的标准以太网数据包（1518 字节） ）。

## **练习 10.**

按照 14.4 节的过程设置接收队列并配置 E1000。您不必支持“长数据包”或多播。现在，不要将卡配置为使用中断；如果您决定使用接收中断，您可以稍后更改它。此外，配置 E1000 以剥离以太网 CRC，因为等级脚本希望它被剥离。

默认情况下，卡将过滤掉*所有*数据包。您必须使用卡自己的 MAC 地址配置接收地址寄存器（RAL 和 RAH），以便接受发往该卡的数据包。您可以简单地对 QEMU 的默认 MAC 地址 52:54:00:12:34:56 进行硬编码（我们已经在 lwIP 中对其进行了硬编码，因此在这里这样做也不会使事情变得更糟）。字节顺序要非常小心；MAC地址是从最低字节到最高字节写入的，所以52:54:00:12是MAC地址的低32位，34:56是高16位。

E1000 仅支持一组特定的接收缓冲区大小（在 13.4.22 中的 RCTL.BSIZE 描述中给出）。如果您使接收数据包缓冲区足够大并禁用长数据包，您就不必担心跨越多个接收缓冲区的数据包。另外，请记住，就像传输一样，接收队列和数据包缓冲区在物理内存中必须是连续的。

您应该至少使用 128 个接收描述符

您现在可以对接收功能进行基本测试，甚至无需编写接收数据包的代码。运行 make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput。 `测试输入`将传输一个 ARP（地址解析协议）公告数据包（使用您的数据包传输系统调用），QEMU 将自动回复。即使您的驱动程序还无法收到此回复，您应该会看到“e1000: unicast match[0]: 52:54:00:12:34:56”消息，表明 E1000 收到了一个数据包并匹配配置的接收过滤器。如果您看到“e1000: unicast mismatch: 52:54:00:12:34:56”消息，则说明 E1000 过滤掉了数据包，这意味着您可能没有正确配置 RAL 和 RAH。确保您的字节顺序正确并且没有忘记在 RAH 中设置“地址有效”位。如果您没有收到任何“e1000”消息，您可能没有正确启用接收。

现在您已准备好实现接收数据包。要接收数据包，您的驱动程序必须跟踪它希望保存下一个接收到的数据包的描述符（提示：根据您的设计，E1000 中可能已经有一个寄存器来跟踪它）。与传输类似，文档指出无法从软件可靠地读取 RDH 寄存器，因此为了确定数据包是否已传送到此描述符的数据包缓冲区，您必须读取描述符中的 DD 状态位。如果设置了 DD 位，您可以从该描述符的数据包缓冲区中复制数据包数据，然后通过更新队列的尾部索引 RDT 来告诉卡该描述符是空闲的。

如果 DD 位未设置，则没有接收到数据包。这是发送队列已满时的接收端等效项，在这种情况下您可以执行多种操作。您可以简单地返回“再试一次”错误并要求调用者重试。虽然这种方法适用于完整的传输队列，因为这是一种瞬态条件，但对于空的接收队列来说不太合理，因为接收队列可能会在很长一段时间内保持为空。第二种方法是暂停调用环境，直到接收队列中有要处理的数据包。这个策略非常类似于`sys_ipc_recv`. 就像在 IPC 的情况下一样，由于每个 CPU 只有一个内核堆栈，一旦我们离开内核，堆栈上的状态就会丢失。我们需要设置一个标志，表明环境已被接收队列下溢挂起，并记录系统调用参数。这种方法的缺点是复杂：必须指示 E1000 生成接收中断，并且驱动程序必须处理它们以恢复阻塞等待数据包的环境。

## **练习 11.**

编写一个函数来接收来自 E1000 的数据包，并通过添加系统调用将其暴露给用户空间。确保您处理接收队列为空。

*挑战！* 如果传输队列已满或接收队列为空，则环境和您的驱动程序可能会花费大量 CPU 周期轮询、等待描述符。E1000 可以在完成发送或接收描述符后生成中断，从而无需轮询。修改您的驱动程序，以便处理发送和接收队列都是中断驱动的，而不是轮询。

请注意，一旦中断被断言，它将保持断言直到驱动程序清除中断。在您的中断处理程序中，请确保在处理中断后立即清除该中断。如果不这样做，从中断处理程序返回后，CPU 将再次跳回它。除了清除E1000卡上的中断外，还需要清除LAPIC上的中断。使用`lapic_eoi`这样做。

## 接收数据包：网络服务器

在网络服务器输入环境中，您将需要使用新的接收系统调用来接收数据包，并使用`NSREQ_INPUT`IPC 消息将它们传递到核心网络服务器环境。这些 IPC 输入消息应该附有一个页面，`union Nsipc`其中的`struct jif_pkt pkt`字段填充了从网络接收到的数据包。

## **练习 12.** 

实现`net/input.c`。

再次 运行`testinput`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput。你应该看到

```
正在发送 ARP 通知...
等待数据包...
e1000：索引 0：0x26dea0：900002a 0
e1000：单播匹配[0]：52:54:00:12:34:56
输入：0000 5254 0012 3456 5255 0a00 0202 0806 0001
输入：0010 0800 0604 0002 5255 0a00 0202 0a00 0202
输入：0020 5254 0012 3456 0a00 020f 0000 0000 0000
输入：0030 0000 0000 0000 0000 0000 0000 0000 0000
```

以“input:”开头的行是 QEMU ARP 回复的十六进制转储。

您的代码应通过`testinput`的测试make grade。请注意，如果不发送至少一个 ARP 数据包来通知 QEMU JOS 的 IP 地址，就无法测试数据包接收，因此传输代码中的错误可能导致此测试失败。

为了更彻底地测试您的网络代码，我们提供了一个名为`echosrv`的守护进程 ，它设置了一个运行在端口 7 上的回显服务器，它将回显通过 TCP 连接发送的任何内容。用于 make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-echosrv在一个终端和make nc-7另一个终端中启动回显服务器以连接到它。您键入的每一行都应由服务器回显。每次模拟的 E1000 收到数据包时，QEMU 应该在控制台打印如下内容：

```
e1000：单播匹配[0]：52:54:00:12:34:56
e1000：索引 2：0x26ea7c：9000036 0
e1000：索引 3：0x26f06a：9000039 0
e1000：单播匹配[0]：52:54:00:12:34:56
```

此时，您应该也可以通过`echosrv` 测试。

**问题**

1. 你是如何构建你的接收实现的？特别是，如果接收队列为空并且用户环境请求下一个传入数据包，您会怎么做？

*挑战！* 阅读开发者手册中关于 EEPROM 的内容，编写代码将 E1000 的 MAC 地址从 EEPROM 中加载出来。目前，QEMU 的默认 MAC 地址被硬编码到您的接收初始化和 lwIP 中。修复您的初始化以使用您从 EEPROM 读取的 MAC 地址，添加系统调用以将 MAC 地址传递给 lwIP，并将 lwIP 修改为从卡读取的 MAC 地址。通过将 QEMU 配置为使用不同的 MAC 地址来测试您的更改。

*挑战！*将您的 E1000 驱动程序修改为“零拷贝”。目前，数据包数据必须从用户空间缓冲区复制到发送数据包缓冲区，并从接收数据包缓冲区复制回用户空间缓冲区。零拷贝驱动程序通过让用户空间和 E1000 直接共享数据包缓冲内存来避免这种情况。对此有许多不同的方法，包括将内核分配的结构映射到用户空间或将用户提供的缓冲区直接传递到 E1000。无论您采用何种方法，请注意重用缓冲区的方式，以免在用户空间代码和 E1000 之间引入竞争。

*挑战！* 将零拷贝概念一直带入 lwIP。

一个典型的数据包由许多报头组成。用户在一个缓冲区中发送要传输到 lwIP 的数据。TCP 层要添加 TCP 头，IP 层要添加 IP 头，MAC 层要添加以太网头。尽管数据包有很多部分，但现在需要将这些部分连接在一起，以便设备驱动程序可以发送最终数据包。

E1000 的传输描述符设计非常适合收集分散在内存中的数据包片段，例如在 lwIP 内部创建的数据包片段。如果您将多个传输描述符排入队列，但只在最后一个设置 EOP 命令位，那么 E1000 将在内部连接来自这些描述符的数据包缓冲区，并且仅在到达 EOP 标记的描述符时传输连接的缓冲区。因此，单个数据包片段永远不需要在内存中连接在一起。

更改您的驱动程序，使其能够发送由许多缓冲区组成的数据包，而无需复制和修改 lwIP，以避免像现在这样合并数据包片段。

*挑战！* 扩充您的系统调用接口以服务多个用户环境。如果有多个网络堆栈（和多个网络服务器），每个网络堆栈都有自己的 IP 地址在用户模式下运行，这将被证明是有用的。接收系统调用需要决定将每个传入数据包转发到哪个环境。

请注意，当前接口无法区分两个数据包之间的区别，如果多个环境调用数据包接收系统调用，则每个相应的环境将获得传入数据包的子集，该子集可能包括不以调用环境为目的地的数据包。

[这篇](http://pdos.csail.mit.edu/papers/exo:tocs.pdf) Exokernel 论文中的 第 2.2 节和第 3 节 对这个问题进行了深入的解释，以及在像 JOS 这样的内核中解决它的方法。使用论文来帮助您解决问题，您可能不需要论文中提出的那么复杂的解决方案。

## 网络服务器

最简单形式的 Web 服务器将文件的内容发送到请求客户端。我们在`user/httpd.c 中`为一个非常简单的 web 服务器提供了框架代码。骨架代码处理传入的连接并解析标头。

## **练习 13.** 

Web 服务器缺少处理将文件内容发送回客户端的代码。通过实现`send_file`和 来完成 Web 服务器`send_data`。

完成 Web 服务器后，启动 Web 服务器 ( make run-httpd-nox) 并将您喜欢的浏览器指向 http:// *host* : *port* /index.html，其中*host*是运行 QEMU 的计算机的名称（如果您正在运行 QEMU athena 使用`hostname.mit.edu`（主机名是` hostname`athena上命令的输出，如果您在同一台计算机上运行 Web 浏览器和 QEMU，则`主机`名是`localhost`）并且*port* 是由 为 Web 服务器报告的端口号make which-ports 。您应该查看由在 JOS 中运行的 HTTP 服务器提供的网页。

此时，您应该在 上得分 105/105 make grade。

*挑战！* 向 JOS 添加一个简单的聊天服务器，多人可以连接到服务器，任何用户输入的任何内容都会传输给其他用户。要做到这一点，你必须找到一种方法同时处理多个插座进行沟通 *，并*在同一时间发送和接收同一插座上。有多种方法可以解决这个问题。lwIP 为 recv 提供了一个 MSG_DONTWAIT 标志（请参阅 `net/lwip/api/ ``sockets.c`中的`lwip_recvfrom`），因此您可以不断循环所有打开的套接字，轮询它们以获取数据。请注意，虽然网络服务器 IPC 支持`recv`标志，但无法通过常规`读取`访问它们``````函数，所以你需要一种方法来传递标志。一种更有效的方法是为每个连接启动一个或多个环境，并使用 IPC 来协调它们。方便的是，在套接字的结构 Fd 中找到的 lwIP 套接字 ID 是全局的（不是每个环境），因此，例如，`fork`的子级 继承其父级套接字。或者，一个环境甚至可以通过构造一个包含正确套接字 ID 的 Fd 来发送另一个环境的套接字。

**问题**

1. JOS 的网络服务器提供的网页内容是什么？
2. 你做这个实验大约花了多长时间？

# 回答问题汇总

既然有了文件系统，那么网络功能就能在基础上进行搭建。在本实验中，我们将编写一些网络接口卡的**驱动程序**和网络服务进程的**中间层**。网卡将基于英特尔 82540EM 芯片，也称为 E1000。

# 网络驱动总览

我们将使用 QEMU 的用户模式网络堆栈，因为它不需要管理权限即可运行。实验更新了 makefile 以启用 QEMU 的用户模式网络堆栈和虚拟 E1000 网卡。

QEMU为 JOS 分配 IP 地址 10.0.2.15并NAT转换为10.0.2.2。为了简单`起见`，我们将这些默认值硬编码到网络服务器的`net/ns.h 中`。

虽然 QEMU 的虚拟网络允许 JOS 与 Internet 进行任意连接，但 JOS 的 10.0.2.15 地址在 QEMU 内部运行的虚拟网络之外没有任何意义（即 QEMU 充当 NAT），因此我们无法直接连接到服务器在 JOS 内部运行，甚至从运行 QEMU 的主机运行。为了解决这个问题，**我们将 QEMU 配置为在*主机*上的某个端口上运行一个服务器**，该服务器只需连接到 JOS 中的某个端口，并在您的真实主机和虚拟网络之间来回传输数据。

您将在端口 7 (echo) 和 80 (http) 上运行 JOS 服务器。

## 数据包检查

makefile 还配置 QEMU 的网络堆栈以将所有传入和传出数据包记录到实验室目录中的`qemu.pcap`。

要获取捕获数据包的十六进制/ASCII 转储，请使用`tcpdump，`如下所示：

```
tcpdump -XXnr qemu.pcap
```

## 调试E1000

我们很幸运能够使用仿真硬件。由于 E1000 在软件中运行，仿真的 E1000 可以以用户可读的格式向我们报告其内部状态和遇到的任何问题。通常，对于使用裸机编写的驱动程序开发人员来说，是不可能的。

E1000 可以产生大量debug输出，因此您必须启用特定的日志记录通道。您可能会觉得有用的一些make flag是

| Flag      | Meaning                                            |
| --------- | -------------------------------------------------- |
| tx        | Log packet transmit operations                     |
| txerr     | Log transmit ring errors                           |
| rx        | Log changes to RCTL                                |
| rxfilter  | Log filtering of incoming packets                  |
| rxerr     | Log receive ring errors                            |
| unknown   | Log reads and writes of unknown registers          |
| eeprom    | Log reads from the EEPROM                          |
| interrupt | Log interrupts and changes to interrupt registers. |

例如，要启用“tx”和“txerr”日志记录，请使用 make E1000_DEBUG=tx,txerr ....

*注意：* `E1000_DEBUG`标志仅适用于 6.828 版本的 QEMU。

您可以更进一步地使用软件模拟硬件进行调试。如果您曾经被卡住并且不明白为什么 E1000 没有按照您预期的方式响应，您可以在`hw/e1000.c 中`查看 QEMU 的 E1000 实现。

## 网络服务器

我们将使用 lwIP而不是从头编写驱动程序，这是一个开源的轻量级 TCP/IP 协议套件，其中包括一个网络堆栈。您可以在[此处](http://www.sics.se/~adam/lwip/)找到有关 lwIP 的更多信息 。在本次作业中，就我们而言，lwIP 是一个实现了 BSD 套接字接口的黑盒，并具有数据包输入端口和数据包输出端口。

网络服务器实际上是四种环境的组合：

- 核心网络服务器环境（包括socket调用调度器和lwIP）
- 输入环境
- 输出环境
- 定时器环境

下图显示了不同的环境及其关系。该图显示了包括设备驱动程序在内的整个系统，稍后将对其进行介绍。在本实验中，我们将实现以绿色突出显示的部分。

![网络服务器架构](https://pdos.csail.mit.edu/6.828/2017/labs/lab6/ns.png)

- 用户应用httpd
- 用户态：输出env和输出env（用来和内核/核心网络环境进行通信）
- 内核态：E1000驱动网络env

### 核心网服务器环境

核心网服务器环境由socket调用和lwIP本身组成。套接字调用调度程序的工作方式与文件服务器完全一样。用户环境将 IPC 消息（在`lib/nsipc.c 中`找到）发送到核心网络环境。

如果查看 `lib/nsipc.c，`您会发现我们找到核心网络服务器的方式与找到文件服务器的方式相同：`i386_init`使用 NS_TYPE_NS 创建 NS 环境，因此我们进行扫描`envs`，寻找这种特殊的环境类型。对于每个用户环境IPC，网络服务器中的调度器代表用户调用lwIP提供的相应BSD套接字接口函数。

常规用户环境不直接调用`nsipc_*`。相反，他们使用`lib/sockets.c 中`的函数，它提供了一个基于文件描述符的套接字 API。因此，用户环境通过fd引用套接字，就像它们引用磁盘文件一样。多个操作（`connect`，`accept`等）使用特定接口，但是`read`，`write`和 `close`依旧是正常操作正常文件描述符的设备代码`LIB / fd.c`。因此类似文件系统的实现fd，lwIP 也为所有打开的套接字生成唯一 ID。在文件服务器和网络服务器中，我们使用存储在其中的信息将`struct Fd`每个环境的文件描述符映射到这些唯一的 ID 空间。

尽管看起来文件服务器和网络服务器的 IPC 调度程序的行为相同，但还是有一个关键的区别。BSD 套接字调用比如 `accept`并且`recv`可以无限期地阻塞。如果调度程序让 lwIP 执行这些阻塞调用之一，调度程序也会阻塞，并且整个系统一次只能有一个未完成的网络调用。由于这是不可接受的，网络服务器使用**用户级线程**来避免阻塞整个服务器环境。对于每个传入的 IPC 消息，调度程序创建一个线程并在新创建的线程中处理请求。如果线程阻塞，则只有该线程进入睡眠状态，而其他线程继续运行。

除了核心网络环境，还有三个辅助环境。除了接受来自用户应用程序的消息，核心网络环境的调度器还接受来自输入和定时器环境的消息。

### 输出环境

在服务用户环境套接字调用时，lwIP 将生成数据包供网卡传输。LwIP 将使用`NSREQ_OUTPUT`IPC 把将要传输的每个数据包发送到input helper environment，数据包附加在 IPC 消息的页面参数中。输出env负责接受这些消息，并通过**即将创建的**系统调用接口将数据包转发到设备驱动程序。

### 输入环境

网卡收到的数据包需要注入lwIP。对于设备驱动程序接收到的每个数据包，输入环境将数据包拉出内核空间（使用您将实现的内核系统调用）并使用`NSREQ_INPUT`IPC 消息将数据包发送到核心服务器环境。

问题：为什么数据包输入功能与内核网络环境分离？是因为 JOS 很难**同时接受 IPC 消息和轮询或等待来自设备驱动程序的数据包**。我们使用`select`来I/O复用， 因此input发送后服务器select环境将会O（n）查找是哪个线程，借而切换进行消息处理。

### 定时器环境

定时器环境定期向`NSREQ_TIMER`核心网络服务器发送类型消息，通知它定时器已到期。lwIP 使用来自该线程的计时器消息来实现各种网络超时。

## Exercise1

1.在`kern/trap.c `中使用`time_tick`，保证一开始进行10ms的计时开始

2.在`kern/ syscall.c`中实现`sys_time_msec`并把它添加到`syscall`，保证每次中断询问时间都可以回应

使用make INIT_CFLAGS=-DTEST_NO_NS run-testtime， INIT_CFLAGS=-DTEST_NO_NS是屏蔽网络，否则测试出错

## Exercise2

浏览英特尔的E1000[软件开发人员手册](https://pdos.csail.mit.edu/6.828/2017/readings/hardware/8254x_GBe_SDM.pdf)（本文件夹下）。本手册涵盖了几个密切相关的以太网控制器。QEMU 模拟 82540EM。

您现在应该浏览第 2 章以了解该设备。要编写驱动程序，您需要熟悉第 3 章和第 14 章以及 4.1（尽管不是 4.1 的小节）。您还需要使用第 13 章作为参考。

总的来说，E1000通过了一些硬件手段加速接收数据，在接收到QEMU作为服务器发送的包后，使用DMA写入内存，然后中断请求IRQ，然后处理，一路向上到达我们的LwIP处理进程。

同时使用[e1000_hw.h](https://pdos.csail.mit.edu/6.828/2017/labs/lab6/e1000_hw.h)对接下来的实验很有帮助，建议配合阅读。

简要讲解一下E1000，作为PCI设备，需要接入PCI总线上，因此给定以下数据结构我们可以看到：pci_bus是总线，pci_func则规定了接入的设备，id，寄存器，寄存器大小等细节。

```
struct pci_func {
    struct pci_bus *bus;	// Primary bus for bridges

    uint32_t dev;
    uint32_t func;

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};

struct pci_bus {
    struct pci_func *parent_bridge;
    uint32_t busno;
} 
```



## Exercise3

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112202204129.png)

在`kern/e100.h` `kern/e100.c`实现一个attachfn函数来初始化 E1000。

如果找到匹配的 PCI 设备，则向`kern/pci.c 中`的`pci_attach_vendor`数组添加一个条目，第 5.2 节中找到 QEMU 模拟的 82540EM 的供应商 ID 和设备 ID。上图为100E和8086，我们为此定义两个宏（模仿e1000 hw.h文件），再添加触发函数。此外保证末尾应该有： `{0, 0, 0}`

对于数组由pci_driver组成

```
struct pci_driver {
    uint32_t key1, key2;
    int (*attachfn) (struct pci_func *pcif);
};
```

kern/e1000.c

```
int
e1000_attachfn(struct pci_func *pcif)
{
       pci_func_enable(pcif);
       return 0;
}
```

kern/pci.c：

```
 //两个宏定义为 100e 8086
 struct pci_driver pci_attach_vendor[] = {
       { E1000_VENDER_ID_82540EM, E1000_DEV_ID_82540EM, &e1000_attachfn },
        { 0, 0, 0 },
 };
```



## Exercise4

在刚刚的attachfn函数中，通过调用`mmio_map_region`（实验 4 中编写的支持内存映射 LAPIC）为 E1000 的 BAR 0 创建虚拟内存映射 。

我们需要将这个映射的位置记录在一个变量中，以便以后可以访问刚刚映射的寄存器。查看`kern/lapic.c`中的`lapic`变量，来确认细节。如果使用指向设备寄存器映射的指针，请务必使用volatile声明它 ；否则，编译器将可能不会立即写入变量。

然后尝试打印设备状态寄存器（第 13.4.2 节）。这是一个 4 字节的寄存器，从寄存器空间的第 8 字节开始。您应该得到`0x80080783`，这表明全双工链路的速度为 1000 MB/s，等等。

前面lab预留的IO孔在这里使用：程序通过内存映射IO（MMIO）和E1000交互。我们直接读写设备。pci_func_enable()决定MMIO范围，并将基址和对应size保存在基地址寄存器0（reg_base[0] and reg_size[0]）中，因此使用mmio_map_region函数来填充kern_pgdir和内核页表，然后我们访问写入的第八个寄存器，确认是否为`0x80080783`（这表明全双工链路的速度为 1000 MB/s）。这里同样使用宏来为以后的使用提供便利。

kern/e1000.c

```c
//宏E1000_STATUS 0x8
//宏GET_E1000_REG(offset) *(uint32_t*)(bar_va + offset)

volatile void *bar_va;
int
e1000_attachfn(struct pci_func *pcif){
	pci_func_enable(pcif);
	bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);//bar_va == base of mapping va
	uint32_t status_reg = GET_E1000_REG(E1000_STATUS);
	assert(status_reg == 0x80080783);
	
	return 0;
}
```

## Exercise5

仔细观察上图，我们会发现E1000通过DMA而不是寄存器读写内存：即使用DMA描述符来读写文件，当我们写入内存时，内存提供了ring（环形数组）来存放（很像管道的实现）。对于DMA和描述符和fd而言，最大的区别就是前者不通过CPU和MMU来文件传输，因此地址是物理地址而不是虚拟地址或者多级指针。

对于这部分，我们将要完成以下设置：

执行第 14.5 节（但不是其小节）中描述的初始化步骤。使用第 13 节作为初始化过程参考的寄存器的参考，使用第 3.3.3 和 3.4 节作为传输描述符和传输描述符数组的参考。

![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112211848841.png)![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112211849036.png)

1. 分配内存作为Tx ring，起始地址要16字节对齐。用基地址填充(TDBAL/TDBAH) 寄存器。
2. 设置(TDLEN)寄存器，保存size，128字节对齐。
3. 设置(TDH/TDT)寄存器，分别指向ring头部和尾部。初始化为0。
4. 初始化TCTL寄存器。设置TCTL.EN位为1，设置TCTL.PSP位为1。设置TCTL.CT为10h。设置TCTL.COLD为40h。
5. 设置TIPG寄存器。



![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112211850304.png)![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112211909695.png)![](https://gitee.com/Khalil-Chen/img_bed/raw/master/img/202112211908572.png)

这里看了半天没看懂，实在不是写硬件的，很惭愧，借鉴了别人的代码。

make E1000_DEBUG=TXERR,TX qemu。应该看到“e1000: tx disabled”消息

```c
struct e1000_tdh *tdh;
struct e1000_tdt *tdt;
struct e1000_tx_desc tx_desc_array[TXDESCS];
char tx_buffer_array[TXDESCS][TX_PKT_SIZE];

static void
e1000_transmit_init()
{
       int i;
       for (i = 0; i < TXDESCS; i++) {
               tx_desc_array[i].addr = PADDR(tx_buffer_array[i]);
               tx_desc_array[i].cmd = 0;
               tx_desc_array[i].status |= E1000_TXD_STAT_DD;
       }
	//设置长度寄存器
       struct e1000_tdlen *tdlen = (struct e1000_tdlen *)E1000REG(E1000_TDLEN);
       tdlen->len = TXDESCS;
			 
	//设置基址低32位
       uint32_t *tdbal = (uint32_t *)E1000REG(E1000_TDBAL);
       *tdbal = PADDR(tx_desc_array);

	//设置基址高32位
       uint32_t *tdbah = (uint32_t *)E1000REG(E1000_TDBAH);
       *tdbah = 0;

	//设置头指针寄存器
       tdh = (struct e1000_tdh *)E1000REG(E1000_TDH);
       tdh->tdh = 0;

	//设置尾指针寄存器
       tdt = (struct e1000_tdt *)E1000REG(E1000_TDT);
       tdt->tdt = 0;

	//设置TCTL寄存器
       struct e1000_tctl *tctl = (struct e1000_tctl *)E1000REG(E1000_TCTL);
       tctl->en = 1;
       tctl->psp = 1;
       tctl->ct = 0x10;
       tctl->cold = 0x40;

	//设置TIPG寄存器
       struct e1000_tipg *tipg = (struct e1000_tipg *)E1000REG(E1000_TIPG);
       tipg->ipgt = 10;
       tipg->ipgr1 = 4;
       tipg->ipgr2 = 6;
}

```



## Exercise6

1.如果发送队列满了怎么办？
当网卡发送了一个描述符指向的数据包后，会设置该描述符的DD位，结合DMA描述符RS标志位就能知道某个描述符是否能被回收。
我们使用重试的方式，就像sys_ipc_try_send()一样。

```c
int
e1000_transmit(void *data, size_t len)
{
       uint32_t current = tdt->tdt;		
       if(!(tx_desc_array[current].status & E1000_TXD_STAT_DD)) {
               return -E_TRANSMIT_RETRY;
       }
       tx_desc_array[current].length = len;
       tx_desc_array[current].status &= ~E1000_TXD_STAT_DD;
       tx_desc_array[current].cmd |= (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
       memcpy(tx_buffer_array[current], data, len);
       uint32_t next = (current + 1) % TXDESCS;
       tdt->tdt = next;
       return 0;
}
```



## Exercise7

代码简单，忽略。

## Exercise8

1.执行一个无限循环，接收核心网络进程的IPC请求

2.解析该请求，然后发送数据。

net/output.c.

```c
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	uint32_t whom;
	int perm;
	int32_t req;

	while (1) {
		req = ipc_recv((envid_t *)&whom, &nsipcbuf,  &perm);     
		if (req != NSREQ_OUTPUT) {
			cprintf("output: not output_ipc\n");
			continue;
		}

    	struct jif_pkt *pkt = &(nsipcbuf.pkt);
    	int i = 100;
    	while(sys_pkt_send(pkt->jp_data, pkt->jp_len) < 0 && i-- > 0) { //重试100次       
       		sys_yield();
    	}	
	}
}
```

## Exercise9



## Exercise10



## Exercise11



## Exercise12



## Exercise13


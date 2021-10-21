# 作业：xv6 系统调用

在下一讲开始之前将您的解决方案提交到[提交网站](https://6828.scripts.mit.edu/2017/handin.py/)。

您将修改 xv6 以添加系统调用。您可以使用与[引导作业](https://pdos.csail.mit.edu/6.828/2017/homework/xv6-boot.html)相同的设置。

## 第一部分：系统调用跟踪

您的第一个任务是修改 xv6 内核，为每个系统调用打印出一行。打印系统调用的名称和返回值就足够了；您不需要打印系统调用参数。

完成后，您应该在启动 xv6 时看到如下输出：

```
...
fork -> 2
exec -> 0
open -> 3
close -> 0
$write -> 1
 write -> 1
```

那是 init fork和执行 sh，sh 确保只有两个文件描述符打开，然后 sh 写入 $ 提示。（注意：shell 的输出和系统调用跟踪是混合的，因为 shell 使用 write syscall 来打印其输出。）

提示：修改 syscall.c 中的 syscall() 函数。

可选挑战：打印系统调用参数。

## 第二部分：日期系统调用

您的第二个任务是向 xv6 添加一个新的系统调用。本练习的重点是让您了解系统调用机制的一些不同部分。您的新系统调用将获取当前 UTC 时间并将其返回给用户程序。您可能需要使用辅助函数`cmostime()` （在`lapic.c 中`定义）来读取实时时钟。`date.h` 包含`struct rtcdate`结构的定义，您将其作为参数提供给`cmostime()`作为指针。

您应该创建一个用户级程序来**调用**您的新日期系统调用；这是您应该放入`date.c`的一些来源：

```
#include "types.h"
#include "user.h"
#include "date.h"

int
main(int argc, char *argv[])
{
  struct rtcdate r;

  if(date(＆r)){
    printf(2, "date failed\n");
    exit();
  }

  // 您的代码,以您喜欢的任何格式打印时间...

  exit();
}
```

为了使您的新`日期`程序可用于从 xv6 shell 运行，请将`_date`添加到`Makefile 中`的`UPROGS`定义。

您进行日期系统调用的策略：应该是复制特定于某些**现有**系统调用的所有代码，例如“正常运行时间”系统调用。您应该使用`grep -n uptime *.[chS]`对所有源文件中的`正常运行时间`进行`grep`查找。

完成后，在 xv6 shell 提示中输入`date`应该会打印当前的 UTC 时间。

为在创建日期系统调用的过程中必须修改的每个文件写下几句话的解释。

可选挑战：添加 dup2() 系统调用并修改 shell 以使用。



**提交**：您在名为 hwN.txt 的文件中对日期修改的解释，其中 N 是日程表上发布的作业编号。
# 作业：shell

注：本文完成的代码和相关测试均在本文件夹下完成，需要提示可以从lab1下的xv6教程0章找寻答案

通过在一个小的 shell 中实现几个特性，这个任务将使你更加熟悉 Unix 系统调用接口和 shell，我们将其称为 6.828 shell。您可以在任何支持 Unix API 的操作系统（Linux Athena 机器、装有 Linux 或 MacOS 的笔记本电脑等）上执行此任务。将您的 6.828 shell 作为名为“hwN.c”的文本文件[提交](https://6828.scripts.mit.edu/2017/handin.py/)到[提交网站](https://6828.scripts.mit.edu/2017/handin.py/)，其中 N 是日程表中列出的作业编号。

阅读[xv6 书籍的](https://pdos.csail.mit.edu/6.828/2017/xv6/book-rev10.pdf)第 0 章。

如果您不熟悉 shell 的作用，请从 6.033 开始[动手](http://web.mit.edu/6.033/www/assignments/handson-unix.html)操作[Unix](http://web.mit.edu/6.033/www/assignments/handson-unix.html)。

下载[6.828 shell](https://pdos.csail.mit.edu/6.828/2017/homework/sh.c)并查看它。6.828 shell 包含两个主要部分：解析 shell 命令和实现它们。解析器仅识别简单的 shell 命令，例如：

```
ls > y
cat < y | sort | uniq| wc > y1
cat y1
rm y1
ls | sort | uniq| wc
rm y
```

将这些命令剪切并粘贴到文件`t.sh 中`

要编译`sh.c`，您需要一个 C 编译器，例如 gcc。在 Athena 上，您可以输入：

```
$ add gnu
```

使 gcc 可用。如果您使用自己的计算机，则可能需要安装 gcc。

一旦你有了 gcc，你就可以编译 shell脚本，如下所示：

```
$ gcc sh.c
```

它会生成一个`a.out`文件，您可以运行该文件：

```
$ ./a.out < t.sh
```

此执行将打印错误消息，因为您尚未实现多个功能。在本作业的其余部分中，您将实现这些功能。

## 执行简单的命令

实现简单的命令，例如：

```
ls
```

解析器已经为您构建了一个`execcmd`，因此您唯一需要编写的代码是`runcmd 中`的情况。您可能会发现查看 exec 的手册页很有用；键入“man 3 exec”，然后阅读有关 `execv 的信息`。exec 失败时打印错误消息。

要测试您的程序，请编译并运行生成的 a.out：

```
$ ./a.out
```

这将打印提示并等待输入。 `sh.c`打印为提示 `6.828$`这样您就不会与计算机的shell混淆。现在在你的 shell 中输入：

```
6.828$ ls
```

您的 shell 可能会打印一条错误消息（除非您的工作目录中有名为`ls`的程序 ，或者您正在使用搜索`PATH`的`exec`版本）。现在在你的 shell 中输入：

```
6.828$ /bin/ls
```

这应该执行程序`/bin/ls`，它应该打印出您工作目录中的文件名。您可以通过键入 ctrl-d 来停止 6.828 shell，这应该会让您回到计算机的 shell。

如果当前工作目录中不存在该程序，您可能希望将 6.828 shell 更改为始终尝试`/bin`，这样您就不必在下面为每个程序键入“/bin”。如果您雄心勃勃，您可以实现对`PATH`变量的支持。

## 输入/输出重定向

实现 I/O 重定向命令，以便您可以运行：

```
echo "6.828 is cool" > x.txt
cat < x.txt
```

解析器已经识别 ">" 和 "<"，并为您构建了一个`redircmd`，因此您的工作只是为这些符号填写`runcmd `中缺少的代码。您可能会发现 open 和 close 的man手册很有用。

请注意，`redircmd`中的`mode`字段包含访问模式（例如，`O_RDONLY`），您应该将其传递 给`open`的`flags`参数；有关 shell 正在使用的模式值，请参阅`parseredirs`，有关`flags`参数的`open`手册页。

如果您正在使用的系统调用之一失败，请确保打印错误消息。

确保您的实现使用上述测试输入正确运行。一个常见的错误是忘记指定创建文件时必须使用的权限（即打开的第三个参数）。

## 实施管道

实现管道，以便您可以运行命令管道，例如：

```
$ ls | sort | uniq| wc
```

解析器已经识别出“|”，并为您构建了一个`pipecmd`，因此您必须编写的唯一代码是“|” 情况`RUNCMD`。您可能会发现 pipe、fork、close 和 dup 的手册页很有用。

测试您是否可以运行上述管道。该`类型的`程序可能在目录`的/ usr / bin/`中，在这种情况下，您可以键入绝对路径`的/ usr / bin/sort`来排序运行。（在您计算机的 shell 中，您可以键入`which sort`以找出 shell 搜索路径中的哪个目录具有名为“sort”的可执行文件。）

现在您应该能够正确运行以下命令：

```
6.828$ a.out < t.sh
```

确保为程序使用正确的绝对路径名。

不要忘记将您的解决方案提交到[提交网站](https://6828.scripts.mit.edu/2017/handin.py/) （使用上面概述的“hwN.c”命名约定），无论是否有挑战解决方案。

## 可选挑战练习

以下练习完全是可选的，不会影响您的成绩。将您选择的任何功能添加到您的 shell，例如：

- 实现命令列表，以“;”分隔
- 通过实现“(”和“)”来实现子shell
- 通过支持“&”和“wait”实现后台运行命令

所有这些都需要对解析器和`runcmd`进行更改功能。
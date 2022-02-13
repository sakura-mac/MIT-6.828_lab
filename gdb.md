gdb:tui enable

Layout reg

layout asm

Focus

info break

info frame 

Frame 0

i args

p *argv@argc:前参数个数的参数

p *argv前1个参数

print/x $pc???

print/x *(p->trapframe)

b trap.c:207//gcc时候需要-g来保证

tbreak

where

```
layout：用于分割窗口，可以一边查看代码，一边测试。主要有以下几种用法： 
layout src：显示源代码窗口 
layout asm：显示汇编窗口 
layout regs：显示源代码/汇编和寄存器窗口 
layout split：显示源代码和汇编窗口 
layout next：显示下一个layout 
layout prev：显示上一个layout 
Ctrl + L：刷新窗口 
Ctrl + x，再按1：单窗口模式，显示一个窗口 
Ctrl + x，再按2：双窗口模式，显示两个窗口 
Ctrl + x，再按a：回到传统模式，即退出layout，回到执行layout之前的调试窗口。
```


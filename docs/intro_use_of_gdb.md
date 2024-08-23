# Intro. 使用GDB

[toc]

GDB全称是GUN Debugger，以带有或者不带有GDB的方式使用`make`指令启动QEMU

- 带有GDB：运行`make qemu[-nox]-gdb`，然后在第二个Shell中启动GDB（`gdb-multiarch`）
- 如果以单核方式启动，则使用`make CPUS=1 qemu-gdb`
- 不带有GDB：当不需要GDB时使用`make qemu[-nox]`命令
- 在 GDB 中，`layout split` 是一个命令，用于将窗口分割成多个部分，以同时显示源代码和汇编代码的视图。这样可以帮助你在调试时更好地跟踪代码的执行。

- 使用GDB，需要加载符号表，但是我们使用`make CPUS=1 qemu-gdb`不会自动加载符号表，一个解决方法是

```bash
sudo gedit ~/.gdbinit		// 修改gdbinit
add-auto-load-safe-path </path/to/your/directory/.gdbinit>		// 添加这句，然后重新编译即可
```

- 使用Ctrl + D退出GDB
- 使用`file kernel/kernel`可以加载符号表

使用的效果如图所示

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/gdb_1.png)

- 使用`layout split`查看多级分布

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/gdb_2.png)

- 输入 `tui enable` 命令会启用 GDB 的文本用户界面（TUI）模式。TUI 模式是一种增强的调试界面，提供了源代码和汇编代码的可视化显示，让你更直观地进行调试。

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/gdb_3.png)

- `layout asm`查看汇编码
- `layout reg`查看寄存器
- `layout source`查看源码
- `layout split`查看汇编和源码
- `focus <window_name>`集中到某个窗口 
- `info breakpoints`查看设置的所有断点


## 5. 中断和设备驱动

[toc]

驱动程序是操作系统中管理特定设备的代码：它配置硬件设备，告诉设备执行操作，处理由此产生的中断，并与可能正在等待设备输入/输出的进程进行交互。编写驱动可能很棘手，因为驱动程序与它管理的设备同时运行。此外，驱动程序必须理解设备的硬件接口，这可能很复杂，而且缺乏文档。

需要操作系统关注的设备通常可以被配置为生成中断，这是陷阱的一种。内核陷阱处理代码识别设备何时引发中断，并调用驱动程序的中断处理程序；在xv6中，这种调度发生在`devintr`中（***kernel/trap.c:177***）。

许多设备驱动程序在两种环境中执行代码：上半部分在进程的内核线程中运行，下半部分在中断时执行。上半部分通过**系统调用**进行调用，如希望设备执行I/O操作的`read`和`write`。这段代码可能会要求硬件执行操作（例如，要求磁盘读取块）；然后代码等待操作完成。最终设备完成操作并引发中断。驱动程序的**中断处理程序**充当下半部分，计算出已经完成的操作，如果合适的话唤醒等待中的进程，并告诉硬件开始执行下一个正在等待的操作。



### 5.1 代码：控制台输入

控制台驱动程序（***console.c***）是驱动程序结构的简单说明。控制台驱动程序通过连接到RISC-V的UART串口硬件接受人们键入的字符。控制台驱动程序一次累积一行输入，处理如`backspace`和`Ctrl-u`的特殊输入字符。用户进程，如Shell，使用`read`系统调用从控制台获取输入行。当您在QEMU中通过键盘输入到xv6时，您的按键将通过QEMU模拟的UART硬件传递到xv6。

驱动程序管理的UART硬件是由QEMU仿真的16550芯片。在真正的计算机上，16550将管理连接到终端或其他计算机的RS232串行链路。运行QEMU时，它连接到键盘和显示器。

UART硬件在软件中看起来是一组**内存映射**的控制寄存器。也就是说，存在一些RISC-V硬件连接到UART的物理地址，以便载入(load)和存储(store)操作与设备硬件而不是内存交互。UART的内存映射地址起始于`0x10000000`或`UART0` (***kernel/memlayout.h:21***)。有几个宽度为一字节的UART控制寄存器，它们关于UART0的偏移量在(***kernel/uart.c:22***)中定义。例如，LSR寄存器包含指示输入字符是否正在等待软件读取的位。这些字符（如果有的话）可用于从RHR寄存器读取。每次读取一个字符，UART硬件都会从等待字符的内部FIFO寄存器中删除它，并在FIFO为空时清除LSR中的“就绪”位。UART传输硬件在很大程度上独立于接收硬件；如果软件向THR写入一个字节，则UART传输该字节。

Xv6的`main`函数调用`consoleinit`（***kernel/console.c:184***）来初始化UART硬件。该代码配置UART：UART对接收到的每个字节的输入生成一个接收中断，对发送完的每个字节的输出生成一个发送完成中断（***kernel/uart.c:53***）。

xv6的shell通过***init.c*** (***user/init.c:19***)中打开的文件描述符从控制台读取输入。对`read`的调用实现了从内核流向`consoleread` (***kernel/console.c:82***)的数据通路。`consoleread`等待输入到达（通过中断）并在`cons.buf`中缓冲，将输入复制到用户空间，然后（在整行到达后）返回给用户进程。如果用户还没有键入整行，任何读取进程都将在`sleep`系统调用中等待（***kernel/console.c:98***）（第7章解释了`sleep`的细节）。

当用户输入一个字符时，UART硬件要求RISC-V发出一个中断，从而激活xv6的陷阱处理程序。陷阱处理程序调用`devintr`（***kernel/trap.c:177***），它查看RISC-V的`scause`寄存器，发现中断来自外部设备。然后它要求一个称为PLIC的硬件单元告诉它哪个设备中断了（***kernel/trap.c:186***）。如果是UART，`devintr`调用`uartintr`。

`uartintr`（***kernel/uart.c:180***）从UART硬件读取所有等待输入的字符，并将它们交给`consoleintr`（***kernel/console.c:138***）；它不会等待字符，因为未来的输入将引发一个新的中断。`consoleintr`的工作是在***cons.buf***中积累输入字符，直到一整行到达。`consoleintr`对`backspace`和其他少量字符进行特殊处理。当换行符到达时，`consoleintr`唤醒一个等待的`consoleread`（如果有的话）。

一旦被唤醒，`consoleread`将监视***cons.buf***中的一整行，将其复制到用户空间，并返回（通过系统调用机制）到用户空间。

### 5.2 代码：控制台输出

在连接到控制台的文件描述符上执行`write`系统调用，最终将到达`uartputc`(***kernel/uart.c:87***) 。设备驱动程序维护一个输出缓冲区（`uart_tx_buf`），这样写进程就不必等待UART完成发送；相反，`uartputc`将每个字符附加到缓冲区，调用`uartstart`来启动设备传输（如果还未启动），然后返回。导致`uartputc`等待的唯一情况是缓冲区已满。

每当UART发送完一个字节，它就会产生一个中断。`uartintr`调用`uartstart`，检查设备是否真的完成了发送，并将下一个缓冲的输出字符交给设备。因此，如果一个进程向控制台写入多个字节，通常第一个字节将由`uartputc`调用`uartstart`发送，而剩余的缓冲字节将由`uartintr`调用`uartstart`发送，直到传输完成中断到来。

需要注意，这里的一般模式是通过**缓冲区**和**中断机制**将设备活动与进程活动解耦。即使没有进程等待读取输入，控制台驱动程序仍然可以处理输入，而后续的读取将看到这些输入。类似地，进程无需等待设备就可以发送输出。这种解耦可以通过允许进程与设备I/O并发执行来提高性能，当设备很慢（如UART）或需要立即关注（如回声型字符(echoing typed characters)）时，这种解耦尤为重要。这种想法有时被称为I/O并发

### 5.3 驱动中的并发

你或许注意到了在`consoleread`和`consoleintr`中对`acquire`的调用。这些调用获得了一个保护控制台驱动程序的数据结构不受并发访问的锁。这里有三种并发风险：运行在不同CPU上的两个进程可能同时调用`consoleread`；硬件或许会在`consoleread`正在执行时要求CPU传递控制台中断；并且硬件可能在当前CPU正在执行`consoleread`时向其他CPU传递控制台中断。第6章探讨了锁在这些场景中的作用。

在驱动程序中需要注意并发的另一种场景是，一个进程可能正在等待来自设备的输入，但是输入的中断信号可能是在另一个进程（或者根本没有进程）正在运行时到达的。因此中断处理程序不允许考虑他们已经中断的进程或代码。例如，中断处理程序不能安全地使用当前进程的页表调用`copyout`（注：因为你不知道是否发生了进程切换，当前进程可能并不是原先的进程）。中断处理程序通常做相对较少的工作（例如，只需将输入数据复制到缓冲区），并唤醒上半部分代码来完成其余工作。

### 5.4 定时器中断

Xv6使用定时器中断来维持其时钟，并使其能够在受计算量限制的进程（compute-bound processes）之间切换；`usertrap`和`kerneltrap`中的`yield`调用会导致这种切换。定时器中断来自附加到每个RISC-V CPU上的时钟硬件。Xv6对该时钟硬件进行编程，以定期中断每个CPU。

RISC-V要求定时器中断在机器模式而不是管理模式下进行。RISC-V机器模式无需分页即可执行，并且有一组单独的控制寄存器，因此在机器模式下运行普通的xv6内核代码是不实际的。因此，xv6处理定时器中断完全不同于上面列出的陷阱机制。

机器模式下执行的代码位于`main`之前的***start.c***中，它设置了接收定时器中断（***kernel/start.c:57***）。工作的一部分是对CLINT（core-local interruptor）硬件编程，以在特定延迟后生成中断。另一部分是设置一个scratch区域，类似于trapframe，以帮助定时器中断处理程序保存寄存器和CLINT寄存器的地址。最后，`start`将`mtvec`设置为`timervec`，并使能定时器中断。

计时器中断可能发生在用户或内核代码正在执行的任何时候；内核无法在临界区操作期间禁用计时器中断。因此，计时器中断处理程序必须保证不干扰中断的内核代码。基本策略是处理程序要求RISC-V发出“软件中断”并立即返回。RISC-V用普通陷阱机制将软件中断传递给内核，并允许内核禁用它们。处理由定时器中断产生的软件中断的代码可以在`devintr` (***kernel/trap.c:204***)中看到。

机器模式定时器中断向量是`timervec`（***kernel/kernelvec.S:93***）。它在`start`准备的scratch区域中保存一些寄存器，以告诉CLINT何时生成下一个定时器中断，要求RISC-V引发软件中断，恢复寄存器，并且返回。定时器中断处理程序中没有C代码。







 

![image-20240819224626569](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_1.png)

- asychronou，异步的
- cocurrency，并行的
- program devices

![image-20240819224949241](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_2.png)







![image-20240819225133548](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_3.png)



![image-20240819230133646](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_4.png)

使用16550作为UART芯片，通过控制其中的寄存器`001`（Interrupt Enable Register），来产生中断

![image-20240819231228067](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_5.png)





**Console是如何工作的：**

![image-20240819231820774](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_6.png)

`mknod` 是一个用于在文件系统中创建特殊文件的命令。它主要用于创建设备文件或命名管道（FIFO）。console维护的FIFO如下所示：

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_9.png)



RISC-V对interupts有一系列的支持：

- `SIE`寄存器，（Supervisor Interrupt Enable）有一位用于设置`Extern Interrupt`，`Soft Interrupt`，`Timer Interrupt`

  - `SSIE` (Supervisor Software Interrupt Enable)：使能或禁用 Supervisor 模式下的软件中断。

    `STIE` (Supervisor Timer Interrupt Enable)：使能或禁用 Supervisor 模式下的定时器中断。

    `SEIE` (Supervisor External Interrupt Enable)：使能或禁用 Supervisor 模式下的外部中断。

- `SSTATUS`寄存器，（Supervisor Status）有一位用于`Enable/Disable`中断

  - `SIE` (Supervisor Interrupt Enable)：全局使能位。如果设置为 1，表示允许处理中断；如果为 0，则处理中断被禁用。

    `SPIE` (Supervisor Previous Interrupt Enable)：保存前一个状态下的 `SIE` 值，用于在中断处理完成后恢复中断状态。

    `SPP` (Supervisor Previous Privilege)：保存前一个状态下的特权级别。当发生陷阱时，硬件会自动保存当前的特权级别，并切换到 Supervisor 模式。

    `FS` 和 `XS`：这些位用于保存浮点和扩展寄存器的状态。

    `UXL` 和 `SXL`：这些位保存用户模式和管理模式的 XLEN（即 32 位或 64 位的处理器模式）。

- `SIP`寄存器，（Supervisor Interrupt Pending）可以保存的是当前核（hart）在监督模式下等待处理的中断状态。

  - `SSIP` (Supervisor Software Interrupt Pending): 表示软件中断是否挂起。

    `STIP` (Supervisor Timer Interrupt Pending): 表示定时器中断是否挂起。

    `SEIP` (Supervisor External Interrupt Pending): 表示外部中断是否挂起。

- `SCAUSE`寄存器，（Supervisor Cause）保存中断产生的原因（中断号）

- `STVEC`寄存器，（Supervisor Trap Vector Base Address）保存了发生异常或中断时的陷阱处理程序的入口地址。它指向操作系统内核中用于处理异常或中断的代码。

  - 

**M-mode** (Machine Mode) 和 **S-mode** (Supervisor Mode) 是 RISC-V 架构中两种不同的特权级别模式，用于控制处理器的访问权限和操作范围。每种模式允许处理器执行不同类型的指令，并决定其对系统资源的访问能力

**M-mode (Machine Mode)**

- 最高特权级别：M-mode 是 RISC-V 架构中权限最高的模式。它拥有对硬件和所有系统资源的完全访问权限。
- 系统初始化和配置：通常用于系统启动、硬件初始化、系统配置等底层操作。M-mode 是启动处理器时的默认模式。
- 异常和中断处理：所有异常和中断首先都会在 M-mode 下处理。然后根据需要，M-mode 可以将控制权交给其他模式（如 S-mode）处理特定的任务。
- 管理和保护：M-mode 负责设置和管理内存保护、特权模式切换、以及各种控制寄存器的配置。

**S-mode (Supervisor Mode)**

- 中等特权级别：S-mode 是特权级别次高的模式，通常用于操作系统的内核级代码。S-mode 可以访问系统资源，但访问权限受到一定的限制，通常无法直接访问所有硬件。
- 操作系统运行：S-mode 通常运行操作系统的内核或类似的特权软件，负责管理用户应用程序的运行和系统资源的分配。
- 内存管理单元 (MMU)：S-mode 负责控制和管理 MMU，用于实现虚拟内存和内存保护机制。
- 处理异常和中断：S-mode 可以处理特定的异常和中断，尤其是那些与操作系统相关的。例如，系统调用（syscall）通常在 S-mode 下处理。



`UART`的作用是在`console`和`QEMU`中信息的传输，键盘通过`QEMU`将字符发送给`console`，然后`console`负责将字符显示在显示器上。

**中断和并发**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_7.png)



**Producer和Consumer问题**

在producer和consumer之间使用缓冲区可以实现两者的解耦，让两者可以独立运行



![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/5_8.png)



键盘->console，console->显示器



对于高速的外设，中断产生的速度以及超过了CPU的处理速度，这种情况可以使用轮询（Polling）


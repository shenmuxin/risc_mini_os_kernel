## 4. 陷阱指令和系统调用

[toc]

有三种事件会导致中央处理器搁置普通指令的执行，并强制将控制权转移到处理该事件的特殊代码上。一种情况是系统调用，当用户程序执行`ecall`指令要求内核为其做些什么时；另一种情况是异常：（用户或内核）指令做了一些非法的事情，例如除以零或使用无效的虚拟地址；第三种情况是设备中断，一个设备，例如当磁盘硬件完成读或写请求时，向系统表明它需要被关注。

本书使用陷阱（trap）作为这些情况的通用术语。通常，陷阱发生时正在执行的任何代码都需要稍后恢复，并且不需要意识到发生了任何特殊的事情。也就是说，我们经常希望陷阱是透明的；这对于中断尤其重要，中断代码通常难以预料。通常的顺序是陷阱强制将控制权转移到内核；内核保存寄存器和其他状态，以便可以恢复执行；内核执行适当的处理程序代码（例如，系统调用接口或设备驱动程序）；内核恢复保存的状态并从陷阱中返回；原始代码从它停止的地方恢复。

xv6内核处理所有陷阱。这对于系统调用来说是顺理成章的。由于隔离性要求用户进程不直接使用设备，而且只有内核具有设备处理所需的状态，因而对中断也是有意义的。因为xv6通过杀死违规程序来响应用户空间中的所有异常，它也对异常有意义。

Xv6陷阱处理分为四个阶段： RISC-V CPU采取的硬件操作、为内核C代码执行而准备的汇编程序集“向量”、决定如何处理陷阱的C陷阱处理程序以及系统调用或设备驱动程序服务例程。虽然三种陷阱类型之间的共性表明内核可以用一个代码路径处理所有陷阱，但对于三种不同的情况：来自用户空间的陷阱、来自内核空间的陷阱和定时器中断，分别使用单独的程序集向量和C陷阱处理程序更加方便。

### 4.1 RISC-V陷入机制

每个RISC-V CPU都有一组控制寄存器，内核通过向这些寄存器写入内容来告诉CPU如何处理陷阱，内核可以读取这些寄存器来明确已经发生的陷阱。RISC-V文档包含了完整的内容。***riscv.h***(***kernel/riscv.h:1***)包含在xv6中使用到的内容的定义。以下是最重要的一些寄存器概述：

- `stvec`：内核在这里写入其陷阱处理程序的地址；RISC-V跳转到这里处理陷阱。
- `sepc`：当发生陷阱时，RISC-V会在这里保存程序计数器`pc`（因为`pc`会被`stvec`覆盖）。`sret`（从陷阱返回）指令会将`sepc`复制到`pc`。内核可以写入`sepc`来控制`sret`的去向。
- `scause`： RISC-V在这里放置一个描述陷阱原因的数字。
- `sscratch`：内核在这里放置了一个值，这个值在陷阱处理程序一开始就会派上用场。
- `sstatus`：其中的**SIE**位控制设备中断是否启用。如果内核清空**SIE**，RISC-V将推迟设备中断，直到内核重新设置**SIE**。**SPP**位指示陷阱是来自用户模式还是管理模式，并控制`sret`返回的模式。

上述寄存器都用于在管理模式下处理陷阱，在用户模式下不能读取或写入。在机器模式下处理陷阱有一组等效的控制寄存器，xv6仅在计时器中断的特殊情况下使用它们。

多核芯片上的每个CPU都有自己的这些寄存器集，并且在任何给定时间都可能有多个CPU在处理陷阱。

当需要强制执行陷阱时，RISC-V硬件对所有陷阱类型（计时器中断除外）执行以下操作：

1. 如果陷阱是设备中断，并且状态**SIE**位被清空，则不执行以下任何操作。
2. 清除**SIE**以禁用中断。
3. 将`pc`复制到`sepc`。
4. 将当前模式（用户或管理）保存在状态的**SPP**位中。
5. 设置`scause`以反映产生陷阱的原因。
6. 将模式设置为管理模式。
7. 将`stvec`复制到`pc`。
8. 在新的`pc`上开始执行。

请注意，CPU不会切换到内核页表，不会切换到内核栈，也不会保存除`pc`之外的任何寄存器。内核软件必须执行这些任务。CPU在陷阱期间执行尽可能少量工作的一个原因是为软件提供灵活性；例如，一些操作系统在某些情况下不需要页表切换，这可以提高性能。

你可能想知道CPU硬件的陷阱处理顺序是否可以进一步简化。例如，假设CPU不切换程序计数器。那么陷阱可以在仍然运行用户指令的情况下切换到管理模式。但因此这些用户指令可以打破用户/内核的隔离机制，例如通过修改`satp`寄存器来指向允许访问所有物理内存的页表。因此，CPU使用专门的寄存器切换到内核指定的指令地址，即`stvec`，是很重要的。

### 4.2 从用户空间陷入

如果用户程序发出系统调用（`ecall`指令），或者做了一些非法的事情，或者设备中断，那么在用户空间中执行时就可能会产生陷阱。来自用户空间的陷阱的高级路径是`uservec` (***kernel/trampoline.S:16***)，然后是`usertrap` (***kernel/trap.c:37***)；返回时，先是`usertrapret` (***kernel/trap.c:90***)，然后是`userret` (***kernel/trampoline.S:88***)。

来自用户代码的陷阱比来自内核的陷阱更具挑战性，因为`satp`指向不映射内核的用户页表，栈指针可能包含无效甚至恶意的值。

由于RISC-V硬件在陷阱期间不会切换页表，所以用户页表必须包括`uservec`（**stvec**指向的陷阱向量指令）的映射。`uservec`必须切换`satp`以指向内核页表；为了在切换后继续执行指令，`uservec`必须在内核页表中与用户页表中映射相同的地址。

xv6使用包含`uservec`的蹦床页面（trampoline page）来满足这些约束。xv6将蹦床页面映射到内核页表和每个用户页表中相同的虚拟地址。这个虚拟地址是`TRAMPOLINE`（如图2.3和图3.3所示）。蹦床内容在***trampoline.S***中设置，并且（当执行用户代码时）`stvec`设置为`uservec` (***kernel/trampoline.S:16***)。

当`uservec`启动时，所有在本练习中，您将向 xv6 添加一项功能，该功能会在进程使用 CPU 时间时定期向其发出警报。这可能对计算受限的进程很有用，这些进程希望限制它们占用的 CPU 时间，或者对想要计算但也想要采取一些定期操作的进程很有用。更一般地说，您将实现一种原始形式的用户级中断/故障处理程序；例如，您可以使用类似的东西来处理应用程序中的页面错误。如果您的解决方案通过了警报测试和用户测试，则说明它是正确的。32个寄存器都包含被中断代码所拥有的值。但是`uservec`需要能够修改一些寄存器，以便设置`satp`并生成保存寄存器的地址。RISC-V以`sscratch`寄存器的形式提供了帮助。`uservec`开始时的`csrrw`指令交换了`a0`和`sscratch`的内容。现在用户代码的`a0`被保存了；`uservec`有一个寄存器（`a0`）可以使用；`a0`包含内核以前放在`sscratch`中的值。

`uservec`的下一个任务是保存用户寄存器。在进入用户空间之前，内核先前将`sscratch`设置为指向一个每个进程的陷阱帧，该帧（除此之外）具有保存所有用户寄存器的空间(***kernel/proc.h:44***)。因为`satp`仍然指向用户页表，所以`uservec`需要将陷阱帧映射到用户地址空间中。每当创建一个进程时，xv6就为该进程的陷阱帧分配一个页面，并安排它始终映射在用户虚拟地址`TRAPFRAME`，该地址就在`TRAMPOLINE`下面。尽管使用物理地址，该进程的`p->trapframe`仍指向陷阱帧，这样内核就可以通过内核页表使用它。

因此在交换`a0`和`sscratch`之后，`a0`持有指向当前进程陷阱帧的指针。`uservec`现在保存那里的所有用户寄存器，包括从`sscratch`读取的用户的`a0`。

陷阱帧包含指向当前进程内核栈的指针、当前CPU的`hartid`、`usertrap`的地址和内核页表的地址。`uservec`取得这些值，将`satp`切换到内核页表，并调用`usertrap`。

`usertrap`的任务是确定陷阱的原因，处理并返回(***kernel/trap.c:37***)。如上所述，它首先改变`stvec`，这样内核中的陷阱将由`kernelvec`处理。它保存了`sepc`（保存的用户程序计数器），再次保存是因为`usertrap`中可能有一个进程切换，可能导致`sepc`被覆盖。如果陷阱来自系统调用，`syscall`会处理它；如果是设备中断，`devintr`会处理；否则它是一个异常，内核会杀死错误进程。系统调用路径在保存的用户程序计数器`pc`上加4，因为在系统调用的情况下，RISC-V会留下指向`ecall`指令的程序指针（返回后需要执行`ecall`之后的下一条指令）。在退出的过程中，`usertrap`检查进程是已经被杀死还是应该让出CPU（如果这个陷阱是计时器中断）。

返回用户空间的第一步是调用`usertrapret` (***kernel/trap.c:90***)。该函数设置RISC-V控制寄存器，为将来来自用户空间的陷阱做准备。这涉及到将`stvec`更改为指向`uservec`，准备`uservec`所依赖的陷阱帧字段，并将`sepc`设置为之前保存的用户程序计数器。最后，`usertrapret`在用户和内核页表中都映射的蹦床页面上调用`userret`；原因是`userret`中的汇编代码会切换页表。

`usertrapret`对`userret`的调用将指针传递到`a0`中的进程用户页表和`a1`中的`TRAPFRAME` (***kernel/trampoline.S:88***)。`userret`将`satp`切换到进程的用户页表。回想一下，用户页表同时映射蹦床页面和`TRAPFRAME`，但没有从内核映射其他内容。同样，蹦床页面映射在用户和内核页表中的同一个虚拟地址上的事实允许用户在更改`satp`后继续执行。`userret`复制陷阱帧保存的用户`a0`到`sscratch`，为以后与`TRAPFRAME`的交换做准备。从此刻开始，`userret`可以使用的唯一数据是寄存器内容和陷阱帧的内容。下一个`userret`从陷阱帧中恢复保存的用户寄存器，做`a0`与`sscratch`的最后一次交换来恢复用户`a0`并为下一个陷阱保存`TRAPFRAME`，并使用`sret`返回用户空间。

**从users pacce陷入kernel space的过程**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_1.png)



- `stvec`：该寄存器保存了特权模式下的异常和中断处理程序的入口地址
- `sepc`：该寄存器保存发生异常时的程序计数器（即 `PC`），用于记录异常发生时的地址
- `sscracth`：该寄存器主要用于保存临时数据，保存了指向 `trapframe` 结构的地址
- `tp`：该寄存器用于保存线程指针
- `sstatus`：该寄存器管理和反映了系统当前的状态，包括中断使能、全局中断状态、CPU模式
  - `SPP`位用于设置kernel mode
  - `SPIE`位用于允许中断

`ecall`调用的时候会发生几件事情：

- `ecall`将系统模式切换为管理者

- `ecall`将程序计数（program counter）保存在`sepc`中，（下一条程序指令将被写入`trapframe->epc`中）
- `ecall`访问`stvec`中设置的地址，从而访问到`trampoline page`

为什么在陷入处理的时候需要保存寄存器的值？

因为，在陷入处理之后将转入C代码运行，C代码运行会覆盖这些寄存器的值，我们需要保存这些寄存器的值，以便在陷入结束后恢复寄存器的值，以便程序继续正常运行。

- 在系统调用之后，将系统调用的返回值写入`p->trapframe-a0`，然后我们可以将其写入`a0`寄存器方便在上级目录中进行返回
- 在陷入trap时，CPU会将系统调用号存放在寄存器`a7`中

- **Trampoline** 是一段用于切换上下文的代码，负责从一个执行环境跳转到另一个执行环境。在xv6中指的是`trampoline.S`这段assembly代码

- **Trapframe** 是一个数据结构，保存了进程的寄存器状态，以便在上下文切换后恢复执行。在xv6中指的是`struct trapframe`这个数据结构



### 4.3 代码：调用系统调用

第2章以***initcode.S***调用`exec`系统调用（***user/initcode.S:11***）结束。让我们看看用户调用是如何在内核中实现`exec`系统调用的。

用户代码将`exec`需要的参数放在寄存器`a0`和`a1`中，并将系统调用号放在`a7`中。系统调用号与`syscalls`数组中的条目相匹配，`syscalls`数组是一个函数指针表（***kernel/syscall.c:108***）。`ecall`指令陷入(trap)到内核中，执行`uservec`、`usertrap`和`syscall`，和我们之前看到的一样。

`syscall`（***kernel/syscall.c:133***）从陷阱帧（trapframe）中保存的`a7`中检索系统调用号（`p->trapframe->a7`），并用它索引到`syscalls`中，对于第一次系统调用，`a7`中的内容是`SYS_exec`（***kernel/syscall. h:8***），导致了对系统调用接口函数`sys_exec`的调用。

当系统调用接口函数返回时，`syscall`将其返回值记录在`p->trapframe->a0`中。这将导致原始用户空间对`exec()`的调用返回该值，因为RISC-V上的C调用约定将返回值放在`a0`中。系统调用通常返回负数表示错误，返回零或正数表示成功。如果系统调用号无效，`syscall`打印错误并返回-1。

### 4.4 系统调用参数

内核中的系统调用接口需要找到用户代码传递的参数。因为用户代码调用了系统调用封装函数，所以参数最初被放置在RISC-V C调用所约定的地方：寄存器。内核陷阱代码将用户寄存器保存到当前进程的陷阱框架中，内核代码可以在那里找到它们。函数`argint`、`argaddr`和`argfd`从陷阱框架中检索第n个**系统调用参数**并以整数、指针或文件描述符的形式保存。他们都调用`argraw`来检索相应的保存的用户寄存器（***kernel/syscall.c:35***）。

有些系统调用传递指针作为参数，内核必须使用这些指针来读取或写入用户内存。例如：`exec`系统调用传递给内核一个指向用户空间中字符串参数的指针数组。这些指针带来了两个挑战。首先，用户程序可能有缺陷或恶意，可能会传递给内核一个无效的指针，或者一个旨在欺骗内核访问内核内存而不是用户内存的指针。其次，xv6内核页表映射与用户页表映射不同，因此内核不能使用普通指令从用户提供的地址加载或存储。

内核实现了安全地将数据传输到用户提供的地址和从用户提供的地址传输数据的功能。`fetchstr`是一个例子（***kernel/syscall.c:25***）。文件系统调用，如`exec`，使用`fetchstr`从用户空间检索字符串文件名参数。`fetchstr`调用`copyinstr`来完成这项困难的工作。

`copyinstr`（***kernel/vm.c:406***）从用户页表页表中的虚拟地址`srcva`复制`max`字节到`dst`。它使用`walkaddr`（它又调用`walk`）在软件中遍历页表，以确定`srcva`的物理地址`pa0`。由于内核将所有物理RAM地址映射到同一个内核虚拟地址，`copyinstr`可以直接将字符串字节从`pa0`复制到`dst`。`walkaddr`（***kernel/vm.c:95***）检查用户提供的虚拟地址是否为进程用户地址空间的一部分，因此程序不能欺骗内核读取其他内存。一个类似的函数`copyout`，将数据从内核复制到用户提供的地址。

### 4.5 从内核空间陷入

xv6根据执行的是用户代码还是内核代码，对CPU陷阱寄存器的配置有所不同。当在CPU上执行内核时，内核将`stvec`指向`kernelvec`(***kernel/kernelvec.S:10***)的汇编代码。由于xv6已经在内核中，`kernelvec`可以依赖于设置为内核页表的`satp`，以及指向有效内核栈的栈指针。`kernelvec`保存所有寄存器，以便被中断的代码最终可以不受干扰地恢复。

`kernelvec`将寄存器保存在被中断的内核线程的栈上，这是有意义的，因为寄存器值属于该线程。如果陷阱导致切换到不同的线程，那这一点就显得尤为重要——在这种情况下，陷阱将实际返回到新线程的栈上，将被中断线程保存的寄存器安全地保存在其栈上。

`Kernelvec`在保存寄存器后跳转到`kerneltrap`(***kernel/trap.c:134***)。`kerneltrap`为两种类型的陷阱做好了准备：设备中断和异常。它调用`devintr`(***kernel/trap.c:177***)来检查和处理前者。如果陷阱不是设备中断，则必定是一个异常，内核中的异常将是一个致命的错误；内核调用`panic`并停止执行。

如果由于计时器中断而调用了`kerneltrap`，并且一个进程的内核线程正在运行（而不是调度程序线程），`kerneltrap`会调用`yield`，给其他线程一个运行的机会。在某个时刻，其中一个线程会让步，让我们的线程和它的`kerneltrap`再次恢复。第7章解释了`yield`中发生的事情。

当`kerneltrap`的工作完成后，它需要返回到任何被陷阱中断的代码。因为一个`yield`可能已经破坏了保存的`sepc`和在`sstatus`中保存的前一个状态模式，因此`kerneltrap`在启动时保存它们。它现在恢复这些控制寄存器并返回到`kernelvec`(***kernel/kernelvec.S:48***)。`kernelvec`从栈中弹出保存的寄存器并执行`sret`，将`sepc`复制到`pc`并恢复中断的内核代码。

值得思考的是，如果内核陷阱由于计时器中断而调用`yield`，陷阱返回是如何发生的。

当CPU从用户空间进入内核时，xv6将CPU的`stvec`设置为`kernelvec`；您可以在`usertrap`(***kernel/trap.c:29***)中看到这一点。内核执行时有一个时间窗口，但`stvec`设置为`uservec`，在该窗口中禁用设备中断至关重要。幸运的是，RISC-V总是在开始设置陷阱时禁用中断，xv6在设置`stvec`之前不会再次启用中断。

### 4.6 页面错误异常

Xv6对异常的响应相当无趣: 如果用户空间中发生异常，内核将终止故障进程。如果内核中发生异常，则内核会崩溃。真正的操作系统通常以更有趣的方式做出反应。

例如，许多内核使用页面错误来实现**写入时拷贝**`fork`——*copy on write (COW) fork*。要解释*COW fork*，请回忆第3章内容：xv6的`fork`通过调用`uvmcopy`(***kernel/vm.c:309***) 为子级分配物理内存，并将父级的内存复制到其中，使子级具有与父级相同的内存内容。如果父子进程可以共享父级的物理内存，则效率会更高。然而武断地实现这种方法是行不通的，因为它会导致父级和子级通过对共享栈和堆的写入来中断彼此的执行。

由页面错误驱动的*COW fork*可以使父级和子级安全地共享物理内存。当CPU无法将虚拟地址转换为物理地址时，CPU会生成页面错误异常。Risc-v有三种不同的页面错误: 加载页面错误 (当加载指令无法转换其虚拟地址时)，存储页面错误 (当存储指令无法转换其虚拟地址时) 和指令页面错误 (当指令的地址无法转换时)。`scause`寄存器中的值指示页面错误的类型，`stval`寄存器包含无法翻译的地址。

COW fork中的基本计划是让父子最初共享所有物理页面，但将它们映射为只读。因此，当子级或父级执行存储指令时，risc-v CPU引发页面错误异常。为了响应此异常，内核复制了包含错误地址的页面。它在子级的地址空间中映射一个权限为读/写的副本，在父级的地址空间中映射另一个权限为读/写的副本。更新页表后，内核会在导致故障的指令处恢复故障进程的执行。由于内核已经更新了相关的PTE以允许写入，所以错误指令现在将正确执行。

COW策略对`fork`很有效，因为通常子进程会在`fork`之后立即调用`exec`，用新的地址空间替换其地址空间。在这种常见情况下，子级只会触发很少的页面错误，内核可以避免拷贝父进程内存完整的副本。此外，*COW fork*是透明的: 无需对应用程序进行任何修改即可使其受益。

除*COW fork*以外，页表和页面错误的结合还开发出了广泛有趣的可能性。另一个广泛使用的特性叫做惰性分配——*lazy allocation。*它包括两部分内容：首先，当应用程序调用`sbrk`时，内核增加地址空间，但在页表中将新地址标记为无效。其次，对于包含于其中的地址的页面错误，内核分配物理内存并将其映射到页表中。由于应用程序通常要求比他们需要的更多的内存，惰性分配可以称得上一次胜利: 内核仅在应用程序实际使用它时才分配内存。像COW fork一样，内核可以对应用程序透明地实现此功能。

利用页面故障的另一个广泛使用的功能是从磁盘分页。如果应用程序需要比可用物理RAM更多的内存，内核可以换出一些页面: 将它们写入存储设备 (如磁盘)，并将它们的PTE标记为无效。如果应用程序读取或写入被换出的页面，则CPU将触发页面错误。然后内核可以检查故障地址。如果该地址属于磁盘上的页面，则内核分配物理内存页面，将该页面从磁盘读取到该内存，将PTE更新为有效并引用该内存，然后恢复应用程序。为了给页面腾出空间，内核可能需要换出另一个页面。此功能不需要对应用程序进行更改，并且如果应用程序具有引用的地址 (即，它们在任何给定时间仅使用其内存的子集)，则该功能可以很好地工作。

结合分页和页面错误异常的其他功能包括自动扩展栈空间和内存映射文件。



**有关Page faults的要点**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_2.png)

- lazy allocation，懒分配
- mmap（Memory Mapped Files）
- copy-on-write fork
- demand paging

**虚拟内存的好处**

1. isolation 隔离
2. level of indirection: `va` -> `pa`



- 页表故障（page fault）码被存储在`scause`寄存器中
- 发生故障的虚拟页表地址被存放在`stval`寄存器中

**SCAUSE寄存器中断号**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_3.png)

**Allocation---->sbrk()**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_4.png)

**Lazy allocation**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_5.png)



> To allow sbrk() to complete more quickly in these cases, sophisticated kernels allocate user memory lazily. That is, sbrk() doesn't allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it.

为了让 sbrk() 在这些情况下更快地完成，复杂的内核会延迟分配用户内存。也就是说，sbrk() 不会分配物理内存，而只是记住分配了哪些用户地址，并在用户页表中将这些地址标记为无效。当进程首次尝试使用任何给定的延迟分配内存页面时，CPU 会产生页面错误，内核会通过分配物理内存、将其清零并映射来处理该错误

主要包含几个要点：

- 通过判断`p->sz`，这是一个`struct proc`数据结构，其中`sz`字段表示进程内存大小
- allocate 1 page
- zero the page
- map the page to virtual page table
- restart instruction



**按需补零（zero fill on demand）**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_6.png)



“Zero on Demand” 通常指的是一种延迟内存分配策略，其中，内存页面在第一次访问时才真正分配，并且内容被初始化为零。这种策略可以有效地节省物理内存资源，因为只有在程序确实需要使用内存时才会分配页面，并且页面内容是零初始化的，确保了内存的安全性和一致性。

**Copy-on-write fork**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_7.png)



- 每个`pa`都会有一个`ref`引用计数，当`ref`减为0的时候，我们才能够释放这个`pa`

> COW fork() creates just a pagetable for the child, with PTEs for user memory pointing to the parent's physical pages. COW fork() marks all the user PTEs in both parent and child as not writable. When either process tries to write one of these COW pages, the CPU will force a page fault. The kernel page-fault handler detects this case, allocates a page of physical memory for the faulting process, copies the original page into the new page, and modifies the relevant PTE in the faulting process to refer to the new page, this time with the PTE marked writeable. When the page fault handler returns, the user process will be able to write its copy of the page.
>
> COW fork() makes freeing of the physical pages that implement user memory a little trickier. COW fork() makes freeing of the physical pages that implement user memory a little trickier. A given physical page may be referred to by multiple processes' page tables, and should be freed only when the last reference disappears.

COW fork() 仅为子进程创建一个页表，其中用户内存的 PTE 指向父进程的物理页面。COW fork() 将父进程和子进程中的所有用户 PTE 标记为不可写。当任一进程尝试写入其中一个 COW 页面时，CPU 将强制页面错误。内核页面错误处理程序会检测到这种情况，为出错的进程分配一个物理内存页面，将原始页面复制到新页面中，并修改出错进程中的相关 PTE 以引用新页面，此时将 PTE 标记为可写。当页面错误处理程序返回时，用户进程将能够写入其页面副本。

COW fork() 使得释放实现用户内存的物理页面变得有些棘手。COW fork() 使得释放实现用户内存的物理页面变得有些棘手。给定的物理页面可能被多个进程的页表引用，并且应该仅在最后一个引用消失时才释放。

1. 减少内存占用:
   - 当一个进程调用 fork() 创建子进程时,copy-on-write 技术会让父子进程共享同一块内存空间,而不是立即创建子进程的副本。
   - 只有当父进程或子进程对共享的内存页面进行写操作时,操作系统才会复制该页面,为写入者创建一个私有副本。
2. 提高 fork() 系统调用的效率:
   - 由于不需要立即复制整个进程的内存空间,copy-on-write 可以大大加快 fork() 系统调用的速度,减少 fork() 操作的开销。
3. 支持写时复制语义:
   - copy-on-write 机制实现了写时复制的语义,即父子进程共享内存,直到有进程需要修改某个页面时,操作系统才会真正复制该页面（复制该页到一个真实的`pa`，然后将父进程和子进程的`PTE`指向这个`pa`，并修改权限为`RW`）。
   - 这种延迟复制的机制可以提高资源利用率,减少不必要的内存复制开销。



**Demand paging**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_8.png)



1. 按需加载页面:
   - Demand Paging 采用按需加载的方式,只有当进程访问某个页面时,操作系统才会将该页面从磁盘加载到内存中。
   - 这避免了一次性将整个进程的地址空间加载到内存中,节省了内存开销。
2. 虚拟内存管理:
   - Demand Paging 是虚拟内存管理的基础,通过将进程的地址空间划分为页面,并按需加载到内存中,实现了虚拟内存的功能。
   - 这使得进程可以访问比实际物理内存大得多的虚拟地址空间。
3. 页面置换算法:
   - 当内存不足时,Demand Paging 会采用页面置换算法(如 LRU、Clock 等)来决定将哪些页面换出到磁盘,以腾出内存空间。
   - 这种按需加载和页面置换的机制,可以提高内存利用率,支持更大的进程地址空间。
4. 错误处理:
   - 当进程访问一个不在内存中的页面时,会触发缺页中断(page fault)。
   - 操作系统会捕获这个中断,并通过Demand Paging机制将所需页面从磁盘加载到内存中。

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_9.png)

页面置换的时候遵循LRU（least recently used）原则，优先选择：

- `accessed`，已经访问过的页面
- `non-dirty`，非脏页

**Memory-mapped files**

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/4_10.png)

## 
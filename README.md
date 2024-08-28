# Mini kernel based on RISC-V

This project is based on [MIT6.S801](https://pdos.csail.mit.edu/6.S081/2020/index.html), These materials can help you to understand this project:

[Preliminary: basic usage of git](./docs/intro_use_of_git.md)

[Preliminary: basic usage of gdb](./docs/intro_use_of_gdb.md)

[Chapter 1 operating system interfaces](./docs/1_operatring_system_interfaces.md)

[Chapter 2 operating system organization](./docs/2_operating_system_organization.md)

[Chapter 3 page table](./docs/3_page_table.md)

[Chapter 4 traps and system calls](./docs/4_traps_and_system_calls.md)

[Chapter 5 interrupts and device drivers](./docs/5_interrupts_and_device_drivers.md)

[Chapter 6 locks](./docs/6_locking.md)

[Chapter 7 scheduling](./docs/7_scheduling.md)

[Chapter 8 file system](./docs/8_file_system.md)

This project refers to a lot of information, which is not listed here one by one. Thanks to the open source contributors for sharing.

## Installing on Ubuntu

You can refer to [here](https://pdos.csail.mit.edu/6.S081/2020/tools.html) for more details.

```bash
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
```

## Basic Usage

This repository has many branch, each branch is a modification of xv6 to realize corresponding function. You can git clone this repository, then 

```bash
git checkout <to_different_branch>
```

Then run QEMU to use the RSIC-V mini kernel.

```bash
$ make qemu
$ ....
```

Then you will see a console like this:


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

```bash
xv6 kernel is booting

init: starting sh
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2059
cat            2 3 23984
echo           2 4 22808
forktest       2 5 13168
grep           2 6 27344
init           2 7 23912
kill           2 8 22768
ln             2 9 22720
ls             2 10 26208
mkdir          2 11 22872
rm             2 12 22856
sh             2 13 41744
stressfs       2 14 23872
usertests      2 15 152312
grind          2 16 38016
wc             2 17 25112
zombie         2 18 22256
symlinktest    2 19 33192
bigfile        2 20 24496
console        3 21 0
$ echo "hi"
"hi"
$ 
```


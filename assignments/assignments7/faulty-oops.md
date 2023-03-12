Faulty kernel module errors

Command run on qemu
echo “hello_world” > /dev/faulty

Output
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
ESR = 0x96000045
EC = 0x25: DABT (current EL), IL = 32 bits
SET = 0, FnV = 0
EA = 0, S1PTW = 0
FSC = 0x05: level 1 translation fault
Data abort info:
ISV = 0, ISS = 0x00000045
CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=00000000420bd000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) scull(O) faulty(O)
CPU: 0 PID: 149 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2a0
sp : ffffffc008d0bd80
x29: ffffffc008d0bd80 x28: ffffff80020e0cc0 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 000000556bab0a00
x20: 000000556bab0a00 x19: ffffff80020cdb00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008d0bdf0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
faulty_write+0x14/0x20 [faulty]
ksys_write+0x68/0x100
__arm64_sys_write+0x20/0x30
invoke_syscall+0x54/0x130
el0_svc_common.constprop.0+0x44/0x100
 do_el0_svc+0x44/0xb0
 el0_svc+0x28/0x80
 el0t_64_sync_handler+0xa4/0x130
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace c4f023369541831a ]---

Analysis
The Kernel dump tells us about the occurrence of the error.
In the kernel dump above we can see that we get an error "Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000" : This means that the dump was because of dereferencing a null pointer at address 0.
The dump gives Internal error: Oops: 9600005 [#1] SMP: It points out to CPU 0 with PID 149 which crashed.
The dump points to all the intermediate registers of the architecture and what their values were at the time of the crash. The PC points to faulty_write+0x14/0x20 [faulty] which means that the error occoured on faulty_write+0x14 address which is the address which caused the null pointer dereference at address 0. The pgd p4d pud are the page tables.
The call stack shows how each function is executed and what address relative to the function name is instruction being executed, that is the line at which the fault occoured. The flow of a call stack is reverse, the faulty_write is the latest function which is executed. And faulty_write+0x10 instruction causes the crash and kernel dump.
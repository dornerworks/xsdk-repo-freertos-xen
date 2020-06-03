# XSDK FreeRTOS Xen

An XSDK repository plugin that enables FreeRTOS applications
to run as a virtual machine under a Xen hypervisor.
This repository is targeted for XSDK 2017.3.

## Installation

In XSDK, go to `Xilinx -> Repositories -> Local Repositories -> New...`.
In the New Local Repository dialog, enter the path to this repository.

## Usage Example

In this example, we will create a simple hello world FreeRTOS
application, and run it under the Xen hypervisor. Note that
throughout this example, anywhere something such as `[project_root]` or
`[yourproject]` is used, this should be replaced by the information
you have used in your project, and should not include square brackets.

### Create a Project

Create a new FreeRTOS project in XSDK
with `File -> New -> Application Project`.

Make sure the OS platform is set to `freertos910_xilinx`
and the hardware platform is set to the hardware which you are using.

On the next page, select the `FreeRTOS Hello World`
template from `Available Templates -> FreeRTOS Hello World`.

### Configure BSP Settings

First, enable the Xen library
from `Xilinx -> Board Support Package Settings`.

If the FreeRTOS Xen plugin has been added correctly,
`xen` should appear as a library in the supported libraries section.
Simply check the box to enable it.

On the `freertos901_xilinx` tab, change `hypervisor_guest`
to true to enable the application as a hypervisor guest.
Additionally, change stdin and stdout to `psu_uart_1`.
This will cause the application to use UART1,
since UART0 is used by the Xen hypervisor.

You can check that everything has worked so far
by looking in `[project_bsp]/psu_corexa53_0/libsrc` and verifying
that `xen_v1_1` appears there. If the library
does not appear there, you may have to regenerate the board sources.

### Modify Linker Script

Since Xen expects the guest to be loaded at an offset,
we need to modify the linker script to account for this.

In `[project_root]/src/lscript.ld/Source`,
modify the MEMORY section as follows:

```
MEMORY
{
	psu_addr_0_MEM_0 : ORIGIN = 0x40000000, LENGTH = 4M
}
```

### Build the Project

After building the ELF file with XSDK, we need
to convert it to a bin file.

In the terminal window, navigate to the folder
containing the .elf file with `cd [project_root]/Debug`.

Convert the elf to a bin using

```
aarch64-linux-gnu-objcopy -O binary [yourproject].elf [yourproject].bin
```

### Create Guest Config File

To easily run your project, you can create a guest config file
with `touch [yourproject].cfg`. Then add the following content
to the config file:

```
name = "[yourproject]"

kernel = "[yourproject].bin"

memory = 16

vcpis = 1
cpus = [1]

# UART1 passthrough
iomem = [ "0xff010,1" ]
```

### Run the Application

Copy the .bin and .cfg files to the system running the Xen hypervisor.
This can be done with `scp` as follows,
replacing `[hypervisor_ip]` with the IP address of the Xen hypervisor:

```
scp [yourproject].bin root@[hypervisor_ip]:/home/root
scp [yourproject].cfg root@[hypervisor_ip]:/home/root
```

On the Xen hypervisor, in the same directory as both files, run

```
xl create -c [yourproject].cfg
```

and your application should start.

## Troubleshooting

### xc_dom_find_loader: no loader found

You may see an error similar to this:

```
xc: error: panic: xc_dom_core.c:702: xc_dom_find_loader: no loader found: Invalid kernel
libxl: error: libxl_dom.c:638:libxl__build_dom: xc_dom_parse_image failed: No such file or directory
libxl: error: libxl_create.c:1223:domcreate_rebuild_done: cannot (re-)build domain: -3
libxl: error: libxl.c:1575:libxl__destroy_domid: non-existant domain 2
libxl: error: libxl.c:1534:domain_destroy_callback: unable to destroy guest with domid 2
libxl: error: libxl.c:1463:domain_destroy_cb: destruction of domain 2 failed
```

If this happens, it means that XSDK plugin is not installed correctly.
Be sure that the repo is installed and that
the Xen library appears in your BSP.
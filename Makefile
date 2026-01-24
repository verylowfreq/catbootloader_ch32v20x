CC=riscv-none-elf-gcc

TUSB_BASE=tinyusb/

CFLAGS += -mabi=ilp32 -march=rv32imac_zicsr -Os -Wall
CFLAGS += -mcmodel=medany -ffat-lto-objects -flto -fdata-sections -ffunction-sections
CFLAGS += -Wl,--defsym=__FLASH_SIZE=16K -Wl,--defsym=__RAM_SIZE=20K  -Wl,-Map=firmware.elf.map -Wl,--cref -Wl,-gc-sections  -Wl,--print-memory-usage
CFLAGS +=  -lgcc -lm -lnosys -nostartfiles
CFLAGS += --specs=nosys.specs --specs=nano.specs  -Wl,-T,Link.ld
CFLAGS += -I. \
		  -I${TUSB_BASE} \
		  -I${TUSB_BASE}/src/ \
		  -I${TUSB_BASE}/hw/ \
		  -I${TUSB_BASE}/hw/bsp/ch32v20x/boards/ch32v203c_r0_1v0/ \
		  -I${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Core/ \
		  -I${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/inc/
SRCS := main.c
SRCS += ${TUSB_BASE}/hw/bsp/ch32v20x/family.c
SRCS += usb_descriptors.c
SRCS += system_ch32v20x.c
SRCS += ${TUSB_BASE}/hw/bsp/board.c
SRCS += ${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_flash.c \
		${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_bkp.c \
		${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_pwr.c \
		${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_gpio.c \
		${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_rcc.c \
		${TUSB_BASE}/hw/mcu/wch/ch32v20x/EVT/EXAM/SRC/Peripheral/src/ch32v20x_usart.c

STARTUP_SRCS := startup_ch32v20x_D6.S

TUSB_SRCS=tinyusb/src/class/hid/hid_device.c \
		  tinyusb/src/class/vendor/vendor_device.c \
		  tinyusb/src/common/tusb_fifo.c \
		  tinyusb/src/device/usbd.c \
		  tinyusb/src/device/usbd_control.c \
		  tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c \
		  tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
		  tinyusb/src/portable/wch/dcd_ch32_usbfs.c \
		  tinyusb/src/portable/wch/dcd_ch32_usbhs.c \
		  tinyusb/src/tusb.c

SRCS += ${TUSB_SRCS}
OBJS := $(SRCS:.c=.o)
OBJS += $(STARTUP_SRCS:.S=.o)

hidbootloader.bin: hidbootloader.elf
	riscv-none-elf-objcopy -O binary $< $@

hidbootloader.elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	riscv-none-elf-size $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f hidbootloader.elf hidbootloader.bin $(OBJS) 

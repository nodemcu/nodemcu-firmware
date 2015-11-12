Notice: AT added some functions so it's larger than before, if you want to compile it, please compile it as 1024KB or larger flash in compilation STEP 5.

1¡¢compile options

(1) COMPILE
    Possible value: gcc
    Default value: 
    If not set, use xt-xcc by default.

(2) BOOT
    Possible value: none/old/new
      none: no need boot
      old: use boot_v1.1
      new: use boot_v1.2+
    Default value: none

(3) APP
    Possible value: 0/1/2
      0: original mode, generate eagle.app.v6.flash.bin and eagle.app.v6.irom0text.bin
      1: generate user1
      2: generate user2
    Default value: 0

(3) SPI_SPEED
    Possible value: 20/26.7/40/80
    Default value: 40

(4) SPI_MODE
    Possible value: QIO/QOUT/DIO/DOUT
    Default value: QIO

(4) SPI_SIZE
    Possible value: 0/2/3/4/5/6
    Default value: 0

For example:
    make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=0

2¡¢You can also use gen_misc to make and generate specific bin you needed.
    Linux: ./gen_misc.sh
    Windows: gen_misc.bat
    Follow the tips and steps.
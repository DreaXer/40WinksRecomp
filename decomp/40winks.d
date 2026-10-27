build/40winks.elf: \
    build/asm/header.o \
    build/assets/ipl3.o \
    build/asm/1000.o \
    build/asm/1050.o \
    build/asm/data/80C80.data.o \
    build/asm/B7F78.o \
    build/asm/data/B7FA8.data.o \
    build/asm/data/B9220.bss.o \
    build/assets/B9220.o
build/asm/header.o:
build/assets/ipl3.o:
build/asm/1000.o:
build/asm/1050.o:
build/asm/data/80C80.data.o:
build/asm/B7F78.o:
build/asm/data/B7FA8.data.o:
build/asm/data/B9220.bss.o:
build/assets/B9220.o:
-include build/asm/header.d build/assets/ipl3.d build/asm/1000.d build/asm/1050.d build/asm/data/80C80.data.d build/asm/B7F78.d build/asm/data/B7FA8.data.d build/asm/data/B9220.bss.d build/assets/B9220.d

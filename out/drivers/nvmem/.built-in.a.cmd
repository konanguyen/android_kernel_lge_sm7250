cmd_drivers/nvmem/built-in.a :=  rm -f drivers/nvmem/built-in.a; llvm-ar rcSTPD drivers/nvmem/built-in.a drivers/nvmem/core.o drivers/nvmem/qfprom.o drivers/nvmem/qcom-spmi-sdam.o

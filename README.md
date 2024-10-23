# FIT IOT LAB Experiments Set-up
Setup experiments in the real IoT testbed from FIT in France. 

# Initial set-up

## Install ARM compiler

Install the compiler following the instructions in [ARM website]() for your target OS with the option `arm-none-eabi`. 
Since I am runing Ubuntu 22.04, and I want to compile for an M3 device, I choose `arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi.tar.xz`.


After un-packing the ARM compiler folder, **add its location** to the `$PATH` variable (e.g., `export PATH="/path-to/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin:$PATH"`), then proceed.




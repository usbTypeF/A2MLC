# A2MLC
AVX2 Memory Latency Checker

Compile commands

gcc -O3 -march=native -funroll-loops -ftree-vectorize -fomit-frame-pointer -flto -falign-functions=16 -falign-loops=16 -falign-jumps=16 -Wall -Wextra -Wno-unused-result a2mlc.c -o A2MLC.exe

![image](https://github.com/user-attachments/assets/d673b143-022c-46a5-98ad-bac9d60a9fc4)

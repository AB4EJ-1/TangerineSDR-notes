gcc -c -fPIC mylib.c -o mylib.o
gcc -shared mylib.o -o mylib.so

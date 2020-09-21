""" Python wrapper for the C shared library mylib"""
import sys, platform
import ctypes, ctypes.util


from ctypes import *

# Find the library and load it
    
print("try a loadlibrary")

#cdll.LoadLibrary("./mylib.so")
libc = CDLL("./mylib.so")
print("libc=",libc)

print("library load done")
test_empty = libc.test_empty

print("Try test_empty:")
libc.test_empty()

test_add = libc.test_add
test_add.argtypes = [ctypes.c_float, ctypes.c_float]
test_add.restype = ctypes.c_float

test_passing_array = libc.test_passing_array
test_passing_array.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int]
test_passing_array.restype = None

test_passing_struct = libc.test_passing_struct
m = ctypes.create_string_buffer(212)
#m = ctypes.create_string_buffer(b"EFGHI")
#test_passing_struct.argtypes = [ctypes.POINTER(ctypes.c_char_p)]

test_passing_struct.argtypes = [ctypes.POINTER(type(m))]
test_passing_struct.restype = None

print("\nTry test_add:")
print(libc.test_add(34.55, 23))

# Create a 25 elements array
numel = 25
data = (ctypes.c_int * numel)(*[x for x in range(numel)])
print("before call, data =",data)
# Pass the above array and the array length to C:
print("\nTry passing an array of 25 integers to C:")
libc.test_passing_array(data, numel)

print("data from Python after returning from C:")
for indx in range(numel):
    print(data[indx], end=" ")

#m = ctypes.create_string_buffer(212)
print("try to get buffer")
libc.test_passing_struct(m);
print("m=")
print(m.value)
print(sizeof(m), repr(m.raw))


print("")

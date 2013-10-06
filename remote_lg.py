import wiringpi2
import os
import struct

I = "<"

def codeToStruct(freq, numpairs, bitcompression, times, pairs):
    return struct.pack(I+"HBBBB", freq, numpairs, bitcompression, len(times)*2, len(pairs)) + "".join([struct.pack(I+"HH", *t) for t in times])+ "".join([struct.pack("B", p) for p in pairs])

def getCode():
    times = [  (895, 446), (61,  52), (61,  163), (61,  4079), (895,  224), (61,  52)]
    #  [0, 1, 1,1,2,1,1,1,1,2,1,2,1,2,2,2,2,2,1,1,1,2,1,1,1,1,2,2,2,1,2,2,2,3,4, 3]
    pairs = [0x04, 0x94, 0x49, 0x28, 0xA2, 0x92, 0x48, 0x92, 0x89, 0x25, 0x24, 0x52, 0x4E, 0x30]
    st = codeToStruct(38462, 36, 3, times, pairs)
    return struct.pack(I+"H",len(st)) + st

def do():
    fd = wiringpi2.wiringPiSPISetup(0,500000)
    os.write(fd, getCode())
    os.close(fd)

wiringpi2.wiringPiSetup()


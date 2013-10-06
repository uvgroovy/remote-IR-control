import wiringpi2
import os
import struct
import math
I = "<"

def codeToStruct(freq, numpairs, bitcompression, times, pairs):
    return struct.pack(I+"HBBBB", freq, numpairs, bitcompression, len(times)*2, len(pairs)) + "".join([struct.pack(I+"HH", *t) for t in times])+ "".join([struct.pack("B", p) for p in pairs])

def toCompresedPairCodes(l,compression):
    s = ""
    for x in l:
        s += ("{0:0" + str(compression)+ "b}").format(x)
    if len(s)%8 != 0:
        s += "0"*(8 - (len(s)%8))
    numBytes = len(s)/8
    res = []
    for i in range(numBytes):
        num = s[i*8:(i+1)*8]
        num = int(num,2)
        res += [num]
    return res

def getCode(freq, times, pairs):
    compression = int(math.ceil(math.log(len(times))/math.log(2)))
    compressedPairs = toCompresedPairCodes(pairs, compression)   
    st = codeToStruct(freq, len(pairs), compression, times, compressedPairs)
    return struct.pack(I+"H",len(st)) + st

sharp_off = (38000,[  (895, 446), (61,  52), (61,  163), (61,  4079), (895,  224), (61,  52)],[0, 1, 1,1,2,1,1,1,1,2,1,2,1,2,2,2,2,2,1,1,1,2,1,1,1,1,2,2,2,1,2,2,2,3,4, 3])

test = (38462,[  (10000, 0)],[0]*20)


def sendCode(c):
    fd = wiringpi2.wiringPiSPISetup(0,500000)
    os.write(fd, getCode(*c))
    os.close(fd)

def do():
    sendCode(sharp_off)

wiringpi2.wiringPiSetup()


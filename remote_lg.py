import os
import struct
import math
import sys
import json

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
sharp_down = (38000, [(58, 4079), (891, 224), (58, 52), (58, 0), (902, 446), (58, 165), (58, 9585), (891, 229)], [4, 2, 2, 2, 5, 2, 2, 2, 2, 5, 2, 5, 2, 5, 5, 5, 5, 5, 2, 5, 5, 2, 2, 2, 2, 2, 5, 2, 2, 5, 5, 5, 5, 0, 7, 6, 1, 3])
sharp_up = (38000, [(58, 52), (18, 2), (58, 22496), (58, 9578), (58, 163), (2, 2), (58, 4077), (2, 142), (895, 224), (895, 446), (9, 3201)], [5, 9, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 4, 0, 4, 4, 4, 4, 0, 4, 4, 4, 0, 0, 0, 0, 4, 0, 0, 0, 4, 4, 4, 4, 6, 8, 3, 5, 8, 2, 1, 5, 10, 7, 5])
sharp_fan_slow = (38000, [(891, 224), (58, 9582), (58, 52), (58, 0), (58, 163), (58, 4077), (897, 446), (897, 224)], [6, 2, 2, 2, 4, 2, 2, 2, 2, 4, 2, 4, 2, 4, 4, 4, 4, 2, 2, 4, 2, 2, 2, 2, 2, 4, 4, 2, 4, 4, 4, 4, 4, 5, 7, 1, 0, 3])
sharp_fan_fast = (38000, [(58, 4079), (58, 52), (58, 0), (58, 165), (895, 446), (895, 224)], [4, 1, 1, 1, 3, 1, 1, 1, 1, 3, 1, 3, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 0, 5, 2])
sharp_cool = (38000, [(58, 52), (58, 0), (893, 226), (58, 4081), (58, 167), (58, 9587), (900, 448)], [6, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 4, 0, 4, 4, 4, 4, 4, 0, 0, 4, 0, 0, 0, 0, 0, 4, 4, 0, 4, 4, 4, 4, 3, 2, 5, 2, 1])
sharp_fan = (38000, [(58, 9589), (58, 0), (58, 54), (58, 165), (58, 4081), (888, 224), (895, 224), (900, 446)], [7, 2, 2, 2, 3, 2, 2, 2, 2, 3, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 6, 0, 5, 0, 5, 1])
sharp_energy_save = (38000, [(58, 52), (58, 9591), (58, 0), (893, 224), (58, 165), (893, 448), (58, 4081), (888, 224)], [5, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 4, 0, 4, 4, 4, 4, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 4, 4, 4, 4, 4, 4, 6, 3, 1, 7, 2])
sharp_auto_cool = (38000, [(56, 4081), (895, 448), (56, 165), (895, 224), (56, 54), (56, 0), (56, 9589)], [1, 4, 4, 4, 2, 4, 4, 4, 4, 2, 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 0, 3, 6, 3, 5])
sharp_timer = (38000, [(58, 52), (58, 9591), (58, 0), (58, 165), (58, 4081), (895, 224), (900, 446)], [6, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 3, 0, 3, 3, 3, 3, 0, 3, 3, 0, 0, 0, 0, 0, 3, 0, 0, 3, 3, 3, 3, 3, 4, 5, 1, 5, 2])


lg_g2_gtv_toggle_on = (38000, [(58, 0), (58, 54), (58, 3836), (58, 167), (900, 448), (900, 224)], [4, 1, 1, 3, 1, 1, 1, 1, 1, 3, 3, 1, 3, 3, 3, 3, 3, 1, 1, 1, 3, 1, 1, 1, 1, 3, 3, 3, 1, 3, 3, 3, 3, 2, 5, 0])


test = (38462,[  (10000, 0)],[0]*20)


def sendCode(c):
    import wiringpi2
    fd = wiringpi2.wiringPiSPISetup(0,500000)
    os.write(fd, getCode(*c))
    os.close(fd)

def sync():
    import wiringpi2
    fd = wiringpi2.wiringPiSPISetup(0,500000)
    os.write(fd, "\0"*257)
    os.close(fd)

def do():
    sendCode(sharp_off)

def firecode():
    l = sys.stdin.readline()
    print "got code",l
    sendCode(json.loads(l))

def main():
    import argparse 
    parser = argparse.ArgumentParser(description='')
    
    # Do not use the default paramter in the params that are also read from the config file!!
    subparsers = parser.add_subparsers()
    
    command_subparser = subparsers.add_parser('firecode', help='Fire an IR code. this is done by writing to the SPI interface. hopfully someone will pickit up from there')
    command_subparser.set_defaults(func=firecode)

    command_subparser = subparsers.add_parser('sync', help='Sync the microncontroller.')
    command_subparser.set_defaults(func=sync)

    args = parser.parse_args()
    args.func()

if __name__ == "__main__":
    main()

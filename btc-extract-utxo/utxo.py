import time
import struct
import binascii
import sys
import hashlib
import webbrowser
import urllib
import json
import math
import os
from bitcoin_tools.analysis.status.data_dump import utxo_dump
from bitcoin_tools.analysis.status.utils import parse_ldb

def printMostUpdateUTXOfile():
    f_utxos = "utxos"
    parse_ldb(f_utxos)
    fileStat = os.stat("data/utxos")
    return fileStat.st_size/68

# After parsing chainstate into utxo, this piece of code convert utxo into different datablock for ORAM
def utxoIntoORAMblocks(utxo_limit, oram_block_bit):
    print "[+] Start converting utxo into ORAM blocks"
    print "[>] Number of ORAM blocks: ", 2**oram_block_bit
    print "[>] Each wallet(pkh) can have up to:", utxo_limit, " utxos"

    remap_dic={}
    unique_pubkeyhash_set = dict()
    # set()
    counter = 0
    start = time.time()
    utxos = []
    uniquePKH=0
    with open("data/utxos", "rb") as f:
        byte = f.read(68)
        while byte != "":
            hash_pubkeyhash = hashlib.sha256(byte[44:64]).hexdigest() 
            # check if exist
            if hash_pubkeyhash not in unique_pubkeyhash_set:
                unique_pubkeyhash_set[hash_pubkeyhash] = 0
                uniquePKH = uniquePKH + 1
            # verify that each public-key-hash has exactly certain number unspent output. 
            if unique_pubkeyhash_set[hash_pubkeyhash] > utxo_limit: 
                byte = f.read(68)
                continue
            unique_pubkeyhash_set[hash_pubkeyhash] +=1
            trunc_hash = hash_pubkeyhash[:8]                                      # take hose bytes
            blockID = int(trunc_hash,16)%pow(2,oram_block_bit)                    # convert to integer from base 16
            try:
                #if(len(remap_dic[blockID]) > utxo_in_each_bin):
                #    byte = f.read(68)
                #    continue
                remap_dic[blockID].append(byte)
            except:
                remap_dic[blockID] = [byte]
            counter += 1
            byte = f.read(68)
            if counter%1000000 == 0:
                end = time.time()
                print counter/1000000, end - start
                start = time.time()
                print len(remap_dic)
    map_len = {}
    for k,v in remap_dic.items():
        try:
            map_len[len(v)] += 1
        except:
            map_len[len(v)] = 1

    # print statistic of utxo after partionning into blocks
    outfile = open("map_len2",'wb')
    outfile.write(json.dumps(map_len))
    outfile = open("data/biggerUTXO", 'wb')
    utxo_in_each_bin = int(math.ceil(2.7*uniquePKH*utxo_limit/(2**oram_block_bit)))
    utxo_in_each_bin = utxo_in_each_bin + (8-utxo_in_each_bin%8)
    print "[>] utxos per oram block: ", utxo_in_each_bin
    for k,v in remap_dic.items():
        # truncate to exactly UTXO_IN_EACH_BIN utxos per bin
        v = v[:min(utxo_in_each_bin, len(v))]
        len_to_append = 0
        if len(v) < utxo_in_each_bin:
            len_to_append = utxo_in_each_bin - len(v)
        appended_bytes = b'\0' * len_to_append * 68
        for utxo in v:
            outfile.write(utxo)
        outfile.write(appended_bytes)
    print "[+] Done"
    print uniquePKH


if __name__ == '__main__':
    UTXO_LIMIT = 2
    ORAM_BLOCK_BIT =21  # number of oram block will be 2^ORAM_BLOCK_BIT
    printMostUpdateUTXOfile()
    utxoIntoORAMblocks(UTXO_LIMIT,ORAM_BLOCK_BIT)

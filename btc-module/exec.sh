#/bin/sh

#N = Number of data blocks
#N=16777216 # 2^24
#N=8388608  # 2^23
#N=4194304  # 2^22
#N=2097152  # 2^21
N=1048576  # 2^20
#N=131072
#no_of_req
no_of_req=100

#stash_size
#Note refer to PathORAM and CircuitORAM papers to understand stash size bounds.
#It is typically sufficient to use 150 for PathORAM and 10 for CircuitORAM
stash_size=100 #stash size should adjust as well

#block_size multiple of 8 because of padding and multiple of 68. mcm(68,8)=136
#block_size=32000
#block_size=1088
#block_size=2176
#block_size=3264
#block_size=6528
block_size=544
#new/resume
#New/Resume flag, Previously ZT had a State Store/Resume mechanism which is currently broken. So hence always use new till this is fixed
new="new"

#memory/hdd, ZT supports any underlying backend storage mechanism. Currently support for HDD is broken. But it's trivially fixable by tailoring parameters in LocalStorage.cpp if you wish to do that.
backend=memory

#oblivious_flag, ZeroTrace is a Doubly-oblivious ORAM i.e. the ORAM controller logic is itself oblivious to provide side-channel security against an adversary that observer the memory trace of this controller. Setting this to 0 improves performance, at the cost of introducing side-channel vulnerabilities.
oblivious_flag=1

#recursion_data_size can be used to tailor the data size of the recursive ORAM trees, since currently ZT uses ids of 4 bytes, recursion size of 64, gives us a compression factor of 16 with each level of recursion.
recursion_data_size=64

#ZT supports circuit/path oram options as the type of underlying ORAM. Remember to adjust stash sizes according to the choice of ORAM_type
#oram_type="circuit"
oram_type="path"

#Z is the number of blocks in a bucket of the ORAMTree, typically PathORAM uses Z=4. But Z can be adjusted to make trade-offs on the security VS performance bar. Read more about this in the original Circuit ORAM/ Path ORAM papers. 
Z=4

#ZT supports a bulk_read_interface (designed for an application we are working on). Setting bulk_request_size to 0, will perform each request in the no_of_req individually, setting any larger value will cause the untrusted components of ZT to chunk the requests into bulk chunks of that size, the enclave will then perform all the requests in the chunk before it returns the response back to the untrusted part of the program.
bulk_request_size=0

#Bitcoin block number
btc_block=400000 

#Write or read
write=0

#Number of threads (read)
threads=1

exec_command="btc_Untrusted/Main "$N" "$no_of_req" "$stash_size" "$block_size" "$new" "$backend" "$oblivious_flag" "$recursion_data_size" "$oram_type" "$Z" "$bulk_request_size" "$btc_block" "$write" "$threads
echo $exec_command

$exec_command


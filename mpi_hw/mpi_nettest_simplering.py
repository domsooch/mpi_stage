#!/usr/bin/env python
import os, math, struct, random
from os import urandom
from sys import getsizeof
from optparse import OptionParser
import json
from mpi4py import MPI
import numpy as np
import time
import matplotlib.pyplot as plt
import cStringIO

def parse_args():
    parser = OptionParser()
    parser.add_option("-v", "--verbose",
                      dest="verbose",
                      metavar="<verbose>",
                      default=None,
                      help="Heavy Logging")
    parser.add_option("-o", "--outpath",
                      dest="outpath",
                      default='test_results.json',
                      metavar="<outpath>",
                      help="write test results to the file <filename>")
    parser.add_option("-c", "--command",
                      dest="command",
                      metavar="<command>",
                      default="M_D_C",
                      help="M=memory_test D=disk_test C=circular_mpi_test")
    parser.add_option("-b", "--block_size",
                      dest="block_size",
                      default=10,
                      metavar="<block_size>",
                      help="Circular_MPI_Test BlockSize")
    parser.add_option("-i", "--iterations",
                      dest="iterations",
                      metavar="<iterations>",
                      default = 8,
                      help="M, D, C : iterations")
    return parser.parse_args()

##Topolgy
EXCLUDE_MASTER=False
def right_rank(my_rank, comm_sz, offset, ExcludeMaster=False):#To
    if ExcludeMaster:
        return (my_rank-1 + offset)%(comm_sz-1)+1
    else:
        return (my_rank + offset)%comm_sz
    
def left_rank(my_rank, comm_sz, offset, ExcludeMaster=False):#FROM
    if ExcludeMaster:
        return (my_rank-1 -offset + comm_sz)%(comm_sz-1)+1
    else:
        return (my_rank - offset + comm_sz)%comm_sz

def CalculatePath_ring(my_rank, offset=10):
    #assert offset ==1 #This is not set up to be changed in this module
    left = left_rank(my_rank, comm_sz, offset)
    right = right_rank(my_rank, comm_sz, offset)
    return left, right



def Allgather_ring(comm, arraysize, blocksize, offset=1):
    """llgather_ring: IS TAKEN FROM pacheco's ParallelProgramming with MPI ch 13.1.2
    transcoded from C"""
    comm_sz = comm.Get_size()
    my_rank = comm.Get_rank()
    
    sendrecv_arr = np.zeros(arraysize, dtype=np.float64)
    recv_arr= np.empty(arraysize, dtype=np.float64)
    
    loop_number = 0
    
    #Mark Array with my_rank
    for i in range(blocksize):
        pos = i + my_rank*blocksize
        sendrecv_arr[pos] = float(my_rank)+float(i)/1000.0
    left, right = CalculatePath_ring(my_rank, offset=offset)
    if ctx.verbose: print 'passing from ', left, right
    bytes_sent = 0
    start_t = time.time()
    for i in range(comm_sz-1):
        send_array_offset = ((my_rank - i + comm_sz)%comm_sz)*blocksize
        recv_array_offset = ((my_rank - i -1 + comm_sz)%comm_sz)*blocksize
        
        send_arr = sendrecv_arr[send_array_offset:send_array_offset+blocksize]
        
        send_request = comm.Isend([send_arr, MPI.DOUBLE], right, tag = loop_number)
        recv_request = comm.Irecv([recv_arr, MPI.DOUBLE], source= left, tag = loop_number)
        
        send_request.wait()
        recv_request.wait()
        sendrecv_arr.put(range(recv_array_offset, recv_array_offset+blocksize, 1), recv_arr)
        bytes_sent += len(recv_arr)*getsizeof(0.0)
        
#     sendrecvLst = []
#     for i in range(comm_sz-1):
#         send_array_offset = ((my_rank - i + comm_sz)%comm_sz)*blocksize
#         recv_array_offset = ((my_rank - i -1 + comm_sz)%comm_sz)*blocksize
#         sendrecvLst.append([send_array_offset, recv_array_offset])
#     #print "saved %f secs"%(time.time()-s)
#     bytes_sent = 0
#     start_t = time.time()
#     for i in range(comm_sz-1):
#         send_array_offset, recv_array_offset = sendrecvLst[i]
#         send_arr = sendrecv_arr[send_array_offset:send_array_offset+blocksize]
#         #print 'p:%i start_send: to %i'%(my_rank, right)
#         comm.Send([send_arr, MPI.DOUBLE], right, tag = loop_number)
#         #print 'p:%i start_recv: from %i'%(my_rank, left)
#         comm.Recv([recv_arr, MPI.DOUBLE], source= left, tag = loop_number)
#         sendrecv_arr.put(range(recv_array_offset, recv_array_offset+blocksize, 1), recv_arr)
#         bytes_sent += len(recv_arr)*getsizeof(0.0)
    elapsed_time = time.time() - start_t
    #Compute correct score
    return sendrecv_arr, elapsed_time, bytes_sent/elapsed_time
    
    
def computeStats(inLst):
    avg = float(sum(inLst))/len(inLst)
    maxd = 0.0;max_i = None
    ds = 0.0
    for i in range(len(inLst)):
        v = inLst[i]
        d = math.pow(v-avg, 2.0)
        ds += d
        if d > maxd:
            maxd = d
            max_i = i
    stdev = math.sqrt(ds)
    outlier_i = max_i
    outlier_diff = math.sqrt(maxd)
    #print 'computeStats(inLst)', inLst, avg, stdev, outlier_i, outlier_diff
    return avg, stdev, outlier_i, outlier_diff
        
class cpu_info:
    def __init__(self):
        inb = open('/proc/cpuinfo', 'r').read().split('\n')
        self.d = {}
        for line in inb:
            try:
                k, v = line.split(':')[:2]
            except:
                #print 'Bad Line: ', line
                continue
            self.d[k.strip()] = v.strip()
        self.num_procs = int(self['processor']) + 1
        print "Proc: ", self['model name'], "num_procs: ", self.num_procs, " cache_sz: ", self.cache_sz(), "\n\n\n"
    def __getitem__(self, k):
        return self.d[k]
    def cache_sz(self):
        c = self['cache size']
        if 'K' in c:
            return int(c.split(' ')[0])*1024

class DataPusher:
    #http://effbot.org/zone/python-with-statement.htm
    def __init__(self, IO_Obj, name = 'test', bytes_per_step=16):
        self.name = name
        #https://docs.python.org/2/library/stringio.html
        self.IO_type = str(type(IO_Obj)).replace("<type '", '').replace("'>", '')
        print 'DataPusher writes to: %s type: %s initial bytes_per_step: %i'%(IO_Obj, self.IO_type, bytes_per_step)
        self.bytes_per_step = bytes_per_step
        self.out_fd = IO_Obj
        self.total_bytes = 0
        self.start_time = 0
        self.elapsed_time = -1
        self.final_elapsed_time = -1
        self.cycles = 0
    def __enter__(self):
        print 'Enter DataPusher'
        self.start_time = time.time()
        return self
    def rand_gen(self, total_bytes, out_fd):
        recs_per_step = max(1, int(total_bytes/(getsizeof(0.0))))
        #https://docs.python.org/2/library/struct.html
        out_fd.write(struct.pack('f'*recs_per_step, *[random.random() for x in range(recs_per_step)]))
    def rand_popen(self, num_bytes, out_fd):
        #Too slow!!
        ##http://stackoverflow.com/questions/18421757/live-output-from-subprocess-command
        cmd_list = ['time','dd','if=/dev/urandom', 'bs=%i'%num_bytes ,'count=1']
        a = subprocess.Popen(cmd_list, stdout=out_fd)
        a.wait()
    def run(self, bytes_per_step = None):
        self.cycles +=1
        if bytes_per_step == None:
            bytes_per_step = self.bytes_per_step
        s = time.time()
        self.rand_gen(bytes_per_step, self.out_fd)
        self.elapsed_time = time.time() - s
        self.total_bytes += bytes_per_step
        print self.name, self.cycles, bytes_per_step, self.elapsed_time
        self.bytes_per_step *=2
    def __exit__(self, typ, value, traceback):
        end_time = time.time()
        self.final_elapsed_time = end_time - self.start_time
        print '%s: Wrote %i bytes, in %i cycles in %f sec'%(self.name, self.total_bytes, self.cycles, self.final_elapsed_time)
        return True
    def results(self):
        return {'name':self.name, 
                'total_bytes':self.total_bytes, 
                'bytes_per_step': self.bytes_per_step,
                'cycles':self.cycles, 
                'elapsed_time': self.elapsed_time}


if __name__ == '__main__':
    ctx, _ = parse_args()
    
    #runme like so: mpiexec -n 5 python mpi_nettest_simplering.py 
    
    comm = MPI.COMM_WORLD
    comm_sz = comm.Get_size()
    my_rank = comm.Get_rank()
    results = {}

    if 'C' in ctx.command:
        test_name = 'Circular_Allgather'
        results[test_name] =  {'details':'', 'column_headers':[], 'data':[]}
        results[test_name]['column_headers'] = ['iteration', 'array_size(elements)', 
                                         'array_size(bytes)', 'elapsed_time_avg', 'data_rate_avg','data_rate_stdev', 'outlier', 'outlier_diff']
        arg_block_size = int(ctx.block_size)
        block_size = max(10, arg_block_size)
        
        for i in range(int(ctx.iterations)):
            if arg_block_size == 0:#if input blocksize is -1 then double size on each run
                block_size *=2
            block_count = comm_sz
            array_size = block_size*block_count
            array_sz_b = array_size*getsizeof(0.0)
            
            arr, elapsed_time, data_rate = Allgather_ring(comm, array_size, block_size, offset=1)
            correct_score = 0.0
            for r in range(comm_sz):
                if arr[r*block_size]== r: 
                    s = 1.0/float(comm_sz)
                else: s = 0.0
                correct_score += s
            send_arr = [elapsed_time, data_rate, correct_score]
            if my_rank > 0:
                comm.isend([send_arr, MPI.DOUBLE], 0, tag = 0)#lower case uses slower pickle
                #see: http://www.bu.edu/pasi/files/2011/01/Lisandro-Dalcin-mpi4py.pdf
            if my_rank == 0:
                print "\nIteration: %i"%i
                timeLst = []
                data_rateLst = []#send_arr] #master's array comes first
                for p in range(comm_sz):
                    if p > 0:
                        r_arr = comm.recv(source= p, tag = 0)#pickle version (lowercase recv)
                        elapsed_time = r_arr[0][0]
                        data_rate = r_arr[0][1]
                        correct_score = r_arr[0][2]
                    timeLst.append(elapsed_time)
                    data_rateLst.append(data_rate)
                    print 'master says that for array p %i pos %i value is %f array_sz: %i time_sec: %f, Bytes/sec: %f correct_score %f'%(p, 
                                                                            p*block_size, 
                                                                            arr[p*block_size],
                                                                            array_sz_b,
                                                                            elapsed_time, 
                                                                            data_rate, 
                                                                            correct_score)
                elapsed_time_avg, stdev, max_i, maxd = computeStats(timeLst)
                data_rate_avg, stdev, outlier, outlier_diff = computeStats(data_rateLst)
                results[test_name]['data'].append([i, array_size, array_sz_b, 
                                                      elapsed_time_avg, data_rate_avg, stdev, outlier, outlier_diff])
    comm.barrier()
    
    if my_rank==0 and 'M' in ctx.command:
        #Memory write
        test_name = 'Memory_Test'
        results[test_name] =  {'details':'', 'column_headers':[], 'data':[]}
        cpu = cpu_info()
        results['Memory_Test']['cpu_data'] = cpu.d
        out_fp = cStringIO.StringIO()
        #assuming float16 as basic unit how many doublings do we need to break cache
        num_iterations = math.log(cpu.cache_sz()*(cpu.num_procs), 2.0)-4 + 1
        num_iterations = int(round(num_iterations, 0))
        results[test_name]['details'] = 'assuming float16 as basic unit how many doublings do we need to break cache %f : %i'%(cpu.cache_sz(), num_iterations)
        

        with DataPusher(out_fp, name= test_name, bytes_per_step=16) as f:
            for i in range(num_iterations):
                f.run(bytes_per_step = None)
                results[test_name]['data'].append(f.results())

    if my_rank==0 and 'D' in ctx.command:
        test_name = 'Disk_Test'
        results[test_name] =  {'details':'', 'column_headers':[], 'data':[]}
        out_fp = open('diskwrite_fn','w')
        max_load_sz = 160000000
        num_iterations = math.log(max_load_sz, 2.0)-4 + 1
        num_iterations = int(round(num_iterations, 0))
        results[test_name]['details'] = 'assuming float16 as basic unit how many doublings to reach max_load_sz: %i'%num_iterations

        t = DataPusher(out_fp, name= test_name, bytes_per_step=16)
        t.run()
        
        with DataPusher(out_fp, name= test_name, bytes_per_step=16) as f:
            for i in range(num_iterations):
                f.run(bytes_per_step = None)
                results[test_name]['data'].append(f.results())
        out_fp.close()
    #OutputResults
    ofp = open(ctx.outpath, 'w')
    ofp.write(json.dumps(results))    
    
    
    
    
    
    




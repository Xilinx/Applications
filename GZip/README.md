# GZip 

Gzip is a file format used for file compression and decompression. 

## Overview

Gzip is based on the DEFLATE algorithm. The complete GZip application has two parts:

1. LZ77 compression 
2. Huffman encoding

In this example GZip implementation, the LZ77 and Static Huffman modules are accelerated on the FPGA
device available on the AWS F1 instance. The accelerated GZip application provides a throughput of 1.6GB/s. This GZip example achieves around 1.7x compression ratio overall (Silesia Benchmark).

Note: This implementation of GZip doesn't include CRC32 (Cyclic Redundancy Check) logic, due to this while using standard gzip decoder with "-d" option results in crc related warning and decompression fails. To overcome this issue please use standard GZip decoder with following option always.


```
	gzip -dc compressedfile.gz > outputfilename
	
	Note: Execution of this command generates "crc" warning, this warning can be ignored.
```



### Resource Utilization

| Design (Bytes/Clock)  | LUT | LUTMEM | REG | BRAM | DSP | Fmax (MHz) |
| --------------- | --- | ------ | --- | ---- | --- | -----|
| 8               | 45183(5.07%) | 1325(0.24%) |46176(2.33%) | 299(18.51%) |0|250


### Performance Benchmark

|Compression Algorithm | Target Hardware | Throughput (MB/s) | Notes |
|----------------------|-----------------|------------|-------|
|GZip (file to file)   |C5.4xlarge       |68.8    |Usage: gzip -1|
|Pigz (file to file)   |C5.4xlarge (16-threads)|641|Usage: pigz -p 16 --fast|
|GZip (buffer to buffer)|F1.2xlarge|1500|  - |


### Results

Table below presents the best throughput achieved during execution of this application.
In batch mode this application performs overlapped execution between host and device.
In single file mode kernel (device) throughput is presented.


| Topic | Result |
|-------|--------|
|Best End to End Throughput| 1.54GB/s |
|Best Kernel Throughput | 1.92GB/s |
|Average Compression Ratio | 1.7x |



## Algorithm Description

In this GZip example, LZ77 and Static Huffman compression algorithms are accelerated using FPGA device.
The GZip kernel is implemented as a C/C++ kernel using SDAccel flow. 

The following picture presents overview of accelerated GZip application. The encoded stream of this application
is compatible with standard GZip application. 

![gzip overview select](./img/sdx_gzip.png)  <br />

The following picture shows various internal dataflow stages (pipeline stages)
into which the kernel implementation has been split:  
  
 
![algorithm flow select](./img/gzip_kernel_optimization.png)  <br />


  
Following are some of the key architectural decisions taken to achieve the
necessary performance:

1.  The whole GZip pipeline has been designed to process 8bytes/cycle. To
achieve this, we read 8byte in parallel from DDR memory, and then perform
multiple comparison in the dictionary to find the best match. Every cycle, we
compare multiple sub-strings in an 8-byte length to find the best match.
2.  To perform multiple comparisons every cycle, several copies of the
dictionary data is maintained. The dictionary is also updated every cycle to
avoid conditional writes.
3.  The last stage of GZip compression performs Static Huffman encoding which is a bit-level packing of the
data. This step by definition has a throughput of 1Byte per cycle. Hence, to
match the throughput of earlier stages of 8Bytes/cycle, the incoming data stream
is split into 8 parallel stream of 1KB each and sent to separate pipelines. In
the end, streams from multiple pipes are written to DDR.  


 ![Byte Pack select](./img/static_huffman_fix.png) <br />
  
## Software & Hardware

```
  Software: Xilinx SDx 2017.1
  Hardware: xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0 (AWS VU9p F1 DSA)
```
 
## Execution Steps

This example is provided with two popular compression benchmarks. 

1. [Silesia](http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia)
2. [Canterbury](http://corpus.canterbury.ac.nz/descriptions/#cantrbry)

```
  Input Arguments: 
    
        1. To execute batch of files: -l <files.list> -b <size of batch> 
        2. To execute single file: -i <file>
        
   Note: Default arguments are set in Makefile
```

### Emulation flows
```
  make check TARGETS=<sw_emu/hw_emu> DEVICES=$AWS_PLATFORM
  
  Note: This command compiles for targeted emulation mode and executes the
        application. To execute it on AWS F1 instance, please follow instructions
        specific to AWS F1 emulation.

```
### Hardware

```
  make all TARGETS=hw DEVICES=$AWS_PLATFORM

  Note: This command compiles for hardware execution. It generates kernel binary ".xclbin" file. 
        This file is placed in ./xclbin directory under GZip folder. To execute it on AWS F1 instance, 
        please follow instructions specific to AWS F1 deployment process.

```

### Usage 

This section presents steps to use this application effectively. 


```
------------------------------------------------------------------------------------------------------------

Batch Mode Command: ./xil_gzip -l silesia.list -b 0
                  
Note: If batch size is 0, all the files are attempted to process in batch mode.
      Throughput (MB/s) presented in batch mode execution corresponds to end to end execution
      (Host and Device).
      
Output: Generates .gz format files for each file present in <filenames>.list
 	Decodes them using standard GZip application using "gzip -dc *.gz > output"
	Validates them by comparing decoded output with input file
      
------------------------------------------------------------------------------------------------------------

Single File Mode Command: ./xil_gzip -i input_file 

Note: In single file mode host to device overlapped execution is not possible.
      Throughput (KT(MBps)) presented in single file mode corresponds to kernel (Device) execution only.

Output: Generates .gz format output file
      
-------------------------------------------------------------------------------------------------------------

```




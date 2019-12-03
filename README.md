# sgx-mpi

This example shows the integration of MPI and SGX to achieve high performance distributed
computing in a secure and privacy-preserving environment. It is a prototype and proof of
concept work of a more generic secure bigdata analytics framework. Currently we use the
word count example to showcase the usability of the system for data-parallel applications.

## Build and run the parallel word count example

To build, simply run

  make clean; make

Then use mpirun to run the example

  mpirun -hostfile mpihosts -n 8 -N 1 ~/sgx-mpi/app TESTFILE.txt

This will run the example on 8 SGX-enabled compute nodes specified in the mpihosts file.

We assumed you have a preconfigured cluster with SGX-enabled compute nodes.

## Use as a template for other similar data-parallel applications

This prototype/example is a proof of concept work towards a more generalized framework.
Currently it serves as a template if you want to run your similar dtaa-parallel applications
using this framework. The steps are the following:

### Define OCall and ECall interface in the Enclave.edl file
In the 'trusted' section, define the interface that you will call from the unsecured part (App)
to the secured part (Enclave). This is the core processing function that deals with the splitted
datasets.
In the 'untrusted' section define the reduce funtion that will reduce the partial results from
each worker process.

### Implement the core proceesing function in Enclave.cpp
This is the implementation of the function defined in the 'trusted' part of the interface. This
part will be executed in SGX enclaves.

### Work on data loading, mapping, results presenting in App.cpp
Use the App.cpp example as a template, to do data loading, splitting, and the framework will
distribute the workload into multiple (number to be specified during running time with mpirun)
nodes/enclaves. Also implement the reduce_partial_results function in which to summarize the
overall results.

## Further generalization of the framework

We are expanding this proof of concept work to a more generalized framework working with more
general real world applications from field such as bioinfromatics. During this process we will
have more abstracted framework structure and more streamlined way for developers and users to
use the framework.

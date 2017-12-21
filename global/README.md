# Scheduling, binding and register allocation on global register architecture

The program has to be compiled in Linux.
It is possible to compile and execute it in Visual Studio by adding <#include "stdafx.h"> among the libraries and then copying the FDS.cpp and FuReg.cpp inside the VS project folder.

In the folder "src" is present the main file. Before compiling and executing, it is necessary to execute these commands
```
mkdir bin
mkdir time
mkdir sch
mkdir res
```

In order to compile, the following command has to be executed (the makefile is also present)
```
g++ -std=c++11 -o bin/fds FDS.cpp
g++ -std=c++11 -o bin/furegglobal FuRegGlobal.cpp
```

To execute the program the following command has to be executed
```
./bin/fds [dfgname.txt] [folder destination] [latency constraint] [max depth propagation of update ASAP-ALAP fds]
./bin/furegglobal [dfgname.txt] [dfgname_result.txt]
```

The dfg have to be present in the same folder of the executable, otherwise it is necessary to give the relative path of the dfg. Same for the dfg results.
The script to automate the execution are present in the folder.

The file can be executed both in linux and in windows.

In the folder src is present the main file

In order to compile, the following command has to be executed (the makefile is also present)
	g++ -std=c++11 -o bin/main src/main.cpp

To execute the program the following command has to be executed
	./main [dfgname.txt] [folder destination/] [latency constraint] [max depth propagation of update ASAP-ALAP fds]

The dfg have to be present in the same folder of the executable, otherwise it is necessary to give the relative path of the dfg.
If the execution is done using the scripts provided the readme has to be removed from the folder otherwise there will be an error.

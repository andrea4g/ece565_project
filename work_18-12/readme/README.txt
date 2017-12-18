The file can be executed both in linux and in windows.

LINUX
In the folder src is present the main file

In order to compile, the following command has to be executed (the makefile is also present)
	g++ -std=c++11 -o bin/main src/main.cpp

To execute the program the following command has to be executed
	./main [dfgname.txt] [folder destination/] [latency constraint] [max depth propagation of update ASAP-ALAP fds]

The dfg have to be present in the same folder of the executable, otherwise it is necessary to give the relative path of the dfg.

WINDOWS VISUAL STUDIO
It is present the folder containing the solution on Visual Studio.
In order to execute correctly it is necessary:
	1) Project Properties -> Configuration Properties -> C/C++ -> Preprocessor -> Preprocessor Definitions
	and insert "_CRT_SECURE_NO_DEPRECATE"
	2) Project Properties -> Configuration Properties -> Debugging -> Command Arguments
	and insert the commands as explained above
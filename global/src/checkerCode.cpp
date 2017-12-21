//The original code is created by Huan Ren. Last modified by Owen and Keivn.
//"lookahead" approach was not discussed in the class but is used in this code, for forces calculation. please refer to the FDS paper section IV: E
#include "stdafx.h"
#include <queue>
#include <vector>
#include <algorithm> 

using namespace std;
const int tnum = 9;	//number of operation types: note, single-speed only
int DFG = 0; //input DFG from 1 to 14, 0 is an example DFG
int LC = 0; //latency constraint, provided by input file

struct FU
{
	int type; //FU-type
	int id; //FU-id
	vector<FU *> fanin;
	vector<FU *> fanout;
	int taval; //available time

	int TavalPort1;
	int TavalPort2;

	vector<int> port1; //store reg_id
	vector<int> port2; //store reg_id
	vector<int> port1Ext; //store the operation ID with constant external input (assume all constant input on the different address, not in reg) for port1
	vector<int> port2Ext; //store the operation ID with constant external input (assume all constant input on the different address, not in reg) for port2
};
struct G_Node    //save the info for operation node
{
	int id;
	int type;
	list<G_Node *> child;   // get input from the current node
	list<G_Node *> parent;  // send output to the current node
	FU *myFU;

	int regID;
	int startTime; //scheduled time
	int endTime; //start + delay - 1
	int liveStartTime; // live start time for reg allocation
	int liveEndTime; //live end time for Reg Allocation

	int givenDataSTime;
	int givenDataETime;

	bool constant;
};
struct resourcetype	//store the delay info
{
	int delay;	//operation delay
};

struct Reg
{
	int id;
	int Taval;
	vector<G_Node *> operations;
	vector<G_Node *> extInput; //store connected external Input
	vector<FU *> outputFU; //store connected fanin-FU
	int demux; 
};

struct BindResult
{
	vector<vector<vector<int> > > bind; //store binding operation
	vector<vector<vector<Reg *> > > regID; //store connected Regs
	vector<vector<vector<int>>> port1; //store connected Reg to port 1
	vector<vector<vector<int>>> port2; //store connected Reg to port 2
};

struct muxResult
{
	int demux21_16bit; // = Sum of all Reg's demux size - 1: Reg's demux = 0,1 --> size = 0; otherwise, size = # of demux inputs - 1;
	int mux21_16bit; // = Sum of all port's mux size - 1: port's # of inputs = 0,1 --> size = 0; otherwise, size = port size - 1;

	int maxDemux;
	int maxMux;
};

bool Comp(G_Node *i, G_Node *j) { return (i->startTime < j->startTime); }
bool CompTlive(G_Node *i, G_Node *j) { return (i->liveStartTime < j->liveStartTime); }
//remove repeat FU
bool Comp2(FU *i, FU *j) { return (i->id < j->id); }
bool Comp3(FU *i, FU *j) { return (i->id == j->id); }
int opn = 0; //# of operations
G_Node* ops;		//operations list
resourcetype* rt = new resourcetype[tnum];	//stucture of operation types, which holds delay and distribution graph of each operation type
BindResult Bresult; //binding result
muxResult Mresult; //mux/demux result
vector<Reg *> reglist; //reg allocation result
//----------------INPUT FILES ------------------//
void readInput(int DFG, char** argv); //read input files: DFG info (node with predecessors and successors); LC; scheduling results
void GET_LIB(); //read FU delay info 
void Read_DFG(const int DFG, char** argv); //read DFG filename
void readGraphInfo(char **argv, int DFG, int *edge_num); //read DFG info based on the filename
void get_Time(int DFG, char** argv); // read the filename of LC and scheduling results
void read_Time(int DFG, char** argv); //read LC and schedulinng results


void get_DataTimeRegInfo(int DFG, char** argv);
void read_DataTimeRegInfo(int DFG, char** argv);

void main(int argc, char **argv) {
	for (DFG = 1; DFG <= 11; DFG++) //do for each DFG from 1 to 11
	{   // Read DFG information: nodes, data-dependency (predecessors and successors of a node), LC, scheduling time info, execution time for each node
		readInput(DFG, argv);
		
		//PART 1: CHECK THE SCHEDULING RESULT
		//checking the scheduling
		cout << "DFG : " << DFG << endl;
		int error = 0;
		for (int n = 0; n < opn; n++)
		{
			if (ops[n].startTime < 1) //check each node's scheduling time < 1
			{
				error++;
				cout << "========ERROR=========" << endl;
				cout << "The operation " << ops[n].id << " has a 0 (INVALID) scheduling time" << endl;
			}

			if (ops[n].startTime + rt[ops[n].type].delay - 1 > LC) //check each node's end time compared to LC
			{
				error++;
				cout << "========ERROR=========" << endl;
				cout << "The operation " << ops[n].id << " has a INVALID scheduling time over latency constraint" << endl;
			}

			if (ops[n].parent.size() != 0)
			{			
				for (auto p = ops[n].parent.begin(); p != ops[n].parent.end(); ++p) //check each parent for the data dependency{
					if ((*p)->startTime + rt[(*p)->type].delay - 1 >= ops[n].startTime) //compare the parent's scheduling time + delay 
					{
						error++;
						cout << "A non-input operation" << endl;
						cout << "========ERROR=========" << endl;
						cout << "The operation " << ops[n].id << " has a DATA Dependency error (INVALID) with the parent operation " << (*p)->id << endl;
					}
			}		
		}
		if (error > 0)
		{
			cout << "===============ERROR EXIST=========================" << endl;
			cout << " Total # of errors of Data/Latency constraints = " << error << endl;
			
		}
		else
		{
			cout << "===============NO ERROR=========================" << endl;
			cout << "=======SCHEDULING RESULT CHECK DONE=============" << endl;
		}




		//Check Data-live-time result
		//1: My data must hold until all my children are done.
		//2: If not output operation: My data start <= end time
		//3: If output operation: My data start can = LC, end = 0 (default value, don't care)

		//Show data time Info from given bind-allocation-input file
		//for (auto n = 0; n < opn; n++)
			//cout << "my id: " << ops[n].id << " ,my data start time: " << ops[n].givenDataSTime << " ,my data end time: " << ops[n].givenDataETime << " ,my REG-ID: " << ops[n].regID << endl;

		// compute data live time from scheduling
		for (auto n = 0; n < opn; n++)
		{
			ops[n].liveStartTime = ops[n].startTime + rt[ops[n].type].delay; //compute from scheduling time (input only!), do not use end time directly since end time is not the input!
			//initialize end time
			ops[n].liveEndTime = 0;
		}
		for (auto n = 0; n < opn; n++)
		{
			//For non-output node: find live end time = MAX of all my children's live-start-time-1 (same as operation end time)
			if (ops[n].child.size() > 0)
			{
				for (auto it = ops[n].child.begin(); it != ops[n].child.end(); it++)
				{
					if (ops[n].liveEndTime < (*it)->liveStartTime - 1)
						ops[n].liveEndTime = (*it)->liveStartTime - 1;
				}
				continue;
			}
			else //output node, set T live end = LC + 2
			{
				ops[n].liveEndTime = LC + 2;
			}
			//for output node, end time don't care.
		}
		//-------------------------------------------------------------------------------------//
		//PART 2: CHECK THE DATA LIVE TIME
		//Start check data live time:

		int invalidDataTime = 0;
		for (auto n = 0; n < opn; n++)
		{
			if (ops[n].child.size() > 0) //non-output node!
			{//1: check the result from allocation result, and the compted result from scheduling result

				if (ops[n].liveStartTime != ops[n].givenDataSTime)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has DIFFERENT Live Start Time!!" << endl;
				}
				if (ops[n].liveEndTime != ops[n].givenDataETime)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has DIFFERENT Live End Time!!" << endl;
				}

			//2: S-time > E-time
				if (ops[n].givenDataETime < ops[n].givenDataSTime)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Allocation Result Live Start Time > End Time !!  INVALID!!" << endl;
				}
				if (ops[n].liveEndTime < ops[n].liveStartTime)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Scheduling Result Live Start Time > End Time !!  INVALID!!" << endl;
				}

			//3: check the data time result compared to LC, for non-output node, both S-time/E-time should <=LC

				if (ops[n].givenDataSTime > LC)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Allocation Result Live Start Time > LC !!  INVALID!!" << endl;
				}
				if (ops[n].givenDataETime > LC)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Allocation Result Live End Time > LC !!  INVALID!!" << endl;
				}

				if (ops[n].liveStartTime > LC)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Scheduling Result Live Start Time > LC !!  INVALID!!" << endl;
				}
				if (ops[n].liveEndTime > LC)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has INPUT from Scheduling Result Live End Time > LC !!  INVALID!!" << endl;
				}
			} //check non-output node

			if (ops[n].child.size() == 0) //output-node
			{
				//1: check the result from allocation result, and the compted result from scheduling result

				if (ops[n].liveStartTime != ops[n].givenDataSTime)
				{
					invalidDataTime++;
					cout << "ERROR: Operation " << ops[n].id << " has DIFFERENT Live Start Time!!" << endl;
				}
				//2: The Data Start time cannot beyond LC+1, latest start time is LC+1

				if (ops[n].liveStartTime > LC + 1)
				{
					invalidDataTime++;
					cout << "ERROR: Output Operation " << ops[n].id << " has INPUT from Scheduling Result Live Start Time > LC + 1 !!  INVALID!!" << endl;
				}
				if (ops[n].givenDataSTime > LC + 1)
				{
					invalidDataTime++;
					cout << "ERROR: Output Operation " << ops[n].id << " has INPUT from Allocation Result Live Start Time > LC + 1 !!  INVALID!!" << endl;
				}
			}
		}

		if (invalidDataTime == 0)
		{
			cout << "===============NO ERROR=========================" << endl;
			cout << "=======DATA LIVE TIME RESULT CHECK DONE=========" << endl;
		}
		else
		{
			cout << "===============ERROR EXIST=========================" << endl;
			cout << " Total # of errors of Data live time = " << invalidDataTime << endl;
		}
		cout << "**********************************************" << endl;



		//--------------------------------------------------------------------//
		//PART 3: CHECK THE REG ALLOCATION CONFILICTION!

		vector <vector<int> > RegInfo; //store the data (operation ID) info of each reg.

		RegInfo.clear();

		//# of reg = max-REG ID+1; (start from ID=  0, size = 1, etc...)
		
		int Regsize = 0;
		for (auto n = 0; n < opn; n++)
		{
			if (Regsize < ops[n].regID + 1)
				Regsize = ops[n].regID + 1;
		}
		//initialize RegInfo structure
		RegInfo.resize(Regsize, vector <int>());
		for (auto i = 0; i < Regsize; i++)
			RegInfo[i].clear();

		for (auto n = 0; n < opn; n++)
			RegInfo[ops[n].regID].push_back(ops[n].id); //store ops id into its reg.


		int regErrorScheduling = 0;

		for (auto regID = 0; regID < Regsize; regID++) //for each reg.
		{
			for (auto j = 0; j < RegInfo[regID].size(); ++j)
			{
				for (auto k = j + 1; k < RegInfo[regID].size(); ++k)
				{//either j's endT >=k's STime, or k's endT >= j's STime
						if ((ops[j].liveEndTime >= ops[k].liveStartTime) || (ops[k].liveEndTime >= ops[j].liveStartTime))
						{
							continue;
						}
						else
						{
							regErrorScheduling++;
							cout << "!!!!!ERROR: Conflict happen on Reg ID: " << regID << " for nodes: " << ops[j].id << " and " << ops[k].id << endl;
						}
						continue;
				}
			}
		}

		if (regErrorScheduling == 0)
		{
			cout << "======NO ERROR based on Scheduling Result=======" << endl;
			cout << "=======REG ALLOCATION RESULT CHECK DONE=========" << endl;
		}
		else
		{
			cout << "===============ERROR EXIST=========================" << endl;
			cout << " Total # of errors of Reg Allocation from scheduling result = " << invalidDataTime << endl;
		}
		cout << "**********************************************" << endl;
		//check reg alloc from given alloc result.

		int regErrorAlloc = 0;

		for (auto regID = 0; regID < Regsize; regID++) //for each reg.
		{
			for (auto j = 0; j < RegInfo[regID].size(); ++j)
			{
				for (auto k = j + 1; k < RegInfo[regID].size(); ++k)
				{//either j's endT >=k's STime, or k's endT >= j's STime
					if ((ops[j].givenDataETime >= ops[k].givenDataSTime) || (ops[k].givenDataETime >= ops[j].givenDataSTime))
					{
						continue;
					}
					else
					{
						regErrorAlloc++;
						cout << "!!!!!ERROR: Conflict happen on Reg ID: " << regID << " for nodes: " << ops[j].id << " and " << ops[k].id << endl;
					}
					continue;
				}
			}
		}

		if (regErrorScheduling == 0)
		{
			cout << "======NO ERROR based on REG ALLOC Result=======" << endl;
			cout << "=======REG ALLOCATION RESULT CHECK DONE=========" << endl;
		}
		else
		{
			cout << "===============ERROR EXIST=========================" << endl;
			cout << " Total # of errors of Reg Allocation from reg allocation result = " << invalidDataTime << endl;
		}
		cout << "**********************************************" << endl;

			

	}
	std::cout << "Press ENTER to terminate the program." << endl;
	std::cin.get();	//press Enter to terminate the program
}

//****************************************************************************//
//-------------------------------INPUT FILES---------------------------------//
void readInput(int DFG, char** argv)
{
	int edge_num = 0;
	Read_DFG(DFG, argv);
	edge_num = 0;
	readGraphInfo(argv, DFG, &edge_num);
	//Read Lib (delay info)
	GET_LIB(); // this part is hard code, can change this part based on the input file
	//Read latency constraint and node scheduling time info  
	get_Time(DFG, argv);
	read_Time(DFG, argv);

	get_DataTimeRegInfo(DFG, argv);
	read_DataTimeRegInfo(DFG, argv);

	//obtain the end-time for each operation: T-end = T-start + delay - 1
	for (int i = 0; i < opn; i++)
		ops[i].endTime = ops[i].startTime + rt[ops[i].type].delay - 1;
}
void GET_LIB()
{
	rt[0].delay = 1;
	rt[1].delay = 1;
	rt[2].delay = 3;
	rt[3].delay = 1;
	rt[4].delay = 1;
	rt[5].delay = 1;
	rt[6].delay = 1;
	rt[7].delay = 1;
	rt[8].delay = 8;
}
void Read_DFG(int DFG, char** argv)
{
	if (DFG == 0)
		argv[1] = "example.txt";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		argv[1] = "hal.txt";
	else if (DFG == 2)
		argv[1] = "horner_bezier_surf_dfg__12.txt";
	else if (DFG == 3)
		argv[1] = "arf.txt";
	else if (DFG == 4)
		argv[1] = "motion_vectors_dfg__7.txt";
	else if (DFG == 5)
		argv[1] = "ewf.txt";
	else if (DFG == 6)
		argv[1] = "feedback_points_dfg__7.txt";
	else if (DFG == 7)
		argv[1] = "write_bmp_header_dfg__7.txt";
	else if (DFG == 8)
		argv[1] = "interpolate_aux_dfg__12.txt";
	else if (DFG == 9)
		argv[1] = "matmul_dfg__3.txt";
	else if (DFG == 10)
		argv[1] = "smooth_color_z_triangle_dfg__31.txt";
	else if (DFG == 11)
		argv[1] = "invert_matrix_general_dfg__3.txt";
	/*
	if (DFG == 0)
		argv[1] = "example.txt";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		argv[1] = "hal.txt";
	else if (DFG == 2)
		argv[1] = "horner_bezier_surf_dfg__12.txt";
	else if (DFG == 3)
		argv[1] = "arf.txt";
	else if (DFG == 4)
		argv[1] = "motion_vectors_dfg__7.txt";
	else if (DFG == 5)
		argv[1] = "ewf.txt";
	else if (DFG == 6)
		argv[1] = "h2v2_smooth_downsample_dfg__6.txt";
	else if (DFG == 7)
		argv[1] = "feedback_points_dfg__7.txt";
	else if (DFG == 8)
		argv[1] = "collapse_pyr_dfg__113.txt";
	else if (DFG == 9)
		argv[1] = "write_bmp_header_dfg__7.txt";
	else if (DFG == 10)
		argv[1] = "interpolate_aux_dfg__12.txt";
	else if (DFG == 11)
		argv[1] = "matmul_dfg__3.txt";
	else if (DFG == 12)
		argv[1] = "idctcol_dfg__3.txt";
	else if (DFG == 13)
		argv[1] = "jpeg_fdct_islow_dfg__6.txt";
	else if (DFG == 14)
		argv[1] = "smooth_color_z_triangle_dfg__31.txt";
	else if (DFG == 15)
		argv[1] = "invert_matrix_general_dfg__3.txt";

	else if (DFG == 16)
		argv[1] = "300_0.txt";
	else if (DFG == 17)
		argv[1] = "1300_0.txt";
		*/
}
void readGraphInfo(char **argv, int DFG, int *edge_num)
{
	FILE *bench;		//the input DFG file
	if (!(bench = fopen(argv[1], "r")))	//open the input DFG file to count the number of operation nodes in the input DFG
	{
		std::cerr << "Error: Reading input DFG file " << argv[1] << " failed." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	char *line = new char[100];
	opn = 0;			//initialize the number of operation node
	if (DFG != 16 && DFG != 17)	//read input DFG file in format .txt
	{
		while (fgets(line, 100, bench))		//count the number of operation nodes in the input DFG
			if (strstr(line, "label") != NULL)	//if the line contains the keyword "label", the equation returns true, otherwise it returns false. search for c/c++ function "strstr" for detail
				opn++;
	}
	else //read input DFG file from format .gdl
	{
		while (fgets(line, 100, bench))
			if (strstr(line, "node") != NULL)
				opn++;
	}
	fclose(bench);	//close the input DFG file
	ops = new G_Node[opn];
	//close the input DFG file
	//based on the number of operation node in the DFG, dynamically set the size
	std::map<string, int> oplist;
	string name, cname;
	char *tok, *label;	//label: the pointer point to "label" or "->" in each line
	char seps[] = " \t\b\n:";		//used with strtok() to extract DFG info from the input DFG file
	int node_id = 0;	//count the number of edges in the input DFG file
	if (!(bench = fopen(argv[1], "r")))	//open the input DFG file agian to read the DFG, so that the cursor returns back to the beginning of the file
	{
		std::cerr << "Error: Failed to load the input DFG file." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	if (DFG != 16 && DFG != 17)
		while (fgets(line, 100, bench))	//read a line from the DFG file, store it into "line[100]
		{
			if ((label = strstr(line, "label")) != NULL)	//if a keyword "label" is incurred, that means a operation node is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain the node name
				oplist.insert(make_pair(name, node_id));	//match the name of the node to its number flag
				tok = strtok(label + 7, seps);	//obtain the name of the operation type
				if (strcmp(tok, "ADD") == 0)	  ops[node_id].type = 0;			//match the operation type to the nod. search for c/c++ function "strcmp" for detail
				else if (strcmp(tok, "AND") == 0) ops[node_id].type = 1;
				else if (strcmp(tok, "MUL") == 0) ops[node_id].type = 2;
				else if (strcmp(tok, "ASR") == 0) ops[node_id].type = 3;
				else if (strcmp(tok, "LSR") == 0) ops[node_id].type = 4;
				else if (strcmp(tok, "LOD") == 0) ops[node_id].type = 5;
				else if (strcmp(tok, "STR") == 0) ops[node_id].type = 6;
				else if (strcmp(tok, "SUB") == 0) ops[node_id].type = 7;
				else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 8;
				ops[node_id].id = node_id;
				node_id++;
			}
			else if ((label = strstr(line, "->")) != NULL)	//if a keyword "->" is incurred, that means an edge is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain node name u from edge (u, v)
				cname.assign(strtok(label + 3, seps));	////obtain node name v from edge (u, v)
				(ops[oplist[name]].child).push_back(&(ops[oplist[cname]]));	//use double linked list to hold the children
				(ops[oplist[cname]].parent).push_back(&(ops[oplist[name]]));//use double linked list to hold the parents
				(*edge_num)++;
			}
		}
	else
	{
		int child, parent;
		while (fgets(line, 100, bench))
		{
			if (strstr(line, "node"))
			{
				fgets(line, 100, bench);
				tok = strtok(line, seps);
				tok = strtok(NULL, seps);
				if (atoi(tok) == 1)
					ops[node_id].type = 0;	//since there are only 4 operation types in .gdl file, we change type 2 to division and keep the other types unchanged.
				else
					ops[node_id].type = atoi(tok);
				ops[node_id].id = node_id;
				node_id++;
			}
			else if (strstr(line, "edge"))
			{
				tok = strtok(line, seps);
				for (int i = 0; i < 4; i++)
					tok = strtok(NULL, seps);
				child = atoi(tok);
				fgets(line, 100, bench);
				tok = strtok(line, seps);
				for (int i = 0; i < 4; i++)
					tok = strtok(NULL, seps);
				parent = atoi(tok);
				ops[child].child.push_back(ops + parent);
				ops[parent].parent.push_back(ops + child);
				(*edge_num)++;
			}
		}
	}//end of reading DFG
	fclose(bench);
	delete[] line;
}

void get_Time(int DFG, char** argv)
{
	if (DFG == 0)
		argv[2] = "example_result.txt";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		argv[2] = "hal_result.txt";
	else if (DFG == 2)
		argv[2] = "horner_bezier_surf_dfg__12_result.txt";
	else if (DFG == 3)
		argv[2] = "arf_result.txt";
	else if (DFG == 4)
		argv[2] = "motion_vectors_dfg__7_result.txt";
	else if (DFG == 5)
		argv[2] = "ewf_result.txt";
	else if (DFG == 6)
		argv[2] = "feedback_points_dfg__7_result.txt";
	else if (DFG == 7)
		argv[2] = "write_bmp_header_dfg__7_result.txt";
	else if (DFG == 8)
		argv[2] = "interpolate_aux_dfg__12_result.txt";
	else if (DFG == 9)
		argv[2] = "matmul_dfg__3_result.txt";
	else if (DFG == 10)
		argv[2] = "smooth_color_z_triangle_dfg__31_result.txt";
	else if (DFG == 11)
		argv[2] = "invert_matrix_general_dfg__3_result.txt";
}
void read_Time(int DFG, char** argv)
{
	FILE *bench;		//the input DFG file
	if (!(bench = fopen(argv[2], "r")))	//open the input DFG file to count the number of operation nodes in the input DFG
	{
		std::cerr << "Error: Reading input DFG file " << argv[2] << " failed." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	char *line = new char[100];
	char *tok, *label;
	char spes[] = " ,.-/n/t/b";
	int my_id = 0;
	int node_num = 0;
	while (fgets(line, 100, bench))
	{
		if ((label = strstr(line, "LC")) != NULL) //read LC info, first line
		{
			tok = strtok(line, spes);
			tok = strtok(NULL, spes);
			LC = atoi(tok);
		}
		else //read t-scheduled info
		{
			if (node_num < opn) //exact opn lines (1 node per line)
			{
				node_num++;
				tok = strtok(line, spes);
				my_id = atoi(tok);
				tok = strtok(NULL, spes);
				ops[my_id].startTime = atoi(tok);
			}
		}
	}
	fclose(bench);
	delete[] line;
}
//-------------------------------INPUT FILES---------------------------------//

void get_DataTimeRegInfo(int DFG, char** argv)
{
	if (DFG == 0)
		argv[3] = "example_result.txt";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		argv[3] = "hal_bind_alloc_result.txt";
	else if (DFG == 2)
		argv[3] = "horner_bezier_surf_dfg__12_bind_alloc_result.txt";
	else if (DFG == 3)
		argv[3] = "arf_bind_alloc_result.txt";
	else if (DFG == 4)
		argv[3] = "motion_vectors_dfg__7_bind_alloc_result.txt";
	else if (DFG == 5)
		argv[3] = "ewf_bind_alloc_result.txt";
	else if (DFG == 6)
		argv[3] = "feedback_points_dfg__7_bind_alloc_result.txt";
	else if (DFG == 7)
		argv[3] = "write_bmp_header_dfg__7_bind_alloc_result.txt";
	else if (DFG == 8)
		argv[3] = "interpolate_aux_dfg__12_bind_alloc_result.txt";
	else if (DFG == 9)
		argv[3] = "matmul_dfg__3_bind_alloc_result.txt";
	else if (DFG == 10)
		argv[3] = "smooth_color_z_triangle_dfg__31_bind_alloc_result.txt";
	else if (DFG == 11)
		argv[3] = "invert_matrix_general_dfg__3_bind_alloc_result.txt";
}
void read_DataTimeRegInfo(int DFG, char** argv)
{
	FILE *bench;		//the input DFG file
	if (!(bench = fopen(argv[3], "r")))	//open the input DFG file to count the number of operation nodes in the input DFG
	{
		std::cerr << "Error: Reading input DFG file " << argv[2] << " failed." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	char *line = new char[100];
	char *tok, *label;
	char spes[] = " ,.-/n/t/b";
	int my_id = 0;
	int node_num = 0;
	while (fgets(line, 100, bench))
	{
		//read t-scheduled info

			if (node_num < opn) //exact opn lines (1 node per line)
			{
				node_num++;
				tok = strtok(line, spes);
				my_id = atoi(tok);
				tok = strtok(NULL, spes);

				ops[my_id].givenDataSTime = atoi(tok);

				tok = strtok(NULL, spes);

				ops[my_id].givenDataETime = atoi(tok);

				tok = strtok(NULL, spes);

				ops[my_id].regID = atoi(tok);

			}
	}
	fclose(bench);
	delete[] line;
}
//The original code is created by Huan Ren. Last modified by Owen and Keivn.
//"lookahead" approach was not discussed in the class but is used in this code, for forces calculation. please refer to the FDS paper section IV: E
//#include "stdafx.h"
#include <stdint.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <map>
#include <ctime>
#include <list>
#include <vector>
#include <queue>
#include <chrono>
#include <algorithm>
#include <sstream>

using namespace std;
const int tnum = 9; //number of operation types: note, single-speed only
int DFG = 0; //input DFG from 1 to 14, 0 is an example DFG
int LC = 0; //latency constraint, provided by input file

string DFGname;
string finalResult;

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

  bool constantInput;
};

struct resourcetype //store the delay info
{
  int delay;  //operation delay
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
  vector<vector<vector<Reg *> > > regID; //store fanin Regs
  vector<vector<vector<int>>> port1; //store connected Reg to port 1
  vector<vector<vector<int>>> port2; //store connected Reg to port 2

  vector<vector<vector<int>>> port1Ext; //store the operation ID with constant external input (assume all constant input on the different address, not in reg) for port1
  vector<vector<vector<int>>> port2Ext; //store the operation ID with constant external input (assume all constant input on the different address, not in reg) for port2

  vector<vector<vector<int>>> fanoutR; //store fanout Reg ID

  //e.g., if u is the parents of w and u's data stored in reg 1, reg 1 connected to a FU with port1, then, port2 has one more external constant input.
};

struct muxResult
{
  int demux21_16bit; // = Sum of all Reg's demux size - 1: Reg's demux = 0,1 --> size = 0; otherwise, size = # of demux inputs - 1;
  int mux21_16bit; // = Sum of all port's mux size - 1: port's # of inputs = 0,1 --> size = 0; otherwise, size = port size - 1;

  int maxDemux; //max input size for Demux
  int maxMux; //max input size for Mux
};

bool Comp(G_Node *i, G_Node *j) { return (i->startTime < j->startTime); }
bool CompTlive(G_Node *i, G_Node *j) { return (i->liveStartTime < j->liveStartTime); }
//remove repeat FU
bool Comp2(FU *i, FU *j) { return (i->id < j->id); }
bool Comp3(FU *i, FU *j) { return (i->id == j->id); }
int opn = 0; //# of operations

int mux_cost = 6;
int demux_cost = 6;
int reg_cost = 384;
double t;
int mux_num, demux_num, reg_num;
int mux_area, demux_area, reg_area;


G_Node* ops;    //operations list
resourcetype* rt = new resourcetype[tnum];  //stucture of operation types, which holds delay and distribution graph of each operation type
BindResult Bresult; //binding result
muxResult Mresult; //mux/demux result
vector<Reg *> reglist; //reg allocation result

//----------------INPUT FILES ------------------//
void readInput(int DFG, char** argv); //read input files: DFG info (node with predecessors and successors); LC; scheduling results
void GET_LIB(); //read FU delay info
//void Read_DFG(const int DFG, char** argv); //read DFG filename
void readGraphInfo(char **argv, int DFG, int *edge_num); //read DFG info based on the filename
//void get_Time(int DFG, char** argv); // read the filename of LC and scheduling results
void read_Time(int DFG, char** argv); //read LC and schedulinng results
void print_time(string str, double time);
void print_data(string str, double time);

//-------------------LE Binding-----------------//
void LEBind();
bool checkAvaFU(int time, vector<FU*> fulist);

//-------------------LE ALLOCATION--------------//
void RegAlloc();
bool regAval(int Tstart, vector<Reg *> reglist);

//-------------------Mux/DeMux------------------//
void MuxComp(vector<Reg *> reglist);

//-------------------PRINT RESULT---------------//
void PrintFinalResult(string str, string name);

int main(int argc, char **argv) {
  // Read DFG information: nodes, data-dependency (predecessors and successors of a node), LC, scheduling time info, execution time for each node
  readInput(DFG, argv);

  //get outputfile name as "DFG_bind_alloc_result.txt"
  stringstream str(argv[1]);
  string tok;
  while (getline(str, tok, '.'))
  {
    if (tok != "txt")
      DFGname = tok;
    //cout << tok << endl;
  }
  string s = "_bind_alloc_result.txt";
  string folder = "res/";
  finalResult = folder + DFGname + s;
  //check constant input for each non-input operation.

  for (auto n = 0; n < opn; ++n)
    if (ops[n].parent.size() == 1)
      ops[n].constantInput = true;
    else
      ops[n].constantInput = false;



  //main-function for LE-FUbinding + LE-Reg allocation: This is the part that you need to modified.
  //----------------LE-based FU Binding----------------//
  auto start = chrono::high_resolution_clock::now();

  LEBind();
  //print binding result

  /*for (auto i = 0; i < tnum; i++)
    if (!Bresult.bind[i].empty())
    {
      cout << "For type " << i << endl;
      for (auto j = 0; j < Bresult.bind[i].size(); j++)
        if (!Bresult.bind[i][j].empty())
        {
          cout << "My FU ID " << j << " :";
          for (auto k = 0; k < Bresult.bind[i][j].size(); k++)
            cout << Bresult.bind[i][j][k] << " ";
          cout << endl;
        }
    }*/



  //----------------LE-based REG ALLOCATION-----------//
  RegAlloc();
  auto end = chrono::high_resolution_clock::now();
  //store reg ID for each operation
  for (auto it = reglist.begin(); it != reglist.end(); it++)
    for (auto it2 = (*it)->operations.begin(); it2 != (*it)->operations.end(); it2++)
      (*it2)->regID = (*it)->id;






  //print reg allocation result

  /*for (auto it = reglist.begin(); it != reglist.end(); it++)
  {
    cout << "my Reg ID " << (*it)->id << ": ";
    for (auto it2 = (*it)->operations.begin(); it2 != (*it)->operations.end(); it2++)
    {
      cout << (*it2)->id << " ";
    }
    cout << endl;
  }*/

  MuxComp(reglist);
  PrintFinalResult(finalResult, DFGname); //command line window output


  for (auto n = 0; n < opn; ++n)
  {
    if (ops[n].child.size() > 0)
    {
      if (ops[n].liveStartTime > LC)
      {
        cout << "invalid Data-Live-Start time because LC = " << LC << " the start time = " << ops[n].liveStartTime << endl;
      }

      if (ops[n].liveEndTime == 0)
      {
        cout << "invalid Data-Live-End time with 0 " << endl;
      }
    }


  }

  string m = "_mapping.txt";
  string mapping = folder + DFGname + m;
  //write results into separate file by DFG name for CHECKER
  ofstream fout(mapping);

  for (auto n = 0; n < opn; ++n)
  {
    fout << ops[n].id << " " << ops[n].liveStartTime << " " << ops[n].liveEndTime << " " << ops[n].regID << endl;
  }

  fout.close();

  string time_reg = "time/" + DFGname + "_time.txt";
  t = (double)chrono::duration_cast<chrono::nanoseconds>(end-start).count()/1000000000;
  print_time(time_reg, t);
  string data = "data/" + DFGname + ".txt";
  print_data(data, t);

  return 0;
}
///////////////////////////////////////////////////////////////////////////////
void print_data(string str, double time) {

  ofstream fout(str, ios::out | ios::app);  // output file to save the scheduling results

  if (fout.is_open()) {
    fout << "DFG: " << str << endl;
    fout << endl;
    fout << "# registers: " << reg_num << endl;
    fout << "Area registers: " << reg_area << endl;
    fout << endl;
    fout << endl;
    fout << "# mux: " << mux_num << endl;
    fout << "Area mux: " << mux_area << endl;
    fout << endl;
    fout << endl;
    fout << "# demux: " << demux_num << endl;
    fout << "Area demux: " << demux_area << endl;
    fout << endl;
    fout << endl;
    fout << "Time Bind Alloc: " << time << " [s]" << endl;
  }
  else cout << "Unable to open file to write data";

}

void print_time(string str, double time) {

  ofstream fout(str, ios::out | ios::app);  // output file to save the scheduling results

  if (fout.is_open()) {
    fout << "Elapsed time Binding and Register alloation: " << time << " s";
    fout << endl;
  }
  else cout << "Unable to open file to write time";
}
// The main function for the LE-FUbinding-------------------------------------//
void LEBind()
{
  //store all operations into a global vector and sort by the increase order of T-scheduled (T-start)
  vector<G_Node *> operations;
  operations.clear();
  for (int i = 0; i < opn; i++)
    operations.push_back(&ops[i]);
  sort(operations.begin(), operations.end(), Comp);

  G_Node *current = new G_Node;
  //FU-list, 2D-vector, store FU info for each type
  vector<vector<FU*> > fulist(tnum, vector<FU*>());
  for (int i = 0; i < tnum; i++)
    fulist.clear();

  bool testFU = false;

  while (!operations.empty())
  {
    current = operations.front();

    if (fulist[current->type].empty() == 1) //empty FU-list for current type, need to allocate new FU and bind operation to it
    {
      FU* newFU = new FU; //allocate a new FU and bind the current operation into it.
      newFU->id = fulist[current->type].size();
      newFU->type = current->type;
      newFU->taval = current->endTime + 1;

      current->myFU = newFU;

      //check Fanin/out info based on the current binding info
      for (auto it = current->parent.begin(); it != current->parent.end(); it++)
        if ((*it)->myFU != NULL)
        {
          auto it3 = find(newFU->fanin.begin(), newFU->fanin.end(), (*it)->myFU);
          //find if the current fanin is existing
          if (it3 == newFU->fanin.end())
            newFU->fanin.push_back((*it)->myFU);
        }


      for (auto it = current->child.begin(); it != current->child.end(); it++)
        if ((*it)->myFU != NULL)
        {
          auto it3 = find(newFU->fanout.begin(), newFU->fanout.end(), (*it)->myFU);
          //find if the current fanout is existing
          if (it3 == newFU->fanout.end())
            newFU->fanout.push_back((*it)->myFU);
        }
      fulist[current->type].push_back(newFU); //allocate new FU into the list
      operations.erase(operations.begin()); //remove the current (first) element in the vector
    }
    else {// FU-list for current type is not empty:
        //case 1, available FU, check which one is the best to be bound (using the existing connection one);
        //case 2, non-available FU, need to allocate one.

      testFU = checkAvaFU(current->startTime, fulist[current->type]); //check available FUs

      FU *tempFU = new FU; //temp FU
      int tempExistConnect = -1;
      int myConnect = -1;
      if (testFU)
      {
        for (auto it = fulist[current->type].begin(); it != fulist[current->type].end(); it++)
          if ((*it)->taval <= current->startTime)
          {
            myConnect = 0;
            for (auto it2 = current->parent.begin(); it2 != current->parent.end(); it2++) //check fanin avaiable connections
              if ((*it2)->myFU != NULL)
              {
                auto it3 = find((*it)->fanin.begin(), (*it)->fanin.end(), (*it2)->myFU);
                if (it3 != (*it)->fanin.end()) //one fanin connection exists
                  myConnect += 1;
              }
            for (auto it2 = current->child.begin(); it2 != current->child.end(); it2++) //check fanout avaiable connections
              if ((*it2)->myFU != NULL)
              {
                auto it3 = find((*it)->fanout.begin(), (*it)->fanout.end(), (*it2)->myFU);
                if (it3 != (*it)->fanout.end()) //one fanout connection exists
                  myConnect += 1;
              }
            if (myConnect > tempExistConnect) //choose the best FU to be bound to (w/ the most existing connections)
            {
              tempExistConnect = myConnect;
              tempFU = (*it);
            }
          }
        // bind current node to tempFU (best one)
        tempFU->taval = current->endTime + 1;
        current->myFU = tempFU;
        //check fanin/out connections
        for (auto it = current->parent.begin(); it != current->parent.end(); it++)
          if ((*it)->myFU != NULL)
          {
            auto it3 = find(tempFU->fanin.begin(), tempFU->fanin.end(), (*it)->myFU);
            //find if the current fanin is existing
            if (it3 == tempFU->fanin.end())
              tempFU->fanin.push_back((*it)->myFU);
          }
        for (auto it = current->child.begin(); it != current->child.end(); it++)
          if ((*it)->myFU != NULL)
          {
            auto it3 = find(tempFU->fanout.begin(), tempFU->fanout.end(), (*it)->myFU);
            //find if the current fanin is existing
            if (it3 == tempFU->fanout.end())
              tempFU->fanout.push_back((*it)->myFU);
          }
        operations.erase(operations.begin()); //remove the current (first) element in the vector
      }
      else //no available one, need to allocate a new FU
      {
        FU* newFU = new FU; //allocate a new FU and bind the current operation into it.
        newFU->id = fulist[current->type].size(); //since the id starts from 0, the size (lastID+1) is the new ID
        newFU->type = current->type;
        newFU->taval = current->endTime + 1;

        current->myFU = newFU;
        //check Fanin/out info based on the current binding info
        for (auto it = current->parent.begin(); it != current->parent.end(); it++)
          if ((*it)->myFU != NULL)
          {
            auto it3 = find(newFU->fanin.begin(), newFU->fanin.end(), (*it)->myFU);
            //find if the current fanin is existing
            if (it3 == newFU->fanin.end())
              newFU->fanin.push_back((*it)->myFU);
          }
        for (auto it = current->child.begin(); it != current->child.end(); it++)
          if ((*it)->myFU != NULL)
          {
            auto it3 = find(newFU->fanout.begin(), newFU->fanout.end(), (*it)->myFU);
            //find if the current fanin is existing
            if (it3 == newFU->fanout.end())
              newFU->fanout.push_back((*it)->myFU);
          }
        fulist[current->type].push_back(newFU); //allocate new FU into the list
        operations.erase(operations.begin()); //remove the current (first) element in the vector
      }
    }
  }
  //obtain final binding results

  //initialization
  Bresult.bind.clear();
  Bresult.bind.resize(tnum, vector< vector<int> >()); //store binding operation
  Bresult.regID.clear();
  Bresult.regID.resize(tnum, vector< vector<Reg*> >()); //store fan-in Reg
  Bresult.port1.clear();
  Bresult.port1.resize(tnum, vector< vector<int> >()); //store connected Reg to port 1
  Bresult.port2.clear();
  Bresult.port2.resize(tnum, vector< vector<int> >()); //store connected Reg to port 2

  Bresult.port1Ext.clear();
  Bresult.port1Ext.resize(tnum, vector< vector<int> >()); //store connected Reg to port 1
  Bresult.port2Ext.clear();
  Bresult.port2Ext.resize(tnum, vector< vector<int> >()); //store connected Reg to port 2

  Bresult.fanoutR.clear();
  Bresult.fanoutR.resize(tnum, vector< vector<int> >()); //store connected fanout Reg


                             //Bresult: [Type][FU-ID][Operation-IDs]: step1 initialize list
  for (int i = 0; i < tnum; i++)
  {
    Bresult.bind[i].clear();
    Bresult.regID[i].clear();
    Bresult.port1[i].clear();
    Bresult.port2[i].clear();
    Bresult.port1Ext[i].clear();
    Bresult.port2Ext[i].clear();
    Bresult.fanoutR[i].clear();

    if (!fulist[i].empty())
    {
      Bresult.bind[i].resize(fulist[i].size(), vector<int>());
      Bresult.regID[i].resize(fulist[i].size(), vector<Reg*>());
      Bresult.port1[i].resize(fulist[i].size(), vector<int>());
      Bresult.port2[i].resize(fulist[i].size(), vector<int>());
      Bresult.port1Ext[i].resize(fulist[i].size(), vector<int>());
      Bresult.port2Ext[i].resize(fulist[i].size(), vector<int>());
      Bresult.fanoutR[i].resize(fulist[i].size(), vector<int>());

      for (int j = 0; j < fulist[i].size(); j++)
      {
        Bresult.bind[i][j].clear();
        Bresult.regID[i][j].clear();
        Bresult.port1[i][j].clear();
        Bresult.port2[i][j].clear();
        Bresult.port1Ext[i][j].clear();
        Bresult.port2Ext[i][j].clear();
        Bresult.fanoutR[i][j].clear();
      }
    }
  }
  //read every operation
  for (int i = 0; i < opn; i++)
    Bresult.bind[ops[i].type][ops[i].myFU->id].push_back(ops[i].id);
}

bool checkAvaFU(int time, vector<FU*> fulist)
{
  bool test = false;
  for (auto it = fulist.begin(); it != fulist.end(); it++)
    if ((*it)->taval <= time)
    {
      test = true;
      break;
    }
  return test;
}

//****************************************************************************//
// The main function for the LE - REGallocation-------------------------------//
void RegAlloc()
{ // compute live end time for every operation
  // live end T = max successor T-start - 1
  for (auto i = 0; i < opn; i++)
  {// T-live-start: cc after operation is done, or self endTime + 1;
   // T-live-end: max cc of all children's T-end
    //Output node: T start can be LC+1, T end: if < T-start: set as LC+2
    ops[i].liveStartTime = ops[i].endTime + 1;

    ops[i].liveEndTime = 0;

    if (ops[i].child.size() > 0)
    {
      for (auto it = ops[i].child.begin(); it != ops[i].child.end(); it++)
        if (ops[i].liveEndTime <= (*it)->endTime)
          ops[i].liveEndTime = (*it)->endTime;
    }
    else //output node
    {
      ops[i].liveEndTime = LC + 2;
    }


  }

  //vector to store all operations by increase order of T-start
  vector<G_Node *> operations;
  operations.clear();
  for (auto i = 0; i < opn; i++)
    operations.push_back(&ops[i]);

  //obtain all external inputs for input nodes
  int counter = 0;
  for (auto i = 0; i < opn; i++)
    if (ops[i].parent.empty()) //input node: no parent
    {
      G_Node *temp1 = new G_Node;
      G_Node *temp2 = new G_Node;

      temp1->id = opn + counter;
      counter++;
      temp2->id = opn + counter;
      counter++;

      temp1->liveStartTime = 1;
      temp2->liveStartTime = 1;

      temp1->liveEndTime = ops[i].endTime;
      temp2->liveEndTime = ops[i].endTime;

      temp1->child.clear();
      temp1->child.push_back(&ops[i]);
      ops[i].parent.push_back(temp1);
      temp2->child.clear();
      temp2->child.push_back(&ops[i]);
      ops[i].parent.push_back(temp2);

      temp1->parent.clear();
      temp2->parent.clear();

      operations.push_back(temp1);
      operations.push_back(temp2);
    }

  sort(operations.begin(), operations.end(), CompTlive);

  //start reg allocation
  //vector to store the regs info

  reglist.clear();

  G_Node *current = new G_Node;
  while (!operations.empty())
  {
    //new reg allocation: no available reg
    current = operations.front(); //read the first node in the list
    if (reglist.empty()) // no existing reg or available reg, need to allocate new one
    {
      Reg *newR = new Reg; //allocate new reg
      newR->id = 0;
      newR->operations.push_back(current);
      newR->Taval = current->liveEndTime + 1; //Reg's available time = current node's live end T + 1

      reglist.push_back(newR);
      operations.erase(operations.begin()); //remove the current node from the list
    }
    else // has existing regs
    {
      if (regAval(current->liveStartTime, reglist) == true) //exists available reg
      {
        for (auto it = reglist.begin(); it != reglist.end(); it++) //allocate to the first available reg,
          if ((*it)->Taval <= current->liveStartTime)
          {
            (*it)->operations.push_back(current);
            (*it)->Taval = current->liveEndTime + 1; //update T-aval

            operations.erase(operations.begin()); //remove the current node from the list
            break; //only allocate once.
          }
      }
      else //no available reg
      {
        Reg *newR = new Reg; //allocate new reg
        newR->id = reglist.size();
        newR->operations.push_back(current);
        newR->Taval = current->liveEndTime + 1; //Reg's available time = current node's live end T + 1

        reglist.push_back(newR);
        operations.erase(operations.begin()); //remove the current node from the list
      }
    } //end existing FU check
  }// end while loop (operation check)

}

bool regAval(int Tstart, vector<Reg *> reglist)
{
  bool test = false;
  for (auto it = reglist.begin(); it != reglist.end(); it++)
    if ((*it)->Taval <= Tstart)
    {
      test = true;
      break;
    }
  return test;
}

// Obtain size of Demux for each reg and Mux for each port of FUs
void MuxComp(vector<Reg *> reglist)
{
  /*
  //find demux: size of demux = # of fanins of regs (external input + outputs of FUs)
  for (auto it = reglist.begin(); it != reglist.end(); it++)
  {
    (*it)->extInput.clear();
    (*it)->outputFU.clear();
    for (auto it2 = (*it)->operations.begin(); it2 != (*it)->operations.end(); it2++)
    {
      if ((*it2)->id >= opn) //external inputs' id >= opn
      {
        (*it)->extInput.push_back(*it2);
      }
      else //operations
      {
        auto pos = find((*it)->outputFU.begin(), (*it)->outputFU.end(), (*it2)->myFU); //curent FU doesn't exist in the list
        if (pos == (*it)->outputFU.end())
          (*it)->outputFU.push_back((*it2)->myFU);
      }
    } //end for check operations (inputs)

    (*it)->demux = (*it)->extInput.size() + (*it)->outputFU.size();
    //cout << "my Reg ID " << (*it)->id << ", my demux size = " << (*it)->demux << endl;
  } //end for check each register
  */

  //start MUX size: For each port, test the data live time confliction.
  //Note all fanin size of FU = 2; (2 ports: port 1 and port 2)

  //cout << "DEBUGGING MUX:" << endl;

  //check each FU, and store reg-ID into them; This is the FU output info that connect to certain REGs
  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
        if (!Bresult.bind[i][j].empty())
          for (auto k = 0; k < Bresult.bind[i][j].size(); k++) //read each operation in each FU:  Bresult.bind[i][j][k] = ops.ID
            for (auto it = ops[Bresult.bind[i][j][k]].parent.begin(); it != ops[Bresult.bind[i][j][k]].parent.end(); it++) //read each parent operation and the reg
            {
              auto pos2 = find(Bresult.regID[i][j].begin(), Bresult.regID[i][j].end(), reglist[(*it)->regID]);
              if (pos2 == Bresult.regID[i][j].end()) //this reg does not exist in the list and store it.
                Bresult.regID[i][j].push_back(reglist[(*it)->regID]);
            }




  // MUX:

  //assign reg + constant external input to FU's port1 and port2 first
  //again, read each operation in each FU and from the parents to assign port (since max fan-in is 2, OK to assign port directly)
  int tempport = -1;
  //check each FU, and store reg-ID into them;
  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
        if (!Bresult.bind[i][j].empty())
          for (auto k = 0; k < Bresult.bind[i][j].size(); k++) //read each operation in each FU:  Bresult.bind[i][j][k] = ops.ID
          {
            for (auto it = ops[Bresult.bind[i][j][k]].parent.begin(); it != ops[Bresult.bind[i][j][k]].parent.end(); it++) //read each parent operation and the reg
            {
              if (ops[Bresult.bind[i][j][k]].parent.size() > 1) //two parents
              {
                auto pos3 = find(Bresult.port1[i][j].begin(), Bresult.port1[i][j].end(), (*it)->regID);
                auto pos4 = find(Bresult.port2[i][j].begin(), Bresult.port2[i][j].end(), (*it)->regID);

                //check both ports
                //Case 1: if anyone has the existing connection, done
                //Case 2: if one at least one ports has the existing connection: done (Note that, since two parents cannot be assigned to the same REG,
                //the existing connection on one port must be used by this parent right now (LE), then, the other operation has to check the other only.

                //Case 3: if non of two ports has the connection, choose the one with the minimum existing connections (including the constant input)
                if (pos3 == Bresult.port1[i][j].end() && pos4 == Bresult.port2[i][j].end()) //non of two ports has connected to this reg.
                {
                  //find the port with less size (including the ext size). This affect the max mux size
                  if (Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() >= Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size())
                    Bresult.port2[i][j].push_back((*it)->regID);
                  else
                    Bresult.port1[i][j].push_back((*it)->regID);
                  continue;
                }
                continue;
              }
              if (ops[Bresult.bind[i][j][k]].parent.size() == 1) //one parent
              {
                //Same as above, but the difference is:
                //For Case 1: the parent will "use" the port with larger input sizes so that, the other port can have the constant input to balance the size of mux.
                //For Case 2: the only way to bind constant input to an FU port is select the port that not connect to the parent's Reg.
                //For Case 3: doesn't matter, since one port has to connect to the reg, the other has to connect to the constant input, each port increases by 1.
                //assign the only one parent's reg to a port first.
                auto pos3 = find(Bresult.port1[i][j].begin(), Bresult.port1[i][j].end(), (*it)->regID);
                auto pos4 = find(Bresult.port2[i][j].begin(), Bresult.port2[i][j].end(), (*it)->regID);

                //check Case 3 both ports if anyone has the existing connection, done
                // To simplify the case, no change compared to the above for parent = 2
                if (pos3 == Bresult.port1[i][j].end() && pos4 == Bresult.port2[i][j].end()) //non of two ports has connected to this reg.
                {
                  //find the port with less size (including the ext size).
                  if (Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() >= Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size())
                  {
                    Bresult.port2[i][j].push_back((*it)->regID); //assign this reg to port2, then, need to assign the my constant input to port 1
                    Bresult.port1Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                  }
                  else
                  {
                    Bresult.port1[i][j].push_back((*it)->regID);
                    Bresult.port2Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                  }
                  continue;
                }

                //Case 2a
                if (pos3 == Bresult.port1[i][j].end() && pos4 != Bresult.port2[i][j].end()) //port2 has this existing connection, store constant to port 1.
                {
                  Bresult.port1Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                  continue;
                }

                //Case 2b
                if (pos3 != Bresult.port1[i][j].end() && pos4 == Bresult.port2[i][j].end()) //port1 has this existing connection, store constant to port 2.
                {
                  Bresult.port2Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                  continue;
                }

                //Case 1
                if (pos3 != Bresult.port1[i][j].end() && pos4 != Bresult.port2[i][j].end())
                {//both port1 and port2 have connection to the reg of the only parent, then, choose the one with the larger size
                  if (Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() >= Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size())
                  { //bind constant to the smaller one. port 2 now.
                    Bresult.port2Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                    continue;
                  }
                  if (Bresult.port2[i][j].size() + Bresult.port1Ext[i][j].size() >= Bresult.port1[i][j].size() + Bresult.port2Ext[i][j].size())
                  {
                    Bresult.port1Ext[i][j].push_back(ops[Bresult.bind[i][j][k]].id);
                    continue;
                  }
                }

                continue;
              }

            }// end for of checking all parents for each operatio
          }// end for of checking all operations in each FU


  //DEBUG Reg/FU-PORT1,2, CONST1, 2 INFO.
  // output all FU-Reg-Port information:
  // FU TYPE:
  // FU ID:
  // All Fan-in Reg:
  // Port 1 Reg:
  // Port 2 Reg:

/*  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
    {
      cout << "FU TYPE: " << i << endl;
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
      {
        if (!Bresult.regID[i][j].empty())
        {
          cout << "My FU ID " << j << ", all of my fan-in REG ID: ";
          for (auto d = 0; d < Bresult.regID[i][j].size(); d++)
            cout << Bresult.regID[i][j][d]->id << " ";
          cout << endl;
        }
        if (!Bresult.port1[i][j].empty())
        {
          cout << "My port 1 REG ID: ";
          for (auto d = 0; d < Bresult.port1[i][j].size(); d++)
            cout << Bresult.port1[i][j][d] << " ";
          cout << endl;
        }

        if (!Bresult.port1Ext[i][j].empty())
        {
          cout << "My port 1 constant (operation had) ID: ";
          for (auto d = 0; d < Bresult.port1[i][j].size(); d++)
            cout << Bresult.port1[i][j][d] << " ";
          cout << endl;
        }


        if (!Bresult.port2[i][j].empty())
        {
          cout << "My port 2 REG ID: ";
          for (auto d = 0; d < Bresult.port2[i][j].size(); d++)
            cout << Bresult.port2[i][j][d]  << " ";
          cout << endl;
        }

        if (!Bresult.port2Ext[i][j].empty())
        {
          cout << "My port 2 constant (operation had) ID: ";
          for (auto d = 0; d < Bresult.port1[i][j].size(); d++)
            cout << Bresult.port2[i][j][d]  << " ";
          cout << endl;
        }
      }
    }*/


  //MUX: for each FU, two MUX (each port):
  //Size: X-1 MUX --> X = sum(regs + constants) for each port
  //cout << "============================" << endl;
  //cout << "============MUX=============" << endl;

  //for each port, if input size = n, Mux size = n-1

  int total21MuxUsed = 0;
  int MaxSize = 0;


  //cout << "FU ID = (type, ID)" << endl;
  for (auto i = 0; i < tnum; i++) //for each Type
  {
    if (!Bresult.bind[i].empty())
    {
      for (auto j = 0; j < Bresult.bind[i].size(); j++)
      {
        //check if there is a port with 0 input, invalid
        //debug
        /*if (Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() == 0)
        {
          cout << "My FU ID: " << "(" << i << "," << j << ") " << " HAS O INPUT ON PORT 1 (INVALID)" << endl;
        }
        cout << "My FU ID: " << "(" << i << "," << j << ") " << "--my_port 1 total inputs (including constant) = " << Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() << endl;

        if (Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size() == 0)
        {
          cout << "My FU ID: " << "(" << i << "," << j << ") " << " HAS O INPUT ON PORT 2 (INVALID)" << endl;
        }
        cout << "My FU ID: " << "(" << i << "," << j << ") " << "--my_port 2 total inputs (including constant) = " << Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size() << endl;
        */
        total21MuxUsed += Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size() - 1;
        total21MuxUsed += Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size() - 1;


        if (MaxSize < Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size())
          MaxSize = Bresult.port1[i][j].size() + Bresult.port1Ext[i][j].size();
        if (MaxSize < Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size())
          MaxSize = Bresult.port2[i][j].size() + Bresult.port2Ext[i][j].size();
      }
    }
  }
  //cout << "-------------------" << endl;
  //cout << "Total # of MUX = " << total21MuxUsed << endl;
  //cout << "MAX MUX SIZE = " << MaxSize << endl;
  //cout << "-------------------" << endl;

  //cout << "============================" << endl;
  //cout << "===========DEMUX============" << endl;

  //DeMUX after each FU is the size of fanout Regs.


  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
        if (!Bresult.bind[i][j].empty())
          for (auto k = 0; k < Bresult.bind[i][j].size(); k++) //read each operation in each FU:  Bresult.bind[i][j][k] = ops.ID Don't consider the repeated ID here. take care later.
          {
            //debug ops ID (int) in bind, ops ID (operation structure), Reg_ID (operation structure)
            //cout << "my ops: " << Bresult.bind[i][j][k] << endl;
            //cout << "my ops ID: " << ops[Bresult.bind[i][j][k]].id << endl;
            //cout << "Reg ID: " << ops[Bresult.bind[i][j][k]].regID << endl;
            Bresult.fanoutR[i][j].push_back(ops[Bresult.bind[i][j][k]].regID);
          }

  //remove all repeated regID
  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
        if (!Bresult.fanoutR[i][j].empty())
        {
          //debug
          //cout << "My FU ID: " << "(" << i << "," << j << ") " << " HAS Fan-OUT Reg: ";
          //for (auto k = Bresult.fanoutR[i][j].begin(); k != Bresult.fanoutR[i][j].end(); k++)
            //cout << (*k) << " ";
          //cout << endl;
          sort(Bresult.fanoutR[i][j].begin(), Bresult.fanoutR[i][j].end()); //sort first,
          auto posRp = unique(Bresult.fanoutR[i][j].begin(), Bresult.fanoutR[i][j].end()); //find unique ID,
          Bresult.fanoutR[i][j].erase(posRp, Bresult.fanoutR[i][j].end()); //remove
        }

  int total12Demux = 0; //each FU has Demux size X = # of Fan-out Reg - 1.
  int maxDemuxSize = 0;

  for (auto i = 0; i < tnum; i++) //for each Type
    if (!Bresult.bind[i].empty())
      for (auto j = 0; j < Bresult.bind[i].size(); j++) //read each FU ID in each type
      {
        if (!Bresult.fanoutR[i][j].empty())
        {
          //cout << "My FU ID: " << "(" << i << "," << j << ") " << " HAS Fan-OUT size = " << Bresult.fanoutR[i][j].size() << endl;
          total12Demux += Bresult.fanoutR[i][j].size() - 1;

          if (maxDemuxSize < Bresult.fanoutR[i][j].size())
            maxDemuxSize = Bresult.fanoutR[i][j].size();
        }

        //else
          //cout << "My FU ID: " << "(" << i << "," << j << ") " << " HAS Fan-OUT = 0" << endl;
      }

  //demux: output port of FU --> certain set of Regs with size X (the 1:X Demux)

  //cout << "-------------------" << endl;
  //cout << "Total # of 12DeMUX = " << total12Demux << endl;
  //cout << "MAX DeMUX SIZE = " << maxDemuxSize << endl;
  //cout << "-------------------" << endl;

  //compute Mux/Demux result

  Mresult.demux21_16bit = 0;
  Mresult.mux21_16bit = 0;
  Mresult.maxDemux = 0;
  Mresult.maxMux = 0;

  Mresult.demux21_16bit = total12Demux;
  Mresult.mux21_16bit = total21MuxUsed;
  Mresult.maxDemux = maxDemuxSize;
  Mresult.maxMux = MaxSize;


  //compute # of mux/demux with 1-bit.

  //The final # for 16-bit mux/demux = 16 * (# of mux/demux for 1-bit)
  Mresult.demux21_16bit *= 16;
  Mresult.mux21_16bit *= 16;
}

//****************************************************************************//
//-------------------------------PRINT FUNCTION-------------------------------//
void PrintFinalResult(string str, string name)
{
  //command line output
  cout << "*****************************************" << endl;
  cout << "For DFG: " << name << endl;
  cout << "Total # of Registers: " << reglist.size() << endl;
  cout << "Total area of Registers: " << reglist.size() * reg_cost << endl;
  cout << "Total # of 2-1 Mux: " << Mresult.mux21_16bit << endl;
  cout << "Total area of 2-1 Mux: " << Mresult.mux21_16bit * mux_cost << endl;
  cout << "Total # of 2-1 Demux: " << Mresult.demux21_16bit << endl;
  cout << "Total area of 2-1 Demux: " << Mresult.demux21_16bit * demux_cost << endl;
  cout << "Max Mux size: " << Mresult.maxMux << endl;
  cout << "Max Demux size: " << Mresult.maxDemux << endl;
  cout << "*****************************************" << endl;

  //write results into one file
  //cout << str << endl;

  reg_num = reglist.size();
  reg_area = reglist.size() * reg_cost;
  mux_num = Mresult.mux21_16bit;
  mux_area = Mresult.mux21_16bit * mux_cost;
  demux_num = Mresult.demux21_16bit;
  demux_area = Mresult.demux21_16bit * demux_cost;

  ofstream fout(str);

  fout << "*****************************************" << endl;
  fout << "For DFG: " << name << endl;
  fout << "Total # of Registers: " << reglist.size() << endl;
  fout << "Total area of Registers: " << reglist.size() * reg_cost << endl;
  fout << "Total # of 2-1 Mux: " << Mresult.mux21_16bit << endl;
  fout << "Total area of 2-1 Mux: " << Mresult.mux21_16bit * mux_cost << endl;
  fout << "Total # of 2-1 Demux: " << Mresult.demux21_16bit << endl;
  fout << "Total area of 2-1 Demux: " << Mresult.demux21_16bit * demux_cost << endl;
  fout << "Max input size for Mux: " << Mresult.maxMux << endl;
  fout << "Max input size for Demux: " << Mresult.maxDemux << endl;
  fout << "*****************************************" << endl;

  fout.close();
}


//****************************************************************************//
//-------------------------------INPUT FILES---------------------------------//
void readInput(int DFG, char** argv)
{
  int edge_num = 0;
  edge_num = 0;
  readGraphInfo(argv, DFG, &edge_num);
  //Read Lib (delay info)
  GET_LIB(); // this part is hard code, can change this part based on the input file
         //Read latency constraint and node scheduling time info
  read_Time(DFG, argv);

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
void readGraphInfo(char **argv, int DFG, int *edge_num)
{
  FILE *bench;    //the input DFG file
  if (!(bench = fopen(argv[1], "r"))) //open the input DFG file to count the number of operation nodes in the input DFG
  {
    std::cerr << "Error: Reading input DFG file " << argv[1] << " failed." << endl;
    cin.get();  //waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }
  char *line = new char[100];
  opn = 0;      //initialize the number of operation node
  if (DFG != 16 && DFG != 17) //read input DFG file in format .txt
  {
    while (fgets(line, 100, bench))   //count the number of operation nodes in the input DFG
      if (strstr(line, "label") != NULL)  //if the line contains the keyword "label", the equation returns true, otherwise it returns false. search for c/c++ function "strstr" for detail
        opn++;
  }
  else //read input DFG file from format .gdl
  {
    while (fgets(line, 100, bench))
      if (strstr(line, "node") != NULL)
        opn++;
  }
  fclose(bench);  //close the input DFG file
  ops = new G_Node[opn];
  //close the input DFG file
  //based on the number of operation node in the DFG, dynamically set the size
  std::map<string, int> oplist;
  string name, cname;
  char *tok, *label;  //label: the pointer point to "label" or "->" in each line
  char seps[] = " \t\b\n:";   //used with strtok() to extract DFG info from the input DFG file
  int node_id = 0;  //count the number of edges in the input DFG file
  if (!(bench = fopen(argv[1], "r"))) //open the input DFG file agian to read the DFG, so that the cursor returns back to the beginning of the file
  {
    std::cerr << "Error: Failed to load the input DFG file." << endl;
    cin.get();  //waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }
  if (DFG != 16 && DFG != 17)
    while (fgets(line, 100, bench)) //read a line from the DFG file, store it into "line[100]
    {
      if ((label = strstr(line, "label")) != NULL)  //if a keyword "label" is incurred, that means a operation node is found in the input DFG file
      {
        tok = strtok(line, seps); //break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok); //obtain the node name
        oplist.insert(make_pair(name, node_id));  //match the name of the node to its number flag
        tok = strtok(label + 7, seps);  //obtain the name of the operation type
        if (strcmp(tok, "ADD") == 0)    ops[node_id].type = 0;      //match the operation type to the nod. search for c/c++ function "strcmp" for detail
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
      else if ((label = strstr(line, "->")) != NULL)  //if a keyword "->" is incurred, that means an edge is found in the input DFG file
      {
        tok = strtok(line, seps); //break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok); //obtain node name u from edge (u, v)
        cname.assign(strtok(label + 3, seps));  ////obtain node name v from edge (u, v)
        (ops[oplist[name]].child).push_back(&(ops[oplist[cname]])); //use double linked list to hold the children
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
          ops[node_id].type = 0;  //since there are only 4 operation types in .gdl file, we change type 2 to division and keep the other types unchanged.
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

void read_Time(int DFG, char** argv)
{
  FILE *bench;    //the input DFG file
  if (!(bench = fopen(argv[2], "r"))) //open the input DFG file to count the number of operation nodes in the input DFG
  {
    std::cerr << "Error: Reading input DFG file " << argv[2] << " failed." << endl;
    cin.get();  //waiting for user to press enter to terminate the program, so that the text can be read
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

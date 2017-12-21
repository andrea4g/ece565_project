/*----------------------------------------------------------------------------*/
/*------------------------------LIBRARIES-------------------------------------*/
/*----------------------------------------------------------------------------*/
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
#include <algorithm>
#include <sstream>

#define DEBUG 0

using namespace std;

/*----------------------------------------------------------------------------*/
/*--------------------------STRUCT DECLARATION--------------------------------*/
/*----------------------------------------------------------------------------*/
struct FU {
  int type;             // FU-type
  int id;               // FU-id
  vector<FU *> fanin;
  vector<FU *> fanout;
  int taval;            // available time

  int TavalPort1;
  int TavalPort2;

  vector<int> port1;    // store reg_id
  vector<int> port2;    // store reg_id
};

/*----------------------------------------------------------------------------*/

struct G_Node {           // save the info for operation node
  int id;
  int type;
  list<G_Node *> child;   // get input from the current node
  list<G_Node *> parent;  // send output to the current node
  FU *myFU;

  int regID;
  int startTime;          // scheduled time
  int endTime;            // start + delay - 1
  int liveStartTime;      // live start time for reg allocation
  int liveEndTime;        // live end time for Reg Allocation
};

/*----------------------------------------------------------------------------*/

struct resourcetype { //store the delay info
  int delay;          // operation delay
};

/*----------------------------------------------------------------------------*/

struct Reg {
  int id;       // my local id for each FU
  int globalID; // my global id
  int Taval;
  vector<G_Node *> operations;
  vector<G_Node *> extInput;  // store connected external Input
  vector<FU *> outputFU;      // store connected fanin-FU
  int demux;
};

/*----------------------------------------------------------------------------*/

struct BindResult {
  vector<vector<vector<int> > > bind;     // store binding operation
  vector<vector<vector<Reg *> > > regID;  // store connected Regs
  vector<vector<vector<int>>> port1;      // store connected Reg to port 1
  vector<vector<vector<int>>> port2;      // store connected Reg to port 2
};

/*----------------------------------------------------------------------------*/

struct muxResult {
  int demux21_16bit;  // = Sum of all Reg's demux size - 1:
                      // Reg's demux = 0,1 --> size = 0;
                      // otherwise, size = # of demux inputs - 1;
  int mux21_16bit;    // = Sum of all port's mux size - 1:
                      // port's # of inputs = 0,1 --> size = 0;
                      // otherwise, size = port size - 1;
  int maxDemux;
  int maxMux;
};

/*----------------------------------------------------------------------------*/
/*--------------------------GLOBAL VARIABLES----------------------------------*/
/*----------------------------------------------------------------------------*/

const int tnum = 9; // number of operation types: note, single-speed only
int DFG = 0;        // input DFG from 1 to 14, 0 is an example DFG
int LC = 0;         // latency constraint, provided by input file
int opn = 0;        // # of operations
string DFGname;
string finalResult;

G_Node* ops;            // operations list
BindResult Bresult;     // binding result
muxResult Mresult;      // mux/demux result
vector<Reg *> reglist;  // reg allocation result
resourcetype* rt = new resourcetype[tnum];  // stucture of operation types,
                                            // which holds delay and
                                            // distribution graph of each
                                            // operation type

/*----------------------------------------------------------------------------*/
/*-----------------------------PROTOTYPES-------------------------------------*/
/*----------------------------------------------------------------------------*/

bool Comp(G_Node *i, G_Node *j) {
  return (i->startTime < j->startTime);
}

bool CompTlive(G_Node *i, G_Node *j) {
  return (i->liveStartTime < j->liveStartTime);
}

// remove repeat FU
bool Comp2(FU *i, FU *j) {
  return (i->id < j->id);
}

bool Comp3(FU *i, FU *j) {
  return (i->id == j->id);
}

void readInput(int DFG, char** argv); // read input files: 
                                      // DFG info (node with predecessors and 
                                      // successors); LC; scheduling results
void GET_LIB();                       // read FU delay info
void readGraphInfo(char **argv, int DFG, int *edge_num); // read DFG info based on the filename
void read_Time(int DFG, char** argv); // read LC and schedulinng results

// functions for LE-FUbinding
void LEBind();
bool checkAvaFU(int time, vector<FU*> fulist);
//-------------------LE Binding-----------------//
void RegAlloc();
bool regAval(int Tstart, vector<Reg *> reglist);
//-------------------Mux/DeMux-----------------//
void MuxComp(vector<Reg *> reglist);

void PrintFinalResult(string str);
void print_time(string str, double time);

/*----------------------------------------------------------------------------*/
/*--------------------------------MAIN----------------------------------------*/
/*----------------------------------------------------------------------------*/

int main(int argc, char **argv) {

  readInput(DFG, argv);

  // get outputfile name as "DFG_bind_alloc_result.txt"
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


  timespec t;
  t.tv_sec = 0;
  t.tv_nsec = 0;
  clock_settime(CLOCK_PROCESS_CPUTIME_ID, &t);
  //----------------LE-based FU Binding----------------//
  LEBind();

  //----------------LE-based REG ALLOCATION-----------//
  RegAlloc();

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);

  // store reg ID for each operation
  for (auto it = reglist.begin(); it != reglist.end(); it++)
    for (auto it2 = (*it)->operations.begin(); it2 != (*it)->operations.end(); it2++)

  MuxComp(reglist);
  PrintFinalResult(finalResult); // print output and write into separate files

  string time_FDS = "time/" + DFGname + "_time.txt";
  print_time(time_FDS, (double)t.tv_nsec/1000000000);

  return 0;
}

/*----------------------------------------------------------------------------*/
/*-----------------------------FUNCTIONS--------------------------------------*/
/*----------------------------------------------------------------------------*/

void print_time(string str, double time) {

  ofstream fout(str, ios::out | ios::app);  // output file to save the scheduling results

  if (fout.is_open()) {
    fout << "Elapsed time Binding and Register alloation: " << time << " s";
    fout << endl;
  }
  else cout << "Unable to open file to write time";
}

// The main function for the LE-FUbinding
void LEBind() {
  // store all operations into a global vector and sort by the increase order of T-scheduled (T-start)
  vector<G_Node *> operations;
  operations.clear();
  for (int i = 0; i < opn; i++)
    operations.push_back(&ops[i]);

  sort(operations.begin(), operations.end(), Comp);

  G_Node *current = new G_Node;
  // FU-list, 2D-vector, store FU info for each type
  vector<vector<FU*> > fulist(tnum, vector<FU*>());
  for (int i = 0; i < tnum; i++)
    fulist.clear();

  bool testFU = false;

  while (!operations.empty()) {
    current = operations.front();
    // empty FU-list for current type, need to allocate new FU and bind operation to it
    if (fulist[current->type].empty() == 1) {
      FU* newFU = new FU; // allocate a new FU and bind the current operation into it.
      newFU->id = fulist[current->type].size();
      newFU->type = current->type;
      newFU->taval = current->endTime + 1;

      current->myFU = newFU;

      //check Fanin/out info based on the current binding info
      for (auto it = current->parent.begin(); it != current->parent.end(); it++) {
        if ((*it)->myFU != NULL) {
          auto it3 = find(newFU->fanin.begin(), newFU->fanin.end(), (*it)->myFU);
          // find if the current fanin is existing
          if (it3 == newFU->fanin.end()) {
            newFU->fanin.push_back((*it)->myFU);
          } // end if it3
        } // end if it
      } // end for auto it

      for (auto it = current->child.begin(); it != current->child.end(); it++) {
        if ((*it)->myFU != NULL) {
          auto it3 = find(newFU->fanout.begin(), newFU->fanout.end(), (*it)->myFU);
          //find if the current fanin is existing
          if (it3 == newFU->fanout.end()) {
            newFU->fanout.push_back((*it)->myFU);
          } // end if it3
        } // end if it
      } // end for auto it

      fulist[current->type].push_back(newFU); // allocate new FU into the list
      operations.erase(operations.begin());   // remove the current (first) element in the vector
    }
    else {    // FU-list for current type is not empty:
              // case 1, available FU, check which one is the best to be bound (using the existing connection one);
              // case 2, non-available FU, need to allocate one.

      testFU = checkAvaFU(current->startTime, fulist[current->type]); //check available FUs

      FU *tempFU = new FU; //temp FU
      int tempExistConnect = -1;
      int myConnect = -1;
      if (testFU) {
        for (auto it = fulist[current->type].begin(); it != fulist[current->type].end(); it++)
          if ((*it)->taval <= current->startTime) {
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
  Bresult.regID.resize(tnum, vector< vector<Reg*> >()); //store connected Reg
  Bresult.bind.clear();
  Bresult.port1.resize(tnum, vector< vector<int> >()); //store connected Reg to port 1
  Bresult.bind.clear();
  Bresult.port2.resize(tnum, vector< vector<int> >()); //store connected Reg to port 2

  // Bresult: [Type][FU-ID][Operation-IDs]: step1 initialize list
  for (int i = 0; i < tnum; i++) {
    Bresult.bind[i].clear();
    Bresult.regID[i].clear();
    Bresult.port1[i].clear();
    Bresult.port2[i].clear();

    if (!fulist[i].empty()) {
      Bresult.bind[i].resize(fulist[i].size(), vector<int>());
      Bresult.regID[i].resize(fulist[i].size(), vector<Reg*>());
      Bresult.port1[i].resize(fulist[i].size(), vector<int>());
      Bresult.port2[i].resize(fulist[i].size(), vector<int>());

      for (int j = 0; j < fulist[i].size(); j++) {
        Bresult.bind[i][j].clear();
        Bresult.regID[i][j].clear();
        Bresult.port1[i][j].clear();
        Bresult.port2[i][j].clear();
      } // end for j
    } // end if fulist
  } // end for i

  //read every operation
  for (int i = 0; i < opn; i++)
    Bresult.bind[ops[i].type][ops[i].myFU->id].push_back(ops[i].id);

  for ( int i = 0; i < tnum; i++ ) {
#if DEBUG
    cout << i << " " << fulist[i].size() << endl;
#endif
  }

}

/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
// The main function for the LE - FUbinding-----------------------------------//
//****************************************************************************//
// The main function for the LE - REGallocation-------------------------------//
void RegAlloc() {
// compute live end time for every operation for each FU: local Reg allocation
// live end T = max successor T-start - 1

  for (auto i = 0; i < opn; i++){
    // T-live-start: cc after operation is done, or self endTime + 1;
    // T-live-end: max cc of all children's T-end
    ops[i].liveStartTime = ops[i].endTime + 1;

    ops[i].liveEndTime = 0;
    for (auto it = ops[i].child.begin(); it != ops[i].child.end(); it++)
      if (ops[i].liveEndTime <= (*it)->endTime)
        ops[i].liveEndTime = (*it)->endTime;
  }

  //obtain all external inputs for input nodes
  int counter = 0;
  for (auto i = 0; i < opn; i++) {
    if (ops[i].parent.empty()) {  // input node: no parent
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
    }
  }

  // for each FU, create a vector to store all local data
  vector<G_Node *> myOpr;
  reglist.clear();
  int globalID = 0;

  for (auto i = 0; i < tnum; i++) { //  for each f-type
    if (!Bresult.bind[i].empty()) { // if exists FUs
      for (auto j = 0; j < Bresult.bind[i].size(); j++) {//for each FU
        if (!Bresult.bind[i][j].empty()) {  // exists operations
          //store all my fan-in opeartions first
          myOpr.clear();

          for (auto k = 0; k < Bresult.bind[i][j].size(); k++) {  //for each of my operation
            for (auto it = ops[Bresult.bind[i][j][k]].parent.begin(); it != ops[Bresult.bind[i][j][k]].parent.end(); it++) {  // for each of parent
              auto pos = find(myOpr.begin(), myOpr.end(), (*it)); //only push into the vector at the first time
              if (pos == myOpr.end())
                myOpr.push_back(*it);
            }
          }
          // finish store all my fanin operation
          // sort by the T-live-start

          sort(myOpr.begin(), myOpr.end(), CompTlive);

          // start reg allocation
          // my reg vector to store the local regs info
          Bresult.regID[i][j].clear();

          G_Node *current = new G_Node;
          while (!myOpr.empty()) {
            // new reg allocation: no available local reg
            current = myOpr.front();
            if (Bresult.regID[i][j].empty()) {  // no existing reg or available
              Reg *newR = new Reg; //allocate new reg
              newR->id = 0;

              newR->globalID = globalID; //get my global reg ID

              globalID++;
              reglist.push_back(newR);

              newR->operations.push_back(current);
              newR->Taval = current->liveEndTime + 1; // Reg's avaiable time = current node's live end T + 1

              Bresult.regID[i][j].push_back(newR);
              myOpr.erase(myOpr.begin());
            } // end if
            else {  // has existing local regs
              if (regAval(current->liveStartTime, Bresult.regID[i][j]) == true) { // exist local avaiable reg
                for (auto it = Bresult.regID[i][j].begin(); it != Bresult.regID[i][j].end(); it++)
                  if ((*it)->Taval <= current->liveStartTime) { // allocate to the first avaiable local reg.
                    (*it)->operations.push_back(current);
                    (*it)->Taval = current->liveEndTime + 1; //update T-aval

                    myOpr.erase(myOpr.begin()); //remove the current from the list
                    break; //only allocate once
                  }
              }
              else {  // no available existing local reg
                Reg *newR = new Reg; //allocate new local reg
                newR->id = reglist.size(); //obtain the new reg ID
                newR->operations.push_back(current); //store the related operation into this R
                newR->Taval = current->liveEndTime + 1; //update available T


                newR->globalID = globalID; //get my global reg ID
                globalID++; //global ID counter up

                reglist.push_back(newR);

                Bresult.regID[i][j].push_back(newR);
                myOpr.erase(myOpr.begin());
              } // end if existing but not available local reg
            } // end if no existing or not local reg
          } // finished allocate local reg
        } //
      } // end for loop for each existing FU with # of non-0 operations
    } // end for loop for each f-type
  } // end fora auto i
}

/*----------------------------------------------------------------------------*/
bool regAval(int Tstart, vector<Reg *> reglist) {
  bool test = false;
  for (auto it = reglist.begin(); it != reglist.end(); it++) {
    if ((*it)->Taval <= Tstart) {
      test = true;
      break;
    }
  }
  return test;
}

/*----------------------------------------------------------------------------*/

// The main function for the LE - REGallocation
// Obtain size of Demux for each reg and Mux for each port of FUs
void MuxComp(vector<Reg *> reglist) {
  // find demux: size of demux = # of fanins of regs (external input + outputs of FUs)
  for (auto it = reglist.begin(); it != reglist.end(); it++) {
    (*it)->extInput.clear();
    (*it)->outputFU.clear();
    for (auto it2 = (*it)->operations.begin(); it2 != (*it)->operations.end(); it2++) {
      if ((*it2)->id >= opn) {  // external inputs' id >= opn
        (*it)->extInput.push_back(*it2);
      }
      else {  // operations
        auto pos = find((*it)->outputFU.begin(), (*it)->outputFU.end(), (*it2)->myFU); // current FU doesn't exist in the list
        if (pos == (*it)->outputFU.end()){
          (*it)->outputFU.push_back((*it2)->myFU);
        }
      }
    } // end for check operations (inputs)

    (*it)->demux = (*it)->extInput.size() + (*it)->outputFU.size();
    // cout << "my Reg ID " << (*it)->id << ", my demux size = " << (*it)->demux << endl;
  } // end for check each register

  // start MUX size: For each port, test the data live time confliction.
  // Note all fanin size of FU = 2; (2 ports: port 1 and port 2)
  //cout << "DEBUGGING MUX:" << endl;

  // assign port1 and port2
  // again, read each operation in each FU and from the parents to assign port 
  // (since max fan-in is 2, OK to assign port directly)
  int counter = 0;
  // check each FU, and store reg-ID into them;
  for (auto i = 0; i < tnum; i++) { // for each Type
    if (!Bresult.bind[i].empty()) {
      for (auto j = 0; j < Bresult.bind[i].size(); j++) {   // read each FU ID in each type
        if (!Bresult.bind[i][j].empty()) {
          for (auto k = 0; k < Bresult.bind[i][j].size(); k++) {  // read each operation in each FU:  Bresult.bind[i][j][k] = ops.ID
            counter = 0;
            for (auto it = ops[Bresult.bind[i][j][k]].parent.begin(); it != ops[Bresult.bind[i][j][k]].parent.end(); it++) {  // read each parent operation and the reg
              counter++;
              if (counter == 1) {
                auto pos3 = find(Bresult.port1[i][j].begin(), Bresult.port1[i][j].end(), (*it)->regID);
                if (pos3 == Bresult.port1[i][j].end()) {
                  Bresult.port1[i][j].push_back((*it)->regID);
                }
              }
              if (counter == 2) {
                auto pos3 = find(Bresult.port2[i][j].begin(), Bresult.port2[i][j].end(), (*it)->regID);
                if (pos3 == Bresult.port2[i][j].end()) {
                  Bresult.port2[i][j].push_back((*it)->regID);
                }
              }
            } // end for of checking all parents for each operation
          } // end for of checking all operations in each FU
        } // end if Bresult
      } // end for auto j
    } // end if Bresult
  } // end for auto i

  //compute Mux/Demux result
  Mresult.demux21_16bit = 0;
  Mresult.mux21_16bit = 0;
  Mresult.maxDemux = 0;
  Mresult.maxMux = 0;
  for (auto it = reglist.begin(); it != reglist.end(); it++) {
    if ((*it)->demux > Mresult.maxDemux)
      Mresult.maxDemux = (*it)->demux;

    if ((*it)->demux > 1)
      Mresult.demux21_16bit += (*it)->demux - 1;
  }

  for (auto i = 0; i < tnum; i++)
    if (!Bresult.regID[i].empty())
      for (auto j = 0; j < Bresult.regID[i].size(); j++)
        if (!Bresult.regID[i][j].empty()) {
          if (Bresult.port1[i][j].size() > 1) {
            Mresult.mux21_16bit += Bresult.port1[i][j].size() - 1;
            if (Bresult.port1[i][j].size() > Mresult.maxMux)
              Mresult.maxMux = Bresult.port1[i][j].size();
          }
          if (Bresult.port2[i][j].size() > 1) {
            Mresult.mux21_16bit += Bresult.port2[i][j].size() - 1;
            if (Bresult.port2[i][j].size() > Mresult.maxMux)
              Mresult.maxMux = Bresult.port2[i][j].size();
          }
        }

  Mresult.demux21_16bit *= 16;
  Mresult.mux21_16bit *= 16;
}

/*----------------------------------------------------------------------------*/
void PrintFinalResult(string str) {
#if DEBUG
  cout << "*****************************************" << endl;
  cout << "For DFG: " << str << endl;
  cout << "Total # of Registers: " << reglist.size() << endl;
  cout << "Total # of 2-1 Mux: " << Mresult.mux21_16bit << endl;
  cout << "Total # of 2-1 Demux: " << Mresult.demux21_16bit << endl;
  cout << "Max Mux size: " << Mresult.maxMux << endl;
  cout << "Max Demux size: " << Mresult.maxDemux << endl;
  cout << "*****************************************" << endl;
#endif
  //write results into separate file by DFG name
  ofstream fout(str);

  fout << "For DFG: " << str << endl;
  fout << "Total # of Registers: " << reglist.size() << endl;
  fout << "Total # of 2-1 Mux: " << Mresult.mux21_16bit << endl;
  fout<< "Total # of 2-1 Demux: " << Mresult.demux21_16bit << endl;
  fout << "Max Mux size: " << Mresult.maxMux << endl;
  fout << "Max Demux size: " << Mresult.maxDemux << endl;

  fout.close();
}


//-------------------------------INPUT FILES---------------------------------//
/*----------------------------------------------------------------------------*/

void readInput(int DFG, char** argv) {
  int edge_num = 0;
  edge_num = 0;
  readGraphInfo(argv, DFG, &edge_num);
  // Read Lib (delay info)
  GET_LIB();  // this part is hard code, can change this part based on the input file
              //Read latency constraint and node scheduling time info
  read_Time(DFG, argv);

  // obtain the end-time for each operation: T-end = T-start + delay - 1
  for (int i = 0; i < opn; i++)
    ops[i].endTime = ops[i].startTime + rt[ops[i].type].delay - 1;
}

/*----------------------------------------------------------------------------*/

void GET_LIB() {
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

/*----------------------------------------------------------------------------*/

void readGraphInfo(char **argv, int DFG, int *edge_num) {
  FILE *bench;    // the input DFG file
  if (!(bench = fopen(argv[1], "r"))) { // open the input DFG file to count the number of operation nodes in the input DFG
    std::cerr << "Error: Reading input DFG file " << argv[1] << " failed." << endl;
    cin.get();  // waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }
  char *line = new char[100];
  opn = 0;      // initialize the number of operation node
  if (DFG != 16 && DFG != 17) { // read input DFG file in format .txt
    while (fgets(line, 100, bench))       // count the number of operation nodes in the input DFG
      if (strstr(line, "label") != NULL)  // if the line contains the keyword "label", the equation returns true, otherwise it returns false
        opn++;
  }
  else {  // read input DFG file from format .gdl
    while (fgets(line, 100, bench))
      if (strstr(line, "node") != NULL)
        opn++;
  }

  fclose(bench);  // close the input DFG file
  ops = new G_Node[opn];

  // close the input DFG file
  // based on the number of operation node in the DFG, dynamically set the size
  std::map<string, int> oplist;
  string name, cname;
  char *tok, *label;          // label: the pointer point to "label" or "->" in each line
  char seps[] = " \t\b\n:";   // used with strtok() to extract DFG info from the input DFG file
  int node_id = 0;            // count the number of edges in the input DFG file
  if (!(bench = fopen(argv[1], "r"))) { // open the input DFG file agian to read the DFG, so that the cursor returns back to the beginning of the file
    std::cerr << "Error: Failed to load the input DFG file." << endl;
    cin.get();  //waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }
  if (DFG != 16 && DFG != 17)
    while (fgets(line, 100, bench)) { // read a line from the DFG file, store it into "line[100]
      if ((label = strstr(line, "label")) != NULL)  { // if a keyword "label" is incurred, that means a operation node is found in the input DFG file
        tok = strtok(line, seps); //break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok); // obtain the node name
        oplist.insert(make_pair(name, node_id));  // match the name of the node to its number flag
        tok = strtok(label + 7, seps);  // obtain the name of the operation type
        if (strcmp(tok, "ADD") == 0)      ops[node_id].type = 0;  // match the operation type to the nod. search for c/c++ function "strcmp" for detail
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
      else if ((label = strstr(line, "->")) != NULL) { // if a keyword "->" is incurred, that means an edge is found in the input DFG file
        tok = strtok(line, seps); // break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok); // obtain node name u from edge (u, v)
        cname.assign(strtok(label + 3, seps));  // obtain node name v from edge (u, v)
        (ops[oplist[name]].child).push_back(&(ops[oplist[cname]]));   // use double linked list to hold the children
        (ops[oplist[cname]].parent).push_back(&(ops[oplist[name]]));  // use double linked list to hold the parents
        (*edge_num)++;
      }
    }
  else {
    int child, parent;
    while (fgets(line, 100, bench)) {
      if (strstr(line, "node")) {
        fgets(line, 100, bench);
        tok = strtok(line, seps);
        tok = strtok(NULL, seps);
        if (atoi(tok) == 1)
          ops[node_id].type = 0;  //since there are only 4 operation types in .gdl file, we change type 2 to division and keep the other types unchanged.
        else
          ops[node_id].type = atoi(tok);

        ops[node_id].id = node_id;
        node_id++;
      } // end if strstr
      else if (strstr(line, "edge")) {
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
      } // end else if
    } // end while fgets
  } // end of reading DFG
  fclose(bench);
  delete[] line;
}

/*----------------------------------------------------------------------------*/

void read_Time(int DFG, char** argv) {
  FILE *bench;    // the input DFG file
  if (!(bench = fopen(argv[2], "r"))) {   // open the input DFG file to count the number of operation nodes in the input DFG
    std::cerr << "Error: Reading input DFG file " << argv[2] << " failed." << endl;
    cin.get();  // waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }

  char *line = new char[100];
  char *tok, *label;
  char spes[] = " ,.-/n/t/b";
  int my_id = 0;
  int node_num = 0;
  while (fgets(line, 100, bench)) {
    if ((label = strstr(line, "LC")) != NULL) {   // read LC info, first line
      tok = strtok(line, spes);
      tok = strtok(NULL, spes);
      LC = atoi(tok);
    }
    else {    // read t-scheduled info
      if (node_num < opn) {   // exact opn lines (1 node per line)
        node_num++;
        tok = strtok(line, spes);
        my_id = atoi(tok);
        tok = strtok(NULL, spes);
        ops[my_id].startTime = atoi(tok);
      } // end if
    } // end else
  } // end while

  fclose(bench);
  delete[] line;
}

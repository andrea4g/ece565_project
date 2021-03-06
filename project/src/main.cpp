/*----------------------------------------------------------------------------*/
/*------------------------------LIBRARIES-------------------------------------*/
/*----------------------------------------------------------------------------*/
//#include "stdfax.h"
#include <stdint.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <map>
#include <set>
#include <ctime>
#include <list>
#include <vector>
#include <queue>
#include <sstream>
#include <chrono>

using namespace std;

/*----------------------------------------------------------------------------*/
/*--------------------------STRUCT DECLARATION--------------------------------*/
/*----------------------------------------------------------------------------*/

struct FU {
  int type;             // FU-type
  int id;               // FU-id
  vector<int> busy;

  int demux_size;
  int taval;            // available time
  int TavalPort1;
  int TavalPort2;

  set<int> port0;
  set<int> port1;

  set<int> future_child;
  set<int> future_parent;

};


struct Reg {
  int id;       // my local id for each FU
  int globalID; // my global id
  vector<int> busy;

  vector<struct G_Node *> extInput;  // store connected external Input
  vector<FU *> outputFU;      // store connected fanin-FU
  //int mux_size;

  set<FU*> in_FU;
  set<FU*> out_FU;

};

struct G_Node {
  int id;                       // node ID
  int type;                     // node Function-type
  list<G_Node *> child;         // successor nodes (distance = 1)
  list<G_Node *> parent;        // predecessor nodes (distance = 1)
  int asap, alap, tasap, talap; // T-asap, T-alap (in # of clock cycle), Temp-Tasap, Temp-Talap;
  bool schl, tschl;             // bool, = 1 scheduled; =0 not
  int schl_time;
  FU* my_FU;
  Reg* my_reg;

  int lifetime_end;

};

/*----------------------------------------------------------------------------*/
/*--------------------------GLOBAL VARIABLES----------------------------------*/
/*----------------------------------------------------------------------------*/
const int tnum = 9;                   // number of operation type
double latency_parameter;             // latency constant parameter, change this
                                      // parameter can affect the latency
                                      // constraint and hence, the final
                                      // scheduling result is changed

int DFG = 0;                          // DFG ID
int LC = 0;                           // global parameter latency constraint.
                                      // LC = latency_parameter * ASAP latency.

string DFGname;                       // filename (DFG name) for input/output
string outputfile;                    // output filename

int opn = 0;                          // # of operations in current DFG
G_Node* ops;                          // operations list
int* top_order;
int rt[tnum];                         // delay info
int ct[tnum];                         // area info
vector< list<FU*> > FU_list(tnum, list<FU*> () );

int mux_cost = 6;
int demux_cost = 6;
int reg_cost = 384;

double t;
int fu_area;
int reg_area;
int mux_area;
int demux_area;
int mux_num, demux_num, reg_num, fu_num, area;

list<Reg*> reg_pool;


/*----------------------------------------------------------------------------*/
/*-----------------------------PROTOTYPES-------------------------------------*/
/*----------------------------------------------------------------------------*/
void GET_LIB();                                           // hardcode for delay info (rt[])
void Read_DFG(const int DFG, char** argv);                // Read-DFG filename
void readGraphInfo(char **argv, int DFG, int *edge_num);  // Read-DFG info: node type, node's predecessors/successors
void output_schedule(string str);                         // print the final scheduling result
void print_schedule();
void print_reg_allocation();
void print_binding();
void printlatex();
// FDS Core Functions
void FDS(int max_depth);      // main function FDS
void getLC();                 // obtain LC

void updateAL(G_Node *ops);   // updating ALAP
void updateAS(G_Node *ops);   // updating ASAP


double computeChildForces(int node, int new_asap_time, vector< vector<double> > DG,
                          int depth, int max_depth);
double computeParentForces( int node, int new_alap_time, vector< vector<double> > DG,
                            int depth, int max_depth);


double computeSelfForce(int node, int t, vector<vector<double>> DG);

void recursiveTopologicalSort(int node, bool* visited, queue<G_Node* > *top_sort);
void topologicalSort();
int computeSharabilityParameter(double* sharability, set<int> future_elements,
                                FU* poss_FU, vector<vector<double>> DG);
double computeBindForce(int node, int t, FU* act_FU, vector<vector<double>> DG, Reg** best_reg);
void allocate( FU* best_FU, Reg* best_Reg, int node, int time, int reg_id );
void create_input_registers(int* reg_id);
void output_fu_binding(string str);
/*----------------------------------------------------------------------------*/
/*--------------------------------MAIN----------------------------------------*/
/*----------------------------------------------------------------------------*/
int main(int argc, char **argv) {

  int edge_num = 0;
  int max_depth = 0;
  GET_LIB();          //get rt[] delay info for all f-types

  if ( argc < 5 ) {
    cout << "Usage: " <<
      argv[0] << "[input_file_path] [output_file_folder] [latency_parameter] [max_depth_propagation]" << endl;
    return -1;
  }

  edge_num = 0;
  readGraphInfo(argv, DFG, &edge_num); //read DFG info

  top_order = new int[opn];

  //cout << "TOP" << endl;
  topologicalSort();
  //cout << "END TOP" << endl;

  //read DFG name without '.txt' and create output filename = 'DFG_result.txt'
  stringstream str(argv[1]);
  string tok;
  while (getline(str, tok, '.')) {
    if (tok != "txt")
      DFGname = tok;
    //cout << tok << endl;
  }

  string folder(argv[2]);
  string s = "_result.txt";
  outputfile = folder + DFGname.substr(DFGname.find_last_of("\\/") + 1,DFGname.length()) + s;


  latency_parameter = atof(argv[3]);
  max_depth = atoi(argv[4]);

  auto start = chrono::high_resolution_clock::now();
  FDS(max_depth);
  auto end = chrono::high_resolution_clock::now();

  // print on the console the results
  cout << "*********************************************" << endl;
  cout << "Current DFG: " << DFGname << endl;
  cout << "Latency contraint = " << LC << endl;
  output_schedule(outputfile);  //output function print output file
  output_fu_binding(outputfile);
  cout << "*********************************************" << endl;
  print_schedule();
  print_reg_allocation();
  print_binding();
  cout << endl;

  cout << "Time [ns]: " << chrono::duration_cast<chrono::nanoseconds>(end-start).count() << endl;
  cout << "Time [s]: " << (double)chrono::duration_cast<chrono::nanoseconds>(end-start).count()/1000000000 << endl;
  t = (double)chrono::duration_cast<chrono::nanoseconds>(end-start).count()/1000000000;
  //printlatex();
  delete[] ops;

  return 0;
}

/*----------------------------------------------------------------------------*/
/*-----------------------------FUNCTIONS--------------------------------------*/
/*----------------------------------------------------------------------------*/

void printlatex(){
  ofstream fout("latex.txt", ios::out | ios::app);  // output file to save the scheduling results

  if (fout.is_open()) {
    fout << "& " << t << " & " << area << " & " << fu_num << " & " << reg_num << " & " << mux_num << " & " << demux_num << " \\\\";
    fout << endl;
  }
  else cout << "Unable to open file to write latex";

}

void print_schedule() {

  vector<vector<int >> cc_schedule(LC+1, vector<int>(opn));
  vector<vector<int >> op_busy(LC+1, vector<int>(9));
  vector<int> max_op(9);
  int number_of_chars;

  for ( int i = 0; i < LC + 1; i++ ) {
    for ( int j = 0; j < opn; j++ ) {
      cc_schedule[i][j] = 0;
    }
  }

  for ( int i = 0; i < LC + 1; i++ ) {
    for ( int j = 0; j < 9; j++ ) {
      op_busy[i][j] = 0;
    }
  }

  for ( int i = 0; i < 9; i++ ) {
    max_op[i] = 0;
  }

  for ( int i = 0; i < opn; i++ ) {
    for ( int j = 0; j < rt[ops[i].type]; j++ ) {
      cc_schedule[ops[i].schl_time + j][i] = 1;
      op_busy[ops[i].schl_time + j][ops[i].type]++;
    }
  }

  for ( int i  = 1; i < LC + 1; i++ ) {
    cout << "CC " << i << ": ";
    number_of_chars = 4;
    for ( int j = 0; j < opn; j++ ) {
      if ( cc_schedule[i][j] != 0 ) {
        cout << j << " ";
        number_of_chars = number_of_chars + 2;
      }
    }
    for ( int i = 0; i < 50 - number_of_chars; i++ ) {
      cout << " " ;
    }
    cout << "\t| ";
    for ( int j = 0; j < 9; j++ ) {
      cout << "R" << j << ": " << op_busy[i][j] << " ";
      if ( op_busy[i][j] > max_op[j] ) {
        max_op[j] = op_busy[i][j];
      }
    }
    cout << endl;
  }

  int total_fu = 0;
  fu_area = 0;
  for ( int i = 0; i < 9; i++ ) {
    cout << "# Resource type " << i << ": " << max_op[i] << endl;
    total_fu += max_op[i];
    fu_area += max_op[i]*ct[i];
  }


  cout << "Total # FU: " << total_fu << endl;
  fu_num = total_fu;
  cout << "Total area FU: " << fu_area << endl;
  cout << endl;

}


// read library paramenters
void GET_LIB() {
  rt[0] = 1;
  rt[1] = 1;
  rt[2] = 3;
  rt[3] = 1;
  rt[4] = 1;
  rt[5] = 1;
  rt[6] = 1;
  rt[7] = 1;
  rt[8] = 8;

  ct[0] = 1368;
  ct[1] = 96;
  ct[2] = 8568;
  ct[3] = 420;
  ct[4] = 288;
  ct[5] = 1672;
  ct[6] = 96;
  ct[7] = 1476;
  ct[8] = 15048;

}

/*----------------------------------------------------------------------------*/

// read fraph info from the file
void readGraphInfo(char **argv, int DFG, int *edge_num) {

  FILE *bench;  // the input DFG file

  // open the input DFG file to count the number of operation nodes in the input DFG
  if (!(bench = fopen(argv[1], "r"))) {
    std::cerr << "Error: Reading input DFG file " << argv[1] << " failed." << endl;
    cin.get();  // waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }

  char *line = new char[100];
  opn = 0;      // initialize the number of operation node

  // read input DFG file in format .txt
  if (DFG != 16 && DFG != 17) {
    // count the number of operation nodes in the input DFG
    while (fgets(line, 100, bench)) {
       // if the line contains the keyword "label", the equation returns true,
       // otherwise it returns false. search for c/c++ function "strstr" for
       // detail "string search"
      if (strstr(line, "label") != NULL)
        opn++;
    } // end while
  } // end if DFG
  // read input DFG file from format .gdl
  else {
    while (fgets(line, 100, bench)) {
      if (strstr(line, "node") != NULL)
        opn++;
    }
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
  // open the input DFG file agian to read the DFG, so that the cursor returns back to the beginning of the file
  if (!(bench = fopen(argv[1], "r"))) {
    std::cerr << "Error: Failed to load the input DFG file." << endl;
    cin.get();  // waiting for user to press enter to terminate the program, so that the text can be read
    exit(EXIT_FAILURE);
  }
  if (DFG != 16 && DFG != 17)
    // read a line from the DFG file, store it into "line[100]
    while (fgets(line, 100, bench)) {
      // if a keyword "label" is incurred, that means a operation node is found in the input DFG file
      if ((label = strstr(line, "label")) != NULL) {
        tok = strtok(line, seps); // break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok);         // obtain the node name
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
      } // end if label
      // if a keyword "->" is incurred, that means an edge is found in the input DFG file
      else if ((label = strstr(line, "->")) != NULL) {
        tok = strtok(line, seps); // break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
        name.assign(tok);         // obtain node name u from edge (u, v)
        cname.assign(strtok(label + 3, seps));  // obtain node name v from edge (u, v)
        (ops[oplist[name]].child).push_back(&(ops[oplist[cname]]));   // use double linked list to hold the children
        (ops[oplist[cname]].parent).push_back(&(ops[oplist[name]]));  // use double linked list to hold the parents
        (*edge_num)++;
      } // end else if
    } // end while fgets
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
      } // end if
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
      } // end else
    } //end while
  } // end of reading DFG
  fclose(bench);
  delete[] line;
}

/*----------------------------------------------------------------------------*/

// print the results on the file
void output_schedule(string str) {
  // obtain filename to output
  ofstream fout_s(str, ios::out | ios::app);  // output file to save the scheduling results
  ofstream fout_s2(str + "bind", ios::out | ios::app);
  if (fout_s.is_open()) {
    fout_s << "LC " << LC << endl;
    for (int i = 0; i < opn; i++) {
      //std::cout << i << " " << ops[i].asap << endl; //after scheduling, ASAP = ALAP of each node
      //fout_s << i << " " << ops[i].schl_time  << " " << ops[i].asap << " " << ops[i].alap << " " << ops[i].type <<  endl;
      fout_s << ops[i].id << " "  << ops[i].asap << endl;
      fout_s2 << ops[i].id << " " << ops[i].asap + rt[ops[i].type] << " " << ops[i].lifetime_end  << " " << ops[i].my_reg->id << endl;
    }
    fout_s.close();
    fout_s.clear();
  }
  else cout << "Unable to open file schedule file";

}


// print the results on the file
void output_fu_binding(string str) {
  // obtain filename to output
  ofstream fout_s(str + "fubind", ios::out | ios::app);  // output file to save the scheduling results
  if (fout_s.is_open()) {
    for ( int i = 0; i < tnum; i++ ) {
      fout_s << "Type: " << i << endl;
      for (auto it = FU_list[i].begin(); it != FU_list[i].end(); it++ ) {
        //std::cout << i << " " << ops[i].asap << endl; //after scheduling, ASAP = ALAP of each node
        //fout_s << i << " " << ops[i].schl_time  << " " << ops[i].asap << " " << ops[i].alap << " " << ops[i].type <<  endl;
        fout_s << "\tFU_id " << (*it)->id << " Port0" << endl;
        for ( auto it_reg = (*it)->port0.begin(); it_reg != (*it)->port0.end(); it_reg++ ) {
          fout_s << "\t\t" << (*it_reg) << " ";
        }
        fout_s << endl;
        fout_s << "\tFU_id " << (*it)->id << " Port1" << endl;
        for ( auto it_reg = (*it)->port1.begin(); it_reg != (*it)->port1.end(); it_reg++ ) {
          fout_s << "\t\t" << (*it_reg) << " ";
        }
        fout_s << endl;
      }
    }

    cout << "-------------CHECKER FU BINDING--------------" << endl;

    for ( int i = 0; i < opn; i++ ) {
      int count;
      G_Node* p1;
      G_Node* p2;
      count = 0;
      cout << "-------------CHECKING PARENTS --------------" << endl;
      for ( auto it = ops[i].parent.begin(); it != ops[i].parent.end(); it++ ) {
        if ( count == 0 ) {
          p1 = *it;
        } else {
          p2 = *it;
        }
        count++;
      }
      if ( !( (ops[i].my_FU->port0.find(p1->my_reg->id) != ops[i].my_FU->port0.end() &&
               ops[i].my_FU->port1.find(p2->my_reg->id) != ops[i].my_FU->port1.end()) ||
              (ops[i].my_FU->port0.find(p2->my_reg->id) != ops[i].my_FU->port0.end() &&
               ops[i].my_FU->port1.find(p1->my_reg->id) != ops[i].my_FU->port1.end()) 
            ) ) {
        cout << "XXXXXXXXXXXXXXXXXX Problem parent of " << i << " XXXXXXXXXXXXXXXX" << endl;
      }
      cout << "-------------CHECKING CHILDREN --------------" << endl;
      for ( auto it = ops[i].child.begin(); it != ops[i].child.end(); it++ ) {
        if ( ops[i].my_reg->out_FU.find((*it)->my_FU) == ops[i].my_reg->out_FU.end() ) {
          cout << "XXXXXXXXXXXXXXXXXX Problem child of " << i << " XXXXXXXXXXXXXXXX" << endl;
        }
      }
    }

    fout_s.close();
    fout_s.clear();
  }
  else cout << "Unable to open file schedule file";

}




/*----------------------------------------------------------------------------*/

// Force directed scheduling function
void FDS(int max_depth) {

  double bestForce = 0.0; // best scheduling force value
  int bestNode = -1, bestT = -1, iteration = 0;    // best Node ID and T (cc), # of iteration;
  double temp = 0.0, force = 0.0; //newP/oldP for the compuataion of force
  double correction = 0.0;
  int flag = 0;

  int busy_flag;
  int res_id = 0;
  int reg_id = 0;
  double min_cost;
  Reg* actual_reg = NULL;
  FU* actual_FU = NULL;
  FU* min_FU = NULL;
  Reg* min_reg = NULL;
  FU* best_FU = NULL;
  Reg* best_reg = NULL;
  int best_FU_new;
  int min_FU_new;
  queue<FU*> considered_FU;
  int diff_occupation;
  double fu_lifetime;
  int est_lifetime;

  for ( int i = 0; i < opn; i++ ) {
    ops[i].schl = false;
    ops[i].id = i;
  }
  //find latency constraint
  //Obtain ASAP latency first
  updateAS(ops); //Obtain ASAP for each operation
  getLC(); //obtain LC
  updateAL(ops); //Obtain ALAP for each operation

  create_input_registers(&reg_id);

  // start FDS
  // initialize DG by tnum X (LC+1) note that, starts from cc = 0 to LC, but we don't do compuation in row cc = 0.
  vector< vector<double> > DG(tnum, vector<double>(LC + 1, 0)); // DG[TYPE][CC]


  while ( !flag ) { // outer loop
    //starting from second iteration, update node's ASAP/ALAP first.
    if (iteration != 0) {
      updateAS(ops);
      updateAL(ops);
    }
    for (auto i = 0; i < tnum; i++ ) {
      for ( auto j = 0; j < LC + 1; j++ ) {
        DG[i][j] = 0;
      }
    }

    // generate DG
    for (auto i = 0; i < opn; i++) {  // for each node
      // if node has asap = alap and not be scheduled, schedule it directly (only 1 available cc)
      if (ops[i].asap == ops[i].alap && !ops[i].schl) {
        ops[i].schl_time = ops[i].asap;
        ops[i].schl = true;
        for ( auto it = FU_list[ops[i].type].begin(); it != FU_list[ops[i].type].end(); it++ ) {
          busy_flag = 1;
          for (int cc = ops[i].asap; cc <= ops[i].asap + rt[ops[i].type] -1; cc++ ) {
            if ( (*it)->busy[cc] == 1 ) {
              busy_flag = 0;
            }
          }
          if ( busy_flag == 1 ) {
            considered_FU.push(*it);
          }
        }
        min_FU_new = 0;
        min_cost = -1;
        min_FU = NULL;
        min_reg = NULL;
        if ( considered_FU.empty() == true ) {
          actual_FU = new FU;
          min_FU_new = 1;
          actual_FU->type = ops[i].type;
          actual_FU->id = res_id;
          actual_FU->busy.resize(LC+2);
          actual_FU->demux_size = 0;
          for (int i = 0; i < LC + 1; i++ ) {
            actual_FU->busy[i] = 0;
          }
          min_cost = computeBindForce(i,ops[i].asap,actual_FU,DG, &actual_reg);
          //min_cost += ct[ops[i].type];
          min_FU = actual_FU;
          min_reg = actual_reg;
        } else {
          while( considered_FU.empty() == false ) {
            double cost;
            actual_FU = considered_FU.front();
            considered_FU.pop();
            cost = computeBindForce(i,ops[i].asap,actual_FU,DG, &actual_reg);
            if ( min_cost == -1 || cost < min_cost ) {
              min_cost = cost;
              min_FU = actual_FU;
              min_reg = actual_reg;
            }
          }
        }
        allocate(min_FU, min_reg, i, ops[i].alap, reg_id);
        if ( min_reg == NULL ) {
          reg_id++;
        }
        if ( min_FU_new == 1 ) {
          res_id++;
          FU_list[ops[i].type].push_back(min_FU);
        }
        ops[i].my_FU = min_FU;
      }
      temp = 1.0 / double(ops[i].alap - ops[i].asap + 1); // set temp = scheduling probability = 1/(# of event), to be fast computed.
      for (auto t = ops[i].asap; t <= ops[i].alap; t++)   // asap to alap cc range,
        for (auto d = 0; d < rt[ops[i].type]; d++)        // delay
          DG[ops[i].type][t + d] += temp;                 // compute DG
    }   // end DG generation


    bestNode = -1;
    for ( auto n = 0; n < opn; n++ ) {
      if ( ops[n].schl == false ) {  
        for (auto t = ops[n].asap; t <= ops[n].alap; t++) {  // check all cc (all event) in MR of n [asap, alap]
          force = 0.0;
          force += computeSelfForce(n, t, DG);
          force += computeChildForces(n, t, DG, 1, max_depth);
          force += computeParentForces(n, t, DG, 1, max_depth);
          if (bestNode == -1 || force < correction) {
            bestNode = n;
            correction = force;
          }
        }
      }
    }



    // start inner loop:
    // initialize best node parameter;
    bestForce = 0.0;
    bestNode = bestT = -1;
    flag = 1;
    for (auto n = 0; n < opn; n++) {    // check all unscheduled node
      if (ops[n].schl) {
        continue;
      } else {
        flag = 0;
      }
      for (auto t = ops[n].asap; t <= ops[n].alap; t++) {  // check all cc (all event) in MR of n [asap, alap]

        force = 0.0;
        force += computeSelfForce(n, t, DG);
        force += computeChildForces(n, t, DG, 1, max_depth);
        force += computeParentForces(n, t, DG, 1, max_depth);

        for ( auto it = FU_list[ops[n].type].begin(); it != FU_list[ops[n].type].end(); it++ ) {
          busy_flag = 1;
          for (int cc = t; cc <= t + rt[ops[n].type] -1; cc++ ) {
            if ( (*it)->busy[cc] == 1 ) {
              busy_flag = 0;
            }
          }
          if ( busy_flag == 1 ) {
            considered_FU.push(*it);
          }
        }



        min_FU_new = 0;
        actual_reg = NULL;
        min_cost = -1;
        min_FU = NULL;
        min_reg = NULL;
        if ( considered_FU.empty() == true ) {
          actual_FU = new FU;
          min_FU_new = 1;
          actual_FU->type = ops[n].type;
          actual_FU->id = res_id;
          actual_FU->busy.resize(LC+2);
          actual_FU->demux_size = 0;
          for (int i = 0; i < LC + 1; i++ ) {
            actual_FU->busy[i] = 0;
          }
          min_cost = computeBindForce(n,t,actual_FU,DG, &actual_reg);
          //min_cost += ct[ops[n].type];
          diff_occupation = 0;
          for (int cc = t-1; cc >= 1 && actual_FU->busy[cc] == 0; cc--) {
            diff_occupation++;
          }
          if (diff_occupation % rt[ops[n].type] != 0 ) {
            fu_lifetime = 0.25*ct[ops[n].type];
          } else {
            fu_lifetime = 1;
          }
          //min_cost = (min_cost + 1)*fu_lifetime;
          min_cost = (min_cost + 1)*fu_lifetime;
          min_FU = actual_FU;
          min_reg = actual_reg;
        } else {
          while( considered_FU.empty() == false ) {
            double cost;
            actual_FU = considered_FU.front();
            considered_FU.pop();
            cost = computeBindForce(n,t,actual_FU,DG, &actual_reg);
            diff_occupation = 0;
            for (int cc = t-1; cc >= 1 && actual_FU->busy[cc] == 0; cc--) {
              diff_occupation++;
            }
            if (diff_occupation % rt[ops[n].type] != 0 ) {
              fu_lifetime = 0.25*ct[ops[n].type];
            } else {
              fu_lifetime = 1;
            }
            //cost = (cost + 1)*fu_lifetime;
            cost = (cost + 1)*fu_lifetime;
            if ( min_cost == -1 || cost < min_cost ) {
              min_cost = cost;
              min_FU = actual_FU;
              min_reg = actual_reg;
            }
          }
        }

        est_lifetime = -1;
        for (auto it = ops[n].child.begin(); it != ops[n].child.end(); it++ ) {
          if ( (*it)->alap + rt[ops[n].type] - 1 > est_lifetime ) {
            est_lifetime = (*it)->alap + rt[ops[n].type] - 1;
          }
        }

        force = (force - correction + 0.001)*min_cost;

        if (bestT < 0 || force < bestForce ) {  // update best node, cc, force value
          bestForce = force;
          bestNode = n;
          bestT = t;
          best_FU = min_FU;
          best_FU_new = min_FU_new;
          best_reg = min_reg;
        }
      } // end for auto t
    } // end for auto n (inner loop)
    //  Terminated visit operation not scheduled
    if ( bestNode >= 0 ) {
      //count_here++;
      //cout << "S: " << bestNode << endl;
      allocate(best_FU, best_reg, bestNode, bestT, reg_id);
      if ( best_reg == NULL ) {
        reg_id++;
      }
      if ( best_FU_new == 1 ) {
        res_id++;
        FU_list[best_FU->type].push_back(best_FU);
      }
      ops[bestNode].my_FU = best_FU;
      ops[bestNode].asap = bestT;
      ops[bestNode].alap = bestT;
      ops[bestNode].schl_time = bestT;
      ops[bestNode].schl = true;
    }
    iteration++;
  } // end FDS-outer loop

  DG.clear();
}

/*----------------------------------------------------------------------------*/

void allocate( FU* best_FU, Reg* best_reg, int node, int time, int id_reg) {

  Reg* actual_reg;
  // Assumption is that each unit has only two in-ports and therefore
  // also only two parents
  G_Node* p1 = NULL;
  G_Node* p2 = NULL;
  int count;
  int a,b,c,d;
  int y0,y1,y2,y3, outcome;

  int lifetime_begin, max_lifetime_end, temporary_lifetime_end;
  int parent_lifetime_end;


  //ops[node].my_FU = best_FU;


  // --------------------- UPDATE BUSY TIME OF THE FU ----------------------------
  // Set busy the functional unit for the time needed to execute the operation
  // node.
  for ( int cc = time; cc <= time + rt[best_FU->type] - 1; cc++ ) {
    best_FU->busy[cc] = 1;
  }

  // ------------------ UPDATE INPUT CONNECTION TO THE FU ------------------------
  // Extract the 2 parents. Assumption: each operation only 2 parents since
  // each FU has only two input ports.
  count = 0;
  for ( auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++ ) {
    if ( count == 0 ) {
      p1 = (*it);
    } else {
      p2 = (*it);
    }
    count++;
  }

  // Check if parents already scheduled to create the connection.
  // Try to minimize the congestion to the input ports of the FU.
  if ( p1->schl && p2->schl ) {
    a = (best_FU->port0.find(p1->my_reg->id) != best_FU->port0.end()) ? 1 : 0;
    b = (best_FU->port1.find(p1->my_reg->id) != best_FU->port1.end()) ? 1 : 0;
    c = (best_FU->port0.find(p2->my_reg->id) != best_FU->port0.end()) ? 1 : 0;
    d = (best_FU->port1.find(p2->my_reg->id) != best_FU->port1.end()) ? 1 : 0;

    y0 = ((~b & ~c & ~d ) | (a & ~b & ~d) | (a & ~c & ~d)) & 0x01;
    y1 = ((~a & b  & ~c )) & 0x01;
    y2 = ((~a & ~b  & c & ~d ))& 0x01;
    y3 = ((~a & ~b  & ~c) | (~a & ~b & d)) & 0x01;

    outcome = 2*y3 + y2;
    if ( outcome == 1 ) {
      // Crea connession tra p1 e porta1
      best_FU->port1.insert(p1->my_reg->id);
    } else if ( outcome == 2 ) {
      // Crea connessione tra p1 e porta0
      best_FU->port0.insert(p1->my_reg->id);
    }

    outcome = 2*y1 + y0;
    if ( outcome == 1 ) {
      // Crea connession tra p2 e porta1
      best_FU->port1.insert(p2->my_reg->id);
    } else if ( outcome == 2 ) {
      // Crea connessione tra p2 e porta0
      best_FU->port0.insert(p2->my_reg->id);
    }

    p1->my_reg->out_FU.insert(best_FU);
    p2->my_reg->out_FU.insert(best_FU);

  } else {
    // Only one parent is scheduled. Than check if it is already
    // connected to one port or if the connection has to be made
    // connect it to the less congestioned port.
    if ( p1->schl == true ) {
      if (  best_FU->port0.find(p1->my_reg->id) == best_FU->port0.end() &&
            best_FU->port1.find(p1->my_reg->id) == best_FU->port1.end() ) {
        // If there is no connection
        if ( best_FU->port0.size() > best_FU->port1.size() ) {
          best_FU->port1.insert(p1->my_reg->id);
        } else {
          best_FU->port0.insert(p1->my_reg->id);
        }
      }
      p1->my_reg->out_FU.insert(best_FU);
    }
    if ( p2->schl == true ) {
      if (  best_FU->port0.find(p2->my_reg->id) == best_FU->port0.end() &&
            best_FU->port1.find(p2->my_reg->id) == best_FU->port1.end() ) {
        // If there is no connection
        if ( best_FU->port0.size() > best_FU->port1.size() ) {
          best_FU->port1.insert(p2->my_reg->id);
        } else {
          best_FU->port0.insert(p2->my_reg->id);
        }
      }
      p2->my_reg->out_FU.insert(best_FU);
    }
  }


  // ----------------------- UPDATE INTERCONNECTIONS REGISTER --------------------------
  // --------------------------- INPUT INTERCONNECTIONS --------------------------------
  // Verify/insert connection to the output register
  if ( best_reg == NULL ) {
    // This means that a new register has to be created.
    actual_reg = new Reg;
    reg_pool.push_back(actual_reg);
    //actual_reg->mux_size = 0;
    actual_reg->id = id_reg;            // Global identifier for the register.
    actual_reg->in_FU.insert(best_FU);  // The output port of the actual FU will
                                        // be connected as an input of the new register.
    //actual_reg->mux_size++;             // Actually mux size simply count number of input of reg.
    ops[node].my_reg = actual_reg;      // Set the new register as the register
                                        // of the variable associated to the operation node.
    best_FU->demux_size++;              // Increase size of demux at the output of the current FU.
    actual_reg->busy.resize(LC+2);
    for (int cc = 0; cc <= LC; cc++) {
      actual_reg->busy[cc] = 0;
    }

    best_reg = actual_reg;
    // TODO: check if the register has to be included in the output list
  } else {
    // A register already exist.
    ops[node].my_reg = best_reg;
    if ( best_reg->in_FU.find(best_FU) == best_reg->in_FU.end() ) {
      // If the register isn't connected to the output of the current FU
      best_reg->in_FU.insert(best_FU);   // Insert the output of FU as input of the register
      //best_reg->mux_size++;             // Increase number of input of the registers
      best_FU->demux_size++;             // Increase number of output of the FU
    }
    // If the register is already connected to the FU no action has to be made.

  }


  // --------------------------- OUTPUT INTERCONNECTIONS --------------------------------
  for ( auto it = ops[node].child.begin(); it != ops[node].child.end(); it++) {
    if ( (*it)->schl == true ) {
      best_reg->out_FU.insert((*it)->my_FU);
      // If the child is scheduled I have to check if exists already an interconnection
      // between my output register and the FU of the child ON THE RIGHT PORT!!!
      for ( auto it_p = (*it)->parent.begin(); it_p != (*it)->parent.end(); it_p++ ) {
        if ( (*it_p)->id != node ) {
          if ( (*it_p)->schl == true  ) {
            // If the other parent is scheduled than for sure it is connected to one of the 2 ports
            // of the common child. Find the port than connect best reg to the other port
            if (  (*it)->my_FU->port0.find((*it_p)->my_reg->id) != (*it)->my_FU->port0.end() ) {
                (*it)->my_FU->port1.insert(best_reg->id);
            } else {
                (*it)->my_FU->port0.insert(best_reg->id);
            }
          } else {
            // If it is not scheduled I choose the less congestioned port
            if ( (*it)->my_FU->port0.size() > (*it)->my_FU->port1.size() ) {
              (*it)->my_FU->port1.insert(best_reg->id);
            } else {
              (*it)->my_FU->port0.insert(best_reg->id);
            }
          }
        }
      }
    }
  }


  // -------------- UPDATE LIFETIME BEST REGISTER FOR OPERATION NODE--------------
  // The lifetime are represented as closed interval [lifetime_begin,lifetime_end]
  lifetime_begin = time + rt[ops[node].type];
  max_lifetime_end = -1;
  if ( ops[node].child.empty() == true ) {
    // If it has no children the variable should be kept until the end.
    max_lifetime_end = LC + 1;
  }
  for ( auto it = ops[node].child.begin(); it != ops[node].child.end(); it++ ) {
    temporary_lifetime_end = (*it)->alap + rt[(*it)->type] - 1;
    if ( temporary_lifetime_end > max_lifetime_end ) {
      max_lifetime_end = temporary_lifetime_end;
    }
  }

  ops[node].lifetime_end = max_lifetime_end;
  for ( int cc = lifetime_begin; cc <= max_lifetime_end; cc++ ) {
    best_reg->busy[cc] = 1;
  }


  // -------------- UPDATE LIFETIME PARENTS REGISTER -------------------------
  for (auto it_p = ops[node].parent.begin(); it_p != ops[node].parent.end(); it_p++ ) {
    if ( (*it_p)->schl == true ) {
      parent_lifetime_end = -1;
      for ( auto it = (*it_p)->child.begin(); it != (*it_p)->child.end(); it++ ) {
        if ( (*it)->id != node ) {
          temporary_lifetime_end = (*it)->alap + rt[(*it)->type] - 1;
          if ( temporary_lifetime_end > parent_lifetime_end ) {
            parent_lifetime_end = temporary_lifetime_end;
          }
        } else {
          if ( lifetime_begin - 1 > parent_lifetime_end ) {
            parent_lifetime_end = lifetime_begin - 1;
          }
        }
      }
      if ( parent_lifetime_end != (*it_p)->lifetime_end ) {
        for (int cc = (*it_p)->lifetime_end; cc > parent_lifetime_end ; cc-- ) {
          (*it_p)->my_reg->busy[cc] = 0;
        }
        (*it_p)->lifetime_end = parent_lifetime_end;
      }
    }
  }

  // --------------------- UPDATE FUTURE THINGS -------------------------------
  // Delete from future child of the parent FU (therefore for the parent node that are
  // scheduled ) the current node
  for (auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++ ) {
    if ( (*it)->id < opn ) {
      if ( (*it)->schl == true ) {
        (*it)->my_FU->future_child.erase(node);
      } else {
        // Add to the future parent of this FU the parents of node that are not scheduled
        best_FU->future_parent.insert((*it)->id);
      }
    }
  }
  // Delete from future parent of the child FU (therefore for the child node that are
  // scheduled ) the current node
  for (auto it = ops[node].child.begin(); it != ops[node].child.end(); it++ ) {
    if ( (*it)->id < opn ) {
      if ( (*it)->schl == true ) {
        (*it)->my_FU->future_parent.erase(node);
      } else {
        // Add to the future child of this FU the children of node that are not scheduled
        best_FU->future_child.insert((*it)->id);
      }
    }
  }


  return;


}



// get latency constraint
void getLC() {
  LC = 0;
  // obtain ASAP latency first
  for (auto i = 0; i < opn; i++)
    if (ops[i].child.empty())
      if (ops[i].asap + rt[ops[i].type] - 1 > LC)
        LC = ops[i].asap + rt[ops[i].type] - 1;

  LC *= latency_parameter;
}


/*----------------------------------------------------------------------------*/

// update ASAP
void updateAS(G_Node *ops) {

  int max, node;
  int possible_time;

  for ( int i = 0; i < opn; i++ ) {
    node = top_order[i];
    if (!ops[node].schl) {
      max = 1;
      for (auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++) {
        if ( (*it)->id < opn ) {
          possible_time = (*it)->asap + rt[(*it)->type];
          if ( possible_time > max ) {
            max = possible_time;
          }
        }
      }
      ops[node].asap = max;
    }
  }


}

/*----------------------------------------------------------------------------*/

// commento ALAP
void updateAL(G_Node *ops) {

  int min, node;

  for ( int i = opn - 1; i >= 0; i-- ) {
    node = top_order[i];
    if (!ops[node].schl) {
      min = LC + 1;
      for (auto it = ops[node].child.begin(); it != ops[node].child.end(); it++) {
        if ( (*it)->id < opn )  {
          if ( (*it)->alap < min ) {
            min = (*it)->alap;
          }
        }
      }
      ops[node].alap = min - rt[ops[node].type];
    }
  }

}


void topologicalSort() {

  queue<G_Node*> top_sort;
  bool* visited = new bool[opn];

  for ( int i = 0; i < opn; i++ ) {
    visited[i] = false;
  }

  for (int i = 0; i < opn; i++) {
    if ( ops[i].parent.empty() ) {
      recursiveTopologicalSort(i,visited,&top_sort);
    }
  }

  for ( int i = opn-1; i >= 0; i-- ) {
    top_order[i] = top_sort.front()->id;
    top_sort.pop();
  }

}


void recursiveTopologicalSort(int node, bool* visited, queue<G_Node* > *top_sort) {

  G_Node* current;

  if (visited[node] == true) {
    return;
  }

  visited[node] = true;
  current = &ops[node];

  for (auto it = current->child.begin(); it != current->child.end(); it++ ) {
    recursiveTopologicalSort((*it)->id, visited, top_sort);
  }

  (*top_sort).push(current);
  return;

}


double computeParentForces( int node, int new_alap_time, vector< vector<double> > DG,
                            int depth, int max_depth) {

  double oldP, newP;
  double force = 0.0;
  int new_parent_alap_time;

  for (auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++) {
    new_parent_alap_time = new_alap_time - rt[(*it)->type];
    if ((*it)->schl || new_parent_alap_time >= (*it)->alap )
      continue;

    oldP = 1/double((*it)->alap - (*it)->asap + 1); // temp is the oldP
    newP = 1/double(new_parent_alap_time - (*it)->asap + 1);

    for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++) {
      for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
        force -= oldP*(DG[(*it)->type][cc + d] - 1.0/3*oldP);
      }
    }

    for (auto cc = (*it)->asap; cc <= new_parent_alap_time; cc++) {
      for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
        force += newP*(DG[(*it)->type][cc + d] + 1.0/3*newP);
      }
    }

    if ( max_depth == -1 || depth < max_depth ) {
      force += computeParentForces((*it)->id, new_parent_alap_time, DG,
                                  depth + 1, max_depth);
    }
  }

  return force;

}


double computeChildForces(int node, int new_asap_time, vector< vector<double> > DG,
                          int depth, int max_depth) {


  double newP, oldP;
  double force = 0.0;
  int new_child_asap_time;
  // Successors: only affect the S(n) asap:
  newP = 0.0;
  oldP = 0.0;
  for (auto it = ops[node].child.begin(); it != ops[node].child.end(); it++) {
    new_child_asap_time = new_asap_time + rt[ops[node].type];
    if ((*it)->schl || new_child_asap_time <= (*it)->asap )
      continue;

    oldP = 1/double((*it)->alap - (*it)->asap + 1); //temp is the oldP
    newP = 1/double((*it)->alap - new_child_asap_time + 1);

    for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++) {
      for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
        force -= oldP*(DG[(*it)->type][cc + d] - 1.0/3*oldP);
      }
    }

    for (auto cc = new_child_asap_time; cc <= (*it)->alap; cc++) {
      for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
        force += newP*(DG[(*it)->type][cc + d] + 1.0/3*newP);
      }
    }

    if ( max_depth == -1 || depth < max_depth ) {
      force += computeChildForces((*it)->id, new_child_asap_time, DG,
                                  depth + 1, max_depth);
    }

  }

  return force;

}

                          // t is the possible schedule time
double computeSelfForce(int node, int t, vector<vector<double>> DG) {

  double force;
  double old_p;

  force = 0.0; // initialize temp force value
  old_p = 1.0 / double(ops[node].alap - ops[node].asap + 1); // old event probability = 1/temp1

  for (auto cc = ops[node].asap; cc <= ops[node].alap; cc++) {
    if (cc == t) { // @temp scheduling cc t
      for (auto d = 0; d < rt[ops[node].type]; d++) { //across multi-delay
        force += (1.0 - old_p) * (DG[ops[node].type][cc + d] + 1.0/3.0 * (1.0 - old_p));
      }
    }
    else {
      for (auto d = 0; d < rt[ops[node].type]; d++) { //across multi-delay
        force += - old_p*(DG[ops[node].type][cc + d] - 1.0/3.0 * old_p);
      }
    }
  }

  return force;

}


int update_temp_lifetime(int parent_id, int child_id, int t) {

  int old_lifetime_end, new_child_lifetime, max_lifetime_end;
  int temporary_lifetime_end;


  old_lifetime_end = ops[parent_id].lifetime_end;
  new_child_lifetime = t + rt[ops[child_id].type] - 1;
  max_lifetime_end = -1;
  for ( auto it = ops[parent_id].child.begin(); it != ops[parent_id].child.end(); it++ ) {
    if ( (*it)->id == child_id ) {
      if ( new_child_lifetime > max_lifetime_end ) {
        max_lifetime_end = new_child_lifetime;
      }
    } else {
      temporary_lifetime_end = (*it)->alap + rt[(*it)->type] - 1;
      if ( temporary_lifetime_end > max_lifetime_end ) {
        max_lifetime_end = temporary_lifetime_end;
      }
    }
  }

  if ( max_lifetime_end != ops[parent_id].lifetime_end ) {
    ops[parent_id].lifetime_end = max_lifetime_end;
    for ( int cc = old_lifetime_end; cc > max_lifetime_end; cc-- ) {
      ops[parent_id].my_reg->busy[cc] = 0;
    }
  }

  return old_lifetime_end;

}


void restore_lifetime(int parent_id, int old_lifetime_end) {

  if ( ops[parent_id].lifetime_end != old_lifetime_end ) {
    for ( int cc = old_lifetime_end; cc > ops[parent_id].lifetime_end; cc-- ) {
      ops[parent_id].my_reg->busy[cc] = 1;
    }
  }

  ops[parent_id].lifetime_end = old_lifetime_end;

  return;

}


bool compatible_register(Reg* reg, int node, int lifetime_begin ) {

  int max_lifetime_end;
  int temporary_lifetime_end;

  max_lifetime_end = -1;
  for ( auto it = ops[node].child.begin(); it != ops[node].child.end(); it++ ) {
    temporary_lifetime_end = (*it)->alap + rt[(*it)->type] - 1;
    if ( temporary_lifetime_end > max_lifetime_end ) {
      max_lifetime_end = temporary_lifetime_end;
    }
  }

  if ( ops[node].child.empty() ) {
    max_lifetime_end = LC + 1;
  }

  for ( int cc = lifetime_begin; cc <= max_lifetime_end; cc++ ) {
    if ( reg->busy[cc] == 1 ) {
      return false;
    }
  }

  return true;

}


double computeBindForce(int node, int t, FU* act_FU, vector<vector<double>> DG, Reg** best_reg) {

  set<int> future_parents;
  set<int> future_children;
  int sharability_element = 0;
  double sharability_parameter = 0.0;
  int old_end_cycle[2];
  int i = 0, flag;
  double actual_cost  = 0.0;
  double total_cost   = 0.0;
  int number_of_schld_children = 0;
  double min_cost;
  FU* child_FU;

  for (auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++) {
    if ( (*it)->schl == false ) {
      // Inserting parents operation non scheduled of node as future parents of act_FU
      future_parents.insert((*it)->id);
    } else {
      // In case some of my parents is already scheduled, update possibly the lifetime
      // of the variable since my ALAP now is changed ( temporarily ) to t.
      old_end_cycle[i] = update_temp_lifetime((*it)->id, node, t);
      i++;
    }
  }

  for ( auto it = act_FU->future_parent.begin(); it != act_FU->future_parent.end(); it++ ) {
    if ( (*it) != node ) {
      // Include also the future parents of the act_FU where trying to bind node
      future_parents.insert((*it));
    }
  }

  for (auto it = ops[node].child.begin(); it != ops[node].child.end(); it++) {
    if ( (*it)->schl == false ) {
      // Inserting children operation non scheduled of node as future children of act_FU
      future_children.insert((*it)->id);
    }
  }

  for ( auto it = act_FU->future_child.begin(); it != act_FU->future_child.end(); it++ ) {
    if ( (*it) != node ) {
      // Include also the future parents of the act_FU where trying to bind node
      future_children.insert((*it));
    }
  }


  sharability_element = 0;
  sharability_parameter = 0.0;

  for ( auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++ ) {
    FU* parent_FU;
    if ( (*it)->schl == true ) {
      // If scheduled compute the cost and the sharability parameter
      if ( (*it)->id < opn ) { 
        parent_FU  = (*it)->my_FU;
      }
      Reg* parent_reg = (*it)->my_reg;
      if ( parent_reg->out_FU.find(act_FU) == parent_reg->out_FU.end() ) {
        if ((*it)->id < opn ) {
          double sharability;
          sharability_element   += computeSharabilityParameter(&sharability, future_parents, parent_FU, DG);
          sharability_parameter += sharability;
          sharability_element   += computeSharabilityParameter(&sharability, parent_FU->future_child, act_FU, DG);
          sharability_parameter += sharability;
        } else {
          sharability_parameter++;
          sharability_element++;
        }
        actual_cost           += mux_cost;
      }
    }
  }

  if (sharability_element == 0 ) {
    total_cost += actual_cost;
  } else {
    total_cost += ((double)actual_cost*sharability_parameter)/((double) sharability_element);
  }


/*
 *              FINE PARTE RELATIVA AI PADRI INIZIO FIGLI MORE COMPLEX!!!
 *
*/



  // Compute sharability due to only future childs -> depends only on the
  // number of future operation of NODE.
  sharability_element = 0;
  sharability_parameter = 0.0;
  for ( auto it = ops[node].child.begin(); it != ops[node].child.end(); it++ ) {
    if ( (*it)->schl == true ) {
      double sharability;
      child_FU = (*it)->my_FU;
      sharability_element   += computeSharabilityParameter(&sharability, child_FU->future_parent, act_FU, DG);
      sharability_parameter += sharability;
      sharability_element   += computeSharabilityParameter(&sharability, future_children, child_FU, DG);
      sharability_parameter += sharability;
    }
  }

  *best_reg = NULL;

  flag = 1;
  min_cost = -1;
  for ( auto reg_it = reg_pool.begin(); reg_it != reg_pool.end(); reg_it++ ) {
    Reg* actual_reg = *reg_it;
    actual_cost = 0;
    double int_sharability_parameter = sharability_parameter;
    int int_sharability_element   = sharability_element;
    if ( compatible_register(actual_reg, node, t + rt[ops[node].type]) ) {
      flag = 0;
      for ( auto it = ops[node].child.begin(); it != ops[node].child.end(); it++ ) {
        if ( (*it)->schl == true ) {
          number_of_schld_children++;
          child_FU = (*it)->my_FU;
          if ( actual_reg->out_FU.find(child_FU) == actual_reg->out_FU.end() ) {
            actual_cost += demux_cost + mux_cost;
          }
        }
      }
      if (sharability_element != 0 ) { 
        actual_cost = ((double) actual_cost*sharability_parameter)/((double)sharability_element);
      }
      for ( auto it = actual_reg->out_FU.begin(); it != actual_reg->out_FU.end(); it++ ) {
        double sharability;
        int_sharability_element   += computeSharabilityParameter(&sharability, (*it)->future_parent, act_FU, DG);
        int_sharability_parameter += sharability;
        int_sharability_element   += computeSharabilityParameter(&sharability, future_children, *it , DG);
        int_sharability_parameter += sharability;
      }
      if ( actual_reg->in_FU.find(act_FU) == actual_reg->in_FU.end() ) {
        if (int_sharability_element != 0 ) {
          actual_cost +=  ( (double) demux_cost)*int_sharability_parameter/((double)int_sharability_element);
        } else {
          actual_cost += demux_cost; 
        }
      }
      if (min_cost == -1 || actual_cost < min_cost) {
        min_cost = actual_cost;
        *best_reg = actual_reg;
      }
    }
  }

  if ( flag == 1 ) {
    if ( sharability_element != 0 ) {
      actual_cost = ((double) number_of_schld_children*mux_cost + demux_cost + reg_cost)*sharability_parameter/((double)sharability_element);
    } else  {
      actual_cost = ((double) number_of_schld_children*mux_cost + demux_cost + reg_cost);
    }
  }


  total_cost += actual_cost;

  i = 0;
  for (auto it = ops[node].parent.begin(); it != ops[node].parent.end(); it++) {
    if ( (*it)->schl == true ) {
      restore_lifetime((*it)->id, old_end_cycle[i]);
      i++;
    }
  }

  return total_cost;

}


int computeSharabilityParameter(double* sharability, set<int> future_elements,
                                FU* poss_FU, vector<vector<double>> DG) {

  int cycle;
  int flag;
  double dg_sum_cycle;
  double dg_sum_mobility;


  int sharability_element = 0;
  double sharability_parameter = 0.0;

  for ( auto it = future_elements.begin(); it != future_elements.end(); it++ ) {
    int node_id = *it;
    if ( ops[node_id].type == poss_FU->type ) {
      sharability_element++;
      cycle = 0;
      dg_sum_cycle = 0;
      dg_sum_mobility = 0;
      for ( int cc = ops[node_id].asap; cc <= ops[node_id].alap; cc++ ) {
        flag = 1;
        int right_edge = cc + rt[ops[node_id].type] - 1;
        for ( int time = cc; time <= right_edge; time++ ) {
          if ( poss_FU->busy[time] == 1) {
            flag = 0;
          }
        }
        if ( flag == 1 ) {
          dg_sum_cycle += DG[poss_FU->type][cc];
          cycle += 1;
        }
        dg_sum_mobility += DG[poss_FU->type][cc];
      }
      int mobility = ops[node_id].alap - ops[node_id].asap + 1;
      if ( dg_sum_mobility != 0 ) {
        //sharability_parameter += 1(double) ((cycle)/((double)mobility)) + ((dg_sum_cycle)/(dg_sum_mobility));
        sharability_parameter += ((double) mobility - cycle)/((double)mobility) + ((dg_sum_cycle)/(dg_sum_mobility));
      } else {
        sharability_parameter += 1;
      }
    }
  }

  *sharability = sharability_parameter;
  return sharability_element;


}


void print_reg_allocation() {

  //int total = 0;

  //cout << "------- REG. INFORMATION --------" << endl;
  cout << "# of registers: " << reg_pool.size() << endl;
  reg_num = reg_pool.size();
  reg_area = reg_pool.size() * reg_cost;
  cout << "Area of registers: " << reg_area << endl;

  for ( auto it = reg_pool.begin(); it != reg_pool.end(); it++ ) {
    //cout << "REG " << (*it)->id << endl;
    for (auto j = 0; j < opn; j++) {
      if ( ops[j].my_reg->id == (*it)->id ) {
        //cout << j << endl;
      }
    }
    //cout << "----------------" << endl;
  }

  //for ( auto it = reg_pool.begin(); it != reg_pool.end(); it++ ) {
    //total += (*it)->mux_size - 1;
    //cout << "REG " << (*it)->id << " | # of input " << (*it)->mux_size;
    //cout << " | # of output " << (*it)->out_FU.size() << endl;
  //}

  //cout << "Total mux regs size: " << total << endl;
  return;

}


void print_binding() {


  int total = 0;
  int total_demux = 0;
  int total_reg_mux = 0;

  //cout << "--------------- RESOURCES OCCUPATION ---------" << endl;
  for ( int i = 0; i < tnum; i++ ) {
    //cout << "Size " << i << " " << FU_list[i].size() << endl;
    for (auto it = FU_list[i].begin(); it != FU_list[i].end(); it++ ) {
      total += (*it)->port0.size() + (*it)->port1.size() - 2;
      total_demux += (*it)->demux_size - 1;
      //cout << "Res: " << (*it)->id << " type: " << (*it)->type << endl;
      for (int j = 0; j < opn; j++) {
        if ( ops[j].my_FU->id == (*it)->id ) {
          //cout << j << endl;
        }
      }
      //cout << "-------------------" << endl;
    }

  }
  //for ( auto it = reg_pool.begin(); it != reg_pool.end(); it++ ) {
    //total_reg_mux += (*it)->mux_size;
    //cout << "REG " << (*it)->id << " | # of input " << (*it)->mux_size;
    //cout << " | # of output " << (*it)->out_FU.size() << endl;
  //}

  //cout << "Total number of mux in input to FUs: " << total << endl;
  mux_area = (total_reg_mux + total) * mux_cost;
  demux_area = total_demux * demux_cost;
  cout << endl;
  cout << "Total mux: " << total_reg_mux + total << endl;
  mux_num = total_reg_mux + total;
  cout << "Area mux: " << mux_area << endl;
  cout << "Total demux: " << total_demux << endl;
  demux_num = total_demux;
  cout << "Area demux: " << demux_area << endl;
  cout << endl;
  cout << "Total area: " << demux_area + mux_area + reg_area + fu_area << endl;
  area = demux_area + mux_area + reg_area + fu_area;


  //cout << "--------------RESOURCES PORT BINDING ---------" << endl;
  for ( int i = 0; i < tnum; i++ ) {
    for (auto it = FU_list[i].begin(); it != FU_list[i].end(); it++ ) {
      //cout << "R" << (*it)->id << ":" << endl;
      //cout << "\tPort0: ";
      for ( auto pc = (*it)->port0.begin(); pc != (*it)->port0.end(); pc++ ) {
        //cout << (*pc) << " ";
      }
      //cout << endl;

      //cout << "\tPort1: ";
      for ( auto pc = (*it)->port1.begin(); pc != (*it)->port1.end(); pc++ ) {
        //cout << (*pc) << " ";
      }
      //cout << endl;
    }
  }
}

void create_input_registers(int* reg_id) {
  
  Reg* input_reg;
  G_Node* new_ops_vec;
  G_Node* input_node;
  G_Node* output_node;
  list<G_Node*> external_nodes;
  int estimated_lifetime_end;
  int external_input_id = opn;
  

  output_node = new G_Node;
  external_nodes.push_back(output_node);
  output_node->id = external_input_id;
  external_input_id++;
  output_node->type = 0;
  output_node->schl = true;
  output_node->schl_time = LC+1;
  output_node->alap = LC+1;
  output_node->asap = LC+1;


  for ( int i = 0; i < opn; i++ ) {
    // For each node that has 0 parents create registers for the input (coming from outside)
    // and put also the registers into the pool of register in order to allow the sharing
    // of this registers.
    if ( ops[i].parent.empty() == true ) {
      for ( int j = 0; j < 2; j++ ) {
        input_node = new G_Node;
        external_nodes.push_back(input_node);
        input_node->id = external_input_id;
        external_input_id++;
        input_node->type = -1;
        input_node->schl = true;
        ops[i].parent.push_back(input_node);
        input_node->child.push_back(&ops[i]);
        input_reg = new Reg;
        input_node->my_reg = input_reg;
        input_reg->id = *reg_id;
        *reg_id = *reg_id +1;
        input_reg->busy.resize(LC+2);
        estimated_lifetime_end = ops[i].alap + rt[ops[i].type] - 1;
        input_node->lifetime_end = estimated_lifetime_end;
        for ( int time = 1; time <= estimated_lifetime_end; time++ ) {
          input_reg->busy[time] = 1;
        }
        reg_pool.push_back(input_reg);
      }
    // For each operation that has only one parent allocate a constant register and so 
    // same as before but without 
    } else if ( ops[i].parent.size() == 1 ) {
      input_node = new G_Node;
      external_nodes.push_back(input_node);
      input_node->id = external_input_id;
      external_input_id++;
      input_node->type = 0;
      input_node->schl = true;
      input_node->child.push_back(output_node);
      input_node->child.push_back(&ops[i]);
      ops[i].parent.push_back(input_node);
      input_reg = new Reg;
      input_node->my_reg = input_reg;
      input_reg->id = *reg_id;
      *reg_id = *reg_id +1;
      input_reg->busy.resize(LC+2);
      estimated_lifetime_end = LC+1;
      input_node->lifetime_end = estimated_lifetime_end;
      for ( int time = 1; time <= estimated_lifetime_end; time++ ) {
        input_reg->busy[time] = 1;
      }
    }
  }

  new_ops_vec = new G_Node[external_input_id];
  for ( int i = 0; i < opn; i++ ) {
    new_ops_vec[i].id =           ops[i].id;
    new_ops_vec[i].type =         ops[i].type;
    for ( auto it = ops[i].child.begin(); it != ops[i].child.end(); it++ ) {
      new_ops_vec[i].child.push_back(&new_ops_vec[(*it)->id]);
    }
    for ( auto it = ops[i].parent.begin(); it != ops[i].parent.end(); it++ ) {
      new_ops_vec[i].parent.push_back(&new_ops_vec[(*it)->id]);
    }
    new_ops_vec[i].alap =         ops[i].alap ;
    new_ops_vec[i].asap =         ops[i].asap ;
    new_ops_vec[i].tasap =        ops[i].tasap;
    new_ops_vec[i].talap =        ops[i].talap;
    new_ops_vec[i].schl =         ops[i].schl ;
    new_ops_vec[i].tschl =        ops[i].tschl;
    new_ops_vec[i].schl_time   =  ops[i].schl_time;    
    new_ops_vec[i].my_FU =        ops[i].my_FU; 
    new_ops_vec[i].my_reg =       ops[i].my_reg;    
    new_ops_vec[i].lifetime_end = ops[i].lifetime_end ;
  }
  //delete[] ops;

  for ( auto it = external_nodes.begin(); it != external_nodes.end(); it++ ) {
    new_ops_vec[(*it)->id].id =           (*it)->id;
    new_ops_vec[(*it)->id].type =         (*it)->type;
    for ( auto it_c = (*it)->child.begin(); it_c != (*it)->child.end(); it_c++ ) {
      new_ops_vec[(*it)->id].child.push_back(&new_ops_vec[(*it_c)->id]);
    }
    for ( auto it_p = (*it)->parent.begin(); it_p != (*it)->parent.end(); it_p++ ) {
      new_ops_vec[(*it)->id].parent.push_back(&new_ops_vec[(*it_p)->id]);
    }
    new_ops_vec[(*it)->id].alap =         (*it)->alap ;
    new_ops_vec[(*it)->id].asap =         (*it)->asap ;
    new_ops_vec[(*it)->id].tasap =        (*it)->tasap;
    new_ops_vec[(*it)->id].talap =        (*it)->talap;
    new_ops_vec[(*it)->id].schl =         (*it)->schl ;
    new_ops_vec[(*it)->id].tschl =        (*it)->tschl;
    new_ops_vec[(*it)->id].schl_time  =   (*it)->schl_time;    
    new_ops_vec[(*it)->id].my_FU =        (*it)->my_FU; 
    new_ops_vec[(*it)->id].my_reg =       (*it)->my_reg;    
    new_ops_vec[(*it)->id].lifetime_end = (*it)->lifetime_end ;
  }

  ops = new_ops_vec;

  return ;
}






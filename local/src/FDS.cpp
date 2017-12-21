/*----------------------------------------------------------------------------*/
/*------------------------------LIBRARIES-------------------------------------*/
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <map>
#include <ctime>
#include <list>
#include <vector>
#include <queue>
#include <sstream>
#include <time.h>

using namespace std;

#define DEBUG 0

/*----------------------------------------------------------------------------*/
/*--------------------------STRUCT DECLARATION--------------------------------*/
/*----------------------------------------------------------------------------*/
struct G_Node {
  int id;                       // node ID
  int type;                     // node Function-type
  list<G_Node *> child;         // successor nodes (distance = 1)
  list<G_Node *> parent;        // predecessor nodes (distance = 1)
  int asap, alap, tasap, talap; // T-asap, T-alap (in # of clock cycle), Temp-Tasap, Temp-Talap;
  bool schl, tschl;             // bool, = 1 scheduled; =0 not
  int schl_time;
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

/*----------------------------------------------------------------------------*/
/*-----------------------------PROTOTYPES-------------------------------------*/
/*----------------------------------------------------------------------------*/
void GET_LIB();                                           // hardcode for delay info (rt[])
void Read_DFG(const int DFG, char** argv);                // Read-DFG filename
void readGraphInfo(char **argv, int DFG, int *edge_num);  // Read-DFG info: node type, node's predecessors/successors
void output_schedule(string str);                         // print the final scheduling result
void print_schedule(string str);
void print_time(string str, double time);

// FDS Core Functions
void FDS(int max_depth);      // main function FDS
void getLC();                 // obtain LC

void updateAL(G_Node *ops);   // updating ALAP
void updateAS(G_Node *ops);   // updating ASAP


double computeChildForces(int node, int new_asap_time, vector< vector<double> > DG,
                          int depth, int max_depth);
double computeParentForces( int node, int new_alap_time, vector< vector<double> > DG,
                            int depth, int max_depth);


void recursiveTopologicalSort(int node, bool* visited, queue<G_Node* > *top_sort);
void topologicalSort();
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
#if DEBUG
      cout << tok << endl;
#endif
  }

  string folder(argv[2]);
  string s = "_result.txt";
  outputfile = folder + DFGname.substr(DFGname.find_last_of("\\/") + 1,DFGname.length() -1 ) + s;


  latency_parameter = atof(argv[3]);
  max_depth = atoi(argv[4]);

  timespec t;
  t.tv_sec = 0;
  t.tv_nsec = 0;
  clock_settime(CLOCK_PROCESS_CPUTIME_ID, &t);

  FDS(max_depth);

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
  //cout << "Time taken is: " << tS.tv_sec << " " << tS.tv_nsec << endl;

  // print on the console the results
  //cout << "*********************************************" << endl;
  //cout << "Current DFG: " << DFGname << endl;
  //cout << "Latency contraint = " << LC << endl;
  output_schedule(outputfile);  //output function print output file
  //cout << "*********************************************" << endl;
  string schedule_file = "sch/" + DFGname + "_sch.txt";
  print_schedule(schedule_file);
  string time_FDS = "time/" + DFGname + "_time.txt";
  print_time(time_FDS, (double)t.tv_nsec/1000000000);

  delete[] ops;

  return 0;
}

/*----------------------------------------------------------------------------*/
/*-----------------------------FUNCTIONS--------------------------------------*/
/*----------------------------------------------------------------------------*/

void print_time(string str, double time) {

  ofstream fout(str, ios::out | ios::app);  // output file to save the scheduling results

  if (fout.is_open()) {
    fout << "Elapsed time FDS: " << time << " s";
    fout << endl;
  }
  else cout << "Unable to open file to write time";
}


void print_schedule(string str) {

  ofstream fout(str, ios::out | ios::app);  // output file to save the scheduling results

  vector<vector<int >> cc_schedule(LC+1, vector<int>(opn));
  vector<vector<int >> op_busy(LC+1, vector<int>(9));
  vector<int> max_op(9);
  int number_of_chars;

  if (fout.is_open()) {

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

    for ( int i = 0; i < 0; i++ ) {
      max_op[i] = 0;
    }

    for ( int i = 0; i < opn; i++ ) {
      for ( int j = 0; j < rt[ops[i].type]; j++ ) {
        cc_schedule[ops[i].schl_time + j][i] = 1;
        op_busy[ops[i].schl_time + j][ops[i].type]++;
      }
    }

    for ( int i  = 1; i < LC + 1; i++ ) {
      fout << "CC" << i << " ";
      number_of_chars = 4;
      for ( int j = 0; j < opn; j++ ) {
        if ( cc_schedule[i][j] != 0 ) {
          fout << j << " ";
          number_of_chars = number_of_chars + 2;
        }
      }
      for ( int i = 0; i < 50 - number_of_chars; i++ ) {
        fout << " " ;
      }
      fout << "\t| ";
      for ( int j = 0; j < 9; j++ ) {
        fout << "R" << j << ": " << op_busy[i][j] << " ";
        if ( op_busy[i][j] > max_op[j] ) {
          max_op[j] = op_busy[i][j];
        }
      }
      fout << endl;
    }

    for ( int i = 0; i < 9; i++ ) {
      fout << "R" << i << " " << max_op[i] << endl;
    }

    fout.close();
    fout.clear();
  }
  else cout << "Unable to open file to write schedule";
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

  if (fout_s.is_open()) {
    fout_s << "Latency Constraint " << LC << endl;
    for (int i = 0; i < opn; i++) {
#if DEBUG
      std::cout << i << " " << ops[i].asap << endl; //after scheduling, ASAP = ALAP of each node
#endif
      fout_s << i << " " << ops[i].schl_time  << " " << ops[i].asap << " " << ops[i].alap << " " << ops[i].type <<  endl;
    }
    fout_s.close();
    fout_s.clear();
  }
  else cout << "Unable to open file to write result";

}

/*----------------------------------------------------------------------------*/

// Force directed scheduling function
void FDS(int max_depth) {

  for ( int i = 0; i < opn; i++ ) {
    ops[i].schl = false;
    ops[i].id = i;
  }
  //find latency constraint
  //Obtain ASAP latency first
  updateAS(ops); //Obtain ASAP for each operation
  getLC(); //obtain LC
  updateAL(ops); //Obtain ALAP for each operation

  // start FDS
  // initialize DG by tnum X (LC+1) note that, starts from cc = 0 to LC, but we don't do compuation in row cc = 0.
  vector< vector<double> > DG(tnum, vector<double>(LC + 1, 0)); // DG[TYPE][CC]
  double bestForce = 0.0; // best scheduling force value
  int bestNode = -1, bestT = -1, iteration = 0;    // best Node ID and T (cc), # of iteration;
  double temp = 0.0, force = 0.0; //newP/oldP for the compuataion of force
  int flag = 0;
  int count_here = 0;

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
      }
      temp = 1.0 / double(ops[i].alap - ops[i].asap + 1); // set temp = scheduling probability = 1/(# of event), to be fast computed.
      for (auto t = ops[i].asap; t <= ops[i].alap; t++)   // asap to alap cc range,
        for (auto d = 0; d < rt[ops[i].type]; d++)        // delay
          DG[ops[i].type][t + d] += temp;                 // compute DG
    }   // end DG generation

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
        force = 0.0; // initialize temp force value
        temp = 1.0 / double(ops[n].alap - ops[n].asap + 1); // old event probability = 1/temp1

        // self force: self = sum across MR { -(deltaP) * (DG + 1/3 * deltaP) };
        for (auto cc = ops[n].asap; cc <= ops[n].alap; cc++) {
          if (cc == t) { // @temp scheduling cc t
            for (auto d = 0; d < rt[ops[n].type]; d++) { //across multi-delay
              force += (1.0 - temp) * (DG[ops[n].type][cc + d] + 1.0/3.0 * (1.0 - temp));
            }
          }
          else {
            for (auto d = 0; d < rt[ops[n].type]; d++) { //across multi-delay
              force += -temp * (DG[ops[n].type][cc + d] - 1.0/3.0 * temp);
            }
          }
        }

        force += computeChildForces(n, t, DG, 1, max_depth);
        force += computeParentForces(n, t, DG, 1, max_depth);

        if (bestT < 0) {  // update best node, cc, force value
          bestForce = force;
          bestNode = n;
          bestT = t;
        }
        else if (force < bestForce) {
          bestForce = force;
          bestNode = n;
          bestT = t;
        }
      } // end for auto t
    } // end for auto n (inner loop)
    //schedule the best node
    //if (bestT < 0) //when all nodes has been scheduled, bestT = -1 (not changed) and break the while to stop the process
    //  break;
    if ( bestNode >= 0 ) {
      count_here++;
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
        possible_time = (*it)->asap + rt[(*it)->type];
        if ( possible_time > max ) {
          max = possible_time;
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
        if ( (*it)->alap < min ) {
          min = (*it)->alap;
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

  for ( int i = 0; i < opn; i++ ) {
#if DEBUG
    cout << top_order[i] << endl;
#endif
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









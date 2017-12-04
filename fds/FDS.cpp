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

using namespace std;

/*----------------------------------------------------------------------------*/
/*--------------------------STRUCT DECLARATION--------------------------------*/
/*----------------------------------------------------------------------------*/
struct G_Node
{
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
const double latency_parameter = 1.5; // latency constant parameter, change this
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
int rt[tnum];                         // delay info

/*----------------------------------------------------------------------------*/
/*-----------------------------PROTOTYPES-------------------------------------*/
/*----------------------------------------------------------------------------*/
void GET_LIB();                                           // hardcode for delay info (rt[])
void Read_DFG(const int DFG, char** argv);                // Read-DFG filename
void readGraphInfo(char **argv, int DFG, int *edge_num);  // Read-DFG info: node type, node's predecessors/successors
void output_schedule(string str);                         // print the final scheduling result

// FDS Core Functions
void FDS();                   // main function FDS
void ASAP(G_Node *ops);       // node's Tasap initialization and comptuation
int checkParent(G_Node *op);  // sub-function for T_asap
void getLC();                 // obtain LC

void ALAP(G_Node *ops);       // node's T_alap initialization and computation
int checkChild(G_Node *op);   // sub-function for Talap

void updateAL(G_Node *ops);   // updating ALAP
void updateAS(G_Node *ops);   // updating ASAP

/*----------------------------------------------------------------------------*/
/*--------------------------------MAIN----------------------------------------*/
/*----------------------------------------------------------------------------*/
int main(int argc, char **argv) {

  int edge_num = 0;
  GET_LIB();          //get rt[] delay info for all f-types

  edge_num = 0;
  readGraphInfo(argv, DFG, &edge_num); //read DFG info

  //read DFG name without '.txt' and create output filename = 'DFG_result.txt'
  stringstream str(argv[1]);
  string tok;
  while (getline(str, tok, '.')) {
    if (tok != "txt")
      DFGname = tok;
    //cout << tok << endl;
  }
  string folder = "res/";
  string s = "_result.txt";
  outputfile = folder + DFGname + s;

  FDS();

  // print on the console the results
  cout << "*********************************************" << endl;
  cout << "Current DFG: " << DFGname << endl;
  cout << "Latency contraint = " << LC << endl;
  output_schedule(outputfile);  //output function print output file
  cout << "*********************************************" << endl;

  delete[] ops;

  return 0;
}

/*----------------------------------------------------------------------------*/
/*-----------------------------FUNCTIONS--------------------------------------*/
/*----------------------------------------------------------------------------*/

// read library paramenters
void GET_LIB()
{
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
      //std::cout << i << " " << ops[i].asap << endl; //after scheduling, ASAP = ALAP of each node
      fout_s << i << " " << ops[i].schl_time  << " " << ops[i].asap << " " << ops[i].alap << " " << ops[i].type <<  endl;
    }
    fout_s.close();
    fout_s.clear();
  }
  else cout << "Unable to open file";

}

/*----------------------------------------------------------------------------*/

// Force directed scheduling function
void FDS() {
  //find latency constraint
  //Obtain ASAP latency first
  ASAP(ops); //Obtain ASAP for each operation
  getLC(); //obtain LC
  ALAP(ops); //Obtain ALAP for each operation

  // start FDS
  // initialize DG by tnum X (LC+1) note that, starts from cc = 0 to LC, but we don't do compuation in row cc = 0.
  vector< vector<double> > DG(tnum, vector<double>(LC + 1, 0)); // DG[TYPE][CC]
  double bestForce = 0.0; // best scheduling force value
  int bestNode = -1, bestT = -1, iteration = 0, temp1;    // best Node ID and T (cc), # of iteration;
  double temp = 0.0, force = 0.0, newP = 0.0, oldP = 0.0; //newP/oldP for the compuataion of force
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
      if ( ops[i].schl ) {
        ops[i].asap = ops[i].schl_time;
        ops[i].alap = ops[i].schl_time;
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
        temp1 = ops[n].alap - ops[n].asap + 1; // # of old events

        // self force: self = sum across MR { -(deltaP) * (DG + 1/3 * deltaP) };
        for (auto cc = ops[n].asap; cc <= ops[n].alap; cc++)
          if (cc == t) // @temp scheduling cc t
            for (auto d = 0; d < rt[ops[n].type]; d++) //across multi-delay
              force += (1.0 - temp) * (DG[ops[n].type][cc + d] + 1.0/3.0 * (1.0 - temp));
          else
            for (auto d = 0; d < rt[ops[n].type]; d++) //across multi-delay
              force += -temp * (DG[ops[n].type][cc + d] - 1.0/3.0 * temp);

        // p-s force:
        // Predecessors: only affect the P(n) alap:
        newP = 0.0;
        oldP = 0.0;
        for (auto it = ops[n].parent.begin(); it != ops[n].parent.end(); it++) {
          if ((*it)->schl || t >= (*it)->alap + rt[(*it)->type] )
            continue;

          oldP = 1/double((*it)->alap - (*it)->asap + 1); // temp is the oldP
          //newP = double(oldP - (temp1 - (t - ops[n].asap + 1))); // newP = oldP - [(n's oldP) - (t - n's ASAP + 1)]
          newP = 1/double( (t - rt[(*it)->type]) - (*it)->asap + 1);
          temp = 1.0 / newP - 1.0 / oldP;

          for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++) {
            for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
              force -= oldP*(DG[(*it)->type][cc + d] - 1/3*oldP);
            }
          }

          for (auto cc = (*it)->asap; cc <= (t - rt[(*it)->type]); cc++) {
            for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
              force += newP*(DG[(*it)->type][cc + d] + 1/3*newP);
            }
          }

            /*  if (cc <= t - rt[(*it)->type])
              for (auto d = 0; d < rt[(*it)->type]; d++)
                force += 0*-(DG[(*it)->type][cc + d] + temp / 3.0) * temp;
            else
              for (auto d = 0; d < rt[(*it)->type]; d++)
                force += 0*(DG[(*it)->type][cc + d] - 1.0 / 3.0 / oldP) / oldP;
          */
        } // end for auto it
        // Successors: only affect the S(n) asap:
        newP = 0.0;
        oldP = 0.0;
        for (auto it = ops[n].child.begin(); it != ops[n].child.end(); it++) {
          if ((*it)->schl || t + rt[ops[n].type] <= (*it)->asap )
            continue;

          oldP = 1/double((*it)->alap - (*it)->asap + 1); //temp is the oldP
          newP = 1/double((*it)->alap - (t + rt[ops[n].type]) +1);
          temp = 1.0 / newP - 1.0 / oldP;

          for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++) {
            for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
              force -= oldP*DG[(*it)->type][cc + d];
            }
          }

          for (auto cc = (t + rt[ops[n].type]); cc <= (*it)->alap; cc++) {
            for ( auto d = 0; d < rt[(*it)->type]; d++ ) {
              force += newP*DG[(*it)->type][cc + d];
            }
          }
          /*
          for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++)
            if (cc >= t - rt[(*it)->type])
              for (auto d = 0; d < rt[(*it)->type]; d++)
                force += 0*-(DG[(*it)->type][cc + d] + temp / 3.0) * temp;
            else
              for (auto d = 0; d < rt[(*it)->type]; d++)
                force += 0*(DG[(*it)->type][cc + d] - 1.0 / 3.0 / oldP) / oldP;
        */
        } // end for auto it
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
      cout << bestNode << " " << bestT <<  endl;
      ops[bestNode].asap = bestT;
      ops[bestNode].alap = bestT;
      ops[bestNode].schl_time = bestT;
      ops[bestNode].schl = true;
    }
    iteration++;
  } // end FDS-outer loop

  cout << "--------" << endl;
  cout << iteration << endl;
  cout << count_here << endl;
  cout << "--------" << endl;
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

// ASA_
void ASAP(G_Node *ops) {

  queue <G_Node *> q;   // queue to read/update nodes' ASAP
  for (auto i = 0; i < opn; i++) {
    ops[i].id = i;        // initialize node id
    ops[i].asap = -1;     // initialize all node's asap to -1
    ops[i].schl = false;  //all nodes are not scheduled.
    if (ops[i].parent.empty()) { // push all input nodes into q (no parent)
      ops[i].asap = 1; //input nodes have asap = 1
      q.push(&ops[i]); //push all input nodes into q.
    }
  }

  G_Node *current = new G_Node; //temp node
  int temp = 0;

  while (!q.empty()) {
    current = q.front();      // read the head of q
    if (current->asap > 0) {  // if head.asap > 0 (all visited), push all unvisited children into q.
      for (auto it = current->child.begin(); it != current->child.end(); it++) {  // check all children and see if they can obtain ASAP and push into q
        temp = checkParent(*it);
        if (temp > 0) {   // all parent are visited (has > 0 T-asap, and then, return my Asap = max Parent Asap + d
          (*it)->asap = temp; // get asap
          q.push((*it));      // push into q
        } // end if temp
      } // end auto it
      q.pop(); // pop the current (head node)
    } // end if current->asap
  } // end while

  //for (int i = 0; i < opn; i++)
    //cout << "my id: " << i << " , asap time = " << ops[i].asap << endl;
}

/*----------------------------------------------------------------------------*/

// check parent
int checkParent(G_Node *op) {

  bool test = true;
  int myAsap = -1;

  for (auto it = op->parent.begin(); it != op->parent.end() && test; it++) {
    if ((*it)->asap > 0) {
      test = true;
      if ((*it)->asap + rt[(*it)->type] > myAsap) {
        myAsap = (*it)->asap + rt[(*it)->type];   // my ASAP = parent.ASAP + delay
      }
    } // end first if
    else {
      test = false;
      myAsap = -1;
    } // end else
  } // end for auto it
  return myAsap;
}

/*----------------------------------------------------------------------------*/

// ALAP
void ALAP(G_Node *ops) {

  queue <G_Node *> q; // same as obtain ASAP:
  //push all output node into q first
  for (auto i = 0; i < opn; i++) {
    ops[i].alap = LC + 1;     // intialize > LC
    if (ops[i].child.empty()) {
      ops[i].alap = LC - rt[ops[i].type] + 1; // LC-Delay+1
      q.push(&ops[i]); // push into q.
    }
  }

  G_Node *current = new G_Node;
  int temp = 0;

  while (!q.empty()) {
    current = q.front();
    if (current->alap <= LC) {  // less than LC, the parent ALAP may be computed
      for (auto it = current->parent.begin(); it != current->parent.end(); it++) {
        temp = checkChild((*it));
        if (temp <= LC) { // my ALAP has been updated
          (*it)->alap = temp;
          q.push(*it);
        } // end if temp
      } // end for auto it
      q.pop();
    } // end if current->alap
  } // end while

}

/*----------------------------------------------------------------------------*/

// check child
int checkChild(G_Node *op) {

  bool test = true;
  int myAlap = LC + 1;
   
  for (auto it = op->child.begin(); it != op->child.end() && true; it++) {
    if ((*it)->alap <= LC) {
      if ((*it)->alap - rt[op->type] <= myAlap) {
        myAlap = (*it)->alap - rt[op->type];
      }
    }
    else {
      test = false;
      myAlap = LC + 1;
    }
  }
  return myAlap;
}

/*----------------------------------------------------------------------------*/

// update ASAP
void updateAS(G_Node *ops) {
  queue <G_Node *> q; // queue to read/update nodes' ASAP
  for (auto i = 0; i < opn; i++) {
    if (ops[i].schl) {  // skip scheduled operations and push into q.
      q.push(&ops[i]);
      continue;
    }

    ops[i].asap = -1; // initialize all unscheduled node's asap to -1

    if (ops[i].parent.empty()) {  // push all input nodes into q (no parent)
      ops[i].asap = 1; // input nodes have asap = 1
      q.push(&ops[i]); // push all input nodes into q.
    }
  } // end for auto i

  G_Node *current = new G_Node; // temp node
  int temp = 0;

  while (!q.empty()) {
    current = q.front();      // read the head of q
    q.pop();
    for (auto it = current->child.begin(); it != current->child.end(); it++) {
      if ( (*it)->schl == false && (*it)->asap == -1 ) {
        temp = checkParent(*it);
        if (temp > 0) { // all parent are visited (has > 0 T-asap, and then, return my Asap = max Parent Asap + d
          (*it)->asap = temp; //get asap
          q.push((*it)); //push into q
        } // end if temp
      }
    } // end for auto it
  } // end while

  //for (int i = 0; i < opn; i++)
  //cout << "my id: " << i << " , asap time = " << ops[i].asap << endl;

}

/*----------------------------------------------------------------------------*/

// commento ALAP
void updateAL(G_Node *ops) {
  queue <G_Node *> q; // same as obtain ASAP:
  // push all output node into q first
  for (auto i = 0; i < opn; i++) {
    if (ops[i].schl) {  // skip scheduled operations and push into q.
      q.push(&ops[i]);
      continue;
    }

    ops[i].alap = LC + 1; //intialize > LC

    if (ops[i].child.empty()) {
      ops[i].alap = LC - rt[ops[i].type] + 1; // LC-Delay+1
      q.push(&ops[i]); // push into q.
    }
  } // end for auto i

  G_Node *current = new G_Node;
  int temp = 0;

  while (!q.empty()) {
    current = q.front();
    q.pop();
    for (auto it = current->parent.begin(); it != current->parent.end(); it++) {
      if ( (*it)->schl == false && (*it)->alap == LC+1 ) {
        temp = checkChild((*it));
        if (temp <= LC) { // my ALAP has been updated
          (*it)->alap = temp;
          q.push(*it);
        } // end if temp
      } // end for auto it
    }
  } // end while

}

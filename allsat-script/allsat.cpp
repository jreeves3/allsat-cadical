#include "../src/cadical.hpp"

#include <iostream>
#include <fstream>
#include <chrono>

// Must be placed in cadical directory so that 
// the include statement in line 1 works.
// This will also ensure that the makefile
// compiles the code correctly.


using namespace std;

// Greedily check if any of the problem variables do not need to be used in the solution.
// Not sure if this will be useful, ignore for now.
int minimize_solution (CaDiCaL::Solver *solver,int dataVars,int nvars, vector<vector<int>> & clauses, vector<int> & used, bool printSolutions) {
  int nUnused = 0;
  fill(used.begin(), used.begin() + dataVars + 1, 0);
  if (dataVars > 0) {
    if (used[dataVars + 1] == 0) {
      cout << "e ERROR: wrong data vars" << endl;
      exit (1);
    }
  }

  vector <int> clause;

  for (auto literals: clauses) {
    bool satisfied = false;

    // check if already used lit or auxiliary var
    // satisfies the clause
    for (auto lit: literals) {
      if (used[abs(lit)] && solver->val(lit) > 0) {
        satisfied = true;
        break;
      }
    }

    // used a previously used literal
    if (satisfied) {
      continue;
    }

    // using a new literal
    for (auto lit: literals) {
      if (solver->val(lit) > 0) {
        used[abs(lit)] = 1;
        clause.push_back (-lit);
        break;
      }
    }
  }

  if (printSolutions) {
    cout << "v ";
    for (int i = 1; i <= dataVars; i++) {
      if (used[i]) {
        cout << solver->val (i) << " ";
      }
    }
  }

  for (int lit : clause)
    solver->add (lit);
  solver->add (0);

  return 1 << nUnused;
}

// Print out solution if printSolutions is set to true.
// Add a clause to the solver that negates the current solution,
// up to the dataVars.
int noMinimize (CaDiCaL::Solver * solver, int dataVars, int nvars, bool printSolutions) {

  vector<int> clause;
  if (printSolutions) {
    cout << "v ";
    for (int i = 1; i <= dataVars; i++) {
      cout << solver->val (i) << " ";
    }
    cout << endl;
  }

  for (int i = 1; i <= dataVars; i++) {
    clause.push_back (-solver->val (i));
  }

  for (auto lit : clause)
    solver->add (lit);
  solver->add (0);

  return 1;
}

int main (int argc, char *argv[]) {

  // Start the timing
  auto start = std::chrono::high_resolution_clock::now();

  bool minimizeSolution = false;


  // ------------------------------------------------------------------
  // Read inputs
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <inputfile> [--dataVars=<Var>] [--printSolutions]" << std::endl;
    return 1;
  }
  string inputfile = argv[1];
  int dataVars = 0;
  bool printSolutions = false;

  if (argc > 2) {
    for (int i = 2; i < argc; i++) {
      string arg = argv[i];
      if (arg == "--printSolutions")
        printSolutions = true;
      else if (arg.rfind("--dataVars=",0) == 0) {
        dataVars = atoi(arg.substr(11).c_str());
      }
    }
  }

  CaDiCaL::Solver *solver = new CaDiCaL::Solver;

  // ------------------------------------------------------------------
  // Read CNF from file
  ifstream infile(inputfile);
  if (!infile) {
    cerr << "e Error: Could not open file " << inputfile << endl;
    return 1;
  }
  int lit;
  int nvars = 0;
  int nclauses = 0;
  string s;
  // parse header
  while (infile >> s) {
    if (s == "c") { // parse a comment line
      getline(infile, s);
      continue;
    }
    if (s == "p") { // parse the headr "p cnf <nvars> <nclauses>"
      infile >> s;
      if (s != "cnf") {
        cerr << "Error: Invalid CNF file format" << endl;
        return 1;
      }
      infile >> nvars >> nclauses;
      break;
    }
  }

  if (dataVars == 0) {
    dataVars = nvars;
  }

  vector<vector<int>> clauses;
  vector<int> clause;
  while (infile >> s) {
    if (s == "c") { // parse a comment line
      getline(infile, s);
      continue;
    }
    lit = stoi(s); // parse a literal; '0' for end of a clause
    solver->add(lit);
    if (minimizeSolution) {
      if (lit == 0) {
        clauses.push_back(clause);
        clause.clear();
      } else {
        clause.push_back(lit);
      }
    }
  }

  // ------------------------------------------------------------------
  // Start the solving process
  int res = 10;
  int nSolutions = 0;
  vector<int> used;
  used.resize(nvars + 1, 1);
  while (res == 10) {
    res = solver->solve (); // Solve instance.
    if (res == 10) {
      int newSolutions;
      if (minimizeSolution)
        newSolutions = minimize_solution(solver, dataVars, nvars, clauses, used, printSolutions);
      else
        newSolutions = noMinimize(solver, dataVars, nvars, printSolutions);
      nSolutions += newSolutions;
      cout << "c Found " << newSolutions << " new solution(s) " << endl;
      cout << "c New total: " << nSolutions << endl; 
    }
  }
  cout << "s " << nSolutions << " SOLUTIONS" << endl;

  delete solver;

  // End the timing
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;
  cout << "c Time taken: " << duration.count() << " seconds" << endl;


  return 0;
}

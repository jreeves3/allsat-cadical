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
int minimize_solution (CaDiCaL::Solver *solver,int dataVars,int nvars, vector<vector<int>> & clauses, vector<int> & used, bool printSolutions, int block_lits) {
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

  for (int lit : clause) {
    if (block_lits == -1 && lit > 0) {
      continue;
    }
    if (block_lits == 1 && lit < 0) {
      continue;
    }
    solver->add (lit);
  }
  solver->add (0);

  return 1 << nUnused;
}

// Print out solution if printSolutions is set to true.
// Add a clause to the solver that negates the current solution,
// up to the dataVars.
int noMinimize (CaDiCaL::Solver * solver, vector<bool> & is_data, int max_datavars, int nvars, bool printSolutions, int block_lits, vector<bool> & used_vars) {

  // cout << "Remove Lits: " << block_lits << endl;

  vector<int> clause;
  if (printSolutions) {
    cout << "v ";
    for (int i = 1; i <= max_datavars; i++) {
      if (!used_vars[i]) {
        continue;
      }
      if (is_data[i])
        cout << solver->val (i) << " ";
    }
    cout << endl;
  }

  for (int i = 1; i <= max_datavars; i++) {
    if (!used_vars[i]) {
        continue;
      }
    if (is_data[i])
      clause.push_back (-solver->val (i));
  }

  if (printSolutions) cout << "c Blocked : ";
  for (auto lit : clause) {
    if (block_lits == -1 && lit < 0) {
      continue;
    }
    if (block_lits == 1 && lit > 0) {
      continue;
    }
    if (printSolutions) cout << lit << " ";
    solver->add (lit);
  }
  solver->add (0);
  if (printSolutions) cout << "0"; 
  if (printSolutions) cout << endl;
  return 1;
}

int main (int argc, char *argv[]) {

  // Start the timing
  auto start = std::chrono::high_resolution_clock::now();

  // no longer supported with new data vars as list, but could be added if needed
  bool minimizeSolution = false;

  int block_lits = 0;
  vector<bool> used_vars;


  // ------------------------------------------------------------------
  // Read inputs
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <inputfile> [--datavars=<Var>] [--printsolutions] [--blocklits=<-1/1>] [--cadical=<OptionName>=<OptionValue>]" << std::endl;
    return 1;
  }
  string inputfile = argv[1];
  int dataVars = 0;
  bool printSolutions = false;

  CaDiCaL::Solver *solver = new CaDiCaL::Solver;

  if (argc > 2) {
    for (int i = 2; i < argc; i++) {
      string arg = argv[i];
      // cout << "Processing argument: " << arg << endl;
      if (arg == "--printsolutions")
        printSolutions = true;
      else if (arg.rfind("--datavars=",0) == 0) {
        dataVars = atoi(arg.substr(11).c_str());
        if (dataVars < 0) {
          cerr << "e Error: Invalid value for datavars. Must be non-negative." << endl;
          return 1;
        }
      }
      else if (arg.rfind("--blocklits=",0) == 0) {
        block_lits = atoi(arg.substr(12).c_str());
        // cout << "Remove Lits set to " << block_lits << endl;
        if (block_lits < -1 || block_lits > 1) {
          cerr << "e Error: Invalid value for blocklits. Must be -1, 0, or 1." << endl;
          return 1;
        }
      }
      else if (arg.rfind("--cadical=",0) == 0) {
        string option = arg.substr(10);
        size_t eq_pos = option.find('=');
        if (eq_pos != string::npos) {
          string option_name = option.substr(0, eq_pos);
          string option_value = option.substr(eq_pos + 1);
          // parse option value as int
          int option_value_int;
          try {
            option_value_int = stoi(option_value);
            bool success = solver->set(option_name.c_str(), option_value_int);
            if (!success) {
              cerr << "e Error: Unknown cadical option or value " << option_name << " " << option_value << endl;
              return 1;
            }
            continue;
          } catch (const std::invalid_argument& ia) {
            cout << "e Error: Invalid cadical option value " << option_name << " " << option_value << " Must be an integer." << endl;
            return 1;
          }
        } else {
          cerr << "e Error: Invalid cadical option format. Use --cadical=<OptionName>=<OptionValue>" << endl;
          return 1;
        }
      }
      else {
        cerr << "e Error: Unknown argument " << arg << endl;
        return 1;
      }
    }
  }

  

  // ------------------------------------------------------------------
  // Read CNF from file

  // optional line s for data vars
  vector<int> dataVarsList;
  bool dataVarsListSet = false;

  ifstream infile(inputfile);
  if (!infile) {
    cerr << "e Error: Could not open file " << inputfile << endl;
    return 1;
  }
  int lit;
  int nvars = 0;
  int nclauses = 0;
  string s;
  // parse data vars
  while (infile >> s) {
    if (s == "c") { // parse a comment line
      getline(infile, s);
      continue;
    }
    if (s == "s") {
      // parse list of data vars "s <dataVars> 0"
      while (infile >> s) {
        if (s == "0") {
          break;
        }
        dataVarsList.push_back(abs(stoi(s)));
      }
      dataVarsListSet = true;
    }
    if (s == "p") break;
  }

  // parse header
  do {
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
  } while (infile >> s);

  if (nvars <= 0) {
    cerr << "e Error: Invalid number of variables" << endl;
    return 1;
  }

  if (dataVars > 0 && dataVarsListSet) {
    cerr << "e Error: Cannot set data vars in both header and command line" << endl;
    return 1;
  }

  // set used to all 0 
  used_vars.resize(nvars + 1, 0);

  vector<bool> is_data;
  int max_dataVars = 0;
  is_data.resize(nvars + 1, false);
  if (dataVarsListSet) {
    for (int i = 0; i < dataVarsList.size(); i++) {
      is_data[dataVarsList[i]] = true;
      if (dataVarsList[i] > max_dataVars) {
        max_dataVars = dataVarsList[i];
      }
    }
  } else if (dataVars > 0) {
    for (int i = 1; i <= dataVars; i++) {
      is_data[i] = true;
    }
    max_dataVars = dataVars;
  } else {
    // all are data vars
    for (int i = 1; i <= nvars; i++) {
      is_data[i] = true;
    }
    max_dataVars = nvars;
  }

  // shrink is_data to size of max_dataVars
  is_data.resize(max_dataVars + 1, false);

  // if (dataVars == 0 && !dataVarsListSet) {
  //   dataVars = nvars;
  // }

  // print out the data vars
  // cout << "c data vars: " ;
  // for (int i = 1; i <= nvars; i++) {
  //   if (is_data[i]) {
  //     cout << " " << i ;
  //   }
  // } cout << endl;

  vector<vector<int>> clauses;
  vector<int> clause;
  while (infile >> s) {
    if (s == "c") { // parse a comment line
      getline(infile, s);
      continue;
    }
    lit = stoi(s); // parse a literal; '0' for end of a clause
    solver->add(lit);
    if (lit != 0)
      used_vars[abs(lit)] = true;
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
        newSolutions = minimize_solution(solver, dataVars, nvars, clauses, used, printSolutions, block_lits);
      else
        newSolutions = noMinimize(solver, is_data, max_dataVars, nvars, printSolutions, block_lits, used_vars);
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

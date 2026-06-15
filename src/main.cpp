#include <bits/stdc++.h>
using namespace std;
int main() {
  // Flush after every std::cout / std:cerr
  cout << std::unitbuf;
  cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true){

    cout << "$ ";
    string command;
    cin>>command;
    
    if(command == "exit") break;
    cout<< command << ": command not found" <<endl;
  }
}

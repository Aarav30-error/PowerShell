#include <bits/stdc++.h>
using namespace std;
int main() {
  // Flush after every std::cout / std:cerr
  cout << std::unitbuf;
  cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true){

    cout << "$ ";
    string input;
    getline(cin, input);
        if(input == "exit") break;
        else if (input.substr(0, 5) == "echo ") {
        cout << input.substr(5) <<endl;
        } else {
        cout << input << ": command not found" <<endl;
        }

  }


  }


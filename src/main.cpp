#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
using namespace std;

void typeCommand(const string& input,
                 const vector<string>& builtins) {
    string cmd = input.substr(5);

    // Check builtins
    for (const string& builtin : builtins) {
        if (builtin == cmd) {
            cout << cmd << " is a shell builtin" << '\n';
            return;
        }
    }

    // Check PATH
    char* path_env = getenv("PATH");
    if (path_env == nullptr) {
        cout << cmd << ": not found" << '\n';
        return;
    }

    string path(path_env);
    istringstream ss(path);

    string directory;
    while (getline(ss, directory, ':')) {
        string full_path = directory + "/" + cmd;

        if (access(full_path.c_str(), X_OK) == 0) {
            cout << cmd << " is " << full_path << '\n';
            return;
        }
    }

    cout << cmd << ": not found" << '\n';
}

int main() {
    cout << unitbuf;
    cerr << unitbuf;

    vector<string> builtins = {"exit", "echo", "type"};

    while (true) {
        cout << "$ ";

        string input;
        if (!getline(cin, input))
            break;

        if (input == "exit") {
            break;
        }
        else if (input.rfind("echo ", 0) == 0) {
            cout << input.substr(5) << '\n';
        }
        else if (input.rfind("type ", 0) == 0) {
            typeCommand(input, builtins);
        }
        else {
          vector<string> tokens;

          istringstream iss(input);

          string token;

          while(iss >> token){
            tokens.push_back(token);
          }

          if(tokens.empty()){
            continue;
          }


          vector<char*> argv;

          for(auto &s : tokens){
            argv.push_back(const_cast<char*>(s.c_str()));
          }

          argv.push_back(nullptr);

          pid_t pid = fork();

        
          
            if (pid == 0) {

                // Child process
                execvp(argv[0], argv.data());

                // Only runs if execvp fails
                cout << input << ": command not found" << '\n';
                exit(1);
            }
            else {

                // Parent process
                int status;
                waitpid(pid, &status, 0);
            }
            
        }
    }

    return 0;
}
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
vector<string> tokenize(const string& input) {
    vector<string> tokens;
    string current;

    enum State {
        NORMAL,
        SINGLE,
        DOUBLE
    };

    State state = NORMAL;

    for (int i = 0; i < input.size(); i++) {

        char ch = input[i];

        if (state == NORMAL) {

            if (ch == '\'') {
                state = SINGLE;
            }

            else if (ch == '"') {
                state = DOUBLE;
            }

            else if (ch == '\\') {

                if (i + 1 < input.size()) {
                    current += input[++i];
                }
            }

            else if (ch == ' ') {

                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            }

            else {
                current += ch;
            }
        }

        else if (state == SINGLE) {

            if (ch == '\'') {
                state = NORMAL;
            }

            else {
                current += ch;
            }
        }

        else if (state == DOUBLE) {

            if (ch == '"') {
                state = NORMAL;
            }

            else if (ch == '\\') {

                if (i + 1 < input.size()) {

                    char next = input[i + 1];

                    if (next == '"' ||
                        next == '\\' ||
                        next == '$') {

                        current += next;
                        i++;
                    }
                    else {
                        current += ch;
                    }
                }
                else {
                    current += ch;
                }
            }

            else {
                current += ch;
            }
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}
int main() {
    cout << unitbuf;
    cerr << unitbuf;

    vector<string> builtins = {"exit", "echo", "type" , "pwd" , "cd"};

    while (true) {
        cout << "$ ";

        string input;
        if (!getline(cin, input))
            break;  

        if (input == "exit") {
            break;
        }
        else if (input.rfind("echo ", 0) == 0) {
            vector<string> tokens;
            tokens = tokenize(input);

            for(int i = 1 ; i<tokens.size() ; i++){
                cout<<tokens[i]<<" ";
            }

            cout<<"\n";
        }
        else if (input.rfind("type ", 0) == 0) {
            typeCommand(input, builtins);
        }else if(input == "pwd"){
            char cwd[4096];
            getcwd(cwd, sizeof(cwd));
            cout << cwd << '\n';
        }
        else if (input.rfind("cd ", 0) == 0) {

        istringstream ss(input);

        vector<string> temp;

        string t;

         while (ss >> t) {
            if (t == "cd")
            continue;

            temp.push_back(t);
         }

        if (temp.empty())
        continue;

         if (temp[0] == "~") {

        char* target_dir = getenv("HOME");

        if (chdir(target_dir) == -1) {
            cout << "cd: " << temp[0]
                 << ": No such file or directory" << '\n';
        }

        } else {

            if (chdir(temp[0].c_str()) == -1) {
            cout << "cd: " << temp[0]
                 << ": No such file or directory" << '\n';
            }
        }
        }   
        else {
          vector<string> tokens;

        //   istringstream iss(input);

        //   string token;

        //   while(iss >> token){
        //     tokens.push_back(token);
        //   }

        tokens = tokenize(input);

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
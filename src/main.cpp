#include <bits/stdc++.h>
#include <unistd.h>

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
            cout << input << ": command not found" << '\n';
        }
    }

    return 0;
}
// ===========================================================================
// Simple Shell Implementation
// Supports: echo, type, pwd, cd, exit, and external commands
// Supports: stdout redirection via > or 1>
// ===========================================================================

#include <bits/stdc++.h>
#include <unistd.h>   // getcwd, chdir, access, execvp, fork
#include <sys/wait.h> // waitpid
#include <fcntl.h>    // open, O_WRONLY, O_CREAT, O_TRUNC
using namespace std;

// ---------------------------------------------------------------------------
// typeCommand
// Handles the "type" builtin: tells the user whether a command is a
// shell builtin or an executable found on PATH, or reports "not found".
// ---------------------------------------------------------------------------
void typeCommand(const string& input, const vector<string>& builtins) {

    // Strip the leading "type " prefix to get the command name
    string cmd = input.substr(5);

    // Check if the command is a builtin
    for (const string& builtin : builtins) {
        if (builtin == cmd) {
            cout << cmd << " is a shell builtin" << '\n';
            return;
        }
    }

    // Search for the command across all directories in PATH
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

        // access(X_OK) checks whether the file exists and is executable
        if (access(full_path.c_str(), X_OK) == 0) {
            cout << cmd << " is " << full_path << '\n';
            return;
        }
    }

    cout << cmd << ": not found" << '\n';
}

// ---------------------------------------------------------------------------
// tokenize
// Splits a raw input string into tokens, respecting:
//   - Single quotes  : no escape processing inside
//   - Double quotes  : backslash escapes \", \\, \$ only
//   - Backslash      : escapes the next character in NORMAL state
//   - Spaces         : delimiter between tokens in NORMAL state
//   - >  and  1>     : treated as standalone redirect tokens   --- stdout fd - 1
//   - >  and 2>      : treated as error redirection tokens -- stderr - fd - 2
// ---------------------------------------------------------------------------
vector<string> tokenize(const string& input) {

    vector<string> tokens;
    string current;         // Token being built up character by character

    enum State { NORMAL, SINGLE, DOUBLE };
    State state = NORMAL;

    for (int i = 0; i < (int)input.size(); i++) {

        char ch = input[i];

        // ---- NORMAL state ------------------------------------------------
        if (state == NORMAL) {

            if (ch == '\'') {
                // Enter single-quote mode
                state = SINGLE;
            }
            else if (ch == '"') {
                // Enter double-quote mode
                state = DOUBLE;
            }
            else if (ch == '\\') {
                // Backslash: take the next character literally
                if (i + 1 < (int)input.size()) {
                    current += input[++i];
                }
            }
                else if (ch == '>') {

                        if (!current.empty()) {

                            if (current == "1") {

                                if (i + 1 < input.size() && input[i + 1] == '>') {
                                    tokens.push_back("1>>");   // apend check stdout
                                    i++;
                                    current.clear();
                                    continue;
                                }

                                tokens.push_back("1>");      //redirect check
                                current.clear();
                                continue;
                            }

                            else if (current == "2") {

                                if (i + 1 < input.size() && input[i + 1] == '>') {
                                    tokens.push_back("2>>");  //apend check stderr
                                    i++;
                                    current.clear();
                                    continue;
                                }

                                tokens.push_back("2>");  
                                current.clear();
                                continue;
                            }

                            tokens.push_back(current);
                            current.clear();
                        }

                        if (i + 1 < input.size() && input[i + 1] == '>') {
                            tokens.push_back(">>");
                            i++;
                        } else {
                            tokens.push_back(">");
                        }
                }
            else if (ch == ' ') {
                // Space: delimiter — push accumulated token if non-empty
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            }
            else {
                current += ch;
            }
        }

        // ---- SINGLE-QUOTE state ------------------------------------------
        else if (state == SINGLE) {

            if (ch == '\'') {
                // Closing single quote — return to NORMAL
                state = NORMAL;
            }
            else {
                // Everything inside single quotes is literal
                current += ch;
            }
        }

        // ---- DOUBLE-QUOTE state ------------------------------------------
        else if (state == DOUBLE) {

            if (ch == '"') {
                // Closing double quote — return to NORMAL
                state = NORMAL;
            }
            else if (ch == '\\') {
                // Only \", \\, and \$ are escape sequences inside ""
                if (i + 1 < (int)input.size()) {
                    char next = input[i + 1];
                    if (next == '"' || next == '\\' || next == '$') {
                        current += next;
                        i++;    // Skip the escaped character
                    }
                    else {
                        // Not a recognised escape — keep the backslash
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

    // Push any remaining accumulated characters as the last token
    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// main — REPL (Read-Eval-Print Loop)
// ---------------------------------------------------------------------------
int main() {

    // Flush stdout and stderr after every write so the prompt is always visible
    cout << unitbuf;
    cerr << unitbuf;

    // List of commands handled directly by the shell (not forked)
    vector<string> builtins = { "exit", "echo", "type", "pwd", "cd" };

    while (true) {

        // Print prompt
        cout << "$ ";

        // Read a full line of input
        string input;
        if (!getline(cin, input))
            break;  // EOF (Ctrl-D) — exit the shell

        // ----------------------------------------------------------------
        // Built-in: exit
        // ----------------------------------------------------------------
        if (input == "exit") {
            break;
        }

        // ----------------------------------------------------------------
        // Built-in: echo
        // Supports output redirection:  echo hello > file.txt
        //                               echo hello 1> file.txt
        // ----------------------------------------------------------------
       else if (input.rfind("echo ", 0) == 0) {

            bool redirect_stdout = false;
            bool redirect_stderr = false;
            bool append_stdout = false;
            bool append_stderr = false;
            vector<string> tokens = tokenize(input);

            string outfile;
            string errfile;

            vector<string> text_tokens;

            // Parse echo arguments and redirection
                for (int i = 1; i < (int)tokens.size(); i++) {

                    if (tokens[i] == ">" || tokens[i] == "1>") {

                        redirect_stdout = true;

                        if (i + 1 < (int)tokens.size()) {
                            outfile = tokens[i + 1];
                        }

                        break;
                    }

                    else if (tokens[i] == ">>" || tokens[i] == "1>>") {

                        append_stdout = true;

                        if (i + 1 < (int)tokens.size()) {
                            outfile = tokens[i + 1];
                        }

                        break;
                    }

                    else if (tokens[i] == "2>") {

                        redirect_stderr = true;

                        if (i + 1 < (int)tokens.size()) {
                            errfile = tokens[i + 1];
                        }

                        break;
                    }

                    else if (tokens[i] == "2>>") {

                        append_stderr = true;

                        if (i + 1 < (int)tokens.size()) {
                            errfile = tokens[i + 1];
                        }

                        break;
                    }

                    text_tokens.push_back(tokens[i]);
                }

            // stdout redirection
            if (redirect_stdout) {

                int fd = open(
                    outfile.c_str(),
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0644
                );

                if (fd != -1) {

                    string out;

                    for (int i = 0; i < (int)text_tokens.size(); i++) {

                        out += text_tokens[i];

                        if (i + 1 < (int)text_tokens.size()) {
                            out += " ";
                        }
                    }

                    out += '\n';

                    write(fd, out.c_str(), out.size());

                    close(fd);
                }
            }

            // stderr redirection
            else if (redirect_stderr) {

                // echo produces no stderr
                // just create/truncate the file

                int fd = open(
                    errfile.c_str(),
                    O_WRONLY | O_CREAT | O_TRUNC,  // O_TRUNC erases the previous contents and rewrites the file
                    0644
                );

                if (fd != -1) {
                    close(fd);
                }

                // print normal output to terminal

                for (int i = 0; i < (int)text_tokens.size(); i++) {

                    cout << text_tokens[i];

                    if (i + 1 < (int)text_tokens.size()) {
                        cout << " ";
                    }
                }

                cout << '\n';
            }
            // append check stdout
            else if(append_stdout){
                int fd = open(
                    outfile.c_str(),
                    O_WRONLY | O_CREAT | O_APPEND,     // O_APPEND  does not rewrite the file but appends to it 
                    0644
                );

                if (fd != -1) {

                    string out;

                    for (int i = 0; i < (int)text_tokens.size(); i++) {

                        out += text_tokens[i];

                        if (i + 1 < (int)text_tokens.size()) {
                            out += " ";
                        }
                    }

                    out += '\n';

                    write(fd, out.c_str(), out.size());

                    close(fd);
                }

            }
            // append check stderr
            else if(append_stderr){
                    
                // echo produces no stderr
                // just create/truncate the file

                int fd = open(
                    errfile.c_str(),
                    O_WRONLY | O_CREAT | O_APPEND,  // O_TRUNC erases the previous contents and rewrites the file
                    0644
                );

                if (fd != -1) {
                    close(fd);
                }

                // print normal output to terminal

                for (int i = 0; i < (int)text_tokens.size(); i++) {

                    cout << text_tokens[i];

                    if (i + 1 < (int)text_tokens.size()) {
                        cout << " ";
                    }
                }

                cout << '\n';
            }   

            // normal echo
            else {

                for (int i = 0; i < (int)text_tokens.size(); i++) {

                    cout << text_tokens[i];

                    if (i + 1 < (int)text_tokens.size()) {
                        cout << " ";
                    }
                }

                cout << '\n';
            }
        }

        // ----------------------------------------------------------------
        // Built-in: type
        // Tells the user what kind of command something is.
        // ----------------------------------------------------------------
        else if (input.rfind("type ", 0) == 0) {
            typeCommand(input, builtins);
        }

        // ----------------------------------------------------------------
        // Built-in: pwd
        // Prints the current working directory.
        // ----------------------------------------------------------------
        else if (input == "pwd") {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                cout << cwd << '\n';
            }
            else {
                perror("pwd");
            }
        }

        // ----------------------------------------------------------------
        // Built-in: cd
        // Changes the current working directory.
        // "~" is resolved to the HOME environment variable.
        // ----------------------------------------------------------------
        else if (input.rfind("cd ", 0) == 0) {

            // Parse out the target path (everything after "cd ")
            istringstream ss(input);
            vector<string> parts;
            string t;

            while (ss >> t) {
                if (t == "cd") continue;   // Skip the command name itself
                parts.push_back(t);
            }

            if (parts.empty()) continue;  // "cd" with no argument — ignore

            string target = parts[0];

            if (target == "~") {
                // Expand ~ to the HOME directory
                char* home = getenv("HOME");
                if (home == nullptr || chdir(home) == -1) {
                    cout << "cd: ~: No such file or directory\n";
                }
            }
            else {
                if (chdir(target.c_str()) == -1) {
                    cout << "cd: " << target << ": No such file or directory\n";
                }
            }
        }

        // ----------------------------------------------------------------
        // External commands  (cat, ls, grep, etc.)
        // Tokenises the input, separates redirection, then fork+exec.
        // ----------------------------------------------------------------
       else {
            vector<string> tokens = tokenize(input);
            if (tokens.empty()) continue;

            bool redirect_stdout = false;
            bool redirect_stderr = false;
            bool append_stdout = false;
            bool append_stderr = false;
            string outfile;
            string errfile;
            vector<string> cmd_tokens;  // renamed from text_tokens — holds command + args

            // Start at i=0 to include the command name itself in cmd_tokens
            for (int i =  0; i < (int)tokens.size(); i++) {

                if (tokens[i] == ">" || tokens[i] == "1>") {

                    redirect_stdout = true;

                    if (i + 1 < (int)tokens.size()) {
                        outfile = tokens[i + 1];
                    }

                    break;
                }

                else if (tokens[i] == ">>" || tokens[i] == "1>>") {

                    append_stdout = true;

                    if (i + 1 < (int)tokens.size()) {
                        outfile = tokens[i + 1];
                    }

                    break;
                }

                else if (tokens[i] == "2>") {

                    redirect_stderr = true;

                    if (i + 1 < (int)tokens.size()) {
                        errfile = tokens[i + 1];
                    }

                    break;
                }

                else if (tokens[i] == "2>>") {

                    append_stderr = true;

                    if (i + 1 < (int)tokens.size()) {
                        errfile = tokens[i + 1];
                    }

                    break;
                }

                cmd_tokens.push_back(tokens[i]);
            }

            if (cmd_tokens.empty()) continue;

            // Build null-terminated argv for execvp
            vector<char*> argv;
            for (auto& s : cmd_tokens) {
                argv.push_back(const_cast<char*>(s.c_str()));
            }
            argv.push_back(nullptr);

            pid_t pid = fork();

            if (pid == 0) {
                // ── CHILD PROCESS ────────────────────────────────────────────

                if (redirect_stdout) {
                    int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDOUT_FILENO);  // fd 1 → output file
                    close(fd);
                }

                if (redirect_stderr) {
                    int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDERR_FILENO);  // fd 2 → error file
                    close(fd);
                }

                if(append_stdout){
                    int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDOUT_FILENO);  // fd 1 → output file
                    close(fd);
                }


                if(append_stderr){
                    int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDERR_FILENO);  // fd 2 → error file
                    close(fd);
                }
                execvp(argv[0], argv.data());

                // Only reached if execvp failed
                if (errno == ENOENT) {
                    cerr << argv[0] << ": command not found\n";
                } else {
                    perror(argv[0]);
                }
                exit(1);
            }
            else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
            }
            else {
                perror("fork");
            }
        }
    }   // end while(true)

    return 0;
}
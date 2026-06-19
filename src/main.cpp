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
//   - >  and  1>     : treated as standalone redirect tokens
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
                // Redirection operator: flush current token first, then
                // push ">" as its own token so the parser can detect it.
                if (!current.empty()) {
                         if (current == "1") {
                                tokens.push_back("1>");  //  this is for me to also check 1>
                                current.clear();
                                continue;
                            }

                                tokens.push_back(current);
                                current.clear();
                }
                // Peek ahead: if next char is '>' it could be >> (append),
                // but for now we only handle single > (truncate).
                tokens.push_back(">");
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

            bool redirect = false;   // Will be set true if > or 1> is found
            vector<string> tokens = tokenize(input);

            // Scan tokens to check whether redirection is requested
            for (const auto& tok : tokens) {
                if (tok == ">" || tok == "1>") {
                    redirect = true;
                    break;
                }
            }

            if (redirect) {
                // ---- echo with redirection --------------------------------
                vector<string> text_tokens;  // Words to print
                string outfile;              // Destination filename
                bool past_redirect = false;  // True once we've seen > / 1>

                for (const auto& tok : tokens) {
                    if (tok == "echo") continue;          // Skip command name

                    if (tok == ">" || tok == "1>") {
                        past_redirect = true;             // Switch to filename mode
                        continue;
                    }

                    if (!past_redirect) {
                        text_tokens.push_back(tok);       // Collect text args
                    }
                    else {
                        outfile = tok;                    // Only one filename expected
                    }
                }

                // Open (or create/truncate) the output file
                int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    cerr << "echo: " << outfile << ": cannot open file\n";
                }
                else {
                    // Build the output string and write it to the file
                    string out;
                    for (int i = 0; i < (int)text_tokens.size(); i++) {
                        out += text_tokens[i];
                        if (i + 1 < (int)text_tokens.size()) out += " ";
                    }
                    out += '\n';
                    write(fd, out.c_str(), out.size());
                    close(fd);
                }
            }
            else {
                // ---- Normal echo (no redirection) ------------------------
                // Print all tokens after "echo", space-separated
                for (int i = 1; i < (int)tokens.size(); i++) {
                    cout << tokens[i];
                    if (i + 1 < (int)tokens.size()) cout << " ";
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

            // Separate the command + its arguments from any redirection
            bool redirect = false;
            string outfile;
            vector<string> cmd_tokens;

            for (int i = 0; i < (int)tokens.size(); i++) {

                if (tokens[i] == ">" || tokens[i] == "1>") {
                    // Everything after the redirect symbol is the filename
                    redirect = true;
                    if (i + 1 < (int)tokens.size()) {
                        outfile = tokens[i + 1];
                    }
                    break;   // No further tokens belong to the command args
                }

                cmd_tokens.push_back(tokens[i]);
            }

            // Build the argv array that execvp() expects (null-terminated)
            vector<char*> argv;
            for (auto& s : cmd_tokens) {
                argv.push_back(const_cast<char*>(s.c_str()));
            }
            argv.push_back(nullptr);    // execvp requires a NULL sentinel

            // Fork a child process to execute the command
            pid_t pid = fork();

            if (pid == 0) {
                // ---- Child process ----------------------------------------

                if (redirect) {
                    // Redirect stdout to the specified file
                    int fd = open(
                        outfile.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC,
                        0644
                    );

                    if (fd == -1) {
                        perror("open");
                        exit(1);
                    }

                    // Replace file descriptor 1 (stdout) with our file fd
                    dup2(fd, STDOUT_FILENO);
                    close(fd);  // Close original fd — dup2 made a copy
                }

                // Replace the child process image with the requested command
                execvp(argv[0], argv.data());

                // execvp only returns if it failed (command not found, etc.)
                cerr << argv[0] << ": command not found\n";
                exit(1);
            }
            else if (pid > 0) {
                // ---- Parent process ---------------------------------------
                // Wait for the child to finish before showing the next prompt
                int status;
                waitpid(pid, &status, 0);
            }
            else {
                // fork() itself failed (very rare)
                perror("fork");
            }
        }

    }   // end while(true)

    return 0;
}
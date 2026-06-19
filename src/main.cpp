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
    // e.g. "type echo" -> substr(5) gives us "echo"
    string cmd = input.substr(5);

    // Check if the command is a builtin
    // Just loop through our known builtin names and compare
    for (const string& builtin : builtins) {
        if (builtin == cmd) {
            cout << cmd << " is a shell builtin" << '\n';
            return;   // found it, no need to check PATH, so exit early
        }
    }

    // Not a builtin -> now search for the command across all directories in PATH
    // PATH looks like "/usr/bin:/bin:/usr/local/bin" (colon separated)
    char* path_env = getenv("PATH");
    if (path_env == nullptr) {
        // No PATH variable set at all, so we can't search anywhere
        cout << cmd << ": not found" << '\n';
        return;
    }

    string path(path_env);
    istringstream ss(path);
    string directory;

    // Split PATH on ':' and check each directory one by one
    while (getline(ss, directory, ':')) {
        string full_path = directory + "/" + cmd;

        // access(X_OK) checks whether the file exists and is executable
        // (X_OK = "executable" permission check)
        if (access(full_path.c_str(), X_OK) == 0) {
            cout << cmd << " is " << full_path << '\n';
            return;  // found an executable match, stop searching
        }
    }

    // Went through every PATH directory and found nothing
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

    // We track a small state machine so quotes change how characters are treated.
    // NORMAL = outside quotes, SINGLE = inside '...', DOUBLE = inside "..."
    enum State { NORMAL, SINGLE, DOUBLE };
    State state = NORMAL;

    for (int i = 0; i < (int)input.size(); i++) {

        char ch = input[i];

        // ---- NORMAL state ------------------------------------------------
        // This is the "default" mode, i.e. we're not inside any quotes.
        if (state == NORMAL) {

            if (ch == '\'') {
                // Enter single-quote mode
                // Note: the quote char itself is NOT added to `current`
                state = SINGLE;
            }
            else if (ch == '"') {
                // Enter double-quote mode
                // Same as above, quote char is just a mode switch, not stored
                state = DOUBLE;
            }
            else if (ch == '\\') {
                // Backslash: take the next character literally
                // e.g. \n outside quotes just becomes the letter 'n', not a newline
                if (i + 1 < (int)input.size()) {
                    current += input[++i];  // ++i moves past the backslash, grabs next char
                }
            }
            else if (ch == '&') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                tokens.push_back("&");
            }
            else if (ch == '>') {
                // We've hit a '>' character. Before treating it as a plain
                // redirect, we need to check if it was actually preceded by
                // a "1" or "2" (forming "1>" or "2>"), since those mean
                // "redirect stdout" / "redirect stderr" specifically.

                if (!current.empty()) {

                    if (current == "1") {
                        // current token so far was just "1", and now we see '>'
                        // so this is actually "1>" (explicit stdout redirect)

                        if (i + 1 < (int)input.size() && input[i + 1] == '>') {
                            // next char is also '>', so it's really "1>>" (append stdout)
                            tokens.push_back("1>>");   // append check stdout
                            i++;                       // consume the second '>'
                            current.clear();
                            continue;
                        }

                        // just a single '>', so it's "1>" (truncate/overwrite stdout)
                        tokens.push_back("1>");      // redirect check
                        current.clear();
                        continue;
                    }
                    else if (current == "2") {
                        // Same idea as above but for stderr ("2>" / "2>>")

                        if (i + 1 < (int)input.size() && input[i + 1] == '>') {
                            tokens.push_back("2>>");  // append check stderr
                            i++;
                            current.clear();
                            continue;
                        }

                        tokens.push_back("2>");
                        current.clear();
                        continue;
                    }

                    // current held some other word (not "1" or "2"), so that
                    // word is a separate token on its own — push it first,
                    // then we'll still handle the '>' below as a plain redirect.
                    tokens.push_back(current);
                    current.clear();
                }

                // Plain '>' (no leading digit) — check if it's actually ">>" (append)
                if (i + 1 < (int)input.size() && input[i + 1] == '>') {
                    tokens.push_back(">>");
                    i++;  // consume the second '>'
                }
                else {
                    tokens.push_back(">");
                }
            }
            else if (ch == ' ') {
                // Space: delimiter — push accumulated token if non-empty
                // (multiple spaces in a row won't create empty tokens because
                // we only push when `current` is non-empty)
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            }
            else {
                // Any regular character just gets appended to the current token
                current += ch;
            }
        }

        // ---- SINGLE-QUOTE state ------------------------------------------
        // Inside '...' everything is taken literally, no escape sequences at all
        else if (state == SINGLE) {

            if (ch == '\'') {
                // Closing single quote — return to NORMAL
                state = NORMAL;
            }
            else {
                // Everything inside single quotes is literal
                // (even backslashes are kept as-is, no special meaning here)
                current += ch;
            }
        }

        // ---- DOUBLE-QUOTE state ------------------------------------------
        // Inside "..." some escape sequences ARE recognized, but only a few
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
                        // Recognized escape: drop the backslash, keep just the char
                        current += next;
                        i++;    // Skip the escaped character
                    }
                    else {
                        // Not a recognised escape — keep the backslash literally
                        // (e.g. \n inside "" stays as backslash + n)
                        current += ch;
                    }
                }
                else {
                    // Backslash is the very last character in the string, nothing
                    // to escape, so just keep it as-is
                    current += ch;
                }
            }
            else {
                current += ch;
            }
        }
    }

    // Push any remaining accumulated characters as the last token
    // (handles the case where input doesn't end with a space/redirect)
    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// Job management
// ---------------------------------------------------------------------------
struct Job {
    int job_id;
    pid_t pid;
    string command;
    string status;
};

// store the background jobs
vector<Job> jobs;
int next_job_id = 1;

// ---------------------------------------------------------------------------
// computeMarkers
// Finds the highest and second-highest job_ids in the current jobs list.
//
// Marker rules:
//   '+' -> job with highest job_id
//   '-' -> job with second-highest job_id
//   ' ' -> everything else
// ---------------------------------------------------------------------------
void computeMarkers(int& max_id, int& second_id) {

    max_id = -1;
    second_id = -1;

    for (auto& j : jobs) {
        if (j.job_id > max_id) {
            second_id = max_id;
            max_id = j.job_id;
        }
        else if (j.job_id > second_id) {
            second_id = j.job_id;
        }
    }
}

// ---------------------------------------------------------------------------
// reapJobs
// Checks for finished background jobs via waitpid(WNOHANG).
//
// If print_done=true  : prints done jobs (with correct +/- markers, computed
//                       the same way as the "jobs" builtin) and removes them
//                       from the list. Used at the top of the REPL loop so
//                       the user sees completion notices before the next
//                       prompt (BV8 behavior).
//
// If print_done=false : only marks finished jobs as "Done", does NOT print
//                       or remove them. Used before the "jobs" builtin so
//                       it can display done jobs inline with running ones
//                       (RQ2 behavior).
// ---------------------------------------------------------------------------
void reapJobs(bool print_done) {

    // First pass: check all running jobs and mark finished ones as "Done"
    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].status == "Running") {
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
            if (result == jobs[i].pid && WIFEXITED(status)) {
                jobs[i].status = "Done";
            }
        }
    }

    if (!print_done) {
        return; 
    }

    // FIX: Compute the +/- markers over the FULL current job list (same as
    // the "jobs" builtin does) BEFORE we print/remove anything. Doing this
    // first means a job that is the most-recently-started ("+") still gets
    // its marker printed correctly when it finishes, matching real shell
    // behavior and the BV8 expected output ("[2]+  Done ...").
    int max_id, second_id;
    computeMarkers(max_id, second_id);

    vector<int> remove_indices;

    for (int i = 0; i < (int)jobs.size(); i++) {
        if (jobs[i].status == "Done") {

            char marker = ' ';
            if (jobs[i].job_id == max_id) {
                marker = '+';
            }
            else if (jobs[i].job_id == second_id) {
                marker = '-';
            }

            // Standard format: [JobID]marker  Status  Command
            cout << "[" << jobs[i].job_id << "]" << marker << "  "
                 << left << setw(24) << "Done";

            string cmd = jobs[i].command;
            // Strip the trailing " &" for cleaner display
            if (cmd.size() >= 2 && cmd.substr(cmd.size() - 2) == " &") {
                cmd.erase(cmd.size() - 2);
            }
            cout << cmd << '\n';

            remove_indices.push_back(i);
        }
    }

    // Remove done jobs in reverse order
    for (int i = (int)remove_indices.size() - 1; i >= 0; i--) {
        jobs.erase(jobs.begin() + remove_indices[i]);
    }
}

// ---------------------------------------------------------------------------
// main — REPL (Read-Eval-Print Loop)
// ---------------------------------------------------------------------------
int main() {

    // Flush stdout and stderr after every write so the prompt is always visible
    // (unitbuf disables buffering delays, important for an interactive shell)
    cout << unitbuf;
    cerr << unitbuf;

    // List of commands handled directly by the shell (not forked into a child process)
    vector<string> builtins = { "exit", "echo", "type", "pwd", "cd", "jobs" };

    while (true) {

        // Print and remove any done jobs before showing the prompt
        // so the user sees completion notices inline (BV8 behavior)
        reapJobs(true);

        // Print prompt (no newline, so user types on the same line)
        cout << "$ ";

        // Read a full line of input
        string input;
        if (!getline(cin, input))
            break;  // EOF (Ctrl-D) — exit the shell

        // ----------------------------------------------------------------
        // Built-in: exit
        // ----------------------------------------------------------------
        if (input == "exit") {
            break;  // breaks out of the while(true) loop, ending the program
        }

        // ----------------------------------------------------------------
        // Built-in: jobs
        // Used for background processing.
        // Shows all current jobs (running and done) with correct markers,
        // then removes done ones from the list.
        // ----------------------------------------------------------------
        else if (input == "jobs") {

            // Reap to update statuses (mark done), but don't print or remove yet
            reapJobs(false);

            // Compute markers over the full current list (including done jobs)
            int max_id, second_id;
            computeMarkers(max_id, second_id);

            vector<int> to_remove;

            for (int i = 0; i < (int)jobs.size(); i++) {

                char marker = ' ';
                if (jobs[i].job_id == max_id) {
                    marker = '+';
                }
                else if (jobs[i].job_id == second_id) {
                    marker = '-';
                }

                string cmd = jobs[i].command;

                if (jobs[i].status == "Done") {
                    // Strip trailing " &" for done job display
                    if (cmd.size() >= 2 && cmd.substr(cmd.size() - 2) == " &") {
                        cmd.erase(cmd.size() - 2);
                    }
                    cout << "[" << jobs[i].job_id << "]" << marker << "  ";
                    cout << left << setw(24) << "Done" << cmd << '\n';
                    to_remove.push_back(i);  // mark for removal after printing
                }
                else {
                    cout << "[" << jobs[i].job_id << "]" << marker << "  ";
                    cout << left << setw(24) << jobs[i].status << cmd << '\n';
                }
            }

            // Remove done jobs in reverse order so indices stay valid
            for (int i = (int)to_remove.size() - 1; i >= 0; i--) {
                jobs.erase(jobs.begin() + to_remove[i]);
            }
        }

        // ----------------------------------------------------------------
        // Built-in: echo
        // Supports output redirection:  echo hello > file.txt
        //                               echo hello 1> file.txt
        // ----------------------------------------------------------------
        else if (input.rfind("echo ", 0) == 0) {
            // rfind("echo ", 0) == 0 means the string STARTS WITH "echo "

            bool redirect_stdout = false;   // true if we saw ">" or "1>" (overwrite)
            bool redirect_stderr = false;   // true if we saw "2>" (overwrite)
            bool append_stdout   = false;   // true if we saw ">>" or "1>>" (append)
            bool append_stderr   = false;   // true if we saw "2>>" (append)

            vector<string> tokens = tokenize(input);  // break the line into proper tokens

            string outfile;   // filename to write stdout to, if redirecting
            string errfile;   // filename to write stderr to, if redirecting

            vector<string> text_tokens;  // the actual words to echo (excludes "echo" itself and redirection bits)

            // Parse echo arguments and redirection
            // Start at index 1 to skip the "echo" token itself
            for (int i = 1; i < (int)tokens.size(); i++) {

                if (tokens[i] == ">" || tokens[i] == "1>") {
                    redirect_stdout = true;
                    // The token right after ">" is the destination filename
                    if (i + 1 < (int)tokens.size()) outfile = tokens[i + 1];
                    break;  // stop scanning, everything after this was just the filename
                }
                else if (tokens[i] == ">>" || tokens[i] == "1>>") {
                    append_stdout = true;
                    if (i + 1 < (int)tokens.size()) outfile = tokens[i + 1];
                    break;
                }
                else if (tokens[i] == "2>") {
                    redirect_stderr = true;
                    if (i + 1 < (int)tokens.size()) errfile = tokens[i + 1];
                    break;
                }
                else if (tokens[i] == "2>>") {
                    append_stderr = true;
                    if (i + 1 < (int)tokens.size()) errfile = tokens[i + 1];
                    break;
                }
                else {
                    // If it wasn't a redirection token, it's part of the text to print
                    text_tokens.push_back(tokens[i]);
                }
            }

            // Case 1: stdout redirection requested (echo ... > file  OR  echo ... 1> file)
            if (redirect_stdout) {

                // O_TRUNC = if file exists, erase its contents first (overwrite behavior)
                // O_CREAT = create the file if it doesn't exist
                int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (fd != -1) {
                    // Manually rebuild the "echo" output string, joining words with spaces
                    string out;
                    for (int i = 0; i < (int)text_tokens.size(); i++) {
                        out += text_tokens[i];
                        if (i + 1 < (int)text_tokens.size()) out += " ";  // space between words, but not after the last one
                    }
                    out += '\n';  // echo always ends with a newline

                    // Write directly to the file descriptor (bypasses cout, goes to the file)
                    write(fd, out.c_str(), out.size());
                    close(fd);
                }
            }

            // Case 2: stderr redirection requested (echo ... 2> file)
            else if (redirect_stderr) {

                // echo produces no stderr — just create/truncate the file
                int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd != -1) {
                    // We open+truncate the error file just to mimic real shell behavior
                    // (the file gets created/emptied even though echo writes nothing to it)
                    close(fd);
                }

                // Since echo's actual output is on stdout, print it normally to the terminal
                for (int i = 0; i < (int)text_tokens.size(); i++) {
                    cout << text_tokens[i];
                    if (i + 1 < (int)text_tokens.size()) cout << " ";
                }
                cout << '\n';
            }

            // Case 3: stdout append requested (echo ... >> file  OR  echo ... 1>> file)
            else if (append_stdout) {

                int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                // O_APPEND does not rewrite the file but appends to it

                if (fd != -1) {
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

            // Case 4: stderr append requested (echo ... 2>> file)
            else if (append_stderr) {

                // echo produces no stderr
                // O_APPEND means the file is created if missing, but NOT truncated
                int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd != -1) {
                    close(fd);
                }

                // echo's real output always goes to stdout/terminal here
                // since there's nothing for echo to actually send to stderr
                for (int i = 0; i < (int)text_tokens.size(); i++) {
                    cout << text_tokens[i];
                    if (i + 1 < (int)text_tokens.size()) cout << " ";
                }
                cout << '\n';
            }

            // Case 5: no redirection at all — plain "echo hello world"
            else {
                for (int i = 0; i < (int)text_tokens.size(); i++) {
                    cout << text_tokens[i];
                    if (i + 1 < (int)text_tokens.size()) cout << " ";
                }
                cout << '\n';
            }
        }

        // ----------------------------------------------------------------
        // Built-in: type
        // Tells the user what kind of command something is.
        // ----------------------------------------------------------------
        else if (input.rfind("type ", 0) == 0) {
            // Delegate entirely to the helper function defined above
            typeCommand(input, builtins);
        }

        // ----------------------------------------------------------------
        // Built-in: pwd
        // Prints the current working directory.
        // ----------------------------------------------------------------
        else if (input == "pwd") {
            char cwd[4096];  // buffer to hold the path string
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                cout << cwd << '\n';
            }
            else {
                // getcwd failed (e.g. buffer too small, or permission issue)
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

            if (parts.empty()) continue;  // "cd" with no argument — ignore (back to prompt)

            string target = parts[0];   // we only look at the first argument, ignore extras

            if (target == "~") {
                // Expand ~ to the HOME directory
                char* home = getenv("HOME");
                if (home == nullptr || chdir(home) == -1) {
                    cout << "cd: ~: No such file or directory\n";
                }
            }
            else {
                // Try to change directory directly to whatever path was given
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
            if (tokens.empty()) continue;  // blank/whitespace-only input, nothing to do

            bool background = false;
            if (!tokens.empty() && tokens.back() == "&") {
                background = true;
                tokens.pop_back();
            }

            bool redirect_stdout = false;
            bool redirect_stderr = false;
            bool append_stdout   = false;
            bool append_stderr   = false;
            string outfile;
            string errfile;
            vector<string> cmd_tokens;  // holds command + args

            // Start at i=0 to include the command name itself in cmd_tokens
            // (unlike the echo case above, here token[0] IS part of what we need —
            // it's the program name to execute, e.g. "ls", "cat", etc.)
            for (int i = 0; i < (int)tokens.size(); i++) {

                if (tokens[i] == ">" || tokens[i] == "1>") {
                    redirect_stdout = true;
                    if (i + 1 < (int)tokens.size()) outfile = tokens[i + 1];
                    break;
                }
                else if (tokens[i] == ">>" || tokens[i] == "1>>") {
                    append_stdout = true;
                    if (i + 1 < (int)tokens.size()) outfile = tokens[i + 1];
                    break;
                }
                else if (tokens[i] == "2>") {
                    redirect_stderr = true;
                    if (i + 1 < (int)tokens.size()) errfile = tokens[i + 1];
                    break;
                }
                else if (tokens[i] == "2>>") {
                    append_stderr = true;
                    if (i + 1 < (int)tokens.size()) errfile = tokens[i + 1];
                    break;
                }
                else {
                    // Not a redirection token — it's part of the command/arguments
                    cmd_tokens.push_back(tokens[i]);
                }
            }

            if (cmd_tokens.empty()) continue;  // safety check, shouldn't normally happen

            // Build null-terminated argv for execvp
            // execvp requires a char* array ending in nullptr, e.g. {"ls", "-l", NULL}
            vector<char*> argv;
            for (auto& s : cmd_tokens) {
                argv.push_back(const_cast<char*>(s.c_str()));
            }
            argv.push_back(nullptr);  // execvp uses this to know where the args end

            // fork() creates a near-identical copy of the current process.
            // It returns 0 in the child process, and the child's PID in the parent.
            pid_t pid = fork();

            if (pid == 0) {
                // ── CHILD PROCESS ────────────────────────────────────────────
                // Everything below only runs inside the newly forked child.
                // We set up any redirection BEFORE calling execvp, since
                // execvp replaces the child's memory/image entirely.

                if (redirect_stdout) {
                    int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDOUT_FILENO);  // fd 1 → output file
                    // dup2 makes STDOUT_FILENO (1) point to our opened file,
                    // so anything the child normally prints to screen goes to the file instead
                    close(fd);  // we can close the original fd, dup2 already aliased it
                }

                if (redirect_stderr) {
                    int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDERR_FILENO);  // fd 2 → error file
                    close(fd);
                }

                if (append_stdout) {
                    int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDOUT_FILENO);  // fd 1 → output file
                    close(fd);
                }

                if (append_stderr) {
                    int fd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) { perror("open"); exit(1); }
                    dup2(fd, STDERR_FILENO);  // fd 2 → error file
                    close(fd);
                }

                // Replace this child process's program image with the requested command.
                // If successful, execvp NEVER RETURNS — the code below only runs on failure.
                execvp(argv[0], argv.data());

                // Only reached if execvp failed
                if (errno == ENOENT) {
                    // ENOENT = the executable simply doesn't exist / wasn't found
                    cerr << argv[0] << ": command not found\n";
                }
                else {
                    // Some other error (permissions, etc.) — perror prints a description
                    perror(argv[0]);
                }
                exit(1);  // child must exit, otherwise we'd have two shells running!
            }
            else if (pid > 0) {
                // ── PARENT PROCESS ───────────────────────────────────────────
                // pid here is the child's process ID.

                if (background) {
                    // Register background job and print its job number + PID
                    jobs.push_back({ next_job_id, pid, input, "Running" });
                    cout << "[" << next_job_id << "] " << pid << '\n';
                    next_job_id++;
                }
                else {
                    // Wait for the child to finish before printing the next prompt
                    // so commands run synchronously
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
            else {
                // fork() returned a negative number — something went wrong
                // (e.g. system resource limits), and no child was created at all.
                perror("fork");
            }
        }

    }   // end while(true)

    return 0;
}
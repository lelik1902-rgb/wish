#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void print_error() {
    static const char message[] = "An error has occurred\n";
    if (write(STDERR_FILENO, message, std::strlen(message)) < 0) {}
}

void print_prompt() {
    static const char prompt[] = "wish> ";
    if (write(STDOUT_FILENO, prompt, std::strlen(prompt)) < 0) {}
}

std::vector<std::string> tokenize(const std::string& text) {
    static const char whitespace[] = " \t\r\n\v\f";
    std::vector<std::string> tokens;
    std::size_t start = text.find_first_not_of(whitespace);
    while (start != std::string::npos) {
        const std::size_t end = text.find_first_of(whitespace, start);
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(whitespace, end);
    }
    return tokens;
}

struct Command {
    std::vector<std::string> args;
    std::string output_file;  
};

bool parse_command(const std::string& text, Command& command) {
    const std::size_t redirect = text.find('>');
    command.args = tokenize(text.substr(0, redirect));
    if (redirect == std::string::npos) return true;

    const std::string target = text.substr(redirect + 1);
    const std::vector<std::string> files = tokenize(target);
    if (command.args.empty() || files.size() != 1 ||
        target.find('>') != std::string::npos) {
        return false;
    }
    command.output_file = files[0];
    return true;
}

bool redirect_output(const std::string& path) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    const bool ok = dup2(fd, STDOUT_FILENO) >= 0 && dup2(fd, STDERR_FILENO) >= 0;
    close(fd);
    return ok;
}

std::vector<std::string> search_path = {"/bin"};

std::string find_program(const std::string& name) {
    for (const std::string& directory : search_path) {
        const std::string candidate = directory + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) return candidate;
    }
    return "";
}

bool run_builtin(const std::vector<std::string>& args, bool& should_exit) {
    const std::string& name = args[0];
    if (name == "exit") {
        if (args.size() != 1) {
            print_error();
        } else {
            should_exit = true;
        }
    } else if (name == "cd") {
        if (args.size() != 2 || chdir(args[1].c_str()) != 0) print_error();
    } else if (name == "path") {
        search_path.assign(args.begin() + 1, args.end());
    } else {
        return false;
    }
    return true;
}

pid_t start_program(const Command& command) {
    const std::vector<std::string>& args = command.args;
    const std::string program = find_program(args[0]);
    if (program.empty()) {
        print_error();
        return -1;
    }

    std::vector<char*> exec_args;
    for (const std::string& arg : args) {
        exec_args.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_args.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        print_error();
        return -1;
    }
    if (pid == 0) {
        if (!command.output_file.empty() && !redirect_output(command.output_file)) {
            print_error();
            _exit(1);
        }
        execv(program.c_str(), exec_args.data());
        print_error();
        _exit(1);
    }

    return pid;
}

void execute_line(const std::string& line) {
    std::vector<Command> commands;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find('&', start);
        Command command;
        if (!parse_command(line.substr(start, end - start), command)) {
            print_error();
            return;
        }
        if (!command.args.empty()) commands.push_back(std::move(command));
        if (end == std::string::npos) break;
        start = end + 1;
    }

    std::vector<pid_t> children;
    bool should_exit = false;
    for (const Command& command : commands) {
        if (run_builtin(command.args, should_exit)) continue;
        const pid_t pid = start_program(command);
        if (pid > 0) children.push_back(pid);
    }

    for (const pid_t pid : children) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    }
    if (should_exit) std::exit(0);
}

}  

int main(int argc, char* argv[]) {
    if (argc > 2) {
        print_error();
        return 1;
    }

    const bool interactive = argc == 1;
    FILE* input = stdin;
    if (!interactive) {
        input = std::fopen(argv[1], "re");
        if (input == nullptr) {
            print_error();
            return 1;
        }
    }

    char* buffer = nullptr;
    std::size_t capacity = 0;
    while (true) {
        if (interactive) print_prompt();

        const ssize_t length = getline(&buffer, &capacity, input);
        if (length < 0) break;
        execute_line(std::string(buffer, static_cast<std::size_t>(length)));
    }

    const bool read_failed = std::ferror(input) != 0;
    std::free(buffer);
    if (read_failed) {
        print_error();
        return 1;
    }
    return 0;
}

// wish - проста оболонка Unix (OSTEP, processes-shell).
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {

void print_error() {
    static const char message[] = "An error has occurred\n";
    // Результат навмисно ігноруємо: повідомити про збій виводу помилки вже нікуди.
    if (write(STDERR_FILENO, message, std::strlen(message)) < 0) {}
}

void print_prompt() {
    static const char prompt[] = "wish> ";
    if (write(STDOUT_FILENO, prompt, std::strlen(prompt)) < 0) {}
}

// Розбиває рядок на слова за пробільними символами.
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

// Шлях пошуку програм; команда path повністю його перезаписує.
std::vector<std::string> search_path = {"/bin"};

// Перший виконуваний файл у каталогах шляху; порожній рядок, якщо не знайдено.
std::string find_program(const std::string& name) {
    for (const std::string& directory : search_path) {
        const std::string candidate = directory + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) return candidate;
    }
    return "";
}

// Виконує вбудовану команду; повертає false, якщо це не вбудована команда.
bool run_builtin(const std::vector<std::string>& args) {
    const std::string& name = args[0];
    if (name == "exit") {
        if (args.size() != 1) {
            print_error();
        } else {
            std::exit(0);
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

void execute_line(const std::string& line) {
    const std::vector<std::string> args = tokenize(line);
    if (args.empty() || run_builtin(args)) return;

    const std::string program = find_program(args[0]);
    if (program.empty()) {
        print_error();
        return;
    }

    // Масив для execv готуємо до fork: він закінчується nullptr.
    std::vector<char*> exec_args;
    for (const std::string& arg : args) {
        exec_args.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_args.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        print_error();
        return;
    }
    if (pid == 0) {
        execv(program.c_str(), exec_args.data());
        // execv повертає керування лише у разі помилки.
        print_error();
        _exit(1);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        print_error();
        return 1;
    }

    const bool interactive = argc == 1;
    FILE* input = stdin;
    if (!interactive) {
        // "e" (O_CLOEXEC): дочірні процеси не мають успадковувати пакетний файл.
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

    // getline повертає -1 і на EOF, і на помилку (наприклад, файл є каталогом).
    const bool read_failed = std::ferror(input) != 0;
    std::free(buffer);
    if (read_failed) {
        print_error();
        return 1;
    }
    return 0;
}

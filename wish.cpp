// wish - проста оболонка Unix (OSTEP, processes-shell).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

void execute_line(const std::string& line) {
    (void)line;
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

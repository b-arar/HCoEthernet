#include "bpf_compiler.h"


int main() {
    struct bpf_program program;
    struct bpf_program *prog = &program;
    char str[100];
    do {
        printf("Input filter string:\n");
        fgets(str, 100, stdin);
        printf("inputted string = %s\n", str);
        if (str[0] != 0) {
            if (compile_filter(&str[0], prog) == 0) {
                print_filter_program(prog);
            }
        }
    } while(str[0] != 0);
    return 0;
}
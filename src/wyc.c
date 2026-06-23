#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/utf8_console.h"

#ifdef _WIN32
#define EXE_EXT ".exe"
#else
#define EXE_EXT ""
#endif
void printHelp(char* exec) {
    printf("用式：%s <源書.wy> [成器]\n", exec);
    printf("編文言之書，鑄為可執之器。\n");
    printf("諸項：\n");
    printf("  -v  詳述其事\n");
    printf("  -l  詳述解詞\n");
    printf("  -x  唯解其詞\n");
    printf("  -c  顯色\n");
    printf("  -h  示此指南而退\n");
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool keepFiles = false;
    bool lexerVerbose = false;
    bool lexerOnly = false;
    bool colorEnabled = false;

    const char *input = NULL, *output = NULL;

    // Read command args
    char** argv_p = &argv[1];
    for (int i = 1; i < argc; i++) {
        if ((*argv_p)[0] == '-') {
            switch ((*argv_p)[1]) {
            case 'v': verbose = true;
                break;
            case 'l': lexerVerbose = true;
                break;
            case 'x': lexerOnly = true;
                break;
            case 'k': keepFiles = true;
                break;
            case 'c': colorEnabled = true;
                break;
            case 'h': printHelp(argv[0]);
                return 0;
            default:
                fprintf(stderr, "謬矣：未識之項：%s\n", *argv_p);
                printHelp(argv[0]);
                return 1;
            }
        } else {
            // Read input and output file path
            if (!input)
                input = *argv_p;
            else if (!output)
                output = *argv_p;
            else {
                fprintf(stderr, "謬矣：所引過繁\n");
                printHelp(argv[0]);
                return 1;
            }
        }

        ++argv_p;
    }

    if (!input && !output) {
        printHelp(argv[0]);
        return 1;
    }
    char* output_name = NULL;
    if (!output) {
        // Extract filename from input path (remove directory and extension)
        const char* filename = strrchr(input, '/');
        if (!filename)
            filename = strrchr(input, '\\');
        filename = filename ? filename + 1 : input;

        // Find the last dot to remove extension
        const char* ext = strrchr(filename, '.');
        size_t name_len = ext ? (size_t)(ext - filename) : strlen(filename);

        // Allocate and copy the name
        output_name = malloc(name_len + 1);
        strncpy(output_name, filename, name_len);
        output_name[name_len] = '\0';
        output = output_name;
    }

    char ll_file[1024];
    char o_file[1024];
    snprintf(ll_file, sizeof(ll_file), "%s.ll", output);
    snprintf(o_file, sizeof(o_file), "%s.o", output);

    char wy_path[1024];
    char wy_path_dir[1024];
    const char* last_slash = strrchr(argv[0], '/');
    if (!last_slash)
        last_slash = strrchr(argv[0], '\\');

    if (last_slash) {
        size_t dir_len = (size_t)(last_slash - argv[0]) + 1;
        strncpy(wy_path, argv[0], dir_len);
        wy_path[dir_len] = '\0';

        strncpy(wy_path_dir, argv[0], dir_len - 1);
        wy_path_dir[dir_len - 1] = '\0';

        strncat(wy_path, "wy", sizeof(wy_path) - dir_len - 1);
        strncat(wy_path, EXE_EXT, sizeof(wy_path) - strlen(wy_path) - 1);
    } else {
        snprintf(wy_path, sizeof(wy_path), "wy%s", EXE_EXT);
        snprintf(wy_path_dir, sizeof(wy_path_dir), ".");
    }

    char cmd[4096];

    // wy <input> <ll_file>
    snprintf(cmd, sizeof(cmd), "\"%s\"%s%s%s%s %s %s", wy_path,
             verbose ? " -v" : "",
             lexerVerbose ? " -l" : "",
             lexerOnly ? " -x" : "",
             colorEnabled ? " -c" : "",
             input, ll_file);
    if (verbose)
        printf("方行：%s\n", cmd);
    if (system(cmd) != 0) {
        fprintf(stderr, "謬矣：未克譯 %s 為 LLVM IR\n", input);
        goto FAILED;
    }

    // llc -relocation-model=pic -filetype=obj <ll_file> -o <o_file>
    // LLVM 14 requires -opaque-pointers for ptr type; LLVM 15+ enables it by default
    const char* opaque = (system("llc --version 2>&1 | grep -q 'LLVM version 14'") == 0) ? " -opaque-pointers" : "";
    snprintf(cmd, sizeof(cmd), "llc -relocation-model=pic -filetype=obj%s %s -o %s", opaque, ll_file, o_file);
    if (verbose)
        printf("方行：%s\n", cmd);
    if (system(cmd) != 0) {
        fprintf(stderr, "謬矣：未克譯 %s 為物檔\n", ll_file);
        goto FAILED;
    }

    // gcc -fPIE <o_file> -L<lib_dir> -lwenyan-runtime -lm -o <output>
    snprintf(cmd, sizeof(cmd), "gcc -fPIE %s -L\"%s/lib\" -lwenyan-runtime -lm -o %s", o_file, wy_path_dir, output);
    if (verbose)
        printf("方行：%s\n", cmd);
    if (system(cmd) != 0) {
        fprintf(stderr, "謬矣：未克鏈結 %s 為成器\n", o_file);
        goto FAILED;
    }

    if (!keepFiles) {
        remove(ll_file);
        remove(o_file);
    }

    free(output_name);
    return 0;

FAILED:
    free(output_name);
    return 1;
}

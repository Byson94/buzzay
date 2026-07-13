#include <stdio.h>
#include <getopt.h>

const char *program_name = "buzzay";
const char *program_ver = "0.1.0";

void print_help() {
    printf("A very extensible wayland compositor.\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -l, --load     Load a plugin\n");
    printf("  -m, --msg      Send a message to a plugin\n");
    printf("  -h, --help     Show this help message and exit\n");
    printf("  -v, --version  Print the program version\n");
}

int main(int argc, char** argv) {
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
        { "load", required_argument, 0, 'l' },
        { "msg", required_argument, 0, 'm' },
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "+hvlm", long_options, NULL)) != -1) {
        switch (opt) {
            case 'l':
                if ((argc - optind) > 1) {
                    printf("'%s' accepts only one argument.\n", argv[optind - 1]);
                    return 1;
                }

                const char* plugin = argv[optind];

                break;
            case 'm':
                for (int i = optind; i < argc; i++) {
                    printf("%s\n", argv[i]);
                }
                break;
            case 'h':
                print_help();
                break;
            case 'v': 
                printf("%s v%s\n", program_name, program_ver);
                break;
        }
    }

    for (int i = optind; i < argc; i++) {
        if (argv[i][0] == '-') {
            fprintf(stderr, "Warning: Unexpected '%s' flag. "
                            "Only one option must be used in each command.\n", argv[i]);
        }
    }

    return 0;
}

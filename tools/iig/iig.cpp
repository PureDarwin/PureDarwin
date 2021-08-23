#include "iig.h"

static void usage(const char *progname) {
    fprintf(stderr, "usage: %s --def <path/to/input.iig> --header <path/to/header.h> --impl <path/to/source.iig.cpp> -- <clang args>\n", progname);
    fprintf(stderr, "note: The --edits, --log, --framework-name, and --deployment-target options from Apple iig are not implemented and will be ignored\n");
    exit(1);
}

int main(int argc, const char * argv[]) {
    string inputFilePath, headerOutputPath, implOutputPath;
    vector<const char *> extraClangArgs; bool seenDashDash = false;

    for (int i = 1; i < argc; i++) {
        if (seenDashDash) {
            extraClangArgs.push_back(argv[i]);
        } else if (strequal(argv[i], "--def")) {
            if (++i == argc) {
                fprintf(stderr, "iig: error: --def option requires an argument\n");
                usage(argv[0]);
            }

            inputFilePath = argv[i];
        } else if (strequal(argv[i], "--header")) {
            if (++i == argc) {
                fprintf(stderr, "iig: error: --header option requires an argument\n");
                usage(argv[0]);
            }

            headerOutputPath = argv[i];
        } else if (strequal(argv[i], "--impl")) {
            if (++i == argc) {
                fprintf(stderr, "iig: error: --impl option requires an argument\n");
                usage(argv[0]);
            }

            implOutputPath = argv[i];
        } else if (strequal(argv[i], "--edits") || strequal(argv[i], "--log") || strequal(argv[i], "--framework-name") || strequal(argv[i], "--deployment-target")) {
            if (++i == argc) {
                fprintf(stderr, "iig: error: %s option requires an argument\n", argv[i]);
                usage(argv[0]);
            }

            fprintf(stderr, "iig: warning: %s option unimplemented and ignored\n", argv[i]);
        } else if (strequal(argv[i], "--help")) {
            usage(argv[0]);
        } else if (strequal(argv[i], "--")) {
            seenDashDash = true;
        } else {
            fprintf(stderr, "iig: error: unrecognized option: %s\n", argv[i]);
            usage(argv[0]);
        }
    }

    if (inputFilePath.size() == 0) {
        fprintf(stderr, "iig: error: input file not specified\n");
        usage(argv[0]);
    } else if (headerOutputPath.size() == 0) {
        fprintf(stderr, "iig: error: output header file not specified\n");
        usage(argv[0]);
    } else if (implOutputPath.size() == 0) {
        fprintf(stderr, "iig: error: output source file not specified\n");
        usage(argv[0]);
    }

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit source = clang_createTranslationUnitFromSourceFile(index, inputFilePath.c_str(), extraClangArgs.size(), &extraClangArgs[0], 0, nullptr);

    clang_disposeIndex(index);
    return 0;
}

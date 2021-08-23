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

    size_t num_diags = clang_getNumDiagnostics(source);
    if (num_diags > 0) {
        bool found_error = false;
        for (size_t diag_index = 0; diag_index < num_diags; diag_index++) {
            auto diag = clang_getDiagnostic(source, diag_index);

            const char *severity = nullptr;
            switch (clang_getDiagnosticSeverity(diag)) {
                case CXDiagnostic_Fatal:
                    severity = "fatal error";
                    break;
                case CXDiagnostic_Error:
                    severity = "error";
                    found_error = true;
                    break;
                case CXDiagnostic_Warning:
                    severity = "warning";
                    break;
                case CXDiagnostic_Note:
                    severity = "note";
                    break;
                default:
                    assertion_failure("unknown CXDiagnosticSeverity");
                    break;
            }
            if (clang_getDiagnosticSeverity(diag) == CXDiagnosticSeverity::CXDiagnostic_Error) {
                found_error = true;
            }

            auto text = clang_formatDiagnostic(diag, CXDiagnostic_DisplaySourceLocation);
            fprintf(stderr, "iig: %s: ", severity);
            fwrite(stderr, text);
            fwrite(stderr, "\n");
            clang_disposeString(text);

            if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal) {
                exit(1);
            }

            clang_disposeDiagnostic(diag);
        }

        if (found_error) exit(1);
    }

    clang_disposeIndex(index);
    return 0;
}

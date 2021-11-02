#include "iig.h"

static void usage(const char *progname) {
    std::cerr <<
        "usage: " << progname <<
        " --def <path/to/input.iig> --header <path/to/header.h> --impl <path/to/source.iig.cpp> -- <clang args>\n"
        "note: The --edits, --log, --framework-name, and --deployment-target options from Apple iig are not "
        "implemented and will be ignored" << std::endl;

    exit(1);
}

int main(int argc, const char * argv[]) {
    int exitCode = 0;
    string inputFilePath, headerOutputPath, implOutputPath;
    vector<const char *> extraClangArgs; bool seenDashDash = false;

    for (int i = 1; i < argc; i++) {
        if (seenDashDash) {
            extraClangArgs.push_back(argv[i]);
        } else if (strequal(argv[i], "--def")) {
            if (++i == argc) {
                std::cerr << "iig: error: --def option requires an argument" << std::endl;
                usage(argv[0]);
            }

            inputFilePath = argv[i];
        } else if (strequal(argv[i], "--header")) {
            if (++i == argc) {
                std::cerr << "iig: error: --header option requires an argument" << std::endl;
                usage(argv[0]);
            }

            headerOutputPath = argv[i];
        } else if (strequal(argv[i], "--impl")) {
            if (++i == argc) {
                std::cerr << "iig: error: --impl option requires an argument" << std::endl;
                usage(argv[0]);
            }

            implOutputPath = argv[i];
        } else if (strequal(argv[i], "--edits") || strequal(argv[i], "--log") || strequal(argv[i], "--framework-name") || strequal(argv[i], "--deployment-target")) {
            if (++i == argc) {
                std::cerr << "iig: error: " << argv[i] << " option requires an argument" << std::endl;
                usage(argv[0]);
            }

            std::cerr << "iig: warning: " << argv[i] << " option unimplemented and ignored" << std::endl;
        } else if (strequal(argv[i], "--help")) {
            usage(argv[0]);
        } else if (strequal(argv[i], "--")) {
            seenDashDash = true;
        } else {
            std::cerr << "iig: error: unrecognized option: " << argv[i] << std::endl;
            usage(argv[0]);
        }
    }

    if (inputFilePath.size() == 0) {
        std::cerr << "iig: error: input file not specified" << std::endl;
        usage(argv[0]);
    } else if (headerOutputPath.size() == 0) {
        std::cerr << "iig: error: output header file not specified" << std::endl;
        usage(argv[0]);
    } else if (implOutputPath.size() == 0) {
        std::cerr << "iig: error: output source file not specified" << std::endl;
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

            auto text = clang_formatDiagnostic(diag, CXDiagnostic_DisplaySourceLocation);
            std::cerr << "iig: " << severity << text << "\n";
            clang_disposeString(text);

            if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal) {
                exit(1);
            }

            clang_disposeDiagnostic(diag);
        }

        if (found_error) exit(1);
    }

    std::cerr << "iig: fatal error: iig is not implemented at this time." << std::endl;
    exitCode = -1;

    clang_disposeIndex(index);
    return exitCode;
}

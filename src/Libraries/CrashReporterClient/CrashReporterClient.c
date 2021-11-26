#include "CrashReporterClient.h"

__attribute__((section("__DATA," CRASHREPORTER_ANNOTATIONS_SECTION)))
struct crashreporter_annotations_t gCRAnnotations = {
	.version = CRASHREPORTER_ANNOTATIONS_VERSION,
	.message = 0,
	.signature_string = 0,
	.backtrace = 0,
	.message2 = 0,
	.thread = 0,
	.dialog_mode = 0,
	.abort_cause = 0
};

#ifndef SWC_OUTPUT_MANAGEMENT_H
#define SWC_OUTPUT_MANAGEMENT_H

#include <stdbool.h>

struct wl_display;

// wlr-output-management, how clients list display modes and switch between them
bool
output_management_create(struct wl_display *display);

// an output's mode changed, clients hear the new state
void
output_management_changed(void);

#endif

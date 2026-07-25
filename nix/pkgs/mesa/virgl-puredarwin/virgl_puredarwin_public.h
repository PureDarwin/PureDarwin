#ifndef VIRGL_PUREDARWIN_PUBLIC_H
#define VIRGL_PUREDARWIN_PUBLIC_H

struct virgl_winsys;
struct sw_winsys;

struct virgl_winsys *virgl_puredarwin_winsys_wrap(struct sw_winsys *sws);

#endif /* VIRGL_PUREDARWIN_PUBLIC_H */

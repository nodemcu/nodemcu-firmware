#ifndef GDBSTUB_H
#define GDBSTUB_H

#ifdef __cplusplus
extern "C" {
#endif

void gdbstub_init();
void gdbstub_redirect_output(int enable);

#ifdef __cplusplus
}
#endif

#endif

//
// Created by alex on 3/21/20.
//

#ifndef LIBWATCHPOINT_WATCHPOINT_H
#define LIBWATCHPOINT_WATCHPOINT_H

#ifdef __cplusplus
extern "C"
{
#endif

void watchpoint_intialize();
void* watchpoint_alloc(size_t size);
void watchpoint_free(void* addr);

#ifdef __cplusplus
};
#endif

#endif //LIBWATCHPOINT_WATCHPOINT_H

//
// Created by alex on 3/21/20.
//

#ifndef LIBWATCHPOINT_WATCHPOINT_H
#define LIBWATCHPOINT_WATCHPOINT_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef void(*watchpoint_callback_t)(const void*, int);

void watchpoint_intialize();
void* watchpoint_alloc(size_t size);
void watchpoint_free(void* addr);
void watch_point_set_callback(watchpoint_callback_t);

#ifdef __cplusplus
};
#endif

#endif //LIBWATCHPOINT_WATCHPOINT_H

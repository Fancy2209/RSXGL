#include "gl_fifo.h"

int32_t __attribute__((noinline))
gcm_reserve_callback(gcmContextData *context, uint32_t count)
{
    typedef int32_t (*CallbackFunc)(gcmContextData *, uint32_t);
    CallbackFunc callback = (CallbackFunc)(context->callback);
    return callback(context, count);
}

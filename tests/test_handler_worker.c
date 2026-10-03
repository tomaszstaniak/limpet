#include "../src/aros_handler.h"
#include <proto/exec.h>
#include <proto/dos.h>
/* Intentionally static: loading the worker privately must isolate this state. */
static ArosSession session;
int main(void)
{
    XHandlerContext *context=aros_handler_context();
    if(!context || aros_session_start_endpoint(&session)) return RETURN_FAIL;
    if(aros_handler_accept(context,&session)) {
        aros_session_stop(&session); aros_session_dispose(&session); return RETURN_FAIL;
    }
    session.line.raw=1;
    while(aros_session_active(&session)) {
        unsigned char bytes[256];
        size_t room=XSESSION_CAPACITY-session.input.length;
        size_t n=aros_session_read(&session,bytes,room<sizeof bytes?room:sizeof bytes);
        session_queue_write(&session.input,bytes,n);
        aros_session_poll(&session);
        if(aros_session_active(&session) && !session.output.length)
            Wait(aros_session_signals(&session));
    }
    return aros_session_dispose(&session)?RETURN_FAIL:RETURN_OK;
}

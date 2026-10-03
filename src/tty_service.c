#include "tty_service.h"
#include <string.h>

static TtyServiceHandle *handle(TtyService *s, TtyHandle id)
{
    size_t i;
    if (!id) return NULL;
    for (i=0;i<TTY_SERVICE_HANDLES;++i)
        if (s->handles[i].id==id) return &s->handles[i];
    return NULL;
}
static void cancel_slot(TtyServiceSlot *slot)
{
    /* A write cancelled after part of it was queued reports that prefix:
       the reader may already have it, and resending all would repeat it. */
    int partial=slot->request->operation==TTY_REQUEST_WRITE && slot->written;
    slot->request->result.status=partial?TTY_OK:TTY_CLOSED;
    slot->request->result.count=partial?slot->written:0;
    slot->request->cancelled=1;
    slot->complete=1;
}
static unsigned operation_lane(TtyOperation op)
{ return op==TTY_REQUEST_READ?0u:1u; }
static int is_drain(TtyOperation op)
{ return op==TTY_REQUEST_DRAIN || op==TTY_REQUEST_DRAIN_CONFIG ||
         op==TTY_REQUEST_DRAIN_CONFIG_DISCARD; }
static TtyResult drain_result(TtyService *s,TtyServiceSlot *slot)
{
    TtyResult result={TTY_AGAIN,0};
    if(!s->pair.refs[TTY_MASTER]) { result.status=TTY_CLOSED;return result; }
    if(slot->request->operation!=TTY_REQUEST_DRAIN &&
       ((slot->config.local_flags&(TTY_ISIG|TTY_NOFLSH)) || slot->config.intr) &&
       (!s->interrupt || slot->signal_generation!=s->signal_generation)) {
        result.status=TTY_UNSUPPORTED;return result;
    }
    if(!slot->drain_started) {
        slot->drain_started=1;
        slot->drain_start=s->pair.output_retired;
        slot->drain_bytes=s->pair.output.count;
    }
    /* Unsigned subtraction handles counter wrap. A barrier covers at most
       one ring; its owner pumps after I/O/flush, not once per counter cycle. */
    if(s->pair.output_retired-slot->drain_start>=slot->drain_bytes) {
        result.status=TTY_OK;
        if(slot->request->operation==TTY_REQUEST_DRAIN_CONFIG)
            result.status=tty_pair_configure(&s->pair,&slot->config);
        else if(slot->request->operation==TTY_REQUEST_DRAIN_CONFIG_DISCARD)
            result.status=tty_pair_configure_discard_input(&s->pair,&slot->config);
    }
    return result;
}
static TtyResult read_result(TtyService *s,TtyServiceSlot *slot)
{
    TtyPair *p=&s->pair;TtyRequest *r=slot->request;TtyResult wait={TTY_AGAIN,0};
    size_t available,minimum;int boundary;unsigned time;
    if(slot->end!=TTY_SLAVE || !r->length || (p->config.local_flags&TTY_CANON)) {
        slot->timer_ds=0;slot->read_started=0;
        return tty_pair_read(p,slot->end,r->buffer,r->length);
    }
    available=tty_pair_input_extent(p,&boundary);
    minimum=p->config.vmin;if(minimum>r->length) minimum=r->length;
    time=p->config.vtime;
    if(!slot->read_started || slot->read_generation!=p->config_generation) {
        slot->read_started=1;slot->read_generation=p->config_generation;
        slot->timer_ds=0;slot->timer_expired=0;slot->read_arrived=p->input_arrived;
    }
    if(boundary || !p->refs[TTY_MASTER] || (available && available>=minimum)) {
        slot->timer_ds=0;return tty_pair_read(p,TTY_SLAVE,r->buffer,r->length);
    }
    if(slot->timer_expired || (!minimum && !time)) {
        slot->timer_ds=0;
        if(available) return tty_pair_read(p,TTY_SLAVE,r->buffer,r->length);
        wait.status=TTY_OK;return wait;
    }
    if(time && (!minimum || available) &&
       (!slot->timer_ds || (minimum && slot->read_arrived!=p->input_arrived))) {
        slot->timer_ds=time;slot->read_arrived=p->input_arrived;
        if(!++s->next_timer) ++s->next_timer;
        slot->timer_token=s->next_timer;
    }
    return wait;
}
unsigned tty_service_read_timer(TtyService *s,TtyRequest **request,uint64_t *token)
{
    size_t i;
    if(!s || !request || !token) return 0;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        TtyServiceSlot *slot=&s->slots[i];
        if(slot->request && !slot->complete && slot->timer_ds) {
            *request=slot->request;*token=slot->timer_token;return slot->timer_ds;
        }
    }
    *request=NULL;*token=0;return 0;
}
void tty_service_read_timeout(TtyService *s,TtyRequest *request,uint64_t token)
{
    size_t i;if(!s || !request) return;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        TtyServiceSlot *slot=&s->slots[i];
        if(slot->request==request && !slot->complete && slot->timer_ds &&
           slot->timer_token==token) { slot->timer_expired=1;break; }
    }
    tty_service_pump(s);
}
void tty_service_init(TtyService *s)
{
    memset(s,0,sizeof *s);
    tty_pair_init(&s->pair);
    s->handles[0].id=1;s->handles[0].end=TTY_MASTER;
    s->handles[1].id=2;s->handles[1].end=TTY_SLAVE;
    s->next_handle=3;s->next_order=1;
}
TtyStatus tty_service_duplicate(TtyService *s,TtyHandle id,TtyHandle *out)
{
    TtyServiceHandle *h;size_t i;TtyStatus status;
    if (!s || !out) return TTY_INVALID;
    h=handle(s,id);
    if (!h) return TTY_CLOSED;
    if (s->next_handle==UINT64_MAX) return TTY_BUSY;
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(!s->handles[i].id) break;
    if(i==TTY_SERVICE_HANDLES) return TTY_BUSY;
    status=tty_pair_retain(&s->pair,h->end);
    if(status!=TTY_OK) return status;
    s->handles[i].end=h->end;s->handles[i].id=s->next_handle++;
    *out=s->handles[i].id;
    return TTY_OK;
}
TtyStatus tty_service_close(TtyService *s,TtyHandle id)
{
    TtyServiceHandle *h;size_t i;TtyStatus status;
    if(!s) return TTY_INVALID;
    h=handle(s,id);
    if(!h) return TTY_CLOSED;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i)
        if(s->slots[i].request && !s->slots[i].complete &&
           s->slots[i].request->handle==id) cancel_slot(&s->slots[i]);
    status=tty_pair_release(&s->pair,h->end);
    h->id=0;
    tty_service_pump(s);
    return status;
}
void tty_service_pump(TtyService *s)
{
    int progress;
    if(!s) return;
    do {
        unsigned blocked=0;uint64_t after=0;
        progress=0;
        for (;;) {
            TtyServiceSlot *slot=NULL;TtyRequest *r;TtyResult result;
            size_t i;unsigned lane;
            /* Dispatch before another request observes a signal-induced flush.
               Without an owner, retain the event and yield all progress. */
            if(s->pair.interrupt_pending) {
                if(!s->interrupt) return;
                tty_pair_take_interrupt(&s->pair);
                s->interrupt(s->interrupt_context);
            }
            /* Physical slot reuse must not change submission FIFO order. */
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(s->slots[i].request && !s->slots[i].complete &&
                   s->slots[i].order>after &&
                   (!slot || s->slots[i].order<slot->order)) slot=&s->slots[i];
            if(!slot) break;
            after=slot->order;r=slot->request;
            lane=1u<<((unsigned)slot->end*2+operation_lane(r->operation));
            /* Empty I/O cannot consume/reorder bytes and must not inherit
               another request's wait for data or queue capacity. */
            if((blocked&lane) && (r->length || is_drain(r->operation))) continue;
            result=is_drain(r->operation)?drain_result(s,slot):
                r->operation==TTY_REQUEST_READ?
                read_result(s,slot):
                tty_pair_write(&s->pair,slot->end,(const unsigned char *)r->buffer+slot->written,
                               r->length-slot->written);
            /* A blocking slave write completes only when every byte is queued,
               as on a POSIX tty: Write() callers do not retry a short count,
               and the output queue is far smaller than one image chunk. */
            if(r->operation==TTY_REQUEST_WRITE && slot->end==TTY_SLAVE) {
                if(result.status==TTY_OK) slot->written+=result.count;
                if(result.status==TTY_OK && slot->written<r->length) {
                    progress|=result.count!=0;blocked|=lane;continue;
                }
                if(result.status==TTY_AGAIN && slot->written<r->length) { blocked|=lane;continue; }
                if(slot->written) { result.status=TTY_OK;result.count=slot->written; }
            }
            if(result.status==TTY_AGAIN) { blocked|=lane;continue; }
            r->result=result;slot->complete=1;progress=1;
        }
    } while(progress);
}
TtyStatus tty_service_submit(TtyService *s,TtyRequest *r)
{
    TtyServiceHandle *h;TtyServiceSlot *free_slot=NULL;size_t i,lane_count=0;
    TtyConfig config;TtyStatus status;
    if(!s || !r) return TTY_INVALID;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        if(s->slots[i].request==r) return TTY_BUSY;
        if(!s->slots[i].request && !free_slot) free_slot=&s->slots[i];
    }
    if(r->operation!=TTY_REQUEST_READ && r->operation!=TTY_REQUEST_WRITE &&
       !is_drain(r->operation))
        return TTY_INVALID;
    if(r->operation==TTY_REQUEST_DRAIN && (r->buffer || r->length)) return TTY_INVALID;
    if(!r->buffer && r->length) return TTY_INVALID;
    if(r->operation==TTY_REQUEST_DRAIN_CONFIG || r->operation==TTY_REQUEST_DRAIN_CONFIG_DISCARD) {
        if(!r->buffer || r->length!=sizeof config) return TTY_INVALID;
        memcpy(&config,r->buffer,sizeof config);
        status=tty_config_validate(&config);if(status!=TTY_OK) return status;
        if(((config.local_flags&(TTY_ISIG|TTY_NOFLSH)) || config.intr) && !s->interrupt)
            return TTY_UNSUPPORTED;
    }
    h=handle(s,r->handle);
    if(!h) return TTY_CLOSED;
    if(is_drain(r->operation) && h->end!=TTY_SLAVE) return TTY_UNSUPPORTED;
    /* Reserve space for each opposing producer/consumer, including when a
       caller has not yet collected completed requests in another lane. */
    for(i=0;i<TTY_SERVICE_REQUESTS;++i)
        if(s->slots[i].request && s->slots[i].end==h->end &&
           operation_lane(s->slots[i].request->operation)==operation_lane(r->operation)) ++lane_count;
    if(lane_count==TTY_SERVICE_LANE_REQUESTS) return TTY_BUSY;
    if(!free_slot || s->next_order==UINT64_MAX) return TTY_BUSY;
    free_slot->request=r;free_slot->end=h->end;
    free_slot->order=s->next_order++;free_slot->complete=0;
    free_slot->written=0;
    free_slot->signal_generation=s->signal_generation;
    if(r->operation==TTY_REQUEST_DRAIN_CONFIG || r->operation==TTY_REQUEST_DRAIN_CONFIG_DISCARD)
        free_slot->config=config;
    r->result.status=TTY_AGAIN;r->result.count=0;r->cancelled=0;
    tty_service_pump(s);
    return TTY_OK;
}
TtyStatus tty_service_cancel(TtyService *s,TtyRequest *r)
{
    size_t i;
    if(!s || !r) return TTY_INVALID;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(s->slots[i].request==r) {
        if(s->slots[i].complete) return TTY_BUSY;
        cancel_slot(&s->slots[i]);tty_service_pump(s);return TTY_OK;
    }
    return TTY_INVALID;
}
TtyResult tty_service_try_io(TtyService *s,TtyHandle id,TtyOperation op,void *buffer,size_t length)
{
    TtyResult result={TTY_INVALID,0};TtyServiceHandle *h;size_t i;
    if(!s || (op!=TTY_REQUEST_READ && op!=TTY_REQUEST_WRITE) ||
       (!buffer && length)) return result;
    h=handle(s,id);
    if(!h) { result.status=TTY_CLOSED;return result; }
    if(!length) { result.status=TTY_OK;return result; }
    /* Advance older work first, including dispatch of an already accepted
       interrupt. No caller buffer is installed into the pending queue. */
    tty_service_pump(s);
    result.status=TTY_AGAIN;
    if(s->pair.interrupt_pending) return result;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        const TtyServiceSlot *slot=&s->slots[i];
        if(slot->request && !slot->complete && slot->end==h->end &&
           operation_lane(slot->request->operation)==operation_lane(op))
            return result;
    }
    /* Pair reads implement record/EOF semantics without a VMIN/VTIME wait. */
    result=op==TTY_REQUEST_READ?tty_pair_read(&s->pair,h->end,buffer,length):
        tty_pair_write(&s->pair,h->end,buffer,length);
    /* A transfer can unblock older work in the opposing lane or accept VINTR.
       Dispatch/pump before another operation observes those effects. */
    tty_service_pump(s);
    return result;
}
size_t tty_service_cancel_all(TtyService *s)
{
    size_t i,count=0;
    if(!s) return 0;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i)
        if(s->slots[i].request && !s->slots[i].complete) {
            cancel_slot(&s->slots[i]);++count;
        }
    return count;
}
TtyRequest *tty_service_take(TtyService *s)
{
    TtyServiceSlot *slot=NULL;TtyRequest *r;size_t i;
    if(!s) return NULL;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i)
        if(s->slots[i].request && s->slots[i].complete &&
           (!slot || s->slots[i].order<slot->order)) slot=&s->slots[i];
    if(!slot) return NULL;
    r=slot->request;memset(slot,0,sizeof *slot);return r;
}
int tty_service_retired(const TtyService *s)
{
    size_t i;
    if(!s) return 0;
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(s->handles[i].id) return 0;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(s->slots[i].request) return 0;
    return 1;
}

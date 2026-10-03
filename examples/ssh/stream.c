#include "stream.h"
#include "pump.h"
#include "term_query.h"
#include "../../src/aros_tty_client.h"
#include <libssh/callbacks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <string.h>

struct stream {
    ssh_session session;
    ssh_channel channel;
    BPTR input,output[2];
    XtTermQuery query;
    XtTermSnapshot status;
    int have_status; /* status is from this loop's query */
    int received_completion;
    unsigned columns,rows;
};

/* Both packets go to the input handle's handler and are answered at once.
 * Never leave a native packet pending across network I/O. */
static int status_packet(void *context,XTermStatus *status)
{
    struct FileHandle *fh=BADDR(((struct stream *)context)->input);
    if(!fh || !fh->fh_Type) return 0;
    if(DoPkt(fh->fh_Type,XTERM_ACTION_QUERY,fh->fh_Arg1,(IPTR)status,
             sizeof *status,0,0)==DOSTRUE) return 1;
    return IoErr()==ERROR_ACTION_NOT_KNOWN?0:-1;
}
static int control_packet(void *context,XtyControl *control)
{
    BPTR input=((struct stream *)context)->input;
    if(!input || !IsInteractive(input)) return 0;
    if(aros_tty_control(input,control)==DOSTRUE) return 1;
    return IoErr()==ERROR_ACTION_NOT_KNOWN?0:-1;
}

/* A snapshot, not an input reservation. SSH is the sole reader of this
 * foreground stream. 1 snapshot, 0 none (CON: and others), -1 failure. */
static int query_input(struct stream *s)
{
    XtTermOps ops={status_packet,control_packet,s};
    int rc=xt_term_query(&s->query,&ops,&s->status);
    s->have_status=rc>0;
    return rc;
}

void xt_ssh_dimensions(unsigned *columns,unsigned *rows)
{
    struct stream s;
    memset(&s,0,sizeof s); s.input=Input();
    *columns=80; *rows=24;
    if(query_input(&s)>0 && s.status.columns && s.status.rows &&
       s.status.columns<=65535 && s.status.rows<=65535) {
        *columns=s.status.columns; *rows=s.status.rows;
    }
}

static int read_remote(void *context,int stream,unsigned char *data,size_t size)
{
    struct stream *s=context;
    int n=ssh_channel_read_nonblocking(s->channel,data,(uint32_t)size,stream);
    return n==SSH_AGAIN || n==SSH_EOF?0:n;
}
static int write_remote(void *context,const unsigned char *data,size_t size)
{
    struct stream *s=context;
    int n=ssh_channel_write(s->channel,data,(uint32_t)size);
    return n==SSH_AGAIN?0:n;
}
static int read_local(void *context,unsigned char *data,size_t size)
{
    struct stream *s=context;
    LONG n;
    /* The control step refreshes this snapshot once per loop. SSH is the
     * sole input consumer, and the pump invokes local_read at most once. */
    if(s->have_status) {
        if(!s->status.data) return s->status.eof?SSH_PUMP_INPUT_EOF:0;
        if(s->status.counted) {
            if(size>s->status.available) size=s->status.available;
        } else if(s->status.try_io) {
            /* XTY read STATE says "some", not how many: take what is there
             * without waiting. AGAIN is not EOF. */
            n=aros_tty_read_try(s->input,data,(LONG)size);
            if(n<0) return IoErr()==ERROR_OBJECT_IN_USE?0:-1;
            return n==0?SSH_PUMP_INPUT_EOF:(int)n;
        } else size=1; /* DATA guarantees one byte, no more */
    } else {
        if(!WaitForChar(s->input,0)) return IoErr()? -1:0;
        /* Unknown handlers need not return short raw reads. */
        size=1;
    }
    n=Read(s->input,data,(LONG)size);
    return n==0?SSH_PUMP_INPUT_EOF:(int)n;
}
static int write_local(void *context,int stream,const unsigned char *data,size_t size)
{
    struct stream *s=context;
    /* Native DOS output is synchronous: bounded byte count does not remove
     * physical sink backpressure. A handler may block this Write. */
    return (int)Write(s->output[stream],data,(LONG)size);
}
static int send_eof(void *context)
{
    struct stream *s=context;
    int rc=ssh_channel_send_eof(s->channel);
    return rc==SSH_OK?1:rc==SSH_AGAIN?0:-1;
}
static int remote_done(void *context)
{
    struct stream *s=context;
    if(s->received_completion) return 1;
    if(!ssh_is_connected(s->session)) return -1;
    return ssh_channel_is_eof(s->channel) || !ssh_channel_is_open(s->channel);
}

static void received_completion(ssh_session session,ssh_channel channel,void *context)
{
    struct stream *s=context;
    (void)session; (void)channel;
    s->received_completion=1;
}

static int poll_control(struct stream *s)
{
    int rc=query_input(s);
    if(rc<=0) return rc;
    if(!s->received_completion && s->status.columns && s->status.rows &&
       s->status.columns<=65535 && s->status.rows<=65535 &&
       (s->status.columns!=s->columns || s->status.rows!=s->rows)) {
        rc=ssh_channel_change_pty_size(s->channel,s->status.columns,s->status.rows);
        /* A resize can also process remote CLOSE; still drain its output. */
        if(rc==SSH_ERROR && !s->received_completion) return -1;
        if(rc==SSH_OK) { s->columns=s->status.columns; s->rows=s->status.rows; }
    }
    return 0;
}

int xt_ssh_stream(ssh_session session,ssh_channel channel,const char *command)
{
    static const struct ssh_pump_ops ops={
        read_remote,write_remote,read_local,write_local,send_eof,remote_done
    };
    struct stream s;
    struct ssh_channel_callbacks_struct callbacks;
    struct ssh_pump pump;
    int rc,progress,exit_code=RETURN_FAIL;
    int interactive=command==NULL,raw=0;
    const char *failure=NULL;
    unsigned idle_eof=0;
    memset(&s,0,sizeof s);
    s.session=session; s.channel=channel; s.input=Input(); s.output[0]=Output();
    s.output[1]=((struct Process *)FindTask(NULL))->pr_CES;
    if(!s.output[1] || interactive) s.output[1]=s.output[0];
    /* EOF/CLOSE callbacks fire even while unread buffers conceal completion
     * in is_eof/is_open. Register before requests can process peer packets. */
    memset(&callbacks,0,sizeof callbacks);
    callbacks.userdata=&s;
    callbacks.channel_eof_function=received_completion;
    callbacks.channel_close_function=received_completion;
    ssh_callbacks_init(&callbacks);
    if(ssh_set_channel_callbacks(channel,&callbacks)!=SSH_OK) return RETURN_FAIL;
    if(command) {
        if(ssh_channel_request_exec(channel,command)!=SSH_OK) {
            Printf("ssh: Exec failed: %s\n",(IPTR)ssh_get_error(session));
            goto cleanup;
        }
    } else {
        /* RFC 4254 section 8: explicit remote cooked settings. Mainline's
         * tcgetattr only fills ICANON; the remote app may select raw later. */
        static const unsigned char modes[]={
            1,0,0,0,3,    /* VINTR Ctrl-C */
            2,0,0,0,28,   /* VQUIT Ctrl-backslash */
            3,0,0,0,127,  /* VERASE DEL */
            4,0,0,0,21,   /* VKILL Ctrl-U */
            5,0,0,0,4,    /* VEOF Ctrl-D */
            36,0,0,0,1,   /* ICRNL */
            50,0,0,0,1,   /* ISIG */
            51,0,0,0,1,   /* ICANON */
            53,0,0,0,1,   /* ECHO */
            54,0,0,0,1,   /* ECHOE */
            55,0,0,0,1,   /* ECHOK */
            70,0,0,0,1,   /* OPOST */
            72,0,0,0,1,   /* ONLCR */
            0
        };
        xt_ssh_dimensions(&s.columns,&s.rows);
        if(ssh_channel_request_pty_size_modes(channel,"xterm",s.columns,s.rows,
                                             modes,sizeof modes)!=SSH_OK ||
           ssh_channel_request_shell(channel)!=SSH_OK) {
            Printf("ssh: Shell request failed: %s\n",(IPTR)ssh_get_error(session));
            goto cleanup;
        }
        if(!SetMode(s.input,1)) goto cleanup;
        raw=1;
    }
    ssh_set_blocking(session,0);
    ssh_pump_init(&pump,!interactive);
    do {
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C) {
            failure="cancelled by local process break";
            rc=SSH_PUMP_ERROR; break;
        }
        /* Geometry remains live with a pending input tail or after local EOF. */
        if(interactive && poll_control(&s)<0) {
            failure="native input status or remote resize failed";
            rc=SSH_PUMP_ERROR; break;
        }
        rc=ssh_pump_step(&pump,&ops,&s,&progress);
        if(rc==SSH_PUMP_ERROR) failure="transport or local I/O failed";
        if(rc==SSH_PUMP_ACTIVE && !progress) {
            /* This is an idle grace, not a global wall timeout: progress
             * resets it. Observe native EOF even with blocked pending input.
             * Expiry cancels with failure and may discard unsent bytes. */
            if(interactive && (pump.local_eof || (s.have_status && s.status.eof)) &&
               ++idle_eof>=250) {
                failure="local input EOF idle grace period expired";
                rc=SSH_PUMP_ERROR; break;
            }
            Delay(1);
        } else if(progress) idle_eof=0;
    } while(rc==SSH_PUMP_ACTIVE);
    if(rc==SSH_PUMP_DONE) {
        unsigned tick;
        uint32_t code=0;
        failure="remote exit status unavailable";
        for(tick=0;tick<250;++tick) {
            int state=ssh_channel_get_exit_state(channel,&code,NULL,NULL);
            if(state==SSH_OK) { exit_code=(int)code; failure=NULL; break; }
            if(state!=SSH_AGAIN || !ssh_channel_is_open(channel)) break;
            if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C) {
                failure="cancelled by local process break"; break;
            }
            Delay(1);
        }
    }
cleanup:
    if(raw && !SetMode(s.input,0)) exit_code=RETURN_FAIL;
    ssh_remove_channel_callbacks(channel,&callbacks);
    /* Keep the session nonblocking through channel cleanup/disconnect. */
    ssh_set_blocking(session,0);
    if(failure) FPrintf(s.output[1],"ssh: %s\n",(IPTR)failure);
    return exit_code;
}

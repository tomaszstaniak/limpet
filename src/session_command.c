#include "session_command.h"
int session_quote_path(const char *path,char *command,size_t capacity)
{
    size_t i,n=0;
    if(!path || !*path || !command || capacity<3) return -1;
    command[n++]='"';
    for(i=0;path[i];++i) {
        if(n+3>=capacity || path[i]=='\r' || path[i]=='\n' ||
           path[i]=='$' || path[i]=='`' || path[i]=='<') return -1;
        if(path[i]=='*' || path[i]=='"') command[n++]='*';
        command[n++]=path[i];
    }
    command[n++]='"'; command[n]=0;
    return 0;
}

/* Output completeness matrix for the PTY frontend. Built eight ways:
   -DMATRIX_LIBC (posixc stdio on stdout; default plain dos.library FPuts on
   Output()), -DMATRIX_NEWLINE (text ends in a newline), -DMATRIX_FLUSH (the
   program flushes before it returns; otherwise normal exit only). Each prints
   two lines and an END# marker whose last byte ('#', or the newline after it)
   must reach the terminal. */
#include <proto/dos.h>
#ifdef MATRIX_LIBC
#include <stdio.h>
#define API "libc"
#else
#define API "dos"
#endif
#ifdef MATRIX_NEWLINE
#define ENDING "nl"
#define LAST "\n"
#else
#define ENDING "nonl"
#define LAST ""
#endif
#ifdef MATRIX_FLUSH
#define MODE "flush"
#else
#define MODE "exit"
#endif
#define NAME API "-" ENDING "-" MODE

int main(void)
{
    static const char text[] =
        "MATRIX " NAME " 1\nMATRIX " NAME " 2\nMATRIX " NAME " END#" LAST;
#ifdef MATRIX_LIBC
    fputs(text, stdout);
#ifdef MATRIX_FLUSH
    fflush(stdout);
#endif
#else
    FPuts(Output(), (CONST_STRPTR)text);
#ifdef MATRIX_FLUSH
    Flush(Output());
#endif
#endif
    return RETURN_OK;
}

#include "input.h"
#include <stdio.h>
#include <setjmp.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct photon_termkey {
    char prefix;
    int P[16];
    int n;
} photon_termkey_t;

static jmp_buf err_handler;

#define BELL() (putchar(7), fflush(stdout))

static int _photon_getch(void){
    int ch = getchar();
    if (ch == EOF)
        longjmp(err_handler, 67);
    return ch;
}

static int _photon_handle_tilde(const photon_termkey_t *key){
    if (key->n < 1) return PHOTON_INVALID_KEY;
    int base;
    switch(*key->P){
    case 1:
        base = PHOTON_KHOME;
        break;
    case 4:
        base = PHOTON_KEND;
        break;
    default:
        base = PHOTON_INVALID_KEY;
        break;
    }

    return base;
}

int photon_input_read_key(void){
    if (setjmp(err_handler)){
        BELL();
        return EOF;
    }
    int ch = _photon_getch();
    if (ch == 27){
        if (_photon_getch() != '[') goto err;
        photon_termkey_t key = {0};
        int p = 0;
        char numBuffer[16];
        int err = 0;
        while (1){
            ch = _photon_getch();
            if (isdigit(ch)){
                if (p == sizeof(numBuffer)) // HOW THE FUCK?? AN INT DOESNT EVEN FIT IN 16 BYTES ITS TOO BIG
                    err = 1;
                numBuffer[p++] = ch;
                continue;
            }
            if (ch == ';' || (ch >= 0x40 && ch <= 0x7E)){
                if (p == 0 && (ch == ';' || key.n)) {
                    err = 1;
                } else {
                    numBuffer[p] = 0;
                    p = 0;
                    // HOW???
                    if (key.n == sizeof(key.P)/sizeof(key.P[0]))
                        err = 1;
                    else key.P[key.n++] = atoi(numBuffer);
                }
                if (ch != ';') {
                    key.prefix = ch;
                    break;
                }
                continue;
            }
            err = 1;
        }
        int val = PHOTON_INVALID_KEY;
        if (err) goto skip;
        switch(key.prefix){
        case 'A':
            val = PHOTON_KUP;
            break;
        case 'B':
            val = PHOTON_KDOWN;
            break;
        case 'C':
            val = PHOTON_KRIGHT;
            break;
        case 'D':
            val = PHOTON_KLEFT;
            break;
        case 'H':
            val = PHOTON_KHOME;
            break;
        case 'F':
            val = PHOTON_KEND;
            break;
        case '~':
            val = _photon_handle_tilde(&key);
            break;
        default: goto err;
        }
        goto skip;
    err:
        val = PHOTON_INVALID_KEY;
    skip:
        if (val == PHOTON_INVALID_KEY) BELL();
        return val;
    }
    return ch;
}


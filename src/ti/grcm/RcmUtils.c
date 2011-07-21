/*
 * Copyright (c) 2011, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 *  ======== RcmUtils.c ========
 *
 */

#include <xdc/std.h>
#include "RcmTypes.h"


/*
 *  ======== memset ========
 */
Void *_memset(Void *s, Int c, Int n)
{
    UChar *p = (UChar *)s;

    while (n-- > 0) {
        *p++ = (UChar)c;
    }

    return(s);
}


/*
 *  ======== _strcpy ========
 *  Copy t to s, including null character.
 */
Void _strcpy(Char *s, Char *t)
{
    while ((*s++ = *t++))
        ;
}


/*
 *  ======== _strcmp ========
 *  Return <0 if s<t, 0 if s==t, >0 if s>t
 */
Int _strcmp(Char *s, Char *t)
{
    for ( ; *s == *t; s++, t++) {
        if (*s == '\0') {
            return(0);
        }
    }
    return(*s - *t);
}


/*
 *  ======== _strlen ========
 *  Return length of s
 */
Int _strlen(const Char *s)
{
    Int n;

    for (n = 0; *s != '\0'; s++) {
        n++;
    }
    return(n);
}

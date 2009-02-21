/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>
#include "nst_wince.h"

#define SHM_NAME "/tmp/counter"
#define SEM_NAME1 "/tmp/foo.sem"
#define SEM_NAME2 "/tmp/bar.sem"
#define SEM_MODE  0666
#define SHM_MODE  0666
#define ITERATIONS 1000

static PRBool debug_mode = PR_FALSE;
static PRIntn iterations = ITERATIONS;
static PRSem *sem1, *sem2;

static void Help(void)
{
    fprintf(stderr, "semaping test program usage:\n");
    fprintf(stderr, "\t-d           debug mode         (FALSE)\n");
    fprintf(stderr, "\t-c <count>   loop count         (%d)\n", ITERATIONS);
    fprintf(stderr, "\t-h           this message\n");
}  /* Help */

int main(int argc, char **argv)
{
    PRProcess *proc;
    PRIntn i;
    char *child_argv[32];
    char **child_arg;
    char iterations_buf[32];
    PRSharedMemory *shm;
    PRIntn *counter_addr;
    PRInt32 exit_code;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dc:h");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option) {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'c':  /* loop count */
                iterations = atoi(opt->value);
                break;
            case 'h':
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    if (PR_DeleteSharedMemory(SHM_NAME) == PR_SUCCESS) {
        fprintf(stderr, "warning: removed shared memory %s left over "
                "from previous run\n", SHM_NAME);
    }
    if (PR_DeleteSemaphore(SEM_NAME1) == PR_SUCCESS) {
        fprintf(stderr, "warning: removed semaphore %s left over "
                "from previous run\n", SEM_NAME1);
    }
    if (PR_DeleteSemaphore(SEM_NAME2) == PR_SUCCESS) {
        fprintf(stderr, "warning: removed semaphore %s left over "
                "from previous run\n", SEM_NAME2);
    }

    shm = PR_OpenSharedMemory(SHM_NAME, sizeof(*counter_addr), PR_SHM_CREATE, SHM_MODE);
    if (NULL == shm) {
        fprintf(stderr, "PR_OpenSharedMemory failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    counter_addr = PR_AttachSharedMemory(shm, 0);
    if (NULL == counter_addr) {
        fprintf(stderr, "PR_AttachSharedMemory failed\n");
        exit(1);
    }
    *counter_addr = 0;
    sem1 = PR_OpenSemaphore(SEM_NAME1, PR_SEM_CREATE, SEM_MODE, 1);
    if (NULL == sem1) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    sem2 = PR_OpenSemaphore(SEM_NAME2, PR_SEM_CREATE, SEM_MODE, 0);
    if (NULL == sem2) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }

    child_arg = &child_argv[0];
    *child_arg++ = "semapong";
    if (debug_mode != PR_FALSE) {
        *child_arg++ = "-d";
    }
    if (iterations != ITERATIONS) {
        *child_arg++ = "-c";
        PR_snprintf(iterations_buf, sizeof(iterations_buf), "%d", iterations);
        *child_arg++ = iterations_buf;
    }
    *child_arg = NULL;
    proc = PR_CreateProcess(child_argv[0], child_argv, NULL, NULL);
    if (NULL == proc) {
        fprintf(stderr, "PR_CreateProcess failed\n");
        exit(1);
    }

    /*
     * Process 1 waits on semaphore 1 and posts to semaphore 2.
     */
    for (i = 0; i < iterations; i++) {
        if (PR_WaitSemaphore(sem1) == PR_FAILURE) {
            fprintf(stderr, "PR_WaitSemaphore failed\n");
            exit(1);
        }
        if (*counter_addr == 2*i) {
            if (debug_mode) printf("process 1: counter = %d\n", *counter_addr);
        } else {
            fprintf(stderr, "process 1: counter should be %d but is %d\n",
                    2*i, *counter_addr);
            exit(1);
        }
        (*counter_addr)++;
        if (PR_PostSemaphore(sem2) == PR_FAILURE) {
            fprintf(stderr, "PR_PostSemaphore failed\n");
            exit(1);
        }
    }
    if (PR_DetachSharedMemory(shm, counter_addr) == PR_FAILURE) {
        fprintf(stderr, "PR_DetachSharedMemory failed\n");
        exit(1);
    }
    if (PR_CloseSharedMemory(shm) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSharedMemory failed\n");
        exit(1);
    }
    if (PR_CloseSemaphore(sem1) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
    }
    if (PR_CloseSemaphore(sem2) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
    }

    if (PR_WaitProcess(proc, &exit_code) == PR_FAILURE) {
        fprintf(stderr, "PR_WaitProcess failed\n");
        exit(1);
    }
    if (exit_code != 0) {
        fprintf(stderr, "process 2 failed with exit code %d\n", exit_code);
        exit(1);
    }

    if (PR_DeleteSharedMemory(SHM_NAME) == PR_FAILURE) {
        fprintf(stderr, "PR_DeleteSharedMemory failed\n");
    }
    if (PR_DeleteSemaphore(SEM_NAME1) == PR_FAILURE) {
        fprintf(stderr, "PR_DeleteSemaphore failed\n");
    }
    if (PR_DeleteSemaphore(SEM_NAME2) == PR_FAILURE) {
        fprintf(stderr, "PR_DeleteSemaphore failed\n");
    }
    printf("PASS\n");
    return 0;
}

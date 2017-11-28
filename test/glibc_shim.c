/**
 * Copyright (c) 2014 Peter Leese
 *
 * Licensed under the GPL License. See LICENSE file in the project root for full license information.  
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef int (*open_t)(const char *, int, ...);
typedef DIR * (*opendir_t)(const char *);


static open_t p_open = NULL;
static opendir_t p_opendir = NULL;


static const char * tmp_test_dir = NULL;
static int sock_fd = 0;
static struct sockaddr_un from_mock_addr; 

#define ENTER_MOCK init()

static void init(void) 
{
    if(!p_open) {
        p_open = (open_t) dlsym(RTLD_NEXT, "open");
        p_opendir = (opendir_t) dlsym(RTLD_NEXT, "opendir");

        tmp_test_dir = getenv("TMP_TEST_DIR");
        const char * sock_name = getenv("TMP_MOCK_FROM");
        if(sock_name) {
            sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
            from_mock_addr.sun_family = AF_UNIX;
            strncpy(from_mock_addr.sun_path, sock_name,
                    sizeof(from_mock_addr.sun_path));
            from_mock_addr.sun_path[sizeof(from_mock_addr.sun_path)-1] = '\0';
        }
    }
}

/**
 * Log an event, the events are sent either over a UNIX socket if it exits of
 * to stderr
 *
 * @param[in] fmt Standard printf options
 */
static void log_event(const char * fmt, ...)
{
    char buf[512];

    int n  = sprintf(buf, "%i:", getpid());
    va_list ap;
    va_start(ap, fmt);
    n = n + vsnprintf(&buf[n], sizeof(buf)-n, fmt, ap);
    va_end(ap);

    if(n >= (int) sizeof(buf)) {
        n = sizeof(buf)-1;
        buf[n] ='\0';
    }
    if(buf[n-1] != '\n') {
        if(n < (int) sizeof(buf)-1) {
            n++;
        }
        buf[n-1] = '\n';
        buf[n] = '\0';
    }
    if(sock_fd) {
        ssize_t len = sendto(sock_fd, buf, n, 0,
                    (const struct sockaddr *)&from_mock_addr,
                    sizeof(from_mock_addr));
        if(len == n) {
            return;
        }
    }
    fputs(buf, stderr);
}

/**
 * Modify any path so that it points to the temporary test directory as opposed
 * to the /proc or /sys filesystem
 *
 * @param[in] pathname The original pathname
 *
 * @return The new pathname (allocated from the heap so must be freed)
 */
const char * modify_path(const char * pathname)
{
    size_t len = strlen(pathname);
    if(tmp_test_dir) {
        len += strlen(tmp_test_dir);
    }
    else {
        log_event("Failed no TMP_TEST_DIR");
        exit(EXIT_FAILURE);
    }
    char * retVal = (char *) malloc(len+1);
    if(retVal) {
        if(tmp_test_dir) {
            sprintf(retVal, "%s%s", tmp_test_dir, pathname);
        }
        else {
            strcpy(retVal, pathname);
        }
    }
    return retVal;
}

/**
 * mock for the open API
 */
int open(const char * pathname, int flags, ...)
{
    ENTER_MOCK;

    log_event("open(%s, %x)", pathname, flags);

    pathname = modify_path(pathname);
    
    int retVal;
    if((flags & (O_CREAT | O_TMPFILE)) != 0) {
        va_list ap;
        va_start(ap, flags);
        const mode_t mode = va_arg(ap, mode_t);
        va_end(ap);

        retVal = p_open(pathname, flags, mode);
    }
    else {
        retVal = p_open(pathname, flags);
    }
    free((void *)pathname);
    return retVal;
}

/**
 * mock for the opendir API
 */
DIR * opendir(const char * name)
{
    ENTER_MOCK;

    log_event("opendir(%s)", name);

    name = modify_path(name);
    DIR * retVal= p_opendir(name);
    free((void *) name);
    return retVal;
}

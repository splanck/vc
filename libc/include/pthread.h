#ifndef VC_PTHREAD_H
#define VC_PTHREAD_H

/* Minimal pthread stubs for vc's single-threaded libc. */

typedef int pthread_t;
typedef int pthread_attr_t;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);

#endif /* VC_PTHREAD_H */

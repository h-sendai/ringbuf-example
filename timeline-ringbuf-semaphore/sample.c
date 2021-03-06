#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "set_cpu.h"

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_BUF 10000
//#define MAX_LOOP_COUNT 10000000
#define MAX_LOOP_COUNT 100000

int buf[MAX_BUF];
int debug       = 0;

sem_t n_stored;
sem_t n_empty;

int writer_cpu = -1;
int reader_cpu = -1;

int usage()
{
    char msg[] = "sample [-c writer_cpu] [-C reader_cpu]";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

struct ts {
    struct timespec *ts0;
    struct timespec *ts1;
} ts;

void *writer(void *arg)
{
    int loop_counter = 0;
    int write_index = 0;
    
    struct ts *ts = (struct ts *)arg;
    //struct timespec *ts[2];
    //struct timespec *writer_0_tv = ((struct timespec *)arg)[0];
    //struct timespec *writer_1_tv = ((struct timespec *)arg)[1];
    struct timespec *writer_0_tv = ts->ts0;
    struct timespec *writer_1_tv = ts->ts1;

    if (writer_cpu != -1) {
        set_cpu(writer_cpu);
    }

    while (loop_counter < MAX_LOOP_COUNT) {
        sem_wait(&n_empty);
        buf[write_index] = write_index;
        clock_gettime(CLOCK_MONOTONIC, &writer_0_tv[loop_counter]);
        sem_post(&n_stored);
        clock_gettime(CLOCK_MONOTONIC, &writer_1_tv[loop_counter]);
        write_index ++;
        if (write_index >= MAX_BUF) {
            write_index = 0;
        }
        loop_counter ++;
    }

    return NULL;
}

void *reader(void *arg)
{
    int loop_counter = 0;
    int read_index  = 0;

    struct timespec *reader_tv = (struct timespec *)arg;

    if (reader_cpu != -1) {
        set_cpu(reader_cpu);
    }

    while (loop_counter < MAX_LOOP_COUNT) {
        sem_wait(&n_stored);
        clock_gettime(CLOCK_MONOTONIC, &reader_tv[loop_counter]);
        if (buf[read_index] != read_index) {
            errx(1, "does not match");
        }
        buf[read_index] = -1;
        sem_post(&n_empty);
        read_index ++;
        if (read_index >= MAX_BUF) {
            read_index = 0;
        }
        loop_counter ++;
    }

    return NULL;
}


int main(int argc, char *argv[])
{
    int s;
    pthread_t reader_id;
    pthread_t writer_id;

    int c;
    while ( (c = getopt(argc, argv, "hc:C:")) != -1) {
        switch (c) {
            case 'h':
                usage();
                exit(0);
            case 'c':
                writer_cpu = strtol(optarg, NULL, 0);
                break;
            case 'C':
                reader_cpu = strtol(optarg, NULL, 0);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    sem_init(&n_stored, 0, 0);
    sem_init(&n_empty,  0, MAX_BUF);

    struct timespec *reader_tv   = malloc(sizeof(struct timespec)*MAX_LOOP_COUNT);
    struct timespec *writer_0_tv = malloc(sizeof(struct timespec)*MAX_LOOP_COUNT);
    struct timespec *writer_1_tv = malloc(sizeof(struct timespec)*MAX_LOOP_COUNT);
    memset(reader_tv,   'X', sizeof(struct timespec)*MAX_LOOP_COUNT);
    memset(writer_0_tv, 'X', sizeof(struct timespec)*MAX_LOOP_COUNT);
    memset(writer_1_tv, 'X', sizeof(struct timespec)*MAX_LOOP_COUNT);

    s = pthread_create(&reader_id, 0, reader, (void *)reader_tv);
    if (s != 0) {
        handle_error_en(s, "pthread_create");
    }
    ts.ts0 = writer_0_tv;
    ts.ts1 = writer_1_tv;
    s = pthread_create(&writer_id, 0, writer, (void *)&ts);
    if (s != 0) {
        handle_error_en(s, "pthread_create");
    }

    s = pthread_join(writer_id, 0);
    if (s != 0) {
        handle_error_en(s, "pthread_join for writer");
    }
    s = pthread_join(reader_id, 0);
    if (s != 0) {
        handle_error_en(s, "pthread_join for writer");
    }

    FILE *writer_0_log = fopen("writer_0.log", "w");
    if (writer_0_log == NULL) {
        err(1, "writer_0.log");
    }
    FILE *writer_1_log = fopen("writer_1.log", "w");
    if (writer_1_log == NULL) {
        err(1, "writer_1.log");
    }
    FILE *reader_log = fopen("reader.log", "w");
    if (reader_log == NULL) {
        err(1, "reader.log");
    }

    for (int i = 0; i < MAX_LOOP_COUNT; ++i) {
        fprintf(writer_0_log, "%ld.%09ld writer_0 %d\n", writer_0_tv[i].tv_sec, writer_0_tv[i].tv_nsec, i);
        fprintf(writer_1_log, "%ld.%09ld writer_1 %d\n", writer_1_tv[i].tv_sec, writer_1_tv[i].tv_nsec, i);
        fprintf(reader_log, "%ld.%09ld reader %d\n", reader_tv[i].tv_sec, reader_tv[i].tv_nsec, i);
    }

    fclose(writer_0_log);
    fclose(writer_1_log);
    fclose(reader_log);

    return 0;
}

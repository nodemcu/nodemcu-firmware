/*
 * testrunner.c
 *
 *  Created on: Jun 18, 2013
 *      Author: petera
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include "testrunner.h"

static struct {
  test *tests;
  test *_last_test;
  int test_count;
  void (*on_stop)(test *t);
  test_res *failed;
  test_res *failed_last;
  test_res *stopped;
  test_res *stopped_last;
  FILE *spec;
} test_main;

void test_init(void (*on_stop)(test *t)) {
  test_main.on_stop = on_stop;
}

static char check_spec(char *name) {
  if (test_main.spec) {
    fseek(test_main.spec, 0, SEEK_SET);
    char *line = NULL;
    size_t sz;
    ssize_t read;
    while ((read = getline(&line, &sz, test_main.spec)) != -1) {
      if (strncmp(line, name, strlen(name)) == 0) {
        free(line);
        return 1;
      }
    }
    free(line);
    return 0;
  } else {
    return 1;
  }
}

void add_test(test_f f, char *name, void (*setup)(test *t), void (*teardown)(test *t)) {
  if (f == 0) return;
  if (!check_spec(name)) return;
  DBGT("adding test %s\n", name);
  test *t = malloc(sizeof(test));
  memset(t, 0, sizeof(test));
  t->f = f;
  strcpy(t->name, name);
  t->setup = setup;
  t->teardown = teardown;
  if (test_main.tests == 0) {
    test_main.tests = t;
  } else {
    test_main._last_test->_next = t;
  }
  test_main._last_test = t;
  test_main.test_count++;
}

static void add_res(test *t, test_res **head, test_res **last) {
  test_res *tr = malloc(sizeof(test_res));
  memset(tr,0,sizeof(test_res));
  strcpy(tr->name, t->name);
  if (*head == 0) {
    *head = tr;
  } else {
    (*last)->_next = tr;
  }
  *last = tr;
}

static void dump_res(test_res **head) {
  test_res *tr = (*head);
  while (tr) {
    test_res *next_tr = tr->_next;
    printf("  %s\n", tr->name);
    free(tr);
    tr = next_tr;
  }
}

void run_tests(int argc, char **args) {
  memset(&test_main, 0, sizeof(test_main));
  if (argc > 1) {
    printf("running tests from %s\n", args[1]);
    FILE *fd = fopen(args[1], "r");
    if (fd == NULL) {
      printf("%s not found\n", args[1]);
      exit(EXIT_FAILURE);
    }
    test_main.spec = fd;
  }

  DBGT("adding suites...\n");
  add_suites();
  DBGT("%i tests added\n", test_main.test_count);
  if (test_main.spec) {
    fclose(test_main.spec);
  }

  if (test_main.test_count == 0) {
    printf("No tests to run\n");
    return;
  }

  int fd_success = open("_tests_ok", O_APPEND | O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  int fd_bad = open("_tests_fail", O_APPEND | O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  DBGT("running tests...\n");
  int ok = 0;
  int failed = 0;
  int stopped = 0;
  test *cur_t = test_main.tests;
  int i = 1;
  while (cur_t) {
    cur_t->setup(cur_t);
    test *next_test = cur_t->_next;
    DBGT("TEST %i/%i : running test %s\n", i, test_main.test_count, cur_t->name);
    i++;
    int res = cur_t->f(cur_t);
    cur_t->teardown(cur_t);
    int fd = res == TEST_RES_OK ? fd_success : fd_bad;
    write(fd, cur_t->name, strlen(cur_t->name));
    write(fd, "\n", 1);
    switch (res) {
    case TEST_RES_OK:
      ok++;
      printf("  .. ok\n");
      break;
    case TEST_RES_FAIL:
      failed++;
      printf("  .. FAILED\n");
      if (test_main.on_stop) test_main.on_stop(cur_t);
      add_res(cur_t, &test_main.failed, &test_main.failed_last);
      break;
    case TEST_RES_ASSERT:
      stopped++;
      printf("  .. ABORTED\n");
      if (test_main.on_stop) test_main.on_stop(cur_t);
      add_res(cur_t, &test_main.stopped, &test_main.stopped_last);
      break;
    }
    free(cur_t);
    cur_t = next_test;
  }
  close(fd_success);
  close(fd_bad);
  DBGT("ran %i tests\n", test_main.test_count);
  printf("Test report, %i tests\n", test_main.test_count);
  printf("%i succeeded\n", ok);
  printf("%i failed\n", failed);
  dump_res(&test_main.failed);
  printf("%i stopped\n", stopped);
  dump_res(&test_main.stopped);
  if (ok < test_main.test_count) {
    printf("\nFAILED\n");
  } else {
    printf("\nALL TESTS OK\n");
  }
}

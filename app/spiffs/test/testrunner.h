/*
 * testrunner.h
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */

/*

SUITE(mysuite)

void setup(test *t) {}

void teardown(test *t) {}

TEST(mytest) {
  printf("mytest runs now..\n");
  return 0;
} TEST_END(mytest)

SUITE_END(mysuite)



SUITE(mysuite2)

void setup(test *t) {}

void teardown(test *t) {}

TEST(mytest2a) {
  printf("mytest2a runs now..\n");
  return 0;
} TEST_END(mytest2a)

TEST(mytest2b) {
  printf("mytest2b runs now..\n");
  return 0;
} TEST_END(mytest2b)

SUITE_END(mysuite2)



void add_suites() {
  ADD_SUITE(mysuite);
  ADD_SUITE(mysuite2);
}
 */

#ifndef TESTRUNNER_H_
#define TESTRUNNER_H_

#define TEST_RES_OK   0
#define TEST_RES_FAIL -1
#define TEST_RES_ASSERT -2

struct test_s;

typedef int (*test_f)(struct test_s *t);

typedef struct test_s {
  test_f f;
  char name[256];
  void *data;
  void (*setup)(struct test_s *t);
  void (*teardown)(struct test_s *t);
  struct test_s *_next;
  unsigned char test_result;
} test;

typedef struct test_res_s {
  char name[256];
  struct test_res_s *_next;
} test_res;

#define TEST_CHECK(x) if (!(x)) { \
  printf("  TEST FAIL %s:%i\n", __FILE__, __LINE__); \
  goto __fail_stop; \
}
#define TEST_CHECK_EQ(x, y) if ((x) != (y)) { \
  printf("  TEST FAIL %s:%i, %i != %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_CHECK_NEQ(x, y) if ((x) == (y)) { \
  printf("  TEST FAIL %s:%i, %i == %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_CHECK_GT(x, y) if ((x) <= (y)) { \
  printf("  TEST FAIL %s:%i, %i <= %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_CHECK_LT(x, y) if ((x) >= (y)) { \
  printf("  TEST FAIL %s:%i, %i >= %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_CHECK_GE(x, y) if ((x) < (y)) { \
  printf("  TEST FAIL %s:%i, %i < %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_CHECK_LE(x, y) if ((x) > (y)) { \
  printf("  TEST FAIL %s:%i, %i > %i\n", __FILE__, __LINE__, (x), (y)); \
  goto __fail_stop; \
}
#define TEST_ASSERT(x) if (!(x)) { \
  printf("  TEST ASSERT %s:%i\n", __FILE__, __LINE__); \
  goto __fail_assert; \
}

#define DBGT(...) printf(__VA_ARGS__)

#define str(s) #s

#define SUITE(sui) \
  extern void __suite_##sui() {
#define SUITE_END(sui) \
  }
#define ADD_SUITE(sui) \
  __suite_##sui();
#define TEST(tf) \
  int tf(struct test_s *t) { do
#define TEST_END(tf) \
  while(0); \
  __fail_stop: return TEST_RES_FAIL; \
  __fail_assert: return TEST_RES_ASSERT; \
  } \
  add_test(tf, str(tf), setup, teardown);


void add_suites();
void test_init(void (*on_stop)(test *t));
void add_test(test_f f, char *name, void (*setup)(test *t), void (*teardown)(test *t));
// returns 0 if all tests ok, -1 if any test failed, -2 on badness
int run_tests(int argc, char **args);

#endif /* TESTRUNNER_H_ */

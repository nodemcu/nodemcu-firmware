/*
 * testsuites.c
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */

#include "testrunner.h"

void add_suites() {
  //ADD_SUITE(dev_tests);
  ADD_SUITE(check_tests);
  ADD_SUITE(hydrogen_tests)
  ADD_SUITE(bug_tests)
}

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#if PRATT_DEBUG
# define ARENA_DEBUG 1
#else
# define ARENA_DEBUG 0
#endif
#include "arena.h"


#define push_array_no_zero_aligned(a, T, c, align) arena_push_array_no_zero_aligned(a, T, c, align)
#define push_array_aligned(a, T, c, align) arena_push_array_aligned(a, T, c, align)
#define push_array(a, T, c) arena_push_array_aligned(a, T, c, Max(8, AlignOf(T)))

typedef uint64_t u64;
typedef int64_t  i64;
typedef int32_t  i32;

typedef struct {
  char* str;
  uint64_t size;
} String8;


#define str8_lit(cstr) {.str=cstr,.size=(sizeof(cstr))-1}
#define Str8Fmt(str8) (int)str8.size, str8.str
#define Unused(p) (void)p

uint32_t try_f64_from_str8(String8 str, double* num)
{
  uint32_t result = 1;
  char* endptr = NULL;
  double temp = strtod(str.str, &endptr);
  if (endptr != (str.str + str.size))
  {
    result = 0;
  }
  else
  {
    *num = temp;
  }
  return result;
}

String8 str8_fmt(Arena* a, const char* fmt, ...)
{
  String8 result = {0};
  Unused(a);
  Unused(fmt);
  Assert(0);
  return result;
}

void feed_queue_error(String8 msg)
{
  printf("(err) %.*s\n", (int)msg.size, msg.str);
}


void feed_queue_info(String8 msg)
{
  printf("(info) %.*s\n", (int)msg.size, msg.str);
}


#define FRED_EMULATION

#include "quick_calc.c"


typedef struct {
  String8 input;
  QCResult expected;
  const char* name;
} Test;

Test tests[] = {
(Test){str8_lit("1 + 2 + 3+4"), (QCResult){.value = 10.0, .status = QC_ERR_NONE}, "input_right"},
(Test){str8_lit("4 * 3 + 2 - 1"), (QCResult){.value = 13.0, .status = QC_ERR_NONE}, "input_left"},
(Test){str8_lit("1 + 2 + 3 + (4 + 5)"), (QCResult){.value = 15.0, .status = QC_ERR_NONE},"input_paren"},
(Test){str8_lit("(1+2)*(3*4.0)"), (QCResult){.value = 36.0, .status = QC_ERR_NONE}, "input_paren2"},
(Test){str8_lit("(((1 + 2)))"), (QCResult){.value = 3.0, .status = QC_ERR_NONE},"input_paren3"},
(Test){str8_lit("((1 + 2) * (3 + 4))*(4)-1"), (QCResult){.value = 83.0, .status = QC_ERR_NONE},"input_paren4"},
(Test){str8_lit("-1 + 2"), (QCResult){.value = 1.0, .status = QC_ERR_NONE}, "negative_start"},
(Test){str8_lit("1 / 0"), (QCResult){.value = INFINITY, .status = QC_ERR_NONE}, "divide_by_zero"},


(Test){str8_lit("+ 2 - 4"), (QCResult){.value = 0.0, .status = QC_ERR_UNEXPECTED_TOKEN}, "operator_at_the_start"},
(Test){str8_lit("2 + + 2 - 4"), (QCResult){.value = 0.0, .status = QC_ERR_UNEXPECTED_TOKEN}, "double_operator"},
(Test){str8_lit("2 ++ 2 - 4"), (QCResult){.value = 0.0, .status = QC_ERR_UNEXPECTED_TOKEN}, "double_operator_no_space"},
};

String8 input_start_op = str8_lit("+ + 2 - 4");

#define ArrSize(arr) (sizeof(arr)/sizeof(arr[0]))

#define ANSI_RED "\e[0;31m"
#define ANSI_GREEN "\e[0;92m"
#define ANSI_RESET "\e[0m"

void test_report(Test* test, QCResult output)
{
  if (output.status != test->expected.status)
  {
    printf("Test %32s status " ANSI_RED "failure" ANSI_RESET ": expected %s, got %s\n", test->name, qc_err_strs[test->expected.status], qc_err_strs[output.status]);
  }
  else
  {
    printf("Test %s value " ANSI_RED "failure" ANSI_RESET ": expected %lf, got %lf\n", test->name, test->expected.value, output.value);
  }
}

bool test_run(Arena* a, Test* test)
{
  QCResult result = qc_core(a, test->input);
  bool test_result = 0;
  if (result.status == test->expected.status)
  {
    if (result.value == test->expected.value)
      test_result = 1;
  }
  if (!test_result)
  {
    test_report(test, result);
  }
  return test_result;
}

int main(void)
{
  //String8 input = input_start_op;
  Arena* arena = arena_alloc();
  int passed_tests = 0;
  int failed_tests = 0;
  for (size_t i = 0;i < ArrSize(tests);++i)
  {
    if (test_run(arena, &tests[i]))
    {
      passed_tests += 1;
    }
    else
    {
      failed_tests += 1;
    }
  }

  printf("%sFinal report: passed = %d, failed = %d" ANSI_RESET "\n", failed_tests > 0 ? ANSI_RED : ANSI_GREEN, passed_tests, failed_tests);
  //QCResult result = qc_core(arena, input);
  //printf("[%.*s] = %lf\n", Str8Fmt(input), result.value);
  arena_release(arena);
}



#include "arena.c"

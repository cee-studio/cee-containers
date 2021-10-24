#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "cee.h"
#include "greatest.h"

struct generic {
  enum { UNDEF=0, INT32, STRING, FLOAT } type;
  void *key;
  union {
    void *p;
    int   i;
    float f;
  };
};


TEST check_string_against_original(char *str)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_str *s = cee_str_mk(st, "%s", str);
  ASSERT_STR_EQ(str, (char *)s);
  cee_del(st);
  PASS();
}

TEST check_string_concatenation(void)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_str *s1, *s2, *s3;
  s1 = cee_str_mk(st, "%d", 10);
  ASSERT_STR_EQ("10", (char *)s1);
  s2 = cee_str_mk(st, "%.1f", 10.3);
  ASSERT_STR_EQ("10.3", (char *)s2);
  s3 = cee_str_mk(st, "%s %s", s1, s2);
  ASSERT_STR_EQ("10 10.3", (char *)s3);
  cee_del(st);
  PASS();
}

TEST check_list_append(void)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_list *list = cee_list_mk(st, 10);
  struct cee_str *s[] = { 
    cee_str_mk(st, "%s", "1"),
    cee_str_mk(st, "%s", "2"),
    cee_str_mk(st, "%s", "3")
  };
  const unsigned arr_len = sizeof(s) / sizeof(struct cee_str*);

  for (int i=0; i < arr_len; ++i) {
    cee_list_append(&list, s[i]);
  }
  ASSERT(cee_list_size(list) == arr_len);

  for (int i=0; i < cee_list_size(list); ++i) {
    char num[32];
    snprintf(num, sizeof(num), "%d", i+1);
    ASSERT_STR_EQ(num, (char *)list->_[i]);
  }
  cee_del(st);
  PASS();
}

TEST check_list_heterogenous(void)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_list *list = cee_list_mk(st, 10);

  /* heterogeneous list [ 10, 10.1, "10"] */
  cee_list_append(&list, cee_tagged_mk(st, INT32, cee_boxed_from_i32(st, 10)));
  cee_list_append(&list, cee_tagged_mk(st, FLOAT, cee_boxed_from_float(st, 10.1f)));
  cee_list_append(&list, cee_tagged_mk(st, STRING, cee_str_mk(st, "10")));
  struct cee_tagged *tv = list->_[0];
  ASSERT_EQ(INT32, tv->tag);
  ASSERT_EQ(10, cee_boxed_to_i32(tv->ptr.boxed));
  tv = list->_[1];
  ASSERT_EQ(FLOAT, tv->tag);
  ASSERT_EQ(10.1f, cee_boxed_to_float(tv->ptr.boxed));
  tv = list->_[2];
  ASSERT_STR_EQ("10", (char*)tv->ptr.str);
  cee_del(st);
  PASS();
}

TEST check_set_find(char *str)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_set *set = cee_set_mk(st, (cee_cmp_fun)&strcmp);
  char *p;

  cee_set_add(set, cee_str_mk(st, str));
  p = cee_set_find(set, str);
  ASSERT_STR_EQ(str, p);
  cee_set_remove(set, cee_str_mk(st, str));
  p = cee_set_find(set, str);
  ASSERT(p == NULL);
  cee_del(st);
  PASS();
}

TEST check_set_values(char *str_list[], const unsigned n_str)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_set *set = cee_set_mk(st, (cee_cmp_fun)&strcmp);
  struct cee_list *set_values;
  char *p;

  for (unsigned i=0; i < n_str; ++i) {
    cee_set_add(set, cee_str_mk(st, str_list[i]));
  }

  set_values = cee_set_values(set);
  ASSERT(cee_set_size(set) == cee_list_size(set_values));

  for (unsigned i=0; i < cee_list_size(set_values); ++i) {
    p = cee_set_find(set, set_values->_[i]);
    ASSERT(p != NULL);
  }
  cee_del(st);
  PASS();
}

TEST check_map_find(struct generic list[], const unsigned n)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_map *mp = cee_map_mk(st, (cee_cmp_fun)&strcmp);
  struct cee_boxed *t;

  for (unsigned i=0; i < n; ++i) {
    cee_map_add(mp, cee_str_mk(st, list[i].key), cee_boxed_from_i32(st, list[i].i));
  }
  for (unsigned i=0; i < n; ++i) {
    t = cee_map_find(mp, list[i].key);
    ASSERT(t != NULL);
  }
  cee_del(st);
  PASS();
}


static void map_iter_cb(void *ctx, void *p_key, void *p_value)
{
  struct cee_str *key = p_key;
  struct cee_boxed *value = p_value;
  //ASSERT(p_key != NULL);
  //ASSERT(p_value != NULL);
  //ASSERT(((void*)123) == ctx);
}

TEST check_map_keys(struct generic list[], const unsigned n)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_map *mp = cee_map_mk(st, (cee_cmp_fun)&strcmp);
  struct cee_list *keys;

  for (unsigned i=0; i < n; ++i) {
    cee_map_add(mp, cee_str_mk(st, list[i].key), cee_boxed_from_i32(st, list[i].i));
  }
  keys = cee_map_keys(mp);
  ASSERT(keys != NULL);
  ASSERT_EQ(n, cee_list_size(keys));
  /* @todo allow returning error value for cee_map_iterate() */
  cee_map_iterate(mp, (void*)123, &map_iter_cb);
  cee_del(st);
  PASS();
}

TEST check_map_overwrite(void)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_map *mp = cee_map_mk(st, (cee_cmp_fun)&strcmp);

  cee_map_add(mp, cee_str_mk(st, "1"), cee_boxed_from_i32(st, 10));
  cee_map_add(mp, cee_str_mk(st, "1"), cee_boxed_from_i32(st, 100));
  struct cee_boxed *t = cee_map_find(mp, "1");
  ASSERT_EQ(100, cee_boxed_to_i32(cee_map_find(mp, "1")));
  cee_del(st);
  PASS();
}

TEST check_stack_push(void)
{
  struct cee_state *st = cee_state_mk(10);
  struct cee_stack *sp = cee_stack_mk(st, 100);

  cee_stack_push(sp, cee_str_mk(st, "1"));
  cee_stack_push(sp, cee_str_mk(st, "2"));
  cee_stack_push(sp, cee_str_mk(st, "3"));
  ASSERT_STR_EQ("3", cee_stack_top(sp, 0));
  ASSERT_STR_EQ("2", cee_stack_top(sp, 1));
  ASSERT_STR_EQ("1", cee_stack_top(sp, 2));
  cee_del(st);
  PASS();
}

TEST check_dict_find(void)
{
  struct cee_state *st   = cee_state_mk(10);
  struct cee_dict  *dict = cee_dict_mk(st, 100);
  struct cee_str   *key;
  
  for (int i=0; i < 1000; ++i) {
    cee_dict_add(dict, cee_str_mk(st, "%d", i)->_, cee_str_mk(st, "value %d", i));
  }

  key = cee_str_mk(st, "9");
  ASSERT_STR_EQ("value 9", cee_dict_find(dict, key->_));
  cee_del(st);
  PASS();
}

TEST check_n_tuple_mk(void)
{
  struct cee_state *st   = cee_state_mk(10);
  struct cee_n_tuple *t = 
    cee_n_tuple_mk(st, 5, cee_str_mk(st, "1"), cee_str_mk(st, "2"), cee_str_mk(st, "3"), cee_str_mk(st, "4"), cee_str_mk(st, "5"));
  
  for (int i=0; i < 5; ++i) {
    ASSERT_EQ(i+1, strtol(t->_[i], NULL, 10));
  }
  cee_del(st);
  PASS();
}

void* baz(struct cee_state *st, struct cee_env *outer, size_t amt, va_list ap) 
{
  int u = va_arg(ap, int), v = va_arg(ap, int), w = va_arg(ap, int);

  int m = cee_boxed_to_i32(cee_env_find(outer, "m"));
  int n = cee_boxed_to_i32(cee_env_find(outer, "n"));

  int x = cee_boxed_to_i32(cee_env_find(outer, "x"));
  int y = cee_boxed_to_i32(cee_env_find(outer, "y"));

  return cee_boxed_from_i32(st, (u+m+x) + v*n*y + w); // E
}

void* bar(struct cee_state *st, struct cee_env *outer, size_t amt, va_list ap) 
{
  int i = va_arg(ap, int), j = va_arg(ap, int), k = va_arg(ap, int);

  struct cee_map * vars = cee_map_mk(st, (cee_cmp_fun)strcmp);
  cee_map_add(vars, cee_str_mk(st, "x"), cee_boxed_from_i32(st, 2)); // C
  cee_map_add(vars, cee_str_mk(st, "y"), cee_boxed_from_i32(st, 5)); // D
  struct cee_env * e = cee_env_mk(st, outer, vars);
  struct cee_closure * c = cee_closure_mk(st, e, &baz);

  return cee_boxed_from_i32(st, (i+j+k) * cee_boxed_to_i32(cee_closure_call(st, c, 3, 1,2,3))); // F
}

TEST check_closure(void)
{
  /* analogous to the following JS script:
   * var m = 2;                              // A
   * var n = 1;                              // B
   * function bar(i, j, k) {
   *   var x = 2;                            // C
   *   var y = 5;                            // D
   *   function baz(u, v, w) {
   *     return (u+m+x) + v*n*y + w;         // E
   *   }
   *   return (i+j+k) * baz(1, 2, 3);        // F
   * }
   * // should return 54
   * bar(1, 1, 1);                           // G
   */
  struct cee_state *st   = cee_state_mk(10);
  struct cee_map   *vars = cee_map_mk(st, (cee_cmp_fun)&strcmp);
  cee_map_add(vars, cee_str_mk(st, "m"), cee_boxed_from_i32(st, 2)); // A
  cee_map_add(vars, cee_str_mk(st, "n"), cee_boxed_from_i32(st, 1)); // B
  struct cee_env     *e   = cee_env_mk(st, NULL, vars);
  struct cee_closure *c   = cee_closure_mk(st, e, &bar);
  struct cee_boxed   *ret = cee_closure_call(st, c, 3, 1,1,1);       // G
  ASSERT_EQ(54, cee_boxed_to_i32(ret));
  cee_del(st);
  PASS();
}

SUITE(cee_str)
{
  char *str_list[] = { "Hello World!", "Fish", "" , "\n\t" };

  for (unsigned i=0; i < sizeof(str_list)/sizeof(char*); ++i) {
    RUN_TESTp(check_string_against_original, str_list[i]);
  }
  RUN_TEST(check_string_concatenation);
}

SUITE(cee_list)
{
  RUN_TEST(check_list_append);
  RUN_TEST(check_list_heterogenous);
}

SUITE(cee_set)
{
  char *str_list[] = { "Hello World!", "Fish", "" , "\n\t" };
  const unsigned n_str = sizeof(str_list)/sizeof(char*);

  for (unsigned i=0; i < n_str; ++i) {
    RUN_TESTp(check_set_find, str_list[i]);
  }
  RUN_TESTp(check_set_values, str_list, n_str);
}

SUITE(cee_map)
{
  // type, key, value
  struct generic list[] = {
    { INT32, "1", (void*)10 },
    { INT32, "2", (void*)20 },
    { INT32, "3", (void*)30 }
  };
  const unsigned n_pairs = sizeof(list)/sizeof(struct generic);

  RUN_TESTp(check_map_find, list, n_pairs);
  RUN_TEST(check_map_overwrite);
  RUN_TESTp(check_map_keys, list, n_pairs);
}

SUITE(cee_stack)
{
  RUN_TEST(check_stack_push);
}

SUITE(cee_dict)
{
  RUN_TEST(check_dict_find);
}

SUITE(cee_n_tuple)
{
  RUN_TEST(check_n_tuple_mk);
}

SUITE(cee_closure)
{
  RUN_TEST(check_closure);
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) 
{
  GREATEST_MAIN_BEGIN();

  RUN_SUITE(cee_str);
  RUN_SUITE(cee_list);
  RUN_SUITE(cee_set);
  RUN_SUITE(cee_map);
  RUN_SUITE(cee_stack);
  RUN_SUITE(cee_dict);
  RUN_SUITE(cee_n_tuple);
  RUN_SUITE(cee_closure);

  GREATEST_MAIN_END();
}
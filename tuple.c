#ifdef CEE_AMALGAMATION
#undef   S
#define  S(f)   _cee_tuple_##f
#else
#define  S(f)   _##f
#include <stdlib.h>
#include <string.h>
#include "cee.h"
#include "cee-internal.h"
#endif
#include "cee-header.h"

struct S(header) {
  enum cee_del_policy del_policies[2];
  struct cee_sect cs;
  void * _[2];
};
    
#include "cee-resize.h"
    
static void S(trace)(void * v, enum cee_trace_action ta) {
  struct S(header) * b = FIND_HEADER(v);
  int i;
  
  switch (ta) {
    case trace_del_no_follow:
      S(de_chain)(b);
      free(b);
      break;
    case trace_del_follow:
      for (i = 0; i < 2; i++)
        cee_del_e(b->del_policies[i], b->_[i]);
      S(de_chain)(b);
      free(b);
      break;
    default:
      b->cs.gc_mark = ta - trace_mark;
      for (i = 0; i < 2; i++)
        cee_trace(b->_[i], ta);
      break;
  }
}


struct cee_tuple * cee_tuple_mk_e (struct cee_state * st, enum cee_del_policy o[2], void * v1, void * v2) {
  size_t mem_block_size = sizeof(struct S(header));
  struct S(header) * m = malloc(mem_block_size);
  ZERO_CEE_SECT(&m->cs);
  S(chain)(m, st);
  
  m->cs.trace = S(trace);
  m->cs.resize_method = resize_with_identity;
  m->cs.mem_block_size = mem_block_size;
  m->cs.state = st;
  m->_[0] = v1;
  m->_[1] = v2;
  int i;
  for (i = 0; i < 2; i++) {
    m->del_policies[i] = o[i];
    cee_incr_indegree(o[i], m->_[i]);
  }
  return (struct cee_tuple *)&m->_;
}

struct cee_tuple * cee_tuple_mk (struct cee_state * st, void * v1, void * v2) {
  static enum cee_del_policy o[2] = { CEE_DEFAULT_DEL_POLICY, CEE_DEFAULT_DEL_POLICY };
  return cee_tuple_mk_e(st, o, v1, v2);
}

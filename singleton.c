#ifdef CEE_AMALGAMATION
#undef   S
#define  S(f)  _cee_singleton_##f
#else
#define  S(f)  _##f
#include <string.h>
#include "cee.h"
#include "cee-internal.h"
#endif
#include "cee-header.h"

struct S(header) {
  struct cee_sect cs;
  uintptr_t _; // tag
  uintptr_t val;
};

/*
 * singleton should never be deleted, hence we pass a noop
 */
static void S(noop)(void *p, enum cee_trace_action ta) {}

/*
 * the parameter of this function has to be a global/static 
 * uintptr_t array of two elements
 */
struct cee_singleton * cee_singleton_init(void *s, uintptr_t tag, uintptr_t val) {
  struct S(header) * b = (struct S(header) *)s;
  ZERO_CEE_SECT(&b->cs);
  b->cs.trace = S(noop);
  b->cs.resize_method = resize_with_identity;
  b->cs.mem_block_size = 0;
  b->cs.n_product = 0;
  b->_ = tag;
  b->val = val;
  return (struct cee_singleton *)&(b->_);
}

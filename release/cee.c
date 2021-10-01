#ifndef CEE_ONE
#define CEE_ONE
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#ifndef CEE_H
#define CEE_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

struct cee_state; /* forwarding */

typedef uintptr_t cee_tag_t;
typedef int (*cee_cmp_fun) (const void *, const void *);

enum cee_resize_method {
  CEE_RESIZE_WITH_IDENTITY = 0, /* resize with identity function */
  CEE_RESIZE_WITH_MALLOC = 1,   /* resize with malloc  (safe, but leak) */
  CEE_RESIZE_WITH_REALLOC = 2   /* resize with realloc (probably unsafe) */
};

enum cee_trace_action {
  CEE_TRACE_DEL_NO_FOLLOW = 0,
  CEE_TRACE_DEL_FOLLOW, /* trace points-to graph and delete each node */
  CEE_TRACE_MARK,       /* trace points-to graph and mark each node */
};




/*
 * a cotainer is an instance of struct cee_*
 * a cee element is an instance of struct cee_*
 * 
 * 
 * a container has one of the three delete policies, the policies dedicate
 * how the elements of the container will be handled once the container is 
 * deleted (freed).
 * 
 * CEE_DP_DEL_RC: if a container is freed, its cee element's in-degree will be 
 *         decreased by one. If any cee element's in-degree is zero, the element 
 *         will be freed. It's developer's responsibility to prevent cyclically
 *         pointed containers from having this policy.
 * 
 * CEE_DP_DEL: if a container is freed, all its cee elements will be freed 
 *         immediately. It's developer's responsiblity to prevent an element is 
 *         retained by multiple containers that have this policy.
 *
 * CEE_DP_NOOP: if a container is freed, nothing will happen to its elements.
 *          It's developer's responsiblity to prevent memory leaks.
 *
 * the default del_policy is CEE_DP_DEL_RC, which can be configured at compile
 * time with CEE_DEFAULT_DEL_POLICY
 */
enum cee_del_policy {
  CEE_DP_DEL_RC = 0,
  CEE_DP_DEL = 1,
  CEE_DP_NOOP = 2
};

#ifndef CEE_DEFAULT_DEL_POLICY
#define CEE_DEFAULT_DEL_POLICY  CEE_DP_DEL_RC
#endif
/*
 *
 * if an object is owned an del_immediate container, retained is 1, and 
 * in_degree is ignored.
 *
 * if an object is owned by multiple del_rc containers, in_degree is the 
 * number of containers.
 *
 */
struct cee_sect {
  uint8_t  cmp_stop_at_null:1;  /* 0: compare all bytes, otherwise stop at '\0' */
  uint8_t  resize_method:2;     /* three values: identity, malloc, realloc */
  uint8_t  retained:1;          /* if it is retained, in_degree is ignored */
  uint8_t  gc_mark:2;           /* used for mark & sweep gc */
  uint8_t  n_product;           /* n-ary (no more than 256) product type */
  uint16_t in_degree;           /* the number of cee objects points to this object */
  /* begin of gc fields */
  struct cee_state * state;     /* the gc state under which this block is allocated */
  struct cee_sect * trace_next; /* used for chaining cee::_::data to be traced */
  struct cee_sect * trace_prev; /* used for chaining cee::_::data to be traced */
  /* end of gc fields */
  uintptr_t mem_block_size;     /* the size of a memory block enclosing this struct */
  void *cmp;                    /* compare two memory blocks */
  
  /* the object specific generic scan function */
  /* it does memory deallocation, reference count decreasing, or liveness marking */
  void (*trace)(void *, enum cee_trace_action);
};

/*
 * A consecutive memory block of unknown length.
 * It can be safely casted to char *, but it may not 
 * be terminated by '\0'.
 */
struct cee_block {
  char _[1]; /* an array of chars */
};

/*
 * n: the number of bytes
 * the function performs one task
 * -- allocate a memory block to include at least n consecutive bytes
 * 
 * return: the address of the first byte in consecutive bytes, the address 
 *         can be freed by cee_del
 */
extern void * cee_block_mk (struct cee_state * s, size_t n);

/*
 * C string is an array of chars, it may or may not be terminated by '\0'.
 * 
 * if it's not terminated by null strlen will read memory out of its bounds.
 *
 */
struct cee_str {
  char _[1];
};


/*
 * the function performs the following task
 * 1  allocate a memory block to include enough consecutive bytes
 * 2. initialize the consecutive bytes as a null terminated string
 *    with fmt and its arguments
 * 
 * return: the start address of the consecutive bytes that is 
 *         null termianted and strlen is 0.
 *         the address can be safely casted to struct cee_block *
 *
 * e.g.
 *
 *      allocate an empty string
 *      cee_str_mk (state, ""); 
 * 
 *      allocate a string for int 10
 *      cee_str_mk (state, "%d", 10);
 *
 */
extern struct cee_str  * cee_str_mk (struct cee_state *s, const char * fmt, ...);


/*
 * the function performs the following task
 * 1  allocate a memory block to include n consecutive bytes
 * 2. initialize the consecutive bytes with fmt and its arguments
 * 
 * return: the start address of the consecutive bytes that is
 *         null terminated.
 *         the address can be safely casted to struct cee_block *
 * e.g.
 *      allocate a string buffer of 100 bytes, and initialize it with 
 *      an empty string.
 *      cee_str_mk_e (state, 100, "");
 * 
 *      allocate a string buffer of 100 bytes and initialize it with
 *      an integer
 *      cee_str_mk_e (state, 100, "%d", 10);
 *
 */
extern struct cee_str  * cee_str_mk_e (struct cee_state * s, size_t n, const char * fmt, ...);

/*
 * return the pointer of the null terminator;
 * if the array is not null terminated, 
 * NULL is returned.
 */
extern char * cee_str_end (struct cee_str *);

/*
 * str: points to the begin of a struct cee_block
 *
 * the function performs one task
 * 1. add any char to the end of str
 *
 * return: the start address of a cee_block, a new cee_block will
 *         be allocated if the cee_block is too small.
 */
extern struct cee_str * cee_str_add (struct cee_str * str, char);

/*
 * just like the standard strcat, but this function composes the src
 * string through a fmt string and its varadic arguments.
 */
extern struct cee_str * cee_str_catf (struct cee_str *, const char * fmt, ...);
extern struct cee_str * cee_str_ncat (struct cee_str *, char * s, size_t);
  
/* an auto expandable list */
struct cee_list {
  void * _[1]; /* an array of `void *`s */
};

/*
 * capacity: the initial capacity of the list
 * when the list is deleted, its elements will be handled by 
 * the default deletion policy
 */
extern struct cee_list * cee_list_mk (struct cee_state * s, size_t capacity);

/*
 *
 */
extern struct cee_list * cee_list_mk_e (struct cee_state * s, enum cee_del_policy o, size_t size);

/*
 * it may return a new list if the parameter list is too small
 */
extern struct cee_list * cee_list_append(struct cee_list ** v, void * e);


/*
 * it inserts an element e at index and shift the rest elements 
 * to higher indices
 */
extern struct cee_list * cee_list_insert(struct cee_state * s, struct cee_list ** l, int index, void *e);

/*
 * it removes an element at index and shift the rest elements
 * to lower indices
 */
extern bool cee_list_remove(struct cee_list * v, int index);

/*
 * returns the number of elements in the list
 */
extern size_t cee_list_size(struct cee_list *);

/*
 * returns the capcity of the list
 */
extern size_t cee_list_capacity (struct cee_list *);


/*
 * applies f to each element of the list with cxt
 */
extern void cee_list_iterate (struct cee_list *, void *ctx, void (*f)(void *cxt, int idx, void * e));
  
struct cee_tuple {
  void * _[2];
};


/*
 * construct a tuple from its parameters
 * v1: the first value of the tuple
 * v2: the second value of the tuple
 */
extern struct cee_tuple * cee_tuple_mk (struct cee_state * s, void * v1, void * v2);
extern struct cee_tuple * cee_tuple_mk_e (struct cee_state * s, 
                           enum cee_del_policy o[2], void * v1, void * v2);

struct cee_triple {
  void * _[3];
};

/* 
 * construct a triple from its parameters
 * v1: the first value of the triple
 * v2: the second value of the triple
 * v3: the third value of the triple
 * when the triple is deleted, its elements will not be deleted
 */
extern struct cee_triple * cee_triple_mk(struct cee_state * s, void * v1, void * v2, void * v3);
extern struct cee_triple * cee_triple_mk_e(struct cee_state * s, 
                           enum cee_del_policy o[3], void * v1, void * v2, void * v3);

  
struct cee_quadruple {
  void * _[4];
};

/* 
 * construct a triple from its parameters
 * v1: the first value of the quaruple
 * v2: the second value of the quaruple
 * v3: the third value of the quadruple
 * v4: the fourth value of the quadruple
 * when the quadruple is deleted, its elements will not be deleted
 */
extern struct cee_quadruple * cee_quadruple_mk(struct cee_state * s, 
                            void * v1, void * v2, void * v3, void * v4);

extern struct cee_quadruple * cee_quadruple_mk_e(struct cee_state * s, 
                              enum cee_del_policy o[4], void * v1, void * v2, 
                              void *v3, void *v4);

struct cee_n_tuple {
  void * _[1];  /* n elements */
};
extern struct cee_n_tuple * cee_n_tuple_mk (struct cee_state * s, size_t n, ...);
extern struct cee_n_tuple * cee_n_tuple_mk_e (struct cee_state * s, size_t n, enum cee_del_policy o[], ...);


struct cee_set {
  void * _;
};

/*
 * a binary tree based set implementation
 * cmp: the function to compare two elements, it returns 0 
 * if they are equal; it returns large than 0 if the first 
 * parameter is larger than the second parameter; it returns 
 * a value smaller than 0 if the first parameter is smaller than
 * the second parameters;
 *
 * dt: specifiy how its element should be handled when the set is deleted.
 *
 */
extern struct cee_set * cee_set_mk (struct cee_state * s, int (*cmp)(const void *, const void *));
extern struct cee_set * cee_set_mk_e (struct cee_state *s, enum cee_del_policy o, 
                         int (*cmp)(const void *, const void *));

extern void cee_set_add(struct cee_set * m, void * key);
extern void * cee_set_find(struct cee_set * m, void * key);
extern void * cee_set_remove(struct cee_set * m, void * key);
extern void cee_set_clear (struct cee_set * m);
extern size_t cee_set_size(struct cee_set * m);
extern bool cee_set_empty(struct cee_set * s);
extern struct cee_list * cee_set_values(struct cee_set * m);
extern struct cee_set * cee_set_union_sets (struct cee_set * s1, struct cee_set * s2);

struct cee_map {
  void * _;
};

/*
 * map implementation based on binary tree
 * add/remove
 */
extern struct cee_map * cee_map_mk(struct cee_state * s, cee_cmp_fun cmp);
extern struct cee_map * cee_map_mk_e(struct cee_state * s, enum cee_del_policy o[2], cee_cmp_fun cmp);

extern uintptr_t cee_map_size(struct cee_map *);
extern void cee_map_add(struct cee_map * m, void * key, void * value);
extern void * cee_map_find(struct cee_map * m, void * key);
extern void * cee_map_remove(struct cee_map *m, void * key);
extern struct cee_list * cee_map_keys(struct cee_map *m);
extern struct cee_list * cee_map_values(struct cee_map *m);

/*
 * applies f to each (k,v) of the map m with ctx
 */
extern void cee_map_iterate(struct cee_map *m, void *ctx, void (*f)(void *ctx, void *key, void *value));


/*
 * dict behaviors like a map with the following properties
 * 
 * 1. fixed size
 * 2. key is char *
 * 3. insertion only
 *
 */
struct cee_dict {
  char _[1];  /* opaque data */
};

/*
 *
 */
extern struct cee_dict * cee_dict_mk (struct cee_state * s, size_t n);
extern struct cee_dict * cee_dict_mk_e (struct cee_state * s, enum cee_del_policy o, size_t n);

extern void cee_dict_add(struct cee_dict * d, char * key, void * value);
extern void * cee_dict_find(struct cee_dict * d, char * key);

/*
 * a stack with a fixed size
 */
struct cee_stack {
  void * _[1];
};
/*
 * create a fixed size stack
 * size: the size of the stack
 * dt: specify how its element should be handled when the stack is deleted.
 */
extern struct cee_stack * cee_stack_mk(struct cee_state *s, size_t n);
extern struct cee_stack * cee_stack_mk_e (struct cee_state *s, enum cee_del_policy o, size_t n);

/*
 * return the element nth element away from the top element
 */
extern void * cee_stack_top(struct cee_stack *, size_t nth);
/*
 * pop out the top element and return it
 */
extern void * cee_stack_pop(struct cee_stack *);
/*
 * push an element to the top of the stack
 */
extern int cee_stack_push(struct cee_stack *, void *);
/*
 * test if the stack is empty
 */
extern bool cee_stack_empty (struct cee_stack *);
/*
 * test if the stack is full
 */
extern bool cee_stack_full (struct cee_stack *);
/*
 * return the size of the stack
 */
extern uintptr_t cee_stack_size (struct cee_stack *);
  
  
/*
 * singleton
 */
struct cee_singleton {
  cee_tag_t  tag;
  uintptr_t val;
};
extern struct cee_singleton * cee_singleton_init(void * s, cee_tag_t tag, uintptr_t val);
#define CEE_SINGLETON_SIZE (sizeof(struct cee_singleton) + sizeof(struct cee_sect))
  
  
enum cee_boxed_primitive_type {
  cee_primitive_f64 = 1,
  cee_primitive_f32,
  cee_primitive_u64,
  cee_primitive_u32,
  cee_primitive_u16,
  cee_primitive_u8,
  cee_primitive_i64,
  cee_primitive_i32,
  cee_primitive_i16,
  cee_primitive_i8
};
union cee_boxed_primitive_value {
  double   f64;
  float    f32;
  uint64_t u64;
  uint32_t u32;
  uint16_t u16;
  uint8_t  u8;
  int64_t  i64;
  int32_t  i32;
  int16_t  i16;
  int8_t   i8;
};

/*
 * boxed primitive value
 */
struct cee_boxed {
  union cee_boxed_primitive_value _;
};

extern struct cee_boxed * cee_boxed_from_double(struct cee_state *, double);
extern struct cee_boxed * cee_boxed_from_float(struct cee_state *, float);

extern struct cee_boxed * cee_boxed_from_u64(struct cee_state *, uint64_t);
extern struct cee_boxed * cee_boxed_from_u32(struct cee_state *, uint32_t);
extern struct cee_boxed * cee_boxed_from_u16(struct cee_state *, uint16_t);
extern struct cee_boxed * cee_boxed_from_u8(struct cee_state *, uint8_t);

extern struct cee_boxed * cee_boxed_from_i64(struct cee_state *, int64_t);
extern struct cee_boxed * cee_boxed_from_i32(struct cee_state *, int32_t);
extern struct cee_boxed * cee_boxed_from_i16(struct cee_state *, int16_t);
extern struct cee_boxed * cee_boxed_from_i8(struct cee_state *, int8_t);

extern double   cee_boxed_to_double(struct cee_boxed * x);
extern float    cee_boxed_to_float(struct cee_boxed * x);

extern uint64_t cee_boxed_to_u64(struct cee_boxed * x);
extern uint32_t cee_boxed_to_u32(struct cee_boxed * x);
extern uint16_t cee_boxed_to_u16(struct cee_boxed * x);
extern uint8_t  cee_boxed_to_u8(struct cee_boxed * x);

extern int64_t  cee_boxed_to_i64(struct cee_boxed * x);
extern int32_t  cee_boxed_to_i32(struct cee_boxed * x);
extern int16_t  cee_boxed_to_i16(struct cee_boxed * x);
extern int8_t   cee_boxed_to_i8(struct cee_boxed * x);

/*
 * number of bytes needed to print out the value
 */
extern size_t cee_boxed_snprint(char * buf, size_t size, struct cee_boxed *p);
  
union cee_tagged_ptr {
  void * _;
  struct cee_str       * str;
  struct cee_set       * set;
  struct cee_list      * list;
  struct cee_map       * map;
  struct cee_dict      * dict;
  struct cee_tuple     * tuple;
  struct cee_triple    * triple;
  struct cee_quadruple * quadruple;
  struct cee_block     * block;
  struct cee_boxed     * boxed;
  struct cee_singleton * singleton;
  struct cee_stack     * stack;
  struct cee_tagged    * tagged;
};


/*
 * the generic tagged value is useful to construct tagged union
 * runtime checking is needed. 
 */
struct cee_tagged {
  cee_tag_t tag;
  union cee_tagged_ptr ptr;
};

/*
 * tag: any integer value
 * v: a pointer
 */
extern struct cee_tagged * cee_tagged_mk (struct cee_state *, uintptr_t tag, void * v);
extern struct cee_tagged * cee_tagged_mk_e (struct cee_state *, enum cee_del_policy o, uintptr_t tag, void *v);

struct cee_env {
  struct cee_env  * outer;
  struct cee_map  * vars;
};
extern struct cee_env * cee_env_mk(struct cee_state *, struct cee_env * outer, struct cee_map *vars);
extern struct cee_env * cee_env_mk_e(struct cee_state *, enum cee_del_policy dp[2], struct cee_env * outer, struct cee_map * vars);
extern void * cee_env_find(struct cee_env * e, char * key);

struct cee_closure {
  struct cee_env * env;
  void * (*vfun)(struct cee_state * s, struct cee_env * env, size_t n, va_list ap);
};

extern struct cee_closure * cee_closure_mk (struct cee_state * s, struct cee_env * env, void * fun);
extern void * cee_closure_call (struct cee_state * s, struct cee_closure * c, size_t n, ...);

extern void cee_use_realloc(void *);
extern void cee_use_malloc(void *);
  
  /*
   * release the memory block pointed by p immediately
   * it may follow the points-to edges to delete
   *    the in-degree (reference count) of targeted memory blocks
   *    or targeted memory blocks
   *
   */
extern void cee_del (void *);
extern void cee_del_ref(void *);
extern void cee_del_e (enum cee_del_policy o, void * p);

extern void cee_trace (void *p, enum cee_trace_action ta);
extern int cee_cmp (void *, void *);

extern void cee_incr_indegree (enum cee_del_policy o, void * p);
extern void cee_decr_indegree (enum cee_del_policy o, void * p);

/* 
 * return the reference count of an object
 */
extern uint16_t cee_get_rc (void *);

/*
 * call this to cause segfault for non-recoverable errors
 */
extern void cee_segfault() __attribute__((noreturn));

struct cee_state {
  /* arbitrary number of contexts */
  struct cee_map   * contexts;
  struct cee_stack * stack;  /* the stack */
  struct cee_sect  * trace_tail;
  /* all memory blocks are reachables from the roots */
  /* are considered alive */
  struct cee_set   * roots; 
  /* the mark value for the next iteration */
  int                next_mark;
};
/*
 * @param n:the size of stack, which is used for parsing
 * json and js
 */
extern struct cee_state * cee_state_mk(size_t n);
extern void cee_state_add_gc_root(struct cee_state *, void *);
extern void cee_state_remove_gc_root(struct cee_state *, void *);
extern void cee_state_gc(struct cee_state *);
extern void cee_state_add_context(struct cee_state *, char * key, void * val);
extern void cee_state_remove_context(struct cee_state *, char * key);
extern void * cee_state_get_context(struct cee_state *, char * key);

#ifndef NULL
#define NULL     ((void *)0)
#endif

#endif /* CEE_H */
#ifndef MUSL_SEARCH_H
#define MUSL_SEARCH_H
#ifdef CEE_AMALGAMATION
#else
#include <stddef.h>
#endif

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;

typedef struct musl_entry {
	char *key;
	void *data;
} MUSL_ENTRY;

int musl_hcreate(size_t);
void musl_hdestroy(void);
MUSL_ENTRY *musl_hsearch(MUSL_ENTRY, ACTION);


struct musl_hsearch_data {
	struct __tab *__tab;
	unsigned int __unused1;
	unsigned int __unused2;
};

int musl_hcreate_r(size_t, struct musl_hsearch_data *);
void musl_hdestroy_r(struct musl_hsearch_data *);
int musl_hsearch_r(MUSL_ENTRY, ACTION, MUSL_ENTRY **, struct musl_hsearch_data *);

void musl_insque(void *, void *);
void musl_remque(void *);

void *musl_lsearch(const void *, void *, size_t *, size_t,
	int (*)(const void *, const void *));
void *musl_lfind(const void *, const void *, size_t *, size_t,
	int (*)(const void *, const void *));

void *musl_tdelete(void * cxt, const void *__restrict, void **__restrict, int(*)(void *, const void *, const void *));
void *musl_tfind(void * cxt, const void *, void *const *, int(*)(void *, const void *, const void *));
void *musl_tsearch(void * cxt, const void *, void **, int (*)(void *, const void *, const void *));
void musl_twalk(void * cxt, const void *, void (*)(void *, const void *, VISIT, int));


struct musl_qelem {
	struct qelem *q_forw, *q_back;
	char q_data[1];
};

void musl_tdestroy(void * cxt, void *, void (*)(void * cxt, void *));

#endif /* MUSL_SEARCH */

 
/*
open addressing hash table with 2^n table size
quadratic probing is used in case of hash collision
tab indices and hash are size_t
after resize fails with ENOMEM the state of tab is still usable

with the posix api items cannot be iterated and length cannot be queried
*/
struct __tab {
  MUSL_ENTRY *entries;
  size_t mask;
  size_t used;
};
static struct musl_hsearch_data htab;
/*
static int musl_hcreate_r(size_t, struct musl_hsearch_data *);
static void musl_hdestroy_r(struct musl_hsearch_data *);
static int mul_hsearch_r(MUSL_ENTRY, ACTION, MUSL_ENTRY **, struct musl_hsearch_data *);
*/
static size_t keyhash(char *k)
{
  unsigned char *p = (unsigned char *)k;
  size_t h = 0;
  while (*p)
    h = 31*h + *p++;
  return h;
}
static int resize(size_t nel, struct musl_hsearch_data *htab)
{
  size_t newsize;
  size_t i, j;
  MUSL_ENTRY *e, *newe;
  MUSL_ENTRY *oldtab = htab->__tab->entries;
  MUSL_ENTRY *oldend = htab->__tab->entries + htab->__tab->mask + 1;
  if (nel > ((size_t)-1/2 + 1))
    nel = ((size_t)-1/2 + 1);
  for (newsize = 8; newsize < nel; newsize *= 2);
  htab->__tab->entries = (MUSL_ENTRY *)calloc(newsize, sizeof *htab->__tab->entries);
  if (!htab->__tab->entries) {
    htab->__tab->entries = oldtab;
    return 0;
  }
  htab->__tab->mask = newsize - 1;
  if (!oldtab)
    return 1;
  for (e = oldtab; e < oldend; e++)
    if (e->key) {
      for (i=keyhash(e->key),j=1; ; i+=j++) {
        newe = htab->__tab->entries + (i & htab->__tab->mask);
        if (!newe->key)
          break;
      }
      *newe = *e;
    }
  free(oldtab);
  return 1;
}
int musl_hcreate(size_t nel)
{
  return musl_hcreate_r(nel, &htab);
}
void musl_hdestroy(void)
{
  musl_hdestroy_r(&htab);
}
static MUSL_ENTRY *lookup(char *key, size_t hash, struct musl_hsearch_data *htab)
{
  size_t i, j;
  MUSL_ENTRY *e;
  for (i=hash,j=1; ; i+=j++) {
    e = htab->__tab->entries + (i & htab->__tab->mask);
    if (!e->key || strcmp(e->key, key) == 0)
      break;
  }
  return e;
}
MUSL_ENTRY *musl_hsearch(MUSL_ENTRY item, ACTION action)
{
  MUSL_ENTRY *e;
  musl_hsearch_r(item, action, &e, &htab);
  return e;
}
int musl_hcreate_r(size_t nel, struct musl_hsearch_data *htab)
{
  int r;
  htab->__tab = (struct __tab *) calloc(1, sizeof *htab->__tab);
  if (!htab->__tab)
    return 0;
  r = resize(nel, htab);
  if (r == 0) {
    free(htab->__tab);
    htab->__tab = 0;
  }
  return r;
}
void musl_hdestroy_r(struct musl_hsearch_data *htab)
{
  if (htab->__tab) free(htab->__tab->entries);
  free(htab->__tab);
  htab->__tab = 0;
}
int musl_hsearch_r(MUSL_ENTRY item, ACTION action, MUSL_ENTRY **retval,
                   struct musl_hsearch_data *htab)
{
  size_t hash = keyhash(item.key);
  MUSL_ENTRY *e = lookup(item.key, hash, htab);
  if (e->key) {
    *retval = e;
    return 1;
  }
  if (action == FIND) {
    *retval = 0;
    return 0;
  }
  *e = item;
  if (++htab->__tab->used > htab->__tab->mask - htab->__tab->mask/4) {
    if (!resize(2*htab->__tab->used, htab)) {
      htab->__tab->used--;
      e->key = 0;
      *retval = 0;
      return 0;
    }
    e = lookup(item.key, hash, htab);
  }
  *retval = e;
  return 1;
}
struct _musl_lsearch__node {
  struct _musl_lsearch__node *next;
  struct _musl_lsearch__node *prev;
};
void musl_insque(void *element, void *pred)
{
  struct _musl_lsearch__node *e = (struct _musl_lsearch__node *)element;
  struct _musl_lsearch__node *p = (struct _musl_lsearch__node *)pred;
  if (!p) {
    e->next = e->prev = 0;
    return;
  }
  e->next = p->next;
  e->prev = p;
  p->next = e;
  if (e->next)
    e->next->prev = e;
}
void musl_remque(void *element)
{
  struct _musl_lsearch__node *e = (struct _musl_lsearch__node *)element;
  if (e->next)
    e->next->prev = e->prev;
  if (e->prev)
    e->prev->next = e->next;
}
void *musl_lsearch(const void *key, void *base, size_t *nelp, size_t width,
  int (*compar)(const void *, const void *))
{
  char **p = (char **)base;
  size_t n = *nelp;
  size_t i;
  for (i = 0; i < n; i++)
    if (compar(p[i], key) == 0)
      return p[i];
  *nelp = n+1;
  /* b.o. here when width is longer than the size of key */
  return memcpy(p[n], key, width);
}
void *musl_lfind(const void *key, const void *base, size_t *nelp,
  size_t width, int (*compar)(const void *, const void *))
{
  char **p = (char **)base;
  size_t n = *nelp;
  size_t i;
  for (i = 0; i < n; i++)
    if (compar(p[i], key) == 0)
      return p[i];
  return 0;
}
/* AVL tree height < 1.44*log2(nodes+2)-0.3, MAXH is a safe upper bound.  */
struct _cee_tsearch_tnode {
  const void *key;
  void *a[2];
  int h;
};
static int height(void *n) { return n ? ((struct _cee_tsearch_tnode *)n)->h : 0; }
static int rot(void **p, struct _cee_tsearch_tnode *x, int dir /* deeper side */)
{
  struct _cee_tsearch_tnode *y = (struct _cee_tsearch_tnode *)x->a[dir];
  struct _cee_tsearch_tnode *z = (struct _cee_tsearch_tnode *)y->a[!dir];
  int hx = x->h;
  int hz = height(z);
  if (hz > height(y->a[dir])) {
    /*
     *   x
     *  / \ dir          z
     * A   y            /      *    / \   -->    x   y
y
     *   z   D        /|   |     *  / \          A B   C D
D
     * B   C
     */
    x->a[dir] = z->a[!dir];
    y->a[!dir] = z->a[dir];
    z->a[!dir] = x;
    z->a[dir] = y;
    x->h = hz;
    y->h = hz;
    z->h = hz+1;
  } else {
    /*
     *   x               y
     *  / \             /      * A   y    -->    x   D
D
     *    / \         /      *   z   D       A   z
z
     */
    x->a[dir] = z;
    y->a[!dir] = x;
    x->h = hz+1;
    y->h = hz+2;
    z = y;
  }
  *p = z;
  return z->h - hx;
}
/* balance *p, return 0 if height is unchanged.  */
static int __tsearch_balance(void **p)
{
  struct _cee_tsearch_tnode *n = (struct _cee_tsearch_tnode *)*p;
  int h0 = height(n->a[0]);
  int h1 = height(n->a[1]);
  if (h0 - h1 + 1u < 3u) {
    int old = n->h;
    n->h = h0<h1 ? h1+1 : h0+1;
    return n->h - old;
  }
  return rot(p, n, h0<h1);
}
void *musl_tsearch(void *cxt, const void *key, void **rootp,
  int (*cmp)(void *, const void *, const void *))
{
  if (!rootp)
    return 0;
  void **a[(sizeof(void*)*8*3/2)];
  struct _cee_tsearch_tnode *n = (struct _cee_tsearch_tnode *)*rootp;
  struct _cee_tsearch_tnode *r;
  int i=0;
  a[i++] = rootp;
  for (;;) {
    if (!n)
      break;
    int c = cmp(cxt, key, n->key);
    if (!c)
      return n;
    a[i++] = &n->a[c>0];
    n = (struct _cee_tsearch_tnode *)n->a[c>0];
  }
  r = (struct _cee_tsearch_tnode *)malloc(sizeof *r);
  if (!r)
    return 0;
  r->key = key;
  r->a[0] = r->a[1] = 0;
  r->h = 1;
  /* insert new tnode, rebalance ancestors.  */
  *a[--i] = r;
  while (i && __tsearch_balance(a[--i]));
  return r;
}
void musl_tdestroy(void * cxt, void *root, void (*freekey)(void *, void *))
{
  struct _cee_tsearch_tnode *r = (struct _cee_tsearch_tnode *)root;
  if (r == 0)
    return;
  musl_tdestroy(cxt, r->a[0], freekey);
  musl_tdestroy(cxt, r->a[1], freekey);
  if (freekey) freekey(cxt, (void *)r->key);
  free(r);
}
void *musl_tfind(void * cxt, const void *key, void *const *rootp,
  int(*cmp)(void * cxt, const void *, const void *))
{
  if (!rootp)
    return 0;
  struct _cee_tsearch_tnode *n = (struct _cee_tsearch_tnode *)*rootp;
  for (;;) {
    if (!n)
      break;
    int c = cmp(cxt, key, n->key);
    if (!c)
      break;
    n = (struct _cee_tsearch_tnode *)n->a[c>0];
  }
  return n;
}
static void walk(void * cxt, struct _cee_tsearch_tnode *r,
                 void (*action)(void *, const void *, VISIT, int), int d)
{
  if (!r)
    return;
  if (r->h == 1)
    action(cxt, r, leaf, d);
  else {
    action(cxt, r, preorder, d);
    walk(cxt, (struct _cee_tsearch_tnode *)r->a[0], action, d+1);
    action(cxt, r, postorder, d);
    walk(cxt, (struct _cee_tsearch_tnode *)r->a[1], action, d+1);
    action(cxt, r, endorder, d);
  }
}
void musl_twalk(void * cxt, const void *root,
                void (*action)(void *, const void *, VISIT, int))
{
  walk(cxt, (struct _cee_tsearch_tnode *)root, action, 0);
}
void *musl_tdelete(void * cxt, const void * key, void ** rootp,
  int(*cmp)(void * cxt, const void *, const void *))
{
  if (!rootp)
    return 0;
  void **a[(sizeof(void*)*8*3/2)+1];
  struct _cee_tsearch_tnode *n = (struct _cee_tsearch_tnode *)*rootp;
  struct _cee_tsearch_tnode *parent;
  struct _cee_tsearch_tnode *child;
  int i=0;
  /* *a[0] is an arbitrary non-null pointer that is returned when
     the root tnode is deleted.  */
  a[i++] = rootp;
  a[i++] = rootp;
  for (;;) {
    if (!n)
      return 0;
    int c = cmp(cxt, key, n->key);
    if (!c)
      break;
    a[i++] = &n->a[c>0];
    n = (struct _cee_tsearch_tnode *)n->a[c>0];
  }
  parent = (struct _cee_tsearch_tnode *)*a[i-2];
  if (n->a[0]) {
    /* free the preceding tnode instead of the deleted one.  */
    struct _cee_tsearch_tnode *deleted = n;
    a[i++] = &n->a[0];
    n = (struct _cee_tsearch_tnode *)n->a[0];
    while (n->a[1]) {
      a[i++] = &n->a[1];
      n = (struct _cee_tsearch_tnode *)n->a[1];
    }
    deleted->key = n->key;
    child = (struct _cee_tsearch_tnode *)n->a[0];
  } else {
    child = (struct _cee_tsearch_tnode *)n->a[1];
  }
  /* freed tnode has at most one child, move it up and rebalance.  */
  if (parent == n)
    parent = NULL;
  free(n);
  *a[--i] = child;
  while (--i && __tsearch_balance(a[i]));
  return parent;
}
void cee_trace (void *p, enum cee_trace_action ta) {
  if (!p) return;
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  cs->trace(p, ta);
}
/*
 * a generic resource delete function for all cee_* pointers
 */
void cee_del(void *p) {
  if (!p) return;
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  cs->trace(p, CEE_TRACE_DEL_FOLLOW);
}
void cee_del_ref(void *p) {
  if (!p) return;
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if (cs->in_degree) cs->in_degree --;
  /* if it's retained by an owner,
     it should be freed by cee_del
  */
  if (cs->retained) return;
  if (!cs->in_degree) cs->trace(p, CEE_TRACE_DEL_FOLLOW);
}
void cee_use_realloc(void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if (cs->resize_method)
    cs->resize_method = CEE_RESIZE_WITH_REALLOC;
}
void cee_use_malloc(void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if (cs->resize_method)
    cs->resize_method = CEE_RESIZE_WITH_MALLOC;
}
void cee_segfault() {
  volatile char * c = 0;
  *c = 0;
  __builtin_unreachable();
}
static void _cee_common_incr_rc (void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if (cs->retained) return;
  cs->in_degree ++;
}
static void _cee_common_decr_rc (void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if (cs->retained) return;
  if (cs->in_degree)
    cs->in_degree --;
  else {
    /* report warnings */
  }
}
uint16_t get_in_degree (void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  return cs->in_degree;
}
static void _cee_common_retain (void *p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  cs->retained = 1;
}
static void _cee_common_release (void * p) {
  struct cee_sect * cs = (struct cee_sect *)((void *)((char *)p - sizeof(struct cee_sect)));
  if(cs->retained)
    cs->retained = 0;
  else {
    /* report error */
    cee_segfault();
  }
}
void cee_incr_indegree (enum cee_del_policy o, void * p) {
  switch(o) {
    case CEE_DP_DEL_RC:
      _cee_common_incr_rc(p);
      break;
    case CEE_DP_DEL:
      _cee_common_retain(p);
      break;
    case CEE_DP_NOOP:
      break;
  }
}
void cee_decr_indegree (enum cee_del_policy o, void * p) {
  switch(o) {
    case CEE_DP_DEL_RC:
      _cee_common_decr_rc(p);
      break;
    case CEE_DP_DEL:
      _cee_common_release(p);
      break;
    case CEE_DP_NOOP:
      break;
  }
}
void cee_del_e (enum cee_del_policy o, void *p) {
  switch(o) {
    case CEE_DP_DEL_RC:
      cee_del_ref(p);
      break;
    case CEE_DP_DEL:
      cee_del(p);
      break;
    case CEE_DP_NOOP:
      break;
  }
}
struct _cee_boxed_header {
  enum cee_boxed_primitive_type type;
  struct cee_sect cs;
  union cee_boxed_primitive_value _[1];
};
static void _cee_boxed_chain (struct _cee_boxed_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_boxed_de_chain (struct _cee_boxed_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_boxed_header * _cee_boxed_resize(struct _cee_boxed_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_boxed_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_boxed_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_boxed_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_boxed_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_boxed_trace (void * v, enum cee_trace_action ta) {
  struct _cee_boxed_header * m = (struct _cee_boxed_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  switch(ta) {
    case CEE_TRACE_DEL_FOLLOW:
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_boxed_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      break;
  }
}
static int _cee_boxed_cmp (void * v1, void * v2) {
  struct _cee_boxed_header * h1 = (struct _cee_boxed_header *)((void *)((char *)(v1) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  struct _cee_boxed_header * h2 = (struct _cee_boxed_header *)((void *)((char *)(v2) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h1->cs.trace == h2->cs.trace)
    cee_segfault();
  else
    cee_segfault();
}
static struct _cee_boxed_header * _cee_boxed_mk_header(struct cee_state * s, enum cee_boxed_primitive_type t) {
  size_t mem_block_size = sizeof(struct _cee_boxed_header);
  struct _cee_boxed_header * b = malloc(mem_block_size);
  do{ memset(&b->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_boxed_chain(b, s);
  b->cs.trace = _cee_boxed_trace;
  b->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  b->cs.mem_block_size = mem_block_size;
  b->cs.cmp = NULL;
  b->cs.n_product = 0;
  b->type = t;
  b->_[0].u64 = 0;
  return b;
}
static int _cee_boxed_cmp_double(double v1, double v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_double (struct cee_state * s, double d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_f64);
  b->cs.cmp = (void *)_cee_boxed_cmp_double;
  b->_[0].f64 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_float(float v1, float v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_float (struct cee_state * s, float d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_f32);
  b->cs.cmp = (void *)_cee_boxed_cmp_float;
  b->_[0].f32 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_u64(uint64_t v1, uint64_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_u64 (struct cee_state * s, uint64_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_u64);
  b->_[0].u64 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_u32(uint32_t v1, uint32_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_u32 (struct cee_state * s, uint32_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_u32);
  b->cs.cmp = (void *)_cee_boxed_cmp_u32;
  b->_[0].u32 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_u16(uint16_t v1, uint16_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_u16 (struct cee_state * s, uint16_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_u16);
  b->cs.cmp = (void *) _cee_boxed_cmp_u16;
  b->_[0].u16 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_u8(uint8_t v1, uint8_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_u8 (struct cee_state * s, uint8_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_u8);
  b->cs.cmp = (void *)_cee_boxed_cmp_u8;
  b->_[0].u8 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_i64(int64_t v1, int64_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_i64 (struct cee_state *s, int64_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_i64);
  b->cs.cmp = (void *)_cee_boxed_cmp_i64;
  b->_[0].i64 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_i32(int32_t v1, int32_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_i32 (struct cee_state * s, int32_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_i32);
  b->cs.cmp = (void *)_cee_boxed_cmp_i32;
  b->_[0].i32 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_i16(int16_t v1, int16_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_i16 (struct cee_state * s, int16_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_i16);
  b->cs.cmp = (void *)_cee_boxed_cmp_i16;
  b->_[0].i16 = d;
  return (struct cee_boxed *)b->_;
}
static int _cee_boxed_cmp_i8(int8_t v1, int8_t v2) {
  if (v1 == v2)
    return 0;
  else if (v1 > v2)
    return 1;
  else
    return -1;
}
struct cee_boxed * cee_boxed_from_i8 (struct cee_state *s, int8_t d) {
  size_t mem_block_size = sizeof(struct cee_boxed);
  struct _cee_boxed_header * b = _cee_boxed_mk_header(s, cee_primitive_i8);
  b->cs.cmp = (void *)_cee_boxed_cmp_i8;
  b->_[0].i8 = d;
  return (struct cee_boxed *)b->_;
}
size_t cee_boxed_snprint (char * buf, size_t size, struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  int s;
  switch(h->type)
  {
    case cee_primitive_f64:
      s = snprintf(buf, size, "%lg", h->_[0].f64);
      break;
    case cee_primitive_f32:
      s = snprintf(buf, size, "%g", h->_[0].f32);
      break;
    case cee_primitive_i64:
      s = snprintf(buf, size, "%"PRId64, h->_[0].i64);
      break;
    case cee_primitive_u32:
    case cee_primitive_u16:
    case cee_primitive_u8:
      s = snprintf(buf, size, "%"PRIu32, h->_[0].u32);
      break;
    case cee_primitive_u64:
      s = snprintf(buf, size, "%"PRIu64, h->_[0].u64);
      break;
    case cee_primitive_i32:
    case cee_primitive_i16:
    case cee_primitive_i8:
      s = snprintf(buf, size, "%"PRId8, h->_[0].i8);
      break;
    default:
      cee_segfault();
      break;
  }
  if (s > 0)
    return (size_t)s;
  else
    cee_segfault();
}
double cee_boxed_to_double (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_f64)
    return h->_[0].f64;
  else
    cee_segfault();
}
float cee_boxed_to_float (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_f32)
    return h->_[0].f32;
  else
    cee_segfault();
}
uint64_t cee_boxed_to_u64 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_u64)
    return h->_[0].u64;
  else
    cee_segfault();
}
uint32_t cee_boxed_to_u32 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_u32)
    return h->_[0].u32;
  else
    cee_segfault();
}
uint16_t cee_boxed_to_u16 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_u16)
    return h->_[0].u16;
  else
    cee_segfault();
}
uint8_t cee_boxed_to_u8 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_u8)
    return h->_[0].u8;
  else
    cee_segfault();
}
int64_t cee_boxed_to_i64 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_i64)
    return h->_[0].i64;
  else
    cee_segfault();
}
int32_t cee_boxed_to_i32 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_i32)
    return h->_[0].i32;
  else
    cee_segfault();
}
int16_t cee_boxed_to_i16 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_i16)
    return h->_[0].i16;
  else
    cee_segfault();
}
int8_t cee_boxed_to_i8 (struct cee_boxed * x) {
  struct _cee_boxed_header * h = (struct _cee_boxed_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_boxed_header, _))));
  if (h->type == cee_primitive_i8)
    return h->_[0].i8;
  else
    cee_segfault();
}
struct _cee_str_header {
  uintptr_t capacity;
  struct cee_sect cs;
  char _[1];
};
static void _cee_str_chain (struct _cee_str_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_str_de_chain (struct _cee_str_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_str_header * _cee_str_resize(struct _cee_str_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_str_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_str_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_str_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_str_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_str_trace (void * p, enum cee_trace_action ta) {
  struct _cee_str_header * m = (struct _cee_str_header *)((void *)((char *)(p) - (__builtin_offsetof(struct _cee_str_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
    case CEE_TRACE_DEL_FOLLOW:
      _cee_str_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      break;
  }
}
struct cee_str * cee_str_mk (struct cee_state * st, const char * fmt, ...) {
  if (!fmt) {
    /* fmt cannot be null */
    /* intentionally cause a segfault */
    cee_segfault();
  }
  uintptr_t s;
  va_list ap;
  va_start(ap, fmt);
  s = vsnprintf(NULL, 0, fmt, ap);
  s ++;
  s += sizeof(struct _cee_str_header);
  s = (s / 64 + 1) * 64;
  size_t mem_block_size = s;
  struct _cee_str_header * h = malloc(mem_block_size);
  do{ memset(&h->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_str_chain(h, st);
  h->cs.trace = _cee_str_trace;
  h->cs.resize_method = CEE_RESIZE_WITH_MALLOC;
  h->cs.mem_block_size = mem_block_size;
  h->cs.cmp = (void *)strcmp;
  h->cs.cmp_stop_at_null = 1;
  h->cs.n_product = 0;
  h->capacity = s - sizeof(struct _cee_str_header);
  va_start(ap, fmt);
  vsnprintf(h->_, s, fmt, ap);
  return (struct cee_str *)(h->_);
}
struct cee_str * cee_str_mk_e (struct cee_state * st, size_t n, const char * fmt, ...) {
  uintptr_t s;
  va_list ap;
  if (fmt) {
    va_start(ap, fmt);
    s = vsnprintf(NULL, 0, fmt, ap);
    s ++; /* including the null terminator */
  }
  else
    s = n;
  s += sizeof(struct _cee_str_header);
  size_t mem_block_size = (s / 64 + 1) * 64;
  struct _cee_str_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  m->cs.trace = _cee_str_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_MALLOC;
  m->cs.mem_block_size = mem_block_size;
  m->cs.cmp = (void *)strcmp;
  m->cs.cmp_stop_at_null = 1;
  _cee_str_chain(m, st);
  m->capacity = mem_block_size - sizeof(struct _cee_str_header);
  if (fmt) {
    va_start(ap, fmt);
    vsnprintf(m->_, mem_block_size, fmt, ap);
  }
  else {
    m->_[0] = '\0'; /* terminates with '\0' */
  }
  return (struct cee_str *)(m->_);
}
static void _cee_str_noop(void * v, enum cee_trace_action ta) {}
struct cee_block * cee_block_empty () {
  static struct _cee_str_header singleton;
  singleton.cs.trace = _cee_str_noop;
  singleton.cs.resize_method = CEE_RESIZE_WITH_MALLOC;
  singleton.cs.mem_block_size = sizeof(struct _cee_str_header);
  singleton.capacity = 1;
  singleton._[0] = 0;
  return (struct cee_block *)&singleton._;
}
/*
 * if it's not NULL terminated, NULL should be returned
 */
char * cee_str_end(struct cee_str * str) {
  struct _cee_str_header * b = (struct _cee_str_header *)((void *)((char *)(str) - (__builtin_offsetof(struct _cee_str_header, _))));
  /* TODO: fixes this */
  return (char *)str + strlen((char *)str);
  /*
  int i = 0; 
  for (i = 0;i < b->used; i++)
    if (b->_[i] == '\0')
      return (b->_ + i);

  return NULL;
  */
}
/*
 * append any char (including '\0') to str;
 */
struct cee_str * cee_str_add(struct cee_str * str, char c) {
  struct _cee_str_header * b = (struct _cee_str_header *)((void *)((char *)(str) - (__builtin_offsetof(struct _cee_str_header, _))));
  uint32_t slen = strlen((char *)str);
  if (slen < b->capacity) {
    b->_[slen] = c;
    b->_[slen+1] = '\0';
    return (struct cee_str *)(b->_);
  }
  else {
    struct _cee_str_header * b1 = _cee_str_resize(b, b->cs.mem_block_size + 64);
    b1->capacity = b->capacity + 64;
    b1->_[b->capacity] = c;
    b1->_[b->capacity+1] = '\0';
    return (struct cee_str *)(b1->_);
  }
}
struct cee_str * cee_str_catf(struct cee_str * str, const char * fmt, ...) {
  struct _cee_str_header * b = (struct _cee_str_header *)((void *)((char *)(str) - (__builtin_offsetof(struct _cee_str_header, _))));
  if (!fmt)
    return str;
  size_t slen = strlen((char *)str);
  va_list ap;
  va_start(ap, fmt);
  size_t s = vsnprintf(NULL, 0, fmt, ap);
  s ++; /* including the null terminator */
  va_start(ap, fmt);
  if (slen + s < b->capacity) {
    vsnprintf(b->_ + slen, s, fmt, ap);
    return str;
  }
  else {
    struct _cee_str_header * b1 = _cee_str_resize(b, slen + s);
    vsnprintf(b1->_ + slen, s, fmt, ap);
    return (struct cee_str *)(b1->_);
  }
}
struct cee_str * cee_str_ncat (struct cee_str * str, char * s, size_t slen) {
  return NULL;
}
struct _cee_dict_header {
  struct cee_list * keys;
  struct cee_list * vals;
  uintptr_t size;
  enum cee_del_policy del_policy;
  struct cee_sect cs;
  struct musl_hsearch_data _[1];
};
static void _cee_dict_chain (struct _cee_dict_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_dict_de_chain (struct _cee_dict_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_dict_header * _cee_dict_resize(struct _cee_dict_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_dict_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_dict_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_dict_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_dict_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_dict_trace(void *d, enum cee_trace_action ta) {
  struct _cee_dict_header * m = (struct _cee_dict_header *)((void *)((char *)(d) - (__builtin_offsetof(struct _cee_dict_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      musl_hdestroy_r(m->_);
      _cee_dict_de_chain(m);
      free(m);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      cee_del_e(m->del_policy, m->keys);
      cee_del_e(m->del_policy, m->vals);
      musl_hdestroy_r(m->_);
      _cee_dict_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      cee_trace(m->keys, ta);
      cee_trace(m->vals, ta);
      break;
  }
}
struct cee_dict * cee_dict_mk_e (struct cee_state * s, enum cee_del_policy o, size_t size) {
  size_t mem_block_size = sizeof(struct _cee_dict_header);
  struct _cee_dict_header * m = malloc(mem_block_size);
  m->del_policy = o;
  m->keys = cee_list_mk(s, size);
  cee_use_realloc(m->keys);
  m->vals = cee_list_mk(s, size);
  cee_use_realloc(m->vals);
  m->size = size;
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_dict_chain(m, s);
  m->cs.trace = _cee_dict_trace;
  m->cs.mem_block_size = mem_block_size;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.n_product = 2; /* key:str, value */
  size_t hsize = (size_t)((float)size * 1.25);
  memset(m->_, 0, sizeof(struct musl_hsearch_data));
  if (musl_hcreate_r(hsize, m->_)) {
    return (struct cee_dict *)(&m->_);
  }
  else {
    cee_del(m->keys);
    cee_del(m->vals);
    free(m);
    return NULL;
  }
}
struct cee_dict * cee_dict_mk (struct cee_state *s, size_t size) {
  return cee_dict_mk_e (s, CEE_DEFAULT_DEL_POLICY, size);
}
void cee_dict_add (struct cee_dict * d, char * key, void * value) {
  struct _cee_dict_header * m = (struct _cee_dict_header *)((void *)((char *)(d) - (__builtin_offsetof(struct _cee_dict_header, _))));
  MUSL_ENTRY n, *np;
  n.key = key;
  n.data = value;
  if (!musl_hsearch_r(n, ENTER, &np, m->_))
    cee_segfault();
  cee_list_append(&m->keys, key);
  cee_list_append(&m->vals, value);
}
void * cee_dict_find(struct cee_dict * d, char * key) {
  struct _cee_dict_header * m = (struct _cee_dict_header *)((void *)((char *)(d) - (__builtin_offsetof(struct _cee_dict_header, _))));
  MUSL_ENTRY n, *np;
  n.key = key;
  n.data = NULL;
  if (musl_hsearch_r(n, FIND, &np, m->_))
    return np->data;
  printf ("%s\n", strerror(errno));
  return NULL;
}
struct _cee_map_header {
  int (*cmp)(const void *l, const void *r);
  uintptr_t size;
  enum cee_del_policy key_del_policy;
  enum cee_del_policy val_del_policy;
  enum cee_trace_action ta;
  struct cee_sect cs;
  void * _[1];
};
static void _cee_map_chain (struct _cee_map_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_map_de_chain (struct _cee_map_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_map_header * _cee_map_resize(struct _cee_map_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_map_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_map_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_map_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_map_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_map_free_pair_follow(void * ctx, void * c) {
  cee_del(c);
}
static void _cee_map_trace_pair (void * ctx, const void *nodep, const VISIT which, const int depth) {
  struct cee_tuple * p;
  struct _cee_map_header * h;
  switch (which)
  {
    case preorder:
    case leaf:
      p = *(void **)nodep;
      cee_trace(p, *(enum cee_trace_action *)ctx);
      break;
    default:
      break;
  }
}
static void _cee_map_trace(void * p, enum cee_trace_action ta) {
  struct _cee_map_header * h = (struct _cee_map_header *)((void *)((char *)(p) - (__builtin_offsetof(struct _cee_map_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      musl_tdestroy(NULL, h->_[0], NULL);
      _cee_map_de_chain(h);
      free(h);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      musl_tdestroy(&ta, h->_[0], _cee_map_free_pair_follow);
      _cee_map_de_chain(h);
      free(h);
      break;
    default:
      h->cs.gc_mark = ta - CEE_TRACE_MARK;
      h->ta = ta;
      musl_twalk(&ta, h->_[0], _cee_map_trace_pair);
      break;
  }
}
static int _cee_map_cmp (void * ctx, const void * v1, const void * v2) {
  struct _cee_map_header * h = ctx;
  struct cee_tuple * t1 = (void *)v1; /* to remove const */
  struct cee_tuple * t2 = (void *)v2;
  return h->cmp(t1->_[0], t2->_[0]);
}
struct cee_map * cee_map_mk_e (struct cee_state * st, enum cee_del_policy o[2],
                  int (*cmp)(const void *, const void *)) {
  size_t mem_block_size = sizeof(struct _cee_map_header);
  struct _cee_map_header * m = malloc(mem_block_size);
  m->cmp = cmp;
  m->size = 0;
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_map_chain(m, st);
  m->cs.trace = _cee_map_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.mem_block_size = mem_block_size;
  m->cs.cmp = 0;
  m->cs.cmp_stop_at_null = 0;
  m->cs.n_product = 2; /* key, value */
  m->key_del_policy = o[0];
  m->val_del_policy = o[1];
  m->_[0] = 0;
  return (void *)m->_;
}
struct cee_map * cee_map_mk(struct cee_state * st, int (*cmp) (const void *, const void *)) {
  static enum cee_del_policy d[2] = { CEE_DEFAULT_DEL_POLICY, CEE_DEFAULT_DEL_POLICY };
  return cee_map_mk_e(st, d, cmp);
}
uintptr_t cee_map_size(struct cee_map * m) {
  struct _cee_map_header * b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  return b->size;
}
void cee_map_add(struct cee_map * m, void * key, void * value) {
  struct _cee_map_header * b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  enum cee_del_policy d[2];
  d[0] = b->key_del_policy;
  d[1] = b->val_del_policy;
  struct cee_tuple * t = cee_tuple_mk_e(b->cs.state, d, key, value);
  struct cee_tuple **oldp = musl_tsearch(b, t, b->_, _cee_map_cmp);
  if (oldp == NULL)
    cee_segfault(); /* run out of memory */
  else if (*oldp != t)
    cee_del(t);
  else
    b->size ++;
  return;
}
void * cee_map_find(struct cee_map * m, void * key) {
  struct _cee_map_header * b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  struct cee_tuple t = { key, 0 };
  struct cee_tuple **pp = musl_tfind(b, &t, b->_, _cee_map_cmp);
  if (pp == NULL)
    return NULL;
  else {
    struct cee_tuple *p = *pp;
    return p->_[1];
  }
}
void * cee_map_remove(struct cee_map * m, void * key) {
  struct _cee_map_header *b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  void **oldp = musl_tdelete(b, key, b->_, _cee_map_cmp);
  if (oldp == NULL)
    return NULL;
  else {
    b->size --;
    struct cee_tuple *ret = *oldp;
    cee_del(ret);
    cee_decr_indegree(b->key_del_policy, ret->_[0]);
    cee_decr_indegree(b->val_del_policy, ret->_[1]);
    return ret->_[1];
  }
}
static void _cee_map_get_key (void * ctx, const void *nodep, const VISIT which, const int depth) {
  struct cee_tuple * p;
  switch (which)
  {
    case preorder:
    case leaf:
      p = *(void **)nodep;
      cee_list_append(ctx, p->_[0]);
      break;
    default:
      break;
  }
}
struct cee_list * cee_map_keys(struct cee_map * m) {
  uintptr_t n = cee_map_size(m);
  struct _cee_map_header * b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  struct cee_list * keys = cee_list_mk(b->cs.state, n);
  musl_twalk(&keys, b->_[0], _cee_map_get_key);
  return keys;
}
static void _cee_map_get_value (void * ctx, const void *nodep, const VISIT which, const int depth) {
  struct cee_tuple * p;
  switch (which)
  {
    case preorder:
    case leaf:
      p = *(void **)nodep;
      cee_list_append(ctx, p->_[1]);
      break;
    default:
      break;
  }
}
struct cee_list* cee_map_values(struct cee_map *m) {
  uintptr_t s = cee_map_size(m);
  struct _cee_map_header *b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  struct cee_list *values = cee_list_mk(b->cs.state, s);
  musl_twalk(&values, b->_[0], _cee_map_get_value);
  return values;
}
/*
 * internal structure for cee_map_iterate
 */
struct _cee_map_fn_ctx {
  void *ctx;
  void (*f)(void *ctx, void *key, void *value);
};
/*
 * helper function for cee_map_iterate
 */
static void _cee_map_apply_each (void *ctx, const void *nodep, const VISIT which, const int depth) {
  struct cee_tuple *p;
  struct _cee_map_fn_ctx * fn_ctx_p = ctx;
  switch(which)
  {
  case preorder:
  case leaf:
    p = *(void **)nodep;
    fn_ctx_p->f(fn_ctx_p->ctx, p->_[0], p->_[1]);
    break;
  default:
    break;
  }
}
/*
 * iterate
 */
void cee_map_iterate(struct cee_map *m, void *ctx,
                     void (*f)(void *ctx, void *key, void *value))
{
  struct _cee_map_header *b = (struct _cee_map_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_map_header, _))));
  struct _cee_map_fn_ctx fn_ctx = { .ctx = ctx, .f = f };
  musl_twalk(&fn_ctx, b->_[0], _cee_map_apply_each);
  return;
}
struct _cee_set_header {
  int (*cmp)(const void *l, const void *r);
  uintptr_t size;
  enum cee_del_policy del_policy;
  enum cee_trace_action ta;
  struct cee_sect cs;
  void * _[1];
};
static void _cee_set_chain (struct _cee_set_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_set_de_chain (struct _cee_set_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_set_header * _cee_set_resize(struct _cee_set_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_set_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_set_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_set_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_set_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_set_free_pair_follow (void * cxt, void * c) {
  enum cee_del_policy dp = * (enum cee_del_policy *) cxt;
  cee_del_e(dp, c);
}
static void _cee_set_trace_pair (void * cxt, const void *nodep, const VISIT which, const int depth) {
  void * p;
  struct _cee_set_header * h;
  switch (which)
  {
    case preorder:
    case leaf:
      p = *(void **)nodep;
      cee_trace(p, *((enum cee_trace_action *)cxt));
      break;
    default:
      break;
  }
}
static void _cee_set_trace(void * p, enum cee_trace_action ta) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(p) - (__builtin_offsetof(struct _cee_set_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      musl_tdestroy(NULL, h->_[0], NULL);
      _cee_set_de_chain(h);
      free(h);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      musl_tdestroy(NULL, h->_[0], _cee_set_free_pair_follow);
      _cee_set_de_chain(h);
      free(h);
      break;
    default:
      h->cs.gc_mark = ta - CEE_TRACE_MARK;
      h->ta = ta;
      musl_twalk(&ta, h->_[0], _cee_set_trace_pair);
      break;
  }
}
int _cee_set_cmp (void * cxt, const void * v1, const void *v2) {
  struct _cee_set_header * h = (struct _cee_set_header *) cxt;
  return h->cmp(v1, v2);
}
/*
 * create a new set and the equality of 
 * its two elements are decided by cmp
 * dt: specify how its elements should be handled if the set is deleted.
 */
struct cee_set * cee_set_mk_e (struct cee_state * st, enum cee_del_policy o,
                  int (*cmp)(const void *, const void *))
{
  struct _cee_set_header * m = (struct _cee_set_header *)malloc(sizeof(struct _cee_set_header));
  m->cmp = cmp;
  m->size = 0;
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_set_chain(m, st);
  m->cs.trace = _cee_set_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.n_product = 1;
  m->_[0] = NULL;
  m->del_policy = o;
  return (struct cee_set *)m->_;
}
struct cee_set * cee_set_mk (struct cee_state * s, int (*cmp)(const void *, const void *)) {
  return cee_set_mk_e(s, CEE_DEFAULT_DEL_POLICY, cmp);
}
size_t cee_set_size (struct cee_set * s) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(s) - (__builtin_offsetof(struct _cee_set_header, _))));
  return h->size;
}
bool cee_set_empty (struct cee_set * s) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(s) - (__builtin_offsetof(struct _cee_set_header, _))));
  return h->size == 0;
}
/*
 * add an element key to the set m
 * 
 */
void cee_set_add(struct cee_set *m, void * val) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_set_header, _))));
  void ** oldp = (void **) musl_tsearch(h, val, h->_, _cee_set_cmp);
  if (oldp == NULL)
    cee_segfault();
  else if (*oldp != (void *)val)
    return;
  else {
    h->size ++;
    cee_incr_indegree(h->del_policy, val);
  }
  return;
}
static void _cee_set_del(void * cxt, void * p) {
  enum cee_del_policy dp = *((enum cee_del_policy *)cxt);
  switch(dp) {
    case CEE_DP_DEL_RC:
      cee_del_ref(p);
      break;
    case CEE_DP_DEL:
      cee_del(p);
      break;
    case CEE_DP_NOOP:
      break;
  }
}
void cee_set_clear (struct cee_set * s) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(s) - (__builtin_offsetof(struct _cee_set_header, _))));
  musl_tdestroy(&h->del_policy, h->_[0], _cee_set_del);
  h->_[0] = NULL;
  h->size = 0;
}
void * cee_set_find(struct cee_set *m, void * key) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_set_header, _))));
  void **oldp = (void **) musl_tfind(h, key, h->_, _cee_set_cmp);
  if (oldp == NULL)
    return NULL;
  else
    return *oldp;
}
static void _cee_set_get_value (void * cxt, const void *nodep, const VISIT which, const int depth) {
  void * p;
  switch (which)
  {
    case preorder:
    case leaf:
      p = *(void **)nodep;
      cee_list_append((struct cee_list **)cxt, p);
      break;
    default:
      break;
  }
}
struct cee_list * cee_set_values(struct cee_set * m) {
  uintptr_t s = cee_set_size(m);
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_set_header, _))));
  struct cee_list *values = cee_list_mk(h->cs.state, s);
  cee_use_realloc(values);
  musl_twalk(&values, h->_[0], _cee_set_get_value);
  return values;
}
void * cee_set_remove(struct cee_set *m, void * key) {
  struct _cee_set_header * h = (struct _cee_set_header *)((void *)((char *)(m) - (__builtin_offsetof(struct _cee_set_header, _))));
  void ** old = (void **)musl_tfind(h, key, h->_, _cee_set_cmp);
  if (old == NULL)
    return NULL;
  else {
    h->size --;
    void * k = *old;
    musl_tdelete(h, key, h->_, _cee_set_cmp);
    return k;
  }
}
struct cee_set * cee_set_union_set (struct cee_state * s, struct cee_set * s1, struct cee_set * s2) {
  struct _cee_set_header * h1 = (struct _cee_set_header *)((void *)((char *)(s1) - (__builtin_offsetof(struct _cee_set_header, _))));
  struct _cee_set_header * h2 = (struct _cee_set_header *)((void *)((char *)(s2) - (__builtin_offsetof(struct _cee_set_header, _))));
  if (h1->cmp == h2->cmp) {
    struct cee_set * s0 = cee_set_mk(s, h1->cmp);
    struct cee_list * v1 = cee_set_values(s1);
    struct cee_list * v2 = cee_set_values(s2);
    int i;
    for (i = 0; i < cee_list_size(v1); i++)
      cee_set_add(s0, v1->_[i]);
    for (i = 0; i < cee_list_size(v2); i++)
      cee_set_add(s0, v2->_[i]);
    cee_del(v1);
    cee_del(v2);
    return s0;
  } else
    cee_segfault();
  return NULL;
}
struct _cee_stack_header {
  uintptr_t used;
  uintptr_t top;
  uintptr_t capacity;
  enum cee_del_policy del_policy;
  struct cee_sect cs;
  void * _[];
};
static void _cee_stack_chain (struct _cee_stack_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_stack_de_chain (struct _cee_stack_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_stack_header * _cee_stack_resize(struct _cee_stack_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_stack_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_stack_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_stack_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_stack_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_stack_trace (void * v, enum cee_trace_action ta) {
  struct _cee_stack_header * m = (struct _cee_stack_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_stack_header, _))));
  int i;
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_stack_de_chain(m);
      free(m);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < m->used; i++)
        cee_del_e(m->del_policy, m->_[i]);
      _cee_stack_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < m->used; i++)
        cee_trace(m->_[i], ta);
      break;
  }
}
struct cee_stack * cee_stack_mk_e (struct cee_state * st, enum cee_del_policy o, size_t size) {
  uintptr_t mem_block_size = sizeof(struct _cee_stack_header) + size * sizeof(void *);
  struct _cee_stack_header * m = malloc(mem_block_size);
  m->capacity = size;
  m->used = 0;
  m->top = (0-1);
  m->del_policy = o;
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_stack_chain(m, st);
  m->cs.trace = _cee_stack_trace;
  m->cs.mem_block_size = mem_block_size;
  return (struct cee_stack *)(m->_);
}
struct cee_stack * cee_stack_mk (struct cee_state * st, size_t size) {
  return cee_stack_mk_e(st, CEE_DEFAULT_DEL_POLICY, size);
}
int cee_stack_push (struct cee_stack * v, void *e) {
  struct _cee_stack_header * m = (struct _cee_stack_header *)((void *)((char *)((void **)v) - (__builtin_offsetof(struct _cee_stack_header, _))));
  if (m->used == m->capacity)
    return 0;
  m->top ++;
  m->used ++;
  m->_[m->top] = e;
  cee_incr_indegree(m->del_policy, e);
  return 1;
}
void * cee_stack_pop (struct cee_stack * v) {
  struct _cee_stack_header * b = (struct _cee_stack_header *)((void *)((char *)((void **)v) - (__builtin_offsetof(struct _cee_stack_header, _))));
  if (b->used == 0) {
    return NULL;
  }
  else {
    void * p = b->_[b->top];
    b->used --;
    b->top --;
    cee_decr_indegree(b->del_policy, p);
    return p;
  }
}
/*
 *  nth: 0 -> the topest element
 *       1 -> 1 element way from the topest element
 */
void * cee_stack_top (struct cee_stack * v, uintptr_t nth) {
  struct _cee_stack_header * b = (struct _cee_stack_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_stack_header, _))));
  if (b->used == 0 || nth >= b->used)
    return NULL;
  else
    return b->_[b->top-nth];
}
uintptr_t cee_stack_size (struct cee_stack *x) {
  struct _cee_stack_header * m = (struct _cee_stack_header *)((void *)((char *)((void **)x) - (__builtin_offsetof(struct _cee_stack_header, _))));
  return m->used;
}
bool cee_stack_empty (struct cee_stack *x) {
  struct _cee_stack_header * b = (struct _cee_stack_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_stack_header, _))));
  return b->used == 0;
}
bool cee_stack_full (struct cee_stack *x) {
  struct _cee_stack_header * b = (struct _cee_stack_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_stack_header, _))));
  return b->used >= b->capacity;
}
struct _cee_tuple_header {
  enum cee_del_policy del_policies[2];
  struct cee_sect cs;
  void * _[2];
};
static void _cee_tuple_chain (struct _cee_tuple_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_tuple_de_chain (struct _cee_tuple_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_tuple_header * _cee_tuple_resize(struct _cee_tuple_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_tuple_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_tuple_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_tuple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_tuple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_tuple_trace(void * v, enum cee_trace_action ta) {
  struct _cee_tuple_header * b = (struct _cee_tuple_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_tuple_header, _))));
  int i;
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_tuple_de_chain(b);
      free(b);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < 2; i++)
        cee_del_e(b->del_policies[i], b->_[i]);
      _cee_tuple_de_chain(b);
      free(b);
      break;
    default:
      b->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < 2; i++)
        cee_trace(b->_[i], ta);
      break;
  }
}
struct cee_tuple * cee_tuple_mk_e (struct cee_state * st, enum cee_del_policy o[2], void * v1, void * v2) {
  size_t mem_block_size = sizeof(struct _cee_tuple_header);
  struct _cee_tuple_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_tuple_chain(m, st);
  m->cs.trace = _cee_tuple_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
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
struct _cee_triple_header {
  enum cee_del_policy del_policies[3];
  struct cee_sect cs;
  void * _[3];
};
static void _cee_triple_chain (struct _cee_triple_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_triple_de_chain (struct _cee_triple_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_triple_header * _cee_triple_resize(struct _cee_triple_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_triple_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_triple_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_triple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_triple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_triple_trace(void * v, enum cee_trace_action ta) {
  struct _cee_triple_header * b = (struct _cee_triple_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_triple_header, _))));
  int i;
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_triple_de_chain(b);
      free(b);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < 3; i++)
        cee_del_e(b->del_policies[i], b->_[i]);
      _cee_triple_de_chain(b);
      free(b);
      break;
    default:
      b->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < 3; i++)
        cee_trace(b->_[i], ta);
      break;
  }
}
struct cee_triple * cee_triple_mk_e (struct cee_state * st, enum cee_del_policy o[3], void * v1, void * v2, void * v3) {
  size_t mem_block_size = sizeof(struct _cee_triple_header);
  struct _cee_triple_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_triple_chain(m, st);
  m->cs.trace = _cee_triple_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.mem_block_size = mem_block_size;
  m->cs.state = st;
  m->_[0] = v1;
  m->_[1] = v2;
  m->_[2] = v3;
  int i;
  for (i = 0; i < 3; i++) {
    m->del_policies[i] = o[i];
    cee_incr_indegree(o[i], m->_[i]);
  }
  return (struct cee_triple *)&m->_;
}
struct cee_triple * cee_triple_mk (struct cee_state * st, void * v1, void * v2, void *v3) {
  static enum cee_del_policy o[3] = { CEE_DEFAULT_DEL_POLICY,
                                 CEE_DEFAULT_DEL_POLICY,
                                 CEE_DEFAULT_DEL_POLICY };
  return cee_triple_mk_e(st, o, v1, v2, v3);
}
struct _cee_quadruple_header {
  enum cee_del_policy del_policies[4];
  struct cee_sect cs;
  void * _[4];
};
static void _cee_quadruple_chain (struct _cee_quadruple_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_quadruple_de_chain (struct _cee_quadruple_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_quadruple_header * _cee_quadruple_resize(struct _cee_quadruple_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_quadruple_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_quadruple_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_quadruple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_quadruple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_quadruple_trace(void * v, enum cee_trace_action ta) {
  struct _cee_quadruple_header * b = (struct _cee_quadruple_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_quadruple_header, _))));
  int i;
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_quadruple_de_chain(b);
      free(b);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < 4; i++)
        cee_del_e(b->del_policies[i], b->_[i]);
      _cee_quadruple_de_chain(b);
      free(b);
      break;
    default:
      b->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < 4; i++)
        cee_trace(b->_[i], ta);
      break;
  }
}
struct cee_quadruple * cee_quadruple_mk_e (struct cee_state * st, enum cee_del_policy o[4],
                        void * v1, void * v2, void * v3, void * v4) {
  size_t mem_block_size = sizeof(struct _cee_quadruple_header);
  struct _cee_quadruple_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_quadruple_chain(m, st);
  m->cs.trace = _cee_quadruple_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.mem_block_size = mem_block_size;
  m->cs.n_product = 4;
  m->cs.state = st;
  m->_[0] = v1;
  m->_[1] = v2;
  m->_[2] = v3;
  m->_[3] = v4;
  int i;
  for (i = 0; i < 4; i++) {
    m->del_policies[i] = o[i];
    cee_incr_indegree(o[i], m->_[i]);
  }
  return (struct cee_quadruple *)&m->_;
}
struct _cee_list_header {
  uintptr_t size;
  uintptr_t capacity;
  enum cee_del_policy del_policy;
  struct cee_sect cs;
  void * _[];
};
static void _cee_list_chain (struct _cee_list_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_list_de_chain (struct _cee_list_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_list_header * _cee_list_resize(struct _cee_list_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_list_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_list_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_list_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_list_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_list_trace (void * v, enum cee_trace_action ta) {
  struct _cee_list_header * m = (struct _cee_list_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_list_header, _))));
  int i;
  switch(ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_list_de_chain(m);
      free(m);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < m->size; i++)
        cee_del_e(m->del_policy, m->_[i]);
      _cee_list_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < m->size; i++)
        cee_trace(m->_[i], ta);
      break;
  }
}
struct cee_list * cee_list_mk_e (struct cee_state * st, enum cee_del_policy o, size_t cap) {
  size_t mem_block_size = sizeof(struct _cee_list_header) + cap * sizeof(void *);
  struct _cee_list_header * m = malloc(mem_block_size);
  m->capacity = cap;
  m->size = 0;
  m->del_policy = o;
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_list_chain(m, st);
  m->cs.trace = _cee_list_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_MALLOC;
  m->cs.mem_block_size = mem_block_size;
  return (struct cee_list *)(m->_);
}
struct cee_list * cee_list_mk (struct cee_state * s, size_t cap) {
  return cee_list_mk_e(s, CEE_DEFAULT_DEL_POLICY, cap);
}
struct cee_list * cee_list_append (struct cee_list ** l, void *e) {
  struct cee_list * v = *l;
  struct _cee_list_header * m = (struct _cee_list_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_list_header, _))));
  size_t capacity = m->capacity;
  size_t extra_cap = capacity ? capacity : 1;
  if (m->size == m->capacity) {
    size_t new_mem_block_size = m->cs.mem_block_size + extra_cap * sizeof(void *);
    struct _cee_list_header * m1 = _cee_list_resize(m, new_mem_block_size);
    m1->capacity = capacity + extra_cap;
    *l = (struct cee_list *)m1->_;
    m = m1;
  }
  m->_[m->size] = e;
  m->size ++;
  cee_incr_indegree(m->del_policy, e);
  return *l;
}
struct cee_list * cee_list_insert(struct cee_state * s, struct cee_list ** l, int index, void *e) {
  struct cee_list * v = *l;
  if (v == NULL) {
    v = cee_list_mk(s, 10);
    cee_use_realloc(v);
  }
  struct _cee_list_header * m = (struct _cee_list_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_list_header, _))));
  size_t capacity = m->capacity;
  size_t extra_cap = capacity ? capacity : 1;
  if (m->size == m->capacity) {
    size_t new_mem_block_size = m->cs.mem_block_size + extra_cap * sizeof(void *);
    struct _cee_list_header * m1 = _cee_list_resize(m, new_mem_block_size);
    m1->capacity = capacity + extra_cap;
    *l = (struct cee_list *)m1->_;
    m = m1;
  }
  int i;
  for (i = m->size; i > index; i--)
    m->_[i] = m->_[i-1];
  m->_[index] = e;
  m->size ++;
  cee_incr_indegree(m->del_policy, e);
  return *l;
}
bool cee_list_remove(struct cee_list * v, int index) {
  struct _cee_list_header * m = (struct _cee_list_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_list_header, _))));
  if (index >= m->size) return false;
  void * e = m->_[index];
  m->_[index] = 0;
  int i;
  for (i = index; i < (m->size - 1); i++)
    m->_[i] = m->_[i+1];
  m->size --;
  cee_decr_indegree(m->del_policy, e);
  return true;
}
size_t cee_list_size (struct cee_list *x) {
  struct _cee_list_header * m = (struct _cee_list_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_list_header, _))));
  return m->size;
}
size_t cee_list_capacity (struct cee_list *x) {
  struct _cee_list_header * h = (struct _cee_list_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_list_header, _))));
  return h->capacity;
}
void cee_list_iterate (struct cee_list *x, void *ctx,
         void (*f)(void *cxt, int idx, void *e)) {
  struct _cee_list_header *m = (struct _cee_list_header *)((void *)((char *)(x) - (__builtin_offsetof(struct _cee_list_header, _))));
  int i;
  for (i = 0; i < m->size; i++)
    f(ctx, i, m->_[i]);
  return;
}
struct _cee_tagged_header {
  enum cee_del_policy del_policy;
  struct cee_sect cs;
  struct cee_tagged _;
};
static void _cee_tagged_chain (struct _cee_tagged_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_tagged_de_chain (struct _cee_tagged_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_tagged_header * _cee_tagged_resize(struct _cee_tagged_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_tagged_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_tagged_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_tagged_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_tagged_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_tagged_trace (void * v, enum cee_trace_action ta) {
  struct _cee_tagged_header * m = (struct _cee_tagged_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_tagged_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_tagged_de_chain(m);
      free(m);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      cee_del_e(m->del_policy, m->_.ptr._);
      _cee_tagged_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      cee_trace(m->_.ptr._, ta);
      break;
  }
}
struct cee_tagged * cee_tagged_mk_e (struct cee_state * st, enum cee_del_policy o, uintptr_t tag, void *p) {
  size_t mem_block_size = sizeof(struct _cee_tagged_header);
  struct _cee_tagged_header * b = malloc(mem_block_size);
  do{ memset(&b->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_tagged_chain(b, st);
  b->cs.trace = _cee_tagged_trace;
  b->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  b->cs.mem_block_size = mem_block_size;
  b->_.tag = tag;
  b->_.ptr._ = p;
  b->del_policy = o;
  cee_incr_indegree(o, p);
  return &b->_;
}
struct cee_tagged * cee_tagged_mk (struct cee_state * st, uintptr_t tag, void *p) {
  return cee_tagged_mk_e(st, CEE_DEFAULT_DEL_POLICY, tag, p);
}
struct _cee_singleton_header {
  struct cee_sect cs;
  uintptr_t _; /* tag */
  uintptr_t val;
};
/*
 * the parameter of this function has to be a global/static 
 * uintptr_t array of two elements
 */
/*
 * singleton should never be deleted, hence we pass a noop
 */
static void _cee_singleton_noop(void *p, enum cee_trace_action ta) {}
struct cee_singleton * cee_singleton_init(void *s, uintptr_t tag, uintptr_t val) {
  struct _cee_singleton_header * b = (struct _cee_singleton_header* )s;
  do{ memset(&b->cs, 0, sizeof(struct cee_sect)); } while(0);;
  b->cs.trace = _cee_singleton_noop;
  b->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  b->cs.mem_block_size = 0;
  b->cs.n_product = 0;
  b->_ = tag;
  b->val = val;
  return (struct cee_singleton *)&(b->_);
}
struct _cee_closure_header {
  struct cee_sect cs;
  struct cee_closure _;
};
static void _cee_closure_chain (struct _cee_closure_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_closure_de_chain (struct _cee_closure_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_closure_header * _cee_closure_resize(struct _cee_closure_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_closure_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_closure_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_closure_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_closure_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_closure_trace (void * v, enum cee_trace_action sa) {
  struct _cee_closure_header * m = (struct _cee_closure_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_closure_header, _))));
  switch (sa) {
    case CEE_TRACE_DEL_NO_FOLLOW:
    case CEE_TRACE_DEL_FOLLOW:
      _cee_closure_de_chain(m);
      free(m);
      break;
    default:
      break;
  }
}
struct cee_closure * cee_closure_mk (struct cee_state * s, struct cee_env * env, void * fun) {
  size_t mem_block_size = sizeof(struct _cee_closure_header);
  struct _cee_closure_header * b = malloc(mem_block_size);
  do{ memset(&b->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_closure_chain(b, s);
  b->cs.trace = _cee_closure_trace;
  b->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  b->cs.mem_block_size = mem_block_size;
  b->_.env = env;
  b->_.vfun = fun;
  return &(b->_);
}
void * cee_closure_call (struct cee_state * s, struct cee_closure * c, size_t n, ...) {
  va_list ap;
  va_start(ap, n);
  void *ret = c->vfun(s, c->env, n, ap);
  va_end(ap);
  return ret;
}
struct _cee_block_header {
  uintptr_t capacity;
  enum cee_del_policy del_policy;
  struct cee_sect cs;
  char _[1]; /* actual data */
};
static void _cee_block_chain (struct _cee_block_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_block_de_chain (struct _cee_block_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_block_header * _cee_block_resize(struct _cee_block_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_block_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_block_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_block_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_block_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_block_trace (void * p, enum cee_trace_action ta) {
  struct _cee_block_header * m = (struct _cee_block_header *)((void *)((char *)(p) - (__builtin_offsetof(struct _cee_block_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_FOLLOW:
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_block_de_chain(m);
      free(m);
      break;
    default:
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      break;
  }
}
static void _cee_block_mark (void * p) {
};
void * cee_block_mk (struct cee_state * s, size_t n) {
  size_t mem_block_size;
  va_list ap;
  mem_block_size = n + sizeof(struct _cee_block_header);
  struct _cee_block_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  m->del_policy = CEE_DP_DEL_RC;
  _cee_block_chain(m, s);
  m->cs.trace = _cee_block_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_MALLOC;
  m->cs.mem_block_size = mem_block_size;
  m->cs.cmp = (void *)memcmp;
  m->capacity = n;
  return (struct cee_block *)(m->_);
}
struct _cee_n_tuple_header {
  enum cee_del_policy del_policies[16];
  struct cee_sect cs;
  void * _[16];
};
static void _cee_n_tuple_chain (struct _cee_n_tuple_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_n_tuple_de_chain (struct _cee_n_tuple_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_n_tuple_header * _cee_n_tuple_resize(struct _cee_n_tuple_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_n_tuple_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_n_tuple_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_n_tuple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_n_tuple_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_n_tuple_trace(void * v, enum cee_trace_action ta) {
  struct _cee_n_tuple_header * b = (struct _cee_n_tuple_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_n_tuple_header, _))));
  int i;
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_n_tuple_de_chain(b);
      free(b);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      for (i = 0; i < b->cs.n_product; i++)
        cee_del_e(b->del_policies[i], b->_[i]);
      _cee_n_tuple_de_chain(b);
      free(b);
      break;
    default:
      b->cs.gc_mark = ta - CEE_TRACE_MARK;
      for (i = 0; i < b->cs.n_product; i++)
        cee_trace(b->_[i], ta);
      break;
  }
}
static struct _cee_n_tuple_header * cee_n_tuple_v (struct cee_state * st, size_t ntuple,
                                         enum cee_del_policy o[], va_list ap) {
  if (ntuple > 16)
    cee_segfault();
  size_t mem_block_size = sizeof(struct _cee_n_tuple_header);
  struct _cee_n_tuple_header * m = malloc(mem_block_size);
  do{ memset(&m->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_n_tuple_chain(m, st);
  m->cs.trace = _cee_n_tuple_trace;
  m->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  m->cs.mem_block_size = mem_block_size;
  m->cs.n_product = ntuple;
  int i;
  for(i = 0; i < ntuple; i++) {
    m->_[i] = va_arg(ap, void *);
    m->del_policies[i] = o[i];
    cee_incr_indegree(o[i], m->_[i]);
  }
  return m;
}
struct cee_n_tuple * cee_n_tuple_mk (struct cee_state * st, size_t ntuple, ...) {
  va_list ap;
  va_start(ap, ntuple);
  enum cee_del_policy * o = malloc(ntuple * sizeof (enum cee_del_policy));
  int i;
  for (i = 0; i < ntuple; i++)
    o[i] = CEE_DEFAULT_DEL_POLICY;
  struct _cee_n_tuple_header * h = cee_n_tuple_v(st, ntuple, o, ap);
  free(o);
  return (struct cee_n_tuple *)(h->_);
}
struct _cee_env_header {
  enum cee_del_policy env_dp;
  enum cee_del_policy vars_dp;
  struct cee_sect cs;
  struct cee_env _;
};
static void _cee_env_chain (struct _cee_env_header * h, struct cee_state * st) {
  h->cs.state = st;
  h->cs.trace_prev = st->trace_tail;
  st->trace_tail->trace_next = &h->cs;
  st->trace_tail = &h->cs;
}
static void _cee_env_de_chain (struct _cee_env_header * h) {
  struct cee_state * st = h->cs.state;
  struct cee_sect * prev = h->cs.trace_prev;
  struct cee_sect * next = h->cs.trace_next;
  if (st->trace_tail == &h->cs) {
    prev->trace_next = NULL;
    st->trace_tail = prev;
  }
  else {
    prev->trace_next = next;
    if (next)
      next->trace_prev = prev;
  }
}
static struct _cee_env_header * _cee_env_resize(struct _cee_env_header * h, size_t n)
{
  struct cee_state * state = h->cs.state;
  struct _cee_env_header * ret;
  switch(h->cs.resize_method)
  {
    case CEE_RESIZE_WITH_REALLOC:
      _cee_env_de_chain(h);
     ret = realloc(h, n);
      ret->cs.mem_block_size = n;
      _cee_env_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_MALLOC:
     ret = malloc(n);
     memcpy(ret, h, h->cs.mem_block_size);
      ret->cs.mem_block_size = n;
      _cee_env_chain(ret, state);
      break;
    case CEE_RESIZE_WITH_IDENTITY:
      ret = h;
      break;
  }
  return ret;
}
static void _cee_env_trace (void * v, enum cee_trace_action ta) {
  struct _cee_env_header * h = (struct _cee_env_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_env_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_NO_FOLLOW:
      _cee_env_de_chain(h);
      free(h);
      break;
    case CEE_TRACE_DEL_FOLLOW:
      cee_del_e(h->env_dp, h->_.outer);
      cee_del_e(h->vars_dp, h->_.vars);
      _cee_env_de_chain(h);
      free(h);
      break;
    default:
      h->cs.gc_mark = ta - CEE_TRACE_MARK;
      cee_trace(h->_.outer, ta);
      cee_trace(h->_.vars, ta);
      break;
  }
}
struct cee_env * cee_env_mk_e(struct cee_state * st, enum cee_del_policy dp[2], struct cee_env * outer, struct cee_map * vars) {
  size_t mem_block_size = sizeof(struct _cee_env_header);
  struct _cee_env_header * h = malloc(mem_block_size);
  do{ memset(&h->cs, 0, sizeof(struct cee_sect)); } while(0);;
  _cee_env_chain(h, st);
  h->cs.trace = _cee_env_trace;
  h->cs.resize_method = CEE_RESIZE_WITH_IDENTITY;
  h->cs.mem_block_size = mem_block_size;
  h->cs.cmp = NULL;
  h->cs.n_product = 0;
  h->env_dp = dp[0];
  h->vars_dp = dp[1];
  h->_.outer = outer;
  h->_.vars = vars;
  return &h->_;
}
struct cee_env * cee_env_mk(struct cee_state * st, struct cee_env * outer, struct cee_map * vars) {
  enum cee_del_policy dp[2] = { CEE_DEFAULT_DEL_POLICY, CEE_DEFAULT_DEL_POLICY };
  return cee_env_mk_e (st, dp, outer, vars);
}
void * cee_env_find(struct cee_env * e, char * key) {
  struct cee_boxed * ret;
  while (e && e->vars) {
    if ( (ret = cee_map_find(e->vars, key)) )
      return ret;
    e = e->outer;
  }
  return NULL;
}
struct _cee_state_header {
  struct cee_sect cs;
  struct cee_state _;
};
static void _cee_state_trace (void * v, enum cee_trace_action ta) {
  struct _cee_state_header * m = (struct _cee_state_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_state_header, _))));
  switch (ta) {
    case CEE_TRACE_DEL_FOLLOW:
    {
      /* following this tracing chain but not the relations */
      struct cee_sect * tail = m->_.trace_tail;
      while (tail != &m->cs) {
        cee_trace(tail + 1, CEE_TRACE_DEL_NO_FOLLOW);
        tail = m->_.trace_tail;
      }
      free(m);
      break;
    }
    case CEE_TRACE_DEL_NO_FOLLOW:
    {
      /* TODO detach the this state from all memory blocks */
      free(m);
      break;
    }
    default:
    {
      m->cs.gc_mark = ta - CEE_TRACE_MARK;
      cee_trace(m->_.roots, ta);
      cee_trace(m->_.stack, ta);
      cee_trace(m->_.contexts, ta);
      break;
    }
  }
}
static void _cee_state_sweep (void * v, enum cee_trace_action ta) {
  struct _cee_state_header * m = (struct _cee_state_header *)((void *)((char *)(v) - (__builtin_offsetof(struct _cee_state_header, _))));
  struct cee_sect * head = &m->cs;
  while (head != NULL) {
    struct cee_sect * next = head->trace_next;
    if (head->gc_mark != ta - CEE_TRACE_MARK)
      cee_trace(head + 1, CEE_TRACE_DEL_NO_FOLLOW);
    head = next;
  }
}
static int _cee_state_cmp (const void * v1, const void * v2) {
  printf("%p %p\n", v1, v2);
  if (v1 < v2)
    return -1;
  else if (v1 == v2)
    return 0;
  else
    return 1;
}
struct cee_state * cee_state_mk(size_t n) {
  size_t memblock_size = sizeof(struct _cee_state_header);
  struct _cee_state_header * h = malloc(memblock_size);
  do{ memset(&h->cs, 0, sizeof(struct cee_sect)); } while(0);;
  h->cs.trace = _cee_state_trace;
  h->_.trace_tail = &h->cs; /* points to self; */
  struct cee_set * roots = cee_set_mk_e(&h->_, CEE_DP_NOOP, _cee_state_cmp);
  h->_.roots = roots;
  h->_.next_mark = 1;
  h->_.stack = cee_stack_mk(&h->_, n);
  h->_.contexts = cee_map_mk(&h->_, (cee_cmp_fun)strcmp);
  return &h->_;
}
void cee_state_add_gc_root(struct cee_state * s, void * key) {
  cee_set_add(s->roots, key);
}
void cee_state_remove_gc_root(struct cee_state * s, void * key) {
  cee_set_remove(s->roots, key);
}
void cee_state_add_context (struct cee_state * s, char * key, void * val) {
  cee_map_add(s->contexts, key, val);
}
void cee_state_remove_context (struct cee_state * s, char * key) {
  cee_map_remove(s->contexts, key);
}
void * cee_state_get_context (struct cee_state * s, char * key) {
  return cee_map_find(s->contexts, key);
}
void cee_state_gc (struct cee_state * s) {
  struct _cee_state_header * h = (struct _cee_state_header *)((void *)((char *)(s) - (__builtin_offsetof(struct _cee_state_header, _))));
  int mark = CEE_TRACE_MARK + s->next_mark;
  cee_trace(s, (enum cee_trace_action)mark);
  _cee_state_sweep(s, (enum cee_trace_action) mark);
  if (s->next_mark == 0) {
    s->next_mark = 1;
  } else {
    s->next_mark = 0;
  }
}
#endif

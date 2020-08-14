# A single file C container library

It provides similar functionalities of C++ STL, but it does not intend to replicate C++ STL function by function.

## It's optimized for the following use cases:

1. If you need a simple minimum C style containers. 
                                                                                
2. If you want to develop your own dynamic typed scripting languages but 
   you don't want reinvent a runtime system.

## How to use it ?

1. Download the two files;
```
  wget https://raw.githubusercontent.com/cee-studio/cee-std/master/release/cee.h
  wget https://raw.githubusercontent.com/cee-studio/cee-std/master/release/cee.c
```
2. Add them to your source folder


## Examples

**string**

```
  #include "cee.h"

  struct cee_str * s, * s1, * s2;
  
  s = cee_str("the number ten: %d", 10);
  printf("%s\n", (char *)s);
  
  s1 = cee_str("the number ten point three: %.1f", 10.3);
  printf("%s\n", (char *)e);
  
  s2 = cee_str("%s, %s", s, s1);
  printf("%s\n", (char *)s2);

  // delete strings
  cee_del(s);
  cee_del(s1);
  cee_del(s2);
```

**vector**
```
  #include "cee.h"
  struct cee_vect *v;
  
  v = cee_vect(1);
  v = cee_vect_append(v, cee_str("1"));
  v = cee_vect_append(v, cee_str("2"));
  v = cee_vect_append(v, cee_str("3"));
  
  printf("v.count %u\n", cee_vect_count(v));
  for (int i = 0; i < cee_vect_count(v); i++) {
    printf ("%d:%s\n", i, (char *)v->_[i]);
  }

  // delete vector
  cee_del(v);
```

**set**
```
  #include "cee.h"

  struct cee_set * st = cee_set((cee_cmp_fun)strcmp);
  printf ("st: %p\n", st);
  cee_set_add(st, cee_str("a"));
  cee_set_add(st, cee_str("aabc"));
  char * p = cee_set_find(st, "aabc");
  printf ("%s\n", p);

  // delete set 
  cee_del(st);
```

**map**
```
  #include "cee.h"

  struct cee_map * mp = cee_map((cee_cmp_fun)strcmp);  
  cee_map_add(mp, cee_str("1"), cee_box_i32(10));
  cee_map_add(mp, cee_str("2"), cee_box_i32(20));
  cee_map_add(mp, cee_str("3"), cee_box_i32(30));
  
  void * t = cee_map_find(mp, "1");
  printf ("found value %u\n", (uintptr_t)t);
  
  struct cee_vect * keys = cee_map_keys(mp);
  for (int i = 0; i < cee_vect_count(keys); i++) {
    printf ("[%d] key:%s\n", i, (char *)keys->_[i]);
  }
 
  // delete map
  cee_del(mp);
```

**stack**
```
  #include "cee.h"

  struct cee_stack * sp = cee_stack_o(cee_non_owner, 100);
  cee_stack_push(sp, "1");
  cee_stack_push(sp, "2");
  cee_stack_push(sp, "3");
  printf ("%s\n", cee_stack_top(sp, 0));

  // delete stack, optional
  cee_del(stack);
```

**free any memory blocks of `struct cee_*`**

any memory blocks pointed by a `cee struct` can be freed with `cee_del` like the following:
```
   #include "cee.h"

   struct cee_str * s = ..;
   cee_del(s);

   struct cee_vect * v = ..;
   cee_del(v);

   struct cee_map * m = ..;
   cee_del(m);

   struct cee_stack * sp = ..;
   cee_del(sp);
```

## How to test/develop it ?

### Using https://cee.studio cloud IDE
1. click [https://cee.studio/?bucket=orca&name=cee-std](https://cee.studio/?bucket=orca&name=cee-std)
2. clone to my account
3. click Start
4. run it Terminal

All memory access violations will be detected and reproted. 


### Using your computer

```
git clone https://github.com/cee-studio/cee-std.git
cd cee-std
make
./a.out
```

## Rationale

[DESIGN](./DESIGN.md)


## Contirubtion is welcome

Please join our discord [https://discord.gg/nBUqrWf](https://discord.gg/nBUqrWf)
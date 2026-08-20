#ifndef TCC_STUB_STDATOMIC_H
#define TCC_STUB_STDATOMIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum memory_order {
	memory_order_relaxed = 0,
	memory_order_consume = 1,
	memory_order_acquire = 2,
	memory_order_release = 3,
	memory_order_acq_rel = 4,
	memory_order_seq_cst = 5
} memory_order;

#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#define ATOMIC_POINTER_LOCK_FREE 2

typedef _Atomic(_Bool) atomic_bool;
typedef _Atomic(char) atomic_char;
typedef _Atomic(signed char) atomic_schar;
typedef _Atomic(unsigned char) atomic_uchar;
typedef _Atomic(short) atomic_short;
typedef _Atomic(unsigned short) atomic_ushort;
typedef _Atomic(int) atomic_int;
typedef _Atomic(unsigned int) atomic_uint;
typedef _Atomic(long) atomic_long;
typedef _Atomic(unsigned long) atomic_ulong;
typedef _Atomic(long long) atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(uint_least8_t) atomic_uint_least8_t;
typedef _Atomic(uint_least16_t) atomic_uint_least16_t;
typedef _Atomic(uint_least32_t) atomic_uint_least32_t;
typedef _Atomic(uint_least64_t) atomic_uint_least64_t;
typedef _Atomic(int_least8_t) atomic_int_least8_t;
typedef _Atomic(int_least16_t) atomic_int_least16_t;
typedef _Atomic(int_least32_t) atomic_int_least32_t;
typedef _Atomic(int_least64_t) atomic_int_least64_t;
typedef _Atomic(uint_fast8_t) atomic_uint_fast8_t;
typedef _Atomic(uint_fast16_t) atomic_uint_fast16_t;
typedef _Atomic(uint_fast32_t) atomic_uint_fast32_t;
typedef _Atomic(uint_fast64_t) atomic_uint_fast64_t;
typedef _Atomic(int_fast8_t) atomic_int_fast8_t;
typedef _Atomic(int_fast16_t) atomic_int_fast16_t;
typedef _Atomic(int_fast32_t) atomic_int_fast32_t;
typedef _Atomic(int_fast64_t) atomic_int_fast64_t;
typedef _Atomic(intptr_t) atomic_intptr_t;
typedef _Atomic(uintptr_t) atomic_uintptr_t;
typedef _Atomic(size_t) atomic_size_t;
typedef _Atomic(ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic(intmax_t) atomic_intmax_t;
typedef _Atomic(uintmax_t) atomic_uintmax_t;

typedef struct atomic_flag {
	atomic_bool __flag;
} atomic_flag;

#define ATOMIC_FLAG_INIT { 0 }
#define ATOMIC_VAR_INIT(value) (value)

#define atomic_init(obj, value) (*(obj) = (value))
#define atomic_store(obj, value) (*(obj) = (value))
#define atomic_store_explicit(obj, value, order) \
	((void)(order), (*(obj) = (value)))
#define atomic_load(obj) (*(obj))
#define atomic_load_explicit(obj, order) \
	((void)(order), (*(obj)))
#define atomic_exchange(obj, value) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) = (value); \
	   __tcc_atomic_old; })
#define atomic_exchange_explicit(obj, value, order) \
	((void)(order), atomic_exchange((obj), (value)))
#define atomic_compare_exchange_strong(obj, expected, desired) \
	((*(obj)) == *(expected) ? ((*(obj)) = (desired), 1) : ((*(expected)) = (*(obj)), 0))
#define atomic_compare_exchange_weak(obj, expected, desired) \
	atomic_compare_exchange_strong((obj), (expected), (desired))
#define atomic_compare_exchange_strong_explicit(obj, expected, desired, success, failure) \
	((void)(success), (void)(failure), \
	 atomic_compare_exchange_strong((obj), (expected), (desired)))
#define atomic_compare_exchange_weak_explicit(obj, expected, desired, success, failure) \
	((void)(success), (void)(failure), \
	 atomic_compare_exchange_weak((obj), (expected), (desired)))
#define atomic_fetch_add(obj, operand) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) += (operand); \
	   __tcc_atomic_old; })
#define atomic_fetch_sub(obj, operand) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) -= (operand); \
	   __tcc_atomic_old; })
#define atomic_fetch_or(obj, operand) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) |= (operand); \
	   __tcc_atomic_old; })
#define atomic_fetch_xor(obj, operand) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) ^= (operand); \
	   __tcc_atomic_old; })
#define atomic_fetch_and(obj, operand) \
	({ __typeof__(*(obj)) __tcc_atomic_old = (*(obj)); \
	   (*(obj)) &= (operand); \
	   __tcc_atomic_old; })
#define atomic_fetch_add_explicit(obj, operand, order) \
	((void)(order), atomic_fetch_add((obj), (operand)))
#define atomic_fetch_sub_explicit(obj, operand, order) \
	((void)(order), atomic_fetch_sub((obj), (operand)))
#define atomic_fetch_or_explicit(obj, operand, order) \
	((void)(order), atomic_fetch_or((obj), (operand)))
#define atomic_fetch_xor_explicit(obj, operand, order) \
	((void)(order), atomic_fetch_xor((obj), (operand)))
#define atomic_fetch_and_explicit(obj, operand, order) \
	((void)(order), atomic_fetch_and((obj), (operand)))

#define atomic_flag_test_and_set(obj) atomic_exchange(&(obj)->__flag, 1)
#define atomic_flag_test_and_set_explicit(obj, order) \
	((void)(order), atomic_flag_test_and_set((obj)))
#define atomic_flag_clear(obj) atomic_store(&(obj)->__flag, 0)
#define atomic_flag_clear_explicit(obj, order) \
	((void)(order), atomic_flag_clear((obj)))

#define atomic_thread_fence(order) ((void)(order))
#define atomic_signal_fence(order) ((void)(order))
#define kill_dependency(y) (y)
#define atomic_is_lock_free(obj) (1)

#endif

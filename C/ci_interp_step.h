//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

//
// Intended to be included multiple times
//

#ifndef CI_INTERP_HELPERS
#define CI_INTERP_HELPERS
#if defined _MSC_VER
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD8(p, d) do { *(uint8_t*)(d) = (uint8_t)__iso_volatile_load8((const volatile __int8*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE8(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store8((volatile __int8*)(p), *(const __int8*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD8(p, d) do { *(uint8_t*)(d) = *(volatile uint8_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE8(p, s) (void)_InterlockedExchange8((volatile char*)(p), *(const char*)(s))
#endif
#define CI_ATOMIC_XCHG8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchange8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_ADD8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchangeAdd8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_SUB8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchangeAdd8((volatile char*)(p), -*(const char*)(v)))
#define CI_ATOMIC_AND8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedAnd8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_OR8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedOr8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_XOR8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedXor8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_CAS8(p, e, d, weak, result) do { uint8_t exp = *(uint8_t*)(e); uint8_t old = (uint8_t)_InterlockedCompareExchange8((volatile char*)(p), *(const char*)(d), (char)exp); (result) = old == exp; *(uint8_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD16(p, d) do { *(uint16_t*)(d) = (uint16_t)__iso_volatile_load16((const volatile __int16*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE16(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store16((volatile __int16*)(p), *(const __int16*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD16(p, d) do { *(uint16_t*)(d) = *(volatile uint16_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE16(p, s) (void)_InterlockedExchange16((volatile short*)(p), *(const short*)(s))
#endif
#define CI_ATOMIC_XCHG16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchange16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_ADD16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchangeAdd16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_SUB16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchangeAdd16((volatile short*)(p), -*(const short*)(v)))
#define CI_ATOMIC_AND16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedAnd16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_OR16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedOr16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_XOR16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedXor16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_CAS16(p, e, d, weak, result) do { uint16_t exp = *(uint16_t*)(e); uint16_t old = (uint16_t)_InterlockedCompareExchange16((volatile short*)(p), *(const short*)(d), (short)exp); (result) = old == exp; *(uint16_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD32(p, d) do { *(uint32_t*)(d) = (uint32_t)__iso_volatile_load32((const volatile __int32*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE32(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store32((volatile __int32*)(p), *(const __int32*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD32(p, d) do { *(uint32_t*)(d) = *(volatile uint32_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE32(p, s) (void)_InterlockedExchange((volatile long*)(p), *(const long*)(s))
#endif
#define CI_ATOMIC_XCHG32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchange((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_ADD32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchangeAdd((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_SUB32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchangeAdd((volatile long*)(p), (long)(0u - *(const uint32_t*)(v))))
#define CI_ATOMIC_AND32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedAnd((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_OR32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedOr((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_XOR32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedXor((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_CAS32(p, e, d, weak, result) do { uint32_t exp = *(uint32_t*)(e); uint32_t old = (uint32_t)_InterlockedCompareExchange((volatile long*)(p), *(const long*)(d), (long)exp); (result) = old == exp; *(uint32_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD64(p, d) do { *(uint64_t*)(d) = (uint64_t)__iso_volatile_load64((const volatile __int64*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE64(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store64((volatile __int64*)(p), *(const __int64*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD64(p, d) do { *(uint64_t*)(d) = *(volatile uint64_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE64(p, s) (void)_InterlockedExchange64((volatile __int64*)(p), *(const __int64*)(s))
#endif
#define CI_ATOMIC_XCHG64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchange64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_ADD64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchangeAdd64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_SUB64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchangeAdd64((volatile __int64*)(p), (__int64)(UINT64_C(0) - *(const uint64_t*)(v))))
#define CI_ATOMIC_AND64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedAnd64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_OR64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedOr64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_XOR64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedXor64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_CAS64(p, e, d, weak, result) do { uint64_t exp = *(uint64_t*)(e); uint64_t old = (uint64_t)_InterlockedCompareExchange64((volatile __int64*)(p), *(const __int64*)(d), (__int64)exp); (result) = old == exp; *(uint64_t*)(e) = old; } while(0)
#define CI_ATOMIC_LOAD128(p, d) do { __int64* out = (d); out[0] = 0; out[1] = 0; _InterlockedCompareExchange128((volatile __int64*)(p), 0, 0, out); } while(0)
#define CI_ATOMIC_XCHG128(p, v, o) do { __int64 ci_atomic_desired[2], previous[2] = {0}; memcpy(ci_atomic_desired, (v), 16); while(!_InterlockedCompareExchange128((volatile __int64*)(p), ci_atomic_desired[1], ci_atomic_desired[0], previous)){} memcpy((o), previous, 16); } while(0)
#define CI_ATOMIC_STORE128(p, v) do { __int64 ignored[2]; CI_ATOMIC_XCHG128(p, v, ignored); } while(0)
#define CI_ATOMIC_CAS128(p, e, d, weak, result) do { __int64 ci_atomic_desired[2]; memcpy(ci_atomic_desired, (d), 16); (result) = (_Bool)_InterlockedCompareExchange128((volatile __int64*)(p), ci_atomic_desired[1], ci_atomic_desired[0], (__int64*)(e)); } while(0)
#define CI_THREAD_FENCE() MemoryBarrier()
#else
#define CI_ATOMIC_LOAD8(p, d) __atomic_load((uint8_t*)(p), (uint8_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE8(p, s) __atomic_store((uint8_t*)(p), (uint8_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG8(p, v, o) __atomic_exchange((uint8_t*)(p), (uint8_t*)(v), (uint8_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS8(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint8_t*)(p), (uint8_t*)(e), (uint8_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_add((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_sub((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_and((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_or((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_xor((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD16(p, d) __atomic_load((uint16_t*)(p), (uint16_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE16(p, s) __atomic_store((uint16_t*)(p), (uint16_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG16(p, v, o) __atomic_exchange((uint16_t*)(p), (uint16_t*)(v), (uint16_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS16(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint16_t*)(p), (uint16_t*)(e), (uint16_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_add((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_sub((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_and((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_or((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_xor((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD32(p, d) __atomic_load((uint32_t*)(p), (uint32_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE32(p, s) __atomic_store((uint32_t*)(p), (uint32_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG32(p, v, o) __atomic_exchange((uint32_t*)(p), (uint32_t*)(v), (uint32_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS32(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint32_t*)(p), (uint32_t*)(e), (uint32_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_add((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_sub((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_and((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_or((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_xor((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD64(p, d) __atomic_load((uint64_t*)(p), (uint64_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE64(p, s) __atomic_store((uint64_t*)(p), (uint64_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG64(p, v, o) __atomic_exchange((uint64_t*)(p), (uint64_t*)(v), (uint64_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS64(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint64_t*)(p), (uint64_t*)(e), (uint64_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_add((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_sub((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_and((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_or((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_xor((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD128(p, d) __atomic_load((CiAtomic16*)(p), (CiAtomic16*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE128(p, s) __atomic_store((CiAtomic16*)(p), (CiAtomic16*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG128(p, v, o) __atomic_exchange((CiAtomic16*)(p), (CiAtomic16*)(v), (CiAtomic16*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS128(p, e, d, weak, result) ((result) = __atomic_compare_exchange((CiAtomic16*)(p), (CiAtomic16*)(e), (CiAtomic16*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_THREAD_FENCE() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif



#if (defined __clang__ || defined __GNUC__) && !defined __DRC__
#define CI_ALIGNED(p, a) __builtin_assume_aligned((p), (a))
#else
#define CI_ALIGNED(p, a) (p)
#endif
#endif

#ifndef CI_OP_BEGIN
#error CI_OP_BEGIN not defined
#endif

#ifndef CI_OP_END
#error CI_OP_END not defined
#endif

#ifndef CI_OP_ENTER
#error CI_OP_ENTER not defined
#endif

#ifndef CI_OP_ERROR
#error CI_OP_ERROR not defined
#endif

#ifndef CI_OP_SYNC_PC
#error CI_OP_SYNC_PC not defined
#endif

{
    CI_OP_BEGIN(CI_OP_CONST8);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, 1);
    pc++;
    CI_OP_END(CI_OP_CONST8);
}
{
    CI_OP_BEGIN(CI_OP_CONST16);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, 2);
    pc++;
    CI_OP_END(CI_OP_CONST16);
}
{
    CI_OP_BEGIN(CI_OP_CONST32);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, 4);
    pc++;
    CI_OP_END(CI_OP_CONST32);
}
{
    CI_OP_BEGIN(CI_OP_CONST64);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, 8);
    pc++;
    CI_OP_END(CI_OP_CONST64);
}
{
    CI_OP_BEGIN(CI_OP_CONST128);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, 16);
    pc++;
    CI_OP_END(CI_OP_CONST128);
}
{
    CI_OP_BEGIN(CI_OP_CONST_N);
    memcpy((char*)(frame + 1) + op->constant.slot, op->constant.immediate, op->constant.immsize);
    pc++;
    CI_OP_END(CI_OP_CONST_N);
}
{
    CI_OP_BEGIN(CI_OP_LOAD8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), 1);
    pc++;
    CI_OP_END(CI_OP_LOAD8);
}
{
    CI_OP_BEGIN(CI_OP_LOAD16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 2), CI_ALIGNED(ptr + op->load.offset, 2), 2);
    pc++;
    CI_OP_END(CI_OP_LOAD16_A);
}
{
    CI_OP_BEGIN(CI_OP_LOAD16_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), 2);
    pc++;
    CI_OP_END(CI_OP_LOAD16_U);
}
{
    CI_OP_BEGIN(CI_OP_LOAD32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 4), CI_ALIGNED(ptr + op->load.offset, 4), 4);
    pc++;
    CI_OP_END(CI_OP_LOAD32_A);
}
{
    CI_OP_BEGIN(CI_OP_LOAD32_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), 4);
    pc++;
    CI_OP_END(CI_OP_LOAD32_U);
}
{
    CI_OP_BEGIN(CI_OP_LOAD64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 8), CI_ALIGNED(ptr + op->load.offset, 8), 8);
    pc++;
    CI_OP_END(CI_OP_LOAD64_A);
}
{
    CI_OP_BEGIN(CI_OP_LOAD64_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), 8);
    pc++;
    CI_OP_END(CI_OP_LOAD64_U);
}
{
    CI_OP_BEGIN(CI_OP_LOAD128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 16), CI_ALIGNED(ptr + op->load.offset, 16), 16);
    pc++;
    CI_OP_END(CI_OP_LOAD128_A);
}
{
    CI_OP_BEGIN(CI_OP_LOAD128_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), 16);
    pc++;
    CI_OP_END(CI_OP_LOAD128_U);
}
{
    CI_OP_BEGIN(CI_OP_LOAD_N);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 1), CI_ALIGNED(ptr + op->load.offset, 1), op->load.slot_size);
    pc++;
    CI_OP_END(CI_OP_LOAD_N);
}
{
    CI_OP_BEGIN(CI_OP_LOAD_N_A2);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 2), CI_ALIGNED(ptr + op->load.offset, 2), op->load.slot_size);
    pc++;
    CI_OP_END(CI_OP_LOAD_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_LOAD_N_A4);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 4), CI_ALIGNED(ptr + op->load.offset, 4), op->load.slot_size);
    pc++;
    CI_OP_END(CI_OP_LOAD_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_LOAD_N_A8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 8), CI_ALIGNED(ptr + op->load.offset, 8), op->load.slot_size);
    pc++;
    CI_OP_END(CI_OP_LOAD_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_LOAD_N_A16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load.src, sizeof ptr);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->load.slot, 16), CI_ALIGNED(ptr + op->load.offset, 16), op->load.slot_size);
    pc++;
    CI_OP_END(CI_OP_LOAD_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_STORE8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), 1);
    pc++;
    CI_OP_END(CI_OP_STORE8);
}
{
    CI_OP_BEGIN(CI_OP_STORE16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 2), CI_ALIGNED((char*)(frame + 1) + op->store.src, 2), 2);
    pc++;
    CI_OP_END(CI_OP_STORE16_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE16_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), 2);
    pc++;
    CI_OP_END(CI_OP_STORE16_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 4), CI_ALIGNED((char*)(frame + 1) + op->store.src, 4), 4);
    pc++;
    CI_OP_END(CI_OP_STORE32_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE32_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), 4);
    pc++;
    CI_OP_END(CI_OP_STORE32_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 8), CI_ALIGNED((char*)(frame + 1) + op->store.src, 8), 8);
    pc++;
    CI_OP_END(CI_OP_STORE64_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE64_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), 8);
    pc++;
    CI_OP_END(CI_OP_STORE64_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 16), CI_ALIGNED((char*)(frame + 1) + op->store.src, 16), 16);
    pc++;
    CI_OP_END(CI_OP_STORE128_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE128_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), 16);
    pc++;
    CI_OP_END(CI_OP_STORE128_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE_N);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 1), CI_ALIGNED((char*)(frame + 1) + op->store.src, 1), op->store.src_size);
    pc++;
    CI_OP_END(CI_OP_STORE_N);
}
{
    CI_OP_BEGIN(CI_OP_STORE_N_A2);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 2), CI_ALIGNED((char*)(frame + 1) + op->store.src, 2), op->store.src_size);
    pc++;
    CI_OP_END(CI_OP_STORE_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_STORE_N_A4);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 4), CI_ALIGNED((char*)(frame + 1) + op->store.src, 4), op->store.src_size);
    pc++;
    CI_OP_END(CI_OP_STORE_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_STORE_N_A8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 8), CI_ALIGNED((char*)(frame + 1) + op->store.src, 8), op->store.src_size);
    pc++;
    CI_OP_END(CI_OP_STORE_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_STORE_N_A16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store.slot, sizeof ptr);
    memmove(CI_ALIGNED(ptr + op->store.offset, 16), CI_ALIGNED((char*)(frame + 1) + op->store.src, 16), op->store.src_size);
    pc++;
    CI_OP_END(CI_OP_STORE_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 1), &op->store_imm.immediate, 1);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM8);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 2), &op->store_imm.immediate, 2);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM16_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM16_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 1), &op->store_imm.immediate, 2);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM16_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 4), &op->store_imm.immediate, 4);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM32_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM32_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 1), &op->store_imm.immediate, 4);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM32_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 8), &op->store_imm.immediate, 8);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM64_A);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM64_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 1), &op->store_imm.immediate, 8);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM64_U);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM_N);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 1), &op->store_imm.immediate, op->store_imm.size);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM_N);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM_N_A2);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 2), &op->store_imm.immediate, op->store_imm.size);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM_N_A4);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 4), &op->store_imm.immediate, op->store_imm.size);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM_N_A8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 8), &op->store_imm.immediate, op->store_imm.size);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_STORE_IMM_N_A16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_imm.slot, sizeof ptr);
    memcpy(CI_ALIGNED(ptr + op->store_imm.offset, 16), &op->store_imm.immediate, op->store_imm.size);
    pc++;
    CI_OP_END(CI_OP_STORE_IMM_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ZERO8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, 1);
    pc++;
    CI_OP_END(CI_OP_ZERO8);
}
{
    CI_OP_BEGIN(CI_OP_ZERO16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 2), 0, 2);
    pc++;
    CI_OP_END(CI_OP_ZERO16_A);
}
{
    CI_OP_BEGIN(CI_OP_ZERO16_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, 2);
    pc++;
    CI_OP_END(CI_OP_ZERO16_U);
}
{
    CI_OP_BEGIN(CI_OP_ZERO32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 4), 0, 4);
    pc++;
    CI_OP_END(CI_OP_ZERO32_A);
}
{
    CI_OP_BEGIN(CI_OP_ZERO32_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, 4);
    pc++;
    CI_OP_END(CI_OP_ZERO32_U);
}
{
    CI_OP_BEGIN(CI_OP_ZERO64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 8), 0, 8);
    pc++;
    CI_OP_END(CI_OP_ZERO64_A);
}
{
    CI_OP_BEGIN(CI_OP_ZERO64_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, 8);
    pc++;
    CI_OP_END(CI_OP_ZERO64_U);
}
{
    CI_OP_BEGIN(CI_OP_ZERO128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 16), 0, 16);
    pc++;
    CI_OP_END(CI_OP_ZERO128_A);
}
{
    CI_OP_BEGIN(CI_OP_ZERO128_U);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, 16);
    pc++;
    CI_OP_END(CI_OP_ZERO128_U);
}
{
    CI_OP_BEGIN(CI_OP_ZERO_N);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 1), 0, op->zero.size);
    pc++;
    CI_OP_END(CI_OP_ZERO_N);
}
{
    CI_OP_BEGIN(CI_OP_ZERO_N_A2);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 2), 0, op->zero.size);
    pc++;
    CI_OP_END(CI_OP_ZERO_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ZERO_N_A4);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 4), 0, op->zero.size);
    pc++;
    CI_OP_END(CI_OP_ZERO_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ZERO_N_A8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 8), 0, op->zero.size);
    pc++;
    CI_OP_END(CI_OP_ZERO_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ZERO_N_A16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->zero.slot, sizeof ptr);
    memset(CI_ALIGNED(ptr + op->zero.offset, 16), 0, op->zero.size);
    pc++;
    CI_OP_END(CI_OP_ZERO_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_COPY8);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), 1);
    pc++;
    CI_OP_END(CI_OP_COPY8);
}
{
    CI_OP_BEGIN(CI_OP_COPY16_A);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 2), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 2), 2);
    pc++;
    CI_OP_END(CI_OP_COPY16_A);
}
{
    CI_OP_BEGIN(CI_OP_COPY16_U);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), 2);
    pc++;
    CI_OP_END(CI_OP_COPY16_U);
}
{
    CI_OP_BEGIN(CI_OP_COPY32_A);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 4), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 4), 4);
    pc++;
    CI_OP_END(CI_OP_COPY32_A);
}
{
    CI_OP_BEGIN(CI_OP_COPY32_U);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), 4);
    pc++;
    CI_OP_END(CI_OP_COPY32_U);
}
{
    CI_OP_BEGIN(CI_OP_COPY64_A);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 8), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 8), 8);
    pc++;
    CI_OP_END(CI_OP_COPY64_A);
}
{
    CI_OP_BEGIN(CI_OP_COPY64_U);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), 8);
    pc++;
    CI_OP_END(CI_OP_COPY64_U);
}
{
    CI_OP_BEGIN(CI_OP_COPY128_A);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 16), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 16), 16);
    pc++;
    CI_OP_END(CI_OP_COPY128_A);
}
{
    CI_OP_BEGIN(CI_OP_COPY128_U);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), 16);
    pc++;
    CI_OP_END(CI_OP_COPY128_U);
}
{
    CI_OP_BEGIN(CI_OP_COPY_N);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 1), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 1), op->copy.slot_size);
    pc++;
    CI_OP_END(CI_OP_COPY_N);
}
{
    CI_OP_BEGIN(CI_OP_COPY_N_A2);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 2), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 2), op->copy.slot_size);
    pc++;
    CI_OP_END(CI_OP_COPY_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_COPY_N_A4);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 4), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 4), op->copy.slot_size);
    pc++;
    CI_OP_END(CI_OP_COPY_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_COPY_N_A8);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 8), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 8), op->copy.slot_size);
    pc++;
    CI_OP_END(CI_OP_COPY_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_COPY_N_A16);
    memmove(CI_ALIGNED((char*)(frame + 1) + op->copy.slot, 16), CI_ALIGNED((char*)(frame + 1) + op->copy.src, 16), op->copy.slot_size);
    pc++;
    CI_OP_END(CI_OP_COPY_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY8);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), 1);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY8);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY16_A);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 2), CI_ALIGNED(src + op->memcopy.src_offset, 2), 2);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY16_A);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY16_U);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), 2);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY16_U);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY32_A);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 4), CI_ALIGNED(src + op->memcopy.src_offset, 4), 4);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY32_A);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY32_U);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), 4);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY32_U);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY64_A);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 8), CI_ALIGNED(src + op->memcopy.src_offset, 8), 8);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY64_A);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY64_U);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), 8);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY64_U);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY128_A);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 16), CI_ALIGNED(src + op->memcopy.src_offset, 16), 16);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY128_A);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY128_U);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), 16);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY128_U);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_N);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), op->memcopy.size);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_N);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_N_A2);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 2), CI_ALIGNED(src + op->memcopy.src_offset, 2), op->memcopy.size);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_N_A4);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 4), CI_ALIGNED(src + op->memcopy.src_offset, 4), op->memcopy.size);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_N_A8);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 8), CI_ALIGNED(src + op->memcopy.src_offset, 8), op->memcopy.size);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_N_A16);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 16), CI_ALIGNED(src + op->memcopy.src_offset, 16), op->memcopy.size);
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_VAR);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 1), CI_ALIGNED(src + op->memcopy.src_offset, 1), ci_read_uint((char*)(frame + 1) + op->memcopy.size, sizeof(size_t)));
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_VAR);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_VAR_A2);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 2), CI_ALIGNED(src + op->memcopy.src_offset, 2), ci_read_uint((char*)(frame + 1) + op->memcopy.size, sizeof(size_t)));
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_VAR_A2);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_VAR_A4);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 4), CI_ALIGNED(src + op->memcopy.src_offset, 4), ci_read_uint((char*)(frame + 1) + op->memcopy.size, sizeof(size_t)));
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_VAR_A4);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_VAR_A8);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 8), CI_ALIGNED(src + op->memcopy.src_offset, 8), ci_read_uint((char*)(frame + 1) + op->memcopy.size, sizeof(size_t)));
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_VAR_A8);
}
{
    CI_OP_BEGIN(CI_OP_MEMCOPY_VAR_A16);
    char *dst, *src;
    memcpy(&dst, (char*)(frame + 1) + op->memcopy.slot, sizeof dst);
    memcpy(&src, (char*)(frame + 1) + op->memcopy.src, sizeof src);
    memmove(CI_ALIGNED(dst + op->memcopy.offset, 16), CI_ALIGNED(src + op->memcopy.src_offset, 16), ci_read_uint((char*)(frame + 1) + op->memcopy.size, sizeof(size_t)));
    pc++;
    CI_OP_END(CI_OP_MEMCOPY_VAR_A16);
}
{
    CI_OP_BEGIN(CI_OP_SEXT8_16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_SEXT8_16);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT8_16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT8_16);
}
{
    CI_OP_BEGIN(CI_OP_SEXT8_32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SEXT8_32);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT8_32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT8_32);
}
{
    CI_OP_BEGIN(CI_OP_SEXT8_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SEXT8_64);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT8_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT8_64);
}
{
    CI_OP_BEGIN(CI_OP_SEXT8_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SEXT8_128);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT8_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT8_128);
}
{
    CI_OP_BEGIN(CI_OP_SEXT16_32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SEXT16_32);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT16_32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT16_32);
}
{
    CI_OP_BEGIN(CI_OP_SEXT16_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SEXT16_64);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT16_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT16_64);
}
{
    CI_OP_BEGIN(CI_OP_SEXT16_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SEXT16_128);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT16_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 2));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT16_128);
}
{
    CI_OP_BEGIN(CI_OP_SEXT32_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 4));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SEXT32_64);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT32_64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 4));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT32_64);
}
{
    CI_OP_BEGIN(CI_OP_SEXT32_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 4));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SEXT32_128);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT32_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 4));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT32_128);
}
{
    CI_OP_BEGIN(CI_OP_SEXT64_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_int64(ci_read_int(src, 8));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SEXT64_128);
}
{
    CI_OP_BEGIN(CI_OP_ZEXT64_128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_read_uint(src, 8));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ZEXT64_128);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_S8);
    int8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = add_overflow((int32_t)a, (int32_t)b, &wide);
    int8_t result = (int8_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_S8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_U8);
    uint8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = add_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint8_t result = (uint8_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_U8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_S16);
    int16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = add_overflow((int32_t)a, (int32_t)b, &wide);
    int16_t result = (int16_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_S16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_U16);
    uint16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = add_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint16_t result = (uint16_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_U16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_S32);
    int32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = add_overflow((int32_t)a, (int32_t)b, &wide);
    int32_t result = (int32_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_S32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_U32);
    uint32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = add_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint32_t result = (uint32_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_U32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_S64);
    int64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int64_t wide;
    _Bool overflow = add_overflow((int64_t)a, (int64_t)b, &wide);
    int64_t result = (int64_t)wide;
    overflow |= wide != (int64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_S64);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_ADD_U64);
    uint64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint64_t wide;
    _Bool overflow = add_overflow((uint64_t)a, (uint64_t)b, &wide);
    uint64_t result = (uint64_t)wide;
    overflow |= wide != (uint64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_ADD_U64);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_S8);
    int8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = sub_overflow((int32_t)a, (int32_t)b, &wide);
    int8_t result = (int8_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_S8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_U8);
    uint8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = sub_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint8_t result = (uint8_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_U8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_S16);
    int16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = sub_overflow((int32_t)a, (int32_t)b, &wide);
    int16_t result = (int16_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_S16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_U16);
    uint16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = sub_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint16_t result = (uint16_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_U16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_S32);
    int32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = sub_overflow((int32_t)a, (int32_t)b, &wide);
    int32_t result = (int32_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_S32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_U32);
    uint32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = sub_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint32_t result = (uint32_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_U32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_S64);
    int64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int64_t wide;
    _Bool overflow = sub_overflow((int64_t)a, (int64_t)b, &wide);
    int64_t result = (int64_t)wide;
    overflow |= wide != (int64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_S64);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_SUB_U64);
    uint64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint64_t wide;
    _Bool overflow = sub_overflow((uint64_t)a, (uint64_t)b, &wide);
    uint64_t result = (uint64_t)wide;
    overflow |= wide != (uint64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_SUB_U64);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_S8);
    int8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = mul_overflow((int32_t)a, (int32_t)b, &wide);
    int8_t result = (int8_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_S8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_U8);
    uint8_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = mul_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint8_t result = (uint8_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_U8);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_S16);
    int16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = mul_overflow((int32_t)a, (int32_t)b, &wide);
    int16_t result = (int16_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_S16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_U16);
    uint16_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = mul_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint16_t result = (uint16_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_U16);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_S32);
    int32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int32_t wide;
    _Bool overflow = mul_overflow((int32_t)a, (int32_t)b, &wide);
    int32_t result = (int32_t)wide;
    overflow |= wide != (int32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_S32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_U32);
    uint32_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint32_t wide;
    _Bool overflow = mul_overflow((uint32_t)a, (uint32_t)b, &wide);
    uint32_t result = (uint32_t)wide;
    overflow |= wide != (uint32_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_U32);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_S64);
    int64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    int64_t wide;
    _Bool overflow = mul_overflow((int64_t)a, (int64_t)b, &wide);
    int64_t result = (int64_t)wide;
    overflow |= wide != (int64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_S64);
}
{
    CI_OP_BEGIN(CI_OP_CHECKED_MUL_U64);
    uint64_t a, b;
    memcpy(&a, (char*)(frame + 1) + op->checked.src, sizeof a);
    memcpy(&b, (char*)(frame + 1) + op->checked.src2, sizeof b);
    uint64_t wide;
    _Bool overflow = mul_overflow((uint64_t)a, (uint64_t)b, &wide);
    uint64_t result = (uint64_t)wide;
    overflow |= wide != (uint64_t)result;
    memcpy((char*)(frame + 1) + op->checked.result, &result, sizeof result);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = overflow;
    pc++;
    CI_OP_END(CI_OP_CHECKED_MUL_U64);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_S8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 8, 0);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_S8);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_U8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 8, 1);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_U8);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_S16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 16, 0);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_S16);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_U16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 16, 1);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_U16);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_S32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 32, 0);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_S32);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_U32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 32, 1);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_U32);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_S64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 64, 0);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_S64);
}
{
    CI_OP_BEGIN(CI_OP_OVERFLOW_U64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->checked.src, 16);
    *((unsigned char*)(frame + 1) + op->checked.overflow) = !ci_uint128_fits_integer(value, 64, 1);
    pc++;
    CI_OP_END(CI_OP_OVERFLOW_U64);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_I8);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_uint(src, 1) != 0);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_I8);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_I8);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_uint(src, 1) != 0));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_I8);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_I16);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_uint(src, 2) != 0);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_I16);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_I16);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_uint(src, 2) != 0));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_I16);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_I32);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_uint(src, 4) != 0);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_I32);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_I32);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_uint(src, 4) != 0));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_I32);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_I64);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_uint(src, 8) != 0);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_I64);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_I64);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_uint(src, 8) != 0));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_I64);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_I128);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_uint128_nonzero(ci_read_u128(src)));
    pc++;
    CI_OP_END(CI_OP_ISTRUE_I128);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_I128);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_uint128_nonzero(ci_read_u128(src))));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_I128);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_F16);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_narrow_float_nonzero((uint16_t)ci_read_uint(src, 2)));
    pc++;
    CI_OP_END(CI_OP_ISTRUE_F16);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_F16);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_narrow_float_nonzero((uint16_t)ci_read_uint(src, 2))));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_F16);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_F32);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_f32(src) != 0.0f);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_F32);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_F32);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_f32(src) != 0.0f));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_F32);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_F64);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_read_f64(src) != 0.0);
    pc++;
    CI_OP_END(CI_OP_ISTRUE_F64);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_F64);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_read_f64(src) != 0.0));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_F64);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_F80);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_float80_nonzero(ci_float80_read(src)));
    pc++;
    CI_OP_END(CI_OP_ISTRUE_F80);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_F80);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_float80_nonzero(ci_float80_read(src))));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_F80);
}
{
    CI_OP_BEGIN(CI_OP_ISTRUE_F128);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (ci_float128_nonzero(ci_float128_read(src)));
    pc++;
    CI_OP_END(CI_OP_ISTRUE_F128);
}
{
    CI_OP_BEGIN(CI_OP_ISFALSE_F128);
    const void* src = (char*)(frame + 1) + op->istrue.src;
    *((unsigned char*)(frame + 1) + op->istrue.slot) = (!(ci_float128_nonzero(ci_float128_read(src))));
    pc++;
    CI_OP_END(CI_OP_ISFALSE_F128);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_int32((int32_t)ci_read_int(src, 4), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S32_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint32((uint32_t)ci_read_uint(src, 4), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U32_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_int64((int64_t)ci_read_int(src, 8), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S64_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint64((uint64_t)ci_read_uint(src, 8), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U64_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint128(ci_read_u128(src), 0, 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S128_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint128(ci_read_u128(src), 1, 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U128_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_int32((int32_t)ci_read_int(src, 4), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S32_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint32((uint32_t)ci_read_uint(src, 4), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U32_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_int64((int64_t)ci_read_int(src, 8), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S64_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint64((uint64_t)ci_read_uint(src, 8), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U64_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint128(ci_read_u128(src), 0, 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_S128_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_uint128(ci_read_u128(src), 1, 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_U128_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = (float)ci_read_int(src, 4);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_S32_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = (float)ci_read_uint(src, 4);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_U32_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = (float)ci_read_int(src, 8);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_S64_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = (float)ci_read_uint(src, 8);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_U64_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_uint128_to_float(ci_read_u128(src), 0);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_S128_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_uint128_to_float(ci_read_u128(src), 1);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_U128_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = (double)ci_read_int(src, 4);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_S32_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = (double)ci_read_uint(src, 4);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_U32_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = (double)ci_read_int(src, 8);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_S64_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = (double)ci_read_uint(src, 8);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_U64_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_uint128_to_double(ci_read_u128(src), 0);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_S128_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_uint128_to_double(ci_read_u128(src), 1);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_U128_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_uint128_from_int64(ci_read_int(src, 4)), 0);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_S32_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 4)), 1);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_U32_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_uint128_from_int64(ci_read_int(src, 8)), 0);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_S64_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 8)), 1);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_U64_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_read_u128(src), 0);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_S128_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_uint128(ci_read_u128(src), 1);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_U128_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_S32_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_uint128_from_int64(ci_read_int(src, 4)), 0);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_S32_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_U32_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 4)), 1);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_U32_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_S64_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_uint128_from_int64(ci_read_int(src, 8)), 0);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_S64_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_U64_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 8)), 1);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_U64_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_S128_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_read_u128(src), 0);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_S128_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_U128_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_uint128(ci_read_u128(src), 1);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_U128_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_S32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_int32((uint16_t)ci_read_uint(src, 2), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_S32);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_U32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_uint32((uint16_t)ci_read_uint(src, 2), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_U32);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_S64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_int64((uint16_t)ci_read_uint(src, 2), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_S64);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_U64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_uint64((uint16_t)ci_read_uint(src, 2), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_U64);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_I128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float16_to_uint128((uint16_t)ci_read_uint(src, 2), 0);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_I128);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_S32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_int32((uint16_t)ci_read_uint(src, 2), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_S32);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_U32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_uint32((uint16_t)ci_read_uint(src, 2), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_U32);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_S64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_int64((uint16_t)ci_read_uint(src, 2), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_S64);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_U64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64((uint64_t)ci_float16_to_uint64((uint16_t)ci_read_uint(src, 2), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_U64);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_I128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float16_to_uint128((uint16_t)ci_read_uint(src, 2), 1);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_I128);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_S32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_S32);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_U32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_U32);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_S64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_S64);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_U64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_U64);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_S128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_double((double)ci_read_f32(src), 0);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_S128);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_U128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_double((double)ci_read_f32(src), 1);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_U128);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_S32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_S32);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_U32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_U32);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_S64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 0));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_S64);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_U64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 1));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_U64);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_S128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_double(ci_read_f64(src), 0);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_S128);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_U128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_uint128_from_double(ci_read_f64(src), 1);
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_U128);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_I8);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float80_to_uint128(ci_float80_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_I8);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_I16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float80_to_uint128(ci_float80_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_I16);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_I32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float80_to_uint128(ci_float80_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_I32);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_I64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float80_to_uint128(ci_float80_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_I64);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_I128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float80_to_uint128(ci_float80_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_I128);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_I8);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float128_to_uint128(ci_float128_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_I8);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_I16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float128_to_uint128(ci_float128_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_I16);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_I32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float128_to_uint128(ci_float128_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_I32);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_I64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float128_to_uint128(ci_float128_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_I64);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_I128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    CiUint128 result = ci_float128_to_uint128(ci_float128_read(src));
    ci_uint128_write((char*)(frame + 1) + op->convert.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_I128);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float(ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 0), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F16_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 0);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_F16_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_float16_to_double((uint16_t)ci_read_uint(src, 2), 0);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_F16_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float16_to_float80((uint16_t)ci_read_uint(src, 2), 0);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_F16_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float16_to_quad((uint16_t)ci_read_uint(src, 2), 0);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_F16_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float(ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 1), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 1);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_float16_to_double((uint16_t)ci_read_uint(src, 2), 1);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float16_to_float80((uint16_t)ci_read_uint(src, 2), 1);
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_BF16_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float16_to_quad((uint16_t)ci_read_uint(src, 2), 1);
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_BF16_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float(ci_read_f32(src), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F32_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float(ci_read_f32(src), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F32_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = (double)ci_read_f32(src);
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_F32_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_float(ci_read_f32(src));
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_F32_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_float(ci_read_f32(src));
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_F32_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_double(ci_read_f64(src), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F64_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_double(ci_read_f64(src), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F64_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = (float)ci_read_f64(src);
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_F64_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_double(ci_read_f64(src));
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_F64_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_double(ci_read_f64(src));
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_F64_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float80(ci_float80_read(src), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F80_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_float80(ci_float80_read(src), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F80_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_float80_to_float(ci_float80_read(src));
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_F80_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_float80_to_double(ci_float80_read(src));
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_F80_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_F80_TO_F128);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat128 result = ci_float128_from_float80(ci_float80_read(src));
    ci_float128_write(dest, 16, result);
    pc++;
    CI_OP_END(CI_OP_F80_TO_F128);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_F16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_quad(ci_float128_read(src), 0);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F128_TO_F16);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_BF16);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    uint16_t result = ci_float16_from_quad(ci_float128_read(src), 1);
    memcpy(dest, &result, 2);
    pc++;
    CI_OP_END(CI_OP_F128_TO_BF16);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_F32);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    float result = ci_float128_to_float(ci_float128_read(src));
    memcpy(dest, &result, 4);
    pc++;
    CI_OP_END(CI_OP_F128_TO_F32);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_F64);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    double result = ci_float128_to_double(ci_float128_read(src));
    memcpy(dest, &result, 8);
    pc++;
    CI_OP_END(CI_OP_F128_TO_F64);
}
{
    CI_OP_BEGIN(CI_OP_F128_TO_F80);
    const void* src = (char*)(frame + 1) + op->convert.src;
    void* dest = (char*)(frame + 1) + op->convert.slot;
    CiFloat80 result = ci_float80_from_float128(ci_float128_read(src));
    ci_float80_write(dest, op->convert.slot_size, result);
    pc++;
    CI_OP_END(CI_OP_F128_TO_F80);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP01_64);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 1);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 1);
    memcpy(slot0, reg0, 8);
    memcpy(slot1, reg1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP01_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP01_64_A);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 8);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 8);
    memcpy(slot0, reg0, 8);
    memcpy(slot1, reg1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP01_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP01_64_CONTIG);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 1);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 1);
    memcpy(slot0, reg0, 8);
    memcpy(slot1, reg1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP01_64_CONTIG);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP01_64_CONTIG_A);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 8);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 8);
    memcpy(slot0, reg0, 8);
    memcpy(slot1, reg1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP01_64_CONTIG_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP01_64);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 1);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 1);
    memcpy(reg0, slot0, 8);
    memcpy(reg1, slot1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP01_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP01_64_A);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 8);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 8);
    memcpy(reg0, slot0, 8);
    memcpy(reg1, slot1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP01_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP01_64_CONTIG);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 1);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 1);
    memcpy(reg0, slot0, 8);
    memcpy(reg1, slot1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP01_64_CONTIG);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP01_64_CONTIG_A);
    void* reg0 = (char*)(&stack->registers) + op->abi_pair.reg0.capture_offset;
    void* reg1 = (char*)(&stack->registers) + op->abi_pair.reg1.capture_offset;
    void* slot0 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot0, 8);
    void* slot1 = CI_ALIGNED((char*)(frame + 1) + op->abi_pair.slot1, 8);
    memcpy(reg0, slot0, 8);
    memcpy(reg1, slot1, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP01_64_CONTIG_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP0_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP0_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP0_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP0_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP0_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP0_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP0_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP0_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP1_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP1_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP1_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP1_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP1_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP1_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP1_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP1_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP2_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP2_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP2_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP2_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP2_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP2_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP2_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP2_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP3_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP3_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP3_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP3_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP3_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP3_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP3_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP3_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP4_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP4_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP4_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP4_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP4_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP4_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP4_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP4_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP5_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP5_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP5_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP5_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP5_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP5_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP5_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP5_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP6_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP6_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP6_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP6_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP6_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP6_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP6_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP6_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP7_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP7_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP7_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP7_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP7_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP7_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP7_8);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP7_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_SEXT_GP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_SEXT_GP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZEXT_GP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZEXT_GP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP0_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP0_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP0_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP0_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP1_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP1_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP1_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP1_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP2_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP2_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP2_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP2_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP3_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP3_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP3_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP3_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP4_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP4_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP4_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP4_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP5_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP5_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP5_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP5_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP6_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP6_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP6_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP6_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_32);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_32_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_FP7_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_FP7_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_128);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_FP7_128_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_FP7_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP0_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP0_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP0_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP0_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP0_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP0_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP0_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP0_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP1_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP1_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP1_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP1_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP1_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP1_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP1_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP1_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP2_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP2_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP2_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP2_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP2_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP2_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP2_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP2_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP3_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP3_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP3_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP3_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP3_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP3_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP3_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP3_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP4_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP4_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP4_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP4_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP4_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP4_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP4_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP4_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP5_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP5_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP5_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP5_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP5_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP5_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP5_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP5_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP6_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP6_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP6_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP6_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP6_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP6_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP6_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP6_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP7_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP7_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_GP7_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_GP7_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP7_64);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP7_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_GP7_64_A);
    void* reg = (char*)(&stack->registers) + op->abi_move.reg.capture_offset;
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_GP7_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN8);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN16);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN16_A);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(slot, reg, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN32);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN32_A);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN64);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN64_A);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN128);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN128_A);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN_N);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN_N_A2);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN_N_A4);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN_N_A8);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_IN_N_A16);
    void* reg = CI_ALIGNED((char*)(frame->incoming) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_IN_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT16_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(slot, reg, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT32);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT32_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT64);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT64_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT128);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT128_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT_N);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT_N_A2);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT_N_A4);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT_N_A8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_READ_OUT_N_A16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(slot, reg, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_READ_OUT_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT16_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(reg, slot, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT32);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT32_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT64);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT64_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT128);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT128_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT_N);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    memcpy(reg, slot, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT_N_A2);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    memcpy(reg, slot, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT_N_A4);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    memcpy(reg, slot, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT_N_A8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 8);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 8);
    memcpy(reg, slot, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_OUT_N_A16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 16);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 16);
    memcpy(reg, slot, op->abi_move.size);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_OUT_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_SEXT8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_SEXT8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_SEXT16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 2);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_SEXT16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_SEXT16_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    uint64_t value = (uint64_t)ci_read_int(slot, 2);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_SEXT16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_SEXT32);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_SEXT32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_SEXT32_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = (uint64_t)ci_read_int(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_SEXT32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_ZEXT8);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 1);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_ZEXT8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_ZEXT16);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 2);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_ZEXT16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_ZEXT16_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 2);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 2);
    uint64_t value = ci_read_uint(slot, 2);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_ZEXT16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_ZEXT32);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 1);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 1);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_ZEXT32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_WRITE_ZEXT32_A);
    void* reg = CI_ALIGNED((char*)(&stack->registers) + op->abi_move.reg.capture_offset, 4);
    void* slot = CI_ALIGNED((char*)(frame + 1) + op->abi_move.slot, 4);
    uint64_t value = ci_read_uint(slot, 4);
    memcpy(reg, &value, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_WRITE_ZEXT32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_8);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_16);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_16_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_32);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_32_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_64);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_64_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_128);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_128_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A2);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A4);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A8);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A16);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_REG_PTR_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_8);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_16);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_16_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_32);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_32_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_64);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_64_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_128);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_128_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A2);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A4);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A8);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A16);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_TO_SLOT_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    memcpy(&source, source, sizeof source);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_8);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_16);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_16_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_32);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_32_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_64);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_64_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_128);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_128_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A2);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A4);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A8);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A16);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_SLOT_TO_SLOT_PTR_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A);
    void* source = (char*)frame->incoming + op->abi_transfer.src_reg.capture_offset;
    void* destination;
    memcpy(&destination, (char*)(frame->incoming) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);
    pc++;
    CI_OP_END(CI_OP_ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A);
    void* source;
    memcpy(&source, (char*)(&stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(&stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_PTR_TO_REG_PTR);
    void* source;
    memcpy(&source, (char*)(frame + 1) + op->abi_transfer.src_pointer, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_PTR_TO_REG_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A);
    void* source;
    memcpy(&source, (char*)(frame + 1) + op->abi_transfer.src_pointer, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_TO_REG_PTR);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_TO_REG_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_TO_REG_PTR_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_TO_REG_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_TO_SLOT_PTR);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_TO_SLOT_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_SLOT_TO_SLOT_PTR_A);
    void* source = (char*)(frame + 1) + op->abi_transfer.src;
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_SLOT_TO_SLOT_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR);
    void* source;
    memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A);
    void* source;
    memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source);
    source = (char*)source + (int32_t)op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR);
    void* source = (char*)frame->incoming + op->abi_transfer.src_base.capture_offset + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A);
    void* source = (char*)frame->incoming + op->abi_transfer.src_base.capture_offset + op->abi_transfer.src;
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);
    pc++;
    CI_OP_END(CI_OP_ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_8);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_16);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_16_A);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 2), 0, 2);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_16_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_32);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_32_A);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 4), 0, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_32_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_64);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_64);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_64_A);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 8), 0, 8);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_64_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_128);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_128);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_128_A);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 16), 0, 16);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_128_A);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_N);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 1), 0, op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_N);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_N_A2);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 2), 0, op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_N_A2);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_N_A4);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 4), 0, op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_N_A4);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_N_A8);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 8), 0, op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_N_A8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_ZERO_SLOT_N_A16);
    void* destination = (char*)(frame + 1) + op->abi_transfer.dst;
    memset(CI_ALIGNED(destination, 16), 0, op->abi_transfer.size);
    pc++;
    CI_OP_END(CI_OP_ABI_ZERO_SLOT_N_A16);
}
{
    CI_OP_BEGIN(CI_OP_ABI_CONSTANT_REG_8);
    void* destination = (char*)(&stack->registers) + op->abi_transfer.dst_reg.capture_offset;
    uint32_t value = op->abi_transfer.immediate;
    memcpy(CI_ALIGNED(destination, 1), &value, 1);
    pc++;
    CI_OP_END(CI_OP_ABI_CONSTANT_REG_8);
}
{
    CI_OP_BEGIN(CI_OP_ABI_CONSTANT_SLOT_PTR_32);
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    uint32_t value = op->abi_transfer.immediate;
    memcpy(CI_ALIGNED(destination, 1), &value, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_CONSTANT_SLOT_PTR_32);
}
{
    CI_OP_BEGIN(CI_OP_ABI_CONSTANT_SLOT_PTR_32_A);
    void* destination;
    memcpy(&destination, (char*)(frame + 1) + op->abi_transfer.dst_pointer, sizeof destination);
    destination = (char*)destination + (int32_t)op->abi_transfer.dst;
    uint32_t value = op->abi_transfer.immediate;
    memcpy(CI_ALIGNED(destination, 4), &value, 4);
    pc++;
    CI_OP_END(CI_OP_ABI_CONSTANT_SLOT_PTR_32_A);
}
{
    CI_OP_BEGIN(CI_OP_JZ8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_false.slot, 1);
    pc = !ci_uint128_nonzero(value) ? (pc + (op->jump_false.jump)) : pc + 1;
    CI_OP_END(CI_OP_JZ8);
}
{
    CI_OP_BEGIN(CI_OP_JZ16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_false.slot, 2);
    pc = !ci_uint128_nonzero(value) ? (pc + (op->jump_false.jump)) : pc + 1;
    CI_OP_END(CI_OP_JZ16);
}
{
    CI_OP_BEGIN(CI_OP_JZ32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_false.slot, 4);
    pc = !ci_uint128_nonzero(value) ? (pc + (op->jump_false.jump)) : pc + 1;
    CI_OP_END(CI_OP_JZ32);
}
{
    CI_OP_BEGIN(CI_OP_JZ64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_false.slot, 8);
    pc = !ci_uint128_nonzero(value) ? (pc + (op->jump_false.jump)) : pc + 1;
    CI_OP_END(CI_OP_JZ64);
}
{
    CI_OP_BEGIN(CI_OP_JZ128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_false.slot, 16);
    pc = !ci_uint128_nonzero(value) ? (pc + (op->jump_false.jump)) : pc + 1;
    CI_OP_END(CI_OP_JZ128);
}
{
    CI_OP_BEGIN(CI_OP_JNZ8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_true.slot, 1);
    pc = ci_uint128_nonzero(value) ? (pc + (op->jump_true.jump)) : pc + 1;
    CI_OP_END(CI_OP_JNZ8);
}
{
    CI_OP_BEGIN(CI_OP_JNZ16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_true.slot, 2);
    pc = ci_uint128_nonzero(value) ? (pc + (op->jump_true.jump)) : pc + 1;
    CI_OP_END(CI_OP_JNZ16);
}
{
    CI_OP_BEGIN(CI_OP_JNZ32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_true.slot, 4);
    pc = ci_uint128_nonzero(value) ? (pc + (op->jump_true.jump)) : pc + 1;
    CI_OP_END(CI_OP_JNZ32);
}
{
    CI_OP_BEGIN(CI_OP_JNZ64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_true.slot, 8);
    pc = ci_uint128_nonzero(value) ? (pc + (op->jump_true.jump)) : pc + 1;
    CI_OP_END(CI_OP_JNZ64);
}
{
    CI_OP_BEGIN(CI_OP_JNZ128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->jump_true.slot, 16);
    pc = ci_uint128_nonzero(value) ? (pc + (op->jump_true.jump)) : pc + 1;
    CI_OP_END(CI_OP_JNZ128);
}
{
    CI_OP_BEGIN(CI_OP_BSWAP16);
    const void* src = (char*)(frame + 1) + op->bswap.src;
    ci_write_uint((char*)(frame + 1) + op->bswap.slot, 2, bswap16((uint16_t)ci_read_uint(src, 2)));
    pc++;
    CI_OP_END(CI_OP_BSWAP16);
}
{
    CI_OP_BEGIN(CI_OP_BSWAP32);
    const void* src = (char*)(frame + 1) + op->bswap.src;
    ci_write_uint((char*)(frame + 1) + op->bswap.slot, 4, bswap32((uint32_t)ci_read_uint(src, 4)));
    pc++;
    CI_OP_END(CI_OP_BSWAP32);
}
{
    CI_OP_BEGIN(CI_OP_BSWAP64);
    const void* src = (char*)(frame + 1) + op->bswap.src;
    ci_write_uint((char*)(frame + 1) + op->bswap.slot, 8, bswap64(ci_read_uint(src, 8)));
    pc++;
    CI_OP_END(CI_OP_BSWAP64);
}
{
    CI_OP_BEGIN(CI_OP_BOUNDS32);
    uint64_t index = ci_read_uint((char*)(frame + 1) + op->bounds.src, 4);
    uint64_t length = ci_read_uint((char*)(frame + 1) + op->bounds.src2, 4);
    if(index >= length){
        const char* close = op->bounds.inclusive ? "]" : ")";
        if(op->bounds.index_signed) CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %lld not in [0, %llu%s", (long long)index, (unsigned long long)length, close));
        CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %llu not in [0, %llu%s", (unsigned long long)index, (unsigned long long)length, close));
    }
    pc++;
    CI_OP_END(CI_OP_BOUNDS32);
}
{
    CI_OP_BEGIN(CI_OP_BOUNDS_INCLUSIVE32);
    uint64_t index = ci_read_uint((char*)(frame + 1) + op->bounds.src, 4);
    uint64_t length = ci_read_uint((char*)(frame + 1) + op->bounds.src2, 4);
    if(index > length){
        const char* close = op->bounds.inclusive ? "]" : ")";
        if(op->bounds.index_signed) CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %lld not in [0, %llu%s", (long long)index, (unsigned long long)length, close));
        CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %llu not in [0, %llu%s", (unsigned long long)index, (unsigned long long)length, close));
    }
    pc++;
    CI_OP_END(CI_OP_BOUNDS_INCLUSIVE32);
}
{
    CI_OP_BEGIN(CI_OP_BOUNDS64);
    uint64_t index = ci_read_uint((char*)(frame + 1) + op->bounds.src, 8);
    uint64_t length = ci_read_uint((char*)(frame + 1) + op->bounds.src2, 8);
    if(index >= length){
        const char* close = op->bounds.inclusive ? "]" : ")";
        if(op->bounds.index_signed) CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %lld not in [0, %llu%s", (long long)index, (unsigned long long)length, close));
        CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %llu not in [0, %llu%s", (unsigned long long)index, (unsigned long long)length, close));
    }
    pc++;
    CI_OP_END(CI_OP_BOUNDS64);
}
{
    CI_OP_BEGIN(CI_OP_BOUNDS_INCLUSIVE64);
    uint64_t index = ci_read_uint((char*)(frame + 1) + op->bounds.src, 8);
    uint64_t length = ci_read_uint((char*)(frame + 1) + op->bounds.src2, 8);
    if(index > length){
        const char* close = op->bounds.inclusive ? "]" : ")";
        if(op->bounds.index_signed) CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %lld not in [0, %llu%s", (long long)index, (unsigned long long)length, close));
        CI_OP_ERROR(ci_error(ci, op->loc, "array subscript out of bounds: index %llu not in [0, %llu%s", (unsigned long long)index, (unsigned long long)length, close));
    }
    pc++;
    CI_OP_END(CI_OP_BOUNDS_INCLUSIVE64);
}
{
    CI_OP_BEGIN(CI_OP_INDEX32_S8);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = (uint64_t)ci_read_int(src, 1);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 4);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 4, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX32_S8);
}
{
    CI_OP_BEGIN(CI_OP_INDEX32_U8);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 1);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 4);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 4, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX32_U8);
}
{
    CI_OP_BEGIN(CI_OP_INDEX32_S16);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = (uint64_t)ci_read_int(src, 2);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 4);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 4, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX32_S16);
}
{
    CI_OP_BEGIN(CI_OP_INDEX32_U16);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 2);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 4);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 4, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX32_U16);
}
{
    CI_OP_BEGIN(CI_OP_INDEX32);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 4);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 4);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 4, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX32);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_S8);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = (uint64_t)ci_read_int(src, 1);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_S8);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_U8);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 1);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_U8);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_S16);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = (uint64_t)ci_read_int(src, 2);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_S16);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_U16);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 2);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_U16);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_S32);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = (uint64_t)ci_read_int(src, 4);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_S32);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64_U32);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 4);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64_U32);
}
{
    CI_OP_BEGIN(CI_OP_INDEX64);
    const void* src = (char*)(frame + 1) + op->index.index;
    uint64_t index = ci_read_uint(src, 8);
    uint64_t base = ci_read_uint((char*)(frame + 1) + op->index.base, 8);
    ci_write_uint((char*)(frame + 1) + op->index.slot, 8, base + index * op->index.scale);
    pc++;
    CI_OP_END(CI_OP_INDEX64);
}
{
    CI_OP_BEGIN(CI_OP_BF_JZ8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 1, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JZ8);
}
{
    CI_OP_BEGIN(CI_OP_BF_JNZ8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 1, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JNZ8);
}
{
    CI_OP_BEGIN(CI_OP_BF_JZ16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 2, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JZ16);
}
{
    CI_OP_BEGIN(CI_OP_BF_JNZ16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 2, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JNZ16);
}
{
    CI_OP_BEGIN(CI_OP_BF_JZ32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 4, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JZ32);
}
{
    CI_OP_BEGIN(CI_OP_BF_JNZ32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 4, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JNZ32);
}
{
    CI_OP_BEGIN(CI_OP_BF_JZ64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 8, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JZ64);
}
{
    CI_OP_BEGIN(CI_OP_BF_JNZ64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 8, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JNZ64);
}
{
    CI_OP_BEGIN(CI_OP_BF_JZ_BYTES);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_uint128_nonzero(ci_bitfield_read128_bytes(ptr + op->bitfield_jump.offset, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width));
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JZ_BYTES);
}
{
    CI_OP_BEGIN(CI_OP_BF_JNZ_BYTES);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->bitfield_jump.src, sizeof ptr);
    _Bool nonzero = ci_uint128_nonzero(ci_bitfield_read128_bytes(ptr + op->bitfield_jump.offset, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width));
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BF_JNZ_BYTES);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JZ8);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 1, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JZ8);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JNZ8);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 1, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JNZ8);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JZ16);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 2, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JZ16);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JNZ16);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 2, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JNZ16);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JZ32);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 4, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JZ32);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JNZ32);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 4, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JNZ32);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JZ64);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 8, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JZ64);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JNZ64);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_bitfield_read(ptr + op->bitfield_jump.offset, 8, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width) != 0;
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JNZ64);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JZ_BYTES);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_uint128_nonzero(ci_bitfield_read128_bytes(ptr + op->bitfield_jump.offset, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width));
    pc = !nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JZ_BYTES);
}
{
    CI_OP_BEGIN(CI_OP_BFS_JNZ_BYTES);
    char* ptr = (char*)(frame + 1) + op->bitfield_jump.src;
    _Bool nonzero = ci_uint128_nonzero(ci_bitfield_read128_bytes(ptr + op->bitfield_jump.offset, op->bitfield_jump.bit_offset, op->bitfield_jump.bit_width));
    pc = nonzero ? pc + op->bitfield_jump.jump : pc + 1;
    CI_OP_END(CI_OP_BFS_JNZ_BYTES);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U8);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S8);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U16);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U16_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 2), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U16_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S16);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S16_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 2), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S16_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U32);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U32_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 4), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U32_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S32);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S32_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 4), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S32_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U64);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U64_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 8), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U64_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S64);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S64_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 8), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S64_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U128);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 1), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U128);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_U128_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 16), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 16), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFS_U128_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S128);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 1), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S128);
}
{
    CI_OP_BEGIN(CI_OP_LDBFS_S128_A);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 16), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 16), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFS_S128_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_U8);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_U8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_S8);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_S8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_U16);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_U16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_S16);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_S16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_U32);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_U32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_S32);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_S32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_U64);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_U64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_S64);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_S64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_U128);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_U128);
}
{
    CI_OP_BEGIN(CI_OP_LDBFBS_S128);
    char* ptr = (char*)(frame + 1) + op->load_bf.src;
    CiUint128 result = ci_bitfield_read128_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFBS_S128);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBF_U8);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBF_S8);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBF_U16);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 2), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBF_U16_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBF_S16);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 2), &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBF_S16_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBF_U32);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 4), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBF_U32_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBF_S32);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 4), &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBF_S32_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBF_U64);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 8), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBF_U64_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBF_S64);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 8), &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBF_S64_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 1), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBF_U128);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_U128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 16), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 16), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBF_U128_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 1), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 1), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBF_S128);
}
{
    CI_OP_BEGIN(CI_OP_LDBF_S128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, 16), op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy(CI_ALIGNED((char*)(frame + 1) + op->load_bf.slot, 16), &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBF_S128_A);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_U8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFB_U8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_S8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 1);
    pc++;
    CI_OP_END(CI_OP_LDBFB_S8);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_U16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFB_U16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_S16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 2);
    pc++;
    CI_OP_END(CI_OP_LDBFB_S16);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_U32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFB_U32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_S32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LDBFB_S32);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_U64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFB_U64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_S64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    uint64_t result = ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 8);
    pc++;
    CI_OP_END(CI_OP_LDBFB_S64);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_U128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !1);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFB_U128);
}
{
    CI_OP_BEGIN(CI_OP_LDBFB_S128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->load_bf.src, sizeof ptr);
    CiUint128 result = ci_bitfield_read128_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width);
    result = ci_bitfield_extend128(result, op->load_bf.bit_width, !0);
    memcpy((char*)(frame + 1) + op->load_bf.slot, &result, 16);
    pc++;
    CI_OP_END(CI_OP_LDBFB_S128);
}
{
    CI_OP_BEGIN(CI_OP_STBF8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 1), 1);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 1), 1, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF8);
}
{
    CI_OP_BEGIN(CI_OP_STBF16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 1), 2);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 1), 2, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF16);
}
{
    CI_OP_BEGIN(CI_OP_STBF16_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 2), 2);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 2), 2, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF16_A);
}
{
    CI_OP_BEGIN(CI_OP_STBF32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 1), 4);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 1), 4, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF32);
}
{
    CI_OP_BEGIN(CI_OP_STBF32_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 4), 4);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 4), 4, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF32_A);
}
{
    CI_OP_BEGIN(CI_OP_STBF64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 1), 8);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 1), 8, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF64);
}
{
    CI_OP_BEGIN(CI_OP_STBF64_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 8), 8);
    ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, 8), 8, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF64_A);
}
{
    CI_OP_BEGIN(CI_OP_STBF128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    CiUint128 value = ci_read_u128(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 1));
    ci_bitfield_write_128(CI_ALIGNED(ptr + op->store_bf.offset, 1), op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF128);
}
{
    CI_OP_BEGIN(CI_OP_STBF128_A);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    CiUint128 value = ci_read_u128(CI_ALIGNED((char*)(frame + 1) + op->store_bf.src, 16));
    ci_bitfield_write_128(CI_ALIGNED(ptr + op->store_bf.offset, 16), op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBF128_A);
}
{
    CI_OP_BEGIN(CI_OP_STBFB8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->store_bf.src, 1);
    ci_bitfield_write_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBFB8);
}
{
    CI_OP_BEGIN(CI_OP_STBFB16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->store_bf.src, 2);
    ci_bitfield_write_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBFB16);
}
{
    CI_OP_BEGIN(CI_OP_STBFB32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->store_bf.src, 4);
    ci_bitfield_write_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBFB32);
}
{
    CI_OP_BEGIN(CI_OP_STBFB64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->store_bf.src, 8);
    ci_bitfield_write_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBFB64);
}
{
    CI_OP_BEGIN(CI_OP_STBFB128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->store_bf.slot, sizeof ptr);
    CiUint128 value = ci_read_u128((char*)(frame + 1) + op->store_bf.src);
    ci_bitfield_write_128_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value);
    pc++;
    CI_OP_END(CI_OP_STBFB128);
}
{
    CI_OP_BEGIN(CI_OP_CALL_DIRECT);
    CI_OP_SYNC_PC();
    CiCallDescriptor* d = op->call.descrip;
    NcCallState* state = &stack->registers;
    const CcFunc* func = d->func;
    void (*fn)(void) = func->native_func;
    if(func && func->defined){
        const CcFunc* target = ci_hotswap_target(func);
        if(!target) CI_OP_ERROR(ci_error(ci, op->loc, "hotswap cycle detected"));
        CI_OP_CALL(target->interp_ops->code);
    }
    if(!fn) CI_OP_ERROR(ci_error(ci, op->loc, "call through null function pointer"));
    NcCallState native = *state;
    int err = d->invoke(d->abi, fn, &native);
    if(err) CI_OP_ERROR(err);
    size_t offset = nc_capture_result_offset(d->abi->target);
    memcpy((char*)state + offset, (char*)&native + offset, nc_capture_size(d->abi->target) - offset);
    pc++;
    CI_OP_END(CI_OP_CALL_DIRECT);
}
{
    CI_OP_BEGIN(CI_OP_CALL_INDIRECT);
    CI_OP_SYNC_PC();
    CiCallDescriptor* d = op->call.descrip;
    NcCallState* state = &stack->registers;
    void (*fn)(void);
    memcpy(&fn, (char*)(frame + 1) + op->call.args_slot - CI_INDIRECT_CALL_PREFIX_SIZE, sizeof fn);
    const CcFunc* func;
    memcpy(&func, (char*)(frame + 1) + op->call.args_slot - sizeof func, sizeof func);
    if(func && func->defined){
        const CcFunc* target = ci_hotswap_target(func);
        if(!target) CI_OP_ERROR(ci_error(ci, op->loc, "hotswap cycle detected"));
        CI_OP_CALL(target->interp_ops->code);
    }
    if(!fn) CI_OP_ERROR(ci_error(ci, op->loc, "call through null function pointer"));
    NcCallState native = *state;
    int err = d->invoke(d->abi, fn, &native);
    if(err) CI_OP_ERROR(err);
    size_t offset = nc_capture_result_offset(d->abi->target);
    memcpy((char*)state + offset, (char*)&native + offset, nc_capture_size(d->abi->target) - offset);
    pc++;
    CI_OP_END(CI_OP_CALL_INDIRECT);
}
{
    CI_OP_BEGIN(CI_OP_CALL_RESOLVE);
    void (*fn)(void);
    char* storage = (char*)(frame + 1) + op->call_resolve.slot;
    memcpy(&fn, storage, sizeof fn);
    const CcFunc* func = ci_closure_map_get(ci, (void*)fn);
    memcpy(storage + sizeof fn, &func, sizeof func);
    pc++;
    CI_OP_END(CI_OP_CALL_RESOLVE);
}
{
    CI_OP_BEGIN(CI_OP_SWITCH_DENSE);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->switch_.slot, 8);
    const CiSwitchTable* table = op->switch_.table;
    uint64_t index = value - table->data[0].low;
    pc = (pc + (index < table->count ? table->data[index].target : op->switch_.jump));
    CI_OP_END(CI_OP_SWITCH_DENSE);
}
{
    CI_OP_BEGIN(CI_OP_SWITCH_SIGNED);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->switch_.slot, 8) ^ (UINT64_C(1) << 63);
    const CiSwitchTable* table = op->switch_.table;
    size_t lo = 0, hi = table->count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if((table->data[mid].high ^ (UINT64_C(1) << 63)) < value) lo = mid + 1;
        else if((table->data[mid].low ^ (UINT64_C(1) << 63)) > value) hi = mid;
        else {
            pc = (pc + (table->data[mid].target));
            CI_OP_END(CI_OP_SWITCH_SIGNED);
        }
    }
    pc = (pc + (op->switch_.jump));
    CI_OP_END(CI_OP_SWITCH_SIGNED);
}
{
    CI_OP_BEGIN(CI_OP_SWITCH_UNSIGNED);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->switch_.slot, 8) ^ (0);
    const CiSwitchTable* table = op->switch_.table;
    size_t lo = 0, hi = table->count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if((table->data[mid].high ^ (0)) < value) lo = mid + 1;
        else if((table->data[mid].low ^ (0)) > value) hi = mid;
        else {
            pc = (pc + (table->data[mid].target));
            CI_OP_END(CI_OP_SWITCH_UNSIGNED);
        }
    }
    pc = (pc + (op->switch_.jump));
    CI_OP_END(CI_OP_SWITCH_UNSIGNED);
}
{
    CI_OP_BEGIN(CI_OP_TRAP);
    CI_OP_ERROR(ci_error(ci, op->loc, "__builtin_trap"));
}
{
    CI_OP_BEGIN(CI_OP_UNREACHABLE);
    CI_OP_ERROR(ci_error(ci, op->loc, "__builtin_unreachable reached"));
}
{
    CI_OP_BEGIN(CI_OP_ABORT);
    CI_OP_ERROR(ci_error(ci, op->loc, "__builtin_abort called"));
}
{
    CI_OP_BEGIN(CI_OP_DEBUGTRAP);
    pc++;
    CI_OP_END(CI_OP_TRAP);
}
{
    CI_OP_BEGIN(CI_OP_BACKTRACE);
    CI_OP_SYNC_PC();
    ci_backtrace(ci, frame, 0);
    pc++;
    CI_OP_END(CI_OP_BACKTRACE);
}
{
    CI_OP_BEGIN(CI_OP_READCYCLECOUNTER);
    uint64_t ticks = performance_counter();
    memcpy((char*)(frame + 1) + op->builtin.slot, &ticks, 8);
    pc++;
    CI_OP_END(CI_OP_READCYCLECOUNTER);
}
{
    CI_OP_BEGIN(CI_OP_READCYCLECOUNTER_DISCARD);
    (void)performance_counter();
    pc++;
    CI_OP_END(CI_OP_READCYCLECOUNTER_DISCARD);
}
{
    CI_OP_BEGIN(CI_OP_STACK_ALLOC);
    size_t size;
    memcpy(&size, (char*)(frame + 1) + op->alloca.src, sizeof size);
    void* ptr = ci_stack_alloc(stack, size, op->alloca.alignment);
    if(!ptr) CI_OP_ERROR(CI_OOM_ERROR);
    memcpy((char*)(frame + 1) + op->alloca.slot, &ptr, sizeof ptr);
    pc++;
    CI_OP_END(CI_OP_STACK_ALLOC);
}
{
    CI_OP_BEGIN(CI_OP_STACK_ZALLOC);
    size_t size;
    memcpy(&size, (char*)(frame + 1) + op->alloca.src, sizeof size);
    void* ptr = ci_stack_zalloc(stack, size, op->alloca.alignment);
    if(!ptr) CI_OP_ERROR(CI_OOM_ERROR);
    memcpy((char*)(frame + 1) + op->alloca.slot, &ptr, sizeof ptr);
    pc++;
    CI_OP_END(CI_OP_STACK_ZALLOC);
}
{
    CI_OP_BEGIN(CI_OP_THREAD_FENCE);
    CI_THREAD_FENCE();
    pc++;
    CI_OP_END(CI_OP_THREAD_FENCE);
}
{
    CI_OP_BEGIN(CI_OP_SIGNAL_FENCE);
    pc++;
    CI_OP_END(CI_OP_SIGNAL_FENCE);
}
{
    CI_OP_BEGIN(CI_OP_FFS8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_ctz(lo, hi, 8) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FFS8);
}
{
    CI_OP_BEGIN(CI_OP_FFS16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_ctz(lo, hi, 16) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FFS16);
}
{
    CI_OP_BEGIN(CI_OP_FFS32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_ctz(lo, hi, 32) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FFS32);
}
{
    CI_OP_BEGIN(CI_OP_FFS64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_ctz(lo, hi, 64) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FFS64);
}
{
    CI_OP_BEGIN(CI_OP_FFS128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_ctz(lo, hi, 128) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FFS128);
}
{
    CI_OP_BEGIN(CI_OP_CLRSB8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo >> 7) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) - 1 : cc_bit_builtin_clz(lo, hi, 8) - 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLRSB8);
}
{
    CI_OP_BEGIN(CI_OP_CLRSB16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo >> 15) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) - 1 : cc_bit_builtin_clz(lo, hi, 16) - 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLRSB16);
}
{
    CI_OP_BEGIN(CI_OP_CLRSB32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo >> 31) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) - 1 : cc_bit_builtin_clz(lo, hi, 32) - 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLRSB32);
}
{
    CI_OP_BEGIN(CI_OP_CLRSB64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo >> 63) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) - 1 : cc_bit_builtin_clz(lo, hi, 64) - 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLRSB64);
}
{
    CI_OP_BEGIN(CI_OP_CLRSB128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((hi >> 63) ? cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) - 1 : cc_bit_builtin_clz(lo, hi, 128) - 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLRSB128);
}
{
    CI_OP_BEGIN(CI_OP_PARITY8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((popcount_64(lo) + popcount_64(hi)) & 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_PARITY8);
}
{
    CI_OP_BEGIN(CI_OP_PARITY16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((popcount_64(lo) + popcount_64(hi)) & 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_PARITY16);
}
{
    CI_OP_BEGIN(CI_OP_PARITY32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((popcount_64(lo) + popcount_64(hi)) & 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_PARITY32);
}
{
    CI_OP_BEGIN(CI_OP_PARITY64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((popcount_64(lo) + popcount_64(hi)) & 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_PARITY64);
}
{
    CI_OP_BEGIN(CI_OP_PARITY128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((popcount_64(lo) + popcount_64(hi)) & 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_PARITY128);
}
{
    CI_OP_BEGIN(CI_OP_CLZ8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo, hi, 8));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLZ8);
}
{
    CI_OP_BEGIN(CI_OP_CLZ16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo, hi, 16));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLZ16);
}
{
    CI_OP_BEGIN(CI_OP_CLZ32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo, hi, 32));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLZ32);
}
{
    CI_OP_BEGIN(CI_OP_CLZ64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo, hi, 64));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLZ64);
}
{
    CI_OP_BEGIN(CI_OP_CLZ128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo, hi, 128));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CLZ128);
}
{
    CI_OP_BEGIN(CI_OP_CTZ8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo, hi, 8));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CTZ8);
}
{
    CI_OP_BEGIN(CI_OP_CTZ16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo, hi, 16));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CTZ16);
}
{
    CI_OP_BEGIN(CI_OP_CTZ32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo, hi, 32));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CTZ32);
}
{
    CI_OP_BEGIN(CI_OP_CTZ64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo, hi, 64));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CTZ64);
}
{
    CI_OP_BEGIN(CI_OP_CTZ128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo, hi, 128));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CTZ128);
}
{
    CI_OP_BEGIN(CI_OP_POPCOUNT8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(popcount_64(lo) + popcount_64(hi));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_POPCOUNT8);
}
{
    CI_OP_BEGIN(CI_OP_POPCOUNT16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(popcount_64(lo) + popcount_64(hi));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_POPCOUNT16);
}
{
    CI_OP_BEGIN(CI_OP_POPCOUNT32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(popcount_64(lo) + popcount_64(hi));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_POPCOUNT32);
}
{
    CI_OP_BEGIN(CI_OP_POPCOUNT64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(popcount_64(lo) + popcount_64(hi));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_POPCOUNT64);
}
{
    CI_OP_BEGIN(CI_OP_POPCOUNT128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(popcount_64(lo) + popcount_64(hi));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_POPCOUNT128);
}
{
    CI_OP_BEGIN(CI_OP_BIT_WIDTH8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(8 - cc_bit_builtin_clz(lo, hi, 8));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BIT_WIDTH8);
}
{
    CI_OP_BEGIN(CI_OP_BIT_WIDTH16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(16 - cc_bit_builtin_clz(lo, hi, 16));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BIT_WIDTH16);
}
{
    CI_OP_BEGIN(CI_OP_BIT_WIDTH32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(32 - cc_bit_builtin_clz(lo, hi, 32));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BIT_WIDTH32);
}
{
    CI_OP_BEGIN(CI_OP_BIT_WIDTH64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(64 - cc_bit_builtin_clz(lo, hi, 64));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BIT_WIDTH64);
}
{
    CI_OP_BEGIN(CI_OP_BIT_WIDTH128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(128 - cc_bit_builtin_clz(lo, hi, 128));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_BIT_WIDTH128);
}
{
    CI_OP_BEGIN(CI_OP_COUNT_ZEROS8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(8 - (popcount_64(lo) + popcount_64(hi)));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_COUNT_ZEROS8);
}
{
    CI_OP_BEGIN(CI_OP_COUNT_ZEROS16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(16 - (popcount_64(lo) + popcount_64(hi)));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_COUNT_ZEROS16);
}
{
    CI_OP_BEGIN(CI_OP_COUNT_ZEROS32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(32 - (popcount_64(lo) + popcount_64(hi)));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_COUNT_ZEROS32);
}
{
    CI_OP_BEGIN(CI_OP_COUNT_ZEROS64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(64 - (popcount_64(lo) + popcount_64(hi)));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_COUNT_ZEROS64);
}
{
    CI_OP_BEGIN(CI_OP_COUNT_ZEROS128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(128 - (popcount_64(lo) + popcount_64(hi)));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_COUNT_ZEROS128);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ONE8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_clz(lo, hi, 8) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ONE8);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ONE16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_clz(lo, hi, 16) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ONE16);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ONE32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_clz(lo, hi, 32) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ONE32);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ONE64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_clz(lo, hi, 64) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ONE64);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ONE128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)((lo || hi) ? cc_bit_builtin_clz(lo, hi, 128) + 1 : 0);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ONE128);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ZERO8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) == 8 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ZERO8);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ZERO16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) == 16 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ZERO16);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ZERO32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) == 32 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ZERO32);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ZERO64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) == 64 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ZERO64);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_LEADING_ZERO128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) == 128 ? 0 : cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_LEADING_ZERO128);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_TRAILING_ZERO8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8) == 8 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_TRAILING_ZERO8);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_TRAILING_ZERO16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16) == 16 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_TRAILING_ZERO16);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_TRAILING_ZERO32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32) == 32 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_TRAILING_ZERO32);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_TRAILING_ZERO64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64) == 64 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_TRAILING_ZERO64);
}
{
    CI_OP_BEGIN(CI_OP_FIRST_TRAILING_ZERO128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128) == 128 ? 0 : cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128) + 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_FIRST_TRAILING_ZERO128);
}
{
    CI_OP_BEGIN(CI_OP_LEADING_ONES8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LEADING_ONES8);
}
{
    CI_OP_BEGIN(CI_OP_LEADING_ONES16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LEADING_ONES16);
}
{
    CI_OP_BEGIN(CI_OP_LEADING_ONES32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LEADING_ONES32);
}
{
    CI_OP_BEGIN(CI_OP_LEADING_ONES64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LEADING_ONES64);
}
{
    CI_OP_BEGIN(CI_OP_LEADING_ONES128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_LEADING_ONES128);
}
{
    CI_OP_BEGIN(CI_OP_TRAILING_ONES8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_TRAILING_ONES8);
}
{
    CI_OP_BEGIN(CI_OP_TRAILING_ONES16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_TRAILING_ONES16);
}
{
    CI_OP_BEGIN(CI_OP_TRAILING_ONES32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_TRAILING_ONES32);
}
{
    CI_OP_BEGIN(CI_OP_TRAILING_ONES64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_TRAILING_ONES64);
}
{
    CI_OP_BEGIN(CI_OP_TRAILING_ONES128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    uint32_t result = (uint32_t)(cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128));
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_TRAILING_ONES128);
}
{
    CI_OP_BEGIN(CI_OP_HAS_SINGLE_BIT8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1;
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_HAS_SINGLE_BIT8);
}
{
    CI_OP_BEGIN(CI_OP_HAS_SINGLE_BIT16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1;
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_HAS_SINGLE_BIT16);
}
{
    CI_OP_BEGIN(CI_OP_HAS_SINGLE_BIT32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1;
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_HAS_SINGLE_BIT32);
}
{
    CI_OP_BEGIN(CI_OP_HAS_SINGLE_BIT64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1;
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_HAS_SINGLE_BIT64);
}
{
    CI_OP_BEGIN(CI_OP_HAS_SINGLE_BIT128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1;
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_HAS_SINGLE_BIT128);
}
{
    CI_OP_BEGIN(CI_OP_BIT_CEIL8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(!hi && lo <= 1) result = ci_uint128_from_uint64(1);
    else {
        uint32_t n = 8 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 56), hi - (lo == 0), 8);
        if(n < 8) result = cc_bit_builtin_power(n);
    }
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_BIT_CEIL8);
}
{
    CI_OP_BEGIN(CI_OP_BIT_CEIL16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(!hi && lo <= 1) result = ci_uint128_from_uint64(1);
    else {
        uint32_t n = 16 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 48), hi - (lo == 0), 16);
        if(n < 16) result = cc_bit_builtin_power(n);
    }
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_BIT_CEIL16);
}
{
    CI_OP_BEGIN(CI_OP_BIT_CEIL32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(!hi && lo <= 1) result = ci_uint128_from_uint64(1);
    else {
        uint32_t n = 32 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 32), hi - (lo == 0), 32);
        if(n < 32) result = cc_bit_builtin_power(n);
    }
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_BIT_CEIL32);
}
{
    CI_OP_BEGIN(CI_OP_BIT_CEIL64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(!hi && lo <= 1) result = ci_uint128_from_uint64(1);
    else {
        uint32_t n = 64 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 0), hi - (lo == 0), 64);
        if(n < 64) result = cc_bit_builtin_power(n);
    }
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_BIT_CEIL64);
}
{
    CI_OP_BEGIN(CI_OP_BIT_CEIL128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(!hi && lo <= 1) result = ci_uint128_from_uint64(1);
    else {
        uint32_t n = 128 - cc_bit_builtin_clz((lo - 1) & UINT64_MAX, hi - (lo == 0), 128);
        if(n < 128) result = cc_bit_builtin_power(n);
    }
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_BIT_CEIL128);
}
{
    CI_OP_BEGIN(CI_OP_BIT_FLOOR8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(lo || hi) result = cc_bit_builtin_power(8 - cc_bit_builtin_clz(lo, hi, 8) - 1);
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_BIT_FLOOR8);
}
{
    CI_OP_BEGIN(CI_OP_BIT_FLOOR16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(lo || hi) result = cc_bit_builtin_power(16 - cc_bit_builtin_clz(lo, hi, 16) - 1);
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_BIT_FLOOR16);
}
{
    CI_OP_BEGIN(CI_OP_BIT_FLOOR32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(lo || hi) result = cc_bit_builtin_power(32 - cc_bit_builtin_clz(lo, hi, 32) - 1);
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_BIT_FLOOR32);
}
{
    CI_OP_BEGIN(CI_OP_BIT_FLOOR64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(lo || hi) result = cc_bit_builtin_power(64 - cc_bit_builtin_clz(lo, hi, 64) - 1);
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_BIT_FLOOR64);
}
{
    CI_OP_BEGIN(CI_OP_BIT_FLOOR128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value);
    CiUint128 result = ci_uint128_from_uint64(0);
    if(lo || hi) result = cc_bit_builtin_power(128 - cc_bit_builtin_clz(lo, hi, 128) - 1);
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_BIT_FLOOR128);
}
{
    CI_OP_BEGIN(CI_OP_ROTL8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 8);
    CiUint128 result = ci_uint128_or(ci_uint128_shl(value, shift), ci_uint128_shr(value, (8 - shift) % 8));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_ROTL8);
}
{
    CI_OP_BEGIN(CI_OP_ROTR8);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 1);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 8);
    CiUint128 result = ci_uint128_or(ci_uint128_shr(value, shift), ci_uint128_shl(value, (8 - shift) % 8));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_ROTR8);
}
{
    CI_OP_BEGIN(CI_OP_ROTL16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 16);
    CiUint128 result = ci_uint128_or(ci_uint128_shl(value, shift), ci_uint128_shr(value, (16 - shift) % 16));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_ROTL16);
}
{
    CI_OP_BEGIN(CI_OP_ROTR16);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 2);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 16);
    CiUint128 result = ci_uint128_or(ci_uint128_shr(value, shift), ci_uint128_shl(value, (16 - shift) % 16));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_ROTR16);
}
{
    CI_OP_BEGIN(CI_OP_ROTL32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 32);
    CiUint128 result = ci_uint128_or(ci_uint128_shl(value, shift), ci_uint128_shr(value, (32 - shift) % 32));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ROTL32);
}
{
    CI_OP_BEGIN(CI_OP_ROTR32);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 4);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 32);
    CiUint128 result = ci_uint128_or(ci_uint128_shr(value, shift), ci_uint128_shl(value, (32 - shift) % 32));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ROTR32);
}
{
    CI_OP_BEGIN(CI_OP_ROTL64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 64);
    CiUint128 result = ci_uint128_or(ci_uint128_shl(value, shift), ci_uint128_shr(value, (64 - shift) % 64));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ROTL64);
}
{
    CI_OP_BEGIN(CI_OP_ROTR64);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 8);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 64);
    CiUint128 result = ci_uint128_or(ci_uint128_shr(value, shift), ci_uint128_shl(value, (64 - shift) % 64));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ROTR64);
}
{
    CI_OP_BEGIN(CI_OP_ROTL128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 128);
    CiUint128 result = ci_uint128_or(ci_uint128_shl(value, shift), ci_uint128_shr(value, (128 - shift) % 128));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ROTL128);
}
{
    CI_OP_BEGIN(CI_OP_ROTR128);
    CiUint128 value;
    ci_uint128_read(&value, (char*)(frame + 1) + op->bit_builtin.src, 16);
    uint32_t shift = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8) % 128);
    CiUint128 result = ci_uint128_or(ci_uint128_shr(value, shift), ci_uint128_shl(value, (128 - shift) % 128));
    ci_uint128_write((char*)(frame + 1) + op->bit_builtin.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ROTR128);
}
{
    CI_OP_BEGIN(CI_OP_CRC32C8);
    uint32_t seed = (uint32_t)ci_read_uint((char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 1);
    uint32_t result = drp_crc32c(seed, value, 1);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CRC32C8);
}
{
    CI_OP_BEGIN(CI_OP_CRC32C16);
    uint32_t seed = (uint32_t)ci_read_uint((char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 2);
    uint32_t result = drp_crc32c(seed, value, 2);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CRC32C16);
}
{
    CI_OP_BEGIN(CI_OP_CRC32C32);
    uint32_t seed = (uint32_t)ci_read_uint((char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 4);
    uint32_t result = drp_crc32c(seed, value, 4);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CRC32C32);
}
{
    CI_OP_BEGIN(CI_OP_CRC32C64);
    uint32_t seed = (uint32_t)ci_read_uint((char*)(frame + 1) + op->bit_builtin.src, 4);
    uint64_t value = ci_read_uint((char*)(frame + 1) + op->bit_builtin.src2, 8);
    uint32_t result = drp_crc32c(seed, value, 8);
    memcpy((char*)(frame + 1) + op->bit_builtin.slot, &result, 4);
    pc++;
    CI_OP_END(CI_OP_CRC32C64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_LOAD8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_load.src, sizeof ptr);
    ptr += op->atomic_load.offset;
    void* dest = (char*)(frame + 1) + op->atomic_load.slot;
    CI_ATOMIC_LOAD8(ptr, dest);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_LOAD8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_LOAD16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_load.src, sizeof ptr);
    ptr += op->atomic_load.offset;
    void* dest = (char*)(frame + 1) + op->atomic_load.slot;
    CI_ATOMIC_LOAD16(ptr, dest);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_LOAD16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_LOAD32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_load.src, sizeof ptr);
    ptr += op->atomic_load.offset;
    void* dest = (char*)(frame + 1) + op->atomic_load.slot;
    CI_ATOMIC_LOAD32(ptr, dest);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_LOAD32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_LOAD64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_load.src, sizeof ptr);
    ptr += op->atomic_load.offset;
    void* dest = (char*)(frame + 1) + op->atomic_load.slot;
    CI_ATOMIC_LOAD64(ptr, dest);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_LOAD64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_LOAD128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_load.src, sizeof ptr);
    ptr += op->atomic_load.offset;
    void* dest = (char*)(frame + 1) + op->atomic_load.slot;
    CI_ATOMIC_LOAD128(ptr, dest);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_LOAD128);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_STORE8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_store.slot, sizeof ptr);
    ptr += op->atomic_store.offset;
    void* src = (char*)(frame + 1) + op->atomic_store.src;
    CI_ATOMIC_STORE8(ptr, src);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_STORE8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_STORE16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_store.slot, sizeof ptr);
    ptr += op->atomic_store.offset;
    void* src = (char*)(frame + 1) + op->atomic_store.src;
    CI_ATOMIC_STORE16(ptr, src);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_STORE16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_STORE32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_store.slot, sizeof ptr);
    ptr += op->atomic_store.offset;
    void* src = (char*)(frame + 1) + op->atomic_store.src;
    CI_ATOMIC_STORE32(ptr, src);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_STORE32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_STORE64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_store.slot, sizeof ptr);
    ptr += op->atomic_store.offset;
    void* src = (char*)(frame + 1) + op->atomic_store.src;
    CI_ATOMIC_STORE64(ptr, src);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_STORE64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_STORE128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_store.slot, sizeof ptr);
    ptr += op->atomic_store.offset;
    void* src = (char*)(frame + 1) + op->atomic_store.src;
    CI_ATOMIC_STORE128(ptr, src);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_STORE128);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XCHG8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XCHG8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XCHG8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XCHG16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XCHG16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XCHG16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XCHG32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XCHG32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XCHG32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XCHG64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XCHG64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XCHG64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XCHG128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XCHG128(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XCHG128);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_ADD8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_ADD8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_ADD8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_ADD16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_ADD16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_ADD16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_ADD32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_ADD32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_ADD32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_ADD64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_ADD64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_ADD64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_SUB8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_SUB8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_SUB8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_SUB16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_SUB16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_SUB16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_SUB32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_SUB32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_SUB32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_SUB64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_SUB64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_SUB64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_AND8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_AND8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_AND8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_AND16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_AND16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_AND16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_AND32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_AND32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_AND32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_AND64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_AND64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_AND64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_OR8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_OR8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_OR8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_OR16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_OR16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_OR16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_OR32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_OR32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_OR32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_OR64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_OR64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_OR64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XOR8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XOR8(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XOR8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XOR16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XOR16(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XOR16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XOR32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XOR32(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XOR32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_XOR64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_rmw.src, sizeof ptr);
    ptr += op->atomic_rmw.offset;
    void* old = (char*)(frame + 1) + op->atomic_rmw.slot;
    void* value = (char*)(frame + 1) + op->atomic_rmw.src2;
    CI_ATOMIC_XOR64(ptr, value, old);
    pc++;
    CI_OP_END(CI_OP_ATOMIC_XOR64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS8(ptr, expected, desired, 0, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS_WEAK8);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS8(ptr, expected, desired, 1, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS_WEAK8);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS16(ptr, expected, desired, 0, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS_WEAK16);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS16(ptr, expected, desired, 1, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS_WEAK16);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS32(ptr, expected, desired, 0, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS_WEAK32);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS32(ptr, expected, desired, 1, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS_WEAK32);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS64(ptr, expected, desired, 0, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS_WEAK64);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS64(ptr, expected, desired, 1, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS_WEAK64);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS128(ptr, expected, desired, 0, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS128);
}
{
    CI_OP_BEGIN(CI_OP_ATOMIC_CAS_WEAK128);
    char* ptr;
    memcpy(&ptr, (char*)(frame + 1) + op->atomic_cas.src, sizeof ptr);
    ptr += op->atomic_cas.offset;
    void* expected = (char*)(frame + 1) + op->atomic_cas.expected;
    void* desired = (char*)(frame + 1) + op->atomic_cas.desired;
    _Bool result;
    CI_ATOMIC_CAS128(ptr, expected, desired, 1, result);
    *((unsigned char*)(frame + 1) + op->atomic_cas.slot) = result;
    pc++;
    CI_OP_END(CI_OP_ATOMIC_CAS_WEAK128);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_SYSV);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    NcSysvVaListTag* va = ap;
    unsigned ng = op->va_arg_.gp, nf = op->va_arg_.fp;
    if(va->gp_offset + ng * 8 <= 48 && va->fp_offset + nf * 16 <= 176){
        pointers[0] = (char*)va->reg_save_area + va->gp_offset;
        pointers[1] = (char*)va->reg_save_area + va->fp_offset;
        va->gp_offset += ng * 8;
        va->fp_offset += nf * 16;
        pc++;
        CI_OP_END();
    }
    void** va_stack = &va->overflow_arg_area;
    void* source = ci_va_stack_source(va_stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc += 2 + op->va_arg_.count;
    CI_OP_END(CI_OP_VA_ARG_SYSV);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_ARM);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    NcAapcs64VaList* va = ap;
    unsigned ng = op->va_arg_.gp, nf = op->va_arg_.fp;
    int go = va->__gr_offs, fo = va->__vr_offs;
    if((!ng || (go < 0 && go + (int)ng * 8 <= 0)) && (!nf || (fo < 0 && fo + (int)nf * 16 <= 0))){
        pointers[0] = ng ? (char*)va->__gr_top + go : NULL;
        pointers[1] = nf ? (char*)va->__vr_top + fo : NULL;
        if(ng) va->__gr_offs = go + (int)ng * 8;
        if(nf) va->__vr_offs = fo + (int)nf * 16;
        pc++;
        CI_OP_END();
    }
    if(ng) va->__gr_offs = 0;
    if(nf) va->__vr_offs = 0;
    void** va_stack = &va->__stack;
    void* source = ci_va_stack_source(va_stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc += 2 + op->va_arg_.count;
    CI_OP_END(CI_OP_VA_ARG_ARM);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_ARM_ALIGN16);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    NcAapcs64VaList* va = ap;
    unsigned ng = op->va_arg_.gp, nf = op->va_arg_.fp;
    int go = va->__gr_offs, fo = va->__vr_offs;
    go = (go + 15) & ~15;
    if((!ng || (go < 0 && go + (int)ng * 8 <= 0)) && (!nf || (fo < 0 && fo + (int)nf * 16 <= 0))){
        pointers[0] = ng ? (char*)va->__gr_top + go : NULL;
        pointers[1] = nf ? (char*)va->__vr_top + fo : NULL;
        if(ng) va->__gr_offs = go + (int)ng * 8;
        if(nf) va->__vr_offs = fo + (int)nf * 16;
        pc++;
        CI_OP_END();
    }
    if(ng) va->__gr_offs = 0;
    if(nf) va->__vr_offs = 0;
    void** va_stack = &va->__stack;
    void* source = ci_va_stack_source(va_stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc += 2 + op->va_arg_.count;
    CI_OP_END(CI_OP_VA_ARG_ARM_ALIGN16);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_ARM_INDIRECT);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    NcAapcs64VaList* va = ap;
    void* location;
    if(va->__gr_offs < 0 && va->__gr_offs + 8 <= 0){
        location = (char*)va->__gr_top + va->__gr_offs;
        va->__gr_offs += 8;
    } else {
        va->__gr_offs = 0;
        location = ci_va_stack_source(&va->__stack, 7, 8);
    }
    void* source;
    memcpy(&source, location, sizeof source);
    pointers[0] = source;
    pc++;
    CI_OP_END(CI_OP_VA_ARG_ARM_INDIRECT);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_STACK);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    void* source = ci_va_stack_source((void**)ap, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc++;
    CI_OP_END(CI_OP_VA_ARG_STACK);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_STACK_INDIRECT);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    void* source = ci_va_stack_source((void**)ap, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    memcpy(&source, source, sizeof source);
    pointers[0] = source;
    pc++;
    CI_OP_END(CI_OP_VA_ARG_STACK_INDIRECT);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_SYSV_STACK);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    void* source = ci_va_stack_source(&((NcSysvVaListTag*)ap)->overflow_arg_area, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc++;
    CI_OP_END(CI_OP_VA_ARG_SYSV_STACK);
}
{
    CI_OP_BEGIN(CI_OP_VA_ARG_ARM_STACK);
    void* ap;
    memcpy(&ap, (char*)(frame + 1) + op->va_arg_.src, sizeof ap);
    void** pointers = (void**)((char*)(frame + 1) + op->va_arg_.pointers);
    void* source = ci_va_stack_source(&((NcAapcs64VaList*)ap)->__stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride);
    pointers[0] = source;
    pc++;
    CI_OP_END(CI_OP_VA_ARG_ARM_STACK);
}

{
    CI_OP_BEGIN(CI_OP_CAPTURE_BACKTRACE);
    CI_OP_SYNC_PC();
    void* result = (char*)(frame + 1) + op->capture_backtrace.slot;
    _Static_assert(sizeof(SrcLoc) == sizeof(void*), "backtrace entry size");
    void** buffer;
    int capacity, count = 0;
    CI_INLINE_MEMCPY(&buffer, (char*)(frame + 1) + op->capture_backtrace.buffer, sizeof buffer);
    CI_INLINE_MEMCPY(&capacity, (char*)(frame + 1) + op->capture_backtrace.capacity, sizeof capacity);
    if(capacity > 0 && !buffer)
        CI_OP_ERROR(ci_error(ci, op->loc, "__builtin_capture_backtrace buffer must not be NULL"));
    for(CiInterpFrame* f = frame; f && count < capacity; f = f->parent){
        if(!f->code || f->pc >= f->code->op_count) continue;
        CI_INLINE_MEMCPY(&buffer[count++], &f->code->ops[f->pc].loc, sizeof(SrcLoc));
    }
    CI_INLINE_MEMCPY(result, &count, sizeof count);
    pc++;
    CI_OP_END(CI_OP_CAPTURE_BACKTRACE);
}

{
    CI_OP_BEGIN(CI_OP_ADD8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_ADD8);
}
{
    CI_OP_BEGIN(CI_OP_SUB8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_SUB8);
}
{
    CI_OP_BEGIN(CI_OP_MUL8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_MUL8);
}
{
    CI_OP_BEGIN(CI_OP_SDIV8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = ci_int64_divrem((int8_t)a, (int8_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_SDIV8);
}
{
    CI_OP_BEGIN(CI_OP_UDIV8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_UDIV8);
}
{
    CI_OP_BEGIN(CI_OP_SMOD8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = ci_int64_divrem((int8_t)a, (int8_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_SMOD8);
}
{
    CI_OP_BEGIN(CI_OP_UMOD8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_UMOD8);
}
{
    CI_OP_BEGIN(CI_OP_AND8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_AND8);
}
{
    CI_OP_BEGIN(CI_OP_OR8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_OR8);
}
{
    CI_OP_BEGIN(CI_OP_XOR8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 1));
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_XOR8);
}
{
    CI_OP_BEGIN(CI_OP_SHL8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_SHL8);
}
{
    CI_OP_BEGIN(CI_OP_ASR8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_asr64((int8_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_ASR8);
}
{
    CI_OP_BEGIN(CI_OP_LSR8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_LSR8);
}
{
    CI_OP_BEGIN(CI_OP_NEG8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint64_t result = (uint32_t)0 - a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_NEG8);
}
{
    CI_OP_BEGIN(CI_OP_NOT8);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 1);
    uint64_t result = ~a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 1, result);
    pc++;
    CI_OP_END(CI_OP_NOT8);
}
{
    CI_OP_BEGIN(CI_OP_ADD16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_ADD16);
}
{
    CI_OP_BEGIN(CI_OP_SUB16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_SUB16);
}
{
    CI_OP_BEGIN(CI_OP_MUL16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_MUL16);
}
{
    CI_OP_BEGIN(CI_OP_SDIV16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = ci_int64_divrem((int16_t)a, (int16_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_SDIV16);
}
{
    CI_OP_BEGIN(CI_OP_UDIV16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_UDIV16);
}
{
    CI_OP_BEGIN(CI_OP_SMOD16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = ci_int64_divrem((int16_t)a, (int16_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_SMOD16);
}
{
    CI_OP_BEGIN(CI_OP_UMOD16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_UMOD16);
}
{
    CI_OP_BEGIN(CI_OP_AND16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_AND16);
}
{
    CI_OP_BEGIN(CI_OP_OR16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_OR16);
}
{
    CI_OP_BEGIN(CI_OP_XOR16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 2));
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_XOR16);
}
{
    CI_OP_BEGIN(CI_OP_SHL16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_SHL16);
}
{
    CI_OP_BEGIN(CI_OP_ASR16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_asr64((int16_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_ASR16);
}
{
    CI_OP_BEGIN(CI_OP_LSR16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_LSR16);
}
{
    CI_OP_BEGIN(CI_OP_NEG16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint64_t result = (uint32_t)0 - a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_NEG16);
}
{
    CI_OP_BEGIN(CI_OP_NOT16);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 2);
    uint64_t result = ~a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 2, result);
    pc++;
    CI_OP_END(CI_OP_NOT16);
}
{
    CI_OP_BEGIN(CI_OP_ADD32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ADD32);
}
{
    CI_OP_BEGIN(CI_OP_SUB32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SUB32);
}
{
    CI_OP_BEGIN(CI_OP_MUL32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_MUL32);
}
{
    CI_OP_BEGIN(CI_OP_SDIV32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_int64_divrem((int32_t)a, (int32_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SDIV32);
}
{
    CI_OP_BEGIN(CI_OP_UDIV32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_UDIV32);
}
{
    CI_OP_BEGIN(CI_OP_SMOD32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_int64_divrem((int32_t)a, (int32_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SMOD32);
}
{
    CI_OP_BEGIN(CI_OP_UMOD32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_UMOD32);
}
{
    CI_OP_BEGIN(CI_OP_AND32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_AND32);
}
{
    CI_OP_BEGIN(CI_OP_OR32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_OR32);
}
{
    CI_OP_BEGIN(CI_OP_XOR32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_XOR32);
}
{
    CI_OP_BEGIN(CI_OP_SHL32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SHL32);
}
{
    CI_OP_BEGIN(CI_OP_ASR32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_asr64((int32_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ASR32);
}
{
    CI_OP_BEGIN(CI_OP_LSR32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(ci_read_uint((char*)(frame + 1) + op->alu.src2, 4));
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_LSR32);
}
{
    CI_OP_BEGIN(CI_OP_NEG32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint64_t result = (uint32_t)0 - a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_NEG32);
}
{
    CI_OP_BEGIN(CI_OP_NOT32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint64_t result = ~a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_NOT32);
}
{
    CI_OP_BEGIN(CI_OP_ADD64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ADD64);
}
{
    CI_OP_BEGIN(CI_OP_SUB64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SUB64);
}
{
    CI_OP_BEGIN(CI_OP_MUL64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_MUL64);
}
{
    CI_OP_BEGIN(CI_OP_SDIV64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = ci_int64_divrem((int64_t)a, (int64_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SDIV64);
}
{
    CI_OP_BEGIN(CI_OP_UDIV64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_UDIV64);
}
{
    CI_OP_BEGIN(CI_OP_SMOD64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = ci_int64_divrem((int64_t)a, (int64_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SMOD64);
}
{
    CI_OP_BEGIN(CI_OP_UMOD64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_UMOD64);
}
{
    CI_OP_BEGIN(CI_OP_AND64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_AND64);
}
{
    CI_OP_BEGIN(CI_OP_OR64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_OR64);
}
{
    CI_OP_BEGIN(CI_OP_XOR64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 8);
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_XOR64);
}
{
    CI_OP_BEGIN(CI_OP_SHL64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SHL64);
}
{
    CI_OP_BEGIN(CI_OP_ASR64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    uint64_t result = ci_asr64((int64_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ASR64);
}
{
    CI_OP_BEGIN(CI_OP_LSR64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_LSR64);
}
{
    CI_OP_BEGIN(CI_OP_NEG64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t result = (uint64_t)0 - a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_NEG64);
}
{
    CI_OP_BEGIN(CI_OP_NOT64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t result = ~a;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_NOT64);
}
{
    CI_OP_BEGIN(CI_OP_ADD_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ADD_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_SUB_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SUB_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_MUL_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_MUL_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_SDIV_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = ci_int64_divrem((int32_t)a, (int32_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SDIV_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_UDIV_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_UDIV_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_SMOD_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = ci_int64_divrem((int32_t)a, (int32_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SMOD_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_UMOD_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_UMOD_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_AND_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_AND_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_OR_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_OR_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_XOR_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_XOR_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_SHL_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_SHL_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_ASR_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = ci_asr64((int32_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_ASR_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_LSR_IMM32);
    uint32_t a = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint32_t b = (uint32_t)(op->alu_imm.immediate);
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 4, result);
    pc++;
    CI_OP_END(CI_OP_LSR_IMM32);
}
{
    CI_OP_BEGIN(CI_OP_ADD_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a + b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ADD_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_SUB_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a - b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SUB_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_MUL_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a * b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_MUL_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_SDIV_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = ci_int64_divrem((int64_t)a, (int64_t)b, 0);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SDIV_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_UDIV_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = b ? a / b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_UDIV_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_SMOD_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = ci_int64_divrem((int64_t)a, (int64_t)b, 1);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SMOD_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_UMOD_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = b ? a % b : 0;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_UMOD_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_AND_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a & b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_AND_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_OR_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a | b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_OR_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_XOR_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = a ^ b;
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_XOR_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_SHL_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = ci_lsl64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_SHL_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_ASR_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = ci_asr64((int64_t)a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_ASR_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_LSR_IMM64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 8);
    uint64_t b = op->alu_imm.immediate;
    uint64_t result = ci_lsr64(a, b);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, result);
    pc++;
    CI_OP_END(CI_OP_LSR_IMM64);
}
{
    CI_OP_BEGIN(CI_OP_ADD128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_add(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ADD128);
}
{
    CI_OP_BEGIN(CI_OP_SUB128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_sub(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SUB128);
}
{
    CI_OP_BEGIN(CI_OP_UMUL32_64);
    uint64_t a = ci_read_uint((char*)(frame + 1) + op->alu.src, 4);
    uint64_t b = ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    ci_write_uint((char*)(frame + 1) + op->alu.slot, 8, a * b);
    pc++;
    CI_OP_END(CI_OP_UMUL32_64);
}
{
    CI_OP_BEGIN(CI_OP_UMUL64_128);
    CiUint128 a = ci_uint128_from_uint64(ci_read_uint((char*)(frame + 1) + op->alu.src, 8));
    CiUint128 b = ci_uint128_from_uint64(ci_read_uint((char*)(frame + 1) + op->alu.src2, 8));
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, ci_uint128_mul(a, b));
    pc++;
    CI_OP_END(CI_OP_UMUL64_128);
}
{
    CI_OP_BEGIN(CI_OP_MUL128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_mul(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_MUL128);
}
{
    CI_OP_BEGIN(CI_OP_SDIV128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_from_int128(ci_int128_div(ci_int128_from_uint128(a), ci_int128_from_uint128(b)));
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SDIV128);
}
{
    CI_OP_BEGIN(CI_OP_UDIV128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_div(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_UDIV128);
}
{
    CI_OP_BEGIN(CI_OP_SMOD128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_from_int128(ci_int128_mod(ci_int128_from_uint128(a), ci_int128_from_uint128(b)));
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SMOD128);
}
{
    CI_OP_BEGIN(CI_OP_UMOD128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_mod(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_UMOD128);
}
{
    CI_OP_BEGIN(CI_OP_AND128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_and(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_AND128);
}
{
    CI_OP_BEGIN(CI_OP_OR128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_or(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_OR128);
}
{
    CI_OP_BEGIN(CI_OP_XOR128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->alu.src2, 16);
    CiUint128 result = ci_uint128_xor(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_XOR128);
}
{
    CI_OP_BEGIN(CI_OP_SHL_IMM128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu_imm.src, 16);
    uint32_t b = (uint32_t)op->alu_imm.immediate;
    CiUint128 result = ci_uint128_shl(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu_imm.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SHL_IMM128);
}
{
    CI_OP_BEGIN(CI_OP_LSR_IMM128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu_imm.src, 16);
    uint32_t b = (uint32_t)op->alu_imm.immediate;
    CiUint128 result = ci_uint128_shr(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu_imm.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_LSR_IMM128);
}
{
    CI_OP_BEGIN(CI_OP_ASR_IMM128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu_imm.src, 16);
    uint32_t b = (uint32_t)op->alu_imm.immediate;
    CiUint128 result = ci_uint128_from_int128(ci_int128_shr(ci_int128_from_uint128(a), b));
    ci_uint128_write((char*)(frame + 1) + op->alu_imm.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ASR_IMM128);
}
{
    CI_OP_BEGIN(CI_OP_SHL128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    uint32_t b = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    CiUint128 result = ci_uint128_shl(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_SHL128);
}
{
    CI_OP_BEGIN(CI_OP_ASR128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    uint32_t b = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    CiUint128 result = ci_uint128_from_int128(ci_int128_shr(ci_int128_from_uint128(a), b));
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_ASR128);
}
{
    CI_OP_BEGIN(CI_OP_LSR128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    uint32_t b = (uint32_t)ci_read_uint((char*)(frame + 1) + op->alu.src2, 4);
    CiUint128 result = ci_uint128_shr(a, b);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_LSR128);
}
{
    CI_OP_BEGIN(CI_OP_NEG128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    CiUint128 result = ci_uint128_sub(ci_uint128_from_uint64(0), a);
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_NEG128);
}
{
    CI_OP_BEGIN(CI_OP_NOT128);
    CiUint128 a;
    ci_uint128_read(&a, (char*)(frame + 1) + op->alu.src, 16);
    CiUint128 result = ci_uint128_xor(a, ci_uint128_from_int64(-1));
    ci_uint128_write((char*)(frame + 1) + op->alu.slot, 16, result);
    pc++;
    CI_OP_END(CI_OP_NOT128);
}
{
    CI_OP_BEGIN(CI_OP_EQ32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a == b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_EQ32);
}
{
    CI_OP_BEGIN(CI_OP_NE32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a != b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_NE32);
}
{
    CI_OP_BEGIN(CI_OP_SLT32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a < (int32_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLT32);
}
{
    CI_OP_BEGIN(CI_OP_ULT32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a < b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULT32);
}
{
    CI_OP_BEGIN(CI_OP_SGT32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a > (int32_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGT32);
}
{
    CI_OP_BEGIN(CI_OP_UGT32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a > b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGT32);
}
{
    CI_OP_BEGIN(CI_OP_SLE32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a <= (int32_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLE32);
}
{
    CI_OP_BEGIN(CI_OP_ULE32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a <= b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULE32);
}
{
    CI_OP_BEGIN(CI_OP_SGE32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a >= (int32_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGE32);
}
{
    CI_OP_BEGIN(CI_OP_UGE32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a >= b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGE32);
}
{
    CI_OP_BEGIN(CI_OP_EQ_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a == b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_EQ_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_NE_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a != b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_NE_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_SLT_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a < (int32_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SLT_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_ULT_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a < b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_ULT_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_SGT_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a > (int32_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SGT_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_UGT_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a > b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_UGT_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_SLE_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a <= (int32_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SLE_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_ULE_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a <= b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_ULE_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_SGE_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int32_t)a >= (int32_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SGE_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_UGE_JUMP32);
    uint32_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a >= b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_UGE_JUMP32);
}
{
    CI_OP_BEGIN(CI_OP_EQ64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a == b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_EQ64);
}
{
    CI_OP_BEGIN(CI_OP_NE64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a != b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_NE64);
}
{
    CI_OP_BEGIN(CI_OP_SLT64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a < (int64_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLT64);
}
{
    CI_OP_BEGIN(CI_OP_ULT64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a < b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULT64);
}
{
    CI_OP_BEGIN(CI_OP_SGT64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a > (int64_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGT64);
}
{
    CI_OP_BEGIN(CI_OP_UGT64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a > b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGT64);
}
{
    CI_OP_BEGIN(CI_OP_SLE64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a <= (int64_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLE64);
}
{
    CI_OP_BEGIN(CI_OP_ULE64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a <= b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULE64);
}
{
    CI_OP_BEGIN(CI_OP_SGE64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a >= (int64_t)b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGE64);
}
{
    CI_OP_BEGIN(CI_OP_UGE64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a >= b;
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGE64);
}
{
    CI_OP_BEGIN(CI_OP_EQ_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a == b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_EQ_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_NE_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a != b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_NE_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_SLT_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a < (int64_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SLT_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_ULT_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a < b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_ULT_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_SGT_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a > (int64_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SGT_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_UGT_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a > b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_UGT_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_SLE_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a <= (int64_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SLE_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_ULE_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a <= b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_ULE_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_SGE_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = (int64_t)a >= (int64_t)b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_SGE_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_UGE_JUMP64);
    uint64_t a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->cmp.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->cmp.src2, sizeof b);
    _Bool result = a >= b;
    pc = result ? (pc + (op->cmp_jump.jump)) : pc + 1;
    CI_OP_END(CI_OP_UGE_JUMP64);
}
{
    CI_OP_BEGIN(CI_OP_EQ128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_eq(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_EQ128);
}
{
    CI_OP_BEGIN(CI_OP_NE128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_ne(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_NE128);
}
{
    CI_OP_BEGIN(CI_OP_SLT128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_int128_lt(ci_int128_from_uint128(a), ci_int128_from_uint128(b));
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLT128);
}
{
    CI_OP_BEGIN(CI_OP_ULT128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_lt(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULT128);
}
{
    CI_OP_BEGIN(CI_OP_SGT128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_int128_gt(ci_int128_from_uint128(a), ci_int128_from_uint128(b));
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGT128);
}
{
    CI_OP_BEGIN(CI_OP_UGT128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_gt(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGT128);
}
{
    CI_OP_BEGIN(CI_OP_SLE128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_int128_le(ci_int128_from_uint128(a), ci_int128_from_uint128(b));
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SLE128);
}
{
    CI_OP_BEGIN(CI_OP_ULE128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_le(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_ULE128);
}
{
    CI_OP_BEGIN(CI_OP_SGE128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_int128_ge(ci_int128_from_uint128(a), ci_int128_from_uint128(b));
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_SGE128);
}
{
    CI_OP_BEGIN(CI_OP_UGE128);
    CiUint128 a, b;
    ci_uint128_read(&a, (char*)(frame + 1) + op->cmp.src, 16);
    ci_uint128_read(&b, (char*)(frame + 1) + op->cmp.src2, 16);
    _Bool result = ci_uint128_ge(a, b);
    *((unsigned char*)(frame + 1) + op->cmp.slot) = (unsigned char)result;
    pc++;
    CI_OP_END(CI_OP_UGE128);
}
{
    CI_OP_BEGIN(CI_OP_FADD16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_add(a, b, 0);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FADD16);
}
{
    CI_OP_BEGIN(CI_OP_FSUB16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_sub(a, b, 0);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FSUB16);
}
{
    CI_OP_BEGIN(CI_OP_FMUL16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_mul(a, b, 0);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FMUL16);
}
{
    CI_OP_BEGIN(CI_OP_FDIV16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_div(a, b, 0);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FDIV16);
}
{
    CI_OP_BEGIN(CI_OP_FNEG16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t r = ci_float16_neg(a);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FNEG16);
}
{
    CI_OP_BEGIN(CI_OP_FADD_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_add(a, b, 1);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FADD_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FSUB_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_sub(a, b, 1);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FSUB_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FMUL_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_mul(a, b, 1);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FMUL_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FDIV_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src2, 2);
    uint16_t r = ci_float16_div(a, b, 1);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FDIV_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FNEG_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->falu32.src, 2);
    uint16_t r = ci_float16_neg(a);
    ci_write_uint((char*)(frame + 1) + op->falu32.slot, 2, r);
    pc++;
    CI_OP_END(CI_OP_FNEG_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FADD32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu32.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu32.slot;
    float res = a + b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FADD32);
}
{
    CI_OP_BEGIN(CI_OP_FSUB32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu32.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu32.slot;
    float res = a - b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FSUB32);
}
{
    CI_OP_BEGIN(CI_OP_FMUL32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu32.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu32.slot;
    float res = a * b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FMUL32);
}
{
    CI_OP_BEGIN(CI_OP_FDIV32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu32.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu32.slot;
    float res = a / b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FDIV32);
}
{
    CI_OP_BEGIN(CI_OP_FNEG32);
    float a;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu32.src, sizeof a);
    void* dest = (char*)(frame + 1) + op->falu32.slot;
    float res = -a;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FNEG32);
}
{
    CI_OP_BEGIN(CI_OP_FADD64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu64.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu64.slot;
    double res = a + b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FADD64);
}
{
    CI_OP_BEGIN(CI_OP_FSUB64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu64.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu64.slot;
    double res = a - b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FSUB64);
}
{
    CI_OP_BEGIN(CI_OP_FMUL64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu64.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu64.slot;
    double res = a * b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FMUL64);
}
{
    CI_OP_BEGIN(CI_OP_FDIV64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->falu64.src2, sizeof b);
    void* dest = (char*)(frame + 1) + op->falu64.slot;
    double res = a / b;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FDIV64);
}
{
    CI_OP_BEGIN(CI_OP_FNEG64);
    double a;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->falu64.src, sizeof a);
    void* dest = (char*)(frame + 1) + op->falu64.slot;
    double res = -a;
    CI_INLINE_MEMCPY(dest, &res, sizeof res);
    pc++;
    CI_OP_END(CI_OP_FNEG64);
}
{
    CI_OP_BEGIN(CI_OP_FADD80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->falu32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat80 r = ci_float80_add(a, b);
    ci_float80_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FADD80);
}
{
    CI_OP_BEGIN(CI_OP_FSUB80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->falu32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat80 r = ci_float80_sub(a, b);
    ci_float80_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FSUB80);
}
{
    CI_OP_BEGIN(CI_OP_FMUL80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->falu32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat80 r = ci_float80_mul(a, b);
    ci_float80_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FMUL80);
}
{
    CI_OP_BEGIN(CI_OP_FDIV80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->falu32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat80 r = ci_float80_div(a, b);
    ci_float80_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FDIV80);
}
{
    CI_OP_BEGIN(CI_OP_FNEG80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->falu32.src);
    CiFloat80 r = ci_float80_neg(a);
    ci_float80_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FNEG80);
}
{
    CI_OP_BEGIN(CI_OP_FADD128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->falu32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat128 r = ci_float128_add(a, b);
    ci_float128_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FADD128);
}
{
    CI_OP_BEGIN(CI_OP_FSUB128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->falu32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat128 r = ci_float128_sub(a, b);
    ci_float128_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FSUB128);
}
{
    CI_OP_BEGIN(CI_OP_FMUL128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->falu32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat128 r = ci_float128_mul(a, b);
    ci_float128_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FMUL128);
}
{
    CI_OP_BEGIN(CI_OP_FDIV128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->falu32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->falu32.src2);
    CiFloat128 r = ci_float128_div(a, b);
    ci_float128_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FDIV128);
}
{
    CI_OP_BEGIN(CI_OP_FNEG128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->falu32.src);
    CiFloat128 r = ci_float128_neg(a);
    ci_float128_write((char*)(frame + 1) + op->falu32.slot, op->falu32.slot_size, r);
    pc++;
    CI_OP_END(CI_OP_FNEG128);
}
{
    CI_OP_BEGIN(CI_OP_FEQ16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_eq(a, b, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FEQ16);
}
{
    CI_OP_BEGIN(CI_OP_FNE16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = !ci_float16_eq(a, b, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FNE16);
}
{
    CI_OP_BEGIN(CI_OP_FLT16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_lt(a, b, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLT16);
}
{
    CI_OP_BEGIN(CI_OP_FGT16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_lt(b, a, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGT16);
}
{
    CI_OP_BEGIN(CI_OP_FLE16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_le(a, b, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLE16);
}
{
    CI_OP_BEGIN(CI_OP_FGE16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_le(b, a, 0);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGE16);
}
{
    CI_OP_BEGIN(CI_OP_FEQ_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_eq(a, b, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FEQ_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FNE_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = !ci_float16_eq(a, b, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FNE_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FLT_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_lt(a, b, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLT_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FGT_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_lt(b, a, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGT_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FLE_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_le(a, b, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLE_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FGE_BF16);
    uint16_t a = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src, 2);
    uint16_t b = (uint16_t)ci_read_uint((char*)(frame + 1) + op->fcmp32.src2, 2);
    _Bool r = ci_float16_le(b, a, 1);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGE_BF16);
}
{
    CI_OP_BEGIN(CI_OP_FEQ32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a == b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FEQ32);
}
{
    CI_OP_BEGIN(CI_OP_FNE32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a != b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FNE32);
}
{
    CI_OP_BEGIN(CI_OP_FLT32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a < b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FLT32);
}
{
    CI_OP_BEGIN(CI_OP_FGT32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a > b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FGT32);
}
{
    CI_OP_BEGIN(CI_OP_FLE32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a <= b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FLE32);
}
{
    CI_OP_BEGIN(CI_OP_FGE32);
    float a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp32.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp32.src2, sizeof b);
    uint64_t res = a >= b;
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FGE32);
}
{
    CI_OP_BEGIN(CI_OP_FEQ64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a == b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FEQ64);
}
{
    CI_OP_BEGIN(CI_OP_FNE64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a != b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FNE64);
}
{
    CI_OP_BEGIN(CI_OP_FLT64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a < b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FLT64);
}
{
    CI_OP_BEGIN(CI_OP_FGT64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a > b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FGT64);
}
{
    CI_OP_BEGIN(CI_OP_FLE64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a <= b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FLE64);
}
{
    CI_OP_BEGIN(CI_OP_FGE64);
    double a, b;
    CI_INLINE_MEMCPY(&a, (char*)(frame + 1) + op->fcmp64.src, sizeof a);
    CI_INLINE_MEMCPY(&b, (char*)(frame + 1) + op->fcmp64.src2, sizeof b);
    uint64_t res = a >= b;
    *((unsigned char*)(frame + 1) + op->fcmp64.slot) = (unsigned char)res;
    pc++;
    CI_OP_END(CI_OP_FGE64);
}
{
    CI_OP_BEGIN(CI_OP_FEQ80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float80_eq(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FEQ80);
}
{
    CI_OP_BEGIN(CI_OP_FNE80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = !ci_float80_eq(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FNE80);
}
{
    CI_OP_BEGIN(CI_OP_FLT80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float80_lt(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLT80);
}
{
    CI_OP_BEGIN(CI_OP_FLE80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float80_le(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLE80);
}
{
    CI_OP_BEGIN(CI_OP_FGT80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float80_lt(b, a);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGT80);
}
{
    CI_OP_BEGIN(CI_OP_FGE80);
    CiFloat80 a = ci_float80_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat80 b = ci_float80_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float80_le(b, a);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGE80);
}
{
    CI_OP_BEGIN(CI_OP_FEQ128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float128_eq(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FEQ128);
}
{
    CI_OP_BEGIN(CI_OP_FNE128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = !ci_float128_eq(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FNE128);
}
{
    CI_OP_BEGIN(CI_OP_FLT128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float128_lt(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLT128);
}
{
    CI_OP_BEGIN(CI_OP_FLE128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float128_le(a, b);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FLE128);
}
{
    CI_OP_BEGIN(CI_OP_FGT128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float128_lt(b, a);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGT128);
}
{
    CI_OP_BEGIN(CI_OP_FGE128);
    CiFloat128 a = ci_float128_read((char*)(frame + 1) + op->fcmp32.src);
    CiFloat128 b = ci_float128_read((char*)(frame + 1) + op->fcmp32.src2);
    _Bool r = ci_float128_le(b, a);
    *((unsigned char*)(frame + 1) + op->fcmp32.slot) = (unsigned char)r;
    pc++;
    CI_OP_END(CI_OP_FGE128);
}


{
    CI_OP_BEGIN(CI_OP_PROPAGATE_ERROR);
    int err;
    CI_INLINE_MEMCPY(&err, (char*)(frame + 1) + op->propagate_error.src, sizeof err);
    if(err) CI_OP_ERROR(err);
    pc++;
    CI_OP_END(CI_OP_PROPAGATE_ERROR);
}
{
    CI_OP_BEGIN(CI_OP_CHECK);
    if(!*((unsigned char*)(frame + 1) + op->check.src))
        CI_OP_ERROR(ci_error(ci, op->loc, "%s", op->check.message));
    pc++;
    CI_OP_END(CI_OP_CHECK);
}
{
    CI_OP_BEGIN(CI_OP_SLOT_ADDR);
    void* addr = (char*)(frame + 1) + op->slot_addr.src;
    CI_INLINE_MEMCPY((char*)(frame + 1) + op->slot_addr.slot, &addr, sizeof addr);
    pc++;
    CI_OP_END(CI_OP_SLOT_ADDR);
}
{
    CI_OP_BEGIN(CI_OP_VAR_ADDR);
    CcVariable* var = op->var_addr.var;
    int err = ci_ensure_var_storage(ci, var);
    if(err) CI_OP_ERROR(err);
    CI_INLINE_MEMCPY((char*)(frame + 1) + op->var_addr.slot, &var->interp_val, sizeof(void*));
    pc++;
    CI_OP_END(CI_OP_VAR_ADDR);
}
{
    CI_OP_BEGIN(CI_OP_TLS_ADDR);
    void* address;
    int err = ci_tls_address(ci, op->var_addr.var, &address);
    if(err) CI_OP_ERROR(err);
    CI_INLINE_MEMCPY((char*)(frame + 1) + op->var_addr.slot, &address, sizeof address);
    pc++;
    CI_OP_END(CI_OP_TLS_ADDR);
}
{
    CI_OP_BEGIN(CI_OP_FUNC_ADDR);
    CcFunc* func = op->func_addr.func;
    if(!func->native_func)
        CI_OP_ERROR(ci_ice(ci, op->loc, "function '%s' not resolved before execution", func->name ? func->name->data : "<unknown>"));
    void (*fn)(void) = func->native_func;
    CI_INLINE_MEMCPY((char*)(frame + 1) + op->func_addr.slot, &fn, sizeof fn);
    pc++;
    CI_OP_END(CI_OP_FUNC_ADDR);
}
{
    CI_OP_BEGIN(CI_OP_JUMP);
    pc = (pc + (op->jump.jump));
    CI_OP_END(CI_OP_JUMP);
}

{
    CI_OP_BEGIN(CI_OP_RETURN);
    pc = frame->code->op_count;
    CI_OP_END(CI_OP_RETURN);
}
{
    CI_OP_BEGIN(CI_OP_FRAME_RESTORE);
    pc = frame->code->op_count;
    CI_OP_END(CI_OP_FRAME_RESTORE);
}
{
    CI_OP_BEGIN(CI_OP_RETURN_SLOT);
    void* destination = root_result;
    size_t capacity = root_result_size;
    if(frame != root){
        CiInterpFrame* parent = frame->parent;
        const CiOp* call = &parent->code->ops[parent->pc];
        capacity = call->call.ret_size;
        destination = capacity ? (char*)(parent + 1) + call->call.ret_slot : NULL;
    }
    if(destination){
        if(op->return_slot.src_size > capacity)
            CI_OP_ERROR(CI_RESULT_TOO_SMALL(ci, op->loc, op->return_slot.src_size, capacity));
        memcpy(destination, (char*)(frame + 1) + op->return_slot.src, op->return_slot.src_size);
    }
    pc = frame->code->op_count;
    CI_OP_END(CI_OP_RETURN_SLOT);
}
{
    CI_OP_BEGIN(CI_OP_CLEAR_CACHE);
    char *begin, *end;
    CI_INLINE_MEMCPY(&begin, (char*)(frame + 1) + op->clear_cache.begin, sizeof begin);
    CI_INLINE_MEMCPY(&end, (char*)(frame + 1) + op->clear_cache.end, sizeof end);
    #ifdef _WIN32
    if(!FlushInstructionCache(GetCurrentProcess(), begin, (SIZE_T)((uintptr_t)end - (uintptr_t)begin)))
        CI_OP_ERROR(ci_error(ci, op->loc, "__builtin___clear_cache failed"));
    #else
    __builtin___clear_cache(begin, end);
    #endif
    pc++;
    CI_OP_END(CI_OP_CLEAR_CACHE);
}
{
    CI_OP_BEGIN(CI_OP_STACK_SAVE);
    int err = ci_stack_save(stack, 0);
    if(err) CI_OP_ERROR(err);
    pc++;
    CI_OP_END(CI_OP_STACK_SAVE);
}
{
    CI_OP_BEGIN(CI_OP_STACK_RESTORE);
    ci_stack_restore(stack);
    pc++;
    CI_OP_END(CI_OP_STACK_RESTORE);
}


{
    CI_OP_BEGIN(CI_OP_FRAME_SAVE);
    int err = ci_stack_save(stack, 1);
    if(err) CI_OP_ERROR(err);
    op++;
    goto execute_op;
    CI_OP_END(CI_OP_FRAME_SAVE);
}
{
    CI_OP_BEGIN(CI_OP_FRAME_ENTER);
    const CiCode* code = (const CiCode*)((const char*)(op - 1) - offsetof(CiCode, ops));
    CiInterpFrame* child = NULL;
    int err = ci_alloc_frame(ci, stack, frame, code->frame_size, (uint32_t)code->alignment, &child);
    if(err){
        ci_stack_unwind_frame(stack);
        CI_OP_ERROR(err);
    }
    child->code = code;
    uint32_t incoming_slot = op->frame_enter.incoming_slot;
    child->incoming = incoming_slot == UINT32_MAX ? NULL : (NcCallState*)((char*)(child + 1) + incoming_slot);
    child->pc = 2;
    CI_OP_ENTER(child);
    CI_OP_END(CI_OP_FRAME_ENTER);
}
{
    CI_OP_BEGIN(CI_OP_FRAME_READY);
    pc++;
    CI_OP_END(CI_OP_FRAME_READY);
}

#undef CI_OP_BEGIN
#undef CI_OP_END
#undef CI_OP_ENTER
#undef CI_OP_CALL
#undef CI_OP_ERROR
#undef CI_OP_SYNC_PC

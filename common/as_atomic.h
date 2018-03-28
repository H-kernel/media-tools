#ifndef __ATOMIC_H__
#define __ATOMIC_H__

/*lint --e{528} -e950 -e40 -e522 -e10 -e818 -e830*/

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v)        (*&v)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v,i)        ((*&v) = (i))

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1.
 */
static __inline__ void atomic_inc(volatile uint32_t *v)
{
    __asm__ __volatile__(
            "lock ; " "incl %0"
            :"=m" (*v)
            :"m" (*v)
            : "memory");
}


/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.
 */
static __inline__ void atomic_dec(volatile uint32_t *v)
{
    __asm__ __volatile__(
            "lock ; " "decl %0"
            :"=m" (*v)
            :"m" (*v)
            : "memory");
}



/**
 * compare_and_swap
 * @p: pointer of type atomic_t
 * @val_old: old values for p->countor
 * @val_new: new valuds for p->countor
 *
 * compare p->countor with val_old, if equal, set p->countor = val_new and return true;
 * else return false.
 */

static __inline__ bool compare_and_swap2(volatile uint32_t* pDest,
                                        uint32_t oldValue, uint32_t oldTag,
                                        uint32_t newValue, uint32_t newTag)
{
    register bool ret = 0;
#if defined(__x86_64__)
    __asm__ __volatile__( 
            "lock; cmpxchg8b %1;" 
            "sete %0;" 
            :"=r"(ret),"=m" (*(pDest)) 
            :"a" (oldValue), "d" (oldTag), "b" (newValue), "c" (newTag));
#else
    __asm__ __volatile__("pushl %%ebx;"
                        "movl %%esi, %%ebx;"
                        "lock; cmpxchg8b %1;"
                        "popl %%ebx;"
                        "setz %0;"
                        : "=r"(ret), "=m"(*(pDest))
                        : "a"(oldValue), "d"(oldTag), "S" (newValue), "c"(newTag)
                        : "memory");
#endif

    return ret;
}

static __inline__ bool compare_and_swap(volatile uint32_t *pDest, uint32_t oldValue, uint32_t newValue)
{
        register bool ret = 0;
       __asm__ __volatile__("lock; cmpxchgl %3, %0; setz %1"
                        : "=m"(*(pDest)), "=r"(ret)
                        : "m"(*(pDest)), "r" (newValue), "a"(oldValue)
                        : "memory");
        return ret;
}

/*lint -restore*/
#endif //__ATOMIC_H__

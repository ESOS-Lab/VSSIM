
#include "libcflat.h"
#include "smp.h"

#define true 1
#define false 0

typedef unsigned long pt_element_t;

#define PAGE_SIZE ((pt_element_t)4096)
#define PAGE_MASK (~(PAGE_SIZE-1))

#define PT_BASE_ADDR_MASK ((pt_element_t)((((pt_element_t)1 << 40) - 1) & PAGE_MASK))
#define PT_PSE_BASE_ADDR_MASK (PT_BASE_ADDR_MASK & ~(1ull << 21))

#define PT_PRESENT_MASK    ((pt_element_t)1 << 0)
#define PT_WRITABLE_MASK   ((pt_element_t)1 << 1)
#define PT_USER_MASK       ((pt_element_t)1 << 2)
#define PT_ACCESSED_MASK   ((pt_element_t)1 << 5)
#define PT_DIRTY_MASK      ((pt_element_t)1 << 6)
#define PT_PSE_MASK        ((pt_element_t)1 << 7)
#define PT_NX_MASK         ((pt_element_t)1 << 63)

#define CR0_WP_MASK (1UL << 16)

#define PFERR_PRESENT_MASK (1U << 0)
#define PFERR_WRITE_MASK (1U << 1)
#define PFERR_USER_MASK (1U << 2)
#define PFERR_RESERVED_MASK (1U << 3)
#define PFERR_FETCH_MASK (1U << 4)

#define MSR_EFER 0xc0000080
#define EFER_NX_MASK		(1ull << 11)

/*
 * page table access check tests
 */

enum {
    AC_PTE_PRESENT,
    AC_PTE_WRITABLE,
    AC_PTE_USER,
    AC_PTE_ACCESSED,
    AC_PTE_DIRTY,
    AC_PTE_NX,
    AC_PTE_BIT51,

    AC_PDE_PRESENT,
    AC_PDE_WRITABLE,
    AC_PDE_USER,
    AC_PDE_ACCESSED,
    AC_PDE_DIRTY,
    AC_PDE_PSE,
    AC_PDE_NX,
    AC_PDE_BIT51,

    AC_ACCESS_USER,
    AC_ACCESS_WRITE,
    AC_ACCESS_FETCH,
    AC_ACCESS_TWICE,
    // AC_ACCESS_PTE,

    AC_CPU_EFER_NX,
    AC_CPU_CR0_WP,

    NR_AC_FLAGS
};

const char *ac_names[] = {
    [AC_PTE_PRESENT] = "pte.p",
    [AC_PTE_ACCESSED] = "pte.a",
    [AC_PTE_WRITABLE] = "pte.rw",
    [AC_PTE_USER] = "pte.user",
    [AC_PTE_DIRTY] = "pte.d",
    [AC_PTE_NX] = "pte.nx",
    [AC_PTE_BIT51] = "pte.51",
    [AC_PDE_PRESENT] = "pde.p",
    [AC_PDE_ACCESSED] = "pde.a",
    [AC_PDE_WRITABLE] = "pde.rw",
    [AC_PDE_USER] = "pde.user",
    [AC_PDE_DIRTY] = "pde.d",
    [AC_PDE_PSE] = "pde.pse",
    [AC_PDE_NX] = "pde.nx",
    [AC_PDE_BIT51] = "pde.51",
    [AC_ACCESS_WRITE] = "write",
    [AC_ACCESS_USER] = "user",
    [AC_ACCESS_FETCH] = "fetch",
    [AC_ACCESS_TWICE] = "twice",
    [AC_CPU_EFER_NX] = "efer.nx",
    [AC_CPU_CR0_WP] = "cr0.wp",
};

static inline void *va(pt_element_t phys)
{
    return (void *)phys;
}

static unsigned long read_cr0()
{
    unsigned long cr0;

    asm volatile ("mov %%cr0, %0" : "=r"(cr0));

    return cr0;
}

static void write_cr0(unsigned long cr0)
{
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));
}

typedef struct {
    unsigned short offset0;
    unsigned short selector;
    unsigned short ist : 3;
    unsigned short : 5;
    unsigned short type : 4;
    unsigned short : 1;
    unsigned short dpl : 2;
    unsigned short p : 1;
    unsigned short offset1;
    unsigned offset2;
    unsigned reserved;
} idt_entry_t;

typedef struct {
    unsigned flags[NR_AC_FLAGS];
    void *virt;
    pt_element_t phys;
    pt_element_t pt_pool;
    unsigned pt_pool_size;
    unsigned pt_pool_current;
    pt_element_t *ptep;
    pt_element_t expected_pte;
    pt_element_t *pdep;
    pt_element_t expected_pde;
    int expected_fault;
    unsigned expected_error;
    idt_entry_t idt[256];
} ac_test_t;

typedef struct {
    unsigned short limit;
    unsigned long linear_addr;
} __attribute__((packed)) descriptor_table_t;

void lidt(idt_entry_t *idt, int nentries)
{
    descriptor_table_t dt;
    
    dt.limit = nentries * sizeof(*idt) - 1;
    dt.linear_addr = (unsigned long)idt;
    asm volatile ("lidt %0" : : "m"(dt));
}

void memset(void *a, unsigned char v, int n)
{
    unsigned char *x = a;

    while (n--)
	*x++ = v;
}

unsigned short read_cs()
{
    unsigned short r;

    asm volatile ("mov %%cs, %0" : "=r"(r));
    return r;
}

unsigned long long rdmsr(unsigned index)
{
    unsigned a, d;

    asm volatile("rdmsr" : "=a"(a), "=d"(d) : "c"(index));
    return ((unsigned long long)d << 32) | a;
}

void wrmsr(unsigned index, unsigned long long val)
{
    unsigned a = val, d = val >> 32;

    asm volatile("wrmsr" : : "a"(a), "d"(d), "c"(index));
}

void set_idt_entry(idt_entry_t *e, void *addr, int dpl)
{
    memset(e, 0, sizeof *e);
    e->offset0 = (unsigned long)addr;
    e->selector = read_cs();
    e->ist = 0;
    e->type = 14;
    e->dpl = dpl;
    e->p = 1;
    e->offset1 = (unsigned long)addr >> 16;
    e->offset2 = (unsigned long)addr >> 32;
}

void set_cr0_wp(int wp)
{
    unsigned long cr0 = read_cr0();

    cr0 &= ~CR0_WP_MASK;
    if (wp)
	cr0 |= CR0_WP_MASK;
    write_cr0(cr0);
}

void set_efer_nx(int nx)
{
    unsigned long long efer;

    efer = rdmsr(MSR_EFER);
    efer &= ~EFER_NX_MASK;
    if (nx)
	efer |= EFER_NX_MASK;
    wrmsr(MSR_EFER, efer);
}


void ac_test_init(ac_test_t *at)
{
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_NX_MASK);
    set_cr0_wp(1);
    for (int i = 0; i < NR_AC_FLAGS; ++i)
	at->flags[i] = 0;
    at->virt = (void *)(0x123400000000 + 16 * smp_id());
    at->phys = 32 * 1024 * 1024;
    at->pt_pool = 33 * 1024 * 1024;
    at->pt_pool_size = 120 * 1024 * 1024 - at->pt_pool;
    at->pt_pool_current = 0;
    memset(at->idt, 0, sizeof at->idt);
    lidt(at->idt, 256);
    extern char page_fault, kernel_entry;
    set_idt_entry(&at->idt[14], &page_fault, 0);
    set_idt_entry(&at->idt[0x20], &kernel_entry, 3);
}

int ac_test_bump_one(ac_test_t *at)
{
    for (int i = 0; i < NR_AC_FLAGS; ++i)
	if (!at->flags[i]) {
	    at->flags[i] = 1;
	    return 1;
	} else
	    at->flags[i] = 0;
    return 0;
}

_Bool ac_test_legal(ac_test_t *at)
{
    if (at->flags[AC_ACCESS_FETCH] && at->flags[AC_ACCESS_WRITE])
	return false;
    return true;
}

int ac_test_bump(ac_test_t *at)
{
    int ret;

    ret = ac_test_bump_one(at);
    while (ret && !ac_test_legal(at))
	ret = ac_test_bump_one(at);
    return ret;
}

unsigned long read_cr3()
{
    unsigned long cr3;

    asm volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void invlpg(void *addr)
{
    asm volatile ("invlpg (%0)" : : "r"(addr));
}

pt_element_t ac_test_alloc_pt(ac_test_t *at)
{
    pt_element_t ret = at->pt_pool + at->pt_pool_current;
    at->pt_pool_current += PAGE_SIZE;
    return ret;
}

_Bool ac_test_enough_room(ac_test_t *at)
{
    return at->pt_pool_current + 4 * PAGE_SIZE <= at->pt_pool_size;
}

void ac_test_reset_pt_pool(ac_test_t *at)
{
    at->pt_pool_current = 0;
}

void ac_test_setup_pte(ac_test_t *at)
{
    unsigned long root = read_cr3();
    int pde_valid, pte_valid;

    if (!ac_test_enough_room(at))
	ac_test_reset_pt_pool(at);

    at->ptep = 0;
    for (int i = 4; i >= 1 && (i >= 2 || !at->flags[AC_PDE_PSE]); --i) {
	pt_element_t *vroot = va(root & PT_BASE_ADDR_MASK);
	unsigned index = ((unsigned long)at->virt >> (12 + (i-1) * 9)) & 511;
	pt_element_t pte = 0;
	switch (i) {
	case 4:
	case 3:
	    pte = vroot[index];
	    pte = ac_test_alloc_pt(at) | PT_PRESENT_MASK;
	    pte |= PT_WRITABLE_MASK | PT_USER_MASK;
	    break;
	case 2:
	    if (!at->flags[AC_PDE_PSE])
		pte = ac_test_alloc_pt(at);
	    else {
		pte = at->phys & PT_PSE_BASE_ADDR_MASK;
		pte |= PT_PSE_MASK;
	    }
	    if (at->flags[AC_PDE_PRESENT])
		pte |= PT_PRESENT_MASK;
	    if (at->flags[AC_PDE_WRITABLE])
		pte |= PT_WRITABLE_MASK;
	    if (at->flags[AC_PDE_USER])
		pte |= PT_USER_MASK;
	    if (at->flags[AC_PDE_ACCESSED])
		pte |= PT_ACCESSED_MASK;
	    if (at->flags[AC_PDE_DIRTY])
		pte |= PT_DIRTY_MASK;
	    if (at->flags[AC_PDE_NX])
		pte |= PT_NX_MASK;
	    if (at->flags[AC_PDE_BIT51])
		pte |= 1ull << 51;
	    at->pdep = &vroot[index];
	    break;
	case 1:
	    pte = at->phys & PT_BASE_ADDR_MASK;
	    if (at->flags[AC_PTE_PRESENT])
		pte |= PT_PRESENT_MASK;
	    if (at->flags[AC_PTE_WRITABLE])
		pte |= PT_WRITABLE_MASK;
	    if (at->flags[AC_PTE_USER])
		pte |= PT_USER_MASK;
	    if (at->flags[AC_PTE_ACCESSED])
		pte |= PT_ACCESSED_MASK;
	    if (at->flags[AC_PTE_DIRTY])
		pte |= PT_DIRTY_MASK;
	    if (at->flags[AC_PTE_NX])
		pte |= PT_NX_MASK;
	    if (at->flags[AC_PTE_BIT51])
		pte |= 1ull << 51;
	    at->ptep = &vroot[index];
	    break;
	}
	vroot[index] = pte;
	root = vroot[index];
    }
    invlpg(at->virt);
    if (at->ptep)
	at->expected_pte = *at->ptep;
    at->expected_pde = *at->pdep;
    at->expected_fault = 0;
    at->expected_error = PFERR_PRESENT_MASK;

    pde_valid = at->flags[AC_PDE_PRESENT]
        && !at->flags[AC_PDE_BIT51]
        && !(at->flags[AC_PDE_NX] && !at->flags[AC_CPU_EFER_NX]);
    pte_valid = pde_valid
        && at->flags[AC_PTE_PRESENT]
        && !at->flags[AC_PTE_BIT51]
        && !(at->flags[AC_PTE_NX] && !at->flags[AC_CPU_EFER_NX]);
    if (at->flags[AC_ACCESS_TWICE]) {
	if (pde_valid) {
	    at->expected_pde |= PT_ACCESSED_MASK;
	    if (pte_valid)
		at->expected_pte |= PT_ACCESSED_MASK;
	}
    }

    if (at->flags[AC_ACCESS_USER])
	at->expected_error |= PFERR_USER_MASK;

    if (at->flags[AC_ACCESS_WRITE])
	at->expected_error |= PFERR_WRITE_MASK;

    if (at->flags[AC_ACCESS_FETCH])
	at->expected_error |= PFERR_FETCH_MASK;

    if (!at->flags[AC_PDE_PRESENT]) {
	at->expected_fault = 1;
	at->expected_error &= ~PFERR_PRESENT_MASK;
    } else if (!pde_valid) {
        at->expected_fault = 1;
        at->expected_error |= PFERR_RESERVED_MASK;
    }

    if (at->flags[AC_ACCESS_USER] && !at->flags[AC_PDE_USER])
	at->expected_fault = 1;

    if (at->flags[AC_ACCESS_WRITE]
	&& !at->flags[AC_PDE_WRITABLE]
	&& (at->flags[AC_CPU_CR0_WP] || at->flags[AC_ACCESS_USER]))
	at->expected_fault = 1;

    if (at->flags[AC_ACCESS_FETCH] && at->flags[AC_PDE_NX])
	at->expected_fault = 1;

    if (at->expected_fault)
	goto fault;

    at->expected_pde |= PT_ACCESSED_MASK;

    if (at->flags[AC_PDE_PSE]) {
	if (at->flags[AC_ACCESS_WRITE])
	    at->expected_pde |= PT_DIRTY_MASK;
	goto no_pte;
    }

    if (!at->flags[AC_PTE_PRESENT]) {
	at->expected_fault = 1;
	at->expected_error &= ~PFERR_PRESENT_MASK;
    } else if (!pte_valid) {
        at->expected_fault = 1;
        at->expected_error |= PFERR_RESERVED_MASK;
    }

    if (at->flags[AC_ACCESS_USER] && !at->flags[AC_PTE_USER])
	at->expected_fault = 1;

    if (at->flags[AC_ACCESS_WRITE]
	&& !at->flags[AC_PTE_WRITABLE]
	&& (at->flags[AC_CPU_CR0_WP] || at->flags[AC_ACCESS_USER]))
	at->expected_fault = 1;

    if (at->flags[AC_ACCESS_FETCH] && at->flags[AC_PTE_NX])
	at->expected_fault = 1;

    if (at->expected_fault)
	goto fault;

    at->expected_pte |= PT_ACCESSED_MASK;
    if (at->flags[AC_ACCESS_WRITE])
	at->expected_pte |= PT_DIRTY_MASK;

no_pte:
fault:
    ;
}

int ac_test_do_access(ac_test_t *at)
{
    static unsigned unique = 42;
    int fault = 0;
    unsigned e;
    static unsigned char user_stack[4096];
    unsigned long rsp;

    ++unique;

    *((unsigned char *)at->phys) = 0xc3; /* ret */

    unsigned r = unique;
    set_cr0_wp(at->flags[AC_CPU_CR0_WP]);
    set_efer_nx(at->flags[AC_CPU_EFER_NX]);

    if (at->flags[AC_ACCESS_TWICE]) {
	asm volatile (
	    "mov $fixed2, %%rsi \n\t"
	    "mov (%[addr]), %[reg] \n\t"
	    "fixed2:"
	    : [reg]"=r"(r), [fault]"=a"(fault), "=b"(e)
	    : [addr]"r"(at->virt)
	    : "rsi"
	    );
	fault = 0;
    }

    asm volatile ("mov $fixed1, %%rsi \n\t"
		  "mov %%rsp, %%rdx \n\t"
		  "cmp $0, %[user] \n\t"
		  "jz do_access \n\t"
		  "push %%rax; mov %[user_ds], %%ax; mov %%ax, %%ds; pop %%rax  \n\t"
		  "pushq %[user_ds] \n\t"
		  "pushq %[user_stack_top] \n\t"
		  "pushfq \n\t"
		  "pushq %[user_cs] \n\t"
		  "pushq $do_access \n\t"
		  "iretq \n"
		  "do_access: \n\t"
		  "cmp $0, %[fetch] \n\t"
		  "jnz 2f \n\t"
		  "cmp $0, %[write] \n\t"
		  "jnz 1f \n\t"
		  "mov (%[addr]), %[reg] \n\t"
		  "jmp done \n\t"
		  "1: mov %[reg], (%[addr]) \n\t"
		  "jmp done \n\t"
		  "2: call *%[addr] \n\t"
		  "done: \n"
		  "fixed1: \n"
		  "int %[kernel_entry_vector] \n\t"
		  "back_to_kernel:"
		  : [reg]"+r"(r), "+a"(fault), "=b"(e), "=&d"(rsp)
		  : [addr]"r"(at->virt),
		    [write]"r"(at->flags[AC_ACCESS_WRITE]),
		    [user]"r"(at->flags[AC_ACCESS_USER]),
		    [fetch]"r"(at->flags[AC_ACCESS_FETCH]),
		    [user_ds]"i"(32+3),
		    [user_cs]"i"(24+3),
		    [user_stack_top]"r"(user_stack + sizeof user_stack),
		    [kernel_entry_vector]"i"(0x20)
		  : "rsi");

    asm volatile (".section .text.pf \n\t"
		  "page_fault: \n\t"
		  "pop %rbx \n\t"
		  "mov %rsi, (%rsp) \n\t"
		  "movl $1, %eax \n\t"
		  "iretq \n\t"
		  ".section .text");

    asm volatile (".section .text.entry \n\t"
		  "kernel_entry: \n\t"
		  "mov %rdx, %rsp \n\t"
		  "jmp back_to_kernel \n\t"
		  ".section .text");

    if (fault && !at->expected_fault) {
	printf("FAIL: unexpected fault\n");
	return 0;
    }
    if (!fault && at->expected_fault) {
	printf("FAIL: unexpected access\n");
	return 0;
    }
    if (fault && e != at->expected_error) {
	printf("FAIL: error code %x expected %x\n", e, at->expected_error);
	return 0;
    }
    if (at->ptep && *at->ptep != at->expected_pte) {
	printf("FAIL: pte %x expected %x\n", *at->ptep, at->expected_pte);
	return 0;
    }

    if (*at->pdep != at->expected_pde) {
	printf("FAIL: pde %x expected %x\n", *at->pdep, at->expected_pde);
	return 0;
    }

    printf("PASS\n");
    return 1;
}

int ac_test_exec(ac_test_t *at)
{
    int r;
    char line[5000];

    *line = 0;
    strcat(line, "test");
    for (int i = 0; i < NR_AC_FLAGS; ++i)
	if (at->flags[i]) {
	    strcat(line, " ");
	    strcat(line, ac_names[i]);
	}
    strcat(line, ": ");
    printf("%s", line);
    ac_test_setup_pte(at);
    r = ac_test_do_access(at);
    return r;
}

int ac_test_run(void)
{
    static ac_test_t at;
    int tests, successes;

    printf("run\n");
    tests = successes = 0;
    ac_test_init(&at);
    do {
	++tests;
	successes += ac_test_exec(&at);
    } while (ac_test_bump(&at));

    printf("\n%d tests, %d failures\n", tests, tests - successes);

    return successes == tests;
}

int main()
{
    int r;

    printf("starting test\n\n");
    smp_init((void(*)(void))ac_test_run);
    r = ac_test_run();
    return r ? 0 : 1;
}

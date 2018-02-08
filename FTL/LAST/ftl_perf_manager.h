#ifndef _PERF_MANAGER_H_
#define _PERF_MANAGER_H_

void INIT_PERF_CHECKER(void);
void TERM_PERF_CHECKER(void);

void PERF_CHECKER(int op_type, int64_t op_delay);

#endif

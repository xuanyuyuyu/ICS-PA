#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;


void switch_boot_pcb() {
  current = &pcb_boot;
}

void init_proc() {
  Log("Initializing processes...");

  char *nterm_argv[] = {"/bin/nterm", NULL};
  char *hello_argv[] = {"/bin/hello", NULL};
  char *envp[] = {NULL};

  context_uload(&pcb[0], "/bin/nterm", nterm_argv, envp);
  context_uload(&pcb[1], "/bin/hello", hello_argv, envp);

  switch_boot_pcb();
}

Context* schedule(Context *prev) {
  current->cp = prev;

  if(current == &pcb[0]) {
    current = &pcb[1];
  } else {
    current = &pcb[0];
  }

  return current->cp;
}

#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;


void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  const char *name = arg;
  int j = 1;

  while (1) {
      Log("Hello from %s, round %d", name, j++);
     // yield();
    }
}

static void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  pcb->cp = kcontext(RANGE(pcb->stack, pcb->stack+STACK_SIZE),
                  entry,
                  arg);
}

 void init_proc() {
    Log("Initializing processes...");

    char *argv[] = {"/bin/hello", NULL};
    char *envp[] = {NULL};

    // 用户进程
    context_uload(&pcb[0], "/bin/hello", argv, envp);

    // 内核线程
    context_kload(&pcb[1], hello_fun, "kernel");

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

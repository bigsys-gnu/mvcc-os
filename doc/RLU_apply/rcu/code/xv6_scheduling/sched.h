// Per-CPU state
struct cpu {
  uchar apicid;                
  struct context *scheduler;   
  struct taskstate ts;         
  struct segdesc gdt[NSEGS];   
  volatile uint started;       
  int ncli;                    
  int intena;                  
  struct proc *proc;           
};

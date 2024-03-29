struct stat;
struct rtcdate;

// system calls
int fork(void);
void ticketlockinit(void);
void ticketlocktest(void);
void chtickets(int, int);
void chpr(int,int);
void chmfq(int,int);
void ps(void);
void rwinit(void);
void rwtest(uint);
void wrinit(void);
void wrtest(uint);
void shm_init(void);
int shm_open(int id, int page_count, int flag);
void* shm_attach(int id);
int shm_close(int id);
int inc_num(int num);
void invoked_syscalls(int pid);
void sort_syscalls(int pid);
int get_count(int pid, int sysnum);
void log_syscalls(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int halt(void);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);

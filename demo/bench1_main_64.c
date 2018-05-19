#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <setjmp.h>

int global_data;

__attribute__ ((noinline)) int is_prime(int n) {
    global_data++;
    for(int i=2;i<n;i++){
        if(n%i==0) return 0;
    }
    return 1;
}

void print_prime() {
    for(int i=2;i<10000000;i++) {
        if(is_prime(i)){
            sleep(1);
            printf("%d\n",i);
        }
    }
}

void signal_handle(int sig_num)
{
    if(sig_num == SIGUSR1)
    {
        // load
        void* handle = dlopen("./fix.so", RTLD_LAZY);
        if(!handle){
            fprintf(stderr, "%s\n", dlerror());
            exit(-1);
        }
        void (*fix_init)(int*) =  dlsym(handle,"fix_init");
        if(!fix_init){
            fprintf(stderr, "%s\n", dlerror());
            exit(-1);
        }
        fix_init(&global_data);
        
        char* new_func = dlsym(handle,"fix_is_prime");

        char *old_func = (char *)is_prime;
        const int pagesize = sysconf(_SC_PAGE_SIZE);
        char* pg = (char*) ((size_t) old_func & ~(pagesize - 1));
        mprotect(pg, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC );
        mprotect(pg + pagesize, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC );

        // push %rax
        old_func[0] = 0x50;     
        
        // movq %0,%%rax
        old_func[1] = 0x48;     
        old_func[2] = 0xb8;
        *((u_int64_t*)(old_func+3)) = (u_int64_t) new_func;

        // jmpq *%eax
        old_func[11] = 0xff;
        old_func[12] = 0xe0;

        old_func[13] = old_func[14] = old_func[15] = 0x90;

        mprotect(pg, pagesize, PROT_READ | PROT_EXEC );
        mprotect(pg + pagesize, pagesize, PROT_READ | PROT_EXEC );
    }
}

// /var/tigerfix.tmp
void shell() {
    signal(SIGUSR1, signal_handle);
}

int main(){
    shell();
    print_prime();
    return 0;
}
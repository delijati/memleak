#define UNW_LOCAL_ONLY
#include <iostream>
#include <set>
#include <mutex>
#include <map>
#include <vector>

#include <signal.h>
#include <assert.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <string.h>
#include <stdio.h>

std::set<void *> s; // keep track of allocations that have not been freed
std::mutex mut; // protect s

extern "C" {

// LD_PRELOAD will cause the process to call this instead of malloc(3)
void *malloc(size_t size)
{
    // on first call, get a function pointer for malloc(3)
    static void *(*real_malloc)(size_t) = NULL;
    if(!real_malloc)
        real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    assert(real_malloc);

    // call malloc(3)
    void *retval = real_malloc(size);

    static __thread int dont_recurse = 0; // init to zero on first call
    if(dont_recurse)
        return retval;
    dont_recurse=1; // if anything below calls malloc, skip analysis

    // on first call, create cache for symbol at each address
    static thread_local std::map<unw_word_t, std::string> *m = NULL;
    if(!m)
        m = new std::map<unw_word_t, std::string>();

    // collect stack symbols, updating cache as needed 
    unw_cursor_t cursor;
    unw_context_t context;
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    std::vector<std::string *> trace;
    while (unw_step(&cursor) > 0)
    {
        unw_word_t pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0)
            break;

        std::string &str = (*m)[pc];
        if(str=="") // build cache
        {
            unw_word_t offset;
            // started as C, feel free to use std::string/ostringstream
            char sym[1024], line[1024];
            sprintf(line,"0x%lx:", pc);
            if (!unw_get_proc_name(&cursor, sym, sizeof(sym), &offset))
                sprintf(&line[strlen(line)], " (%s+0x%lx)\n",
                        sym, offset);
            else
                sprintf(&line[strlen(line)], " -- no symbol\n");
            str = line;
        }
        trace.push_back(&str);
    }

    // look for our particular stack context
    // - log it
    // - save for free()
    // - raise signal
    if(trace.size() >=4)
    if(strstr(trace[0]->c_str(),"__pyx_pw_6memerr_1random_noise"))
    {
        fprintf(stderr,"malloc @ %p for %lu\n", retval, size);
        std::lock_guard<std::mutex> g(mut);
        s.insert(retval);
        raise(SIGUSR1);
    }

    dont_recurse=0;
    return retval;
}

// report matching free() calls
void free(void *ptr)
{
    static void (*real_free)(void *) = NULL;
    if(!real_free)
        real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    assert(real_free);
    real_free(ptr);

    static __thread int dont_recurse = 0;
    if(dont_recurse)
        return;

    dont_recurse=1;
    mut.lock(); // in case lock_guard would call malloc/free?
    if(s.find(ptr) != s.end())
    {
        fprintf(stderr,"free @ %p\n", ptr);
        s.erase(ptr); // b/c addr will get reused
    }
    mut.unlock(); // before the following line, unlike with lock_guard
    dont_recurse=0;
}

} // extern C

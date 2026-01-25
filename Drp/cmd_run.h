#ifndef CMD_RUN_H
#define CMD_RUN_H
#include <stdint.h>
#include <string.h>
#include "cmd_builder.h"
#include "Allocators/allocator.h"
#include "env.h"
#include "which.h"
#include "windowsheader.h"
#include "posixheader.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif
#endif

static
int
cmd_wait(intptr_t* pproc_handle){
    int ret = 0;
    if(*pproc_handle == -1) goto finish;
    if(IS_WINDOWS){
        #ifdef _WIN32
        HANDLE proc = (HANDLE)(uintptr_t)*pproc_handle;
        if(proc == INVALID_HANDLE_VALUE) {ret = -1; goto finish;}
        DWORD waited = WaitForSingleObject(proc, -1);
        (void)waited;
        DWORD exit_code = 0;
        BOOL ok = GetExitCodeProcess(proc, &exit_code);
        if(!ok)printf("GetExitCodeProcess failed: %u\n", (unsigned)GetLastError());
        if(!ok) exit_code = 1;
        CloseHandle(proc);
        *pproc_handle = -1;
        ret = (int)exit_code;
        #endif
    }
    else {
    #ifndef _WIN32
        pid_t pid = (pid_t)*pproc_handle;
        if(pid == -1) {ret = -1; goto finish;}
        int status;
        int options = 0;
        for(pid_t p;;){
            p = waitpid(pid, &status, options);
            if(p == -1){
                if(errno == EINTR) continue;
            }
            break;
        }
        *pproc_handle = -1;
        if(!WIFEXITED(status))
            ret = 1;
        else if(WEXITSTATUS(status) != 0)
            ret = WEXITSTATUS(status);
        #endif
    }
    finish:;
    return ret;
}

static
int
cmd_wait_many(intptr_t* proc_handles, size_t n, size_t* which, int* exit_code, _Bool block){
    if(!n) return 1;
    if(IS_WINDOWS){
        #ifdef _WIN32
        DWORD ret = WaitForMultipleObjects((DWORD)n, (const HANDLE*)proc_handles, FALSE, block?INFINITE:0);
        if(ret == WAIT_TIMEOUT){
            *which = (size_t)-1;
            if(block) return 1;
            else return 0;
        }
        if(ret >= WAIT_OBJECT_0 && n <= WAIT_OBJECT_0 + (DWORD)n){
            DWORD idx = ret - WAIT_OBJECT_0;
            *which = (size_t)idx;
            HANDLE h = (HANDLE)proc_handles[idx];
            BOOL ok = GetExitCodeProcess(h, (DWORD*)exit_code);
            if(!ok){
                *exit_code = 0;
            }
            CloseHandle(h);
            return 0;
        }
        // WAIT_FAILED or other weird results
        return ret;
        #endif
    }
    else {
        #ifndef _WIN32
        int status = 0;
        pid_t p;
        do {
            p = waitpid(WAIT_ANY, &status, block?0:WNOHANG);
        }while(p == -1 && errno == EINTR);
        if(p == 0){
            *which  = (size_t)-1;
            return block?1:0;
        }
        for(size_t i = 0; i < n; i++){
            if((pid_t)proc_handles[i] == p){
                proc_handles[i] = 0;
                *which = i;
                if(WIFEXITED(status)){
                    // fprintf(stderr, "WIFEXITED. WEXITSTATUS(status): %d\n", WEXITSTATUS(status));
                    *exit_code = WEXITSTATUS(status);
                }
                else if(WIFSIGNALED(status)){
                    // fprintf(stderr, "WIFSIGNALED. WTERMSIG(status): %d\n", WTERMSIG(status));
                    *exit_code = WTERMSIG(status);
                }
                else {
                    return 1;
                }
                return 0;
            }
        }
        return 1;
        #endif
    }
}

static
_Bool
arg_needs_escape_win32(StringView arg){
    for(size_t i = 0; i < arg.length; i++){
        char c = arg.text[i];
        switch(c){
            case ' ':
            case '\r':
            case '\t':
            case '\n':
            case '\f':
            case '"':
                return 1;
            default:
                break;
        }
    }
    return 0;
}

static
int
cmd_run(CmdBuilder* cmd, void* envp, intptr_t*_Nullable proc_handle){
    if(cmd->errored){
        return 1;
    }
    if(!cmd->prog.cursor){
        return 1;
    }
    intptr_t hProc = -1;
    if(IS_WINDOWS){
        msb_reset(&cmd->cmd_line);
        for(size_t i = 0; i < cmd->args.count; i++){
            if(i != 0) msb_write_char(&cmd->cmd_line, ' ');
            StringView sv = LS_to_SV(cmd->args.data[i]);
            if(arg_needs_escape_win32(sv)){
                msb_write_char(&cmd->cmd_line, '"');
                for(size_t j = 0; j < sv.length; j++){
                    char c = sv.text[j];
                    if(c == '"' || c == '\\')
                        msb_write_char(&cmd->cmd_line, '\\');
                    msb_write_char(&cmd->cmd_line, c);
                }
                msb_write_char(&cmd->cmd_line, '"');
            }
            else
                msb_write_str(&cmd->cmd_line, sv.text, sv.length);
        }
        msb_nul_terminate(&cmd->cmd_line);
        msb_nul_terminate(&cmd->prog);
        if(cmd->cmd_line.errored) return 1;
        if(cmd->prog.errored) return 1;

        char* prog = cmd->prog.data;
        char* cmdline = cmd->cmd_line.data;
        #ifdef _WIN32
            STARTUPINFOA si = {.cb = sizeof si};
            PROCESS_INFORMATION pi = {0};
            BOOL ok = CreateProcessA(prog, cmdline, NULL, NULL, 0, 0, envp, NULL, &si, &pi);
            if(!ok) {
                printf("CreateProcessA failed: %u\n", (unsigned)GetLastError());
                return 1;
            }
            CloseHandle(pi.hThread);
            hProc = (intptr_t)(uintptr_t)pi.hProcess;
        #else
            (void)prog;
            (void)cmdline;
        #endif
    }
    else {
        void* _argv = Allocator_zalloc(cmd->allocator, (cmd->args.count+1) * sizeof(char*));
        char** argv = _argv;
        {
            const char** pargv = _argv;
            for(size_t i = 0; i < cmd->args.count; i++)
                pargv[i] = cmd->args.data[i].text;
        }
        #ifndef _WIN32
            pid_t pid;
            int e;
            posix_spawn_file_actions_t* actions = NULL;
            posix_spawnattr_t* attrs = NULL;
            e = posix_spawn(&pid, cmd->prog.data, actions, attrs, argv, envp);
            if(e){
                // fprintf(stderr, "posix_spawn: %s\n", strerror(e));
                return e;
            }
            hProc = (intptr_t)pid;
        #else
            (void)argv;
        #endif
    }
    if(proc_handle){
        *proc_handle = hProc;
        return 0;
    }
    int ret = cmd_wait(&hProc);
    return ret;
}

static
int
cmd_exec(CmdBuilder* cmd, void* envp){
    if(cmd->errored) return 1;
    if(IS_WINDOWS){
        int err = cmd_run(cmd, envp, NULL);
        #ifdef _WIN32
        ExitProcess(err);
        #endif
        if(err) return err;
    }
    else {
        void* _argv = Allocator_zalloc(cmd->allocator, (cmd->args.count+1) * sizeof(char*));
        char** argv = _argv;
        {
            const char** pargv = _argv;
            for(size_t i = 0; i < cmd->args.count; i++)
                pargv[i] = cmd->args.data[i].text;
        }
        #ifndef _WIN32
        int err = execve(cmd->prog.data, argv, envp);
        if(err) return err;
        #else
        (void)argv;
        #endif
    }
    return 1;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif

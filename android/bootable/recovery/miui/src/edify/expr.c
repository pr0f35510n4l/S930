/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "expr.h"
/*lenovo-sw jixj 2013.5.24 add begin*/
#ifdef LENOVO_RECOVERY_ONLINE
#include <cutils/properties.h>
#include <utils/Log.h>
#ifndef LENOVO_BUILD_LIB
#define ALOGE(fmt, args...) printf(fmt,##args)
#else
#define LOG_TAG "recovery_online"
#endif
static char system_backup[] = "/cache/tmp/system";
static char system1[] = "/system";
static char system_tmp[] = "/cache/tmp";
static char tmp[] = "/tmp";
static int is_recovery_online() {
    #ifdef LENOVO_BUILD_LIB
    char value[32] = {'\0'};
    property_get("sys.recovery.online",value,"f");
    return (value[0] == '1');
    #else
    return 0;
    #endif
}
#endif
/*lenovo-sw jixj 2013.5.24 add end*/
// Functions should:
//
//    - return a malloc()'d string
//    - if Evaluate() on any argument returns NULL, return NULL.

int BooleanString(const char* s) {
    return s[0] != '\0';
}

char* Evaluate(State* state, Expr* expr) {
    Value* v = expr->fn(expr->name, state, expr->argc, expr->argv);
    if (v == NULL) return NULL;
    if (v->type != VAL_STRING) {
        ErrorAbort(state, "expecting string, got value type %d", v->type);
        FreeValue(v);
        return NULL;
    }
    char* result = v->data;
    free(v);
    /*lenovo-sw jixj 2013.5.24 add begin*/
    #ifdef LENOVO_RECOVERY_ONLINE
    if(is_recovery_online()) {
        if((memcmp(result, system_backup, strlen(system_backup)) != 0) && (memcmp(result, system1, strlen(system1)) == 0)) {
            char path[256];
            strcpy(path, strlen(system1)+result);
            char* temp;
            temp = (char *)malloc(strlen(result) + strlen(system_backup) - strlen(system1)+1);
            strcpy(temp, system_backup);
            strcat(temp, path);
            fprintf(stderr,"jixj system backup=%s\n", temp);
            ALOGE("jixj system backup=%s\n", temp);
            free(result);
            result = temp;
        } else if((memcmp(result, system_tmp, strlen(system_tmp)) != 0) && (memcmp(result, tmp, strlen(tmp)) == 0)) {
            char path[256];
            strcpy(path, strlen(tmp)+result);
            char* temp;
            temp = (char *)malloc(strlen(result) + strlen(system_tmp) - strlen(tmp)+1);
            strcpy(temp, system_tmp);
            strcat(temp, path);
            fprintf(stderr,"jixj system_tmp temp=%s\n", temp);
            ALOGE("jixj system_tmp temp=%s\n", temp);
            free(result);
            result = temp;
        }
    }
    #endif
    /*lenovo-sw jixj 2013.5.24 add end*/
    return result;
}

Value* EvaluateValue(State* state, Expr* expr) {
    /*lenovo-sw jixj 2013.5.24 modified begin*/
    #ifndef LENOVO_RECOVERY_ONLINE
    return expr->fn(expr->name, state, expr->argc, expr->argv);
    #else
    if(!is_recovery_online()) {
        return expr->fn(expr->name, state, expr->argc, expr->argv);
    } else {
        Value* v = expr->fn(expr->name, state, expr->argc, expr->argv);
        char* result = v->data;
        if((memcmp(result, system_backup, strlen(system_backup)) != 0) && (memcmp(result, system1, strlen(system1)) == 0)) {
            char path[256];
            strcpy(path, strlen(system1)+result);
            char* temp;
            temp = (char *)malloc(strlen(result) + strlen(system_backup) - strlen(system1)+1);
            strcpy(temp, system_backup);
            strcat(temp, path);
            fprintf(stderr,"jixj EvaluateValue system backup temp=%s\n", temp);
            ALOGE("jixj EvaluateValue system backup temp=%s\n", temp);
            free(result);
            result = temp;
        } else if((memcmp(result, system_tmp, strlen(system_tmp)) != 0) && (memcmp(result, tmp, strlen(tmp)) == 0)) {
            char path[256];
            strcpy(path, strlen(tmp)+result);
            char* temp;
            temp = (char *)malloc(strlen(result) + strlen(system_tmp) - strlen(tmp)+1);
            strcpy(temp, system_tmp);
            strcat(temp, path);
            fprintf(stderr,"jixj EvaluateValue system_tmp temp=%s\n", temp);
            ALOGE("jixj EvaluateValue system_tmp temp=%s\n", temp);
            free(result);
            result = temp;
        }
        v->data = result;
        return v;
    }
    #endif
    /*lenovo-sw jixj 2013.5.24 modified end*/
}

Value* StringValue(char* str) {
    if (str == NULL) return NULL;
    Value* v = malloc(sizeof(Value));
    v->type = VAL_STRING;
    v->size = strlen(str);
    v->data = str;
    return v;
}

void FreeValue(Value* v) {
    if (v == NULL) return;
    free(v->data);
    free(v);
}

Value* ConcatFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc == 0) {
        return StringValue(strdup(""));
    }
    char** strings = malloc(argc * sizeof(char*));
    int i;
    for (i = 0; i < argc; ++i) {
        strings[i] = NULL;
    }
    char* result = NULL;
    int length = 0;
    for (i = 0; i < argc; ++i) {
        strings[i] = Evaluate(state, argv[i]);
        if (strings[i] == NULL) {
            goto done;
        }
        length += strlen(strings[i]);
    }

    result = malloc(length+1);
    int p = 0;
    for (i = 0; i < argc; ++i) {
        strcpy(result+p, strings[i]);
        p += strlen(strings[i]);
    }
    result[p] = '\0';

  done:
    for (i = 0; i < argc; ++i) {
        free(strings[i]);
    }
    free(strings);
    return StringValue(result);
}

Value* IfElseFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc != 2 && argc != 3) {
        free(state->errmsg);
        state->errmsg = strdup("ifelse expects 2 or 3 arguments");
        return NULL;
    }
    char* cond = Evaluate(state, argv[0]);
    if (cond == NULL) {
        return NULL;
    }

    if (BooleanString(cond) == true) {
        free(cond);
        return EvaluateValue(state, argv[1]);
    } else {
        if (argc == 3) {
            free(cond);
            return EvaluateValue(state, argv[2]);
        } else {
            return StringValue(cond);
        }
    }
}

Value* AbortFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* msg = NULL;
    if (argc > 0) {
        msg = Evaluate(state, argv[0]);
    }
    free(state->errmsg);
    if (msg) {
        state->errmsg = msg;
    } else {
        state->errmsg = strdup("called abort()");
    }
    return NULL;
}

Value* AssertFn(const char* name, State* state, int argc, Expr* argv[]) {
    int i;
    for (i = 0; i < argc; ++i) {
        char* v = Evaluate(state, argv[i]);
        if (v == NULL) {
            return NULL;
        }
        int b = BooleanString(v);
        free(v);
        if (!b) {
            int prefix_len;
            int len = argv[i]->end - argv[i]->start;
            char* err_src = malloc(len + 20);
            strcpy(err_src, "assert failed: ");
            prefix_len = strlen(err_src);
            memcpy(err_src + prefix_len, state->script + argv[i]->start, len);
            err_src[prefix_len + len] = '\0';
            free(state->errmsg);
            state->errmsg = err_src;
            return NULL;
        }
    }
    return StringValue(strdup(""));
}

Value* SleepFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* val = Evaluate(state, argv[0]);
    if (val == NULL) {
        return NULL;
    }
    int v = strtol(val, NULL, 10);
    sleep(v);
    return StringValue(val);
}

Value* StdoutFn(const char* name, State* state, int argc, Expr* argv[]) {
    int i;
    for (i = 0; i < argc; ++i) {
        char* v = Evaluate(state, argv[i]);
        if (v == NULL) {
            return NULL;
        }
        fputs(v, stdout);
        free(v);
    }
    return StringValue(strdup(""));
}

Value* LogicalAndFn(const char* name, State* state,
                   int argc, Expr* argv[]) {
    char* left = Evaluate(state, argv[0]);
    if (left == NULL) return NULL;
    if (BooleanString(left) == true) {
        free(left);
        return EvaluateValue(state, argv[1]);
    } else {
        return StringValue(left);
    }
}

Value* LogicalOrFn(const char* name, State* state,
                   int argc, Expr* argv[]) {
    char* left = Evaluate(state, argv[0]);
    if (left == NULL) return NULL;
    if (BooleanString(left) == false) {
        free(left);
        return EvaluateValue(state, argv[1]);
    } else {
        return StringValue(left);
    }
}

Value* LogicalNotFn(const char* name, State* state,
                    int argc, Expr* argv[]) {
    char* val = Evaluate(state, argv[0]);
    if (val == NULL) return NULL;
    bool bv = BooleanString(val);
    free(val);
    return StringValue(strdup(bv ? "" : "t"));
}

Value* SubstringFn(const char* name, State* state,
                   int argc, Expr* argv[]) {
    char* needle = Evaluate(state, argv[0]);
    if (needle == NULL) return NULL;
    char* haystack = Evaluate(state, argv[1]);
    if (haystack == NULL) {
        free(needle);
        return NULL;
    }

    char* result = strdup(strstr(haystack, needle) ? "t" : "");
    free(needle);
    free(haystack);
    return StringValue(result);
}

Value* EqualityFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* left = Evaluate(state, argv[0]);
    if (left == NULL) return NULL;
    char* right = Evaluate(state, argv[1]);
    if (right == NULL) {
        free(left);
        return NULL;
    }

    char* result = strdup(strcmp(left, right) == 0 ? "t" : "");
    free(left);
    free(right);
    return StringValue(result);
}

Value* InequalityFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* left = Evaluate(state, argv[0]);
    if (left == NULL) return NULL;
    char* right = Evaluate(state, argv[1]);
    if (right == NULL) {
        free(left);
        return NULL;
    }

    char* result = strdup(strcmp(left, right) != 0 ? "t" : "");
    free(left);
    free(right);
    return StringValue(result);
}

Value* SequenceFn(const char* name, State* state, int argc, Expr* argv[]) {
    Value* left = EvaluateValue(state, argv[0]);
    if (left == NULL) return NULL;
    FreeValue(left);
    return EvaluateValue(state, argv[1]);
}

Value* LessThanIntFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc != 2) {
        free(state->errmsg);
        state->errmsg = strdup("less_than_int expects 2 arguments");
        return NULL;
    }

    char* left;
    char* right;
    if (ReadArgs(state, argv, 2, &left, &right) < 0) return NULL;

    bool result = false;
    char* end;

    long l_int = strtol(left, &end, 10);
    if (left[0] == '\0' || *end != '\0') {
        fprintf(stderr, "[%s] is not an int\n", left);
        goto done;
    }

    long r_int = strtol(right, &end, 10);
    if (right[0] == '\0' || *end != '\0') {
        fprintf(stderr, "[%s] is not an int\n", right);
        goto done;
    }

    result = l_int < r_int;

  done:
    free(left);
    free(right);
    return StringValue(strdup(result ? "t" : ""));
}

Value* GreaterThanIntFn(const char* name, State* state,
                        int argc, Expr* argv[]) {
    if (argc != 2) {
        free(state->errmsg);
        state->errmsg = strdup("greater_than_int expects 2 arguments");
        return NULL;
    }

    Expr* temp[2];
    temp[0] = argv[1];
    temp[1] = argv[0];

    return LessThanIntFn(name, state, 2, temp);
}

Value* Literal(const char* name, State* state, int argc, Expr* argv[]) {
    return StringValue(strdup(name));
}

Expr* Build(Function fn, YYLTYPE loc, int count, ...) {
    va_list v;
    va_start(v, count);
    Expr* e = malloc(sizeof(Expr));
    e->fn = fn;
    e->name = "(operator)";
    e->argc = count;
    e->argv = malloc(count * sizeof(Expr*));
    int i;
    for (i = 0; i < count; ++i) {
        e->argv[i] = va_arg(v, Expr*);
    }
    va_end(v);
    e->start = loc.start;
    e->end = loc.end;
    return e;
}

// -----------------------------------------------------------------
//   the function table
// -----------------------------------------------------------------

static int fn_entries = 0;
static int fn_size = 0;
NamedFunction* fn_table = NULL;

void RegisterFunction(const char* name, Function fn) {
    if (fn_entries >= fn_size) {
        fn_size = fn_size*2 + 1;
        fn_table = realloc(fn_table, fn_size * sizeof(NamedFunction));
    }
    fn_table[fn_entries].name = name;
    fn_table[fn_entries].fn = fn;
    ++fn_entries;
}

static int fn_entry_compare(const void* a, const void* b) {
    const char* na = ((const NamedFunction*)a)->name;
    const char* nb = ((const NamedFunction*)b)->name;
    return strcmp(na, nb);
}

void FinishRegistration() {
    qsort(fn_table, fn_entries, sizeof(NamedFunction), fn_entry_compare);
}

Function FindFunction(const char* name) {
    NamedFunction key;
    key.name = name;
    NamedFunction* nf = bsearch(&key, fn_table, fn_entries,
                                sizeof(NamedFunction), fn_entry_compare);
    if (nf == NULL) {
        return NULL;
    }
    return nf->fn;
}

void RegisterBuiltins() {
    RegisterFunction("ifelse", IfElseFn);
    RegisterFunction("abort", AbortFn);
    RegisterFunction("assert", AssertFn);
    RegisterFunction("concat", ConcatFn);
    RegisterFunction("is_substring", SubstringFn);
    RegisterFunction("stdout", StdoutFn);
    RegisterFunction("sleep", SleepFn);

    RegisterFunction("less_than_int", LessThanIntFn);
    RegisterFunction("greater_than_int", GreaterThanIntFn);
}


// -----------------------------------------------------------------
//   convenience methods for functions
// -----------------------------------------------------------------

// Evaluate the expressions in argv, giving 'count' char* (the ... is
// zero or more char** to put them in).  If any expression evaluates
// to NULL, free the rest and return -1.  Return 0 on success.
int ReadArgs(State* state, Expr* argv[], int count, ...) {
    char** args = malloc(count * sizeof(char*));
    va_list v;
    va_start(v, count);
    int i;
    for (i = 0; i < count; ++i) {
        args[i] = Evaluate(state, argv[i]);
        if (args[i] == NULL) {
            va_end(v);
            int j;
            for (j = 0; j < i; ++j) {
                free(args[j]);
            }
            free(args);
            return -1;
        }
        *(va_arg(v, char**)) = args[i];
    }
    va_end(v);
    free(args);
    return 0;
}

// Evaluate the expressions in argv, giving 'count' Value* (the ... is
// zero or more Value** to put them in).  If any expression evaluates
// to NULL, free the rest and return -1.  Return 0 on success.
int ReadValueArgs(State* state, Expr* argv[], int count, ...) {
    Value** args = malloc(count * sizeof(Value*));
    va_list v;
    va_start(v, count);
    int i;
    for (i = 0; i < count; ++i) {
        args[i] = EvaluateValue(state, argv[i]);
        if (args[i] == NULL) {
            va_end(v);
            int j;
            for (j = 0; j < i; ++j) {
                FreeValue(args[j]);
            }
            free(args);
            return -1;
        }
        *(va_arg(v, Value**)) = args[i];
    }
    va_end(v);
    free(args);
    return 0;
}

// Evaluate the expressions in argv, returning an array of char*
// results.  If any evaluate to NULL, free the rest and return NULL.
// The caller is responsible for freeing the returned array and the
// strings it contains.
char** ReadVarArgs(State* state, int argc, Expr* argv[]) {
    char** args = (char**)malloc(argc * sizeof(char*));
    int i = 0;
    for (i = 0; i < argc; ++i) {
        args[i] = Evaluate(state, argv[i]);
        if (args[i] == NULL) {
            int j;
            for (j = 0; j < i; ++j) {
                free(args[j]);
            }
            free(args);
            return NULL;
        }
    }
    return args;
}

// Evaluate the expressions in argv, returning an array of Value*
// results.  If any evaluate to NULL, free the rest and return NULL.
// The caller is responsible for freeing the returned array and the
// Values it contains.
Value** ReadValueVarArgs(State* state, int argc, Expr* argv[]) {
    Value** args = (Value**)malloc(argc * sizeof(Value*));
    int i = 0;
    for (i = 0; i < argc; ++i) {
        args[i] = EvaluateValue(state, argv[i]);
        if (args[i] == NULL) {
            int j;
            for (j = 0; j < i; ++j) {
                FreeValue(args[j]);
            }
            free(args);
            return NULL;
        }
    }
    return args;
}

// Use printf-style arguments to compose an error message to put into
// *state.  Returns NULL.
Value* ErrorAbort(State* state, char* format, ...) {
    char* buffer = malloc(4096);
    va_list v;
    va_start(v, format);
    vsnprintf(buffer, 4096, format, v);
    va_end(v);
    free(state->errmsg);
    state->errmsg = buffer;
    return NULL;
}

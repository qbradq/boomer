#include "script_sys.h"
#include "fs.h"
#include <stdio.h>
#include <string.h>

static JSRuntime* rt = NULL;
static JSContext* ctx = NULL;

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (!str) return JS_EXCEPTION;
        if (i > 0) putchar(' ');
        fputs(str, stdout);
        JS_FreeCString(ctx, str);
    }
    putchar('\n');
    return JS_UNDEFINED;
}

bool Script_Init(void) {
    if (ctx) Script_Shutdown();
    
    rt = JS_NewRuntime();
    if (!rt) {
        printf("QuickJS: Failed to create runtime.\n");
        return false;
    }
    
    // Increase stack limit for Web/WASM
    JS_SetMaxStackSize(rt, 0);
    
    ctx = JS_NewContext(rt);
    if (!ctx) {
        printf("QuickJS: Failed to create context.\n");
        JS_FreeRuntime(rt);
        rt = NULL;
        return false;
    }
    
    // Add system helpers (print, console) generic basics?
    // QuickJS doesn't have a default console without std modules.
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "print", JS_NewCFunction(ctx, js_print, "print", 1));
    JS_FreeValue(ctx, global_obj);
    
    return true;
}

void Script_Shutdown(void) {
    if (ctx) {
        JS_FreeContext(ctx);
        ctx = NULL;
    }
    if (rt) {
        JS_FreeRuntime(rt);
        rt = NULL;
    }
}

JSContext* Script_GetContext(void) {
    return ctx;
}

JSValue Script_EvalFile(const char* path) {
    if (!ctx) return JS_EXCEPTION;
    
    size_t size;
    char* data = FS_ReadFile(path, &size);
    if (!data) {
        printf("Script: Could not read script '%s'\n", path);
        return JS_ThrowInternalError(ctx, "Could not read script file");
    }
    
    JSValue val = JS_Eval(ctx, data, size, path, JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(val)) {
        JSValue ex = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, ex);
        if (str) {
            printf("Script: Exception in '%s': %s\n", path, str);
            JS_FreeCString(ctx, str);
        }
        
        JSValue stack = JS_GetPropertyStr(ctx, ex, "stack");
        if (!JS_IsUndefined(stack)) {
            const char* stack_str = JS_ToCString(ctx, stack);
            if (stack_str) {
                printf("%s\n", stack_str);
                JS_FreeCString(ctx, stack_str);
            }
        }
        JS_FreeValue(ctx, stack);
        
        JS_FreeValue(ctx, ex);
        FS_FreeFile(data);
        return JS_EXCEPTION; 
    }
    
    FS_FreeFile(data);
    return val;
}

void Script_RegisterFunc(const char* name, JSCFunction* func, int length) {
    if (!ctx) return;
    
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue js_func = JS_NewCFunction(ctx, func, name, length);
    
    JS_SetPropertyStr(ctx, global_obj, name, js_func);
    
    JS_FreeValue(ctx, global_obj);
}

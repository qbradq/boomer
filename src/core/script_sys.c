#include "script_sys.h"
#include "fs.h"
#include "../ui/console.h"
#include <stdio.h>
#include <string.h>

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

static JSRuntime* rt = NULL;
static JSContext* ctx = NULL;

// --- Console Bindings ---

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Join args
    char buf[1024];
    buf[0] = 0;
    size_t current_len = 0;
    
    for (int i = 0; i < argc; i++) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (!str) return JS_EXCEPTION;
        
        size_t len = strlen(str);
        if (current_len + len + 2 < sizeof(buf)) { // +2 for space and null
            if (i > 0) {
                strcat(buf, " ");
                current_len++;
            }
            strcat(buf, str);
            current_len += len;
        }
        JS_FreeCString(ctx, str);
    }
    
    // Output
    Console_Log("%s", buf);
    
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_console_funcs[] = {
    JS_CFUNC_DEF("log", 1, js_print), 
    JS_CFUNC_DEF("info", 1, js_print),
    JS_CFUNC_DEF("warn", 1, js_print),
    JS_CFUNC_DEF("error", 1, js_print),
};

static int js_console_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, js_console_funcs, countof(js_console_funcs));
}

// --- Module System ---

// Simple path join/normalization
static char* ResolvePath(const char* base, const char* name) {
    // If name is "console", return it as is (builtin)
    if (strcmp(name, "console") == 0) return strdup(name);

    char buf[512];
    buf[0] = 0;
    
    // If base is present, take its directory
    if (base && strlen(base) > 0) {
        const char* last_slash = strrchr(base, '/');
        if (last_slash) {
            int len = last_slash - base;
            strncpy(buf, base, len);
            buf[len] = 0;
        } else {
             // Base has no slash? Use it as is?
             strcpy(buf, base);
        }
    }
    
    // If name starts with ./ or ../, append to base dir
    // If name is bare (e.g. "utils.js"), prompt says root is `scripts` directory.
    // But `main.js` is IN `scripts` directory.
    // So relative to `main.js` (base) works for siblings.
    // But what if it's "sub/mod.js"?
    
    // Logic:
    // 1. If name starts with '.', resolve relative to base dir.
    // 2. If name is "std" or "os", keep as is.
    // 3. If name is something else, treating it same as relative to base dir is usually fine 
    //    for simple engines, OR relative to "scripts" root.
    //    Let's assume relative to base dir for simplicity unless "scripts/" needed.
    //    Actually, if base is `games/demo/scripts/main.js`, dir is `games/demo/scripts`.
    //    `import "foo.js"` -> `games/demo/scripts/foo.js`. matches.
    
    if (buf[0] != 0 && buf[strlen(buf)-1] != '/') strcat(buf, "/");
    strcat(buf, name);
    
    // Simplify ".." and "." (Naive implementation)
    // For now, let's just return the joined path. FS_ReadFile usually handles paths OK, 
    // but "." in middle might annoy.
    
    return strdup(buf);
}

static JSModuleDef *js_module_loader(JSContext *ctx, const char *module_name, void *opaque) {
    // 1. Block prohibited
    if (strcmp(module_name, "std") == 0 || strcmp(module_name, "os") == 0) {
        JS_ThrowReferenceError(ctx, "Access to system module '%s' is prohibited.", module_name);
        return NULL;
    }
    
    // 2. Handle 'console' built-in
    if (strcmp(module_name, "console") == 0) {
        JSModuleDef *m = JS_NewCModule(ctx, "console", js_console_init);
        if (!m) return NULL;
        JS_AddModuleExportList(ctx, m, js_console_funcs, countof(js_console_funcs));
        return m;
    }
    
    // 3. Load file
    size_t size;
    char* data = FS_ReadFile(module_name, &size);
    if (!data) {
        JS_ThrowReferenceError(ctx, "Could not load module '%s'", module_name);
        return NULL;
    }
    
    JSValue func_val = JS_Eval(ctx, data, size, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    FS_FreeFile(data);
    
    if (JS_IsException(func_val)) return NULL;
    
    JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
}

static char *js_module_normalize(JSContext *ctx, const char *module_base_name, const char *module_name, void *opaque) {
    return ResolvePath(module_base_name, module_name);
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
    
    // Set Module Loader
    JS_SetModuleLoaderFunc(rt, js_module_normalize, js_module_loader, NULL);

    // Note: 'console' module will be added later in Console_Init or manually here?
    // The requirement says "Add a new built-in JavaScript module 'console'".
    // We should expose a C function for that.
    
    // Add 'print' to global object as well (for convenience)
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
    
    // Treat as Module
    JSValue val = JS_Eval(ctx, data, size, path, JS_EVAL_TYPE_MODULE);
    
    if (JS_IsException(val)) {
        JSValue ex = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, ex);
        if (str) {
            printf("Script: Exception in '%s': %s\n", path, str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, ex);
        FS_FreeFile(data);
        return JS_EXCEPTION; 
    }
    
    // For module, JS_Eval returns the Module Object (compiled).
    // We must execute it?
    // JS_Eval with JS_EVAL_TYPE_MODULE returns a Module Object.
    // It is NOT a function in the traditional sense, but we don't need to call it?
    // Wait, QuickJS modules need to be evaluated.
    // If JS_Eval returns a Module, it's not yet evaluated.
    // We strictly need to eval it if it's a module?
    // Actually JS_Eval(..., JS_EVAL_TYPE_MODULE) *compiles* it. 
    // To run it, we do nothing? No, we probably need `JS_EvalFunction` but that's for functions.
    
    // In QuickJS, JS_Eval with JS_EVAL_TYPE_MODULE returns the module. 
    // You typically don't run it immediately if dependencies are loading.
    // But since we use synchronous loader, maybe it is fine?
    // Actually we need to check if we need to call `JS_EvalFunction(ctx, val)`.
    // NOTE: JS_Eval doc says: "If 'input' is a module, return the module object."
    // To execute it:
    
    if (JS_IsModule(val)) {
         // Evaluate the module
         // But JS_EvalFunction expects a function.
         // Actually, if we look at `qjs.c`:
         // if ((val = JS_Eval(ctx, buf, len, filename, JS_EVAL_TYPE_MODULE)) ...)
         //    /* nothing? */
         // return val;
         //
         // Wait, relying on implicit evaluation?
         // No, QuickJS modules are evaluated only when imported?
         // Or do we need `JS_GetModuleExport`?
         
         // Looking at quickjs docs/examples:
         // `val = JS_Eval(..., JS_EVAL_TYPE_MODULE);`
         // `if (!JS_IsException(val)) { JSValue res = JS_EvalFunction(ctx, val); }`
         // Yes, JS_EvalFunction is used to execute the module body.
         
         JSValue res = JS_EvalFunction(ctx, val);
         if (JS_IsException(res)) {
            JSValue ex = JS_GetException(ctx);
            const char* str = JS_ToCString(ctx, ex);
            printf("Script: Uncaught exception in module '%s': %s\n", path, str);
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, ex);
            FS_FreeFile(data);
            return JS_EXCEPTION;
         }
         JS_FreeValue(ctx, res); // Result of module eval (usually undefined)
    }
    
    FS_FreeFile(data);
    return val; // Return the Module object (or result?)
}

void Script_RegisterFunc(const char* name, JSCFunction* func, int length) {
    if (!ctx) return;
    
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue js_func = JS_NewCFunction(ctx, func, name, length);
    
    JS_SetPropertyStr(ctx, global_obj, name, js_func);
    
    JS_FreeValue(ctx, global_obj);
}

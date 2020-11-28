#include "quickjs-libc.h"
#include "cutils.h"

#include "uv.h"

#define TJS__PLATFORM "n/a"
#define QJS_VERSION_STR "n/a"

#include "tuv/version.c"
//#include "tuv/dns.c"
#include "tuv/error.c"
#include "tuv/fs.c"
#include "tuv/misc.c"
#include "tuv/process.c"
//#include "tuv/signals.c"
//#include "tuv/std.c"
#include "tuv/streams.c"
#include "tuv/timers.c"
#include "tuv/udp.c"
#include "tuv/utils.c"
//#include "tuv/worker.c"

//#define QJLS_MOD_TRACE 1
#ifdef QJLS_MOD_TRACE
#include <stdio.h>
#define trace(...) printf(__VA_ARGS__)
#else
#define trace(...) ((void)0)
#endif

static JSClassID qjs_tuv_class_id;

static void qjs_tuv_finalizer(JSRuntime *rt, JSValue val) {
	trace("qjs_tuv_finalizer(%p)\n", rt);
	TJSRuntime *qrt = JS_GetOpaque(val, qjs_tuv_class_id);
	if (qrt) {
		trace("qjs_tuv_finalizer() free %p\n", qrt);
		free(qrt);
	}
	trace("qjs_tuv_finalizer() completed\n");
}

static JSClassDef qjs_tuv_class = {
	"TUV",
	.finalizer = qjs_tuv_finalizer,
};

#define TUV_CONTEXT "_tuv_context"


// From "tuv/vm.c"
/*
JSContext *TJS_GetJSContext(TJSRuntime *qrt) {
    return qrt->ctx;
}
static void uv__stop(uv_async_t *handle) {
    TJSRuntime *qrt = handle->data;
    CHECK_NOT_NULL(qrt);
    uv_stop(&qrt->loop);
}
*/
JSValue tjs__get_args(JSContext *ctx) {
    JSValue args = JS_NewArray(ctx);
		/*for (int i = 0; i < tjs__argc; i++) {
        JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, tjs__argv[i]));
    }*/
    return args;
}
TJSRuntime *TJS_GetRuntime(JSContext *ctx) {
	trace("TJS_GetRuntime(%p)\n", ctx);
	TJSRuntime *qrt;
	JSValue global_obj = JS_GetGlobalObject(ctx);
	JSValue tuv_context_prop = JS_GetPropertyStr(ctx, global_obj, TUV_CONTEXT);
	if (!JS_IsUndefined(tuv_context_prop)) {
		// we should check 
		trace("TJS_GetRuntime() tuv_context found\n");
		JS_FreeValue(ctx, global_obj);
		JS_FreeValue(ctx, tuv_context_prop);
		qrt = JS_GetOpaque(tuv_context_prop, qjs_tuv_class_id);
		return qrt;
	}
	// Create
	tuv_context_prop = JS_NewObjectClass(ctx, qjs_tuv_class_id);
	if (JS_IsException(tuv_context_prop)) {
		JS_FreeValue(ctx, global_obj);
		return NULL;
	}

	qrt = calloc(1, sizeof(TJSRuntime)); // The memory is set to zero
	if (!qrt) {
		JS_FreeValue(ctx, tuv_context_prop);
		JS_FreeValue(ctx, global_obj);
		return NULL;
	}

	trace("TJS_GetRuntime() creating tuv_context\n");
	CHECK_EQ(uv_loop_init(&qrt->loop), 0);
	/*
	// handle which runs the job queue
	CHECK_EQ(uv_prepare_init(&qrt->loop, &qrt->jobs.prepare), 0);
	qrt->jobs.prepare.data = qrt;

	// handle to prevent the loop from blocking for i/o when there are pending jobs.
	CHECK_EQ(uv_idle_init(&qrt->loop, &qrt->jobs.idle), 0);
	qrt->jobs.idle.data = qrt;

	// handle which runs the job queue
	CHECK_EQ(uv_check_init(&qrt->loop, &qrt->jobs.check), 0);
	qrt->jobs.check.data = qrt;

	// hande for stopping this runtime (also works from another thread)
	CHECK_EQ(uv_async_init(&qrt->loop, &qrt->stop, uv__stop), 0);
	qrt->stop.data = qrt;
	*/
	JS_SetOpaque(tuv_context_prop, qrt);

	// return -1 in case of exception or TRUE or FALSE. Warning: 'val' is freed by the function
	int ret = JS_SetPropertyStr(ctx, global_obj, TUV_CONTEXT, tuv_context_prop);
	JS_FreeValue(ctx, global_obj);
	if (ret != TRUE) {
		trace("TJS_GetRuntime() => unable to set " TUV_CONTEXT " global property\n");
		if (ret < 0) {
			tjs_dump_error(ctx);
		}
		JS_FreeValue(ctx, tuv_context_prop);
		return NULL;
	}
	trace("TJS_GetRuntime() => %p\n", qrt);
	return qrt;
}
uv_loop_t *TJS_GetLoop(TJSRuntime *qrt) {
	return &qrt->loop;
}
void tjs_execute_jobs(JSContext *ctx) {
    JSContext *ctx1;
    int err;
    /* execute the pending jobs */
    for (;;) {
        err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
        if (err <= 0) {
            if (err < 0)
                tjs_dump_error(ctx1);
            break;
        }
    }
}
int tjs__load_file(JSContext *ctx, DynBuf *dbuf, const char *filename) {
    uv_fs_t req;
    uv_file fd;
    int r;

    r = uv_fs_open(NULL, &req, filename, O_RDONLY, 0, NULL);
    uv_fs_req_cleanup(&req);
    if (r < 0)
        return r;

    fd = r;
    char buf[64 * 1024];
    uv_buf_t b = uv_buf_init(buf, sizeof(buf));
    size_t offset = 0;

    do {
        r = uv_fs_read(NULL, &req, fd, &b, 1, offset, NULL);
        uv_fs_req_cleanup(&req);
        if (r <= 0)
            break;
        offset += r;
        r = dbuf_put(dbuf, (const uint8_t *) b.base, r);
        if (r != 0)
            break;
    } while (1);

    return r;
}
// End From "tuv/vm.c"

static JSValue qjs_tuv_run(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_tuv_run(%p)\n", ctx);
	int mode = 0; // default once nowait
	if ((argc > 0) && JS_ToInt32(ctx, &mode, argv[0])) {
		return JS_EXCEPTION;
	}
	trace("qjs_tuv_run(%p) mode: %d\n", ctx, mode);
  int ret = uv_run(tjs_get_loop(ctx), (uv_run_mode)mode);
	trace("qjs_tuv_run(%p) => %d\n", ctx, ret);
  if (ret < 0) {
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_tuv_funcs[] = {
	JS_CFUNC_DEF("run", 1, qjs_tuv_run),
};

static int qjs_mod_tuv_init(JSContext *ctx, JSModuleDef *m) {
	JS_NewClassID(&qjs_tuv_class_id);
	JS_NewClass(JS_GetRuntime(ctx), qjs_tuv_class_id, &qjs_tuv_class);
	JS_SetModuleExportList(ctx, m, qjs_tuv_funcs, countof(qjs_tuv_funcs));
	return 0;
};

static int qjs_mod_tuv_export(JSContext *ctx, JSModuleDef *m) {
	JS_AddModuleExportList(ctx, m, qjs_tuv_funcs, countof(qjs_tuv_funcs));
	return 0;
};

static int qjs_tuv_init(JSContext *ctx, JSModuleDef *m) {
	qjs_mod_tuv_init(ctx, m);
	//tjs_mod_dns_init(ctx, m);
	tjs_mod_error_init(ctx, m);
	tjs_mod_fs_init(ctx, m);
	tjs_mod_misc_init(ctx, m);
	tjs_mod_process_init(ctx, m);
	//tjs_mod_signals_init(ctx, m);
	//tjs_mod_std_init(ctx, m);
	tjs_mod_streams_init(ctx, m);
	tjs_mod_timers_init(ctx, m);
	tjs_mod_udp_init(ctx, m);
	//tjs_mod_worker_init(ctx, m);
	return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_tuv
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name) {
	JSModuleDef *m;
	m = JS_NewCModule(ctx, module_name, qjs_tuv_init);
	qjs_mod_tuv_export(ctx, m);
	//tjs_mod_dns_export(ctx, m);
	tjs_mod_error_export(ctx, m);
	tjs_mod_fs_export(ctx, m);
	tjs_mod_misc_export(ctx, m);
	tjs_mod_process_export(ctx, m);
	//tjs_mod_std_export(ctx, m);
	tjs_mod_streams_export(ctx, m);
	//tjs_mod_signals_export(ctx, m);
	tjs_mod_timers_export(ctx, m);
	tjs_mod_udp_export(ctx, m);
	//tjs_mod_worker_export(ctx, m);
	return m;
}
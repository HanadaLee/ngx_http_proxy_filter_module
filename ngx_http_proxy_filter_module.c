
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_proxy_filter_module.h>
#include <ngx_http_proxy_module.h>


typedef struct {
    unsigned                finalized:1;
} ngx_http_proxy_filter_request_ctx_t;


static void *ngx_http_proxy_filter_create_main_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_proxy_filter_run_phase(ngx_http_request_t *r,
    ngx_uint_t phase, ngx_http_proxy_filter_ctx_t *ctx, ngx_uint_t *ran);
static ngx_int_t ngx_http_proxy_filter_has_shadow(ngx_chain_t *cl,
    ngx_buf_t *raw);


static ngx_http_module_t  ngx_http_proxy_filter_module_ctx = {
    NULL,                                    /* preconfiguration */
    NULL,                                    /* postconfiguration */

    ngx_http_proxy_filter_create_main_conf,  /* create main configuration */
    NULL,                                    /* init main configuration */

    NULL,                                    /* create server configuration */
    NULL,                                    /* merge server configuration */

    NULL,                                    /* create location configuration */
    NULL                                     /* merge location configuration */
};


ngx_module_t  ngx_http_proxy_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_filter_module_ctx,       /* module context */
    NULL,                                    /* module directives */
    NGX_HTTP_MODULE,                         /* module type */
    NULL,                                    /* init master */
    NULL,                                    /* init module */
    NULL,                                    /* init process */
    NULL,                                    /* init thread */
    NULL,                                    /* exit thread */
    NULL,                                    /* exit process */
    NULL,                                    /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_proxy_filter_create_main_conf(ngx_conf_t *cf)
{
    ngx_uint_t                         i;
    ngx_http_proxy_filter_main_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_filter_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    for (i = 0; i <= NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER; i++) {
        if (ngx_array_init(&conf->phases[i], cf->pool, 1,
                           sizeof(ngx_http_proxy_filter_pt))
            != NGX_OK)
        {
            return NULL;
        }
    }

    return conf;
}


static ngx_int_t
ngx_http_proxy_filter_run_phase(ngx_http_request_t *r, ngx_uint_t phase,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_uint_t *ran)
{
    ngx_uint_t                         i;
    ngx_int_t                          rc;
    ngx_http_proxy_filter_pt          *h;
    ngx_http_proxy_filter_main_conf_t *pmcf;

    if (ran) {
        *ran = 0;
    }

    if (phase > NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER) {
        return NGX_ERROR;
    }

    pmcf = ngx_http_get_module_main_conf(r, ngx_http_proxy_filter_module);
    if (pmcf == NULL || pmcf->phases[phase].nelts == 0) {
        return NGX_OK;
    }

    if (ran) {
        *ran = 1;
    }

    ctx->phase = phase;

    h = pmcf->phases[phase].elts;

    for (i = pmcf->phases[phase].nelts; i > 0; i--) {
        rc = h[i - 1](r, ctx);

        if (rc == NGX_OK) {
            return rc;
        }

        if (rc == NGX_DECLINED) {
            continue;
        }

        return rc;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_access_hook(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_proxy_filter_ctx_t  ctx;

    ngx_memzero(&ctx, sizeof(ngx_http_proxy_filter_ctx_t));

    rc = ngx_http_proxy_filter_run_phase(r, NGX_HTTP_PROXY_ACCESS_FILTER,
                                         &ctx, NULL);

    return rc;
}


ngx_int_t
ngx_http_proxy_filter_get_uri(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_str_t **uri)
{
    ngx_http_upstream_t  *u;

    if (ctx == NULL || uri == NULL
        || ctx->phase == NGX_HTTP_PROXY_ACCESS_FILTER)
    {
        return NGX_ERROR;
    }

    u = r->upstream;

    if (u == NULL) {
        return NGX_ERROR;
    }

    /* already set */
    if (u->uri.data != NULL) {
        *uri = &u->uri;
        return NGX_OK;
    }

    if (ctx->uri_len == 0 || ctx->uri_handler == NULL) {
        return NGX_ERROR;
    }

    u->uri.data = ngx_pnalloc(r->pool, ctx->uri_len);
    if (u->uri.data == NULL) {
        return NGX_ERROR;
    }


    if (ctx->uri_handler(r, &u->uri) != NGX_OK) {
        ngx_str_null(&u->uri);
        return NGX_ERROR;
    }

    *uri = &u->uri;

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_set_uri(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_str_t *uri)
{
    ngx_http_upstream_t  *u;

    if (ctx == NULL || uri == NULL || uri->len == 0 || uri->data == NULL
        || ctx->phase != NGX_HTTP_PROXY_REQUEST_FILTER)
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "proxy filter: invalid uri");
        return NGX_ERROR;
    }

    u = r->upstream;

    if (u == NULL) {
        return NGX_ERROR;
    }

    u->uri = *uri;

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_get_method(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_str_t **method)
{
    ngx_http_upstream_t  *u;

    if (ctx == NULL || method == NULL
        || ctx->phase == NGX_HTTP_PROXY_ACCESS_FILTER)
    {
        return NGX_ERROR;
    }

    u = r->upstream;

    if (u == NULL) {
        return NGX_ERROR;
    }

    *method = &u->method;

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_set_method(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_str_t *method)
{
    ngx_http_upstream_t  *u;

    if (ctx == NULL || method == NULL || method->len == 0
        || method->data == NULL || ctx->phase != NGX_HTTP_PROXY_REQUEST_FILTER)
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "proxy filter: invalid method");
        return NGX_ERROR;
    }

    u = r->upstream;

    if (u == NULL) {
        return NGX_ERROR;
    }

    u->method = *method;
    ctx->method_set = 1;

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_get_host(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx, ngx_str_t **host)
{
    if (ctx == NULL || host == NULL
        || ctx->phase != NGX_HTTP_PROXY_REQUEST_FILTER)
    {
        return NGX_ERROR;
    }

#if (nginx_version >= 1031004 || NGX_RESTY_EXT_1_31_4_BACKPORT)
    {
        ngx_str_t                 *value;
        ngx_http_proxy_ctx_t      *pctx;
        ngx_http_proxy_loc_conf_t *plcf;

        pctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);
        plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

        if (pctx == NULL || plcf == NULL) {
            return NGX_ERROR;
        }

        value = ngx_pnalloc(r->pool, sizeof(ngx_str_t));
        if (value == NULL) {
            return NGX_ERROR;
        }

        value->len = 0;
        value->data = NULL;

        if (plcf->host_value
            && ngx_http_complex_value(r, plcf->host_value, value) != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (plcf->host_value == NULL
            || (value->len == 0 && plcf->http_version == NGX_HTTP_VERSION_11))
        {
            *value = pctx->vars.host_header;
        }

        if (value->len == 0) {
            return NGX_ERROR;
        }

        *host = value;

        return NGX_OK;
    }
#else
    {
        ngx_uint_t       i;
        ngx_list_part_t *part;
        ngx_table_elt_t *h;

        if (ctx->headers == NULL) {
            return NGX_ERROR;
        }

        part = &ctx->headers->part;
        h = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                h = part->elts;
                i = 0;
            }

            if (h[i].hash == 0) {
                continue;
            }

            if (h[i].key.len == sizeof("Host") - 1
                && ngx_strncasecmp(h[i].key.data, (u_char *) "Host",
                                   sizeof("Host") - 1)
                   == 0)
            {
                *host = &h[i].value;
                return NGX_OK;
            }
        }

        return NGX_ERROR;
    }
#endif
}


ngx_int_t
ngx_http_proxy_filter_request_hook(ngx_http_request_t *r, ngx_list_t *headers,
    size_t uri_len, ngx_http_proxy_filter_uri_pt uri_handler,
    ngx_uint_t *method_set)
{
    ngx_int_t                    rc;
    ngx_http_proxy_filter_ctx_t  ctx;

    ngx_memzero(&ctx, sizeof(ngx_http_proxy_filter_ctx_t));
    ctx.headers = headers;
    ctx.uri_len = uri_len;
    ctx.uri_handler = uri_handler;

    rc = ngx_http_proxy_filter_run_phase(r, NGX_HTTP_PROXY_REQUEST_FILTER,
                                         &ctx, NULL);

    if (method_set) {
        *method_set = ctx.method_set;
    }

    if (rc == NGX_OK) {
        return NGX_OK;
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_http_proxy_filter_header_hook(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_upstream_t         *u;
    ngx_http_proxy_filter_ctx_t  ctx;

    u = r->upstream;

    if (u == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(&ctx, sizeof(ngx_http_proxy_filter_ctx_t));
    ctx.headers = &u->headers_in.headers;
    ctx.headers_in = &u->headers_in;

    rc = ngx_http_proxy_filter_run_phase(r, NGX_HTTP_PROXY_HEADER_FILTER,
                                         &ctx, NULL);

    if (rc != NGX_OK) {
        if (rc == NGX_ERROR) {
            return rc;
        }

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_proxy_filter_body_hook(ngx_http_request_t *r, ngx_chain_t **body)
{
    ngx_int_t                    rc;
    ngx_http_proxy_filter_ctx_t  ctx;

    ngx_memzero(&ctx, sizeof(ngx_http_proxy_filter_ctx_t));
    ctx.body = body;

    rc = ngx_http_proxy_filter_run_phase(r, NGX_HTTP_PROXY_BODY_FILTER,
                                         &ctx, NULL);

    if (rc == NGX_OK) {
        return NGX_OK;
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_http_proxy_filter_event_pipe_body_hook(ngx_http_request_t *r,
    ngx_event_pipe_t *p, ngx_chain_t **body, ngx_buf_t *raw)
{
    ngx_int_t  rc;

    rc = ngx_http_proxy_filter_body_hook(r, body);
    if (rc != NGX_OK) {
        return rc;
    }

    if (raw && raw->shadow
        && ngx_http_proxy_filter_has_shadow(*body, raw) != NGX_OK)
    {
        if (ngx_event_pipe_add_free_buf(p, raw) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


void
ngx_http_proxy_filter_finalize_hook(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_uint_t                           phase;
    ngx_http_proxy_filter_ctx_t          ctx;
    ngx_http_proxy_filter_request_ctx_t *rctx;

    rctx = ngx_http_get_module_ctx(r, ngx_http_proxy_filter_module);

    if (rctx && rctx->finalized) {
        return;
    }

    if (rctx == NULL) {
        rctx = ngx_pcalloc(r->pool,
                           sizeof(ngx_http_proxy_filter_request_ctx_t));
        if (rctx == NULL) {
            return;
        }

        ngx_http_set_ctx(r, rctx, ngx_http_proxy_filter_module);
    }

    rctx->finalized = 1;

    ngx_memzero(&ctx, sizeof(ngx_http_proxy_filter_ctx_t));
    ctx.rc = rc;

    phase = NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER;

    (void) ngx_http_proxy_filter_run_phase(r, phase, &ctx, NULL);
}


void
ngx_http_proxy_filter_update_chain_last(ngx_chain_t **chain,
    ngx_chain_t ***last)
{
    while (*chain) {
        chain = &(*chain)->next;
    }

    *last = chain;
}


static ngx_int_t
ngx_http_proxy_filter_has_shadow(ngx_chain_t *cl, ngx_buf_t *raw)
{
    ngx_buf_t  *first, *last;

    first = NULL;
    last = NULL;

    for ( /* void */ ; cl; cl = cl->next) {
        if (cl->buf && cl->buf->shadow == raw) {
            if (first == NULL) {
                first = cl->buf;
            }

            last = cl->buf;
        }
    }

    if (last) {
        raw->shadow = first;
        last->last_shadow = 1;
        return NGX_OK;
    }

    return NGX_DECLINED;
}

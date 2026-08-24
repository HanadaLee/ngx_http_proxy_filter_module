/*
 * Test consumer for ngx_http_proxy_filter_module.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_proxy_filter_module.h>


typedef struct {
    ngx_flag_t    enabled;
    ngx_uint_t    access_status;
    ngx_str_t     uri;
    ngx_str_t     method;
    ngx_flag_t    stop;
    ngx_flag_t    response_header;
    ngx_flag_t    body_uppercase;
    ngx_flag_t    finalize;
} ngx_http_proxy_filter_test_loc_conf_t;


static ngx_int_t ngx_http_proxy_filter_test_init(ngx_conf_t *cf);
static void *ngx_http_proxy_filter_test_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_proxy_filter_test_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static ngx_int_t ngx_http_proxy_filter_test_access(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_request_first(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_request_second(
    ngx_http_request_t *r, ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_header(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_body(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_finalize(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
static ngx_int_t ngx_http_proxy_filter_test_add_header(ngx_http_request_t *r,
    ngx_list_t *headers, ngx_str_t *key, ngx_str_t *value);


static ngx_str_t  ngx_http_proxy_filter_test_first_name =
    ngx_string("X-Proxy-Filter-First");
static ngx_str_t  ngx_http_proxy_filter_test_second_name =
    ngx_string("X-Proxy-Filter-Second");
static ngx_str_t  ngx_http_proxy_filter_test_host_name =
    ngx_string("X-Proxy-Filter-Host");
static ngx_str_t  ngx_http_proxy_filter_test_uri_name =
    ngx_string("X-Proxy-Filter-Original-Uri");
static ngx_str_t  ngx_http_proxy_filter_test_response_name =
    ngx_string("X-Proxy-Filter-Response");
static ngx_str_t  ngx_http_proxy_filter_test_one = ngx_string("1");
static ngx_str_t  ngx_http_proxy_filter_test_set = ngx_string("set");


static ngx_command_t  ngx_http_proxy_filter_test_commands[] = {

    { ngx_string("proxy_filter_test"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, enabled),
      NULL },

    { ngx_string("proxy_filter_test_access_status"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, access_status),
      NULL },

    { ngx_string("proxy_filter_test_uri"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, uri),
      NULL },

    { ngx_string("proxy_filter_test_method"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, method),
      NULL },

    { ngx_string("proxy_filter_test_stop"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, stop),
      NULL },

    { ngx_string("proxy_filter_test_response_header"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, response_header),
      NULL },

    { ngx_string("proxy_filter_test_body_uppercase"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, body_uppercase),
      NULL },

    { ngx_string("proxy_filter_test_finalize"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_filter_test_loc_conf_t, finalize),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_filter_test_module_ctx = {
    NULL,                                           /* preconfiguration */
    ngx_http_proxy_filter_test_init,                /* postconfiguration */

    NULL,                                           /* create main conf */
    NULL,                                           /* init main conf */

    NULL,                                           /* create server conf */
    NULL,                                           /* merge server conf */

    ngx_http_proxy_filter_test_create_loc_conf,     /* create location conf */
    ngx_http_proxy_filter_test_merge_loc_conf       /* merge location conf */
};


ngx_module_t  ngx_http_proxy_filter_test_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_filter_test_module_ctx,         /* module context */
    ngx_http_proxy_filter_test_commands,            /* module directives */
    NGX_HTTP_MODULE,                                /* module type */
    NULL,                                           /* init master */
    NULL,                                           /* init module */
    NULL,                                           /* init process */
    NULL,                                           /* init thread */
    NULL,                                           /* exit thread */
    NULL,                                           /* exit process */
    NULL,                                           /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_proxy_filter_test_access(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_http_proxy_filter_test_loc_conf_t  *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled || ptcf->access_status == 0) {
        return NGX_DECLINED;
    }

    return ptcf->access_status;
}


static ngx_int_t
ngx_http_proxy_filter_test_request_first(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_http_proxy_filter_test_loc_conf_t  *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled) {
        return NGX_DECLINED;
    }

    if (ngx_http_proxy_filter_test_add_header(r, ctx->headers,
            &ngx_http_proxy_filter_test_first_name,
            &ngx_http_proxy_filter_test_one)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_filter_test_request_second(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_str_t                                  *host, *uri;
    ngx_http_proxy_filter_test_loc_conf_t      *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled) {
        return NGX_DECLINED;
    }

    if (ngx_http_proxy_filter_get_host(r, ctx, &host) != NGX_OK
        || ngx_http_proxy_filter_get_uri(r, ctx, &uri) != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_http_proxy_filter_test_add_header(r, ctx->headers,
            &ngx_http_proxy_filter_test_second_name,
            &ngx_http_proxy_filter_test_one)
        != NGX_OK
        || ngx_http_proxy_filter_test_add_header(r, ctx->headers,
            &ngx_http_proxy_filter_test_host_name, host)
           != NGX_OK
        || ngx_http_proxy_filter_test_add_header(r, ctx->headers,
            &ngx_http_proxy_filter_test_uri_name, uri)
           != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ptcf->uri.len
        && ngx_http_proxy_filter_set_uri(r, ctx, &ptcf->uri) != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ptcf->method.len
        && ngx_http_proxy_filter_set_method(r, ctx, &ptcf->method) != NGX_OK)
    {
        return NGX_ERROR;
    }

    return ptcf->stop ? NGX_OK : NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_filter_test_header(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_http_proxy_filter_test_loc_conf_t  *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled || !ptcf->response_header) {
        return NGX_DECLINED;
    }

    if (ngx_http_proxy_filter_test_add_header(r, ctx->headers,
            &ngx_http_proxy_filter_test_response_name,
            &ngx_http_proxy_filter_test_set)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_filter_test_body(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    u_char                                    *p;
    ngx_buf_t                                 *b;
    ngx_chain_t                               *cl;
    ngx_http_proxy_filter_test_loc_conf_t     *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled || !ptcf->body_uppercase) {
        return NGX_DECLINED;
    }

    for (cl = *ctx->body; cl; cl = cl->next) {
        b = cl->buf;

        if (b == NULL || !ngx_buf_in_memory(b)) {
            continue;
        }

        for (p = b->pos; p < b->last; p++) {
            *p = ngx_toupper(*p);
        }
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_filter_test_finalize(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    ngx_http_proxy_filter_test_loc_conf_t  *ptcf;

    ptcf = ngx_http_get_module_loc_conf(r,
                                        ngx_http_proxy_filter_test_module);

    if (!ptcf->enabled || !ptcf->finalize) {
        return NGX_DECLINED;
    }

    ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                  "proxy filter test finalized: %V %i", &r->uri, ctx->rc);

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_filter_test_add_header(ngx_http_request_t *r,
    ngx_list_t *headers, ngx_str_t *key, ngx_str_t *value)
{
    ngx_table_elt_t  *h;

    h = ngx_list_push(headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    ngx_memzero(h, sizeof(ngx_table_elt_t));

    h->hash = ngx_hash_key_lc(key->data, key->len);
    h->key = *key;
    h->value = *value;

    h->lowcase_key = ngx_pnalloc(r->pool, key->len);
    if (h->lowcase_key == NULL) {
        h->hash = 0;
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, key->data, key->len);

    return NGX_OK;
}


static void *
ngx_http_proxy_filter_test_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_filter_test_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                       sizeof(ngx_http_proxy_filter_test_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enabled = NGX_CONF_UNSET;
    conf->access_status = NGX_CONF_UNSET_UINT;
    conf->stop = NGX_CONF_UNSET;
    conf->response_header = NGX_CONF_UNSET;
    conf->body_uppercase = NGX_CONF_UNSET;
    conf->finalize = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_proxy_filter_test_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child)
{
    ngx_http_proxy_filter_test_loc_conf_t *prev = parent;
    ngx_http_proxy_filter_test_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enabled, prev->enabled, 0);
    ngx_conf_merge_uint_value(conf->access_status, prev->access_status, 0);
    ngx_conf_merge_str_value(conf->uri, prev->uri, "");
    ngx_conf_merge_str_value(conf->method, prev->method, "");
    ngx_conf_merge_value(conf->stop, prev->stop, 0);
    ngx_conf_merge_value(conf->response_header, prev->response_header, 0);
    ngx_conf_merge_value(conf->body_uppercase, prev->body_uppercase, 0);
    ngx_conf_merge_value(conf->finalize, prev->finalize, 0);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_filter_test_init(ngx_conf_t *cf)
{
    ngx_http_proxy_filter_pt           *h;
    ngx_http_proxy_filter_main_conf_t  *pmcf;

    pmcf = ngx_http_conf_get_module_main_conf(cf,
                                              ngx_http_proxy_filter_module);
    if (pmcf == NULL) {
        return NGX_ERROR;
    }

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_ACCESS_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_access;

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_REQUEST_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_request_first;

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_REQUEST_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_request_second;

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_HEADER_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_header;

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_BODY_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_body;

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_proxy_filter_test_finalize;

    return NGX_OK;
}

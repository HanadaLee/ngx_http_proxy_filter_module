# ngx_http_proxy_filter_module

`ngx_http_proxy_filter_module` is an nginx C module that provides a hook-based filter framework for proxy requests. It allows third-party modules to register custom filters at various phases of the upstream proxy lifecycle.

## Features

- **Multi-Phase Filtering**: Register filters at distinct stages of the proxy request lifecycle:
  - **Access Filter** — Execute access control checks before forwarding the request.
  - **Request Filter** — Inspect or modify upstream request headers.
  - **Header Filter** — Inspect or modify upstream response headers.
  - **Body Filter** — Inspect or modify upstream response body chunks.
  - **Finalize Request Filter** — Execute cleanup or logging logic when the request is finalized.

- **Chain-Aware Body Processing**: Safe handling of nginx buffer chains and shadow buffers during body filtering.

- **Header Refresh Support**: Allows response headers to be dynamically refreshed and re-parsed by nginx after modification.

## Requirements

- The nginx version must be 1.31.1 or higher.
- This module requires the `NGX_RESTY_EXT` macro to be defined by nginx core patch (https://git.hanada.info/hanada/openresty/-/tree/main/patches). 
- Must be compiled as a **static module** (`--add-module`), dynamic module (`--add-dynamic-module`) is not supported.

## Installation

Before use this module, you must apply the nginx core patch:
```bash
cd /path/to/nginx-1.xx.x
patch -p1 < nginx-ext_1.xx.x+.patch
```
The patch is from https://git.hanada.info/hanada/openresty/-/tree/main/patches

Compile nginx or OpenResty with this module:

```bash
cd /path/to/nginx-1.xx.x
./configure \
    --add-module=/path/to/ngx_http_proxy_filter_module \
    ...
make
make install
```

## API for Third-Party Modules

Include the header file:

```c
#include <ngx_http_proxy_filter_module.h>
```

### Register a Filter

```c
ngx_int_t ngx_http_proxy_filter_register(ngx_conf_t *cf, ngx_uint_t phase,
    ngx_http_proxy_filter_pt handler);
```

| Parameter | Description |
|-----------|-------------|
| `cf` | nginx configuration pointer. |
| `phase` | One of the `ngx_http_proxy_filter_phase_e` values. |
| `handler` | Your callback function. |

### Filter Phases

```c
typedef enum {
    NGX_HTTP_PROXY_ACCESS_FILTER = 0,      /* Access control */
    NGX_HTTP_PROXY_REQUEST_FILTER,         /* Upstream request headers */
    NGX_HTTP_PROXY_HEADER_FILTER,          /* Upstream response headers */
    NGX_HTTP_PROXY_BODY_FILTER,            /* Upstream response body */
    NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER,/* Request finalization */
    NGX_HTTP_PROXY_FILTER_PHASES
} ngx_http_proxy_filter_phase_e;
```

### Filter Handler Signature

```c
typedef ngx_int_t (*ngx_http_proxy_filter_pt)(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx);
```

The `ctx` structure provides context for the current phase:

| Field | Available In | Description |
|-------|--------------|-------------|
| `phase` | All | Current phase identifier. |
| `headers` | Request / Header | Pointer to the headers list. |
| `headers_in` | Header | Pointer to `ngx_http_upstream_headers_in_t`. |
| `body` | Body | Pointer to the response body chain. |
| `rc` | Finalize | The finalization status code. |
| `refresh_headers` | Header | Set to `1` to trigger upstream response headers re-parsing after modification. |

Return `NGX_OK` or `NGX_DECLINED` to continue processing, or any other value to abort with that status.

## Example

```c
#include <ngx_http_proxy_filter_module.h>

static ngx_int_t my_header_filter(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    /* Inspect or modify upstream response headers */
    return NGX_OK;
}

static ngx_int_t my_module_init(ngx_conf_t *cf)
{
    return ngx_http_proxy_filter_register(cf, NGX_HTTP_PROXY_HEADER_FILTER,
                                          my_header_filter);
}
```

## Copyright

Copyright (C) Hanada

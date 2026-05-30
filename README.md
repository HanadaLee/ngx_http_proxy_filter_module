# ngx_http_proxy_filter_module

`ngx_http_proxy_filter_module` is an nginx C module that provides a hook-based filter framework for proxy requests. It allows third-party modules to attach custom handlers at various phases of the upstream proxy lifecycle.

## Features

- **Multi-Phase Filtering**: Attach handlers at distinct stages of the proxy request lifecycle:
  - **Access Filter** — Execute access control checks before forwarding the request.
  - **Request Filter** — Inspect or modify upstream request headers.
  - **Header Filter** — Inspect or modify upstream response headers.
  - **Body Filter** — Inspect or modify upstream response body chunks.
  - **Finalize Request Filter** — Execute cleanup or logging logic when the request is finalized.

- **Nginx-Style Phase Semantics**: Handlers follow the familiar nginx return-code conventions (`NGX_OK`, `NGX_DECLINED`, error propagation).

- **Chain-Aware Body Processing**: Safe handling of nginx buffer chains and shadow buffers during body filtering.

- **Header Refresh Support**: Allows response headers to be dynamically refreshed and re-parsed by nginx after modification.

## Requirements

- nginx with **HTTP proxy module** enabled.
- **OpenResty / NGX_RESTY_EXT**: This module requires the `NGX_RESTY_EXT` macro to be defined.
- Must be compiled as a **static module** (`--add-module`), dynamic module (`--add-dynamic-module`) is not supported.

## Installation

Compile nginx or OpenResty with this module:

```bash
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

### Attach a Handler

Like nginx request phases, handlers are appended by pushing into the proxy
filter main conf phase array during `postconfiguration`.

### Filter Phases

```c
typedef enum {
    NGX_HTTP_PROXY_ACCESS_FILTER = 0,      /* Access control */
    NGX_HTTP_PROXY_REQUEST_FILTER,         /* Upstream request headers */
    NGX_HTTP_PROXY_HEADER_FILTER,          /* Upstream response headers */
    NGX_HTTP_PROXY_BODY_FILTER,            /* Upstream response body */
    NGX_HTTP_PROXY_FINALIZE_REQUEST_FILTER /* Request finalization */
} ngx_http_proxy_filter_phase_e;
```

### Handler Signature

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
| `refresh_headers` | Header | Set to `1` to trigger header re-parsing after modification. |

### Handler Return Value Semantics

Handlers follow nginx phase semantics:

| Return Value | Behaviour |
|--------------|-----------|
| `NGX_OK` | **Finishes** the current proxy phase immediately. No subsequent handlers in the same phase are executed. |
| `NGX_DECLINED` | **Advances** to the next handler in the same proxy phase. |
| Other values | **Propagated** to the proxy hook caller, aborting further processing in this phase. |

A proxy phase with no handlers registered, or where all handlers decline, completes with `NGX_OK`.

## Example

```c
#include <ngx_http_proxy_filter_module.h>

static ngx_int_t my_header_filter(ngx_http_request_t *r,
    ngx_http_proxy_filter_ctx_t *ctx)
{
    /* Inspect or modify upstream response headers */

    /* Return NGX_OK to finish this phase immediately */
    return NGX_OK;

    /* Or return NGX_DECLINED to let the next handler run */
    /* return NGX_DECLINED; */
}

static ngx_int_t my_module_init(ngx_conf_t *cf)
{
    ngx_http_proxy_filter_pt          *h;
    ngx_http_proxy_filter_main_conf_t *pmcf;

    pmcf = ngx_http_conf_get_module_main_conf(cf,
                                              ngx_http_proxy_filter_module);
    if (pmcf == NULL) {
        return NGX_ERROR;
    }

    h = ngx_array_push(&pmcf->phases[NGX_HTTP_PROXY_HEADER_FILTER]);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = my_header_filter;

    return NGX_OK;
}
```

## Copyright

Copyright (C) Hanada

#!/usr/bin/env bash

set -euo pipefail

nginx_source="$(cd "${1:?Usage: run-tests.sh <nginx-source> <nginx-tests> [expr-source]}" && pwd)"
nginx_tests="$(cd "${2:?nginx-tests checkout is required}" && pwd)"
module_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

configure_args=(
    --prefix="$nginx_source/test-prefix"
    --with-debug
    --with-http_ssl_module
    --with-http_v2_module
)

if [[ "${TEST_CORE:-nginx}" == without-ext ]]; then
    configure_args+=(--without-ext)
fi

export TEST_NGINX_EXPR=0
if [[ "${TEST_EXPR:-disabled}" == enabled ]]; then
    expr_source="$(cd "${3:?ngx_expr_module checkout is required}" && pwd)"
    configure_args+=(--with-http_json_module --add-module="$expr_source")
    export TEST_NGINX_EXPR=1
fi

bash "$module_root/util/apply-nginx-patches.sh" "$nginx_source"
configure_args+=(--add-module="$module_root" --add-module="$module_root/t")

cd "$nginx_source"
configure=./configure
[[ -f "$configure" ]] || configure=auto/configure
sh "$configure" "${configure_args[@]}"
make -j"${TEST_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"

export TEST_NGINX_UPSTREAM_EXPR=0
if [[ "$TEST_NGINX_EXPR" == 1 ]] && grep -q '^#define NGX_EXT  *1$' objs/ngx_auto_config.h; then
    export TEST_NGINX_UPSTREAM_EXPR=1
fi

export TEST_NGINX_BINARY="$nginx_source/objs/nginx"
export PERL5LIB="$nginx_tests/lib"
if [[ "$(id -u)" == 0 ]]; then
    export TEST_NGINX_GLOBALS="${TEST_NGINX_GLOBALS:-user root;}"
fi
"$TEST_NGINX_BINARY" -V
cd "$module_root"
prove -v t/proxy_filter.t

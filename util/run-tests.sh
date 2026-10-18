#!/usr/bin/env bash

set -euo pipefail

nginx_source="$(cd "${1:?Usage: run-tests.sh <nginx-source> <nginx-tests> <expr-source>}" && pwd)"
nginx_tests="$(cd "${2:?nginx-tests checkout is required}" && pwd)"
expr_source="$(cd "${3:?ngx_expr_module checkout is required}" && pwd)"
module_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$nginx_source"
./auto/configure \
    --prefix="$nginx_source/test-prefix" \
    --with-debug \
    --with-http_ssl_module \
    --with-http_v2_module \
    --with-http_json_module \
    --add-module="$expr_source" \
    --add-module="$module_root" \
    --add-module="$module_root/t"
make -j"${TEST_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"

export TEST_NGINX_BINARY="$nginx_source/objs/nginx"
export PERL5LIB="$nginx_tests/lib"
if [[ "$(id -u)" == 0 ]]; then
    export TEST_NGINX_GLOBALS="${TEST_NGINX_GLOBALS:-user root;}"
fi
"$TEST_NGINX_BINARY" -V
cd "$module_root"
prove -v t/proxy_filter.t

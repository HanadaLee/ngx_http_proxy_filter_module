#!/usr/bin/perl

# End-to-end tests for the proxy filter hook framework.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use Test::Nginx qw/ :DEFAULT http_content /;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http proxy rewrite http_v2
	ngx_http_proxy_filter_module/)
	->plan(28);

my $core_expr = $ENV{TEST_NGINX_UPSTREAM_EXPR} // 0;
my $config = <<'EOF';

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    upstream retry_backend {
        server 127.0.0.1:8083 max_fails=0;
        server 127.0.0.1:8081 backup;
    }

    server {
        listen 127.0.0.1:8083;
        http2 on;
        return 502;
    }

    server {
        listen       127.0.0.1:8081;
        server_name  backend;
        http2 on;

        location = /body {
            return 200 lower-body;
        }

        location / {
            return 200 "$request_method|$request_uri|$http_x_proxy_filter_host|$http_x_proxy_filter_original_uri|$http_x_proxy_filter_first|$http_x_proxy_filter_second";
        }
    }

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        %%CORE_EXPR%%

        location = /basic {
            proxy_filter_test on;
            proxy_set_header Host custom.example;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /default-host {
            proxy_filter_test on;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /rewrite {
            proxy_filter_test on;
            proxy_filter_test_uri /changed?z=1;
            proxy_filter_test_method POST;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /stop {
            proxy_filter_test on;
            proxy_filter_test_stop on;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /access {
            proxy_filter_test on;
            proxy_filter_test_access_status 418;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /header {
            proxy_filter_test on;
            proxy_filter_test_response_header on;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /body-buffered {
            proxy_filter_test on;
            proxy_filter_test_body_uppercase on;
            proxy_pass http://127.0.0.1:8081/body;
        }

        location = /body-unbuffered {
            proxy_filter_test on;
            proxy_filter_test_body_uppercase on;
            proxy_buffering off;
            proxy_pass http://127.0.0.1:8081/body;
        }

        location = /disabled {
            proxy_pass http://127.0.0.1:8081;
        }

        location = /empty-host-http11 {
            proxy_filter_test on;
            proxy_set_header Host "";
            proxy_http_version 1.1;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /empty-host-http10 {
            proxy_filter_test on;
            proxy_set_header Host "";
            proxy_http_version 1.0;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /h2-basic {
            proxy_filter_test on;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /h2-rewrite {
            proxy_filter_test on;
            proxy_filter_test_uri /changed?z=1;
            proxy_filter_test_method POST;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /h2-body-buffered {
            proxy_filter_test on;
            proxy_filter_test_body_uppercase on;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081/body;
        }

        location = /h2-body-unbuffered {
            proxy_filter_test on;
            proxy_filter_test_body_uppercase on;
            proxy_buffering off;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081/body;
        }

        location = /h2-header {
            proxy_filter_test on;
            proxy_filter_test_response_header on;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /h2-finalize {
            proxy_filter_test on;
            proxy_filter_test_finalize on;
            proxy_http_version 2;
            proxy_pass http://127.0.0.1:8081;
        }

        %%CONDITIONAL_PROXY%%

        location = /retry {
            proxy_filter_test on;
            proxy_filter_test_finalize on;
            proxy_next_upstream http_502;
            add_header X-Upstream-Attempts $upstream_status always;
            proxy_pass http://retry_backend;
        }

        location = /h2-retry {
            proxy_filter_test on;
            proxy_filter_test_finalize on;
            proxy_http_version 2;
            proxy_next_upstream http_502;
            add_header X-Upstream-Attempts $upstream_status always;
            proxy_pass http://retry_backend;
        }

        location = /finalize {
            proxy_filter_test on;
            proxy_filter_test_finalize on;
            proxy_pass http://127.0.0.1:8081;
        }
    }
}

EOF

my $conditional = $core_expr ? <<'EOF' : '';
        location = /conditional-host {
            proxy_filter_test on;
            proxy_set_header Host "";

            when use_http_11 {
                proxy_http_version 1.1;
            }

            proxy_http_version 1.0;
            proxy_pass http://127.0.0.1:8081;
        }

EOF
$config =~ s/%%CONDITIONAL_PROXY%%/$conditional/;
my $expression = $core_expr ? 'expr use_http_11 str_eq $arg_mode http11;' : '';
$config =~ s/%%CORE_EXPR%%/$expression/;
$t->write_file_expand('nginx.conf', $config);
$t->run();

###############################################################################

is(body('/basic'), 'GET|/basic|custom.example|/basic|1|1',
	'request phase exposes Host and URI and runs handlers in reverse order');
like(body('/default-host'),
	qr/^GET\|\/default-host\|127\.0\.0\.1:8081\|\/default-host\|1\|1$/,
	'get_host falls back to the proxy upstream host');
is(body('/rewrite'), 'POST|/changed?z=1|127.0.0.1:8081|/rewrite|1|1',
	'request phase can replace method and URI');
is(body('/stop'), 'GET|/stop|127.0.0.1:8081|/stop||1',
	'NGX_OK stops earlier handlers in the same phase');

like(response('/access'), qr/^HTTP\/1\.1 418 /,
	'access phase status aborts proxying');
like(response('/header'), qr/^X-Proxy-Filter-Response: set\x0d?$/mi,
	'header phase adds an upstream response header');

is(body('/body-buffered'), 'LOWER-BODY',
	'body phase transforms buffered response chunks');
is(body('/body-unbuffered'), 'LOWER-BODY',
	'body phase transforms non-buffered response chunks');

is(body('/disabled'), 'GET|/disabled||||',
	'disabled test consumer leaves proxying unchanged');
SKIP: {
	skip 'requires conditional core upstream configuration', 2 unless $core_expr;
	like(body('/conditional-host?mode=http11'),
		qr/^GET\|\/conditional-host\?mode=http11\|127\.0\.0\.1:8081\|/,
		'conditional HTTP/1.1 selection supplies the fallback authority');
	like(response('/conditional-host?mode=http10'), qr/^HTTP\/1\.1 500 /,
		'empty HTTP/1.0 Host is rejected by get_host');
}

is(body('/finalize'), 'GET|/finalize|127.0.0.1:8081|/finalize|1|1',
	'finalized request still returns its upstream response');

like(body('/empty-host-http11'), qr/127\.0\.0\.1:8081/,
    'HTTP/1.1 supplies the fallback authority');
like(response('/empty-host-http10'), qr/^HTTP\/1\.1 500 /,
    'empty HTTP/1.0 Host is rejected');
is(body('/h2-basic'), 'GET|/h2-basic|127.0.0.1:8081|/h2-basic|1|1',
    'HTTP/2 upstream runs request filters');
is(body('/h2-rewrite'), 'POST|/changed?z=1|127.0.0.1:8081|/h2-rewrite|1|1',
    'HTTP/2 upstream replaces method and URI');
is(body('/h2-body-buffered'), 'LOWER-BODY', 'HTTP/2 buffered body filter');
is(body('/h2-body-unbuffered'), 'LOWER-BODY', 'HTTP/2 non-buffered body filter');
like(response('/h2-header'), qr/^X-Proxy-Filter-Response: set\x0d?$/mi,
    'HTTP/2 response header filter');
is(body('/h2-finalize'), 'GET|/h2-finalize|127.0.0.1:8081|/h2-finalize|1|1',
    'HTTP/2 finalized response');

my $retry = response('/retry');
like($retry, qr/^X-Upstream-Attempts: 502, 200\x0d?$/mi,
	'HTTP/1 retries the failed upstream');
is(http_content($retry), 'GET|/retry|retry_backend|/retry|1|1',
	'HTTP/1 retry preserves the filtered request');

my $h2_retry = response('/h2-retry');
like($h2_retry, qr/^X-Upstream-Attempts: 502, 200\x0d?$/mi,
	'HTTP/2 retries the failed upstream');
is(http_content($h2_retry), 'GET|/h2-retry|retry_backend|/h2-retry|1|1',
	'HTTP/2 retry preserves the filtered request');

my $log = $t->read_file('error.log');
my @finalized = $log =~ /proxy filter test finalized: \/finalize /g;
is(scalar @finalized, 1, 'finalize phase runs once per request');
@finalized = $log =~ /proxy filter test finalized: \/h2-finalize /g;
is(scalar @finalized, 1, 'HTTP/2 finalize phase runs once per request');

@finalized = $log =~ /proxy filter test finalized: \/retry /g;
is(scalar @finalized, 1, 'HTTP/1 retry finalizes once');
@finalized = $log =~ /proxy filter test finalized: \/h2-retry /g;
is(scalar @finalized, 1, 'HTTP/2 retry finalizes once');

###############################################################################

sub body {
	return http_content(response(@_));
}


sub response {
	my ($uri) = @_;

	return http(<<EOF);
GET $uri HTTP/1.1
Host: localhost
Connection: close

EOF
}

###############################################################################

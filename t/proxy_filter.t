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

my $t = Test::Nginx->new()->has(qw/http proxy rewrite ngx_expr_module
	ngx_http_proxy_filter_module/)
	->plan(13);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    server {
        listen       127.0.0.1:8081;
        server_name  backend;

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

        expr use_http_11 str_eq $arg_mode http11;

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

        location = /conditional-host {
            proxy_filter_test on;
            proxy_set_header Host "";

            when use_http_11 {
                proxy_http_version 1.1;
            }

            proxy_http_version 1.0;
            proxy_pass http://127.0.0.1:8081;
        }

        location = /finalize {
            proxy_filter_test on;
            proxy_filter_test_finalize on;
            proxy_pass http://127.0.0.1:8081;
        }
    }
}

EOF

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
like(body('/conditional-host?mode=http11'),
	qr/^GET\|\/conditional-host\?mode=http11\|127\.0\.0\.1:8081\|/,
	'conditional HTTP/1.1 selection supplies the fallback authority');
like(response('/conditional-host?mode=http10'), qr/^HTTP\/1\.1 500 /,
	'empty HTTP/1.0 Host is rejected by get_host');

is(body('/finalize'), 'GET|/finalize|127.0.0.1:8081|/finalize|1|1',
	'finalized request still returns its upstream response');

my $log = $t->read_file('error.log');
my @finalized = $log =~ /proxy filter test finalized: \/finalize /g;
is(scalar @finalized, 1, 'finalize phase runs once per request');

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

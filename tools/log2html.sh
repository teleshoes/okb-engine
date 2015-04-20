#! /bin/bash -e

MAX_DAYS=8

set -o pipefail

[ -f "$HOME/.okboard-test" ] && . $HOME/.okboard-test

log_dir=${OKBOARD_LOG_DIR:-/tmp/okboard-logs}
html_dir=${OKBOARD_HTML_DIR:-/tmp/okboard-html}

mkdir -p "$html_dir"

find "$log_dir/" -name '*.bz2' -mtime -$MAX_DAYS | xargs -r lbzip2 -dc | `dirname "$0"`/log2html.py "$html_dir/"
firefox "$html_dir/index.html"
find "$html_dir/" -mtime +$MAX_DAYS -type f -delete

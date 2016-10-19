#!/usr/bin/env zsh

emulate -L zsh

print_headers() {
        print "HTTP/1.0 200 OK
Server: OpenBTTN
Connection: close
Content-Type: $1
Content-Length: $2

"
}

SCRIPT=${0:A:h}
TMP=$(mktemp -d -t openbttn)
path=($SCRIPT/node_modules/.bin $path)

(cd -q $SCRIPT; cp *.html *.js $TMP)
(cd -q $TMP;
        uglifyjs --quotes 3 src.js -o src.js
        cat src.js
        html-inline -i index.html -o index2.html

        chars=$(wc -c index2.html | awk '{print $1}')
        {
                print_headers text/html $chars
                cat index2.html
        } > index.html

        print
        cat index.html

        print
        xxd -i index.html | pbcopy
        pbpaste
)


rm -r $TMP

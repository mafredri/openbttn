#!/usr/bin/env zsh

emulate -L zsh

SCRIPT=${0:A:h}
TMP=$(mktemp -d -t openbttn)
path=($SCRIPT/node_modules/.bin $path)

(cd -q $SCRIPT; cp *.html *.js *.css $TMP)
(cd -q $TMP;
	for js in *.js; do
		uglifyjs --enclose --mangle --compress --quotes 3 $js -o $js
		cat $js
	done

	for html in *.html; do
		html-inline -i $html -o $html.2
		mv $html.2 $html

		html-minifier \
			--html5 \
			--collapse-whitespace \
			--remove-attribute-quotes \
			--remove-comments \
			--minify-css \
			--output $html.2 \
			$html
		mv $html.2 $html

		print
		cat $html
	done

	print

	data_c=''
	data_h_size=''
	data_h_extern=''
	for html in *.html; do
		gzip -9 $html
		xxd -i $html.gz > $html.xxd
		grep -v "^unsigned int" $html.xxd >> $html.c

		name=$(head -n1 $html.c)
		name=${(j..)${(Cs._.)${${name#unsigned char }%\[*}}}
		name="const uint8_t g_Data${name}[]"
		data_c+="$(print "$name = {"; tail -n +2 $html.c)"$'\n'
		data_h_extern+="extern ${name};"$'\n'

		size=$(grep "^unsigned int" $html.xxd)
		sizeNum=${${size#*\= }%;}
		size=${(U)${${size#unsigned int }%_len \=*}}
		data_h_size+="#define DATA_${size}_LENGTH ${sizeNum}"$'\n'
	done

	cat <<EOF > $SCRIPT/../../src/data.c
#include <stdint.h>

#include "data.h"

${data_c}
EOF
	cat <<EOF > $SCRIPT/../../src/data.h
#ifndef DATA_H
#define DATA_H

#include <stdint.h>

${data_h_size}
${data_h_extern}
#endif /* DATA_H */
EOF
)

rm -r $TMP

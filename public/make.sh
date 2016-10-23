#!/usr/bin/env zsh

emulate -L zsh

SCRIPT=${0:A:h}
TMP=$(mktemp -d -t openbttn)
path=($SCRIPT/node_modules/.bin $path)

(cd -q $SCRIPT; cp *.html *.js $TMP)
(cd -q $TMP;
	for js in *.js; do
		uglifyjs --mangle --compress --quotes 3 $js -o $js
		cat $js
	done

	for html in *.html; do
		html-inline -i $html -o $html.2
		mv $html.2 $html

		html-minifier \
			--html5 \
			--collapse-whitespace \
			--collapse-inline-tag-whitespace \
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
	for html in *.html; do
		xxd -i $html > $html.xxd
		grep -v "^unsigned int" $html.xxd >> $html.c

		name=$(head -n1 $html.c)
		name=${(j..)${(Cs._.)${${name#unsigned char }%\[*}}}
		name="uint8_t g_Data${name}[] = {"
		{ print $name; tail -n +2 $html.c } >> data.c

		size=$(grep "^unsigned int" $html.xxd)
		sizeNum=${${size#*\= }%;}
		size=${(U)${${size#unsigned int }%_len \=*}}
		size="#define DATA_${size}_LENGTH ${sizeNum}"
		print $size >> data.h
	done

	{
		cat data.h
		cat data.c
	} | pbcopy
	pbpaste
)


rm -r $TMP

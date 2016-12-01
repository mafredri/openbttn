'use strict';

// Parser is a simple html a element that is used for parsing URL parts.
const parser = document.createElement('a');

// at takes care of encoding and decoding URLs to/from the AT format used by the
// bttn.
// For example:
// 	"http://example.com:8000/hello" -> "example.com,/hello,8000".
const at = {
	encodeURL(url) {
		if (!url.match(/^https?:\/\//)) {
			url = 'http://' + url;
		}
		parser.href = url;
		if (!parser.hostname) {
			return '';
		}

		let port = parser.port;
		if (parser.protocol === 'https:' && !port) {
			port = 443;
		}

		let newURL = parser.hostname + ',' + parser.pathname;
		if (port) {
			newURL += ',' + port;
		}

		// Maximum URL length, as defined in firmware.
		if (newURL.length > 200) {
			throw new Error('URL too long: ' + url);
		}

		return newURL;
	},
	decodeURL(url) {
		let [domain = '', search = '', port = ''] = url.split(',');
		if (port) {
			port = ':' + port;
		}
		return 'http://' + domain + port + search;
	}
};

window.at = at;

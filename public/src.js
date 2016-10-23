'use strict';

// Parser is a simple html a element that is used for parsing URL parts.
const parser = document.createElement('a');

// We use output.cgi, which is a part of the SPWF01SA http server to send CIND
// messages to the bttn.
const url = '/output.cgi?text=';

// cindMap maps form input names to CIND (custom indication) IDs so that they
// can be received by the bttn.
const cindMap = {
	auth: 0,
	commit: 1,
	url1: 2,
	url2: 3,
	user_desc: 20,
	ssid: 21,
	password: 22,
	priv_mode: 23,
	dhcp: 24,
	ip_addr: 25,
	ip_netmask: 26,
	ip_gateway: 27,
	ip_dns: 28,
	wifi_commit: 29,
};

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
		if (newURL.length > 120) {
			throw new Error(`URL too long: ${url}`);
		}

		return newURL;
	},
	decodeURL(url) {
		let [domain, search, port = ''] = url.split(',');
		if (port) {
			port = ':' + port;
		}
		return 'http://' + domain + port + search;
	}
};

function fetchConfig() {
	fetch('/config.json')
		.then((r) => r.json())
		.then((c) => {
			for (let key in c) {
				let element = document.getElementsByName(key)[0];
				if (element) {
					element.value = at.decodeURL(c[key]);
				}
			}
		});
}

document.addEventListener('DOMContentLoaded', () => {
	fetchConfig();

	// submitter keeps track of which input element was pressed to submit the
	// form.
	let submitter = null;
	const submits = document.querySelectorAll('input[type="submit"]');
	for (let submit of Array.from(submits)) {
		submit.addEventListener('click', (e) => {
			submitter = e.target;
		});
	}

	const forms = document.getElementsByTagName('form');
	for (let form of Array.from(forms)) {
		form.addEventListener('submit', (e) => {
			e.preventDefault(); // Do not submit the form.
			try {
				const urls = Array.from(e.target)
					.map(inputToCIND)
					.filter(i => i);
				try {
					urls.push(createCIND(submitter.name, submitter.value));
				} catch (e) {
					// Pass.
				}

				let p = Promise.resolve();
				for (let url of urls) {
					p = p.then(r => fetch(url))
				}
				p.catch(r => console.log(r));
			} catch (e) {
				// Notify the user of the error.
				alert(e);
			}
			submitter = null;
		});
	}
});

function inputToCIND(input) {
	if (input.type === 'submit') {
		return;
	}

	console.log(input);

	let value;
	if (input.name.match(/^url/)) {
		value = at.encodeURL(input.value);
	} else {
		value = input.value;
	}

	return createCIND(input.name, value);
}

function createCIND(name, text) {
	const id = cindMap[name];
	if (id === undefined) {
		throw new Error('Unknown indication (cind): ${name}');
	}

	const cind = `+CIND:${id}:${encodeURIComponent(text)}`;
	// Maximum length of text to output.cgi.
	if (cind.length > 120) {
		throw new Error(`Encoded length too long: ${name}`);
	}

	return url + cind;
}

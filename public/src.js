'use strict';

const url = '/output.cgi?text=';
const parser = document.createElement('a');

fetch('/config.json')
	.then(r => r.json())
	.then(s => {
		for (let key in s) {
			document.getElementById(key).value = decodeATURL(s[key]);
		}
	});

document.addEventListener('DOMContentLoaded', () => {
	document.getElementById('apply').addEventListener('click', click(false));
	document.getElementById('commit').addEventListener('click', click(true));

	function click(commitChanges) {
		return (e) => {
			e.preventDefault();

			const url1 = createCIND(1, atURL(document.getElementById('url1').value));
			const url2 = createCIND(2, atURL(document.getElementById('url2').value));
			const commit = createCIND(0, 'Commit configuration');

			fetch(url1)
				.then(() => fetch(url2))
				.then(() => commitChanges ? fetch(commit) : '')
				.then(() => console.log('Saved!'))
				.catch(r => console.log(r));
		};
	}
});

function atURL(url) {
	if (!url.match(/^https?:\/\//)) {
		url = 'http://' + url;
	}
	parser.href = url;

	let port = parser.port;
	if (parser.protocol === 'https:' && !port) {
		port = 443;
	}

	let newURL = `${parser.hostname},${parser.pathname}${parser.search},${port}`;
	// Maximum URL length, as defined in firmware.
	if (newURL.length > 100) {
		throw new Error('max length');
	}

	return newURL;
}

function decodeATURL(url) {
	let [domain, search, port] = url.split(',');
	if (port) {
		port = ':' + port;
	}
	return 'http://' + domain + port + search;
}

function createCIND(id, text) {
	const cind = `+CIND:${id}:${encodeURIComponent(text)}`;
	// Maximum length of text to output.cgi.
	if (cind.length > 120) {
		throw new Error('max encoded length');
	}

	return url + cind;
}

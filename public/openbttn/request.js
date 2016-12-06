'use strict';

let requestAuthKey = null;

function request(body, timeout = 5000) {
	let xhr = new XMLHttpRequest();
	xhr.open('POST', `http://${location.hostname}:8774/socket`);

	return new Promise(function (resolve, reject) {
		xhr.onload = function (e) {
			if (xhr.status === 200) {
				resolve(xhr.responseText);
			} else if ([400, 403, 500].includes(xhr.status)) {
				reject(`${xhr.statusText}: ${xhr.responseText}`);
			} else {
				reject(`Unknown response: ${xhr.status} ${xhr.statusText} (${xhr.responseText})`)
			}
		};
		xhr.ontimeout = (e) => reject('Request timed out!');
		xhr.onerror = (e) => reject('Error communicating with bttn!');

		xhr.timeout = timeout;

		if (requestAuthKey) {
			body = 'auth = ' + requestAuthKey + '\n' + body;
		}
		xhr.send(body);
	});
}

function setRequestAuthKey(key) {
	requestAuthKey = key;
}

window.request = request;
window.setRequestAuthKey = setRequestAuthKey;

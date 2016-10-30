'use strict';

// cindMap maps form input names to CIND (custom indication) IDs so that they
// can be received by the bttn.
const cinds = {
	auth: 0,
	commit: 1,
	url1: 2,
	url2: 3,
};
cind.register(cinds);

function fetchConfig() {
	fetch('/config.json')
		.then((r) => r.json())
		.then((c) => {
			for (let key in c) {
				let element = document.getElementById(key);
				if (element) {
					element.value = at.decodeURL(c[key]);
				}
			}
		});
}

fetchConfig();

document.getElementsByTagName('form')[0].addEventListener('submit', e => {
	setTimeout(fetchConfig, 2000);
});

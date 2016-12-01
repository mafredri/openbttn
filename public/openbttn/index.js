'use strict';

const authForm = document.getElementById('auth-form');
const confSection = document.getElementById('conf');
const configForm = document.getElementById('conf-form');

function authenticate(authKey) {
	setRequestAuthKey(authKey);
	authForm.classList.add('hidden');
	confSection.classList.remove('hidden');
	fetchConfig();
}

function fetchConfig() {
	request('dump_config')
		.then((resp) => JSON.parse(resp))
		.then(setConfig)
		.catch(e => console.log(e));

	function setConfig(c) {
		for (let key in c) {
			let element = document.getElementById(key);
			if (element) {
				element.value = at.decodeURL(c[key]);
			}
		}
	}
}

authForm.addEventListener('submit', (e) => {
	e.preventDefault();
	const auth = authForm.auth.value;
	request(`auth = ${auth}`).then(() => authenticate(auth), alert);
});

configForm.addEventListener('submit', (e) => {
	e.preventDefault();
	request(formToMessage(configForm))
		.then(alert, alert);
});

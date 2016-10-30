'use strict';

// We use output.cgi, which is a part of the SPWF01SA http server to send CIND
// messages to the bttn.
const url = '/output.cgi?text=';

const cind = {
	register(cinds) {
		this.cinds = cinds;
	},
	create(name, text) {
		const id = this.cinds[name];

		if (id === undefined) {
			throw new Error('Unknown indication (cind): ' + name);
		}

		const cind = `+CIND:${id}:${encodeURIComponent(text)}`;
		// Maximum length of text to output.cgi.
		if (cind.length > 120) {
			throw new Error('Encoded length too long: ' + name);
		}

		return url + cind;
	}
}

window.cind = cind;

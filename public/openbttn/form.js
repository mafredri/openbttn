'use strict';

function formToMessage(f) {
	try {
		const messages = Array.from(f)
			.filter(i => i.value && i.type !== 'submit')
			.map(inputToKeyValueString)
			.filter(i => i);

		return messages.join('\n');
	} catch (e) {
		// Notify the user of the error.
		alert(e);
		throw new Error(e);
	}
}

function inputToKeyValueString(input) {
	let value;
	if (input.type === 'url') {
		value = at.encodeURL(input.value);
	} else {
		value = input.value;
	}

	if (input.type === 'textarea') {
		return value.trim()
			.split('\n')
			.map(v => `${input.id} = ${v}`)
			.join('\n');
	}

	return `${input.id} = ${value}`;
}

window.formToMessage = formToMessage;

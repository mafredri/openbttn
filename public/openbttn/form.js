'use strict';

// submitter keeps track of which input element was pressed to submit the
// form.
let submitter = null;
for (let submit of document.querySelectorAll('input[type="submit"]')) {
	submit.addEventListener('click', (e) => {
		submitter = e.target;
	});
}

for (let form of document.querySelectorAll('form')) {
	form.addEventListener('submit', e => {
		e.preventDefault(); // Do not submit the form.
		try {
			const urls = Array.from(e.target)
				.filter(i => i.value && i.type !== 'submit')
				.map(inputToCIND)
				.filter(i => i);
			try {
				urls.push(cind.create(submitter.id, submitter.value));
			} catch (e) {
				// Pass.
			}

			urls.reduce((prev, url) => prev.then(() => fetch(url)), Promise.resolve())
				.catch(r => console.log(r));
		} catch (e) {
			// Notify the user of the error.
			alert(e);
		}
		submitter = null;
	});
}

function inputToCIND(input) {
	let value;
	if (input.type === 'url') {
		value = at.encodeURL(input.value);
	} else {
		value = input.value;
	}

	if (input.type === 'textarea') {
		value = value.trim().replace('\n', '\r') + '\r';
	}

	return cind.create(input.id, value);
}

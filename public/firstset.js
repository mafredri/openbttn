'use strict';

document.getElementById('dhcp').addEventListener('change', (e) => {
	Array.from(document.querySelectorAll('.static')).forEach((el) => {
		if (e.target.value == 1) {
			el.classList.add('hidden');
		} else {
			el.classList.remove('hidden');
		}
	});
});

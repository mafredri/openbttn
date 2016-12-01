'use strict';

const FIRMWARE_TIMEOUT = 120000;

const configForm = document.getElementById('conf-form');
const firmwareForm = document.getElementById('fw-form');

// Allow toggling of DHCP mode.
document.getElementById('dhcp').addEventListener('change', e => {
	for (let el of document.querySelectorAll('.static')) {
		el.classList.toggle('hidden');
	}
});

configForm.addEventListener('submit', function (e) {
	e.preventDefault();
	request(formToMessage(configForm))
		.then(() => alert('Success! Your bttn will now reboot and connect to the configured Wi-Fi network'))
		.catch(alert);
});

firmwareForm.addEventListener('submit', function (e) {
	e.preventDefault();
	document.querySelector('#conf').classList.add('hidden');

	let i = FIRMWARE_TIMEOUT / 1000;
	loading();
	let iv = setInterval(loading, 1000);

	request(formToMessage(firmwareForm), FIRMWARE_TIMEOUT)
		.then((resp) => {
			clearInterval(iv);
			firmwareForm.innerHTML = `${resp}...<br><br>Please reconnect to the 'OpenBttn' Wi-Fi network and reload this page.`;
		})
		.catch((err) => {
			clearInterval(iv);
			firmwareForm.innerHTML = `${err}, please reload this page and try again...`;
		});

	function loading() {
		firmwareForm.innerHTML = 'Downloading firmware...<br><br>Please wait: ' + i--;
		if (i < 0) {
			clearInterval(iv);
		}
	}
});

// Fetch the firmware version from status.html.
let xhr = new XMLHttpRequest();
xhr.open('GET', '/status.shtml');
xhr.onload = () => checkFirmwareVersion(getFirmwareVersion(xhr.responseText));
xhr.send(null);

function getFirmwareVersion(data) {
	let [_, date, commit] = data.match(/version = ([0-9]+)-([0-9a-f]+)/i);
	date = parseInt(date, 10);
	return { date, commit };
}

function checkFirmwareVersion({ date, commit }) {
	if (date < 141106) {
		document.getElementById('fw-form').innerHTML = '<p class="warn">Firmware is too old for OTA update.</p>';
	} else if (date < 160129) {
		document.getElementById('fw').classList.remove('hidden');
	}
	document.getElementById('fw-ver').textContent = date + '-' + commit;
}

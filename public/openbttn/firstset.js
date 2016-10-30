'use strict';


// cindMap maps form input names to CIND (custom indication) IDs so that they
// can be received by the bttn.
const recoveryCINDS = {
	user_desc: 0,
	ssid: 1,
	password: 2,
	priv_mode: 3,
	dhcp: 4,
	ip_addr: 5,
	ip_netmask: 6,
	ip_gateway: 7,
	ip_dns: 8,
	wifi_mode: 9,
	commit: 20,
	ota: 21,
};

// Register the recovery CINDs.
cind.register(recoveryCINDS);

document.getElementById('dhcp').addEventListener('change', e => {
	for (let el of document.querySelectorAll('.static')) {
		el.classList.toggle('hidden');
	}
});

const fwForm = document.getElementById('fw-form');
fwForm.addEventListener('submit', function(e) {
	document.querySelector('#conf').classList.add('hidden');

	let i = 40; // Firmware update should take ~35s so we add a little extra.
	loading();
	let iv = setInterval(loading, 1000);

	function loading() {
		fwForm.innerHTML = 'Upgrading... check LEDs for status...<br>Time left: ' + i--;
		if (i < 0) {
			fwForm.innerHTML = 'Done! Please reconnect and refresh the page.';
			clearInterval(iv);
		}
	}
});

fetch('/status.shtml')
	.then(r => r.text())
	.then(getVersion)
	.then(checkVersion);

function getVersion(data) {
	let [_, date, commit] = data.match(/version = ([0-9]+)-([0-9a-f]+)/i);
	date = parseInt(date, 10);
	return { date, commit };
}

function checkVersion({ date, commit }) {
	if (date < 141106) {
		document.getElementById('fw-form').innerHTML = '<p class="warn">Firmware is too old for OTA update.</p>';
	} else if (date === 160129) {
		document.getElementById('fw').classList.add('hidden');
	}
	document.getElementById('fw-ver').textContent = date + '-' + commit;
}

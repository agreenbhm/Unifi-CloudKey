<?php
require_once('settings.inc');
require_once('common.inc');

function get_app_port($cfgfile, $key, $def_port) {
    $port = $def_port;
    $cfg = @parse_ini_file($cfgfile, FALSE);
    if ($cfg) {
        $nport = intVal(arrayValueOrNull($key, $cfg));
        if ($nport > 0 && $nport < 65535) {
            $port = $nport;
        }
    }
    return $port;
}

function get_config() {
	$response = array();
	$response[] = get_basic_config();
	$response[] = get_network_config();
	return $response;
}

function get_basic_config() {
	$config = array();
	$config['id'] = 'basic';
	$config['hostname'] = get_hostname();
	$config['timezone'] = get_system_timezone();
	$config['reset-button'] = get_resetbutton_status();

	return $config;
}

function save_basic_config($config) {
	$rc = 0;
	foreach ($config as $key => $value) {
		switch ($key) {
		case 'hostname':
			if (!empty($value)) {
				$rc = change_hostname($value);
			}
			break;
		case 'timezone':
			if (!empty($value)) {
				$rc = change_timezone($value);
			}
			break;
		case 'reset-button':
			$rc = change_resetbutton_status($value);
			break;
		}
		if ($rc != 0) {
			return $rc;
		}
	}
	return 0;
}

function get_resetbutton_status() {
	$is_resetbutton_enabled = true;
	$cfg = @parse_ini_file(INFCTLD_CONFIG, FALSE, INI_SCANNER_RAW);
	if ($cfg) {
		if (isset($cfg['INFCTLD_ARGS'])) {
			$args = $cfg['INFCTLD_ARGS'];
			if (strpos($args, '-n') !== false) {
				$is_resetbutton_enabled = false;
			}
		}
	}
	return $is_resetbutton_enabled;
}

function change_resetbutton_status($newstate) {
	$strstate = $newstate ? 'true' : 'false';
	$cmd = CMD_CHRESETBUTTON . ' ' . $strstate;
	$out = shell_exec($cmd);
	return $out;
}

function _get_network_config($ifname, &$iface_cfg) {
	$netconfig_defaults = array('mode' => 'dhcp',
			'address' => DEFAULT_IP,
			'netmask' => DEFAULT_NETMASK,
			'gateway' => DEFAULT_GATEWAY,
			'dhcp-fallback-ip' => DEFAULT_IP,
			'dhcp-fallback-netmask' => DEFAULT_NETMASK,
			'dns' => array(DEFAULT_DNS1, DEFAULT_DNS2));
	$config = array();
	$config['id'] = 'network';
	$config['mode'] = $iface_cfg['method'];
	$config['address'] = arrayValueOrNull('address', $iface_cfg['options']);
	$config['netmask'] = arrayValueOrNull('netmask', $iface_cfg['options']);
	$config['gateway'] = arrayValueOrNull('gateway', $iface_cfg['options']);
	$config['dhcp-fallback-ip'] = $iface_cfg['options']['dhcp-fallback-ip'];
	$config['dhcp-fallback-netmask'] = $iface_cfg['options']['dhcp-fallback-netmask'];

	$dns_servers = array();
	if (is_file('/sbin/resolvconf') && !empty($iface_cfg['options']['dns-nameservers'])) {
		$dns_servers = explode(' ', $iface_cfg['options']['dns-nameservers']);
	}
	if (count($dns_servers) == 0) {
		$dns_servers = resolv_conf_get_servers('/etc/resolv.conf');
	}
	$config['dns'] = $dns_servers;
	$ifaddr = ifconfig_get_addr($ifname);
	if (count($ifaddr) == 3) {
		if (empty($config['address'])) {
			$config['address'] = $ifaddr['addr'];
		}
		if (empty($config['netmask'])) {
			$config['netmask'] = $ifaddr['mask'];
		}
	}
	if (empty($config['gateway'])) {
		$config['gateway'] = routes_get_default_gw();
	}

	foreach ($config as $key => $value) {
		if (empty($config[$key])) {
			$config[$key] = $netconfig_defaults[$key];
		}
	}

	return $config;
}

function ifupdown_get_network_config() {
	$ifname = 'eth0';
	$iface_cfg = network_interface_config_get('/etc/network/interfaces', 'inet', $ifname);
	return _get_network_config($ifname, $iface_cfg);
}

function ifupdown_save_network_config($config) {
	$netcfg = network_interface_config_parse('/etc/network/interfaces');
	$ifname = 'eth0';
	$iffamily = 'inet';
	if (empty($netcfg['ifaces'][$iffamily][$ifname])) {
		$config['ifaces'][$iffamily][$ifname] = array();
	}
	$iface_cfg = &$netcfg['ifaces'][$iffamily][$ifname];
	$netmode = $config['mode'];
	$update = false;
	if ($netmode == 'static') {
		$update = true;
		$iface_cfg['name'] = $ifname;
		$iface_cfg['family'] = $iffamily;
		$iface_cfg['method'] = 'static';
		$dns = $config['dns'];
		$iface_cfg['options']['address'] = $config['address'];
		$iface_cfg['options']['netmask'] = $config['netmask'];
		$iface_cfg['options']['gateway'] = $config['gateway'];
		if (!empty($dns)) {
			$iface_cfg['options']['dns-nameservers'] = implode(' ', $dns);
		}
	} else if ($netmode == 'dhcp') {
		$update = true;
		$iface_cfg['name'] = $ifname;
		$iface_cfg['family'] = $iffamily;
		unset($iface_cfg['options']['address']);
		unset($iface_cfg['options']['netmask']);
		unset($iface_cfg['options']['gateway']);
		unset($iface_cfg['options']['dns-nameservers']);
		$iface_cfg['method'] = 'dhcp';
		$iface_cfg['options']['dhcp-fallback-ip'] = $config['dhcp-fallback-ip'];
		$iface_cfg['options']['dhcp-fallback-netmask'] = $config['dhcp-fallback-netmask'];
	}
	if ($update) {
		$tmpfile = tempnam('/tmp', 'netif-');
		network_interface_config_write($tmpfile, $netcfg);
		change_network($tmpfile);
		unlink($tmpfile);
	}
	return 0;
}

function systemd_get_network_config() {
	$ifname = 'eth0';
	$cfg_file = '/etc/systemd/network/' . $ifname . '.network';
	if (!(is_file($cfg_file) || is_link($cfg_file))) {
		$cfg_file = '/lib/systemd/network/' . $ifname . '.network';
	}
	$iface_cfg = systemd_networkd_config_parse($cfg_file);
	return _get_network_config($ifname, $iface_cfg);
}

function systemd_save_network_config($config) {
	$ifname = 'eth0';
	$cfg_file = '/etc/systemd/network/' . $ifname . '.network';
	if (!(is_file($cfg_file) || is_link($cfg_file))) {
		$cfg_file = '/lib/systemd/network/' . $ifname . '.network';
	}
	$netcfg = systemd_networkd_config_parse($cfg_file);
	$netmode = $config['mode'];
	$update = false;

	$config['name'] = $ifname;
	$options = array();
	if ($netmode == 'static') {
		$update = true;
		$config['method'] = $netmode;
        if (array_key_exists('address', $config)) $options['address'] = $config['address'];
        if (array_key_exists('netmask', $config)) $options['netmask'] = $config['netmask'];
        if (array_key_exists('gateway', $config)) $options['gateway'] = $config['gateway'];
        if (array_key_exists('dns', $config)) $options['dns-nameservers'] = array_unique(array_filter($config['dns']));
        $config['options'] = $options;
	} else if ($netmode == 'dhcp') {
		$update = true;
		$config['method'] = $netmode;
        if (array_key_exists('dhcp-fallback-ip', $config)) $options['dhcp-fallback-ip'] = $config['dhcp-fallback-ip'];
        if (array_key_exists('dhcp-fallback-netmask', $config)) $options['dhcp-fallback-netmask'] = $config['dhcp-fallback-netmask'];
        if (array_key_exists('dhcp-fallback-gateway', $config)) $options['dhcp-fallback-gateway'] = $config['dhcp-fallback-gateway'];
        $config['options'] = $options;
    }

	if ($update) {
		$tmpfile = tempnam('/tmp', 'netif-');
		systemd_networkd_config_write($tmpfile, $config);
		change_network($tmpfile);
		unlink($tmpfile);
	}
	return 0;
}

function get_network_config() {
	if (file_exists('/run/systemd/netif')) {
		return systemd_get_network_config();
	} else {
		return ifupdown_get_network_config();
	}
}

function save_network_config($config) {
	if (file_exists('/run/systemd/netif')) {
		return systemd_save_network_config($config);
	} else {
		return ifupdown_save_network_config($config);
	}
}

function get_status() {
	$status = array();
	$disk_srv = get_diskstats('/srv');

	$disk_sd = get_diskstats('/data');
	$pkgs = array();
	$pkgs['unifi-video'] = get_uvc_status();
	$pkgs['unifi'] = get_unifi_status();

	$status['id'] = 'status';
	$status['version'] = get_version();
	$status['hostname'] = get_hostname();
	$status['uptime'] = get_uptime();
	$status['date'] = time();
	$status['hwaddr'] = ifconfig_get_hwaddr('eth0');
	$status['timezone'] = get_system_timezone();
	$status['diskFree'] = $disk_srv['Available'];
	$status['diskTotal'] = $disk_srv['1B-blocks'];
	$status['sdDiskFree'] = $disk_sd['Available'];
	$status['sdDiskTotal'] = $disk_sd['1B-blocks'];
	$status['sDCardMounted'] = is_sd_card_mounted();
	$status['packages'] = $pkgs;
	$status['username'] = $_SESSION['username'];

	return $status;
}

function get_uvc_status() {
	$pkg = array();
	$version = get_package_version('unifi-video*');
	$pkg['version'] = $version;
	if (!empty($version)) {
		$pkg['port'] = 7443;
	}
	return $pkg;
}

function get_unifi_status() {
	$pkg = array();
	$version = get_package_version('unifi');
	$pkg['version'] = $version;
	$pkg['is_running'] = is_unifi_running();
	return $pkg;
}

function sanitizeUrlForShell($url) {
  $url = escapeshellcmd($url);
  $url = str_replace(['\?', '\&', '\~'], ['?', '&', '~'], $url);
  $url = '"' . $url . '"';
  return $url;
}

function is_unifi_running() {
	$status_port = get_app_port(UNIFI_CONFIGFILE, 'unifi.http.port', 8080);
	$unifi_href = 'http://127.0.0.1:' . $status_port . '/status';
	exec(CMD_CURL . sanitizeUrlForShell($unifi_href), $out, $rc);
	if ($rc == 0) {
		return true;
	}
	return false;
}

function check_update_firmware(&$output) {
	exec(CMD_CURL . 'http://localhost:9090/system', $curl_out, $rc);
	if ($rc != 0) {
		return $rc;
	}

	foreach($curl_out as $line) {
		$output .= $line;
	}

	$output = @json_decode($output, true)['firmware'];
	return $rc;
}

function timezone_lookup() {
	$cache_file = '/tmp/tz.lookup';
	$xml = false; $cached = true;
	$tzname = null;
	if (file_exists($cache_file)) {
		$xml = simplexml_load_file($cache_file);
	}
	if (!$xml) {
		$cached = false;
		$xml = simplexml_load_file('http://geoip.ubuntu.com/lookup');
	}
	if ($xml) {
		$tzname = $xml->TimeZone;
		if (!empty($tzname) && !$cached) {
			$xml->asXML($cache_file);
		}
	}
	return $tzname;
}

function get_timezones($group=true) {
	$zoneids = timezone_identifiers_list();
	if (!$group) {
		$zoneids['id'] = 'timezones';
		return $zoneids;
	}
	$zones = array();
	$zones['id'] = 'timezones';
	foreach ($zoneids as $z) {
		$w = explode('/', $z);
		if (count($w) > 1) {
			$group = $w[0];
			$area = $w[1];
		} else {
			$group = 'Misc';
			$area = $z;
		}
		$area = str_replace('_', ' ', $area);
		$zones[$group][$area] = $z;
	}
	return $zones;
}

function send_error($msg, $error_code = 412) {
	$meta = array();
	$meta['rc'] = 'error';
	$meta['msg'] = $msg;
	http_response_code($error_code);
	send_json(array(), $meta);
}

function send_success($data) {
	$meta = array();
	$meta['rc'] = 'ok';
	send_json($data, $meta);
}

function send_json($data, $meta) {
	$response['data'] = $data;
	$response['meta'] = $meta;
	Header('Content-Type: application/json');
	echo json_encode($response);
	exit;
}

function read_update_status() {
	$status = array();
	exec(CMD_SYSTOOL . ' fwupdatestatus', $out, $rc);
	$status['status'] = $out[0];
	$is_wget_running = is_wget_running();
	$status['is_wget_running'] = $is_wget_running;
	$status['fw_file_exists'] = @file_exists(FW_FILE);
	if (@file_exists(DOWNLOAD_PROGRESS_FILE) || $is_wget_running) {
		$status['status'] = 'downloading';
		$status['progress'] = read_progress();
	}
	return $status;
}

function read_progress() {
	$progress = @file_get_contents(DOWNLOAD_PROGRESS_FILE);
	if (preg_match('/[^0-9]*([0-9]+)%[^%]*$/', $progress, $matches)) {
		return intval($matches[1]);
	}
	return null;
}

function get_unifi_update_availability() {
	exec(CMD_APTCACHE . ' policy unifi', $out, $rc);
	$status = array();
	if (is_apt_get_running()) {
		$status['state'] = 'update-packages';
	} else {
		$installed = get_unifi_version($out[1], 'Installed');
		$candidate = get_unifi_version($out[2], 'Candidate');
		if ($rc == 0) {
			if (version_compare($installed, $candidate, '<')) {
				$status['state'] = 'available';
			} else if (version_compare($installed, $candidate, '=')) {
				$status['state'] = 'up-to-date';
			} else {
				$status['state'] = 'not-available';
			}
		}
		$status['candidate'] = $candidate;
	}
	return $status;
}

function is_unifi_update_in_progress() {
	$status = array();
	$status['in_progress'] = is_apt_get_running();
	return $status;
}

function is_restore_in_progress() {
	$status = array();
	$status['in_progress'] = is_restore_running();
	$status['status'] = @file_get_contents(RESTORE_STATUS_FILE);
	return $status;
}

function get_unifi_version($text, $field) {
	$pattern = '/' . $field . ':\s*([0-9\.\-]*)/';
	preg_match($pattern, $text, $matches);
	return $matches[1];
}

function update_unifi() {
	exec(CMD_APTGET . ' install --only-upgrade unifi' . CMD_BACKGROUND, $out, $rc);
	return $rc;
}

function install_unifi() {
	exec(CMD_APTGET . ' install unifi' . CMD_BACKGROUND, $out, $rc);
	return $rc;
}

function update_packages() {
	if (!is_apt_get_running()) {
		exec(CMD_APTGET . ' update -o Dir::Etc::sourcelist=sources.list.d/ubnt-unifi.list -o Dir::Etc::sourceparts="-" -o APT::Get:List-Cleanup="0"' . CMD_BACKGROUND);
	}
}

function clean_packages() {
	if (!is_apt_get_running()) {
		exec(CMD_APTGET . ' clean -y');
	}
}

function is_apt_get_running() {
	exec(CMD_APTGET_RUNNING, $out, $rc);
	if ($rc == 0) {
		return true;
	}
	return false;
}

function is_restore_running() {
	exec(CMD_RESTORE_RUNNING, $out, $rc);
	if ($rc == 0) {
		return true;
	}
	return false;
}

function is_wget_running() {
	exec(CMD_WGET_RUNNING, $out, $rc);
	if ($rc == 0) {
		return true;
	}
	return false;
}

function is_valid_username($username) {
	return (preg_match("/^[_a-z][\-._a-z0-9]*$/i", $username) && preg_match("/^.{1,32}$/", $username));
}

function chusername($username) {
	if (is_valid_username($username)) {
		exec(CMD_CHUSERNAME . ' ' . escapeshellcmd($username), $out, $rc);
	} else {
		$rc = 1;
	}
	return $rc;
}

function removeProgressFile() {
	if (@file_exists(DOWNLOAD_PROGRESS_FILE)) {
		exec('rm ' . DOWNLOAD_PROGRESS_FILE);
	}
}

function removeFirmwareFile() {
	if (@file_exists(FW_FILE)) {
		exec('rm ' . FW_FILE);
	}
}

function removeRestoreStatusFile() {
	if (@file_exists(RESTORE_STATUS_FILE)) {
		exec('rm ' . RESTORE_STATUS_FILE);
	}
}

function fwdownload($url) {
  $url = sanitizeUrlForShell($url);
  $command = CMD_WGET . $url . CMD_WGET_OPTIONS;

  removeProgressFile();
  removeFirmwareFile();
  exec($command, $out, $rc);
}

function fwupdateViaFile() {
	removeProgressFile();
	if (@file_exists(FW_FILE)) {
		exec(CMD_FWUPDATE . ' ' . escapeshellarg(FW_FILE), $out, $rc);
		return $rc;
	}
}

function fwupload($file) {
	$rc = validateUploadedFile($file);

	if ($rc == 0) {
		if ($file['size'] < 128) {
			return "file is too small";
		}

		move_uploaded_file($file['tmp_name'], FW_FILE);
	}
	return $rc;
}

function restore($file) {
	removeRestoreStatusFile();
	$rc = validateUploadedFile($file);
	if ($rc == 0) {
		move_uploaded_file($file['tmp_name'], BACKUP_FILE);
		exec(CMD_RESTORE, $out, $rc);
		if ($rc == 0) {
			return $rc;
		}
		return $out;
	}
	return $rc;
}

function restore_autobackup($filename) {
	removeRestoreStatusFile();
	if (in_array($filename, get_external_autobackup_files())) {
	  $command = sprintf(CMD_RESTORE_WITH_FILENAME, AUTOBACKUP_DIR . '/' . escapeshellarg($filename));
	} else {
	  $command = sprintf(CMD_RESTORE_WITH_FILENAME, INTERNAL_AUTOBACKUP_DIR . '/' . escapeshellarg($filename));
	}
	exec($command, $out, $rc);
	if ($rc == 0) {
		return $rc;
	}
	return $out;
}

function get_autobackup_list() {
  $sdList = read_autobackup_list(AUTOBACKUP_GET_LIST);
  $internalList = read_autobackup_list(INTERNAL_AUTOBACKUP_GET_LIST);
  return array_merge($sdList, $internalList);
}

function read_autobackup_list($cmd) {
	exec($cmd, $list, $rc);

	if (!empty($list[0])) {
		$list = @json_decode($list[0], true);
	}

	if ($list) {
		foreach($list as $filename => &$data) {
			$data['filename'] = $filename;
		}
		$list = array_values($list);
	}

	return $list ? $list : array();
}

function get_external_autobackup_files() {
	exec(AUTOBACKUP_GET_LIST, $list, $rc);

	if (!empty($list[0])) {
		$list = @json_decode($list[0], true);
	}

	return $list ? array_keys($list) : array();
}

function validateUploadedFile($file) {
	// file upload sanity checks
	if (!isset($file['error'])) {
		return "invalid input";
	}
	if ($file['error'] != UPLOAD_ERR_OK) {
		switch ($file['error']) {
			case UPLOAD_ERR_NO_FILE:
				return "no file";
			case UPLOAD_ERR_INI_SIZE:
			case UPLOAD_ERR_FORM_SIZE:
				return "file is too big";
			case UPLOAD_ERR_CANT_WRITE:
				return "failed to write file to disk";
			default:
				return "unknown error";
		}
	}

	if (!is_uploaded_file($file['tmp_name'])) {
		return "invalid file";
	}

	return 0;
}

function start_unifi($username) {
	exec(CMD_SERVICE_UNIFI . ' start', $out, $rc);
	return $rc;
}

function stop_unifi($username) {
	exec(CMD_SERVICE_UNIFI . ' stop', $out, $rc);
	return $rc;
}

function is_sd_card_mounted() {
	exec(CMD_SYSTEMCTL . ' --quiet is-active data.mount', $out, $rc);
	return $rc == 0;
}
?>

<?php
require_once('settings.inc');
require_once('common.inc');
require_once('api.inc');
if (!is_session_valid(true, true)) {
	send_error('api.err.LoginRequired');
	exit;
}
$method = strtolower($_SERVER['REQUEST_METHOD']);
$path_info = $_SERVER['REQUEST_URI'];
$qpos = strpos($path_info, '?');
if ($qpos !== false) {
	$path_info = substr($path_info, 0, $qpos);
}
if (strpos($path_info, '/') === 0) {
	$path_info = substr($path_info, 1);
}
$request = explode('/', $path_info);
array_shift($request);
$request_vars = array();

switch ($method) {
case 'get':
	do_get($request, $_GET);
	break;
case 'post':
	$vars = array();
	$vars['data'] = file_get_contents('php://input');
	do_post($request, $vars);
	break;
case 'put':
	$vars = array();
	$vars['data'] = file_get_contents('php://input');
	do_put($request, $vars);
	break;
case 'delete':
	do_delete($request);
	break;
default:
	do_unsupported($method, $request);
	break;
}

function do_get($req, $vars) {
	$data = '';
	if (isset($vars['data'])) {
		$data = json_decode($vars['data'], true);
	}
	$cmd = count($req) > 0 ? $req[0] : 'status';
	switch ($cmd) {
	case 'status':
		$response = array();
		$response[] = get_status();
		send_success($response);
		break;
	case 'config':
		send_success(get_config());
		break;
	case 'tzlookup':
		$tz = (string)timezone_lookup();
		if (empty($tz)) {
			send_error('api.err.TimezoneLookupFailure');
		} else {
			send_success(array('timezone' => $tz));
		}
		break;
	case 'timezones':
		$group = true;
		if ((count($req) > 1) && $req[1] == 'list')
			$group = false;
		$response = array();
		$response[] = get_timezones($group);
		send_success($response);
		break;
	case 'reboot':
		reboot();
		//fake_cmd('reboot');
		send_success(array());
		break;
	case 'poweroff':
		poweroff();
		//fake_cmd('poweroff');
		send_success(array());
		break;
	case 'reset2defaults':
		reset2defaults();
		//fake_cmd('reset2defaults');
		send_success(array());
		break;
	case 'update-unifi':
		if (is_apt_get_running()) {
			send_error('api.err.UpdatePackagesInProgress', 503);
		}
		$rc = update_unifi();
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to update unifi package. Error: ' . $rc);
		}
		break;
	case 'install-unifi':
		if (is_apt_get_running()) {
			send_error('api.err.UpdatePackagesInProgress', 503);
		}
		$rc = install_unifi();
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to install unifi package. Error: ' . $rc);
		}
		break;
	case 'update-status':
		$response = array();
		$response[] = read_update_status();
		send_success($response);
		break;
	case 'check-update-unifi':
		$response = array();
		$response[] = get_unifi_update_availability();
		send_success($response);
		break;
	case 'check-update-unifi-status':
		$response = array();
		$response[] = is_unifi_update_in_progress();
		send_success($response);
		break;
	case 'update-packages':
		update_packages();
		send_success(array());
		break;
	case 'clean-packages':
		clean_packages();
		send_success(array());
		break;
	case 'restore-status':
		$response = array();
		$response[] = is_restore_in_progress();
		send_success($response);
		break;
	case 'check-update-firmware':
		$rc = check_update_firmware($output);
		if ($rc == 0) {
			send_success($output);
		} else {
			send_error('Failed to get firmware update version Error: ' . $rc);
		}
		break;
  case 'autobackup-list':
    $response = get_autobackup_list();
    send_success($response);
    break;
	default:
		do_unsupported('get', $req, $vars);
	}
}

function do_unsupported($method, $req, $vars) {
	send_error('api.err.UnsupportedMethod');
}

function do_post($req, $vars) {
	$data = '';
	if (isset($vars['data'])) {
		$data = json_decode($vars['data'], true);
	}
	if (count($req) < 1)
		send_error('api.err.MissingCommand');
	$cmd = $req[0];
	switch ($cmd) {
	case 'account':
		$username = $data['username'];
		if (isset($username)) {
			$rc = chusername($username);
			if ($rc == 0) {
				$_SESSION['username'] = $username;
				send_success(array());
			} else {
				send_error('Failed changing username', 500);
			}
		} else {
			$old_passwd = $data['old_password'];
			$new_passwd = $data['new_password'];
			if ($new_passwd == 'ubnt') {
				send_error('Failed changing password: cannot use default', 403);
			}
			$rc = chpasswd($_SESSION['username'], $old_passwd, $new_passwd);
			if ($rc == 0) {
				$_SESSION['is.default.password'] = false;
				send_success(array());
			} else {
				send_error('Failed changing password for user ' . $_SESSION['username'] . '. Error: ' . $rc, 401);
			}
		}

		break;
	case 'config':
		foreach ($data as $entry) {
			$rc = 0;
			switch ($entry['id']) {
			case 'basic':
				$rc = save_basic_config($entry);
				break;
			case 'network':
				$rc = save_network_config($entry);
				break;
			}
		}
		sleep(1);
		send_success($data);
		break;
    case 'fw-download':
        if (filter_var($data['fileUrl'], FILTER_VALIDATE_URL) !== false) {
            fwdownload($data['fileUrl']);
            send_success(array());
        } else {
            send_error('Failed to download firmware. Given URL is not a valid.', 406);
        }
        break;
	case 'update':
		removeProgressFile();
		if (@file_exists(FW_FILE)) {
			exec(CMD_FWUPDATE . ' ' . escapeshellarg(FW_FILE), $out, $rc);
			if ($rc == 0) {
				send_success(array());
			} else {
				send_error('Failed to update firmware. Error: ' . $rc . implode(",", $out), 406);
			}
		}
		break;
	case 'fw-upload':
		$rc = fwupload($_FILES['file']);
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to upload firmware. Error: ' . $rc, 406);
		}
		break;
	case 'restore':
		$rc = restore($_FILES['file']);
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to restore from backup. Error: ' . implode(",", $rc), 406);
		}
		break;
	case 'autobackup-restore':
		$rc = restore_autobackup($data['filename']);
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to restore from backup. Error: ' . implode(",", $rc), 406);
		}
		break;
	case 'stop-unifi':
		$rc = stop_unifi();
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to stop Unifi. Error: ' . implode(",", $rc), 500);
		}
		break;
	case 'start-unifi':
		$rc = start_unifi();
		if ($rc == 0) {
			send_success(array());
		} else {
			send_error('Failed to start Unifi. Error: ' . implode(",", $rc), 500);
		}
		break;
	default:
		do_unsupported('post', $req, $vars);
	}
}

function do_put($req, $vars) {
	do_unsupported('put', $req, $vars);
}

function do_delete($req, $vars) {
	do_unsupported('delete', $req, $vars);
}

function fake_cmd($cmd) {
	$fp = fopen("/tmp/fake-cmd.log", "a");
	if (is_resource($fp)) {
		$now = time();
		fwrite($fp, "Command '$cmd' to be executed at $now\n");
		fflush($fp);
		fclose($fp);
		session_destroy();
	}
}

?>

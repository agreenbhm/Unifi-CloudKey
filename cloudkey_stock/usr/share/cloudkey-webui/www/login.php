<?php
require_once('settings.inc');
require_once('api.inc');
function login($user, $password) {
	$ph = @popen(CMD_PWCHECK, 'w');
	if (!is_resource($ph)) {
		return 1;
	}
	@fwrite($ph, $user . "\n" . $password . "\n");
	@fflush($ph);
	$rc = @pclose($ph);
	return $rc;
}
session_start();
session_regenerate_id(true);
if ($_SERVER['REQUEST_METHOD'] == "POST") {
	$value = json_decode(file_get_contents('php://input'), true);
	$username = $value['username'];
	$password = $value['password'];
	$rc = login($username, $password);
	if ($rc == 0) {
		$_SESSION['authed'] = true;
		$_SESSION['username'] = $username;
		$_SESSION['lastaccess'] = time();
		$_SESSION['session.timeout'] = DEFAULT_SESSION_TIMEOUT;
		$_SESSION['is.default.password'] = ($password == 'ubnt');
		$_SESSION['token'] = sha1(session_id() . uniqid());
        $response = array();
        $response['token'] = $_SESSION['token'];
        send_success($response);
	} else {
		$error_msg = "Invalid credentials.";
        send_error($error_msg, 400);
	}
}
session_write_close();

<?php
require_once('settings.inc');
require_once('api.inc');

session_start();

$response = array();
$info = array();
$info['systems'] = get_systems();
$info['auth_default'] = get_auth_default();
$info['version'] = get_version();
if (isset($_SESSION)) {
    $info['token'] = $_SESSION['token'];
}
$response[] = $info;
send_success($response);

function get_systems() {
    if (!isset($_SERVER['HTTP_HOST'])) {
        $c_host = $_SERVER['SERVER_ADDR'];
    } else {
        $c_host = $_SERVER['HTTP_HOST'];
    }
    $c_proto = "https://";
    $unifi_installed = @file_exists(UNIFI_CONFIGFILE);
    $uvc_installed = @file_exists(UVC_CONFIGFILE);
    $mfi_installed = @file_exists(MFI_CONFIGFILE);

    $uvc_port = get_app_port(UVC_CONFIGFILE, 'app.https.port', 7443);
    $unifi_port = get_app_port(UNIFI_CONFIGFILE, 'unifi.https.port', 8443);
    $mfi_port = get_app_port(MFI_CONFIGFILE, 'unifi.https.port', 6443);

    $uvc_href = $c_proto . $c_host . ':' . $uvc_port;
    $unifi_href = $c_proto . $c_host . ':' . $unifi_port;
    $mfi_href = $c_proto . $c_host . ':' . $mfi_port;

    $uvc_version = $uvc_installed ? get_package_version('unifi-video*') : null;
    $unifi_version = $unifi_installed ? get_package_version('unifi') : null;
    $mfi_version = null;

    $systems = array();
    $systems[] = build_response('uvc', $uvc_installed, $uvc_href, $uvc_version);
    $systems[] = build_response('unifi', $unifi_installed, $unifi_href, $unifi_version);
    $systems[] = build_response('mfi', $mfi_installed, $mfi_href, $mfi_version);
    return $systems;
}


function build_response($name, $installed, $url, $version = null) {
    $response = array();
    $response['id'] = $name;
    $response['installed'] = $installed;
    $response['url'] = $url;
    $response['version'] = $version;
    return $response;
}

function get_auth_default() {
    return !empty($_SESSION['is.default.password']);
}
?>

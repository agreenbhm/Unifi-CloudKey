<?php
require_once('settings.inc');
session_start();
$_SESSION = array();
$_SESSION['authed'] = false;
session_destroy();
Header("Location: /");
?>

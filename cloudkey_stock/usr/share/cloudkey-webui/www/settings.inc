<?php
ini_set('register_globals', 0);
ini_set('allow_url_fopen', 'Off');
ini_set('allow_url_include', 'Off');
ini_set('display_errors', 'Off');
ini_set('error_reporting', 'E_ALL & ~E_DEPRECATED & ~E_STRICT');
ini_set('variables_order', 'GPCS');
ini_set('short_open_tag', 'Off');
ini_set('output_buffering', 1024);
ini_set('expose_php', 'Off');
ini_set('default_charset', 'UTF-8');
ini_set('max_execution_time', 800);
ini_set('max_input_time', 600);
ini_set('file_uploads', TRUE);
ini_set('post_max_size', '512M');
ini_set('upload_max_filesize', '502M');
ini_set('upload_tmp_dir', '/var/php/uploads');
ini_set('session.name', 'CKSESSIONID');
ini_set('session.hash_function', 1);
ini_set('session.save_path', '/run/php/sessions');
ini_set('session.hash_bits_per_character', 5);

define('DEFAULT_SESSION_TIMEOUT', 900);

define('UVC_CONFIGFILE', '/usr/lib/unifi-video/data/system.properties');
define('UNIFI_CONFIGFILE', '/var/run/unifi_runtime.cfg');
define('MFI_CONFIGFILE', '/usr/lib/mfi/data/system.properties');
define('DOWNLOAD_PROGRESS_FILE', '/var/tmp/fwdownloadprogress');
define('RESTORE_STATUS_FILE', '/var/tmp/restorestatus');
define('FW_FILE', '/var/tmp/fwupdate.bin');
define('BACKUP_FILE', '/var/tmp/backup.unf');
define('AUTOBACKUP_DIR', '/data/autobackup');
define('AUTOBACKUP_GET_LIST', '/usr/bin/sudo /bin/cat ' . AUTOBACKUP_DIR . '/autobackup_meta.json');
define('INTERNAL_AUTOBACKUP_DIR', '/srv/unifi/data/backup/autobackup');
define('INTERNAL_AUTOBACKUP_GET_LIST', '/usr/bin/sudo /bin/cat ' . INTERNAL_AUTOBACKUP_DIR . '/autobackup_meta.json');

define('UBNTTOOLS_CONFIG', '/etc/default/ubnt-tools');
define('INFCTLD_CONFIG', '/etc/default/infctld');

define('CMD_SYSTOOL', '/usr/bin/sudo /sbin/ubnt-systool');
define('CMD_APTGET', '/usr/bin/sudo /usr/bin/apt-get');
define('CMD_APTCACHE', '/usr/bin/sudo /usr/bin/apt-cache');
define('CMD_APTGET_RUNNING', 'ps -C apt-get,dpkg >/dev/null');
define('CMD_WGET_RUNNING', 'ps -C wget >/dev/null');
define('CMD_RESTORE_RUNNING', 'ps -C java >/dev/null');
define('CMD_BACKGROUND', ' > /dev/null 2>/dev/null &');
define('CMD_WGET', 'wget ');
define('CMD_CURL', 'curl ');
define('CMD_SERVICE_UNIFI', '/usr/bin/sudo /usr/sbin/service unifi ');
define('CMD_WGET_OPTIONS', ' -O ' . FW_FILE . ' --progress=dot 2>&1 | grep --line-buffered "%" | sed -u -e "s,\.,,g" | awk \'{printf("\b\b\b\b%4s", $2) > "' . DOWNLOAD_PROGRESS_FILE . '" }\' ' . CMD_BACKGROUND);
define('CMD_ACE_JAR', '/usr/bin/sudo /usr/bin/java -jar /usr/lib/unifi/lib/ace.jar');
define('CMD_SYSTEMCTL', 'systemctl');

define('CMD_PWCHECK', CMD_SYSTOOL . ' pwcheck');
define('CMD_CHPASSWD', CMD_SYSTOOL . ' chpasswd');
define('CMD_CHUSERNAME', CMD_SYSTOOL . ' adminname');
define('CMD_REBOOT', CMD_SYSTOOL . ' reboot');
define('CMD_POWEROFF', CMD_SYSTOOL . ' poweroff');
define('CMD_RESET2DEFAULTS', CMD_SYSTOOL . ' reset2defaults');
define('CMD_CHHOSTNAME', CMD_SYSTOOL . ' hostname');
define('CMD_CHTIMEZONE', CMD_SYSTOOL . ' timezone');
define('CMD_CHRESETBUTTON', CMD_SYSTOOL . ' resetbutton');
define('CMD_CHNETWORK', CMD_SYSTOOL . ' network');
define('CMD_FWUPDATE', CMD_SYSTOOL . ' fwupdate');
define('CMD_RESTORE', CMD_ACE_JAR . ' restore ' . BACKUP_FILE . ' >> ' . RESTORE_STATUS_FILE . ' 2> /dev/null &');
define('CMD_RESTORE_WITH_FILENAME', CMD_ACE_JAR . ' restore %s >> ' . RESTORE_STATUS_FILE . ' 2> /dev/null &');

define('DEFAULT_IP', '192.168.1.30');
define('DEFAULT_NETMASK', '255.255.255.0');
define('DEFAULT_GATEWAY', '192.168.1.1');
define('DEFAULT_DNS1', '208.67.222.222');
define('DEFAULT_DNS2', '208.67.220.220');
?>

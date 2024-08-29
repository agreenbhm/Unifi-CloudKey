<?php
require_once('settings.inc');

function is_session_valid($start_session = true, $token_check = false) {
	if ($start_session) {
		session_start();
	}
	$now = time();
	if ($_SERVER['REMOTE_ADDR'] == '127.0.0.1') {
		$_SESSION['authed'] = true;
		$_SESSION['username'] = 'root';
	} else {
		if ($_SESSION['authed'] != true) {
			return false;
		}

		$diff = $now - $_SESSION['lastaccess'];
		if ($diff > $_SESSION['session.timeout']) {
			return false;
		}

		if ($token_check && ($_SESSION['token'] != $_SERVER['HTTP_X_ACCESS_TOKEN'])) {
			return false;
		}
	}
	$_SESSION['lastaccess'] = $now;
	return true;
}

function dns_entry_process($fromstring, $removebracket) {
	$res = array();
	$lines = explode("\n", $fromstring);
	foreach ($lines as $line) {
		if ($removebracket) {
			$res[] = preg_replace('/^DNS\[\]/s', 'DNS', $line);
		} else {
			$res[] = preg_replace('/^DNS/s', 'DNS[]', $line);
		}
	}
	return implode("\n", $res);
}

/**
 * simple parser for /etc/systemd/network/*.network files, that supports only single
 * interface [Match] section.
 */
function systemd_networkd_config_parse($file) {
	$config = array();
	if (!(is_file($file) || is_link($file)))
		return $config;

	$rawcfg = @parse_ini_string(dns_entry_process(file_get_contents($file), FALSE), TRUE, INI_SCANNER_RAW);
	if ($rawcfg === false)
		return $config;

	$options = array();
	$config['name'] = $rawcfg['Match']['Name'];
	$config['family'] = 'inet'; // TODO: proper ipv4/ipv6/both detection!
	if (array_key_exists('DHCP', $rawcfg['Network'])) {
		$dhcp = $rawcfg['Network']['DHCP'];
	} else {
		$dhcp = 'no';
	}
	if ($dhcp == 'yes' || $dhcp == 'ipv4' || $dhcp == 'ipv6') {
		$config['method'] = 'dhcp';
	} else {
		$config['method'] = 'static';
		$addr = $rawcfg['Address']['Address'];
		$netmask = "255.255.255.0";
		$pos = strpos($addr, '/');
		if ($pos !== FALSE) {
			$netmask_c = substr($addr, $pos + 1);
			$addr = substr($addr, 0, $pos);
			if ($netmask_c > 0 && $netmask_c <= 32) {
				$mask = ~((1 << (32 - $netmask_c)) - 1);
				$netmask = long2ip($mask);
			}
		}
		$options['address'] = $addr;
		$options['netmask'] = $netmask;
		$options['gateway'] = $rawcfg['Route']['Gateway'];
		$options['dns-nameservers'] = $rawcfg['Network']['DNS'];
	}
	if (!empty($rawcfg['Fallback'])) {
		$options['dhcp-fallback-ip'] = arrayValueOrNull('Address', $rawcfg['Fallback']);
		$options['dhcp-fallback-netmask'] = arrayValueOrNull('Netmask', $rawcfg['Fallback']);
		$options['dhcp-fallback-gateway'] = arrayValueOrNull('Gateway', $rawcfg['Fallback']);
	}
	$config['options'] = $options;
	return $config;
}

function mask2cidr($mask) {
    $long = ip2long($mask);
    $base = ip2long('255.255.255.255');
    return 32 - log(($long ^ $base) + 1, 2);
}

function write_ini_to_string($assoc_arr, $has_sections=TRUE) {
	$res = array();
	if ($has_sections) {
		foreach ($assoc_arr as $key => $elem) {
			if (count($elem) == 0) continue;
			$res[] = "[$key]";
			foreach ($elem as $key2 => $elem2) {
				if (is_array($elem2)) {
					for ($i = 0; $i < count($elem2); $i++) {
						if ($elem2[$i] != "") $res[] = $key2."[] = ".$elem2[$i];
					}
				} else {
					if ($elem2 != "") $res[] = $key2." = ".$elem2;
				}
			}
		}
	} else {
		foreach ($assoc_arr as $key => $elem) {
			if (is_array($elem)) {
				for ($i = 0; $i < count($elem); $i++) {
					if ($elem[$i] != "") $res[] = $key."[] = ".$elem[$i];
				}
			} else {
				if ($elem != "") $res[] = $key." = ".$elem;
			}
		}
	}

    return implode("\n", $res);
}

function systemd_networkd_config_write($file, $config) {

    $rawcfg = array();
    $match = array();
    $address = array();
    $route = array();
    $network = array();

    $options = $config['options'];

    if (!array_key_exists('name', $config)) {
        return -1;
    }
    $match['Name'] = $config['name'];
    $rawcfg['Match'] = $match;

    if (!array_key_exists('method', $config)) {
        return -1;
    }

    if ($config['method'] == 'dhcp') {

        $network['DHCP'] = 'yes';
        $rawcfg['Network'] = $network;

    } elseif ($config['method'] == 'static') {

        if (array_key_exists('netmask', $options)) {
            $bits = mask2cidr($options['netmask']);
        } else {
            $bits = 24;
        }
        if (!array_key_exists('address', $options)) {
            return -1;
        }
        $address['Address'] = $options['address'] . '/' . $bits;
        $rawcfg['Address'] = $address;

        if (array_key_exists('gateway', $options)) {
            $route['Gateway'] = $options['gateway'];
            $rawcfg['Route'] = $route;
        }

        if (array_key_exists('dns-nameservers', $options)) {
            $network['DNS'] = $options['dns-nameservers'];
            $rawcfg['Network'] = $network;
        }
    } else {
        return -1;
    }

    $fallback = array();
    if (array_key_exists('dhcp-fallback-ip', $options)) $fallback['Address'] = $options['dhcp-fallback-ip'];
    if (array_key_exists('dhcp-fallback-netmask', $options)) $fallback['Netmask'] = $options['dhcp-fallback-netmask'];
    if (array_key_exists('dhcp-fallback-gateway', $options)) $fallback['Gateway'] = $options['dhcp-fallback-gateway'];
    $rawcfg['Fallback'] = $fallback;

	return file_put_contents($file, dns_entry_process(write_ini_to_string($rawcfg, TRUE), TRUE)."\n");
}

/**
 * simplistic parser for /etc/network/interfaces - returns hierarchical hashmap in form:
 *  'auto' : [ <interfaces marked as auto or allow-auto> ]
 *  'allow-*' : [ <interfaces marked with matching allow-* stanza> ]
 *  'ifaces' : [  <family> (inet|inet6|etc) : [
 *      		<ifname> (lo, eth0 etc) : {
 *                       name :
 *                       family :
 *                       method :
 *                       options : [
 *                           <option-key> : <option value>
 *                       ]
 *                 ]
 *   ]
 * NOTE: mapping and source stanzas are not supported!
 */
function network_interface_config_parse($file) {
	$config = array();
	if (!(is_file($file) || is_link($file)))
		return $config;
	$lines = @file($file);
	if ($lines === false)
		return $config;
	$iface = array();
	foreach ($lines as $line) {
		$trline = trim($line);
		# ignore empty / commented lines
		if (empty($trline) || $trline{0} == '#') {
			continue;
		}
		$words = preg_split('/\s/', $trline, -1, PREG_SPLIT_NO_EMPTY);
		# there are no single word options afaik
		if (count($words) < 2)
			continue;
		if ($words[0] == 'iface') {
			if (!empty($iface['name']) && !empty($iface['family'])) {
				$config['ifaces'][$iface['family']][$iface['name']] = $iface;
				$iface = array();
			}
			# TODO: check number of words
			$iface['name'] = $words[1];
			$iface['family'] = $words[2];
			$iface['method'] = $words[3];
		} else if (preg_match('/^\s/', $line)){
			# for option case re-split into 2 words only
			$keyval = preg_split('/\s/', $trline, 2, PREG_SPLIT_NO_EMPTY);
			$iface['options'][$keyval[0]] = trim($keyval[1]);
		} else if ($words[0] == 'auto' || strncmp($words[0], 'allow-auto', 10) === 0) {
			for ($i = 1; $i < count($words); ++$i) {
				$config['auto'][] = $words[$i];
			}
		} else if (strncmp($words[0], 'allow-', 6) === 0) {
			for ($i = 1; $i < count($words); ++$i) {
				$config[$words[0]][] = $words[$i];
			}
		}
	}

	if (!empty($iface['name']) && !empty($iface['family'])) {
		$config['ifaces'][$iface['family']][$iface['name']] = $iface;
	}
	return $config;
}

/**
 * saves hashmap (as defined in {@link network_interface_config_parse} to a file.
 * as there, mappings are not supported.
 */
function network_interface_config_write($filename, $config) {
	$lines = array();
	if (!empty($config['auto'])) {
		$lines[] = 'auto ' . implode(' ', $config['auto']);
	}
	reset($config);
	while (list($key, $value) = each($config)) {
		if (strncmp($key, 'allow-', 6) === 0) {
			$lines[] = $key . ' ' . implode(' ', $value);
		}
	}
	reset($config);
	$families = &$config['ifaces'];
	while (list($key, $value) = each($families)) {
		$ifaces = &$value;
		foreach ($ifaces as $iface) {
			$lines[] = '';
			$lines[] = 'iface ' . $iface['name'] . ' ' . $iface['family'] . ' ' . $iface['method'];
			if (!empty($iface['options'])) {
				while (list($opkey,$opvalue) = each($iface['options'])) {
					$lines[] = "\t  " . $opkey . ' ' . $opvalue;
				}
			}
		}
	}
	$lines[] = '';

	return file_put_contents($filename, implode("\n", $lines));
}

function network_interface_config_get($interface_file, $family, $interface) {
	$config = network_interface_config_parse($interface_file);

	$iface_cfg = $config['ifaces'][$family][$interface];
	if (empty($iface_cfg)) {
		$iface_cfg = array();
	}

	return $iface_cfg;
}

function resolv_conf_get_servers($file) {
	$servers = array();
	if (!(is_file($file) || is_link($file)))
		return $servers;
	$file = @file($file);
	if ($file === false)
		return $servers;
	foreach ($file as $line) {
		$words = preg_split('/\s/', $line, 7, PREG_SPLIT_NO_EMPTY);
		if (count($words) > 1 && $words[0] == 'nameserver') {
			$servers[] = $words[1];
		}
	}
	return $servers;
}

function file_contents($file, $default_value) {
	$value = @file_get_contents($file);
	if ($value === false) {
		$value = $default_value;
	}
	return trim($value);
}

function get_system_timezone() {
	return file_contents('/etc/timezone', 'UTC');
}

function get_version() {
	if (file_exists('/usr/lib/version')) {
		return file_contents('/usr/lib/version', 'unknown');
	} else {
		return file_contents('/etc/version', 'unknown');
	}
}

function get_hostname() {
	return file_contents('/proc/sys/kernel/hostname', 'unknown');
}

function get_uptime() {
	$uptime_str = file_contents('/proc/uptime', 0);
	return strtok($uptime_str, '.');
}

function get_diskstats($path) {
	$disk = array();
	$df = shell_exec('df -T -B 1 ' . $path);
	$df = trim($df);
	$df_lines = preg_split('/\n/', $df, 2, PREG_SPLIT_NO_EMPTY);
	$keys = preg_split('/\s/', $df_lines[0], 7, PREG_SPLIT_NO_EMPTY);
	$values = preg_split('/\s/', $df_lines[1], 7, PREG_SPLIT_NO_EMPTY);
	for ($i = 0; $i < count($keys); $i++) {
		$disk[$keys[$i]] = trim($values[$i]);
	}
	return $disk;
}

function get_package_info($packages) {
	$cmd = 'dpkg-query -W -f=\'${db:Status-Abbrev}\t${Package}\t${Version}\n\' ' . $packages . ' | grep ^ii';
	$out = exec($cmd, $output, $rc);
	if ($rc !== 0) {
		return null;
	}

	$pkgs = array();
	for ($i = 0; $i < count($output); $i++) {
		$status = explode("\t", $output[$i]);
		if (strncmp('ii', $status[0], 2) === 0) {
			$pkgs[$status[1]] = $status[2];
		}
	}
	return $pkgs;
}

function get_package_version($package) {
	$cmd = 'dpkg-query -W -f=\'${db:Status-Abbrev}\t${Version}\n\' ' . $package . ' | grep ^ii';
	$out = exec($cmd, $output, $rc);
	if ($rc !== 0) {
		return null;
	}

	$status = explode("\t", $output[0]);
	if (strncmp('ii', $status[0], 2) === 0) {
		return $status[1];
	}

	return null;
}

function pwcheck($user, $password) {
	$ph = @popen(CMD_PWCHECK, 'w');
	if (!is_resource($ph)) {
		return 1;
	}
	@fwrite($ph, $user . "\n" . $password . "\n");
	@fflush($ph);
	$rc = @pclose($ph);
	return $rc;
}

function chpasswd($user, $old_passwd, $new_passwd) {
	$rc = pwcheck($user, $old_passwd);
	if ($rc != 0) {
		return $rc;
	}
	$ph = @popen(CMD_CHPASSWD, 'w');
	if (!is_resource($ph)) {
		return 11;
	}
	@fwrite($ph, $user . ':' . $new_passwd . "\n");
	@fflush($ph);
	$rc = @pclose($ph);
	return $rc;
}

function reboot() {
	exec(CMD_REBOOT);
}

function poweroff() {
	exec(CMD_POWEROFF);
}

function reset2defaults() {
	exec(CMD_RESET2DEFAULTS);
}

function change_timezone($tzname) {
	if (is_file('/usr/share/zoneinfo/' . $tzname)) {
		exec(CMD_CHTIMEZONE . ' ' . escapeshellcmd($tzname), $out, $rc);
	} else {
		$rc = 1;
	}
	return $rc;
}

function is_valid_hostname($hostname) {
	return (preg_match("/^[0-9a-z]([\-._]?[0-9a-z]+)*$/i", $hostname) && preg_match("/^.{1,64}$/", $hostname));
}

function change_hostname($hostname) {
	if (is_valid_hostname($hostname)) {
		exec(CMD_CHHOSTNAME . ' ' . escapeshellcmd($hostname), $out, $rc);
	} else {
		$rc = 1;
	}
	return $rc;
}

function change_network($netfile) {
	exec(CMD_CHNETWORK . ' ' . escapeshellcmd($netfile), $out, $rc);
	return $rc;
}

function ifconfig_get_addr($ifname) {
	$cmd = '/sbin/ifconfig ' . escapeshellcmd($ifname) . ' | grep \'inet addr\' 2>/dev/null';
	$out = exec($cmd, $output, $rc);
	if ($rc !== 0) {
		return array();
	}
	$chunks = preg_split('/\s+/', $output[0], -1, PREG_SPLIT_NO_EMPTY);
	$ifaddr = array();
	foreach ($chunks as $chunk) {
		$v = explode(':', $chunk);
		if (count($v) == 2) {
			$ifaddr[strtolower($v[0])] = $v[1];
		}
	}

	return $ifaddr;
}

function ifconfig_get_hwaddr($ifname) {
	return file_contents('/sys/class/net/' . $ifname . '/address', 'n/a');
}

function routes_get() {
	$out = shell_exec('/sbin/route -n 2>/dev/null');
	$out = trim($out);
	$lines = preg_split('/\n/', $out, -1, PREG_SPLIT_NO_EMPTY);
	$line_count = count($lines);
	if ($line_count < 3) {
		return array();
	}
	$keys = preg_split('/\s/', $lines[1], 8, PREG_SPLIT_NO_EMPTY);
	$key_count = count($keys);
	$routes = array();
	for ($l = 2; $l < $line_count; $l++) {
		$values = preg_split('/\s/', $lines[$l], 8, PREG_SPLIT_NO_EMPTY);
		$route = array();
		for ($i = 0; $i < $key_count; $i++) {
			$route[$keys[$i]] = trim($values[$i]);
		}
		if (count($route) == 8) {
			$routes[] = $route;
		}
	}
	return $routes;
}

function routes_get_default_gw() {
	$routes = routes_get();
	foreach ($routes as $r) {
		if ($r['Flags'] == 'UG') {
			return arrayValueOrNull('Gateway', $r);
		}
	}
	return null;
}

function inaddr2bitmask($inaddr) {
	$len = strlen($inaddr);
	if ($len == 16) {
		$unpacked = unpack('A16', $inaddr);
	} else if ($len == 4) {
		$unpacked = unpack('A4', $inaddr);
	} else {
		return "0";
	}
	$unpacked = str_split($unpacked[1]);
	$bin = '';
	foreach ($unpacked as $char) {
		$bin .= str_pad(decbin(ord($char)), 8, '0', STR_PAD_LEFT);
	}
	return $bin;
}

function to_cidr($addr, $netmask) {
	$ipaddr = inet_pton($addr);
	$mask = inet_pton($netmask);
	$bitmask = inaddr2bitmask($mask);
	$bitcount = strpos($bitmask, '0');

	$addr_cidr = inet_ntop($ipaddr);
	if ($addr_cidr !== FALSE) {
		$addr_cidr .= '/' . $bitcount;
	} else {
		$addr_cidr = $addr . '/' . $bitcount;
	}
	return $addr_cidr;
}

function arrayValueOrNull($key, $array) {
	if (array_key_exists($key, $array)) {
		return $array[$key];
	}
	return null;
}

?>

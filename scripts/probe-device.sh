#!/bin/sh
# Device probe for pbnotes (research R-2, R-6, R-11). Run on the InkPad One (for example over
# SSH from a KOReader install) and paste the output into specs/001-pocketbook-notes-mvp/research.md.
#
#   sh probe-device.sh
#
# Prints no secrets. TLS handshakes are always done with certificate verification ON.

echo "== system"
uname -a 2>&1
echo

echo "== Qt runtime (R-2)"
ls -l /usr/lib/libQt6Core.so* /usr/lib/libQt6Quick.so* /usr/lib/libQt6Qml.so* 2>&1 | head -12
echo "-- QML modules in /usr/qml:"
ls /usr/qml 2>&1 | head -30
echo "-- QPA platform plugins:"
ls /usr/plugins/platforms 2>&1
echo

echo "== CA certificate candidates (R-6)"
for f in /etc/ssl/certs/ca-certificates.crt /etc/ssl/cert.pem /etc/ssl/certs/ca-bundle.crt; do
    if [ -f "$f" ]; then
        n=$(grep -c 'BEGIN CERTIFICATE' "$f" 2>/dev/null)
        echo "$f: $(wc -c < "$f") bytes, $n certificates"
    else
        echo "$f: not present"
    fi
done
echo "-- other files that look like CA material:"
find /etc/ssl /ebrmain -maxdepth 3 \( -iname '*ca*' -o -iname '*cert*' \) 2>/dev/null | head -50
echo

echo "== HTTPS handshake with each existing candidate (verification ON)"
if command -v curl >/dev/null 2>&1; then
    for f in /etc/ssl/certs/ca-certificates.crt /etc/ssl/cert.pem /etc/ssl/certs/ca-bundle.crt; do
        [ -f "$f" ] || continue
        code=$(curl --cacert "$f" -sS -m 20 -o /dev/null -w '%{http_code}' https://curl.se/ 2>&1)
        echo "$f -> $code"
    done
else
    echo "curl not available on this device; test the handshake from the app instead."
fi
echo

echo "== storage and app data location (R-6 custom-ca.pem)"
df -k /mnt/ext1 2>&1 | tail -n 2
if [ -d /mnt/ext1/system ]; then
    t=/mnt/ext1/system/.pbnotes-probe.$$
    if : > "$t" 2>/dev/null; then
        echo "/mnt/ext1/system exists and is writable"
        rm -f "$t"
    else
        echo "/mnt/ext1/system exists but is NOT writable"
    fi
else
    echo "/mnt/ext1/system does not exist"
fi
echo

echo "== note"
echo "QueryNetwork() (R-11) cannot be called from a shell: start the pbnotes Hello World with"
echo "Wi-Fi on and off and read the 'net=' value in /mnt/ext1/system/pbnotes/hello.log."

#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Gerçek irssi ile elle doğrulama yardımcısı.
# Sunucuyu başlatır ve irssi kuruluysa doğrudan bağlanır.
#
#   ./tests/irssi_manual.sh [port] [sifre]
# ---------------------------------------------------------------------------

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-6667}"
PASSWORD="${2:-ftirc42}"

[ -x "$ROOT/ircserv" ] || { echo "ircserv yok, önce 'make' çalıştırın"; exit 1; }

"$ROOT/ircserv" "$PORT" "$PASSWORD" &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null' EXIT INT TERM
sleep 0.5

echo
echo "Sunucu çalışıyor: 127.0.0.1:$PORT  (şifre: $PASSWORD)"
echo

if ! command -v irssi >/dev/null 2>&1; then
    cat <<EOF
irssi kurulu değil. Kurmak için:

    brew install irssi

Kurulduktan sonra elle bağlanmak için:

    irssi -c 127.0.0.1 -p $PORT -w $PASSWORD -n testnick

Denenecek komutlar:
    /join #test
    /topic #test yeni konu
    /mode #test +t
    /mode #test +i
    /invite arkadas #test
    /mode #test +k gizli
    /mode #test +l 5
    /mode #test +o arkadas
    /kick #test arkadas sebep
    /part #test
    /quit

Sunucuyu durdurmak için Ctrl+C.
EOF
    wait $SERVER_PID
    exit 0
fi

echo "irssi başlatılıyor... (çıkmak için /quit)"
sleep 1
irssi -c 127.0.0.1 -p "$PORT" -w "$PASSWORD" -n "${USER:-tester}"

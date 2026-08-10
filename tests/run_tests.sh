#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# ft_irc otomatik test paketi
#
# irssi'nin gerçek protokol davranışını taklit eder:
#   - Bağlantı açılır açılmaz "CAP LS 302" gönderir ve cevap bekler
#   - PASS / NICK / USER dizisini TEK bir TCP paketinde yollar
#   - "CAP END" sonrası 001 karşılama numeriğini bekler
#   - Düzenli PING gönderir, PONG gelmezse bağlantıyı düşürür
#   - JOIN sonrası MODE ve WHO sorguları yapar
#
# Kullanım:
#   ./tests/run_tests.sh            # tüm testler
#   ./tests/run_tests.sh -v         # sunucu ve oturum çıktılarını da göster
#   ./tests/run_tests.sh -p 6669    # belirli port
# ---------------------------------------------------------------------------

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="$ROOT/ircserv"
PASSWORD="ftirc42"
PORT=""
VERBOSE=0

while [ $# -gt 0 ]; do
    case "$1" in
        -v|--verbose) VERBOSE=1; shift ;;
        -p|--port)    PORT="$2"; shift 2 ;;
        -h|--help)    sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "Bilinmeyen argüman: $1"; exit 1 ;;
    esac
done

if [ -t 1 ]; then
    RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[0;33m'
    BLUE=$'\033[0;34m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
else
    RED=""; GREEN=""; YELLOW=""; BLUE=""; BOLD=""; RESET=""
fi

TMP="$(mktemp -d "${TMPDIR:-/tmp}/ftirc_test.XXXXXX")"
SERVER_LOG="$TMP/server.log"
SERVER_PID=""
SESSIONS=""
NEXT_FD=10

PASS_COUNT=0
FAIL_COUNT=0
FAILED_NAMES=""

# ---------------------------------------------------------------------------
# Altyapı
# ---------------------------------------------------------------------------

cleanup() {
    for name in $SESSIONS; do
        session_close "$name" >/dev/null 2>&1
    done
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1
        wait "$SERVER_PID" 2>/dev/null
    fi
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

die() { echo "${RED}FATAL:${RESET} $*" >&2; exit 1; }

find_free_port() {
    local candidate
    for _ in $(seq 1 50); do
        candidate=$(( 6700 + RANDOM % 900 ))
        if ! nc -z 127.0.0.1 "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done
    die "boş port bulunamadı"
}

start_server() {
    [ -x "$BINARY" ] || die "ircserv bulunamadı, önce 'make' çalıştırın"
    "$BINARY" "$PORT" "$PASSWORD" > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!

    local i=0
    while [ $i -lt 50 ]; do
        if nc -z 127.0.0.1 "$PORT" >/dev/null 2>&1; then
            return 0
        fi
        kill -0 "$SERVER_PID" 2>/dev/null || die "sunucu başlatılamadı:\n$(cat "$SERVER_LOG")"
        sleep 0.1
        i=$(( i + 1 ))
    done
    die "sunucu $PORT portunda dinlemeye başlamadı"
}

# Bir istemci oturumu açar. Her oturumun kendi fifo'su (giriş) ve log'u (çıkış) olur.
session_open() {
    local name="$1"
    local fifo="$TMP/$name.in"
    local out="$TMP/$name.out"

    mkfifo "$fifo"
    : > "$out"

    nc 127.0.0.1 "$PORT" < "$fifo" > "$out" 2>/dev/null &
    eval "PID_$name=\$!"

    # fifo'yu açık tutmak için kalıcı bir dosya tanımlayıcı ayır (bash 3.2 uyumlu)
    eval "exec $NEXT_FD> \"$fifo\""
    eval "FD_$name=$NEXT_FD"
    NEXT_FD=$(( NEXT_FD + 1 ))

    SESSIONS="$SESSIONS $name"
    sleep 0.1
}

# Satırları CRLF ile ve TEK write ile gönderir (irssi de böyle davranır).
session_send() {
    local name="$1"; shift
    local fd; eval "fd=\$FD_$name"
    local payload=""
    local line
    for line in "$@"; do
        payload="$payload$line"$'\r'$'\n'
    done
    printf '%s' "$payload" >&"$fd" 2>/dev/null || true
}

# Ham veri gönderir (satır sonu eklemez) — kısmi paket testleri için.
session_send_raw() {
    local name="$1"; shift
    local fd; eval "fd=\$FD_$name"
    printf '%s' "$1" >&"$fd" 2>/dev/null || true
}

session_out() { echo "$TMP/$1.out"; }

session_close() {
    local name="$1"
    local fd pid
    eval "fd=\${FD_$name:-}"
    eval "pid=\${PID_$name:-}"
    [ -n "$fd" ] && eval "exec $fd>&-" 2>/dev/null
    [ -n "$pid" ] && kill "$pid" >/dev/null 2>&1
    eval "unset FD_$name PID_$name"
}

# irssi'nin bağlantı açılışını birebir taklit eder.
irssi_handshake() {
    local name="$1" nick="$2" pass="${3:-$PASSWORD}"
    session_open "$name"
    # irssi CAP LS'i ayrı yollar, ardından PASS/NICK/USER'ı tek pakette gönderir
    session_send "$name" "CAP LS 302"
    session_send "$name" "PASS $pass" "NICK $nick" "USER $nick 0 * :$nick realname"
    wait_for "$name" "CAP" 20 >/dev/null
    session_send "$name" "CAP END"
}

# Belirtilen metin oturum çıktısında görünene kadar bekler (0.1s adımlarla).
wait_for() {
    local name="$1" needle="$2" tries="${3:-30}"
    local out; out="$(session_out "$name")"
    local i=0
    while [ $i -lt "$tries" ]; do
        if grep -qF -- "$needle" "$out" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        i=$(( i + 1 ))
    done
    return 1
}

# ---------------------------------------------------------------------------
# Assertion'lar
# ---------------------------------------------------------------------------

ok()   { PASS_COUNT=$(( PASS_COUNT + 1 )); echo "  ${GREEN}✔${RESET} $1"; }
nok()  {
    FAIL_COUNT=$(( FAIL_COUNT + 1 ))
    FAILED_NAMES="$FAILED_NAMES|$1"
    echo "  ${RED}✘${RESET} $1"
    [ -n "${2:-}" ] && echo "      ${YELLOW}beklenen:${RESET} $2"
    if [ "$VERBOSE" -eq 1 ] && [ -n "${3:-}" ]; then
        echo "      ${YELLOW}alınan:${RESET}"
        sed 's/\r$//; s/^/        /' "$3" | tail -20
    fi
    return 0
}

# expect <oturum> <beklenen metin> <açıklama>
expect() {
    local name="$1" needle="$2" desc="$3" tries="${4:-30}"
    if wait_for "$name" "$needle" "$tries"; then
        ok "$desc"
    else
        nok "$desc" "$needle" "$(session_out "$name")"
    fi
}

# refute <oturum> <olmaması gereken metin> <açıklama>
refute() {
    local name="$1" needle="$2" desc="$3"
    sleep 0.4
    if grep -qF -- "$needle" "$(session_out "$name")" 2>/dev/null; then
        nok "$desc" "'$needle' GÖRÜNMEMELİYDİ" "$(session_out "$name")"
    else
        ok "$desc"
    fi
}

section() { echo; echo "${BOLD}${BLUE}▸ $1${RESET}"; }

# ---------------------------------------------------------------------------
# Testler
# ---------------------------------------------------------------------------

test_handshake() {
    section "Kayıt akışı (irssi handshake)"

    session_open h1
    session_send h1 "CAP LS 302"
    expect h1 "CAP * LS" "CAP LS isteğine cevap veriliyor (irssi burada bekler)"

    # irssi gerçek hayatta bu üç satırı tek TCP paketinde gönderir
    session_send h1 "PASS $PASSWORD" "NICK alice" "USER alice 0 * :Alice Liddell"
    refute h1 " 001 " "CAP END gelmeden kayıt tamamlanmıyor"

    session_send h1 "CAP END"
    expect h1 " 001 alice :" "001 RPL_WELCOME gönderiliyor"
    expect h1 " 002 alice" "002 RPL_YOURHOST gönderiliyor"
    expect h1 " 004 alice" "004 RPL_MYINFO gönderiliyor"
    expect h1 " 005 alice" "005 ISUPPORT gönderiliyor"
    expect h1 " 376 alice" "376 MOTD sonu gönderiliyor"
}

test_pipelined_registration() {
    section "Tek pakette gelen komut zinciri"

    # Eski kodda extractCommand() boş satırda döngüyü kırıyordu:
    # aynı pakette gelen sonraki komutlar hiç işlenmiyordu.
    session_open p1
    session_send_raw p1 $'CAP LS 302\r\n\r\nPASS '"$PASSWORD"$'\r\nNICK pipe1\r\nUSER pipe1 0 * :Pipe\r\nCAP END\r\n'
    expect p1 " 001 pipe1 :" "Aradaki boş satır komut akışını durdurmuyor"

    # Komut ortadan ikiye bölünürse buffer birleştirmeli
    session_open p2
    session_send p2 "CAP LS 302"
    session_send_raw p2 "PASS $PASSWORD"$'\r\n'"NIC"
    sleep 0.3
    session_send_raw p2 $'K pipe2\r\nUSER pipe2 0 * :Pipe\r\nCAP END\r\n'
    expect p2 " 001 pipe2 :" "Yarım gelen komut sonraki pakette tamamlanıyor"
}

test_password() {
    section "Şifre doğrulama"

    session_open w1
    session_send w1 "CAP LS 302"
    session_send w1 "PASS yanlissifre" "NICK mallory" "USER mallory 0 * :Mallory"
    expect w1 " 464 " "Yanlış şifre 464 ERR_PASSWDMISMATCH veriyor"
    expect w1 "ERROR :Closing link" "Yanlış şifrede bağlantı kapatılıyor"
    refute w1 " 001 " "Yanlış şifreyle karşılama gönderilmiyor"
}

test_ping_pong() {
    section "PING / PONG (irssi lag ölçümü)"

    session_send h1 "PING :LAG1755000000"
    expect h1 "PONG" "PING'e PONG dönülüyor"
    expect h1 "LAG1755000000" "PONG aynı token'ı taşıyor"
}

test_nick_rules() {
    section "NICK kuralları"

    irssi_handshake n1 bob
    expect n1 " 001 bob :" "İkinci istemci kayıt oluyor"

    session_send n1 "NICK alice"
    expect n1 " 433 " "Kullanılan nick 433 ERR_NICKNAMEINUSE veriyor"

    session_send n1 "NICK 9invalid"
    expect n1 " 432 " "Geçersiz nick 432 ERR_ERRONEUSNICKNAME veriyor"

    session_send n1 "NICK bobby"
    expect n1 "NICK :bobby" "Nick değişimi tam maske ile yayınlanıyor"
    session_send n1 "NICK bob"
    sleep 0.2
}

test_join() {
    section "JOIN / NAMES"

    session_send h1 "JOIN #test"
    expect h1 "alice!alice@" "JOIN yankısı nick!user@host maskesiyle geliyor"
    expect h1 "JOIN #test" "JOIN yankısı istemciye dönüyor"
    expect h1 " 353 alice = #test :@alice" "353 NAMES listesinde kurucu operatör (@)"
    expect h1 " 366 alice #test" "366 End of NAMES gönderiliyor"

    session_send n1 "JOIN #test"
    expect n1 " 353 bob = #test :" "İkinci üye NAMES listesini alıyor"
    expect h1 "bob!bob@" "Kanaldaki diğer üye JOIN bildirimini görüyor"

    session_send h1 "JOIN gecersizkanal"
    expect h1 " 403 " "Geçersiz kanal adı 403 ERR_NOSUCHCHANNEL veriyor"
}

test_privmsg() {
    section "PRIVMSG / NOTICE"

    session_send h1 "PRIVMSG #test :merhaba kanal"
    expect n1 "PRIVMSG #test :merhaba kanal" "Kanal mesajı diğer üyeye ulaşıyor"
    refute h1 "PRIVMSG #test :merhaba kanal" "Gönderen kendi kanal mesajını geri almıyor"

    session_send n1 "PRIVMSG alice :ozel mesaj"
    expect h1 "PRIVMSG alice :ozel mesaj" "Özel mesaj hedefe ulaşıyor"

    session_send h1 "PRIVMSG #yokboylekanal :selam"
    expect h1 " 403 " "Olmayan kanala mesaj 403 veriyor"

    session_send h1 "PRIVMSG hayaletkullanici :selam"
    expect h1 " 401 " "Olmayan kullanıcıya mesaj 401 veriyor"

    session_send h1 "PRIVMSG"
    expect h1 " 411 " "Hedefsiz PRIVMSG 411 veriyor"

    session_send n1 "NOTICE #test :dikkat"
    expect h1 "NOTICE #test :dikkat" "NOTICE kanala iletiliyor"
}

test_who_mode_query() {
    section "WHO / MODE sorguları (irssi JOIN sonrası bunları yollar)"

    session_send h1 "WHO #test"
    expect h1 " 352 alice #test" "352 WHO cevabı geliyor"
    expect h1 " 315 alice #test" "315 End of WHO geliyor"

    session_send h1 "MODE #test"
    expect h1 " 324 alice #test" "324 kanal modları sorgulanabiliyor"

    session_send h1 "MODE #test b"
    expect h1 " 368 " "Ban listesi sorgusu 368 ile sonlanıyor"

    session_send h1 "WHOIS bob"
    expect h1 " 311 alice bob" "311 WHOIS cevabı geliyor"
    expect h1 " 318 alice bob" "318 End of WHOIS geliyor"
}

test_topic() {
    section "TOPIC"

    session_send h1 "TOPIC #test"
    expect h1 " 331 " "Konu yokken 331 RPL_NOTOPIC dönüyor"

    session_send h1 "TOPIC #test :yeni konu"
    expect n1 "TOPIC #test :yeni konu" "Konu değişimi kanala yayınlanıyor"

    session_send n1 "TOPIC #test"
    expect n1 " 332 bob #test :yeni konu" "332 RPL_TOPIC dönüyor"

    session_send h1 "MODE #test +t"
    expect n1 "MODE #test +t" "+t modu yayınlanıyor"

    session_send n1 "TOPIC #test :izinsiz degisiklik"
    expect n1 " 482 " "+t iken operatör olmayan konuyu değiştiremiyor"
}

test_mode_operator() {
    section "MODE +o / KICK"

    session_send n1 "KICK #test alice :deneme"
    expect n1 " 482 " "Operatör olmayan KICK atamıyor"

    session_send h1 "MODE #test +o bob"
    expect n1 "MODE #test +o bob" "+o modu yayınlanıyor"

    session_send n1 "KICK #test alice :hoscakal"
    expect h1 "KICK #test alice :hoscakal" "Operatör olan KICK atabiliyor"

    session_send h1 "PRIVMSG #test :hala burada miyim"
    expect h1 " 404 " "Atılan kullanıcı kanala mesaj gönderemiyor"

    # bob operatör olarak kalır; sonraki mod testleri bu yetkiyi kullanır
}

test_mode_invite_key_limit() {
    section "MODE +i / +k / +l"

    irssi_handshake c1 carol
    expect c1 " 001 carol :" "Üçüncü istemci kayıt oluyor"

    session_send n1 "MODE #test +i"
    session_send c1 "JOIN #test"
    expect c1 " 473 " "+i kanalına davetsiz girilemiyor"

    session_send n1 "INVITE carol #test"
    expect n1 " 341 " "341 RPL_INVITING gönderiliyor"
    expect c1 "INVITE carol :#test" "Davet mesajı hedefe ulaşıyor"

    session_send c1 "JOIN #test"
    expect c1 " 366 carol #test" "Davet edilen kullanıcı +i kanalına girebiliyor"

    session_send n1 "MODE #test -i"
    session_send n1 "MODE #test +k gizli"
    expect c1 "MODE #test +k gizli" "+k modu yayınlanıyor"

    session_send h1 "JOIN #test"
    expect h1 " 475 " "Anahtarsız giriş 475 ERR_BADCHANNELKEY veriyor"

    session_send h1 "JOIN #test gizli"
    expect h1 " 366 alice #test" "Doğru anahtarla giriş yapılabiliyor"

    session_send n1 "MODE #test -k"
    session_send n1 "MODE #test +l 3"
    expect h1 "MODE #test +l 3" "+l modu yayınlanıyor"

    irssi_handshake d1 dave
    session_send d1 "JOIN #test"
    expect d1 " 471 " "Limit dolu kanala giriş 471 ERR_CHANNELISFULL veriyor"

    session_send n1 "MODE #test -l"
    session_send d1 "JOIN #test"
    expect d1 " 366 dave #test" "Limit kaldırılınca giriş yapılabiliyor"

    session_send n1 "MODE #test +z"
    expect n1 " 472 " "Bilinmeyen mod 472 ERR_UNKNOWNMODE veriyor"
}

test_part_quit() {
    section "PART / QUIT"

    session_send d1 "PART #test :gorusuruz"
    expect d1 "PART #test :gorusuruz" "PART yankısı ayrılan kişiye dönüyor"
    expect h1 "dave!dave@" "PART bildirimi kanala ulaşıyor"

    session_send d1 "PART #yokboyle"
    expect d1 " 403 " "Olmayan kanaldan PART 403 veriyor"

    session_send c1 "QUIT :bb"
    expect c1 "ERROR :Closing link" "QUIT sonrası ERROR gönderiliyor"
    expect h1 "QUIT :Quit: bb" "QUIT bildirimi kanaldaki üyelere ulaşıyor"

    # Eski kodda cmdQuit içinde nesne silindiği için burada use-after-free oluşuyordu.
    kill -0 "$SERVER_PID" 2>/dev/null \
        && ok "QUIT sonrası sunucu ayakta (use-after-free yok)" \
        || nok "QUIT sonrası sunucu ayakta (use-after-free yok)" "sunucu çökmemeli"
}

test_errors() {
    section "Hata cevapları"

    session_send h1 "BILINMEYENKOMUT param"
    expect h1 " 421 " "Bilinmeyen komut 421 veriyor"

    session_open u1
    session_send u1 "JOIN #test"
    expect u1 " 451 " "Kayıtsız istemci 451 ERR_NOTREGISTERED alıyor"

    session_send h1 "JOIN"
    expect h1 " 461 " "Eksik parametre 461 veriyor"

    session_send h1 "KICK #test"
    expect h1 " 461 " "Eksik parametreli KICK 461 veriyor"

    session_send h1 "TOPIC #yokboyle :x"
    expect h1 " 403 " "Olmayan kanalda TOPIC 403 veriyor"

    session_send h1 "MODE #test +o hayalet"
    expect h1 " 401 " "Olmayan kullanıcıya +o 401 veriyor"
}

test_stability() {
    section "Kararlılık"

    # Ani kopma (istemci hiç QUIT göndermeden gider)
    session_open x1
    session_send x1 "CAP LS 302"
    session_send x1 "PASS $PASSWORD" "NICK ghost" "USER ghost 0 * :Ghost"
    session_send x1 "CAP END"
    wait_for x1 " 001 ghost" 30 >/dev/null
    session_send x1 "JOIN #test"
    sleep 0.3
    session_close x1
    sleep 0.5

    kill -0 "$SERVER_PID" 2>/dev/null \
        && ok "Ani bağlantı kopmasında sunucu ayakta" \
        || nok "Ani bağlantı kopmasında sunucu ayakta" "sunucu çökmemeli"

    # Aynı nick yeniden kullanılabilmeli
    irssi_handshake x2 ghost
    expect x2 " 001 ghost" "Kopan bağlantının nick'i yeniden kullanılabiliyor"

    # Uzun satır koruması
    session_open big
    local long
    long="$(printf 'A%.0s' $(seq 1 9000))"
    session_send_raw big "$long"
    sleep 0.5
    kill -0 "$SERVER_PID" 2>/dev/null \
        && ok "Aşırı uzun girdi sunucuyu düşürmüyor" \
        || nok "Aşırı uzun girdi sunucuyu düşürmüyor" "sunucu çökmemeli"
}

# ---------------------------------------------------------------------------
# Çalıştır
# ---------------------------------------------------------------------------

[ -n "$PORT" ] || PORT="$(find_free_port)"

echo "${BOLD}ft_irc test paketi${RESET}  (port $PORT, şifre '$PASSWORD')"
start_server

test_handshake
test_pipelined_registration
test_password
test_ping_pong
test_nick_rules
test_join
test_privmsg
test_who_mode_query
test_topic
test_mode_operator
test_mode_invite_key_limit
test_part_quit
test_errors
test_stability

echo
echo "${BOLD}────────────────────────────────────────${RESET}"
TOTAL=$(( PASS_COUNT + FAIL_COUNT ))
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "${GREEN}${BOLD}TÜM TESTLER GEÇTİ${RESET}  ($PASS_COUNT/$TOTAL)"
else
    echo "${RED}${BOLD}$FAIL_COUNT TEST BAŞARISIZ${RESET}  ($PASS_COUNT/$TOTAL geçti)"
    echo "$FAILED_NAMES" | tr '|' '\n' | sed "/^\$/d; s/^/  ${RED}✘${RESET} /"
fi

if [ "$VERBOSE" -eq 1 ]; then
    echo
    echo "${BOLD}Sunucu logu:${RESET}"
    sed 's/^/  /' "$SERVER_LOG"
fi

[ "$FAIL_COUNT" -eq 0 ]

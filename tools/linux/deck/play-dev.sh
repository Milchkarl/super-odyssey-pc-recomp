#!/bin/bash
# The Steam shortcut for testing on the Steam Deck (Gaming Mode or Desktop Mode): the newest quick build
# (tools/deck_dev.sh -> ~/sopr/build/linux/x86_64/AppDir) on the 1.4.1 TEST copy - never the real install.
# Add once in Desktop Mode: Steam > Games > Add a Non-Steam Game > Browse > /home/deck/sopr/tools/linux/deck/play-dev.sh
#
# Played from Steam: the owner's test save and settings, frame stats and thread CPU use logged to
#   /run/media/deck/SN01T/sopr-0.2.9.1/logs/dev-session-<time>.log (+ threads-<time>.log).
# Started by a scripted run from the PC (deck-bench.sh LAUNCH=steam): that run's settings are in
#   ~/sopr/bench/next-run.env (written just before; used once, and only if under 2 minutes old) and its output goes
#   to that run's folder.
APP=/home/deck/sopr/build/linux/x86_64/AppDir/AppRun
D=$(dirname "$(readlink -f "$0")")
N=/home/deck/sopr/bench/next-run.env
if [ -f "$N" ] && [ $(( $(date +%s) - $(stat -c %Y "$N") )) -lt 120 ]; then
    . "$N"
    mv "$N" "$OUT/run.env"
    ("$D/deck-threads.sh" "$OUT/threads.log" &) ; ("$D/deck-gpu.sh" "$OUT/gpu.log" &)
    timeout 900 "$APP" > "$OUT/run.log" 2>&1
    echo "EXIT=$?" >> "$OUT/run.log"
    touch "$OUT/game-exited"
    exit 0
fi
rm -f "$N"  # (a stale one: a scripted run that never started)

F=/run/media/deck/SN01T/sopr-fonttest
L=/run/media/deck/SN01T/sopr-0.2.9.1/logs
TS=$(date +%Y%m%d-%H%M%S)
mkdir -p "$L"
export ODYSSEY_DATA_ROOT=$F ODYSSEY_SAVE_DIR=$F/save ODYSSEY_SETTINGS=$F/powermoon-0291.ini ODYSSEY_NO_UPDATE_CHECK=1
export ODYSSEY_VK_PROF=1 ODYSSEY_LOG_SLOW_FRAMES=25 ODYSSEY_LOG_STAGES=1
("$D/deck-threads.sh" "$L/threads-$TS.log" &)
# (the log is kept by a detached helper once the game has exited: quitting from Steam ends this script with the game)
setsid bash -c 'sleep 20; while pgrep -f "AppDir/usr/bin/SuperOdysseyRecomp" > /dev/null; do sleep 2; done
    cp "$0" "$1"' "$F/SuperOdysseyRecomp.log" "$L/dev-session-$TS.log" < /dev/null > /dev/null 2>&1 &
"$APP" "$@"

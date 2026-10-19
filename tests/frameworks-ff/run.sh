#!/bin/zsh
# runs every framework test once at boot, results on the console between FWTEST markers
sleep 3
cd /tmp
echo "FWTEST BEGIN"
for t in cs as iokit media objc misc ffsyms; do
  /usr/local/libexec/fwtests/${t}_test
  echo "FWTEST $t exit=$?"
done
ls -l /dev/dsp0
/usr/local/libexec/fwtests/dsp_tone
echo "FWTEST dsp_tone exit=$?"
/usr/local/libexec/fwtests/audio_test 3
echo "FWTEST audio exit=$?"
echo "FWTEST END"

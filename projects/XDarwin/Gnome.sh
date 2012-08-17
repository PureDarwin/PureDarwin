 # launch dubs deamon
 launchctl load -w /Library/LaunchAgents/org.freedesktop.dbus-session.plist

 # make the freedesktop menu entries work
 export XDG_DATA_DIRS=/opt/local/share
 export XDG_DATA_HOME=/opt/local/share
 export XDG_CONFIG_DIRS=/opt/local/etc/xdg
 
 # enable sound
 export ESPEAKER=localhost

 # start X
 exec ./XDarwin :0 & sleep 1

 # use GNOME's window manager
 exec metacity -d :0 &
 
 # start GNOME
 exec gnome-session --display=:0

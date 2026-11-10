# /Applications/MiniBrowser.app, so LaunchServices hands http and https links and html files to WebKitGTK's MiniBrowser
# it is a bundle around the binary the webkitgtk package installs in /usr/libexec, which keeps a change here from rebuilding webkit
{ lib, runCommand }:

runCommand "puredarwin-minibrowser-app" { } ''
  app=$out/Applications/MiniBrowser.app/Contents
  mkdir -p $app/MacOS

  cat > $app/MacOS/MiniBrowser <<'SCRIPT'
#!/bin/sh
exec /usr/libexec/webkit2gtk-4.1/MiniBrowser "$@"
SCRIPT
  chmod 755 $app/MacOS/MiniBrowser

  cat > $app/Info.plist <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>MiniBrowser</string>
	<key>CFBundleIdentifier</key>
	<string>org.webkitgtk.MiniBrowser</string>
	<key>CFBundleName</key>
	<string>MiniBrowser</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>2.52.5</string>
	<key>CFBundleURLTypes</key>
	<array>
		<dict>
			<key>CFBundleURLName</key>
			<string>Web site URL</string>
			<key>CFBundleURLSchemes</key>
			<array>
				<string>http</string>
				<string>https</string>
			</array>
		</dict>
	</array>
	<key>CFBundleDocumentTypes</key>
	<array>
		<dict>
			<key>CFBundleTypeName</key>
			<string>HTML document</string>
			<key>CFBundleTypeRole</key>
			<string>Viewer</string>
			<key>CFBundleTypeExtensions</key>
			<array>
				<string>html</string>
				<string>htm</string>
			</array>
			<key>CFBundleTypeMIMETypes</key>
			<array>
				<string>text/html</string>
			</array>
		</dict>
	</array>
</dict>
</plist>
PLIST
''

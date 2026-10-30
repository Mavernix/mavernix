"""Builds an icon theme for the Sea Lion theme from the installer's own icons.

Run it after make_theme.py:

    python make_icons.py WORK OUTPUT/sea-lion

WORK is the folder holding the unpacked installer under root/. The icons
that have a freedesktop.org name, and every program's and System Preferences
pane's under its bundle identifier, are converted from .icns and written as
an icon theme into OUTPUT/share/icons/SeaLion/; other names fall back to
INHERITS.
"""

import argparse
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ICON_THEME_NAME = "SeaLion"

# Fallback themes, in order. Every application installs its icon into "hicolor".
INHERITS = ["breeze", "Adwaita", "hicolor"]

SYSTEM_ICONS = "System/Library/CoreServices/CoreTypes.bundle/Contents/Resources"

# Linux icon name -> (context, source). The context decides the sub-folder.
# The source is an .icns file in SYSTEM_ICONS, an .icns path from the top of
# the image (it has a "/"), or a program's or System Preferences pane's
# bundle, whose Info.plist names its icon.
ICONS = {
    "folder": ("places", "GenericFolderIcon.icns"),
    "folder-open": ("places", "OpenFolderIcon.icns"),
    "folder-applications": ("places", "ApplicationsFolderIcon.icns"),
    "folder-utilities": ("places", "UtilitiesFolder.icns"),
    "user-home": ("places", "HomeFolderIcon.icns"),
    "user-desktop": ("places", "DesktopFolderIcon.icns"),
    "folder-documents": ("places", "DocumentsFolderIcon.icns"),
    "folder-download": ("places", "DownloadsFolder.icns"),
    "folder-music": ("places", "MusicFolderIcon.icns"),
    "folder-pictures": ("places", "PicturesFolderIcon.icns"),
    "folder-videos": ("places", "MovieFolderIcon.icns"),
    "folder-publicshare": ("places", "PublicFolderIcon.icns"),
    "folder-saved-search": ("places", "SmartFolderIcon.icns"),
    "folder-remote": ("places", "GenericFileServerIcon.icns"),
    "network-workgroup": ("places", "GenericNetworkIcon.icns"),
    "network-server": ("places", "GenericFileServerIcon.icns"),
    "user-trash": ("places", "TrashIcon.icns"),
    "user-trash-full": ("places", "FullTrashIcon.icns"),

    # GTK programs ask for side-bar icons under "-symbolic" names.
    "user-home-symbolic": ("places", "SidebarHomeFolder.icns"),
    "user-desktop-symbolic": ("places", "SidebarDesktopFolder.icns"),
    "folder-symbolic": ("places", "SidebarGenericFolder.icns"),
    "folder-documents-symbolic": ("places", "SidebarDocumentsFolder.icns"),
    "folder-download-symbolic": ("places", "SidebarDownloadsFolder.icns"),
    "folder-downloads-symbolic": ("places", "SidebarDownloadsFolder.icns"),
    "folder-music-symbolic": ("places", "SidebarMusicFolder.icns"),
    "folder-pictures-symbolic": ("places", "SidebarPicturesFolder.icns"),
    "folder-videos-symbolic": ("places", "SidebarMoviesFolder.icns"),
    "folder-saved-search-symbolic": ("places", "SidebarSmartFolder.icns"),
    "network-workgroup-symbolic": ("places", "SidebarNetwork.icns"),
    "document-open-recent-symbolic": ("places", "SidebarRecents.icns"),
    "drive-harddisk-symbolic": ("devices", "SidebarInternalDisk.icns"),
    "drive-removable-media-symbolic": ("devices", "SidebarRemovableDisk.icns"),
    "media-optical-symbolic": ("devices", "SidebarOpticalDisk.icns"),
    "computer-symbolic": ("devices", "SidebarLaptop.icns"),

    "computer": ("devices", "com.apple.macbookpro-15-unibody.icns"),
    "video-display": ("devices", "com.apple.led-cinema-display-27.icns"),
    "media-optical": ("devices", "CDAudioVolumeIcon.icns"),
    "drive-removable-media": ("devices", "GenericAirDiskIcon.icns"),

    "text-x-generic": ("mimetypes", "GenericDocumentIcon.icns"),
    "application-octet-stream": ("mimetypes", "GenericDocumentIcon.icns"),
    "application-x-zerosize": ("mimetypes", "GenericDocumentIcon.icns"),
    # The installer has no general set of document icons; each program brings its own.
    "text-plain": ("mimetypes", "Applications/TextEdit.app/Contents/Resources/txt.icns"),
    "text-rtf": ("mimetypes", "Applications/TextEdit.app/Contents/Resources/rtf.icns"),
    "application-rtf": ("mimetypes", "Applications/TextEdit.app/Contents/Resources/rtf.icns"),
    "x-office-document": ("mimetypes", "Applications/TextEdit.app/Contents/Resources/rtf.icns"),
    "application-pdf": ("mimetypes", "Applications/Preview.app/Contents/Resources/pdf.icns"),
    "application-postscript": ("mimetypes", "Applications/Preview.app/Contents/Resources/ps.icns"),
    "image-x-generic": ("mimetypes", "Applications/Preview.app/Contents/Resources/jpeg.icns"),
    "image-jpeg": ("mimetypes", "Applications/Preview.app/Contents/Resources/jpeg.icns"),
    "image-png": ("mimetypes", "Applications/Preview.app/Contents/Resources/png.icns"),
    "image-gif": ("mimetypes", "Applications/Preview.app/Contents/Resources/gif.icns"),
    "image-tiff": ("mimetypes", "Applications/Preview.app/Contents/Resources/tiff.icns"),
    "image-bmp": ("mimetypes", "Applications/Preview.app/Contents/Resources/bmp.icns"),
    "image-x-icon": ("mimetypes", "Applications/Preview.app/Contents/Resources/ico.icns"),
    "image-vnd.adobe.photoshop": ("mimetypes", "Applications/Preview.app/Contents/Resources/psd.icns"),
    "video-x-generic": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/Movie.icns"),
    "video-mp4": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/MPEG-4.icns"),
    "video-mpeg": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/MPEG.icns"),
    "video-quicktime": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/QuickTime Movie.icns"),
    "video-x-msvideo": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/AVI.icns"),
    "audio-x-generic": ("mimetypes", "Applications/QuickTime Player.app/Contents/Resources/Audio.icns"),
    "audio-mpeg": ("mimetypes", "Applications/iTunes.app/Contents/Resources/iTunes-mp3.icns"),
    "audio-x-wav": ("mimetypes", "Applications/iTunes.app/Contents/Resources/iTunes-wav.icns"),
    "audio-x-aiff": ("mimetypes", "Applications/iTunes.app/Contents/Resources/iTunes-aiff.icns"),
    "audio-mp4": ("mimetypes", "Applications/iTunes.app/Contents/Resources/iTunes-mpeg4.icns"),
    "audio-x-mpegurl": ("mimetypes", "Applications/iTunes.app/Contents/Resources/iTunes-playlist.icns"),
    "package-x-generic": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah.icns"),
    "application-zip": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-zip.icns"),
    "application-x-tar": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-tar.icns"),
    "application-x-compressed-tar": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-tgz.icns"),
    "application-gzip": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-gz.icns"),
    "application-x-bzip": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-bz2.icns"),
    "application-x-bzip-compressed-tar": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-tbz2.icns"),
    "application-x-cpio": ("mimetypes", "System/Library/CoreServices/Archive Utility.app/Contents/Resources/bah-cpio.icns"),
    "font-ttf": ("mimetypes", "Applications/Font Book.app/Contents/Resources/ttf.icns"),
    "font-otf": ("mimetypes", "Applications/Font Book.app/Contents/Resources/otf.icns"),
    "font-collection": ("mimetypes", "Applications/Font Book.app/Contents/Resources/ttc.icns"),
    "text-calendar": ("mimetypes", "Applications/Calendar.app/Contents/Resources/ics.icns"),
    "text-x-log": ("mimetypes", "Applications/Utilities/Console.app/Contents/Resources/LogFile.icns"),
    "application-x-shellscript": ("mimetypes", "Applications/Utilities/Terminal.app/Contents/Resources/command_icon.icns"),
    "text-x-script": ("mimetypes", "Applications/Utilities/Terminal.app/Contents/Resources/command_icon.icns"),
    "application-x-raw-disk-image": ("mimetypes", "System/Library/CoreServices/DiskImageMounter.app/Contents/Resources/diskcopy-doc.icns"),
    "application-x-cd-image": ("mimetypes", "System/Library/CoreServices/DiskImageMounter.app/Contents/Resources/diskcopy-doc.icns"),
    "message-rfc822": ("mimetypes", "Applications/Mail.app/Contents/Resources/document.icns"),
    "application-x-executable": ("mimetypes", "ExecutableBinaryIcon.icns"),
    "font-x-generic": ("mimetypes", "GenericFontIcon.icns"),
    "text-html": ("mimetypes", "Applications/TextEdit.app/Contents/Resources/html.icns"),
    "text-vcard": ("mimetypes", "VCard.icns"),
    "unknown": ("mimetypes", "UnknownFSObjectIcon.icns"),

    "dialog-warning": ("status", "AlertCautionIcon.icns"),
    "dialog-information": ("status", "AlertNoteIcon.icns"),
    "dialog-error": ("status", "AlertStopIcon.icns"),
    "dialog-question": ("status", "GenericQuestionMarkIcon.icns"),

    "system-file-manager": ("apps", "System/Library/CoreServices/Finder.app"),
    "utilities-terminal": ("apps", "Applications/Utilities/Terminal.app"),
    "accessories-calculator": ("apps", "Applications/Calculator.app"),
    "accessories-text-editor": ("apps", "Applications/TextEdit.app"),
    "accessories-dictionary": ("apps", "Applications/Dictionary.app"),
    "preferences-system": ("apps", "Applications/System Preferences.app"),
    "preferences-desktop": ("apps", "Applications/System Preferences.app"),
    "preferences-dock": ("apps", "System/Library/PreferencePanes/Dock.prefPane/Contents/Resources/Dock.icns"),
    "preferences-desktop-theme": ("apps", "System/Library/PreferencePanes/Appearance.prefPane/Contents/Resources/General.icns"),
    "preferences-desktop-wallpaper": ("apps", "System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane/Contents/Resources/DesktopScreenEffectsPref.icns"),
    "preferences-desktop-sound": ("apps", "System/Library/PreferencePanes/Sound.prefPane/Contents/Resources/SoundPref.icns"),
    "input-touchpad": ("devices", "System/Library/PreferencePanes/Trackpad.prefPane/Contents/Resources/Trackpad.icns"),
    "preferences-desktop-display": ("apps", "System/Library/PreferencePanes/Displays.prefPane/Contents/Resources/Displays.icns"),
    "preferences-system-power-management": ("apps", "System/Library/PreferencePanes/EnergySaver.prefPane/Contents/Resources/EnergySaver.icns"),
    "preferences-desktop-virtual": ("apps", "System/Library/PreferencePanes/Expose.prefPane/Contents/Resources/MissionControlPrefPane.icns"),
    "preferences-system-bluetooth": ("apps", "System/Library/PreferencePanes/Bluetooth.prefPane/Contents/Resources/NewBluetoothLogo.icns"),
    "bluetooth": ("apps", "System/Library/PreferencePanes/Bluetooth.prefPane/Contents/Resources/NewBluetoothLogo.icns"),
    "web-browser": ("apps", "Applications/Safari.app"),
    "internet-mail": ("apps", "Applications/Mail.app"),
    "office-calendar": ("apps", "Applications/Calendar.app"),
    "multimedia-audio-player": ("apps", "Applications/iTunes.app"),
    "office-address-book": ("apps", "Applications/Contacts.app"),
    "utilities-system-monitor": ("apps", "Applications/Utilities/Activity Monitor.app"),
    "system-software-install": ("apps", "Applications/App Store.app"),
    "help-browser": ("apps", "HelpIcon.icns"),
    "system-help": ("apps", "HelpIcon.icns"),
    "applications-other": ("apps", "GenericApplicationIcon.icns"),
    "system-software-update": ("apps", "System/Library/CoreServices/Software Update.app"),
    "utilities-log-viewer": ("apps", "Applications/Utilities/Console.app"),
    "applets-screenshooter": ("apps", "Applications/Utilities/Grab.app"),
    "preferences-desktop-font": ("apps", "Applications/Font Book.app"),
    "preferences-desktop-font-installer": ("apps", "Applications/Font Book.app"),
    "preferences-desktop-color": ("apps", "Applications/Utilities/ColorSync Utility.app"),
    "preferences-desktop-notification": ("apps", "Notifications.icns"),

    # The other System Preferences panes, for the panes of Settings and of
    # other desktops' settings programs.
    "input-keyboard": ("devices", "System/Library/PreferencePanes/Keyboard.prefPane"),
    "preferences-desktop-keyboard": ("apps", "System/Library/PreferencePanes/Keyboard.prefPane"),
    "input-mouse": ("devices", "System/Library/PreferencePanes/Mouse.prefPane"),
    "preferences-system-network": ("apps", "System/Library/PreferencePanes/Network.prefPane"),
    "preferences-system-users": ("apps", "System/Library/PreferencePanes/Accounts.prefPane"),
    "system-users": ("apps", "System/Library/PreferencePanes/Accounts.prefPane"),
    # Login items are among a user's settings on OS X.
    "preferences-system-startup": ("apps", "System/Library/PreferencePanes/Accounts.prefPane"),
    "preferences-system-login": ("apps", "System/Library/PreferencePanes/Accounts.prefPane"),
    "preferences-system-time": ("apps", "System/Library/PreferencePanes/DateAndTime.prefPane"),
    "preferences-desktop-locale": ("apps", "System/Library/PreferencePanes/Localization.prefPane"),
    "preferences-desktop-accessibility": ("apps", "System/Library/PreferencePanes/UniversalAccessPref.prefPane"),
    "preferences-desktop-screensaver": ("apps", "System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane"),
    "preferences-system-privacy": ("apps", "System/Library/PreferencePanes/Security.prefPane"),
    "preferences-system-sharing": ("apps", "System/Library/PreferencePanes/SharingPref.prefPane"),
    "preferences-system-search": ("apps", "System/Library/PreferencePanes/Spotlight.prefPane"),
    "preferences-devices-printer": ("apps", "System/Library/PreferencePanes/PrintAndScan.prefPane"),
    "printer": ("devices", "System/Library/CoreServices/AddPrinter.app"),

    "phone": ("devices", "com.apple.iphone-4-black.icns"),
    "smartphone": ("devices", "com.apple.iphone-4-black.icns"),
    "multimedia-player": ("devices", "com.apple.ipod-touch-4-black.icns"),
    "avatar-default": ("status", "UserIcon.icns"),
    "user-identity": ("status", "UserIcon.icns"),
    "folder-development": ("places", "DeveloperFolderIcon.icns"),
    "inode-directory": ("mimetypes", "GenericFolderIcon.icns"),
    "text-x-generic-template": ("mimetypes", "GenericStationeryIcon.icns"),
    "text-x-vcard": ("mimetypes", "VCard.icns"),
    "application-vnd.iccprofile": ("mimetypes", "ColorSyncProfileIcon.icns"),
    "application-x-apple-diskimage": ("mimetypes", "System/Library/CoreServices/DiskImageMounter.app/Contents/Resources/diskcopy-doc.icns"),

    # Single applications, by the Icon= name in their desktop entry.
    "org.kde.dolphin": ("apps", "System/Library/CoreServices/Finder.app"),
    "system-file-manager-symbolic": ("apps", "System/Library/CoreServices/Finder.app"),
    "org.gnome.Nautilus": ("apps", "System/Library/CoreServices/Finder.app"),
    "foot": ("apps", "Applications/Utilities/Terminal.app"),
    "org.kde.konsole": ("apps", "Applications/Utilities/Terminal.app"),
    "featherpad": ("apps", "Applications/TextEdit.app"),
    "knotes": ("apps", "Applications/Notes.app"),
    "kamoso": ("apps", "Applications/Photo Booth.app"),
    "org.kde.kate": ("apps", "Applications/TextEdit.app"),
    "galculator": ("apps", "Applications/Calculator.app"),
    "org.kde.kcalc": ("apps", "Applications/Calculator.app"),
    "firefox": ("apps", "Applications/Safari.app"),
    "view-app-grid": ("apps", "Applications/Launchpad.app"),
    "org.kde.okular": ("apps", "Applications/Preview.app"),
    "org.kde.gwenview": ("apps", "Applications/Preview.app"),
    "nemo": ("apps", "System/Library/CoreServices/Finder.app"),
    "thunar": ("apps", "System/Library/CoreServices/Finder.app"),
    "org.xfce.thunar": ("apps", "System/Library/CoreServices/Finder.app"),
    "org.gnome.Console": ("apps", "Applications/Utilities/Terminal.app"),
    "org.gnome.Terminal": ("apps", "Applications/Utilities/Terminal.app"),
    "org.xfce.terminal": ("apps", "Applications/Utilities/Terminal.app"),
    "kitty": ("apps", "Applications/Utilities/Terminal.app"),
    "Alacritty": ("apps", "Applications/Utilities/Terminal.app"),
    "com.mitchellh.ghostty": ("apps", "Applications/Utilities/Terminal.app"),
    "kate": ("apps", "Applications/TextEdit.app"),
    "kwrite": ("apps", "Applications/TextEdit.app"),
    "org.gnome.TextEditor": ("apps", "Applications/TextEdit.app"),
    "org.gnome.gedit": ("apps", "Applications/TextEdit.app"),
    "org.xfce.mousepad": ("apps", "Applications/TextEdit.app"),
    "org.gnome.Calculator": ("apps", "Applications/Calculator.app"),
    "qalculate": ("apps", "Applications/Calculator.app"),
    "chromium": ("apps", "Applications/Safari.app"),
    "org.mozilla.firefox": ("apps", "Applications/Safari.app"),
    "org.gnome.Epiphany": ("apps", "Applications/Safari.app"),
    "org.kde.falkon": ("apps", "Applications/Safari.app"),
    "thunderbird": ("apps", "Applications/Mail.app"),
    "org.mozilla.Thunderbird": ("apps", "Applications/Mail.app"),
    "org.kde.kmail2": ("apps", "Applications/Mail.app"),
    "org.gnome.Evolution": ("apps", "Applications/Mail.app"),
    "org.gnome.Geary": ("apps", "Applications/Mail.app"),
    "org.gnome.Calendar": ("apps", "Applications/Calendar.app"),
    "korganizer": ("apps", "Applications/Calendar.app"),
    "org.gnome.Contacts": ("apps", "Applications/Contacts.app"),
    "kaddressbook": ("apps", "Applications/Contacts.app"),
    "elisa": ("apps", "Applications/iTunes.app"),
    "org.kde.elisa": ("apps", "Applications/iTunes.app"),
    "rhythmbox": ("apps", "Applications/iTunes.app"),
    "org.gnome.Rhythmbox3": ("apps", "Applications/iTunes.app"),
    "strawberry": ("apps", "Applications/iTunes.app"),
    "vlc": ("apps", "Applications/QuickTime Player.app"),
    "mpv": ("apps", "Applications/QuickTime Player.app"),
    "totem": ("apps", "Applications/QuickTime Player.app"),
    "org.gnome.Totem": ("apps", "Applications/QuickTime Player.app"),
    "org.kde.haruna": ("apps", "Applications/QuickTime Player.app"),
    "dragonplayer": ("apps", "Applications/QuickTime Player.app"),
    "okular": ("apps", "Applications/Preview.app"),
    "gwenview": ("apps", "Applications/Preview.app"),
    "org.gnome.Evince": ("apps", "Applications/Preview.app"),
    "org.gnome.Papers": ("apps", "Applications/Preview.app"),
    "org.gnome.Loupe": ("apps", "Applications/Preview.app"),
    "org.gnome.eog": ("apps", "Applications/Preview.app"),
    "org.gnome.Snapshot": ("apps", "Applications/Photo Booth.app"),
    "org.gnome.Cheese": ("apps", "Applications/Photo Booth.app"),
    "org.gnome.SystemMonitor": ("apps", "Applications/Utilities/Activity Monitor.app"),
    "systemsettings": ("apps", "Applications/System Preferences.app"),
    "org.gnome.Settings": ("apps", "Applications/System Preferences.app"),
    "plasmadiscover": ("apps", "Applications/App Store.app"),
    "org.gnome.Software": ("apps", "Applications/App Store.app"),
    "org.gnome.Dictionary": ("apps", "Applications/Dictionary.app"),
    "kfontview": ("apps", "Applications/Font Book.app"),
    "org.gnome.font-viewer": ("apps", "Applications/Font Book.app"),
    "gparted": ("apps", "Applications/Utilities/Disk Utility.app"),
    "partitionmanager": ("apps", "Applications/Utilities/Disk Utility.app"),
    "org.gnome.DiskUtility": ("apps", "Applications/Utilities/Disk Utility.app"),
    "kwalletmanager": ("apps", "Applications/Utilities/Keychain Access.app"),
    "org.gnome.seahorse.Application": ("apps", "Applications/Utilities/Keychain Access.app"),
    "spectacle": ("apps", "Applications/Utilities/Grab.app"),
    "org.gnome.Screenshot": ("apps", "Applications/Utilities/Grab.app"),
    "org.gnome.Logs": ("apps", "Applications/Utilities/Console.app"),
    "hwinfo": ("apps", "Applications/Utilities/System Information.app"),
    "ark": ("apps", "System/Library/CoreServices/Archive Utility.app"),
    "org.gnome.FileRoller": ("apps", "System/Library/CoreServices/Archive Utility.app"),
    "org.gnome.SimpleScan": ("apps", "Applications/Image Capture.app"),
    "org.kde.skanlite": ("apps", "Applications/Image Capture.app"),
    "org.gnome.DejaDup": ("apps", "Applications/Time Machine.app"),
    "timeshift": ("apps", "Applications/Time Machine.app"),
    "org.gnome.Chess": ("apps", "Applications/Chess.app"),
    "knights": ("apps", "Applications/Chess.app"),
    "org.gnome.Maps": ("apps", "Applications/Maps.app"),
    "marble": ("apps", "Applications/Maps.app"),
    "com.github.johnfactotum.Foliate": ("apps", "Applications/iBooks.app"),
    "org.gnome.Todo": ("apps", "Applications/Reminders.app"),
    "org.remmina.Remmina": ("apps", "System/Library/CoreServices/Screen Sharing.app"),
    "krdc": ("apps", "System/Library/CoreServices/Screen Sharing.app"),
    "kcolorchooser": ("apps", "Applications/Utilities/DigitalColor Meter.app"),
    "org.gnome.Yelp": ("apps", "System/Library/CoreServices/HelpViewer.app"),
}

# Every program and System Preferences pane that has an icon of its own,
# under its bundle identifier ("com.apple.Chess"), so that the Icons pane
# offers all of Mavericks' icons, also those no Linux program asks for.
BUNDLES = [
    "Applications/App Store.app",
    "Applications/Automator.app",
    "Applications/Calculator.app",
    "Applications/Calendar.app",
    "Applications/Chess.app",
    "Applications/Contacts.app",
    "Applications/DVD Player.app",
    "Applications/Dashboard.app",
    "Applications/Dictionary.app",
    "Applications/FaceTime.app",
    "Applications/Font Book.app",
    "Applications/Game Center.app",
    "Applications/Image Capture.app",
    "Applications/Launchpad.app",
    "Applications/Mail.app",
    "Applications/Maps.app",
    "Applications/Messages.app",
    "Applications/Mission Control.app",
    "Applications/Notes.app",
    "Applications/Photo Booth.app",
    "Applications/Preview.app",
    "Applications/QuickTime Player.app",
    "Applications/Reminders.app",
    "Applications/Safari.app",
    "Applications/Stickies.app",
    "Applications/System Preferences.app",
    "Applications/TextEdit.app",
    "Applications/Time Machine.app",
    "Applications/iBooks.app",
    "Applications/iTunes.app",
    "Applications/Utilities/Activity Monitor.app",
    "Applications/Utilities/AirPort Utility.app",
    "Applications/Utilities/AppleScript Editor.app",
    "Applications/Utilities/Audio MIDI Setup.app",
    "Applications/Utilities/Bluetooth File Exchange.app",
    "Applications/Utilities/Boot Camp Assistant.app",
    "Applications/Utilities/ColorSync Utility.app",
    "Applications/Utilities/Console.app",
    "Applications/Utilities/DigitalColor Meter.app",
    "Applications/Utilities/Disk Utility.app",
    "Applications/Utilities/Firmware Password Utility.app",
    "Applications/Utilities/Grab.app",
    "Applications/Utilities/Grapher.app",
    "Applications/Utilities/Keychain Access.app",
    "Applications/Utilities/Migration Assistant.app",
    "Applications/Utilities/Reset Password.app",
    "Applications/Utilities/System Information.app",
    "Applications/Utilities/Terminal.app",
    "Applications/Utilities/VoiceOver Utility.app",
    "System/Library/CoreServices/AddPrinter.app",
    "System/Library/CoreServices/AppleScript Utility.app",
    "System/Library/CoreServices/Applications/Feedback Assistant.app",
    "System/Library/CoreServices/Applications/Network Utility.app",
    "System/Library/CoreServices/Applications/RAID Utility.app",
    "System/Library/CoreServices/Applications/Wireless Diagnostics.app",
    "System/Library/CoreServices/Archive Utility.app",
    "System/Library/CoreServices/Bluetooth Setup Assistant.app",
    "System/Library/CoreServices/Certificate Assistant.app",
    "System/Library/CoreServices/Directory Utility.app",
    "System/Library/CoreServices/DiskImageMounter.app",
    "System/Library/CoreServices/Dock.app",
    "System/Library/CoreServices/Finder.app",
    "System/Library/CoreServices/Folder Actions Setup.app",
    "System/Library/CoreServices/HelpViewer.app",
    "System/Library/CoreServices/Installer.app",
    "System/Library/CoreServices/Network Diagnostics.app",
    "System/Library/CoreServices/Problem Reporter.app",
    "System/Library/CoreServices/Screen Sharing.app",
    "System/Library/CoreServices/Setup Assistant.app",
    "System/Library/CoreServices/Software Update.app",
    "System/Library/CoreServices/Spotlight.app",
    "System/Library/CoreServices/System Image Utility.app",
    "System/Library/CoreServices/Ticket Viewer.app",
    "System/Library/CoreServices/VoiceOver.app",
    "System/Library/PreferencePanes/Accounts.prefPane",
    "System/Library/PreferencePanes/Appearance.prefPane",
    "System/Library/PreferencePanes/Bluetooth.prefPane",
    "System/Library/PreferencePanes/DateAndTime.prefPane",
    "System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane",
    "System/Library/PreferencePanes/DigiHubDiscs.prefPane",
    "System/Library/PreferencePanes/Displays.prefPane",
    "System/Library/PreferencePanes/Dock.prefPane",
    "System/Library/PreferencePanes/EnergySaver.prefPane",
    "System/Library/PreferencePanes/Expose.prefPane",
    "System/Library/PreferencePanes/FibreChannel.prefPane",
    "System/Library/PreferencePanes/Ink.prefPane",
    "System/Library/PreferencePanes/Keyboard.prefPane",
    "System/Library/PreferencePanes/Localization.prefPane",
    "System/Library/PreferencePanes/Mouse.prefPane",
    "System/Library/PreferencePanes/Network.prefPane",
    "System/Library/PreferencePanes/ParentalControls.prefPane",
    "System/Library/PreferencePanes/PrintAndScan.prefPane",
    "System/Library/PreferencePanes/Profiles.prefPane",
    "System/Library/PreferencePanes/Security.prefPane",
    "System/Library/PreferencePanes/SharingPref.prefPane",
    "System/Library/PreferencePanes/Sound.prefPane",
    "System/Library/PreferencePanes/Speech.prefPane",
    "System/Library/PreferencePanes/Spotlight.prefPane",
    "System/Library/PreferencePanes/StartupDisk.prefPane",
    "System/Library/PreferencePanes/TimeMachine.prefPane",
    "System/Library/PreferencePanes/Trackpad.prefPane",
    "System/Library/PreferencePanes/UniversalAccessPref.prefPane",
    "System/Library/PreferencePanes/Xsan.prefPane",
    "System/Library/PreferencePanes/iCloudPref.prefPane",
]

# KDE programs ask for side-bar icons under ordinary names at 22 pixels, as
# KDE's own theme keeps its one-colour drawings; these go there only.
SIDE_BAR_ICONS = {
    "user-home": ("places", "SidebarHomeFolder.icns"),
    "user-desktop": ("places", "SidebarDesktopFolder.icns"),
    "folder": ("places", "SidebarGenericFolder.icns"),
    "folder-documents": ("places", "SidebarDocumentsFolder.icns"),
    "folder-download": ("places", "SidebarDownloadsFolder.icns"),
    "folder-downloads": ("places", "SidebarDownloadsFolder.icns"),
    "folder-music": ("places", "SidebarMusicFolder.icns"),
    "folder-pictures": ("places", "SidebarPicturesFolder.icns"),
    "folder-videos": ("places", "SidebarMoviesFolder.icns"),
    "folder-saved-search": ("places", "SidebarSmartFolder.icns"),
    "folder-network": ("places", "SidebarNetwork.icns"),
    "network-workgroup": ("places", "SidebarNetwork.icns"),
    "folder-root": ("places", "SidebarInternalDisk.icns"),
    "document-open-recent": ("places", "SidebarRecents.icns"),
    "view-calendar-day": ("places", "SidebarRecents.icns"),
    "drive-harddisk": ("devices", "SidebarInternalDisk.icns"),
    "drive-harddisk-root": ("devices", "SidebarInternalDisk.icns"),
    "drive-removable-media": ("devices", "SidebarRemovableDisk.icns"),
    "media-optical": ("devices", "SidebarOpticalDisk.icns"),
    "computer": ("devices", "SidebarLaptop.icns"),
}

# The 16-pixel drawing is centred, unscaled, in KDE's 22-pixel square.
SIDE_BAR_SIZE = 22
SIDE_BAR_DRAWING_SIZE = 16

# A side-bar .icns file holds a black drawing at the top level and, nested in
# an element of this type, the blue-grey engraved set Finder shows.
SIDE_BAR_SET_TYPE = b"sbpp"

# PNG element types and their size in pixels. libicns skips some of these
# (the 128-pixel one among others), so they are read here directly.
PNG_ELEMENT_SIZES = {
    b"icp4": 16, b"icp5": 32, b"icp6": 64,
    b"ic07": 128, b"ic08": 256, b"ic09": 512,
    # Double-density pictures, by their pixel size.
    b"ic11": 32, b"ic12": 64, b"ic13": 256, b"ic14": 512,
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

ICNS_HEADER_LENGTH = 8

SIZES = [16, 32, 48, 64, 128, 256, 512]

# Sizes from this one up are declared scalable.
SMALLEST_SCALABLE_SIZE = 128


def fail(message):
    sys.exit(f"make_icons: {message}")


def bundle_info(bundle):
    with open(bundle / "Contents" / "Info.plist", "rb") as file:
        return plistlib.load(file)


def icns_file_for(root, source):
    """The .icns file a source in ICONS stands for."""
    if source.endswith(".icns"):
        if "/" in source:
            return root / source
        return root / SYSTEM_ICONS / source

    bundle = root / source
    name = bundle_info(bundle).get("CFBundleIconFile")
    if name is None:
        fail(f"{bundle} names no icon in its Info.plist")
    if not name.endswith(".icns"):
        name += ".icns"
    return bundle / "Contents" / "Resources" / name


def icns_elements(icns_file):
    """The elements of an .icns file, as (four-letter type, content) pairs.

    After the 8-byte header, each element is a four-letter type, a big-endian
    32-bit length that counts these 8 bytes too, and the data.
    """
    data = icns_file.read_bytes()
    position = ICNS_HEADER_LENGTH
    while position + 8 <= len(data):
        element_type = data[position:position + 4]
        element_length = int.from_bytes(data[position + 4:position + 8], "big")
        if element_length < 8:
            break
        yield element_type, data[position + 8:position + element_length]
        position += element_length


def nested_set(icns_file, set_type, target):
    """Writes the icon set nested in one element as an .icns file of its own
    (the content only lacks the header)."""
    for element_type, content in icns_elements(icns_file):
        if element_type == set_type:
            length = ICNS_HEADER_LENGTH + len(content)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"icns" + length.to_bytes(4, "big") + content)
            return target
    fail(f"{icns_file} has no nested icon set of type {set_type.decode()}")


def png_elements(icns_file):
    """The pictures an .icns file stores as PNG; returns {size: PNG bytes}."""
    pictures = {}
    for element_type, content in icns_elements(icns_file):
        size = PNG_ELEMENT_SIZES.get(element_type)
        # JPEG 2000 pictures under the same types are left to icns2png.
        if size is not None and content.startswith(PNG_SIGNATURE):
            pictures.setdefault(size, content)
    return pictures


def extract_sizes(icns_file, work_directory):
    """Converts one .icns file; returns {size: path of the PNG}.

    PNG elements are copied out; icns2png converts the older packed kinds,
    naming each NAME_WIDTHxHEIGHTxDEPTH.png, of which the 32-bit ones are kept.
    """
    if work_directory.exists():
        shutil.rmtree(work_directory)
    work_directory.mkdir(parents=True)
    result = subprocess.run(
        ["icns2png", "-x", "-o", str(work_directory), str(icns_file)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        fail(f"icns2png could not read {icns_file}: {result.stderr.strip()}")

    by_size = {}
    for picture in work_directory.glob("*.png"):
        match = re.search(r"_(\d+)x(\d+)x32\.png$", picture.name)
        if match and match.group(1) == match.group(2):
            by_size[int(match.group(1))] = picture

    for size, content in png_elements(icns_file).items():
        picture = work_directory / f"direct-{size}.png"
        picture.write_bytes(content)
        by_size[size] = picture
    return by_size


def write_centred(picture, square_size, target):
    """Writes a picture into the middle of a transparent square."""
    drawing = Image.open(picture).convert("RGBA")
    square = Image.new("RGBA", (square_size, square_size), (0, 0, 0, 0))
    square.paste(drawing, ((square_size - drawing.width) // 2, (square_size - drawing.height) // 2))
    target.parent.mkdir(parents=True, exist_ok=True)
    square.save(target)


def folder_name(size, scale):
    """A size folder's name: "22x22", or "22x22@2" for double density."""
    if scale == 1:
        return f"{size}x{size}"
    return f"{size}x{size}@{scale}"


def index_text(contexts_by_size):
    """The theme's index file; contexts_by_size is {(size, scale): contexts}."""
    directories = []
    sections = []
    for size, scale in sorted(contexts_by_size):
        for context in sorted(contexts_by_size[(size, scale)]):
            directory = f"{folder_name(size, scale)}/{context}"
            directories.append(directory)
            context_name = {"apps": "Applications", "places": "Places", "devices": "Devices",
                            "mimetypes": "MimeTypes", "status": "Status"}[context]
            lines = [f"[{directory}]", f"Size={size}", f"Context={context_name}"]
            if scale != 1:
                lines += [f"Scale={scale}"]
            if size >= SMALLEST_SCALABLE_SIZE:
                lines += ["Type=Scalable", "MinSize=64", "MaxSize=1024"]
            else:
                lines += ["Type=Fixed"]
            sections.append("\n".join(lines))

    head = "\n".join([
        "[Icon Theme]",
        f"Name={ICON_THEME_NAME}",
        "Comment=Sea Lion's icons, made by make_icons.py",
        f"Inherits={','.join(INHERITS)}",
        f"Directories={','.join(directories)}",
    ])
    return head + "\n\n" + "\n\n".join(sections) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("work", type=Path, help="the work folder, holding the unpacked installer under root/")
    parser.add_argument("output", type=Path, help="theme directory, e.g. OUTPUT/sea-lion")
    arguments = parser.parse_args()

    root = arguments.work / "root"
    theme_directory = arguments.output / "share" / "icons" / ICON_THEME_NAME
    if theme_directory.exists():
        shutil.rmtree(theme_directory)

    contexts_by_size = {}  # (size, scale) -> the contexts that have a folder there
    converted = {}  # .icns path -> {size: PNG path}, so each file is converted once
    with tempfile.TemporaryDirectory() as scratch:

        def sizes_of(icon_name, source):
            icns_file = icns_file_for(root, source)
            if not icns_file.is_file():
                fail(f"{icns_file} is missing (wanted for the icon \"{icon_name}\")")
            if icns_file not in converted:
                converted[icns_file] = extract_sizes(icns_file, Path(scratch) / str(len(converted)))
            return converted[icns_file]

        def write_icon(icon_name, context, source):
            for size, picture in sizes_of(icon_name, source).items():
                if size not in SIZES:
                    continue
                target = theme_directory / folder_name(size, 1) / context / f"{icon_name}.png"
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(picture, target)
                contexts_by_size.setdefault((size, 1), set()).add(context)

        for icon_name, (context, source) in ICONS.items():
            write_icon(icon_name, context, source)
        for bundle in BUNDLES:
            write_icon(bundle_info(root / bundle)["CFBundleIdentifier"], "apps", bundle)

        for icon_name, (context, source) in SIDE_BAR_ICONS.items():
            whole_file = icns_file_for(root, source)
            if not whole_file.is_file():
                fail(f"{whole_file} is missing (wanted for the side-bar icon \"{icon_name}\")")
            side_bar_file = Path(scratch) / "side-bar" / source
            if side_bar_file not in converted:
                nested_set(whole_file, SIDE_BAR_SET_TYPE, side_bar_file)
                converted[side_bar_file] = extract_sizes(
                    side_bar_file, Path(scratch) / str(len(converted)))
            pictures = converted[side_bar_file]
            for scale in (1, 2):
                picture = pictures.get(SIDE_BAR_DRAWING_SIZE * scale)
                if picture is None:
                    continue
                target = (theme_directory / folder_name(SIDE_BAR_SIZE, scale) / context
                          / f"{icon_name}.png")
                write_centred(picture, SIDE_BAR_SIZE * scale, target)
                contexts_by_size.setdefault((SIDE_BAR_SIZE, scale), set()).add(context)

    (theme_directory / "index.theme").write_text(index_text(contexts_by_size))
    print(f"wrote {len(ICONS) + len(BUNDLES) + len(SIDE_BAR_ICONS)} icons from {len(converted)} of the installer's "
          f"icon files into {theme_directory}")


if __name__ == "__main__":
    main()

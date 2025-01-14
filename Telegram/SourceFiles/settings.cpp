/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2016 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "pspecific.h"
#include "settings.h"
#include "lang.h"

bool gRtl = false;
Qt::LayoutDirection gLangDir = gRtl ? Qt::RightToLeft : Qt::LeftToRight;

QString gArguments;

bool gDevVersion = DevVersion;
uint64 gBetaVersion = BETA_VERSION;
uint64 gRealBetaVersion = BETA_VERSION;
QByteArray gBetaPrivateKey;

bool gTestMode = false;
bool gDebug = false;
bool gManyInstance = false;
QString gKeyFile;
QString gWorkingDir, gExeDir, gExeName;

QStringList gSendPaths;
QString gStartUrl;

QString gLangErrors;

QString gDialogLastPath, gDialogHelperPath; // optimize QFileDialog

bool gSoundNotify = true;
bool gIncludeMuted = true;
bool gDesktopNotify = true;
DBINotifyView gNotifyView = dbinvShowPreview;
bool gWindowsNotifications = true;
bool gStartMinimized = false;
bool gStartInTray = false;
bool gAutoStart = false;
bool gSendToMenu = false;
bool gAutoUpdate = true;
TWindowPos gWindowPos;
LaunchMode gLaunchMode = LaunchModeNormal;
bool gSupportTray = true;
DBIWorkMode gWorkMode = dbiwmWindowAndTray;
DBIConnectionType gConnectionType = dbictAuto;
ConnectionProxy gConnectionProxy;
#ifdef Q_OS_WIN
bool gTryIPv6 = false;
#else
bool gTryIPv6 = true;
#endif
bool gSeenTrayTooltip = false;
bool gRestartingUpdate = false, gRestarting = false, gRestartingToSettings = false, gWriteProtected = false;
int32 gLastUpdateCheck = 0;
bool gNoStartUpdate = false;
bool gStartToSettings = false;
DBIDefaultAttach gDefaultAttach = dbidaDocument;
bool gReplaceEmojis = true;
bool gAskDownloadPath = false;
QString gDownloadPath;
QByteArray gDownloadPathBookmark;

bool gNeedConfigResave = false;

bool gCtrlEnter = false;

QPixmapPointer gChatBackground = 0;
int32 gChatBackgroundId = 0;
QPixmapPointer gChatDogImage = 0;
bool gTileBackground = false;

uint32 gConnectionsInSession = 1;
QString gLoggedPhoneNumber;

QByteArray gLocalSalt;
DBIScale gRealScale = dbisAuto, gScreenScale = dbisOne, gConfigScale = dbisAuto;
bool gCompressPastedImage = true;

QString gTimeFormat = qsl("hh:mm");

int32 gAutoLock = 3600;
bool gHasPasscode = false;

bool gHasAudioPlayer = true;
bool gHasAudioCapture = true;

RecentEmojiPack gRecentEmojis;
RecentEmojisPreload gRecentEmojisPreload;
EmojiColorVariants gEmojiVariants;

RecentStickerPreload gRecentStickersPreload;
RecentStickerPack gRecentStickers;

SavedGifs gSavedGifs;
uint64 gLastSavedGifsUpdate = 0;
bool gShowingSavedGifs = false;

RecentHashtagPack gRecentWriteHashtags, gRecentSearchHashtags;

RecentInlineBots gRecentInlineBots;

bool gPasswordRecovered = false;
int32 gPasscodeBadTries = 0;
uint64 gPasscodeLastTry = 0;

int32 gLang = -2; // auto
QString gLangFile;

bool gRetina = false;
float64 gRetinaFactor = 1.;
int32 gIntRetinaFactor = 1;
bool gCustomNotifies = true;

#ifdef Q_OS_WIN
DBIPlatform gPlatform = dbipWindows;
#elif defined Q_OS_MAC
DBIPlatform gPlatform = dbipMac;
#elif defined Q_OS_LINUX64
DBIPlatform gPlatform = dbipLinux64;
#elif defined Q_OS_LINUX32
DBIPlatform gPlatform = dbipLinux32;
#else
#error Unknown platform
#endif
QString gPlatformString;
QUrl gUpdateURL;
bool gIsElCapitan = false;

bool gContactsReceived = false;
bool gDialogsReceived = false;

int gOtherOnline = 0;

float64 gSongVolume = 0.9;

SavedPeers gSavedPeers;
SavedPeersByTime gSavedPeersByTime;

ReportSpamStatuses gReportSpamStatuses;

int32 gAutoDownloadPhoto = 0; // all auto download
int32 gAutoDownloadAudio = 0;
int32 gAutoDownloadGif = 0;
bool gAutoPlayGif = true;

void settingsParseArgs(int argc, char *argv[]) {
#ifdef Q_OS_MAC
	if (QSysInfo::macVersion() >= QSysInfo::MV_10_11) {
		gIsElCapitan = true;
	} else if (QSysInfo::macVersion() < QSysInfo::MV_10_8) {
		gPlatform = dbipMacOld;
	}
#endif

	switch (cPlatform()) {
	case dbipWindows:
		gUpdateURL = QUrl(qsl("http://tdesktop.com/win/tupdates/current"));
		gPlatformString = qsl("Windows");
	break;
	case dbipMac:
		gUpdateURL = QUrl(qsl("http://tdesktop.com/mac/tupdates/current"));
		gPlatformString = qsl("MacOS");
		gCustomNotifies = false;
	break;
	case dbipMacOld:
		gUpdateURL = QUrl(qsl("http://tdesktop.com/mac32/tupdates/current"));
		gPlatformString = qsl("MacOSold");
	break;
	case dbipLinux64:
		gUpdateURL = QUrl(qsl("http://tdesktop.com/linux/tupdates/current"));
		gPlatformString = qsl("Linux64bit");
	break;
	case dbipLinux32:
		gUpdateURL = QUrl(qsl("http://tdesktop.com/linux32/tupdates/current"));
		gPlatformString = qsl("Linux32bit");
	break;
	}

	QStringList args;
	for (int32 i = 0; i < argc; ++i) {
		args.push_back('"' + fromUtf8Safe(argv[i]) + '"');
	}
	gArguments = args.join(' ');

	gExeDir = psCurrentExeDirectory(argc, argv);
	gExeName = psCurrentExeName(argc, argv);
	if (argc == 2 && fromUtf8Safe(argv[1]).endsWith(qstr(".telegramcrash")) && QFile(fromUtf8Safe(argv[1])).exists()) {
		gLaunchMode = LaunchModeShowCrash;
		gStartUrl = fromUtf8Safe(argv[1]);
	}
    for (int32 i = 0; i < argc; ++i) {
		if (string("-testmode") == argv[i]) {
			gTestMode = true;
		} else if (string("-debug") == argv[i]) {
			gDebug = true;
		} else if (string("-many") == argv[i]) {
			gManyInstance = true;
		} else if (string("-key") == argv[i] && i + 1 < argc) {
			gKeyFile = fromUtf8Safe(argv[++i]);
		} else if (string("-autostart") == argv[i]) {
			gLaunchMode = LaunchModeAutoStart;
		} else if (string("-fixprevious") == argv[i]) {
			gLaunchMode = LaunchModeFixPrevious;
		} else if (string("-cleanup") == argv[i]) {
			gLaunchMode = LaunchModeCleanup;
		} else if (string("-crash") == argv[i] && i + 1 < argc) {
			gLaunchMode = LaunchModeShowCrash;
			gStartUrl = fromUtf8Safe(argv[++i]);
		} else if (string("-noupdate") == argv[i]) {
			gNoStartUpdate = true;
		} else if (string("-tosettings") == argv[i]) {
			gStartToSettings = true;
		} else if (string("-startintray") == argv[i]) {
			gStartInTray = true;
		} else if (string("-sendpath") == argv[i] && i + 1 < argc) {
			for (++i; i < argc; ++i) {
				gSendPaths.push_back(fromUtf8Safe(argv[i]));
			}
		} else if (string("-workdir") == argv[i] && i + 1 < argc) {
			QString dir = fromUtf8Safe(argv[++i]);
			if (QDir().exists(dir)) {
				gWorkingDir = dir;
			}
		} else if (string("--") == argv[i] && i + 1 < argc) {
			gStartUrl = fromUtf8Safe(argv[++i]);
		}
	}
}

RecentEmojiPack &cGetRecentEmojis() {
	if (cRecentEmojis().isEmpty()) {
		RecentEmojiPack r;
		if (!cRecentEmojisPreload().isEmpty()) {
			RecentEmojisPreload p(cRecentEmojisPreload());
			cSetRecentEmojisPreload(RecentEmojisPreload());
			r.reserve(p.size());
			for (RecentEmojisPreload::const_iterator i = p.cbegin(), e = p.cend(); i != e; ++i) {
				uint64 code = ((!(i->first & 0xFFFFFFFF00000000LLU) && (i->first & 0xFFFFU) == 0xFE0FU)) ? ((i->first >> 16) & 0xFFFFU) : i->first;
				EmojiPtr ep(emojiFromKey(code));
				if (!ep) continue;

				if (ep->postfix) {
					int32 j = 0, l = r.size();
					for (; j < l; ++j) {
						if (emojiKey(r[j].first) == code) {
							break;
						}
					}
					if (j < l) {
						continue;
					}
				}
				r.push_back(qMakePair(ep, i->second));
			}
		}
		uint64 defaultRecent[] = {
			0xD83DDE02LLU,
			0xD83DDE18LLU,
			0x2764LLU,
			0xD83DDE0DLLU,
			0xD83DDE0ALLU,
			0xD83DDE01LLU,
			0xD83DDC4DLLU,
			0x263ALLU,
			0xD83DDE14LLU,
			0xD83DDE04LLU,
			0xD83DDE2DLLU,
			0xD83DDC8BLLU,
			0xD83DDE12LLU,
			0xD83DDE33LLU,
			0xD83DDE1CLLU,
			0xD83DDE48LLU,
			0xD83DDE09LLU,
			0xD83DDE03LLU,
			0xD83DDE22LLU,
			0xD83DDE1DLLU,
			0xD83DDE31LLU,
			0xD83DDE21LLU,
			0xD83DDE0FLLU,
			0xD83DDE1ELLU,
			0xD83DDE05LLU,
			0xD83DDE1ALLU,
			0xD83DDE4ALLU,
			0xD83DDE0CLLU,
			0xD83DDE00LLU,
			0xD83DDE0BLLU,
			0xD83DDE06LLU,
			0xD83DDC4CLLU,
			0xD83DDE10LLU,
			0xD83DDE15LLU,
		};
		for (int32 i = 0, s = sizeof(defaultRecent) / sizeof(defaultRecent[0]); i < s; ++i) {
			if (r.size() >= EmojiPanPerRow * EmojiPanRowsPerPage) break;

			EmojiPtr ep(emojiGet(defaultRecent[i]));
			if (!ep || ep == TwoSymbolEmoji) continue;

			int32 j = 0, l = r.size();
			for (; j < l; ++j) {
				if (r[j].first == ep) {
					break;
				}
			}
			if (j < l) continue;

			r.push_back(qMakePair(ep, 1));
		}
		cSetRecentEmojis(r);
	}
	return cRefRecentEmojis();
}

RecentStickerPack &cGetRecentStickers() {
	if (cRecentStickers().isEmpty() && !cRecentStickersPreload().isEmpty()) {
		RecentStickerPreload p(cRecentStickersPreload());
		cSetRecentStickersPreload(RecentStickerPreload());

		RecentStickerPack &recent(cRefRecentStickers());
		recent.reserve(p.size());
		for (RecentStickerPreload::const_iterator i = p.cbegin(), e = p.cend(); i != e; ++i) {
			DocumentData *doc = App::document(i->first);
			if (!doc || !doc->sticker()) continue;

			recent.push_back(qMakePair(doc, i->second));
		}
	}
	return cRefRecentStickers();
}

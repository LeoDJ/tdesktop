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
#include "style.h"
#include "lang.h"

#include "application.h"
#include "window.h"
#include "mainwidget.h"
#include "apiwrap.h"

#include "localstorage.h"

ApiWrap::ApiWrap(QObject *parent) : QObject(parent)
, _messageDataResolveDelayed(new SingleDelayedCall(this, "resolveMessageDatas")) {
	App::initBackground();

	connect(&_webPagesTimer, SIGNAL(timeout()), this, SLOT(resolveWebPages()));
}

void ApiWrap::init() {
}

void ApiWrap::requestMessageData(ChannelData *channel, MsgId msgId, RequestMessageDataCallback *callback) {
	MessageDataRequest::CallbackPtr pcallback(callback);
	MessageDataRequest &req(channel ? _channelMessageDataRequests[channel][msgId] : _messageDataRequests[msgId]);
	req.callbacks.append(pcallback);
	if (!req.req) _messageDataResolveDelayed->call();
}

ApiWrap::MessageIds ApiWrap::collectMessageIds(const MessageDataRequests &requests) {
	MessageIds result;
	result.reserve(requests.size());
	for (MessageDataRequests::const_iterator i = requests.cbegin(), e = requests.cend(); i != e; ++i) {
		if (i.value().req > 0) continue;
		result.push_back(MTP_int(i.key()));
	}
	return result;
}

ApiWrap::MessageDataRequests *ApiWrap::messageDataRequests(ChannelData *channel, bool onlyExisting) {
	if (channel) {
		ChannelMessageDataRequests::iterator i = _channelMessageDataRequests.find(channel);
		if (i == _channelMessageDataRequests.cend()) {
			if (onlyExisting) return 0;
			i = _channelMessageDataRequests.insert(channel, MessageDataRequests());
		}
		return &i.value();
	}
	return &_messageDataRequests;
}

void ApiWrap::resolveMessageDatas() {
	if (_messageDataRequests.isEmpty() && _channelMessageDataRequests.isEmpty()) return;

	MessageIds ids = collectMessageIds(_messageDataRequests);
	if (!ids.isEmpty()) {
		mtpRequestId req = MTP::send(MTPmessages_GetMessages(MTP_vector<MTPint>(ids)), rpcDone(&ApiWrap::gotMessageDatas, (ChannelData*)nullptr), RPCFailHandlerPtr(), 0, 5);
		for (MessageDataRequests::iterator i = _messageDataRequests.begin(); i != _messageDataRequests.cend(); ++i) {
			if (i.value().req > 0) continue;
			i.value().req = req;
		}
	}
	for (ChannelMessageDataRequests::iterator j = _channelMessageDataRequests.begin(); j != _channelMessageDataRequests.cend();) {
		if (j->isEmpty()) {
			j = _channelMessageDataRequests.erase(j);
			continue;
		}
		MessageIds ids = collectMessageIds(j.value());
		if (!ids.isEmpty()) {
			mtpRequestId req = MTP::send(MTPchannels_GetMessages(j.key()->inputChannel, MTP_vector<MTPint>(ids)), rpcDone(&ApiWrap::gotMessageDatas, j.key()), RPCFailHandlerPtr(), 0, 5);
			for (MessageDataRequests::iterator i = j->begin(); i != j->cend(); ++i) {
				if (i.value().req > 0) continue;
				i.value().req = req;
			}
		}
		++j;
	}
}

void ApiWrap::gotMessageDatas(ChannelData *channel, const MTPmessages_Messages &msgs, mtpRequestId req) {
	switch (msgs.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(msgs.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages, NewMessageExisting);
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(msgs.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages, NewMessageExisting);
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(msgs.c_messages_channelMessages());
		if (channel) {
			channel->ptsReceived(d.vpts.v);
		} else {
			LOG(("App Error: received messages.channelMessages when no channel was passed! (ApiWrap::gotDependencyItem)"));
		}
		if (d.has_collapsed()) { // should not be returned
			LOG(("API Error: channels.getMessages and messages.getMessages should not return collapsed groups! (ApiWrap::gotDependencyItem)"));
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages, NewMessageExisting);
	} break;
	}
	MessageDataRequests *requests(messageDataRequests(channel, true));
	if (requests) {
		for (MessageDataRequests::iterator i = requests->begin(); i != requests->cend();) {
			if (i.value().req == req) {
				for (MessageDataRequest::Callbacks::const_iterator j = i.value().callbacks.cbegin(), e = i.value().callbacks.cend(); j != e; ++j) {
					(*j)->call(channel, i.key());
				}
				i = requests->erase(i);
			} else {
				++i;
			}
		}
		if (channel && requests->isEmpty()) {
			_channelMessageDataRequests.remove(channel);
		}
	}
}

void ApiWrap::requestFullPeer(PeerData *peer) {
	if (!peer || _fullPeerRequests.contains(peer)) return;

	mtpRequestId req = 0;
	if (peer->isUser()) {
		req = MTP::send(MTPusers_GetFullUser(peer->asUser()->inputUser), rpcDone(&ApiWrap::gotUserFull, peer), rpcFail(&ApiWrap::gotPeerFullFailed, peer));
	} else if (peer->isChat()) {
		req = MTP::send(MTPmessages_GetFullChat(peer->asChat()->inputChat), rpcDone(&ApiWrap::gotChatFull, peer), rpcFail(&ApiWrap::gotPeerFullFailed, peer));
	} else if (peer->isChannel()) {
		req = MTP::send(MTPchannels_GetFullChannel(peer->asChannel()->inputChannel), rpcDone(&ApiWrap::gotChatFull, peer), rpcFail(&ApiWrap::gotPeerFullFailed, peer));
	}
	if (req) _fullPeerRequests.insert(peer, req);
}

void ApiWrap::processFullPeer(PeerData *peer, const MTPmessages_ChatFull &result) {
	gotChatFull(peer, result, 0);
}

void ApiWrap::processFullPeer(PeerData *peer, const MTPUserFull &result) {
	gotUserFull(peer, result, 0);
}

void ApiWrap::gotChatFull(PeerData *peer, const MTPmessages_ChatFull &result, mtpRequestId req) {
	const MTPDmessages_chatFull &d(result.c_messages_chatFull());
	const QVector<MTPChat> &vc(d.vchats.c_vector().v);
	bool badVersion = false;
	if (peer->isChat()) {
		badVersion = (!vc.isEmpty() && vc.at(0).type() == mtpc_chat && vc.at(0).c_chat().vversion.v < peer->asChat()->version);
	} else if (peer->isChannel()) {
		badVersion = (!vc.isEmpty() && vc.at(0).type() == mtpc_channel && vc.at(0).c_channel().vversion.v < peer->asChannel()->version);
	}

	App::feedUsers(d.vusers, false);
	App::feedChats(d.vchats, false);

	if (peer->isChat()) {
		if (d.vfull_chat.type() != mtpc_chatFull) {
			LOG(("MTP Error: bad type in gotChatFull for chat: %1").arg(d.vfull_chat.type()));
			return;
		}
		const MTPDchatFull &f(d.vfull_chat.c_chatFull());
		App::feedParticipants(f.vparticipants, false, false);
		const QVector<MTPBotInfo> &v(f.vbot_info.c_vector().v);
		for (QVector<MTPBotInfo>::const_iterator i = v.cbegin(), e = v.cend(); i < e; ++i) {
			switch (i->type()) {
			case mtpc_botInfo: {
				const MTPDbotInfo &b(i->c_botInfo());
				UserData *user = App::userLoaded(b.vuser_id.v);
				if (user) {
					user->setBotInfo(*i);
					App::clearPeerUpdated(user);
					emit fullPeerUpdated(user);
				}
			} break;
			}
		}
		PhotoData *photo = App::feedPhoto(f.vchat_photo);
		ChatData *chat = peer->asChat();
		if (photo) {
			chat->photoId = photo->id;
			photo->peer = chat;
		} else {
			chat->photoId = 0;
		}
		chat->invitationUrl = (f.vexported_invite.type() == mtpc_chatInviteExported) ? qs(f.vexported_invite.c_chatInviteExported().vlink) : QString();

		App::main()->gotNotifySetting(MTP_inputNotifyPeer(peer->input), f.vnotify_settings);
	} else if (peer->isChannel()) {
		if (d.vfull_chat.type() != mtpc_channelFull) {
			LOG(("MTP Error: bad type in gotChatFull for channel: %1").arg(d.vfull_chat.type()));
			return;
		}
		const MTPDchannelFull &f(d.vfull_chat.c_channelFull());
		PhotoData *photo = App::feedPhoto(f.vchat_photo);
		ChannelData *channel = peer->asChannel();
		channel->flagsFull = f.vflags.v;
		if (photo) {
			channel->photoId = photo->id;
			photo->peer = channel;
		} else {
			channel->photoId = 0;
		}
		if (f.has_migrated_from_chat_id()) {
			if (!channel->mgInfo) {
				channel->flags |= MTPDchannel::Flag::f_megagroup;
				channel->flagsUpdated();
			}
			ChatData *cfrom = App::chat(peerFromChat(f.vmigrated_from_chat_id));
			bool updatedTo = (cfrom->migrateToPtr != channel), updatedFrom = (channel->mgInfo->migrateFromPtr != cfrom);
			if (updatedTo) {
				cfrom->migrateToPtr = channel;
			}
			if (updatedFrom) {
				channel->mgInfo->migrateFromPtr = cfrom;
				if (History *h = App::historyLoaded(cfrom->id)) {
					if (History *hto = App::historyLoaded(channel->id)) {
						if (!h->isEmpty()) {
							h->clear(true);
						}
						if (hto->inChatList() && h->inChatList()) {
							App::removeDialog(h);
						}
					}
				}
				Notify::migrateUpdated(channel);
			}
			if (updatedTo) {
				Notify::migrateUpdated(cfrom);
				App::main()->peerUpdated(cfrom);
			}
		}
		const QVector<MTPBotInfo> &v(f.vbot_info.c_vector().v);
		for (QVector<MTPBotInfo>::const_iterator i = v.cbegin(), e = v.cend(); i < e; ++i) {
			switch (i->type()) {
			case mtpc_botInfo: {
				const MTPDbotInfo &b(i->c_botInfo());
				UserData *user = App::userLoaded(b.vuser_id.v);
				if (user) {
					user->setBotInfo(*i);
					App::clearPeerUpdated(user);
					emit fullPeerUpdated(user);
				}
			} break;
			}
		}
		channel->about = qs(f.vabout);
		int32 newCount = f.has_participants_count() ? f.vparticipants_count.v : 0;
		if (newCount != channel->count) {
			if (channel->isMegagroup() && !channel->mgInfo->lastParticipants.isEmpty()) {
				channel->mgInfo->lastParticipantsStatus |= MegagroupInfo::LastParticipantsCountOutdated;
				channel->mgInfo->lastParticipantsCount = channel->count;
			}
			channel->count = newCount;
		}
		channel->adminsCount = f.has_admins_count() ? f.vadmins_count.v : 0;
		channel->invitationUrl = (f.vexported_invite.type() == mtpc_chatInviteExported) ? qs(f.vexported_invite.c_chatInviteExported().vlink) : QString();
		if (History *h = App::historyLoaded(channel->id)) {
			if (h->inboxReadBefore < f.vread_inbox_max_id.v + 1) {
				h->setUnreadCount(channel->isMegagroup() ? f.vunread_count.v : f.vunread_important_count.v);
				h->inboxReadBefore = f.vread_inbox_max_id.v + 1;
				h->asChannelHistory()->unreadCountAll = f.vunread_count.v;
			}
		}
		if (channel->isMegagroup()) {
			if (f.has_pinned_msg_id()) {
				channel->mgInfo->pinnedMsgId = f.vpinned_msg_id.v;
			} else {
				channel->mgInfo->pinnedMsgId = 0;
			}
		}
		channel->fullUpdated();

		App::main()->gotNotifySetting(MTP_inputNotifyPeer(peer->input), f.vnotify_settings);
	}

	if (req) {
		QMap<PeerData*, mtpRequestId>::iterator i = _fullPeerRequests.find(peer);
		if (i != _fullPeerRequests.cend() && i.value() == req) {
			_fullPeerRequests.erase(i);
		}
	}
	if (badVersion) {
		if (peer->isChat()) {
			peer->asChat()->version = vc.at(0).c_chat().vversion.v;
		} else if (peer->isChannel()) {
			peer->asChannel()->version = vc.at(0).c_channel().vversion.v;
		}
		requestPeer(peer);
	}
	App::clearPeerUpdated(peer);
	emit fullPeerUpdated(peer);
	App::emitPeerUpdated();
}

void ApiWrap::gotUserFull(PeerData *peer, const MTPUserFull &result, mtpRequestId req) {
	const MTPDuserFull &d(result.c_userFull());
	App::feedUsers(MTP_vector<MTPUser>(1, d.vuser), false);
	if (d.has_profile_photo()) {
		App::feedPhoto(d.vprofile_photo);
	}
	App::feedUserLink(MTP_int(peerToUser(peer->id)), d.vlink.c_contacts_link().vmy_link, d.vlink.c_contacts_link().vforeign_link, false);
	if (App::main()) {
		App::main()->gotNotifySetting(MTP_inputNotifyPeer(peer->input), d.vnotify_settings);
	}

	if (d.has_bot_info()) {
		peer->asUser()->setBotInfo(d.vbot_info);
	} else {
		peer->asUser()->setBotInfoVersion(-1);
	}
	peer->asUser()->blocked = d.is_blocked() ? UserIsBlocked : UserIsNotBlocked;
	peer->asUser()->about = d.has_about() ? qs(d.vabout) : QString();

	if (req) {
		QMap<PeerData*, mtpRequestId>::iterator i = _fullPeerRequests.find(peer);
		if (i != _fullPeerRequests.cend() && i.value() == req) {
			_fullPeerRequests.erase(i);
		}
	}
	App::clearPeerUpdated(peer);
	emit fullPeerUpdated(peer);
	App::emitPeerUpdated();
}

bool ApiWrap::gotPeerFullFailed(PeerData *peer, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	_fullPeerRequests.remove(peer);
	return true;
}

void ApiWrap::requestPeer(PeerData *peer) {
	if (!peer || _fullPeerRequests.contains(peer) || _peerRequests.contains(peer)) return;

	mtpRequestId req = 0;
	if (peer->isUser()) {
		req = MTP::send(MTPusers_GetUsers(MTP_vector<MTPInputUser>(1, peer->asUser()->inputUser)), rpcDone(&ApiWrap::gotUser, peer), rpcFail(&ApiWrap::gotPeerFailed, peer));
	} else if (peer->isChat()) {
		req = MTP::send(MTPmessages_GetChats(MTP_vector<MTPint>(1, peer->asChat()->inputChat)), rpcDone(&ApiWrap::gotChat, peer), rpcFail(&ApiWrap::gotPeerFailed, peer));
	} else if (peer->isChannel()) {
		req = MTP::send(MTPchannels_GetChannels(MTP_vector<MTPInputChannel>(1, peer->asChannel()->inputChannel)), rpcDone(&ApiWrap::gotChat, peer), rpcFail(&ApiWrap::gotPeerFailed, peer));
	}
	if (req) _peerRequests.insert(peer, req);
}

void ApiWrap::requestPeers(const QList<PeerData*> &peers) {
	QVector<MTPint> chats;
	QVector<MTPInputChannel> channels;
	QVector<MTPInputUser> users;
	chats.reserve(peers.size());
	channels.reserve(peers.size());
	users.reserve(peers.size());
	for (QList<PeerData*>::const_iterator i = peers.cbegin(), e = peers.cend(); i != e; ++i) {
		if (!*i || _fullPeerRequests.contains(*i) || _peerRequests.contains(*i)) continue;
		if ((*i)->isUser()) {
			users.push_back((*i)->asUser()->inputUser);
		} else if ((*i)->isChat()) {
			chats.push_back((*i)->asChat()->inputChat);
		} else if ((*i)->isChannel()) {
			channels.push_back((*i)->asChannel()->inputChannel);
		}
	}
	if (!chats.isEmpty()) MTP::send(MTPmessages_GetChats(MTP_vector<MTPint>(chats)), rpcDone(&ApiWrap::gotChats));
	if (!channels.isEmpty()) MTP::send(MTPchannels_GetChannels(MTP_vector<MTPInputChannel>(channels)), rpcDone(&ApiWrap::gotChats));
	if (!users.isEmpty()) MTP::send(MTPusers_GetUsers(MTP_vector<MTPInputUser>(users)), rpcDone(&ApiWrap::gotUsers));
}

void ApiWrap::requestLastParticipants(ChannelData *peer, bool fromStart) {
	if (!peer || !peer->isMegagroup()) return;
	bool needAdmins = peer->amEditor(), adminsOutdated = (peer->mgInfo->lastParticipantsStatus & MegagroupInfo::LastParticipantsAdminsOutdated);
	if ((needAdmins && adminsOutdated) || peer->lastParticipantsCountOutdated()) {
		fromStart = true;
	}
	QMap<PeerData*, mtpRequestId>::iterator i = _participantsRequests.find(peer);
	if (i != _participantsRequests.cend()) {
		if (fromStart && i.value() < 0) { // was not loading from start
			_participantsRequests.erase(i);
		} else {
			return;
		}
	}
	mtpRequestId req = MTP::send(MTPchannels_GetParticipants(peer->inputChannel, MTP_channelParticipantsRecent(), MTP_int(fromStart ? 0 : peer->mgInfo->lastParticipants.size()), MTP_int(Global::ChatSizeMax())), rpcDone(&ApiWrap::lastParticipantsDone, peer), rpcFail(&ApiWrap::lastParticipantsFail, peer));
	_participantsRequests.insert(peer, fromStart ? req : -req);
}

void ApiWrap::requestBots(ChannelData *peer) {
	if (!peer || !peer->isMegagroup() || _botsRequests.contains(peer)) return;
	_botsRequests.insert(peer, MTP::send(MTPchannels_GetParticipants(peer->inputChannel, MTP_channelParticipantsBots(), MTP_int(0), MTP_int(Global::ChatSizeMax())), rpcDone(&ApiWrap::lastParticipantsDone, peer), rpcFail(&ApiWrap::lastParticipantsFail, peer)));
}

void ApiWrap::gotChat(PeerData *peer, const MTPmessages_Chats &result) {
	_peerRequests.remove(peer);

	if (result.type() == mtpc_messages_chats) {
		const QVector<MTPChat> &v(result.c_messages_chats().vchats.c_vector().v);
		bool badVersion = false;
		if (peer->isChat()) {
			badVersion = (!v.isEmpty() && v.at(0).type() == mtpc_chat && v.at(0).c_chat().vversion.v < peer->asChat()->version);
		} else if (peer->isChannel()) {
			badVersion = (!v.isEmpty() && v.at(0).type() == mtpc_channel && v.at(0).c_chat().vversion.v < peer->asChannel()->version);
		}
		PeerData *chat = App::feedChats(result.c_messages_chats().vchats);
		if (chat == peer) {
			if (badVersion) {
				if (peer->isChat()) {
					peer->asChat()->version = v.at(0).c_chat().vversion.v;
				} else if (peer->isChannel()) {
					peer->asChannel()->version = v.at(0).c_channel().vversion.v;
				}
				requestPeer(peer);
			}
		}
	}
}

void ApiWrap::gotUser(PeerData *peer, const MTPVector<MTPUser> &result) {
	_peerRequests.remove(peer);

	UserData *user = App::feedUsers(result);
	if (user == peer) {
	}
}

void ApiWrap::gotChats(const MTPmessages_Chats &result) {
	App::feedChats(result.c_messages_chats().vchats);
}

void ApiWrap::gotUsers(const MTPVector<MTPUser> &result) {
	App::feedUsers(result);
}

bool ApiWrap::gotPeerFailed(PeerData *peer, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	_peerRequests.remove(peer);
	return true;
}

void ApiWrap::lastParticipantsDone(ChannelData *peer, const MTPchannels_ChannelParticipants &result, mtpRequestId req) {
	bool bots = (_botsRequests.value(peer) == req), fromStart = false;
	if (bots) {
		_botsRequests.remove(peer);
	} else {
		int32 was = _participantsRequests.value(peer);
		if (was == req) {
			fromStart = true;
		} else if (was != -req) {
			return;
		}
		_participantsRequests.remove(peer);
	}

	if (!peer->mgInfo || result.type() != mtpc_channels_channelParticipants) return;

	History *h = 0;
	if (bots) {
		h = App::historyLoaded(peer->id);
		peer->mgInfo->bots.clear();
		peer->mgInfo->botStatus = -1;
	} else if (fromStart) {
		peer->mgInfo->lastAdmins.clear();
		peer->mgInfo->lastParticipants.clear();
		peer->mgInfo->lastParticipantsStatus = MegagroupInfo::LastParticipantsUpToDate;
	}

	const MTPDchannels_channelParticipants &d(result.c_channels_channelParticipants());
	const QVector<MTPChannelParticipant> &v(d.vparticipants.c_vector().v);
	App::feedUsers(d.vusers);
	bool added = false, needBotsInfos = false;
	int32 botStatus = peer->mgInfo->botStatus;
	bool keyboardBotFound = !h || !h->lastKeyboardFrom;
	for (QVector<MTPChannelParticipant>::const_iterator i = v.cbegin(), e = v.cend(); i != e; ++i) {
		int32 userId = 0;
		bool admin = false;

		switch (i->type()) {
		case mtpc_channelParticipant: userId = i->c_channelParticipant().vuser_id.v; break;
		case mtpc_channelParticipantSelf: userId = i->c_channelParticipantSelf().vuser_id.v; break;
		case mtpc_channelParticipantModerator: userId = i->c_channelParticipantModerator().vuser_id.v; break;
		case mtpc_channelParticipantEditor: userId = i->c_channelParticipantEditor().vuser_id.v; admin = true; break;
		case mtpc_channelParticipantKicked: userId = i->c_channelParticipantKicked().vuser_id.v; break;
		case mtpc_channelParticipantCreator: userId = i->c_channelParticipantCreator().vuser_id.v; admin = true; break;
		}
		UserData *u = App::user(userId);
		if (bots) {
			if (u->botInfo) {
				peer->mgInfo->bots.insert(u);
				botStatus = 2;// (botStatus > 0/* || !i.key()->botInfo->readsAllHistory*/) ? 2 : 1;
				if (!u->botInfo->inited) {
					needBotsInfos = true;
				}
			}
			if (!keyboardBotFound && u->id == h->lastKeyboardFrom) {
				keyboardBotFound = true;
			}
		} else {
			if (peer->mgInfo->lastParticipants.indexOf(u) < 0) {
				peer->mgInfo->lastParticipants.push_back(u);
				if (admin) peer->mgInfo->lastAdmins.insert(u);
				if (u->botInfo) {
					peer->mgInfo->bots.insert(u);
					if (peer->mgInfo->botStatus != 0 && peer->mgInfo->botStatus < 2) {
						peer->mgInfo->botStatus = 2;
					}
				}
				added = true;
			}
		}
	}
	if (needBotsInfos) {
		requestFullPeer(peer);
	}
	if (!keyboardBotFound) {
		h->clearLastKeyboard();
		if (App::main()) App::main()->updateBotKeyboard(h);
	}
	if (d.vcount.v > peer->count) {
		peer->count = d.vcount.v;
	} else if (v.count() > peer->count) {
		peer->count = v.count();
	}
	if (!bots && v.isEmpty()) {
		peer->count = peer->mgInfo->lastParticipants.size();
	}
	peer->mgInfo->botStatus = botStatus;
	if (App::main()) emit fullPeerUpdated(peer);
}

bool ApiWrap::lastParticipantsFail(ChannelData *peer, const RPCError &error, mtpRequestId req) {
	if (mtpIsFlood(error)) return false;
	if (_participantsRequests.value(peer) == req || _participantsRequests.value(peer) == -req) {
		_participantsRequests.remove(peer);
	} else if (_botsRequests.value(peer) == req) {
		_botsRequests.remove(peer);
	}
	return true;
}

void ApiWrap::requestSelfParticipant(ChannelData *channel) {
	if (_selfParticipantRequests.contains(channel)) return;
	_selfParticipantRequests.insert(channel, MTP::send(MTPchannels_GetParticipant(channel->inputChannel, MTP_inputUserSelf()), rpcDone(&ApiWrap::gotSelfParticipant, channel), rpcFail(&ApiWrap::gotSelfParticipantFail, channel), 0, 5));
}

void ApiWrap::gotSelfParticipant(ChannelData *channel, const MTPchannels_ChannelParticipant &result) {
	_selfParticipantRequests.remove(channel);
	if (result.type() != mtpc_channels_channelParticipant) {
		LOG(("API Error: unknown type in gotSelfParticipant (%1)").arg(result.type()));
		channel->inviter = -1;
		if (App::main()) App::main()->onSelfParticipantUpdated(channel);
		return;
	}

	const MTPDchannels_channelParticipant &p(result.c_channels_channelParticipant());
	App::feedUsers(p.vusers);

	switch (p.vparticipant.type()) {
	case mtpc_channelParticipantSelf: {
		const MTPDchannelParticipantSelf &d(p.vparticipant.c_channelParticipantSelf());
		channel->inviter = d.vinviter_id.v;
		channel->inviteDate = date(d.vdate);
	} break;
	case mtpc_channelParticipantCreator: {
		const MTPDchannelParticipantCreator &d(p.vparticipant.c_channelParticipantCreator());
		channel->inviter = MTP::authedId();
		channel->inviteDate = date(MTP_int(channel->date));
	} break;
	case mtpc_channelParticipantModerator: {
		const MTPDchannelParticipantModerator &d(p.vparticipant.c_channelParticipantModerator());
		channel->inviter = d.vinviter_id.v;
		channel->inviteDate = date(d.vdate);
	} break;
	case mtpc_channelParticipantEditor: {
		const MTPDchannelParticipantEditor &d(p.vparticipant.c_channelParticipantEditor());
		channel->inviter = d.vinviter_id.v;
		channel->inviteDate = date(d.vdate);
	} break;

	}

	if (App::main()) App::main()->onSelfParticipantUpdated(channel);
}

bool ApiWrap::gotSelfParticipantFail(ChannelData *channel, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	if (error.type() == qstr("USER_NOT_PARTICIPANT")) {
		channel->inviter = -1;
	}
	_selfParticipantRequests.remove(channel);
	return true;
}

void ApiWrap::kickParticipant(PeerData *peer, UserData *user) {
	KickRequest req(peer, user);
	if (_kickRequests.contains(req)) return;

	if (peer->isChannel()) {
		_kickRequests.insert(req, MTP::send(MTPchannels_KickFromChannel(peer->asChannel()->inputChannel, user->inputUser, MTP_bool(true)), rpcDone(&ApiWrap::kickParticipantDone, req), rpcFail(&ApiWrap::kickParticipantFail, req)));
	}
}

void ApiWrap::kickParticipantDone(KickRequest kick, const MTPUpdates &result, mtpRequestId req) {
	_kickRequests.remove(kick);
	if (kick.first->isMegagroup()) {
		int32 i = kick.first->asChannel()->mgInfo->lastParticipants.indexOf(kick.second);
		if (i >= 0) {
			kick.first->asChannel()->mgInfo->lastParticipants.removeAt(i);
		}
		if (kick.first->asChannel()->count > 1) {
			--kick.first->asChannel()->count;
		} else {
			kick.first->asChannel()->mgInfo->lastParticipantsStatus |= MegagroupInfo::LastParticipantsCountOutdated;
			kick.first->asChannel()->mgInfo->lastParticipantsCount = 0;
		}
		if (kick.first->asChannel()->mgInfo->lastAdmins.contains(kick.second)) {
			kick.first->asChannel()->mgInfo->lastAdmins.remove(kick.second);
			if (kick.first->asChannel()->adminsCount > 1) {
				--kick.first->asChannel()->adminsCount;
			}
		}
		kick.first->asChannel()->mgInfo->bots.remove(kick.second);
		if (kick.first->asChannel()->mgInfo->bots.isEmpty() && kick.first->asChannel()->mgInfo->botStatus > 0) {
			kick.first->asChannel()->mgInfo->botStatus = -1;
		}
	}
	emit fullPeerUpdated(kick.first);
}

bool ApiWrap::kickParticipantFail(KickRequest kick, const RPCError &error, mtpRequestId req) {
	if (mtpIsFlood(error)) return false;
	_kickRequests.remove(kick);
	return true;
}

void ApiWrap::scheduleStickerSetRequest(uint64 setId, uint64 access) {
	if (!_stickerSetRequests.contains(setId)) {
		_stickerSetRequests.insert(setId, qMakePair(access, 0));
	}
}

void ApiWrap::requestStickerSets() {
	for (QMap<uint64, QPair<uint64, mtpRequestId> >::iterator i = _stickerSetRequests.begin(), j = i, e = _stickerSetRequests.end(); i != e; i = j) {
		++j;
		if (i.value().second) continue;

		int32 wait = (j == e) ? 0 : 10;
		i.value().second = MTP::send(MTPmessages_GetStickerSet(MTP_inputStickerSetID(MTP_long(i.key()), MTP_long(i.value().first))), rpcDone(&ApiWrap::gotStickerSet, i.key()), rpcFail(&ApiWrap::gotStickerSetFail, i.key()), 0, wait);
	}
}

void ApiWrap::gotStickerSet(uint64 setId, const MTPmessages_StickerSet &result) {
	_stickerSetRequests.remove(setId);

	if (result.type() != mtpc_messages_stickerSet) return;
	const MTPDmessages_stickerSet &d(result.c_messages_stickerSet());

	if (d.vset.type() != mtpc_stickerSet) return;
	const MTPDstickerSet &s(d.vset.c_stickerSet());

	Stickers::Sets &sets(Global::RefStickerSets());
	auto it = sets.find(setId);
	if (it == sets.cend()) return;

	it->access = s.vaccess_hash.v;
	it->hash = s.vhash.v;
	it->shortName = qs(s.vshort_name);
	it->title = stickerSetTitle(s);
	it->flags = s.vflags.v;

	const QVector<MTPDocument> &d_docs(d.vdocuments.c_vector().v);
	auto custom = sets.find(Stickers::CustomSetId);

	StickerPack pack;
	pack.reserve(d_docs.size());
	for (int32 i = 0, l = d_docs.size(); i != l; ++i) {
		DocumentData *doc = App::feedDocument(d_docs.at(i));
		if (!doc || !doc->sticker()) continue;

		pack.push_back(doc);
		if (custom != sets.cend()) {
			int32 index = custom->stickers.indexOf(doc);
			if (index >= 0) {
				custom->stickers.removeAt(index);
			}
		}
	}
	if (custom != sets.cend() && custom->stickers.isEmpty()) {
		sets.erase(custom);
		custom = sets.end();
	}

	bool writeRecent = false;
	RecentStickerPack &recent(cGetRecentStickers());
	for (RecentStickerPack::iterator i = recent.begin(); i != recent.cend();) {
		if (it->stickers.indexOf(i->first) >= 0 && pack.indexOf(i->first) < 0) {
			i = recent.erase(i);
			writeRecent = true;
		} else {
			++i;
		}
	}

	if (pack.isEmpty()) {
		int removeIndex = Global::StickerSetsOrder().indexOf(setId);
		if (removeIndex >= 0) Global::RefStickerSetsOrder().removeAt(removeIndex);
		sets.erase(it);
	} else {
		it->stickers = pack;
		it->emoji.clear();
		const QVector<MTPStickerPack> &v(d.vpacks.c_vector().v);
		for (int32 i = 0, l = v.size(); i < l; ++i) {
			if (v.at(i).type() != mtpc_stickerPack) continue;

			const MTPDstickerPack &pack(v.at(i).c_stickerPack());
			if (EmojiPtr e = emojiGetNoColor(emojiFromText(qs(pack.vemoticon)))) {
				const QVector<MTPlong> &stickers(pack.vdocuments.c_vector().v);
				StickerPack p;
				p.reserve(stickers.size());
				for (int32 j = 0, c = stickers.size(); j < c; ++j) {
					DocumentData *doc = App::document(stickers.at(j).v);
					if (!doc || !doc->sticker()) continue;

					p.push_back(doc);
				}
				it->emoji.insert(e, p);
			}
		}
	}

	if (writeRecent) {
		Local::writeUserSettings();
	}

	Local::writeStickers();

	if (App::main()) emit App::main()->stickersUpdated();
}

bool ApiWrap::gotStickerSetFail(uint64 setId, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	_stickerSetRequests.remove(setId);
	return true;
}

void ApiWrap::requestWebPageDelayed(WebPageData *page) {
	if (page->pendingTill <= 0) return;
	_webPagesPending.insert(page, 0);
	int32 left = (page->pendingTill - unixtime()) * 1000;
	if (!_webPagesTimer.isActive() || left <= _webPagesTimer.remainingTime()) {
		_webPagesTimer.start((left < 0 ? 0 : left) + 1);
	}
}

void ApiWrap::clearWebPageRequest(WebPageData *page) {
	_webPagesPending.remove(page);
	if (_webPagesPending.isEmpty() && _webPagesTimer.isActive()) _webPagesTimer.stop();
}

void ApiWrap::clearWebPageRequests() {
	_webPagesPending.clear();
	_webPagesTimer.stop();
}

void ApiWrap::resolveWebPages() {
	MessageIds ids; // temp_req_id = -1
	typedef QPair<int32, MessageIds> IndexAndMessageIds;
	typedef QMap<ChannelData*, IndexAndMessageIds> MessageIdsByChannel;
	MessageIdsByChannel idsByChannel; // temp_req_id = -index - 2

	const WebPageItems &items(App::webPageItems());
	ids.reserve(_webPagesPending.size());
	int32 t = unixtime(), m = INT_MAX;
	for (WebPagesPending::iterator i = _webPagesPending.begin(); i != _webPagesPending.cend(); ++i) {
		if (i.value() > 0) continue;
		if (i.key()->pendingTill <= t) {
			WebPageItems::const_iterator j = items.constFind(i.key());
			if (j != items.cend() && !j.value().isEmpty()) {
				for (HistoryItemsMap::const_iterator it = j.value().cbegin(); it != j.value().cend(); ++it) {
					HistoryItem *item = j.value().begin().key();
					if (item->id > 0) {
						if (item->channelId() == NoChannel) {
							ids.push_back(MTP_int(item->id));
							i.value() = -1;
						} else {
							ChannelData *channel = item->history()->peer->asChannel();
							MessageIdsByChannel::iterator channelMap = idsByChannel.find(channel);
							if (channelMap == idsByChannel.cend()) {
								channelMap = idsByChannel.insert(channel, IndexAndMessageIds(idsByChannel.size(), MessageIds(1, MTP_int(item->id))));
							} else {
								channelMap.value().second.push_back(MTP_int(item->id));
							}
							i.value() = -channelMap.value().first - 2;
						}
						break;
					}
				}
			}
		} else {
			m = qMin(m, i.key()->pendingTill - t);
		}
	}

	mtpRequestId req = ids.isEmpty() ? 0 : MTP::send(MTPmessages_GetMessages(MTP_vector<MTPint>(ids)), rpcDone(&ApiWrap::gotWebPages, (ChannelData*)0), RPCFailHandlerPtr(), 0, 5);
	typedef QVector<mtpRequestId> RequestIds;
	RequestIds reqsByIndex(idsByChannel.size(), 0);
	for (MessageIdsByChannel::const_iterator i = idsByChannel.cbegin(), e = idsByChannel.cend(); i != e; ++i) {
		reqsByIndex[i.value().first] = MTP::send(MTPchannels_GetMessages(i.key()->inputChannel, MTP_vector<MTPint>(i.value().second)), rpcDone(&ApiWrap::gotWebPages, i.key()), RPCFailHandlerPtr(), 0, 5);
	}
	if (req || !reqsByIndex.isEmpty()) {
		for (WebPagesPending::iterator i = _webPagesPending.begin(); i != _webPagesPending.cend(); ++i) {
			if (i.value() > 0) continue;
			if (i.value() < 0) {
				if (i.value() == -1) {
					i.value() = req;
				} else {
					i.value() = reqsByIndex[-i.value() - 2];
				}
			}
		}
	}

	if (m < INT_MAX) _webPagesTimer.start(m * 1000);
}

void ApiWrap::delayedRequestParticipantsCount() {
	if (App::main() && App::main()->peer() && App::main()->peer()->isChannel()) {
		requestFullPeer(App::main()->peer());
	}
}

void ApiWrap::gotWebPages(ChannelData *channel, const MTPmessages_Messages &msgs, mtpRequestId req) {
	const QVector<MTPMessage> *v = 0;
	switch (msgs.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(msgs.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(msgs.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(msgs.c_messages_channelMessages());
		if (channel) {
			channel->ptsReceived(d.vpts.v);
		} else {
			LOG(("API Error: received messages.channelMessages when no channel was passed! (ApiWrap::gotWebPages)"));
		}
		if (d.has_collapsed()) { // should not be returned
			LOG(("API Error: channels.getMessages and messages.getMessages should not return collapsed groups! (ApiWrap::gotWebPages)"));
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;
	}

	if (!v) return;
	QMap<uint64, int32> msgsIds; // copied from feedMsgs
	for (int32 i = 0, l = v->size(); i < l; ++i) {
		const MTPMessage &msg(v->at(i));
		switch (msg.type()) {
		case mtpc_message: msgsIds.insert((uint64(uint32(msg.c_message().vid.v)) << 32) | uint64(i), i); break;
		case mtpc_messageEmpty: msgsIds.insert((uint64(uint32(msg.c_messageEmpty().vid.v)) << 32) | uint64(i), i); break;
		case mtpc_messageService: msgsIds.insert((uint64(uint32(msg.c_messageService().vid.v)) << 32) | uint64(i), i); break;
		}
	}

	for (QMap<uint64, int32>::const_iterator i = msgsIds.cbegin(), e = msgsIds.cend(); i != e; ++i) {
		if (HistoryItem *item = App::histories().addNewMessage(v->at(i.value()), NewMessageExisting)) {
			item->setPendingInitDimensions();
		}
	}

	const WebPageItems &items(App::webPageItems());
	for (WebPagesPending::iterator i = _webPagesPending.begin(); i != _webPagesPending.cend();) {
		if (i.value() == req) {
			if (i.key()->pendingTill > 0) {
				i.key()->pendingTill = -1;
				WebPageItems::const_iterator j = items.constFind(i.key());
				if (j != items.cend()) {
					for (HistoryItemsMap::const_iterator k = j.value().cbegin(), e = j.value().cend(); k != e; ++k) {
						k.key()->setPendingInitDimensions();
					}
				}
			}
			i = _webPagesPending.erase(i);
		} else {
			++i;
		}
	}
}

ApiWrap::~ApiWrap() {
	App::clearHistories();
}

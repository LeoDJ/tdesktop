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
#pragma once

void historyInit();

class HistoryItem;

typedef QMap<int32, HistoryItem*> SelectedItemSet;

#include "structs.h"

enum NewMessageType {
	NewMessageUnread,
	NewMessageLast,
	NewMessageExisting,
};

class History;
class Histories {
public:
	typedef QHash<PeerId, History*> Map;
	Map map;

	Histories() : _a_typings(animation(this, &Histories::step_typings)), _unreadFull(0), _unreadMuted(0) {
	}

	void regSendAction(History *history, UserData *user, const MTPSendMessageAction &action);
	void step_typings(uint64 ms, bool timer);

	History *find(const PeerId &peerId);
	History *findOrInsert(const PeerId &peerId, int32 unreadCount, int32 maxInboxRead);

	void clear();
	void remove(const PeerId &peer);
	~Histories() {
		_unreadFull = _unreadMuted = 0;
	}

	HistoryItem *addNewMessage(const MTPMessage &msg, NewMessageType type);

	typedef QMap<History*, uint64> TypingHistories; // when typing in this history started
	TypingHistories typing;
	Animation _a_typings;

	int32 unreadBadge() const {
		return _unreadFull - (cIncludeMuted() ? 0 : _unreadMuted);
	}
	bool unreadOnlyMuted() const {
		return cIncludeMuted() ? (_unreadMuted >= _unreadFull) : false;
	}
	void unreadIncrement(int32 count, bool muted) {
		_unreadFull += count;
		if (muted) {
			_unreadMuted += count;
		}
	}
	void unreadMuteChanged(int32 count, bool muted) {
		if (muted) {
			_unreadMuted += count;
		} else {
			_unreadMuted -= count;
		}
	}

private:
	int32 _unreadFull, _unreadMuted;

};

class HistoryBlock;

struct DialogRow {
	DialogRow(History *history = 0, DialogRow *prev = 0, DialogRow *next = 0, int32 pos = 0) : prev(prev), next(next), history(history), pos(pos), attached(0) {
	}

	void paint(Painter &p, int32 w, bool act, bool sel, bool onlyBackground) const;

	DialogRow *prev, *next;
	History *history;
	int32 pos;
	void *attached; // for any attached data, for example View in contacts list
};

struct FakeDialogRow {
	FakeDialogRow(HistoryItem *item) : _item(item), _cacheFor(0), _cache(st::dlgRichMinWidth) {
	}

	void paint(Painter &p, int32 w, bool act, bool sel, bool onlyBackground) const;

	HistoryItem *_item;
	mutable const HistoryItem *_cacheFor;
	mutable Text _cache;
};

enum HistoryMediaType {
	MediaTypePhoto,
	MediaTypeVideo,
	MediaTypeContact,
	MediaTypeFile,
	MediaTypeGif,
	MediaTypeSticker,
	MediaTypeLocation,
	MediaTypeWebPage,
	MediaTypeMusicFile,
	MediaTypeVoiceFile,

	MediaTypeCount
};

enum MediaOverviewType {
	OverviewPhotos     = 0,
	OverviewVideos     = 1,
	OverviewMusicFiles = 2,
	OverviewFiles      = 3,
	OverviewVoiceFiles = 4,
	OverviewLinks      = 5,

	OverviewCount
};

inline MTPMessagesFilter typeToMediaFilter(MediaOverviewType &type) {
	switch (type) {
	case OverviewPhotos: return MTP_inputMessagesFilterPhotos();
	case OverviewVideos: return MTP_inputMessagesFilterVideo();
	case OverviewMusicFiles: return MTP_inputMessagesFilterMusic();
	case OverviewFiles: return MTP_inputMessagesFilterDocument();
	case OverviewVoiceFiles: return MTP_inputMessagesFilterVoice();
	case OverviewLinks: return MTP_inputMessagesFilterUrl();
	default: type = OverviewCount; break;
	}
	return MTPMessagesFilter();
}

enum SendActionType {
	SendActionTyping,
	SendActionRecordVideo,
	SendActionUploadVideo,
	SendActionRecordVoice,
	SendActionUploadVoice,
	SendActionUploadPhoto,
	SendActionUploadFile,
	SendActionChooseLocation,
	SendActionChooseContact,
};
struct SendAction {
	SendAction(SendActionType type, uint64 until, int32 progress = 0) : type(type), until(until), progress(progress) {
	}
	SendActionType type;
	uint64 until;
	int32 progress;
};

struct HistoryDraft {
	HistoryDraft() : msgId(0), previewCancelled(false) {
	}
	HistoryDraft(const QString &text, MsgId msgId, const MessageCursor &cursor, bool previewCancelled)
		: text(text)
		, msgId(msgId)
		, cursor(cursor)
		, previewCancelled(previewCancelled) {
	}
	HistoryDraft(const FlatTextarea &field, MsgId msgId, bool previewCancelled)
		: text(field.getLastText())
		, msgId(msgId)
		, cursor(field)
		, previewCancelled(previewCancelled) {
	}
	QString text;
	MsgId msgId; // replyToId for message draft, editMsgId for edit draft
	MessageCursor cursor;
	bool previewCancelled;
};
struct HistoryEditDraft : public HistoryDraft {
	HistoryEditDraft()
		: HistoryDraft()
		, saveRequest(0) {
	}
	HistoryEditDraft(const QString &text, MsgId msgId, const MessageCursor &cursor, bool previewCancelled, mtpRequestId saveRequest = 0)
		: HistoryDraft(text, msgId, cursor, previewCancelled)
		, saveRequest(saveRequest) {
	}
	HistoryEditDraft(const FlatTextarea &field, MsgId msgId, bool previewCancelled, mtpRequestId saveRequest = 0)
		: HistoryDraft(field, msgId, previewCancelled)
		, saveRequest(saveRequest) {
	}
	mtpRequestId saveRequest;
};

class HistoryMedia;
class HistoryMessage;

enum AddToOverviewMethod {
	AddToOverviewNew, // when new message is added to history
	AddToOverviewFront, // when old messages slice was received
	AddToOverviewBack, // when new messages slice was received and it is the last one, we index all media
};

struct DialogsIndexed;
class ChannelHistory;
class History {
public:

	History(const PeerId &peerId);
	History(const History &) = delete;
	History &operator=(const History &) = delete;

	ChannelId channelId() const {
		return peerToChannel(peer->id);
	}
	bool isChannel() const {
		return peerIsChannel(peer->id);
	}
	bool isMegagroup() const {
		return peer->isMegagroup();
	}
	ChannelHistory *asChannelHistory();
	const ChannelHistory *asChannelHistory() const;

	bool isEmpty() const {
		return blocks.isEmpty();
	}
	void clear(bool leaveItems = false);

	virtual ~History();

	HistoryItem *addNewService(MsgId msgId, QDateTime date, const QString &text, MTPDmessage::Flags flags = 0, HistoryMedia *media = 0, bool newMsg = true);
	HistoryItem *addNewMessage(const MTPMessage &msg, NewMessageType type);
	HistoryItem *addToHistory(const MTPMessage &msg);
	HistoryItem *addNewForwarded(MsgId id, MTPDmessage::Flags flags, QDateTime date, int32 from, HistoryMessage *item);
	HistoryItem *addNewDocument(MsgId id, MTPDmessage::Flags flags, int32 viaBotId, MsgId replyTo, QDateTime date, int32 from, DocumentData *doc, const QString &caption);
	HistoryItem *addNewPhoto(MsgId id, MTPDmessage::Flags flags, int32 viaBotId, MsgId replyTo, QDateTime date, int32 from, PhotoData *photo, const QString &caption);

	void addOlderSlice(const QVector<MTPMessage> &slice, const QVector<MTPMessageGroup> *collapsed);
	void addNewerSlice(const QVector<MTPMessage> &slice, const QVector<MTPMessageGroup> *collapsed);
	bool addToOverview(MediaOverviewType type, MsgId msgId, AddToOverviewMethod method);
	void eraseFromOverview(MediaOverviewType type, MsgId msgId);

	void newItemAdded(HistoryItem *item);
	void unregTyping(UserData *from);

	int countUnread(MsgId upTo);
	void updateShowFrom();
	MsgId inboxRead(MsgId upTo);
	MsgId inboxRead(HistoryItem *wasRead);
	MsgId outboxRead(MsgId upTo);
	MsgId outboxRead(HistoryItem *wasRead);

	HistoryItem *lastImportantMessage() const;

	void setUnreadCount(int newUnreadCount, bool psUpdate = true);
	void setMute(bool newMute);
	void getNextShowFrom(HistoryBlock *block, int i);
	void addUnreadBar();
	void destroyUnreadBar();
	void clearNotifications();

	bool loadedAtBottom() const; // last message is in the list
	void setNotLoadedAtBottom();
	bool loadedAtTop() const; // nothing was added after loading history back
	bool isReadyFor(MsgId msgId, MsgId &fixInScrollMsgId, int32 &fixInScrollMsgTop); // has messages for showing history at msgId
	void getReadyFor(MsgId msgId, MsgId &fixInScrollMsgId, int32 &fixInScrollMsgTop);

	void setLastMessage(HistoryItem *msg);
	void fixLastMessage(bool wasAtBottom);

	typedef QMap<QChar, DialogRow*> ChatListLinksMap;
	void setChatsListDate(const QDateTime &date);
	QPair<int32, int32> adjustByPosInChatsList(DialogsIndexed &indexed);
	uint64 sortKeyInChatList() const {
		return _sortKeyInChatList;
	}
	bool inChatList() const {
		return !_chatListLinks.isEmpty();
	}
	int32 posInChatList() const {
		return mainChatListLink()->pos;
	}
	DialogRow *addToChatList(DialogsIndexed &indexed);
	void removeFromChatList(DialogsIndexed &indexed);
	void removeChatListEntryByLetter(QChar letter);
	void addChatListEntryByLetter(QChar letter, DialogRow *row);
	void updateChatListEntry() const;

	MsgId minMsgId() const;
	MsgId maxMsgId() const;
	MsgId msgIdForRead() const;

	int resizeGetHeight(int newWidth);

	void removeNotification(HistoryItem *item) {
		if (!notifies.isEmpty()) {
			for (auto i = notifies.begin(), e = notifies.end(); i != e; ++i) {
				if ((*i) == item) {
					notifies.erase(i);
					break;
				}
			}
		}
	}
	HistoryItem *currentNotification() {
		return notifies.isEmpty() ? 0 : notifies.front();
	}
	bool hasNotification() const {
		return !notifies.isEmpty();
	}
	void skipNotification() {
		if (!notifies.isEmpty()) {
			notifies.pop_front();
		}
	}
	void popNotification(HistoryItem *item) {
		if (!notifies.isEmpty() && notifies.back() == item) notifies.pop_back();
	}

	bool hasPendingResizedItems() const {
		return _flags & Flag::f_has_pending_resized_items;
	}
	void setHasPendingResizedItems();
	void setPendingResize() {
		_flags |= Flag::f_pending_resize;
		setHasPendingResizedItems();
	}

	void paintDialog(Painter &p, int32 w, bool sel) const;
	bool updateTyping(uint64 ms, bool force = false);
	void clearLastKeyboard();

	// optimization for userpics displayed on the left
	// if this returns false there is no need to even try to handle them
	bool canHaveFromPhotos() const;

	typedef QList<HistoryBlock*> Blocks;
	Blocks blocks;

	int32 width, height, msgCount, unreadCount;
	int32 inboxReadBefore, outboxReadBefore;
	HistoryItem *showFrom;
	HistoryItem *unreadBar;

	PeerData *peer;
	bool oldLoaded, newLoaded;
	HistoryItem *lastMsg;
	QDateTime lastMsgDate;

	typedef QList<HistoryItem*> NotifyQueue;
	NotifyQueue notifies;

	HistoryDraft *msgDraft;
	HistoryEditDraft *editDraft;
	HistoryDraft *draft() {
		return editDraft ? editDraft : msgDraft;
	}
	void setMsgDraft(HistoryDraft *draft) {
		if (msgDraft) delete msgDraft;
		msgDraft = draft;
	}
	void setEditDraft(HistoryEditDraft *draft) {
		if (editDraft) delete editDraft;
		editDraft = draft;
	}

	// some fields below are a property of a currently displayed instance of this
	// conversation history not a property of the conversation history itself
public:
	// we save the last showAtMsgId to restore the state when switching
	// between different conversation histories
	MsgId showAtMsgId;

	// we save a pointer of the history item at the top of the displayed window
	// together with an offset from the window top to the top of this message
	// resulting scrollTop = top(scrollTopItem) + scrollTopOffset
	HistoryItem *scrollTopItem;
	int scrollTopOffset;
	void forgetScrollState() {
		scrollTopItem = nullptr;
	}

	// find the correct scrollTopItem and scrollTopOffset using given top
	// of the displayed window relative to the history start coord
	void countScrollState(int top);

protected:
	// when this item is destroyed scrollTopItem just points to the next one
	// and scrollTopOffset remains the same
	// if we are at the bottom of the window scrollTopItem == nullptr and
	// scrollTopOffset is undefined
	void getNextScrollTopItem(HistoryBlock *block, int32 i);

	// helper method for countScrollState(int top)
	void countScrollTopItem(int top);

public:

	bool mute;

	bool lastKeyboardInited, lastKeyboardUsed;
	MsgId lastKeyboardId, lastKeyboardHiddenId;
	PeerId lastKeyboardFrom;

	mtpRequestId sendRequestId;

	mutable const HistoryItem *textCachedFor; // cache
	mutable Text lastItemTextCache;

	typedef QMap<UserData*, uint64> TypingUsers;
	TypingUsers typing;
	typedef QMap<UserData*, SendAction> SendActionUsers;
	SendActionUsers sendActions;
	QString typingStr;
	Text typingText;
	uint32 typingDots;
	QMap<SendActionType, uint64> mySendActions;

	typedef QList<MsgId> MediaOverview;
	MediaOverview overview[OverviewCount];

	bool overviewCountLoaded(int32 overviewIndex) const {
		return overviewCountData[overviewIndex] >= 0;
	}
	bool overviewLoaded(int32 overviewIndex) const {
		return overviewCount(overviewIndex) == overview[overviewIndex].size();
	}
	int32 overviewCount(int32 overviewIndex, int32 defaultValue = -1) const {
		int32 result = overviewCountData[overviewIndex], loaded = overview[overviewIndex].size();
		if (result < 0) return defaultValue;
		if (result < loaded) {
			if (result > 0) {
				const_cast<History*>(this)->overviewCountData[overviewIndex] = 0;
			}
			return loaded;
		}
		return result;
	}
	MsgId overviewMinId(int32 overviewIndex) const {
		for (MediaOverviewIds::const_iterator i = overviewIds[overviewIndex].cbegin(), e = overviewIds[overviewIndex].cend(); i != e; ++i) {
			if (i.key() > 0) {
				return i.key();
			}
		}
		return 0;
	}
	void overviewSliceDone(int32 overviewIndex, const MTPmessages_Messages &result, bool onlyCounts = false);
	bool overviewHasMsgId(int32 overviewIndex, MsgId msgId) const {
		return overviewIds[overviewIndex].constFind(msgId) != overviewIds[overviewIndex].cend();
	}

	void changeMsgId(MsgId oldId, MsgId newId);

protected:

	void clearOnDestroy();
	HistoryItem *addNewToLastBlock(const MTPMessage &msg, NewMessageType type);

	friend class HistoryBlock;

	// this method just removes a block from the blocks list
	// when the last item from this block was detached and
	// calls the required previousItemChanged()
	void removeBlock(HistoryBlock *block);

	void clearBlocks(bool leaveItems);

	HistoryItem *createItem(const MTPMessage &msg, bool applyServiceAction, bool detachExistingItem);
	HistoryItem *createItemForwarded(MsgId id, MTPDmessage::Flags flags, QDateTime date, int32 from, HistoryMessage *msg);
	HistoryItem *createItemDocument(MsgId id, MTPDmessage::Flags flags, int32 viaBotId, MsgId replyTo, QDateTime date, int32 from, DocumentData *doc, const QString &caption);
	HistoryItem *createItemPhoto(MsgId id, MTPDmessage::Flags flags, int32 viaBotId, MsgId replyTo, QDateTime date, int32 from, PhotoData *photo, const QString &caption);

	HistoryItem *addNewItem(HistoryItem *adding, bool newMsg);
	HistoryItem *addNewInTheMiddle(HistoryItem *newItem, int32 blockIndex, int32 itemIndex);

	// All this methods add a new item to the first or last block
	// depending on if we are in isBuildingFronBlock() state.
	// The last block is created on the go if it is needed.

	// If the previous item is a message group the new group is
	// not created but is just united with the previous one.
	// create(HistoryItem *previous) should return a new HistoryGroup*
	// unite(HistoryGroup *existing) should unite a new group with an existing
	template <typename CreateGroup, typename UniteGroup>
	void addMessageGroup(CreateGroup create, UniteGroup unite);
	void addMessageGroup(const MTPDmessageGroup &group);

	// Adds the item to the back or front block, depending on
	// isBuildingFrontBlock(), creating the block if necessary.
	void addItemToBlock(HistoryItem *item);

	// Usually all new items are added to the last block.
	// Only when we scroll up and add a new slice to the
	// front we want to create a new front block.
	void startBuildingFrontBlock(int expectedItemsCount = 1);
	HistoryBlock *finishBuildingFrontBlock(); // Returns the built block or nullptr if nothing was added.
	bool isBuildingFrontBlock() const {
		return !_buildingFrontBlock.isNull();
	}

private:

	enum class Flag {
		f_has_pending_resized_items = (1 << 0),
		f_pending_resize            = (1 << 1),
	};
	Q_DECLARE_FLAGS(Flags, Flag);
	Q_DECL_CONSTEXPR friend inline QFlags<Flags::enum_type> operator|(Flags::enum_type f1, Flags::enum_type f2) noexcept {
		return QFlags<Flags::enum_type>(f1) | f2;
	}
	Q_DECL_CONSTEXPR friend inline QFlags<Flags::enum_type> operator|(Flags::enum_type f1, QFlags<Flags::enum_type> f2) noexcept {
		return f2 | f1;
	}
	Q_DECL_CONSTEXPR friend inline QFlags<Flags::enum_type> operator~(Flags::enum_type f) noexcept {
		return ~QFlags<Flags::enum_type>(f);
	}
	Flags _flags;

	ChatListLinksMap _chatListLinks;
	DialogRow *mainChatListLink() const {
		auto it = _chatListLinks.constFind(0);
		t_assert(it != _chatListLinks.cend());
		return it.value();
	}
	uint64 _sortKeyInChatList; // like ((unixtime) << 32) | (incremented counter)

	typedef QMap<MsgId, NullType> MediaOverviewIds;
	MediaOverviewIds overviewIds[OverviewCount];
	int32 overviewCountData[OverviewCount]; // -1 - not loaded, 0 - all loaded, > 0 - count, but not all loaded

	// A pointer to the block that is currently being built.
	// We hold this pointer so we can destroy it while building
	// and then create a new one if it is necessary.
	struct BuildingBlock {
		int expectedItemsCount = 0; // optimization for block->items.reserve() call
		HistoryBlock *block = nullptr;
	};
	UniquePointer<BuildingBlock> _buildingFrontBlock;

	// Creates if necessary a new block for adding item.
	// Depending on isBuildingFrontBlock() gets front or back block.
	HistoryBlock *prepareBlockForAddingItem();

 };

class HistoryGroup;
class HistoryCollapse;
class HistoryJoined;
class ChannelHistory : public History {
public:

	ChannelHistory(const PeerId &peer);

	void messageDetached(HistoryItem *msg);
	void messageDeleted(HistoryItem *msg);
	void messageWithIdDeleted(MsgId msgId);

	bool isSwitchReadyFor(MsgId switchId, MsgId &fixInScrollMsgId, int32 &fixInScrollMsgTop); // has messages for showing history after switching mode at switchId
	void getSwitchReadyFor(MsgId switchId, MsgId &fixInScrollMsgId, int32 &fixInScrollMsgTop);

	void insertCollapseItem(MsgId wasMinId);
	void getRangeDifference();
	void getRangeDifferenceNext(int32 pts);

	void addNewGroup(const MTPMessageGroup &group);

	int32 unreadCountAll;
	bool onlyImportant() const {
		return _onlyImportant;
	}

	HistoryCollapse *collapse() const {
		return _collapseMessage;
	}

	void clearOther() {
		_otherNewLoaded = true;
		_otherOldLoaded = false;
		_otherList.clear();
	}

	HistoryJoined *insertJoinedMessage(bool unread);
	void checkJoinedMessage(bool createUnread = false);
	const QDateTime &maxReadMessageDate();

	~ChannelHistory();

private:

	friend class History;
	HistoryItem* addNewChannelMessage(const MTPMessage &msg, NewMessageType type);
	HistoryItem *addNewToBlocks(const MTPMessage &msg, NewMessageType type);
	void addNewToOther(HistoryItem *item, NewMessageType type);

	void checkMaxReadMessageDate();

	HistoryGroup *findGroup(MsgId msgId) const;
	HistoryBlock *findGroupBlock(MsgId msgId) const;
	HistoryGroup *findGroupInOther(MsgId msgId) const;
	HistoryItem *findPrevItem(HistoryItem *item) const;
	void switchMode();

	void cleared();

	bool _onlyImportant;

	QDateTime _maxReadMessageDate;

	typedef QList<HistoryItem*> OtherList;
	OtherList _otherList;
	bool _otherOldLoaded, _otherNewLoaded;

	HistoryCollapse *_collapseMessage;
	HistoryJoined *_joinedMessage;

	MsgId _rangeDifferenceFromId, _rangeDifferenceToId;
	int32 _rangeDifferencePts;
	mtpRequestId _rangeDifferenceRequestId;

};

enum DialogsSortMode {
	DialogsSortByDate,
	DialogsSortByName,
	DialogsSortByAdd
};

struct DialogsList {
	DialogsList(DialogsSortMode sortMode) : begin(&last), end(&last), sortMode(sortMode), count(0), current(&last) {
	}

	void adjustCurrent(int32 y, int32 h) const {
		int32 pos = (y > 0) ? (y / h) : 0;
		while (current->pos > pos && current != begin) {
			current = current->prev;
		}
		while (current->pos + 1 <= pos && current->next != end) {
			current = current->next;
		}
	}

	void paint(Painter &p, int32 w, int32 hFrom, int32 hTo, PeerData *act, PeerData *sel, bool onlyBackground) const {
		adjustCurrent(hFrom, st::dlgHeight);

		DialogRow *drawFrom = current;
		p.translate(0, drawFrom->pos * st::dlgHeight);
		while (drawFrom != end && drawFrom->pos * st::dlgHeight < hTo) {
			bool active = (drawFrom->history->peer == act) || (drawFrom->history->peer->migrateTo() && drawFrom->history->peer->migrateTo() == act);
			bool selected = (drawFrom->history->peer == sel);
			drawFrom->paint(p, w, active, selected, onlyBackground);
			drawFrom = drawFrom->next;
			p.translate(0, st::dlgHeight);
		}
	}

	DialogRow *rowAtY(int32 y, int32 h) const {
		if (!count) return 0;

		int32 pos = (y > 0) ? (y / h) : 0;
		adjustCurrent(y, h);
		return (pos == current->pos) ? current : 0;
	}

	DialogRow *addToEnd(History *history) {
		DialogRow *result = new DialogRow(history, end->prev, end, end->pos);
		end->pos++;
		if (begin == end) {
			begin = current = result;
		} else {
			end->prev->next = result;
		}
		rowByPeer.insert(history->peer->id, result);
		++count;
		end->prev = result;
		if (sortMode == DialogsSortByDate) {
			adjustByPos(result);
		}
		return result;
	}

	bool insertBefore(DialogRow *row, DialogRow *before) {
		if (row == before) return false;

		if (current == row) current = row->prev;

		DialogRow *updateTill = row->prev;
		remove(row);

		// insert row
		row->next = before; // update row
		row->prev = before->prev;
		row->next->prev = row; // update row->next
		if (row->prev) { // update row->prev
			row->prev->next = row;
		} else {
			begin = row;
		}

		// update y
		for (DialogRow *n = row; n != updateTill; n = n->next) {
			n->next->pos++;
			row->pos--;
		}
		return true;
	}

	bool insertAfter(DialogRow *row, DialogRow *after) {
		if (row == after) return false;

		if (current == row) current = row->next;

		DialogRow *updateFrom = row->next;
		remove(row);

		// insert row
		row->prev = after; // update row
		row->next = after->next;
		row->prev->next = row; // update row->prev
		row->next->prev = row; // update row->next

		// update y
		for (DialogRow *n = updateFrom; n != row; n = n->next) {
			n->pos--;
			row->pos++;
		}
		return true;
	}

	DialogRow *adjustByName(const PeerData *peer) {
		if (sortMode != DialogsSortByName) return 0;

		RowByPeer::iterator i = rowByPeer.find(peer->id);
		if (i == rowByPeer.cend()) return 0;

		DialogRow *row = i.value(), *change = row;
		while (change->prev && change->prev->history->peer->name > peer->name) {
			change = change->prev;
		}
		if (!insertBefore(row, change)) {
			while (change->next != end && change->next->history->peer->name < peer->name) {
				change = change->next;
			}
			insertAfter(row, change);
		}
		return row;
	}

	DialogRow *addByName(History *history) {
		if (sortMode != DialogsSortByName) return 0;

		DialogRow *row = addToEnd(history), *change = row;
		const QString &peerName(history->peer->name);
		while (change->prev && change->prev->history->peer->name.compare(peerName, Qt::CaseInsensitive) > 0) {
			change = change->prev;
		}
		if (!insertBefore(row, change)) {
			while (change->next != end && change->next->history->peer->name.compare(peerName, Qt::CaseInsensitive) < 0) {
				change = change->next;
			}
			insertAfter(row, change);
		}
		return row;
	}

	void adjustByPos(DialogRow *row) {
		if (sortMode != DialogsSortByDate) return;

		DialogRow *change = row;
		if (change != begin && begin->history->sortKeyInChatList() < row->history->sortKeyInChatList()) {
			change = begin;
		} else while (change->prev && change->prev->history->sortKeyInChatList() < row->history->sortKeyInChatList()) {
			change = change->prev;
		}
		if (!insertBefore(row, change)) {
			if (change->next != end && end->prev->history->sortKeyInChatList() > row->history->sortKeyInChatList()) {
				change = end->prev;
			} else while (change->next != end && change->next->history->sortKeyInChatList() > row->history->sortKeyInChatList()) {
				change = change->next;
			}
			insertAfter(row, change);
		}
	}

	bool del(const PeerId &peerId, DialogRow *replacedBy = 0);

	void remove(DialogRow *row) {
		row->next->prev = row->prev; // update row->next
		if (row->prev) { // update row->prev
			row->prev->next = row->next;
		} else {
			begin = row->next;
		}
	}

	void clear() {
		while (begin != end) {
			current = begin;
			begin = begin->next;
			delete current;
		}
		current = begin;
		rowByPeer.clear();
		count = 0;
	}

	~DialogsList() {
		clear();
	}

	DialogRow last;
	DialogRow *begin, *end;
	DialogsSortMode sortMode;
	int32 count;

	typedef QHash<PeerId, DialogRow*> RowByPeer;
	RowByPeer rowByPeer;

	mutable DialogRow *current; // cache
};

struct DialogsIndexed {
	DialogsIndexed(DialogsSortMode sortMode) : sortMode(sortMode), list(sortMode) {
	}

	History::ChatListLinksMap addToEnd(History *history) {
		History::ChatListLinksMap result;
		DialogsList::RowByPeer::const_iterator i = list.rowByPeer.find(history->peer->id);
		if (i == list.rowByPeer.cend()) {
			result.insert(0, list.addToEnd(history));
			for (PeerData::NameFirstChars::const_iterator i = history->peer->chars.cbegin(), e = history->peer->chars.cend(); i != e; ++i) {
				DialogsIndex::iterator j = index.find(*i);
				if (j == index.cend()) {
					j = index.insert(*i, new DialogsList(sortMode));
				}
				result.insert(*i, j.value()->addToEnd(history));
			}
		}
		return result;
	}

	DialogRow *addByName(History *history) {
		DialogsList::RowByPeer::const_iterator i = list.rowByPeer.constFind(history->peer->id);
		if (i != list.rowByPeer.cend()) {
			return i.value();
		}

		DialogRow *res = list.addByName(history);
		for (PeerData::NameFirstChars::const_iterator i = history->peer->chars.cbegin(), e = history->peer->chars.cend(); i != e; ++i) {
			DialogsIndex::iterator j = index.find(*i);
			if (j == index.cend()) {
				j = index.insert(*i, new DialogsList(sortMode));
			}
			j.value()->addByName(history);
		}
		return res;
	}

	void adjustByPos(const History::ChatListLinksMap &links) {
		for (History::ChatListLinksMap::const_iterator i = links.cbegin(), e = links.cend(); i != e; ++i) {
			if (i.key() == QChar(0)) {
				list.adjustByPos(i.value());
			} else {
				DialogsIndex::iterator j = index.find(i.key());
				if (j != index.cend()) {
					j.value()->adjustByPos(i.value());
				}
			}
		}
	}

	void peerNameChanged(PeerData *peer, const PeerData::Names &oldNames, const PeerData::NameFirstChars &oldChars);

	void del(const PeerData *peer, DialogRow *replacedBy = 0) {
		if (list.del(peer->id, replacedBy)) {
			for (PeerData::NameFirstChars::const_iterator i = peer->chars.cbegin(), e = peer->chars.cend(); i != e; ++i) {
				DialogsIndex::iterator j = index.find(*i);
				if (j != index.cend()) {
					j.value()->del(peer->id, replacedBy);
				}
			}
		}
	}

	~DialogsIndexed() {
		clear();
	}

	void clear();

	DialogsSortMode sortMode;
	DialogsList list;
	typedef QMap<QChar, DialogsList*> DialogsIndex;
	DialogsIndex index;
};

class HistoryBlock {
public:
	HistoryBlock(History *hist) : y(0), height(0), history(hist), _indexInHistory(-1) {
	}

	HistoryBlock(const HistoryBlock &) = delete;
	HistoryBlock &operator=(const HistoryBlock &) = delete;

	typedef QVector<HistoryItem*> Items;
	Items items;

	void clear(bool leaveItems = false);
	~HistoryBlock() {
		clear();
	}
	void removeItem(HistoryItem *item);

	int resizeGetHeight(int newWidth, bool resizeAllItems);
	int32 y, height;
	History *history;

	HistoryBlock *previous() const {
		t_assert(_indexInHistory >= 0);

		return (_indexInHistory > 0) ? history->blocks.at(_indexInHistory - 1) : nullptr;
	}
	void setIndexInHistory(int index) {
		_indexInHistory = index;
	}
	int indexInHistory() const {
		t_assert(_indexInHistory >= 0);
		t_assert(history->blocks.at(_indexInHistory) == this);

		return _indexInHistory;
	}

protected:

	int _indexInHistory;

};

class HistoryElem {
public:

	HistoryElem() : _maxw(0), _minh(0), _height(0) {
	}

	int32 maxWidth() const {
		return _maxw;
	}
	int32 minHeight() const {
		return _minh;
	}
	int32 height() const {
		return _height;
	}

	virtual ~HistoryElem() {
	}

protected:

	mutable int32 _maxw, _minh, _height;
	HistoryElem &operator=(const HistoryElem &);

};

class HistoryMessage; // dynamic_cast optimize

enum HistoryCursorState {
	HistoryDefaultCursorState,
	HistoryInTextCursorState,
	HistoryInDateCursorState,
	HistoryInForwardedCursorState,
};

enum InfoDisplayType {
	InfoDisplayDefault,
	InfoDisplayOverImage,
	InfoDisplayOverBackground,
};

inline bool isImportantChannelMessage(MsgId id, MTPDmessage::Flags flags) { // client-side important msgs always has_views or has_from_id
	return (flags & MTPDmessage::Flag::f_out) || (flags & MTPDmessage::Flag::f_mentioned) || (flags & MTPDmessage::Flag::f_post);
}

enum HistoryItemType {
	HistoryItemMsg = 0,
	HistoryItemGroup,
	HistoryItemCollapse,
	HistoryItemJoined
};

struct HistoryMessageVia : public BaseComponent<HistoryMessageVia> {
	HistoryMessageVia(Composer*) {
	}
	void create(int32 userId);
	void resize(int32 availw) const;

	UserData *_bot = nullptr;
	mutable QString _text;
	mutable int _width = 0;
	mutable int _maxWidth = 0;
	TextLinkPtr _lnk;
};

struct HistoryMessageViews : public BaseComponent<HistoryMessageViews> {
	HistoryMessageViews(Composer*) {
	}

	QString _viewsText;
	int _views = 0;
	int _viewsWidth = 0;
};

struct HistoryMessageSigned : public BaseComponent<HistoryMessageSigned> {
	HistoryMessageSigned(Composer*) {
	}

	void create(UserData *from, const QDateTime &date);
	int maxWidth() const;

	Text _signature;
};

struct HistoryMessageForwarded : public BaseComponent<HistoryMessageForwarded> {
	HistoryMessageForwarded(Composer*) {
	}
	void create(const HistoryMessageVia *via) const;

	PeerData *_authorOriginal = nullptr;
	PeerData *_fromOriginal = nullptr;
	MsgId _originalId = 0;
	mutable Text _text = { 1 };
};

struct HistoryMessageReply : public BaseComponent<HistoryMessageReply> {
	HistoryMessageReply(Composer*) {
	}
	HistoryMessageReply &operator=(HistoryMessageReply &&other) {
		replyToMsgId = other.replyToMsgId;
		std::swap(replyToMsg, other.replyToMsg);
		replyToLnk = std_::move(other.replyToLnk);
		replyToName = std_::move(other.replyToName);
		replyToText = std_::move(other.replyToText);
		replyToVersion = other.replyToVersion;
		_maxReplyWidth = other._maxReplyWidth;
		std::swap(_replyToVia, other._replyToVia);
		return *this;
	}
	~HistoryMessageReply() {
		// clearData() should be called by holder
		t_assert(replyToMsg == nullptr);
		t_assert(_replyToVia == nullptr);
	}
	bool updateData(HistoryMessage *holder, bool force = false);
	void clearData(HistoryMessage *holder); // must be called before destructor

	void checkNameUpdate() const;
	void updateName() const;
	void resize(int width) const;
	void itemRemoved(HistoryMessage *holder, HistoryItem *removed);

	enum PaintFlag {
		PaintInBubble = 0x01,
		PaintSelected = 0x02,
	};
	Q_DECLARE_FLAGS(PaintFlags, PaintFlag);
	void paint(Painter &p, const HistoryItem *holder, int x, int y, int w, PaintFlags flags) const;

	MsgId replyToId() const {
		return replyToMsgId;
	}
	int replyToWidth() const {
		return _maxReplyWidth;
	}
	TextLinkPtr replyToLink() const {
		return replyToLnk;
	}

	MsgId replyToMsgId = 0;
	HistoryItem *replyToMsg = nullptr;
	TextLinkPtr replyToLnk;
	mutable Text replyToName, replyToText;
	mutable int replyToVersion = 0;
	mutable int _maxReplyWidth = 0;
	HistoryMessageVia *_replyToVia = nullptr;
	int toWidth = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(HistoryMessageReply::PaintFlags);

class HistoryDependentItemCallback : public SharedCallback2<void, ChannelData*, MsgId> {
public:
	HistoryDependentItemCallback(FullMsgId dependent) : _dependent(dependent) {
	}
	void call(ChannelData *channel, MsgId msgId) const override;

private:
	FullMsgId _dependent;

};

// any HistoryItem can have this Interface for
// displaying the day mark above the message
struct HistoryMessageDate : public BaseComponent<HistoryMessageDate> {
	HistoryMessageDate(Composer*) {
	}
	void init(const QDateTime &date);

	int height() const;
	void paint(Painter &p, int y, int w) const;

	QString _text;
	int _width = 0;
};

// any HistoryItem can have this Interface for
// displaying the unread messages bar above the message
struct HistoryMessageUnreadBar : public BaseComponent<HistoryMessageUnreadBar> {
	HistoryMessageUnreadBar(Composer*) {
	}
	void init(int count);

	static int height();
	static int marginTop();

	void paint(Painter &p, int y, int w) const;

	QString _text;
	int _width = 0;

	// if unread bar is freezed the new messages do not
	// increment the counter displayed by this bar
	//
	// it happens when we've opened the conversation and
	// we've seen the bar and new messages are marked as read
	// as soon as they are added to the chat history
	bool _freezed = false;
};

class HistoryMedia;
class HistoryItem : public HistoryElem, public Composer {
public:

	HistoryItem(const HistoryItem &) = delete;
	HistoryItem &operator=(const HistoryItem &) = delete;

	int resizeGetHeight(int width) {
		if (_flags & MTPDmessage_ClientFlag::f_pending_init_dimensions) {
			_flags &= ~MTPDmessage_ClientFlag::f_pending_init_dimensions;
			initDimensions();
		}
		if (_flags & MTPDmessage_ClientFlag::f_pending_resize) {
			_flags &= ~MTPDmessage_ClientFlag::f_pending_resize;
		}
		return resizeGetHeight_(width);
	}
	virtual void draw(Painter &p, const QRect &r, uint32 selection, uint64 ms) const = 0;

	virtual void dependencyItemRemoved(HistoryItem *dependency) {
	}
	virtual bool updateDependencyItem() {
		return true;
	}
	virtual MsgId dependencyMsgId() const {
		return 0;
	}
	virtual bool notificationReady() const {
		return true;
	}

	UserData *viaBot() const {
		if (const HistoryMessageVia *via = Get<HistoryMessageVia>()) {
			return via->_bot;
		}
		return nullptr;
	}

	History *history() const {
		return _history;
	}
	PeerData *from() const {
		return _from;
	}
	HistoryBlock *block() {
		return _block;
	}
	const HistoryBlock *block() const {
		return _block;
	}
	virtual void destroy();
	void detach();
	void detachFast();
	bool detached() const {
		return !_block;
	}
	void attachToBlock(HistoryBlock *block, int index) {
		t_assert(_block == nullptr);
		t_assert(_indexInBlock < 0);
		t_assert(block != nullptr);
		t_assert(index >= 0);

		_block = block;
		_indexInBlock = index;
		if (pendingResize()) {
			_history->setHasPendingResizedItems();
		}
	}
	void setIndexInBlock(int index) {
		t_assert(_block != nullptr);
		t_assert(index >= 0);

		_indexInBlock = index;
	}
	int indexInBlock() const {
		if (_indexInBlock >= 0) {
			t_assert(_block != nullptr);
			t_assert(_block->items.at(_indexInBlock) == this);
		} else if (_block != nullptr) {
			t_assert(_indexInBlock >= 0);
			t_assert(_block->items.at(_indexInBlock) == this);
		}
		return _indexInBlock;
	}
	bool out() const {
		return _flags & MTPDmessage::Flag::f_out;
	}
	bool unread() const {
		if (out() && id > 0 && id < _history->outboxReadBefore) return false;
		if (!out() && id > 0) {
			if (id < _history->inboxReadBefore) return false;
			if (channelId() != NoChannel) return true; // no unread flag for incoming messages in channels
		}
		if (history()->peer->isSelf()) return false; // messages from myself are always read
		if (out() && history()->peer->migrateTo()) return false; // outgoing messages in converted chats are always read
		return (_flags & MTPDmessage::Flag::f_unread);
	}
	bool mentionsMe() const {
		return _flags & MTPDmessage::Flag::f_mentioned;
	}
	bool isMediaUnread() const {
		return (_flags & MTPDmessage::Flag::f_media_unread) && (channelId() == NoChannel);
	}
	void markMediaRead() {
		_flags &= ~MTPDmessage::Flag::f_media_unread;
	}
	bool hasReplyMarkup() const {
		return _flags & MTPDmessage::Flag::f_reply_markup;
	}
	bool hasTextLinks() const {
		return _flags & MTPDmessage_ClientFlag::f_has_text_links;
	}
	bool isGroupMigrate() const {
		return _flags & MTPDmessage_ClientFlag::f_is_group_migrate;
	}
	bool hasViews() const {
		return _flags & MTPDmessage::Flag::f_views;
	}
	bool isPost() const {
		return _flags & MTPDmessage::Flag::f_post;
	}
	bool isImportant() const {
		return _history->isChannel() && isImportantChannelMessage(id, _flags);
	}
	bool indexInOverview() const {
		return (id > 0) && (!history()->isChannel() || history()->isMegagroup() || isPost());
	}
	bool isSilent() const {
		return _flags & MTPDmessage::Flag::f_silent;
	}
	bool hasOutLayout() const {
		return out() && !isPost();
	}
	virtual int32 viewsCount() const {
		return hasViews() ? 1 : -1;
	}

	virtual bool needCheck() const {
		return out() || (id < 0 && history()->peer->isSelf());
	}
	virtual bool hasPoint(int32 x, int32 y) const {
		return false;
	}
	virtual void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y) const {
		lnk = TextLinkPtr();
		state = HistoryDefaultCursorState;
	}
	virtual void getSymbol(uint16 &symbol, bool &after, bool &upon, int32 x, int32 y) const { // from text
		upon = hasPoint(x, y);
		symbol = upon ? 0xFFFF : 0;
		after = false;
	}
	virtual uint32 adjustSelection(uint16 from, uint16 to, TextSelectType type) const {
		return (from << 16) | to;
	}
	virtual void linkOver(const TextLinkPtr &lnk) {
	}
	virtual void linkOut(const TextLinkPtr &lnk) {
	}
	virtual HistoryItemType type() const {
		return HistoryItemMsg;
	}
	virtual bool serviceMsg() const {
		return false;
	}
	virtual void updateMedia(const MTPMessageMedia *media, bool edited = false) {
	}
	virtual int32 addToOverview(AddToOverviewMethod method) {
		return 0;
	}
	virtual bool hasBubble() const {
		return false;
	}
	virtual void previousItemChanged();

	virtual QString selectedText(uint32 selection) const {
		return qsl("[-]");
	}
	virtual QString inDialogsText() const {
		return qsl("-");
	}
	virtual QString inReplyText() const {
		return inDialogsText();
	}

	virtual void drawInfo(Painter &p, int32 right, int32 bottom, int32 width, bool selected, InfoDisplayType type) const {
	}
	virtual void setViewsCount(int32 count) {
	}
	virtual void setId(MsgId newId);
	virtual void drawInDialog(Painter &p, const QRect &r, bool act, const HistoryItem *&cacheFor, Text &cache) const = 0;
    virtual QString notificationHeader() const {
        return QString();
    }
    virtual QString notificationText() const = 0;

	bool canDelete() const {
		ChannelData *channel = _history->peer->asChannel();
		if (!channel) return !(_flags & MTPDmessage_ClientFlag::f_is_group_migrate);

		if (id == 1) return false;
		if (channel->amCreator()) return true;
		if (isPost()) {
			if (channel->amEditor() && out()) return true;
			return false;
		}
		return (channel->amEditor() || channel->amModerator() || out());
	}

	bool canPin() const {
		return id > 0 && _history->peer->isMegagroup() && (_history->peer->asChannel()->amEditor() || _history->peer->asChannel()->amCreator()) && toHistoryMessage();
	}

	bool canEdit(const QDateTime &cur) const;

	bool suggestBanReportDeleteAll() const {
		ChannelData *channel = history()->peer->asChannel();
		if (!channel || (!channel->amEditor() && !channel->amCreator())) return false;
		return !isPost() && !out() && from()->isUser() && toHistoryMessage();
	}

	bool hasDirectLink() const {
		return id > 0 && _history->peer->isChannel() && _history->peer->asChannel()->isPublic() && !_history->peer->isMegagroup();
	}
	QString directLink() const {
		return hasDirectLink() ? qsl("https://telegram.me/") + _history->peer->asChannel()->username + '/' + QString::number(id) : QString();
	}

	int32 y;
	MsgId id;
	QDateTime date;

	ChannelId channelId() const {
		return _history->channelId();
	}
	FullMsgId fullId() const {
		return FullMsgId(channelId(), id);
	}

	virtual HistoryMedia *getMedia(bool inOverview = false) const {
		return 0;
	}
	virtual void setText(const QString &text, const EntitiesInText &links) {
	}
	virtual QString originalText() const {
		return QString();
	}
	virtual EntitiesInText originalEntities() const {
		return EntitiesInText();
	}
	virtual bool textHasLinks() {
		return false;
	}

	virtual int32 infoWidth() const {
		return 0;
	}
	virtual int32 timeLeft() const {
		return 0;
	}
	virtual int32 timeWidth() const {
		return 0;
	}
	virtual bool pointInTime(int32 right, int32 bottom, int32 x, int32 y, InfoDisplayType type) const {
		return false;
	}

	int32 skipBlockWidth() const {
		return st::msgDateSpace + infoWidth() - st::msgDateDelta.x();
	}
	int32 skipBlockHeight() const {
		return st::msgDateFont->height - st::msgDateDelta.y();
	}
	QString skipBlock() const {
		return textcmdSkipBlock(skipBlockWidth(), skipBlockHeight());
	}

	virtual HistoryMessage *toHistoryMessage() { // dynamic_cast optimize
		return 0;
	}
	virtual const HistoryMessage *toHistoryMessage() const { // dynamic_cast optimize
		return 0;
	}
	MsgId replyToId() const {
		if (auto *reply = Get<HistoryMessageReply>()) {
			return reply->replyToId();
		}
		return 0;
	}

	bool hasFromName() const {
		return (!out() || isPost()) && !history()->peer->isUser();
	}
	PeerData *author() const {
		return isPost() ? history()->peer : _from;
	}

	PeerData *fromOriginal() const {
		if (const HistoryMessageForwarded *fwd = Get<HistoryMessageForwarded>()) {
			return fwd->_fromOriginal;
		}
		return from();
	}
	PeerData *authorOriginal() const {
		if (const HistoryMessageForwarded *fwd = Get<HistoryMessageForwarded>()) {
			return fwd->_authorOriginal;
		}
		return author();
	}

	// count > 0 - creates the unread bar if necessary and
	// sets unread messages count if bar is not freezed yet
	// count <= 0 - destroys the unread bar
	void setUnreadBarCount(int count);
	void destroyUnreadBar();

	// marks the unread bar as freezed so that unread
	// messages count will not change for this bar
	// when the new messages arrive in this chat history
	void setUnreadBarFreezed();

	bool pendingResize() const {
		return _flags & MTPDmessage_ClientFlag::f_pending_resize;
	}
	void setPendingResize() {
		_flags |= MTPDmessage_ClientFlag::f_pending_resize;
		if (!detached()) {
			_history->setHasPendingResizedItems();
		}
	}
	bool pendingInitDimensions() const {
		return _flags & MTPDmessage_ClientFlag::f_pending_init_dimensions;
	}
	void setPendingInitDimensions() {
		_flags |= MTPDmessage_ClientFlag::f_pending_init_dimensions;
		setPendingResize();
	}

	int displayedDateHeight() const {
		if (auto *date = Get<HistoryMessageDate>()) {
			return date->height();
		}
		return 0;
	}
	int marginTop() const {
		int result = 0;
		if (isAttachedToPrevious()) {
			result += st::msgMarginTopAttached;
		} else {
			result += st::msgMargin.top();
		}
		result += displayedDateHeight();
		if (auto *unreadbar = Get<HistoryMessageUnreadBar>()) {
			result += unreadbar->height();
		}
		return result;
	}
	int marginBottom() const {
		return st::msgMargin.bottom();
	}
	bool isAttachedToPrevious() const {
		return _flags & MTPDmessage_ClientFlag::f_attach_to_previous;
	}

	void clipCallback(ClipReaderNotification notification);

	virtual ~HistoryItem();

protected:

	HistoryItem(History *history, MsgId msgId, MTPDmessage::Flags flags, QDateTime msgDate, int32 from);

	// to completely create history item we need to call
	// a virtual method, it can not be done from constructor
	virtual void finishCreate();

	// called from resizeGetHeight() when MTPDmessage_ClientFlag::f_pending_init_dimensions is set
	virtual void initDimensions() = 0;

	virtual int resizeGetHeight_(int width) = 0;

	PeerData *_from;
	History *_history;
	HistoryBlock *_block = nullptr;
	int _indexInBlock = -1;
	MTPDmessage::Flags _flags;

	mutable int32 _authorNameVersion;

	HistoryItem *previous() const {
		if (_block && _indexInBlock >= 0) {
			if (_indexInBlock > 0) {
				return _block->items.at(_indexInBlock - 1);
			}
			if (HistoryBlock *previousBlock = _block->previous()) {
				t_assert(!previousBlock->items.isEmpty());
				return previousBlock->items.back();
			}
		}
		return nullptr;
	}

	// this should be used only in previousItemChanged()
	// to add required bits to the Interfaces mask
	// after that always use Has<HistoryMessageDate>()
	bool displayDate() const {
		if (HistoryItem *prev = previous()) {
			return prev->date.date().day() != date.date().day();
		}
		return true;
	}

	// this should be used only in previousItemChanged() or when
	// HistoryMessageDate or HistoryMessageUnreadBar bit is changed in the Interfaces mask
	// then the result should be cached in a client side flag MTPDmessage_ClientFlag::f_attach_to_previous
	void recountAttachToPrevious();

};

// make all the constructors in HistoryItem children protected
// and wrapped with a static create() call with the same args
// so that history item can not be created directly, without
// calling a virtual finishCreate() method
template <typename T>
class HistoryItemInstantiated {
public:
	template <typename ... Args>
	static T *_create(Args ... args) {
		T *result = new T(args ...);
		result->finishCreate();
		return result;
	}
};

class MessageLink : public ITextLink {
	TEXT_LINK_CLASS(MessageLink)

public:
	MessageLink(PeerId peer, MsgId msgid) : _peer(peer), _msgid(msgid) {
	}
	MessageLink(HistoryItem *item) : _peer(item->history()->peer->id), _msgid(item->id) {
	}
	void onClick(Qt::MouseButton button) const;
	PeerId peer() const {
		return _peer;
	}
	MsgId msgid() const {
		return _msgid;
	}

private:
	PeerId _peer;
	MsgId _msgid;
};

class CommentsLink : public ITextLink {
	TEXT_LINK_CLASS(CommentsLink)

public:
	CommentsLink(HistoryItem *item) : _item(item) {
	}
	void onClick(Qt::MouseButton button) const;

private:
	HistoryItem *_item;
};

class RadialAnimation {
public:

	RadialAnimation(AnimationCreator creator);

	float64 opacity() const {
		return _opacity;
	}
	bool animating() const {
		return _animation.animating();
	}

	void start(float64 prg);
	void update(float64 prg, bool finished, uint64 ms);
	void stop();

	void step(uint64 ms);
	void step() {
		step(getms());
	}

	void draw(Painter &p, const QRect &inner, int32 thickness, const style::color &color);

private:

	uint64 _firstStart, _lastStart, _lastTime;
	float64 _opacity;
	anim::ivalue a_arcEnd, a_arcStart;
	Animation _animation;

};

class HistoryMedia : public HistoryElem {
public:

	HistoryMedia() : _width(0) {
	}
	HistoryMedia(const HistoryMedia &other) : _width(0) {
	}

	virtual HistoryMediaType type() const = 0;
	virtual const QString inDialogsText() const = 0;
	virtual const QString inHistoryText() const = 0;

	bool hasPoint(int32 x, int32 y, const HistoryItem *parent) const {
		return (x >= 0 && y >= 0 && x < _width && y < _height);
	}

	virtual bool isDisplayed() const {
		return true;
	}
	virtual void initDimensions(const HistoryItem *parent) = 0;
	virtual int32 resize(int32 width, const HistoryItem *parent) { // return new height
		_width = qMin(width, _maxw);
		return _height;
	}
	virtual void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const = 0;
	virtual void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const = 0;

	virtual void linkOver(HistoryItem *parent, const TextLinkPtr &lnk) {
	}
	virtual void linkOut(HistoryItem *parent, const TextLinkPtr &lnk) {
	}

	virtual bool uploading() const {
		return false;
	}
	virtual HistoryMedia *clone() const = 0;

	virtual DocumentData *getDocument() {
		return 0;
	}
	virtual ClipReader *getClipReader() {
		return 0;
	}

	bool playInline(HistoryItem *item/*, bool autoplay = false*/) {
		return playInline(item, false);
	}
	virtual bool playInline(HistoryItem *item, bool autoplay) {
		return false;
	}
	virtual void stopInline(HistoryItem *item) {
	}

	virtual void attachToItem(HistoryItem *item) {
	}

	virtual void detachFromItem(HistoryItem *item) {
	}

	virtual void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) {
	}

	virtual bool isImageLink() const {
		return false;
	}

	virtual bool animating() const {
		return false;
	}

	virtual bool hasReplyPreview() const {
		return false;
	}
	virtual ImagePtr replyPreview() {
		return ImagePtr();
	}
	virtual QString getCaption() const {
		return QString();
	}
	virtual bool needsBubble(const HistoryItem *parent) const = 0;
	virtual bool customInfoLayout() const = 0;
	virtual QMargins bubbleMargins() const {
		return QMargins();
	}
	virtual bool hideFromName() const {
		return false;
	}
	virtual bool hideForwardedFrom() const {
		return false;
	}

	int32 currentWidth() const {
		return _width;
	}

protected:

	int32 _width;

};

inline MediaOverviewType mediaToOverviewType(HistoryMedia *media) {
	switch (media->type()) {
	case MediaTypePhoto: return OverviewPhotos;
	case MediaTypeVideo: return OverviewVideos;
	case MediaTypeFile: return OverviewFiles;
	case MediaTypeMusicFile: return media->getDocument()->isMusic() ? OverviewMusicFiles : OverviewFiles;
	case MediaTypeVoiceFile: return OverviewVoiceFiles;
	case MediaTypeGif: return media->getDocument()->isGifv() ? OverviewCount : OverviewFiles;
//	case MediaTypeSticker: return OverviewFiles;
	}
	return OverviewCount;
}

class HistoryFileMedia : public HistoryMedia {
public:

	HistoryFileMedia();

	void linkOver(HistoryItem *parent, const TextLinkPtr &lnk);
	void linkOut(HistoryItem *parent, const TextLinkPtr &lnk);

	~HistoryFileMedia();

protected:

	TextLinkPtr _openl, _savel, _cancell;
	void setLinks(ITextLink *openl, ITextLink *savel, ITextLink *cancell);

	// >= 0 will contain download / upload string, _statusSize = loaded bytes
	// < 0 will contain played string, _statusSize = -(seconds + 1) played
	// 0x7FFFFFF0 will contain status for not yet downloaded file
	// 0x7FFFFFF1 will contain status for already downloaded file
	// 0x7FFFFFF2 will contain status for failed to download / upload file
	mutable int32 _statusSize;
	mutable QString _statusText;

	// duration = -1 - no duration, duration = -2 - "GIF" duration
	void setStatusSize(int32 newSize, int32 fullSize, int32 duration, qint64 realDuration) const;

	void step_thumbOver(const HistoryItem *parent, float64 ms, bool timer);
	void step_radial(const HistoryItem *parent, uint64 ms, bool timer);

	void ensureAnimation(const HistoryItem *parent) const;
	void checkAnimationFinished();

	bool isRadialAnimation(uint64 ms) const {
		if (!_animation || !_animation->radial.animating()) return false;

		_animation->radial.step(ms);
		return _animation && _animation->radial.animating();
	}
	bool isThumbAnimation(uint64 ms) const {
		if (!_animation || !_animation->_a_thumbOver.animating()) return false;

		_animation->_a_thumbOver.step(ms);
		return _animation && _animation->_a_thumbOver.animating();
	}

	virtual float64 dataProgress() const = 0;
	virtual bool dataFinished() const = 0;
	virtual bool dataLoaded() const = 0;

	struct AnimationData {
		AnimationData(AnimationCreator thumbOverCallbacks, AnimationCreator radialCallbacks) : a_thumbOver(0, 0)
			, _a_thumbOver(thumbOverCallbacks)
			, radial(radialCallbacks) {
		}
		anim::fvalue a_thumbOver;
		Animation _a_thumbOver;

		RadialAnimation radial;
	};
	mutable AnimationData *_animation;

private:

	HistoryFileMedia(const HistoryFileMedia &other);

};

class HistoryPhoto : public HistoryFileMedia {
public:

	HistoryPhoto(PhotoData *photo, const QString &caption, const HistoryItem *parent);
	HistoryPhoto(PeerData *chat, const MTPDphoto &photo, int32 width = 0);
	HistoryPhoto(const HistoryPhoto &other);
	void init();
	HistoryMediaType type() const override {
		return MediaTypePhoto;
	}
	HistoryMedia *clone() const override {
		return new HistoryPhoto(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	PhotoData *photo() const {
		return _data;
	}

	void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) override;

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	bool hasReplyPreview() const override {
		return !_data->thumb->isNull();
	}
	ImagePtr replyPreview() override;

	QString getCaption() const override {
		return _caption.original();
	}
	bool needsBubble(const HistoryItem *parent) const override {
		return !_caption.isEmpty() || parent->Has<HistoryMessageForwarded>() || parent->Has<HistoryMessageReply>() || parent->viaBot();
	}
	bool customInfoLayout() const override {
		return _caption.isEmpty();
	}
	bool hideFromName() const override {
		return true;
	}

protected:

	float64 dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading() && !_data->uploading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}

private:
	PhotoData *_data;
	int16 _pixw, _pixh;
	Text _caption;

};

class HistoryVideo : public HistoryFileMedia {
public:

	HistoryVideo(DocumentData *document, const QString &caption, const HistoryItem *parent);
	HistoryVideo(const HistoryVideo &other);
	HistoryMediaType type() const override {
		return MediaTypeVideo;
	}
	HistoryMedia *clone() const override {
		return new HistoryVideo(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	DocumentData *getDocument() override {
		return _data;
	}

	bool uploading() const override {
		return _data->uploading();
	}

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	bool hasReplyPreview() const override {
		return !_data->thumb->isNull();
	}
	ImagePtr replyPreview() override;

	QString getCaption() const override {
		return _caption.original();
	}
	bool needsBubble(const HistoryItem *parent) const override {
		return !_caption.isEmpty() || parent->Has<HistoryMessageForwarded>() || parent->Has<HistoryMessageReply>() || parent->viaBot();
	}
	bool customInfoLayout() const override {
		return _caption.isEmpty();
	}
	bool hideFromName() const override {
		return true;
	}

protected:

	float64 dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading() && !_data->uploading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}

private:
	DocumentData *_data;
	int32 _thumbw;
	Text _caption;

	void setStatusSize(int32 newSize) const;
	void updateStatusText(const HistoryItem *parent) const;

};

struct HistoryDocumentThumbed : public BaseComponent<HistoryDocumentThumbed> {
	HistoryDocumentThumbed(Composer*) {
	}
	TextLinkPtr _linksavel, _linkcancell;
	int _thumbw = 0;

	mutable int _linkw = 0;
	mutable QString _link;
};
struct HistoryDocumentCaptioned : public BaseComponent<HistoryDocumentCaptioned> {
	HistoryDocumentCaptioned(Composer*) : _caption(st::msgFileMinWidth - st::msgPadding.left() - st::msgPadding.right()) {
	}
	Text _caption;
};
struct HistoryDocumentNamed : public BaseComponent<HistoryDocumentNamed> {
	HistoryDocumentNamed(Composer*) {
	}
	QString _name;
	int _namew = 0;
};
class HistoryDocument;
struct HistoryDocumentVoicePlayback {
	HistoryDocumentVoicePlayback(const HistoryDocument *that);

	int32 _position;
	anim::fvalue a_progress;
	Animation _a_progress;
};
struct HistoryDocumentVoice : public BaseComponent<HistoryDocumentVoice> {
	HistoryDocumentVoice(Composer*) {
	}
	HistoryDocumentVoice &operator=(HistoryDocumentVoice &&other) {
		std::swap(_playback, other._playback);
		return *this;
	}
	~HistoryDocumentVoice() {
		deleteAndMark(_playback);
	}
	void ensurePlayback(const HistoryDocument *interfaces) const;
	void checkPlaybackFinished() const;
	mutable HistoryDocumentVoicePlayback *_playback = nullptr;
};

class HistoryDocument : public HistoryFileMedia, public Composer {
public:

	HistoryDocument(DocumentData *document, const QString &caption, const HistoryItem *parent);
	HistoryDocument(const HistoryDocument &other);
	HistoryMediaType type() const override {
		return _data->voice() ? MediaTypeVoiceFile : (_data->song() ? MediaTypeMusicFile : MediaTypeFile);
	}
	HistoryMedia *clone() const override {
		return new HistoryDocument(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	bool uploading() const override {
		return _data->uploading();
	}

	DocumentData *getDocument() override {
		return _data;
	}

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) override;

	bool hasReplyPreview() const override {
		return !_data->thumb->isNull();
	}
	ImagePtr replyPreview() override;

	QString getCaption() const override {
		if (const HistoryDocumentCaptioned *captioned = Get<HistoryDocumentCaptioned>()) {
			return captioned->_caption.original();
		}
		return QString();
	}
	bool needsBubble(const HistoryItem *parent) const override {
		return true;
	}
	bool customInfoLayout() const override {
		return false;
	}
	QMargins bubbleMargins() const override {
		return Get<HistoryDocumentThumbed>() ? QMargins(st::msgFileThumbPadding.left(), st::msgFileThumbPadding.top(), st::msgFileThumbPadding.left(), st::msgFileThumbPadding.bottom()) : st::msgPadding;
	}
	bool hideForwardedFrom() const override {
		return _data->song();
	}

	void step_voiceProgress(float64 ms, bool timer);

protected:

	float64 dataProgress() const override {
		return _data->progress();
	}
	bool dataFinished() const override {
		return !_data->loading() && !_data->uploading();
	}
	bool dataLoaded() const override {
		return _data->loaded();
	}

private:

	void createInterfaces(bool caption);
	const HistoryItem *_parent;
	DocumentData *_data;

	void setStatusSize(int32 newSize, qint64 realDuration = 0) const;
	bool updateStatusText(const HistoryItem *parent) const; // returns showPause

};

class HistoryGif : public HistoryFileMedia {
public:

	HistoryGif(DocumentData *document, const QString &caption, const HistoryItem *parent);
	HistoryGif(const HistoryGif &other);
	HistoryMediaType type() const override {
		return MediaTypeGif;
	}
	HistoryMedia *clone() const override {
		return new HistoryGif(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	bool uploading() const override {
		return _data->uploading();
	}

	DocumentData *getDocument() override {
		return _data;
	}
	ClipReader *getClipReader() override {
		return gif();
	}

	bool playInline(HistoryItem *item, bool autoplay) override;
	void stopInline(HistoryItem *item) override;

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) override;

	bool hasReplyPreview() const override {
		return !_data->thumb->isNull();
	}
	ImagePtr replyPreview() override;

	QString getCaption() const override {
		return _caption.original();
	}
	bool needsBubble(const HistoryItem *parent) const override {
		return !_caption.isEmpty() || parent->Has<HistoryMessageForwarded>() || parent->Has<HistoryMessageReply>() || parent->viaBot();
	}
	bool customInfoLayout() const override {
		return _caption.isEmpty();
	}
	bool hideFromName() const override {
		return true;
	}

	~HistoryGif();

protected:

	float64 dataProgress() const override;
	bool dataFinished() const override;
	bool dataLoaded() const override;

private:

	const HistoryItem *_parent;
	DocumentData *_data;
	int32 _thumbw, _thumbh;
	Text _caption;

	ClipReader *_gif;
	ClipReader *gif() {
		return (_gif == BadClipReader) ? nullptr : _gif;
	}
	const ClipReader *gif() const {
		return (_gif == BadClipReader) ? nullptr : _gif;
	}

	void setStatusSize(int32 newSize) const;
	void updateStatusText(const HistoryItem *parent) const;

};

class HistorySticker : public HistoryMedia {
public:

	HistorySticker(DocumentData *document);
	HistoryMediaType type() const override {
		return MediaTypeSticker;
	}
	HistoryMedia *clone() const override {
		return new HistorySticker(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	DocumentData *getDocument() override {
		return _data;
	}

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) override;

	bool needsBubble(const HistoryItem *parent) const override {
		return false;
	}
	bool customInfoLayout() const override {
		return true;
	}

private:

	int16 _pixw, _pixh;
	DocumentData *_data;
	QString _emoji;

};

class SendMessageLink : public PeerLink {
	TEXT_LINK_CLASS(SendMessageLink)

public:
	SendMessageLink(PeerData *peer) : PeerLink(peer) {
	}
	void onClick(Qt::MouseButton button) const;

};

class AddContactLink : public MessageLink {
	TEXT_LINK_CLASS(AddContactLink)

public:
	AddContactLink(PeerId peer, MsgId msgid) : MessageLink(peer, msgid) {
	}
	void onClick(Qt::MouseButton button) const;

};

class HistoryContact : public HistoryMedia {
public:

	HistoryContact(int32 userId, const QString &first, const QString &last, const QString &phone);
	HistoryMediaType type() const override {
		return MediaTypeContact;
	}
	HistoryMedia *clone() const override {
		return new HistoryContact(_userId, _fname, _lname, _phone);
	}

	void initDimensions(const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	void updateFrom(const MTPMessageMedia &media, HistoryItem *parent) override;

	bool needsBubble(const HistoryItem *parent) const override {
		return true;
	}
	bool customInfoLayout() const override {
		return false;
	}

	const QString &fname() const {
		return _fname;
	}
	const QString &lname() const {
		return _lname;
	}
	const QString &phone() const {
		return _phone;
	}

private:

	int32 _userId;
	UserData *_contact;

	int32 _phonew;
	QString _fname, _lname, _phone;
	Text _name;

	TextLinkPtr _linkl;
	int32 _linkw;
	QString _link;
};

class HistoryWebPage : public HistoryMedia {
public:

	HistoryWebPage(WebPageData *data);
	HistoryWebPage(const HistoryWebPage &other);
	HistoryMediaType type() const override {
		return MediaTypeWebPage;
	}
	HistoryMedia *clone() const override {
		return new HistoryWebPage(*this);
	}

	void initDimensions(const HistoryItem *parent) override;
	int32 resize(int32 width, const HistoryItem *parent) override;

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const override;

	const QString inDialogsText() const override;
	const QString inHistoryText() const override;

	void linkOver(HistoryItem *parent, const TextLinkPtr &lnk) override;
	void linkOut(HistoryItem *parent, const TextLinkPtr &lnk) override;

	bool isDisplayed() const override {
		return !_data->pendingTill;
	}
	DocumentData *getDocument() override {
		return _attach ? _attach->getDocument() : 0;
	}
	ClipReader *getClipReader() override {
		return _attach ? _attach->getClipReader() : 0;
	}
	bool playInline(HistoryItem *item, bool autoplay) override {
		return _attach ? _attach->playInline(item, autoplay) : false;
	}
	void stopInline(HistoryItem *item) override {
		if (_attach) _attach->stopInline(item);
	}

	void attachToItem(HistoryItem *item) override;
	void detachFromItem(HistoryItem *item) override;

	bool hasReplyPreview() const override {
		return (_data->photo && !_data->photo->thumb->isNull()) || (_data->doc && !_data->doc->thumb->isNull());
	}
	ImagePtr replyPreview() override;

	WebPageData *webpage() {
		return _data;
	}

	bool needsBubble(const HistoryItem *parent) const override {
		return true;
	}
	bool customInfoLayout() const override {
		return false;
	}

	HistoryMedia *attach() const {
		return _attach;
	}

	~HistoryWebPage();

private:
	WebPageData *_data;
	TextLinkPtr _openl;
	HistoryMedia *_attach;

	bool _asArticle;
	int32 _titleLines, _descriptionLines;

	Text _title, _description;
	int32 _siteNameWidth;

	QString _duration;
	int32 _durationWidth;

	int16 _pixw, _pixh;
};

void initImageLinkManager();
void reinitImageLinkManager();
void deinitImageLinkManager();

struct LocationData {
	LocationData(const LocationCoords &coords) : coords(coords), loading(false) {
	}

	LocationCoords coords;
	ImagePtr thumb;
	bool loading;

	void load();
};

class LocationManager : public QObject {
	Q_OBJECT
public:
	LocationManager() : manager(0), black(0) {
	}
	void init();
	void reinit();
	void deinit();

	void getData(LocationData *data);

	~LocationManager() {
		deinit();
	}

public slots:
	void onFinished(QNetworkReply *reply);
	void onFailed(QNetworkReply *reply);

private:
	void failed(LocationData *data);

	QNetworkAccessManager *manager;
	QMap<QNetworkReply*, LocationData*> dataLoadings, imageLoadings;
	QMap<LocationData*, int32> serverRedirects;
	ImagePtr *black;
};

class HistoryLocation : public HistoryMedia {
public:

	HistoryLocation(const LocationCoords &coords, const QString &title = QString(), const QString &description = QString());
	HistoryMediaType type() const {
		return MediaTypeLocation;
	}
	HistoryMedia *clone() const {
		return new HistoryLocation(*this);
	}

	void initDimensions(const HistoryItem *parent);
	int32 resize(int32 width, const HistoryItem *parent);

	void draw(Painter &p, const HistoryItem *parent, const QRect &r, bool selected, uint64 ms) const;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y, const HistoryItem *parent) const;

	const QString inDialogsText() const;
	const QString inHistoryText() const;

	bool isImageLink() const {
		return true;
	}

	bool needsBubble(const HistoryItem *parent) const {
		return !_title.isEmpty() || !_description.isEmpty() || parent->Has<HistoryMessageForwarded>() || parent->Has<HistoryMessageReply>() || parent->viaBot();
	}
	bool customInfoLayout() const {
		return true;
	}

private:
	LocationData *_data;
	Text _title, _description;
	TextLinkPtr _link;

	int32 fullWidth() const;
	int32 fullHeight() const;

};

class ViaInlineBotLink : public ITextLink {
	TEXT_LINK_CLASS(ViaInlineBotLink)

public:
	ViaInlineBotLink(UserData *bot) : _bot(bot) {
	}
	void onClick(Qt::MouseButton button) const;

private:
	UserData *_bot;

};

class HistoryMessage : public HistoryItem, private HistoryItemInstantiated<HistoryMessage> {
public:

	static HistoryMessage *create(History *history, const MTPDmessage &msg) {
		return _create(history, msg);
	}
	static HistoryMessage *create(History *history, MsgId msgId, MTPDmessage::Flags flags, QDateTime date, int32 from, HistoryMessage *fwd) {
		return _create(history, msgId, flags, date, from, fwd);
	}
	static HistoryMessage *create(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, const QString &msg, const EntitiesInText &entities) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, msg, entities);
	}
	static HistoryMessage *create(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, DocumentData *doc, const QString &caption) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, doc, caption);
	}
	static HistoryMessage *create(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, PhotoData *photo, const QString &caption) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, photo, caption);
	}

	void initTime();
	void initMedia(const MTPMessageMedia *media, QString &currentText);
	void initMediaFromDocument(DocumentData *doc, const QString &caption);
	void fromNameUpdated(int32 width) const;

	int32 plainMaxWidth() const;
	void countPositionAndSize(int32 &left, int32 &width) const;

	bool emptyText() const {
		return _text.isEmpty();
	}
	bool drawBubble() const {
		return _media ? (!emptyText() || _media->needsBubble(this)) : true;
	}
	bool hasBubble() const override {
		return drawBubble();
	}
	bool displayFromName() const {
		if (!hasFromName()) return false;
		if (isAttachedToPrevious()) return false;

		return (!emptyText() || !_media || !_media->isDisplayed() || Has<HistoryMessageReply>() || Has<HistoryMessageForwarded>() || viaBot() || !_media->hideFromName());
	}
	bool uploading() const {
		return _media && _media->uploading();
	}

	void drawInfo(Painter &p, int32 right, int32 bottom, int32 width, bool selected, InfoDisplayType type) const override;
	void setViewsCount(int32 count) override;
	void setId(MsgId newId) override;
	void draw(Painter &p, const QRect &r, uint32 selection, uint64 ms) const override;

	void dependencyItemRemoved(HistoryItem *dependency) override;

	void destroy() override;

	bool hasPoint(int32 x, int32 y) const override;
	bool pointInTime(int32 right, int32 bottom, int32 x, int32 y, InfoDisplayType type) const override;

	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y) const override;

	void getSymbol(uint16 &symbol, bool &after, bool &upon, int32 x, int32 y) const override;
	uint32 adjustSelection(uint16 from, uint16 to, TextSelectType type) const override {
		return _text.adjustSelection(from, to, type);
	}
	void linkOver(const TextLinkPtr &lnk) override {
		if (_media) _media->linkOver(this, lnk);
	}
	void linkOut(const TextLinkPtr &lnk) override {
		if (_media) _media->linkOut(this, lnk);
	}

	void drawInDialog(Painter &p, const QRect &r, bool act, const HistoryItem *&cacheFor, Text &cache) const override;
    QString notificationHeader() const override;
    QString notificationText() const override;

	void updateMedia(const MTPMessageMedia *media, bool edited = false) override {
		if (!edited && media && _media && _media->type() != MediaTypeWebPage) {
			_media->updateFrom(*media, this);
		} else {
			setMedia(media);
		}
		setPendingInitDimensions();
	}
	int32 addToOverview(AddToOverviewMethod method) override;
	void eraseFromOverview();

	QString selectedText(uint32 selection) const override;
	QString inDialogsText() const override;
	HistoryMedia *getMedia(bool inOverview = false) const override;
	void setMedia(const MTPMessageMedia *media);
	void setText(const QString &text, const EntitiesInText &entities) override;
	QString originalText() const override;
	EntitiesInText originalEntities() const override;
	bool textHasLinks() override;

	int32 infoWidth() const override {
		int32 result = _timeWidth;
		if (const HistoryMessageViews *views = Get<HistoryMessageViews>()) {
			result += st::msgDateViewsSpace + views->_viewsWidth + st::msgDateCheckSpace + st::msgViewsImg.pxWidth();
		} else if (id < 0 && history()->peer->isSelf()) {
			result += st::msgDateCheckSpace + st::msgCheckImg.pxWidth();
		}
		if (out() && !isPost()) {
			result += st::msgDateCheckSpace + st::msgCheckImg.pxWidth();
		}
		return result;
	}
	int32 timeLeft() const override {
		int32 result = 0;
		if (const HistoryMessageViews *views = Get<HistoryMessageViews>()) {
			result += st::msgDateViewsSpace + views->_viewsWidth + st::msgDateCheckSpace + st::msgViewsImg.pxWidth();
		} else if (id < 0 && history()->peer->isSelf()) {
			result += st::msgDateCheckSpace + st::msgCheckImg.pxWidth();
		}
		return result;
	}
	int32 timeWidth() const override {
		return _timeWidth;
	}

	int32 viewsCount() const override {
		if (const HistoryMessageViews *views = Get<HistoryMessageViews>()) {
			return views->_views;
		}
		return HistoryItem::viewsCount();
	}

	bool updateDependencyItem() override {
		if (auto *reply = Get<HistoryMessageReply>()) {
			return reply->updateData(this, true);
		}
		return true;
	}
	MsgId dependencyMsgId() const override {
		return replyToId();
	}

	HistoryMessage *toHistoryMessage() override { // dynamic_cast optimize
		return this;
	}
	const HistoryMessage *toHistoryMessage() const override { // dynamic_cast optimize
		return this;
	}

	// hasFromPhoto() returns true even if we don't display the photo
	// but we need to skip a place at the left side for this photo
	bool displayFromPhoto() const;
	bool hasFromPhoto() const;

	~HistoryMessage();

protected:

	HistoryMessage(History *history, const MTPDmessage &msg);
	HistoryMessage(History *history, MsgId msgId, MTPDmessage::Flags flags, QDateTime date, int32 from, HistoryMessage *fwd); // local forwarded
	HistoryMessage(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, const QString &msg, const EntitiesInText &entities); // local message
	HistoryMessage(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, DocumentData *doc, const QString &caption); // local document
	HistoryMessage(History *history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId, QDateTime date, int32 from, PhotoData *photo, const QString &caption); // local photo
	friend class HistoryItemInstantiated<HistoryMessage>;

	void initDimensions() override;
	int resizeGetHeight_(int width) override;

	bool displayForwardedFrom() const {
		if (const HistoryMessageForwarded *fwd = Get<HistoryMessageForwarded>()) {
			return Has<HistoryMessageVia>() || !_media || !_media->isDisplayed() || fwd->_authorOriginal->isChannel() || !_media->hideForwardedFrom();
		}
		return false;
	}

	void paintForwardedInfo(Painter &p, QRect &trect, bool selected) const;
	void paintReplyInfo(Painter &p, QRect &trect, bool selected) const;

	// this method draws "via @bot" if it is not painted in forwarded info or in from name
	void paintViaBotIdInfo(Painter &p, QRect &trect, bool selected) const;

	Text _text = { int(st::msgMinWidth) };

	int _textWidth = 0;
	int _textHeight = 0;

	HistoryMedia *_media = nullptr;
	QString _timeText;
	int _timeWidth = 0;

	void createInterfacesHelper(MTPDmessage::Flags flags, MsgId replyTo, int32 viaBotId);
	void createInterfaces(MsgId replyTo, int32 viaBotId, int32 viewsCount, const PeerId &authorIdOriginal = 0, const PeerId &fromIdOriginal = 0, MsgId originalId = 0);

};

inline MTPDmessage::Flags newMessageFlags(PeerData *p) {
	MTPDmessage::Flags result = 0;
	if (!p->isSelf()) {
		result |= MTPDmessage::Flag::f_out;
		if (p->isChat() || (p->isUser() && !p->asUser()->botInfo)) {
			result |= MTPDmessage::Flag::f_unread;
		}
	}
	return result;
}
inline MTPDmessage::Flags newForwardedFlags(PeerData *p, int32 from, HistoryMessage *fwd) {
	MTPDmessage::Flags result = newMessageFlags(p);
	if (from) {
		result |= MTPDmessage::Flag::f_from_id;
	}
	if (fwd->Has<HistoryMessageVia>()) {
		result |= MTPDmessage::Flag::f_via_bot_id;
	}
	if (!p->isChannel()) {
		if (HistoryMedia *media = fwd->getMedia()) {
			if (media->type() == MediaTypeVoiceFile) {
				result |= MTPDmessage::Flag::f_media_unread;
//			} else if (media->type() == MediaTypeVideo) {
//				result |= MTPDmessage::flag_media_unread;
			}
		}
	}
	return result;
}

struct HistoryServicePinned : public BaseComponent<HistoryServicePinned> {
	HistoryServicePinned(Composer*) {
	}

	MsgId msgId = 0;
	HistoryItem *msg = nullptr;
	TextLinkPtr lnk;
};

class HistoryService : public HistoryItem, private HistoryItemInstantiated<HistoryService> {
public:

	static HistoryService *create(History *history, const MTPDmessageService &msg) {
		return _create(history, msg);
	}
	static HistoryService *create(History *history, MsgId msgId, QDateTime date, const QString &msg, MTPDmessage::Flags flags = 0, HistoryMedia *media = 0, int32 from = 0) {
		return _create(history, msgId, date, msg, flags, media, from);
	}

	bool updateDependencyItem() override {
		return updatePinned(true);
	}
	MsgId dependencyMsgId() const override {
		if (const HistoryServicePinned *pinned = Get<HistoryServicePinned>()) {
			return pinned->msgId;
		}
		return 0;
	}
	bool notificationReady() const override {
		if (const HistoryServicePinned *pinned = Get<HistoryServicePinned>()) {
			return (pinned->msg || !pinned->msgId);
		}
		return true;
	}

	void countPositionAndSize(int32 &left, int32 &width) const;

	void draw(Painter &p, const QRect &r, uint32 selection, uint64 ms) const override;
	bool hasPoint(int32 x, int32 y) const override;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y) const override;
	void getSymbol(uint16 &symbol, bool &after, bool &upon, int32 x, int32 y) const override;
	uint32 adjustSelection(uint16 from, uint16 to, TextSelectType type) const override {
		return _text.adjustSelection(from, to, type);
	}

	void linkOver(const TextLinkPtr &lnk) override {
		if (_media) _media->linkOver(this, lnk);
	}
	void linkOut(const TextLinkPtr &lnk) override {
		if (_media) _media->linkOut(this, lnk);
	}

	void drawInDialog(Painter &p, const QRect &r, bool act, const HistoryItem *&cacheFor, Text &cache) const override;
    QString notificationText() const override;

	bool needCheck() const override {
		return false;
	}
	bool serviceMsg() const override {
		return true;
	}
	QString selectedText(uint32 selection) const override;
	QString inDialogsText() const override;
	QString inReplyText() const override;

	HistoryMedia *getMedia(bool inOverview = false) const override;

	void setServiceText(const QString &text);

	~HistoryService();

protected:

	HistoryService(History *history, const MTPDmessageService &msg);
	HistoryService(History *history, MsgId msgId, QDateTime date, const QString &msg, MTPDmessage::Flags flags = 0, HistoryMedia *media = 0, int32 from = 0);
	friend class HistoryItemInstantiated<HistoryService>;

	void initDimensions() override;
	int resizeGetHeight_(int width) override;

	void setMessageByAction(const MTPmessageAction &action);
	bool updatePinned(bool force = false);
	bool updatePinnedText(const QString *pfrom = nullptr, QString *ptext = nullptr);

	Text _text = { int(st::msgMinWidth) };
	HistoryMedia *_media = nullptr;

	int32 _textWidth, _textHeight;
};

class HistoryGroup : public HistoryService, private HistoryItemInstantiated<HistoryGroup> {
public:

	static HistoryGroup *create(History *history, const MTPDmessageGroup &group, const QDateTime &date) {
		return _create(history, group, date);
	}
	static HistoryGroup *create(History *history, HistoryItem *newItem, const QDateTime &date) {
		return _create(history, newItem, date);
	}

	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y) const;
	void getSymbol(uint16 &symbol, bool &after, bool &upon, int32 x, int32 y) const {
		symbol = 0xFFFF;
		after = false;
		upon = false;
	}
	QString selectedText(uint32 selection) const {
		return QString();
	}
	HistoryItemType type() const {
		return HistoryItemGroup;
	}
	void uniteWith(MsgId minId, MsgId maxId, int32 count);
	void uniteWith(HistoryItem *item) {
		uniteWith(item->id - 1, item->id + 1, 1);
	}
	void uniteWith(HistoryGroup *other) {
		uniteWith(other->_minId, other->_maxId, other->_count);
	}

	bool decrementCount(); // returns true if result count > 0

	MsgId minId() const {
		return _minId;
	}
	MsgId maxId() const {
		return _maxId;
	}

protected:

	HistoryGroup(History *history, const MTPDmessageGroup &group, const QDateTime &date);
	HistoryGroup(History *history, HistoryItem *newItem, const QDateTime &date);
	using HistoryItemInstantiated<HistoryGroup>::_create;
	friend class HistoryItemInstantiated<HistoryGroup>;

private:
	MsgId _minId, _maxId;
	int32 _count;

	TextLinkPtr _lnk;

	void updateText();

};

class HistoryCollapse : public HistoryService, private HistoryItemInstantiated<HistoryCollapse> {
public:

	static HistoryCollapse *create(History *history, MsgId wasMinId, const QDateTime &date) {
		return _create(history, wasMinId, date);
	}

	void draw(Painter &p, const QRect &r, uint32 selection, uint64 ms) const;
	void getState(TextLinkPtr &lnk, HistoryCursorState &state, int32 x, int32 y) const;
	void getSymbol(uint16 &symbol, bool &after, bool &upon, int32 x, int32 y) const {
		symbol = 0xFFFF;
		after = false;
		upon = false;
	}
	QString selectedText(uint32 selection) const {
		return QString();
	}
	HistoryItemType type() const {
		return HistoryItemCollapse;
	}
	MsgId wasMinId() const {
		return _wasMinId;
	}

protected:

	HistoryCollapse(History *history, MsgId wasMinId, const QDateTime &date);
	using HistoryItemInstantiated<HistoryCollapse>::_create;
	friend class HistoryItemInstantiated<HistoryCollapse>;

private:
	MsgId _wasMinId;

};

class HistoryJoined : public HistoryService, private HistoryItemInstantiated<HistoryJoined> {
public:

	static HistoryJoined *create(History *history, const QDateTime &date, UserData *from, MTPDmessage::Flags flags) {
		return _create(history, date, from, flags);
	}

	HistoryItemType type() const {
		return HistoryItemJoined;
	}

protected:

	HistoryJoined(History *history, const QDateTime &date, UserData *from, MTPDmessage::Flags flags);
	using HistoryItemInstantiated<HistoryJoined>::_create;
	friend class HistoryItemInstantiated<HistoryJoined>;

};

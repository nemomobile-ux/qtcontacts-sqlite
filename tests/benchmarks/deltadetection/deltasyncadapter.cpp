/*
 * Copyright (C) 2016 Jolla Ltd.
 * Contact: Chris Adams <chris.adams@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "deltasyncadapter.h"
#include "../../../src/extensions/twowaycontactsyncadapter_impl.h"
#include "../../../src/extensions/qtcontacts-extensions.h"

#include <QTimer>

#include <QContact>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>
#include <QContactName>

#define TSA_GUID_STRING(accountId, fname, lname) QString(accountId + ":" + fname + lname)

namespace {

QContactPhoneNumber generatePhoneNumber(QStringList *seen)
{
    QContactPhoneNumber ret;
    int random = qrand();
    int random2 = qrand();

    if (random % 2 == 0) {
        ret.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
    }

    if (random % 3 == 0) {
        ret.setContexts(QContactDetail::ContextHome);
    } else {
        ret.setContexts(QContactDetail::ContextWork);
    }

    QString number;
    if (random % 5 == 0) {
        // 8 digit phone number
        number = QStringLiteral("%1%2%3%4%5%6%7%8")
                .arg(random%10).arg(random2%9).arg(random%8)
                .arg(random2%7).arg(random%6).arg(random2%5)
                .arg(random%4).arg(random2%3);
    } else {
        // 9 digit phone number
        number = QStringLiteral("%1%2%3%4%5%6%7%8%9")
                .arg(random2%10).arg(random2%9).arg(random2%8)
                .arg(random%7).arg(random%6).arg(random%5)
                .arg(random2%4).arg(random2%3).arg(random%2);
    }
    ret.setNumber(number);
    if (seen->contains(number)) {
        return generatePhoneNumber(seen);
    }
    seen->append(number);
    return ret;
}

QMap<QString, QString> managerParameters() {
    QMap<QString, QString> params;
    params.insert(QStringLiteral("autoTest"), QStringLiteral("true"));
    params.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("true"));
    return params;
}

}

DeltaSyncAdapter::DeltaSyncAdapter(const QString &accountId, QObject *parent)
    : QObject(parent), TwoWayContactSyncAdapter(QStringLiteral("testsyncadapter"), managerParameters())
    , m_accountId(accountId)
{
    cleanUp(accountId);
}

DeltaSyncAdapter::~DeltaSyncAdapter()
{
    cleanUp(m_accountId);
}

void DeltaSyncAdapter::cleanUp(const QString &accountId)
{
    initSyncAdapter(accountId);
    readSyncStateData(&m_remoteSince[accountId], accountId, TwoWayContactSyncAdapter::ReadPartialState);
    purgeSyncStateData(accountId, true);
}

void DeltaSyncAdapter::addRemoteDuplicates(const QString &accountId, const QString &fname, const QString &lname, const QString &phone)
{
    addRemoteContact(accountId, fname, lname, phone);
    addRemoteContact(accountId, fname, lname, phone);
    addRemoteContact(accountId, fname, lname, phone);
}

void DeltaSyncAdapter::mergeRemoteDuplicates(const QString &accountId)
{
    Q_FOREACH (const QString &dupGuid, m_remoteServerDuplicates[accountId].values()) {
        m_remoteAddMods[accountId].remove(dupGuid); // shouldn't be any here anyway.
        m_remoteDeletions[accountId].append(m_remoteServerContacts[accountId].value(dupGuid));
        m_remoteServerContacts[accountId].remove(dupGuid);
    }
    m_remoteServerDuplicates[accountId].clear();
}

void DeltaSyncAdapter::addRemoteContact(const QString &accountId, const QString &fname, const QString &lname, const QString &phone, DeltaSyncAdapter::PhoneModifiability mod)
{
    QContactName ncn;
    ncn.setFirstName(fname);
    ncn.setLastName(lname);

    QContactPhoneNumber ncp;
    ncp.setNumber(phone);
    if (mod == DeltaSyncAdapter::ExplicitlyModifiable) {
        ncp.setValue(QContactDetail__FieldModifiable, true);
    } else if (mod == DeltaSyncAdapter::ExplicitlyNonModifiable) {
        ncp.setValue(QContactDetail__FieldModifiable, false);
    }

    QContact newContact;
    newContact.saveDetail(&ncn);
    if (phone == QStringLiteral("insert250phones")) {
        QStringList seen;
        for (int i = 0; i < 250; ++i) {
            QContactPhoneNumber p = generatePhoneNumber(&seen);
            newContact.saveDetail(&p);
        }
    } else {
        newContact.saveDetail(&ncp);
    }

    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    if (m_remoteServerContacts[accountId].contains(contactGuidStr)) {
        // this is an intentional duplicate.  we have special handling for duplicates.
        QString duplicateGuidString = contactGuidStr + ":" + QString::number(m_remoteServerDuplicates[accountId].values(contactGuidStr).size() + 1);
        QContactGuid guid; guid.setGuid(duplicateGuidString); newContact.saveDetail(&guid);
        m_remoteServerDuplicates[accountId].insert(contactGuidStr, duplicateGuidString);
        m_remoteServerContacts[accountId].insert(duplicateGuidString, newContact);
        m_remoteAddMods[accountId].insert(duplicateGuidString);
    } else {
        QContactGuid guid; guid.setGuid(contactGuidStr); newContact.saveDetail(&guid);
        m_remoteServerContacts[accountId].insert(contactGuidStr, newContact);
        m_remoteAddMods[accountId].insert(contactGuidStr);
    }
}

void DeltaSyncAdapter::removeRemoteContact(const QString &accountId, const QString &fname, const QString &lname)
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    QContact remContact = m_remoteServerContacts[accountId].value(contactGuidStr);

    // stop tracking the contact if we are currently tracking it.
    m_remoteAddMods[accountId].remove(contactGuidStr);

    // remove it from our remote cache
    m_remoteServerContacts[accountId].remove(contactGuidStr);

    // report the contact as deleted
    m_remoteDeletions[accountId].append(remContact);
}

void DeltaSyncAdapter::setRemoteContact(const QString &accountId, const QString &fname, const QString &lname, const QContact &contact)
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    QContact setContact = contact;

    QContactGuid sguid = setContact.detail<QContactGuid>();
    sguid.setGuid(contactGuidStr);
    setContact.saveDetail(&sguid);

    QContactOriginMetadata somd = setContact.detail<QContactOriginMetadata>();
    somd.setGroupId(setContact.id().toString());
    setContact.saveDetail(&somd);

    m_remoteServerContacts[accountId][contactGuidStr] = setContact;
    m_remoteAddMods[accountId].insert(contactGuidStr);
}

void DeltaSyncAdapter::changeRemoteContactPhone(const QString &accountId,  const QString &fname, const QString &lname, const QString &modPhone)
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    if (!m_remoteServerContacts[accountId].contains(contactGuidStr)) {
        qWarning() << "Contact:" << contactGuidStr << "doesn't exist remotely!";
        return;
    }

    QContact modContact = m_remoteServerContacts[accountId].value(contactGuidStr);
    if (modPhone == QStringLiteral("modify10phones")) {
        QList<QContactPhoneNumber> allPhones = modContact.details<QContactPhoneNumber>();
        if (allPhones.size() < 10) {
            qWarning() << "Modifying 10 phones but have only:" << allPhones.size() << "to modify!  aborting!";
            return;
        }
        int stride = allPhones.size() / 10;
        for (int i = 0; i < allPhones.size(); i+=stride) {
            QContactPhoneNumber mcp = allPhones.at(i);
            mcp.setNumber(mcp.number() + QString::number(i));
            modContact.saveDetail(&mcp);
        }
        QContactPhoneNumber removeOne = allPhones.at(qMax(allPhones.size() - 8, 0));
        modContact.removeDetail(&removeOne);
    } else if (modPhone == QStringLiteral("modifyallphones")) {
        QList<QContactPhoneNumber> allPhones = modContact.details<QContactPhoneNumber>();
        for (int i = 0; i < allPhones.size(); ++i) {
            QContactPhoneNumber mcp = allPhones.at(i);
            mcp.setNumber(mcp.number() + QString::number(i));
            modContact.saveDetail(&mcp);
        }
        QContactPhoneNumber removeOne = allPhones.at(qMax(allPhones.size() - 8, 0));
        modContact.removeDetail(&removeOne);
    } else {
        QContactPhoneNumber mcp = modContact.detail<QContactPhoneNumber>();
        mcp.setNumber(modPhone);
        modContact.saveDetail(&mcp);
    }

    m_remoteServerContacts[accountId][contactGuidStr] = modContact;
    m_remoteAddMods[accountId].insert(contactGuidStr);
}

void DeltaSyncAdapter::changeRemoteContactEmail(const QString &accountId,  const QString &fname, const QString &lname, const QString &modEmail)
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    if (!m_remoteServerContacts[accountId].contains(contactGuidStr)) {
        qWarning() << "Contact:" << contactGuidStr << "doesn't exist remotely!";
        return;
    }

    QContact modContact = m_remoteServerContacts[accountId].value(contactGuidStr);
    QContactEmailAddress mce = modContact.detail<QContactEmailAddress>();
    mce.setEmailAddress(modEmail);
    modContact.saveDetail(&mce);

    m_remoteServerContacts[accountId][contactGuidStr] = modContact;
    m_remoteAddMods[accountId].insert(contactGuidStr);
}

void DeltaSyncAdapter::changeRemoteContactName(const QString &accountId, const QString &fname, const QString &lname, const QString &modfname, const QString &modlname)
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    if (!m_remoteServerContacts[accountId].contains(contactGuidStr)) {
        qWarning() << "Contact:" << contactGuidStr << "doesn't exist remotely!";
        return;
    }

    QContact modContact = m_remoteServerContacts[accountId].value(contactGuidStr);
    QContactName mcn = modContact.detail<QContactName>();
    if (modfname.isEmpty() && modlname.isEmpty()) {
        modContact.removeDetail(&mcn);
    } else {
        mcn.setFirstName(modfname);
        mcn.setLastName(modlname);
        modContact.saveDetail(&mcn);
    }

    const QString modContactGuidStr(TSA_GUID_STRING(accountId, modfname, modlname));
    m_remoteServerContacts[accountId].remove(contactGuidStr);
    m_remoteAddMods[accountId].remove(contactGuidStr);
    m_remoteServerContacts[accountId][modContactGuidStr] = modContact;
    m_remoteAddMods[accountId].insert(modContactGuidStr);
}

void DeltaSyncAdapter::performTwoWaySync(const QString &accountId)
{
    // reset our state.
    m_downsyncWasRequired[accountId] = false;
    m_upsyncWasRequired[accountId] = false;

    // do the sync process as described in twowaycontactsyncadapter.h
    if (!initSyncAdapter(accountId)) {
        qWarning() << Q_FUNC_INFO << "couldn't init adapter";
        emit failed();
        return;
    }

    if (!readSyncStateData(&m_remoteSince[accountId], accountId, TwoWayContactSyncAdapter::ReadPartialState)) {
        qWarning() << Q_FUNC_INFO << "couldn't read sync state data";
        emit failed();
        return;
    }

    determineRemoteChanges(m_remoteSince[accountId], accountId);
    // continued in continueTwoWaySync().
}

void DeltaSyncAdapter::determineRemoteChanges(const QDateTime &, const QString &accountId)
{
    // continuing the sync process as described in twowaycontactsyncadapter.h
    if (m_remoteDeletions[accountId].isEmpty() && m_remoteAddMods[accountId].isEmpty()) {
        m_downsyncWasRequired[accountId] = false;
    } else {
        m_downsyncWasRequired[accountId] = true;
    }

    // call storeRemoteChanges anyway so that the state machine continues to work.
    // alternatively, we could set the state to StoredRemoteChanges manually, and skip
    // this call in the else block above, but we should test that it works properly anyway.
    QList<QContact> remoteAddMods;
    QMap<int, QString> additions;
    foreach (const QString &contactGuidStr, m_remoteAddMods[accountId]) {
        remoteAddMods.append(m_remoteServerContacts[accountId].value(contactGuidStr));
        if (remoteAddMods.last().id().isNull()) {
            additions.insert(remoteAddMods.count() - 1, contactGuidStr);
        }
    }

    if (!storeRemoteChanges(m_remoteDeletions[accountId], &remoteAddMods, accountId)) {
        qWarning() << Q_FUNC_INFO << "couldn't store remote changes";
        emit failed();
        return;
    }

    // Store the ID of any contact we added
    QMap<int, QString>::const_iterator ait = additions.constBegin(), aend = additions.constEnd();
    for ( ; ait != aend; ++ait) {
        const QContact &added(remoteAddMods.at(ait.key()));
        const QString &contactGuidStr(ait.value());
        m_remoteServerContacts[accountId][contactGuidStr].setId(added.id());
    }

    m_modifiedIds[accountId].clear();
    foreach (const QContact &stored, remoteAddMods) {
        m_modifiedIds[accountId].insert(stored.id());
    }

    // clear our simulated remote changes deltas, as we've already reported / stored them.
    m_remoteDeletions[accountId].clear();
    m_remoteAddMods[accountId].clear();

    QList<QContact> locallyAdded, locallyModified, locallyDeleted;
    QDateTime localSince;
    if (!determineLocalChanges(&localSince, &locallyAdded, &locallyModified, &locallyDeleted, accountId)) {
        qWarning() << Q_FUNC_INFO << "couldn't determine local changes";
        emit failed();
        return;
    }

    if (locallyAdded.isEmpty() && locallyModified.isEmpty() && locallyDeleted.isEmpty()) {
        m_upsyncWasRequired[accountId] = false;
    } else {
        m_upsyncWasRequired[accountId] = true;
    }

    upsyncLocalChanges(localSince, locallyAdded, locallyModified, locallyDeleted, accountId);
}

bool DeltaSyncAdapter::testAccountProvenance(const QContact &contact, const QString &accountId)
{
    foreach (const QContact &remoteContact, m_remoteServerContacts[accountId]) {
        if (remoteContact.id() == contact.id()) {
            return true;
        }
    }

    return false;
}

void DeltaSyncAdapter::upsyncLocalChanges(const QDateTime &,
                                         const QList<QContact> &locallyAdded,
                                         const QList<QContact> &locallyModified,
                                         const QList<QContact> &locallyDeleted,
                                         const QString &accountId)
{
    // first, apply the local changes to our in memory store.
    foreach (const QContact &c, locallyAdded) {
        setRemoteContact(accountId, c.detail<QContactName>().firstName(), c.detail<QContactName>().lastName(), c);
    }
    foreach (const QContact &c, locallyModified) {
        // we cannot simply call setRemoteContact since the name might be modified or empty due to a previous test.
        Q_FOREACH (const QString &storedGuid, m_remoteServerContacts[m_accountId].keys()) {
            if (m_remoteServerContacts[m_accountId][storedGuid].id() == c.id()) {
                m_remoteServerContacts[m_accountId][storedGuid] = c;
                m_remoteAddMods[accountId].insert(storedGuid);
            }
        }
    }
    foreach (const QContact &c, locallyDeleted) {
        // we cannot simply call removeRemoteContact since the name might be modified or empty due to a previous test.
        QMap<QString, QContact> remoteServerContacts = m_remoteServerContacts[m_accountId];
        Q_FOREACH (const QString &storedGuid, remoteServerContacts.keys()) {
            if (remoteServerContacts.value(storedGuid).id() == c.id()) {
                m_remoteServerContacts[m_accountId].remove(storedGuid);
            }
        }
    }

    // then finalize the sync
    if (!storeSyncStateData(accountId)) {
        qWarning() << Q_FUNC_INFO << "couldn't store sync state data";
        emit failed();
        return;
    }
    emit finished(); // succeeded.
}

bool DeltaSyncAdapter::upsyncWasRequired(const QString &accountId) const
{
    return m_upsyncWasRequired[accountId];
}

bool DeltaSyncAdapter::downsyncWasRequired(const QString &accountId) const
{
    return m_downsyncWasRequired[accountId];
}

QContact DeltaSyncAdapter::remoteContact(const QString &accountId, const QString &fname, const QString &lname) const
{
    const QString contactGuidStr(TSA_GUID_STRING(accountId, fname, lname));
    return m_remoteServerContacts[accountId].value(contactGuidStr);
}

QSet<QContactId> DeltaSyncAdapter::modifiedIds(const QString &accountId) const
{
    return m_modifiedIds[accountId];
}


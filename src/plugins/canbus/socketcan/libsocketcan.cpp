/****************************************************************************
**
** Copyright (C) 2017 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "libsocketcan.h"

#include <QtCore/qlibrary.h>

#define GENERATE_SYMBOL(returnType, symbolName, ...) \
    typedef returnType (*fp_##symbolName)(__VA_ARGS__); \
    static fp_##symbolName symbolName;

#define RESOLVE_SYMBOL(symbolName) \
    symbolName = reinterpret_cast<fp_##symbolName>(library->resolve(#symbolName)); \
    if (!symbolName) \
        return false;

struct can_bittiming {
    quint32 bitrate;       /* Bit-rate in bits/second */
    quint32 sample_point;  /* Sample point in one-tenth of a percent */
    quint32 tq;            /* Time quanta (TQ) in nanoseconds */
    quint32 prop_seg;      /* Propagation segment in TQs */
    quint32 phase_seg1;    /* Phase buffer segment 1 in TQs */
    quint32 phase_seg2;    /* Phase buffer segment 2 in TQs */
    quint32 sjw;           /* Synchronization jump width in TQs */
    quint32 brp;           /* Bit-rate prescaler */
};

struct can_ctrlmode {
    quint32 mask;
    quint32 flags;
};

#define CAN_CTRLMODE_LOOPBACK       0x01    /* Loopback mode */
#define CAN_CTRLMODE_LISTENONLY     0x02    /* Listen-only mode */
#define CAN_CTRLMODE_3_SAMPLES      0x04    /* Triple sampling mode */
#define CAN_CTRLMODE_ONE_SHOT       0x08    /* One-Shot mode */
#define CAN_CTRLMODE_BERR_REPORTING	0x10    /* Bus-error reporting */
#define CAN_CTRLMODE_FD             0x20    /* CAN FD mode */
#define CAN_CTRLMODE_PRESUME_ACK    0x40    /* Ignore missing CAN ACKs */

GENERATE_SYMBOL(int, can_do_restart, const char * /* name */)
GENERATE_SYMBOL(int, can_set_restart_ms, const char * /* name */, quint32 /* restart_ms */)
GENERATE_SYMBOL(int, can_do_stop, const char * /* name */)
GENERATE_SYMBOL(int, can_do_start, const char * /* name */)
GENERATE_SYMBOL(int, can_set_bitrate, const char * /* name */, quint32 /* bitrate */)
GENERATE_SYMBOL(int, can_get_bittiming, const char * /* name */, struct can_bittiming * /* bt */)
GENERATE_SYMBOL(int, can_set_ctrlmode, const char * /* name */, struct can_ctrlmode * /* cm */)

inline bool resolveSymbols(QLibrary *library)
{
    if (!library->isLoaded()) {
        library->setFileName(QStringLiteral("socketcan"));
        if (!library->load())
            return false;
    }

    RESOLVE_SYMBOL(can_do_restart);
    RESOLVE_SYMBOL(can_set_restart_ms);
    RESOLVE_SYMBOL(can_do_stop);
    RESOLVE_SYMBOL(can_do_start);
    RESOLVE_SYMBOL(can_set_bitrate);
    RESOLVE_SYMBOL(can_get_bittiming);
    RESOLVE_SYMBOL(can_set_ctrlmode);

    return true;
}

LibSocketCan::LibSocketCan(QString *errorString)
{
    QLibrary lib;
    if (Q_UNLIKELY(!resolveSymbols(&lib) && errorString)) {
        *errorString = lib.errorString();
    }
}

/*!
    Brings the CAN \a interface up.

    \internal
    \note Requires appropriate permissions.
*/
bool LibSocketCan::start(const QString &interface)
{
    if (!can_do_start)
        return false;

    return can_do_start(interface.toLatin1().constData()) == 0;
}

/*!
    Brings the CAN \a interface down.

    \internal
    \note Requires appropriate permissions.
*/
bool LibSocketCan::stop(const QString &interface)
{
    if (!can_do_stop)
        return false;

    return can_do_stop(interface.toLatin1().constData()) == 0;
}

/*!
    Performs a CAN controller reset on the CAN \a interface.

    \internal
    \note Reset can only be triggerd if the controller is in bus off
    and the auto restart not turned on.
    \note Requires appropriate permissions.
    \sa setRestartMilliseconds()
 */
bool LibSocketCan::restart(const QString &interface)
{
    if (!can_do_restart)
        return false;

    return can_do_restart(interface.toLatin1().constData()) == 0;
}

/*!
    Sets the timeout for automatic CAN controller reset on the CAN \a interface
    to \a milliseconds.
    \internal
    \note Requires appropriate permissions.
    \sa restart()
 */
bool LibSocketCan::setRestartMilliseconds(const QString &interface, quint32 milliseconds)
{
    if (!can_set_restart_ms)
        return false;

    return can_set_restart_ms(interface.toLatin1().constData(), milliseconds) == 0;
}

/*!
    Sets the CAN \a interface to listen-only mode if \a listenOnly is true.
    \internal
    \note Requires appropriate permissions.
*/
bool LibSocketCan::setListenOnly(const QString &interface, bool listenOnly)
{
    if (!can_set_ctrlmode)
        return false;

    struct can_ctrlmode cm;
    cm.flags = CAN_CTRLMODE_LISTENONLY;
    cm.mask  = listenOnly ? CAN_CTRLMODE_LISTENONLY : 0;
    return can_set_ctrlmode(interface.toLatin1().constData(), &cm);
}

/*!
    Returns the configured bitrate for \a interface.
    \internal
*/
quint32 LibSocketCan::bitrate(const QString &interface) const
{
    if (!can_get_bittiming)
        return 0;

    struct can_bittiming bt;
    if (can_get_bittiming(interface.toLatin1().constData(), &bt) == 0)
        return bt.bitrate;

    return 0;
}

/*!
    Sets the bitrate for the CAN \a interface.

    \internal
    \note Requires appropriate permissions.
 */
void LibSocketCan::setBitrate(const QString &interface, quint32 bitrate)
{
    if (!can_set_bitrate)
        return;

    can_set_bitrate(interface.toLatin1().constData(), bitrate);
}

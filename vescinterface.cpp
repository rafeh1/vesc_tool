﻿/*
    Copyright 2016 - 2019 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "vescinterface.h"
#include <QDebug>
#include <QHostInfo>
#include <QFileInfo>
#include <QThread>
#include <QEventLoop>
#include <utility.h>
#include <cmath>
#include <QRegularExpression>
#include <QDateTime>
#include <QDir>

#ifdef HAS_SERIALPORT
#include <QSerialPortInfo>
#endif

#ifdef HAS_CANBUS
#include <QCanBus>
#endif

#ifndef VT_INTRO_VERSION
#define VT_INTRO_VERSION 1
#endif

VescInterface::VescInterface(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<MCCONF_TEMP>();

    mMcConfig = new ConfigParams(this);
    mAppConfig = new ConfigParams(this);
    mInfoConfig = new ConfigParams(this);
    mPacket = new Packet(this);
    mCommands = new Commands(this);

    // Compatible firmwares
    mFwVersionReceived = false;
    mFwRetries = 0;
    mFwPollCnt = 0;
    mFwTxt = "x.x";
    mIsUploadingFw = false;

    mCancelSwdUpload = false;

    mTimer = new QTimer(this);
    mTimer->setInterval(20);
    mTimer->start();

    mLastConnType = static_cast<conn_t>(mSettings.value("connection_type", CONN_NONE).toInt());
    mLastTcpServer = mSettings.value("tcp_server", "127.0.0.1").toString();
    mLastTcpPort = mSettings.value("tcp_port", 65102).toInt();

    mSendCanBefore = false;
    mCanIdBefore = 0;
    mWasConnected = false;
    mAutoconnectOngoing = false;
    mAutoconnectProgress = 0.0;
    mIgnoreCanChange = false;

    // Serial
#ifdef HAS_SERIALPORT
    mSerialPort = new QSerialPort(this);
    mLastSerialPort = mSettings.value("serial_port", "").toString();
    mLastSerialBaud = mSettings.value("serial_baud", 115200).toInt();

    connect(mSerialPort, SIGNAL(readyRead()),
            this, SLOT(serialDataAvailable()));
    connect(mSerialPort, SIGNAL(error(QSerialPort::SerialPortError)),
            this, SLOT(serialPortError(QSerialPort::SerialPortError)));
#endif

    // CANbus
#ifdef HAS_CANBUS
    mCanDevice = nullptr;
    mLastCanDeviceInterface = mSettings.value("CANbusDeviceInterface", "can0").toString();
    mLastCanDeviceBitrate = mSettings.value("CANbusDeviceBitrate", 500000).toInt();
    mLastCanBackend = mSettings.value("CANbusBackend", "socketcan").toString();
    mLastCanDeviceID = mSettings.value("CANbusLastDeviceID", 01).toInt();
    mCANbusScanning = false;
#endif

    // TCP
    mTcpSocket = new QTcpSocket(this);
    mTcpConnected = false;
    mLastTcpServer = QSettings().value("tcp_server", "").toString();
    mLastTcpPort = QSettings().value("tcp_port", 65102).toInt();

    connect(mTcpSocket, SIGNAL(readyRead()), this, SLOT(tcpInputDataAvailable()));
    connect(mTcpSocket, SIGNAL(connected()), this, SLOT(tcpInputConnected()));
    connect(mTcpSocket, SIGNAL(disconnected()),
            this, SLOT(tcpInputDisconnected()));
    connect(mTcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(tcpInputError(QAbstractSocket::SocketError)));

    // BLE
#ifdef HAS_BLUETOOTH
    mBleUart = new BleUart(this);
    mLastBleAddr = mSettings.value("ble_addr").toString();

    {
        int size = mSettings.beginReadArray("bleNames");
        for (int i = 0; i < size; ++i) {
            mSettings.setArrayIndex(i);
            QString address = mSettings.value("address").toString();
            QString name = mSettings.value("name").toString();
            mBleNames.insert(address, name);
        }
        mSettings.endArray();
    }

    connect(mBleUart, SIGNAL(dataRx(QByteArray)), this, SLOT(bleDataRx(QByteArray)));
    connect(mBleUart, &BleUart::connected, [this]{
        setLastConnectionType(CONN_BLE);
        mSettings.setValue("ble_addr", mLastBleAddr);
    });
#endif

    {
        int size = mSettings.beginReadArray("profiles");
        for (int i = 0; i < size; ++i) {
            mSettings.setArrayIndex(i);
            MCCONF_TEMP cfg;
            cfg.current_min_scale = mSettings.value("current_min_scale", 1.0).toDouble();
            cfg.current_max_scale = mSettings.value("current_max_scale", 1.0).toDouble();
            cfg.erpm_or_speed_min = mSettings.value("erpm_or_speed_min").toDouble();
            cfg.erpm_or_speed_max = mSettings.value("erpm_or_speed_max").toDouble();
            cfg.duty_min = mSettings.value("duty_min").toDouble();
            cfg.duty_max = mSettings.value("duty_max").toDouble();
            cfg.watt_min = mSettings.value("watt_min").toDouble();
            cfg.watt_max = mSettings.value("watt_max").toDouble();
            cfg.name = mSettings.value("name").toString();
            mProfiles.append(QVariant::fromValue(cfg));
        }
        mSettings.endArray();
    }

    {
        int size = mSettings.beginReadArray("pairedUuids");
        for (int i = 0; i < size; ++i) {
            mSettings.setArrayIndex(i);
            QString uuid = mSettings.value("uuid").toString().toUpper();
            mPairedUuids.append(uuid.replace(" ", ""));
        }
        mSettings.endArray();
    }

    mUseImperialUnits = mSettings.value("useImperialUnits", false).toBool();
    mKeepScreenOn = mSettings.value("keepScreenOn", true).toBool();

    mCommands->setAppConfig(mAppConfig);
    mCommands->setMcConfig(mMcConfig);

    // Other signals/slots
    connect(mTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
    connect(mPacket, SIGNAL(dataToSend(QByteArray&)),
            this, SLOT(packetDataToSend(QByteArray&)));
    connect(mPacket, SIGNAL(packetReceived(QByteArray&)),
            this, SLOT(packetReceived(QByteArray&)));
    connect(mCommands, SIGNAL(dataToSend(QByteArray&)),
            this, SLOT(cmdDataToSend(QByteArray&)));
    connect(mCommands, SIGNAL(fwVersionReceived(int,int,QString,QByteArray,bool)),
            this, SLOT(fwVersionReceived(int,int,QString,QByteArray,bool)));
    connect(mCommands, SIGNAL(ackReceived(QString)), this, SLOT(ackReceived(QString)));
    connect(mMcConfig, SIGNAL(updated()), this, SLOT(mcconfUpdated()));
    connect(mAppConfig, SIGNAL(updated()), this, SLOT(appconfUpdated()));

    connect(mCommands, &Commands::valuesReceived, [this](MC_VALUES v) {
        if (mRtLogFile.isOpen()) {
            auto t = QTime::currentTime();
            QTextStream os(&mRtLogFile);
            os << t.msecsSinceStartOfDay() << ";";
            os << v.v_in << ";";
            os << v.temp_mos << ";";
            os << v.temp_mos_1 << ";";
            os << v.temp_mos_2 << ";";
            os << v.temp_mos_3 << ";";
            os << v.temp_motor << ";";
            os << v.current_motor << ";";
            os << v.current_in << ";";
            os << v.id << ";";
            os << v.iq << ";";
            os << v.rpm << ";";
            os << v.duty_now << ";";
            os << v.amp_hours << ";";
            os << v.amp_hours_charged << ";";
            os << v.watt_hours << ";";
            os << v.watt_hours_charged << ";";
            os << v.tachometer << ";";
            os << v.tachometer_abs << ";";
            os << v.position << ";";
            os << v.fault_code << ";";
            os << v.vesc_id << ";";
            os << "\n";
            os.flush();
        }
    });
}

VescInterface::~VescInterface()
{
    storeSettings();
    closeRtLogFile();
}

Commands *VescInterface::commands() const
{
    return mCommands;
}

ConfigParams *VescInterface::mcConfig()
{
    return mMcConfig;
}

ConfigParams *VescInterface::appConfig()
{
    return mAppConfig;
}

ConfigParams *VescInterface::infoConfig()
{
    return mInfoConfig;
}

QStringList VescInterface::getSupportedFirmwares()
{
    QList<QPair<int, int> > fwPairs = getSupportedFirmwarePairs();
    QStringList fws;

    for (int i = 0;i < fwPairs.size();i++) {
        QString tmp;
        tmp.sprintf("%d.%d", fwPairs.at(i).first, fwPairs.at(i).second);
        fws.append(tmp);
    }
    return fws;
}

QList<QPair<int, int> > VescInterface::getSupportedFirmwarePairs()
{
    QList<QPair<int, int> > fws;

    ConfigParam *p = mInfoConfig->getParam("fw_version");

    if (p) {
        QStringList strs = p->enumNames;

        for (int i = 0;i < strs.size();i++) {
            QStringList mami = strs.at(i).split(".");
            QPair<int, int> fw = qMakePair(mami.at(0).toInt(), mami.at(1).toInt());
            fws.append(fw);
        }
    }

    return fws;
}

QString VescInterface::getFirmwareNow()
{
    return mFwTxt;
}

void VescInterface::emitStatusMessage(const QString &msg, bool isGood)
{
    emit statusMessage(msg, isGood);
}

void VescInterface::emitMessageDialog(const QString &title, const QString &msg, bool isGood, bool richText)
{
    emit messageDialog(title, msg, isGood, richText);
}

bool VescInterface::fwRx()
{
    return mFwVersionReceived;
}

void VescInterface::storeSettings()
{
    mSettings.beginWriteArray("bleNames");
    QHashIterator<QString, QString> i(mBleNames);
    int ind = 0;
    while (i.hasNext()) {
        i.next();
        mSettings.setArrayIndex(ind);
        mSettings.setValue("address", i.key());
        mSettings.setValue("name", i.value());
        ind++;
    }
    mSettings.endArray();

    mSettings.beginWriteArray("profiles");
    for (int i = 0; i < mProfiles.size(); ++i) {
        MCCONF_TEMP cfg = mProfiles.value(i).value<MCCONF_TEMP>();
        mSettings.setArrayIndex(i);
        mSettings.setValue("current_min_scale", cfg.current_min_scale);
        mSettings.setValue("current_max_scale", cfg.current_max_scale);
        mSettings.setValue("erpm_or_speed_min", cfg.erpm_or_speed_min);
        mSettings.setValue("erpm_or_speed_max", cfg.erpm_or_speed_max);
        mSettings.setValue("duty_min", cfg.duty_min);
        mSettings.setValue("duty_max", cfg.duty_max);
        mSettings.setValue("watt_min", cfg.watt_min);
        mSettings.setValue("watt_max", cfg.watt_max);
        mSettings.setValue("name", cfg.name);
    }
    mSettings.endArray();

    mSettings.beginWriteArray("pairedUuids");
    for (int i = 0;i < mPairedUuids.size();i++) {
        mSettings.setArrayIndex(i);
        mSettings.setValue("uuid", mPairedUuids.at(i));
    }
    mSettings.endArray();

    mSettings.setValue("useImperialUnits", mUseImperialUnits);
    mSettings.setValue("keepScreenOn", mKeepScreenOn);
}

QVariantList VescInterface::getProfiles()
{
    return mProfiles;
}

void VescInterface::addProfile(QVariant profile)
{
    mProfiles.append(profile);
    emit profilesUpdated();
}

void VescInterface::clearProfiles()
{
    mProfiles.clear();
    emit profilesUpdated();
}

void VescInterface::deleteProfile(int index)
{
    if (index >= 0 && mProfiles.length() > index) {
        mProfiles.removeAt(index);
        emit profilesUpdated();
    }
}

void VescInterface::moveProfileUp(int index)
{
    if (index > 0 && index < mProfiles.size()) {
        mProfiles.swap(index, index - 1);
        emit profilesUpdated();
    }
}

void VescInterface::moveProfileDown(int index)
{
    if (index >= 0 && index < (mProfiles.size() - 1)) {
        mProfiles.swap(index, index + 1);
        emit profilesUpdated();
    }
}

MCCONF_TEMP VescInterface::getProfile(int index)
{
    MCCONF_TEMP conf = createMcconfTemp();

    if (index >= 0 && mProfiles.length() > index) {
        conf = mProfiles.value(index).value<MCCONF_TEMP>();
    }

    return conf;
}

void VescInterface::updateProfile(int index, QVariant profile)
{
    if (index >= 0 && mProfiles.length() > index) {
        mProfiles[index] = profile;
        emit profilesUpdated();
    }
}

bool VescInterface::isProfileInUse(int index)
{
    MCCONF_TEMP conf = getProfile(index);

    bool res = true;

    if (!Utility::almostEqual(conf.current_max_scale,
                              mMcConfig->getParamDouble("l_current_max_scale"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.current_min_scale,
                              mMcConfig->getParamDouble("l_current_min_scale"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.duty_max,
                              mMcConfig->getParamDouble("l_max_duty"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.duty_min,
                              mMcConfig->getParamDouble("l_min_duty"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.watt_max,
                              mMcConfig->getParamDouble("l_watt_max"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.watt_min,
                              mMcConfig->getParamDouble("l_watt_min"), 0.0001)) {
        res = false;
    }

    double speedFact = ((mMcConfig->getParamInt("si_motor_poles") / 2.0) * 60.0 *
            mMcConfig->getParamDouble("si_gear_ratio")) /
            (mMcConfig->getParamDouble("si_wheel_diameter") * M_PI);

    if (!Utility::almostEqual(conf.erpm_or_speed_max * speedFact,
                              mMcConfig->getParamDouble("l_max_erpm"), 0.0001)) {
        res = false;
    }

    if (!Utility::almostEqual(conf.erpm_or_speed_min * speedFact,
                              mMcConfig->getParamDouble("l_min_erpm"), 0.0001)) {
        res = false;
    }

    return res;
}

MCCONF_TEMP VescInterface::createMcconfTemp()
{
    MCCONF_TEMP conf;
    conf.name = "Unnamed Profile";
    conf.current_min_scale = 1.0;
    conf.current_max_scale = 1.0;
    conf.duty_min = 0.05;
    conf.duty_max = 0.95;
    conf.erpm_or_speed_min = -5.0;
    conf.erpm_or_speed_max = 5.0;
    conf.watt_min = -500.0;
    conf.watt_max = 500.0;
    return conf;
}

void VescInterface::updateMcconfFromProfile(MCCONF_TEMP profile)
{
    double speedFact = (((double)mMcConfig->getParamInt("si_motor_poles") / 2.0) * 60.0 *
            mMcConfig->getParamDouble("si_gear_ratio")) /
            (mMcConfig->getParamDouble("si_wheel_diameter") * M_PI);

    mMcConfig->updateParamDouble("l_current_min_scale", profile.current_min_scale);
    mMcConfig->updateParamDouble("l_current_max_scale", profile.current_max_scale);
    mMcConfig->updateParamDouble("l_watt_min", profile.watt_min);
    mMcConfig->updateParamDouble("l_watt_max", profile.watt_max);
    mMcConfig->updateParamDouble("l_min_erpm", profile.erpm_or_speed_min * speedFact);
    mMcConfig->updateParamDouble("l_max_erpm", profile.erpm_or_speed_max * speedFact);
    mMcConfig->updateParamDouble("l_min_duty", profile.duty_min);
    mMcConfig->updateParamDouble("l_max_duty", profile.duty_max);
}

QStringList VescInterface::getPairedUuids()
{
    return mPairedUuids;
}

bool VescInterface::addPairedUuid(QString uuid)
{
    bool res = false;

    uuid = uuid.replace(" ", "").toUpper();

    QRegularExpression hexMatcher("^[0-9A-F]{24}$",
                                  QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = hexMatcher.match(uuid);
    if (!match.hasMatch()) {
        emitMessageDialog("Add VESC",
                          "The UUID must consist of 24 hexadecimal characters.",
                          false, false);
        return false;
    }

    if (hasPairedUuid(uuid)) {
        emitMessageDialog("Add VESC",
                          "This VESC already is in your paired UUID list.",
                          true, false);
    } else {
        mPairedUuids.append(uuid);
        emit pairingListUpdated();
        res = true;
    }

    return res;
}

bool VescInterface::deletePairedUuid(QString uuid)
{
    bool res = false;

    uuid = uuid.replace(" ", "").toUpper();

    for (int i = 0;i < mPairedUuids.size();i++) {
        QString str = mPairedUuids.at(i);
        if (str.replace(" ", "").toUpper() == uuid) {
            mPairedUuids.removeAt(i);
            emit pairingListUpdated();
            res = true;
            break;
        }
    }

    return res;
}

void VescInterface::clearPairedUuids()
{
    mPairedUuids.clear();
    emit pairingListUpdated();
}

bool VescInterface::hasPairedUuid(QString uuid)
{
    bool res = false;

    uuid = uuid.replace(" ", "").toUpper();

    for (int i = 0;i < mPairedUuids.size();i++) {
        QString str = mPairedUuids.at(i);
        if (str.replace(" ", "").toUpper() == uuid) {
            res = true;
            break;
        }
    }

    return res;
}

QString VescInterface::getConnectedUuid()
{
    QString res;

    if (isPortConnected()) {
        res = mUuidStr;
    }

    return res;
}

bool VescInterface::isIntroDone()
{
    if (mSettings.contains("introVersion")) {
        if (mSettings.value("introVersion").toInt() != VT_INTRO_VERSION) {
            mSettings.setValue("intro_done", false);
        }
    } else {
        mSettings.setValue("intro_done", false);
    }

    return mSettings.value("intro_done", false).toBool();
}

void VescInterface::setIntroDone(bool done)
{
    mSettings.setValue("introVersion", VT_INTRO_VERSION);
    mSettings.setValue("intro_done", done);
}

QString VescInterface::getLastTcpServer() const
{
    return mLastTcpServer;
}

int VescInterface::getLastTcpPort() const
{
    return mLastTcpPort;
}

bool VescInterface::swdEraseFlash()
{
    auto waitBmEraseRes = [this]() {
        int res = -10;

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(20000);
        auto conn = connect(mCommands, &Commands::bmEraseFlashAllRes, [&res,&loop](int erRes) {
            res = erRes;
            loop.quit();
        });

        connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
        loop.exec();

        disconnect(conn);
        return res;
    };

    mCommands->bmEraseFlashAll();
    emit fwUploadStatus("Erasing flash...", 0.0, true);
    int erRes = waitBmEraseRes();
    if (erRes != 1) {
        QString msg = "Unknown failure";

        if (erRes == -10) {
            msg = "Erase timed out";
        } else if (erRes == -3) {
            msg = "Erase failed";
        } else if (erRes == -2) {
            msg = "Could not recognize target";
        } else if (erRes == -1) {
            msg = "Not connected to target";
        }

        emitMessageDialog("SWD Upload", msg, false, false);
        emit fwUploadStatus(msg, 0.0, false);

        return false;
    }

    emit fwUploadStatus("Erase done", 0.0, false);

    return true;
}

bool VescInterface::swdUploadFw(QByteArray newFirmware, uint32_t startAddr)
{
    auto waitBmWriteRes = [this]() {
        int res = -10;

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(3000);
        auto conn = connect(mCommands, &Commands::bmWriteFlashRes, [&res,&loop](int wrRes) {
            res = wrRes;
            loop.quit();
        });

        connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
        loop.exec();

        disconnect(conn);
        return res;
    };

    auto writeChunk = [this, &waitBmWriteRes](uint32_t addr, QByteArray chunk) {
        for (int i = 0;i < 3;i++) {
            mCommands->bmWriteFlash(addr, chunk);
            int res = waitBmWriteRes();

            if (res != -10) {
                return res;
            }
        }

        return -20;
    };

    mCancelSwdUpload = false;
    uint32_t addr = startAddr;
    uint32_t szTot = newFirmware.size();
    while (newFirmware.size() > 0) {
        int chunkSize = 256;

        uint32_t sz = newFirmware.size() > chunkSize ? chunkSize : newFirmware.size();
        int res = writeChunk(addr, newFirmware.mid(0, sz));
        newFirmware.remove(0, sz);
        addr += sz;

        if (res == 1) {
            emit fwUploadStatus("Uploading firmware over SWD", (double)(addr - startAddr) / (double)szTot, true);
        } else {
            QString msg = "Unknown failure";

            if (res == -20) {
                msg = "Timed out";
            } else if (res == -2) {
                msg = "Write failed";
            } else if (res == -1) {
                msg = "Not connected to target";
            }

            emitMessageDialog("SWD Upload", msg, false, false);
            emit fwUploadStatus(msg, 0.0, false);

            return false;
        }

        if (mCancelSwdUpload) {
            emit fwUploadStatus("Upload cancelled", 0.0, false);
            return false;
        }
    }

    emit fwUploadStatus("Upload done", 1.0, false);

    return true;
}

void VescInterface::swdCancel()
{
    mCancelSwdUpload = true;
}

bool VescInterface::swdReboot()
{
    auto waitBmReboot = [this]() {
        int res = -10;

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(3000);
        auto conn = connect(mCommands, &Commands::bmRebootRes, [&res,&loop](int wrRes) {
            res = wrRes;
            loop.quit();
        });

        connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
        loop.exec();

        disconnect(conn);
        return res;
    };

    mCommands->bmReboot();
    int res = waitBmReboot();
    if (res == -10) {
        QString msg = "Reboot: Unknown failure";

        if (res == -10) {
            msg = "Reboot: Timed out";
        } else if (res == -1) {
            msg = "Reboot: Not connected to target";
        } else if (res == -2) {
            msg = "Reboot: Flash done failed";
        }

        emitMessageDialog("SWD Upload", msg, false, false);
        emit fwUploadStatus(msg, 1.0, false);

        return false;
    }

    return true;
}

bool VescInterface::openRtLogFile(QString outDirectory)
{
    if (outDirectory.startsWith("file:/")) {
        outDirectory.remove(0, 6);
    }

    if (!QDir(outDirectory).exists()) {
        QDir().mkpath(outDirectory);
    }

    if (!QDir(outDirectory).exists()) {
        emitMessageDialog("Log to file",
                          "Output directory does not exist",
                          false, false);
        return false;
    }

    QDateTime d = QDateTime::currentDateTime();
    mRtLogFile.setFileName(QString("%1/%2-%3-%4_%5:%6:%7.csv").
                           arg(outDirectory).
                           arg(d.date().year(), 2, 10, QChar('0')).
                           arg(d.date().month(), 2, 10, QChar('0')).
                           arg(d.date().day(), 2, 10, QChar('0')).
                           arg(d.time().hour(), 2, 10, QChar('0')).
                           arg(d.time().minute(), 2, 10, QChar('0')).
                           arg(d.time().second(), 2, 10, QChar('0')));

    bool res = mRtLogFile.open(QIODevice::WriteOnly | QIODevice::Text);

    if (!res) {
        emitMessageDialog("Log to file",
                          "Could not open file for writing.",
                          false, false);
    }

    return res;
}

void VescInterface::closeRtLogFile()
{
    if (mRtLogFile.isOpen()) {
        mRtLogFile.close();
    }
}

bool VescInterface::isRtLogOpen()
{
    return mRtLogFile.isOpen();
}

bool VescInterface::useImperialUnits()
{
    return mUseImperialUnits;
}

void VescInterface::setUseImperialUnits(bool useImperialUnits)
{
    mUseImperialUnits = useImperialUnits;
}

bool VescInterface::keepScreenOn()
{
    return mKeepScreenOn;
}

void VescInterface::setKeepScreenOn(bool on)
{
    mKeepScreenOn = on;
}

#ifdef HAS_SERIALPORT
QString VescInterface::getLastSerialPort() const
{
    return mLastSerialPort;
}

int VescInterface::getLastSerialBaud() const
{
    return mLastSerialBaud;
}
#endif

#ifdef HAS_CANBUS
QString VescInterface::getLastCANbusInterface() const
{
    return mLastCanDeviceInterface;
}

int VescInterface::getLastCANbusBitrate() const
{
    return mLastCanDeviceBitrate;
}
#endif

#ifdef HAS_BLUETOOTH
BleUart *VescInterface::bleDevice()
{
    return mBleUart;
}

void VescInterface::storeBleName(QString address, QString name)
{
    mBleNames.insert(address, name);
}

QString VescInterface::getBleName(QString address)
{
    QString res;
    if(mBleNames.contains(address)) {
        res = mBleNames[address];
    }
    return res;
}

QString VescInterface::getLastBleAddr() const
{
    return mLastBleAddr;
}
#endif

bool VescInterface::isPortConnected()
{
    bool res = false;

#ifdef HAS_SERIALPORT
    if (mSerialPort->isOpen()) {
        res = true;
    }
#endif

#ifdef HAS_CANBUS
    if (isCANbusConnected() && !mCANbusScanning) {
        res = true;
    }
#endif

    if (mTcpConnected) {
        res = true;
    }

#ifdef HAS_BLUETOOTH
    if (mBleUart->isConnected()) {
        res = true;
    }
#endif

    return res;
}

void VescInterface::disconnectPort()
{
#ifdef HAS_SERIALPORT
    if(mSerialPort->isOpen()) {
        mSerialPort->close();
        updateFwRx(false);
    }
#endif

#ifdef HAS_CANBUS
    if(isCANbusConnected()) {
        mCanDevice->disconnectDevice();
        delete mCanDevice;
        mCanDevice = nullptr;
    }
#endif

    if (mTcpConnected) {
        mTcpSocket->close();
        updateFwRx(false);
    }

#ifdef HAS_BLUETOOTH
    if (mBleUart->isConnected()) {
        mBleUart->disconnectBle();
        updateFwRx(false);
    }
#endif

    mFwRetries = 0;
}

bool VescInterface::reconnectLastPort()
{
    if (mLastConnType == CONN_SERIAL) {
#ifdef HAS_SERIALPORT
        return connectSerial(mLastSerialPort, mLastSerialBaud);
#else
        return false;
#endif
    } else if (mLastConnType == CONN_TCP) {
        connectTcp(mLastTcpServer, mLastTcpPort);
        return true;
    } else if (mLastConnType == CONN_BLE) {
#ifdef HAS_BLUETOOTH
        mBleUart->startConnect(mLastBleAddr);
#endif
        return true;
    } else if (mLastConnType == CONN_CANBUS) {
#ifdef HAS_CANBUS
        return connectCANbus(mLastCanBackend, mLastCanDeviceInterface, mLastCanDeviceBitrate);
#else
        return false;
#endif
    } else {
#ifdef HAS_SERIALPORT
        QList<VSerialInfo_t> ports = listSerialPorts();
        if (!ports.isEmpty()) {
            return connectSerial(ports.first().systemPath);
        } else  {
            emit messageDialog(tr("Reconnect"), tr("No ports found"), false, false);
            return false;
        }
#else
        emit messageDialog(tr("Reconnect"),
                           tr("Please specify the connection manually "
                              "the first time you are connecting."),
                           false, false);
        return false;
#endif
    }
}

bool VescInterface::autoconnect()
{
    bool res = false;

#ifdef HAS_SERIALPORT
    QList<VSerialInfo_t> ports = listSerialPorts();
    mAutoconnectOngoing = true;
    mAutoconnectProgress = 0.0;

    disconnectPort();
    disconnect(mCommands, SIGNAL(fwVersionReceived(int,int,QString,QByteArray,bool)),
               this, SLOT(fwVersionReceived(int,int,QString,QByteArray,bool)));

    for (int i = 0;i < ports.size();i++) {
        VSerialInfo_t serial = ports[i];

        if (!connectSerial(serial.systemPath)) {
            continue;
        }

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(500);
        connect(mCommands, SIGNAL(fwVersionReceived(int,int,QString,QByteArray,bool)), &loop, SLOT(quit()));
        connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
        loop.exec();

        if (timeoutTimer.isActive()) {
            // If the timer is still running a firmware version was received.
            res = true;
            break;
        } else {
            mAutoconnectProgress = (double)i / (double)ports.size();
            emit autoConnectProgressUpdated(mAutoconnectProgress, false);
            disconnectPort();
        }
    }

    connect(mCommands, SIGNAL(fwVersionReceived(int,int,QString,QByteArray,bool)),
            this, SLOT(fwVersionReceived(int,int,QString,QByteArray,bool)));
#endif

    emit autoConnectProgressUpdated(1.0, true);
    emit autoConnectFinished();
    mAutoconnectOngoing = false;
    return res;
}

QString VescInterface::getConnectedPortName()
{
    QString res = tr("Not connected");
    bool connected = false;

#ifdef HAS_SERIALPORT
    if (mSerialPort->isOpen()) {
        res = tr("Connected (serial) to %1").arg(mSerialPort->portName());
        connected = true;
    }
#endif

#ifdef HAS_CANBUS
    if (isCANbusConnected()) {
        res = tr("Connected (CAN bus) to %1").arg(mLastCanDeviceInterface);
        connected = true;
    }
#endif

    if (mTcpConnected) {
        res = tr("Connected (TCP) to %1:%2").arg(mLastTcpServer).arg(mLastTcpPort);
        connected = true;
    }

#ifdef HAS_BLUETOOTH
    if (mBleUart->isConnected()) {
        res = tr("Connected (BLE) to %1").arg(mLastBleAddr);
        connected = true;
    }
#endif

    if (connected && mCommands->isLimitedMode()) {
        res += tr(", limited mode");
    }

    return res;
}

bool VescInterface::connectSerial(QString port, int baudrate)
{
#ifdef HAS_SERIALPORT
    bool found = false;
    for (VSerialInfo_t ser: listSerialPorts()) {
        if (ser.systemPath == port) {
            found = true;
            break;
        }
    }

    if (!found) {
        emit statusMessage(tr("Invalid serial port: %1").arg(port), false);
        return false;
    }

    if(!mSerialPort->isOpen()) {
        // TODO: Maybe this test works on other OSes as well
#ifdef Q_OS_UNIX
        QFileInfo fi(port);
        if (fi.exists()) {
            if (!fi.isWritable()) {
                emit statusMessage(tr("Serial port is not writable"), false);
                emit serialPortNotWritable(port);
                return false;
            }
        }
#endif

        mSerialPort->setPortName(port);
        mSerialPort->open(QIODevice::ReadWrite);

        if(!mSerialPort->isOpen()) {
            return false;
        }

        mSerialPort->setBaudRate(baudrate);
        mSerialPort->setDataBits(QSerialPort::Data8);
        mSerialPort->setParity(QSerialPort::NoParity);
        mSerialPort->setStopBits(QSerialPort::OneStop);
        mSerialPort->setFlowControl(QSerialPort::NoFlowControl);

        // For nrf
        mSerialPort->setRequestToSend(true);
        mSerialPort->setDataTerminalReady(true);
        QThread::msleep(5);
        mSerialPort->setDataTerminalReady(false);
        QThread::msleep(100);
    }

    mLastSerialPort = port;
    mLastSerialBaud = baudrate;
    mSettings.setValue("serial_port", mLastSerialPort);
    mSettings.setValue("serial_baud", mLastSerialBaud);
    setLastConnectionType(CONN_SERIAL);
    return true;
#else
    (void)port;
    (void)baudrate;
    emit messageDialog(tr("Connect serial"),
                       tr("Serial port support is not enabled in this build "
                          "of VESC Tool."),
                       false, false);
    return false;
#endif
}

QList<VSerialInfo_t> VescInterface::listSerialPorts()
{
    QList<VSerialInfo_t> res;

#ifdef HAS_SERIALPORT
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    foreach(const QSerialPortInfo &port, ports) {
        VSerialInfo_t info;
        info.name = port.portName();
        info.systemPath = port.systemLocation();
        int index = res.size();

        if(port.manufacturer().startsWith("STMicroelectronics")) {
            info.name.insert(0, "VESC - ");
            info.isVesc = true;
            index = 0;
        } else {
            info.isVesc = false;
        }

        res.insert(index, info);
    }
#endif

    return res;
}

QList<QString> VescInterface::listCANbusInterfaces()
{
    QList<QString> res;
#ifdef HAS_CANBUS
#ifdef Q_OS_UNIX
    QFile devicesFile("/proc/net/dev");

    if (devicesFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&devicesFile);
        do {
            QString line = in.readLine();
            for ( int i = 0; i<10; i++) {
                QString interface = QString("can").append(QString::number(i));
                if (line.contains(interface)) {
                    res.append(interface);
                }
            }
        } while (!in.atEnd());

        devicesFile.close();
    }
#endif
#endif
    return res;
}

bool VescInterface::connectCANbus(QString backend, QString interface, int bitrate)
{
#ifdef HAS_CANBUS
    QString errorString;

    mCANbusScanning = false;
    mCanDevice = QCanBus::instance()->createDevice(backend, interface, &errorString);
    if (!mCanDevice) {
        emit statusMessage(tr("Error creating device '%1' using backend '%2', reason: '%3'").arg(mLastCanDeviceInterface).arg(mLastCanBackend).arg(errorString), false);
        return false;
    }

    connect(mCanDevice, SIGNAL(framesReceived()), this, SLOT(CANbusDataAvailable()));
    connect(mCanDevice, SIGNAL(errorOccurred(QCanBusDevice::CanBusError)), this, SLOT(CANbusError(QCanBusDevice::CanBusError)));

    mCanDevice->setConfigurationParameter(QCanBusDevice::LoopbackKey, false);
    mCanDevice->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, false);
    // bitrate change not supported yet by socketcan. It is possible to set the rate when
    // configuring the CAN network interface using the ip link command.
    // mCanDevice->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);
    mCanDevice->setConfigurationParameter(QCanBusDevice::CanFdKey, false);
    mCanDevice->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, false);

    if (!mCanDevice->connectDevice()) {
        emit statusMessage(tr("Connection error: %1").arg(mCanDevice->errorString()), false);

        delete mCanDevice;
        mCanDevice = nullptr;
        return false;
    }

    QThread::msleep(100);

    mLastCanBackend = backend;
    mLastCanDeviceInterface = interface;
    mLastCanDeviceBitrate = bitrate;

    mSettings.setValue("CANbusBackend", mLastCanBackend);
    mSettings.setValue("CANbusDeviceInterface", mLastCanDeviceInterface);
    mSettings.setValue("CANbusDeviceBitrate", mLastCanDeviceBitrate);
    setLastConnectionType(CONN_CANBUS);
    return true;
#else
    (void)backend;
    (void)interface;
    (void)bitrate;
    emit messageDialog(tr("Connect serial"),
                       tr("CAN bus support is not enabled in this build "
                          "of VESC Tool."),
                       false, false);
    return false;
#endif
}

bool VescInterface::isCANbusConnected()
{
#ifdef HAS_CANBUS
    if (mCanDevice != nullptr) {
        if (mCanDevice->state() == QCanBusDevice::ConnectedState) {
            return true;
        }
    }
#endif
    return false;
}

void VescInterface::setCANbusReceiverID(int node_ID)
{
#ifdef HAS_CANBUS
    mLastCanDeviceID = node_ID;
#else
    (void)node_ID;
#endif
}

void VescInterface::connectTcp(QString server, int port)
{
    mLastTcpServer = server;
    mLastTcpPort = port;

    QHostAddress host;
    host.setAddress(server);

    // Try DNS lookup
    if (host.isNull()) {
        QList<QHostAddress> addresses = QHostInfo::fromName(server).addresses();

        if (!addresses.isEmpty()) {
            host.setAddress(addresses.first().toString());
        }
    }

    mTcpSocket->abort();
    mTcpSocket->connectToHost(host, port);
}

void VescInterface::connectBle(QString address)
{
#ifdef HAS_BLUETOOTH
    mBleUart->startConnect(address);
    mLastBleAddr = address;
#else
    (void)address;
#endif
}

bool VescInterface::isAutoconnectOngoing() const
{
    return mAutoconnectOngoing;
}

double VescInterface::getAutoconnectProgress() const
{
    return mAutoconnectProgress;
}

void VescInterface::scanCANbus()
{
#ifdef HAS_CANBUS
    if (!isCANbusConnected()) {
        return;
    }

    mCANbusScanning = true;
    mCanNodesID.clear();

    QCanBusFrame frame;
    frame.setExtendedFrameFormat(true);
    frame.setFrameType(QCanBusFrame::UnknownFrame);
    frame.setFlexibleDataRateFormat(false);
    frame.setBitrateSwitch(false);

    QEventLoop loop;
    QTimer pollTimer;
    pollTimer.start(15);
    unsigned int i = 0;

    auto conn = connect(&pollTimer, &QTimer::timeout,
                        [this, &loop, &frame, &i]() {
        frame.setFrameId(i | uint32_t(CAN_PACKET_PING << 8));
        mCanDevice->writeFrame(frame);
        i++;
        if (i >= 254) {
            loop.quit();
        }
    });

    loop.exec();
    disconnect(conn);
#endif
    return;
}

QVector<int> VescInterface::scanCan()
{
    QVector<int> canDevs;

    if (!isPortConnected()) {
        return canDevs;
    }

    QEventLoop loop;

    bool timeout;
    auto conn = connect(commands(), &Commands::pingCanRx,
                        [&canDevs, &timeout, &loop](QVector<int> devs, bool isTimeout) {
        for (int dev: devs) {
            canDevs.append(dev);
        }
        timeout = isTimeout;
        loop.quit();
    });

    commands()->pingCan();
    loop.exec();

    disconnect(conn);

    if (!timeout) {
        mCanDevsLast = canDevs;
    } else {
        canDevs.clear();
    }

    return canDevs;
}

QVector<int> VescInterface::getCanDevsLast() const
{
    return mCanDevsLast;
}

void VescInterface::ignoreCanChange(bool ignore)
{
    mIgnoreCanChange = ignore;
}

#ifdef HAS_SERIALPORT
void VescInterface::serialDataAvailable()
{
    while (mSerialPort->bytesAvailable() > 0) {
        mPacket->processData(mSerialPort->readAll());
    }
}

void VescInterface::serialPortError(QSerialPort::SerialPortError error)
{
    QString message;
    switch (error) {
    case QSerialPort::NoError:
        break;

    default:
        message = "Serial port error: " + mSerialPort->errorString();
        break;
    }

    if(!message.isEmpty()) {
        emit statusMessage(message, false);

        if (mSerialPort->isOpen()) {
            mSerialPort->close();
        }

       updateFwRx(false);
    }
}
#endif

#ifdef HAS_CANBUS
void VescInterface::CANbusDataAvailable()
{
    QCanBusFrame frame;
    QByteArray payload;
    unsigned short rxbuf_len = 0;
    unsigned short crc;
    char commands_send;

    while (mCanDevice->framesAvailable() > 0) {
        frame = mCanDevice->readFrame();
        if (frame.isValid() && (frame.frameType() == QCanBusFrame::DataFrame)) {
            int packet_type = frame.frameId() >> 8;
            payload = frame.payload();

            switch(packet_type) {
            case CAN_PACKET_PONG:
                mCanNodesID.append(payload[0]);
                emit CANbusNewNode(payload[0]);
                break;

            case CAN_PACKET_PROCESS_SHORT_BUFFER:
                payload.remove(0,2);

                rxbuf_len = payload.size();
                crc = Packet::crc16((const unsigned char*)payload.data(), rxbuf_len);

                // add stop, start, length and crc for the packet decoder
                payload.prepend((unsigned char) rxbuf_len);
                payload.prepend(2);
                payload.append((unsigned char)(crc>>8));
                payload.append((unsigned char)(crc & 0xFF));
                payload.append(3);
                mPacket->processData(payload);
                break;

            case CAN_PACKET_FILL_RX_BUFFER:
                payload.remove(0,1);    // discard index
                mCanRxBuffer.append(payload);
                break;

            case CAN_PACKET_FILL_RX_BUFFER_LONG:
                payload.remove(0,2);    // discard the 2 byte index
                mCanRxBuffer.append(payload);
                break;

            case CAN_PACKET_PROCESS_RX_BUFFER:
                commands_send = payload[1];
                rxbuf_len = (unsigned short)payload[2] << 8 | (unsigned char)payload[3];

                if (rxbuf_len > 512) {
                    return;
                }
                unsigned char len_high = payload[2];
                unsigned char len_low = payload[3];

                unsigned char crc_high = payload[4];
                unsigned char crc_low = payload[5];

                if (Packet::crc16((const unsigned char*)mCanRxBuffer.data(), rxbuf_len) ==
                        ((unsigned short) crc_high << 8 | (unsigned short) crc_low)) {
                    switch (commands_send) {
                        case 0:
                            break;
                        case 1:
                            // add stop, start, length and crc for the packet decoder
                            if (len_high == 0) {
                                mCanRxBuffer.prepend(len_low);
                                mCanRxBuffer.prepend(2);
                            } else {
                                mCanRxBuffer.prepend(len_low);
                                mCanRxBuffer.prepend(len_high);
                                mCanRxBuffer.prepend(3); // size is 16 bit long
                            }

                            mCanRxBuffer.append(crc_high);
                            mCanRxBuffer.append(crc_low);
                            mCanRxBuffer.append(3);
                            mPacket->processData(mCanRxBuffer);
                            break;
                        case 2:
                            //commands_process_packet(rx_buffer, rxbuf_len, 0);
                            break;
                        default:
                            break;
                    }
                }
                mCanRxBuffer.clear();
                break;
            }
        }
    }
}

void VescInterface::CANbusError(QCanBusDevice::CanBusError error)
{
    QString message;
    switch (error) {
    case QCanBusDevice::NoError:
        break;

    default:
        message = "CAN bus error: " + mCanDevice->errorString();
        break;
    }

    if(!message.isEmpty()) {
        emit statusMessage(message, false);
        mCanDevice->disconnectDevice();
        updateFwRx(false);
    }
}
#endif

void VescInterface::tcpInputConnected()
{
    mSettings.setValue("tcp_server", mLastTcpServer);
    mSettings.setValue("tcp_port", mLastTcpPort);
    setLastConnectionType(CONN_TCP);

    mTcpConnected = true;
    updateFwRx(false);
}

void VescInterface::tcpInputDisconnected()
{
    mTcpConnected = false;
    updateFwRx(false);
}

void VescInterface::tcpInputDataAvailable()
{
    while (mTcpSocket->bytesAvailable() > 0) {
        mPacket->processData(mTcpSocket->readAll());
    }
}

void VescInterface::tcpInputError(QAbstractSocket::SocketError socketError)
{
    (void)socketError;

    QString errorStr = mTcpSocket->errorString();
    emit statusMessage(tr("TCP Error") + errorStr, false);
    mTcpSocket->close();
    updateFwRx(false);
}

#ifdef HAS_BLUETOOTH
void VescInterface::bleDataRx(QByteArray data)
{
    mPacket->processData(data);
}
#endif

void VescInterface::timerSlot()
{
    // Poll the serial port as well since readyRead is not emitted recursively. This
    // can be a problem when waiting for input with an additional event loop, such as
    // when using QMessageBox.
#ifdef HAS_SERIALPORT
    serialDataAvailable();
#endif

#ifdef HAS_CANBUS
    if (mCanDeviceInterfaces != listCANbusInterfaces()) {
        mCanDeviceInterfaces = listCANbusInterfaces();
        emit CANbusInterfaceListUpdated();
    }
#endif

    if (!mIgnoreCanChange) {
        if (isPortConnected()) {
            if (mSendCanBefore != mCommands->getSendCan() ||
                    mCanIdBefore != mCommands->getCanSendId()) {
                updateFwRx(false);
                mFwRetries = 0;
            }

            mFwPollCnt++;
            if (mFwPollCnt >= 4) {
                mFwPollCnt = 0;
                if (!mFwVersionReceived) {
                    mCommands->getFwVersion();
                    mFwRetries++;

                    // Timeout if the firmware cannot be read
                    if (mFwRetries >= 25) {
                        emit statusMessage(tr("No firmware read response"), false);
                        emit messageDialog(tr("Read Firmware Version"),
                                           tr("Could not read firmware version. Make sure that "
                                              "the selected port really belongs to the VESC. "),
                                           false, false);
                        disconnectPort();
                    }
                }
            }
        } else {
            updateFwRx(false);
            mFwRetries = 0;
        }
        mSendCanBefore = mCommands->getSendCan();
        mCanIdBefore = mCommands->getCanSendId();
    }

    // Update fw upload bar and label
    double fwProg = mCommands->getFirmwareUploadProgress();
    QString fwStatus = mCommands->getFirmwareUploadStatus();
    if (fwProg > -0.1) {
        mIsUploadingFw = true;
        emit fwUploadStatus(fwStatus, fwProg, true);
    } else {
        // The firmware upload just finished or failed
        if (mIsUploadingFw) {
            mFwRetries = 0;
            if (fwStatus.compare("FW Upload Done") == 0) {
                disconnectPort();
                emit fwUploadStatus(fwStatus, 1.0, false);
                emitMessageDialog("Firmware Upload",
                                  "Firmware upload finished! Give the VESC around 10 "
                                  "seconds to apply the firmware and reboot, then reconnect.",
                                  true, false);
            } else {
                emit fwUploadStatus(fwStatus, 0.0, false);
            }
        }
        mIsUploadingFw = false;
    }

    if (mWasConnected != isPortConnected()) {
        mWasConnected = isPortConnected();
        emit portConnectedChanged();
    }
}

void VescInterface::packetDataToSend(QByteArray &data)
{
#ifdef HAS_SERIALPORT
    if (mSerialPort->isOpen()) {
        mSerialPort->write(data);
    }
#endif

#ifdef HAS_CANBUS
    if (isCANbusConnected()) {
        QCanBusFrame frame;
        frame.setExtendedFrameFormat(true);
        frame.setFrameType(QCanBusFrame::UnknownFrame);
        frame.setFlexibleDataRateFormat(false);
        frame.setBitrateSwitch(false);

        // Remove start byte and length
        if (data[0] == char(2)) {
            data.remove(0, 2);
        } else if (data[0] == char(3)) {
            data.remove(0, 3);
        } else if (data[0] == char(4)) {
            data.remove(0, 4);
        }

        // Remove CRC and stop byte
        data.truncate(data.size() - 3);

        // Since we already are on the CAN-bus, we can send packets that
        // are supposed to be forwarded directly to the correct device.
        int target_id = mLastCanDeviceID;
        if (data.at(0) == COMM_FORWARD_CAN) {
            target_id = uint8_t(data.at(1));
            data.remove(0, 2);
        }

        if (data.size() <= 6) { // Send packet in a single frame
            data.prepend(char(0)); // Process packet at receiver
            data.prepend(char(254)); // VESC Tool sender ID

            frame.setFrameId(uint32_t(target_id) |
                             uint32_t(CAN_PACKET_PROCESS_SHORT_BUFFER << 8));
            frame.setPayload(data);

            mCanDevice->writeFrame(frame);
        } else {
            int len = data.size();
            QByteArray payload;
            int end_a = 0;

            unsigned short crc = Packet::crc16(
                        reinterpret_cast<const unsigned char*>(data.data()),
                        uint32_t(len));

            for (int i = 0;i < len;i += 7) {
                if (i > 255) {
                    break;
                }

                end_a = i + 7;

                payload[0] = char(i);
                payload.append(data.left(7));
                data.remove(0,7);
                frame.setPayload(payload);
                frame.setFrameId(uint32_t(target_id) |
                                 uint32_t(CAN_PACKET_FILL_RX_BUFFER << 8));

                mCanDevice->writeFrame(frame);
                mCanDevice->waitForFramesWritten(5);
                QThread::msleep(5);
                payload.clear();
            }

            for (int i = end_a;i < len;i += 6) {
                payload[0] = char(i >> 8);
                payload[1] = char(i & 0xFF);

                payload.append(data.left(6));
                data.remove(0,6);
                frame.setPayload(payload);
                frame.setFrameId(uint32_t(target_id) |
                                 uint32_t(CAN_PACKET_FILL_RX_BUFFER_LONG << 8));

                mCanDevice->writeFrame(frame);
                mCanDevice->waitForFramesWritten(5);
                QThread::msleep(5);
                payload.clear();
            }

            payload[0] = char(254); // vesc tool node ID
            payload[1] = char(0); // process
            payload[2] = char(len >> 8);
            payload[3] = char(len & 0xFF);
            payload[4] = char(crc >> 8);
            payload[5] = char(crc & 0xFF);
            frame.setPayload(payload);
            frame.setFrameId(uint32_t(target_id) |
                             uint32_t(CAN_PACKET_PROCESS_RX_BUFFER << 8));

            mCanDevice->writeFrame(frame);
        }
    }
#endif

    if (mTcpConnected && mTcpSocket->isOpen()) {
        mTcpSocket->write(data);
    }

#ifdef HAS_BLUETOOTH
    if (mBleUart->isConnected()) {
        mBleUart->writeData(data);
    }
#endif
}

void VescInterface::packetReceived(QByteArray &data)
{
    mCommands->processPacket(data);
}

void VescInterface::cmdDataToSend(QByteArray &data)
{
    mPacket->sendPacket(data);
}

void VescInterface::fwVersionReceived(int major, int minor, QString hw, QByteArray uuid, bool isPaired)
{
    QString uuidStr = Utility::uuid2Str(uuid, true);
    mUuidStr = uuidStr.toUpper();
    mUuidStr.replace(" ", "");

#ifdef HAS_BLUETOOTH
    if (mBleUart->isConnected()) {
        if (isPaired && !hasPairedUuid(mUuidStr)) {
            disconnectPort();
            emitMessageDialog("Pairing",
                              "This VESC is not paired to your local version of VESC Tool. You can either "
                              "add the UUID to the pairing list manually, or connect over USB and set the app "
                              "pairing flag to false for this VESC. Then you can pair to this version of VESC "
                              "tool, or leave the VESC unpaired.",
                              false, false);
            return;
        }
    }
#else
    (void)isPaired;
#endif

    QList<QPair<int, int> > fwPairs = getSupportedFirmwarePairs();

    if (fwPairs.isEmpty()) {
        emit messageDialog(tr("No Supported Firmwares"),
                           tr("This version of VESC Tool does not seem to have any supported "
                              "firmwares. Something is probably wrong with the motor configuration "
                              "file."),
                           false, false);
        updateFwRx(false);
        mFwRetries = 0;
        disconnectPort();
        return;
    }

    QPair<int, int> highest_supported = *std::max_element(fwPairs.begin(), fwPairs.end());
    QPair<int, int> fw_connected = qMakePair(major, minor);

    mCommands->setLimitedSupportsFwdAllCan(fw_connected >= qMakePair(3, 45));

    bool wasReceived = mFwVersionReceived;
    mCommands->setLimitedMode(false);

    if (major < 0) {
        updateFwRx(false);
        mFwRetries = 0;
        disconnectPort();
        emit messageDialog(tr("Error"), tr("The firmware on the connected VESC is too old. Please"
                                           " update it using a programmer."), false, false);
    } else if (fw_connected > highest_supported) {
        mCommands->setLimitedMode(true);
        updateFwRx(true);
        if (!wasReceived) {
            emit messageDialog(tr("Warning"), tr("The connected VESC has newer firmware than this version of"
                                                " VESC Tool supports. It is recommended that you update VESC "
                                                " Tool to the latest version. Alternatively, the firmware on"
                                                " the connected VESC can be downgraded in the firmware page."
                                                " Until then, limited communication mode will be used where"
                                                " only the firmware can be changed."), false, false);
        }
    } else if (!fwPairs.contains(fw_connected)) {
        if (fw_connected >= qMakePair(1, 1)) {
            mCommands->setLimitedMode(true);
            updateFwRx(true);
            if (!wasReceived) {
                emit messageDialog(tr("Warning"), tr("The connected VESC has too old firmware. Since the"
                                                    " connected VESC has firmware with bootloader support, it can be"
                                                    " updated from the Firmware page."
                                                    " Until then, limited communication mode will be used where only the"
                                                    " firmware can be changed."), false, false);
            }
        } else {
            updateFwRx(false);
            mFwRetries = 0;
            disconnectPort();
            if (!wasReceived) {
                emit messageDialog(tr("Error"), tr("The firmware on the connected VESC is too old. Please"
                                                   " update it using a programmer."), false, false);
            }
        }
    } else {
        updateFwRx(true);
        if (fw_connected < highest_supported) {
            if (!wasReceived) {
                emit messageDialog(tr("Warning"), tr("The connected VESC has compatible, but old"
                                                    " firmware. It is recommended that you update it."), false, false);
            }
        }

        QString fwStr;
        fwStr.sprintf("VESC Firmware Version %d.%d", major, minor);
        if (!hw.isEmpty()) {
            fwStr += ", Hardware: " + hw;
        }

        if (!uuidStr.isEmpty()) {
            fwStr += ", UUID: " + uuidStr;
        }

        emit statusMessage(fwStr, true);
    }

    if (major >= 0) {
        mFwTxt.sprintf("Fw: %d.%d", major, minor);
        mHwTxt = hw;
        if (!hw.isEmpty()) {
            mFwTxt += ", Hw: " + hw;
        }

        if (!uuidStr.isEmpty()) {
            mFwTxt += "\n" + uuidStr;
        }
    }
}

void VescInterface::appconfUpdated()
{
    emit statusMessage(tr("App configuration updated"), true);
}

void VescInterface::mcconfUpdated()
{
    emit statusMessage(tr("MC configuration updated"), true);
}

void VescInterface::ackReceived(QString ackType)
{
    emit statusMessage(ackType, true);
}

void VescInterface::updateFwRx(bool fwRx)
{
    bool change = mFwVersionReceived != fwRx;
    mFwVersionReceived = fwRx;
    if (change) {
        emit fwRxChanged(mFwVersionReceived, mCommands->isLimitedMode());
    }
}

void VescInterface::setLastConnectionType(conn_t type)
{
    mLastConnType = type;
    mSettings.setValue("connection_type", type);
}

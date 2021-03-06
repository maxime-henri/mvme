/* mvme - Mesytec VME Data Acquisition
 *
 * Copyright (C) 2016-2020 mesytec GmbH & Co. KG <info@mesytec.com>
 *
 * Author: Florian Lüke <f.lueke@mesytec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "vme_controller_ui.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>

#include <mesytec-mvlc/mvlc_impl_usb.h>

#include "gui_util.h"
#include "mvme_context.h"
#include "qt_util.h"
#include "sis3153.h"
#include "vme_controller_factory.h"
#include "vme_controller_ui_p.h"


//
// VMUSBSettingsWidget
//
VMUSBSettingsWidget::VMUSBSettingsWidget(QWidget *parent)
    : VMEControllerSettingsWidget(parent)
    , m_cb_debugRawBuffers(new QCheckBox)
{
    auto l = new QFormLayout(this);
    l->addRow(QSL("Debug: Write raw buffer file"), m_cb_debugRawBuffers);
}

void VMUSBSettingsWidget::loadSettings(const QVariantMap &settings)
{
    m_cb_debugRawBuffers->setChecked(settings.value("DebugRawBuffers").toBool());
}

QVariantMap VMUSBSettingsWidget::getSettings()
{
    QVariantMap result;
    result["DebugRawBuffers"] = m_cb_debugRawBuffers->isChecked();
    return result;
}

//
// SIS3153EthSettingsWidget
//
SIS3153EthSettingsWidget::SIS3153EthSettingsWidget(QWidget *parent)
    : VMEControllerSettingsWidget(parent)
    , m_le_sisAddress(new QLineEdit)
    , m_cb_jumboFrames(new QCheckBox)
    , m_cb_debugRawBuffers(new QCheckBox)
    , m_cb_disableBuffering(new QCheckBox)
    , m_cb_disableWatchdog(new QCheckBox)
    , m_gb_enableForwarding(new QGroupBox)
    , m_le_forwardingAddress(new QLineEdit)
    , m_spin_forwardingPort(new QSpinBox)
    , m_combo_packetGap(new QComboBox)
{
    {
        using namespace SIS3153Registers::UDPConfigurationValues;

        for (u32 gapValue = 0; gapValue < GapTimeValueCount; gapValue++)
        {
            m_combo_packetGap->addItem(GapTimeValues[gapValue], static_cast<u32>(gapValue));
        }
    }

    auto l = new QFormLayout(this);

    l->addRow(QSL("Hostname / IP Address"), m_le_sisAddress);
    l->addRow(make_framed_description_label(QSL(
                "If using DHCP the requested hostname is <i>sis3153-NNNN</i>, "
                " where N is the serial number in decimal with leading zeroes.<br/>"
                "Example: sis3153-0045 for serial number 45."
                )));

    l->addRow(QSL("Enable UDP Jumbo Frames"), m_cb_jumboFrames);
    l->addRow(make_framed_description_label(QSL(
                "Use ethernet jumbo frames. Note that all intermediate network components"
                " and the receiving network card have to support jumbo frames and have to"
                "be setup correctly for this option to work."
                )));

    l->addRow(QSL("Debug: UDP Packet Gap"), m_combo_packetGap);
    l->addRow(QSL("Debug: Write raw buffer file"), m_cb_debugRawBuffers);
    l->addRow(QSL("Debug: Disable Buffering"), m_cb_disableBuffering);

#if 0
    l->addRow(make_framed_description_label(QSL(
                "Disable buffering to make optimal use of the maximum packet size."
                " This will greatly decrease performance and increase the total number of "
                " packets sent out by the controller."
                )));
#endif

    l->addRow(QSL("Debug: Disable Watchdog"), m_cb_disableWatchdog);

#if 0
    l->addRow(make_framed_description_label(QSL(
                "Do not generate watchdog packets. At low data rates this will result in"
                " receive timeouts being logged during a DAQ run."
                )));
#endif

    m_gb_enableForwarding->setTitle("Enable UDP Forwarding");
    m_gb_enableForwarding->setCheckable(true);
    m_spin_forwardingPort->setMinimum(0);
    m_spin_forwardingPort->setMaximum(std::numeric_limits<u16>::max());

    auto forwardLayout = new QFormLayout(m_gb_enableForwarding);
    forwardLayout->addRow(QSL("Hostname / IP Address"), m_le_forwardingAddress);
    forwardLayout->addRow(QSL("UDP Port"), m_spin_forwardingPort);

    l->addRow(m_gb_enableForwarding);
    //l->addRow(make_framed_description_label(QSL(
    //            "Forward raw datagrams from the controller to the given host:port"
    //            " combination."
    //            )));
}

void SIS3153EthSettingsWidget::validate()
{
    auto addressText = m_le_sisAddress->text();

    if (addressText.isEmpty())
    {
        throw QString(QSL("Hostname / IP Address is empty"));
    }
}

static const QString DefaultHostname("sis3153-0040");
static const u16 DefaultForwardingPort = 42101;

void SIS3153EthSettingsWidget::loadSettings(const QVariantMap &settings)
{
    m_cb_jumboFrames->setChecked(settings["JumboFrames"].toBool());
    m_cb_debugRawBuffers->setChecked(settings.value("DebugRawBuffers").toBool());
    m_cb_disableBuffering->setChecked(settings.value("DisableBuffering").toBool());
    m_cb_disableWatchdog->setChecked(settings.value("DisableWatchdog").toBool());
    m_gb_enableForwarding->setChecked(settings.value("UDP_Forwarding_Enable").toBool());

    // Use the hostname from the setting first.
    auto hostname = settings["hostname"].toString();

    if (hostname.isEmpty())
    {
        // Next check the application wide settings for a previously connected address.
        QSettings appSettings;
        hostname = appSettings.value("VME/LastConnectedSIS3153").toString();
    }

    if (hostname.isEmpty())
    {
        // Still no hostname. Use the default name.
        hostname = DefaultHostname;
    }

    m_le_sisAddress->setText(hostname);

    m_le_forwardingAddress->setText(settings.value("UDP_Forwarding_Address").toString());
    m_spin_forwardingPort->setValue(settings.value("UDP_Forwarding_Port", DefaultForwardingPort).toUInt());
    m_combo_packetGap->setCurrentIndex(settings.value("UDP_PacketGap", 0u).toUInt());
}

QVariantMap SIS3153EthSettingsWidget::getSettings()
{
    QVariantMap result;

    result["hostname"] = m_le_sisAddress->text();
    result["JumboFrames"] = m_cb_jumboFrames->isChecked();
    result["DebugRawBuffers"] = m_cb_debugRawBuffers->isChecked();
    result["DisableBuffering"] = m_cb_disableBuffering->isChecked();
    result["DisableWatchdog"] = m_cb_disableWatchdog->isChecked();
    result["UDP_Forwarding_Enable"] = m_gb_enableForwarding->isChecked();
    result["UDP_Forwarding_Address"] = m_le_forwardingAddress->text();
    result["UDP_Forwarding_Port"] = static_cast<u16>(m_spin_forwardingPort->value());
    result["UDP_PacketGap"] = static_cast<u32>(m_combo_packetGap->currentIndex());

    return result;
}

//
// MVLC_USB_SettingsWidget
//
MVLC_USB_SettingsWidget::MVLC_USB_SettingsWidget(QWidget *parent)
    : VMEControllerSettingsWidget(parent)
    , rb_first(new QRadioButton("First Device"))
    , rb_index(new QRadioButton("By Index"))
    , rb_serial(new QRadioButton("By Serial"))
    , spin_index(new QSpinBox)
    , le_serial(new QLineEdit)
    , pb_listDevices(new QPushButton("List connected devices"))
    , tb_devices(new QTextBrowser)
{
    spin_index->setMinimum(0);
    spin_index->setMaximum(255);
    le_serial->setText("1");

    auto layout = new QVBoxLayout(this);

    // first device
    {
        auto l = make_layout<QHBoxLayout>();
        l->addWidget(rb_first);
        layout->addLayout(l);
    }

    // by index
    {
        auto l = make_layout<QHBoxLayout>();
        l->addWidget(rb_index);
        l->addWidget(spin_index);
        layout->addLayout(l);
    }

    // by serial
    {
        auto l = make_layout<QHBoxLayout>();
        l->addWidget(rb_serial);
        l->addWidget(le_serial);
        layout->addLayout(l);
    }

    layout->addWidget(pb_listDevices);
    layout->addWidget(tb_devices);

    connect(rb_first, &QRadioButton::toggled,
            [this] (bool en)
    {
        if (en)
        {
            spin_index->setEnabled(false);
            le_serial->setEnabled(false);
        }
    });

    connect(rb_index, &QRadioButton::toggled,
            [this] (bool en)
    {
        if (en)
        {
            spin_index->setEnabled(true);
            le_serial->setEnabled(false);
        }
    });

    connect(rb_serial, &QRadioButton::toggled,
            [this] (bool en)
    {
        if (en)
        {
            spin_index->setEnabled(false);
            le_serial->setEnabled(true);
        }
    });

    rb_first->setChecked(true);

    connect(pb_listDevices, &QPushButton::clicked,
            this, &MVLC_USB_SettingsWidget::listDevices);
}

void MVLC_USB_SettingsWidget::listDevices()
{
    using namespace mesytec::mvlc::usb;
    auto allDevices = get_device_info_list(ListOptions::AllDevices);

    QString textBuffer;
    QTextStream ss(&textBuffer);

    for (const auto &devInfo: allDevices)
    {
        ss << QString("[%1] descr='%3', serial='%4'\n")
            .arg(devInfo.index)
            .arg(devInfo.description.c_str())
            .arg(devInfo.serial.c_str());
    }

    tb_devices->setPlainText(textBuffer);
}

void MVLC_USB_SettingsWidget::validate()
{
}

void MVLC_USB_SettingsWidget::loadSettings(const QVariantMap &settings)
{
    auto method = settings["method"].toString();

    if (method == "by_index")
    {
        rb_index->setChecked(true);
        spin_index->setValue(settings["index"].toUInt());
    }
    else if (method == "by_serial")
    {
        rb_serial->setChecked(true);
        le_serial->setText(settings["serial"].toString());
    }
    else
    {
        rb_first->setChecked(true);
    }
}

QVariantMap MVLC_USB_SettingsWidget::getSettings()
{
    QVariantMap result;

    if (rb_index->isChecked())
    {
        result["method"] = "by_index";
        result["index"] = spin_index->value();
    }
    else if (rb_serial->isChecked())
    {
        result["method"] = "by_serial";
        result["serial"] = le_serial->text();
    }
    else
    {
        result["method"] = "first";
    }

    return result;
}

//
// MVLC_ETH_SettingsWidget
//
MVLC_ETH_SettingsWidget::MVLC_ETH_SettingsWidget(QWidget *parent)
    : VMEControllerSettingsWidget(parent)
    , le_address(new QLineEdit)
    , cb_jumboFrames(new QCheckBox)
{
    auto layout = new QFormLayout(this);

    layout->addRow("Hostname / IP Address", le_address);
    layout->addRow(make_framed_description_label(QSL(
                "When using DHCP the MVLC will request a hostname of the form "
                "<i>MVLC-NNNN</i> where NNNN is the serial number.<br/>"
                "This value is  also printed on the MVLCs front panel close to "
                "the ethernet plug."
                )));

    layout->addRow("Enable Jumbo Frames", cb_jumboFrames);
    layout->addRow(make_framed_description_label(QSL(
                "Enable ethernet jumbo frames on the MVLC data pipe.<br/>"
                "Note that all intermediate network components and the receiving network card "
                "have to support jumbo frames and have to be setup correctly for this option to work."
                )));
}

void MVLC_ETH_SettingsWidget::validate()
{
}

void MVLC_ETH_SettingsWidget::loadSettings(const QVariantMap &settings)
{
    auto hostname = settings["mvlc_hostname"].toString();
    if (hostname.isEmpty())
        hostname = "MVLC-0001";
    le_address->setText(hostname);

    cb_jumboFrames->setChecked(settings["mvlc_eth_enable_jumbos"].toBool());
}

QVariantMap MVLC_ETH_SettingsWidget::getSettings()
{
    QVariantMap result;

    result["mvlc_hostname"] = le_address->text();
    result["mvlc_eth_enable_jumbos"] = cb_jumboFrames->isChecked();

    return result;
}

//
// VMEControllerSettingsDialog
//

namespace
{
    struct LabelAndType
    {
        const char *label;
        VMEControllerType type;
    };

    static const QVector<LabelAndType> LabelsAndTypes =
    {
        { "MVLC_USB",       VMEControllerType::MVLC_USB },
        { "MVLC_ETH",       VMEControllerType::MVLC_ETH },
        { "VM-USB",         VMEControllerType::VMUSB },
        { "SIS3153",        VMEControllerType::SIS3153 }
    };
}

VMEControllerSettingsDialog::VMEControllerSettingsDialog(MVMEContext *context, QWidget *parent)
    : QDialog(parent)
    , m_context(context)
    , m_buttonBox(new QDialogButtonBox)
    , m_comboType(new QComboBox)
    , m_controllerStack(new QStackedWidget)
{
    setWindowTitle(QSL("VME Controller Selection"));

    auto widgetLayout = new QVBoxLayout(this);

    // add type combo groupbox
    {
        auto gb = new QGroupBox(QSL("Controller Type"));
        auto l  = new QHBoxLayout(gb);
        l->addWidget(m_comboType);
        widgetLayout->addWidget(gb);
    }

    auto currentControllerType = m_context->getVMEConfig()->getControllerType();
    s32 currentControllerIndex = 0;

    // fill combo and add settings widgets
    for (s32 i = 0; i< LabelsAndTypes.size(); ++i)
    {
        auto lt = LabelsAndTypes[i];
        m_comboType->addItem(lt.label, static_cast<s32>(lt.type));
        VMEControllerFactory f(lt.type);
        auto settingsWidget = f.makeSettingsWidget();

        settingsWidget->loadSettings(m_context->getVMEConfig()->getControllerSettings());
        if (lt.type == currentControllerType)
        {
            currentControllerIndex = i;
        }
        //else
        //{
        //    settingsWidget->loadSettings(QVariantMap());
        //}

        auto gb = new QGroupBox(QSL("Controller Settings"));
        auto l  = new QHBoxLayout(gb);
        l->setContentsMargins(2, 2, 2, 2);
        l->setSpacing(2);
        l->addWidget(settingsWidget);
        m_controllerStack->addWidget(gb);
        m_settingsWidgets.push_back(settingsWidget);
    }

    // controller config stack
    widgetLayout->addWidget(m_controllerStack);

    // buttonbox
    m_buttonBox->setStandardButtons(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    connect(m_buttonBox, &QDialogButtonBox::clicked,
            this, &VMEControllerSettingsDialog::onButtonBoxClicked);

    widgetLayout->addWidget(m_buttonBox);

    // setup
    connect(m_comboType, static_cast<void (QComboBox::*) (int)>(&QComboBox::currentIndexChanged),
            m_controllerStack, &QStackedWidget::setCurrentIndex);

    m_comboType->setCurrentIndex(currentControllerIndex);
}

void VMEControllerSettingsDialog::onButtonBoxClicked(QAbstractButton *button)
{
    auto buttonRole = m_buttonBox->buttonRole(button);

    if (buttonRole == QDialogButtonBox::RejectRole)
    {
        reject();
        return;
    }

    Q_ASSERT(buttonRole == QDialogButtonBox::AcceptRole || buttonRole == QDialogButtonBox::ApplyRole);

    // change controller type here
    // delete old controller
    // set new controller
    auto selectedType = static_cast<VMEControllerType>(m_comboType->currentData().toInt());

    auto settingsWidget = qobject_cast<VMEControllerSettingsWidget *>(
        m_settingsWidgets.value(m_comboType->currentIndex()));
    Q_ASSERT(settingsWidget);

    try
    {
        settingsWidget->validate();
    }
    catch (const QString &e)
    {
        QMessageBox::critical(this, QSL("Invalid Settings"),
                              QString("Settings validation failed: %1").arg(e));
        return;
    }

    auto settings = settingsWidget->getSettings();
    VMEControllerFactory f(selectedType);
    auto controller = f.makeController(settings);
    qDebug() << "before m_context->setVMEController()";
    m_context->setVMEController(controller, settings);
    qDebug() << "after m_context->setVMEController()";

    if (buttonRole == QDialogButtonBox::AcceptRole)
    {
        accept();
    }
}

#include "mvlc/mvlc_vme_debug_widget.h"
#include "ui_vme_debug_widget.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>

#include "mvlc/mvlc_usb.h"
#include "mvlc/mvlc_util.h"
#include "vme_script.h"

namespace
{

struct WriterUiElements
{
    QLineEdit *le_offset,
              *le_address,
              *le_value;

    QPushButton *pb_loop,
                *pb_write;
};

struct ReaderUiElements
{
    QLineEdit *le_offset,
              *le_address;

    QPushButton *pb_loop,
                *pb_read;

    QLabel *l_result_hex,
           *l_result_dec;

    // optional elements used only for the first reader

    QRadioButton *rb_single,
                 *rb_blt,
                 *rb_mblt;

    QSpinBox *spin_blockReadCount;
};

} // end anon namespace

static const int TabStop = 4;
static const QString Key_LastScriptDirectory = QSL("Files/LastDebugScriptDirectory");
static const int LoopInterval = 1000;

namespace mesytec
{
namespace mvlc
{

using usb::MVLCDialog;

struct VMEDebugWidget::Private
{
    MVLCObject *mvlc;

    QVector<WriterUiElements> ui_writers;
    QVector<ReaderUiElements> ui_readers;
};

VMEDebugWidget::VMEDebugWidget(MVLCObject *mvlc, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
    , ui(std::make_unique<Ui::VMEDebugWidget>())
{
    d->mvlc = mvlc;
    ui->setupUi(this);
    ui->gb_scriptOutput->setVisible(false);

    new vme_script::SyntaxHighlighter(ui->scriptInput);
    {
        QString spaces;
        for (int i = 0; i < TabStop; ++i)
            spaces += " ";
        QFontMetrics metrics(ui->scriptInput->font());
        ui->scriptInput->setTabStopWidth(metrics.width(spaces));
    }

    // Writer1
    {
        WriterUiElements w = {};
        w.le_offset = ui->writeOffset1;
        w.le_address = ui->writeAddress1;
        w.le_value = ui->writeValue1;
        w.pb_loop = ui->writeLoop1;
        w.pb_write = ui->writeWrite1;
        d->ui_writers.push_back(w);
    }

    // Writer2
    {
        WriterUiElements w = {};
        w.le_offset = ui->writeOffset2;
        w.le_address = ui->writeAddress2;
        w.le_value = ui->writeValue2;
        w.pb_loop = ui->writeLoop2;
        w.pb_write = ui->writeWrite2;
        d->ui_writers.push_back(w);
    }

    // Writer3
    {
        WriterUiElements w = {};
        w.le_offset = ui->writeOffset3;
        w.le_address = ui->writeAddress3;
        w.le_value = ui->writeValue3;
        w.pb_loop = ui->writeLoop3;
        w.pb_write = ui->writeWrite3;
        d->ui_writers.push_back(w);
    }

    // Reader1 with block reads
    {
        ReaderUiElements r = {};
        r.le_offset = ui->readOffset1;
        r.le_address = ui->readAddress1;
        r.pb_loop = ui->readLoop1;
        r.pb_read = ui->readRead1;
        r.l_result_hex = ui->readResult1_hex;
        r.l_result_dec = ui->readResult1_dec;

        r.rb_single = ui->readModeSingle;
        r.rb_blt = ui->readModeBLT;
        r.rb_mblt = ui->readModeMBLT;
        r.spin_blockReadCount = ui->blockReadCount;
        d->ui_readers.push_back(r);
    }

    // Reader2
    {
        ReaderUiElements r = {};
        r.le_offset = ui->readOffset2;
        r.le_address = ui->readAddress2;
        r.pb_loop = ui->readLoop2;
        r.pb_read = ui->readRead2;
        r.l_result_hex = ui->readResult2_hex;
        r.l_result_dec = ui->readResult2_dec;
        d->ui_readers.push_back(r);
    }

    // Reader3
    {
        ReaderUiElements r = {};
        r.le_offset = ui->readOffset3;
        r.le_address = ui->readAddress3;
        r.pb_loop = ui->readLoop3;
        r.pb_read = ui->readRead3;
        r.l_result_hex = ui->readResult3_hex;
        r.l_result_dec = ui->readResult3_dec;
        d->ui_readers.push_back(r);
    }

    // Writer actions
    for (int writerIndex = 0;
         writerIndex < d->ui_writers.size();
         writerIndex++)
    {
        auto &writer = d->ui_writers[writerIndex];
        connect(writer.pb_loop, &QPushButton::toggled,
                this, [this, writerIndex] (bool checked)
        {
            slt_writeLoop_toggled(writerIndex, checked);
        });

        connect(writer.pb_write, &QPushButton::clicked,
                this, [this, writerIndex] ()
        {
            slt_doWrite_clicked(writerIndex);
        });
    }

    // Reader actions
    for (int readerIndex = 0;
         readerIndex < d->ui_readers.size();
         readerIndex++)
    {
        auto &reader = d->ui_readers[readerIndex];
        connect(reader.pb_loop, &QPushButton::toggled,
                this, [this, readerIndex] (bool checked)
        {
            slt_readLoop_toggled(readerIndex, checked);
        });

        connect(reader.pb_read, &QPushButton::clicked,
                this, [this, readerIndex] ()
        {
            slt_doRead_clicked(readerIndex);
        });
    }

    // Script actions
    connect(ui->runScript, &QPushButton::clicked,
            this, &VMEDebugWidget::slt_runScript);

    connect(ui->loadScript, &QPushButton::clicked,
            this, &VMEDebugWidget::slt_loadScript);

    connect(ui->saveScript, &QPushButton::clicked,
            this, &VMEDebugWidget::slt_saveScript);
}

VMEDebugWidget::~VMEDebugWidget()
{
}

void VMEDebugWidget::slt_writeLoop_toggled(int writerIndex, bool checked)
{
    for (int i = 0; i < d->ui_writers.size(); i++)
    {
        auto &writer = d->ui_writers[i];
        writer.pb_loop->setEnabled(i != writerIndex ? !checked : true);
        writer.pb_write->setEnabled(!checked);
    }

    for (auto &reader: d->ui_readers)
    {
        reader.pb_loop->setEnabled(!checked);
        reader.pb_read->setEnabled(!checked);
    }

    if (checked)
        slt_doWrite_clicked(writerIndex);
}

void VMEDebugWidget::slt_doWrite_clicked(int writerIndex)
{
    if (0 > writerIndex || writerIndex >= d->ui_writers.size())
        return;

    auto &writer = d->ui_writers.at(writerIndex);

    bool ok = true;
    u32 offset = writer.le_offset->text().toUInt(&ok, 0);
    u32 address = writer.le_address->text().toUInt(&ok, 0);
    u16 value = writer.le_value->text().toUInt(&ok, 0);
    address += (offset << 16);

    doWrite(address, value);

    if (writer.pb_loop->isChecked())
    {
        QTimer::singleShot(LoopInterval,
                           this, [this, writerIndex]
        {
            // enqueue the next write
            slt_doWrite_clicked(writerIndex);
        });
    }
}

void VMEDebugWidget::slt_readLoop_toggled(int readerIndex, bool checked)
{
    for (int i = 0; i < d->ui_readers.size(); i++)
    {
        auto &reader = d->ui_readers[i];
        reader.pb_loop->setEnabled(i != readerIndex ? !checked : true);
        reader.pb_read->setEnabled(!checked);
    }

    for (auto &writer: d->ui_writers)
    {
        writer.pb_loop->setEnabled(!checked);
        writer.pb_write->setEnabled(!checked);
    }

    if (checked)
        slt_doRead_clicked(readerIndex);
}

void VMEDebugWidget::slt_doRead_clicked(int readerIndex)
{
    if (0 > readerIndex || readerIndex >= d->ui_readers.size())
        return;

    auto &reader = d->ui_readers.at(readerIndex);

    ui->bltResult->clear();
    reader.l_result_hex->clear();
    reader.l_result_dec->clear();

    bool ok = true;
    u32 offset = reader.le_offset->text().toUInt(&ok, 0);
    u32 address = reader.le_address->text().toUInt(&ok, 0);
    address += (offset << 16);

    if (!(reader.rb_single && reader.rb_blt && reader.rb_mblt)
        || reader.rb_single->isChecked())
    {
        u16 value = doSingleRead(address);

        reader.l_result_hex->setText(QString("0x%1")
                                     .arg(value, 4, 16, QChar('0')));
        reader.l_result_dec->setText(QString("%1")
                                 .arg(value, 0, 10, QChar(' ')));
        return;
    }

    // block read

    AddressMode amod = reader.rb_blt->isChecked() ? AddressMode::BLT32 : AddressMode::MBLT64;
    u16 transfers = static_cast<u16>(reader.spin_blockReadCount->value());
    QVector<u32> buffer;
    buffer.reserve(transfers);

    MVLCDialog dlg(d->mvlc->getImpl());
    auto error = dlg.vmeBlockRead(address, amod, transfers, buffer);

    if (!error)
    {
        emit sigLogMessage(QString("VMEDebug - error from block read: ") + error.toString());
    }

    QStringList lines;
    lines.reserve(buffer.size());

    for (int i=0; i<buffer.size(); ++i)
    {
        lines << QString(QSL("%1: 0x%2"))
            .arg(i, 2, 10, QChar(' '))
            .arg(buffer[i], 8, 16, QChar('0'));
    }

    ui->bltResult->setText(lines.join("\n"));

    if (reader.pb_loop->isChecked())
    {
        QTimer::singleShot(LoopInterval,
                           this, [this, readerIndex]
        {
            // enqueue the next write
            slt_doRead_clicked(readerIndex);
        });
    }
}

void VMEDebugWidget::doWrite(u32 address, u16 value)
{
    MVLCDialog dlg(d->mvlc->getImpl());
    auto error = dlg.vmeSingleWrite(address, value,
                                    AddressMode::A32, VMEDataWidth::D16);

    if (!error)
    {
        emit sigLogMessage(QString("VMEDebug - error from write: ") + error.toString());
    }
}


u16 VMEDebugWidget::doSingleRead(u32 address)
{
    MVLCDialog dlg(d->mvlc->getImpl());
    u32 value = 0u;

    auto error = dlg.vmeSingleRead(address, value,
                                   AddressMode::A32, VMEDataWidth::D16);

    if (!error)
    {
        emit sigLogMessage(QString("VMEDebug - error from read: ") + error.toString());
    }

    return static_cast<u16>(value);
}

void VMEDebugWidget::slt_saveScript()
{
    QString path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).at(0);
    QSettings settings;
    if (settings.contains(Key_LastScriptDirectory))
    {
        path = settings.value(Key_LastScriptDirectory).toString();
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, QSL("Save vme script"), path,
        QSL("VME scripts (*.vmescript *.vme);; All Files (*)"));

    if (fileName.isEmpty())
        return;

    QFileInfo fi(fileName);
    if (fi.completeSuffix().isEmpty())
    {
        fileName += ".vmescript";
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, "File error",
                              QString("Error opening \"%1\" for writing").arg(fileName));
        return;
    }

    QTextStream stream(&file);
    stream << ui->scriptInput->toPlainText();

    if (stream.status() != QTextStream::Ok)
    {
        QMessageBox::critical(this, "File error",
                              QString("Error writing to \"%1\"").arg(fileName));
        return;
    }

    settings.setValue(Key_LastScriptDirectory, fi.absolutePath());
}

void VMEDebugWidget::slt_loadScript()
{
    QString path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).at(0);
    QSettings settings;
    if (settings.contains(Key_LastScriptDirectory))
    {
        path = settings.value(Key_LastScriptDirectory).toString();
    }

    QString fileName = QFileDialog::getOpenFileName(
        this, QSL("Load vme script file"), path,
        QSL("VME scripts (*.vmescript *.vme);; All Files (*)"));

    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);
            ui->scriptInput->setPlainText(stream.readAll());
            QFileInfo fi(fileName);
            settings.setValue(Key_LastScriptDirectory, fi.absolutePath());
            ui->scriptOutput->clear();
        }
    }
}

// TODO: move into mvlc_util or mvlc_vme_script_bridge or something like that
vme_script::Result run_command(MVLCObject *mvlc, const vme_script::Command &cmd,
                               std::function<void (const QString &)> logger)
{
    using namespace vme_script;
    Result result;
    result.command = cmd;

    try
    {
        auto uploadData = QVector<u32>::fromStdVector(mvlc::build_upload_command_buffer(
                    { cmd }, mvlc::CommandPipe, mvlc::stacks::StackMemoryBegin));

        log_buffer(uploadData, "run_command upload data");

        usb::MVLCError error = {};

        // -> stack upload
        error = usb::write_buffer(&mvlc->getImpl(), uploadData);
        if (!error) throw error;

        QVector<u32> mirrorBuffer;
        mirrorBuffer.reserve(uploadData.size());
        // <- stack upload mirror response
        error = usb::read_response(&mvlc->getImpl(), mirrorBuffer);
        if (!error) throw error;

        // stack mirror check
        error = usb::check_mirror(uploadData, mirrorBuffer);
        if (!error) throw error;

        // exec stack
        MVLCDialog dlg(mvlc->getImpl());
        error = dlg.writeRegister(stacks::Stack0OffsetRegister, 0x0);
        if (!error) throw error;

        error = dlg.writeRegister(stacks::Stack0TriggerRegister, 1u << stacks::ImmediateShift);
        if (!error) throw error;

        // <- stack response
        error = usb::read_response(&mvlc->getImpl(), result.valueVector);
        if (!error) throw error;
    }
    catch (const usb::MVLCError &e)
    {
        qDebug() << __PRETTY_FUNCTION__ << "caught MVLCError:" << e.toString();
        result.error = VMEError { VMEError::UnknownError, e.toString() };
        assert(result.error.isError());
    }

    return result;
}

// TODO: move into mvlc_util or mvlc_vme_script_bridge or something like that
vme_script::ResultList run_script(MVLCObject *mvlc, const vme_script::VMEScript &script,
                                  std::function<void (const QString &)> logger)
{
    using namespace vme_script;

    ResultList results;
    results.reserve(script.size());

    for (auto &cmd: script)
    {
        if (cmd.type != CommandType::Invalid)
        {
            if (!cmd.warning.isEmpty())
            {
                logger(QString("Warning: %1 on line %2 (cmd=%3)")
                       .arg(cmd.warning)
                       .arg(cmd.lineNumber)
                       .arg(to_string(cmd.type))
                      );
            }

            auto result = run_command(mvlc, cmd, logger);
            results.push_back(result);

            //if (logEachResult)
            //    logger(format_result(result));
        }
    }

    return results;
}

void VMEDebugWidget::slt_runScript()
{
    //ui->scriptOutput->clear(); // This is hidden in the MVLC version

    //auto logger = std::bind(&QTextEdit::append, ui->scriptOutput, _1);
    //auto logger = std::bind(&MVMEContext::logMessage, m_context, _1);
    auto logger = [this] (const QString &msg)
    {
        emit sigLogMessage(msg);
    };

    try
    {
        bool ok;
        auto baseAddress = ui->scriptOffset->text().toUInt(&ok, 0);
        baseAddress <<= 16;

        auto script = vme_script::parse(ui->scriptInput->toPlainText(), baseAddress);
        auto resultList = run_script(d->mvlc, script, logger);

        for (auto result: resultList)
        {
            if (result.error.isError())
            {
                QString str = format_result(result);
                logger(str);
            }
            else
            {
                // FIXME (maybe)
                // Hack for because the run_script of the mvlc does not set
                // vme_script::Result.value but instead copies all data to
                // vme_script::Result.valueVector

                QString str(to_string(result.command) + " ->\n");
                for (int i=0; i<result.valueVector.size(); ++i)
                {
                    str += QString(QSL("%1: 0x%2\n"))
                        .arg(i, 4, 10, QChar(' '))
                        .arg(result.valueVector[i], 8, 16, QChar('0'));
                }
                logger(str);
            }
        }
    }
    catch (const vme_script::ParseError &e)
    {
        logger(QSL("Parse error: ") + e.what());
    }
}

} // end namespace mvlc
} // end namespace mesytec
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include <QTime>
#include <QIntValidator>
#include <QDebug>
#include <QRegularExpression>

Options init_options() {
    Options opt;
    opt.dpi = 900;

    int cnt = QThread::idealThreadCount();
    if (cnt > 1) cnt--;
    opt.threads = cnt;

    opt.pagesPerDict = 10;
    opt.classifier = 3;
    opt.agression = 100;
    opt.lossy = opt.clean = opt.match =
            opt.protos = opt.averaging = opt.erosion = true;
    opt.report = true;
    opt.warnings = opt.verbose = false;
    opt.indirect = false;
    opt.unbuffered = true;
    return opt;
}

Settings init_settings()
{
    Settings set;
#ifdef __linux
    set.path2Bin = "/usr/bin/minidjvu-mod";
#else
    set.path2Bin = "bin/minidjvu-mod";
#endif
    return set;
}

QString add_opt(const QChar& c, QString val, const QSet<QChar>& supported, bool condition = true)
{
    if (supported.contains(c) && condition) {
        return QString(" -%1 %2").arg(c).arg(val);
    } else {
        return QString();
    }
}

QString opt2cmd(const Options& opt, const QSet<QChar>& supported)
{
    QString res;
    res += add_opt('d', QString::number(opt.dpi), supported);
    res += add_opt('p', QString::number(opt.pagesPerDict), supported);
    res += add_opt('t', QString::number(opt.threads), supported);
    res += add_opt('C', QString::number(opt.classifier), supported);
    res += add_opt('i', "", supported, opt.indirect);
    res += add_opt('X', opt.ext, supported, !opt.ext.isEmpty());
    res += add_opt('l', "", supported, opt.lossy);
    res += add_opt('c', "", supported, !opt.lossy && opt.clean);
    res += add_opt('m', "", supported, !opt.lossy && opt.match);
    res += add_opt('s', "", supported, !opt.lossy && opt.smooth);
    res += add_opt('e', "", supported, !opt.lossy && opt.erosion);
    res += add_opt('n', "", supported, !opt.lossy && !opt.protos);
    res += add_opt('A', "", supported, !opt.lossy && opt.protos && opt.averaging);

    res += add_opt('a', QString::number(opt.agression), supported, opt.lossy || opt.match);
    res += add_opt('r', "", supported, opt.report);
    res += add_opt('w', "", supported, opt.warnings);
    res += add_opt('v', "", supported, opt.verbose);
    res += add_opt('u', "", supported, opt.unbuffered);

    return res;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_proc(this)
{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);
    m_ptrPreviousTab = ui->tabWidget->widget(0);

    QIntValidator * validator = new QIntValidator(5, 10000, this);
    ui->edDPI->setValidator(validator);
    ui->edPagesPerDict->setValidator(validator);
    ui->edMaxThreads->setValidator(validator);

    m_set = init_settings();
    m_opt = init_options();
    displaySettings();
    filterSupportedOpts();
    displayOpts();

    connect(&m_proc, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        addToLog(tr("Process error: %1").arg(error));

        ui->btnConvert->disconnect();
        displayStartProcessMode();
    });


    connect(&m_proc, &QProcess::started, this, [=]() {
        m_timer.start();
        addToLog(tr("Process started"));

        ui->btnConvert->disconnect();
        displayStopProcessMode();
    });

    connect(&m_proc, &QProcess::readyRead, this, [=]() {
        QString val(m_proc.readAll());
        addToLog(val);

        const QStringList sl = val.split("\n", Qt::SkipEmptyParts);
        for (const QString& s: sl) {
            if (s.startsWith(tr("Saving")) && s.endsWith(tr("completed"))) {
                ui->progressBar->setValue(ui->progressBar->value()+1);
            }
        }
    });

    connect(&m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [=](int exitCode,  QProcess::ExitStatus /*exitStatus*/) {
        QTime t(0,0);
        t = t.addMSecs(m_timer.elapsed());
        addToLog(tr("Process finished with exit code: %1").arg(exitCode));

        QString time = t.toString("ss.zzz") + tr(" sec.");
        if (t.minute()) time += t.toString("mm") + tr(" min. ");
        if (t.hour()) time += t.toString("HH") + tr(" h. ");
        addToLog(tr("Elapsed time: %1").arg(time));

        QFileInfo fi(m_proc.arguments().constLast());
        if (fi.exists()) {
            QString size;
            const double size_b = fi.size();
            const double size_k = size_b / 1024.;
            const double size_m = size_k / 1024.;

            if (size_m > 1) {
                size = QString::number(size_m, 'f', 2) + "MiB";
            } else if (size_k > 1) {
                size = QString::number(size_k, 'f', 2) + "KiB";
            } else
                size = QString::number(size_b) + " bytes";

            addToLog(tr("Output file size: %1").arg( size ));
        }
        ui->textLog->ensureCursorVisible();

        ui->progressBar->setValue(ui->progressBar->maximum());
        ui->btnConvert->disconnect();
        displayStartProcessMode();

        QMessageBox::information(this, tr("Conversion"), tr("Operation completed."));
    });

    displayStartProcessMode();
}

void MainWindow::displayStopProcessMode()
{
    ui->btnConvert->setText(tr("&Stop"));

    connect(ui->btnConvert, &QPushButton::clicked, this, [=](){
       m_proc.kill();
       ui->progressBar->setValue(0);
    });
}

void MainWindow::displayStartProcessMode()
{
    ui->btnConvert->setText(tr("&Convert"));

    connect(ui->btnConvert, &QPushButton::clicked, this, [=](){
        ui->progressBar->setValue(0);

        updateSettings();
        updateOpts();

        if (!ui->listInputFiles->count()) {
            QMessageBox::critical(this, tr("Conversion"), tr("Please select some images to convert!"));
            return;
        }

        if (ui->edTargetFile->text().isEmpty()) {
            QMessageBox::critical(this, tr("Conversion"), tr("Please enter a target filename!"));
            return;
        }

        QFileInfo fi(ui->edTargetFile->text());
        const QString ext = fi.suffix().toLower();
        if (ext != "djvu" && ext != "djv") {
            if (QMessageBox::question(this, tr("Conversion"), tr("Target filename must have .djvu or .djv extension!\nAdd \".djvu\" to filename and continue?")) !=
                    QMessageBox::StandardButton::Yes) {
                return;
            }
            ui->edTargetFile->setText(ui->edTargetFile->text() + ".djvu");
            fi.setFile(ui->edTargetFile->text());
        }

        if (fi.exists()) {
            if (QMessageBox::question(this, tr("Conversion"), tr("Target file already exists! Continue?")) !=
                    QMessageBox::StandardButton::Yes) {
                return;
            }
        }


        QStringList opts (opt2cmd(m_opt, m_supportedOpts).split(" ", Qt::SkipEmptyParts));

        for(int i = 0; i < ui->listInputFiles->count(); ++i) {
            opts.append(ui->listInputFiles->item(i)->data(Qt::UserRole).toString());
        }

        ui->progressBar->setMaximum(ui->listInputFiles->count());

        opts.append(ui->edTargetFile->text());

        m_proc.setArguments(opts);
        m_proc.setProcessChannelMode(QProcess::ProcessChannelMode::MergedChannels);
        m_proc.setProgram(m_set.path2Bin);
        m_proc.setArguments(opts);

        if (!ui->textLog->document()->isEmpty()) {
            addToLog("===\n\n");
        }

        addToLog(tr("Command line: \"%1\"").arg(
                     m_proc.program() + " " + m_proc.arguments().join(" ")));
        m_proc.start(QProcess::Unbuffered|QProcess::ReadOnly);
    });

}


MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnOpenFolder_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select all images in directory");
    if (!path.isEmpty()) {
        QDir dir(path, "*.bmp;*.tiff;*.tif;*.pbm");
        const QFileInfoList fl = dir.entryInfoList();
        for (const QFileInfo& fi: fl) {
            QListWidgetItem* item = new QListWidgetItem(fi.fileName(), ui->listInputFiles);
            item->setData(Qt::UserRole, fi.filePath());
            ui->listInputFiles->addItem(item);
        }
    }

}

void MainWindow::on_btnClearFiles_clicked()
{
    ui->listInputFiles->clear();
}

void MainWindow::on_btnRemove_clicked()
{
    qDeleteAll(ui->listInputFiles->selectedItems());
}

void MainWindow::on_btnOpenFiles_clicked()
{
    const QStringList res = QFileDialog::getOpenFileNames(this, "Select images", "", "*.bmp *.tiff *.tif *.pbm");
    for (const QString& filename: res) {
        QFileInfo fi(filename);
        QListWidgetItem* item = new QListWidgetItem(fi.fileName(), ui->listInputFiles);
        item->setData(Qt::UserRole, fi.filePath());
        ui->listInputFiles->addItem(item);
    }
}

void MainWindow::on_btnSaveFile_clicked()
{
    QString res = QFileDialog::getSaveFileName(this, "Select target document", "", "*djvu *.djv");
    if (!res.isEmpty()) {
        QFileInfo fi(res);
        if (fi.completeSuffix().isEmpty()) res += ".djvu";
        ui->edTargetFile->setText(res);
    }
}

void enable_if_supported(QWidget* w, const QChar& c, const QSet<QChar> supported)
{
    if (w) {
        w->setEnabled(supported.contains(c));
    }
}

void MainWindow::filterSupportedOpts()
{
    qDebug() << m_supportedOpts;
    enable_if_supported(ui->edDPI, 'd', m_supportedOpts);
    enable_if_supported(ui->edMaxThreads, 't', m_supportedOpts);
    enable_if_supported(ui->edPagesPerDict, 'p', m_supportedOpts);
    enable_if_supported(ui->cbClassifier, 'C', m_supportedOpts);
    enable_if_supported(ui->sbAgression, 'a', m_supportedOpts);
    enable_if_supported(ui->edExt, 'X', m_supportedOpts);
    enable_if_supported(ui->cbLossy, 'l', m_supportedOpts);
    enable_if_supported(ui->cbClean, 'c', m_supportedOpts);
    enable_if_supported(ui->cbMatch, 'm', m_supportedOpts);
    enable_if_supported(ui->cbSmooth, 's', m_supportedOpts);
    enable_if_supported(ui->cbProtos, 'n', m_supportedOpts);
    enable_if_supported(ui->cbAveraging, 'A', m_supportedOpts);
    enable_if_supported(ui->cbErosion, 'e', m_supportedOpts);
    enable_if_supported(ui->cbReport, 'r', m_supportedOpts);
    enable_if_supported(ui->cbWarnings, 'w', m_supportedOpts);
    enable_if_supported(ui->cbVerbose, 'v', m_supportedOpts);
    enable_if_supported(ui->rbIndirectDoc, 'i', m_supportedOpts);
    enable_if_supported(ui->rbBundledDoc, 'i', m_supportedOpts);
}

void MainWindow::displayOpts()
{
    ui->edDPI->setText(QString::number(m_opt.dpi));
    ui->edMaxThreads->setText(QString::number(m_opt.threads));
    ui->edPagesPerDict->setText(QString::number(m_opt.pagesPerDict));
    ui->cbClassifier->setCurrentIndex(m_opt.classifier-1);
    ui->sbAgression->setValue(m_opt.agression);
    ui->edExt->setText(m_opt.ext);
    ui->cbLossy->setChecked(m_opt.lossy);

    emit ui->cbLossy->stateChanged(m_opt.lossy);

    if (!m_opt.lossy) {
        ui->cbClean->setChecked(m_opt.clean);
        ui->cbMatch->setChecked(m_opt.match);
        ui->cbSmooth->setChecked(m_opt.smooth);
        ui->cbProtos->setChecked(m_opt.protos);
        ui->cbAveraging->setChecked(m_opt.averaging);
        ui->cbErosion->setChecked(m_opt.erosion);
    }

    ui->cbReport->setChecked(m_opt.report);
    ui->cbWarnings->setChecked(m_opt.warnings);
    ui->cbVerbose->setChecked(m_opt.verbose);

    if (m_opt.indirect) {
        ui->rbIndirectDoc->setChecked(true);
    } else ui->rbBundledDoc->setChecked(true);
}

void MainWindow::updateOpts()
{

    m_opt.dpi = ui->edDPI->text().toInt();

    if (ui->edMaxThreads->isEnabled()) {
        m_opt.threads = ui->edMaxThreads->text().toInt();
    }

    m_opt.pagesPerDict = ui->edPagesPerDict->text().toInt();

    if (ui->cbClassifier->isEnabled()) {
        m_opt.classifier = ui->cbClassifier->currentIndex()+1;
    }

    m_opt.agression = ui->sbAgression->value();
    m_opt.ext = ui->edExt->text();
    m_opt.lossy = ui->cbLossy->isChecked();
    m_opt.clean = ui->cbClean->isChecked();
    m_opt.match = ui->cbMatch->isChecked();
    m_opt.smooth = ui->cbSmooth->isChecked();
    m_opt.protos = ui->cbProtos->isChecked();
    m_opt.averaging = ui->cbAveraging->isChecked();
    m_opt.erosion = ui->cbErosion->isChecked();
    m_opt.report = ui->cbReport->isChecked();
    m_opt.warnings = ui->cbWarnings->isChecked();
    m_opt.verbose = ui->cbVerbose->isChecked();
    m_opt.indirect = ui->rbIndirectDoc->isChecked();
}

void MainWindow::displaySettings()
{
    ui->edPath2Bin->setText(m_set.path2Bin);
}

void MainWindow::updateSettings()
{
    m_set.path2Bin = ui->edPath2Bin->text();
}

void MainWindow::on_cbLossy_stateChanged(int arg1)
{
    if (!m_supportedOpts.contains('l')) {
        return;
    }

    if (arg1) {
        ui->cbClean->setChecked(true);
        ui->cbMatch->setChecked(true);
        ui->cbSmooth->setChecked(true);
        ui->cbProtos->setChecked(true);
        ui->cbAveraging->setChecked(true);
        ui->cbErosion->setChecked(true);
    }

    ui->cbClean->setEnabled(!arg1 && m_supportedOpts.contains('c'));
    ui->cbMatch->setEnabled(!arg1 && m_supportedOpts.contains('m'));
    ui->cbSmooth->setEnabled(!arg1 && m_supportedOpts.contains('s'));
    ui->cbProtos->setEnabled(!arg1 && m_supportedOpts.contains('n'));
    ui->cbAveraging->setEnabled(!arg1 && m_supportedOpts.contains('A'));
    ui->cbErosion->setEnabled(!arg1 && m_supportedOpts.contains('e'));
}

void MainWindow::on_label_7_linkActivated(const QString &/*link*/)
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_btnClearLog_clicked()
{
    ui->textLog->clear();
}

void MainWindow::getEncoderDetails()
{
    ui->btnConvert->setEnabled(true);
    updateSettings();
    m_supportedOpts.clear();
    QProcess proc(this);
    proc.start(m_set.path2Bin, QStringList());
    proc.waitForFinished(2000);

    const QString output(proc.readAllStandardOutput());

    // Let's grep the minidjvu version
    QRegularExpression re(".*minidjvu[mod-]* ([0-9.a-zA-Z]+) - .*");
    QRegularExpressionMatch match = re.match(output);
    QString version;

    if (match.hasMatch() && match.lastCapturedIndex() > 0) {
        version = match.captured(1);
    } else {
        //"Предупреждение: версии программы и библиотеки не совпадают:
        //program version 0.9, library version 0.9m01."
        re.setPattern(".*program version ([0-9.a-zA-Z]+, library version [0-9.a-zA-Z]+)\\..*");
        match = re.match(output);
        if (match.hasMatch() && match.lastCapturedIndex() > 0) {
            version = match.captured(1);
        } else {
            version = tr("! can't be found !");
            ui->btnConvert->setEnabled(false);
        }
    }

    QString supported_opts;

    const QStringList sl(output.split('\n'));
    if (!sl.isEmpty()) {
        for (const QString& s: sl) {
            const QString trimmed = s.trimmed();
            if (trimmed.startsWith("-") && trimmed.length() > 2) {
                supported_opts += QString("-%1 ").arg(trimmed[1]);
                m_supportedOpts += trimmed[1];
            }
        }
    }

    ui->lblBinVer->setText(tr("Detected minidjvu version: <i>%1</i>").arg(version));
    if (!supported_opts.isEmpty()) {
        ui->lblBinVer->setText(tr("%1<br>Supported options: %2").arg(ui->lblBinVer->text()).arg(supported_opts));
    }

    filterSupportedOpts();
    displayOpts();
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    QWidget* new_tab = ui->tabWidget->widget(index);

    if (m_ptrPreviousTab == ui->tabOptions) {
        updateOpts();
    }
    m_ptrPreviousTab = new_tab;

    if (ui->tabSettings == new_tab) {
        getEncoderDetails();
    } else if (ui->tabOptions == new_tab) {
        filterSupportedOpts();
        displayOpts();
    }
}

void MainWindow::on_btnOptReset_clicked()
{
    init_options();
    filterSupportedOpts();
    displayOpts();
}

void MainWindow::on_cbProtos_stateChanged(int arg1)
{
    if (m_supportedOpts.contains('A')) {
        ui->cbAveraging->setEnabled(arg1);
    }
}

void MainWindow::on_cbMatch_stateChanged(int arg1)
{
    if (m_supportedOpts.contains('a')) {
        ui->sbAgression->setEnabled(arg1);
    }
}

void MainWindow::on_edPath2Bin_textChanged(const QString &/*arg1*/)
{
    getEncoderDetails();
}

void MainWindow::addToLog(const QString& txt)
{
    ui->textLog->appendPlainText(txt);
    if (ui->textLog->verticalScrollBar()) {
        ui->textLog->moveCursor(QTextCursor::End);
        ui->textLog->ensureCursorVisible();
    }
}

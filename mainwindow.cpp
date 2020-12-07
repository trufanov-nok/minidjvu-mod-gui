#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include <QTime>
#include <QIntValidator>

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
    return opt;
}

Settings init_settings()
{
    Settings set;
    set.path2Bin = "/home/truf/dev/minidjvu/minidjvu";
    set.isOldBin = false;
    return set;
}

QString opt2cmd(const Options& opt, bool is_new = true) {

    QString res("-d %1 -p %2 ");
    res = res.arg(opt.dpi).arg(opt.pagesPerDict);

    if (is_new && opt.threads) {
        res += "-t " + QString::number(opt.threads) + " ";
    }

    if (is_new && opt.classifier) {
        res += "-C " + QString::number(opt.classifier) + " ";
    }

    if (opt.indirect) {
        res += "-i ";
    }

    if (!opt.ext.isEmpty()) {
        res += "-X " + opt.ext;
    }

    if (opt.lossy) {
        res += "-l ";
    } else {
        if (opt.clean)     res += "-c ";
        if (opt.match)     res += "-m ";
        if (opt.smooth)    res += "-s ";
        if (opt.erosion)   res += "-e ";

        if (!opt.protos) {
            res += "-n ";
        } else if (opt.averaging)
            res += "-A ";
    }

    if (opt.lossy || opt.match) {
        res += QString("-a %1 ").arg(opt.agression);
    }

    if (opt.report)   res += "-r ";
    if (opt.warnings) res += "-w ";
    if (opt.verbose)  res += "-v ";

    return res;
}

#include <QtGlobal>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_proc(this)

{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);
    QIntValidator * validator = new QIntValidator(5, 10000, this);
    ui->edDPI->setValidator(validator);
    ui->edPagesPerDict->setValidator(validator);
    ui->edMaxThreads->setValidator(validator);

    m_set = init_settings();
    m_opt = init_options();
    displaySettings();
    displayOpts();


    connect(&m_proc, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        ui->textLog->appendPlainText(tr("Process error: %1").arg(error));

        ui->btnConvert->disconnect();
        displayStartProcessMode();
    });


    connect(&m_proc, &QProcess::started, this, [=]() {
        m_timer.start();
        ui->textLog->appendPlainText(tr("Process started"));

        ui->btnConvert->disconnect();
        displayStopProcessMode();
    });

    connect(&m_proc, &QProcess::readyRead, this, [=]() {
        QString val(m_proc.readAll());
        ui->textLog->appendPlainText(val);
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
        ui->textLog->appendPlainText(tr("Process finished with exit code: %1").arg(exitCode));

        QString time = t.toString("ss.zzz") + tr(" sec.");
        if (t.minute()) time += t.toString("mm") + tr(" min. ");
        if (t.hour()) time += t.toString("HH") + tr(" h. ");
        ui->textLog->appendPlainText(tr("Elapsed time: %1").arg(time));

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

            ui->textLog->appendPlainText(tr("Output file size: %1").arg( size ));
        }

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


        QStringList opts (opt2cmd(m_opt, !m_set.isOldBin).split(" ", Qt::SkipEmptyParts));

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
            ui->textLog->appendPlainText("===\n\n");
        }

        ui->textLog->appendPlainText(tr("Command line: \"%1\"").arg(
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

void MainWindow::displayOpts()
{
    ui->edDPI->setText(QString::number(m_opt.dpi));

    ui->edMaxThreads->setEnabled(!m_set.isOldBin);
    ui->edMaxThreads->setText(QString::number(m_opt.threads));

    ui->edPagesPerDict->setText(QString::number(m_opt.pagesPerDict));

    ui->cbClassifier->setEnabled(!m_set.isOldBin);
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
    ui->cbOldMinidjvu->setChecked(m_set.isOldBin);
}

void MainWindow::updateSettings()
{
    m_set.path2Bin = ui->edPath2Bin->text();
    m_set.isOldBin = ui->cbOldMinidjvu->isChecked();
}

void MainWindow::on_cbLossy_stateChanged(int arg1)
{
    if (arg1) {
        ui->cbClean->setChecked(true);
        ui->cbMatch->setChecked(true);
        ui->cbSmooth->setChecked(true);
        ui->cbProtos->setChecked(true);
        ui->cbAveraging->setChecked(true);
        ui->cbErosion->setChecked(true);
    }

    ui->cbClean->setEnabled(!arg1);
    ui->cbMatch->setEnabled(!arg1);
    ui->cbSmooth->setEnabled(!arg1);
    ui->cbProtos->setEnabled(!arg1);
    ui->cbAveraging->setEnabled(!arg1);
    ui->cbErosion->setEnabled(!arg1);
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
    updateSettings();
    QProcess proc(this);
    proc.start(m_set.path2Bin, QStringList());
    proc.waitForFinished(2000);
    QString output(proc.readAllStandardOutput());
    const int pos = output.indexOf(" ") + 1;
    QString ver = output.mid(pos, output.indexOf(" ", pos) - pos);
    if (ver.isEmpty()) {
        ver = tr("! can't be found !");
    }
    ui->lblBinVer->setText(tr("Detected minidjvu version: <i>%1</i>").arg(ver));
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    QWidget* selected = ui->tabWidget->widget(index);
    if (ui->tabSettings == selected) {
        getEncoderDetails();
    } else if (ui->tabOptions == selected) {
        const bool is_old_bin = ui->cbOldMinidjvu->isChecked();
        ui->edMaxThreads->setEnabled(!is_old_bin);
        ui->cbClassifier->setEnabled(!is_old_bin);
    }
}

void MainWindow::on_btnOptReset_clicked()
{
    init_options();
    displayOpts();
}

void MainWindow::on_cbProtos_stateChanged(int arg1)
{
    ui->cbAveraging->setEnabled(arg1);
}

void MainWindow::on_cbMatch_stateChanged(int arg1)
{
    ui->sbAgression->setEnabled(arg1);
}

void MainWindow::on_edPath2Bin_textChanged(const QString &/*arg1*/)
{
    getEncoderDetails();
}

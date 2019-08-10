#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include <QTime>

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

    QString res("-d %1 -p %2 -a %3 ");
    res = res.arg(opt.dpi).arg(opt.pagesPerDict).arg(opt.agression);

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
        if (!opt.protos)   res += "-n ";
        if (opt.averaging) res += "-A ";
        if (opt.erosion)   res += "-e ";
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
    m_set = init_settings();
    m_opt = init_options();
    displaySettings();
    displayOpts();


    connect(&m_proc, &QProcess::errorOccurred, [=](QProcess::ProcessError error) {
        ui->textLog->appendPlainText(tr("Process error: %1").arg(error));

        ui->btnConvert->disconnect();
        displayStartProcessMode();
    });


    connect(&m_proc, &QProcess::started, [=]() {
        m_timer.start();
        ui->textLog->appendPlainText(tr("Process started"));

        ui->btnConvert->disconnect();
        displayStopProcessMode();
    });

    connect(&m_proc, &QProcess::readyRead, [=]() {
        ui->textLog->appendPlainText(m_proc.readAll());
    });

    connect(&m_proc, qOverload<int>(&QProcess::finished), [=](int exitCode) {
        QTime t(0,0);
        t = t.addMSecs(m_timer.elapsed());
        ui->textLog->appendPlainText(tr("Process finished with exit code: %1").arg(exitCode));
        QString time = t.toString("ss.zzz") + tr(" sec.");
        if (t.minute()) time += t.toString("mm") + tr(" min. ");
        if (t.hour()) time += t.toString("HH") + tr(" h. ");
        ui->textLog->appendPlainText(tr("Elapsed time: %1\n=============================").arg(time));

        ui->btnConvert->disconnect();
        displayStartProcessMode();
    });

    displayStartProcessMode();
}

void MainWindow::displayStopProcessMode()
{
    ui->btnConvert->setText(tr("&Stop"));

    connect(ui->btnConvert, &QPushButton::clicked, [=](){
       m_proc.kill();
       ui->progressBar->setValue(0);
    });
}

void MainWindow::displayStartProcessMode()
{
    ui->btnConvert->setText(tr("&Convert"));

    connect(ui->btnConvert, &QPushButton::clicked, [=](){
      ui->progressBar->setValue(0);

      updateSettings();
      updateOpts();

      QStringList opts (opt2cmd(m_opt, !m_set.isOldBin).split(" ", QString::SkipEmptyParts));

      for(int i = 0; i < ui->listInputFiles->count(); ++i) {
          opts.append(ui->listInputFiles->item(i)->data(Qt::UserRole).toString());
      }

      opts.append(ui->edTargetFile->text());

      m_proc.setArguments(opts);
      m_proc.setProcessChannelMode(QProcess::ProcessChannelMode::MergedChannels);
      m_proc.setProgram(m_set.path2Bin);
      m_proc.setArguments(opts);
      ui->textLog->appendPlainText(tr("Command line: \"%1\"").arg(
                                       m_proc.program() + " " + m_proc.arguments().join(" ")));
      m_proc.start(QIODevice::ReadOnly);
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
        QFileInfoList fl = dir.entryInfoList();
        for (QFileInfo fi: fl) {
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
    QStringList res = QFileDialog::getOpenFileNames(this, "Select images", "", "*.bmp *.tiff *.tif *.pbm");
    for (QString filename: res) {
        QFileInfo fi(filename);
        QListWidgetItem* item = new QListWidgetItem(fi.fileName(), ui->listInputFiles);
        item->setData(Qt::UserRole, fi.filePath());
        ui->listInputFiles->addItem(item);
    }
}

void MainWindow::on_btnSaveFile_clicked()
{
    if (ui->rbBundledDoc->isChecked()) {
        QString res = QFileDialog::getSaveFileName(this, "Select target document", "", "*djvu *.djv");
        if (!res.isEmpty()) {
            QFileInfo fi(res);
            if (fi.completeSuffix().isEmpty()) res += ".djvu";
            ui->edTargetFile->setText(res);
        }
    } else {
        QString res = QFileDialog::getExistingDirectory(this, "Save indirect document to directory");
        if (!res.isEmpty()) {
            if (!res.endsWith(QDir::separator())) res += QDir::separator();
            ui->edTargetFile->setText(res);
        }
    }
}

void MainWindow::displayOpts()
{
    ui->edDPI->setText(QString::number(m_opt.dpi));

    ui->edMaxThreads->setEnabled(!m_set.isOldBin);
    ui->edMaxThreads->setText(QString::number(m_opt.threads));

    ui->edPagesPerDict->setText(QString::number(m_opt.pagesPerDict));

    ui->cbClassifier->setEnabled(!m_set.isOldBin);
    ui->cbClassifier->setCurrentIndex(3 - m_opt.classifier);

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
    m_opt.threads = ui->edMaxThreads->text().toInt();
    m_opt.pagesPerDict = ui->edPagesPerDict->text().toInt();
    m_opt.classifier = ui->cbClassifier->currentIndex();
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

void MainWindow::on_rbIndirectDoc_toggled(bool checked)
{
    if (checked) {
        ui->gbOutput->setTitle(tr("Set output path:"));
        QString path(ui->edTargetFile->text());
        path = QFileInfo(path).path();
        if (!path.endsWith(QDir::separator())) path += QDir::separator();
        ui->edTargetFile->setText(path);
    } else {
        ui->gbOutput->setTitle(tr("Set output file:"));
    }
}



void MainWindow::on_tabWidget_currentChanged(int index)
{
    QWidget* selected = ui->tabWidget->widget(index);
    if (ui->tabAbout == selected) {
        updateSettings();
        QProcess proc(this);
        proc.start(m_set.path2Bin);
        proc.waitForFinished(2000);
        QString output(proc.readAllStandardOutput());
        const int pos = output.indexOf(" ") + 1;
        QString ver = output.mid(pos, output.indexOf(" ", pos) - pos);
        if (ver.isEmpty()) {
            ver = tr("! can't be found !");
        }
        ui->lblBinVer->setText(tr("Detected minidjvu version: <i>%1</i>").arg(ver));
    } else if (ui->tabOptions == selected) {
        const bool is_old_bin = ui->cbOldMinidjvu->isChecked();
        ui->edMaxThreads->setEnabled(!is_old_bin);
        ui->cbClassifier->setEnabled(!is_old_bin);
    }
}

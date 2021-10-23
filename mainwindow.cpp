#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include <QTime>
#include <QIntValidator>
#include <QRegularExpression>
#include <QDebug>

const int __MAX_COMMAND_LINE_SIZE = 65536 / 2; // Windows limit

Options init_options() {
    Options opt;
    opt.dpi = 900;

    int cnt = QThread::idealThreadCount();
    if (cnt > 1) cnt--;
    opt.threads = cnt;

    opt.pagesPerDict = 10;
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
    res += add_opt('d', QString::number(opt.dpi), supported, opt.dpi > 0);
    res += add_opt('p', QString::number(opt.pagesPerDict), supported, opt.pagesPerDict > 0);
    res += add_opt('t', QString::number(opt.threads), supported, opt.threads > 0);
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

QStringList generate_sett_content(const QStringList& files, const Options& opt)
{
    QStringList sl;
    sl.append("(options\t\t\t\t\t# application options and defaults");
    sl.append("");
    sl.append("\t(default-djbz\t\t\t# default djbz settings");
    sl.append(QString("\t\taveraging\t\t%1\t# default averaging").arg(opt.protos && opt.averaging?"1":"0"));
    sl.append(QString("\t\taggression\t\t%1\t# default aggression level").arg(opt.agression));
    sl.append(QString("\t\terosion\t\t\t%1\t# default erosion").arg(opt.erosion?"1":"0"));
    sl.append(QString("\t\tno-prototypes\t%1\t# default prototypes usage").arg(opt.protos?"0":"1"));
    if (!opt.ext.isEmpty()) {
        sl.append(QString("\t\txtension\t\t%1\t# default djbz id extension").arg(opt.ext));
    }
    sl.append("\t)");
    sl.append("");
    sl.append("\t(default-image\t\t\t# default image options");
    sl.append("");
    sl.append(QString("\t\tdpi\t\t\t%1\t\t# if set, use this dpi value for encoding all images").arg(opt.dpi));
    sl.append("\t\t\t\t\t\t\t# except those that have personal dpi option set.");
    sl.append("\t\t\t\t\t\t\t# if not set, use dpi of source image of each page.");
    sl.append("");
    sl.append(QString("\t\tsmooth\t\t\t%1\t# default smoothing image before processing").arg(opt.smooth?"1":"0"));
    sl.append(QString("\t\tclean\t\t\t%1\t# default cleaning image after processing").arg(opt.clean?"1":"0"));
    sl.append(QString("\t\terosion\t\t\t%1\t# default erosion image after processing").arg(opt.erosion?"1":"0"));
    sl.append("\t)");
    sl.append("");
    sl.append(QString("\tindirect\t\t\t%1\t# save indirect djvu (multifile)").arg(opt.indirect?"1":"0"));
    sl.append(QString("\tlossy\t\t\t\t%1\t# if set, turns off or on following options:").arg(opt.lossy?"1":"0"));
    sl.append("\t\t\t\t\t\t\t# default-djbz::erosion, default-djbz::averaging");
    sl.append("\t\t\t\t\t\t\t# default-image::smooth, default-image::clean, default-image::match");
    sl.append("");
    sl.append(QString("\tmatch\t\t\t\t%1\t# use substitutions of visually similar characters for compression").arg(opt.match?"1":"0"));
    sl.append(QString("\tpages-per-dict\t\t%1\t# automatically assign pages that aren't referred").arg(opt.pagesPerDict));
    sl.append("\t\t\t\t\t\t\t# in any djbz blocks to the new djbz dictionaries.");
    sl.append("\t\t\t\t\t\t\t# New dictionaries contain 10 (default) pages or less.");
    sl.append("");
    sl.append(QString("\treport\t\t\t\t%1\t# report progress to stdout").arg(opt.report?"1":"0"));
    sl.append(QString("\tthreads-max\t\t\t%1\t# if set, use max N threads for processing (each thread").arg(opt.threads));
    sl.append("\t\t\t\t\t\t\t# process one block of pages. One djbz is a one block).");
    sl.append("\t\t\t\t\t\t\t# By default, if CPU have C cores:");
    sl.append("\t\t\t\t\t\t\t# if C > 2 then N = C-1, otherwise N = 1");
    sl.append("");
    sl.append(QString("\tverbose\t\t\t\t%1\t# print verbose log to stdout").arg(opt.verbose?"1":"0"));
    sl.append(QString("\twarnings\t\t\t%1\t# print libtiff warnings to stdout").arg(opt.warnings?"1":"0"));
    sl.append(")");
    sl.append("");
    sl.append("");
    sl.append("(input-files\t\t\t\t# Contains a list of input image files");
    sl.append("\t\t\t\t\t\t\t# the order is the same as the the order of pages in document.");
    sl.append("\t\t\t\t\t\t\t# Multipage tiff's are expanded to thet set of single page tiffs.");
    sl.append("");

    for (const QString& f: files) {
        sl.append("\t\"" + f + "\"");
    }

    sl.append(")");
    return sl;
}

QString get_sett_filename(const QString& document_filename)
{
    QFileInfo fi(document_filename);
    return fi.absolutePath() + "/" + fi.baseName() + ".sett";
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_proc(this),
    m_conversion_in_progress(false)
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

#if QT_VERSION >= 0x050600
    connect(&m_proc, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(process_error(QProcess::ProcessError)));
#else
    connect(&m_proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(process_error(QProcess::ProcessError)));
#endif

    connect(&m_proc, SIGNAL(started()), this, SLOT(process_started()));
    connect(&m_proc, SIGNAL(readyRead()), this, SLOT(process_ready_read()));
    connect(&m_proc, SIGNAL(finished(int)), this, SLOT(process_finished(int)));

    displayStartProcessMode();
}

void MainWindow::process_error(QProcess::ProcessError error)
{
    addToLog(tr("Process error: %1").arg(error));
    displayStartProcessMode();
}

void MainWindow::process_started()
{
    m_timer.start();
    addToLog(tr("Process started"));

    ui->btnConvert->setText(tr("&Stop"));
    m_conversion_in_progress = true;
}

void MainWindow::process_ready_read()
{
    QString val(m_proc.readAll());
    addToLog(val);
#if QT_VERSION >= 0x050E00
    const QStringList sl = val.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
#else
    const QStringList sl = val.split(QRegularExpression("[\r\n]"), QString::SkipEmptyParts);
#endif
    for (const QString& s: sl) {
        if (s.startsWith("[") && s.endsWith("%]")) {
            float progress = s.mid(1,s.length()-3).toFloat() /100.;
            ui->progressBar->setValue(ui->progressBar->maximum()*progress);
        }
    }
}
void MainWindow::process_finished(int exitCode)
{
    QTime t(0,0);
    t = t.addMSecs(m_timer.elapsed());
    addToLog(tr("Process finished with exit code: %1").arg(exitCode));

    QString time = t.toString("ss.zzz") + tr(" sec.");
    if (t.minute()) time += t.toString("mm") + tr(" min. ");
    if (t.hour()) time += t.toString("HH") + tr(" h. ");
    addToLog(tr("Elapsed time: %1").arg(time));

    const QStringList args = m_proc.arguments();
    QFileInfo fi(args.last());
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
    displayStartProcessMode();

    QMessageBox::information(this, tr("Conversion"), tr("Operation completed."));
}

void MainWindow::displayStartProcessMode()
{
    ui->btnConvert->setText(tr("&Convert"));
    m_conversion_in_progress = false;
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
    enable_if_supported(ui->edDPI, 'd', m_supportedOpts);
    enable_if_supported(ui->edMaxThreads, 't', m_supportedOpts);
    enable_if_supported(ui->edPagesPerDict, 'p', m_supportedOpts);
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
        ui->lblBinVer->setText(tr("%1<br>Supported options: %2").arg(ui->lblBinVer->text(), supported_opts));
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

void MainWindow::on_btnConvert_clicked()
{
    if (m_conversion_in_progress) {
        m_proc.kill();
        ui->progressBar->setValue(0);
    } else {
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

        ui->progressBar->setMaximum(ui->listInputFiles->count());

        QStringList files;
        for(int i = 0; i < ui->listInputFiles->count(); ++i) {
            files.append(ui->listInputFiles->item(i)->data(Qt::UserRole).toString());
        }

        bool use_sett_file = ui->cbUseSettingsFile->isChecked();

#if QT_VERSION >= 0x050E00
        const QStringList cmd_opts(opt2cmd(m_opt, m_supportedOpts).split(" ", Qt::SkipEmptyParts));
#else
        const QStringList cmd_opts(opt2cmd(m_opt, m_supportedOpts).split(" ", QString::SkipEmptyParts));
#endif

        if (!use_sett_file &&
                ( m_set.path2Bin.size() +
                  cmd_opts.join("\" \"").size() +
                  files.join("\" \"").size() +
                  ui->edTargetFile->text().size() + 2 >
                  (__MAX_COMMAND_LINE_SIZE - 300) ) ) {

            // enforce cbUseSettingsFile if our command may too big to execute

            if (QMessageBox::warning(this, tr("Command line is too big"),
                                     tr("The list of input files and options can't be passed to encoder as command line "
                                        "arguments because of its size is limited.\n"
                                        "The settings file usage will be enforced. Continue?"),
                                     QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Cancel) {
                return;
            }

            ui->cbUseSettingsFile->setChecked(true);
            use_sett_file = true;
        }

        QStringList opts;
        if (!use_sett_file) {
            opts.append(cmd_opts);
            opts.append(files);
        } else {
            const QString filename = get_sett_filename(ui->edTargetFile->text());
            QFile f(filename);
            if (f.open(QIODevice::WriteOnly)) {
                const QStringList sl = generate_sett_content(files, m_opt);
                QTextStream ts(&f);
                for (const QString& s: sl) {
                    ts << s << "\n";
                }
                f.close();
            } else {
                QMessageBox::critical(this, tr("Write file error"),
                                      tr("Can't open file %1 for writing. Terminating.").arg(filename));
                return;
            }

            opts.append("-S");
            opts.append(filename);
        }

        opts.append(ui->edTargetFile->text());

        m_proc.setProcessChannelMode(QProcess::ProcessChannelMode::MergedChannels);
        m_proc.setProgram(m_set.path2Bin);
        m_proc.setArguments(opts);

        if (!ui->textLog->document()->isEmpty()) {
            addToLog("===\n\n");
        }

        addToLog(tr("Command line: \"%1\"").arg(
                     m_proc.program() + " " + m_proc.arguments().join(" ")));
        m_proc.start(QProcess::Unbuffered|QProcess::ReadOnly);
    }
}

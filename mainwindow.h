#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QElapsedTimer>
#include <QSet>
#include <QtGlobal> // for QT_VERSION

namespace Ui {
class MainWindow;
}

struct Settings {
    QString path2Bin;
};

struct Options {
    int dpi;
    int threads;
    int pagesPerDict;
    int agression;
    QString ext;
    bool lossy;
    bool clean;
    bool match;
    bool smooth;
    bool protos;
    bool averaging;
    bool erosion;
    bool report;
    bool warnings;
    bool verbose;
    bool indirect;
    bool unbuffered;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private Q_SLOTS:
    void process_error(QProcess::ProcessError error);
    void process_started();
    void process_ready_read();
    void process_finished(int exitCode);

private Q_SLOTS:
    void on_btnOpenFolder_clicked();
    void on_btnClearFiles_clicked();
    void on_btnRemove_clicked();
    void on_btnOpenFiles_clicked();
    void on_btnSaveFile_clicked();
    void on_cbLossy_stateChanged(int arg1);
    void on_label_7_linkActivated(const QString &link);
    void on_btnClearLog_clicked();
    void on_tabWidget_currentChanged(int index);
    void on_btnOptReset_clicked();
    void on_cbProtos_stateChanged(int arg1);
    void on_cbMatch_stateChanged(int arg1);
    void on_edPath2Bin_textChanged(const QString &arg1);

    void on_btnConvert_clicked();

private:
    void addToLog(const QString &txt);
    void filterSupportedOpts();
    void displayOpts();
    void displaySettings();
    void updateOpts();
    void updateSettings();
    void displayStartProcessMode();
    void getEncoderDetails();

private:
    Ui::MainWindow *ui;
    Settings m_set;
    Options m_opt;
    QProcess m_proc;
    QElapsedTimer m_timer;
    QSet<QChar> m_supportedOpts;
    QWidget* m_ptrPreviousTab;
    bool m_conversion_in_progress;
};

#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QElapsedTimer>

namespace Ui {
class MainWindow;
}

struct Settings {
    QString path2Bin;
    bool isOldBin;
};

struct Options {
    int dpi;
    int threads;
    int pagesPerDict;
    int classifier;
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
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnOpenFolder_clicked();

    void on_btnClearFiles_clicked();

    void on_btnRemove_clicked();

    void on_btnOpenFiles_clicked();

    void on_btnSaveFile_clicked();

    void on_cbLossy_stateChanged(int arg1);

    void on_label_7_linkActivated(const QString &link);

    void on_btnClearLog_clicked();

    void on_tabWidget_currentChanged(int index);

    void on_rbIndirectDoc_toggled(bool checked);

private:
    void displayOpts();
    void displaySettings();
    void updateOpts();
    void updateSettings();
    void displayStopProcessMode();
    void displayStartProcessMode();
private:
    Ui::MainWindow *ui;
    Settings m_set;
    Options m_opt;
    QProcess m_proc;
    QElapsedTimer m_timer;
};

#endif // MAINWINDOW_H

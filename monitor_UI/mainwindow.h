#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QMessageBox>
#include <QJsonArray>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct DataPoint {
    qint64 timestamp;
    double cpuPercent;
    double memUsedPercent;
    double netInRate;
    double netOutRate;
    double diskUtil;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void fetchData();
    void onReplyFinished(QNetworkReply *reply);
    void onHostSelected(int index);
    void updateStatus(const QString &msg);

private:
    void setupUI();
    void setupLightTheme();  // 改为白色主题
    void appendData(const QJsonObject &obj);
    void updateCharts();
    void updateAllForCurrentHost();
    void updateOverview(const QJsonObject &obj);
    void updateCpuTable(const QJsonArray &cpuStats);
    void updateMemTable(const QJsonObject &memInfo);
    void updateNetTable(const QJsonArray &netInfos);
    void updateDiskTable(const QJsonArray &diskInfos);
    void updateSoftIrqTable(const QJsonArray &softIrqs);

    Ui::MainWindow *ui;

    QNetworkAccessManager *networkManager;
    QTimer *timer;
    QPushButton *refreshBtn;
    QLabel *statusLabel;
    QLabel *hostCountLabel;
    QComboBox *hostSelector;
    QTabWidget *tabWidget;

    QList<DataPoint> history;
    const int MAX_HISTORY = 60;

    // 图表控件
    QChartView *cpuChartView;
    QChartView *memChartView;
    QChartView *netChartView;
    QChartView *diskChartView;

    // 表格
    QTableWidget *cpuTable;
    QTableWidget *memTable;
    QTableWidget *netTable;
    QTableWidget *diskTable;
    QTableWidget *softIrqTable;

    // 概览卡片
    QLabel *scoreLabel;
    QLabel *load1Label;
    QLabel *load5Label;
    QLabel *load15Label;
    QLabel *memLabel;

    // 数据缓存
    QJsonArray latestArray;
    QString currentHost;

    const QString API_URL = "http://192.168.31.135:50052/api/latest";
};

#endif // MAINWINDOW_H
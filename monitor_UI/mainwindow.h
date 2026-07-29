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
    void setupDarkTheme();
    void appendData(const QJsonObject &obj);
    void updateCharts();
    void updateAllForCurrentHost();          // 新函数：更新所有显示，使用当前选中的主机
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
    QLabel *hostCountLabel;       // 显示在线主机数量
    QComboBox *hostSelector;      // 主机选择下拉框
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
    QJsonArray latestArray;        // 最近一次获取的所有主机数据
    QString currentHost;           // 当前选中的主机名

    const QString API_URL = "http://192.168.31.135:50052/api/latest";
};

#endif // MAINWINDOW_H
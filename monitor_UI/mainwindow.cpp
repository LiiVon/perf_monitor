#include "mainwindow.h"
#include "ui_mainwindow.h"

// QtCharts 头文件
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QHeaderView>
#include <QStyleFactory>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setupUI();
    setupLightTheme();

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onReplyFinished);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::fetchData);
    timer->start(3000);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::fetchData);

    fetchData();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle("系统性能监控看板");
    resize(1200, 750);

    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // ---- 顶部状态栏 ----
    QHBoxLayout *topLayout = new QHBoxLayout();
    statusLabel = new QLabel("状态: 初始化中...", this);
    statusLabel->setObjectName("statusLabel");

    // 在线主机数量标签
    hostCountLabel = new QLabel("在线: 0", this);
    hostCountLabel->setObjectName("hostCountLabel");

    // 主机选择下拉框
    hostSelector = new QComboBox(this);
    hostSelector->setMinimumWidth(150);
    hostSelector->setObjectName("hostSelector");
    connect(hostSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onHostSelected);

    refreshBtn = new QPushButton("刷新", this);
    refreshBtn->setObjectName("refreshBtn");

    topLayout->addWidget(statusLabel);
    topLayout->addStretch();
    topLayout->addWidget(hostCountLabel);
    topLayout->addWidget(new QLabel("  主机:", this));
    topLayout->addWidget(hostSelector);
    topLayout->addWidget(refreshBtn);
    mainLayout->addLayout(topLayout);

    // ---- Tab 控件 ----
    tabWidget = new QTabWidget(this);

    // ========== 1. 概览 Tab ==========
    QWidget *overviewTab = new QWidget();
    QVBoxLayout *overviewLayout = new QVBoxLayout(overviewTab);

    // 卡片区域
    QHBoxLayout *cardLayout = new QHBoxLayout();
    scoreLabel = new QLabel("评分: --", this);
    load1Label = new QLabel("1min负载: --", this);
    load5Label = new QLabel("5min负载: --", this);
    load15Label = new QLabel("15min负载: --", this);
    memLabel = new QLabel("内存使用率: --", this);

    for (auto lbl : {scoreLabel, load1Label, load5Label, load15Label, memLabel}) {
        lbl->setProperty("class", "cardLabel");
        lbl->setMinimumWidth(140);
        cardLayout->addWidget(lbl);
    }
    cardLayout->addStretch();
    overviewLayout->addLayout(cardLayout);

    // CPU 图表
    cpuChartView = new QChartView(this);
    cpuChartView->setRenderHint(QPainter::Antialiasing);
    cpuChartView->setMinimumHeight(180);
    overviewLayout->addWidget(cpuChartView);

    // 内存 + 网络 并排
    QHBoxLayout *chartRow = new QHBoxLayout();
    memChartView = new QChartView(this);
    memChartView->setRenderHint(QPainter::Antialiasing);
    memChartView->setMinimumHeight(160);
    netChartView = new QChartView(this);
    netChartView->setRenderHint(QPainter::Antialiasing);
    netChartView->setMinimumHeight(160);
    chartRow->addWidget(memChartView);
    chartRow->addWidget(netChartView);
    overviewLayout->addLayout(chartRow);

    // 磁盘图表
    diskChartView = new QChartView(this);
    diskChartView->setRenderHint(QPainter::Antialiasing);
    diskChartView->setMinimumHeight(160);
    overviewLayout->addWidget(diskChartView);

    tabWidget->addTab(overviewTab, "概览");

    // ========== 其他 Tab ==========
    cpuTable = new QTableWidget(this);
    cpuTable->setAlternatingRowColors(true);
    cpuTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabWidget->addTab(cpuTable, "CPU");

    memTable = new QTableWidget(this);
    memTable->setAlternatingRowColors(true);
    memTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabWidget->addTab(memTable, "内存");

    netTable = new QTableWidget(this);
    netTable->setAlternatingRowColors(true);
    netTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabWidget->addTab(netTable, "网络");

    diskTable = new QTableWidget(this);
    diskTable->setAlternatingRowColors(true);
    diskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabWidget->addTab(diskTable, "磁盘");

    softIrqTable = new QTableWidget(this);
    softIrqTable->setAlternatingRowColors(true);
    softIrqTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabWidget->addTab(softIrqTable, "软中断");

    mainLayout->addWidget(tabWidget);
    setCentralWidget(central);
}

// ============================================================
// 白色主题（浅色风格）
// ============================================================
void MainWindow::setupLightTheme()
{
    QString lightStyle = R"(
        /* 全局背景 */
        QMainWindow, QWidget {
            background-color: #f5f7fa;
            color: #2c3e50;
            font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
        }

        /* 顶部状态栏 */
        QLabel#statusLabel {
            background: transparent;
            color: #2c3e50;
            font-size: 14px;
            padding: 4px 12px;
        }
        QPushButton#refreshBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #1abc9c, stop:1 #16a085);
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
            font-weight: bold;
        }
        QPushButton#refreshBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #16a085, stop:1 #1abc9c);
        }

        /* TabWidget */
        QTabWidget::pane {
            background: transparent;
            border: none;
        }
        QTabBar::tab {
            background: transparent;
            color: #7f8c8d;
            padding: 10px 24px;
            margin-right: 2px;
            border: none;
            border-bottom: 3px solid transparent;
            font-weight: bold;
            font-size: 13px;
        }
        QTabBar::tab:selected {
            color: #2c3e50;
            border-bottom: 3px solid #2986d8;
        }
        QTabBar::tab:hover:!selected {
            color: #34495e;
            border-bottom: 3px solid #bdc3c7;
        }

        /* 表格 */
        QTableWidget {
            background-color: white;
            alternate-background-color: #f8f9fa;
            gridline-color: #e8ecf1;
            color: #2c3e50;
            border: none;
            font-size: 13px;
            border-radius: 8px;
        }
        QHeaderView::section {
            background-color: #ecf0f1;
            color: #2c3e50;
            padding: 8px 12px;
            border: none;
            font-weight: bold;
        }
        QTableWidget::item:selected {
            background: #2986d8;
            color: white;
        }

        /* 卡片标签 */
        QLabel[class="cardLabel"] {
            background-color: white;
            border: 1px solid #e8ecf1;
            border-radius: 8px;
            padding: 16px 20px;
            font-weight: bold;
            font-size: 16px;
            color: #2c3e50;
        }
        QLabel[class="cardLabel"]:hover {
            background-color: #eef2f7;
        }

        /* 图表容器 */
        QChartView {
            background: white;
            border: 1px solid #e8ecf1;
            border-radius: 8px;
            padding: 4px;
        }

        /* 滚动条 */
        QScrollBar:vertical {
            background: #ecf0f1;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #bdc3c7;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #95a5a6;
        }
        QScrollBar:horizontal {
            background: #ecf0f1;
            height: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #bdc3c7;
            border-radius: 4px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #95a5a6;
        }

        /* 下拉框 */
        QComboBox {
            background: white;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 5px 10px;
            color: #2c3e50;
        }
        QComboBox:hover {
            border-color: #2986d8;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #7f8c8d;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background: white;
            color: #2c3e50;
            selection-background-color: #2986d8;
            selection-color: white;
        }

        /* 进度条（备用） */
        QProgressBar {
            background: #ecf0f1;
            border: none;
            border-radius: 4px;
            text-align: center;
            color: #2c3e50;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #1abc9c, stop:1 #2986d8);
            border-radius: 4px;
        }

        /* 状态栏 */
        QStatusBar {
            background: #f5f7fa;
            color: #7f8c8d;
            border-top: 1px solid #e8ecf1;
        }

        /* 特定控件 */
        QLabel#hostCountLabel {
            color: #2986d8;
            font-weight: bold;
        }
        QComboBox#hostSelector {
            background: white;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 4px 10px;
            color: #2c3e50;
        }
        QComboBox#hostSelector:hover {
            border-color: #2986d8;
        }
        QComboBox#hostSelector::drop-down {
            border: none;
        }
        QComboBox#hostSelector QAbstractItemView {
            background: white;
            color: #2c3e50;
            selection-background-color: #2986d8;
            selection-color: white;
        }
    )";
    setStyleSheet(lightStyle);
}

void MainWindow::appendData(const QJsonObject &obj)
{
    DataPoint dp;
    dp.timestamp = QDateTime::currentSecsSinceEpoch();

    // CPU 使用率（取第一个核心）
    auto cpuStats = obj["cpu_stats"].toArray();
    dp.cpuPercent = cpuStats.isEmpty() ? 0 : cpuStats[0].toObject()["cpu_percent"].toDouble();

    // 内存使用率
    auto memInfo = obj["mem_info"].toObject();
    dp.memUsedPercent = memInfo["used_percent"].toDouble();

    // 网络速率（取第一个网卡）
    auto netInfos = obj["net_infos"].toArray();
    if (!netInfos.isEmpty()) {
        auto net = netInfos[0].toObject();
        dp.netInRate = net["rcv_rate"].toDouble();
        dp.netOutRate = net["send_rate"].toDouble();
    } else {
        dp.netInRate = 0;
        dp.netOutRate = 0;
    }

    // 磁盘利用率：取所有磁盘中最大值
    auto diskInfos = obj["disk_infos"].toArray();
    double maxUtil = 0;
    for (const auto &item : diskInfos) {
        double util = item.toObject()["util_percent"].toDouble();
        if (util > maxUtil) maxUtil = util;
    }
    dp.diskUtil = maxUtil;

    history.append(dp);
    if (history.size() > MAX_HISTORY)
        history.removeFirst();

    updateCharts();
}

void MainWindow::updateCharts()
{
    if (!cpuChartView || !memChartView || !netChartView || !diskChartView)
        return;

    if (history.size() < 2) return;

    // ---- CPU 图表 ----
    {
        QLineSeries *cpuSeries = new QLineSeries();
        cpuSeries->setName("CPU使用率 (%)");
        cpuSeries->setColor(QColor("#2986d8"));  // 蓝色
        for (int i = 0; i < history.size(); ++i) {
            cpuSeries->append(i, history[i].cpuPercent);
        }
        QChart *cpuChart = new QChart();
        cpuChart->addSeries(cpuSeries);
        cpuChart->setTitle("CPU使用率趋势");
        cpuChart->setTheme(QChart::ChartThemeLight);  // 浅色主题
        cpuChart->createDefaultAxes();
        cpuChart->axisX()->setRange(0, MAX_HISTORY);
        cpuChart->axisY()->setRange(0, 100);
        cpuChart->legend()->setVisible(true);
        cpuChartView->setChart(cpuChart);
    }

    // ---- 内存图表 ----
    {
        QLineSeries *memSeries = new QLineSeries();
        memSeries->setName("内存使用率 (%)");
        memSeries->setColor(QColor("#e67e22"));  // 橙色
        for (int i = 0; i < history.size(); ++i) {
            memSeries->append(i, history[i].memUsedPercent);
        }
        QChart *memChart = new QChart();
        memChart->addSeries(memSeries);
        memChart->setTitle("内存使用率趋势");
        memChart->setTheme(QChart::ChartThemeLight);
        memChart->createDefaultAxes();
        memChart->axisX()->setRange(0, MAX_HISTORY);
        memChart->axisY()->setRange(0, 100);
        memChart->legend()->setVisible(true);
        memChartView->setChart(memChart);
    }

    // ---- 网络图表（双线） ----
    {
        QLineSeries *inSeries = new QLineSeries();
        inSeries->setName("接收速率 (KB/s)");
        inSeries->setColor(QColor("#1abc9c"));  // 青绿
        QLineSeries *outSeries = new QLineSeries();
        outSeries->setName("发送速率 (KB/s)");
        outSeries->setColor(QColor("#2986d8"));  // 蓝色
        for (int i = 0; i < history.size(); ++i) {
            inSeries->append(i, history[i].netInRate);
            outSeries->append(i, history[i].netOutRate);
        }
        QChart *netChart = new QChart();
        netChart->addSeries(inSeries);
        netChart->addSeries(outSeries);
        netChart->setTitle("网络收发趋势");
        netChart->setTheme(QChart::ChartThemeLight);
        netChart->createDefaultAxes();
        netChart->axisX()->setRange(0, MAX_HISTORY);
        netChart->legend()->setVisible(true);
        netChartView->setChart(netChart);
    }

    // ---- 磁盘图表 ----
    {
        QLineSeries *diskSeries = new QLineSeries();
        diskSeries->setName("磁盘利用率 (%)");
        diskSeries->setColor(QColor("#f39c12"));  // 橙色
        for (int i = 0; i < history.size(); ++i) {
            diskSeries->append(i, history[i].diskUtil);
        }
        QChart *diskChart = new QChart();
        diskChart->addSeries(diskSeries);
        diskChart->setTitle("磁盘利用率趋势");
        diskChart->setTheme(QChart::ChartThemeLight);
        diskChart->createDefaultAxes();
        diskChart->axisX()->setRange(0, MAX_HISTORY);
        diskChart->axisY()->setRange(0, 100);
        diskChart->legend()->setVisible(true);
        diskChartView->setChart(diskChart);
    }
}

void MainWindow::updateOverview(const QJsonObject &obj)
{
    double score = obj["score"].toDouble();
    auto loadObj = obj["cpu_load"].toObject();
    double load1 = loadObj["load_avg_1"].toDouble();
    double load5 = loadObj["load_avg_5"].toDouble();
    double load15 = loadObj["load_avg_15"].toDouble();
    double memUsed = obj["mem_info"].toObject()["used_percent"].toDouble();

    scoreLabel->setText(QString("评分: %1").arg(score, 0, 'f', 1));
    load1Label->setText(QString("1min负载: %1").arg(load1, 0, 'f', 2));
    load5Label->setText(QString("5min负载: %1").arg(load5, 0, 'f', 2));
    load15Label->setText(QString("15min负载: %1").arg(load15, 0, 'f', 2));
    memLabel->setText(QString("内存使用率: %1%").arg(memUsed, 0, 'f', 1));

    // 内存高亮颜色（浅色主题下用深色）
    if (memUsed > 90) {
        memLabel->setStyleSheet("color: #e74c3c;");
    } else if (memUsed > 70) {
        memLabel->setStyleSheet("color: #f39c12;");
    } else {
        memLabel->setStyleSheet("color: #27ae60;");
    }
}

void MainWindow::updateCpuTable(const QJsonArray &cpuStats)
{
    cpuTable->setColumnCount(9);
    cpuTable->setHorizontalHeaderLabels({
        "CPU", "总使用率", "用户态", "内核态", "Nice", "空闲", "I/O等待", "硬中断", "软中断"
    });
    cpuTable->setRowCount(0);

    for (const auto &item : cpuStats) {
        QJsonObject obj = item.toObject();
        QString cpuName = obj["cpu_name"].toString();
        if (cpuName.isEmpty()) continue;   // 跳过空名称的条目
        int row = cpuTable->rowCount();
        cpuTable->insertRow(row);
        int col = 0;
        cpuTable->setItem(row, col++, new QTableWidgetItem(obj["cpu_name"].toString()));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["cpu_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["usr_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["system_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["nice_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["idle_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["io_wait_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["irq_percent"].toDouble(), 'f', 2) + "%"));
        cpuTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["soft_irq_percent"].toDouble(), 'f', 2) + "%"));
    }
}

void MainWindow::updateMemTable(const QJsonObject &memInfo)
{
    memTable->setColumnCount(4);
    memTable->setHorizontalHeaderLabels({"指标", "值(GB)", "指标", "值(GB)"});
    memTable->setRowCount(0);

    struct MemField { QString name; double value; };
    QList<MemField> fields = {
        {"总内存", memInfo["total"].toDouble()},
        {"空闲内存", memInfo["free"].toDouble()},
        {"可用内存", memInfo["avail"].toDouble()},
        {"缓冲区", memInfo["buffers"].toDouble()},
        {"缓存", memInfo["cached"].toDouble()},
        {"交换缓存", memInfo["swap_cached"].toDouble()},
        {"活跃内存", memInfo["active"].toDouble()},
        {"非活跃内存", memInfo["inactive"].toDouble()},
        {"脏页", memInfo["dirty"].toDouble()},
        {"回写中", memInfo["writeback"].toDouble()},
        {"匿名页", memInfo["anon_pages"].toDouble()},
        {"映射内存", memInfo["mapped"].toDouble()},
        {"可回收Slab", memInfo["kreclaimable"].toDouble()},
        {"不可回收Slab", memInfo["sunreclaim"].toDouble()}
    };

    for (int i = 0; i < fields.size(); i += 2) {
        int row = memTable->rowCount();
        memTable->insertRow(row);
        int col = 0;
        memTable->setItem(row, col++, new QTableWidgetItem(fields[i].name));
        memTable->setItem(row, col++, new QTableWidgetItem(QString::number(fields[i].value, 'f', 2)));
        if (i + 1 < fields.size()) {
            memTable->setItem(row, col++, new QTableWidgetItem(fields[i + 1].name));
            memTable->setItem(row, col++, new QTableWidgetItem(QString::number(fields[i + 1].value, 'f', 2)));
        }
    }
}

void MainWindow::updateNetTable(const QJsonArray &netInfos)
{
    netTable->setColumnCount(8);
    netTable->setHorizontalHeaderLabels({
        "网卡", "接收(KB/s)", "发送(KB/s)", "收包(pkt/s)",
        "发包(pkt/s)", "接收错误", "发送错误", "丢弃"
    });
    netTable->setRowCount(0);

    for (const auto &item : netInfos) {
        QJsonObject obj = item.toObject();
        int row = netTable->rowCount();
        netTable->insertRow(row);
        int col = 0;
        netTable->setItem(row, col++, new QTableWidgetItem(obj["name"].toString()));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["rcv_rate"].toDouble(), 'f', 2)));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["send_rate"].toDouble(), 'f', 2)));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["rcv_packets_rate"].toDouble(), 'f', 2)));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["send_packets_rate"].toDouble(), 'f', 2)));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["err_in"].toDouble())));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["err_out"].toDouble())));
        netTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["drop_in"].toDouble()) + "/" + QString::number(obj["drop_out"].toDouble())));
    }
}

void MainWindow::updateDiskTable(const QJsonArray &diskInfos)
{
    diskTable->setColumnCount(8);
    diskTable->setHorizontalHeaderLabels({
        "磁盘", "读吞吐(KB/s)", "写吞吐(KB/s)", "读IOPS", "写IOPS",
        "读延迟(ms)", "写延迟(ms)", "利用率"
    });
    diskTable->setRowCount(0);

    for (const auto &item : diskInfos) {
        QJsonObject obj = item.toObject();
        int row = diskTable->rowCount();
        diskTable->insertRow(row);
        int col = 0;
        diskTable->setItem(row, col++, new QTableWidgetItem(obj["name"].toString()));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["read_bytes_per_sec"].toDouble() / 1024.0, 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["write_bytes_per_sec"].toDouble() / 1024.0, 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["read_iops"].toDouble(), 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["write_iops"].toDouble(), 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["avg_read_latency_ms"].toDouble(), 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["avg_write_latency_ms"].toDouble(), 'f', 2)));
        diskTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["util_percent"].toDouble(), 'f', 2) + "%"));
    }
}

void MainWindow::updateSoftIrqTable(const QJsonArray &softIrqs)
{
    // 设置 11 列：CPU + 10 种软中断
    softIrqTable->setColumnCount(11);
    softIrqTable->setHorizontalHeaderLabels({
        "CPU", "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK",
        "IRQ_POLL", "TASKLET", "SCHED", "HRTIMER", "RCU"
    });
    softIrqTable->setRowCount(0);

    for (const auto &item : softIrqs) {
        QJsonObject obj = item.toObject();
        int row = softIrqTable->rowCount();
        softIrqTable->insertRow(row);
        int col = 0;
        softIrqTable->setItem(row, col++, new QTableWidgetItem(obj["cpu"].toString()));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["hi"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["timer"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["net_tx"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["net_rx"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["block"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["irq_poll"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["tasklet"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["sched"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["hrtimer"].toDouble(), 'f', 0)));
        softIrqTable->setItem(row, col++, new QTableWidgetItem(QString::number(obj["rcu"].toDouble(), 'f', 0)));
    }
}

void MainWindow::fetchData()
{
    updateStatus("正在获取数据...");
    QUrl url(API_URL);
    QNetworkRequest request(url);
    networkManager->get(request);
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        updateStatus("网络错误: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        updateStatus("数据格式错误");
        return;
    }

    latestArray = doc.array();
    if (latestArray.isEmpty()) {
        updateStatus("无数据: 等待 Worker 推送...");
        hostCountLabel->setText("在线: 0");
        hostSelector->clear();
        return;
    }

    // 更新主机数量
    hostCountLabel->setText(QString("在线: %1").arg(latestArray.size()));

    // 填充主机下拉框
    QString currentSelected = hostSelector->currentText();
    hostSelector->clear();
    QStringList hostNames;
    for (const auto &item : latestArray) {
        QString name = item.toObject()["hostname"].toString();
        hostNames << name;
    }
    hostSelector->addItems(hostNames);

    // 恢复之前选中的主机，或选第一个
    if (!currentSelected.isEmpty() && hostNames.contains(currentSelected)) {
        hostSelector->setCurrentText(currentSelected);
    } else {
        hostSelector->setCurrentIndex(0);
    }

    // 更新所有显示
    updateAllForCurrentHost();

    // 更新状态栏
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    updateStatus(QString("已更新 %1 | 在线: %2 台")
                     .arg(timestamp)
                     .arg(latestArray.size()));
}

void MainWindow::onHostSelected(int index)
{
    if (index < 0 || index >= latestArray.size()) return;
    QString host = latestArray[index].toObject()["hostname"].toString();
    if (host != currentHost) {
        currentHost = host;
        history.clear();   // 清空历史，重新记录当前主机的趋势
        updateAllForCurrentHost();
    }
}

void MainWindow::updateAllForCurrentHost()
{
    if (latestArray.isEmpty()) return;

    int index = hostSelector->currentIndex();
    if (index < 0) index = 0;
    if (index >= latestArray.size()) return;

    QJsonObject obj = latestArray[index].toObject();
    currentHost = obj["hostname"].toString();

    updateOverview(obj);
    appendData(obj);
    updateCpuTable(obj["cpu_stats"].toArray());
    updateMemTable(obj["mem_info"].toObject());
    updateNetTable(obj["net_infos"].toArray());
    updateDiskTable(obj["disk_infos"].toArray());
    updateSoftIrqTable(obj["soft_irqs"].toArray());

    // 更新状态栏中的主机信息
    updateStatus(QString("当前主机: %1").arg(currentHost));
}

void MainWindow::updateStatus(const QString &msg)
{
    statusLabel->setText(msg);
}
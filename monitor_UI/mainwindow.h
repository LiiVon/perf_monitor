#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// ============================================================
// 性能监控看板 v2 — shadcn/zinc 风格
// 参考 front.md：左侧边栏导航 + 顶部标题栏 + 统计卡片 + 渐变趋势图
// ============================================================

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QFrame>
#include <QButtonGroup>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPaintEvent>
#include <QMouseEvent>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 单个采样点（每个主机独立保存历史）
struct DataPoint {
    qint64 timestamp = 0;
    double cpuPercent = 0;
    double memUsedPercent = 0;
    double netInRate = 0;
    double netOutRate = 0;
    double diskUtil = 0;
};

// ------------------------------------------------------------
// 自绘渐变面积趋势图（对应 front.md 的 AreaChartSVG）
// 深色主线 + 灰色副线，纵向渐变填充，hover 显示数值
// ------------------------------------------------------------
class TrendAreaChart : public QWidget
{
    Q_OBJECT
public:
    enum class SeriesRole { Primary, Secondary };

    explicit TrendAreaChart(QWidget *parent = nullptr);

    void setHistory(const QList<DataPoint> *history);
    void setExtractor(SeriesRole role,
                      const std::function<double(const DataPoint &)> &fn);
    void setSeriesName(SeriesRole role, const QString &name);
    void setYRange(double lo, double hi);   // 固定 Y 轴（不设则自动）
    void setMaxPoints(int n);               // 时间范围（对应 30s/1min/3min 切换）
    void refresh();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    double valueAt(SeriesRole role, const DataPoint &dp) const;

    const QList<DataPoint> *m_history = nullptr;
    std::function<double(const DataPoint &)> m_primary;
    std::function<double(const DataPoint &)> m_secondary;
    QString m_primaryName;
    QString m_secondaryName;
    double m_yLo = 0, m_yHi = -1;   // yHi<0 表示自动
    int m_maxPoints = 60;
    int m_hoverIndex = -1;
};

// ------------------------------------------------------------
// 统计卡片（对应 front.md 的 statCards）
// 标题 + 趋势徽章 + 大数值 + 底部说明
// ------------------------------------------------------------
class StatCard : public QFrame
{
    Q_OBJECT
public:
    explicit StatCard(const QString &title, QWidget *parent = nullptr);
    // trendUp = true 绿色徽章，false 红色徽章
    void updateValue(const QString &value, const QString &badge,
                     bool trendUp, const QString &footer, const QString &detail);

private:
    QLabel *m_title;
    QLabel *m_badge;
    QLabel *m_value;
    QLabel *m_footer;
    QLabel *m_detail;
};

// ------------------------------------------------------------
// 主窗口
// ------------------------------------------------------------
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
    void onRangeChanged(int id);
    void onPageButton();

private:
    // ---- 构建 UI ----
    void applyZincTheme();
    QWidget *buildSidebar();
    QWidget *buildHeader();
    QWidget *buildOverviewPage();
    QWidget *buildCpuPage();
    QWidget *buildMemPage();
    QWidget *buildNetPage();
    QWidget *buildDiskPage();
    QWidget *buildSoftIrqPage();

    // ---- 构建辅助 ----
    QPushButton *addNavItem(const QString &title, const QString &pageId);
    QFrame *wrapInCard(QWidget *content, const QString &title = QString(),
                       const QString &subtitle = QString(),
                       QWidget *headerRight = nullptr);
    QTableWidget *makeTable(const QStringList &headers);
    void gotoPage(const QString &pageId);

    // ---- 数据更新 ----
    void appendData(const QJsonObject &obj);
    void updateAllForCurrentHost();
    void updateStatCards(const QJsonObject &obj);
    void refreshCharts();
    void updateCpuTable(const QJsonArray &cpuStats);
    void updateMemTable(const QJsonObject &memInfo);
    void updateNetTable(const QJsonArray &netInfos);
    void updateDiskTable(const QJsonArray &diskInfos);
    void updateSoftIrqTable(const QJsonArray &softIrqs);
    void setCellBadge(QTableWidget *table, int row, int col,
                      const QString &text, const QColor &color);
    void setStatus(const QString &msg, bool ok);

    // ---- 静态辅助 ----
    static QColor levelColor(double v, double warn, double crit);

    Ui::MainWindow *ui;

    // 网络与定时器
    QNetworkAccessManager *networkManager;
    QTimer *timer;

    // 布局核心
    QFrame *sidebar;
    QStackedWidget *pages;
    QMap<QString, int> pageIndex;
    QMap<QString, QPushButton *> navButtons;
    QButtonGroup *navGroup;

    // 顶部栏
    QPushButton *menuBtn;
    QLabel *titleLabel;
    QLabel *statusDot;
    QLabel *statusLabel;
    QLabel *onlineBadge;
    QComboBox *hostSelector;
    QPushButton *refreshBtn;

    // 概览页
    StatCard *scoreCard;
    StatCard *cpuCard;
    StatCard *memCard;
    StatCard *loadCard;
    TrendAreaChart *cpuTrendChart;
    TrendAreaChart *netTrendChart;
    TrendAreaChart *diskTrendChart;
    QButtonGroup *rangeGroup;

    // 主机状态表（分页，对应 front.md 的 Data Table）
    QTableWidget *hostsTable;
    QLabel *pageInfoLabel;
    QPushButton *pageFirst;
    QPushButton *pagePrev;
    QPushButton *pageNext;
    QPushButton *pageLast;
    int hostPage = 0;
    static constexpr int PAGE_SIZE = 5;

    // 详情页
    QTableWidget *cpuTable;
    QTableWidget *memTable;
    QTableWidget *netTable;
    QTableWidget *diskTable;
    QTableWidget *softIrqTable;

    // 数据
    QMap<QString, QList<DataPoint>> historyMap;
    QJsonArray latestArray;
    QString currentHost;
    double prevScore = -1;
    double prevCpu = -1;
    double prevMem = -1;
    double prevLoad = -1;

    static constexpr int MAX_HISTORY = 60;
    const QString API_URL = "http://192.168.31.135:50052/api/latest";
};

#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"

// ============================================================
// 性能监控看板 v2 — shadcn/zinc 风格
// 布局：左侧边栏导航 + 顶部标题栏 + 统计卡片 + 渐变面积图 + 分页表格
// ============================================================

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScrollArea>
#include <QSizePolicy>
#include <QDateTime>
#include <QHeaderView>
#include <QJsonDocument>
#include <QStyle>
#include <QApplication>

// ---- zinc 色板 ----
namespace zinc {
static const QColor bg("#fafafa");        // zinc-50  页面背景
static const QColor panel("#ffffff");     // 卡片背景
static const QColor border("#e4e4e7");    // zinc-200 边框
static const QColor text("#18181b");      // zinc-900 主文字
static const QColor sub("#71717a");       // zinc-500 次文字
static const QColor faint("#a1a1aa");     // zinc-400 弱化
static const QColor hoverBg("#f4f4f5");   // zinc-100 悬停
static const QColor emerald("#047857");   // 上涨徽章文字
static const QColor emeraldBg("#d1fae5"); // 上涨徽章背景
static const QColor red("#b91c1c");       // 下跌徽章文字
static const QColor redBg("#fee2e2");     // 下跌徽章背景
} // namespace zinc

// ============================================================
// TrendAreaChart —— 自绘渐变面积图
// ============================================================
TrendAreaChart::TrendAreaChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void TrendAreaChart::setHistory(const QList<DataPoint> *history) { m_history = history; }

void TrendAreaChart::setExtractor(SeriesRole role,
                                  const std::function<double(const DataPoint &)> &fn)
{
    if (role == SeriesRole::Primary) m_primary = fn;
    else m_secondary = fn;
}

void TrendAreaChart::setSeriesName(SeriesRole role, const QString &name)
{
    if (role == SeriesRole::Primary) m_primaryName = name;
    else m_secondaryName = name;
}

void TrendAreaChart::setYRange(double lo, double hi) { m_yLo = lo; m_yHi = hi; }
void TrendAreaChart::setMaxPoints(int n) { m_maxPoints = qMax(2, n); }
void TrendAreaChart::refresh() { update(); }

double TrendAreaChart::valueAt(SeriesRole role, const DataPoint &dp) const
{
    auto &fn = (role == SeriesRole::Primary) ? m_primary : m_secondary;
    return fn ? fn(dp) : 0.0;
}

void TrendAreaChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), zinc::panel);

    const int padL = 14, padR = 14, padT = 14, padB = 26;
    int cw = width() - padL - padR;
    int ch = height() - padT - padB;
    if (cw <= 0 || ch <= 0) return;

    // 取最近 maxPoints 个采样点
    QList<DataPoint> pts;
    if (m_history) {
        int start = qMax(0, m_history->size() - m_maxPoints);
        pts = m_history->mid(start);
    }
    if (pts.size() < 2 || !m_primary) {
        p.setPen(zinc::faint);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待数据…"));
        return;
    }

    // Y 轴范围（固定或自适应）
    double yLo = m_yLo, yHi = 100;
    if (m_yHi <= 0) {
        yHi = 1;
        for (const auto &dp : pts) {
            yHi = qMax(yHi, valueAt(SeriesRole::Primary, dp));
            if (m_secondary) yHi = qMax(yHi, valueAt(SeriesRole::Secondary, dp));
        }
        yHi *= 1.15;
        yLo = 0;
    }

    auto X = [&](int i) { return padL + (double)i / (pts.size() - 1) * cw; };
    auto Y = [&](double v) {
        return padT + ch - (v - yLo) / (yHi - yLo) * ch;
    };

    // 网格线（5 条，低透明度）
    p.setPen(QPen(QColor(24, 24, 27, 20), 1));
    for (int f = 0; f <= 4; ++f) {
        int y = padT + ch * f / 4;
        p.drawLine(padL, y, padL + cw, y);
    }

    // 绘制一条序列：渐变面积 + 线
    auto drawSeries = [&](SeriesRole role, const QColor &line, int fillAlpha, double width) {
        QPainterPath linePath, areaPath;
        areaPath.moveTo(X(0), padT + ch);
        for (int i = 0; i < pts.size(); ++i) {
            QPointF pt(X(i), Y(valueAt(role, pts[i])));
            if (i == 0) linePath.moveTo(pt);
            else linePath.lineTo(pt);
            areaPath.lineTo(pt);
        }
        areaPath.lineTo(X(pts.size() - 1), padT + ch);
        areaPath.closeSubpath();

        QLinearGradient g(0, padT, 0, padT + ch);
        QColor top = line; top.setAlpha(fillAlpha);
        QColor bottom = line; bottom.setAlpha(5);
        g.setColorAt(0.05, top);
        g.setColorAt(0.95, bottom);
        p.fillPath(areaPath, g);
        p.setPen(QPen(line, width));
        p.drawPath(linePath);
    };

    if (m_secondary) drawSeries(SeriesRole::Secondary, zinc::faint, 38, 1.5);
    drawSeries(SeriesRole::Primary, QColor(24, 24, 27), 76, 2.0);

    // X 轴时间标签（约 5 个）
    p.setPen(zinc::faint);
    p.setFont(font());
    int step = qMax(1, pts.size() / 5);
    for (int i = 0; i < pts.size(); i += step) {
        QString t = QDateTime::fromSecsSinceEpoch(pts[i].timestamp).toString("hh:mm:ss");
        QRectF r(X(i) - 40, height() - padB + 4, 80, 16);
        p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, t);
    }

    // 图例（右上角）
    {
        QFont f = font(); f.setPointSize(9);
        p.setFont(f);
        int lx = padL + cw - 150, ly = padT - 2;
        p.setPen(QPen(QColor(24, 24, 27), 2));
        p.drawLine(lx, ly + 6, lx + 14, ly + 6);
        p.setPen(zinc::sub);
        p.drawText(lx + 20, ly + 10, m_primaryName);
        if (m_secondary) {
            p.setPen(QPen(zinc::faint, 2));
            p.drawLine(lx + 80, ly + 6, lx + 94, ly + 6);
            p.setPen(zinc::sub);
            p.drawText(lx + 100, ly + 10, m_secondaryName);
        }
    }

    // hover 十字线与数值
    if (m_hoverIndex >= 0 && m_hoverIndex < pts.size()) {
        double hx = X(m_hoverIndex);
        p.setPen(QPen(QColor(24, 24, 27, 90), 1, Qt::DashLine));
        p.drawLine(hx, padT, hx, padT + ch);

        double v1 = valueAt(SeriesRole::Primary, pts[m_hoverIndex]);
        QString tip = QString("%1: %2").arg(m_primaryName).arg(v1, 0, 'f', 1);
        if (m_secondary) {
            tip += QString("  %1: %2")
                       .arg(m_secondaryName)
                       .arg(valueAt(SeriesRole::Secondary, pts[m_hoverIndex]), 0, 'f', 1);
        }
        QFont f = font(); f.setPointSize(9);
        p.setFont(f);
        QFontMetrics fm(f);
        QRect box = fm.boundingRect(tip).adjusted(-8, -4, 8, 4);
        box.moveTopLeft(QPointF(qMin(hx + 8, (double)width() - box.width() - 4), padT + 4).toPoint());
        p.setPen(zinc::border);
        p.setBrush(zinc::panel);
        p.drawRoundedRect(box, 6, 6);
        p.setPen(zinc::text);
        p.drawText(box, Qt::AlignCenter, tip);
    }
}

void TrendAreaChart::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_history || m_history->size() < 2) return;
    const int padL = 14, padR = 14;
    int cw = width() - padL - padR;
    int n = qMin(m_history->size(), m_maxPoints);
    double frac = (event->position().x() - padL) / qMax(1, cw);
    m_hoverIndex = qBound(0, (int)(frac * (n - 1) + 0.5), n - 1);
    update();
}

void TrendAreaChart::leaveEvent(QEvent *)
{
    m_hoverIndex = -1;
    update();
}

// ============================================================
// StatCard —— 统计卡片
// ============================================================
StatCard::StatCard(const QString &title, QWidget *parent) : QFrame(parent)
{
    setProperty("class", "card");
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 18, 20, 16);
    lay->setSpacing(4);

    // 第一行：标题 + 趋势徽章
    auto *row = new QHBoxLayout();
    m_title = new QLabel(title, this);
    m_title->setObjectName("cardTitle");
    m_badge = new QLabel(this);
    m_badge->setObjectName("badge");
    row->addWidget(m_title);
    row->addStretch();
    row->addWidget(m_badge);
    lay->addLayout(row);

    // 大数值
    m_value = new QLabel("--", this);
    m_value->setObjectName("cardValue");
    lay->addWidget(m_value);

    // 底部说明
    m_footer = new QLabel(this);
    m_footer->setObjectName("cardFooter");
    m_detail = new QLabel(this);
    m_detail->setObjectName("cardDetail");
    lay->addSpacing(8);
    lay->addWidget(m_footer);
    lay->addWidget(m_detail);
}

void StatCard::updateValue(const QString &value, const QString &badge,
                           bool trendUp, const QString &footer, const QString &detail)
{
    m_value->setText(value);
    m_badge->setText((trendUp ? QStringLiteral("▲ ") : QStringLiteral("▼ ")) + badge);
    m_badge->setStyleSheet(QString(
        "QLabel{background:%1;color:%2;border-radius:9px;padding:2px 8px;"
        "font-size:11px;font-weight:bold;}")
        .arg(trendUp ? zinc::emeraldBg.name() : zinc::redBg.name(),
             trendUp ? zinc::emerald.name() : zinc::red.name()));
    m_footer->setText(footer);
    m_detail->setText(detail);
}

// ============================================================
// MainWindow
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    setWindowTitle(QStringLiteral("系统性能监控看板"));
    resize(1280, 800);

    applyZincTheme();

    // ---- 根布局：侧边栏 + 主区域 ----
    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildSidebar());

    auto *mainCol = new QWidget(central);
    auto *mainLay = new QVBoxLayout(mainCol);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(buildHeader());

    pages = new QStackedWidget(mainCol);
    pages->setObjectName("pageStack");
    mainLay->addWidget(pages, 1);
    root->addWidget(mainCol, 1);
    setCentralWidget(central);

    // ---- 页面 ----
    pageIndex["overview"] = pages->addWidget(buildOverviewPage());
    pageIndex["cpu"] = pages->addWidget(buildCpuPage());
    pageIndex["mem"] = pages->addWidget(buildMemPage());
    pageIndex["net"] = pages->addWidget(buildNetPage());
    pageIndex["disk"] = pages->addWidget(buildDiskPage());
    pageIndex["softirq"] = pages->addWidget(buildSoftIrqPage());
    gotoPage("overview");

    // ---- 网络 ----
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onReplyFinished);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::fetchData);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::fetchData);
    timer->start(3000);

    fetchData();
}

MainWindow::~MainWindow() { delete ui; }

// ============================================================
// zinc 主题样式
// ============================================================
void MainWindow::applyZincTheme()
{
    setStyleSheet(QStringLiteral(R"(
        * { font-family: "Microsoft YaHei", "PingFang SC", "Segoe UI", sans-serif; }
        QMainWindow, QWidget { background: %1; color: %2; font-size: 13px; }

        /* ---- 侧边栏 ---- */
        QFrame#sidebar { background: %1; border-right: 1px solid %3; }
        QLabel#brandTitle { font-size: 13px; font-weight: bold; color: %2; }
        QLabel#sectionLabel { font-size: 11px; font-weight: bold; color: %5;
                              letter-spacing: 1px; padding-left: 10px; }
        QPushButton[class="navBtn"] {
            background: transparent; color: #52525b; text-align: left;
            border: none; border-radius: 6px; padding: 8px 12px; font-size: 13px;
        }
        QPushButton[class="navBtn"]:hover { background: %6; color: %2; }
        QPushButton[class="navBtn"][active="true"] { background: %2; color: white; }
        QLabel#userAvatar { background: #d4d4d8; color: #3f3f46; border-radius: 15px;
                            font-size: 11px; font-weight: bold; }
        QLabel#userName { font-size: 13px; font-weight: bold; color: %2; }
        QLabel#userMail { font-size: 11px; color: %5; }

        /* ---- 顶部栏 ---- */
        QFrame#header { background: %7; border-bottom: 1px solid %3; }
        QPushButton#menuBtn { background: transparent; border: none; border-radius: 6px;
                              padding: 6px 10px; font-size: 15px; color: #52525b; }
        QPushButton#menuBtn:hover { background: %6; }
        QLabel#pageTitle { font-size: 13px; font-weight: bold; color: %2; }
        QLabel#statusDot { border-radius: 4px; min-width: 8px; max-width: 8px;
                           min-height: 8px; max-height: 8px; background: #d4d4d8; }
        QLabel#statusLabel { font-size: 12px; color: %5; }
        QLabel#onlineBadge { background: %8; color: %9; border-radius: 10px;
                             padding: 3px 10px; font-size: 11px; font-weight: bold; }
        QComboBox#hostSelector {
            background: white; border: 1px solid %3; border-radius: 6px;
            padding: 5px 10px; color: %2; min-width: 140px;
        }
        QComboBox#hostSelector:hover { border-color: %5; }
        QComboBox#hostSelector::drop-down { border: none; width: 18px; }
        QComboBox#hostSelector::down-arrow {
            image: none; border-left: 4px solid transparent;
            border-right: 4px solid transparent; border-top: 5px solid %5;
            margin-right: 8px;
        }
        QComboBox#hostSelector QAbstractItemView {
            background: white; border: 1px solid %3; color: %2;
            selection-background-color: %2; selection-color: white;
        }
        QPushButton#refreshBtn {
            background: %2; color: white; border: none; border-radius: 6px;
            padding: 6px 16px; font-weight: bold;
        }
        QPushButton#refreshBtn:hover { background: #3f3f46; }

        /* ---- 卡片 ---- */
        QFrame[class="card"] { background: %7; border: 1px solid %3; border-radius: 12px; }
        QLabel#cardTitle { font-size: 13px; color: %5; background: transparent; }
        QLabel#cardValue { font-size: 26px; font-weight: bold; color: %2;
                           background: transparent; }
        QLabel#cardFooter { font-size: 12px; font-weight: bold; color: #3f3f46;
                            background: transparent; }
        QLabel#cardDetail { font-size: 12px; color: %5; background: transparent; }
        QLabel#chartTitle { font-size: 14px; font-weight: bold; color: %2;
                            background: transparent; }
        QLabel#chartSubtitle { font-size: 12px; color: %5; background: transparent; }

        /* ---- 分段按钮（时间范围） ---- */
        QFrame#segmentBox { background: white; border: 1px solid %3; border-radius: 8px; }
        QPushButton[class="segBtn"] {
            background: transparent; border: none; border-radius: 6px;
            padding: 5px 12px; font-size: 12px; color: #52525b;
        }
        QPushButton[class="segBtn"]:hover { background: %6; }
        QPushButton[class="segBtn"][active="true"] { background: %2; color: white; }

        /* ---- 表格 ---- */
        QTableWidget {
            background: %7; alternate-background-color: #fafafa;
            border: none; gridline-color: #f4f4f5; color: %2; font-size: 13px;
        }
        QHeaderView::section {
            background: %7; color: %5; border: none;
            border-bottom: 1px solid %3; padding: 10px 14px; font-weight: normal;
        }
        QTableWidget::item { padding: 4px 14px; border: none; }
        QTableWidget::item:selected { background: %6; color: %2; }

        /* ---- 分页按钮 ---- */
        QPushButton[class="pageBtn"] {
            background: transparent; border: none; border-radius: 6px;
            padding: 5px 9px; color: #52525b; font-size: 14px;
        }
        QPushButton[class="pageBtn"]:hover { background: %6; }
        QPushButton[class="pageBtn"]:disabled { color: #d4d4d8; }
        QLabel#pageInfo { font-size: 12px; color: %5; background: transparent; }

        /* ---- 滚动条 ---- */
        QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: #d4d4d8; border-radius: 4px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: %5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
        QScrollBar::handle:horizontal { background: #d4d4d8; border-radius: 4px; min-width: 30px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        QScrollArea { border: none; background: %1; }
    )")
        .arg(zinc::bg.name(), zinc::text.name(), zinc::border.name(),
             QString(), zinc::sub.name(), zinc::hoverBg.name(),
             zinc::panel.name(), zinc::emeraldBg.name(), zinc::emerald.name()));
}

// ============================================================
// 侧边栏
// ============================================================
QPushButton *MainWindow::addNavItem(const QString &title, const QString &pageId)
{
    auto *btn = new QPushButton(title, sidebar);
    btn->setProperty("class", "navBtn");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    navGroup->addButton(btn);
    navButtons[pageId] = btn;
    connect(btn, &QPushButton::clicked, this, [this, pageId] { gotoPage(pageId); });
    return btn;
}

QWidget *MainWindow::buildSidebar()
{
    sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    auto *lay = new QVBoxLayout(sidebar);
    lay->setContentsMargins(12, 0, 12, 12);
    lay->setSpacing(4);

    // 品牌区
    auto *brandRow = new QHBoxLayout();
    auto *logo = new QLabel(sidebar);
    logo->setFixedSize(20, 20);
    logo->setStyleSheet("background:#18181b;border-radius:5px;");
    auto *brandTitle = new QLabel(QStringLiteral("Perf Monitor"), sidebar);
    brandTitle->setObjectName("brandTitle");
    brandRow->addWidget(logo);
    brandRow->addWidget(brandTitle);
    brandRow->addStretch();
    auto *brandWrap = new QWidget(sidebar);
    brandWrap->setFixedHeight(48);
    brandWrap->setLayout(brandRow);
    brandWrap->setStyleSheet("background:transparent;");
    lay->addWidget(brandWrap);

    // 主导航
    navGroup = new QButtonGroup(this);
    lay->addWidget(addNavItem(QStringLiteral("◻  总览"), "overview"));
    lay->addWidget(addNavItem(QStringLiteral("▤  CPU"), "cpu"));
    lay->addWidget(addNavItem(QStringLiteral("◧  内存"), "mem"));
    lay->addWidget(addNavItem(QStringLiteral("⇄  网络"), "net"));
    lay->addWidget(addNavItem(QStringLiteral("▣  磁盘"), "disk"));
    lay->addWidget(addNavItem(QStringLiteral("⚡  软中断"), "softirq"));

    // 分组标题
    lay->addSpacing(18);
    auto *secLabel = new QLabel(QStringLiteral("DETAILS"), sidebar);
    secLabel->setObjectName("sectionLabel");
    lay->addWidget(secLabel);
    lay->addStretch();

    // 底部用户区
    auto *userRow = new QHBoxLayout();
    auto *avatar = new QLabel("PM", sidebar);
    avatar->setObjectName("userAvatar");
    avatar->setFixedSize(30, 30);
    avatar->setAlignment(Qt::AlignCenter);
    auto *userCol = new QVBoxLayout();
    auto *userName = new QLabel(QStringLiteral("监控中心"), sidebar);
    userName->setObjectName("userName");
    auto *userMail = new QLabel(QStringLiteral("manager:50052"), sidebar);
    userMail->setObjectName("userMail");
    userCol->addWidget(userName);
    userCol->addWidget(userMail);
    userRow->addWidget(avatar);
    userRow->addLayout(userCol);
    userRow->addStretch();
    lay->addLayout(userRow);

    return sidebar;
}

// ============================================================
// 顶部栏
// ============================================================
QWidget *MainWindow::buildHeader()
{
    auto *header = new QFrame(this);
    header->setObjectName("header");
    header->setFixedHeight(48);
    auto *lay = new QHBoxLayout(header);
    lay->setContentsMargins(16, 0, 16, 0);
    lay->setSpacing(8);

    menuBtn = new QPushButton(QStringLiteral("☰"), header);
    menuBtn->setObjectName("menuBtn");
    connect(menuBtn, &QPushButton::clicked, this,
            [this] { sidebar->setVisible(!sidebar->isVisible()); });
    lay->addWidget(menuBtn);

    auto *sep = new QFrame(header);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color:#e4e4e7;");
    sep->setFixedHeight(16);
    lay->addWidget(sep);

    titleLabel = new QLabel(QStringLiteral("总览"), header);
    titleLabel->setObjectName("pageTitle");
    lay->addWidget(titleLabel);

    lay->addSpacing(12);
    statusDot = new QLabel(header);
    statusDot->setObjectName("statusDot");
    statusLabel = new QLabel(QStringLiteral("初始化中…"), header);
    statusLabel->setObjectName("statusLabel");
    lay->addWidget(statusDot);
    lay->addWidget(statusLabel);

    lay->addStretch();

    onlineBadge = new QLabel(QStringLiteral("● 在线 0 台"), header);
    onlineBadge->setObjectName("onlineBadge");
    lay->addWidget(onlineBadge);

    hostSelector = new QComboBox(header);
    hostSelector->setObjectName("hostSelector");
    connect(hostSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onHostSelected);
    lay->addWidget(hostSelector);

    refreshBtn = new QPushButton(QStringLiteral("刷新"), header);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setCursor(Qt::PointingHandCursor);
    lay->addWidget(refreshBtn);

    return header;
}

// ============================================================
// 卡片包装辅助
// ============================================================
QFrame *MainWindow::wrapInCard(QWidget *content, const QString &title,
                               const QString &subtitle, QWidget *headerRight)
{
    auto *card = new QFrame(this);
    card->setProperty("class", "card");
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(12);

    if (!title.isEmpty()) {
        auto *headRow = new QHBoxLayout();
        auto *col = new QVBoxLayout();
        auto *t = new QLabel(title, card);
        t->setObjectName("chartTitle");
        col->addWidget(t);
        if (!subtitle.isEmpty()) {
            auto *s = new QLabel(subtitle, card);
            s->setObjectName("chartSubtitle");
            col->addWidget(s);
        }
        headRow->addLayout(col);
        headRow->addStretch();
        if (headerRight) headRow->addWidget(headerRight);
        lay->addLayout(headRow);
    }
    lay->addWidget(content);
    return card;
}

QTableWidget *MainWindow::makeTable(const QStringList &headers)
{
    auto *table = new QTableWidget(this);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    return table;
}

void MainWindow::gotoPage(const QString &pageId)
{
    if (!pageIndex.contains(pageId)) return;
    pages->setCurrentIndex(pageIndex[pageId]);

    static const QMap<QString, QString> titles = {
        {"overview", QStringLiteral("总览")},
        {"cpu", QStringLiteral("CPU 详情")},
        {"mem", QStringLiteral("内存详情")},
        {"net", QStringLiteral("网络详情")},
        {"disk", QStringLiteral("磁盘详情")},
        {"softirq", QStringLiteral("软中断详情")},
    };
    titleLabel->setText(titles.value(pageId));

    for (auto it = navButtons.begin(); it != navButtons.end(); ++it) {
        it.value()->setProperty("active", it.key() == pageId);
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }
}

// ============================================================
// 总览页
// ============================================================
QWidget *MainWindow::buildOverviewPage()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget();
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(20);

    // ---- 4 个统计卡片 ----
    auto *cardGrid = new QGridLayout();
    cardGrid->setSpacing(16);
    scoreCard = new StatCard(QStringLiteral("健康评分"), content);
    cpuCard = new StatCard(QStringLiteral("CPU 使用率"), content);
    memCard = new StatCard(QStringLiteral("内存使用率"), content);
    loadCard = new StatCard(QStringLiteral("系统负载 (1min)"), content);
    cardGrid->addWidget(scoreCard, 0, 0);
    cardGrid->addWidget(cpuCard, 0, 1);
    cardGrid->addWidget(memCard, 0, 2);
    cardGrid->addWidget(loadCard, 0, 3);
    lay->addLayout(cardGrid);

    // ---- CPU/内存趋势图卡片（带时间范围切换） ----
    auto *segBox = new QFrame(content);
    segBox->setObjectName("segmentBox");
    auto *segLay = new QHBoxLayout(segBox);
    segLay->setContentsMargins(3, 3, 3, 3);
    segLay->setSpacing(2);
    rangeGroup = new QButtonGroup(this);
    const struct { QString text; int points; } ranges[] = {
        {QStringLiteral("3 分钟"), 60}, {QStringLiteral("1 分钟"), 20}, {QStringLiteral("30 秒"), 10},
    };
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton(ranges[i].text, segBox);
        b->setProperty("class", "segBtn");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        rangeGroup->addButton(b, ranges[i].points);
        if (i == 0) b->setChecked(true);
        segLay->addWidget(b);
    }
    connect(rangeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &MainWindow::onRangeChanged);

    cpuTrendChart = new TrendAreaChart(content);
    cpuTrendChart->setMinimumHeight(240);
    cpuTrendChart->setYRange(0, 100);
    cpuTrendChart->setSeriesName(TrendAreaChart::SeriesRole::Primary, QStringLiteral("CPU"));
    cpuTrendChart->setSeriesName(TrendAreaChart::SeriesRole::Secondary, QStringLiteral("内存"));
    cpuTrendChart->setExtractor(TrendAreaChart::SeriesRole::Primary,
                                [](const DataPoint &dp) { return dp.cpuPercent; });
    cpuTrendChart->setExtractor(TrendAreaChart::SeriesRole::Secondary,
                                [](const DataPoint &dp) { return dp.memUsedPercent; });
    lay->addWidget(wrapInCard(cpuTrendChart,
                              QStringLiteral("资源使用趋势"),
                              QStringLiteral("当前主机 CPU / 内存使用率 (%)"), segBox));

    // ---- 网络 + 磁盘趋势 ----
    auto *chartRow = new QHBoxLayout();
    chartRow->setSpacing(16);

    netTrendChart = new TrendAreaChart(content);
    netTrendChart->setMinimumHeight(200);
    netTrendChart->setSeriesName(TrendAreaChart::SeriesRole::Primary, QStringLiteral("接收"));
    netTrendChart->setSeriesName(TrendAreaChart::SeriesRole::Secondary, QStringLiteral("发送"));
    netTrendChart->setExtractor(TrendAreaChart::SeriesRole::Primary,
                                [](const DataPoint &dp) { return dp.netInRate; });
    netTrendChart->setExtractor(TrendAreaChart::SeriesRole::Secondary,
                                [](const DataPoint &dp) { return dp.netOutRate; });
    chartRow->addWidget(wrapInCard(netTrendChart,
                                   QStringLiteral("网络收发趋势"),
                                   QStringLiteral("KB/s")), 1);

    diskTrendChart = new TrendAreaChart(content);
    diskTrendChart->setMinimumHeight(200);
    diskTrendChart->setYRange(0, 100);
    diskTrendChart->setSeriesName(TrendAreaChart::SeriesRole::Primary, QStringLiteral("磁盘利用率"));
    diskTrendChart->setExtractor(TrendAreaChart::SeriesRole::Primary,
                                 [](const DataPoint &dp) { return dp.diskUtil; });
    chartRow->addWidget(wrapInCard(diskTrendChart,
                                   QStringLiteral("磁盘利用率趋势"),
                                   QStringLiteral("所有磁盘中最大利用率 (%)")), 1);
    lay->addLayout(chartRow);

    // ---- 主机状态表（分页） ----
    hostsTable = makeTable({QStringLiteral("主机"), QStringLiteral("健康评分"),
                            QStringLiteral("CPU"), QStringLiteral("内存"),
                            QStringLiteral("状态")});
    hostsTable->setMinimumHeight(240);

    // 分页栏
    auto *pageBar = new QHBoxLayout();
    pageInfoLabel = new QLabel(content);
    pageInfoLabel->setObjectName("pageInfo");
    pageBar->addWidget(pageInfoLabel);
    pageBar->addStretch();
    auto mkPageBtn = [&](const QString &text) {
        auto *b = new QPushButton(text, content);
        b->setProperty("class", "pageBtn");
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, &MainWindow::onPageButton);
        pageBar->addWidget(b);
        return b;
    };
    pageFirst = mkPageBtn(QStringLiteral("«"));
    pagePrev = mkPageBtn(QStringLiteral("‹"));
    pageNext = mkPageBtn(QStringLiteral("›"));
    pageLast = mkPageBtn(QStringLiteral("»"));

    auto *hostsCard = wrapInCard(hostsTable,
                                 QStringLiteral("主机状态"),
                                 QStringLiteral("所有在线主机实时指标"));
    static_cast<QVBoxLayout *>(hostsCard->layout())->addLayout(pageBar);
    lay->addWidget(hostsCard);

    scroll->setWidget(content);
    outer->addWidget(scroll);
    return page;
}

// ============================================================
// 详情页
// ============================================================
QWidget *MainWindow::buildCpuPage()
{
    cpuTable = makeTable({QStringLiteral("CPU"), QStringLiteral("总使用率"),
                          QStringLiteral("用户态"), QStringLiteral("内核态"),
                          QStringLiteral("Nice"), QStringLiteral("空闲"),
                          QStringLiteral("I/O等待"), QStringLiteral("硬中断"),
                          QStringLiteral("软中断"), QStringLiteral("状态")});
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addWidget(wrapInCard(cpuTable, QStringLiteral("CPU 核心明细"),
                              QStringLiteral("每个核心的时间占比")));
    return page;
}

QWidget *MainWindow::buildMemPage()
{
    memTable = makeTable({QStringLiteral("指标"), QStringLiteral("值 (GB)"),
                          QStringLiteral("指标"), QStringLiteral("值 (GB)")});
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addWidget(wrapInCard(memTable, QStringLiteral("内存明细"),
                              QStringLiteral("/proc/meminfo 关键指标")));
    return page;
}

QWidget *MainWindow::buildNetPage()
{
    netTable = makeTable({QStringLiteral("网卡"), QStringLiteral("接收(KB/s)"),
                          QStringLiteral("发送(KB/s)"), QStringLiteral("收包(pkt/s)"),
                          QStringLiteral("发包(pkt/s)"), QStringLiteral("接收错误"),
                          QStringLiteral("发送错误"), QStringLiteral("丢弃"),
                          QStringLiteral("状态")});
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addWidget(wrapInCard(netTable, QStringLiteral("网卡明细"),
                              QStringLiteral("每个网卡的速率与错误计数")));
    return page;
}

QWidget *MainWindow::buildDiskPage()
{
    diskTable = makeTable({QStringLiteral("磁盘"), QStringLiteral("读吞吐(KB/s)"),
                           QStringLiteral("写吞吐(KB/s)"), QStringLiteral("读IOPS"),
                           QStringLiteral("写IOPS"), QStringLiteral("读延迟(ms)"),
                           QStringLiteral("写延迟(ms)"), QStringLiteral("利用率"),
                           QStringLiteral("状态")});
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addWidget(wrapInCard(diskTable, QStringLiteral("磁盘明细"),
                              QStringLiteral("/proc/diskstats 速率与延迟")));
    return page;
}

QWidget *MainWindow::buildSoftIrqPage()
{
    softIrqTable = makeTable({QStringLiteral("CPU"), QStringLiteral("HI"),
                              QStringLiteral("TIMER"), QStringLiteral("NET_TX"),
                              QStringLiteral("NET_RX"), QStringLiteral("BLOCK"),
                              QStringLiteral("IRQ_POLL"), QStringLiteral("TASKLET"),
                              QStringLiteral("SCHED"), QStringLiteral("HRTIMER"),
                              QStringLiteral("RCU")});
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->addWidget(wrapInCard(softIrqTable, QStringLiteral("软中断明细"),
                              QStringLiteral("/proc/softirqs 每 CPU 累计计数")));
    return page;
}

// ============================================================
// 数据获取
// ============================================================
void MainWindow::fetchData()
{
    networkManager->get(QNetworkRequest(QUrl(API_URL)));
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        setStatus(QStringLiteral("网络错误: ") + reply->errorString(), false);
        reply->deleteLater();
        return;
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        setStatus(QStringLiteral("数据格式错误"), false);
        return;
    }

    latestArray = doc.array();
    if (latestArray.isEmpty()) {
        setStatus(QStringLiteral("无数据: 等待 Worker 推送…"), false);
        onlineBadge->setText(QStringLiteral("● 在线 0 台"));
        hostSelector->blockSignals(true);
        hostSelector->clear();
        hostSelector->blockSignals(false);
        return;
    }

    onlineBadge->setText(QStringLiteral("● 在线 %1 台").arg(latestArray.size()));

    // 追加历史数据
    for (const auto &item : latestArray)
        appendData(item.toObject());

    // 更新主机下拉框（保持当前选中）
    QString currentSelected = hostSelector->currentText();
    hostSelector->blockSignals(true);
    hostSelector->clear();
    QStringList names;
    for (const auto &item : latestArray)
        names << item.toObject()["hostname"].toString();
    hostSelector->addItems(names);
    if (!currentSelected.isEmpty() && names.contains(currentSelected))
        hostSelector->setCurrentText(currentSelected);
    else
        hostSelector->setCurrentIndex(0);
    hostSelector->blockSignals(false);

    // 更新主机状态表 + 当前主机视图
    onPageButton();
    updateAllForCurrentHost();
    setStatus(QStringLiteral("已更新 %1")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss")), true);
}

void MainWindow::appendData(const QJsonObject &obj)
{
    DataPoint dp;
    dp.timestamp = QDateTime::currentSecsSinceEpoch();

    auto cpuStats = obj["cpu_stats"].toArray();
    dp.cpuPercent = cpuStats.isEmpty() ? 0 : cpuStats[0].toObject()["cpu_percent"].toDouble();
    dp.memUsedPercent = obj["mem_info"].toObject()["used_percent"].toDouble();

    auto netInfos = obj["net_infos"].toArray();
    if (!netInfos.isEmpty()) {
        auto net = netInfos[0].toObject();
        dp.netInRate = net["rcv_rate"].toDouble();
        dp.netOutRate = net["send_rate"].toDouble();
    }

    double maxUtil = 0;
    for (const auto &item : obj["disk_infos"].toArray())
        maxUtil = qMax(maxUtil, item.toObject()["util_percent"].toDouble());
    dp.diskUtil = maxUtil;

    QString host = obj["hostname"].toString();
    if (host.isEmpty()) return;
    historyMap[host].append(dp);
    if (historyMap[host].size() > MAX_HISTORY)
        historyMap[host].removeFirst();
}

void MainWindow::onHostSelected(int index)
{
    if (index < 0 || index >= latestArray.size()) return;
    currentHost = latestArray[index].toObject()["hostname"].toString();
    updateAllForCurrentHost();
}

void MainWindow::updateAllForCurrentHost()
{
    if (latestArray.isEmpty()) return;
    int index = hostSelector->currentIndex();
    if (index < 0 || index >= latestArray.size()) return;

    QJsonObject obj = latestArray[index].toObject();
    currentHost = obj["hostname"].toString();

    updateStatCards(obj);

    // 绑定历史数据并刷新趋势图
    auto &history = historyMap[currentHost];
    cpuTrendChart->setHistory(&history);
    netTrendChart->setHistory(&history);
    diskTrendChart->setHistory(&history);
    refreshCharts();

    updateCpuTable(obj["cpu_stats"].toArray());
    updateMemTable(obj["mem_info"].toObject());
    updateNetTable(obj["net_infos"].toArray());
    updateDiskTable(obj["disk_infos"].toArray());
    updateSoftIrqTable(obj["soft_irqs"].toArray());
}

// ============================================================
// 统计卡片
// ============================================================
QColor MainWindow::levelColor(double v, double warn, double crit)
{
    if (v >= crit) return zinc::red;
    if (v >= warn) return QColor("#b45309");
    return zinc::emerald;
}

void MainWindow::updateStatCards(const QJsonObject &obj)
{
    double score = obj["score"].toDouble();
    auto cpuStats = obj["cpu_stats"].toArray();
    double cpu = cpuStats.isEmpty() ? 0 : cpuStats[0].toObject()["cpu_percent"].toDouble();
    double mem = obj["mem_info"].toObject()["used_percent"].toDouble();
    double load = obj["cpu_load"].toObject()["load_avg_1"].toDouble();

    auto badgeText = [](double cur, double prev) {
        if (prev < 0) return QStringLiteral("--");
        double d = cur - prev;
        return QString("%1%2").arg(d >= 0 ? "+" : "").arg(d, 0, 'f', 1);
    };

    // 评分：越高越好，上升为绿
    scoreCard->updateValue(QString::number(score, 'f', 1),
                           badgeText(score, prevScore),
                           prevScore < 0 || score >= prevScore,
                           score >= 80 ? QStringLiteral("运行状态良好")
                                       : QStringLiteral("存在一定负载压力"),
                           QStringLiteral("多维度加权综合评分"));

    // CPU：越低越好，上升为红
    cpuCard->updateValue(QString("%1%").arg(cpu, 0, 'f', 1),
                         badgeText(cpu, prevCpu),
                         prevCpu >= 0 && cpu <= prevCpu,
                         QStringLiteral("使用率 %1%2")
                             .arg(cpu >= 80 ? QStringLiteral("偏高，需关注") : QStringLiteral("正常范围")),
                         QStringLiteral("主 CPU 核心实时使用率"));

    // 内存
    memCard->updateValue(QString("%1%").arg(mem, 0, 'f', 1),
                         badgeText(mem, prevMem),
                         prevMem >= 0 && mem <= prevMem,
                         mem >= 90 ? QStringLiteral("内存紧张") : QStringLiteral("内存充足"),
                         QStringLiteral("已用 / 总内存占比"));

    // 负载
    loadCard->updateValue(QString::number(load, 'f', 2),
                          badgeText(load, prevLoad),
                          prevLoad >= 0 && load <= prevLoad,
                          QStringLiteral("1 分钟平均负载"),
                          QStringLiteral("load5: %1 / load15: %2")
                              .arg(obj["cpu_load"].toObject()["load_avg_5"].toDouble(), 0, 'f', 2)
                              .arg(obj["cpu_load"].toObject()["load_avg_15"].toDouble(), 0, 'f', 2));

    prevScore = score;
    prevCpu = cpu;
    prevMem = mem;
    prevLoad = load;
}

void MainWindow::refreshCharts()
{
    cpuTrendChart->refresh();
    netTrendChart->refresh();
    diskTrendChart->refresh();
}

void MainWindow::onRangeChanged(int points)
{
    cpuTrendChart->setMaxPoints(points);
    netTrendChart->setMaxPoints(points);
    diskTrendChart->setMaxPoints(points);

    for (auto *b : rangeGroup->buttons()) {
        b->setProperty("active", rangeGroup->id(b) == points);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
    refreshCharts();
}

// ============================================================
// 主机状态表（分页）
// ============================================================
void MainWindow::onPageButton()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    int total = latestArray.size();
    int totalPages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);

    if (btn == pageFirst) hostPage = 0;
    else if (btn == pagePrev) hostPage = qMax(0, hostPage - 1);
    else if (btn == pageNext) hostPage = qMin(totalPages - 1, hostPage + 1);
    else if (btn == pageLast) hostPage = totalPages - 1;
    hostPage = qBound(0, hostPage, totalPages - 1);

    // 填表
    hostsTable->setRowCount(0);
    int start = hostPage * PAGE_SIZE;
    int end = qMin(start + PAGE_SIZE, total);
    for (int i = start; i < end; ++i) {
        auto obj = latestArray[i].toObject();
        int row = hostsTable->rowCount();
        hostsTable->insertRow(row);

        double score = obj["score"].toDouble();
        auto cpuStats = obj["cpu_stats"].toArray();
        double cpu = cpuStats.isEmpty() ? 0 : cpuStats[0].toObject()["cpu_percent"].toDouble();
        double mem = obj["mem_info"].toObject()["used_percent"].toDouble();

        auto set = [&](int col, const QString &text) {
            hostsTable->setItem(row, col, new QTableWidgetItem(text));
        };
        set(0, obj["hostname"].toString());
        set(1, QString::number(score, 'f', 1));
        set(2, QString("%1%").arg(cpu, 0, 'f', 1));
        set(3, QString("%1%").arg(mem, 0, 'f', 1));

        // 状态徽章
        auto *badge = new QLabel(QStringLiteral("● 在线"));
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(QString(
            "QLabel{background:%1;color:%2;border-radius:10px;padding:3px 10px;"
            "font-size:11px;font-weight:bold;border:none;}")
            .arg(zinc::emeraldBg.name(), zinc::emerald.name()));
        hostsTable->setCellWidget(row, 4, badge);

        // 评分着色
        if (auto *item = hostsTable->item(row, 1))
            item->setForeground(levelColor(100 - score, 20, 40));
    }

    pageInfoLabel->setText(total == 0
                               ? QStringLiteral("0 行")
                               : QStringLiteral("%1-%2 / 共 %3 行")
                                     .arg(start + 1).arg(end).arg(total));
    pageFirst->setDisabled(hostPage == 0);
    pagePrev->setDisabled(hostPage == 0);
    pageNext->setDisabled(hostPage >= totalPages - 1);
    pageLast->setDisabled(hostPage >= totalPages - 1);
}

// ============================================================
// 明细表格更新
// ============================================================
void MainWindow::setCellBadge(QTableWidget *table, int row, int col,
                              const QString &text, const QColor &color)
{
    auto *badge = new QLabel(text);
    badge->setAlignment(Qt::AlignCenter);
    QColor bg = color; bg.setAlpha(38);
    badge->setStyleSheet(QString(
        "QLabel{background:%1;color:%2;border-radius:10px;padding:3px 10px;"
        "font-size:11px;font-weight:bold;border:none;}")
        .arg(bg.name(), color.name()));
    table->setCellWidget(row, col, badge);
}

void MainWindow::updateCpuTable(const QJsonArray &cpuStats)
{
    cpuTable->setRowCount(0);
    for (const auto &item : cpuStats) {
        auto obj = item.toObject();
        if (obj["cpu_name"].toString().isEmpty()) continue;
        int row = cpuTable->rowCount();
        cpuTable->insertRow(row);
        double pct = obj["cpu_percent"].toDouble();
        const char *keys[] = {"cpu_name", "cpu_percent", "usr_percent", "system_percent",
                              "nice_percent", "idle_percent", "io_wait_percent",
                              "irq_percent", "soft_irq_percent"};
        for (int c = 0; c < 9; ++c) {
            QString text = (c == 0) ? obj[keys[c]].toString()
                                    : QString::number(obj[keys[c]].toDouble(), 'f', 2) + "%";
            cpuTable->setItem(row, c, new QTableWidgetItem(text));
        }
        QColor color = pct >= 90 ? zinc::red : (pct >= 70 ? QColor("#b45309") : zinc::emerald);
        setCellBadge(cpuTable, row, 9,
                     pct >= 90 ? QStringLiteral("高负载") : (pct >= 70 ? QStringLiteral("偏忙") : QStringLiteral("正常")),
                     color);
    }
}

void MainWindow::updateMemTable(const QJsonObject &memInfo)
{
    memTable->setRowCount(0);
    struct Field { QString name; double value; };
    QList<Field> fields = {
        {QStringLiteral("总内存"), memInfo["total"].toDouble()},
        {QStringLiteral("空闲内存"), memInfo["free"].toDouble()},
        {QStringLiteral("可用内存"), memInfo["avail"].toDouble()},
        {QStringLiteral("缓冲区"), memInfo["buffers"].toDouble()},
        {QStringLiteral("缓存"), memInfo["cached"].toDouble()},
        {QStringLiteral("交换缓存"), memInfo["swap_cached"].toDouble()},
        {QStringLiteral("活跃内存"), memInfo["active"].toDouble()},
        {QStringLiteral("非活跃内存"), memInfo["inactive"].toDouble()},
        {QStringLiteral("脏页"), memInfo["dirty"].toDouble()},
        {QStringLiteral("回写中"), memInfo["writeback"].toDouble()},
        {QStringLiteral("匿名页"), memInfo["anon_pages"].toDouble()},
        {QStringLiteral("映射内存"), memInfo["mapped"].toDouble()},
        {QStringLiteral("可回收Slab"), memInfo["kreclaimable"].toDouble()},
        {QStringLiteral("不可回收Slab"), memInfo["sunreclaim"].toDouble()},
    };
    for (int i = 0; i < fields.size(); i += 2) {
        int row = memTable->rowCount();
        memTable->insertRow(row);
        memTable->setItem(row, 0, new QTableWidgetItem(fields[i].name));
        memTable->setItem(row, 1, new QTableWidgetItem(QString::number(fields[i].value, 'f', 2)));
        if (i + 1 < fields.size()) {
            memTable->setItem(row, 2, new QTableWidgetItem(fields[i + 1].name));
            memTable->setItem(row, 3, new QTableWidgetItem(QString::number(fields[i + 1].value, 'f', 2)));
        }
    }
}

void MainWindow::updateNetTable(const QJsonArray &netInfos)
{
    netTable->setRowCount(0);
    for (const auto &item : netInfos) {
        auto obj = item.toObject();
        int row = netTable->rowCount();
        netTable->insertRow(row);
        auto set = [&](int col, const QString &text) {
            netTable->setItem(row, col, new QTableWidgetItem(text));
        };
        set(0, obj["name"].toString());
        set(1, QString::number(obj["rcv_rate"].toDouble(), 'f', 2));
        set(2, QString::number(obj["send_rate"].toDouble(), 'f', 2));
        set(3, QString::number(obj["rcv_packets_rate"].toDouble(), 'f', 2));
        set(4, QString::number(obj["send_packets_rate"].toDouble(), 'f', 2));
        set(5, QString::number(obj["err_in"].toDouble()));
        set(6, QString::number(obj["err_out"].toDouble()));
        set(7, QString("%1/%2").arg(obj["drop_in"].toInt()).arg(obj["drop_out"].toInt()));

        double errs = obj["err_in"].toDouble() + obj["err_out"].toDouble();
        double drops = obj["drop_in"].toDouble() + obj["drop_out"].toDouble();
        if (errs > 0 || drops > 0)
            setCellBadge(netTable, row, 8, QStringLiteral("有异常"), zinc::red);
        else
            setCellBadge(netTable, row, 8, QStringLiteral("正常"), zinc::emerald);
    }
}

void MainWindow::updateDiskTable(const QJsonArray &diskInfos)
{
    diskTable->setRowCount(0);
    for (const auto &item : diskInfos) {
        auto obj = item.toObject();
        int row = diskTable->rowCount();
        diskTable->insertRow(row);
        auto set = [&](int col, const QString &text) {
            diskTable->setItem(row, col, new QTableWidgetItem(text));
        };
        double util = obj["util_percent"].toDouble();
        set(0, obj["name"].toString());
        set(1, QString::number(obj["read_bytes_per_sec"].toDouble() / 1024.0, 'f', 2));
        set(2, QString::number(obj["write_bytes_per_sec"].toDouble() / 1024.0, 'f', 2));
        set(3, QString::number(obj["read_iops"].toDouble(), 'f', 2));
        set(4, QString::number(obj["write_iops"].toDouble(), 'f', 2));
        set(5, QString::number(obj["avg_read_latency_ms"].toDouble(), 'f', 2));
        set(6, QString::number(obj["avg_write_latency_ms"].toDouble(), 'f', 2));
        set(7, QString::number(util, 'f', 2) + "%");

        QColor color = util >= 90 ? zinc::red : (util >= 70 ? QColor("#b45309") : zinc::emerald);
        setCellBadge(diskTable, row, 8,
                     util >= 90 ? QStringLiteral("繁忙") : (util >= 70 ? QStringLiteral("偏忙") : QStringLiteral("空闲")),
                     color);
    }
}

void MainWindow::updateSoftIrqTable(const QJsonArray &softIrqs)
{
    softIrqTable->setRowCount(0);
    const char *keys[] = {"cpu", "hi", "timer", "net_tx", "net_rx", "block",
                          "irq_poll", "tasklet", "sched", "hrtimer", "rcu"};
    for (const auto &item : softIrqs) {
        auto obj = item.toObject();
        int row = softIrqTable->rowCount();
        softIrqTable->insertRow(row);
        for (int c = 0; c < 11; ++c) {
            QString text = (c == 0) ? obj[keys[c]].toString()
                                    : QString::number(obj[keys[c]].toDouble(), 'f', 0);
            softIrqTable->setItem(row, c, new QTableWidgetItem(text));
        }
    }
}

void MainWindow::setStatus(const QString &msg, bool ok)
{
    statusLabel->setText(msg);
    statusDot->setStyleSheet(QString("background:%1;border-radius:4px;")
                                 .arg(ok ? "#10b981" : "#ef4444"));
}

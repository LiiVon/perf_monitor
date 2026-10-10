#include "mainwindow.h"
#include "ui_mainwindow.h"

// ============================================================
// 性能监控看板 — Modern Observability 风格
// 布局：左侧导航 + 工具栏 + KPI 卡片 + 交互式趋势图 + 主机明细表
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
#include <QIcon>
#include <QPixmap>
#include <QSettings>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

// ---- Theme tokens; updated atomically when the user switches appearance ----
namespace zinc {
static QColor bg("#f7f8fa");
static QColor panel("#ffffff");
static QColor surface2("#f3f4f6");
static QColor border("#e5e7eb");
static QColor borderStrong("#d1d5db");
static QColor text("#111827");
static QColor sub("#6b7280");
static QColor faint("#9ca3af");
static QColor hoverBg("#f3f4f6");
static QColor accent("#111827");
static QColor accentHover("#374151");
static QColor accentSoft("#f3f4f6");
static QColor onAccent("#ffffff");
static QColor emerald("#15803d");
static QColor emeraldBg("#ecfdf3");
static QColor red("#dc2626");
static QColor redBg("#fef2f2");
static QColor warning("#b45309");
static QColor warningBg("#fffbeb");
static QColor chartPrimary("#2563eb");
static QColor chartSecondary("#0f9f91");
static QColor chartGrid("#e9edf3");
static QColor chartCursor("#94a3b8");
static QColor tooltipBg("#ffffff");
static QColor tooltipText("#111827");
static QColor violet("#7c3aed");
static QColor violetBg("#f5f3ff");
} // namespace zinc

namespace {

QIcon makeNavIcon(const QString &pageId, const QColor &color)
{
    QPixmap canvas(20, 20);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (pageId == QStringLiteral("overview")) {
        painter.drawRoundedRect(QRectF(3, 3, 5.5, 5.5), 1.2, 1.2);
        painter.drawRoundedRect(QRectF(11.5, 3, 5.5, 5.5), 1.2, 1.2);
        painter.drawRoundedRect(QRectF(3, 11.5, 5.5, 5.5), 1.2, 1.2);
        painter.drawRoundedRect(QRectF(11.5, 11.5, 5.5, 5.5), 1.2, 1.2);
    } else if (pageId == QStringLiteral("cpu")) {
        painter.drawRoundedRect(QRectF(5, 5, 10, 10), 2, 2);
        painter.drawRoundedRect(QRectF(8, 8, 4, 4), 1, 1);
        for (int i = 0; i < 4; ++i) {
            const qreal offset = 6 + i * 2.7;
            painter.drawLine(QPointF(offset, 2.3), QPointF(offset, 4.3));
            painter.drawLine(QPointF(offset, 15.7), QPointF(offset, 17.7));
            painter.drawLine(QPointF(2.3, offset), QPointF(4.3, offset));
            painter.drawLine(QPointF(15.7, offset), QPointF(17.7, offset));
        }
    } else if (pageId == QStringLiteral("mem")) {
        painter.drawRoundedRect(QRectF(3, 4, 14, 5), 1.3, 1.3);
        painter.drawRoundedRect(QRectF(3, 11, 14, 5), 1.3, 1.3);
        for (int x : {6, 10, 14}) {
            painter.drawLine(QPointF(x, 5.3), QPointF(x, 7.7));
            painter.drawLine(QPointF(x, 12.3), QPointF(x, 14.7));
        }
    } else if (pageId == QStringLiteral("net")) {
        painter.drawLine(QPointF(3, 6.5), QPointF(16.5, 6.5));
        painter.drawLine(QPointF(13.5, 3.8), QPointF(16.5, 6.5));
        painter.drawLine(QPointF(13.5, 9.2), QPointF(16.5, 6.5));
        painter.drawLine(QPointF(17, 13.5), QPointF(3.5, 13.5));
        painter.drawLine(QPointF(6.5, 10.8), QPointF(3.5, 13.5));
        painter.drawLine(QPointF(6.5, 16.2), QPointF(3.5, 13.5));
    } else if (pageId == QStringLiteral("disk")) {
        painter.drawEllipse(QRectF(4, 3, 12, 5));
        painter.drawArc(QRectF(4, 5, 12, 5), 180 * 16, 180 * 16);
        painter.drawArc(QRectF(4, 9, 12, 5), 180 * 16, 180 * 16);
        painter.drawLine(QPointF(4, 5.5), QPointF(4, 13));
        painter.drawLine(QPointF(16, 5.5), QPointF(16, 13));
    } else {
        QPainterPath pulse;
        pulse.moveTo(2.5, 10.5);
        pulse.lineTo(6, 10.5);
        pulse.lineTo(8.4, 5);
        pulse.lineTo(11.2, 15.2);
        pulse.lineTo(13.7, 8.5);
        pulse.lineTo(16, 10.5);
        pulse.lineTo(18, 10.5);
        painter.drawPath(pulse);
    }

    return QIcon(canvas);
}

} // namespace

// ============================================================
// TrendAreaChart —— 自绘渐变面积图
// ============================================================
TrendAreaChart::TrendAreaChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(190);
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

    const int padL = 46, padR = 18, padT = 24, padB = 30;
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
        p.setFont(QFont(font().family(), 11));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("正在等待主机数据…"));
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

    // 轻量网格 + Y 轴刻度，兼顾阅读性与画面留白
    QFont tickFont = font();
    tickFont.setPointSize(9);
    p.setFont(tickFont);
    for (int f = 0; f <= 4; ++f) {
        const int y = padT + ch * f / 4;
        p.setPen(QPen(zinc::chartGrid, 1));
        p.drawLine(padL, y, padL + cw, y);

        const double tickValue = yHi - (yHi - yLo) * f / 4.0;
        p.setPen(zinc::sub);
        p.drawText(QRect(0, y - 9, padL - 10, 18),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(tickValue, 'f', 0));
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

    if (m_secondary) drawSeries(SeriesRole::Secondary, zinc::chartSecondary, 20, 1.8);
    drawSeries(SeriesRole::Primary, zinc::chartPrimary, 24, 2.3);

    // X 轴时间标签（约 5 个）
    QFont timeFont = font();
    timeFont.setPointSize(9);
    p.setFont(timeFont);
    p.setPen(zinc::sub);
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
        p.setPen(QPen(zinc::chartPrimary, 2));
        p.drawLine(lx, ly + 6, lx + 14, ly + 6);
        p.setPen(zinc::sub);
        p.drawText(lx + 20, ly + 10, m_primaryName);
        if (m_secondary) {
            p.setPen(QPen(zinc::chartSecondary, 2));
            p.drawLine(lx + 80, ly + 6, lx + 94, ly + 6);
            p.setPen(zinc::sub);
            p.drawText(lx + 100, ly + 10, m_secondaryName);
        }
    }

    // hover 十字线与数值
    if (m_hoverIndex >= 0 && m_hoverIndex < pts.size()) {
        double hx = X(m_hoverIndex);
        p.setPen(QPen(zinc::chartCursor, 1, Qt::DashLine));
        p.drawLine(hx, padT, hx, padT + ch);

        const QPointF primaryPoint(hx, Y(valueAt(SeriesRole::Primary, pts[m_hoverIndex])));
        p.setPen(QPen(QColor("#cbd5e4"), 2));
        p.setBrush(zinc::chartPrimary);
        p.drawEllipse(primaryPoint, 4.2, 4.2);
        if (m_secondary) {
            const QPointF secondaryPoint(hx, Y(valueAt(SeriesRole::Secondary, pts[m_hoverIndex])));
            p.setBrush(zinc::chartSecondary);
            p.drawEllipse(secondaryPoint, 3.7, 3.7);
        }

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
    const int padL = 46, padR = 18;
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
    setMinimumWidth(0);
    setMinimumHeight(154);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 19, 20, 17);
    lay->setSpacing(4);

    // 第一行：标题 + 趋势徽章
    auto *row = new QHBoxLayout();
    auto *metricIcon = new QLabel(this);
    metricIcon->setObjectName("metricIcon");
    metricIcon->setFixedSize(30, 30);
    metricIcon->setAlignment(Qt::AlignCenter);

    QString icon = QStringLiteral("·");
    QString tone = QStringLiteral("blue");
    if (title.contains(QStringLiteral("健康"))) {
        icon = QStringLiteral("✓");
        tone = QStringLiteral("green");
    } else if (title.contains(QStringLiteral("CPU"))) {
        icon = QStringLiteral("⌁");
        tone = QStringLiteral("blue");
    } else if (title.contains(QStringLiteral("内存"))) {
        icon = QStringLiteral("▦");
        tone = QStringLiteral("violet");
    } else {
        icon = QStringLiteral("↗");
        tone = QStringLiteral("amber");
    }
    metricIcon->setText(icon);
    metricIcon->setProperty("tone", tone);

    m_title = new QLabel(title, this);
    m_title->setObjectName("cardTitle");
    m_title->setMinimumWidth(0);
    m_title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_badge = new QLabel(this);
    m_badge->setObjectName("badge");
    row->addWidget(metricIcon);
    row->addSpacing(9);
    row->addWidget(m_title);
    row->addStretch();
    row->addWidget(m_badge);
    lay->addLayout(row);

    // 大数值
    m_value = new QLabel("--", this);
    m_value->setObjectName("cardValue");
    m_value->setMinimumWidth(0);
    m_value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    lay->addWidget(m_value);

    // 底部说明
    m_footer = new QLabel(this);
    m_footer->setObjectName("cardFooter");
    m_footer->setMinimumWidth(0);
    m_footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_footer->setWordWrap(true);
    m_detail = new QLabel(this);
    m_detail->setObjectName("cardDetail");
    m_detail->setMinimumWidth(0);
    m_detail->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_detail->setWordWrap(true);
    lay->addSpacing(8);
    lay->addWidget(m_footer);
    lay->addWidget(m_detail);
}

void StatCard::updateValue(const QString &value, const QString &badge,
                           bool trendUp, const QString &footer, const QString &detail)
{
    m_value->setText(value);
    const bool neutral = badge == QStringLiteral("--");
    m_badge->setText(neutral ? badge :
        (trendUp ? QStringLiteral("↗ ") : QStringLiteral("↘ ")) + badge);
    const QColor badgeBg = neutral ? zinc::surface2
                                   : (trendUp ? zinc::emeraldBg : zinc::redBg);
    const QColor badgeFg = neutral ? zinc::sub
                                   : (trendUp ? zinc::emerald : zinc::red);
    m_badge->setStyleSheet(QString(
        "QLabel{background:%1;color:%2;border-radius:8px;padding:3px 8px;"
        "font-size:10px;font-weight:650;}")
        .arg(badgeBg.name(), badgeFg.name()));
    m_footer->setText(footer);
    m_detail->setText(detail);
}

// ============================================================
// MainWindow
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    setWindowTitle(QStringLiteral("Perf Monitor | Server Observability"));
    QSettings settings(QStringLiteral("LiiVon"), QStringLiteral("PerfMonitor"));
    m_darkTheme = settings.value(QStringLiteral("appearance/darkTheme"), false).toBool();
    resize(1440, 900);
    setMinimumSize(820, 580);

    applyModernTheme();

    // ---- 根布局：侧边栏 + 主区域 ----
    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    central->setObjectName("appRoot");
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildSidebar());

    auto *mainCol = new QWidget(central);
    mainCol->setObjectName("mainContent");
    mainCol->setMinimumWidth(0);
    mainCol->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *mainLay = new QVBoxLayout(mainCol);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(buildHeader());

    pages = new QStackedWidget(mainCol);
    pages->setObjectName("pageStack");
    pages->setMinimumWidth(0);
    pages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    applyModernTheme(); // Re-apply now that all theme-aware controls exist.

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
// Modern Observability 主题样式
// ============================================================
void MainWindow::applyModernTheme()
{
    // Geist-inspired neutral surfaces; data series remain distinguishable and accessible.
    if (m_darkTheme) {
        zinc::bg = QColor("#0b0d11");
        zinc::panel = QColor("#11151b");
        zinc::surface2 = QColor("#171c24");
        zinc::border = QColor("#252b35");
        zinc::borderStrong = QColor("#343c49");
        zinc::text = QColor("#eceff4");
        zinc::sub = QColor("#9ba4b2");
        zinc::faint = QColor("#6d7787");
        zinc::hoverBg = QColor("#1c232e");
        zinc::accent = QColor("#e8ebf0");
        zinc::accentHover = QColor("#ffffff");
        zinc::accentSoft = QColor("#222a36");
        zinc::onAccent = QColor("#11151b");
        zinc::emerald = QColor("#4ade80");
        zinc::emeraldBg = QColor("#173326");
        zinc::red = QColor("#fb7185");
        zinc::redBg = QColor("#3b2028");
        zinc::warning = QColor("#fbbf24");
        zinc::warningBg = QColor("#392f1b");
        zinc::chartPrimary = QColor("#8ea2ff");
        zinc::chartSecondary = QColor("#45d6c2");
        zinc::chartGrid = QColor("#29313d");
        zinc::chartCursor = QColor("#68778f");
        zinc::tooltipBg = QColor("#171d26");
        zinc::tooltipText = QColor("#eceff4");
        zinc::violet = QColor("#c4a7ff");
        zinc::violetBg = QColor("#302640");
    } else {
        zinc::bg = QColor("#f7f8fa");
        zinc::panel = QColor("#ffffff");
        zinc::surface2 = QColor("#f3f4f6");
        zinc::border = QColor("#e5e7eb");
        zinc::borderStrong = QColor("#d1d5db");
        zinc::text = QColor("#111827");
        zinc::sub = QColor("#6b7280");
        zinc::faint = QColor("#9ca3af");
        zinc::hoverBg = QColor("#f3f4f6");
        zinc::accent = QColor("#111827");
        zinc::accentHover = QColor("#374151");
        zinc::accentSoft = QColor("#f3f4f6");
        zinc::onAccent = QColor("#ffffff");
        zinc::emerald = QColor("#15803d");
        zinc::emeraldBg = QColor("#ecfdf3");
        zinc::red = QColor("#dc2626");
        zinc::redBg = QColor("#fef2f2");
        zinc::warning = QColor("#b45309");
        zinc::warningBg = QColor("#fffbeb");
        zinc::chartPrimary = QColor("#2563eb");
        zinc::chartSecondary = QColor("#0f9f91");
        zinc::chartGrid = QColor("#e9edf3");
        zinc::chartCursor = QColor("#94a3b8");
        zinc::tooltipBg = QColor("#ffffff");
        zinc::tooltipText = QColor("#111827");
        zinc::violet = QColor("#7c3aed");
        zinc::violetBg = QColor("#f5f3ff");
    }

    QString css = QStringLiteral(R"THEME(
        * { font-family: "Inter", "Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", sans-serif; }
        QMainWindow { background: @@BG@@; }
        QWidget {
            color: @@TEXT@@; font-size: 13px; background: transparent;
            selection-background-color: @@ACCENT_SOFT@@; selection-color: @@TEXT@@;
        }
        QWidget#appRoot, QWidget#mainContent, QStackedWidget#pageStack,
        QWidget#pageContent, QScrollArea, QAbstractScrollArea { background: @@BG@@; }
        QScrollArea { border: none; }
        QScrollArea QWidget { background: transparent; }

        /* Sidebar */
        QFrame#sidebar { background: @@SIDEBAR@@; border-right: 1px solid @@BORDER@@; }
        QLabel#brandMark {
            background: @@ACCENT@@; color: @@ON_ACCENT@@; border-radius: 8px;
            font-size: 13px; font-weight: 800;
        }
        QLabel#brandTitle { font-size: 14px; font-weight: 700; color: @@TEXT@@; }
        QLabel#sectionLabel {
            color: @@FAINT@@; font-size: 10px; font-weight: 700;
            letter-spacing: 1.3px; padding: 12px 10px 7px 10px;
        }
        QPushButton[class="navBtn"] {
            background: transparent; color: @@MUTED@@; text-align: left;
            border: 1px solid transparent; border-left: 3px solid transparent;
            border-radius: 8px; padding: 9px 12px; font-size: 13px;
            font-weight: 500; min-height: 20px; spacing: 11px;
        }
        QPushButton[class="navBtn"]:hover {
            background: @@HOVER@@; color: @@TEXT@@;
        }
        QPushButton[class="navBtn"][active="true"] {
            background: @@ACCENT_SOFT@@; color: @@TEXT@@; font-weight: 650;
            border-left: 3px solid @@ACCENT@@; padding-left: 9px;
        }
        QLabel#userAvatar {
            background: @@ACCENT_SOFT@@; color: @@TEXT@@; border-radius: 16px;
            font-size: 10px; font-weight: 700;
        }
        QLabel#userName { color: @@TEXT@@; font-size: 12px; font-weight: 650; }
        QLabel#userMail { color: @@MUTED@@; font-size: 10px; }

        /* Header / tools */
        QFrame#header { background: @@PANEL@@; border-bottom: 1px solid @@BORDER@@; }
        QFrame#headerSeparator { color: @@BORDER_STRONG@@; }
        QPushButton#menuBtn, QPushButton#themeToggleBtn {
            color: @@MUTED@@; background: transparent; border: 1px solid transparent;
            border-radius: 8px; padding: 7px 10px; font-size: 12px; font-weight: 550;
        }
        QPushButton#menuBtn:hover, QPushButton#themeToggleBtn:hover {
            background: @@HOVER@@; color: @@TEXT@@;
        }
        QLabel#pageTitle { color: @@TEXT@@; font-size: 13px; font-weight: 650; }
        QLabel#statusDot {
            border-radius: 4px; min-width: 7px; max-width: 7px;
            min-height: 7px; max-height: 7px; background: @@FAINT@@;
        }
        QLabel#statusLabel { color: @@MUTED@@; font-size: 11px; }
        QLabel#onlineBadge {
            color: @@SUCCESS@@; background: @@SUCCESS_BG@@; border: 1px solid @@SUCCESS_BORDER@@;
            border-radius: 8px; padding: 6px 10px; font-size: 11px; font-weight: 650;
        }
        QComboBox#hostSelector {
            background: @@PANEL@@; border: 1px solid @@BORDER@@; border-radius: 8px;
            padding: 8px 11px; color: @@TEXT@@; min-width: 150px;
        }
        QComboBox#hostSelector:hover { border-color: @@BORDER_STRONG@@; }
        QComboBox#hostSelector:focus { border: 1px solid @@ACCENT@@; }
        QComboBox#hostSelector::drop-down { border: none; width: 22px; }
        QComboBox#hostSelector::down-arrow {
            image: none; border-left: 4px solid transparent;
            border-right: 4px solid transparent; border-top: 5px solid @@MUTED@@;
            margin-right: 8px;
        }
        QComboBox#hostSelector QAbstractItemView {
            background: @@PANEL@@; border: 1px solid @@BORDER@@; color: @@TEXT@@;
            selection-background-color: @@ACCENT_SOFT@@; selection-color: @@TEXT@@;
            outline: none; padding: 4px;
        }
        QPushButton#refreshBtn {
            background: @@ACCENT@@; color: @@ON_ACCENT@@; border: 1px solid @@ACCENT@@;
            border-radius: 8px; padding: 8px 14px; font-size: 12px; font-weight: 650;
        }
        QPushButton#refreshBtn:hover { background: @@ACCENT_HOVER@@; border-color: @@ACCENT_HOVER@@; }
        QPushButton#refreshBtn:pressed { background: @@ACCENT@@; }

        /* Metric and chart cards */
        QFrame[class="card"] {
            background: @@PANEL@@; border: 1px solid @@BORDER@@; border-radius: 12px;
        }
        QFrame[class="card"]:hover { border-color: @@BORDER_STRONG@@; }
        QLabel#metricIcon { border-radius: 8px; border: none; font-size: 13px; font-weight: 700; }
        QLabel#metricIcon[tone="green"] { background: @@SUCCESS_BG@@; color: @@SUCCESS@@; }
        QLabel#metricIcon[tone="blue"] { background: @@BLUE_BG@@; color: @@BLUE@@; }
        QLabel#metricIcon[tone="violet"] { background: @@VIOLET_BG@@; color: @@VIOLET@@; }
        QLabel#metricIcon[tone="amber"] { background: @@WARNING_BG@@; color: @@WARNING@@; }
        QLabel#cardTitle { font-size: 12px; font-weight: 550; color: @@MUTED@@; background: transparent; }
        QLabel#cardValue { font-size: 29px; font-weight: 720; color: @@TEXT@@; background: transparent; }
        QLabel#cardFooter { font-size: 11px; font-weight: 600; color: @@TEXT@@; background: transparent; }
        QLabel#cardDetail { font-size: 11px; color: @@MUTED@@; background: transparent; }
        QLabel#chartTitle { font-size: 13px; font-weight: 680; color: @@TEXT@@; background: transparent; }
        QLabel#chartSubtitle { font-size: 11px; color: @@MUTED@@; background: transparent; }
        QLabel#dashboardHeading { font-size: 23px; font-weight: 750; color: @@TEXT@@; background: transparent; }
        QLabel#dashboardSubtitle { font-size: 12px; color: @@MUTED@@; background: transparent; }
        QLabel#dashboardLive {
            color: @@SUCCESS@@; background: @@SUCCESS_BG@@; border: 1px solid @@SUCCESS_BORDER@@;
            border-radius: 8px; padding: 6px 10px; font-size: 11px; font-weight: 650;
        }

        /* Chart range switch */
        QFrame#segmentBox { background: @@SURFACE2@@; border: 1px solid @@BORDER@@; border-radius: 8px; }
        QPushButton[class="segBtn"] {
            background: transparent; color: @@MUTED@@; border: 1px solid transparent;
            border-radius: 6px; padding: 5px 10px; font-size: 11px;
        }
        QPushButton[class="segBtn"]:hover { background: @@HOVER@@; color: @@TEXT@@; }
        QPushButton[class="segBtn"][active="true"] {
            background: @@PANEL@@; color: @@TEXT@@; font-weight: 650; border: 1px solid @@BORDER@@;
        }

        /* Dense metric tables */
        QTableWidget {
            background: @@PANEL@@; alternate-background-color: @@ROW_ALT@@;
            border: none; gridline-color: @@GRID@@; color: @@TEXT@@; font-size: 12px;
            selection-background-color: @@ACCENT_SOFT@@; selection-color: @@TEXT@@;
            outline: none;
        }
        QHeaderView::section {
            background: @@TABLE_HEADER@@; color: @@MUTED@@; border: none;
            border-bottom: 1px solid @@BORDER@@; padding: 10px 12px;
            font-weight: 650; font-size: 11px;
        }
        QTableWidget::item { padding: 5px 12px; border: none; }
        QTableWidget::item:hover { background: @@HOVER@@; }
        QTableWidget::item:selected { background: @@ACCENT_SOFT@@; color: @@TEXT@@; }
        QTableWidget QTableCornerButton::section { background: @@TABLE_HEADER@@; border: none; }

        /* Scrollbars / tooltips */
        QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 1px; }
        QScrollBar::handle:vertical { background: @@SCROLL@@; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: @@BORDER_STRONG@@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 1px 4px; }
        QScrollBar::handle:horizontal { background: @@SCROLL@@; border-radius: 4px; min-width: 28px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QToolTip {
            background: @@TOOLTIP_BG@@; color: @@TOOLTIP_TEXT@@;
            border: 1px solid @@BORDER_STRONG@@; padding: 6px 8px;
        }
    )THEME");

    const QMap<QString, QString> tokens = {
        {"@@BG@@", zinc::bg.name()}, {"@@PANEL@@", zinc::panel.name()},
        {"@@SURFACE2@@", zinc::surface2.name()}, {"@@BORDER@@", zinc::border.name()},
        {"@@BORDER_STRONG@@", zinc::borderStrong.name()}, {"@@TEXT@@", zinc::text.name()},
        {"@@MUTED@@", zinc::sub.name()}, {"@@FAINT@@", zinc::faint.name()},
        {"@@HOVER@@", zinc::hoverBg.name()}, {"@@ACCENT@@", zinc::accent.name()},
        {"@@ACCENT_HOVER@@", zinc::accentHover.name()}, {"@@ACCENT_SOFT@@", zinc::accentSoft.name()},
        {"@@ON_ACCENT@@", zinc::onAccent.name()}, {"@@SUCCESS@@", zinc::emerald.name()},
        {"@@SUCCESS_BG@@", zinc::emeraldBg.name()}, {"@@SUCCESS_BORDER@@", m_darkTheme ? "#2b4b39" : "#ccebd7"},
        {"@@DANGER@@", zinc::red.name()}, {"@@DANGER_BG@@", zinc::redBg.name()},
        {"@@WARNING@@", zinc::warning.name()}, {"@@WARNING_BG@@", zinc::warningBg.name()},
        {"@@BLUE@@", m_darkTheme ? "#9db0ff" : "#2563eb"},
        {"@@BLUE_BG@@", m_darkTheme ? "#222a43" : "#eff6ff"},
        {"@@VIOLET@@", zinc::violet.name()}, {"@@VIOLET_BG@@", zinc::violetBg.name()},
        {"@@SIDEBAR@@", m_darkTheme ? "#0e1117" : "#ffffff"},
        {"@@TABLE_HEADER@@", m_darkTheme ? "#151a22" : "#f9fafb"},
        {"@@ROW_ALT@@", m_darkTheme ? "#131820" : "#fcfcfd"},
        {"@@GRID@@", zinc::chartGrid.name()}, {"@@SCROLL@@", m_darkTheme ? "#343d4b" : "#d1d5db"},
        {"@@TOOLTIP_BG@@", zinc::tooltipBg.name()}, {"@@TOOLTIP_TEXT@@", zinc::tooltipText.name()}
    };
    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
        css.replace(it.key(), it.value());

    setStyleSheet(css);

    if (themeToggleBtn) {
        themeToggleBtn->setText(m_darkTheme ? QStringLiteral("☼ 浅色") : QStringLiteral("◐ 深色"));
        themeToggleBtn->setToolTip(m_darkTheme
            ? QStringLiteral("切换到浅色主题") : QStringLiteral("切换到深色主题"));
    }

    for (auto it = navButtons.begin(); it != navButtons.end(); ++it) {
        const bool active = it.value()->property("active").toBool();
        it.value()->setIcon(makeNavIcon(it.key(), active ? zinc::accent : zinc::sub));
    }

    applyNativeTitleBarTheme();
}

void MainWindow::applyNativeTitleBarTheme()
{
#ifdef Q_OS_WIN
    // The Windows caption is a non-client area, so Qt style sheets cannot recolor it.
    // DWM dark-mode support is available on modern Windows 10/11 builds.
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const BOOL useDarkMode = m_darkTheme ? TRUE : FALSE;

    constexpr DWORD useImmersiveDarkMode = 20; // Windows 11 / current Windows 10
    const HRESULT darkModeResult = DwmSetWindowAttribute(
        hwnd, useImmersiveDarkMode, &useDarkMode, sizeof(useDarkMode));

    if (FAILED(darkModeResult)) {
        // Older Windows 10 builds used attribute 19.
        constexpr DWORD legacyUseImmersiveDarkMode = 19;
        DwmSetWindowAttribute(
            hwnd, legacyUseImmersiveDarkMode, &useDarkMode, sizeof(useDarkMode));
    }

    // Apply explicit caption and frame colors where supported (Windows 11).
    // Unsupported attributes are intentionally ignored for older Windows versions.
    const COLORREF captionColor = m_darkTheme
        ? RGB(17, 21, 27)      // #11151b
        : RGB(247, 248, 250);  // #f7f8fa
    const COLORREF titleTextColor = m_darkTheme
        ? RGB(236, 239, 244)   // #eceff4
        : RGB(17, 24, 39);     // #111827
    const COLORREF frameColor = m_darkTheme
        ? RGB(37, 43, 53)      // #252b35
        : RGB(229, 231, 235);  // #e5e7eb

    constexpr DWORD borderColorAttribute = 34;
    constexpr DWORD captionColorAttribute = 35;
    constexpr DWORD textColorAttribute = 36;
    DwmSetWindowAttribute(hwnd, captionColorAttribute, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(hwnd, textColorAttribute, &titleTextColor, sizeof(titleTextColor));
    DwmSetWindowAttribute(hwnd, borderColorAttribute, &frameColor, sizeof(frameColor));
#endif
}


// ============================================================
// Sidebar
// ============================================================
QPushButton *MainWindow::addNavItem(const QString &title, const QString &pageId)
{
    auto *btn = new QPushButton(title, sidebar);
    btn->setIcon(makeNavIcon(pageId, zinc::sub));
    btn->setIconSize(QSize(18, 18));
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
    sidebar->setFixedWidth(236);
    auto *lay = new QVBoxLayout(sidebar);
    lay->setContentsMargins(14, 8, 14, 14);
    lay->setSpacing(5);

    // 品牌区
    auto *brandRow = new QHBoxLayout();
    auto *logo = new QLabel(sidebar);
    logo->setFixedSize(28, 28);
    logo->setObjectName("brandMark");
    logo->setText(QStringLiteral("P"));
    logo->setAlignment(Qt::AlignCenter);
    auto *brandTitle = new QLabel(QStringLiteral("Perf Monitor"), sidebar);
    brandTitle->setObjectName("brandTitle");
    brandRow->addWidget(logo);
    brandRow->addSpacing(10);
    brandRow->addWidget(brandTitle);
    brandRow->addStretch();
    auto *brandWrap = new QWidget(sidebar);
    brandWrap->setFixedHeight(58);
    brandWrap->setLayout(brandRow);
    brandWrap->setStyleSheet("background:transparent;");
    lay->addWidget(brandWrap);

    // Main navigation: custom line icons keep the sidebar quiet and consistent.
    auto *section = new QLabel(QStringLiteral("MONITORING"), sidebar);
    section->setObjectName("sectionLabel");
    lay->addWidget(section);

    navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    lay->addWidget(addNavItem(QStringLiteral("总览"), "overview"));
    lay->addWidget(addNavItem(QStringLiteral("CPU 核心"), "cpu"));
    lay->addWidget(addNavItem(QStringLiteral("内存"), "mem"));
    lay->addWidget(addNavItem(QStringLiteral("网络"), "net"));
    lay->addWidget(addNavItem(QStringLiteral("磁盘"), "disk"));
    lay->addWidget(addNavItem(QStringLiteral("软中断"), "softirq"));

    lay->addStretch();

    // 底部用户区
    auto *userRow = new QHBoxLayout();
    auto *avatar = new QLabel("PM", sidebar);
    avatar->setObjectName("userAvatar");
    avatar->setFixedSize(32, 32);
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
    header->setFixedHeight(62);
    auto *lay = new QHBoxLayout(header);
    lay->setContentsMargins(22, 0, 24, 0);
    lay->setSpacing(10);

    menuBtn = new QPushButton(QStringLiteral("☰"), header);
    menuBtn->setObjectName("menuBtn");
    menuBtn->setFixedSize(36, 36);
    connect(menuBtn, &QPushButton::clicked, this,
            [this] { sidebar->setVisible(!sidebar->isVisible()); });
    lay->addWidget(menuBtn);

    auto *sep = new QFrame(header);
    sep->setObjectName("headerSeparator");
    sep->setFrameShape(QFrame::VLine);
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
    hostSelector->setMinimumHeight(34);
    connect(hostSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onHostSelected);
    lay->addWidget(hostSelector);

    themeToggleBtn = new QPushButton(header);
    themeToggleBtn->setObjectName("themeToggleBtn");
    themeToggleBtn->setCursor(Qt::PointingHandCursor);
    themeToggleBtn->setMinimumHeight(34);
    connect(themeToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    lay->addWidget(themeToggleBtn);

    refreshBtn = new QPushButton(QStringLiteral("刷新"), header);
    refreshBtn->setObjectName("refreshBtn");
    refreshBtn->setMinimumHeight(36);
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
    lay->setContentsMargins(22, 19, 22, 20);
    lay->setSpacing(14);

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
    table->setMinimumWidth(0);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setAlternatingRowColors(true);
    table->setMouseTracking(true);
    table->setFocusPolicy(Qt::NoFocus);
    table->setWordWrap(false);
    table->setCornerButtonEnabled(false);
    table->horizontalHeader()->setHighlightSections(false);
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
        const bool active = it.key() == pageId;
        it.value()->setProperty("active", active);
        it.value()->setIcon(makeNavIcon(it.key(), active ? zinc::accent : zinc::sub));
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }
}

void MainWindow::toggleTheme()
{
    m_darkTheme = !m_darkTheme;

    QSettings settings(QStringLiteral("LiiVon"), QStringLiteral("PerfMonitor"));
    settings.setValue(QStringLiteral("appearance/darkTheme"), m_darkTheme);

    applyModernTheme();

    if (!latestArray.isEmpty())
        updateAllForCurrentHost();
    else
        refreshCharts();
}

// ============================================================
// 总览页
// ============================================================
QWidget *MainWindow::buildOverviewPage()
{
    auto *page = new QWidget(this);
    page->setMinimumWidth(0);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(0);
    auto *content = new QWidget();
    content->setObjectName("pageContent");
    content->setMinimumWidth(0);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(28, 26, 28, 28);
    lay->setSpacing(20);

    // ---- 页面标题：先交代当前视图，再进入指标细节 ----
    auto *intro = new QHBoxLayout();
    intro->setContentsMargins(0, 0, 0, 2);
    intro->setSpacing(16);
    auto *introText = new QVBoxLayout();
    introText->setSpacing(5);
    auto *heading = new QLabel(QStringLiteral("集群运行概览"), content);
    heading->setObjectName("dashboardHeading");
    auto *description = new QLabel(
        QStringLiteral("实时洞察 CPU、内存、网络与磁盘的运行状态。"), content);
    description->setObjectName("dashboardSubtitle");
    introText->addWidget(heading);
    introText->addWidget(description);
    intro->addLayout(introText);
    intro->addStretch();
    auto *refreshHint = new QLabel(QStringLiteral("●  自动刷新 · 3 秒"), content);
    refreshHint->setObjectName("dashboardLive");
    intro->addWidget(refreshHint, 0, Qt::AlignVCenter);
    lay->addLayout(intro);

    // ---- 4 个统计卡片 ----
    // Two-column KPI grid stays readable on high-DPI laptops and compact screens.
    auto *cardGrid = new QGridLayout();
    cardGrid->setHorizontalSpacing(14);
    cardGrid->setVerticalSpacing(14);
    cardGrid->setColumnStretch(0, 1);
    cardGrid->setColumnStretch(1, 1);

    scoreCard = new StatCard(QStringLiteral("健康评分"), content);
    cpuCard = new StatCard(QStringLiteral("CPU 使用率"), content);
    memCard = new StatCard(QStringLiteral("内存使用率"), content);
    loadCard = new StatCard(QStringLiteral("系统负载 (1min)"), content);
    for (auto *card : {scoreCard, cpuCard, memCard, loadCard}) {
        card->setMinimumWidth(0);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cardGrid->addWidget(scoreCard, 0, 0);
    cardGrid->addWidget(cpuCard, 0, 1);
    cardGrid->addWidget(memCard, 1, 0);
    cardGrid->addWidget(loadCard, 1, 1);
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
                            QStringLiteral("CPU 使用率"), QStringLiteral("内存使用率"),
                            QStringLiteral("连接状态")});
    hostsTable->setMinimumHeight(240);
    hostsTable->setCursor(Qt::PointingHandCursor);
    connect(hostsTable, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
                auto *hostItem = hostsTable->item(row, 0);
                if (!hostItem) return;
                const int hostIndex = hostSelector->findText(hostItem->text());
                if (hostIndex >= 0) hostSelector->setCurrentIndex(hostIndex);
            });

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
    page->setMinimumWidth(0);
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
    page->setMinimumWidth(0);
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
    page->setMinimumWidth(0);
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
    page->setMinimumWidth(0);
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
    page->setMinimumWidth(0);
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
    if (v >= warn) return zinc::warning;
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
    // Blend the status color with the dark surface; QColor::name() omits alpha.
    constexpr int mix = 42;
    const QColor bg = QColor::fromRgb(
        (color.red() * mix + zinc::panel.red() * (255 - mix)) / 255,
        (color.green() * mix + zinc::panel.green() * (255 - mix)) / 255,
        (color.blue() * mix + zinc::panel.blue() * (255 - mix)) / 255);
    badge->setStyleSheet(QString(
        "QLabel{background:%1;color:%2;border-radius:8px;padding:3px 10px;"
        "font-size:10px;font-weight:650;border:1px solid %3;}")
        .arg(bg.name(), color.name(), zinc::border.name()));
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
        QColor color = pct >= 90 ? zinc::red : (pct >= 70 ? zinc::warning : zinc::emerald);
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

        QColor color = util >= 90 ? zinc::red : (util >= 70 ? zinc::warning : zinc::emerald);
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
                                 .arg(ok ? zinc::emerald.name() : zinc::red.name()));
}

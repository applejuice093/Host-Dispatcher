#include "main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSizePolicy>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace host_dispatcher;

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void TimelineWidget::setRecords(const std::vector<TickRecord>& records)
{
    records_ = records;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#111827"));
    painter.setRenderHint(QPainter::Antialiasing);
    if (records_.empty()) {
        painter.setPen(QColor("#9ca3af"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Step or play the simulation to build the CPU timeline."));
        return;
    }
    const std::size_t first = records_.size() > 40 ? records_.size() - 40 : 0;
    const std::size_t count = records_.size() - first;
    const int left = 18, right = 18, top = 25, barHeight = 38;
    const int available = qMax(1, width() - left - right);
    const double unit = static_cast<double>(available) / count;
    painter.setPen(QColor("#cbd5e1"));
    painter.drawText(left, 17, QStringLiteral("Showing ticks %1-%2 of %3 (latest 40)")
        .arg(records_[first].time).arg(records_.back().time).arg(static_cast<qulonglong>(records_.size())));
    for (std::size_t i = first; i < records_.size(); ++i) {
        const std::size_t visibleIndex = i - first;
        const int x = left + static_cast<int>(visibleIndex * unit);
        const int nextX = left + static_cast<int>((visibleIndex + 1) * unit);
        const QRect bar(x, top, qMax(1, nextX - x - 1), barHeight);
        const int id = records_[i].processId;
        const QColor color = id == 0 ? QColor("#374151") : QColor::fromHsv((id * 67) % 360, 155, 205);
        painter.fillRect(bar, color);
        painter.setPen(Qt::white);
        painter.drawText(bar, Qt::AlignCenter, id == 0 ? QStringLiteral("Idle") : QStringLiteral("P%1").arg(id));
        painter.setPen(QColor("#cbd5e1"));
        if (visibleIndex % 5 == 0 || visibleIndex + 1 == count)
            painter.drawText(x, top + barHeight + 21, QString::number(records_[i].time));
    }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("HOST Dispatcher Simulator"));
    setMinimumSize(1040, 700);
    resize(1240, 820);
    QWidget *root = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(root);

    QHBoxLayout *toolbar = new QHBoxLayout;
    clockLabel_ = new QLabel(QStringLiteral("Simulated tick: 0"), root);
    memoryCapacityInput_ = new QSpinBox(root);
    memoryCapacityInput_->setRange(1, 1000000);
    memoryCapacityInput_->setValue(simulation_.memory().capacity());
    memoryCapacityInput_->setSuffix(QStringLiteral(" MB"));
    QPushButton *applyMemory = new QPushButton(QStringLiteral("Set Capacity"), root);
    QPushButton *sample = new QPushButton(QStringLiteral("Load Sample"), root);
    QPushButton *add = new QPushButton(QStringLiteral("Add Process"), root);
    playButton_ = new QPushButton(QStringLiteral("Play"), root);
    QPushButton *step = new QPushButton(QStringLiteral("Step"), root);
    QPushButton *reset = new QPushButton(QStringLiteral("Reset"), root);
    suspendButton_ = new QPushButton(QStringLiteral("Suspend"), root);
    resumeButton_ = new QPushButton(QStringLiteral("Resume"), root);
    terminateButton_ = new QPushButton(QStringLiteral("Terminate"), root);
    toolbar->addWidget(clockLabel_);
    toolbar->addWidget(new QLabel(QStringLiteral("Memory"), root));
    toolbar->addWidget(memoryCapacityInput_);
    toolbar->addWidget(applyMemory);
    toolbar->addStretch();
    toolbar->addWidget(sample); toolbar->addWidget(add); toolbar->addWidget(playButton_);
    toolbar->addWidget(step); toolbar->addWidget(reset); toolbar->addWidget(suspendButton_);
    toolbar->addWidget(resumeButton_); toolbar->addWidget(terminateButton_);
    layout->addLayout(toolbar);

    QTabWidget *tabs = new QTabWidget(root);
    tabs->addTab(makeDashboard(), QStringLiteral("Simulation"));
    layout->addWidget(tabs);
    setCentralWidget(root);

    connect(sample, &QPushButton::clicked, this, [this]() {
        timer_.stop(); playButton_->setText(QStringLiteral("Play")); simulation_.loadSample(); refresh();
    });
    connect(add, &QPushButton::clicked, this, &MainWindow::addProcess);
    connect(applyMemory, &QPushButton::clicked, this, [this]() {
        try { simulation_.setMemoryCapacity(memoryCapacityInput_->value()); refresh(); }
        catch (const std::exception& error) { QMessageBox::warning(this, QStringLiteral("Memory capacity"), QString::fromUtf8(error.what())); }
    });
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(step, &QPushButton::clicked, this, [this]() { simulation_.step(); refresh(); });
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (simulation_.time() > 0 || !simulation_.processes().all().empty()) {
            if (QMessageBox::question(this, QStringLiteral("Reset simulation"),
                    QStringLiteral("Clear the current processes, timeline, resources, and metrics?")) != QMessageBox::Yes) return;
        }
        timer_.stop(); playButton_->setText(QStringLiteral("Play")); simulation_.reset(); refresh();
    });
    connect(suspendButton_, &QPushButton::clicked, this, &MainWindow::suspendSelected);
    connect(resumeButton_, &QPushButton::clicked, this, &MainWindow::resumeSelected);
    connect(terminateButton_, &QPushButton::clicked, this, &MainWindow::terminateSelected);
    connect(&timer_, &QTimer::timeout, this, [this]() { simulation_.step(); refresh(); });
    timer_.setInterval(450);
    refresh();
}

QWidget *MainWindow::makeDashboard()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    summaryLabel_ = new QLabel(page);
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    processTable_ = new QTableWidget(0, 14, page);
    processTable_->setHorizontalHeaderLabels({"ID", "Name", "Type", "State", "Arrival", "CPU burst", "Remaining", "Queue", "Memory MB", "Deadline", "First start", "Finish", "Missed", "Waiting"});
    processTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    processTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    processTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    processTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    processTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(processTable_, 2);
    connect(processTable_, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActionAvailability);

    QLabel *schedulerTitle = new QLabel(QStringLiteral("CPU timeline (simulated ticks)"), page);
    layout->addWidget(schedulerTitle);
    timeline_ = new TimelineWidget(page);
    layout->addWidget(timeline_);

    QHBoxLayout *details = new QHBoxLayout;
    queueLabel_ = new QLabel(page); queueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    memoryLabel_ = new QLabel(page); memoryLabel_->setWordWrap(true);
    ioLabel_ = new QLabel(page); ioLabel_->setWordWrap(true);
    for (QLabel *label : {queueLabel_, memoryLabel_, ioLabel_}) {
        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        label->setStyleSheet("background:#1f2937;color:#e5e7eb;padding:10px;border-radius:6px;");
        details->addWidget(label, 1);
    }
    layout->addLayout(details);
    metricsLabel_ = new QLabel(page);
    layout->addWidget(metricsLabel_);
    eventsView_ = new QPlainTextEdit(page);
    eventsView_->setReadOnly(true);
    eventsView_->setMaximumHeight(75);
    eventsView_->setPlaceholderText(QStringLiteral("Simulation events and admission messages appear here."));
    layout->addWidget(eventsView_);
    return page;
}

int MainWindow::selectedProcessId() const
{
    if (!processTable_ || processTable_->currentRow() < 0) return 0;
    return processTable_->item(processTable_->currentRow(), 0)->text().toInt();
}

void MainWindow::updateActionAvailability()
{
    const int id = selectedProcessId();
    const Process *selected = id && simulation_.processes().contains(id) ? &simulation_.processes().at(id) : nullptr;
    suspendButton_->setEnabled(selected && (selected->state == ProcessState::Ready || selected->state == ProcessState::Running || selected->state == ProcessState::Blocked));
    resumeButton_->setEnabled(selected && (selected->state == ProcessState::SuspendedReady || selected->state == ProcessState::SuspendedBlocked));
    terminateButton_->setEnabled(selected && selected->state != ProcessState::Terminated);
}

void MainWindow::refresh()
{
    const int preserveId = selectedProcessId();
    clockLabel_->setText(QStringLiteral("Simulated tick: %1").arg(simulation_.time()));
    memoryCapacityInput_->blockSignals(true);
    memoryCapacityInput_->setValue(simulation_.memory().capacity());
    memoryCapacityInput_->blockSignals(false);
    memoryCapacityInput_->setEnabled(simulation_.time() == 0 && simulation_.processes().all().empty() && !timer_.isActive());
    const auto& all = simulation_.processes().all();
    processTable_->setRowCount(static_cast<int>(all.size()));
    int row = 0;
    for (const auto& entry : all) {
        const Process& p = entry.second;
        const QString values[] = {
            QString::number(p.spec.id), QString::fromStdString(p.spec.name), QString::fromStdString(toString(p.spec.type)),
            QString::fromStdString(toString(p.state)), QString::number(p.spec.arrival), QString::number(p.spec.cpuBurst),
            QString::number(p.remaining),
            p.spec.type == ProcessType::User ? QStringLiteral("Q%1").arg(p.queueLevel) : QStringLiteral("EDF"),
            QString::number(p.spec.memoryMb), p.spec.hasDeadline ? QString::number(p.absoluteDeadline()) : QStringLiteral("—"),
            p.firstStart < 0 ? QStringLiteral("—") : QString::number(p.firstStart),
            p.completion < 0 ? QStringLiteral("—") : QString::number(p.completion),
            p.deadlineMissed ? QStringLiteral("Yes") : QStringLiteral("No"),
            QString::number(p.waitingTime)
        };
        for (int column = 0; column < 14; ++column)
            processTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        ++row;
    }
    if (preserveId) {
        for (int candidate = 0; candidate < processTable_->rowCount(); ++candidate) {
            if (processTable_->item(candidate, 0)->text().toInt() == preserveId) {
                processTable_->selectRow(candidate);
                break;
            }
        }
    }
    updateActionAvailability();
    const Metrics m = simulation_.metrics();
    summaryLabel_->setText(QStringLiteral("Simulated dispatcher | Real-time EDF preempts user work; user tasks use MLFQ (Q0 RR 2, Q1 RR 4, Q2 FCFS), with 20-tick waiting boosts.")
        + QStringLiteral("  |  Processes: %1  |  Completed: %2").arg(static_cast<qulonglong>(all.size())).arg(m.completed));
    const auto queues = simulation_.readyQueues();
    QString queueText = QStringLiteral("MLFQ ready queues\n");
    for (int i = 0; i < 3; ++i) {
        queueText += QStringLiteral("Q%1: ").arg(i);
        for (int id : queues[static_cast<std::size_t>(i)]) queueText += QStringLiteral("P%1 ").arg(id);
        queueText += '\n';
    }
    queueLabel_->setText(queueText);
    memoryLabel_->setText(QStringLiteral("Simulated memory\nCapacity: %1 MB\nUsed: %2 MB\nFree: %3 MB")
        .arg(simulation_.memory().capacity()).arg(simulation_.memory().used()).arg(simulation_.memory().free()));
    ioLabel_->setText(QString::fromStdString(simulation_.io().describe()));
    timeline_->setRecords(simulation_.timeline());
    metricsLabel_->setText(QStringLiteral("Metrics | completed %1 | avg waiting %2 | turnaround %3 | response %4 | CPU %5% | throughput %6 tasks/tick | deadline misses %7/%8 real-time (%9%)")
        .arg(m.completed).arg(m.averageWaiting, 0, 'f', 2).arg(m.averageTurnaround, 0, 'f', 2)
        .arg(m.averageResponse, 0, 'f', 2).arg(m.utilizationPercent, 0, 'f', 1).arg(m.throughput, 0, 'f', 3)
        .arg(m.deadlineMisses).arg(m.realTimeTasks).arg(m.deadlineMissRatePercent, 0, 'f', 1));
    QStringList messages;
    const auto& rawMessages = simulation_.messages();
    const std::size_t start = rawMessages.size() > 8 ? rawMessages.size() - 8 : 0;
    for (std::size_t i = start; i < rawMessages.size(); ++i) messages << QString::fromStdString(rawMessages[i]);
    eventsView_->setPlainText(messages.join('\n'));
}

void MainWindow::togglePlay()
{
    if (timer_.isActive()) timer_.stop();
    else timer_.start();
    playButton_->setText(timer_.isActive() ? QStringLiteral("Pause") : QStringLiteral("Play"));
}

void MainWindow::addProcess()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Add simulated process"));
    QFormLayout form(&dialog);
    QSpinBox id; id.setRange(1, 1000000); id.setValue(1);
    if (!simulation_.processes().all().empty()) id.setValue(simulation_.processes().all().rbegin()->first + 1);
    QLineEdit name; name.setText(QStringLiteral("Process"));
    QComboBox type; type.addItems({QStringLiteral("User"), QStringLiteral("Real-Time")});
    QSpinBox arrival; arrival.setRange(0, 1000000); arrival.setValue(simulation_.time());
    QSpinBox burst; burst.setRange(1, 1000000); burst.setValue(5);
    QSpinBox memory; memory.setRange(1, simulation_.memory().capacity()); memory.setValue(64);
    QSpinBox deadline; deadline.setRange(1, 1000000); deadline.setValue(10);
    QCheckBox hasIo(QStringLiteral("Issue one I/O request"));
    QComboBox device; device.addItems({"Disk", "Printer", "Keyboard"});
    QSpinBox ioAfter; ioAfter.setRange(1, 999999); ioAfter.setValue(2);
    QSpinBox ioDuration; ioDuration.setRange(1, 1000000); ioDuration.setValue(2);
    form.addRow(QStringLiteral("ID"), &id); form.addRow(QStringLiteral("Name"), &name);
    form.addRow(QStringLiteral("Process type"), &type); form.addRow(QStringLiteral("Arrival tick"), &arrival);
    form.addRow(QStringLiteral("CPU burst"), &burst); form.addRow(QStringLiteral("Memory (MB)"), &memory);
    form.addRow(QStringLiteral("Relative deadline (real-time)"), &deadline);
    form.addRow(&hasIo); form.addRow(QStringLiteral("I/O device"), &device);
    form.addRow(QStringLiteral("CPU ticks before I/O"), &ioAfter); form.addRow(QStringLiteral("I/O service ticks"), &ioDuration);
    connect(&type, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [&deadline](int index) { deadline.setEnabled(index == 1); });
    connect(&hasIo, &QCheckBox::toggled, &dialog, [&device, &ioAfter, &ioDuration](bool enabled) {
        device.setEnabled(enabled); ioAfter.setEnabled(enabled); ioDuration.setEnabled(enabled);
    });
    deadline.setEnabled(false); device.setEnabled(false); ioAfter.setEnabled(false); ioDuration.setEnabled(false);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    ProcessSpec spec;
    spec.id = id.value(); spec.name = name.text().trimmed().toStdString();
    spec.type = type.currentIndex() == 1 ? ProcessType::RealTime : ProcessType::User;
    spec.arrival = arrival.value(); spec.cpuBurst = burst.value(); spec.memoryMb = memory.value();
    spec.hasDeadline = spec.type == ProcessType::RealTime; spec.relativeDeadline = deadline.value();
    spec.hasIo = hasIo.isChecked();
    if (spec.hasIo) spec.io = IoRequest{device.currentText().toStdString(), ioAfter.value(), ioDuration.value()};
    try { simulation_.addProcess(spec); refresh(); }
    catch (const std::exception& error) { QMessageBox::warning(this, QStringLiteral("Invalid process"), QString::fromUtf8(error.what())); }
}

void MainWindow::suspendSelected()
{
    if (!simulation_.suspend(selectedProcessId())) QMessageBox::information(this, QStringLiteral("Suspend"), QStringLiteral("Select a ready, running, or blocked process."));
    refresh();
}

void MainWindow::resumeSelected()
{
    if (!simulation_.resume(selectedProcessId())) QMessageBox::information(this, QStringLiteral("Resume"), QStringLiteral("Select a suspended process."));
    refresh();
}

void MainWindow::terminateSelected()
{
    if (!simulation_.terminate(selectedProcessId())) QMessageBox::information(this, QStringLiteral("Terminate"), QStringLiteral("Select a process that has not terminated."));
    refresh();
}

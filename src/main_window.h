#pragma once

#include "simulation.h"

#include <QMainWindow>
#include <QTimer>

class QLabel;
class QTableWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QWidget;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);
    void setRecords(const std::vector<host_dispatcher::TickRecord>& records);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    std::vector<host_dispatcher::TickRecord> records_;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private slots:
    void refresh();
    void addProcess();
    void togglePlay();
    void suspendSelected();
    void resumeSelected();
    void terminateSelected();
private:
    host_dispatcher::Simulation simulation_;
    QTimer timer_;
    QTableWidget *processTable_ = nullptr;
    QPushButton *playButton_ = nullptr;
    QPushButton *suspendButton_ = nullptr;
    QPushButton *resumeButton_ = nullptr;
    QPushButton *terminateButton_ = nullptr;
    QSpinBox *memoryCapacityInput_ = nullptr;
    QLabel *clockLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *queueLabel_ = nullptr;
    QLabel *memoryLabel_ = nullptr;
    QLabel *ioLabel_ = nullptr;
    QLabel *metricsLabel_ = nullptr;
    QPlainTextEdit *eventsView_ = nullptr;
    TimelineWidget *timeline_ = nullptr;
    QWidget *makeDashboard();
    int selectedProcessId() const;
    void updateActionAvailability();
};

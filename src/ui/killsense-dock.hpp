// REALM KillSense - main OBS dock ("REALM KillSense").
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPointer>
#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace killsense::obsplugin {

class Controller;
class DebugViewDialog;

class KillSenseDock : public QWidget {
	Q_OBJECT

public:
	KillSenseDock(std::shared_ptr<Controller> controller, QWidget *parent = nullptr);
	~KillSenseDock() override;

private slots:
	void refreshSources();
	void refreshProfiles();
	void onSourceChosen(int index);
	void onProfileChosen(int index);
	void onAutoDetect();
	void onStartStop();
	void onTestKill();
	void onResetSession();
	void onProfileManager();
	void onEventOutputs();
	void onDebugView();
	void onSettings();
	void onCaptureNamePlate();
	void refreshStatus();

private:
	std::shared_ptr<Controller> controller() const { return controller_.lock(); }
	void openSettings(int tab);

	std::weak_ptr<Controller> controller_;
	QComboBox *sourceCombo_ = nullptr;
	QComboBox *profileCombo_ = nullptr;
	QPushButton *startStop_ = nullptr;
	QLabel *source_ = nullptr;
	QLabel *profile_ = nullptr;
	QLabel *state_ = nullptr;
	QLabel *kills_ = nullptr;
	QLabel *streak_ = nullptr;
	QLabel *deaths_ = nullptr;
	QLabel *kd_ = nullptr;
	QLabel *gamertag_ = nullptr;
	QLabel *setupHint_ = nullptr;
	QPushButton *capture_ = nullptr;
	QLabel *lastEvent_ = nullptr;
	QLabel *lastConfidence_ = nullptr;
	QLabel *fps_ = nullptr;
	QLabel *latency_ = nullptr;
	QLabel *dropped_ = nullptr;
	QLabel *cpu_ = nullptr;
	QLabel *gpu_ = nullptr;
	QLabel *ocr_ = nullptr;
	QLabel *message_ = nullptr;
	QTimer *timer_ = nullptr;
	QPointer<DebugViewDialog> debugView_;
	bool updatingCombos_ = false;
};

} // namespace killsense::obsplugin

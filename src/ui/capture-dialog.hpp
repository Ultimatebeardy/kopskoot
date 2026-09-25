// REALM KillSense - "Capture my name plate" dialog.
// Grabs the profile's name-plate region from the live analysis frames, cuts
// out the player's own name and stores it as a per-user template.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/image.hpp"

#include <QDialog>

#include <memory>
#include <string>

class QLabel;
class QPushButton;
class QTimer;

namespace killsense::obsplugin {

class Controller;

class CaptureDialog : public QDialog {
	Q_OBJECT

public:
	CaptureDialog(std::weak_ptr<Controller> controller, QWidget *parent = nullptr);
	~CaptureDialog() override;

private slots:
	void poll();
	void onRetry();
	void onSave();

private:
	void setMessage(const QString &text, bool error);

	std::weak_ptr<Controller> controller_;
	std::string slotName_;
	std::string regionId_;
	QLabel *info_ = nullptr;
	QLabel *preview_ = nullptr;
	QLabel *message_ = nullptr;
	QPushButton *retry_ = nullptr;
	QPushButton *save_ = nullptr;
	QTimer *timer_ = nullptr;
	Image captured_;
	bool haveCapture_ = false;
	uint64_t lastSequence_ = 0;
	int attempts_ = 0;
};

} // namespace killsense::obsplugin

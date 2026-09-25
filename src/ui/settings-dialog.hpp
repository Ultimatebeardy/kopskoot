// REALM KillSense - settings dialog (Detection / Event Outputs / Advanced).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QLabel;

namespace killsense::obsplugin {

class Controller;

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	enum Tab { DetectionTab = 0, OutputsTab = 1, AdvancedTab = 2 };
	SettingsDialog(std::shared_ptr<Controller> controller, int initialTab, QWidget *parent = nullptr);

private slots:
	void onAccept();
	void onBrowseJsonl();

private:
	void load();

	std::shared_ptr<Controller> controller_;
	QTabWidget *tabs_ = nullptr;
	// Detection
	QLineEdit *gamertag_ = nullptr;
	QComboBox *preset_ = nullptr;
	QDoubleSpinBox *fps_ = nullptr;
	QCheckBox *thresholdOn_ = nullptr;
	QDoubleSpinBox *threshold_ = nullptr;
	QCheckBox *cooldownOn_ = nullptr;
	QSpinBox *cooldown_ = nullptr;
	QCheckBox *roiOn_ = nullptr;
	QDoubleSpinBox *roiX_ = nullptr;
	QDoubleSpinBox *roiY_ = nullptr;
	QDoubleSpinBox *roiW_ = nullptr;
	QDoubleSpinBox *roiH_ = nullptr;
	QCheckBox *autoStart_ = nullptr;
	// Outputs
	QCheckBox *websocket_ = nullptr;
	QLabel *websocketStatus_ = nullptr;
	QCheckBox *jsonl_ = nullptr;
	QLineEdit *jsonlPath_ = nullptr;
	QComboBox *counterSource_ = nullptr;
	QLineEdit *counterFormat_ = nullptr;
	// Advanced
	QComboBox *logLevel_ = nullptr;
	QLineEdit *ocrLanguage_ = nullptr;
};

} // namespace killsense::obsplugin

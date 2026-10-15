// KopSkoot - Profile Manager: lists loaded profiles and load errors,
// reloads profiles from disk and opens the profile folders.
// (Profile editing inside OBS is planned together with Learn This Game.)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>

#include <memory>

class QPlainTextEdit;
class QTableWidget;

namespace killsense::obsplugin {

class Controller;

class ProfileManagerDialog : public QDialog {
	Q_OBJECT
public:
	ProfileManagerDialog(std::shared_ptr<Controller> controller, QWidget *parent = nullptr);

private slots:
	void reload();
	void populate();

private:
	std::shared_ptr<Controller> controller_;
	QTableWidget *table_ = nullptr;
	QPlainTextEdit *issues_ = nullptr;
};

} // namespace killsense::obsplugin

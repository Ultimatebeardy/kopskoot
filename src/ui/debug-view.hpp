// REALM KillSense - Debug View: shows the analysed regions with detector
// overlays, OCR text, confidence contributions and decisions.
// Debug capture is only enabled while this window is open.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QImage>
#include <QWidget>

#include <memory>
#include <vector>

class QPlainTextEdit;
class QTimer;
class QLabel;

namespace killsense::obsplugin {

class Controller;

class RegionCanvas : public QWidget {
	Q_OBJECT
public:
	explicit RegionCanvas(QWidget *parent = nullptr);
	struct Box {
		QRect rect;
		QColor color;
		QString label;
	};
	void setContent(QImage image, std::vector<Box> boxes);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QImage image_;
	std::vector<Box> boxes_;
};

class DebugViewDialog : public QDialog {
	Q_OBJECT
public:
	DebugViewDialog(std::weak_ptr<Controller> controller, QWidget *parent = nullptr);
	~DebugViewDialog() override;

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void refresh();

private:
	std::weak_ptr<Controller> controller_;
	RegionCanvas *canvas_ = nullptr;
	QPlainTextEdit *details_ = nullptr;
	QLabel *header_ = nullptr;
	QTimer *timer_ = nullptr;
	uint64_t lastSequence_ = 0;
};

} // namespace killsense::obsplugin

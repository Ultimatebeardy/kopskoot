// SPDX-License-Identifier: GPL-2.0-or-later
#include "capture-dialog.hpp"

#include "ui-common.hpp"

#include "../controller.hpp"

#include "killsense/user-template.hpp"

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace killsense::obsplugin {

CaptureDialog::CaptureDialog(std::weak_ptr<Controller> controller, QWidget *parent)
	: QDialog(parent),
	  controller_(std::move(controller))
{
	setWindowTitle(T("KillSense.Capture.Title"));
	resize(500, 400);
	auto *layout = new QVBoxLayout(this);
	info_ = new QLabel(T("KillSense.Capture.Info"), this);
	info_->setWordWrap(true);
	layout->addWidget(info_);
	preview_ = new QLabel(this);
	preview_->setMinimumHeight(90);
	preview_->setAlignment(Qt::AlignCenter);
	preview_->setStyleSheet(QStringLiteral("background:#202028; border:1px solid #444;"));
	layout->addWidget(preview_, 1);
	layout->addWidget(new QLabel(T("KillSense.Capture.Region"), this));
	regionPreview_ = new QLabel(this);
	regionPreview_->setMinimumHeight(70);
	regionPreview_->setAlignment(Qt::AlignCenter);
	regionPreview_->setStyleSheet(QStringLiteral("background:#202028; border:1px solid #444;"));
	layout->addWidget(regionPreview_);
	message_ = new QLabel(this);
	message_->setWordWrap(true);
	layout->addWidget(message_);
	auto *row = new QHBoxLayout();
	retry_ = new QPushButton(T("KillSense.Capture.Retry"), this);
	save_ = new QPushButton(T("KillSense.Capture.Save"), this);
	auto *cancel = new QPushButton(T("KillSense.Capture.Cancel"), this);
	save_->setEnabled(false);
	row->addWidget(retry_);
	row->addStretch(1);
	row->addWidget(save_);
	row->addWidget(cancel);
	layout->addLayout(row);
	connect(retry_, &QPushButton::clicked, this, &CaptureDialog::onRetry);
	connect(save_, &QPushButton::clicked, this, &CaptureDialog::onSave);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

	timer_ = new QTimer(this);
	timer_->setInterval(250);
	connect(timer_, &QTimer::timeout, this, &CaptureDialog::poll);

	auto c = controller_.lock();
	if (!c) {
		setMessage(T("KillSense.Debug.Unavailable"), true);
		retry_->setEnabled(false);
		return;
	}
	for (const auto &slot : c->userTemplateSlots()) {
		slotName_ = slot.name;
		regionId_ = slot.regionId;
		break;
	}
	if (slotName_.empty()) {
		setMessage(T("KillSense.Capture.NotNeeded"), true);
		retry_->setEnabled(false);
		return;
	}
	if (!c->detecting()) {
		setMessage(T("KillSense.Capture.StartFirst"), true);
		retry_->setEnabled(false);
		return;
	}
	c->setDebugEnabled(true); // the worker keeps copies of the region images
	onRetry();
}

CaptureDialog::~CaptureDialog()
{
	if (auto c = controller_.lock())
		c->setDebugEnabled(false);
}

void CaptureDialog::setMessage(const QString &text, bool error)
{
	message_->setText(text);
	message_->setStyleSheet(error ? QStringLiteral("color:#ff8a65;") : QStringLiteral("color:#81c784;"));
}

void CaptureDialog::onRetry()
{
	haveCapture_ = false;
	attempts_ = 0;
	save_->setEnabled(false);
	setMessage(T("KillSense.Capture.Looking"), false);
	timer_->start();
}

void CaptureDialog::poll()
{
	auto c = controller_.lock();
	if (!c)
		return;
	DebugSnapshot snap;
	if (!c->debugSnapshot(snap) || !snap.valid || snap.analysis.sequence == lastSequence_)
		return;
	lastSequence_ = snap.analysis.sequence;
	for (const auto &r : snap.regions) {
		if (r.regionId != regionId_ || r.rgba.empty())
			continue;
		const TemplateCrop crop = cropNameTemplate(r.rgba, r.pxPerReference);
		// Always show the whole captured area (alpha ignored: some sources
		// deliver alpha = 0 which would otherwise look black).
		{
			QImage region = QImage(r.rgba.data(), r.rgba.width(), r.rgba.height(), r.rgba.stride(),
					       QImage::Format_RGBX8888)
						.copy();
			if (crop.ok) {
				QPainter painter(&region);
				painter.setPen(QPen(QColor(255, 210, 0), 2));
				painter.drawRect(crop.box.x, crop.box.y, crop.box.w - 1, crop.box.h - 1);
			}
			regionPreview_->setPixmap(QPixmap::fromImage(region).scaledToHeight(
				std::min(120, region.height() * 2), Qt::SmoothTransformation));
			regionPreview_->setToolTip(QStringLiteral("%1 x %2 px").arg(r.rgba.width()).arg(r.rgba.height()));
		}
		if (!crop.ok) {
			if (++attempts_ >= 12) { // ~2 s of frames without a usable name
				timer_->stop();
				setMessage(Q(crop.message), true);
			}
			return;
		}
		timer_->stop();
		captured_ = crop.image;
		haveCapture_ = true;
		QImage view(captured_.data(), captured_.width(), captured_.height(), captured_.stride(),
			    QImage::Format_RGBX8888);
		preview_->setPixmap(QPixmap::fromImage(view.copy()).scaledToHeight(
			std::min(80, captured_.height() * 2), Qt::SmoothTransformation));
		setMessage(T("KillSense.Capture.Check"), false);
		save_->setEnabled(true);
		return;
	}
}

void CaptureDialog::onSave()
{
	auto c = controller_.lock();
	if (!c || !haveCapture_)
		return;
	std::string err;
	if (!c->saveUserTemplate(slotName_, captured_, &err)) {
		setMessage(Q(err), true);
		return;
	}
	accept();
}

} // namespace killsense::obsplugin

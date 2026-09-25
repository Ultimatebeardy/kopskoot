// SPDX-License-Identifier: GPL-2.0-or-later
#include "debug-view.hpp"

#include "ui-common.hpp"

#include "../controller.hpp"

#include <algorithm>

#include <QCloseEvent>
#include <QFont>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace killsense::obsplugin {

RegionCanvas::RegionCanvas(QWidget *parent) : QWidget(parent)
{
	setMinimumSize(320, 120);
}

void RegionCanvas::setContent(QImage image, std::vector<Box> boxes)
{
	image_ = std::move(image);
	boxes_ = std::move(boxes);
	update();
}

void RegionCanvas::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(20, 20, 24));
	if (image_.isNull()) {
		p.setPen(Qt::gray);
		p.drawText(rect(), Qt::AlignCenter, T("KillSense.Debug.NoFrame"));
		return;
	}
	const QSize scaled = image_.size().scaled(size(), Qt::KeepAspectRatio);
	const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
	p.drawImage(target, image_);
	const double sx = static_cast<double>(target.width()) / image_.width();
	const double sy = static_cast<double>(target.height()) / image_.height();
	for (const auto &b : boxes_) {
		const QRect r(target.x() + static_cast<int>(b.rect.x() * sx),
			      target.y() + static_cast<int>(b.rect.y() * sy), static_cast<int>(b.rect.width() * sx),
			      static_cast<int>(b.rect.height() * sy));
		p.setPen(QPen(b.color, 2));
		p.drawRect(r);
		if (!b.label.isEmpty()) {
			p.fillRect(QRect(r.x(), r.y() - 14, p.fontMetrics().horizontalAdvance(b.label) + 6, 14),
				   QColor(0, 0, 0, 170));
			p.drawText(r.x() + 3, r.y() - 3, b.label);
		}
	}
}

DebugViewDialog::DebugViewDialog(std::weak_ptr<Controller> controller, QWidget *parent)
	: QDialog(parent),
	  controller_(std::move(controller))
{
	setWindowTitle(T("KillSense.DebugView"));
	resize(760, 620);
	auto *layout = new QVBoxLayout(this);
	header_ = new QLabel(this);
	header_->setWordWrap(true);
	layout->addWidget(header_);
	auto *split = new QSplitter(Qt::Vertical, this);
	canvas_ = new RegionCanvas(split);
	details_ = new QPlainTextEdit(split);
	details_->setReadOnly(true);
	details_->setLineWrapMode(QPlainTextEdit::NoWrap);
	QFont mono(QStringLiteral("Consolas"));
	mono.setStyleHint(QFont::Monospace);
	details_->setFont(mono);
	split->addWidget(canvas_);
	split->addWidget(details_);
	split->setStretchFactor(0, 3);
	split->setStretchFactor(1, 2);
	layout->addWidget(split, 1);

	if (auto c = controller_.lock())
		c->setDebugEnabled(true);
	timer_ = new QTimer(this);
	timer_->setInterval(200);
	connect(timer_, &QTimer::timeout, this, &DebugViewDialog::refresh);
	timer_->start();
	refresh();
}

DebugViewDialog::~DebugViewDialog()
{
	if (auto c = controller_.lock())
		c->setDebugEnabled(false);
}

void DebugViewDialog::closeEvent(QCloseEvent *event)
{
	if (auto c = controller_.lock())
		c->setDebugEnabled(false);
	timer_->stop();
	QDialog::closeEvent(event);
}

namespace {
QColor colorFor(DetectorKind kind)
{
	switch (kind) {
	case DetectorKind::Template:
		return QColor(255, 80, 80);
	case DetectorKind::Ocr:
		return QColor(80, 200, 255);
	case DetectorKind::Color:
		return QColor(255, 200, 60);
	case DetectorKind::HudChange:
		return QColor(160, 255, 120);
	case DetectorKind::Onnx:
		return QColor(200, 120, 255);
	}
	return Qt::white;
}
} // namespace

void DebugViewDialog::refresh()
{
	auto c = controller_.lock();
	if (!c) {
		header_->setText(T("KillSense.Debug.Unavailable"));
		return;
	}
	if (!c->detecting()) {
		header_->setText(T("KillSense.Debug.NotRunning"));
		return;
	}
	DebugSnapshot snap;
	if (!c->debugSnapshot(snap) || !snap.valid) {
		header_->setText(T("KillSense.Debug.Waiting"));
		return;
	}
	if (snap.analysis.sequence == lastSequence_)
		return;
	lastSequence_ = snap.analysis.sequence;

	// Stack all captured regions vertically into one image.
	int totalH = 0, maxW = 0;
	for (const auto &r : snap.regions) {
		totalH += r.rgba.height() + 4;
		maxW = std::max(maxW, r.rgba.width());
	}
	QImage image;
	std::vector<RegionCanvas::Box> boxes;
	if (maxW > 0 && totalH > 0) {
		image = QImage(maxW, totalH, QImage::Format_RGBA8888);
		image.fill(QColor(40, 40, 48));
		QPainter p(&image);
		int y = 0;
		for (const auto &r : snap.regions) {
			if (r.rgba.empty())
				continue;
			QImage view(r.rgba.data(), r.rgba.width(), r.rgba.height(), r.rgba.stride(),
				    QImage::Format_RGBA8888);
			p.drawImage(0, y, view);
			boxes.push_back({QRect(0, y, r.rgba.width(), r.rgba.height()), QColor(255, 255, 255, 120),
					 Q(r.regionId)});
			for (const auto &d : snap.analysis.detectors) {
				if (d.regionId != r.regionId)
					continue;
				for (const auto &b : d.boxes)
					boxes.push_back(
						{QRect(b.x, y + b.y, b.w, b.h), colorFor(d.kind),
						 d.kind == DetectorKind::Template ? Q(d.detectorId) : QString()});
			}
			y += r.rgba.height() + 4;
		}
	}
	canvas_->setContent(std::move(image), std::move(boxes));

	QString text;
	text += QStringLiteral("frame #%1  analysis %2 ms\n\n")
			.arg(snap.analysis.sequence)
			.arg(snap.analysis.processingMs, 0, 'f', 2);
	text += QStringLiteral("DETECTORS\n");
	for (const auto &d : snap.analysis.detectors) {
		text += QStringLiteral("  %1 [%2/%3] %4 score=%5 %6%7\n")
				.arg(Q(d.detectorId), -14)
				.arg(QString::fromUtf8(detectorKindId(d.kind)))
				.arg(Q(d.regionId))
				.arg(!d.available ? QStringLiteral("UNAVAILABLE")
						  : (d.active ? QStringLiteral("ACTIVE") : QStringLiteral("idle")))
				.arg(d.score, 0, 'f', 2)
				.arg(Q(d.detail))
				.arg(d.veto ? QStringLiteral(" VETO") : QString());
		if (d.kind == DetectorKind::Ocr && !d.rawText.empty())
			text += QStringLiteral("      OCR: \"%1\"\n")
					.arg(Q(d.rawText).replace(QLatin1Char('\n'), QStringLiteral(" | ")));
	}
	text += QStringLiteral("\nTARGETS\n");
	for (const auto &t : snap.analysis.targets) {
		text += QStringLiteral("  %1 -> %2  confidence=%3  %4\n")
				.arg(Q(t.targetId))
				.arg(QString::fromUtf8(eventTypeId(t.event)))
				.arg(t.fusion.confidence, 0, 'f', 3)
				.arg(t.fusion.passes ? QStringLiteral("PASS") : QStringLiteral("no"));
		for (const auto &cn : t.fusion.contributions)
			text += QStringLiteral("      %1 w=%2 s=%3 -> +%4%5\n")
					.arg(Q(cn.detector), -14)
					.arg(cn.weight, 0, 'f', 2)
					.arg(cn.score, 0, 'f', 2)
					.arg(cn.contribution, 0, 'f', 3)
					.arg(cn.available ? QString() : QStringLiteral(" (unavailable)"));
		text += QStringLiteral("      reason: %1\n").arg(Q(t.fusion.reason));
		text += QStringLiteral("      temporal: %1%2%3\n")
				.arg(t.decision.state == TrackState::Active ? QStringLiteral("ACTIVE")
									    : QStringLiteral("idle"))
				.arg(t.decision.emitCount > 0 ? QStringLiteral(" EMIT x%1 (%2)")
									.arg(t.decision.emitCount)
									.arg(Q(t.decision.reason))
							      : QString())
				.arg(t.decision.suppressed
					     ? QStringLiteral(" suppressed: ") + Q(t.decision.suppressReason)
					     : QString());
	}
	details_->setPlainText(text);
	header_->setText(T("KillSense.Debug.Header"));
}

} // namespace killsense::obsplugin

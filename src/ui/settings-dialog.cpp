// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings-dialog.hpp"

#include "ui-common.hpp"

#include "../controller.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace killsense::obsplugin {

namespace {
QDoubleSpinBox *normSpin(QWidget *parent)
{
	auto *s = new QDoubleSpinBox(parent);
	s->setRange(0.0, 1.0);
	s->setDecimals(3);
	s->setSingleStep(0.01);
	return s;
}
} // namespace

SettingsDialog::SettingsDialog(std::shared_ptr<Controller> controller, int initialTab, QWidget *parent)
	: QDialog(parent),
	  controller_(std::move(controller))
{
	setWindowTitle(T("KillSense.Settings"));
	auto *layout = new QVBoxLayout(this);
	tabs_ = new QTabWidget(this);
	layout->addWidget(tabs_);

	// ---- Detection ----
	auto *det = new QWidget(tabs_);
	auto *df = new QFormLayout(det);
	preset_ = new QComboBox(det);
	preset_->addItem(T("KillSense.Preset.LowImpact"), QStringLiteral("low_impact"));
	preset_->addItem(T("KillSense.Preset.Balanced"), QStringLiteral("balanced"));
	preset_->addItem(T("KillSense.Preset.HighAccuracy"), QStringLiteral("high_accuracy"));
	df->addRow(T("KillSense.Settings.Preset"), preset_);
	fps_ = new QDoubleSpinBox(det);
	fps_->setRange(0.0, 30.0);
	fps_->setDecimals(1);
	fps_->setSpecialValueText(T("KillSense.Settings.FpsPreset"));
	df->addRow(T("KillSense.Settings.Fps"), fps_);

	auto *thRow = new QHBoxLayout();
	thresholdOn_ = new QCheckBox(T("KillSense.Settings.Override"), det);
	threshold_ = new QDoubleSpinBox(det);
	threshold_->setRange(0.05, 1.0);
	threshold_->setDecimals(2);
	threshold_->setSingleStep(0.05);
	thRow->addWidget(thresholdOn_);
	thRow->addWidget(threshold_, 1);
	df->addRow(T("KillSense.Settings.Threshold"), thRow);

	auto *cdRow = new QHBoxLayout();
	cooldownOn_ = new QCheckBox(T("KillSense.Settings.Override"), det);
	cooldown_ = new QSpinBox(det);
	cooldown_->setRange(0, 60000);
	cooldown_->setSuffix(QStringLiteral(" ms"));
	cooldown_->setSingleStep(100);
	cdRow->addWidget(cooldownOn_);
	cdRow->addWidget(cooldown_, 1);
	df->addRow(T("KillSense.Settings.Cooldown"), cdRow);

	roiOn_ = new QCheckBox(T("KillSense.Settings.CustomRoi"), det);
	roiOn_->setToolTip(T("KillSense.Settings.CustomRoi.Tooltip"));
	df->addRow(roiOn_);
	auto *roiRow = new QHBoxLayout();
	roiX_ = normSpin(det);
	roiY_ = normSpin(det);
	roiW_ = normSpin(det);
	roiH_ = normSpin(det);
	for (auto [label, spin] :
	     {std::pair{"x", roiX_}, std::pair{"y", roiY_}, std::pair{"w", roiW_}, std::pair{"h", roiH_}}) {
		roiRow->addWidget(new QLabel(QString::fromLatin1(label), det));
		roiRow->addWidget(spin);
	}
	df->addRow(T("KillSense.Settings.RoiRect"), roiRow);
	autoStart_ = new QCheckBox(T("KillSense.Settings.AutoStart"), det);
	df->addRow(autoStart_);
	tabs_->addTab(det, T("KillSense.Settings.DetectionTab"));

	// ---- Outputs ----
	auto *out = new QWidget(tabs_);
	auto *of = new QFormLayout(out);
	websocket_ = new QCheckBox(T("KillSense.Outputs.Websocket"), out);
	websocketStatus_ = new QLabel(out);
	websocketStatus_->setWordWrap(true);
	of->addRow(websocket_);
	of->addRow(websocketStatus_);
	jsonl_ = new QCheckBox(T("KillSense.Outputs.Jsonl"), out);
	of->addRow(jsonl_);
	auto *pathRow = new QHBoxLayout();
	jsonlPath_ = new QLineEdit(out);
	auto *browse = new QPushButton(T("KillSense.Browse"), out);
	pathRow->addWidget(jsonlPath_, 1);
	pathRow->addWidget(browse);
	of->addRow(T("KillSense.Outputs.JsonlPath"), pathRow);
	auto *planned = new QLabel(T("KillSense.Outputs.Planned"), out);
	planned->setWordWrap(true);
	of->addRow(planned);
	tabs_->addTab(out, T("KillSense.Settings.OutputsTab"));

	// ---- Advanced ----
	auto *adv = new QWidget(tabs_);
	auto *af = new QFormLayout(adv);
	logLevel_ = new QComboBox(adv);
	for (const char *lvl : {"error", "warning", "info", "debug", "trace"})
		logLevel_->addItem(QString::fromLatin1(lvl), QString::fromLatin1(lvl));
	af->addRow(T("KillSense.Settings.LogLevel"), logLevel_);
	ocrLanguage_ = new QLineEdit(adv);
	ocrLanguage_->setPlaceholderText(T("KillSense.Settings.OcrLanguage.Placeholder"));
	af->addRow(T("KillSense.Settings.OcrLanguage"), ocrLanguage_);
	auto *privacy = new QLabel(T("KillSense.Settings.Privacy"), adv);
	privacy->setWordWrap(true);
	af->addRow(privacy);
	tabs_->addTab(adv, T("KillSense.Settings.AdvancedTab"));

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(browse, &QPushButton::clicked, this, &SettingsDialog::onBrowseJsonl);

	load();
	tabs_->setCurrentIndex(initialTab);
}

void SettingsDialog::load()
{
	const AppConfig c = controller_->config();
	preset_->setCurrentIndex(preset_->findData(QString::fromLatin1(presetId(c.preset))));
	fps_->setValue(c.analysisFpsOverride);
	thresholdOn_->setChecked(c.thresholdOverride.has_value());
	threshold_->setValue(c.thresholdOverride.value_or(0.80));
	cooldownOn_->setChecked(c.cooldownOverrideMs.has_value());
	cooldown_->setValue(c.cooldownOverrideMs.value_or(1500));
	roiOn_->setChecked(c.customRoiEnabled);
	roiX_->setValue(c.customRoi.x);
	roiY_->setValue(c.customRoi.y);
	roiW_->setValue(c.customRoi.w);
	roiH_->setValue(c.customRoi.h);
	autoStart_->setChecked(c.autoStart);
	websocket_->setChecked(c.outputWebsocketVendor);
	websocketStatus_->setText(controller_->snapshot().websocketAvailable ? T("KillSense.Outputs.WebsocketReady")
									     : T("KillSense.Outputs.WebsocketMissing"));
	jsonl_->setChecked(c.outputJsonl);
	jsonlPath_->setText(Q(c.jsonlPath));
	logLevel_->setCurrentIndex(logLevel_->findData(QString::fromLatin1(logLevelName(c.logLevel))));
	ocrLanguage_->setText(Q(c.ocrLanguage));
}

void SettingsDialog::onBrowseJsonl()
{
	const QString file = QFileDialog::getSaveFileName(this, T("KillSense.Outputs.JsonlPath"), jsonlPath_->text(),
							  QStringLiteral("JSON Lines (*.jsonl);;All files (*)"));
	if (!file.isEmpty())
		jsonlPath_->setText(file);
}

void SettingsDialog::onAccept()
{
	AppConfig c = controller_->config();
	PerformancePreset preset;
	if (parsePreset(S(preset_->currentData().toString()), preset))
		c.preset = preset;
	c.analysisFpsOverride = fps_->value();
	c.thresholdOverride = thresholdOn_->isChecked() ? std::optional<double>(threshold_->value()) : std::nullopt;
	c.cooldownOverrideMs = cooldownOn_->isChecked() ? std::optional<int>(cooldown_->value()) : std::nullopt;
	const NormRect roi{roiX_->value(), roiY_->value(), roiW_->value(), roiH_->value()};
	if (roiOn_->isChecked() && !roi.isValid()) {
		tabs_->setCurrentIndex(DetectionTab);
		roiOn_->setText(T("KillSense.Settings.CustomRoi") + QStringLiteral(" - ") +
				T("KillSense.Settings.RoiInvalid"));
		return;
	}
	c.customRoiEnabled = roiOn_->isChecked();
	c.customRoi = roi.isValid() ? roi : c.customRoi;
	c.autoStart = autoStart_->isChecked();
	c.outputWebsocketVendor = websocket_->isChecked();
	c.outputJsonl = jsonl_->isChecked();
	c.jsonlPath = S(jsonlPath_->text().trimmed());
	LogLevel lvl;
	if (parseLogLevel(S(logLevel_->currentData().toString()), lvl))
		c.logLevel = lvl;
	c.ocrLanguage = S(ocrLanguage_->text().trimmed());
	controller_->applyConfig(c);
	accept();
}

} // namespace killsense::obsplugin

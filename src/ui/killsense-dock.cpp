// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense-dock.hpp"

#include "debug-view.hpp"
#include "profile-manager-dialog.hpp"
#include "settings-dialog.hpp"
#include "ui-common.hpp"

#include "../controller.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace killsense::obsplugin {

namespace {
QLabel *valueLabel(QWidget *parent)
{
	auto *l = new QLabel(QStringLiteral("-"), parent);
	l->setTextInteractionFlags(Qt::TextSelectableByMouse);
	l->setWordWrap(true);
	return l;
}
} // namespace

KillSenseDock::KillSenseDock(std::shared_ptr<Controller> controller, QWidget *parent)
	: QWidget(parent),
	  controller_(controller)
{
	setObjectName(QStringLiteral("RealmKillSenseDock"));
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *content = new QWidget(scroll);
	auto *root = new QVBoxLayout(content);
	scroll->setWidget(content);
	outer->addWidget(scroll);

	// --- Source ---
	auto *sourceBox = new QGroupBox(T("KillSense.Source"), content);
	auto *sourceRow = new QHBoxLayout(sourceBox);
	sourceCombo_ = new QComboBox(sourceBox);
	sourceCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	sourceCombo_->setToolTip(T("KillSense.Source.Tooltip"));
	auto *refresh = new QPushButton(T("KillSense.Refresh"), sourceBox);
	sourceRow->addWidget(sourceCombo_, 1);
	sourceRow->addWidget(refresh);
	root->addWidget(sourceBox);

	// --- Profile ---
	auto *profileBox = new QGroupBox(T("KillSense.Profile"), content);
	auto *profileLayout = new QVBoxLayout(profileBox);
	profileCombo_ = new QComboBox(profileBox);
	profileCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	auto *profileRow = new QHBoxLayout();
	auto *autoDetect = new QPushButton(T("KillSense.AutoDetect"), profileBox);
	autoDetect->setToolTip(T("KillSense.AutoDetect.Tooltip"));
	auto *manager = new QPushButton(T("KillSense.ProfileManager"), profileBox);
	profileRow->addWidget(autoDetect);
	profileRow->addWidget(manager);
	profileLayout->addWidget(profileCombo_);
	profileLayout->addLayout(profileRow);
	root->addWidget(profileBox);

	// --- Main controls ---
	auto *controls = new QHBoxLayout();
	startStop_ = new QPushButton(T("KillSense.Start"), content);
	auto *testKill = new QPushButton(T("KillSense.TestKill"), content);
	auto *reset = new QPushButton(T("KillSense.ResetSession"), content);
	controls->addWidget(startStop_);
	controls->addWidget(testKill);
	controls->addWidget(reset);
	root->addLayout(controls);

	// --- Status ---
	auto *statusBox = new QGroupBox(T("KillSense.Status"), content);
	auto *form = new QFormLayout(statusBox);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	source_ = valueLabel(statusBox);
	profile_ = valueLabel(statusBox);
	state_ = valueLabel(statusBox);
	kills_ = valueLabel(statusBox);
	streak_ = valueLabel(statusBox);
	lastEvent_ = valueLabel(statusBox);
	lastConfidence_ = valueLabel(statusBox);
	fps_ = valueLabel(statusBox);
	latency_ = valueLabel(statusBox);
	dropped_ = valueLabel(statusBox);
	cpu_ = valueLabel(statusBox);
	gpu_ = valueLabel(statusBox);
	ocr_ = valueLabel(statusBox);
	form->addRow(T("KillSense.Status.Source"), source_);
	form->addRow(T("KillSense.Status.Profile"), profile_);
	form->addRow(T("KillSense.Status.State"), state_);
	form->addRow(T("KillSense.Status.Kills"), kills_);
	form->addRow(T("KillSense.Status.Streak"), streak_);
	form->addRow(T("KillSense.Status.LastEvent"), lastEvent_);
	form->addRow(T("KillSense.Status.LastConfidence"), lastConfidence_);
	form->addRow(T("KillSense.Status.Fps"), fps_);
	form->addRow(T("KillSense.Status.Latency"), latency_);
	form->addRow(T("KillSense.Status.Dropped"), dropped_);
	form->addRow(T("KillSense.Status.Cpu"), cpu_);
	form->addRow(T("KillSense.Status.Gpu"), gpu_);
	form->addRow(T("KillSense.Status.Ocr"), ocr_);
	root->addWidget(statusBox);

	// --- Tools ---
	auto *tools = new QGroupBox(T("KillSense.Tools"), content);
	auto *toolsLayout = new QVBoxLayout(tools);
	auto *learn = new QPushButton(T("KillSense.LearnThisGame"), tools);
	learn->setEnabled(false); // planned: Milestone 2 (core logic exists, see docs/MILESTONES.md)
	learn->setToolTip(T("KillSense.LearnThisGame.Planned"));
	auto *row1 = new QHBoxLayout();
	auto *outputs = new QPushButton(T("KillSense.EventOutputs"), tools);
	auto *debug = new QPushButton(T("KillSense.DebugView"), tools);
	auto *settings = new QPushButton(T("KillSense.Settings"), tools);
	row1->addWidget(outputs);
	row1->addWidget(debug);
	row1->addWidget(settings);
	toolsLayout->addWidget(learn);
	toolsLayout->addLayout(row1);
	root->addWidget(tools);

	message_ = new QLabel(content);
	message_->setWordWrap(true);
	root->addWidget(message_);
	root->addStretch(1);

	connect(refresh, &QPushButton::clicked, this, &KillSenseDock::refreshSources);
	connect(sourceCombo_, QOverload<int>::of(&QComboBox::activated), this, &KillSenseDock::onSourceChosen);
	connect(profileCombo_, QOverload<int>::of(&QComboBox::activated), this, &KillSenseDock::onProfileChosen);
	connect(autoDetect, &QPushButton::clicked, this, &KillSenseDock::onAutoDetect);
	connect(manager, &QPushButton::clicked, this, &KillSenseDock::onProfileManager);
	connect(startStop_, &QPushButton::clicked, this, &KillSenseDock::onStartStop);
	connect(testKill, &QPushButton::clicked, this, &KillSenseDock::onTestKill);
	connect(reset, &QPushButton::clicked, this, &KillSenseDock::onResetSession);
	connect(outputs, &QPushButton::clicked, this, &KillSenseDock::onEventOutputs);
	connect(debug, &QPushButton::clicked, this, &KillSenseDock::onDebugView);
	connect(settings, &QPushButton::clicked, this, &KillSenseDock::onSettings);

	refreshSources();
	refreshProfiles();

	timer_ = new QTimer(this);
	timer_->setInterval(250);
	connect(timer_, &QTimer::timeout, this, &KillSenseDock::refreshStatus);
	timer_->start();
	// Sources are created after modules load; refresh once OBS has settled.
	QTimer::singleShot(3000, this, &KillSenseDock::refreshSources);
}

KillSenseDock::~KillSenseDock()
{
	if (timer_)
		timer_->stop();
	if (auto c = controller())
		c->setDebugEnabled(false);
}

void KillSenseDock::refreshSources()
{
	auto c = controller();
	if (!c)
		return;
	updatingCombos_ = true;
	const AppConfig cfg = c->config();
	sourceCombo_->clear();
	sourceCombo_->addItem(T("KillSense.Source.None"), QString());
	int selected = 0;
	for (const auto &s : c->videoSources()) {
		QString label = Q(s.name);
		if (s.isScene)
			label += QStringLiteral("  [") + T("KillSense.Source.Scene") + QStringLiteral("]");
		else if (!s.typeId.empty())
			label += QStringLiteral("  [") + Q(s.typeId) + QStringLiteral("]");
		sourceCombo_->addItem(label, Q(s.uuid));
		sourceCombo_->setItemData(sourceCombo_->count() - 1, Q(s.name), Qt::UserRole + 1);
		if ((!cfg.sourceUuid.empty() && s.uuid == cfg.sourceUuid) ||
		    (cfg.sourceUuid.empty() && s.name == cfg.sourceName))
			selected = sourceCombo_->count() - 1;
	}
	if (selected == 0 && !cfg.sourceName.empty()) {
		sourceCombo_->addItem(Q(cfg.sourceName) + QStringLiteral(" (") + T("KillSense.Source.Missing") +
					      QStringLiteral(")"),
				      Q(cfg.sourceUuid));
		sourceCombo_->setItemData(sourceCombo_->count() - 1, Q(cfg.sourceName), Qt::UserRole + 1);
		selected = sourceCombo_->count() - 1;
	}
	sourceCombo_->setCurrentIndex(selected);
	updatingCombos_ = false;
}

void KillSenseDock::refreshProfiles()
{
	auto c = controller();
	if (!c)
		return;
	updatingCombos_ = true;
	const AppConfig cfg = c->config();
	profileCombo_->clear();
	int selected = -1;
	for (const auto &p : c->profiles()) {
		QString label = Q(p->displayName);
		if (!p->builtin)
			label += QStringLiteral("  [") + T("KillSense.Profile.User") + QStringLiteral("]");
		profileCombo_->addItem(label, Q(p->id));
		if (p->id == cfg.profileId)
			selected = profileCombo_->count() - 1;
	}
	if (selected >= 0)
		profileCombo_->setCurrentIndex(selected);
	updatingCombos_ = false;
}

void KillSenseDock::onSourceChosen(int index)
{
	auto c = controller();
	if (!c || updatingCombos_ || index < 0)
		return;
	const std::string uuid = S(sourceCombo_->itemData(index).toString());
	const std::string name = S(sourceCombo_->itemData(index, Qt::UserRole + 1).toString());
	c->selectSource(uuid, name);
	message_->clear();
}

void KillSenseDock::onProfileChosen(int index)
{
	auto c = controller();
	if (!c || updatingCombos_ || index < 0)
		return;
	c->selectProfile(S(profileCombo_->itemData(index).toString()));
}

void KillSenseDock::onAutoDetect()
{
	auto c = controller();
	if (!c)
		return;
	message_->setText(Q(c->autoDetectProfile()));
	refreshProfiles();
}

void KillSenseDock::onStartStop()
{
	auto c = controller();
	if (!c)
		return;
	if (c->detecting()) {
		c->stopDetection();
		message_->clear();
	} else {
		std::string err;
		if (!c->startDetection(&err))
			message_->setText(T("KillSense.StartFailed") + QStringLiteral(" ") + Q(err));
		else
			message_->clear();
	}
	refreshStatus();
}

void KillSenseDock::onTestKill()
{
	if (auto c = controller())
		c->testKill();
}

void KillSenseDock::onResetSession()
{
	if (auto c = controller())
		c->resetSession();
}

void KillSenseDock::onProfileManager()
{
	auto c = controller();
	if (!c)
		return;
	ProfileManagerDialog dlg(c, this);
	dlg.exec();
	refreshProfiles();
}

void KillSenseDock::openSettings(int tab)
{
	auto c = controller();
	if (!c)
		return;
	SettingsDialog dlg(c, tab, this);
	dlg.exec();
}

void KillSenseDock::onEventOutputs()
{
	openSettings(SettingsDialog::OutputsTab);
}

void KillSenseDock::onSettings()
{
	openSettings(SettingsDialog::DetectionTab);
}

void KillSenseDock::onDebugView()
{
	auto c = controller();
	if (!c)
		return;
	if (!debugView_) {
		debugView_ = new DebugViewDialog(controller_, this);
		debugView_->setAttribute(Qt::WA_DeleteOnClose);
	}
	debugView_->show();
	debugView_->raise();
	debugView_->activateWindow();
}

void KillSenseDock::refreshStatus()
{
	auto c = controller();
	if (!c) {
		setEnabled(false);
		return;
	}
	const ControllerSnapshot s = c->snapshot();
	// Keep the combos in sync when the source/profile was changed elsewhere
	// (hotkeys, obs-websocket vendor requests, fallback to the generic profile).
	if (!updatingCombos_ && !profileCombo_->view()->isVisible()) {
		const int pi = profileCombo_->findData(Q(s.profileId));
		if (pi >= 0 && pi != profileCombo_->currentIndex()) {
			updatingCombos_ = true;
			profileCombo_->setCurrentIndex(pi);
			updatingCombos_ = false;
		}
	}
	if (!updatingCombos_ && !sourceCombo_->view()->isVisible()) {
		const QString wantedName = Q(s.sourceName);
		const QString currentName = sourceCombo_->currentData(Qt::UserRole + 1).toString();
		if (currentName != wantedName) {
			const int si = sourceCombo_->findData(wantedName, Qt::UserRole + 1);
			if (si >= 0) {
				updatingCombos_ = true;
				sourceCombo_->setCurrentIndex(si);
				updatingCombos_ = false;
			} else {
				refreshSources();
			}
		}
	}
	startStop_->setText(s.detecting ? T("KillSense.Stop") : T("KillSense.Start"));

	QString src = s.sourceSelected ? Q(s.sourceName) : T("KillSense.Source.None");
	if (s.detecting && s.sampler.sourceAvailable)
		src += QStringLiteral(" (%1x%2)").arg(s.sampler.sourceWidth).arg(s.sampler.sourceHeight);
	source_->setText(src);
	profile_->setText(Q(s.profileName));
	state_->setText(Q(s.detectorState));
	if (!s.worker.unavailableDetectors.empty()) {
		QString tip;
		for (const auto &u : s.worker.unavailableDetectors)
			tip += Q(u) + QStringLiteral("\n");
		state_->setToolTip(tip.trimmed());
	} else {
		state_->setToolTip(QString());
	}
	kills_->setText(QString::number(s.session.kills));
	streak_->setText(QStringLiteral("%1 (best %2)").arg(s.session.streak).arg(s.session.bestStreak));
	if (s.session.lastEvent) {
		const auto &ev = *s.session.lastEvent;
		const QString when =
			QDateTime::fromMSecsSinceEpoch(ev.timestampMs).toString(QStringLiteral("HH:mm:ss"));
		lastEvent_->setText(QStringLiteral("%1 @ %2%3")
					    .arg(QString::fromUtf8(eventTypeVendorName(ev.type)), when,
						 ev.test ? QStringLiteral(" (test)") : QString()));
		lastConfidence_->setText(QString::number(ev.confidence, 'f', 2));
	} else {
		lastEvent_->setText(QStringLiteral("-"));
		lastConfidence_->setText(s.detecting ? QStringLiteral("live %1").arg(s.worker.lastConfidence, 0, 'f', 2)
						     : QStringLiteral("-"));
	}
	fps_->setText(s.detecting ? QString::number(s.worker.analysisFps, 'f', 1) : QStringLiteral("-"));
	latency_->setText(s.detecting ? QStringLiteral("%1 ms analysis / %2 ms end-to-end / %3 us render thread")
						.arg(s.worker.avgProcessingMs, 0, 'f', 1)
						.arg(s.worker.avgLatencyMs, 0, 'f', 0)
						.arg(s.sampler.avgTickUs, 0, 'f', 0)
				      : QStringLiteral("-"));
	dropped_->setText(QStringLiteral("%1 (queue) + %2 (busy skip)")
				  .arg(s.worker.framesDropped)
				  .arg(s.sampler.skippedWorkerBusy));
	cpu_->setText(s.cpuPercent >= 0.0 ? QStringLiteral("OBS process %1%").arg(s.cpuPercent, 0, 'f', 1)
					  : QStringLiteral("n/a"));
	gpu_->setText(Q(s.gpuState));
	ocr_->setText(s.worker.ocrAvailable ? Q(s.worker.ocrEngine)
					    : (s.detecting ? T("KillSense.Ocr.Unavailable") : QStringLiteral("-")));
}

} // namespace killsense::obsplugin

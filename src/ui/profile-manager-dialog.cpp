// SPDX-License-Identifier: GPL-2.0-or-later
#include "profile-manager-dialog.hpp"

#include "ui-common.hpp"

#include "../controller.hpp"

#include "killsense/fsutil.hpp"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace killsense::obsplugin {

ProfileManagerDialog::ProfileManagerDialog(std::shared_ptr<Controller> controller, QWidget *parent)
	: QDialog(parent),
	  controller_(std::move(controller))
{
	setWindowTitle(T("KillSense.ProfileManager"));
	resize(720, 480);
	auto *layout = new QVBoxLayout(this);
	auto *help = new QLabel(T("KillSense.ProfileManager.Help"), this);
	help->setWordWrap(true);
	layout->addWidget(help);

	table_ = new QTableWidget(this);
	table_->setColumnCount(4);
	table_->setHorizontalHeaderLabels({T("KillSense.ProfileManager.Id"), T("KillSense.ProfileManager.Name"),
					   T("KillSense.ProfileManager.Origin"), T("KillSense.ProfileManager.File")});
	table_->horizontalHeader()->setStretchLastSection(true);
	table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	layout->addWidget(table_, 2);

	layout->addWidget(new QLabel(T("KillSense.ProfileManager.Issues"), this));
	issues_ = new QPlainTextEdit(this);
	issues_->setReadOnly(true);
	layout->addWidget(issues_, 1);

	auto *row = new QHBoxLayout();
	auto *reloadBtn = new QPushButton(T("KillSense.ProfileManager.Reload"), this);
	auto *openBuiltin = new QPushButton(T("KillSense.ProfileManager.OpenBuiltin"), this);
	auto *openUser = new QPushButton(T("KillSense.ProfileManager.OpenUser"), this);
	row->addWidget(reloadBtn);
	row->addWidget(openBuiltin);
	row->addWidget(openUser);
	row->addStretch(1);
	layout->addLayout(row);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	layout->addWidget(buttons);

	connect(reloadBtn, &QPushButton::clicked, this, &ProfileManagerDialog::reload);
	connect(openBuiltin, &QPushButton::clicked, this, [this] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(Q(pathToUtf8(controller_->builtinProfilesDir()))));
	});
	connect(openUser, &QPushButton::clicked, this, [this] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(Q(pathToUtf8(controller_->userProfilesDir()))));
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	populate();
}

void ProfileManagerDialog::reload()
{
	controller_->reloadProfiles();
	populate();
}

void ProfileManagerDialog::populate()
{
	const auto profiles = controller_->profiles();
	table_->setRowCount(static_cast<int>(profiles.size()));
	int row = 0;
	for (const auto &p : profiles) {
		table_->setItem(row, 0, new QTableWidgetItem(Q(p->id)));
		table_->setItem(row, 1, new QTableWidgetItem(Q(p->displayName)));
		table_->setItem(row, 2,
				new QTableWidgetItem(p->builtin ? T("KillSense.ProfileManager.Builtin")
								: T("KillSense.Profile.User")));
		table_->setItem(row, 3, new QTableWidgetItem(Q(pathToUtf8(p->sourcePath))));
		++row;
	}
	table_->resizeColumnsToContents();

	QString text;
	for (const auto &issue : controller_->profileIssues()) {
		text += Q(issue.file) + QStringLiteral("\n");
		for (const auto &e : issue.errors)
			text += QStringLiteral("  ERROR: ") + Q(e) + QStringLiteral("\n");
		for (const auto &w : issue.warnings)
			text += QStringLiteral("  warning: ") + Q(w) + QStringLiteral("\n");
	}
	issues_->setPlainText(text.isEmpty() ? T("KillSense.ProfileManager.NoIssues") : text);
}

} // namespace killsense::obsplugin
